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

TEST_CASE("CXC manifest Reader rejects a non-object root and zero diagnostic budget",
          "[cxc][manifest][boundary]") {
    const auto nonObject = cuexis::cxc::CxcManifestLoader::load("[]");
    CHECK_FALSE(nonObject.hasValue());
    CHECK_FALSE(nonObject.diagnostics.empty());
    CHECK(nonObject.diagnostics.items().front().code() == "json.type.mismatch");

    auto limits = cuexis::cxc::CxcManifestLimits{};
    limits.maxDiagnostics = 0;
    const auto zeroBudget = cuexis::cxc::CxcManifestLoader::load("{}", limits);
    CHECK_FALSE(zeroBudget.hasValue());
    REQUIRE_FALSE(zeroBudget.diagnostics.empty());
    CHECK(zeroBudget.diagnostics.items().front().code() == "cxc.budget.exceeded");
}

TEST_CASE("CXC manifest Reader validates root fields and preserves no partial document",
          "[cxc][manifest][branch-coverage]") {
    constexpr std::string_view invalidRoot = R"({
      "format":"not.cxc","version":2,"project":"wrong.json",
      "entries":[
        {"path":"cuexis.project.json","byteCount":1,
         "sha256":"1111111111111111111111111111111111111111111111111111111111111111"}
      ],"requiredExtensions":[],"extensions":{},"unknown":true
    })";
    const auto rootResult = cuexis::cxc::CxcManifestLoader::load(invalidRoot);
    CHECK_FALSE(rootResult.hasValue());
    CHECK(hasDiagnostic(rootResult, "cxc.format.unsupported"));
    CHECK(hasDiagnostic(rootResult, "cxc.version.unsupported"));
    CHECK(hasDiagnostic(rootResult, "cxc.project.invalid"));
    CHECK(hasDiagnostic(rootResult, "json.field.unknown"));
    CHECK_FALSE(rootResult.document.has_value());

    constexpr std::string_view missingRequired =
        R"({"format":"cuexis.cxc","version":1,"project":"cuexis.project.json"})";
    const auto missingResult = cuexis::cxc::CxcManifestLoader::load(missingRequired);
    CHECK_FALSE(missingResult.hasValue());
    CHECK(hasDiagnostic(missingResult, "json.field.missing"));
    CHECK_FALSE(missingResult.document.has_value());

    constexpr std::string_view nonObjectExtensions = R"({
      "format":"cuexis.cxc","version":1,"project":"cuexis.project.json",
      "entries":[
        {"path":"cuexis.project.json","byteCount":1,
         "sha256":"1111111111111111111111111111111111111111111111111111111111111111"}
      ],"requiredExtensions":[],"extensions":[]
    })";
    const auto extensionsResult = cuexis::cxc::CxcManifestLoader::load(nonObjectExtensions);
    CHECK_FALSE(extensionsResult.hasValue());
    CHECK(hasDiagnostic(extensionsResult, "json.type.mismatch"));
    CHECK_FALSE(extensionsResult.document.has_value());
}

