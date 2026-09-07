#include <cuexis/chart/packed_chart_io.hpp>

#include <cuexis/chart/packed_chart_primitives.hpp>
#include <cuexis/core/error.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <fstream>
#include <limits>
#include <map>
#include <span>
#include <string>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace cuexis::chart {
namespace {

namespace fs = std::filesystem;
using packed::ByteReader;

[[nodiscard]] auto error(std::string code, std::string message) -> core::Error {
    return core::Error{std::move(code), std::move(message)};
}

[[nodiscard]] auto temporarySibling(const fs::path& target, std::uint64_t attempt) -> fs::path {
    static std::atomic<std::uint64_t> sequence{0};
    const auto ticks =
        static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto token = ticks ^ sequence.fetch_add(1, std::memory_order_relaxed) ^ attempt;
    return target.parent_path() /
           (target.filename().native() + fs::path{".tmp." + std::to_string(token)}.native());
}

[[nodiscard]] auto checkedRange(std::size_t offset, std::size_t length, std::size_t total) -> bool {
    return offset <= total && length <= total - offset;
}

[[nodiscard]] auto parseSizing(std::span<const std::byte> bytes,
                               const packed::PackedChartStatistics& statistics)
    -> core::Result<PackedChartSizing> {
    if (bytes.size() < 96U) {
        return core::unexpected(error("packed.io.truncated_header", "Packed header is truncated"));
    }
    ByteReader header{bytes};
    auto magic = header.readBytes(8);
    if (!magic) {
        return core::unexpected(std::move(magic.error()));
    }
    constexpr std::array<std::byte, 8> expected{std::byte{'C'}, std::byte{'X'}, std::byte{'P'},
                                                std::byte{'K'}, std::byte{'5'}, std::byte{0},
                                                std::byte{0},   std::byte{0}};
    if (!std::equal(expected.begin(), expected.end(), magic->begin())) {
        return core::unexpected(error("packed.io.invalid_magic", "Packed magic is invalid"));
    }
    auto version = header.readU16();
    auto headerBytes = header.readU16();
    auto flags = header.readU32();
    auto totalBytes = header.readU32();
    auto directoryOffset = header.readU32();
    auto directoryCount = header.readU32();
    auto directoryBytes = header.readU32();
    if (!version || !headerBytes || !flags || !totalBytes || !directoryOffset || !directoryCount ||
        !directoryBytes) {
        return core::unexpected(error("packed.io.truncated_header", "Packed header is truncated"));
    }
    if (*version != 1U || *headerBytes != 96U || *directoryOffset != 96U ||
        *totalBytes != bytes.size()) {
        return core::unexpected(
            error("packed.io.header_mismatch", "Packed header values mismatch"));
    }
    if (*directoryCount > (std::numeric_limits<std::size_t>::max() / 32U) ||
        *directoryBytes != *directoryCount * 32U ||
        !checkedRange(*directoryOffset, *directoryBytes, bytes.size())) {
        return core::unexpected(error("packed.io.directory_bounds", "Packed directory is invalid"));
    }

    PackedChartSizing sizing{.statistics = statistics,
                             .headerBytes = *headerBytes,
                             .directoryBytes = *directoryBytes,
                             .sections = {}};
    sizing.sections.reserve(*directoryCount);
    ByteReader directory{bytes.subspan(*directoryOffset, *directoryBytes)};
    std::size_t previousEnd = *directoryOffset + *directoryBytes;
    for (std::uint32_t index = 0; index < *directoryCount; ++index) {
        auto typeBytes = directory.readBytes(4);
        auto codec = directory.readU8();
        auto sectionFlags = directory.readU8();
        auto reserved = directory.readU16();
        auto offset = directory.readU32();
        auto encoded = directory.readU32();
        auto decoded = directory.readU32();
        auto records = directory.readU32();
        auto crc = directory.readU32();
        auto reserved2 = directory.readU32();
        if (!typeBytes || !codec || !sectionFlags || !reserved || !offset || !encoded || !decoded ||
            !records || !crc || !reserved2 || *codec != 0U || *reserved != 0U || *reserved2 != 0U ||
            *sectionFlags > 1U || !checkedRange(*offset, *encoded, bytes.size()) ||
            *offset != previousEnd) {
            return core::unexpected(
                error("packed.io.directory_invalid", "Packed directory entry is invalid"));
        }
        std::string type;
        type.reserve(4);
        for (const auto byte : *typeBytes) {
            const auto value = std::to_integer<unsigned char>(byte);
            if (value < 0x20U || value > 0x7eU) {
                return core::unexpected(
                    error("packed.io.section_type", "Packed section type is invalid"));
            }
            type.push_back(static_cast<char>(value));
        }
        sizing.sections.push_back(
            PackedSectionSize{std::move(type), *offset, *encoded, *decoded, *records});
        previousEnd = static_cast<std::size_t>(*offset) + *encoded;
    }
    if (previousEnd != bytes.size()) {
        return core::unexpected(
            error("packed.io.trailing_bytes", "Packed file has trailing bytes"));
    }
    return sizing;
}

[[nodiscard]] auto readF32(ByteReader& reader) -> core::Result<float> {
    auto value = reader.readU32();
    if (!value)
        return core::unexpected(std::move(value.error()));
    return std::bit_cast<float>(*value);
}

[[nodiscard]] auto readF64(ByteReader& reader) -> core::Result<double> {
    auto value = reader.readU64();
    if (!value)
        return core::unexpected(std::move(value.error()));
    return std::bit_cast<double>(*value);
}

[[nodiscard]] auto readUuid(ByteReader& reader) -> core::Result<std::string> {
    auto bytes = reader.readBytes(16);
    if (!bytes)
        return core::unexpected(std::move(bytes.error()));
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(36);
    for (std::size_t index = 0; index < 16; ++index) {
        if (index == 4 || index == 6 || index == 8 || index == 10)
            result.push_back('-');
        const auto value = std::to_integer<std::uint8_t>((*bytes)[index]);
        result.push_back(digits[value >> 4U]);
        result.push_back(digits[value & 0x0fU]);
    }
    return result;
}

[[nodiscard]] auto readTransform(ByteReader& reader) -> core::Result<CanonicalTransform> {
    auto x = readF32(reader);
    auto y = readF32(reader);
    auto z = readF32(reader);
    auto rx = readF32(reader);
    auto ry = readF32(reader);
    auto rz = readF32(reader);
    auto rw = readF32(reader);
    auto sx = readF32(reader);
    auto sy = readF32(reader);
    auto sz = readF32(reader);
    if (!x || !y || !z || !rx || !ry || !rz || !rw || !sx || !sy || !sz) {
        return core::unexpected(
            error("packed.io.transform", "Packed Transform payload is truncated"));
    }
    return CanonicalTransform{core::Vec3{*x, *y, *z}, core::Quat{*rx, *ry, *rz, *rw},
                              core::Vec3{*sx, *sy, *sz}};
}

struct RawSection final {
    std::span<const std::byte> bytes;
    std::uint8_t flags{};
    std::uint32_t records{};
};

[[nodiscard]] auto sections(std::span<const std::byte> bytes)
    -> core::Result<std::map<std::string, RawSection>> {
    ByteReader reader{bytes};
    if (!reader.readBytes(8) || !reader.readU16() || !reader.readU16() || !reader.readU32() ||
        !reader.readU32() || !reader.readU32()) {
        return core::unexpected(error("packed.io.truncated_header", "Packed header is truncated"));
    }
    auto count = reader.readU32();
    auto directoryBytes = reader.readU32();
    if (!count || !directoryBytes || *directoryBytes != *count * 32U ||
        !checkedRange(96U, *directoryBytes, bytes.size())) {
        return core::unexpected(error("packed.io.directory_bounds", "Packed directory is invalid"));
    }
    static_cast<void>(reader.readBytes(32));
    for (int index = 0; index < 8; ++index)
        static_cast<void>(reader.readU32());
    std::map<std::string, RawSection> result;
    ByteReader directory{bytes.subspan(96U, *directoryBytes)};
    std::size_t expectedOffset = 96U + *directoryBytes;
    for (std::uint32_t index = 0; index < *count; ++index) {
        auto type = directory.readBytes(4);
        auto codec = directory.readU8();
        auto flags = directory.readU8();
        auto reserved = directory.readU16();
        auto offset = directory.readU32();
        auto encoded = directory.readU32();
        auto decoded = directory.readU32();
        auto records = directory.readU32();
        auto crc = directory.readU32();
        auto reserved2 = directory.readU32();
        if (!type || !codec || !flags || !reserved || !offset || !encoded || !decoded || !records ||
            !crc || !reserved2 || *codec != 0U || *flags > 1U || *reserved != 0U ||
            *reserved2 != 0U || *decoded != *encoded || *offset != expectedOffset ||
            !checkedRange(*offset, *encoded, bytes.size()) ||
            packed::crc32(bytes.subspan(*offset, *encoded)) != *crc) {
            return core::unexpected(
                error("packed.io.directory_invalid", "Packed directory entry is invalid"));
        }
        const std::string key{reinterpret_cast<const char*>(type->data()), 4};
        if (!result.emplace(key, RawSection{bytes.subspan(*offset, *encoded), *flags, *records})
                 .second) {
            return core::unexpected(
                error("packed.io.duplicate_section", "Packed section is duplicated"));
        }
        expectedOffset = static_cast<std::size_t>(*offset) + *encoded;
    }
    if (expectedOffset != bytes.size()) {
        return core::unexpected(
            error("packed.io.trailing_bytes", "Packed file has trailing bytes"));
    }
    return result;
}

[[maybe_unused]] [[nodiscard]] auto decodeCandidate(std::span<const std::byte> bytes,
                                                    PackedChartLimits limits)
    -> core::Result<CanonicalSemanticChart> {
    auto stats = packed::inspect(bytes, limits);
    if (!stats)
        return core::unexpected(std::move(stats.error()));
    auto table = sections(bytes);
    if (!table)
        return core::unexpected(std::move(table.error()));
    static constexpr std::array<std::string_view, 14> known{"META", "TIME", "STR0", "REF0", "IDN0",
                                                            "ARCH", "ENT0", "TRN0", "REN0", "REQ0",
                                                            "CNS0", "CAM0", "DBG0", ""};
    for (const auto& [name, section] : *table) {
        if (std::find(known.begin(), known.end(), name) == known.end()) {
            return core::unexpected(
                error("packed.io.unknown_section", "Packed section is not registered"));
        }
        if (name != "DBG0" && section.flags != 0U) {
            return core::unexpected(
                error("packed.io.section_flags", "Semantic Packed section has inspection flags"));
        }
    }
    const auto get = [&](std::string_view name) -> core::Result<RawSection> {
        const auto found = table->find(std::string{name});
        if (found == table->end())
            return core::unexpected(
                error("packed.io.required_section", "Packed required section is missing"));
        return found->second;
    };
    auto meta = get("META");
    auto str0 = get("STR0");
    auto ref0 = get("REF0");
    auto idn0 = get("IDN0");
    auto arch = get("ARCH");
    auto ent0 = get("ENT0");
    auto req0 = get("REQ0");
    auto cns0 = get("CNS0");
    if (!meta || !str0 || !ref0 || !idn0 || !arch || !ent0 || !req0 || !cns0) {
        return core::unexpected(
            error("packed.io.required_section", "Packed required section is missing"));
    }
    std::vector<std::string> strings;
    {
        ByteReader reader{str0->bytes};
        auto count = reader.readU32();
        auto dataBytes = reader.readU32();
        if (!count || !dataBytes || *count > limits.maxPackedStrings)
            return core::unexpected(
                error("packed.io.string_budget", "Packed string table exceeds limits"));
        std::vector<std::uint32_t> offsets;
        offsets.reserve(*count + 1U);
        for (std::uint32_t i = 0; i <= *count; ++i) {
            auto value = reader.readU32();
            if (!value)
                return core::unexpected(std::move(value.error()));
            offsets.push_back(*value);
        }
        auto data = reader.readBytes(*dataBytes);
        if (!data || !reader.empty() || offsets.back() != *dataBytes)
            return core::unexpected(
                error("packed.io.string_table", "Packed string table is invalid"));
        for (std::uint32_t i = 0; i < *count; ++i) {
            if (offsets[i] > offsets[i + 1U])
                return core::unexpected(
                    error("packed.io.string_table", "Packed string offsets are not monotonic"));
            strings.emplace_back(reinterpret_cast<const char*>(data->data() + offsets[i]),
                                 offsets[i + 1U] - offsets[i]);
        }
    }
    struct Ref final {
        std::uint8_t kind{};
        std::uint32_t string{};
    };
    std::vector<Ref> refs;
    {
        ByteReader reader{ref0->bytes};
        for (std::uint32_t i = 0; i < ref0->records; ++i) {
            auto kind = reader.readU8();
            auto string = reader.readUnsignedLeb128();
            if (!kind || !string || *string >= strings.size())
                return core::unexpected(
                    error("packed.io.reference", "Packed reference is invalid"));
            refs.push_back(Ref{*kind, static_cast<std::uint32_t>(*string)});
        }
        if (!reader.empty())
            return core::unexpected(
                error("packed.io.reference", "Packed reference table has trailing bytes"));
    }
    CanonicalSemanticChart chart;
    {
        ByteReader reader{meta->bytes};
        auto id = readUuid(reader);
        auto version = reader.readU16();
        auto music = reader.readUnsignedLeb128();
        auto camera = reader.readU8();
        auto fov = readF64(reader);
        auto nearPlane = readF64(reader);
        auto farPlane = readF64(reader);
        auto pitch = readF64(reader);
        auto yaw = readF64(reader);
        auto roll = readF64(reader);
        auto px = readF32(reader);
        auto py = readF32(reader);
        auto pz = readF32(reader);
        auto featureCount = reader.readU32();
        if (!id || !version || !music || !camera || !fov || !nearPlane || !farPlane || !pitch ||
            !yaw || !roll || !px || !py || !pz || !featureCount || *version != 5U || *camera != 1U)
            return core::unexpected(error("packed.io.meta", "Packed META is invalid"));
        chart.chartId = ChartId{*id};
        chart.defaultCamera =
            CameraData{"perspective", *fov,
                       *nearPlane,    *farPlane,
                       *pitch,        *yaw,
                       *roll,         TransformData{{*px, *py, *pz}, {}, {1.0F, 1.0F, 1.0F}}};
        if (*music != 0U) {
            if (*music - 1U >= refs.size() || refs[*music - 1U].kind != 1U)
                return core::unexpected(
                    error("packed.io.meta", "Packed main music reference is invalid"));
            chart.mainMusic = AssetId{strings[refs[*music - 1U].string]};
        }
        for (std::uint32_t i = 0; i < *featureCount; ++i) {
            auto ref = reader.readUnsignedLeb128();
            auto featureVersion = reader.readU32();
            if (!ref || !featureVersion || *ref >= refs.size() || refs[*ref].kind != 7U)
                return core::unexpected(
                    error("packed.io.meta", "Packed feature reference is invalid"));
            chart.features.push_back(CanonicalFeature{strings[refs[*ref].string], *featureVersion});
        }
    }
    if (const auto timeIt = table->find("TIME"); timeIt != table->end()) {
        ByteReader reader{timeIt->second.bytes};
        auto offset = readF64(reader);
        auto bpm = readF64(reader);
        auto tempoCount = reader.readU32();
        auto stopCount = reader.readU32();
        auto tempoMode = reader.readU8();
        auto tempoDurationMode = reader.readU8();
        auto stopMode = reader.readU8();
        if (!offset || !bpm || !tempoCount || !stopCount || !tempoMode || !tempoDurationMode ||
            !stopMode || *tempoMode != 0U || *tempoDurationMode != 0U || *stopMode != 0U)
            return core::unexpected(error("packed.io.time", "Packed TIME is invalid"));
        chart.timing.offsetMs = *offset;
        chart.timing.defaultBpm = *bpm;
        for (std::uint32_t i = 0; i < *tempoCount; ++i) {
            auto start = packed::readRationalBeatAtom(reader);
            auto duration = packed::readRationalBeatAtom(reader);
            auto startBpm = readF64(reader);
            auto endBpm = readF64(reader);
            auto startSlope = readF64(reader);
            auto endSlope = readF64(reader);
            if (!start || !duration || !startBpm || !endBpm || !startSlope || !endSlope)
                return core::unexpected(error("packed.io.time", "Packed tempo record is invalid"));
            chart.timing.tempoEvents.push_back(
                TempoEvent{*start, *duration, *startBpm, *endBpm, *startSlope, *endSlope});
        }
        for (std::uint32_t i = 0; i < *stopCount; ++i) {
            auto beat = packed::readRationalBeatAtom(reader);
            auto duration = readF64(reader);
            if (!beat || !duration)
                return core::unexpected(error("packed.io.time", "Packed stop record is invalid"));
            chart.timing.stops.push_back(TimingStop{*beat, *duration});
        }
    }
    struct DecodedArchetype final {
        std::uint64_t mask{};
        std::vector<CanonicalComponent> components;
    };
    std::vector<DecodedArchetype> archetypes;
    {
        ByteReader reader{arch->bytes};
        for (std::uint32_t i = 0; i < arch->records; ++i) {
            auto mask = reader.readU64();
            auto payloadBytes = reader.readU32();
            if (!mask || !payloadBytes || *payloadBytes > reader.remaining())
                return core::unexpected(error("packed.io.archetype", "Packed ARCH row is invalid"));
            ByteReader payload{*reader.readBytes(*payloadBytes)};
            DecodedArchetype decoded{*mask, {}};
            if ((*mask & 1U) != 0U) {
                auto transform = readTransform(payload);
                if (!transform)
                    return core::unexpected(std::move(transform.error()));
                decoded.components.emplace_back(*transform);
            }
            if ((*mask & 2U) != 0U) {
                auto mesh = payload.readUnsignedLeb128();
                auto material = payload.readUnsignedLeb128();
                auto alpha = payload.readU8();
                if (!mesh || !material || !alpha || *mesh >= refs.size() ||
                    *material >= refs.size())
                    return core::unexpected(
                        error("packed.io.archetype", "Packed Renderable default is invalid"));
                decoded.components.emplace_back(
                    CanonicalRenderable{AssetId{strings[refs[*mesh].string]},
                                        AssetId{strings[refs[*material].string]}, *alpha});
            }
            if ((*mask & 16U) != 0U) {
                auto type = payload.readU8();
                auto fov = readF64(payload);
                auto nearPlane = readF64(payload);
                auto farPlane = readF64(payload);
                if (!type || !fov || !nearPlane || !farPlane || *type != 1U)
                    return core::unexpected(
                        error("packed.io.archetype", "Packed Camera default is invalid"));
                decoded.components.emplace_back(
                    CanonicalCamera{"perspective", *fov, *nearPlane, *farPlane});
            }
            if (!payload.empty())
                return core::unexpected(
                    error("packed.io.archetype", "Packed ARCH payload has trailing bytes"));
            archetypes.push_back(std::move(decoded));
        }
    }
    std::vector<std::uint32_t> parents;
    std::vector<std::uint32_t> entityArchetypes;
    {
        ByteReader reader{ent0->bytes};
        for (std::uint32_t i = 0; i < stats->entityCount; ++i) {
            auto parent = reader.readUnsignedLeb128();
            auto archetype = reader.readUnsignedLeb128();
            if (!parent || !archetype || *parent > stats->entityCount ||
                *archetype >= archetypes.size())
                return core::unexpected(error("packed.io.entities", "Packed ENT0 is invalid"));
            parents.push_back(static_cast<std::uint32_t>(*parent));
            entityArchetypes.push_back(static_cast<std::uint32_t>(*archetype));
        }
        if (!reader.empty())
            return core::unexpected(error("packed.io.entities", "Packed ENT0 has trailing bytes"));
    }
    {
        ByteReader reader{idn0->bytes};
        auto scopes = reader.readU32();
        auto paths = reader.readU32();
        auto count = reader.readU32();
        if (!scopes || !paths || !count || *count != stats->entityCount)
            return core::unexpected(error("packed.io.identity", "Packed IDN0 count is invalid"));
        for (std::uint32_t i = 0; i < *count; ++i) {
            auto tag = reader.readU8();
            if (!tag)
                return core::unexpected(std::move(tag.error()));
            if (*tag == 0U) {
                auto id = readUuid(reader);
                if (!id)
                    return core::unexpected(std::move(id.error()));
                CanonicalEntity entity;
                entity.identity = ExplicitEntityIdentity{ChartObjectId{*id}};
                chart.entities.push_back(std::move(entity));
            } else if (*tag == 1U) {
                auto chartId = readUuid(reader);
                auto binding = reader.readUnsignedLeb128();
                auto module = reader.readUnsignedLeb128();
                auto exportId = reader.readUnsignedLeb128();
                auto pathCount = reader.readUnsignedLeb128();
                if (!chartId || !binding || !module || !exportId || !pathCount ||
                    *binding >= strings.size() || *module >= strings.size() ||
                    *exportId >= strings.size())
                    return core::unexpected(
                        error("packed.io.identity", "Packed generated identity is invalid"));
                GeneratedEntityIdentity identity{
                    ChartId{*chartId}, strings[*binding], strings[*module], strings[*exportId], {}};
                for (std::uint64_t step = 0; step < *pathCount; ++step) {
                    auto node = reader.readUnsignedLeb128();
                    auto iteration = reader.readUnsignedLeb128();
                    if (!node || !iteration || *node >= strings.size())
                        return core::unexpected(error("packed.io.identity",
                                                      "Packed generated identity path is invalid"));
                    identity.path.push_back(SemanticIdentityStep{
                        strings[*node], static_cast<std::uint32_t>(*iteration)});
                }
                CanonicalEntity entity;
                entity.identity = std::move(identity);
                chart.entities.push_back(std::move(entity));
            } else
                return core::unexpected(
                    error("packed.io.identity", "Packed identity tag is unsupported"));
        }
    }
    for (std::size_t i = 0; i < chart.entities.size(); ++i) {
        chart.entities[i].components = archetypes[entityArchetypes[i]].components;
        if (parents[i] != 0U)
            chart.entities[i].parent = chart.entities[parents[i] - 1U].identity;
    }
    {
        std::vector<std::uint32_t> laneSets;
        ByteReader reader{cns0->bytes};
        for (std::uint32_t i = 0; i < cns0->records; ++i) {
            auto kind = reader.readU8();
            auto lane = reader.readUnsignedLeb128();
            if (!kind || !lane || *kind != 1U || *lane > std::numeric_limits<std::uint32_t>::max())
                return core::unexpected(
                    error("packed.io.constraints", "Packed CNS0 row is invalid"));
            laneSets.push_back(static_cast<std::uint32_t>(*lane));
        }
        ByteReader requirements{req0->bytes};
        auto startMode = requirements.readU8();
        auto endMode = requirements.readU8();
        if (!startMode || !endMode || *startMode != 0U || *endMode != 0U)
            return core::unexpected(
                error("packed.io.requirements", "Packed REQ0 descriptor is invalid"));
        std::uint32_t ordinal = 0;
        for (std::uint32_t i = 0; i < stats->requirementCount; ++i) {
            auto delta = requirements.readUnsignedLeb128();
            auto local = requirements.readUnsignedLeb128();
            auto kind = requirements.readU8();
            auto intervalKind = requirements.readU8();
            auto start = packed::readRationalBeatAtom(requirements);
            if (!delta || !local || !kind || !intervalKind || !start || *local >= strings.size())
                return core::unexpected(
                    error("packed.io.requirements", "Packed REQ0 row is invalid"));
            ordinal += static_cast<std::uint32_t>(*delta);
            if (ordinal >= chart.entities.size())
                return core::unexpected(error("packed.io.requirements",
                                              "Packed requirement entity ordinal is invalid"));
            CanonicalRequirement requirement;
            requirement.localId = strings[*local];
            requirement.kind = static_cast<CanonicalRequirementKind>(*kind);
            requirement.interval.kind = static_cast<CanonicalIntervalKind>(*intervalKind);
            requirement.interval.startBeat = *start;
            if (requirement.interval.kind == CanonicalIntervalKind::HalfOpenRange) {
                auto end = packed::readRationalBeatAtom(requirements);
                if (!end)
                    return core::unexpected(std::move(end.error()));
                requirement.interval.endBeat = *end;
            }
            auto domain = requirements.readUnsignedLeb128();
            auto action = requirements.readUnsignedLeb128();
            auto lane = requirements.readUnsignedLeb128();
            auto effects = requirements.readU8();
            if (!domain || !action || !lane || !effects || *domain >= refs.size() ||
                *action >= refs.size() || *lane == 0U || *lane > laneSets.size() ||
                *effects != 0U || refs[*domain].kind != 4U || refs[*action].kind != 5U)
                return core::unexpected(
                    error("packed.io.requirements", "Packed requirement references are invalid"));
            requirement.judgementDomain =
                TypedReference{"candidate.lanes4", strings[refs[*domain].string]};
            requirement.requiredAction = TypedReference{"action", strings[refs[*action].string]};
            requirement.constraints.emplace_back(LaneConstraint{laneSets[*lane - 1U]});
            chart.entities[ordinal].requirements.push_back(std::move(requirement));
        }
        if (!requirements.empty())
            return core::unexpected(
                error("packed.io.requirements", "Packed REQ0 has trailing bytes"));
    }
    return chart;
}

[[nodiscard]] auto writeTemporary(const fs::path& path, std::span<const std::byte> bytes)
    -> core::Result<void> {
#if defined(_WIN32)
    const HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return core::unexpected(
            error("packed.io.temp_create_failed", "Packed temporary file could not be created"));
    }
    bool succeeded = true;
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto remaining = std::min<std::size_t>(bytes.size() - offset, 0x7fffffffU);
        DWORD written = 0;
        if (WriteFile(handle, bytes.data() + offset, static_cast<DWORD>(remaining), &written,
                      nullptr) == 0 ||
            written == 0) {
            succeeded = false;
            break;
        }
        offset += written;
    }
    if (succeeded && FlushFileBuffers(handle) == 0) {
        succeeded = false;
    }
    const bool closed = CloseHandle(handle) != 0;
    if (!succeeded || !closed) {
        return core::unexpected(
            error("packed.io.temp_write_failed", "Packed temporary file write failed"));
    }
