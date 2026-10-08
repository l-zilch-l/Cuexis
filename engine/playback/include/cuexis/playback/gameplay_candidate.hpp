#pragma once

// Experimental owning Gameplay values. Available only in the candidate SDK flavor.
// Internal execution and recovery types are deliberately absent from this header.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cuexis/core/abi_warnings.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/playback/playback_export.hpp>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::playback {
CUEXIS_ABI_WARNING_PUSH
struct GameplayTick final {
    std::int64_t value;
    friend auto operator==(GameplayTick, GameplayTick) -> bool = default;
};
struct GameplayObservationTick final {
    std::int64_t value;
};
struct GameplayPresentationTick final {
    std::int64_t value;
};
struct GameplayRational final {
    std::int64_t numerator, denominator;
};
enum class GameplayInputAction : std::uint8_t { Press, Release, Update };
enum class GameplayPhase : std::uint8_t { Tap, Head, Body, Tail };
enum class GameplayOutcome : std::uint8_t { Hit, Miss };
enum class GameplayState : std::uint8_t { Inactive, Prepared, Running, Paused, Faulted };
enum class GameplayFaultStage : std::uint8_t { Kernel, Fold, Control };
enum class GameplayPublicationFailureStage : std::uint8_t { Result, Projection, Runtime };
struct GameplayRawTimestamps final {
    std::int64_t device, hostArrival, audioFrame, renderFrame;
};
struct GameplayInput final {
    GameplayObservationTick observationTick;
    std::uint64_t sequence;
    GameplayInputAction action;
    std::string channel, domain, sourceClass;
    std::optional<GameplayRational> amount;
    // Provenance only: these values never produce observationTick.
    GameplayRawTimestamps rawTimestamps{};
    bool crossedSamplingGap{}, reconnected{}, droppedSamples{};
};
struct GameplayInputDomain final {
    std::string id;
    GameplayRational scale;
    std::int64_t minimum, maximum;
    bool inclusive;
};
struct GameplayScoreRule final {
    GameplayPhase phase;
    GameplayOutcome outcome;
    std::optional<std::string> grade;
    std::int64_t delta;
    bool incrementsCombo;
};
struct GameplayRequirementRef final {
    std::string chartEntryId, invocationId, moduleId, exportId, requirementLocalId;
    struct PathStep final {
        std::string nodeId;
        std::uint64_t repeatIndex;
        friend auto operator==(const PathStep&, const PathStep&) -> bool = default;
    };
    std::vector<PathStep> emissionPath;
    friend auto operator==(const GameplayRequirementRef&, const GameplayRequirementRef&)
        -> bool = default;
};
enum class GameplayTimingClass : std::uint8_t { Any, Early, Exact, Late };
enum class GameplayAggregation : std::uint8_t { Any, All, GroupCommit };
struct GameplayFactBinding final {
    std::string bindingId;
    GameplayRequirementRef source;
    GameplayPhase phase;
    GameplayOutcome outcome;
    GameplayTimingClass timing;
    std::string target;
    bool visible;
    GameplayPresentationTick start;
    // Absent is the explicit UntilReset alternative, never an integer sentinel.
    std::optional<GameplayPresentationTick> end;
    GameplayAggregation aggregation{GameplayAggregation::Any};
    std::vector<std::string> groupMembers{};
};
struct GameplayFactId final {
    std::uint64_t commitId, localOrdinal;
    friend auto operator==(const GameplayFactId&, const GameplayFactId&) -> bool = default;
};
struct GameplayPresentationSource final {
    GameplayFactId factId;
    std::string target;
    bool visible;
    GameplayPresentationTick start;
    std::optional<GameplayPresentationTick> end;
};
struct GameplayPresentationMap final {
    std::uint64_t projectionScope;
    std::vector<GameplayPresentationSource> sources;
};
enum class GameplayProgramPolicy : std::uint8_t { Locked, Extendable, Open };
enum class GameplayOutcomeScope : std::uint8_t { Declared, Extended };
enum class GameplayArithmetic : std::uint8_t { Checked, Clamp };
enum class GameplayHookConsumer : std::uint8_t { Fold, Kernel, Shared };
enum class GameplayCombine : std::uint8_t { Maximum, BitOr };
struct GameplayModule final {
    std::string id, revision, build;
};
struct GameplayHook final {
    std::string target;
    GameplayHookConsumer consumer;
    GameplayCombine combine;
    std::vector<std::string> contributors;
    std::uint64_t value;
    bool stepRoute;
};
struct GameplayCapabilityRecord final {
    std::string semanticKind, requiredFormat, staticBudget, snapshotCost, replayImpact;
    std::vector<std::string> supportedDomains;
    std::string stableRejectCode;
};
struct GameplayCapability final {
    std::string id, revision, build;
    GameplayCapabilityRecord record;
};
enum class GameplayCapabilityState : std::uint8_t { Unknown, Disabled, Insufficient, Available };
struct GameplayCapabilityQuery final {
    std::string id, revision;
    GameplayCapabilityState state;
    std::optional<core::Error> diagnostic;
};
[[nodiscard]] CUEXIS_PLAYBACK_API auto gameplayCapabilities()
    -> core::Result<std::vector<GameplayCapability>>;

