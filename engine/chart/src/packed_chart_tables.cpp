#include <cuexis/chart/packed_chart_tables.hpp>

#include <cuexis/chart/uuid.hpp>
#include <cuexis/core/error.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>

namespace cuexis::chart::packed {
namespace {

auto fail(std::string code, std::string message) -> core::Error {
    return core::Error{std::move(code), std::move(message)};
}

auto hexValue(char c) -> int {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

auto writeUuid(ByteWriter& writer, std::string_view value) -> core::Result<void> {
    if (value.size() != 36 || value[8] != '-' || value[13] != '-' || value[18] != '-' ||
        value[23] != '-') {
        return core::unexpected(fail("packed.identity.invalid_uuid", "Identity is not UUID text"));
    }
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '-')
            continue;
        const auto hi = hexValue(value[i]);
        const auto lo = hexValue(value[++i]);
        if (hi < 0 || lo < 0) {
            return core::unexpected(
                fail("packed.identity.invalid_uuid", "Identity is not UUID text"));
        }
        writer.writeU8(static_cast<std::uint8_t>((hi << 4) | lo));
    }
    return {};
}

void writeF32(ByteWriter& writer, float value) {
    writer.writeU32(std::bit_cast<std::uint32_t>(value));
}
void writeF64(ByteWriter& writer, double value) {
    writer.writeU64(std::bit_cast<std::uint64_t>(value));
}
auto readF32(ByteReader& reader) -> core::Result<float> {
    auto v = reader.readU32();
    if (!v)
        return core::unexpected(std::move(v.error()));
    return std::bit_cast<float>(*v);
}
auto readF64(ByteReader& reader) -> core::Result<double> {
    auto v = reader.readU64();
    if (!v)
        return core::unexpected(std::move(v.error()));
    return std::bit_cast<double>(*v);
}
struct Section final {
    std::array<char, 4> code{};
    std::vector<std::byte> bytes;
    std::uint32_t records{};
};

auto identityKey(const CanonicalEntityIdentity& identity) -> std::string {
    if (const auto* explicitIdentity = std::get_if<ExplicitEntityIdentity>(&identity)) {
        return "0:" + explicitIdentity->objectId.value;
    }
    const auto& generated = std::get<GeneratedEntityIdentity>(identity);
    std::string key = "1:" + generated.chartId.value + ":" + generated.bindingId + ":" +
                      generated.moduleId + ":" + generated.exportId;
    for (const auto& step : generated.path) {
        key += ":" + step.nodeId + ":" + std::to_string(step.iterationIndexPlusOne);
    }
    return key;
}

struct Dictionaries final {
    std::vector<std::string> strings;
    std::map<std::string, std::uint32_t> stringIndex;
    std::map<std::pair<std::uint8_t, std::string>, std::uint32_t> refIndex;
};

void addString(Dictionaries& dict, std::string value) {
    dict.strings.push_back(std::move(value));
}

void collectStrings(Dictionaries& dict, const CanonicalSemanticChart& chart) {
    addString(dict, chart.chartId.value);
    for (const auto& feature : chart.features)
        addString(dict, feature.id);
    if (chart.mainMusic)
        addString(dict, chart.mainMusic->value);
    for (const auto& entity : chart.entities) {
        if (const auto* generated = std::get_if<GeneratedEntityIdentity>(&entity.identity)) {
            addString(dict, generated->chartId.value);
            addString(dict, generated->bindingId);
            addString(dict, generated->moduleId);
            addString(dict, generated->exportId);
            for (const auto& step : generated->path)
                addString(dict, step.nodeId);
        }
        for (const auto& component : entity.components) {
            if (const auto* renderable = std::get_if<CanonicalRenderable>(&component)) {
                addString(dict, renderable->mesh.value);
                addString(dict, renderable->material.value);
            }
        }
        for (const auto& req : entity.requirements) {
            addString(dict, req.localId);
            addString(dict, req.judgementDomain.id);
            addString(dict, req.requiredAction.id);
            for (const auto& effect : req.effects)
                addString(dict, effect.id);
        }
    }
    std::sort(dict.strings.begin(), dict.strings.end());
    dict.strings.erase(std::unique(dict.strings.begin(), dict.strings.end()), dict.strings.end());
    for (std::size_t i = 0; i < dict.strings.size(); ++i) {
        dict.stringIndex.emplace(dict.strings[i], static_cast<std::uint32_t>(i));
    }
    auto addRef = [&](std::uint8_t kind, const std::string& id) {
        dict.refIndex.emplace(std::make_pair(kind, id),
                              static_cast<std::uint32_t>(dict.refIndex.size()));
    };
    if (chart.mainMusic)
        addRef(1, chart.mainMusic->value);
    for (const auto& feature : chart.features)
        addRef(7, feature.id);
    for (const auto& entity : chart.entities) {
        for (const auto& component : entity.components) {
            if (const auto* renderable = std::get_if<CanonicalRenderable>(&component)) {
                addRef(1, renderable->mesh.value);
                addRef(1, renderable->material.value);
            }
        }
        for (const auto& req : entity.requirements) {
            addRef(4, req.judgementDomain.id);
            addRef(5, req.requiredAction.id);
        }
    }
    // REF0 ordering is (kind, string bytes), not insertion order.
    std::vector<std::pair<std::pair<std::uint8_t, std::string>, std::uint32_t>> refs;
    for (const auto& [key, unused] : dict.refIndex)
        refs.emplace_back(key, unused);
    std::sort(refs.begin(), refs.end(),
              [](const auto& left, const auto& right) { return left.first < right.first; });
    dict.refIndex.clear();
    for (std::size_t i = 0; i < refs.size(); ++i)
        dict.refIndex.emplace(refs[i].first, static_cast<std::uint32_t>(i));
}

auto stringRef(const Dictionaries& dict, const std::string& value) -> core::Result<std::uint32_t> {
    const auto it = dict.stringIndex.find(value);
    if (it == dict.stringIndex.end())
        return core::unexpected(fail("packed.tables.string_missing", "String is absent from STR0"));
    return it->second;
}

auto typedRef(const Dictionaries& dict, std::uint8_t kind, const std::string& value)
    -> core::Result<std::uint32_t> {
    const auto it = dict.refIndex.find(std::make_pair(kind, value));
    if (it == dict.refIndex.end())
        return core::unexpected(
            fail("packed.tables.reference_missing", "Reference is absent from REF0"));
    return it->second;
}

void writeTransform(ByteWriter& writer, const CanonicalTransform& value) {
    writeF32(writer, value.position.x);
    writeF32(writer, value.position.y);
    writeF32(writer, value.position.z);
    writeF32(writer, value.rotation.x);
    writeF32(writer, value.rotation.y);
    writeF32(writer, value.rotation.z);
    writeF32(writer, value.rotation.w);
    writeF32(writer, value.scale.x);
    writeF32(writer, value.scale.y);
    writeF32(writer, value.scale.z);
}
auto writeRenderable(ByteWriter& writer, const CanonicalRenderable& value, const Dictionaries& dict)
    -> core::Result<void> {
    auto mesh = typedRef(dict, 1, value.mesh.value);
    auto material = typedRef(dict, 1, value.material.value);
    if (!mesh || !material) {
        return core::unexpected(
            fail("packed.tables.reference_missing", "Renderable reference is absent from REF0"));
    }
    writer.writeUnsignedLeb128(*mesh);
    writer.writeUnsignedLeb128(*material);
    writer.writeU8(value.alpha);
    return {};
}

auto writeStringSection(const Dictionaries& dict) -> Section {
    ByteWriter writer;
    writer.writeU32(static_cast<std::uint32_t>(dict.strings.size()));
    std::uint32_t dataBytes = 0;
    for (const auto& value : dict.strings)
        dataBytes += static_cast<std::uint32_t>(value.size());
    writer.writeU32(dataBytes);
    std::uint32_t offset = 0;
    writer.writeU32(0);
    for (const auto& value : dict.strings) {
        offset += static_cast<std::uint32_t>(value.size());
        writer.writeU32(offset);
    }
    for (const auto& value : dict.strings)
        writer.writeBytes(std::as_bytes(std::span{value.data(), value.size()}));
    return {{'S', 'T', 'R', '0'},
            std::move(writer).takeBytes(),
            static_cast<std::uint32_t>(dict.strings.size())};
}

auto writeReferenceSection(const Dictionaries& dict) -> Section {
    ByteWriter writer;
    for (const auto& [key, index] : dict.refIndex) {
        writer.writeU8(key.first);
        writer.writeUnsignedLeb128(*stringRef(dict, key.second));
    }
    return {{'R', 'E', 'F', '0'},
            std::move(writer).takeBytes(),
            static_cast<std::uint32_t>(dict.refIndex.size())};
}

auto writeMetaSection(const CanonicalSemanticChart& chart, const Dictionaries& dict)
    -> core::Result<Section> {
    ByteWriter writer;
    if (auto r = writeUuid(writer, chart.chartId.value); !r)
        return core::unexpected(std::move(r.error()));
    writer.writeU16(5);
    if (chart.mainMusic) {
        auto r = typedRef(dict, 1, chart.mainMusic->value);
        if (!r)
            return core::unexpected(std::move(r.error()));
        writer.writeUnsignedLeb128(*r + 1U);
    } else
        writer.writeU8(0);
    writer.writeU8(1); // perspective
    writeF64(writer, chart.defaultCamera.fovY);
    writeF64(writer, chart.defaultCamera.nearPlane);
    writeF64(writer, chart.defaultCamera.farPlane);
    writeF64(writer, chart.defaultCamera.pitch);
    writeF64(writer, chart.defaultCamera.yaw);
    writeF64(writer, chart.defaultCamera.roll);
    const auto position = chart.defaultCamera.defaultTransform
                              ? chart.defaultCamera.defaultTransform->position
                              : core::Vec3{};
    writeF32(writer, position.x);
    writeF32(writer, position.y);
    writeF32(writer, position.z);
    writer.writeU32(static_cast<std::uint32_t>(chart.features.size()));
    for (const auto& feature : chart.features) {
        auto r = typedRef(dict, 7, feature.id);
        if (!r)
            return core::unexpected(std::move(r.error()));
        writer.writeUnsignedLeb128(*r);
        writer.writeU32(feature.version);
    }
    return Section{{'M', 'E', 'T', 'A'}, std::move(writer).takeBytes(), 1};
}

auto writeTimeSection(const ChartTiming& timing) -> Section {
    ByteWriter writer;
    writeF64(writer, timing.offsetMs);
    writeF64(writer, timing.defaultBpm);
    writer.writeU32(static_cast<std::uint32_t>(timing.tempoEvents.size()));
    writer.writeU32(static_cast<std::uint32_t>(timing.stops.size()));
    writer.writeU8(0);
    writer.writeU8(0);
    writer.writeU8(0);
    for (const auto& tempo : timing.tempoEvents) {
        (void)writeRationalBeatAtom(writer, tempo.startBeat);
        (void)writeRationalBeatAtom(writer, tempo.durationBeats);
        writeF64(writer, tempo.startBpm);
        writeF64(writer, tempo.endBpm);
        writeF64(writer, tempo.startSlope);
        writeF64(writer, tempo.endSlope);
    }
    for (const auto& stop : timing.stops) {
        (void)writeRationalBeatAtom(writer, stop.beat);
        writeF64(writer, stop.durationMs);
    }
    return {{'T', 'I', 'M', 'E'}, std::move(writer).takeBytes(), 1};
}

struct EntityOrder final {
    std::vector<const CanonicalEntity*> entities;
    std::map<std::string, std::uint32_t> ordinals;
};
auto orderEntities(const CanonicalSemanticChart& chart) -> EntityOrder {
    EntityOrder order;
    for (const auto& entity : chart.entities)
        order.entities.push_back(&entity);
    std::sort(order.entities.begin(), order.entities.end(), [](const auto* a, const auto* b) {
        return identityKey(a->identity) < identityKey(b->identity);
    });
    for (std::size_t i = 0; i < order.entities.size(); ++i)
        order.ordinals.emplace(identityKey(order.entities[i]->identity),
                               static_cast<std::uint32_t>(i));
    return order;
}

auto writeIdentitySection(const EntityOrder& order, const CanonicalSemanticChart&,
                          const Dictionaries& dict) -> core::Result<Section> {
    ByteWriter writer;
    writer.writeU32(0);
    writer.writeU32(0); // scopes/paths are emitted inline for deterministic bridge
    writer.writeU32(static_cast<std::uint32_t>(order.entities.size()));
    for (const auto* entity : order.entities) {
        if (const auto* explicitIdentity = std::get_if<ExplicitEntityIdentity>(&entity->identity)) {
            writer.writeU8(0);
            auto r = writeUuid(writer, explicitIdentity->objectId.value);
            if (!r)
                return core::unexpected(std::move(r.error()));
        } else {
            const auto& generated = std::get<GeneratedEntityIdentity>(entity->identity);
            writer.writeU8(1);
            if (auto r = writeUuid(writer, generated.chartId.value); !r)
                return core::unexpected(std::move(r.error()));
            for (const auto& value :
                 {generated.bindingId, generated.moduleId, generated.exportId}) {
                auto r = stringRef(dict, value);
                if (!r)
                    return core::unexpected(std::move(r.error()));
                writer.writeUnsignedLeb128(*r);
            }
            writer.writeUnsignedLeb128(generated.path.size());
            for (const auto& step : generated.path) {
                auto r = stringRef(dict, step.nodeId);
                if (!r)
                    return core::unexpected(std::move(r.error()));
                writer.writeUnsignedLeb128(*r);
                writer.writeUnsignedLeb128(step.iterationIndexPlusOne);
            }
        }
    }
    return Section{{'I', 'D', 'N', '0'},
                   std::move(writer).takeBytes(),
                   static_cast<std::uint32_t>(order.entities.size())};
}

struct Archetype final {
    std::uint64_t mask{};
    std::optional<CanonicalTransform> transform;
    std::optional<CanonicalRenderable> renderable;
    std::optional<CanonicalCamera> camera;
};
auto makeArchetypes(const EntityOrder& order, std::map<std::uint64_t, std::uint32_t>& indices)
    -> std::vector<Archetype> {
    std::map<std::uint64_t, Archetype> byMask;
    for (const auto* entity : order.entities) {
        auto& arch = byMask[entity->componentMask()];
        arch.mask = entity->componentMask();
        for (const auto& component : entity->components) {
            if (const auto* v = std::get_if<CanonicalTransform>(&component))
                arch.transform = *v;
            if (const auto* v = std::get_if<CanonicalRenderable>(&component))
                arch.renderable = *v;
            if (const auto* v = std::get_if<CanonicalCamera>(&component))
                arch.camera = *v;
        }
    }
    std::vector<Archetype> result;
    for (auto& [mask, arch] : byMask) {
        indices.emplace(mask, static_cast<std::uint32_t>(result.size()));
        result.push_back(std::move(arch));
    }
    return result;
}

auto writeArchetypeSection(const std::vector<Archetype>& arches, const Dictionaries& dict)
    -> core::Result<Section> {
    ByteWriter writer;
    for (const auto& arch : arches) {
        ByteWriter payload;
        if (arch.transform)
            writeTransform(payload, *arch.transform);
        if (arch.renderable) {
            auto rendered = writeRenderable(payload, *arch.renderable, dict);
            if (!rendered)
                return core::unexpected(std::move(rendered.error()));
        }
        if (arch.camera) {
            payload.writeU8(1);
            writeF64(payload, arch.camera->fovY);
            writeF64(payload, arch.camera->nearPlane);
            writeF64(payload, arch.camera->farPlane);
        }
        writer.writeU64(arch.mask);
        writer.writeU32(static_cast<std::uint32_t>(payload.bytes().size()));
        writer.writeBytes(payload.bytes());
    }
    return Section{{'A', 'R', 'C', 'H'},
                   std::move(writer).takeBytes(),
                   static_cast<std::uint32_t>(arches.size())};
}

auto writeEntitySection(const EntityOrder& order,
                        const std::map<std::uint64_t, std::uint32_t>& arches) -> Section {
    ByteWriter writer;
    for (const auto* entity : order.entities) {
        std::uint32_t parent = 0;
        if (entity->parent)
            parent = order.ordinals.at(identityKey(*entity->parent)) + 1U;
        writer.writeUnsignedLeb128(parent);
        writer.writeUnsignedLeb128(arches.at(entity->componentMask()));
    }
    return Section{{'E', 'N', 'T', '0'},
                   std::move(writer).takeBytes(),
                   static_cast<std::uint32_t>(order.entities.size())};
}

auto writeComponentStream(const EntityOrder& order, const std::vector<Archetype>& arches,
                          const std::map<std::uint64_t, std::uint32_t>& archIndices,
                          const Dictionaries& dict, int kind) -> core::Result<Section> {
    ByteWriter writer;
    std::uint32_t rows = 0;
    for (std::size_t ordinal = 0; ordinal < order.entities.size(); ++ordinal) {
        const auto* entity = order.entities[ordinal];
        const auto mask = entity->componentMask();
        if ((kind == 0 && !(mask & 1U)) || (kind == 1 && !(mask & 2U)) ||
            (kind == 2 && !(mask & 16U)))
            continue;
        const auto& arch = arches[archIndices.at(mask)];
        std::uint32_t changed = 0;
        const CanonicalTransform* transform = nullptr;
        const CanonicalRenderable* renderable = nullptr;
        const CanonicalCamera* camera = nullptr;
        for (const auto& component : entity->components) {
            if (kind == 0)
                transform = std::get_if<CanonicalTransform>(&component);
            if (kind == 1)
                renderable = std::get_if<CanonicalRenderable>(&component);
            if (kind == 2)
                camera = std::get_if<CanonicalCamera>(&component);
        }
        if (kind == 0 && transform &&
            (!arch.transform || transform->position != arch.transform->position))
            changed |= 1U;
        if (kind == 0 && transform &&
            (!arch.transform || transform->rotation != arch.transform->rotation))
            changed |= 8U;
        if (kind == 0 && transform &&
            (!arch.transform || transform->scale != arch.transform->scale))
            changed |= 112U;
        if (kind == 1 && renderable &&
            (!arch.renderable || renderable->mesh != arch.renderable->mesh))
            changed |= 1U;
        if (kind == 1 && renderable &&
            (!arch.renderable || renderable->material != arch.renderable->material))
            changed |= 2U;
        if (kind == 1 && renderable &&
            (!arch.renderable || renderable->alpha != arch.renderable->alpha))
            changed |= 4U;
        if (kind == 2 && camera &&
            (!arch.camera || camera->type != arch.camera->type ||
             camera->fovY != arch.camera->fovY))
            changed |= 1U;
        writer.writeUnsignedLeb128(ordinal);
        writer.writeUnsignedLeb128(changed);
        if (kind == 0 && transform) {
            if (changed & 1U) {
                writeF32(writer, transform->position.x);
                writeF32(writer, transform->position.y);
                writeF32(writer, transform->position.z);
            }
            if (changed & 8U) {
                writeF32(writer, transform->rotation.x);
                writeF32(writer, transform->rotation.y);
                writeF32(writer, transform->rotation.z);
                writeF32(writer, transform->rotation.w);
            }
            if (changed & 112U) {
                writeF32(writer, transform->scale.x);
                writeF32(writer, transform->scale.y);
                writeF32(writer, transform->scale.z);
            }
        }
        if (kind == 1 && renderable) {
            if (changed & 1U) {
                auto ref = typedRef(dict, 1, renderable->mesh.value);
                if (!ref)
                    return core::unexpected(std::move(ref.error()));
                writer.writeUnsignedLeb128(*ref);
            }
            if (changed & 2U) {
                auto ref = typedRef(dict, 1, renderable->material.value);
                if (!ref)
                    return core::unexpected(std::move(ref.error()));
                writer.writeUnsignedLeb128(*ref);
            }
            if (changed & 4U)
                writer.writeU8(renderable->alpha);
        }
        if (kind == 2 && camera && (changed & 1U)) {
            writer.writeU8(1);
            writeF64(writer, camera->fovY);
            writeF64(writer, camera->nearPlane);
            writeF64(writer, camera->farPlane);
        }
        ++rows;
    }
    const std::array<char, 4> code = kind == 0   ? std::array<char, 4>{'T', 'R', 'N', '0'}
                                     : kind == 1 ? std::array<char, 4>{'R', 'E', 'N', '0'}
                                                 : std::array<char, 4>{'C', 'A', 'M', '0'};
    return Section{code, std::move(writer).takeBytes(), rows};
}

auto writeConstraintAndRequirementSections(const EntityOrder& order, const Dictionaries& dict)
    -> core::Result<std::pair<Section, Section>> {
    std::map<std::uint32_t, std::uint32_t> laneSets;
    std::vector<std::uint32_t> lanes;
    for (const auto* entity : order.entities)
        for (const auto& req : entity->requirements) {
            if (req.constraints.size() != 1 ||
                !std::holds_alternative<LaneConstraint>(req.constraints[0]))
                return core::unexpected(fail("packed.requirement.unsupported",
                                             "Only one lane constraint is supported"));
            const auto lane = std::get<LaneConstraint>(req.constraints[0]).lane;
            if (!laneSets.contains(lane)) {
                laneSets.emplace(lane, static_cast<std::uint32_t>(lanes.size() + 1U));
                lanes.push_back(lane);
            }
        }
    ByteWriter cns;
    for (auto lane : lanes) {
        cns.writeU8(1);
        cns.writeUnsignedLeb128(lane);
    }
    Section cnsSection{
        {'C', 'N', 'S', '0'}, std::move(cns).takeBytes(), static_cast<std::uint32_t>(lanes.size())};
    ByteWriter req;
    req.writeU8(0);
    req.writeU8(0);
    std::uint32_t count = 0;
    std::uint32_t previous = 0;
    bool first = true;
    for (std::size_t ordinal = 0; ordinal < order.entities.size(); ++ordinal) {
        const auto* entity = order.entities[ordinal];
        for (const auto& requirement : entity->requirements) {
            const auto delta = first ? static_cast<std::uint32_t>(ordinal)
                                     : static_cast<std::uint32_t>(ordinal - previous);
            first = false;
            previous = static_cast<std::uint32_t>(ordinal);
            req.writeUnsignedLeb128(delta);
            req.writeUnsignedLeb128(*stringRef(dict, requirement.localId));
            req.writeU8(static_cast<std::uint8_t>(requirement.kind));
            req.writeU8(static_cast<std::uint8_t>(requirement.interval.kind));
            (void)writeRationalBeatAtom(req, requirement.interval.startBeat);
            if (requirement.interval.endBeat)
                (void)writeRationalBeatAtom(req, *requirement.interval.endBeat);
            req.writeUnsignedLeb128(*typedRef(dict, 4, requirement.judgementDomain.id));
            req.writeUnsignedLeb128(*typedRef(dict, 5, requirement.requiredAction.id));
            const auto lane = std::get<LaneConstraint>(requirement.constraints[0]).lane;
            req.writeUnsignedLeb128(laneSets.at(lane));
            req.writeU8(0);
            ++count;
        }
    }
    return std::make_pair(std::move(cnsSection),
                          Section{{'R', 'E', 'Q', '0'}, std::move(req).takeBytes(), count});
}

} // namespace

