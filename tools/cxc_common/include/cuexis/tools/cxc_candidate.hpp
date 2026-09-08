#pragma once

#include <cuexis/core/diagnostic.hpp>
#include <cuexis/cxc/cxc_package.hpp>

namespace cuexis::tools {

// Validates the registered Chart v5 candidate extension carried by a CXC v1
// manifest. The validator only inspects declared archive bytes; it never reads
// CXT source or performs template expansion.
[[nodiscard]] auto validateCandidateChartExtension(const cxc::CxcPackage& package)
    -> core::Diagnostics;

} // namespace cuexis::tools
