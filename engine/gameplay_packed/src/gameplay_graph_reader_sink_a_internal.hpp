// Included by gameplay_graph_reader_internal.hpp after the GraphSink declaration.

auto GraphSink::onEvent(const Event& event) -> core::Result<void> {
    try {
        consume(event);
        return {};
    } catch (Failure& error) {
        if (error.error.code().starts_with("packed.") || error.error.code().starts_with("chart."))
            return core::unexpected(
                graphFailure(std::move(error.error), "graph.structure.invalid"));
        return core::unexpected(std::move(error.error));
    }
}

auto GraphSink::names(Node node) -> std::string_view {
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

auto GraphSink::fieldIndex(Node node, std::string_view key) -> unsigned {
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

auto GraphSink::required(Node node) -> std::uint64_t {
    const auto fields = names(node);
    return (std::uint64_t{1} << (1 + std::count(fields.begin(), fields.end(), ' '))) - 1;
}

auto GraphSink::unsignedInteger(const Event& e, std::uint64_t maximum) -> std::uint64_t {
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

auto GraphSink::signedInteger(const Event& e) -> std::int64_t {
    if (e.kind == Kind::SignedInteger)
        return e.signedInteger;
    if (e.kind == Kind::UnsignedInteger && e.unsignedInteger <= INT64_MAX)
        return static_cast<std::int64_t>(e.unsignedInteger);
    fail("graph.integer.out_of_range", "Expected physical i64 token");
}

auto GraphSink::text(const Event& e) -> std::string {
    if (e.kind != Kind::String)
        graphInvalid("Expected Graph string");
    return std::string{e.text};
}

auto GraphSink::number(const Event& e) -> double {
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

auto GraphSink::f32(double value) -> float {
    const auto converted = static_cast<float>(value);
    if (!std::isfinite(converted) || static_cast<double>(converted) != value ||
        (value == 0 && std::signbit(converted) != std::signbit(value)))
        graphInvalid("Static Graph number does not round trip f32");
    return converted;
}

template <typename T> auto GraphSink::data(Frame& frame) -> T& {
    return std::get<T>(frame.data);
}

void GraphSink::addStringBytes(std::size_t size) {
    if (size > limits_.maxRowDecodedStringBytes - rowStringBytes_)
        fail("graph.budget.exceeded", "Graph row decoded string budget exceeded");
    rowStringBytes_ += size;
}

void GraphSink::atom(GraphAtom value) {
    if (atoms_.size() >= limits_.maxRowAtoms)
        fail("graph.budget.exceeded", "Graph row atom budget exceeded");
    atoms_.push_back(std::move(value));
}

void GraphSink::key(const Event& e) {
    auto& frame = frames_.back();
    const auto index = fieldIndex(frame.node, e.text);
    if (frame.node == Node::Root && index >= 4 && (frame.seen & 15) != 15)
        graphInvalid("Graph payload precedes the complete validated header");
    frame.seen |= std::uint64_t{1} << index;
    frame.key = e.text;
}

void GraphSink::push(Node node, Table table) {
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
        frame.data = chart::TempoEvent{chart::RationalBeat::zero(), chart::RationalBeat::zero()};
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

auto GraphSink::table(std::string_view key) -> Table {
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

void GraphSink::start(bool object) {
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
