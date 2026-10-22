#include "hash_core_fixture.hpp"
#include "packed_fixture_support.hpp"
#include "packed_gameplay_internal.hpp"
#include <chrono>
#include <cuexis/chart/packed_chart_io.hpp>
#include <cuexis_internal/sha256.hpp>
#include <fstream>

namespace {
using namespace cuexis;
using namespace gameplay_packed;
using namespace gameplay_packed::testing;
namespace work = gameplay_packed::detail;
namespace gp = chart::packed::gameplay_detail;
namespace wire = chart::packed::test;
using Bytes = std::vector<std::byte>;
template <class T> auto take(core::Result<T> value) -> T {
    INFO((value ? ""
                : std::string(value.error().code()) + ": " + std::string(value.error().message())));
    REQUIRE(value);
    return std::move(*value);
}
auto request(const HashCoreFixture& f) -> EncodeRequest {
    const auto& d = *f.decoded;
    return {d.chart, d.gameplay, d.owners, d.profiles, d.patterns, d.measures, 4};
}
auto context(const HashCoreFixture& f) -> DecodeContext {
    auto c = f.source.context();
    c.candidateRevision = 4;
    return c;
}
auto payload(const Bytes& bytes, std::string_view code) -> std::span<const std::byte> {
    const auto s = wire::findSection(bytes, code);
    REQUIRE(s);
    return std::span{bytes}.subspan(s->offset, s->size);
}
void beforeSha(const work::HashObservation& observation) {
    CHECK(observation.shaComputations == 0);
    CHECK(observation.shaUpdates == 0);
    CHECK(observation.dynamicLiveBytes == 0);
}
} // namespace

TEST_CASE("Revision4 verified metadata identity gates reconstruction and shared prepare",
          "[capsule][revision4][metadata-order]") {
    HashCoreFixture f;
    const auto bytes = take(encode(request(f)));
    work::HashObservation baseline;
    const auto raw = take(work::decodeBounded(bytes, context(f), {}, {}, &baseline));
    REQUIRE(raw.verifiedSemanticDigest);
    const auto expected = core::detail::sha256Hex(*raw.verifiedSemanticDigest);
    const std::string wrong(64, '0');
    auto c = context(f);
    c.expectedCompiledSemanticIdentity = expected;
    work::HashObservation observed;
    const auto valid = take(work::decodeBounded(bytes, c, {}, {}, &observed));
    CHECK(valid.verifiedSemanticDigest == raw.verifiedSemanticDigest);
    CHECK(observed.shaComputations == 1);
    CHECK(observed.shaBytes == baseline.shaBytes);
    CHECK(observed.reconstructions == 1);
    CHECK(observed.sharedPrepareCalls == 1);
    CHECK(observed.scratchHighWater == baseline.scratchHighWater);
    CHECK(observed.allocationCount == baseline.allocationCount);
    CHECK(observed.allocationBytes == baseline.allocationBytes);
    c.capabilities.enabledCapabilityIds.clear();
    const auto prepareFailure = work::decodeBounded(bytes, c, {}, {}, &observed);
    REQUIRE_FALSE(prepareFailure);
    CHECK(prepareFailure.error().code() == "judgement.closure.capability_disabled");
    CHECK(observed.shaComputations == 1);
    CHECK(observed.reconstructions == 1);
    CHECK(observed.sharedPrepareCalls == 1);
    c.expectedCompiledSemanticIdentity = wrong;
    const auto metadataFailure = work::decodeBounded(bytes, c, {}, {}, &observed);
    REQUIRE_FALSE(metadataFailure);
    CHECK(metadataFailure.error().code() == "packed.metadata.mismatch");
    CHECK(std::any_of(metadataFailure.error().context().begin(),
                      metadataFailure.error().context().end(), [](const auto& value) {
                          return value.key == "fieldPath" &&
                                 value.value == "metadata.compiledSemanticIdentity";
                      }));
    CHECK(observed.shaComputations == 1);
    CHECK(observed.shaBytes == baseline.shaBytes);
    CHECK(observed.reconstructions == 0);
    CHECK(observed.sharedPrepareCalls == 0);
    CHECK(observed.dynamicLiveBytes == 0);
    auto badHeader = bytes;
    badHeader[32] ^= std::byte{1};
    wire::refreshHeaderCrc(badHeader);
    const auto headerFailure = work::decodeBounded(badHeader, c, {}, {}, &observed);
    REQUIRE_FALSE(headerFailure);
    CHECK(headerFailure.error().code() == "packed.identity.mismatch");
    CHECK(observed.shaComputations == 1);
    CHECK(observed.reconstructions == 0);
    CHECK(observed.sharedPrepareCalls == 0);
    work::HashOptions noBytes;
    noBytes.requested.maxSemanticPreimageBytes = 0;
    const auto budgetFailure = work::decodeBounded(bytes, c, {}, noBytes, &observed);
    REQUIRE_FALSE(budgetFailure);
    CHECK(budgetFailure.error().code() == "capability.budget_insufficient");
    beforeSha(observed);
    CHECK(observed.reconstructions == 0);
    CHECK(observed.sharedPrepareCalls == 0);
    for (const auto revision : {2U, 3U}) {
        HashCoreFixture legacy{revision};
        const auto& d = *legacy.decoded;
        const auto old = take(
            encode({d.chart, d.gameplay, d.owners, d.profiles, d.patterns, d.measures, revision}));
        auto oldContext = legacy.source.context();
        oldContext.candidateRevision = revision;
        oldContext.expectedCompiledSemanticIdentity = wrong;
        CHECK(decode(old, oldContext));
    }
}

