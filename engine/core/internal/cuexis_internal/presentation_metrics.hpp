#pragma once

// Opt-in, bounded test-only observations. No SDK export or session dependency.
#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>

namespace cuexis::internal::pd {
enum class Stage : unsigned { judgement, projection, runtime, snapshot, renderer, count };
inline constexpr std::array names{"judgement", "projection", "runtime", "snapshot", "renderer"};
struct Samples final {
    std::array<double, 4096> exclusiveUs{};
    std::size_t count{}, omitted{};
    std::uint64_t items{};
};
struct Metrics final {
    std::array<char, 2048> pathStorage{};
    const char* path{};
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
            std::ofstream out{std::string{path} + "." +
                              std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) +
                              ".jsonl", std::ios::app};
            for (unsigned i = 0; i < stages.size(); ++i) {
                const auto& s = stages[i];
                if (!s.count && !s.omitted)
                    continue;
                out << "{\"stage\":\"" << names[i] << "\",\"items\":" << s.items
                    << ",\"omitted\":" << s.omitted << ",\"exclusive_us\":[";
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
class Timer final {
  public:
    explicit Timer(Stage stage) noexcept : stage_(stage) {
        if (metrics.path) {
            parent_ = active_;
            active_ = this;
            start_ = Clock::now();
        }
    }
    ~Timer() noexcept {
        if (!metrics.path)
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
        if (metrics.path)
            metrics.stages[static_cast<unsigned>(stage_)].items += count;
    }

  private:
    using Clock = std::chrono::steady_clock;
    inline static thread_local Timer* active_{};
    Stage stage_;
    Timer* parent_{};
    Clock::time_point start_{};
    double children_{};
};
} // namespace cuexis::internal::pd
#else
namespace cuexis::internal::pd {
enum class Stage : unsigned { judgement, projection, runtime, snapshot, renderer, count };
class Timer final {
  public:
    explicit Timer(Stage) noexcept {}
    void items(decltype(sizeof(0))) noexcept {}
};
} // namespace cuexis::internal::pd
#endif