auto encode(const CanonicalSemanticChart& chart, PackedChartProfile profile)
    -> core::Result<std::vector<std::byte>> {
    if (profile.flags != 1 || profile.candidateRevision != 1)
        return core::unexpected(
            fail("packed.header.unsupported_revision", "Only candidate revision 1 is supported"));
    if (chart.entities.size() > 40000U)
        return core::unexpected(
            fail("packed.budget.entities", "Packed chart exceeds the 40000 entity limit"));
    std::size_t requirementCount = 0;
    for (const auto& entity : chart.entities) {
        if (entity.requirements.size() > 40000U - requirementCount)
            return core::unexpected(fail("packed.budget.requirements",
                                         "Packed chart exceeds the 40000 requirement limit"));
        requirementCount += entity.requirements.size();
    }
    Dictionaries dict;
    collectStrings(dict, chart);
    auto order = orderEntities(chart);
    std::map<std::uint64_t, std::uint32_t> archIndices;
    auto arches = makeArchetypes(order, archIndices);
    std::vector<Section> sections;
    sections.push_back(writeStringSection(dict));
    sections.push_back(writeReferenceSection(dict));
    auto meta = writeMetaSection(chart, dict);
    if (!meta)
        return core::unexpected(std::move(meta.error()));
    sections.push_back(std::move(*meta));
    sections.push_back(writeTimeSection(chart.timing));
    auto idn = writeIdentitySection(order, chart, dict);
    if (!idn)
        return core::unexpected(std::move(idn.error()));
    sections.push_back(std::move(*idn));
    auto arch = writeArchetypeSection(arches, dict);
    if (!arch)
        return core::unexpected(std::move(arch.error()));
    sections.push_back(std::move(*arch));
    sections.push_back(writeEntitySection(order, archIndices));
    for (int kind = 0; kind < 3; ++kind) {
        auto stream = writeComponentStream(order, arches, archIndices, dict, kind);
        if (!stream)
            return core::unexpected(std::move(stream.error()));
        if (stream->records != 0)
            sections.push_back(std::move(*stream));
    }
    auto reqs = writeConstraintAndRequirementSections(order, dict);
    if (!reqs)
        return core::unexpected(std::move(reqs.error()));
    sections.push_back(std::move(reqs->first));
    sections.push_back(std::move(reqs->second));
    std::sort(sections.begin(), sections.end(),
              [](const auto& a, const auto& b) { return a.code < b.code; });
    ByteWriter file;
    static constexpr std::array<std::byte, 8> magic{std::byte{'C'}, std::byte{'X'}, std::byte{'P'},
                                                    std::byte{'K'}, std::byte{'5'}, std::byte{0},
                                                    std::byte{0},   std::byte{0}};
    file.writeBytes(magic);
    file.writeU16(1);
    file.writeU16(96);
    file.writeU32(1);
    const auto directoryBytes = static_cast<std::uint32_t>(sections.size() * 32U);
    std::uint32_t total = 96U + directoryBytes;
    for (const auto& section : sections)
        total += static_cast<std::uint32_t>(section.bytes.size());
    file.writeU32(total);
    file.writeU32(96);
    file.writeU32(static_cast<std::uint32_t>(sections.size()));
    file.writeU32(directoryBytes);
    for (int i = 0; i < 32; ++i)
        file.writeU8(0);
    file.writeU32(static_cast<std::uint32_t>(order.entities.size()));
    std::size_t requirements = 0;
    for (const auto* e : order.entities)
        requirements += e->requirements.size();
    file.writeU32(static_cast<std::uint32_t>(requirements));
    file.writeU32(0);
    std::uint32_t decoded = 0;
    for (const auto& s : sections)
        decoded += static_cast<std::uint32_t>(s.bytes.size());
    file.writeU32(decoded);
    file.writeU32(static_cast<std::uint32_t>(dict.strings.size()));
    file.writeU32(static_cast<std::uint32_t>(dict.refIndex.size()));
    file.writeU32(1);
    file.writeU32(0);
    std::uint32_t offset = total - decoded;
    for (const auto& section : sections) {
        file.writeBytes(std::as_bytes(std::span{section.code.data(), section.code.size()}));
        file.writeU8(0);
        file.writeU8(0);
        file.writeU16(0);
        file.writeU32(offset);
        file.writeU32(static_cast<std::uint32_t>(section.bytes.size()));
        file.writeU32(static_cast<std::uint32_t>(section.bytes.size()));
        file.writeU32(section.records);
        file.writeU32(crc32(section.bytes));
        file.writeU32(0);
        offset += static_cast<std::uint32_t>(section.bytes.size());
    }
    for (const auto& section : sections)
        file.writeBytes(section.bytes);
    auto result = std::move(file).takeBytes();
    if (result.size() > 16U * 1024U * 1024U)
        return core::unexpected(fail("packed.budget.file_bytes", "Packed chart exceeds 16 MiB"));
    const auto headerCrc = crc32(std::span<const std::byte>{result}.first(92));
    result[92] = static_cast<std::byte>(headerCrc & 0xffU);
    result[93] = static_cast<std::byte>((headerCrc >> 8U) & 0xffU);
    result[94] = static_cast<std::byte>((headerCrc >> 16U) & 0xffU);
    result[95] = static_cast<std::byte>((headerCrc >> 24U) & 0xffU);
    return result;
}

