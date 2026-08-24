#include <cuexis/cxc/cxc_writer.hpp>

#include <cuexis/cxc/cxc_manifest_loader.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/value.hpp>

#include "cxc_hash_internal.hpp"
#include "cxc_path_internal.hpp"
#include "zip32_envelope_internal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <new>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::cxc {
namespace {

[[nodiscard]] auto makeDiagnostics(const CxcPackageLimits& limits) -> core::Diagnostics {
    return core::Diagnostics{limits.maxDiagnostics,
                             core::Diagnostic{core::DiagnosticSeverity::Error,
                                              "cxc.budget.exceeded",
                                              "CXC diagnostics reached the configured limit", "$"}};
}

void addError(core::Diagnostics& diagnostics, std::string code, std::string message,
              std::string fieldPath, std::string_view path = {}) {
    auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error, std::move(code),
                                       std::move(message), std::move(fieldPath)};
    if (!path.empty()) {
        diagnostic.withContext("path", std::string{path});
    }
    static_cast<void>(diagnostics.add(std::move(diagnostic)));
}

void addError(core::Diagnostics& diagnostics, const core::Error& error, std::string fieldPath) {
    auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error, std::string{error.code()},
                                       std::string{error.message()}, std::move(fieldPath)};
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    static_cast<void>(diagnostics.add(std::move(diagnostic)));
}

[[nodiscard]] auto manifestBytes(const CxcWriteRequest& request, const CxcPackageLimits& limits,
                                 core::Diagnostics& diagnostics) -> std::optional<std::string> {
    auto extensions =
        json::parse(request.extensionsJson, {.maxBytes = limits.maxManifestBytes,
                                             .maxDepth = limits.manifest.maxNestingDepth,
                                             .maxStringBytes = limits.manifest.maxStringBytes});
    if (!extensions || extensions->object() == nullptr) {
        if (!extensions) {
            addError(diagnostics, extensions.error(), "$/extensions");
        } else {
            addError(diagnostics, "cxc.project.invalid",
                     "CXC manifest extensions must be an object", "$/extensions");
        }
        return std::nullopt;
    }

    json::Value::Array entries;
    entries.reserve(request.entries.size());
    for (const auto& entry : request.entries) {
        json::Value::Object item;
        item.emplace("byteCount", json::Value{static_cast<std::uint64_t>(entry.bytes.size())});
        item.emplace("path", json::Value{entry.path});
        item.emplace("sha256", json::Value{detail::sha256Hex(entry.bytes)});
        entries.emplace_back(json::Value{std::move(item)});
    }

    json::Value::Array requiredExtensions;
    requiredExtensions.reserve(request.requiredExtensions.size());
    for (const auto& extension : request.requiredExtensions) {
        json::Value::Object item;
        item.emplace("id", json::Value{extension.id});
        item.emplace("version", json::Value{static_cast<std::uint64_t>(extension.version)});
        requiredExtensions.emplace_back(json::Value{std::move(item)});
    }

    json::Value::Object root;
    root.emplace("entries", json::Value{std::move(entries)});
    root.emplace("extensions", std::move(*extensions));
    root.emplace("format", json::Value{"cuexis.cxc"});
    root.emplace("project", json::Value{"cuexis.project.json"});
    root.emplace("requiredExtensions", json::Value{std::move(requiredExtensions)});
    root.emplace("version", json::Value{std::uint64_t{1}});
    auto serialized = json::serialize(json::Value{std::move(root)}, json::SerializeStyle::Pretty);
    if (!serialized) {
        addError(diagnostics, serialized.error(), "$");
        return std::nullopt;
    }
    serialized->push_back('\n');
    if (serialized->size() > limits.maxManifestBytes) {
        addError(diagnostics, "cxc.budget.exceeded", "CXC manifest exceeds the byte limit",
                 "$/manifest");
        return std::nullopt;
    }
    return std::move(*serialized);
}

[[nodiscard]] auto toBytes(std::string_view text) -> std::vector<std::byte> {
    std::vector<std::byte> result;
    result.reserve(text.size());
    for (const char character : text) {
        result.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
    }
    return result;
}

