// Stage 6 E3 publication transaction tests.
//
// The cases exercise the real transaction: an immutable content-addressed generation directory, an
// OS-level inter-process publication lock, and a CXC package replace that re-validates the bytes on
// disk through the production loader before it becomes visible. Failure paths are driven through
// the library's test-only injection hooks and through genuinely invalid inputs, never by asserting
// on code that did not run.

#include <cuexis/tools/asset_publish.hpp>

#include <cuexis/chart/canonical_semantic_chart.hpp>
#include <cuexis/chart/packed_chart_io.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/cxc/cxc_package.hpp>
#include <cuexis_internal/sha256.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#if defined(_WIN32)
#include <process.h>
#endif
#include <fstream>
#include <ios>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;
using cuexis::tools::GenerationPublishRequest;
using cuexis::tools::PackagePairRequest;
using cuexis::tools::PackagePublishRequest;
using cuexis::tools::PublicationLock;
using cuexis::tools::PublishEntry;

constexpr std::string_view candidateEntryPath{"compiled/chart.packed"};

// Fails the case with the library diagnostic instead of only the boolean, so a failing publication
// explains itself. Mirrors REQUIRE semantics: a failure stops the case.
template <typename T> void requireOk(const T& result) {
    if (!result.has_value()) {
        FAIL(std::string{result.error().message()});
    }
}

[[nodiscard]] auto scratchRoot(std::string_view name) -> fs::path {
    const auto root = fs::temp_directory_path() / "cuexis-s6e3-asset-publish" / std::string{name};
    std::error_code status;
    fs::remove_all(root, status);
    fs::create_directories(root, status);
    REQUIRE_FALSE(status);
    return root;
}

[[nodiscard]] auto readBytes(const fs::path& path) -> std::vector<std::byte> {
    std::ifstream stream{path, std::ios::binary};
    REQUIRE(stream.good());
    stream.seekg(0, std::ios::end);
    const auto size = static_cast<std::size_t>(stream.tellg());
    stream.seekg(0, std::ios::beg);
    std::vector<std::byte> bytes(size);
    if (size > 0) {
        stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
        REQUIRE(stream.good());
    }
    return bytes;
}

[[nodiscard]] auto asBytes(std::string_view text) -> std::vector<std::byte> {
    const auto* data = reinterpret_cast<const std::byte*>(text.data());
    return std::vector<std::byte>{data, data + text.size()};
}

void writeBytes(const fs::path& path, std::string_view text) {
    std::error_code status;
    fs::create_directories(path.parent_path(), status);
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    REQUIRE(stream.good());
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    REQUIRE(stream.good());
}

// Scoped environment variable, so an injection hook cannot leak into another case.
class ScopedEnv final {
  public:
    ScopedEnv(std::string name, std::string value) : name_(std::move(name)) {
#if defined(_WIN32)
        _putenv_s(name_.c_str(), value.c_str());
#else
        ::setenv(name_.c_str(), value.c_str(), 1);
#endif
    }

    ScopedEnv(const ScopedEnv&) = delete;
    auto operator=(const ScopedEnv&) -> ScopedEnv& = delete;

    ~ScopedEnv() {
#if defined(_WIN32)
        _putenv_s(name_.c_str(), "");
#else
        ::unsetenv(name_.c_str());
#endif
    }

  private:
    std::string name_;
};

[[nodiscard]] auto fixtureProjectRoot() -> fs::path {
    return fs::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" / "chart_format_update" /
           "static_project";
}

[[nodiscard]] auto textEntry(std::string path, const fs::path& file) -> cuexis::cxc::CxcWriteEntry {
    return cuexis::cxc::CxcWriteEntry{std::move(path), readBytes(file)};
}

// The committed static fixture project is a complete, valid v4 CXC input.
[[nodiscard]] auto staticProjectEntries() -> std::vector<cuexis::cxc::CxcWriteEntry> {
    const auto root = fixtureProjectRoot();
    return {
        textEntry("cuexis.project.json", root / "cuexis.project.json"),
        textEntry("assets/cuexis.asset-index.json", root / "assets" / "cuexis.asset-index.json"),
        textEntry("assets/charts/main.cuexis.chart.json",
                  root / "assets" / "charts" / "main.cuexis.chart.json"),
    };
}

