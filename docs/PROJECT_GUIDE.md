# Cuexis Project Guide

状态：现行项目指南

更新日期：2026-08-28

本文是项目入口，不再保存完整路线、格式字段或阶段测试日志。整理前的完整长版快照见
[archive/PROJECT_GUIDE_LEGACY_2026-08-10.md](archive/PROJECT_GUIDE_LEGACY_2026-08-10.md)。

## 1. 项目定位

Cuexis 是基于 C++20、CMake 和版本化内容格式的可嵌入 Playback SDK。正式产品结构：

```text
Cuexis Playback SDK
Cuexis Player
Cuexis Studio
```

SDL3、OpenGL 和 EnTT 是实现或可选 adapter，不是宿主必须采用的公共产品边界。宿主使用
PlaybackSession、ContentProvider、RuntimeFrame 和 FrameSnapshot，不访问 RuntimeSession、World
或后端对象。

当前状态见 [CURRENT_STATUS.md](CURRENT_STATUS.md)，架构见
[architecture/OVERVIEW.md](architecture/OVERVIEW.md)。

## 2. 核心目标

- 加载、验证、迁移和确定性编译 Cuexis Chart。
- 在任意目标时间进行可重复采样，并正确处理 Seek、Stop、reload 和 discontinuity。
- 通过拥有型 FrameSnapshot 和 portable resource 边界输出表现结果。
- 支持 filesystem、memory 和 host ContentProvider。
- 提供 headless、static/shared package 和 external consumer 路径。
- 最终交付 SDK 内的 Input/Judgement/Replay，同时让宿主持有主循环、平台生命周期和游戏 UI。

## 3. 非目标

Cuexis 不建设通用商业游戏引擎、物理/导航/AI/联网框架、完整游戏状态机或宿主任意插件系统。
运行时脚本和逐帧脚本回调无限期延后，不预留格式字段、capability、字节码或执行入口。

## 4. 架构原则

```text
Chart documents are not Runtime state
ChartRuntime does not contain EnTT entities
World does not parse Chart JSON
PlaybackSession is the public host facade
FrameSnapshot is the only public frame
adapters consume portable/public values
load and reload are transactional
time discontinuities rebuild from absolute time
```

模块边界见 [architecture/MODULE_BOUNDARIES.md](architecture/MODULE_BOUNDARIES.md)。

## 5. 内容和格式

Playback 保留 `cuexis.chart` v1/v2/v3 的全部生产路径。Stage Chart Format Update 已接受 CXC v1、
Chart v4 和 CXT v1 合同，并完成 CFU-C0–C4。CFU-D1/D2 已关闭显式 JSON lift 迁移与 CLI
`--target 4`；CFU-E 已关闭公共 API、统一 PlaybackSource、typed/CXC factory、SDK `0.6.0`、
Chart v4 prepare、capability 接入与 `PreparedSemanticIdentity`。静态和参数化 v4 可使用现有
Runtime。CFU-D3 已关闭 Playback FrameSnapshot / FrameDigest v3 /
seek-stop 等价；整包 CFU-D 已由项目所有者记录“未提供外部资产”并关闭，兼容窗口不缩短。
CFU-F 已在最终实现 SHA 上关闭跨平台 consumer、确定性、安全与性能门禁。CFU-G 的 G0 状态校准、
G1 退出审计和 G2 Stage 4 typed handoff 已完成；G3 本地候选门禁已于 2026-08-19 通过。最终候选
SHA `4371fdcf04f4f89bfddf070cbb15e4c903810a53` 的 hosted Linux/MSVC/MinGW 验证已于 2026-08-24
全部通过，G3 hosted 与 G4 已完成。G5 completion report SHA
`6e52677be2f27f83d0b709619be7ffcc141d6f95` 的 hosted revalidation 也已全部通过；项目所有者已于
2026-08-24 明确接受报告，CFU-G 与 Stage Chart Format Update 已关闭。Stage 4 已完成；
最终 SHA `3df92747e575e97b3606aa4a2030a04643c46473` 的 hosted Linux/MSVC/MinGW 已于
2026-08-27 全部通过，项目所有者已接受完成报告。Stage 5 的 S5-A 合同已冻结；S5-B 已接线可选
shader 工具；S5-C 已完成公开 Presentation 类型、kind 4/5 解析与 SDK API `0.7.0`；S5-D 已完成
可选 GLSL 450 编译与 reflection；S5-E 已完成 profile 与 presentation capability 预检；S5-F 已完成
`CXSCCH01` 缓存；S5-G 已完成 OpenGL/Player/Validation Sink 消费与默认 shader/parameterized
capability；S5-H 尚未开始。

格式权威入口：[formats/README.md](formats/README.md)。

```text
Source Project -> explicit validate/pack -> CXC exchange package
Chart/CXT/resources -> prepare -> typed runtime data
typed runtime data -> Runtime/Animation -> FrameSnapshot
```

Pack、prepare 和 Playback 不执行脚本。

## 6. 时间和属性

Chart 保存 Beat，TimingMap 映射到 chartTimeMs。Behavior 和 Animation 均以绝对目标时间采样，
不从上一帧结果累积。属性通过统一 PropertyResolver 提交：

```text
Initial -> Behavior -> Animation -> HostOverride -> StudioPreviewOverride -> commit
```

精确语义见 [TIMING_MODEL.md](formats/TIMING_MODEL.md) 和
[ANIMATION_MIXING.md](formats/ANIMATION_MIXING.md)。

## 7. 工程结构

```text
engine/       libraries and internal modules
app/player/   reference Player
app/studio/   future separate Studio application
tests/        Catch2 and CMake script tests
schemas/      versioned JSON schemas
tools/        validators, migrators and import/build tools
cmake/        project CMake modules and package checks
docs/         architecture, formats, plans, reports and guides
```

新增模块或依赖必须同步更新 CMake target allowlist、架构测试、依赖政策和第三方许可记录。

## 8. 开发工作流

标准构建、测试、格式、package 和 GPU smoke 命令见
[BUILDING.md](guides/BUILDING.md)。编码、异常、Result、线程和公共头规则见
[CODE_POLICY.md](guides/CODE_POLICY.md)。版本和依赖分别见
[VERSIONING.md](guides/VERSIONING.md) 与 [DEPENDENCY_POLICY.md](guides/DEPENDENCY_POLICY.md)。

实现顺序：

1. 先接受 ADR 和字段合同。
2. 再实现 typed model、Schema、Reader/Writer 和 validator。
3. 再接入 Runtime/Playback。
4. 最后取得 headless、external consumer、跨平台和必要 GPU 证据。

不得把候选文档描述为已实现，不得用本地结果替代要求的 hosted 或目标平台证据。

## 9. Definition of Done

一项功能只有在以下内容同时关闭后才能标记完成：

```text
accepted contract
implementation and focused tests
architecture and public-header checks
failure and rollback paths
format/migration compatibility where applicable
external consumer evidence for public SDK changes
required cross-platform and GPU evidence
current status, plan and report alignment
```

## 10. 路线与历史

现行路线见 [ROADMAP.md](ROADMAP.md)。阶段计划和证据分别见
[stage_plans/README.md](stage_plans/README.md) 与
[stage_reports/README.md](stage_reports/README.md)。

历史长版指南、旧路线、Simple Chart 和早期审视材料见 [archive/README.md](archive/README.md)。

## 11. 文档维护

文档角色、状态和权威规则见 [DOCUMENTATION_POLICY.md](DOCUMENTATION_POLICY.md)。任何新的顶层
规范都必须加入 [docs/README.md](README.md) 和对应索引。
