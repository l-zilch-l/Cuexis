#include "gameplay_entry_internal.hpp"
#include "gameplay_internal.hpp"
#include <algorithm>
#include <cuexis/json/parse.hpp>
#include <cuexis_internal/portable_path.hpp>
#include <cuexis_internal/sha256.hpp>
namespace cuexis::playback::detail {
namespace {
using json::Value;
auto invalid(std::string message) -> core::Error {
    return core::Error{"playback.gameplay.invalid", std::move(message)}
        .withContext("category", "invalid_relation")
        .withContext("severity", "error")
        .withContext("faulted", "false");
}
auto strings(std::span<const std::string> input) -> Value {
    Value::Array result;
    for (const auto& s : input)
        result.emplace_back(s);
    return Value{std::move(result)};
}
auto capabilities(const auto& input) -> Value {
    Value::Array result;
    for (const auto& c : input)
        result.emplace_back(Value::Array{Value{c.capabilityId}, Value{c.revision}});
    return Value{std::move(result)};
}
} // namespace
auto gameplayEntryMetadata(const GameplayContent& content, const GameplayConfiguration& config,
                           std::string_view path, std::string_view kind,
                           std::span<const std::byte> bytes,
                           std::span<const PlaybackAssetDescriptor> assets)
    -> core::Result<Value> try {
    const auto* storage = GameplayAccess::content(content);
    if (!storage || (kind != "packed-chart" && kind != "gameplay-graph"))
        return core::unexpected(invalid("Invalid Gameplay entry content or kind"));
    const auto& capsule = storage->capsule;
    const auto& graph = capsule.gameplay.assembled().graph;
    auto preimage = gameplay_packed::semanticPreimage({capsule.chart, capsule.gameplay,
                                                       capsule.owners, capsule.profiles,
                                                       capsule.patterns, capsule.measures, 3});
    if (!preimage)
        return core::unexpected(projectGameplayError(std::move(preimage.error())));
    auto configuration = encodeGameplayConfiguration(config);
    if (!configuration)
        return core::unexpected(std::move(configuration.error()));
    // This generated metadata tree contains no caller-controlled numeric allocation requests.
    auto configurationValue =
        json::parse(*configuration, {configuration->size(), 32, configuration->size()});
    if (!configurationValue)
        return core::unexpected(std::move(configurationValue.error()));
    Value::Array features, gameplayResources, chartResources, assetValues;
    for (const auto& f : graph.derivedFeatures.features)
        features.emplace_back(f.featureId);
    for (const auto& r : graph.resourceClosure.resources)
        gameplayResources.emplace_back(r.resourceId);
    for (const auto& r : capsule.chart.resourceClosure.resources)
        chartResources.emplace_back(
            Value::Array{Value{r.assetId.value}, Value{static_cast<std::uint64_t>(r.use)}});
    std::vector<const PlaybackAssetDescriptor*> sorted;
    for (const auto& a : assets)
        sorted.push_back(&a);
    std::sort(sorted.begin(), sorted.end(),
              [](const auto* a, const auto* b) { return a->id < b->id; });
    for (const auto* a : sorted)
        assetValues.emplace_back(Value::Object{{"id", Value{a->id}},
                                               {"type", Value{static_cast<std::uint64_t>(a->type)}},
                                               {"rootId", Value{a->rootId}},
                                               {"logicalSource", Value{a->logicalSource}},
                                               {"dependencies", strings(a->dependencies)}});
    auto bindings = *configurationValue->find("configuration")->find("bindings");
    return Value{Value::Object{
        {"format", Value{"cuexis.gameplay-entry"}},
        {"version", Value{std::uint64_t{1}}},
        {"path", Value{std::string{path}}},
        {"playback", Value{true}},
        {"entryKind", Value{std::string{kind}}},
        {"encoding", Value{kind == "packed-chart" ? "capsule.3" : "graph.1"}},
        {"compilerProfile", Value{graph.sourceClosure.compilerProfileToken}},
        {"expandedEntityCount", Value{static_cast<std::uint64_t>(capsule.chart.entities.size())}},
        {"expandedRequirementCount", Value{static_cast<std::uint64_t>(graph.requirements.size())}},
        {"compiledSemanticIdentity", Value{core::detail::sha256Hex(*preimage)}},
        {"artifactIdentity", Value{core::detail::sha256Hex(bytes)}},
        {"rulesetBinding", std::move(*configurationValue)},
        {"capabilityClosure",
         Value{Value::Object{{"declared", capabilities(graph.declaredCapabilities.capabilities)},
                             {"derived", capabilities(graph.derivedCapabilities.capabilities)},
                             {"requiredFeatures", Value{std::move(features)}}}}},
        {"resourcePresentationClosure",
         Value{Value::Object{{"gameplayResources", Value{std::move(gameplayResources)}},
                             {"chartResources", Value{std::move(chartResources)}},
                             {"assets", Value{std::move(assetValues)}},
                             {"bindings", std::move(bindings)}}}},
        {"sourceOf",
         Value{Value::Object{{"kind", Value{"not-packaged"}},
                             {"documentIds", strings(graph.sourceClosure.sourceDocumentIds)}}}}}};
} catch (...) {
    return core::unexpected(invalid("Gameplay metadata allocation failed"));
}

auto parseGameplayEntryMetadata(std::string_view text, GameplayConfigurationDecodeBudget b)
    -> core::Result<Value> try {
    if (!b.testOnly || !b.maxBytes || !b.maxDepth || !b.maxStringBytes || !b.maxValues ||
        !b.maxContainerElements)
        return core::unexpected(
            core::Error{"capability.budget_insufficient", "Explicit metadata budgets required"}
                .withContext("category", "budget_exceeded")
                .withContext("severity", "error")
                .withContext("faulted", "false"));
    auto parsed = json::parseBounded(
        text, {{b.maxBytes, b.maxDepth, b.maxStringBytes}, b.maxValues, b.maxContainerElements});
    if (!parsed) {
        const auto code = parsed.error().code();
        const bool limit = code == "json.parse.size_limit" || code == "json.parse.depth_limit" ||
                           code == "json.parse.string_limit" || code == "json.parse.value_limit" ||
                           code == "json.parse.element_limit";
        return core::unexpected(
            core::Error{limit ? "capability.budget_insufficient" : "playback.gameplay.invalid",
                        std::string{parsed.error().message()}}
                .withContext("category", limit ? "budget_exceeded" : "invalid_relation")
                .withContext("sourceCode", std::string{code})
                .withContext("severity", "error")
                .withContext("faulted", "false"));
    }
    constexpr std::array keys{"format",
                              "version",
                              "path",
                              "playback",
                              "entryKind",
                              "encoding",
                              "compilerProfile",
                              "expandedEntityCount",
                              "expandedRequirementCount",
                              "compiledSemanticIdentity",
                              "artifactIdentity",
                              "rulesetBinding",
                              "capabilityClosure",
                              "resourcePresentationClosure",
                              "sourceOf"};
    if (!parsed->object() || parsed->object()->size() != keys.size())
        return core::unexpected(invalid("Gameplay metadata has missing or unknown fields"));
    for (const auto* key : keys)
        if (!parsed->find(key))
            return core::unexpected(invalid("Gameplay metadata field missing"));
    const auto stringIs = [&](const char* key, std::string_view expected) {
        const auto* value = parsed->find(key)->string();
        return value && *value == expected;
    };
    const auto* version = parsed->find("version")->unsignedInteger();
    const auto* playback = parsed->find("playback")->boolean();
    if (!stringIs("format", "cuexis.gameplay-entry") || !version || *version != 1 || !playback ||
        !*playback ||
        !(stringIs("entryKind", "packed-chart") && stringIs("encoding", "capsule.3")) &&
            !(stringIs("entryKind", "gameplay-graph") && stringIs("encoding", "graph.1")))
        return core::unexpected(invalid("Unsupported Gameplay metadata header"));
    for (const auto* key : {"artifactIdentity", "compiledSemanticIdentity"}) {
        const auto* hash = parsed->find(key)->string();
        if (!hash || hash->size() != 64 || !std::all_of(hash->begin(), hash->end(), [](char c) {
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
            }))
            return core::unexpected(invalid("Gameplay metadata identity must be lowercase SHA256"));
    }
    return parsed;
} catch (...) {
    return core::unexpected(invalid("Gameplay metadata parsing failed"));
}
} // namespace cuexis::playback::detail
