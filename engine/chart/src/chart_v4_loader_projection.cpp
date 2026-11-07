#include "chart_v4_loader_helpers_internal.hpp"

#include "canonical_chart_loader_internal.hpp"
#include "chart_v4_common_reader_internal.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::chart::loader_internal {
namespace {

[[nodiscard]] auto expectedParameterType(std::string_view path)
    -> std::optional<ChartParameterType> {
    if (path == "$/camera/fovY" || path.ends_with("/components/cuexis.camera/fovY")) {
        return ChartParameterType::Number;
    }
    const auto isAxis = path.ends_with("/0") || path.ends_with("/1") || path.ends_with("/2");
    if (isAxis && (path.find("/components/cuexis.transform/position/") != std::string_view::npos ||
                   path.find("/components/cuexis.transform/scale/") != std::string_view::npos)) {
        return ChartParameterType::Number;
    }
    return std::nullopt;
}

[[nodiscard]] auto neutralParameterValue(std::string_view path) -> json::Value {
    if (path.find("/components/cuexis.transform/scale/") != std::string_view::npos) {
        return json::Value{1.0};
    }
    if (path.ends_with("/fovY")) {
        return json::Value{60.0};
    }
    if (path.ends_with("/mesh") || path.ends_with("/material") || path.ends_with("/mainMusic")) {
        json::Value::Object reference;
        reference.emplace("domain", json::Value{"asset"});
        reference.emplace("id", json::Value{"invalid.placeholder"});
        return json::Value{std::move(reference)};
    }
    return json::Value{0.0};
}

void scanParameterReferences(json::Value& value, std::string semanticPath, std::string sourcePath,
                             const ChartLimits& limits, core::Diagnostics& diagnostics,
                             std::vector<ParameterUse>& uses) {
    auto* object = value.object();
    if (object != nullptr) {
        const auto parameter = object->find("parameter");
        if (parameter != object->end()) {
            const auto expectedType = expectedParameterType(semanticPath);
            const auto* reference = parameter->second.object();
            const auto* domainValue =
                reference != nullptr ? parameter->second.find("domain") : nullptr;
            const auto* idValue = reference != nullptr ? parameter->second.find("id") : nullptr;
            const auto* domain = domainValue != nullptr ? domainValue->string() : nullptr;
            const auto* id = idValue != nullptr ? idValue->string() : nullptr;
            if (object->size() != 1 || reference == nullptr || reference->size() != 2 ||
                domain == nullptr || *domain != "chart-parameter" || id == nullptr) {
                detail::addV4Error(diagnostics, "chart.parameter.use_not_allowed",
                                   "Malformed ChartParameterRef", sourcePath);
            } else if (!expectedType) {
                detail::addV4Error(diagnostics, "chart.parameter.use_not_allowed",
                                   "ChartParameterRef is not allowed at this field", sourcePath);
            } else {
                json::Reader idReader{*idValue, diagnostics, sourcePath + "/parameter/id"};
                const auto validatedId = detail::readPortableStableId(idReader, limits, diagnostics,
                                                                      "Chart parameter reference");
                if (validatedId) {
                    uses.push_back(ParameterUse{*validatedId, *expectedType, sourcePath});
                }
            }
            value = neutralParameterValue(semanticPath);
            return;
        }
        for (auto& [name, child] : *object) {
            scanParameterReferences(child, json::appendFieldPath(semanticPath, name),
                                    json::appendFieldPath(sourcePath, name), limits, diagnostics,
                                    uses);
        }
        return;
    }
    auto* array = value.array();
    if (array == nullptr) {
        return;
    }
    for (std::size_t index = 0; index < array->size(); ++index) {
        scanParameterReferences((*array)[index], json::appendIndexPath(semanticPath, index),
                                json::appendIndexPath(sourcePath, index), limits, diagnostics,
                                uses);
    }
}

void sanitizeComponents(json::Value& components, std::string path, const ChartLimits& limits,
                        core::Diagnostics& diagnostics, std::vector<ParameterUse>& uses) {
    auto* object = components.object();
    if (object == nullptr) {
        return;
    }
    object->erase("cuexis.animator");
    for (auto& [name, component] : *object) {
        const auto componentPath = json::appendFieldPath(path, name);
        scanParameterReferences(component, componentPath, componentPath, limits, diagnostics, uses);
    }
    if (object->empty()) {
        json::Value::Object element;
        element.emplace("version", json::Value{std::int64_t{1}});
        object->emplace("cuexis.element", json::Value{std::move(element)});
    }
}

void sanitizePatches(json::Value& patches, std::string ownerPath, std::string_view patchField,
                     const ChartLimits& limits, core::Diagnostics& diagnostics,
                     std::vector<ParameterUse>& uses) {
    auto* array = patches.array();
    if (array == nullptr) {
        return;
    }
    const auto patchesPath = json::appendFieldPath(ownerPath, patchField);
    auto output = json::Value::Array{};
    output.reserve(array->size());
    for (std::size_t index = 0; index < array->size(); ++index) {
        auto& patch = (*array)[index];
        auto* object = patch.object();
        const auto* pathValue = object != nullptr ? patch.find("path") : nullptr;
        const auto* patchPath = pathValue != nullptr ? pathValue->string() : nullptr;
        if (patchPath != nullptr && *patchPath == "/components/cuexis.animator") {
            continue;
        }
        if (patchPath != nullptr && patchPath->starts_with("/components/cuexis.animator/")) {
            continue;
        }
        if (object != nullptr && patchPath != nullptr) {
            if (auto* value = patch.find("value"); value != nullptr) {
                std::string semanticPath = ownerPath;
                for (char character : *patchPath) {
                    semanticPath.push_back(character);
                }
                const auto patchPathInSource = json::appendIndexPath(patchesPath, index);
                scanParameterReferences(*value, std::move(semanticPath),
                                        json::appendFieldPath(patchPathInSource, "value"), limits,
                                        diagnostics, uses);
            }
        }
        output.push_back(std::move(patch));
    }
    *array = std::move(output);
}

void sanitizeOwners(json::Value& root, std::string_view arrayName, const ChartLimits& limits,
                    core::Diagnostics& diagnostics, std::vector<ParameterUse>& uses) {
    auto* ownersValue = root.find(arrayName);
    auto* owners = ownersValue != nullptr ? ownersValue->array() : nullptr;
    if (owners == nullptr) {
        return;
    }
    for (std::size_t index = 0; index < owners->size(); ++index) {
        auto& owner = (*owners)[index];
        auto* object = owner.object();
        if (object == nullptr) {
            continue;
        }
        const auto ownerPath = json::appendIndexPath(json::appendFieldPath("$", arrayName), index);
        if (arrayName == "templates") {
            if (auto* prototype = owner.find("prototype"); prototype != nullptr) {
                if (auto* components = prototype->find("components"); components != nullptr) {
                    sanitizeComponents(*components, ownerPath + "/prototype/components", limits,
                                       diagnostics, uses);
                }
            }
            if (auto* patches = owner.find("patch"); patches != nullptr) {
                sanitizePatches(*patches, ownerPath, "patch", limits, diagnostics, uses);
            }
        } else {
            if (auto* components = owner.find("components"); components != nullptr) {
                sanitizeComponents(*components, ownerPath + "/components", limits, diagnostics,
                                   uses);
            }
            if (auto* patches = owner.find("overrides"); patches != nullptr) {
                sanitizePatches(*patches, ownerPath, "overrides", limits, diagnostics, uses);
            }
        }
    }
}

} // namespace

[[nodiscard]] auto makeLegacyProjection(json::Value source, const ChartLimits& limits,
                                        core::Diagnostics& diagnostics,
                                        std::vector<ParameterUse>& parameterUses)
    -> std::optional<ChartDocument> {
    auto* root = source.object();
    if (root == nullptr) {
        return std::nullopt;
    }
    root->insert_or_assign("version", json::Value{std::int64_t{3}});
    root->erase("parameters");
    root->erase("animationTemplateImports");
    root->erase("animationClips");
    root->insert_or_assign("requiredExtensions", json::Value{json::Value::Array{}});
    if (auto* camera = source.find("camera"); camera != nullptr) {
        scanParameterReferences(*camera, "$/camera", "$/camera", limits, diagnostics,
                                parameterUses);
    }
    sanitizeOwners(source, "templates", limits, diagnostics, parameterUses);
    sanitizeOwners(source, "objects", limits, diagnostics, parameterUses);
    auto projection = detail::loadCanonicalValue(source, limits);
    diagnostics.append(std::move(projection.diagnostics));
    if (!projection.document) {
        return std::nullopt;
    }
    projection.document->version = 4;
    return std::move(*projection.document);
}

} // namespace cuexis::chart::loader_internal
