#include <algorithm>
#include <cuexis/core/error.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis/playback/gameplay_candidate.hpp>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace cuexis::playback {
namespace {
using json::Value;
struct ShapeError : std::runtime_error {
    using std::runtime_error::runtime_error;
};
auto error(std::string message, bool budget = false) -> core::Error {
    return core::Error{budget ? "capability.budget_insufficient" : "playback.gameplay.invalid",
                       std::move(message)}
        .withContext("category", budget ? "budget_exceeded" : "invalid_relation")
        .withContext("severity", "error")
        .withContext("faulted", "false");
}
template <class T> void transfer(Value&, T&, bool);
struct Fields {
    Value& value;
    bool reading;
    std::size_t count{};
    template <class T> void field(const char* name, T& target) {
        ++count;
        if (reading) {
            auto* member = value.find(name);
            if (!member)
                throw ShapeError{std::string{"Missing configuration field: "} + name};
            transfer(*member, target, true);
        } else {
            Value member;
            transfer(member, target, false);
            value.object()->emplace(name, std::move(member));
        }
    }
    void finish() const {
        if (reading && value.object()->size() != count)
            throw ShapeError{"Unknown configuration field"};
    }
};
#define FIELD(name) f.field(#name, x.name)
void fields(Fields& f, GameplayRational& x) {
    FIELD(numerator);
    FIELD(denominator);
}
void fields(Fields& f, GameplayInputDomain& x) {
    FIELD(id);
    FIELD(scale);
    FIELD(minimum);
    FIELD(maximum);
    FIELD(inclusive);
}
void fields(Fields& f, GameplayModule& x) {
    FIELD(id);
    FIELD(revision);
    FIELD(build);
}
void fields(Fields& f, GameplayHook& x) {
    FIELD(target);
    FIELD(consumer);
    FIELD(combine);
    FIELD(contributors);
    FIELD(value);
    FIELD(stepRoute);
}
void fields(Fields& f, GameplayScoreRule& x) {
    FIELD(phase);
    FIELD(outcome);
    FIELD(grade);
    FIELD(delta);
    FIELD(incrementsCombo);
}
void fields(Fields& f, GameplayRequirementRef::PathStep& x) {
    FIELD(nodeId);
    FIELD(repeatIndex);
}
void fields(Fields& f, GameplayRequirementRef& x) {
    FIELD(chartEntryId);
    FIELD(invocationId);
    FIELD(moduleId);
    FIELD(exportId);
    FIELD(requirementLocalId);
    FIELD(emissionPath);
}
void fields(Fields& f, GameplayFactBinding& x) {
    FIELD(bindingId);
    FIELD(source);
    FIELD(phase);
    FIELD(outcome);
    FIELD(timing);
    FIELD(target);
    FIELD(visible);
    FIELD(start);
    FIELD(end);
    FIELD(aggregation);
    FIELD(groupMembers);
}
void fields(Fields& f, GameplayConfiguration& x) {
    FIELD(engine);
    FIELD(rulesetInterfaceProjection);
    FIELD(rulesetBuild);
    FIELD(rulesetModuleOrder);
    FIELD(session);
    FIELD(enabledCapabilities);
    FIELD(mappingId);
    FIELD(mappingVersion);
    FIELD(sourceClass);
    FIELD(calibration);
    FIELD(domains);
    FIELD(loadout);
    FIELD(actualRulesetInterface);
    FIELD(actualRulesetRevision);
    FIELD(actualRulesetBuild);
    FIELD(modules);
    FIELD(programPolicy);
    FIELD(outcomeScope);
    FIELD(arithmetic);
    FIELD(hooks);
    FIELD(externalPackage);
    FIELD(life);
    FIELD(initialScore);
    FIELD(minimumScore);
    FIELD(maximumScore);
    FIELD(initialCombo);
    FIELD(scoreRules);
    FIELD(bindings);
    FIELD(testOnly);
}
#undef FIELD
template <class T> auto enumNames() {
    if constexpr (std::is_same_v<T, GameplayPhase>)
        return std::array{"tap", "head", "body", "tail"};
    else if constexpr (std::is_same_v<T, GameplayOutcome>)
        return std::array{"hit", "miss"};
    else if constexpr (std::is_same_v<T, GameplayTimingClass>)
        return std::array{"any", "early", "exact", "late"};
    else if constexpr (std::is_same_v<T, GameplayAggregation>)
        return std::array{"any", "all", "groupCommit"};
    else if constexpr (std::is_same_v<T, GameplayProgramPolicy>)
        return std::array{"locked", "extendable", "open"};
    else if constexpr (std::is_same_v<T, GameplayOutcomeScope>)
        return std::array{"declared", "extended"};
    else if constexpr (std::is_same_v<T, GameplayArithmetic>)
        return std::array{"checked", "clamp"};
    else if constexpr (std::is_same_v<T, GameplayHookConsumer>)
        return std::array{"fold", "kernel", "shared"};
    else if constexpr (std::is_same_v<T, GameplayCombine>)
        return std::array{"maximum", "bit-or"};
}
template <class T> struct Optional : std::false_type {};
template <class T> struct Optional<std::optional<T>> : std::true_type {};
template <class T> struct Vector : std::false_type {};
template <class T> struct Vector<std::vector<T>> : std::true_type {};
template <class T> struct Array : std::false_type {};
template <class T, std::size_t N> struct Array<std::array<T, N>> : std::true_type {};
template <class T> void transfer(Value& v, T& x, bool read) {
    if constexpr (std::is_same_v<T, std::string>) {
        if (!read)
            v = Value{x};
        else if (auto p = v.string())
            x = *p;
        else
            throw ShapeError{"Expected configuration string"};
    } else if constexpr (std::is_same_v<T, bool>) {
        if (!read)
            v = Value{x};
        else if (auto p = v.boolean())
            x = *p;
        else
            throw ShapeError{"Expected configuration boolean"};
    } else if constexpr (std::is_same_v<T, std::int64_t>) {
        if (!read)
            v = Value{x};
        else if (auto p = v.signedInteger())
            x = *p;
        else if (auto unsignedValue = v.unsignedInteger();
                 unsignedValue && *unsignedValue <= static_cast<std::uint64_t>(INT64_MAX))
            x = static_cast<std::int64_t>(*unsignedValue);
        else
            throw ShapeError{"Expected configuration i64"};
    } else if constexpr (std::is_same_v<T, std::uint64_t>) {
        if (!read)
            v = Value{x};
        else if (auto p = v.unsignedInteger())
            x = *p;
        else if (auto signedValue = v.signedInteger(); signedValue && *signedValue >= 0)
            x = static_cast<std::uint64_t>(*signedValue);
        else
            throw ShapeError{"Expected configuration u64"};
    } else if constexpr (std::is_enum_v<T>) {
        const auto names = enumNames<T>();
        if (read) {
            auto p = v.string();
            if (p)
                for (std::size_t i = 0; i < names.size(); ++i)
                    if (*p == names[i]) {
                        x = static_cast<T>(i);
                        return;
                    }
            throw ShapeError{"Unknown configuration enum"};
        }
        const auto index = static_cast<std::size_t>(x);
        if (index >= names.size())
            throw ShapeError{"Unknown configuration enum"};
        v = Value{names[index]};
    } else if constexpr (std::is_same_v<T, GameplayPresentationTick>) {
        transfer(v, x.value, read);
    } else if constexpr (Optional<T>::value) {
        if (read) {
            if (v.isNull())
                x.reset();
            else {
                typename T::value_type item{};
                transfer(v, item, true);
                x = std::move(item);
            }
        } else if (x)
            transfer(v, *x, false);
        else
            v = Value{};
    } else if constexpr (Vector<T>::value || Array<T>::value) {
        if (read) {
            auto* items = v.array();
            if (!items)
                throw ShapeError{"Expected configuration array"};
            if constexpr (Vector<T>::value) {
                x.clear();
                for (auto& item : *items) {
                    typename T::value_type target{};
                    transfer(item, target, true);
                    x.push_back(std::move(target));
                }
            } else {
                if (items->size() != x.size())
                    throw ShapeError{"Wrong configuration identity component count"};
                for (std::size_t i = 0; i < x.size(); ++i) {
                    transfer((*items)[i], x[i], true);
                }
            }
        } else {
            Value::Array items;
            for (auto& item : x) {
                Value target;
                transfer(target, item, false);
                items.push_back(std::move(target));
            }
            v = Value{std::move(items)};
        }
    } else {
        if (read && !v.object())
            throw ShapeError{"Expected configuration object"};
        if (!read)
            v = Value{Value::Object{}};
        Fields f{v, read};
        fields(f, x);
        f.finish();
    }
}
} // namespace

