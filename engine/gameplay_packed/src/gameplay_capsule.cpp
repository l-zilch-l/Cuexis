#include <cuexis/gameplay_packed/gameplay_capsule.hpp>

#include "claim_key_internal.hpp"
#include "packed_file_internal.hpp"
#include "packed_gameplay_internal.hpp"
#include "packed_identity_internal.hpp"
#include "packed_limits_internal.hpp"

#include <cuexis/chart/packed_chart_primitives.hpp>
#include <cuexis/core/error.hpp>
#include <cuexis/filesystem/secure_file.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis_internal/sha256.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <exception>
#include <limits>
#include <map>
#include <set>
#include <tuple>

namespace cuexis::gameplay_packed {
namespace {
using namespace judgement;
using chart::packed::ByteReader;
using chart::packed::ByteWriter;
using chart::packed::crc32;
namespace gp = chart::packed::gameplay_detail;
using Bytes = std::vector<std::byte>;
using Ref = gp::Reference;
constexpr std::string_view kResolver = "grace.resolver.s2.v1";
constexpr std::string_view kClose = "local.close.t4.v1";

struct Failure final {
    core::Error error;
};
[[noreturn]] void fail(std::string_view code, std::string_view message,
                       std::string_view path = "gameplay") {
    throw Failure{core::Error{std::string{code}, std::string{message}}.withContext(
        "fieldPath", std::string{path})};
}
void require(bool condition, std::string_view path, std::string_view message) {
    if (!condition) {
        fail("packed.requirements.invalid", message, path);
    }
}
template <typename T> auto checked(core::Result<T> result) -> T {
    if (!result) {
        throw Failure{std::move(result.error())};
    }
    return std::move(*result);
}
void checked(core::Result<void> result) {
    if (!result) {
        throw Failure{std::move(result.error())};
    }
}
auto u32(std::size_t value) -> std::uint32_t {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        fail("packed.field.wire_range", "counter is outside the u32 domain");
    }
    return static_cast<std::uint32_t>(value);
}
void bound(std::size_t value, std::size_t limit, std::string_view code) {
    if (value > limit) {
        fail(code, "accepted Packed budget exceeded");
    }
}
auto utf8(std::string_view text) -> bool {
    std::uint32_t value = 0, minimum = 0;
    unsigned remaining = 0;
    for (const unsigned char byte : text) {
        if (remaining == 0) {
            if (byte < 0x80) {
                continue;
            }
            if (byte >= 0xc2 && byte <= 0xdf) {
                value = byte & 0x1fU;
                minimum = 0x80;
                remaining = 1;
            } else if (byte >= 0xe0 && byte <= 0xef) {
                value = byte & 0x0fU;
                minimum = 0x800;
                remaining = 2;
            } else if (byte >= 0xf0 && byte <= 0xf4) {
                value = byte & 7U;
                minimum = 0x10000;
                remaining = 3;
            } else {
                return false;
            }
        } else {
            if ((byte & 0xc0U) != 0x80U) {
                return false;
            }
            value = (value << 6U) | (byte & 0x3fU);
            if (--remaining == 0 &&
                (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))) {
                return false;
            }
        }
    }
    return remaining == 0;
}
auto textLess(std::string_view left, std::string_view right) -> bool {
    return std::lexicographical_compare(
        left.begin(), left.end(), right.begin(), right.end(), [](char l, char r) {
            return static_cast<unsigned char>(l) < static_cast<unsigned char>(r);
        });
}

