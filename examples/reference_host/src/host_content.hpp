#pragma once

// Host-owned content model. The reference host resolves its own logical asset
// sources, serves the bytes through its own IContentProvider implementation and
// hands the SDK a declared asset table. The SDK never reads the host filesystem
// by itself in this flow.

#include <cuexis/playback/content_provider.hpp>
#include <cuexis/playback/playback_source.hpp>

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "host_report.hpp"

namespace cuexis_reference_host {

// Resolves logical asset sources below one host content root and refuses any
// request that escapes it. Every read is counted so the host can prove that the
// SDK asked the host for content.
class HostFileProvider final : public cuexis::content::IContentProvider {
  public:
    HostFileProvider(std::filesystem::path root, HostReport& report);

    [[nodiscard]] auto readBlob(const cuexis::content::ContentRequest& request)
        -> cuexis::core::Result<cuexis::content::ContentBlob> override;

    // Host-side fault injection: every later read fails with the given host
    // code, as it would when a host storage device disappears.
    void failAllReads(std::string code);

    [[nodiscard]] auto readCount() const noexcept -> std::size_t;

  private:
    std::filesystem::path root_;
    HostReport& report_;
    std::size_t readCount_{0};
    std::string pendingFailure_;
};

struct HostContent final {
    std::filesystem::path projectDirectory;
    std::string chartEntryPath{"charts/main.cuexis.chart.json"};
    std::string providerRootId{"main"};
};

// The declared asset table of the reference content. A production host derives
// this from its own authoring data; the reference host keeps it explicit so the
// public contract stays visible.
[[nodiscard]] auto hostAssetTable() -> std::vector<cuexis::playback::PlaybackAssetDescriptor>;

// Builds a typed project source from host content. When faulty is true the
// provider refuses the first read, which must make the load or reload fail
// without disturbing active content.
[[nodiscard]] auto buildProjectSource(const HostContent& content, HostReport& report, bool faulty)
    -> cuexis::core::Result<cuexis::playback::PlaybackSource>;

} // namespace cuexis_reference_host
