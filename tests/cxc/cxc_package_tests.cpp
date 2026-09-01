#include "cxc_test_support.hpp"

#include <cuexis/content/content_provider.hpp>
#include <cuexis/cxc/cxc_package.hpp>
#include <cuexis/cxc/cxc_writer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using cuexis::cxc::CxcPackage;
using cuexis::cxc::CxcPackageLimits;
using cuexis::cxc::CxcPackageLoader;
using cuexis::cxc::CxcWriteEntry;
using cuexis::cxc::CxcWriter;
using cuexis::cxc::CxcWriteRequest;
namespace support = cuexis::cxc::test;

class TemporaryCxcFile final {
  public:
    explicit TemporaryCxcFile(std::span<const std::byte> bytes, std::string extension = ".cxc") {
        static std::atomic<unsigned int> next{1};
        path_ = std::filesystem::temp_directory_path() /
                ("cuexis-cxc-tests-" + std::to_string(next.fetch_add(1)) + extension);
        support::writeBytes(path_, bytes);
    }

    ~TemporaryCxcFile() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    TemporaryCxcFile(const TemporaryCxcFile&) = delete;
    auto operator=(const TemporaryCxcFile&) -> TemporaryCxcFile& = delete;

    [[nodiscard]] auto path() const noexcept -> const std::filesystem::path& {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

[[nodiscard]] auto findRequestEntry(CxcWriteRequest& request, std::string_view path)
    -> CxcWriteEntry& {
    const auto found = std::ranges::find(request.entries, path, &CxcWriteEntry::path);
    REQUIRE(found != request.entries.end());
    return *found;
}

void eraseRequestEntry(CxcWriteRequest& request, std::string_view path) {
    const auto found = std::ranges::find(request.entries, path, &CxcWriteEntry::path);
    REQUIRE(found != request.entries.end());
    request.entries.erase(found);
}

[[nodiscard]] auto contentRequest(std::string_view root, std::string_view source,
                                  std::size_t maxBytes = 1024U) -> cuexis::content::ContentRequest {
    return {.rootId = root, .source = source, .maxBytes = maxBytes};
}

[[nodiscard]] auto packageRequest(const CxcPackage& package) -> CxcWriteRequest {
    CxcWriteRequest request;
    request.requiredExtensions = package.manifest().requiredExtensions;
    request.extensionsJson = package.manifest().canonicalExtensionsJson;
    const auto bytes = package.bytes();
    const auto layout = support::parseZip(bytes);
    for (const auto& declared : package.manifest().entries) {
        const auto& entry = support::findEntry(layout, declared.path);
        const auto source = bytes.subspan(entry.dataOffset, entry.byteCount);
        request.entries.push_back(CxcWriteEntry{declared.path, {source.begin(), source.end()}});
    }
    return request;
}

[[nodiscard]] auto twoRootProject(std::string_view firstPath = "a",
                                  std::string_view secondPath = "b") -> std::string {
    std::string result = R"({
  "format":"cuexis.project",
  "version":1,
  "projectId":"019f0000-0000-7abc-8def-000000000c31",
  "assetRoots":[
    {"id":"first","path":")";
    result.append(firstPath);
    result += R"("},
    {"id":"second","path":")";
    result.append(secondPath);
    result += R"("}
  ],
  "entry":{"chart":{"root":"first","path":"charts/main.cuexis.chart.json"}},
  "extensions":{}
}
)";
    return result;
}

[[nodiscard]] auto twoRootRequest(std::string firstIndex, std::string secondIndex,
                                  std::string_view firstRoot = "a",
                                  std::string_view secondRoot = "b") -> CxcWriteRequest {
    CxcWriteRequest request;
    request.entries = {
        support::textEntry("cuexis.project.json", twoRootProject(firstRoot, secondRoot)),
        support::textEntry(std::string{firstRoot} + "/cuexis.asset-index.json",
                           std::move(firstIndex)),
        support::textEntry(std::string{secondRoot} + "/cuexis.asset-index.json",
                           std::move(secondIndex)),
        support::textEntry(std::string{firstRoot} + "/charts/main.cuexis.chart.json",
                           support::staticV4Chart()),
    };
    return request;
}

[[nodiscard]] auto emptyIndex(std::uint32_t version = 1) -> std::string {
    return "{\"format\":\"cuexis.asset-index\",\"version\":" + std::to_string(version) +
           ",\"assets\":[],\"extensions\":{}}";
}

[[nodiscard]] auto referenceV4Request() -> CxcWriteRequest {
    const auto root = support::sourceRoot() / "tests" / "fixtures" / "chart_format_update" /
                      "cfu_f_reference_project";
    CxcWriteRequest request;
    request.entries = {
        support::textEntry("cuexis.project.json", support::readText(root / "cuexis.project.json")),
        support::textEntry("assets/cuexis.asset-index.json",
                           support::readText(root / "assets" / "cuexis.asset-index.json")),
        support::textEntry(
            "assets/charts/main.cuexis.chart.json",
            support::readText(root / "assets" / "charts" / "main.cuexis.chart.json")),
        CxcWriteEntry{"assets/materials/blend.material.bin",
                      support::readBytes(root / "assets" / "materials" / "blend.material.bin")},
        CxcWriteEntry{"assets/materials/opaque.material.bin",
                      support::readBytes(root / "assets" / "materials" / "opaque.material.bin")},
        CxcWriteEntry{"assets/meshes/triangle.mesh.bin",
                      support::readBytes(root / "assets" / "meshes" / "triangle.mesh.bin")},
        CxcWriteEntry{"assets/textures/checker.texture.bin",
                      support::readBytes(root / "assets" / "textures" / "checker.texture.bin")},
    };
    return request;
}

} // namespace

