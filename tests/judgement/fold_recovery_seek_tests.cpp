#include "fold_recovery_fixture.hpp"

using namespace fold_fixture;

TEST_CASE("explicit new recovery preserves valid faulted DTO and mutation refusal",
          "[recovery][fault][lifecycle]") {
    FoldFixture f;
    f.tap();
    auto s = f.session();
    detail::KernelTestControls controls{
        Tick{5}, detail::KernelFailurePoint::beforeFold, {}, {}, {}};
    REQUIRE(detail::KernelTestAccess::inject(s, controls));
    REQUIRE(s.submit({raw(5, 1)}));
    CHECK_FALSE(s.advance(Tick{20}));
    CHECK_FALSE(s.snapshot());
    auto dto = detail::KernelTestAccess::captureDTO(s);
    REQUIRE(dto);
    auto inputs = std::make_shared<const RecoveryInputs>(f.inputs());
    auto recovered = detail::ExecutionKernel::restore({*dto, inputs}, *inputs);
    REQUIRE(recovered);
    CHECK(sameKernelResult((*recovered)->query()->projection, s.query()->kernelView()));
    CHECK_FALSE((*recovered)->advance(Tick{20}));
    CHECK_FALSE((*recovered)->submitCanonical({canonical(100, 2)}));
    CHECK_FALSE((*recovered)->snapshot());
}
TEST_CASE("partial checkpoint resumes remaining source control and preserves original archive",
          "[recovery][checkpoint][partial]") {
    FoldFixture f{true};
    f.tap();
    auto s = f.session();
    REQUIRE(s.submit({raw(5, 1)}));
    REQUIRE(s.advance(Tick{20}));
    auto archive = *s.archive();
    auto cp = JudgementSession::checkpoint(archive, Tick{5});
    REQUIRE(cp);
    REQUIRE(cp->cut.partialHorizon);
    std::vector<ReplayCheckpoint> checkpoints{*cp};
    CHECK_FALSE(s.seek(archive, Tick{5}, ReplayCut{1, {}}, checkpoints));
    REQUIRE(s.seek(archive, Tick{10}, replayCutAt(archive, Tick{10}), checkpoints));
    auto reference = f.session();
    REQUIRE(reference.seek(archive, Tick{10}));
    CHECK(sameSnapshot(s.snapshot()->state(), reference.snapshot()->state()));
    CHECK(sameReplayHistory(*s.archive(), archive));
    REQUIRE(s.advance(Tick{10}));
    REQUIRE(JudgementSession::evaluateReplay(*s.archive()));
    CHECK(archive.data().records.back().horizon == Tick{20});
}

TEST_CASE("R3 checkpoint cannot certify an externally mutated archive owner",
          "[recovery][checkpoint][r3][provenance]") {
    FoldFixture f;
    f.tap();
    auto live = f.session();
    REQUIRE(live.submitCanonical({canonical(1, 0)}));
    REQUIRE(live.advance(Tick{10}));
    auto original = live.archive();
    REQUIRE(original);
    auto mutableData = std::make_shared<ReplayData>(original->data());
    ReplayArchive external{mutableData, std::make_shared<const RecoveryInputs>(f.inputs())};
    auto checkpoint = JudgementSession::checkpoint(external, Tick{10});
    REQUIRE(checkpoint);
    // Valid shape/count/identity, but an admission no longer matches its batch.
    mutableData->records[0].batch[0] = canonical(2, 0);
    auto origin = f.session();
    auto accelerated = f.session();
    const auto old = accelerated.snapshot();
    REQUIRE(old);
    auto rejected = origin.seek(external, Tick{10});
    REQUIRE_FALSE(rejected);
    auto checked = accelerated.seek(external, Tick{10}, replayCutAt(external, Tick{10}),
                                    std::span{&*checkpoint, 1});
    REQUIRE_FALSE(checked);
    CHECK(sameRecoveryError(rejected.error(), checked.error()));
    CHECK(sameSnapshot(old->state(), accelerated.snapshot()->state()));
}

