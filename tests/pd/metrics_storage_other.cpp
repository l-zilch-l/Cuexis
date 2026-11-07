#include <cuexis_internal/presentation_metrics.hpp>

namespace cuexis::test_support {
auto metricsFromOtherTranslationUnit() noexcept -> const void* {
    return &internal::pd::threadMetrics();
}
} // namespace cuexis::test_support
