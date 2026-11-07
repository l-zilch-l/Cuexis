#pragma once

// Capsule closure entry points shared by the units.

#include "gameplay_capsule_lookup_internal.hpp"
#include "gameplay_capsule_relation_internal.hpp"

namespace cuexis::gameplay_packed {
namespace capsule_detail {

void validate(Model& m, chart::PackedChartLimits limits,
              const PatternCompileBudget& patternBudget = {});

auto makeModel(const EncodeRequest& request, chart::PackedChartLimits limits) -> Model;

auto staticTables(Model& m, chart::PackedChartLimits limits) -> gp::StaticTables;

auto preimage(Model& m, chart::PackedChartLimits limits) -> Bytes;

auto hashModel(const Model& m, detail::HashOptions options = {},
               detail::HashObservation* observation = nullptr) -> detail::SemanticHashResult;

auto physicalTables(Model& m, gp::StaticTables tables, chart::PackedChartLimits limits)
    -> gp::StaticTables;

auto envelope(Model& m, gp::StaticTables tables, chart::PackedChartLimits limits,
              detail::HashOptions hashOptions = {}, detail::HashObservation* observation = nullptr)
    -> Bytes;

auto decoded(std::span<const std::byte> bytes, const DecodeContext& context,
             chart::PackedChartLimits limits, detail::HashOptions hashOptions = {},
             detail::HashObservation* observation = nullptr) -> PreparedCapsule;

// The visitor rows are shared by the preimage, the static tables and the envelope.
template <typename M> void semanticRows(Output& a, M& m) {
    globalRow(a, m);
    a.count(m.graph.requirements.size());
    for (std::size_t i = 0; i < m.graph.requirements.size(); ++i) {
        std::optional<HashCoreWork::EmissionScope> scope;
        if (a.operation)
            scope.emplace(*a.operation, i);
        requirementRow(a, m.graph.requirements[i], m.indices[i], m);
    }
    definitions(a, m);
    coordination(a, m);
}

template <typename T, typename F> auto boundary(F operation) -> core::Result<T> {
    try {
        if constexpr (std::is_void_v<T>) {
            operation();
            return {};
        } else {
            return operation();
        }
    } catch (detail::HashWorkFailure& error) {
        return core::unexpected(std::move(error.error));
    } catch (Failure& error) {
        return core::unexpected(std::move(error.error));
    } catch (const std::exception&) {
        return core::unexpected(core::Error{"packed.requirements.invalid",
                                            "Capsule could not produce an owning result"});
    }
}

inline auto sum(std::span<const std::uint32_t> values) -> std::uint32_t {
    std::uint64_t total = 0;
    for (auto v : values) {
        total += v;
    }
    require(total <= UINT32_MAX, "GPH0.counts", "table count sum exceeds u32");
    return static_cast<std::uint32_t>(total);
}

} // namespace capsule_detail
} // namespace cuexis::gameplay_packed
