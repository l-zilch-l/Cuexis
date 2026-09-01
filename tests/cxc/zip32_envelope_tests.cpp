#include "cxc_test_support.hpp"

#include "cxc_hash_internal.hpp"
#include "zip32_envelope_internal.hpp"

#include <cuexis/cxc/cxc_package.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using cuexis::cxc::CxcPackageLimits;
using cuexis::cxc::CxcPackageLoader;
using cuexis::cxc::CxcWriteEntry;
namespace detail = cuexis::cxc::detail;
namespace support = cuexis::cxc::test;

void checkRejected(std::vector<std::byte> bytes, std::string_view code) {
    const auto result = CxcPackageLoader::loadMemory(std::move(bytes));
    INFO(support::diagnosticsText(result.diagnostics));
    CHECK_FALSE(result.hasValue());
    CHECK(support::hasDiagnostic(result.diagnostics, code));
}

[[nodiscard]] auto rawEntryZip(std::string path = "safe/asset.bin") -> std::vector<std::byte> {
    return support::writeRawZip({support::binaryEntry(std::move(path), "payload")});
}

} // namespace

TEST_CASE("CXC SHA-256 and CRC32 primitives match standard vectors", "[cxc][hash][cfu-c3]") {
    const auto empty = std::span<const std::byte>{};
    CHECK(detail::sha256Hex(empty) ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    const auto abc = support::bytesFromText("abc");
    CHECK(detail::sha256Hex(abc) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const auto digits = support::bytesFromText("123456789");
    CHECK(support::crc32(digits) == 0xCBF43926U);
}

TEST_CASE("CXC ZIP32 envelope rejects unsupported archive features before extraction",
          "[cxc][zip32][features][cfu-c3]") {
    const auto canonical = support::writePackage(support::makeV4CxtRequest());
    const auto layout = support::parseZip(canonical);
    REQUIRE_FALSE(layout.entries.empty());
    const auto& first = layout.entries.front();

    SECTION("ZIP64 entry count sentinel") {
        auto bytes = canonical;
        support::writeU16(bytes, layout.eocdOffset + 8U, 0xFFFFU);
        support::writeU16(bytes, layout.eocdOffset + 10U, 0xFFFFU);
        checkRejected(std::move(bytes), "cxc.archive.feature_unsupported");
    }

    SECTION("ZIP64 central size sentinel") {
        auto bytes = canonical;
        support::writeU32(bytes, layout.eocdOffset + 12U, 0xFFFFFFFFU);
        checkRejected(std::move(bytes), "cxc.archive.feature_unsupported");
    }

    SECTION("ZIP64 local size sentinel") {
        auto bytes = canonical;
        support::writeU32(bytes, first.localHeaderOffset + 18U, 0xFFFFFFFFU);
        checkRejected(std::move(bytes), "cxc.archive.feature_unsupported");
    }

    SECTION("ZIP64 end record locator") {
        auto bytes = canonical;
        std::vector<std::byte> locator(20U);
        support::writeU32(locator, 0U, 0x07064B50U);
        bytes.insert(bytes.begin() + static_cast<std::ptrdiff_t>(layout.eocdOffset),
                     locator.begin(), locator.end());
        checkRejected(std::move(bytes), "cxc.archive.feature_unsupported");
    }

    SECTION("ZIP64 end record") {
        auto bytes = canonical;
        std::vector<std::byte> endRecord(56U);
        support::writeU32(endRecord, 0U, 0x06064B50U);
        bytes.insert(bytes.begin() + static_cast<std::ptrdiff_t>(layout.eocdOffset),
                     endRecord.begin(), endRecord.end());
        checkRejected(std::move(bytes), "cxc.archive.feature_unsupported");
    }

    SECTION("ZIP64 central extra field") {
        auto bytes = canonical;
        support::writeU16(bytes, first.centralHeaderOffset + 30U, 4U);
        std::vector<std::byte> extra(4U);
        support::writeU16(extra, 0U, 0x0001U);
        const auto extraOffset = first.centralHeaderOffset + 46U + first.path.size();
        bytes.insert(bytes.begin() + static_cast<std::ptrdiff_t>(extraOffset), extra.begin(),
                     extra.end());
        support::writeU32(bytes, layout.eocdOffset + extra.size() + 12U,
                          static_cast<std::uint32_t>(layout.centralSize + extra.size()));
        checkRejected(std::move(bytes), "cxc.archive.feature_unsupported");
    }

    SECTION("data descriptor flag") {
        auto bytes = canonical;
        support::writeU16(bytes, first.localHeaderOffset + 6U, 0x0008U);
        support::writeU16(bytes, first.centralHeaderOffset + 8U, 0x0008U);
        checkRejected(std::move(bytes), "cxc.archive.feature_unsupported");
    }

    SECTION("encryption flag") {
        auto bytes = canonical;
        support::writeU16(bytes, first.localHeaderOffset + 6U, 0x0001U);
        support::writeU16(bytes, first.centralHeaderOffset + 8U, 0x0001U);
        checkRejected(std::move(bytes), "cxc.archive.feature_unsupported");
    }

    SECTION("compression") {
        auto bytes = canonical;
        support::writeU16(bytes, first.localHeaderOffset + 8U, 8U);
        support::writeU16(bytes, first.centralHeaderOffset + 10U, 8U);
        checkRejected(std::move(bytes), "cxc.archive.feature_unsupported");
    }

    SECTION("unsupported version") {
        auto bytes = canonical;
        support::writeU16(bytes, first.localHeaderOffset + 4U, 21U);
        support::writeU16(bytes, first.centralHeaderOffset + 6U, 21U);
        checkRejected(std::move(bytes), "cxc.archive.feature_unsupported");
    }

    SECTION("multi-disk") {
        auto bytes = canonical;
        support::writeU16(bytes, layout.eocdOffset + 4U, 1U);
        checkRejected(std::move(bytes), "cxc.archive.feature_unsupported");
    }

    SECTION("central extra field") {
        auto bytes = canonical;
        support::writeU16(bytes, first.centralHeaderOffset + 30U, 1U);
        checkRejected(std::move(bytes), "cxc.archive.feature_unsupported");
    }

    SECTION("local extra field") {
        auto bytes = canonical;
        support::writeU16(bytes, first.localHeaderOffset + 28U, 1U);
        checkRejected(std::move(bytes), "cxc.archive.feature_unsupported");
    }

    SECTION("entry comment") {
        auto bytes = canonical;
        support::writeU16(bytes, first.centralHeaderOffset + 32U, 1U);
        checkRejected(std::move(bytes), "cxc.archive.feature_unsupported");
    }

    SECTION("archive comment") {
        auto bytes = canonical;
        support::writeU16(bytes, layout.eocdOffset + 20U, 1U);
        checkRejected(std::move(bytes), "cxc.archive.feature_unsupported");
    }

    SECTION("DOS directory") {
        auto bytes = canonical;
        support::writeU32(bytes, first.centralHeaderOffset + 38U, 0x10U);
        checkRejected(std::move(bytes), "cxc.archive.feature_unsupported");
    }

    SECTION("DOS volume label") {
        auto bytes = canonical;
        support::writeU32(bytes, first.centralHeaderOffset + 38U, 0x08U);
        checkRejected(std::move(bytes), "cxc.archive.feature_unsupported");
    }

    SECTION("UNIX symlink") {
        auto bytes = canonical;
        support::writeU16(bytes, first.centralHeaderOffset + 4U, 0x0314U);
        support::writeU32(bytes, first.centralHeaderOffset + 38U, 0120000U << 16U);
        checkRejected(std::move(bytes), "cxc.archive.feature_unsupported");
    }
}

TEST_CASE("CXC ZIP32 envelope rejects mismatched overlapping and trailing ranges",
          "[cxc][zip32][ranges][cfu-c3]") {
    const auto canonical = support::writePackage(support::makeV4CxtRequest());
    const auto layout = support::parseZip(canonical);
    REQUIRE(layout.entries.size() >= 2);
    const auto& first = layout.entries[0];

    SECTION("local and central mismatch") {
        auto bytes = canonical;
        support::writeU16(bytes, first.localHeaderOffset + 8U, 8U);
        checkRejected(std::move(bytes), "cxc.archive.invalid");
    }

    SECTION("invalid local offset") {
        auto bytes = canonical;
        support::writeU32(bytes, first.centralHeaderOffset + 42U, 1U);
        checkRejected(std::move(bytes), "cxc.archive.invalid");
    }

    SECTION("overlapping local entry ranges") {
        auto bytes = canonical;
        const auto expandedSize = first.byteCount + 4U;
        REQUIRE(first.dataOffset + expandedSize <= layout.centralOffset);
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
        checkRejected(std::move(bytes), "cxc.archive.invalid");
    }

    SECTION("central span mismatch") {
        auto bytes = canonical;
        support::writeU32(bytes, layout.eocdOffset + 12U,
                          static_cast<std::uint32_t>(layout.centralSize - 1U));
        checkRejected(std::move(bytes), "cxc.archive.invalid");
    }

    SECTION("trailing bytes") {
        auto bytes = canonical;
        bytes.push_back(std::byte{0});
        checkRejected(std::move(bytes), "cxc.archive.invalid");
    }
}

TEST_CASE("CXC ZIP32 envelope rejects nonportable and case-conflicting paths",
          "[cxc][zip32][paths][cfu-c3]") {
    SECTION("parent traversal") {
        auto bytes = rawEntryZip();
        support::replaceEntryPath(bytes, "safe/asset.bin", "../e/asset.bin");
        checkRejected(std::move(bytes), "cxc.entry.path_invalid");
    }

    SECTION("absolute path") {
        auto bytes = rawEntryZip();
        support::replaceEntryPath(bytes, "safe/asset.bin", "/afe/asset.bin");
        checkRejected(std::move(bytes), "cxc.entry.path_invalid");
    }

    SECTION("backslash") {
        auto bytes = rawEntryZip();
        support::replaceEntryPath(bytes, "safe/asset.bin", "safe\\asset.bin");
        checkRejected(std::move(bytes), "cxc.entry.path_invalid");
    }

    SECTION("reserved Windows segment") {
        auto bytes = rawEntryZip();
        support::replaceEntryPath(bytes, "safe/asset.bin", "CON./asset.bin");
        checkRejected(std::move(bytes), "cxc.entry.path_invalid");
    }

    SECTION("trailing dot") {
        auto bytes = rawEntryZip();
        support::replaceEntryPath(bytes, "safe/asset.bin", "safe/asset.bi.");
        checkRejected(std::move(bytes), "cxc.entry.path_invalid");
    }

    SECTION("ASCII case-fold conflict") {
        auto bytes = support::writeRawZip(
            {support::binaryEntry("safe/A.bin", "a"), support::binaryEntry("safe/b.bin", "b")});
        support::replaceEntryPath(bytes, "safe/b.bin", "safe/a.bin");
        checkRejected(std::move(bytes), "cxc.entry.duplicate");
    }

    SECTION("file and descendant path conflict") {
        auto bytes = support::writeRawZip(
            {support::binaryEntry("safe/file", "a"), support::binaryEntry("safe/xxxx/child", "b")});
        support::replaceEntryPath(bytes, "safe/xxxx/child", "safe/file/child");
        checkRejected(std::move(bytes), "cxc.entry.duplicate");
    }
}

TEST_CASE("CXC ZIP32 envelope enforces exact count size path and field budgets",
          "[cxc][zip32][limits][cfu-c3]") {
    const auto raw = rawEntryZip();

    SECTION("exact package boundary") {
        auto limits = CxcPackageLimits{};
        limits.maxPackageBytes = raw.size();
        const auto accepted = detail::validateZip32Envelope(raw, limits);
        REQUIRE(accepted.hasValue());
        --limits.maxPackageBytes;
        const auto rejected = detail::validateZip32Envelope(raw, limits);
        CHECK_FALSE(rejected.hasValue());
        CHECK(support::hasDiagnostic(rejected.diagnostics, "cxc.budget.exceeded"));
    }

    SECTION("entry byte boundary") {
        const auto layout = support::parseZip(raw);
        REQUIRE(layout.entries.size() == 1);
        auto limits = CxcPackageLimits{};
        limits.maxEntryBytes = layout.entries[0].byteCount;
        REQUIRE(detail::validateZip32Envelope(raw, limits).hasValue());
        --limits.maxEntryBytes;
        const auto rejected = detail::validateZip32Envelope(raw, limits);
        CHECK_FALSE(rejected.hasValue());
        CHECK(support::hasDiagnostic(rejected.diagnostics, "cxc.budget.exceeded"));
    }

    SECTION("entry count boundary") {
        auto limits = CxcPackageLimits{};
        limits.maxEntries = 1;
        REQUIRE(detail::validateZip32Envelope(raw, limits).hasValue());
        limits.maxEntries = 0;
        const auto rejected = detail::validateZip32Envelope(raw, limits);
        CHECK_FALSE(rejected.hasValue());
        CHECK(support::hasDiagnostic(rejected.diagnostics, "cxc.budget.exceeded"));
    }

    SECTION("path depth") {
        auto limits = CxcPackageLimits{};
        limits.maxPathDepth = 1;
        const auto rejected = detail::validateZip32Envelope(raw, limits);
        CHECK_FALSE(rejected.hasValue());
        CHECK(support::hasDiagnostic(rejected.diagnostics, "cxc.entry.path_invalid"));
    }

    SECTION("ZIP filename field limit") {
        auto limits = CxcPackageLimits{};
        limits.maxPathBytes = 65536U;
        limits.maxPathDepth = 1;
        std::vector<std::pair<std::string, std::vector<std::byte>>> entries;
        entries.emplace_back(std::string(65536U, 'a'), std::vector<std::byte>{});
        const auto rejected = detail::writeCanonicalZip32(entries, limits);
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.error().code() == "cxc.budget.exceeded");
    }

    SECTION("canonical writer path-prefix conflict") {
        const std::vector<std::pair<std::string, std::vector<std::byte>>> entries{
            {"safe/file", {}}, {"safe/file/child", {}}};
        const auto rejected = detail::writeCanonicalZip32(entries, CxcPackageLimits{});
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.error().code() == "cxc.entry.duplicate");
    }
}

