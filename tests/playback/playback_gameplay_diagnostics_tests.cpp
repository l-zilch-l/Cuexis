#include "playback_gameplay_fixture.hpp"

using namespace playback_gameplay_fixture;

TEST_CASE("Gameplay configuration codec preserves exact integers and rejects implicit shape",
          "[playback][gameplay][candidate]") {
    TestContent fixture;
    auto& c = fixture.config;
    c.initialScore = INT64_MIN;
    c.initialCombo = UINT64_MAX;
    c.domains[0].minimum = -9007199254740993LL;
    c.domains[0].maximum = 9007199254740993LL;
    c.hooks = {{"hook",
                playback::GameplayHookConsumer::Shared,
                playback::GameplayCombine::BitOr,
                {"score"},
                UINT64_MAX,
                true}};
    playback::GameplayFactBinding binding{};
    binding.bindingId = "binding";
    binding.source = {"entry",  "invocation",  "module",
                      "export", "requirement", {{"repeat", UINT64_MAX}}};
    binding.phase = playback::GameplayPhase::Tail;
    binding.outcome = playback::GameplayOutcome::Miss;
    binding.timing = playback::GameplayTimingClass::Late;
    binding.target = "target";
    binding.visible = false;
    binding.start = {INT64_MIN};
    binding.end = playback::GameplayPresentationTick{INT64_MAX};
    c.bindings = {binding};
    const playback::GameplayConfigurationDecodeBudget budget{65536, 32, 16384, 4096, 1024, true};
    auto encoded = playback::encodeGameplayConfiguration(c);
    REQUIRE(encoded);
    auto decoded = playback::decodeGameplayConfiguration(*encoded, budget);
    REQUIRE(decoded);
    CHECK(decoded->initialScore == INT64_MIN);
    CHECK(decoded->initialCombo == UINT64_MAX);
    CHECK(decoded->domains[0].minimum == -9007199254740993LL);
    CHECK(decoded->domains[0].maximum == 9007199254740993LL);
    CHECK(decoded->hooks[0].value == UINT64_MAX);
    CHECK(decoded->bindings[0].source.emissionPath[0].repeatIndex == UINT64_MAX);
    CHECK(decoded->bindings[0].start.value == INT64_MIN);
    REQUIRE(decoded->bindings[0].end);
    CHECK(decoded->bindings[0].end->value == INT64_MAX);
    auto reencoded = playback::encodeGameplayConfiguration(*decoded);
    REQUIRE(reencoded);
    CHECK(*reencoded == *encoded);
    auto value = json::parse(*encoded, {65536, 32, 16384});
    REQUIRE(value);
    const auto reject = [&](json::Value input) {
        auto text = json::serialize(input);
        REQUIRE(text);
        auto result = playback::decodeGameplayConfiguration(*text, budget);
        REQUIRE_FALSE(result);
        CHECK(result.error().code() == "playback.gameplay.invalid");
    };
    auto missing = *value;
    missing.find("configuration")->object()->erase("calibration");
    reject(std::move(missing));
    auto unknown = *value;
    unknown.find("configuration")->object()->emplace("hiddenDefault", json::Value{true});
    reject(std::move(unknown));
    auto floating = *value;
    *floating.find("configuration")->find("initialScore") = json::Value{1.0};
    reject(std::move(floating));
    auto overflowing = *value;
    *overflowing.find("configuration")->find("initialScore") = json::Value{UINT64_MAX};
    reject(std::move(overflowing));
    auto unknownEnum = *value;
    *unknownEnum.find("configuration")->find("arithmetic") = json::Value{"unchecked"};
    reject(std::move(unknownEnum));
    auto absentTestOnly = *value;
    *absentTestOnly.find("configuration")->find("testOnly") = json::Value{false};
    reject(std::move(absentTestOnly));
    auto shortIdentity = *value;
    shortIdentity.find("configuration")->find("engine")->array()->pop_back();
    reject(std::move(shortIdentity));
    auto duplicate = *encoded;
    duplicate.insert(1, "\"version\":1,");
    CHECK_FALSE(playback::decodeGameplayConfiguration(duplicate, budget));
    for (unsigned limit = 0; limit < 6; ++limit) {
        auto tiny = budget;
        if (limit == 0)
            tiny.maxBytes = 1;
        if (limit == 1)
            tiny.maxDepth = 1;
        if (limit == 2)
            tiny.maxStringBytes = 1;
        if (limit == 3)
            tiny.maxValues = 1;
        if (limit == 4)
            tiny.maxContainerElements = 1;
        if (limit == 5)
            tiny.testOnly = false;
        auto rejected = playback::decodeGameplayConfiguration(*encoded, tiny);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().code() == "capability.budget_insufficient");
        bool categoryFound = false;
        for (const auto& context : rejected.error().context())
            if (context.key == "category") {
                CHECK(context.value == "budget_exceeded");
                categoryFound = true;
            }
        CHECK(categoryFound);
    }
    c.programPolicy = static_cast<playback::GameplayProgramPolicy>(255);
    CHECK_FALSE(playback::encodeGameplayConfiguration(c));
    TestContent runtimeFixture;
    auto runtimeText = playback::encodeGameplayConfiguration(runtimeFixture.config);
    REQUIRE(runtimeText);
    auto runtimeConfig = playback::decodeGameplayConfiguration(*runtimeText, budget);
    REQUIRE(runtimeConfig);
    CHECK(playback::GameplayContent::fromPacked(runtimeFixture.packed(), *runtimeConfig));
}
TEST_CASE("Gameplay Entry metadata rejects incomplete closures before source publication",
          "[playback][gameplay][candidate]") {
    TestContent fixture;
    auto bytes = fixture.packed();
    auto content = fixture.content();
    const playback::GameplayConfigurationDecodeBudget budget{65536, 32, 16384, 4096, 1024, true};
    const playback::GameplayGraphDecodeBudget graphBudget{65536, 32, 16384, 4096, 1024, 512, 16384};
    auto provider = content::MemoryContentProvider::create({});
    REQUIRE(provider);
    auto metadata = playback::detail::gameplayEntryMetadata(content, fixture.config, "main.packed",
                                                            "packed-chart", bytes, {});
    REQUIRE(metadata);
    const auto consume = [&](const json::Value& v, std::vector<std::byte> payload) {
        auto text = json::serialize(v);
        REQUIRE(text);
        return playback::PlaybackSource::fromGameplayEntry(
            "entry-fixture", *text, std::move(payload), fixture.config, budget, graphBudget, {},
            *provider, playback::GameplayPrepareIntent::GameplayOnly);
    };
    auto source = consume(*metadata, bytes);
    REQUIRE(source);
    playback::PlaybackSession session;
    auto prepared = session.prepareLoad(std::move(*source), playback::PlaybackMode::ChartClock);
    REQUIRE(prepared);
    REQUIRE(session.commit(std::move(*prepared)));
    auto receipt = session.advanceGameplay({20}, {20}, playback::RuntimeFrame{});
    REQUIRE(receipt);
    auto result = session.queryGameplay();
    REQUIRE(result);
    auto score = result->score();
    REQUIRE(score);
    CHECK(score->misses == 1);
    CHECK(score->score == -1);
    for (const auto* field :
         {"entryKind", "compiledSemanticIdentity", "artifactIdentity", "rulesetBinding",
          "capabilityClosure", "resourcePresentationClosure", "sourceOf"}) {
        auto missing = *metadata;
        missing.object()->erase(field);
        CHECK_FALSE(consume(missing, bytes));
    }
    auto tampered = bytes;
    tampered.back() ^= std::byte{1};
    auto badHash = consume(*metadata, std::move(tampered));
    REQUIRE_FALSE(badHash);
    CHECK(badHash.error().message() == "Gameplay artifact hash mismatch");
    const auto rejectField = [&](const char* field, json::Value replacement) {
        auto changed = *metadata;
        *changed.find(field) = std::move(replacement);
        CHECK_FALSE(consume(changed, bytes));
    };
    rejectField("compiledSemanticIdentity", json::Value{std::string(64, '0')});
    rejectField("expandedEntityCount", json::Value{std::uint64_t{2}});
    rejectField("expandedRequirementCount", json::Value{1.0});
    rejectField("compilerProfile", json::Value{"fake-profile"});
    rejectField("entryKind", json::Value{"author-source"});
    rejectField("encoding", json::Value{"capsule.2"});
    rejectField("version", json::Value{std::uint64_t{2}});
    rejectField("path", json::Value{"../outside"});
    auto fakeCapability = *metadata;
    fakeCapability.find("capabilityClosure")->find("derived")->array()->clear();
    CHECK_FALSE(consume(fakeCapability, bytes));
    auto fakeResource = *metadata;
    fakeResource.find("resourcePresentationClosure")
        ->find("gameplayResources")
        ->array()
        ->emplace_back("fake");
    CHECK_FALSE(consume(fakeResource, bytes));
    auto packagedSource = *metadata;
    *packagedSource.find("sourceOf")->find("kind") = json::Value{"packaged"};
    CHECK_FALSE(consume(packagedSource, bytes));
    auto unknown = *metadata;
    unknown.object()->emplace("hiddenProof", json::Value{true});
    CHECK_FALSE(consume(unknown, bytes));
}
TEST_CASE(
    "Gameplay Entry CXC file memory and captured filesystem generation share complete results",
    "[candidate][gameplay][entry][cxc]") {
    const auto revision = GENERATE(3U, 4U);
    TestContent fixture;
    auto content = fixture.content(revision);
    auto packed = fixture.packed(revision);
    auto prepared = prepareResolvedGameplay(fixture.request);
    REQUIRE(prepared);
    auto graph =
        gameplay_packed::encodeGraph({fixture.chart,
                                      *prepared,
                                      fixture.owners,
                                      {"normalization.one", "coordinator.policy.greedy_v1"},
                                      {},
                                      {},
                                      3},
                                     {131072, 8192, 8192, true});
    REQUIRE(graph);
    const playback::GameplayConfigurationDecodeBudget metadataBudget{131072, 64,   8192,
                                                                     16384,  8192, true};
    const playback::GameplayGraphDecodeBudget graphBudget{131072, 64,   8192, 16384,
                                                          8192,   8192, 65536};
    const auto bytesOf = [](std::string_view text) {
        std::vector<std::byte> bytes(text.size());
        std::memcpy(bytes.data(), text.data(), text.size());
        return bytes;
    };
    const auto read = [&](std::string_view relative) {
        std::ifstream stream{std::filesystem::path{CUEXIS_SOURCE_DIR} /
                                 "tests/fixtures/chart_format_update/static_project" / relative,
                             std::ios::binary};
        REQUIRE(stream);
        return std::string{std::istreambuf_iterator<char>{stream}, {}};
    };
    auto provider = content::MemoryContentProvider::create({});
    REQUIRE(provider);
    for (const auto kind : {"packed-chart", "gameplay-graph"}) {
        auto bytes = std::string_view{kind} == "packed-chart" ? packed : bytesOf(*graph);
        const auto entryPath = std::string_view{kind} == "packed-chart"
                                   ? "compiled/main.packed"
                                   : "compiled/main.graph.json";
        auto metadata = playback::detail::gameplayEntryMetadata(content, fixture.config, entryPath,
                                                                kind, bytes, {});
        REQUIRE(metadata);
        auto project = json::parse(read("cuexis.project.json"), {65536, 32, 8192});
        REQUIRE(project);
        project->find("extensions")
            ->object()
            ->emplace("cuexis.gameplay-entry.v1",
                      json::Value{json::Value::Object{
                          {"entries", json::Value{json::Value::Array{*metadata}}}}});
        auto projectText = json::serialize(*project);
        REQUIRE(projectText);
        cxc::CxcWriteRequest request;
        request.entries = {
            {"cuexis.project.json", bytesOf(*projectText)},
            {"assets/cuexis.asset-index.json", bytesOf(read("assets/cuexis.asset-index.json"))},
            {"assets/charts/main.cuexis.chart.json",
             bytesOf(read("assets/charts/main.cuexis.chart.json"))},
            {entryPath, bytes}};
        auto package = cxc::CxcWriter::write(request);
        std::string diagnostics;
        for (const auto& diagnostic : package.diagnostics.items())
            diagnostics +=
                std::string{diagnostic.code()} + ": " + std::string{diagnostic.message()} + "\n";
        INFO(diagnostics);
        REQUIRE(package.hasValue());
        struct Directory {
            std::filesystem::path path;
            Directory() {
                const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
                for (unsigned i = 0; i < 4096; ++i) {
                    path = std::filesystem::temp_directory_path() /
                           ("cuexis-gameplay-entry-" + std::to_string(nonce) + "-" +
                            std::to_string(i));
                    if (std::filesystem::create_directory(path))
                        return;
                }
                throw std::runtime_error{"Cannot claim fixture directory"};
            }
            ~Directory() {
                std::error_code ignored;
                std::filesystem::remove_all(path, ignored);
            }
        } directory;
        const auto write = [](const std::filesystem::path& path,
                              std::span<const std::byte> payload) {
            std::filesystem::create_directories(path.parent_path());
            std::ofstream stream{path, std::ios::binary};
            REQUIRE(stream);
            stream.write(reinterpret_cast<const char*>(payload.data()),
                         static_cast<std::streamsize>(payload.size()));
            REQUIRE(stream.good());
        };
        write(directory.path / "package.cxc", *package.bytes);
        for (const auto& entry : request.entries)
            write(directory.path / "generation" / entry.path, entry.bytes);
        std::optional<playback::GameplayResult> expected;
        for (unsigned carrier = 0; carrier < 3; ++carrier) {
            auto source =
                carrier == 0 ? playback::PlaybackSource::fromCxcMemoryGameplayEntry(
                                   *package.bytes, entryPath, fixture.config, metadataBudget,
                                   graphBudget, playback::GameplayPrepareIntent::GameplayOnly)
                : carrier == 1
                    ? playback::PlaybackSource::fromCxcFileGameplayEntry(
                          directory.path / "package.cxc", entryPath, fixture.config, metadataBudget,
                          graphBudget, playback::GameplayPrepareIntent::GameplayOnly)
                    : playback::PlaybackSource::fromFilesystemGameplayEntry(
                          directory.path / "generation", entryPath, fixture.config, metadataBudget,
                          graphBudget, playback::GameplayPrepareIntent::GameplayOnly);
            INFO((source ? "" : std::string{source.error().message()}));
            REQUIRE(source);
            playback::PlaybackSession session;
            auto candidate =
                session.prepareLoad(std::move(*source), playback::PlaybackMode::ChartClock);
            REQUIRE(candidate);
            REQUIRE(session.commit(std::move(*candidate)));
            auto event = input();
            REQUIRE(session.submitGameplay(std::span{&event, 1}));
            REQUIRE(session.advanceGameplay({20}, {20}, {20, 0, 0}));
            auto result = session.queryGameplay();
            REQUIRE(result);
            CHECK(result->score()->score == 2);
            CHECK(result->score()->hits == 1);
            if (expected)
                CHECK(*expected->sameResult(*result));
            else
                expected = *result;
            auto archive = session.archiveGameplay();
            REQUIRE(archive);
            auto replay = session.evaluateGameplayReplay(*archive);
            REQUIRE(replay);
            CHECK(replay->evidenceValid);
            CHECK(*result->sameResult(replay->result));
        }
        auto tinyBudget = metadataBudget;
        tinyBudget.maxBytes = 1;
        auto budgetRejected = playback::PlaybackSource::fromCxcMemoryGameplayEntry(
            *package.bytes, entryPath, fixture.config, tinyBudget, graphBudget,
            playback::GameplayPrepareIntent::GameplayOnly);
        REQUIRE_FALSE(budgetRejected);
        CHECK(budgetRejected.error().code() == "capability.budget_insufficient");
        bool sourceFound = false;
        for (const auto& context : budgetRejected.error().context())
            if (context.key == "sourceCode") {
                CHECK(context.value == "json.parse.size_limit");
                sourceFound = true;
            }
        CHECK(sourceFound);
        CHECK_FALSE(playback::PlaybackSource::fromCxcMemoryGameplayEntry(
            *package.bytes, "wrong.packed", fixture.config, metadataBudget, graphBudget,
            playback::GameplayPrepareIntent::GameplayOnly));
        auto corrupt = *package.bytes;
        corrupt[corrupt.size() / 2] ^= std::byte{1};
        CHECK_FALSE(playback::PlaybackSource::fromCxcMemoryGameplayEntry(
            std::move(corrupt), entryPath, fixture.config, metadataBudget, graphBudget,
            playback::GameplayPrepareIntent::GameplayOnly));
    }
}

