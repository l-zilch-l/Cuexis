#if defined(_WIN32)
#include <windows.h>
#endif

#include <cuexis/animation/animation_compiler.hpp>
#include <cuexis/animation/animation_system.hpp>
#include <cuexis/chart/animation_program_input.hpp>
#include <cuexis/chart/rational_beat.hpp>
#include <cuexis/world/property.hpp>

#include <entt/entity/entity.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <psapi.h>
#else
#include <fstream>
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::size_t programSize = 128;
constexpr std::size_t warmupFrames = 64;
constexpr std::size_t hotFrames = 1024;

struct MemorySnapshot final {
    std::uint64_t residentBytes{};
    std::uint64_t peakResidentBytes{};
};

[[nodiscard]] auto beat(std::int64_t numerator, std::int64_t denominator = 1)
    -> cuexis::chart::RationalBeat {
    auto value = cuexis::chart::RationalBeat::create(numerator, denominator);
    if (!value) {
        throw std::runtime_error{"S4-G probe could not create a RationalBeat"};
    }
    return *value;
}

[[nodiscard]] auto memorySnapshot() -> MemorySnapshot {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)) == FALSE) {
        throw std::runtime_error{"GetProcessMemoryInfo failed"};
    }
    return {.residentBytes = static_cast<std::uint64_t>(counters.WorkingSetSize),
            .peakResidentBytes = static_cast<std::uint64_t>(counters.PeakWorkingSetSize)};
#else
    std::ifstream statm{"/proc/self/statm"};
    std::uint64_t totalPages = 0;
    std::uint64_t residentPages = 0;
    statm >> totalPages >> residentPages;
    if (!statm) {
        throw std::runtime_error{"Could not read /proc/self/statm"};
    }
    (void)totalPages;
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        throw std::runtime_error{"getrusage failed"};
    }
    const auto nativePageSize = sysconf(_SC_PAGESIZE);
    if (nativePageSize <= 0) {
        throw std::runtime_error{"sysconf(_SC_PAGESIZE) failed"};
    }
    const auto pageSize = static_cast<std::uint64_t>(nativePageSize);
    return {.residentBytes = residentPages * pageSize,
            .peakResidentBytes = static_cast<std::uint64_t>(usage.ru_maxrss) * 1024ULL};
#endif
}

[[nodiscard]] auto elapsedMicroseconds(Clock::time_point started) -> double {
    return std::chrono::duration<double, std::micro>{Clock::now() - started}.count();
}

[[nodiscard]] auto positiveDelta(std::uint64_t after, std::uint64_t before) -> std::uint64_t {
    return after > before ? after - before : 0;
}

[[nodiscard]] auto makeProgramInput() -> cuexis::chart::AnimationProgramInput {
    cuexis::chart::AnimationProgramInput input;
    input.clips.reserve(programSize);
    input.objects.reserve(programSize);
    const auto duration = beat(4);
    const auto start = beat(0);
    const auto scale = beat(1);
    for (std::size_t index = 0; index < programSize; ++index) {
        const auto id = "clip-" + std::to_string(index);
        input.clips.push_back({
            .identity = id,
            .clip =
                {
                    .id = id,
                    .durationBeats = duration,
                    .tracks = {{
                        .property = cuexis::chart::AnimationProperty::TransformPositionY,
                        .segments = {{
                            .startBeat = start,
                            .durationBeats = duration,
                            .startValue = 0.0,
                            .endValue = 1.0,
                        }},
                    }},
                },
        });

        cuexis::chart::ResolvedClipInstance instance{
            .identity = "instance-" + std::to_string(index),
            .clipIdentity = id,
            .startBeat = start,
            .durationScale = scale,
            .iterations = {.infinite = false, .count = 1},
            .fillMode = cuexis::chart::AnimationFillMode::Hold,
            .weight = 1.0,
            .propertyMask = {.properties = {"transform.position.y"}},
        };
        cuexis::chart::ResolvedBlendGroup group{
            .identity = "group-" + std::to_string(index),
            .weight = 1.0,
            .instances = {std::move(instance)},
        };
        cuexis::chart::ResolvedAnimationLayer layer{
            .identity = "layer-" + std::to_string(index),
            .weight = 1.0,
            .propertyMask = {.properties = {"transform.position.y"}},
            .blendGroups = {std::move(group)},
        };
        input.objects.push_back({
            .objectId = cuexis::chart::ChartObjectId{"note-" + std::to_string(index)},
            .layers = {std::move(layer)},
        });
    }
    return input;
}

} // namespace

