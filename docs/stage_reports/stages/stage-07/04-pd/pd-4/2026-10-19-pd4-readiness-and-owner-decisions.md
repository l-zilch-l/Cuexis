# PD-4 准备与 owner 决策清单

日期：2026-10-19。状态：owner 已在本对话接受三项推荐并授权实施 PD-4；本文保留准备时点记录，后续实施与本地验证见 [PD-4 报告](2026-10-19-pd4-delivery-retirement-and-observation.md)。
当前状态唯一见 [CURRENT_STATUS](../../../../../CURRENT_STATUS.md)。

## 已核对的起点

分支 stage-7，HEAD `d344088236ae589c0b49a4994ef0fbddd6567ce2`，SDK 0.8.1。
PD-3-c 内部实施及适用门禁和独立审核完成；实施输入指纹为
`abdf37111c38826b803d3e6d4ec36c561861d2f94d53d8247ea6844b1e980ed8`。
既有脏工作区保持原样，准备入口源码/文档/构建文件归档在 `out/pd4-preparation-20261019/entry-workspace.zip`，
文件 hash、分支和 HEAD 为 `entry-state.json`，原始差异为 `entry-diff.patch`。

[实体记录](../pd-3/c/2026-10-19-pd3c-physical-music-and-keyboard-observations.md)中，Release 键盘首轮
约 17.6 秒出现 capacity_exceeded、退出 1，第二轮 Pause/R/Resume 退出 0。
R 重建账本不能证明回收；现场未记失败 frameId，不能称它精确失败于第 4097 帧。
普通音乐 Player 的同步载入/重载等待和 underrun 计数另行保留，不作为 PD-4 的隐含修复范围。

## 哪些工作可以独立完成

代理可独立完成代码盘点、实现拆分、额度/lease/有界 journal 设计与故障注入、源码绑定、
Debug/Release/static/shared/candidate OFF 和安装消费者验证、受控长时自动化、独立审核与证据归档。
容器、内部类型、测试 fixture 规模及代码组织由代理按维护性优先选择，避免把这些实现细节交给 owner 裁决。
不修改 Gameplay Kernel、Fact/Fold、Replay/Score 语义，不提前拆 owner，不为消除错误扩容。
现有 renderer/main、Playback D2/D3 worker 和 audio owner 的控制、设备资源及关闭责任保留。

实体键盘、听音、设备插拔/恢复的最后复测仍需人工协助；生产 profile 与阈值接受继续独立。
缺少这些证据不阻止已接受合同下的内部实施与自动化，但不能据自动化宣称设备或 Stage 7 已退出。

## 需要 owner 接受的三项

推荐一次接受 [ADR 0047](../../../../../adr/0047-pd-feedback-lifetime-and-delivery-retirement.md)中的三项：

| 决策 | 推荐及影响 | 依据 |
| --- | --- | --- |
| 显式反馈寿命与 SDK minor | expiry 改为 optional typed end，SDK 0.8.1 → 0.9.0；旧消费者迁移并重建，拒绝跨 minor；不承诺直接替换旧 DLL | VERSIONING SDK 更新流程第 2 条、宿主 §8.2、Gameplay ABI PD 迁移节 |
| 注册、交付及观察不足的行为 | 普通帧/重绘统一校验 ConsumerId；激活/撤销经 Renderer owner 确认；实际读取结束即释放 CPU backing；journal 不持有完整帧。纯观察不足成为 Unknown/coverage loss，可靠通道不足仍按原故障规则 | 宿主 §8.3、PD 细则 §6–8/§11 |
| 保留画面的压力降级 | 显式声明压力或设备/资源失效才可撤销资格；可靠通知，释放自身引用；暂停无原图时报告内部 NoRetainedFrame，不拿新图替代。合法恢复后重建，首版不保证跨代精确暂停画面恢复 | 宿主 §8.4、PD 细则 §12.4 |

