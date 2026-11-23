# Stage 7-RR 架构精简方案基线索引

状态：candidate；2026-11-22 技术设计基线，未实施、未发布

更新日期：2026-11-23

<!-- cuexis-meta
id: PLAN-active.stage-07-rr.simplification-proposal
status: candidate
authority: summarizes
-->

本页只作索引：边界见[架构设计附页](architecture-design.md)，可测规则见三份候选合同，任务见[任务索引](plan.md#任务索引)。附页保存 owner 共识。四组冷启动及差量审查见[首轮设计报告](../../../stage_reports/stages/stage-07-rr/verification/2026-11-22-design-readiness-review.md)，九项修订复验见[Astra 报告](../../../stage_reports/stages/stage-07-rr/verification/2026-11-22-astra-review-resolution.md)；后续独立 checkout 发现的缺口及当前修订见[本轮处置](../../../stage_reports/stages/stage-07-rr/verification/2026-11-22-checkout-review-resolution.md)和[可实施性索引](design-readiness.md)。`candidate` 标记未实施、未发布。[计划](plan.md#唯一派单授权入口)管实施范围与采用；A-01 阶段 owner 接受和产品运行证据见[验收](acceptance.md)。

## 整体设计基线（2026-11-22）

### P1 数据与 SDK

隔离默认与候选消费者的构建，分窗口迁移候选公开面，运行时直接交付当次结果。见[运行合同](contracts/runtime-timing.md)、[R1](tasks/T-04.md)、[RR-4](tasks/T-08.md)、[运行内核](tasks/T-10.md)与[成品](tasks/T-13.md)。

### P2 渲染与兼容迁移

同次提取交付布局、快照和资源身份，保留旧谱投影与旧入口兼容窗口。见[呈现合同](contracts/render-player-writer.md#p2同提交布局与呈现)、[几何](tasks/T-05.md)、[帧链路](tasks/T-06.md)与[后继作者比例](tasks/T-11.md)。

### P3 时间与会话时钟

新作者编译共享离散边界，旧规则和旧成品继续兼容；设备校正在 Playback 的会话换算中处理。见[时间合同](contracts/runtime-timing.md#谱面时间-api)、[格式](tasks/T-11.md)、[编译](tasks/T-12.md)、[成品与源包合同](contracts/prepared-artifact.md)、[成品任务](tasks/T-13.md)和[会话](tasks/T-14.md)。

### P4 Player 与旧控制权

保留启动显式请求和单一会话控制者。见[Player 合同](contracts/render-player-writer.md#p4player-生命周期)、[会话替代](tasks/T-14.md)与[启动和交互](tasks/T-15.md)。

### P5 写回与诊断

仓内 Writer 做显式同版写回和结构化损失报告，覆盖和交互归调用方。见[写回合同](contracts/render-player-writer.md)及 [T-16](tasks/T-16.md)；既有格式与诊断处置见 [T-03](tasks/T-03.md)。

### P6 迁移与验收

按[任务索引](plan.md#任务索引)、[调度](schedule.md)和[阶段验收](acceptance.md)交付；共享登记册与生成物的汇合按[§7 分批细化](plan.md#7-分批细化)执行。保存实际基线与消费者证据。旧 API 逐组兼容新增，每组至少有一个实际发布的过渡版本，替代稳定后才裁定该组退役，不绑定统一 SDK 版本。`NEXT` 已纳入本轮 T-01–T-17 的统一安排，随同一次基线采用分批实施；RR-3、独立 RR-4 的具名全面清理与相邻阶段边界按[计划](plan.md#唯一派单授权入口)。
