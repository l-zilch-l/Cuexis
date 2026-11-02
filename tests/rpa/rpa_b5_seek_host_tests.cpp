#include "rpa_b5_host_fixture.hpp"

// Lifetime, pending reply, boundary cases and the seek matrix include.

TEST_CASE("R1 application lifetime drains while callback captures survive exception unwinding",
          "[rpa][r1][lifetime][clock]") {
    struct Unwind {};
    std::atomic<bool> alive{}, entered{};
    std::atomic<unsigned> accessesAfterDestruction{}, pumps{}, accepts{}, confirms{};
    std::promise<void> release;
    auto ready = release.get_future().share();
    bool unwinding{}, released{}, shifted{};
    std::int64_t logicalUs{};
    std::string originalError;
    std::unique_ptr<player::ExecutionHost> host;
    presentation_renderer::TestPresentationRenderer renderer;
    try {
        struct Captures {
            std::atomic<bool>& alive;
            explicit Captures(std::atomic<bool>& value) : alive(value) {
                alive = true;
            }
            ~Captures() {
                alive = false;
            }
        } captures{alive};
        auto settings = options();
        settings.session.beforeDispatch = [&](std::uint64_t, const SessionCommand& command) {
            if (!alive)
                ++accessesAfterDestruction;
            if (const auto* value = std::get_if<SessionAcceptControl>(&command);
                value && value->request.kind == ControlKind::close) {
                ++accepts;
                entered = true;
                ready.wait();
            }
            if (const auto* value = std::get_if<SessionConfirmControl>(&command);
                value && value->proof.request.kind == ControlKind::close)
                ++confirms;
        };
        auto wired = ports(pumps);
        wired.closeNow = [&] {
            return std::chrono::steady_clock::time_point{std::chrono::microseconds{logicalUs}};
        };
        wired.pumpMain = [&]() -> core::Result<void> {
            if (!alive)
                ++accessesAfterDestruction;
            ++pumps;
            if (unwinding && !released) {
                released = true;
                release.set_value();
            }
            return {};
        };
        wired.beforeClosePhase = [&](player::ExecutionClosePhase phase) -> core::Result<void> {
            if (!shifted && phase == player::ExecutionClosePhase::awaitAcceptance) {
                const auto guard = std::chrono::steady_clock::now() + std::chrono::seconds{5};
                while (!entered && std::chrono::steady_clock::now() < guard)
                    std::this_thread::yield();
                REQUIRE(entered);
                logicalUs = 1000001;
                shifted = true;
            }
            return {};
        };
        auto made = player::ExecutionHost::create(renderer, settings, std::move(wired));
        REQUIRE(made);
        host = std::move(*made);
        // This is the same guard used by runExecutionPlayer, declared after its captures.
        player::ExecutionHostLifetime lifetime{host};
        struct ArmUnwind {
            bool& value;
            ~ArmUnwind() {
                value = true;
            }
        } arm{unwinding};
        REQUIRE(host->load(test_support::presentation()));
        const auto first = host->close();
        REQUIRE_FALSE(first);
        originalError = first.error().code();
        CHECK(host->state() == HostState::closing);
        CHECK(host->closeBudgetExceeded());
        CHECK(alive);
        throw Unwind{};
    } catch (const Unwind&) {
    }
    CHECK(originalError == "host.rpa.control_pending");
    CHECK_FALSE(host);
    CHECK_FALSE(alive);
    CHECK(released);
    CHECK(accessesAfterDestruction == 0);
    CHECK(accepts == 1);
    CHECK(confirms == 1);
}

#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
TEST_CASE("R1 unavailable Close observation is distinct from a rejected Close step",
          "[rpa][r1][observation][missing]") {
    namespace obs = cuexis::internal::execution_diagnostics::rpa;
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    std::atomic<bool> closeSeen{}, injected{};
    auto settings = options();
    settings.session.beforeDispatch = [&](std::uint64_t, const SessionCommand& command) {
        if (std::holds_alternative<SessionSoftwareClose>(command))
            closeSeen = true;
        if (closeSeen && std::holds_alternative<SessionRpaObservation>(command) &&
            !injected.exchange(true))
            throw std::bad_alloc{};
    };
    auto made = player::ExecutionHost::create(renderer, settings, ports(pumps));
    REQUIRE(made);
    REQUIRE((*made)->load(test_support::presentation()));
    REQUIRE((*made)->close());
    CHECK(injected);
    const auto& seen = (*made)->rpaObservation();
    CHECK(seen.fence.closeState == obs::CloseStepState::observationMissing);
    CHECK(seen.fence.closeAttempted);
    CHECK(seen.fence.closeApplied); // The reliable production receipt is still known.
    CHECK(seen.closeRetireFence.retireApplied);
    CHECK(seen.closedFlag);
    CHECK_FALSE((*made)->closeFirstError());
}
#endif

