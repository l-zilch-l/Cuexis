// Foundation semantic identity: the typed preimage of docs/formats/PACKED_CHART_FORMAT.md 10.1.
//
// The preimage covers the canonical semantic model only. It never reads the Packed dictionary
// indices, the section layout or the file bytes, and it never reuses artifact bytes.

#include <cuexis/chart/packed_chart_tables.hpp>

#include "packed_identity_internal.hpp"
#include "sha256_internal.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::chart::packed {
namespace {

using identity_detail::canonicalIdentityBytes;
using identity_detail::uuidBytes;

constexpr std::string_view semanticDomain{"cuexis.chart.semantic.v5.candidate.1"};

auto fail(std::string code, std::string message) -> core::Error {
    return core::Error{std::move(code), std::move(message)};
}

// Fixed-width little-endian preimage buffer. Spec 10.1 deliberately uses no varint encoding.
struct Preimage final {
    std::vector<std::byte> bytes;
};

void append(Preimage& out, std::span<const std::byte> data) {
    out.bytes.insert(out.bytes.end(), data.begin(), data.end());
}

void writeU8(Preimage& out, std::uint8_t value) {
    out.bytes.push_back(static_cast<std::byte>(value));
}

void writeUnsigned(Preimage& out, std::uint64_t value, std::size_t width) {
    for (std::size_t index = 0; index < width; ++index) {
        out.bytes.push_back(static_cast<std::byte>((value >> (index * 8U)) & 0xffU));
    }
}

void writeU16(Preimage& out, std::uint16_t value) {
    writeUnsigned(out, value, 2U);
}
void writeU32(Preimage& out, std::uint32_t value) {
    writeUnsigned(out, value, 4U);
}
void writeU64(Preimage& out, std::uint64_t value) {
    writeUnsigned(out, value, 8U);
}

// S(text): u32 UTF-8 byte length + exact bytes.
void writeText(Preimage& out, std::string_view text) {
    writeU32(out, static_cast<std::uint32_t>(text.size()));
    append(out, std::as_bytes(std::span{text.data(), text.size()}));
}

// R(reference): u8 kind + S(id). Kind values are the REF0 registry of Spec 6.2.
void writeReference(Preimage& out, std::uint8_t kind, std::string_view id) {
    writeU8(out, kind);
    writeText(out, id);
}

// B(beat): i64 reduced numerator + u64 positive denominator.
auto writeBeat(Preimage& out, const RationalBeat& beat) -> core::Result<void> {
    if (beat.denominator() <= 0) {
        return core::unexpected(
            fail("packed.identity.beat", "Beat denominator must be a positive integer"));
    }
    writeU64(out, static_cast<std::uint64_t>(beat.numerator()));
    writeU64(out, static_cast<std::uint64_t>(beat.denominator()));
    return {};
}

// I(identity): u32 byte length + canonical identity bytes from Spec 6.5.
auto writeIdentity(Preimage& out, const CanonicalEntityIdentity& identity) -> core::Result<void> {
    auto bytes = canonicalIdentityBytes(identity);
    if (!bytes) {
        return core::unexpected(std::move(bytes.error()));
    }
    writeU32(out, static_cast<std::uint32_t>(bytes->size()));
    append(out, *bytes);
    return {};
}

// Spec 4.1: all floats must be finite and -0 is canonicalized to +0.
auto canonicalFloat(float value) noexcept -> float {
    return value == 0.0F ? 0.0F : value;
}
auto canonicalFloat(double value) noexcept -> double {
    return value == 0.0 ? 0.0 : value;
}

auto writeF32(Preimage& out, float value) -> core::Result<void> {
    if (!std::isfinite(value)) {
        return core::unexpected(
            fail("packed.identity.non_finite", "Semantic identity rejects non-finite floats"));
    }
    writeU32(out, std::bit_cast<std::uint32_t>(canonicalFloat(value)));
    return {};
}

auto writeF64(Preimage& out, double value) -> core::Result<void> {
    if (!std::isfinite(value)) {
        return core::unexpected(
            fail("packed.identity.non_finite", "Semantic identity rejects non-finite floats"));
    }
    writeU64(out, std::bit_cast<std::uint64_t>(canonicalFloat(value)));
    return {};
}

auto writeCameraType(Preimage& out, std::string_view type) -> core::Result<void> {
    if (type != "perspective") {
        return core::unexpected(
            fail("packed.identity.camera_type", "Only the perspective camera is registered"));
    }
    writeU8(out, 1);
    return {};
}

auto writeTransform(Preimage& out, const CanonicalTransform& value) -> core::Result<void> {
    for (const auto component :
         {value.position.x, value.position.y, value.position.z, value.rotation.x, value.rotation.y,
          value.rotation.z, value.rotation.w, value.scale.x, value.scale.y, value.scale.z}) {
        if (auto result = writeF32(out, component); !result) {
            return result;
        }
    }
    return {};
}

auto writeCamera(Preimage& out, const CanonicalCamera& value) -> core::Result<void> {
    if (auto result = writeCameraType(out, value.type); !result) {
        return result;
    }
    for (const auto component : {value.fovY, value.nearPlane, value.farPlane}) {
        if (auto result = writeF64(out, component); !result) {
            return result;
        }
    }
    return {};
}

void writeRenderable(Preimage& out, const CanonicalRenderable& value) {
    writeReference(out, 1, value.mesh.value);
    writeReference(out, 1, value.material.value);
    writeU8(out, value.alpha);
}

auto writeRequirement(Preimage& out, const CanonicalRequirement& requirement)
    -> core::Result<void> {
    // The wire stores exactly one constraint set and an empty effect set, so these three branches
    // are the preimage half of the Spec 7.6 profile rules. They reuse the same packed.profile.*
    // diagnostics as packed::encode/packed::decode so one rule has exactly one code.
    writeText(out, requirement.localId);
    writeU8(out, static_cast<std::uint8_t>(requirement.kind));
    writeU8(out, static_cast<std::uint8_t>(requirement.interval.kind));
    if (auto result = writeBeat(out, requirement.interval.startBeat); !result) {
        return result;
    }
    if (requirement.interval.kind == CanonicalIntervalKind::HalfOpenRange) {
        if (!requirement.interval.endBeat) {
            return core::unexpected(
                fail("packed.profile.interval", "Range requirements require an end beat"));
        }
        if (auto result = writeBeat(out, *requirement.interval.endBeat); !result) {
            return result;
        }
    } else if (requirement.interval.endBeat) {
        // The wire carries an end atom only for ranges, so accepting it here would silently
        // drop semantic data from the preimage.
        return core::unexpected(
            fail("packed.profile.interval", "Point requirements must not carry an end beat"));
    }
    writeReference(out, 4, requirement.judgementDomain.id);
    writeReference(out, 5, requirement.requiredAction.id);
    // Foundation revision 1 stores exactly one lane constraint set. The wire has room for one,
    // so hashing more would promise data the artifact cannot carry.
    if (requirement.constraints.size() != 1U) {
        return core::unexpected(fail("packed.profile.constraints",
                                     "Foundation requirements carry exactly one constraint"));
    }
    writeU32(out, 1);
    const auto& constraint = std::get<LaneConstraint>(requirement.constraints.front());
    writeU8(out, 1);
    writeU32(out, constraint.lane);
    if (!requirement.effects.empty()) {
        return core::unexpected(
            fail("packed.profile.effects", "Foundation revision 1 requires empty effect sets"));
    }
    writeU32(out, 0);
    return {};
}

auto writeEntity(Preimage& out, const CanonicalEntity& entity,
                 const std::set<std::vector<std::byte>>& identities) -> core::Result<void> {
    // The wire stores at most one component per kind, so hashing more would describe an
    // artifact the format cannot carry.
    std::size_t transforms = 0;
    std::size_t renderables = 0;
    std::size_t cameras = 0;
    for (const auto& component : entity.components) {
        if (std::holds_alternative<CanonicalTransform>(component)) {
            ++transforms;
        } else if (std::holds_alternative<CanonicalRenderable>(component)) {
            ++renderables;
        } else {
            ++cameras;
        }
    }
    if (transforms > 1U || renderables > 1U || cameras > 1U) {
        return core::unexpected(fail("packed.identity.component_duplicate",
                                     "An entity must not carry two components of the same kind"));
    }
    if (auto result = writeIdentity(out, entity.identity); !result) {
        return result;
    }
    if (entity.parent) {
        auto parentBytes = canonicalIdentityBytes(*entity.parent);
        if (!parentBytes) {
            return core::unexpected(std::move(parentBytes.error()));
        }
        if (!identities.contains(*parentBytes)) {
            return core::unexpected(
                fail("packed.identity.parent_missing", "Entity parent is not part of the chart"));
        }
        writeU32(out, static_cast<std::uint32_t>(parentBytes->size()));
        append(out, *parentBytes);
    } else {
        writeU32(out, 0);
    }
    writeU64(out, entity.componentMask());
    // Spec 10.1 writes complete components in ascending bit order: 0, 1, 4. Bits 2 and 5 carry
    // no inline payload.
    for (const auto& component : entity.components) {
        if (const auto* transform = std::get_if<CanonicalTransform>(&component)) {
            if (auto result = writeTransform(out, *transform); !result) {
                return result;
            }
        }
    }
    for (const auto& component : entity.components) {
        if (const auto* renderable = std::get_if<CanonicalRenderable>(&component)) {
            writeRenderable(out, *renderable);
        }
    }
    for (const auto& component : entity.components) {
        if (const auto* camera = std::get_if<CanonicalCamera>(&component)) {
            if (auto result = writeCamera(out, *camera); !result) {
                return result;
            }
        }
    }
    auto requirements = std::vector<const CanonicalRequirement*>{};
    requirements.reserve(entity.requirements.size());
    for (const auto& requirement : entity.requirements) {
        requirements.push_back(&requirement);
    }
    std::sort(requirements.begin(), requirements.end(),
              [](const auto* left, const auto* right) { return left->localId < right->localId; });
    for (std::size_t index = 1; index < requirements.size(); ++index) {
        if (requirements[index - 1U]->localId == requirements[index]->localId) {
            return core::unexpected(fail("packed.identity.requirement_duplicate",
                                         "Requirement localId must be unique per entity"));
        }
    }
    writeU32(out, static_cast<std::uint32_t>(requirements.size()));
    for (const auto* requirement : requirements) {
        if (auto result = writeRequirement(out, *requirement); !result) {
            return result;
        }
    }
    return {};
}

auto collectAssets(const CanonicalSemanticChart& chart) -> std::set<std::string> {
    std::set<std::string> assets;
    if (chart.mainMusic) {
        assets.insert(chart.mainMusic->value);
    }
    for (const auto& entity : chart.entities) {
        for (const auto& component : entity.components) {
            if (const auto* renderable = std::get_if<CanonicalRenderable>(&component)) {
                assets.insert(renderable->mesh.value);
                assets.insert(renderable->material.value);
            }
        }
    }
    return assets;
}

} // namespace

