# Stage Chart Format Update CFU-D1/D2 Migration Report

状态：D1/D2 本地 Debug 验证已取得并关闭；整包 CFU-D 未关（D3 等待 CFU-E）

快照日期：2026-08-13；验证补记：2026-08-13 同日稍后取得本地 Debug 证据

后续关闭：[CFU-E 关闭报告](260814-chart-format-update-e-close.md) 已关闭整包 CFU-E；CFU-D3 现在可以开始。整包 CFU-D 仍未关。

权威计划：[Stage Chart Format Update 实施计划](../stage_plans/stage_chart_format_update_implementation_plan.md)

## 1. 结论

本轮落地 CFU-D1/D2 的显式 Chart 迁移，不是完整 CFU-D 关闭，也不是 CXC 产品支持或 Playback
接入。

```text
保留 ChartMigrator::migrateToV3 与默认 CLI v3 输出
新增 ChartMigrator::migrateToV4
CLI 增加可选 --target 3|4，缺省仍为 3
v3 → v4 只做 JSON lift：version=4 + 空 parameters/animationTemplateImports/animationClips
v1/v2 → v4 复用 migrateToV3，再对规范化 v3 JSON lift
拒绝 v4 及其他 version，不写 artifact
v3 报告字节继续对齐 tests/fixtures/stage2_migration_report.golden.json
```

该检查点不得描述为 CFU-D complete 或 Stage 4 完成。D1/D2 关闭后下一批次是 CFU-E；CFU-D3
必须等 Playback 能加载 v4。仓库外旧 Chart 资产未确认，全部 v1/v2/v3 Reader 与迁移入口保留。

## 2. 交付范围

### 2.1 库

- `ChartMigrator::migrateToV4` 先 peek 顶层 `version`，避免合法 v3 被 `ChartLoader` 拒绝后无法
  进入 lift。
- v1/v2 调用现有 `migrateToV3`，再对 `chartJson` lift。
- v3 先由 `ChartLoader` 证明合法，再对源 JSON lift。
- lift 后走 `ChartV4Loader::load` → `ChartWriter::writeV4`；失败合并 v4 diagnostics，无 artifact。
- `ChartMigrationArtifact.v4Document` 仅 v4 路径填充；`document` 使用 `legacyProjection`。
- source/target identity 哈希 Writer canonical bytes（`ChartWriter::writeCanonicalJson` /
  `writeV4`），复用内部 SHA-256。
- 未引入 `ChartV4Resolver`、CXT、CXC pack 或 Playback 路径。

### 2.2 报告

v3 报告 key 集合不变。`targetVersion == 4` 时追加：

```text
discardedFields
diagnostics
fieldCounts.{animationClips,animationTemplateImports,behaviors,objects,parameters}
generatedBindings / generatedClips / generatedParameters
sourceCanonicalIdentity / targetCanonicalIdentity
warnings
```

v1/v2 → v4 保留 v3 hop 的 `convertedBehaviors` / `generatedEvents` / `rewrittenBindings` /
`expandedTemplateObjects` / `unboundBehaviorIds`；纯 v3 → v4 这些计数为 0。报告不把 CXC
pack/unpack 写成 migration。

### 2.3 CLI

- 旧调用继续合法，默认 `migrateToV3`。
- `--target 4` 走 `migrateToV4`；非法 `--target` 为 exit 2。
- 继续复用 `commitOutputs()`：路径互不冲突，失败不改已有目标。
- 无 `--target` 的 v3 输入继续拒绝，以保住既有 CMake 门禁。

## 3. 测试与 fixture

库测试扩 `tests/chart/chart_migrator_tests.cpp`，不改现有 `[stage2]` 断言：

```text
合法 v3 → canonical 空动画 v4，reload ChartV4Loader
与 chart_v4_static_migration.canonical.json 字节一致
v1 fixture → v4：保留 v3 hop 计数，chart 对齐真实 writeV4 LF golden
含 tempoEvents/templates 的 v3 不被 ChartWriter::write 吃掉
拒绝 v4 源和非法 v3，无 artifact
```

`cmake/VerifyChartTools.cmake` 增加 `--target 4`：v4 chart golden 字节比较、报告字段/identity
正则、失败不改目标、非法 `--target` exit 2。v4 报告不提交 SHA golden。

新增：

```text
tests/fixtures/chart_format_update/valid/chart_v3_static_migration.json
tests/fixtures/chart_format_update/golden/chart_v1_to_v4.cuexis.chart.json
```

`tests/runtime/migration_equivalence_tests.cpp` 仍只覆盖 v1↔v3。本轮不加 FrameDigest /
Playback v4 等价。

## 4. 本地 Debug 验证

初稿时工作树没有可用的 `out/build/debug`，且代理 Shell 传输在命令启动前关闭。同日稍后在
本 worktree 取得 Visual Studio 2026 + vcpkg + CMake 4.2.1 的 Debug 证据：

```text
cmake --preset debug --fresh                         已通过
cmake --build --preset debug --target
  cuexis_chart_tests cuexis_chart_migrator
  cuexis_chart_validator                             已通过
out/build/debug/bin/cuexis_chart_tests.exe
  [chart][migration]                                 9 cases / 133 assertions
out/build/debug/bin/cuexis_chart_tests.exe           102 cases / 707 assertions
ctest --test-dir out/build/debug
  -R cuexis_chart_tool_tests                         1/1 passed
```

构建中途曾把 CTest label `cuexis_chart_tool_tests` 误当成编译目标；真实目标是
`cuexis_chart_tests` / `cuexis_chart_migrator` / `cuexis_chart_validator`。identity 哈希改用
`detail::sha256Hex`，避免 D1/D2 引入 Resolver。`chart_v1_to_v4.cuexis.chart.json` 初稿是
CRLF 且仍按 v3 Writer 事件顺序；已用 `cuexis_chart_migrator --target 4` 重写为 5698 字节
LF-only `writeV4` 输出（behavior events 按 `property`/`startBeat` 排序，单尾随 LF）。

本快照未跑全量 `ctest --preset debug`、`cuexis_format_check`、`check_docs.py` 或 hosted CI。
这些不是 D1/D2 关闭条件，也不把整包 CFU-D 标为完成。

## 5. 兼容边界

```text
无 PlaybackSource / prepare / reload v4 接入
无 FrameSnapshot / FrameDigest v3 等价
无 CXT 生成、参数/Clip/Binding/Animator lowering
无 CXC pack 作为 migration
无公共 API / 安装头变化
无 Stage 4 animation runtime
外部旧 Chart 资产未确认
```

D1/D2 已按结构/Writer golden 与 CLI 合同关闭。下一批次是 CFU-E。不得开始 CFU-D3。