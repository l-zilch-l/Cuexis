#include "host_content.hpp"

#include <cstddef>
#include <fstream>
#include <iterator>
#include <memory>
#include <string_view>
#include <utility>

namespace cuexis_reference_host {
namespace {

using cuexis::content::ContentBlob;
using cuexis::content::ContentRequest;
using cuexis::core::Error;
using cuexis::core::Result;
using cuexis::core::unexpected;

[[nodiscard]] auto isPortableRelativeSource(std::string_view source) -> bool {
    if (source.empty() || source.size() > 512U) {
        return false;
    }
    if (source.front() == '/' || source.find('\\') != std::string_view::npos ||
        source.find(':') != std::string_view::npos) {
        return false;
    }
    std::size_t begin = 0;
    while (begin <= source.size()) {
        const auto end = source.find('/', begin);
        const auto part = source.substr(
            begin, end == std::string_view::npos ? std::string_view::npos : end - begin);
        if (part.empty() || part == "." || part == "..") {
            return false;
        }
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1U;
    }
    return true;
}

[[nodiscard]] auto readFileBytes(const std::filesystem::path& path, std::size_t maxBytes)
    -> Result<std::vector<std::byte>> {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        return unexpected(Error{"host.content.asset_missing", "Host asset could not be opened"});
    }
    std::vector<std::byte> bytes;
    char buffer[4096];
    while (stream) {
        stream.read(buffer, sizeof(buffer));
        const auto read = stream.gcount();
        for (std::streamsize index = 0; index < read; ++index) {
            bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(buffer[index])));
        }
        if (bytes.size() > maxBytes) {
            return unexpected(
                Error{"host.content.asset_too_large", "Host asset exceeds the request limit"});
        }
    }
    if (bytes.empty()) {
        return unexpected(Error{"host.content.asset_empty", "Host asset is empty"});
    }
    return bytes;
}

[[nodiscard]] auto readText(const std::filesystem::path& path) -> Result<std::string> {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        return unexpected(
            Error{"host.content.chart_missing", "Host chart document is unavailable"});
    }
    std::string text{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
    if (text.empty()) {
        return unexpected(Error{"host.content.chart_empty", "Host chart document is empty"});
    }
    return text;
}

} // namespace

HostFileProvider::HostFileProvider(std::filesystem::path root, HostReport& report)
    : root_{std::move(root)}, report_{report} {}

void HostFileProvider::failAllReads(std::string code) {
    pendingFailure_ = std::move(code);
}

auto HostFileProvider::readBlob(const ContentRequest& request) -> Result<ContentBlob> {
    ++readCount_;
    if (!pendingFailure_.empty()) {
        const auto code = pendingFailure_;
        report_.note(std::string{"provider.read index="} + std::to_string(readCount_) +
                     " outcome=injected code=" + code);
        return unexpected(Error{code, "Reference host content provider is unavailable"});
    }
    if (request.rootId != "main") {
        return unexpected(Error{"host.content.root_unknown", "Unknown host content root"});
    }
    if (request.maxBytes == 0U) {
        return unexpected(
            Error{"host.content.limit_invalid", "Content byte limit must be non-zero"});
    }
    if (!isPortableRelativeSource(request.source)) {
        return unexpected(Error{"host.content.source_refused",
                                "Host refuses a non-portable relative asset source"});
    }
    auto resolved = (root_ / std::filesystem::path{std::string{request.source}}).lexically_normal();
    const auto relative = resolved.lexically_relative(root_.lexically_normal());
    if (relative.empty() || *relative.begin() == "..") {
        return unexpected(Error{"host.content.source_refused",
                                "Host refuses an asset source outside the content root"});
    }
    auto bytes = readFileBytes(resolved, request.maxBytes);
    if (!bytes) {
        return unexpected(std::move(bytes.error()));
    }
    report_.note(std::string{"provider.read index="} + std::to_string(readCount_) +
                 " outcome=ok source=" + std::string{request.source} +
                 " bytes=" + std::to_string(bytes->size()));
    return ContentBlob{.bytes = std::move(*bytes), .revision = readCount_};
}

auto hostAssetTable() -> std::vector<cuexis::playback::PlaybackAssetDescriptor> {
    using cuexis::playback::PlaybackAssetDescriptor;
    using cuexis::playback::PlaybackAssetType;
    return {
        PlaybackAssetDescriptor{.id = "texture.checker",
                                .type = PlaybackAssetType::Texture,
                                .rootId = "main",
                                .logicalSource = "textures/checker.texture.bin",
                                .dependencies = {}},
        PlaybackAssetDescriptor{.id = "material.blend",
                                .type = PlaybackAssetType::Material,
                                .rootId = "main",
                                .logicalSource = "materials/blend.material.bin",
                                .dependencies = {"texture.checker"}},
        PlaybackAssetDescriptor{.id = "mesh.triangle",
                                .type = PlaybackAssetType::Mesh,
                                .rootId = "main",
                                .logicalSource = "meshes/triangle.mesh.bin",
                                .dependencies = {}},
        PlaybackAssetDescriptor{.id = "material.opaque",
                                .type = PlaybackAssetType::Material,
                                .rootId = "main",
                                .logicalSource = "materials/opaque.material.bin",
                                .dependencies = {}},
    };
}

auto buildProjectSource(const HostContent& content, HostReport& report, bool faulty)
    -> Result<cuexis::playback::PlaybackSource> {
    if (content.candidateEntry) {
        if (faulty) {
            return unexpected(Error{"host.content.asset_unavailable",
                                    "Injected source failure before candidate loading"});
        }
        return cuexis::playback::PlaybackSource::fromFilesystemProjectEntry(
            content.projectDirectory, *content.candidateEntry);
    }
    const auto chartPath =
        content.projectDirectory / "assets" / std::filesystem::path{content.chartEntryPath};
    auto chart = readText(chartPath);
    if (!chart) {
        return unexpected(std::move(chart.error()));
    }
    auto provider = std::make_shared<HostFileProvider>(content.projectDirectory / "assets", report);
    if (faulty) {
        provider->failAllReads("host.content.asset_unavailable");
    }
    auto project = cuexis::playback::TypedPlaybackProjectSource{
        .sourceId = std::string{"reference-host-"} + (faulty ? "faulty" : "content"),
        .entryChartPath = content.chartEntryPath,
        .projectDocuments = {{.path = content.chartEntryPath, .utf8Text = std::move(*chart)}},
        .assets = hostAssetTable()};
    auto source = cuexis::playback::PlaybackSource::fromTypedProjectSource(
        std::move(project),
        std::shared_ptr<cuexis::content::IContentProvider>{std::move(provider)});
    if (!source) {
        return unexpected(std::move(source.error()));
    }
    return std::move(*source);
}

} // namespace cuexis_reference_host
