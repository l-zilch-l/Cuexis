#pragma once

// Opt-in, bounded test-only observations. No SDK export or session dependency.
#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
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
    queue,
    packedDecode,
    packedLinkValidate,
    packedIdentity,
    packedReconstruct,
    gameplayContentCreate,
    packedSharedPrepare,
    packedSemanticCompare,
    gameplayPreparePreflight,
    gameplayPrepareCopy,
    gameplayPrepareGraceResolution,
    gameplayPrepareAssembly,
    gameplayPrepareRequirements,
    gameplayPrepareResources,
    gameplayPrepareFinalize,
    gameplayAssemblyEntryCheck,
    gameplayAssemblyNamespaceMerge,
    gameplayAssemblyGraphBuild,
    gameplayAssemblyStabilize,
    gameplayAssemblyDeclarations,
    gameplayAssemblyClosures,
    gameplayAssemblyBudget,
    gameplayAssemblyIdentities,
    count
};
inline constexpr std::array names{"judgement",
                                  "projection",
                                  "runtime",
                                  "snapshot",
                                  "renderer",
                                  "runtime_state_copy",
                                  "runtime_prepare",
                                  "runtime_commit",
                                  "feedback",
                                  "receipt",
                                  "ledger_take",
                                  "ledger_publish",
                                  "renderer_present",
                                  "queue",
                                  "packed_decode",
                                  "packed_link_validate",
                                  "packed_identity",
                                  "packed_reconstruct",
                                  "gameplay_content_create",
                                  "packed_shared_prepare",
                                  "packed_semantic_compare",
                                  "gameplay_prepare_preflight",
                                  "gameplay_prepare_copy",
                                  "gameplay_prepare_grace_resolution",
                                  "gameplay_prepare_assembly",
                                  "gameplay_prepare_requirements",
                                  "gameplay_prepare_resources",
                                  "gameplay_prepare_finalize",
                                  "gameplay_assembly_entry_check",
                                  "gameplay_assembly_namespace_merge",
                                  "gameplay_assembly_graph_build",
                                  "gameplay_assembly_stabilize",
                                  "gameplay_assembly_declarations",
                                  "gameplay_assembly_closures",
                                  "gameplay_assembly_budget",
                                  "gameplay_assembly_identities"};
struct Samples final {
    // A separate observer process owns this bounded storage. The largest declared
    // trajectory has 115200 updates; omissions are rejected by the collector.
    std::array<double, 131072> exclusiveUs{}, inclusiveUs{};
    std::array<std::uint64_t, 131072> correlations{};
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
                out << "],\"inclusive_us\":[";
                for (std::size_t j = 0; j < s.count; ++j)
                    out << (j ? "," : "") << s.inclusiveUs[j];
                out << "],\"correlations\":[";
                for (std::size_t j = 0; j < s.count; ++j)
                    out << (j ? "," : "") << s.correlations[j];
                out << "]}\n";
            }
        } catch (...) {
            // Observation cannot change a production operation's result.
        }
    }
};
inline auto threadMetrics() noexcept -> Metrics& {
    static thread_local Metrics value;
    return value;
}
inline std::atomic<bool> processRecording{true};
inline void setProcessRecording(bool enabled) noexcept {
    processRecording.store(enabled, std::memory_order_release);
}
inline void setRecording(bool enabled) noexcept {
    threadMetrics().recording = enabled;
}
inline void observe(Stage stage, double inclusive, double exclusive,
                    std::uint64_t correlation = 0) noexcept {
    auto& metrics = threadMetrics();
    if (!metrics.path || !metrics.recording || !processRecording.load(std::memory_order_acquire))
        return;
    auto& sample = metrics.stages[static_cast<unsigned>(stage)];
    if (sample.count == sample.exclusiveUs.size()) {
        ++sample.omitted;
        return;
    }
    sample.exclusiveUs[sample.count] = exclusive;
    sample.inclusiveUs[sample.count] = inclusive;
    sample.correlations[sample.count++] = correlation;
}
class Timer final {
  public:
    explicit Timer(Stage stage) noexcept : stage_(stage) {
        auto& metrics = threadMetrics();
        enabled_ =
            metrics.path && metrics.recording && processRecording.load(std::memory_order_acquire);
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
        observe(stage_, total, total - children_);
        if (parent_)
            parent_->children_ += total;
        active_ = parent_;
    }
    Timer(const Timer&) = delete;
    auto operator=(const Timer&) -> Timer& = delete;
    void items(std::size_t count) noexcept {
        if (enabled_)
            threadMetrics().stages[static_cast<unsigned>(stage_)].items += count;
    }
    void logicalCopy(std::size_t bytes) noexcept {
        if (enabled_)
            threadMetrics().stages[static_cast<unsigned>(stage_)].logicalCopyBytes += bytes;
    }
    static void allocated(std::size_t bytes) noexcept {
        if (active_) {
            auto& sample = threadMetrics().stages[static_cast<unsigned>(active_->stage_)];
            ++sample.allocationCount;
            sample.requestedAllocationBytes += bytes;
        }
    }
    static void released(std::size_t bytes) noexcept {
        if (active_)
            threadMetrics()
                .stages[static_cast<unsigned>(active_->stage_)]
                .releasedAllocationBytes += bytes;
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
    queue,
    count
};
inline void setRecording(bool) noexcept {}
inline void setProcessRecording(bool) noexcept {}
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