TEST_CASE("R3 Seek export cannot seal a mutable source through another checkpoint",
          "[recovery][checkpoint][r3][provenance]") {
    FoldFixture f;
    f.tap();
    auto first = f.session();
    REQUIRE(first.submitCanonical({canonical(1, 0)}));
    REQUIRE(first.advance(Tick{10}));
    auto second = f.session();
    REQUIRE(second.submitCanonical({canonical(2, 0)}));
    REQUIRE(second.advance(Tick{10}));
    const auto a = first.archive();
    const auto b = second.archive();
    REQUIRE(a);
    REQUIRE(b);
    auto mutableData = std::make_shared<ReplayData>(a->data());
    ReplayArchive external{mutableData, std::make_shared<const RecoveryInputs>(f.inputs())};
    auto middle = f.session();
    REQUIRE(middle.seek(external, Tick{10}));
    const auto exported = middle.archive();
    REQUIRE(exported);
    REQUIRE(exported->anchor() == external.anchor());
    auto checkpoint = JudgementSession::checkpoint(*exported, Tick{10});
    REQUIRE(checkpoint);
    // A different fully valid history, rather than merely an invalid admission.
    *mutableData = b->data();
    auto origin = f.session();
    auto checked = f.session();
    REQUIRE(origin.seek(*exported, Tick{10}));
    REQUIRE(checked.seek(*exported, Tick{10}, replayCutAt(*exported, Tick{10}),
                         std::span{&*checkpoint, 1}));
    CHECK(sameSnapshot(origin.snapshot()->state(), checked.snapshot()->state()));
    CHECK(sameKernelResult(second.query()->kernelView(), checked.query()->kernelView()));
    CHECK_FALSE(sameKernelResult(first.query()->kernelView(), checked.query()->kernelView()));
}
TEST_CASE("real body break error grades and timer Miss absence remain distinct",
          "[fold][grade][break]") {
    FoldFixture f;
    auto& r = f.assembly.sources[0].document.requirements[0];
    r.measure.components.push_back(
        {PhaseKind::body, "hold_body", {"break"}, std::vector<GradeInterval>{{-149, -1, "break"}}});
    f.rules.score.rules.push_back({PhaseKind::body, Outcome::miss, "break", -7, false});
    auto s = f.session();
    REQUIRE(s.submit({raw(900, 1), raw(950, 2, InputAction::release)}));
    auto p = s.advance(Tick{1103});
    REQUIRE(p);
    const auto broken = std::find_if(p->kernelView().facts.begin(), p->kernelView().facts.end(),
                                     [](const auto& fact) {
                                         const auto* f = std::get_if<PhaseOutcomeFact>(&fact);
                                         return f && f->phase == PhaseKind::body;
                                     });
    REQUIRE(broken != p->kernelView().facts.end());
    const auto& fact = std::get<PhaseOutcomeFact>(*broken);
    CHECK(fact.error == TickDelta{-50});
    CHECK(fact.grade == "break");
    CHECK(p->kernelView().fold->combo == 0);
    CHECK(p->kernelView().fold->maxCombo == 1);
    auto timer = f.session();
    auto miss = timer.advance(Tick{1103});
    REQUIRE(miss);
    for (const auto& item : miss->kernelView().facts) {
        const auto& timerFact = std::get<PhaseOutcomeFact>(item);
        CHECK_FALSE(timerFact.error);
        CHECK_FALSE(timerFact.grade);
    }
}

