# Chart Format Foundation: Closure and Stage 6 Handoff

日期：2026-09-16

状态：completed；项目所有者确认完成，Stage 6 进入 active

## 关闭依据与证据边界

项目所有者于 2026-09-16 明确确认 Chart Format Foundation 已完成，并要求将计划移入
`completed`、Stage 6 移入 `active`。本地 Git 记录显示 PR #24
`codex/chart-format-foundation` 已于 2026-09-08 合并，合并提交为 `13dab93`。

本记录保存此次完成确认和文档交接，不是新增构建或 hosted 验证报告。已有
[F8 容量报告](2026-09-07-f8-capacity-harness.md) 和
[F9 readiness 快照](2026-09-07-f9-handoff-readiness.md) 保留其 2026-09-07 的原始结论；
其中待补的三平台运行链接、最终验证 SHA 和原始测量数据未在本次文档更新中独立核验，
不得据此宣称已取得新的 hosted PASS 或实测值。阶段关闭按本次 owner 确认记录，
技术验证证据仍以各带日期报告及其实际运行产物为准。

## Stage 6 接手边界

- Packed candidate `flags=1`、`candidateRevision=1`。
- Foundation 静态 Tap/point/lane/press profile，以及已登记的
  STR0/REF0/IDN0/ARCH/ENT0/TRN0/REN0/CNS0/REQ0。
- CXT v2 Core 的 integer/beat 参数与有限展开，参数变更必须显式重新编译。
- CXC v1 candidate entry mapping，以及 40,000 实体 / 16 MiB 容量测试框架。
- Chart v4、CXT v1、CXC v1 与 SDK API `0.7.0` 继续作为兼容基线。

关闭 Foundation 不代表 v5 默认 Writer、正式 Packed Playback 发行、Hold/Release、
Slide/Flick、多指、未定义 Behavior/Animation/Effect sections 或运行时 CXT 展开已实现。
尚未冻结的 decoded/prepare 预算也不能被描述为已冻结。

## 文档交接

- [Foundation 计划](../../../stage_plans/completed/chart-format-foundation/plan.md) 归档到 completed。
- [Stage 6 计划](../../../stage_plans/active/stage-06/plan.md) 激活，从 S6-A 合同与基线准备开始；
  S6-A 至 S6-F 不因本次目录迁移而标记完成。
- [Chart v5 跨阶段总工作包](../../../stage_plans/active/chart-format-update-for-v5/plan.md)
  保留 active，正式发行门禁仍归 Stage 8。
- 版本递增门禁、后端中立渲染和常用媒体支持三个工程问题仍归 Stage 6，保持 open。

当前阶段摘要以 [CURRENT_STATUS.md](../../../CURRENT_STATUS.md) 为准。
