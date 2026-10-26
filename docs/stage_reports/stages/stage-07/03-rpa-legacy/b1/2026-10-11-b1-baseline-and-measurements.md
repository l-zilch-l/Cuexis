# B1.3/B1.4 原路径基线与初始测量

状态：implementation evidence；B1 观测、基线和测量设计已完成，RPA 产品协议仍未完成

证据日期：2026-10-11（Asia/Shanghai）。本记录承接
[B1.1/B1.2 设施记录](2026-10-11-b1-observation-and-oracle.md)，覆盖原路径 Release
基线、平台 owner 只读核对、观测开销口径和初始测量输出。

## 工作身份和配置

- branch：`stage-7`
- HEAD：`7ca1fc6cf2dd4c7072aab3ae260ee5931f5df2d3`
- SDK/date build：`0.7.5` / `26.10.10-1`
- 工作树：保留既有未提交改动；本轮只增量修改 probe、B1 记录、状态和索引
- Release 配置：`headless-release --fresh`，实际编译器为
  `C:/msys64/ucrt64/bin/x86_64-w64-mingw32-g++.exe`，triplet `x64-mingw-static`

MSVC `cl.exe` 未在当前 PowerShell 环境进入 PATH，因此没有冒称 Windows/MSVC Release。
此前使用不匹配的 `headless-debug` 被仓库配置保护拒绝，随后使用匹配 MinGW preset 完成了
本次 Release 基线。

## B1.3 原路径和 owner 核对

只读核对得到以下当前边界，未把它们当作 RPA 新协议实现：

| 责任域 | 当前入口和证据 | 当前 owner 结论 |
| --- | --- | --- |
| capture/control | `app/player/src/player_gameplay.cpp` 的 `PlayerGameplay::step`；SDL 离散输入在 host/player loop 形成 `GameplayInput` | 当前是 host/player owner；使用 test-only `hStep`，不是 RPA capture-time 时钟 |
| admission/seal/Fold | `PlaybackSession::submitGameplay` / `advanceGameplay` / `gameplayAdvanceReceipt` | 当前由 Playback/Judgement 同步调用；RPA D2、G1/Q1/K1 尚未接入 |
| publish/consume | `PlaybackSession::queryGameplay`、`archiveGameplay`、`evaluateGameplayReplay` | 当前 host 同步查询和完整 Replay 比较；没有 RPA immutable prefix 或跨 worker owner |
| present | `PlaybackSession::extractFrame` 与 `ValidationSink::validateFrame` | headless 验证 proxy；不代表 D5 GPU present 或用户首见 |
| audio clock | `engine/audio/src/audio_transport.cpp` 的 source-clock 校验和 `RuntimeTimeline` | 现有 audio/host clock 合同；没有 RPA `ClockObservation` 的生产发布 |
| thread ownership | `engine/playback/src/playback_session.cpp` 的 `ThreadChecker` / `not_owner_thread` | 当前 PlaybackSession、PreparedPlayback 和内部 Runtime 状态保持调用线程 owner；没有 RPA worker/domain 拆分 |

这份核对确认了 B1 的观测边界：事件身份和 manifest 可以独立建设，但不能把现有 host
`hStep`、同步 `advanceGameplay` 或 headless validation 命名成 RPA 生产行为。

## B1.4 初始数据

命令：

```text
cmake --preset headless-release --fresh -DVCPKG_TARGET_TRIPLET=x64-mingw-static
cmake --build out/build/headless-release --target stage3_performance_probe cuexis_s5h_performance_probe --clean-first
out/build/headless-release/bin/stage3_performance_probe.exe
out/build/headless-release/bin/cuexis_s5h_performance_probe.exe
```

原路径 probe 输出如下。所有时间为同机 `steady_clock` 趋势值，不能跨机器比较，也不是 F3
硬预算：

| Probe | p50 us | p95 us | p99 us | max us | 其它初始输入 |
| --- | ---: | ---: | ---: | ---: | --- |
| Stage 3 update+extract headless proxy | 1.2 | 1.3 | 1.9 | 25.9 | avg 1.257 us；prepare 275628.1 us；reload 270078.6 us |
| Stage 3 headless validation proxy | 0.7 | 0.7 | 0.7 | 3.3 | avg 0.657 us |
| S5-H warmed update+extract | 0.6 | 0.7 | 0.7 | 0.9 | avg 0.625 us；prepare 1112.6 us |

资源/保留初始输入：

- Stage 3 最大 texture：encoded `67108864` bytes，decoded `67108836` bytes。
- Stage 3 resident after prepare `208891904` bytes；reload peak delta `134430720` bytes。
- S5-H prepare resident delta `1466368` bytes；hot peak delta `12288` bytes。

这些数据用于后续 M-CAP/M-SPLIT/F3 的起始记录，不自动形成接受阈值。

## EOF/tail 和缺样记录

当前 Playback/Stage 3/S5-H probe 没有 RPA `TailClockTransition`、EOF pending timer 或
`ClockObservation` 事件源，因此不能从原路径伪造尾段 timer 分布。B1 已把缺样显式记录为：

```text
tail_timer_distribution = unavailable_on_legacy_path
missing_sample_reason = no_RPA_tail_event_source_before_B3
production_tail_profile = not_enabled
```

后续 B3/B5 必须在真正的 RPA execution/tail source 上补齐 EOF 锚点、timer/window 分布、终止
horizon、观察误差和有效期；本记录不把当前缺样改写成通过。

## B1 退出结论

B1.1/B1.2 的事件身份、manifest、可控交付、fake clock、独立有理数和扫描 oracle 已由前一份
记录的 6/6 定向测试验证；B1.3/B1.4 的原路径 Release probe、owner 只读核对、分位数/峰值
输出和缺样口径已在本记录落档。B1 后续依赖已具备，RPA 产品协议、真实 clock drift、EOF
tail、跨线程 owner、音频/GPU、实体设备和 hosted 验证仍按 B2–B5/F1–F4 执行。

本轮未提交、未推送、未创建 PR。