TEST_CASE("Candidate FactBinding groups preserve partial commits without changing judgement",
          "[candidate][gameplay][presentation][group]") {
    TestContent f;
    auto& doc = f.request.assembly.sources[0].document;
    doc.factBindings.push_back({"binding.two"});
    doc.requirements[0].factBindingRefs.push_back("binding.two");
    const auto& e = doc.requirements[0].identity;
    playback::GameplayRequirementRef ref{e.chartEntryId, e.invocationId,       e.moduleId,
                                         e.exportId,     e.requirementLocalId, {}};
    for (const auto& step : e.emissionPath)
        ref.emissionPath.push_back({step.nodeId, step.repeatIndex});
    playback::GameplayFactBinding one{"binding.one",
                                      ref,
                                      playback::GameplayPhase::Tap,
                                      playback::GameplayOutcome::Hit,
                                      playback::GameplayTimingClass::Any,
                                      "019b0000-0000-7abc-8def-000000000002",
                                      false,
                                      {10},
                                      playback::GameplayPresentationTick{30}};
    one.aggregation = playback::GameplayAggregation::GroupCommit;
    one.groupMembers = {"binding.two", "binding.one"};
    auto two = one;
    two.bindingId = "binding.two";
    two.target = "absent-presentation-target";
    f.config.bindings = {one, two};
    TestContent plain = f;
    plain.config.bindings.clear();
    playback::PlaybackSession grouped, control;
    f.load(grouped);
    plain.load(control);
    const auto event = input();
    for (auto* session : {&grouped, &control}) {
        REQUIRE(session->submitGameplay(std::span{&event, 1}));
        REQUIRE(session->advanceGameplay({20}, {10}, {10, 0, 0}));
    }
    CHECK(*grouped.queryGameplay()->sameResult(*control.queryGameplay()));
    CHECK_FALSE(grouped.extractFrame({640, 480})->objects[0].visible);
    auto sourceMap = grouped.queryGameplayPresentation();
    REQUIRE(sourceMap);
    REQUIRE(sourceMap->sources.size() == 1);
    CHECK_FALSE(sourceMap->sources[0].visible);
    CHECK(sourceMap->projectionScope != 0);
    const auto originalScope = sourceMap->projectionScope;
    auto diagnostics = grouped.lastOperationDiagnostics();
    REQUIRE(diagnostics);
    CHECK(std::ranges::any_of(diagnostics->items(),
                              [](const auto& d) { return d.code() == "partial-group"; }));
    CHECK(std::ranges::any_of(diagnostics->items(), [](const auto& d) {
        return d.code() == "presentation-target-missing";
    }));
    REQUIRE(grouped.advanceGameplay({20}, {20}, {20, 0, 0}));
    CHECK_FALSE(grouped.extractFrame({640, 480})->objects[0].visible);
    CHECK(*grouped.gameplayState() == playback::GameplayState::Running);
    REQUIRE(grouped.advanceGameplay({20}, {30}, {30, 0, 0}));
    CHECK(grouped.extractFrame({640, 480})->objects[0].visible);
    CHECK(grouped.queryGameplayPresentation()->sources.empty());
    auto archive = grouped.archiveGameplay();
    REQUIRE(archive);
    REQUIRE(grouped.seekGameplay(*archive, {20}, *archive->cutAt({20}), {}, {10}, {10, 0, 1}));
    CHECK_FALSE(grouped.extractFrame({640, 480})->objects[0].visible);
    CHECK(grouped.queryGameplayPresentation()->projectionScope != originalScope);
    CHECK(sourceMap->sources.size() == 1); // Owning old map remains valid.
    auto laterOne = one;
    laterOne.start = {20};
    laterOne.end = playback::GameplayPresentationTick{30};
    auto earlyMissing = two;
    earlyMissing.start = {0};
    earlyMissing.end = playback::GameplayPresentationTick{10};
    f.config.bindings = {laterOne, earlyMissing};
    playback::PlaybackSession later;
    f.load(later);
    REQUIRE(later.submitGameplay(std::span{&event, 1}));
    REQUIRE(later.advanceGameplay({20}, {0}, {0, 0, 0}));
    CHECK(later.extractFrame({640, 480})->objects[0].visible);
    REQUIRE(later.advanceGameplay({20}, {20}, {20, 0, 0}));
    CHECK_FALSE(later.extractFrame({640, 480})->objects[0].visible);
    f.config.bindings = {one, two};
    f.config.bindings[0].groupMembers.push_back("missing.binding");
    CHECK_FALSE(playback::GameplayContent::fromPacked(f.packed(), f.config));
    f.config.bindings = {one, two};
    for (auto& b : f.config.bindings)
        b.aggregation = playback::GameplayAggregation::All;
    f.config.bindings[1].outcome = playback::GameplayOutcome::Miss;
    auto alias = f.config.bindings[1];
    alias.aggregation = playback::GameplayAggregation::Any;
    alias.groupMembers.clear();
    alias.outcome = playback::GameplayOutcome::Hit;
    f.config.bindings.push_back(alias);
    CHECK_FALSE(playback::GameplayContent::fromPacked(f.packed(), f.config));
    std::reverse(f.config.bindings.begin(), f.config.bindings.end());
    CHECK_FALSE(playback::GameplayContent::fromPacked(f.packed(), f.config));
    std::reverse(f.config.bindings.begin(), f.config.bindings.end());
    f.config.bindings.pop_back();

    playback::PlaybackSession all;
    f.load(all);
    REQUIRE(all.submitGameplay(std::span{&event, 1}));
    REQUIRE(all.advanceGameplay({20}, {20}, {20, 0, 0}));
    CHECK(all.extractFrame({640, 480})->objects[0].visible);
    CHECK(*all.queryGameplay()->sameResult(*control.queryGameplay()));
}

