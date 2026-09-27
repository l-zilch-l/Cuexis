#include "host_runner.hpp"

#include <cuexis/playback/frame_digest.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/presentation.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis_reference_host {
namespace {

using cuexis::playback::FrameSnapshot;
using cuexis::playback::FrameViewport;
using cuexis::playback::PlaybackMode;
using cuexis::playback::PlaybackSession;
using cuexis::playback::PlaybackSource;
using cuexis::playback::PreparedSemanticIdentity;
using cuexis::playback::ReloadPolicy;
using cuexis::playback::RuntimeFrame;
using cuexis::playback::SessionState;

// The host clock script. The host owns chart time, so it advances or jumps the
// clock itself and bumps the discontinuity id on every jump.
struct ClockStep final {
    double chartTimeMs{};
    double simulationDeltaTimeMs{};
    std::uint64_t discontinuityId{};
};

constexpr std::array<ClockStep, 4> scriptedSteps{
    ClockStep{.chartTimeMs = 0.0, .simulationDeltaTimeMs = 0.0, .discontinuityId = 0},
    ClockStep{.chartTimeMs = 625.0, .simulationDeltaTimeMs = 0.0, .discontinuityId = 1},
    ClockStep{.chartTimeMs = 250.0, .simulationDeltaTimeMs = 0.0, .discontinuityId = 2},
    ClockStep{.chartTimeMs = 1250.0, .simulationDeltaTimeMs = 0.0, .discontinuityId = 3},
};

[[nodiscard]] auto stateName(SessionState state) -> std::string_view {
    switch (state) {
    case SessionState::Empty:
        return "Empty";
    case SessionState::Ready:
        return "Ready";
    case SessionState::Running:
        return "Running";
    case SessionState::Failed:
        return "Failed";
    }
    return "Unknown";
}

[[nodiscard]] auto identityText(const PreparedSemanticIdentity& identity) -> std::string {
    return hexIdentity(identity.sha256.data(), identity.sha256.size());
}

struct FrameObservation final {
    RuntimeFrame frame;
    std::uint64_t digest{};
    std::size_t objects{};
    bool seek{false};
};

// Consumes one host clock step: updates the SDK, extracts the frame and records
// the digest. The host decides when a step is an advance and when it is a seek.
[[nodiscard]] auto consumeStep(PlaybackSession& session, const ClockStep& step, bool seek,
                               HostReport& report, std::size_t index)
    -> std::optional<FrameObservation> {
    const RuntimeFrame frame{.chartTimeMs = step.chartTimeMs,
                             .simulationDeltaTimeMs = step.simulationDeltaTimeMs,
                             .timeDiscontinuityId = step.discontinuityId};
    const auto stepName = seek ? "seek" : "advance";
    auto updated = session.update(frame);
    if (!updated) {
        report.failure(stepName, updated.error().code());
        return std::nullopt;
    }
    auto snapshot = session.extractFrame(FrameViewport{.width = 1280, .height = 720});
    if (!snapshot) {
        report.failure(stepName, snapshot.error().code());
        return std::nullopt;
    }
    auto digest = cuexis::playback::computeFrameDigest(frame, *snapshot);
    if (!digest || digest->algorithmVersion != 3U) {
        report.failure(stepName, "frame digest unavailable");
        return std::nullopt;
    }
    report.event("frame",
                 std::string{"index="} + std::to_string(index) + " mode=" + std::string{stepName} +
                     " chartTimeMs=" + std::to_string(static_cast<std::int64_t>(step.chartTimeMs)) +
                     " discontinuityId=" + std::to_string(step.discontinuityId) +
                     " objects=" + std::to_string(snapshot->objects.size()) +
                     " digest=" + std::to_string(digest->value) +
                     " algorithm=" + std::to_string(digest->algorithmVersion));
    return FrameObservation{
        .frame = frame, .digest = digest->value, .objects = snapshot->objects.size(), .seek = seek};
}

[[nodiscard]] auto activeIdentity(PlaybackSession& session, HostReport& report,
                                  std::string_view step) -> std::optional<std::string> {
    auto identity = session.semanticIdentity();
    if (!identity) {
        report.failure(step, identity.error().code());
        return std::nullopt;
    }
    return identityText(*identity);
}

[[nodiscard]] auto currentState(PlaybackSession& session, HostReport& report, std::string_view step)
    -> std::optional<SessionState> {
    auto state = session.state();
    if (!state) {
        report.failure(step, state.error().code());
        return std::nullopt;
    }
    return *state;
}

// Runs the whole frame script once and returns the observations in order.
[[nodiscard]] auto runFrameScript(PlaybackSession& session, const HostOptions& options,
                                  HostReport& report, std::string_view phase)
    -> std::optional<std::vector<FrameObservation>> {
    std::vector<FrameObservation> observations;
    observations.reserve(scriptedSteps.size() + options.advanceFrames);
    const std::uint64_t discontinuityId = scriptedSteps.back().discontinuityId;
    double chartTimeMs = scriptedSteps.back().chartTimeMs;
    std::size_t index = 0;
    for (const auto& step : scriptedSteps) {
        const bool seek =
            index > 0U && step.discontinuityId != scriptedSteps[index - 1U].discontinuityId;
        auto observation = consumeStep(session, step, seek, report, index);
        if (!observation) {
            report.failure(phase, "frame script failed");
            return std::nullopt;
        }
        observations.push_back(*observation);
        ++index;
    }
    // Host-owned advance loop: the host keeps its own monotonic clock, stays
    // inside the current discontinuity and feeds a real simulation delta.
    for (std::size_t extra = 0; extra < options.advanceFrames; ++extra) {
        chartTimeMs += 250.0;
        const ClockStep step{.chartTimeMs = chartTimeMs,
                             .simulationDeltaTimeMs = 250.0,
                             .discontinuityId = discontinuityId};
        auto observation = consumeStep(session, step, false, report, index);
        if (!observation) {
            report.failure(phase, "advance loop failed");
            return std::nullopt;
        }
        observations.push_back(*observation);
        ++index;
    }
    return observations;
}

[[nodiscard]] auto sameDigests(const std::vector<FrameObservation>& left,
                               const std::vector<FrameObservation>& right) -> bool {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index].digest != right[index].digest) {
            return false;
        }
    }
    return true;
}

} // namespace

