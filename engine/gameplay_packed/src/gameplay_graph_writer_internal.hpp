// Included inside the Capsule adapter namespace, after the shared typed visitors.
// This writer never constructs a JSON DOM or a second semantic graph.
class GraphText final {
  public:
    explicit GraphText(GraphWriterLimits limits) : limits_(limits) {}
    void append(std::string_view text) {
        if (text.size() > limits_.maxBytes - output_.size())
            fail("graph.budget.exceeded", "Graph text byte budget exceeded");
        output_.append(text);
    }
    void key(std::string_view name) {
        auto& frame = stack_.back();
        if (!frame.object || frame.expectValue)
            fail("graph.structure.invalid", "Graph Writer member state is invalid");
        comma(frame);
        quoted(name);
        append(":");
        frame.expectValue = true;
    }
    void string(std::string_view text) {
        beforeValue();
        quoted(text);
    }
    void boolean(bool value) {
        beforeValue();
        append(value ? "true" : "false");
    }
    void null() {
        beforeValue();
        append("null");
    }
    template <typename T> void integer(T value) {
        char buffer[32];
        const auto result = std::to_chars(std::begin(buffer), std::end(buffer), value);
        if (result.ec != std::errc{})
            fail("graph.integer.out_of_range", "Graph integer formatting failed");
        beforeValue();
        append({buffer, static_cast<std::size_t>(result.ptr - buffer)});
    }
    void number(double value) {
        if (!std::isfinite(value))
            fail("graph.structure.invalid", "Graph static float must be finite");
        char buffer[64];
        const auto result =
            std::to_chars(std::begin(buffer), std::end(buffer), value, std::chars_format::general,
                          std::numeric_limits<double>::max_digits10);
        if (result.ec != std::errc{})
            fail("graph.structure.invalid", "Graph float formatting failed");
        const std::string_view text{buffer, static_cast<std::size_t>(result.ptr - buffer)};
        beforeValue();
        append(text);
        // Preserve floating classification, including negative zero, across JSON SAX.
        if (text.find_first_of(".eE") == std::string_view::npos)
            append(".0");
    }
    void begin(bool object) {
        beforeValue();
        append(object ? "{" : "[");
        stack_.push_back({object, true, false});
    }
    void end() {
        const auto frame = stack_.back();
        if (frame.expectValue)
            fail("graph.structure.invalid", "Graph Writer member has no value");
        stack_.pop_back();
        append(frame.object ? "}" : "]");
    }
    template <typename F> void object(F fields) {
        begin(true);
        fields();
        end();
    }
    template <typename F> void array(F elements) {
        begin(false);
        elements();
        end();
    }
    auto take() -> std::string {
        if (!stack_.empty())
            fail("graph.structure.invalid", "Graph Writer has an unclosed container");
        return std::move(output_);
    }
    auto limits() const -> GraphWriterLimits {
        return limits_;
    }

  private:
    struct Frame {
        bool object, first, expectValue;
    };
    void comma(Frame& frame) {
        if (!frame.first)
            append(",");
        frame.first = false;
    }
    void beforeValue() {
        if (stack_.empty())
            return;
        auto& frame = stack_.back();
        if (!frame.object) {
            comma(frame);
            return;
        }
        if (!frame.expectValue)
            fail("graph.structure.invalid", "Graph Writer value has no member name");
        frame.expectValue = false;
    }
    void quoted(std::string_view value) {
        if (!utf8(value))
            fail("graph.structure.invalid", "Graph string must be valid UTF-8");
        if (value.size() > limits_.maxStringBytes)
            fail("graph.budget.exceeded", "Graph decoded string budget exceeded");
        append("\"");
        constexpr char digits[] = "0123456789abcdef";
        for (const unsigned char c : value) {
            if (c == '"' || c == '\\') {
                const char escaped[]{'\\', static_cast<char>(c)};
                append({escaped, 2});
            } else if (c < 0x20) {
                const char escaped[]{'\\', 'u', '0', '0', digits[c >> 4], digits[c & 15]};
                append({escaped, 6});
            } else {
                const char byte = static_cast<char>(c);
                append({&byte, 1});
            }
        }
        append("\"");
    }
    GraphWriterLimits limits_;
    std::string output_;
    std::vector<Frame> stack_;
};

