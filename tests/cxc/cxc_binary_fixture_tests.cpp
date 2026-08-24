#include "cxc_test_support.hpp"

#include <cuexis/cxc/cxc_package.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using cuexis::cxc::CxcPackageLoader;
namespace support = cuexis::cxc::test;

struct BinaryFixture final {
    std::string name;
    std::vector<std::byte> bytes;
    std::string expectedDiagnostic;
};

[[nodiscard]] auto shouldUpdateFixtures() -> bool {
#if defined(_MSC_VER)
    char* value = nullptr;
    std::size_t length = 0;
    const auto status = _dupenv_s(&value, &length, "CUEXIS_UPDATE_CXC_FIXTURES");
    const bool enabled = status == 0 && value != nullptr && std::string_view{value} == "1";
    std::free(value);
    return enabled;
#else
    const auto* value = std::getenv("CUEXIS_UPDATE_CXC_FIXTURES");
    return value != nullptr && std::string_view{value} == "1";
#endif
}

[[nodiscard]] auto canonicalPackage() -> std::vector<std::byte> {
    return support::writePackage(support::makeV4CxtRequest());
}

[[nodiscard]] auto canonicalStaticPackage() -> std::vector<std::byte> {
    return support::writePackage(support::makeV4StaticRequest());
}

[[nodiscard]] auto noncanonicalMetadata(std::span<const std::byte> canonical)
    -> std::vector<std::byte> {
    auto bytes = std::vector<std::byte>{canonical.begin(), canonical.end()};
    const auto layout = support::parseZip(bytes);
    const auto& entry = layout.entries[1];
    support::writeU16(bytes, entry.localHeaderOffset + 4U, 20U);
    support::writeU16(bytes, entry.localHeaderOffset + 10U, 0x1234U);
    support::writeU16(bytes, entry.localHeaderOffset + 12U, 0x4A21U);
    support::writeU16(bytes, entry.centralHeaderOffset + 4U, 0x0014U);
    support::writeU16(bytes, entry.centralHeaderOffset + 6U, 20U);
    support::writeU16(bytes, entry.centralHeaderOffset + 12U, 0x1234U);
    support::writeU16(bytes, entry.centralHeaderOffset + 14U, 0x4A21U);
    return bytes;
}

