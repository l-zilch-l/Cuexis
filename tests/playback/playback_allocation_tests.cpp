#include <cuexis/content/content_provider.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>

#include "../presentation/validation_sink.hpp"
#include "presentation_extraction.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif
#include <iostream>
#include <limits>
#if defined(_WIN32)
#include <malloc.h>
#endif
#include <new>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::atomic_size_t allocationCount{};
std::atomic_bool trackingAllocations{};

#if defined(_MSC_VER) && defined(_DEBUG)
_CRT_ALLOC_HOOK previousAllocationHook{};

int allocationHook(int allocationType, void* data, std::size_t size, int blockType,
                   long requestNumber, const unsigned char* fileName, int lineNumber) {
    if (previousAllocationHook != nullptr &&
        previousAllocationHook(allocationType, data, size, blockType, requestNumber, fileName,
                               lineNumber) == 0) {
        return 0;
    }
    if (allocationType == _HOOK_ALLOC && trackingAllocations.load(std::memory_order_relaxed)) {
        allocationCount.fetch_add(1, std::memory_order_relaxed);
    }
    return 1;
}
#endif

void beginAllocationTracking() {
    allocationCount.store(0, std::memory_order_relaxed);
#if defined(_MSC_VER) && defined(_DEBUG)
    previousAllocationHook = _CrtSetAllocHook(allocationHook);
#endif
    trackingAllocations.store(true, std::memory_order_relaxed);
}

[[nodiscard]] auto endAllocationTracking() -> std::size_t {
    trackingAllocations.store(false, std::memory_order_relaxed);
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetAllocHook(previousAllocationHook);
    previousAllocationHook = nullptr;
#endif
    return allocationCount.load(std::memory_order_relaxed);
}

#if !defined(_MSC_VER) || !defined(_DEBUG)
[[nodiscard]] auto allocate(std::size_t size) -> void* {
    if (trackingAllocations.load(std::memory_order_relaxed)) {
        allocationCount.fetch_add(1, std::memory_order_relaxed);
    }
    void* memory = std::malloc(size == 0 ? 1 : size);
    if (memory != nullptr) {
        return memory;
    }
    throw std::bad_alloc{};
}

[[nodiscard]] auto allocateAligned(std::size_t size, std::size_t alignment) -> void* {
    if (trackingAllocations.load(std::memory_order_relaxed)) {
        allocationCount.fetch_add(1, std::memory_order_relaxed);
    }
#if defined(_WIN32)
    void* memory = _aligned_malloc(size == 0 ? 1 : size, alignment);
#else
    const std::size_t requested = size == 0 ? alignment : size;
    const std::size_t remainder = requested % alignment;
    if (remainder != 0 && requested > std::numeric_limits<std::size_t>::max() - alignment + 1) {
        throw std::bad_alloc{};
    }
    const std::size_t adjusted = remainder == 0 ? requested : requested + alignment - remainder;
    void* memory = std::aligned_alloc(alignment, adjusted);
#endif
    if (memory != nullptr) {
        return memory;
    }
    throw std::bad_alloc{};
}
#endif

[[nodiscard]] auto emptyChart(std::uint32_t version) -> std::string {
    std::ostringstream output;
    output << R"({"format":"cuexis.chart","version":)" << version
           << R"(,"chartId":"01a00000-0000-7abc-8def-000000000f4)" << version
           << R"(","metadata":{},"timing":{"offsetMs":0,"defaultBpm":120,)"
           << (version <= 2 ? "\"bpmChanges\":[]" : "\"tempoEvents\":[]")
           << R"(,"stops":[]},"camera":{"type":"perspective","fovY":60,"near":0.1,"far":1000},)";
    if (version == 4) {
        output << R"("parameters":[],)";
    }
    output << R"("templates":[],"behaviors":[],)";
    if (version == 4) {
        output << R"("animationTemplateImports":[],"animationClips":[],)";
    }
    output << R"("objects":[],"requiredExtensions":[],"extensions":{}})";
    return output.str();
}

} // namespace

#if !defined(_MSC_VER) || !defined(_DEBUG)
void* operator new(std::size_t size) {
    return allocate(size);
}

void* operator new[](std::size_t size) {
    return allocate(size);
}