TEST_CASE("Candidate capability four-state query is readonly and requires matching actual commit",
          "[candidate][gameplay][registry][query]") {
    TestContent f;
    playback::PlaybackSession session;
    const auto cap = "cuexis.gameplay.t4-k4.v1";
    auto unknown = session.queryGameplayCapability("unknown", "1", &f.config);
    REQUIRE(unknown);
    CHECK(unknown->state == playback::GameplayCapabilityState::Unknown);
    CHECK(unknown->diagnostic->code() == "capability.unknown");
    auto revision = session.queryGameplayCapability(cap, "2", &f.config);
    REQUIRE(revision);
    CHECK(revision->diagnostic->code() == "capability.revision_mismatch");
    CHECK(session.queryGameplayCapability(cap, "1")->state ==
          playback::GameplayCapabilityState::Disabled);
    auto pending = session.queryGameplayCapability(cap, "1", &f.config);
    REQUIRE(pending);
    CHECK(pending->state == playback::GameplayCapabilityState::Insufficient);
    CHECK(pending->diagnostic->code() == "capability.budget_insufficient");
    auto prepared = session.prepareGameplayLoad(presentation(), playback::PlaybackMode::ChartClock,
                                                f.content());
    REQUIRE(prepared);
    CHECK(session.queryGameplayCapability(cap, "1", &f.config)->state ==
          playback::GameplayCapabilityState::Insufficient);
    REQUIRE(session.commit(std::move(*prepared)));
    auto ready = session.queryGameplayCapability(cap, "1", &f.config);
    REQUIRE(ready);
    CHECK(ready->state == playback::GameplayCapabilityState::Available);
    CHECK_FALSE(ready->diagnostic);
    auto changed = f.config;
    changed.calibration = "another.calibration";
    CHECK(session.queryGameplayCapability(cap, "1", &changed)->state ==
          playback::GameplayCapabilityState::Insufficient);
    changed.enabledCapabilities.clear();
    CHECK(session.queryGameplayCapability(cap, "1", &changed)->state ==
          playback::GameplayCapabilityState::Disabled);
    auto reload = session.prepareGameplayReload(presentation(), f.content(),
                                                playback::ReloadPolicy::KeepChartTime);
    REQUIRE(reload);
    CHECK(session.queryGameplayCapability(cap, "1")->state ==
          playback::GameplayCapabilityState::Available);
    REQUIRE(session.commit(std::move(*reload))); // Readonly query did not invalidate generation.
    REQUIRE(session.controlGameplay(playback::GameplayControl::Pause));
    CHECK(session.queryGameplayCapability(cap, "1")->state ==
          playback::GameplayCapabilityState::Available);
    CHECK(*session.gameplayState() == playback::GameplayState::Paused);
}

