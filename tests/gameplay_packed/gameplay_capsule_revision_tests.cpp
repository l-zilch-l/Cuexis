#include "gameplay_capsule_test_fixture.hpp"

// Golden bytes, revision 3, graph writer and ownership index cases.

TEST_CASE("Capsule canonical byte and structural preimage goldens", "[capsule][golden]") {
    CapsuleFixture f;
    const auto prepared = f.prepare();
    const auto bytes = f.encode(prepared);
    const auto preimage = capsule::semanticPreimage({f.chart, prepared, f.owners, f.profiles});
    REQUIRE(preimage);
    CHECK(bytes.size() == 3264);
    CHECK(digestText(bytes) == "471e5cefc7cd2b1376163ad87cf1b1d7557f624252fe2cef707b7af1a9774e7c");
    CHECK(preimage->size() == 4528);
    CHECK(digestText(*preimage) ==
          "82765a403f7fe3c0b820aae1e3ca699793fe206223278d7a2c33606121b7c9ff");
    const auto digest = core::detail::sha256(*preimage);
    CHECK(std::equal(digest.begin(), digest.end(), bytes.begin() + 32,
                     [](auto l, auto r) { return l == std::to_integer<std::uint8_t>(r); }));
}

TEST_CASE("Capsule revision 3 preserves executable fields and mutually rejects older readers",
          "[capsule][execution][revision3]") {
    CapsuleFixture f;
    f.base.latePolicy = executionLate();
    executionFields(f.request.assembly);
    f.profiles = {f.request.assembly.normalizationProfileToken,
                  f.request.assembly.coordinatorPolicyToken};
    const auto prepared = f.prepare();
    capsule::EncodeRequest request{f.chart, prepared, f.owners, f.profiles, {}, {}, 3};
    const auto bytes = capsule::encode(request);
    INFO((bytes ? "" : std::string{bytes.error().message()}));
    REQUIRE(bytes);
    auto preimage = capsule::semanticPreimage(request);
    REQUIRE(preimage);
    CHECK(bytes->size() == 3370);
    CHECK(digestText(*bytes) == "ff1ff218925f2f8c684505651b9170c46ce2f386091fd3da34a8bdfe99e18bb4");
    CHECK(preimage->size() == 4793);
    CHECK(digestText(*preimage) ==
          "8c5c41467cc986974d038eb66d5836198918ffe245adf51d8e575f156bf7a8ca");
    CHECK(u32(*bytes, 88) == 3);
    auto context = f.context();
    CHECK_FALSE(capsule::decode(*bytes, context));
    context.candidateRevision = 3;
    const auto decoded = capsule::decode(*bytes, context);
    INFO((decoded ? "" : std::string{decoded.error().message()}));
    REQUIRE(decoded);
    CHECK(decoded->candidateRevision == 3);
    CHECK(semanticDiff(prepared.assembled().graph, decoded->gameplay.assembled().graph).empty());
    CHECK(decoded->gameplay.identityDeclarations() == prepared.identityDeclarations());
    CHECK(std::equal(decoded->gameplay.timers().begin(), decoded->gameplay.timers().end(),
                     prepared.timers().begin(), prepared.timers().end()));
    const auto encodedAgain =
        capsule::encode({decoded->chart, decoded->gameplay, decoded->owners, decoded->profiles,
                         decoded->patterns, decoded->measures, 3});
    REQUIRE(encodedAgain);
    CHECK(*encodedAgain == *bytes);
    CHECK_FALSE(capsule::encode({f.chart, prepared, f.owners, f.profiles}));
    CapsuleFixture legacy;
    CHECK_FALSE(capsule::decode(legacy.encode(legacy.prepare()), context));
    auto changed = f.request;
    changed.assembly.sources[0].document.requirements[0].atomBindings[0].channelToken = "lane.two";
    auto second = prepareResolvedGameplay(changed);
    REQUIRE(second);
    CHECK(second->assembled().chart != prepared.assembled().chart);
    auto changedBytes = capsule::encode({f.chart, *second, f.owners, f.profiles, {}, {}, 3});
    REQUIRE(changedBytes);
    CHECK(*changedBytes != *bytes);
}