#else
    const int descriptor = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (descriptor < 0) {
        return core::unexpected(
            error("packed.io.temp_create_failed", "Packed temporary file could not be created"));
    }
    bool succeeded = true;
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto written = ::write(descriptor, bytes.data() + offset, bytes.size() - offset);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            succeeded = false;
            break;
        }
        offset += static_cast<std::size_t>(written);
    }
    if (succeeded && ::fsync(descriptor) != 0) {
        succeeded = false;
    }
    const bool closed = ::close(descriptor) == 0;
    if (!succeeded || !closed) {
        return core::unexpected(
            error("packed.io.temp_write_failed", "Packed temporary file write failed"));
    }
#endif
    return {};
}

[[nodiscard]] auto replaceAtomically(const fs::path& source, const fs::path& target)
    -> core::Result<void> {
#if defined(_WIN32)
    if (MoveFileExW(source.c_str(), target.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
        return core::unexpected(
            error("packed.io.replace_failed", "Packed atomic replacement failed"));
    }
#else
    std::error_code renameError;
    fs::rename(source, target, renameError);
    if (renameError) {
        return core::unexpected(
            error("packed.io.replace_failed", "Packed atomic replacement failed"));
    }
#endif
    return {};
}

} // namespace

auto PackedChartWriter::size(const CanonicalSemanticChart& chart,
                             packed::PackedChartProfile profile, PackedChartLimits limits)
    -> core::Result<PackedChartSizing> {
    auto bytes = packed::encode(chart, profile);
    if (!bytes) {
        return core::unexpected(std::move(bytes.error()));
    }
    if (bytes->size() > limits.maxPackedFileBytes) {
        return core::unexpected(
            error("packed.io.file_limit", "Packed chart exceeds file byte limit"));
    }
    auto statistics = packed::inspect(*bytes, limits);
    if (!statistics) {
        return core::unexpected(std::move(statistics.error()));
    }
    return parseSizing(*bytes, *statistics);
}

