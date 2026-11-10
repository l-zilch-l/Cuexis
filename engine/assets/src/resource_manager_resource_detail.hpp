#pragma once

// Typed pools, per-tag resource traits and the small built-in helpers the ResourceManager
// state and the scope unit share.

#include <cuexis/assets/resource_manager.hpp>

#include <cuexis/core/error.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::assets {
namespace detail {
namespace resource_detail {

inline std::atomic<std::uint64_t> nextManagerToken{1};

template <typename Tag> struct ResourceTraits;

template <> struct ResourceTraits<MeshTag> final {
    static constexpr AssetType type = AssetType::Mesh;
    static constexpr std::string_view fallbackId = "cuexis.builtin.fallback.mesh";
    static constexpr std::string_view fallbackPayload = "CUEXIS_FALLBACK_MESH_V1";
};

template <> struct ResourceTraits<MaterialTag> final {
    static constexpr AssetType type = AssetType::Material;
    static constexpr std::string_view fallbackId = "cuexis.builtin.fallback.material";
    static constexpr std::string_view fallbackPayload = "CUEXIS_FALLBACK_MATERIAL_V1";
};

template <> struct ResourceTraits<TextureTag> final {
    static constexpr AssetType type = AssetType::Texture;
    static constexpr std::string_view fallbackId = "cuexis.builtin.fallback.texture";
    static constexpr std::string_view fallbackPayload = "CUEXIS_FALLBACK_TEXTURE_V1";
};

template <> struct ResourceTraits<AudioSourceTag> final {
    static constexpr AssetType type = AssetType::Audio;
    static constexpr std::string_view fallbackId = "cuexis.builtin.fallback.audio";
    static constexpr std::string_view fallbackPayload = "CUEXIS_FALLBACK_AUDIO_SOURCE_V1";
};

template <> struct ResourceTraits<ShaderTag> final {
    static constexpr AssetType type = AssetType::Shader;
    static constexpr std::string_view fallbackId = "cuexis.builtin.fallback.shader";
    static constexpr std::string_view fallbackPayload = "CUEXIS_FALLBACK_SHADER_V1";
};

template <typename Tag> struct Slot final {
    AssetId id;
    ResourceState state{ResourceState::Unloaded};
    std::uint32_t generation{1};
    std::uint64_t contentRevision{};
    std::size_t strongReferences{};
    std::shared_ptr<const CpuResource<Tag>> resource;
    bool pinned{};
    bool retired{};
};

template <typename Tag> struct Pool final {
    std::vector<Slot<Tag>> slots;
    std::map<std::string, std::uint32_t, std::less<>> byId;
    std::uint32_t fallbackIndex{};
};

inline auto makeBlob(std::string_view payload, std::string source)
    -> std::shared_ptr<const AssetBlob> {
    auto blob = std::make_shared<AssetBlob>();
    blob->rootId = "builtin";
    blob->source = std::move(source);
    blob->bytes.reserve(payload.size());
    for (const char character : payload) {
        blob->bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
    }
    return blob;
}

inline auto makeDiagnostics(std::size_t capacity) -> core::Diagnostics {
    return core::Diagnostics{capacity,
                             core::Diagnostic{core::DiagnosticSeverity::Error,
                                              "assets.resource.diagnostic_limit",
                                              "Resource diagnostic limit was reached", "$"}};
}

} // namespace resource_detail

using resource_detail::makeBlob;
using resource_detail::makeDiagnostics;
using resource_detail::nextManagerToken;
using resource_detail::Pool;
using resource_detail::ResourceTraits;
using resource_detail::Slot;

} // namespace detail
} // namespace cuexis::assets
