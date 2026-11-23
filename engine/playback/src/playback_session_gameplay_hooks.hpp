#pragma once

// Candidate-only gameplay hook entries shared by the experimental source-table units. The ordinary
// source table never includes this header, so its closure sees no candidate gameplay hook
// declaration. Every declaration below has exactly one definition in
// playback_session_gameplay_hooks.cpp.
//
// This header is build-only, not installed and carries no SDK stability promise.
#include "playback_session_internal.hpp"

namespace cuexis::playback {

[[nodiscard]] auto validateGameplayFrame(const RuntimeFrame& frame,
                                         const std::optional<RuntimeFrame>& previous)
    -> core::Result<void>;

[[nodiscard]] auto makeGameplaySession(const GameplayContent& content)
    -> core::Result<judgement::JudgementSession>;

[[nodiscard]] auto publicTick(std::optional<judgement::Tick> tick) -> std::optional<GameplayTick>;

[[nodiscard]] auto publicFault(std::optional<judgement::FaultStage> stage)
    -> std::optional<GameplayFaultStage>;

[[nodiscard]] auto internalContext(GameplayExecutionContext context) -> judgement::RpaContext;

[[nodiscard]] auto canonicalOccurrence(const GameplayOccurrence& occurrence,
                                       const judgement::SessionConfiguration& config)
    -> core::Result<judgement::CanonicalInput>;

[[nodiscard]] auto publicDecision(const judgement::AdvanceDecision& decision)
    -> GameplayAdvanceDecision;

} // namespace cuexis::playback