TEST_CASE("CXC ZIP32 writer rejects empty archives and invalid zero limits",
          "[cxc][zip32][boundary]") {
    const std::vector<std::pair<std::string, std::vector<std::byte>>> empty;
    const auto emptyArchive = detail::writeCanonicalZip32(empty, CxcPackageLimits{});
    REQUIRE_FALSE(emptyArchive.has_value());
    CHECK(emptyArchive.error().code() == "cxc.budget.exceeded");

    auto limits = CxcPackageLimits{};
    limits.maxPackageBytes = 0;
    const auto invalidLimits = detail::validateZip32Envelope({}, limits);
    CHECK_FALSE(invalidLimits.hasValue());
    REQUIRE_FALSE(invalidLimits.diagnostics.empty());
    CHECK(invalidLimits.diagnostics.items().front().code() == "cxc.budget.exceeded");
}

TEST_CASE("CXC ZIP32 validator rejects malformed end records and unrepresentable writer input",
          "[cxc][zip32][branch-coverage]") {
    SECTION("truncated and malformed end records") {
        const auto truncated = detail::validateZip32Envelope({}, CxcPackageLimits{});
        CHECK_FALSE(truncated.hasValue());
        CHECK(support::hasDiagnostic(truncated.diagnostics, "cxc.archive.invalid"));

        std::vector<std::byte> malformed(22U);
        const auto malformedResult = detail::validateZip32Envelope(malformed, CxcPackageLimits{});
        CHECK_FALSE(malformedResult.hasValue());
        CHECK(support::hasDiagnostic(malformedResult.diagnostics, "cxc.archive.invalid"));
    }

    SECTION("zero archive entries and damaged central headers") {
        const auto canonical = support::writePackage(support::makeV4StaticRequest());
        const auto layout = support::parseZip(canonical);

        auto zeroEntries = canonical;
        support::writeU16(zeroEntries, layout.eocdOffset + 8U, 0U);
        support::writeU16(zeroEntries, layout.eocdOffset + 10U, 0U);
        const auto zeroResult = detail::validateZip32Envelope(zeroEntries, CxcPackageLimits{});
        CHECK_FALSE(zeroResult.hasValue());
        CHECK(support::hasDiagnostic(zeroResult.diagnostics, "cxc.budget.exceeded"));

        auto centralHeader = canonical;
        support::writeU32(centralHeader, layout.centralOffset, 0U);
        const auto centralResult = detail::validateZip32Envelope(centralHeader, CxcPackageLimits{});
        CHECK_FALSE(centralResult.hasValue());
        CHECK(support::hasDiagnostic(centralResult.diagnostics, "cxc.archive.invalid"));
    }

    SECTION("writer refuses directory entries and payloads beyond its configured limit") {
        const std::vector<std::pair<std::string, std::vector<std::byte>>> directory{
            {"assets/", {}}};
        const auto directoryResult = detail::writeCanonicalZip32(directory, CxcPackageLimits{});
        REQUIRE_FALSE(directoryResult.has_value());
        CHECK(directoryResult.error().code() == "cxc.entry.path_invalid");

        auto limits = CxcPackageLimits{};
        limits.maxEntryBytes = 1;
        const std::vector<std::pair<std::string, std::vector<std::byte>>> oversized{
            {"assets/data.bin", {std::byte{0}, std::byte{1}}}};
        const auto oversizedResult = detail::writeCanonicalZip32(oversized, limits);
        REQUIRE_FALSE(oversizedResult.has_value());
        CHECK(oversizedResult.error().code() == "cxc.budget.exceeded");

        limits = CxcPackageLimits{};
        limits.maxPackageBytes = 22U;
        const std::vector<std::pair<std::string, std::vector<std::byte>>> packageLimit{
            {"assets/data.bin", {}}};
        const auto packageResult = detail::writeCanonicalZip32(packageLimit, limits);
        REQUIRE_FALSE(packageResult.has_value());
        CHECK(packageResult.error().code() == "cxc.budget.exceeded");
    }
}

