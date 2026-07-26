#include <cuexis/content/content_provider.hpp>
#include <cuexis/playback/playback_session.hpp>

#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
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

} // namespace

int main() {
    auto provider = cuexis::content::MemoryContentProvider::create({
        {.rootId = "memory", .source = "payload.bin", .bytes = {std::byte{0x2A}}, .revision = 7},
    });
    if (!provider) {
        return fail("MemoryContentProvider creation failed");
    }
    auto content =
        (*provider)->readBlob({.rootId = "memory", .source = "payload.bin", .maxBytes = 1});
    if (!content || content->bytes.size() != 1 || content->bytes[0] != std::byte{0x2A} ||
        content->revision != 7) {
        return fail("MemoryContentProvider read contract failed");
    }

    cuexis::playback::PlaybackSession session;
    if (!session.loadChart(chart)) {
        return fail("PlaybackSession load failed");
    }
    if (!session.update({.chartTimeMs = 250.0})) {
        return fail("PlaybackSession update failed");
    }
    auto first = session.extractFrame({.width = 1280, .height = 720});
    if (!first || first->objects.size() != 1 || !near(first->objects[0].worldMatrix[12], 5.0F)) {
        return fail("PlaybackSession first snapshot differed");
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

    if (!session.unload()) {
        return fail("PlaybackSession unload failed");
    }
    const auto state = session.state();
    if (!state || *state != cuexis::playback::SessionState::Empty) {
        return fail("PlaybackSession did not return to Empty");
    }

    std::cout << "Cuexis external consumer passed\n";
    return 0;
}
