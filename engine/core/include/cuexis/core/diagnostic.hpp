#pragma once

//  Diagnostic / Diagnostics - structured diagnostic system
//  Used for bulk validation problems (chart compilation, resource loading, and similar);
//  carries a severity, a stable code, a field path rooted at `$`, and context key/values.
//  Diagnostics supports a bounded capacity and deterministic ordering.

#include <cuexis/core/abi_warnings.hpp>
#include <cuexis/core/core_export.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::core {

CUEXIS_ABI_WARNING_PUSH

enum class DiagnosticSeverity {
    Info,
    Warning,
    Error,
};

struct DiagnosticContext {
    std::string key;
    std::string value;

    friend bool operator==(const DiagnosticContext&, const DiagnosticContext&) = default;
};

class CUEXIS_CORE_API Diagnostic final {
  public:
    Diagnostic(DiagnosticSeverity severity, std::string code, std::string message,
               std::string fieldPath = {});

    [[nodiscard]] DiagnosticSeverity severity() const noexcept;
    [[nodiscard]] std::string_view code() const noexcept;
    [[nodiscard]] std::string_view message() const noexcept;
    [[nodiscard]] std::string_view fieldPath() const noexcept;
    [[nodiscard]] const std::vector<DiagnosticContext>& context() const noexcept;

    Diagnostic& withContext(std::string key, std::string value) &;
    Diagnostic&& withContext(std::string key, std::string value) &&;

  private:
    DiagnosticSeverity severity_;
    std::string code_;
    std::string message_;
    std::string fieldPath_;
    std::vector<DiagnosticContext> context_;
};

class CUEXIS_CORE_API Diagnostics final {
  public:
    Diagnostics() = default;
    Diagnostics(std::size_t maxDiagnostics, Diagnostic limitDiagnostic);

    // The bounded capacity includes one caller-supplied limit diagnostic; a capacity of 0 is
    // normalized to 1 so that the limit diagnostic is preserved
    bool add(Diagnostic diagnostic);
    bool append(Diagnostics diagnostics);
    void clear() noexcept;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool hasErrors() const noexcept;
    [[nodiscard]] bool hasWarnings() const noexcept;
    [[nodiscard]] bool limitReached() const noexcept;
    [[nodiscard]] std::size_t count(DiagnosticSeverity severity) const noexcept;
    [[nodiscard]] const std::vector<Diagnostic>& items() const noexcept;

    // Sorts by stable machine-readable fields, ensuring reproducible output
    void sortDeterministically();

  private:
    struct LimitState {
        std::size_t maxDiagnostics;
        Diagnostic diagnostic;
        bool reached{false};
    };

    std::vector<Diagnostic> items_;
    std::optional<LimitState> limit_;
    std::size_t acceptedCount_{0};
};

CUEXIS_ABI_WARNING_POP

} // namespace cuexis::core
