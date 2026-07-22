//  Diagnostics 实现 — 有界诊断收集器
//  add() 在有界模式下超过上限后插入 limitDiagnostic 并抑制后续条目
//  sortDeterministically() 按字段路径、严重级别、code、message 稳定排序，确保输出可复现

#include <cuexis/core/diagnostic.hpp>

#include <algorithm>
#include <tuple>
#include <utility>

namespace cuexis::core {

Diagnostic::Diagnostic(DiagnosticSeverity severity, std::string code, std::string message,
                       std::string fieldPath)
    : severity_(severity), code_(std::move(code)), message_(std::move(message)),
      fieldPath_(std::move(fieldPath)) {}

DiagnosticSeverity Diagnostic::severity() const noexcept {
    return severity_;
}

std::string_view Diagnostic::code() const noexcept {
    return code_;
}

std::string_view Diagnostic::message() const noexcept {
    return message_;
}

std::string_view Diagnostic::fieldPath() const noexcept {
    return fieldPath_;
}

const std::vector<DiagnosticContext>& Diagnostic::context() const noexcept {
    return context_;
}

Diagnostic& Diagnostic::withContext(std::string key, std::string value) & {
    context_.push_back(DiagnosticContext{std::move(key), std::move(value)});
    return *this;
}

Diagnostic&& Diagnostic::withContext(std::string key, std::string value) && {
    withContext(std::move(key), std::move(value));
    return std::move(*this);
}

Diagnostics::Diagnostics(std::size_t maxDiagnostics, Diagnostic limitDiagnostic)
    : limit_(LimitState{std::max<std::size_t>(maxDiagnostics, 1), std::move(limitDiagnostic)}) {}

bool Diagnostics::add(Diagnostic diagnostic) {
    if (limit_.has_value() && limit_->reached) {
        return false;
    }

    if (limit_.has_value() && acceptedCount_ >= limit_->maxDiagnostics) {
        items_.back() = limit_->diagnostic;
        --acceptedCount_;
        limit_->reached = true;
        return false;
    }

    items_.push_back(std::move(diagnostic));
    ++acceptedCount_;
    return true;
}

bool Diagnostics::append(Diagnostics diagnostics) {
    if (!limit_.has_value()) {
        items_.reserve(items_.size() + diagnostics.items_.size());
    }
    for (auto& diagnostic : diagnostics.items_) {
        if (!add(std::move(diagnostic))) {
            return false;
        }
    }
    return true;
}

void Diagnostics::clear() noexcept {
    items_.clear();
    acceptedCount_ = 0;
    if (limit_.has_value()) {
        limit_->reached = false;
    }
}

bool Diagnostics::empty() const noexcept {
    return items_.empty();
}

std::size_t Diagnostics::size() const noexcept {
    return items_.size();
}

bool Diagnostics::hasErrors() const noexcept {
    return count(DiagnosticSeverity::Error) != 0;
}

bool Diagnostics::hasWarnings() const noexcept {
    return count(DiagnosticSeverity::Warning) != 0;
}

bool Diagnostics::limitReached() const noexcept {
    return limit_.has_value() && limit_->reached;
}

std::size_t Diagnostics::count(DiagnosticSeverity severity) const noexcept {
    return static_cast<std::size_t>(
        std::count_if(items_.begin(), items_.end(), [severity](const Diagnostic& diagnostic) {
            return diagnostic.severity() == severity;
        }));
}

const std::vector<Diagnostic>& Diagnostics::items() const noexcept {
    return items_;
}

void Diagnostics::sortDeterministically() {
    std::stable_sort(
        items_.begin(), items_.end(), [](const Diagnostic& left, const Diagnostic& right) {
            return std::tuple{left.fieldPath(), left.severity(), left.code(), left.message()} <
                   std::tuple{right.fieldPath(), right.severity(), right.code(), right.message()};
        });
}

} // namespace cuexis::core
