#include "gameplay_capsule_operations_internal.hpp"

// Section decoding, graph linking, reconstruction and canonical re-hash.

namespace cuexis::gameplay_packed {
namespace capsule_detail {

struct Payload final {
    std::span<const std::byte> bytes;
    std::uint32_t rows;
};
auto payloads(std::span<const std::byte> bytes) -> std::map<std::string, Payload> {
    ByteReader header{bytes.subspan(24, 4)};
    const auto n = checked(header.readU32());
    std::map<std::string, Payload> result;
    for (std::uint32_t i = 0; i < n; ++i) {
        ByteReader row{bytes.subspan(96 + static_cast<std::size_t>(i) * 32, 32)};
        const auto code = checked(row.readBytes(4));
        checked(row.readBytes(4));
        const auto offset = checked(row.readU32()), encoded = checked(row.readU32());
        checked(row.readU32());
        const auto records = checked(row.readU32());
        result.emplace(std::string{reinterpret_cast<const char*>(code.data()), 4},
                       Payload{bytes.subspan(offset, encoded), records});
    }
    return result;
}
template <typename F>
void decodeSection(const Payload& p, std::string path, Model& m, const gp::DecodedStatic& base,
                   F decodeRows) {
    ByteReader framing{p.bytes};
    require(checked(framing.readU16()) == 1 && checked(framing.readU8()) == 1 &&
                checked(framing.readU8()) == 0,
            path, "unknown Gameplay table framing");
    require(checked(framing.readU32()) == p.rows, path, "table count disagrees with directory");
    require(checked(framing.readU64()) == framing.remaining(), path, "payload length mismatch");
    Input in{ByteReader{p.bytes.subspan(16)}, base.strings, base.references, std::move(path)};
    decodeRows(in, m, p.rows);
    require(in.reader.empty(), in.path, "trailing Gameplay bytes");
}
void link(Model& m, const std::vector<std::uint64_t>& masks) {
    m.bind();
    auto& g = m.graph;
    std::vector<bool> presence(m.entities.size());
    for (std::size_t i = 0; i < g.requirements.size(); ++i) {
        auto& r = g.requirements[i];
        const auto& ix = m.indices[i];
        require(ix.entity < m.entities.size() && ix.pattern < m.patterns.size() &&
                    ix.measure < m.measures.size() && ix.domain < g.judgementDomains.size() &&
                    ix.solver < g.solverProfiles.size(),
                "GPR0.indices", "dangling table index");
        presence[ix.entity] = true;
        if (std::find(m.owners.begin(), m.owners.end(), m.entities[ix.entity]) == m.owners.end()) {
            m.owners.push_back(m.entities[ix.entity]);
        }
        r.pattern = m.patterns[ix.pattern];
        r.measure = m.measures[ix.measure].declaration;
        r.judgementDomainId = g.judgementDomains[ix.domain].domainId;
        r.solverProfileRef = g.solverProfiles[ix.solver].solverId;
        for (auto c : ix.claims) {
            require(c < m.claims.size() && m.claims[c].requirement == i, "GPR0.claims",
                    "claim owner mismatch");
            r.resourceClaims.push_back(m.claims[c].declaration);
        }
        for (auto b : ix.bindings) {
            require(b < g.factBindings.size(), "GPR0.factBindings", "dangling binding");
            r.factBindingRefs.push_back(g.factBindings[b].bindingId);
        }
    }
    require(masks.size() == presence.size(), "ENT0", "entity mask count mismatch");
    for (std::size_t i = 0; i < masks.size(); ++i) {
        require(((masks[i] & 4U) != 0) == presence[i], "ENT0.bit2",
                "Requirement presence disagrees with ownership");
    }
}
auto reconstruct(Model& m, const DecodeContext& context,
                 detail::HashObservation* observation = nullptr) -> PreparedGameplay {
    if (observation)
        ++observation->reconstructions;
    m.bind();
    auto& g = m.graph;
    require(m.profiles.normalizationProfileToken ==
                    context.identities.session.normalizationProfileToken &&
                m.profiles.coordinatorPolicyToken == context.coordinatorPolicyToken,
            "GPH0.profiles", "execution context does not match compiled profiles");
    std::vector<GameplaySource> sources;
    std::map<std::string, std::size_t> sourceIndex;
    for (const auto& id : g.sourceClosure.sourceDocumentIds) {
        require(!id.empty(), "sourceClosure", "empty source document ID");
        sourceIndex.emplace(id, sources.size());
        sources.push_back({SourceForm::memory, "packed.capsule.v2",
                           GameplaySourceDocument{id, 5, 2, {}, {}, {}, {}, {}, {}, {}}});
    }
    const auto sourceFor = [&](const std::string& id) -> GameplaySourceDocument& {
        const auto it = sourceIndex.find(id);
        require(it != sourceIndex.end(), "sourceClosure",
                "declaration source is not in source closure");
        return sources[it->second].document;
    };
    for (const auto& d : g.mergedNamespace.declarations) {
        auto& doc = sourceFor(d.stableId.sourceDocumentId);
        LocalDeclaration declaration{d.stableId, d.kind, d.name, {}, {}};
        for (const auto& r : d.references) {
            declaration.references.push_back(
                {ReferenceScope::explicitCrossDocument, r.sourceDocumentId, r.declarationOrdinal});
        }
        for (const auto& f : d.requiredFeatureIds) {
            declaration.required.features.push_back({f});
        }
        for (const auto& c : d.requiredCapabilityIds) {
            declaration.required.capabilities.push_back({c, ""});
        }
        doc.declarations.push_back(std::move(declaration));
    }
    for (const auto& r : g.requirements) {
        sourceFor(r.stableId.sourceDocumentId).requirements.push_back(r);
    }
    require(!sources.empty(), "sourceClosure", "prepare requires an explicit source closure");
    // Typed records without a separate source field are located by their merged declarations.
    const auto definitionSource = [&](DeclarationKind kind,
                                      std::string_view name) -> GameplaySourceDocument& {
        const auto it = std::find_if(
            g.mergedNamespace.declarations.begin(), g.mergedNamespace.declarations.end(),
            [kind, name](const auto& d) { return d.kind == kind && d.name == name; });
        require(it != g.mergedNamespace.declarations.end(), "mergedDeclarations",
                "definition source is missing");
        return sourceFor(it->stableId.sourceDocumentId);
    };
    for (const auto& r : g.resources) {
        definitionSource(DeclarationKind::resourceRecord, r.ref.resourceId).resources.push_back(r);
    }
    for (const auto& s : g.solverProfiles) {
        definitionSource(DeclarationKind::solverProfile, s.solverId).solverProfiles.push_back(s);
    }
    for (const auto& d : g.judgementDomains) {
        definitionSource(DeclarationKind::judgementDomain, d.domainId)
            .judgementDomains.push_back(d);
    }
    sources.front().document.relations = g.relations;
    sources.front().document.factBindings = g.factBindings;
    AssemblyRequest request{EntryKind::packedChart,
                            true,
                            std::move(sources),
                            g.graphRevision,
                            g.rulesetRef,
                            g.declaredCapabilities,
                            g.declaredFeatures,
                            g.closureContributions,
                            g.sourceClosure.compilerProfileToken,
                            &m.timebase,
                            &m.latePolicy,
                            context.capabilities,
                            context.contentLimits,
                            context.identities,
                            {},
                            {},
                            {}};
    request.executionProfile = g.executionProfile;
    request.normalizationProfileToken = g.normalizationProfileToken;
    request.coordinatorPolicyToken = g.coordinatorPolicyToken;
    if (observation)
        ++observation->sharedPrepareCalls;
#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
    auto prepared = [&] {
        internal::pd::Timer measurement{internal::pd::Stage::packedSharedPrepare};
        return checked(prepareResolvedGameplay({std::move(request), context.patternBudget}));
    }();
#else
    auto prepared = checked(prepareResolvedGameplay({std::move(request), context.patternBudget}));
#endif
#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
    const auto differences = [&] {
        internal::pd::Timer measurement{internal::pd::Stage::packedSemanticCompare};
        return semanticDiff(g, prepared.assembled().graph);
    }();
#else
    const auto differences = semanticDiff(g, prepared.assembled().graph);
#endif
    require(differences.empty(), differences.empty() ? "gameplay" : differences.front().path,
            "shared prepare changed a decoded semantic field");
    require(g.sourceClosure == prepared.assembled().graph.sourceClosure, "sourceClosure",
            "source closure changed during prepare");
    return prepared;
}
auto decoded(std::span<const std::byte> bytes, const DecodeContext& context,
             chart::PackedChartLimits limits, detail::HashOptions hashOptions,
             detail::HashObservation* observation) -> PreparedCapsule {
#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
    internal::pd::Timer decodeMeasurement{internal::pd::Stage::packedDecode};
#endif
    require(bytes.size() >= 96, "Header", "truncated Capsule header");
    ByteReader revisionReader{bytes.subspan(88, 4)};
    const auto revision = checked(revisionReader.readU32());
    if (revision != context.candidateRevision || (revision != 2 && !executionRevision(revision))) {
        fail("packed.header.unsupported_revision", "Capsule Reader revision mismatch", "Header");
    }
    const auto statistics = checked(gp::inspect(bytes, limits));
    auto base = checked(gp::decodeStatic(bytes, limits));
    const auto sections = payloads(bytes);
    Model m;
    m.candidateRevision = revision;
    m.chart = std::move(base.chart);
    for (const auto& e : m.chart.entities) {
        m.entities.push_back(e.identity);
    }
    decodeSection(sections.at("GPH0"), "GPH0", m, base, [](auto& in, auto& model, auto n) {
        require(n == 1, "GPH0", "global row count must be one");
        globalRow(in, model);
    });
    decodeSection(sections.at("GPD0"), "GPD0", m, base,
                  [](auto& in, auto& model, auto) { definitions(in, model); });
    decodeSection(sections.at("GPR0"), "GPR0", m, base, [limits](auto& in, auto& model, auto n) {
        bound(n, limits.maxPackedRequirements, "packed.budget.requirements");
        require(n <= in.reader.remaining(), "GPR0", "row count exceeds available bytes");
        model.graph.requirements.reserve(n);
        model.indices.reserve(n);
        for (std::uint32_t i = 0; i < n; ++i) {
            RequirementRecord r{};
            RequirementIndices ix;
            requirementRow(in, r, ix, model);
            model.graph.requirements.push_back(std::move(r));
            model.indices.push_back(std::move(ix));
        }
    });
    decodeSection(sections.at("GRC0"), "GRC0", m, base,
                  [](auto& in, auto& model, auto) { coordination(in, model); });
    const auto declaredCounts = m.counts;
    counts(m, base.references.size());
    require(m.counts == declaredCounts && m.counts[0] == statistics.requirementCount &&
                sections.at("GPR0").rows == m.counts[0] &&
                sections.at("GPD0").rows == sum(std::span{m.counts}.subspan(1, 4)) &&
                sections.at("GRC0").rows == sum(std::span{m.counts}.subspan(5, 5)),
            "GPH0.counts", "Header, directory and actual table counts disagree");
#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
    {
        internal::pd::Timer validationMeasurement{internal::pd::Stage::packedLinkValidate};
#endif
        link(m, base.entityMasks);
        validate(m, limits, context.patternBudget);
#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
    }
#endif
    // Reconstruct dictionaries from typed fields, not from a canonical whole-file comparison.
    // This keeps section permutation and validated inspection independent of judgement.
#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
    std::optional<internal::pd::Timer> identityMeasurement;
    identityMeasurement.emplace(internal::pd::Stage::packedIdentity);
#endif
    const auto canonicalStatic = staticTables(m, limits);
    require(canonicalStatic.references == base.references, "REF0",
            "reference closure differs from typed records");
    const auto digest = m.candidateRevision == 4 ? hashModel(m, hashOptions, observation).digest
                                                 : core::detail::sha256(preimage(m, limits));
    if (!std::equal(digest.begin(), digest.end(), bytes.begin() + 32, [](auto left, auto right) {
            return left == std::to_integer<std::uint8_t>(right);
        }))
        fail("packed.identity.mismatch", "structural semantic identity mismatch",
             "Header.semanticIdentity");
    if (revision == 4 && context.expectedCompiledSemanticIdentity) {
        // Compare the verified digest without exporting a preimage or allocating hex text.
        const auto expected = *context.expectedCompiledSemanticIdentity;
        constexpr std::string_view hex = "0123456789abcdef";
        bool matches = expected.size() == digest.size() * 2;
        for (std::size_t i = 0; matches && i < digest.size(); ++i)
            matches = expected[2 * i] == hex[digest[i] >> 4] &&
                      expected[2 * i + 1] == hex[digest[i] & 15];
        if (!matches)
            fail("packed.metadata.mismatch", "Gameplay metadata semantic identity mismatch",
                 "metadata.compiledSemanticIdentity");
    }
#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
    identityMeasurement.reset();
#endif
#if defined(CUEXIS_PD_METRICS_TEST_ONLY)
    auto prepared = [&] {
        internal::pd::Timer reconstructMeasurement{internal::pd::Stage::packedReconstruct};
        return reconstruct(m, context, observation);
    }();
#else
    auto prepared = reconstruct(m, context, observation);
#endif
    std::vector<RequirementOwner> owners;
    for (std::size_t i = 0; i < m.graph.requirements.size(); ++i) {
        owners.push_back({m.graph.requirements[i].identity, m.entities[m.indices[i].entity]});
    }
    return {std::move(m.chart),    std::move(prepared),   std::move(owners), std::move(m.profiles),
            std::move(m.patterns), std::move(m.measures), revision,          digest};
}

#include "gameplay_graph_reader_internal.hpp"

} // namespace capsule_detail

using namespace capsule_detail;

auto decodeGraph(std::string_view text, const DecodeContext& context, GraphReaderLimits graphLimits,
                 chart::PackedChartLimits limits) -> core::Result<PreparedCapsule> {
    return boundary<PreparedCapsule>([&] {
        return graphDecoded(text, context, graphLimits,
                            chart::packed::limits_detail::effectiveLimits(limits));
    });
}

} // namespace cuexis::gameplay_packed