[[nodiscard]] auto generatedFixtures(std::span<const std::byte> canonical)
    -> std::vector<BinaryFixture> {
    const auto layout = support::parseZip(canonical);
    const auto& first = layout.entries.front();
    std::vector<BinaryFixture> fixtures;

    {
        auto bytes = std::vector<std::byte>{canonical.begin(), canonical.end()};
        support::writeU16(bytes, layout.eocdOffset + 8U, 0xFFFFU);
        support::writeU16(bytes, layout.eocdOffset + 10U, 0xFFFFU);
        fixtures.push_back(
            {"invalid_zip64.cxc", std::move(bytes), "cxc.archive.feature_unsupported"});
    }
    {
        auto bytes = std::vector<std::byte>{canonical.begin(), canonical.end()};
        std::vector<std::byte> record(56U);
        support::writeU32(record, 0U, 0x06064B50U);
        bytes.insert(bytes.begin() + static_cast<std::ptrdiff_t>(layout.eocdOffset), record.begin(),
                     record.end());
        fixtures.push_back(
            {"invalid_zip64_eocd.cxc", std::move(bytes), "cxc.archive.feature_unsupported"});
    }
    {
        auto bytes = std::vector<std::byte>{canonical.begin(), canonical.end()};
        std::vector<std::byte> locator(20U);
        support::writeU32(locator, 0U, 0x07064B50U);
        bytes.insert(bytes.begin() + static_cast<std::ptrdiff_t>(layout.eocdOffset),
                     locator.begin(), locator.end());
        fixtures.push_back(
            {"invalid_zip64_locator.cxc", std::move(bytes), "cxc.archive.feature_unsupported"});
    }
    {
        auto bytes = std::vector<std::byte>{canonical.begin(), canonical.end()};
        std::vector<std::byte> extra(4U);
        support::writeU16(extra, 0U, 0x0001U);
        const auto extraOffset = first.centralHeaderOffset + 46U + first.path.size();
        bytes.insert(bytes.begin() + static_cast<std::ptrdiff_t>(extraOffset), extra.begin(),
                     extra.end());
        support::writeU16(bytes, first.centralHeaderOffset + 30U,
                          static_cast<std::uint16_t>(extra.size()));
        const auto newEocdOffset = layout.eocdOffset + extra.size();
        support::writeU32(bytes, newEocdOffset + 12U,
                          static_cast<std::uint32_t>(layout.centralSize + extra.size()));
        fixtures.push_back(
            {"invalid_zip64_extra_field.cxc", std::move(bytes), "cxc.archive.feature_unsupported"});
    }
    {
        auto bytes = std::vector<std::byte>{canonical.begin(), canonical.end()};
        support::writeU16(bytes, first.localHeaderOffset + 6U, 0x0008U);
        support::writeU16(bytes, first.centralHeaderOffset + 8U, 0x0008U);
        fixtures.push_back(
            {"invalid_data_descriptor.cxc", std::move(bytes), "cxc.archive.feature_unsupported"});
    }
    {
        auto bytes = std::vector<std::byte>{canonical.begin(), canonical.end()};
        support::writeU16(bytes, first.localHeaderOffset + 8U, 8U);
        support::writeU16(bytes, first.centralHeaderOffset + 10U, 8U);
        fixtures.push_back(
            {"invalid_compression.cxc", std::move(bytes), "cxc.archive.feature_unsupported"});
    }
    {
        auto bytes = std::vector<std::byte>{canonical.begin(), canonical.end()};
        support::writeU16(bytes, layout.eocdOffset + 4U, 1U);
        fixtures.push_back(
            {"invalid_multi_disk.cxc", std::move(bytes), "cxc.archive.feature_unsupported"});
    }
    {
        auto bytes = std::vector<std::byte>{canonical.begin(), canonical.end()};
        support::writeU16(bytes, first.centralHeaderOffset + 30U, 1U);
        fixtures.push_back(
            {"invalid_extra_field.cxc", std::move(bytes), "cxc.archive.feature_unsupported"});
    }
    {
        auto bytes = std::vector<std::byte>{canonical.begin(), canonical.end()};
        support::writeU16(bytes, layout.eocdOffset + 20U, 1U);
        fixtures.push_back(
            {"invalid_archive_comment.cxc", std::move(bytes), "cxc.archive.feature_unsupported"});
    }
    {
        auto bytes = std::vector<std::byte>{canonical.begin(), canonical.end()};
        support::writeU32(bytes, first.centralHeaderOffset + 38U, 0x10U);
        fixtures.push_back(
            {"invalid_directory.cxc", std::move(bytes), "cxc.archive.feature_unsupported"});
    }
    {
        auto bytes = std::vector<std::byte>{canonical.begin(), canonical.end()};
        support::writeU16(bytes, first.centralHeaderOffset + 4U, 0x0314U);
        support::writeU32(bytes, first.centralHeaderOffset + 38U, 0120000U << 16U);
        fixtures.push_back(
            {"invalid_symlink.cxc", std::move(bytes), "cxc.archive.feature_unsupported"});
    }
    {
        auto bytes = std::vector<std::byte>{canonical.begin(), canonical.end()};
        support::writeU16(bytes, first.localHeaderOffset + 8U, 8U);
        fixtures.push_back(
            {"invalid_header_mismatch.cxc", std::move(bytes), "cxc.archive.invalid"});
    }
    {
        auto bytes = std::vector<std::byte>{canonical.begin(), canonical.end()};
        const auto expandedSize = first.byteCount + 4U;
        support::writeU32(bytes, first.localHeaderOffset + 18U,
                          static_cast<std::uint32_t>(expandedSize));
        support::writeU32(bytes, first.localHeaderOffset + 22U,
                          static_cast<std::uint32_t>(expandedSize));
        support::writeU32(bytes, first.centralHeaderOffset + 20U,
                          static_cast<std::uint32_t>(expandedSize));
        support::writeU32(bytes, first.centralHeaderOffset + 24U,
                          static_cast<std::uint32_t>(expandedSize));
        const auto checksum = support::crc32(
            std::span<const std::byte>{bytes.data() + first.dataOffset, expandedSize});
        support::writeU32(bytes, first.localHeaderOffset + 14U, checksum);
        support::writeU32(bytes, first.centralHeaderOffset + 16U, checksum);
        fixtures.push_back({"invalid_overlap.cxc", std::move(bytes), "cxc.archive.invalid"});
    }
    {
        auto bytes = std::vector<std::byte>{canonical.begin(), canonical.end()};
        bytes.push_back(std::byte{0});
        fixtures.push_back({"invalid_trailing_bytes.cxc", std::move(bytes), "cxc.archive.invalid"});
    }
    {
        auto bytes = std::vector<std::byte>{canonical.begin(), canonical.end()};
        const auto& entry = support::findEntry(layout, "assets/textures/unused.bin");
        bytes[entry.dataOffset] ^= std::byte{0x01};
        fixtures.push_back({"invalid_crc.cxc", std::move(bytes), "cxc.archive.invalid"});
    }
    {
        auto bytes = std::vector<std::byte>{canonical.begin(), canonical.end()};
        const auto loaded = CxcPackageLoader::loadMemory(canonical);
        if (!loaded.hasValue()) {
            throw std::runtime_error{"Could not load canonical CXC fixture source"};
        }
        const auto found =
            std::ranges::find(loaded.package->manifest().entries, "assets/textures/unused.bin",
                              &cuexis::cxc::CxcManifestEntry::path);
        if (found == loaded.package->manifest().entries.end()) {
            throw std::runtime_error{"Could not locate canonical CXC fixture manifest entry"};
        }
        auto changedHash = found->sha256;
        changedHash[0] = changedHash[0] == '0' ? '1' : '0';
        support::replaceEntryText(bytes, "cuexis.cxc.json", found->sha256, changedHash);
        fixtures.push_back(
            {"invalid_manifest_hash.cxc", std::move(bytes), "cxc.entry.hash_mismatch"});
    }
    {
        auto bytes = std::vector<std::byte>{canonical.begin(), canonical.end()};
        const auto loaded = CxcPackageLoader::loadMemory(canonical);
        if (!loaded.hasValue()) {
            throw std::runtime_error{"Could not load canonical CXC fixture source"};
        }
        const auto found =
            std::ranges::find(loaded.package->manifest().entries, "assets/textures/unused.bin",
                              &cuexis::cxc::CxcManifestEntry::path);
        if (found == loaded.package->manifest().entries.end()) {
            throw std::runtime_error{"Could not locate canonical CXC fixture manifest entry"};
        }
        const auto oldValue = std::string{"\"byteCount\": "} + std::to_string(found->byteCount);
        const auto newValue =
            std::string{"\"byteCount\": "} + std::to_string(found->byteCount - 1U);
        support::replaceEntryText(bytes, "cuexis.cxc.json", oldValue, newValue);
        fixtures.push_back(
            {"invalid_manifest_size.cxc", std::move(bytes), "cxc.entry.size_mismatch"});
    }
    {
        auto bytes = support::writeRawZip(
            {support::binaryEntry("safe/A.bin", "a"), support::binaryEntry("safe/b.bin", "b")});
        support::replaceEntryPath(bytes, "safe/b.bin", "safe/a.bin");
        fixtures.push_back({"invalid_case_conflict.cxc", std::move(bytes), "cxc.entry.duplicate"});
    }
    {
        auto bytes = support::writeRawZip(
            {support::binaryEntry("safe/file", "a"), support::binaryEntry("safe/xxxx/child", "b")});
        support::replaceEntryPath(bytes, "safe/xxxx/child", "safe/file/child");
        fixtures.push_back({"invalid_path_prefix.cxc", std::move(bytes), "cxc.entry.duplicate"});
    }
    {
        auto bytes = support::writeRawZip({support::binaryEntry("safe/asset.bin", "payload")});
        support::replaceEntryPath(bytes, "safe/asset.bin", "../e/asset.bin");
        fixtures.push_back({"invalid_parent_path.cxc", std::move(bytes), "cxc.entry.path_invalid"});
    }
    return fixtures;
}

