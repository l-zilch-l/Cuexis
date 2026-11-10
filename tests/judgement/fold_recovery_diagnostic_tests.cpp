#include "fold_recovery_fixture.hpp"

using namespace fold_fixture;

namespace {
auto originalRecovery(std::string_view name) -> std::vector<std::byte> {
    const auto path = std::filesystem::path{__FILE__}.parent_path().parent_path() /
                      "fixtures/diagnostic_revision1" / name;
    std::ifstream file{path, std::ios::binary | std::ios::ate};
    REQUIRE(file);
    const auto length = file.tellg();
    REQUIRE(length > 0);
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), length);
    REQUIRE(file.good());
    return bytes;
}
auto diagnosticEnvelope(const KernelProjection& p, std::uint64_t events,
                        const detail::wire::Writer& body, std::uint8_t kind,
                        std::uint32_t revision = 2) -> std::vector<std::byte> {
    detail::wire::Writer header, payload, envelope;
    header(p.judgementIdentity.canonicalBytes().bytes(), p.effectiveFactSemanticRevision,
           std::uint32_t{1}, events, static_cast<std::uint64_t>(p.facts.size()),
           std::string{"canonical.discrete.le.v1"});
    for (const auto& [id, data] :
         {std::pair<std::uint8_t, const detail::wire::Writer*>{1, &header},
          std::pair<std::uint8_t, const detail::wire::Writer*>{2, &body}}) {
        payload(id, static_cast<std::uint64_t>(data->bytes.size()));
        payload.bytes.insert(payload.bytes.end(), data->bytes.begin(), data->bytes.end());
    }
    for (char c : std::string{"CXGREC01"})
        envelope.bytes.push_back(static_cast<std::byte>(c));
    envelope(revision, kind, static_cast<std::uint64_t>(payload.bytes.size()),
             detail::wire::digest(payload.bytes));
    envelope.bytes.insert(envelope.bytes.end(), payload.bytes.begin(), payload.bytes.end());
    return envelope.bytes;
}
auto oldSnapshotDTO(const std::vector<std::byte>& bytes, const KernelProjection& initial)
    -> SnapshotDTO {
    detail::wire::Reader reader{std::span{bytes}.subspan(29), 0, 0, {}, initial};
    std::uint8_t id;
    std::uint64_t length;
    reader(id, length);
    REQUIRE(id == 1);
    reader.pos += static_cast<std::size_t>(length);
    reader(id, length);
    REQUIRE(id == 2);
    auto dto = reader.seed<SnapshotDTO>();
    reader(dto);
    REQUIRE(reader.pos == reader.bytes.size());
    return dto;
}
void expectDialectFailure(const cuexis::core::Error& e, std::string_view code) {
    CHECK(e.code() == code);
    REQUIRE(e.context().size() == 3);
    CHECK(e.context()[0].key == "category");
    CHECK(e.context()[0].value == "invalid_relation");
    CHECK(e.context()[1].key == "severity");
    CHECK(e.context()[1].value == "error");
    CHECK(e.context()[2].key == "faulted");
    CHECK(e.context()[2].value == "false");
    CHECK_FALSE(e.cause());
}
} // namespace
TEST_CASE("diagnostic revision2 rejects real revision1 originals and preserves healthy body",
          "[diagnostic][recovery][compatibility]") {
    for (const auto name : {"healthy", "kernel", "fold"}) {
        FoldFixture f{std::string_view{name} == "healthy"};
        if (std::string_view{name} != "healthy")
            f.tap();
        const auto inputs = f.inputs();
        auto replay = originalRecovery(std::string{name} + ".replay");
        auto snapshot = originalRecovery(std::string{name} + ".snapshot");
        for (auto* bytes : {&replay, &snapshot}) {
            const bool isReplay = bytes == &replay;
            auto result = isReplay ? decodeReplay(*bytes, inputs).has_value()
                                   : decodeSnapshot(*bytes, inputs).has_value();
            CHECK_FALSE(result);
            if (isReplay)
                expectDialectFailure(decodeReplay(*bytes, inputs).error(),
                                     "codec.version_unsupported");
            else
                expectDialectFailure(decodeSnapshot(*bytes, inputs).error(),
                                     "codec.version_unsupported");
            auto corrupt = *bytes;
            corrupt[12] = std::byte{99}; // version before tag
            if (isReplay)
                CHECK(decodeReplay(corrupt, inputs).error().code() == "codec.version_unsupported");
            else
                CHECK(decodeSnapshot(corrupt, inputs).error().code() ==
                      "codec.version_unsupported");
            corrupt[0] = std::byte{0}; // magic before version
            if (isReplay)
                CHECK(decodeReplay(corrupt, inputs).error().code() == "codec.magic_invalid");
            else
                CHECK(decodeSnapshot(corrupt, inputs).error().code() == "codec.magic_invalid");
            const CodecBudget tiny{1, 1, 1, true};
            if (isReplay)
                CHECK(decodeReplay(corrupt, inputs, tiny).error().code() ==
                      "codec.budget_exceeded");
            else
                CHECK(decodeSnapshot(corrupt, inputs, tiny).error().code() ==
                      "codec.budget_exceeded");
        }
        replay[8] = std::byte{2};
        snapshot[8] = std::byte{2};
        if (std::string_view{name} == "kernel") {
            expectDialectFailure(decodeReplay(replay, inputs).error(), "codec.record_invalid");
            expectDialectFailure(decodeSnapshot(snapshot, inputs).error(), "codec.record_invalid");
        } else {
            REQUIRE(decodeReplay(replay, inputs));
            REQUIRE(decodeSnapshot(snapshot, inputs));
        }
    }
    FoldFixture f{true};
    auto s = f.session();
    REQUIRE(s.submit({raw(900, 1), raw(1000, 2, InputAction::release)}));
    REQUIRE(s.advance(Tick{1103}));
    auto replay = *encodeReplay(*s.archive());
    auto snapshot = *encodeSnapshot(*s.snapshot());
    const auto oldReplay = originalRecovery("healthy.replay");
    const auto oldSnapshot = originalRecovery("healthy.snapshot");
    CHECK(replay[8] == std::byte{2});
    CHECK(snapshot[8] == std::byte{2});
    REQUIRE(replay.size() == oldReplay.size());
    REQUIRE(snapshot.size() == oldSnapshot.size());
    // The only healthy difference is envelope version, including identical payload digest.
    for (std::size_t i = 0; i < replay.size(); ++i)
        if (i != 8)
            CHECK(replay[i] == oldReplay[i]);
    for (std::size_t i = 0; i < snapshot.size(); ++i)
        if (i != 8)
            CHECK(snapshot[i] == oldSnapshot[i]);
    auto owning = decodeSnapshot(snapshot, f.inputs());
    REQUIRE(owning);
    REQUIRE(s.restore(*owning, f.inputs()));
    CHECK(sameSnapshot(owning->state(), s.snapshot()->state()));
}
TEST_CASE("diagnostic fault originals differ only in permitted kernel code across full DTO",
          "[diagnostic][recovery][fault][full-result]") {
    for (const auto point :
         {detail::KernelFailurePoint::beforeSeal, detail::KernelFailurePoint::beforeFold}) {
        FoldFixture f;
        f.tap();
        auto s = f.session();
        detail::KernelTestControls controls{Tick{5}, point, {}, {}, {}};
        REQUIRE(detail::KernelTestAccess::inject(s, controls));
        REQUIRE(s.submit({raw(5, 1)}));
        auto failed = s.advance(Tick{20});
        REQUIRE_FALSE(failed);
        const auto dto = *detail::KernelTestAccess::captureDTO(s);
        const auto name = point == detail::KernelFailurePoint::beforeSeal ? "kernel" : "fold";
        auto old = oldSnapshotDTO(originalRecovery(std::string{name} + ".snapshot"), dto.kernel);
        REQUIRE(old.kernel.faultDiagnostic);
        REQUIRE(dto.kernel.faultDiagnostic);
        if (point == detail::KernelFailurePoint::beforeSeal) {
            CHECK(old.kernel.faultDiagnostic->code() == "judgement.s7a4.kernel.transaction_failed");
            CHECK(dto.kernel.faultDiagnostic->code() == "judgement.kernel.transaction_failed");
            auto expected = cuexis::core::Error{"judgement.kernel.transaction_failed",
                                                std::string{old.kernel.faultDiagnostic->message()}};
            for (const auto& field : old.kernel.faultDiagnostic->context())
                expected.withContext(field.key, field.value);
            REQUIRE_FALSE(old.kernel.faultDiagnostic->cause());
            old.kernel.faultDiagnostic = std::move(expected);
        }
        CHECK(sameSnapshot(old, dto)); // all remaining DTO/kernel/Fold/error fields checked
        auto receipt = dto.kernel;
        receipt.faultDiagnostic = failed.error();
        CHECK(sameKernelResult(receipt, dto.kernel));
        auto archive = *s.archive();
        auto read = decodeReplay(*encodeReplay(archive), f.inputs());
        REQUIRE(read);
        CHECK(sameReplayHistory(*read, archive));
        auto evaluated = detail::ExecutionKernel::evaluate(*read, &controls);
        REQUIRE(evaluated);
        CHECK(evaluated->evidenceValid);
        CHECK(sameKernelResult(*evaluated->result, dto.kernel));
        REQUIRE_FALSE(
            JudgementSession::evaluateReplay(*read)); // no fabricated success without injection
        auto owning = detail::KernelTestAccess::snapshotValue(dto, f.inputs());
        auto decoded = decodeSnapshot(*encodeSnapshot(owning), f.inputs());
        REQUIRE(decoded);
        CHECK(sameSnapshot(decoded->state(), dto));
        auto restored = JudgementSession::recover(*decoded, f.inputs());
        REQUIRE(restored);
        CHECK(sameKernelResult(restored->query()->kernelView(), dto.kernel));
    }
}
TEST_CASE("diagnostic pollution visits cause sourceCode early records and direct recovery",
          "[diagnostic][recovery][pollution]") {
    FoldFixture f;
    f.tap();
    auto s = f.session();
    REQUIRE(s.advance(Tick{10}));
    REQUIRE(s.advance(Tick{20}));
    const auto archive = *s.archive();
    const auto oldName = std::string{detail::legacyDiagnosticCodes.front()};
    cuexis::core::Error error{"judgement.kernel.transaction_failed", "unchanged"};
    SECTION("direct code") {
        error = cuexis::core::Error{oldName, "unchanged"};
    }
    SECTION("nested cause") {
        error.withCause(cuexis::core::Error{"current", "cause"}.withCause(
            cuexis::core::Error{oldName, "root"}));
    }
    SECTION("repeated sourceCode") {
        error.withContext("sourceCode", oldName).withContext("sourceCode", "current");
    }
    SECTION("deep cause") {
        error = cuexis::core::Error{oldName, "root"};
        for (int i = 0; i < 512; ++i)
            error = cuexis::core::Error{"current", "node"}.withCause(std::move(error));
    }
    REQUIRE(detail::containsLegacyDiagnostic(error));
    auto data = archive.data();
    auto polluted = *data.records.front().result;
    polluted.faultDiagnostic = error; // intentionally artificial negative
    data.records.front().result = std::make_shared<const KernelProjection>(std::move(polluted));
    auto forged = ReplayArchive{std::make_shared<const ReplayData>(data),
                                std::make_shared<const RecoveryInputs>(f.inputs())};
    const auto before = data;
    auto encoded = encodeReplay(forged);
    REQUIRE_FALSE(encoded);
    expectDialectFailure(encoded.error(), "codec.record_invalid");
    detail::wire::Writer body;
    body(data.records);
    auto bytes = diagnosticEnvelope(*data.records.back().result, data.eventCount, body, 1);
    auto read = decodeReplay(bytes, f.inputs());
    REQUIRE_FALSE(read);
    expectDialectFailure(read.error(), "codec.record_invalid");
    CHECK(decodeReplay(bytes, f.inputs(), CodecBudget{bytes.size() + 1, 100, 1, true})
              .error()
              .code() == "codec.budget_exceeded");
    bytes[21] ^= std::byte{1};
    CHECK(decodeReplay(bytes, f.inputs()).error().code() == "codec.digest_mismatch");
    auto evaluated = JudgementSession::evaluateReplay(forged);
    REQUIRE_FALSE(evaluated);
    expectDialectFailure(evaluated.error(), "judgement.execution.relation_invalid");
    CHECK_FALSE(JudgementSession::checkpoint(forged, Tick{20}));
    auto query = s.query();
    REQUIRE_FALSE(s.seek(forged, Tick{20}));
    CHECK(sameKernelResult(query->kernelView(), s.query()->kernelView()));
    CHECK(sameReplayHistory(forged,
                            ReplayArchive{std::make_shared<const ReplayData>(before),
                                          std::make_shared<const RecoveryInputs>(f.inputs())}));
    auto dto = s.snapshot()->state();
    dto.kernel.state = KernelSessionState::Faulted;
    dto.kernel.faultDiagnostic = error;
    const auto owning = detail::KernelTestAccess::snapshotValue(dto, f.inputs());
    REQUIRE_FALSE(encodeSnapshot(owning));
    expectDialectFailure(encodeSnapshot(owning).error(), "codec.record_invalid");
    body.bytes.clear();
    body(dto);
    const auto wire = diagnosticEnvelope(dto.kernel, dto.ingress.accepted.size(), body, 2);
    REQUIRE_FALSE(decodeSnapshot(wire, f.inputs()));
    expectDialectFailure(decodeSnapshot(wire, f.inputs()).error(), "codec.record_invalid");
    auto restored = JudgementSession::recover(owning, f.inputs());
    REQUIRE_FALSE(restored);
    expectDialectFailure(restored.error(), "judgement.execution.relation_invalid");
    REQUIRE_FALSE(s.restore(owning, f.inputs()));
    CHECK(sameKernelResult(query->kernelView(), s.query()->kernelView()));
    CHECK(sameSnapshot(owning.state(), dto));
}
TEST_CASE("diagnostic historical prose is accepted and all finite legacy spellings are detected",
          "[diagnostic][recovery][finite-policy]") {
    for (const auto code : detail::legacyDiagnosticCodes) {
        const auto original = cuexis::core::Error{std::string{code}, "original"};
        CHECK(detail::containsLegacyDiagnostic(original));
        CHECK(original.code() == code);
        CHECK(
            detail::containsLegacyDiagnostic(cuexis::core::Error{"current", "message"}.withContext(
                "sourceCode", std::string{code})));
        CHECK_FALSE(detail::containsLegacyDiagnostic(cuexis::core::Error{
            "current", std::string{code}}.withContext("description", std::string{code})));
    }
}

