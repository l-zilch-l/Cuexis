#include "cxc_test_support.hpp"

#include <cuexis/cxc/cxc_manifest_loader.hpp>
#include <cuexis/cxc/cxc_package.hpp>
#include <cuexis/cxc/cxc_writer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace support = cuexis::cxc::test;

constexpr std::string_view shaA =
    "1111111111111111111111111111111111111111111111111111111111111111";
constexpr std::string_view shaB =
    "2222222222222222222222222222222222222222222222222222222222222222";

[[nodiscard]] auto manifestText(const std::vector<std::pair<std::string, std::uint64_t>>& entries,
                                std::string_view format = "cuexis.cxc", std::uint64_t version = 1,
                                std::string_view project = "cuexis.project.json") -> std::string {
    std::ostringstream output;
    output << "{\"format\":\"" << format << "\",\"version\":" << version << ",\"project\":\""
           << project << "\",\"entries\":[";
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << "{\"path\":\"" << entries[index].first
               << "\",\"byteCount\":" << entries[index].second << ",\"sha256\":\""
               << (index == 0 ? shaA : shaB) << "\"}";
    }
    output << "],\"requiredExtensions\":[],\"extensions\":{}}";
    return output.str();
}

[[nodiscard]] auto diagnosticSignature(const cuexis::core::Diagnostics& diagnostics)
    -> std::string {
    std::ostringstream output;
    bool first = true;
    for (const auto& diagnostic : diagnostics.items()) {
        if (!first) {
            output << '|';
        }
        first = false;
        output << diagnostic.code() << '@' << diagnostic.fieldPath();
    }
    return output.str();
}

[[nodiscard]] auto findRequestEntry(cuexis::cxc::CxcWriteRequest& request, std::string_view path)
    -> cuexis::cxc::CxcWriteEntry& {
    const auto found = std::ranges::find(request.entries, path, &cuexis::cxc::CxcWriteEntry::path);
    if (found == request.entries.end()) {
        throw std::runtime_error{"F4 test request entry is missing"};
    }
    return *found;
}

} // namespace

TEST_CASE("CFU-F4 manifest aggregate byte budgets accept exact total and reject plus one",
          "[cxc][cfu-f4][limits][manifest]") {
    const auto manifest = manifestText({{"a.bin", 1}, {"cuexis.project.json", 2}});
    auto limits = cuexis::cxc::CxcManifestLimits{};
    limits.maxEntries = 2;
    limits.maxEntryBytes = 2;
    limits.maxListedBytes = 3;

    const auto exact = cuexis::cxc::CxcManifestLoader::load(manifest, limits);
    INFO(support::diagnosticsText(exact.diagnostics));
    REQUIRE(exact.hasValue());

    limits.maxListedBytes = 2;
    const auto over = cuexis::cxc::CxcManifestLoader::load(manifest, limits);
    INFO(support::diagnosticsText(over.diagnostics));
    CHECK_FALSE(over.hasValue());
    CHECK(support::hasDiagnostic(over.diagnostics, "cxc.budget.exceeded"));
}

TEST_CASE("CFU-F4 manifest byte accounting rejects integer overflow from pseudo totals",
          "[cxc][cfu-f4][limits][overflow][manifest]") {
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    const auto manifest = manifestText({{"a.bin", maximum}, {"cuexis.project.json", 1}});
    auto limits = cuexis::cxc::CxcManifestLimits{};
    limits.maxEntries = 2;
    limits.maxEntryBytes = maximum;
    limits.maxListedBytes = maximum;

    const auto result = cuexis::cxc::CxcManifestLoader::load(manifest, limits);
    INFO(support::diagnosticsText(result.diagnostics));
    CHECK_FALSE(result.hasValue());
    CHECK(support::hasDiagnostic(result.diagnostics, "cxc.budget.exceeded"));
}

TEST_CASE("CFU-F4 manifest diagnostic truncation has a stable signature",
          "[cxc][cfu-f4][limits][diagnostics]") {
    const auto invalid = manifestText({}, "wrong.format", 2, "wrong.project.json");
    auto limits = cuexis::cxc::CxcManifestLimits{};
    limits.maxDiagnostics = 3;

    const auto first = cuexis::cxc::CxcManifestLoader::load(invalid, limits);
    const auto second = cuexis::cxc::CxcManifestLoader::load(invalid, limits);
    REQUIRE_FALSE(first.hasValue());
    REQUIRE(first.diagnostics.limitReached());
    REQUIRE(first.diagnostics.size() == 3);
    CHECK(diagnosticSignature(first.diagnostics) == diagnosticSignature(second.diagnostics));
    CHECK(diagnosticSignature(first.diagnostics) ==
          "cxc.budget.exceeded@$|cxc.format.unsupported@$/format|"
          "cxc.version.unsupported@$/version");
}

