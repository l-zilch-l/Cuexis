#pragma once

// CXSCCH01 cache envelope, canonical cache key, and owner-thread pipeline swap.
// This header is not installed. cuexis_shader must not include Playback headers.

#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/shader/shader_compiler.hpp>
#include <cuexis/shader/shader_diagnostics.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::shader {

struct ShaderToolVersion final {
    std::string name;
    std::string version;

    friend bool operator==(const ShaderToolVersion&, const ShaderToolVersion&) = default;
};

struct ShaderCacheKeyInput final {
    std::array<std::uint8_t, 32> sourceIdentity{};
    std::string_view importerProfile{importerProfileShaderV1};
    std::span<const std::string_view> targetProfiles{};
    std::span<const std::string_view> selectedKeywords{};
    std::string_view vertexEntry{"main"};
    std::string_view fragmentEntry{"main"};
    std::span<const ShaderToolVersion> tools{};
};

struct ShaderCacheRecord final {
    std::array<std::uint8_t, 32> key{};
    std::array<std::uint8_t, 32> sourceIdentity{};
    std::string importerProfile{std::string{importerProfileShaderV1}};
    std::vector<std::string> targetProfiles;
    std::vector<std::string> selectedKeywords;
    std::string vertexEntry{"main"};
    std::string fragmentEntry{"main"};
    std::vector<ShaderToolVersion> tools;
    ShaderCompileArtifact artifact;

    friend bool operator==(const ShaderCacheRecord&, const ShaderCacheRecord&) = default;
};

[[nodiscard]] auto defaultTargetProfiles() -> std::array<std::string_view, 3>;
[[nodiscard]] auto currentToolVersions() -> std::vector<ShaderToolVersion>;
[[nodiscard]] auto encodeCacheKey(const ShaderCacheKeyInput& input) -> std::array<std::uint8_t, 32>;
[[nodiscard]] auto cacheFileName(const std::array<std::uint8_t, 32>& key) -> std::string;
// Importer CLI identity when Playback CXPRES identity is not supplied via --identity.
// OpenGL lookup passes Playback's 32-byte CXPRES source identity instead.
[[nodiscard]] auto hashStandaloneSourceIdentity(
    std::string_view vertexSource, std::string_view fragmentSource,
    std::string_view vertexEntry = "main", std::string_view fragmentEntry = "main",
    std::span<const std::string_view> selectedKeywords = {}) -> std::array<std::uint8_t, 32>;

[[nodiscard]] auto encodeCache(const ShaderCacheRecord& record)
    -> core::Result<std::vector<std::byte>>;
[[nodiscard]] auto decodeCache(std::span<const std::byte> bytes) -> core::Result<ShaderCacheRecord>;
[[nodiscard]] auto adoptCacheRecord(const ShaderCacheRecord& record)
    -> core::Result<ShaderCacheRecord>;

[[nodiscard]] auto makeShaderDiagnostics() -> core::Diagnostics;

class ShaderCacheStore final {
  public:
    explicit ShaderCacheStore(std::filesystem::path directory);

    [[nodiscard]] auto directory() const noexcept -> const std::filesystem::path&;
    [[nodiscard]] auto pathForKey(const std::array<std::uint8_t, 32>& key) const
        -> std::filesystem::path;

    [[nodiscard]] auto load(const ShaderCacheKeyInput& input) const
        -> core::Result<ShaderCacheRecord>;
    [[nodiscard]] auto store(ShaderCacheRecord record) const -> core::Result<std::filesystem::path>;

  private:
    std::filesystem::path directory_;
};

// Candidate/active swap for Player adapter prepare. Compile runs on a worker thread;
// activate() is a noexcept owner-thread swap and never calls the compiler.
class ShaderPipelineCache final {
  public:
    explicit ShaderPipelineCache(std::filesystem::path directory);

    [[nodiscard]] auto prepareCandidate(const ShaderCompileRequest& request,
                                        const ShaderCacheKeyInput& key, bool compileEnabled)
        -> core::Result<void>;
    void activate() noexcept;

    [[nodiscard]] auto active() const noexcept -> const ShaderCacheRecord*;
    [[nodiscard]] auto candidate() const noexcept -> const ShaderCacheRecord*;
    [[nodiscard]] auto diagnostics() const noexcept -> const core::Diagnostics&;

  private:
    ShaderCacheStore store_;
    std::unique_ptr<ShaderCacheRecord> active_;
    std::unique_ptr<ShaderCacheRecord> candidate_;
    core::Diagnostics diagnostics_{makeShaderDiagnostics()};
};

} // namespace cuexis::shader