struct CandidateFixture final {
    std::vector<std::byte> bytes;
    std::string extensionJson;
};

// A real packed candidate chart plus the registered extension document that declares it, so the
// package closure admits the entry through the candidate path rather than a test shortcut.
[[nodiscard]] auto candidateFixture() -> CandidateFixture {
    using namespace cuexis::chart;
    CanonicalSemanticChart chart;
    chart.chartId = ChartId{"019b0000-0000-7abc-8def-000000000001"};
    chart.features.push_back(CanonicalFeature{"cuexis.gameplay.candidate.lanes4", 1});
    CanonicalRequirement requirement;
    requirement.localId = "hit";
    requirement.interval.startBeat = RationalBeat::zero();
    requirement.judgementDomain = {"judgement-domain", "candidate.lanes4"};
    requirement.requiredAction = {"action", "press"};
    requirement.constraints.emplace_back(LaneConstraint{2});
    CanonicalEntity entity;
    entity.identity = ExplicitEntityIdentity{ChartObjectId{"019b0000-0000-7abc-8def-000000000010"}};
    entity.components.emplace_back(CanonicalTransform{});
    entity.requirements.push_back(std::move(requirement));
    chart.entities.push_back(std::move(entity));

    auto encoded = packed::encode(chart);
    requireOk(encoded);
    auto statistics = packed::inspect(*encoded);
    requireOk(statistics);
    auto identity = packed::semanticIdentity(chart);
    requireOk(identity);
    const auto artifactIdentity = cuexis::core::detail::sha256Hex(
        std::span<const std::byte>{encoded->data(), encoded->size()});

    std::ostringstream json;
    json << R"({"cuexis.chart-entry.v1":{"entries":[{"path":")" << candidateEntryPath
         << R"(","kind":"chart","encoding":"packed-chart","playback":true,)"
         << R"("compiledSemanticIdentity":")" << packed::semanticIdentityHex(*identity)
         << R"(","artifactIdentity":")" << artifactIdentity
         << R"(","compilerProfile":"candidate.static-tap-lanes4-v1","expandedEntityCount":)"
         << statistics->entityCount << R"(,"expandedRequirementCount":)"
         << statistics->requirementCount << R"(}]}})";
    return CandidateFixture{std::move(*encoded), json.str()};
}

[[nodiscard]] auto loadPackage(const fs::path& path) -> cuexis::cxc::CxcPackage {
    auto loaded = cuexis::cxc::CxcPackageLoader::loadFile(path);
    REQUIRE(loaded.package.has_value());
    return *loaded.package;
}

[[nodiscard]] auto errorCode(const cuexis::core::Error& error) -> std::string {
    return std::string{error.code()};
}

[[nodiscard]] auto waitForFile(const fs::path& path, std::chrono::milliseconds timeout) -> bool {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        std::error_code status;
        if (fs::exists(path, status) && !status) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
    }
    return false;
}

[[nodiscard]] auto readEnv(std::string_view name) -> std::string {
    const std::string key{name};
#if defined(_WIN32)
    char* value = nullptr;
    std::size_t size = 0;
    if (::_dupenv_s(&value, &size, key.c_str()) != 0 || value == nullptr) {
        return {};
    }
    std::string out{value};
    std::free(value);
    return out;
#else
    const char* value = std::getenv(key.c_str());
    return value == nullptr ? std::string{} : std::string{value};
#endif
}

