// R4 Packed capacity probe.
//
// Emits one JSON object on stdout with machine-readable capacity data for the frozen Foundation
// profiles, and a short human summary on stderr. The probe owns its own timing and process-peak
// measurement; cmake/VerifyChartCapacity.cmake runs it behind CUEXIS_RUN_PERFORMANCE_PROBE and
// records the implementation SHA next to the data.
//
// Usage: cuexis_chart_capacity_probe [--sha <implementation-sha>]
//
// Methodology notes carried in the JSON itself:
//   * resident/peak values are process-wide working-set numbers (Windows
//     GetProcessMemoryInfo().PeakWorkingSetSize, POSIX getrusage().ru_maxrss). They include the
//     allocator, page commitment and platform differences, so they show order of magnitude and
//     regression trends only; the deterministic gates stay with the frozen byte/count budgets.
//   * elapsed values are std::chrono::steady_clock over one operation on this machine.
//   * a directly constructed typed model has no source bytes; null is recorded instead of a
//     fabricated JSON size.

#include <cuexis/chart/cxt_v2_loader.hpp>
#include <cuexis/chart/packed_chart_io.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>
#include <cuexis/version.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#include <windows.h>

#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace {

using Clock = std::chrono::steady_clock;

struct MemorySnapshot final {
    std::uint64_t residentBytes{};
    std::uint64_t peakResidentBytes{};
};

[[nodiscard]] auto memorySnapshot() -> MemorySnapshot {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)) == FALSE) {
        return {};
    }
    return {static_cast<std::uint64_t>(counters.WorkingSetSize),
            static_cast<std::uint64_t>(counters.PeakWorkingSetSize)};
#else
    long residentPages = 0;
    std::uint64_t resident = 0;
    if (std::FILE* file = std::fopen("/proc/self/statm", "r")) {
        if (std::fscanf(file, "%ld", &residentPages) == 1) {
            resident = static_cast<std::uint64_t>(residentPages) *
                       static_cast<std::uint64_t>(::sysconf(_SC_PAGESIZE));
        }
        std::fclose(file);
    }
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    return {resident, static_cast<std::uint64_t>(usage.ru_maxrss) * 1024ULL};
#endif
}

[[nodiscard]] auto elapsedMicroseconds(Clock::time_point started) -> double {
    return std::chrono::duration<double, std::micro>{Clock::now() - started}.count();
}

[[nodiscard]] auto positiveDelta(std::uint64_t before, std::uint64_t after) -> std::uint64_t {
    return after > before ? after - before : 0U;
}

[[nodiscard]] auto compilerIdentity() -> std::string {
#if defined(_MSC_VER)
    return "MSVC " + std::to_string(_MSC_VER);
#elif defined(__clang__)
    return std::string{"Clang "} + __clang_version__;
#elif defined(__GNUC__)
    return "GCC " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
#else
    return "unknown";
#endif
}

[[nodiscard]] auto buildType() -> std::string {
#if defined(NDEBUG)
    return "Release";
#else
    return "Debug";
#endif
}

[[nodiscard]] auto platformIdentity() -> std::string {
#if defined(_WIN32) && defined(_M_X64)
    return "Windows x64";
#elif defined(_WIN32)
    return "Windows";
#elif defined(__linux__) && defined(__x86_64__)
    return "Linux x86_64";
#else
    return "unknown";
#endif
}

[[nodiscard]] auto jsonString(std::string_view value) -> std::string {
    std::string result{"\""};
    for (const auto character : value) {
        switch (character) {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result.push_back(character);
            break;
        }
    }
    result.push_back('"');
    return result;
}

