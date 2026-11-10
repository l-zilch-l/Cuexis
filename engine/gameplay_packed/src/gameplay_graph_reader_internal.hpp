// Included after the Capsule decoder helpers. Each SAX frame owns only its typed record.
using GraphAtom = std::variant<bool, std::int64_t, std::uint64_t, std::string, Ref>;
[[noreturn]] void graphInvalid(std::string_view message) {
    fail("graph.structure.invalid", message);
}
auto graphFailure(core::Error error, std::string_view code) -> core::Error {
    core::Error result{std::string{code}, std::string{error.message()}};
    for (const auto& context : error.context())
        result.withContext(context.key, context.value);
    result.withContext("sourceCode", std::string{error.code()});
    return result;
}
template <typename F> auto graphClosure(F operation) -> decltype(operation()) {
    try {
        return operation();
    } catch (Failure& error) {
        if (error.error.code().starts_with("packed.") || error.error.code().starts_with("chart."))
            throw Failure{graphFailure(std::move(error.error), "graph.closure.invalid")};
        throw;
    }
}
struct GraphInput final {
    static constexpr bool reading = true;
    static constexpr bool deferLinks = true;
    std::span<const GraphAtom> atoms;
    std::size_t position{};
    auto next() -> const GraphAtom& {
        if (position == atoms.size())
            graphInvalid("Graph typed row is truncated");
        return atoms[position++];
    }
    void value(std::uint64_t& v) {
        const auto& atom = next();
        if (const auto* n = std::get_if<std::uint64_t>(&atom))
            v = *n;
        else if (const auto* signedNumber = std::get_if<std::int64_t>(&atom);
                 signedNumber && *signedNumber >= 0)
            v = static_cast<std::uint64_t>(*signedNumber);
        else
            fail("graph.integer.out_of_range", "Expected unsigned integer Graph atom");
    }
    void value(std::uint32_t& v) {
        std::uint64_t n{};
        value(n);
        if (n > UINT32_MAX)
            fail("graph.integer.out_of_range", "Graph integer is outside u32");
        v = static_cast<std::uint32_t>(n);
    }
    void value(std::int64_t& v) {
        const auto& atom = next();
        if (const auto* n = std::get_if<std::int64_t>(&atom))
            v = *n;
        else if (const auto* unsignedNumber = std::get_if<std::uint64_t>(&atom);
                 unsignedNumber && *unsignedNumber <= INT64_MAX)
            v = static_cast<std::int64_t>(*unsignedNumber);
        else
            fail("graph.integer.out_of_range", "Graph integer is outside i64");
    }
    auto tag() -> std::uint8_t {
        std::uint32_t n{};
        value(n);
        if (n > UINT8_MAX)
            fail("graph.integer.out_of_range", "Graph tag is outside u8");
        return static_cast<std::uint8_t>(n);
    }
    void value(bool& v) {
        const auto* b = std::get_if<bool>(&next());
        if (!b)
            graphInvalid("Expected boolean Graph atom");
        v = *b;
    }
    void value(std::string& v) {
        const auto* s = std::get_if<std::string>(&next());
        if (!s)
            graphInvalid("Expected string Graph atom");
        v = *s;
    }
    void ref(std::uint8_t kind, std::string& v) {
        const auto* r = std::get_if<Ref>(&next());
        if (!r || r->first != kind)
            graphInvalid("Graph reference kind does not match consuming field");
        v = r->second;
    }
    auto count() -> std::size_t {
        std::uint32_t n{};
        value(n);
        if (n > atoms.size() - position)
            graphInvalid("Graph count exceeds remaining typed row atoms");
        return n;
    }
    template <typename... T> void fields(T&... v) {
        (value(v), ...);
    }
    template <typename T> void value(std::vector<T>& v) {
        const auto n = count();
        v.clear();
        v.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            T row{};
            value(row);
            v.push_back(std::move(row));
        }
    }
    template <typename T> void value(std::optional<T>& v) {
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
    template <typename T> void value(T& v) {
        visit(*this, v);
    }
};

