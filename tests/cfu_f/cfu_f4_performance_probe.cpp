#if defined(_WIN32)
#include <windows.h>
#endif

#include <cuexis/cxc/cxc_package.hpp>
#include <cuexis/cxc/cxc_writer.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::uint32_t maxTextureWidth = 2'799;
constexpr std::uint32_t maxTextureHeight = 5'994;
constexpr std::size_t maxResourceBytes = 64U * 1024U * 1024U;
constexpr std::size_t warmedFrameIterations = 4'096;

struct MemorySnapshot final {
    std::uint64_t residentBytes{};
    std::uint64_t peakResidentBytes{};
};

[[nodiscard]] auto projectRoot() -> std::filesystem::path {
    return std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" / "chart_format_update" /
           "cfu_f_reference_project";
}

[[nodiscard]] auto readText(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Could not open CFU-F4 fixture text: " + path.string()};
    }
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

[[nodiscard]] auto readBytes(const std::filesystem::path& path) -> std::vector<std::byte> {
    std::ifstream input{path, std::ios::binary | std::ios::ate};
    if (!input) {
        throw std::runtime_error{"Could not open CFU-F4 fixture bytes: " + path.string()};
    }
    const auto length = input.tellg();
    if (length < 0) {
        throw std::runtime_error{"Could not size CFU-F4 fixture: " + path.string()};
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), length);
    }
    if (!input) {
        throw std::runtime_error{"Could not read CFU-F4 fixture: " + path.string()};
    }
    return bytes;
}

[[nodiscard]] auto textBytes(std::string_view text) -> std::vector<std::byte> {
    std::vector<std::byte> bytes;
    bytes.reserve(text.size());
    for (const char character : text) {
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
    }
    return bytes;
}

void writeU32(std::span<std::byte> bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        bytes[offset + index] = static_cast<std::byte>((value >> (index * 8U)) & 0xFFU);
    }
}

void writeU64(std::span<std::byte> bytes, std::size_t offset, std::uint64_t value) {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        bytes[offset + index] = static_cast<std::byte>((value >> (index * 8U)) & 0xFFU);
    }
}

[[nodiscard]] auto maximumTexturePayload(std::byte fill) -> std::vector<std::byte> {
    std::vector<std::byte> bytes(maxResourceBytes, fill);
    constexpr std::array magic{'C', 'X', 'P', 'R', 'E', 'S', '0', '1'};
    for (std::size_t index = 0; index < magic.size(); ++index) {
        bytes[index] = static_cast<std::byte>(magic[index]);
    }
    writeU32(bytes, 8, 2);
    writeU32(bytes, 12, 1);
    writeU64(bytes, 16, bytes.size());
    writeU32(bytes, 24, maxTextureWidth);
    writeU32(bytes, 28, maxTextureHeight);
    writeU32(bytes, 32, 2);
    writeU32(bytes, 36, 0);
    return bytes;
}

[[nodiscard]] auto makeWriteRequest() -> cuexis::cxc::CxcWriteRequest {
    const auto root = projectRoot();
    cuexis::cxc::CxcWriteRequest request;
    request.entries = {
        {"cuexis.project.json", textBytes(readText(root / "cuexis.project.json"))},
        {"assets/cuexis.asset-index.json",
         textBytes(readText(root / "assets" / "cuexis.asset-index.json"))},
        {"assets/charts/main.cuexis.chart.json",
         textBytes(readText(root / "assets" / "charts" / "main.cuexis.chart.json"))},
        {"assets/materials/blend.material.bin",
         readBytes(root / "assets" / "materials" / "blend.material.bin")},
        {"assets/materials/opaque.material.bin",
         readBytes(root / "assets" / "materials" / "opaque.material.bin")},
        {"assets/meshes/triangle.mesh.bin",
         readBytes(root / "assets" / "meshes" / "triangle.mesh.bin")},
        {"assets/textures/checker.texture.bin", maximumTexturePayload(std::byte{0x55})},
    };
    return request;
}

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

[[nodiscard]] auto throughputMiBPerSecond(std::size_t bytes, double microseconds) -> double {
    const auto seconds = std::max(microseconds, 1.0) / 1'000'000.0;
    return (static_cast<double>(bytes) / (1024.0 * 1024.0)) / seconds;
}

[[nodiscard]] auto positiveDelta(std::uint64_t after, std::uint64_t before) -> std::uint64_t {
    return after > before ? after - before : 0;
}

[[noreturn]] void fail(std::string_view operation, const cuexis::core::Error& error) {
    throw std::runtime_error{std::string{operation} + " failed: " + std::string{error.code()}};
}

} // namespace

