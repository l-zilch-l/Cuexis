#include <cuexis/chart/packed_chart_tables.hpp>

#include <cuexis/chart/uuid.hpp>
#include <cuexis/core/error.hpp>

#include "packed_chart_tables_units_internal.hpp"
#include "packed_gameplay_internal.hpp"
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

auto writeTimeSection(const ChartTiming& timing) -> core::Result<Section> {
    const auto tempoCount = narrowU32(timing.tempoEvents.size());
    if (!tempoCount)
        return core::unexpected(std::move(tempoCount.error()));
    const auto stopCount = narrowU32(timing.stops.size());
    if (!stopCount)
        return core::unexpected(std::move(stopCount.error()));
    // Spec 6.4: tempo events and stops are written in ascending Beat order. The semantic preimage
    // already refuses shared beats, so the canonical order is unambiguous and independent of the
    // model order (A12).
    auto tempos = std::vector<const TempoEvent*>{};
    tempos.reserve(timing.tempoEvents.size());
    for (const auto& tempo : timing.tempoEvents)
        tempos.push_back(&tempo);
    std::sort(tempos.begin(), tempos.end(), [](const auto* left, const auto* right) {
        return left->startBeat < right->startBeat;
    });
    auto stops = std::vector<const TimingStop*>{};
    stops.reserve(timing.stops.size());
    for (const auto& stop : timing.stops)
        stops.push_back(&stop);
    std::sort(stops.begin(), stops.end(),
              [](const auto* left, const auto* right) { return left->beat < right->beat; });
    ByteWriter writer;
    writeF64(writer, timing.offsetMs);
    writeF64(writer, timing.defaultBpm);
    writer.writeU32(*tempoCount);
    writer.writeU32(*stopCount);
    writer.writeU8(0);
    writer.writeU8(0);
    writer.writeU8(0);
    for (const auto* tempo : tempos) {
        (void)writeRationalBeatAtom(writer, tempo->startBeat);
        (void)writeRationalBeatAtom(writer, tempo->durationBeats);
        writeF64(writer, tempo->startBpm);
        writeF64(writer, tempo->endBpm);
        writeF64(writer, tempo->startSlope);
        writeF64(writer, tempo->endSlope);
    }
    for (const auto* stop : stops) {
        (void)writeRationalBeatAtom(writer, stop->beat);
        writeF64(writer, stop->durationMs);
    }
    return Section{{'T', 'I', 'M', 'E'}, std::move(writer).takeBytes(), 1};
}

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
    std::sort(keyed.begin(), keyed.end(), [](const auto& left, const auto& right) {
        return identity_detail::ByteKeyLess{}(left.first, right.first);
    });
    order.entities.clear();
    for (std::size_t i = 0; i < keyed.size(); ++i) {
        order.entities.push_back(keyed[i].second);
        order.masks.emplace(keyed[i].second, keyed[i].second->componentMask());
        if (!order.ordinals.emplace(keyed[i].first, static_cast<std::uint32_t>(i)).second) {
            return core::unexpected(fail("packed.identity.duplicate_identity",
                                         "Entity identities must be unique in a candidate chart"));
        }
    }
    return order;
}

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

// Spec 7.1 tie-break material: the canonical field bytes of one complete component value, in
// field-index order and without dictionary indices, so the default selection cannot depend on
// the model order or on the physical dictionary.
[[nodiscard]] auto defaultKey(const CanonicalTransform& value) -> std::vector<std::byte> {
    ByteWriter writer;
    writeTransform(writer, value);
    return std::move(writer).takeBytes();
}

[[nodiscard]] auto defaultKey(const CanonicalRenderable& value) -> std::vector<std::byte> {
    ByteWriter writer;
    for (const auto* text : {&value.mesh.value, &value.material.value}) {
        const auto length = narrowU32(text->size());
        writer.writeUnsignedLeb128(length ? *length : 0U);
        writer.writeBytes(std::as_bytes(std::span{text->data(), text->size()}));
    }
    writer.writeU8(value.alpha);
    return std::move(writer).takeBytes();
}

[[nodiscard]] auto defaultKey(const CanonicalCamera& value) -> std::vector<std::byte> {
    ByteWriter writer;
    const auto length = narrowU32(value.type.size());
    writer.writeUnsignedLeb128(length ? *length : 0U);
    writer.writeBytes(std::as_bytes(std::span{value.type.data(), value.type.size()}));
    writeF64(writer, value.fovY);
    writeF64(writer, value.nearPlane);
    writeF64(writer, value.farPlane);
    return std::move(writer).takeBytes();
}

