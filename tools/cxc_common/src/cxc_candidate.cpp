#include <cuexis/tools/cxc_candidate.hpp>

#include <cuexis/cxc/cxc_candidate.hpp>

namespace cuexis::tools {

auto validateCandidateChartExtension(const cxc::CxcPackage& package) -> core::Diagnostics {
    return cxc::validateCandidateChartExtension(package);
}

} // namespace cuexis::tools
