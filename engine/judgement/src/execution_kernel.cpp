#include "execution_kernel.hpp"
#include "execution_testing.hpp"

#include <algorithm>
#include <bit>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>

namespace cuexis::judgement::detail {
namespace {
auto error(std::string_view suffix, std::string_view message, bool faulted = false) -> core::Error {
    return core::Error{"judgement.s7a4." + std::string{suffix}, std::string{message}}
        .withContext("category", suffix == "execution.profile_incomplete"
                                     ? "identity_closure_incomplete"
                                     : "invalid_relation")
        .withContext("severity", "error")
        .withContext("faulted", faulted ? "true" : "false");
}
auto sameTimebase(const TimebaseProfile& a, const TimebaseProfile& b) -> bool {
    return a.profileId == b.profileId && a.unitToken == b.unitToken && a.tickScale == b.tickScale &&
           a.originBeat == b.originBeat && a.initialTempo == b.initialTempo &&
           a.tempoSections == b.tempoSections && a.stopSections == b.stopSections;
}
auto keyOf(const NormalizedObservation& v) -> CanonicalObservationKey {
    return {v.observationTick,
            std::string{v.domainToken},
            std::string{v.sourceClass.token()},
            std::string{v.channel.token()},
            v.action,
            v.amount ? std::optional{v.amount->value.canonicalInteger} : std::nullopt};
}
auto sameChannel(const ContactHandle& contact, const CanonicalObservationKey& key) -> bool {
    return contact.domainToken == key.domainToken && contact.sourceClass == key.sourceClass &&
           contact.channelToken == key.channelToken;
}
auto matches(const AtomBinding& binding, const CanonicalObservationKey& key) -> bool {
    return binding.domainToken == key.domainToken && binding.sourceClass == key.sourceClass &&
           binding.channelToken == key.channelToken && binding.action == key.action &&
           (!binding.amountRange || (key.amount && *key.amount >= binding.amountRange->minimum &&
                                     *key.amount <= binding.amountRange->maximum));
}
auto observer(const RequirementRecord& r) -> bool {
    return !r.resourceClaims.empty() &&
           r.resourceClaims.front().intent == ResourceClaimIntent::observe;
}
auto rootPhase(const RequirementRecord& r) -> PhaseKind {
    return r.phases.front().kind == PhaseKind::tap ? PhaseKind::tap : PhaseKind::head;
}
struct MatcherState final {
    std::vector<std::size_t> states;
    bool epsilonAttempted{false};
};
struct RuntimeState final : ProjectionStorage {
    RuntimeState(KernelProjection projection, std::vector<MatcherState> matchers)
        : ProjectionStorage(std::move(projection)), matchers(std::move(matchers)) {}
    std::vector<MatcherState> matchers;
    std::vector<HeadContactOwnership> coverageHistory;
    std::vector<std::size_t> activeRequirements;
    std::vector<Tick> signalVisibility;
    std::size_t timerCursor{0};
    std::uint64_t nextCommitId{0};
    bool idsExhausted{false};
};
struct FactDescriptor final {
    OriginId origin;
    std::optional<RequirementIdentity> requirement;
    std::optional<PhaseKind> phase;
    FactKind kind;
    Outcome outcome;
    std::optional<TickDelta> error;
    std::optional<CanonicalObservationKey> evidence;
};
} // namespace

auto validateSessionConfiguration(const SessionConfiguration& c) -> core::Result<void> try {
    const auto& engine = c.identityDeclarations.engine;
    if (c.executionProfileToken.empty() || c.lateAlgorithmToken.empty() ||
        c.factSemanticRevision.empty() || c.calibrationIdentityToken.empty() ||
        (!engine.executionProfileToken || engine.executionProfileToken->empty()) ||
        (!engine.lateAlgorithmToken || engine.lateAlgorithmToken->empty()) ||
        engine.judgementSemanticRevision.empty() || engine.factSemanticRevision.empty() ||
        engine.fixedPointTableId.empty() || engine.coordinationPhaseOrderToken.empty()) {
        return core::unexpected(core::Error{"judgement.s7a4.execution.profile_incomplete",
                                            "execution configuration tokens are incomplete"}
                                    .withContext("category", "identity_closure_incomplete")
                                    .withContext("severity", "error")
                                    .withContext("faulted", "false"));
    }
    if (c.executionProfileToken != "gameplay.execution.t4-k4.v1" ||
        c.lateAlgorithmToken != "late.window.logical.v1" ||
        c.factSemanticRevision != "fact.semantic.phase-local.v1" ||
        c.calibrationIdentityToken.empty() ||
        engine.executionProfileToken != c.executionProfileToken ||
        engine.lateAlgorithmToken != c.lateAlgorithmToken ||
        engine.judgementSemanticRevision != "judgement.t4-k4.v1" ||
        engine.factSemanticRevision != c.factSemanticRevision ||
        engine.fixedPointTableId != "fixed-point.none.v1" ||
        engine.coordinationPhaseOrderToken != "coordination.six-eight.t4-k4.v1") {
        return core::unexpected(
            core::Error{"judgement.s7a4.execution.profile_unsupported",
                        "execution configuration tokens are unsupported or inconsistent"}
                .withContext("category", "capability_disabled")
                .withContext("severity", "error")
                .withContext("faulted", "false"));
    }
    auto mapping = c.inputMapping.view();
    if (auto valid = validateInputMapping(mapping); !valid) {
        return core::unexpected(valid.error());
    }
    if (auto valid = validatePrepare(c.timebase.view(), c.latePolicy); !valid) {
        return core::unexpected(valid.error());
    }
    return validateExecutionLateParameters(c.latePolicy);
} catch (const std::exception&) {
    return core::unexpected(error("execution.relation_invalid", "configuration allocation failed"));
}

struct ExecutionKernel::Impl final {
    SessionConfiguration configuration;
    PreparedGameplay prepared;
    SessionIngressState ingress;
    bool historyAvailable{true};
    std::vector<AdmittedObservation> pending;
    std::shared_ptr<const RuntimeState> active;
    std::optional<KernelTestControls> testControls;
    std::shared_ptr<RuntimeState> faultReserve;
    std::shared_ptr<const RecoveryInputs> recoveryInputs;
    std::shared_ptr<const ReplayData> recording;
    std::shared_ptr<const ReplayData> seekSource;
    core::Error failureResult;
    core::Error foldFailureResult;

    Impl(SessionConfiguration c, PreparedGameplay p, std::shared_ptr<const RuntimeState> state)
        : configuration(std::move(c)), prepared(std::move(p)), active(std::move(state)),
          faultReserve(std::make_shared<RuntimeState>(*active)),
          failureResult(
              error("kernel.transaction_failed", "the unsealed Tick transaction failed", true)),
          foldFailureResult(
              core::Error{"ruleset.transaction_failed", "the sealed Tick Fold transaction failed"}
                  .withContext("category", "invalid_relation")
                  .withContext("severity", "error")
                  .withContext("faulted", "true")) {
        recoveryInputs =
            std::make_shared<const RecoveryInputs>(RecoveryInputs{configuration, prepared});
        recording = std::make_shared<const ReplayData>(
            ReplayData{{}, 0, active->projection.judgementIdentity});
        faultReserve->projection.state = KernelSessionState::Faulted;
        faultReserve->projection.faultDiagnostic = failureResult;
    }
    auto reserveFault(const RuntimeState& value) const -> std::shared_ptr<RuntimeState> {
        auto backup = std::make_shared<RuntimeState>(value);
        backup->projection.state = KernelSessionState::Faulted;
        backup->projection.faultDiagnostic = failureResult;
        return backup;
    }
    auto graph() const -> const CanonicalGameplayGraph& {
        return prepared.assembled().graph;
    }
    void failIfRequested(Tick tick, KernelFailurePoint point) const {
        if (testControls && testControls->failedTick == tick &&
            testControls->failurePoint == point) {
            throw std::bad_alloc{};
        }
    }
    auto phase(RuntimeState& d, std::size_t i, PhaseKind kind) const -> KernelPhaseState& {
        auto& rows = d.projection.phases;
        auto found = std::find_if(rows.begin(), rows.end(), [&](const auto& row) {
            return row.requirement == graph().requirements[i].identity && row.phase == kind;
        });
        if (found == rows.end()) {
            throw std::logic_error{"phase not declared"};
        }
        return found->state;
    }
    auto pendingPhase(RuntimeState& d, std::size_t i, PhaseKind kind) const -> bool {
        return phase(d, i, kind) == KernelPhaseState::Pending;
    }
    auto inWindow(const RequirementRecord& r, PhaseKind kind, Tick tick) const -> bool {
        return std::any_of(
            r.timing->successWindows.begin(), r.timing->successWindows.end(),
            [&](const auto& w) { return w.phase.kind == kind && tick >= w.start && tick < w.end; });
    }
    auto target(const RequirementRecord& r, PhaseKind kind) const -> Tick {
        auto found = std::find_if(r.timing->phaseTargets.begin(), r.timing->phaseTargets.end(),
                                  [kind](const auto& t) { return t.phase == kind; });
        if (found == r.timing->phaseTargets.end()) {
            throw std::logic_error{"target missing"};
        }
        return found->chartTick;
    }
    void outcome(RuntimeState& d, std::vector<FactDescriptor>& facts, std::size_t i, PhaseKind kind,
                 Outcome outcome, const OriginId& origin,
                 std::optional<CanonicalObservationKey> evidence = {},
                 bool observedError = false) const {
        auto& value = phase(d, i, kind);
        if (value != KernelPhaseState::Pending) {
            return;
        }
        value = outcome == Outcome::hit ? KernelPhaseState::Hit : KernelPhaseState::Miss;
        std::optional<TickDelta> delta;
        if (observedError && evidence) {
            auto checked = differenceTicks(evidence->observationTick.tick(),
                                           target(graph().requirements[i], kind));
            if (!checked) {
                throw std::overflow_error{"phase error overflow"};
            }
            delta = *checked;
        }
        facts.push_back({origin, graph().requirements[i].identity, kind, FactKind::phaseOutcome,
                         outcome, delta, std::move(evidence)});
    }
    void endOwnership(RuntimeState& d, std::size_t i) const {
        const auto& r = graph().requirements[i];
        auto& owned = d.projection.ownership;
        auto found = std::find_if(owned.begin(), owned.end(),
                                  [&](const auto& o) { return o.requirement == r.identity; });
        if (found == owned.end()) {
            return;
        }
        if (found->lease) {
            auto resource = std::find_if(
                d.projection.resources.begin(), d.projection.resources.end(),
                [&](const auto& v) { return v.resourceId == found->lease->slot.resourceId; });
            auto declaration = std::find_if(
                graph().resources.begin(), graph().resources.end(),
                [&](const auto& v) { return v.ref.resourceId == resource->resourceId; });
            resource->state = declaration->terminalAfterTermination ? ResourceState{Terminal{}}
                                                                    : ResourceState{Free{}};
        }
        owned.erase(found);
    }
    void failPending(RuntimeState& d, std::vector<FactDescriptor>& facts, std::size_t i,
                     const OriginId& origin) const {
        for (const auto& p : graph().requirements[i].phases) {
            outcome(d, facts, i, p.kind, Outcome::miss, origin);
        }
        endOwnership(d, i);
    }
    auto available(RuntimeState& d, std::size_t i, const std::optional<ContactHandle>& contact,
                   bool acquiring = true) const -> bool {
        const auto& r = graph().requirements[i];
        if (!r.resourceClaims.empty()) {
            auto resource = std::find_if(
                d.projection.resources.begin(), d.projection.resources.end(), [&](const auto& v) {
                    return v.resourceId == r.resourceClaims.front().resourceRef.resourceId;
                });
            if (resource == d.projection.resources.end() ||
                !std::holds_alternative<Free>(resource->state)) {
                return false;
            }
        }
        if (rootPhase(r) == PhaseKind::head && acquiring) {
            if (!contact) {
                return false;
            }
            if (std::any_of(d.projection.ownership.begin(), d.projection.ownership.end(),
                            [&](const auto& o) { return o.contact == *contact; })) {
                return false;
            }
        }
        return true;
    }
    void success(RuntimeState& d, std::vector<FactDescriptor>& facts, std::size_t i,
                 const OriginId& origin, Tick tick, std::optional<CanonicalObservationKey> evidence,
                 std::optional<ContactHandle> contact) const {
        const auto& r = graph().requirements[i];
        const auto kind = rootPhase(r);
        outcome(d, facts, i, kind, Outcome::hit, origin, evidence, evidence.has_value());
        if (kind == PhaseKind::tap) {
            if (!r.resourceClaims.empty()) {
                auto resource = std::find_if(
                    d.projection.resources.begin(), d.projection.resources.end(),
                    [&](const auto& v) {
                        return v.resourceId == r.resourceClaims.front().resourceRef.resourceId;
                    });
                auto declaration = std::find_if(
                    graph().resources.begin(), graph().resources.end(),
                    [&](const auto& v) { return v.ref.resourceId == resource->resourceId; });
                if (declaration->terminalAfterTermination) {
                    resource->state = Terminal{};
                }
            }
            return;
        }
        std::optional<LeaseIdentity> lease;
        if (!r.resourceClaims.empty()) {
            const auto& claim = r.resourceClaims.front();
            auto declaration =
                std::find_if(graph().resources.begin(), graph().resources.end(),
                             [&](const auto& v) { return v.ref == claim.resourceRef; });
            lease = LeaseIdentity{
                {claim.resourceRef.resourceId, declaration->slotToken, 0},
                r.identity,
                {r.identity, kind, origin, claim.resourceRef.resourceId, claim.intent}};
            auto resource = std::find_if(
                d.projection.resources.begin(), d.projection.resources.end(),
                [&](const auto& v) { return v.resourceId == claim.resourceRef.resourceId; });
            resource->state = Held{r.identity, *lease, *contact};
        }
        d.projection.ownership.push_back({r.identity, *contact, tick, lease});
        d.coverageHistory.push_back({r.identity, *contact, tick, lease});
    }