TEST_CASE("Revision4 omits only physical projections and retains complete Gameplay2",
          "[capsule][revision4]") {
    for (auto count : {0U, 1U, 64U}) {
        DYNAMIC_SECTION("requirements " << count) {
            HashCoreFixture f{3, count, true};
            const auto old = f.legacyBytes();
            const auto expected = core::detail::sha256(old);
            work::HashObservation writer;
            const auto bytes = take(work::encodeBounded(request(f), {}, {}, &writer));
            CHECK(wire::readU32(bytes, 88) == 4);
            CHECK(writer.shaComputations == 1);
            CHECK(writer.shaBytes == old.size());
            CHECK(writer.dynamicLiveBytes == 0);
            CHECK(std::equal(expected.begin(), expected.end(), bytes.begin() + 32,
                             [](auto a, auto b) { return a == std::to_integer<std::uint8_t>(b); }));
            const auto stats = take(gp::inspect(bytes));
            const auto raw = take(gp::decodeStatic(bytes));
            for (const auto& [kind, key] : raw.references) {
                CHECK(kind != 11);
                CHECK(kind != 12);
                CHECK_FALSE((kind == 13 && key.starts_with("cxgp2:01")));
            }
            CHECK(stats.referenceCount == raw.references.size());
            work::HashObservation reader;
            const auto decoded = take(work::decodeBounded(bytes, context(f), {}, {}, &reader));
            CHECK(reader.shaComputations == 1);
            CHECK(reader.shaBytes == old.size());
            CHECK(reader.dynamicLiveBytes == 0);
            REQUIRE(decoded.verifiedSemanticDigest);
            CHECK(*decoded.verifiedSemanticDigest == expected);
            CHECK(judgement::semanticDiff(f.decoded->gameplay.assembled().graph,
                                          decoded.gameplay.assembled().graph)
                      .empty());
            CHECK(decoded.gameplay.assembled().prepared ==
                  f.decoded->gameplay.assembled().prepared);
            CHECK(decoded.gameplay.identityDeclarations() ==
                  f.decoded->gameplay.identityDeclarations());
            CHECK(decoded.patterns == f.decoded->patterns);
            CHECK(decoded.measures == f.decoded->measures);
            CHECK(take(encode({decoded.chart, decoded.gameplay, decoded.owners, decoded.profiles,
                               decoded.patterns, decoded.measures, 4})) == bytes);
            work::HashOptions exportOptions;
            exportOptions.exportCapacity = old.size();
            const auto exported = take(work::hashRequest(request(f), {}, exportOptions));
            CHECK(std::ranges::equal(exported.bytes(), old));
            CHECK(exported.cost.bytes == old.size());
            CHECK(exported.digest == expected);
            auto legacy = context(f);
            legacy.candidateRevision = 3;
            CHECK_FALSE(decode(bytes, legacy));
            CHECK_FALSE(chart::packed::decode(bytes));
        }
    }
}