class GraphSink final : public json::ISaxEventSink {
    using Kind = json::SaxEventKind;
    using Event = json::SaxEvent;
    enum class Node {
        Root,
        Static,
        Timing,
        Camera,
        Transform,
        Identity,
        Entity,
        Component,
        Feature,
        Tempo,
        Stop,
        ResourceUse,
        PathStep,
        Definitions,
        Coordination,
        Row,
        Reference,
        Features,
        Tempos,
        Stops,
        Entities,
        Components,
        Resources,
        Path,
        Vector,
        Rational,
        Rows
    };
    enum class Table {
        Global,
        Requirement,
        Pattern,
        Measure,
        Domain,
        Merged,
        Resource,
        Relation,
        Solver,
        Binding,
        Claim
    };
    struct IdentityValue {
        std::uint32_t kind{};
        chart::ExplicitEntityIdentity explicitValue;
        chart::GeneratedEntityIdentity generatedValue;
    };
    struct ComponentValue {
        std::uint32_t kind{};
        chart::CanonicalTransform transform;
        chart::CanonicalRenderable renderable;
        chart::CanonicalCamera camera;
    };
    struct Numbers {
        std::array<double, 4> values{};
        std::array<std::int64_t, 2> integers{};
    };
    using Data =
        std::variant<std::monostate, chart::CameraData, chart::CanonicalTransform, IdentityValue,
                     chart::CanonicalEntity, ComponentValue, chart::CanonicalFeature,
                     chart::TempoEvent, chart::TimingStop, chart::CanonicalResourceUse,
                     chart::SemanticIdentityStep, Numbers, Ref>;
    struct Frame {
        Node node;
        Table table{Table::Global};
        std::uint64_t seen{};
        std::size_t count{};
        std::string key{};
        Data data{};
    };

  public:
    GraphSink(Model& model, GraphReaderLimits limits, chart::PackedChartLimits physical)
        : model_(model), limits_(limits), physical_(physical) {}
    auto onEvent(const Event& event) -> core::Result<void> override;
    auto identity() const -> const std::string& {
        return identity_;
    }
    bool complete() const {
        return complete_ && frames_.empty();
    }

  private:
    static auto names(Node node) -> std::string_view;
    static auto fieldIndex(Node node, std::string_view key) -> unsigned;
    static auto required(Node node) -> std::uint64_t;
    static auto unsignedInteger(const Event& e, std::uint64_t maximum = UINT64_MAX)
        -> std::uint64_t;
    static auto signedInteger(const Event& e) -> std::int64_t;
    static auto text(const Event& e) -> std::string;
    static auto number(const Event& e) -> double;
    static auto f32(double value) -> float;
    template <typename T> static auto data(Frame& frame) -> T&;
    void addStringBytes(std::size_t size);
    void atom(GraphAtom value);
    void key(const Event& e);
    void push(Node node, Table table = Table::Global);
    static auto table(std::string_view key) -> Table;
    void start(bool object);
    void scalar(const Event& e);
    template <typename T, typename F>
    void decodeRow(std::vector<T>& values, GraphInput& in, F visitor);
    void finishRow(Table selected);
    void end();
    void consume(const Event& event);
    Model& model_;
    GraphReaderLimits limits_;
    chart::PackedChartLimits physical_;
    std::vector<Frame> frames_;
    std::vector<GraphAtom> atoms_;
    std::size_t rowStringBytes_{};
    std::string identity_;
    bool started_{}, complete_{};
};

#include "gameplay_graph_reader_sink_a_internal.hpp"
#include "gameplay_graph_reader_sink_b_internal.hpp"