TEST_CASE("CXC Writer emits deterministic canonical ZIP32 and accepts safe noncanonical metadata",
          "[cxc][writer][zip32][cfu-c3]") {
    auto firstRequest = support::makeV4CxtRequest();
    firstRequest.requiredExtensions = {{"org.example.z", 2}, {"org.example.a", 1}};
    auto secondRequest = firstRequest;
    std::ranges::reverse(secondRequest.entries);
    std::ranges::reverse(secondRequest.requiredExtensions);

    const auto first = support::writePackage(std::move(firstRequest));
    const auto second = support::writePackage(std::move(secondRequest));
    REQUIRE(first == second);

    const auto layout = support::parseZip(first);
    REQUIRE(layout.entries.size() == 6);
    CHECK(layout.entries.front().path == "cuexis.cxc.json");
    std::vector<std::string> contentPaths;
    for (std::size_t index = 1; index < layout.entries.size(); ++index) {
        contentPaths.push_back(layout.entries[index].path);
    }
    CHECK(std::ranges::is_sorted(contentPaths));

    for (const auto& entry : layout.entries) {
        CHECK(support::readU16(first, entry.localHeaderOffset + 4U) == 10U);
        CHECK(support::readU16(first, entry.localHeaderOffset + 6U) == 0U);
        CHECK(support::readU16(first, entry.localHeaderOffset + 8U) == 0U);
        CHECK(support::readU16(first, entry.localHeaderOffset + 10U) == 0U);
        CHECK(support::readU16(first, entry.localHeaderOffset + 12U) == 0x0021U);
        CHECK(support::readU16(first, entry.localHeaderOffset + 28U) == 0U);
        CHECK(support::readU16(first, entry.centralHeaderOffset + 4U) == 0x000AU);
        CHECK(support::readU16(first, entry.centralHeaderOffset + 6U) == 10U);
        CHECK(support::readU16(first, entry.centralHeaderOffset + 8U) == 0U);
        CHECK(support::readU16(first, entry.centralHeaderOffset + 10U) == 0U);
        CHECK(support::readU16(first, entry.centralHeaderOffset + 12U) == 0U);
        CHECK(support::readU16(first, entry.centralHeaderOffset + 14U) == 0x0021U);
        CHECK(support::readU16(first, entry.centralHeaderOffset + 30U) == 0U);
        CHECK(support::readU16(first, entry.centralHeaderOffset + 32U) == 0U);
        CHECK(support::readU16(first, entry.centralHeaderOffset + 34U) == 0U);
        CHECK(support::readU16(first, entry.centralHeaderOffset + 36U) == 0U);
        CHECK(support::readU32(first, entry.centralHeaderOffset + 38U) == 0U);
    }

    auto noncanonical = first;
    const auto& changed = layout.entries[1];
    support::writeU16(noncanonical, changed.localHeaderOffset + 4U, 20U);
    support::writeU16(noncanonical, changed.localHeaderOffset + 10U, 0x1234U);
    support::writeU16(noncanonical, changed.localHeaderOffset + 12U, 0x4A21U);
    support::writeU16(noncanonical, changed.centralHeaderOffset + 4U, 0x0014U);
    support::writeU16(noncanonical, changed.centralHeaderOffset + 6U, 20U);
    support::writeU16(noncanonical, changed.centralHeaderOffset + 12U, 0x1234U);
    support::writeU16(noncanonical, changed.centralHeaderOffset + 14U, 0x4A21U);

    auto loaded = CxcPackageLoader::loadMemory(noncanonical);
    INFO(support::diagnosticsText(loaded.diagnostics));
    REQUIRE(loaded.hasValue());
    CHECK(loaded.package->identity().hex() !=
          CxcPackageLoader::loadMemory(first).package->identity().hex());
    const auto repacked = support::writePackage(packageRequest(*loaded.package));
    CHECK(repacked == first);
}

TEST_CASE("CXC file and memory packages own equivalent bytes and keep content domains separate",
          "[cxc][package][content][cfu-c3]") {
    const auto canonical = support::writePackage(support::makeV4CxtRequest());
    auto callerBytes = canonical;
    auto memory = CxcPackageLoader::loadMemory(
        std::span<const std::byte>{callerBytes.data(), callerBytes.size()});
    INFO(support::diagnosticsText(memory.diagnostics));
    REQUIRE(memory.hasValue());

    std::ranges::fill(callerBytes, std::byte{0});
    callerBytes.clear();
    callerBytes.shrink_to_fit();
    CHECK(std::ranges::equal(memory.package->bytes(), canonical));

    REQUIRE(memory.package->assetIndexes().size() == 1);
    REQUIRE(memory.package->assetIndexes()[0].document.assets.size() == 1);
    CHECK(memory.package->assetIndexes()[0].document.assets[0].id == "texture.unused");
    REQUIRE(memory.package->projectDocuments().size() == 2);
    CHECK(memory.package->projectDocuments()[0].path == "assets/charts/main.cuexis.chart.json");
    CHECK(memory.package->projectDocuments()[1].path == "templates/move-y.cxt");

    const auto provider = memory.package->contentProvider();
    const auto asset = provider->readBlob(contentRequest("main", "textures/unused.bin"));
    REQUIRE(asset.has_value());
    CHECK(support::textFromBytes(asset->span()) == "unused-resource\n");
    CHECK(asset->revision != 0);

    const auto chart =
        provider->readBlob(contentRequest("main", "charts/main.cuexis.chart.json", 1024U * 1024U));
    REQUIRE_FALSE(chart.has_value());
    CHECK(chart.error().code() == "content.cxc.source_not_found");
    const auto cxt = provider->readBlob(contentRequest("main", "templates/move-y.cxt"));
    REQUIRE_FALSE(cxt.has_value());
    CHECK(cxt.error().code() == "content.cxc.source_not_found");

    TemporaryCxcFile file{canonical};
    auto loadedFile = CxcPackageLoader::loadFile(file.path());
    INFO(support::diagnosticsText(loadedFile.diagnostics));
    REQUIRE(loadedFile.hasValue());
    CHECK(loadedFile.package->identity() == memory.package->identity());
    CHECK(loadedFile.package->project() == memory.package->project());
    CHECK(std::ranges::equal(loadedFile.package->entries(), memory.package->entries()));
    CHECK(loadedFile.package->manifest().canonicalSourceJson ==
          memory.package->manifest().canonicalSourceJson);

    TemporaryCxcFile zipFile{canonical, ".zip"};
    const auto rejectedLocator = CxcPackageLoader::loadFile(zipFile.path());
    CHECK_FALSE(rejectedLocator.hasValue());
    CHECK(support::hasDiagnostic(rejectedLocator.diagnostics, "cxc.archive.invalid"));
}

