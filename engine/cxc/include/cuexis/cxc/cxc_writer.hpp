#pragma once

#include <cuexis/core/diagnostic.hpp>
#include <cuexis/cxc/cxc_manifest.hpp>
#include <cuexis/cxc/cxc_package.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace cuexis::cxc {

struct CxcWriteEntry final {
    std::string path;
    std::vector<std::byte> bytes;
};

struct CxcWriteRequest final {
    std::vector<CxcWriteEntry> entries;
    std::vector<CxcRequiredExtension> requiredExtensions;
    std::string extensionsJson{"{}"};
};

struct CxcWriteResult final {
    std::optional<std::vector<std::byte>> bytes;
    core::Diagnostics diagnostics;

    [[nodiscard]] auto hasValue() const noexcept -> bool {
        return bytes.has_value() && !diagnostics.hasErrors();
    }
};

class CxcWriter final {
  public:
    [[nodiscard]] static auto write(CxcWriteRequest request, const CxcPackageLimits& limits = {})
        -> CxcWriteResult;
};

} // namespace cuexis::cxc
