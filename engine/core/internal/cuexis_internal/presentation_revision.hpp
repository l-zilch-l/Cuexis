#pragma once

#include <cstdint>
#include <cuexis/core/result.hpp>
#include <limits>
#include <utility>

namespace cuexis::internal::presentation {
enum class Source : std::uint8_t { Prepared, RuntimeUpdate, Restore, Mutation };
struct RevisionClock final {
    std::uint64_t version{}, layout{}, epoch{}, updates{};
};

// One owner operation reserves before writing and publishes only at its no-fail tail.
// Epoch invalidates borrowing even when a candidate fails. Reads never reserve a mutation.
class Mutation final {
  public:
    Mutation(const Mutation&) = delete;
    auto operator=(const Mutation&) -> Mutation& = delete;
    Mutation(Mutation&& other) noexcept
        : clock_(std::exchange(other.clock_, nullptr)), layout_(other.layout_),
          update_(other.update_) {}
    static auto reserve(RevisionClock& clock, bool layout = false, bool update = false)
        -> core::Result<Mutation> {
        constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
        if (clock.version == maximum || clock.epoch == maximum ||
            (layout && clock.layout == maximum) || (update && clock.updates == maximum))
            return core::unexpected(core::Error{"runtime.session.update_exception",
                                                "Presentation revision capacity exhausted"});
        ++clock.epoch;
        return Mutation{clock, layout, update};
    }
    void publish() noexcept {
        if (!clock_)
            return;
        ++clock_->version;
        clock_->layout += layout_;
        clock_->updates += update_;
        clock_ = nullptr;
    }
    auto version() const noexcept -> std::uint64_t {
        return clock_->version + 1;
    }

  private:
    Mutation(RevisionClock& clock, bool layout, bool update) noexcept
        : clock_(&clock), layout_(layout), update_(update) {}
    RevisionClock* clock_;
    bool layout_, update_;
};
} // namespace cuexis::internal::presentation
