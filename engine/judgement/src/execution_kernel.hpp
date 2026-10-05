#pragma once

#include "ingress_transaction.hpp"
#include <cuexis/judgement/kernel_types.hpp>

namespace cuexis::judgement::detail {
struct KernelTestControls;
class ProjectionStorage {
  public:
    explicit ProjectionStorage(KernelProjection value) : projection(std::move(value)) {}
    KernelProjection projection;
};
auto validateSessionConfiguration(const SessionConfiguration&) -> core::Result<void>;

class ExecutionKernel final {
  public:
    static auto prepare(const SessionConfiguration& configuration, const PreparedGameplay& prepared)
        -> core::Result<std::unique_ptr<ExecutionKernel>>;
    auto submit(std::vector<ClockedIngress> batch)
        -> core::Result<std::vector<InputReceiptPending>>;
    auto advance(Tick horizon) -> core::Result<void>;
    [[nodiscard]] auto query() const noexcept -> std::shared_ptr<const ProjectionStorage>;
    auto inject(KernelTestControls) -> core::Result<void>;
    auto visibleSignalCount(Tick) const noexcept -> std::size_t;
    ~ExecutionKernel();

  private:
    struct Impl;
    explicit ExecutionKernel(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl_;
};
} // namespace cuexis::judgement::detail