TEST_CASE("Snapshot binds coverage history and exact ingress ID state",
          "[recovery][snapshot][closure]") {
    FoldFixture f;
    auto s = f.session();
    REQUIRE(s.submit({raw(900, 1)}));
    REQUIRE(s.advance(Tick{1103}));
    auto value = s.snapshot();
    REQUIRE(value);
    auto inputs = std::make_shared<const RecoveryInputs>(f.inputs());
    SECTION("coverage") {
        auto dto = value->state();
        dto.coverageHistory.clear();
        CHECK_FALSE(detail::ExecutionKernel::restore({dto, inputs}, *inputs));
    }
    SECTION("exhaustion") {
        auto dto = value->state();
        dto.ingress.exhausted = true;
        CHECK_FALSE(detail::ExecutionKernel::restore({dto, inputs}, *inputs));
    }
    SECTION("ID jump") {
        auto dto = value->state();
        dto.ingress.nextId = UINT64_MAX;
        CHECK_FALSE(detail::ExecutionKernel::restore({dto, inputs}, *inputs));
    }
    REQUIRE(JudgementSession::recover(*value, *inputs));
}
TEST_CASE("Replay Reader rejects forged canonical journal counts and duplicate sequence",
          "[recovery][codec][negative]") {
    FoldFixture f;
    f.tap();
    auto s = f.session();
    REQUIRE(s.submit({raw(5, 1)}));
    REQUIRE(s.advance(Tick{20}));
    auto original = *s.archive();
    auto data = original.data();
    SECTION("count") {
        ++data.eventCount;
    }
    SECTION("duplicate") {
        data.records.insert(data.records.begin(), data.records.front());
        ++data.eventCount;
    }
    auto forged = ReplayArchive{std::make_shared<const ReplayData>(data),
                                std::make_shared<const RecoveryInputs>(f.inputs())};
    auto bytes = encodeReplay(forged);
    REQUIRE(bytes);
    CHECK_FALSE(decodeReplay(*bytes, f.inputs()));
}

TEST_CASE("new Fold profile rejects invalid UTF8 at prepare live canonical and wire",
          "[fold][recovery][utf8]") {
    const std::string invalid{"\xc0\x80", 2};
    FoldFixture f;
    f.tap();
    auto s = f.session();
    auto incoming = raw(5, 1);
    incoming.declaration.domainToken = invalid;
    CHECK_FALSE(s.submit({incoming}));
    auto key = canonical(5, 1);
    key.key.domainToken = invalid;
    CHECK_FALSE(s.submitCanonical({key}));
    detail::wire::Writer writer;
    CHECK_THROWS(writer(key));
    FoldFixture metadata;
    metadata.tap();
    auto config = metadata.configuration();
    auto mapping = config.inputMapping.view();
    mapping.profileId = invalid;
    config.inputMapping = OwnedInputMappingProfile{mapping};
    auto candidate = JudgementSession::create();
    REQUIRE(candidate);
    auto configured = candidate->configure(config);
    if (configured) {
        CHECK_FALSE(candidate->prepare(metadata.prepared()));
    } else {
        CHECK_FALSE(configured);
    }

    f.assembly.sources[0].document.requirements[0].identity.chartEntryId = invalid;
    auto p = prepareResolvedGameplay({f.assembly, {}});
    if (p) {
        auto chartCandidate = JudgementSession::create();
        REQUIRE(chartCandidate);
        REQUIRE(chartCandidate->configure(f.configuration()));
        CHECK_FALSE(chartCandidate->prepare(*p));
    }
}
TEST_CASE("negative exact Seek and same horizon cuts preserve canonical pending state",
          "[recovery][seek][cut]") {
    FoldFixture f;
    f.tap();
    auto& r = f.assembly.sources[0].document.requirements[0];
    r.timing = RequirementRecord::Timing{
        Tick{-2}, {{Tick{-10}, Tick{-2}, {PhaseKind::tap, 1}}}, {}, {{PhaseKind::tap, Tick{-5}}}};
    auto live = f.session();
    REQUIRE(live.submitCanonical({canonical(-5, 1)}));
    REQUIRE(live.advance(Tick{-1}));
    REQUIRE(live.advance(Tick{-1}));
    auto archive = *live.archive();
    auto target = f.session();
    REQUIRE(target.seek(archive, Tick{-1}));
    CHECK(sameSnapshot(target.snapshot()->state(), live.snapshot()->state()));
    auto cp = JudgementSession::checkpoint(archive, Tick{-1});
    REQUIRE(cp);
    std::vector<ReplayCheckpoint> cps{*cp};
    REQUIRE(target.seek(archive, Tick{-1}, ReplayCut{3, {}}, cps));
    CHECK(sameSnapshot(target.snapshot()->state(), live.snapshot()->state()));
}

