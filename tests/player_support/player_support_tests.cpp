#include <cuexis/player_support/audio_device_profile.hpp>
#include <cuexis/player_support/config_location.hpp>
#include <cuexis/player_support/resolved_config.hpp>
#include <cuexis/player_support/user_preferences.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

[[nodiscard]] auto schemaPath(const char* name) -> std::filesystem::path {
    return std::filesystem::path{CUEXIS_SOURCE_DIR} / "schemas" / name;
}

[[nodiscard]] auto tempDirectory() -> std::filesystem::path {
    const auto path = std::filesystem::temp_directory_path() / "cuexis-player-support-tests";
    std::filesystem::create_directories(path);
    return path;
}

void writeText(const std::filesystem::path& path, std::string_view text) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    REQUIRE(output.good());
    output << text;
}

} // namespace

TEST_CASE("Missing and future preferences keep the documented defaults", "[player][preferences]") {
    const auto root = tempDirectory();
    const auto schema = schemaPath("cuexis.player-preferences.v1.schema.json");
    const auto missing = root / "missing-preferences.json";
    std::filesystem::remove(missing);
    const auto loaded = cuexis::player_support::loadUserPreferences(missing, schema);
    REQUIRE(loaded.has_value());
    CHECK(loaded->usedDefaults);
    CHECK(loaded->preferences == cuexis::player_support::defaultUserPreferences());
    CHECK(loaded->diagnostics.items().front().code() == "player.preferences.missing");

    const auto future = root / "future-preferences.json";
    const std::string original =
        R"({"format":"cuexis.player-preferences","version":2,"windowWidth":1})";
    writeText(future, original);
    const auto futureLoad = cuexis::player_support::loadUserPreferences(future, schema);
    REQUIRE(futureLoad.has_value());
    CHECK(futureLoad->preserveExistingFile);
    CHECK(futureLoad->preferences.windowWidth == 1280);
    CHECK(futureLoad->diagnostics.items().front().code() ==
          "player.preferences.unsupported_version");
    const auto saved =
        cuexis::player_support::saveUserPreferences(future, futureLoad->preferences, schema);
    REQUIRE_FALSE(saved.has_value());
    CHECK(saved.error().code() == "player.preferences.unsupported_version");
    std::ifstream input{future, std::ios::binary};
    std::string after{(std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>()};
    CHECK(after == original);

    const auto resolved = cuexis::player_support::resolveAppConfig(*futureLoad);
    CHECK(resolved.preferencesSource == cuexis::player_support::ConfigValueSource::CodeDefault);
    CHECK(resolved.preservePreferencesFile);
}

TEST_CASE("Preferences round-trip and the lock rejects a second writer", "[player][preferences]") {
    const auto root = tempDirectory();
    const auto schema = schemaPath("cuexis.player-preferences.v1.schema.json");
    const auto path = root / "preferences.json";
    std::filesystem::remove(path);
    std::filesystem::remove(std::filesystem::path{path.string() + ".lock"});
    auto preferences = cuexis::player_support::defaultUserPreferences();
    preferences.windowWidth = 1600;
    preferences.gain = 0.25;
    REQUIRE(cuexis::player_support::saveUserPreferences(path, preferences, schema).has_value());
    const auto loaded = cuexis::player_support::loadUserPreferences(path, schema);
    REQUIRE(loaded.has_value());
    CHECK_FALSE(loaded->usedDefaults);
    CHECK(loaded->preferences == preferences);

    const auto lockPath = path.string() + ".lock";
    std::ofstream lock{lockPath, std::ios::binary | std::ios::trunc};
    REQUIRE(lock.good());
    lock << "held";
    lock.close();
    const auto busy = cuexis::player_support::saveUserPreferences(path, preferences, schema);
    REQUIRE_FALSE(busy.has_value());
    CHECK(busy.error().code() == "player.preferences.busy");
    std::filesystem::remove(lockPath);
}

TEST_CASE("Audio profiles match exactly one device and keep correction arithmetic",
          "[player][audio-profile]") {
    const auto root = tempDirectory();
    const auto schema = schemaPath("cuexis.audio-device-profile.v1.schema.json");
    const auto exactPath = root / "exact-profile.json";
    writeText(
        exactPath,
        R"({"format":"cuexis.audio-device-profile","version":1,"id":"desk-speakers","selector":{"kind":"exact","driver":"wasapi","deviceName":"Speakers"},"outputCorrectionUs":-12500})");
    const auto exact = cuexis::player_support::loadAudioDeviceProfile(exactPath, schema);
    REQUIRE(exact.has_value());
    const cuexis::player_support::AudioOutputDevice devices[] = {
        {.driver = "wasapi", .deviceName = "Headphones"},
        {.driver = "wasapi", .deviceName = "Speakers"},
    };
    const auto matched = cuexis::player_support::matchAudioDevice(*exact, devices);
    REQUIRE(matched.has_value());
    CHECK(matched->deviceName == "Speakers");

    const cuexis::player_support::AudioOutputDevice duplicates[] = {
        {.driver = "wasapi", .deviceName = "Speakers"},
        {.driver = "wasapi", .deviceName = "Speakers"},
    };
    const auto ambiguous = cuexis::player_support::matchAudioDevice(*exact, duplicates);
    REQUIRE_FALSE(ambiguous.has_value());
    CHECK(ambiguous.error().code() == "player.audio_profile.ambiguous");

    const auto none = cuexis::player_support::matchAudioDevice(*exact, {});
    REQUIRE_FALSE(none.has_value());
    CHECK(none.error().code() == "player.audio_profile.unmatched");

    const auto defaultPath = root / "default-profile.json";
    writeText(
        defaultPath,
        R"({"format":"cuexis.audio-device-profile","version":1,"id":"system-default","selector":{"kind":"system-default"},"outputCorrectionUs":0})");
    const auto systemDefault = cuexis::player_support::loadAudioDeviceProfile(defaultPath, schema);
    REQUIRE(systemDefault.has_value());
    const auto routed = cuexis::player_support::matchAudioDevice(*systemDefault, devices);
    REQUIRE(routed.has_value());
    CHECK(routed->systemDefault);
    CHECK(routed->deviceName.empty());

    const auto corrected = cuexis::player_support::correctedAudioPositionUs(1000, 2500);
    REQUIRE(corrected.has_value());
    CHECK(*corrected == 0);
    const auto negative = cuexis::player_support::correctedAudioPositionUs(1000000, -12500);
    REQUIRE(negative.has_value());
    CHECK(*negative == 1012500);
    const auto seek = cuexis::player_support::reverseSeekSourcePositionUs(500000, 10000, -12500);
    REQUIRE(seek.has_value());
    CHECK(*seek == 497500);
}

TEST_CASE("Config directory and app load keep defaults when files are absent",
          "[player][preferences]") {
    cuexis::player_support::ConfigDirectoryEnvironment windows;
    windows.windows = true;
    windows.appData = "C:/Users/cuexis/AppData/Roaming";
    const auto windowsDirectory = cuexis::player_support::userConfigDirectory(windows);
    REQUIRE(windowsDirectory.has_value());
    CHECK(windowsDirectory->filename() == "Cuexis");

    cuexis::player_support::ConfigDirectoryEnvironment missing;
    missing.windows = true;
    CHECK_FALSE(cuexis::player_support::userConfigDirectory(missing).has_value());

    cuexis::player_support::ConfigDirectoryEnvironment linuxHome;
    linuxHome.home = "/home/cuexis";
    const auto linuxDirectory = cuexis::player_support::userConfigDirectory(linuxHome);
    REQUIRE(linuxDirectory.has_value());
    CHECK(linuxDirectory->filename() == "Cuexis");

    const auto root = tempDirectory();
    const auto preferencesSchema = schemaPath("cuexis.player-preferences.v1.schema.json");
    const auto profileSchema = schemaPath("cuexis.audio-device-profile.v1.schema.json");
    const auto loaded = cuexis::player_support::loadAppConfig(root / "empty-config",
                                                              preferencesSchema, profileSchema);
    REQUIRE(loaded.has_value());
    CHECK(loaded->app.preferencesSource == cuexis::player_support::ConfigValueSource::CodeDefault);
    CHECK(loaded->profile.selector == cuexis::player_support::AudioSelectorKind::SystemDefault);
    CHECK(loaded->profile.outputCorrectionUs == 0);
    CHECK(loaded->diagnostics.items().back().code() == "player.audio_profile.default");

    writeText(
        root / "named-preferences.json",
        R"({"format":"cuexis.player-preferences","version":1,"windowWidth":1280,"windowHeight":720,"fullscreen":false,"vsync":true,"gain":1,"audioDeviceProfileId":"desk-speakers"})");
    const auto configRoot = root / "named-config";
    std::filesystem::create_directories(configRoot);
    std::filesystem::copy_file(root / "named-preferences.json",
                               cuexis::player_support::preferencesFilePath(configRoot),
                               std::filesystem::copy_options::overwrite_existing);
    const auto missingProfile =
        cuexis::player_support::loadAppConfig(configRoot, preferencesSchema, profileSchema);
    REQUIRE_FALSE(missingProfile.has_value());
    CHECK(missingProfile.error().code() == "player.audio_profile.missing");

    CHECK_FALSE(cuexis::player_support::audioProfileFilePath(configRoot, "../escape").has_value());
    const auto blocked = root / "not-a-directory";
    writeText(blocked, "file");
    const auto rejected = cuexis::player_support::saveUserPreferences(
        blocked / "preferences.json", cuexis::player_support::defaultUserPreferences(),
        preferencesSchema);
    REQUIRE_FALSE(rejected.has_value());
    std::ifstream blockedInput{blocked, std::ios::binary};
    std::string blockedText{(std::istreambuf_iterator<char>(blockedInput)),
                            std::istreambuf_iterator<char>()};
    CHECK(blockedText == "file");

    const auto corrected = cuexis::player_support::correctConsumedAudioPositionMs(1.0, 2500);
    REQUIRE(corrected.has_value());
    CHECK(*corrected == 0.0);
    const auto negative = cuexis::player_support::correctConsumedAudioPositionMs(1000.0, -12500);
    REQUIRE(negative.has_value());
    CHECK(*negative == 1012.5);
}

TEST_CASE("Session configuration identity ignores window state", "[player][session-config]") {
    using cuexis::player_support::PlaybackClockMode;
    cuexis::player_support::ResolvedSessionConfig chart;
    chart.clockMode = PlaybackClockMode::ChartClock;
    chart.outputCorrectionUs =
        cuexis::player_support::consumedOutputCorrectionUs(PlaybackClockMode::ChartClock, -12500);
    const auto first = cuexis::player_support::resolvedSessionConfigIdentity(chart);
    const auto second = cuexis::player_support::resolvedSessionConfigIdentity(chart);
    CHECK(first == second);
    CHECK(chart.outputCorrectionUs == 0);

    cuexis::player_support::ResolvedSessionConfig audio;
    audio.clockMode = PlaybackClockMode::CuexisAudio;
    audio.outputCorrectionUs =
        cuexis::player_support::consumedOutputCorrectionUs(PlaybackClockMode::CuexisAudio, -12500);
    CHECK(cuexis::player_support::resolvedSessionConfigIdentity(audio) != first);
}