[[nodiscard]] auto readTextFile(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        return {};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

// ---------------------------------------------------------------------------------------------
// Fixtures (identical shapes to the R4 round-trip and capacity tests)
// ---------------------------------------------------------------------------------------------

[[nodiscard]] auto beat(std::int64_t numerator, std::int64_t denominator)
    -> cuexis::chart::RationalBeat {
    const auto value = cuexis::chart::RationalBeat::create(numerator, denominator);
    return value ? *value : cuexis::chart::RationalBeat::zero();
}

[[nodiscard]] auto baseChart() -> cuexis::chart::CanonicalSemanticChart {
    cuexis::chart::CanonicalSemanticChart chart;
    chart.chartId = cuexis::chart::ChartId{"019b0000-0000-7abc-8def-000000000001"};
    chart.defaultCamera = cuexis::chart::CameraData{};
    chart.features.push_back(
        cuexis::chart::CanonicalFeature{"cuexis.gameplay.candidate.lanes4", 1});
    return chart;
}

[[nodiscard]] auto lowReuseChart(std::size_t count) -> cuexis::chart::CanonicalSemanticChart {
    auto chart = baseChart();
    constexpr char digits[] = "0123456789abcdef";
    chart.entities.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        auto value = static_cast<std::uint32_t>(index + 0x100U);
        std::string suffix(12, '0');
        for (std::size_t position = suffix.size(); position > 0; --position) {
            suffix[position - 1] = digits[value & 0x0fU];
            value >>= 4U;
        }
        cuexis::chart::CanonicalEntity entity;
        entity.identity = cuexis::chart::ExplicitEntityIdentity{
            cuexis::chart::ChartObjectId{"019b0000-0000-7abc-8def-" + suffix}};
        cuexis::chart::CanonicalRequirement requirement;
        requirement.localId = "hit";
        requirement.interval.startBeat = beat(static_cast<std::int64_t>(index), 1);
        requirement.judgementDomain = {"judgement-domain", "candidate.lanes4"};
        requirement.requiredAction = {"action", "press"};
        requirement.constraints.emplace_back(
            cuexis::chart::LaneConstraint{static_cast<std::uint32_t>(index % 4U)});
        entity.requirements.push_back(std::move(requirement));
        chart.entities.push_back(std::move(entity));
    }
    return chart;
}

struct LadderInput final {
    std::string text;
    std::size_t sourceBytes{};
    std::size_t entities{};
    std::size_t requirements{};
};

[[nodiscard]] auto ladderSource(std::size_t groups) -> LadderInput {
    const auto path = std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" /
                      "chart_format_foundation" / "valid" / "pattern_stair.cxt";
    LadderInput input;
    input.text = readTextFile(path);
    input.sourceBytes = input.text.size();
    cuexis::chart::CxtV2Invocation invocation;
    invocation.chartId = cuexis::chart::ChartId{"019b0000-0000-7abc-8def-000000000001"};
    invocation.bindingId = "intro-stair";
    invocation.moduleId = "pattern.stair";
    invocation.exportId = "stair";
    invocation.startBeat = beat(16, 1);
    if (groups != 4U) {
        invocation.parameters.push_back(
            cuexis::chart::CxtV2ParameterBinding{"groups", static_cast<std::int64_t>(groups)});
    }
    const auto expanded = cuexis::chart::CxtV2Loader::expand(input.text, invocation);
    if (expanded.chart) {
        input.entities = expanded.counts.entityCount;
        input.requirements = expanded.counts.requirementCount;
    }
    return input;
}

// ---------------------------------------------------------------------------------------------
// Measurement
// ---------------------------------------------------------------------------------------------

struct ProfileMeasurements final {
    std::string id;
    std::string inputKind;
    std::string status{"ok"};
    std::string detail;
    bool hasSourceBytes{};
    std::size_t sourceBytes{};
    std::size_t expansionEntities{};
    std::size_t expansionRequirements{};
    std::size_t entityCount{};
    std::size_t requirementCount{};
    std::size_t stringCount{};
    std::size_t referenceCount{};
    std::size_t packedBytes{};
    std::size_t decodedBytes{};
    std::size_t headerBytes{};
    std::size_t directoryBytes{};
    std::size_t closureUses{};
    std::size_t closureAssets{};
    double sizeUs{};
    double encodeUs{};
    double decodeUs{};
    double reencodeUs{};
    std::uint64_t residentBeforeBytes{};
    std::uint64_t peakAfterBytes{};
    std::uint64_t peakDeltaBytes{};
    std::vector<cuexis::chart::PackedSectionSize> sections;
};

