#pragma once
#include "execution_kernel.hpp"
#include <bit>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace cuexis::judgement::detail::wire {
struct Failure final : std::runtime_error {
    using std::runtime_error::runtime_error;
};
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, EmissionPathStep>
void fields(A& a, V& v) {
    a(v.nodeId, v.repeatIndex);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, RequirementIdentity>
void fields(A& a, V& v) {
    a(v.chartEntryId, v.invocationId, v.moduleId, v.exportId, v.emissionPath, v.requirementLocalId);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, CanonicalObservationKey>
void fields(A& a, V& v) {
    a(v.observationTick, v.domainToken, v.sourceClass, v.channelToken, v.action, v.amount);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, CanonicalInput>
void fields(A& a, V& v) {
    a(v.key, v.sequence);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, InputReceiptPending>
void fields(A& a, V& v) {
    a(v.observationKey, v.dispatchTick, v.wasForwarded, v.admissionFrontier, v.admissionHorizon);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, AdmittedObservation>
void fields(A& a, V& v) {
    a(v.observationKey, v.dispatchTick, v.wasForwarded, v.admissionFrontier, v.admissionHorizon);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, ObservationOrigin>
void fields(A& a, V& v) {
    a(v.dispatchTick, v.observationKey);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, TimerOrigin>
void fields(A& a, V& v) {
    a(v.tick, v.requirement);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, CoordinationOrigin>
void fields(A& a, V& v) {
    a(v.dispatchTick, v.observationKey);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, CandidateKey>
void fields(A& a, V& v) {
    a(v.requirement, v.phase, v.origin, v.resourceId, v.intent);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, ContactHandle>
void fields(A& a, V& v) {
    a(v.domainToken, v.sourceClass, v.channelToken, v.startPressKey);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, SlotIdentity>
void fields(A& a, V& v) {
    a(v.resourceId, v.slotToken, v.ordinal);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, LeaseIdentity>
void fields(A& a, V& v) {
    a(v.slot, v.requirement, v.candidate);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, HeadContactOwnership>
void fields(A& a, V& v) {
    a(v.requirement, v.contact, v.acquiredTick, v.lease);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, Free>
void fields(A& a, V& v) {
    static_cast<void>(v);
    a();
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, Terminal>
void fields(A& a, V& v) {
    static_cast<void>(v);
    a();
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, Held>
void fields(A& a, V& v) {
    a(v.requirement, v.lease, v.contact);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, ResourceProjection>
void fields(A& a, V& v) {
    a(v.resourceId, v.state);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, PhaseProjection>
void fields(A& a, V& v) {
    a(v.requirement, v.phase, v.state);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, FactId>
void fields(A& a, V& v) {
    a(v.commitId, v.localOrdinal);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, LogicalCanonicalOrdinal>
void fields(A& a, V& v) {
    a(v.originScope, v.originOrdinal, v.localOrdinal, v.phaseRank, v.factKind);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, PhaseOutcomeFact>
void fields(A& a, V& v) {
    a(v.requirement, v.phase, v.outcome, v.category, v.error, v.evidence, v.originId, v.commitId,
      v.factId, v.commitTick, v.canonicalOrdinal, v.grade);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, ReceiptFact>
void fields(A& a, V& v) {
    a(v.kind, v.observationKey, v.originId, v.commitId, v.factId, v.commitTick, v.canonicalOrdinal);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, ObservationRecord>
void fields(A& a, V& v) {
    a(v.requirement, v.originKey, v.commitTick, v.observationKey, v.progressed, v.accepted);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, InputReceipt>
void fields(A& a, V& v) {
    a(v.observationKey, v.consideredCount, v.consumedBy, v.dispatchTick);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, HookValue>
void fields(A& a, V& v) {
    a(v.target, v.visibleTick, v.value);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, StatisticsCount>
void fields(A& a, V& v) {
    a(v.phase, v.outcome, v.grade, v.count);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, FoldProjection>
void fields(A& a, V& v) {
    a(v.score, v.combo, v.maxCombo, v.hits, v.misses, v.factCursor, v.workTick, v.produced,
      v.hookConsumerCursor, v.bonus, v.monoidValue, v.ledgerDerivedCount, v.counts, v.strayCount,
      v.consumeEmptyCount);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, KernelProjection>
void fields(A& a, V& v) {
    a(v.phases, v.resources, v.contacts, v.ownership, v.observers, v.receipts, v.facts,
      v.processedFrontier, v.lastAdvanceHorizon, v.state, v.faultDiagnostic, v.failedTick,
      v.requestedHorizon, v.kernelWorkTick, v.sealedFactCursor, v.fold,
      v.effectiveFactSemanticRevision, v.faultStage);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, MatcherSnapshot>
void fields(A& a, V& v) {
    a(v.states, v.epsilonAttempted);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, IngressSnapshot>
void fields(A& a, V& v) {
    a(v.accepted, v.lastObservedTick, v.nextId, v.exhausted);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, SnapshotDTO>
void fields(A& a, V& v) {
    a(v.kernel, v.matchers, v.coverageHistory, v.activeRequirements, v.timerCursor, v.nextCommitId,
      v.commitIdsExhausted, v.ingress, v.pending);
}
template <class A, class V>
    requires std::is_same_v<std::remove_cv_t<V>, ReplayRecord>
void fields(A& a, V& v) {
    a(v.batch, v.admission, v.horizon, v.result);
}

class Writer;
class Reader;
template <class T> void put(Writer&, const T&);
template <class T> void get(Reader&, T&);
class Writer final {
  public:
    std::vector<std::byte> bytes;
    template <class... T> void operator()(const T&... v) {
        (put(*this, v), ...);
    }
    void integer(std::uint64_t v, unsigned width) {
        for (unsigned i = 0; i < width; ++i) {
            bytes.push_back(static_cast<std::byte>((v >> (i * 8)) & 255U));
        }
    }
};
class Reader final {
  public:
    std::span<const std::byte> bytes;
    std::size_t pos{0};
    std::uint64_t elements{0};
    std::optional<CodecBudget> budget;
    KernelProjection initial;
    template <class... T> void operator()(T&... v) {
        (get(*this, v), ...);
    }
    auto integer(unsigned width) -> std::uint64_t {
        if (width > bytes.size() - pos) {
            throw Failure{"codec.truncated"};
        }
        std::uint64_t v = 0;
        for (unsigned i = 0; i < width; ++i) {
            v |= static_cast<std::uint64_t>(std::to_integer<unsigned>(bytes[pos++])) << (i * 8);
        }
        return v;
    }
    auto count() -> std::size_t {
        const auto n = integer(8);
        if (n > bytes.size() - pos || n > SIZE_MAX || n > UINT64_MAX - elements) {
            throw Failure{"codec.length_invalid"};
        }
        elements += n;
        if (budget && elements > budget->maxElements) {
            throw Failure{"codec.budget_exceeded"};
        }
        return static_cast<std::size_t>(n);
    }
    template <class T> auto seed() -> T {
        if constexpr (std::is_same_v<T, KernelProjection>) {
            return initial;
        } else if constexpr (std::is_same_v<T, core::Error>) {
            return core::Error{"", ""};
        } else if constexpr (std::is_same_v<T, SnapshotDTO>) {
            return SnapshotDTO{initial, {}, {}, {}, 0, 0, false, {}, {}};
        } else {
            return T{};
        }
    }
};
template <class T> struct Vector : std::false_type {};
template <class T, class A> struct Vector<std::vector<T, A>> : std::true_type {
    using Value = T;
};
template <class T> struct Optional : std::false_type {};
template <class T> struct Optional<std::optional<T>> : std::true_type {
    using Value = T;
};
template <class T> struct Variant : std::false_type {};
template <class... T> struct Variant<std::variant<T...>> : std::true_type {};
template <class T> struct Shared : std::false_type {};
template <class T> struct Shared<std::shared_ptr<T>> : std::true_type {
    using Value = std::remove_const_t<T>;
};
template <class T> constexpr auto maxEnum() -> unsigned {
    if constexpr (std::is_same_v<T, InputAction>) {
        return static_cast<unsigned>(InputAction::update);
    } else if constexpr (std::is_same_v<T, PhaseKind>) {
        return static_cast<unsigned>(PhaseKind::tail);
    } else if constexpr (std::is_same_v<T, Outcome>) {
        return static_cast<unsigned>(Outcome::miss);
    } else if constexpr (std::is_same_v<T, FactCategory>) {
        return static_cast<unsigned>(FactCategory::holdTail);
    } else if constexpr (std::is_same_v<T, ResourceClaimIntent>) {
        return static_cast<unsigned>(ResourceClaimIntent::claim);
    } else if constexpr (std::is_same_v<T, FaultStage>) {
        return static_cast<unsigned>(FaultStage::control);
    } else if constexpr (std::is_same_v<T, KernelSessionState>) {
        return static_cast<unsigned>(KernelSessionState::Faulted);
    } else if constexpr (std::is_same_v<T, KernelPhaseState>) {
        return static_cast<unsigned>(KernelPhaseState::Expired);
    } else if constexpr (std::is_same_v<T, FactKind>) {
        return static_cast<unsigned>(FactKind::consumeEmpty);
    } else {
        static_assert(!sizeof(T), "wire enum not registered");
    }
}
template <class T> void put(Writer& a, const T& v) {
    if constexpr (std::is_same_v<T, bool>) {
        a.integer(v ? 1 : 0, 1);
    } else if constexpr (std::is_same_v<T, std::byte>) {
        a.integer(std::to_integer<unsigned>(v), 1);
    } else if constexpr (std::is_integral_v<T>) {
        if constexpr (std::is_signed_v<T>) {
            a.integer(std::bit_cast<std::make_unsigned_t<T>>(v), sizeof(T));
        } else {
            a.integer(v, sizeof(T));
        }
    } else if constexpr (std::is_enum_v<T>) {
        a.integer(static_cast<unsigned>(v), 1);
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (!validUtf8(v)) {
            throw Failure{"codec.utf8_invalid"};
        }
        a.integer(static_cast<std::uint64_t>(v.size()), 8);
        for (const unsigned char c : v) {
            a.integer(c, 1);
        }
    } else if constexpr (std::is_same_v<T, Tick> || std::is_same_v<T, TickDelta> ||
                         std::is_same_v<T, IngressSequence>) {
        a(v.value());
    } else if constexpr (std::is_same_v<T, ObservationTick>) {
        a(v.tick());
    } else if constexpr (Vector<T>::value) {
        a.integer(static_cast<std::uint64_t>(v.size()), 8);
        for (const auto& x : v) {
            a(x);
        }
    } else if constexpr (Optional<T>::value || Shared<T>::value) {
        a(static_cast<bool>(v));
        if (v) {
            a(*v);
        }
    } else if constexpr (Variant<T>::value) {
        a.integer(v.index(), 1);
        std::visit([&](const auto& x) { a(x); }, v);
    } else if constexpr (std::is_same_v<T, core::Error>) {
        const auto* current = &v;
        while (current) {
            a(std::string{current->code()}, std::string{current->message()});
            a.integer(static_cast<std::uint64_t>(current->context().size()), 8);
            for (const auto& c : current->context()) {
                a(c.key, c.value);
            }
            a(current->cause() != nullptr);
            current = current->cause();
        }
    } else {
        fields(a, v);
    }
}
template <std::size_t I = 0, class T> void getVariant(Reader& a, T& v, unsigned tag) {
    if constexpr (I < std::variant_size_v<T>) {
        if (tag == I) {
            auto x = a.seed<std::variant_alternative_t<I, T>>();
            a(x);
            v = std::move(x);
        } else {
            getVariant<I + 1>(a, v, tag);
        }
    } else {
        throw Failure{"codec.tag_invalid"};
    }
}
template <class T> void get(Reader& a, T& v) {
    if constexpr (std::is_same_v<T, bool>) {
        const auto b = a.integer(1);
        if (b > 1) {
            throw Failure{"codec.tag_invalid"};
        }
        v = b != 0;
    } else if constexpr (std::is_same_v<T, std::byte>) {
        v = static_cast<std::byte>(a.integer(1));
    } else if constexpr (std::is_integral_v<T>) {
        auto bits = static_cast<std::make_unsigned_t<T>>(a.integer(sizeof(T)));
        if constexpr (std::is_signed_v<T>) {
            v = std::bit_cast<T>(bits);
        } else {
            v = bits;
        }
    } else if constexpr (std::is_enum_v<T>) {
        const auto tag = a.integer(1);
        if (tag > maxEnum<T>()) {
            throw Failure{"codec.tag_invalid"};
        }
        v = static_cast<T>(tag);
    } else if constexpr (std::is_same_v<T, std::string>) {
        const auto n = a.count();
        v.clear();
        v.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            v.push_back(static_cast<char>(a.integer(1)));
        }
        if (!validUtf8(v)) {
            throw Failure{"codec.utf8_invalid"};
        }
    } else if constexpr (std::is_same_v<T, Tick> || std::is_same_v<T, TickDelta>) {
        std::int64_t x;
        a(x);
        v = T{x};
    } else if constexpr (std::is_same_v<T, IngressSequence>) {
        std::uint64_t x;
        a(x);
        v = T{x};
    } else if constexpr (std::is_same_v<T, ObservationTick>) {
        Tick x;
        a(x);
        v = ObservationTick{x};
    } else if constexpr (Vector<T>::value) {
        const auto n = a.count();
        v.clear();
        if (n > v.max_size()) {
            throw Failure{"codec.length_invalid"};
        }
        v.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            auto x = a.seed<typename Vector<T>::Value>();
            a(x);
            v.push_back(std::move(x));
        }
    } else if constexpr (Optional<T>::value) {
        bool present;
        a(present);
        v.reset();
        if (present) {
            auto x = a.seed<typename Optional<T>::Value>();
            a(x);
            v = std::move(x);
        }
    } else if constexpr (Shared<T>::value) {
        bool present;
        a(present);
        v.reset();
        if (present) {
            auto x = a.seed<typename Shared<T>::Value>();
            a(x);
            v = std::make_shared<const typename Shared<T>::Value>(std::move(x));
        }
    } else if constexpr (Variant<T>::value) {
        getVariant(a, v, static_cast<unsigned>(a.integer(1)));
    } else if constexpr (std::is_same_v<T, core::Error>) {
        std::vector<core::Error> chain;
        bool cause = true;
        while (cause) {
            std::string code, message;
            a(code, message);
            core::Error error{std::move(code), std::move(message)};
            const auto n = a.count();
            for (std::size_t i = 0; i < n; ++i) {
                std::string k, value;
                a(k, value);
                error.withContext(std::move(k), std::move(value));
            }
            a(cause);
            chain.push_back(std::move(error));
        }
        v = std::move(chain.back());
        chain.pop_back();
        while (!chain.empty()) {
            auto parent = std::move(chain.back());
            chain.pop_back();
            parent.withCause(std::move(v));
            v = std::move(parent);
        }
    } else {
        fields(a, v);
    }
}
inline auto digest(std::span<const std::byte> bytes) noexcept -> std::uint64_t {
    std::uint64_t h = 14695981039346656037ULL;
    for (const auto b : bytes) {
        h = (h ^ std::to_integer<unsigned>(b)) * 1099511628211ULL;
    }
    return h;
}
} // namespace cuexis::judgement::detail::wire
