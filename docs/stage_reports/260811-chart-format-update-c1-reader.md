# Stage Chart Format Update CFU-C1 Reader Report

状态：completed implementation checkpoint

快照日期：2026-08-11

权威范围：[Stage Chart Format Update 实施计划](../stage_plans/stage_chart_format_update_implementation_plan.md)

## 交付

CFU-C1 建立了不接入 Playback/Runtime 的生产格式读取基线：

```text
Chart v4、CXT v1 和 CXC manifest v1 production Schema
tests/fixtures/chart_format_update 下的 valid/invalid 正式 fixture
Chart v4 source model、CXT document model 和严格 typed Reader
内部 cuexis_cxc target、manifest source model 和 typed Reader
JSON/CXT LF 与未来 .cxc binary Git 属性
Schema、Reader、diagnostic、architecture 和 external consumer 测试
```

Chart v4 Reader 保留参数引用、CXT import、Clip、Animator、扩展、规范 source JSON 和字段路径；它
通过独立 v3 projection 复用既有 Timing、Behavior、Object 与资源引用验证，但没有修改 v1-v3 Loader
语义。CXT Reader 只接受声明式 local-time 模板，未知 `script` 核心字段稳定失败。

CXC 本批只读取 manifest JSON，并验证固定 format/version/project、portable path、ASCII case conflict、
entry 排序、byteCount、SHA-256、预算、扩展和确定诊断。ZIP envelope、archive bytes、闭包、Reader/
Writer、package-backed provider 与工具仍属于 CFU-C3/C4。

## Fixture Matrix

所有已提升的 JSON/CXT/manifest fixture 都经过对应 Schema 与 typed Reader：

- valid fixture 由两层共同接受。
- deep Animator patch、Asset 字段参数引用和 runtime-script CXT 同时由 Schema 与 typed Reader 拒绝。
- additive、mask、weight 和参数类型等语义负例保持 Schema-shaped，并由 typed Reader 拒绝。
- missing import 与 templateId mismatch 在 C1 保留为合法 source document，由 CFU-C2 的独立
  project-document lookup/resolver 拒绝。
- CXC entry order 与 ASCII case conflict 保持 Schema-shaped，由 manifest Reader 拒绝。

## 收口审阅

2026-08-11 的 finding-first 收口审阅额外关闭了以下 Reader 边界：

- CXT 零时长 Segment 后的相同 startBeat 仍按冲突拒绝，空 Step 数组按非法 Clip 拒绝。
- CXC path 拒绝 Windows 保留名和 trailing-dot segment，entry/total byte budget 使用 checked
  arithmetic。
- patch 内 ParameterRef 分离 semantic target path 与原始 source field path；deep Animator patch 不再
  产生缺少数组索引的重复诊断。
- BlendGroup 和 ClipInstance 的 `64`/`256` 嵌套数组预算由 typed Reader 显式执行。

## 验证

2026-08-11 在 Visual Studio Developer Shell、Debug preset 下完成：

```text
fresh configure and full Debug build
Chart v4/CXT targeted Catch2 tests
CXC manifest targeted Catch2 tests
Schema artifact targeted Catch2 tests
architecture test and target dependency allowlists
all seven clean external consumer/package tests
cuexis_format_check
full Debug CTest: 295 tests, zero failures; one environment-dependent Windows symlink test skipped
documentation check: 114 Markdown files and 20 candidate JSON/CXT files
version consistency: 26.08.01-1
git diff --check
```

本报告不替代后续 CFU-C2/C3/C4、Release 或 hosted 跨平台证据。

## 边界

CFU-C1 不声明以下能力：

```text
canonical Chart/CXT/manifest Writer or round-trip identity
ChartParameterSet resolution or parameter identity
CXT import lookup, templateId matching or deterministic Binding lowering
CXC ZIP32 envelope, package file/memory loading, closure or content provider
PlaybackSource, prepare/capability integration or Chart v4 playback
AnimationProgram evaluation, mixing, scripts or callbacks
```

下一检查点是 CFU-C2；Stage 4 仍未开始。
