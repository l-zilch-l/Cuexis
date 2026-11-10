// Gameplay assembler graph: declaration lookup, namespace merge and graph stabilisation.

#include "gameplay_assembler_internal.hpp"

namespace cuexis::judgement {

using namespace assembler_detail;

[[nodiscard]] auto countPatternNodes(const PatternNodeDeclaration& root) -> std::uint64_t {
    std::uint64_t count = 0;
    std::vector<const PatternNodeDeclaration*> stack;
    stack.push_back(&root);
    while (!stack.empty()) {
        const PatternNodeDeclaration* node = stack.back();
        stack.pop_back();
        ++count;
        for (const auto& operand : node->operands) {
            stack.push_back(&operand);
        }
    }
    return count;
}

[[maybe_unused, nodiscard]] auto findRequirement(const CanonicalGameplayGraph& graph,
                                                 const RequirementIdentity& identity)
    -> const RequirementRecord* {
    for (const auto& requirement : graph.requirements) {
        if (requirement.identity == identity) {
            return &requirement;
        }
    }
    return nullptr;
}

[[nodiscard]] auto findResource(const CanonicalGameplayGraph& graph, std::string_view resourceId)
    -> const ResourceRecord* {
    const auto& rows = graph.resources;
    if (rows.size() < 2)
        return !rows.empty() && rows.front().ref.resourceId == resourceId ? &rows.front() : nullptr;
    // Content validation follows stabilization; resource ID is the primary canonical key.
    const auto found = std::lower_bound(
        rows.begin(), rows.end(), resourceId,
        [](const auto& row, std::string_view wanted) { return row.ref.resourceId < wanted; });
    return found != rows.end() && found->ref.resourceId == resourceId ? &*found : nullptr;
}

[[nodiscard]] auto findDomain(const CanonicalGameplayGraph& graph, std::string_view domainId)
    -> const JudgementDomainRecord* {
    for (const auto& domain : graph.judgementDomains) {
        if (domain.domainId == domainId) {
            return &domain;
        }
    }
    return nullptr;
}

[[nodiscard]] auto findSolverProfile(const CanonicalGameplayGraph& graph, std::string_view solverId)
    -> const SolverProfileDeclaration* {
    for (const auto& profile : graph.solverProfiles) {
        if (profile.solverId == solverId) {
            return &profile;
        }
    }
    return nullptr;
}

[[nodiscard]] auto findDeclarationById(const MergedNamespace& merged, const StableDeclarationId& id)
    -> const MergedDeclaration* {
    // Merge sorts and rejects duplicate stable IDs before resolving references.
    // Stabilization retains that order for content validation and relation members.
    const auto found =
        std::lower_bound(merged.declarations.begin(), merged.declarations.end(), id,
                         [](const MergedDeclaration& declaration, const StableDeclarationId& key) {
                             return declaration.stableId < key;
                         });
    return found != merged.declarations.end() && found->stableId == id ? &*found : nullptr;
}

[[nodiscard]] auto findDeclarationByName(const MergedNamespace& merged,
                                         std::span<const std::size_t> byName, DeclarationKind kind,
                                         std::string_view name) -> const MergedDeclaration* {
    const auto found = std::lower_bound(byName.begin(), byName.end(), name,
                                        [&](std::size_t index, std::string_view key) {
                                            return merged.declarations[index].name < key;
                                        });
    if (found != byName.end()) {
        const auto& declaration = merged.declarations[*found];
        if (declaration.name == name && declaration.kind == kind)
            return &declaration;
    }
    return nullptr;
}

[[nodiscard]] auto findFactBinding(const CanonicalGameplayGraph& graph, std::string_view bindingId)
    -> const FactBindingRef* {
    for (const auto& binding : graph.factBindings) {
        if (binding.bindingId == bindingId) {
            return &binding;
        }
    }
    return nullptr;
}

//  ---------------------------------------------------------------------------------------------
//  Entry and version gates
//  ---------------------------------------------------------------------------------------------

[[nodiscard]] auto validateEntry(const AssemblyRequest& request) -> core::Result<void> {
    //  ABI domain 8: a playback session carries an explicit entry kind, and an authoring source
    //  must be marked as not being a playback entry. The rule is checked before anything else,
    //  because an entry whose kind and playback flag disagree has no decidable content semantics.
    if (request.entryKind == EntryKind::authorSource && request.playback) {
        return core::unexpected(declarationInvalidError(
            codes::kPlaybackEntryMismatchCode, codes::kEntrySection, codes::kPlaybackPath,
            "an author-source entry must declare that it is not a playback entry"));
    }
    if (request.sources.empty()) {
        return core::unexpected(declarationInvalidError(
            codes::kSourceSetEmptyCode, codes::kEntrySection, codes::kSourceDocumentsPath,
            "an assembly needs at least one typed source"));
    }
    for (std::size_t index = 0; index < request.sources.size(); ++index) {
        const GameplaySourceDocument& document = request.sources[index].document;
        if (document.sourceDocumentId.empty()) {
            return core::unexpected(
                declarationInvalidError(codes::kDeclarationIncompleteCode, codes::kEntrySection,
                                        codes::kSourceDocumentIdPath,
                                        "a source document must declare its stable identity"));
        }
        for (std::size_t other = index + 1; other < request.sources.size(); ++other) {
            if (request.sources[other].document.sourceDocumentId == document.sourceDocumentId) {
                return core::unexpected(declarationInvalidError(
                    codes::kDuplicateSourceDocumentCode, codes::kEntrySection,
                    codes::kSourceDocumentIdPath,
                    "two source documents may not share one stable identity: their declaration "
                    "ordinals would collide"));
            }
        }
        //  The outer version gate. An older chart revision is not migrated here: interpreting it as
        //  V2 is exactly what Spec 2.2 item 2 forbids.
        if (document.chartVersion != 5U) {
            return core::unexpected(versionError(codes::kChartVersionUnsupportedCode,
                                                 codes::kChartVersionPath,
                                                 "only chart revision 5 is accepted at this entry; "
                                                 "an older revision must be migrated "
                                                 "offline by a separate tool"));
        }
        //  The semantic version gate: `gameplay.version = 2` is the only V2 semantic entry.
        if (document.gameplayVersion != 2U) {
            return core::unexpected(versionError(
                codes::kGameplayVersionUnsupportedCode, codes::kGameplayVersionPath,
                "only gameplay.version 2 is accepted at this entry; an older revision must be "
                "migrated offline by a separate tool"));
        }
    }
    if (request.timebase == nullptr) {
        return core::unexpected(declarationInvalidError(
            codes::kTimebaseMissingCode, codes::kEntrySection, codes::kTimebaseRefPath,
            "the canonical graph needs a typed timebase binding; it has no default binding"));
    }
    if (request.latePolicy == nullptr) {
        return core::unexpected(declarationInvalidError(
            codes::kTimebaseMissingCode, codes::kEntrySection, codes::kLatePolicyReferencePath,
            "the canonical graph needs the late-policy parameters it was validated against"));
    }
    //  The S7A-2 gate, reused rather than re-implemented.
    const auto prepared = validatePrepare(*request.timebase, *request.latePolicy);
    if (!prepared.has_value()) {
        return core::unexpected(prepared.error());
    }
    return {};
}

//  ---------------------------------------------------------------------------------------------
//  The P1-02 merge of the declaration namespace
//  ---------------------------------------------------------------------------------------------

struct DeclarationInput final {
    StableDeclarationId stableId;
    DeclarationKind kind;
    std::string name;
    std::vector<DeclarationRef> references;
    RequiredRefs required;
    //  The stable identity of the document this declaration was physically declared in. A
    //  declaration that claims another document's identity would claim that document's ordinal
    //  range, so the two have to agree.
    std::string enclosingDocumentId;
};

[[nodiscard]] auto mergeNamespace(const AssemblyRequest& request)
    -> core::Result<MergedNamespaceBuild> {
    std::vector<DeclarationInput> inputs;
    for (const auto& source : request.sources) {
        for (const auto& declaration : source.document.declarations) {
            inputs.push_back(
                DeclarationInput{.stableId = declaration.stableId,
                                 .kind = declaration.kind,
                                 .name = declaration.localName,
                                 .references = declaration.references,
                                 .required = declaration.required,
                                 .enclosingDocumentId = source.document.sourceDocumentId});
        }
    }
    if (inputs.empty()) {
        return core::unexpected(declarationInvalidError(
            codes::kDeclarationIncompleteCode, codes::kMergeSection, codes::kDeclarationsPath,
            "an assembly needs at least one declared name; content tables cannot name themselves "
            "outside the merged namespace"));
    }

    MergedNamespace merged;
    merged.declarations.reserve(inputs.size());
    for (const auto& input : inputs) {
        if (input.stableId.sourceDocumentId.empty()) {
            return core::unexpected(declarationInvalidError(
                codes::kDeclarationIncompleteCode, codes::kMergeSection,
                codes::kSourceDocumentIdPath,
                "a declaration must carry the stable identity of the document it was declared in"));
        }
        if (input.stableId.sourceDocumentId != input.enclosingDocumentId) {
            return core::unexpected(declarationInvalidError(
                codes::kDeclarationIncompleteCode, codes::kMergeSection,
                codes::kSourceDocumentIdPath,
                "a declaration carries the stable identity of the document it is declared in; a "
                "declaration may not claim another document's ordinal range"));
        }
        if (input.name.empty()) {
            return core::unexpected(declarationInvalidError(
                codes::kDeclarationIncompleteCode, codes::kMergeSection, codes::kLocalNamePath,
                "a declaration must give itself a name; an unnamed declaration cannot be "
                "referenced"));
        }
        if (input.stableId.declarationOrdinal == 0U) {
            //  Ordinals are declarations of the source document and start at 1, so an ordinal of 0
            //  is the "never assigned" state of the field rather than a declaration index.
            return core::unexpected(
                declarationInvalidError(codes::kDeclarationOrdinalZeroCode, codes::kMergeSection,
                                        codes::kDeclarationOrdinalPath,
                                        "a declaration ordinal must be declared: 0 is the "
                                        "unassigned state of the field, not "
                                        "a declaration index"));
        }
        merged.declarations.push_back(MergedDeclaration{.stableId = input.stableId,
                                                        .kind = input.kind,
                                                        .name = input.name,
                                                        .references = {},
                                                        .requiredFeatureIds = {},
                                                        .requiredCapabilityIds = {}});
    }

    std::sort(merged.declarations.begin(), merged.declarations.end(),
              [](const MergedDeclaration& left, const MergedDeclaration& right) {
                  return canonicalCompare(left, right) < 0;
              });
    for (std::size_t index = 1; index < merged.declarations.size(); ++index) {
        if (merged.declarations[index].stableId == merged.declarations[index - 1].stableId) {
            return core::unexpected(declarationInvalidError(
                codes::kStableIdDuplicateCode, codes::kMergeSection, codes::kDeclarationOrdinalPath,
                "two declarations share one stable identity; a stable identity is unique in an "
                "assembly"));
        }
    }
    //  A duplicate name is rejected, never merged (P1-02). The comparison is over names, not over
    //  the array, so the verdict cannot depend on the order the documents arrived in.
    std::vector<std::size_t> byName;
    {
        byName.resize(merged.declarations.size());
        for (std::size_t index = 0; index < byName.size(); ++index) {
            byName[index] = index;
        }
        std::sort(byName.begin(), byName.end(), [&](std::size_t left, std::size_t right) {
            return merged.declarations[left].name < merged.declarations[right].name;
        });
        for (std::size_t index = 1; index < byName.size(); ++index) {
            const MergedDeclaration& previous = merged.declarations[byName[index - 1]];
            const MergedDeclaration& current = merged.declarations[byName[index]];
            if (previous.name == current.name) {
                return core::unexpected(
                    declarationInvalidError(codes::kDeclarationNameDuplicateCode,
                                            codes::kMergeSection, codes::kLocalNamePath,
                                            "two declarations share one name; a name identifies at "
                                            "most one declaration and "
                                            "is never merged"));
            }
        }
    }

    //  Resolve every reference against the whole set, so a forward reference and a backward one are
    //  the same thing.
    for (const auto& input : inputs) {
        std::vector<StableDeclarationId> resolved;
        resolved.reserve(input.references.size());
        for (const auto& reference : input.references) {
            std::string documentId = input.stableId.sourceDocumentId;
            if (reference.scope == ReferenceScope::explicitCrossDocument) {
                if (reference.sourceDocumentId.empty()) {
                    return core::unexpected(
                        declarationInvalidError(codes::kCrossDocumentReferenceImplicitCode,
                                                codes::kMergeSection, codes::kReferencesPath,
                                                "a cross-document reference must name the document "
                                                "it refers to; it is never "
                                                "resolved by looking the name up elsewhere"));
                }
                documentId = reference.sourceDocumentId;
            } else if (!reference.sourceDocumentId.empty() &&
                       reference.sourceDocumentId != input.stableId.sourceDocumentId) {
                return core::unexpected(declarationInvalidError(
                    codes::kDeclarationIncompleteCode, codes::kMergeSection, codes::kReferencesPath,
                    "a same-document reference must not name another document; the scope it "
                    "declares and the document it names disagree"));
            }
            const StableDeclarationId wanted{.sourceDocumentId = documentId,
                                             .declarationOrdinal = reference.declarationOrdinal};
            if (findDeclarationById(merged, wanted) == nullptr) {
                return core::unexpected(declarationInvalidError(
                    codes::kReferenceDanglingCode, codes::kMergeSection, codes::kReferencesPath,
                    "a declaration references a declaration that is not in the merged namespace"));
            }
            resolved.push_back(wanted);
        }
        sortUnique(resolved);
        // Every input was inserted before sorting and duplicate rejection above.
        // Only non-key fields change while references are resolved.
        const auto found = std::lower_bound(
            merged.declarations.begin(), merged.declarations.end(), input.stableId,
            [](const MergedDeclaration& declaration, const StableDeclarationId& key) {
                return declaration.stableId < key;
            });
        auto& declaration = *found;
        declaration.references = std::move(resolved);
        declaration.requiredFeatureIds = {};
        for (const auto& feature : input.required.features) {
            declaration.requiredFeatureIds.push_back(feature.featureId);
        }
        declaration.requiredCapabilityIds = {};
        for (const auto& capability : input.required.capabilities) {
            declaration.requiredCapabilityIds.push_back(capability.capabilityId);
        }
        sortUnique(declaration.requiredFeatureIds);
        sortUnique(declaration.requiredCapabilityIds);
    }
    return MergedNamespaceBuild{std::move(merged), std::move(byName)};
}

//  ---------------------------------------------------------------------------------------------
//  Graph construction and stabilization
//  ---------------------------------------------------------------------------------------------

void stabilizeGraph(CanonicalGameplayGraph& graph) {
    for (auto& requirement : graph.requirements) {
        std::sort(requirement.atomBindings.begin(), requirement.atomBindings.end(),
                  [](const auto& left, const auto& right) {
                      return std::lexicographical_compare(
                          left.atomRef.begin(), left.atomRef.end(), right.atomRef.begin(),
                          right.atomRef.end(),
                          [](unsigned char a, unsigned char b) { return a < b; });
                  });
        std::sort(requirement.patternArmRefs.begin(), requirement.patternArmRefs.end());
        if (requirement.timing.has_value()) {
            std::sort(requirement.timing->phaseTargets.begin(),
                      requirement.timing->phaseTargets.end(),
                      [](const auto& left, const auto& right) { return left.phase < right.phase; });
            std::sort(requirement.timing->successWindows.begin(),
                      requirement.timing->successWindows.end(),
                      [](const auto& left, const auto& right) {
                          return std::tuple{left.phase, left.start, left.end} <
                                 std::tuple{right.phase, right.start, right.end};
                      });
        }
        std::sort(requirement.phases.begin(), requirement.phases.end(),
                  [](const PhaseDeclaration& left, const PhaseDeclaration& right) {
                      if (left.kind != right.kind) {
                          return static_cast<unsigned>(left.kind) <
                                 static_cast<unsigned>(right.kind);
                      }
                      return left.declarationOrdinal < right.declarationOrdinal;
                  });
        std::sort(
            requirement.measure.components.begin(), requirement.measure.components.end(),
            [](const MeasureComponentDeclaration& left, const MeasureComponentDeclaration& right) {
                if (left.phase != right.phase) {
                    return static_cast<unsigned>(left.phase) < static_cast<unsigned>(right.phase);
                }
                return left.categoryToken < right.categoryToken;
            });
        std::sort(requirement.resourceClaims.begin(), requirement.resourceClaims.end(),
                  [](const ResourceClaimDeclaration& left, const ResourceClaimDeclaration& right) {
                      return left.resourceRef.resourceId < right.resourceRef.resourceId;
                  });
        std::sort(requirement.requiredActions.begin(), requirement.requiredActions.end());
        //  Sorted and deliberately not deduplicated: a declared list that names the same thing
        //  twice is a duplicate declaration the validation below reports, not an input the merge
        //  fixes.
        std::sort(requirement.factBindingRefs.begin(), requirement.factBindingRefs.end());
        sortUnique(requirement.required.features);
        sortUnique(requirement.required.capabilities);
        sortUnique(requirement.pattern.required.features);
        sortUnique(requirement.pattern.required.capabilities);
        sortUnique(requirement.measure.required.features);
        sortUnique(requirement.measure.required.capabilities);
        std::sort(requirement.unsupportedForms.begin(), requirement.unsupportedForms.end());
    }
    std::sort(graph.requirements.begin(), graph.requirements.end(),
              [](const RequirementRecord& left, const RequirementRecord& right) {
                  return canonicalCompare(left, right) < 0;
              });

    for (auto& resource : graph.resources) {
        sortUnique(resource.required.features);
        sortUnique(resource.required.capabilities);
        std::sort(resource.unsupportedForms.begin(), resource.unsupportedForms.end());
    }
    std::sort(graph.resources.begin(), graph.resources.end(),
              [](const ResourceRecord& left, const ResourceRecord& right) {
                  return canonicalCompare(left, right) < 0;
              });

    for (auto& relation : graph.relations) {
        std::sort(relation.members.begin(), relation.members.end());
        sortUnique(relation.required.features);
        sortUnique(relation.required.capabilities);
    }
    std::sort(graph.relations.begin(), graph.relations.end(),
              [](const RelationDeclaration& left, const RelationDeclaration& right) {
                  return canonicalCompare(left, right) < 0;
              });

    for (auto& profile : graph.solverProfiles) {
        sortUnique(profile.required.features);
        sortUnique(profile.required.capabilities);
    }
    std::sort(graph.solverProfiles.begin(), graph.solverProfiles.end(),
              [](const SolverProfileDeclaration& left, const SolverProfileDeclaration& right) {
                  return canonicalCompare(left, right) < 0;
              });

    sortUnique(graph.factBindings);

    for (auto& domain : graph.judgementDomains) {
        std::sort(domain.axes.begin(), domain.axes.end(),
                  [](const JudgementAxisRange& left, const JudgementAxisRange& right) {
                      return left.axisToken < right.axisToken;
                  });
        sortUnique(domain.required.features);
        sortUnique(domain.required.capabilities);
    }
    std::sort(graph.judgementDomains.begin(), graph.judgementDomains.end(),
              [](const JudgementDomainRecord& left, const JudgementDomainRecord& right) {
                  return canonicalCompare(left, right) < 0;
              });

    sortUnique(graph.declaredCapabilities.capabilities);
    sortUnique(graph.declaredFeatures.features);
    sortUnique(graph.closureContributions.features);
    sortUnique(graph.closureContributions.capabilities);
    sortUnique(graph.sourceClosure.sourceDocumentIds);
    std::sort(graph.diagnosticMap.entries.begin(), graph.diagnosticMap.entries.end(),
              [](const DiagnosticMap::Entry& left, const DiagnosticMap::Entry& right) {
                  if (left.declarationId != right.declarationId) {
                      return left.declarationId < right.declarationId;
                  }
                  return left.fieldPath < right.fieldPath;
              });
}

//  ---------------------------------------------------------------------------------------------
//  Declaration validation
//  ---------------------------------------------------------------------------------------------

//  The declared shape of one Pattern tree: the number of primitive declarations and the deepest
//  declaration nesting. Both are measurements of the content and never limits on it.

//  Validates one Pattern declaration tree and measures its shape.
//
//  The walk is iterative on purpose. A recursive walker needs a compiled-in depth bound to keep the
//  host stack safe, and such a constant is exactly the hidden content limit this batch refuses to
//  freeze: it would stably refuse legal content that no measurement ever bounded. The depth is a
//  budgeted, measurable dimension instead (see `PatternCompileBudget`), and the traversal stack of
//  this walker lives on the heap, so a deeply nested declaration is measured rather than refused.
[[nodiscard]] auto measurePatternTree(const PatternNodeDeclaration& root)
    -> core::Result<PatternDeclarationShape> {
    struct Frame final {
        const PatternNodeDeclaration* node;
        std::uint64_t depth;
    };
    PatternDeclarationShape shape;
    std::vector<Frame> stack;
    stack.push_back(Frame{&root, 1U});
    while (!stack.empty()) {
        const Frame frame = stack.back();
        stack.pop_back();
        const PatternNodeDeclaration& node = *frame.node;
        ++shape.nodeCount;
        shape.declarationDepth = std::max(shape.declarationDepth, frame.depth);
        if (node.unsupportedForm.has_value()) {
            const UnsupportedMapping mapping = mapUnsupported(node.unsupportedForm->kind);
            return core::unexpected(
                capabilityRejection(mapping.code, mapping.category, codes::kGraphSection,
                                    std::string{codes::kPatternRootPath} + "." +
                                        std::string{codes::kUnsupportedFormPath},
                                    "this pattern node declares a content form Stage 7A rejects",
                                    mapping.capabilityId, mapping.remediation));
        }
        if (node.repeatBounds.has_value() && node.primitive != PatternPrimitive::boundedRepeat) {
            return core::unexpected(declarationInvalidError(
                codes::kDeclarationIncompleteCode, codes::kGraphSection, codes::kRepeatBoundsPath,
                "only a bounded repeat declares repeat bounds"));
        }
        if (node.primitive == PatternPrimitive::atom) {
            if (node.atomRef.empty()) {
                return core::unexpected(declarationInvalidError(
                    codes::kDeclarationIncompleteCode, codes::kGraphSection, codes::kAtomRefPath,
                    "an atom must declare the reference it matches"));
            }
            if (!node.operands.empty()) {
                return core::unexpected(
                    declarationInvalidError(codes::kDeclarationIncompleteCode, codes::kGraphSection,
                                            codes::kPatternRootPath, "an atom takes no operand"));
            }
        } else if (!node.atomRef.empty()) {
            return core::unexpected(declarationInvalidError(
                codes::kDeclarationIncompleteCode, codes::kGraphSection, codes::kAtomRefPath,
                "only an atom declares the reference it matches"));
        }
        if (node.primitive == PatternPrimitive::boundedRepeat) {
            if (!node.repeatBounds.has_value()) {
                return core::unexpected(declarationInvalidError(
                    codes::kDeclarationIncompleteCode, codes::kGraphSection,
                    codes::kRepeatBoundsPath,
                    "a bounded repeat must declare its bounds; an undeclared bound would be an "
                    "implicit "
                    "unbounded repetition"));
            }
            if (node.repeatBounds->minimum > node.repeatBounds->maximum) {
                return core::unexpected(valueError(
                    codes::kDomainRangeReversedCode, codes::kGraphSection, codes::kRepeatBoundsPath,
                    "a declared repeat range is reversed: its maximum "
                    "precedes its minimum"));
            }
            if (node.operands.size() != 1U) {
                return core::unexpected(declarationInvalidError(
                    codes::kDeclarationIncompleteCode, codes::kGraphSection,
                    codes::kPatternRootPath, "a bounded repeat takes exactly one operand"));
            }
        }
        if ((node.primitive == PatternPrimitive::sequence ||
             node.primitive == PatternPrimitive::choice ||
             node.primitive == PatternPrimitive::complement) &&
            node.operands.empty()) {
            return core::unexpected(declarationInvalidError(
                codes::kDeclarationIncompleteCode, codes::kGraphSection, codes::kPatternRootPath,
                "a composed pattern primitive must declare at least one operand"));
        }
        if ((node.primitive == PatternPrimitive::skip ||
             node.primitive == PatternPrimitive::instant) &&
            !node.operands.empty()) {
            return core::unexpected(declarationInvalidError(
                codes::kDeclarationIncompleteCode, codes::kGraphSection, codes::kPatternRootPath,
                "this pattern primitive takes no operand"));
        }
        //  Pushed in reverse so that the stack pops the operands in their declared order, which is
        //  the order the earlier recursive walker reported a fault in.
        for (auto operand = node.operands.rbegin(); operand != node.operands.rend(); ++operand) {
            stack.push_back(Frame{&(*operand), frame.depth + 1U});
        }
    }
    return shape;
}
} // namespace cuexis::judgement
