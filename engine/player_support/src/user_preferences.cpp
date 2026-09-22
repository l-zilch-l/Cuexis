#include <cuexis/player_support/user_preferences.hpp>

#include "config_json.hpp"

#include <cuexis/json/parse.hpp>

#include <cstdio>
#include <fstream>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace cuexis::player_support {
namespace {

class ExclusiveFileLock final {
  public:
    ExclusiveFileLock() = default;
    ExclusiveFileLock(const ExclusiveFileLock&) = delete;
    auto operator=(const ExclusiveFileLock&) -> ExclusiveFileLock& = delete;
    ExclusiveFileLock(ExclusiveFileLock&& other) noexcept
        : path_(std::move(other.path_)), held_(other.held_) {
        other.held_ = false;
    }
    auto operator=(ExclusiveFileLock&& other) noexcept -> ExclusiveFileLock& {
        if (this != &other) {
            release();
            path_ = std::move(other.path_);
            held_ = other.held_;
            other.held_ = false;
        }
        return *this;
    }
    ~ExclusiveFileLock() {
        release();
    }

    static auto acquire(const std::filesystem::path& path) -> core::Result<ExclusiveFileLock> {
        ExclusiveFileLock lock;
        lock.path_ = path;
        std::FILE* handle = nullptr;
#if defined(_MSC_VER)
        if (fopen_s(&handle, path.string().c_str(), "wx") != 0) {
            handle = nullptr;
        }
#else
        handle = std::fopen(path.string().c_str(), "wx");
#endif
        if (handle == nullptr) {
            return core::unexpected(
                core::Error{"player.preferences.busy", "Another writer holds the preferences lock"}
                    .withContext("path", path.string()));
        }
        std::fclose(handle);
        lock.held_ = true;
        return lock;
    }

  private:
    void release() noexcept {
        if (!held_) {
            return;
        }
        std::error_code error;
        std::filesystem::remove(path_, error);
        held_ = false;
    }

