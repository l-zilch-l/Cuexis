#include <cuexis/json/parse.hpp>

#include <nlohmann/json.hpp>

#include <exception>
#include <optional>
#include <set>
#include <utility>

namespace cuexis::json {
namespace {
class BoundedReader final : public nlohmann::json_sax<nlohmann::json> {
  public:
    explicit BoundedReader(ValueParseLimits limits, ISaxEventSink* sink = nullptr)
        : limits_(limits), sink_(sink) {}

    bool null() override {
        return scalar({SaxEventKind::Null});
    }
    bool boolean(bool value) override {
        return scalar({SaxEventKind::Boolean, value});
    }
    bool number_integer(number_integer_t value) override {
        return scalar({SaxEventKind::SignedInteger, false, static_cast<std::int64_t>(value)});
    }
    bool number_unsigned(number_unsigned_t value) override {
        return scalar({SaxEventKind::UnsignedInteger, false, 0, static_cast<std::uint64_t>(value)});
    }
    bool number_float(number_float_t value, const string_t&) override {
        return scalar({SaxEventKind::Number, false, 0, 0, value});
    }
    bool string(string_t& value) override {
        if (!stringFits(value, "string_value") || !admitValue() ||
            !forward({SaxEventKind::String, false, 0, 0, 0, value}))
            return false;
        if (sink_)
            return append({});
        return append(Value{std::move(value)});
    }
    bool binary(binary_t&) override {
        return reject("json.parse.syntax_error", "Binary values are not JSON");
    }
    bool start_object(std::size_t count) override {
        return begin(true, count);
    }
    bool start_array(std::size_t count) override {
        return begin(false, count);
    }
    bool end_object() override {
        return end();
    }
    bool end_array() override {
        return end();
    }
    bool key(string_t& value) override {
        if (!stringFits(value, "object_key"))
            return false;
        auto& frame = stack_.back();
        if (sink_ ? frame.keys.contains(value) : frame.value->object()->contains(value)) {
            error_ = core::Error{"json.parse.duplicate_key", "JSON object keys must be unique"}
                         .withContext("key", value);
            return false;
        }
        if (frame.elements >= limits_.maxContainerElements)
            return reject("json.parse.element_limit", "JSON object member limit exceeded");
        if (!forward({SaxEventKind::Key, false, 0, 0, 0, value}))
            return false;
        if (sink_)
            frame.keys.insert(std::move(value));
        else
            frame.key = std::move(value);
        return true;
    }
    bool parse_error(std::size_t position, const std::string&,
                     const nlohmann::detail::exception&) override {
        if (!error_)
            error_ = core::Error{"json.parse.syntax_error", "JSON syntax is invalid"}.withContext(
                "byte_offset", std::to_string(position));
        return false;
    }
    auto result() -> core::Result<Value> {
        auto done = completion();
        if (!done)
            return core::unexpected(std::move(done.error()));
        return std::move(*root_);
    }
    auto completion() -> core::Result<void> {
        if (error_)
            return core::unexpected(std::move(*error_));
        if (!rootSeen_ || !stack_.empty())
            return core::unexpected(core::Error{"json.parse.syntax_error", "Incomplete JSON"});
        return {};
    }

