# Stage 7-PD 帧账本设计收口记录

状态：dated historical design selection；2026-10-15；本记录保留“单 owner 双层账本”的中间裁定，但已被同日更广泛的“准备数据、事务状态、求值结果与按需交付”架构修订取代。未实施产品代码，不启动 PD-0/PD-1，不表示 RPA-F1–F4、Stage 7-PD 或 Stage 7 已关闭。

## 中间采用方案（已被取代）

PD-3/PD-4 曾采用**单 Playback owner、不可变帧交换、双层有界账本**。该方案针对原始槽位不回收问题提出了 retire/reclaim 协议，但没有从根本上解除完整帧、设备使用和观察历史之间的结构联系。继续增加 slot 状态和回收条件会提高协调成本，不能减少完整帧被反复复制和保存的根因。

- Playback owner 唯一裁决 frame lifecycle、projection cursor、receipt journal 和 feedback journal。
- Frame payload ledger 保存 owning `FramePacket` slot；slot 内承载不可变 `FrameEnvelope`、`FrameSnapshot`、RuntimeFrame、资源引用和 feedback membership。
- Observation journal 只保存紧凑 receipt、反馈终态和 scope/generation tombstone，不保存完整帧，也不决定帧是否可退休。
- latest-value 只合并尚未被 Renderer/GPU 使用的帧；没有可回收 slot 时产生表现背压，保留最后有效帧，不改变 Gameplay。
- 输入、控制、Fact、Replay 等可靠通道继续遵守现有 `host.rpa.capacity_exceeded`、`Suspended`、Close 和 fence 合同。

共享引用计数可以用于不可变 payload/resource 的物理保活，但必须由 owner-issued retire token 和 resource fence 驱动。引用归零不能单独表示协议退休、Unknown 终结或 Close 完成；涉及 SDL、GL、设备或回收队列的析构必须回到所属 owner。

## 当时已收口、仍然保留的语义

1. **发布源**：Judgement 只发布 committed Fact prefix + cursor；`PresentationEffectBatch` 只能由 Playback adapter 派生，不能成为第二发布源。
2. **对象映射**：`FrameEnvelope -> FramePacket slot -> PresentReceipt -> observation journal`。FramePacket 与 FrameEnvelope 共享 frame identity，但 receipt 不持有可变 slot 指针。
3. **投影提交**：`projectionCursor` 只在 immutable PresentationProjection 与投影 token 成功发布后推进；失败不回退 Judgement cursor，且可从同一 Fact prefix 重试。
4. **回收状态**：帧负载、观察结果和资源使用分别维护状态；只有 latest slot 已解除、Renderer/GPU fence 已完成、CPU 读者已释放且 journal tombstone 已建立时才 reclaim。
5. **Unknown**：资源仍在使用时为 `UnknownInFlight`；大帧已释放但结果未知时为 `UnknownDetached`。迟到成功只能更新 tombstone 为 `LateObserved`，不能复活帧或污染新 scope。
6. **控制代际**：retirement watermark 按 `(sessionScope, projectionScope, controlSequence, frameId)` 分域；跨 scope/generation 不比较 frameId 大小。
7. **暂停与关闭**：暂停期间不为不变的冻结画面创建新 FramePacket；Close 按停止发布、排空 retire/fence、回收 payload、保留/压缩 journal、最终 Closed 的顺序执行，未完成时保持 Closing。

## 被取代原因

后续重审发现，完整 `FramePacket` 不应成为 Gameplay 推进条件，也不应进入长期 observation history。帧交付、GPU 资源寿命、Runtime 生命周期和 receipt 观察是不同责任域；只增加双层 ledger 仍会把交付容量和状态提交绑定在一起。现行 PD 计划改为共同求值结果加按需 FrameSnapshot 交付，双层账本只保留为可选的过渡实现。

## 未来扩展边界

该中间方案的 owner、scope/generation、receipt、Close 和共享引用计数边界仍是有效输入，但不再冻结完整帧 ledger 为目标架构。未来实现可以引入异步 reclaimer、多窗口或多 owner，前提是先稳定共同求值结果、消费者 lease、attempt identity 和有界 observation journal。多-owner 只有在新 PD-5 的连续 Release 窗口证据满足计划条件并经 owner 接受后另行设计。

## 未冻结项目

具体 journal 容量、Unknown tombstone 的保留时长和硬件相关背压窗口仍属于 F3/设备测量输入；它们不能被实现默认值隐式冻结，也不能通过扩大固定容量替代生命周期协议。