TEST_CASE("Snapshot retains orphan contact and does not promote repeat press into head",
          "[recovery][snapshot][contact]") {
    FoldFixture f;
    auto s = f.session();
    REQUIRE(s.submit({raw(800, 1), raw(900, 2)}));
    REQUIRE(s.advance(Tick{1103}));
    auto snapshot = s.snapshot();
    REQUIRE(snapshot);
    CHECK(snapshot->state().coverageHistory.empty());

    auto restored = JudgementSession::recover(*snapshot, f.inputs());
    REQUIRE(restored);
    CHECK(sameSnapshot(snapshot->state(), restored->snapshot()->state()));
}

TEST_CASE("real same Tick body and tail Fold checks MAX plus one before minus one",
          "[fold][arithmetic][transaction]") {
    FoldFixture f;
    f.rules.score.initial = INT64_MAX;
    for (auto& rule : f.rules.score.rules) {
        if (rule.phase == PhaseKind::head && rule.outcome == Outcome::hit) {
            rule.delta = 0;
        }
        if (rule.phase == PhaseKind::body && rule.outcome == Outcome::miss) {
            rule.delta = 1;
        }
        if (rule.phase == PhaseKind::tail && rule.outcome == Outcome::miss) {
            rule.delta = -1;
        }
    }
    auto checked = f.session();
    REQUIRE(checked.submit({raw(900, 1), raw(950, 2, InputAction::release)}));
    CHECK_FALSE(checked.advance(Tick{1103}));
    CHECK(checked.query()->kernelView().facts.size() == 3);
    CHECK(checked.query()->kernelView().fold->score == INT64_MAX);
    f.rules.score.arithmetic = ArithmeticPolicy::clamp;
    auto clamp = f.session();
    REQUIRE(clamp.submit({raw(900, 1), raw(950, 2, InputAction::release)}));
    auto result = clamp.advance(Tick{1103});
    REQUIRE(result);
    CHECK(result->kernelView().facts.size() == 3);
    CHECK(result->kernelView().fold->score == INT64_MAX - 1);
}

TEST_CASE("Snapshot prefix verification stops at exact watermark with near future pending",
          "[recovery][snapshot][pending]") {
    FoldFixture f;
    f.tap();
    auto s = f.session();
    REQUIRE(s.submitCanonical({canonical(8, 1)}));
    REQUIRE(s.advance(Tick{10}));
    auto value = s.snapshot();
    REQUIRE(value);
    REQUIRE(value->state().pending.size() == 1);
    auto restored = JudgementSession::recover(*value, f.inputs());
    REQUIRE(restored);
    CHECK(sameSnapshot(value->state(), restored->snapshot()->state()));
    auto empty = f.session();
    REQUIRE(empty.advance(Tick{10}));
    auto pending = empty.snapshot();
    REQUIRE(pending);
    REQUIRE(JudgementSession::recover(*pending, f.inputs()));
}

TEST_CASE("Statistics includes declared absent grade keys and real stray receipt",
          "[fold][statistics][golden]") {
    FoldFixture f;
    auto s = f.session();
    REQUIRE(s.submit({raw(800, 1), raw(900, 2)}));
    REQUIRE(s.advance(Tick{1103}));
    auto p = s.query();
    REQUIRE(p);
    expectOracle(p->kernelView());
    CHECK(p->kernelView().fold->strayCount == 1);
    CHECK(p->kernelView().fold->consumeEmptyCount == 1);
    CHECK(p->kernelView().fold->counts.size() == 8);
    auto saved = s.snapshot();
    REQUIRE(saved);
    auto restored = JudgementSession::recover(*saved, f.inputs());
    REQUIRE(restored);
    CHECK(sameKernelResult(p->kernelView(), restored->query()->kernelView()));
    auto permuted = f.rules;
    std::reverse(permuted.score.rules.begin(), permuted.score.rules.end());
    auto a = prepareRuleset(f.rules), b = prepareRuleset(permuted);
    REQUIRE(a);
    REQUIRE(b);
    CHECK(rulesetIdentityToken(*a) == rulesetIdentityToken(*b));
}

