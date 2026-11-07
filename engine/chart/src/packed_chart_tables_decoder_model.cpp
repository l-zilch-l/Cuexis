#include "packed_chart_tables_decoder_internal.hpp"

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
#include <vector>

namespace cuexis::chart::packed::decoder_detail {

auto Decoder::readArchetypes() -> core::Result<void> {
    const auto* archSection = section("ARCH");
    ByteReader archReader(archSection->data);
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
            auto mesh = p.readUnsignedLeb128(refCount);
            auto material = p.readUnsignedLeb128(refCount);
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
    return {};
}

auto Decoder::readEntities() -> core::Result<void> {
    parents.assign(entityCount, 0U);
    archetypes.assign(entityCount, 0U);
    const auto* entSection = section("ENT0");
    ByteReader ent(entSection->data);
    for (std::uint32_t i = 0; i < entityCount; ++i) {
        auto parent = ent.readUnsignedLeb128(static_cast<std::uint64_t>(entityCount) + 1U);
        auto arch = ent.readUnsignedLeb128(arches.size());
        if (!parent || !arch || *arch >= arches.size() || *parent > entityCount ||
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
    for (std::uint32_t start = 0; start < entityCount; ++start) {
        std::vector<std::uint8_t> visited(entityCount, 0);
        auto current = start;
        while (parents[current] != 0) {
            if (visited[current] != 0)
                return core::unexpected(
                    fail("packed.entities.parent_cycle", "ENT0 parent graph contains a cycle"));
            visited[current] = 1;
            current = parents[current] - 1U;
        }
    }
    return {};
}

auto Decoder::applyStreams(std::string_view code, int kind) -> core::Result<void> {
    const auto* stream = section(code);
    if (gameplay) {
        const auto bit = kind == 0 ? 1U : kind == 1 ? 2U : 16U;
        std::size_t expectedRows = 0;
        for (const auto arch : archetypes) {
            expectedRows += (arches[arch].mask & bit) != 0U ? 1U : 0U;
        }
        if ((stream && stream->records != expectedRows) || (!stream && expectedRows != 0U) ||
            (stream && expectedRows == 0U)) {
            return core::unexpected(
                fail("packed.stream.component", "Component stream presence or count is invalid"));
        }
    }
    if (!stream)
        return {};
    ByteReader rows(stream->data);
    std::uint32_t previous = 0;
    for (std::uint32_t row = 0; row < stream->records; ++row) {
        auto ordinal = rows.readUnsignedLeb128(entityCount);
        auto mask = rows.readUnsignedLeb128();
        if (!ordinal || !mask || *ordinal >= entityCount || (row != 0 && *ordinal <= previous))
            return core::unexpected(
                fail("packed.stream.ordinal", "Component stream ordinal is invalid"));
        // Spec 7.3: a difference row may only set the field atoms the stream actually
        // consumes; every other bit is undefined and must not be ignored.
        const auto allowedMask = kind == 0   ? std::uint64_t{0x79U}
                                 : kind == 1 ? std::uint64_t{0x07U}
                                             : std::uint64_t{0x0fU};
        if ((*mask & ~allowedMask) != 0U)
            return core::unexpected(fail("packed.stream.mask",
                                         "Component stream change mask uses undefined field bits"));
        previous = static_cast<std::uint32_t>(*ordinal);
        auto& components = entities[*ordinal].components;
        auto archIndex = archetypes[*ordinal];
        const auto& a = arches[archIndex];
        if (kind == 0 && !a.transform)
            return core::unexpected(
                fail("packed.stream.component", "Transform stream row has no Transform component"));
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
                std::remove_if(
                    components.begin(), components.end(),
                    [](const auto& c) { return std::holds_alternative<CanonicalTransform>(c); }),
                components.end());
            components.push_back(value);
        } else if (kind == 1) {
            auto value = *a.renderable;
            if (*mask & 1U) {
                auto ref = rows.readUnsignedLeb128(refCount);
                if (!ref)
                    return core::unexpected(std::move(ref.error()));
                auto text = refValue(*ref, 1);
                if (!text)
                    return core::unexpected(std::move(text.error()));
                value.mesh = AssetId{*text};
            }
            if (*mask & 2U) {
                auto ref = rows.readUnsignedLeb128(refCount);
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
                std::remove_if(
                    components.begin(), components.end(),
                    [](const auto& c) { return std::holds_alternative<CanonicalRenderable>(c); }),
                components.end());
            components.push_back(value);
        } else {
            auto value = *a.camera;
            // Spec 7.3: the Camera field indices are 0 type, 1 fovY, 2 near, 3 far, and each
            // set bit carries exactly that field atom.
            if (*mask & 1U) {
                auto type = rows.readU8();
                if (!type || *type != 1)
                    return core::unexpected(
                        fail("packed.stream.camera", "Camera type atom is invalid"));
            }
            if (*mask & 2U) {
                auto f = readF64(rows);
                if (!f)
                    return core::unexpected(
                        fail("packed.stream.camera", "Camera fovY atom is invalid"));
                value.fovY = *f;
            }
            if (*mask & 4U) {
                auto n = readF64(rows);
                if (!n)
                    return core::unexpected(
                        fail("packed.stream.camera", "Camera near atom is invalid"));
                value.nearPlane = *n;
            }
            if (*mask & 8U) {
                auto farValue = readF64(rows);
                if (!farValue)
                    return core::unexpected(
                        fail("packed.stream.camera", "Camera far atom is invalid"));
                value.farPlane = *farValue;
            }
            components.erase(std::remove_if(components.begin(), components.end(),
                                            [](const auto& c) {
                                                return std::holds_alternative<CanonicalCamera>(c);
                                            }),
                             components.end());
            components.push_back(value);
        }
    }
    if (!rows.empty())
        return core::unexpected(
            fail("packed.stream.trailing", "Component stream has trailing bytes"));
    return {};
}

auto Decoder::readConstraints() -> core::Result<void> {
    const auto* cnsSection = section("CNS0");
    ByteReader cns(cnsSection->data);
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
    return {};
}

auto Decoder::readRequirements() -> core::Result<void> {
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
    const auto legacyRequirementCount = gameplay ? 0U : requirementCount;
    for (std::uint32_t i = 0; i < legacyRequirementCount; ++i) {
        auto delta = req.readUnsignedLeb128(entityCount);
        auto local = req.readUnsignedLeb128(stringCount);
        auto kind = req.readU8();
        auto interval = req.readU8();
        auto start = readRationalBeatAtom(req);
        if (!delta || !local || !kind || !interval || !start)
            return core::unexpected(fail("packed.requirements.invalid", "REQ0 row is invalid"));
        if (i == 0) {
            if (entityCount != 0 && *delta >= entityCount)
                return core::unexpected(
                    fail("packed.requirements.invalid", "REQ0 first ordinal is invalid"));
            ordinal = static_cast<std::uint32_t>(*delta);
        } else {
            if (*delta > entityCount || ordinal > entityCount - *delta)
                return core::unexpected(
                    fail("packed.requirements.invalid", "REQ0 ordinal delta overflows"));
            ordinal += static_cast<std::uint32_t>(*delta);
        }
        if (ordinal >= entityCount)
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
        auto domain = req.readUnsignedLeb128(refCount);
        auto action = req.readUnsignedLeb128(refCount);
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
        requirement.judgementDomain = {"judgement-domain", *domainValue};
        requirement.requiredAction = {"action", *actionValue};
        requirement.constraints.push_back(LaneConstraint{lanes[*constraint - 1U]});
        entities[ordinal].requirements.push_back(std::move(requirement));
    }
    if (!req.empty())
        return core::unexpected(fail("packed.requirements.trailing", "REQ0 has trailing bytes"));
    for (std::uint32_t i = 0; i < entityCount; ++i) {
        if (parents[i] != 0)
            entities[i].parent = entities[parents[i] - 1U].identity;
        if (!entities[i].requirements.empty() && !(entities[i].componentMask() & (1U << 2U)))
            return core::unexpected(
                fail("packed.requirements.component", "Requirement presence bit is missing"));
    }
    return {};
}

auto Decoder::finish() -> core::Result<CanonicalSemanticChart> {
    chart.entities = std::move(entities);
    // R4/D9: the wire stores no explicit resource closure, so a decoded model is completed with
    // the closure derived from its asset references. The closure is a set of (asset, use) pairs,
    // so duplicates collapse and the result is in canonical ascending order.
    auto derivedClosure = std::set<CanonicalResourceUse>{};
    if (chart.mainMusic) {
        derivedClosure.insert(
            CanonicalResourceUse{*chart.mainMusic, CanonicalResourceUseKind::MainMusic});
    }
    for (const auto& entity : chart.entities) {
        for (const auto& component : entity.components) {
            if (const auto* renderable = std::get_if<CanonicalRenderable>(&component)) {
                derivedClosure.insert(CanonicalResourceUse{
                    renderable->mesh, CanonicalResourceUseKind::RenderableMesh});
                derivedClosure.insert(CanonicalResourceUse{
                    renderable->material, CanonicalResourceUseKind::RenderableMaterial});
            }
        }
    }
    chart.resourceClosure.resources.assign(derivedClosure.begin(), derivedClosure.end());
    if (gameplay) {
        std::vector<CanonicalEntityIdentity> owners;
        std::vector<std::uint64_t> masks;
        for (std::size_t i = 0; i < chart.entities.size(); ++i) {
            const auto mask = arches[archetypes[i]].mask;
            masks.push_back(mask);
            if ((mask & 4U) != 0U) {
                owners.push_back(chart.entities[i].identity);
            }
        }
        auto validated = gameplay_detail::staticPreimage(chart, owners);
        if (!validated) {
            return core::unexpected(std::move(validated.error()));
        }
        staticResult->strings = std::move(strings);
        staticResult->references = std::move(refs);
        staticResult->entityMasks = std::move(masks);
        return std::move(chart);
    }
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
    if (std::memcmp(semanticHash.data(), recomputed->data(), recomputed->size()) != 0)
        return core::unexpected(
            fail("packed.identity.mismatch",
                 "Packed semantic identity does not match the recomputed digest"));
    return std::move(chart);
}

} // namespace cuexis::chart::packed::decoder_detail
