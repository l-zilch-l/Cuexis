# Stage 7-PD 帧账本回收计划修订记录

状态：dated planning revision；2026-10-15；本记录保存帧槽回收缺陷与当时的寿命矩阵修订，后续同日重审已将目标架构扩大为“准备数据、事务状态、求值结果与按需交付”。未实施 RPA/PD 产品代码，未启动 PD-0/PD-1，也不表示 RPA-F1–F4、Stage 7-PD 或 Stage 7 已关闭。

## 依据与成因

接手文档和缺陷记录指出，当前 `FrameObservationLedger` 的 `publish` 在槽位耗尽时返回
`capacity_exceeded`，但 `takeLatest`、成功 `PresentReceipt`、`expire` 和 scope 切换都没有
释放 frame slot。受控 Release 复现中，容量 4 在第 5 帧、容量 4096 在第 4097 帧失败；所有
先前帧均已提交 succeeded receipt 并执行 expire，推进 Tick 与冻结 Tick=0 结果一致。暂停、
重置和 4096 只是暴露路径或容量参数，不是根因。

因此根因是 frame payload 槽位缺少可审计的 retire/reclaim 协议。更深一层的问题是把完整帧当作长期交付状态，并把资源引用、receipt/反馈及 scope/generation 历史放进同一寿命假设。PD 原有 latest-value 与异步 receipt 条款不足以修复或约束这件事。

## 本次计划修订

`plan-presentation-decoupling.md` 增加并明确：

- PD-3 必须在实现前冻结准备数据、事务状态、共同求值结果和帧交付对象的边界；完整帧不得成为 Gameplay 推进条件。
- PD-4 必须让交付对象、资源引用、反馈/receipt 历史、scope/generation 历史分离寿命和独立上界；latest-value 交换不能代替交付额度或观察协议。
- PD-4 必须为消费者 lease、attempt identity、Unknown、迟到成功、重复回执和旧 scope/generation 回执建立显式状态机，并保留有界 tombstone/diagnostic 证据，不能通过清空旧 scope 或扩大容量绕过问题。
- 明确高低水位、背压、冻结帧合并/抑制、capacity-exceeded 的失败阶段和可观测性。
- 增加长期稳态、长暂停冻结帧、暂停恢复、重置/关闭、被丢弃帧、延迟/重复/旧代回执、最后
  资源释放和独立内存上界验收；PD 后重新绑定 F3 五类测量与新实现 SHA。

## 仍待 owner 裁定与后续实现准入

本记录不擅自冻结具体容量数字、消费者额度、resultVersion 保留数、Unknown 的终结时限、receipt tombstone 编码或冻结帧合并策略。上述项目必须在 PD-3/PD-4 实现准入前写入新的数据寿命矩阵和状态转移表；在此之前不得实现交付回收逻辑或以测试容量扩大宣称缺陷解决。

## 范围与验证边界

本轮只修改 PD 计划、dated 记录和报告索引。没有修改 `app/execution_host/`、Player、
Playback、Judgement 或任何 FramePacket/Receipt 实现，也没有开始 PD-0/PD-1 基线计时。