void* operator new(std::size_t size, std::align_val_t alignment) {
    return allocateAligned(size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    return allocateAligned(size, static_cast<std::size_t>(alignment));
}

void deallocate(void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory) noexcept {
    deallocate(memory);
}

void operator delete[](void* memory) noexcept {
    deallocate(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    deallocate(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
    deallocate(memory);
}

void operator delete(void* memory, std::align_val_t) noexcept {
#if defined(_WIN32)
    _aligned_free(memory);
#else
    std::free(memory);
#endif
}

void operator delete[](void* memory, std::align_val_t alignment) noexcept {
    operator delete(memory, alignment);
}

void operator delete(void* memory, std::size_t, std::align_val_t alignment) noexcept {
    operator delete(memory, alignment);
}

void operator delete[](void* memory, std::size_t, std::align_val_t alignment) noexcept {
    operator delete(memory, alignment);
}
#endif

TEST_CASE("Stage 2 playback update and reusable extraction allocate nothing after warmup",
          "[playback][stage2][allocation]") {
    constexpr std::string_view firstMaterial =
        "material.stage2.long_identifier_crossing_small_string_boundary_alpha";
    constexpr std::string_view secondMaterial =
        "material.stage2.long_identifier_crossing_small_string_boundary_beta";
    constexpr std::string_view chart = R"json(
{
  "format":"cuexis.chart","version":3,
  "chartId":"019c0000-0000-7abc-8def-000000000301","metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"tempoEvents":[],"stops":[]},
  "camera":{"type":"perspective","fovY":60,"near":0.1,"far":1000},
  "templates":[],
  "behaviors":[{
    "id":"material.steps","type":"behavior.event","version":1,"events":[],
    "stepEvents":[
      {"property":"render.material","beat":{"numerator":1,"denominator":2},
       "value":{"domain":"asset","id":"material.stage2.long_identifier_crossing_small_string_boundary_alpha"}},
      {"property":"render.material","beat":{"numerator":1,"denominator":1},
       "value":{"domain":"asset","id":"material.stage2.long_identifier_crossing_small_string_boundary_beta"}}
    ]
  }],
  "objects":[{
    "id":"019c0000-0000-7abc-8def-000000000310","parent":null,
    "components":{
      "cuexis.transform":{"version":1,"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]},
      "cuexis.renderable":{"version":1,
        "mesh":{"domain":"asset","id":"mesh.stage2.allocation"},
        "material":{"domain":"asset","id":"material.stage2.baseline"}},
      "cuexis.behavior":{"version":1,"behavior":{"domain":"behavior","id":"material.steps"}}
    },"extensions":{}
  }],
  "requiredExtensions":[],"extensions":{}
}
)json";
    auto provider = cuexis::content::HostContentProvider::create(
        [](const cuexis::content::ContentRequest&)
            -> cuexis::core::Result<cuexis::content::ContentBlob> {
            return cuexis::content::ContentBlob{.bytes = {std::byte{0x42}}, .revision = 1};
        });
    REQUIRE(provider.has_value());
    auto source = cuexis::playback::PlaybackSource::fromTypedProject(
        {.sourceId = "stage2-allocation",
         .chartJson = std::string{chart},
         .assets = {{.id = "mesh.stage2.allocation",
                     .type = cuexis::playback::PlaybackAssetType::Mesh,
                     .rootId = "memory",
                     .logicalSource = "mesh.bin"},
                    {.id = "material.stage2.baseline",
                     .type = cuexis::playback::PlaybackAssetType::Material,
                     .rootId = "memory",
                     .logicalSource = "baseline.material.bin"},
                    {.id = std::string{firstMaterial},
                     .type = cuexis::playback::PlaybackAssetType::Material,
                     .rootId = "memory",
                     .logicalSource = "first.material.bin"},
                    {.id = std::string{secondMaterial},
                     .type = cuexis::playback::PlaybackAssetType::Material,
                     .rootId = "memory",
                     .logicalSource = "second.material.bin"}}},
        std::move(*provider));
    REQUIRE(source.has_value());
    cuexis::playback::PlaybackSession session;
    REQUIRE(
        session.load(std::move(*source), cuexis::playback::PlaybackMode::ChartClock).has_value());
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    cuexis::playback::FrameSnapshot snapshot;
    REQUIRE(session.extractFrame({.width = 1280, .height = 720}, snapshot).has_value());

    beginAllocationTracking();
    bool succeeded = true;
    for (std::size_t index = 1; index <= 64; ++index) {
        const double chartTimeMs = static_cast<double>(index) * 10.0;
        const auto updated =
            session.update({.chartTimeMs = chartTimeMs, .simulationDeltaTimeMs = 10.0});
        const auto extracted = session.extractFrame({.width = 1280, .height = 720}, snapshot);
        if (!updated || !extracted) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = endAllocationTracking();

    std::cout << "FrameSnapshot bytes=" << sizeof(cuexis::playback::FrameSnapshot)
              << " ObjectSnapshot bytes=" << sizeof(cuexis::playback::FrameSnapshot::ObjectSnapshot)
              << '\n';
    CHECK(succeeded);
    CHECK(allocations == 0);
    REQUIRE(snapshot.objects.size() == 1);
    CHECK(snapshot.objects[0].materialAssetId == secondMaterial);
}

TEST_CASE("Stage 3 portable frame extraction and normalization allocate nothing after warmup",
          "[playback][stage3][allocation]") {
    const auto root =
        std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" / "stage3_project";
    auto source = cuexis::playback::PlaybackSource::fromFilesystemProject(root);
    REQUIRE(source.has_value());
    cuexis::playback::PlaybackSession session;
    REQUIRE(
        session.load(std::move(*source), cuexis::playback::PlaybackMode::ChartClock).has_value());
    auto manifest = session.presentationManifest();
    REQUIRE(manifest.has_value());
    std::vector<cuexis::playback::PortableResourcePtr> resources;
    resources.reserve(manifest->entries.size());
    for (const auto& entry : manifest->entries) {
        auto resource = session.acquirePresentationResource(entry.reference);
        REQUIRE(resource.has_value());
        resources.push_back(std::move(*resource));
    }

    cuexis::playback::FrameSnapshot snapshot;
    cuexis::playback::detail::NormalizedPresentationFrame normalized;
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    REQUIRE(session.extractFrame({.width = 1280, .height = 720}, snapshot).has_value());
    REQUIRE(cuexis::playback::detail::normalizePresentationFrame(snapshot, *manifest, resources,
                                                                 normalized)
                .has_value());

    beginAllocationTracking();
    bool succeeded = true;
    for (std::size_t index = 1; index <= 64; ++index) {
        const double chartTimeMs = index % 2 == 0 ? 0.0 : 500.0;
        const auto updated = session.update({.chartTimeMs = chartTimeMs,
                                             .simulationDeltaTimeMs = 0.0,
                                             .timeDiscontinuityId = index});
        const auto extracted = session.extractFrame({.width = 1280, .height = 720}, snapshot);
        const auto presented = cuexis::playback::detail::normalizePresentationFrame(
            snapshot, *manifest, resources, normalized);
        if (!updated || !extracted || !presented) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = endAllocationTracking();

    CHECK(succeeded);
    CHECK(allocations == 0);
    REQUIRE(snapshot.objects.size() == 2);
    CHECK(snapshot.objects[0].materialAssetId == "material.opaque");
    REQUIRE(snapshot.objects[0].mesh.has_value());
    REQUIRE(snapshot.objects[0].material.has_value());
    REQUIRE(normalized.opaque.size() == 1);
    CHECK(normalized.transparent.empty());
}

TEST_CASE("Stage 3 Validation Sink repeated frame validation allocates nothing after warmup",
          "[playback][stage3][validation][allocation]") {
    const auto root =
        std::filesystem::path{CUEXIS_SOURCE_DIR} / "assets" / "projects" / "stage3_project";
    auto source = cuexis::playback::PlaybackSource::fromFilesystemProject(root);
    REQUIRE(source.has_value());
    cuexis::playback::PlaybackSession session;
    auto prepared =
        session.prepareLoad(std::move(*source), cuexis::playback::PlaybackMode::ChartClock);
    REQUIRE(prepared.has_value());

    const cuexis::playback::PresentationCapabilities capabilities{
        .opaquePass = true,
        .transparentPass = true,
        .linearTexture = true,
        .srgbTexture = true,
        .straightAlphaBlend = true,
        .backFaceCulling = true,
        .doubleSided = true,
        .debugPass = true,
        .maxResourceBytes = 64ULL * 1024ULL * 1024ULL,
        .maxTotalDecodedBytes = 512ULL * 1024ULL * 1024ULL,
        .maxTextureDimension = 8192,
        .maxMeshVertices = 1'048'576,
        .maxMeshIndices = 3'145'728,
    };
    auto validationCandidate = cuexis::test_support::prepareValidationCandidate(
        *prepared, capabilities, {.enableDebugPass = true});
    REQUIRE(validationCandidate.hasValue());
    REQUIRE(session.commit(std::move(*prepared)).has_value());
    cuexis::test_support::ValidationSink sink;
    sink.activate(std::move(*validationCandidate.candidate));

    REQUIRE(session.update({.chartTimeMs = 625.0}).has_value());
    cuexis::playback::FrameSnapshot snapshot;
    REQUIRE(session.extractFrame({.width = 1280, .height = 720}, snapshot).has_value());
    cuexis::test_support::ValidationSummary summary;
    REQUIRE(sink.validateFrame(snapshot, summary).has_value());
    REQUIRE(sink.validateFrame(snapshot, summary).has_value());

    beginAllocationTracking();
    bool succeeded = true;
    for (std::size_t index = 0; index < 128; ++index) {
        if (!sink.validateFrame(snapshot, summary)) {
            succeeded = false;
            break;
        }
    }
    const auto allocations = endAllocationTracking();

    CHECK(succeeded);
    CHECK(allocations == 0);
    CHECK(summary.opaque.empty());
    REQUIRE(summary.transparent.size() == 1);
    CHECK(summary.digest != 0);
}

TEST_CASE("CFU-F4 warmed empty Chart v1 through v4 update and extraction allocate nothing",
          "[playback][cfu-f4][allocation][v4]") {
    for (std::uint32_t version = 1; version <= 4; ++version) {
        INFO("Chart version " << version);
        auto source = cuexis::playback::PlaybackSource::fromChartText(emptyChart(version));
        REQUIRE(source.has_value());
        cuexis::playback::PlaybackSession session;
        REQUIRE(session.load(std::move(*source), cuexis::playback::PlaybackMode::ChartClock)
                    .has_value());

        cuexis::playback::FrameSnapshot snapshot;
        REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
        REQUIRE(session.extractFrame({.width = 640, .height = 480}, snapshot).has_value());
        REQUIRE(snapshot.objects.empty());

        beginAllocationTracking();
        bool succeeded = true;
        for (std::size_t index = 1; index <= 128; ++index) {
            const auto updated = session.update({.chartTimeMs = static_cast<double>(index),
                                                 .simulationDeltaTimeMs = 0.0,
                                                 .timeDiscontinuityId = index});
            const auto extracted = session.extractFrame({.width = 640, .height = 480}, snapshot);
            if (!updated || !extracted) {
                succeeded = false;
                break;
            }
        }
        const auto allocations = endAllocationTracking();

        CHECK(succeeded);
        CHECK(allocations == 0);
        CHECK(snapshot.objects.empty());
    }
}

TEST_CASE("S4-G warmed nonempty CXT update and extract stay within the bounded allocation contract",
          "[playback][s4-g][allocation][animation]") {
    const auto root = std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" /
                      "chart_format_update" / "source_project";
    auto source = cuexis::playback::PlaybackSource::fromFilesystemProject(root);
    REQUIRE(source.has_value());
    cuexis::playback::PlaybackSession session;
    REQUIRE(
        session.load(std::move(*source), cuexis::playback::PlaybackMode::ChartClock).has_value());
    cuexis::playback::FrameSnapshot snapshot;
    REQUIRE(session.update({.chartTimeMs = 0.0}).has_value());
    REQUIRE(session.extractFrame({.width = 1280, .height = 720}, snapshot).has_value());
    for (std::size_t index = 1; index <= 32; ++index) {
        REQUIRE(session
                    .update({.chartTimeMs = static_cast<double>(index) * 10.0,
                             .simulationDeltaTimeMs = 10.0})
                    .has_value());
        REQUIRE(session.extractFrame({.width = 1280, .height = 720}, snapshot).has_value());
    }

    auto runWindow = [&](std::size_t start, std::size_t count) -> std::size_t {
        beginAllocationTracking();
        bool succeeded = true;
        for (std::size_t index = 0; index < count; ++index) {
            const double chartTimeMs = static_cast<double>(start + index) * 10.0;
            const auto updated =
                session.update({.chartTimeMs = chartTimeMs, .simulationDeltaTimeMs = 10.0});
            const auto extracted = session.extractFrame({.width = 1280, .height = 720}, snapshot);
            if (!updated || !extracted) {
                succeeded = false;
                break;
            }
        }
        const auto allocations = endAllocationTracking();
        CHECK(succeeded);
        return allocations;
    };

    const auto first = runWindow(33, 64);
    const auto second = runWindow(97, 64);
    std::cout << "S4-G nonempty CXT first=" << first << " second=" << second << '\n';
    CHECK(second <= first);
    CHECK_FALSE(snapshot.objects.empty());
}
