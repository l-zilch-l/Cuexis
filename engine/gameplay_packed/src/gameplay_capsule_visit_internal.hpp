#pragma once

// Typed field visitors shared by the writer, the reader and the sizing traversal.

#include "gameplay_capsule_adapters_internal.hpp"

namespace cuexis::gameplay_packed {
namespace capsule_detail {

template <typename A> void byte(A& a, std::uint8_t& v) {
    if constexpr (A::reading) {
        v = a.tag();
    } else {
        a.tag(v);
    }
}
template <typename A> void constant(A& a, std::uint8_t expected) {
    auto v = expected;
    byte(a, v);
    require(v == expected, "gameplay", "unsupported registry value");
}
template <typename E> auto registry();
template <> inline auto registry<PhaseKind>() {
    return std::array{PhaseKind::tap, PhaseKind::head, PhaseKind::body, PhaseKind::tail};
}
template <> inline auto registry<PatternPrimitive>() {
    return std::array{PatternPrimitive::atom,      PatternPrimitive::sequence,
                      PatternPrimitive::choice,    PatternPrimitive::boundedRepeat,
                      PatternPrimitive::skip,      PatternPrimitive::instant,
                      PatternPrimitive::complement};
}
template <> inline auto registry<DeclarationKind>() {
    return std::array{DeclarationKind::requirement,       DeclarationKind::patternDefinition,
                      DeclarationKind::measureDefinition, DeclarationKind::resourceRecord,
                      DeclarationKind::judgementDomain,   DeclarationKind::solverProfile};
}
template <> inline auto registry<ResourceClaimIntent>() {
    return std::array{ResourceClaimIntent::observe, ResourceClaimIntent::consume,
                      ResourceClaimIntent::claim};
}
template <> inline auto registry<GraceResolutionPolicy>() {
    return std::array{GraceResolutionPolicy::explicitDeclaration,
                      GraceResolutionPolicy::inheritedDeclaration,
                      GraceResolutionPolicy::defaultDeclaration};
}
template <> inline auto registry<LateEventPolicy>() {
    return std::array{LateEventPolicy::rejectLate, LateEventPolicy::queueNextTick};
}
template <> inline auto registry<InputAction>() {
    return std::array{InputAction::press, InputAction::release, InputAction::update};
}
template <typename A, typename E> void enumeration(A& a, E& v) {
    const auto table = registry<std::remove_cv_t<E>>();
    std::uint8_t ordinal{};
    if constexpr (!A::reading) {
        const auto it = std::find(table.begin(), table.end(), v);
        require(it != table.end(), "gameplay", "enum outside accepted registry");
        ordinal = static_cast<std::uint8_t>(it - table.begin() + 1);
    }
    byte(a, ordinal);
    require(ordinal > 0 && ordinal <= table.size(), "gameplay", "unknown wire enum");
    if constexpr (A::reading) {
        v = table[ordinal - 1];
    }
}
template <typename A, typename V> void visitFields(A& a, V& v, std::type_identity<Tick>) {
    auto n = v.value();
    a.value(n);
    if constexpr (A::reading) {
        v = Tick{n};
    }
}
template <typename A, typename V> void visitFields(A& a, V& v, std::type_identity<TickSpan>) {
    auto n = v.value();
    a.value(n);
    if constexpr (A::reading) {
        v = TickSpan{n};
    }
}
template <typename A, typename V> void visitFields(A& a, V& v, std::type_identity<PreparedGrace>) {
    auto n = v.span();
    a.value(n);
    if constexpr (A::reading) {
        v = PreparedGrace{n};
    }
}
template <typename A, typename V>
void visitFields(A& a, V& v, std::type_identity<StableDeclarationId>) {
    a.fields(v.sourceDocumentId, v.declarationOrdinal);
}
struct StructuralOutputAdapter final {
    Output& out;
    void write(std::span<const std::byte> bytes) {
        out.raw(bytes);
    }
    auto checkedCount(std::size_t value) const -> std::uint32_t {
        return u32(value);
    }
    auto tryEmission(const RequirementIdentity& identity) -> bool {
        if (out.operation && out.sizing)
            if (const auto* cost = out.operation->emission(identity)) {
                out.add(static_cast<std::size_t>(cost->bytes));
                return true;
            }
        return false;
    }
    void validateText(std::string_view text) const {
        if (!out.operation && !utf8(text))
            fail("packed.strings.bounds", "token is not valid UTF-8");
    }
};

template <typename A, typename V>
void visitFields(A& a, V& v, std::type_identity<EmissionPathStep>) {
    judgement::detail::emissionStepFields(a, v);
}
template <typename A, typename V>
void visitFields(A& a, V& v, std::type_identity<RequirementIdentity>) {
    if constexpr (std::is_same_v<A, Output>) {
        if (a.mode == Mode::hash) {
            if (a.operation && a.operation->counting && a.cost)
                if (const auto* cost = a.operation->emission(v)) {
                    a.operation->combine(a, *cost);
                    ++a.operation->observation.cacheHits;
                    return;
                }
            StructuralOutputAdapter adapter{a};
            judgement::detail::StructuralKeyEncoder encoder{adapter};
            judgement::detail::emissionIdentityFields(encoder, v);
            return;
        }
    }
    judgement::detail::emissionIdentityFields(a, v);
}

template <typename A, typename V>
void visitFields(A& a, V& v, std::type_identity<PhaseDeclaration>) {
    enumeration(a, v.kind);
    a.value(v.declarationOrdinal);
}
template <typename A, typename V> void visitFields(A& a, V& v, std::type_identity<PhaseTarget>) {
    enumeration(a, v.phase);
    a.value(v.chartTick);
}
template <typename A, typename V>
void visitFields(A& a, V& v, std::type_identity<AmountMatchRange>) {
    a.fields(v.minimum, v.maximum);
}
template <typename A, typename V> void visitFields(A& a, V& v, std::type_identity<AtomBinding>) {
    a.fields(v.atomRef, v.domainToken, v.sourceClass, v.channelToken);
    enumeration(a, v.action);
    a.fields(v.amountRange, v.tailOnly);
}
template <typename A, typename V> void visitFields(A& a, V& v, std::type_identity<TimeInterval>) {
    a.fields(v.start, v.end);
}
template <typename A, typename V>
void visitFields(A& a, V& v, std::type_identity<ClaimPolicyDeclaration::CompetitionKey>) {
    a.fields(v.priority, v.tieRank);
}
template <typename A, typename V> void visitFields(A& a, V& v, std::type_identity<FeatureRef>) {
    a.ref(7, v.featureId);
}
template <typename A, typename V>
void visitFields(A& a, V& v, std::type_identity<std::string_view>) {
    static_assert(!A::reading);
    a.text(v);
}
template <typename A, typename V> void visitFields(A& a, V& v, std::type_identity<ResourceRef>) {
    a.ref(10, v.resourceId);
}
inline void projectionFields(Output& a, std::uint8_t subdomain,
                             std::initializer_list<std::string_view> tokens,
                             const RequirementIdentity* identity) {
    StructuralOutputAdapter adapter{a};
    judgement::detail::StructuralKeyEncoder encoder{adapter};
    judgement::detail::structuralProjectionFields(encoder, subdomain, tokens, identity);
}

template <typename Emit> auto projection(Emit emit) -> std::string {
    Output a;
    a.mode = Mode::hash;
    a.limit = chart::PackedChartLimits{}.maxPackedSectionBytes;
    emit(a);
    return judgement::detail::formatStructuralProjectionKey(a.bufferedWriter().bytes(), [] {
        fail("packed.field.wire_range", "projection key hex length overflow");
    });
}

inline auto projection(std::uint8_t subdomain, std::initializer_list<std::string_view> tokens,
                       const RequirementIdentity* identity = nullptr) -> std::string {
    return projection([&](Output& out) { projectionFields(out, subdomain, tokens, identity); });
}

template <typename A, typename Emit> void projectedFromFields(A& a, std::uint8_t kind, Emit emit) {
    if constexpr (std::is_same_v<A, Output>) {
        if (a.operation) {
            Output count;
            count.mode = Mode::hash;
            count.sizing = true;
            count.operation = a.operation;
            emit(count);
            const auto length = judgement::detail::structuralProjectionKeySize(
                count.size, a.operation->limits.maxProjectionKeyBytes);
            if (length > a.operation->limits.maxProjectionKeyBytes)
                detail::hashBudget("maxProjectionKeyBytes", "semanticHash.projectionKeyBytes",
                                   a.operation->limits.maxProjectionKeyBytes, length, false);
            a.cost->maxKey = std::max(a.cost->maxKey, length);
            a.tag(kind);
            a.unsignedValue(length, false);
            if (a.operation->counting)
                a.add(static_cast<std::size_t>(length));
            else {
                const auto prefix = judgement::detail::kStructuralKeyPrefix;
                a.raw(std::as_bytes(std::span{prefix.data(), prefix.size()}));
                gp::SemanticByteSink hexSink{&a,
                                             [](void* context, auto bytes) {
                                                 auto& parent = *static_cast<Output*>(context);
                                                 judgement::detail::structuralHexChunks(
                                                     bytes, [&](auto chunk) { parent.raw(chunk); });
                                             },
                                             &a.operation->scratch};
                Output binary;
                binary.mode = Mode::hash;
                binary.operation = a.operation;
                binary.sink = &hexSink;
                emit(binary);
            }
            return;
        }
    }
    auto expected = projection(emit), actual = expected;
    a.ref(kind, actual);
    require(actual == expected, "REF0", "typed reference subdomain or structural key mismatch");
}

template <typename A>
void projected(A& a, std::uint8_t kind, std::uint8_t subdomain,
               std::initializer_list<std::string_view> tokens,
               const RequirementIdentity* identity = nullptr) {
    projectedFromFields(a, kind,
                        [&](Output& out) { projectionFields(out, subdomain, tokens, identity); });
}

inline void projectedClaim(Output& a, std::string_view explicitNamespace,
                           const RequirementIdentity& identity) {
    projectedFromFields(a, 11, [&](Output& out) {
        StructuralOutputAdapter adapter{out};
        judgement::detail::StructuralKeyEncoder encoder{adapter};
        judgement::detail::claimKeyFields(encoder, explicitNamespace, identity);
    });
}

template <typename A> void capabilityFields(A& a, std::string_view id, std::string_view revision) {
    projected(a, 13, 0, {id});
    bool present = !revision.empty();
    a.value(present);
    if (present)
        a.text(revision);
}
template <typename A, typename V> void visitFields(A& a, V& v, std::type_identity<CapabilityRef>) {
    if constexpr (A::reading) {
        std::string key;
        a.ref(13, key);
        require(key.starts_with(judgement::detail::kStructuralKeyPrefix) &&
                    std::string_view{key}.substr(judgement::detail::kStructuralKeyPrefix.size(),
                                                 2) == "00",
                "REF0", "capability requires subdomain zero");
        const auto encoded = key.substr(judgement::detail::kStructuralKeyPrefix.size());
        require(encoded.size() % 2 == 0, "REF0", "invalid reference hex");
        Bytes bytes;
        for (std::size_t i = 0; i < encoded.size(); i += 2) {
            const auto nibble = [](char c) -> unsigned {
                if (c >= '0' && c <= '9') {
                    return static_cast<unsigned>(c - '0');
                }
                if (c >= 'a' && c <= 'f') {
                    return static_cast<unsigned>(c - 'a' + 10);
                }
                fail("packed.references.invalid", "reference hex must be lowercase");
            };
            bytes.push_back(
                static_cast<std::byte>((nibble(encoded[i]) << 4) | nibble(encoded[i + 1])));
        }
        ByteReader r{bytes};
        require(checked(r.readU8()) == 0, "REF0", "invalid capability subdomain");
        const auto n = checked(r.readU32());
        const auto token = checked(r.readBytes(n));
        require(r.empty(), "REF0", "trailing capability reference bytes");
        v.capabilityId.assign(reinterpret_cast<const char*>(token.data()), token.size());
        require(!v.capabilityId.empty() && utf8(v.capabilityId), "REF0", "invalid capability ID");
        require(key == projection(0, {v.capabilityId}), "REF0", "noncanonical capability key");
        std::optional<std::string> revision;
        a.value(revision);
        require(!revision || !revision->empty(), "capability.revision",
                "present empty revision cannot become absence");
        v.revision = revision.value_or("");
    } else
        capabilityFields(a, v.capabilityId, v.revision);
}

template <typename A, typename V> void visitFields(A& a, V& v, std::type_identity<RequiredRefs>) {
    a.fields(v.features, v.capabilities);
}
template <typename A, typename R> void rational(A& a, R& v) {
    auto n = v.numerator();
    auto d = static_cast<std::uint64_t>(v.denominator());
    a.fields(n, d);
    require(d > 0 && d <= INT64_MAX, "timebase", "invalid rational denominator");
    if constexpr (A::reading) {
        v = checked(R::create(n, static_cast<std::int64_t>(d)));
        require(v.numerator() == n && static_cast<std::uint64_t>(v.denominator()) == d, "timebase",
                "rational is not reduced");
    }
}
template <typename A, typename V> void visitFields(A& a, V& v, std::type_identity<RationalBeat>) {
    rational(a, v);
}
template <typename A, typename V>
void visitFields(A& a, V& v, std::type_identity<RationalDuration>) {
    rational(a, v);
}
template <typename A, typename V> void visitFields(A& a, V& v, std::type_identity<TempoSection>) {
    a.fields(v.startBeat, v.durationPerBeat);
}
template <typename A, typename V> void visitFields(A& a, V& v, std::type_identity<StopSection>) {
    a.fields(v.startBeat, v.endBeat, v.duration);
}
template <typename A, typename V, typename T>
void visitFields(A& a, V& v, std::type_identity<std::vector<T>>) {
    a.list(v);
}
template <typename A, typename V, typename T>
void visitFields(A& a, V& v, std::type_identity<std::optional<T>>) {
    a.option(v);
}

template <typename A, typename V, typename T>
void measuredOption(A& a, V& v, std::type_identity<T>) {
    std::uint8_t tag{};
    if constexpr (!A::reading) {
        tag = !v ? 0 : v->measuredValue() ? 2 : 1;
    }
    byte(a, tag);
    require(tag <= 2, "measured", "unknown measured tag");
    if (tag == 2) {
        T value{};
        if constexpr (!A::reading) {
            value = *v->measuredValue();
        }
        a.value(value);
        if constexpr (A::reading) {
            v = MeasuredParameter<T>::measured(value);
        }
    } else if constexpr (A::reading) {
        if (tag == 0) {
            v.reset();
        } else {
            v = MeasuredParameter<T>::pendingMeasurement();
        }
    }
}
template <typename A, typename T> void measured(A& a, std::optional<MeasuredParameter<T>>& v) {
    measuredOption(a, v, std::type_identity<T>{});
}
template <typename A, typename T>
void measured(A& a, const std::optional<MeasuredParameter<T>>& v) {
    measuredOption(a, v, std::type_identity<T>{});
}
template <typename A, typename V> void measured(A& a, V& v) {
    std::optional option{v};
    measured(a, option);
    require(option.has_value(), "latePolicy", "late parameter cannot be absent");
    if constexpr (A::reading) {
        v = *option;
    }
}
template <typename A, typename V> void late(A& a, V& v) {
    bool present = v.policy.has_value();
    a.value(present);
    if (present) {
        auto policy = v.policy.value_or(LateEventPolicy::rejectLate);
        enumeration(a, policy);
        if constexpr (A::reading) {
            v.policy = policy;
        }
    } else if constexpr (A::reading) {
        v.policy.reset();
    }
    measured(a, v.finalizationWatermark);
    measured(a, v.maxQueueHop);
    measured(a, v.windowCloseThreshold);
    measured(a, v.windowOpenThreshold);
}
template <typename A, typename V, typename S> void timebase(A& a, V& v, S& id, S& unit) {
    a.fields(id, unit, v.tickScale, v.originBeat);
    bool initial = v.initialTempo.has_value();
    a.value(initial);
    if (initial) {
        auto tempo = v.initialTempo.value_or(checked(RationalDuration::create(1, 1)));
        a.value(tempo);
        if constexpr (A::reading) {
            v.initialTempo = tempo;
        }
    } else if constexpr (A::reading) {
        v.initialTempo.reset();
    }
    if constexpr (A::reading) {
        auto n = a.count();
        for (std::size_t i = 0; i < n; ++i) {
            TempoSection row{checked(RationalBeat::create(0, 1)),
                             checked(RationalDuration::create(1, 1))};
            a.value(row);
            v.tempoSections.push_back(std::move(row));
        }
        n = a.count();
        for (std::size_t i = 0; i < n; ++i) {
            StopSection row{checked(RationalBeat::create(0, 1)),
                            checked(RationalBeat::create(0, 1)),
                            checked(RationalDuration::create(1, 1))};
            a.value(row);
            v.stopSections.push_back(std::move(row));
        }
        v.profileId = id;
        v.unitToken = unit;
    } else {
        a.fields(v.tempoSections, v.stopSections);
    }
}

} // namespace capsule_detail
} // namespace cuexis::gameplay_packed
