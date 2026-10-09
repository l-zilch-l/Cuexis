//  Judgement typed kernel - S7A-1 诊断承载结构投影实现。
//
//  本文件只做一件事：把承载结构投影进 core 的 Result 错误通道。
//  它不定义任何诊断码：码表、类别集合与严重度集合属第 6 轮
//  （CM-D04 / P2-04 / P2-06），尚未建立。
//  调用方自带 token，因此承载结构承载什么值与本模块无关。

#include <cuexis/judgement/diagnostic.hpp>

#include <string>
#include <string_view>

namespace cuexis::judgement {
namespace {

//  上下文条目只在适用时附加；空 token 表示该分量对本条诊断不适用（见头文件约定）。
void attach(core::Error& error, std::string_view key, std::string_view value) {
    if (value.empty()) {
        return;
    }
    error.withContext(std::string{key}, std::string{value});
}

} // namespace

auto toError(const Diagnostic& diagnostic) -> core::Error {
    core::Error error{std::string{diagnostic.code}, std::string{diagnostic.summary}};
    attach(error, "category", diagnostic.category);
    attach(error, "severity", diagnostic.severity);
    error.withContext("faulted", diagnostic.faulted ? std::string{"true"} : std::string{"false"});
    attach(error, "field.section", diagnostic.context.fieldPath.section);
    attach(error, "field.path", diagnostic.context.fieldPath.path);
    attach(error, "requirement.kind", diagnostic.context.requirement.kind);
    attach(error, "requirement.identity", diagnostic.context.requirement.identity);
    attach(error, "identity.component", diagnostic.context.identity.component);
    attach(error, "identity.token", diagnostic.context.identity.token);
    attach(error, "capabilityId", diagnostic.context.capabilityId);
    attach(error, "remediation", diagnostic.context.remediation);
    //  原始时间戳只是诊断上下文（Spec 3.7.3）：它以不透明 token 附加，绝不参与判定时间定位。
    attach(error, "raw.time", diagnostic.context.rawTime);
    //  预算上下文只通过接口读取，且只读描述符：本批次不携带任何数值或上限。
    if (diagnostic.context.budget != nullptr) {
        attach(error, "budget.domain", diagnostic.context.budget->budgetDomain());
        attach(error, "budget.subject", diagnostic.context.budget->budgetSubject());
    }
    return error;
}

} // namespace cuexis::judgement
