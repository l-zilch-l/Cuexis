#include "gameplay_entry_internal.hpp"
#include <charconv>
#include <cuexis/chart/packed_chart_io.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/tools/asset_publish.hpp>
#include <cuexis/tools/gameplay_assembler_cli.hpp>
#include <cuexis/tools/gameplay_author.hpp>
#include <cuexis_internal/sha256.hpp>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <type_traits>

namespace {
using namespace cuexis;
using json::Value;
struct Rejection {
    core::Error error;
};
template <class T> auto checked(core::Result<T> result) -> T {
    if (!result)
        throw Rejection{std::move(result.error())};
    if constexpr (!std::is_void_v<T>)
        return std::move(*result);
}
void require(bool condition, std::string_view message) {
    if (!condition)
        throw Rejection{core::Error{"playback.gameplay.invalid", std::string{message}}};
}
auto bytes(std::string_view text) -> std::vector<std::byte> {
    return {reinterpret_cast<const std::byte*>(text.data()),
            reinterpret_cast<const std::byte*>(text.data() + text.size())};
}
auto read(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary | std::ios::ate};
    const auto end = input.tellg();
    require(input && end >= 0 &&
                static_cast<std::uintmax_t>(end) <= chart::ChartLimits{}.maxInputBytes,
            "Source unavailable or exceeds existing Chart input bound");
    std::string text(static_cast<std::size_t>(end), '\0');
    input.seekg(0);
    input.read(text.data(), static_cast<std::streamsize>(text.size()));
    require(input.good(), "Source read failed");
    return text;
}
auto limits(std::string_view text, std::size_t count) -> std::vector<std::size_t> {
    std::vector<std::size_t> result;
    while (!text.empty()) {
        const auto comma = text.find(',');
        const auto token = text.substr(0, comma);
        std::size_t number{};
        const auto parsed = std::from_chars(token.data(), token.data() + token.size(), number);
        require(parsed.ec == std::errc{} && parsed.ptr == token.data() + token.size() &&
                    number != 0,
                "Explicit positive integer budget required");
        result.push_back(number);
        require(result.size() <= count, "Too many budget components");
        if (comma == std::string_view::npos) {
            text = {};
            break;
        }
        text.remove_prefix(comma + 1);
        require(!text.empty(), "Trailing budget separator");
    }
    require(result.size() == count, "Wrong budget component count");
    return result;
}
auto packageAssets(const cxc::CxcPackage& package)
    -> std::vector<playback::PlaybackAssetDescriptor> {
    std::vector<playback::PlaybackAssetDescriptor> result;
    for (const auto& index : package.assetIndexes())
        for (const auto& asset : index.document.assets) {
            playback::PlaybackAssetType type;
            switch (asset.type) {
            case project::AssetType::Mesh:
                type = playback::PlaybackAssetType::Mesh;
                break;
            case project::AssetType::Material:
                type = playback::PlaybackAssetType::Material;
                break;
            case project::AssetType::Texture:
                type = playback::PlaybackAssetType::Texture;
                break;
            case project::AssetType::Audio:
                type = playback::PlaybackAssetType::Audio;
                break;
            case project::AssetType::Shader:
                type = playback::PlaybackAssetType::Shader;
                break;
            default:
                throw Rejection{core::Error{"playback.gameplay.invalid", "Unknown asset type"}};
            }
            result.push_back({asset.id, type, index.rootId, asset.source, asset.dependencies});
        }
    return result;
}
int run(int argc, char** argv) {
    const std::set<std::string, std::less<>> required{
        "--base",      "--foundation",           "--source",      "--source-kind",
        "--entry-id",  "--configuration",        "--output",      "--publish",
        "--test-only", "--configuration-budget", "--graph-budget"};
    const std::set<std::string, std::less<>> optional{"--binding", "--module", "--export"};
    std::map<std::string, std::string, std::less<>> args;
    std::vector<chart::CxtV2ParameterBinding> parameters;
    std::set<std::string> parameterIds;
    for (int i = 1; i < argc; i += 2) {
        require(i + 1 < argc && std::string_view{argv[i + 1]}.size() > 0,
                "Options require explicit values");
        const std::string option = argv[i];
        if (option == "--parameter") {
            const std::string_view value = argv[i + 1];
            const auto split = value.find('=');
            require(split != std::string_view::npos && split != 0, "Expected parameter id=i64");
            const auto token = value.substr(split + 1);
            std::int64_t integer{};
            const auto parsed = std::from_chars(token.data(), token.data() + token.size(), integer);
            require(parsed.ec == std::errc{} && parsed.ptr == token.data() + token.size(),
                    "Parameter must be an exact i64");
            auto id = std::string{value.substr(0, split)};
            require(parameterIds.insert(id).second, "Duplicate parameter");
            parameters.push_back({std::move(id), integer});
        } else
            require((required.contains(option) || optional.contains(option)) &&
                        args.emplace(option, argv[i + 1]).second,
                    "Unknown or duplicate option");
    }
    for (const auto& option : required)
        require(args.contains(option), "Missing required assembler option");
    require(args.at("--test-only") == "true", "Explicit test-only candidate execution required");
    const bool cxt = args.at("--source-kind") == "cxt";
    require(cxt || args.at("--source-kind") == "inline", "Source kind must be inline or cxt");
    for (const auto& option : optional)
        require(cxt == args.contains(option),
                "CXT requires binding/module/export; inline rejects them");
    require(cxt || parameters.empty(), "Inline has no invocation parameters");
    require(args.at("--publish") == "cxc" || args.at("--publish") == "filesystem",
            "Publish must be cxc or filesystem");
    const auto cb = limits(args.at("--configuration-budget"), 5);
    const auto gb = limits(args.at("--graph-budget"), 7);
    const playback::GameplayConfigurationDecodeBudget configBudget{cb[0], cb[1], cb[2],
                                                                   cb[3], cb[4], true};
    const playback::GameplayGraphDecodeBudget graphBudget{gb[0], gb[1], gb[2], gb[3],
                                                          gb[4], gb[5], gb[6]};
    auto configuration = checked(
        playback::decodeGameplayConfiguration(read(args.at("--configuration")), configBudget));
    auto foundation = checked(chart::PackedChartReader::read(args.at("--foundation")));
    auto base = cxc::CxcPackageLoader::loadFile(args.at("--base"));
    if (!base.hasValue()) {
        for (const auto& diagnostic : base.diagnostics.items())
            std::cerr << diagnostic.code() << ": " << diagnostic.message() << '\n';
        require(false, "Base CXC invalid");
    }
    require(base.package->manifest().requiredExtensions.empty() &&
                base.package->manifest().canonicalExtensionsJson == "{}",
            "Base must not contain compiled candidate extensions");
    tools::gameplay_author::AuthorCompileContext context;
    context.chart = {args.at("--entry-id"), std::move(foundation)};
    context.compilerProfileToken = "gameplay.author.t4-k4.v1";
    auto registry = checked(playback::gameplayCapabilities());
    for (const auto& capability : registry)
        context.capabilities.recognisedCapabilityIds.push_back(capability.id);
    context.capabilities.enabledCapabilityIds = configuration.enabledCapabilities;
    context.identityDeclarations = {{configuration.engine[0], configuration.engine[1],
                                     configuration.engine[2], configuration.engine[3],
                                     configuration.engine[4], configuration.engine[5]},
                                    {configuration.rulesetInterfaceProjection,
                                     configuration.rulesetModuleOrder, configuration.rulesetBuild},
                                    {configuration.session[0], configuration.session[1],
                                     configuration.session[2], configuration.session[3]}};
    if (cxt) {
        context.invocation = chart::CxtV2Invocation{};
        context.invocation->chartId = context.chart.foundation.chartId;
        context.invocation->bindingId = args.at("--binding");
        context.invocation->moduleId = args.at("--module");
        context.invocation->exportId = args.at("--export");
        context.invocation->parameters = parameters;
    }
    const auto authorSource = read(args.at("--source"));
    auto artifact = checked(tools::gameplay_author::compile(
        authorSource,
        cxt ? json::gameplay::SourceKind::cxtModule : json::gameplay::SourceKind::chartInline,
        context));
    const gameplay_packed::EncodeRequest encode{artifact.chart,
                                                artifact.gameplay,
                                                artifact.owners,
                                                artifact.profiles,
                                                artifact.additionalPatterns,
                                                artifact.additionalMeasures,
                                                3};
    auto packed = checked(gameplay_packed::encode(encode));
    auto graph = checked(gameplay_packed::encodeGraph(encode, {gb[0], gb[2], gb[5], true}));
    auto content = checked(playback::GameplayContent::fromPacked(packed, configuration));
    auto graphContent =
        checked(playback::GameplayContent::fromGraph(graph, configuration, graphBudget));
    static_cast<void>(graphContent);
    const auto assets = packageAssets(*base.package);
    constexpr auto packedPath = "compiled/gameplay.packed";
    constexpr auto graphPath = "compiled/gameplay.graph.json";
    const auto graphBytes = bytes(graph);
    auto packedMetadata = checked(playback::detail::gameplayEntryMetadata(
        content, configuration, packedPath, "packed-chart", packed, assets));
    auto graphMetadata = checked(playback::detail::gameplayEntryMetadata(
        content, configuration, graphPath, "gameplay-graph", graphBytes, assets));
    Value directory{Value::Object{{"entries", Value{Value::Array{packedMetadata, graphMetadata}}}}};
    const auto& counts = artifact.gameplay.assembled().contentProfile;
    Value::Object countFields;
#define COUNT(name) countFields.emplace(#name, Value{counts.name})
    COUNT(requirements);
    COUNT(resources);
    COUNT(exclusiveRelations);
    COUNT(relationMembers);
    COUNT(solverProfiles);
    COUNT(factBindings);
    COUNT(judgementDomains);
    COUNT(emissions);
    COUNT(mergedDeclarations);
    COUNT(declaredCapabilities);
    COUNT(derivedCapabilities);
    COUNT(declaredFeatures);
    COUNT(derivedFeatures);
    COUNT(patternNodes);
    COUNT(measureComponents);
    COUNT(phaseDeclarations);
    COUNT(requiredFeatureRefs);
    COUNT(requiredCapabilityRefs);
    COUNT(diagnosticMapEntries);
#undef COUNT
    Value report{Value::Object{
        {"format", Value{"cuexis.gameplay-closure"}},
        {"version", Value{std::uint64_t{1}}},
        {"entries", *directory.find("entries")},
        {"counts", Value{std::move(countFields)}},
        {"sourceArtifactIdentity", Value{core::detail::sha256Hex(bytes(authorSource))}},
        {"productionBudgetAccepted", Value{false}},
        {"actualPrepareValidated", Value{true}}}};
    cxc::CxcWriteRequest request;
    for (const auto& entry : base.package->entries()) {
        if (entry.path == "cuexis.cxc.json")
            continue;
        require(entry.path != packedPath && entry.path != graphPath,
                "Output collides with base entry");
        auto data = base.package->entryBytes(entry.path);
        require(data.has_value(), "Base entry missing");
        if (entry.path == base.package->manifest().projectPath) {
            auto projectValue = checked(json::parse(
                {reinterpret_cast<const char*>(data->data()), data->size()},
                {project::ProjectLimits{}.maxInputBytes, project::ProjectLimits{}.maxNestingDepth,
                 project::ProjectLimits{}.maxStringBytes}));
            auto* extensions = projectValue.find("extensions");
            require(extensions && extensions->object() &&
                        !extensions->find("cuexis.gameplay-entry.v1"),
                    "Base project Gameplay extension conflict");
            extensions->object()->emplace("cuexis.gameplay-entry.v1", directory);
            request.entries.push_back({entry.path, bytes(checked(json::serialize(projectValue)))});
        } else
            request.entries.push_back({entry.path, {data->begin(), data->end()}});
    }
    request.entries.push_back({packedPath, std::move(packed)});
    request.entries.push_back({graphPath, graphBytes});
    request.extensionsJson =
        checked(json::serialize(Value{Value::Object{{"cuexis.gameplay-closure.v1", report}}}));
    auto candidate = cxc::CxcWriter::write(request);
    if (!candidate.hasValue()) {
        for (const auto& diagnostic : candidate.diagnostics.items())
            std::cerr << diagnostic.code() << ": " << diagnostic.message() << '\n';
        require(false, "Complete Gameplay package validation failed");
    }
    for (const auto path : {packedPath, graphPath})
        for (const auto intent : {playback::GameplayPrepareIntent::Presentation,
                                  playback::GameplayPrepareIntent::GameplayOnly}) {
            auto source = checked(playback::PlaybackSource::fromCxcMemoryGameplayEntry(
                *candidate.bytes, path, configuration, configBudget, graphBudget, intent));
            playback::PlaybackSession consumer;
            auto prepared = checked(consumer.prepareLoad(
                std::move(source), artifact.chart.mainMusic ? playback::PlaybackMode::HostClock
                                                            : playback::PlaybackMode::ChartClock));
            checked(consumer.commit(std::move(prepared)));
            auto result = checked(consumer.queryGameplay());
            require(checked(result.factCount()) == 0, "Prepare must not emit a Fact");
        }
    const auto reportText = checked(json::serialize(report));
    if (args.at("--publish") == "cxc") {
        tools::PackagePublishRequest publication;
        publication.target = args.at("--output");
        publication.entries = std::move(request.entries);
        publication.extensionsJson = request.extensionsJson;
        checked(tools::publishPackage(publication));
    } else {
        tools::GenerationPublishRequest publication;
        publication.root = args.at("--output");
        publication.generationId = core::detail::sha256Hex(*candidate.bytes);
        for (auto& entry : request.entries)
            publication.entries.push_back({entry.path, std::move(entry.bytes)});
        publication.provenanceRecords.push_back({"gameplay-closure.json", bytes(reportText)});
        checked(tools::publishAndAdoptGeneration(publication));
    }
    std::cout << reportText << '\n';
    return 0;
}
} // namespace
int cuexis::tools::runGameplayAssembler(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const Rejection& rejected) {
        std::cerr << rejected.error.code() << ": " << rejected.error.message() << '\n';
        for (const auto& context : rejected.error.context())
            std::cerr << context.key << "=" << context.value << '\n';
    } catch (const std::exception& exception) {
        std::cerr << "playback.gameplay.invalid: " << exception.what() << '\n';
    } catch (...) {
        std::cerr << "playback.gameplay.invalid: assembler failure\n";
    }
    return 2;
}
