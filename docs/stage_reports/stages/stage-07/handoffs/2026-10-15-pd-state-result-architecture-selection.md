# Stage 7-PD 准备数据与求值结果架构收口记录

状态：dated design selection；2026-10-15；本记录收口 PD 计划的目标架构，不实施产品代码，不启动 PD-0/PD-1，不表示 RPA-F1–F4、Stage 7-PD 或 Stage 7 已关闭。

2026-10-16 后续重审：本记录保留原选型时点；其中每次更新保存完整当前结果的首版实现选择，
由[事务、交付与演进成本重审](2026-10-16-pd-transaction-and-delivery-design-review.md)替换为
单一提交状态源、轻量修订描述、中性只读物化和独立交付绑定。准备数据、按需交付、独立观察
的职责保留，现行细节以 [PD 设计细则](../../../../stage_plans/active/stage-07/presentation-decoupling-design.md)
为准。具体 bytes/生产阈值仍待测量接受；本补充不是产品实施或模拟实施证据。

## 重新审视的结论

此前的“单 owner 双层账本”在设计层面针对已复现的 frame slot 不回收问题，但仍把完整帧、设备使用、Gameplay 推进和观察历史放在同一设计中心；它没有形成产品修复证据。继续增加 slot 状态、retirement watermark 或异步回收器，会提高协调成本，却不能减少完整帧被反复复制和保存的根因。

新的目标架构是：

**准备数据、事务状态、已提交求值结果、按需帧交付、紧凑观察记录。**

```text
prepared data + transaction state + RuntimeFrame
                         |
                 one presentation evaluator
                         |
             next state + committed result
                         |
              demand-driven FrameSnapshot
                         |
              renderer / capture / preview
                         |
                 compact observation journal
```

## 收口的职责

- 准备数据包含曲线、动画定义、绑定索引、静态基线和资源描述；可共享部分为不可变 owning 值。
- 事务状态包含 projection cursor、active effects、future queue、group state、tokens、override lifetime 和时间连续性，由唯一 owner 提交。
- 一次成功 Runtime 更新同时产生下一事务状态和可复用的已提交表现结果。Projection cursor、token、lifetime、World 提交和 resultVersion 不能拆成互相独立的成功点。
- FrameSnapshot 只在获得消费者额度后按当前已提交结果生成。没有消费者时不构建无用完整帧；消费者丢帧不回退 Gameplay，也不重复消耗 lifetime。
- Frame delivery 对象拥有 CPU 读者 lease；GPU 资源和设备 fence 由 Renderer owner 独立管理。
- Observation journal 只保留 attempt identity、上下文、feedback membership、receipt 状态、时间、错误和有界 tombstone，不持有完整 FrameSnapshot。
- 共享引用计数或异步 reclaimer 可以用于物理保活，但不能裁决 Gameplay、控制 fence、设备完成、Unknown 终结或 Close。

## 兼容与迁移

FramePacket 允许作为宿主内部的短期包装，但不再是完整帧历史或 Gameplay 推进凭据。现有 `PropertyOverrideWrite`、World 提交和 Runtime rollback 先保留；PD-3 提取共同求值结果，逐步减少整 `RuntimeEvaluationState` 复制。不得另写一套动画或表现求值引擎。

实时窗口、录制、预览和截图使用不同的消费者额度与交付策略。窗口可以合并过期画面，录制必须使用独立可靠交付；截图或重绘不自动生成新的 Gameplay feedback obligation。

## 未冻结项目

具体消费者额度、resultVersion 保留数、journal 条数/字节/时间上界、Unknown history window、资源 fence 机制和生产预算仍需 PD-0 与 RPA-F3/目标设备测量后接受。未冻结数值不能由实现默认值隐式变成合同。

本记录取代同日“单 owner 双层账本”作为 PD 目标架构，但保留其 scope/generation、receipt、Close 和设备资源边界作为迁移输入。