// Starts this same test binary again with the hidden lock-holder tag. The child is detached: the
// case synchronises on the ready and release files instead of joining it.
void spawnLockHolder() {
    std::string executable{CUEXIS_ASSET_PUBLISH_TEST_BINARY};
    if (executable.size() >= 2 && executable.front() == '"' && executable.back() == '"') {
        executable = executable.substr(1, executable.size() - 2);
    }
    constexpr std::string_view argument{"[.publish-lock-child]"};
#if defined(_WIN32)
    ::_spawnl(_P_NOWAIT, executable.c_str(), executable.c_str(), argument.data(), nullptr);
#else
    const std::string command{"\"" + executable + "\" \"" + std::string{argument} +
                              "\" >/dev/null 2>&1 &"};
    static_cast<void>(std::system(command.c_str()));
#endif
}

} // namespace

TEST_CASE("E3 generation publication writes an immutable content-addressed generation",
          "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("generation-basic");
    const auto texture = asBytes("portable-texture-bytes");
    const auto provenance = asBytes("{\"kind\":\"texture\"}\n");

    GenerationPublishRequest request;
    request.root = root;
    request.generationId = "26.09.24-1";
    request.entries = {PublishEntry{"textures/checker.texture", texture}};
    request.provenanceRecords = {PublishEntry{"texture-checker.provenance.json", provenance}};

    const auto published = cuexis::tools::publishGeneration(request);
    requireOk(published);
    CHECK(published->generationIdentity.size() == 64U);
    CHECK(published->generationPath == root / "generations" / published->generationIdentity);
    CHECK(published->closureEntries == 1U);
    CHECK(published->provenanceEntries == 1U);
    CHECK(published->closureBytes == texture.size());
    CHECK(readBytes(published->generationPath / "textures" / "checker.texture") == texture);
    CHECK(readBytes(published->generationPath / "provenance" / "texture-checker.provenance.json") ==
          provenance);
    CHECK(fs::is_regular_file(published->generationPath / "cuexis.generation"));
    // The staging directory is gone: the rename was the single visible switch.
    CHECK_FALSE(fs::exists(root / "staging" / published->generationIdentity));
    // The audit sidecar is stored beside the closure, not inside it.
    CHECK(fs::is_directory(published->generationPath / "provenance"));
}

TEST_CASE("E3 generation publication is idempotent and content addressed",
          "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("generation-idempotent");
    GenerationPublishRequest request;
    request.root = root;
    request.generationId = "26.09.24-1";
    request.entries = {PublishEntry{"audio/track.wav", asBytes("canonical-wav-bytes")}};

    const auto first = cuexis::tools::publishGeneration(request);
    requireOk(first);
    const auto second = cuexis::tools::publishGeneration(request);
    requireOk(second);
    CHECK(second->generationIdentity == first->generationIdentity);
    CHECK(second->generationPath == first->generationPath);

    // Different content gets a different immutable directory; the first one survives untouched.
    request.entries = {PublishEntry{"audio/track.wav", asBytes("different-canonical-wav")}};
    const auto third = cuexis::tools::publishGeneration(request);
    requireOk(third);
    CHECK(third->generationIdentity != first->generationIdentity);
    CHECK(fs::is_regular_file(first->generationPath / "audio" / "track.wav"));
    CHECK(readBytes(first->generationPath / "audio" / "track.wav") ==
          asBytes("canonical-wav-bytes"));

    // The label is part of the recorded identity, so relabelling the same content is a new record.
    request.entries = {PublishEntry{"audio/track.wav", asBytes("canonical-wav-bytes")}};
    request.generationId = "26.09.25-1";
    const auto relabelled = cuexis::tools::publishGeneration(request);
    requireOk(relabelled);
    CHECK(relabelled->generationIdentity != first->generationIdentity);
}

