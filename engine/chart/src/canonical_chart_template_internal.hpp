#pragma once

#include <cuexis/chart/chart_document.hpp>
#include <cuexis/chart/limits.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/json/reader.hpp>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::chart::detail {

using ReadIdentifierFn = std::optional<std::string> (*)(const json::Reader&, const ChartLimits&,
                                                        core::Diagnostics&, std::string_view);
using ReadReferenceFn = std::optional<std::string> (*)(const json::Reader&, std::string_view,
                                                       const ChartLimits&, core::Diagnostics&);
using ReadNullableNameFn = std::optional<std::string> (*)(const json::Reader&, const ChartLimits&,
                                                          core::Diagnostics&);
using OpaqueJsonFn = OpaqueJson (*)(const json::Value&, core::Diagnostics&, std::string_view);
using ParseComponentsFn = std::optional<ObjectComponents> (*)(const json::Value&, std::string,
                                                              const ChartLimits&,
                                                              core::Diagnostics&, bool);

struct TemplateParserCallbacks final {
    ReadIdentifierFn readIdentifier;
    ReadReferenceFn readReference;
    ReadNullableNameFn readNullableName;
    OpaqueJsonFn opaqueJson;
    ParseComponentsFn parseComponents;
};

void applyPatches(json::Value& components, const json::Value::Array& patches,
                  std::string_view patchesPath, const ChartLimits& limits,
                  core::Diagnostics& diagnostics);

[[nodiscard]] auto parseTemplates(const json::Reader& reader, const ChartLimits& limits,
                                  core::Diagnostics& diagnostics,
                                  const TemplateParserCallbacks& callbacks)
    -> std::pair<std::vector<ChartTemplate>, std::map<std::string, json::Value>>;

} // namespace cuexis::chart::detail
