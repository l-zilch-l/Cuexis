#pragma once

#include <cuexis/judgement/gameplay_assembler.hpp>
#include <optional>
#include <utility>

namespace cuexis::judgement::detail {
// One preparation owns one accepted budget and at most one exact declaration/result pair.
// Successful products own immutable storage; failures always retain the compiler's diagnostics.
class PatternCompileReuse final {
  public:
    explicit PatternCompileReuse(PatternCompileBudget budget = {}) : budget_(std::move(budget)) {}

    auto compile(const PatternDeclaration& pattern) -> core::Result<CompiledPattern> {
        if (previous_ && previous_->first == pattern)
            return previous_->second;
        auto compiled = compilePattern(pattern, budget_);
        if (compiled)
            previous_.emplace(pattern, *compiled);
        return compiled;
    }

  private:
    PatternCompileBudget budget_;
    std::optional<std::pair<PatternDeclaration, CompiledPattern>> previous_;
};
} // namespace cuexis::judgement::detail