TEST_CASE("E3 generation publication rejects invalid batches without touching disk",
          "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("generation-invalid");
    GenerationPublishRequest request;
    request.root = root;
    request.generationId = "26.09.24-1";

    const auto empty = cuexis::tools::publishGeneration(request);
    REQUIRE_FALSE(empty.has_value());
    CHECK(errorCode(empty.error()) == "asset.publish.invalid_request");

    request.entries = {PublishEntry{"../escape.texture", asBytes("bytes")}};
    const auto escape = cuexis::tools::publishGeneration(request);
    REQUIRE_FALSE(escape.has_value());
    CHECK(errorCode(escape.error()) == "asset.publish.invalid_request");

    request.entries = {PublishEntry{"textures/a.texture", asBytes("bytes")},
                       PublishEntry{"textures/a.texture", asBytes("other")}};
    const auto duplicate = cuexis::tools::publishGeneration(request);
    REQUIRE_FALSE(duplicate.has_value());
    CHECK(errorCode(duplicate.error()) == "asset.publish.invalid_request");

    // A case-folded collision would collide on a case-insensitive filesystem.
    request.entries = {PublishEntry{"textures/A.texture", asBytes("bytes")},
                       PublishEntry{"textures/a.texture", asBytes("bytes")}};
    const auto folded = cuexis::tools::publishGeneration(request);
    REQUIRE_FALSE(folded.has_value());
    CHECK(errorCode(folded.error()) == "asset.publish.invalid_request");

    request.entries = {PublishEntry{"textures/empty.texture", {}}};
    const auto blank = cuexis::tools::publishGeneration(request);
    REQUIRE_FALSE(blank.has_value());
    CHECK(errorCode(blank.error()) == "asset.publish.invalid_request");

    CHECK_FALSE(fs::exists(root / "generations"));
    CHECK_FALSE(fs::exists(root / "staging"));
}

TEST_CASE("E3 a failed generation publish cleans staging and keeps previous generations",
          "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("generation-failure");
    GenerationPublishRequest request;
    request.root = root;
    request.generationId = "26.09.24-1";
    request.entries = {PublishEntry{"textures/checker.texture", asBytes("first")}};

    const auto published = cuexis::tools::publishGeneration(request);
    requireOk(published);

    request.entries = {PublishEntry{"textures/checker.texture", asBytes("second")}};
    {
        const ScopedEnv fail{"CUEXIS_ASSET_PUBLISH_FAIL_AFTER_STAGING", "1"};
        const auto staged = cuexis::tools::publishGeneration(request);
        REQUIRE_FALSE(staged.has_value());
        CHECK(errorCode(staged.error()) == "asset.publish.staging_failed");
    }
    {
        const ScopedEnv fail{"CUEXIS_ASSET_PUBLISH_FAIL_BEFORE_PUBLISH", "1"};
        const auto published2 = cuexis::tools::publishGeneration(request);
        REQUIRE_FALSE(published2.has_value());
        CHECK(errorCode(published2.error()) == "asset.publish.replace_failed");
    }

    // No partial generation appeared, no staging remained, and the first generation is intact.
    CHECK(readBytes(published->generationPath / "textures" / "checker.texture") ==
          asBytes("first"));
    std::error_code status;
    std::size_t generations = 0;
    for (const auto& entry : fs::directory_iterator{root / "generations", status}) {
        static_cast<void>(entry);
        ++generations;
    }
    CHECK(generations == 1U);
    std::size_t staged = 0;
    for (const auto& entry : fs::directory_iterator{root / "staging", status}) {
        static_cast<void>(entry);
        ++staged;
    }
    CHECK(staged == 0U);
}

TEST_CASE("E3 a tampered published generation is refused instead of silently rebuilt",
          "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("generation-tampered");
    GenerationPublishRequest request;
    request.root = root;
    request.generationId = "26.09.24-1";
    request.entries = {PublishEntry{"textures/checker.texture", asBytes("canonical")}};

    const auto published = cuexis::tools::publishGeneration(request);
    requireOk(published);
    const auto target = published->generationPath / "textures" / "checker.texture";
    writeBytes(target, "tampered-bytes!");

    const auto republished = cuexis::tools::publishGeneration(request);
    REQUIRE_FALSE(republished.has_value());
    CHECK(errorCode(republished.error()) == "asset.publish.validation_failed");
    // The damaged file is still exactly as it was found: nothing was silently overwritten.
    CHECK(readBytes(target) == asBytes("tampered-bytes!"));
}