TEST_CASE("CXC packages preserve Chart v1 through v4 and all declared Asset Index records",
          "[cxc][package][versions][cfu-c3]") {
    struct Case final {
        CxcWriteRequest request;
        std::size_t expectedDocuments{};
        std::string assetSource;
    };
    std::vector<Case> cases;
    cases.push_back({support::makeV1Request(), 1, "meshes/note.mesh.bin"});
    cases.push_back({support::makeV2Request(), 1, "audio/main.wav"});
    cases.push_back({support::makeV3Request(), 1, {}});
    cases.push_back({support::makeV4CxtRequest(), 2, "textures/unused.bin"});

    for (auto& item : cases) {
        const auto bytes = support::writePackage(std::move(item.request));
        auto loaded = CxcPackageLoader::loadMemory(bytes);
        INFO(support::diagnosticsText(loaded.diagnostics));
        REQUIRE(loaded.hasValue());
        CHECK(loaded.package->projectDocuments().size() == item.expectedDocuments);
        CHECK_FALSE(loaded.package->assetIndexes().empty());
        if (!item.assetSource.empty()) {
            const auto blob = loaded.package->contentProvider()->readBlob(
                contentRequest("main", item.assetSource, 64U * 1024U * 1024U));
            REQUIRE(blob.has_value());
            CHECK_FALSE(blob->bytes.empty());
        }
    }
}

TEST_CASE("CXC Writer rejects hidden payload and incomplete project closure",
          "[cxc][closure][cfu-c3]") {
    SECTION("hidden payload") {
        auto request = support::makeV4CxtRequest();
        request.entries.push_back(support::binaryEntry("private/hidden.bin", "hidden"));
        const auto result = CxcWriter::write(std::move(request));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.entry.unlisted"));
    }

    SECTION("missing declared source") {
        auto request = support::makeV1Request();
        eraseRequestEntry(request, "assets/textures/white.texture.bin");
        const auto result = CxcWriter::write(std::move(request));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.entry.missing"));
    }

    SECTION("missing dependency") {
        auto request = support::makeV4CxtRequest();
        auto& index = findRequestEntry(request, "assets/cuexis.asset-index.json");
        index = support::textEntry(
            index.path,
            R"({"format":"cuexis.asset-index","version":1,"assets":[{"id":"texture.unused","type":"texture","source":"textures/unused.bin","dependencies":["texture.missing"]}],"extensions":{}})");
        const auto result = CxcWriter::write(std::move(request));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.project.invalid"));
    }

    SECTION("record-level Asset Index extensions are rejected") {
        auto request = support::makeV1Request();
        auto& index = findRequestEntry(request, "assets/cuexis.asset-index.json");
        auto indexText = support::textFromBytes(index.bytes);
        const auto marker = std::string_view{"\"dependencies\": []"};
        const auto markerPosition = indexText.find(marker);
        REQUIRE(markerPosition != std::string::npos);
        indexText.replace(markerPosition, marker.size(),
                          R"("dependencies": [],"extensions":{"org.record":{"enabled":true}})");
        index = support::textEntry(index.path, std::move(indexText));

        const auto result = CxcWriter::write(std::move(request));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "json.field.unknown"));
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.project.invalid"));
    }

    SECTION("missing CXT") {
        auto request = support::makeV4CxtRequest();
        eraseRequestEntry(request, "templates/move-y.cxt");
        const auto result = CxcWriter::write(std::move(request));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.entry.missing"));
    }
}

TEST_CASE("CXC validates dependency rules across Asset Index files with a bounded traversal",
          "[cxc][closure][dependencies][cfu-c3]") {
    SECTION("cross-index cycle") {
        auto request = twoRootRequest(
            R"({"format":"cuexis.asset-index","version":1,"assets":[{"id":"mesh.a","type":"mesh","source":"a.bin","dependencies":["material.b"]}],"extensions":{}})",
            R"({"format":"cuexis.asset-index","version":1,"assets":[{"id":"material.b","type":"material","source":"b.bin","dependencies":["mesh.a"]}],"extensions":{}})");
        request.entries.push_back(support::binaryEntry("a/a.bin", "a"));
        request.entries.push_back(support::binaryEntry("b/b.bin", "b"));
        const auto result = CxcWriter::write(std::move(request));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.project.invalid"));
    }

    SECTION("cross-index Audio dependency") {
        auto request = twoRootRequest(
            R"({"format":"cuexis.asset-index","version":2,"assets":[{"id":"mesh.a","type":"mesh","source":"a.bin","dependencies":["audio.b"]}],"extensions":{}})",
            R"({"format":"cuexis.asset-index","version":2,"assets":[{"id":"audio.b","type":"audio","source":"b.wav","dependencies":[]}],"extensions":{}})");
        request.entries.push_back(support::binaryEntry("a/a.bin", "a"));
        request.entries.push_back(support::binaryEntry("b/b.wav", "b"));
        const auto result = CxcWriter::write(std::move(request));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.project.invalid"));
    }

    SECTION("dependency depth") {
        auto request = support::makeV4CxtRequest();
        auto& index = findRequestEntry(request, "assets/cuexis.asset-index.json");
        index = support::textEntry(
            index.path,
            R"({"format":"cuexis.asset-index","version":1,"assets":[{"id":"mesh.a","type":"mesh","source":"a.bin","dependencies":["material.b"]},{"id":"material.b","type":"material","source":"b.bin","dependencies":["texture.c"]},{"id":"texture.c","type":"texture","source":"c.bin","dependencies":[]}],"extensions":{}})");
        eraseRequestEntry(request, "assets/textures/unused.bin");
        request.entries.push_back(support::binaryEntry("assets/a.bin", "a"));
        request.entries.push_back(support::binaryEntry("assets/b.bin", "b"));
        request.entries.push_back(support::binaryEntry("assets/c.bin", "c"));
        auto limits = CxcPackageLimits{};
        limits.maxDependencyDepth = 2;
        const auto result = CxcWriter::write(std::move(request), limits);
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.budget.exceeded"));
    }

    SECTION("aggregate Asset count across roots") {
        auto request = twoRootRequest(
            R"({"format":"cuexis.asset-index","version":1,"assets":[{"id":"mesh.a","type":"mesh","source":"a.bin","dependencies":[]}],"extensions":{}})",
            R"({"format":"cuexis.asset-index","version":1,"assets":[{"id":"mesh.b","type":"mesh","source":"b.bin","dependencies":[]}],"extensions":{}})");
        request.entries.push_back(support::binaryEntry("a/a.bin", "a"));
        request.entries.push_back(support::binaryEntry("b/b.bin", "b"));
        auto limits = CxcPackageLimits{};
        limits.assetIndex.maxAssets = 1;
        const auto result = CxcWriter::write(std::move(request), limits);
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.budget.exceeded"));
    }
}