TEST_CASE("CXC manifest Reader validates required extension records and canonicalizes valid order",
          "[cxc][manifest][branch-coverage]") {
    constexpr std::string_view invalidExtensions = R"({
      "format":"cuexis.cxc","version":1,"project":"cuexis.project.json",
      "entries":[
        {"path":"cuexis.project.json","byteCount":1,
         "sha256":"1111111111111111111111111111111111111111111111111111111111111111"}
      ],
      "requiredExtensions":[
        {"id":"invalid id","version":1},
        {"id":"good","version":0},
        {"id":"tooLarge","version":4294967296},
        {"id":"duplicate","version":1},
        {"id":"duplicate","version":2},
        {"id":"unknown","version":1,"unexpected":true}
      ],"extensions":{}
    })";
    const auto invalidResult = cuexis::cxc::CxcManifestLoader::load(invalidExtensions);
    CHECK_FALSE(invalidResult.hasValue());
    CHECK(hasDiagnostic(invalidResult, "cxc.project.invalid"));
    CHECK(hasDiagnostic(invalidResult, "cxc.version.unsupported"));
    CHECK(hasDiagnostic(invalidResult, "cxc.entry.duplicate"));
    CHECK(hasDiagnostic(invalidResult, "json.field.unknown"));
    CHECK_FALSE(invalidResult.document.has_value());

    constexpr std::string_view validExtensions = R"({
      "format":"cuexis.cxc","version":1,"project":"cuexis.project.json",
      "entries":[
        {"path":"cuexis.project.json","byteCount":1,
         "sha256":"1111111111111111111111111111111111111111111111111111111111111111"}
      ],
      "requiredExtensions":[{"id":"zeta","version":2},{"id":"alpha","version":1}],
      "extensions":{"vendor":{"value":1}}
    })";
    const auto validResult = cuexis::cxc::CxcManifestLoader::load(validExtensions);
    REQUIRE(validResult.hasValue());
    REQUIRE(validResult.document.has_value());
    REQUIRE(validResult.document->requiredExtensions.size() == 2);
    CHECK(validResult.document->requiredExtensions[0].id == "alpha");
    CHECK(validResult.document->requiredExtensions[1].id == "zeta");
    CHECK(validResult.document->canonicalExtensionsJson == "{\"vendor\":{\"value\":1}}");

    auto extensionLimit = cuexis::cxc::CxcManifestLimits{};
    extensionLimit.maxExtensions = 1;
    const auto limitResult = cuexis::cxc::CxcManifestLoader::load(validExtensions, extensionLimit);
    CHECK_FALSE(limitResult.hasValue());
    CHECK(hasDiagnostic(limitResult, "cxc.budget.exceeded"));
    CHECK_FALSE(limitResult.document.has_value());
}

TEST_CASE("CXC manifest Reader validates entry records, budgets, and duplicate paths",
          "[cxc][manifest][branch-coverage]") {
    constexpr std::string_view malformedEntries = R"({
      "format":"cuexis.cxc","version":1,"project":"cuexis.project.json",
      "entries":[
        {"path":"/absolute.bin","byteCount":1,
         "sha256":"1111111111111111111111111111111111111111111111111111111111111111"},
        {"path":"bad-hash.bin","byteCount":2,
         "sha256":"UPPERCASE"},
        {"path":"valid.bin","byteCount":1,
         "sha256":"2222222222222222222222222222222222222222222222222222222222222222",
         "unknown":true},
        {"path":"valid.bin","byteCount":1,
         "sha256":"3333333333333333333333333333333333333333333333333333333333333333"},
        {"path":"cuexis.project.json","byteCount":1,
         "sha256":"4444444444444444444444444444444444444444444444444444444444444444"}
      ],"requiredExtensions":[],"extensions":{}
    })";
    const auto malformedResult = cuexis::cxc::CxcManifestLoader::load(malformedEntries);
    CHECK_FALSE(malformedResult.hasValue());
    CHECK(hasDiagnostic(malformedResult, "cxc.entry.path_invalid"));
    CHECK(hasDiagnostic(malformedResult, "cxc.entry.hash_mismatch"));
    CHECK(hasDiagnostic(malformedResult, "cxc.entry.duplicate"));
    CHECK(hasDiagnostic(malformedResult, "json.field.unknown"));
    CHECK_FALSE(malformedResult.document.has_value());

    constexpr std::string_view twoEntries = R"({
      "format":"cuexis.cxc","version":1,"project":"cuexis.project.json",
      "entries":[
        {"path":"asset.bin","byteCount":1,
         "sha256":"1111111111111111111111111111111111111111111111111111111111111111"},
        {"path":"cuexis.project.json","byteCount":1,
         "sha256":"2222222222222222222222222222222222222222222222222222222222222222"}
      ],"requiredExtensions":[],"extensions":{}
    })";
    auto entryLimit = cuexis::cxc::CxcManifestLimits{};
    entryLimit.maxEntries = 1;
    const auto entryLimitResult = cuexis::cxc::CxcManifestLoader::load(twoEntries, entryLimit);
    CHECK_FALSE(entryLimitResult.hasValue());
    CHECK(hasDiagnostic(entryLimitResult, "cxc.budget.exceeded"));

    auto byteLimit = cuexis::cxc::CxcManifestLimits{};
    byteLimit.maxEntryBytes = 0;
    const auto byteLimitResult = cuexis::cxc::CxcManifestLoader::load(twoEntries, byteLimit);
    CHECK_FALSE(byteLimitResult.hasValue());
    CHECK(hasDiagnostic(byteLimitResult, "cxc.budget.exceeded"));

    constexpr std::string_view emptyEntries = R"({
      "format":"cuexis.cxc","version":1,"project":"cuexis.project.json",
      "entries":[],"requiredExtensions":[],"extensions":{}
    })";
    const auto emptyResult = cuexis::cxc::CxcManifestLoader::load(emptyEntries);
    CHECK_FALSE(emptyResult.hasValue());
    CHECK(hasDiagnostic(emptyResult, "cxc.entry.missing"));
    CHECK_FALSE(emptyResult.document.has_value());
}

