// R4 transaction tests: atomic Packed output keeps the previous valid artifact, never publishes a
// partial file, cleans up its own temporary file, and never deletes a file it does not own.

#include <cuexis/chart/packed_chart_io.hpp>
#include <cuexis/chart/packed_chart_tables.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

using cuexis::chart::CanonicalEntity;
using cuexis::chart::CanonicalFeature;
using cuexis::chart::CanonicalRequirement;
using cuexis::chart::CanonicalSemanticChart;
using cuexis::chart::ChartId;
using cuexis::chart::ChartObjectId;
using cuexis::chart::ExplicitEntityIdentity;
using cuexis::chart::LaneConstraint;
using cuexis::chart::PackedChartLimits;

constexpr std::string_view registeredFeature{"cuexis.gameplay.candidate.lanes4"};

// `lastTwelve` is the last 12 hexadecimal digits of the object UUID.
[[nodiscard]] auto tapChart(std::string_view lastTwelve) -> CanonicalSemanticChart {
    CanonicalSemanticChart chart;
    chart.chartId = ChartId{"019b0000-0000-7abc-8def-000000000001"};
    chart.defaultCamera = cuexis::chart::CameraData{};
    chart.features.push_back(CanonicalFeature{std::string{registeredFeature}, 1});
    CanonicalEntity entity;
    entity.identity =
        ExplicitEntityIdentity{ChartObjectId{"019b0000-0000-7abc-8def-" + std::string{lastTwelve}}};
    CanonicalRequirement requirement;
    requirement.localId = "hit";
    requirement.judgementDomain = {"judgement-domain", "candidate.lanes4"};
    requirement.requiredAction = {"action", "press"};
    requirement.constraints.emplace_back(LaneConstraint{1});
    entity.requirements.push_back(std::move(requirement));
    chart.entities.push_back(std::move(entity));
    return chart;
}

// A fresh directory per test so the temp-file assertions cannot observe another test's files.
class Workspace final {
  public:
    Workspace() {
        static std::atomic<std::uint32_t> counter{0};
        path_ = fs::temp_directory_path() /
                ("cuexis-r4-atomic-" + std::to_string(counter.fetch_add(1)));
        std::error_code ignored;
        fs::remove_all(path_, ignored);
        REQUIRE(fs::create_directories(path_));
    }
    ~Workspace() {
        std::error_code ignored;
        fs::remove_all(path_, ignored);
    }
    Workspace(const Workspace&) = delete;
    auto operator=(const Workspace&) -> Workspace& = delete;
    Workspace(Workspace&&) = delete;
    auto operator=(Workspace&&) -> Workspace& = delete;

    [[nodiscard]] auto file(std::string_view name) const -> fs::path {
        return path_ / name;
    }

    // Names of the temporary siblings this process would create for a target in this directory.
    [[nodiscard]] auto temporaryNames() const -> std::vector<std::string> {
        std::vector<std::string> names;
        for (const auto& entry : fs::directory_iterator(path_)) {
            const auto name = entry.path().filename().string();
            if (name.find(".tmp.") != std::string::npos) {
                names.push_back(name);
            }
        }
        return names;
    }

  private:
    fs::path path_;
};

[[nodiscard]] auto readBytes(const fs::path& path) -> std::vector<std::byte> {
    std::ifstream input{path, std::ios::binary};
    REQUIRE(input);
    std::vector<std::byte> bytes;
    char value = 0;
    while (input.get(value)) {
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(value)));
    }
    return bytes;
}

void writeText(const fs::path& path, std::string_view text) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    REQUIRE(output);
    output << text;
}

[[nodiscard]] auto readText(const fs::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    REQUIRE(input);
    std::string text;
    char value = 0;
    while (input.get(value)) {
        text.push_back(value);
    }
    return text;
}

[[nodiscard]] auto encodedBytes(const CanonicalSemanticChart& chart) -> std::vector<std::byte> {
    const auto encoded = cuexis::chart::packed::encode(chart);
    REQUIRE(encoded);
    return *encoded;
}

} // namespace