void measureProfile(ProfileMeasurements& report, cuexis::chart::CanonicalSemanticChart chart) {
    report.entityCount = chart.entities.size();
    for (const auto& entity : chart.entities) {
        report.requirementCount += entity.requirements.size();
    }
    report.closureUses = chart.resourceClosure.resources.size();
    report.closureAssets = chart.resourceClosure.assetIds().size();

    const auto residentStart = memorySnapshot();
    report.residentBeforeBytes = residentStart.residentBytes;

    auto started = Clock::now();
    const auto sizing = cuexis::chart::PackedChartWriter::size(chart);
    report.sizeUs = elapsedMicroseconds(started);
    if (!sizing) {
        report.status = "refused";
        report.detail = sizing.error().code();
        return;
    }
    report.packedBytes = sizing->statistics.packedBytes;
    report.decodedBytes = sizing->statistics.decodedBytes;
    report.headerBytes = sizing->headerBytes;
    report.directoryBytes = sizing->directoryBytes;
    report.stringCount = sizing->statistics.stringCount;
    report.referenceCount = sizing->statistics.referenceCount;
    report.sections = sizing->sections;

    started = Clock::now();
    const auto encoded = cuexis::chart::packed::encode(chart);
    report.encodeUs = elapsedMicroseconds(started);
    const auto afterEncode = memorySnapshot();
    report.peakAfterBytes = afterEncode.peakResidentBytes;
    report.peakDeltaBytes =
        positiveDelta(residentStart.peakResidentBytes, afterEncode.peakResidentBytes);
    if (!encoded) {
        report.status = "refused";
        report.detail = encoded.error().code();
        return;
    }

    started = Clock::now();
    const auto decoded = cuexis::chart::packed::decode(*encoded);
    report.decodeUs = elapsedMicroseconds(started);
    if (!decoded) {
        report.status = "refused";
        report.detail = decoded.error().code();
        return;
    }

    started = Clock::now();
    const auto reencoded = cuexis::chart::packed::encode(*decoded);
    report.reencodeUs = elapsedMicroseconds(started);
    if (!reencoded) {
        report.status = "refused";
        report.detail = reencoded.error().code();
        return;
    }
    if (*reencoded != *encoded) {
        report.status = "not-canonical";
        report.detail = "re-encode differs from the published artifact";
    }
}

void writeProfile(std::ostream& output, const ProfileMeasurements& report) {
    output << "    {\n";
    output << "      \"id\": " << jsonString(report.id) << ",\n";
    output << "      \"inputKind\": " << jsonString(report.inputKind) << ",\n";
    output << "      \"status\": " << jsonString(report.status) << ",\n";
    if (!report.detail.empty()) {
        output << "      \"detail\": " << jsonString(report.detail) << ",\n";
    }
    output << "      \"sourceBytes\": "
           << (report.hasSourceBytes ? std::to_string(report.sourceBytes) : std::string{"null"})
           << ",\n";
    if (report.hasSourceBytes) {
        output << "      \"expansion\": {\"entities\": " << report.expansionEntities
               << ", \"requirements\": " << report.expansionRequirements << ", \"events\": 0},\n";
    } else {
        output << "      \"expansion\": null,\n";
    }
    output << "      \"counts\": {\"entities\": " << report.entityCount
           << ", \"requirements\": " << report.requirementCount
           << ", \"strings\": " << report.stringCount
           << ", \"references\": " << report.referenceCount << "},\n";
    output << "      \"packedBytes\": " << report.packedBytes << ",\n";
    output << "      \"decodedBytes\": " << report.decodedBytes << ",\n";
    output << "      \"headerBytes\": " << report.headerBytes << ",\n";
    output << "      \"directoryBytes\": " << report.directoryBytes << ",\n";
    output << "      \"closure\": {\"assetIds\": " << report.closureAssets
           << ", \"uses\": " << report.closureUses << "},\n";
    output << "      \"elapsedUs\": {\"size\": " << report.sizeUs
           << ", \"encode\": " << report.encodeUs << ", \"decode\": " << report.decodeUs
           << ", \"reencode\": " << report.reencodeUs << "},\n";
    output << "      \"memoryBytes\": {\"residentBefore\": " << report.residentBeforeBytes
           << ", \"peakAfterEncode\": " << report.peakAfterBytes
           << ", \"peakDelta\": " << report.peakDeltaBytes << "},\n";
    output << "      \"sections\": [";
    for (std::size_t index = 0; index < report.sections.size(); ++index) {
        const auto& section = report.sections[index];
        output << (index == 0U ? "\n" : ",\n");
        output << "        {\"type\": " << jsonString(section.type)
               << ", \"encodedBytes\": " << section.encodedBytes
               << ", \"decodedBytes\": " << section.decodedBytes
               << ", \"recordCount\": " << section.recordCount << "}";
    }
    output << (report.sections.empty() ? "]\n" : "\n      ]\n");
    output << "    }";
}

} // namespace

