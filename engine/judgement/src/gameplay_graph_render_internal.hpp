#pragma once

// The deterministic renderings and the diagnostic constructors of the canonical gameplay graph.
// The graph units keep the declaration model, the geometry checks, the diff recorder and the
// closure derivation; the rendering unit owns every string form they need, so all of them spell a
// value the same way.

#include <cuexis/judgement/gameplay_graph.hpp>

#include "source_codes.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::judgement {

inline constexpr std::int64_t kInt64Min = std::numeric_limits<std::int64_t>::min();
inline constexpr std::int64_t kInt64Max = std::numeric_limits<std::int64_t>::max();

struct DiagnosticTokens final {
    std::string_view code;
    std::string_view category;
    std::string_view summary;
    std::string_view section;
    std::string_view path;
    std::string_view capabilityId = codes::kAbsent;
    std::string_view remediation = codes::kAbsent;
};

[[nodiscard]] auto declarationInvalidError(std::string_view code, std::string_view section,
                                           std::string_view path, std::string_view summary)
    -> core::Error;

[[nodiscard]] auto valueError(std::string_view code, std::string_view section,
                              std::string_view path, std::string_view summary) -> core::Error;

[[nodiscard]] auto capabilityRejection(std::string_view code, std::string_view section,
                                       std::string_view path, std::string_view summary,
                                       std::string_view capabilityId, std::string_view remediation)
    -> core::Error;

[[nodiscard]] auto renderText(std::string_view text) -> std::string;

[[nodiscard]] auto renderBool(bool value) -> std::string;

[[nodiscard]] auto renderTokens(const std::vector<std::string>& tokens) -> std::string;

[[nodiscard]] auto renderFeatureRefs(const std::vector<FeatureRef>& refs) -> std::string;

[[nodiscard]] auto renderCapabilityRefs(const std::vector<CapabilityRef>& refs) -> std::string;

[[nodiscard]] auto renderStableIds(const std::vector<StableDeclarationId>& ids) -> std::string;

[[nodiscard]] auto renderRequirementIdentity(const RequirementIdentity& identity) -> std::string;

[[nodiscard]] auto renderPhaseKind(PhaseKind kind) -> std::string;

[[nodiscard]] auto renderGracePolicy(GraceResolutionPolicy policy) -> std::string;

[[nodiscard]] auto renderPatternNode(const PatternNodeDeclaration& root) -> std::string;

[[nodiscard]] auto sameTimebase(const TimebaseProfile& left, const TimebaseProfile& right) -> bool;

[[nodiscard]] auto renderTimebase(const TimebaseProfile& profile) -> std::string;

[[nodiscard]] auto renderLatePolicy(const LatePolicyParameters& parameters) -> std::string;

[[nodiscard]] auto
renderRequirementCapacity(const std::optional<MeasuredParameter<std::uint64_t>>& value)
    -> std::string;

template <typename Integer> [[nodiscard]] auto renderInteger(Integer value) -> std::string {
    return std::to_string(value);
}

} // namespace cuexis::judgement
