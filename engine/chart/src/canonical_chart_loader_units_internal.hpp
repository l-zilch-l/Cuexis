#pragma once

// The canonical chart reader is split into a primitive unit, two section units and the entry
// point. This header names the readers the units share, so a value is read one way everywhere.

#include <cuexis/chart/canonical_chart_loader.hpp>
#include <cuexis/chart/chart_document.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/json/reader.hpp>
#include <cuexis/json/value.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::chart {

using core::DiagnosticSeverity;

[[nodiscard]] void addError(core::Diagnostics& diagnostics, std::string code, std::string message,
                            std::string path);

[[nodiscard]] void addWarning(core::Diagnostics& diagnostics, std::string code, std::string message,
                              std::string path);

[[nodiscard]] void addError(core::Diagnostics& diagnostics, const core::Error& error,
                            std::string path);

[[nodiscard]] auto isChartEntityUuid(std::string_view value) noexcept -> bool;

[[nodiscard]] auto isExtensionId(std::string_view value) noexcept -> bool;

[[nodiscard]] auto opaqueJson(const json::Value& value, core::Diagnostics& diagnostics,
                              std::string_view path) -> OpaqueJson;

[[nodiscard]] auto readIdentifier(const json::Reader& reader, const ChartLimits& limits,
                                  core::Diagnostics& diagnostics, std::string_view purpose)
    -> std::optional<std::string>;

[[nodiscard]] auto readVersion(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<std::uint32_t>;

[[nodiscard]] auto readSupportedComponentVersion(const json::Reader& reader,
                                                 core::Diagnostics& diagnostics) -> bool;

[[nodiscard]] auto readVec3(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<core::Vec3>;

[[nodiscard]] auto readQuat(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<core::Quat>;

[[nodiscard]] auto readNullableName(const json::Reader& objectReader, const ChartLimits& limits,
                                    core::Diagnostics& diagnostics) -> std::optional<std::string>;

[[nodiscard]] auto readReference(const json::Reader& reader, std::string_view expectedDomain,
                                 const ChartLimits& limits, core::Diagnostics& diagnostics)
    -> std::optional<std::string>;

[[nodiscard]] auto readChartAudio(const json::Reader& reader, const ChartLimits& limits,
                                  core::Diagnostics& diagnostics) -> std::optional<ChartAudioData>;

[[nodiscard]] auto readBeat(const json::Reader& reader, const ChartLimits& limits,
                            core::Diagnostics& diagnostics) -> std::optional<RationalBeat>;

[[nodiscard]] auto readBehaviorProperty(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<BehaviorProperty>;

[[nodiscard]] auto readBehaviorEasing(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<BehaviorEasing>;

[[nodiscard]] auto readBehaviorValue(const json::Reader& reader, BehaviorProperty property,
                                     core::Diagnostics& diagnostics)
    -> std::optional<BehaviorValue>;

[[nodiscard]] auto readEventProperty(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<BehaviorProperty>;

[[nodiscard]] auto readStepProperty(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> std::optional<BehaviorStepProperty>;

[[nodiscard]] auto readGroupId(const json::Reader& reader, const ChartLimits& limits,
                               core::Diagnostics& diagnostics) -> std::optional<std::string>;

[[nodiscard]] auto readComponentVersion(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> bool;

[[nodiscard]] auto parseComponents(const json::Value& value, std::string path,
                                   const ChartLimits& limits, core::Diagnostics& diagnostics,
                                   bool concreteObject) -> std::optional<ObjectComponents>;

[[nodiscard]] auto parseBehaviors(const json::Reader& reader, const ChartLimits& limits,
                                  core::Diagnostics& diagnostics) -> std::vector<ChartBehavior>;

[[nodiscard]] auto parseEventBehaviors(const json::Reader& reader, const ChartLimits& limits,
                                       core::Diagnostics& diagnostics)
    -> std::vector<ChartBehavior>;

[[nodiscard]] auto parseObjects(const json::Reader& reader,
                                const std::map<std::string, json::Value>& templates,
                                const ChartLimits& limits, core::Diagnostics& diagnostics)
    -> std::vector<ChartObject>;

[[nodiscard]] auto parseTiming(const json::Reader& reader, std::uint32_t formatVersion,
                               const ChartLimits& limits, core::Diagnostics& diagnostics)
    -> std::optional<ChartTiming>;

[[nodiscard]] auto readCamera(const json::Reader& reader, core::Diagnostics& diagnostics)
    -> CameraData;

} // namespace cuexis::chart
