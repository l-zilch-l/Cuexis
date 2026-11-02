#include "fold_recovery_fixture.hpp"

using namespace fold_fixture;

TEST_CASE("finite Ruleset registry rejects malformed package build policy and manifest",
          "[fold][recovery][registry]") {
    auto d = declaration();
    REQUIRE(prepareRuleset(d));
    SECTION("external package") {
        d.externalPackage = true;
        auto r = prepareRuleset(d);
        REQUIRE_FALSE(r);
        CHECK(r.error().code() == "ruleset.package_unsupported");
    }
    SECTION("Life") {
        d.life = true;
        auto r = prepareRuleset(d);
        REQUIRE_FALSE(r);
        CHECK(r.error().code() == "capability.disabled");
    }
    SECTION("fake build") {
        d.compiledBuild = "fake";
        CHECK_FALSE(prepareRuleset(d));
    }
    SECTION("wrong scope") {
        d.outcomeScope = OutcomeScope::extended;
        CHECK_FALSE(prepareRuleset(d));
    }
    SECTION("duplicate owner") {
        d.modules[1] = d.modules[0];
        CHECK_FALSE(prepareRuleset(d));
    }
    SECTION("missing mapping") {
        d.score.rules.clear();
        CHECK_FALSE(prepareRuleset(d));
    }
}
TEST_CASE("score checked and clamp use each mathematical intermediate",
          "[fold][arithmetic][golden]") {
    auto c = declaration().score;
    c.minimum = -10;
    c.maximum = 10;
    CHECK_FALSE(checkedScore(10, 1, c));
    c.arithmetic = ArithmeticPolicy::clamp;
    CHECK(*checkedScore(10, 1, c) == 10);
    CHECK(*checkedScore(10, -1, c) == 9);
    c.minimum = INT64_MIN;
    c.maximum = INT64_MAX;
    CHECK(*checkedScore(INT64_MAX, 1, c) == INT64_MAX);
    CHECK(*checkedScore(INT64_MIN, -1, c) == INT64_MIN);
    CHECK(*checkedScore(INT64_MIN, INT64_MAX, c) == -1);
    c.arithmetic = ArithmeticPolicy::checked;
    CHECK_FALSE(checkedScore(INT64_MAX, 1, c));
}
TEST_CASE("optional signed grade table seals actual error and permits repeated grade references",
          "[fold][grade][golden]") {
    FoldFixture f;
    auto& c = f.assembly.sources[0].document.requirements[0].measure.components[0];
    c.declaredGradeTokens = {"tight"};
    c.gradeTable = std::vector<GradeInterval>{{-50, -1, "tight"}, {0, 0, "tight"}};
    f.rules.score.rules.push_back({PhaseKind::head, Outcome::hit, "tight", 2, true});
    auto s = f.session();
    REQUIRE(s.submit({raw(900, 1), raw(1000, 2, InputAction::release)}));
    auto p = s.advance(Tick{1103});
    REQUIRE(p);
    const auto& facts = p->kernelView().facts;
    REQUIRE(facts.size() == 3);
    CHECK(std::get<PhaseOutcomeFact>(facts[0]).grade == "tight");
    CHECK_FALSE(std::get<PhaseOutcomeFact>(facts[1]).grade);
    expectOracle(p->kernelView());
    c.gradeTable = std::vector<GradeInterval>{{-50, -2, "tight"}, {0, 0, "tight"}};
    auto other = JudgementSession::create();
    REQUIRE(other);
    REQUIRE(other->configure(f.configuration()));
    CHECK_FALSE(other->prepare(f.prepared()));
}
TEST_CASE("real Fold matches hand golden and independent oracle with owning reset",
          "[fold][golden][oracle]") {
    // Independent reference is proved with a human three-Fact expectation before path comparison.
    FoldFixture f;
    auto s = f.session();
    REQUIRE(s.submit({raw(900, 1), raw(1000, 2, InputAction::release)}));
    auto p = s.advance(Tick{1103});
    REQUIRE(p);
    expectOracle(p->kernelView());
    CHECK(p->kernelView().fold->score == 6);
    CHECK(p->kernelView().fold->maxCombo == 3);
    auto old = s.snapshot();
    REQUIRE(old);
    REQUIRE(s.reset());
    CHECK(p->kernelView().fold->score == 6);
    CHECK(old->state().kernel.fold->score == 6);
}
TEST_CASE("real t plus one bonus and sparse due consumption survive full restore",
          "[fold][hook][recovery]") {
    FoldFixture f{true};
    auto s = f.session();
    REQUIRE(s.submit({raw(900, 1), raw(1000, 2, InputAction::release)}));
    auto first = s.advance(Tick{903});
    REQUIRE(first);
    REQUIRE(first->kernelView().fold);
    CHECK(first->kernelView().fold->score == 2);
    CHECK(first->kernelView().fold->bonus == 0);
    REQUIRE(s.advance(Tick{904}));
    CHECK(s.query()->kernelView().fold->bonus == 3);
    REQUIRE(first->kernelView().fold->produced.size() == 1);
    CHECK(first->kernelView().fold->produced[0].visibleTick == Tick{901});
    auto snapshot = s.snapshot();
    REQUIRE(snapshot);
    auto restored = JudgementSession::recover(*snapshot, f.inputs());
    REQUIRE(restored);
    auto a = s.advance(Tick{1103});
    auto b = restored->advance(Tick{1103});
    REQUIRE(a);
    REQUIRE(b);
    CHECK(sameKernelResult(a->kernelView(), b->kernelView()));
    CHECK(a->kernelView().fold->score == 12);
    CHECK(a->kernelView().fold->produced.size() == 2);
    CHECK(a->kernelView().fold->hookConsumerCursor == 2);
}
TEST_CASE("unsupported Hook routes reject instead of emitting visibility markers",
          "[fold][hook][negative]") {
    auto d = declaration(true);
    SECTION("kernel") {
        d.hooks[0].consumer = HookConsumer::kernel;
    }
    SECTION("shared") {
        d.hooks[0].consumer = HookConsumer::shared;
    }
    SECTION("step") {
        d.hooks[0].stepRoute = true;
    }
    SECTION("target") {
        d.hooks[0].target = "unknown";
    }
    SECTION("contributor") {
        d.hooks[0].contributors = {"score"};
    }
    CHECK_FALSE(prepareRuleset(d));
}
TEST_CASE("seal after failure retains Fact and entire previous Fold",
          "[fold][fault][transaction]") {
    FoldFixture f;
    f.tap();
    f.rules.score.initial = INT64_MAX;
    auto s = f.session();
    REQUIRE(s.advance(Tick{3}));
    const auto old = *s.query()->kernelView().fold;
    REQUIRE(s.submit({raw(5, 1)}));
    CHECK_FALSE(s.advance(Tick{20}));
    auto p = s.query();
    REQUIRE(p);
    CHECK(p->kernelView().state == KernelSessionState::Faulted);
    CHECK(p->kernelView().facts.size() == 1);
    CHECK(p->kernelView().fold == old);
    CHECK(p->kernelView().sealedFactCursor == 1);
    CHECK(p->kernelView().kernelWorkTick == Tick{5});
    CHECK(p->kernelView().faultDiagnostic->code() == "ruleset.transaction_failed");
    CHECK_FALSE(s.snapshot());
    auto archive = s.archive();
    REQUIRE(archive);
    auto evaluation = JudgementSession::evaluateReplay(*archive);
    REQUIRE(evaluation);
    CHECK(evaluation->evidenceValid);
    CHECK(sameKernelResult(*evaluation->result, p->kernelView()));
    CHECK_FALSE(s.seek(*archive, Tick{3}));
    REQUIRE(s.reset());
}
TEST_CASE("Fold-only due cursor is rolled back when empty Tick Fold fails", "[fold][hook][fault]") {
    FoldFixture f{true};
    f.tap();
    auto s = f.session();
    REQUIRE(s.submit({raw(5, 1)}));
    REQUIRE(s.advance(Tick{8}));
    auto old = *s.query()->kernelView().fold;
    CHECK(old.hookConsumerCursor == 0);
    REQUIRE(detail::KernelTestAccess::inject(
        s, {Tick{6}, detail::KernelFailurePoint::beforeFold, {}, {}, {}}));
    CHECK_FALSE(s.advance(Tick{9}));
    const auto& p = s.query()->kernelView();
    CHECK(p.fold == old);
    CHECK(p.kernelWorkTick == Tick{6});
    CHECK(p.processedFrontier == Tick{6});
}
TEST_CASE("canonical amount bypasses scale and shares atomic sequence admission",
          "[recovery][canonical]") {
    FoldFixture f;
    f.tap();
    auto a = f.session();
    auto b = f.session();
    REQUIRE(a.submit({raw(5, 1, InputAction::press, makeBeat(2, 1))}));
    REQUIRE(b.submitCanonical({canonical(5, 1, 1)}));
    auto x = a.advance(Tick{20});
    auto y = b.advance(Tick{20});
    REQUIRE(x);
    REQUIRE(y);
    CHECK(sameKernelResult(x->kernelView(), y->kernelView()));
    auto c = f.session();
    CHECK_FALSE(c.submitCanonical({canonical(5, 1), canonical(100, 1)}));
    CHECK(c.archive()->data().eventCount == 0);
    CHECK_FALSE(c.submitCanonical({canonical(5, 1, 101)}));
    CHECK(c.archive()->data().eventCount == 0);
    auto mismatched = canonical(5, 1);
    mismatched.key.sourceClass = "pointer";
    CHECK_FALSE(c.submitCanonical({mismatched}));
}
TEST_CASE("future pending last observed Tick and archive lazy fork survive exact Seek",
          "[recovery][seek][branch]") {
    FoldFixture f;
    f.tap();
    auto s = f.session();
    REQUIRE(s.submitCanonical({canonical(5, 1), canonical(100, 2)}));
    REQUIRE(s.advance(Tick{20}));
    auto original = s.archive();
    REQUIRE(original);
    REQUIRE(s.seek(*original, Tick{10}));
    auto state = s.snapshot();
    REQUIRE(state);
    CHECK(state->state().kernel.processedFrontier == Tick{7});
    CHECK(state->state().ingress.lastObservedTick == ObservationTick{Tick{100}});
    REQUIRE(state->state().pending.size() == 1);
    CHECK(state->state().pending[0].observationKey.observationTick == ObservationTick{Tick{100}});
    CHECK(sameReplayHistory(*s.archive(), *original));
    CHECK_FALSE(s.submitCanonical({canonical(10, 3)}));
    REQUIRE(s.submitCanonical({}));
    CHECK(sameReplayHistory(*s.archive(), *original));
    REQUIRE(s.submitCanonical({canonical(101, 3)}));
    CHECK_FALSE(sameReplayHistory(*s.archive(), *original));
    CHECK(original->data().eventCount == 2);
    CHECK(s.archive()->data().eventCount == 3);
    REQUIRE(JudgementSession::evaluateReplay(*s.archive()));
}
TEST_CASE("exact horizon uses F H and partial control does not compare larger terminal fault",
          "[recovery][seek][fault]") {
    FoldFixture f;
    f.tap();
    auto s = f.session();
    REQUIRE(s.submit({raw(5, 1)}));
    detail::KernelTestControls controls{
        Tick{8}, detail::KernelFailurePoint::beforeFold, {}, {}, {}};
    REQUIRE(detail::KernelTestAccess::inject(s, controls));
    CHECK_FALSE(s.advance(Tick{20}));
    auto archive = s.archive();
    REQUIRE(archive);
    REQUIRE(detail::ExecutionKernel::evaluate(*archive, &controls));
    CHECK_FALSE(JudgementSession::evaluateReplay(*archive));
    auto before = detail::ExecutionKernel::seekCandidate(*archive, Tick{10}, &controls);
    REQUIRE(before);
    CHECK((*before)->query()->projection.processedFrontier == Tick{7});
    CHECK((*before)->query()->projection.state == KernelSessionState::Prepared);
    CHECK_FALSE(detail::ExecutionKernel::seekCandidate(*archive, Tick{11}, &controls));
}
TEST_CASE("Snapshot all private state survives destruction and rejects tampered candidate",
          "[recovery][snapshot][negative]") {
    FoldFixture f{true};
    f.tap();
    auto inputs = f.inputs();
    auto saved = [&] {
        auto s = f.session();
        REQUIRE(s.submitCanonical({canonical(5, 1), canonical(100, 2)}));
        REQUIRE(s.advance(Tick{10}));
        return *s.snapshot();
    }();
    auto restored = JudgementSession::recover(saved, inputs);
    REQUIRE(restored);
    auto full = restored->snapshot();
    REQUIRE(full);
    CHECK(sameSnapshot(saved.state(), full->state()));
    CHECK_FALSE(restored->archive());
    auto dto = saved.state();
    dto.kernel.fold->score = INT64_MIN;
    detail::SnapshotStorage invalid{dto, std::make_shared<const RecoveryInputs>(inputs)};
    CHECK_FALSE(detail::ExecutionKernel::restore(invalid, inputs));
    dto = saved.state();
    dto.pending[0].dispatchTick = Tick{10};
    detail::SnapshotStorage wrongRoute{dto, std::make_shared<const RecoveryInputs>(inputs)};
    CHECK_FALSE(detail::ExecutionKernel::restore(wrongRoute, inputs));
    auto mismatch = inputs;
    auto r = f.rules;
    r.score.rules[0].delta = 12;
    mismatch.configuration.ruleset = *prepareRuleset(r);
    CHECK_FALSE(JudgementSession::recover(saved, mismatch));
    CHECK(sameSnapshot(saved.state(), restored->snapshot()->state()));
}
TEST_CASE("full Replay comparison catches state and Hook differences without Fact changes",
          "[recovery][replay][negative]") {
    FoldFixture f{true};
    f.tap();
    auto s = f.session();
    REQUIRE(s.submit({raw(5, 1)}));
    REQUIRE(s.advance(Tick{20}));
    auto archive = *s.archive();
    auto data = std::make_shared<ReplayData>(archive.data());
    auto p = std::make_shared<KernelProjection>(*data->records.back().result);
    ++p->fold->hits;
    data->records.back().result = p;
    ReplayArchive forged{data, std::make_shared<const RecoveryInputs>(archive.dependencies())};
    CHECK_FALSE(JudgementSession::evaluateReplay(forged));
    CHECK_FALSE(s.seek(forged, Tick{20}));
    p->fold->hits--;
    p->fold->bonus++;
    CHECK_FALSE(JudgementSession::evaluateReplay(forged));
}
TEST_CASE("checkpoint binds full history cut and horizon and ignores foreign or forged states",
          "[recovery][checkpoint]") {
    FoldFixture f;
    f.tap();
    auto s = f.session();
    REQUIRE(s.submit({raw(5, 1)}));
    REQUIRE(s.advance(Tick{10}));
    REQUIRE(s.advance(Tick{20}));
    auto archive = *s.archive();
    auto cp = JudgementSession::checkpoint(archive, Tick{10});
    REQUIRE(cp);
    auto reference = f.session();
    REQUIRE(reference.seek(archive, Tick{20}));
    std::vector<ReplayCheckpoint> checkpoints{*cp};
    REQUIRE(s.seek(archive, Tick{20}, replayCutAt(archive, Tick{20}), checkpoints));
    CHECK(sameSnapshot(s.snapshot()->state(), reference.snapshot()->state()));
    auto forged = std::make_shared<SnapshotDTO>(*cp->state);
    forged->kernel.fold->bonus = 777;
    checkpoints[0].state = forged;
    REQUIRE(s.seek(archive, Tick{20}, replayCutAt(archive, Tick{20}), checkpoints));
    CHECK(sameSnapshot(s.snapshot()->state(), reference.snapshot()->state()));
    auto other = f.session();
    REQUIRE(other.advance(Tick{10}));
    checkpoints[0] = *JudgementSession::checkpoint(*other.archive(), Tick{10});
    REQUIRE(s.seek(archive, Tick{20}, replayCutAt(archive, Tick{20}), checkpoints));
    CHECK(sameSnapshot(s.snapshot()->state(), reference.snapshot()->state()));
}
TEST_CASE("W2 artificial canonical bytes are fixed LE with UTF8 and optional signed amount",
          "[recovery][codec][golden]") {
    CanonicalInput input{{ObservationTick{Tick{5}}, "d", "s", "c", InputAction::press, -2},
                         IngressSequence{9}};
    const std::vector<unsigned> golden{5,   0,   0,   0, 0, 0, 0,  0, 1, 0,   0,   0,   0,   0,
                                       0,   0,   100, 1, 0, 0, 0,  0, 0, 0,   0,   115, 1,   0,
                                       0,   0,   0,   0, 0, 0, 99, 0, 1, 254, 255, 255, 255, 255,
                                       255, 255, 255, 9, 0, 0, 0,  0, 0, 0,   0};
    detail::wire::Writer writer;
    writer(input);
    REQUIRE(writer.bytes.size() == golden.size());
    for (std::size_t i = 0; i < golden.size(); ++i) {
        CHECK(std::to_integer<unsigned>(writer.bytes[i]) == golden[i]);
    }
    CHECK(validUtf8("中文"));
    CHECK_FALSE(validUtf8(std::string{"\xc0\x80", 2}));
    CHECK_FALSE(validUtf8(std::string{"\xed\xa0\x80", 3}));
}
TEST_CASE("W2 Replay and full Snapshot roundtrip reject corruption version and budget",
          "[recovery][codec]") {
    FoldFixture f{true};
    auto s = f.session();
    REQUIRE(s.submit({raw(900, 1), raw(1000, 2, InputAction::release)}));
    REQUIRE(s.advance(Tick{1103}));
    auto archive = *s.archive();
    auto bytes = encodeReplay(archive);
    REQUIRE(bytes);
    auto read = decodeReplay(*bytes, f.inputs());
    REQUIRE(read);
    auto evaluated = JudgementSession::evaluateReplay(*read);
    REQUIRE(evaluated);
    CHECK(sameKernelResult(*evaluated->result, s.query()->kernelView()));
    auto snap = s.snapshot();
    REQUIRE(snap);
    auto encoded = encodeSnapshot(*snap);
    REQUIRE(encoded);
    auto decoded = decodeSnapshot(*encoded, f.inputs());
    INFO((decoded ? ""
                  : std::string{decoded.error().message()} + " " +
                        std::string{decoded.error().code()}));
    REQUIRE(decoded);
    CHECK(sameSnapshot(snap->state(), decoded->state()));
    SECTION("truncated") {
        bytes->pop_back();
        CHECK_FALSE(decodeReplay(*bytes, f.inputs()));
    }
    SECTION("version") {
        (*bytes)[8] = std::byte{3};
        auto r = decodeReplay(*bytes, f.inputs());
        REQUIRE_FALSE(r);
        CHECK(r.error().code() == "codec.version_unsupported");
    }
    SECTION("digest") {
        (*bytes)[21] ^= std::byte{1};
        auto r = decodeReplay(*bytes, f.inputs());
        REQUIRE_FALSE(r);
        CHECK(r.error().code() == "codec.digest_mismatch");
    }
    SECTION("test budget") {
        CHECK_FALSE(decodeReplay(*bytes, f.inputs(), CodecBudget{1, 1, 1, true}));
    }
    SECTION("production budget") {
        auto r = decodeReplay(*bytes, f.inputs(), CodecBudget{1000000, 1000000, 1000000, false});
        REQUIRE_FALSE(r);
        CHECK(r.error().code() == "codec.budget_unaccepted");
    }
    bytes = encodeReplay(archive);
    REQUIRE(bytes);
    std::string output;
#ifdef _MSC_VER
    char* envValue = nullptr;
    std::size_t envLength = 0;
    if (_dupenv_s(&envValue, &envLength, "CUEXIS_GOLDEN_OUTPUT") == 0 && envValue) {
        output = envValue;
    }
    std::free(envValue);
#else
    if (const auto* envValue = std::getenv("CUEXIS_GOLDEN_OUTPUT")) {
        output = envValue;
    }
#endif
    if (!output.empty()) {
        const auto* path = output.c_str();
        std::ofstream replay{std::string{path} + ".replay", std::ios::binary};
        replay.write(reinterpret_cast<const char*>(bytes->data()),
                     static_cast<std::streamsize>(bytes->size()));
        std::ofstream snapshot{std::string{path} + ".snapshot", std::ios::binary};
        snapshot.write(reinterpret_cast<const char*>(encoded->data()),
                       static_cast<std::streamsize>(encoded->size()));
    }
}

