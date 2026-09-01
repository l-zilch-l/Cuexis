# Stage 11 Implementation Plan: Input, Judgement, Score, and Replay

状态：future；未开始

更新日期：2026-08-10

归档来源：[旧版 PROJECT_GUIDE](../../../archive/PROJECT_GUIDE_LEGACY_2026-08-10.md) 与
[SDK transition plan 快照](../../../archive/CUEXIS_SDK_TRANSITION_PLAN_2026-08-10.md)。

## 1. 阶段目标

交付 Playback SDK 必选的 cuexis_judgement 模块。宿主提交标准化输入，SDK 计算并返回判定、
分数、连击和统计，同时提供确定性记录与回放。完整玩法状态机、UI 和游戏流程仍由宿主拥有。

## 2. 前置条件

- Stage 7 Studio 核心使用稳定 PlaybackSession 预览路径。
- Stage 9A 已测量输入时间戳、音频延迟、Session 和快照成本。
- Chart/Playback 已提供稳定 Note/Event 时间流、ObjectId 和确定性 Session 配置 identity。

## 3. 输入与校准合同

- 定义带单调 event time、source、arrival time 和 sequence 的 InputEvent/InputFrame。
- 定义事件时间到 Timeline 的映射。
- 分离输出延迟、输入延迟、主观校准和 Chart offset。
- 定义版本化 InputProfile、CalibrationProfile 和 JudgementConfigSnapshot。
- arrivalTime 和 frameIndex 只用于审计，不参与判定语义。

## 4. Judgement 与结果

- 实现 Tap/Miss、JudgementEvent、Score、Combo 和 Statistics。
- PlaybackSession 接收 RuntimeFrame 与 InputEvent，并发布累积 JudgementResult 快照。
- 定义宿主可查询的 Note/Event 时间流和稳定 ObjectId。
- 为 Hold、Slide、多指和宿主扩展保留版本化、受预算约束的扩展点。
- JudgementResult 必须拥有明确生命周期，且 extract 不复制无界历史。

## 5. Recording 与 Replay

- startRecording 记录 Session 收到的完整规范化 InputEvent、chartTimeMs 和 frameIndex。
- stopRecording 返回版本化 ReplayData 和确定性 Session 配置快照。
- loadReplay 使用记录事件替代实时输入。
- Replay 模式拒绝混合提交实时 InputEvent，并保持原 Session 状态原子性。
- ReplayData 具有事件数、字节数和解码预算。

## 6. 验收标准

- 相同 Chart、InputEvent、校准和配置产生一致的判定、分数、连击和统计。
- Judgement/Replay 不暴露 World、EnTT、SDL、音频/渲染后端或宿主引擎类型。
- 无 InputEvent 的纯播放和 Studio Preview 中 Judgement 休眠且不改变 FrameSnapshot。
- 宿主在运行或停止后可以取得有明确有效期的完整结果快照。
- 记录事件与宿主提交的原始规范化 InputEvent 完全一致。
- 实时模式与 Replay 模式的 JudgementResult 和 FrameSnapshot 确定一致。
- ReplayData 序列化/反序列化往返保持结果。
- Replay 版本不支持、内容损坏或配置 identity 不一致时稳定失败。
- Replay 模式提交实时输入时返回明确错误且不部分应用。
- public external consumer、static/shared package 和支持平台矩阵覆盖完整生命周期。

## 7. 明确不包含

- 宿主 UI、完整游戏状态机、在线服务和宿主持久化策略。
- 把判定结果写回 Chart、Behavior 或 World 持久化数据。
- 运行时脚本或宿主任意回调作为判定规则。
