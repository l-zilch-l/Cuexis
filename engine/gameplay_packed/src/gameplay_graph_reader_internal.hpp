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
        std::string key;
        Data data;
    };

  public:
    GraphSink(Model& model, GraphReaderLimits limits, chart::PackedChartLimits physical)
        : model_(model), limits_(limits), physical_(physical) {}
    auto onEvent(const Event& event) -> core::Result<void> override {
        try {
            consume(event);
            return {};
        } catch (Failure& error) {
            if (error.error.code().starts_with("packed.") ||
                error.error.code().starts_with("chart."))
                return core::unexpected(
                    graphFailure(std::move(error.error), "graph.structure.invalid"));
            return core::unexpected(std::move(error.error));
        }
    }
    auto identity() const -> const std::string& {
        return identity_;
    }
    bool complete() const {
        return complete_ && frames_.empty();
    }

  private:
    static auto names(Node node) -> std::string_view {
        switch (node) {
        case Node::Root:
            return "format graphFormatRevision capsuleRevision semanticIdentity staticChart GPH0 "
                   "GPR0 GPD0 GRC0";
        case Node::Static:
            return "chartId mainMusic features timing defaultCamera resourceClosure entities";
        case Node::Timing:
            return "offsetMs defaultBpm tempoEvents stops";
        case Node::Camera:
            return "type fovY nearPlane farPlane pitch yaw roll defaultTransform";
        case Node::Transform:
            return "position rotation scale";
        case Node::Identity:
            return "kind objectId chartId bindingId moduleId exportId path";
        case Node::Entity:
            return "identity parent components";
        case Node::Component:
            return "kind value mesh material alpha type fovY nearPlane farPlane";
        case Node::Feature:
            return "id version";
        case Node::Tempo:
            return "startBeat durationBeats startBpm endBpm startSlope endSlope";
        case Node::Stop:
            return "beat durationMs";
        case Node::ResourceUse:
            return "assetId use";
        case Node::PathStep:
            return "nodeId iterationIndexPlusOne";
        case Node::Definitions:
            return "patterns measures judgementDomains mergedDeclarations";
        case Node::Coordination:
            return "resources relations solverProfiles factBindings claims";
        case Node::Reference:
            return "kind token";
        default:
            return {};
        }
    }
    static auto fieldIndex(Node node, std::string_view key) -> unsigned {
        auto fields = names(node);
        for (unsigned index = 0; !fields.empty(); ++index) {
            const auto end = fields.find(' ');
            if (fields.substr(0, end) == key)
                return index;
            if (end == std::string_view::npos)
                break;
            fields.remove_prefix(end + 1);
        }
        graphInvalid("Unknown Graph record field");
    }
    static auto required(Node node) -> std::uint64_t {
        const auto fields = names(node);
        return (std::uint64_t{1} << (1 + std::count(fields.begin(), fields.end(), ' '))) - 1;
    }
    static auto unsignedInteger(const Event& e, std::uint64_t maximum = UINT64_MAX)
        -> std::uint64_t {
        std::uint64_t value{};
        if (e.kind == Kind::UnsignedInteger)
            value = e.unsignedInteger;
        else if (e.kind == Kind::SignedInteger && e.signedInteger >= 0)
            value = static_cast<std::uint64_t>(e.signedInteger);
        else
            fail("graph.integer.out_of_range", "Expected physical unsigned integer token");
        if (value > maximum)
            fail("graph.integer.out_of_range", "Graph integer range exceeded");
        return value;
    }
    static auto signedInteger(const Event& e) -> std::int64_t {
        if (e.kind == Kind::SignedInteger)
            return e.signedInteger;
        if (e.kind == Kind::UnsignedInteger && e.unsignedInteger <= INT64_MAX)
            return static_cast<std::int64_t>(e.unsignedInteger);
        fail("graph.integer.out_of_range", "Expected physical i64 token");
    }
    static auto text(const Event& e) -> std::string {
        if (e.kind != Kind::String)
            graphInvalid("Expected Graph string");
        return std::string{e.text};
    }
    static auto number(const Event& e) -> double {
        double value{};
        if (e.kind == Kind::Number)
            value = e.number;
        else if (e.kind == Kind::SignedInteger) {
            value = static_cast<double>(e.signedInteger);
            if (value < -0x1p63 || value >= 0x1p63 ||
                static_cast<std::int64_t>(value) != e.signedInteger)
                graphInvalid("Static integer does not convert exactly to f64");
        } else if (e.kind == Kind::UnsignedInteger) {
            value = static_cast<double>(e.unsignedInteger);
            if (value >= 0x1p64 || static_cast<std::uint64_t>(value) != e.unsignedInteger)
                graphInvalid("Static integer does not convert exactly to f64");
        } else
            graphInvalid("Expected finite static Graph number");
        if (!std::isfinite(value))
            graphInvalid("Static Graph number is not finite");
        return value;
    }
    static auto f32(double value) -> float {
        const auto converted = static_cast<float>(value);
        if (!std::isfinite(converted) || static_cast<double>(converted) != value ||
            (value == 0 && std::signbit(converted) != std::signbit(value)))
            graphInvalid("Static Graph number does not round trip f32");
        return converted;
    }
    template <typename T> static auto data(Frame& frame) -> T& {
        return std::get<T>(frame.data);
    }
    void addStringBytes(std::size_t size) {
        if (size > limits_.maxRowDecodedStringBytes - rowStringBytes_)
            fail("graph.budget.exceeded", "Graph row decoded string budget exceeded");
        rowStringBytes_ += size;
    }
    void atom(GraphAtom value) {
        if (atoms_.size() >= limits_.maxRowAtoms)
            fail("graph.budget.exceeded", "Graph row atom budget exceeded");
        atoms_.push_back(std::move(value));
    }
    void key(const Event& e) {
        auto& frame = frames_.back();
        const auto index = fieldIndex(frame.node, e.text);
        if (frame.node == Node::Root && index >= 4 && (frame.seen & 15) != 15)
            graphInvalid("Graph payload precedes the complete validated header");
        frame.seen |= std::uint64_t{1} << index;
        frame.key = e.text;
    }
    void push(Node node, Table table = Table::Global) {
        Frame frame{node, table};
        switch (node) {
        case Node::Camera:
            frame.data = chart::CameraData{};
            break;
        case Node::Transform:
            frame.data = chart::CanonicalTransform{};
            break;
        case Node::Identity:
            frame.data = IdentityValue{};
            break;
        case Node::Entity:
            frame.data = chart::CanonicalEntity{};
            break;
        case Node::Component:
            frame.data = ComponentValue{};
            break;
        case Node::Feature:
            frame.data = chart::CanonicalFeature{};
            break;
        case Node::Tempo:
            frame.data =
                chart::TempoEvent{chart::RationalBeat::zero(), chart::RationalBeat::zero()};
            break;
        case Node::Stop:
            frame.data = chart::TimingStop{chart::RationalBeat::zero()};
            break;
        case Node::ResourceUse:
            frame.data = chart::CanonicalResourceUse{};
            break;
        case Node::PathStep:
            frame.data = chart::SemanticIdentityStep{};
            break;
        case Node::Vector:
        case Node::Rational:
            frame.data = Numbers{};
            break;
        case Node::Reference:
            frame.data = Ref{};
            break;
        case Node::Row:
            atoms_.clear();
            rowStringBytes_ = 0;
            break;
        default:
            break;
        }
        frames_.push_back(std::move(frame));
    }
    static auto table(std::string_view key) -> Table {
        if (key == "GPR0")
            return Table::Requirement;
        if (key == "patterns")
            return Table::Pattern;
        if (key == "measures")
            return Table::Measure;
        if (key == "judgementDomains")
            return Table::Domain;
        if (key == "mergedDeclarations")
            return Table::Merged;
        if (key == "resources")
            return Table::Resource;
        if (key == "relations")
            return Table::Relation;
        if (key == "solverProfiles")
            return Table::Solver;
        if (key == "factBindings")
            return Table::Binding;
        if (key == "claims")
            return Table::Claim;
        graphInvalid("Unknown Graph row table");
    }
    void start(bool object) {
        if (frames_.empty()) {
            if (!object || started_)
                graphInvalid("Graph root must be one object");
            started_ = true;
            push(Node::Root);
            return;
        }
        const auto parent = frames_.back().node;
        const auto key = frames_.back().key;
        if (object) {
            if (parent == Node::Root && key == "staticChart") {
                push(Node::Static);
                return;
            }
            if (parent == Node::Root && key == "GPD0") {
                push(Node::Definitions);
                return;
            }
            if (parent == Node::Root && key == "GRC0") {
                push(Node::Coordination);
                return;
            }
            if (parent == Node::Static && key == "timing") {
                push(Node::Timing);
                return;
            }
            if (parent == Node::Static && key == "defaultCamera") {
                push(Node::Camera);
                return;
            }
            if (parent == Node::Camera && key == "defaultTransform") {
                push(Node::Transform);
                return;
            }
            if (parent == Node::Component && key == "value") {
                push(Node::Transform);
                return;
            }
            if (parent == Node::Entity && (key == "identity" || key == "parent")) {
                push(Node::Identity);
                return;
            }
            if (parent == Node::Features) {
                push(Node::Feature);
                return;
            }
            if (parent == Node::Tempos) {
                push(Node::Tempo);
                return;
            }
            if (parent == Node::Stops) {
                push(Node::Stop);
                return;
            }
            if (parent == Node::Entities) {
                if (model_.chart.entities.size() >= physical_.maxPackedEntities)
                    fail("graph.budget.exceeded", "Existing entity ceiling exceeded");
                push(Node::Entity);
                return;
            }
            if (parent == Node::Components) {
                push(Node::Component);
                return;
            }
            if (parent == Node::Resources) {
                push(Node::ResourceUse);
                return;
            }
            if (parent == Node::Path) {
                push(Node::PathStep);
                return;
            }
            if (parent == Node::Row) {
                if (atoms_.size() >= limits_.maxRowAtoms)
                    fail("graph.budget.exceeded", "Graph row atom budget exceeded");
                push(Node::Reference);
                return;
            }
        } else {
            if (parent == Node::Root && key == "GPH0") {
                push(Node::Row);
                return;
            }
            if ((parent == Node::Root && key == "GPR0") || parent == Node::Definitions ||
                parent == Node::Coordination) {
                push(Node::Rows, table(key));
                return;
            }
            if (parent == Node::Rows) {
                const auto selected = frames_.back().table;
                if (selected == Table::Requirement &&
                    model_.graph.requirements.size() >= physical_.maxPackedRequirements)
                    fail("graph.budget.exceeded", "Existing Requirement ceiling exceeded");
                push(Node::Row, selected);
                return;
            }
            if (parent == Node::Static && key == "features") {
                push(Node::Features);
                return;
            }
            if (parent == Node::Static && key == "resourceClosure") {
                push(Node::Resources);
                return;
            }
            if (parent == Node::Static && key == "entities") {
                push(Node::Entities);
                return;
            }
            if (parent == Node::Timing && key == "tempoEvents") {
                push(Node::Tempos);
                return;
            }
            if (parent == Node::Timing && key == "stops") {
                push(Node::Stops);
                return;
            }
            if (parent == Node::Identity && key == "path") {
                push(Node::Path);
                return;
            }
            if (parent == Node::Entity && key == "components") {
                push(Node::Components);
                return;
            }
            if (parent == Node::Transform) {
                push(Node::Vector);
                return;
            }
            if ((parent == Node::Tempo && (key == "startBeat" || key == "durationBeats")) ||
                (parent == Node::Stop && key == "beat")) {
                push(Node::Rational);
                return;
            }
        }
        graphInvalid("Graph container does not match field type");
    }
    void scalar(const Event& e) {
        auto& frame = frames_.back();
        const auto& key = frame.key;
        switch (frame.node) {
        case Node::Root:
            if (key == "format") {
                if (text(e) != "cuexis.gameplay-graph")
                    fail("graph.header.unsupported_format", "Unsupported Graph format");
            } else if (key == "graphFormatRevision" || key == "capsuleRevision") {
                const auto expected = key == "capsuleRevision" ? 3 : 1;
                if (unsignedInteger(e, UINT32_MAX) != static_cast<unsigned>(expected))
                    fail("graph.header.unsupported_revision",
                         "Unsupported explicit Graph revision");
            } else if (key == "semanticIdentity") {
                identity_ = text(e);
                if (identity_.size() != 64 ||
                    identity_.find_first_not_of("0123456789abcdef") != std::string::npos)
                    graphInvalid("Graph semanticIdentity must be lowercase SHA-256");
            } else
                graphInvalid("Graph payload must be a typed container");
            break;
        case Node::Static:
            if (key == "chartId")
                model_.chart.chartId.value = text(e);
            else if (key == "mainMusic") {
                if (e.kind == Kind::Null)
                    model_.chart.mainMusic.reset();
                else
                    model_.chart.mainMusic = chart::AssetId{text(e)};
            } else
                graphInvalid("Static Graph field requires a container");
            break;
        case Node::Timing:
            if (key == "offsetMs")
                model_.chart.timing.offsetMs = number(e);
            else if (key == "defaultBpm")
                model_.chart.timing.defaultBpm = number(e);
            else
                graphInvalid("Timing Graph field requires a container");
            break;
        case Node::Feature:
            if (key == "id")
                data<chart::CanonicalFeature>(frame).id = text(e);
            else
                data<chart::CanonicalFeature>(frame).version =
                    static_cast<std::uint32_t>(unsignedInteger(e, UINT32_MAX));
            break;
        case Node::Camera: {
            auto& camera = data<chart::CameraData>(frame);
            if (key == "type")
                camera.type = text(e);
            else if (key == "defaultTransform") {
                if (e.kind != Kind::Null)
                    graphInvalid("Expected optional camera Transform");
                camera.defaultTransform.reset();
            } else {
                const auto n = number(e);
                if (key == "fovY")
                    camera.fovY = n;
                else if (key == "nearPlane")
                    camera.nearPlane = n;
                else if (key == "farPlane")
                    camera.farPlane = n;
                else if (key == "pitch")
                    camera.pitch = n;
                else if (key == "yaw")
                    camera.yaw = n;
                else
                    camera.roll = n;
            }
            break;
        }
        case Node::Identity: {
            auto& identity = data<IdentityValue>(frame);
            if (key == "kind")
                identity.kind = static_cast<std::uint32_t>(unsignedInteger(e, 1));
            else if (key == "objectId")
                identity.explicitValue.objectId.value = text(e);
            else if (key == "chartId")
                identity.generatedValue.chartId.value = text(e);
            else if (key == "bindingId")
                identity.generatedValue.bindingId = text(e);
            else if (key == "moduleId")
                identity.generatedValue.moduleId = text(e);
            else if (key == "exportId")
                identity.generatedValue.exportId = text(e);
            else
                graphInvalid("Generated path must be an array");
            break;
        }
        case Node::Entity:
            if (key != "parent" || e.kind != Kind::Null)
                graphInvalid("Entity field requires a typed container");
            data<chart::CanonicalEntity>(frame).parent.reset();
            break;
        case Node::Component: {
            auto& v = data<ComponentValue>(frame);
            if (key == "kind")
                v.kind = static_cast<std::uint32_t>(unsignedInteger(e, 2));
            else if (key == "mesh")
                v.renderable.mesh.value = text(e);
            else if (key == "material")
                v.renderable.material.value = text(e);
            else if (key == "alpha")
                v.renderable.alpha = static_cast<std::uint8_t>(unsignedInteger(e, UINT8_MAX));
            else if (key == "type")
                v.camera.type = text(e);
            else if (key == "fovY")
                v.camera.fovY = number(e);
            else if (key == "nearPlane")
                v.camera.nearPlane = number(e);
            else if (key == "farPlane")
                v.camera.farPlane = number(e);
            else
                graphInvalid("Transform component requires a typed value");
            break;
        }
        case Node::Tempo: {
            auto& v = data<chart::TempoEvent>(frame);
            if (key == "startBpm")
                v.startBpm = number(e);
            else if (key == "endBpm")
                v.endBpm = number(e);
            else if (key == "startSlope")
                v.startSlope = number(e);
            else if (key == "endSlope")
                v.endSlope = number(e);
            else
                graphInvalid("Tempo Beat requires a rational pair");
            break;
        }
        case Node::Stop:
            if (key != "durationMs")
                graphInvalid("Stop Beat requires a rational pair");
            data<chart::TimingStop>(frame).durationMs = number(e);
            break;
        case Node::ResourceUse:
            if (key == "assetId")
                data<chart::CanonicalResourceUse>(frame).assetId.value = text(e);
            else
                data<chart::CanonicalResourceUse>(frame).use =
                    static_cast<chart::CanonicalResourceUseKind>(unsignedInteger(e, 2));
            break;
        case Node::PathStep:
            if (key == "nodeId")
                data<chart::SemanticIdentityStep>(frame).nodeId = text(e);
            else
                data<chart::SemanticIdentityStep>(frame).iterationIndexPlusOne =
                    static_cast<std::uint32_t>(unsignedInteger(e, UINT32_MAX));
            break;
        case Node::Vector:
            if (frame.count >= 4)
                graphInvalid("Transform vector length exceeded");
            data<Numbers>(frame).values[frame.count++] = number(e);
            break;
        case Node::Rational:
            if (frame.count >= 2)
                graphInvalid("Rational pair length exceeded");
            data<Numbers>(frame).integers[frame.count++] = signedInteger(e);
            break;
        case Node::Reference:
            if (key == "kind")
                data<Ref>(frame).first = static_cast<std::uint8_t>(unsignedInteger(e, UINT8_MAX));
            else {
                addStringBytes(e.text.size());
                data<Ref>(frame).second = text(e);
            }
            break;
        case Node::Row:
            if (atoms_.size() >= limits_.maxRowAtoms)
                fail("graph.budget.exceeded", "Graph row atom budget exceeded");
            if (e.kind == Kind::Boolean)
                atom(e.boolean);
            else if (e.kind == Kind::SignedInteger)
                atom(e.signedInteger);
            else if (e.kind == Kind::UnsignedInteger)
                atom(e.unsignedInteger);
            else if (e.kind == Kind::String) {
                addStringBytes(e.text.size());
                atom(text(e));
            } else
                graphInvalid("Graph row atom cannot be a float, null or container");
            break;
        default:
            graphInvalid("Graph array expects typed records");
        }
    }
    template <typename T, typename F>
    void decodeRow(std::vector<T>& values, GraphInput& in, F visitor) {
        T value{};
        visitor(in, value);
        if (in.position != atoms_.size())
            graphInvalid("Trailing Graph typed row atoms");
        values.push_back(std::move(value));
    }
    void finishRow(Table selected) {
        GraphInput in{atoms_};
        switch (selected) {
        case Table::Global:
            globalRow(in, model_);
            break;
        case Table::Requirement: {
            RequirementRecord value{};
            RequirementIndices indices;
            requirementRow(in, value, indices, model_);
            if (in.position != atoms_.size())
                graphInvalid("Trailing Graph typed row atoms");
            model_.graph.requirements.push_back(std::move(value));
            model_.indices.push_back(std::move(indices));
            break;
        }
        case Table::Pattern:
            decodeRow(model_.patterns, in, [](auto& a, auto& v) { patternRow(a, v); });
            break;
        case Table::Measure:
            decodeRow(model_.measures, in, [](auto& a, auto& v) { measureRow(a, v); });
            break;
        case Table::Domain:
            decodeRow(model_.graph.judgementDomains, in, [](auto& a, auto& v) { domainRow(a, v); });
            break;
        case Table::Merged:
            decodeRow(model_.graph.mergedNamespace.declarations, in,
                      [](auto& a, auto& v) { mergedRow(a, v); });
            break;
        case Table::Resource:
            decodeRow(model_.graph.resources, in, [](auto& a, auto& v) { resourceRow(a, v); });
            break;
        case Table::Relation:
            decodeRow(model_.graph.relations, in,
                      [&](auto& a, auto& v) { relationRow(a, v, model_); });
            break;
        case Table::Solver:
            decodeRow(model_.graph.solverProfiles, in, [](auto& a, auto& v) { solverRow(a, v); });
            break;
        case Table::Binding:
            decodeRow(model_.graph.factBindings, in, [](auto& a, auto& v) { bindingRow(a, v); });
            break;
        case Table::Claim:
            decodeRow(model_.claims, in, [&](auto& a, auto& v) { claimRow(a, v, model_); });
            break;
        }
        if (in.position != atoms_.size())
            graphInvalid("Trailing Graph typed row atoms");
        atoms_.clear();
        rowStringBytes_ = 0;
    }
    void end() {
        auto frame = std::move(frames_.back());
        frames_.pop_back();
        const auto fields = names(frame.node);
        if (!fields.empty()) {
            auto expected = required(frame.node);
            if (frame.node == Node::Identity)
                expected = data<IdentityValue>(frame).kind == 0 ? 3 : 125;
            if (frame.node == Node::Component) {
                const auto k = data<ComponentValue>(frame).kind;
                expected = k == 0 ? 3 : k == 1 ? 29 : 481;
            }
            if (frame.seen != expected)
                graphInvalid("Graph typed record fields are incomplete or inconsistent");
        }
        if (frame.node == Node::Root) {
            complete_ = true;
            return;
        }
        auto& parent = frames_.back();
        switch (frame.node) {
        case Node::Camera:
            model_.chart.defaultCamera = std::move(data<chart::CameraData>(frame));
            break;
        case Node::Transform: {
            auto value = std::move(data<chart::CanonicalTransform>(frame));
            if (parent.node == Node::Camera)
                data<chart::CameraData>(parent).defaultTransform =
                    chart::TransformData{value.position, value.rotation, value.scale};
            else
                data<ComponentValue>(parent).transform = std::move(value);
            break;
        }
        case Node::Identity: {
            auto& v = data<IdentityValue>(frame);
            chart::CanonicalEntityIdentity identity =
                v.kind == 0 ? chart::CanonicalEntityIdentity{std::move(v.explicitValue)}
                            : chart::CanonicalEntityIdentity{std::move(v.generatedValue)};
            if (parent.key == "identity")
                data<chart::CanonicalEntity>(parent).identity = std::move(identity);
            else
                data<chart::CanonicalEntity>(parent).parent = std::move(identity);
            break;
        }
        case Node::Entity:
            model_.chart.entities.push_back(std::move(data<chart::CanonicalEntity>(frame)));
            break;
        case Node::Component: {
            auto& v = data<ComponentValue>(frame);
            auto& entity = data<chart::CanonicalEntity>(frames_[frames_.size() - 2]);
            if (v.kind == 0)
                entity.components.emplace_back(std::move(v.transform));
            else if (v.kind == 1)
                entity.components.emplace_back(std::move(v.renderable));
            else
                entity.components.emplace_back(std::move(v.camera));
            break;
        }
        case Node::Feature:
            model_.chart.features.push_back(std::move(data<chart::CanonicalFeature>(frame)));
            break;
        case Node::Tempo:
            model_.chart.timing.tempoEvents.push_back(std::move(data<chart::TempoEvent>(frame)));
            break;
        case Node::Stop:
            model_.chart.timing.stops.push_back(std::move(data<chart::TimingStop>(frame)));
            break;
        case Node::ResourceUse:
            model_.chart.resourceClosure.resources.push_back(
                std::move(data<chart::CanonicalResourceUse>(frame)));
            break;
        case Node::PathStep:
            data<IdentityValue>(frames_[frames_.size() - 2])
                .generatedValue.path.push_back(std::move(data<chart::SemanticIdentityStep>(frame)));
            break;
        case Node::Vector: {
            const auto& numbers = data<Numbers>(frame).values;
            auto& transform = data<chart::CanonicalTransform>(parent);
            const auto count = parent.key == "rotation" ? 4U : 3U;
            if (frame.count != count)
                graphInvalid("Transform vector has wrong length");
            if (parent.key == "position")
                transform.position = {f32(numbers[0]), f32(numbers[1]), f32(numbers[2])};
            else if (parent.key == "scale")
                transform.scale = {f32(numbers[0]), f32(numbers[1]), f32(numbers[2])};
            else
                transform.rotation = {f32(numbers[0]), f32(numbers[1]), f32(numbers[2]),
                                      f32(numbers[3])};
            break;
        }
        case Node::Rational: {
            const auto& n = data<Numbers>(frame).integers;
            if (frame.count != 2 || n[1] <= 0)
                graphInvalid("Invalid static rational pair");
            const auto value = checked(chart::RationalBeat::create(n[0], n[1]));
            if (value.numerator() != n[0] || value.denominator() != n[1])
                graphInvalid("Static rational pair is not canonical");
            if (parent.node == Node::Tempo) {
                if (parent.key == "startBeat")
                    data<chart::TempoEvent>(parent).startBeat = value;
                else
                    data<chart::TempoEvent>(parent).durationBeats = value;
            } else
                data<chart::TimingStop>(parent).beat = value;
            break;
        }
        case Node::Reference:
            atom(std::move(data<Ref>(frame)));
            break;
        case Node::Row:
            finishRow(frame.table);
            break;
        default:
            break;
        }
        ++parent.count;
    }
    void consume(const Event& event) {
        if (event.kind == Kind::Key) {
            key(event);
            return;
        }
        if (event.kind == Kind::ObjectStart) {
            start(true);
            return;
        }
        if (event.kind == Kind::ArrayStart) {
            start(false);
            return;
        }
        if (frames_.empty())
            graphInvalid("Unexpected value after Graph root");
        if (event.kind == Kind::ObjectEnd || event.kind == Kind::ArrayEnd) {
            end();
            return;
        }
        scalar(event);
    }
    Model& model_;
    GraphReaderLimits limits_;
    chart::PackedChartLimits physical_;
    std::vector<Frame> frames_;
    std::vector<GraphAtom> atoms_;
    std::size_t rowStringBytes_{};
    std::string identity_;
    bool started_{}, complete_{};
};

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
    if (core::detail::sha256Hex(graphClosure([&] { return preimage(model, physical); })) !=
        sink.identity())
        fail("graph.identity.mismatch", "Graph structural semantic identity mismatch");
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
            3};
}
