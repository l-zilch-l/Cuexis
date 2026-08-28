#pragma once

// Frozen Stage 5 compile/import diagnostic codes and truncation sentinel.
// Playback presentation parse codes remain owned by cuexis_playback; this module
// repeats the string values so cuexis_shader does not include Playback headers.

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace cuexis::shader {

inline constexpr std::string_view diagnosticCompileFailed = "shader.compile.failed";
inline constexpr std::string_view diagnosticReflectMismatch = "shader.reflect.mismatch";
inline constexpr std::string_view diagnosticLimitExceeded = "shader.diagnostics.limit_exceeded";
inline constexpr std::string_view diagnosticCacheMissing = "shader.cache.missing";
inline constexpr std::string_view diagnosticCacheKeyInvalid = "shader.cache.key_invalid";
inline constexpr std::string_view diagnosticCacheToolMismatch = "shader.cache.tool_mismatch";
inline constexpr std::string_view diagnosticHotReloadFailed = "shader.hot_reload.failed";

inline constexpr std::string_view diagnosticKeywordInvalid =
    "playback.presentation.shader.keyword_invalid";
inline constexpr std::string_view diagnosticSubsetInvalid =
    "playback.presentation.shader.subset_invalid";
inline constexpr std::string_view diagnosticReservedBinding =
    "playback.presentation.shader.reserved_binding";
inline constexpr std::string_view diagnosticSchemaInvalid =
    "playback.presentation.shader.schema_invalid";
inline constexpr std::string_view diagnosticSourceEncodingInvalid =
    "playback.presentation.shader.source_encoding_invalid";

inline constexpr std::string_view contextStage = "stage";
inline constexpr std::string_view contextTool = "tool";
inline constexpr std::string_view contextAssetId = "asset_id";

inline constexpr std::string_view toolCuexisShader = "cuexis_shader";
inline constexpr std::string_view toolShaderc = "shaderc";
inline constexpr std::string_view toolSpirvTools = "spirv-tools";
inline constexpr std::string_view toolSpirvCross = "spirv-cross";

// Frozen toolchain IDs. Values must match cuexis::playback presentation.hpp; this
// module must not include Playback headers.
inline constexpr std::string_view importerProfileShaderV1 = "cuexis.importer.shader.v1";
inline constexpr std::string_view targetProfileSpirvV1 = "cuexis.target.spirv.v1";
inline constexpr std::string_view targetProfileGlsl330V1 = "cuexis.target.glsl330.v1";
inline constexpr std::string_view targetProfileGlslEs300V1 = "cuexis.target.glsles300.v1";
inline constexpr std::string_view rendererProfilePortableV1 = "cuexis.renderer.portable.v1";
inline constexpr std::string_view rendererProfileBuiltInV1 = "cuexis.renderer.builtin.v1";

inline constexpr std::string_view cacheDomain = "cuexis.shader.cache.v1";
inline constexpr std::string_view cacheMagic = "CXSCCH01";
inline constexpr std::uint32_t cacheVersionV1 = 1;
inline constexpr std::string_view cacheFileExtension = ".cxscch01";
inline constexpr std::string_view toolGlslang = "glslang";

inline constexpr std::size_t maxDiagnostics{1024};
inline constexpr std::size_t maxSourceBytesPerStage{262144};
inline constexpr std::size_t maxSpirvBytesPerStage{1048576};
inline constexpr std::size_t maxVariantKeywords{4};

} // namespace cuexis::shader
