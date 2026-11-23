// Candidate gameplay hook entries shared by the experimental source-table units. Keeping them in
// their own unit means the ordinary closure neither compiles nor declares a candidate hook, and the
// experimental closure still defines each hook once.

#include "playback_session_gameplay_hooks.hpp"

#include <cmath>

namespace cuexis::playback {

auto validateGameplayFrame(const RuntimeFrame& frame, const std::optional<RuntimeFrame>& previous)
    -> core::Result<void> {
    // The frame has its own time domain. Never derive integer H or T from this double.
    if (!std::isfinite(frame.chartTimeMs))
        return core::unexpected(core::Error{"runtime.frame.chart_time_non_finite",
                                            "RuntimeFrame chartTimeMs must be finite"});
    if (!std::isfinite(frame.simulationDeltaTimeMs) || frame.simulationDeltaTimeMs < 0)
        return core::unexpected(
            core::Error{"runtime.frame.delta_invalid", "Invalid RuntimeFrame delta"});
    if (previous) {
        if (frame.timeDiscontinuityId == previous->timeDiscontinuityId &&
            frame.chartTimeMs < previous->chartTimeMs)
            return core::unexpected(core::Error{"runtime.frame.backward_seek_undeclared",
                                                "Backward time requires discontinuity"});
        if (frame.timeDiscontinuityId != previous->timeDiscontinuityId &&
            frame.simulationDeltaTimeMs != 0)
            return core::unexpected(core::Error{"runtime.frame.discontinuity_delta_nonzero",
                                                "Discontinuity requires zero delta"});
    }
    return {};
}
auto makeGameplaySession(const GameplayContent& c) -> core::Result<judgement::JudgementSession> {
    const auto* content = detail::GameplayAccess::content(c);
    if (!content)
        return core::unexpected(gameplayError("Empty Gameplay content"));
    auto session = judgement::JudgementSession::create();
    if (!session)
        return core::unexpected(detail::projectGameplayError(std::move(session.error())));
    if (auto r = session->configure(content->configuration); !r)
        return core::unexpected(detail::projectGameplayError(std::move(r.error())));
    if (auto r = session->prepare(content->capsule.gameplay); !r)
        return core::unexpected(detail::projectGameplayError(std::move(r.error())));
    return std::move(*session);
}
auto publicTick(std::optional<judgement::Tick> t) -> std::optional<GameplayTick> {
    return t ? std::optional{GameplayTick{t->value()}} : std::nullopt;
}
auto publicFault(std::optional<judgement::FaultStage> s) -> std::optional<GameplayFaultStage> {
    if (!s)
        return {};
    switch (*s) {
    case judgement::FaultStage::kernel:
        return GameplayFaultStage::Kernel;
    case judgement::FaultStage::fold:
        return GameplayFaultStage::Fold;
    case judgement::FaultStage::control:
        return GameplayFaultStage::Control;
    }
    return {};
}

} // namespace cuexis::playback
