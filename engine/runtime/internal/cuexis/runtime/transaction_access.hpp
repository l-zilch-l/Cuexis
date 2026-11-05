#pragma once
// Noninstalled test bridge; observations never drive Runtime decisions.
#include <cuexis/runtime/runtime_session.hpp>
namespace cuexis::runtime::detail {
struct RuntimeTransactionState final {
    std::optional<RuntimeFrame> frame;
    std::uint64_t baseRevision;
    std::vector<std::uint64_t> tokenState;
};
struct RuntimeTestAccess final {
    static auto sampleReload(RuntimeSession&, PreparedRuntimeSession&, const RuntimeFrame&)
        -> core::Result<void>;
    static auto state(const RuntimeSession&) -> RuntimeTransactionState;
#if defined(CUEXIS_EXECUTION_ALLOCATION_OBSERVER)
    static void observePlaybackActiveWorld(const RuntimeSession&) noexcept;
#endif
};
} // namespace cuexis::runtime::detail