TEST_CASE(
    "diagnostic mixed Checkpoint remains optional and falls back without changing owning values",
    "[diagnostic][recovery][checkpoint][fallback]") {
    FoldFixture f;
    f.tap();
    auto live = f.session();
    REQUIRE(live.advance(Tick{10}));
    REQUIRE(live.advance(Tick{20}));
    auto archive = *live.archive();
    const auto original = *JudgementSession::checkpoint(archive, Tick{10});
    auto mixed = original;
    auto state = *mixed.state;
    state.kernel.faultDiagnostic =
        cuexis::core::Error{std::string{detail::legacyDiagnosticCodes.front()}, "old owning"};
    mixed.state = std::make_shared<const SnapshotDTO>(state);
    auto seek = f.session();
    REQUIRE(seek.seek(archive, Tick{20}, replayCutAt(archive, Tick{20}), std::span{&mixed, 1}));
    auto origin = f.session();
    REQUIRE(origin.seek(archive, Tick{20}));
    CHECK(sameSnapshot(seek.snapshot()->state(), origin.snapshot()->state()));
    CHECK(sameSnapshot(*mixed.state, state));
    CHECK(sameSnapshot(*original.state, *JudgementSession::checkpoint(archive, Tick{10})->state));
    mixed.horizon = Tick{100}; // inapplicable is still ignored
    REQUIRE(seek.seek(archive, Tick{20}, replayCutAt(archive, Tick{20}), std::span{&mixed, 1}));
    CHECK(sameSnapshot(seek.snapshot()->state(), origin.snapshot()->state()));
}
TEST_CASE("diagnostic session origins and legacy entry messages retain lifecycle precedence",
          "[diagnostic][recovery][lifecycle]") {
    FoldFixture f;
    auto empty = *JudgementSession::create();
    auto legacy = empty.configure();
    REQUIRE_FALSE(legacy);
    CHECK(legacy.error().code() == "judgement.session.legacy_entry_unsupported");
    CHECK(legacy.error().message() == "the legacy judgement entry is unsupported");
    auto lifecycle = empty.prepare();
    REQUIRE_FALSE(lifecycle);
    CHECK(lifecycle.error().code() == "judgement.session.operation_state_invalid");
    CHECK(lifecycle.error().message() ==
          "the judgement session is not in the lifecycle phase this verb requires");
    auto typed = empty.prepare(f.prepared());
    REQUIRE_FALSE(typed);
    CHECK(typed.error().code() == "judgement.session.lifecycle_order");
    CHECK(typed.error().message() == lifecycle.error().message());
    REQUIRE(lifecycle.error().context().size() == typed.error().context().size());
    for (std::size_t i = 0; i < lifecycle.error().context().size(); ++i) {
        CHECK(lifecycle.error().context()[i].key == typed.error().context()[i].key);
        CHECK(lifecycle.error().context()[i].value == typed.error().context()[i].value);
    }
    REQUIRE(empty.configure(f.configuration()));
    auto legacyPrepare = empty.prepare();
    REQUIRE_FALSE(legacyPrepare);
    CHECK(legacyPrepare.error().code() == legacy.error().code());
    REQUIRE(empty.prepare(f.prepared()));
    REQUIRE(empty.query());
    REQUIRE_FALSE(empty.submit());
    CHECK(empty.submit().error().code() == legacy.error().code());
    REQUIRE_FALSE(empty.advance());
    CHECK(empty.advance().error().code() == legacy.error().code());
    REQUIRE_FALSE(empty.seek());
    CHECK(empty.seek().error().code() == legacy.error().code());
    REQUIRE(empty.snapshot());
    REQUIRE(empty.reset());
    CHECK(empty.query().error().code() == "judgement.session.operation_state_invalid");
}