    std::filesystem::path path_{};
    bool held_{false};
};

[[nodiscard]] auto unsupportedVersion(const json::Value& value) -> bool {
    const auto* object = value.object();
    if (object == nullptr) {
        return false;
    }
    const auto* format = detail::objectField(*object, "format");
    const auto* version = detail::objectField(*object, "version");
    if (format == nullptr || format->string() == nullptr || version == nullptr) {
        return false;
    }
    if (*format->string() != "cuexis.player-preferences") {
        return false;
    }
    const auto versionNumber = detail::readInteger(*version);
    return versionNumber.has_value() && *versionNumber != 1;
}

[[nodiscard]] auto readPreferences(const json::Value& value) -> core::Result<UserPreferences> {
    const auto* object = value.object();
    if (object == nullptr) {
        return core::unexpected(
            core::Error{"player.preferences.malformed", "Preferences JSON must be an object"});
    }
    const auto* widthValue = detail::objectField(*object, "windowWidth");
    const auto* heightValue = detail::objectField(*object, "windowHeight");
    const auto* fullscreenValue = detail::objectField(*object, "fullscreen");
    const auto* vsyncValue = detail::objectField(*object, "vsync");
    const auto* profileValue = detail::objectField(*object, "audioDeviceProfileId");
    const auto* gainValue = detail::objectField(*object, "gain");
    if (widthValue == nullptr || heightValue == nullptr || fullscreenValue == nullptr ||
        vsyncValue == nullptr || profileValue == nullptr || gainValue == nullptr) {
        return core::unexpected(core::Error{"player.preferences.malformed",
                                            "Preferences JSON is missing a required field"});
    }
    UserPreferences preferences;
    const auto width = detail::readInteger(*widthValue);
    const auto height = detail::readInteger(*heightValue);
    const auto* fullscreen = fullscreenValue->boolean();
    const auto* vsync = vsyncValue->boolean();
    const auto* profile = profileValue->string();
    if (!width || !height || fullscreen == nullptr || vsync == nullptr || profile == nullptr ||
        gainValue == nullptr) {
        return core::unexpected(core::Error{"player.preferences.malformed",
                                            "Preferences JSON is missing a required field"});
    }
    double gain = 0.0;
    if (const auto* number = gainValue->number()) {
        gain = *number;
    } else if (const auto integer = detail::readInteger(*gainValue)) {
        gain = static_cast<double>(*integer);
    } else {
        return core::unexpected(
            core::Error{"player.preferences.malformed", "Preferences gain must be numeric"});
    }
    preferences.windowWidth = static_cast<int>(*width);
    preferences.windowHeight = static_cast<int>(*height);
    preferences.fullscreen = *fullscreen;
    preferences.vsync = *vsync;
    preferences.gain = gain;
    preferences.audioDeviceProfileId = *profile;
    return preferences;
}

[[nodiscard]] auto toJson(const UserPreferences& preferences) -> json::Value {
    json::Value::Object object;
    object.emplace("format", json::Value{std::string{"cuexis.player-preferences"}});
    object.emplace("version", json::Value{std::int64_t{1}});
    object.emplace("windowWidth", json::Value{static_cast<std::int64_t>(preferences.windowWidth)});
    object.emplace("windowHeight",
                   json::Value{static_cast<std::int64_t>(preferences.windowHeight)});
    object.emplace("fullscreen", json::Value{preferences.fullscreen});
    object.emplace("vsync", json::Value{preferences.vsync});
    object.emplace("gain", json::Value{preferences.gain});
    object.emplace("audioDeviceProfileId", json::Value{preferences.audioDeviceProfileId});
    return json::Value{std::move(object)};
}

[[nodiscard]] auto replaceFile(const std::filesystem::path& from, const std::filesystem::path& to)
    -> core::Result<void> {
#if defined(_WIN32)
    if (MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ==
        0) {
        return core::unexpected(core::Error{"player.preferences.replace_failed",
                                            "The preferences file could not be replaced"}
                                    .withContext("path", to.string()));
    }
    return {};
#else
    std::error_code error;
    std::filesystem::rename(from, to, error);
    if (error) {
        return core::unexpected(core::Error{"player.preferences.replace_failed",
                                            "The preferences file could not be replaced"}
                                    .withContext("path", to.string()));
    }
    return {};
#endif
}

void addDiagnostic(core::Diagnostics& diagnostics, std::string code, std::string message) {
    diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error, std::move(code),
                                     std::move(message), "$"});
}

} // namespace

auto defaultUserPreferences() noexcept -> UserPreferences {
    return {};
}

auto loadUserPreferences(const std::filesystem::path& preferencesPath,
                         const std::filesystem::path& schemaPath)
    -> core::Result<UserPreferencesLoad> {
    UserPreferencesLoad loaded;
    loaded.preferences = defaultUserPreferences();
    auto schema = detail::loadSchema(schemaPath);
    if (!schema) {
        return core::unexpected(std::move(schema.error()));
    }
    std::error_code existsError;
    if (!std::filesystem::exists(preferencesPath, existsError) || existsError) {
        loaded.usedDefaults = true;
        addDiagnostic(loaded.diagnostics, "player.preferences.missing",
                      "Preferences file is missing; code defaults are in use");
        return loaded;
    }
    auto text = detail::readTextFile(preferencesPath);
    if (!text) {
        loaded.usedDefaults = true;
        addDiagnostic(loaded.diagnostics, "player.preferences.malformed",
                      std::string{text.error().message()});
        return loaded;
    }
    auto parsed = json::parse(*text, detail::configParseLimits);
    if (!parsed) {
        loaded.usedDefaults = true;
        addDiagnostic(loaded.diagnostics, "player.preferences.malformed",
                      std::string{parsed.error().message()});
        return loaded;
    }
    if (unsupportedVersion(*parsed)) {
        loaded.usedDefaults = true;
        loaded.preserveExistingFile = true;
        addDiagnostic(loaded.diagnostics, "player.preferences.unsupported_version",
                      "Preferences version is not supported; the original file is preserved");
        return loaded;
    }
    if (auto validated = json::validateAgainstSchema(*parsed, *schema, loaded.diagnostics);
        !validated || !loaded.diagnostics.empty()) {
        loaded.preferences = defaultUserPreferences();
        loaded.usedDefaults = true;
        if (loaded.diagnostics.empty()) {
            addDiagnostic(loaded.diagnostics, "player.preferences.malformed",
                          validated ? std::string{"Preferences schema validation failed"}
                                    : std::string{validated.error().message()});
        }
        return loaded;
    }
    auto preferences = readPreferences(*parsed);
    if (!preferences) {
        loaded.usedDefaults = true;
        loaded.diagnostics.clear();
        addDiagnostic(loaded.diagnostics, "player.preferences.malformed",
                      std::string{preferences.error().message()});
        return loaded;
    }
    loaded.preferences = std::move(*preferences);
    return loaded;
}

