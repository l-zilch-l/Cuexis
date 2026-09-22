#include <cuexis/player_support/audio_device_profile.hpp>

#include "config_json.hpp"

#include <limits>

namespace cuexis::player_support {
namespace {

[[nodiscard]] auto unsupportedProfileVersion(const json::Value& value) -> bool {
    const auto* object = value.object();
    if (object == nullptr) {
        return false;
    }
    const auto* format = detail::objectField(*object, "format");
    const auto* version = detail::objectField(*object, "version");
    if (format == nullptr || format->string() == nullptr || version == nullptr) {
        return false;
    }
    if (*format->string() != "cuexis.audio-device-profile") {
        return false;
    }
    const auto versionNumber = detail::readInteger(*version);
    return versionNumber.has_value() && *versionNumber != 1;
}

[[nodiscard]] auto readProfile(const json::Value& value) -> core::Result<AudioDeviceProfile> {
    const auto* object = value.object();
    if (object == nullptr) {
        return core::unexpected(
            core::Error{"player.audio_profile.malformed", "Audio profile JSON must be an object"});
    }
    const auto* id = detail::objectField(*object, "id");
    const auto* selectorValue = detail::objectField(*object, "selector");
    const auto* correctionValue = detail::objectField(*object, "outputCorrectionUs");
    if (id == nullptr || id->string() == nullptr || selectorValue == nullptr ||
        selectorValue->object() == nullptr || correctionValue == nullptr) {
        return core::unexpected(core::Error{"player.audio_profile.malformed",
                                            "Audio profile JSON is missing a required field"});
    }
    const auto correction = detail::readInteger(*correctionValue);
    if (!correction) {
        return core::unexpected(core::Error{"player.audio_profile.malformed",
                                            "Audio profile correction must be an integer"});
    }
    const auto& selector = *selectorValue->object();
    const auto* kind = detail::objectField(selector, "kind");
    if (kind == nullptr || kind->string() == nullptr) {
        return core::unexpected(
            core::Error{"player.audio_profile.malformed", "Audio profile selector is invalid"});
    }
    AudioDeviceProfile profile;
    profile.id = *id->string();
    profile.outputCorrectionUs = *correction;
    if (*kind->string() == "system-default") {
        profile.selector = AudioSelectorKind::SystemDefault;
        return profile;
    }
    if (*kind->string() != "exact") {
        return core::unexpected(
            core::Error{"player.audio_profile.malformed", "Audio profile selector is invalid"});
    }
    const auto* driver = detail::objectField(selector, "driver");
    const auto* deviceName = detail::objectField(selector, "deviceName");
    if (driver == nullptr || driver->string() == nullptr || deviceName == nullptr ||
        deviceName->string() == nullptr) {
        return core::unexpected(
            core::Error{"player.audio_profile.malformed",
                        "Exact audio selector is missing driver or device name"});
    }
    profile.selector = AudioSelectorKind::Exact;
    profile.driver = *driver->string();
    profile.deviceName = *deviceName->string();
    return profile;
}

[[nodiscard]] auto checkedAdd(std::int64_t left, std::int64_t right) -> core::Result<std::int64_t> {
    if ((right > 0 && left > std::numeric_limits<std::int64_t>::max() - right) ||
        (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right)) {
        return core::unexpected(core::Error{"player.audio_profile.correction_overflow",
                                            "Audio correction arithmetic is outside int64 range"});
    }
    return left + right;
}

[[nodiscard]] auto checkedSubtract(std::int64_t left, std::int64_t right)
    -> core::Result<std::int64_t> {
    if ((right > 0 && left < std::numeric_limits<std::int64_t>::min() + right) ||
        (right < 0 && left > std::numeric_limits<std::int64_t>::max() + right)) {
        return core::unexpected(core::Error{"player.audio_profile.correction_overflow",
                                            "Audio correction arithmetic is outside int64 range"});
    }
    return left - right;
}

} // namespace

auto loadAudioDeviceProfile(const std::filesystem::path& profilePath,
                            const std::filesystem::path& schemaPath)
    -> core::Result<AudioDeviceProfile> {
    auto schema = detail::loadSchema(schemaPath);
    if (!schema) {
        return core::unexpected(std::move(schema.error()));
    }
    auto text = detail::readTextFile(profilePath);
    if (!text) {
        return core::unexpected(
            core::Error{"player.audio_profile.missing", "The audio device profile file is missing"}
                .withContext("path", profilePath.string()));
    }
    auto parsed = json::parse(*text, detail::configParseLimits);
    if (!parsed) {
        return core::unexpected(
            core::Error{"player.audio_profile.malformed", std::string{parsed.error().message()}}
                .withContext("path", profilePath.string()));
    }
    if (unsupportedProfileVersion(*parsed)) {
        return core::unexpected(core::Error{"player.audio_profile.unsupported_version",
                                            "The audio device profile version is not supported"}
                                    .withContext("path", profilePath.string()));
    }
    core::Diagnostics diagnostics;
    if (auto validated = json::validateAgainstSchema(*parsed, *schema, diagnostics);
        !validated || !diagnostics.empty()) {
        return core::unexpected(core::Error{"player.audio_profile.malformed",
                                            "The audio device profile failed schema validation"}
                                    .withContext("path", profilePath.string()));
    }
    return readProfile(*parsed);
}

auto matchAudioDevice(const AudioDeviceProfile& profile, std::span<const AudioOutputDevice> devices)
    -> core::Result<AudioDeviceMatch> {
    if (profile.selector == AudioSelectorKind::SystemDefault) {
        if (profile.outputCorrectionUs != 0) {
            return core::unexpected(
                core::Error{"player.audio_profile.malformed",
                            "The system-default route cannot carry correction"});
        }
        return AudioDeviceMatch{.systemDefault = true};
    }
    const AudioOutputDevice* matched = nullptr;
    for (const auto& device : devices) {
        if (device.driver == profile.driver && device.deviceName == profile.deviceName) {
            if (matched != nullptr) {
                return core::unexpected(
                    core::Error{"player.audio_profile.ambiguous",
                                "More than one output device matches the profile"}
                        .withContext("profile_id", profile.id));
            }
            matched = &device;
        }
    }
    if (matched == nullptr) {
        return core::unexpected(core::Error{"player.audio_profile.unmatched",
                                            "No output device matches the explicit profile"}
                                    .withContext("profile_id", profile.id));
    }
    return AudioDeviceMatch{
        .systemDefault = false, .driver = matched->driver, .deviceName = matched->deviceName};
}

auto correctedAudioPositionUs(std::int64_t rawAudioPositionUs, std::int64_t correctionUs)
    -> core::Result<std::int64_t> {
    auto delta = checkedSubtract(rawAudioPositionUs, correctionUs);
    if (!delta) {
        return core::unexpected(std::move(delta.error()));
    }
    return *delta < 0 ? 0 : *delta;
}

auto reverseSeekSourcePositionUs(std::int64_t targetChartTimeUs, std::int64_t timingOffsetUs,
                                 std::int64_t correctionUs) -> core::Result<std::int64_t> {
    auto withOffset = checkedAdd(targetChartTimeUs, timingOffsetUs);
    if (!withOffset) {
        return core::unexpected(std::move(withOffset.error()));
    }
    return checkedAdd(*withOffset, correctionUs);
}

} // namespace cuexis::player_support
