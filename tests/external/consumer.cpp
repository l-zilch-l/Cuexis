#include <cuexis/content/content_provider.hpp>
#include <cuexis/playback/frame_digest.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>

#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::string_view chart = R"json(
{
  "format":"cuexis.chart","version":1,
  "chartId":"019b0000-0000-7abc-8def-000000000201","metadata":{},
  "timing":{"offsetMs":0,"defaultBpm":120,"bpmChanges":[],"stops":[]},
  "camera":{"type":"perspective","fovY":60,"near":0.1,"far":1000,
            "pitch":0,"yaw":0,"roll":0,"defaultTransform":{"position":[0,0,-10]}},
  "templates":[],
  "behaviors":[{
    "id":"move","type":"behavior.transform.keyframe","version":1,
    "tracks":[{"property":"transform.position.x","keys":[
      {"beat":{"numerator":0,"denominator":1},"value":0},
      {"beat":{"numerator":1,"denominator":1},"value":10,"easing":"linear"}
    ]}]
  }],
  "objects":[{
    "id":"019b0000-0000-7abc-8def-000000000210","name":"object","parent":null,
    "components":{
      "cuexis.transform":{"version":1,"position":[0,0,0],
                          "rotation":[0,0,0,1],"scale":[1,1,1]},
      "cuexis.renderable":{"version":1,
                            "mesh":{"domain":"asset","id":"mesh.note"},
                            "material":{"domain":"asset","id":"material.note"}},
      "cuexis.behavior":{"version":1,"behavior":{"domain":"behavior","id":"move"}}
    },"extensions":{}
  }],
  "requiredExtensions":[],"extensions":{}
}
)json";

[[nodiscard]] auto near(float value, float expected) -> bool {
    return std::fabs(value - expected) < 0.0001F;
}

[[nodiscard]] auto fail(std::string_view message) -> int {
    std::cerr << message << '\n';
    return 1;
}

[[nodiscard]] auto bytes(std::string_view value) -> std::vector<std::byte> {
    std::vector<std::byte> result;
    result.reserve(value.size());
    for (const char character : value) {
        result.push_back(std::byte{static_cast<unsigned char>(character)});
    }
    return result;
}

} // namespace

int main() {
    std::size_t providerReads = 0;
    auto provider = cuexis::content::HostContentProvider::create(
        [&providerReads](const cuexis::content::ContentRequest& request)
            -> cuexis::core::Result<cuexis::content::ContentBlob> {
            if (request.rootId != "memory") {
                return cuexis::core::unexpected(
                    cuexis::core::Error{"consumer.root_missing", "Unexpected content root"});
            }

            std::string_view payload;
            if (request.source == "mesh.bin") {
                payload = "external-mesh";
            } else if (request.source == "material.bin") {
                payload = "external-material";
            } else if (request.source == "texture.bin") {
                payload = "external-texture";
            } else {
                return cuexis::core::unexpected(
                    cuexis::core::Error{"consumer.source_missing", "Unexpected content source"});
            }
            ++providerReads;
            return cuexis::content::ContentBlob{.bytes = bytes(payload), .revision = 9};
        });
    if (!provider) {
        return fail("HostContentProvider creation failed");
    }

    auto source = cuexis::playback::PlaybackSource::fromTypedProject(
        {.sourceId = "external-project",
         .chartJson = std::string{chart},
         .assets = {{.id = "mesh.note",
                     .type = cuexis::playback::PlaybackAssetType::Mesh,
                     .rootId = "memory",
                     .logicalSource = "mesh.bin",
                     .dependencies = {"texture.note"}},
                    {.id = "material.note",
                     .type = cuexis::playback::PlaybackAssetType::Material,
                     .rootId = "memory",
                     .logicalSource = "material.bin",
                     .dependencies = {"texture.note"}},
                    {.id = "texture.note",
                     .type = cuexis::playback::PlaybackAssetType::Texture,
                     .rootId = "memory",
                     .logicalSource = "texture.bin"}}},
        std::move(*provider));
    if (!source) {
        return fail("PlaybackSource creation failed");
    }

    cuexis::playback::PlaybackSession session;
    if (!session.load(std::move(*source), cuexis::playback::PlaybackMode::ChartClock)) {
        return fail("PlaybackSession load failed");
    }
    const auto info = session.chartInfo();
    if (!info || info->renderableCount != 1 || info->resourceCount != 3 || providerReads != 3) {
        return fail("PlaybackSession did not load the indexed Renderable resource closure");
    }
    if (!session.update({.chartTimeMs = 250.0})) {
        return fail("PlaybackSession update failed");
    }
    auto first = session.extractFrame({.width = 1280, .height = 720});
    if (!first || first->objects.size() != 1 || !near(first->objects[0].worldMatrix[12], 5.0F)) {
        return fail("PlaybackSession first snapshot differed");
    }
    const auto digest = cuexis::playback::computeFrameDigest({.chartTimeMs = 250.0}, *first);
    constexpr std::uint64_t expectedDigest = 6726938620466257503ULL;
    if (!digest || digest->algorithmVersion != 1 || digest->value != expectedDigest) {
        return fail("Playback frame digest failed");
    }

    if (!session.reload(chart, {.chartTimeMs = 375.0},
                        cuexis::playback::ReloadPolicy::KeepChartTime)) {
        return fail("PlaybackSession reload failed");
    }
    auto reloaded = session.extractFrame({.width = 1280, .height = 720});
    if (!reloaded || reloaded->objects.size() != 1 ||
        !near(reloaded->objects[0].worldMatrix[12], 7.5F)) {
        return fail("PlaybackSession reloaded snapshot differed");
    }
    if (first->objects.size() != 1 || !near(first->objects[0].worldMatrix[12], 5.0F)) {
        return fail("Reload invalidated an owning FrameSnapshot");
    }

    if (!session.unload()) {
        return fail("PlaybackSession unload failed");
    }
    const auto state = session.state();
    if (!state || *state != cuexis::playback::SessionState::Empty) {
        return fail("PlaybackSession did not return to Empty");
    }
    if (first->objects.size() != 1 || !near(first->objects[0].worldMatrix[12], 5.0F)) {
        return fail("Unload invalidated an owning FrameSnapshot");
    }

    std::cout << "Cuexis external consumer passed digest=" << digest->value << '\n';
    return 0;
}
