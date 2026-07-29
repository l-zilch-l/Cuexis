#pragma once

//  Error - the unified error type; must carry a stable error code and a readable message
//  Program logic must branch on the code, never on the message text, which may be localized
//  Error supports attached context key/value pairs and a causal chain (cause) to aid
//  diagnosis

#include <cuexis/core/abi_warnings.hpp>
#include <cuexis/core/core_export.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::core {

CUEXIS_ABI_WARNING_PUSH

struct ErrorContext {
    std::string key;
    std::string value;
};

class CUEXIS_CORE_API Error final {
  public:
    Error(std::string code, std::string message);

    [[nodiscard]] std::string_view code() const noexcept;
    [[nodiscard]] std::string_view message() const noexcept;
    [[nodiscard]] const std::vector<ErrorContext>& context() const noexcept;

    // Returns the upstream error in the cause chain; the returned pointer stays valid as long
    // as this Error holds its cause
    [[nodiscard]] const Error* cause() const noexcept;

    // Chained diagnostic context (lvalue overload, returns a reference to itself)
    Error& withContext(std::string key, std::string value) &;
    // Chained diagnostic context (rvalue overload, enables the Error{} << "key" << "value"
    // style)
    Error&& withContext(std::string key, std::string value) &&;

    // Sets the upstream error in the cause chain
    Error& withCause(Error cause) &;
    Error&& withCause(Error cause) &&;

  private:
    std::string code_;
    std::string message_;
    std::vector<ErrorContext> context_;
    std::shared_ptr<const Error> cause_;
};

CUEXIS_ABI_WARNING_POP

} // namespace cuexis::core