TEST_CASE("CXC rejects root overlaps and project-document content aliases",
          "[cxc][closure][paths][cfu-c3]") {
    SECTION("archive path prefix conflict") {
        auto request = support::makeV4CxtRequest();
        request.entries.push_back(support::binaryEntry("assets", "conflict"));
        const auto result = CxcWriter::write(std::move(request));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.entry.duplicate"));
    }

    SECTION("ASCII case-folded root overlap") {
        auto request = twoRootRequest(emptyIndex(), emptyIndex(), "Assets", "assets/sub");
        const auto result = CxcWriter::write(std::move(request));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.project.invalid"));
    }

    SECTION("Asset source aliases its Asset Index") {
        auto request = support::makeV4CxtRequest();
        auto& index = findRequestEntry(request, "assets/cuexis.asset-index.json");
        index = support::textEntry(
            index.path,
            R"({"format":"cuexis.asset-index","version":1,"assets":[{"id":"texture.alias","type":"texture","source":"cuexis.asset-index.json","dependencies":[]}],"extensions":{}})");
        eraseRequestEntry(request, "assets/textures/unused.bin");
        const auto result = CxcWriter::write(std::move(request));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.project.invalid"));
    }

    SECTION("entry Chart also declared as asset source") {
        auto request = support::makeV4CxtRequest();
        auto& index = findRequestEntry(request, "assets/cuexis.asset-index.json");
        index = support::textEntry(
            index.path,
            R"({"format":"cuexis.asset-index","version":1,"assets":[{"id":"texture.alias","type":"texture","source":"charts/main.cuexis.chart.json","dependencies":[]}],"extensions":{}})");
        eraseRequestEntry(request, "assets/textures/unused.bin");
        const auto result = CxcWriter::write(std::move(request));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.project.invalid"));
    }

    SECTION("CXT also declared as asset source") {
        auto request = support::makeV4CxtRequest();
        auto& chart = findRequestEntry(request, "assets/charts/main.cuexis.chart.json");
        auto chartText = support::textFromBytes(chart.bytes);
        const auto source = chartText.find("templates/move-y.cxt");
        REQUIRE(source != std::string::npos);
        chartText.replace(source, std::string_view{"templates/move-y.cxt"}.size(),
                          "assets/templates/move-y.cxt");
        chart.bytes = support::bytesFromText(chartText);
        auto& index = findRequestEntry(request, "assets/cuexis.asset-index.json");
        index = support::textEntry(
            index.path,
            R"({"format":"cuexis.asset-index","version":1,"assets":[{"id":"texture.alias","type":"texture","source":"templates/move-y.cxt","dependencies":[]}],"extensions":{}})");
        eraseRequestEntry(request, "assets/textures/unused.bin");
        auto& cxt = findRequestEntry(request, "templates/move-y.cxt");
        cxt.path = "assets/templates/move-y.cxt";
        const auto result = CxcWriter::write(std::move(request));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.project.invalid"));
    }
}

TEST_CASE("CXC detects CRC manifest hash size and package budget mismatches",
          "[cxc][integrity][limits][cfu-c3]") {
    const auto valid = support::writePackage(support::makeV4CxtRequest());

    SECTION("CRC mismatch") {
        auto corrupted = valid;
        const auto layout = support::parseZip(corrupted);
        const auto& entry = support::findEntry(layout, "assets/textures/unused.bin");
        corrupted[entry.dataOffset] ^= std::byte{0x01};
        const auto result = CxcPackageLoader::loadMemory(corrupted);
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.archive.invalid"));
    }

    SECTION("manifest hash mismatch") {
        auto corrupted = valid;
        const auto loaded = CxcPackageLoader::loadMemory(valid);
        REQUIRE(loaded.hasValue());
        const auto found =
            std::ranges::find(loaded.package->manifest().entries, "assets/textures/unused.bin",
                              &cuexis::cxc::CxcManifestEntry::path);
        REQUIRE(found != loaded.package->manifest().entries.end());
        auto changedHash = found->sha256;
        changedHash[0] = changedHash[0] == '0' ? '1' : '0';
        support::replaceEntryText(corrupted, "cuexis.cxc.json", found->sha256, changedHash);
        const auto result = CxcPackageLoader::loadMemory(corrupted);
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.entry.hash_mismatch"));
    }

    SECTION("manifest size mismatch") {
        auto corrupted = valid;
        const auto loaded = CxcPackageLoader::loadMemory(valid);
        REQUIRE(loaded.hasValue());
        const auto found =
            std::ranges::find(loaded.package->manifest().entries, "assets/textures/unused.bin",
                              &cuexis::cxc::CxcManifestEntry::path);
        REQUIRE(found != loaded.package->manifest().entries.end());
        const auto oldValue = std::string{"\"byteCount\": "} + std::to_string(found->byteCount);
        const auto newValue =
            std::string{"\"byteCount\": "} + std::to_string(found->byteCount - 1U);
        support::replaceEntryText(corrupted, "cuexis.cxc.json", oldValue, newValue);
        const auto result = CxcPackageLoader::loadMemory(corrupted);
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.entry.size_mismatch"));
    }

    SECTION("package budget before span copy") {
        auto limits = CxcPackageLimits{};
        limits.maxPackageBytes = valid.size() - 1U;
        const auto result = CxcPackageLoader::loadMemory(
            std::span<const std::byte>{valid.data(), valid.size()}, limits);
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.budget.exceeded"));
    }
}

