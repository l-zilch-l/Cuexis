#if defined(_WIN32)
#include <windows.h>
#endif

#include <cuexis/content/content_provider.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>

#include "../presentation/validation_sink.hpp"

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
#include <memory>
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
using cuexis::playback::PlaybackAssetDescriptor;
using cuexis::playback::PlaybackAssetType;

constexpr std::uint32_t maxTextureWidth = 2'799;
constexpr std::uint32_t maxTextureHeight = 5'994;
constexpr std::uint64_t maxResourceBytes = 64ULL * 1024ULL * 1024ULL;
constexpr std::size_t warmIterationCount = 4'096;

struct MemorySnapshot final {
    std::uint64_t residentBytes{};
    std::uint64_t peakResidentBytes{};
};

[[nodiscard]] auto fixtureRoot() -> std::filesystem::path {
    return std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" / "stage3_project" /
           "assets";
}

[[nodiscard]] auto readText(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Could not open fixture text: " + path.string()};
    }
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

[[nodiscard]] auto readBytes(const std::filesystem::path& path) -> std::vector<std::byte> {
    const auto text = readText(path);
    std::vector<std::byte> bytes;
    bytes.reserve(text.size());
    for (const unsigned char value : text) {
        bytes.push_back(static_cast<std::byte>(value));
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

[[nodiscard]] auto descriptors() -> std::vector<PlaybackAssetDescriptor> {
    return {
        {.id = "texture.checker",
         .type = PlaybackAssetType::Texture,
         .rootId = "main",
         .logicalSource = "textures/checker.texture.bin",
         .dependencies = {}},
        {.id = "material.blend",
         .type = PlaybackAssetType::Material,
         .rootId = "main",
         .logicalSource = "materials/blend.material.bin",
         .dependencies = {"texture.checker"}},
        {.id = "mesh.triangle",
         .type = PlaybackAssetType::Mesh,
         .rootId = "main",
         .logicalSource = "meshes/triangle.mesh.bin",
         .dependencies = {}},
        {.id = "material.opaque",
         .type = PlaybackAssetType::Material,
         .rootId = "main",
         .logicalSource = "materials/opaque.material.bin",
         .dependencies = {}},
    };
}

[[nodiscard]] auto makeSource(std::byte textureFill)
    -> cuexis::core::Result<cuexis::playback::PlaybackSource> {
    const auto root = fixtureRoot();
    std::vector<cuexis::content::MemoryContentEntry> entries;
    entries.reserve(4);
    entries.push_back({.rootId = "main",
                       .source = "textures/checker.texture.bin",
                       .bytes = maximumTexturePayload(textureFill),
                       .revision = 1});
    entries.push_back({.rootId = "main",
                       .source = "materials/blend.material.bin",
                       .bytes = readBytes(root / "materials" / "blend.material.bin"),
                       .revision = 1});
    entries.push_back({.rootId = "main",
                       .source = "meshes/triangle.mesh.bin",
                       .bytes = readBytes(root / "meshes" / "triangle.mesh.bin"),
                       .revision = 1});
    entries.push_back({.rootId = "main",
                       .source = "materials/opaque.material.bin",
                       .bytes = readBytes(root / "materials" / "opaque.material.bin"),
                       .revision = 1});

    auto provider = cuexis::content::MemoryContentProvider::create(std::move(entries));
    if (!provider) {
        return cuexis::core::unexpected(std::move(provider.error()));
    }
    cuexis::playback::TypedPlaybackProject project{
        .sourceId = "stage3-performance-probe",
        .chartJson = readText(root / "charts" / "stage3_example.cuexis.chart.json"),
        .assets = descriptors(),
    };
    return cuexis::playback::PlaybackSource::fromTypedProject(std::move(project),
                                                              std::move(*provider));
}

[[nodiscard]] auto capabilities() -> cuexis::playback::PresentationCapabilities {
    return {
        .opaquePass = true,
        .transparentPass = true,
        .linearTexture = true,
        .srgbTexture = true,
        .straightAlphaBlend = true,
        .backFaceCulling = true,
        .doubleSided = true,
        .debugPass = true,
        .maxResourceBytes = maxResourceBytes,
        .maxTotalDecodedBytes = 512ULL * 1024ULL * 1024ULL,
        .maxTextureDimension = 8'192,
        .maxMeshVertices = 1'048'576,
        .maxMeshIndices = 3'145'728,
    };
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
    const auto pageSize = static_cast<std::uint64_t>(sysconf(_SC_PAGESIZE));
    return {.residentBytes = residentPages * pageSize,
            .peakResidentBytes = static_cast<std::uint64_t>(usage.ru_maxrss) * 1024ULL};
#endif
}

template <typename Duration = std::chrono::duration<double, std::micro>>
[[nodiscard]] auto elapsed(Clock::time_point started) -> double {
    return std::chrono::duration_cast<Duration>(Clock::now() - started).count();
}

[[noreturn]] void fail(std::string_view operation, const cuexis::core::Error& error) {
    throw std::runtime_error{std::string{operation} + " failed: " + std::string{error.code()}};
}

} // namespace

int main() {
    try {
        auto source = makeSource(std::byte{0x55});
        if (!source) {
            fail("source creation", source.error());
        }

        cuexis::playback::PlaybackSession session;
        const auto prepareStarted = Clock::now();
        auto prepared =
            session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
        const double prepareMicroseconds = elapsed(prepareStarted);
        if (!prepared) {
            fail("prepareLoad", prepared.error());
        }
        const auto afterPrepareMemory = memorySnapshot();

        const auto acquireStarted = Clock::now();
        const auto* candidateManifest = prepared->presentationManifest();
        if (candidateManifest == nullptr) {
            throw std::runtime_error{"Candidate manifest is missing"};
        }
        auto manifestCopy = *candidateManifest;
        std::vector<cuexis::playback::PortableResourcePtr> acquiredResources;
        acquiredResources.reserve(manifestCopy.entries.size());
        for (const auto& entry : manifestCopy.entries) {
            auto resource = prepared->acquirePresentationResource(entry.reference);
            if (!resource) {
                fail("candidate acquisition", resource.error());
            }
            acquiredResources.push_back(std::move(*resource));
        }
        const double acquireMicroseconds = elapsed(acquireStarted);

        const auto validationPrepareStarted = Clock::now();
        auto validationCandidate = cuexis::test_support::prepareValidationCandidate(
            *prepared, capabilities(), {.enableDebugPass = true});
        const double validationPrepareMicroseconds = elapsed(validationPrepareStarted);
        if (!validationCandidate.hasValue()) {
            throw std::runtime_error{"Validation candidate preparation failed"};
        }
        auto committed = session.commit(std::move(*prepared));
        if (!committed) {
            fail("commit", committed.error());
        }
        cuexis::test_support::ValidationSink sink;
        sink.activate(std::move(*validationCandidate.candidate));

        cuexis::playback::FrameSnapshot snapshot;
        cuexis::test_support::ValidationSummary summary;
        auto updated = session.update({.chartTimeMs = 625.0});
        if (!updated) {
            fail("warm update", updated.error());
        }
        auto extracted = session.extractFrame({.width = 1280, .height = 720}, snapshot);
        if (!extracted) {
            fail("warm extract", extracted.error());
        }
        auto validated = sink.validateFrame(snapshot, summary);
        if (!validated) {
            fail("warm validate", validated.error());
        }

        const auto frameStarted = Clock::now();
        for (std::size_t index = 0; index < warmIterationCount; ++index) {
            const double chartTimeMs = index % 2 == 0 ? 0.0 : 625.0;
            updated = session.update({.chartTimeMs = chartTimeMs,
                                      .simulationDeltaTimeMs = 0.0,
                                      .timeDiscontinuityId = index + 1});
            if (!updated) {
                fail("warmed update", updated.error());
            }
            extracted = session.extractFrame({.width = 1280, .height = 720}, snapshot);
            if (!extracted) {
                fail("warmed extract", extracted.error());
            }
        }
        const double frameMicroseconds = elapsed(frameStarted);

        const auto validationStarted = Clock::now();
        for (std::size_t index = 0; index < warmIterationCount; ++index) {
            validated = sink.validateFrame(snapshot, summary);
            if (!validated) {
                fail("warmed validation", validated.error());
            }
        }
        const double validationMicroseconds = elapsed(validationStarted);

        auto replacementSource = makeSource(std::byte{0xAA});
        if (!replacementSource) {
            fail("replacement source creation", replacementSource.error());
        }
        const auto beforeReloadMemory = memorySnapshot();
        const auto reloadStarted = Clock::now();
        auto replacement =
            session.prepareReload(std::move(*replacementSource), {.chartTimeMs = 625.0},
                                  cuexis::playback::ReloadPolicy::KeepChartTime);
        const double reloadPrepareMicroseconds = elapsed(reloadStarted);
        if (!replacement) {
            fail("prepareReload", replacement.error());
        }
        const auto afterReloadMemory = memorySnapshot();

        const auto textureEntry = std::find_if(
            manifestCopy.entries.begin(), manifestCopy.entries.end(),
            [](const auto& entry) { return entry.reference.assetId == "texture.checker"; });
        if (textureEntry == manifestCopy.entries.end()) {
            throw std::runtime_error{"Maximum texture manifest entry is missing"};
        }

        const auto reloadPeakDelta =
            afterReloadMemory.peakResidentBytes > beforeReloadMemory.peakResidentBytes
                ? afterReloadMemory.peakResidentBytes - beforeReloadMemory.peakResidentBytes
                : 0;
        std::cout << std::fixed << std::setprecision(3)
                  << "max_texture_encoded_bytes=" << textureEntry->encodedByteCount << '\n'
                  << "max_texture_decoded_bytes=" << textureEntry->decodedByteCount << '\n'
                  << "prepare_us=" << prepareMicroseconds << '\n'
                  << "manifest_acquire_us=" << acquireMicroseconds << '\n'
                  << "validation_candidate_us=" << validationPrepareMicroseconds << '\n'
                  << "warmed_update_extract_avg_us="
                  << frameMicroseconds / static_cast<double>(warmIterationCount) << '\n'
                  << "warmed_validate_avg_us="
                  << validationMicroseconds / static_cast<double>(warmIterationCount) << '\n'
                  << "reload_prepare_us=" << reloadPrepareMicroseconds << '\n'
                  << "resident_after_prepare_bytes=" << afterPrepareMemory.residentBytes << '\n'
                  << "peak_after_prepare_bytes=" << afterPrepareMemory.peakResidentBytes << '\n'
                  << "resident_before_reload_bytes=" << beforeReloadMemory.residentBytes << '\n'
                  << "resident_after_reload_bytes=" << afterReloadMemory.residentBytes << '\n'
                  << "peak_after_reload_bytes=" << afterReloadMemory.peakResidentBytes << '\n'
                  << "reload_peak_delta_bytes=" << reloadPeakDelta << '\n';
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