auto runHost(const HostOptions& options, HostReport& report) -> int {
    if (options.contentDirectory.empty()) {
        report.failure("start", "content directory is required");
        return 1;
    }
    HostContent content{.projectDirectory = options.contentDirectory};

    report.event("start", std::string{"sdk_api_baseline=0.7.0 content="} +
                              options.contentDirectory.filename().string() +
                              (options.packageFile ? " package=yes" : " package=no"));

    PlaybackSession session;
    auto state = currentState(session, report, "start");
    if (!state || *state != SessionState::Empty) {
        report.failure("start", "fresh session is not Empty");
        return 1;
    }

    // 1. Load the host project content through the host content provider.
    auto source = buildProjectSource(content, report, false);
    if (!source) {
        report.failure("load", source.error().code());
        return 1;
    }
    auto prepared = session.prepareLoad(std::move(*source), PlaybackMode::ChartClock);
    if (!prepared) {
        report.failure("load", prepared.error().code());
        return 1;
    }
    const auto candidateIdentity = prepared->semanticIdentity();
    const auto* manifest = prepared->presentationManifest();
    if (!candidateIdentity || manifest == nullptr) {
        report.failure("load", "candidate is incomplete");
        return 1;
    }
    report.event("load", std::string{"source=host-project identity="} +
                             identityText(*candidateIdentity) +
                             " presentation_entries=" + std::to_string(manifest->entries.size()));
    auto committed = session.commit(std::move(*prepared));
    if (!committed) {
        report.failure("commit", committed.error().code());
        return 1;
    }
    auto active = activeIdentity(session, report, "commit");
    if (!active) {
        return 1;
    }
    const auto sessionIdentity = *active;
    auto commitState = currentState(session, report, "commit");
    if (!commitState) {
        return 1;
    }
    report.event("commit", std::string{"state="} + std::string{stateName(*commitState)} +
                               " identity=" + sessionIdentity);

    // 2. Frames, seeks and digests on the host clock.
    auto firstObservations = runFrameScript(session, options, report, "frames");
    if (!firstObservations) {
        return 1;
    }
    for (const auto& [index, expected] : options.expectedDigests) {
        if (index >= firstObservations->size()) {
            report.failure("frames", "expected digest index is out of range");
            return 1;
        }
        if ((*firstObservations)[index].digest != expected) {
            report.failure("frames", std::string{"digest mismatch at index "} +
                                         std::to_string(index) + ": expected " +
                                         std::to_string(expected) + " observed " +
                                         std::to_string((*firstObservations)[index].digest));
            return 1;
        }
        report.event("digest", std::string{"index="} + std::to_string(index) +
                                   " expected=" + std::to_string(expected) + " matched=yes");
    }

    // 3. Reload the same content. Chart time must be preserved and the observed
    //    frames must be identical.
    auto reloadSource = buildProjectSource(content, report, false);
    if (!reloadSource) {
        report.failure("reload", reloadSource.error().code());
        return 1;
    }
    auto reloaded = session.prepareReload(std::move(*reloadSource), firstObservations->back().frame,
                                          ReloadPolicy::KeepChartTime);
    if (!reloaded) {
        report.failure("reload", reloaded.error().code());
        return 1;
    }
    auto reloadCommitted = session.commit(std::move(*reloaded));
    if (!reloadCommitted) {
        report.failure("reload", reloadCommitted.error().code());
        return 1;
    }
    auto reloadIdentity = activeIdentity(session, report, "reload");
    if (!reloadIdentity || *reloadIdentity != sessionIdentity) {
        report.failure("reload", "active identity changed across a successful reload");
        return 1;
    }
    auto secondObservations = runFrameScript(session, options, report, "reload-frames");
    if (!secondObservations || !sameDigests(*firstObservations, *secondObservations)) {
        report.failure("reload", "reload changed the observed frames");
        return 1;
    }
    auto reloadState = currentState(session, report, "reload");
    if (!reloadState) {
        return 1;
    }
    report.event("reload", std::string{"outcome=ok identity="} + sessionIdentity + " state=" +
                               std::string{stateName(*reloadState)} + " frames_identical=yes");

    // 4. A rejected reload must not disturb active content.
    const auto identityBeforeFailure = sessionIdentity;
    const auto digestBeforeFailure = secondObservations->back().digest;
    auto faultySource = buildProjectSource(content, report, true);
    if (!faultySource) {
        report.failure("reload-failed", faultySource.error().code());
        return 1;
    }
    auto rejected = session.prepareReload(
        std::move(*faultySource), secondObservations->back().frame, ReloadPolicy::KeepChartTime);
    if (rejected) {
        report.failure("reload-failed", "a host provider failure was accepted as a candidate");
        return 1;
    }
    report.rejection("reload-failed", rejected.error().code());
    auto identityAfterFailure = activeIdentity(session, report, "reload-failed");
    auto stateAfterFailure = currentState(session, report, "reload-failed");
    if (!identityAfterFailure || !stateAfterFailure ||
        *identityAfterFailure != identityBeforeFailure ||
        *stateAfterFailure == SessionState::Failed) {
        report.failure("reload-failed", "active content was disturbed by a rejected reload");
        return 1;
    }
    auto frameAfterFailure = session.extractFrame(FrameViewport{.width = 1280, .height = 720});
    if (!frameAfterFailure) {
        report.failure("reload-failed", frameAfterFailure.error().code());
        return 1;
    }
    auto digestAfterFailure =
        cuexis::playback::computeFrameDigest(secondObservations->back().frame, *frameAfterFailure);
    if (!digestAfterFailure || digestAfterFailure->value != digestBeforeFailure) {
        report.failure("reload-failed", "active frame changed after a rejected reload");
        return 1;
    }
    report.event("active", std::string{"identity="} + *identityAfterFailure +
                               " state=" + std::string{stateName(*stateAfterFailure)} + " digest=" +
                               std::to_string(digestAfterFailure->value) + " preserved=yes");

    // 5. Load the published package. A production host must observe the same
    //    identity and the same frames as it does for host-provided content.
    if (options.packageFile) {
        // Switching content is an explicit host decision: the host unloads and
        // then loads the published artifact.
        auto unloadedForPackage = session.unload();
        if (!unloadedForPackage) {
            report.failure("package", unloadedForPackage.error().code());
            return 1;
        }
        auto packageSource = PlaybackSource::fromCxcFile(*options.packageFile);
        if (!packageSource) {
            report.failure("package", packageSource.error().code());
            return 1;
        }
        auto packagePrepared =
            session.prepareLoad(std::move(*packageSource), PlaybackMode::ChartClock);
        if (!packagePrepared) {
            report.failure("package", packagePrepared.error().code());
            return 1;
        }
        const auto packageIdentity = packagePrepared->semanticIdentity();
        if (!packageIdentity) {
            report.failure("package", "published package candidate is incomplete");
            return 1;
        }
        if (identityText(*packageIdentity) != sessionIdentity) {
            report.failure("package", "published package identity differs from host content");
            return 1;
        }
        auto packageCommitted = session.commit(std::move(*packagePrepared));
        if (!packageCommitted) {
            report.failure("package", packageCommitted.error().code());
            return 1;
        }
        auto packageObservations = runFrameScript(session, options, report, "package-frames");
        if (!packageObservations || !sameDigests(*firstObservations, *packageObservations)) {
            report.failure("package", "published package changed the observed frames");
            return 1;
        }
        report.event("package", std::string{"outcome=ok identity="} + sessionIdentity +
                                    " identity_matches_host_content=yes frames_identical=yes");
    }

    // 6. Destroy: the host unloads and releases the session.
    auto unloaded = session.unload();
    if (!unloaded) {
        report.failure("destroy", unloaded.error().code());
        return 1;
    }
    auto finalState = currentState(session, report, "destroy");
    if (!finalState || *finalState != SessionState::Empty) {
        report.failure("destroy", "session did not return to Empty");
        return 1;
    }
    report.event("destroy", "state=Empty resources_released=yes");

    if (options.expectedIdentity && *options.expectedIdentity != sessionIdentity) {
        report.failure("verify", std::string{"identity mismatch: expected "} +
                                     *options.expectedIdentity + " observed " + sessionIdentity);
    } else if (options.expectedIdentity) {
        report.event("verify", std::string{"expected_identity="} + *options.expectedIdentity +
                                   " matched=yes");
    }
    return report.ok() ? 0 : 1;
}

} // namespace cuexis_reference_host
