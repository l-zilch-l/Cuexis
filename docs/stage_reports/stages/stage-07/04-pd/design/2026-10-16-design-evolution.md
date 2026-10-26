# PD 设计演变与账本缺陷边界

状态：historical

证据日期：2026-10-15 至 2026-10-16；整理日期：2026-10-17

本页合并已被替代的帧账本修订、双层账本选型、逐更新完整结果选型和实施起步快照。
现行范围与字段分别归[PD 计划](../../../../../stage_plans/active/stage-07/04-pd/plan.md)
及[设计细则](../../../../../stage_plans/active/stage-07/04-pd/design.md)，不以本页重新定义合同。

## 缺陷与已被替代的方向

FrameObservationLedger 在 takeLatest、成功 receipt、expire 或 scope 切换后仍保留 frame payload 槽位。
旧实现容量 4 在第 5 帧、容量 4096 在第 4097 帧耗尽；推进 Tick 和冻结 Tick=0 都能触发。
根因是缺少可审计的 retire/reclaim 寿命，而不是暂停、Reset 或某个具体容量数字。
其修复仍归 PD-4，不能与请求身份表的前缀回收修复混为一谈。

10 月 15 日曾选择单 owner 双层账本，并进一步考虑每次更新保存完整当前求值结果。
后续重审发现，完整数组与 World 同时成为需要同步的状态源，会增加复制、失败回滚和释放协调成本。
因此这些中间首版选择已被替代，不能作为当前实施指令。

## 后续采用边界

10 月 16 日的[事务与交付重审](2026-10-16-pd-transaction-and-delivery-design-review.md)保留单 owner、
共享不可变准备数据、单一已提交状态、按修订只读物化以及独立交付/观察寿命。
跨阶段义务见[兼容与演进复核](2026-10-16-pd-cross-stage-design-review.md)，
消费准入/撤销见[注册生命周期复核](2026-10-16-pd-registration-and-plan-self-review.md)。

完整帧不得成为 Gameplay 推进凭据；资源证明、控制 fence、scope/generation、Unknown、迟到/重复回执、
Close 与物理释放仍必须遵守 RPA 合同。共享引用归零不自动证明设备安全或控制完成。
实施起步记录仅是 WIP 存档，不是实现、设备、性能或阶段接受；后续实际基线与修复分别有独立报告。
