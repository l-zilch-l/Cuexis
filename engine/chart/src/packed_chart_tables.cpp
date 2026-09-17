#include <cuexis/chart/packed_chart_tables.hpp>

#include <cuexis/chart/uuid.hpp>
#include <cuexis/core/error.hpp>

#include "packed_identity_internal.hpp"
#include "packed_limits_internal.hpp"
#include "packed_profile_internal.hpp"

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
#include <variant>

namespace cuexis::chart::packed {
namespace {

auto fail(std::string code, std::string message) -> core::Error {
    return core::Error{std::move(code), std::move(message)};
}

// Checked narrowing for the fixed-width wire fields of the artifact. Every call site is preceded
// by a budget gate that makes the value fit; the check keeps that provable instead of implicit.
[[nodiscard]] auto narrowU32(std::uint64_t value) -> core::Result<std::uint32_t> {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        return core::unexpected(
            fail("packed.budget.section_bytes", "Packed field exceeds the uint32 wire range"));
    }
    return static_cast<std::uint32_t>(value);
}

// Spec 5.3 section registry. inspect() and decode() share this decision so the two entry points
// cannot disagree about which sections an artifact may carry.
enum class SectionRole : std::uint8_t { semantic, inspection };

[[nodiscard]] auto classifySection(std::string_view name, std::uint8_t flags)
    -> core::Result<SectionRole> {
    static constexpr std::array<std::string_view, 12> semantic{"META", "TIME", "STR0", "REF0",
                                                               "IDN0", "ARCH", "ENT0", "TRN0",
                                                               "REN0", "CAM0", "CNS0", "REQ0"};
    static constexpr std::array<std::string_view, 4> refused{"ANM0", "BEH0", "BHD0", "FXS0"};
    if (flags > 1U) {
        return core::unexpected(
            fail("packed.directory.flags", "Packed section flags must be 0 or 1"));
    }
    if (std::find(semantic.begin(), semantic.end(), name) != semantic.end()) {
        if (flags != 0U) {
            return core::unexpected(
                fail("packed.directory.flags", "Foundation semantic sections require flags=0"));
        }
        return SectionRole::semantic;
    }
    if (name == "DBG0") {
        if (flags != 1U) {
            return core::unexpected(
                fail("packed.directory.flags",
                     "The registered inspection section DBG0 requires flags=1"));
        }
        return SectionRole::inspection;
    }
    if (std::find(refused.begin(), refused.end(), name) != refused.end()) {
        if (flags != 0U) {
            return core::unexpected(
                fail("packed.directory.flags", "Foundation semantic sections require flags=0"));
        }
        return core::unexpected(
            fail("packed.directory.refused",
                 "Packed section belongs to a later field contract and is refused by Foundation"));
    }
    // Spec 5.2: unknown inspection sections may be ignored after length and CRC checks, but they
    // never provide runtime data. Unknown semantic sections are always refused.
    if (flags == 1U) {
        return SectionRole::inspection;
    }
    return core::unexpected(
        fail("packed.directory.invalid", "Packed section code is not registered"));
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

auto identityText(std::span<const std::byte> bytes) -> std::string {
    static constexpr char digits[] = "0123456789abcdef";
    std::string value;
    value.reserve(36);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        if (index == 4U || index == 6U || index == 8U || index == 10U)
            value.push_back('-');
        const auto byteValue = std::to_integer<std::uint8_t>(bytes[index]);
        value.push_back(digits[byteValue >> 4]);
        value.push_back(digits[byteValue & 15]);
    }
    return value;
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

auto identityKey(const CanonicalEntityIdentity& identity) -> core::Result<std::vector<std::byte>> {
    return identity_detail::canonicalIdentityBytes(identity);
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

auto writeStringSection(const Dictionaries& dict) -> core::Result<Section> {
    std::uint64_t dataBytes = 0;
    for (const auto& value : dict.strings)
        dataBytes += value.size();
    const auto count = narrowU32(dict.strings.size());
    if (!count)
        return core::unexpected(std::move(count.error()));
    const auto dataSize = narrowU32(dataBytes);
    if (!dataSize)
        return core::unexpected(std::move(dataSize.error()));
    ByteWriter writer;
    writer.writeU32(*count);
    writer.writeU32(*dataSize);
    std::uint64_t offset = 0;
    writer.writeU32(0);
    for (const auto& value : dict.strings) {
        offset += value.size();
        auto next = narrowU32(offset);
        if (!next)
            return core::unexpected(std::move(next.error()));
        writer.writeU32(*next);
    }
    for (const auto& value : dict.strings)
        writer.writeBytes(std::as_bytes(std::span{value.data(), value.size()}));
    return Section{{'S', 'T', 'R', '0'}, std::move(writer).takeBytes(), *count};
}

auto writeReferenceSection(const Dictionaries& dict) -> core::Result<Section> {
    const auto count = narrowU32(dict.refIndex.size());
    if (!count)
        return core::unexpected(std::move(count.error()));
    ByteWriter writer;
    for (const auto& [key, index] : dict.refIndex) {
        writer.writeU8(key.first);
        auto ref = stringRef(dict, key.second);
        if (!ref)
            return core::unexpected(std::move(ref.error()));
        writer.writeUnsignedLeb128(*ref);
    }
    return Section{{'R', 'E', 'F', '0'}, std::move(writer).takeBytes(), *count};
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
    const auto featureCount = narrowU32(chart.features.size());
    if (!featureCount)
        return core::unexpected(std::move(featureCount.error()));
    writer.writeU32(*featureCount);
    for (const auto& feature : chart.features) {
        auto r = typedRef(dict, 7, feature.id);
        if (!r)
            return core::unexpected(std::move(r.error()));
        writer.writeUnsignedLeb128(*r);
        writer.writeU32(feature.version);
    }
    return Section{{'M', 'E', 'T', 'A'}, std::move(writer).takeBytes(), 1};
}

auto writeTimeSection(const ChartTiming& timing) -> core::Result<Section> {
    const auto tempoCount = narrowU32(timing.tempoEvents.size());
    if (!tempoCount)
        return core::unexpected(std::move(tempoCount.error()));
    const auto stopCount = narrowU32(timing.stops.size());
    if (!stopCount)
        return core::unexpected(std::move(stopCount.error()));
    ByteWriter writer;
    writeF64(writer, timing.offsetMs);
    writeF64(writer, timing.defaultBpm);
    writer.writeU32(*tempoCount);
    writer.writeU32(*stopCount);
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
    return Section{{'T', 'I', 'M', 'E'}, std::move(writer).takeBytes(), 1};
}

struct EntityOrder final {
    std::vector<const CanonicalEntity*> entities;
    std::map<std::vector<std::byte>, std::uint32_t> ordinals;
};
// Spec 6.5: canonical identity bytes define both the entity ordinal and the ENT0 order.
auto orderEntities(const CanonicalSemanticChart& chart) -> core::Result<EntityOrder> {
    EntityOrder order;
    order.entities.reserve(chart.entities.size());
    for (const auto& entity : chart.entities)
        order.entities.push_back(&entity);
    std::vector<std::pair<std::vector<std::byte>, const CanonicalEntity*>> keyed;
    keyed.reserve(order.entities.size());
    for (const auto* entity : order.entities) {
        auto bytes = identityKey(entity->identity);
        if (!bytes)
            return core::unexpected(std::move(bytes.error()));
        keyed.emplace_back(std::move(*bytes), entity);
    }
    std::sort(keyed.begin(), keyed.end(),
              [](const auto& left, const auto& right) { return left.first < right.first; });
    order.entities.clear();
    for (std::size_t i = 0; i < keyed.size(); ++i) {
        order.entities.push_back(keyed[i].second);
        if (!order.ordinals.emplace(keyed[i].first, static_cast<std::uint32_t>(i)).second) {
            return core::unexpected(fail("packed.identity.duplicate_identity",
                                         "Entity identities must be unique in a candidate chart"));
        }
    }
    return order;
}

struct IdentityScope final {
    std::array<std::uint8_t, 16> chartId{};
    std::string bindingId;
    std::string moduleId;
    std::string exportId;
};
struct IdentityPath final {
    std::vector<std::pair<std::string, bool>> steps;
};

auto scopeLess(const IdentityScope& left, const IdentityScope& right) -> bool {
    if (left.chartId != right.chartId)
        return left.chartId < right.chartId;
    if (left.bindingId != right.bindingId)
        return left.bindingId < right.bindingId;
    if (left.moduleId != right.moduleId)
        return left.moduleId < right.moduleId;
    return left.exportId < right.exportId;
}
auto scopeEqual(const IdentityScope& left, const IdentityScope& right) -> bool {
    return left.chartId == right.chartId && left.bindingId == right.bindingId &&
           left.moduleId == right.moduleId && left.exportId == right.exportId;
}
auto pathLess(const IdentityPath& left, const IdentityPath& right) -> bool {
    return left.steps < right.steps;
}
auto pathEqual(const IdentityPath& left, const IdentityPath& right) -> bool {
    return left.steps == right.steps;
}

auto writeIdentitySection(const EntityOrder& order, const CanonicalSemanticChart&,
                          const Dictionaries& dict) -> core::Result<Section> {
    std::vector<IdentityScope> scopes;
    std::vector<IdentityPath> paths;
    for (const auto* entity : order.entities) {
        const auto* generated = std::get_if<GeneratedEntityIdentity>(&entity->identity);
        if (generated == nullptr)
            continue;
        auto chartId = identity_detail::uuidBytes(generated->chartId.value);
        if (!chartId)
            return core::unexpected(std::move(chartId.error()));
        scopes.push_back(IdentityScope{*chartId, generated->bindingId, generated->moduleId,
                                       generated->exportId});
        IdentityPath path;
        for (const auto& step : generated->path)
            path.steps.emplace_back(step.nodeId, step.iterationIndexPlusOne != 0U);
        paths.push_back(std::move(path));
    }
    std::sort(scopes.begin(), scopes.end(), scopeLess);
    scopes.erase(std::unique(scopes.begin(), scopes.end(), scopeEqual), scopes.end());
    std::sort(paths.begin(), paths.end(), pathLess);
    paths.erase(std::unique(paths.begin(), paths.end(), pathEqual), paths.end());

    ByteWriter writer;
    writer.writeU32(static_cast<std::uint32_t>(scopes.size()));
    for (const auto& scope : scopes) {
        writer.writeBytes(std::as_bytes(std::span{scope.chartId}));
        for (const auto& value : {scope.bindingId, scope.moduleId, scope.exportId}) {
            auto index = stringRef(dict, value);
            if (!index)
                return core::unexpected(std::move(index.error()));
            writer.writeUnsignedLeb128(*index);
        }
    }
    writer.writeU32(static_cast<std::uint32_t>(paths.size()));
    for (const auto& path : paths) {
        writer.writeUnsignedLeb128(path.steps.size());
        for (const auto& [nodeId, indexed] : path.steps) {
            auto index = stringRef(dict, nodeId);
            if (!index)
                return core::unexpected(std::move(index.error()));
            writer.writeUnsignedLeb128(*index);
            writer.writeU8(indexed ? 1U : 0U);
        }
    }
    writer.writeU32(static_cast<std::uint32_t>(order.entities.size()));
    for (const auto* entity : order.entities) {
        if (const auto* explicitIdentity = std::get_if<ExplicitEntityIdentity>(&entity->identity)) {
            writer.writeU8(0);
            auto r = writeUuid(writer, explicitIdentity->objectId.value);
            if (!r)
                return core::unexpected(std::move(r.error()));
            continue;
        }
        const auto& generated = std::get<GeneratedEntityIdentity>(entity->identity);
        auto chartId = identity_detail::uuidBytes(generated.chartId.value);
        if (!chartId)
            return core::unexpected(std::move(chartId.error()));
        const IdentityScope scope{*chartId, generated.bindingId, generated.moduleId,
                                  generated.exportId};
        IdentityPath path;
        for (const auto& step : generated.path)
            path.steps.emplace_back(step.nodeId, step.iterationIndexPlusOne != 0U);
        const auto scopeIt = std::lower_bound(scopes.begin(), scopes.end(), scope, scopeLess);
        const auto pathIt = std::lower_bound(paths.begin(), paths.end(), path, pathLess);
        writer.writeU8(1);
        writer.writeUnsignedLeb128(
            static_cast<std::uint32_t>(std::distance(scopes.begin(), scopeIt)));
        writer.writeUnsignedLeb128(
            static_cast<std::uint32_t>(std::distance(paths.begin(), pathIt)));
        for (const auto& step : generated.path) {
            if (step.iterationIndexPlusOne != 0U)
                writer.writeUnsignedLeb128(step.iterationIndexPlusOne);
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
                        const std::map<std::uint64_t, std::uint32_t>& arches)
    -> core::Result<Section> {
    ByteWriter writer;
    for (const auto* entity : order.entities) {
        std::uint32_t parent = 0;
        if (entity->parent) {
            auto parentBytes = identityKey(*entity->parent);
            if (!parentBytes)
                return core::unexpected(std::move(parentBytes.error()));
            const auto it = order.ordinals.find(*parentBytes);
            if (it == order.ordinals.end()) {
                return core::unexpected(fail("packed.identity.parent_missing",
                                             "Entity parent is not part of the chart"));
            }
            parent = it->second + 1U;
        }
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
    // Spec 7.5: constraint sets are deduplicated and ordered by their canonical payload bytes,
    // which for the single registered lane constraint means ascending lane.
    std::set<std::uint32_t> laneValues;
    for (const auto* entity : order.entities) {
        for (const auto& req : entity->requirements) {
            if (req.constraints.size() != 1U ||
                !std::holds_alternative<LaneConstraint>(req.constraints[0])) {
                return core::unexpected(fail("packed.profile.constraints",
                                             "Foundation requirements carry exactly one lane "
                                             "constraint"));
            }
            laneValues.insert(std::get<LaneConstraint>(req.constraints[0]).lane);
        }
    }
    std::map<std::uint32_t, std::uint32_t> laneSets;
    for (const auto lane : laneValues) {
        laneSets.emplace(lane, static_cast<std::uint32_t>(laneSets.size() + 1U));
    }
    ByteWriter cns;
    for (const auto lane : laneValues) {
        cns.writeU8(1);
        cns.writeUnsignedLeb128(lane);
    }
    Section cnsSection{{'C', 'N', 'S', '0'},
                       std::move(cns).takeBytes(),
                       static_cast<std::uint32_t>(laneValues.size())};
    ByteWriter req;
    req.writeU8(0);
    req.writeU8(0);
    std::uint32_t count = 0;
    std::uint32_t previous = 0;
    bool first = true;
    for (std::size_t ordinal = 0; ordinal < order.entities.size(); ++ordinal) {
        const auto* entity = order.entities[ordinal];
        // Spec 7.4: REQ0 rows are stored in strictly ascending (entity ordinal, localId bytes)
        // order, so the Writer emits each entity's requirements sorted by localId.
        std::vector<const CanonicalRequirement*> requirements;
        requirements.reserve(entity->requirements.size());
        for (const auto& requirement : entity->requirements) {
            requirements.push_back(&requirement);
        }
        std::sort(
            requirements.begin(), requirements.end(),
            [](const auto* left, const auto* right) { return left->localId < right->localId; });
        for (const auto* requirement : requirements) {
            const auto delta = first ? static_cast<std::uint32_t>(ordinal)
                                     : static_cast<std::uint32_t>(ordinal - previous);
            first = false;
            previous = static_cast<std::uint32_t>(ordinal);
            req.writeUnsignedLeb128(delta);
            req.writeUnsignedLeb128(*stringRef(dict, requirement->localId));
            req.writeU8(static_cast<std::uint8_t>(requirement->kind));
            req.writeU8(static_cast<std::uint8_t>(requirement->interval.kind));
            (void)writeRationalBeatAtom(req, requirement->interval.startBeat);
            if (requirement->interval.endBeat)
                (void)writeRationalBeatAtom(req, *requirement->interval.endBeat);
            req.writeUnsignedLeb128(*typedRef(dict, 4, requirement->judgementDomain.id));
            req.writeUnsignedLeb128(*typedRef(dict, 5, requirement->requiredAction.id));
            const auto lane = std::get<LaneConstraint>(requirement->constraints[0]).lane;
            req.writeUnsignedLeb128(laneSets.at(lane));
            req.writeU8(0);
            ++count;
        }
    }
    return std::make_pair(std::move(cnsSection),
                          Section{{'R', 'E', 'Q', '0'}, std::move(req).takeBytes(), count});
}

} // namespace

auto encode(const CanonicalSemanticChart& chart, PackedChartProfile profile,
            PackedChartLimits limits) -> core::Result<std::vector<std::byte>> {
    if (profile.flags != 1 || profile.candidateRevision != 1)
        return core::unexpected(
            fail("packed.header.unsupported_revision", "Only candidate revision 1 is supported"));
    // Spec 7.6: the Writer refuses any model outside the registered Foundation subset before
    // computing an identity or emitting bytes, so an out-of-profile artifact is never published.
    if (auto registered = profile_detail::validateFoundationProfile(chart); !registered)
        return core::unexpected(std::move(registered.error()));
    const auto budget = limits_detail::effectiveLimits(limits);
    // Spec 3.2: counts are checked before the identity preimage and before any allocation sized
    // by them. The requirement total is accumulated in 64 bits so the sum itself cannot wrap.
    if (chart.entities.size() > budget.maxPackedEntities)
        return core::unexpected(
            fail("packed.budget.entities", "Packed chart exceeds the entity budget"));
    std::uint64_t requirementCount = 0;
    for (const auto& entity : chart.entities)
        requirementCount += entity.requirements.size();
    if (requirementCount > budget.maxPackedRequirements)
        return core::unexpected(
            fail("packed.budget.requirements", "Packed chart exceeds the requirement budget"));
    // Validate the hash preconditions and compute the semantic identity before any artifact
    // bytes exist, so an inconsistent chart is never published (Spec 9 and 10.1).
    auto identity = semanticIdentity(chart);
    if (!identity)
        return core::unexpected(std::move(identity.error()));
    Dictionaries dict;
    collectStrings(dict, chart);
    if (dict.strings.size() > budget.maxPackedStrings)
        return core::unexpected(
            fail("packed.budget.strings", "Packed chart exceeds the string budget"));
    if (dict.refIndex.size() > budget.maxPackedReferences)
        return core::unexpected(
            fail("packed.budget.references", "Packed chart exceeds the typed reference budget"));
    auto ordered = orderEntities(chart);
    if (!ordered)
        return core::unexpected(std::move(ordered.error()));
    const auto& order = *ordered;
    std::map<std::uint64_t, std::uint32_t> archIndices;
    auto arches = makeArchetypes(order, archIndices);
    std::vector<Section> sections;
    auto strings = writeStringSection(dict);
    if (!strings)
        return core::unexpected(std::move(strings.error()));
    sections.push_back(std::move(*strings));
    auto refs = writeReferenceSection(dict);
    if (!refs)
        return core::unexpected(std::move(refs.error()));
    sections.push_back(std::move(*refs));
    auto meta = writeMetaSection(chart, dict);
    if (!meta)
        return core::unexpected(std::move(meta.error()));
    sections.push_back(std::move(*meta));
    auto time = writeTimeSection(chart.timing);
    if (!time)
        return core::unexpected(std::move(time.error()));
    sections.push_back(std::move(*time));
    auto idn = writeIdentitySection(order, chart, dict);
    if (!idn)
        return core::unexpected(std::move(idn.error()));
    sections.push_back(std::move(*idn));
    auto arch = writeArchetypeSection(arches, dict);
    if (!arch)
        return core::unexpected(std::move(arch.error()));
    sections.push_back(std::move(*arch));
    auto entities = writeEntitySection(order, archIndices);
    if (!entities)
        return core::unexpected(std::move(entities.error()));
    sections.push_back(std::move(*entities));
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
    // Spec 3.1 and 3.2: the exact artifact envelope is computed and checked before the file is
    // assembled and before any size is narrowed to a fixed-width header field, so an over-budget
    // model produces no partial artifact and no wrapped counter.
    std::uint64_t decodedBytes = 0;
    for (const auto& section : sections) {
        if (section.bytes.size() > budget.maxPackedSectionBytes)
            return core::unexpected(
                fail("packed.budget.section_bytes", "Packed section exceeds the section budget"));
        decodedBytes += section.bytes.size();
    }
    if (decodedBytes > budget.maxPackedDecodedBytes)
        return core::unexpected(
            fail("packed.budget.decoded_bytes", "Packed chart exceeds the decoded byte budget"));
    const std::uint64_t directoryBytes = static_cast<std::uint64_t>(sections.size()) * 32U;
    const std::uint64_t totalBytes = 96U + directoryBytes + decodedBytes;
    if (totalBytes > budget.maxPackedFileBytes)
        return core::unexpected(
            fail("packed.budget.file_bytes", "Packed chart exceeds the file byte budget"));
    const auto sectionCount = narrowU32(sections.size());
    const auto directorySize = narrowU32(directoryBytes);
    const auto total = narrowU32(totalBytes);
    const auto decoded = narrowU32(decodedBytes);
    if (!sectionCount || !directorySize || !total || !decoded)
        return core::unexpected(
            fail("packed.budget.section_bytes", "Packed directory exceeds the uint32 wire range"));
    ByteWriter file;
    static constexpr std::array<std::byte, 8> magic{std::byte{'C'}, std::byte{'X'}, std::byte{'P'},
                                                    std::byte{'K'}, std::byte{'5'}, std::byte{0},
                                                    std::byte{0},   std::byte{0}};
    file.writeBytes(magic);
    file.writeU16(1);
    file.writeU16(96);
    file.writeU32(1);
    file.writeU32(*total);
    file.writeU32(96);
    file.writeU32(*sectionCount);
    file.writeU32(*directorySize);
    for (const auto value : *identity)
        file.writeU8(value);
    // These four counters are already bounded by the entity, requirement, string and reference
    // budgets checked above; narrowU32() keeps the wire narrowing explicit anyway.
    const auto entityTotal = narrowU32(order.entities.size());
    const auto requirementTotal = narrowU32(requirementCount);
    const auto stringTotal = narrowU32(dict.strings.size());
    const auto referenceTotal = narrowU32(dict.refIndex.size());
    if (!entityTotal || !requirementTotal || !stringTotal || !referenceTotal)
        return core::unexpected(
            fail("packed.budget.section_bytes", "Packed counters exceed the uint32 wire range"));
    file.writeU32(*entityTotal);
    file.writeU32(*requirementTotal);
    file.writeU32(0);
    file.writeU32(*decoded);
    file.writeU32(*stringTotal);
    file.writeU32(*referenceTotal);
    file.writeU32(1);
    file.writeU32(0);
    std::uint64_t offset = totalBytes - decodedBytes;
    for (const auto& section : sections) {
        const auto sectionOffset = narrowU32(offset);
        if (!sectionOffset)
            return core::unexpected(std::move(sectionOffset.error()));
        const auto sectionBytes = narrowU32(section.bytes.size());
        if (!sectionBytes)
            return core::unexpected(std::move(sectionBytes.error()));
        file.writeBytes(std::as_bytes(std::span{section.code.data(), section.code.size()}));
        file.writeU8(0);
        file.writeU8(0);
        file.writeU16(0);
        file.writeU32(*sectionOffset);
        file.writeU32(*sectionBytes);
        file.writeU32(*sectionBytes);
        file.writeU32(section.records);
        file.writeU32(crc32(section.bytes));
        file.writeU32(0);
        offset += section.bytes.size();
    }
    for (const auto& section : sections)
        file.writeBytes(section.bytes);
    auto result = std::move(file).takeBytes();
    const auto headerCrc = crc32(std::span<const std::byte>{result}.first(92));
    result[92] = static_cast<std::byte>(headerCrc & 0xffU);
    result[93] = static_cast<std::byte>((headerCrc >> 8U) & 0xffU);
    result[94] = static_cast<std::byte>((headerCrc >> 16U) & 0xffU);
    result[95] = static_cast<std::byte>((headerCrc >> 24U) & 0xffU);
    return result;
}

auto inspect(std::span<const std::byte> bytes, PackedChartLimits limits)
    -> core::Result<PackedChartStatistics> {
    const auto budget = limits_detail::effectiveLimits(limits);
    if (bytes.size() < 96)
        return core::unexpected(
            fail("packed.header.invalid_size", "Packed chart is smaller than the fixed header"));
    if (bytes.size() > budget.maxPackedFileBytes)
        return core::unexpected(
            fail("packed.budget.file_bytes", "Packed chart exceeds the file byte budget"));
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
    // Spec 3.2 and 5.1: the directory size is a checked uint64 product before any reservation and
    // before the directory is read, so a wrapped count cannot size an allocation.
    const auto directoryBytesExpected = static_cast<std::uint64_t>(*count) * 32U;
    if (*version != 1 || *header != 96 || *flags != 1 || *dirOff != 96 || *total != bytes.size() ||
        directoryBytesExpected > std::numeric_limits<std::uint32_t>::max() ||
        *dirBytes != static_cast<std::uint32_t>(directoryBytesExpected) ||
        static_cast<std::uint64_t>(*dirOff) + *dirBytes > bytes.size())
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
    // Spec 5.1: only the implemented candidate revision may be read, and the Foundation profile
    // declares no schedule events. Neither declaration may be ignored as "nothing to do".
    if (*revision != 1)
        return core::unexpected(
            fail("packed.header.unsupported_revision", "Only candidate revision 1 is supported"));
    if (*events != 0U)
        return core::unexpected(
            fail("packed.header.events", "Foundation revision 1 declares no schedule events"));
    // Spec 3.2: the declared counters are pre-checked per field, so a rejection names the budget
    // that was exceeded instead of a single aggregate counter diagnostic. The counters are only
    // a pre-check; decode() re-counts the actual tables and compares them below.
    if (*entities > budget.maxPackedEntities)
        return core::unexpected(
            fail("packed.budget.entities", "Packed chart exceeds the entity budget"));
    if (*requirements > budget.maxPackedRequirements)
        return core::unexpected(
            fail("packed.budget.requirements", "Packed chart exceeds the requirement budget"));
    if (*strings > budget.maxPackedStrings)
        return core::unexpected(
            fail("packed.budget.strings", "Packed chart exceeds the string budget"));
    if (*refs > budget.maxPackedReferences)
        return core::unexpected(
            fail("packed.budget.references", "Packed chart exceeds the typed reference budget"));
    if (*decoded > budget.maxPackedDecodedBytes)
        return core::unexpected(
            fail("packed.budget.decoded_bytes", "Packed chart exceeds the decoded byte budget"));
    PackedChartStatistics stats{bytes.size(), *decoded, *strings, *refs, *entities, *requirements};
    std::vector<std::pair<std::uint32_t, std::uint32_t>> ranges;
    ranges.reserve(*count);
    std::set<std::string> sectionNames;
    std::uint64_t decodedSum = 0;
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
        if (*codec != 0 || *res != 0 || *res2 != 0 || *enc != *dec || *off > bytes.size() ||
            *enc > bytes.size() - *off)
            return core::unexpected(
                fail("packed.directory.invalid", "Packed directory entry is invalid"));
        if (!sectionNames.insert(name).second)
            return core::unexpected(
                fail("packed.directory.duplicate", "Packed section is duplicated"));
        if (auto role = classifySection(name, *fl); !role)
            return core::unexpected(std::move(role.error()));
        // Spec 3.3: the per-section ceiling applies to every directory entry, including the
        // optional inspection sections whose payload a reader still has to read and CRC-check.
        // It precedes the section CRC so an over-budget section is refused before its payload is
        // touched.
        if (*dec > budget.maxPackedSectionBytes)
            return core::unexpected(
                fail("packed.budget.section_bytes", "Packed section exceeds the section budget"));
        if (crc32(bytes.subspan(*off, *enc)) != *crc)
            return core::unexpected(fail("packed.section.crc", "Packed section CRC mismatch"));
        ranges.emplace_back(*off, *enc);
        decodedSum += *dec;
    }
    std::sort(ranges.begin(), ranges.end());
    std::uint64_t expectedOffset = 96U + static_cast<std::uint64_t>(*dirBytes);
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
        SectionRole role{SectionRole::semantic};
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
        if (*codec != 0 || *reserved != 0 || *reserved2 != 0 || *encoded != *decoded ||
            *offset > bytes.size() || *encoded > bytes.size() - *offset)
            return core::unexpected(
                fail("packed.directory.invalid", "Packed directory entry is invalid"));
        // Spec 5.3: the same registry decision as inspect(). Foundation semantic sections are
        // parsed, the registered inspection section and unknown inspection sections are ignored,
        // and later-contract sections are refused.
        auto role = classifySection(code, *flags);
        if (!role)
            return core::unexpected(std::move(role.error()));
        auto data = bytes.subspan(*offset, *encoded);
        if (crc32(data) != *sectionCrc)
            return core::unexpected(fail("packed.section.crc", "Packed section CRC mismatch"));
        directories.push_back(Directory{std::move(code), data, *records, *role});
    }
    const auto section = [&](std::string_view code) -> const Directory* {
        for (const auto& value : directories)
            if (value.code == code && value.role == SectionRole::semantic)
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
    // Spec 3.2: the declared row count is bounded by the bytes that can actually carry the rows
    // (one u32 offset per row plus the two header words) before the offset table is reserved.
    if (strSection->data.size() < 8U ||
        static_cast<std::uint64_t>(*strCount) + 1U >
            (static_cast<std::uint64_t>(strSection->data.size()) - 8U) / 4U)
        return core::unexpected(
            fail("packed.strings.count", "STR0 row count exceeds the section payload"));
    const std::size_t offsetCount = static_cast<std::size_t>(*strCount) + 1U;
    std::vector<std::uint32_t> offsets;
    offsets.reserve(offsetCount);
    for (std::size_t i = 0; i < offsetCount; ++i) {
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
    // Spec 6.1: STR0 strings are deduplicated and strictly ascending by bytes. The repository
    // Writer already emits that order; a non-canonical artifact is refused instead of resolved.
    for (std::size_t i = 1; i < strings.size(); ++i) {
        if (!(strings[i - 1U] < strings[i]))
            return core::unexpected(
                fail("packed.strings.order", "STR0 strings must be unique and strictly ascending"));
    }
    auto getString = [&](std::uint64_t index) -> core::Result<std::string> {
        if (index >= strings.size())
            return core::unexpected(fail("packed.strings.index", "String index is out of range"));
        return strings[static_cast<std::size_t>(index)];
    };
    const auto* refSection = section("REF0");
    ByteReader refReader(refSection->data);
    // Spec 3.2: every REF0 row occupies at least a kind byte and a one-byte string index, so the
    // declared count is bounded by the payload before the row vector is reserved.
    if (static_cast<std::uint64_t>(*refCount) > refSection->data.size() / 2U)
        return core::unexpected(
            fail("packed.references.invalid", "REF0 row count exceeds the section payload"));
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
    // Spec 6.2: REF0 rows are deduplicated and strictly ascending by (kind, string bytes).
    for (std::size_t i = 1; i < refs.size(); ++i) {
        if (!(refs[i - 1U] < refs[i]))
            return core::unexpected(
                fail("packed.references.order",
                     "REF0 rows must be unique and strictly ascending by (kind, string)"));
    }
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
    if (!scopeCount)
        return core::unexpected(fail("packed.identity.invalid", "IDN0 scope count is invalid"));
    struct DecodedScope final {
        std::array<std::uint8_t, 16> chartIdBytes{};
        std::string chartId;
        std::string bindingId;
        std::string moduleId;
        std::string exportId;
    };
    // Spec 6.5: scope tables are deduplicated and ordered by their decoded tuple.
    const auto scopeLess = [](const DecodedScope& left, const DecodedScope& right) {
        if (left.chartIdBytes != right.chartIdBytes)
            return left.chartIdBytes < right.chartIdBytes;
        if (left.bindingId != right.bindingId)
            return left.bindingId < right.bindingId;
        if (left.moduleId != right.moduleId)
            return left.moduleId < right.moduleId;
        return left.exportId < right.exportId;
    };
    std::vector<DecodedScope> scopes;
    // Spec 3.2: one scope row occupies 16 chartId bytes plus three one-byte string indices at a
    // minimum, and the reservation is capped so a legal but large table grows with the rows it
    // actually contains instead of with the declared count.
    if (static_cast<std::uint64_t>(*scopeCount) > idn.remaining() / 19U)
        return core::unexpected(
            fail("packed.identity.invalid", "IDN0 scope count exceeds the section payload"));
    scopes.reserve(std::min<std::size_t>(*scopeCount, 4096U));
    for (std::uint32_t i = 0; i < *scopeCount; ++i) {
        auto uuid = idn.readBytes(16);
        auto bind = idn.readUnsignedLeb128(*stringCount);
        auto module = idn.readUnsignedLeb128(*stringCount);
        auto exportId = idn.readUnsignedLeb128(*stringCount);
        if (!uuid || !bind || !module || !exportId)
            return core::unexpected(fail("packed.identity.invalid", "IDN0 scope is invalid"));
        auto bindValue = getString(*bind);
        auto moduleValue = getString(*module);
        auto exportValue = getString(*exportId);
        if (!bindValue || !moduleValue || !exportValue)
            return core::unexpected(fail("packed.identity.string", "IDN0 scope string is invalid"));
        auto scopeChartIdBytes = std::array<std::uint8_t, 16>{};
        for (std::size_t byteIndex = 0; byteIndex < scopeChartIdBytes.size(); ++byteIndex)
            scopeChartIdBytes[byteIndex] = std::to_integer<std::uint8_t>((*uuid)[byteIndex]);
        scopes.push_back(DecodedScope{scopeChartIdBytes, identityText(*uuid), std::move(*bindValue),
                                      std::move(*moduleValue), std::move(*exportValue)});
        if (scopes.size() > 1U && !scopeLess(scopes[scopes.size() - 2U], scopes.back()))
            return core::unexpected(
                fail("packed.identity.order",
                     "IDN0 scopes must be unique and strictly ascending by decoded tuple"));
        // Spec 6.5: every generated identity scope must use the META chartId.
        if (scopes.back().chartId != chart.chartId.value)
            return core::unexpected(
                fail("packed.identity.scope_chart", "IDN0 scope chartId must match META"));
    }
    auto pathCount = idn.readU32();
    if (!pathCount)
        return core::unexpected(fail("packed.identity.invalid", "IDN0 path count is invalid"));
    std::vector<std::vector<std::pair<std::string, bool>>> paths;
    if (static_cast<std::uint64_t>(*pathCount) > idn.remaining())
        return core::unexpected(
            fail("packed.identity.invalid", "IDN0 path count exceeds the section payload"));
    paths.reserve(std::min<std::size_t>(*pathCount, 1024U));
    for (std::uint32_t i = 0; i < *pathCount; ++i) {
        auto steps = idn.readUnsignedLeb128();
        if (!steps)
            return core::unexpected(fail("packed.identity.invalid", "IDN0 path is invalid"));
        // Spec 3.2: one path step occupies a string index and an indexed flag, so the declared
        // step count is bounded by the remaining payload before the step vector is reserved.
        if (*steps > idn.remaining() / 2U)
            return core::unexpected(
                fail("packed.identity.invalid", "IDN0 step count exceeds the section payload"));
        auto path = std::vector<std::pair<std::string, bool>>{};
        path.reserve(std::min<std::size_t>(*steps, 256U));
        for (std::uint64_t step = 0; step < *steps; ++step) {
            auto node = idn.readUnsignedLeb128(*stringCount);
            auto indexed = idn.readU8();
            if (!node || !indexed || *indexed > 1U)
                return core::unexpected(fail("packed.identity.path", "IDN0 path step is invalid"));
            auto nodeValue = getString(*node);
            if (!nodeValue)
                return core::unexpected(std::move(nodeValue.error()));
            path.emplace_back(std::move(*nodeValue), *indexed == 1U);
        }
        paths.push_back(std::move(path));
        if (paths.size() > 1U && !(paths[paths.size() - 2U] < paths.back()))
            return core::unexpected(
                fail("packed.identity.order",
                     "IDN0 paths must be unique and strictly ascending by (nodeId, indexed)"));
    }
    auto identityCount = idn.readU32();
    if (!identityCount || *identityCount != *entityCount)
        return core::unexpected(fail("packed.identity.invalid", "IDN0 identity count is invalid"));
    for (std::uint32_t i = 0; i < *identityCount; ++i) {
        auto tag = idn.readU8();
        if (!tag)
            return core::unexpected(std::move(tag.error()));
        if (*tag == 0U) {
            auto uuid = idn.readBytes(16);
            if (!uuid)
                return core::unexpected(std::move(uuid.error()));
            entities[i].identity = ExplicitEntityIdentity{ChartObjectId{identityText(*uuid)}};
            continue;
        }
        if (*tag != 1U)
            return core::unexpected(fail("packed.identity.tag", "Identity tag is unsupported"));
        auto scopeIndex = idn.readUnsignedLeb128(scopes.size());
        auto pathIndex = idn.readUnsignedLeb128(paths.size());
        if (!scopeIndex || !pathIndex || *scopeIndex >= scopes.size() || *pathIndex >= paths.size())
            return core::unexpected(fail("packed.identity.path", "Generated identity is invalid"));
        GeneratedEntityIdentity generated;
        generated.chartId = ChartId{scopes[*scopeIndex].chartId};
        generated.bindingId = scopes[*scopeIndex].bindingId;
        generated.moduleId = scopes[*scopeIndex].moduleId;
        generated.exportId = scopes[*scopeIndex].exportId;
        for (const auto& [nodeId, indexed] : paths[*pathIndex]) {
            if (!indexed) {
                generated.path.push_back(SemanticIdentityStep{nodeId, 0});
                continue;
            }
            auto iteration = idn.readUnsignedLeb128();
            if (!iteration || *iteration == 0U)
                return core::unexpected(
                    fail("packed.identity.path", "Generated identity iteration is invalid"));
            // Spec 6.5: the repeat index is a uint32 field, so a larger wire value is refused
            // instead of being narrowed onto a different generated identity.
            if (*iteration > std::numeric_limits<std::uint32_t>::max())
                return core::unexpected(fail("packed.identity.path",
                                             "Generated identity iteration exceeds the uint32 "
                                             "range"));
            generated.path.push_back(
                SemanticIdentityStep{nodeId, static_cast<std::uint32_t>(*iteration)});
        }
        // Spec 6.5: the last path step must be a non-indexed emit label.
        if (generated.path.empty() || generated.path.back().iterationIndexPlusOne != 0U)
            return core::unexpected(
                fail("packed.identity.generated_path",
                     "Generated identity paths must end with a non-indexed step"));
        entities[i].identity = std::move(generated);
    }
    // Spec 6.5: canonical identity bytes define the entity ordinal, so the IDN0 identity records
    // must be strictly ascending and duplicate-free.
    std::vector<std::byte> previousIdentityBytes;
    for (std::uint32_t i = 0; i < *identityCount; ++i) {
        auto identityBytes = identity_detail::canonicalIdentityBytes(entities[i].identity);
        if (!identityBytes)
            return core::unexpected(std::move(identityBytes.error()));
        if (i != 0U && !(previousIdentityBytes < *identityBytes))
            return core::unexpected(
                fail("packed.identity.order",
                     "IDN0 identities must be unique and strictly ascending by canonical bytes"));
        previousIdentityBytes = std::move(*identityBytes);
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
        // Spec 7.1: Foundation registers component bits 0, 1, 2 and 4 only. Bits 3, 6, 7 and
        // 8..63 belong to later contracts and must be refused instead of silently dropped.
        if ((*mask & ~std::uint64_t{0x17U}) != 0U)
            return core::unexpected(
                fail("packed.arch.mask",
                     "ARCH component mask uses bits outside the Foundation registry"));
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
    // Spec 7.1: the canonical Writer emits exactly one archetype per distinct mask in ascending
    // mask order, so a Reader that resolves archetype indices by position must verify it.
    for (std::size_t i = 1; i < arches.size(); ++i) {
        if (!(arches[i - 1U].mask < arches[i].mask))
            return core::unexpected(
                fail("packed.arch.order",
                     "ARCH archetypes must be unique and strictly ascending by component mask"));
    }
    std::vector<std::uint32_t> parents(*entityCount), archetypes(*entityCount);
    const auto* entSection = section("ENT0");
    ByteReader ent(entSection->data);
    for (std::uint32_t i = 0; i < *entityCount; ++i) {
        auto parent = ent.readUnsignedLeb128(static_cast<std::uint64_t>(*entityCount) + 1U);
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
            // Spec 7.3: a difference row may only set the field atoms the stream actually
            // consumes; every other bit is undefined and must not be ignored.
            const auto allowedMask = kind == 0   ? std::uint64_t{0x79U}
                                     : kind == 1 ? std::uint64_t{0x07U}
                                                 : std::uint64_t{0x01U};
            if ((*mask & ~allowedMask) != 0U)
                return core::unexpected(
                    fail("packed.stream.mask",
                         "Component stream change mask uses undefined field bits"));
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
        if (!kind || !lane)
            return core::unexpected(fail("packed.constraints.invalid", "CNS0 lane set is invalid"));
        if (*kind != 1U)
            return core::unexpected(
                fail("packed.profile.constraints",
                     "Only the registered lane constraint kind is supported by Foundation"));
        // Spec 3.2: the wire lane is a uint32 field. A larger value must not be narrowed to a
        // registered lane, which would turn an illegal artifact into a legal model.
        if (*lane > std::numeric_limits<std::uint32_t>::max())
            return core::unexpected(
                fail("packed.constraints.invalid", "CNS0 lane value exceeds the uint32 range"));
        lanes.push_back(static_cast<std::uint32_t>(*lane));
    }
    if (!cns.empty())
        return core::unexpected(fail("packed.constraints.trailing", "CNS0 has trailing bytes"));
    // Spec 7.5: sets are deduplicated and ordered by their canonical payload bytes. The lane
    // value itself is validated on the typed model by the profile gate below.
    for (std::size_t i = 1; i < lanes.size(); ++i) {
        if (!(lanes[i - 1U] < lanes[i]))
            return core::unexpected(
                fail("packed.constraints.order",
                     "CNS0 sets must be unique and strictly ascending by lane"));
    }
    const auto* reqSection = section("REQ0");
    ByteReader req(reqSection->data);
    auto startMode = req.readU8();
    auto endMode = req.readU8();
    if (!startMode || !endMode || *startMode != 0 || *endMode != 0)
        return core::unexpected(
            fail("packed.requirements.codec", "REQ0 Beat codec is unsupported"));
    std::uint32_t ordinal = 0;
    std::uint32_t previousOrdinal = 0;
    std::string previousLocalId;
    bool hasPrevious = false;
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
        if (ordinal >= *entityCount)
            return core::unexpected(
                fail("packed.requirements.invalid", "REQ0 ordinal is out of range"));
        if (*kind != 1U)
            return core::unexpected(
                fail("packed.profile.requirement",
                     "Foundation revision 1 registers the tap requirement kind only"));
        if (*interval > 1U)
            return core::unexpected(
                fail("packed.profile.interval",
                     "Foundation revision 1 registers the point interval only"));
        auto localValue = getString(*local);
        if (!localValue)
            return core::unexpected(std::move(localValue.error()));
        // Spec 7.4: (entity ordinal, localId bytes) is strictly ascending. Duplicated localIds on
        // one entity are therefore refused here rather than silently carried into the model.
        if (hasPrevious && (ordinal < previousOrdinal ||
                            (ordinal == previousOrdinal && !(previousLocalId < *localValue))))
            return core::unexpected(
                fail("packed.requirements.order",
                     "REQ0 rows must be unique and strictly ascending by (ordinal, localId)"));
        previousOrdinal = ordinal;
        previousLocalId = *localValue;
        hasPrevious = true;
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
        if (!domain || !action || !constraint || !effect)
            return core::unexpected(
                fail("packed.requirements.invalid", "REQ0 references are invalid"));
        // A nonzero effect set index must never be dropped silently, and the constraint index
        // must resolve inside CNS0 before the lane value can be read.
        if (*effect != 0U)
            return core::unexpected(
                fail("packed.profile.effects", "Foundation revision 1 requires empty effect sets"));
        if (*constraint == 0U || *constraint > lanes.size())
            return core::unexpected(
                fail("packed.profile.constraints",
                     "Requirement constraint index is outside the Foundation profile"));
        auto domainValue = refValue(*domain, 4);
        if (!domainValue)
            return core::unexpected(std::move(domainValue.error()));
        auto actionValue = refValue(*action, 5);
        if (!actionValue)
            return core::unexpected(std::move(actionValue.error()));
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
    // Spec 7.6: the profile gate runs on the rebuilt typed model, so a wire artifact and a typed
    // model are judged by exactly the same rules, and a profile rejection always precedes the
    // semantic identity comparison below.
    if (auto registered = profile_detail::validateFoundationProfile(chart); !registered)
        return core::unexpected(std::move(registered.error()));
    // Spec 9: the semantic identity is verified after structural, budget and semantic
    // validation and before any chart is published. A mismatch never yields a partial chart.
    auto recomputed = semanticIdentity(chart);
    if (!recomputed)
        return core::unexpected(std::move(recomputed.error()));
    if (std::memcmp(semanticHash->data(), recomputed->data(), recomputed->size()) != 0)
        return core::unexpected(
            fail("packed.identity.mismatch",
                 "Packed semantic identity does not match the recomputed digest"));
    return chart;
}

} // namespace cuexis::chart::packed
