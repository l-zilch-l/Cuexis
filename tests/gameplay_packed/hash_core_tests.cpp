#include "../pd/pd_allocation_probe.hpp"
#include "hash_core_fixture.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cuexis_internal/sha256.hpp>
#include <numeric>

namespace {
using namespace cuexis;
namespace work = gameplay_packed::detail;
using gameplay_packed::testing::HashCoreFixture;
// MSVC's default PMR resource need not route through the executable's replacement new.
// This upstream requests the identical capacities/alignment through an observable C++ allocator.
struct ObservableResource final : std::pmr::memory_resource {
    std::size_t remaining{SIZE_MAX};
    auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override {
        if (remaining == 0)
            throw std::bad_alloc{};
        if (remaining != SIZE_MAX)
            --remaining;
        return ::operator new(bytes, std::align_val_t{alignment});
    }
    void do_deallocate(void* pointer, std::size_t, std::size_t alignment) override {
        ::operator delete(pointer, std::align_val_t{alignment});
    }
    auto do_is_equal(const std::pmr::memory_resource& other) const noexcept -> bool override {
        return this == &other;
    }
};
ObservableResource observableResource;
struct AllocationWindow final {
    std::size_t base{pd_allocation::live.load()};
    AllocationWindow() {
        pd_allocation::count.store(0);
        pd_allocation::bytes.store(0);
        pd_allocation::peak.store(base);
        pd_allocation::tracking = true;
    }
    ~AllocationWindow() {
        pd_allocation::tracking = false;
    }
};
struct Observed final {
    core::Result<work::SemanticHashResult> result;
    work::HashObservation work;
    std::uint64_t allocations{}, bytes{}, heapPeak{}, retained{};
};
auto observe(const work::SemanticInput& input, work::HashOptions options = {}) -> Observed {
    work::HashObservation state;
    if (options.upstream == std::pmr::new_delete_resource())
        options.upstream = &observableResource;
    AllocationWindow window;
    auto result = work::calculateSemanticHash(input, options, &state);
    pd_allocation::tracking = false;
    return {std::move(result),
            state,
            pd_allocation::count.load(),
            pd_allocation::bytes.load(),
            pd_allocation::peak.load() - window.base,
            pd_allocation::live.load() - window.base};
}
auto budget(const core::Error& error) -> std::string_view {
    for (const auto& context : error.context())
        if (context.key == "budgetId")
            return context.value;
    return {};
}
void cleanSuccess(const Observed& value, std::size_t exportCapacity = 0) {
    REQUIRE(value.result);
    CHECK(value.work.budgetAccepted);
    CHECK(value.work.inlineObjectBytes < 8192);
    CHECK(value.work.shaComputations == 1);
    CHECK(value.work.shaBytes == value.result->cost.bytes);
    CHECK(value.work.dynamicLiveBytes == 0);
    CHECK(value.allocations == value.work.allocationCount);
    CHECK(value.bytes == value.work.allocationBytes);
    CHECK(value.heapPeak + 65536 == value.work.scratchHighWater);
    CHECK(value.retained == exportCapacity);
}
void dump(const char* label, const Observed& value) {
    const auto& s = value.work;
    const auto u = [](std::uint64_t v) { return static_cast<unsigned long long>(v); };
    std::printf("hash-observation %s ok=%d B=%llu V=%llu K=%llu scratch=%llu allocations=%llu "
                "bytes=%llu peak=%llu retained=%llu unique=%llu hits=%llu nodes.count=%llu "
                "nodes.hash=%llu sha=%llu sha.bytes=%llu export=%llu meter.allocations=%llu "
                "meter.bytes=%llu meter.live=%llu inline=%llu\n",
                label, static_cast<int>(static_cast<bool>(value.result)),
                u(value.result ? value.result->cost.bytes : 0),
                u(value.result ? value.result->cost.visits : 0),
                u(value.result ? value.result->cost.maxKey : 0), u(s.scratchHighWater),
                u(value.allocations), u(value.bytes), u(value.heapPeak), u(value.retained),
                u(s.uniqueRows), u(s.cacheHits), u(s.patternCountNodes), u(s.patternHashNodes),
                u(s.shaComputations), u(s.shaBytes), u(s.exportedBytes), u(s.allocationCount),
                u(s.allocationBytes), u(s.dynamicLiveBytes), u(s.inlineObjectBytes));
}
void cleanFailure(const Observed& value, std::string_view expected) {
    REQUIRE_FALSE(value.result);
    CHECK(budget(value.result.error()) == expected);
    CHECK(value.result.error().code() == "capability.budget_insufficient");
    for (const auto& context : value.result.error().context()) {
        if (context.key == "profile")
            CHECK(context.value == work::kHashWorkProfile);
        if (context.key == "phase")
            CHECK(context.value == "count");
        if (context.key == "exact")
            CHECK(context.value == "false");
        if (context.key == "faulted")
            CHECK(context.value == "false");
    }
    CHECK_FALSE(value.work.budgetAccepted);
    CHECK(value.work.shaComputations == 0);
    CHECK(value.work.shaUpdates == 0);
    CHECK(value.work.shaBytes == 0);
    CHECK(value.work.exportedBytes == 0);
    CHECK(value.work.dynamicLiveBytes == 0);
    // Bounded diagnostics use the fixed control allowance, not attacker-owned strings.
    CHECK(value.retained < 16384);
    CHECK(value.heapPeak + 8192 <= std::max<std::uint64_t>(65536, value.work.scratchHighWater));
    for (const auto& context : value.result.error().context())
        CHECK(context.value.size() < 128);
}
auto relaxed() -> work::HashOptions {
    work::HashOptions result;
    result.requested = {UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX};
    return result;
}
auto numbered(char prefix, std::size_t index) -> std::string {
    char text[16];
    std::snprintf(text, sizeof(text), "%c%06zu", prefix, index);
    return text;
}
// Construct canonical indexed test tables outside the operation. Linked owning copies in
// RequirementRecord are deliberately not used: definitions and Ix tables are authoritative.
void requirements(HashCoreFixture& f, std::size_t count, std::size_t bindings = 1) {
    const auto prototype = f.graph.requirements[0];
    auto declaration = *std::find_if(
        f.graph.mergedNamespace.declarations.begin(), f.graph.mergedNamespace.declarations.end(),
        [&](const auto& d) { return d.stableId == prototype.stableId; });
    std::erase_if(f.graph.mergedNamespace.declarations,
                  [](const auto& d) { return d.kind == judgement::DeclarationKind::requirement; });
    std::uint32_t ordinal = 0;
    for (const auto& d : f.graph.mergedNamespace.declarations)
        ordinal = std::max(ordinal, d.stableId.declarationOrdinal + 1);
    const auto claim = f.claims[0];
    f.graph.requirements.clear();
    f.graph.requirements.reserve(count);
    f.indices.clear();
    f.indices.reserve(count);
    f.claims.clear();
    f.claims.reserve(count);
    f.graph.factBindings.clear();
    for (std::size_t i = 0; i < bindings; ++i)
        f.graph.factBindings.push_back({numbered('b', i)});
    for (std::size_t i = 0; i < count; ++i) {
        auto row = prototype;
        row.identity.requirementLocalId = numbered('r', i);
        row.stableId.declarationOrdinal = ordinal + static_cast<std::uint32_t>(i);
        auto named = declaration;
        named.stableId = row.stableId;
        named.name = row.identity.requirementLocalId;
        f.graph.mergedNamespace.declarations.push_back(std::move(named));
        f.graph.requirements.push_back(std::move(row));
        work::RequirementIndices ix{0, 0, 0, 0, 0, {static_cast<std::uint32_t>(i)}, {}};
        ix.bindings.resize(bindings);
        std::iota(ix.bindings.begin(), ix.bindings.end(), 0U);
        f.indices.push_back(std::move(ix));
        auto c = claim;
        c.requirement = static_cast<std::uint32_t>(i);
        c.declaration.claimPolicy.competition->tieRank = i;
        f.claims.push_back(std::move(c));
    }
    std::sort(f.graph.mergedNamespace.declarations.begin(),
              f.graph.mergedNamespace.declarations.end(),
              [](const auto& a, const auto& b) { return a.stableId < b.stableId; });
}
void namedDefinition(HashCoreFixture& f, judgement::DeclarationKind kind, std::string name,
                     const judgement::RequiredRefs& required) {
    std::uint32_t ordinal = 0;
    for (const auto& d : f.graph.mergedNamespace.declarations)
        ordinal = std::max(ordinal, d.stableId.declarationOrdinal + 1);
    judgement::MergedDeclaration d;
    d.stableId = {f.graph.requirements[0].stableId.sourceDocumentId, ordinal};
    d.kind = kind;
    d.name = std::move(name);
    for (const auto& feature : required.features)
        d.requiredFeatureIds.push_back(feature.featureId);
    for (const auto& cap : required.capabilities)
        d.requiredCapabilityIds.push_back(cap.capabilityId);
    f.graph.mergedNamespace.declarations.push_back(std::move(d));
    std::sort(f.graph.mergedNamespace.declarations.begin(),
              f.graph.mergedNamespace.declarations.end(),
              [](const auto& a, const auto& b) { return a.stableId < b.stableId; });
}
void deepPattern(HashCoreFixture& f, std::size_t depth) {
    auto* node = &f.decoded->patterns[0].root;
    *node = judgement::PatternNodeDeclaration{};
    for (std::size_t i = 1; i < depth; ++i) {
        node->primitive = judgement::PatternPrimitive::choice;
        node->operands.emplace_back();
        node = &node->operands.back();
    }
    node->primitive = judgement::PatternPrimitive::atom;
    node->atomRef = "atom.one";
}

} // namespace