TEST_CASE("W2 element budget counts identity bytes and aggregate nested body elements",
          "[recovery][codec][element-budget]") {
    FoldFixture f;
    auto s = f.session();
    const auto initial = s.query()->kernelView();
    const std::vector<std::byte> identity{std::byte{1}, std::byte{2}, std::byte{3}};
    const std::vector<std::string> strings{"a", "bc"};
    const std::vector<std::uint64_t> numbers{7, 8};
    detail::wire::Writer writer;
    writer(identity, strings, numbers);
    std::vector<std::byte> decodedIdentity;
    std::vector<std::string> decodedStrings;
    std::vector<std::uint64_t> decodedNumbers;
    // Three identity bytes + two strings + their 1/2 bytes + two numbers = 10 elements.
    CodecBudget budget{writer.bytes.size(), 100, 10, true};
    detail::wire::Reader exact{writer.bytes, 0, 0, budget, initial};
    exact(decodedIdentity);
    CHECK(decodedIdentity == identity);
    CHECK(exact.elements == 3);
    exact(decodedStrings);
    CHECK(decodedStrings == strings);
    CHECK(exact.elements == 8);
    exact(decodedNumbers);
    CHECK(decodedNumbers == numbers);
    CHECK(exact.elements == 10);
    CHECK(exact.pos == writer.bytes.size());

    budget.maxElements = 9;
    detail::wire::Reader below{writer.bytes, 0, 0, budget, initial};
    CHECK_THROWS_WITH(below(decodedIdentity, decodedStrings, decodedNumbers),
                      "codec.budget_exceeded");
    budget.maxElements = 2;
    detail::wire::Reader headerBelow{writer.bytes, 0, 0, budget, initial};
    CHECK_THROWS_WITH(headerBelow(decodedIdentity), "codec.budget_exceeded");
}

