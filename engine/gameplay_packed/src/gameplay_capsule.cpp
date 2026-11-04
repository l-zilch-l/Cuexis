#include "gameplay_capsule_operations_internal.hpp"

#include <cmath>

// Public capsule entry points. Every fallible call crosses one boundary.

namespace cuexis::gameplay_packed {

using namespace capsule_detail;

namespace capsule_detail {
#include "gameplay_graph_writer_internal.hpp"
} // namespace capsule_detail

auto detail::encodeBounded(const EncodeRequest& request, chart::PackedChartLimits limits,
                           HashOptions options, HashObservation* observation)
    -> core::Result<Bytes> {
    if (observation)
        *observation = {};
    return boundary<Bytes>([&] {
        limits = chart::packed::limits_detail::effectiveLimits(limits);
        auto m = makeModel(request, limits);
        m.bind();
        auto tables = staticTables(m, limits);
        return envelope(m, std::move(tables), limits, options, observation);
    });
}

auto encode(const EncodeRequest& request, chart::PackedChartLimits limits) -> core::Result<Bytes> {
    return detail::encodeBounded(request, limits);
}

auto encodeGraph(const EncodeRequest& request, GraphWriterLimits graphLimits,
                 chart::PackedChartLimits limits) -> core::Result<std::string> {
    return boundary<std::string>([&] {
        if (!graphLimits.testOnly || !graphLimits.maxBytes || !graphLimits.maxStringBytes ||
            !graphLimits.maxRowAtoms)
            fail("capability.budget_insufficient",
                 "Explicit test-only Graph Writer bounds required");
        if (request.candidateRevision != 3)
            fail("graph.header.unsupported_revision", "Graph v1 requires Capsule revision 3");
        limits = chart::packed::limits_detail::effectiveLimits(limits);
        auto m = makeModel(request, limits);
        m.bind();
        const auto tables = staticTables(m, limits);
        (void)tables;
        const auto identity = core::detail::sha256Hex(preimage(m, limits));
        GraphText out{graphLimits};
        const auto row = [&](auto visitRow) {
            out.array([&] {
                GraphOutput visitor{Mode::wire, out};
                visitRow(visitor);
            });
        };
        const auto rows = [&](auto& values, auto visitRow) {
            out.array([&] {
                for (auto& value : values)
                    row([&](auto& visitor) { visitRow(visitor, value); });
            });
        };
        out.object([&] {
            out.key("format");
            out.string("cuexis.gameplay-graph");
            out.key("graphFormatRevision");
            out.integer(1);
            out.key("capsuleRevision");
            out.integer(3);
            out.key("semanticIdentity");
            out.string(identity);
            out.key("staticChart");
            graphStaticChart(out, m.chart);
            out.key("GPH0");
            row([&](auto& visitor) { globalRow(visitor, m); });
            out.key("GPR0");
            out.array([&] {
                for (std::size_t i = 0; i < m.graph.requirements.size(); ++i)
                    row([&](auto& visitor) {
                        requirementRow(visitor, m.graph.requirements[i], m.indices[i], m);
                    });
            });
            out.key("GPD0");
            out.object([&] {
                out.key("patterns");
                rows(m.patterns, [](auto& a, auto& v) { patternRow(a, v); });
                out.key("measures");
                rows(m.measures, [](auto& a, auto& v) { measureRow(a, v); });
                out.key("judgementDomains");
                rows(m.graph.judgementDomains, [](auto& a, auto& v) { domainRow(a, v); });
                out.key("mergedDeclarations");
                rows(m.graph.mergedNamespace.declarations,
                     [](auto& a, auto& v) { mergedRow(a, v); });
            });
            out.key("GRC0");
            out.object([&] {
                out.key("resources");
                rows(m.graph.resources, [](auto& a, auto& v) { resourceRow(a, v); });
                out.key("relations");
                rows(m.graph.relations, [&](auto& a, auto& v) { relationRow(a, v, m); });
                out.key("solverProfiles");
                rows(m.graph.solverProfiles, [](auto& a, auto& v) { solverRow(a, v); });
                out.key("factBindings");
                rows(m.graph.factBindings, [](auto& a, auto& v) { bindingRow(a, v); });
                out.key("claims");
                rows(m.claims, [&](auto& a, auto& v) { claimRow(a, v, m); });
            });
        });
        return out.take();
    });
}

auto detail::calculateSemanticHash(const SemanticInput& input, HashOptions options,
                                   HashObservation* observation)
    -> core::Result<SemanticHashResult> {
    HashObservation local;
    auto& observed = observation ? *observation : local;
    observed = {};
    try {
        require(input.candidateRevision == 2 || input.candidateRevision == 3, "semanticHash.codec",
                "Unknown semantic codec selector");
        require(input.indices.size() == input.graph.requirements.size(), "semanticHash.indices",
                "Requirement index count disagrees");
        require(input.relationResources.empty() ||
                    input.relationResources.size() == input.graph.relations.size(),
                "semanticHash.relations", "Relation index count disagrees");
        HashCoreWork work{input, options, observed};
        for (std::size_t i = 0; i < input.graph.requirements.size(); ++i) {
            HashCoreWork::EmissionScope scope{work, i};
            Output size;
            size.mode = Mode::hash;
            size.operation = &work;
            size.sizing = true;
            StructuralOutputAdapter adapter{size};
            judgement::detail::StructuralKeyEncoder encoder{adapter};
            judgement::detail::emissionIdentityFields(encoder,
                                                      input.graph.requirements[i].identity);
            auto& slot = work.group(CostGroup::emission)[i];
            slot.cost.bytes = size.size;
            slot.flags = 1;
            ++observed.uniqueRows;
        }
        for (std::size_t i = 0; i < input.entities.size(); ++i) {
            auto& slot = work.group(CostGroup::entity)[i];
            slot.cost.bytes =
                4 +
                checked(chart::packed::identity_detail::canonicalIdentitySize(input.entities[i]));
            slot.flags = 1;
            ++observed.uniqueRows;
        }
        const auto emit = [&](Output& out) {
            gp::SemanticByteSink prefix{
                &out, [](void* context, auto bytes) { static_cast<Output*>(context)->raw(bytes); },
                &work.scratch};
            checked(gp::streamStaticPreimage(input.chart, input.owners, prefix,
                                             input.candidateRevision));
            semanticRows(out, input);
        };
        HashCost counted;
        Output count;
        count.mode = Mode::hash;
        count.operation = &work;
        count.cost = &counted;
        emit(count);
        checkHashCost(counted, work.limits);
        observed.budgetAccepted = true;
        work.counting = false;
        work.scratch.setStage("hash", "semanticHash.total");
        work.sha.emplace();
        ++observed.shaComputations;
        gp::SemanticByteSink tee{
            &work,
            [](void* context, auto bytes) { static_cast<HashCoreWork*>(context)->write(bytes); },
            &work.scratch};
        HashCost actual;
        Output hash;
        hash.mode = Mode::hash;
        hash.operation = &work;
        hash.cost = &actual;
        hash.sink = &tee;
        emit(hash);
        require(actual.bytes == counted.bytes && actual.visits == counted.visits &&
                    actual.maxKey == counted.maxKey && work.emitted == counted.bytes,
                "semanticHash.conformance", "Count and emitted semantic bytes disagree");
        return SemanticHashResult{counted, work.sha->finish(), observed.exportedBytes,
                                  work.exportBuffer.take()};
    } catch (HashWorkFailure& failure) {
        return core::unexpected(std::move(failure.error));
    } catch (Failure& failure) {
        return core::unexpected(std::move(failure.error));
    }
}

auto detail::hashRequest(const EncodeRequest& request, chart::PackedChartLimits limits,
                         HashOptions options, HashObservation* observation)
    -> core::Result<SemanticHashResult> {
    if (observation)
        *observation = {};
    return boundary<SemanticHashResult>([&] {
        require(request.candidateRevision == 4, "candidateRevision",
                "Bounded request hash requires revision 4");
        limits = chart::packed::limits_detail::effectiveLimits(limits);
        auto m = makeModel(request, limits);
        m.bind();
        const auto tables = physicalTables(m, staticTables(m, limits), limits);
        (void)tables;
        return hashModel(m, options, observation);
    });
}
auto semanticPreimage(const EncodeRequest& request, chart::PackedChartLimits limits)
    -> core::Result<Bytes> {
    return boundary<Bytes>([&] {
        require(request.candidateRevision != 4, "semanticHash.export",
                "Revision 4 byte export requires the bounded hashRequest API");
        limits = chart::packed::limits_detail::effectiveLimits(limits);
        auto m = makeModel(request, limits);
        m.bind();
        const auto tables = staticTables(m, limits);
        (void)tables;
        return preimage(m, limits);
    });
}
auto detail::decodeBounded(std::span<const std::byte> bytes, const DecodeContext& context,
                           chart::PackedChartLimits limits, HashOptions options,
                           HashObservation* observation) -> core::Result<PreparedCapsule> {
    if (observation)
        *observation = {};
    return boundary<PreparedCapsule>([&] {
        return decoded(bytes, context, chart::packed::limits_detail::effectiveLimits(limits),
                       options, observation);
    });
}

auto decode(std::span<const std::byte> bytes, const DecodeContext& context,
            chart::PackedChartLimits limits) -> core::Result<PreparedCapsule> {
    return detail::decodeBounded(bytes, context, limits);
}

auto read(const std::filesystem::path& source, const DecodeContext& context,
          chart::PackedChartLimits limits) -> core::Result<PreparedCapsule> {
    return boundary<PreparedCapsule>([&] {
        limits = chart::packed::limits_detail::effectiveLimits(limits);
        const auto path = std::filesystem::absolute(source);
        const auto file = checked(
            filesystem::readBoundedFile(path, {path.parent_path(), limits.maxPackedFileBytes, {}}));
        return decoded(file.bytes, context, limits);
    });
}
auto writeAtomic(const EncodeRequest& request, const std::filesystem::path& target,
                 chart::PackedChartLimits limits) -> core::Result<void> {
    return boundary<void>([&] {
        const auto bytes = checked(encode(request, limits));
        checked(chart::packed::file_detail::writeAtomic(bytes, target));
    });
}
auto decodeInto(std::optional<PreparedCapsule>& active, std::span<const std::byte> bytes,
                const DecodeContext& context, chart::PackedChartLimits limits)
    -> core::Result<void> {
    auto candidate = decode(bytes, context, limits);
    if (!candidate) {
        return core::unexpected(std::move(candidate.error()));
    }
    active = std::move(*candidate);
    return {};
}

} // namespace cuexis::gameplay_packed