TEST_CASE("Bounded semantic operation matches legacy bytes with one reusable digest",
          "[capsule][hash-core]") {
    for (auto revision : {2U, 3U}) {
        DYNAMIC_SECTION("revision " << revision) {
            HashCoreFixture fixture{revision};
            const auto expected = fixture.legacyBytes();
            const auto identityBefore = fixture.graph.requirements[0].identity;
            const auto* rowsBefore = fixture.graph.requirements.data();
            work::HashOptions options;
            options.exportCapacity = expected.size();
            const auto observed = observe(fixture.input(), options);
            if (!observed.result)
                INFO(observed.result.error().message());
            cleanSuccess(observed, expected.size());
            REQUIRE(observed.result);
            CHECK(observed.result->cost.bytes == expected.size());
            CHECK(observed.result->cost.visits == 12);
            CHECK(std::ranges::equal(observed.result->bytes(), expected));
            CHECK(observed.result->digest == core::detail::sha256(expected));
            const auto headerDigest = observed.result->digest;
            const auto metadataDigest = observed.result->digest;
            CHECK(headerDigest == metadataDigest);
            CHECK(observed.work.shaComputations == 1);
            CHECK(fixture.graph.requirements.data() == rowsBefore);
            CHECK(fixture.graph.requirements[0].identity == identityBefore);
        }
    }
}