auto main(int argc, char** argv) -> int {
    std::string implementationSha{"unset"};
    for (int index = 1; index + 1 < argc; ++index) {
        if (std::string_view{argv[index]} == "--sha") {
            implementationSha = argv[index + 1];
        }
    }

    const cuexis::chart::PackedChartLimits limits{};
    const auto sourceDirectory = std::filesystem::path{CUEXIS_SOURCE_DIR};
    const auto ladderPath = sourceDirectory / "tests" / "fixtures" / "chart_format_foundation" /
                            "valid" / "pattern_stair.cxt";

    ProfileMeasurements lowReuse;
    lowReuse.id = "low-reuse-v1";
    lowReuse.inputKind = "typed-model";
    measureProfile(lowReuse, lowReuseChart(40000U));

    ProfileMeasurements ladder;
    ladder.id = "cxt-stair-ladder";
    ladder.inputKind = "cxt-v2-source";
    ladder.hasSourceBytes = true;
    {
        const auto source = ladderSource(4U);
        ladder.sourceBytes = source.sourceBytes;
        ladder.expansionEntities = source.entities;
        ladder.expansionRequirements = source.requirements;
        cuexis::chart::CxtV2Invocation invocation;
        invocation.chartId = cuexis::chart::ChartId{"019b0000-0000-7abc-8def-000000000001"};
        invocation.bindingId = "intro-stair";
        invocation.moduleId = "pattern.stair";
        invocation.exportId = "stair";
        invocation.startBeat = beat(16, 1);
        auto expanded = cuexis::chart::CxtV2Loader::expand(source.text, invocation);
        if (!expanded.chart) {
            ladder.status = "refused";
            ladder.detail = expanded.diagnostics.hasErrors()
                                ? expanded.diagnostics.items().front().code()
                                : std::string{"cxt.v2.unknown"};
        } else {
            auto chart = *expanded.chart;
            chart.features.push_back(
                cuexis::chart::CanonicalFeature{"cuexis.gameplay.candidate.lanes4", 1});
            measureProfile(ladder, std::move(chart));
        }
    }

    ProfileMeasurements highReuse;
    highReuse.id = "high-reuse-v1";
    highReuse.inputKind = "cxt-v2-source";
    highReuse.hasSourceBytes = true;
    {
        const auto source = ladderSource(10000U);
        highReuse.sourceBytes = source.sourceBytes;
        highReuse.expansionEntities = source.entities;
        highReuse.expansionRequirements = source.requirements;
        cuexis::chart::CxtV2Invocation invocation;
        invocation.chartId = cuexis::chart::ChartId{"019b0000-0000-7abc-8def-000000000001"};
        invocation.bindingId = "intro-stair";
        invocation.moduleId = "pattern.stair";
        invocation.exportId = "stair";
        invocation.startBeat = beat(16, 1);
        invocation.parameters.push_back(cuexis::chart::CxtV2ParameterBinding{"groups", 10000});
        auto expanded = cuexis::chart::CxtV2Loader::expand(source.text, invocation);
        if (!expanded.chart) {
            highReuse.status = "refused";
            highReuse.detail = expanded.diagnostics.hasErrors()
                                   ? expanded.diagnostics.items().front().code()
                                   : std::string{"cxt.v2.unknown"};
        } else {
            auto chart = *expanded.chart;
            chart.features.push_back(
                cuexis::chart::CanonicalFeature{"cuexis.gameplay.candidate.lanes4", 1});
            measureProfile(highReuse, std::move(chart));
        }
    }

    std::ostringstream json;
    json << "{\n";
    json << "  \"provenance\": {\n";
    json << "    \"probe\": \"cuexis_chart_capacity_probe\",\n";
    json << "    \"implementationSha\": " << jsonString(implementationSha) << ",\n";
    json << "    \"sdkVersion\": " << jsonString(cuexis::version::canonical) << ",\n";
    json << "    \"buildType\": " << jsonString(buildType()) << ",\n";
    json << "    \"compiler\": " << jsonString(compilerIdentity()) << ",\n";
    json << "    \"platform\": " << jsonString(platformIdentity()) << ",\n";
    json << "    \"packedProfile\": {\"candidateRevision\": 1, \"flags\": 1},\n";
    json << "    \"budgets\": {\"maxPackedFileBytes\": " << limits.maxPackedFileBytes
         << ", \"maxPackedEntities\": " << limits.maxPackedEntities
         << ", \"maxPackedSectionBytes\": " << limits.maxPackedSectionBytes
         << ", \"maxPackedDecodedBytes\": " << limits.maxPackedDecodedBytes
         << ", \"maxPackedStrings\": " << limits.maxPackedStrings
         << ", \"maxPackedReferences\": " << limits.maxPackedReferences
         << ", \"maxPackedRequirements\": " << limits.maxPackedRequirements << "},\n";
    json << "    \"fixture\": " << jsonString(ladderPath.string()) << "\n";
    json << "  },\n";
    json << "  \"memoryMethod\": \"Windows: GetProcessMemoryInfo PeakWorkingSetSize; POSIX: "
            "getrusage ru_maxrss. Process-wide, observation only.\",\n";
    json << "  \"timeMethod\": \"std::chrono::steady_clock, one operation per profile\",\n";
    json << "  \"profiles\": [\n";
    writeProfile(json, lowReuse);
    json << ",\n";
    writeProfile(json, ladder);
    json << ",\n";
    writeProfile(json, highReuse);
    json << "\n  ],\n";
    json << "  \"notMeasured\": [\n";
    json << "    {\"field\": \"preparePeakBytes\", \"reason\": \"no prepare implementation in "
            "this stage\"},\n";
    json << "    {\"field\": \"sourceBytes (typed-model profile)\", \"reason\": \"the typed model "
            "is constructed directly; no source bytes apply\"},\n";
    json << "    {\"field\": \"decodedSemanticHeapBytes\", \"reason\": \"decoded section bytes are "
            "not interpreted as C++ heap usage\"},\n";
    json << "    {\"field\": \"idn0DecodedObjectBytes\", \"reason\": \"scope/path tables have no "
            "frozen object-memory budget\"}\n";
    json << "  ]\n";
    json << "}\n";

    std::cout << json.str();
    std::cerr << "chart.capacity low-reuse-v1 packedBytes=" << lowReuse.packedBytes
              << " entities=" << lowReuse.entityCount << " status=" << lowReuse.status << '\n';
    std::cerr << "chart.capacity cxt-stair-ladder packedBytes=" << ladder.packedBytes
              << " entities=" << ladder.entityCount << " sourceBytes=" << ladder.sourceBytes
              << " status=" << ladder.status << '\n';
    std::cerr << "chart.capacity high-reuse-v1 packedBytes=" << highReuse.packedBytes
              << " entities=" << highReuse.entityCount << " status=" << highReuse.status << '\n';
    return 0;
}