auto inspect(std::span<const std::byte> bytes, PackedChartLimits limits)
    -> core::Result<PackedChartStatistics> {
    if (bytes.size() < 96 || bytes.size() > limits.maxPackedFileBytes)
        return core::unexpected(
            fail("packed.header.invalid_size", "Packed chart size is outside limits"));
    ByteReader reader(bytes);
    auto magic = reader.readBytes(8);
    if (!magic || std::memcmp(magic->data(), "CXPK5\0\0\0", 8) != 0)
        return core::unexpected(
            fail("packed.header.invalid_magic", "Packed chart magic is invalid"));
    auto version = reader.readU16();
    auto header = reader.readU16();
    auto flags = reader.readU32();
    auto total = reader.readU32();
    auto dirOff = reader.readU32();
    auto count = reader.readU32();
    auto dirBytes = reader.readU32();
    if (!version || !header || !flags || !total || !dirOff || !count || !dirBytes)
        return core::unexpected(fail("packed.header.truncated", "Packed header is truncated"));
    if (*version != 1 || *header != 96 || *flags != 1 || *dirOff != 96 || *total != bytes.size() ||
        *dirBytes != *count * 32U)
        return core::unexpected(fail("packed.header.invalid", "Packed header fields are invalid"));
    auto semanticHash = reader.readBytes(32);
    if (!semanticHash)
        return core::unexpected(std::move(semanticHash.error()));
    auto entities = reader.readU32();
    auto requirements = reader.readU32();
    auto events = reader.readU32();
    auto decoded = reader.readU32();
    auto strings = reader.readU32();
    auto refs = reader.readU32();
    auto revision = reader.readU32();
    auto headerCrc = reader.readU32();
    if (!entities || !requirements || !events || !decoded || !strings || !refs || !revision ||
        !headerCrc)
        return core::unexpected(
            fail("packed.header.truncated", "Packed header counters are truncated"));
    if (*headerCrc != crc32(bytes.first(92)))
        return core::unexpected(fail("packed.header.crc", "Packed header CRC mismatch"));
    if (*entities > limits.maxPackedEntities || *requirements > limits.maxPackedRequirements ||
        *strings > limits.maxPackedStrings || *refs > limits.maxPackedReferences ||
        *decoded > limits.maxPackedDecodedBytes)
        return core::unexpected(
            fail("packed.budget.header", "Packed chart counters exceed limits"));
    PackedChartStatistics stats{bytes.size(), *decoded, *strings, *refs, *entities, *requirements};
    std::vector<std::pair<std::uint32_t, std::uint32_t>> ranges;
    ranges.reserve(*count);
    std::set<std::string> sectionNames;
    std::size_t decodedSum = 0;
    for (std::uint32_t i = 0; i < *count; ++i) {
        auto code = reader.readBytes(4);
        auto codec = reader.readU8();
        auto fl = reader.readU8();
        auto res = reader.readU16();
        auto off = reader.readU32();
        auto enc = reader.readU32();
        auto dec = reader.readU32();
        auto rec = reader.readU32();
        auto crc = reader.readU32();
        auto res2 = reader.readU32();
        if (!code || !codec || !fl || !res || !off || !enc || !dec || !rec || !crc || !res2)
            return core::unexpected(
                fail("packed.directory.invalid", "Packed directory entry is truncated"));
        std::string name(reinterpret_cast<const char*>(code->data()), 4);
        const bool known = name == "META" || name == "TIME" || name == "STR0" || name == "REF0" ||
                           name == "IDN0" || name == "ARCH" || name == "ENT0" || name == "TRN0" ||
                           name == "REN0" || name == "CAM0" || name == "CNS0" || name == "REQ0";
        if (!known || !sectionNames.insert(name).second || *codec != 0 || *fl > 1 || *res != 0 ||
            *res2 != 0 || *enc != *dec || *off > bytes.size() || *enc > bytes.size() - *off ||
            crc32(bytes.subspan(*off, *enc)) != *crc)
            return core::unexpected(
                fail("packed.directory.invalid", "Packed directory entry is invalid"));
        ranges.emplace_back(*off, *enc);
        decodedSum += *dec;
    }
    std::sort(ranges.begin(), ranges.end());
    std::uint32_t expectedOffset = 96U + *dirBytes;
    for (const auto& range : ranges) {
        if (range.first != expectedOffset)
            return core::unexpected(
                fail("packed.directory.layout", "Packed section ranges overlap or contain gaps"));
        expectedOffset += range.second;
    }
    if (expectedOffset != bytes.size() || decodedSum != *decoded)
        return core::unexpected(
            fail("packed.directory.size", "Packed decoded byte count is inconsistent"));
    return stats;
}

