# ADR 0047: PD 反馈寿命、交付许可与回收边界

状态：accepted design；owner 于 2026-10-19 在本对话明确接受三项推荐并授权实施 PD-4；内部实施与适用本地门禁完成，物理设备/正式性能/发行未验收；证据见 [PD-4 实施报告](../stage_reports/stages/stage-07/04-pd/pd-4/2026-10-19-pd4-delivery-retirement-and-observation.md)。

PD-3-c 后真实 DFJK 运行出现帧观察容量耗尽。完整 CPU 帧、设备在途责任和观察历史需要各自有界，
当前整数 expiry 又不能区分有限最大 Tick 与 UntilReset。owner 已接受以下三项，作为 PD-4 的合同及兼容性决定；
实施顺序与验证准备见 [PD-4 准备记录](../stage_reports/stages/stage-07/04-pd/pd-4/2026-10-19-pd4-readiness-and-owner-decisions.md)。

## 已接受决定

1. 接受[宿主候选 §8.2](../api/realtime-host-boundary.md)及
   [Gameplay ABI 拟议迁移](../api/GAMEPLAY_V2_ABI.md)：保留 FeedbackObservation 的 id、created、status、
   laterConfirmed；expiry 改为 `optional<GameplayPresentationTick>`。有值是有限 exclusive end，
   无值是 UntilReset；有限 INT64_MAX 与无期限必须可区分。gameplayFeedback 保留该分支，
   不改变 FactBinding 的有效区间、Gameplay/Replay identity 或既有结果语义。
   当前 SDK 0.8.1 迁移至 **0.9.0**，消费者修改源码并干净重建；旧 minor 明确拒绝。
   不保留永久双版本反馈状态机，不承诺旧头/旧二进制可继续使用新库。
2. 接受[宿主候选 §8.3](../api/realtime-host-boundary.md)和
   [PD 细则 §6–8](../stage_plans/active/stage-07/04-pd/design.md)：消费者注册须经呈现端
   owner 确认，旧注册撤销确认后同端才能重新加入。交付请求先准入额度，移交后请求历史不再保活完整帧。
   CPU 最后读者释放、Renderer retire/fence 和 present 证据独立管理；实际执行去重不能随 journal 淘汰。
   纯观察历史不足报告 Unknown/coverage loss/history unavailable，不能继续把它等同于可靠输入/控制容量耗尽。
   后者仍沿原故障合同；不靠遗失可靠记录或扩大容量维持运行。
3. 接受[宿主候选 §8.4](../api/realtime-host-boundary.md)和
   [PD 细则 §12.4](../stage_plans/active/stage-07/04-pd/design.md)：只有已声明的显式内存压力
   策略或设备/资源失效可撤销保留画面的后续使用资格。owner 串行裁决，经可靠生命周期通道确认，
   释放自身引用；外部 CPU lease 与 GPU 在途责任继续存活、计费和清理。暂停缺失原画面时显式报告
   NoRetainedFrame，不用较新 World 或 frozen update 补图；合法 Resume/更新和设备确认后才可恢复。
   跨资源代恢复精确暂停画面仍不在首版承诺内。NoRetainedFrame 首版采用内部结果及 Player 可观察状态，
   不在本提案中增加安装公共错误码。

## 取舍

继续使用 INT64_MAX sentinel 可少改消费者，但丢失有限最大 Tick 与无期限的区别；长期保留 V1/V2 查询
会增加两套映射和测试责任，故推荐一次 minor 迁移。只扩容或清空现有 ledger 无法证明真实存储回收，
也破坏迟到证据及去重；无界 journal 则无法控制历史成本。推荐有界 owning backing、独立存储计账和紧凑
metadata journal，优先沿用现有 owner；具体容器无需 owner 决定，不预先增加线程。

撤销保留资格会失去精确暂停重绘能力，因此必须显式降级，不能悄悄驱逐。该接受只规定触发与结果，
不接受任何未测生产内存阈值，也不把撤销确认当作全部内存或 GPU 已释放。

## 生效边界

owner 已明确回复“我接受这三项，你开始实施PD-4”。实施前同步宿主 §8.2–8.4、Gameplay ABI、版本决定与消费者迁移，
再更新头文件和 SDK。现有已接受合同仍有效，不以本提案覆盖它们。
若实际集成发现还需增加安装 API/错误、改变 Replay wire 或扩大设备保证，须先补充相应差异决定。
不授权 PD-5 owner 拆分、真实生产预算接受、提交、推送、PR、合并或发行。