TEST_CASE("R1 a dispatched Close step remains pending until its original typed reply arrives",
          "[rpa][r1][observation][inflight]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{}, dispatched{}, acknowledged{};
    std::atomic<bool> entered{};
    std::atomic<std::uint64_t> originalId{};
    std::promise<void> release;
    auto ready = release.get_future().share();
    bool interrupted{}, released{};
    auto settings = options();
    settings.session.beforeDispatch = [&](std::uint64_t id, const SessionCommand& command) {
        if (std::holds_alternative<SessionSoftwareClose>(command)) {
            originalId = id;
            ++dispatched;
            entered = true;
            ready.wait();
        }
    };
    settings.session.replyAcknowledged = [&](std::uint64_t id) {
        if (id == originalId && originalId != 0)
            ++acknowledged;
    };
    auto wired = ports(pumps);
    wired.pumpMain = [&]() -> core::Result<void> {
        ++pumps;
        if (entered && !interrupted) {
            interrupted = true;
            return core::unexpected(core::Error{"probe.close_step", "In-flight Close step"});
        }
        return {};
    };
    auto made = player::ExecutionHost::create(renderer, settings, std::move(wired));
    REQUIRE(made);
    struct ReleaseGuard {
        std::promise<void>& release;
        bool& released;
        ~ReleaseGuard() {
            if (!released) {
                released = true;
                release.set_value();
            }
        }
    } guard{release, released};
    REQUIRE((*made)->load(test_support::presentation()));
    const auto first = (*made)->close();
    REQUIRE_FALSE(first);
    CHECK((*made)->state() == HostState::closing);
    CHECK(dispatched == 1);
    CHECK(acknowledged == 0);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    namespace obs = cuexis::internal::execution_diagnostics::rpa;
    CHECK((*made)->rpaObservation().fence.closeState == obs::CloseStepState::resultPending);
    CHECK_FALSE((*made)->rpaObservation().fence.closeAttempted);
#endif
    released = true;
    release.set_value();
    REQUIRE((*made)->close());
    CHECK(dispatched == 1);
    CHECK(acknowledged == 1);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    CHECK((*made)->rpaObservation().fence.closeState == obs::CloseStepState::applied);
    CHECK((*made)->rpaObservation().fence.closeApplied);
#endif
}

TEST_CASE("R1 an exception at a Close boundary retains the original cause and phase",
          "[rpa][r1][exception][continuation]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{}, accepts{}, retires{};
    auto settings = options();
    settings.session.beforeDispatch = [&](std::uint64_t, const SessionCommand& command) {
        if (const auto* value = std::get_if<SessionAcceptControl>(&command);
            value && value->request.kind == ControlKind::close)
            ++accepts;
        if (std::holds_alternative<SessionSoftwareRetire>(command))
            ++retires;
    };
    bool injected{};
    auto wired = ports(pumps);
    wired.beforeClosePhase = [&](player::ExecutionClosePhase phase) -> core::Result<void> {
        if (phase == player::ExecutionClosePhase::exportEvidence && !injected) {
            injected = true;
            throw std::bad_alloc{};
        }
        return {};
    };
    auto made = player::ExecutionHost::create(renderer, settings, std::move(wired));
    REQUIRE(made);
    REQUIRE((*made)->load(test_support::presentation()));
    const auto first = (*made)->close();
    REQUIRE_FALSE(first);
    CHECK(first.error().code() == "host.rpa.capacity_exceeded");
    CHECK((*made)->state() == HostState::closing);
    CHECK((*made)->closePhase() == player::ExecutionClosePhase::exportEvidence);
    REQUIRE((*made)->close());
    REQUIRE((*made)->closeFirstError());
    CHECK((*made)->closeFirstError()->code() == first.error().code());
    CHECK(accepts == 1);
    CHECK(retires == 1);
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    CHECK(std::string{(*made)->rpaObservation().firstErrorCode} == first.error().code());
#endif
}