auto decode(std::span<const std::byte> bytes, PackedChartLimits limits)
    -> core::Result<CanonicalSemanticChart> {
    auto stats = inspect(bytes, limits);
    if (!stats)
        return core::unexpected(std::move(stats.error()));
    struct Directory final {
        std::string code;
        std::span<const std::byte> data;
        std::uint32_t records{};
    };
    ByteReader header(bytes);
    (void)header.readBytes(8);
    (void)header.readU16();
    (void)header.readU16();
    (void)header.readU32();
    (void)header.readU32();
    (void)header.readU32();
    auto directoryCount = header.readU32();
    auto directoryBytes = header.readU32();
    if (!directoryCount || !directoryBytes)
        return core::unexpected(
            fail("packed.directory.truncated", "Packed directory is truncated"));
    auto semanticHash = header.readBytes(32);
    if (!semanticHash)
        return core::unexpected(std::move(semanticHash.error()));
    auto entityCount = header.readU32();
    auto requirementCount = header.readU32();
    auto eventCount = header.readU32();
    auto decodedBytes = header.readU32();
    auto stringCount = header.readU32();
    auto refCount = header.readU32();
    auto revision = header.readU32();
    auto headerCrc = header.readU32();
    if (!entityCount || !requirementCount || !eventCount || !decodedBytes || !stringCount ||
        !refCount || !revision || !headerCrc || *revision != 1)
        return core::unexpected(
            fail("packed.header.invalid", "Packed header counters are invalid"));
    std::vector<Directory> directories;
    directories.reserve(*directoryCount);
    std::set<std::string> seen;
    for (std::uint32_t index = 0; index < *directoryCount; ++index) {
        auto codeBytes = header.readBytes(4);
        auto codec = header.readU8();
        auto flags = header.readU8();
        auto reserved = header.readU16();
        auto offset = header.readU32();
        auto encoded = header.readU32();
        auto decoded = header.readU32();
        auto records = header.readU32();
        auto sectionCrc = header.readU32();
        auto reserved2 = header.readU32();
        if (!codeBytes || !codec || !flags || !reserved || !offset || !encoded || !decoded ||
            !records || !sectionCrc || !reserved2)
            return core::unexpected(
                fail("packed.directory.truncated", "Packed directory is truncated"));
        std::string code(reinterpret_cast<const char*>(codeBytes->data()), 4);
        if (!seen.insert(code).second)
            return core::unexpected(
                fail("packed.directory.duplicate", "Packed section is duplicated"));
        const bool known = code == "META" || code == "TIME" || code == "STR0" || code == "REF0" ||
                           code == "IDN0" || code == "ARCH" || code == "ENT0" || code == "TRN0" ||
                           code == "REN0" || code == "CAM0" || code == "CNS0" || code == "REQ0";
        if (!known || *codec != 0 || *flags > 1 || *reserved != 0 || *reserved2 != 0 ||
            *encoded != *decoded || *offset > bytes.size() || *encoded > bytes.size() - *offset)
            return core::unexpected(
                fail("packed.directory.unsupported", "Packed section is unknown or invalid"));
        auto data = bytes.subspan(*offset, *encoded);
        if (crc32(data) != *sectionCrc)
            return core::unexpected(fail("packed.section.crc", "Packed section CRC mismatch"));
        directories.push_back(Directory{std::move(code), data, *records});
    }
    const auto section = [&](std::string_view code) -> const Directory* {
        for (const auto& value : directories)
            if (value.code == code)
                return &value;
        return nullptr;
    };
    for (const auto required :
         {"META", "TIME", "STR0", "REF0", "IDN0", "ARCH", "ENT0", "CNS0", "REQ0"})
        if (!section(required))
            return core::unexpected(
                fail("packed.directory.required", "Packed required section is missing"));
    const auto* strSection = section("STR0");
    ByteReader strReader(strSection->data);
    auto strCount = strReader.readU32();
    auto dataBytes = strReader.readU32();
    if (!strCount || !dataBytes || *strCount != *stringCount)
        return core::unexpected(fail("packed.strings.count", "STR0 count mismatch"));
    std::vector<std::uint32_t> offsets;
    offsets.reserve(*strCount + 1U);
    for (std::uint32_t i = 0; i < *strCount + 1U; ++i) {
        auto value = strReader.readU32();
        if (!value)
            return core::unexpected(std::move(value.error()));
        offsets.push_back(*value);
    }
    auto stringData = strReader.readBytes(*dataBytes);
    if (!stringData || !strReader.empty())
        return core::unexpected(fail("packed.strings.bounds", "STR0 payload is invalid"));
    std::vector<std::string> strings;
    strings.reserve(*strCount);
    for (std::uint32_t i = 0; i < *strCount; ++i) {
        if (offsets[i] > offsets[i + 1U] || offsets[i + 1U] > *dataBytes)
            return core::unexpected(fail("packed.strings.offset", "STR0 offsets are invalid"));
        strings.emplace_back(reinterpret_cast<const char*>(stringData->data() + offsets[i]),
                             offsets[i + 1U] - offsets[i]);
    }
    auto getString = [&](std::uint64_t index) -> core::Result<std::string> {
        if (index >= strings.size())
            return core::unexpected(fail("packed.strings.index", "String index is out of range"));
        return strings[static_cast<std::size_t>(index)];
    };
    const auto* refSection = section("REF0");
    ByteReader refReader(refSection->data);
    std::vector<std::pair<std::uint8_t, std::string>> refs;
    refs.reserve(*refCount);
    for (std::uint32_t i = 0; i < *refCount; ++i) {
        auto kind = refReader.readU8();
        auto index = refReader.readUnsignedLeb128(*stringCount);
        if (!kind || !index)
            return core::unexpected(fail("packed.references.invalid", "REF0 row is invalid"));
        auto value = getString(*index);
        if (!value)
            return core::unexpected(std::move(value.error()));
        refs.emplace_back(*kind, std::move(*value));
    }
    if (!refReader.empty())
        return core::unexpected(fail("packed.references.trailing", "REF0 has trailing bytes"));
    auto refValue = [&](std::uint64_t index, std::uint8_t kind) -> core::Result<std::string> {
        if (index >= refs.size() || refs[index].first != kind)
            return core::unexpected(
                fail("packed.references.kind", "Typed reference kind is invalid"));
        return refs[index].second;
    };
    CanonicalSemanticChart chart;
    const auto* metaSection = section("META");
    ByteReader meta(metaSection->data);
    auto chartIdBytes = meta.readBytes(16);
    auto semanticVersion = meta.readU16();
    auto musicIndex = meta.readUnsignedLeb128();
    auto cameraType = meta.readU8();
    auto fov = readF64(meta);
    auto nearPlane = readF64(meta);
    auto farPlane = readF64(meta);
    auto pitch = readF64(meta);
    auto yaw = readF64(meta);
    auto roll = readF64(meta);
    auto px = readF32(meta);
    auto py = readF32(meta);
    auto pz = readF32(meta);
    auto features = meta.readU32();
    if (!chartIdBytes || !semanticVersion || !musicIndex || !cameraType || !fov || !nearPlane ||
        !farPlane || !pitch || !yaw || !roll || !px || !py || !pz || !features ||
        *semanticVersion != 5 || *cameraType != 1)
        return core::unexpected(fail("packed.meta.invalid", "META payload is invalid"));
    static constexpr char digits[] = "0123456789abcdef";
    chart.chartId.value.reserve(36);
    for (std::size_t i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            chart.chartId.value.push_back('-');
        const auto v = std::to_integer<std::uint8_t>((*chartIdBytes)[i]);
        chart.chartId.value.push_back(digits[v >> 4]);
        chart.chartId.value.push_back(digits[v & 15]);
    }
    chart.defaultCamera.type = "perspective";
    chart.defaultCamera.fovY = *fov;
    chart.defaultCamera.nearPlane = *nearPlane;
    chart.defaultCamera.farPlane = *farPlane;
    chart.defaultCamera.pitch = *pitch;
    chart.defaultCamera.yaw = *yaw;
    chart.defaultCamera.roll = *roll;
    if (*px != 0.0F || *py != 0.0F || *pz != 0.0F)
        chart.defaultCamera.defaultTransform =
            TransformData{{*px, *py, *pz}, {0, 0, 0, 1}, {1, 1, 1}};
    if (*musicIndex != 0) {
        auto value = refValue(*musicIndex - 1U, 1);
        if (!value)
            return core::unexpected(std::move(value.error()));
        chart.mainMusic = AssetId{*value};
    }
    for (std::uint32_t i = 0; i < *features; ++i) {
        auto index = meta.readUnsignedLeb128();
        auto versionValue = meta.readU32();
        if (!index || !versionValue)
            return core::unexpected(fail("packed.meta.feature", "META feature is invalid"));
        auto value = refValue(*index, 7);
        if (!value)
            return core::unexpected(std::move(value.error()));
        chart.features.push_back(CanonicalFeature{*value, *versionValue});
    }
    if (!meta.empty())
        return core::unexpected(fail("packed.meta.trailing", "META has trailing bytes"));
    const auto* timeSection = section("TIME");
    ByteReader time(timeSection->data);
    auto offsetMs = readF64(time);
    auto bpm = readF64(time);
    auto tempoCount = time.readU32();
    auto stopCount = time.readU32();
    auto tempoMode = time.readU8();
    auto tempoDurationMode = time.readU8();
    auto stopMode = time.readU8();
    if (!offsetMs || !bpm || !tempoCount || !stopCount || !tempoMode || !tempoDurationMode ||
        !stopMode || *tempoMode != 0 || *tempoDurationMode != 0 || *stopMode != 0)
        return core::unexpected(fail("packed.time.invalid", "TIME header is invalid"));
    chart.timing.offsetMs = *offsetMs;
    chart.timing.defaultBpm = *bpm;
    for (std::uint32_t i = 0; i < *tempoCount; ++i) {
        auto start = readRationalBeatAtom(time);
        auto duration = readRationalBeatAtom(time);
        auto a = readF64(time);
        auto b = readF64(time);
        auto c = readF64(time);
        auto d = readF64(time);
        if (!start || !duration || !a || !b || !c || !d)
            return core::unexpected(fail("packed.time.tempo", "Tempo row is invalid"));
        chart.timing.tempoEvents.push_back(TempoEvent{*start, *duration, *a, *b, *c, *d});
    }
    for (std::uint32_t i = 0; i < *stopCount; ++i) {
        auto beat = readRationalBeatAtom(time);
        auto duration = readF64(time);
        if (!beat || !duration)
            return core::unexpected(fail("packed.time.stop", "Stop row is invalid"));
        chart.timing.stops.push_back(TimingStop{*beat, *duration});
    }
    if (!time.empty())
        return core::unexpected(fail("packed.time.trailing", "TIME has trailing bytes"));
    std::vector<CanonicalEntity> entities(*entityCount);
    const auto* idnSection = section("IDN0");
    ByteReader idn(idnSection->data);
    auto scopeCount = idn.readU32();
    auto pathCount = idn.readU32();
    auto identityCount = idn.readU32();
    if (!scopeCount || !pathCount || !identityCount || *scopeCount != 0 || *pathCount != 0 ||
        *identityCount != *entityCount)
        return core::unexpected(fail("packed.identity.invalid", "IDN0 header is invalid"));
    for (std::uint32_t i = 0; i < *identityCount; ++i) {
        auto tag = idn.readU8();
        if (!tag)
            return core::unexpected(std::move(tag.error()));
        if (*tag == 0) {
            auto uuid = idn.readBytes(16);
            if (!uuid)
                return core::unexpected(std::move(uuid.error()));
            std::string value;
            static constexpr char hex[] = "0123456789abcdef";
            for (std::size_t j = 0; j < 16; ++j) {
                if (j == 4 || j == 6 || j == 8 || j == 10)
                    value.push_back('-');
                auto v = std::to_integer<std::uint8_t>((*uuid)[j]);
                value.push_back(hex[v >> 4]);
                value.push_back(hex[v & 15]);
            }
            entities[i].identity = ExplicitEntityIdentity{ChartObjectId{std::move(value)}};
        } else if (*tag == 1) {
            auto generatedChart = idn.readBytes(16);
            if (!generatedChart)
                return core::unexpected(std::move(generatedChart.error()));
            std::string value;
            static constexpr char hex[] = "0123456789abcdef";
            for (std::size_t j = 0; j < 16; ++j) {
                if (j == 4 || j == 6 || j == 8 || j == 10)
                    value.push_back('-');
                auto v = std::to_integer<std::uint8_t>((*generatedChart)[j]);
                value.push_back(hex[v >> 4]);
                value.push_back(hex[v & 15]);
            }
            GeneratedEntityIdentity generated;
            generated.chartId = ChartId{std::move(value)};
            auto bind = idn.readUnsignedLeb128(*stringCount);
            auto module = idn.readUnsignedLeb128(*stringCount);
            auto exportId = idn.readUnsignedLeb128(*stringCount);
            auto steps = idn.readUnsignedLeb128();
            if (!bind || !module || !exportId || !steps)
                return core::unexpected(
                    fail("packed.identity.invalid", "Generated identity is invalid"));
            auto bindValue = getString(*bind);
            auto moduleValue = getString(*module);
            auto exportValue = getString(*exportId);
            if (!bindValue || !moduleValue || !exportValue)
                return core::unexpected(
                    fail("packed.identity.string", "Generated identity string is invalid"));
            generated.bindingId = *bindValue;
            generated.moduleId = *moduleValue;
            generated.exportId = *exportValue;
            for (std::uint64_t s = 0; s < *steps; ++s) {
                auto node = idn.readUnsignedLeb128(*stringCount);
                auto iteration = idn.readUnsignedLeb128();
                if (!node || !iteration)
                    return core::unexpected(
                        fail("packed.identity.path", "Generated identity path is invalid"));
                auto nodeValue = getString(*node);
                if (!nodeValue)
                    return core::unexpected(std::move(nodeValue.error()));
                generated.path.push_back(
                    SemanticIdentityStep{*nodeValue, static_cast<std::uint32_t>(*iteration)});
            }
            entities[i].identity = std::move(generated);
        } else
            return core::unexpected(fail("packed.identity.tag", "Identity tag is unsupported"));
    }
    if (!idn.empty())
        return core::unexpected(fail("packed.identity.trailing", "IDN0 has trailing bytes"));
    const auto* archSection = section("ARCH");
    ByteReader archReader(archSection->data);
    struct DecodedArch final {
        std::uint64_t mask{};
        std::optional<CanonicalTransform> transform;
        std::optional<CanonicalRenderable> renderable;
        std::optional<CanonicalCamera> camera;
    };
    std::vector<DecodedArch> arches;
    for (std::uint32_t i = 0; i < archSection->records; ++i) {
        auto mask = archReader.readU64();
        auto payloadBytes = archReader.readU32();
        if (!mask || !payloadBytes)
            return core::unexpected(fail("packed.arch.invalid", "ARCH row is invalid"));
        auto payload = archReader.readBytes(*payloadBytes);
        if (!payload)
            return core::unexpected(std::move(payload.error()));
        ByteReader p(*payload);
        DecodedArch value{.mask = *mask,
                          .transform = std::nullopt,
                          .renderable = std::nullopt,
                          .camera = std::nullopt};
        if (*mask & 1U) {
            auto transform = CanonicalTransform{};
            auto x = readF32(p);
            auto y = readF32(p);
            auto z = readF32(p);
            auto rx = readF32(p);
            auto ry = readF32(p);
            auto rz = readF32(p);
            auto rw = readF32(p);
            auto sx = readF32(p);
            auto sy = readF32(p);
            auto sz = readF32(p);
            if (!x || !y || !z || !rx || !ry || !rz || !rw || !sx || !sy || !sz)
                return core::unexpected(fail("packed.arch.transform", "ARCH transform is invalid"));
            transform.position = {*x, *y, *z};
            transform.rotation = {*rx, *ry, *rz, *rw};
            transform.scale = {*sx, *sy, *sz};
            value.transform = transform;
        }
        if (*mask & 2U) {
            auto mesh = p.readUnsignedLeb128(*refCount);
            auto material = p.readUnsignedLeb128(*refCount);
            auto alpha = p.readU8();
            if (!mesh || !material || !alpha)
                return core::unexpected(
                    fail("packed.arch.renderable", "ARCH renderable is invalid"));
            auto meshValue = refValue(*mesh, 1);
            auto materialValue = refValue(*material, 1);
            if (!meshValue || !materialValue)
                return core::unexpected(
                    fail("packed.arch.reference", "ARCH renderable reference is invalid"));
            value.renderable =
                CanonicalRenderable{AssetId{*meshValue}, AssetId{*materialValue}, *alpha};
        }
        if (*mask & 16U) {
            auto type = p.readU8();
            auto f = readF64(p);
            auto n = readF64(p);
            auto farValue = readF64(p);
            if (!type || !f || !n || !farValue || *type != 1)
                return core::unexpected(fail("packed.arch.camera", "ARCH camera is invalid"));
            value.camera = CanonicalCamera{"perspective", *f, *n, *farValue};
        }
        if (!p.empty())
            return core::unexpected(
                fail("packed.arch.trailing", "ARCH payload has trailing bytes"));
        arches.push_back(std::move(value));
    }
    std::vector<std::uint32_t> parents(*entityCount), archetypes(*entityCount);
    const auto* entSection = section("ENT0");
    ByteReader ent(entSection->data);
    for (std::uint32_t i = 0; i < *entityCount; ++i) {
        auto parent = ent.readUnsignedLeb128(*entityCount + 1U);
        auto arch = ent.readUnsignedLeb128(arches.size());
        if (!parent || !arch || *arch >= arches.size() || *parent > *entityCount ||
            (*parent != 0 && *parent - 1U == i))
            return core::unexpected(fail("packed.entities.invalid", "ENT0 row is invalid"));
        parents[i] = static_cast<std::uint32_t>(*parent);
        archetypes[i] = static_cast<std::uint32_t>(*arch);
        entities[i].components.clear();
        const auto& a = arches[*arch];
        if (a.transform)
            entities[i].components.push_back(*a.transform);
        if (a.renderable)
            entities[i].components.push_back(*a.renderable);
        if (a.camera)
            entities[i].components.push_back(*a.camera);
    }
    if (!ent.empty())
        return core::unexpected(fail("packed.entities.trailing", "ENT0 has trailing bytes"));
    for (std::uint32_t start = 0; start < *entityCount; ++start) {
        std::vector<std::uint8_t> visited(*entityCount, 0);
        auto current = start;
        while (parents[current] != 0) {
            if (visited[current] != 0)
                return core::unexpected(
                    fail("packed.entities.parent_cycle", "ENT0 parent graph contains a cycle"));
            visited[current] = 1;
            current = parents[current] - 1U;
        }
    }
    auto applyStreams = [&](std::string_view code, int kind) -> core::Result<void> {
        const auto* stream = section(code);
        if (!stream)
            return {};
        ByteReader rows(stream->data);
        std::uint32_t previous = 0;
        for (std::uint32_t row = 0; row < stream->records; ++row) {
            auto ordinal = rows.readUnsignedLeb128(*entityCount);
            auto mask = rows.readUnsignedLeb128();
            if (!ordinal || !mask || *ordinal >= *entityCount || (row != 0 && *ordinal <= previous))
                return core::unexpected(
                    fail("packed.stream.ordinal", "Component stream ordinal is invalid"));
            previous = static_cast<std::uint32_t>(*ordinal);
            auto& components = entities[*ordinal].components;
            auto archIndex = archetypes[*ordinal];
            const auto& a = arches[archIndex];
            if (kind == 0 && !a.transform)
                return core::unexpected(fail("packed.stream.component",
                                             "Transform stream row has no Transform component"));
            if (kind == 1 && !a.renderable)
                return core::unexpected(fail("packed.stream.component",
                                             "Renderable stream row has no Renderable component"));
            if (kind == 2 && !a.camera)
                return core::unexpected(
                    fail("packed.stream.component", "Camera stream row has no Camera component"));
            if (kind == 0) {
                auto value = *a.transform;
                if (*mask & 1U) {
                    auto x = readF32(rows);
                    auto y = readF32(rows);
                    auto z = readF32(rows);
                    if (!x || !y || !z)
                        return core::unexpected(
                            fail("packed.stream.transform", "Transform row is invalid"));
                    value.position = {*x, *y, *z};
                }
                if (*mask & 8U) {
                    auto x = readF32(rows);
                    auto y = readF32(rows);
                    auto z = readF32(rows);
                    auto w = readF32(rows);
                    if (!x || !y || !z || !w)
                        return core::unexpected(
                            fail("packed.stream.transform", "Transform row is invalid"));
                    value.rotation = {*x, *y, *z, *w};
                }
                if (*mask & 112U) {
                    auto x = readF32(rows);
                    auto y = readF32(rows);
                    auto z = readF32(rows);
                    if (!x || !y || !z)
                        return core::unexpected(
                            fail("packed.stream.transform", "Transform row is invalid"));
                    value.scale = {*x, *y, *z};
                }
                components.erase(
                    std::remove_if(components.begin(), components.end(),
                                   [](const auto& c) {
                                       return std::holds_alternative<CanonicalTransform>(c);
                                   }),
                    components.end());
                components.push_back(value);
            } else if (kind == 1) {
                auto value = *a.renderable;
                if (*mask & 1U) {
                    auto ref = rows.readUnsignedLeb128(*refCount);
                    if (!ref)
                        return core::unexpected(std::move(ref.error()));
                    auto text = refValue(*ref, 1);
                    if (!text)
                        return core::unexpected(std::move(text.error()));
                    value.mesh = AssetId{*text};
                }
                if (*mask & 2U) {
                    auto ref = rows.readUnsignedLeb128(*refCount);
                    if (!ref)
                        return core::unexpected(std::move(ref.error()));
                    auto text = refValue(*ref, 1);
                    if (!text)
                        return core::unexpected(std::move(text.error()));
                    value.material = AssetId{*text};
                }
                if (*mask & 4U) {
                    auto alpha = rows.readU8();
                    if (!alpha)
                        return core::unexpected(std::move(alpha.error()));
                    value.alpha = *alpha;
                }
                components.erase(
                    std::remove_if(components.begin(), components.end(),
                                   [](const auto& c) {
                                       return std::holds_alternative<CanonicalRenderable>(c);
                                   }),
                    components.end());
                components.push_back(value);
            } else {
                auto value = *a.camera;
                if (*mask & 1U) {
                    auto type = rows.readU8();
                    auto f = readF64(rows);
                    auto n = readF64(rows);
                    auto farValue = readF64(rows);
                    if (!type || !f || !n || !farValue || *type != 1)
                        return core::unexpected(
                            fail("packed.stream.camera", "Camera row is invalid"));
                    value.fovY = *f;
                    value.nearPlane = *n;
                    value.farPlane = *farValue;
                }
                components.erase(
                    std::remove_if(
                        components.begin(), components.end(),
                        [](const auto& c) { return std::holds_alternative<CanonicalCamera>(c); }),
                    components.end());
                components.push_back(value);
            }
        }
        if (!rows.empty())
            return core::unexpected(
                fail("packed.stream.trailing", "Component stream has trailing bytes"));
        return {};
    };
    if (auto result = applyStreams("TRN0", 0); !result)
        return core::unexpected(std::move(result.error()));
    if (auto result = applyStreams("REN0", 1); !result)
        return core::unexpected(std::move(result.error()));
    if (auto result = applyStreams("CAM0", 2); !result)
        return core::unexpected(std::move(result.error()));
    const auto* cnsSection = section("CNS0");
    ByteReader cns(cnsSection->data);
    std::vector<std::uint32_t> lanes;
    for (std::uint32_t i = 0; i < cnsSection->records; ++i) {
        auto kind = cns.readU8();
        auto lane = cns.readUnsignedLeb128();
        if (!kind || !lane || *kind != 1 || *lane > 3U)
            return core::unexpected(fail("packed.constraints.invalid", "CNS0 lane set is invalid"));
        lanes.push_back(static_cast<std::uint32_t>(*lane));
    }
    if (!cns.empty())
        return core::unexpected(fail("packed.constraints.trailing", "CNS0 has trailing bytes"));
    const auto* reqSection = section("REQ0");
    ByteReader req(reqSection->data);
    auto startMode = req.readU8();
    auto endMode = req.readU8();
    if (!startMode || !endMode || *startMode != 0 || *endMode != 0)
        return core::unexpected(
            fail("packed.requirements.codec", "REQ0 Beat codec is unsupported"));
    std::uint32_t ordinal = 0;
    for (std::uint32_t i = 0; i < *requirementCount; ++i) {
        auto delta = req.readUnsignedLeb128(*entityCount);
        auto local = req.readUnsignedLeb128(*stringCount);
        auto kind = req.readU8();
        auto interval = req.readU8();
        auto start = readRationalBeatAtom(req);
        if (!delta || !local || !kind || !interval || !start)
            return core::unexpected(fail("packed.requirements.invalid", "REQ0 row is invalid"));
        if (i == 0) {
            if (*entityCount != 0 && *delta >= *entityCount)
                return core::unexpected(
                    fail("packed.requirements.invalid", "REQ0 first ordinal is invalid"));
            ordinal = static_cast<std::uint32_t>(*delta);
        } else {
            if (*delta > *entityCount || ordinal > *entityCount - *delta)
                return core::unexpected(
                    fail("packed.requirements.invalid", "REQ0 ordinal delta overflows"));
            ordinal += static_cast<std::uint32_t>(*delta);
        }
        if (ordinal >= *entityCount || *kind != 1 || *interval > 1)
            return core::unexpected(
                fail("packed.requirements.profile", "Requirement is outside Foundation profile"));
        auto localValue = getString(*local);
        if (!localValue)
            return core::unexpected(std::move(localValue.error()));
        CanonicalRequirement requirement;
        requirement.localId = *localValue;
        requirement.kind = CanonicalRequirementKind::Tap;
        requirement.interval.kind = static_cast<CanonicalIntervalKind>(*interval);
        requirement.interval.startBeat = *start;
        if (*interval == 1) {
            auto end = readRationalBeatAtom(req);
            if (!end)
                return core::unexpected(std::move(end.error()));
            requirement.interval.endBeat = *end;
        }
        auto domain = req.readUnsignedLeb128(*refCount);
        auto action = req.readUnsignedLeb128(*refCount);
        auto constraint = req.readUnsignedLeb128();
        auto effect = req.readUnsignedLeb128();
        if (!domain || !action || !constraint || !effect || *effect != 0 || *constraint == 0 ||
            *constraint > lanes.size())
            return core::unexpected(
                fail("packed.requirements.profile", "Requirement references unsupported profile"));
        auto domainValue = refValue(*domain, 4);
        auto actionValue = refValue(*action, 5);
        if (!domainValue || !actionValue || *domainValue != "candidate.lanes4" ||
            *actionValue != "press")
            return core::unexpected(
                fail("packed.requirements.profile", "Requirement domain/action is unsupported"));
        requirement.judgementDomain = {"candidate.lanes4", *domainValue};
        requirement.requiredAction = {"action", *actionValue};
        requirement.constraints.push_back(LaneConstraint{lanes[*constraint - 1U]});
        entities[ordinal].requirements.push_back(std::move(requirement));
    }
    if (!req.empty())
        return core::unexpected(fail("packed.requirements.trailing", "REQ0 has trailing bytes"));
    for (std::uint32_t i = 0; i < *entityCount; ++i) {
        if (parents[i] != 0)
            entities[i].parent = entities[parents[i] - 1U].identity;
        if (!entities[i].requirements.empty() && !(entities[i].componentMask() & (1U << 2U)))
            return core::unexpected(
                fail("packed.requirements.component", "Requirement presence bit is missing"));
    }
    chart.entities = std::move(entities);
    return chart;
}

} // namespace cuexis::chart::packed
