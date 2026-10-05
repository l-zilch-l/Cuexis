#include <cuexis/json/gameplay_author.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/json/schema.hpp>

#include "gameplay_author_schema.hpp"
#include "gameplay_inline_schema.hpp"
#include "json_conversion.hpp"
#include <cmath>
#include <limits>

#include <charconv>
#include <numeric>
#include <stdexcept>

namespace cuexis::json::gameplay {
namespace {
using J = nlohmann::json;
struct Invalid {
    std::string path, reason;
};
auto text(const J& j, std::string_view field) -> std::string {
    return j.at(std::string{field}).get<std::string>();
}
template <class T> auto decimal(const J& j) -> T {
    const auto& s = j.get_ref<const std::string&>();
    T value{};
    auto parsed = std::from_chars(s.data(), s.data() + s.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != s.data() + s.size()) {
        throw Invalid{"numeric", "decimal is not representable"};
    }
    if (std::to_string(value) != s) {
        throw Invalid{"numeric", "decimal is not canonical"};
    }
    return value;
}
auto i64(const J& j, std::string_view key) -> std::int64_t {
    return decimal<std::int64_t>(j.at(std::string{key}));
}
auto u64(const J& j, std::string_view key) -> std::uint64_t {
    return decimal<std::uint64_t>(j.at(std::string{key}));
}
auto u32(const J& j, std::string_view key) -> std::uint32_t {
    const auto& value = j.at(std::string{key});
    if (!value.is_number_integer() || (value.is_number_integer() && !value.is_number_unsigned() &&
                                       value.get<std::int64_t>() < 0)) {
        throw Invalid{std::string{key}, "expected exact unsigned JSON integer"};
    }
    auto number = value.get<std::uint64_t>();
    if (number > UINT32_MAX) {
        throw Invalid{std::string{key}, "u32 overflow"};
    }
    return static_cast<std::uint32_t>(number);
}
auto boolean(const J& j, std::string_view key) -> bool {
    return j.at(std::string{key}).get<bool>();
}
auto optionalText(const J& j, std::string_view key) -> std::optional<std::string> {
    return j.contains(std::string{key}) ? std::optional{text(j, key)} : std::nullopt;
}
template <class F> auto list(const J& j, std::string_view key, F convert) {
    std::vector<decltype(convert(std::declval<const J&>()))> result;
    for (const auto& v : j.at(std::string{key})) {
        result.push_back(convert(v));
    }
    return result;
}
auto strings(const J& j, std::string_view key) -> std::vector<std::string> {
    return list(j, key, [](const J& v) { return v.get<std::string>(); });
}
auto q(const J& j) -> Q {
    const auto n = i64(j, "numerator");
    const auto d = u64(j, "denominator");
    const auto magnitude =
        n < 0 ? static_cast<std::uint64_t>(-(n + 1)) + 1 : static_cast<std::uint64_t>(n);
    if (d == 0 || d > INT64_MAX || std::gcd(magnitude, d) != 1) {
        throw Invalid{"rational",
                      "rational must be reduced with a representable positive denominator"};
    }
    return {n, static_cast<std::int64_t>(d)};
}
auto optionalQ(const J& j, std::string_view key) -> std::optional<Q> {
    return j.contains(std::string{key}) ? std::optional{q(j.at(std::string{key}))} : std::nullopt;
}
auto stable(const J& j) -> StableId {
    return {text(j, "sourceDocumentId"), u32(j, "declarationOrdinal")};
}
auto identity(const J& j) -> Identity {
    return {text(j, "chartEntryId"),
            text(j, "invocationId"),
            text(j, "moduleId"),
            text(j, "exportId"),
            list(j, "emissionPath",
                 [](const J& v) -> PathStep { return {text(v, "nodeId"), u64(v, "repeatIndex")}; }),
            text(j, "requirementLocalId")};
}
auto capability(const J& j) -> Capability {
    return {text(j, "capabilityId"), optionalText(j, "revision")};
}
auto refs(const J& j) -> Refs {
    return {strings(j, "features"), list(j, "capabilities", capability)};
}
auto phase(const J& j) -> Phase {
    return {text(j, "kind"), u32(j, "declarationOrdinal")};
}
auto node(const J& j) -> PatternNode {
    PatternNode result{
        text(j, "primitive"), list(j, "operands", node), optionalText(j, "atomRef"), {}};
    if (j.contains("repeatBounds")) {
        result.repeatBounds = {u64(j.at("repeatBounds"), "minimum"),
                               u64(j.at("repeatBounds"), "maximum")};
    }
    return result;
}
auto pattern(const J& j) -> Pattern {
    return {optionalText(j, "patternId"), text(j, "matchPolicy"), node(j.at("root")),
            refs(j.at("requiredRefs"))};
}
auto measure(const J& j) -> Measure {
    return {list(j, "components",
                 [](const J& v) -> MeasureComponent {
                     return {text(v, "phase"), text(v, "categoryToken"), strings(v, "gradeTokens")};
                 }),
            refs(j.at("requiredRefs"))};
}
auto measureDefinition(const J& j) -> MeasureDefinition {
    return {optionalText(j, "id"), measure(j.at("declaration"))};
}
auto grace(const J& j) -> Grace {
    return {text(j, "policy"), boolean(j, "allowChartGrace"),
            optionalText(j, "inheritedFromDeclarationId")};
}
auto graceInputs(const J& j) -> GraceInputs {
    return {q(j.at("unitInTicks")),
            i64(j, "minimumCanonical"),
            i64(j, "maximumCanonical"),
            optionalQ(j, "chartDuration"),
            optionalQ(j, "inheritedDuration"),
            optionalQ(j, "defaultDuration")};
}
auto timing(const J& j) -> Timing {
    Timing result{i64(j, "end"),
                  list(j, "successWindows",
                       [](const J& v) -> Window {
                           return {phase(v.at("phase")), i64(v, "start"), i64(v, "end")};
                       }),
                  {},
                  list(j, "phaseTargets", [](const J& v) -> Target {
                      return {text(v, "phase"), i64(v, "chartTick")};
                  })};
    if (j.contains("body")) {
        result.body = {i64(j.at("body"), "start"), i64(j.at("body"), "end")};
    }
    return result;
}
auto atomBinding(const J& j) -> AtomBinding {
    AtomBinding result{text(j, "atomRef"),      text(j, "domainToken"), text(j, "sourceClass"),
                       text(j, "channelToken"), text(j, "action"),      {},
                       boolean(j, "tailOnly")};
    if (j.contains("amountRange")) {
        result.amountRange = {i64(j.at("amountRange"), "minimum"),
                              i64(j.at("amountRange"), "maximum")};
    }
    return result;
}
auto requirement(const J& j) -> Requirement {
    return {stable(j.at("stableId")),
            identity(j.at("identity")),
            strings(j, "requiredActions"),
            text(j, "domainBinding"),
            text(j, "judgementDomainId"),
            list(j, "phases", phase),
            boolean(j, "requiresReleaseTailSemantics"),
            pattern(j.at("pattern")),
            strings(j, "patternArmRefs"),
            u64(j.at("maxArmElements"), "value"),
            u64(j.at("maxDeadlineElements"), "value"),
            measure(j.at("measure")),
            list(j, "resourceClaims",
                 [](const J& v) -> Claim {
                     return {text(v, "resourceId"), text(v, "intent"), text(v, "policyToken"),
                             text(v.at("graceOverride"), "mode"),
                             text(v.at("graceOverride"), "overrideToken")};
                 }),
            grace(j.at("grace")),
            timing(j.at("timing")),
            list(j, "atomBindings", atomBinding),
            text(j, "solverProfileRef"),
            text(j, "localClosePolicyToken"),
            strings(j, "factBindingRefs"),
            refs(j.at("requiredRefs"))};
}
auto common(const J& j) -> Common {
    const auto& time = j.at("timebase");
    const auto& late = j.at("latePolicy");
    return {text(j, "chartEntryId"),
            u64(j, "graphRevision"),
            text(j, "rulesetRef"),
            text(j, "normalizationProfileToken"),
            text(j, "coordinatorPolicy"),
            text(j, "executionProfile"),
            {text(time, "profileId"), text(time, "unitToken"), q(time.at("tickScale")),
             q(time.at("originBeat")), q(time.at("initialTempo")),
             list(time, "tempoSections",
                  [](const J& v) -> Tempo {
                      return {q(v.at("startBeat")), q(v.at("durationPerBeat"))};
                  }),
             list(time, "stopSections",
                  [](const J& v) -> Stop {
                      return {q(v.at("startBeat")), q(v.at("endBeat")), q(v.at("duration"))};
                  })},
            {text(late, "mode"), i64(late.at("finalizationWatermark"), "value"),
             i64(late.at("maxQueueHop"), "value"), i64(late.at("windowCloseThreshold"), "value"),
             i64(late.at("windowOpenThreshold"), "value")},
            list(j, "graceInputs",
                 [](const J& v) -> GraceBinding {
                     return {stable(v.at("requirement")), graceInputs(v.at("inputs"))};
                 }),
            list(j, "declarations",
                 [](const J& v) -> Declaration {
                     return {stable(v.at("stableId")), text(v, "kind"), text(v, "localName"),
                             list(v, "references",
                                  [](const J& r) -> DeclarationRef {
                                      return {text(r, "scope"), optionalText(r, "sourceDocumentId"),
                                              u32(r, "declarationOrdinal")};
                                  }),
                             refs(v.at("requiredRefs"))};
                 }),
            list(j, "resources",
                 [](const J& v) -> Resource {
                     return {text(v, "resourceId"),
                             u64(v, "declaredCapacity"),
                             text(v, "slotToken"),
                             text(v, "decisionPolicyRef"),
                             boolean(v, "terminalAfterTermination"),
                             i64(v, "declaredGapGrace"),
                             refs(v.at("requiredRefs"))};
                 }),
            list(j, "relations",
                 [](const J& v) -> Relation {
                     return {text(v, "kind"),
                             text(v, "resourceId"),
                             list(v, "members", stable),
                             text(v, "policyToken"),
                             u64(v, "declaredCapacity"),
                             refs(v.at("requiredRefs"))};
                 }),
            list(j, "solverProfiles",
                 [](const J& v) -> Solver {
                     return {text(v, "solverId"),       text(v, "revision"),
                             text(v, "algorithmToken"), strings(v, "objective"),
                             strings(v, "tieBreak"),    boolean(v, "rejectIfNonUnique"),
                             refs(v.at("requiredRefs"))};
                 }),
            strings(j, "factBindings"),
            list(j, "judgementDomains",
                 [](const J& v) -> Domain {
                     return {text(v, "domainId"), text(v, "coordinateSystemToken"),
                             list(v, "axes",
                                  [](const J& a) -> Axis {
                                      return {text(a, "axisToken"), i64(a, "minimum"),
                                              i64(a, "maximum")};
                                  }),
                             text(v, "frame"), refs(v.at("requiredRefs"))};
                 }),
            strings(j, "declaredFeatures"),
            list(j, "declaredCapabilities", capability),
            list(j, "namedGraceDurations",
                 [](const J& v) -> NamedGrace {
                     return {text(v, "declarationId"), q(v.at("duration"))};
                 }),
            list(j, "patternDefinitions", pattern),
            list(j, "measureDefinitions", measureDefinition)};
}
auto rank(const J& j) -> RankAssignment {
    RankAssignment result{text(j, "mode"), {}, {}};
    if (result.mode == "explicit") {
        result.rows = list(j, "rows", [](const J& v) -> RankRow {
            return {identity(v.at("identity")), i64(v, "priority"), u64(v, "tieRank"),
                    text(v, "namespace")};
        });
    } else {
        result.blocks = list(j, "blocks", [](const J& v) -> RankBlock {
            return {text(v, "invocationId"),
                    text(v, "moduleId"),
                    text(v, "exportId"),
                    text(v, "requirementLocalId"),
                    i64(v, "priority"),
                    u64(v, "base"),
                    u64(v, "stride"),
                    text(v, "namespace"),
                    strings(v, "nodeOrder"),
                    list(v, "radices", [](const J& n) { return decimal<std::uint64_t>(n); })};
        });
    }
    return result;
}
auto rawInteger(const J& value) -> std::int64_t {
    if (!value.is_number_integer() ||
        (value.is_number_unsigned() && value.get<std::uint64_t>() > INT64_MAX)) {
        throw Invalid{"count", "CXT integer is not representable"};
    }
    return value.get<std::int64_t>();
}
void families(const J& nodes, std::string_view exportId, std::vector<RepeatCount> path,
              std::vector<EmissionFamily>& output) {
    for (const auto& n : nodes) {
        const auto op = text(n, "op");
        if (op == "emit") {
            output.push_back({std::string{exportId}, path, text(n, "nodeId")});
        } else if (op == "repeat") {
            const auto& count = n.at("count");
            RepeatCount repeat{text(n, "nodeId"), {}, {}};
            if (text(count, "kind") == "literal") {
                auto value = rawInteger(count.at("value"));
                if (value < 0) {
                    throw Invalid{"count", "negative CXT repeat count"};
                }
                repeat.literal = static_cast<std::uint64_t>(value);
            } else if (text(count, "kind") == "parameter") {
                repeat.parameter = text(count, "id");
            } else {
                throw Invalid{"count", "unsupported CXT repeat count"};
            }
            auto child = path;
            child.push_back(std::move(repeat));
            families(n.at("body"), exportId, std::move(child), output);
        } else {
            throw Invalid{"op", "unknown CXT node"};
        }
    }
}
auto number(const J& j, std::string_view key) -> double {
    const auto& v = j.at(std::string{key});
    if (!v.is_number()) {
        throw Invalid{std::string{key}, "expected number"};
    }
    const auto value = v.get<double>();
    if (!std::isfinite(value)) {
        throw Invalid{std::string{key}, "number must be finite"};
    }
    return value;
}
template <std::size_t N> auto floats(const J& j, std::string_view key) -> std::array<float, N> {
    std::array<float, N> result{};
    const auto& values = j.at(std::string{key});
    if (!values.is_array() || values.size() != N) {
        throw Invalid{std::string{key}, "wrong vector size"};
    }
    for (std::size_t i = 0; i < N; ++i) {
        const auto v = values[i].get<double>();
        if (!std::isfinite(v) || std::abs(v) > std::numeric_limits<float>::max()) {
            throw Invalid{std::string{key}, "finite float32 required"};
        }
        result[i] = static_cast<float>(v);
    }
    return result;
}
auto transform(const J& j) -> FoundationTransform {
    return {floats<3>(j, "position"), floats<3>(j, "scale"), floats<4>(j, "rotation")};
}
auto camera(const J& j) -> FoundationCamera {
    return {text(j, "type"), number(j, "fovY"), number(j, "nearPlane"), number(j, "farPlane")};
}
auto foundationIdentity(const J& j) -> FoundationIdentity {
    FoundationIdentity r;
    r.kind = text(j, "kind");
    if (r.kind == "explicit") {
        r.objectId = text(j, "objectId");
    } else {
        r.chartId = text(j, "chartId");
        r.bindingId = text(j, "bindingId");
        r.moduleId = text(j, "moduleId");
        r.exportId = text(j, "exportId");
        r.path = list(j, "path", [](const J& p) {
            return std::pair{text(p, "nodeId"), u32(p, "iterationIndexPlusOne")};
        });
    }
    return r;
}
auto inlineFoundation(const J& root) -> InlineFoundation {
    const auto& t = root.at("timing");
    const auto& c = root.at("defaultCamera");
    InlineFoundation r{
        text(root, "chartId"),
        optionalText(root, "mainMusic"),
        list(root, "features",
             [](const J& f) { return std::pair{text(f, "id"), u32(f, "version")}; }),
        number(t, "offsetMs"),
        number(t, "defaultBpm"),
        list(t, "tempoEvents",
             [](const J& v) -> FoundationTempo {
                 return {q(v.at("startBeat")), q(v.at("durationBeats")), number(v, "startBpm"),
                         number(v, "endBpm"),  number(v, "startSlope"),  number(v, "endSlope")};
             }),
        list(t, "stops",
             [](const J& v) -> FoundationStop {
                 return {q(v.at("beat")), number(v, "durationMs")};
             }),
        camera(c),
        number(c, "pitch"),
        number(c, "yaw"),
        number(c, "roll"),
        {},
        list(root, "resourceClosure",
             [](const J& v) { return std::pair{text(v, "assetId"), text(v, "use")}; }),
        {}};
    if (c.contains("defaultTransform")) {
        r.defaultTransform = transform(c.at("defaultTransform"));
    }
    r.entities = list(root, "entities", [](const J& v) -> FoundationEntity {
        FoundationEntity e{foundationIdentity(v.at("identity")), {}, {}};
        if (v.contains("parent")) {
            e.parent = foundationIdentity(v.at("parent"));
        }
        e.components = list(v, "components", [](const J& item) -> FoundationComponent {
            FoundationComponent component;
            component.kind = text(item, "kind");
            if (component.kind == "transform") {
                component.transform = transform(item);
            } else if (component.kind == "camera") {
                component.camera = camera(item);
            } else {
                component.mesh = text(item, "mesh");
                component.material = text(item, "material");
                component.alpha = u32(item, "alpha");
            }
            return component;
        });
        return e;
    });
    return r;
}
void validateSchema(const J& source, std::string_view bytes) {
    auto schema = json::parse(bytes, {bytes.size(), bytes.size(), bytes.size()});
    if (!schema) {
        throw Invalid{"schema", std::string{schema.error().message()}};
    }
    core::Diagnostics diagnostics;
    auto result =
        json::validateAgainstSchema(json::detail::fromNlohmann(source), *schema, diagnostics);
    if (!result) {
        throw Invalid{"schema", std::string{result.error().message()}};
    }
    if (diagnostics.hasErrors()) {
        const auto& first = diagnostics.items().front();
        throw Invalid{std::string{first.fieldPath()}, std::string{first.message()}};
    }
}
auto reject(std::string path, std::string message) -> core::Error {
    return core::Error{"judgement.s7a4.execution.relation_invalid", std::move(message)}
        .withContext("category", "invalid_relation")
        .withContext("severity", "error")
        .withContext("faulted", "false")
        .withContext("field.path", std::move(path));
}
} // namespace

auto readAuthorSource(std::string_view original, SourceKind kind, SourceLimits limits)
    -> core::Result<AuthorSource> try {
    auto value = json::parse(original,
                             {limits.maxInputBytes, limits.maxNestingDepth, limits.maxStringBytes});
    if (!value) {
        return core::unexpected(value.error());
    }
    auto root = json::detail::toNlohmann(*value);
    if (!root.is_object()) {
        throw Invalid{"$", "author source root must be an object"};
    }
    const auto expectedFormat =
        kind == SourceKind::cxtModule ? "cuexis.animation-template" : "cuexis.chart";
    const auto expectedVersion = kind == SourceKind::cxtModule ? 2 : 5;
    if (text(root, "format") != expectedFormat ||
        rawInteger(root.at("version")) != expectedVersion) {
        throw Invalid{"version", "explicit author root format/version mismatch"};
    }
    const auto& extensions = root.at("requiredExtensions");
    if (!extensions.is_array() || extensions.size() != 1 || extensions[0] != "cuexis.gameplay.v2") {
        throw Invalid{"requiredExtensions",
                      "author entry accepts exactly its declared Gameplay extension"};
    }
    std::optional<InlineFoundation> inlineValues;
    J payload;
    if (kind == SourceKind::cxtModule) {
        if (root.contains("gameplay") || !root.at("extensions").is_object() ||
            root.at("extensions").size() != 1) {
            throw Invalid{"extensions", "ambiguous or unknown extension"};
        }
        payload = root.at("extensions").at("cuexis.gameplay.v2");
        root["extensions"] = J::object();
        if (text(root, "moduleKind") != "prototype" && text(root, "moduleKind") != "pattern") {
            throw Invalid{"moduleKind",
                          "author entry requires finite prototype or pattern expansion"};
        }
        for (const auto& prototype : root.at("prototypes")) {
            if (!prototype.at("requirements").is_array() || !prototype.at("requirements").empty()) {
                throw Invalid{"prototypes.requirements",
                              "legacy Foundation requirements cannot coexist with Gameplay V2"};
            }
        }
    } else {
        validateSchema(root, kAuthorInlineSchema);
        inlineValues = inlineFoundation(root);
        if (root.contains("extensions") && root.at("extensions").contains("cuexis.gameplay.v2")) {
            throw Invalid{"extensions", "two Gameplay authorities are forbidden"};
        }
        payload = root.at("gameplay");
        root.erase("gameplay");
    }
    if (rawInteger(payload.at("version")) != 2) {
        throw Invalid{"gameplay.version", "exact integer version 2 required"};
    }
    root["requiredExtensions"] = J::array();
    auto schemaValue =
        json::parse(kAuthorPayloadSchema, {kAuthorPayloadSchema.size(), kAuthorPayloadSchema.size(),
                                           kAuthorPayloadSchema.size()});
    if (!schemaValue) {
        return core::unexpected(schemaValue.error());
    }
    core::Diagnostics diagnostics;
    auto checked =
        json::validateAgainstSchema(json::detail::fromNlohmann(payload), *schemaValue, diagnostics);
    if (!checked) {
        return core::unexpected(checked.error());
    }
    if (diagnostics.hasErrors()) {
        const auto& first = diagnostics.items().front();
        return core::unexpected(
            reject(std::string{first.fieldPath()}, std::string{first.message()}));
    }
    AuthorSource result{kind,
                        text(payload, "sourceDocumentId"),
                        common(payload.at("common")),
                        list(payload, "requirements", requirement),
                        rank(payload.at("rankAssignment")),
                        root.dump(),
                        std::string{original},
                        {},
                        {},
                        std::move(inlineValues)};
    if (kind == SourceKind::cxtModule) {
        for (const auto& p : root.at("parameters")) {
            if (text(p, "type") == "integer") {
                result.integerDefaults.push_back({text(p, "id"), rawInteger(p.at("default"))});
            }
        }
        if (text(root, "moduleKind") == "prototype") {
            for (const auto& e : root.at("exports")) {
                result.emissionFamilies.push_back({text(e, "id"), {}, "__direct__"});
            }
        } else {
            for (const auto& p : root.at("patterns")) {
                families(p.at("nodes"), text(p, "id"), {}, result.emissionFamilies);
            }
        }
    }
    return result;
} catch (const Invalid& e) {
    return core::unexpected(reject(e.path, e.reason));
} catch (const std::exception& e) {
    return core::unexpected(reject("author", e.what()));
}
} // namespace cuexis::json::gameplay
