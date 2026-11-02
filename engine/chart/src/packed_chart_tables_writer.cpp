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

auto fail(std::string code, std::string message) -> core::Error {
    return core::Error{std::move(code), std::move(message)};
}

// Checked narrowing for the fixed-width wire fields of the artifact. Every call site is preceded
// by a budget gate that makes the value fit; the check keeps that provable instead of implicit.
// The diagnostic is distinct from the byte budgets on purpose: a wire-range overflow is a
// representability failure, not a budget violation, so it must not be reported as
// `packed.budget.section_bytes` (see Spec 3.3 and the review note SPEC-02).
[[nodiscard]] auto narrowU32(std::uint64_t value) -> core::Result<std::uint32_t> {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        return core::unexpected(
            fail("packed.field.wire_range", "Packed field exceeds the uint32 wire range"));
    }
    return static_cast<std::uint32_t>(value);
}

// Spec 5.3 section registry. inspect() and decode() share this decision so the two entry points
// cannot disagree about which sections an artifact may carry.

[[nodiscard]] auto classifySection(std::string_view name, std::uint8_t flags, bool gameplay)
    -> core::Result<SectionRole> {
    static constexpr std::array<std::string_view, 12> semantic{"META", "TIME", "STR0", "REF0",
                                                               "IDN0", "ARCH", "ENT0", "TRN0",
                                                               "REN0", "CAM0", "CNS0", "REQ0"};
    static constexpr std::array<std::string_view, 4> refused{"ANM0", "BEH0", "BHD0", "FXS0"};
    if (flags > 1U) {
        return core::unexpected(
            fail("packed.directory.flags", "Packed section flags must be 0 or 1"));
    }
    const bool gameplaySection =
        gameplay && (name == "GPH0" || name == "GPR0" || name == "GPD0" || name == "GRC0");
    if (gameplaySection || std::find(semantic.begin(), semantic.end(), name) != semantic.end()) {
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

auto validUtf8(std::string_view text) -> bool {
    std::uint32_t value = 0;
    std::uint32_t minimum = 0;
    unsigned remaining = 0;
    for (const unsigned char byte : text) {
        if (remaining == 0) {
            if (byte < 0x80U) {
                continue;
            }
            if (byte >= 0xc2U && byte <= 0xdfU) {
                value = byte & 0x1fU;
                minimum = 0x80U;
                remaining = 1;
            } else if (byte >= 0xe0U && byte <= 0xefU) {
                value = byte & 0x0fU;
                minimum = 0x800U;
                remaining = 2;
            } else if (byte >= 0xf0U && byte <= 0xf4U) {
                value = byte & 0x07U;
                minimum = 0x10000U;
                remaining = 3;
            } else {
                return false;
            }
        } else {
            if ((byte & 0xc0U) != 0x80U) {
                return false;
            }
            value = (value << 6U) | (byte & 0x3fU);
            if (--remaining == 0 &&
                (value < minimum || value > 0x10ffffU || (value >= 0xd800U && value <= 0xdfffU))) {
                return false;
            }
        }
    }
    return remaining == 0;
}

auto identityKey(const CanonicalEntityIdentity& identity) -> core::Result<std::vector<std::byte>> {
    return identity_detail::canonicalIdentityBytes(identity);
}

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
    std::vector<const CanonicalFeature*> features;
    for (const auto& feature : chart.features) {
        features.push_back(&feature);
    }
    std::sort(features.begin(), features.end(),
              [](const auto* left, const auto* right) { return left->id < right->id; });
    for (const auto* feature : features) {
        auto r = typedRef(dict, 7, feature->id);
        if (!r)
            return core::unexpected(std::move(r.error()));
        writer.writeUnsignedLeb128(*r);
        writer.writeU32(feature->version);
    }
    return Section{{'M', 'E', 'T', 'A'}, std::move(writer).takeBytes(), 1};
}

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
            fail("packed.field.wire_range", "Packed directory exceeds the uint32 wire range"));
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
            fail("packed.field.wire_range", "Packed counters exceed the uint32 wire range"));
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

} // namespace cuexis::chart::packed