TEST_CASE("Revision3 hostile bytes and file publication preserve the previous artifact",
          "[capsule][revision3][hostile][atomic]") {
    const auto revision = GENERATE(3U, 4U);
    CapsuleFixture f;
    f.base.latePolicy = executionLate();
    executionFields(f.request.assembly);
    f.profiles = {f.request.assembly.normalizationProfileToken,
                  f.request.assembly.coordinatorPolicyToken};
    const auto prepared = f.prepare();
    capsule::EncodeRequest request{f.chart, prepared, f.owners, f.profiles, {}, {}, revision};
    auto encoded = capsule::encode(request);
    REQUIRE(encoded);
    auto bytes = *encoded;
    auto context = f.context();
    context.candidateRevision = revision;
    std::optional<capsule::PreparedCapsule> active;
    REQUIRE(capsule::decodeInto(active, bytes, context));
    const auto identity = active->gameplay.assembled().prepared;
    SECTION("wrong revision") {
        setU32(bytes, 88, 2);
        refreshHeader(bytes);
    }
    SECTION("hash forgery") {
        bytes[32] ^= std::byte{1};
        refreshHeader(bytes);
    }
    SECTION("section CRC") {
        auto dir = directory(bytes, "GPR0");
        bytes[u32(bytes, dir + 8) + u32(bytes, dir + 12) - 1] ^= std::byte{1};
    }
    SECTION("invalid UTF8") {
        const std::string token = "gameplay.execution.t4-k4.v1";
        const auto pattern = std::as_bytes(std::span{token.data(), token.size()});
        auto found = std::search(bytes.begin(), bytes.end(), pattern.begin(), pattern.end());
        REQUIRE(found != bytes.end());
        *found = std::byte{0xff};
        refreshSection(bytes, directory(bytes, "STR0"));
    }
    SECTION("table schema revision high byte") {
        auto dir = directory(bytes, "GPD0");
        bytes[u32(bytes, dir + 8) + 1] = std::byte{0xff};
        refreshSection(bytes, dir);
    }
    SECTION("option tag") {
        auto dir = directory(bytes, "GPD0");
        bytes[u32(bytes, dir + 8) + 17] = std::byte{0xff};
        refreshSection(bytes, dir);
    }
    SECTION("unknown opcode or illegal child shape") {
        auto dir = directory(bytes, "GPD0");
        auto position = static_cast<std::size_t>(u32(bytes, dir + 8)) + 16;
        auto readV = [&]() {
            std::uint64_t result = 0;
            unsigned shift = 0;
            for (;;) {
                REQUIRE(position < bytes.size());
                const auto byte = std::to_integer<unsigned>(bytes[position++]);
                result |= static_cast<std::uint64_t>(byte & 127) << shift;
                if (!(byte & 128)) {
                    return result;
                }
                shift += 7;
                REQUIRE(shift < 64);
            }
        };
        REQUIRE(readV() == 1); // Pattern table count precedes its first row.
        const auto present = std::to_integer<unsigned>(bytes[position++]);
        REQUIRE(present <= 1);
        if (present) {
            (void)readV();
        }
        REQUIRE(bytes[position++] == std::byte{1});
        auto features = readV();
        for (std::uint64_t i = 0; i < features; ++i) {
            (void)readV();
        }
        auto capabilities = readV();
        for (std::uint64_t i = 0; i < capabilities; ++i) {
            (void)readV();
        }
        REQUIRE(readV() > 0);
        SECTION("opcode") {
            bytes[position] = std::byte{0xff};
        }
        SECTION("atom cannot own child or backedge") {
            REQUIRE(bytes[position] == std::byte{1});
            bytes[position + 1] = std::byte{1};
        }
        refreshSection(bytes, dir);
    }
    SECTION("resource capacity or gap cannot bypass prepare") {
        auto dir = directory(bytes, "GRC0");
        auto position = static_cast<std::size_t>(u32(bytes, dir + 8)) + 16;
        auto skipV = [&]() {
            while (std::to_integer<unsigned>(bytes[position++]) & 128) {
                REQUIRE(position < bytes.size());
            }
        };
        skipV(); // Resource table count.
        skipV(); // Resource REF0 index.
        const auto capacity = position;
        skipV();
        skipV();
        skipV();
        REQUIRE(position < bytes.size());
        ++position;
        const auto gap = position;
        SECTION("capacity") {
            REQUIRE(bytes[capacity] == std::byte{1});
            bytes[capacity] = std::byte{2};
        }
        SECTION("gap") {
            REQUIRE(bytes[gap] == std::byte{0});
            bytes[gap] = std::byte{2};
        }
        refreshSection(bytes, dir);
    }
    SECTION("truncated execution record") {
        bytes.pop_back();
    }
    SECTION("trailing bytes") {
        bytes.push_back(std::byte{0});
    }
    SECTION("unknown execution token") {
        const std::string token = "gameplay.execution.t4-k4.v1";
        const auto pattern = std::as_bytes(std::span{token.data(), token.size()});
        auto found = std::search(bytes.begin(), bytes.end(), pattern.begin(), pattern.end());
        REQUIRE(found != bytes.end());
        *found = std::byte{'x'};
        refreshSection(bytes, directory(bytes, "STR0"));
    }
    CHECK_FALSE(capsule::decodeInto(active, bytes, context));
    REQUIRE(active);
    CHECK(active->gameplay.assembled().prepared == identity);
    Workspace workspace;
    const auto target = workspace.path / "execution.cxp";
    REQUIRE(capsule::writeAtomic(request, target));
    CHECK(readBytes(target) == *encoded);
    auto file = capsule::read(target, context);
    REQUIRE(file);
    CHECK(file->gameplay.assembled().prepared == identity);
    auto malformed = f.request;
    malformed.assembly.sources[0].document.requirements[0].atomBindings.clear();
    PreparedGameplayPublication publication;
    REQUIRE(prepareResolvedInto(publication, f.request));
    CHECK_FALSE(prepareResolvedInto(publication, malformed));
    CHECK(publication.active()->assembled().prepared == identity);
    auto badChart = f.chart;
    badChart.features.clear();
    CHECK_FALSE(
        capsule::writeAtomic({badChart, prepared, f.owners, f.profiles, {}, {}, revision}, target));
    CHECK(readBytes(target) == *encoded);
    auto permuted = capsule::decode(rewrite(*encoded, true), context);
    REQUIRE(permuted);
    auto rewritten =
        capsule::encode({permuted->chart, permuted->gameplay, permuted->owners, permuted->profiles,
                         permuted->patterns, permuted->measures, revision});
    REQUIRE(rewritten);
    CHECK(*rewritten == *encoded);
}