void updateFixtures(std::span<const std::byte> canonical,
                    std::span<const std::byte> staticCanonical,
                    const std::vector<BinaryFixture>& fixtures) {
    support::writeBytes(support::goldenFixtureRoot() / "cxc_v1_v4_cxt.cxc", canonical);
    const auto layout = support::parseZip(canonical);
    const auto& manifest = support::findEntry(layout, "cuexis.cxc.json");
    support::writeBytes(support::goldenFixtureRoot() / "cxc_v1_v4_cxt.manifest.json",
                        canonical.subspan(manifest.dataOffset, manifest.byteCount));
    support::writeBytes(support::goldenFixtureRoot() / "cxc_v1_v4_static.cxc", staticCanonical);
    const auto staticLayout = support::parseZip(staticCanonical);
    const auto& staticManifest = support::findEntry(staticLayout, "cuexis.cxc.json");
    support::writeBytes(
        support::goldenFixtureRoot() / "cxc_v1_v4_static.manifest.json",
        staticCanonical.subspan(staticManifest.dataOffset, staticManifest.byteCount));
    const auto noncanonical = noncanonicalMetadata(canonical);
    support::writeBytes(support::binaryFixtureRoot() / "valid_noncanonical_metadata.cxc",
                        noncanonical);
    for (const auto& fixture : fixtures) {
        support::writeBytes(support::binaryFixtureRoot() / fixture.name, fixture.bytes);
    }
}

} // namespace

