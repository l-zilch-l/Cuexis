#pragma once

#include <cuexis/content/content_provider.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>
#include <cuexis/playback/presentation.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::test_support::s5h {

inline void appendU32(std::vector<std::byte>& bytes, std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
        bytes.push_back(static_cast<std::byte>((value >> (index * 8U)) & 0xFFU));
    }
}

inline void appendU64(std::vector<std::byte>& bytes, std::uint64_t value) {
    for (std::size_t index = 0; index < 8; ++index) {
        bytes.push_back(static_cast<std::byte>((value >> (index * 8U)) & 0xFFU));
    }
}

inline void appendRaw(std::vector<std::byte>& bytes, std::string_view text) {
    for (const unsigned char character : text) {
        bytes.push_back(static_cast<std::byte>(character));
    }
}

inline auto wrapPayload(std::uint32_t kind, const std::vector<std::byte>& body)
    -> std::vector<std::byte> {
    std::vector<std::byte> bytes;
    appendRaw(bytes, "CXPRES01");
    appendU32(bytes, kind);
    appendU32(bytes, 1);
    appendU64(bytes, 24ULL + body.size());
    bytes.insert(bytes.end(), body.begin(), body.end());
    return bytes;
}

inline constexpr std::string_view kVertex =
    "#version 450\nvoid main() {\n    gl_Position = vec4(0.0);\n}\n";
inline constexpr std::string_view kFragment =
    "#version 450\nlayout(location=0) out vec4 outColor;\nvoid "
    "main() {\n    outColor = vec4(1.0);\n}\n";

inline constexpr std::string_view kChart = R"json({
  "format": "cuexis.chart",
  "version": 3,
  "chartId": "019f0000-0000-7abc-8def-000000000501",
  "metadata": { "title": "S5-H Parameterized", "artist": "Cuexis" },
  "timing": {
    "offsetMs": 0.0,
    "defaultBpm": 120.0,
    "tempoEvents": [],
    "stops": []
  },
  "camera": {
    "type": "perspective",
    "fovY": 60.0,
    "near": 0.1,
    "far": 1000.0,
    "defaultTransform": { "position": [0.0, 0.0, 5.0] }
  },
  "templates": [],
  "behaviors": [],
  "objects": [
    {
      "id": "019f0000-0000-7abc-8def-000000000510",
      "name": "sprite",
      "parent": null,
      "components": {
        "cuexis.transform": {
          "version": 1,
          "position": [0.0, 0.0, 0.0],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.0, 1.0, 1.0]
        },
        "cuexis.renderable": {
          "version": 1,
          "mesh": { "domain": "asset", "id": "mesh.triangle" },
          "material": { "domain": "asset", "id": "material.sprite" }
        }
      },
      "extensions": {}
    },
    {
      "id": "019f0000-0000-7abc-8def-000000000520",
      "name": "camera_main",
      "parent": null,
      "components": {
        "cuexis.transform": {
          "version": 1,
          "position": [0.0, 0.0, 5.0],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.0, 1.0, 1.0]
        },
        "cuexis.camera": {
          "version": 1,
          "type": "perspective",
          "fovY": 60.0,
          "near": 0.1,
          "far": 1000.0
        }
      },
      "extensions": {}
    }
  ],
  "requiredExtensions": [],
  "extensions": {}
})json";

struct ShaderLayout final {
    std::uint32_t keywordCount{};
    std::uint32_t parameterCount{};
    std::uint32_t bindingCount{};
    std::uint32_t hostExtensionCount{};
    std::uint32_t claimedVertexSourceBytes{};
    std::uint32_t claimedFragmentSourceBytes{};
    bool writeKeywords{true};
    bool writeParameters{true};
    bool writeBindings{true};
    bool writeHostExtensions{true};
    bool writeSources{true};
    bool textureParameters{false};
};

struct MaterialLayout final {
    std::uint32_t keywordCount{};
    std::uint32_t parameterCount{};
    std::uint32_t textureCount{};
    bool writeKeywords{true};
    bool writeParameters{true};
    bool writeTextures{true};
};

inline auto indexedName(std::string_view prefix, std::uint32_t index) -> std::string {
    auto name = std::string{prefix};
    if (index < 10U) {
        name.push_back('0');
    }
    name += std::to_string(index);
    return name;
}

inline void appendTypedParameter(std::vector<std::byte>& body, std::string_view name,
                                 std::uint32_t type, std::uint32_t binding) {
    appendU32(body, static_cast<std::uint32_t>(name.size()));
    appendRaw(body, name);
    appendU32(body, type);
    appendU32(body, 0);
    appendU32(body, binding);
    appendU32(body, 0);
    appendU32(body, 0);
    appendU32(body, 0);
    appendU32(body, 0);
    appendU32(body, 0);
    appendU32(body, 0);
}

inline void appendTypedBinding(std::vector<std::byte>& body, std::string_view name,
                               std::uint32_t type, std::uint32_t binding) {
    appendU32(body, 0);
    appendU32(body, binding);
    appendU32(body, type);
    appendU32(body, static_cast<std::uint32_t>(name.size()));
    appendRaw(body, name);
}