TEST_CASE(
    "Candidate complete Hold Graph and Packed sources match independent scan and full recovery",
    "[candidate][gameplay][hold][oracle][entry]") {
    TestContent f;
    executionFields(f.request.assembly);
    auto& r = f.request.assembly.sources[0].document.requirements[0];
    r.measure.components = {{PhaseKind::head, "hold_head", {}, {}},
                            {PhaseKind::body, "hold_body", {}, {}},
                            {PhaseKind::tail, "hold_tail", {}, {}}};
    f.config.scoreRules.clear();
    for (auto [phase, weight] : std::vector<std::pair<playback::GameplayPhase, std::int64_t>>{
             {playback::GameplayPhase::Head, 2},
             {playback::GameplayPhase::Body, 3},
             {playback::GameplayPhase::Tail, 5}}) {
        f.config.scoreRules.push_back({phase, playback::GameplayOutcome::Hit, {}, weight, true});
        f.config.scoreRules.push_back({phase, playback::GameplayOutcome::Miss, {}, -weight, false});
    }
    const auto content = f.content();
    const auto* immutable = playback::detail::GameplayAccess::content(content);
    auto graph = gameplay_packed::encodeGraph(
        {immutable->capsule.chart, immutable->capsule.gameplay, immutable->capsule.owners,
         immutable->capsule.profiles, immutable->capsule.patterns, immutable->capsule.measures, 3},
        {131072, 8192, 8192, true});
    REQUIRE(graph);
    auto provider = cuexis::content::MemoryContentProvider::create({});
    REQUIRE(provider);
    playback::PlaybackSession packedSession, graphSession;
    auto packedSource = playback::PlaybackSource::fromGameplayPacked(
        "hold", "compiled/main.packed", f.packed(), f.config, {}, *provider,
        playback::GameplayPrepareIntent::GameplayOnly);
    REQUIRE(packedSource);
    std::vector<std::byte> graphBytes(graph->size());
    std::memcpy(graphBytes.data(), graph->data(), graph->size());
    auto graphSource = playback::PlaybackSource::fromGameplayGraph(
        "hold", "compiled/main.graph", std::move(graphBytes), f.config,
        {131072, 64, 8192, 16384, 8192, 8192, 65536}, {}, *provider,
        playback::GameplayPrepareIntent::GameplayOnly);
    REQUIRE(graphSource);
    for (auto pair :
         {std::pair{&packedSession, &*packedSource}, std::pair{&graphSession, &*graphSource}}) {
        auto prepared =
            pair.first->prepareLoad(std::move(*pair.second), playback::PlaybackMode::ChartClock);
        REQUIRE(prepared);
        REQUIRE(pair.first->commit(std::move(*prepared)));
    }
    auto press = input();
    press.observationTick = {900};
    auto release = press;
    release.observationTick = {1000};
    release.sequence = 2;
    release.action = playback::GameplayInputAction::Release;
    const std::array events{press, release};
    std::vector<AdmittedObservation> observations;
    for (const auto& event : events)
        observations.push_back(
            {{ObservationTick{Tick{event.observationTick.value}},
              "domain.binding.one",
              "keyboard",
              "lane.one",
              event.action == playback::GameplayInputAction::Press ? InputAction::press
                                                                   : InputAction::release,
              {}},
             Tick{event.observationTick.value},
             false,
             {},
             {}});
    auto oracle = scanReference(immutable->capsule.gameplay, immutable->configuration, observations,
                                Tick{1100});
    for (auto* session : {&packedSession, &graphSession}) {
        REQUIRE(session->submitGameplay(events));
        auto pending = session->snapshotGameplay();
        REQUIRE(pending);
        REQUIRE(session->advanceGameplay({900}, {7}, {0, 0, 0}));
        CHECK(*session->queryGameplay()->factCount() == 0);
        REQUIRE(session->advanceGameplay({1100}, {14}, {0, 0, 0}));
        auto result = session->queryGameplay();
        REQUIRE(result);
        const auto score = *result->score();
        CHECK(score.score == 10);
        CHECK(score.combo == 3);
        CHECK(score.hits == 3);
        CHECK(score.misses == 0);
        const auto& actual = playback::detail::GameplayAccess::result(*result)->kernel;
        CHECK(actual.facts == oracle.facts);
        CHECK(actual.phases == oracle.phases);
        CHECK(actual.resources == oracle.resources);
        CHECK(actual.contacts == oracle.contacts);
        CHECK(actual.ownership == oracle.ownership);
        CHECK(actual.observers == oracle.observers);
        CHECK(actual.receipts == oracle.receipts);
        auto archive = session->archiveGameplay();
        REQUIRE(archive);
        auto replay = session->evaluateGameplayReplay(*archive);
        REQUIRE(replay);
        CHECK(replay->evidenceValid);
        CHECK(*result->sameResult(replay->result));
        auto checkpoint = session->checkpointGameplay(*archive, {1100});
        REQUIRE(checkpoint);
        REQUIRE(session->seekGameplay(*archive, {1100}, *archive->cutAt({1100}),
                                      std::span{&*checkpoint, 1}, {14}, {0, 0, 1}));
        CHECK(*result->sameResult(*session->queryGameplay()));
        REQUIRE(session->restoreGameplay(*pending, {0}, {0, 0, 2}));
        REQUIRE(session->advanceGameplay({1100}, {14}, {0, 0, 2}));
        CHECK(*result->sameResult(*session->queryGameplay()));
    }
    CHECK(*packedSession.queryGameplay()->sameResult(*graphSession.queryGameplay()));
}

