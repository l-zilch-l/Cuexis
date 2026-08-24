# Stage 4 Implementation Plan: Cuexis Presentation Animation

状态：future；未开始，等待 Stage Chart Format Update 关闭

更新日期：2026-08-16

归档来源：[旧版 PROJECT_GUIDE](../archive/PROJECT_GUIDE_LEGACY_2026-08-10.md) 与
[SDK transition plan 快照](../archive/CUEXIS_SDK_TRANSITION_PLAN_2026-08-10.md)。

## 1. 阶段目标

实现 Cuexis 谱面和资源预览所需的确定性表现动画，使 Behavior、Animation、宿主覆盖和 Studio
预览通过统一属性求值路径协作。本阶段不建设通用角色状态机、骨骼动画控制器、游戏对象脚本
系统或运行时脚本。

## 2. 前置条件

- Stage Chart Format Update 的 Chart v4、CXT、Clip、Binding、Property ID、mask 和 lowering 合同关闭。
- Stage 4 只消费格式阶段交付的 typed 数据，不读取 JSON、CXC 或 CXT。
- [ANIMATION_MIXING.md](../formats/ANIMATION_MIXING.md) 与 ADR 0019 的混合和覆盖语义保持权威。
- [CFU-G2 Stage 4 typed handoff](../stage_reports/260816-chart-format-update-g2-stage4-handoff.md)
  已冻结 `AnimationProgramInput`、capability、fixture、预算、diagnostics、所有权和验收入口；Stage 4
  仍等待格式阶段 completion report 经项目所有者接受后解锁。
- [CFU-G4 hosted verification](../stage_reports/260824-chart-format-update-g4-hosted.md)
  已记录同 SHA Linux/MSVC/MinGW 全部成功；completion report 与 owner acceptance 未完成，因此
  本计划状态仍为 future, blocked。

## 3. 实施范围

- 实现 AnimationClip、AnimatorComponent 和 AnimationSystem。
- 复用 Curve、Track 和 Sampler，支持 Transform 与允许的 Material 数值参数动画。
- 支持播放、暂停、循环和绝对目标时间采样。
- 实现 Animation Layer、BlendGroup、weight、priority 和 property mask。
- 实现 HostOverride 与 StudioPreviewOverride Token。
- 通过 PropertyResolver 实现确定性的 Override 和受限 Additive 混合。
- 为动画安全上限、诊断和热路径分配建立测试；应用配置不得改变动画语义。
- 在调试快照中公开属性来源、Layer、权重和最终贡献，且不泄漏 World/EnTT。

## 4. 验收标准

- AnimationSystem 不依赖 Chart 文档、JSON、CXC、CXT、SDL 或图形后端。
- BehaviorSystem 和 AnimationSystem 可以同时作用，并按已冻结顺序提交到 PropertyResolver。
- 冲突属性只通过显式 blend mode、priority 和 mask 处理，不依赖 Entity 遍历顺序。
- 相同 Animation/Animator 数据和相同显式输入在 Seek、Stop、reload 和不同帧率下结果一致。
- HostOverride 或 StudioPreviewOverride 结束后，属性恢复为当前下层求值结果。
- 宿主只通过稳定 ObjectId、PropertyId 和 OverrideToken 操作，不访问 World 或最终 Component。
- warmed update/extract 路径满足阶段冻结的零分配或有界分配要求。
- headless Playback、Player 和 external consumer 对相同动画输入产生相同 FrameSnapshot/digest。

## 5. 明确不包含

- 运行时脚本、逐帧脚本回调、任意表达式执行或通用状态机。
- Shader、粒子、UI、骨骼动画和通用游戏对象生命周期。
- 在 AnimationSystem 内解析或迁移持久化格式。