TEST_CASE("diagnostic envelope and structural first errors remain ordered before dialect",
          "[diagnostic][recovery][codec][first-error]") {
    FoldFixture f{true};
    auto s = f.session();
    REQUIRE(s.advance(Tick{1103}));
    const auto bytes = *encodeReplay(*s.archive());
    for (const auto& [offset, value, code] :
         std::array<std::tuple<std::size_t, unsigned, std::string_view>, 4>{
             {{8, 99, "codec.version_unsupported"},
              {12, 99, "codec.tag_invalid"},
              {13, 0, "codec.length_invalid"},
              {29, 99, "codec.section_invalid"}}}) {
        auto invalid = bytes;
        invalid[offset] = static_cast<std::byte>(value);
        auto rejected = decodeReplay(invalid, f.inputs());
        REQUIRE_FALSE(rejected);
        expectDialectFailure(rejected.error(), code);
    }
    auto truncated = bytes;
    truncated.resize(28);
    CHECK(decodeReplay(truncated, f.inputs()).error().code() == "codec.truncated");
    auto inputs = f.inputs();
    inputs.configuration.executionProfileToken.clear();
    CHECK(decodeReplay(truncated, inputs).error().code() ==
          "judgement.execution.profile_incomplete");
    CHECK(decodeReplay(truncated, f.inputs(), CodecBudget{1, 1, 1, false}).error().code() ==
          "codec.budget_unaccepted");
}
