#include <cuexis/tools/gameplay_author.hpp>

#include <algorithm>
#include <fstream>
#include <map>
#include <set>

namespace cuexis::tools::gameplay_author {
namespace {
namespace dto = json::gameplay;
using namespace judgement;
struct Reject {
    core::Error error;
};
auto invalid(std::string_view field, std::string_view message) -> core::Error {
    return core::Error{"judgement.s7a4.execution.relation_invalid", std::string{message}}
        .withContext("category", "invalid_relation")
        .withContext("severity", "error")
        .withContext("faulted", "false")
        .withContext("field.path", std::string{field});
}
void require(bool condition, std::string_view field, std::string_view message) {
    if (!condition) {
        throw Reject{invalid(field, message)};
    }
}
template <class T> auto checked(core::Result<T> value) -> T {
    if (!value) {
        throw Reject{value.error()};
    }
    return std::move(*value);
}
auto stable(const dto::StableId& d) -> StableDeclarationId {
    return {d.sourceDocumentId, d.declarationOrdinal};
}
auto identity(const dto::Identity& d) -> RequirementIdentity {
    RequirementIdentity result{d.chartEntryId,      d.invocationId, d.moduleId, d.exportId, {},
                               d.requirementLocalId};
    for (const auto& step : d.emissionPath) {
        result.emissionPath.push_back({step.nodeId, step.repeatIndex});
    }
    return result;
}
auto refs(const dto::Refs& d) -> RequiredRefs {
    RequiredRefs result;
    for (const auto& f : d.features) {
        result.features.push_back({f});
    }
    for (const auto& c : d.capabilities) {
        result.capabilities.push_back({c.capabilityId, c.revision.value_or("")});
    }
    return result;
}
auto phase(std::string_view value) -> PhaseKind {
    if (value == "tap") {
        return PhaseKind::tap;
    }
    if (value == "head") {
        return PhaseKind::head;
    }
    if (value == "body") {
        return PhaseKind::body;
    }
    require(value == "tail", "phase", "unknown phase");
    return PhaseKind::tail;
}
auto duration(dto::Q value) -> RationalDuration {
    return checked(RationalDuration::create(value.numerator, value.denominator));
}
auto beat(dto::Q value) -> RationalBeat {
    return checked(RationalBeat::create(value.numerator, value.denominator));
}
auto optionalDuration(std::optional<dto::Q> value) -> std::optional<RationalDuration> {
    return value ? std::optional{duration(*value)} : std::nullopt;
}
auto node(const dto::PatternNode& d) -> PatternNodeDeclaration {
    const std::map<std::string, PatternPrimitive> primitives{
        {"atom", PatternPrimitive::atom},
        {"sequence", PatternPrimitive::sequence},
        {"choice", PatternPrimitive::choice},
        {"boundedRepeat", PatternPrimitive::boundedRepeat},
        {"skip", PatternPrimitive::skip},
        {"instant", PatternPrimitive::instant},
        {"complement", PatternPrimitive::complement}};
    const auto found = primitives.find(d.primitive);
    require(found != primitives.end(), "pattern.primitive", "unknown Pattern opcode");
    PatternNodeDeclaration result{found->second, {}, d.atomRef.value_or(""), {}, {}};
    if (d.repeatBounds) {
        result.repeatBounds =
            RepeatBoundsDeclaration{d.repeatBounds->first, d.repeatBounds->second};
    }
    for (const auto& child : d.operands) {
        result.operands.push_back(node(child));
    }
    return result;
}
auto pattern(const dto::Pattern& d) -> PatternDeclaration {
    require(d.matchPolicy == "leftmost-first", "pattern.matchPolicy",
            "unsupported matching policy");
    return {d.patternId.value_or(""), MatchPolicy::leftmostFirst, node(d.root),
            refs(d.requiredRefs)};
}
auto measure(const dto::Measure& d) -> MeasureSpecDeclaration {
    MeasureSpecDeclaration result{{}, refs(d.requiredRefs)};
    for (const auto& c : d.components) {
        result.components.push_back({phase(c.phase), c.categoryToken, c.gradeTokens, {}});
    }
    return result;
}
auto grace(const dto::Grace& d) -> GraceDeclaration {
    const auto policy = d.policy == "explicit"    ? GraceResolutionPolicy::explicitDeclaration
                        : d.policy == "inherited" ? GraceResolutionPolicy::inheritedDeclaration
                                                  : GraceResolutionPolicy::defaultDeclaration;
    return {policy, d.inheritedFromDeclarationId.value_or(""), d.allowChartGrace};
}
auto graceInputs(const dto::GraceInputs& d) -> GraceResolutionInputs {
    return {duration(d.unitInTicks),
            d.minimumCanonical,
            d.maximumCanonical,
            optionalDuration(d.chartDuration),
            optionalDuration(d.inheritedDuration),
            optionalDuration(d.defaultDuration),
            {}};
}
auto requirement(const dto::Requirement& d, PreparedGrace resolvedGrace) -> RequirementRecord {
    RequirementRecord result;
    result.stableId = stable(d.stableId);
    result.identity = identity(d.identity);
    for (const auto& action : d.requiredActions) {
        result.requiredActions.push_back(checked(RequiredActionRef::fromToken(action)));
    }
    result.domainBinding = checked(DomainBindingRef::fromToken(d.domainBinding));
    result.judgementDomainId = d.judgementDomainId;
    for (const auto& p : d.phases) {
        result.phases.push_back({phase(p.kind), p.declarationOrdinal});
    }
    result.requiresReleaseTailSemantics = d.requiresReleaseTailSemantics;
    result.pattern = pattern(d.pattern);
    result.patternArmRefs = d.patternArmRefs;
    result.maxArmElements = MeasuredParameter<std::uint64_t>::measured(d.maxArmElements);
    result.maxDeadlineElements = MeasuredParameter<std::uint64_t>::measured(d.maxDeadlineElements);
    result.measure = measure(d.measure);
    for (const auto& c : d.resourceClaims) {
        require(c.policyToken == "coordinator.policy.greedy_v1", "resourceClaims.policyToken",
                "claim must declare selected greedy policy");
        auto intent = c.intent == "observe"   ? ResourceClaimIntent::observe
                      : c.intent == "consume" ? ResourceClaimIntent::consume
                                              : ResourceClaimIntent::claim;
        result.resourceClaims.push_back({{c.resourceId},
                                         {c.policyToken, "", {}},
                                         intent,
                                         {GraceOverrideMode::none, c.overrideToken}});
    }
    result.grace = grace(d.grace);
    result.preparedGrace = resolvedGrace;
    result.timing = RequirementRecord::Timing{Tick{d.timing.end}, {}, {}, {}};
    for (const auto& w : d.timing.successWindows) {
        result.timing->successWindows.push_back(
            {Tick{w.start}, Tick{w.end}, {phase(w.phase.kind), w.phase.declarationOrdinal}});
    }
    if (d.timing.body) {
        result.timing->body = TimeInterval{Tick{d.timing.body->start}, Tick{d.timing.body->end}};
    }
    for (const auto& t : d.timing.phaseTargets) {
        result.timing->phaseTargets.push_back({phase(t.phase), Tick{t.chartTick}});
    }
    for (const auto& b : d.atomBindings) {
        AtomBinding binding{b.atomRef,
                            b.domainToken,
                            b.sourceClass,
                            b.channelToken,
                            b.action == "press"     ? InputAction::press
                            : b.action == "release" ? InputAction::release
                                                    : InputAction::update,
                            {},
                            b.tailOnly};
        if (b.amountRange) {
            binding.amountRange = AmountMatchRange{b.amountRange->first, b.amountRange->second};
        }
        result.atomBindings.push_back(std::move(binding));
    }
    result.solverProfileRef = d.solverProfileRef;
    result.localClosePolicyToken = d.localClosePolicyToken;
    result.factBindingRefs = d.factBindingRefs;
    result.required = refs(d.requiredRefs);
    return result;
}
auto lowerCommon(const dto::AuthorSource& source) -> GameplaySourceDocument {
    const auto& c = source.common;
    GameplaySourceDocument result{source.sourceDocumentId, 5, 2, {}, {}, {}, {}, {}, {}, {}};
    const std::map<std::string, DeclarationKind> kinds{
        {"requirement", DeclarationKind::requirement},
        {"patternDefinition", DeclarationKind::patternDefinition},
        {"measureDefinition", DeclarationKind::measureDefinition},
        {"resourceRecord", DeclarationKind::resourceRecord},
        {"judgementDomain", DeclarationKind::judgementDomain},
        {"solverProfile", DeclarationKind::solverProfile}};
    for (const auto& d : c.declarations) {
        require(d.stableId.sourceDocumentId == source.sourceDocumentId, "declarations.stableId",
                "declaration belongs to another source document");
        LocalDeclaration row{
            stable(d.stableId), kinds.at(d.kind), d.localName, {}, refs(d.requiredRefs)};
        for (const auto& r : d.references) {
            row.references.push_back({r.scope == "sameDocument"
                                          ? ReferenceScope::sameDocument
                                          : ReferenceScope::explicitCrossDocument,
                                      r.sourceDocumentId.value_or(""), r.declarationOrdinal});
        }
        result.declarations.push_back(std::move(row));
    }
    for (const auto& r : c.resources) {
        result.resources.push_back({{r.resourceId},
                                    r.declaredCapacity,
                                    r.slotToken,
                                    r.decisionPolicyRef,
                                    r.terminalAfterTermination,
                                    TickSpan{r.declaredGapGrace},
                                    {},
                                    refs(r.requiredRefs)});
    }
    for (const auto& r : c.relations) {
        require(r.policyToken == "coordinator.policy.greedy_v1", "relations.policyToken",
                "relation must declare selected policy");
        RelationDeclaration row{RelationKind::exclusive, {r.resourceId},     {},
                                {r.policyToken, "", {}}, r.declaredCapacity, refs(r.requiredRefs)};
        for (const auto& member : r.members) {
            row.members.push_back(stable(member));
        }
        result.relations.push_back(std::move(row));
    }
    for (const auto& s : c.solverProfiles) {
        result.solverProfiles.push_back({s.solverId, s.revision, s.algorithmToken, s.objective,
                                         s.tieBreak, s.rejectIfNonUnique, refs(s.requiredRefs)});
    }
    for (const auto& f : c.factBindings) {
        result.factBindings.push_back({f});
    }
    for (const auto& d : c.judgementDomains) {
        JudgementDomainRecord row{d.domainId, d.coordinateSystemToken,
                                  {},         FrameResolution::staticDeclaration,
                                  "",         refs(d.requiredRefs)};
        for (const auto& axis : d.axes) {
            row.axes.push_back({axis.axisToken, axis.minimum, axis.maximum});
        }
        result.judgementDomains.push_back(std::move(row));
    }
    return result;
}

auto sameEmission(const chart::GeneratedEntityIdentity& entity,
                  const RequirementIdentity& requirement, const ChartContext& context) -> bool {
    if (entity.chartId != context.foundation.chartId ||
        requirement.chartEntryId != context.entryId ||
        entity.bindingId != requirement.invocationId || entity.moduleId != requirement.moduleId ||
        entity.exportId != requirement.exportId ||
        entity.path.size() != requirement.emissionPath.size()) {
        return false;
    }
    for (std::size_t i = 0; i < entity.path.size(); ++i) {
        if (entity.path[i].nodeId != requirement.emissionPath[i].nodeId ||
            entity.path[i].iterationIndexPlusOne != requirement.emissionPath[i].repeatIndex) {
            return false;
        }
    }
    return true;
}
auto foundationIdentity(const dto::FoundationIdentity& d) -> chart::CanonicalEntityIdentity {
    if (d.kind == "explicit") {
        return chart::ExplicitEntityIdentity{{d.objectId}};
    }
    chart::GeneratedEntityIdentity result{{d.chartId}, d.bindingId, d.moduleId, d.exportId, {}};
    for (const auto& p : d.path) {
        result.path.push_back({p.first, p.second});
    }
    return result;
}
auto foundationTransform(const dto::FoundationTransform& d) -> chart::CanonicalTransform {
    return {{d.position[0], d.position[1], d.position[2]},
            {d.rotation[0], d.rotation[1], d.rotation[2], d.rotation[3]},
            {d.scale[0], d.scale[1], d.scale[2]}};
}
auto lowerInline(const dto::InlineFoundation& d) -> chart::CanonicalSemanticChart {
    chart::CanonicalSemanticChart result;
    result.chartId = {d.chartId};
    if (d.mainMusic) {
        result.mainMusic = chart::AssetId{*d.mainMusic};
    }
    for (const auto& f : d.features) {
        result.features.push_back({f.first, f.second});
    }
    result.timing = {d.offsetMs, d.defaultBpm, {}, {}};
    for (const auto& t : d.tempoEvents) {
        result.timing.tempoEvents.push_back(
            {checked(chart::RationalBeat::create(t.startBeat.numerator, t.startBeat.denominator)),
             checked(chart::RationalBeat::create(t.durationBeats.numerator,
                                                 t.durationBeats.denominator)),
             t.startBpm, t.endBpm, t.startSlope, t.endSlope});
    }
    for (const auto& t : d.stops) {
        result.timing.stops.push_back(
            {checked(chart::RationalBeat::create(t.beat.numerator, t.beat.denominator)),
             t.durationMs});
    }
    result.defaultCamera = {d.camera.type,
                            d.camera.fovY,
                            d.camera.nearPlane,
                            d.camera.farPlane,
                            d.pitch,
                            d.yaw,
                            d.roll,
                            {}};
    if (d.defaultTransform) {
        const auto t = foundationTransform(*d.defaultTransform);
        result.defaultCamera.defaultTransform =
            chart::TransformData{t.position, t.rotation, t.scale};
    }
    for (const auto& r : d.resources) {
        const auto use = r.second == "mainMusic" ? chart::CanonicalResourceUseKind::MainMusic
                         : r.second == "renderableMesh"
                             ? chart::CanonicalResourceUseKind::RenderableMesh
                             : chart::CanonicalResourceUseKind::RenderableMaterial;
        result.resourceClosure.resources.push_back({{r.first}, use});
    }
    for (const auto& e : d.entities) {
        chart::CanonicalEntity entity{foundationIdentity(e.identity), {}, {}, {}};
        if (e.parent) {
            entity.parent = foundationIdentity(*e.parent);
        }
        for (const auto& c : e.components) {
            if (c.transform) {
                entity.components.push_back(foundationTransform(*c.transform));
            } else if (c.camera) {
                entity.components.push_back(chart::CanonicalCamera{
                    c.camera->type, c.camera->fovY, c.camera->nearPlane, c.camera->farPlane});
            } else {
                entity.components.push_back(chart::CanonicalRenderable{
                    {c.mesh}, {c.material}, static_cast<std::uint8_t>(c.alpha)});
            }
        }
        result.entities.push_back(std::move(entity));
    }
    return result;
}
auto frozenCount(const dto::RepeatCount& count, const dto::AuthorSource& source,
                 const AuthorCompileContext& context) -> std::uint64_t {
    if (count.literal) {
        return *count.literal;
    }
    require(count.parameter.has_value() && context.invocation.has_value(), "rankAssignment.radices",
            "repeat parameter is not frozen");
    const auto& parameters = context.invocation->parameters;
    auto binding = std::find_if(parameters.begin(), parameters.end(),
                                [&](const auto& p) { return p.id == *count.parameter; });
    std::int64_t value = 0;
    if (binding != parameters.end()) {
        require(std::holds_alternative<std::int64_t>(binding->value), "rankAssignment.radices",
                "repeat requires integer Binding");
        value = std::get<std::int64_t>(binding->value);
    } else {
        auto declared = std::find_if(source.integerDefaults.begin(), source.integerDefaults.end(),
                                     [&](const auto& d) { return d.id == *count.parameter; });
        require(declared != source.integerDefaults.end(), "rankAssignment.radices",
                "repeat parameter default is missing");
        value = declared->value;
    }
    require(value >= 0, "rankAssignment.radices", "negative repeat cardinality");
    return static_cast<std::uint64_t>(value);
}
auto affineRank(const dto::RankBlock& block, const RequirementIdentity& e,
                const dto::AuthorSource& source, const AuthorCompileContext& context)
    -> std::uint64_t {
    require(block.stride > 0 && block.nodeOrder.size() == block.radices.size(), "rankAssignment",
            "invalid affine stride or radix count");
    const dto::EmissionFamily* family = nullptr;
    for (const auto& f : source.emissionFamilies) {
        if (f.exportId != e.exportId || f.repeats.size() + 1 != e.emissionPath.size() ||
            e.emissionPath.back().nodeId != f.emitNodeId ||
            e.emissionPath.back().repeatIndex != 0) {
            continue;
        }
        bool match = true;
        for (std::size_t i = 0; i < f.repeats.size(); ++i) {
            match = match && f.repeats[i].nodeId == e.emissionPath[i].nodeId;
        }
        if (match) {
            require(family == nullptr, "rankAssignment.nodeOrder", "ambiguous emission family");
            family = &f;
        }
    }
    require(family != nullptr && family->repeats.size() == block.nodeOrder.size(),
            "rankAssignment.nodeOrder", "affine block does not cover complete Repeat ancestry");
    std::set<std::string> nodes;
    std::uint64_t index = 0;
    for (std::size_t i = 0; i < block.nodeOrder.size(); ++i) {
        const auto count = frozenCount(family->repeats[i], source, context);
        const auto marker = e.emissionPath[i].repeatIndex;
        require(nodes.insert(block.nodeOrder[i]).second &&
                    block.nodeOrder[i] == family->repeats[i].nodeId && block.radices[i] == count &&
                    marker > 0 && marker <= count,
                "rankAssignment.nodeOrder", "node order, marker or frozen radix mismatch");
        require(count != 0 && index <= UINT64_MAX / count, "rankAssignment",
                "mixed radix product overflow");
        index *= count;
        require(marker - 1 <= UINT64_MAX - index, "rankAssignment", "mixed radix sum overflow");
        index += marker - 1;
    }
    require(index <= (UINT64_MAX - block.base) / block.stride, "rankAssignment",
            "affine rank overflow");
    return block.base + block.stride * index;
}
void ranks(GameplaySourceDocument& document, const dto::AuthorSource& source,
           const AuthorCompileContext& context) {
    const auto& assignment = source.rankAssignment;
    std::vector<bool> usedRows(assignment.rows.size(), false),
        usedBlocks(assignment.blocks.size(), false);
    for (auto& r : document.requirements) {
        if (!r.resourceClaims.empty() &&
            r.resourceClaims[0].intent == ResourceClaimIntent::observe) {
            continue;
        }
        std::optional<ClaimPolicyDeclaration::CompetitionKey> pair;
        std::string nameSpace;
        if (assignment.mode == "explicit") {
            for (std::size_t i = 0; i < assignment.rows.size(); ++i) {
                const auto& row = assignment.rows[i];
                if (identity(row.identity) != r.identity) {
                    continue;
                }
                require(!pair && !usedRows[i], "rankAssignment.rows", "duplicate rank row");
                pair = {row.priority, row.tieRank};
                nameSpace = row.nameSpace;
                usedRows[i] = true;
            }
        } else {
            for (std::size_t i = 0; i < assignment.blocks.size(); ++i) {
                const auto& b = assignment.blocks[i];
                if (std::tie(b.invocationId, b.moduleId, b.exportId, b.requirementLocalId) !=
                    std::tie(r.identity.invocationId, r.identity.moduleId, r.identity.exportId,
                             r.identity.requirementLocalId)) {
                    continue;
                }
                require(!pair, "rankAssignment.blocks", "overlapping affine blocks");
                pair = {b.priority, affineRank(b, r.identity, source, context)};
                nameSpace = b.nameSpace;
                usedBlocks[i] = true;
            }
        }
        require(pair.has_value(), "rankAssignment", "occupying Requirement has no rank");
        if (r.resourceClaims.empty()) {
            require(nameSpace == "@independent", "rankAssignment.namespace",
                    "independent namespace must be explicit");
            r.independentCompetition = pair;
        } else {
            require(!nameSpace.empty() && nameSpace != "@independent", "rankAssignment.namespace",
                    "real claim needs a real namespace");
            r.resourceClaims[0].claimPolicy.claimKeyToken = nameSpace;
            r.resourceClaims[0].claimPolicy.competition = pair;
        }
    }
    require(std::all_of(usedRows.begin(), usedRows.end(), [](bool used) { return used; }),
            "rankAssignment.rows", "unknown or observe rank row");
    require(std::all_of(usedBlocks.begin(), usedBlocks.end(), [](bool used) { return used; }),
            "rankAssignment.blocks", "unused affine block");
}
} // namespace

auto compile(std::string_view bytes, dto::SourceKind kind, const AuthorCompileContext& context)
    -> core::Result<AuthorPreparedArtifact> try {
    auto source = checked(dto::readAuthorSource(bytes, kind,
                                                {context.foundationLimits.maxInputBytes,
                                                 context.foundationLimits.maxNestingDepth,
                                                 context.foundationLimits.maxStringBytes}));
    require(source.common.chartEntryId == context.chart.entryId &&
                !context.compilerProfileToken.empty(),
            "common.chartEntryId", "source Chart context mismatch");
    require(source.common.normalizationProfileToken ==
                context.identityDeclarations.session.normalizationProfileToken,
            "common.normalizationProfileToken", "normalization declaration mismatch");
    auto foundation = context.chart.foundation;
    if (kind == dto::SourceKind::chartInline) {
        require(source.inlineFoundation.has_value() &&
                    source.inlineFoundation->chartId == foundation.chartId.value,
                "chartId", "inline Chart context identity mismatch");
        require(source.rankAssignment.mode == "explicit", "rankAssignment",
                "inline has no CXT Repeat cardinality proof");
        foundation = lowerInline(*source.inlineFoundation);
    } else {
        require(context.invocation.has_value() && context.invocation->chartId == foundation.chartId,
                "invocation", "explicit CXT Binding / Chart context required");
        auto expanded = chart::CxtV2Loader::expand(source.foundationSource, *context.invocation,
                                                   context.foundationLimits);
        if (!expanded.chart || expanded.diagnostics.hasErrors()) {
            const auto& diagnostics = expanded.diagnostics.items();
            throw Reject{
                diagnostics.empty()
                    ? invalid("cxt", "Foundation expansion failed")
                    : core::Error{std::string{diagnostics.front().code()},
                                  std::string{diagnostics.front().message()}}
                          .withContext("category", "invalid_relation")
                          .withContext("severity", "error")
                          .withContext("faulted", "false")
                          .withContext("field.path", std::string{diagnostics.front().fieldPath()})};
        }
        foundation.entities.insert(foundation.entities.end(), expanded.chart->entities.begin(),
                                   expanded.chart->entities.end());
    }
    for (const auto& e : foundation.entities) {
        require(e.requirements.empty(), "entities.requirements",
                "legacy requirements cannot coexist with Gameplay V2");
    }
    auto document = lowerCommon(source);
    std::vector<RequirementGraceInput> graceBindings;
    std::vector<NamedGraceDuration> named;
    for (const auto& v : source.common.graceInputs) {
        graceBindings.push_back({stable(v.requirement), graceInputs(v.inputs)});
    }
    for (const auto& v : source.common.namedGraceDurations) {
        named.push_back({v.declarationId, duration(v.duration)});
    }
    std::vector<gameplay_packed::RequirementOwner> owners;
    for (const auto& dtoRequirement : source.requirements) {
        require(dtoRequirement.stableId.sourceDocumentId == source.sourceDocumentId &&
                    dtoRequirement.identity.chartEntryId == context.chart.entryId,
                "requirements.identity", "Requirement belongs to another source / Chart context");
        auto binding =
            std::find_if(graceBindings.begin(), graceBindings.end(), [&](const auto& row) {
                return row.requirement == stable(dtoRequirement.stableId);
            });
        require(binding != graceBindings.end(), "graceInputs",
                "Requirement grace input is missing");
        auto inputs = binding->inputs;
        auto declaration = grace(dtoRequirement.grace);
        if (declaration.policy == GraceResolutionPolicy::inheritedDeclaration) {
            auto literal = std::find_if(named.begin(), named.end(), [&](const auto& row) {
                return row.declarationId == declaration.inheritedFromDeclarationId;
            });
            require(literal != named.end() && (!inputs.inheritedDuration ||
                                               *inputs.inheritedDuration == literal->duration),
                    "namedGraceDurations", "inheritance must resolve its named literal");
            inputs.inheritedDuration = literal->duration;
        }
        auto row = requirement(dtoRequirement, checked(resolvePreparedGrace(declaration, inputs)));
        std::optional<chart::CanonicalEntityIdentity> owner;
        for (const auto& entity : foundation.entities) {
            const auto* generated = std::get_if<chart::GeneratedEntityIdentity>(&entity.identity);
            if (generated && sameEmission(*generated, row.identity, context.chart)) {
                require(!owner, "requirements.identity", "emission is not unique");
                owner = entity.identity;
            }
        }
        require(owner.has_value(), "requirements.identity",
                "Requirement has no real generated Foundation entity");
        owners.push_back({row.identity, *owner});
        document.requirements.push_back(std::move(row));
    }
    ranks(document, source, context);
    const auto& t = source.common.timebase;
    TimebaseProfile timebase{t.profileId,
                             t.unitToken,
                             duration(t.tickScale),
                             beat(t.originBeat),
                             duration(t.initialTempo),
                             {},
                             {}};
    for (const auto& v : t.tempoSections) {
        timebase.tempoSections.push_back({beat(v.startBeat), duration(v.durationPerBeat)});
    }
    for (const auto& v : t.stopSections) {
        timebase.stopSections.push_back({beat(v.startBeat), beat(v.endBeat), duration(v.duration)});
    }
    const auto& l = source.common.latePolicy;
    LatePolicyParameters late{
        MeasuredParameter<TickSpan>::measured(TickSpan{l.finalizationWatermark}),
        MeasuredParameter<TickSpan>::measured(TickSpan{l.maxQueueHop}),
        MeasuredParameter<TickSpan>::measured(TickSpan{l.windowCloseThreshold}),
        MeasuredParameter<TickSpan>::measured(TickSpan{l.windowOpenThreshold}),
        l.mode == "reject_late" ? LateEventPolicy::rejectLate : LateEventPolicy::queueNextTick};
    DeclaredCapabilitySet capabilities;
    for (const auto& v : source.common.declaredCapabilities) {
        capabilities.capabilities.push_back({v.capabilityId, v.revision.value_or("")});
    }
    FeatureClosure features;
    for (const auto& v : source.common.declaredFeatures) {
        features.features.push_back({v});
    }
    AssemblyRequest assembly{EntryKind::authorSource,
                             false,
                             {{SourceForm::memory, "author.memory", std::move(document)}},
                             source.common.graphRevision,
                             source.common.rulesetRef,
                             capabilities,
                             features,
                             context.closureContributions,
                             context.compilerProfileToken,
                             &timebase,
                             &late,
                             context.capabilities,
                             context.contentLimits,
                             context.identityDeclarations,
                             source.common.executionProfile,
                             source.common.normalizationProfileToken,
                             source.common.coordinatorPolicy};
    auto prepared = checked(prepareGameplay(
        {std::move(assembly), std::move(graceBindings), std::move(named), context.patternBudget}));
    std::vector<PatternDeclaration> patterns;
    std::vector<gameplay_packed::MeasureDefinition> measures;
    for (const auto& v : source.common.patternDefinitions) {
        patterns.push_back(pattern(v));
    }
    for (const auto& v : source.common.measureDefinitions) {
        measures.push_back({v.id, measure(v.declaration)});
    }
    for (const auto& d : source.common.declarations) {
        if (d.kind == "patternDefinition") {
            require(std::count_if(patterns.begin(), patterns.end(),
                                  [&](const auto& p) { return p.patternId == d.localName; }) == 1,
                    "patternDefinitions",
                    "named declaration must have exactly one Pattern definition");
        } else if (d.kind == "measureDefinition") {
            require(std::count_if(measures.begin(), measures.end(),
                                  [&](const auto& m) { return m.id == d.localName; }) == 1,
                    "measureDefinitions",
                    "named declaration must have exactly one Measure definition");
        }
    }
    std::set<std::string> definitionNames;
    for (const auto& p : patterns) {
        if (p.patternId.empty()) {
            continue;
        }
        require(definitionNames.insert(p.patternId).second &&
                    std::count_if(
                        source.common.declarations.begin(), source.common.declarations.end(),
                        [&](const auto& d) {
                            return d.kind == "patternDefinition" && d.localName == p.patternId;
                        }) == 1,
                "patternDefinitions", "named Pattern must have one unique declaration");
    }
    for (const auto& m : measures) {
        if (!m.id) {
            continue;
        }
        require(definitionNames.insert(*m.id).second &&
                    std::count_if(source.common.declarations.begin(),
                                  source.common.declarations.end(),
                                  [&](const auto& d) {
                                      return d.kind == "measureDefinition" && d.localName == *m.id;
                                  }) == 1,
                "measureDefinitions", "named Measure must have one unique declaration");
    }
    gameplay_packed::CapsuleProfiles profiles{source.common.normalizationProfileToken,
                                              source.common.coordinatorPolicy};
    checked(
        gameplay_packed::encode({foundation, prepared, owners, profiles, patterns, measures, 3}));
    return AuthorPreparedArtifact{std::move(foundation),
                                  std::move(prepared),
                                  std::move(owners),
                                  std::move(profiles),
                                  std::move(patterns),
                                  std::move(measures),
                                  std::move(source.originalSource)};
} catch (const Reject& rejected) {
    return core::unexpected(rejected.error);
} catch (const std::exception& e) {
    return core::unexpected(invalid("author", e.what()));
}

auto read(const std::filesystem::path& path, dto::SourceKind kind,
          const AuthorCompileContext& context) -> core::Result<AuthorPreparedArtifact> try {
    std::ifstream input{path, std::ios::binary | std::ios::ate};
    require(static_cast<bool>(input), "source", "source file could not be opened");
    const auto end = input.tellg();
    require(end >= 0 && static_cast<std::uintmax_t>(end) <= context.foundationLimits.maxInputBytes,
            "source", "original source exceeds byte budget");
    std::string bytes(static_cast<std::size_t>(end), '\0');
    input.seekg(0);
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    require(static_cast<bool>(input), "source", "source file read failed");
    return compile(bytes, kind, context);
} catch (const Reject& rejected) {
    return core::unexpected(rejected.error);
} catch (const std::exception& e) {
    return core::unexpected(invalid("source", e.what()));
}

auto compileInto(std::optional<AuthorPreparedArtifact>& active, std::string_view source,
                 dto::SourceKind kind, const AuthorCompileContext& context) -> core::Result<void> {
    auto candidate = compile(source, kind, context);
    if (!candidate) {
        return core::unexpected(candidate.error());
    }
    active = std::move(*candidate);
    return {};
}
} // namespace cuexis::tools::gameplay_author