auto encodeGameplayConfiguration(const GameplayConfiguration& input)
    -> core::Result<std::string> try {
    if (!input.testOnly)
        return core::unexpected(error("Configuration codec requires explicit testOnly"));
    auto config = input;
    for (auto& binding : config.bindings)
        std::sort(binding.groupMembers.begin(), binding.groupMembers.end());
    Value encoded;
    transfer(encoded, config, false);
    return json::serialize(Value{Value::Object{{"format", Value{"cuexis.gameplay-configuration"}},
                                               {"version", Value{std::uint64_t{1}}},
                                               {"configuration", std::move(encoded)}}});
} catch (const std::exception& e) {
    return core::unexpected(error(e.what()));
} catch (...) {
    return core::unexpected(error("Configuration encoding failed"));
}

auto decodeGameplayConfiguration(std::string_view text, GameplayConfigurationDecodeBudget b)
    -> core::Result<GameplayConfiguration> try {
    if (!b.testOnly || !b.maxBytes || !b.maxDepth || !b.maxStringBytes || !b.maxValues ||
        !b.maxContainerElements)
        return core::unexpected(
            error("Configuration requires explicit positive test-only decode budgets", true));
    auto parsed = json::parseBounded(
        text, {{b.maxBytes, b.maxDepth, b.maxStringBytes}, b.maxValues, b.maxContainerElements});
    if (!parsed) {
        auto projected = error(std::string{parsed.error().message()},
                               parsed.error().code() == "json.parse.size_limit" ||
                                   parsed.error().code() == "json.parse.depth_limit" ||
                                   parsed.error().code() == "json.parse.string_limit" ||
                                   parsed.error().code() == "json.parse.value_limit" ||
                                   parsed.error().code() == "json.parse.element_limit");
        projected.withContext("sourceCode", std::string{parsed.error().code()});
        return core::unexpected(std::move(projected));
    }
    std::string format;
    std::uint64_t version{};
    GameplayConfiguration config{};
    Fields root{*parsed, true};
    if (!parsed->object())
        throw ShapeError{"Expected configuration envelope"};
    root.field("format", format);
    root.field("version", version);
    if (format != "cuexis.gameplay-configuration" || version != 1)
        throw ShapeError{"Unsupported configuration envelope"};
    root.field("configuration", config);
    root.finish();
    if (!config.testOnly)
        throw ShapeError{"Configuration must explicitly declare testOnly"};
    return config;
} catch (const std::exception& e) {
    return core::unexpected(error(e.what()));
} catch (...) {
    return core::unexpected(error("Configuration decoding failed"));
}
} // namespace cuexis::playback