// Shared visitors collect dictionaries, emit compact wire, or emit the fixed-width preimage.
// Sizing traverses the same fields without allocating output.
enum class Mode { collect, wire, hash };
struct Output final {
    static constexpr bool reading = false;
    Mode mode{Mode::wire};
    ByteWriter writer;
    bool sizing{};
    std::size_t size{};
    std::size_t limit{std::numeric_limits<std::size_t>::max()};
    std::set<std::string>* collected{};
    std::set<Ref>* collectedRefs{};
    const std::vector<std::string>* strings{};
    const std::vector<Ref>* references{};
    void add(std::size_t n) {
        if (n > limit - size) {
            fail("packed.budget.decoded_bytes", "expanded payload exceeds budget");
        }
        size += n;
    }
    void raw(std::span<const std::byte> bytes) {
        if (mode == Mode::collect) {
            return;
        }
        add(bytes.size());
        if (!sizing) {
            writer.writeBytes(bytes);
        }
    }
    void tag(std::uint8_t v) {
        if (mode == Mode::collect) {
            return;
        }
        add(1);
        if (!sizing) {
            writer.writeU8(v);
        }
    }
    void unsignedValue(std::uint64_t v, bool wide) {
        if (mode == Mode::collect) {
            return;
        }
        if (mode == Mode::hash) {
            add(wide ? 8 : 4);
            if (!sizing) {
                if (wide) {
                    writer.writeU64(v);
                } else {
                    writer.writeU32(static_cast<std::uint32_t>(v));
                }
            }
        } else {
            auto copy = v;
            std::size_t n = 1;
            while (copy >= 128) {
                copy >>= 7;
                ++n;
            }
            add(n);
            if (!sizing) {
                writer.writeUnsignedLeb128(v);
            }
        }
    }
    void value(std::uint32_t& v) {
        unsignedValue(v, false);
    }
    void value(std::uint64_t& v) {
        unsignedValue(v, true);
    }
    void value(std::int64_t& v) {
        if (mode == Mode::hash) {
            unsignedValue(std::bit_cast<std::uint64_t>(v), true);
        } else {
            unsignedValue((std::bit_cast<std::uint64_t>(v) << 1U) ^ (v < 0 ? UINT64_MAX : 0), true);
        }
    }
    void value(bool& v) {
        tag(v ? 1 : 0);
    }
    void text(std::string_view v) {
        if (!utf8(v)) {
            fail("packed.strings.bounds", "token is not valid UTF-8");
        }
        if (mode == Mode::collect) {
            collected->emplace(v);
            return;
        }
        if (mode == Mode::hash) {
            unsignedValue(u32(v.size()), false);
            raw(std::as_bytes(std::span{v.data(), v.size()}));
        } else {
            const auto it = std::lower_bound(strings->begin(), strings->end(), v,
                                             [](const auto& l, auto r) { return textLess(l, r); });
            require(it != strings->end() && *it == v, "STR0", "token missing from dictionary");
            unsignedValue(u32(static_cast<std::size_t>(it - strings->begin())), false);
        }
    }
    void value(std::string& v) {
        text(v);
    }
    void ref(std::uint8_t kind, std::string_view v) {
        if (mode == Mode::collect) {
            text(v);
            collectedRefs->emplace(kind, v);
            return;
        }
        if (mode == Mode::hash) {
            tag(kind);
            text(v);
            return;
        }
        Ref key{kind, std::string{v}};
        const auto it = std::lower_bound(references->begin(), references->end(), key);
        require(it != references->end() && *it == key, "REF0", "reference missing from dictionary");
        unsignedValue(u32(static_cast<std::size_t>(it - references->begin())), false);
    }
    void count(std::size_t n) {
        unsignedValue(u32(n), false);
    }
    template <typename T> void value(T& v);
    template <typename... T> void fields(T&... v) {
        (value(v), ...);
    }
    template <typename T> void list(std::vector<T>& v) {
        count(v.size());
        for (auto& row : v) {
            value(row);
        }
    }
    template <typename T> void option(std::optional<T>& v) {
        tag(v ? 1 : 0);
        if (v) {
            value(*v);
        }
    }
};
struct Input final {
    static constexpr bool reading = true;
    ByteReader reader;
    const std::vector<std::string>& strings;
    const std::vector<Ref>& references;
    std::string path;
    auto tag() -> std::uint8_t {
        return checked(reader.readU8());
    }
    void value(std::uint32_t& v) {
        v = static_cast<std::uint32_t>(checked(reader.readUnsignedLeb128(UINT32_MAX)));
    }
    void value(std::uint64_t& v) {
        v = checked(reader.readUnsignedLeb128());
    }
    void value(std::int64_t& v) {
        v = checked(reader.readSignedZigZag());
    }
    void value(bool& v) {
        const auto t = tag();
        require(t <= 1, path, "invalid boolean tag");
        v = t == 1;
    }
    void value(std::string& v) {
        std::uint32_t i{};
        value(i);
        require(i < strings.size(), path, "STR0 index out of range");
        v = strings[i];
    }
    void ref(std::uint8_t kind, std::string& v) {
        std::uint32_t i{};
        value(i);
        require(i < references.size() && references[i].first == kind, path,
                "REF0 index or kind mismatch");
        v = references[i].second;
    }
    auto count() -> std::size_t {
        std::uint32_t n{};
        value(n);
        require(n <= reader.remaining(), path, "count exceeds remaining bytes");
        return n;
    }
    template <typename T> void value(T& v);
    template <typename... T> void fields(T&... v) {
        (value(v), ...);
    }
    template <typename T> void list(std::vector<T>& v) {
        const auto n = count();
        v.clear();
        v.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            T row{};
            value(row);
            v.push_back(std::move(row));
        }
    }
    template <typename T> void option(std::optional<T>& v) {
        bool present{};
        value(present);
        if (present) {
            T row{};
            value(row);
            v = std::move(row);
        } else {
            v.reset();
        }
    }
};
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
template <> auto registry<PhaseKind>() {
    return std::array{PhaseKind::tap, PhaseKind::head, PhaseKind::body, PhaseKind::tail};
}
template <> auto registry<PatternPrimitive>() {
    return std::array{PatternPrimitive::atom,      PatternPrimitive::sequence,
                      PatternPrimitive::choice,    PatternPrimitive::boundedRepeat,
                      PatternPrimitive::skip,      PatternPrimitive::instant,
                      PatternPrimitive::complement};
}
template <> auto registry<DeclarationKind>() {
    return std::array{DeclarationKind::requirement,       DeclarationKind::patternDefinition,
                      DeclarationKind::measureDefinition, DeclarationKind::resourceRecord,
                      DeclarationKind::judgementDomain,   DeclarationKind::solverProfile};
}
template <> auto registry<ResourceClaimIntent>() {
    return std::array{ResourceClaimIntent::observe, ResourceClaimIntent::consume,
                      ResourceClaimIntent::claim};
}
template <> auto registry<GraceResolutionPolicy>() {
    return std::array{GraceResolutionPolicy::explicitDeclaration,
                      GraceResolutionPolicy::inheritedDeclaration,
                      GraceResolutionPolicy::defaultDeclaration};
}
template <> auto registry<LateEventPolicy>() {
    return std::array{LateEventPolicy::rejectLate, LateEventPolicy::queueNextTick};
}
template <> auto registry<InputAction>() {
    return std::array{InputAction::press, InputAction::release, InputAction::update};
}
template <typename A, typename E> void enumeration(A& a, E& v) {
    const auto table = registry<E>();
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
template <typename A> void visit(A& a, Tick& v) {
    auto n = v.value();
    a.value(n);
    if constexpr (A::reading) {
        v = Tick{n};
    }
}
template <typename A> void visit(A& a, TickSpan& v) {
    auto n = v.value();
    a.value(n);
    if constexpr (A::reading) {
        v = TickSpan{n};
    }
}
template <typename A> void visit(A& a, PreparedGrace& v) {
    auto n = v.span();
    a.value(n);
    if constexpr (A::reading) {
        v = PreparedGrace{n};
    }
}
template <typename A> void visit(A& a, StableDeclarationId& v) {
    a.fields(v.sourceDocumentId, v.declarationOrdinal);
}
template <typename A> void visit(A& a, EmissionPathStep& v) {
    a.fields(v.nodeId, v.repeatIndex);
}
template <typename A> void visit(A& a, RequirementIdentity& v) {
    a.fields(v.chartEntryId, v.invocationId, v.moduleId, v.exportId, v.emissionPath,
             v.requirementLocalId);
}
template <typename A> void visit(A& a, PhaseDeclaration& v) {
    enumeration(a, v.kind);
    a.value(v.declarationOrdinal);
}
template <typename A> void visit(A& a, PhaseTarget& v) {
    enumeration(a, v.phase);
    a.value(v.chartTick);
}
template <typename A> void visit(A& a, AmountMatchRange& v) {
    a.fields(v.minimum, v.maximum);
}
template <typename A> void visit(A& a, AtomBinding& v) {
    a.fields(v.atomRef, v.domainToken, v.sourceClass, v.channelToken);
    enumeration(a, v.action);
    a.fields(v.amountRange, v.tailOnly);
}
template <typename A> void visit(A& a, TimeInterval& v) {
    a.fields(v.start, v.end);
}
template <typename A> void visit(A& a, ClaimPolicyDeclaration::CompetitionKey& v) {
    a.fields(v.priority, v.tieRank);
}
template <typename A> void visit(A& a, FeatureRef& v) {
    a.ref(7, v.featureId);
}
template <typename A> void visit(A& a, ResourceRef& v) {
    a.ref(10, v.resourceId);
}
auto hex(std::span<const std::byte> bytes) -> std::string {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2);
    for (auto v : bytes) {
        const auto b = std::to_integer<unsigned>(v);
        result += digits[b >> 4];
        result += digits[b & 15];
    }
    return result;
}
auto projection(std::uint8_t subdomain, std::initializer_list<std::string_view> tokens,
                const RequirementIdentity* identity = nullptr) -> std::string {
    Output a;
    a.mode = Mode::hash;
    a.limit = chart::PackedChartLimits{}.maxPackedSectionBytes;
    a.tag(subdomain);
    for (auto token : tokens) {
        a.text(token);
    }
    if (identity) {
        auto copy = *identity;
        a.value(copy);
    }
    return "cxgp2:" + hex(a.writer.bytes());
}
template <typename A>
void projected(A& a, std::uint8_t kind, std::uint8_t subdomain,
               std::initializer_list<std::string_view> tokens,
               const RequirementIdentity* identity = nullptr) {
    auto expected = projection(subdomain, tokens, identity), actual = expected;
    a.ref(kind, actual);
    require(actual == expected, "REF0", "typed reference subdomain or structural key mismatch");
}
template <typename A> void visit(A& a, CapabilityRef& v) {
    if constexpr (A::reading) {
        std::string key;
        a.ref(13, key);
        require(key.starts_with("cxgp2:00"), "REF0", "capability requires subdomain zero");
        const auto encoded = key.substr(6);
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
    } else {
        projected(a, 13, 0, {v.capabilityId});
    }
    std::optional<std::string> revision;
    if constexpr (!A::reading) {
        if (!v.revision.empty()) {
            revision = v.revision;
        }
    }
    a.value(revision);
    if constexpr (A::reading) {
        require(!revision || !revision->empty(), "capability.revision",
                "present empty revision cannot become absence");
        v.revision = revision.value_or("");
    }
}
template <typename A> void visit(A& a, RequiredRefs& v) {
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
template <typename A> void visit(A& a, RationalBeat& v) {
    rational(a, v);
}
template <typename A> void visit(A& a, RationalDuration& v) {
    rational(a, v);
}
template <typename A> void visit(A& a, TempoSection& v) {
    a.fields(v.startBeat, v.durationPerBeat);
}
template <typename A> void visit(A& a, StopSection& v) {
    a.fields(v.startBeat, v.endBeat, v.duration);
}
template <typename T> void visit(Output& a, std::vector<T>& v) {
    a.list(v);
}
template <typename T> void visit(Input& a, std::vector<T>& v) {
    a.list(v);
}
template <typename T> void visit(Output& a, std::optional<T>& v) {
    a.option(v);
}
template <typename T> void visit(Input& a, std::optional<T>& v) {
    a.option(v);
}
template <typename A, typename T> void measured(A& a, std::optional<MeasuredParameter<T>>& v) {
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
template <typename A, typename T> void measured(A& a, MeasuredParameter<T>& v) {
    std::optional<MeasuredParameter<T>> option = v;
    measured(a, option);
    require(option.has_value(), "latePolicy", "late parameter cannot be absent");
    if constexpr (A::reading) {
        v = *option;
    }
}
template <typename A> void late(A& a, LatePolicyParameters& v) {
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
template <typename A> void timebase(A& a, TimebaseProfile& v, std::string& id, std::string& unit) {
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
struct RequirementIndices final {
    std::uint32_t entity{}, pattern{}, measure{}, domain{}, solver{};
    std::vector<std::uint32_t> claims, bindings;
};
struct ClaimRow final {
    std::uint32_t requirement{}, resource{};
    ResourceClaimDeclaration declaration;
    std::optional<std::string> deferredClaimKey;
};
struct Model final {
    chart::CanonicalSemanticChart chart;
    CanonicalGameplayGraph graph{};
    std::vector<chart::CanonicalEntityIdentity> entities, owners;
    std::vector<RequirementIndices> indices;
    std::vector<PatternDeclaration> patterns;
    std::vector<MeasureDefinition> measures;
    std::vector<ClaimRow> claims;
    std::vector<std::uint32_t> deferredRelationResources;
    CapsuleProfiles profiles;
    std::uint32_t candidateRevision{2};
    std::string resolver{kResolver}, close{kClose}, profileId, unit;
    TimebaseProfile timebase{
        {}, {}, checked(RationalDuration::create(1, 1)), checked(RationalBeat::create(0, 1)), {},
        {}, {}};
    LatePolicyParameters latePolicy{};
    std::array<std::uint32_t, 11> counts{};
    void bind() {
        timebase.profileId = profileId;
        timebase.unitToken = unit;
        graph.timebase = &timebase;
        graph.latePolicy = &latePolicy;
        if (candidateRevision == 3) {
            graph.normalizationProfileToken = profiles.normalizationProfileToken;
            graph.coordinatorPolicyToken = profiles.coordinatorPolicyToken;
        }
    }
};

template <typename A> void optionalRef(A& a, std::uint8_t kind, std::optional<std::string>& v) {
    bool present = v.has_value();
    a.value(present);
    if (present) {
        auto token = v.value_or("");
        a.ref(kind, token);
        require(!token.empty(), "definitions.id", "present definition ID must not be empty");
        if constexpr (A::reading) {
            v = std::move(token);
        }
    } else if constexpr (A::reading) {
        v.reset();
    }
}
auto nodes(PatternNodeDeclaration& root) -> std::vector<PatternNodeDeclaration*> {
    std::vector<PatternNodeDeclaration*> result, pending{&root};
    while (!pending.empty()) {
        auto* node = pending.back();
        pending.pop_back();
        result.push_back(node);
        for (auto i = node->operands.rbegin(); i != node->operands.rend(); ++i) {
            pending.push_back(&*i);
        }
    }
    return result;
}
template <typename A> void nodeFields(A& a, PatternNodeDeclaration& v, std::uint32_t& children) {
    enumeration(a, v.primitive);
    a.value(children);
    require(!v.unsupportedForm, "patterns.nodes", "unsupported Pattern form");
    switch (v.primitive) {
    case PatternPrimitive::atom:
        require(children == 0 && !v.repeatBounds, "patterns.atom", "atom must be a leaf");
        a.value(v.atomRef);
        require(!v.atomRef.empty(), "patterns.atom", "atom reference is missing");
        break;
    case PatternPrimitive::sequence:
    case PatternPrimitive::choice:
        require(children > 0 && v.atomRef.empty() && !v.repeatBounds, "patterns.operands",
                "sequence and choice require nonempty operands");
        break;
    case PatternPrimitive::boundedRepeat: {
        require(children == 1 && v.atomRef.empty(), "patterns.repeat", "repeat needs one operand");
        if constexpr (A::reading) {
            v.repeatBounds = RepeatBoundsDeclaration{};
        }
        require(v.repeatBounds.has_value(), "patterns.repeat", "repeat bounds are missing");
        a.fields(v.repeatBounds->minimum, v.repeatBounds->maximum);
        require(v.repeatBounds->minimum <= v.repeatBounds->maximum, "patterns.repeat",
                "repeat bounds are reversed");
        break;
    }
    case PatternPrimitive::complement:
        require(children == 1 && v.atomRef.empty() && !v.repeatBounds, "patterns.complement",
                "complement needs one operand");
        break;
    case PatternPrimitive::skip:
    case PatternPrimitive::instant:
        require(children == 0 && v.atomRef.empty() && !v.repeatBounds, "patterns.leaf",
                "empty-language primitive must be a leaf");
        break;
    }
}
template <typename A> void patternRow(A& a, PatternDeclaration& v) {
    std::optional<std::string> id;
    if constexpr (!A::reading) {
        if (!v.patternId.empty()) {
            id = v.patternId;
        }
    }
    optionalRef(a, 8, id);
    if constexpr (A::reading) {
        v.patternId = id.value_or("");
    }
    require(v.matchPolicy == MatchPolicy::leftmostFirst, "patterns.matchPolicy",
            "unsupported match policy");
    constant(a, 1);
    a.value(v.required);
    if constexpr (A::reading) {
        const auto n = a.count();
        require(n > 0, "patterns.nodes", "root node is missing");
        // An explicit stack consumes preorder without recursive calls or pointers invalidated by
        // growth.
        struct Parent {
            PatternNodeDeclaration* node;
            std::uint32_t remaining;
        };
        std::vector<Parent> stack;
        std::uint64_t pending = 1;
        for (std::size_t i = 0; i < n; ++i) {
            require(pending > 0, "patterns.nodes", "disconnected Pattern node");
            PatternNodeDeclaration* node = &v.root;
            if (i > 0) {
                while (!stack.empty() && stack.back().remaining == 0) {
                    stack.pop_back();
                }
                require(!stack.empty(), "patterns.nodes", "disconnected Pattern node");
                auto& parent = stack.back();
                parent.node->operands.emplace_back();
                node = &parent.node->operands.back();
                --parent.remaining;
            }
            std::uint32_t childCount{};
            nodeFields(a, *node, childCount);
            --pending;
            require(childCount <= n - i - 1 && pending <= n - i - 1 - childCount, "patterns.nodes",
                    "child count exceeds available nodes");
            pending += childCount;
            if (childCount > 0) {
                node->operands.reserve(childCount);
                stack.push_back({node, childCount});
            }
        }
        require(pending == 0, "patterns.nodes", "incomplete Pattern tree");
    } else {
        auto flat = nodes(v.root);
        a.count(flat.size());
        for (auto* node : flat) {
            auto n = u32(node->operands.size());
            nodeFields(a, *node, n);
        }
    }
}
template <typename A> void visit(A& a, MeasureComponentDeclaration& v) {
    enumeration(a, v.phase);
    a.fields(v.categoryToken, v.declaredGradeTokens);
}
template <typename A> void measureRow(A& a, MeasureDefinition& v) {
    optionalRef(a, 9, v.id);
    a.fields(v.declaration.components, v.declaration.required);
}
template <typename A> void visit(A& a, JudgementAxisRange& v) {
    a.fields(v.axisToken, v.minimum, v.maximum);
}
template <typename A> void domainRow(A& a, JudgementDomainRecord& v) {
    a.ref(4, v.domainId);
    a.fields(v.coordinateSystemToken, v.axes);
    constant(a, 1);
    require(v.frameResolution == FrameResolution::staticDeclaration &&
                v.dynamicFrameProviderToken.empty(),
            "domains.frame", "only static judgement geometry is accepted");
    a.fields(v.dynamicFrameProviderToken, v.required);
}
template <typename A> void mergedRow(A& a, MergedDeclaration& v) {
    a.value(v.stableId);
    enumeration(a, v.kind);
    a.fields(v.name, v.references);
    RequiredRefs refs;
    if constexpr (!A::reading) {
        for (const auto& token : v.requiredFeatureIds) {
            refs.features.push_back({token});
        }
        for (const auto& token : v.requiredCapabilityIds) {
            refs.capabilities.push_back({token, ""});
        }
    }
    a.value(refs);
    if constexpr (A::reading) {
        for (const auto& row : refs.features) {
            v.requiredFeatureIds.push_back(row.featureId);
        }
        for (const auto& row : refs.capabilities) {
            require(row.revision.empty(), "mergedDeclarations.required",
                    "merged declaration cannot carry an unrepresented revision");
            v.requiredCapabilityIds.push_back(row.capabilityId);
        }
    }
}
template <typename A> void resourceRow(A& a, ResourceRecord& v) {
    require(v.unsupportedForms.empty(), "resources", "unsupported resource form");
    a.fields(v.ref, v.declaredCapacity, v.slotToken, v.decisionPolicyRef,
             v.terminalAfterTermination, v.declaredGapGrace, v.required);
}
template <typename A> void solverRow(A& a, SolverProfileDeclaration& v) {
    a.fields(v.solverId, v.revision, v.algorithmToken, v.objective, v.tieBreak, v.rejectIfNonUnique,
             v.required);
    projected(a, 13, 5, {v.solverId, v.revision});
}
template <typename A> void bindingRow(A& a, FactBindingRef& v) {
    a.value(v.bindingId);
    projected(a, 13, 7, {v.bindingId});
}
template <typename A, typename T, typename F>
void index(A& a, std::uint32_t& i, std::vector<T>& table, F row) {
    if constexpr (A::reading) {
        a.value(i);
    } else if (a.mode == Mode::hash) {
        require(i < table.size(), "gameplay.index", "table index is out of range");
        row(a, table[i]);
    } else {
        a.value(i);
    }
}
template <typename A> constexpr auto defersGraphLinks() -> bool {
    if constexpr (requires { A::deferLinks; })
        return A::deferLinks;
    return false;
}
template <typename A> void relationRow(A& a, RelationDeclaration& v, Model& m) {
    require(v.kind == RelationKind::exclusive, "relations.kind", "unsupported relation kind");
    constant(a, 1);
    std::uint32_t resource{};
    if constexpr (!A::reading) {
        const auto it = std::find_if(m.graph.resources.begin(), m.graph.resources.end(),
                                     [&v](const auto& r) { return r.ref == v.resourceRef; });
        require(it != m.graph.resources.end(), "relations.resource", "dangling resource");
        resource = u32(static_cast<std::size_t>(it - m.graph.resources.begin()));
    }
    index(a, resource, m.graph.resources, [](auto& out, auto& r) { out.value(r.ref.resourceId); });
    if constexpr (A::reading) {
        if constexpr (defersGraphLinks<A>()) {
            m.deferredRelationResources.push_back(resource);
        } else {
            require(resource < m.graph.resources.size(), "relations.resource",
                    "resource index out of range");
            v.resourceRef = m.graph.resources[resource].ref;
        }
    }
    require(v.policy.claimKeyToken.empty() && !v.policy.competition, "relations.policy",
            "relation cannot carry requirement-side namespace or competition");
    a.fields(v.members, v.policy.policyToken, v.declaredCapacity, v.required);
}
template <typename A> void claimRow(A& a, ClaimRow& row, Model& m) {
    auto& v = row.declaration;
    index(a, row.requirement, m.graph.requirements,
          [](auto& out, auto& r) { out.value(r.identity); });
    index(a, row.resource, m.graph.resources,
          [](auto& out, auto& r) { out.value(r.ref.resourceId); });
    if constexpr (!defersGraphLinks<A>()) {
        require(row.requirement < m.graph.requirements.size() &&
                    row.resource < m.graph.resources.size(),
                "claims", "dangling claim owner or resource");
        if constexpr (A::reading)
            v.resourceRef = m.graph.resources[row.resource].ref;
    }
    enumeration(a, v.intent);
    a.value(v.claimPolicy.policyToken);
    std::optional<std::string> ns;
    if constexpr (!A::reading) {
        if (!v.claimPolicy.claimKeyToken.empty()) {
            ns = v.claimPolicy.claimKeyToken;
        }
    }
    a.value(ns);
    bool hasKey = v.intent != ResourceClaimIntent::observe;
    a.value(hasKey);
    require(hasKey == (v.intent != ResourceClaimIntent::observe), "claims.claimKey",
            "claim-key presence disagrees with intent");
    require(ns.has_value() == hasKey && (!ns || !ns->empty()), "claims.namespace",
            "namespace presence disagrees with intent");
    if (hasKey) {
        if constexpr (defersGraphLinks<A>()) {
            std::string key;
            a.ref(11, key);
            row.deferredClaimKey = std::move(key);
        } else {
            const auto expected = judgement::detail::structuralClaimKey(
                *ns, m.graph.requirements[row.requirement].identity);
            auto actual = expected;
            a.ref(11, actual);
            require(actual == expected, "claims.claimKey",
                    "claim key does not match namespace and E");
        }
    }
    a.value(v.claimPolicy.competition);
    require(v.claimPolicy.competition.has_value() == hasKey, "claims.competition",
            "competition presence disagrees with intent");
    std::optional<std::uint32_t> slot;
    if constexpr (!A::reading) {
        if (hasKey) {
            slot = 0;
        }
    }
    a.value(slot);
    require(slot.has_value() == hasKey && (!slot || *slot == 0), "claims.slot",
            "capacity-one claim must use slot zero");
    constant(a, 0);
    require(v.graceOverride.mode == GraceOverrideMode::none &&
                v.graceOverride.overrideToken.empty(),
            "claims.graceOverride", "unsupported grace override");
    if constexpr (A::reading) {
        v.claimPolicy.claimKeyToken = ns.value_or("");
    }
}
template <typename A> void visit(A& a, RequirementRecord::SuccessWindow& v) {
    a.fields(v.phase, v.start, v.end);
}
template <typename A> void visit(A& a, RequirementRecord::Timing& v) {
    a.fields(v.end, v.successWindows, v.body);
}
template <typename A> void executionFields(A& a, RequirementRecord& v) {
    require(v.timing.has_value(), "requirements.timing", "revision 3 requires timing");
    a.fields(v.timing->phaseTargets, v.atomBindings, v.independentCompetition);
}
template <typename A>
void requirementRow(A& a, RequirementRecord& v, RequirementIndices& ix, Model& m) {
    if constexpr (A::reading) {
        a.value(ix.entity);
    } else if (a.mode == Mode::hash) {
        require(ix.entity < m.entities.size(), "requirements.entity", "dangling entity");
        const auto bytes =
            checked(chart::packed::identity_detail::canonicalIdentityBytes(m.entities[ix.entity]));
        a.count(bytes.size());
        a.raw(bytes);
    } else {
        a.value(ix.entity);
    }
    a.fields(v.stableId, v.identity);
    projected(a, 12, 0, {}, &v.identity);
    projected(a, 13, 1, {}, &v.identity);
    if constexpr (A::reading) {
        const auto n = a.count();
        for (std::size_t i = 0; i < n; ++i) {
            std::string token;
            a.ref(5, token);
            v.requiredActions.push_back(checked(RequiredActionRef::fromToken(std::move(token))));
        }
        std::string token;
        a.value(token);
        v.domainBinding = checked(DomainBindingRef::fromToken(std::move(token)));
    } else {
        a.count(v.requiredActions.size());
        for (const auto& action : v.requiredActions) {
            a.ref(5, action.token());
        }
        a.text(v.domainBinding.token());
    }
    index(a, ix.domain, m.graph.judgementDomains, [](auto& out, auto& d) { domainRow(out, d); });
    a.fields(v.phases, v.requiresReleaseTailSemantics);
    index(a, ix.pattern, m.patterns, [](auto& out, auto& p) { patternRow(out, p); });
    measured(a, v.maxArmElements);
    measured(a, v.maxDeadlineElements);
    a.value(v.patternArmRefs);
    index(a, ix.measure, m.measures, [](auto& out, auto& w) { measureRow(out, w); });
    if constexpr (A::reading) {
        a.value(ix.claims);
    } else {
        a.count(ix.claims.size());
        for (auto& i : ix.claims) {
            index(a, i, m.claims, [&m](auto& out, auto& c) { claimRow(out, c, m); });
        }
    }
    a.fields(v.preparedGrace, v.localClosePolicyToken);
    index(a, ix.solver, m.graph.solverProfiles, [](auto& out, auto& s) { solverRow(out, s); });
    if constexpr (A::reading) {
        a.value(ix.bindings);
    } else {
        a.count(ix.bindings.size());
        for (auto& i : ix.bindings) {
            index(a, i, m.graph.factBindings, [](auto& out, auto& b) { bindingRow(out, b); });
        }
    }
    a.fields(v.required, v.timing);
    if constexpr (!A::reading) {
        if (a.mode == Mode::hash) {
            if (m.candidateRevision == 3) {
                executionFields(a, v);
            }
            return;
        }
    }
    enumeration(a, v.grace.policy);
    std::optional<std::string> inherited;
    if constexpr (!A::reading) {
        if (!v.grace.inheritedFromDeclarationId.empty()) {
            inherited = v.grace.inheritedFromDeclarationId;
        }
    }
    a.fields(inherited, v.grace.allowChartGrace);
    require(inherited.has_value() ==
                    (v.grace.policy == GraceResolutionPolicy::inheritedDeclaration) &&
                (!inherited || !inherited->empty()),
            "requirements.graceProvenance", "invalid inherited provenance");
    if constexpr (A::reading) {
        v.grace.inheritedFromDeclarationId = inherited.value_or("");
    }
    if (m.candidateRevision == 3) {
        executionFields(a, v);
    }
}
template <typename A> void globalRow(A& a, Model& m) {
    auto& g = m.graph;
    a.fields(g.gameplayVersion, g.graphRevision, g.rulesetRef);
    require(g.gameplayVersion == 2, "GPH0.gameplayVersion", "unsupported gameplay version");
    projected(a, 13, 8, {g.rulesetRef});
    a.value(m.profiles.normalizationProfileToken);
    projected(a, 13, 4, {m.profiles.normalizationProfileToken});
    a.value(m.profiles.coordinatorPolicyToken);
    projected(a, 13, 6, {m.profiles.coordinatorPolicyToken});
    if constexpr (A::reading) {
        a.value(m.resolver);
    } else if (a.mode != Mode::hash) {
        a.value(m.resolver);
    }
    a.value(m.close);
    // Projection precedes the timebase it identifies; decode defers the equality check until the ID
    // exists.
    if constexpr (A::reading) {
        std::string key;
        a.ref(13, key);
        timebase(a, m.timebase, m.profileId, m.unit);
        require(key == projection(2, {m.profileId}), "GPH0.timebaseProjection",
                "wrong timebase projection");
    } else {
        projected(a, 13, 2, {m.profileId});
        timebase(a, m.timebase, m.profileId, m.unit);
    }
    projected(a, 13, 3, {m.profileId});
    late(a, m.latePolicy);
    a.fields(g.declaredFeatures.features, g.declaredCapabilities.capabilities,
             g.closureContributions.features, g.closureContributions.capabilities,
             g.derivedFeatures.features, g.derivedCapabilities.capabilities,
             g.resourceClosure.resources);
    if constexpr (A::reading) {
        a.fields(g.sourceClosure.sourceDocumentIds, g.sourceClosure.compilerProfileToken);
        for (auto& n : m.counts) {
            a.value(n);
        }
    } else if (a.mode != Mode::hash) {
        a.fields(g.sourceClosure.sourceDocumentIds, g.sourceClosure.compilerProfileToken);
        for (auto& n : m.counts) {
            a.value(n);
        }
    }
    if (m.candidateRevision == 3) {
        a.value(g.executionProfile);
    }
}
template <typename A, typename T, typename F> void table(A& a, std::vector<T>& rows, F visitRow) {
    if constexpr (A::reading) {
        const auto n = a.count();
        rows.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            T row{};
            visitRow(a, row);
            rows.push_back(std::move(row));
        }
    } else {
        a.count(rows.size());
        for (auto& row : rows) {
            visitRow(a, row);
        }
    }
}
template <typename A> void definitions(A& a, Model& m) {
    table(a, m.patterns, [](auto& out, auto& p) { patternRow(out, p); });
    table(a, m.measures, [](auto& out, auto& w) { measureRow(out, w); });
    table(a, m.graph.judgementDomains, [](auto& out, auto& h) { domainRow(out, h); });
    table(a, m.graph.mergedNamespace.declarations, [](auto& out, auto& k) { mergedRow(out, k); });
}
template <typename A> void coordination(A& a, Model& m) {
    table(a, m.graph.resources, [](auto& out, auto& t) { resourceRow(out, t); });
    table(a, m.graph.relations, [&m](auto& out, auto& f) { relationRow(out, f, m); });
    table(a, m.graph.solverProfiles, [](auto& out, auto& y) { solverRow(out, y); });
    table(a, m.graph.factBindings, [](auto& out, auto& b) { bindingRow(out, b); });
    table(a, m.claims, [&m](auto& out, auto& x) { claimRow(out, x, m); });
}
template <typename T> void Output::value(T& v) {
    visit(*this, v);
}
template <typename T> void Input::value(T& v) {
    visit(*this, v);
}

#include "gameplay_graph_writer_internal.hpp"

auto refsKey(const RequiredRefs& v) {
    return std::tie(v.features, v.capabilities);
}
auto patternLess(const PatternDeclaration& l, const PatternDeclaration& r) -> bool {
    if (l.patternId != r.patternId) {
        return textLess(l.patternId, r.patternId);
    }
    if (refsKey(l.required) != refsKey(r.required)) {
        return refsKey(l.required) < refsKey(r.required);
    }
    std::vector<const PatternNodeDeclaration*> left{&l.root}, right{&r.root};
    while (!left.empty() && !right.empty()) {
        const auto* a = left.back();
        left.pop_back();
        const auto* b = right.back();
        right.pop_back();
        const auto ak = std::tuple{a->primitive, a->operands.size(), a->atomRef, a->repeatBounds};
        const auto bk = std::tuple{b->primitive, b->operands.size(), b->atomRef, b->repeatBounds};
        if (ak != bk) {
            return ak < bk;
        }
        for (auto i = a->operands.rbegin(); i != a->operands.rend(); ++i) {
            left.push_back(&*i);
        }
        for (auto i = b->operands.rbegin(); i != b->operands.rend(); ++i) {
            right.push_back(&*i);
        }
    }
    return left.empty() && !right.empty();
}
auto measureLess(const MeasureDefinition& l, const MeasureDefinition& r) -> bool {
    if (l.id != r.id) {
        return l.id < r.id;
    }
    if (l.declaration.components != r.declaration.components) {
        return l.declaration.components < r.declaration.components;
    }
    return refsKey(l.declaration.required) < refsKey(r.declaration.required);
}
template <typename T, typename F>
void ordered(const std::vector<T>& rows, F less, std::string_view path) {
    for (std::size_t i = 1; i < rows.size(); ++i) {
        require(less(rows[i - 1], rows[i]), path, "set must be strictly sorted and unique");
    }
}
template <typename T> void ordered(const std::vector<T>& rows, std::string_view path) {
    ordered(rows, std::less<T>{}, path);
}
void refsValid(const RequiredRefs& v) {
    ordered(v.features, "required.features");
    ordered(v.capabilities, "required.capabilities");
    for (const auto& r : v.features) {
        require(!r.featureId.empty(), "required.features", "empty feature ID");
    }
    for (const auto& r : v.capabilities) {
        require(!r.capabilityId.empty(), "required.capabilities", "empty capability ID");
    }
}
void counts(Model& m, std::size_t referenceCount) {
    const auto& g = m.graph;
    m.counts = {u32(g.requirements.size()),
                u32(m.patterns.size()),
                u32(m.measures.size()),
                u32(g.judgementDomains.size()),
                u32(g.mergedNamespace.declarations.size()),
                u32(g.resources.size()),
                u32(g.relations.size()),
                u32(g.solverProfiles.size()),
                u32(g.factBindings.size()),
                u32(m.claims.size()),
                u32(referenceCount)};
}
template <typename T, typename F>
auto findIndex(const std::vector<T>& rows, F predicate, std::string_view path) -> std::uint32_t {
    const auto it = std::find_if(rows.begin(), rows.end(), predicate);
    require(it != rows.end(), path, "dangling table reference");
    return u32(static_cast<std::size_t>(it - rows.begin()));
}
auto namedMeasure(const RequirementRecord& r, const Model& m) -> std::optional<std::string> {
    const auto declaration = std::find_if(m.graph.mergedNamespace.declarations.begin(),
                                          m.graph.mergedNamespace.declarations.end(),
                                          [&r](const auto& d) { return d.stableId == r.stableId; });
    require(declaration != m.graph.mergedNamespace.declarations.end(), "requirements.stableId",
            "missing declaration");
    std::optional<std::string> result;
    for (const auto& ref : declaration->references) {
        const auto d = std::find_if(m.graph.mergedNamespace.declarations.begin(),
                                    m.graph.mergedNamespace.declarations.end(),
                                    [&ref](const auto& k) { return k.stableId == ref; });
        require(d != m.graph.mergedNamespace.declarations.end(), "mergedDeclarations.references",
                "dangling declaration");
        if (d->kind == DeclarationKind::measureDefinition) {
            require(!result, "requirements.measure", "multiple named Measure references");
            result = d->name;
        }
    }
    return result;
}
void validate(Model& m, chart::PackedChartLimits limits,
              const PatternCompileBudget& patternBudget = {}) {
    auto& g = m.graph;
    m.bind();
    require(m.resolver == kResolver && m.close == kClose, "GPH0.policy",
            "unsupported resolver or close algorithm");
    require(!m.profiles.normalizationProfileToken.empty() &&
                !m.profiles.coordinatorPolicyToken.empty(),
            "GPH0.profiles", "explicit normalization and coordinator profiles are required");
    bound(g.requirements.size(), limits.maxPackedRequirements, "packed.budget.requirements");
    require(m.indices.size() == g.requirements.size(), "GPR0", "requirement-index count mismatch");
    ordered(
        g.requirements, [](const auto& l, const auto& r) { return l.identity < r.identity; },
        "GPR0");
    ordered(m.patterns, patternLess, "GPD0.patterns");
    ordered(m.measures, measureLess, "GPD0.measures");
    ordered(
        g.resources, [](const auto& l, const auto& r) { return l.ref < r.ref; }, "GRC0.resources");
    ordered(
        g.relations,
        [](const auto& l, const auto& r) {
            return std::tie(l.resourceRef, l.kind, l.members, l.policy.policyToken) <
                   std::tie(r.resourceRef, r.kind, r.members, r.policy.policyToken);
        },
        "GRC0.relations");
    ordered(
        g.solverProfiles,
        [](const auto& l, const auto& r) {
            return std::tie(l.solverId, l.revision) < std::tie(r.solverId, r.revision);
        },
        "GRC0.solverProfiles");
    ordered(g.factBindings, "GRC0.factBindings");
    ordered(
        g.judgementDomains,
        [](const auto& l, const auto& r) { return textLess(l.domainId, r.domainId); },
        "GPD0.domains");
    ordered(
        g.mergedNamespace.declarations,
        [](const auto& l, const auto& r) { return l.stableId < r.stableId; },
        "GPD0.mergedDeclarations");
    ordered(g.sourceClosure.sourceDocumentIds, "GPH0.sourceClosure");
    refsValid({g.declaredFeatures.features, g.declaredCapabilities.capabilities});
    refsValid({g.derivedFeatures.features, g.derivedCapabilities.capabilities});
    refsValid({g.closureContributions.features, g.closureContributions.capabilities});
    ordered(g.resourceClosure.resources, "GPH0.resourceClosure");
    require(g.derivedFeatures == deriveFeatureClosure(g) &&
                g.derivedCapabilities == deriveCapabilityClosure(g) &&
                g.resourceClosure == deriveResourceClosure(g),
            "GPH0.closure", "derived closure does not match records");
    std::vector<std::string> features;
    for (const auto& f : m.chart.features) {
        features.push_back(f.id);
    }
    std::sort(features.begin(), features.end());
    std::vector<std::string> derived;
    for (const auto& f : g.derivedFeatures.features) {
        derived.push_back(f.featureId);
    }
    require(features == derived, "META.requiredFeatures",
            "META and Gameplay feature closures differ");
    std::set<std::string> names, patternNames, measureNames;
    const auto definitionNeeds = [&g](const RequiredRefs& required) {
        refsValid(required);
        for (const auto& feature : required.features) {
            require(std::binary_search(g.declaredFeatures.features.begin(),
                                       g.declaredFeatures.features.end(), feature) &&
                        std::binary_search(g.derivedFeatures.features.begin(),
                                           g.derivedFeatures.features.end(), feature),
                    "GPD0.required.features",
                    "definition feature is missing from the prepared closure");
        }
        for (const auto& capability : required.capabilities) {
            require(std::binary_search(g.declaredCapabilities.capabilities.begin(),
                                       g.declaredCapabilities.capabilities.end(), capability) &&
                        std::binary_search(g.derivedCapabilities.capabilities.begin(),
                                           g.derivedCapabilities.capabilities.end(), capability),
                    "GPD0.required.capabilities",
                    "definition capability is missing from the prepared closure");
        }
    };
    for (const auto& p : m.patterns) {
        definitionNeeds(p.required);
        if (!p.patternId.empty()) {
            require(patternNames.insert(p.patternId).second, "patterns.id",
                    "conflicting named Pattern");
        }
        checked(compilePattern(p, patternBudget));
    }
    for (const auto& w : m.measures) {
        definitionNeeds(w.declaration.required);
        ordered(
            w.declaration.components,
            [](const auto& l, const auto& r) {
                return std::tie(l.phase, l.categoryToken) < std::tie(r.phase, r.categoryToken);
            },
            "measures.components");
        if (w.id) {
            require(!w.id->empty() && measureNames.insert(*w.id).second, "measures.id",
                    "conflicting named Measure");
        }
        MeasurePhaseContext context{.declaredPhases = {},
                                    .requiresReleaseTailSemantics = false,
                                    .fieldPathPrefix = "GPD0.measures"};
        for (const auto& c : w.declaration.components) {
            context.declaredPhases.push_back({c.phase, 0});
        }
        checked(compileMeasure(w.declaration, context));
    }
    for (const auto& d : g.judgementDomains) {
        refsValid(d.required);
        ordered(
            d.axes, [](const auto& l, const auto& r) { return textLess(l.axisToken, r.axisToken); },
            "domains.axes");
        checked(validateJudgementDomain(d));
    }
    for (const auto& d : g.mergedNamespace.declarations) {
        require(!d.name.empty() && names.insert(d.name).second, "mergedDeclarations.name",
                "conflicting declaration name");
        ordered(d.references, "mergedDeclarations.references");
        ordered(d.requiredFeatureIds, "mergedDeclarations.features");
        ordered(d.requiredCapabilityIds, "mergedDeclarations.capabilities");
        for (const auto& ref : d.references) {
            require(std::any_of(g.mergedNamespace.declarations.begin(),
                                g.mergedNamespace.declarations.end(),
                                [&ref](const auto& k) { return k.stableId == ref; }),
                    "mergedDeclarations.references", "dangling declaration");
        }
        bool exists = false;
        switch (d.kind) {
        case DeclarationKind::requirement:
            exists =
                std::any_of(g.requirements.begin(), g.requirements.end(), [&d, &g](const auto& r) {
                    return r.stableId == d.stableId &&
                           (!g.executionProfile.empty() || r.identity.requirementLocalId == d.name);
                });
            break;
        case DeclarationKind::patternDefinition:
            exists = patternNames.contains(d.name);
            break;
        case DeclarationKind::measureDefinition:
            exists = measureNames.contains(d.name);
            break;
        case DeclarationKind::resourceRecord:
            exists = std::any_of(g.resources.begin(), g.resources.end(),
                                 [&d](const auto& r) { return r.ref.resourceId == d.name; });
            break;
        case DeclarationKind::judgementDomain:
            exists = std::any_of(g.judgementDomains.begin(), g.judgementDomains.end(),
                                 [&d](const auto& h) { return h.domainId == d.name; });
            break;
        case DeclarationKind::solverProfile:
            exists = std::any_of(g.solverProfiles.begin(), g.solverProfiles.end(),
                                 [&d](const auto& s) { return s.solverId == d.name; });
            break;
        }
        require(exists, "mergedDeclarations.kind", "declaration does not locate its typed record");
    }
    for (const auto& p : patternNames) {
        require(std::any_of(g.mergedNamespace.declarations.begin(),
                            g.mergedNamespace.declarations.end(),
                            [&p](const auto& d) {
                                return d.kind == DeclarationKind::patternDefinition && d.name == p;
                            }),
                "patterns.id", "named Pattern is not declared");
    }
    for (const auto& w : measureNames) {
        require(std::any_of(g.mergedNamespace.declarations.begin(),
                            g.mergedNamespace.declarations.end(),
                            [&w](const auto& d) {
                                return d.kind == DeclarationKind::measureDefinition && d.name == w;
                            }),
                "measures.id", "named Measure is not declared");
    }
    ordered(
        m.claims,
        [&m](const auto& l, const auto& r) {
            return std::tie(m.graph.resources[l.resource].ref,
                            m.graph.requirements[l.requirement].identity) <
                   std::tie(m.graph.resources[r.resource].ref,
                            m.graph.requirements[r.requirement].identity);
        },
        "GRC0.claims");
    std::vector<unsigned> usedClaims(m.claims.size()), usedOwners(m.entities.size());
    for (std::size_t i = 0; i < g.requirements.size(); ++i) {
        auto& r = g.requirements[i];
        const auto& ix = m.indices[i];
        require(ix.entity < m.entities.size() && ix.domain < g.judgementDomains.size() &&
                    ix.pattern < m.patterns.size() && ix.measure < m.measures.size() &&
                    ix.solver < g.solverProfiles.size(),
                "GPR0.indices", "table index out of range");
        ++usedOwners[ix.entity];
        require(r.unsupportedForms.empty() && r.localClosePolicyToken == m.close,
                "GPR0.localClosePolicy", "unsupported Requirement representation or policy");
        refsValid(r.required);
        ordered(r.phases, "requirements.phases");
        ordered(r.patternArmRefs, "requirements.patternArmRefs");
        if (m.candidateRevision == 3) {
            ordered(
                r.atomBindings,
                [](const auto& l, const auto& rr) { return textLess(l.atomRef, rr.atomRef); },
                "requirements.atomBindings");
            require(r.timing.has_value(), "requirements.timing", "execution timing absent");
            ordered(
                r.timing->phaseTargets,
                [](const auto& l, const auto& rr) { return l.phase < rr.phase; },
                "requirements.phaseTargets");
        }
        ordered(
            r.requiredActions,
            [](const auto& l, const auto& rr) { return textLess(l.token(), rr.token()); },
            "requirements.requiredActions");
        ordered(r.factBindingRefs, "requirements.factBindings");
        if (r.timing) {
            ordered(
                r.timing->successWindows,
                [](const auto& l, const auto& rr) {
                    return std::tie(l.phase, l.start, l.end) < std::tie(rr.phase, rr.start, rr.end);
                },
                "requirements.timing.successWindows");
        }
        require(r.pattern == m.patterns[ix.pattern] &&
                    r.measure == m.measures[ix.measure].declaration &&
                    namedMeasure(r, m) == m.measures[ix.measure].id &&
                    r.judgementDomainId == g.judgementDomains[ix.domain].domainId &&
                    r.solverProfileRef == g.solverProfiles[ix.solver].solverId,
                "GPR0.indices", "referenced definition differs from Requirement");
        require(ix.claims.size() == r.resourceClaims.size() && ix.claims.size() <= 1 &&
                    ix.bindings.size() == r.factBindingRefs.size(),
                "GPR0.claims", "reference count mismatch or fanout exceeds one");
        for (std::size_t j = 0; j < ix.claims.size(); ++j) {
            const auto c = ix.claims[j];
            require(c < m.claims.size() && m.claims[c].requirement == i &&
                        m.claims[c].declaration == r.resourceClaims[j],
                    "GPR0.claims", "claim is not owned by Requirement");
            ++usedClaims[c];
        }
        for (std::size_t j = 0; j < ix.bindings.size(); ++j) {
            require(ix.bindings[j] < g.factBindings.size() &&
                        g.factBindings[ix.bindings[j]].bindingId == r.factBindingRefs[j],
                    "GPR0.factBindings", "binding index mismatch");
        }
    }
    require(std::all_of(usedClaims.begin(), usedClaims.end(), [](auto n) { return n == 1; }),
            "GRC0.claims", "orphaned or multiply-owned claim");
    for (const auto& t : g.resources) {
        refsValid(t.required);
    }
    for (const auto& f : g.relations) {
        refsValid(f.required);
        ordered(f.members, "relations.members");
    }
    for (const auto& s : g.solverProfiles) {
        refsValid(s.required);
    }
}
auto makeModel(const EncodeRequest& request, chart::PackedChartLimits limits) -> Model {
    Model m;
    m.chart = request.chart;
    m.graph = request.gameplay.assembled().graph;
    m.candidateRevision = request.candidateRevision;
    require(m.candidateRevision == 2 || m.candidateRevision == 3, "candidateRevision",
            "unsupported Writer revision");
    if (m.candidateRevision == 2) {
        require(m.graph.executionProfile.empty() && m.graph.normalizationProfileToken.empty() &&
                    m.graph.coordinatorPolicyToken.empty(),
                "executionProfile", "revision 2 Writer cannot discard execution fields");
        for (const auto& r : m.graph.requirements) {
            require(r.atomBindings.empty() && !r.independentCompetition &&
                        (!r.timing || r.timing->phaseTargets.empty()),
                    "requirements", "revision 2 Writer cannot discard execution fields");
        }
    } else {
        require(m.graph.executionProfile == "gameplay.execution.t4-k4.v1" &&
                    m.graph.normalizationProfileToken ==
                        request.profiles.normalizationProfileToken &&
                    m.graph.coordinatorPolicyToken == request.profiles.coordinatorPolicyToken,
                "executionProfile", "revision 3 graph and Writer profiles must agree");
    }
    require(m.graph.timebase && m.graph.latePolicy, "GPH0.timebase", "missing prepared timebase");
    m.timebase = *m.graph.timebase;
    m.latePolicy = *m.graph.latePolicy;
    m.profileId = std::string{m.timebase.profileId};
    m.unit = std::string{m.timebase.unitToken};
    m.profiles = request.profiles;
    m.bind();
    for (const auto& entity : m.chart.entities) {
        m.entities.push_back(entity.identity);
    }
    std::sort(m.entities.begin(), m.entities.end(), [](const auto& l, const auto& r) {
        return chart::packed::identity_detail::ByteKeyLess{}(
            checked(chart::packed::identity_detail::canonicalIdentityBytes(l)),
            checked(chart::packed::identity_detail::canonicalIdentityBytes(r)));
    });
    require(request.owners.size() == m.graph.requirements.size(), "GPR0.entity",
            "explicit ownership must cover every Requirement exactly once");
    std::set<RequirementIdentity> ownerIds;
    for (const auto& owner : request.owners) {
        require(ownerIds.insert(owner.requirement).second, "GPR0.entity", "duplicate ownership");
        require(std::any_of(m.graph.requirements.begin(), m.graph.requirements.end(),
                            [&owner](const auto& r) { return r.identity == owner.requirement; }),
                "GPR0.entity", "ownership names no Requirement");
    }
    m.patterns.assign(request.additionalPatterns.begin(), request.additionalPatterns.end());
    m.measures.assign(request.additionalMeasures.begin(), request.additionalMeasures.end());
    for (const auto& r : m.graph.requirements) {
        m.patterns.push_back(r.pattern);
        const auto id = namedMeasure(r, m);
        if (id) {
            require(std::any_of(m.measures.begin(), m.measures.end(),
                                [&id, &r](const auto& w) {
                                    return w.id == id && w.declaration == r.measure;
                                }),
                    "requirements.measure", "named Measure definition must be explicitly supplied");
        } else {
            m.measures.push_back({std::nullopt, r.measure});
        }
    }
    std::sort(m.patterns.begin(), m.patterns.end(), patternLess);
    m.patterns.erase(std::unique(m.patterns.begin(), m.patterns.end()), m.patterns.end());
    std::sort(m.measures.begin(), m.measures.end(), measureLess);
    m.measures.erase(std::unique(m.measures.begin(), m.measures.end()), m.measures.end());
    for (std::size_t i = 0; i < m.graph.requirements.size(); ++i) {
        const auto& r = m.graph.requirements[i];
        const auto owner =
            std::find_if(request.owners.begin(), request.owners.end(),
                         [&r](const auto& o) { return o.requirement == r.identity; });
        RequirementIndices ix;
        ix.entity = findIndex(
            m.entities, [&owner](const auto& e) { return e == owner->entity; },
            "requirements.entity");
        if (std::find(m.owners.begin(), m.owners.end(), owner->entity) == m.owners.end()) {
            m.owners.push_back(owner->entity);
        }
        ix.pattern = findIndex(
            m.patterns, [&r](const auto& p) { return p == r.pattern; }, "requirements.pattern");
        ix.measure = findIndex(
            m.measures,
            [&r, &m](const auto& w) {
                return w.id == namedMeasure(r, m) && w.declaration == r.measure;
            },
            "requirements.measure");
        ix.domain = findIndex(
            m.graph.judgementDomains,
            [&r](const auto& d) { return d.domainId == r.judgementDomainId; },
            "requirements.domain");
        ix.solver = findIndex(
            m.graph.solverProfiles,
            [&r](const auto& s) { return s.solverId == r.solverProfileRef; },
            "requirements.solver");
        for (const auto& c : r.resourceClaims) {
            const auto resource = findIndex(
                m.graph.resources, [&c](const auto& t) { return t.ref == c.resourceRef; },
                "claims.resource");
            m.claims.push_back({u32(i), resource, c});
        }
        for (const auto& b : r.factBindingRefs) {
            ix.bindings.push_back(findIndex(
                m.graph.factBindings, [&b](const auto& s) { return s.bindingId == b; },
                "requirements.factBinding"));
        }
        m.indices.push_back(std::move(ix));
    }
    std::sort(m.claims.begin(), m.claims.end(), [&m](const auto& l, const auto& r) {
        return std::tie(m.graph.resources[l.resource].ref,
                        m.graph.requirements[l.requirement].identity) <
               std::tie(m.graph.resources[r.resource].ref,
                        m.graph.requirements[r.requirement].identity);
    });
    for (std::size_t i = 0; i < m.claims.size(); ++i) {
        m.indices[m.claims[i].requirement].claims.push_back(u32(i));
    }
    validate(m, limits);
    return m;
}

template <typename F>
auto output(Mode mode, Model& m, chart::PackedChartLimits limits,
            const std::vector<std::string>* strings, const std::vector<Ref>* references,
            F visitRows) -> Bytes {
    Output sizing;
    sizing.mode = mode;
    sizing.sizing = true;
    sizing.limit = mode == Mode::hash ? limits.maxPackedDecodedBytes : limits.maxPackedSectionBytes;
    sizing.strings = strings;
    sizing.references = references;
    visitRows(sizing, m);
    Output out;
    out.mode = mode;
    out.limit = sizing.limit;
    out.strings = strings;
    out.references = references;
    visitRows(out, m);
    return std::move(out.writer).takeBytes();
}
void allRows(Output& a, Model& m) {
    globalRow(a, m);
    for (std::size_t i = 0; i < m.graph.requirements.size(); ++i) {
        requirementRow(a, m.graph.requirements[i], m.indices[i], m);
    }
    definitions(a, m);
    coordination(a, m);
}
auto staticTables(Model& m, chart::PackedChartLimits limits) -> gp::StaticTables {
    std::set<std::string> strings;
    std::set<Ref> references;
    Output collect;
    collect.mode = Mode::collect;
    collect.collected = &strings;
    collect.collectedRefs = &references;
    allRows(collect, m);
    std::vector<std::string> s{strings.begin(), strings.end()};
    std::vector<Ref> r{references.begin(), references.end()};
    auto result = checked(gp::encodeStatic(m.chart, m.owners, s, r, limits));
    counts(m, result.references.size());
    return result;
}
auto preimage(Model& m, chart::PackedChartLimits limits) -> Bytes {
    m.bind();
    auto prefix = checked(gp::staticPreimage(m.chart, m.owners, m.candidateRevision));
    bound(prefix.size(), limits.maxPackedDecodedBytes, "packed.budget.decoded_bytes");
    const auto visitRows = [&prefix](auto& a, auto& model) {
        a.raw(prefix);
        globalRow(a, model);
        a.count(model.graph.requirements.size());
        for (std::size_t i = 0; i < model.graph.requirements.size(); ++i) {
            requirementRow(a, model.graph.requirements[i], model.indices[i], model);
        }
        definitions(a, model);
        coordination(a, model);
    };
    return output(Mode::hash, m, limits, nullptr, nullptr, visitRows);
}
auto framed(std::array<char, 4> code, Bytes payload, std::uint32_t rows,
            chart::PackedChartLimits limits) -> gp::Section {
    bound(16 + payload.size(), limits.maxPackedSectionBytes, "packed.budget.section_bytes");
    ByteWriter out;
    out.writeU16(1);
    out.writeU8(1);
    out.writeU8(0);
    out.writeU32(rows);
    out.writeU64(payload.size());
    out.writeBytes(payload);
    return {code, std::move(out).takeBytes(), rows};
}
auto sum(std::span<const std::uint32_t> values) -> std::uint32_t {
    std::uint64_t total = 0;
    for (auto v : values) {
        total += v;
    }
    require(total <= UINT32_MAX, "GPH0.counts", "table count sum exceeds u32");
    return static_cast<std::uint32_t>(total);
}
void patchU32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        bytes[offset + i] = static_cast<std::byte>((value >> (8 * i)) & 255U);
    }
}
auto envelope(Model& m, gp::StaticTables tables, chart::PackedChartLimits limits) -> Bytes {
    const auto rows = [&](auto f) {
        return output(Mode::wire, m, limits, &tables.strings, &tables.references, f);
    };
    tables.sections.push_back(framed(
        {'G', 'P', 'H', '0'}, rows([](auto& a, auto& model) { globalRow(a, model); }), 1, limits));
    tables.sections.push_back(
        framed({'G', 'P', 'R', '0'}, rows([](auto& a, auto& model) {
                   for (std::size_t i = 0; i < model.graph.requirements.size(); ++i) {
                       requirementRow(a, model.graph.requirements[i], model.indices[i], model);
                   }
               }),
               m.counts[0], limits));
    tables.sections.push_back(framed({'G', 'P', 'D', '0'},
                                     rows([](auto& a, auto& model) { definitions(a, model); }),
                                     sum(std::span{m.counts}.subspan(1, 4)), limits));
    tables.sections.push_back(framed({'G', 'R', 'C', '0'},
                                     rows([](auto& a, auto& model) { coordination(a, model); }),
                                     sum(std::span{m.counts}.subspan(5, 5)), limits));
    std::sort(tables.sections.begin(), tables.sections.end(),
              [](const auto& l, const auto& r) { return l.code < r.code; });
    std::uint64_t decoded = 0;
    for (const auto& section : tables.sections) {
        bound(section.bytes.size(), limits.maxPackedSectionBytes, "packed.budget.section_bytes");
        decoded += section.bytes.size();
    }
    bound(static_cast<std::size_t>(decoded), limits.maxPackedDecodedBytes,
          "packed.budget.decoded_bytes");
    const auto directoryBytes = tables.sections.size() * 32;
    const auto total = 96 + directoryBytes + static_cast<std::size_t>(decoded);
    bound(total, limits.maxPackedFileBytes, "packed.budget.file_bytes");
    const auto digest = core::detail::sha256(preimage(m, limits));
    ByteWriter out;
    constexpr std::array magic{std::byte{'C'}, std::byte{'X'}, std::byte{'P'}, std::byte{'K'},
                               std::byte{'5'}, std::byte{0},   std::byte{0},   std::byte{0}};
    out.writeBytes(magic);
    out.writeU16(1);
    out.writeU16(96);
    out.writeU32(1);
    out.writeU32(u32(total));
    out.writeU32(96);
    out.writeU32(u32(tables.sections.size()));
    out.writeU32(u32(directoryBytes));
    for (auto b : digest) {
        out.writeU8(b);
    }
    out.writeU32(u32(m.entities.size()));
    out.writeU32(m.counts[0]);
    out.writeU32(0);
    out.writeU32(u32(static_cast<std::size_t>(decoded)));
    out.writeU32(u32(tables.strings.size()));
    out.writeU32(u32(tables.references.size()));
    out.writeU32(m.candidateRevision);
    out.writeU32(0);
    auto offset = 96 + directoryBytes;
    for (const auto& s : tables.sections) {
        out.writeBytes(std::as_bytes(std::span{s.code}));
        out.writeU8(0);
        out.writeU8(0);
        out.writeU16(0);
        out.writeU32(u32(offset));
        out.writeU32(u32(s.bytes.size()));
        out.writeU32(u32(s.bytes.size()));
        out.writeU32(s.records);
        out.writeU32(crc32(s.bytes));
        out.writeU32(0);
        offset += s.bytes.size();
    }
    for (const auto& s : tables.sections) {
        out.writeBytes(s.bytes);
    }
    auto bytes = std::move(out).takeBytes();
    patchU32(bytes, 92, crc32(std::span<const std::byte>{bytes}.first(92)));
    return bytes;
}
struct Payload final {
    std::span<const std::byte> bytes;
    std::uint32_t rows;
};
auto payloads(std::span<const std::byte> bytes) -> std::map<std::string, Payload> {
    ByteReader header{bytes.subspan(24, 4)};
    const auto n = checked(header.readU32());
    std::map<std::string, Payload> result;
    for (std::uint32_t i = 0; i < n; ++i) {
        ByteReader row{bytes.subspan(96 + static_cast<std::size_t>(i) * 32, 32)};
        const auto code = checked(row.readBytes(4));
        checked(row.readBytes(4));
        const auto offset = checked(row.readU32()), encoded = checked(row.readU32());
        checked(row.readU32());
        const auto records = checked(row.readU32());
        result.emplace(std::string{reinterpret_cast<const char*>(code.data()), 4},
                       Payload{bytes.subspan(offset, encoded), records});
    }
    return result;
}
template <typename F>
void decodeSection(const Payload& p, std::string path, Model& m, const gp::DecodedStatic& base,
                   F decodeRows) {
    ByteReader framing{p.bytes};
    require(checked(framing.readU16()) == 1 && checked(framing.readU8()) == 1 &&
                checked(framing.readU8()) == 0,
            path, "unknown Gameplay table framing");
    require(checked(framing.readU32()) == p.rows, path, "table count disagrees with directory");
    require(checked(framing.readU64()) == framing.remaining(), path, "payload length mismatch");
    Input in{ByteReader{p.bytes.subspan(16)}, base.strings, base.references, std::move(path)};
    decodeRows(in, m, p.rows);
    require(in.reader.empty(), in.path, "trailing Gameplay bytes");
}
void link(Model& m, const std::vector<std::uint64_t>& masks) {
    m.bind();
    auto& g = m.graph;
    std::vector<bool> presence(m.entities.size());
    for (std::size_t i = 0; i < g.requirements.size(); ++i) {
        auto& r = g.requirements[i];
        const auto& ix = m.indices[i];
        require(ix.entity < m.entities.size() && ix.pattern < m.patterns.size() &&
                    ix.measure < m.measures.size() && ix.domain < g.judgementDomains.size() &&
                    ix.solver < g.solverProfiles.size(),
                "GPR0.indices", "dangling table index");
        presence[ix.entity] = true;
        if (std::find(m.owners.begin(), m.owners.end(), m.entities[ix.entity]) == m.owners.end()) {
            m.owners.push_back(m.entities[ix.entity]);
        }
        r.pattern = m.patterns[ix.pattern];
        r.measure = m.measures[ix.measure].declaration;
        r.judgementDomainId = g.judgementDomains[ix.domain].domainId;
        r.solverProfileRef = g.solverProfiles[ix.solver].solverId;
        for (auto c : ix.claims) {
            require(c < m.claims.size() && m.claims[c].requirement == i, "GPR0.claims",
                    "claim owner mismatch");
            r.resourceClaims.push_back(m.claims[c].declaration);
        }
        for (auto b : ix.bindings) {
            require(b < g.factBindings.size(), "GPR0.factBindings", "dangling binding");
            r.factBindingRefs.push_back(g.factBindings[b].bindingId);
        }
    }
    require(masks.size() == presence.size(), "ENT0", "entity mask count mismatch");
    for (std::size_t i = 0; i < masks.size(); ++i) {
        require(((masks[i] & 4U) != 0) == presence[i], "ENT0.bit2",
                "Requirement presence disagrees with ownership");
    }
}
auto reconstruct(Model& m, const DecodeContext& context) -> PreparedGameplay {
    m.bind();
    auto& g = m.graph;
    require(m.profiles.normalizationProfileToken ==
                    context.identities.session.normalizationProfileToken &&
                m.profiles.coordinatorPolicyToken == context.coordinatorPolicyToken,
            "GPH0.profiles", "execution context does not match compiled profiles");
    std::vector<GameplaySource> sources;
    std::map<std::string, std::size_t> sourceIndex;
    for (const auto& id : g.sourceClosure.sourceDocumentIds) {
        require(!id.empty(), "sourceClosure", "empty source document ID");
        sourceIndex.emplace(id, sources.size());
        sources.push_back({SourceForm::memory, "packed.capsule.v2",
                           GameplaySourceDocument{id, 5, 2, {}, {}, {}, {}, {}, {}, {}}});
    }
    const auto sourceFor = [&](const std::string& id) -> GameplaySourceDocument& {
        const auto it = sourceIndex.find(id);
        require(it != sourceIndex.end(), "sourceClosure",
                "declaration source is not in source closure");
        return sources[it->second].document;
    };
    for (const auto& d : g.mergedNamespace.declarations) {
        auto& doc = sourceFor(d.stableId.sourceDocumentId);
        LocalDeclaration declaration{d.stableId, d.kind, d.name, {}, {}};
        for (const auto& r : d.references) {
            declaration.references.push_back(
                {ReferenceScope::explicitCrossDocument, r.sourceDocumentId, r.declarationOrdinal});
        }
        for (const auto& f : d.requiredFeatureIds) {
            declaration.required.features.push_back({f});
        }
        for (const auto& c : d.requiredCapabilityIds) {
            declaration.required.capabilities.push_back({c, ""});
        }
        doc.declarations.push_back(std::move(declaration));
    }
    for (const auto& r : g.requirements) {
        sourceFor(r.stableId.sourceDocumentId).requirements.push_back(r);
    }
    require(!sources.empty(), "sourceClosure", "prepare requires an explicit source closure");
    // Typed records without a separate source field are located by their merged declarations.
    const auto definitionSource = [&](DeclarationKind kind,
                                      std::string_view name) -> GameplaySourceDocument& {
        const auto it = std::find_if(
            g.mergedNamespace.declarations.begin(), g.mergedNamespace.declarations.end(),
            [kind, name](const auto& d) { return d.kind == kind && d.name == name; });
        require(it != g.mergedNamespace.declarations.end(), "mergedDeclarations",
                "definition source is missing");
        return sourceFor(it->stableId.sourceDocumentId);
    };
    for (const auto& r : g.resources) {
        definitionSource(DeclarationKind::resourceRecord, r.ref.resourceId).resources.push_back(r);
    }
    for (const auto& s : g.solverProfiles) {
        definitionSource(DeclarationKind::solverProfile, s.solverId).solverProfiles.push_back(s);
    }
    for (const auto& d : g.judgementDomains) {
        definitionSource(DeclarationKind::judgementDomain, d.domainId)
            .judgementDomains.push_back(d);
    }
    sources.front().document.relations = g.relations;
    sources.front().document.factBindings = g.factBindings;
    AssemblyRequest request{EntryKind::packedChart,
                            true,
                            std::move(sources),
                            g.graphRevision,
                            g.rulesetRef,
                            g.declaredCapabilities,
                            g.declaredFeatures,
                            g.closureContributions,
                            g.sourceClosure.compilerProfileToken,
                            &m.timebase,
                            &m.latePolicy,
                            context.capabilities,
                            context.contentLimits,
                            context.identities,
                            {},
                            {},
                            {}};
    request.executionProfile = g.executionProfile;
    request.normalizationProfileToken = g.normalizationProfileToken;
    request.coordinatorPolicyToken = g.coordinatorPolicyToken;
    auto prepared = checked(prepareResolvedGameplay({std::move(request), context.patternBudget}));
    const auto differences = semanticDiff(g, prepared.assembled().graph);
    require(differences.empty(), differences.empty() ? "gameplay" : differences.front().path,
            "shared prepare changed a decoded semantic field");
    require(g.sourceClosure == prepared.assembled().graph.sourceClosure, "sourceClosure",
            "source closure changed during prepare");
    return prepared;
}
auto decoded(std::span<const std::byte> bytes, const DecodeContext& context,
             chart::PackedChartLimits limits) -> PreparedCapsule {
    require(bytes.size() >= 96, "Header", "truncated Capsule header");
    ByteReader revisionReader{bytes.subspan(88, 4)};
    const auto revision = checked(revisionReader.readU32());
    if (revision != context.candidateRevision || (revision != 2 && revision != 3)) {
        fail("packed.header.unsupported_revision", "Capsule Reader revision mismatch", "Header");
    }
    const auto statistics = checked(gp::inspect(bytes, limits));
    auto base = checked(gp::decodeStatic(bytes, limits));
    const auto sections = payloads(bytes);
    Model m;
    m.candidateRevision = revision;
    m.chart = std::move(base.chart);
    for (const auto& e : m.chart.entities) {
        m.entities.push_back(e.identity);
    }
    decodeSection(sections.at("GPH0"), "GPH0", m, base, [](auto& in, auto& model, auto n) {
        require(n == 1, "GPH0", "global row count must be one");
        globalRow(in, model);
    });
    decodeSection(sections.at("GPD0"), "GPD0", m, base,
                  [](auto& in, auto& model, auto) { definitions(in, model); });
    decodeSection(sections.at("GPR0"), "GPR0", m, base, [limits](auto& in, auto& model, auto n) {
        bound(n, limits.maxPackedRequirements, "packed.budget.requirements");
        require(n <= in.reader.remaining(), "GPR0", "row count exceeds available bytes");
        model.graph.requirements.reserve(n);
        model.indices.reserve(n);
        for (std::uint32_t i = 0; i < n; ++i) {
            RequirementRecord r{};
            RequirementIndices ix;
            requirementRow(in, r, ix, model);
            model.graph.requirements.push_back(std::move(r));
            model.indices.push_back(std::move(ix));
        }
    });
    decodeSection(sections.at("GRC0"), "GRC0", m, base,
                  [](auto& in, auto& model, auto) { coordination(in, model); });
    const auto declaredCounts = m.counts;
    counts(m, base.references.size());
    require(m.counts == declaredCounts && m.counts[0] == statistics.requirementCount &&
                sections.at("GPR0").rows == m.counts[0] &&
                sections.at("GPD0").rows == sum(std::span{m.counts}.subspan(1, 4)) &&
                sections.at("GRC0").rows == sum(std::span{m.counts}.subspan(5, 5)),
            "GPH0.counts", "Header, directory and actual table counts disagree");
    link(m, base.entityMasks);
    validate(m, limits, context.patternBudget);
    // Reconstruct dictionaries from typed fields, not from a canonical whole-file comparison.
    // This keeps section permutation and validated inspection independent of judgement.
    const auto canonicalStatic = staticTables(m, limits);
    require(canonicalStatic.references == base.references, "REF0",
            "reference closure differs from typed records");
    const auto digest = core::detail::sha256(preimage(m, limits));
    if (!std::equal(digest.begin(), digest.end(), bytes.begin() + 32,
                    [](auto l, auto r) { return l == std::to_integer<std::uint8_t>(r); })) {
        fail("packed.identity.mismatch", "structural semantic identity mismatch",
             "Header.semanticIdentity");
    }
    auto prepared = reconstruct(m, context);
    std::vector<RequirementOwner> owners;
    for (std::size_t i = 0; i < m.graph.requirements.size(); ++i) {
        owners.push_back({m.graph.requirements[i].identity, m.entities[m.indices[i].entity]});
    }
    return {std::move(m.chart),    std::move(prepared),   std::move(owners), std::move(m.profiles),
            std::move(m.patterns), std::move(m.measures), revision};
}
#include "gameplay_graph_reader_internal.hpp"

template <typename T, typename F> auto boundary(F operation) -> core::Result<T> {
    try {
        if constexpr (std::is_void_v<T>) {
            operation();
            return {};
        } else {
            return operation();
        }
    } catch (Failure& error) {
        return core::unexpected(std::move(error.error));
    } catch (const std::exception&) {
        return core::unexpected(core::Error{"packed.requirements.invalid",
                                            "Capsule could not produce an owning result"});
    }
}
} // namespace