auto PackedChartWriter::writeAtomic(const CanonicalSemanticChart& chart, const fs::path& target,
                                    PackedChartWriteOptions options)
    -> core::Result<PackedChartSizing> {
    auto bytes = packed::encode(chart, options.profile);
    if (!bytes) {
        return core::unexpected(std::move(bytes.error()));
    }
    if (bytes->size() > options.limits.maxPackedFileBytes) {
        return core::unexpected(
            error("packed.io.file_limit", "Packed chart exceeds file byte limit"));
    }
    auto statistics = packed::inspect(*bytes, options.limits);
    if (!statistics) {
        return core::unexpected(std::move(statistics.error()));
    }
    auto sizing = parseSizing(*bytes, *statistics);
    if (!sizing) {
        return core::unexpected(std::move(sizing.error()));
    }
    std::error_code statusError;
    const auto parent =
        target.has_parent_path() ? target.parent_path() : fs::current_path(statusError);
    if (statusError || !fs::is_directory(parent, statusError)) {
        return core::unexpected(
            error("packed.io.parent_missing", "Packed output parent directory is missing"));
    }
    fs::path temporary;
    core::Result<void> written = core::unexpected(
        error("packed.io.temp_create_failed", "Packed temporary file creation failed"));
    for (std::uint64_t attempt = 0; attempt < 128U; ++attempt) {
        temporary = temporarySibling(target, attempt);
        written = writeTemporary(temporary, *bytes);
        if (written) {
            break;
        }
        std::error_code ignored;
        fs::remove(temporary, ignored);
    }
    if (!written) {
        return core::unexpected(std::move(written.error()));
    }
    auto cleanup = [&temporary]() {
        std::error_code ignored;
        fs::remove(temporary, ignored);
    };
    auto replaced = replaceAtomically(temporary, target);
    if (!replaced) {
        cleanup();
        return core::unexpected(std::move(replaced.error()));
    }
    return *sizing;
}

