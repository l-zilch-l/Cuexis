#include <cuexis/judgement/compile/runtime_identity_inputs.hpp>

#include <cuexis/judgement/compile/legacy_prepared_view.hpp>

#include <cuexis/judgement/gameplay_assembler_identity.hpp>
#include <cuexis/judgement/identity_bytes.hpp>

#include <algorithm>
#include <string>
#include <utility>

namespace cuexis::judgement::compile {
namespace {

auto projectionError(std::string message) -> core::Error {
    return core::Error{"judgement.identity.chart_projection_missing", std::move(message)}
        .withContext("category", "identity_closure_incomplete")
        .withContext("severity", "error")
        .withContext("faulted", "false");
}

} // namespace

auto chartProjectionBytes(const PreparedGameplay& prepared)
    -> core::Result<std::vector<std::byte>> {
    IdentityByteWriter prefix{"cuexis.judgement.identity.chart.v1"};
    prefix.beginComponent("chart");
    const auto header = prefix.take();
    const auto& bytes = prepared.assembled().chart.canonicalBytes().bytes();
    if (bytes.size() < header.size() || !std::equal(header.begin(), header.end(), bytes.begin())) {
        return core::unexpected(
            projectionError("the prepared chart identity does not start with the chart header"));
    }
    return std::vector<std::byte>{bytes.begin() + static_cast<std::ptrdiff_t>(header.size()),
                                  bytes.end()};
}

auto executionSessionBytes(const InputMappingProfile& mapping, const LatePolicyParameters& late,
                           std::string_view calibration) -> std::vector<std::byte> {
    IdentityByteWriter writer;
    writer.writeText("cuexis.judgement.identity.session.execution.v1");
    writer.writeText(mapping.profileId);
    writer.writeText(mapping.profileVersion);
    writer.writeText(mapping.sourceClass.token());
    auto domains = mapping.domains;
    std::sort(domains.begin(), domains.end(),
              [](const auto& a, const auto& b) { return a.domainToken < b.domainToken; });
    writer.writeCount(domains.size());
    for (const auto& domain : domains) {
        writer.writeText(domain.domainToken);
        writer.writeSigned(domain.amount.scale.numerator());
        writer.writeSigned(domain.amount.scale.denominator());
        writer.writeSigned(domain.amount.minimum);
        writer.writeSigned(domain.amount.maximum);
        writer.writeText(domain.amount.boundaryPolicy == AmountBoundaryPolicy::inclusive
                             ? "inclusive"
                             : "exclusive");
    }
    for (const auto& parameter : {late.finalizationWatermark, late.maxQueueHop,
                                  late.windowCloseThreshold, late.windowOpenThreshold}) {
        writer.writeBool(parameter.isMeasured());
        if (parameter.isMeasured()) {
            writer.writeSigned(parameter.measuredValue()->value());
        }
    }
    writer.writeBool(late.policy.has_value());
    if (late.policy) {
        writer.writeText(*late.policy == LateEventPolicy::rejectLate ? "reject_late"
                                                                     : "queue_next_tick");
    }
    writer.writeText(calibration);
    return writer.take();
}

auto runtimePreparedIdentity(const PreparedGameplay& prepared,
                             const PreparedIdentityDeclarations& declarations,
                             const InputMappingProfile& mapping, const LatePolicyParameters& late,
                             std::string_view calibration)
    -> core::Result<runtime::PreparedIdentityBytes> {
    auto view = preparedGameplayView(prepared);
    if (!view) {
        return core::unexpected(view.error());
    }
    return runtime::composePreparedIdentity(*view, declarations,
                                            executionSessionBytes(mapping, late, calibration));
}

} // namespace cuexis::judgement::compile