auto saveUserPreferences(const std::filesystem::path& preferencesPath,
                         const UserPreferences& preferences,
                         const std::filesystem::path& schemaPath) -> core::Result<void> {
    auto schema = detail::loadSchema(schemaPath);
    if (!schema) {
        return core::unexpected(std::move(schema.error()));
    }
    std::error_code existsError;
    if (std::filesystem::exists(preferencesPath, existsError) && !existsError) {
        auto existingText = detail::readTextFile(preferencesPath);
        if (existingText) {
            auto existing = json::parse(*existingText, detail::configParseLimits);
            if (existing && unsupportedVersion(*existing)) {
                return core::unexpected(
                    core::Error{"player.preferences.unsupported_version",
                                "Refusing to overwrite a future preferences file"}
                        .withContext("path", preferencesPath.string()));
            }
        }
    }
    auto document = toJson(preferences);
    core::Diagnostics diagnostics;
    if (auto validated = json::validateAgainstSchema(document, *schema, diagnostics);
        !validated || !diagnostics.empty()) {
        return core::unexpected(core::Error{"player.preferences.malformed",
                                            "Preferences failed schema validation before save"});
    }
    auto serialized = json::serialize(document, json::SerializeStyle::Pretty);
    if (!serialized) {
        return core::unexpected(std::move(serialized.error()));
    }
    const auto lockPath = preferencesPath.string() + ".lock";
    auto lock = ExclusiveFileLock::acquire(lockPath);
    if (!lock) {
        return core::unexpected(std::move(lock.error()));
    }
    const auto temporaryPath = preferencesPath.string() + ".tmp";
    {
        std::ofstream output{temporaryPath, std::ios::binary | std::ios::trunc};
        if (!output) {
            std::error_code removeError;
            std::filesystem::remove(temporaryPath, removeError);
            return core::unexpected(
                core::Error{"player.preferences.write_failed",
                            "The temporary preferences file could not be created"}
                    .withContext("path", temporaryPath));
        }
        output << *serialized;
        output.flush();
        if (!output) {
            std::error_code removeError;
            std::filesystem::remove(temporaryPath, removeError);
            return core::unexpected(
                core::Error{"player.preferences.write_failed",
                            "The temporary preferences file could not be written"}
                    .withContext("path", temporaryPath));
        }
    }
    auto written = detail::readTextFile(temporaryPath);
    if (!written) {
        std::error_code removeError;
        std::filesystem::remove(temporaryPath, removeError);
        return core::unexpected(std::move(written.error()));
    }
    auto reparsed = json::parse(*written, detail::configParseLimits);
    core::Diagnostics rewritten;
    if (!reparsed || !json::validateAgainstSchema(*reparsed, *schema, rewritten) ||
        !rewritten.empty()) {
        std::error_code removeError;
        std::filesystem::remove(temporaryPath, removeError);
        return core::unexpected(core::Error{"player.preferences.malformed",
                                            "The temporary preferences file failed validation"});
    }
    auto replaced = replaceFile(temporaryPath, preferencesPath);
    if (!replaced) {
        std::error_code removeError;
        std::filesystem::remove(temporaryPath, removeError);
        return core::unexpected(std::move(replaced.error()));
    }
    return {};
}

} // namespace cuexis::player_support