auto semanticPreimage(const CanonicalSemanticChart& chart) -> core::Result<std::vector<std::byte>> {
    Preimage out;
    out.bytes.reserve(256U + chart.entities.size() * 128U);

    append(out, std::as_bytes(std::span{semanticDomain.data(), semanticDomain.size()}));
    const std::array<std::byte, 1> terminator{std::byte{0}};
    append(out, terminator);
    writeU16(out, 5);

    auto chartId = uuidBytes(chart.chartId.value);
    if (!chartId) {
        return core::unexpected(std::move(chartId.error()));
    }
    append(out, std::as_bytes(std::span{*chartId}));

    if (chart.mainMusic) {
        writeU8(out, 1);
        writeReference(out, 1, chart.mainMusic->value);
    } else {
        writeU8(out, 0);
    }

    if (auto result = writeCameraType(out, chart.defaultCamera.type); !result) {
        return core::unexpected(std::move(result.error()));
    }
    for (const auto component :
         {chart.defaultCamera.fovY, chart.defaultCamera.nearPlane, chart.defaultCamera.farPlane,
          chart.defaultCamera.pitch, chart.defaultCamera.yaw, chart.defaultCamera.roll}) {
        if (auto result = writeF64(out, component); !result) {
            return core::unexpected(std::move(result.error()));
        }
    }
    const auto position = chart.defaultCamera.defaultTransform
                              ? chart.defaultCamera.defaultTransform->position
                              : core::Vec3{};
    for (const auto component : {position.x, position.y, position.z}) {
        if (auto result = writeF32(out, component); !result) {
            return core::unexpected(std::move(result.error()));
        }
    }

    auto features = std::vector<const CanonicalFeature*>{};
    features.reserve(chart.features.size());
    for (const auto& feature : chart.features) {
        features.push_back(&feature);
    }
    std::sort(features.begin(), features.end(),
              [](const auto* left, const auto* right) { return left->id < right->id; });
    for (std::size_t index = 1; index < features.size(); ++index) {
        if (features[index - 1U]->id == features[index]->id) {
            return core::unexpected(fail("packed.identity.feature_duplicate",
                                         "Feature ids must be unique in a candidate chart"));
        }
    }
    writeU32(out, static_cast<std::uint32_t>(features.size()));
    for (const auto* feature : features) {
        writeReference(out, 7, feature->id);
        writeU32(out, feature->version);
    }

    if (auto result = writeF64(out, chart.timing.offsetMs); !result) {
        return core::unexpected(std::move(result.error()));
    }
    if (auto result = writeF64(out, chart.timing.defaultBpm); !result) {
        return core::unexpected(std::move(result.error()));
    }
    auto tempos = std::vector<const TempoEvent*>{};
    tempos.reserve(chart.timing.tempoEvents.size());
    for (const auto& tempo : chart.timing.tempoEvents) {
        tempos.push_back(&tempo);
    }
    std::sort(tempos.begin(), tempos.end(), [](const auto* left, const auto* right) {
        return left->startBeat < right->startBeat;
    });
    for (std::size_t index = 1; index < tempos.size(); ++index) {
        if (tempos[index - 1U]->startBeat == tempos[index]->startBeat) {
            return core::unexpected(
                fail("packed.identity.timing_order", "Tempo events must not share a start beat"));
        }
    }
    auto stops = std::vector<const TimingStop*>{};
    stops.reserve(chart.timing.stops.size());
    for (const auto& stop : chart.timing.stops) {
        stops.push_back(&stop);
    }
    std::sort(stops.begin(), stops.end(),
              [](const auto* left, const auto* right) { return left->beat < right->beat; });
    for (std::size_t index = 1; index < stops.size(); ++index) {
        if (stops[index - 1U]->beat == stops[index]->beat) {
            return core::unexpected(
                fail("packed.identity.timing_order", "Timing stops must not share a beat"));
        }
    }
    writeU32(out, static_cast<std::uint32_t>(tempos.size()));
    writeU32(out, static_cast<std::uint32_t>(stops.size()));
    for (const auto* tempo : tempos) {
        if (auto result = writeBeat(out, tempo->startBeat); !result) {
            return core::unexpected(std::move(result.error()));
        }
        if (auto result = writeBeat(out, tempo->durationBeats); !result) {
            return core::unexpected(std::move(result.error()));
        }
        for (const auto component :
             {tempo->startBpm, tempo->endBpm, tempo->startSlope, tempo->endSlope}) {
            if (auto result = writeF64(out, component); !result) {
                return core::unexpected(std::move(result.error()));
            }
        }
    }
    for (const auto* stop : stops) {
        if (auto result = writeBeat(out, stop->beat); !result) {
            return core::unexpected(std::move(result.error()));
        }
        if (auto result = writeF64(out, stop->durationMs); !result) {
            return core::unexpected(std::move(result.error()));
        }
    }

    // The canonical resource-requirement closure is derived from the chart's asset references.
    // CanonicalResourceClosure itself is not part of the Packed wire format.
    const auto assets = collectAssets(chart);
    writeU32(out, static_cast<std::uint32_t>(assets.size()));
    for (const auto& asset : assets) {
        writeReference(out, 1, asset);
    }

    // Spec 6.5: canonical identity bytes define the entity order. Duplicates are rejected rather
    // than silently collapsed by the sort.
    auto entities = std::vector<std::pair<std::vector<std::byte>, const CanonicalEntity*>>{};
    entities.reserve(chart.entities.size());
    auto identities = std::set<std::vector<std::byte>>{};
    for (const auto& entity : chart.entities) {
        if (const auto* generated = std::get_if<GeneratedEntityIdentity>(&entity.identity)) {
            if (generated->chartId.value != chart.chartId.value) {
                return core::unexpected(fail("packed.identity.scope_chart",
                                             "Generated identity scope must use the META chartId"));
            }
            if (generated->path.empty() || generated->path.back().iterationIndexPlusOne != 0U) {
                return core::unexpected(
                    fail("packed.identity.generated_path",
                         "Generated identity paths must end with a non-indexed step"));
            }
        }
        auto bytes = canonicalIdentityBytes(entity.identity);
        if (!bytes) {
            return core::unexpected(std::move(bytes.error()));
        }
        if (!identities.insert(*bytes).second) {
            return core::unexpected(fail("packed.identity.duplicate_identity",
                                         "Entity identities must be unique in a candidate chart"));
        }
        entities.emplace_back(std::move(*bytes), &entity);
    }
    std::sort(entities.begin(), entities.end(),
              [](const auto& left, const auto& right) { return left.first < right.first; });

    // The wire stores parents as entity ordinals and rejects self-parents and cycles, so the
    // writer must refuse those graphs before publishing rather than emit an undecodable file.
    auto parentOf = std::map<std::vector<std::byte>, const CanonicalEntity*>{};
    for (const auto& [bytes, entity] : entities) {
        parentOf.emplace(bytes, entity);
    }
    auto visited = std::map<std::vector<std::byte>, std::size_t>{};
    std::size_t generation = 0;
    for (const auto& [bytes, entity] : entities) {
        ++generation;
        visited[bytes] = generation;
        const CanonicalEntity* current = entity;
        while (current->parent) {
            auto parentBytes = canonicalIdentityBytes(*current->parent);
            if (!parentBytes) {
                return core::unexpected(std::move(parentBytes.error()));
            }
            if (visited[*parentBytes] == generation) {
                return core::unexpected(
                    fail("packed.identity.parent_cycle", "Entity parent graph contains a cycle"));
            }
            visited[*parentBytes] = generation;
            const auto found = parentOf.find(*parentBytes);
            if (found == parentOf.end()) {
                return core::unexpected(fail("packed.identity.parent_missing",
                                             "Entity parent is not part of the chart"));
            }
            current = found->second;
        }
    }

    writeU32(out, static_cast<std::uint32_t>(entities.size()));
    for (const auto& [bytes, entity] : entities) {
        if (auto result = writeEntity(out, *entity, identities); !result) {
            return core::unexpected(std::move(result.error()));
        }
    }

    // Foundation revision 1 has no Behavior, Animation or Effect definitions.
    writeU32(out, 0);
    writeU32(out, 0);
    writeU32(out, 0);

    return std::move(out.bytes);
}

