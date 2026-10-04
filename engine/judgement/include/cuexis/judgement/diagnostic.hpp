#pragma once

//  Judgement typed kernel - S7A-1 diagnostic carriers.
//
//  S7A-1 must establish the carrier structure of a diagnostic without freezing a single
//  diagnostic value. The Gameplay V2 ABI fixes the diagnostic layering (code / category /
//  severity / faulted) and states that the machine-readable code table, the category set and the
//  severity set are unresolved: they belong to round 6 (CM-D04 / P2-04 / P2-06) and their
//  creation is a separate accepted work item. Therefore:
//
//    * This header fixes the carriers: the field names, their order and what each one addresses.
//    * It fixes no code, no category, no severity and no code table. Every value carrier is an
//      opaque token supplied by whoever produces the diagnostic.
//    * category and severity stay opaque tokens instead of enumerations on purpose: an
//      enumeration would freeze the set that CM-D04 owns. Their final form replaces these tokens
//      when that ruling lands.
//    * A concrete diagnostic value produced inside this module lives in a .cpp file, never in a
//      header. The ABI forbids writing a code that is not registered in the code table into a
//      public header, and no code is registered yet.
//
//  The code is the only stable identity of a diagnostic. Field path, requirement component,
//  identity component, capability id, remediation and budget context are context: they are
//  attached for diagnosis and must not take part in code equivalence.
//
//  Absence convention: an empty token means "this context component does not apply to this
//  diagnostic". That is a documented absence marker, not a default value for an unfrozen role.

#include <cuexis/core/error.hpp>

#include <string_view>

namespace cuexis::judgement {

//  Where in the typed contract a diagnostic points. Both tokens are context, not stable codes.
struct DiagnosticFieldPath final {
    std::string_view section;
    std::string_view path;
};

//  Which requirement a diagnostic refers to: the requirement role token plus an opaque identity
//  token. The six-component RequirementIdentity stays a role until its own batch freezes it, so
//  the carrier keeps one opaque token instead of inventing its components.
struct DiagnosticRequirementRef final {
    std::string_view kind;
    std::string_view identity;
};

//  Which identity component a diagnostic refers to, and the opaque token for it. The three
//  parallel identity judgements (semantic / artifact / interchange) must stay separately
//  expressible, so this carrier names one component and never collapses them into one equality.
struct DiagnosticIdentityComponent final {
    std::string_view component;
    std::string_view token;
};

//  Budget context interface. Production budgets, upper bounds, limits and measured values are
//  unfrozen until S7A-9, so this interface exposes opaque descriptors only and deliberately has
//  no numeric accessor: a number here would become an ABI constant without a measurement.
//
//  Implementations are called while a diagnostic is being projected into the error channel, so
//  every accessor is noexcept and must not throw.
class BudgetContext {
  public:
    BudgetContext() noexcept = default;
    virtual ~BudgetContext() noexcept = default;
    BudgetContext(const BudgetContext&) = delete;
    auto operator=(const BudgetContext&) -> BudgetContext& = delete;

    //  Opaque budget domain token: the contract section or judgement domain the budget belongs to.
    [[nodiscard]] virtual auto budgetDomain() const noexcept -> std::string_view = 0;
    //  Opaque budget subject token: the requirement, session or artifact that would be measured.
    [[nodiscard]] virtual auto budgetSubject() const noexcept -> std::string_view = 0;
};

struct DiagnosticContext final {
    DiagnosticFieldPath fieldPath;
    DiagnosticRequirementRef requirement;
    DiagnosticIdentityComponent identity;
    //  Non-owning. A null pointer means this diagnostic carries no budget context. No bound value
    //  is carried in either case, because budget numbers are unfrozen.
    const BudgetContext* budget;
    //  Opaque capability token. Empty when the rejection is not a capability rejection. A
    //  capability that is unknown, disabled or permanently unsupported stays itself here; it is
    //  never rewritten into an older kind.
    std::string_view capabilityId;
    //  Stable alternative-path token. A capability rejection must name a replacement capability;
    //  a lifecycle rejection has no alternative and leaves this empty.
    std::string_view remediation;
    //  Opaque token for one raw diagnostic timestamp. Spec 3.7.3 keeps device time, host arrival
    //  time, audio time and render frame time out of judgement time location and admits them as
    //  diagnostic context only; this member is where that context travels. It is an opaque
    //  string_view and never a tick, so a raw timestamp cannot be read back as a judgement time.
    std::string_view rawTime;
};

//  The diagnostic carrier. `summary` is readable text for humans; it may be localized and must
//  never be used for branching. Branching is on `code` only.
struct Diagnostic final {
    std::string_view code;
    std::string_view category;
    std::string_view severity;
    //  Whether this failure would put the session into the queriable faulted state.
    bool faulted;
    std::string_view summary;
    DiagnosticContext context;
};

//  Projects a carrier into the recoverable error channel. The error owns copies of every token,
//  so the diagnostic only has to outlive this call. Context components that do not apply are not
//  attached at all, and the budget context is read through its interface.
//
//  Not marked noexcept on purpose: building the owning error text allocates, and allocation
//  failure is not a recoverable Cuexis diagnostic. The function itself never throws a Cuexis
//  exception and never lets an exception cross the module boundary.
[[nodiscard]] auto toError(const Diagnostic& diagnostic) -> core::Error;

} // namespace cuexis::judgement
