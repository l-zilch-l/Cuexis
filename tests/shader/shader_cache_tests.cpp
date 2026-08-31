#include <cuexis/shader/shader_cache.hpp>
#include <cuexis/shader/shader_compiler.hpp>
#include <cuexis/shader/shader_diagnostics.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view kVertex =
    "#version 450\n"
    "layout(std140, set = 0, binding = 0) uniform CuexisObject {\n"
    "    mat4 world;\n"
    "    mat4 viewProjection;\n"
    "    vec3 tint;\n"
    "    float opacity;\n"
    "} cuexisObject;\n"
    "layout(location = 0) in vec3 aPosition;\n"
    "void main() {\n"
    "    gl_Position = cuexisObject.viewProjection * cuexisObject.world * vec4(aPosition, 1.0);\n"
    "}\n";

constexpr std::string_view kFragment =
    "#version 450\n"
    "layout(std140, set = 0, binding = 0) uniform CuexisObject {\n"
    "    mat4 world;\n"
    "    mat4 viewProjection;\n"
    "    vec3 tint;\n"
    "    float opacity;\n"
    "} cuexisObject;\n"
    "layout(location = 0) out vec4 outColor;\n"
    "void main() {\n"
    "    outColor = vec4(cuexisObject.tint, cuexisObject.opacity);\n"
    "}\n";

[[nodiscard]] auto passthroughRequest() -> cuexis::shader::ShaderCompileRequest {
    return cuexis::shader::ShaderCompileRequest{
        .vertexSource = kVertex,
        .fragmentSource = kFragment,
    };
}

[[nodiscard]] auto workRoot(std::string_view name) -> std::filesystem::path {
    const auto root =
        std::filesystem::temp_directory_path() / "cuexis-s5f-shader-cache" / std::string{name};
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

[[nodiscard]] auto makeIdentity() -> std::array<std::uint8_t, 32> {
    return cuexis::shader::hashStandaloneSourceIdentity(kVertex, kFragment);
}

[[nodiscard]] auto makeKey(const std::array<std::uint8_t, 32>& identity = makeIdentity())
    -> cuexis::shader::ShaderCacheKeyInput {
    return cuexis::shader::ShaderCacheKeyInput{.sourceIdentity = identity};
}

} // namespace

TEST_CASE("S5-F freezes cache diagnostic codes and tool identities", "[shader][s5-f][cache]") {
    CHECK(cuexis::shader::diagnosticCacheMissing == "shader.cache.missing");
    CHECK(cuexis::shader::diagnosticCacheKeyInvalid == "shader.cache.key_invalid");
    CHECK(cuexis::shader::diagnosticCacheToolMismatch == "shader.cache.tool_mismatch");
    CHECK(cuexis::shader::diagnosticHotReloadFailed == "shader.hot_reload.failed");
    CHECK(cuexis::shader::cacheMagic == "CXSCCH01");
    CHECK(cuexis::shader::cacheVersionV1 == 1);

    const auto tools = cuexis::shader::currentToolVersions();
    REQUIRE(tools.size() == 4);
    CHECK(tools[0].name == "glslang");
    CHECK(tools[1].name == "shaderc");
    CHECK(tools[2].name == "spirv-cross");
    CHECK(tools[3].name == "spirv-tools");
    CHECK_FALSE(tools[0].version.empty());
}

TEST_CASE("S5-F cache key changes with identity, profile, keyword, entry, and tools",
          "[shader][s5-f][key]") {
    const auto identity = makeIdentity();
    const auto base = cuexis::shader::encodeCacheKey({.sourceIdentity = identity});
    auto otherIdentity = identity;
    otherIdentity[0] ^= 0xFF;
    CHECK(cuexis::shader::encodeCacheKey({.sourceIdentity = otherIdentity}) != base);

    CHECK(cuexis::shader::encodeCacheKey(
              {.sourceIdentity = identity, .importerProfile = "cuexis.importer.other.v1"}) != base);

    const std::string_view keyword[] = {"PREMULTIPLY"};
    CHECK(cuexis::shader::encodeCacheKey(
              {.sourceIdentity = identity, .selectedKeywords = keyword}) != base);
    CHECK(cuexis::shader::encodeCacheKey({.sourceIdentity = identity, .vertexEntry = "vertMain"}) !=
          base);

    auto tools = cuexis::shader::currentToolVersions();
    tools[1].version = "0.0.0";
    CHECK(cuexis::shader::encodeCacheKey({.sourceIdentity = identity, .tools = tools}) != base);
    CHECK(cuexis::shader::encodeCacheKey({.sourceIdentity = identity}) == base);
}

