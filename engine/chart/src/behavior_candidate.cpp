#include <cuexis/chart/behavior_candidate.hpp>
#include <cuexis/chart/chart_v4_loader.hpp>
#include <cuexis/chart/chart_writer.hpp>
#include <cuexis/json/parse.hpp>
#include <map>
#include <stdexcept>

namespace cuexis::chart {
namespace {
struct Rejected {
    core::Error error;
};
void require(bool condition, std::string_view field, std::string_view message) {
    if (!condition)
        throw Rejected{
            core::Error{"chart.v5.behavior_candidate.invalid", std::string{message}}.withContext(
                "field.path", std::string{field})};
}
template <class T> auto take(core::Result<T> result) -> T {
    if (!result)
        throw Rejected{std::move(result.error())};
    return std::move(*result);
}
auto integer(const json::Value& value, std::string_view field) -> std::uint64_t {
    if (auto n = value.unsignedInteger())
        return *n;
    if (auto n = value.signedInteger(); n && *n >= 0)
        return static_cast<std::uint64_t>(*n);
    require(false, field, "Exact nonnegative JSON integer required");
    return 0;
}
auto member(json::Value& value, std::string_view name) -> json::Value& {
    auto* result = value.find(name);
    require(result != nullptr, name, "Required field missing");
    return *result;
}
auto array(json::Value& value, std::string_view name) -> json::Value::Array& {
    auto* result = member(value, name).array();
    require(result != nullptr, name, "Array required");
    return *result;
}
void diagnostics(const core::Diagnostics& errors) {
    if (!errors.hasErrors())
        return;
    for (const auto& item : errors.items())
        if (item.severity() == core::DiagnosticSeverity::Error)
            throw Rejected{
                core::Error{std::string{item.code()}, std::string{item.message()}}.withContext(
                    "field.path", std::string{item.fieldPath()})};
}
} // namespace
auto compileBehaviorCandidate(std::string_view source, const ChartLimits& limits)
    -> core::Result<CandidateRuntimeArtifact> try {
    auto value = take(
        json::parse(source, {limits.maxInputBytes, limits.maxNestingDepth, limits.maxStringBytes}));
    require(integer(member(value, "version"), "$/version") == 5, "$/version", "Version 5 required");
    for (const auto name :
         {"parameters", "templates", "animationClips", "animationTemplateImports"})
        require(array(value, name).empty(), name,
                "This behavior candidate does not implement templates, parameters or Clips");
    std::map<std::string, std::uint8_t, std::less<>> alphas;
    for (auto& object : array(value, "objects")) {
        auto& id = member(object, "id");
        require(id.string() != nullptr, "$/objects/id", "Object ID required");
        auto& components = member(object, "components");
        require(components.find("cuexis.animator") == nullptr,
                "$/objects/components/cuexis.animator",
                "Animator is outside this candidate profile");
        if (auto* renderable = components.find("cuexis.renderable")) {
            require(renderable->object() != nullptr, "$/objects/components/cuexis.renderable",
                    "Renderable object required");
            auto* alpha = renderable->find("alpha");
            const auto n =
                alpha ? integer(*alpha, "$/objects/components/cuexis.renderable/alpha") : 255;
            require(n <= 255, "$/objects/components/cuexis.renderable/alpha",
                    "alpha must be in [0,255]");
            require(alphas.emplace(*id.string(), static_cast<std::uint8_t>(n)).second,
                    "$/objects/id", "Duplicate Object identity");
            renderable->object()->erase("alpha");
        }
    }
    for (auto& behavior : array(value, "behaviors")) {
        require(integer(member(behavior, "version"), "$/behaviors/version") == 2,
                "$/behaviors/version", "v5 behavior.event requires version 2");
        auto& type = member(behavior, "type");
        require(type.string() && *type.string() == "behavior.event", "$/behaviors/type",
                "behavior.event required");
        member(behavior, "version") = json::Value{std::uint64_t{1}};
        for (auto& event : array(behavior, "events")) {
            auto& property = member(event, "property");
            require(property.string() != nullptr, "$/behaviors/events/property",
                    "Property required");
            require(*property.string() != "material.opacity", "$/behaviors/events/property",
                    "v5 uses render.alpha");
            if (*property.string() == "render.alpha") {
                for (const auto name : {"startValue", "endValue"}) {
                    auto& endpoint = member(event, name);
                    const auto n = integer(endpoint, "$/behaviors/events/alpha");
                    require(n <= 255, "$/behaviors/events/alpha",
                            "alpha endpoint must be in [0,255]");
                    endpoint = json::Value{static_cast<double>(n) / 255.0};
                }
                property = json::Value{"material.opacity"};
            }
        }
    }
    member(value, "version") = json::Value{std::uint64_t{4}};
    const auto loweredText = take(json::serialize(value));
    auto loaded = ChartV4Loader::load(loweredText, limits);
    diagnostics(loaded.diagnostics);
    require(loaded.hasValue(), "$", "Behavior source lowering failed");
    const auto canonical = take(ChartWriter::writeV4(*loaded.document, limits));
    auto resolved = ChartV4Resolver::resolve(*loaded.document, {}, {}, {}, limits);
    diagnostics(resolved.diagnostics);
    require(resolved.hasValue(), "$", "Behavior source resolution failed");
    auto compiled = ChartCompiler::compile(resolved.artifact->document.chart, limits);
    diagnostics(compiled.diagnostics);
    require(compiled.hasValue(), "$", "Behavior source compilation failed");
    CandidateRuntimeArtifact result{std::move(*compiled.runtime), {}};
    result.runtime.version = 5;
    for (auto& behavior : result.runtime.behaviors)
        behavior.version = 2;
    result.metadata.compilerProfile = std::string{behaviorCandidateProfile};
    std::string identity = "cuexis.chart.v5.behavior-candidate.1\n" + canonical;
    for (const auto& [id, alpha] : alphas)
        identity += id + ":" + std::to_string(alpha) + "\n";
    result.metadata.semanticIdentity = canonicalBytesIdentity(identity).sha256;
    for (const auto& requirement : resolved.artifact->resourceRequirements) {
        require(!requirement.uses.empty(), "$", "Resource use required");
        result.metadata.resourceClosure.resources.push_back(
            {requirement.assetId, requirement.uses.front() == ChartResourceUse::MainMusic
                                      ? CanonicalResourceUseKind::MainMusic
                                  : requirement.uses.front() == ChartResourceUse::RenderableMesh
                                      ? CanonicalResourceUseKind::RenderableMesh
                                      : CanonicalResourceUseKind::RenderableMaterial});
    }
    for (std::size_t i = 0; i < result.runtime.objects.size(); ++i) {
        auto& object = result.runtime.objects[i];
        CandidateRuntimeObjectMetadata metadata;
        metadata.runtimeIndex = i;
        metadata.identity = ExplicitEntityIdentity{object.id};
        metadata.executionId = object.id.value;
        if (object.parentIndex)
            metadata.parent =
                ExplicitEntityIdentity{result.runtime.objects[*object.parentIndex].id};
        if (object.components.renderable) {
            object.renderableOpacity = static_cast<double>(alphas.at(object.id.value)) / 255.0;
            metadata.renderableOpacity = object.renderableOpacity;
        }
        result.metadata.objects.push_back(std::move(metadata));
    }
    return result;
} catch (const Rejected& rejected) {
    return core::unexpected(rejected.error);
} catch (...) {
    return core::unexpected(core::Error{"chart.v5.behavior_candidate.invalid",
                                        "Behavior candidate compilation failed"});
}
} // namespace cuexis::chart
