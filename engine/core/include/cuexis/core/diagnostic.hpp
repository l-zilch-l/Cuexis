#pragma once

//  Diagnostic / Diagnostics — 结构化诊断系统
//  用于批量校验问题（Chart 编译、资源加载等），包含严重级别、稳定 code、
//  以 `$` 为根的字段路径和上下文键值。Diagnostics 支持有界容量和确定性排序。

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::core {

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

class Diagnostic final {
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

class Diagnostics final {
  public:
    Diagnostics() = default;
    Diagnostics(std::size_t maxDiagnostics, Diagnostic limitDiagnostic);

    // 有界容量包含一条调用方提供的上限诊断；0 容量会被归一化为 1 以保留上限诊断
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

    // 按稳定机器可读字段排序，确保输出可复现
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

} // namespace cuexis::core