struct GameplayConfiguration final {
    // Explicit compile identity declarations, in the ABI table order.
    std::array<std::string, 6> engine;
    std::string rulesetInterfaceProjection, rulesetBuild;
    std::vector<std::string> rulesetModuleOrder;
    std::array<std::string, 4> session;
    std::vector<std::string> enabledCapabilities;
    std::string mappingId, mappingVersion, sourceClass, calibration;
    std::vector<GameplayInputDomain> domains;
    std::string loadout;
    std::string actualRulesetInterface, actualRulesetRevision, actualRulesetBuild;
    std::vector<GameplayModule> modules;
    GameplayProgramPolicy programPolicy;
    GameplayOutcomeScope outcomeScope;
    GameplayArithmetic arithmetic;
    std::vector<GameplayHook> hooks;
    bool externalPackage, life;
    std::int64_t initialScore, minimumScore, maximumScore;
    std::uint64_t initialCombo;
    std::vector<GameplayScoreRule> scoreRules;
    std::vector<GameplayFactBinding> bindings;
    // Numeric codec budgets are not production commitments.
    bool testOnly;
};
struct GameplayGraphDecodeBudget final {
    std::size_t maxBytes, maxDepth, maxStringBytes, maxValues, maxContainerElements;
    std::size_t maxRowAtoms, maxRowDecodedStringBytes;
};
struct GameplayConfigurationDecodeBudget final {
    std::size_t maxBytes, maxDepth, maxStringBytes, maxValues, maxContainerElements;
    bool testOnly;
};
[[nodiscard]] CUEXIS_PLAYBACK_API auto encodeGameplayConfiguration(const GameplayConfiguration&)
    -> core::Result<std::string>;
[[nodiscard]] CUEXIS_PLAYBACK_API auto
    decodeGameplayConfiguration(std::string_view, GameplayConfigurationDecodeBudget)
        -> core::Result<GameplayConfiguration>;
namespace detail {
struct GameplayContentStorage;
struct GameplayResultStorage;
struct GameplayReplayStorage;
struct GameplaySnapshotStorage;
struct GameplayCheckpointStorage;
struct GameplayAccess;
} // namespace detail
class CUEXIS_PLAYBACK_API GameplayContent final {
  public:
    GameplayContent() noexcept = default;
    [[nodiscard]] static auto fromPacked(std::span<const std::byte>, const GameplayConfiguration&)
        -> core::Result<GameplayContent>;
    [[nodiscard]] static auto fromGraph(std::string_view, const GameplayConfiguration&,
                                        GameplayGraphDecodeBudget) -> core::Result<GameplayContent>;
    [[nodiscard]] auto valid() const noexcept -> bool;