TEST_CASE("B5 declared 256 requirement boundary completes simultaneous timers and Replay",
          "[b5][host][measurement]") {
    const auto requirementBound = GENERATE(std::size_t{1}, std::size_t{256});
    auto fixture = std::make_unique<test_support::SessionTestContent>();
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    test_support::rpa(*fixture, mapping);
    auto& document = fixture->request.assembly.sources[0].document;
    const auto prototype = document.requirements[0];
    document.requirements.clear();
    std::erase_if(document.declarations, [](const auto& declaration) {
        return declaration.kind == judgement::DeclarationKind::requirement;
    });
    document.relations[0].members.clear();
    fixture->owners.clear();
    fixture->request.assembly.contentProfileLimits.maxRequirements =
        judgement::MeasuredParameter<std::uint64_t>::measured(requirementBound);
    for (std::size_t i = 0; i < requirementBound; ++i) {
        auto requirement = prototype;
        requirement.resourceClaims[0].claimPolicy.competition =
            judgement::ClaimPolicyDeclaration::CompetitionKey{-1, static_cast<std::uint64_t>(i)};
        requirement.stableId.declarationOrdinal = static_cast<std::uint32_t>(100 + i);
        requirement.identity.requirementLocalId = "boundary." + std::to_string(i);
        document.declarations.push_back(judgement::testing::testDeclaration(
            judgement::DeclarationKind::requirement, requirement.identity.requirementLocalId,
            requirement.stableId.declarationOrdinal));
        document.relations[0].members.push_back(requirement.stableId);
        fixture->owners.push_back({requirement.identity, fixture->chart.entities[0].identity});
        document.requirements.push_back(std::move(requirement));
    }
    const auto prepareStart = std::chrono::steady_clock::now();
    auto packed = fixture->packed();
    auto content = GameplayContent::fromPacked(packed, fixture->config);
    REQUIRE(content);
    const auto prepareUs = std::chrono::duration_cast<std::chrono::microseconds>(
                               std::chrono::steady_clock::now() - prepareStart)
                               .count();
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto settings = options();
    settings.realtime =
        SessionRealtimeStart{{1000000, 10, 1024, "", true}, mapping, {}, "b5.boundary"};
    auto host = player::ExecutionHost::create(renderer, settings, ports(pumps));
    REQUIRE(host);
    REQUIRE((*host)->load(test_support::presentation(), *content));
    ClockObservation eof{};
    eof.monotonicDomain = "b5.boundary";
    eof.segment = (*host)->context().clockSegment;
    eof.publicationSequence = 1;
    eof.sampleMonotonicUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::steady_clock::now().time_since_epoch())
                                .count();
    eof.validUntilMonotonicUs = eof.sampleMonotonicUs + 1000000;
    eof.source = ClockFramePosition{0, 1000000};
    eof.presented = eof.source;
    eof.quality = ClockQuality::Exact;
    eof.playback = ClockPlaybackState::EndOfStream;
    auto tail =
        (*host)->beginTail(eof, {{1, 1}, 100000, 0, "declared 256 requirement boundary", true});
    REQUIRE(tail);
    GameplayAdvanceDecision final{};
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds{3};
    do {
        auto decision = (*host)->advance(eof);
        REQUIRE(decision);
        final = std::move(*decision);
    } while ((final.disposition != GameplayAdvanceDisposition::Executed ||
              final.request.horizon.value != tail->terminalHorizon) &&
             std::chrono::steady_clock::now() < limit);
    REQUIRE(final.disposition == GameplayAdvanceDisposition::Executed);
    REQUIRE(final.request.horizon.value == tail->terminalHorizon);
    auto score = final.result.score();
    REQUIRE(score);
    CHECK(score->misses == requirementBound);
    CHECK(score->score == -static_cast<std::int64_t>(requirementBound));
    auto replay = (*host)->archive();
    REQUIRE(replay);
    auto evaluation = (*host)->evaluateReplay(*replay);
    REQUIRE(evaluation);
    REQUIRE(evaluation->evidenceValid);
    REQUIRE(final.result.sameResult(evaluation->result).value());
    const auto sampleStart = std::chrono::steady_clock::now();
    auto frame = (*host)->sample(
        tail->terminalHorizon,
        {static_cast<double>(tail->terminalHorizon) / 1000.0, 0, (*host)->context().clockSegment},
        false);
    REQUIRE(frame);
    REQUIRE((*host)->present(*frame));
    const auto sampleUs = std::chrono::duration_cast<std::chrono::microseconds>(
                              std::chrono::steady_clock::now() - sampleStart)
                              .count();
    REQUIRE((*host)->close());
    std::cout << "{\"event\":\"b5.cpu.content_boundary\",\"test_only\":true,\"requirements\":"
              << requirementBound << ",\"prepare_us\":" << prepareUs
              << ",\"packed_bytes\":" << packed.size() << ",\"sample_present_us\":" << sampleUs
              << ",\"misses\":" << score->misses << "}\n";
}

