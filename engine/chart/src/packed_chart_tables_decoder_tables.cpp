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

auto Decoder::readStrings() -> core::Result<void> {
    const auto* strSection = section("STR0");
    ByteReader strReader(strSection->data);
    auto strCount = strReader.readU32();
    auto dataBytes = strReader.readU32();
    if (!strCount || !dataBytes || *strCount != stringCount)
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
    if (offsets.front() != 0U || offsets.back() != *dataBytes)
        return core::unexpected(fail("packed.strings.offset", "STR0 endpoint offsets are invalid"));
    strings.reserve(*strCount);
    for (std::uint32_t i = 0; i < *strCount; ++i) {
        if (offsets[i] > offsets[i + 1U] || offsets[i + 1U] > *dataBytes)
            return core::unexpected(fail("packed.strings.offset", "STR0 offsets are invalid"));
        strings.emplace_back(reinterpret_cast<const char*>(stringData->data() + offsets[i]),
                             offsets[i + 1U] - offsets[i]);
        if (gameplay && !validUtf8(strings.back())) {
            return core::unexpected(fail("packed.strings.bounds", "STR0 text is not valid UTF-8"));
        }
    }
    // Spec 6.1: STR0 strings are deduplicated and strictly ascending by bytes. The repository
    // Writer already emits that order; a non-canonical artifact is refused instead of resolved.
    for (std::size_t i = 1; i < strings.size(); ++i) {
        if (!(strings[i - 1U] < strings[i]))
            return core::unexpected(
                fail("packed.strings.order", "STR0 strings must be unique and strictly ascending"));
    }
    return {};
}

auto Decoder::readRefs() -> core::Result<void> {
    const auto* refSection = section("REF0");
    ByteReader refReader(refSection->data);
    // Spec 3.2: every REF0 row occupies at least a kind byte and a one-byte string index, so the
    // declared count is bounded by the payload before the row vector is reserved.
    if (static_cast<std::uint64_t>(refCount) > refSection->data.size() / 2U)
        return core::unexpected(
            fail("packed.references.invalid", "REF0 row count exceeds the section payload"));
    refs.reserve(refCount);
    for (std::uint32_t i = 0; i < refCount; ++i) {
        auto kind = refReader.readU8();
        auto index = refReader.readUnsignedLeb128(stringCount);
        if (!kind || !index)
            return core::unexpected(fail("packed.references.invalid", "REF0 row is invalid"));
        auto value = getString(*index);
        if (!value)
            return core::unexpected(std::move(value.error()));
        if (gameplay && (*kind == 0U || *kind > 13U))
            return core::unexpected(fail("packed.references.kind",
                                         "Reference kind is outside the revision-2 registry"));
        if (gameplay && revision == 4U) {
            if (*kind == 11U || *kind == 12U)
                return core::unexpected(
                    fail("packed.references.kind", "Revision 4 omits derived physical references"));
            if (*kind == 13U && (value->size() < 8 || !value->starts_with("cxgp2:0") ||
                                 ((*value)[7] != '0' && ((*value)[7] < '2' || (*value)[7] > '8'))))
                return core::unexpected(
                    fail("packed.references.kind", "Illegal revision-4 projection subdomain"));
        }
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
    return {};
}

auto Decoder::readMeta() -> core::Result<void> {
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
        if (gameplay && chart.features.size() > 1U &&
            !(chart.features[chart.features.size() - 2U].id < chart.features.back().id)) {
            return core::unexpected(
                fail("packed.meta.feature", "META features must be unique and ascending"));
        }
    }
    if (!meta.empty())
        return core::unexpected(fail("packed.meta.trailing", "META has trailing bytes"));
    return {};
}

auto Decoder::readTiming() -> core::Result<void> {
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
    // Spec 6.4: tempo events and stops are ordered by Beat. The preimage sorts them, so a
    // non-canonical order would otherwise be accepted as if it were canonical (A12).
    for (std::size_t i = 1; i < chart.timing.tempoEvents.size(); ++i) {
        if (!(chart.timing.tempoEvents[i - 1U].startBeat < chart.timing.tempoEvents[i].startBeat))
            return core::unexpected(fail(
                "packed.time.order", "Tempo events must be unique and ascending by start beat"));
    }
    for (std::size_t i = 1; i < chart.timing.stops.size(); ++i) {
        if (!(chart.timing.stops[i - 1U].beat < chart.timing.stops[i].beat))
            return core::unexpected(
                fail("packed.time.order", "Timing stops must be unique and ascending by beat"));
    }
    if (!time.empty())
        return core::unexpected(fail("packed.time.trailing", "TIME has trailing bytes"));
    return {};
}

} // namespace cuexis::chart::packed::decoder_detail
