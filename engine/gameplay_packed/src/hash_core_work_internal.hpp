// Included inside the Capsule implementation. This owns numeric costs and workspace only.
// All semantic fields remain in the existing visitors.
struct Output;
enum class CostGroup : std::size_t {
    emission,
    pattern,
    measure,
    domain,
    merged,
    resource,
    relation,
    solver,
    binding,
    claim,
    entity
};
template <typename T> constexpr auto costGroup() -> CostGroup {
    using V = std::remove_cv_t<T>;
    if constexpr (std::is_same_v<V, RequirementRecord>)
        return CostGroup::emission;
    else if constexpr (std::is_same_v<V, PatternDeclaration>)
        return CostGroup::pattern;
    else if constexpr (std::is_same_v<V, MeasureDefinition>)
        return CostGroup::measure;
    else if constexpr (std::is_same_v<V, JudgementDomainRecord>)
        return CostGroup::domain;
    else if constexpr (std::is_same_v<V, MergedDeclaration>)
        return CostGroup::merged;
    else if constexpr (std::is_same_v<V, ResourceRecord>)
        return CostGroup::resource;
    else if constexpr (std::is_same_v<V, RelationDeclaration>)
        return CostGroup::relation;
    else if constexpr (std::is_same_v<V, SolverProfileDeclaration>)
        return CostGroup::solver;
    else if constexpr (std::is_same_v<V, FactBindingRef>)
        return CostGroup::binding;
    else if constexpr (std::is_same_v<V, ClaimRow>)
        return CostGroup::claim;
    else
        static_assert(!sizeof(V), "No semantic table cost group");
}

struct HashCoreWork final {
    const detail::SemanticInput& input;
    detail::HashWorkLimits limits;
    detail::HashObservation& observation;
    detail::HashScratch scratch;
    detail::HashFrames frames;
    std::pmr::vector<detail::HashCostSlot> costs{&scratch};
    std::pmr::vector<std::uint32_t> relationResources{&scratch};
    std::array<std::span<detail::HashCostSlot>, 11> groups{};
    std::size_t currentEmission{SIZE_MAX};
    bool counting{true};
    std::optional<std::uint64_t> exportCapacity;
    detail::HashWorkspaceBuffer exportBuffer;
    std::optional<core::detail::Sha256> sha;
    std::uint64_t emitted{};

    HashCoreWork(const detail::SemanticInput& source, detail::HashOptions options,
                 detail::HashObservation& observed)
        : input{source}, limits{detail::effectiveHashWork(options.requested)},
          observation{observed}, scratch{limits, options.upstream, &observation}, frames{scratch},
          exportCapacity{options.exportCapacity}, exportBuffer{scratch} {
        observation.inlineObjectBytes = sizeof(*this);
        if (exportCapacity) {
            exportBuffer.allocate(*exportCapacity);
            limits.maxSemanticPreimageBytes =
                std::min<std::uint64_t>(limits.maxSemanticPreimageBytes, *exportCapacity);
        }
        const std::array<std::size_t, 11> sizes{input.graph.requirements.size(),
                                                input.patterns.size(),
                                                input.measures.size(),
                                                input.graph.judgementDomains.size(),
                                                input.graph.mergedNamespace.declarations.size(),
                                                input.graph.resources.size(),
                                                input.graph.relations.size(),
                                                input.graph.solverProfiles.size(),
                                                input.graph.factBindings.size(),
                                                input.claims.size(),
                                                input.entities.size()};
        std::uint64_t count = 1;
        for (auto size : sizes)
            count = detail::cappedAdd(count, size, limits.maxHashScratchBytes / 32);
        if (count > limits.maxHashScratchBytes / 32)
            detail::hashBudget("maxHashScratchBytes", "semanticHash.scratchBytes",
                               limits.maxHashScratchBytes, limits.maxHashScratchBytes + 1, false);
        costs.resize(static_cast<std::size_t>(count));
        std::size_t start = 0;
        for (std::size_t i = 0; i < sizes.size(); ++i) {
            groups[i] = std::span{costs}.subspan(start, sizes[i]);
            start += sizes[i];
        }
        if (input.relationResources.empty()) {
            relationResources.reserve(input.graph.relations.size());
            for (const auto& relation : input.graph.relations) {
                const auto it =
                    std::lower_bound(input.graph.resources.begin(), input.graph.resources.end(),
                                     relation.resourceRef, [](const auto& row, const auto& ref) {
                                         return row.ref < ref;
                                     });
                require(it != input.graph.resources.end() && it->ref == relation.resourceRef,
                        "relations.resource", "Dangling semantic relation resource");
                relationResources.push_back(
                    static_cast<std::uint32_t>(it - input.graph.resources.begin()));
            }
        }
    }
    auto group(CostGroup value) -> std::span<detail::HashCostSlot> {
        return groups[static_cast<std::size_t>(value)];
    }
    void add(detail::HashCost& destination, const detail::HashCost& value) const {
        destination.bytes =
            detail::cappedAdd(destination.bytes, value.bytes, limits.maxSemanticPreimageBytes);
        destination.visits =
            detail::cappedAdd(destination.visits, value.visits, limits.maxExpandedReferenceVisits);
        destination.maxKey = std::max(destination.maxKey, value.maxKey);
    }
    auto emission(const RequirementIdentity& identity) -> const detail::HashCost* {
        if (currentEmission < input.graph.requirements.size() &&
            &input.graph.requirements[currentEmission].identity == &identity) {
            const auto& slot = group(CostGroup::emission)[currentEmission];
            if (slot.flags == 1)
                return &slot.cost;
        }
        return nullptr;
    }
    struct EmissionScope final {
        HashCoreWork& work;
        std::size_t previous;
        EmissionScope(HashCoreWork& value, std::size_t index)
            : work{value}, previous{value.currentEmission} {
            work.currentEmission = index;
        }
        ~EmissionScope() {
            work.currentEmission = previous;
        }
    };
    template <typename Emit>
    auto resolve(CostGroup groupId, std::size_t index, Emit emit) -> detail::HashCost;
    void combine(Output& out, const detail::HashCost& value) const;
    void visitEdge(Output& out) const;
    void write(std::span<const std::byte> bytes) {
        require(!counting && observation.budgetAccepted && sha.has_value(), "semanticHash.phase",
                "SHA requires accepted count");
        sha->update(bytes);
        ++observation.shaUpdates;
        observation.shaBytes += bytes.size();
        if (exportCapacity) {
            require(bytes.size() <= exportBuffer.capacity() - emitted, "semanticHash.export",
                    "Export size disagrees");
            std::copy(bytes.begin(), bytes.end(),
                      exportBuffer.data() + static_cast<std::size_t>(emitted));
            observation.exportedBytes += bytes.size();
        }
        emitted += bytes.size();
    }
};
static_assert(sizeof(HashCoreWork) < 8192);
