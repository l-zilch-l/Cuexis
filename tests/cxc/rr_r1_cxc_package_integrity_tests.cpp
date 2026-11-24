// R1 package integrity: the ordinary CXC v1 entry keeps its original envelope, path and
// content-closure checks after candidate decoding moved to a separate internal archive.

#include "cxc_package_test_fixture.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace {

using cuexis::cxc::CxcPackageLoader;
namespace support = cuexis::cxc::test;

} // namespace

TEST_CASE("rr_r1_cxc_package_integrity", "[cxc][r1][isolation]") {
    const auto validBytes = support::writePackage(support::makeV1Request());
    const auto valid = CxcPackageLoader::loadMemory(validBytes);
    REQUIRE(valid.hasValue());
    REQUIRE(valid.package.has_value());
    CHECK(valid.package->entryBytes("cuexis.project.json").has_value());

    // A truncated archive no longer contains a complete central directory. The ordinary loader
    // must reject it with the pre-existing archive diagnostic and never publish a package.
    auto truncatedBytes = std::vector<std::byte>(
        validBytes.begin(),
        validBytes.begin() + static_cast<std::ptrdiff_t>(validBytes.size() / 2U));
    const auto truncated = CxcPackageLoader::loadMemory(truncatedBytes);
    CHECK_FALSE(truncated.hasValue());
    CHECK_FALSE(truncated.package.has_value());
    CHECK(support::hasDiagnostic(truncated.diagnostics, "cxc.archive.invalid"));

    // Drop a resource that the project Asset Index still references. Integrity validation must
    // keep reporting the missing entry instead of accepting the reduced closure.
    auto request = support::makeV1Request();
    std::erase_if(request.entries, [](const auto& entry) {
        return entry.path == "assets/textures/white.texture.bin";
    });
    const auto missingResource =
        CxcPackageLoader::loadMemory(support::writeUncheckedPackage(std::move(request.entries)));
    CHECK_FALSE(missingResource.hasValue());
    CHECK_FALSE(missingResource.package.has_value());
    CHECK(support::hasDiagnostic(missingResource.diagnostics, "cxc.entry.missing"));
}
