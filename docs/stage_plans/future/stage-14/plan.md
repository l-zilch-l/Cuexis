# Stage 14 计划（提纲）：稳定 ABI 与 Playback SDK v1

状态：future；未启动，仅为方向提纲

更新日期：2026-11-17

<!-- cuexis-meta
id: PLAN-future.stage-14.plan
status: future
authority: none
-->

本文件是**未启动阶段的提纲**，不是执行计划。启动本阶段时，用
[plan.md](../../../templates/plan.md)（详细规划协议）重写，并在变更记录里注明替换关系。

归档来源：原 Stage 12 稳定 ABI 计划的重新编号版本，旧路径映射见
[legacy-paths](../../legacy-paths.md)；阶段位置见 [ROADMAP](../../../ROADMAP.md) 与
[Stage 13](../stage-13/plan.md) 的可选能力边界。

## 阶段目标

候选，未承诺：在 C++ preview、Gameplay/Replay 生命周期、发行格式和目标平台证据稳定后，冻结
稳定 C ABI，提供薄 C++ RAII wrapper，并发布正式 Cuexis Playback SDK v1。

## 范围与边界

- 稳定 C ABI 的唯一归属阶段是 Stage 14；Stage 7A/7.5/8 只交付 C++ typed preview（见
  [ROADMAP](../../../ROADMAP.md)）。
- 前置：Stage 7A Input/Judgement/Replay 公共生命周期、external consumer 与确定性回放门禁完成；
  Stage 8 的 v5 发行合同完成；[Stage 9](../stage-09/plan.md)、[Stage 11](../stage-11/plan.md) 与
  [Stage 12](../stage-12/plan.md) 交付的 Presentation、性能和目标平台矩阵已被接受。
- 公共 FrameSnapshot、JudgementResult、ReplayData、ContentProvider 和错误生命周期无开放语义问题。
- 本阶段不增加第二套 Playback、Judgement、Replay 或 Presentation 语义，也不必等待 Stage 13 全部
  可选能力完成。

## 入口条件

- Stage 7A 公共生命周期、external consumer 与确定性回放门禁完成。
- Stage 8 Chart v5 正式发行合同完成，纳入 v1 的 Stage 7.5 capability 已明确。
- Stage 9、Stage 11、Stage 12 的 Presentation、性能和目标平台矩阵已被接受。
- 公共 FrameSnapshot、JudgementResult、ReplayData、ContentProvider 与错误生命周期无开放语义问题。
- 若纳入 Stage 13 的高级表现能力，其 capability 与预算合同已接受。

## 已确认约束

- 稳定 C ABI 的唯一归属是 Stage 14；Stage 7A/7.5/8 只交付 C++ typed preview。
- 不增加第二套 Playback、Judgement、Replay 或 Presentation 语义。
- C consumer 不依赖 C++ 标准库、异常、RTTI 或第三方实现类型；C++ wrapper 与 C consumer 对相同
  输入产生相同 FrameSnapshot/Judgement/Replay 结果。
- 若采用 PD 基线，沿 [PD 细则 §12.7](../../completed/stage-07/04-pd/design.md) 核对跨 Close
  owning 结果、取消终态和释放责任。内部 shared_ptr、optional、span 和借用读桥不成为 C ABI
  布局；opaque handle 的 allocator、有效期、释放线程、CRT/模块卸载限制必须独立接受，不能由
  引用计数推断安全。
- 若 v1 纳入未来重建任务，须补任务 handle/取消/关闭矩阵；未纳入则明确不支持，不要求本阶段等待
  全部 Stage 13 可选能力。

## 候选方向（非承诺）

启动时可能按以下工作线细化，均未承诺：

- ABI 合同：opaque handle、allocator、字符串、数组、回调、快照有效期、线程 owner、重入、取消和
  释放合同。
- 版本与兼容：独立 C ABI version、符号可见性、capability 查询、兼容和弃用政策。
- 平台矩阵：Windows CRT、Debug/Release、static/shared 和支持平台矩阵。
- 宿主与消费者：至少一个正式宿主 adapter、纯 C external consumer，以及安装、升级、部署和许可证
  文档。
- 薄 C++ RAII wrapper，与 C ABI 共享同一语义实现。

## 待决问题（启动前必须回答）

1. `1.x` 版本政策由哪份 ADR 接受？由 ABI owner 与 owner 决定。
2. 实际发布版本 `1.0.0` 与同 SHA 证据何时批准？由 owner 在发行前决定。
3. v1 是否纳入 Stage 13 的重建任务 handle/取消/关闭矩阵？由 owner 在启动时决定。
4. 支持平台矩阵（Windows CRT、Debug/Release、static/shared 及具体平台）的最终范围由谁冻结？
5. 许可证、部署与升级文档由谁批准并作为发行证据？

## 验收标准

启动时定义。当前只写方向性判据：

- C consumer 不依赖 C++ 标准库、异常、RTTI 或第三方实现类型。
- 正反例覆盖对象、错误、回调、快照、版本拒绝和二进制兼容。
- C++ wrapper 与 C consumer 对相同输入产生相同 FrameSnapshot/Judgement/Replay 结果。
- `1.x` 版本政策有接受的 ADR；实际 `1.0.0`、同 SHA 证据和 owner acceptance 完整。
- 至少一个正式宿主 adapter 与纯 C external consumer 可作为消费证据。

具体矩阵、命令、数值与证据形式在启动时由 owner 与规划模型制定，本提纲不承诺。

## 明确不包含

- 宿主插件 ABI、编辑器 ABI 或渲染后端 ABI，除非另有 ADR。
- 暴露 RuntimeSession、World、EnTT、SDL、OpenGL、JSON DOM 或宿主引擎类型。