TEST_CASE("B5 main closes after audio factory failure before Load acceptance",
          "[b5][host][fault]") {
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto wired = ports(pumps);
    wired.audioDevice =
        [](audio::AudioClipStore&) -> core::Result<std::unique_ptr<OwnedAudioDevice>> {
        return core::unexpected(core::Error{"probe.factory_failure", "Injected startup failure"});
    };
    auto settings = options();
    settings.sourceMode = PlaybackMode::HostClock;
    auto host = player::ExecutionHost::create(renderer, settings, std::move(wired));
    REQUIRE(host);
    auto source = PlaybackSource::fromFilesystemProject(std::filesystem::path{CUEXIS_SOURCE_DIR} /
                                                        "assets/projects/stage1d_project");
    REQUIRE(source);
    auto loaded = (*host)->load(std::move(*source));
    INFO((loaded
              ? ""
              : std::string{loaded.error().code()} + ": " + std::string{loaded.error().message()}));
    REQUIRE_FALSE(loaded);
    REQUIRE((*host)->hasAudio());
    auto publication = (*host)->audioPublication();
    REQUIRE(publication);
    REQUIRE(publication->fault == AudioPublication::Fault::device);
    auto closed = (*host)->close();
    INFO((closed
              ? ""
              : std::string{closed.error().code()} + ": " + std::string{closed.error().message()}));
    REQUIRE(closed);
    CHECK((*host)->state() == HostState::closed);
}

