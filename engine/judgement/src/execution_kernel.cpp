#include "execution_kernel.hpp"
#include "execution_testing.hpp"

#include <algorithm>
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
    std::vector<AdmittedObservation> pending;
    std::shared_ptr<const RuntimeState> active;
    std::optional<KernelTestControls> testControls;
    std::shared_ptr<RuntimeState> faultReserve;
    core::Error failureResult;

    Impl(SessionConfiguration c, PreparedGameplay p, std::shared_ptr<const RuntimeState> state)
        : configuration(std::move(c)), prepared(std::move(p)), active(std::move(state)),
          faultReserve(std::make_shared<RuntimeState>(*active)),
          failureResult(
              error("kernel.transaction_failed", "the unsealed Tick transaction failed", true)) {
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
                                                                 ordinal});
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
    const auto mapping = c.inputMapping.view();
    std::vector<MatcherState> matchers;
    KernelProjection projection{{},
                                {},
                                {},
                                {},
                                {},
                                {},
                                {},
                                {},
                                {},
                                KernelSessionState::Prepared,
                                {},
                                makeRuntimePreparedIdentity(g, c.identityDeclarations, mapping,
                                                            c.latePolicy,
                                                            c.calibrationIdentityToken),
                                {},
                                {},
                                {}};
    projection.runScope = std::make_shared<const std::uint8_t>(0);
    for (std::size_t i = 0; i < g.requirements.size(); ++i) {
        const auto& r = g.requirements[i];
        if (!prepared.requirements()[i].executionProgram) {
            return core::unexpected(
                error("execution.profile_incomplete", "prepared executable program missing"));
        }
        for (const auto& b : r.atomBindings) {
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
        for (const auto& p : r.phases) {
            projection.phases.push_back({r.identity, p.kind, KernelPhaseState::Dormant});
        }
        matchers.push_back({{prepared.requirements()[i].executionProgram->start}, false});
    }
    for (const auto& r : g.resources) {
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
    -> core::Result<std::vector<InputReceiptPending>> try {
    if (impl_->active->projection.state != KernelSessionState::Prepared) {
        return core::unexpected(
            error("session.lifecycle_order", "submission requires a healthy prepared session"));
    }
    if (batch.empty()) {
        return std::vector<InputReceiptPending>{};
    }
    auto mapping = impl_->configuration.inputMapping.view();
    auto journal = prepareIngressBatch(impl_->ingress, mapping, batch);
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
    commitIngressBatch(impl_->ingress, std::move(*journal));
    for (auto& entry : entries) {
        impl_->pending.push_back(std::move(entry));
    }
    std::sort(impl_->pending.begin(), impl_->pending.end(),
              [](const auto& a, const auto& b) { return a.dispatchTick < b.dispatchTick; });
    return receipts;
} catch (const std::exception&) {
    return core::unexpected(
        error("execution.relation_invalid", "submission storage allocation failed"));
}

auto ExecutionKernel::advance(Tick horizon) -> core::Result<void> {
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
            auto reserve = impl_->reserveFault(*draft);
            impl_->active = std::move(draft);
            impl_->faultReserve = std::move(reserve);
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
        if (failedTick) {
            projection.processedFrontier = failedTick->value() == INT64_MIN
                                               ? std::nullopt
                                               : std::optional{Tick{failedTick->value() - 1}};
        }
        impl_->active = std::move(impl_->faultReserve);
        return core::unexpected(std::move(impl_->failureResult));
    }
}
} // namespace cuexis::judgement::detail
