#pragma once

// Shared CXC request builders for the package case units.

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

namespace cxc_package_fixture {

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

[[nodiscard]] inline auto findRequestEntry(CxcWriteRequest& request, std::string_view path)
    -> CxcWriteEntry& {
    const auto found = std::ranges::find(request.entries, path, &CxcWriteEntry::path);
    REQUIRE(found != request.entries.end());
    return *found;
}

inline void eraseRequestEntry(CxcWriteRequest& request, std::string_view path) {
    const auto found = std::ranges::find(request.entries, path, &CxcWriteEntry::path);
    REQUIRE(found != request.entries.end());
    request.entries.erase(found);
}

[[nodiscard]] inline auto contentRequest(std::string_view root, std::string_view source,
                                         std::size_t maxBytes = 1024U)
    -> cuexis::content::ContentRequest {
    return {.rootId = root, .source = source, .maxBytes = maxBytes};
}

[[nodiscard]] inline auto packageRequest(const CxcPackage& package) -> CxcWriteRequest {
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

[[nodiscard]] inline auto twoRootProject(std::string_view firstPath = "a",
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

[[nodiscard]] inline auto twoRootRequest(std::string firstIndex, std::string secondIndex,
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

[[nodiscard]] inline auto emptyIndex(std::uint32_t version = 1) -> std::string {
    return "{\"format\":\"cuexis.asset-index\",\"version\":" + std::to_string(version) +
           ",\"assets\":[],\"extensions\":{}}";
}

[[nodiscard]] inline auto referenceV4Request() -> CxcWriteRequest {
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

} // namespace cxc_package_fixture

using namespace cxc_package_fixture;
