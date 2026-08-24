#pragma once

#include <cuexis/core/diagnostic.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cuexis::cxc {

struct CxcManifestLimits final {
    std::size_t maxManifestBytes{1024U * 1024U};
    std::size_t maxNestingDepth{64};
    std::size_t maxStringBytes{1024U * 1024U};
    std::size_t maxEntries{65533};
    std::uint64_t maxEntryBytes{64U * 1024U * 1024U};
    std::uint64_t maxListedBytes{512U * 1024U * 1024U};
    std::size_t maxExtensions{256};
    std::size_t maxDiagnostics{1024};
};

struct CxcRequiredExtension final {
    std::string id;
    std::uint32_t version{1};
};

struct CxcManifestEntry final {
    std::string path;
    std::uint64_t byteCount{};
    std::string sha256;
    std::string fieldPath;
};

struct CxcManifestDocument final {
    std::string projectPath;
    std::vector<CxcManifestEntry> entries;
    std::vector<CxcRequiredExtension> requiredExtensions;
    std::string canonicalExtensionsJson;
    std::string canonicalSourceJson;
};

struct CxcManifestResult final {
    std::optional<CxcManifestDocument> document;
    core::Diagnostics diagnostics;

    [[nodiscard]] auto hasValue() const noexcept -> bool {
        return document.has_value() && !diagnostics.hasErrors();
    }
};

} // namespace cuexis::cxc