TEST_CASE("CXC failed package loads do not publish partial package state",
          "[cxc][integrity][rollback][branch-coverage]") {
    const auto valid = support::writePackage(support::makeV4StaticRequest());
    auto corrupted = valid;
    const auto loaded = CxcPackageLoader::loadMemory(valid);
    REQUIRE(loaded.hasValue());
    const auto found = std::ranges::find(loaded.package->manifest().entries,
                                         "assets/charts/main.cuexis.chart.json",
                                         &cuexis::cxc::CxcManifestEntry::path);
    REQUIRE(found != loaded.package->manifest().entries.end());
    auto changedHash = found->sha256;
    changedHash[0] = changedHash[0] == '0' ? '1' : '0';
    support::replaceEntryText(corrupted, "cuexis.cxc.json", found->sha256, changedHash);

    const auto rejected = CxcPackageLoader::loadMemory(std::move(corrupted));
    REQUIRE_FALSE(rejected.hasValue());
    CHECK_FALSE(rejected.package.has_value());
    CHECK(support::hasDiagnostic(rejected.diagnostics, "cxc.entry.hash_mismatch"));
}

TEST_CASE("CXC content provider validates requests and package entry lookup",
          "[cxc][content][branch-coverage]") {
    const auto bytes = support::writePackage(support::makeV4CxtRequest());
    const auto loaded = CxcPackageLoader::loadMemory(bytes);
    REQUIRE(loaded.hasValue());

    CHECK_FALSE(loaded.package->entryBytes("missing.bin").has_value());
    const auto manifestBytes = loaded.package->entryBytes("cuexis.cxc.json");
    REQUIRE(manifestBytes.has_value());
    CHECK_FALSE(manifestBytes->empty());

    const auto provider = loaded.package->contentProvider();
    const auto invalidRoot = provider->readBlob(contentRequest("", "textures/unused.bin"));
    CHECK_FALSE(invalidRoot.has_value());
    CHECK(invalidRoot.error().code() == "content.cxc.request_invalid");

    const auto invalidSource = provider->readBlob(contentRequest("main", ""));
    CHECK_FALSE(invalidSource.has_value());
    CHECK(invalidSource.error().code() == "content.cxc.request_invalid");

    auto zeroLimit = contentRequest("main", "textures/unused.bin");
    zeroLimit.maxBytes = 0;
    const auto invalidLimit = provider->readBlob(zeroLimit);
    CHECK_FALSE(invalidLimit.has_value());
    CHECK(invalidLimit.error().code() == "content.cxc.limit_invalid");

    const auto invalidPath = provider->readBlob(contentRequest("main", "../unused.bin"));
    CHECK_FALSE(invalidPath.has_value());
    CHECK(invalidPath.error().code() == "content.cxc.source_invalid");

    const auto missing = provider->readBlob(contentRequest("main", "textures/missing.bin"));
    CHECK_FALSE(missing.has_value());
    CHECK(missing.error().code() == "content.cxc.source_not_found");

    const auto tooLarge = provider->readBlob(contentRequest("main", "textures/unused.bin", 1));
    CHECK_FALSE(tooLarge.has_value());
    CHECK(tooLarge.error().code() == "content.provider.too_large");

    const auto valid = provider->readBlob(contentRequest("main", "textures/unused.bin"));
    REQUIRE(valid.has_value());
    CHECK_FALSE(valid->bytes.empty());
    CHECK(valid->revision != 0);
}

TEST_CASE("CXC package loader rejects missing manifest and bounded manifest",
          "[cxc][package][branch-coverage]") {
    const auto noManifest = support::writeRawZip({support::binaryEntry("payload.bin", "payload")});
    const auto missing = CxcPackageLoader::loadMemory(noManifest);
    CHECK_FALSE(missing.hasValue());
    CHECK(support::hasDiagnostic(missing.diagnostics, "cxc.entry.missing"));
    CHECK_FALSE(missing.package.has_value());

    const auto valid = support::writePackage(support::makeV4StaticRequest());
    const auto layout = support::parseZip(valid);
    const auto& manifest = support::findEntry(layout, "cuexis.cxc.json");
    auto limits = CxcPackageLimits{};
    limits.maxManifestBytes = manifest.byteCount - 1U;
    const auto oversized = CxcPackageLoader::loadMemory(valid, limits);
    CHECK_FALSE(oversized.hasValue());
    CHECK(support::hasDiagnostic(oversized.diagnostics, "cxc.budget.exceeded"));
    CHECK_FALSE(oversized.package.has_value());
}