int main() {
    try {
        auto input = makeProgramInput();
        const auto clipCount = input.clips.size();
        const auto objectCount = input.objects.size();
        const auto beforeCompile = memorySnapshot();
        const auto compileStarted = Clock::now();
        auto compiled = cuexis::animation::AnimationCompiler::compile(std::move(input));
        const auto compileMicroseconds = elapsedMicroseconds(compileStarted);
        if (!compiled.hasValue() || !compiled.program.has_value()) {
            throw std::runtime_error{"S4-G animation compile failed"};
        }
        const auto afterCompile = memorySnapshot();
        const auto& program = *compiled.program;

        std::vector<cuexis::animation::AnimationObjectBinding> bindings;
        std::vector<cuexis::animation::AnimationObjectBaseline> baselines;
        bindings.reserve(program.objectCount());
        baselines.reserve(program.objectCount());
        for (std::size_t index = 0; index < program.objects().size(); ++index) {
            const auto& object = program.objects()[index];
            bindings.push_back(
                {.objectId = object.objectId, .entity = static_cast<entt::entity>(index + 1)});
            baselines.push_back(
                {.objectId = object.objectId,
                 .properties = {
                     {.property = cuexis::world::PropertyId::TransformPositionY, .value = 0.0}}});
        }

        cuexis::world::PropertyWriteBuffer writes{
            *cuexis::world::requiredAnimationWrites(program.objectCount())};
        const auto chartBeat = beat(0);
        auto evaluateFrame = [&]() {
            auto mixed = cuexis::animation::AnimationSystem::evaluate(program, chartBeat, bindings,
                                                                      baselines, writes);
            if (!mixed) {
                throw std::runtime_error{std::string{"S4-G animation evaluate failed: "} +
                                         std::string{mixed.error().code()}};
            }
            if (mixed->hasErrors()) {
                throw std::runtime_error{"S4-G animation evaluate produced diagnostics"};
            }
        };

        for (std::size_t index = 0; index < warmupFrames; ++index) {
            evaluateFrame();
        }
        const auto afterWarmup = memorySnapshot();

        const auto hotStarted = Clock::now();
        for (std::size_t index = 0; index < hotFrames; ++index) {
            evaluateFrame();
        }
        const auto hotMicroseconds = elapsedMicroseconds(hotStarted);
        const auto afterHot = memorySnapshot();

        std::cout << std::fixed << std::setprecision(3) << "clip_count=" << clipCount << '\n'
                  << "object_count=" << objectCount << '\n'
                  << "compile_us=" << compileMicroseconds << '\n'
                  << "warmed_evaluate_avg_us=" << hotMicroseconds / static_cast<double>(hotFrames)
                  << '\n'
                  << "resident_before_compile_bytes=" << beforeCompile.residentBytes << '\n'
                  << "resident_after_compile_bytes=" << afterCompile.residentBytes << '\n'
                  << "peak_after_compile_bytes=" << afterCompile.peakResidentBytes << '\n'
                  << "compile_resident_delta_bytes="
                  << positiveDelta(afterCompile.residentBytes, beforeCompile.residentBytes) << '\n'
                  << "resident_after_warmup_bytes=" << afterWarmup.residentBytes << '\n'
                  << "peak_after_warmup_bytes=" << afterWarmup.peakResidentBytes << '\n'
                  << "resident_after_hot_frames_bytes=" << afterHot.residentBytes << '\n'
                  << "peak_after_hot_frames_bytes=" << afterHot.peakResidentBytes << '\n'
                  << "hot_peak_delta_bytes="
                  << positiveDelta(afterHot.peakResidentBytes, afterWarmup.peakResidentBytes)
                  << '\n';
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
