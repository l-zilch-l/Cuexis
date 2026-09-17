// Foundation revision 1 registered-subset (profile) validation.
//
// Spec 7.6 registers exactly one demo profile. Both the Writer and the Reader call this single
// implementation, so the accepted subset cannot drift between writing and reading, and every
// rejection carries one stable diagnostic instead of an identity- or wire-flavoured alias.

#include "packed_profile_internal.hpp"

#include <string>
#include <utility>
#include <variant>

namespace cuexis::chart::packed::profile_detail {
namespace {

auto fail(std::string code, std::string message) -> core::Error {
    return core::Error{std::move(code), std::move(message)};
}

} // namespace

auto validateFoundationProfile(const CanonicalSemanticChart& chart) -> core::Result<void> {
    bool hasRequirements = false;
    for (const auto& entity : chart.entities) {
        if (!entity.requirements.empty()) {
            hasRequirements = true;
            break;
        }
    }

    // Spec 7.6: the registry admits exactly one feature ID/version pair. A declared but
    // unregistered capability is rejected rather than ignored, and requirements require the
    // registered feature to be declared.
    bool registeredFeatureDeclared = false;
    for (const auto& feature : chart.features) {
        if (feature.id != registeredFeatureId) {
            return core::unexpected(
                fail("packed.profile.feature",
                     "Feature '" + feature.id + "' is not registered by the Foundation profile"));
        }
        if (feature.version != registeredFeatureVersion) {
            return core::unexpected(fail("packed.profile.feature",
                                         "Feature version " + std::to_string(feature.version) +
                                             " is not the registered Foundation version 1"));
        }
        registeredFeatureDeclared = true;
    }
    if (hasRequirements && !registeredFeatureDeclared) {
        return core::unexpected(
            fail("packed.profile.feature",
                 "Requirements require the registered Foundation feature declaration"));
    }

    for (const auto& entity : chart.entities) {
        for (const auto& requirement : entity.requirements) {
            if (requirement.kind != CanonicalRequirementKind::Tap) {
                return core::unexpected(
                    fail("packed.profile.requirement",
                         "Foundation revision 1 registers the tap requirement kind only"));
            }
            if (requirement.interval.kind != CanonicalIntervalKind::Point) {
                return core::unexpected(
                    fail("packed.profile.interval",
                         "Foundation revision 1 registers the point interval only"));
            }
            if (requirement.interval.endBeat) {
                return core::unexpected(fail("packed.profile.interval",
                                             "A point requirement must not carry an end beat"));
            }
            if (requirement.judgementDomain.id != registeredJudgementDomainId) {
                return core::unexpected(
                    fail("packed.profile.domain",
                         "Judgement domain '" + requirement.judgementDomain.id +
                             "' is not the registered candidate.lanes4 domain"));
            }
            if (requirement.requiredAction.id != registeredActionId) {
                return core::unexpected(
                    fail("packed.profile.action", "Action '" + requirement.requiredAction.id +
                                                      "' is not the registered press action"));
            }
            if (requirement.constraints.size() != 1U ||
                !std::holds_alternative<LaneConstraint>(requirement.constraints.front())) {
                return core::unexpected(
                    fail("packed.profile.constraints",
                         "Foundation requirements carry exactly one lane constraint"));
            }
            const auto lane = std::get<LaneConstraint>(requirement.constraints.front()).lane;
            if (lane > registeredMaxLane) {
                return core::unexpected(
                    fail("packed.profile.lane",
                         "Lane " + std::to_string(lane) +
                             " is outside the registered discrete lane range [0,3]"));
            }
            if (!requirement.effects.empty()) {
                return core::unexpected(fail("packed.profile.effects",
                                             "Foundation revision 1 requires empty effect sets"));
            }
        }
    }
    return {};
}

} // namespace cuexis::chart::packed::profile_detail
