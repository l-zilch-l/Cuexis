#include <cuexis/chart/chart_v4_loader.hpp>

#include <cuexis/chart/canonical_chart_loader.hpp>
#include <cuexis/chart/uuid.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/reader.hpp>

#include "chart_v4_reader_internal.hpp"
#include "diagnostic_limit.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::chart {
namespace {

void addParseError(core::Diagnostics& diagnostics, const core::Error& error) {
    auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error, std::string{error.code()},
                                       std::string{error.message()}, "$"};
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    diagnostics.add(std::move(diagnostic));
}

[[nodiscard]] auto parameterTypeName(ChartParameterType type) -> std::string_view {
    switch (type) {
    case ChartParameterType::Number:
        return "number";
    case ChartParameterType::Rational:
        return "rational";
    case ChartParameterType::Weight:
        return "weight";
    }
    return "unknown";
}

[[nodiscard]] auto readParameterType(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<ChartParameterType> {
    const auto value = reader.readString();
    if (!value) {
        return std::nullopt;
    }
    if (*value == "number") {
        return ChartParameterType::Number;
    }
    if (*value == "rational") {
        return ChartParameterType::Rational;
    }
    if (*value == "weight") {
        return ChartParameterType::Weight;
    }
    detail::addV4Error(diagnostics, "chart.parameter.type_mismatch",
                       "Chart parameter type is unsupported", std::string{reader.fieldPath()});
    return std::nullopt;
}

[[nodiscard]] auto readFiniteNumber(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<double> {
    const auto value = reader.readNumber();
    if (!value) {
        return std::nullopt;
    }
    if (!std::isfinite(*value)) {
        detail::addV4Error(diagnostics, "chart.parameter.out_of_range",
                           "Chart parameter value must be finite", std::string{reader.fieldPath()});
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] auto readParameterValue(const json::Reader& reader, ChartParameterType type,
                                      const ChartLimits& limits, core::Diagnostics& diagnostics)
    -> std::optional<ChartParameterValue> {
    if (type == ChartParameterType::Rational) {
        const auto value = detail::readV4Rational(reader, limits, diagnostics, false, true);
        return value ? std::optional<ChartParameterValue>{ChartParameterValue{*value}}
                     : std::nullopt;
    }
    const auto value = readFiniteNumber(reader, diagnostics);
    if (!value) {
        return std::nullopt;
    }
    if (type == ChartParameterType::Weight && (*value < 0.0 || *value > 1.0)) {
        detail::addV4Error(diagnostics, "chart.parameter.out_of_range",
                           "Weight parameter value must be within [0, 1]",
                           std::string{reader.fieldPath()});
        return std::nullopt;
    }
    return ChartParameterValue{*value};
}

[[nodiscard]] auto compareParameterValues(const ChartParameterValue& left,
                                          const ChartParameterValue& right)
    -> std::optional<std::strong_ordering> {
    if (const auto* leftNumber = std::get_if<double>(&left)) {
        const auto* rightNumber = std::get_if<double>(&right);
        if (rightNumber == nullptr) {
            return std::nullopt;
        }
        if (*leftNumber < *rightNumber) {
            return std::strong_ordering::less;
        }
        if (*leftNumber > *rightNumber) {
            return std::strong_ordering::greater;
        }
        return std::strong_ordering::equal;
    }
    const auto* leftRational = std::get_if<RationalBeat>(&left);
    const auto* rightRational = std::get_if<RationalBeat>(&right);
    if (leftRational == nullptr || rightRational == nullptr) {
        return std::nullopt;
    }
    return *leftRational <=> *rightRational;
}

void validateConstraints(const ChartParameterDeclaration& declaration,
                         core::Diagnostics& diagnostics) {
    const auto& constraints = declaration.constraints;
    const auto validatePair = [&](const std::optional<ChartParameterValue>& lower,
                                  const std::optional<ChartParameterValue>& upper, bool strict,
                                  std::string_view message) {
        if (!lower || !upper) {
            return;
        }
        const auto order = compareParameterValues(*lower, *upper);
        if (!order || *order == std::strong_ordering::greater ||
            (strict && *order == std::strong_ordering::equal)) {
            detail::addV4Error(diagnostics, "chart.parameter.out_of_range", std::string{message},
                               declaration.fieldPath + "/constraints");
        }
    };
    validatePair(constraints.minimum, constraints.maximum, false,
                 "Parameter minimum exceeds maximum");
    validatePair(constraints.minimum, constraints.exclusiveMaximum, true,
                 "Parameter minimum is not below exclusiveMaximum");
    validatePair(constraints.exclusiveMinimum, constraints.maximum, true,
                 "Parameter exclusiveMinimum is not below maximum");
    validatePair(constraints.exclusiveMinimum, constraints.exclusiveMaximum, true,
                 "Parameter exclusive bounds are empty");

    if (!declaration.defaultValue) {
        return;
    }
    const auto violatesLower = [&](const std::optional<ChartParameterValue>& bound, bool strict) {
        if (!bound) {
            return false;
        }
        const auto order = compareParameterValues(*declaration.defaultValue, *bound);
        return !order || *order == std::strong_ordering::less ||
               (strict && *order == std::strong_ordering::equal);
    };
    const auto violatesUpper = [&](const std::optional<ChartParameterValue>& bound, bool strict) {
        if (!bound) {
            return false;
        }
        const auto order = compareParameterValues(*declaration.defaultValue, *bound);
        return !order || *order == std::strong_ordering::greater ||
               (strict && *order == std::strong_ordering::equal);
    };
    if (violatesLower(constraints.minimum, false) ||
        violatesLower(constraints.exclusiveMinimum, true) ||
        violatesUpper(constraints.maximum, false) ||
        violatesUpper(constraints.exclusiveMaximum, true)) {
        detail::addV4Error(diagnostics, "chart.parameter.out_of_range",
                           "Chart parameter default is outside its constraints",
                           declaration.fieldPath + "/default");
    }
}

[[nodiscard]] auto readParameters(const json::Reader& reader, const ChartLimits& limits,
                                  core::Diagnostics& diagnostics)
    -> std::vector<ChartParameterDeclaration> {
    std::vector<ChartParameterDeclaration> result;
    const auto* items = reader.readArray();
    if (items == nullptr) {
        return result;
    }
    if (items->size() > limits.maxChartParameters) {
        detail::addV4Error(diagnostics, "chart.parameter.out_of_range",
                           "Chart parameter declaration count exceeds the configured limit",
                           std::string{reader.fieldPath()});
    }
    std::set<std::string, std::less<>> ids;
    for (std::size_t index = 0; index < items->size(); ++index) {
        const auto item = reader.element(index);
        if (!item || item->readObject() == nullptr) {
            continue;
        }
        constexpr std::array fields{std::string_view{"id"}, std::string_view{"type"},
                                    std::string_view{"default"}, std::string_view{"constraints"}};
        item->rejectUnknownFields(fields);
        const auto idReader = item->requiredField("id");
        const auto typeReader = item->requiredField("type");
        const auto constraintsReader = item->requiredField("constraints");
        const auto id = idReader ? detail::readPortableStableId(*idReader, limits, diagnostics,
                                                                "Chart parameter ID")
                                 : std::nullopt;
        const auto type = typeReader ? readParameterType(*typeReader, diagnostics) : std::nullopt;
        if (!id || !type || !constraintsReader || constraintsReader->readObject() == nullptr) {
            continue;
        }
        if (!ids.emplace(*id).second) {
            detail::addV4Error(diagnostics, "chart.parameter.duplicate",
                               "Chart parameter ID is duplicated",
                               std::string{idReader->fieldPath()});
        }
        constexpr std::array constraintFields{
            std::string_view{"minimum"}, std::string_view{"exclusiveMinimum"},
            std::string_view{"maximum"}, std::string_view{"exclusiveMaximum"}};
        constraintsReader->rejectUnknownFields(constraintFields);
        ChartParameterConstraints constraints;
        const auto readConstraint =
            [&](std::string_view name) -> std::optional<ChartParameterValue> {
            const auto field = constraintsReader->optionalField(name);
            return field ? readParameterValue(*field, *type, limits, diagnostics) : std::nullopt;
        };
        constraints.minimum = readConstraint("minimum");
        constraints.exclusiveMinimum = readConstraint("exclusiveMinimum");
        constraints.maximum = readConstraint("maximum");
        constraints.exclusiveMaximum = readConstraint("exclusiveMaximum");
        const auto defaultReader = item->optionalField("default");
        const auto defaultValue =
            defaultReader ? readParameterValue(*defaultReader, *type, limits, diagnostics)
                          : std::nullopt;
        auto declaration = ChartParameterDeclaration{*id, *type, defaultValue, constraints,
                                                     std::string{item->fieldPath()}};
        validateConstraints(declaration, diagnostics);
        result.push_back(std::move(declaration));
    }
    std::ranges::sort(result, {}, &ChartParameterDeclaration::id);
    return result;
}

[[nodiscard]] auto readImports(const json::Reader& reader, const ChartLimits& limits,
                               core::Diagnostics& diagnostics)
    -> std::vector<AnimationTemplateImport> {
    std::vector<AnimationTemplateImport> result;
    const auto* items = reader.readArray();
    if (items == nullptr) {
        return result;
    }
    if (items->size() > limits.maxAnimationImports) {
        detail::addV4Error(diagnostics, "cxt.budget.exceeded",
                           "Animation template import count exceeds the configured limit",
                           std::string{reader.fieldPath()});
    }
    std::set<std::string, std::less<>> ids;
    std::set<std::string, std::less<>> paths;
    std::set<std::string, std::less<>> foldedPaths;
    for (std::size_t index = 0; index < items->size(); ++index) {
        const auto item = reader.element(index);
        if (!item || item->readObject() == nullptr) {
            continue;
        }
        constexpr std::array fields{std::string_view{"id"}, std::string_view{"source"}};
        item->rejectUnknownFields(fields);
        const auto idReader = item->requiredField("id");
        const auto sourceReader = item->requiredField("source");
        const auto id = idReader ? detail::readPortableStableId(*idReader, limits, diagnostics,
                                                                "Animation template import ID")
                                 : std::nullopt;
        const auto source = sourceReader ? sourceReader->readString() : std::nullopt;
        if (!id || !source) {
            continue;
        }
        if (!detail::isCxtProjectPath(*source)) {
            detail::addV4Error(
                diagnostics, "cxt.template.invalid",
                "Animation template source path must be a portable lowercase .cxt path",
                std::string{sourceReader->fieldPath()});
            continue;
        }
        if (!ids.emplace(*id).second || !paths.emplace(*source).second ||
            !foldedPaths.emplace(detail::portableProjectPathCaseKey(*source)).second) {
            detail::addV4Error(diagnostics, "cxt.import.duplicate",
                               "Animation template import ID or source is duplicated",
                               std::string{item->fieldPath()});
        }
        result.push_back(
            AnimationTemplateImport{*id, std::string{*source}, std::string{item->fieldPath()}});
    }
    std::ranges::sort(result, {}, &AnimationTemplateImport::id);
    return result;
}

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
    auto serialized = json::serialize(source);
    if (!serialized) {
        addParseError(diagnostics, serialized.error());
        return std::nullopt;
    }
    auto projection = CanonicalChartLoader::load(*serialized, limits);
    diagnostics.append(std::move(projection.diagnostics));
    if (!projection.document) {
        return std::nullopt;
    }
    projection.document->version = 4;
    return std::move(*projection.document);
}

void readAnimatorFromComponents(const json::Reader& componentsReader, AnimatorOwnerKind ownerKind,
                                std::string ownerId, const ChartLimits& limits,
                                core::Diagnostics& diagnostics,
                                std::vector<ParameterUse>& parameterUses,
                                std::vector<AnimatorSource>& animators) {
    const auto animatorReader = componentsReader.optionalField("cuexis.animator");
    if (!animatorReader) {
        return;
    }
    auto component =
        detail::readAnimatorComponent(*animatorReader, limits, diagnostics, parameterUses);
    if (component) {
        animators.push_back(AnimatorSource{ownerKind, std::move(ownerId), std::move(*component),
                                           std::string{animatorReader->fieldPath()}});
    }
}

void readAnimatorFromPatches(const json::Reader& patchesReader, AnimatorOwnerKind ownerKind,
                             const std::string& ownerId, const ChartLimits& limits,
                             core::Diagnostics& diagnostics,
                             std::vector<ParameterUse>& parameterUses,
                             std::vector<AnimatorSource>& animators) {
    const auto* patches = patchesReader.readArray();
    if (patches == nullptr) {
        return;
    }
    for (std::size_t index = 0; index < patches->size(); ++index) {
        const auto patchReader = patchesReader.element(index);
        if (!patchReader || patchReader->readObject() == nullptr) {
            continue;
        }
        const auto pathReader = patchReader->optionalField("path");
        const auto operationReader = patchReader->optionalField("op");
        const auto path = pathReader ? pathReader->readString() : std::nullopt;
        const auto operation = operationReader ? operationReader->readString() : std::nullopt;
        if (!path) {
            continue;
        }
        if (path->starts_with("/components/cuexis.animator/") ||
            (*path != "/components/cuexis.animator" &&
             path->starts_with("/components/cuexis.animator"))) {
            detail::addV4Error(diagnostics, "chart.patch.path_unsupported",
                               "Animator patch must replace the entire component",
                               std::string{pathReader->fieldPath()});
            continue;
        }
        if (*path != "/components/cuexis.animator" || !operation || *operation == "remove") {
            continue;
        }
        const auto valueReader = patchReader->optionalField("value");
        if (!valueReader) {
            continue;
        }
        auto component =
            detail::readAnimatorComponent(*valueReader, limits, diagnostics, parameterUses);
        if (component) {
            animators.push_back(AnimatorSource{ownerKind, ownerId, std::move(*component),
                                               std::string{valueReader->fieldPath()}});
        }
    }
}

[[nodiscard]] auto readAnimators(const json::Reader& root, const ChartLimits& limits,
                                 core::Diagnostics& diagnostics,
                                 std::vector<ParameterUse>& parameterUses)
    -> std::vector<AnimatorSource> {
    std::vector<AnimatorSource> result;
    const auto readOwners = [&](std::string_view field, AnimatorOwnerKind ownerKind) {
        const auto ownersReader = root.requiredField(field);
        const auto* owners = ownersReader ? ownersReader->readArray() : nullptr;
        if (owners == nullptr) {
            return;
        }
        for (std::size_t index = 0; index < owners->size(); ++index) {
            const auto ownerReader = ownersReader->element(index);
            if (!ownerReader || ownerReader->readObject() == nullptr) {
                continue;
            }
            const auto idReader = ownerReader->optionalField("id");
            const auto id = idReader ? idReader->readString() : std::nullopt;
            if (!id) {
                continue;
            }
            if (ownerKind == AnimatorOwnerKind::Template) {
                if (const auto prototype = ownerReader->optionalField("prototype")) {
                    if (const auto components = prototype->optionalField("components")) {
                        readAnimatorFromComponents(*components, ownerKind, std::string{*id}, limits,
                                                   diagnostics, parameterUses, result);
                    }
                }
                if (const auto patches = ownerReader->optionalField("patch")) {
                    readAnimatorFromPatches(*patches, ownerKind, std::string{*id}, limits,
                                            diagnostics, parameterUses, result);
                }
            } else {
                if (const auto components = ownerReader->optionalField("components")) {
                    readAnimatorFromComponents(*components, ownerKind, std::string{*id}, limits,
                                               diagnostics, parameterUses, result);
                }
                if (const auto patches = ownerReader->optionalField("overrides")) {
                    readAnimatorFromPatches(*patches, ownerKind, std::string{*id}, limits,
                                            diagnostics, parameterUses, result);
                }
            }
        }
    };
    readOwners("templates", AnimatorOwnerKind::Template);
    readOwners("objects", AnimatorOwnerKind::Object);
    std::ranges::sort(result, [](const AnimatorSource& left, const AnimatorSource& right) {
        if (left.ownerKind != right.ownerKind) {
            return left.ownerKind < right.ownerKind;
        }
        if (left.ownerId != right.ownerId) {
            return left.ownerId < right.ownerId;
        }
        return left.fieldPath < right.fieldPath;
    });
    return result;
}

void validateParameterUses(const std::vector<ChartParameterDeclaration>& parameters,
                           const std::vector<ParameterUse>& uses, core::Diagnostics& diagnostics) {
    std::map<std::string, ChartParameterType, std::less<>> types;
    for (const auto& parameter : parameters) {
        types.insert_or_assign(parameter.id, parameter.type);
    }
    for (const auto& use : uses) {
        const auto iterator = types.find(use.id);
        if (iterator == types.end()) {
            detail::addV4Error(diagnostics, "chart.parameter.unknown",
                               "ChartParameterRef refers to an unknown declaration", use.fieldPath);
            continue;
        }
        if (iterator->second != use.expectedType) {
            detail::addV4Error(
                diagnostics, "chart.parameter.type_mismatch",
                "ChartParameterRef expects " + std::string{parameterTypeName(use.expectedType)} +
                    " but declaration is " + std::string{parameterTypeName(iterator->second)},
                use.fieldPath);
        }
    }
}

void validateAnimationReferences(const std::vector<AnimationTemplateImport>& imports,
                                 const std::vector<AnimationClip>& clips,
                                 const std::vector<AnimatorSource>& animators,
                                 core::Diagnostics& diagnostics) {
    std::set<std::string, std::less<>> importIds;
    std::set<std::string, std::less<>> clipIds;
    for (const auto& item : imports) {
        importIds.emplace(item.id);
    }
    for (const auto& clip : clips) {
        clipIds.emplace(clip.id);
    }
    for (const auto& animator : animators) {
        for (const auto& binding : animator.component.templateBindings) {
            if (!importIds.contains(binding.templateId)) {
                detail::addV4Error(diagnostics, "chart.animation.template_reference_missing",
                                   "Template binding refers to an unknown CXT import",
                                   binding.fieldPath + "/template");
            }
        }
        for (const auto& layer : animator.component.layers) {
            for (const auto& group : layer.blendGroups) {
                for (const auto& instance : group.instances) {
                    if (!clipIds.contains(instance.clipId)) {
                        detail::addV4Error(diagnostics, "chart.animation.reference_missing",
                                           "Clip instance refers to an unknown AnimationClip",
                                           instance.fieldPath + "/clip");
                    }
                }
            }
        }
    }
}

} // namespace

auto ChartV4Loader::isV4(std::string_view jsonText, const ChartLimits& limits) -> bool {
    auto parsed =
        json::parse(jsonText, json::ParseLimits{limits.maxInputBytes, limits.maxNestingDepth,
                                                limits.maxStringBytes});
    if (!parsed) {
        return false;
    }
    const auto* version = parsed->find("version");
    if (version == nullptr) {
        return false;
    }
    if (const auto* signedValue = version->signedInteger()) {
        return *signedValue == 4;
    }
    if (const auto* unsignedValue = version->unsignedInteger()) {
        return *unsignedValue == 4;
    }
    return false;
}

auto ChartV4Loader::load(std::string_view jsonText, const ChartLimits& limits)
    -> ChartV4SourceResult {
    auto diagnostics = detail::makeDiagnostics(limits);
    if (detail::rejectInvalidDiagnosticLimit(diagnostics, limits)) {
        return ChartV4SourceResult{std::nullopt, std::move(diagnostics)};
    }
    auto parsed =
        json::parse(jsonText, json::ParseLimits{limits.maxInputBytes, limits.maxNestingDepth,
                                                limits.maxStringBytes});
    if (!parsed) {
        addParseError(diagnostics, parsed.error());
        diagnostics.sortDeterministically();
        return ChartV4SourceResult{std::nullopt, std::move(diagnostics)};
    }

    json::Reader root{*parsed, diagnostics};
    constexpr std::array fields{std::string_view{"format"},
                                std::string_view{"version"},
                                std::string_view{"chartId"},
                                std::string_view{"metadata"},
                                std::string_view{"timing"},
                                std::string_view{"camera"},
                                std::string_view{"audio"},
                                std::string_view{"parameters"},
                                std::string_view{"templates"},
                                std::string_view{"behaviors"},
                                std::string_view{"animationTemplateImports"},
                                std::string_view{"animationClips"},
                                std::string_view{"objects"},
                                std::string_view{"requiredExtensions"},
                                std::string_view{"extensions"}};
    if (root.readObject() == nullptr) {
        diagnostics.sortDeterministically();
        return ChartV4SourceResult{std::nullopt, std::move(diagnostics)};
    }
    root.rejectUnknownFields(fields);
    const auto formatReader = root.requiredField("format");
    const auto versionReader = root.requiredField("version");
    const auto chartIdReader = root.requiredField("chartId");
    const auto parametersReader = root.requiredField("parameters");
    const auto importsReader = root.requiredField("animationTemplateImports");
    const auto clipsReader = root.requiredField("animationClips");
    const auto requiredExtensionsReader = root.requiredField("requiredExtensions");
    const auto extensionsReader = root.requiredField("extensions");
    static_cast<void>(root.requiredField("metadata"));
    static_cast<void>(root.requiredField("timing"));
    static_cast<void>(root.requiredField("templates"));
    static_cast<void>(root.requiredField("behaviors"));
    static_cast<void>(root.requiredField("objects"));

    const auto format = formatReader ? formatReader->readString() : std::nullopt;
    auto version = std::optional<std::int64_t>{};
    if (versionReader) {
        version = versionReader->readInt64();
    }
    const auto chartId = chartIdReader ? chartIdReader->readString() : std::nullopt;
    if (format && *format != "cuexis.chart") {
        detail::addV4Error(diagnostics, "chart.format.unsupported", "Chart format is unsupported",
                           std::string{formatReader->fieldPath()});
    }
    if (version && *version != 4) {
        detail::addV4Error(diagnostics, "chart.version.unsupported", "Chart version is unsupported",
                           std::string{versionReader->fieldPath()});
    }
    if (chartId && !isUuidV7(*chartId)) {
        detail::addV4Error(diagnostics, "chart.chart_id.invalid",
                           "Chart ID must be a lowercase UUIDv7",
                           std::string{chartIdReader->fieldPath()});
    }

    auto parameters = parametersReader ? readParameters(*parametersReader, limits, diagnostics)
                                       : std::vector<ChartParameterDeclaration>{};
    auto imports = importsReader ? readImports(*importsReader, limits, diagnostics)
                                 : std::vector<AnimationTemplateImport>{};
    std::vector<AnimationClip> clips;
    std::set<std::string, std::less<>> clipIds;
    if (clipsReader) {
        if (const auto* items = clipsReader->readArray()) {
            if (items->size() > limits.maxAnimationClips) {
                detail::addV4Error(diagnostics, "chart.animation.generated_limit",
                                   "Animation clip count exceeds the configured limit",
                                   std::string{clipsReader->fieldPath()});
            }
            for (std::size_t index = 0; index < items->size(); ++index) {
                const auto item = clipsReader->element(index);
                if (!item) {
                    continue;
                }
                auto clip = detail::readAnimationClip(*item, limits, diagnostics, true);
                if (!clip) {
                    continue;
                }
                if (!clipIds.emplace(clip->id).second) {
                    detail::addV4Error(diagnostics, "chart.animation.clip_invalid",
                                       "Animation clip ID is duplicated", clip->fieldPath + "/id");
                }
                clips.push_back(std::move(*clip));
            }
        }
    }
    std::ranges::sort(clips, {}, &AnimationClip::id);

    std::vector<ParameterUse> parameterUses;
    auto animators = readAnimators(root, limits, diagnostics, parameterUses);
    const auto projection = makeLegacyProjection(*parsed, limits, diagnostics, parameterUses);
    validateParameterUses(parameters, parameterUses, diagnostics);
    validateAnimationReferences(imports, clips, animators, diagnostics);

    auto requiredExtensions =
        requiredExtensionsReader
            ? detail::readRequiredExtensions(*requiredExtensionsReader, limits, diagnostics)
            : std::vector<RequiredExtension>{};
    OpaqueJson extensions;
    if (extensionsReader) {
        if (auto serialized = json::serialize(extensionsReader->value())) {
            extensions.canonicalText = std::move(*serialized);
        } else {
            addParseError(diagnostics, serialized.error());
        }
    }
    OpaqueJson canonicalSource;
    if (auto serialized = json::serialize(*parsed)) {
        canonicalSource.canonicalText = std::move(*serialized);
    } else {
        addParseError(diagnostics, serialized.error());
    }

    diagnostics.sortDeterministically();
    if (diagnostics.hasErrors() || !format || *format != "cuexis.chart" || !version ||
        *version != 4 || !chartId || !isUuidV7(*chartId) || !projection) {
        return ChartV4SourceResult{std::nullopt, std::move(diagnostics)};
    }
    return ChartV4SourceResult{
        ChartV4SourceDocument{
            ChartId{std::string{*chartId}}, *projection, std::move(canonicalSource),
            std::move(parameters), std::move(parameterUses), std::move(imports), std::move(clips),
            std::move(animators), std::move(requiredExtensions), std::move(extensions)},
        std::move(diagnostics)};
}

} // namespace cuexis::chart
