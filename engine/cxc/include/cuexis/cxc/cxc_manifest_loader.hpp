#pragma once

#include <cuexis/cxc/cxc_manifest.hpp>

#include <string_view>

namespace cuexis::cxc {

class CxcManifestLoader final {
  public:
    [[nodiscard]] static auto load(std::string_view jsonText, const CxcManifestLimits& limits = {})
        -> CxcManifestResult;
};

} // namespace cuexis::cxc