TEST_CASE("Revision3 optional amount and independent competition survive all projections",
          "[capsule][revision3][optional]") {
    CapsuleFixture f;
    f.base.latePolicy = executionLate();
    executionFields(f.request.assembly);
    f.profiles = {f.request.assembly.normalizationProfileToken,
                  f.request.assembly.coordinatorPolicyToken};
    auto& doc = f.request.assembly.sources[0].document;
    auto& r = doc.requirements[0];
    r.phases = {{PhaseKind::tap, 1}};
    r.requiresReleaseTailSemantics = false;
    r.timing = RequirementRecord::Timing{Tick{940},
                                         {{Tick{850}, Tick{920}, {PhaseKind::tap, 1}}},
                                         {},
                                         {{PhaseKind::tap, Tick{900}}}};
    r.measure.components = {{PhaseKind::tap, "tap", {}}};
    r.atomBindings.resize(1);
    r.atomBindings[0].amountRange = AmountMatchRange{-4, 9};
    r.resourceClaims.clear();
    r.independentCompetition = {-1, 42};
    doc.relations.clear();
    const auto prepared = f.prepare();
    auto bytes = capsule::encode({f.chart, prepared, f.owners, f.profiles, {}, {}, 3});
    REQUIRE(bytes);
    auto context = f.context();
    context.candidateRevision = 3;
    auto decoded = capsule::decode(*bytes, context);
    REQUIRE(decoded);
    const auto& restored = decoded->gameplay.assembled().graph.requirements[0];
    CHECK(restored.atomBindings == r.atomBindings);
    CHECK(restored.independentCompetition == r.independentCompetition);
    CHECK(semanticDiff(prepared.assembled().graph, decoded->gameplay.assembled().graph).empty());
    auto again = capsule::encode({decoded->chart, decoded->gameplay, decoded->owners,
                                  decoded->profiles, decoded->patterns, decoded->measures, 3});
    REQUIRE(again);
    CHECK(*again == *bytes);
}