// Spec 7.1: one Archetype per distinct component mask, and the default of each defaultable
// component is the most frequent complete value; ties are broken by ascending canonical field
// bytes, i.e. by the first key in the ordered map. A last-value-wins rule would make the
// canonical bytes depend on the model order (A11).
template <typename T> struct DefaultPicker final {
    std::map<std::vector<std::byte>, std::pair<T, std::size_t>, identity_detail::ByteKeyLess>
        counts;
    std::optional<T> value;

    void add(const T& candidate) {
        auto key = defaultKey(candidate);
        auto [it, inserted] =
            counts.try_emplace(std::move(key), std::pair<T, std::size_t>{candidate, 0U});
        ++it->second.second;
    }

    void finish() {
        std::size_t best = 0U;
        for (const auto& [key, entry] : counts) {
            if (!value || entry.second > best) {
                value = entry.first;
                best = entry.second;
            }
        }
    }
};

auto makeArchetypes(const EntityOrder& order, std::map<std::uint64_t, std::uint32_t>& indices)
    -> std::vector<Archetype> {
    struct Group final {
        std::uint64_t mask{};
        DefaultPicker<CanonicalTransform> transform;
        DefaultPicker<CanonicalRenderable> renderable;
        DefaultPicker<CanonicalCamera> camera;
    };
    std::map<std::uint64_t, Group> byMask;
    for (const auto* entity : order.entities) {
        auto& group = byMask[order.masks.at(entity)];
        group.mask = order.masks.at(entity);
        for (const auto& component : entity->components) {
            if (const auto* v = std::get_if<CanonicalTransform>(&component))
                group.transform.add(*v);
            if (const auto* v = std::get_if<CanonicalRenderable>(&component))
                group.renderable.add(*v);
            if (const auto* v = std::get_if<CanonicalCamera>(&component))
                group.camera.add(*v);
        }
    }
    std::vector<Archetype> result;
    for (auto& [mask, group] : byMask) {
        group.transform.finish();
        group.renderable.finish();
        group.camera.finish();
        Archetype arch{.mask = mask,
                       .transform = group.transform.value,
                       .renderable = group.renderable.value,
                       .camera = group.camera.value};
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
        writer.writeUnsignedLeb128(arches.at(order.masks.at(entity)));
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
        const auto mask = order.masks.at(entity);
        if ((kind == 0 && !(mask & 1U)) || (kind == 1 && !(mask & 2U)) ||
            (kind == 2 && !(mask & 16U)))
            continue;
        const auto& arch = arches[archIndices.at(mask)];
        std::uint32_t changed = 0;
        const CanonicalTransform* transform = nullptr;
        const CanonicalRenderable* renderable = nullptr;
        const CanonicalCamera* camera = nullptr;
        // R4/A18: each lookup must keep its own pointer. Testing only the requested kind per
        // iteration would reset the pointer for every other component, so a delta was silently
        // dropped whenever the component was not the entity's last one.
        for (const auto& component : entity->components) {
            if (const auto* value = std::get_if<CanonicalTransform>(&component))
                transform = value;
            if (const auto* value = std::get_if<CanonicalRenderable>(&component))
                renderable = value;
            if (const auto* value = std::get_if<CanonicalCamera>(&component))
                camera = value;
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
        // Spec 7.3: the change mask uses the Camera field indices 0 type, 1 fovY, 2 near, 3 far.
        // Comparing only a subset of the fields would silently drop the others back to the
        // archetype default (A10).
        if (kind == 2 && camera) {
            if (!arch.camera || camera->type != arch.camera->type)
                changed |= 1U;
            if (!arch.camera || camera->fovY != arch.camera->fovY)
                changed |= 2U;
            if (!arch.camera || camera->nearPlane != arch.camera->nearPlane)
                changed |= 4U;
            if (!arch.camera || camera->farPlane != arch.camera->farPlane)
                changed |= 8U;
        }
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
        if (kind == 2 && camera) {
            if (changed & 1U)
                writer.writeU8(1); // registered perspective type atom
            if (changed & 2U)
                writeF64(writer, camera->fovY);
            if (changed & 4U)
                writeF64(writer, camera->nearPlane);
            if (changed & 8U)
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

auto gameplay_detail::encodeStatic(const CanonicalSemanticChart& chart,
                                   std::span<const CanonicalEntityIdentity> gameplayOwners,
                                   std::span<const std::string> extraStrings,
                                   std::span<const Reference> extraReferences,
                                   PackedChartLimits limits) -> core::Result<StaticTables> {
    const auto budget = limits_detail::effectiveLimits(limits);
    if (chart.entities.size() > budget.maxPackedEntities) {
        return core::unexpected(fail("packed.budget.entities", "Packed entity budget exceeded"));
    }
    auto prefix = staticPreimage(chart, gameplayOwners);
    if (!prefix) {
        return core::unexpected(std::move(prefix.error()));
    }
    auto ordered = orderEntities(chart);
    if (!ordered) {
        return core::unexpected(std::move(ordered.error()));
    }
    for (const auto& owner : gameplayOwners) {
        auto key = identityKey(owner);
        if (!key) {
            return core::unexpected(std::move(key.error()));
        }
        const auto* entity = ordered->entities[ordered->ordinals.at(*key)];
        ordered->masks.at(entity) |= 4U;
    }
    Dictionaries dict;
    collectStrings(dict, chart);
    dict.strings.insert(dict.strings.end(), extraStrings.begin(), extraStrings.end());
    for (const auto& ref : extraReferences) {
        if (ref.first == 0U || ref.first > 13U) {
            return core::unexpected(fail("packed.references.kind",
                                         "Reference kind is outside the revision-2 registry"));
        }
        dict.strings.push_back(ref.second);
        dict.refIndex.emplace(ref, 0U);
    }
    std::sort(dict.strings.begin(), dict.strings.end());
    dict.strings.erase(std::unique(dict.strings.begin(), dict.strings.end()), dict.strings.end());
    if (dict.strings.size() > budget.maxPackedStrings) {
        return core::unexpected(fail("packed.budget.strings", "Packed string budget exceeded"));
    }
    if (dict.refIndex.size() > budget.maxPackedReferences) {
        return core::unexpected(
            fail("packed.budget.references", "Packed reference budget exceeded"));
    }
    std::uint64_t stringBytes = 12U + std::uint64_t{4} * dict.strings.size();
    for (const auto& text : dict.strings) {
        if (!validUtf8(text)) {
            return core::unexpected(fail("packed.strings.bounds", "STR0 text is not valid UTF-8"));
        }
        if (text.size() > budget.maxPackedSectionBytes ||
            stringBytes > budget.maxPackedSectionBytes - text.size()) {
            return core::unexpected(
                fail("packed.budget.section_bytes", "STR0 section budget exceeded"));
        }
        stringBytes += text.size();
    }
    if (stringBytes > budget.maxPackedSectionBytes) {
        return core::unexpected(
            fail("packed.budget.section_bytes", "STR0 section budget exceeded"));
    }
    dict.stringIndex.clear();
    for (std::size_t i = 0; i < dict.strings.size(); ++i) {
        dict.stringIndex.emplace(dict.strings[i], static_cast<std::uint32_t>(i));
    }
    StaticTables result;
    result.strings = dict.strings;
    for (auto& [key, index] : dict.refIndex) {
        index = static_cast<std::uint32_t>(result.references.size());
        result.references.push_back(key);
    }
    const auto add = [&](core::Result<Section> section) -> core::Result<void> {
        if (!section) {
            return core::unexpected(std::move(section.error()));
        }
        if (section->bytes.size() > budget.maxPackedSectionBytes) {
            return core::unexpected(
                fail("packed.budget.section_bytes", "Packed static section budget exceeded"));
        }
        result.sections.push_back(std::move(*section));
        return {};
    };
    std::map<std::uint64_t, std::uint32_t> archIndices;
    auto arches = makeArchetypes(*ordered, archIndices);
    // All static records use the same final shared dictionaries.
    for (auto section :
         {writeStringSection(dict), writeReferenceSection(dict), writeMetaSection(chart, dict),
          writeTimeSection(chart.timing), writeIdentitySection(*ordered, chart, dict),
          writeArchetypeSection(arches, dict), writeEntitySection(*ordered, archIndices)}) {
        if (auto added = add(std::move(section)); !added) {
            return core::unexpected(std::move(added.error()));
        }
    }
    for (int kind = 0; kind < 3; ++kind) {
        auto stream = writeComponentStream(*ordered, arches, archIndices, dict, kind);
        if (!stream) {
            return core::unexpected(std::move(stream.error()));
        }
        if (stream->records != 0U) {
            if (auto added = add(std::move(stream)); !added) {
                return core::unexpected(std::move(added.error()));
            }
        }
    }
    result.sections.push_back(Section{{'C', 'N', 'S', '0'}, {}, 0U});
    result.sections.push_back(Section{{'R', 'E', 'Q', '0'}, {std::byte{0}, std::byte{0}}, 0U});
    std::uint64_t decodedBytes = 0;
    for (const auto& section : result.sections) {
        decodedBytes += section.bytes.size();
    }
    if (decodedBytes > budget.maxPackedDecodedBytes) {
        return core::unexpected(
            fail("packed.budget.decoded_bytes", "Packed static decoded byte budget exceeded"));
    }
    if (96U + result.sections.size() * 32U + decodedBytes > budget.maxPackedFileBytes) {
        return core::unexpected(
            fail("packed.budget.file_bytes", "Packed static envelope budget exceeded"));
    }
    for (const auto* entity : ordered->entities) {
        result.entityIdentities.push_back(entity->identity);
        result.entityMasks.push_back(ordered->masks.at(entity));
    }
    std::sort(result.sections.begin(), result.sections.end(),
              [](const auto& left, const auto& right) { return left.code < right.code; });
    return result;
}

} // namespace cuexis::chart::packed