TEST_CASE("A2 cache key normalizes profiles, keywords, entries, and tools",
          "[shader][a2][key][normalization]") {
    const auto identity = makeIdentity();
    const std::string_view targetsUnordered[] = {
        cuexis::shader::targetProfileSpirvV1,
        cuexis::shader::targetProfileGlsl330V1,
        cuexis::shader::targetProfileSpirvV1,
    };
    const std::string_view targetsSorted[] = {
        cuexis::shader::targetProfileGlsl330V1,
        cuexis::shader::targetProfileSpirvV1,
    };
    CHECK(cuexis::shader::encodeCacheKey(
              {.sourceIdentity = identity, .targetProfiles = targetsUnordered}) ==
          cuexis::shader::encodeCacheKey(
              {.sourceIdentity = identity, .targetProfiles = targetsSorted}));
    const std::string_view alternateTarget[] = {cuexis::shader::targetProfileSpirvV1};
    CHECK(cuexis::shader::encodeCacheKey(
              {.sourceIdentity = identity, .targetProfiles = alternateTarget}) !=
          cuexis::shader::encodeCacheKey(
              {.sourceIdentity = identity, .targetProfiles = targetsSorted}));

    const std::string_view keywordsUnordered[] = {"ZETA", "ALPHA", "ZETA"};
    const std::string_view keywordsSorted[] = {"ALPHA", "ZETA"};
    const std::string_view keywordsCaseChanged[] = {"alpha", "ZETA"};
    CHECK(cuexis::shader::encodeCacheKey(
              {.sourceIdentity = identity, .selectedKeywords = keywordsUnordered}) ==
          cuexis::shader::encodeCacheKey(
              {.sourceIdentity = identity, .selectedKeywords = keywordsSorted}));
    CHECK(cuexis::shader::encodeCacheKey(
              {.sourceIdentity = identity, .selectedKeywords = keywordsSorted}) !=
          cuexis::shader::encodeCacheKey(
              {.sourceIdentity = identity, .selectedKeywords = keywordsCaseChanged}));

    auto tools = cuexis::shader::currentToolVersions();
    auto reversedTools = tools;
    std::reverse(reversedTools.begin(), reversedTools.end());
    CHECK(cuexis::shader::encodeCacheKey({.sourceIdentity = identity, .tools = tools}) ==
          cuexis::shader::encodeCacheKey({.sourceIdentity = identity, .tools = reversedTools}));

    auto changedTool = tools;
    REQUIRE_FALSE(changedTool.empty());
    changedTool.front().version += ".changed";
    CHECK(cuexis::shader::encodeCacheKey({.sourceIdentity = identity, .tools = changedTool}) !=
          cuexis::shader::encodeCacheKey({.sourceIdentity = identity, .tools = tools}));

    CHECK(
        cuexis::shader::encodeCacheKey({.sourceIdentity = identity, .fragmentEntry = "fragMain"}) !=
        cuexis::shader::encodeCacheKey({.sourceIdentity = identity}));

    CHECK(cuexis::shader::encodeCacheKey({.sourceIdentity = identity,
                                          .importerProfile = "",
                                          .vertexEntry = "",
                                          .fragmentEntry = ""}) ==
          cuexis::shader::encodeCacheKey({.sourceIdentity = identity}));
}