TEST_CASE("Compiled Graph Writer shares Capsule records with explicit candidate bounds",
          "[capsule][graph][writer][revision3]") {
    CapsuleFixture f;
    f.base.latePolicy = executionLate();
    executionFields(f.request.assembly);
    f.profiles = {f.request.assembly.normalizationProfileToken,
                  f.request.assembly.coordinatorPolicyToken};
    const auto prepared = f.prepare();
    capsule::EncodeRequest request{f.chart, prepared, f.owners, f.profiles, {}, {}, 3};
    const capsule::GraphWriterLimits bounds{131072, 8192, 8192, true};
    const auto graph = capsule::encodeGraph(request, bounds);
    INFO((graph ? "" : std::string{graph.error().message()}));
    REQUIRE(graph);
    CHECK(graph->starts_with(
        "{\"format\":\"cuexis.gameplay-graph\",\"graphFormatRevision\":1,\"capsuleRevision\":3,"));
    const auto repeated = capsule::encodeGraph(request, bounds);
    REQUIRE(repeated);
    CHECK(*repeated == *graph);
    const auto tree =
        json::parseBounded(*graph, {{bounds.maxBytes, 32, bounds.maxStringBytes}, 16384, 8192});
    REQUIRE(tree);
    REQUIRE(tree->find("semanticIdentity"));
    CHECK(*tree->find("semanticIdentity")->string() ==
          "8c5c41467cc986974d038eb66d5836198918ffe245adf51d8e575f156bf7a8ca");
    const auto& staticChart = *tree->find("staticChart");
    CHECK(*staticChart.find("chartId")->string() == f.chart.chartId.value);
    REQUIRE(staticChart.find("entities")->array()->size() == 2);
    const auto& visual = staticChart.find("entities")->array()->at(1);
    CHECK(*visual.find("parent")->find("objectId")->string() ==
          std::get<chart::ExplicitEntityIdentity>(f.chart.entities[0].identity).objectId.value);
    const auto& component = visual.find("components")->array()->at(1);
    CHECK(*component.find("alpha")->unsignedInteger() == 37);
    REQUIRE(tree->find("GPR0")->array()->size() == 1);
    const auto& row = *tree->find("GPR0")->array()->at(0).array();
    REQUIRE_FALSE(row.empty());
    CHECK(*row.front().unsignedInteger() == 0);
    CHECK(row.back().type() != json::ValueType::Number);
    CHECK(tree->find("GPD0")->object()->size() == 4);
    CHECK(tree->find("GRC0")->object()->size() == 5);
    auto exact = bounds;
    exact.maxBytes = graph->size();
    CHECK(capsule::encodeGraph(request, exact));
    --exact.maxBytes;
    auto rejected = capsule::encodeGraph(request, exact);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "graph.budget.exceeded");
    for (unsigned field = 0; field < 3; ++field) {
        auto invalid = bounds;
        if (field == 0)
            invalid.maxBytes = 0;
        if (field == 1)
            invalid.maxStringBytes = 0;
        if (field == 2)
            invalid.maxRowAtoms = 0;
        const auto result = capsule::encodeGraph(request, invalid);
        REQUIRE_FALSE(result);
        CHECK(result.error().code() == "capability.budget_insufficient");
    }
    auto disabled = bounds;
    disabled.testOnly = false;
    CHECK_FALSE(capsule::encodeGraph(request, disabled));
    auto tiny = bounds;
    tiny.maxRowAtoms = 1;
    rejected = capsule::encodeGraph(request, tiny);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "graph.budget.exceeded");
    tiny = bounds;
    tiny.maxStringBytes = 1;
    rejected = capsule::encodeGraph(request, tiny);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "graph.budget.exceeded");
    request.candidateRevision = 2;
    rejected = capsule::encodeGraph(request, bounds);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "graph.header.unsupported_revision");
}