TEST_CASE("Runtime identity matches graph reference with actual ruleset and grade declarations",
          "[fold][recovery][identity]") {
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
        auto actual = configuration.identityDeclarations;
        actual.engine.judgementSemanticRevision += ";gameplay.fold.finite.v1";
        if (graded)
            actual.engine.factSemanticRevision += ";grade.signed-interval.v1";
        actual.ruleset.interfaceProjectionToken = rulesetIdentityToken(*configuration.ruleset);
        actual.ruleset.buildHash = configuration.ruleset->declaration().compiledBuild;
        actual.ruleset.moduleOrder.clear();
        for (const auto& module : configuration.ruleset->declaration().modules)
            actual.ruleset.moduleOrder.push_back(module.id);
        actual.session.loadoutToken = configuration.ruleset->declaration().loadoutId;
        const auto expected = makeRuntimePreparedIdentity(
            prepared.assembled().graph, actual, configuration.inputMapping.view(),
            configuration.latePolicy, configuration.calibrationIdentityToken);
        std::optional<PreparedIdentity> retained;
        {
            auto session = JudgementSession::create();
            REQUIRE(session);
            REQUIRE(session->configure(configuration));
            REQUIRE(session->prepare(prepared));
            auto query = session->query();
            REQUIRE(query);
            retained = query->kernelView().judgementIdentity;
        }
        REQUIRE(retained);
        CHECK(*retained == expected);
        CHECK_FALSE(*retained == makeRuntimePreparedIdentity(
                                     prepared.assembled().graph, configuration.identityDeclarations,
                                     configuration.inputMapping.view(), configuration.latePolicy,
                                     configuration.calibrationIdentityToken));
    }
}

TEST_CASE("actual identity isolates Loadout scoring chart and rejects fake engine revision",
          "[fold][recovery][identity]") {
    const auto components = [](const PreparedIdentity& identity) {
        const auto& bytes = identity.canonicalBytes().bytes();
        std::array<std::size_t, 5> offsets{};
        const std::array<std::string, 4> names{"engine", "ruleset", "chart", "session"};
        for (std::size_t i = 0; i < names.size(); ++i) {
            std::vector<std::byte> marker(8, std::byte{0});
            marker[7] = std::byte{static_cast<unsigned char>(names[i].size())};
            for (char c : names[i]) {
                marker.push_back(std::byte{static_cast<unsigned char>(c)});
            }
            auto found = std::search(bytes.begin(), bytes.end(), marker.begin(), marker.end());
            REQUIRE(found != bytes.end());
            offsets[i] = static_cast<std::size_t>(found - bytes.begin());
        }
        offsets[4] = bytes.size();
        std::array<std::vector<std::byte>, 4> result;
        for (std::size_t i = 0; i < 4; ++i) {
            REQUIRE(offsets[i] < offsets[i + 1]);
            result[i] = {bytes.begin() + static_cast<std::ptrdiff_t>(offsets[i]),
                         bytes.begin() + static_cast<std::ptrdiff_t>(offsets[i + 1])};
        }
        return result;
    };
    FoldFixture base;
    base.tap();
    auto initial = base.session();
    auto expected = components(initial.query()->kernelView().judgementIdentity);
    auto fake = base.configuration();
    fake.factSemanticRevision = "fact.fake";
    fake.identityDeclarations.engine.factSemanticRevision = "fact.fake";
    auto rejected = JudgementSession::create();
    REQUIRE(rejected);
    CHECK_FALSE(rejected->configure(fake));

    for (unsigned changed = 1; changed < 4; ++changed) {
        FoldFixture f;
        f.tap();
        auto config = f.configuration();
        if (changed == 1) {
            ++f.rules.score.rules[0].delta;
            config = f.configuration();
        }
        if (changed == 2) {
            f.assembly.sources[0].document.requirements[0].timing->phaseTargets[0].chartTick =
                Tick{6};
        }
        if (changed == 3) {
            f.rules.loadoutId = "loadout.other";
            config = f.configuration();
        }
        auto session = JudgementSession::create();
        REQUIRE(session);
        REQUIRE(session->configure(config));
        REQUIRE(session->prepare(f.prepared()));
        auto actual = components(session->query()->kernelView().judgementIdentity);
        for (unsigned i = 0; i < 4; ++i) {
            CHECK((actual[i] != expected[i]) == (i == changed));
        }
    }
}

