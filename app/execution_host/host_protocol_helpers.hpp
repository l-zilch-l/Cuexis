// Shared inline helpers for the host protocol translation units.
// These were anonymous-namespace functions; they are pure and stay internal to the
// execution-host support library, so inline definitions are safe here.
#pragma once

#include "host_protocol.hpp"

#include <algorithm>
#include <cmath>

namespace cuexis::playback::detail {

inline auto failure(std::string_view code, std::string_view message) -> core::Error {
    return {"host.rpa." + std::string{code}, std::string{message}};
}
inline auto terminal(StepStatus s) -> bool {
    return s == StepStatus::applied || s == StepStatus::rejectedNotApplied ||
           s == StepStatus::canceledNotApplied || s == StepStatus::failed ||
           s == StepStatus::retired;
}

inline auto same(const StepReceipt& a, const StepReceipt& b) -> bool {
    return a.request == b.request && a.status == b.status && a.effects == b.effects &&
           sameHostError(a.cause, b.cause);
}
inline auto index(Participant p) -> std::size_t {
    return static_cast<std::size_t>(p);
}
inline auto index(StepPhase p) -> std::size_t {
    return static_cast<std::size_t>(p);
}
inline auto validNextContext(const ControlRequest& request) -> bool {
    if (static_cast<std::uint8_t>(request.kind) > static_cast<std::uint8_t>(ControlKind::close))
        return false;
    if (request.target.audioPositionMs && !std::isfinite(*request.target.audioPositionMs))
        return false;
    if (!request.target.nextContext)
        return true;
    const auto& next = *request.target.nextContext;
    const auto& current = request.context;
    if (next.sessionScope != current.sessionScope ||
        next.controlSequence != current.controlSequence ||
        next.resourceGeneration < current.resourceGeneration ||
        next.rendererGeneration < current.rendererGeneration)
        return false;
    if (request.kind == ControlKind::resume || request.kind == ControlKind::seek)
        return next.clockSegment > current.clockSegment && next.runEpoch > current.runEpoch;
    return next.clockSegment == current.clockSegment && next.runEpoch == current.runEpoch;
}
inline auto cleanupPhase(StepPhase phase) -> bool {
    return phase == StepPhase::cut || phase == StepPhase::stop || phase == StepPhase::isolate ||
           phase == StepPhase::retire || phase == StepPhase::close;
}
inline auto confirmed(const ControlReceipt& control, Participant participant, StepPhase phase)
    -> const StepReceipt* {
    const auto& step = control.steps[index(participant)][index(phase)];
    if (!step || step->status != StepStatus::applied ||
        step->request.key.sessionScope != control.request.context.sessionScope ||
        step->request.key.operationId != control.request.operationId ||
        step->request.key.participant != participant || step->request.key.phase != phase ||
        step->request.target != control.request.target)
        return nullptr;
    return &*step;
}
inline auto ownersApplied(const ControlReceipt& control) -> bool {
    for (std::size_t p = 0; p < control.request.participants.size(); ++p) {
        if (!control.request.participants[p])
            continue;
        const auto participant = static_cast<Participant>(p);
        if (!confirmed(control, participant,
                       controlRequiredPhase(control.request.kind, participant)))
            return false;
        if (participant == Participant::audio && (control.request.kind == ControlKind::stop ||
                                                  control.request.kind == ControlKind::reset)) {
            const auto* isolated = confirmed(control, participant, StepPhase::isolate);
            if (!isolated || !isolated->effects.callbackIsolated)
                return false;
        }
    }
    return true;
}

} // namespace cuexis::playback::detail