TEST_CASE(
    "Compiled Graph Writer preserves float sign and string escaping outside Gameplay integers",
    "[capsule][graph][writer][revision3]") {
    CapsuleFixture f;
    f.base.latePolicy = executionLate();
    executionFields(f.request.assembly);
    f.profiles = {f.request.assembly.normalizationProfileToken,
                  f.request.assembly.coordinatorPolicyToken};
    f.chart.timing.offsetMs = -0.0;
    const std::string escapedId = "measure\"\\\n";
    f.request.assembly.sources[0].document.declarations.push_back(
        testDeclaration(DeclarationKind::measureDefinition, escapedId, 100));
    const std::vector measures{capsule::MeasureDefinition{
        escapedId, f.request.assembly.sources[0].document.requirements[0].measure}};
    const auto prepared = f.prepare();
    const auto graph = capsule::encodeGraph(
        {f.chart, prepared, f.owners, f.profiles, {}, measures, 3}, {131072, 8192, 8192, true});
    INFO((graph ? "" : std::string{graph.error().message()}));
    REQUIRE(graph);
    CHECK(graph->find("\"offsetMs\":-0.0") != std::string::npos);
    const auto tree = json::parseBounded(*graph, {{131072, 32, 8192}, 16384, 8192});
    REQUIRE(tree);
    const auto& staticChart = *tree->find("staticChart");
    const auto* offset = staticChart.find("timing")->find("offsetMs")->number();
    REQUIRE(offset);
    CHECK(std::signbit(*offset));
    bool found = false;
    for (const auto& row : *tree->find("GPD0")->find("measures")->array()) {
        for (const auto& atom : *row.array()) {
            if (const auto* token = atom.find("token");
                token && token->string() && *token->string() == escapedId)
                found = true;
        }
    }
    CHECK(found);
}

TEST_CASE("Compiled Graph typed SAX roundtrip and table permutations preserve full Capsule",
          "[capsule][graph][reader][revision3]") {
    CapsuleFixture f;
    f.base.latePolicy = executionLate();
    executionFields(f.request.assembly);
    f.profiles = {f.request.assembly.normalizationProfileToken,
                  f.request.assembly.coordinatorPolicyToken};
    const auto prepared = f.prepare();
    capsule::EncodeRequest request{f.chart, prepared, f.owners, f.profiles, {}, {}, 3};
    const auto graph = capsule::encodeGraph(request, {131072, 8192, 8192, true});
    REQUIRE(graph);
    auto context = f.context();
    context.candidateRevision = 3;
    const capsule::GraphReaderLimits bounds{131072, 64, 8192, 16384, 8192, 8192, 65536, true};
    const auto decoded = capsule::decodeGraph(*graph, context, bounds);
    INFO((decoded ? ""
                  : std::string{decoded.error().code()} + ": " +
                        std::string{decoded.error().message()}));
    REQUIRE(decoded);
    CHECK(semanticDiff(prepared.assembled().graph, decoded->gameplay.assembled().graph).empty());
    CHECK(decoded->gameplay.identityDeclarations() == prepared.identityDeclarations());
    CHECK(std::equal(decoded->gameplay.timers().begin(), decoded->gameplay.timers().end(),
                     prepared.timers().begin(), prepared.timers().end()));
    REQUIRE(decoded->owners.size() == f.owners.size());
    CHECK(decoded->owners[0].requirement == f.owners[0].requirement);
    CHECK(decoded->owners[0].entity == f.owners[0].entity);
    auto packed = capsule::encode(request);
    auto roundtrip = capsule::encode({decoded->chart, decoded->gameplay, decoded->owners,
                                      decoded->profiles, decoded->patterns, decoded->measures, 3});
    REQUIRE(packed);
    REQUIRE(roundtrip);
    CHECK(*roundtrip == *packed);
    CHECK(digestText(*roundtrip) ==
          "ff1ff218925f2f8c684505651b9170c46ce2f386091fd3da34a8bdfe99e18bb4");
    const auto tree = json::parseBounded(*graph, {{131072, 64, 8192}, 16384, 8192});
    REQUIRE(tree);
    std::string permuted = "{";
    for (const auto name : {"semanticIdentity", "capsuleRevision", "format", "graphFormatRevision",
                            "GRC0", "GPR0", "GPD0", "staticChart", "GPH0"}) {
        if (permuted.size() > 1)
            permuted += ",";
        const auto value = json::serialize(*tree->find(name));
        REQUIRE(value);
        permuted += '"';
        permuted += name;
        permuted += "\":";
        permuted += *value;
    }
    permuted += '}';
    const auto reordered = capsule::decodeGraph(permuted, context, bounds);
    INFO((reordered ? "" : std::string{reordered.error().message()}));
    REQUIRE(reordered);
    roundtrip = capsule::encode({reordered->chart, reordered->gameplay, reordered->owners,
                                 reordered->profiles, reordered->patterns, reordered->measures, 3});
    REQUIRE(roundtrip);
    CHECK(*roundtrip == *packed);
}