auto semanticIdentity(const CanonicalSemanticChart& chart) -> core::Result<PackedSemanticIdentity> {
    auto preimage = semanticPreimage(chart);
    if (!preimage) {
        return core::unexpected(std::move(preimage.error()));
    }
    return PackedSemanticIdentity{cuexis::chart::detail::sha256(*preimage)};
}

auto semanticIdentityHex(const PackedSemanticIdentity& identity) -> std::string {
    return cuexis::chart::detail::sha256Hex(identity);
}

namespace identity_detail {
namespace {

auto hexDigit(char character) -> int {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

void appendU32(std::vector<std::byte>& bytes, std::size_t value) {
    const auto narrowed = static_cast<std::uint32_t>(value);
    for (std::size_t index = 0; index < 4U; ++index) {
        bytes.push_back(static_cast<std::byte>((narrowed >> (index * 8U)) & 0xffU));
    }
}

void appendText(std::vector<std::byte>& bytes, std::string_view text) {
    appendU32(bytes, text.size());
    for (const auto character : text) {
        bytes.push_back(static_cast<std::byte>(character));
    }
}

} // namespace

auto uuidBytes(std::string_view text) -> core::Result<std::array<std::uint8_t, 16>> {
    const auto invalid = [&text]() {
        return core::unexpected(fail("packed.identity.invalid_uuid",
                                     "Identity is not canonical UUID text: " + std::string{text}));
    };
    if (text.size() != 36U || text[8] != '-' || text[13] != '-' || text[18] != '-' ||
        text[23] != '-') {
        return invalid();
    }
    std::array<std::uint8_t, 16> bytes{};
    std::size_t written = 0;
    for (std::size_t index = 0; index < text.size();) {
        if (text[index] == '-') {
            ++index;
            continue;
        }
        if (written >= bytes.size() || index + 1U >= text.size()) {
            return invalid();
        }
        const auto high = hexDigit(text[index]);
        const auto low = hexDigit(text[index + 1U]);
        if (high < 0 || low < 0) {
            return invalid();
        }
        bytes[written++] = static_cast<std::uint8_t>((high << 4) | low);
        index += 2U;
    }
    if (written != bytes.size()) {
        return invalid();
    }
    return bytes;
}

auto canonicalIdentityBytes(const CanonicalEntityIdentity& identity)
    -> core::Result<std::vector<std::byte>> {
    std::vector<std::byte> bytes;
    if (const auto* explicitIdentity = std::get_if<ExplicitEntityIdentity>(&identity)) {
        bytes.push_back(std::byte{0});
        auto uuid = uuidBytes(explicitIdentity->objectId.value);
        if (!uuid) {
            return core::unexpected(std::move(uuid.error()));
        }
        for (const auto value : *uuid) {
            bytes.push_back(static_cast<std::byte>(value));
        }
        return bytes;
    }
    const auto& generated = std::get<GeneratedEntityIdentity>(identity);
    bytes.push_back(std::byte{1});
    auto chartId = uuidBytes(generated.chartId.value);
    if (!chartId) {
        return core::unexpected(std::move(chartId.error()));
    }
    for (const auto value : *chartId) {
        bytes.push_back(static_cast<std::byte>(value));
    }
    appendText(bytes, generated.bindingId);
    appendText(bytes, generated.moduleId);
    appendText(bytes, generated.exportId);
    appendU32(bytes, generated.path.size());
    for (const auto& step : generated.path) {
        appendText(bytes, step.nodeId);
        appendU32(bytes, step.iterationIndexPlusOne);
    }
    return bytes;
}

} // namespace identity_detail

} // namespace cuexis::chart::packed
