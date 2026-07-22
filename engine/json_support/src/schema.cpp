//  JSON Schema 验证实现 — 基于 nlohmann/json-schema-validator
//  CollectingErrorHandler 将 Schema 违规转换为 Diagnostics，带字段路径定位
//  Schema 本身无法编译时返回操作错误；实例违规追加到 diagnostics 集合

#include <cuexis/json/schema.hpp>

#include "json_conversion.hpp"

#include <nlohmann/json-schema.hpp>

#include <exception>
#include <string>

namespace cuexis::json {
namespace {

class CollectingErrorHandler final : public nlohmann::json_schema::basic_error_handler {
  public:
    CollectingErrorHandler(core::Diagnostics& diagnostics, std::string_view rootFieldPath)
        : diagnostics_(diagnostics), rootFieldPath_(rootFieldPath) {}

    void error(const nlohmann::json::json_pointer& pointer, const nlohmann::json& instance,
               const std::string& message) override {
        nlohmann::json_schema::basic_error_handler::error(pointer, instance, message);
        auto path = rootFieldPath_;
        path.append(pointer.to_string());
        static_cast<void>(diagnostics_.add(core::Diagnostic{core::DiagnosticSeverity::Error,
                                                            "json.schema.validation_failed",
                                                            message, std::move(path)}));
    }

  private:
    core::Diagnostics& diagnostics_;
    std::string rootFieldPath_;
};

} // namespace

core::Result<void> validateAgainstSchema(const Value& instance, const Value& schema,
                                         core::Diagnostics& diagnostics,
                                         std::string_view rootFieldPath) {
    try {
        nlohmann::json_schema::json_validator validator;
        validator.set_root_schema(detail::toNlohmann(schema));

        core::Diagnostics validationDiagnostics;
        CollectingErrorHandler errorHandler{validationDiagnostics, rootFieldPath};
        validator.validate(detail::toNlohmann(instance), errorHandler);
        validationDiagnostics.sortDeterministically();
        static_cast<void>(diagnostics.append(std::move(validationDiagnostics)));
        return {};
    } catch (const std::exception& exception) {
        return core::unexpected(
            core::Error{"json.schema.invalid", "JSON Schema could not be compiled or evaluated"}
                .withContext("exception", exception.what()));
    }
}

} // namespace cuexis::json