TEST_CASE("Compiled Graph SAX rejects header shape counts and identity without fallback",
          "[capsule][graph][reader][revision3][hostile]") {
    CapsuleFixture f;
    f.base.latePolicy = executionLate();
    executionFields(f.request.assembly);
    f.profiles = {f.request.assembly.normalizationProfileToken,
                  f.request.assembly.coordinatorPolicyToken};
    const auto prepared = f.prepare();
    const auto graph = capsule::encodeGraph({f.chart, prepared, f.owners, f.profiles, {}, {}, 3},
                                            {131072, 8192, 8192, true});
    REQUIRE(graph);
    auto context = f.context();
    context.candidateRevision = 3;
    const capsule::GraphReaderLimits bounds{131072, 64, 8192, 16384, 8192, 8192, 65536, true};
    for (const auto& [text, code] : std::vector<std::pair<std::string, std::string>>{
             {R"({"GPR0":[invalid)", "graph.structure.invalid"},
             {R"({"format":"wrong","payload":invalid)", "graph.header.unsupported_format"},
             {R"({"format":"cuexis.gameplay-graph","capsuleRevision":99,"payload":invalid)",
              "graph.header.unsupported_revision"},
             {R"({"format":"cuexis.gameplay-graph","format":invalid)",
              "graph.structure.duplicate_key"},
             {R"({"format":"cuexis.gameplay-graph","graphFormatRevision":1.0)",
              "graph.integer.out_of_range"},
             {R"({"format":"cuexis.gameplay-graph","graphFormatRevision":18446744073709551615)",
              "graph.integer.out_of_range"}}) {
        const auto rejected = capsule::decodeGraph(text, context, bounds);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().code() == code);
    }
    auto tree = json::parseBounded(*graph, {{131072, 64, 8192}, 16384, 8192});
    REQUIRE(tree);
    const auto encodeTree = [&](const json::Value& value) {
        std::string text = "{";
        for (const auto name :
             {"format", "graphFormatRevision", "capsuleRevision", "semanticIdentity", "staticChart",
              "GPH0", "GPR0", "GPD0", "GRC0"}) {
            if (text.size() > 1)
                text += ",";
            const auto field = json::serialize(*value.find(name));
            REQUIRE(field);
            text += '"';
            text += name;
            text += "\":";
            text += *field;
        }
        text += '}';
        return text;
    };
    auto sample = *tree;
    *sample.find("semanticIdentity") = json::Value{std::string(64, '0')};
    auto rejected = capsule::decodeGraph(encodeTree(sample), context, bounds);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "graph.identity.mismatch");
    sample = *tree;
    sample.find("GPR0")->array()->at(0).array()->push_back(json::Value{std::uint64_t{1}});
    rejected = capsule::decodeGraph(encodeTree(sample), context, bounds);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "graph.structure.invalid");
    sample = *tree;
    sample.find("GPR0")->array()->at(0).array()->pop_back();
    rejected = capsule::decodeGraph(encodeTree(sample), context, bounds);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "graph.structure.invalid");
    sample = *tree;
    auto& global = *sample.find("GPH0")->array();
    REQUIRE(global.back().string());
    global[global.size() - 12] = json::Value{std::uint64_t{2}};
    rejected = capsule::decodeGraph(encodeTree(sample), context, bounds);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "graph.closure.invalid");
    for (unsigned field = 0; field < 7; ++field) {
        auto tiny = bounds;
        if (field == 0)
            tiny.maxBytes = graph->size() - 1;
        if (field == 1)
            tiny.maxDepth = 1;
        if (field == 2)
            tiny.maxStringBytes = 1;
        if (field == 3)
            tiny.maxValues = 1;
        if (field == 4)
            tiny.maxContainerElements = 1;
        if (field == 5)
            tiny.maxRowAtoms = 1;
        if (field == 6)
            tiny.maxRowDecodedStringBytes = 1;
        rejected = capsule::decodeGraph(*graph, context, tiny);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().code() == "graph.budget.exceeded");
    }
    auto production = bounds;
    production.testOnly = false;
    rejected = capsule::decodeGraph(*graph, context, production);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "capability.budget_insufficient");
}