[[nodiscard]] auto writeImpl(CxcWriteRequest request, const CxcPackageLimits& limits)
    -> CxcWriteResult {
    auto diagnostics = makeDiagnostics(limits);
    if (limits.maxDiagnostics == 0 || limits.maxEntries < 2 || limits.maxPackageBytes == 0 ||
        limits.maxEntryBytes == 0 || limits.maxManifestBytes == 0 || limits.maxPathBytes == 0 ||
        limits.maxPathDepth == 0 || limits.maxDependencyDepth == 0) {
        addError(diagnostics, "cxc.budget.exceeded",
                 "CXC writer limits must all be greater than zero", "$/limits");
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }
    if (request.entries.empty() || request.entries.size() >= limits.maxEntries) {
        addError(diagnostics, "cxc.budget.exceeded", "CXC content entry count is invalid",
                 "$/entries");
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }

    std::ranges::sort(request.entries, {}, &CxcWriteEntry::path);
    std::ranges::sort(request.requiredExtensions, {}, &CxcRequiredExtension::id);
    std::set<std::string, std::less<>> foldedPaths{"cuexis.cxc.json"};
    bool hasProject = false;
    std::uint64_t listedBytes = 0;
    for (std::size_t index = 0; index < request.entries.size(); ++index) {
        const auto& entry = request.entries[index];
        const auto fieldPath = "$/entries/" + std::to_string(index);
        if (!detail::isPortablePath(entry.path, limits.maxPathBytes, limits.maxPathDepth) ||
            entry.path == "cuexis.cxc.json") {
            addError(diagnostics, "cxc.entry.path_invalid", "CXC entry path is not portable",
                     fieldPath, entry.path);
        }
        if (!detail::insertUniqueArchivePath(foldedPaths, entry.path)) {
            addError(diagnostics, "cxc.entry.duplicate",
                     "CXC entry path is duplicated, conflicts by ASCII case, or overlaps a path "
                     "prefix",
                     fieldPath, entry.path);
        }
        if (entry.bytes.size() > limits.maxEntryBytes) {
            addError(diagnostics, "cxc.budget.exceeded", "CXC entry exceeds the byte limit",
                     fieldPath, entry.path);
        }
        if (listedBytes > limits.manifest.maxListedBytes ||
            entry.bytes.size() > limits.manifest.maxListedBytes - listedBytes) {
            addError(diagnostics, "cxc.budget.exceeded",
                     "CXC listed entry total exceeds the byte limit", fieldPath, entry.path);
        } else {
            listedBytes += entry.bytes.size();
        }
        hasProject = hasProject || entry.path == "cuexis.project.json";
    }
    if (!hasProject) {
        addError(diagnostics, "cxc.entry.missing", "CXC package has no ProjectConfig", "$/entries",
                 "cuexis.project.json");
    }
    if (diagnostics.hasErrors()) {
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }

    const auto manifest = manifestBytes(request, limits, diagnostics);
    if (!manifest) {
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }
    auto manifestValidation = CxcManifestLoader::load(*manifest, limits.manifest);
    const auto manifestValid = manifestValidation.hasValue();
    static_cast<void>(diagnostics.append(std::move(manifestValidation.diagnostics)));
    if (!manifestValid || diagnostics.hasErrors()) {
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }

    std::vector<std::pair<std::string, std::vector<std::byte>>> archiveEntries;
    archiveEntries.reserve(request.entries.size() + 1U);
    archiveEntries.emplace_back("cuexis.cxc.json", toBytes(*manifest));
    for (auto& entry : request.entries) {
        archiveEntries.emplace_back(std::move(entry.path), std::move(entry.bytes));
    }
    auto archive = detail::writeCanonicalZip32(archiveEntries, limits);
    if (!archive) {
        addError(diagnostics, archive.error(), "$/archive");
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }

    auto validation = CxcPackageLoader::loadMemory(
        std::span<const std::byte>{archive->data(), archive->size()}, limits);
    const auto packageValid = validation.hasValue();
    static_cast<void>(diagnostics.append(std::move(validation.diagnostics)));
    diagnostics.sortDeterministically();
    if (!packageValid || diagnostics.hasErrors()) {
        return {std::nullopt, std::move(diagnostics)};
    }
    return {std::move(*archive), std::move(diagnostics)};
}

} // namespace

auto CxcWriter::write(CxcWriteRequest request, const CxcPackageLimits& limits) -> CxcWriteResult {
    try {
        return writeImpl(std::move(request), limits);
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception& exception) {
        auto diagnostics = makeDiagnostics(limits);
        auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error, "cxc.internal.failure",
                                           "CXC Writer failed", "$"};
        diagnostic.withContext("exception", exception.what());
        static_cast<void>(diagnostics.add(std::move(diagnostic)));
        return {std::nullopt, std::move(diagnostics)};
    } catch (...) {
        auto diagnostics = makeDiagnostics(limits);
        addError(diagnostics, "cxc.internal.failure", "CXC Writer failed", "$");
        return {std::nullopt, std::move(diagnostics)};
    }
}

} // namespace cuexis::cxc
