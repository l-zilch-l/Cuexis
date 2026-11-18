# Documentation Map

<!-- cuexis-meta
id: INDEX-MAP
status: current
authority: index
-->

状态：current（由 tools/verification/gen_docs_map.py 生成，勿手改）

给 agent 的阅读规则：

1. 先读本页，再只打开与任务相关的 1–3 篇；不要全仓 grep docs/。
2. 事实以代码、`schemas/`、`cmake/CuexisVersion.cmake`、`limits.hpp` 为准；文档中的数值若是复述，以生成物/代码为准。
3. `docs/stage_reports/**` 与 `docs/archive/**` 是**证据**，不是权威；不得据其推断当前实现。
4. `status` 为 `completed`/`historical` 的文档只用于追溯；`candidate`/`future`/`deferred` 不是已实现能力。

当前阶段：**Stage 7-RR**（见 [CURRENT_STATUS.md](CURRENT_STATUS.md)）；当前工作项：[stage_plans/active/stage-07-rr/plan.md](stage_plans/active/stage-07-rr/plan.md)；当前登记册：[stage_plans/active/stage-07-rr/findings.md](stage_plans/active/stage-07-rr/findings.md)。

## 常见变更的阅读路径

| 要改什么 | 先读 |
| --- | --- |
| 公共 API 签名/语义 | [api/README.md](api/README.md) → 对应 api 页 → [ADR 0030](adr/0030-playback-preview-api-version-and-result.md) |
| 格式字段/预算 | [formats/README.md](formats/README.md) → 对应 Spec → [guides/LIMITS.md](guides/LIMITS.md) |
| 格式 Schema/校验 | `schemas/*.schema.json`（如 [cuexis.chart.v4.schema.json](../schemas/cuexis.chart.v4.schema.json)）→ 对应 Spec |
| 依赖/构建 target | [guides/DEPENDENCY_POLICY.md](guides/DEPENDENCY_POLICY.md) → [guides/BUILDING.md](guides/BUILDING.md) |
| 修 bug / 复核差异 | [CURRENT_STATUS.md](CURRENT_STATUS.md) → 相关合同 → 当前登记册（见上方） |

单一事实源：

| 事实 | 权威位置 |
| --- | --- |
| 当前阶段与遗留 | [CURRENT_STATUS.md](CURRENT_STATUS.md) |
| 阶段顺序与依赖 | [ROADMAP.md](ROADMAP.md) |
| 版本（SDK / 日期构建） | `cmake/CuexisVersion.cmake`（机器校验：`vcpkg.json`） |
| 构建 target 与依赖 | [guides/BUILDING.md](guides/BUILDING.md) 的生成块 + CMakeLists |
| 格式预算/上限 | [guides/LIMITS.md](guides/LIMITS.md)（生成自 `engine/chart/include/cuexis/chart/limits.hpp`） |
| 诊断码与类别 | `schemas/cuexis.gameplay-diagnostics.v2.codes.json`（可读视图：[guides/DIAGNOSTIC_CODES.md](guides/DIAGNOSTIC_CODES.md)） |
| 决策 | `docs/adr/` |

## 合同类（权威，按状态排序）