struct GraphOutput final {
    static constexpr bool reading = false;
    Mode mode{Mode::wire};
    GraphText& output;
    std::size_t atoms{};
    void atom() {
        if (atoms >= output.limits().maxRowAtoms)
            fail("graph.budget.exceeded", "Graph typed row atom budget exceeded");
        ++atoms;
    }
    void tag(std::uint8_t value) {
        atom();
        output.integer(value);
    }
    void value(std::uint32_t& v) {
        atom();
        output.integer(v);
    }
    void value(std::uint64_t& v) {
        atom();
        output.integer(v);
    }
    void value(std::int64_t& v) {
        atom();
        output.integer(v);
    }
    void value(bool& v) {
        atom();
        output.boolean(v);
    }
    void text(std::string_view v) {
        atom();
        output.string(v);
    }
    void value(std::string& v) {
        text(v);
    }
    void ref(std::uint8_t kind, std::string_view token) {
        atom();
        output.object([&] {
            output.key("kind");
            output.integer(kind);
            output.key("token");
            output.string(token);
        });
    }
    void count(std::size_t size) {
        auto n = u32(size);
        value(n);
    }
    template <typename... T> void fields(T&... v) {
        (value(v), ...);
    }
    template <typename T> void value(std::vector<T>& v) {
        list(v);
    }
    template <typename T> void list(std::vector<T>& v) {
        count(v.size());
        for (auto& row : v)
            value(row);
    }
    template <typename T> void value(std::optional<T>& v) {
        option(v);
    }
    template <typename T> void option(std::optional<T>& v) {
        bool present = v.has_value();
        value(present);
        if (v)
            value(*v);
    }
    template <typename T> void value(T& v) {
        visit(*this, v);
    }
    void raw(std::span<const std::byte>) {
        fail("graph.structure.invalid", "Binary semantic preimage cannot become a Graph atom");
    }
};

