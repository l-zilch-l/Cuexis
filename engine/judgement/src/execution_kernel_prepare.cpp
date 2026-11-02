#include "execution_kernel_internal.hpp"

namespace cuexis::judgement::detail {

using namespace kernel_detail;

auto ExecutionKernel::prepare(const SessionConfiguration& c, const PreparedGameplay& prepared)
    -> core::Result<std::unique_ptr<ExecutionKernel>> try {
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpaObservation::SeekWorkTimer timer{SeekPhase::s2CandidatePrep, &SeekWork::kernelPrepareUs};
    rpaObservation::seekAdd(SeekPhase::s2CandidatePrep, &SeekWork::kernelPrepareCalls);
#endif
    if (auto valid = validateSessionConfiguration(c); !valid) {
        return core::unexpected(valid.error());
    }
    const auto& g = prepared.assembled().graph;
    if (g.executionProfile != c.executionProfileToken ||
        prepared.identityDeclarations() != c.identityDeclarations ||
        !sameTimebase(c.timebase.view(), *g.timebase) || c.latePolicy != *g.latePolicy) {
        return core::unexpected(
            error("execution.relation_invalid", "configuration and prepared declarations differ"));
    }
    if (c.ruleset) {
        auto registry = prepareRuleset(c.ruleset->declaration());
        if (!registry) {
            return core::unexpected(registry.error());
        }
    }
    const auto mapping = c.inputMapping.view();
    if (c.ruleset) {
        const auto timebase = c.timebase.view();
        const auto& engine = c.identityDeclarations.engine;
        const auto& session = c.identityDeclarations.session;
        for (const auto token :
             {mapping.profileId, mapping.profileVersion, mapping.sourceClass.token(),
              timebase.profileId, timebase.unitToken, std::string_view{c.calibrationIdentityToken},
              std::string_view{engine.judgementSemanticRevision},
              std::string_view{engine.factSemanticRevision},
              std::string_view{engine.fixedPointTableId},
              std::string_view{engine.coordinationPhaseOrderToken},
              std::string_view{session.defaultGraceSourceToken},
              std::string_view{session.normalizationProfileToken},
              std::string_view{session.judgementConfigToken}}) {
            if (!validUtf8(token)) {
                return core::unexpected(
                    error("execution.relation_invalid", "invalid UTF-8 profile metadata"));
            }
        }
        for (const auto& domain : mapping.domains) {
            if (!validUtf8(domain.domainToken)) {
                return core::unexpected(
                    error("execution.relation_invalid", "invalid UTF-8 mapping domain"));
            }
        }
    }
    auto actual = c.identityDeclarations;
    if (c.ruleset) {
        const auto token = rulesetIdentityToken(*c.ruleset);
        actual.engine.judgementSemanticRevision += ";gameplay.fold.finite.v1";

        actual.ruleset.interfaceProjectionToken = token;
        actual.ruleset.buildHash = c.ruleset->declaration().compiledBuild;
        actual.ruleset.moduleOrder.clear();
        for (const auto& m : c.ruleset->declaration().modules) {
            actual.ruleset.moduleOrder.push_back(m.id);
        }
        actual.session.loadoutToken = c.ruleset->declaration().loadoutId;
    }
    const bool hasGradeTables =
        std::any_of(g.requirements.begin(), g.requirements.end(), [](const auto& r) {
            return std::any_of(r.measure.components.begin(), r.measure.components.end(),
                               [](const auto& c) { return c.gradeTable.has_value(); });
        });
    if (hasGradeTables) {
        actual.engine.factSemanticRevision += ";grade.signed-interval.v1";
    }
    SharedSequence<MatcherState> matchers;
    KernelProjection projection{
        .phases = {},
        .resources = {},
        .contacts = {},
        .ownership = {},
        .observers = {},
        .receipts = {},
        .facts = {},
        .processedFrontier = {},
        .lastAdvanceHorizon = {},
        .state = KernelSessionState::Prepared,
        .faultDiagnostic = {},
        .judgementIdentity = IdentityFactory::runtime(prepared, actual, mapping, c.latePolicy,
                                                      c.calibrationIdentityToken),
        .runScope = {},
        .failedTick = {},
        .requestedHorizon = {},
        .kernelWorkTick = {},
        .sealedFactCursor = 0,
        .fold = {},
        .effectiveFactSemanticRevision = {},
        .faultStage = {}};
    projection.effectiveFactSemanticRevision = actual.engine.factSemanticRevision;
    if (c.executionProfileToken == "gameplay.execution.t4-k4.rpa.v1") {
        projection.rpa.emplace();
    }
    projection.runScope = std::make_shared<const std::uint8_t>(0);
    if (c.ruleset) {
        auto initial = c.ruleset->initialState();
        if (!initial) {
            return core::unexpected(initial.error());
        }
        projection.fold = std::move(*initial);
    }

    for (std::size_t i = 0; i < g.requirements.size(); ++i) {
        const auto& r = g.requirements[i];
        if (!prepared.requirements()[i].executionProgram) {
            return core::unexpected(
                error("execution.profile_incomplete", "prepared executable program missing"));
        }
        if (c.ruleset) {
            const auto& id = r.identity;
            if (!validUtf8(id.chartEntryId) || !validUtf8(id.invocationId) ||
                !validUtf8(id.moduleId) || !validUtf8(id.exportId) ||
                !validUtf8(id.requirementLocalId)) {
                return core::unexpected(
                    error("execution.relation_invalid", "invalid UTF-8 RequirementIdentity"));
            }
            for (const auto& step : id.emissionPath) {
                if (!validUtf8(step.nodeId)) {
                    return core::unexpected(
                        error("execution.relation_invalid", "invalid UTF-8 emission identity"));
                }
            }
            for (const auto& component : r.measure.components) {
                for (const auto& token : component.declaredGradeTokens) {
                    if (!validUtf8(token)) {
                        return core::unexpected(
                            error("execution.relation_invalid", "invalid UTF-8 grade token"));
                    }
                }
            }
        }
        for (const auto& b : r.atomBindings) {
            if (c.ruleset && (!validUtf8(b.domainToken) || !validUtf8(b.sourceClass) ||
                              !validUtf8(b.channelToken))) {
                return core::unexpected(
                    error("execution.relation_invalid", "invalid UTF-8 binding"));
            }
            auto domain =
                std::find_if(mapping.domains.begin(), mapping.domains.end(),
                             [&](const auto& d) { return d.domainToken == b.domainToken; });
            if (domain == mapping.domains.end() || b.sourceClass != mapping.sourceClass.token() ||
                (b.amountRange &&
                 (b.amountRange->minimum < domain->amount.minimum ||
                  b.amountRange->maximum > domain->amount.maximum ||
                  (domain->amount.boundaryPolicy == AmountBoundaryPolicy::exclusive &&
                   (b.amountRange->minimum == domain->amount.minimum ||
                    b.amountRange->maximum == domain->amount.maximum))))) {
                return core::unexpected(error("execution.relation_invalid",
                                              "atom binding is incompatible with mapping"));
            }
        }
        for (const auto& component : r.measure.components) {
            if (component.gradeTable) {
                const auto& table = *component.gradeTable;
                if (table.empty()) {
                    return core::unexpected(
                        error("execution.relation_invalid", "empty declared grade table"));
                }
                for (std::size_t n = 0; n < table.size(); ++n) {
                    const auto& row = table[n];
                    if (row.minimum > row.maximum || row.grade.empty() || !validUtf8(row.grade) ||
                        std::find(component.declaredGradeTokens.begin(),
                                  component.declaredGradeTokens.end(),
                                  row.grade) == component.declaredGradeTokens.end() ||
                        (n && (table[n - 1].maximum == INT64_MAX ||
                               row.minimum != table[n - 1].maximum + 1))) {
                        return core::unexpected(
                            error("execution.relation_invalid",
                                  "grade table gap, overlap or undeclared grade"));
                    }
                }
                const auto target =
                    std::find_if(r.timing->phaseTargets.begin(), r.timing->phaseTargets.end(),
                                 [&](const auto& t) { return t.phase == component.phase; });
                std::vector<std::pair<Tick, Tick>> domains;
                if (component.phase == PhaseKind::body) {
                    std::optional<Tick> first;
                    for (const auto& w : r.timing->successWindows) {
                        if (w.phase.kind == PhaseKind::head && (!first || w.start < *first)) {
                            first = w.start;
                        }
                    }
                    if (first && r.timing->body) {
                        auto low = offsetTicks(*first, TickSpan{1});
                        if (low && *low < r.timing->body->end) {
                            domains.emplace_back(*low, Tick{r.timing->body->end.value() - 1});
                        }
                    }
                } else if (component.phase == PhaseKind::tail) {
                    const auto deadline = prepared.requirements()[i].deadline;
                    if (r.timing->body->end < deadline) {
                        domains.emplace_back(r.timing->body->end, Tick{deadline.value() - 1});
                    }
                } else {
                    for (const auto& w : r.timing->successWindows) {
                        if (w.phase.kind == component.phase) {
                            domains.emplace_back(w.start, Tick{w.end.value() - 1});
                        }
                    }
                }
                const auto minimum =
                    INT64_MIN + c.latePolicy.windowOpenThreshold.measuredValue()->value();
                const auto maximum =
                    INT64_MAX - c.latePolicy.finalizationWatermark.measuredValue()->value();
                for (auto [lower, upper] : domains) {
                    lower = std::max(lower, Tick{minimum});
                    upper = std::min(upper, Tick{maximum});
                    if (lower > upper) {
                        continue;
                    }
                    auto low = differenceTicks(lower, target->chartTick);
                    auto high = differenceTicks(upper, target->chartTick);
                    if (!low || !high || table.front().minimum > low->value() ||
                        table.back().maximum < high->value()) {
                        return core::unexpected(
                            error("execution.relation_invalid",
                                  "grade table does not cover reachable phase errors"));
                    }
                }
            }
            if (c.ruleset) {
                std::vector<std::optional<std::string>> grades{std::nullopt};
                if (component.gradeTable) {
                    for (const auto& row : *component.gradeTable) {
                        grades.push_back(row.grade);
                    }
                }
                for (const auto outcome : kStage7AOutcomes) {
                    for (const auto& grade : grades) {
                        if (grade &&
                            ((component.phase == PhaseKind::body && outcome == Outcome::hit) ||
                             (component.phase != PhaseKind::body &&
                              component.phase != PhaseKind::tail && outcome == Outcome::miss))) {
                            continue;
                        }
                        const auto& rules = c.ruleset->declaration().score.rules;
                        if (std::none_of(rules.begin(), rules.end(), [&](const auto& rule) {
                                return rule.phase == component.phase && rule.outcome == outcome &&
                                       rule.grade == grade;
                            })) {
                            return core::unexpected(
                                error("execution.relation_invalid",
                                      "required scoring configuration missing"));
                        }
                    }
                }
            }
        }
        for (const auto& p : r.phases) {
            if (c.ruleset) {
                const auto& rules = c.ruleset->declaration().score.rules;
                for (const auto outcome : kStage7AOutcomes) {
                    if (std::none_of(rules.begin(), rules.end(), [&](const auto& rule) {
                            return rule.phase == p.kind && rule.outcome == outcome && !rule.grade;
                        })) {
                        return core::unexpected(error("execution.relation_invalid",
                                                      "phase scoring configuration incomplete"));
                    }
                }
            }
            projection.phases.push_back({r.identity, p.kind, KernelPhaseState::Dormant});
        }
        matchers.push_back({{prepared.requirements()[i].executionProgram->start}, false});
    }
    for (const auto& r : g.resources) {
        if (c.ruleset && (!validUtf8(r.ref.resourceId) || !validUtf8(r.slotToken))) {
            return core::unexpected(
                error("execution.relation_invalid", "invalid UTF-8 resource identity"));
        }
        projection.resources.push_back({r.ref.resourceId, Free{}});
    }
    auto state = std::make_shared<const RuntimeState>(std::move(projection), std::move(matchers));
    auto impl = std::make_unique<Impl>(std::move(c), std::move(prepared), std::move(state));
#if defined(CUEXIS_RPA_OBSERVATION_TEST_ONLY)
    rpaObservation::seekAdd(SeekPhase::s2CandidatePrep, &SeekWork::kernelPrepareSuccesses);
#endif
    return std::unique_ptr<ExecutionKernel>{new ExecutionKernel{std::move(impl)}};
} catch (const std::exception&) {
    return core::unexpected(
        error("execution.relation_invalid", "kernel preparation allocation failed"));
}

} // namespace cuexis::judgement::detail