TEST_CASE("A2 cache key filename uses lowercase hexadecimal", "[shader][a2][key][filename]") {
    std::array<std::uint8_t, 32> bytes{};
    bytes[0] = 0xAB;
    bytes[1] = 0xCD;
    const auto filename = cuexis::shader::cacheFileName(bytes);
    REQUIRE(filename.size() == 64U + cuexis::shader::cacheFileExtension.size());
    CHECK(filename.ends_with(cuexis::shader::cacheFileExtension));
    for (const auto character : filename.substr(0, 64)) {
        CHECK(((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f')));
    }
}

TEST_CASE("A2 cache rejects a nonempty keyword set without semantic source identity",
          "[shader][a2][key][red]") {
    cuexis::shader::ShaderCacheRecord record;
    record.selectedKeywords.emplace_back("FEATURE");

    const auto encoded = cuexis::shader::encodeCache(record);
    REQUIRE_FALSE(encoded.has_value());
    CHECK(encoded.error().code() == cuexis::shader::diagnosticCacheKeyInvalid);
}

TEST_CASE("A2 cache store rejects an invalid record without creating its directory",
          "[shader][a2][key][store][red]") {
    const auto root = workRoot("invalid-store");
    std::filesystem::remove_all(root);

    cuexis::shader::ShaderCacheRecord record;
    record.selectedKeywords.emplace_back("FEATURE");
    cuexis::shader::ShaderCacheStore store{root};
    const auto stored = store.store(record);
    REQUIRE_FALSE(stored.has_value());
    CHECK(stored.error().code() == cuexis::shader::diagnosticCacheKeyInvalid);
    CHECK_FALSE(std::filesystem::exists(root));
}

TEST_CASE("A2 pipeline rejects request keywords when the key omits semantic identity",
          "[shader][a2][key][pipeline][red]") {
    const auto root = workRoot("half-empty-key");
    const std::string_view declaredKeywords[] = {"FEATURE"};
    const std::string_view selectedKeywords[] = {"FEATURE"};
    auto request = passthroughRequest();
    request.declaredKeywords = declaredKeywords;
    request.selectedKeywords = selectedKeywords;

    cuexis::shader::ShaderPipelineCache cache{root};
    const auto prepared = cache.prepareCandidate(request, {}, true);
    REQUIRE_FALSE(prepared.has_value());
    CHECK(prepared.error().code() == cuexis::shader::diagnosticCacheKeyInvalid);
    CHECK(cache.active() == nullptr);
    CHECK(cache.candidate() == nullptr);
    CHECK(std::filesystem::is_empty(root));
}

TEST_CASE("S5-F encodes and decodes CXSCCH01 with identical bytes", "[shader][s5-f][roundtrip]") {
    const auto compiled = cuexis::shader::ShaderCompiler::compile(passthroughRequest());
    REQUIRE(compiled.has_value());

    cuexis::shader::ShaderCacheRecord record;
    record.sourceIdentity = makeIdentity();
    record.artifact = *compiled;
    const auto first = cuexis::shader::encodeCache(record);
    REQUIRE(first.has_value());
    REQUIRE(first->size() >= 24);
    CHECK(std::string_view{reinterpret_cast<const char*>(first->data()), 8} == "CXSCCH01");

    const auto decoded = cuexis::shader::decodeCache(*first);
    REQUIRE(decoded.has_value());
    CHECK(decoded->sourceIdentity == record.sourceIdentity);
    CHECK(decoded->artifact == record.artifact);
    CHECK(decoded->tools == cuexis::shader::currentToolVersions());

    const auto second = cuexis::shader::encodeCache(*decoded);
    REQUIRE(second.has_value());
    CHECK(*first == *second);
}

TEST_CASE("S5-F store and load use an explicit directory and do not scan",
          "[shader][s5-f][store]") {
    const auto root = workRoot("store");
    const auto compiled = cuexis::shader::ShaderCompiler::compile(passthroughRequest());
    REQUIRE(compiled.has_value());

    cuexis::shader::ShaderCacheStore store{root};
    cuexis::shader::ShaderCacheRecord record;
    record.sourceIdentity = makeIdentity();
    record.artifact = *compiled;
    const auto path = store.store(record);
    REQUIRE(path.has_value());
    CHECK(path->parent_path() == root);
    CHECK(std::filesystem::is_regular_file(*path));

    const auto nested = root / "nested";
    std::filesystem::create_directories(nested);
    std::ofstream{nested / "ignored.bin"} << "nope";

    const auto loaded = store.load(makeKey());
    REQUIRE(loaded.has_value());
    CHECK(loaded->artifact == *compiled);

    auto missingKey = makeKey();
    missingKey.sourceIdentity[1] ^= 0x7F;
    const auto missing = store.load(missingKey);
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error().code() == cuexis::shader::diagnosticCacheMissing);
}

TEST_CASE("S5-F rejects a corrupt envelope and a tool mismatch without reuse",
          "[shader][s5-f][mismatch]") {
    const auto compiled = cuexis::shader::ShaderCompiler::compile(passthroughRequest());
    REQUIRE(compiled.has_value());
    cuexis::shader::ShaderCacheRecord record;
    record.sourceIdentity = makeIdentity();
    record.artifact = *compiled;
    auto bytes = cuexis::shader::encodeCache(record);
    REQUIRE(bytes.has_value());
    (*bytes)[0] = std::byte{'X'};
    const auto corrupt = cuexis::shader::decodeCache(*bytes);
    REQUIRE_FALSE(corrupt.has_value());
    CHECK(corrupt.error().code() == cuexis::shader::diagnosticCacheKeyInvalid);

    auto stale = record;
    stale.tools = cuexis::shader::currentToolVersions();
    REQUIRE_FALSE(stale.tools.empty());
    stale.tools[0].version = "0.0.0";
    const auto encodedStale = cuexis::shader::encodeCache(stale);
    REQUIRE(encodedStale.has_value());
    const auto decodedStale = cuexis::shader::decodeCache(*encodedStale);
    REQUIRE(decodedStale.has_value());
    const auto adopted = cuexis::shader::adoptCacheRecord(*decodedStale);
    REQUIRE_FALSE(adopted.has_value());
    CHECK(adopted.error().code() == cuexis::shader::diagnosticCacheToolMismatch);

    const auto root = workRoot("mismatch");
    cuexis::shader::ShaderCacheStore store{root};
    const auto stored = store.store(stale);
    REQUIRE(stored.has_value());
    auto lookup = makeKey();
    lookup.tools = stale.tools;
    const auto loaded = store.load(lookup);
    REQUIRE_FALSE(loaded.has_value());
    CHECK(loaded.error().code() == cuexis::shader::diagnosticCacheToolMismatch);
}

TEST_CASE("S5-F rejects truncated cache envelopes without publishing a candidate",
          "[shader][s5-f][cache][rollback][branch-coverage]") {
    const auto compiled = cuexis::shader::ShaderCompiler::compile(passthroughRequest());
    REQUIRE(compiled.has_value());
    cuexis::shader::ShaderCacheRecord record;
    record.sourceIdentity = makeIdentity();
    record.artifact = *compiled;
    const auto encoded = cuexis::shader::encodeCache(record);
    REQUIRE(encoded.has_value());

    auto truncated = *encoded;
    truncated.resize(12U);
    const auto rejected = cuexis::shader::decodeCache(truncated);
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code() == cuexis::shader::diagnosticCacheKeyInvalid);

    const auto root = workRoot("truncated-candidate");
    cuexis::shader::ShaderPipelineCache cache{root};
    REQUIRE(cache.prepareCandidate(passthroughRequest(), makeKey(), true).has_value());
    cache.activate();
    REQUIRE(cache.active() != nullptr);
    const auto activeKey = cache.active()->key;
    cuexis::shader::ShaderCompileRequest bad = passthroughRequest();
    bad.vertexSource = "#version 450\n#include \"stolen.glsl\"\nvoid main() {}\n";
    const auto badIdentity =
        cuexis::shader::hashStandaloneSourceIdentity(bad.vertexSource, bad.fragmentSource);
    CHECK_FALSE(cache.prepareCandidate(bad, makeKey(badIdentity), true).has_value());
    REQUIRE(cache.active() != nullptr);
    CHECK(cache.active()->key == activeKey);
    CHECK(cache.candidate() == nullptr);
}

TEST_CASE("S5-F deleting a cache file rebuilds identical bytes", "[shader][s5-f][rebuild]") {
    const auto root = workRoot("rebuild");
    const auto compiled = cuexis::shader::ShaderCompiler::compile(passthroughRequest());
    REQUIRE(compiled.has_value());
    cuexis::shader::ShaderCacheStore store{root};
    cuexis::shader::ShaderCacheRecord record;
    record.sourceIdentity = makeIdentity();
    record.artifact = *compiled;
    const auto firstPath = store.store(record);
    REQUIRE(firstPath.has_value());
    std::ifstream firstFile{*firstPath, std::ios::binary};
    std::vector<char> firstBytes{std::istreambuf_iterator<char>{firstFile}, {}};
    firstFile.close();
    std::filesystem::remove(*firstPath);

    const auto secondPath = store.store(record);
    REQUIRE(secondPath.has_value());
    CHECK(*secondPath == *firstPath);
    std::ifstream secondFile{*secondPath, std::ios::binary};
    std::vector<char> secondBytes{std::istreambuf_iterator<char>{secondFile}, {}};
    CHECK(firstBytes == secondBytes);
}

TEST_CASE("S5-F pipeline compile-disabled miss does not compile and keeps no pipeline",
          "[shader][s5-f][pipeline]") {
    const auto root = workRoot("miss");
    cuexis::shader::ShaderPipelineCache cache{root};
    const auto prepared = cache.prepareCandidate(passthroughRequest(), makeKey(), false);
    REQUIRE_FALSE(prepared.has_value());
    CHECK(prepared.error().code() == cuexis::shader::diagnosticCacheMissing);
    CHECK(cache.active() == nullptr);
    CHECK(cache.candidate() == nullptr);
}

TEST_CASE("S5-F pipeline worker compile plus noexcept swap; failed reload keeps active",
          "[shader][s5-f][hot-reload]") {
    const auto root = workRoot("reload");
    cuexis::shader::ShaderPipelineCache cache{root};
    const auto prepared = cache.prepareCandidate(passthroughRequest(), makeKey(), true);
    REQUIRE(prepared.has_value());
    REQUIRE(cache.candidate() != nullptr);
    CHECK(cache.active() == nullptr);
    const auto candidateIdentity = cache.candidate()->key;
    cache.activate();
    REQUIRE(cache.active() != nullptr);
    CHECK(cache.candidate() == nullptr);
    CHECK(cache.active()->key == candidateIdentity);

    cuexis::shader::ShaderCompileRequest bad = passthroughRequest();
    static constexpr std::string_view kBad =
        "#version 450\n#include \"stolen.glsl\"\nvoid main() {}\n";
    bad.vertexSource = kBad;
    const auto badIdentity = cuexis::shader::hashStandaloneSourceIdentity(kBad, kFragment);
    const auto failed = cache.prepareCandidate(bad, makeKey(badIdentity), true);
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error().code() == cuexis::shader::diagnosticHotReloadFailed);
    REQUIRE(cache.active() != nullptr);
    CHECK(cache.active()->key == candidateIdentity);
    CHECK(cache.candidate() == nullptr);
}

