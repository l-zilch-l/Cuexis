#include <cuexis/assets/asset_id.hpp>
#include <cuexis/assets/resource_handle.hpp>

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

static_assert(!std::is_same_v<cuexis::assets::MeshHandle, cuexis::assets::MaterialHandle>);
static_assert(!std::is_convertible_v<cuexis::assets::MeshHandle, cuexis::assets::MaterialHandle>);

TEST_CASE("AssetId is an ordered Cuexis-owned value", "[assets]") {
    const cuexis::assets::AssetId first{"mesh.note"};
    const cuexis::assets::AssetId same{"mesh.note"};
    const cuexis::assets::AssetId later{"mesh.track"};

    CHECK(first == same);
    CHECK(first < later);
    CHECK_FALSE(first.empty());
    CHECK(cuexis::assets::AssetId{}.empty());
}

TEST_CASE("Typed resource handles require an index and non-zero generation", "[assets]") {
    const cuexis::assets::MeshHandle empty;
    const cuexis::assets::MeshHandle missingGeneration{.index = 3, .generation = 0};
    const cuexis::assets::MeshHandle valid{.index = 3, .generation = 2};
    const cuexis::assets::MeshHandle same{.index = 3, .generation = 2};

    CHECK_FALSE(empty.valid());
    CHECK_FALSE(missingGeneration.valid());
    CHECK(valid.valid());
    CHECK(valid == same);
}