TEST_CASE("CXC package loader reports project closure failures without publishing a package",
          "[cxc][package][closure][branch-coverage]") {
    SECTION("invalid ProjectConfig") {
        const auto archive =
            support::writeUncheckedPackage({support::textEntry("cuexis.project.json", "{}")});
        const auto result = CxcPackageLoader::loadMemory(archive);
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.project.invalid"));
        CHECK_FALSE(result.package.has_value());
    }

    SECTION("missing Asset Index") {
        const auto archive = support::writeUncheckedPackage(
            {support::textEntry("cuexis.project.json",
                                support::readText(support::sourceRoot() / "tests" / "fixtures" /
                                                  "chart_format_update" / "static_project" /
                                                  "cuexis.project.json")),
             support::textEntry("assets/charts/main.cuexis.chart.json", support::staticV4Chart())});
        const auto result = CxcPackageLoader::loadMemory(archive);
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.entry.missing"));
        CHECK_FALSE(result.package.has_value());
    }

    SECTION("missing entry Chart") {
        const auto archive = support::writeUncheckedPackage(
            {support::textEntry("cuexis.project.json",
                                support::readText(support::sourceRoot() / "tests" / "fixtures" /
                                                  "chart_format_update" / "static_project" /
                                                  "cuexis.project.json")),
             support::textEntry("assets/cuexis.asset-index.json", emptyIndex())});
        const auto result = CxcPackageLoader::loadMemory(archive);
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.entry.missing"));
        CHECK_FALSE(result.package.has_value());
    }
}

TEST_CASE("CXC package loader retains archive and asset-closure boundaries",
          "[cxc][package][closure][branch-coverage]") {
    SECTION("manifest and archive must declare the same content") {
        auto bytes = support::writePackage(support::makeV4StaticRequest());
        support::replaceEntryText(bytes, "cuexis.cxc.json", "assets/charts/main.cuexis.chart.json",
                                  "assets/charts/gone.cuexis.chart.json");
        const auto result = CxcPackageLoader::loadMemory(std::move(bytes));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.entry.missing"));
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.entry.unlisted"));
        CHECK_FALSE(result.package.has_value());
    }

    SECTION("Asset Index records require a physical source entry") {
        auto request = support::makeV4StaticRequest();
        auto& index = findRequestEntry(request, "assets/cuexis.asset-index.json");
        index = support::textEntry(
            index.path,
            R"({"format":"cuexis.asset-index","version":1,"assets":[{"id":"texture.missing","type":"texture","source":"textures/absent.bin","dependencies":[]}],"extensions":{}})");
        const auto result = CxcWriter::write(std::move(request));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.entry.missing"));
        CHECK_FALSE(result.bytes.has_value());
    }

    SECTION("Asset IDs are unique across independent roots") {
        auto request = twoRootRequest(
            R"({"format":"cuexis.asset-index","version":1,"assets":[{"id":"texture.shared","type":"texture","source":"a.bin","dependencies":[]}],"extensions":{}})",
            R"({"format":"cuexis.asset-index","version":1,"assets":[{"id":"texture.shared","type":"texture","source":"b.bin","dependencies":[]}],"extensions":{}})");
        request.entries.push_back(support::binaryEntry("a/a.bin", "a"));
        request.entries.push_back(support::binaryEntry("b/b.bin", "b"));
        const auto result = CxcWriter::write(std::move(request));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.project.invalid"));
        CHECK_FALSE(result.bytes.has_value());
    }
}

TEST_CASE("CXC package validates legacy resource references and CXT project documents",
          "[cxc][package][closure][branch-coverage]") {
    SECTION("legacy Chart resource IDs must resolve through the Asset Index") {
        auto request = support::makeV1Request();
        auto& index = findRequestEntry(request, "assets/cuexis.asset-index.json");
        auto text = support::textFromBytes(index.bytes);
        const auto found = text.find("material.basic");
        REQUIRE(found != std::string::npos);
        text.replace(found, std::string_view{"material.basic"}.size(), "material.missing");
        index = support::textEntry(index.path, std::move(text));

        const auto result = CxcWriter::write(std::move(request));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.project.invalid"));
        CHECK_FALSE(result.bytes.has_value());
    }

    SECTION("legacy Chart resource types must match the declared Asset Index type") {
        auto request = support::makeV1Request();
        auto& index = findRequestEntry(request, "assets/cuexis.asset-index.json");
        auto text = support::textFromBytes(index.bytes);
        const auto found = text.find("\"type\": \"material\"");
        REQUIRE(found != std::string::npos);
        text.replace(found, std::string_view{"\"type\": \"material\""}.size(),
                     "\"type\": \"texture\"");
        index = support::textEntry(index.path, std::move(text));

        const auto result = CxcWriter::write(std::move(request));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.project.invalid"));
        CHECK_FALSE(result.bytes.has_value());
    }

    SECTION("invalid CXT source does not leave a partial package") {
        auto request = support::makeV4CxtRequest();
        auto& cxt = findRequestEntry(request, "templates/move-y.cxt");
        cxt = support::textEntry(cxt.path, "{}");
        const auto result = CxcWriter::write(std::move(request));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.project.invalid"));
        CHECK_FALSE(result.bytes.has_value());
    }

    SECTION("CXT template ID must agree with the Chart import") {
        auto request = support::makeV4CxtRequest();
        auto& cxt = findRequestEntry(request, "templates/move-y.cxt");
        auto text = support::textFromBytes(cxt.bytes);
        const auto found = text.find("motion.move-y");
        REQUIRE(found != std::string::npos);
        text.replace(found, std::string_view{"motion.move-y"}.size(), "motion.move-x");
        cxt = support::textEntry(cxt.path, std::move(text));

        const auto result = CxcWriter::write(std::move(request));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxt.template.id_mismatch"));
        CHECK_FALSE(result.bytes.has_value());
    }
}

