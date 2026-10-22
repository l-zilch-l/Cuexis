// Offline wire budget verification only. No Host, Snapshot, Seek, Replay or performance run.
#include "../../engine/chart/src/packed_gameplay_internal.hpp"
#include "../../engine/gameplay_packed/src/semantic_stream_internal.hpp"
#include "../rpa/execution_fixture.hpp"
#include "../support/environment.hpp"
#include <cuexis_internal/sha256.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>
namespace {
using namespace cuexis;
using namespace cuexis::playback;
namespace work = gameplay_packed::detail;
TEST_CASE("Revision4 original capacity fixture complete physical envelope",
          "[.][revision4][physical-envelope]") {
    for (const auto scale : {1U, 64U, 40000U})
        for (const std::string reuse : {"low", "high"}) {
            DYNAMIC_SECTION("wire only " << scale << " " << reuse) {
                auto fixture = std::make_unique<test_support::SessionTestContent>();
                const ClockMappingConfiguration mapping{{0, 1}, {0, 1}, {0, 1}, {1, 1}};
                test_support::rpa(*fixture, mapping);
                auto& document = fixture->request.assembly.sources[0].document;
                const auto prototype = document.requirements[0];
                const auto entityPrototype = fixture->chart.entities[0];
                document.requirements.clear();
                fixture->chart.entities.clear();
                fixture->owners.clear();
                std::erase_if(document.declarations, [](const auto& d) {
                    return d.kind == judgement::DeclarationKind::requirement;
                });
                document.relations[0].members.clear();
                fixture->request.assembly.contentProfileLimits.maxRequirements =
                    judgement::MeasuredParameter<std::uint64_t>::measured(scale);
                for (unsigned i = 0; i < scale; ++i) {
                    auto requirement = prototype;
                    requirement.stableId.declarationOrdinal = 100 + i;
                    requirement.identity.requirementLocalId = "capacity." + std::to_string(i);
                    requirement.resourceClaims[0].claimPolicy.competition =
                        judgement::ClaimPolicyDeclaration::CompetitionKey{-1, i};
                    if (reuse == "low")
                        requirement.timing->end =
                            judgement::Tick{requirement.timing->end.value() + i};
                    document.declarations.push_back(judgement::testing::testDeclaration(
                        judgement::DeclarationKind::requirement,
                        requirement.identity.requirementLocalId,
                        requirement.stableId.declarationOrdinal));
                    document.relations[0].members.push_back(requirement.stableId);
                    auto entity = entityPrototype;
                    std::ostringstream suffix;
                    suffix << "019b0000-0000-7abc-8def-" << std::hex << std::setw(12)
                           << std::setfill('0') << i + 2;
                    entity.identity =
                        chart::ExplicitEntityIdentity{chart::ChartObjectId{suffix.str()}};
                    fixture->owners.push_back({requirement.identity, entity.identity});
                    fixture->chart.entities.push_back(std::move(entity));
                    document.requirements.push_back(std::move(requirement));
                }

                const auto prepared = prepareResolvedGameplay(fixture->request);
                INFO((prepared ? "" : std::string(prepared.error().message())));
                REQUIRE(prepared);
                const gameplay_packed::EncodeRequest request{
                    fixture->chart,
                    *prepared,
                    fixture->owners,
                    {"normalization.one", "coordinator.policy.greedy_v1"},
                    {},
                    {},
                    4};
                work::HashObservation writer;
                const auto bytes = work::encodeBounded(request, {}, {}, &writer);
                INFO((bytes ? ""
                            : std::string(bytes.error().code()) + ": " +
                                  std::string(bytes.error().message())));
                REQUIRE(bytes);
                const auto stats = chart::packed::gameplay_detail::inspect(*bytes);
                REQUIRE(stats);
                std::size_t largest = 0;
                const auto word = [&](std::size_t at) {
                    chart::packed::ByteReader r{std::span{*bytes}.subspan(at, 4)};
                    const auto v = r.readU32();
                    REQUIRE(v);
                    return *v;
                };
                for (std::size_t i = 0; i < word(24); ++i)
                    largest = std::max(largest, std::size_t(word(96 + i * 32 + 16)));
                CHECK(stats->packedBytes <= 16777216);
                CHECK(stats->decodedBytes <= 16777216);
                CHECK(largest <= 16777216);
                CHECK(stats->entityCount == scale);
                CHECK(stats->requirementCount == scale);
                CHECK(stats->stringCount == scale + 48);
                CHECK(stats->referenceCount == 13);
                CHECK(writer.shaComputations == 1);
                CHECK(writer.scratchHighWater <= 8388608);
                CHECK(writer.dynamicLiveBytes == 0);
                gameplay_packed::DecodeContext context{
                    fixture->request.assembly.capabilityContext,
                    fixture->request.assembly.contentProfileLimits,
                    fixture->request.patternBudget,
                    fixture->request.assembly.identityDeclarations,
                    "coordinator.policy.greedy_v1",
                    4};
                work::HashObservation reader;
                const auto decoded = work::decodeBounded(*bytes, context, {}, {}, &reader);
                INFO((decoded ? ""
                              : std::string(decoded.error().code()) + ": " +
                                    std::string(decoded.error().message())));
                REQUIRE(decoded);
                CHECK(decoded->gameplay.assembled().prepared == prepared->assembled().prepared);
                CHECK(reader.shaComputations == 1);
                CHECK(reader.shaBytes == writer.shaBytes);
                CHECK(reader.scratchHighWater <= 8388608);
                CHECK(reader.dynamicLiveBytes == 0);
                if (scale == 40000)
                    CHECK(writer.shaBytes == 99588531);
                std::cout << "R4_PHYSICAL scale=" << scale << " reuse=" << reuse
                          << " file=" << stats->packedBytes << " decoded=" << stats->decodedBytes
                          << " section=" << largest << " strings=" << stats->stringCount
                          << " refs=" << stats->referenceCount << " B=" << writer.shaBytes
                          << " writer.scratch=" << writer.scratchHighWater
                          << " reader.scratch=" << reader.scratchHighWater
                          << " sha.writer=" << writer.shaComputations
                          << " sha.reader=" << reader.shaComputations << " identity="
                          << core::detail::sha256Hex(*decoded->verifiedSemanticDigest)
                          << " host=false formal=false\n";
                const auto directory =
                    test_support::environmentVariable("CUEXIS_R4_WIRE_EVIDENCE_DIRECTORY");
                if (!directory.empty()) {
                    const auto path = std::filesystem::path(directory) /
                                      (std::to_string(scale) + "-" + reuse + ".r4.packed");
                    REQUIRE_FALSE(std::filesystem::exists(path));
                    std::ofstream stream(path, std::ios::binary);
                    REQUIRE(stream);
                    stream.write(reinterpret_cast<const char*>(bytes->data()),
                                 static_cast<std::streamsize>(bytes->size()));
                    REQUIRE(stream.good());
                }
            }
        }
}
} // namespace
