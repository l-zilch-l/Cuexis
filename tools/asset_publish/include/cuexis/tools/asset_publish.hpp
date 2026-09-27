#pragma once

// Atomic publication transactions for Stage 6 E3.
//
// Project-side outputs are published as an immutable generation directory: everything is written
// and validated in a staging directory first, and one directory rename makes the whole generation
// visible. A process-shared publication lock rejects a concurrent writer instead of letting two
// writers interleave. CXC packages are written to a same-directory temporary file, re-opened and
// validated through the production loader, and only then replaced atomically, so a failure leaves
// the previous valid package in place.
//
// This is an internal developer tool library: it is not installed, it is not part of the Cuexis
// SDK, and Playback/Player never link it. Publishing never rewrites a user project entry: adopting
// a generation is a separate, explicit call.

#include <cuexis/core/result.hpp>
#include <cuexis/cxc/cxc_package.hpp>
#include <cuexis/cxc/cxc_writer.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::tools {

inline constexpr std::string_view generationMarkerName = "cuexis.generation";
inline constexpr std::string_view generationMarkerFormat = "cuexis.generation";
inline constexpr std::uint32_t generationMarkerVersion = 1;
inline constexpr std::string_view adoptedGenerationName = "cuexis.adopted";
inline constexpr std::string_view adoptedGenerationFormat = "cuexis.adopted";
inline constexpr std::uint32_t adoptedGenerationVersion = 1;
inline constexpr std::string_view publicationLockName = ".cuexis.publish.lock";
inline constexpr std::string_view provenanceDirectoryName = "provenance";

// One file of a publication batch. `path` is a portable relative path inside the generation.
struct PublishEntry final {
    std::string path;
    std::vector<std::byte> bytes;
};

// A generation is one immutable project-side output tree. `entries` form the runtime closure;
// `provenanceRecords` are author-side audit sidecars that are stored beside the closure but are not
// part of it, so a runtime consumer never needs them.
struct GenerationPublishRequest final {
    std::filesystem::path root;
    std::string generationId;
    std::vector<PublishEntry> entries;
    std::vector<PublishEntry> provenanceRecords;
};

struct GenerationPublishResult final {
    std::filesystem::path generationPath;
    std::string generationIdentity;
    std::uint64_t closureBytes{};
    std::size_t closureEntries{};
    std::size_t provenanceEntries{};
};

// An exclusive lock file held for the lifetime of the object. A second writer, in this process or
// in another one, fails with `asset.publish.busy` instead of queueing.
class PublicationLock final {
  public:
    PublicationLock(const PublicationLock&) = delete;
    auto operator=(const PublicationLock&) -> PublicationLock& = delete;
    PublicationLock(PublicationLock&& other) noexcept;
    auto operator=(PublicationLock&& other) noexcept -> PublicationLock&;
    ~PublicationLock();

    [[nodiscard]] static auto acquire(const std::filesystem::path& target)
        -> core::Result<PublicationLock>;

    [[nodiscard]] auto path() const noexcept -> const std::filesystem::path&;

  private:
    PublicationLock() = default;

    void release() noexcept;

    std::filesystem::path path_;
    void* handle_{};
};

// Removes staging directories and abandoned temporary files left by an interrupted writer. Returns
// the number of removed entries. Call it while holding the publication lock.
[[nodiscard]] auto recoverPublicationStaging(const std::filesystem::path& root)
    -> core::Result<std::size_t>;

// Writes, validates and publishes one generation. On any failure the staging directory is removed
// and the previously published generations are untouched.
[[nodiscard]] auto publishGeneration(const GenerationPublishRequest& request)
    -> core::Result<GenerationPublishResult>;

// Explicit adoption: records which generation is in use. It never rewrites the user project entry.
[[nodiscard]] auto adoptGeneration(const std::filesystem::path& root, std::string_view generationId)
    -> core::Result<void>;
[[nodiscard]] auto readAdoptedGeneration(const std::filesystem::path& root)
    -> core::Result<std::string>;

struct PackagePublishRequest final {
    std::filesystem::path target;
    std::vector<cxc::CxcWriteEntry> entries;
    std::string extensionsJson{"{}"};
    std::vector<cxc::CxcRequiredExtension> requiredExtensions;
    cxc::CxcPackageLimits limits{};
};

struct PackagePublishResult final {
    std::filesystem::path path;
    std::string packageIdentity;
    std::uint64_t bytes{};
    bool replacedExisting{};
};

// Builds one package in memory, validates it with the production loader, writes it to a
// same-directory temporary file, re-validates the bytes on disk, and replaces the target
// atomically.
[[nodiscard]] auto publishPackage(const PackagePublishRequest& request)
    -> core::Result<PackagePublishResult>;

// One import batch, two packages: the v4 package and the candidate package are built from the same
// shared closure, the candidate package additionally carries the entries its extension declares,
// and both are validated before either target is replaced. If the second replacement fails, the
// first target is rolled back, so a failure never leaves a half-updated pair.
struct PackagePairRequest final {
    std::filesystem::path v4Target;
    std::filesystem::path candidateTarget;
    // The project closure both packages share.
    std::vector<cxc::CxcWriteEntry> entries;
    // Entries that only the candidate package declares, for example a compiled chart.
    std::vector<cxc::CxcWriteEntry> candidateEntries;
    std::string candidateExtensionsJson;
    std::vector<cxc::CxcRequiredExtension> requiredExtensions;
    cxc::CxcPackageLimits limits{};
};

struct PackagePairResult final {
    PackagePublishResult v4;
    PackagePublishResult candidate;
};

[[nodiscard]] auto publishPackagePair(const PackagePairRequest& request)
    -> core::Result<PackagePairResult>;

} // namespace cuexis::tools
