#pragma once

// Shapes shared by the two reference host runner units: the clock script step, the observed
// frame, the command extras, the live session context and the helpers both units call.

#include "host_clock.hpp"
#include "host_runner.hpp"

#include <cuexis/playback/playback_session.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis_reference_host {

using cuexis::playback::FrameSnapshot;
using cuexis::playback::FrameViewport;
using cuexis::playback::PlaybackSession;
using cuexis::playback::PreparedSemanticIdentity;
using cuexis::playback::RuntimeFrame;
using cuexis::playback::SessionState;

struct ClockStep final {
    double chartTimeMs{};
    double simulationDeltaTimeMs{};
    std::uint64_t discontinuityId{};
};

struct FrameObservation final {
    RuntimeFrame frame;
    std::uint64_t digest{};
    std::size_t objects{};
    bool seek{false};
};

struct CommandStep final {
    std::uint64_t commandIndex{0};
    // The diagnostic step for a refused step. "advance" is not one of the frozen
    // step names, so a command-mode failure names the verb that asked for it.
    std::string_view diagnosticStep;
};

struct HostContext final {
    PlaybackSession session;
    HostContent content;
    Transport transport{Transport::Empty};
    Clock clock;
    Counters counters;
    // The frame most recently submitted through a real update. reload() hands
    // exactly this frame to prepareReload, which is what makes H4.1 measurable;
    // it is empty until the first successful update.
    std::optional<FrameObservation> lastSample;
    std::optional<std::string> identity;
    std::size_t frameIndex{0};
    bool hasSuccessfulUpdate{false};
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
    std::int64_t gameplayH{}, gameplayT{};
    bool gameplayInputsSubmitted{false};
#endif
};

constexpr std::array<ClockStep, 4> scriptedSteps{
    ClockStep{.chartTimeMs = 0.0, .simulationDeltaTimeMs = 0.0, .discontinuityId = 0},
    ClockStep{.chartTimeMs = 625.0, .simulationDeltaTimeMs = 0.0, .discontinuityId = 1},
    ClockStep{.chartTimeMs = 250.0, .simulationDeltaTimeMs = 0.0, .discontinuityId = 2},
    ClockStep{.chartTimeMs = 1250.0, .simulationDeltaTimeMs = 0.0, .discontinuityId = 3},
};

[[nodiscard]] auto stateName(cuexis::playback::SessionState state) -> std::string_view;

[[nodiscard]] auto identityText(const cuexis::playback::PreparedSemanticIdentity& identity)
    -> std::string;

void reportStepFailure(HostReport& report, std::string_view step, std::string_view code,
                       const CommandStep* command);

[[nodiscard]] auto consumeStep(cuexis::playback::PlaybackSession& session, const ClockStep& step,
                               bool seek, HostReport& report, std::size_t index,
                               const CommandStep* command = nullptr, bool alreadyUpdated = false)
    -> std::optional<FrameObservation>;

[[nodiscard]] auto runCommandProgram(const HostOptions& options, HostReport& report,
                                     const Program& program) -> int;

} // namespace cuexis_reference_host
