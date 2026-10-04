#pragma once

#include <cuexis/judgement/gameplay_assembler.hpp>

#include <memory>
#include <span>

namespace cuexis::judgement {

struct RequirementGraceInput final {
    StableDeclarationId requirement;
    GraceResolutionInputs inputs;
};

// Named sources are frozen literals, never another inheritance declaration.
struct NamedGraceDuration final {
    std::string declarationId;
    RationalDuration duration;
};

struct GameplayPrepareRequest final {
    AssemblyRequest assembly;
    std::vector<RequirementGraceInput> graceInputs;
    std::vector<NamedGraceDuration> namedGraceDurations;
    PatternCompileBudget patternBudget;
};

// Resolved artifacts carry frozen tick values, not author-duration resolver inputs.
struct ResolvedGameplayPrepareRequest final {
    AssemblyRequest assembly;
    PatternCompileBudget patternBudget;
};

struct PreparedRequirement final {
    StableDeclarationId stableId;
    CompiledPattern pattern;
    CompiledMeasure measure;
    Tick deadline;
};

struct PreparedResource final {
    std::string resourceId;
    ResourceClaimResolution claims;
    // Canonical graph requirement indices in exactly the same order as claims.candidates().
    std::vector<std::size_t> requirementIndices;
};

namespace detail {
class PreparedGameplayStorage;
}

// An owning immutable publication. Even the profile string_views are owned by the storage.
class PreparedGameplay final {
  public:
    [[nodiscard]] auto assembled() const noexcept -> const AssembledGameplay&;
    [[nodiscard]] auto requirements() const noexcept -> std::span<const PreparedRequirement>;
    [[nodiscard]] auto resources() const noexcept -> std::span<const PreparedResource>;
    // Pure prepared-window query, not a runtime state transition. At D the timer wins.
    [[nodiscard]] auto admitsSuccess(std::size_t requirementIndex, Tick tick) const noexcept
        -> bool;

  private:
    explicit PreparedGameplay(std::shared_ptr<const detail::PreparedGameplayStorage> storage)
        : storage_(std::move(storage)) {}
    friend auto prepareGameplay(const GameplayPrepareRequest&) -> core::Result<PreparedGameplay>;
    friend auto prepareResolvedGameplay(const ResolvedGameplayPrepareRequest&)
        -> core::Result<PreparedGameplay>;
    std::shared_ptr<const detail::PreparedGameplayStorage> storage_;
};

[[nodiscard]] auto prepareGameplay(const GameplayPrepareRequest& request)
    -> core::Result<PreparedGameplay>;

[[nodiscard]] auto prepareResolvedGameplay(const ResolvedGameplayPrepareRequest& request)
    -> core::Result<PreparedGameplay>;

class PreparedGameplayPublication final {
  public:
    [[nodiscard]] auto active() const noexcept -> const PreparedGameplay* {
        return active_ ? &*active_ : nullptr;
    }

  private:
    friend auto prepareInto(PreparedGameplayPublication&, const GameplayPrepareRequest&)
        -> core::Result<void>;
    friend auto prepareResolvedInto(PreparedGameplayPublication&,
                                    const ResolvedGameplayPrepareRequest&) -> core::Result<void>;
    std::optional<PreparedGameplay> active_;
};

[[nodiscard]] auto prepareInto(PreparedGameplayPublication& publication,
                               const GameplayPrepareRequest& request) -> core::Result<void>;

[[nodiscard]] auto prepareResolvedInto(PreparedGameplayPublication& publication,
                                       const ResolvedGameplayPrepareRequest& request)
    -> core::Result<void>;

} // namespace cuexis::judgement
