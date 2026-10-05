#include "execution_reference.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>

namespace cuexis::judgement::testing {
namespace {
auto add(std::int64_t a, std::int64_t b) -> std::optional<std::int64_t> {
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) {
        return {};
    }
    return a + b;
}
auto channelEqual(const ContactHandle& c, const CanonicalObservationKey& k) -> bool {
    return std::tie(c.domainToken, c.sourceClass, c.channelToken) ==
           std::tie(k.domainToken, k.sourceClass, k.channelToken);
}
auto bindingEqual(const AtomBinding& b, const CanonicalObservationKey& k) -> bool {
    if (std::tie(b.domainToken, b.sourceClass, b.channelToken, b.action) !=
        std::tie(k.domainToken, k.sourceClass, k.channelToken, k.action)) {
        return false;
    }
    return !b.amountRange ||
           (k.amount && b.amountRange->minimum <= *k.amount && *k.amount <= b.amountRange->maximum);
}
struct Descriptor {
    OriginId origin;
    std::optional<RequirementIdentity> requirement;
    std::optional<PhaseKind> phase;
    FactKind kind;
    Outcome outcome;
    std::optional<TickDelta> error;
    std::optional<CanonicalObservationKey> evidence;
};
struct Row {
    std::map<PhaseKind, KernelPhaseState> phases;
    std::set<std::size_t> matcher;
    bool epsilonUsed{false};
};
} // namespace

auto referenceRoute(const LatePolicyParameters& p, Tick observation, std::optional<Tick> horizon)
    -> std::optional<Tick> {
    if (!p.policy || !p.finalizationWatermark.isMeasured() || !p.maxQueueHop.isMeasured() ||
        !p.windowCloseThreshold.isMeasured() || !p.windowOpenThreshold.isMeasured()) {
        return {};
    }
    const auto o = p.windowOpenThreshold.measuredValue()->value(),
               c = p.windowCloseThreshold.measuredValue()->value();
    const auto f = p.finalizationWatermark.measuredValue()->value(),
               h = p.maxQueueHop.measuredValue()->value();
    if (o < 0 || c < 0 || f <= c ||
        (*p.policy == LateEventPolicy::rejectLate ? h != 0 : h < 1 || h > f - c)) {
        return {};
    }
    const auto t = observation.value();
    auto close = add(t, c), final = add(t, f);
    if (!add(t, -o) || !add(t, 1) || !close || !final) {
        return {};
    }
    if (!horizon || horizon->value() < *close) {
        return observation;
    }
    if (horizon->value() >= *final || *p.policy == LateEventPolicy::rejectLate) {
        return {};
    }
    auto frontier = add(horizon->value(), -c);
    if (!frontier) {
        return {};
    }
    auto q = add(*frontier, 1);
    if (!q || *q <= t || (t < 0 && *q > INT64_MAX + t)) {
        return {};
    }
    if (*q - t > h) {
        return {};
    }
    return Tick{*q};
}