TEST_CASE("CXC ZIP32 validator checks local metadata against central records",
          "[cxc][zip32][branch-coverage]") {
    const auto canonical = support::writePackage(support::makeV4StaticRequest());
    const auto layout = support::parseZip(canonical);
    REQUIRE_FALSE(layout.entries.empty());
    const auto& first = layout.entries.front();

    SECTION("UNIX regular-file metadata remains accepted") {
        auto bytes = canonical;
        support::writeU16(bytes, first.centralHeaderOffset + 4U, 0x0314U);
        support::writeU32(bytes, first.centralHeaderOffset + 38U, 0100000U << 16U);
        const auto result = CxcPackageLoader::loadMemory(std::move(bytes));
        INFO(support::diagnosticsText(result.diagnostics));
        REQUIRE(result.hasValue());
        CHECK_FALSE(result.package->entries().empty());
    }

    SECTION("central size mismatch is rejected before project parsing") {
        auto bytes = canonical;
        support::writeU32(bytes, first.centralHeaderOffset + 20U,
                          static_cast<std::uint32_t>(first.byteCount + 1U));
        const auto result = CxcPackageLoader::loadMemory(std::move(bytes));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.archive.invalid"));
        CHECK_FALSE(result.package.has_value());
    }

    SECTION("local filename must match its central directory entry") {
        auto bytes = canonical;
        const auto replacement = support::bytesFromText(std::string{"x"} + first.path.substr(1));
        std::copy(replacement.begin(), replacement.end(),
                  bytes.begin() + static_cast<std::ptrdiff_t>(first.localHeaderOffset + 30U));
        const auto result = CxcPackageLoader::loadMemory(std::move(bytes));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.archive.invalid"));
        CHECK_FALSE(result.package.has_value());
    }
}
