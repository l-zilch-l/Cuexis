#pragma once

// Opt-in, bounded test-only observations. No SDK export or session dependency.
#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>

namespace cuexis::internal::pd {
enum class Stage : unsigned {
    judgement,
    projection,
    runtime,
    snapshot,
    renderer,
    runtimeCopy,
    runtimePrepare,
    runtimeCommit,
    feedback,
    receipt,
    ledgerTake,
    ledgerPublish,
    rendererPresent,
    count
};
inline constexpr std::array names{
    "judgement",          "projection",      "runtime",         "snapshot", "renderer",
    "runtime_state_copy", "runtime_prepare", "runtime_commit",  "feedback", "receipt",
    "ledger_take",        "ledger_publish",  "renderer_present"};
struct Samples final {
    std::array<double, 16384> exclusiveUs{};
    std::size_t count{}, omitted{};
    std::uint64_t items{};
    std::uint64_t allocationCount{}, requestedAllocationBytes{}, releasedAllocationBytes{};
    std::uint64_t logicalCopyBytes{};
};
struct Metrics final {
    std::array<char, 2048> pathStorage{};
    const char* path{};
    bool recording{true};
    std::array<Samples, static_cast<unsigned>(Stage::count)> stages{};
    Metrics() noexcept {
#ifdef _MSC_VER
        std::size_t size{};
        if (getenv_s(&size, pathStorage.data(), pathStorage.size(), "CUEXIS_PD_METRICS") == 0 &&
            size > 1 && size <= pathStorage.size())
            path = pathStorage.data();
#else
        const auto* value = std::getenv("CUEXIS_PD_METRICS");
        if (value && *value && std::strlen(value) < pathStorage.size()) {
            std::strcpy(pathStorage.data(), value);
            path = pathStorage.data();
        }
#endif
    }
    ~Metrics() noexcept {
        if (!path)
            return;
        try {
            // Each thread writes its own file, including workers and shared modules.
            std::ofstream out{
                std::string{path} + "." +
                    std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) +
                    ".jsonl",
                std::ios::app};
            for (unsigned i = 0; i < stages.size(); ++i) {
                const auto& s = stages[i];
                if (!s.count && !s.omitted)
                    continue;
                out << "{\"stage\":\"" << names[i] << "\",\"items\":" << s.items
                    << ",\"omitted\":" << s.omitted << ",\"allocation_count\":" << s.allocationCount
                    << ",\"requested_allocation_bytes\":" << s.requestedAllocationBytes
                    << ",\"released_allocation_bytes\":" << s.releasedAllocationBytes
                    << ",\"logical_copy_bytes\":" << s.logicalCopyBytes << ",\"exclusive_us\":[";
                for (std::size_t j = 0; j < s.count; ++j)
                    out << (j ? "," : "") << s.exclusiveUs[j];
                out << "]}\n";
            }
        } catch (...) {
            // Observation cannot change a production operation's result.
        }
    }
};
inline thread_local Metrics metrics;
inline void setRecording(bool enabled) noexcept {
    metrics.recording = enabled;
}
class Timer final {
  public:
    explicit Timer(Stage stage) noexcept : stage_(stage) {
        enabled_ = metrics.path && metrics.recording;
        if (enabled_) {
            parent_ = active_;
            active_ = this;
            start_ = Clock::now();
        }
    }
    ~Timer() noexcept {
        if (!enabled_)
            return;
        const auto total = std::chrono::duration<double, std::micro>(Clock::now() - start_).count();
        auto& s = metrics.stages[static_cast<unsigned>(stage_)];
        if (s.count < s.exclusiveUs.size())
            s.exclusiveUs[s.count++] = total - children_;
        else
            ++s.omitted;
        if (parent_)
            parent_->children_ += total;
        active_ = parent_;
    }
    Timer(const Timer&) = delete;
    auto operator=(const Timer&) -> Timer& = delete;
    void items(std::size_t count) noexcept {
        if (enabled_)
            metrics.stages[static_cast<unsigned>(stage_)].items += count;
    }
    void logicalCopy(std::size_t bytes) noexcept {
        if (enabled_)
            metrics.stages[static_cast<unsigned>(stage_)].logicalCopyBytes += bytes;
    }
    static void allocated(std::size_t bytes) noexcept {
        if (active_) {
            auto& sample = metrics.stages[static_cast<unsigned>(active_->stage_)];
            ++sample.allocationCount;
            sample.requestedAllocationBytes += bytes;
        }
    }
    static void released(std::size_t bytes) noexcept {
        if (active_)
            metrics.stages[static_cast<unsigned>(active_->stage_)].releasedAllocationBytes += bytes;
    }

  private:
    using Clock = std::chrono::steady_clock;
    inline static thread_local Timer* active_{};
    Stage stage_;
    Timer* parent_{};
    Clock::time_point start_{};
    double children_{};
    bool enabled_{};
};
} // namespace cuexis::internal::pd
#else
namespace cuexis::internal::pd {
enum class Stage : unsigned {
    judgement,
    projection,
    runtime,
    snapshot,
    renderer,
    runtimeCopy,
    runtimePrepare,
    runtimeCommit,
    feedback,
    receipt,
    ledgerTake,
    ledgerPublish,
    rendererPresent,
    count
};
inline void setRecording(bool) noexcept {}
class Timer final {
  public:
    explicit Timer(Stage) noexcept {}
    void items(decltype(sizeof(0))) noexcept {}
    void logicalCopy(decltype(sizeof(0))) noexcept {}
    static void allocated(decltype(sizeof(0))) noexcept {}
    static void released(decltype(sizeof(0))) noexcept {}
};
} // namespace cuexis::internal::pd
#endif