TEST_CASE("W2 Snapshot element budget includes header and body at the exact limit",
          "[recovery][codec][element-budget]") {
    FoldFixture f{true};
    auto s = f.session();
    REQUIRE(s.submit({raw(900, 1), raw(1000, 2, InputAction::release)}));
    REQUIRE(s.advance(Tick{1103}));
    const auto snapshot = s.snapshot();
    REQUIRE(snapshot);
    const auto bytes = encodeSnapshot(*snapshot);
    REQUIRE(bytes);
    const auto& kernel = snapshot->state().kernel;
    const auto identityElements = kernel.judgementIdentity.canonicalBytes().bytes().size();
    const auto headerElements = identityElements + kernel.effectiveFactSemanticRevision.size() +
                                std::string_view{"canonical.discrete.le.v1"}.size();
    // Use the existing typed wire helper for the small body; the header charge above is
    // independent. Public decode must carry that header charge into the body Reader.
    detail::wire::Writer body;
    body(snapshot->state());
    detail::wire::Reader reader{body.bytes, 0, 0, {}, kernel};
    auto dto = reader.seed<SnapshotDTO>();
    reader(dto);
    REQUIRE(reader.pos == body.bytes.size());
    REQUIRE(reader.elements > 0);
    const auto totalElements = headerElements + reader.elements;
    CodecBudget budget{bytes->size(), 100, totalElements, true};
    const auto decoded = decodeSnapshot(*bytes, f.inputs(), budget);
    REQUIRE(decoded);
    CHECK(sameSnapshot(snapshot->state(), decoded->state()));
    const auto reencoded = encodeSnapshot(*decoded);
    REQUIRE(reencoded);
    CHECK(*reencoded == *bytes);
    for (const auto limit :
         {identityElements - 1, headerElements, reader.elements, totalElements - 1}) {
        CAPTURE(limit, headerElements, reader.elements, totalElements);
        budget.maxElements = limit;
        const auto rejected = decodeSnapshot(*bytes, f.inputs(), budget);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().code() == "codec.budget_exceeded");
    }
}

