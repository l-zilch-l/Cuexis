#include <cuexis/chart/candidate_assembler.hpp>

#include <cuexis/chart/packed_chart_tables.hpp>

#include <exception>
#include <set>
#include <utility>

namespace cuexis::chart {

auto assembleCandidateChart(CanonicalSemanticChart chart,
                            std::span<const CanonicalSemanticChart> expanded,
                            PackedChartLimits limits) -> core::Result<CanonicalSemanticChart> try {
    // The assembler owns derived output. A caller cannot smuggle extra registered features into
    // a static artifact. Existing declarations may only assert what actual content needs.
    const auto declaredFeatures = chart.features;
    const auto declaredResources = chart.resourceClosure.resources;
    // Nonempty input declarations describe the base only, not the still-unassembled fragments.
    // Check them against the base's actual content before deriving the combined output below.
    if (!declaredFeatures.empty() || !declaredResources.empty()) {
        auto base = packed::encode(chart, {}, limits);
        if (!base) {
            return core::unexpected(std::move(base.error()));
        }
    }
    for (const auto& fragment : expanded) {
        if (fragment.chartId != chart.chartId || fragment.mainMusic || !fragment.features.empty() ||
            !fragment.resourceClosure.resources.empty()) {
            return core::unexpected(core::Error{"candidate.assembly.fragment",
                                                "Expanded fragment has foreign metadata"});
        }
        if (fragment.entities.size() > limits.maxPackedEntities ||
            chart.entities.size() > limits.maxPackedEntities - fragment.entities.size()) {
            return core::unexpected(core::Error{"candidate.assembly.budget",
                                                "Expanded entities exceed the candidate budget"});
        }
        chart.entities.insert(chart.entities.end(), fragment.entities.begin(),
                              fragment.entities.end());
    }
    bool requirements = false;
    std::set<CanonicalResourceUse> resources;
    if (chart.mainMusic) {
        resources.insert({*chart.mainMusic, CanonicalResourceUseKind::MainMusic});
    }
    for (const auto& entity : chart.entities) {
        requirements = requirements || !entity.requirements.empty();
        for (const auto& component : entity.components) {
            if (const auto* renderable = std::get_if<CanonicalRenderable>(&component)) {
                resources.insert({renderable->mesh, CanonicalResourceUseKind::RenderableMesh});
                resources.insert(
                    {renderable->material, CanonicalResourceUseKind::RenderableMaterial});
            }
        }
    }
    chart.features.clear();
    if (requirements) {
        chart.features.push_back({"cuexis.gameplay.candidate.lanes4", 1});
    }
    chart.resourceClosure.resources.assign(resources.begin(), resources.end());
    auto encoded = packed::encode(chart, {}, limits);
    if (!encoded) {
        return core::unexpected(std::move(encoded.error()));
    }
    // Return the canonical Reader result: identity uniqueness, parent graph, registered profile,
    // resource uses and all wire ranges have now been checked by actual production codecs.
    return packed::decode(*encoded, limits);
} catch (const std::exception& error) {
    return core::unexpected(core::Error{"candidate.assembly.failed", error.what()});
}

} // namespace cuexis::chart