TEST_CASE("Hash allocation observer has independent positive controls", "[capsule][hash-core]") {
    std::uint64_t count{}, bytes{};
    {
        AllocationWindow window;
        std::vector<std::byte> plain;
        plain.reserve(128);
        auto* block = observableResource.allocate(256, 64);
        observableResource.deallocate(block, 256, 64);
        pd_allocation::tracking = false;
        count = pd_allocation::count.load();
        bytes = pd_allocation::bytes.load();
    }
    CHECK(count == 2);
    CHECK(bytes == 384);
}

TEST_CASE("Hash core four dimensions use exact literal and clamped limits",
          "[capsule][hash-core]") {
    HashCoreFixture f;
    const auto baseline = observe(f.input());
    cleanSuccess(baseline);
    dump("small", baseline);
    const auto c = baseline.result->cost;
    const work::HashWorkLimits exact{c.bytes, c.visits, c.maxKey, baseline.work.scratchHighWater};
    work::HashOptions options;
    options.requested = exact;
    cleanSuccess(observe(f.input(), options));
    for (unsigned dimension = 0; dimension < 4; ++dimension) {
        const std::array names{"maxSemanticPreimageBytes", "maxExpandedReferenceVisits",
                               "maxProjectionKeyBytes", "maxHashScratchBytes"};
        for (bool zero : {false, true}) {
            options.requested = exact;
            auto& l = options.requested;
            auto* field = dimension == 0   ? &l.maxSemanticPreimageBytes
                          : dimension == 1 ? &l.maxExpandedReferenceVisits
                          : dimension == 2 ? &l.maxProjectionKeyBytes
                                           : &l.maxHashScratchBytes;
            *field = zero ? 0 : *field - 1;
            const auto rejected = observe(f.input(), options);
            cleanFailure(rejected, names[dimension]);
            dump(names[dimension], rejected);
        }
    }
    const auto widened = observe(f.input(), relaxed());
    cleanSuccess(widened);
    CHECK(widened.result->cost.bytes == c.bytes);
    CHECK(widened.result->digest == baseline.result->digest);
    options = {};
    options.exportCapacity = c.bytes - 1;
    cleanFailure(observe(f.input(), options), "maxSemanticPreimageBytes");
    options.exportCapacity = 0;
    cleanFailure(observe(f.input(), options), "maxSemanticPreimageBytes");
    options.exportCapacity = c.bytes;
    cleanSuccess(observe(f.input(), options), static_cast<std::size_t>(c.bytes));
    options.exportCapacity = UINT64_MAX;
    cleanFailure(observe(f.input(), options), "maxHashScratchBytes");
}

