#include "execution_kernel_internal.hpp"

namespace cuexis::judgement::detail {

using namespace kernel_detail;

auto ExecutionKernel::Impl::phaseIndex(std::size_t i, PhaseKind kind) const -> std::size_t {
    const auto& declared = graph().requirements[i].phases;
    const auto found = std::find_if(declared.begin(), declared.end(),
                                    [kind](const auto& p) { return p.kind == kind; });
    if (found == declared.end()) {
        throw std::logic_error{"phase not declared"};
    }
    return preparation->phaseOffsets[i] + static_cast<std::size_t>(found - declared.begin());
}

auto ExecutionKernel::Impl::phase(const RuntimeState& d, std::size_t i, PhaseKind kind) const
    -> KernelPhaseState {
    return d.projection.phases.at(phaseIndex(i, kind)).state;
}

void ExecutionKernel::Impl::setPhase(RuntimeState& d, std::size_t i, PhaseKind kind,
                                     KernelPhaseState value) const {
    d.projection.phases.assignScalar(phaseIndex(i, kind), &PhaseProjection::state, value);
}

auto ExecutionKernel::Impl::pendingPhase(RuntimeState& d, std::size_t i, PhaseKind kind) const
    -> bool {
    return phase(d, i, kind) == KernelPhaseState::Pending;
}

auto ExecutionKernel::Impl::inWindow(const RequirementRecord& r, PhaseKind kind, Tick tick) const
    -> bool {
    return std::any_of(
        r.timing->successWindows.begin(), r.timing->successWindows.end(),
        [&](const auto& w) { return w.phase.kind == kind && tick >= w.start && tick < w.end; });
}

auto ExecutionKernel::Impl::target(const RequirementRecord& r, PhaseKind kind) const -> Tick {
    auto found = std::find_if(r.timing->phaseTargets.begin(), r.timing->phaseTargets.end(),
                              [kind](const auto& t) { return t.phase == kind; });
    if (found == r.timing->phaseTargets.end()) {
        throw std::logic_error{"target missing"};
    }
    return found->chartTick;
}

void ExecutionKernel::Impl::outcome(RuntimeState& d, std::vector<FactDescriptor>& facts,
                                    std::size_t i, PhaseKind kind, Outcome outcome,
                                    const OriginId& origin,
                                    std::optional<CanonicalObservationKey> evidence,
                                    bool observedError) const {
    const auto value = phase(d, i, kind);
    if (value != KernelPhaseState::Pending) {
        return;
    }
    setPhase(d, i, kind, outcome == Outcome::hit ? KernelPhaseState::Hit : KernelPhaseState::Miss);
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

void ExecutionKernel::Impl::endOwnership(RuntimeState& d, std::size_t i) const {
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
        auto declaration =
            std::find_if(graph().resources.begin(), graph().resources.end(),
                         [&](const auto& v) { return v.ref.resourceId == resource->resourceId; });
        resource->state = declaration->terminalAfterTermination ? ResourceState{Terminal{}}
                                                                : ResourceState{Free{}};
    }
    owned.erase(found);
}

void ExecutionKernel::Impl::failPending(RuntimeState& d, std::vector<FactDescriptor>& facts,
                                        std::size_t i, const OriginId& origin) const {
    for (const auto& p : graph().requirements[i].phases) {
        outcome(d, facts, i, p.kind, Outcome::miss, origin);
    }
    endOwnership(d, i);
}

auto ExecutionKernel::Impl::available(RuntimeState& d, std::size_t i,
                                      const std::optional<ContactHandle>& contact,
                                      bool acquiring) const -> bool {
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

void ExecutionKernel::Impl::success(RuntimeState& d, std::vector<FactDescriptor>& facts,
                                    std::size_t i, const OriginId& origin, Tick tick,
                                    std::optional<CanonicalObservationKey> evidence,
                                    std::optional<ContactHandle> contact) const {
    const auto& r = graph().requirements[i];
    const auto kind = rootPhase(r);
    outcome(d, facts, i, kind, Outcome::hit, origin, evidence, evidence.has_value());
    if (kind == PhaseKind::tap) {
        if (!r.resourceClaims.empty()) {
            auto resource = std::find_if(
                d.projection.resources.begin(), d.projection.resources.end(), [&](const auto& v) {
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
        auto declaration = std::find_if(graph().resources.begin(), graph().resources.end(),
                                        [&](const auto& v) { return v.ref == claim.resourceRef; });
        lease =
            LeaseIdentity{{claim.resourceRef.resourceId, declaration->slotToken, 0},
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

} // namespace cuexis::judgement::detail
