#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cuexis/core/result.hpp>
#include <cuexis_internal/presentation_metrics.hpp>
#include <limits>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace cuexis::playback::detail {
// A slot reserves both its command and terminal reply until explicit acknowledgement.
// All APIs are thread-safe. take() has one consumer; replies are polled by the host.
template <typename Command, typename Reply> class OwnerChannel final {
    // Result publication moves through tl::expected's internal noexcept construction path.
    static_assert(std::is_nothrow_move_constructible_v<Reply>);
    static_assert(std::is_nothrow_move_constructible_v<core::Error>);

  public:
    enum class Fault : std::uint8_t { none, allocation, conflict, exhausted };
    OwnerChannel(std::size_t capacity, std::size_t reserve) : slots_(capacity), reserve_(reserve) {}
    OwnerChannel(const OwnerChannel&) = delete;
    auto operator=(const OwnerChannel&) -> OwnerChannel& = delete;
    [[nodiscard]] auto post(Command command, bool control) -> core::Result<std::uint64_t> try {
        std::lock_guard lock{mutex_};
        if (closed_)
            return core::unexpected(error("owner_closed"));
        if (reserve_ >= slots_.size() || (!control && fault_ != Fault::none) || next_ == UINT64_MAX)
            return core::unexpected(error("capacity_exceeded"));
        const auto limit = control ? slots_.size() : slots_.size() - reserve_;
        for (std::size_t i = 0; i < limit; ++i) {
            if (slots_[i])
                continue;
            slots_[i].emplace(Record{next_, std::move(command), {}, false, false});
#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
            slots_[i]->enqueued = std::chrono::steady_clock::now();
#endif
            ++next_;
            ++wakeVersion_;
            cv_.notify_one();
            return next_ - 1;
        }
        return core::unexpected(error("capacity_exceeded"));
    } catch (...) {
        markFault(Fault::allocation);
        return core::unexpected(error("capacity_exceeded"));
    }
    [[nodiscard]] auto take() -> std::optional<std::pair<std::uint64_t, Command>> {
        return takeIf([](const Command&) { return true; });
    }
    template <class Predicate>
    [[nodiscard]] auto takeIf(Predicate&& eligible)
        -> std::optional<std::pair<std::uint64_t, Command>> {
        std::lock_guard lock{mutex_};
        Record* first = nullptr;
        for (auto& slot : slots_)
            if (slot && !slot->taken && eligible(slot->command) && (!first || slot->id < first->id))
                first = &*slot;
        if (!first)
            return {};
        std::optional<std::pair<std::uint64_t, Command>> result{std::in_place, first->id,
                                                                std::move(first->command)};
        first->taken = true;
#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
        const auto elapsed = std::chrono::duration<double, std::micro>(
                                 std::chrono::steady_clock::now() - first->enqueued)
                                 .count();
        internal::pd::observe(internal::pd::Stage::queue, elapsed, elapsed, first->id);
#endif
        return result;
    }
    void complete(std::uint64_t id, core::Result<Reply>&& reply) noexcept {
        try {
            std::lock_guard lock{mutex_};
            for (auto& slot : slots_)
                if (slot && slot->id == id) {
                    if (!slot->taken || slot->terminal) {
                        fault_ = Fault::conflict;
                        return;
                    }
                    try {
                        slot->reply.emplace(std::move(reply));
                    } catch (...) {
                        fault_ = Fault::allocation;
                    }
                    slot->terminal = true;
                    ++wakeVersion_;
                    cv_.notify_all();
                    replyCv_.notify_all();
                    return;
                }
            fault_ = Fault::conflict;
        } catch (...) {
            // Failure to acquire the process mutex is fatal, not a recoverable reply.
            std::terminate();
        }
    }
    [[nodiscard]] auto poll(std::uint64_t id)
        -> core::Result<std::optional<core::Result<Reply>>> try {
        std::lock_guard lock{mutex_};
        for (const auto& slot : slots_)
            if (slot && slot->id == id) {
                if (slot->terminal && !slot->reply)
                    return core::unexpected(error("capacity_exceeded"));
                return slot->reply;
            }
        return core::unexpected(error("context_stale"));
    } catch (...) {
        markFault(Fault::allocation);
        return core::unexpected(error("capacity_exceeded"));
    }
    [[nodiscard]] auto ack(std::uint64_t id) -> core::Result<void> try {
        std::lock_guard lock{mutex_};
        for (auto& slot : slots_)
            if (slot && slot->id == id) {
                if (!slot->terminal)
                    return core::unexpected(error("control_pending"));
                slot.reset();
                replyCv_.notify_all();
                return {};
            }
        return core::unexpected(error("context_stale"));
    } catch (...) {
        markFault(Fault::allocation);
        return core::unexpected(error("capacity_exceeded"));
    }
    // Host reply waits use persistent target state, including completion before entry.
    // They do not consume replies, wake for queued commands, or steal a command notification.
    void waitForReply(std::uint64_t id, std::chrono::steady_clock::duration duration) {
        std::unique_lock lock{mutex_};
        replyCv_.wait_for(lock, duration, [&] {
            for (const auto& slot : slots_)
                if (slot && slot->id == id)
                    return slot->terminal;
            // Let poll report a missing/acknowledged job instead of waiting on a stale id.
            return true;
        });
    }
    void waitFor(std::chrono::milliseconds duration) {
        std::unique_lock lock{mutex_};
        const auto version = wakeVersion_;
        cv_.wait_for(lock, duration, [&] {
            if (wakeVersion_ != version)
                return true;
            for (const auto& slot : slots_)
                if (slot && !slot->taken)
                    return true;
            return false;
        });
    }
    void wake() {
        std::lock_guard lock{mutex_};
        ++wakeVersion_;
        cv_.notify_all();
    }
    // Seal admission under the same mutex as post before draining on shutdown.
    void closeAdmission() {
        std::lock_guard lock{mutex_};
        closed_ = true;
        ++wakeVersion_;
        cv_.notify_all();
    }
    [[nodiscard]] auto fault() const -> Fault {
        std::lock_guard lock{mutex_};
        return fault_;
    }

  private:
    struct Record final {
        std::uint64_t id;
        Command command;
        std::optional<core::Result<Reply>> reply;
        bool taken, terminal;
#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
        std::chrono::steady_clock::time_point enqueued{};
#endif
    };
    static auto error(const char* reason) -> core::Error {
        return {std::string{"host.rpa."} + reason, "Owner channel rejected request"};
    }
    void markFault(Fault reason) {
        std::lock_guard lock{mutex_};
        fault_ = reason;
    }
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable replyCv_;
    std::vector<std::optional<Record>> slots_;
    std::size_t reserve_;
    std::uint64_t next_{1}, wakeVersion_{};
    Fault fault_{};
    bool closed_{};
};
} // namespace cuexis::playback::detail