TEST_CASE("CFU-F4 ZIP32 pseudo headers reject offset and data range overflow",
          "[cxc][cfu-f4][zip32][offset][overflow]") {
    const auto canonical = support::writePackage(support::makeV4StaticRequest());
    const auto layout = support::parseZip(canonical);
    REQUIRE_FALSE(layout.entries.empty());

    SECTION("central offset plus size") {
        auto bytes = canonical;
        support::writeU32(bytes, layout.eocdOffset + 12U, 2U);
        support::writeU32(bytes, layout.eocdOffset + 16U, 0xFFFFFFFEU);
        const auto result = cuexis::cxc::CxcPackageLoader::loadMemory(std::move(bytes));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.archive.invalid"));
    }

    SECTION("local data end") {
        auto bytes = canonical;
        const auto& entry = layout.entries.front();
        constexpr std::uint32_t pseudoSize = 0xFFFFFFFEU;
        support::writeU32(bytes, entry.localHeaderOffset + 18U, pseudoSize);
        support::writeU32(bytes, entry.localHeaderOffset + 22U, pseudoSize);
        support::writeU32(bytes, entry.centralHeaderOffset + 20U, pseudoSize);
        support::writeU32(bytes, entry.centralHeaderOffset + 24U, pseudoSize);
        auto limits = cuexis::cxc::CxcPackageLimits{};
        limits.maxEntryBytes = pseudoSize;
        const auto result = cuexis::cxc::CxcPackageLoader::loadMemory(std::move(bytes), limits);
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.archive.invalid"));
    }
}

TEST_CASE("CFU-F4 ZIP32 configured totals accept exact maxima and reject boundary plus one",
          "[cxc][cfu-f4][zip32][limits]") {
    const auto canonical = support::writePackage(support::makeV4StaticRequest());
    const auto layout = support::parseZip(canonical);
    REQUIRE(layout.entries.size() >= 2);

    auto limits = cuexis::cxc::CxcPackageLimits{};
    limits.maxPackageBytes = canonical.size();
    limits.maxEntries = layout.entries.size();
    const auto exact = cuexis::cxc::CxcPackageLoader::loadMemory(canonical, limits);
    INFO(support::diagnosticsText(exact.diagnostics));
    REQUIRE(exact.hasValue());

    limits.maxEntries = layout.entries.size() - 1U;
    const auto overEntries = cuexis::cxc::CxcPackageLoader::loadMemory(canonical, limits);
    CHECK_FALSE(overEntries.hasValue());
    CHECK(support::hasDiagnostic(overEntries.diagnostics, "cxc.budget.exceeded"));

    limits.maxEntries = layout.entries.size();
    limits.maxPackageBytes = canonical.size() - 1U;
    const auto overBytes = cuexis::cxc::CxcPackageLoader::loadMemory(canonical, limits);
    CHECK_FALSE(overBytes.hasValue());
    CHECK(support::hasDiagnostic(overBytes.diagnostics, "cxc.budget.exceeded"));
}

TEST_CASE("CFU-F4 closure aggregate Asset totals accept exact max and reject plus one",
          "[cxc][cfu-f4][closure][limits]") {
    auto makeRequest = [] {
        auto request = support::makeV4CxtRequest();
        auto& index = findRequestEntry(request, "assets/cuexis.asset-index.json");
        index = support::textEntry(
            index.path,
            R"({"format":"cuexis.asset-index","version":1,"assets":[{"id":"texture.extra","type":"texture","source":"textures/extra.bin","dependencies":[]},{"id":"texture.unused","type":"texture","source":"textures/unused.bin","dependencies":[]}],"extensions":{}})");
        request.entries.push_back(
            support::binaryEntry("assets/textures/extra.bin", "extra-resource"));
        return request;
    };

    auto limits = cuexis::cxc::CxcPackageLimits{};
    limits.assetIndex.maxAssets = 2;
    const auto exact = cuexis::cxc::CxcWriter::write(makeRequest(), limits);
    INFO(support::diagnosticsText(exact.diagnostics));
    REQUIRE(exact.hasValue());

    limits.assetIndex.maxAssets = 1;
    const auto over = cuexis::cxc::CxcWriter::write(makeRequest(), limits);
    INFO(support::diagnosticsText(over.diagnostics));
    CHECK_FALSE(over.hasValue());
    CHECK(support::hasDiagnostic(over.diagnostics, "asset_index.assets.limit"));
    CHECK(support::hasDiagnostic(over.diagnostics, "cxc.project.invalid"));
}
