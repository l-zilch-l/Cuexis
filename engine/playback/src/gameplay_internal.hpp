#pragma once
#include <algorithm>
#include <array>
#include <cuexis/gameplay_packed/gameplay_capsule.hpp>
#include <cuexis/judgement/judgement_session.hpp>
#include <cuexis/playback/gameplay_candidate.hpp>

namespace cuexis::playback::detail {
inline auto projectGameplayError(core::Error error) -> core::Error {
    const auto code = error.code();
    if (code.starts_with("graph.")) {
        const auto category = code == "graph.header.unsupported_revision" ? "unknown_capability"
                              : code == "graph.budget.exceeded"           ? "budget_exceeded"
                              : code == "graph.closure.invalid" || code == "graph.identity.mismatch"
                                  ? "identity_closure_incomplete"
                                  : "invalid_relation";
        error.withContext("category", category);
        error.withContext("severity", "error");
        error.withContext("faulted", "false");
        return error;
    }
    if (!code.starts_with("judgement.s7a2.") && !code.starts_with("judgement.s7a3."))
        return error;
    const std::array<std::string_view, 5> publicInputCodes{
        "judgement.s7a2.timebase.tick_overflow", "judgement.s7a2.timebase.time_reversal",
        "judgement.s7a2.input.amount_out_of_range", "judgement.s7a2.input.amount_narrowed",
        "judgement.s7a2.input.same_tick_collision"};
    if (std::find(publicInputCodes.begin(), publicInputCodes.end(), code) != publicInputCodes.end())
        return error;
    std::string category = "invalid_relation";
    for (const auto& c : error.context())
        if (c.key == "category")
            category = c.value;
    const std::array<std::pair<std::string_view, std::string_view>, 9> mapping{
        {{"unknown_capability", "capability.unknown"},
         {"capability_disabled", "capability.disabled"},
         {"budget_exceeded", "capability.budget_insufficient"},
         {"identity_closure_incomplete", "identity_closure_incomplete"},
         {"late_policy_incomplete", "judgement.s7a4.late.parameters_invalid"},
         {"ambiguous_migration", "migration.ambiguous"},
         {"non_unique_solution", "playback.gameplay.non_unique_solution"},
         {"non_terminating_source", "playback.gameplay.non_terminating_source"},
         {"invalid_relation", "playback.gameplay.invalid"}}};
    const auto entry = std::find_if(mapping.begin(), mapping.end(),
                                    [&](const auto& row) { return row.first == category; });
    core::Error projected{entry == mapping.end() ? "playback.gameplay.invalid"
                                                 : std::string{entry->second},
                          std::string{error.message()}};
    for (const auto& c : error.context())
        projected.withContext(c.key, c.value);
    projected.withContext("sourceCode", std::string{code});
    return projected;
}

struct GameplayContentStorage final {
    gameplay_packed::PreparedCapsule capsule;
    judgement::SessionConfiguration configuration;
    std::vector<GameplayFactBinding> bindings;
    std::vector<std::string> enabledCapabilities;
    std::string publicConfiguration;
};
struct GameplayResultStorage final {
    judgement::KernelProjection kernel;
    GameplayState state;
};
struct GameplayReplayStorage final {
    judgement::ReplayArchive archive;
};
struct GameplaySnapshotStorage final {
    judgement::SnapshotPayload snapshot;
};
struct GameplayCheckpointStorage final {
    judgement::ReplayCheckpoint checkpoint;
};
struct GameplayAccess final {
    static auto content(gameplay_packed::PreparedCapsule capsule,
                        judgement::SessionConfiguration configuration,
                        std::vector<GameplayFactBinding> bindings,
                        std::vector<std::string> capabilities, std::string configurationJson)
        -> GameplayContent {
        GameplayContent result;
        result.storage_ = std::make_shared<GameplayContentStorage>(
            std::move(capsule), std::move(configuration), std::move(bindings),
            std::move(capabilities), std::move(configurationJson));
        return result;
    }
    static auto checkpoint(const GameplayCheckpoint& c) -> const GameplayCheckpointStorage* {
        return c.storage_.get();
    }
    static auto checkpoint(judgement::ReplayCheckpoint c) -> GameplayCheckpoint {
        GameplayCheckpoint result;
        result.storage_ = std::make_shared<GameplayCheckpointStorage>(std::move(c));
        return result;
    }
    static auto result(const GameplayResult& r) -> const GameplayResultStorage* {
        return r.storage_.get();
    }
    static auto content(const GameplayContent& c) -> const GameplayContentStorage* {
        return c.storage_.get();
    }
    static auto replay(const GameplayReplay& r) -> const GameplayReplayStorage* {
        return r.storage_.get();
    }
    static auto snapshot(const GameplaySnapshot& s) -> const GameplaySnapshotStorage* {
        return s.storage_.get();
    }
    static auto result(const judgement::KernelProjection& k, GameplayState s) -> GameplayResult {
        GameplayResult r;
        r.storage_ = std::make_shared<GameplayResultStorage>(k, s);
        return r;
    }
    static auto replay(judgement::ReplayArchive a) -> GameplayReplay {
        GameplayReplay r;
        r.storage_ = std::make_shared<GameplayReplayStorage>(std::move(a));
        return r;
    }
    static auto snapshot(judgement::SnapshotPayload s) -> GameplaySnapshot {
        GameplaySnapshot r;
        r.storage_ = std::make_shared<GameplaySnapshotStorage>(std::move(s));
        return r;
    }
};
} // namespace cuexis::playback::detail