TEST_CASE("real Fold consumer preserves full u64 bonus mathematical range",
          "[fold][hook][arithmetic]") {
    FoldFixture f{true};
    f.rules.hooks[0].value = UINT64_MAX;
    SECTION("clamp") {
        f.rules.score.arithmetic = ArithmeticPolicy::clamp;
    }
    SECTION("checked mathematical representability") {
        f.rules.score.initial = INT64_MIN;
        for (auto& rule : f.rules.score.rules) {
            rule.delta = 0;
        }
    }
    auto s = f.session();
    REQUIRE(s.submit({raw(900, 1)}));
    auto p = s.advance(Tick{1003});
    REQUIRE(p);
    CHECK(p->kernelView().fold->score == INT64_MAX);
    CHECK(p->kernelView().fold->bonus == UINT64_MAX);
    CHECK(p->kernelView().fold->hits == 2);
    REQUIRE(JudgementSession::recover(*s.snapshot(), f.inputs()));
}

TEST_CASE("real maximum Tick no output succeeds and actual Hook output fails",
          "[fold][hook][extreme]") {
    FoldFixture f{true};
    f.tap();
    f.base.latePolicy.windowCloseThreshold = MeasuredParameter<TickSpan>::measured(TickSpan{0});
    auto& r = f.assembly.sources[0].document.requirements[0];
    r.timing =
        RequirementRecord::Timing{Tick{INT64_MAX},
                                  {{Tick{INT64_MAX - 12}, Tick{INT64_MAX}, {PhaseKind::tap, 1}}},
                                  {},
                                  {{PhaseKind::tap, Tick{INT64_MAX - 12}}}};
    auto success = f.session();
    REQUIRE(success.submit({raw(INT64_MAX - 12, 1)}));
    REQUIRE(success.advance(Tick{INT64_MAX}));
    CHECK(success.query()->kernelView().kernelWorkTick == Tick{INT64_MAX});
    CHECK(success.query()->kernelView().fold->workTick == Tick{INT64_MAX});
    CHECK(success.query()->kernelView().fold->produced.size() == 1);
    REQUIRE(JudgementSession::recover(*success.snapshot(), f.inputs()));
    auto failure = f.session();
    CHECK_FALSE(failure.advance(Tick{INT64_MAX}));
    auto p = failure.query();
    REQUIRE(p);
    CHECK(p->kernelView().kernelWorkTick == Tick{INT64_MAX});
    CHECK(p->kernelView().faultStage == FaultStage::fold);
    CHECK(p->kernelView().facts.size() == 1);
    CHECK(p->kernelView().fold->produced.empty());
    CHECK(p->kernelView().fold->score == 0);
    CHECK(p->kernelView().faultDiagnostic->code() == "ruleset.transaction_failed");
    auto evaluated = JudgementSession::evaluateReplay(*failure.archive());
    REQUIRE(evaluated);
    CHECK(sameKernelResult(*evaluated->result, p->kernelView()));
    CHECK_FALSE(failure.snapshot());
}
