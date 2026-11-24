#include <cuexis/judgement/prepared_identity_bytes.hpp>

#include <cuexis/judgement/identity_bytes.hpp>

#include <utility>

namespace cuexis::judgement::runtime {
namespace {

auto error(std::string message) -> core::Error {
    return core::Error{"judgement.identity.chart_projection_missing", std::move(message)}
        .withContext("category", "identity_closure_incomplete")
        .withContext("severity", "error")
        .withContext("faulted", "false");
}

void writePreparedPrefix(IdentityByteWriter& writer,
                         const PreparedIdentityDeclarations& declarations) {
    writer.beginComponent("engine");
    writer.writeText(declarations.engine.judgementSemanticRevision);
    writer.writeText(declarations.engine.factSemanticRevision);
    writer.writeText(declarations.engine.fixedPointTableId);
    writer.writeText(declarations.engine.coordinationPhaseOrderToken);
    if (declarations.engine.executionProfileToken || declarations.engine.lateAlgorithmToken) {
        writer.writeText("execution.engine.v1");
        writer.writeBool(declarations.engine.executionProfileToken.has_value());
        if (declarations.engine.executionProfileToken) {
            writer.writeText(*declarations.engine.executionProfileToken);
        }
        writer.writeBool(declarations.engine.lateAlgorithmToken.has_value());
        if (declarations.engine.lateAlgorithmToken) {
            writer.writeText(*declarations.engine.lateAlgorithmToken);
        }
    }
    //  The snapshot state schema revision has no field here on purpose: a normalized snapshot
    //  detail must not change the semantic judgement identity.
    writer.beginComponent("ruleset");
    writer.writeText(declarations.ruleset.interfaceProjectionToken);
    writer.writeCount(declarations.ruleset.moduleOrder.size());
    for (const auto& module : declarations.ruleset.moduleOrder) {
        writer.writeText(module);
    }
    writer.writeText(declarations.ruleset.buildHash);
    writer.beginComponent("chart");
}

void writePreparedSession(IdentityByteWriter& writer,
                          const PreparedIdentityDeclarations& declarations) {
    writer.beginComponent("session");
    writer.writeText(declarations.session.loadoutToken);
    writer.writeText(declarations.session.defaultGraceSourceToken);
    writer.writeText(declarations.session.normalizationProfileToken);
    writer.writeText(declarations.session.judgementConfigToken);
}

} // namespace

auto PreparedIdentityBytes::toHex() const -> std::string {
    static constexpr char digits[] = "0123456789abcdef";
    std::string rendered;
    rendered.reserve(bytes_.size() * 2);
    for (const auto byte : bytes_) {
        const auto value = static_cast<unsigned>(byte);
        rendered.push_back(digits[(value >> 4U) & 0x0FU]);
        rendered.push_back(digits[value & 0x0FU]);
    }
    return rendered;
}

auto composePreparedIdentity(const PreparedGameplay& prepared,
                             const PreparedIdentityDeclarations& declarations,
                             std::span<const std::byte> executionSession)
    -> core::Result<PreparedIdentityBytes> {
    const auto projection = prepared.chartProjection();
    if (projection.empty()) {
        return core::unexpected(error("the prepared gameplay carries no chart projection bytes"));
    }
    IdentityByteWriter writer{"cuexis.judgement.identity.prepared.v1"};
    writePreparedPrefix(writer, declarations);
    writer.append(projection);
    writePreparedSession(writer, declarations);
    writer.append(executionSession);
    return PreparedIdentityBytes{writer.take()};
}

} // namespace cuexis::judgement::runtime