TEST_CASE("S5-F compile-disabled prepare hits an existing cache without compiling",
          "[shader][s5-f][pipeline]") {
    const auto root = workRoot("hit");
    cuexis::shader::ShaderPipelineCache cache{root};
    REQUIRE(cache.prepareCandidate(passthroughRequest(), makeKey(), true).has_value());
    cache.activate();
    REQUIRE(cache.active() != nullptr);

    const auto hit = cache.prepareCandidate(passthroughRequest(), makeKey(), false);
    REQUIRE(hit.has_value());
    REQUIRE(cache.candidate() != nullptr);
    CHECK(cache.candidate()->key == cache.active()->key);
}

TEST_CASE("S5-F undeclared keywords never become macros or cache entries",
          "[shader][s5-f][keyword]") {
    const auto root = workRoot("keyword");
    cuexis::shader::ShaderPipelineCache cache{root};
    const std::string_view selected[] = {"NOT_DECLARED"};
    const auto request = cuexis::shader::ShaderCompileRequest{
        .vertexSource = kVertex,
        .fragmentSource = kFragment,
        .selectedKeywords = selected,
    };
    const auto failed = cache.prepareCandidate(request, makeKey(), true);
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error().code() == cuexis::shader::diagnosticKeywordInvalid);
    CHECK(cache.active() == nullptr);
    CHECK(std::filesystem::is_empty(root));
}

TEST_CASE("S5-F shader diagnostics truncate at 1024 with the frozen sentinel",
          "[shader][s5-f][diagnostics]") {
    auto diagnostics = cuexis::shader::makeShaderDiagnostics();
    for (int index = 0; index < 2000; ++index) {
        diagnostics.add(cuexis::core::Diagnostic{cuexis::core::DiagnosticSeverity::Error,
                                                 "shader.compile.failed", "fill", "$"});
    }
    REQUIRE(diagnostics.limitReached());
    REQUIRE(diagnostics.size() == 1024);
    CHECK(diagnostics.items().back().code() == cuexis::shader::diagnosticLimitExceeded);
}
