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

auto decodeImpl(std::span<const std::byte> bytes, PackedChartLimits limits, bool gameplay,
                gameplay_detail::DecodedStatic* staticResult)
    -> core::Result<CanonicalSemanticChart> {
    auto stats = inspectImpl(bytes, limits, gameplay);
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
        !refCount || !revision || !headerCrc ||
        (gameplay ? (*revision != 2U && *revision != 3U && *revision != 4U) : (*revision != 1U)))
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
        auto role = classifySection(code, *flags, gameplay);
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
    if (gameplay) {
        for (const auto required : {"GPH0", "GPR0", "GPD0", "GRC0"}) {
            if (!section(required)) {
                return core::unexpected(
                    fail("packed.directory.required", "Gameplay required section is missing"));
            }
        }
        if (section("STR0")->records != *stringCount || section("REF0")->records != *refCount ||
            section("ENT0")->records != *entityCount || section("IDN0")->records != *entityCount ||
            section("META")->records != 1U || section("TIME")->records != 1U ||
            section("GPH0")->records != 1U || section("GPR0")->records != *requirementCount ||
            section("REQ0")->records != 0U || section("CNS0")->records != 0U ||
            section("REQ0")->data.size() != 2U || !section("CNS0")->data.empty()) {
            return core::unexpected(
                fail("packed.header.invalid", "Revision-2 section counters are inconsistent"));
        }
    }
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
    if (offsets.front() != 0U || offsets.back() != *dataBytes)
        return core::unexpected(fail("packed.strings.offset", "STR0 endpoint offsets are invalid"));
    std::vector<std::string> strings;
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
        if (gameplay && (*kind == 0U || *kind > 13U))
            return core::unexpected(fail("packed.references.kind",
                                         "Reference kind is outside the revision-2 registry"));
        if (gameplay && *revision == 4U) {
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
        if (gameplay && chart.features.size() > 1U &&
            !(chart.features[chart.features.size() - 2U].id < chart.features.back().id)) {
            return core::unexpected(
                fail("packed.meta.feature", "META features must be unique and ascending"));
        }
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
        if (gameplay) {
            const auto bit = kind == 0 ? 1U : kind == 1 ? 2U : 16U;
            std::size_t expectedRows = 0;
            for (const auto arch : archetypes) {
                expectedRows += (arches[arch].mask & bit) != 0U ? 1U : 0U;
            }
            if ((stream && stream->records != expectedRows) || (!stream && expectedRows != 0U) ||
                (stream && expectedRows == 0U)) {
                return core::unexpected(fail("packed.stream.component",
                                             "Component stream presence or count is invalid"));
            }
        }
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
                                                 : std::uint64_t{0x0fU};
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
    const auto legacyRequirementCount = gameplay ? 0U : *requirementCount;
    for (std::uint32_t i = 0; i < legacyRequirementCount; ++i) {
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
        requirement.judgementDomain = {"judgement-domain", *domainValue};
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
        return chart;
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
    if (std::memcmp(semanticHash->data(), recomputed->data(), recomputed->size()) != 0)
        return core::unexpected(
            fail("packed.identity.mismatch",
                 "Packed semantic identity does not match the recomputed digest"));
    return chart;
}

} // namespace cuexis::chart::packed
