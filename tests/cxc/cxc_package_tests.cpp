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
    const auto found = std::ranges::find(
        loaded.package->manifest().entries, "assets/charts/main.cuexis.chart.json",
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
