#pragma once

// The cxt v2 loader reads a declarative template into a small module model and then expands it
// against one invocation. The readers, the expansion and the entry point share the model, so it
// lives here together with the readers they call.

#include <cuexis/chart/cxt_v2_loader.hpp>
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/core/result.hpp>
#include <cuexis/json/value.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::chart {

using json::Value;

enum class ValueKind : std::uint8_t { Integer, Beat };
enum class SourceKind : std::uint8_t { Literal, Parameter, Slot, Index, Affine };

struct Source final {
    SourceKind kind{};
    ValueKind valueKind{};
    CxtV2Value literal{std::int64_t{0}};
    std::string id;
    std::optional<std::unique_ptr<Source>> input;
    std::optional<std::unique_ptr<Source>> scale;
    std::optional<std::unique_ptr<Source>> offset;
};

struct Slot final {
    std::string id;
    ValueKind kind{};
    bool required{};
    std::optional<CxtV2Value> defaultValue;
    std::optional<CxtV2Value> minimum;
    std::optional<CxtV2Value> maximum;
};

struct Requirement final {
    std::string id;
    Source beat;
    Source lane;
};

struct Prototype final {
    std::string id;
    std::map<std::string, Slot, std::less<>> slots;
    std::vector<Requirement> requirements;
    bool hasTransform{};
    std::optional<Source> positionX;
};

struct Node final {
    enum class Op : std::uint8_t { Emit, Repeat };
    enum class ParentKind : std::uint8_t { Invocation, Root, Emission };
    Op op{};
    std::string nodeId;
    std::string prototype;
    std::map<std::string, Source, std::less<>> bindings;
    Source count;
    std::string indexId;
    std::vector<Node> body;
    ParentKind parentKind{ParentKind::Invocation};
    std::string parentNodeId;
};

struct Pattern final {
    std::string id;
    std::vector<Node> nodes;
};

struct Parameter final {
    ValueKind kind{};
    CxtV2Value defaultValue{std::int64_t{0}};
    std::optional<CxtV2Value> minimum;
    std::optional<CxtV2Value> maximum;
};

struct Module final {
    std::string id;
    std::string moduleKind;
    std::string exportId;
    std::map<std::string, Parameter, std::less<>> parameters;
    std::map<std::string, Prototype, std::less<>> prototypes;
    std::map<std::string, Pattern, std::less<>> patterns;
};

using Bindings = std::map<std::string, CxtV2Value, std::less<>>;

struct ExpansionState final {
    CanonicalSemanticChart chart;
    std::size_t requirementCount{};
    struct PendingParent final {
        std::size_t entityIndex{};
        Node::ParentKind kind{Node::ParentKind::Invocation};
        std::string nodeId;
        std::vector<SemanticIdentityStep> scopePath;
    };
    std::vector<PendingParent> pendingParents;
};

void addCxtError(core::Diagnostics& diagnostics, std::string code, std::string message,
                 std::string path);

[[nodiscard]] auto childPath(std::string_view parent, std::string_view child) -> std::string;

[[nodiscard]] auto indexPath(std::string_view parent, std::size_t index) -> std::string;

[[nodiscard]] auto integer(const Value& value) -> std::optional<std::int64_t>;

[[nodiscard]] auto string(const Value& value) -> std::optional<std::string_view>;

[[nodiscard]] auto object(const Value& value, core::Diagnostics& diagnostics, std::string_view path)
    -> const Value::Object*;

[[nodiscard]] auto array(const Value& value, core::Diagnostics& diagnostics, std::string_view path)
    -> const Value::Array*;

[[nodiscard]] auto field(const Value::Object& value, std::string_view name,
                         core::Diagnostics& diagnostics, std::string_view path) -> const Value*;

void rejectUnknown(const Value::Object& value, std::initializer_list<std::string_view> known,
                   core::Diagnostics& diagnostics, std::string_view path);

[[nodiscard]] auto requireEmptyObject(const Value::Object& parent, std::string_view name,
                                      core::Diagnostics& diagnostics, std::string_view path)
    -> bool;

[[nodiscard]] auto requireEmptyArray(const Value::Object& parent, std::string_view name,
                                     core::Diagnostics& diagnostics, std::string_view path) -> bool;

[[nodiscard]] auto checkedAdd(std::int64_t left, std::int64_t right) -> core::Result<std::int64_t>;

[[nodiscard]] auto checkedMultiply(std::int64_t left, std::int64_t right)
    -> core::Result<std::int64_t>;

[[nodiscard]] auto stableId(std::string_view value) -> bool;

[[nodiscard]] auto readId(const Value& value, core::Diagnostics& diagnostics, std::string_view path)
    -> std::optional<std::string>;

[[nodiscard]] auto readBeat(const Value& value, core::Diagnostics& diagnostics,
                            std::string_view path) -> std::optional<RationalBeat>;

[[nodiscard]] auto parseSource(const Value& value, core::Diagnostics& diagnostics,
                               std::string_view path, bool allowIndex, bool allowSlot,
                               bool allowParameter) -> std::optional<Source>;

[[nodiscard]] auto inDeclaredRange(const CxtV2Value& value,
                                   const std::optional<CxtV2Value>& minimum,
                                   const std::optional<CxtV2Value>& maximum) -> bool;

[[nodiscard]] auto parseParameter(const Value& value, core::Diagnostics& diagnostics,
                                  std::string_view path)
    -> std::optional<std::pair<std::string, Parameter>>;

[[nodiscard]] auto parseSlot(const Value& value, core::Diagnostics& diagnostics,
                             std::string_view path) -> std::optional<std::pair<std::string, Slot>>;

[[nodiscard]] auto isLiteralReference(const Value& value, std::string_view domain,
                                      std::string_view id, core::Diagnostics& diagnostics,
                                      std::string_view path) -> bool;

[[nodiscard]] auto parseModule(const Value& root, core::Diagnostics& diagnostics,
                               const ChartLimits& limits) -> std::optional<Module>;

[[nodiscard]] auto valueMatches(const CxtV2Value& value, ValueKind kind) noexcept -> bool;

[[nodiscard]] auto countFor(const Source& source, const Bindings& parameters,
                            core::Diagnostics& diagnostics) -> std::optional<std::size_t>;

[[nodiscard]] auto valueMatches(const CxtV2Value& value, const Slot& slot) -> bool;

[[nodiscard]] auto identityPathLess(const std::vector<SemanticIdentityStep>& left,
                                    const std::vector<SemanticIdentityStep>& right) noexcept
    -> bool;

void expandNodes(const std::vector<Node>& nodes, const Module& module, const Pattern& pattern,
                 const CxtV2Invocation& invocation, const Bindings& parameters, Bindings& indices,
                 std::vector<SemanticIdentityStep>& path, ExpansionState& state,
                 core::Diagnostics& diagnostics, const ChartLimits& limits, std::size_t depth);

} // namespace cuexis::chart
