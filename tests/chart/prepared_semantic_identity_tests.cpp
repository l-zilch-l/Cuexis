#include <cuexis/chart/prepared_semantic_identity.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace {

[[nodiscard]] auto filledIdentity(std::uint8_t value) -> cuexis::chart::CanonicalContentIdentity {
    cuexis::chart::CanonicalContentIdentity identity{};
    identity.sha256.fill(value);
    return identity;
}

[[nodiscard]] auto sequentialIdentity() -> cuexis::chart::CanonicalContentIdentity {
    cuexis::chart::CanonicalContentIdentity identity{};
    for (std::size_t index = 0; index < identity.sha256.size(); ++index) {
        identity.sha256[index] = static_cast<std::uint8_t>(index);
    }
    return identity;
}

[[nodiscard]] auto bytes(std::string_view text) -> std::vector<std::byte> {
    std::vector<std::byte> result;
    result.reserve(text.size());
    for (const unsigned char value : text) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

} // namespace

TEST_CASE("Prepared semantic identity matches the frozen combiner encoding",
          "[chart][identity][cfu-e3]") {
    CHECK(cuexis::chart::emptyParameterIdentity().hex() ==
          "4b0e4518a21071fdce6ae84f6cafa7a17ddebb0af3fb6859e3c0e8e98a41251e");
    CHECK(cuexis::chart::audioContentIdentity(bytes("RIFF")).hex() ==
          "f25a3eb3603a5497ef4ebaeb5194aad73d5f564731f45f61125d4d64766ba7c9");
    CHECK(cuexis::chart::canonicalBytesIdentity("cuexis").hex() ==
          cuexis::chart::canonicalBytesIdentity(std::string_view{"cuexis"}).hex());

    const auto chart = sequentialIdentity();
    const std::array cxt{
        cuexis::chart::CxtIdentityComponent{"omega.import", filledIdentity(0x11U)},
        cuexis::chart::CxtIdentityComponent{"beta.import", filledIdentity(0x22U)},
    };
    const std::array resources{
        cuexis::chart::PreparedResourceIdentityComponent{
            .assetId = cuexis::chart::AssetId{"mesh.note"},
            .identity = filledIdentity(0x33U),
        },
        cuexis::chart::PreparedResourceIdentityComponent{
            .assetId = cuexis::chart::AssetId{"audio.main"},
            .identity = filledIdentity(0x44U),
        },
    };
    const auto assembled = cuexis::chart::assemblePreparedSemanticIdentity(chart, cxt, resources,
                                                                           filledIdentity(0x55U));
    CHECK(assembled.hex() == "8e7327de39106bd3ebc7b05cf2c9b17eb5f352338954054a79a905cf6723d3f8");

    const std::array reversedCxt{cxt[1], cxt[0]};
    const std::array reversedResources{resources[1], resources[0]};
    CHECK(cuexis::chart::assemblePreparedSemanticIdentity(chart, reversedCxt, reversedResources,
                                                          filledIdentity(0x55U)) == assembled);

    const auto emptyLists = cuexis::chart::assemblePreparedSemanticIdentity(
        chart, {}, {}, cuexis::chart::emptyParameterIdentity());
    CHECK(emptyLists != assembled);
    CHECK(emptyLists.hex().size() == 64U);
}