  private:
    struct Frame final {
        std::optional<Value> value;
        bool object;
        std::string key;
        std::set<std::string, std::less<>> keys;
        std::size_t elements{};
    };
    bool reject(std::string code, std::string message) {
        error_ = core::Error{std::move(code), std::move(message)};
        return false;
    }
    bool stringFits(std::string_view value, std::string_view kind) {
        if (value.size() <= limits_.text.maxStringBytes)
            return true;
        error_ = core::Error{"json.parse.string_limit", "JSON decoded string limit exceeded"}
                     .withContext("string_kind", std::string{kind})
                     .withContext("actual_bytes", std::to_string(value.size()))
                     .withContext("max_string_bytes", std::to_string(limits_.text.maxStringBytes));
        return false;
    }
    bool admitValue() {
        if (values_ >= limits_.maxValues)
            return reject("json.parse.value_limit", "JSON owning value limit exceeded");
        if (!stack_.empty() && stack_.back().elements >= limits_.maxContainerElements)
            return reject("json.parse.element_limit", "JSON container element limit exceeded");
        ++values_;
        return true;
    }
    bool forward(const SaxEvent& event) {
        if (!sink_)
            return true;
        auto accepted = sink_->onEvent(event);
        if (accepted)
            return true;
        error_ = std::move(accepted.error());
        return false;
    }
    bool scalar(const SaxEvent& event) {
        if (!admitValue() || !forward(event))
            return false;
        if (sink_)
            return append({});
        switch (event.kind) {
        case SaxEventKind::Null:
            return append(Value{});
        case SaxEventKind::Boolean:
            return append(Value{event.boolean});
        case SaxEventKind::SignedInteger:
            return append(Value{event.signedInteger});
        case SaxEventKind::UnsignedInteger:
            return append(Value{event.unsignedInteger});
        case SaxEventKind::Number:
            return append(Value{event.number});
        default:
            return reject("json.parse.failed", "Unexpected scalar callback");
        }
    }
    bool begin(bool object, std::size_t count) {
        if (stack_.size() >= limits_.text.maxDepth)
            return reject("json.parse.depth_limit", "JSON nesting depth exceeds configured limit");
        // JSON SAX reports unknown size. Never reserve from an untrusted size declaration.
        if (count != static_cast<std::size_t>(-1) && count > limits_.maxContainerElements)
            return reject("json.parse.element_limit", "JSON container element limit exceeded");
        if (!admitValue() ||
            !forward({object ? SaxEventKind::ObjectStart : SaxEventKind::ArrayStart}))
            return false;
        std::optional<Value> value;
        if (!sink_)
            value = object ? Value{Value::Object{}} : Value{Value::Array{}};
        stack_.push_back({std::move(value), object, {}, {}, 0});
        return true;
    }
    bool end() {
        if (!forward({stack_.back().object ? SaxEventKind::ObjectEnd : SaxEventKind::ArrayEnd}))
            return false;
        auto value = std::move(stack_.back().value);
        stack_.pop_back();
        return append(std::move(value));
    }
    bool append(std::optional<Value> value) {
        if (stack_.empty()) {
            root_ = std::move(value);
            rootSeen_ = true;
            return true;
        }
        auto& frame = stack_.back();
        if (!sink_) {
            if (auto* array = frame.value->array())
                array->push_back(std::move(*value));
            else
                frame.value->object()->emplace(std::move(frame.key), std::move(*value));
        }
        ++frame.elements;
        return true;
    }
    ValueParseLimits limits_;
    ISaxEventSink* sink_{};
    std::size_t values_{};
    std::vector<Frame> stack_;
    std::optional<Value> root_;
    std::optional<core::Error> error_;
    bool rootSeen_{};
};

auto validateText(std::string_view text, ValueParseLimits limits) -> core::Result<void> {
    if (!limits.text.maxBytes || !limits.text.maxDepth || !limits.text.maxStringBytes ||
        !limits.maxValues || !limits.maxContainerElements)
        return core::unexpected(core::Error{"json.parse.invalid_limits",
                                            "Explicit JSON parse limits must be positive"});
    if (text.size() > limits.text.maxBytes)
        return core::unexpected(
            core::Error{"json.parse.size_limit", "JSON input exceeds configured byte limit"});
    return {};
}
} // namespace

auto parseBounded(std::string_view text, ValueParseLimits limits) -> core::Result<Value> try {
    if (auto valid = validateText(text, limits); !valid)
        return core::unexpected(std::move(valid.error()));
    BoundedReader reader{limits};
    (void)nlohmann::json::sax_parse(text.begin(), text.end(), &reader);
    return reader.result();
} catch (const std::exception& exception) {
    return core::unexpected(
        core::Error{"json.parse.failed", "Bounded JSON parsing failed"}.withContext(
            "exception", exception.what()));
} catch (...) {
    return core::unexpected(core::Error{"json.parse.failed", "Bounded JSON parsing failed"});
}

auto parseEvents(std::string_view text, ValueParseLimits limits, ISaxEventSink& sink)
    -> core::Result<void> try {
    if (auto valid = validateText(text, limits); !valid)
        return valid;
    BoundedReader reader{limits, &sink};
    (void)nlohmann::json::sax_parse(text.begin(), text.end(), &reader);
    return reader.completion();
} catch (const std::exception& exception) {
    return core::unexpected(
        core::Error{"json.parse.failed", "JSON event parsing failed"}.withContext(
            "exception", exception.what()));
} catch (...) {
    return core::unexpected(core::Error{"json.parse.failed", "JSON event parsing failed"});
}
} // namespace cuexis::json