TEST_CASE("R4 a successful write replaces the previous artifact and leaves no temporary file",
          "[chart][packed][atomic][r4]") {
    const Workspace workspace;
    const auto target = workspace.file("chart.packed");
    const auto first = tapChart("000000000010");
    const auto second = tapChart("000000000020");

    const auto initial = cuexis::chart::PackedChartWriter::writeAtomic(first, target);
    REQUIRE(initial);
    REQUIRE(fs::exists(target));
    const auto firstBytes = readBytes(target);
    CHECK(firstBytes == encodedBytes(first));
    CHECK(workspace.temporaryNames().empty());

    const auto replaced = cuexis::chart::PackedChartWriter::writeAtomic(second, target);
    REQUIRE(replaced);
    const auto secondBytes = readBytes(target);
    CHECK(secondBytes == encodedBytes(second));
    CHECK(secondBytes != firstBytes);
    CHECK(replaced->statistics.packedBytes == secondBytes.size());
    CHECK(workspace.temporaryNames().empty());
}

TEST_CASE("R4 a refused model keeps the previous artifact and writes nothing",
          "[chart][packed][atomic][r4]") {
    const Workspace workspace;
    const auto target = workspace.file("chart.packed");
    const auto valid = tapChart("000000000010");
    REQUIRE(cuexis::chart::PackedChartWriter::writeAtomic(valid, target));
    const auto before = readBytes(target);

    SECTION("profile refusal") {
        auto outOfProfile = tapChart("000000000020");
        outOfProfile.entities.front().requirements.front().constraints.front() = LaneConstraint{9};
        const auto written = cuexis::chart::PackedChartWriter::writeAtomic(outOfProfile, target);
        REQUIRE_FALSE(written);
        CHECK(written.error().code() == "packed.profile.lane");
    }
    SECTION("budget refusal") {
        auto limits = PackedChartLimits{};
        limits.maxPackedSectionBytes = 1U;
        const auto written = cuexis::chart::PackedChartWriter::writeAtomic(
            tapChart("000000000020"), target, {.limits = limits});
        REQUIRE_FALSE(written);
        CHECK(written.error().code() == "packed.budget.section_bytes");
    }
    SECTION("identity precondition refusal") {
        auto inconsistent = tapChart("000000000020");
        inconsistent.mainMusic = cuexis::chart::AssetId{"audio.main"};
        const auto written = cuexis::chart::PackedChartWriter::writeAtomic(inconsistent, target);
        REQUIRE_FALSE(written);
        CHECK(written.error().code() == "packed.identity.closure");
    }

    CHECK(readBytes(target) == before);
    CHECK(workspace.temporaryNames().empty());
}

TEST_CASE("R4 a failed replacement removes its temporary file and keeps foreign files",
          "[chart][packed][atomic][r4]") {
    const Workspace workspace;
    const auto target = workspace.file("occupied");
    // A directory cannot be replaced by a file, which fails the atomic replacement after the
    // temporary file has already been written.
    REQUIRE(fs::create_directories(target));
    const auto foreign = workspace.file("foreign.bin");
    writeText(foreign, "not ours");
    const auto decoy = workspace.file("occupied.tmp.4242");
    writeText(decoy, "pre-existing temporary-looking file");

    const auto written =
        cuexis::chart::PackedChartWriter::writeAtomic(tapChart("000000000010"), target);
    REQUIRE_FALSE(written);
    CHECK(written.error().code() == "packed.io.replace_failed");

    // The temporary file created by this call is gone, the target directory is untouched, and the
    // foreign files (including a temporary-looking one this call does not own) survive.
    CHECK(fs::is_directory(target));
    CHECK(readText(foreign) == "not ours");
    CHECK(fs::exists(decoy));
    CHECK(readText(decoy) == "pre-existing temporary-looking file");
    CHECK(workspace.temporaryNames() == std::vector<std::string>{"occupied.tmp.4242"});
}

TEST_CASE("R4 a missing output directory is refused before any file is created",
          "[chart][packed][atomic][r4]") {
    const Workspace workspace;
    const auto target = workspace.file("missing") / "chart.packed";

    const auto written =
        cuexis::chart::PackedChartWriter::writeAtomic(tapChart("000000000010"), target);
    REQUIRE_FALSE(written);
    CHECK(written.error().code() == "packed.io.parent_missing");
    CHECK_FALSE(fs::exists(workspace.file("missing")));
    CHECK(workspace.temporaryNames().empty());
}
