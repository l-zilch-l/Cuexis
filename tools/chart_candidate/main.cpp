#include <cuexis/chart/candidate_assembler.hpp>
#include <cuexis/chart/cxt_v2_loader.hpp>
#include <cuexis/chart/packed_chart_io.hpp>
#include <cuexis/cxc/cxc_candidate.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/tools/asset_publish.hpp>
#include <cuexis_internal/sha256.hpp>

#include <charconv>
#include <fstream>
#include <iostream>
#include <map>
#include <set>

namespace {
using cuexis::json::Value;
auto bytes(std::string_view text) -> std::vector<std::byte> {
    return {reinterpret_cast<const std::byte*>(text.data()),
            reinterpret_cast<const std::byte*>(text.data() + text.size())};
}
auto text(const std::filesystem::path& path) -> cuexis::core::Result<std::string> {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    const auto size = in.tellg();
    if (!in || size < 0 || size > 16 * 1024 * 1024) {
        return cuexis::core::unexpected(cuexis::core::Error{
            "candidate.tool.input", "Source unavailable or exceeds input bound"});
    }
    std::string data(static_cast<std::size_t>(size), '\0');
    in.seekg(0);
    in.read(data.data(), static_cast<std::streamsize>(size));
    if (!in) {
        return cuexis::core::unexpected(
            cuexis::core::Error{"candidate.tool.read", "Source read failed"});
    }
    return data;
}
int fail(std::string_view code, std::string_view detail) {
    std::cerr << code << ": " << detail << '\n';
    return 2;
}
int run(int argc, char** argv) {
    std::map<std::string, std::string, std::less<>> args;
    std::vector<cuexis::chart::CxtV2ParameterBinding> parameters;
    const std::set<std::string, std::less<>> allowed{
        "--base", "--metadata", "--template", "--binding", "--module", "--export", "--output"};
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc || std::string_view{argv[i + 1]}.empty()) {
            return fail("candidate.tool.arguments", "Options require explicit values");
        }
        if (std::string_view{argv[i]} == "--parameter") {
            std::string_view value{argv[i + 1]};
            const auto split = value.find('=');
            if (split == std::string_view::npos || split == 0) {
                return fail("candidate.tool.parameter", "Expected id=integer");
            }
            std::int64_t integer{};
            const auto number = value.substr(split + 1);
            auto parsed = std::from_chars(number.data(), number.data() + number.size(), integer);
            if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size()) {
                return fail("candidate.tool.parameter", "Invalid integer parameter");
            }
            parameters.push_back({std::string{value.substr(0, split)}, integer});
        } else if (!allowed.contains(argv[i]) || !args.emplace(argv[i], argv[i + 1]).second) {
            return fail("candidate.tool.arguments", "Unknown or duplicate option");
        }
    }
    if (args.size() != allowed.size()) {
        return fail("candidate.tool.arguments",
                    "Required: --base CXC --metadata Packed --template CXT --binding id "
                    "--module id --export id --output CXC [--parameter id=integer]");
    }
    auto base = cuexis::cxc::CxcPackageLoader::loadFile(args.at("--base"));
    if (!base.hasValue()) {
        return fail("candidate.tool.base", "Invalid fallback package");
    }
    if (base.package->manifest().canonicalExtensionsJson != "{}" ||
        !base.package->manifest().requiredExtensions.empty()) {
        return fail("candidate.tool.base_extensions", "Base must have no candidate extensions");
    }
    auto metadata = cuexis::chart::PackedChartReader::read(args.at("--metadata"));
    if (!metadata) {
        return fail(metadata.error().code(), metadata.error().message());
    }
    auto source = text(args.at("--template"));
    if (!source) {
        return fail(source.error().code(), source.error().message());
    }
    cuexis::chart::CxtV2Invocation invocation;
    invocation.chartId = metadata->chartId;
    invocation.bindingId = args.at("--binding");
    invocation.moduleId = args.at("--module");
    invocation.exportId = args.at("--export");
    invocation.parameters = std::move(parameters);
    auto expanded = cuexis::chart::CxtV2Loader::expand(*source, invocation);
    if (!expanded.chart || expanded.diagnostics.hasErrors()) {
        for (const auto& error : expanded.diagnostics.items()) {
            std::cerr << error.code() << ": " << error.message() << '\n';
        }
        return 2;
    }
    auto assembled =
        cuexis::chart::assembleCandidateChart(std::move(*metadata), std::span{&*expanded.chart, 1});
    if (!assembled) {
        return fail(assembled.error().code(), assembled.error().message());
    }
    auto encoded = cuexis::chart::packed::encode(*assembled);
    auto identity = cuexis::chart::packed::semanticIdentity(*assembled);
    if (!encoded || !identity) {
        return fail("candidate.tool.encode", "Validated candidate could not be encoded");
    }
    std::uint64_t requirements{};
    for (const auto& entity : assembled->entities) {
        requirements += entity.requirements.size();
    }
    const auto semantic = cuexis::chart::packed::semanticIdentityHex(*identity);
    const auto artifact = cuexis::core::detail::sha256Hex(*encoded);
    constexpr auto entryPath = "compiled/chart.packed";
    Value::Object entry{
        {"path", Value{entryPath}},
        {"kind", Value{"chart"}},
        {"encoding", Value{"packed-chart"}},
        {"playback", Value{true}},
        {"compiledSemanticIdentity", Value{semantic}},
        {"artifactIdentity", Value{artifact}},
        {"compilerProfile", Value{"candidate.static-tap-lanes4-v1"}},
        {"expandedEntityCount", Value{static_cast<std::uint64_t>(assembled->entities.size())}},
        {"expandedRequirementCount", Value{requirements}}};
    auto extension = cuexis::json::serialize(Value{Value::Object{
        {"cuexis.chart-entry.v1",
         Value{Value::Object{{"entries", Value{Value::Array{Value{std::move(entry)}}}}}}}}});
    if (!extension) {
        return fail(extension.error().code(), extension.error().message());
    }
    Value::Array features, resources;
    for (const auto& feature : assembled->features) {
        features.emplace_back(Value::Object{{"id", Value{feature.id}},
                                            {"version", Value{std::uint64_t{feature.version}}}});
    }
    for (const auto& resource : assembled->resourceClosure.resources) {
        resources.emplace_back(
            Value::Object{{"asset", Value{resource.assetId.value}},
                          {"use", Value{static_cast<std::uint64_t>(resource.use)}}});
    }
    auto report = cuexis::json::serialize(
        Value{Value::Object{{"format", Value{"cuexis.candidate-closure"}},
                            {"version", Value{std::uint64_t{1}}},
                            {"compilerProfile", Value{"candidate.static-tap-lanes4-v1"}},
                            {"semanticIdentity", Value{semantic}},
                            {"artifactIdentity", Value{artifact}},
                            {"requirements", Value{requirements}},
                            {"derivedFeatures", Value{std::move(features)}},
                            {"derivedResources", Value{std::move(resources)}},
                            {"gameplayV2ProductionBudget", Value{"not-accepted"}}}});
    if (!report) {
        return fail(report.error().code(), report.error().message());
    }
    cuexis::tools::PackagePublishRequest request;
    request.target = args.at("--output");
    auto manifestMetadata = cuexis::json::parse(*extension, {1024U * 1024U, 32, 4096});
    auto closureMetadata = cuexis::json::parse(*report, {16U * 1024U * 1024U, 64, 4096});
    if (!manifestMetadata || !closureMetadata) {
        return fail("candidate.tool.report", "Closure report cannot be embedded");
    }
    manifestMetadata->object()->emplace("cuexis.candidate-closure.v1", *closureMetadata);
    auto combinedMetadata = cuexis::json::serialize(*manifestMetadata);
    if (!combinedMetadata) {
        return fail(combinedMetadata.error().code(), combinedMetadata.error().message());
    }
    request.extensionsJson = *combinedMetadata;
    for (const auto& original : base.package->entries()) {
        if (original.path == "cuexis.cxc.json") {
            continue;
        }
        if (original.path == entryPath) {
            return fail("candidate.tool.path_conflict", "Base collides with derived output");
        }
        auto data = base.package->entryBytes(original.path);
        if (!data) {
            return fail("candidate.tool.base_entry", "Base entry unavailable");
        }
        if (original.path == base.package->manifest().projectPath) {
            auto project = cuexis::json::parse(
                std::string_view{reinterpret_cast<const char*>(data->data()), data->size()},
                {16U * 1024U * 1024U, 64, 4096});
            auto entryMetadata = cuexis::json::parse(*extension, {1024U * 1024U, 32, 4096});
            if (!project || !project->object() || !entryMetadata) {
                return fail("candidate.tool.project", "Project metadata cannot be composed");
            }
            auto* extensions = project->find("extensions");
            if (!extensions || !extensions->object() || extensions->find("cuexis.chart-entry.v1")) {
                return fail("candidate.tool.project", "Project extension conflicts with output");
            }
            extensions->object()->emplace("cuexis.chart-entry.v1",
                                          *entryMetadata->find("cuexis.chart-entry.v1"));
            auto serialized = cuexis::json::serialize(*project);
            if (!serialized) {
                return fail(serialized.error().code(), serialized.error().message());
            }
            request.entries.push_back({original.path, bytes(*serialized)});
        } else {
            request.entries.push_back({original.path, {data->begin(), data->end()}});
        }
    }
    request.entries.push_back({entryPath, std::move(*encoded)});
    auto candidate = cuexis::cxc::CxcWriter::write(
        {request.entries, request.requiredExtensions, request.extensionsJson});
    if (!candidate.hasValue()) {
        for (const auto& diagnostic : candidate.diagnostics.items()) {
            std::cerr << diagnostic.code() << ": " << diagnostic.message() << '\n';
        }
        return fail("candidate.tool.package", "Candidate package validation failed");
    }
    auto loaded = cuexis::cxc::CxcPackageLoader::loadMemory(*candidate.bytes);
    if (!loaded.hasValue() ||
        cuexis::cxc::validateCandidateChartExtension(*loaded.package).hasErrors()) {
        return fail("candidate.tool.closure", "Candidate package closure validation failed");
    }
    auto playbackSource =
        cuexis::playback::PlaybackSource::fromCxcMemoryEntry(*candidate.bytes, entryPath);
    if (!playbackSource) {
        return fail(playbackSource.error().code(), playbackSource.error().message());
    }
    cuexis::playback::PlaybackSession consumer;
    auto prepared =
        consumer.prepareLoad(std::move(*playbackSource),
                             assembled->mainMusic ? cuexis::playback::PlaybackMode::CuexisAudio
                                                  : cuexis::playback::PlaybackMode::ChartClock);
    if (!prepared) {
        return fail(prepared.error().code(), prepared.error().message());
    }
    const auto preparedIdentity = prepared->semanticIdentity();
    if (!preparedIdentity) {
        return fail("candidate.tool.prepared_identity", "Prepared identity is unavailable");
    }
    auto committed = consumer.commit(std::move(*prepared));
    if (!committed) {
        return fail(committed.error().code(), committed.error().message());
    }
    auto capabilities = consumer.capabilities();
    if (!capabilities) {
        return fail(capabilities.error().code(), capabilities.error().message());
    }
    Value::Array playbackCapabilities;
    for (const auto& capability : capabilities->ids) {
        playbackCapabilities.emplace_back(capability);
    }
    closureMetadata->object()->emplace("playbackCapabilities",
                                       Value{std::move(playbackCapabilities)});
    closureMetadata->object()->emplace(
        "preparedIdentity", Value{cuexis::core::detail::sha256Hex(preparedIdentity->sha256)});
    auto finalReport = cuexis::json::serialize(*closureMetadata);
    if (!finalReport) {
        return fail(finalReport.error().code(), finalReport.error().message());
    }
    *manifestMetadata->find("cuexis.candidate-closure.v1") = *closureMetadata;
    combinedMetadata = cuexis::json::serialize(*manifestMetadata);
    if (!combinedMetadata) {
        return fail(combinedMetadata.error().code(), combinedMetadata.error().message());
    }
    request.extensionsJson = *combinedMetadata;
    auto published = cuexis::tools::publishPackage(request);
    if (!published) {
        return fail(published.error().code(), published.error().message());
    }
    std::cout << *finalReport << '\n';
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& error) {
        return fail("candidate.tool.failure", error.what());
    }
}
