#include "execution_kernel_internal.hpp"

namespace cuexis::judgement::detail {

using namespace kernel_detail;

void ExecutionKernel::Impl::timer(RuntimeState& d, std::vector<FactDescriptor>& facts,
                                  const PreparedTimerKey& timer, std::size_t i,
                                  std::span<const AdmittedObservation> inputs) const {
    const auto ri = graph().requirements.begin() + static_cast<std::ptrdiff_t>(i);
    // Activation/window timers often publish no fact; construct an owning origin only
    // when a fact or observer actually needs one. All consumers copy it synchronously.
    const auto origin = [&]() -> OriginId { return TimerOrigin{timer.tick, ri->identity}; };
    if (timer.kind == PreparedTimerKind::activation) {
        d.activeRequirements.push_back(i);
        for (const auto& p : ri->phases) {
            if (phase(d, i, p.kind) == KernelPhaseState::Dormant) {
                setPhase(d, i, p.kind, KernelPhaseState::Pending);
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
            failPending(d, facts, i, origin());
        } else {
            outcome(d, facts, i, PhaseKind::body, Outcome::hit, origin());
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
                setPhase(d, i, timer.phase, KernelPhaseState::Expired);
            } else if (timer.phase == PhaseKind::head) {
                failPending(d, facts, i, origin());
            } else {
                outcome(d, facts, i, timer.phase, Outcome::miss, origin());
                endOwnership(d, i);
            }
        }
    }
    if (timer.kind == PreparedTimerKind::hardDeadline) {
        if (observer(*ri)) {
            if (pendingPhase(d, i, rootPhase(*ri))) {
                setPhase(d, i, rootPhase(*ri), KernelPhaseState::Expired);
            }
        } else if (std::any_of(ri->phases.begin(), ri->phases.end(),
                               [&](const auto& p) { return pendingPhase(d, i, p.kind); })) {
            failPending(d, facts, i, origin());
        } else {
            endOwnership(d, i);
        }
    }
    if ((timer.kind == PreparedTimerKind::activation ||
         timer.kind == PreparedTimerKind::windowOpen) &&
        timer.phase == rootPhase(*ri) && pendingPhase(d, i, timer.phase) &&
        inWindow(*ri, timer.phase, timer.tick) && !d.matchers[i].epsilonAttempted) {
        const auto& program = *prepared.requirements()[i].executionProgram;
        if (program.accepting[program.start]) {
            d.matchers.update(i, [](auto& matcher) { matcher.epsilonAttempted = true; });
            if (observer(*ri)) {
                setPhase(d, i, timer.phase, KernelPhaseState::Observed);
                d.projection.observers.push_back(
                    {ri->identity, origin(), timer.tick, {}, false, true});
            } else if (available(d, i, {})) {
                success(d, facts, i, origin(), timer.tick, {}, {});
            }
        }
    }
}

void ExecutionKernel::Impl::input(RuntimeState& d, std::vector<FactDescriptor>& facts,
                                  const AdmittedObservation& input) const {
    const auto& key = input.observationKey;
    const auto tick = input.dispatchTick;
    if (d.projection.rpa) {
        auto& quarantine = d.projection.rpa->quarantine;
        auto blocked = std::find_if(quarantine.begin(), quarantine.end(), [&](const auto& v) {
            return v.domainToken == key.domainToken && v.sourceClass == key.sourceClass &&
                   v.channelToken == key.channelToken;
        });
        const bool deferred =
            std::any_of(d.projection.rpa->deferred.begin(), d.projection.rpa->deferred.end(),
                        [&](const auto& v) { return v.startPress == key; });
        if (blocked != quarantine.end() && !deferred) {
            if (key.action == InputAction::release) {
                quarantine.erase(blocked);
            }
            d.projection.receipts.push_back({key, 0, {}, tick});
            return;
        }
    }
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
            auto ri = std::find_if(graph().requirements.begin(), graph().requirements.end(),
                                   [&](const auto& r) { return r.identity == owned->requirement; });
            releaseOwner = static_cast<std::size_t>(ri - graph().requirements.begin());
            ++receipt.consideredCount;
            receipt.consumedBy = ri->identity;
            if (pendingPhase(d, *releaseOwner, PhaseKind::body)) {
                outcome(d, facts, *releaseOwner, PhaseKind::body, Outcome::miss, origin, key, true);
                for (const auto& p : ri->phases) {
                    outcome(d, facts, *releaseOwner, p.kind, Outcome::miss, origin);
                }
            } else {
                const bool matched = inWindow(*ri, PhaseKind::tail, key.observationTick.tick()) &&
                                     inWindow(*ri, PhaseKind::tail, tick) &&
                                     tick < prepared.requirements()[*releaseOwner].deadline &&
                                     std::any_of(
                                         ri->atomBindings.begin(), ri->atomBindings.end(),
                                         [&](const auto& b) { return matches(b, key); });
                if (std::any_of(ri->phases.begin(), ri->phases.end(),
                                [](const auto& p) { return p.kind == PhaseKind::tail; }))
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
                d.matchers.update(i, [&](auto& matcher) { matcher.states = next; });
            }
            if (accepted) {
                setPhase(d, i, root, KernelPhaseState::Observed);
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
            d.matchers.update(candidate.index,
                              [&](auto& matcher) { matcher.states = std::move(candidate.next); });
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

void ExecutionKernel::Impl::interruptRpa(RuntimeState& d, std::vector<FactDescriptor>& facts,
                                         const CanonicalObservationKey& start,
                                         const OriginId& origin) const {
    std::vector<std::size_t> affected;
    for (const auto& owned : d.projection.ownership) {
        if (owned.contact.startPressKey == start) {
            const auto r =
                std::find_if(graph().requirements.begin(), graph().requirements.end(),
                             [&](const auto& v) { return v.identity == owned.requirement; });
            affected.push_back(static_cast<std::size_t>(r - graph().requirements.begin()));
        }
    }
    for (const auto i : affected) {
        for (const auto& phase : graph().requirements[i].phases) {
            if (phase.kind == PhaseKind::body || phase.kind == PhaseKind::tail) {
                outcome(d, facts, i, phase.kind, Outcome::miss, origin);
            }
        }
        endOwnership(d, i);
    }
    auto& contacts = d.projection.contacts;
    contacts.erase(std::remove_if(contacts.begin(), contacts.end(),
                                  [&](const auto& v) { return v.startPressKey == start; }),
                   contacts.end());
}

void ExecutionKernel::Impl::deferredRpa(RuntimeState& d, std::vector<FactDescriptor>& facts,
                                        const AdmittedObservation& input) const {
    if (!d.projection.rpa) {
        return;
    }
    auto& deferred = d.projection.rpa->deferred;
    for (const auto& v : deferred) {
        if (v.startPress == input.observationKey) {
            interruptRpa(d, facts, v.startPress,
                         ReconciliationOrigin{input.dispatchTick, v.controlOrdinal, v.startPress,
                                              v.release.sequence, v.release});
        }
    }
    deferred.erase(
        std::remove_if(deferred.begin(), deferred.end(),
                       [&](const auto& v) { return v.startPress == input.observationKey; }),
        deferred.end());
}

} // namespace cuexis::judgement::detail
