#include <cuexis/cxc/cxc_manifest_loader.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

[[nodiscard]] auto readFile(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Could not open CXC manifest fixture: " + path.string()};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

[[nodiscard]] auto hasDiagnostic(const cuexis::cxc::CxcManifestResult& result,
                                 std::string_view code) -> bool {
    for (const auto& diagnostic : result.diagnostics.items()) {
        if (diagnostic.code() == code) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("CXC manifest Reader accepts promoted v1 fixtures", "[cxc][manifest][cfu-c1]") {
    const auto root = std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" /
                      "chart_format_update" / "valid";
    for (const auto* name : {"cxc_manifest_v1.json", "cxc_manifest_cxt.json"}) {
        const auto result = cuexis::cxc::CxcManifestLoader::load(readFile(root / name));
        INFO(name);
        REQUIRE(result.hasValue());
        CHECK(result.document->projectPath == "cuexis.project.json");
        CHECK_FALSE(result.document->entries.empty());
    }
}

TEST_CASE("CXC manifest Reader rejects noncanonical entry order", "[cxc][manifest][cfu-c1]") {
    const auto path = std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" /
                      "chart_format_update" / "invalid" / "cxc_manifest_unsorted.json";
    const auto result = cuexis::cxc::CxcManifestLoader::load(readFile(path));
    CHECK_FALSE(result.hasValue());
    CHECK(hasDiagnostic(result, "cxc.entry.order_invalid"));
}

TEST_CASE("CXC manifest Reader rejects ASCII case path conflicts", "[cxc][manifest][cfu-c1]") {
    const auto path = std::filesystem::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" /
                      "chart_format_update" / "invalid" / "cxc_manifest_case_conflict.json";
    const auto result = cuexis::cxc::CxcManifestLoader::load(readFile(path));
    CHECK_FALSE(result.hasValue());
    CHECK(hasDiagnostic(result, "cxc.entry.duplicate"));
}

TEST_CASE("CXC manifest Reader rejects archive path-prefix conflicts",
          "[cxc][manifest][paths][cfu-c3]") {
    constexpr std::string_view manifest = R"({
      "format":"cuexis.cxc","version":1,"project":"cuexis.project.json",
      "entries":[
        {"path":"assets","byteCount":1,
         "sha256":"1111111111111111111111111111111111111111111111111111111111111111"},
        {"path":"assets/item.bin","byteCount":1,
         "sha256":"2222222222222222222222222222222222222222222222222222222222222222"},
        {"path":"cuexis.project.json","byteCount":1,
         "sha256":"3333333333333333333333333333333333333333333333333333333333333333"}
      ],
      "requiredExtensions":[],"extensions":{}
    })";
    const auto result = cuexis::cxc::CxcManifestLoader::load(manifest);
    CHECK_FALSE(result.hasValue());
    CHECK(hasDiagnostic(result, "cxc.entry.duplicate"));
}

TEST_CASE("CXC manifest Reader rejects nonportable Windows paths", "[cxc][manifest][cfu-c1]") {
    constexpr std::string_view manifest = R"({
      "format":"cuexis.cxc","version":1,"project":"cuexis.project.json",
      "entries":[
        {"path":"assets/CON.txt","byteCount":1,
         "sha256":"1111111111111111111111111111111111111111111111111111111111111111"},
        {"path":"cuexis.project.json.","byteCount":1,
         "sha256":"2222222222222222222222222222222222222222222222222222222222222222"}
      ],
      "requiredExtensions":[],"extensions":{}
    })";
    const auto result = cuexis::cxc::CxcManifestLoader::load(manifest);
    CHECK_FALSE(result.hasValue());
    CHECK(hasDiagnostic(result, "cxc.entry.path_invalid"));
    CHECK(hasDiagnostic(result, "cxc.entry.missing"));
}

TEST_CASE("CXC manifest Reader enforces listed byte budgets", "[cxc][manifest][cfu-c1]") {
    constexpr std::string_view manifest = R"({
      "format":"cuexis.cxc","version":1,"project":"cuexis.project.json",
      "entries":[
        {"path":"cuexis.project.json","byteCount":2,
         "sha256":"1111111111111111111111111111111111111111111111111111111111111111"}
      ],
      "requiredExtensions":[],"extensions":{}
    })";
    auto limits = cuexis::cxc::CxcManifestLimits{};
    limits.maxListedBytes = 1;
    const auto result = cuexis::cxc::CxcManifestLoader::load(manifest, limits);
    CHECK_FALSE(result.hasValue());
    CHECK(hasDiagnostic(result, "cxc.budget.exceeded"));
}
