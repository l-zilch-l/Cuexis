//  S7A-1 boundary tests: the compile-time half of this batch's guarantees.
//
//  These assertions are the machine-checkable form of "no width, no enumeration value, no layout,
//  no default value, no temporary integer typedef":
//
//    * every declared role is incomplete. An incomplete type has no size, no member, no layout and
//      no serialization encoding, and it cannot be an alias for an integer type or any other
//      complete type, because an alias would be complete;
//    * the output carriers are self-owning value types with no invented empty state;
//    * the session is a move-only single owner whose destructor does not throw, and whose only
//      read-back is noexcept.

#include <catch2/catch_test_macros.hpp>

#include <cuexis/judgement/diagnostic.hpp>
#include <cuexis/judgement/input_boundary.hpp>
#include <cuexis/judgement/judgement_session.hpp>
#include <cuexis/judgement/role_boundary.hpp>

#include <type_traits>
#include <utility>

namespace {

namespace judgement = cuexis::judgement;

//  Satisfied only when the size of T is known, that is only for a complete type.
template <typename T>
concept Complete = requires { sizeof(T); };

} // namespace

//  Input and time roles.
static_assert(!Complete<judgement::roles::Tick>);
static_assert(!Complete<judgement::roles::TimeInterval>);
static_assert(!Complete<judgement::roles::TickSpan>);
static_assert(!Complete<judgement::roles::TickDelta>);
static_assert(!Complete<judgement::roles::NormalizedObservation>);
static_assert(!Complete<judgement::roles::InputDomain>);
static_assert(!Complete<judgement::roles::InputAction>);
static_assert(!Complete<judgement::roles::ChannelRef>);
static_assert(!Complete<judgement::roles::DomainAmount>);
static_assert(!Complete<judgement::roles::SourceClass>);
static_assert(!Complete<judgement::roles::EventSequence>);

//  Judgement-side roles.
static_assert(!Complete<judgement::roles::RequirementRecord>);
static_assert(!Complete<judgement::roles::PatternRef>);
static_assert(!Complete<judgement::roles::MeasureSpec>);
static_assert(!Complete<judgement::roles::PreparedGrace>);
static_assert(!Complete<judgement::roles::GraceResolutionPolicy>);
static_assert(!Complete<judgement::roles::ResourceClaimIntent>);
static_assert(!Complete<judgement::roles::FactRecord>);
static_assert(!Complete<judgement::roles::JudgementResult>);
static_assert(!Complete<judgement::roles::JudgementIdentity>);
static_assert(!Complete<judgement::roles::DiagnosticCode>);
static_assert(!Complete<judgement::roles::DiagnosticCategory>);
static_assert(!Complete<judgement::roles::DiagnosticSeverity>);
static_assert(!Complete<judgement::roles::JudgementError>);
static_assert(!Complete<judgement::roles::CapabilityId>);
static_assert(!Complete<judgement::roles::CapabilityRecord>);
static_assert(!Complete<judgement::roles::CapabilityState>);

//  The roles are distinct declarations, not aliases of one another.
static_assert(!std::is_same_v<judgement::roles::Tick, judgement::roles::EventSequence>);
static_assert(
    !std::is_same_v<judgement::roles::PreparedGrace, judgement::roles::GraceResolutionPolicy>);
static_assert(
    !std::is_same_v<judgement::roles::DiagnosticCode, judgement::roles::DiagnosticCategory>);

//  The boundary carriers are complete on purpose; their representation is pimpl, so completeness
//  commits no field table.
static_assert(Complete<judgement::JudgementSession>);
static_assert(Complete<judgement::JudgementProjection>);
static_assert(Complete<judgement::SnapshotPayload>);
static_assert(Complete<judgement::Diagnostic>);
static_assert(Complete<judgement::DiagnosticContext>);
static_assert(Complete<judgement::DiagnosticFieldPath>);
static_assert(Complete<judgement::DiagnosticRequirementRef>);
static_assert(Complete<judgement::DiagnosticIdentityComponent>);

//  Ownership: a session has exactly one owner and cannot be copied, while the output carriers are
//  self-owning values that never reference session storage.
static_assert(!std::is_copy_constructible_v<judgement::JudgementSession>);
static_assert(!std::is_copy_assignable_v<judgement::JudgementSession>);
static_assert(std::is_move_constructible_v<judgement::JudgementSession>);
static_assert(std::is_move_assignable_v<judgement::JudgementSession>);
static_assert(!std::is_default_constructible_v<judgement::JudgementSession>);
static_assert(std::is_copy_constructible_v<judgement::JudgementProjection>);
static_assert(std::is_copy_assignable_v<judgement::JudgementProjection>);
static_assert(std::is_copy_constructible_v<judgement::SnapshotPayload>);
static_assert(!std::is_default_constructible_v<judgement::JudgementProjection>);
static_assert(!std::is_default_constructible_v<judgement::SnapshotPayload>);

//  Exception boundary: a destructor and a real-time-adjacent read must not throw. The verbs that
//  build a diagnostic are not noexcept because building the owning error text allocates; they
//  never throw a Cuexis exception and report every recoverable failure through Result.
static_assert(std::is_nothrow_move_constructible_v<judgement::JudgementSession>);
static_assert(std::is_nothrow_move_assignable_v<judgement::JudgementSession>);
static_assert(std::is_nothrow_destructible_v<judgement::JudgementSession>);
static_assert(std::is_nothrow_destructible_v<judgement::JudgementProjection>);
static_assert(std::is_nothrow_destructible_v<judgement::SnapshotPayload>);
static_assert(noexcept(std::declval<const judgement::JudgementSession&>().hasPreparedState()));

//  The budget context interface is abstract, has a virtual destructor and exposes no number.
static_assert(std::is_abstract_v<judgement::BudgetContext>);
static_assert(std::has_virtual_destructor_v<judgement::BudgetContext>);
static_assert(std::is_nothrow_destructible_v<judgement::BudgetContext>);

TEST_CASE("the typed boundary is usable without a device", "[judgement][s7a-1][boundary]") {
    auto created = judgement::JudgementSession::create();
    REQUIRE(created.has_value());

    judgement::JudgementSession session = std::move(created).value();
    CHECK_FALSE(session.hasPreparedState());
}