auto PackedChartReader::decode(std::span<const std::byte> bytes, PackedChartLimits limits)
    -> core::Result<CanonicalSemanticChart> {
    if (bytes.size() > limits.maxPackedFileBytes) {
        return core::unexpected(
            error("packed.io.file_limit", "Packed chart exceeds file byte limit"));
    }
    auto table = sections(bytes);
    if (!table) {
        return core::unexpected(std::move(table.error()));
    }
    static constexpr std::array<std::string_view, 14> known{"META", "TIME", "STR0", "REF0", "IDN0",
                                                            "ARCH", "ENT0", "TRN0", "REN0", "REQ0",
                                                            "CNS0", "CAM0", "DBG0", ""};
    for (const auto& [name, section] : *table) {
        if (std::find(known.begin(), known.end(), name) == known.end() ||
            (name != "DBG0" && section.flags != 0U)) {
            return core::unexpected(
                error("packed.io.unknown_section", "Packed section is not registered"));
        }
    }
    return packed::decode(bytes, limits);
}

auto PackedChartReader::read(const fs::path& source, PackedChartLimits limits)
    -> core::Result<CanonicalSemanticChart> {
    std::error_code statusError;
    const auto size = fs::file_size(source, statusError);
    if (statusError) {
        return core::unexpected(
            error("packed.io.open_failed", "Packed input file could not be stat'ed"));
    }
    if (size > limits.maxPackedFileBytes) {
        return core::unexpected(
            error("packed.io.file_limit", "Packed chart exceeds file byte limit"));
    }
    std::ifstream input(source, std::ios::binary);
    if (!input) {
        return core::unexpected(
            error("packed.io.open_failed", "Packed input file could not be opened"));
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    if (size != 0U) {
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    }
    if (!input && !input.eof()) {
        return core::unexpected(
            error("packed.io.read_failed", "Packed input file could not be read"));
    }
    return decode(bytes, limits);
}

} // namespace cuexis::chart