auto scanReference(const PreparedGameplay& prepared, const SessionConfiguration& configuration,
                   const std::vector<AdmittedObservation>& inputs, Tick horizon)
    -> KernelProjection {
    const auto& graph = prepared.assembled().graph;
    const auto frontierValue = add(
        horizon.value(), -configuration.latePolicy.windowCloseThreshold.measuredValue()->value());
    if (!frontierValue) {
        throw std::overflow_error{"oracle horizon"};
    }
    const Tick frontier{*frontierValue};
    KernelProjection result{{},
                            {},
                            {},
                            {},
                            {},
                            {},
                            {},
                            frontier,
                            horizon,
                            KernelSessionState::Prepared,
                            {},
                            makeRuntimePreparedIdentity(graph, configuration.identityDeclarations,
                                                        configuration.inputMapping.view(),
                                                        configuration.latePolicy,
                                                        configuration.calibrationIdentityToken),
                            {},
                            {},
                            {}};
    std::vector<Row> rows(graph.requirements.size());
    std::set<Tick> ticks;
    std::map<std::string, std::size_t> resources;
    for (std::size_t i = 0; i < graph.resources.size(); ++i) {
        result.resources.push_back({graph.resources[i].ref.resourceId, Free{}});
        resources.emplace(graph.resources[i].ref.resourceId, i);
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto& r = graph.requirements[i];
        for (const auto& p : r.phases) {
            rows[i].phases[p.kind] = KernelPhaseState::Dormant;
        }
        rows[i].matcher.insert(prepared.requirements()[i].executionProgram->start);
        for (const auto& w : r.timing->successWindows) {
            ticks.insert(w.start);
            ticks.insert(w.end);
            if (w.phase.kind == PhaseKind::head) {
                ticks.insert(Tick{*add(r.timing->body->start.value(), 1)});
            }
        }
        if (r.timing->body) {
            ticks.insert(r.timing->body->end);
        }
        ticks.insert(prepared.requirements()[i].deadline);
    }
    for (const auto& input : inputs) {
        ticks.insert(input.dispatchTick);
    }
    std::uint64_t nextCommit = 0;
    const auto root = [&](std::size_t i) {
        return graph.requirements[i].timing->body ? PhaseKind::head : PhaseKind::tap;
    };
    const auto observed = [&](std::size_t i) {
        const auto& claims = graph.requirements[i].resourceClaims;
        return !claims.empty() && claims[0].intent == ResourceClaimIntent::observe;
    };
    const auto pending = [&](std::size_t i, PhaseKind phase) {
        return rows[i].phases.at(phase) == KernelPhaseState::Pending;
    };
    const auto inside = [&](std::size_t i, PhaseKind phase, Tick tick) {
        for (const auto& w : graph.requirements[i].timing->successWindows) {
            if (w.phase.kind == phase && w.start <= tick && tick < w.end) {
                return true;
            }
        }
        return false;
    };
    const auto ownership = [&](std::size_t i) {
        return std::find_if(result.ownership.begin(), result.ownership.end(), [&](const auto& o) {
            return o.requirement == graph.requirements[i].identity;
        });
    };
    const auto end = [&](std::size_t i) {
        const auto owner = ownership(i);
        if (owner == result.ownership.end()) {
            return;
        }
        if (owner->lease) {
            auto ri = resources.at(owner->lease->slot.resourceId);
            result.resources[ri].state = graph.resources[ri].terminalAfterTermination
                                             ? ResourceState{Terminal{}}
                                             : ResourceState{Free{}};
        }
        result.ownership.erase(owner);
    };
    for (Tick tick : ticks) {
        if (tick > frontier) {
            break;
        }
        std::vector<Descriptor> descriptors;
        const auto outcome = [&](std::size_t i, PhaseKind phase, Outcome value, OriginId origin,
                                 std::optional<CanonicalObservationKey> evidence = {},
                                 bool withError = false) {
            if (!pending(i, phase)) {
                return;
            }
            rows[i].phases[phase] =
                value == Outcome::hit ? KernelPhaseState::Hit : KernelPhaseState::Miss;
            std::optional<TickDelta> delta;
            if (withError && evidence) {
                auto t = std::find_if(graph.requirements[i].timing->phaseTargets.begin(),
                                      graph.requirements[i].timing->phaseTargets.end(),
                                      [phase](const auto& v) { return v.phase == phase; });
                const auto observation = evidence->observationTick.tick().value();
                const auto target = t->chartTick.value();
                const auto error = (target > 0 && observation < INT64_MIN + target) ||
                                           (target < 0 && observation > INT64_MAX + target)
                                       ? std::nullopt
                                       : std::optional<std::int64_t>{observation - target};
                if (!error) {
                    throw std::overflow_error{"oracle phase error"};
                }
                delta = TickDelta{*error};
            }
            descriptors.push_back({std::move(origin), graph.requirements[i].identity, phase,
                                   FactKind::phaseOutcome, value, delta, evidence});
        };
        const auto fail = [&](std::size_t i, OriginId origin) {
            for (const auto& p : graph.requirements[i].phases) {
                outcome(i, p.kind, Outcome::miss, origin);
            }
            end(i);
        };
        const auto free = [&](std::size_t i, std::optional<ContactHandle> contact, bool accepting) {
            const auto& r = graph.requirements[i];
            if (!r.resourceClaims.empty() &&
                !std::holds_alternative<Free>(
                    result.resources[resources.at(r.resourceClaims[0].resourceRef.resourceId)]
                        .state)) {
                return false;
            }
            if (accepting && root(i) == PhaseKind::head) {
                if (!contact) {
                    return false;
                }
                for (const auto& o : result.ownership) {
                    if (o.contact == *contact) {
                        return false;
                    }
                }
            }
            return true;
        };
        const auto hit = [&](std::size_t i, OriginId origin,
                             std::optional<CanonicalObservationKey> evidence,
                             std::optional<ContactHandle> contact) {
            const auto& r = graph.requirements[i];
            outcome(i, root(i), Outcome::hit, origin, evidence, evidence.has_value());
            std::optional<LeaseIdentity> lease;
            if (!r.resourceClaims.empty()) {
                const auto& claim = r.resourceClaims[0];
                const auto ri = resources.at(claim.resourceRef.resourceId);
                if (root(i) == PhaseKind::tap) {
                    if (graph.resources[ri].terminalAfterTermination) {
                        result.resources[ri].state = Terminal{};
                    }
                    return;
                }
                lease = LeaseIdentity{
                    {claim.resourceRef.resourceId, graph.resources[ri].slotToken, 0},
                    r.identity,
                    {r.identity, root(i), origin, claim.resourceRef.resourceId, claim.intent}};
                result.resources[ri].state = Held{r.identity, *lease, *contact};
            }
            if (root(i) == PhaseKind::head) {
                result.ownership.push_back({r.identity, *contact, tick, lease});
            }
        };
        // Scan each execution kind globally before the next kind. The source obligations, rather
        // than S2's prepared table, establish eligibility on this Tick.
        for (unsigned kind = 0; kind != 5; ++kind) {
            for (std::size_t i = 0; i < rows.size(); ++i) {
                const auto& r = graph.requirements[i];
                const OriginId origin = TimerOrigin{tick, r.identity};
                Tick activation{INT64_MAX};
                for (const auto& w : r.timing->successWindows) {
                    if (w.phase.kind == root(i)) {
                        activation = std::min(activation, w.start);
                    }
                }
                if (kind == 0 && activation == tick) {
                    for (auto& [phase, value] : rows[i].phases) {
                        value = KernelPhaseState::Pending;
                    }
                }
                if (kind <= 1 && pending(i, root(i)) && inside(i, root(i), tick) &&
                    (kind == 0 ? activation == tick
                               : std::any_of(r.timing->successWindows.begin(),
                                             r.timing->successWindows.end(), [&](const auto& w) {
                                                 return w.phase.kind == root(i) && w.start == tick;
                                             }))) {
                    const auto& program = *prepared.requirements()[i].executionProgram;
                    if (!rows[i].epsilonUsed && program.accepting[program.start]) {
                        rows[i].epsilonUsed = true;
                        if (observed(i)) {
                            rows[i].phases[root(i)] = KernelPhaseState::Observed;
                            result.observers.push_back({r.identity, origin, tick, {}, false, true});
                        } else if (free(i, {}, true)) {
                            hit(i, origin, {}, {});
                        }
                    }
                }
                if (kind == 2 && r.timing->body && r.timing->body->end == tick &&
                    pending(i, PhaseKind::body)) {
                    const auto owner = ownership(i);
                    bool covered = owner != result.ownership.end() &&
                                   owner->acquiredTick <= r.timing->body->start;
                    if (covered) {
                        for (const auto& input : inputs) {
                            if (input.dispatchTick == tick &&
                                input.observationKey.action == InputAction::release &&
                                channelEqual(owner->contact, input.observationKey) &&
                                input.observationKey.observationTick.tick() < tick) {
                                covered = false;
                            }
                        }
                    }
                    if (!covered) {
                        fail(i, origin);
                    } else {
                        outcome(i, PhaseKind::body, Outcome::hit, origin);
                        if (!rows[i].phases.contains(PhaseKind::tail)) {
                            end(i);
                        }
                    }
                }
                if (kind == 3) {
                    for (const auto& p : r.phases) {
                        Tick close{INT64_MIN};
                        for (const auto& w : r.timing->successWindows) {
                            if (w.phase.kind != p.kind) {
                                continue;
                            }
                            auto closeEnd = w.end;
                            if (p.kind == PhaseKind::head) {
                                closeEnd = std::min(closeEnd,
                                                    Tick{*add(r.timing->body->start.value(), 1)});
                            }
                            if (closeEnd > w.start) {
                                close = std::max(close, closeEnd);
                            }
                        }
                        if (tick != close || !pending(i, p.kind)) {
                            continue;
                        }
                        if (observed(i)) {
                            rows[i].phases[p.kind] = KernelPhaseState::Expired;
                        } else if (p.kind == PhaseKind::head) {
                            fail(i, origin);
                        } else {
                            outcome(i, p.kind, Outcome::miss, origin);
                            end(i);
                        }
                    }
                }
                if (kind == 4 && prepared.requirements()[i].deadline == tick) {
                    if (observed(i)) {
                        if (pending(i, root(i))) {
                            rows[i].phases[root(i)] = KernelPhaseState::Expired;
                        }
                    } else {
                        fail(i, origin);
                    }
                }
            }
        }
        for (const auto& input : inputs) {
            if (input.dispatchTick != tick) {
                continue;
            }
            const auto& key = input.observationKey;
            const OriginId origin = ObservationOrigin{tick, key};
            auto contactIterator =
                std::find_if(result.contacts.begin(), result.contacts.end(),
                             [&](const auto& c) { return channelEqual(c, key); });
            const bool newPress =
                key.action == InputAction::press && contactIterator == result.contacts.end();
            if (newPress) {
                result.contacts.push_back(
                    {key.domainToken, key.sourceClass, key.channelToken, key});
                contactIterator = std::prev(result.contacts.end());
            }
            std::optional<ContactHandle> contact = contactIterator == result.contacts.end()
                                                       ? std::nullopt
                                                       : std::optional{*contactIterator};
            InputReceipt receipt{key, 0, {}, tick};
            std::optional<std::size_t> releaseOwner;
            if (key.action == InputAction::release && contact) {
                for (std::size_t i = 0; i < rows.size(); ++i) {
                    const auto owner = ownership(i);
                    if (owner == result.ownership.end() || owner->contact != *contact) {
                        continue;
                    }
                    releaseOwner = i;
                    receipt.consideredCount = 1;
                    receipt.consumedBy = graph.requirements[i].identity;
                    if (pending(i, PhaseKind::body)) {
                        outcome(i, PhaseKind::body, Outcome::miss, origin, key, true);
                        fail(i, origin);
                    } else {
                        bool tailHit = inside(i, PhaseKind::tail, key.observationTick.tick()) &&
                                       inside(i, PhaseKind::tail, tick);
                        tailHit = tailHit && tick < prepared.requirements()[i].deadline;
                        tailHit = tailHit &&
                                  std::any_of(graph.requirements[i].atomBindings.begin(),
                                              graph.requirements[i].atomBindings.end(),
                                              [&](const auto& b) { return bindingEqual(b, key); });
                        outcome(i, PhaseKind::tail, tailHit ? Outcome::hit : Outcome::miss, origin,
                                key, true);
                        end(i);
                    }
                    break;
                }
            }
            struct Offer {
                std::size_t i;
                std::set<std::size_t> next;
                bool accept;
            };
            std::vector<Offer> offers;
            for (std::size_t i = 0; i < rows.size(); ++i) {
                const auto& r = graph.requirements[i];
                if (releaseOwner == i || !pending(i, root(i)) || !inside(i, root(i), tick) ||
                    !inside(i, root(i), key.observationTick.tick()) ||
                    (r.timing->body && tick > r.timing->body->start)) {
                    continue;
                }
                const auto& program = *prepared.requirements()[i].executionProgram;
                bool considered = false;
                std::set<std::size_t> next;
                for (const auto& binding : r.atomBindings) {
                    if (binding.tailOnly || !bindingEqual(binding, key)) {
                        continue;
                    }
                    considered = true;
                    const auto symbol = static_cast<std::size_t>(std::find(program.atomRefs.begin(),
                                                                           program.atomRefs.end(),
                                                                           binding.atomRef) -
                                                                 program.atomRefs.begin());
                    for (const auto old : rows[i].matcher) {
                        const auto value =
                            program.transitions[old * program.atomRefs.size() + symbol];
                        if (value && program.live[*value]) {
                            next.insert(*value);
                        }
                    }
                }
                if (!considered) {
                    continue;
                }
                ++receipt.consideredCount;
                const bool accept = std::any_of(next.begin(), next.end(),
                                                [&](auto v) { return program.accepting[v] != 0; });
                if (observed(i)) {
                    result.observers.push_back(
                        {r.identity, origin, tick, key, !next.empty(), accept});
                    if (!next.empty()) {
                        rows[i].matcher = next;
                    }
                    if (accept) {
                        rows[i].phases[root(i)] = KernelPhaseState::Observed;
                    }
                } else if (!next.empty() && (root(i) != PhaseKind::head || !accept || newPress)) {
                    offers.push_back({i, next, accept});
                }
            }
            const auto order = [&](std::size_t i) {
                const auto& r = graph.requirements[i];
                return r.resourceClaims.empty()
                           ? std::tuple{std::string{"@independent"},
                                        r.independentCompetition->priority,
                                        r.independentCompetition->tieRank}
                           : std::tuple{r.resourceClaims[0].resourceRef.resourceId,
                                        r.resourceClaims[0].claimPolicy.competition->priority,
                                        r.resourceClaims[0].claimPolicy.competition->tieRank};
            };
            std::optional<std::size_t> winner;
            if (!receipt.consumedBy) {
                for (std::size_t o = 0; o < offers.size(); ++o) {
                    if (free(offers[o].i, contact, offers[o].accept) &&
                        (!winner || order(offers[o].i) < order(offers[*winner].i))) {
                        winner = o;
                    }
                }
            }
            if (winner) {
                const auto& offer = offers[*winner];
                rows[offer.i].matcher = offer.next;
                receipt.consumedBy = graph.requirements[offer.i].identity;
                if (offer.accept) {
                    hit(offer.i, origin, key, contact);
                }
            }
            if (key.action == InputAction::release && contact) {
                result.contacts.erase(
                    std::remove(result.contacts.begin(), result.contacts.end(), *contact),
                    result.contacts.end());
            }
            if (!receipt.consumedBy) {
                descriptors.push_back(
                    {CoordinationOrigin{tick, key},
                     {},
                     {},
                     receipt.consideredCount ? FactKind::consumeEmpty : FactKind::stray,
                     Outcome::miss,
                     {},
                     key});
            }
            result.receipts.push_back(receipt);
        }
        const auto rank = [](const auto& d) {
            return d.phase ? static_cast<unsigned>(*d.phase) + 1 : 0U;
        };
        std::stable_sort(descriptors.begin(), descriptors.end(), [&](const auto& a, const auto& b) {
            if (a.origin != b.origin) {
                return a.origin < b.origin;
            }
            if (rank(a) != rank(b)) {
                return rank(a) < rank(b);
            }
            if (a.kind != b.kind) {
                return a.kind < b.kind;
            }
            return a.requirement < b.requirement;
        });
        std::optional<OriginId> last;
        std::uint64_t id = 0, local = 0;
        for (const auto& d : descriptors) {
            if (!last || *last != d.origin) {
                id = nextCommit++;
                local = 0;
                last = d.origin;
            }
            const LogicalCanonicalOrdinal ordinal{d.origin, 0, local,
                                                  static_cast<std::uint8_t>(rank(d)), d.kind};
            if (d.phase) {
                result.facts.emplace_back(PhaseOutcomeFact{*d.requirement,
                                                           *d.phase,
                                                           d.outcome,
                                                           factCategoryOfPhase(*d.phase),
                                                           d.error,
                                                           d.evidence,
                                                           d.origin,
                                                           id,
                                                           {id, local},
                                                           tick,
                                                           ordinal});
            } else {
                result.facts.emplace_back(
                    ReceiptFact{d.kind, *d.evidence, d.origin, id, {id, local}, tick, ordinal});
            }
            ++local;
        }
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        for (const auto& phase : graph.requirements[i].phases) {
            result.phases.push_back(
                {graph.requirements[i].identity, phase.kind, rows[i].phases.at(phase.kind)});
        }
    }
    return result;
}
} // namespace cuexis::judgement::testing