  private:
    friend struct detail::GameplayAccess;
    friend class PlaybackSession;
    std::shared_ptr<const detail::GameplayContentStorage> storage_;
};
struct GameplayScore final {
    std::int64_t score;
    std::uint64_t combo, maxCombo, hits, misses;
};
class CUEXIS_PLAYBACK_API GameplayResult final {
  public:
    GameplayResult() noexcept = default;
    [[nodiscard]] auto state() const -> core::Result<GameplayState>;
    [[nodiscard]] auto score() const -> core::Result<GameplayScore>;
    [[nodiscard]] auto factCount() const -> core::Result<std::uint64_t>;
    [[nodiscard]] auto publishableFactCount() const -> core::Result<std::uint64_t>;
    [[nodiscard]] auto sameResult(const GameplayResult&) const -> core::Result<bool>;

  private:
    friend struct detail::GameplayAccess;
    std::shared_ptr<const detail::GameplayResultStorage> storage_;
};
struct GameplayCodecBudget final {
    std::uint64_t maxBytes, maxRecords, maxElements;
    bool testOnly;
};
struct GameplayReplayCut final {
    std::uint64_t completeRecords;
    std::optional<GameplayTick> partialHorizon;
};
class CUEXIS_PLAYBACK_API GameplayCheckpoint final {
  public:
    GameplayCheckpoint() noexcept = default;
    [[nodiscard]] auto valid() const noexcept -> bool;

  private:
    friend struct detail::GameplayAccess;
    std::shared_ptr<const detail::GameplayCheckpointStorage> storage_;
};
class CUEXIS_PLAYBACK_API GameplayReplay final {
  public:
    GameplayReplay() noexcept = default;
    [[nodiscard]] static auto fromBytes(std::span<const std::byte>, const GameplayContent&,
                                        GameplayCodecBudget) -> core::Result<GameplayReplay>;
    [[nodiscard]] auto toBytes() const -> core::Result<std::vector<std::byte>>;
    [[nodiscard]] auto cutAt(GameplayTick horizon) const -> core::Result<GameplayReplayCut>;
    [[nodiscard]] auto valid() const noexcept -> bool;

  private:
    friend struct detail::GameplayAccess;
    std::shared_ptr<const detail::GameplayReplayStorage> storage_;
};
class CUEXIS_PLAYBACK_API GameplaySnapshot final {
  public:
    GameplaySnapshot() noexcept = default;
    [[nodiscard]] static auto fromBytes(std::span<const std::byte>, const GameplayContent&,
                                        GameplayCodecBudget) -> core::Result<GameplaySnapshot>;
    [[nodiscard]] auto toBytes() const -> core::Result<std::vector<std::byte>>;
    [[nodiscard]] auto valid() const noexcept -> bool;

  private:
    friend struct detail::GameplayAccess;
    std::shared_ptr<const detail::GameplaySnapshotStorage> storage_;
};
struct GameplayReplayEvaluation final {
    bool evidenceValid;
    GameplayResult result;
};
struct GameplayAdvanceReceipt final {
    GameplayTick requestedHorizon;
    std::optional<GameplayTick> processedFrontier, lastAdvanceHorizon, failedTick;
    std::uint64_t sealedFacts, publishableFacts;
    bool runtimeUpdated;
    std::optional<GameplayFaultStage> faultStage;
    std::optional<core::Error> error;
    bool admitted{true};
    std::optional<GameplayTick> foldedWorkTick;
    std::optional<GameplayPublicationFailureStage> publicationFailureStage;
};
enum class GameplayPrepareIntent : std::uint8_t { Presentation, GameplayOnly };
enum class GameplayControl : std::uint8_t { Pause, Resume, Reset, Stop };
CUEXIS_ABI_WARNING_POP
} // namespace cuexis::playback