TEST_CASE("three Register kinds enforce single write carrier closure and contribution identity",
          "[fold][register][golden]") {
    RegisterDeclaration d{
        "r",        RegisterKind::commutativeMonoid, RegisterValueTag::unsigned64, "",
        {"a", "b"}, CombineOperator::bitOr};
    std::vector<RegisterContribution> writes{{"r", "a", 1, 0, 0, RegisterValue{std::uint64_t{1}}},
                                             {"r", "b", 1, 1, 0, RegisterValue{std::uint64_t{4}}}};
    CHECK(std::get<std::uint64_t>(*commitRegister(d, {}, RegisterValue{std::uint64_t{0}})) == 0);
    CHECK(std::get<std::uint64_t>(*commitRegister(d, writes, RegisterValue{std::uint64_t{0}})) ==
          5);
    std::reverse(writes.begin(), writes.end());
    CHECK(std::get<std::uint64_t>(*commitRegister(d, writes, RegisterValue{std::uint64_t{0}})) ==
          5);
    auto duplicate = writes;
    duplicate.push_back(writes[0]);
    CHECK_FALSE(commitRegister(d, duplicate, RegisterValue{std::uint64_t{0}}));
    d.kind = RegisterKind::exclusive;
    d.owner = "a";
    CHECK_FALSE(commitRegister(d, writes, RegisterValue{std::uint64_t{0}}));
    d.kind = RegisterKind::ledgerDerived;
    CHECK_FALSE(commitRegister(d, writes, RegisterValue{std::uint64_t{0}}));
    CHECK(std::get<std::uint64_t>(*commitRegister(d, {}, RegisterValue{std::uint64_t{17}})) == 17);
    for (auto combine : {CombineOperator::maximum, CombineOperator::bitOr}) {
        const auto op = [&](std::uint64_t a, std::uint64_t b) {
            return combine == CombineOperator::maximum ? std::max(a, b) : a | b;
        };
        for (auto a : std::array<std::uint64_t, 3>{0, 1, UINT64_MAX}) {
            for (auto b : std::array<std::uint64_t, 3>{0, 3, UINT64_MAX}) {
                for (auto c : std::array<std::uint64_t, 3>{0, 5, UINT64_MAX}) {
                    CHECK(op(a, b) == op(b, a));
                    CHECK(op(op(a, b), c) == op(a, op(b, c)));
                    CHECK(op(a, 0) == a);
                }
            }
        }
    }
}
TEST_CASE("invalid Register deltas fault after seal without changing any old Fold member",
          "[fold][register][fault]") {
    for (auto fault :
         {detail::RegisterFault::secondExclusive, detail::RegisterFault::duplicateContribution,
          detail::RegisterFault::directDerived}) {
        FoldFixture f{true};
        f.tap();
        auto s = f.session();
        REQUIRE(s.advance(Tick{3}));
        auto old = *s.query()->kernelView().fold;
        detail::KernelTestControls controls{};
        controls.invalidRegisterTick = Tick{5};
        controls.registerFault = fault;
        REQUIRE(detail::KernelTestAccess::inject(s, controls));
        REQUIRE(s.submit({raw(5, 1)}));
        CHECK_FALSE(s.advance(Tick{20}));
        CHECK(s.query()->kernelView().facts.size() == 1);
        CHECK(s.query()->kernelView().fold == old);
    }
}
TEST_CASE("Replay record reservation failures publish neither admission nor control",
          "[recovery][record][fault]") {
    FoldFixture f;
    f.tap();
    auto s = f.session();
    auto old = s.query();
    REQUIRE(old);
    auto archive = *s.archive();
    detail::KernelTestControls controls{};
    controls.failSubmitRecordReservation = true;
    controls.failRecordReservationHorizon = Tick{20};
    REQUIRE(detail::KernelTestAccess::inject(s, controls));
    CHECK_FALSE(s.submit({raw(5, 1)}));
    CHECK_FALSE(s.advance(Tick{20}));
    CHECK(sameKernelResult(old->kernelView(), s.query()->kernelView()));
    CHECK(sameReplayHistory(archive, *s.archive()));
    controls.failSubmitRecordReservation = false;
    controls.failRecordReservationHorizon.reset();
    REQUIRE(detail::KernelTestAccess::inject(s, controls));
    REQUIRE(s.submit({raw(5, 1)}));
    REQUIRE(s.advance(Tick{20}));
    REQUIRE(JudgementSession::evaluateReplay(*s.archive()));
}
TEST_CASE("live Replay restore and Seek agree with independent kernel and Fold start oracle",
          "[recovery][oracle][four-path]") {
    FoldFixture f;
    auto inputs = f.inputs();
    auto live = f.session();
    auto receipts = live.submit({raw(900, 1), raw(1000, 2, InputAction::release)});
    REQUIRE(receipts);
    std::vector<AdmittedObservation> admitted;
    for (const auto& r : *receipts) {
        admitted.push_back({r.observationKey, r.dispatchTick, r.wasForwarded, r.admissionFrontier,
                            r.admissionHorizon});
    }
    auto snapshot = live.snapshot();
    REQUIRE(snapshot);
    auto restored = JudgementSession::recover(*snapshot, inputs);
    REQUIRE(restored);
    auto p = live.advance(Tick{1103});
    REQUIRE(p);
    auto q = restored->advance(Tick{1103});
    REQUIRE(q);
    auto archive = *live.archive();
    auto evaluation = JudgementSession::evaluateReplay(archive);
    REQUIRE(evaluation);
    auto seek = f.session();
    REQUIRE(seek.seek(archive, Tick{1103}));
    CHECK(sameKernelResult(p->kernelView(), q->kernelView()));
    CHECK(sameKernelResult(p->kernelView(), *evaluation->result));
    CHECK(sameSnapshot(live.snapshot()->state(), seek.snapshot()->state()));
    auto reference = scanReference(inputs.prepared, inputs.configuration, admitted, Tick{1103});
    CHECK(reference.facts == p->kernelView().facts);
    CHECK(reference.phases == p->kernelView().phases);
    CHECK(reference.resources == p->kernelView().resources);
    CHECK(reference.contacts == p->kernelView().contacts);
    CHECK(reference.ownership == p->kernelView().ownership);
    CHECK(reference.receipts == p->kernelView().receipts);
    CHECK(reference.observers == p->kernelView().observers);
    expectOracle(p->kernelView());
}
TEST_CASE("INT64 maximum horizon preserves finalization gap and pending timer",
          "[fold][hook][extreme]") {
    FoldFixture f{true};
    f.tap();
    auto& r = f.assembly.sources[0].document.requirements[0];
    r.timing =
        RequirementRecord::Timing{Tick{INT64_MAX},
                                  {{Tick{INT64_MAX - 12}, Tick{INT64_MAX}, {PhaseKind::tap, 1}}},
                                  {},
                                  {{PhaseKind::tap, Tick{INT64_MAX - 12}}}};
    auto success = f.session();
    REQUIRE(success.submit({raw(INT64_MAX - 12, 1)}));
    REQUIRE(success.advance(Tick{INT64_MAX}));
    CHECK(success.query()->kernelView().processedFrontier == Tick{INT64_MAX - 3});
    CHECK(success.query()->kernelView().fold->produced.size() == 1);
    CHECK(success.query()->kernelView().fold->hookConsumerCursor == 1);
    auto pending = f.session();
    REQUIRE(pending.advance(Tick{INT64_MAX}));
    CHECK(pending.query()->kernelView().facts.empty());
    CHECK_FALSE(offsetTicks(Tick{INT64_MAX}, TickSpan{1}));
}