template <typename T> void graphTransform(GraphText& out, const T& value) {
    out.object([&] {
        out.key("position");
        out.array([&] {
            out.number(value.position.x);
            out.number(value.position.y);
            out.number(value.position.z);
        });
        out.key("rotation");
        out.array([&] {
            out.number(value.rotation.x);
            out.number(value.rotation.y);
            out.number(value.rotation.z);
            out.number(value.rotation.w);
        });
        out.key("scale");
        out.array([&] {
            out.number(value.scale.x);
            out.number(value.scale.y);
            out.number(value.scale.z);
        });
    });
}
void graphIdentity(GraphText& out, const chart::CanonicalEntityIdentity& identity) {
    out.object([&] {
        out.key("kind");
        out.integer(static_cast<unsigned>(identity.index()));
        std::visit(
            [&](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, chart::ExplicitEntityIdentity>) {
                    out.key("objectId");
                    out.string(value.objectId.value);
                } else {
                    out.key("chartId");
                    out.string(value.chartId.value);
                    out.key("bindingId");
                    out.string(value.bindingId);
                    out.key("moduleId");
                    out.string(value.moduleId);
                    out.key("exportId");
                    out.string(value.exportId);
                    out.key("path");
                    out.array([&] {
                        for (const auto& step : value.path)
                            out.object([&] {
                                out.key("nodeId");
                                out.string(step.nodeId);
                                out.key("iterationIndexPlusOne");
                                out.integer(step.iterationIndexPlusOne);
                            });
                    });
                }
            },
            identity);
    });
}
void graphRational(GraphText& out, const chart::RationalBeat& value) {
    out.array([&] {
        out.integer(value.numerator());
        out.integer(value.denominator());
    });
}
template <typename T> void graphCameraFields(GraphText& out, const T& value) {
    out.key("type");
    out.string(value.type);
    out.key("fovY");
    out.number(value.fovY);
    out.key("nearPlane");
    out.number(value.nearPlane);
    out.key("farPlane");
    out.number(value.farPlane);
}
void graphStaticChart(GraphText& out, const chart::CanonicalSemanticChart& source) {
    // The carrier writes the same canonical set order used by the Packed preimage.
    // This is a typed ordering pass, with no binary or JSON round trip.
    auto value = source;
    std::sort(value.features.begin(), value.features.end(),
              [](const auto& left, const auto& right) { return left.id < right.id; });
    std::sort(value.timing.tempoEvents.begin(), value.timing.tempoEvents.end(),
              [](const auto& left, const auto& right) { return left.startBeat < right.startBeat; });
    std::sort(value.timing.stops.begin(), value.timing.stops.end(),
              [](const auto& left, const auto& right) { return left.beat < right.beat; });
    std::sort(value.resourceClosure.resources.begin(), value.resourceClosure.resources.end());
    std::vector<std::pair<Bytes, chart::CanonicalEntity>> keyed;
    for (auto& entity : value.entities) {
        std::sort(entity.components.begin(), entity.components.end(),
                  [](const auto& left, const auto& right) { return left.index() < right.index(); });
        auto key = checked(chart::packed::identity_detail::canonicalIdentityBytes(entity.identity));
        keyed.emplace_back(std::move(key), std::move(entity));
    }
    std::sort(keyed.begin(), keyed.end(), [](const auto& left, const auto& right) {
        return chart::packed::identity_detail::ByteKeyLess{}(left.first, right.first);
    });
    value.entities.clear();
    for (auto& item : keyed)
        value.entities.push_back(std::move(item.second));

    out.object([&] {
        out.key("chartId");
        out.string(value.chartId.value);
        out.key("mainMusic");
        if (value.mainMusic)
            out.string(value.mainMusic->value);
        else
            out.null();
        out.key("features");
        out.array([&] {
            for (const auto& feature : value.features)
                out.object([&] {
                    out.key("id");
                    out.string(feature.id);
                    out.key("version");
                    out.integer(feature.version);
                });
        });
        out.key("timing");
        out.object([&] {
            out.key("offsetMs");
            out.number(value.timing.offsetMs);
            out.key("defaultBpm");
            out.number(value.timing.defaultBpm);
            out.key("tempoEvents");
            out.array([&] {
                for (const auto& tempo : value.timing.tempoEvents)
                    out.object([&] {
                        out.key("startBeat");
                        graphRational(out, tempo.startBeat);
                        out.key("durationBeats");
                        graphRational(out, tempo.durationBeats);
                        out.key("startBpm");
                        out.number(tempo.startBpm);
                        out.key("endBpm");
                        out.number(tempo.endBpm);
                        out.key("startSlope");
                        out.number(tempo.startSlope);
                        out.key("endSlope");
                        out.number(tempo.endSlope);
                    });
            });
            out.key("stops");
            out.array([&] {
                for (const auto& stop : value.timing.stops)
                    out.object([&] {
                        out.key("beat");
                        graphRational(out, stop.beat);
                        out.key("durationMs");
                        out.number(stop.durationMs);
                    });
            });
        });
        out.key("defaultCamera");
        out.object([&] {
            const auto& camera = value.defaultCamera;
            graphCameraFields(out, camera);
            out.key("pitch");
            out.number(camera.pitch);
            out.key("yaw");
            out.number(camera.yaw);
            out.key("roll");
            out.number(camera.roll);
            out.key("defaultTransform");
            if (camera.defaultTransform)
                graphTransform(out, *camera.defaultTransform);
            else
                out.null();
        });
        out.key("resourceClosure");
        out.array([&] {
            for (const auto& resource : value.resourceClosure.resources)
                out.object([&] {
                    out.key("assetId");
                    out.string(resource.assetId.value);
                    out.key("use");
                    out.integer(static_cast<unsigned>(resource.use));
                });
        });
        out.key("entities");
        out.array([&] {
            for (const auto& entity : value.entities)
                out.object([&] {
                    out.key("identity");
                    graphIdentity(out, entity.identity);
                    out.key("parent");
                    if (entity.parent)
                        graphIdentity(out, *entity.parent);
                    else
                        out.null();
                    out.key("components");
                    out.array([&] {
                        for (const auto& component : entity.components)
                            out.object([&] {
                                out.key("kind");
                                out.integer(static_cast<unsigned>(component.index()));
                                std::visit(
                                    [&](const auto& v) {
                                        using T = std::decay_t<decltype(v)>;
                                        if constexpr (std::is_same_v<T,
                                                                     chart::CanonicalTransform>) {
                                            out.key("value");
                                            graphTransform(out, v);
                                        } else if constexpr (std::is_same_v<
                                                                 T, chart::CanonicalRenderable>) {
                                            out.key("mesh");
                                            out.string(v.mesh.value);
                                            out.key("material");
                                            out.string(v.material.value);
                                            out.key("alpha");
                                            out.integer(v.alpha);
                                        } else {
                                            graphCameraFields(out, v);
                                        }
                                    },
                                    component);
                            });
                    });
                });
        });
    });
}