TEST_CASE("Bounded Gameplay2 static sink preserves complete legacy prefix bytes",
          "[gameplay_packed][capsule4][identity]") {
    CapsuleFixture f;
    SECTION("explicit UUID identities") {}
    SECTION("generated paths retain little endian identity order") {
        for (unsigned i = 0; i < 2; ++i)
            f.chart.entities[i].identity = chart::GeneratedEntityIdentity{
                f.chart.chartId, "binding", "module", "export", {{"loop", 255 + i}, {"leaf", 0}}};
        f.chart.entities[1].parent = f.chart.entities[0].identity;
        f.owners[0].entity = f.chart.entities[0].identity;
    }
    std::vector<chart::CanonicalEntityIdentity> owners{f.owners[0].entity};
    const auto expected = packed::gameplay_detail::staticPreimage(f.chart, owners, 3);
    REQUIRE(expected);
    capsule::detail::HashScratch scratch{{}};
    std::pmr::vector<std::byte> actual{&scratch};
    actual.reserve(expected->size());
    const auto append = [](void* state, std::span<const std::byte> bytes) {
        auto& output = *static_cast<std::pmr::vector<std::byte>*>(state);
        output.insert(output.end(), bytes.begin(), bytes.end());
    };
    const auto streamed =
        packed::gameplay_detail::streamStaticPreimage(f.chart, owners, {&actual, append, &scratch});
    REQUIRE(streamed);
    CHECK(std::equal(expected->begin(), expected->end(), actual.begin(), actual.end()));
    CHECK(scratch.highWater() <= capsule::detail::kAcceptedHashWork.maxHashScratchBytes);
}

TEST_CASE("Capsule ownership index preserves coverage rejection and canonical owner order",
          "[capsule][ownership][indexed]") {
    CapsuleFixture fixture;
    auto& document = fixture.request.assembly.sources[0].document;
    auto second = document.requirements[0];
    second.stableId.declarationOrdinal = 5;
    second.identity.requirementLocalId = "requirement.two";
    second.identity.emissionPath[0].repeatIndex = 1;
    second.resourceClaims[0].claimPolicy.competition =
        ClaimPolicyDeclaration::CompetitionKey{-1, 3};
    document.declarations.push_back(
        testDeclaration(DeclarationKind::requirement, "requirement.two", 5));
    document.relations[0].members.push_back(second.stableId);
    fixture.owners.push_back({second.identity, fixture.chart.entities[0].identity});
    document.requirements.push_back(second);
    const auto prepared = fixture.prepare();
    const auto original = fixture.encode(prepared);
    auto owners = fixture.owners;
    SECTION("owner order does not affect bytes or roundtrip") {
        std::reverse(owners.begin(), owners.end());
        const auto encoded = capsule::encode({fixture.chart, prepared, owners, fixture.profiles});
        REQUIRE(encoded);
        CHECK(*encoded == original);
        const auto decoded = capsule::decode(*encoded, fixture.context());
        REQUIRE(decoded);
        CHECK(
            semanticDiff(prepared.assembled().graph, decoded->gameplay.assembled().graph).empty());
    }
    SECTION("duplicate ownership is still rejected") {
        owners[1] = owners[0];
        const auto encoded = capsule::encode({fixture.chart, prepared, owners, fixture.profiles});
        REQUIRE_FALSE(encoded);
        CHECK(encoded.error().code() == "packed.requirements.invalid");
        CHECK(encoded.error().message() == "duplicate ownership");
    }
    SECTION("unknown ownership is still rejected") {
        owners[0].requirement.requirementLocalId = "not.present";
        const auto encoded = capsule::encode({fixture.chart, prepared, owners, fixture.profiles});
        REQUIRE_FALSE(encoded);
        CHECK(encoded.error().message() == "ownership names no Requirement");
    }
    SECTION("missing ownership is still rejected") {
        owners.pop_back();
        const auto encoded = capsule::encode({fixture.chart, prepared, owners, fixture.profiles});
        REQUIRE_FALSE(encoded);
        CHECK(encoded.error().message() ==
              "explicit ownership must cover every Requirement exactly once");
    }
    CHECK(fixture.encode(prepared) == original);
}
