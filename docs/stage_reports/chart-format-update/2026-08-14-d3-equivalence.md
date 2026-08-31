# Stage Chart Format Update CFU-D3 Runtime Equivalence

状态：CFU-D3 complete；整包 CFU-D 未关（外部旧 Chart 资产仍未确认）

快照日期：2026-08-14

后续关闭：[CFU-D 关闭报告](2026-08-14-d-close.md) 已按项目所有者“未提供外部资产”决策关闭整包 CFU-D；兼容窗口不缩短。本文件仍是 D3 快照。

权威计划：[Stage Chart Format Update 实施计划](../../stage_plans/completed/chart-format-update/plan.md) §5.6

结构基线：[CFU-D1/D2 报告](2026-08-13-d-migration.md)

Playback 基线：[CFU-E 关闭报告](2026-08-14-e-close.md)

## 1. 结论

CFU-D3 已关闭。lift 后的空动画 v4 现在与仓库内源 Chart 在 Playback 公共面上对齐：

```text
源 v3 vs migrateToV4              FrameSnapshot 与 FrameDigest v3 位级相同
v1 的 migrateToV3 vs migrateToV4  FrameSnapshot 与 FrameDigest v3 位级相同
源 v1 vs migrateToV4              snapshot 在 1e-6 误差预算内；digest 不要求位级相同
源 v2 vs migrateToV4              snapshot 在 1e-6 误差预算内
v2 的 migrateToV3 vs migrateToV4  FrameSnapshot 与 FrameDigest v3 位级相同
含 Stop 的 v3 vs migrateToV4      同一时间表与 400→100 seek 位级相同
```

`chartFormatVersion`、capability 与 `PreparedSemanticIdentity` 允许不同。该关闭不是整包 CFU-D、完整 CXC 产品支持、公共 package API、完整 v4 动画 Playback、CFU-F/CFU-G 或 Stage 4。仓库外旧 Chart 资产仍未确认；全部 v1/v2/v3 Reader 与迁移入口保留。

## 2. 测试矩阵

新增 [`tests/playback/playback_migration_equivalence_tests.cpp`](../../../tests/playback/playback_migration_equivalence_tests.cpp)。内部测试链接 `cuexis::chart` 以调用 `ChartMigrator`，不改变安装头或 public consumer。既有 [`tests/runtime/migration_equivalence_tests.cpp`](../../../tests/runtime/migration_equivalence_tests.cpp) 仍只覆盖 v1↔v3 Runtime World 分量，不作为 D3 关闭证据。

| 用例 | 源 | 比较 |
| --- | --- | --- |
| 静态 v3 lift | `tests/fixtures/chart_format_update/valid/chart_v3_static_migration.json` | 位级 snapshot + digest |
| v3 Behavior/Stop + seek | `assets/charts/stage2_example.cuexis.chart.json` | 位级 snapshot + digest；Stop 前/中/后与 `400→100` |
| v1 Quaternion hop | `tests/fixtures/stage2_migration_v1.cuexis.chart.json` | `1e-6` snapshot；digest 只断言 algorithmVersion 3 |
| v1 v3 hop vs 再 lift | 同一 v1 fixture | 位级 snapshot + digest |
| v2 hop | 测试内嵌无 audio 最小 v2 | 源 vs v4 用 `1e-6`；v3 hop vs v4 位级 |

时间表复用 Stage 2 采样并覆盖 Stop 区间：`{-500,0,125,250,375,500,750,1000,1100,1250,2000}` ms，另加 `400→100` discontinuity seek。未改 FrameDigest v3 历史 golden。

## 3. Local verification

工具链：Visual Studio 2026 Community 18 / MSVC 19.51，CMake 4.3.3，Windows x64。

```text
cmake --build --preset debug --target cuexis_playback_tests cuexis_chart_tests   passed
cuexis_playback_tests [cfu-d3]                                                  7197 assertions / 5 cases passed
cuexis_chart_tests [migration]                                                  133 assertions / 9 cases passed
cuexis_playback_tests                                                           9110 assertions / 52 cases passed
cuexis_chart_tests                                                              714 assertions / 103 cases passed
tools/check_docs.py                                                             125 Markdown / 20 JSON-CXT passed
tools/update_version.py --check                                                 passed (26.08.01-1)
git diff --check                                                                passed
```

本报告没有 hosted Linux/Windows/MinGW、sanitizer 或 Release 全量 CTest；那些数字属于 CFU-F/CFU-G。

## 4. 兼容边界

```text
无整包 CFU-D 关闭
无外部旧 Chart 资产确认
无完整 CXC 公共产品支持或公共 package API
无 Stage 4 动画采样、混合或 AnimationSystem
无 FrameSnapshot / FrameDigest v3 结构变化
无 runtime scripts、per-frame callbacks 或 bytecode
```

## 5. Handoff

下一批次是 CFU-F。整包 CFU-D 仍等待项目所有者确认仓库外旧 Chart 资产清单，或明确记录“未提供外部资产”及兼容窗口决策。不得从 D3 已关闭推断格式阶段完成或 Stage 4 已开始。