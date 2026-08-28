//  AssetIndexReader 实现 — 资产索引加载
//  每个资产根独立一份 cuexis.asset-index.json，format: "cuexis.asset-index"
//  v1 types: mesh, material, texture. v2 adds leaf audio. v3 adds leaf shader.
//  目录枚举不参与 AssetId 发现，所有资产必须显式列入索引

#include <cuexis/project/asset_index_reader.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/reader.hpp>

#include "project_validation.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

namespace cuexis::project {
namespace {

using core::DiagnosticSeverity;

[[nodiscard]] core::Diagnostics makeDiagnostics(const AssetIndexLimits& limits) {
    return core::Diagnostics{
        limits.maxDiagnostics,
        core::Diagnostic{DiagnosticSeverity::Error, "asset_index.diagnostics.limit_exceeded",
                         "Asset Index diagnostic count exceeds the configured limit", "$"}};
}

void addDiagnostic(core::Diagnostics& diagnostics, DiagnosticSeverity severity, std::string code,
                   std::string message, std::string fieldPath) {
    static_cast<void>(diagnostics.add(
        core::Diagnostic{severity, std::move(code), std::move(message), std::move(fieldPath)}));
}

void addError(core::Diagnostics& diagnostics, std::string code, std::string message,
              std::string fieldPath) {
    addDiagnostic(diagnostics, DiagnosticSeverity::Error, std::move(code), std::move(message),
                  std::move(fieldPath));
}

void addError(core::Diagnostics& diagnostics, const core::Error& error, std::string fieldPath) {
    auto diagnostic = core::Diagnostic{DiagnosticSeverity::Error, std::string{error.code()},
                                       std::string{error.message()}, std::move(fieldPath)};
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    static_cast<void>(diagnostics.add(std::move(diagnostic)));
}

void addWarning(core::Diagnostics& diagnostics, std::string code, std::string message,
                std::string fieldPath) {
    addDiagnostic(diagnostics, DiagnosticSeverity::Warning, std::move(code), std::move(message),
                  std::move(fieldPath));
}

[[nodiscard]] bool validateLimits(const AssetIndexLimits& limits, core::Diagnostics& diagnostics) {
    if (limits.maxInputBytes == 0 || limits.maxNestingDepth == 0 || limits.maxStringBytes == 0 ||
        limits.maxPortablePathBytes == 0 || limits.maxAssetIdBytes == 0 ||
        limits.maxDiagnostics == 0 || limits.maxAssets == 0 ||
        limits.maxDependenciesPerAsset == 0 || limits.maxExtensions == 0) {
        addError(diagnostics, "asset_index.limits.invalid",
                 "Asset Index limits must all be greater than zero", "$");
        return false;
    }
    return true;
}

[[nodiscard]] bool isAssetId(std::string_view value, std::size_t maxBytes) noexcept {
    if (value.empty() || value.size() > maxBytes) {
        return false;
    }
    const auto isAlphaNumeric = [](char character) {
        return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
               (character >= '0' && character <= '9');
    };
    if (!isAlphaNumeric(value.front())) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [&](char character) {
        return isAlphaNumeric(character) || character == '.' || character == '_' ||
               character == '/' || character == '-';
    });
}

[[nodiscard]] std::optional<std::string> readString(const json::Reader& reader) {
    const auto value = reader.readString();
    return value ? std::optional<std::string>{std::string{*value}} : std::nullopt;
}

[[nodiscard]] std::optional<AssetType> readAssetType(const json::Reader& reader,
                                                     std::uint32_t formatVersion,
                                                     core::Diagnostics& diagnostics) {
    const auto value = reader.readString();
    if (!value) {
        return std::nullopt;
    }
    if (*value == "mesh") {
        return AssetType::Mesh;
    }
    if (*value == "material") {
        return AssetType::Material;
    }
    if (*value == "texture") {
        return AssetType::Texture;
    }
    if (*value == "audio" &&
        (formatVersion == assetIndexFormatVersion2 || formatVersion == assetIndexFormatVersion3)) {
        return AssetType::Audio;
    }
    if (*value == "shader" && formatVersion == assetIndexFormatVersion3) {
        return AssetType::Shader;
    }
    addError(diagnostics, "asset_index.type.unsupported",
             formatVersion == assetIndexFormatVersion3
                 ? "Asset type must be mesh, material, texture, audio, or shader"
             : formatVersion == assetIndexFormatVersion2
                 ? "Asset type must be mesh, material, texture, or audio"
                 : "Asset type must be mesh, material, or texture",
             std::string{reader.fieldPath()});
    return std::nullopt;
}

[[nodiscard]] std::optional<OpaqueJson> readExtensions(const json::Reader& reader,
                                                       const AssetIndexLimits& limits,
                                                       core::Diagnostics& diagnostics) {
    const auto* object = reader.readObject();
    if (object == nullptr) {
        return std::nullopt;
    }
    if (object->size() > limits.maxExtensions) {
        addError(diagnostics, "asset_index.extensions.limit",
                 "Asset Index extension count exceeds the configured limit",
                 std::string{reader.fieldPath()});
    }
    auto serialized = json::serialize(reader.value());
    if (!serialized) {
        addError(diagnostics, serialized.error(), std::string{reader.fieldPath()});
        return std::nullopt;
    }
    if (!object->empty()) {
        addWarning(diagnostics, "asset_index.extensions.opaque",
                   "Asset Index extensions are preserved without v1 runtime behavior",
                   std::string{reader.fieldPath()});
    }
    return OpaqueJson{std::move(*serialized)};
}

} // namespace