TEST_CASE("Revision4 all physical budgets precede the bounded semantic operation",
          "[capsule][revision4][budget]") {
    HashCoreFixture f{3, 64, true};
    const auto bytes = take(encode(request(f)));
    const auto stats = take(gp::inspect(bytes));
    std::size_t largest = 0;
    for (std::size_t i = 0; i < wire::readU32(bytes, 24); ++i)
        largest = std::max(largest, std::size_t(wire::readU32(bytes, 96 + i * 32 + 16)));
    const chart::PackedChartLimits exact{
        bytes.size(),      stats.decodedBytes,    largest, stats.stringCount, stats.referenceCount,
        stats.entityCount, stats.requirementCount};
    REQUIRE(encode(request(f), exact));
    REQUIRE(decode(bytes, context(f), exact));
    using Field = std::size_t chart::PackedChartLimits::*;
    const std::array<Field, 7> fields{&chart::PackedChartLimits::maxPackedFileBytes,
                                      &chart::PackedChartLimits::maxPackedDecodedBytes,
                                      &chart::PackedChartLimits::maxPackedSectionBytes,
                                      &chart::PackedChartLimits::maxPackedStrings,
                                      &chart::PackedChartLimits::maxPackedReferences,
                                      &chart::PackedChartLimits::maxPackedEntities,
                                      &chart::PackedChartLimits::maxPackedRequirements};
    for (auto field : fields)
        for (bool zero : {false, true}) {
            auto limits = exact;
            limits.*field = zero ? 0 : limits.*field - 1;
            work::HashObservation w, r, x;
            auto rejectedWriter = work::encodeBounded(request(f), limits, {}, &w);
            auto rejectedReader = work::decodeBounded(bytes, context(f), limits, {}, &r);
            auto rejectedExport = work::hashRequest(request(f), limits, {}, &x);
            REQUIRE_FALSE(rejectedWriter);
            REQUIRE_FALSE(rejectedReader);
            REQUIRE_FALSE(rejectedExport);
            CHECK(rejectedWriter.error().code().starts_with("packed.budget."));
            CHECK(rejectedReader.error().code().starts_with("packed.budget."));
            beforeSha(w);
            beforeSha(r);
            beforeSha(x);
        }
    auto relaxed = exact;
    for (auto field : fields)
        relaxed.*field = SIZE_MAX;
    CHECK(take(encode(request(f), relaxed)) == bytes);
    CHECK(decode(bytes, context(f), relaxed));
}

TEST_CASE("Revision4 Writer Reader and bounded export enforce all four hash limits",
          "[capsule][revision4][budget]") {
    HashCoreFixture f;
    const auto bytes = take(encode(request(f)));
    for (unsigned dimension = 0; dimension < 4; ++dimension) {
        work::HashOptions options;
        auto& l = options.requested;
        auto* field = dimension == 0   ? &l.maxSemanticPreimageBytes
                      : dimension == 1 ? &l.maxExpandedReferenceVisits
                      : dimension == 2 ? &l.maxProjectionKeyBytes
                                       : &l.maxHashScratchBytes;
        *field = 0;
        work::HashObservation w, r, x;
        const auto ew = work::encodeBounded(request(f), {}, options, &w);
        const auto er = work::decodeBounded(bytes, context(f), {}, options, &r);
        const auto ex = work::hashRequest(request(f), {}, options, &x);
        REQUIRE_FALSE(ew);
        REQUIRE_FALSE(er);
        REQUIRE_FALSE(ex);
        CHECK(ew.error().code() == "capability.budget_insufficient");
        CHECK(er.error().code() == ew.error().code());
        CHECK(ex.error().code() == ew.error().code());
        beforeSha(w);
        beforeSha(r);
        beforeSha(x);
    }
    work::HashOptions widened;
    widened.requested = {UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX};
    CHECK(take(work::encodeBounded(request(f), {}, widened)) == bytes);
    CHECK(work::decodeBounded(bytes, context(f), {}, widened));
}