inline auto makeShaderPayload(std::string_view vertexSource, std::string_view fragmentSource,
                              const ShaderLayout& layout) -> std::vector<std::byte> {
    const auto profile = cuexis::playback::rendererProfileBuiltInV1;
    const auto vertexClaim = layout.claimedVertexSourceBytes == 0
                                 ? static_cast<std::uint32_t>(vertexSource.size())
                                 : layout.claimedVertexSourceBytes;
    const auto fragmentClaim = layout.claimedFragmentSourceBytes == 0
                                   ? static_cast<std::uint32_t>(fragmentSource.size())
                                   : layout.claimedFragmentSourceBytes;
    std::vector<std::byte> body;
    appendU32(body, 0);
    appendU32(body, 4);
    appendU32(body, 4);
    appendU32(body, layout.keywordCount);
    appendU32(body, layout.parameterCount);
    appendU32(body, layout.bindingCount);
    appendU32(body, layout.hostExtensionCount);
    appendU32(body, 1);
    appendU32(body, 0);
    appendU32(body, static_cast<std::uint32_t>(profile.size()));
    appendU32(body, vertexClaim);
    appendU32(body, fragmentClaim);
    appendRaw(body, "main");
    appendRaw(body, "main");
    appendRaw(body, profile);
    const auto parameterType = layout.textureParameters ? 7U : 1U;
    if (layout.writeKeywords) {
        for (std::uint32_t index = 0; index < layout.keywordCount; ++index) {
            const auto name = indexedName("Kw", index);
            appendU32(body, static_cast<std::uint32_t>(name.size()));
            appendRaw(body, name);
        }
    }
    if (layout.writeParameters) {
        for (std::uint32_t index = 0; index < layout.parameterCount; ++index) {
            appendTypedParameter(body, indexedName("p", index), parameterType, (index % 16U) + 1U);
        }
    }
    if (layout.writeBindings) {
        for (std::uint32_t index = 0; index < layout.bindingCount; ++index) {
            appendTypedBinding(body, indexedName("b", index), parameterType, index + 1U);
        }
    }
    if (layout.writeHostExtensions) {
        for (std::uint32_t index = 0; index < layout.hostExtensionCount; ++index) {
            const auto name = indexedName("ext", index);
            appendU32(body, static_cast<std::uint32_t>(name.size()));
            appendRaw(body, name);
        }
    }
    if (layout.writeSources) {
        appendRaw(body, vertexSource);
        appendRaw(body, fragmentSource);
    }
    return wrapPayload(4, body);
}

inline auto makeParameterizedPayload(std::string_view shaderAssetId,
                                     const MaterialLayout& layout = {}) -> std::vector<std::byte> {
    const auto headerParameterCount =
        layout.parameterCount == 0 ? layout.textureCount : layout.parameterCount;
    std::vector<std::byte> body;
    appendU32(body, 1);
    appendU32(body, 0);
    appendU32(body, layout.keywordCount);
    appendU32(body, headerParameterCount);
    appendU32(body, static_cast<std::uint32_t>(shaderAssetId.size()));
    appendU32(body, 0);
    appendRaw(body, shaderAssetId);
    if (layout.writeKeywords) {
        for (std::uint32_t index = 0; index < layout.keywordCount; ++index) {
            const auto name = indexedName("Kw", index);
            appendU32(body, static_cast<std::uint32_t>(name.size()));
            appendRaw(body, name);
        }
    }
    if (layout.writeTextures && layout.textureCount > 0) {
        for (std::uint32_t index = 0; index < layout.textureCount; ++index) {
            const auto name = indexedName("texture.slot", index);
            appendU32(body, static_cast<std::uint32_t>(name.size()));
            appendRaw(body, name);
        }
    } else if (layout.writeParameters) {
        for (std::uint32_t index = 0; index < headerParameterCount; ++index) {
            appendU32(body, 0);
            appendU32(body, 0);
            appendU32(body, 0);
            appendU32(body, 0);
        }
    }
    return wrapPayload(5, body);
}

inline auto makeSource(std::vector<std::byte> shaderBytes, std::vector<std::byte> meshBytes,
                       std::vector<std::byte> materialBytes)
    -> cuexis::core::Result<cuexis::playback::PlaybackSource> {
    std::vector<cuexis::content::MemoryContentEntry> entries{
        {.rootId = "main",
         .source = "shaders/sprite.shader.bin",
         .bytes = std::move(shaderBytes),
         .revision = 1},
        {.rootId = "main",
         .source = "materials/sprite.material.bin",
         .bytes = std::move(materialBytes),
         .revision = 1},
        {.rootId = "main",
         .source = "meshes/triangle.mesh.bin",
         .bytes = std::move(meshBytes),
         .revision = 1},
    };
    auto provider = cuexis::content::MemoryContentProvider::create(std::move(entries));
    if (!provider) {
        return cuexis::core::unexpected(std::move(provider.error()));
    }
    cuexis::playback::TypedPlaybackProject project{
        .sourceId = "s5h-parameterized",
        .chartJson = std::string{kChart},
        .assets = {{.id = "shader.sprite",
                    .type = cuexis::playback::PlaybackAssetType::Shader,
                    .rootId = "main",
                    .logicalSource = "shaders/sprite.shader.bin",
                    .dependencies = {}},
                   {.id = "material.sprite",
                    .type = cuexis::playback::PlaybackAssetType::Material,
                    .rootId = "main",
                    .logicalSource = "materials/sprite.material.bin",
                    .dependencies = {"shader.sprite"}},
                   {.id = "mesh.triangle",
                    .type = cuexis::playback::PlaybackAssetType::Mesh,
                    .rootId = "main",
                    .logicalSource = "meshes/triangle.mesh.bin",
                    .dependencies = {}}},
    };
    return cuexis::playback::PlaybackSource::fromTypedProject(std::move(project),
                                                              std::move(*provider));
}

inline auto makeSource(std::vector<std::byte> shaderBytes, std::vector<std::byte> meshBytes)
    -> cuexis::core::Result<cuexis::playback::PlaybackSource> {
    return makeSource(std::move(shaderBytes), std::move(meshBytes),
                      makeParameterizedPayload("shader.sprite"));
}

} // namespace cuexis::test_support::s5h