| 文档 | 状态 | 权限 | 标题 |
| --- | --- | --- | --- |
| [adr/README.md](adr/README.md) | active | index | ADR Index |
| [api/GLOSSARY.md](api/GLOSSARY.md) | active | owns | Cuexis RPA 与表现术语 |
| [api/README.md](api/README.md) | active | index | Cuexis API 参考 |
| [api/diagnostics-identity-and-compatibility.md](api/diagnostics-identity-and-compatibility.md) | active | owns | 诊断、身份与兼容性 |
| [api/frames-digests-and-timelines.md](api/frames-digests-and-timelines.md) | active | owns | 帧、摘要与时间线 |
| [api/internal-module-catalog.md](api/internal-module-catalog.md) | active | owns | 内部模块速查 |
| [api/playback-session.md](api/playback-session.md) | active | owns | `PlaybackSession` 生命周期 |
| [api/presentation-and-capabilities.md](api/presentation-and-capabilities.md) | active | owns | 表现资源与能力预检 |
| [api/sources-and-content.md](api/sources-and-content.md) | active | owns | `PlaybackSource` 与 `ContentProvider` |
| [architecture/CHART_REPRESENTATIONS.md](architecture/CHART_REPRESENTATIONS.md) | active | none | Chart 表示与转换点 |
| [architecture/GAMEPLAY_ABSTRACTION_MODEL.md](architecture/GAMEPLAY_ABSTRACTION_MODEL.md) | active | owns | 音乐游戏玩法抽象模型 |
| [architecture/MODULE_BOUNDARIES.md](architecture/MODULE_BOUNDARIES.md) | active | owns | Cuexis Module Boundaries |
| [architecture/OVERVIEW.md](architecture/OVERVIEW.md) | active | owns | Cuexis Architecture Overview |
| [architecture/PLAYER_APPLICATION.md](architecture/PLAYER_APPLICATION.md) | active | owns | Player 应用结构 |
| [architecture/README.md](architecture/README.md) | active | index | Architecture Index |
| [architecture/RUNTIME_SESSION.md](architecture/RUNTIME_SESSION.md) | active | owns | Cuexis RuntimeSession 规范 |
| [formats/CHART_FORMAT.md](formats/CHART_FORMAT.md) | active | owns | Cuexis Chart Format v1/v2/v3 |
| [formats/README.md](formats/README.md) | active | index | Cuexis Format Index |
| [formats/TIMING_MODEL.md](formats/TIMING_MODEL.md) | active | owns | Cuexis TimingMap 规范 |
| [guides/BUILDING.md](guides/BUILDING.md) | active | owns | Building Cuexis |
| [guides/CHART_V4_AUTHORING.md](guides/CHART_V4_AUTHORING.md) | active | owns | Chart v4 谱面编写指南（兼容入口） |
| [guides/COMMIT_AND_PR_CONVENTION.md](guides/COMMIT_AND_PR_CONVENTION.md) | active | owns | 提交与 PR 标题约定 |
| [guides/DIAGNOSTIC_CODES.md](guides/DIAGNOSTIC_CODES.md) | current | references | 诊断码与类别（可读视图） |
| [guides/LIMITS.md](guides/LIMITS.md) | current | references | Chart 与 Packed 预算上限 |
| [guides/PLAYER_OPTIONS.md](guides/PLAYER_OPTIONS.md) | current | references | Player 命令行选项 |
| [guides/README.md](guides/README.md) | active | index | Cuexis Guides |
| [guides/reading-order.md](guides/reading-order.md) | active | owns | Cuexis 阅读顺序 |
| [adr/0001-project-name-cuexis.md](adr/0001-project-name-cuexis.md) | accepted | owns | ADR 0001：项目命名为 Cuexis |
| [adr/0002-use-vcpkg.md](adr/0002-use-vcpkg.md) | accepted | owns | ADR 0002：使用 vcpkg manifest mode |
| [adr/0003-use-cmake-presets.md](adr/0003-use-cmake-presets.md) | accepted | owns | ADR 0003：使用 CMake Presets |
| [adr/0005-use-entt.md](adr/0005-use-entt.md) | accepted | owns | ADR 0005：使用 EnTT 管理运行时 Entity |
| [adr/0006-render-backend-abstraction.md](adr/0006-render-backend-abstraction.md) | accepted | owns | ADR 0006：分离渲染前端与后端 |
| [adr/0007-chart-runtime-world-boundary.md](adr/0007-chart-runtime-world-boundary.md) | accepted | owns | ADR 0007：Chart、Runtime 与 World 的边界 |
| [adr/0008-coordinate-transform-hierarchy.md](adr/0008-coordinate-transform-hierarchy.md) | accepted | owns | ADR 0008：坐标、Transform 与父子层级语义 |
| [adr/0009-property-evaluation-and-conflicts.md](adr/0009-property-evaluation-and-conflicts.md) | accepted | owns | ADR 0009：Behavior 与 Animation 的属性求值和冲突规则 |
| [adr/0011-use-catch2-and-ctest.md](adr/0011-use-catch2-and-ctest.md) | accepted | owns | ADR 0011：使用 Catch2 v3 和 CTest |
| [adr/0012-use-sdl3-audio-backend.md](adr/0012-use-sdl3-audio-backend.md) | accepted | owns | ADR 0012：使用 SDL3 音频后端和采样帧时钟 |
| [adr/0013-resource-handle-lease-and-scope.md](adr/0013-resource-handle-lease-and-scope.md) | accepted | owns | ADR 0013：资源 Handle、Lease 与 Scope 生命周期 |
| [adr/0014-unified-chart-object-schema.md](adr/0014-unified-chart-object-schema.md) | accepted | owns | ADR 0014：统一 Chart Object 与 Component Schema |
| [adr/0016-use-apache-2-license.md](adr/0016-use-apache-2-license.md) | accepted | owns | ADR 0016：Cuexis 使用 Apache License 2.0 |
| [adr/0017-transactional-runtime-session.md](adr/0017-transactional-runtime-session.md) | accepted | owns | ADR 0017：事务式 RuntimeSession 生命周期 |
| [adr/0018-timing-map-semantics.md](adr/0018-timing-map-semantics.md) | accepted | owns | ADR 0018：TimingMap 的 BPM、Stop 与 Offset 语义 |
| [adr/0019-animation-layers-and-runtime-overrides.md](adr/0019-animation-layers-and-runtime-overrides.md) | accepted | owns | ADR 0019：动画 Layer 与 Runtime Override |
| [adr/0020-deterministic-particle-timeline.md](adr/0020-deterministic-particle-timeline.md) | accepted | owns | ADR 0020：确定性粒子时间轴与 Seek |
| [adr/0021-spirv-centered-shader-pipeline.md](adr/0021-spirv-centered-shader-pipeline.md) | accepted | owns | ADR 0021：以 SPIR-V 为中心的 Shader 管线 |
| [adr/0022-android-resource-and-input-strategy.md](adr/0022-android-resource-and-input-strategy.md) | accepted | owns | ADR 0022：Android 资源与输入延迟策略 |
| [adr/0023-code-error-and-thread-policy.md](adr/0023-code-error-and-thread-policy.md) | accepted | owns | ADR 0023：编码、错误与线程政策 |
| [adr/0024-configuration-ownership-and-staged-formats.md](adr/0024-configuration-ownership-and-staged-formats.md) | accepted | owns | ADR 0024：配置所有权与格式分阶段冻结 |
| [adr/0025-project-config-v1-and-path-security.md](adr/0025-project-config-v1-and-path-security.md) | accepted | owns | ADR 0025：ProjectConfig v1 与项目路径安全 |
| [adr/0026-asset-index-and-source-resolution.md](adr/0026-asset-index-and-source-resolution.md) | accepted | owns | ADR 0026：Asset Index 与来源解析 |
| [adr/0027-playback-sdk-product-boundary.md](adr/0027-playback-sdk-product-boundary.md) | accepted | owns | ADR 0027：Cuexis Playback SDK 产品边界与宿主职责 |
| [adr/0028-camera-projection-and-events.md](adr/0028-camera-projection-and-events.md) | accepted | owns | ADR 0028：相机投影模型、ECS 组件归属与 FrameSnapshot 契约 |
| [adr/0029-behavior-keyframe-and-runtime-frame.md](adr/0029-behavior-keyframe-and-runtime-frame.md) | accepted | owns | ADR 0029：Behavior Keyframe v1、绝对时间采样与 PlaybackSession 帧契约 |
| [adr/0030-playback-preview-api-version-and-result.md](adr/0030-playback-preview-api-version-and-result.md) | accepted | owns | ADR 0030: Playback Preview API Version and Result Boundary |
| [adr/0031-main-music-content-format-v2.md](adr/0031-main-music-content-format-v2.md) | accepted | owns | ADR 0031: Main Music Content in Chart and Asset Index v2 |
| [adr/0032-playback-clock-and-prepared-audio-transaction.md](adr/0032-playback-clock-and-prepared-audio-transaction.md) | accepted | owns | ADR 0032: Playback Clock and Prepared Audio Transaction |
| [adr/0033-cpp-shared-library-preview-boundary.md](adr/0033-cpp-shared-library-preview-boundary.md) | accepted | owns | ADR 0033: C++ Shared Library Preview Boundary |
| [adr/0034-chart-v3-tempo-and-behavior-events.md](adr/0034-chart-v3-tempo-and-behavior-events.md) | accepted | owns | ADR 0034：Chart v3 Tempo Event 与 Behavior Event |
| [adr/0035-retire-simple-chart-format.md](adr/0035-retire-simple-chart-format.md) | accepted | owns | ADR 0035：阶段 2A 移除 Simple Chart 格式 |
| [adr/0036-stage-2-runtime-contracts.md](adr/0036-stage-2-runtime-contracts.md) | accepted | owns | ADR 0036：Stage 2 Timing、Step Event 与可观察表现合同 |
| [adr/0037-stage-3-portable-presentation-contracts.md](adr/0037-stage-3-portable-presentation-contracts.md) | accepted | owns | ADR 0037：Stage 3 可移植表现与渲染适配合同 |
| [adr/0038-cxc-v1-and-chart-v4-boundary.md](adr/0038-cxc-v1-and-chart-v4-boundary.md) | accepted | owns | ADR 0038：CXC v1、Chart v4 与 CXT v1 动画序列化边界 |
| [adr/0039-remove-hour-from-build-version.md](adr/0039-remove-hour-from-build-version.md) | accepted | owns | ADR 0039：从构建版本移除小时字段 |
| [adr/0040-stage-5-material-shader-contracts.md](adr/0040-stage-5-material-shader-contracts.md) | accepted | owns | ADR 0040：Stage 5 Material、Shader 与能力分层合同 |
| [adr/0041-legacy-format-exit-policy.md](adr/0041-legacy-format-exit-policy.md) | accepted | owns | ADR 0041: Legacy Format Exit Policy |
| [adr/0042-stage-6-productization-boundaries.md](adr/0042-stage-6-productization-boundaries.md) | accepted | owns | ADR 0042：Stage 6 产品化的实施边界与长期维护决策 |
| [adr/0046-realtime-playback-coordination.md](adr/0046-realtime-playback-coordination.md) | accepted | owns | ADR 0046: 实时播放的时间、执行与发布边界 |
| [adr/0047-pd-feedback-lifetime-and-delivery-retirement.md](adr/0047-pd-feedback-lifetime-and-delivery-retirement.md) | accepted | owns | ADR 0047: PD 反馈寿命、交付许可与回收边界 |
| [formats/ANIMATION_MIXING.md](formats/ANIMATION_MIXING.md) | accepted | owns | Cuexis Animation Mixing 规范 |
| [formats/CHART_V4_FORMAT.md](formats/CHART_V4_FORMAT.md) | accepted | owns | Cuexis Chart Format v4 |
| [formats/CXC_FORMAT.md](formats/CXC_FORMAT.md) | accepted | owns | Cuexis CXC v1 |
| [formats/CXT_FORMAT.md](formats/CXT_FORMAT.md) | accepted | owns | Cuexis Animation Template (CXT) v1 |
| [formats/PORTABLE_PRESENTATION.md](formats/PORTABLE_PRESENTATION.md) | accepted | owns | Cuexis Portable Presentation Profile v1 |
| [guides/CODE_POLICY.md](guides/CODE_POLICY.md) | accepted | owns | Cuexis 编码、错误与线程政策 |
| [guides/DEPENDENCY_POLICY.md](guides/DEPENDENCY_POLICY.md) | accepted | owns | Cuexis 开源与第三方依赖政策 |
| [guides/VERSIONING.md](guides/VERSIONING.md) | accepted | owns | Cuexis Versioning |
| [adr/0044-gameplay-v2-semantic-kernel.md](adr/0044-gameplay-v2-semantic-kernel.md) | candidate | owns | ADR 0044：Gameplay V2 语义内核与冻结边界 |
| [adr/0045-gameplay-v2-execution-profile.md](adr/0045-gameplay-v2-execution-profile.md) | candidate | owns | ADR 0045: Gameplay V2 execution profile for S7A-3 / S7A-4 |
| [api/GAMEPLAY_V2_ABI.md](api/GAMEPLAY_V2_ABI.md) | candidate | owns | Gameplay V2 ABI（候选）：内部 typed preview 边界 |
| [api/execution-coordination.md](api/execution-coordination.md) | candidate | owns | Execution coordination preview |
| [api/gameplay-v2-execution-types.md](api/gameplay-v2-execution-types.md) | candidate | owns | Gameplay V2 execution typed boundary supplement |
| [api/realtime-host-boundary.md](api/realtime-host-boundary.md) | candidate | owns | 实时宿主边界合同 |
| [architecture/execution-state-and-recovery.md](architecture/execution-state-and-recovery.md) | candidate | owns | 执行状态、恢复与准备路径 |
| [formats/CXT_V2_FORMAT.md](formats/CXT_V2_FORMAT.md) | candidate | owns | Cuexis Chart Template (CXT) v2 |
| [formats/GAMEPLAY_CAPSULE_V2_FORMAT.md](formats/GAMEPLAY_CAPSULE_V2_FORMAT.md) | candidate | owns | Gameplay Capsule v2 candidate format |
| [formats/GAMEPLAY_GRAPH_V1_FORMAT.md](formats/GAMEPLAY_GRAPH_V1_FORMAT.md) | candidate | owns | Compiled Gameplay Graph JSON candidate v1 |
| [formats/GAMEPLAY_V2_SPEC.md](formats/GAMEPLAY_V2_SPEC.md) | candidate | owns | Gameplay V2 字段与运行语义规范 |
| [formats/PACKED_CHART_FORMAT.md](formats/PACKED_CHART_FORMAT.md) | candidate | owns | Cuexis Packed Chart v1 |
| [formats/execution-recovery-wire-v3.md](formats/execution-recovery-wire-v3.md) | candidate | owns | Execution Recovery revision 3 wire |
| [formats/gameplay-v2-author-profile.md](formats/gameplay-v2-author-profile.md) | candidate | owns | Gameplay V2 candidate author profile |
| [formats/gameplay-v2-execution-profile.md](formats/gameplay-v2-execution-profile.md) | candidate | owns | Gameplay V2 S7A-3 / S7A-4 execution profile |
| [formats/rpa-execution-contract.md](formats/rpa-execution-contract.md) | candidate | owns | RPA 执行与控制补充合同 |
| [guides/CHART_V5_AUTHORING.md](guides/CHART_V5_AUTHORING.md) | candidate | owns | Chart v5 谱面编写指南 |
| [architecture/STAGE6_PRODUCTIZATION_BOUNDARIES.md](architecture/STAGE6_PRODUCTIZATION_BOUNDARIES.md) | completed | owns | Stage 6 Productization Boundaries |
| [formats/CHART_ENTRY_V1_FORMAT.md](formats/CHART_ENTRY_V1_FORMAT.md) | completed | owns | Cuexis Chart Entry Extension v1 |
| [formats/MATERIAL_SHADER.md](formats/MATERIAL_SHADER.md) | completed | owns | Cuexis Material and Shader Profile v1 |
| [formats/STAGE6_CONFIG_AND_MEDIA.md](formats/STAGE6_CONFIG_AND_MEDIA.md) | completed | owns | Stage 6 Configuration And Media Contract |
| [adr/0004-version-format-yy-mm-dd-hh.md](adr/0004-version-format-yy-mm-dd-hh.md) | historical | owns | ADR 0004：使用时间型版本号 |
| [adr/0010-canonical-and-simple-chart-formats.md](adr/0010-canonical-and-simple-chart-formats.md) | historical | owns | ADR 0010：规范谱面格式与简易导入格式 |
| [adr/0015-simple-chart-format.md](adr/0015-simple-chart-format.md) | historical | owns | ADR 0015：方案 B 简易谱面格式 |
| [adr/0043-gameplay-judgement-ruleset-convergence.md](adr/0043-gameplay-judgement-ruleset-convergence.md) | historical | owns | ADR 0043：Gameplay Judgement 与 Ruleset 收敛边界 |
| [api/GAMEPLAY_JUDGEMENT_ABI.md](api/GAMEPLAY_JUDGEMENT_ABI.md) | historical | owns | Gameplay Judgement ABI（候选） |
| [formats/GAMEPLAY_JUDGEMENT_SPEC.md](formats/GAMEPLAY_JUDGEMENT_SPEC.md) | historical | owns | Gameplay Judgement Spec |

