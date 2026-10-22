#include "claim_key_internal.hpp"

#include <catch2/catch_test_macros.hpp>

#include <limits>

namespace {
using namespace cuexis::judgement;
namespace codec = cuexis::judgement::detail;
} // namespace

TEST_CASE("Claim key shared fields preserve fixed framing golden", "[judgement][claim-key]") {
    RequirementIdentity identity;
    identity.chartEntryId = "entry:one";
    identity.moduleId = "m";
    identity.exportId = "\xC3\xA9";
    identity.emissionPath = {{std::string{"node\0A", 6}, 256}, {"leaf/:", 0}};
    identity.requirementLocalId = "req";
    const auto explicitNamespace = std::string{"ns:\0", 4} + "\xE7\x95\x8C";
    CHECK(codec::structuralClaimKey(explicitNamespace, identity) ==
          "cxgp2:"
          "00070000006e733a00e7958c09000000656e7472793a6f6e6500000000010000006d02000000c3a902000000"
          "060000006e6f646500410001000000000000060000006c6561662f3a000000000000000003000000726571");
    const auto key = codec::structuralClaimKey(explicitNamespace, identity);
    CHECK(codec::structuralProjectionKeySize(87, key.size()) == key.size());
    CHECK(codec::structuralProjectionKeySize(87, key.size() - 1) == key.size());
    CHECK(codec::structuralProjectionKeySize(UINT64_MAX, 4096) == 4097);

    auto other = identity;
    other.chartEntryId = "bc";
    const auto first = codec::structuralClaimKey("a", other);
    other.chartEntryId = "c";
    CHECK(first != codec::structuralClaimKey("ab", other));
    CHECK_NOTHROW(codec::structuralClaimKey(std::string(1, static_cast<char>(0xff)), identity));
}

TEST_CASE("Structural key framing preserves adapter rejection policy", "[judgement][claim-key]") {
    struct CountRejected final {};
    struct Adapter final {
        std::size_t writes{};
        void write(std::span<const std::byte>) {
            ++writes;
        }
        auto checkedCount(std::size_t value) const -> std::uint32_t {
            if (value > UINT32_MAX)
                throw CountRejected{};
            return static_cast<std::uint32_t>(value);
        }
        void validateText(std::string_view) const noexcept {}
    } adapter;
    codec::StructuralKeyEncoder encoder{adapter};
    REQUIRE_THROWS_AS(encoder.count(static_cast<std::uint64_t>(UINT32_MAX) + 1), CountRejected);
    CHECK(adapter.writes == 0);
    encoder.count(256);
    CHECK(adapter.writes == 1);
}
