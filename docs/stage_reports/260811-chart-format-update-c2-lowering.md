# Stage Chart Format Update CFU-C2 Lowering Report

状态：completed implementation checkpoint

快照日期：2026-08-11

权威范围：[Stage Chart Format Update 实施计划](../stage_plans/stage_chart_format_update_implementation_plan.md)

## 交付

CFU-C2 在 `cuexis_chart` 内完成了不接入 Playback/Runtime 的 prepare 前格式解析层：

```text
Chart v4 与 CXT v1 canonical Writer
ChartParameter 默认值、宿主覆盖、类型/范围校验与 parameter identity
project-document lookup、CXT import、templateId/version/required-extension 校验
Template Binding 到 concrete Clip/Layer/BlendGroup/Instance 的 deterministic lowering
generated composite identity 与稳定诊断上下文
PropertyMask、组件、混合模式、离散权重和冲突验证
资源 requirement closure、capability requirement 推导和 checked aggregate budgets
canonical golden、round-trip、identity、lowering 与合同测试
```

`ResolvedChartDocument` 只保存 concrete typed value，`AnimationProgramInput` 不含 JSON DOM、CXC/CXT
path、ParameterRef、World、EnTT 或脚本执行入口。Chart/CXT/parameter identity 作为后续
`PreparedSemanticIdentity` 的输入组件保存；资源的实际 semantic identity 仍由 CFU-E3 在资源获取和
typed validation 后组合。

## 收口审阅

finding-first review 关闭了以下问题：

- Chart import Schema 与 typed Reader 同时要求 portable source path 以精确小写 `.cxt` 结尾。
- CXT format/version/required-extension 等 import 诊断统一携带 package-relative `source`、
  `template_id`、`import_id` 和字段路径；generated record 错误继续携带 Object、Binding、Template 与
  record kind。
- canonical Writer 在 string/Rational 主排序键相同时，以规范化 record 的 compact JSON bytes 作为
  最终 tie-break，消除合法 Reader 输入顺序对输出 bytes 的影响。
- v4 concrete projection 已通过 `CanonicalChartLoader` 复用 `ChartCompiler` 的 Timing、Behavior、
  component/reference 等 v3 语义门禁；没有增加重复编译路径。
- prepared-content Track/Segment/Step 预算按 prepare 峰值计数：imported source CXT Clip、Chart-local
  Clip 和每个 Binding 生成的 concrete Clip 分别进入总量。边界与 `limit - 1` 失败由测试固定。

## 验证

本报告的 2026-08-11 快照对应工作树于 2026-08-12 在 Visual Studio Developer 环境、
Debug preset 下完成定向与最终收口验证：

```text
cuexis_chart_tests: 98 cases, 625 assertions, zero failures
CFU-C2 filter after final diagnostic closure: 30 cases, 206 assertions, zero failures
Chart Format Update Schema artifact filter: 3 cases, 82 assertions, zero failures
cuexis_format_check: passed
complete Debug build: passed with MSVC
complete Debug CTest: 327 registered tests, zero failures; 1 platform-conditioned test skipped
documentation checks: 115 Markdown files and 20 candidate JSON/CXT files validated
version consistency check: 26.08.01-1
git diff --check: passed; only existing LF/CRLF conversion warnings were reported
```

第一次从普通 PowerShell 运行完整 CTest 时，7 个 external consumer gate 因子 CMake 进程没有
继承 Windows SDK library path 而以 `LNK1104: kernel32.lib` 失败；同一套件在声明的 Visual Studio
Developer 环境中重跑后全部通过。该结果不用本节替代 Stage Chart Format Update 的全阶段验收。

## 边界

CFU-C2 不声明以下能力：

```text
CXC ZIP32 envelope、archive Reader/Writer、package-backed provider 或 package identity
pack/validate/unpack CLI、binary golden 或 archive closure
PlaybackSource、prepare/capability public API integration 或 Chart v4 playback
AnimationProgram evaluation、mixing、World writes、runtime scripts 或 callbacks
Stage Chart Format Update completion 或 Stage 4 completion
```

下一检查点是 CFU-C3：严格 ZIP32 与内部 `cuexis_cxc` archive/package 实现。Stage 4 仍未开始。