TEST_CASE("Hash core counts shared Pattern definitions once before amplification refusal",
          "[capsule][hash-core]") {
    HashCoreFixture f;
    requirements(f, 1000);
    auto& root = f.decoded->patterns[0].root;
    root = judgement::PatternNodeDeclaration{};
    root.primitive = judgement::PatternPrimitive::choice;
    for (unsigned i = 0; i < 10000; ++i)
        root.operands.emplace_back(judgement::PatternPrimitive::atom,
                                   std::vector<judgement::PatternNodeDeclaration>{}, "atom.one");
    // One 10,001-node definition is referenced 1,000 times: do not expand 10 million nodes to
    // count.
    const auto rejected = observe(f.input(), relaxed());
    cleanFailure(rejected, "maxSemanticPreimageBytes");
    dump("shared-pattern-refused", rejected);
    CHECK(rejected.work.patternCountNodes == 10001);
    CHECK(rejected.work.patternHashNodes == 0);
    CHECK(rejected.work.cacheHits >= 1000);
}

TEST_CASE("Hash core dense short references retain every logical Ix on memo hits",
          "[capsule][hash-core]") {
    HashCoreFixture f;
    requirements(f, 999, 991);
    const auto boundary = observe(f.input(), relaxed());
    cleanSuccess(boundary);
    dump("visits-exact-million", boundary);
    REQUIRE(boundary.result->cost.visits == 1000000);
    CHECK(boundary.result->cost.bytes < work::kAcceptedHashWork.maxSemanticPreimageBytes);
    CHECK(boundary.work.uniqueRows < 5000);
    CHECK(boundary.work.cacheHits > 990000);
    CHECK(boundary.work.patternCountNodes == 1);
    f.graph.factBindings.push_back({numbered('b', 991)});
    f.indices.back().bindings.push_back(991);
    const auto rejected = observe(f.input(), relaxed());
    cleanFailure(rejected, "maxExpandedReferenceVisits");
    dump("visits-million-plus-one", rejected);
}

TEST_CASE("Hash core streams long Claim projections at the exact key ceiling",
          "[capsule][hash-core]") {
    HashCoreFixture f;
    const auto base = observe(f.input());
    cleanSuccess(base);
    auto& ns = f.claims[0].declaration.claimPolicy.claimKeyToken;
    ns.append(static_cast<std::size_t>((4096 - base.result->cost.maxKey) / 2), 'x');
    const auto boundary = observe(f.input(), relaxed());
    cleanSuccess(boundary);
    dump("key-exact-4096", boundary);
    CHECK(boundary.result->cost.maxKey == 4096);
    ns.push_back('x');
    const auto rejected = observe(f.input(), relaxed());
    cleanFailure(rejected, "maxProjectionKeyBytes");
    dump("key-4098-refused", rejected);
}

