#pragma once

// Shared staging/lock fixture for the asset publish case units.

#include <cuexis/tools/asset_publish.hpp>

#include <cuexis/chart/canonical_semantic_chart.hpp>
#include <cuexis/chart/packed_chart_io.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/cxc/cxc_package.hpp>
#include <cuexis_internal/sha256.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#if defined(_WIN32)
#include "../../tools/common/asset_publish/src/publish_fs_internal.hpp"

#include <Windows.h>
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

namespace asset_publish_fixture {

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
        std::ostringstream diagnostic;
        diagnostic << result.error().code() << ": " << result.error().message();
        for (const auto& item : result.error().context())
            diagnostic << " [" << item.key << "=" << item.value << "]";
        FAIL(diagnostic.str());
    }
}

inline [[nodiscard]] auto scratchRoot(std::string_view name) -> fs::path {
    const auto root = fs::temp_directory_path() / "cuexis-s6e3-asset-publish" / std::string{name};
    std::error_code status;
    fs::remove_all(root, status);
    fs::create_directories(root, status);
    REQUIRE_FALSE(status);
    return root;
}

inline [[nodiscard]] auto readBytes(const fs::path& path) -> std::vector<std::byte> {
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

inline [[nodiscard]] auto asBytes(std::string_view text) -> std::vector<std::byte> {
    const auto* data = reinterpret_cast<const std::byte*>(text.data());
    return std::vector<std::byte>{data, data + text.size()};
}

inline void writeBytes(const fs::path& path, std::string_view text) {
    std::error_code status;
    fs::create_directories(path.parent_path(), status);
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    REQUIRE(stream.good());
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    REQUIRE(stream.good());
}

// The same thing for an already-read package: a fixture that must hold exact bytes rather than
// text.
inline void writeBytes(const fs::path& path, const std::vector<std::byte>& bytes) {
    std::error_code status;
    fs::create_directories(path.parent_path(), status);
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    REQUIRE(stream.good());
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
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

inline [[nodiscard]] auto fixtureProjectRoot() -> fs::path {
    return fs::path{CUEXIS_SOURCE_DIR} / "tests" / "fixtures" / "chart_format_update" /
           "static_project";
}

inline [[nodiscard]] auto textEntry(std::string path, const fs::path& file)
    -> cuexis::cxc::CxcWriteEntry {
    return cuexis::cxc::CxcWriteEntry{std::move(path), readBytes(file)};
}

// The committed static fixture project is a complete, valid v4 CXC input.
inline [[nodiscard]] auto staticProjectEntries() -> std::vector<cuexis::cxc::CxcWriteEntry> {
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
inline [[nodiscard]] auto candidateFixture() -> CandidateFixture {
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

inline [[nodiscard]] auto loadPackage(const fs::path& path) -> cuexis::cxc::CxcPackage {
    auto loaded = cuexis::cxc::CxcPackageLoader::loadFile(path);
    REQUIRE(loaded.package.has_value());
    return *loaded.package;
}

inline [[nodiscard]] auto errorCode(const cuexis::core::Error& error) -> std::string {
    return std::string{error.code()};
}

inline [[nodiscard]] auto waitForFile(const fs::path& path, std::chrono::milliseconds timeout)
    -> bool {
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

inline [[nodiscard]] auto readEnv(std::string_view name) -> std::string {
    const std::string key{name};
    // _dupenv_s is MSVC-only; MinGW links no such symbol.
#if defined(_MSC_VER)
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
inline void spawnLockHolder() {
    std::string executable{CUEXIS_ASSET_PUBLISH_TEST_BINARY};
    if (executable.size() >= 2 && executable.front() == '"' && executable.back() == '"') {
        executable = executable.substr(1, executable.size() - 2);
    }
    constexpr std::string_view argument{"[.publish-lock-child]"};
#if defined(_WIN32)
    REQUIRE(::_spawnl(_P_NOWAIT, executable.c_str(), executable.c_str(), argument.data(),
                      nullptr) != -1);
#else
    const std::string command{"\"" + executable + "\" \"" + std::string{argument} +
                              "\" >/dev/null 2>&1 &"};
    REQUIRE(std::system(command.c_str()) == 0);
#endif
}

#if defined(_WIN32)
class ScopedNativeHandle final {
  public:
    explicit ScopedNativeHandle(HANDLE handle) noexcept : handle_(handle) {}
    ScopedNativeHandle(const ScopedNativeHandle&) = delete;
    auto operator=(const ScopedNativeHandle&) -> ScopedNativeHandle& = delete;
    ~ScopedNativeHandle() {
        (void)close();
    }

    [[nodiscard]] auto valid() const noexcept -> bool {
        return handle_ != INVALID_HANDLE_VALUE;
    }

    auto close() noexcept -> bool {
        if (!valid()) {
            return true;
        }
        if (CloseHandle(handle_) == 0) {
            return false;
        }
        handle_ = INVALID_HANDLE_VALUE;
        return true;
    }

  private:
    HANDLE handle_;
};
#endif

} // namespace asset_publish_fixture

using namespace asset_publish_fixture;
