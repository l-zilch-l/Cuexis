#pragma once

// Budget constants, diagnostics and reference helpers.

#include "validation_sink.hpp"

#include <cuexis/core/error.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <limits>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::test_support {
namespace validation_detail {

constexpr std::size_t maxDiagnostics = 1024;
constexpr std::size_t maxManifestEntries = 65'536;
constexpr std::size_t maxNormalizedRecords = 100'000;
constexpr std::uint32_t maxMeshVertices = 1'048'576;
constexpr std::uint32_t maxMeshIndices = 3'145'728;
constexpr std::uint32_t maxTextureDimension = 8'192;
constexpr std::uint64_t maxResourceBytes = 64ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t maxSessionBytes = 512ULL * 1024ULL * 1024ULL;
constexpr float maxPositionMagnitude = 1'000'000.0F;
constexpr double depthQuantization = 4096.0;
constexpr double signedIntegerLimit = 0x1p63;

[[nodiscard]] inline auto resourceTypeName(playback::PresentationResourceType type) noexcept
    -> std::string_view {
    switch (type) {
    case playback::PresentationResourceType::Mesh:
        return "mesh";
    case playback::PresentationResourceType::Texture2D:
        return "texture2d";
    case playback::PresentationResourceType::UnlitMaterial:
        return "unlit_material";
    case playback::PresentationResourceType::Shader:
        return "shader";
    case playback::PresentationResourceType::ParameterizedMaterial:
        return "parameterized_material";
    }
    return "unknown";
}

[[nodiscard]] inline auto diagnostics() -> core::Diagnostics {
    return core::Diagnostics{
        maxDiagnostics,
        core::Diagnostic{core::DiagnosticSeverity::Error,
                         "playback.presentation.diagnostics.limit_exceeded",
                         "Validation Sink diagnostics reached the configured limit", "$"}
            .withContext("max_diagnostics", std::to_string(maxDiagnostics))};
}

inline void addError(core::Diagnostics& destination, const core::Error& error) {
    auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error, std::string{error.code()},
                                       std::string{error.message()}};
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    destination.add(std::move(diagnostic));
    destination.sortDeterministically();
}

[[nodiscard]] inline auto
referenceError(std::string message, const playback::PresentationResourceRef* reference = nullptr)
    -> core::Error {
    auto error = core::Error{"playback.presentation.reference.invalid", std::move(message)};
    if (reference != nullptr) {
        error.withContext("asset_id", reference->assetId)
            .withContext("resource_type", std::string{resourceTypeName(reference->type)});
    }
    return error;
}

[[nodiscard]] inline auto
frameResourceError(std::string message, std::string_view objectId,
                   const playback::PresentationResourceRef* reference = nullptr) -> core::Error {
    auto error = core::Error{"playback.presentation.frame.resource_mismatch", std::move(message)};
    if (!objectId.empty()) {
        error.withContext("object_id", std::string{objectId});
    }
    if (reference != nullptr) {
        error.withContext("asset_id", reference->assetId)
            .withContext("resource_type", std::string{resourceTypeName(reference->type)});
    }
    return error;
}

[[nodiscard]] inline auto nonFiniteError(std::string_view objectId, std::string_view field)
    -> core::Error {
    auto error = core::Error{"playback.presentation.frame.non_finite",
                             "Validation Sink frame calculation contains a non-finite value"}
                     .withContext("field", std::string{field});
    if (!objectId.empty()) {
        error.withContext("object_id", std::string{objectId});
    }
    return error;
}

[[nodiscard]] inline auto frameValueError(std::string_view objectId, std::string_view field)
    -> core::Error {
    auto error = core::Error{"playback.presentation.frame.value_invalid",
                             "Validation Sink frame value is outside the Portable v1 range"}
                     .withContext("field", std::string{field});
    if (!objectId.empty()) {
        error.withContext("object_id", std::string{objectId});
    }
    return error;
}

[[nodiscard]] inline auto checkedAdd(std::uint64_t left, std::uint64_t right,
                                     std::uint64_t& destination) noexcept -> bool {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        return false;
    }
    destination = left + right;
    return true;
}

[[nodiscard]] inline auto checkedMultiply(std::uint64_t left, std::uint64_t right,
                                          std::uint64_t& destination) noexcept -> bool {
    if (left != 0 && right > std::numeric_limits<std::uint64_t>::max() / left) {
        return false;
    }
    destination = left * right;
    return true;
}

[[nodiscard]] inline auto
referenceKey(const playback::PresentationResourceRef& reference) noexcept {
    return std::tie(reference.assetId, reference.type);
}

[[nodiscard]] inline auto isPortableAssetId(std::string_view value) noexcept -> bool {
    const auto alphaNumeric = [](unsigned char character) {
        return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
               (character >= '0' && character <= '9');
    };
    if (value.empty() || value.size() > 256 ||
        !alphaNumeric(static_cast<unsigned char>(value.front()))) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [&](unsigned char character) {
        return alphaNumeric(character) || character == '.' || character == '_' ||
               character == '-' || character == '/';
    });
}

[[nodiscard]] inline auto referenceByteCount(const playback::PresentationResourceRef& reference,
                                             std::uint64_t& destination) noexcept -> bool {
    return checkedAdd(37ULL, static_cast<std::uint64_t>(reference.assetId.size()), destination);
}

} // namespace validation_detail
} // namespace cuexis::test_support