auto encode(const EncodeRequest& request, chart::PackedChartLimits limits) -> core::Result<Bytes> {
    return boundary<Bytes>([&] {
        limits = chart::packed::limits_detail::effectiveLimits(limits);
        auto m = makeModel(request, limits);
        m.bind();
        auto tables = staticTables(m, limits);
        return envelope(m, std::move(tables), limits);
    });
}

auto encodeGraph(const EncodeRequest& request, GraphWriterLimits graphLimits,
                 chart::PackedChartLimits limits) -> core::Result<std::string> {
    return boundary<std::string>([&] {
        if (!graphLimits.testOnly || !graphLimits.maxBytes || !graphLimits.maxStringBytes ||
            !graphLimits.maxRowAtoms)
            fail("capability.budget_insufficient",
                 "Explicit test-only Graph Writer bounds required");
        if (request.candidateRevision != 3)
            fail("graph.header.unsupported_revision", "Graph v1 requires Capsule revision 3");
        limits = chart::packed::limits_detail::effectiveLimits(limits);
        auto m = makeModel(request, limits);
        m.bind();
        const auto tables = staticTables(m, limits);
        (void)tables;
        const auto identity = core::detail::sha256Hex(preimage(m, limits));
        GraphText out{graphLimits};
        const auto row = [&](auto visitRow) {
            out.array([&] {
                GraphOutput visitor{Mode::wire, out};
                visitRow(visitor);
            });
        };
        const auto rows = [&](auto& values, auto visitRow) {
            out.array([&] {
                for (auto& value : values)
                    row([&](auto& visitor) { visitRow(visitor, value); });
            });
        };
        out.object([&] {
            out.key("format");
            out.string("cuexis.gameplay-graph");
            out.key("graphFormatRevision");
            out.integer(1);
            out.key("capsuleRevision");
            out.integer(3);
            out.key("semanticIdentity");
            out.string(identity);
            out.key("staticChart");
            graphStaticChart(out, m.chart);
            out.key("GPH0");
            row([&](auto& visitor) { globalRow(visitor, m); });
            out.key("GPR0");
            out.array([&] {
                for (std::size_t i = 0; i < m.graph.requirements.size(); ++i)
                    row([&](auto& visitor) {
                        requirementRow(visitor, m.graph.requirements[i], m.indices[i], m);
                    });
            });
            out.key("GPD0");
            out.object([&] {
                out.key("patterns");
                rows(m.patterns, [](auto& a, auto& v) { patternRow(a, v); });
                out.key("measures");
                rows(m.measures, [](auto& a, auto& v) { measureRow(a, v); });
                out.key("judgementDomains");
                rows(m.graph.judgementDomains, [](auto& a, auto& v) { domainRow(a, v); });
                out.key("mergedDeclarations");
                rows(m.graph.mergedNamespace.declarations,
                     [](auto& a, auto& v) { mergedRow(a, v); });
            });
            out.key("GRC0");
            out.object([&] {
                out.key("resources");
                rows(m.graph.resources, [](auto& a, auto& v) { resourceRow(a, v); });
                out.key("relations");
                rows(m.graph.relations, [&](auto& a, auto& v) { relationRow(a, v, m); });
                out.key("solverProfiles");
                rows(m.graph.solverProfiles, [](auto& a, auto& v) { solverRow(a, v); });
                out.key("factBindings");
                rows(m.graph.factBindings, [](auto& a, auto& v) { bindingRow(a, v); });
                out.key("claims");
                rows(m.claims, [&](auto& a, auto& v) { claimRow(a, v, m); });
            });
        });
        return out.take();
    });
}
auto semanticPreimage(const EncodeRequest& request, chart::PackedChartLimits limits)
    -> core::Result<Bytes> {
    return boundary<Bytes>([&] {
        limits = chart::packed::limits_detail::effectiveLimits(limits);
        auto m = makeModel(request, limits);
        m.bind();
        const auto tables = staticTables(m, limits);
        (void)tables;
        return preimage(m, limits);
    });
}
auto decode(std::span<const std::byte> bytes, const DecodeContext& context,
            chart::PackedChartLimits limits) -> core::Result<PreparedCapsule> {
    return boundary<PreparedCapsule>([&] {
        return decoded(bytes, context, chart::packed::limits_detail::effectiveLimits(limits));
    });
}