auto graphDecoded(std::string_view text, const DecodeContext& context, GraphReaderLimits limits,
                  chart::PackedChartLimits physical) -> PreparedCapsule {
    if (!limits.testOnly || !limits.maxBytes || !limits.maxDepth || !limits.maxStringBytes ||
        !limits.maxValues || !limits.maxContainerElements || !limits.maxRowAtoms ||
        !limits.maxRowDecodedStringBytes)
        fail("capability.budget_insufficient", "Explicit test-only Graph Reader bounds required");
    if (context.candidateRevision != 3)
        fail("graph.header.unsupported_revision", "Graph context requires Capsule revision 3");
    Model model;
    model.candidateRevision = 3;
    GraphSink sink{model, limits, physical};
    auto parsed = json::parseEvents(text,
                                    {{limits.maxBytes, limits.maxDepth, limits.maxStringBytes},
                                     limits.maxValues,
                                     limits.maxContainerElements},
                                    sink);
    if (!parsed) {
        auto error = std::move(parsed.error());
        const std::string source{error.code()};
        if (source.starts_with("json.parse.")) {
            const auto code = source == "json.parse.duplicate_key" ? "graph.structure.duplicate_key"
                              : source.ends_with("_limit")         ? "graph.budget.exceeded"
                                                                   : "graph.structure.invalid";
            throw Failure{
                core::Error{code, std::string{error.message()}}.withContext("sourceCode", source)};
        }
        throw Failure{std::move(error)};
    }
    if (!sink.complete())
        graphInvalid("Graph root was not completed");
    model.bind();
    const auto declared = model.counts;
    counts(model, declared.back());
    if (declared != model.counts)
        fail("graph.closure.invalid", "Graph table counts do not match typed rows");
    for (const auto& entity : model.chart.entities)
        model.entities.push_back(entity.identity);
    std::sort(model.entities.begin(), model.entities.end(), [](const auto& l, const auto& r) {
        return chart::packed::identity_detail::ByteKeyLess{}(
            checked(chart::packed::identity_detail::canonicalIdentityBytes(l)),
            checked(chart::packed::identity_detail::canonicalIdentityBytes(r)));
    });
    if (model.deferredRelationResources.size() != model.graph.relations.size())
        fail("graph.closure.invalid", "Graph relation indices are incomplete");
    for (std::size_t i = 0; i < model.graph.relations.size(); ++i) {
        const auto index = model.deferredRelationResources[i];
        if (index >= model.graph.resources.size())
            fail("graph.closure.invalid", "Dangling Graph relation resource");
        model.graph.relations[i].resourceRef = model.graph.resources[index].ref;
    }
    for (auto& claim : model.claims) {
        if (claim.requirement >= model.graph.requirements.size() ||
            claim.resource >= model.graph.resources.size())
            fail("graph.closure.invalid", "Dangling Graph claim owner or resource");
        claim.declaration.resourceRef = model.graph.resources[claim.resource].ref;
        if (claim.deferredClaimKey &&
            *claim.deferredClaimKey != judgement::detail::structuralClaimKey(
                                           claim.declaration.claimPolicy.claimKeyToken,
                                           model.graph.requirements[claim.requirement].identity))
            fail("graph.closure.invalid", "Graph claim key differs from namespace and full E");
        claim.deferredClaimKey.reset();
    }
    std::vector<std::uint64_t> masks(model.entities.size());
    for (const auto& indices : model.indices) {
        if (indices.entity >= masks.size())
            fail("graph.closure.invalid", "Dangling Graph Requirement owner");
        masks[indices.entity] |= 4;
    }
    graphClosure([&] { link(model, masks); });
    graphClosure([&] { validate(model, physical, context.patternBudget); });
    const auto canonical = graphClosure([&] { return staticTables(model, physical); });
    (void)canonical;
    if (model.counts != declared)
        fail("graph.closure.invalid", "Graph derived REF0 count differs from header");
    const auto digest = [&] {
        const auto bytes = graphClosure([&] { return preimage(model, physical); });
        const auto result = core::detail::sha256(bytes);
        if (core::detail::sha256Hex(result) != sink.identity())
            fail("graph.identity.mismatch", "Graph structural semantic identity mismatch");
        return result;
    }();
    auto prepared = reconstruct(model, context);
    std::vector<RequirementOwner> owners;
    for (std::size_t i = 0; i < model.graph.requirements.size(); ++i)
        owners.push_back(
            {model.graph.requirements[i].identity, model.entities[model.indices[i].entity]});
    return {std::move(model.chart),
            std::move(prepared),
            std::move(owners),
            std::move(model.profiles),
            std::move(model.patterns),
            std::move(model.measures),
            3,
            digest};
}