TEST_CASE("Snapshot rejects lost pending lost activation invalid Fact rank and fake fault",
          "[recovery][snapshot][closure]") {
    FoldFixture f;
    f.tap();
    auto s = f.session();
    REQUIRE(s.submitCanonical({canonical(5, 1), canonical(100, 2)}));
    REQUIRE(s.advance(Tick{10}));
    auto saved = s.snapshot();
    REQUIRE(saved);
    auto inputs = std::make_shared<const RecoveryInputs>(f.inputs());
    SECTION("lost pending") {
        auto dto = saved->state();
        dto.pending.clear();
        CHECK_FALSE(detail::ExecutionKernel::restore({dto, inputs}, *inputs));
    }
    SECTION("lost contact") {
        auto dto = saved->state();
        dto.kernel.contacts.clear();
        CHECK_FALSE(detail::ExecutionKernel::restore({dto, inputs}, *inputs));
    }
    SECTION("receipt count") {
        auto dto = saved->state();
        ++dto.kernel.receipts[0].consideredCount;
        CHECK_FALSE(detail::ExecutionKernel::restore({dto, inputs}, *inputs));
    }
    SECTION("lost activation") {
        auto dto = saved->state();
        dto.activeRequirements.clear();
        CHECK_FALSE(detail::ExecutionKernel::restore({dto, inputs}, *inputs));
    }
    SECTION("rank") {
        auto dto = saved->state();
        dto.kernel.facts.update(0, [](auto& fact) {
            std::get<PhaseOutcomeFact>(fact).canonicalOrdinal.phaseRank = 255;
        });
        CHECK_FALSE(detail::ExecutionKernel::restore({dto, inputs}, *inputs));
    }
    SECTION("kind") {
        auto dto = saved->state();
        dto.kernel.facts.update(0, [](auto& fact) {
            std::get<PhaseOutcomeFact>(fact).canonicalOrdinal.factKind = FactKind::stray;
        });
        CHECK_FALSE(detail::ExecutionKernel::restore({dto, inputs}, *inputs));
    }
    SECTION("grade") {
        auto dto = saved->state();
        dto.kernel.facts.update(
            0, [](auto& fact) { std::get<PhaseOutcomeFact>(fact).grade = "fake"; });
        CHECK_FALSE(detail::ExecutionKernel::restore({dto, inputs}, *inputs));
    }
    SECTION("fake fault") {
        auto dto = saved->state();
        dto.kernel.state = KernelSessionState::Faulted;
        dto.kernel.faultDiagnostic = cuexis::core::Error{"", ""};
        CHECK_FALSE(detail::ExecutionKernel::restore({dto, inputs}, *inputs));
    }
}
