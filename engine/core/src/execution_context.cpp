#include <cuexis_internal/execution_stop.hpp>

namespace cuexis::internal::execution_stop {
auto context() noexcept -> Context& {
    static thread_local Context value;
    return value;
}
} // namespace cuexis::internal::execution_stop

#if defined(CUEXIS_EXECUTION_DIAGNOSTICS_TEST_ONLY)
namespace cuexis::internal::execution_diagnostics {
auto observerState() noexcept -> ObserverState& {
    static ObserverState value;
    return value;
}
auto activeTrace() noexcept -> Trace*& {
    static thread_local Trace* value{};
    return value;
}
} // namespace cuexis::internal::execution_diagnostics
#endif