TEST_CASE("Candidate unknown capability first diagnostic is independent of declaration order",
          "[candidate][gameplay][diagnostic]") {
    TestContent f;
    const auto packed = f.packed();
    for (const auto& capabilities : std::vector<std::vector<std::string>>{
             {"unknown.z", "unknown.a"}, {"unknown.a", "unknown.z"}}) {
        f.config.enabledCapabilities = capabilities;
        const auto content = playback::GameplayContent::fromPacked(packed, f.config);
        REQUIRE_FALSE(content);
        CHECK(content.error().code() == "capability.unknown");
        const auto& context = content.error().context();
        const auto found = std::find_if(context.begin(), context.end(),
                                        [](const auto& row) { return row.key == "capabilityId"; });
        REQUIRE(found != context.end());
        CHECK(found->value == "unknown.a");
    }
}

TEST_CASE("Candidate explicit diagnostic projection preserves all fields and boundaries",
          "[candidate][gameplay][diagnostic][projection]") {
    const std::array<std::pair<std::string_view, std::string_view>, 10> categories{
        {{"unknown_capability", "capability.unknown"},
         {"capability_disabled", "capability.disabled"},
         {"budget_exceeded", "capability.budget_insufficient"},
         {"identity_closure_incomplete", "identity_closure_incomplete"},
         {"late_policy_incomplete", "judgement.late.parameters_invalid"},
         {"ambiguous_migration", "migration.ambiguous"},
         {"non_unique_solution", "playback.gameplay.non_unique_solution"},
         {"non_terminating_source", "playback.gameplay.non_terminating_source"},
         {"invalid_relation", "playback.gameplay.invalid"},
         {"unknown-category", "playback.gameplay.invalid"}}};
    for (const auto code : judgement::detail::categoryProjectedDiagnosticCodes) {
        for (const auto& [category, target] : categories) {
            auto original = core::Error{std::string{code}, "unchanged summary"}
                                .withContext("category", "ignored first")
                                .withContext("severity", "error")
                                .withContext("faulted", "false")
                                .withContext("category", std::string{category})
                                .withContext("sourceCode", "existing")
                                .withCause(core::Error{"upstream", "unchanged cause"});
            const auto projected = playback::detail::projectGameplayError(original);
            auto expected = core::Error{std::string{target}, "unchanged summary"};
            for (const auto& field : original.context())
                expected.withContext(field.key, field.value);
            expected.withContext("sourceCode", std::string{code});
            checkExactError(projected, expected);
            REQUIRE(original.cause()); // projection did not rewrite the owning original
            CHECK(original.code() == code);
            auto missing =
                playback::detail::projectGameplayError(core::Error{std::string{code}, "missing"});
            checkExactError(missing,
                            core::Error{"playback.gameplay.invalid", "missing"}.withContext(
                                "sourceCode", std::string{code}));
        }
    }
    for (const auto code :
         {"judgement.input.amount_narrowed", "judgement.input.amount_out_of_range",
          "judgement.input.same_tick_collision", "judgement.timebase.tick_overflow",
          "judgement.timebase.time_reversal", "judgement.kernel.transaction_failed",
          "judgement.session.operation_state_invalid", "judgement.session.legacy_entry_unsupported",
          "judgement.session.execution_state_unavailable", "unknown.business.reason",
          "judgement.s7a2.artificial.undefined"}) {
        auto original = core::Error{code, "same"}
                            .withContext("category", "invalid_relation")
                            .withCause(core::Error{"cause", "same"});
        checkExactError(playback::detail::projectGameplayError(original), original);
    }
    for (const auto code : {"graph.header.unsupported_revision", "graph.budget.exceeded",
                            "graph.closure.invalid", "graph.other"}) {
        auto original = core::Error{code, "same"}
                            .withContext("ordered", "first")
                            .withCause(core::Error{"cause", "same"});
        auto expected = original;
        expected
            .withContext("category", std::string_view{code} == "graph.header.unsupported_revision"
                                         ? "unknown_capability"
                                     : std::string_view{code} == "graph.budget.exceeded"
                                         ? "budget_exceeded"
                                     : std::string_view{code} == "graph.closure.invalid"
                                         ? "identity_closure_incomplete"
                                         : "invalid_relation")
            .withContext("severity", "error")
            .withContext("faulted", "false");
        checkExactError(playback::detail::projectGameplayError(original), expected);
    }
}
TEST_CASE("Candidate diagnostic failure matches full kernel Result and AdvanceReceipt",
          "[candidate][gameplay][diagnostic][fault][full-result]") {
    TestContent fixture;
    fixture.config.maximumScore = 1;
    playback::PlaybackSession session;
    fixture.load(session);
    const auto event = input();
    REQUIRE(session.submitGameplay(std::span{&event, 1}));
    const auto failed = session.advanceGameplay({20}, {20}, {20, 0, 0});
    REQUIRE_FALSE(failed);
    const auto result = session.queryGameplay();
    REQUIRE(result);
    const auto receipt = session.gameplayAdvanceReceipt();
    REQUIRE(receipt);
    REQUIRE(receipt->error);
    const auto* stored = playback::detail::GameplayAccess::result(*result);
    REQUIRE(stored);
    const auto& kernel = stored->kernel;
    REQUIRE(kernel.faultDiagnostic);
    checkExactError(failed.error(), *kernel.faultDiagnostic);
    checkExactError(*receipt->error, *kernel.faultDiagnostic);
    CHECK(receipt->faultStage == playback::GameplayFaultStage::Fold);
    CHECK(receipt->sealedFacts == kernel.sealedFactCursor);
    REQUIRE(kernel.fold);
    CHECK(receipt->publishableFacts == kernel.fold->factCursor);
    REQUIRE(kernel.failedTick);
    REQUIRE(receipt->failedTick);
    CHECK(receipt->failedTick->value == kernel.failedTick->value());
    const auto archive = session.archiveGameplay();
    REQUIRE(archive);
    const auto evaluation = session.evaluateGameplayReplay(*archive);
    REQUIRE(evaluation);
    CHECK(evaluation->evidenceValid);
    REQUIRE(result->sameResult(evaluation->result));
    CHECK(*result->sameResult(evaluation->result));
    checkExactError(
        *receipt->error,
        *playback::detail::GameplayAccess::result(evaluation->result)->kernel.faultDiagnostic);
}