TEST_CASE("Revision4 rejects unused forbidden physical roles and mixed layouts before SHA",
          "[capsule][revision4][hostile]") {
    HashCoreFixture f;
    const auto bytes = take(encode(request(f)));
    for (auto kind : {11U, 12U, 0U, 14U, 255U}) {
        const auto section = wire::findSection(bytes, "REF0");
        REQUIRE(section);
        auto refs = wire::readReferences(payload(bytes, "REF0"));
        refs.push_back({static_cast<std::uint8_t>(kind), 0});
        auto bad = wire::replaceSection(bytes, section->index, wire::buildReferences(refs));
        wire::writeU32(bad, 84, static_cast<std::uint32_t>(refs.size()));
        wire::writeU32(bad, 96 + section->index * 32 + 20, static_cast<std::uint32_t>(refs.size()));
        wire::refreshHeaderCrc(bad);
        work::HashObservation o;
        const auto rejected = work::decodeBounded(bad, context(f), {}, {}, &o);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().code() == "packed.references.kind");
        beforeSha(o);
    }
    for (const auto* key : {"cxgp2:01aa", "cxgp2:09aa", "cxgp2:ffaa"}) {
        auto strings = wire::readStrings(payload(bytes, "STR0"));
        const auto at =
            std::lower_bound(strings.begin(), strings.end(), std::string(key)) - strings.begin();
        strings.insert(strings.begin() + at, key);
        auto bad = wire::replaceSection(bytes, wire::findSection(bytes, "STR0")->index,
                                        wire::buildStrings(strings));
        auto refs = wire::readReferences(payload(bad, "REF0"));
        for (auto& ref : refs)
            if (ref.stringIndex >= static_cast<unsigned>(at))
                ++ref.stringIndex;
        refs.push_back({13, static_cast<std::uint32_t>(at)});
        const auto section = wire::findSection(bad, "REF0");
        REQUIRE(section);
        bad = wire::replaceSection(bad, section->index, wire::buildReferences(refs));
        wire::writeU32(bad, 80, static_cast<std::uint32_t>(strings.size()));
        wire::writeU32(bad, 84, static_cast<std::uint32_t>(refs.size()));
        wire::writeU32(bad, 96 + section->index * 32 + 20, static_cast<std::uint32_t>(refs.size()));
        const auto str = wire::findSection(bad, "STR0");
        wire::writeU32(bad, 96 + str->index * 32 + 20, static_cast<std::uint32_t>(strings.size()));
        wire::refreshHeaderCrc(bad);
        work::HashObservation o;
        const auto rejected = work::decodeBounded(bad, context(f), {}, {}, &o);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().code() == "packed.references.kind");
        beforeSha(o);
    }
    const auto& d = *f.decoded;
    auto old = take(encode({d.chart, d.gameplay, d.owners, d.profiles, d.patterns, d.measures, 3}));
    wire::writeU32(old, 88, 4);
    wire::refreshHeaderCrc(old);
    CHECK_FALSE(decode(old, context(f)));
    auto unknown = bytes;
    wire::writeU32(unknown, 88, 5);
    wire::refreshHeaderCrc(unknown);
    auto c = context(f);
    c.candidateRevision = 5;
    CHECK_FALSE(decode(unknown, c));
}

