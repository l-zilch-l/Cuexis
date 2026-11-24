#include "fold_recovery_fixture.hpp"

#include <cstddef>
#include <string>
#include <vector>

using namespace fold_fixture;

// RR-F132 baseline record.
//
// The contract cites two identity variants of the same fixture: one without a grade table and one
// with `;grade.signed-interval.v1`. T-10 changes the identity fields, so it needs the canonical
// bytes the current implementation produces. Run this case with `-s` and keep its output next to
// the change; this record does not prove any historical release value, it only pins the value that
// exists before the migration.

TEST_CASE("rr_f132_identity_baseline", "[judgement][identity][f132][baseline]") {
    for (const bool graded : {false, true}) {
        FoldFixture f;
        if (graded) {
            auto& component = f.assembly.sources[0].document.requirements[0].measure.components[0];
            component.declaredGradeTokens = {"tight"};
            component.gradeTable = std::vector<GradeInterval>{{-50, -1, "tight"}, {0, 0, "tight"}};
            f.rules.score.rules.push_back({PhaseKind::head, Outcome::hit, "tight", 2, true});
        }
        const auto prepared = f.prepared();
        const auto configuration = f.configuration();
        REQUIRE(configuration.ruleset);
        auto session = JudgementSession::create();
        REQUIRE(session);
        REQUIRE(session->configure(configuration));
        REQUIRE(session->prepare(prepared));
        const auto query = session->query();
        REQUIRE(query);
        const auto& identity = query->kernelView().judgementIdentity;
        INFO("graded=" << graded << " identity=" << identity.canonicalBytes().toHex());
        CHECK(identity.canonicalBytes().bytes().size() > 32U);
    }
}