int main() {
    try {
        auto request = makeWriteRequest();
        std::size_t contentBytes = 0;
        for (const auto& entry : request.entries) {
            contentBytes += entry.bytes.size();
        }

        const auto beforeWriteMemory = memorySnapshot();
        const auto writeStarted = Clock::now();
        auto written = cuexis::cxc::CxcWriter::write(std::move(request));
        const auto writeMicroseconds = elapsedMicroseconds(writeStarted);
        if (!written.hasValue()) {
            throw std::runtime_error{"CXC maximum-content write failed"};
        }
        const auto afterWriteMemory = memorySnapshot();
        const auto packageBytes = written.bytes->size();

        const auto loadStarted = Clock::now();
        auto loaded = cuexis::cxc::CxcPackageLoader::loadMemory(
            std::span<const std::byte>{written.bytes->data(), written.bytes->size()});
        const auto loadMicroseconds = elapsedMicroseconds(loadStarted);
        if (!loaded.hasValue()) {
            throw std::runtime_error{"CXC maximum-content reload failed"};
        }
        const auto afterLoadMemory = memorySnapshot();
        loaded.package.reset();

        auto sourceBytes = *written.bytes;
        const auto sourceStarted = Clock::now();
        auto source = cuexis::playback::PlaybackSource::fromCxcMemory(std::move(sourceBytes));
        const auto sourceMicroseconds = elapsedMicroseconds(sourceStarted);
        if (!source) {
            fail("Playback CXC source", source.error());
        }

        cuexis::playback::PlaybackSession session;
        const auto prepareStarted = Clock::now();
        auto prepared =
            session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
        const auto prepareMicroseconds = elapsedMicroseconds(prepareStarted);
        if (!prepared) {
            fail("Playback prepare", prepared.error());
        }
        const auto afterPrepareMemory = memorySnapshot();
        auto committed = session.commit(std::move(*prepared));
        if (!committed) {
            fail("Playback commit", committed.error());
        }

        cuexis::playback::FrameSnapshot snapshot;
        auto updated = session.update({.chartTimeMs = 625.0});
        if (!updated) {
            fail("Playback warm update", updated.error());
        }
        auto extracted = session.extractFrame({.width = 1280, .height = 720}, snapshot);
        if (!extracted) {
            fail("Playback warm extract", extracted.error());
        }

        const auto frameStarted = Clock::now();
        for (std::size_t index = 0; index < warmedFrameIterations; ++index) {
            updated = session.update({.chartTimeMs = index % 2 == 0 ? 0.0 : 625.0,
                                      .simulationDeltaTimeMs = 0.0,
                                      .timeDiscontinuityId = index + 1U});
            if (!updated) {
                fail("Playback warmed update", updated.error());
            }
            extracted = session.extractFrame({.width = 1280, .height = 720}, snapshot);
            if (!extracted) {
                fail("Playback warmed extract", extracted.error());
            }
        }
        const auto frameMicroseconds = elapsedMicroseconds(frameStarted);

        const auto beforeReloadMemory = memorySnapshot();
        auto reloadBytes = *written.bytes;
        const auto reloadSourceStarted = Clock::now();
        auto reloadSource = cuexis::playback::PlaybackSource::fromCxcMemory(std::move(reloadBytes));
        const auto reloadSourceMicroseconds = elapsedMicroseconds(reloadSourceStarted);
        if (!reloadSource) {
            fail("Playback reload CXC source", reloadSource.error());
        }
        const auto reloadPrepareStarted = Clock::now();
        auto replacement = session.prepareReload(std::move(*reloadSource), {.chartTimeMs = 625.0},
                                                 cuexis::playback::ReloadPolicy::KeepChartTime);
        const auto reloadPrepareMicroseconds = elapsedMicroseconds(reloadPrepareStarted);
        if (!replacement) {
            fail("Playback reload prepare", replacement.error());
        }
        const auto afterReloadMemory = memorySnapshot();

        std::cout << std::fixed << std::setprecision(3) << "content_bytes=" << contentBytes << '\n'
                  << "package_bytes=" << packageBytes << '\n'
                  << "cxc_write_us=" << writeMicroseconds << '\n'
                  << "cxc_write_mib_per_s="
                  << throughputMiBPerSecond(contentBytes, writeMicroseconds) << '\n'
                  << "cxc_hash_load_us=" << loadMicroseconds << '\n'
                  << "cxc_hash_load_mib_per_s="
                  << throughputMiBPerSecond(packageBytes, loadMicroseconds) << '\n'
                  << "cxc_source_factory_us=" << sourceMicroseconds << '\n'
                  << "prepare_us=" << prepareMicroseconds << '\n'
                  << "warmed_update_extract_avg_us="
                  << frameMicroseconds / static_cast<double>(warmedFrameIterations) << '\n'
                  << "reload_source_factory_us=" << reloadSourceMicroseconds << '\n'
                  << "reload_prepare_us=" << reloadPrepareMicroseconds << '\n'
                  << "resident_before_write_bytes=" << beforeWriteMemory.residentBytes << '\n'
                  << "resident_after_write_bytes=" << afterWriteMemory.residentBytes << '\n'
                  << "peak_after_write_bytes=" << afterWriteMemory.peakResidentBytes << '\n'
                  << "write_peak_delta_bytes="
                  << positiveDelta(afterWriteMemory.peakResidentBytes,
                                   beforeWriteMemory.peakResidentBytes)
                  << '\n'
                  << "resident_after_hash_load_bytes=" << afterLoadMemory.residentBytes << '\n'
                  << "peak_after_hash_load_bytes=" << afterLoadMemory.peakResidentBytes << '\n'
                  << "resident_after_prepare_bytes=" << afterPrepareMemory.residentBytes << '\n'
                  << "peak_after_prepare_bytes=" << afterPrepareMemory.peakResidentBytes << '\n'
                  << "resident_before_reload_bytes=" << beforeReloadMemory.residentBytes << '\n'
                  << "resident_after_reload_bytes=" << afterReloadMemory.residentBytes << '\n'
                  << "peak_after_reload_bytes=" << afterReloadMemory.peakResidentBytes << '\n'
                  << "reload_resident_delta_bytes="
                  << positiveDelta(afterReloadMemory.residentBytes,
                                   beforeReloadMemory.residentBytes)
                  << '\n'
                  << "reload_peak_delta_bytes="
                  << positiveDelta(afterReloadMemory.peakResidentBytes,
                                   beforeReloadMemory.peakResidentBytes)
                  << '\n';
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
