// T-10 fourth round: runtime prepared identity bytes against RR-F132.
//
// The runtime generator composes the identity from the prepared chart-projection bytes plus the
// execution session bytes. The test uses the same declarations the kernel prepare derives (ruleset
// projection token, build hash, module order, loadout, grade suffix) and asserts that the runtime
// bytes equal the legacy `makeRuntimePreparedIdentity` output and the pinned F132 lengths.

#include <cuexis/judgement/compile/runtime_identity_inputs.hpp>

#include <catch2/catch_test_macros.hpp>

#include "fold_recovery_fixture.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace {

using namespace cuexis::judgement;

auto kernelDeclarations(const PreparedGameplay& prepared, const SessionConfiguration& configuration,
                        bool graded) -> PreparedIdentityDeclarations {
    auto declarations = prepared.identityDeclarations();
    declarations.engine.judgementSemanticRevision += ";gameplay.fold.finite.v1";
    if (graded) {
        declarations.engine.factSemanticRevision += ";grade.signed-interval.v1";
    }
    const auto& ruleset = configuration.ruleset->declaration();
    declarations.ruleset.interfaceProjectionToken = rulesetIdentityToken(*configuration.ruleset);
    declarations.ruleset.buildHash = ruleset.compiledBuild;
    declarations.ruleset.moduleOrder.clear();
    for (const auto& module : ruleset.modules) {
        declarations.ruleset.moduleOrder.push_back(module.id);
    }
    declarations.session.loadoutToken = ruleset.loadoutId;
    return declarations;
}

} // namespace

TEST_CASE("cuexis_runtime_prepared_identity", "[judgement][identity][runtime]") {
    for (const bool graded : {false, true}) {
        fold_fixture::FoldFixture fixture;
        if (graded) {
            auto& component =
                fixture.assembly.sources[0].document.requirements[0].measure.components[0];
            component.declaredGradeTokens = {"tight"};
            component.gradeTable = std::vector<GradeInterval>{{-50, -1, "tight"}, {0, 0, "tight"}};
            fixture.rules.score.rules.push_back({PhaseKind::head, Outcome::hit, "tight", 2, true});
        }
        const auto prepared = fixture.prepared();
        const auto configuration = fixture.configuration();
        REQUIRE(configuration.ruleset);
        const auto declarations = kernelDeclarations(prepared, configuration, graded);
        const auto mapping = configuration.inputMapping.view();
        const auto& late = configuration.latePolicy;
        const auto calibration = configuration.calibrationIdentityToken;

        const auto legacy = makeRuntimePreparedIdentity(prepared.assembled().graph, declarations,
                                                        mapping, late, calibration);
        auto runtime =
            compile::runtimePreparedIdentity(prepared, declarations, mapping, late, calibration);
        REQUIRE(runtime);
        CHECK(runtime->bytes() == legacy.canonicalBytes().bytes());
        CHECK(runtime->toHex() == legacy.canonicalBytes().toHex());
        CHECK(runtime->bytes().size() == (graded ? 3592U : 3425U));

        //  The chart projection is a real prefix of the legacy prepared identity.
        auto projection = compile::chartProjectionBytes(prepared);
        REQUIRE(projection);
        CHECK_FALSE(projection->empty());
        CHECK(projection->size() < runtime->bytes().size());
    }
}
