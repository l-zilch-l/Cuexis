#include <cuexis/player_support/audio_device_profile.hpp>
#include <cuexis/player_support/config_location.hpp>
#include <cuexis/player_support/resolved_config.hpp>
#include <cuexis/player_support/user_preferences.hpp>

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <thread>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

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

[[nodiscard]] auto readText(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

} // namespace

int holdPreferencesLock(int argc, char** argv) {
    if (argc != 4) {
        return 2;
    }
    const std::filesystem::path lockPath{argv[1]};
    const std::filesystem::path readyPath{argv[2]};
    const std::filesystem::path releasePath{argv[3]};
#if defined(_WIN32)
    const HANDLE handle = CreateFileW(lockPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return 3;
    }
#else
    std::FILE* handle = std::fopen(lockPath.string().c_str(), "wx");
    if (handle == nullptr) {
        return 3;
    }
#endif
    {
        std::ofstream ready{readyPath, std::ios::binary | std::ios::trunc};
        ready << "ready";
    }
    for (int attempt = 0; attempt < 400; ++attempt) {
        std::error_code error;
        if (std::filesystem::exists(releasePath, error)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
#if defined(_WIN32)
    CloseHandle(handle);
#else
    std::fclose(handle);
#endif
    std::error_code removeError;
    std::filesystem::remove(lockPath, removeError);
    return 0;
}

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

TEST_CASE("A second process holding the lock leaves the saved preferences unchanged",
          "[player][preferences][process]") {
    const auto root = tempDirectory() / "process-lock";
    std::filesystem::create_directories(root);
    const auto schema = schemaPath("cuexis.player-preferences.v1.schema.json");
    const auto path = root / "preferences.json";
    const auto lockPath = std::filesystem::path{path.string() + ".lock"};
    const auto readyPath = root / "ready";
    const auto releasePath = root / "release";
    std::filesystem::remove(path);
    std::filesystem::remove(lockPath);
    std::filesystem::remove(readyPath);
    std::filesystem::remove(releasePath);
    auto preferences = cuexis::player_support::defaultUserPreferences();
    preferences.gain = 0.5;
    REQUIRE(cuexis::player_support::saveUserPreferences(path, preferences, schema).has_value());
    const auto original = readText(path);

#if defined(_WIN32)
    std::wstring buffer(32768, L'\0');
    const DWORD length =
        GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    REQUIRE(length > 0);
    buffer.resize(length);
    const std::filesystem::path executable{buffer};
    std::wstring command = L"\"" + executable.wstring() + L"\" --hold-preferences-lock \"" +
                           lockPath.wstring() + L"\" \"" + readyPath.wstring() + L"\" \"" +
                           releasePath.wstring() + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    REQUIRE(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                           nullptr, nullptr, &startup, &process) != 0);
    struct ProcessGuard {
        PROCESS_INFORMATION* process{nullptr};
        std::filesystem::path release;
        ~ProcessGuard() {
            {
                std::ofstream output{release, std::ios::binary | std::ios::trunc};
                output << "go";
            }
            if (process != nullptr && process->hProcess != nullptr) {
                WaitForSingleObject(process->hProcess, 5000);
                CloseHandle(process->hThread);
                CloseHandle(process->hProcess);
            }
        }
    } guard{&process, releasePath};
#else
    std::string executable(4096, '\0');
    const auto length = ::readlink("/proc/self/exe", executable.data(), executable.size());
    REQUIRE(length > 0);
    executable.resize(static_cast<std::size_t>(length));
    const pid_t child = ::fork();
    REQUIRE(child >= 0);
    if (child == 0) {
        const char* arguments[] = {executable.c_str(), "--hold-preferences-lock", lockPath.c_str(),
                                   readyPath.c_str(),  releasePath.c_str(),       nullptr};
        ::execv(executable.c_str(), const_cast<char**>(arguments));
        _exit(127);
    }
    struct ProcessGuard {
        pid_t child{-1};
        std::filesystem::path release;
        ~ProcessGuard() {
            {
                std::ofstream output{release, std::ios::binary | std::ios::trunc};
                output << "go";
            }
            if (child > 0) {
                int status = 0;
                ::waitpid(child, &status, 0);
            }
        }
    } guard{child, releasePath};
#endif
    bool ready = false;
    for (int attempt = 0; attempt < 200 && !ready; ++attempt) {
        std::error_code error;
        ready = std::filesystem::exists(readyPath, error);
        if (!ready) {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
    }
    REQUIRE(ready);
    preferences.gain = 0.25;
    const auto busy = cuexis::player_support::saveUserPreferences(path, preferences, schema);
    REQUIRE_FALSE(busy.has_value());
    CHECK(busy.error().code() == "player.preferences.busy");
    CHECK(readText(path) == original);
}

TEST_CASE("Read-only preferences keep the last valid file", "[player][preferences]") {
    const auto root = tempDirectory() / "readonly-preferences";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto schema = schemaPath("cuexis.player-preferences.v1.schema.json");
    const auto path = root / "preferences.json";
    auto preferences = cuexis::player_support::defaultUserPreferences();
    preferences.windowWidth = 1440;
    REQUIRE(cuexis::player_support::saveUserPreferences(path, preferences, schema).has_value());
    const auto original = readText(path);
    const auto previous = std::filesystem::status(root).permissions();
    std::filesystem::permissions(root,
                                 std::filesystem::perms::owner_write |
                                     std::filesystem::perms::group_write |
                                     std::filesystem::perms::others_write,
                                 std::filesystem::perm_options::remove);
#if defined(_WIN32)
    REQUIRE(SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_READONLY) != 0);
#endif
    struct RestoreGuard {
        std::filesystem::path directory;
        std::filesystem::path file;
        std::filesystem::perms permissions;
        ~RestoreGuard() {
            std::error_code error;
#if defined(_WIN32)
            SetFileAttributesW(file.c_str(), FILE_ATTRIBUTE_NORMAL);
#endif
            std::filesystem::permissions(directory, permissions,
                                         std::filesystem::perm_options::replace, error);
        }
    } restore{root, path, previous};
    preferences.windowWidth = 800;
    const auto rejected = cuexis::player_support::saveUserPreferences(path, preferences, schema);
    REQUIRE_FALSE(rejected.has_value());
    CHECK(readText(path) == original);
}

TEST_CASE("Published app config does not reread a later preferences edit",
          "[player][preferences]") {
    const auto root = tempDirectory() / "frozen-config";
    std::filesystem::create_directories(root);
    const auto preferencesSchema = schemaPath("cuexis.player-preferences.v1.schema.json");
    const auto profileSchema = schemaPath("cuexis.audio-device-profile.v1.schema.json");
    auto preferences = cuexis::player_support::defaultUserPreferences();
    preferences.gain = 0.5;
    REQUIRE(cuexis::player_support::saveUserPreferences(
                cuexis::player_support::preferencesFilePath(root), preferences, preferencesSchema)
                .has_value());
    const auto published =
        cuexis::player_support::loadAppConfig(root, preferencesSchema, profileSchema);
    REQUIRE(published.has_value());
    CHECK(published->app.requested.gain == 0.5);
    preferences.gain = 0.25;
    REQUIRE(cuexis::player_support::saveUserPreferences(
                cuexis::player_support::preferencesFilePath(root), preferences, preferencesSchema)
                .has_value());
    CHECK(published->app.requested.gain == 0.5);
    const auto reread =
        cuexis::player_support::loadAppConfig(root, preferencesSchema, profileSchema);
    REQUIRE(reread.has_value());
    CHECK(reread->app.requested.gain == 0.25);
}

TEST_CASE("Hotplug removal, ambiguity, and format change fail closed", "[player][audio-profile]") {
    using cuexis::player_support::AudioDeviceMatch;
    using cuexis::player_support::AudioOutputDevice;
    using cuexis::player_support::OpenedAudioFormat;
    AudioDeviceMatch bound;
    bound.systemDefault = false;
    bound.driver = "wasapi";
    bound.deviceName = "Speakers";
    const AudioOutputDevice present[] = {{.driver = "wasapi", .deviceName = "Speakers"}};
    const OpenedAudioFormat opened{.sampleRate = 48000, .channelCount = 2};
    const auto stable = cuexis::player_support::observeOpenedFormat(bound, present, opened, opened);
    REQUIRE(stable.has_value());
    CHECK(stable->deviceName == "Speakers");

    const auto removed = cuexis::player_support::observeOpenedFormat(bound, {}, opened, opened);
    REQUIRE_FALSE(removed.has_value());
    CHECK(removed.error().code() == "player.audio_profile.unmatched");

    const AudioOutputDevice duplicates[] = {{.driver = "wasapi", .deviceName = "Speakers"},
                                            {.driver = "wasapi", .deviceName = "Speakers"}};
    const auto ambiguous =
        cuexis::player_support::observeOpenedFormat(bound, duplicates, opened, opened);
    REQUIRE_FALSE(ambiguous.has_value());
    CHECK(ambiguous.error().code() == "player.audio_profile.ambiguous");

    const auto changed = cuexis::player_support::observeOpenedFormat(
        bound, present, opened, OpenedAudioFormat{.sampleRate = 44100, .channelCount = 2});
    REQUIRE_FALSE(changed.has_value());
    CHECK(changed.error().code() == "player.audio_profile.format_changed");

    AudioDeviceMatch route;
    route.systemDefault = true;
    const auto routeChanged = cuexis::player_support::observeOpenedFormat(
        route, {}, opened, OpenedAudioFormat{.sampleRate = 48000, .channelCount = 1});
    REQUIRE_FALSE(routeChanged.has_value());
    CHECK(routeChanged.error().code() == "player.audio_profile.format_changed");
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

int main(int argc, char** argv) {
    if (argc == 5 && std::string_view{argv[1]} == "--hold-preferences-lock") {
        return holdPreferencesLock(argc - 1, argv + 1);
    }
    return Catch::Session().run(argc, argv);
}