TEST_CASE("Hash core deep Patterns use charged pages instead of recursion",
          "[capsule][hash-core]") {
    HashCoreFixture f;
    deepPattern(f, 20000);
    // Long optional definition IDs also exercise the borrowed-ID path rather than a hidden copy.
    f.decoded->patterns[0].patternId = std::string(1000, 'p');
    namedDefinition(f, judgement::DeclarationKind::patternDefinition,
                    f.decoded->patterns[0].patternId, f.decoded->patterns[0].required);
    const auto accepted = observe(f.input());
    cleanSuccess(accepted);
    dump("deep-20000", accepted);
    CHECK(accepted.work.patternCountNodes == 20000);
    CHECK(accepted.work.patternHashNodes == 40000);
    CHECK(accepted.work.scratchHighWater < work::kAcceptedHashWork.maxHashScratchBytes);
    auto tight = relaxed();
    tight.requested.maxHashScratchBytes = accepted.work.scratchHighWater - 1;
    cleanFailure(observe(f.input(), tight), "maxHashScratchBytes");
    deepPattern(f, 530000);
    const auto rejected = observe(f.input(), relaxed());
    cleanFailure(rejected, "maxHashScratchBytes");
    dump("deep-530000-refused", rejected);
}

TEST_CASE("Hash core preimage reaches exact accepted bytes without an expanded vector",
          "[capsule][hash-core]") {
    HashCoreFixture f;
    requirements(f, 31);
    auto& measures = f.decoded->measures;
    auto unused = measures[0];
    unused.id = "unused.measure";
    unused.declaration.components[0].declaredGradeTokens = {"g"};
    unused.declaration.components[0].gradeTable.reset();
    measures[0].declaration.components[0].declaredGradeTokens = {"g"};
    measures[0].declaration.components[0].gradeTable.reset();
    measures.push_back(std::move(unused));
    namedDefinition(f, judgement::DeclarationKind::measureDefinition, *measures[1].id,
                    measures[1].declaration.required);
    const auto base = observe(f.input());
    cleanSuccess(base);
    const auto delta = work::kAcceptedHashWork.maxSemanticPreimageBytes - base.result->cost.bytes;
    auto& shared = measures[0].declaration.components[0].declaredGradeTokens[0];
    auto& tail = measures[1].declaration.components[0].declaredGradeTokens[0];
    shared.append(static_cast<std::size_t>(delta / 32), 'g');
    tail.append(static_cast<std::size_t>(delta % 32), 'g');
    CHECK(shared.size() < 8388608);
    const auto accepted = observe(f.input(), relaxed());
    cleanSuccess(accepted);
    dump("preimage-exact-128MiB", accepted);
    REQUIRE(accepted.result->cost.bytes == 134217728);
    CHECK(accepted.work.scratchHighWater < 1000000);
    CHECK(accepted.work.exportedBytes == 0);
    CHECK(accepted.bytes < shared.size());
    tail.push_back('g');
    const auto rejected = observe(f.input(), relaxed());
    cleanFailure(rejected, "maxSemanticPreimageBytes");
    dump("preimage-128MiB-plus-one", rejected);
}