    void timer(RuntimeState& d, std::vector<FactDescriptor>& facts, const PreparedTimerKey& timer,
               std::span<const AdmittedObservation> inputs) const {
        auto ri = std::find_if(graph().requirements.begin(), graph().requirements.end(),
                               [&](const auto& r) { return r.identity == timer.requirement; });
        const auto i = static_cast<std::size_t>(ri - graph().requirements.begin());
        const OriginId origin = TimerOrigin{timer.tick, ri->identity};
        if (timer.kind == PreparedTimerKind::activation) {
            d.activeRequirements.push_back(i);
            std::sort(d.activeRequirements.begin(), d.activeRequirements.end());
            for (const auto& p : ri->phases) {
                if (phase(d, i, p.kind) == KernelPhaseState::Dormant) {
                    phase(d, i, p.kind) = KernelPhaseState::Pending;
                }
            }
        }
        if (timer.kind == PreparedTimerKind::bodyEnd && pendingPhase(d, i, PhaseKind::body)) {
            auto owned = std::find_if(d.projection.ownership.begin(), d.projection.ownership.end(),
                                      [&](const auto& o) { return o.requirement == ri->identity; });
            const bool interrupted =
                owned != d.projection.ownership.end() &&
                std::any_of(inputs.begin(), inputs.end(), [&](const auto& input) {
                    return input.observationKey.action == InputAction::release &&
                           sameChannel(owned->contact, input.observationKey) &&
                           input.observationKey.observationTick.tick() < ri->timing->body->end;
                });
            if (owned == d.projection.ownership.end() || interrupted ||
                owned->acquiredTick > ri->timing->body->start) {
                failPending(d, facts, i, origin);
            } else {
                outcome(d, facts, i, PhaseKind::body, Outcome::hit, origin);
                if (std::none_of(ri->phases.begin(), ri->phases.end(),
                                 [](const auto& p) { return p.kind == PhaseKind::tail; })) {
                    endOwnership(d, i);
                }
            }
        }
        if (timer.kind == PreparedTimerKind::phaseClose && pendingPhase(d, i, timer.phase)) {
            Tick last = timer.tick;
            for (const auto& w : ri->timing->successWindows) {
                if (w.phase.kind != timer.phase) {
                    continue;
                }
                auto end = w.end;
                if (timer.phase == PhaseKind::head) {
                    end = std::min(end, *offsetTicks(ri->timing->body->start, TickSpan{1}));
                }
                last = std::max(last, end);
            }
            if (timer.tick == last) {
                if (observer(*ri)) {
                    phase(d, i, timer.phase) = KernelPhaseState::Expired;
                } else if (timer.phase == PhaseKind::head) {
                    failPending(d, facts, i, origin);
                } else {
                    outcome(d, facts, i, timer.phase, Outcome::miss, origin);
                    endOwnership(d, i);
                }
            }
        }
        if (timer.kind == PreparedTimerKind::hardDeadline) {
            if (observer(*ri)) {
                if (pendingPhase(d, i, rootPhase(*ri))) {
                    phase(d, i, rootPhase(*ri)) = KernelPhaseState::Expired;
                }
            } else {
                failPending(d, facts, i, origin);
            }
        }
        if ((timer.kind == PreparedTimerKind::activation ||
             timer.kind == PreparedTimerKind::windowOpen) &&
            timer.phase == rootPhase(*ri) && pendingPhase(d, i, timer.phase) &&
            inWindow(*ri, timer.phase, timer.tick) && !d.matchers[i].epsilonAttempted) {
            auto& matcher = d.matchers[i];
            const auto& program = *prepared.requirements()[i].executionProgram;
            if (program.accepting[program.start]) {
                matcher.epsilonAttempted = true;
                if (observer(*ri)) {
                    phase(d, i, timer.phase) = KernelPhaseState::Observed;
                    d.projection.observers.push_back(
                        {ri->identity, origin, timer.tick, {}, false, true});
                } else if (available(d, i, {})) {
                    success(d, facts, i, origin, timer.tick, {}, {});
                }
            }
        }
    }

    void input(RuntimeState& d, std::vector<FactDescriptor>& facts,
               const AdmittedObservation& input) const {
        const auto& key = input.observationKey;
        const auto tick = input.dispatchTick;
        const OriginId origin = ObservationOrigin{tick, key};
        auto& contacts = d.projection.contacts;
        auto existing = std::find_if(contacts.begin(), contacts.end(),
                                     [&](const auto& c) { return sameChannel(c, key); });
        const bool newPress = key.action == InputAction::press && existing == contacts.end();
        if (newPress) {
            contacts.push_back({key.domainToken, key.sourceClass, key.channelToken, key});
            existing = std::prev(contacts.end());
        }
        std::optional<ContactHandle> contact =
            existing == contacts.end() ? std::nullopt : std::optional{*existing};
        InputReceipt receipt{key, 0, {}, tick};
        std::optional<std::size_t> releaseOwner;
        if (key.action == InputAction::release && contact) {
            auto owned = std::find_if(d.projection.ownership.begin(), d.projection.ownership.end(),
                                      [&](const auto& o) { return o.contact == *contact; });
            if (owned != d.projection.ownership.end()) {
                auto ri =
                    std::find_if(graph().requirements.begin(), graph().requirements.end(),
                                 [&](const auto& r) { return r.identity == owned->requirement; });
                releaseOwner = static_cast<std::size_t>(ri - graph().requirements.begin());
                ++receipt.consideredCount;
                receipt.consumedBy = ri->identity;
                if (pendingPhase(d, *releaseOwner, PhaseKind::body)) {
                    outcome(d, facts, *releaseOwner, PhaseKind::body, Outcome::miss, origin, key,
                            true);
                    for (const auto& p : ri->phases) {
                        outcome(d, facts, *releaseOwner, p.kind, Outcome::miss, origin);
                    }
                } else {
                    const bool matched =
                        inWindow(*ri, PhaseKind::tail, key.observationTick.tick()) &&
                        inWindow(*ri, PhaseKind::tail, tick) &&
                        tick < prepared.requirements()[*releaseOwner].deadline &&
                        std::any_of(
                            ri->atomBindings.begin(), ri->atomBindings.end(),
                            [&](const auto& b) { return matches(b, key); });
                    outcome(d, facts, *releaseOwner, PhaseKind::tail,
                            matched ? Outcome::hit : Outcome::miss, origin, key, true);
                }
                endOwnership(d, *releaseOwner);
            }
        }
        struct Candidate {
            std::size_t index;
            std::vector<std::size_t> next;
            bool accepted;
        };
        std::vector<Candidate> candidates;
        for (auto i : d.activeRequirements) {
            const auto& r = graph().requirements[i];
            const auto root = rootPhase(r);
            if (releaseOwner == i || !pendingPhase(d, i, root) || !inWindow(r, root, tick) ||
                !inWindow(r, root, key.observationTick.tick()) ||
                (root == PhaseKind::head && tick > r.timing->body->start)) {
                continue;
            }
            const auto& program = *prepared.requirements()[i].executionProgram;
            std::vector<std::size_t> atoms;
            for (const auto& b : r.atomBindings) {
                if (b.tailOnly || !matches(b, key)) {
                    continue;
                }
                auto atom = std::find(program.atomRefs.begin(), program.atomRefs.end(), b.atomRef);
                if (atom != program.atomRefs.end()) {
                    atoms.push_back(static_cast<std::size_t>(atom - program.atomRefs.begin()));
                }
            }
            if (atoms.empty()) {
                continue;
            }
            ++receipt.consideredCount;
            std::vector<std::size_t> next;
            for (auto state : d.matchers[i].states) {
                for (auto atom : atoms) {
                    auto value = program.transitions[state * program.atomRefs.size() + atom];
                    if (value && program.live[*value]) {
                        next.push_back(*value);
                    }
                }
            }
            std::sort(next.begin(), next.end());
            next.erase(std::unique(next.begin(), next.end()), next.end());
            const bool accepted = std::any_of(next.begin(), next.end(),
                                              [&](auto s) { return program.accepting[s] != 0; });
            if (observer(r)) {
                d.projection.observers.push_back(
                    {r.identity, origin, tick, key, !next.empty(), accepted});
                if (!next.empty()) {
                    d.matchers[i].states = next;
                }
                if (accepted) {
                    phase(d, i, root) = KernelPhaseState::Observed;
                }
            } else if (!next.empty() && (root != PhaseKind::head || !accepted || newPress)) {
                candidates.push_back({i, std::move(next), accepted});
            }
        }
        const auto competition = [&](std::size_t i) {
            const auto& r = graph().requirements[i];
            return r.resourceClaims.empty()
                       ? std::pair{std::string{"@independent"}, *r.independentCompetition}
                       : std::pair{r.resourceClaims.front().resourceRef.resourceId,
                                   *r.resourceClaims.front().claimPolicy.competition};
        };
        std::sort(candidates.begin(), candidates.end(), [&](const auto& a, const auto& b) {
            return competition(a.index) < competition(b.index);
        });
        if (!receipt.consumedBy) {
            for (auto& candidate : candidates) {
                if (!available(d, candidate.index, contact, candidate.accepted)) {
                    continue;
                }
                d.matchers[candidate.index].states = std::move(candidate.next);
                receipt.consumedBy = graph().requirements[candidate.index].identity;
                if (candidate.accepted) {
                    success(d, facts, candidate.index, origin, tick, key, contact);
                }
                break;
            }
        }
        if (key.action == InputAction::release && contact) {
            contacts.erase(std::remove(contacts.begin(), contacts.end(), *contact), contacts.end());
        }
        if (!receipt.consumedBy) {
            facts.push_back({CoordinationOrigin{tick, key},
                             {},
                             {},
                             receipt.consideredCount ? FactKind::consumeEmpty : FactKind::stray,
                             Outcome::miss,
                             {},
                             key});
        }
        d.projection.receipts.push_back(std::move(receipt));
    }

