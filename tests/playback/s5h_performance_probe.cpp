#if defined(_WIN32)
#include <windows.h>
#endif

#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>

#include "s5h_shader_fixtures.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::size_t warmupFrames = 64;
constexpr std::size_t hotFrames = 1024;

struct MemorySnapshot final {
    std::uint64_t residentBytes{};
    std::uint64_t peakResidentBytes{};
};

[[nodiscard]] auto memorySnapshot() -> MemorySnapshot {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)) == FALSE) {
        throw std::runtime_error{"GetProcessMemoryInfo failed"};
    }
    return {.residentBytes = static_cast<std::uint64_t>(counters.WorkingSetSize),
            .peakResidentBytes = static_cast<std::uint64_t>(counters.PeakWorkingSetSize)};
#else
    std::ifstream statm{"/proc/self/statm"};
    std::uint64_t totalPages = 0;
    std::uint64_t residentPages = 0;
    statm >> totalPages >> residentPages;
    if (!statm) {
        throw std::runtime_error{"Could not read /proc/self/statm"};
    }
    (void)totalPages;
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        throw std::runtime_error{"getrusage failed"};
    }
    const auto nativePageSize = sysconf(_SC_PAGESIZE);
    if (nativePageSize <= 0) {
        throw std::runtime_error{"sysconf(_SC_PAGESIZE) failed"};
    }
    const auto pageSize = static_cast<std::uint64_t>(nativePageSize);
    return {.residentBytes = residentPages * pageSize,
            .peakResidentBytes = static_cast<std::uint64_t>(usage.ru_maxrss) * 1024ULL};
#endif
}

[[nodiscard]] auto elapsedMicroseconds(Clock::time_point started) -> double {
    return std::chrono::duration<double, std::micro>{Clock::now() - started}.count();
}

[[nodiscard]] auto positiveDelta(std::uint64_t after, std::uint64_t before) -> std::uint64_t {
    return after > before ? after - before : 0;
}

[[nodiscard]] auto readBytes(const std::filesystem::path& path) -> std::vector<std::byte> {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Could not open fixture: " + path.string()};
    }
    std::string text{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    std::vector<std::byte> bytes;
    bytes.reserve(text.size());
    for (const unsigned char value : text) {
        bytes.push_back(static_cast<std::byte>(value));
    }
    return bytes;
}

} // namespace

int main() {
    try {
        cuexis::test_support::s5h::ShaderLayout layout;
        layout.keywordCount = 4;
        layout.parameterCount = 32;
        layout.bindingCount = 16;
        cuexis::test_support::s5h::MaterialLayout material;
        material.parameterCount = 32;
        const auto meshBytes =
            readBytes(std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" /
                      "stage3_project" / "assets" / "meshes" / "triangle.mesh.bin");
        auto source = cuexis::test_support::s5h::makeSource(
            cuexis::test_support::s5h::makeShaderPayload(
                cuexis::test_support::s5h::kVertex, cuexis::test_support::s5h::kFragment, layout),
            meshBytes,
            cuexis::test_support::s5h::makeParameterizedPayload("shader.sprite", material));
        if (!source) {
            throw std::runtime_error{"S5-H probe could not build PlaybackSource"};
        }

        const auto beforePrepare = memorySnapshot();
        const auto prepareStarted = Clock::now();
        cuexis::playback::PlaybackSession session;
        auto loaded = session.load(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
        if (!loaded) {
            throw std::runtime_error{"S5-H probe load failed: " +
                                     std::string{loaded.error().code()}};
        }
        const auto prepareMicroseconds = elapsedMicroseconds(prepareStarted);
        const auto afterPrepare = memorySnapshot();

        cuexis::playback::FrameSnapshot snapshot;
        for (std::size_t index = 0; index < warmupFrames; ++index) {
            const auto updated = session.update(
                {.chartTimeMs = static_cast<double>(index) * 10.0, .simulationDeltaTimeMs = 10.0});
            const auto extracted = session.extractFrame({.width = 1280, .height = 720}, snapshot);
            if (!updated || !extracted) {
                throw std::runtime_error{"S5-H probe warmup failed"};
            }
        }
        const auto afterWarmup = memorySnapshot();

        const auto hotStarted = Clock::now();
        for (std::size_t index = 0; index < hotFrames; ++index) {
            const auto updated =
                session.update({.chartTimeMs = static_cast<double>(warmupFrames + index) * 10.0,
                                .simulationDeltaTimeMs = 10.0});
            const auto extracted = session.extractFrame({.width = 1280, .height = 720}, snapshot);
            if (!updated || !extracted) {
                throw std::runtime_error{"S5-H probe hot frames failed"};
            }
        }
        const auto hotMicroseconds = elapsedMicroseconds(hotStarted);
        const auto afterHot = memorySnapshot();

        std::cout << std::fixed << std::setprecision(3) << "keyword_count=" << layout.keywordCount
                  << '\n'
                  << "parameter_count=" << layout.parameterCount << '\n'
                  << "binding_count=" << layout.bindingCount << '\n'
                  << "prepare_us=" << prepareMicroseconds << '\n'
                  << "warmed_evaluate_avg_us=" << hotMicroseconds / static_cast<double>(hotFrames)
                  << '\n'
                  << "resident_before_prepare_bytes=" << beforePrepare.residentBytes << '\n'
                  << "resident_after_prepare_bytes=" << afterPrepare.residentBytes << '\n'
                  << "peak_after_prepare_bytes=" << afterPrepare.peakResidentBytes << '\n'
                  << "prepare_resident_delta_bytes="
                  << positiveDelta(afterPrepare.residentBytes, beforePrepare.residentBytes) << '\n'
                  << "resident_after_warmup_bytes=" << afterWarmup.residentBytes << '\n'
                  << "peak_after_warmup_bytes=" << afterWarmup.peakResidentBytes << '\n'
                  << "resident_after_hot_frames_bytes=" << afterHot.residentBytes << '\n'
                  << "peak_after_hot_frames_bytes=" << afterHot.peakResidentBytes << '\n'
                  << "hot_peak_delta_bytes="
                  << positiveDelta(afterHot.peakResidentBytes, afterWarmup.peakResidentBytes)
                  << '\n';
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
