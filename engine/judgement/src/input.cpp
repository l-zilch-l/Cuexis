#include "ingress_transaction.hpp"
#include "source_codes.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cuexis/judgement/diagnostic.hpp>
#include <cuexis/judgement/input_boundary.hpp>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "input_helpers.hpp"

namespace cuexis::judgement {

using namespace input_detail;

//  ---------------------------------------------------------------------------------------------
//  The entry boundary (Spec 3.7.3, CM-T05, CM-T10)
//  ---------------------------------------------------------------------------------------------

namespace {
auto buildNormalizedEntry(const InputMappingProfile& mapping, ObservationTick sessionClock,
                          const IngressDeclaration& declaration, ObservationId id,
                          bool resolveDomains,
                          const std::optional<std::int64_t>* canonicalAmount = nullptr)
    -> core::Result<NormalizedObservationEntry> {
    //  The mapping is re-validated here, so an entry on a mapping that validateInputMapping would
    //  refuse is a stable rejection rather than undefined behaviour.
    const auto mappingStatus = validateInputMapping(mapping);
    if (!mappingStatus.has_value()) {
        return core::unexpected(mappingStatus.error());
    }

    const std::string rawTime = describeRawTime(declaration.rawTimestamps);

    //  Continuous input capability is a stable 7A rejection that points at S7B-1 (Spec 3.7.7).
    if (declaration.continuity != ContinuityKind::discrete) {
        return core::unexpected(capabilityUnsupportedError(
            codes::kInputContinuousUnsupportedCode, codes::kContinuityPath,
            "continuous input capability is not enabled in Stage 7A", rawTime));
    }

    //  A crossing, a reconnect or a dropped sample has no 7A representation either: Spec 3.7.7 puts
    //  the discontinuity representation in the same "continuous input capability" row as the
    //  trajectory and rolls its admission into S7B-1, so the explicit CM-T10 behavior for crossing
    //  a sampling hole is the same R-05 capability rejection. It keeps the frozen R-05 code and
    //  capability id and is distinguished from a declared trajectory by its field path, because the
    //  ABI code vocabulary lists no discontinuity code and this batch adds no ABI code.
    if (declaration.discontinuity.crossedSamplingGap || declaration.discontinuity.reconnected ||
        declaration.discontinuity.droppedSamples) {
        return core::unexpected(capabilityUnsupportedError(
            codes::kInputContinuousUnsupportedCode, codes::kDiscontinuityPath,
            "discontinuity representation is not enabled in Stage 7A", rawTime));
    }

    //  The calibrated session clock is the only source of observationTick (Spec 3.7.3). A negative
    //  value is representable in the frozen signed 64-bit tick domain and is admitted; only a
    //  reversal is rejected below.
    const ObservationTick tick = sessionClock;

    std::optional<NormalizedAmount> amount;
    std::string_view domainToken{};
    if (declaration.quantity.has_value() || resolveDomains) {
        const auto domain = resolveDomain(mapping, declaration.domainToken);
        if (!domain.has_value()) {
            return core::unexpected(domain.error());
        }
        if (canonicalAmount && *canonicalAmount) {
            const auto value = **canonicalAmount;
            const auto& spec = (*domain)->amount;
            if (value < spec.minimum || value > spec.maximum ||
                (spec.boundaryPolicy == AmountBoundaryPolicy::exclusive &&
                 (value == spec.minimum || value == spec.maximum))) {
                return core::unexpected(declarationInvalidError(
                    codes::kDomainTokenPath, "canonical amount outside declared range"));
            }
            amount = NormalizedAmount{{value, &spec}, (*domain)->domainToken};
        } else if (!canonicalAmount && declaration.quantity) {
            const auto quantized = quantizeAmount((*domain)->amount, *declaration.quantity);
            if (!quantized) {
                return core::unexpected(quantized.error());
            }
            amount = NormalizedAmount{.value = *quantized, .domainToken = (*domain)->domainToken};
        }
        //  The quantized amount borrows the specification from the session's mapping profile, the
        //  same non-owning prepared-view convention the ABI fixes.
        domainToken = (*domain)->domainToken;
    }

    const NormalizedObservation observation{
        .observationId = id,
        .observationTick = tick,
        .ingressSequence = declaration.ingressSequence,
        .domainToken = domainToken,
        .action = declaration.action,
        .channel = declaration.channel,
        //  The source class is the session's declared device class: the mapping decides which class
        //  a session is bound to, and the canonical observation carries it.
        .sourceClass = mapping.sourceClass,
        .amount = amount,
    };

    //  The canonical subject is derived here and never supplied by a caller. It carries no ingress
    //  ordinal, which is the type-level statement of Spec 3.7.4 item 4.
    const CanonicalIngressSubject subject = deriveCanonicalSubject(observation);

    return NormalizedObservationEntry{.subject = subject, .observation = observation};
}
} // namespace

auto detail::prepareIngressBatch(const SessionIngressState& state,
                                 const InputMappingProfile& mapping,
                                 std::span<const ClockedIngress> batch, bool resolveDomains,
                                 std::span<const std::optional<std::int64_t>> canonicalAmounts)
    -> core::Result<IngressJournal> try {
    if (!canonicalAmounts.empty() && canonicalAmounts.size() != batch.size()) {
        return core::unexpected(
            declarationInvalidError(codes::kDomainTokenPath, "canonical amount count mismatch"));
    }
    IngressJournal journal{
        {}, {}, state.lastObservedTick_, state.nextObservationId_, state.observationIdsExhausted_};
    journal.entries.reserve(batch.size());
    journal.owners.reserve(batch.size());
    // Sequence duplication has precedence over canonical and Tick collisions throughout the batch.
    std::vector<IngressSequence> sequences;
    for (const auto& item : batch) {
        auto sequence = item.declaration.ingressSequence;
        if (sequenceAdmitted(state.admittedSequences_, sequence) ||
            sequenceAdmitted(sequences, sequence)) {
            return core::unexpected(lateRejectionError(
                codes::kIngressSequenceDuplicateCode, "this ingress sequence was already admitted",
                codes::kIngressSequencePath, describeRawTime(item.declaration.rawTimestamps)));
        }
        sequences.push_back(sequence);
    }
    for (const auto& item : batch) {
        if (resolveDomains && item.declaration.action != InputAction::press &&
            item.declaration.action != InputAction::release &&
            item.declaration.action != InputAction::update) {
            return core::unexpected(
                declarationInvalidError(codes::kDomainTokenPath, "unknown input action"));
        }
        auto entry = buildNormalizedEntry(
            mapping, item.observationTick, item.declaration, ObservationId{}, resolveDomains,
            canonicalAmounts.empty()
                ? nullptr
                : &canonicalAmounts[static_cast<std::size_t>(&item - batch.data())]);
        if (!entry) {
            return core::unexpected(entry.error());
        }
        if (item.declaration.channel.token().empty()) {
            return core::unexpected(
                declarationInvalidError(codes::kDomainTokenPath, "empty channel"));
        }
        auto owner = std::make_unique<const OwnedIngressSubject>(
            OwnedIngressSubject{std::string{entry->observation.domainToken},
                                std::string{entry->observation.channel.token()},
                                std::string{entry->observation.sourceClass.token()}});
        entry->observation.domainToken = owner->domain;
        entry->observation.channel = *ChannelRef::fromToken(owner->channel);
        entry->observation.sourceClass = *SourceClass::fromToken(owner->source);
        if (entry->observation.amount) {
            entry->observation.amount->domainToken = owner->domain;
        }
        entry->subject = deriveCanonicalSubject(entry->observation);
        journal.owners.push_back(std::move(owner));
        journal.entries.push_back(*entry);
    }
    for (std::size_t i = 0; i < journal.entries.size(); ++i) {
        const auto& candidate = journal.entries[i].subject;
        const auto duplicate = [&]() {
            return resolveDomains ? lateRejectionError(codes::kDuplicateQueueCode,
                                                       "canonical subject was already queued",
                                                       codes::kCanonicalQueueKeyPath, {})
                                  : sameTickCollisionError({});
        };
        if (collidesAtTick(state.admittedSubjects_, candidate)) {
            return core::unexpected(duplicate());
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (journal.entries[j].subject == candidate) {
                return core::unexpected(duplicate());
            }
        }
    }
    if (resolveDomains) {
        for (std::size_t i = 0; i < journal.entries.size(); ++i) {
            auto tick = journal.entries[i].subject.observationTick;
            for (const auto& subject : state.admittedSubjects_) {
                if (tick == subject.observationTick) {
                    return core::unexpected(sameTickCollisionError({}));
                }
            }
            for (std::size_t j = 0; j < i; ++j) {
                if (tick == journal.entries[j].subject.observationTick) {
                    return core::unexpected(sameTickCollisionError({}));
                }
            }
        }
    }
    for (const auto& item : batch) {
        if (state.lastObservedTick_ && item.observationTick < *state.lastObservedTick_) {
            return core::unexpected(
                timeReversalError(describeRawTime(item.declaration.rawTimestamps)));
        }
        if (!journal.latestTick || *journal.latestTick < item.observationTick) {
            journal.latestTick = item.observationTick;
        }
    }
    std::sort(journal.entries.begin(), journal.entries.end(), [](const auto& l, const auto& r) {
        const auto key = [](const auto& v) {
            return std::tuple{v.observationTick,       v.domainToken, v.sourceClass.token(),
                              v.channel.token(),       v.action,      v.hasAmount,
                              v.amountCanonicalInteger};
        };
        return key(l.subject) < key(r.subject);
    });
    for (auto& entry : journal.entries) {
        if (journal.exhausted) {
            return core::unexpected(
                declarationInvalidError(codes::kDomainTokenPath, "observation ID exhausted"));
        }
        entry.observation.observationId = ObservationId{journal.nextId};
        if (journal.nextId == UINT64_MAX) {
            journal.exhausted = true;
        } else {
            ++journal.nextId;
        }
    }
    return journal;
} catch (const std::exception&) {
    return core::unexpected(
        declarationInvalidError(codes::kDomainTokenPath, "ingress journal allocation failed"));
}

auto detail::reserveIngressBatch(SessionIngressState& state, const IngressJournal& journal)
    -> core::Result<void> try {
    const auto count = journal.entries.size();
    if (count > state.admittedSequences_.max_size() - state.admittedSequences_.size() ||
        count > state.admittedSubjects_.max_size() - state.admittedSubjects_.size() ||
        count > state.ownedSubjects_.max_size() - state.ownedSubjects_.size()) {
        return core::unexpected(
            declarationInvalidError(codes::kDomainTokenPath, "ingress storage size overflow"));
    }
    state.admittedSequences_.reserve(state.admittedSequences_.size() + count);
    state.admittedSubjects_.reserve(state.admittedSubjects_.size() + count);
    state.ownedSubjects_.reserve(state.ownedSubjects_.size() + count);
    return {};
} catch (const std::exception&) {
    return core::unexpected(
        declarationInvalidError(codes::kDomainTokenPath, "ingress reservation failed"));
}

void detail::commitIngressBatch(SessionIngressState& state, IngressJournal&& journal) noexcept {
    for (auto& owner : journal.owners) {
        state.ownedSubjects_.push_back(std::move(owner));
    }
    for (const auto& entry : journal.entries) {
        state.admittedSequences_.push_back(entry.observation.ingressSequence);
        state.admittedSubjects_.push_back(entry.subject);
    }
    state.lastObservedTick_ = journal.latestTick;
    state.nextObservationId_ = journal.nextId;
    state.observationIdsExhausted_ = journal.exhausted;
}

auto normalizeObservation(SessionIngressState& state, const InputMappingProfile& mapping,
                          ObservationTick tick, const IngressDeclaration& declaration)
    -> core::Result<NormalizedObservationEntry> {
    const ClockedIngress input{tick, declaration};
    auto journal = detail::prepareIngressBatch(state, mapping, std::span{&input, 1}, false);
    if (!journal) {
        return core::unexpected(journal.error());
    }
    auto reserved = detail::reserveIngressBatch(state, *journal);
    if (!reserved) {
        return core::unexpected(reserved.error());
    }
    const auto result = journal->entries.front();
    detail::commitIngressBatch(state, std::move(*journal));
    return result;
}

//  ---------------------------------------------------------------------------------------------
//  Late-policy admission (Spec 3.7.4 item 4, CM-T10)
//  ---------------------------------------------------------------------------------------------

auto admitLateQueueEntry(const CanonicalIngressSubject& alreadyQueued,
                         const CanonicalIngressSubject& candidate) -> core::Result<void> {
    if (!(alreadyQueued == candidate)) {
        return {};
    }
    return core::unexpected(lateRejectionError(
        codes::kDuplicateQueueCode,
        "the canonical observation identity is already queued, so it is not queued again",
        codes::kCanonicalQueueKeyPath, codes::kAbsent));
}

auto hasDistinctIdentityAtSameTick(const CanonicalIngressSubject& queued,
                                   const CanonicalIngressSubject& candidate) noexcept -> bool {
    if (queued.observationTick != candidate.observationTick) {
        return false;
    }
    return !(queued == candidate);
}

} // namespace cuexis::judgement