TEST_CASE("Committed CXC binary fixtures match production Writer bytes and stable rejection gates",
          "[cxc][fixtures][zip32][cfu-c3]") {
    const auto canonical = canonicalPackage();
    const auto staticCanonical = canonicalStaticPackage();
    const auto fixtures = generatedFixtures(canonical);
    if (shouldUpdateFixtures()) {
        updateFixtures(canonical, staticCanonical, fixtures);
    }

    const auto golden = support::readBytes(support::goldenFixtureRoot() / "cxc_v1_v4_cxt.cxc");
    REQUIRE(golden == canonical);
    auto loadedGolden = CxcPackageLoader::loadMemory(golden);
    INFO(support::diagnosticsText(loadedGolden.diagnostics));
    REQUIRE(loadedGolden.hasValue());

    const auto layout = support::parseZip(golden);
    const auto& manifest = support::findEntry(layout, "cuexis.cxc.json");
    const auto manifestText = support::textFromBytes(
        std::span<const std::byte>{golden.data() + manifest.dataOffset, manifest.byteCount});
    CHECK(manifestText ==
          support::readText(support::goldenFixtureRoot() / "cxc_v1_v4_cxt.manifest.json"));

    const auto noncanonical =
        support::readBytes(support::binaryFixtureRoot() / "valid_noncanonical_metadata.cxc");
    CHECK(noncanonical == noncanonicalMetadata(canonical));
    auto loadedNoncanonical = CxcPackageLoader::loadMemory(noncanonical);
    INFO(support::diagnosticsText(loadedNoncanonical.diagnostics));
    REQUIRE(loadedNoncanonical.hasValue());
    CHECK(loadedNoncanonical.package->identity() != loadedGolden.package->identity());

    const auto staticGolden =
        support::readBytes(support::goldenFixtureRoot() / "cxc_v1_v4_static.cxc");
    REQUIRE(staticGolden == staticCanonical);
    auto loadedStatic = CxcPackageLoader::loadMemory(staticGolden);
    INFO(support::diagnosticsText(loadedStatic.diagnostics));
    REQUIRE(loadedStatic.hasValue());
    const auto staticLayout = support::parseZip(staticGolden);
    const auto& staticManifest = support::findEntry(staticLayout, "cuexis.cxc.json");
    const auto staticManifestText = support::textFromBytes(std::span<const std::byte>{
        staticGolden.data() + staticManifest.dataOffset, staticManifest.byteCount});
    CHECK(staticManifestText ==
          support::readText(support::goldenFixtureRoot() / "cxc_v1_v4_static.manifest.json"));

    for (const auto& fixture : fixtures) {
        const auto committed = support::readBytes(support::binaryFixtureRoot() / fixture.name);
        INFO(fixture.name);
        REQUIRE(committed == fixture.bytes);
        const auto result = CxcPackageLoader::loadMemory(committed);
        INFO(support::diagnosticsText(result.diagnostics));
        CHECK_FALSE(result.hasValue());
        CHECK_FALSE(result.package.has_value());
        CHECK(support::hasDiagnostic(result.diagnostics, fixture.expectedDiagnostic));
    }
}