auto decodeGraph(std::string_view text, const DecodeContext& context, GraphReaderLimits graphLimits,
                 chart::PackedChartLimits limits) -> core::Result<PreparedCapsule> {
    return boundary<PreparedCapsule>([&] {
        return graphDecoded(text, context, graphLimits,
                            chart::packed::limits_detail::effectiveLimits(limits));
    });
}
auto read(const std::filesystem::path& source, const DecodeContext& context,
          chart::PackedChartLimits limits) -> core::Result<PreparedCapsule> {
    return boundary<PreparedCapsule>([&] {
        limits = chart::packed::limits_detail::effectiveLimits(limits);
        const auto path = std::filesystem::absolute(source);
        const auto file = checked(
            filesystem::readBoundedFile(path, {path.parent_path(), limits.maxPackedFileBytes, {}}));
        return decoded(file.bytes, context, limits);
    });
}
auto writeAtomic(const EncodeRequest& request, const std::filesystem::path& target,
                 chart::PackedChartLimits limits) -> core::Result<void> {
    return boundary<void>([&] {
        const auto bytes = checked(encode(request, limits));
        checked(chart::packed::file_detail::writeAtomic(bytes, target));
    });
}
auto decodeInto(std::optional<PreparedCapsule>& active, std::span<const std::byte> bytes,
                const DecodeContext& context, chart::PackedChartLimits limits)
    -> core::Result<void> {
    auto candidate = decode(bytes, context, limits);
    if (!candidate) {
        return core::unexpected(std::move(candidate.error()));
    }
    active = std::move(*candidate);
    return {};
}
} // namespace cuexis::gameplay_packed