TEST_CASE("CXC Writer rejects invalid requests before producing archive bytes",
          "[cxc][writer][branch-coverage]") {
    SECTION("empty content entries") {
        const auto result = CxcWriter::write(CxcWriteRequest{});
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.budget.exceeded"));
    }

    SECTION("invalid limits") {
        auto limits = CxcPackageLimits{};
        limits.maxEntries = 1;
        const auto result = CxcWriter::write(support::makeV4StaticRequest(), limits);
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.budget.exceeded"));
    }

    SECTION("invalid extension JSON") {
        auto request = support::makeV4StaticRequest();
        request.extensionsJson = "[]";
        const auto result = CxcWriter::write(std::move(request));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.project.invalid"));
    }

    SECTION("malformed extension JSON") {
        auto request = support::makeV4StaticRequest();
        request.extensionsJson = "{";
        const auto result = CxcWriter::write(std::move(request));
        CHECK_FALSE(result.hasValue());
        CHECK_FALSE(result.diagnostics.empty());
    }

    SECTION("missing ProjectConfig and duplicate entries") {
        auto noProject = support::makeV4StaticRequest();
        eraseRequestEntry(noProject, "cuexis.project.json");
        const auto projectResult = CxcWriter::write(std::move(noProject));
        CHECK_FALSE(projectResult.hasValue());
        CHECK(support::hasDiagnostic(projectResult.diagnostics, "cxc.entry.missing"));
        CHECK_FALSE(projectResult.bytes.has_value());

        auto duplicate = support::makeV4StaticRequest();
        duplicate.entries.push_back(duplicate.entries.back());
        const auto duplicateResult = CxcWriter::write(std::move(duplicate));
        CHECK_FALSE(duplicateResult.hasValue());
        CHECK(support::hasDiagnostic(duplicateResult.diagnostics, "cxc.entry.duplicate"));
        CHECK_FALSE(duplicateResult.bytes.has_value());
    }

    SECTION("path, entry, and aggregate byte limits prevent archive publication") {
        auto invalidPath = support::makeV4StaticRequest();
        invalidPath.entries.push_back(support::binaryEntry("assets/../escape.bin", "x"));
        const auto pathResult = CxcWriter::write(std::move(invalidPath));
        CHECK_FALSE(pathResult.hasValue());
        CHECK(support::hasDiagnostic(pathResult.diagnostics, "cxc.entry.path_invalid"));
        CHECK_FALSE(pathResult.bytes.has_value());

        auto entryLimit = support::makeV4StaticRequest();
        auto limits = CxcPackageLimits{};
        limits.maxEntryBytes = 1;
        const auto perEntry = CxcWriter::write(std::move(entryLimit), limits);
        CHECK_FALSE(perEntry.hasValue());
        CHECK(support::hasDiagnostic(perEntry.diagnostics, "cxc.budget.exceeded"));
        CHECK_FALSE(perEntry.bytes.has_value());

        auto aggregateLimit = support::makeV4StaticRequest();
        limits = CxcPackageLimits{};
        limits.manifest.maxListedBytes = 1;
        const auto aggregate = CxcWriter::write(std::move(aggregateLimit), limits);
        CHECK_FALSE(aggregate.hasValue());
        CHECK(support::hasDiagnostic(aggregate.diagnostics, "cxc.budget.exceeded"));
        CHECK_FALSE(aggregate.bytes.has_value());
    }
}

TEST_CASE("CXC validates resource closure for V4 legacy projection and animation clips",
          "[cxc][package][closure][branch-coverage]") {
    SECTION("V4 behavior material references remain valid when the Asset Index agrees") {
        const auto bytes = support::writePackage(referenceV4Request());
        const auto loaded = CxcPackageLoader::loadMemory(bytes);
        REQUIRE(loaded.hasValue());
        CHECK(loaded.package->projectDocuments().size() == 1U);
    }

    SECTION("V4 behavior material references must resolve to Material assets") {
        auto missingRequest = referenceV4Request();
        auto& missingChart =
            findRequestEntry(missingRequest, "assets/charts/main.cuexis.chart.json");
        auto missingText = support::textFromBytes(missingChart.bytes);
        const auto materialId = missingText.find("material.blend");
        REQUIRE(materialId != std::string::npos);
        missingText.replace(materialId, std::string_view{"material.blend"}.size(),
                            "material.missing");
        missingChart = support::textEntry(missingChart.path, std::move(missingText));

        const auto missing = CxcWriter::write(std::move(missingRequest));
        CHECK_FALSE(missing.hasValue());
        CHECK(support::hasDiagnostic(missing.diagnostics, "cxc.project.invalid"));
        CHECK_FALSE(missing.bytes.has_value());

        auto typeRequest = referenceV4Request();
        auto& index = findRequestEntry(typeRequest, "assets/cuexis.asset-index.json");
        auto indexText = support::textFromBytes(index.bytes);
        const auto blendId = indexText.find("\"id\": \"material.blend\"");
        REQUIRE(blendId != std::string::npos);
        const auto materialType = indexText.find("\"type\": \"material\"", blendId);
        REQUIRE(materialType != std::string::npos);
        indexText.replace(materialType, std::string_view{"\"type\": \"material\""}.size(),
                          "\"type\": \"texture\"");
        index = support::textEntry(index.path, std::move(indexText));

        const auto incompatible = CxcWriter::write(std::move(typeRequest));
        CHECK_FALSE(incompatible.hasValue());
        CHECK(support::hasDiagnostic(incompatible.diagnostics, "cxc.project.invalid"));
        CHECK_FALSE(incompatible.bytes.has_value());
    }

    SECTION("V4 animation step tracks validate Material resources before package publication") {
        auto request = referenceV4Request();
        auto& chart = findRequestEntry(request, "assets/charts/main.cuexis.chart.json");
        auto chartText = support::textFromBytes(chart.bytes);
        const auto clips = chartText.find("\"animationClips\": []");
        REQUIRE(clips != std::string::npos);
        chartText.replace(clips, std::string_view{"\"animationClips\": []"}.size(),
                          R"("animationClips": [
    {
      "id": "animation.material-swap",
      "version": 1,
      "durationBeats": { "numerator": 1, "denominator": 1 },
      "tracks": [],
      "stepTracks": [
        {
          "property": "render.material",
          "steps": [
            {
              "beat": { "numerator": 0, "denominator": 1 },
              "value": { "domain": "asset", "id": "material.blend" }
            }
          ]
        }
      ]
    }
  ])");
        chart = support::textEntry(chart.path, chartText);

        const auto valid = CxcWriter::write(request);
        REQUIRE(valid.hasValue());
        REQUIRE(valid.bytes.has_value());

        const auto swapId = chartText.rfind("material.blend");
        REQUIRE(swapId != std::string::npos);
        chartText.replace(swapId, std::string_view{"material.blend"}.size(), "material.missing");
        chart = support::textEntry(chart.path, std::move(chartText));
        const auto missing = CxcWriter::write(std::move(request));
        CHECK_FALSE(missing.hasValue());
        CHECK(support::hasDiagnostic(missing.diagnostics, "cxc.project.invalid"));
        CHECK_FALSE(missing.bytes.has_value());
    }
}