TEST_CASE("Revision4 file and active publication remain intact on rejection",
          "[capsule][revision4][lifecycle]") {
    HashCoreFixture f;
    auto bytes = take(encode(request(f)));
    auto c = context(f);
    std::optional<PreparedCapsule> active = take(decode(bytes, c));
    const auto identity = active->gameplay.assembled().prepared;
    const auto digest = active->verifiedSemanticDigest;
    auto bad = bytes;
    bad[32] ^= std::byte{1};
    wire::refreshHeaderCrc(bad);
    CHECK_FALSE(decodeInto(active, bad, c));
    CHECK(active->gameplay.assembled().prepared == identity);
    CHECK(active->verifiedSemanticDigest == digest);
    const auto path =
        std::filesystem::temp_directory_path() /
        ("cuexis-r4-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".packed");
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code e;
            std::filesystem::remove(path, e);
        }
    } cleanup{path};
    REQUIRE(writeAtomic(request(f), path));
    const auto file = take(read(path, c));
    CHECK(file.verifiedSemanticDigest == digest);
    chart::PackedChartLimits limits;
    limits.maxPackedFileBytes = 0;
    CHECK_FALSE(writeAtomic(request(f), path, limits));
    std::ifstream in(path, std::ios::binary);
    const std::string actual{std::istreambuf_iterator<char>(in), {}};
    CHECK(std::as_bytes(std::span{actual.data(), actual.size()}).size() == bytes.size());
    CHECK(std::ranges::equal(std::as_bytes(std::span{actual.data(), actual.size()}), bytes));
    CHECK_FALSE(decodeInto(active, bytes, c, limits));
    CHECK(active->gameplay.assembled().prepared == identity);
}

TEST_CASE("Revision4 long derived Claim keys are refused before SHA at both codec entries",
          "[capsule][revision4][hostile]") {
    HashCoreFixture f;
    const auto bytes = take(encode(request(f)));
    const auto ns = f.graph.requirements[0].resourceClaims[0].claimPolicy.claimKeyToken;
    auto strings = wire::readStrings(payload(bytes, "STR0"));
    const auto it = std::find(strings.begin(), strings.end(), ns);
    REQUIRE(it != strings.end());
    *it += std::string(2100, 'x');
    REQUIRE(std::is_sorted(strings.begin(), strings.end()));
    const auto bad = wire::replaceSection(bytes, wire::findSection(bytes, "STR0")->index,
                                          wire::buildStrings(strings));
    work::HashObservation reader;
    auto refused = work::decodeBounded(bad, context(f), {}, {}, &reader);
    REQUIRE_FALSE(refused);
    CHECK(refused.error().code() == "capability.budget_insufficient");
    beforeSha(reader);
    auto& source = f.source;
    source.request.assembly.sources[0]
        .document.requirements[0]
        .resourceClaims[0]
        .claimPolicy.claimKeyToken += std::string(2100, 'x');
    const auto prepared = source.prepare();
    work::HashObservation writer;
    const auto rejected = work::encodeBounded(
        {source.chart, prepared, source.owners, source.profiles, {}, {}, 4}, {}, {}, &writer);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code() == "capability.budget_insufficient");
    beforeSha(writer);
}

TEST_CASE("Revision4 entry hash workspace cleans up every upstream allocation failure",
          "[capsule][revision4][oom]") {
    struct Failing final : std::pmr::memory_resource {
        std::size_t remaining, live{};
        explicit Failing(std::size_t n) : remaining(n) {}
        void* do_allocate(std::size_t n, std::size_t a) override {
            if (remaining-- == 0)
                throw std::bad_alloc{};
            auto* p = std::pmr::new_delete_resource()->allocate(n, a);
            live += n;
            return p;
        }
        void do_deallocate(void* p, std::size_t n, std::size_t a) override {
            std::pmr::new_delete_resource()->deallocate(p, n, a);
            live -= n;
        }
        bool do_is_equal(const std::pmr::memory_resource& x) const noexcept override {
            return this == &x;
        }
    };
    HashCoreFixture f;
    work::HashObservation baseline;
    const auto bytes = take(work::encodeBounded(request(f), {}, {}, &baseline));
    for (bool reader : {false, true})
        for (std::size_t i = 0; i < baseline.allocationCount; ++i) {
            Failing memory{i};
            work::HashOptions options;
            options.upstream = &memory;
            work::HashObservation o;
            if (reader) {
                const auto result = work::decodeBounded(bytes, context(f), {}, options, &o);
                REQUIRE_FALSE(result);
                CHECK(result.error().code() == "packed.requirements.invalid");
            } else {
                const auto result = work::encodeBounded(request(f), {}, options, &o);
                REQUIRE_FALSE(result);
                CHECK(result.error().code() == "packed.requirements.invalid");
            }
            CHECK(memory.live == 0);
            CHECK(o.dynamicLiveBytes == 0);
            CHECK(o.shaComputations <= 1);
        }
}