    void validateDraft(const RuntimeState& d) const {
        std::set<std::pair<RequirementIdentity, PhaseKind>> phases;
        for (const auto& p : d.projection.phases) {
            if (!phases.emplace(p.requirement, p.phase).second) {
                throw std::logic_error{"duplicate phase draft"};
            }
        }
        std::set<std::tuple<std::string, std::string, std::string>> channels;
        for (const auto& c : d.projection.contacts) {
            if (!channels.emplace(c.domainToken, c.sourceClass, c.channelToken).second) {
                throw std::logic_error{"duplicate physical contact"};
            }
        }
        for (std::size_t i = 0; i < d.projection.ownership.size(); ++i) {
            const auto& o = d.projection.ownership[i];
            if (std::find(d.projection.contacts.begin(), d.projection.contacts.end(), o.contact) ==
                d.projection.contacts.end()) {
                throw std::logic_error{"ownership without physical contact"};
            }
            for (std::size_t j = 0; j < i; ++j) {
                if (d.projection.ownership[j].contact == o.contact ||
                    d.projection.ownership[j].requirement == o.requirement) {
                    throw std::logic_error{"duplicate ownership"};
                }
            }
            if (o.lease) {
                const auto r = std::find_if(
                    d.projection.resources.begin(), d.projection.resources.end(),
                    [&](const auto& r) { return r.resourceId == o.lease->slot.resourceId; });
                if (r == d.projection.resources.end()) {
                    throw std::logic_error{"missing owned resource"};
                }
                const auto* held = std::get_if<Held>(&r->state);
                if (!held || held->lease != *o.lease || held->contact != o.contact) {
                    throw std::logic_error{"lease state mismatch"};
                }
            }
        }
        for (const auto& r : d.projection.resources) {
            if (const auto* held = std::get_if<Held>(&r.state)) {
                if (std::count_if(
                        d.projection.ownership.begin(), d.projection.ownership.end(),
                        [&](const auto& o) { return o.lease && *o.lease == held->lease; }) != 1) {
                    throw std::logic_error{"unowned held resource"};
                }
            }
        }
    }
    void fold(RuntimeState& d, Tick tick) const {
        if (!configuration.ruleset) {
            return;
        }
        auto& v = *d.projection.fold;
        const auto& config = configuration.ruleset->declaration();
        while (v.hookConsumerCursor < v.produced.size() &&
               v.produced[static_cast<std::size_t>(v.hookConsumerCursor)].visibleTick <= tick) {
            v.bonus = v.produced[static_cast<std::size_t>(v.hookConsumerCursor++)].value;
        }
        const auto increment = [](std::uint64_t& x) {
            if (x == UINT64_MAX) {
                throw std::overflow_error{"Fold count overflow"};
            }
            ++x;
        };
        bool phaseFact = false;
        std::uint64_t combined = 0;
        std::vector<RegisterContribution> hookWrites;
        for (std::size_t i = static_cast<std::size_t>(v.factCursor); i < d.projection.facts.size();
             ++i) {
            const auto* f = std::get_if<PhaseOutcomeFact>(&d.projection.facts[i]);
            if (!f) {
                const auto& receipt = std::get<ReceiptFact>(d.projection.facts[i]);
                increment(receipt.kind == FactKind::stray ? v.strayCount : v.consumeEmptyCount);
                continue;
            }
            phaseFact = true;
            const auto rule = std::find_if(
                config.score.rules.begin(), config.score.rules.end(), [&](const auto& r) {
                    return r.phase == f->phase && r.outcome == f->outcome && r.grade == f->grade;
                });
            if (rule == config.score.rules.end()) {
                throw std::logic_error{"score mapping missing"};
            }
            const auto count = std::find_if(v.counts.begin(), v.counts.end(), [&](const auto& row) {
                return row.phase == f->phase && row.outcome == f->outcome && row.grade == f->grade;
            });
            if (count == v.counts.end()) {
                throw std::logic_error{"statistics mapping missing"};
            }
            increment(count->count);
            auto score = checkedScore(v.score, rule->delta, config.score);
            if (!score) {
                throw std::overflow_error{"score checked failure"};
            }
            v.score = *score;
            if (f->outcome == Outcome::hit && v.bonus) {
                const auto available =
                    static_cast<std::uint64_t>(INT64_MAX) - static_cast<std::uint64_t>(v.score);
                if (v.bonus > available) {
                    if (config.score.arithmetic != ArithmeticPolicy::clamp) {
                        throw std::overflow_error{"bonus score overflow"};
                    }
                    v.score = config.score.maximum;
                } else {
                    const auto sum =
                        std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(v.score) + v.bonus);
                    score = checkedScore(sum, 0, config.score);
                    if (!score) {
                        throw std::overflow_error{"bonus checked failure"};
                    }
                    v.score = *score;
                }
            }
            if (rule->incrementsCombo) {
                increment(v.combo);
            } else {
                v.combo = 0;
            }
            v.maxCombo = std::max(v.maxCombo, v.combo);
            if (f->outcome == Outcome::hit) {
                increment(v.hits);
            } else {
                increment(v.misses);
            }
            for (const auto& h : config.hooks) {
                hookWrites.push_back({h.target, "hook", f->factId.commitId, f->factId.localOrdinal,
                                      0, RegisterValue{h.value}});
            }
        }
        const auto fault = testControls && testControls->invalidRegisterTick == tick
                               ? testControls->registerFault
                               : RegisterFault::none;
        std::vector<RegisterContribution> scoreWrites{
            {"score", "score", 0, 0, 0, RegisterValue{v.score}}};
        if (fault == RegisterFault::secondExclusive) {
            scoreWrites.push_back(scoreWrites.front());
        }
        const RegisterDeclaration scoreRegister{
            "score", RegisterKind::exclusive, RegisterValueTag::signed64, "score",
            {},      CombineOperator::maximum};
        auto scoreCommit = commitRegister(scoreRegister, scoreWrites, RegisterValue{v.score});
        if (!scoreCommit) {
            throw std::logic_error{"exclusive score write invalid"};
        }
        if (!config.hooks.empty()) {
            if (fault == RegisterFault::duplicateContribution && !hookWrites.empty()) {
                hookWrites.push_back(hookWrites.front());
            }
            const auto& h = config.hooks.front();
            const RegisterDeclaration hookRegister{h.target,
                                                   RegisterKind::commutativeMonoid,
                                                   RegisterValueTag::unsigned64,
                                                   "",
                                                   h.contributors,
                                                   h.combine};
            auto hookCommit =
                commitRegister(hookRegister, hookWrites, RegisterValue{std::uint64_t{0}});
            if (!hookCommit) {
                throw std::logic_error{"monoid Hook contribution invalid"};
            }
            combined = std::get<std::uint64_t>(*hookCommit);
        }
        const RegisterDeclaration ledgerRegister{
            "fact.count", RegisterKind::ledgerDerived, RegisterValueTag::unsigned64, "",
            {},           CombineOperator::maximum};
        std::vector<RegisterContribution> direct;
        if (fault == RegisterFault::directDerived) {
            direct.push_back(
                {"fact.count", "statistics", 0, 0, 0, RegisterValue{std::uint64_t{0}}});
        }
        auto derived =
            commitRegister(ledgerRegister, direct,
                           RegisterValue{static_cast<std::uint64_t>(d.projection.facts.size())});
        if (!derived) {
            throw std::logic_error{"ledger-derived direct write invalid"};
        }
        v.factCursor = static_cast<std::uint64_t>(d.projection.facts.size());
        v.ledgerDerivedCount = v.factCursor;
        v.workTick = tick;
        if (phaseFact && !config.hooks.empty()) {
            const auto visible = offsetTicks(tick, TickSpan{1});
            if (!visible) {
                throw std::overflow_error{"Hook visibility overflow"};
            }
            v.monoidValue = config.hooks.front().combine == CombineOperator::maximum
                                ? std::max(v.monoidValue, combined)
                                : v.monoidValue | combined;
            v.produced.push_back({config.hooks.front().target, *visible, combined});
        }
    }
    void ledger(RuntimeState& d, std::vector<FactDescriptor>& facts, Tick tick) const {
        const auto rank = [](const auto& v) {
            return v.phase ? static_cast<unsigned>(*v.phase) + 1 : 0U;
        };
        std::sort(facts.begin(), facts.end(), [&](const auto& a, const auto& b) {
            return std::tuple{a.origin, rank(a), a.kind, a.requirement} <
                   std::tuple{b.origin, rank(b), b.kind, b.requirement};
        });
        std::optional<OriginId> previous;
        std::uint64_t commit = 0, local = 0;
        for (const auto& f : facts) {
            if (!previous || *previous != f.origin) {
                if (d.idsExhausted) {
                    throw std::overflow_error{"commit ID exhausted"};
                }
                commit = d.nextCommitId;
                if (commit == UINT64_MAX) {
                    d.idsExhausted = true;
                } else {
                    ++d.nextCommitId;
                }
                previous = f.origin;
                local = 0;
            }
            LogicalCanonicalOrdinal ordinal{f.origin, 0, local, static_cast<std::uint8_t>(rank(f)),
                                            f.kind};
            if (f.phase) {
                std::optional<std::string> grade;
                if (f.error) {
                    const auto r =
                        std::find_if(graph().requirements.begin(), graph().requirements.end(),
                                     [&](const auto& r) { return r.identity == *f.requirement; });
                    const auto c =
                        std::find_if(r->measure.components.begin(), r->measure.components.end(),
                                     [&](const auto& c) { return c.phase == *f.phase; });
                    if (c != r->measure.components.end() && c->gradeTable) {
                        const auto row = std::find_if(c->gradeTable->begin(), c->gradeTable->end(),
                                                      [&](const auto& row) {
                                                          return f.error->value() >= row.minimum &&
                                                                 f.error->value() <= row.maximum;
                                                      });
                        if (row == c->gradeTable->end()) {
                            throw std::logic_error{"grade outside compiled table"};
                        }
                        grade = row->grade;
                    }
                }
                d.projection.facts.emplace_back(PhaseOutcomeFact{*f.requirement,
                                                                 *f.phase,
                                                                 f.outcome,
                                                                 factCategoryOfPhase(*f.phase),
                                                                 f.error,
                                                                 f.evidence,
                                                                 f.origin,
                                                                 commit,
                                                                 {commit, local},
                                                                 tick,
                                                                 ordinal,
                                                                 std::move(grade)});
            } else {
                d.projection.facts.emplace_back(ReceiptFact{
                    f.kind, *f.evidence, f.origin, commit, {commit, local}, tick, ordinal});
            }
            if (local == UINT64_MAX) {
                throw std::overflow_error{"local ordinal exhausted"};
            }
            ++local;
        }
    }
};

