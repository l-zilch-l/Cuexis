#pragma once

#include <array>
#include <cstdint>
#include <cuexis/core/result.hpp>
#include <optional>
#include <string>
#include <vector>

namespace cuexis::json::gameplay {
// Owning semantic DTOs. JSON objects, readers and third-party DOM types do not escape this module.
struct Q {
    std::int64_t numerator, denominator;
};
struct StableId {
    std::string sourceDocumentId;
    std::uint32_t declarationOrdinal;
};
struct PathStep {
    std::string nodeId;
    std::uint64_t repeatIndex;
};
struct Identity {
    std::string chartEntryId, invocationId, moduleId, exportId;
    std::vector<PathStep> emissionPath;
    std::string requirementLocalId;
};
struct Capability {
    std::string capabilityId;
    std::optional<std::string> revision;
};
struct Refs {
    std::vector<std::string> features;
    std::vector<Capability> capabilities;
};
struct Phase {
    std::string kind;
    std::uint32_t declarationOrdinal;
};
struct PatternNode {
    std::string primitive;
    std::vector<PatternNode> operands;
    std::optional<std::string> atomRef;
    std::optional<std::pair<std::uint64_t, std::uint64_t>> repeatBounds;
};
struct Pattern {
    std::optional<std::string> patternId;
    std::string matchPolicy;
    PatternNode root;
    Refs requiredRefs;
};
struct MeasureComponent {
    std::string phase, categoryToken;
    std::vector<std::string> gradeTokens;
};
struct Measure {
    std::vector<MeasureComponent> components;
    Refs requiredRefs;
};
struct MeasureDefinition {
    std::optional<std::string> id;
    Measure declaration;
};
struct Grace {
    std::string policy;
    bool allowChartGrace;
    std::optional<std::string> inheritedFromDeclarationId;
};
struct GraceInputs {
    Q unitInTicks;
    std::int64_t minimumCanonical, maximumCanonical;
    std::optional<Q> chartDuration, inheritedDuration, defaultDuration;
};
struct GraceBinding {
    StableId requirement;
    GraceInputs inputs;
};
struct Interval {
    std::int64_t start, end;
};
struct Window {
    Phase phase;
    std::int64_t start, end;
};
struct Target {
    std::string phase;
    std::int64_t chartTick;
};
struct Timing {
    std::int64_t end;
    std::vector<Window> successWindows;
    std::optional<Interval> body;
    std::vector<Target> phaseTargets;
};
struct AtomBinding {
    std::string atomRef, domainToken, sourceClass, channelToken, action;
    std::optional<std::pair<std::int64_t, std::int64_t>> amountRange;
    bool tailOnly;
};
struct Claim {
    std::string resourceId, intent, policyToken, overrideMode, overrideToken;
};
struct Requirement {
    StableId stableId;
    Identity identity;
    std::vector<std::string> requiredActions;
    std::string domainBinding, judgementDomainId;
    std::vector<Phase> phases;
    bool requiresReleaseTailSemantics;
    Pattern pattern;
    std::vector<std::string> patternArmRefs;
    std::uint64_t maxArmElements, maxDeadlineElements;
    Measure measure;
    std::vector<Claim> resourceClaims;
    Grace grace;
    Timing timing;
    std::vector<AtomBinding> atomBindings;
    std::string solverProfileRef, localClosePolicyToken;
    std::vector<std::string> factBindingRefs;
    Refs requiredRefs;
};
struct DeclarationRef {
    std::string scope;
    std::optional<std::string> sourceDocumentId;
    std::uint32_t declarationOrdinal;
};
struct Declaration {
    StableId stableId;
    std::string kind, localName;
    std::vector<DeclarationRef> references;
    Refs requiredRefs;
};
struct Resource {
    std::string resourceId;
    std::uint64_t declaredCapacity;
    std::string slotToken, decisionPolicyRef;
    bool terminalAfterTermination;
    std::int64_t declaredGapGrace;
    Refs requiredRefs;
};
struct Relation {
    std::string kind, resourceId;
    std::vector<StableId> members;
    std::string policyToken;
    std::uint64_t declaredCapacity;
    Refs requiredRefs;
};
struct Solver {
    std::string solverId, revision, algorithmToken;
    std::vector<std::string> objective, tieBreak;
    bool rejectIfNonUnique;
    Refs requiredRefs;
};
struct Axis {
    std::string axisToken;
    std::int64_t minimum, maximum;
};
struct Domain {
    std::string domainId, coordinateSystemToken;
    std::vector<Axis> axes;
    std::string frame;
    Refs requiredRefs;
};
struct Tempo {
    Q startBeat, durationPerBeat;
};
struct Stop {
    Q startBeat, endBeat, duration;
};
struct Timebase {
    std::string profileId, unitToken;
    Q tickScale, originBeat, initialTempo;
    std::vector<Tempo> tempoSections;
    std::vector<Stop> stopSections;
};
struct Late {
    std::string mode;
    std::int64_t finalizationWatermark, maxQueueHop, windowCloseThreshold, windowOpenThreshold;
};
struct NamedGrace {
    std::string declarationId;
    Q duration;
};
struct Common {
    std::string chartEntryId;
    std::uint64_t graphRevision;
    std::string rulesetRef, normalizationProfileToken, coordinatorPolicy, executionProfile;
    Timebase timebase;
    Late latePolicy;
    std::vector<GraceBinding> graceInputs;
    std::vector<Declaration> declarations;
    std::vector<Resource> resources;
    std::vector<Relation> relations;
    std::vector<Solver> solverProfiles;
    std::vector<std::string> factBindings;
    std::vector<Domain> judgementDomains;
    std::vector<std::string> declaredFeatures;
    std::vector<Capability> declaredCapabilities;
    std::vector<NamedGrace> namedGraceDurations;
    std::vector<Pattern> patternDefinitions;
    std::vector<MeasureDefinition> measureDefinitions;
};
struct RankRow {
    Identity identity;
    std::int64_t priority;
    std::uint64_t tieRank;
    std::string nameSpace;
};
struct RankBlock {
    std::string invocationId, moduleId, exportId, requirementLocalId;
    std::int64_t priority;
    std::uint64_t base, stride;
    std::string nameSpace;
    std::vector<std::string> nodeOrder;
    std::vector<std::uint64_t> radices;
};
struct RankAssignment {
    std::string mode;
    std::vector<RankRow> rows;
    std::vector<RankBlock> blocks;
};
struct RepeatCount {
    std::string nodeId;
    std::optional<std::uint64_t> literal;
    std::optional<std::string> parameter;
};
struct EmissionFamily {
    std::string exportId;
    std::vector<RepeatCount> repeats;
    std::string emitNodeId;
};
struct IntegerDefault {
    std::string id;
    std::int64_t value;
};
struct FoundationIdentity {
    std::string kind, objectId, chartId, bindingId, moduleId, exportId;
    std::vector<std::pair<std::string, std::uint32_t>> path;
};
struct FoundationTransform {
    std::array<float, 3> position, scale;
    std::array<float, 4> rotation;
};
struct FoundationCamera {
    std::string type;
    double fovY, nearPlane, farPlane;
};
struct FoundationComponent {
    std::string kind;
    std::optional<FoundationTransform> transform;
    std::optional<FoundationCamera> camera;
    std::string mesh, material;
    std::uint32_t alpha{};
};
struct FoundationEntity {
    FoundationIdentity identity;
    std::optional<FoundationIdentity> parent;
    std::vector<FoundationComponent> components;
};
struct FoundationTempo {
    Q startBeat, durationBeats;
    double startBpm, endBpm, startSlope, endSlope;
};
struct FoundationStop {
    Q beat;
    double durationMs;
};
struct InlineFoundation {
    std::string chartId;
    std::optional<std::string> mainMusic;
    std::vector<std::pair<std::string, std::uint32_t>> features;
    double offsetMs, defaultBpm;
    std::vector<FoundationTempo> tempoEvents;
    std::vector<FoundationStop> stops;
    FoundationCamera camera;
    double pitch, yaw, roll;
    std::optional<FoundationTransform> defaultTransform;
    std::vector<std::pair<std::string, std::string>> resources;
    std::vector<FoundationEntity> entities;
};
enum class SourceKind : std::uint8_t { chartInline, cxtModule };
struct AuthorSource {
    SourceKind kind;
    std::string sourceDocumentId;
    Common common;
    std::vector<Requirement> requirements;
    RankAssignment rankAssignment;
    std::string foundationSource;
    std::string originalSource;
    std::vector<EmissionFamily> emissionFamilies;
    std::vector<IntegerDefault> integerDefaults;
    std::optional<InlineFoundation> inlineFoundation;
};
struct SourceLimits {
    std::size_t maxInputBytes, maxNestingDepth, maxStringBytes;
};
[[nodiscard]] auto readAuthorSource(std::string_view original, SourceKind kind, SourceLimits limits)
    -> core::Result<AuthorSource>;
} // namespace cuexis::json::gameplay
