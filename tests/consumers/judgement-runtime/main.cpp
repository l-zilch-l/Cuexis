// Independent runtime consumer of the judgement library. It links Core and Judgement only, so it
// exercises exactly the runtime value headers and the shared score helpers a host that has no
// compile-side, Chart or Playback dependency can use.
#include <cuexis/judgement/judgement_session.hpp>
#include <cuexis/judgement/normalized_score.hpp>
#include <cuexis/judgement/phase_types.hpp>
#include <cuexis/judgement/prepared_gameplay.hpp>
#include <cuexis/judgement/prepared_gameplay_records.hpp>
#include <cuexis/judgement/prepared_identity.hpp>

#include <cstdint>
#include <iostream>
#include <string>

namespace {

auto fail(const char* message) -> int {
    std::cerr << "rr_judgement_runtime: " << message << '\n';
    return 1;
}

} // namespace

int main() {
    using namespace cuexis::judgement;

    //  The four identity declarations are complete value types and compare by value.
    PreparedIdentityDeclarations declarations;
    declarations.engine.judgementSemanticRevision = "judgement.t4-k4.v1";
    declarations.engine.factSemanticRevision = "fact.semantic.phase-local.v1";
    declarations.engine.fixedPointTableId = "fixed-point.none.v1";
    declarations.engine.coordinationPhaseOrderToken = "coordination.six-eight.t4-k4.v1";
    declarations.ruleset.interfaceProjectionToken = "ruleset.one";
    declarations.session.judgementConfigToken = "judgement.config.one";
    auto copy = declarations;
    if (copy != declarations) {
        return fail("the identity declarations do not compare by value");
    }

    //  The phase and declaration values are constructible without a compile-side header.
    StableDeclarationId declaration{"consumer.document", 3};
    if (declaration.declarationOrdinal != 3U) {
        return fail("the stable declaration id lost its ordinal");
    }
    if (factCategoryOfPhase(PhaseKind::body) != FactCategory::holdBody) {
        return fail("the phase derived the wrong fact category");
    }

    //  The shared exact score helper produces the GP-05 boundary result.
    const auto half = normalizedScoreOutput(1, 0, 1, 1, 2, 0, 0);
    if (!half || *half != 0U) {
        return fail("the exact half tie did not round to even");
    }
    const auto total = normalizedScoreOutput(900000, 100000, 1000000, 8, 8, 3, 3);
    if (!total || *total != 1000000U) {
        return fail("the full normalized score did not reach the total");
    }

    //  The runtime prepared records and the shared observation-domain helper are usable without an
    //  author header: the consumer builds a requirement value and asks the domain helper directly.
    cuexis::judgement::runtime::PreparedRequirement requirement;
    requirement.stableId = declaration;
    requirement.phases = {PhaseDeclaration{PhaseKind::tap, 0}};
    if (requirement.phases.front().kind != PhaseKind::tap) {
        return fail("the runtime requirement lost its phase");
    }
    const auto domain = cuexis::judgement::runtime::releaseObservationDomain(850, 1000, 4);
    if (!domain || domain->start != 846 || domain->end != 1000) {
        return fail("the release observation domain is wrong");
    }

    //  The runtime session factory is reachable without any author or Playback type.
    auto session = JudgementSession::create();
    if (!session) {
        return fail("JudgementSession::create did not return a session");
    }

    std::cout << "rr_judgement_runtime: ok\n";
    return 0;
}