ExecutionKernel::ExecutionKernel(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
ExecutionKernel::~ExecutionKernel() = default;

auto ExecutionKernel::prepare(const SessionConfiguration& c, const PreparedGameplay& prepared)
    -> core::Result<std::unique_ptr<ExecutionKernel>> try {
    if (auto valid = validateSessionConfiguration(c); !valid) {
        return core::unexpected(valid.error());
    }
    const auto& g = prepared.assembled().graph;
    if (g.executionProfile != c.executionProfileToken ||
        prepared.identityDeclarations() != c.identityDeclarations ||
        !sameTimebase(c.timebase.view(), *g.timebase) || c.latePolicy != *g.latePolicy) {
        return core::unexpected(
            error("execution.relation_invalid", "configuration and prepared declarations differ"));
    }
    if (c.ruleset) {
        auto registry = prepareRuleset(c.ruleset->declaration());
        if (!registry) {
            return core::unexpected(registry.error());
        }
    }
    const auto mapping = c.inputMapping.view();
    if (c.ruleset) {
        const auto timebase = c.timebase.view();
        const auto& engine = c.identityDeclarations.engine;
        const auto& session = c.identityDeclarations.session;
        for (const auto token :
             {mapping.profileId, mapping.profileVersion, mapping.sourceClass.token(),
              timebase.profileId, timebase.unitToken, std::string_view{c.calibrationIdentityToken},
              std::string_view{engine.judgementSemanticRevision},
              std::string_view{engine.factSemanticRevision},
              std::string_view{engine.fixedPointTableId},
              std::string_view{engine.coordinationPhaseOrderToken},
              std::string_view{session.defaultGraceSourceToken},
              std::string_view{session.normalizationProfileToken},
              std::string_view{session.judgementConfigToken}}) {
            if (!validUtf8(token)) {
                return core::unexpected(
                    error("execution.relation_invalid", "invalid UTF-8 profile metadata"));
            }
        }
        for (const auto& domain : mapping.domains) {
            if (!validUtf8(domain.domainToken)) {
                return core::unexpected(
                    error("execution.relation_invalid", "invalid UTF-8 mapping domain"));
            }
        }
    }
    auto actual = c.identityDeclarations;
    if (c.ruleset) {
        const auto token = rulesetIdentityToken(*c.ruleset);
        actual.engine.judgementSemanticRevision += ";gameplay.fold.finite.v1";

        actual.ruleset.interfaceProjectionToken = token;
        actual.ruleset.buildHash = c.ruleset->declaration().compiledBuild;
        actual.ruleset.moduleOrder.clear();
        for (const auto& m : c.ruleset->declaration().modules) {
            actual.ruleset.moduleOrder.push_back(m.id);
        }
        actual.session.loadoutToken = c.ruleset->declaration().loadoutId;
    }
    const bool hasGradeTables =
        std::any_of(g.requirements.begin(), g.requirements.end(), [](const auto& r) {
            return std::any_of(r.measure.components.begin(), r.measure.components.end(),
                               [](const auto& c) { return c.gradeTable.has_value(); });
        });
    if (hasGradeTables) {
        actual.engine.factSemanticRevision += ";grade.signed-interval.v1";
    }
    std::vector<MatcherState> matchers;
    KernelProjection projection{.phases = {},
                                .resources = {},
                                .contacts = {},
                                .ownership = {},
                                .observers = {},
                                .receipts = {},
                                .facts = {},
                                .processedFrontier = {},
                                .lastAdvanceHorizon = {},
                                .state = KernelSessionState::Prepared,
                                .faultDiagnostic = {},
                                .judgementIdentity = makeRuntimePreparedIdentity(
                                    g, actual, mapping, c.latePolicy, c.calibrationIdentityToken),
                                .runScope = {},
                                .failedTick = {},
                                .requestedHorizon = {},
                                .kernelWorkTick = {},
                                .sealedFactCursor = 0,
                                .fold = {},
                                .effectiveFactSemanticRevision = {},
                                .faultStage = {}};
    projection.effectiveFactSemanticRevision = actual.engine.factSemanticRevision;
    projection.runScope = std::make_shared<const std::uint8_t>(0);
    if (c.ruleset) {
        auto initial = c.ruleset->initialState();
        if (!initial) {
            return core::unexpected(initial.error());
        }
        projection.fold = std::move(*initial);
    }

    for (std::size_t i = 0; i < g.requirements.size(); ++i) {
        const auto& r = g.requirements[i];
        if (!prepared.requirements()[i].executionProgram) {
            return core::unexpected(
                error("execution.profile_incomplete", "prepared executable program missing"));
        }
        if (c.ruleset) {
            const auto& id = r.identity;
            if (!validUtf8(id.chartEntryId) || !validUtf8(id.invocationId) ||
                !validUtf8(id.moduleId) || !validUtf8(id.exportId) ||
                !validUtf8(id.requirementLocalId)) {
                return core::unexpected(
                    error("execution.relation_invalid", "invalid UTF-8 RequirementIdentity"));
            }
            for (const auto& step : id.emissionPath) {
                if (!validUtf8(step.nodeId)) {
                    return core::unexpected(
                        error("execution.relation_invalid", "invalid UTF-8 emission identity"));
                }
            }
            for (const auto& component : r.measure.components) {
                for (const auto& token : component.declaredGradeTokens) {
                    if (!validUtf8(token)) {
                        return core::unexpected(
                            error("execution.relation_invalid", "invalid UTF-8 grade token"));
                    }
                }
            }
        }
        for (const auto& b : r.atomBindings) {
            if (c.ruleset && (!validUtf8(b.domainToken) || !validUtf8(b.sourceClass) ||
                              !validUtf8(b.channelToken))) {
                return core::unexpected(
                    error("execution.relation_invalid", "invalid UTF-8 binding"));
            }
            auto domain =
                std::find_if(mapping.domains.begin(), mapping.domains.end(),
                             [&](const auto& d) { return d.domainToken == b.domainToken; });
            if (domain == mapping.domains.end() || b.sourceClass != mapping.sourceClass.token() ||
                (b.amountRange &&
                 (b.amountRange->minimum < domain->amount.minimum ||
                  b.amountRange->maximum > domain->amount.maximum ||
                  (domain->amount.boundaryPolicy == AmountBoundaryPolicy::exclusive &&
                   (b.amountRange->minimum == domain->amount.minimum ||
                    b.amountRange->maximum == domain->amount.maximum))))) {
                return core::unexpected(error("execution.relation_invalid",
                                              "atom binding is incompatible with mapping"));
            }
        }
        for (const auto& component : r.measure.components) {
            if (component.gradeTable) {
                const auto& table = *component.gradeTable;
                if (table.empty()) {
                    return core::unexpected(
                        error("execution.relation_invalid", "empty declared grade table"));
                }
                for (std::size_t n = 0; n < table.size(); ++n) {
                    const auto& row = table[n];
                    if (row.minimum > row.maximum || row.grade.empty() || !validUtf8(row.grade) ||
                        std::find(component.declaredGradeTokens.begin(),
                                  component.declaredGradeTokens.end(),
                                  row.grade) == component.declaredGradeTokens.end() ||
                        (n && (table[n - 1].maximum == INT64_MAX ||
                               row.minimum != table[n - 1].maximum + 1))) {
                        return core::unexpected(
                            error("execution.relation_invalid",
                                  "grade table gap, overlap or undeclared grade"));
                    }
                }
                const auto target =
                    std::find_if(r.timing->phaseTargets.begin(), r.timing->phaseTargets.end(),
                                 [&](const auto& t) { return t.phase == component.phase; });
                std::vector<std::pair<Tick, Tick>> domains;
                if (component.phase == PhaseKind::body) {
                    std::optional<Tick> first;
                    for (const auto& w : r.timing->successWindows) {
                        if (w.phase.kind == PhaseKind::head && (!first || w.start < *first)) {
                            first = w.start;
                        }
                    }
                    if (first && r.timing->body) {
                        auto low = offsetTicks(*first, TickSpan{1});
                        if (low && *low < r.timing->body->end) {
                            domains.emplace_back(*low, Tick{r.timing->body->end.value() - 1});
                        }
                    }
                } else if (component.phase == PhaseKind::tail) {
                    const auto deadline = prepared.requirements()[i].deadline;
                    if (r.timing->body->end < deadline) {
                        domains.emplace_back(r.timing->body->end, Tick{deadline.value() - 1});
                    }
                } else {
                    for (const auto& w : r.timing->successWindows) {
                        if (w.phase.kind == component.phase) {
                            domains.emplace_back(w.start, Tick{w.end.value() - 1});
                        }
                    }
                }
                const auto minimum =
                    INT64_MIN + c.latePolicy.windowOpenThreshold.measuredValue()->value();
                const auto maximum =
                    INT64_MAX - c.latePolicy.finalizationWatermark.measuredValue()->value();
                for (auto [lower, upper] : domains) {
                    lower = std::max(lower, Tick{minimum});
                    upper = std::min(upper, Tick{maximum});
                    if (lower > upper) {
                        continue;
                    }
                    auto low = differenceTicks(lower, target->chartTick);
                    auto high = differenceTicks(upper, target->chartTick);
                    if (!low || !high || table.front().minimum > low->value() ||
                        table.back().maximum < high->value()) {
                        return core::unexpected(
                            error("execution.relation_invalid",
                                  "grade table does not cover reachable phase errors"));
                    }
                }
            }
            if (c.ruleset) {
                std::vector<std::optional<std::string>> grades{std::nullopt};
                if (component.gradeTable) {
                    for (const auto& row : *component.gradeTable) {
                        grades.push_back(row.grade);
                    }
                }
                for (const auto outcome : kStage7AOutcomes) {
                    for (const auto& grade : grades) {
                        if (grade &&
                            ((component.phase == PhaseKind::body && outcome == Outcome::hit) ||
                             (component.phase != PhaseKind::body &&
                              component.phase != PhaseKind::tail && outcome == Outcome::miss))) {
                            continue;
                        }
                        const auto& rules = c.ruleset->declaration().score.rules;
                        if (std::none_of(rules.begin(), rules.end(), [&](const auto& rule) {
                                return rule.phase == component.phase && rule.outcome == outcome &&
                                       rule.grade == grade;
                            })) {
                            return core::unexpected(
                                error("execution.relation_invalid",
                                      "required scoring configuration missing"));
                        }
                    }
                }
            }
        }
        for (const auto& p : r.phases) {
            if (c.ruleset) {
                const auto& rules = c.ruleset->declaration().score.rules;
                for (const auto outcome : kStage7AOutcomes) {
                    if (std::none_of(rules.begin(), rules.end(), [&](const auto& rule) {
                            return rule.phase == p.kind && rule.outcome == outcome && !rule.grade;
                        })) {
                        return core::unexpected(error("execution.relation_invalid",
                                                      "phase scoring configuration incomplete"));
                    }
                }
            }
            projection.phases.push_back({r.identity, p.kind, KernelPhaseState::Dormant});
        }
        matchers.push_back({{prepared.requirements()[i].executionProgram->start}, false});
    }
    for (const auto& r : g.resources) {
        if (c.ruleset && (!validUtf8(r.ref.resourceId) || !validUtf8(r.slotToken))) {
            return core::unexpected(
                error("execution.relation_invalid", "invalid UTF-8 resource identity"));
        }
        projection.resources.push_back({r.ref.resourceId, Free{}});
    }
    auto state = std::make_shared<const RuntimeState>(std::move(projection), std::move(matchers));
    auto impl = std::make_unique<Impl>(std::move(c), std::move(prepared), std::move(state));
    return std::unique_ptr<ExecutionKernel>{new ExecutionKernel{std::move(impl)}};
} catch (const std::exception&) {
    return core::unexpected(
        error("execution.relation_invalid", "kernel preparation allocation failed"));
}

auto ExecutionKernel::query() const noexcept -> std::shared_ptr<const ProjectionStorage> {
    return impl_->active;
}

auto ExecutionKernel::inject(KernelTestControls controls) -> core::Result<void> try {
    auto draft = std::make_shared<RuntimeState>(*impl_->active);
    if (controls.nextCommitId) {
        draft->nextCommitId = *controls.nextCommitId;
        draft->idsExhausted = false;
    }
    impl_->testControls = std::move(controls);
    impl_->active = std::move(draft);
    return {};
} catch (const std::exception&) {
    return core::unexpected(
        error("execution.relation_invalid", "checking fixture allocation failed"));
}
auto ExecutionKernel::visibleSignalCount(Tick tick) const noexcept -> std::size_t {
    return static_cast<std::size_t>(std::count_if(
        impl_->active->signalVisibility.begin(), impl_->active->signalVisibility.end(),
        [tick](Tick visible) { return visible <= tick; }));
}

auto ExecutionKernel::submit(std::vector<ClockedIngress> batch)
    -> core::Result<std::vector<InputReceiptPending>> {
    return submitBatch(batch);
}

auto ExecutionKernel::submitCanonical(std::vector<CanonicalInput> batch)
    -> core::Result<std::vector<InputReceiptPending>> try {
    std::vector<ClockedIngress> inputs;
    std::vector<std::optional<std::int64_t>> amounts;
    const auto mapping = impl_->configuration.inputMapping.view();
    for (const auto& c : batch) {
        if (c.key.sourceClass != mapping.sourceClass.token()) {
            return core::unexpected(
                error("execution.relation_invalid", "canonical source class mismatch"));
        }
        const auto channel = ChannelRef::fromToken(c.key.channelToken);
        if (!channel) {
            return core::unexpected(channel.error());
        }
        inputs.push_back({c.key.observationTick,
                          {c.sequence,
                           c.key.action,
                           *channel,
                           c.key.domainToken,
                           {},
                           {false, false, false},
                           ContinuityKind::discrete,
                           {}}});
        amounts.push_back(c.key.amount);
    }
    return submitBatch(inputs, amounts);
} catch (const std::exception&) {
    return core::unexpected(
        error("execution.relation_invalid", "canonical submission allocation failed"));
}
auto ExecutionKernel::submitBatch(std::span<const ClockedIngress> batch,
                                  std::span<const std::optional<std::int64_t>> amounts)
    -> core::Result<std::vector<InputReceiptPending>> try {
    if (impl_->active->projection.state != KernelSessionState::Prepared) {
        return core::unexpected(
            error("session.lifecycle_order", "submission requires a healthy prepared session"));
    }
    if (batch.empty()) {
        return std::vector<InputReceiptPending>{};
    }
    auto mapping = impl_->configuration.inputMapping.view();
    if (impl_->configuration.ruleset) {
        if (!validUtf8(mapping.profileId) || !validUtf8(mapping.profileVersion) ||
            !validUtf8(mapping.sourceClass.token())) {
            return core::unexpected(error("execution.relation_invalid", "invalid UTF-8 mapping"));
        }
        for (const auto& item : batch) {
            if (!validUtf8(item.declaration.domainToken) ||
                !validUtf8(item.declaration.channel.token())) {
                return core::unexpected(error("execution.relation_invalid", "invalid UTF-8 input"));
            }
        }
    }
    auto journal = prepareIngressBatch(impl_->ingress, mapping, batch, true, amounts);
    if (!journal) {
        return core::unexpected(journal.error());
    }
    std::vector<AdmittedObservation> entries;
    std::vector<InputReceiptPending> receipts;
    const auto& projection = impl_->active->projection;
    for (const auto& entry : journal->entries) {
        auto route = routeExecutionObservation(impl_->configuration.latePolicy,
                                               entry.observation.observationTick.tick(),
                                               projection.lastAdvanceHorizon);
        if (!route) {
            return core::unexpected(route.error());
        }
        auto key = keyOf(entry.observation);
        if (key.action == InputAction::release) {
            for (const auto& ownership : impl_->active->coverageHistory) {
                const auto requirement = std::find_if(
                    impl_->graph().requirements.begin(), impl_->graph().requirements.end(),
                    [&](const auto& r) { return r.identity == ownership.requirement; });
                const auto body = std::find_if(
                    projection.phases.begin(), projection.phases.end(), [&](const auto& p) {
                        return p.requirement == ownership.requirement && p.phase == PhaseKind::body;
                    });
                if (body != projection.phases.end() && body->state == KernelPhaseState::Hit &&
                    sameChannel(ownership.contact, key) &&
                    key.observationTick >= ownership.contact.startPressKey.observationTick &&
                    key.observationTick.tick() < requirement->timing->body->end) {
                    return core::unexpected(core::Error{"judgement.s7a4.late.rejected",
                                                        "release would rewrite sealed coverage"}
                                                .withContext("category", "late_policy_incomplete")
                                                .withContext("severity", "error")
                                                .withContext("faulted", "false"));
                }
            }
        }
        for (const auto& existing : impl_->pending) {
            if (existing.dispatchTick == route->dispatchTick) {
                return core::unexpected(
                    error("late.dispatch_collision", "two observations share a dispatch Tick"));
            }
        }
        for (const auto& existing : entries) {
            if (existing.dispatchTick == route->dispatchTick) {
                return core::unexpected(
                    error("late.dispatch_collision", "batch observations share a dispatch Tick"));
            }
        }
        entries.push_back({key, route->dispatchTick, route->wasForwarded,
                           projection.processedFrontier, projection.lastAdvanceHorizon});
        receipts.push_back({std::move(key), route->dispatchTick, route->wasForwarded,
                            projection.processedFrontier, projection.lastAdvanceHorizon});
    }
    if (entries.size() > impl_->pending.max_size() - impl_->pending.size()) {
        return core::unexpected(
            error("execution.relation_invalid", "submission storage is not representable"));
    }
    static_assert(std::is_nothrow_move_constructible_v<AdmittedObservation>);
    static_assert(std::is_nothrow_move_assignable_v<AdmittedObservation>);
    impl_->pending.reserve(impl_->pending.size() + entries.size());
    if (auto reserved = reserveIngressBatch(impl_->ingress, *journal); !reserved) {
        return core::unexpected(reserved.error());
    }
    if (impl_->testControls && impl_->testControls->failSubmitRecordReservation) {
        throw std::bad_alloc{};
    }
    auto recording = std::make_shared<ReplayData>(*impl_->recording);
    if (journal->entries.size() > UINT64_MAX - recording->eventCount) {
        return core::unexpected(error("execution.relation_invalid", "Replay event count overflow"));
    }
    ReplayRecord record{{}, receipts, {}, {}};
    record.batch.reserve(journal->entries.size());
    for (const auto& entry : journal->entries) {
        record.batch.push_back({keyOf(entry.observation), entry.observation.ingressSequence});
    }
    recording->eventCount += static_cast<std::uint64_t>(record.batch.size());
    recording->records.push_back(std::move(record));
    commitIngressBatch(impl_->ingress, std::move(*journal));
    for (auto& entry : entries) {
        impl_->pending.push_back(std::move(entry));
    }
    std::sort(impl_->pending.begin(), impl_->pending.end(),
              [](const auto& a, const auto& b) { return a.dispatchTick < b.dispatchTick; });
    impl_->recording = std::move(recording);
    impl_->seekSource.reset();
    return receipts;
} catch (const std::exception&) {
    return core::unexpected(
        error("execution.relation_invalid", "submission storage allocation failed"));
}

auto ExecutionKernel::advance(Tick horizon) -> core::Result<void> try {
    if (impl_->testControls && impl_->testControls->failRecordReservationHorizon == horizon) {
        throw std::bad_alloc{};
    }
    auto record = std::make_shared<ReplayData>(*impl_->recording);
    record->records.push_back({{}, {}, horizon, {}});
    const auto before = impl_->active;
    auto outcome = advanceRecorded(horizon);
    if (outcome || before != impl_->active) {
        record->records.back().result =
            std::shared_ptr<const KernelProjection>(impl_->active, &impl_->active->projection);
        impl_->recording = std::move(record);
        impl_->seekSource.reset();
    }
    return outcome;
} catch (const std::exception&) {
    return core::unexpected(
        error("execution.relation_invalid", "Replay control reservation failed"));
}

auto ExecutionKernel::advanceRecorded(Tick horizon) -> core::Result<void> {
    if (impl_->active->projection.state != KernelSessionState::Prepared ||
        (impl_->active->projection.lastAdvanceHorizon &&
         horizon < *impl_->active->projection.lastAdvanceHorizon)) {
        return core::unexpected(
            error("session.lifecycle_order", "advance requires a monotonic healthy session"));
    }
    auto frontier = offsetTicks(
        horizon,
        TickSpan{-impl_->configuration.latePolicy.windowCloseThreshold.measuredValue()->value()});
    if (!frontier) {
        return core::unexpected(frontier.error());
    }
    if (impl_->active->projection.lastAdvanceHorizon == horizon) {
        return {};
    }
    std::optional<Tick> failedTick;
    bool foldFailed = false;
    try {
        const auto timers = impl_->prepared.timers();
        while (true) {
            std::optional<Tick> tick;
            auto cursor = impl_->active->timerCursor;
            if (cursor < timers.size() && timers[cursor].tick <= *frontier) {
                tick = timers[cursor].tick;
            }
            for (const auto& input : impl_->pending) {
                if (input.dispatchTick <= *frontier &&
                    (!impl_->active->projection.processedFrontier ||
                     input.dispatchTick > *impl_->active->projection.processedFrontier) &&
                    (!tick || input.dispatchTick < *tick)) {
                    tick = input.dispatchTick;
                }
            }
            if (impl_->active->projection.fold) {
                const auto& fold = *impl_->active->projection.fold;
                if (fold.hookConsumerCursor < fold.produced.size()) {
                    const auto due =
                        fold.produced[static_cast<std::size_t>(fold.hookConsumerCursor)]
                            .visibleTick;
                    if (due <= *frontier && (!tick || due < *tick)) {
                        tick = due;
                    }
                }
            }
            if (impl_->testControls) {
                for (const auto signal : impl_->testControls->signalTicks) {
                    if (signal <= *frontier &&
                        (!impl_->active->projection.processedFrontier ||
                         signal > *impl_->active->projection.processedFrontier) &&
                        (!tick || signal < *tick)) {
                        tick = signal;
                    }
                }
            }
            if (!tick) {
                break;
            }
            failedTick = *tick;
            auto draft = std::make_shared<RuntimeState>(*impl_->active);
            std::vector<AdmittedObservation> inputs;
            for (const auto& input : impl_->pending) {
                if (input.dispatchTick == *tick) {
                    inputs.push_back(input);
                }
            }
            std::vector<FactDescriptor> facts;
            while (draft->timerCursor < timers.size() && timers[draft->timerCursor].tick == *tick) {
                impl_->timer(*draft, facts, timers[draft->timerCursor++], inputs);
            }
            impl_->failIfRequested(*tick, KernelFailurePoint::afterTimers);
            for (const auto& input : inputs) {
                impl_->input(*draft, facts, input);
            }
            impl_->failIfRequested(*tick, KernelFailurePoint::afterInput);
            if (impl_->testControls && impl_->testControls->duplicatePhaseTick == *tick &&
                !draft->projection.phases.empty()) {
                draft->projection.phases.push_back(draft->projection.phases[0]);
            }
            impl_->validateDraft(*draft);
            impl_->failIfRequested(*tick, KernelFailurePoint::beforeLedger);
            impl_->ledger(*draft, facts, *tick);
            if (impl_->testControls && std::find(impl_->testControls->signalTicks.begin(),
                                                 impl_->testControls->signalTicks.end(),
                                                 *tick) != impl_->testControls->signalTicks.end()) {
                auto visible = offsetTicks(*tick, TickSpan{1});
                if (!visible) {
                    throw std::overflow_error{"signal visibility overflow"};
                }
                draft->signalVisibility.push_back(*visible);
            }
            impl_->failIfRequested(*tick, KernelFailurePoint::beforeSeal);
            draft->projection.processedFrontier = *tick;
            draft->projection.kernelWorkTick = *tick;
            draft->projection.sealedFactCursor =
                static_cast<std::uint64_t>(draft->projection.facts.size());
            auto reserve = impl_->reserveFault(*draft);
            reserve->projection.faultDiagnostic = impl_->foldFailureResult;
            impl_->active = std::move(draft);
            impl_->faultReserve = std::move(reserve);
            impl_->pending.erase(
                std::remove_if(impl_->pending.begin(), impl_->pending.end(),
                               [&](const auto& v) { return v.dispatchTick <= *tick; }),
                impl_->pending.end());
            foldFailed = true;
            auto folded = std::make_shared<RuntimeState>(*impl_->active);
            impl_->failIfRequested(*tick, KernelFailurePoint::beforeFold);
            impl_->fold(*folded, *tick);
            auto foldedReserve = impl_->reserveFault(*folded);
            impl_->active = std::move(folded);
            impl_->faultReserve = std::move(foldedReserve);
            foldFailed = false;
            failedTick.reset();
        }
        auto final = std::make_shared<RuntimeState>(*impl_->active);
        final->projection.processedFrontier = *frontier;
        final->projection.lastAdvanceHorizon = horizon;
        auto reserve = impl_->reserveFault(*final);
        impl_->active = std::move(final);
        impl_->faultReserve = std::move(reserve);
        return {};
    } catch (const std::exception&) {
        // The reserve mirrors the last sealed prefix and has never been published.
        // This path performs no allocation, including the Result error construction.
        auto& projection = impl_->faultReserve->projection;
        projection.failedTick = failedTick;
        projection.requestedHorizon = horizon;
        projection.faultStage =
            foldFailed ? FaultStage::fold : (failedTick ? FaultStage::kernel : FaultStage::control);
        if (failedTick && !foldFailed) {
            projection.processedFrontier = failedTick->value() == INT64_MIN
                                               ? std::nullopt
                                               : std::optional{Tick{failedTick->value() - 1}};
        }
        impl_->active = std::move(impl_->faultReserve);
        return core::unexpected(foldFailed ? std::move(impl_->foldFailureResult)
                                           : std::move(impl_->failureResult));
    }
}

auto ExecutionKernel::snapshot() const -> core::Result<std::shared_ptr<const SnapshotStorage>> {
    if (impl_->active->projection.state != KernelSessionState::Prepared) {
        return core::unexpected(
            error("session.lifecycle_order", "faulted session cannot create Snapshot"));
    }
    return captureState();
}
auto ExecutionKernel::captureState() const
    -> core::Result<std::shared_ptr<const SnapshotStorage>> try {
    SnapshotDTO dto{impl_->active->projection,
                    {},
                    impl_->active->coverageHistory,
                    {},
                    static_cast<std::uint64_t>(impl_->active->timerCursor),
                    impl_->active->nextCommitId,
                    impl_->active->idsExhausted,
                    {{},
                     impl_->ingress.lastObservedTick_,
                     impl_->ingress.nextObservationId_,
                     impl_->ingress.observationIdsExhausted_},
                    impl_->pending};
    dto.kernel.runScope.reset();
    for (const auto& m : impl_->active->matchers) {
        MatcherSnapshot v{{}, m.epsilonAttempted};
        for (const auto state : m.states) {
            v.states.push_back(static_cast<std::uint64_t>(state));
        }
        dto.matchers.push_back(std::move(v));
    }
    for (const auto i : impl_->active->activeRequirements) {
        dto.activeRequirements.push_back(static_cast<std::uint64_t>(i));
    }
    for (std::size_t i = 0; i < impl_->ingress.admittedSubjects_.size(); ++i) {
        const auto& v = impl_->ingress.admittedSubjects_[i];
        dto.ingress.accepted.push_back(
            {{v.observationTick, std::string{v.domainToken}, std::string{v.sourceClass.token()},
              std::string{v.channel.token()}, v.action,
              v.hasAmount ? std::optional{v.amountCanonicalInteger} : std::nullopt},
             impl_->ingress.admittedSequences_[i]});
    }
    return std::make_shared<const SnapshotStorage>(std::move(dto), impl_->recoveryInputs);
} catch (const std::exception&) {
    return core::unexpected(error("execution.relation_invalid", "Snapshot allocation failed"));
}
auto ExecutionKernel::restore(const SnapshotStorage& saved, const RecoveryInputs& inputs)
    -> core::Result<std::unique_ptr<ExecutionKernel>> try {
    auto candidate = prepare(inputs.configuration, inputs.prepared);
    if (!candidate) {
        return core::unexpected(candidate.error());
    }
    auto& k = **candidate;
    const auto& dto = saved.dto;
    const auto& p = dto.kernel;
    const auto& initial = k.impl_->active->projection;
    if (p.judgementIdentity != initial.judgementIdentity ||
        saved.inputs->prepared.assembled().prepared != inputs.prepared.assembled().prepared) {
        return core::unexpected(
            error("execution.relation_invalid", "Snapshot dependency identity mismatch"));
    }
    if ((p.state != KernelSessionState::Prepared && p.state != KernelSessionState::Faulted) ||
        (p.state == KernelSessionState::Faulted) != p.faultDiagnostic.has_value() ||
        p.phases.size() != initial.phases.size() ||
        p.resources.size() != initial.resources.size() ||
        dto.matchers.size() != inputs.prepared.requirements().size() ||
        dto.timerCursor > inputs.prepared.timers().size() || p.sealedFactCursor != p.facts.size() ||
        p.fold.has_value() != inputs.configuration.ruleset.has_value()) {
        return core::unexpected(
            error("execution.relation_invalid", "Snapshot state closure invalid"));
    }
    if (p.state == KernelSessionState::Prepared) {
        if (p.faultStage || p.failedTick || p.requestedHorizon) {
            return core::unexpected(
                error("execution.relation_invalid", "healthy Snapshot has fault progress"));
        }
    } else {
        if (!p.faultStage || !p.requestedHorizon || p.faultDiagnostic->message().empty()) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot fault schema incomplete"));
        }
        const auto expected = *p.faultStage == FaultStage::fold
                                  ? "ruleset.transaction_failed"
                                  : "judgement.s7a4.kernel.transaction_failed";
        if (p.faultDiagnostic->code() != expected || p.faultDiagnostic->cause() ||
            p.faultDiagnostic->context().size() != 3) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot fault diagnostic invalid"));
        }
        const auto& context = p.faultDiagnostic->context();
        if (context[0].key != "category" || context[0].value != "invalid_relation" ||
            context[1].key != "severity" || context[1].value != "error" ||
            context[2].key != "faulted" || context[2].value != "true" ||
            (*p.faultStage == FaultStage::control ? p.failedTick.has_value()
                                                  : !p.failedTick.has_value())) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot fault context/progress invalid"));
        }
        if (*p.faultStage == FaultStage::fold &&
            (!p.fold || p.kernelWorkTick != p.failedTick || p.processedFrontier != p.failedTick)) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot Fold fault boundary invalid"));
        }
        if (*p.faultStage == FaultStage::kernel) {
            const auto frontier = p.failedTick->value() == INT64_MIN
                                      ? std::nullopt
                                      : std::optional{Tick{p.failedTick->value() - 1}};
            if (p.processedFrontier != frontier ||
                (p.kernelWorkTick && *p.kernelWorkTick >= *p.failedTick)) {
                return core::unexpected(
                    error("execution.relation_invalid", "Snapshot kernel fault boundary invalid"));
            }
        }
    }
    for (std::size_t i = 0; i < p.phases.size(); ++i) {
        if (p.phases[i].requirement != initial.phases[i].requirement ||
            p.phases[i].phase != initial.phases[i].phase ||
            p.phases[i].state > KernelPhaseState::Expired) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot phase reference invalid"));
        }
    }
    for (std::size_t i = 0; i < p.resources.size(); ++i) {
        if (p.resources[i].resourceId != initial.resources[i].resourceId) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot resource reference invalid"));
        }
    }
    if (p.fold &&
        (p.fold->factCursor > p.facts.size() ||
         p.fold->hookConsumerCursor > p.fold->produced.size() ||
         p.fold->ledgerDerivedCount != p.fold->factCursor || p.fold->maxCombo < p.fold->combo ||
         (p.state == KernelSessionState::Prepared && p.fold->factCursor != p.facts.size()))) {
        return core::unexpected(
            error("execution.relation_invalid", "Snapshot Fold prefix invalid"));
    }
    if (p.effectiveFactSemanticRevision != initial.effectiveFactSemanticRevision ||
        (p.kernelWorkTick && (!p.processedFrontier || *p.kernelWorkTick > *p.processedFrontier))) {
        return core::unexpected(
            error("execution.relation_invalid", "Snapshot revision or work frontier invalid"));
    }
    const auto timers = inputs.prepared.timers();
    for (std::size_t i = 0; i < timers.size(); ++i) {
        if ((i < dto.timerCursor) !=
            (p.processedFrontier && timers[i].tick <= *p.processedFrontier)) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot timer frontier invalid"));
        }
    }
    std::optional<std::tuple<Tick, OriginId, std::uint8_t, FactKind, RequirementIdentity>> lastFact;
    std::optional<FactId> lastId;
    for (const auto& fact : p.facts) {
        bool valid = true;
        std::visit(
            [&](const auto& f) {
                RequirementIdentity requirement;
                if constexpr (std::is_same_v<std::decay_t<decltype(f)>, PhaseOutcomeFact>) {
                    requirement = f.requirement;
                    const auto r =
                        std::find_if(inputs.prepared.assembled().graph.requirements.begin(),
                                     inputs.prepared.assembled().graph.requirements.end(),
                                     [&](const auto& r) { return r.identity == f.requirement; });
                    valid = f.outcome <= Outcome::miss &&
                            r != inputs.prepared.assembled().graph.requirements.end() &&
                            f.category == factCategoryOfPhase(f.phase) &&
                            f.canonicalOrdinal.factKind == FactKind::phaseOutcome &&
                            f.canonicalOrdinal.phaseRank == static_cast<unsigned>(f.phase) + 1 &&
                            std::any_of(p.phases.begin(), p.phases.end(), [&](const auto& row) {
                                return row.requirement == f.requirement && row.phase == f.phase &&
                                       row.state == (f.outcome == Outcome::hit
                                                         ? KernelPhaseState::Hit
                                                         : KernelPhaseState::Miss);
                            });
                    std::optional<std::string> grade;
                    if (valid && f.error) {
                        const auto target = std::find_if(
                            r->timing->phaseTargets.begin(), r->timing->phaseTargets.end(),
                            [&](const auto& t) { return t.phase == f.phase; });
                        if (!f.evidence || target == r->timing->phaseTargets.end()) {
                            valid = false;
                        } else {
                            auto delta = differenceTicks(f.evidence->observationTick.tick(),
                                                         target->chartTick);
                            valid = delta && *delta == *f.error;
                        }
                        const auto component =
                            std::find_if(r->measure.components.begin(), r->measure.components.end(),
                                         [&](const auto& c) { return c.phase == f.phase; });
                        if (component != r->measure.components.end() && component->gradeTable) {
                            const auto row =
                                std::find_if(component->gradeTable->begin(),
                                             component->gradeTable->end(), [&](const auto& row) {
                                                 return f.error->value() >= row.minimum &&
                                                        f.error->value() <= row.maximum;
                                             });
                            if (row == component->gradeTable->end()) {
                                valid = false;
                            } else {
                                grade = row->grade;
                            }
                        }
                    }
                    valid = valid && f.grade == grade;
                    if (const auto* origin = std::get_if<ObservationOrigin>(&f.originId)) {
                        valid = valid && origin->dispatchTick == f.commitTick &&
                                (!f.evidence || f.evidence == origin->observationKey);
                    } else if (const auto* timerOrigin = std::get_if<TimerOrigin>(&f.originId)) {
                        valid = valid && timerOrigin->tick == f.commitTick &&
                                timerOrigin->requirement == f.requirement && !f.evidence &&
                                !f.error;
                    } else {
                        valid = false;
                    }
                } else {
                    const auto* origin = std::get_if<CoordinationOrigin>(&f.originId);
                    valid = origin && origin->dispatchTick == f.commitTick &&
                            origin->observationKey == f.observationKey &&
                            f.kind != FactKind::phaseOutcome &&
                            f.canonicalOrdinal.factKind == f.kind &&
                            f.canonicalOrdinal.phaseRank == 0;
                }
                valid = valid && f.canonicalOrdinal.originOrdinal == 0;
                const auto key = std::tuple{f.commitTick, f.originId, f.canonicalOrdinal.phaseRank,
                                            f.canonicalOrdinal.factKind, requirement};
                valid = valid && (!lastFact || *lastFact < key) &&
                        f.factId.commitId == f.commitId &&
                        f.canonicalOrdinal.originScope == f.originId &&
                        f.canonicalOrdinal.localOrdinal == f.factId.localOrdinal &&
                        (!lastId || f.factId.commitId > lastId->commitId ||
                         (f.factId.commitId == lastId->commitId &&
                          f.factId.localOrdinal > lastId->localOrdinal)) &&
                        (dto.commitIdsExhausted || f.commitId < dto.nextCommitId) &&
                        p.processedFrontier && f.commitTick <= *p.processedFrontier;
                lastFact = key;
                lastId = f.factId;
            },
            fact);
        if (!valid) {
            return core::unexpected(error("execution.relation_invalid",
                                          "Snapshot Fact identity/order/reference invalid"));
        }
    }
    if (p.fold) {
        const auto& config = inputs.configuration.ruleset->declaration().score;
        if (p.fold->score < config.minimum || p.fold->score > config.maximum) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot Score outside declared range"));
        }
        RuntimeState rebuilt{initial, {}};
        auto rebuiltInitial = inputs.configuration.ruleset->initialState();
        if (!rebuiltInitial) {
            return core::unexpected(rebuiltInitial.error());
        }
        rebuilt.projection.fold = std::move(*rebuiltInitial);
        std::set<Tick> work;
        if (p.fold->workTick) {
            for (std::size_t i = 0; i < dto.timerCursor; ++i) {
                if (timers[i].tick <= *p.fold->workTick) {
                    work.insert(timers[i].tick);
                }
            }
            for (const auto& r : p.receipts) {
                if (r.dispatchTick <= *p.fold->workTick) {
                    work.insert(r.dispatchTick);
                }
            }
            for (std::uint64_t i = 0; i < p.fold->factCursor; ++i) {
                std::visit([&](const auto& f) { work.insert(f.commitTick); },
                           p.facts[static_cast<std::size_t>(i)]);
            }
        }
        std::size_t cursor = 0;
        while (!work.empty()) {
            const auto tick = *work.begin();
            work.erase(work.begin());
            if (!p.fold->workTick || tick > *p.fold->workTick) {
                break;
            }
            while (
                cursor < p.fold->factCursor &&
                std::visit([&](const auto& f) { return f.commitTick <= tick; }, p.facts[cursor])) {
                rebuilt.projection.facts.push_back(p.facts[cursor++]);
            }
            k.impl_->fold(rebuilt, tick);
            const auto& fold = *rebuilt.projection.fold;
            if (fold.hookConsumerCursor < fold.produced.size()) {
                work.insert(
                    fold.produced[static_cast<std::size_t>(fold.hookConsumerCursor)].visibleTick);
            }
        }
        if (*rebuilt.projection.fold != *p.fold) {
            return core::unexpected(
                error("execution.relation_invalid",
                      "Snapshot Fold state does not match successful Fact prefix"));
        }
    }
    std::vector<HeadContactOwnership> expectedCoverage;
    for (const auto& fact : p.facts) {
        const auto* head = std::get_if<PhaseOutcomeFact>(&fact);
        if (!head || head->phase != PhaseKind::head || head->outcome != Outcome::hit) {
            continue;
        }
        if (!head->evidence || head->evidence->action != InputAction::press) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot head evidence invalid"));
        }
        const auto& graph = inputs.prepared.assembled().graph;
        const auto requirement =
            std::find_if(graph.requirements.begin(), graph.requirements.end(),
                         [&](const auto& r) { return r.identity == head->requirement; });
        std::optional<LeaseIdentity> lease;
        if (!requirement->resourceClaims.empty()) {
            const auto& claim = requirement->resourceClaims.front();
            const auto resource =
                std::find_if(graph.resources.begin(), graph.resources.end(),
                             [&](const auto& r) { return r.ref == claim.resourceRef; });
            lease = LeaseIdentity{{claim.resourceRef.resourceId, resource->slotToken, 0},
                                  head->requirement,
                                  {head->requirement, PhaseKind::head, head->originId,
                                   claim.resourceRef.resourceId, claim.intent}};
        }
        const auto& key = *head->evidence;
        std::optional<CanonicalObservationKey> startPress;
        for (const auto& receipt : p.receipts) {
            const auto& observed = receipt.observationKey;
            if (observed.domainToken != key.domainToken ||
                observed.sourceClass != key.sourceClass ||
                observed.channelToken != key.channelToken) {
                continue;
            }
            if (observed.action == InputAction::press && !startPress) {
                startPress = observed;
            }
            if (observed == key) {
                break;
            }
            if (observed.action == InputAction::release) {
                startPress.reset();
            }
        }
        if (!startPress) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot contact history invalid"));
        }
        expectedCoverage.push_back(
            {head->requirement,
             {key.domainToken, key.sourceClass, key.channelToken, *startPress},
             head->commitTick,
             lease});
    }
    if (expectedCoverage != dto.coverageHistory) {
        return core::unexpected(
            error("execution.relation_invalid", "Snapshot coverage closure invalid"));
    }
    std::vector<std::uint64_t> expectedActive;
    for (std::size_t i = 0; i < inputs.prepared.assembled().graph.requirements.size(); ++i) {
        const auto& identity = inputs.prepared.assembled().graph.requirements[i].identity;
        if (std::any_of(timers.begin(),
                        timers.begin() + static_cast<std::ptrdiff_t>(dto.timerCursor),
                        [&](const auto& timer) {
                            return timer.kind == PreparedTimerKind::activation &&
                                   timer.requirement == identity;
                        })) {
            expectedActive.push_back(static_cast<std::uint64_t>(i));
        }
    }
    if (dto.activeRequirements != expectedActive) {
        return core::unexpected(
            error("execution.relation_invalid", "Snapshot activation closure invalid"));
    }
    std::vector<MatcherState> matchers;
    for (std::size_t i = 0; i < dto.matchers.size(); ++i) {
        const auto& program = *inputs.prepared.requirements()[i].executionProgram;
        if (dto.matchers[i].states.empty() ||
            !std::is_sorted(dto.matchers[i].states.begin(), dto.matchers[i].states.end()) ||
            std::adjacent_find(dto.matchers[i].states.begin(), dto.matchers[i].states.end()) !=
                dto.matchers[i].states.end() ||
            dto.matchers[i].epsilonAttempted !=
                (program.accepting[program.start] != 0 &&
                 std::find(expectedActive.begin(), expectedActive.end(), i) !=
                     expectedActive.end())) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot matcher closure invalid"));
        }
        MatcherState m{{}, dto.matchers[i].epsilonAttempted};
        for (const auto v : dto.matchers[i].states) {
            if (v >= program.accepting.size()) {
                return core::unexpected(
                    error("execution.relation_invalid", "Snapshot matcher state invalid"));
            }
            m.states.push_back(static_cast<std::size_t>(v));
        }
        matchers.push_back(std::move(m));
    }
    auto state = std::make_shared<RuntimeState>(p, std::move(matchers));
    state->projection.runScope = std::make_shared<const std::uint8_t>(0);
    state->coverageHistory = dto.coverageHistory;
    std::set<std::uint64_t> active;
    for (const auto i : dto.activeRequirements) {
        if (i >= inputs.prepared.requirements().size() || !active.insert(i).second) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot active instance invalid"));
        }
        state->activeRequirements.push_back(static_cast<std::size_t>(i));
    }
    state->timerCursor = static_cast<std::size_t>(dto.timerCursor);
    state->nextCommitId = dto.nextCommitId;
    state->idsExhausted = dto.commitIdsExhausted;
    k.impl_->validateDraft(*state);
    std::vector<ClockedIngress> raw;
    std::vector<std::optional<std::int64_t>> amounts;
    const auto mapping = inputs.configuration.inputMapping.view();
    std::optional<ObservationTick> last;
    for (const auto& v : dto.ingress.accepted) {
        if (v.key.sourceClass != mapping.sourceClass.token()) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot source mismatch"));
        }
        const auto channel = ChannelRef::fromToken(v.key.channelToken);
        if (!channel) {
            return core::unexpected(channel.error());
        }
        raw.push_back({v.key.observationTick,
                       {v.sequence,
                        v.key.action,
                        *channel,
                        v.key.domainToken,
                        {},
                        {false, false, false},
                        ContinuityKind::discrete,
                        {}}});
        amounts.push_back(v.key.amount);
        if (!last || v.key.observationTick > *last) {
            last = v.key.observationTick;
        }
    }
    if (last != dto.ingress.lastObservedTick || dto.ingress.exhausted ||
        dto.ingress.nextId != dto.ingress.accepted.size()) {
        return core::unexpected(
            error("execution.relation_invalid", "Snapshot ingress counters invalid"));
    }
    auto journal = prepareIngressBatch(k.impl_->ingress, mapping, raw, true, amounts);
    if (!journal) {
        return core::unexpected(journal.error());
    }
    if (auto reserve = reserveIngressBatch(k.impl_->ingress, *journal); !reserve) {
        return core::unexpected(reserve.error());
    }
    commitIngressBatch(k.impl_->ingress, std::move(*journal));
    k.impl_->ingress.nextObservationId_ = dto.ingress.nextId;
    k.impl_->ingress.observationIdsExhausted_ = dto.ingress.exhausted;
    std::set<Tick> dispatches;
    for (const auto& v : dto.pending) {
        auto route =
            routeExecutionObservation(inputs.configuration.latePolicy,
                                      v.observationKey.observationTick.tick(), v.admissionHorizon);
        std::optional<Tick> admissionWatermark;
        if (v.admissionHorizon) {
            auto frontier = offsetTicks(
                *v.admissionHorizon,
                TickSpan{-inputs.configuration.latePolicy.windowCloseThreshold.measuredValue()
                              ->value()});
            if (!frontier) {
                return core::unexpected(frontier.error());
            }
            admissionWatermark = *frontier;
        }
        if (v.admissionFrontier != admissionWatermark || !route ||
            route->dispatchTick != v.dispatchTick || route->wasForwarded != v.wasForwarded ||
            !dispatches.insert(v.dispatchTick).second ||
            (p.processedFrontier && v.dispatchTick <= *p.processedFrontier)) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot pending route invalid"));
        }
        if (std::none_of(dto.ingress.accepted.begin(), dto.ingress.accepted.end(),
                         [&](const auto& input) { return input.key == v.observationKey; })) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot pending input not accepted"));
        }
    }
    for (const auto& accepted : dto.ingress.accepted) {
        const auto pending =
            std::count_if(dto.pending.begin(), dto.pending.end(),
                          [&](const auto& v) { return v.observationKey == accepted.key; });
        const auto processed =
            std::count_if(p.receipts.begin(), p.receipts.end(),
                          [&](const auto& v) { return v.observationKey == accepted.key; });
        if (pending + processed != 1) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot accepted input closure incomplete"));
        }
    }
    for (const auto& receipt : p.receipts) {
        const auto hop =
            differenceTicks(receipt.dispatchTick, receipt.observationKey.observationTick.tick());
        if (!p.processedFrontier || receipt.dispatchTick > *p.processedFrontier || !hop ||
            hop->value() < 0 ||
            hop->value() > inputs.configuration.latePolicy.maxQueueHop.measuredValue()->value() ||
            std::count_if(dto.ingress.accepted.begin(), dto.ingress.accepted.end(),
                          [&](const auto& v) { return v.key == receipt.observationKey; }) != 1) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot processed receipt closure invalid"));
        }
    }
    // Re-execute the sealed kernel prefix from prepared state and recorded dispatches.
    // Structural index checks alone cannot prove matcher/contact/phase reachability.
    auto prefix = prepare(inputs.configuration, inputs.prepared);
    if (!prefix) {
        return core::unexpected(prefix.error());
    }
    (*prefix)->impl_->configuration.ruleset.reset();
    for (const auto& receipt : p.receipts) {
        (*prefix)->impl_->pending.push_back(
            {receipt.observationKey,
             receipt.dispatchTick,
             receipt.dispatchTick != receipt.observationKey.observationTick.tick(),
             {},
             {}});
    }
    (*prefix)->impl_->pending.insert((*prefix)->impl_->pending.end(), dto.pending.begin(),
                                     dto.pending.end());
    if (p.processedFrontier) {
        const auto horizon =
            offsetTicks(*p.processedFrontier,
                        *inputs.configuration.latePolicy.windowCloseThreshold.measuredValue());
        if (!horizon) {
            return core::unexpected(horizon.error());
        }
        const auto advanced = (*prefix)->advanceRecorded(*horizon);
        if (!advanced) {
            return core::unexpected(advanced.error());
        }
    }
    const auto& rebuilt = *(*prefix)->impl_->active;
    const auto& result = rebuilt.projection;
    if (result.phases != p.phases || result.resources != p.resources ||
        result.contacts != p.contacts || result.ownership != p.ownership ||
        result.observers != p.observers || result.receipts != p.receipts ||
        result.facts != p.facts || rebuilt.matchers.size() != state->matchers.size()) {
        return core::unexpected(
            error("execution.relation_invalid", "Snapshot kernel prefix unreachable"));
    }
    for (std::size_t i = 0; i < rebuilt.matchers.size(); ++i) {
        if (rebuilt.matchers[i].states != state->matchers[i].states ||
            rebuilt.matchers[i].epsilonAttempted != state->matchers[i].epsilonAttempted) {
            return core::unexpected(
                error("execution.relation_invalid", "Snapshot matcher prefix unreachable"));
        }
    }
    k.impl_->pending = dto.pending;
    k.impl_->faultReserve = k.impl_->reserveFault(*state);
    k.impl_->active = std::move(state);
    k.impl_->historyAvailable = false;
    return std::move(*candidate);
} catch (const std::exception&) {
    return core::unexpected(
        error("execution.relation_invalid", "Snapshot restore validation failed"));
}
auto ExecutionKernel::archive() const -> ReplayArchive {
    return ReplayArchive{impl_->seekSource ? impl_->seekSource : impl_->recording,
                         impl_->recoveryInputs};
}
auto ExecutionKernel::canArchive() const noexcept -> bool {
    return impl_->historyAvailable;
}
auto ExecutionKernel::evaluate(const ReplayArchive& archive, const KernelTestControls* controls)
    -> core::Result<ReplayEvaluation> try {
    auto k = prepare(archive.dependencies().configuration, archive.dependencies().prepared);
    if (!k) {
        return core::unexpected(k.error());
    }
    if ((*k)->query()->projection.judgementIdentity != archive.data().identity) {
        return core::unexpected(error("execution.relation_invalid", "Replay identity mismatch"));
    }
    if (controls) {
        auto applied = (*k)->inject(*controls);
        if (!applied) {
            return core::unexpected(applied.error());
        }
    }
    std::uint64_t count = 0;
    for (const auto& record : archive.data().records) {
        if (record.horizon) {
            if (!record.batch.empty() || !record.admission.empty() || !record.result) {
                return core::unexpected(
                    error("execution.relation_invalid", "Replay control structure invalid"));
            }
            auto result = (*k)->advance(*record.horizon);
            const auto& current = (*k)->query()->projection;
            if ((!result && current.state != KernelSessionState::Faulted) ||
                !sameKernelResult(current, *record.result)) {
                return core::unexpected(
                    error("execution.relation_invalid", "Replay full result mismatch"));
            }
        } else {
            if (record.result || record.batch.empty() || record.batch.size() > UINT64_MAX - count) {
                return core::unexpected(
                    error("execution.relation_invalid", "Replay accepted batch structure invalid"));
            }
            const auto sorted =
                std::is_sorted(record.batch.begin(), record.batch.end(),
                               [](const auto& a, const auto& b) { return a.key < b.key; });
            if (!sorted) {
                return core::unexpected(
                    error("execution.relation_invalid", "Replay canonical records unordered"));
            }
            auto accepted = (*k)->submitCanonical(record.batch);
            if (!accepted || *accepted != record.admission) {
                return core::unexpected(
                    error("execution.relation_invalid", "Replay admission mismatch"));
            }
            count += static_cast<std::uint64_t>(record.batch.size());
        }
    }
    if (count != archive.data().eventCount) {
        return core::unexpected(error("execution.relation_invalid", "Replay event count mismatch"));
    }
    return ReplayEvaluation{true, std::shared_ptr<const KernelProjection>(
                                      (*k)->impl_->active, &(*k)->impl_->active->projection)};
} catch (const std::exception&) {
    return core::unexpected(error("execution.relation_invalid", "Replay evaluation failed"));
}
auto ExecutionKernel::seekCandidate(const ReplayArchive& archive, Tick horizon,
                                    const KernelTestControls* controls,
                                    const ReplayCut* requestedCut,
                                    std::span<const ReplayCheckpoint> checkpoints)
    -> core::Result<std::unique_ptr<ExecutionKernel>> try {
    const auto cut = requestedCut ? *requestedCut : replayCutAt(archive, horizon);
    if (cut.completeRecords > archive.data().records.size() ||
        (cut.partialHorizon && *cut.partialHorizon != horizon)) {
        return core::unexpected(error("execution.relation_invalid", "Seek cut invalid"));
    }
    std::uint64_t eventCount = 0;
    for (const auto& r : archive.data().records) {
        if (r.horizon) {
            if (!r.result || !r.batch.empty() || !r.admission.empty()) {
                return core::unexpected(
                    error("execution.relation_invalid", "Replay control shape invalid"));
            }
        } else {
            if (r.batch.empty() || r.result || r.batch.size() != r.admission.size() ||
                r.batch.size() > UINT64_MAX - eventCount ||
                !std::is_sorted(r.batch.begin(), r.batch.end(),
                                [](const auto& a, const auto& b) { return a.key < b.key; })) {
                return core::unexpected(
                    error("execution.relation_invalid", "Replay batch shape invalid"));
            }
            eventCount += static_cast<std::uint64_t>(r.batch.size());
        }
    }
    if (eventCount != archive.data().eventCount) {
        return core::unexpected(error("execution.relation_invalid", "Replay event count invalid"));
    }
    if (!cut.partialHorizon) {
        std::optional<Tick> recordedHorizon;
        for (std::uint64_t i = 0; i < cut.completeRecords; ++i) {
            if (archive.data().records[static_cast<std::size_t>(i)].horizon) {
                recordedHorizon = archive.data().records[static_cast<std::size_t>(i)].horizon;
            }
        }
        if (recordedHorizon != horizon) {
            return core::unexpected(
                error("execution.relation_invalid", "Seek full cut horizon mismatch"));
        }
    }
    auto candidate = prepare(archive.dependencies().configuration, archive.dependencies().prepared);
    if (!candidate) {
        return core::unexpected(candidate.error());
    }
    if ((*candidate)->query()->projection.judgementIdentity != archive.data().identity) {
        return core::unexpected(
            error("execution.relation_invalid", "Seek archive identity mismatch"));
    }
    std::uint64_t begin = 0;
    const ReplayCheckpoint* chosen = nullptr;
    for (const auto& checkpoint : checkpoints) {
        if (checkpoint.state && checkpoint.horizon <= horizon &&
            checkpoint.cut.completeRecords <= cut.completeRecords &&
            (checkpoint.cut.completeRecords != cut.completeRecords ||
             !checkpoint.cut.partialHorizon ||
             (cut.partialHorizon && *checkpoint.cut.partialHorizon <= *cut.partialHorizon)) &&
            (!chosen || checkpoint.horizon > chosen->horizon ||
             (checkpoint.horizon == chosen->horizon &&
              checkpoint.cut.completeRecords > chosen->cut.completeRecords)) &&
            sameReplayHistory(checkpoint.archive, archive)) {
            chosen = &checkpoint;
        }
    }
    if (chosen) {
        // A same-identity, structurally valid state is insufficient. Bind the full archive,
        // exact cut/H and compare every private member from the origin before trusting it.
        const auto& cert = chosen->certificate_;
        bool verified = cert && cert->archive == archive.anchor() && cert->cut == chosen->cut &&
                        cert->horizon == chosen->horizon && cert->state == chosen->state &&
                        !controls;
        if (!verified) {
            auto origin = seekCandidate(archive, chosen->horizon, controls, &chosen->cut);
            if (origin) {
                auto full = (*origin)->snapshot();
                verified = full && sameSnapshot((*full)->dto, *chosen->state);
            }
        }
        if (verified) {
            SnapshotStorage state{*chosen->state,
                                  std::make_shared<const RecoveryInputs>(archive.dependencies())};
            auto restored = restore(state, archive.dependencies());
            if (restored) {
                candidate = std::move(restored);
                begin = chosen->cut.completeRecords;
                auto prefix = std::make_shared<ReplayData>(archive.data());
                prefix->records.resize(static_cast<std::size_t>(begin));
                prefix->eventCount = 0;
                for (const auto& r : prefix->records) {
                    prefix->eventCount += static_cast<std::uint64_t>(r.batch.size());
                }
                (*candidate)->impl_->recording = std::move(prefix);
                (*candidate)->impl_->historyAvailable = true;
            }
        }
    }
    auto& k = **candidate;
    if (controls) {
        auto applied = k.inject(*controls);
        if (!applied) {
            return core::unexpected(applied.error());
        }
    }
    for (std::uint64_t i = begin; i < cut.completeRecords; ++i) {
        const auto& record = archive.data().records[static_cast<std::size_t>(i)];
        if (record.horizon) {
            if (!record.result || !record.batch.empty() || *record.horizon > horizon) {
                return core::unexpected(
                    error("execution.relation_invalid", "Seek complete control invalid"));
            }
            auto result = k.advance(*record.horizon);
            if (!result) {
                return core::unexpected(result.error());
            }
            if (!sameKernelResult(k.query()->projection, *record.result)) {
                return core::unexpected(
                    error("execution.relation_invalid", "Seek complete control mismatch"));
            }
        } else {
            if (record.result || record.batch.empty()) {
                return core::unexpected(error("execution.relation_invalid", "Seek batch invalid"));
            }
            auto accepted = k.submitCanonical(record.batch);
            if (!accepted || *accepted != record.admission) {
                return core::unexpected(
                    error("execution.relation_invalid", "Seek admission mismatch"));
            }
        }
    }
    if (cut.partialHorizon) {
        if (cut.completeRecords < archive.data().records.size()) {
            const auto& next =
                archive.data().records[static_cast<std::size_t>(cut.completeRecords)];
            if (!next.horizon || *next.horizon < horizon) {
                return core::unexpected(
                    error("execution.relation_invalid", "Seek partial control invalid"));
            }
        }
        auto result = k.advance(horizon);
        if (!result) {
            return core::unexpected(result.error());
        }
    } else if (k.query()->projection.lastAdvanceHorizon != horizon) {
        return core::unexpected(
            error("execution.relation_invalid", "Seek full cut horizon mismatch"));
    }
    k.impl_->seekSource = archive.anchor();
    return std::move(*candidate);
} catch (const std::exception&) {
    return core::unexpected(
        error("execution.relation_invalid", "Seek candidate allocation failed"));
}

} // namespace cuexis::judgement::detail