TEST_CASE("CXC manifest Reader preserves diagnostics across malformed nested values and limits",
          "[cxc][manifest][branch-coverage]") {
    SECTION("malformed JSON never publishes a document") {
        const auto result = cuexis::cxc::CxcManifestLoader::load("{\"format\":");
        CHECK_FALSE(result.hasValue());
        CHECK_FALSE(result.diagnostics.empty());
        CHECK_FALSE(result.document.has_value());
    }

    SECTION("nested array and object type errors are retained together") {
        constexpr std::string_view manifest = R"({
          "format":"cuexis.cxc","version":1,"project":"cuexis.project.json",
          "entries":[null,"not-an-entry"],
          "requiredExtensions":[null,"not-an-extension"],"extensions":{}
        })";
        const auto result = cuexis::cxc::CxcManifestLoader::load(manifest);
        CHECK_FALSE(result.hasValue());
        CHECK(hasDiagnostic(result, "json.type.mismatch"));
        CHECK_FALSE(result.document.has_value());
    }

    SECTION("extension member and JSON parser budgets reject before publication") {
        constexpr std::string_view manifest = R"({
          "format":"cuexis.cxc","version":1,"project":"cuexis.project.json",
          "entries":[{"path":"cuexis.project.json","byteCount":1,
          "sha256":"1111111111111111111111111111111111111111111111111111111111111111"}],
          "requiredExtensions":[],"extensions":{"one":{},"two":{}}
        })";
        auto memberLimit = cuexis::cxc::CxcManifestLimits{};
        memberLimit.maxExtensions = 1;
        const auto members = cuexis::cxc::CxcManifestLoader::load(manifest, memberLimit);
        CHECK_FALSE(members.hasValue());
        CHECK(hasDiagnostic(members, "cxc.budget.exceeded"));
        CHECK_FALSE(members.document.has_value());

        auto byteLimit = cuexis::cxc::CxcManifestLimits{};
        byteLimit.maxManifestBytes = manifest.size() - 1U;
        const auto bytes = cuexis::cxc::CxcManifestLoader::load(manifest, byteLimit);
        CHECK_FALSE(bytes.hasValue());
        CHECK_FALSE(bytes.diagnostics.empty());
        CHECK_FALSE(bytes.document.has_value());
    }
}

TEST_CASE("CXC manifest Reader enforces portable identifier and hash boundaries",
          "[cxc][manifest][branch-coverage]") {
    const auto longId = std::string(257U, 'a');
    const auto validId = std::string(256U, 'a');
    auto manifest = std::string{R"({
      "format":"cuexis.cxc","version":1,"project":"cuexis.project.json",
      "entries":[
        {"path":"cuexis.project.json","byteCount":0,
         "sha256":"1111111111111111111111111111111111111111111111111111111111111111"},
        {"path":"short.bin","byteCount":0,"sha256":"abc"},
        {"path":"upper.bin","byteCount":0,
         "sha256":"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"}
      ],
      "requiredExtensions":[
        {"id":"","version":1},{"id":"-invalid","version":1},
        {"id":")"};
    manifest += longId;
    manifest += R"(","version":1},{"id":")";
    manifest += validId;
    manifest += R"(","version":1}
      ],"extensions":{}
    })";
    const auto result = cuexis::cxc::CxcManifestLoader::load(manifest);
    CHECK_FALSE(result.hasValue());
    CHECK(hasDiagnostic(result, "cxc.project.invalid"));
    CHECK(hasDiagnostic(result, "cxc.entry.hash_mismatch"));
    CHECK_FALSE(result.document.has_value());
}