## 计划类

| 文档 | 状态 | 权限 | 标题 |
| --- | --- | --- | --- |
| [stage_plans/README.md](stage_plans/README.md) | active | index | Stage Plan Index |
| [stage_plans/active/chart-format-update-for-v5/plan.md](stage_plans/active/chart-format-update-for-v5/plan.md) | active | owns | chart-format-update-for-v5：Chart v5 谱面格式与采样合同 |
| [stage_plans/active/stage-07-5/README.md](stage_plans/active/stage-07-5/README.md) | active | index | Stage 7.5: Gameplay Evolution Plans |
| [stage_plans/active/stage-07-5/plan.md](stage_plans/active/stage-07-5/plan.md) | active | owns | Stage 7.5 Plan: Gameplay Evolution |
| [stage_plans/active/stage-07-rr/README.md](stage_plans/active/stage-07-rr/README.md) | active | index | Stage 7-RR：Review and Refactoring After Stage-7 |
| [stage_plans/active/stage-07-rr/architecture-design.md](stage_plans/active/stage-07-rr/architecture-design.md) | active | none | Stage 7-RR 架构设计附页：Player 实时执行与普通播放收敛 |
| [stage_plans/active/stage-07-rr/findings.md](stage_plans/active/stage-07-rr/findings.md) | active | owns | Stage 7-RR 发现登记册（2026-11-17） |
| [stage_plans/active/stage-07-rr/plan.md](stage_plans/active/stage-07-rr/plan.md) | active | owns | Stage 7-RR Plan: Review and Refactoring After Stage-7 |
| [stage_plans/completed/stage-07/04-pd/pd-5/pd5-read-candidate-and-memory-domains-plan.md](stage_plans/completed/stage-07/04-pd/pd-5/pd5-read-candidate-and-memory-domains-plan.md) | accepted | owns | PD-5 读取候选与分域内存观测实施方案 |
| [stage_plans/completed/stage-07/05-rpa-new/r4/caps-proposal.md](stage_plans/completed/stage-07/05-rpa-new/r4/caps-proposal.md) | accepted | owns | R4 O1 预算与生产参数裁定包 |
| [stage_plans/completed/stage-07/05-rpa-new/r4/decision-proposal.md](stage_plans/completed/stage-07/05-rpa-new/r4/decision-proposal.md) | accepted | owns | R4 独立推进与 owner 裁定方案 |
| [stage_plans/completed/stage-07/05-rpa-new/r4/formal-registration.md](stage_plans/completed/stage-07/05-rpa-new/r4/formal-registration.md) | accepted | owns | R4 固定取证登记与剩余预注册 |
| [stage_plans/completed/stage-07/04-pd/pd-5/pd5-read-result-candidate.md](stage_plans/completed/stage-07/04-pd/pd-5/pd5-read-result-candidate.md) | candidate | owns | PD-5 C2: caller-owned neutral read result candidate |
| [stage_plans/completed/stage-07/05-rpa-new/r3/trusted-restore-candidate.md](stage_plans/completed/stage-07/05-rpa-new/r3/trusted-restore-candidate.md) | candidate | owns | R3b explicit trusted restore candidate |
| [stage_plans/future/realtime-playback-foundation/plan.md](stage_plans/future/realtime-playback-foundation/plan.md) | future | owns | Stage 7-RPA 计划迁移入口 |
| [stage_plans/future/stage-08/plan.md](stage_plans/future/stage-08/plan.md) | future | none | Stage 8 计划（提纲）：Chart v5 正式发行与语义收敛 |
| [stage_plans/future/stage-09/plan.md](stage_plans/future/stage-09/plan.md) | future | none | Stage 9 计划（提纲）：Presentation Foundation 与 Chart v6 |
| [stage_plans/future/stage-10/plan.md](stage_plans/future/stage-10/plan.md) | future | none | Stage 10 计划（提纲）：Studio 与创作/发行工作流 |
| [stage_plans/future/stage-11/plan.md](stage_plans/future/stage-11/plan.md) | future | none | Stage 11 计划（提纲）：Chart v7 几何表现 |
| [stage_plans/future/stage-12/plan.md](stage_plans/future/stage-12/plan.md) | future | none | Stage 12 计划（提纲）：规模化、桌面性能与目标平台 |
| [stage_plans/future/stage-13/plan.md](stage_plans/future/stage-13/plan.md) | future | none | Stage 13 计划（提纲）：高级表现与 Chart v8 |
| [stage_plans/future/stage-14/plan.md](stage_plans/future/stage-14/plan.md) | future | none | Stage 14 计划（提纲）：稳定 ABI 与 Playback SDK v1 |
| [stage_plans/completed/260830-followup/plan.md](stage_plans/completed/260830-followup/plan.md) | completed | owns | 260830-followup 维护计划 |
| [stage_plans/completed/260830-followup/task-2-chart-cxc-parse-once.md](stage_plans/completed/260830-followup/task-2-chart-cxc-parse-once.md) | completed | owns | 任务 2：Chart/CXC parse-once 实施计划 |
| [stage_plans/completed/260830-followup/task-3-branch-backlog.md](stage_plans/completed/260830-followup/task-3-branch-backlog.md) | completed | owns | 任务 3 分支 backlog |
| [stage_plans/completed/260830-followup/task-3-critical-branch-coverage.md](stage_plans/completed/260830-followup/task-3-critical-branch-coverage.md) | completed | owns | 任务 3：关键模块分支覆盖率实施计划 |
| [stage_plans/completed/chart-format-foundation-hardening/plan.md](stage_plans/completed/chart-format-foundation-hardening/plan.md) | completed | owns | Chart Format Foundation Hardening：交接加固实施计划 |
| [stage_plans/completed/chart-format-foundation/plan.md](stage_plans/completed/chart-format-foundation/plan.md) | completed | owns | Chart Format Foundation：Chart v5 前置基础 |
| [stage_plans/completed/chart-format-update/plan.md](stage_plans/completed/chart-format-update/plan.md) | completed | owns | Stage Chart Format Update：谱面格式更新实施计划 |
| [stage_plans/completed/stage-01/stage-1b-plan.md](stage_plans/completed/stage-01/stage-1b-plan.md) | completed | owns | 阶段 1B 实施计划：资源生命周期闭环 |
| [stage_plans/completed/stage-01/stage-1c-plan.md](stage_plans/completed/stage-01/stage-1c-plan.md) | completed | owns | 阶段 1C 实施计划：时间、基础行为与 Headless Playback 闭环 |
| [stage_plans/completed/stage-01/stage-1d-plan.md](stage_plans/completed/stage-01/stage-1d-plan.md) | completed | owns | 阶段 1D 实施计划：主音乐内容与可选音频适配器闭环 |
| [stage_plans/completed/stage-01/stage-1e-plan.md](stage_plans/completed/stage-01/stage-1e-plan.md) | completed | owns | 阶段 1E 实施计划：SDK 封装与外部消费闭环 |
| [stage_plans/completed/stage-02/plan.md](stage_plans/completed/stage-02/plan.md) | completed | owns | 阶段 2：Behavior Event 与 Cuexis 表现能力实施计划 |
| [stage_plans/completed/stage-03/plan.md](stage_plans/completed/stage-03/plan.md) | completed | owns | 阶段 3：可移植表现前端与渲染适配实施计划 |
| [stage_plans/completed/stage-04/plan.md](stage_plans/completed/stage-04/plan.md) | completed | owns | Stage 4 Implementation Plan: Cuexis Presentation Animation |
| [stage_plans/completed/stage-05/plan.md](stage_plans/completed/stage-05/plan.md) | completed | owns | Stage 5 Implementation Plan: Material and Shader Pipeline |
| [stage_plans/completed/stage-07/01-a/README.md](stage_plans/completed/stage-07/01-a/README.md) | completed | index | Stage 7A Plans |
| [stage_plans/completed/stage-07/01-a/plan.md](stage_plans/completed/stage-07/01-a/plan.md) | completed | owns | Stage 7A Plan: Gameplay Kernel and Product Integration |
| [stage_plans/completed/stage-07/02-diagnostics/README.md](stage_plans/completed/stage-07/02-diagnostics/README.md) | completed | index | Diagnostics Plans |
| [stage_plans/completed/stage-07/02-diagnostics/plan.md](stage_plans/completed/stage-07/02-diagnostics/plan.md) | completed | owns | Diagnostic Formalization Prerequisite Plan: N0–N3 |
| [stage_plans/completed/stage-07/03-rpa-legacy/README.md](stage_plans/completed/stage-07/03-rpa-legacy/README.md) | completed | index | Legacy RPA Plans |
| [stage_plans/completed/stage-07/04-pd/README.md](stage_plans/completed/stage-07/04-pd/README.md) | completed | index | Presentation Decoupling Plans |
| [stage_plans/completed/stage-07/05-rpa-new/README.md](stage_plans/completed/stage-07/05-rpa-new/README.md) | completed | index | New RPA Plans |
| [stage_plans/completed/stage-07/05-rpa-new/r3/plan.md](stage_plans/completed/stage-07/05-rpa-new/r3/plan.md) | completed | owns | R3 implementation and verification |
| [stage_plans/completed/stage-07/05-rpa-new/r4/plan.md](stage_plans/completed/stage-07/05-rpa-new/r4/plan.md) | completed | owns | Cuexis Stage 7-RPA R4 实施与验证方案 |
| [stage_plans/completed/stage-07/README.md](stage_plans/completed/stage-07/README.md) | completed | index | Stage 7 Plans and Materials |
| [stage_plans/completed/stage-07/plan.md](stage_plans/completed/stage-07/plan.md) | completed | owns | Stage 7 Implementation Plan: Gameplay Foundation and Realtime Playback |
| [stage_plans/reviews/full-review-2026-08/remediation-plan.md](stage_plans/reviews/full-review-2026-08/remediation-plan.md) | completed | owns | 260829 Full Review 整改实施计划 |
| [stage_plans/reviews/stage-06-review-remediation/R9-reference-host-command-loop.md](stage_plans/reviews/stage-06-review-remediation/R9-reference-host-command-loop.md) | completed | owns | R9：Reference Host 命令循环与 play/pause（实施文档） |
| [stage_plans/reviews/stage-06-review-remediation/plan.md](stage_plans/reviews/stage-06-review-remediation/plan.md) | completed | owns | stage-06-review-remediation：Stage 6 复核发现项修正计划 |
| [stage_plans/completed/stage-06/plan.md](stage_plans/completed/stage-06/plan.md) | historical | owns | Stage 6 Implementation Plan: Playback C++ API and Player Productization |
| [stage_plans/completed/stage-07/03-rpa-legacy/performance-modification-plan.md](stage_plans/completed/stage-07/03-rpa-legacy/performance-modification-plan.md) | historical | owns | Stage 7 RPA Performance Modification Plan 归档入口 |
| [stage_plans/completed/stage-07/03-rpa-legacy/plan.md](stage_plans/completed/stage-07/03-rpa-legacy/plan.md) | historical | owns | Stage 7-RPA 计划归档入口 |
| [stage_plans/completed/stage-07/04-pd/design.md](stage_plans/completed/stage-07/04-pd/design.md) | historical | owns | Stage 7-PD 内部设计与验收细则 |
| [stage_plans/completed/stage-07/04-pd/pd-5/pd5-gameplay-admission.md](stage_plans/completed/stage-07/04-pd/pd-5/pd5-gameplay-admission.md) | historical | owns | PD-5 40k Gameplay-host 准入：阶段 A 实施登记 |
| [stage_plans/completed/stage-07/04-pd/pd-5/pd5-measurement-proposal.md](stage_plans/completed/stage-07/04-pd/pd-5/pd5-measurement-proposal.md) | historical | owns | PD-5 measurement and threshold proposal |
| [stage_plans/completed/stage-07/04-pd/plan.md](stage_plans/completed/stage-07/04-pd/plan.md) | historical | owns | Stage 7-PD: Judgement/Presentation Decoupling Plan |
| [stage_plans/completed/stage-07/05-rpa-new/plan.md](stage_plans/completed/stage-07/05-rpa-new/plan.md) | historical | owns | Cuexis Stage 7-RPA 性能整改方案 |
| [stage_plans/completed/stage-07/05-rpa-new/r1/plan.md](stage_plans/completed/stage-07/05-rpa-new/r1/plan.md) | historical | owns | Stage 7-RPA R1 实施与验证计划 |
| [stage_plans/completed/stage-07/06-b/README.md](stage_plans/completed/stage-07/06-b/README.md) | historical | index | Stage 7B+ Plans: Compatibility Entry |
| [stage_plans/completed/stage-07/06-b/plan.md](stage_plans/completed/stage-07/06-b/plan.md) | historical | owns | Stage 7B+ Plan: Compatibility Entry |
| [stage_plans/completed/stage-07/legacy-paths.md](stage_plans/completed/stage-07/legacy-paths.md) | historical | owns | Stage 7 Legacy Planning Paths |
| [stage_plans/historical-inputs/chart-format-update-for-v6/plan.md](stage_plans/historical-inputs/chart-format-update-for-v6/plan.md) | historical | owns | chart-format-update-for-v6：Chart v6 模型、内置网格与 submesh 合同 |
| [stage_plans/historical-inputs/chart-format-update-for-v7/plan.md](stage_plans/historical-inputs/chart-format-update-for-v7/plan.md) | historical | owns | chart-format-update-for-v7：Chart v7 曲线形变、line 模型与后处理接口 |
| [stage_plans/historical-inputs/chart-format-update-for-v8/plan.md](stage_plans/historical-inputs/chart-format-update-for-v8/plan.md) | historical | owns | chart-format-update-for-v8：Chart v8 双轴贝塞尔形变、模型动画与内置后处理 |
| [stage_plans/historical-inputs/stage-09a/plan.md](stage_plans/historical-inputs/stage-09a/plan.md) | historical | owns | Stage 9A Implementation Plan: SDK and Host Performance Validation |
| [stage_plans/historical-inputs/stage-09b/plan.md](stage_plans/historical-inputs/stage-09b/plan.md) | historical | owns | Stage 9B Historical Input: Android SDK and Host Adapter Validation |
| [stage_plans/historical-inputs/stage-10/plan.md](stage_plans/historical-inputs/stage-10/plan.md) | historical | owns | Historical Input: Optional Vulkan Adapter Validation |
| [stage_plans/historical/sdk-transition/plan.md](stage_plans/historical/sdk-transition/plan.md) | historical | owns | Cuexis SDK Transition Plan |
| [stage_plans/legacy-paths.md](stage_plans/legacy-paths.md) | historical | owns | Legacy Stage Plan Paths |
| [stage_plans/reviews/stage-06-review-remediation/R9-gate-policy-audit-decisions.md](stage_plans/reviews/stage-06-review-remediation/R9-gate-policy-audit-decisions.md) | historical | owns | R9 门禁策略审计 — 裁定与实施依据 |
| [stage_plans/reviews/stage-06-review-remediation/R9-gate-policy-impact-table.md](stage_plans/reviews/stage-06-review-remediation/R9-gate-policy-impact-table.md) | historical | owns | R9 gate policy impact table |

## 证据与历史（非权威）

- [stage_reports](stage_reports/README.md)：带日期的实施/验证证据；只用于追溯与核对结论。
- [archive](archive/README.md)：被取代的历史文档。
- [proposals](proposals/README.md)：候选设计输入，不是合同。
- [examples](examples/README.md)：评审与验证用的样例，不是合同。

## 顶层

| 文档 | 状态 | 权限 | 标题 |
| --- | --- | --- | --- |
| [HANDOFF.md](HANDOFF.md) | active | owns | 当前交接 |
| [PROJECT_GUIDE.md](PROJECT_GUIDE.md) | active | owns | Cuexis Project Guide |
| [README.md](README.md) | active | index | Cuexis Documentation |
| [ROADMAP.md](ROADMAP.md) | active | owns | Cuexis Roadmap |
| [DOCUMENTATION_POLICY.md](DOCUMENTATION_POLICY.md) | accepted | owns | Cuexis Documentation Policy |
| [legacy-paths.md](legacy-paths.md) | historical | index | Legacy Documentation Paths |