TEST_CASE("R3 CPU host Seek measures first and repeated requests through target present",
          "[.][b5][redesign-host-perf]") {
    if (test_support::environmentVariable("CUEXIS_REDESIGN_BARRIER") == "1") {
        static const bool admitted = [] {
            std::string line;
            return static_cast<bool>(std::getline(std::cin, line)) && line == "go";
        }();
        REQUIRE(admitted);
    }
    const auto policy = test_support::environmentVariable("CUEXIS_SEEK_IDENTITY");
    REQUIRE((policy.empty() || policy == "canonical" || policy == "source"));
    const auto recordSetting = test_support::environmentVariable("CUEXIS_SEEK_RECORDS");
    REQUIRE((recordSetting.empty() || recordSetting == "33" || recordSetting == "129"));
    const unsigned records = recordSetting == "129" ? 129 : 33;
    const std::int64_t horizon = 1103 + records - 1;
    const auto requirementSetting = test_support::environmentVariable("CUEXIS_SEEK_REQUIREMENTS");
    REQUIRE((requirementSetting.empty() || requirementSetting == "1000" ||
             requirementSetting == "10000" || requirementSetting == "40000"));
    const auto count = requirementSetting.empty()
                           ? GENERATE(std::size_t{64}, std::size_t{256}, std::size_t{2048})
                           : (requirementSetting == "1000"    ? std::size_t{1000}
                              : requirementSetting == "10000" ? std::size_t{10000}
                                                              : std::size_t{40000});
    const auto contentPath = test_support::environmentVariable("CUEXIS_SEEK_CONTENT");
    REQUIRE((contentPath.empty() || contentPath == "packed" || contentPath == "typed"));
    REQUIRE((contentPath != "typed" || !requirementSetting.empty()));
    const auto setupStart = std::chrono::steady_clock::now();
    std::cout << "{\"event\":\"r3.redesign.setup_start\",\"requirements\":" << count
              << ",\"records\":" << records << "}" << std::endl;
    auto fixture = std::make_unique<test_support::SessionTestContent>();
    const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
    test_support::rpa(*fixture, mapping);
    auto& document = fixture->request.assembly.sources[0].document;
    const auto prototype = document.requirements[0];
    document.requirements.clear();
    std::erase_if(document.declarations,
                  [](const auto& d) { return d.kind == judgement::DeclarationKind::requirement; });
    document.relations[0].members.clear();
    fixture->owners.clear();
    fixture->request.assembly.contentProfileLimits.maxRequirements =
        judgement::MeasuredParameter<std::uint64_t>::measured(count);
    for (std::size_t i = 0; i < count; ++i) {
        auto requirement = prototype;
        requirement.stableId.declarationOrdinal = static_cast<std::uint32_t>(100 + i);
        requirement.identity.requirementLocalId = "redesign.host." + std::to_string(i);
        requirement.resourceClaims[0].claimPolicy.competition =
            judgement::ClaimPolicyDeclaration::CompetitionKey{-1, static_cast<std::uint64_t>(i)};
        document.declarations.push_back(judgement::testing::testDeclaration(
            judgement::DeclarationKind::requirement, requirement.identity.requirementLocalId,
            requirement.stableId.declarationOrdinal));
        document.relations[0].members.push_back(requirement.stableId);
        fixture->owners.push_back({requirement.identity, fixture->chart.entities[0].identity});
        document.requirements.push_back(std::move(requirement));
    }
    presentation_renderer::TestPresentationRenderer renderer;
    std::atomic<unsigned> pumps{};
    auto settings = options();
    settings.seekIdentity = policy == "canonical" ? player::SeekIdentityPolicy::canonicalContent
                                                  : player::SeekIdentityPolicy::owningSource;
    settings.realtime =
        SessionRealtimeStart{{1000000, 10, 128, "", true}, mapping, {}, "redesign.host"};
    auto host = player::ExecutionHost::create(renderer, settings, ports(pumps));
    REQUIRE(host);
    const auto content = [&]() -> GameplayContent {
        if (requirementSetting.empty())
            return fixture->content();
        // The existing revision 4 path streams the semantic preimage separately from
        // the unchanged 16 MiB physical Packed envelope used by large fixtures.
        const auto prepared = judgement::prepareResolvedGameplay(fixture->request);
        INFO((prepared ? "" : std::string{prepared.error().message()}));
        REQUIRE(prepared);
        REQUIRE(prepared->assembled().graph.requirements.size() == count);
        if (contentPath == "typed") {
            // Test-only typed fixture: retain the same validated configuration and
            // pattern/measure definitions, replacing the owning prepared graph.
            test_support::SessionTestContent prototypeFixture;
            test_support::rpa(prototypeFixture, mapping);
            const auto prototypeContent = prototypeFixture.content();
            const auto* basis = GameplayAccess::content(prototypeContent);
            REQUIRE(basis);
            auto capsule = basis->capsule;
            capsule.chart = fixture->chart;
            capsule.gameplay = *prepared;
            capsule.owners = fixture->owners;
            capsule.verifiedSemanticDigest.reset();
            return GameplayAccess::content(std::move(capsule), basis->configuration,
                                           basis->bindings, basis->enabledCapabilities,
                                           basis->publicConfiguration);
        }
        const auto packed =
            gameplay_packed::encode({fixture->chart,
                                     *prepared,
                                     fixture->owners,
                                     {"normalization.one", "coordinator.policy.greedy_v1"},
                                     {},
                                     {},
                                     4});
        INFO((packed ? "" : std::string{packed.error().message()}));
        REQUIRE(packed);
        const auto decoded = GameplayContent::fromPacked(*packed, fixture->config);
        INFO((decoded ? "" : std::string{decoded.error().message()}));
        REQUIRE(decoded);
        return *decoded;
    }();
    const auto loaded = (*host)->load(test_support::presentation(), content);
    INFO((loaded
              ? ""
              : std::string{loaded.error().code()} + ": " + std::string{loaded.error().message()}));
    REQUIRE(loaded);
    const auto setupUs = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now() - setupStart)
                             .count();
    std::cout << "{\"event\":\"r3.redesign.setup_done\",\"requirements\":" << count
              << ",\"records\":" << records << ",\"setup_us\":" << setupUs << "}" << std::endl;
    std::int64_t target = 1103;
    std::uint64_t sequence{};
    player::ExecutionHost::ClockSource clock =
        [&](const HostContext& context) -> core::Result<ClockObservation> {
        ClockObservation observation{};
        observation.monotonicDomain = "redesign.host";
        observation.segment = context.clockSegment;
        observation.publicationSequence = ++sequence;
        observation.sampleMonotonicUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                            std::chrono::steady_clock::now().time_since_epoch())
                                            .count();
        observation.validUntilMonotonicUs = observation.sampleMonotonicUs + 1000000;
        observation.presented = ClockFramePosition{target, 1000000};
        observation.source = observation.presented;
        observation.maximumPresented = observation.presented;
        observation.playback = ClockPlaybackState::Playing;
        observation.quality = ClockQuality::Exact;
        return observation;
    };
    for (unsigned record = 0; record < records; ++record) {
        target = 1103 + record;
        const auto observation = clock((*host)->context());
        REQUIRE(observation);
        const auto decision = (*host)->advance(*observation);
        INFO((decision ? "" : std::string{decision.error().code()}));
        REQUIRE(decision);
        REQUIRE(decision->disposition == GameplayAdvanceDisposition::Executed);
    }
    const auto archiveStart = std::chrono::steady_clock::now();
    const auto archive = (*host)->archive();
    const auto archiveUs = std::chrono::duration_cast<std::chrono::microseconds>(
                               std::chrono::steady_clock::now() - archiveStart)
                               .count();
    REQUIRE(archive);
    const auto cutStart = std::chrono::steady_clock::now();
    const auto cut = archive->cutAt({horizon});
    REQUIRE(cut);
    const auto cutUs = std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::steady_clock::now() - cutStart)
                           .count();
    const auto expected = (*host)->queryGameplay();
    REQUIRE(expected);
    const auto expectedScore = expected->score();
    REQUIRE(expectedScore);
    CHECK(expectedScore->misses == count);
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        target = horizon;
        REQUIRE((*host)->pause(clock));
        const auto started = std::chrono::steady_clock::now();
        auto sought = (*host)->seek(*archive, *cut, {horizon}, clock);
        const auto seekUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::steady_clock::now() - started)
                                .count();
        INFO((sought ? ""
                     : std::string{sought.error().code()} + ": " +
                           std::string{sought.error().message()}));
        REQUIRE(sought);
        auto frame = (*host)->sample(
            horizon, {static_cast<double>(horizon) / 1000.0, 0, (*host)->context().clockSegment},
            false);
        REQUIRE(frame);
        const auto presented = (*host)->present(*frame);
        REQUIRE(presented);
        CHECK(presented->status == PresentStatus::succeeded);
        const auto totalUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                 std::chrono::steady_clock::now() - started)
                                 .count();
        const auto actual = (*host)->queryGameplay();
        REQUIRE(actual);
        CHECK(actual->sameResult(*expected).value());
        std::cout << "{\"event\":\"r3.redesign.host\",\"requirements\":" << count
                  << ",\"attempt\":" << attempt << ",\"seek_us\":" << seekUs
                  << ",\"request_to_present_us\":" << totalUs << ",\"archive_us\":" << archiveUs
                  << ",\"capsule_revision\":"
                  << (contentPath == "typed" ? 0 : (requirementSetting.empty() ? 3 : 4))
                  << ",\"content_path\":\"" << (contentPath == "typed" ? "typed" : "packed") << "\""
                  << ",\"setup_us\":" << setupUs << ",\"cut_us\":" << cutUs
                  << ",\"records\":" << records << ",\"identity\":\""
                  << (policy == "canonical" ? "canonical" : "source") << "\""
                  << ",\"renderer\":\"cpu-test\",\"audio\":false,\"formal\":false}\n";
    }
    REQUIRE((*host)->close());
}
#include "r3_seek_matrix_tests.hpp"