TEST_CASE("E3 publication recovery removes abandoned staging and temporary files",
          "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("recovery");
    writeBytes(root / "staging" / "abcdef" / "textures" / "half.texture", "half");
    writeBytes(root / "generations" / "abcdef.cuexis-publish.tmp.7", "temp");
    writeBytes(root / "package.cxc.cuexis-publish.tmp.9", "temp");
    writeBytes(root / "keep.txt", "keep");
    writeBytes(root / "generations" / "0123456789abcdef", "published");

    const auto removed = cuexis::tools::recoverPublicationStaging(root);
    requireOk(removed);
    CHECK(*removed == 3U);
    CHECK_FALSE(fs::exists(root / "staging" / "abcdef"));
    CHECK_FALSE(fs::exists(root / "generations" / "abcdef.cuexis-publish.tmp.7"));
    CHECK_FALSE(fs::exists(root / "package.cxc.cuexis-publish.tmp.9"));
    CHECK(fs::is_regular_file(root / "keep.txt"));
    CHECK(fs::is_regular_file(root / "generations" / "0123456789abcdef"));

    const auto missing = cuexis::tools::recoverPublicationStaging(root / "absent");
    REQUIRE_FALSE(missing.has_value());
    CHECK(errorCode(missing.error()) == "asset.publish.invalid_request");
}

TEST_CASE("E3 adoption is explicit and never rewrites a project entry", "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("adoption");
    GenerationPublishRequest request;
    request.root = root;
    request.generationId = "26.09.24-1";
    request.entries = {PublishEntry{"textures/checker.texture", asBytes("canonical")}};
    const auto published = cuexis::tools::publishGeneration(request);
    requireOk(published);

    const auto before = cuexis::tools::readAdoptedGeneration(root);
    REQUIRE_FALSE(before.has_value());
    CHECK(errorCode(before.error()) == "asset.publish.validation_failed");

    const auto adopted = cuexis::tools::adoptGeneration(root, published->generationIdentity);
    requireOk(adopted);
    const auto after = cuexis::tools::readAdoptedGeneration(root);
    requireOk(after);
    CHECK(*after == published->generationIdentity);
    // No project configuration was created or modified by publishing or adopting.
    CHECK_FALSE(fs::exists(root / "cuexis.project.json"));

    const auto unknown = cuexis::tools::adoptGeneration(root, std::string(64U, 'a'));
    REQUIRE_FALSE(unknown.has_value());
    CHECK(errorCode(unknown.error()) == "asset.publish.validation_failed");
}

TEST_CASE("E3 the publication lock rejects a second writer in the same process",
          "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("lock-in-process");
    const auto lockPath = root / ".cuexis.publish.lock";
    {
        const auto first = PublicationLock::acquire(lockPath);
        requireOk(first);
        CHECK(first->path() == lockPath);
        const auto second = PublicationLock::acquire(lockPath);
        REQUIRE_FALSE(second.has_value());
        CHECK(errorCode(second.error()) == "asset.publish.busy");
    }
    // Releasing hands the lock over without deleting the lock file.
    const auto third = PublicationLock::acquire(lockPath);
    requireOk(third);
}