TEST_CASE("Hash workspace unwinds every injected allocation failure without misreporting a budget",
          "[capsule][hash-core]") {
    HashCoreFixture f;
    deepPattern(f, 600);
    const auto seed = observe(f.input());
    cleanSuccess(seed);
    for (bool exportRequested : {false, true}) {
        work::HashOptions initial;
        if (exportRequested)
            initial.exportCapacity = seed.result->cost.bytes;
        const auto baseline = observe(f.input(), initial);
        cleanSuccess(baseline,
                     exportRequested ? static_cast<std::size_t>(seed.result->cost.bytes) : 0);
        for (std::uint64_t point = 0; point < baseline.work.allocationCount; ++point) {
            ObservableResource failing;
            failing.remaining = static_cast<std::size_t>(point);
            auto options = initial;
            options.upstream = &failing;
            work::HashObservation state;
            bool badAllocation = false;
            std::uint64_t retained = 0;
            {
                AllocationWindow window;
                try {
                    (void)work::calculateSemanticHash(f.input(), options, &state);
                } catch (const std::bad_alloc&) {
                    badAllocation = true;
                }
                pd_allocation::tracking = false;
                retained = pd_allocation::live.load() - window.base;
            }
            CHECK(badAllocation);
            CHECK(state.dynamicLiveBytes == 0);
            CHECK(retained == 0);
            CHECK(state.shaComputations <= 1);
        }
    }
}

TEST_CASE("Hash scratch meters both capacities during vector migration", "[capsule][hash-core]") {
    for (bool enough : {false, true}) {
        auto limits = work::kAcceptedHashWork;
        limits.maxHashScratchBytes = 65536 + 3072 - (enough ? 0 : 1);
        work::HashAllocationStats state;
        ObservableResource upstream;
        work::HashScratch scratch{limits, &upstream, &state};
        bool refused = false;
        {
            std::pmr::vector<std::byte> bytes{&scratch};
            bytes.resize(1024, std::byte{3});
            auto* previous = bytes.data();
            try {
                bytes.reserve(2048);
            } catch (const work::HashWorkFailure&) {
                refused = true;
            }
            CHECK(refused == !enough);
            CHECK(bytes.size() == 1024);
            CHECK(bytes.front() == std::byte{3});
            CHECK(bytes.back() == std::byte{3});
            if (enough) {
                CHECK(state.scratchHighWater == 65536 + 3072);
                CHECK(state.dynamicLiveBytes == 2048);
                CHECK(state.allocationCount == 2);
            } else {
                CHECK(bytes.data() == previous);
                CHECK(state.scratchHighWater == 65536 + 1024);
                CHECK(state.dynamicLiveBytes == 1024);
                CHECK(state.allocationCount == 1);
            }
        }
        CHECK(state.dynamicLiveBytes == 0);
        CHECK(scratch.liveBytes() == 65536);
    }
}

TEST_CASE("Hash core accounts large static prefix sorting and identity storage",
          "[capsule][hash-core]") {
    HashCoreFixture f;
    auto& chart = f.decoded->chart;
    const auto prototype = chart.entities[0];
    for (std::size_t i = chart.entities.size(); i < 8192; ++i) {
        auto entity = prototype;
        char uuid[40];
        std::snprintf(uuid, sizeof(uuid), "019b0000-0000-7abc-8def-%012zx", i + 2);
        entity.identity = chart::ExplicitEntityIdentity{chart::ChartObjectId{uuid}};
        entity.parent = chart.entities[0].identity;
        chart.entities.push_back(std::move(entity));
    }
    chart.entities.back().identity =
        chart::GeneratedEntityIdentity{chart.chartId,
                                       "long.binding.identity.borrowed",
                                       "long.module.identity.borrowed",
                                       "long.export.identity.borrowed",
                                       {{"long.loop.scope.borrowed", 255},
                                        {"long.repeat.scope.borrowed", 256},
                                        {"long.leaf.scope.borrowed", 0}}};
    f.entities.clear();
    for (const auto& entity : chart.entities)
        f.entities.push_back(entity.identity);
    const gameplay_packed::EncodeRequest request{
        chart, f.decoded->gameplay, f.decoded->owners, f.decoded->profiles, {}, {}, 3};
    // Validate this static-source enlargement through the unchanged legacy seven-budget path.
    const auto encoded = gameplay_packed::encode(request);
    REQUIRE(encoded);
    const auto expected = gameplay_packed::semanticPreimage(request);
    REQUIRE(expected);
    const auto actual = observe(f.input());
    cleanSuccess(actual);
    dump("static-prefix-8192", actual);
    CHECK(actual.result->cost.bytes == expected->size());
    CHECK(actual.result->digest == core::detail::sha256(*expected));
}