std::string_view assetTypeName(AssetType type) noexcept {
    switch (type) {
    case AssetType::Mesh:
        return "mesh";
    case AssetType::Material:
        return "material";
    case AssetType::Texture:
        return "texture";
    case AssetType::Audio:
        return "audio";
    case AssetType::Shader:
        return "shader";
    }
    return "unknown";
}

AssetIndexResult AssetIndexReader::read(std::string_view jsonText, const AssetIndexLimits& limits) {
    auto diagnostics = makeDiagnostics(limits);
    if (!validateLimits(limits, diagnostics)) {
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }

    auto parsed =
        json::parse(jsonText, json::ParseLimits{limits.maxInputBytes, limits.maxNestingDepth,
                                                limits.maxStringBytes});
    if (!parsed) {
        addError(diagnostics, parsed.error(), "$");
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }

    const json::Reader root{*parsed, diagnostics};
    if (root.readObject() == nullptr) {
        diagnostics.sortDeterministically();
        return {std::nullopt, std::move(diagnostics)};
    }
    constexpr std::array rootFields{std::string_view{"format"}, std::string_view{"version"},
                                    std::string_view{"assets"}, std::string_view{"extensions"}};
    root.rejectUnknownFields(rootFields);

    AssetIndexDocument document;
    const auto formatReader = root.requiredField("format");
    const auto versionReader = root.requiredField("version");
    const auto assetsReader = root.requiredField("assets");
    const auto extensionsReader = root.requiredField("extensions");

    if (formatReader) {
        const auto value = readString(*formatReader);
        if (value) {
            document.format = *value;
            if (*value != assetIndexFormat) {
                addError(diagnostics, "asset_index.format.unsupported",
                         "Asset Index format is unsupported",
                         std::string{formatReader->fieldPath()});
            }
        }
    }
    if (versionReader) {
        const auto value = versionReader->readInt64();
        if (value && *value != assetIndexFormatVersion && *value != assetIndexFormatVersion2 &&
            *value != assetIndexFormatVersion3) {
            addError(diagnostics, "asset_index.version.unsupported",
                     "Asset Index format version is unsupported",
                     std::string{versionReader->fieldPath()});
        } else if (value && *value >= 0) {
            document.version = static_cast<std::uint32_t>(*value);
        }
    }

    std::set<std::string, std::less<>> assetIds;
    if (assetsReader) {
        const auto* assets = assetsReader->readArray();
        if (assets != nullptr) {
            if (assets->size() > limits.maxAssets) {
                addError(diagnostics, "asset_index.assets.limit",
                         "Asset record count exceeds the configured limit",
                         std::string{assetsReader->fieldPath()});
            }
            const auto count = std::min(assets->size(), limits.maxAssets);
            document.assets.reserve(count);
            for (std::size_t index = 0; index < count; ++index) {
                const auto item = assetsReader->element(index);
                if (!item || item->readObject() == nullptr) {
                    continue;
                }
                constexpr std::array fields{
                    std::string_view{"id"}, std::string_view{"type"}, std::string_view{"source"},
                    std::string_view{"dependencies"}, std::string_view{"extensions"}};
                item->rejectUnknownFields(fields);
                const auto idReader = item->requiredField("id");
                const auto typeReader = item->requiredField("type");
                const auto sourceReader = item->requiredField("source");
                const auto dependenciesReader = item->requiredField("dependencies");
                const auto recordExtensionsReader = item->optionalField("extensions");

                AssetIndexRecord record;
                bool complete = true;
                if (idReader) {
                    const auto value = readString(*idReader);
                    if (!value) {
                        complete = false;
                    } else {
                        record.id = *value;
                        if (!isAssetId(*value, limits.maxAssetIdBytes)) {
                            addError(
                                diagnostics, "asset_index.id.invalid",
                                "Asset ID contains unsupported characters or exceeds its limit",
                                std::string{idReader->fieldPath()});
                        }
                        if (!assetIds.insert(*value).second) {
                            addError(diagnostics, "asset_index.id.duplicate",
                                     "Asset IDs must be unique within an index",
                                     std::string{idReader->fieldPath()});
                        }
                    }
                } else {
                    complete = false;
                }
                if (typeReader) {
                    const auto value = readAssetType(*typeReader, document.version, diagnostics);
                    if (value) {
                        record.type = *value;
                    } else {
                        complete = false;
                    }
                } else {
                    complete = false;
                }
                if (sourceReader) {
                    const auto value = readString(*sourceReader);
                    if (!value) {
                        complete = false;
                    } else {
                        record.source = *value;
                        static_cast<void>(
                            detail::validatePortablePath(*value, limits.maxPortablePathBytes,
                                                         diagnostics, sourceReader->fieldPath()));
                    }
                } else {
                    complete = false;
                }

                if (dependenciesReader) {
                    const auto* dependencies = dependenciesReader->readArray();
                    if (dependencies == nullptr) {
                        complete = false;
                    } else {
                        if (dependencies->size() > limits.maxDependenciesPerAsset) {
                            addError(diagnostics, "asset_index.dependencies.limit",
                                     "Asset dependency count exceeds the configured limit",
                                     std::string{dependenciesReader->fieldPath()});
                        }
                        const auto dependencyCount =
                            std::min(dependencies->size(), limits.maxDependenciesPerAsset);
                        std::set<std::string, std::less<>> uniqueDependencies;
                        record.dependencies.reserve(dependencyCount);
                        for (std::size_t dependencyIndex = 0; dependencyIndex < dependencyCount;
                             ++dependencyIndex) {
                            const auto dependency = dependenciesReader->element(dependencyIndex);
                            if (!dependency) {
                                continue;
                            }
                            const auto value = readString(*dependency);
                            if (!value) {
                                continue;
                            }
                            if (!isAssetId(*value, limits.maxAssetIdBytes)) {
                                addError(diagnostics, "asset_index.dependency.id_invalid",
                                         "Asset dependency ID contains unsupported characters or "
                                         "exceeds its limit",
                                         std::string{dependency->fieldPath()});
                            }
                            if (!uniqueDependencies.insert(*value).second) {
                                addError(diagnostics, "asset_index.dependency.duplicate",
                                         "Asset dependencies must be unique",
                                         std::string{dependency->fieldPath()});
                            }
                            record.dependencies.push_back(*value);
                        }
                        std::sort(record.dependencies.begin(), record.dependencies.end());
                    }
                } else {
                    complete = false;
                }
                if (recordExtensionsReader) {
                    const auto value = readExtensions(*recordExtensionsReader, limits, diagnostics);
                    if (value) {
                        record.extensions = *value;
                    }
                }
                if (complete) {
                    document.assets.push_back(std::move(record));
                }
            }
        }
    }

    if (extensionsReader) {
        const auto value = readExtensions(*extensionsReader, limits, diagnostics);
        if (value) {
            document.extensions = *value;
        }
    }

    if (document.version == assetIndexFormatVersion2 ||
        document.version == assetIndexFormatVersion3) {
        std::set<std::string, std::less<>> audioIds;
        std::set<std::string, std::less<>> shaderIds;
        for (const auto& record : document.assets) {
            if (record.type == AssetType::Audio) {
                audioIds.insert(record.id);
                if (!record.dependencies.empty()) {
                    addError(diagnostics, "asset_index.audio.dependencies_not_empty",
                             "Audio records must be dependency leaves",
                             "$/assets/" + record.id + "/dependencies");
                }
            }
            if (record.type == AssetType::Shader) {
                shaderIds.insert(record.id);
                if (!record.dependencies.empty()) {
                    addError(diagnostics, "asset_index.shader.dependencies_not_empty",
                             "Shader records must be dependency leaves",
                             "$/assets/" + record.id + "/dependencies");
                }
            }
        }
        for (const auto& record : document.assets) {
            if (record.type == AssetType::Audio) {
                continue;
            }
            for (const auto& dependency : record.dependencies) {
                if (audioIds.contains(dependency)) {
                    addError(diagnostics, "asset_index.audio.dependency_forbidden",
                             "Non-audio records must not depend on Audio records",
                             "$/assets/" + record.id + "/dependencies");
                }
                if (shaderIds.contains(dependency) && record.type != AssetType::Material) {
                    addError(diagnostics, "asset_index.shader.dependency_forbidden",
                             "Only material records may depend on Shader records",
                             "$/assets/" + record.id + "/dependencies");
                }
            }
        }
    }

    diagnostics.sortDeterministically();
    if (diagnostics.hasErrors()) {
        return {std::nullopt, std::move(diagnostics)};
    }
    return {std::move(document), std::move(diagnostics)};
}

} // namespace cuexis::project
