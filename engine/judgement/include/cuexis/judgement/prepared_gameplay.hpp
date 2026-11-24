#pragma once

//  Judgement runtime - the owning prepared gameplay handle.
//
//  `PreparedGameplay` is a copyable, non-default-constructible immutable handle over one
//  `detail::PreparedGameplayStorage`. The storage owns every record, token and identity the running
//  kernel reads; a successful `prepareCompiledGameplay` never borrows from the source view. A
//  source view may therefore be destroyed immediately after the call and the handle keeps working.
//
//  The handle is in `cuexis::judgement::runtime`; the legacy `cuexis::judgement::PreparedGameplay`
//  keeps its existing declaration and ABI. The two are adapted explicitly, never reinterpreted.

#include <cuexis/judgement/advance.hpp>
#include <cuexis/judgement/prepared_gameplay_records.hpp>

#include <cstddef>
#include <memory>
#include <span>
#include <string_view>

namespace cuexis::judgement::runtime {

namespace detail {
class PreparedGameplayStorage;
}

class PreparedGameplay final {
  public:
    PreparedGameplay(const PreparedGameplay&) noexcept = default;
    PreparedGameplay(PreparedGameplay&&) noexcept = default;
    auto operator=(const PreparedGameplay&) noexcept -> PreparedGameplay& = default;
    auto operator=(PreparedGameplay&&) noexcept -> PreparedGameplay& = default;
    ~PreparedGameplay() noexcept = default;

    //  The borrowed view of the owned records. The view is valid while this handle (or any copy of
    //  it) is alive.
    [[nodiscard]] auto records() const noexcept -> CompiledGameplayView;
    [[nodiscard]] auto requirements() const noexcept -> std::span<const PreparedRequirement>;
    [[nodiscard]] auto resources() const noexcept -> std::span<const PreparedResource>;
    [[nodiscard]] auto timers() const noexcept -> std::span<const PreparedTimerKey>;
    [[nodiscard]] auto identityDeclarations() const noexcept -> const PreparedIdentityDeclarations&;
    [[nodiscard]] auto identity() const noexcept -> std::string_view;
    //  The exact chart-projection bytes carried by the source content, or an empty span when the
    //  source did not carry one. The runtime identity generator needs it.
    [[nodiscard]] auto chartProjection() const noexcept -> std::span<const std::byte>;

    //  Pure prepared-window query, not a runtime state transition. At the deadline the timer wins,
    //  so a tick equal to the deadline is not admitted.
    [[nodiscard]] auto admitsSuccess(std::size_t requirementIndex, Tick tick) const noexcept
        -> bool;

    //  The number of phase instances, which is the exact settlement slot count P of one call.
    [[nodiscard]] auto settlementCapacity() const noexcept -> std::size_t;

    //  The owning dictionary of this prepared run. The phases are in requirement and phase
    //  declaration order; the grades are distinct table identifiers in UTF-8 byte order.
    [[nodiscard]] auto settlementDictionary() const noexcept
        -> const cuexis::judgement::SettlementDictionary&;

  private:
    explicit PreparedGameplay(std::shared_ptr<const detail::PreparedGameplayStorage> storage);

    friend auto prepareCompiledGameplay(const CompiledGameplayView& view,
                                        const PrepareGameplayLimits& limits)
        -> core::Result<PreparedGameplay>;

    std::shared_ptr<const detail::PreparedGameplayStorage> storage_;
};

//  Validates a borrowed, fully compiled view and publishes it as an owning prepared handle. On
//  failure the previous prepared handle (if any) is untouched; there is no partial publication.
[[nodiscard]] auto prepareCompiledGameplay(const CompiledGameplayView& view,
                                           const PrepareGameplayLimits& limits)
    -> core::Result<PreparedGameplay>;

} // namespace cuexis::judgement::runtime