owner 已明确回复“我接受这三项，你开始实施PD-4”，三项合同与目标 SDK 0.9.0 已接受；不等同于实施或验证通过。
VERSIONING 明确要求“经 owner 接受；不兼容变更须先接受相应 ADR”，PD 细则 §11 分别要求
PD-4 前接受 optional expiry、内部注册细化和保留资格撤销。问题不是由代理能力或工具权限造成。

## 版本与迁移准备

基线已现场核验为 0.8.1，故具体推荐目标是 0.9.0，不从历史 0.8.0 提案直接跳版本。
已发现的安装差异是 execution_control.hpp 的 FeedbackObservation.expiry 以及 gameplayFeedback 的返回表示。
created 仍按原 signed i64 presentation T，status/laterConfirmed 与有限 exclusive end 语义保持；
UntilReset 为 absence，有限 end=INT64_MAX 的效果仍可正常到期。

本次 ABI 变化需要 consumer 源码迁移：检查 optional 分支、读取 typed tick，并干净重建；
不保持永久旧反馈状态机。SDK 版本只在实施落地时修改，日期构建身份另按 version updater 流程处理。
预期不改 FrameSnapshot、HostContext、Gameplay identity 或 Replay wire；实际审计如有额外差异先补决定。
消费者矩阵须包括 default OFF/ON 安装头、ASCII、static/shared、0.9.0 最低请求、过高 patch、
0.8.x/其他 minor/major 拒绝、真实旧 0.8.1 包拒绝 0.9.0 请求；旧源码按已批准迁移规则验证，不伪称无需改动。

## 可独立推进的实施批次

以下是对现有 PD-4 范围的执行顺序，不新增阶段合同；完整语义归 [PD 细则](../../../../../stage_plans/active/stage-07/04-pd/design.md)。

1. 合同接受落约与公开 optional expiry 迁移；先做真实 lease/存储计账和寿命矩阵，明确 CPU/GPU/证据三维结束条件。
2. 消费注册、确认/撤销、RealtimeLatest 与 ExactCapture；先准入再物化，Ready/take/取消/过期唯一先后，移交后请求历史只留 metadata。
3. 紧凑 journal、有界 coverage/tombstone、异步 receipt；设备执行去重及 last-success 与可淘汰历史独立；receipt 丢失或 journal 淘汰不延长 CPU payload。
4. 保留重绘与压力撤销；同 viewport 共享 backing，resize 独立准入；验证迟到旧注册/旧保留身份拒绝及 Close 后外部 lease 真实释放。
5. 四配置和消费者门禁、受控长时/小容量与故障竞态、实际 Player 调用点及独立审核，再进行新二进制身份下的实体复测。

首选有界 owning backing 和独立 storage account、紧凑 metadata journal，按实际引用释放后归还额度。
首版不为物理析构先增加回收线程；若确有不可满足的销毁预算，作为独立证据记录，不隐含扩大 owner 拓扑。

测试 profile 将明确边界：呈现端最低双 payload；至少两个独立 CPU capture 消费者的注册/退役实占与全局份额；
bytes 取准备布局的保守单帧上界并计 membership/字符串/响应元数据；用极小 journal 强迫淘汰和重复/迟到回执，
长测累计次数明显超过原 4096 容量，并核对当前实占、高水位和 coverage，而非只看进程退出。
这些数值与范围是 testOnly 边界，不能直接接受为生产默认配额、TTL、噪声界或延迟预算。

## 准备完成与停止点

- 准备时点已形成可审查的 ADR、具体版本方向、工作拆分和验收清单；当时尚未改生产源码、头文件或 SDK。
- 三项合同已接受，进入相应生产实施；每个增量仍须保留验证与回退证据。
- 新 owner/线程、第二 Renderer、录制/导出产品、平台压力探测、跨代暂停精确恢复和音乐同步载入优化不在该准备范围内。
- 发现额外安装 API/错误、格式/Replay wire 或产品保证变化时先写具体差异再决定。
- 不授权提交、推送、PR、合并、发行；不把 PD-4 完成等同于正式性能、物理设备或 Stage 7 关闭。