TEST_CASE("CXC validates shader and audio dependency constraints and duplicate content sources",
          "[cxc][writer][closure][branch-coverage]") {
    SECTION("Shader assets are dependency leaves and only Material may depend on them") {
        auto shaderLeaf = twoRootRequest(
            R"({"format":"cuexis.asset-index","version":3,"assets":[{"id":"shader.vertex","type":"shader","source":"shader.bin","dependencies":["texture.base"]},{"id":"texture.base","type":"texture","source":"texture.bin","dependencies":[]}],"extensions":{}})",
            emptyIndex());
        shaderLeaf.entries.push_back(support::binaryEntry("a/shader.bin", "shader"));
        shaderLeaf.entries.push_back(support::binaryEntry("a/texture.bin", "texture"));
        const auto nonLeaf = CxcWriter::write(std::move(shaderLeaf));
        CHECK_FALSE(nonLeaf.hasValue());
        CHECK(support::hasDiagnostic(nonLeaf.diagnostics,
                                     "asset_index.shader.dependencies_not_empty"));
        CHECK(support::hasDiagnostic(nonLeaf.diagnostics, "cxc.project.invalid"));
        CHECK_FALSE(nonLeaf.bytes.has_value());

        auto nonMaterial = twoRootRequest(
            R"({"format":"cuexis.asset-index","version":3,"assets":[{"id":"texture.base","type":"texture","source":"texture.bin","dependencies":["shader.vertex"]},{"id":"shader.vertex","type":"shader","source":"shader.bin","dependencies":[]}],"extensions":{}})",
            emptyIndex());
        nonMaterial.entries.push_back(support::binaryEntry("a/shader.bin", "shader"));
        nonMaterial.entries.push_back(support::binaryEntry("a/texture.bin", "texture"));
        const auto shaderDependency = CxcWriter::write(std::move(nonMaterial));
        CHECK_FALSE(shaderDependency.hasValue());
        CHECK(support::hasDiagnostic(shaderDependency.diagnostics,
                                     "asset_index.shader.dependency_forbidden"));
        CHECK(support::hasDiagnostic(shaderDependency.diagnostics, "cxc.project.invalid"));
        CHECK_FALSE(shaderDependency.bytes.has_value());
    }

    SECTION("Audio assets are dependency leaves") {
        auto audioLeaf = twoRootRequest(
            R"({"format":"cuexis.asset-index","version":2,"assets":[{"id":"audio.main","type":"audio","source":"main.wav","dependencies":["texture.base"]},{"id":"texture.base","type":"texture","source":"texture.bin","dependencies":[]}],"extensions":{}})",
            emptyIndex());
        audioLeaf.entries.push_back(support::binaryEntry("a/main.wav", "audio"));
        audioLeaf.entries.push_back(support::binaryEntry("a/texture.bin", "texture"));

        const auto result = CxcWriter::write(std::move(audioLeaf));
        CHECK_FALSE(result.hasValue());
        CHECK(
            support::hasDiagnostic(result.diagnostics, "asset_index.audio.dependencies_not_empty"));
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.project.invalid"));
        CHECK_FALSE(result.bytes.has_value());
    }

    SECTION("Material assets may share a Shader dependency") {
        auto validGraph = twoRootRequest(
            R"({"format":"cuexis.asset-index","version":3,"assets":[{"id":"material.first","type":"material","source":"first.material.bin","dependencies":["shader.vertex"]},{"id":"material.second","type":"material","source":"second.material.bin","dependencies":["shader.vertex"]},{"id":"shader.vertex","type":"shader","source":"vertex.shader.bin","dependencies":[]}],"extensions":{}})",
            emptyIndex());
        validGraph.entries.push_back(support::binaryEntry("a/first.material.bin", "first"));
        validGraph.entries.push_back(support::binaryEntry("a/second.material.bin", "second"));
        validGraph.entries.push_back(support::binaryEntry("a/vertex.shader.bin", "shader"));

        const auto written = CxcWriter::write(std::move(validGraph));
        REQUIRE(written.hasValue());
        REQUIRE(written.bytes.has_value());
        const auto loaded = CxcPackageLoader::loadMemory(*written.bytes);
        REQUIRE(loaded.hasValue());
        REQUIRE(loaded.package.has_value());
        CHECK(loaded.package->assetIndexes().front().document.assets.size() == 3);
    }

    SECTION("separate Asset IDs cannot claim the same content source") {
        auto request = twoRootRequest(
            R"({"format":"cuexis.asset-index","version":1,"assets":[{"id":"texture.first","type":"texture","source":"shared.bin","dependencies":[]},{"id":"texture.second","type":"texture","source":"shared.bin","dependencies":[]}],"extensions":{}})",
            emptyIndex());
        request.entries.push_back(support::binaryEntry("a/shared.bin", "shared"));
        const auto result = CxcWriter::write(std::move(request));
        CHECK_FALSE(result.hasValue());
        CHECK(support::hasDiagnostic(result.diagnostics, "cxc.project.invalid"));
        CHECK_FALSE(result.bytes.has_value());
    }
}
