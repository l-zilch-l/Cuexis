// Included by gameplay_graph_reader_internal.hpp after the GraphSink declaration.

void GraphSink::scalar(const Event& e) {
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
                fail("graph.header.unsupported_revision", "Unsupported explicit Graph revision");
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
void GraphSink::decodeRow(std::vector<T>& values, GraphInput& in, F visitor) {
    T value{};
    visitor(in, value);
    if (in.position != atoms_.size())
        graphInvalid("Trailing Graph typed row atoms");
    values.push_back(std::move(value));
}

void GraphSink::finishRow(Table selected) {
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
        decodeRow(model_.graph.relations, in, [&](auto& a, auto& v) { relationRow(a, v, model_); });
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

void GraphSink::end() {
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

void GraphSink::consume(const Event& event) {
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
