#pragma once

#include <cuexis/judgement/gameplay_prepare.hpp>
#include <variant>

namespace cuexis::judgement {

enum class ProgramPolicy : std::uint8_t { locked, extendable, open };
enum class OutcomeScope : std::uint8_t { declared, extended };
enum class ArithmeticPolicy : std::uint8_t { checked, clamp };
enum class RegisterKind : std::uint8_t { exclusive, commutativeMonoid, ledgerDerived };
enum class CombineOperator : std::uint8_t { maximum, bitOr };
enum class HookConsumer : std::uint8_t { fold, kernel, shared };
enum class RegisterValueTag : std::uint8_t { signed64, unsigned64 };
using RegisterValue = std::variant<std::int64_t, std::uint64_t>;
struct RegisterDeclaration final {
    std::string target;
    RegisterKind kind;
    RegisterValueTag valueTag;
    std::string owner;
    std::vector<std::string> contributors;
    CombineOperator combine;
};
struct RegisterContribution final {
    std::string target, owner;
    std::uint64_t commitId, factOrdinal, localOrdinal;
    RegisterValue value;
};
[[nodiscard]] auto commitRegister(const RegisterDeclaration&, std::span<const RegisterContribution>,
                                  RegisterValue derived) -> core::Result<RegisterValue>;

struct ScoreRule final {
    PhaseKind phase;
    Outcome outcome;
    std::optional<std::string> grade;
    std::int64_t delta;
    bool incrementsCombo;
    friend auto operator==(const ScoreRule&, const ScoreRule&) -> bool = default;
};
struct ScoreConfiguration final {
    std::int64_t initial, minimum, maximum;
    ArithmeticPolicy arithmetic;
    std::uint64_t initialCombo;
    std::vector<ScoreRule> rules;
    friend auto operator==(const ScoreConfiguration&, const ScoreConfiguration&) -> bool = default;
};
struct ModuleDeclaration final {
    std::string id, revision, build;
    friend auto operator==(const ModuleDeclaration&, const ModuleDeclaration&) -> bool = default;
};
struct HookDeclaration final {
    std::string target;
    HookConsumer consumer;
    CombineOperator combine;
    std::vector<std::string> contributors;
    std::uint64_t value;
    bool stepRoute;
    friend auto operator==(const HookDeclaration&, const HookDeclaration&) -> bool = default;
};
struct RulesetDeclaration final {
    std::string interfaceId, interfaceRevision, compiledBuild, loadoutId;
    ProgramPolicy programPolicy;
    OutcomeScope outcomeScope;
    std::vector<ModuleDeclaration> modules;
    ScoreConfiguration score;
    std::vector<HookDeclaration> hooks;
    bool externalPackage, life;
    friend auto operator==(const RulesetDeclaration&, const RulesetDeclaration&) -> bool = default;
};
struct HookValue final {
    std::string target;
    Tick visibleTick;
    std::uint64_t value;
    friend auto operator==(const HookValue&, const HookValue&) -> bool = default;
};
struct StatisticsCount final {
    PhaseKind phase;
    Outcome outcome;
    std::optional<std::string> grade;
    std::uint64_t count;
    friend auto operator==(const StatisticsCount&, const StatisticsCount&) -> bool = default;
};
struct FoldProjection final {
    std::int64_t score;
    std::uint64_t combo, maxCombo, hits, misses;
    std::uint64_t factCursor;
    std::optional<Tick> workTick;
    std::vector<HookValue> produced;
    std::uint64_t hookConsumerCursor;
    std::uint64_t bonus;
    std::uint64_t monoidValue;
    std::uint64_t ledgerDerivedCount;
    std::vector<StatisticsCount> counts;
    std::uint64_t strayCount{0}, consumeEmptyCount{0};
    friend auto operator==(const FoldProjection&, const FoldProjection&) -> bool = default;
};
class PreparedRuleset final {
  public:
    [[nodiscard]] auto declaration() const noexcept -> const RulesetDeclaration&;
    [[nodiscard]] auto initialState() const -> core::Result<FoldProjection>;

  private:
    friend auto prepareRuleset(const RulesetDeclaration&) -> core::Result<PreparedRuleset>;
    explicit PreparedRuleset(std::shared_ptr<const RulesetDeclaration> declaration)
        : declaration_(std::move(declaration)) {}
    std::shared_ptr<const RulesetDeclaration> declaration_;
};
[[nodiscard]] auto prepareRuleset(const RulesetDeclaration&) -> core::Result<PreparedRuleset>;
[[nodiscard]] auto checkedScore(std::int64_t current, std::int64_t delta, const ScoreConfiguration&)
    -> core::Result<std::int64_t>;
[[nodiscard]] auto rulesetIdentityToken(const PreparedRuleset&) -> std::string;
[[nodiscard]] auto validUtf8(std::string_view) noexcept -> bool;

} // namespace cuexis::judgement
