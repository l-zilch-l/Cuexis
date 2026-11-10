#include <cuexis/judgement/ruleset.hpp>

#include <algorithm>
#include <set>
#include <tuple>

namespace cuexis::judgement {
namespace {
auto reject(std::string_view code, std::string_view message) -> core::Error {
    return core::Error{std::string{code}, std::string{message}}
        .withContext("category", "invalid_relation")
        .withContext("severity", "error")
        .withContext("faulted", "false");
}
} // namespace
auto validUtf8(std::string_view s) noexcept -> bool {
    std::size_t i = 0;
    while (i < s.size()) {
        const auto c = static_cast<unsigned char>(s[i++]);
        if (c < 0x80) {
            continue;
        }
        unsigned n;
        std::uint32_t v, minimum;
        if (c >= 0xc2 && c <= 0xdf) {
            n = 1;
            v = c & 31U;
            minimum = 0x80;
        } else if (c >= 0xe0 && c <= 0xef) {
            n = 2;
            v = c & 15U;
            minimum = 0x800;
        } else if (c >= 0xf0 && c <= 0xf4) {
            n = 3;
            v = c & 7U;
            minimum = 0x10000;
        } else {
            return false;
        }
        if (n > s.size() - i) {
            return false;
        }
        while (n--) {
            const auto b = static_cast<unsigned char>(s[i++]);
            if ((b & 0xc0U) != 0x80U) {
                return false;
            }
            v = (v << 6) | (b & 63U);
        }
        if (v < minimum || v > 0x10ffff || (v >= 0xd800 && v <= 0xdfff)) {
            return false;
        }
    }
    return true;
}
auto checkedScore(std::int64_t a, std::int64_t b, const ScoreConfiguration& c)
    -> core::Result<std::int64_t> {
    if (c.minimum > c.maximum || c.arithmetic > ArithmeticPolicy::clamp) {
        return core::unexpected(
            reject("ruleset.configuration_invalid", "invalid score arithmetic bounds"));
    }
    const bool high = b > 0 && a > INT64_MAX - b;
    const bool low = b < 0 && a < INT64_MIN - b;
    if (high || low) {
        if (c.arithmetic == ArithmeticPolicy::clamp) {
            return high ? c.maximum : c.minimum;
        }
        return core::unexpected(reject("ruleset.transaction_failed", "score overflow"));
    }
    const auto value = a + b;
    if (value < c.minimum || value > c.maximum) {
        if (c.arithmetic == ArithmeticPolicy::clamp) {
            return std::clamp(value, c.minimum, c.maximum);
        }
        return core::unexpected(
            reject("ruleset.transaction_failed", "score outside declared range"));
    }
    return value;
}
auto prepareRuleset(const RulesetDeclaration& d) -> core::Result<PreparedRuleset> try {
    if (d.externalPackage) {
        return core::unexpected(
            reject("ruleset.package_unsupported", "external Ruleset packages are unsupported"));
    }
    if (d.life) {
        return core::unexpected(reject("capability.disabled", "Life is disabled"));
    }
    if (d.interfaceId != "cuexis.ruleset.finite" || d.interfaceRevision != "1" ||
        d.compiledBuild != "cuexis.finite-fold.1" || d.loadoutId.empty() ||
        !validUtf8(d.loadoutId) || d.programPolicy > ProgramPolicy::open ||
        d.outcomeScope > OutcomeScope::extended ||
        (d.outcomeScope == OutcomeScope::extended && d.programPolicy != ProgramPolicy::open)) {
        return core::unexpected(
            reject("ruleset.registry_invalid", "unsupported Interface, build or policy"));
    }
    const std::vector<std::string_view> ids{"score", "combo", "statistics", "hook"};
    if (d.modules.size() != (d.hooks.empty() ? 3U : 4U)) {
        return core::unexpected(reject("ruleset.registry_invalid", "module manifest incomplete"));
    }
    for (std::size_t i = 0; i < d.modules.size(); ++i) {
        const auto& m = d.modules[i];
        if (m.id != ids[i] || m.revision != "1" || m.build != d.compiledBuild) {
            return core::unexpected(
                reject("ruleset.registry_invalid", "unknown, reordered or duplicate module"));
        }
    }
    const auto& c = d.score;
    if (c.minimum > c.maximum || c.initial < c.minimum || c.initial > c.maximum ||
        c.arithmetic > ArithmeticPolicy::clamp || c.rules.empty()) {
        return core::unexpected(
            reject("ruleset.configuration_invalid", "explicit score configuration invalid"));
    }
    for (std::size_t i = 0; i < c.rules.size(); ++i) {
        const auto& r = c.rules[i];
        if (r.phase > PhaseKind::tail || r.outcome > Outcome::miss ||
            (r.grade && (r.grade->empty() || !validUtf8(*r.grade)))) {
            return core::unexpected(
                reject("ruleset.configuration_invalid", "unknown phase, outcome or grade"));
        }
        for (std::size_t j = 0; j < i; ++j) {
            const auto& p = c.rules[j];
            if (r.phase == p.phase && r.outcome == p.outcome && r.grade == p.grade) {
                return core::unexpected(
                    reject("ruleset.configuration_invalid", "duplicate score mapping"));
            }
        }
    }
    std::set<std::string> targets;
    for (const auto& h : d.hooks) {
        if (h.stepRoute || h.consumer != HookConsumer::fold) {
            return core::unexpected(
                reject("capability.disabled", "Hook consumer or step route unsupported"));
        }
        if (h.target != "fold.bonus.u64" || h.combine > CombineOperator::bitOr ||
            h.contributors != std::vector<std::string>{"hook"} ||
            !targets.insert(h.target).second) {
            return core::unexpected(
                reject("ruleset.hook_invalid", "unknown target, operator or contributor"));
        }
    }
    auto canonical = d;
    std::sort(canonical.score.rules.begin(), canonical.score.rules.end(),
              [](const auto& a, const auto& b) {
                  return std::tuple{a.phase, a.outcome, a.grade} <
                         std::tuple{b.phase, b.outcome, b.grade};
              });
    return PreparedRuleset{std::make_shared<const RulesetDeclaration>(std::move(canonical))};
} catch (const std::exception&) {
    return core::unexpected(reject("ruleset.configuration_invalid", "prepare allocation failed"));
}
auto commitRegister(const RegisterDeclaration& d, std::span<const RegisterContribution> writes,
                    RegisterValue derived) -> core::Result<RegisterValue> try {
    if (d.kind > RegisterKind::ledgerDerived || d.valueTag > RegisterValueTag::unsigned64 ||
        d.combine > CombineOperator::bitOr) {
        return core::unexpected(
            reject("ruleset.transaction_failed", "unknown Register kind, tag or operator"));
    }
    if (d.kind == RegisterKind::ledgerDerived) {
        if (!writes.empty() || derived.index() != static_cast<std::size_t>(d.valueTag)) {
            return core::unexpected(reject("ruleset.transaction_failed",
                                           "direct ledger-derived write or invalid carrier"));
        }
        return derived;
    }
    if (d.kind == RegisterKind::exclusive && writes.size() != 1) {
        return core::unexpected(
            reject("ruleset.transaction_failed", "exclusive second or missing write"));
    }
    if (d.kind == RegisterKind::commutativeMonoid && d.valueTag != RegisterValueTag::unsigned64) {
        return core::unexpected(
            reject("ruleset.transaction_failed", "operator not closed over declared carrier"));
    }
    std::set<std::tuple<std::string, std::string, std::uint64_t, std::uint64_t, std::uint64_t>>
        keys;
    std::uint64_t combined = 0;
    for (const auto& w : writes) {
        if (w.target != d.target || w.value.index() != static_cast<std::size_t>(d.valueTag) ||
            (d.kind == RegisterKind::exclusive
                 ? w.owner != d.owner
                 : std::find(d.contributors.begin(), d.contributors.end(), w.owner) ==
                       d.contributors.end()) ||
            !keys.emplace(w.target, w.owner, w.commitId, w.factOrdinal, w.localOrdinal).second) {
            return core::unexpected(reject("ruleset.transaction_failed",
                                           "invalid owner, tag or duplicate contribution"));
        }
        if (d.kind == RegisterKind::commutativeMonoid) {
            const auto v = std::get<std::uint64_t>(w.value);
            combined = d.combine == CombineOperator::maximum ? std::max(combined, v) : combined | v;
        }
    }
    return d.kind == RegisterKind::exclusive ? writes.front().value : RegisterValue{combined};
} catch (const std::exception&) {
    return core::unexpected(
        reject("ruleset.transaction_failed", "Register candidate allocation failed"));
}
auto rulesetIdentityToken(const PreparedRuleset& r) -> std::string {
    std::string out;
    const auto text = [&](std::string_view v) {
        out += std::to_string(v.size()) + ":";
        out += v;
    };
    const auto number = [&](auto v) { text(std::to_string(v)); };
    const auto& d = r.declaration();
    text(d.interfaceId);
    text(d.interfaceRevision);
    text(d.compiledBuild);
    number(static_cast<unsigned>(d.programPolicy));
    number(static_cast<unsigned>(d.outcomeScope));
    number(d.modules.size());
    for (const auto& m : d.modules) {
        text(m.id);
        text(m.revision);
        text(m.build);
    }
    number(d.score.initial);
    number(d.score.minimum);
    number(d.score.maximum);
    number(static_cast<unsigned>(d.score.arithmetic));
    number(d.score.initialCombo);
    number(d.score.rules.size());
    for (const auto& v : d.score.rules) {
        number(static_cast<unsigned>(v.phase));
        number(static_cast<unsigned>(v.outcome));
        number(v.grade.has_value());
        if (v.grade) {
            text(*v.grade);
        }
        number(v.delta);
        number(v.incrementsCombo);
    }
    number(d.hooks.size());
    for (const auto& h : d.hooks) {
        text(h.target);
        number(static_cast<unsigned>(h.consumer));
        number(static_cast<unsigned>(h.combine));
        number(h.contributors.size());
        for (const auto& c : h.contributors) {
            text(c);
        }
        number(h.value);
        number(h.stepRoute);
    }
    return out;
}
auto PreparedRuleset::declaration() const noexcept -> const RulesetDeclaration& {
    return *declaration_;
}
auto PreparedRuleset::initialState() const -> core::Result<FoldProjection> try {
    FoldProjection result{declaration_->score.initial,
                          declaration_->score.initialCombo,
                          declaration_->score.initialCombo,
                          0,
                          0,
                          0,
                          {},
                          {},
                          0,
                          0,
                          0,
                          0,
                          {}};
    for (const auto& rule : declaration_->score.rules) {
        result.counts.push_back({rule.phase, rule.outcome, rule.grade, 0});
    }
    return result;
} catch (const std::exception&) {
    return core::unexpected(
        reject("ruleset.configuration_invalid", "initial statistics allocation failed"));
}
} // namespace cuexis::judgement
