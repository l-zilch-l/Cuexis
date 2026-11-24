# RR-4 候选安装与宏头：合同 vs 现状 diff 清单

状态：evidence

更新日期：2026-11-23

<!-- cuexis-meta
id: EVIDENCE-stage-07-rr.t-03-install-diff
status: evidence
authority: none
date: 2026-11-23
-->

本清单只描述当前源码/CMake 与已接受合同之间的差异，供 RR-4（T-08）限定迁移。未执行安装、未构建
消费者、未修改任何安装面。

## 已采用合同

- D-24：默认与候选构建中的共享公开类保持同一定义；新增未成熟能力先经独立实验头和窄扩展入口试用，
  不再向既有公开类的宏分支添加方法；实验包按 D-19 显式选用、独立安装
  （`docs/stage_plans/active/stage-07-rr/architecture-design.md:463`）。
- D-18：纯候选播放读取器只编入候选构建；两种 Player 构建共用同一套会话、时钟与渲染路径；默认构建
  拒绝显式实验请求（`architecture-design.md:317-327`）。
- D-25：当前只完成格式文字更正，未改变读取实现或现有 wire；渲染迁移仍是目标设计
  （`architecture-design.md:477`）。

## 现状（`engine/playback`）

| 维度 | 现状 | 证据 |
| --- | --- | --- |
| 安装头集合 | `playback_session.hpp`、`playback_source.hpp`、`presentation_candidate.hpp` 等无条件进入 `PUBLIC FILE_SET HEADERS`；只有 `gameplay_candidate.hpp`、`execution_control.hpp`、`execution_coordination.hpp` 在候选开关下追加 | `engine/playback/CMakeLists.txt:59-74`、`:77-82` |
| 宏定义传播 | `CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE=1` 只在候选构建以 `PUBLIC` 定义；默认构建不定义 | `CMakeLists.txt:47-54` |
| 共享公开类声明 | `PlaybackSession` 的公开候选方法（进入执行 fence、commit、control 等）在 `#if` 块内；私有 helper 也有 `#if` 块 | `engine/playback/include/cuexis/playback/playback_session.hpp:24-28`、`:279-379`、`:465-476` |
| 源输入声明 | `PlaybackSource` 的 `fromCxc...Gameplay...` 工厂与私有 `fromGameplayEncoded` 在 `#if` 块内 | `include/cuexis/playback/playback_source.hpp:10-12`、`:98-...`、`:139-...` |
| 无条件候选入口 | `PreparedPlayback::materializePresentation` 声明在宏块之外；其返回类型 `PresentationCandidateInput`/`HostContext` 在无条件安装的 `presentation_candidate.hpp` | `playback_session.hpp:240-255`；`presentation_candidate.hpp:6-31` |
| 实现编入 | `presentation_materializer.cpp`、`playback_session_execution.cpp`、`playback_session_gameplay.cpp`、`playback_session_realtime.cpp` 等无条件列入 library sources；`gameplay_candidate.cpp` 等只在候选构建追加 | `CMakeLists.txt:13-33`、`:47-54` |
| 默认构建可达错误码 | 无条件的 `materializePresentation` 实现返回 `host.rpa.context_stale` / `host.rpa.device_failed` / `host.rpa.capacity_exceeded`；`playback_session_execution.cpp` 等也无条件出现 `host.rpa.*` | `src/playback_session_prepared.cpp:10-66`；`src/playback_session_execution.cpp`、`src/playback_session_realtime.cpp` 的 `host.rpa.*` 检索 |
| 实验头范围 | 三个候选头只在候选构建安装，但 `PlaybackSession`/`PlaybackSource` 的同一头在默认构建也安装，声明面不同 | 同前两行 |

## 与合同的主要 diff

1. **共享公开类的宏分支**：D-24 要求共享公开类同一定义、候选能力走独立实验头/窄扩展；现状在
   `PlaybackSession`、`PlaybackSource` 两个既有公开类里保留 `#if CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE`
   方法块。默认与候选安装的同一头因此声明不同。
2. **候选入口随默认安装**：`PreparedPlayback::materializePresentation` 与 `PresentationCandidateInput`
   在默认构建可编译、可调用并返回 `host.rpa.*`，不满足“候选读取器只编入候选构建”的隔离方向
   （D-18）。`presentation_materializer.cpp` 与 `materializePresentation` 是当前已存在路径，是否属于
   “候选读取器”需 RR-4 按消费者与安装面逐符号裁定，本卡不预判。
3. **宏的传递面**：候选开关以 `PUBLIC` 定义在 target 上，任何链接 `cuexis::playback` 的消费者在候选
   构建都会看到实验声明；默认构建不会。合同要求实验包显式选用、独立安装，现状没有独立的实验包/导出
   集合。
4. **错误码暴露面**：`host.rpa.*` 已出现在默认构建编译的 Playback 源文件中；这些码是否算“安装面稳定
   码”与是否只在候选入口可达，需 RR-4 用符号/入口审计回答（本卡只做源码检索，不证明运行不可达）。

## 建议的移交验收（RR-4 自行定稿）

- 逐符号列出默认/候选两种安装头的声明差集，区分“共享类方法”“独立实验头”“私有 helper”。
- 对 `materializePresentation` 决定：迁入独立实验头、保留为默认安装并补合同，或以其它已接受方式收敛；
  任何选择都要有安装消费者证据与版本/兼容窗口说明。
- 证明默认构建不编译纯候选读取器、不导出候选符号；证明候选构建的额外安装集合可被独立发现/安装。
- 本清单不改变 `docs/adr/0041`、`docs/adr/0042` 的兼容边界，也不构成候选能力晋升。
