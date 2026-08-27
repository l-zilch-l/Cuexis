# Stage 4 S4-G Safety, Allocation, and Performance Report

状态：S4-G local checkpoint；Stage 4 仍为 unblocked / not started 的整体阶段

快照日期：2026-08-27

权威范围：[Stage 4 实施计划](../stage_plans/stage_4_implementation_plan.md) 与
[CFU-G2 Stage 4 typed handoff](260816-chart-format-update-g2-stage4-handoff.md)。

## 交付

S4-G 把 compiled animation 接到 G2 已校验的 Clip/Track/Segment/generated-record 总量和每帧
600,000 Property Write 上限，冻结 warmed 热路径分配合同，并复用 F4 最大内容方法记录趋势。

```text
AnimationCompiler::compile(input, limits, maxWritesPerFrame)
generated totals -> animation.compile.generated_limit
write budget    -> animation.compile.write_limit
overflow-safe requiredAnimationWrites / animationWriteBudgetFits
diagnostics stop at maxDiagnostics; last item is animation.diagnostics.limit_exceeded
runtime fieldPath is clip.fieldPath or $/animationProgram; never a forged JSON path
empty Chart v1-v4 and Stage 2/3 warmed update/extract remain zero new allocations
nonempty CXT uses a bounded contract: second 64-frame window <= first
CUEXIS_RUN_PERFORMANCE_PROBE=1 records compile/hot-frame/memory trends; no machine thresholds
```

应用配置不改变动画语义。Linux ASan/UBSan 仍由 S4-H hosted Quality 执行；本地 MSVC 不能跑该
sanitizer 预设。

## 分配合同

书面冻结如下：

1. 空 Chart v1–v4：warmed `update()` 与复用 `FrameSnapshot` 的 `extractFrame()` 为零新增分配。
2. 既有 Stage 2 材质步进与 Stage 3 portable/Validation Sink 热路径保持零新增分配。
3. 非空合法 CXT：热身后再跑两个连续 64 帧窗口；第二窗口分配次数必须小于或等于第一窗口。该有界
   合同覆盖 `rebuildAnimationBaselines` 等每帧复用容器，不把机器相关绝对次数写成硬阈值。

热路径通过 PropertyWriteBuffer / PropertyValue 字符串容量复用、跳过空 Host/Preview override
scratch，以及按 resolver entry 数量预留 `thisCommit_` 来满足上述合同。

## 明确未改

- FrameDigest v3 字段与算法版本
- SDK API `0.6.0`
- 公共 CXC package API
- `cuexis_animation` 安装组件
- Stage 4 整体 completed 状态

`engine/animation/` 仍只消费 typed `AnimationProgramInput`，不解析 JSON、CXC 或 CXT。

## 本地验证

2026-08-27 MSVC Debug：

```text
cuexis_format_check
cuexis_architecture_tests
cuexis_animation_tests.exe [s4-g]  8 cases / 6243 assertions
cuexis_animation_tests.exe         32 cases / 8789 assertions
cuexis_world_tests.exe [s4-g]
cuexis_playback_allocation_tests.exe [allocation]  5 cases / 144 assertions
cuexis_playback_tests.exe [v4]     16 cases / 241 assertions
cuexis_s4g_performance             skipped unless CUEXIS_RUN_PERFORMANCE_PROBE=1
python -B tools/check_docs.py
git diff --check
```

非空 CXT 有界分配（同一 Debug 进程，64 帧窗口）：

```text
S4-G nonempty CXT first=8064 second=8064
```

显式探针 `cuexis_s4g_performance_probe`（`clip_count=128`，`object_count=128`，1024 热帧）：

| Metric | Debug |
| --- | ---: |
| compile_us | 1310.000 |
| warmed_evaluate_avg_us | 4144.094 |
| compile_resident_delta_bytes | 290816 |
| hot_peak_delta_bytes | 0 |

这些数值是本 worktree 的趋势证据，不能作为不同 CI runner 的硬回归阈值。默认 CTest 跳过探针。

S4-G 未把 Stage 4 标为 completed，也不是完整 v4 动画 Playback。Sanitizer 与三平台 hosted 留在
S4-H。

## 下一步

S4-H：hosted 验收与关闭。S4-G 关闭不表示 Stage 4 已完成。
