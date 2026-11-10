#pragma once

// State shared by the player control units. The controller splits a command into a transaction,
// and at most one transaction may own the application at a time.

namespace cuexis::player::control_internal {

// Marks a load, reload, or rebuild as owning the application, so a re-entrant command is
// rejected instead of interleaving two transactions.
class TransactionGuard final {
  public:
    explicit TransactionGuard(bool& flag) noexcept : flag_(flag) {
        flag_ = true;
    }
    ~TransactionGuard() {
        flag_ = false;
    }

    TransactionGuard(const TransactionGuard&) = delete;
    auto operator=(const TransactionGuard&) -> TransactionGuard& = delete;
    TransactionGuard(TransactionGuard&&) = delete;
    auto operator=(TransactionGuard&&) -> TransactionGuard& = delete;

  private:
    bool& flag_;
};

} // namespace cuexis::player::control_internal
