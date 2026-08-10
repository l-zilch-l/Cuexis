# Stage 8 Implementation Plan: Optional Deterministic Particles

状态：future；可选，未开始

更新日期：2026-08-10

归档来源：[旧版 PROJECT_GUIDE](../archive/PROJECT_GUIDE_LEGACY_2026-08-10.md) 与
[SDK transition plan 快照](../archive/CUEXIS_SDK_TRANSITION_PLAN_2026-08-10.md)。设计输入见
[Particle Timeline proposal](../proposals/deferred/PARTICLE_TIMELINE.md) 和 ADR 0020。

## 1. 阶段目标

在材质、Shader 与 Studio 基础上，实现可编辑、可确定性恢复且可由宿主消费的 Cuexis CPU
粒子表现扩展。本阶段不建设通用游戏粒子引擎。

## 2. Stage 8A：基础发射器与表现输出

- 定义 ParticleEmitterAsset、ParticleEmitterComponent 和 ParticleSystem。
- 定义 FrameSnapshot 的版本化粒子表现扩展。
- 实现 CPU 粒子、Billboard、生命周期颜色/大小、速度和重力参数。
- 内建 renderer 作为参考 adapter，粒子核心不依赖 OpenGL。

## 3. Stage 8B：确定性时间轴

- 实现 120 Hz 固定步长和版本化随机种子。
- 实现 Checkpoint、LRU、正向重放 Seek 和 Rebuilding 状态。
- 暂停、向后 Seek、Audio Seek 和大幅 discontinuity 从绝对目标时间恢复。
- Checkpoint 存在与布局不得改变目标时刻结果。

## 4. Stage 8C：编辑、调试与预算

- 实现 Studio 发射器编辑和粒子调试面板。
- 记录粒子数量、Checkpoint 内存和重建耗时。
- 定义 Checkpoint、单帧重建和资源预算的 typed config 与稳定诊断。
- 默认预算等待 Stage 9A 测量，不在本计划中写死设备常数。

## 5. 验收标准

- Entity 可以挂载并编辑粒子发射器，粒子正确跟随父级 Transform。
- 粒子通过 FrameSnapshot 扩展输出，不直接依赖 OpenGL、SDL 或宿主引擎类型。
- 不同渲染帧率、Entity 遍历顺序和 Checkpoint 布局产生相同目标粒子状态。
- 暂停、向后 Seek 和 Audio discontinuity 后可恢复正确状态。
- 调整预算只影响缓存布局和重建耗时，不改变目标时刻粒子结果。
- 宿主能力不足时稳定失败，或仅使用项目显式允许的受控降级。
- headless consumer 可以验证粒子 identity、状态摘要和确定性，不要求 GPU。

## 6. 明确不包含

- 通用游戏粒子引擎、任意脚本化发射逻辑或未版本化随机算法。
- 用 GPU 粒子静默替换确定性 CPU 路径。
