# 诊断、身份与兼容性

状态：active

更新日期：2026-08-30

适用版本：SDK API `0.7.0`

文档角色：公共 API 参考

权威头文件：[result.hpp](../../engine/core/include/cuexis/core/result.hpp)、
[error.hpp](../../engine/core/include/cuexis/core/error.hpp)、
[diagnostic.hpp](../../engine/core/include/cuexis/core/diagnostic.hpp)、
[playback_session.hpp](../../engine/playback/include/cuexis/playback/playback_session.hpp)

## 快速结论

- 可恢复失败使用 `core::Result<T>`，不能忽略返回值。
- `Error` 描述一次操作失败；`Diagnostics` 描述一组可排序的问题。
- 机器判断优先使用 code 和 fieldPath，不只匹配 message。
- `PreparedSemanticIdentity`、`PresentationContentIdentity` 和 `FrameDigest` 含义不同。
- SDK `0.7.0` 仍保持 Chart v1/v2/v3 Reader 与 FrameDigest v1-v3 兼容合同。

## 错误与诊断速查

| 类型 | 用途 | 关键观察面 |
| --- | --- | --- |
| `core::Result<T>` | 成功值或单个失败 | value / `core::Error` |
| `core::Error` | 操作级失败 | code、message、context、cause |
| `core::Diagnostic` | 一个 warning/error | severity、code、message、fieldPath、context |
| `core::Diagnostics` | 有界诊断集合 | items、count、limitReached、deterministic order |

调用方应记录 code、message、fieldPath 和 context。code 用于程序分支，message 用于人类阅读；message 不是
稳定枚举值。

## 失败阶段顺序

prepare 诊断按以下阶段形成并保持稳定顺序：

1. parse
2. semantic validation
3. import resolution
4. parameter resolution
5. budget validation
6. capability preflight
7. compile/lowering

实现不得因 unordered container、并发完成顺序或异常转换改变可观察诊断顺序。

## 身份类型速查

| 类型 | 标识对象 | 不能替代 |
| --- | --- | --- |
| `PreparedSemanticIdentity` | 完成解析、资源校验、参数冻结和 lowering 后的 prepared 语义 | CXC package bytes、单帧结果 |
| `PresentationContentIdentity` | 单个 portable resource 内容 | 整个 Playback source |
| CXC package identity | 精确 package bytes 与 entry | 跨 source semantic identity |
| `FrameDigest` | 指定时间输入与 snapshot 的 canonical 帧结果 | prepared 内容身份 |

失败 reload 不得改变 active semantic identity。相同规范内容与相同冻结参数跨 source 应产生相同
`PreparedSemanticIdentity`。

## 兼容合同

- SDK API 当前为 `0.7.0`。
- Chart v1/v2/v3 Reader、迁移入口和已公开旧 API 按 ADR 0041 退出政策维护。
- FrameDigest v1-v3 和 canonical bytes/order 保持稳定。
- installed Playback headers 不暴露 EnTT、SDL、OpenGL/GLAD、JSON DOM、RuntimeSession 或 World。
- 异常不能跨越模块公共边界。
- 运行时脚本和逐帧 script callback 无限期延后，不存在预留 ABI 或 capability。

相关内容：[ADR 0027](../adr/0027-playback-sdk-product-boundary.md)、
[ADR 0041](../adr/0041-legacy-format-exit-policy.md)、[帧与摘要](frames-digests-and-timelines.md)。