TEST_CASE("E3 the publication lock rejects a writer in another process", "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("lock-cross-process");
    const auto lockPath = root / ".cuexis.publish.lock";
    const auto ready = root / "child.ready";
    const auto release = root / "child.release";

    const ScopedEnv lockEnv{"CUEXIS_ASSET_PUBLISH_LOCK_PATH", lockPath.string()};
    const ScopedEnv readyEnv{"CUEXIS_ASSET_PUBLISH_LOCK_READY", ready.string()};
    const ScopedEnv releaseEnv{"CUEXIS_ASSET_PUBLISH_LOCK_RELEASE", release.string()};

    spawnLockHolder();
    const bool childReady = waitForFile(ready, std::chrono::milliseconds{30000});
    REQUIRE(childReady);
    const auto blocked = PublicationLock::acquire(lockPath);
    REQUIRE_FALSE(blocked.has_value());
    CHECK(errorCode(blocked.error()) == "asset.publish.busy");

    writeBytes(release, "release");
    // The child exits after the release file appears; the lock is free once its handle is closed.
    bool released = false;
    for (int attempt = 0; attempt < 600 && !released; ++attempt) {
        const auto probe = PublicationLock::acquire(lockPath);
        if (probe.has_value()) {
            released = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
    }
    CHECK(released);
}

// Hidden helper case: the parent case above starts this binary again with this tag, so the lock is
// genuinely held by a different process.
TEST_CASE("publication lock child holder", "[.publish-lock-child]") {
    const auto path = readEnv("CUEXIS_ASSET_PUBLISH_LOCK_PATH");
    const auto ready = readEnv("CUEXIS_ASSET_PUBLISH_LOCK_READY");
    const auto release = readEnv("CUEXIS_ASSET_PUBLISH_LOCK_RELEASE");
    if (path.empty() || ready.empty() || release.empty()) {
        return;
    }
    const auto lock = PublicationLock::acquire(fs::path{path});
    if (!lock.has_value()) {
        return;
    }
    writeBytes(fs::path{ready}, "ready");
    static_cast<void>(waitForFile(fs::path{release}, std::chrono::milliseconds{20000}));
}

TEST_CASE("E3 package publication validates the bytes on disk before replacing the target",
          "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("package-basic");
    const auto target = root / "static.cxc";
    PackagePublishRequest request;
    request.target = target;
    request.entries = staticProjectEntries();

    const auto published = cuexis::tools::publishPackage(request);
    requireOk(published);
    CHECK_FALSE(published->replacedExisting);
    CHECK(published->packageIdentity.size() == 64U);
    CHECK(published->bytes == readBytes(target).size());
    CHECK(loadPackage(target).identity().hex() == published->packageIdentity);

    // A republish of the same batch is a byte-identical replacement, not a new package.
    const auto again = cuexis::tools::publishPackage(request);
    requireOk(again);
    CHECK(again->replacedExisting);
    CHECK(again->packageIdentity == published->packageIdentity);

    // No temporary or backup siblings were left behind; only the published package and the
    // persistent lock file remain.
    std::error_code status;
    for (const auto& entry : fs::directory_iterator{root, status}) {
        const auto name = entry.path().filename().string();
        CHECK(name.find(".tmp.") == std::string::npos);
        CHECK((name == target.filename().string() || name == ".cuexis.publish.lock"));
    }
}

TEST_CASE("E3 a failed package replacement keeps the previous valid package",
          "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("package-failure");
    const auto target = root / "static.cxc";
    PackagePublishRequest request;
    request.target = target;
    request.entries = staticProjectEntries();
    const auto published = cuexis::tools::publishPackage(request);
    requireOk(published);
    const auto original = readBytes(target);

    const auto fixture = candidateFixture();
    request.entries.push_back(
        cuexis::cxc::CxcWriteEntry{std::string{candidateEntryPath}, fixture.bytes});
    request.extensionsJson = fixture.extensionJson;
    {
        const ScopedEnv fail{"CUEXIS_ASSET_PUBLISH_FAIL_BEFORE_REPLACE", "1"};
        const auto failed = cuexis::tools::publishPackage(request);
        REQUIRE_FALSE(failed.has_value());
        CHECK(errorCode(failed.error()) == "asset.publish.replace_failed");
    }
    // The previous package is byte-identical: a failure never publishes a partial result.
    CHECK(readBytes(target) == original);
    CHECK(loadPackage(target).identity().hex() == published->packageIdentity);
}

TEST_CASE("E3 a missing package target directory fails closed", "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("package-missing-directory");
    PackagePublishRequest request;
    request.target = root / "absent" / "static.cxc";
    request.entries = staticProjectEntries();

    const auto published = cuexis::tools::publishPackage(request);
    REQUIRE_FALSE(published.has_value());
    CHECK(errorCode(published.error()) == "asset.publish.invalid_request");
    CHECK_FALSE(fs::exists(request.target));
}

TEST_CASE("E3 one batch publishes a v4 and a candidate package closure", "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("package-pair");
    const auto fixture = candidateFixture();
    PackagePairRequest request;
    request.v4Target = root / "static.cxc";
    request.candidateTarget = root / "static.candidate.cxc";
    request.entries = staticProjectEntries();
    request.candidateEntries.push_back(
        cuexis::cxc::CxcWriteEntry{std::string{candidateEntryPath}, fixture.bytes});
    request.candidateExtensionsJson = fixture.extensionJson;

    const auto published = cuexis::tools::publishPackagePair(request);
    requireOk(published);
    CHECK(published->v4.packageIdentity != published->candidate.packageIdentity);

    const auto v4 = loadPackage(request.v4Target);
    const auto candidate = loadPackage(request.candidateTarget);
    CHECK(v4.identity().hex() == published->v4.packageIdentity);
    CHECK(candidate.identity().hex() == published->candidate.packageIdentity);
    // The v4 closure is the shared project closure; only the candidate adds the packed chart.
    CHECK_FALSE(v4.entryBytes(std::string{candidateEntryPath}).has_value());
    CHECK(candidate.entryBytes(std::string{candidateEntryPath}).has_value());
    CHECK(v4.manifest().canonicalExtensionsJson.find("cuexis.chart-entry.v1") == std::string::npos);
    CHECK(candidate.manifest().canonicalExtensionsJson.find("cuexis.chart-entry.v1") !=
          std::string::npos);
}

TEST_CASE("E3 an invalid candidate closure fails before either package is replaced",
          "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("package-pair-validation");
    const auto fixture = candidateFixture();
    PackagePairRequest request;
    request.v4Target = root / "static.cxc";
    request.candidateTarget = root / "static.candidate.cxc";
    request.entries = staticProjectEntries();
    // The extension declares a playback entry the candidate closure does not contain, so the
    // production loader rejects the candidate package before any target is replaced.
    request.candidateEntries.push_back(
        cuexis::cxc::CxcWriteEntry{std::string{candidateEntryPath}, fixture.bytes});
    request.candidateExtensionsJson =
        R"({"cuexis.chart-entry.v1":{"entries":[{"path":"compiled/missing.packed",)"
        R"("kind":"chart","encoding":"packed-chart","playback":true,)"
        R"("compiledSemanticIdentity":"0000000000000000000000000000000000000000000000000000000000000000",)"
        R"("artifactIdentity":"0000000000000000000000000000000000000000000000000000000000000000",)"
        R"("compilerProfile":"candidate.static-tap-lanes4-v1","expandedEntityCount":1,)"
        R"("expandedRequirementCount":1}]}})";

    const auto published = cuexis::tools::publishPackagePair(request);
    REQUIRE_FALSE(published.has_value());
    CHECK(errorCode(published.error()) == "asset.publish.validation_failed");
    CHECK_FALSE(fs::exists(request.v4Target));
    CHECK_FALSE(fs::exists(request.candidateTarget));
}

TEST_CASE("E3 a failed candidate replacement rolls the v4 package back", "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("package-pair-rollback");
    const auto fixture = candidateFixture();
    PackagePairRequest request;
    request.v4Target = root / "static.cxc";
    request.candidateTarget = root / "static.candidate.cxc";
    request.entries = staticProjectEntries();
    request.candidateEntries.push_back(
        cuexis::cxc::CxcWriteEntry{std::string{candidateEntryPath}, fixture.bytes});
    request.candidateExtensionsJson = fixture.extensionJson;

    {
        const ScopedEnv fail{"CUEXIS_ASSET_PUBLISH_FAIL_REPLACE_TARGET",
                             request.candidateTarget.filename().string()};
        const auto published = cuexis::tools::publishPackagePair(request);
        REQUIRE_FALSE(published.has_value());
        CHECK(errorCode(published.error()) == "asset.publish.replace_failed");
    }
    // The pair is transactional: no half-updated v4 package is left visible.
    CHECK_FALSE(fs::exists(request.v4Target));
    CHECK_FALSE(fs::exists(request.candidateTarget));
}
