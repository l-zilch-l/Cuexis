# Stage 7-RR 可实施性索引

状态：candidate；独立 checkout 复审后的修订与验证以本轮报告为准，未实施、未发布

更新日期：2026-11-23

<!-- cuexis-meta
id: PLAN-active.stage-07-rr.design-readiness
status: candidate
authority: index
-->

输入：2026-11-21 PR #47 可实施性审查（`9623efa3`），编号按报告 §4.1–4.6、§5 保留。[发现登记册](findings.md)记录基线与核对状态。本页检查设计闭合；[计划](plan.md#唯一派单授权入口)单独登记实施采用；[验收](acceptance.md)单独登记证据。当前新增生产实现全部未开始。

## 路线比较

| 路线 | 收益 | 代价 |
| --- | --- | --- |
| 保留开放提案逐项评审 | 改动最小，可继续收集意见 | 派单时要重开类型、数据和兼容选择 |
| 先做纯函数原型 | 快速暴露数值与测试问题 | 不能验证完整消费、安装和迁移路径 |
| 只任务化 D-05 现行范围 | 缩短首批路径 | P3/P4/P5 仍需重建设计上下文 |
| 把跨阶段实现合成一条重构 | 可能减少临时桥 | 扩大范围，回滚和旧集验证难隔离 |
| **合同与任务统一安排，分批实施验收** | 已接受的修复、结构清理与实验能力连续交付，每卡有明确写域和正反证据 | 需维护真实前置及旧 SDK 兼容窗口，最终退出须核对全部分支 |

2026-11-23 按 owner 决定采用第五条的统一安排：T-01–T-17 及具名全面结构清理均纳入本轮。新玩法先实现并实验验证，Stage 8 按后续续议以 v3 为正式主线并保留已承诺旧内容兼容；公开接口实际退役继续按组保留过渡条件。裁定与残余处置见[登记册 §24](findings.md#24-统一实施前设计裁定2026-11-23)。

2026-11-23 的最新 plan/task 接手复查见[报告](../../../stage_reports/stages/stage-07-rr/handoffs/2026-11-23-execution-readiness-handoff.md)和[登记册 §25](findings.md#25-实施接手复查2026-11-23)。任务执行口径以修订后的卡为准；旧轮次的待审或阶段外移交记录不覆盖后续裁定。

## 七维闭合口径

| 维度 | 检查点 | 权威落点 |
| --- | --- | --- |
| 决策 | 已接受方向与候选技术选择可区分；逐卡有 scope | [附页](architecture-design.md)、[采用入口](plan.md#唯一派单授权入口) |
| 接口 | 行为、类型、签名、所有权、失败及状态转移给定 | 下表合同章节 |
| 格式与诊断 | 版本、字段、拒绝、注册和旧新接受集区别明确 | 下表格式与失败落点 |
| 构建 | 源、目标、头、安装和消费边界定位 | 下表构建与迁移落点 |
| 迁移 | 具名兼容窗口、旧行为、删除门槛与回滚可识别 | 合同及任务的停止条件 |
| 验收 | 可复现正反输入、期望及证据位置 | 任务卡的任务级验收与[阶段矩阵](acceptance.md) |
| 任务化 | ID、唯一调度、写域、步骤与证据可冷启动 | [任务索引](plan.md#任务索引)、[调度](schedule.md) |

「缺口追踪」一节的表列出技术合同与对应的验收向量。实施采用见计划，实施证据与逐组发布迁移见验收。

Huygens、Nash、Epicurus、Bacon 分别复验 R1、R5 帧交付与 `NEXT` 合同，四组结论均成立；当期结论见[首轮设计报告](../../../stage_reports/stages/stage-07-rr/verification/2026-11-22-design-readiness-review.md)。后续 Astra 九项的合同、责任卡和验收见[修订与交叉复验](../../../stage_reports/stages/stage-07-rr/verification/2026-11-22-astra-review-resolution.md#结果与证据)。A-01 的阶段 owner 接受和产品运行另按[验收](acceptance.md)登记。

独立 checkout 于 `8599c83a` 复审时暴露的新缺口与处置见[本轮复审处置](../../../stage_reports/stages/stage-07-rr/verification/2026-11-22-checkout-review-resolution.md)，旧九项复验仅覆盖当次修订。四组审查的 `out/review/stage-07-rr/T-01/final-20261122/` 等路径是原工作区证据，独立 checkout 未取得时不得依此宣布本次门禁通过；按 T-01 重验当前修订。本轮必需回执与短检查输出随报告交付。

## 缺口追踪

Player 与工具外部行为按 [D-16 已接受共识](architecture-design.md#外部行为2026-11-21-owner-已接受拟实施)记录；[P4 启动与退出](contracts/render-player-writer.md#命令与启动)由 T-14/T-15 分验，[P5 工具行为](contracts/render-player-writer.md#工具外部行为)由 T-13/T-16 分验，均纳入本轮安排，生产实施尚未启动。

| 审查编号 | 维度及唯一设计落点 | 任务 | 验收证据 |
| --- | --- | --- | --- |
| P1.1 | 构建：[目标、头、ALLOWED 与安装](contracts/runtime-timing.md#构建头与安装) | [T-04](tasks/T-04.md) | 源表、目标图、消费者头链 |
| P1.2 | 构建：[Playback 源表、prepare hook 与 CXC 解码隔离](contracts/runtime-timing.md#r1八个-playback-tu) | [T-04](tasks/T-04.md) | 默认/候选实际闭包、通用整包校验、禁入对照 |
| P1.3 | 接口：[Prepared 构造与归属](contracts/runtime-timing.md#prepared-与当次推进) | [T-10](tasks/T-10.md) | runtime-only 消费与构造隔离 |
| P1.4 | 接口：[时间正反查与类型](contracts/runtime-timing.md#谱面时间-api) | [T-12](tasks/T-12.md) | 正反查与溢出向量 |
| P1.5 | 接口：[当次推进及字典](contracts/runtime-timing.md#prepared-与当次推进) | [T-10](tasks/T-10.md) | 空结果、Miss、过期身份、故障前缀 |
| P1.6 | 验收：[可结算槽上界](contracts/runtime-timing.md#prepared-与当次推进) | [T-10](tasks/T-10.md) | 展开计数、槽不足前后状态 |
| P1.7 | 格式与构建：[纯 Reader 与源编译入口](contracts/prepared-artifact.md#拥有关系与入口)、[成品 wire](contracts/prepared-artifact.md#版本目录与原语)、[场景与动态表现](contracts/prepared-artifact.md#场景与动态表现)、[玩法准备行](contracts/prepared-artifact.md#jpr1-玩法准备行)、[资源闭包与完整身份](contracts/prepared-artifact.md#资源闭包与完整身份)、[CXC 源包与派发](contracts/prepared-artifact.md#cxc-条目源包及派发) | [T-13](tasks/T-13.md) | [PA-01–06](contracts/prepared-artifact.md#验收向量)：独立纯读、源与两种成品等价、锚键和身份拒绝、无成品源包与旧金样 |
| P2.1 | 接口：[目标矩形和坐标](contracts/render-player-writer.md#内存与布局) | [T-05](tasks/T-05.md) | 非零宿主偏移、像素与 GL 状态 |
| P2.2 | 格式：[多画幅作者字段与归一化](contracts/render-player-writer.md#作者画幅)、[比例选择与寿命](contracts/render-player-writer.md#内存与布局) | [T-11](tasks/T-11.md)、[T-15](tasks/T-15.md) | R5-06/09：作者原 pair、倍率、已规约帧字段及 Exact 拒绝 |
| P2.3 | 接口：[宿主桥与 FramePayload](contracts/render-player-writer.md#提交资源与交付) | [T-06](tasks/T-06.md) | R5-07：同 backend、submit 前截止对照、空区域无回执 |
| P2.4 | 接口：[当前/retained 与资源](contracts/render-player-writer.md#提交资源与交付) | [T-06](tasks/T-06.md) | 换代拒绝和宿主自有资源绘制 |
| P2.5 | 构建/迁移：[实验头、命名空间及窗口](contracts/render-player-writer.md#p2同提交布局与呈现) | [T-06](tasks/T-06.md) | 安装面与历史消费者 |
| P3.1 | 格式：[作者版本和计分字段](contracts/runtime-timing.md#作者版本stop比例和最小多等级)、[归一化计分与 Prepared 记录](contracts/runtime-timing.md#归一化计分与-prepared-记录) | [T-11](tasks/T-11.md)、[T-12](tasks/T-12.md) | 同一 Chart 的 common.score、漏规则和错版拒绝、源编译计分向量 |
| P3.2 | 诊断：[稳定诊断与迁移](contracts/runtime-timing.md#稳定诊断与迁移) | [T-11](tasks/T-11.md) | 两码 registry 与生成文档差分 |
| P3.3 | 构建：[分阶段创建源和目标](contracts/runtime-timing.md#构建头与安装)、[成品 read/source 分界](contracts/prepared-artifact.md#拥有关系与入口) | [T-10](tasks/T-10.md)、[T-12](tasks/T-12.md)、[T-13](tasks/T-13.md) | T-10 旧链可构建，T-12 独立产出时间与共享记录，T-13 新增 read/source 并验证依赖闭包 |
| P3.4 | 接口/迁移：[会话校正与查询职责](contracts/runtime-timing.md#playback-时钟与实时准入) | [T-14](tasks/T-14.md) | CLK-01–03：校正/过期、非零 late 截止与 EOF 单调时钟实推进 |
| P3.5 | 接口：[反查结果与 Stop 边界](contracts/runtime-timing.md#谱面时间-api) | [T-12](tasks/T-12.md) | S、E−1、E 及旧相邻样例 |
| P4.1 | 决策：[键位](contracts/render-player-writer.md#命令与启动) | [T-15](tasks/T-15.md) | R/F5 的调用轨迹 |
| P4.2 | 接口：[CLI 与诊断](contracts/render-player-writer.md#命令与启动) | [T-15](tasks/T-15.md) | P4-06/08：显式项目/CXC 选谱、wrapper 同轨迹、已声明支持的载体计分，源包支持由 Player 决定 |
| P4.3 | 接口：[六态矩阵](contracts/render-player-writer.md#完整准入矩阵) | [T-14](tasks/T-14.md)、[T-15](tasks/T-15.md) | T-14 验纯值准入与 owner 提交，包括 P4-07 Retry/Pause 竞合；T-15 验 CLI/窗口到同一命令路径 |
| P4.4 命令 | 接口：[生产 Retry 命令](contracts/render-player-writer.md#命令与启动) | [T-14](tasks/T-14.md#任务级验收) | P4-07：已有 owner 通道中 Retry 与两来源 Pause 的终态回执 |
| P4.4 按键 | 输入：[R/F5 绑定](contracts/render-player-writer.md#命令与启动) | [T-15](tasks/T-15.md#任务级验收) | R/F5 到 Retry/Reload 的轨迹，释放和重复按下不重发 |
| P4.5 | 迁移：[首次失败重载](contracts/render-player-writer.md#完整准入矩阵) | [T-15](tasks/T-15.md) | 无 A 时修复同一来源再加载 |
| P4.6 | 决策：[无音轨收尾](contracts/render-player-writer.md#所有权暂停与收尾) | [T-14](tasks/T-14.md) | P4-04/05：无音轨边界、输入截止与一次结果交付 |
| P5.1 | 接口：[源模型与版本入口](contracts/render-player-writer.md#p5内部-writer-与显式迁移) | [T-16](tasks/T-16.md) | v3/v4 同版、错版拒绝；P5-05 用 Resolver 核对 Animator add/replace |
| P5.2 | 格式：[损失 kind 和路径](contracts/render-player-writer.md#损失定位与迁移映射) | [T-16](tasks/T-16.md) | 字段级结构化损失快照 |
| P5.3 | 迁移：[discardedFields 处置](contracts/render-player-writer.md#损失定位与迁移映射) | [T-16](tasks/T-16.md) | Migrator 损失与 Writer 差异不丢失 |
| P5.4 | 构建/迁移：[旧 helper 与调用](contracts/render-player-writer.md#调用点与可见性) | [T-16](tasks/T-16.md) | 调用扫描与写回回归 |
| P5.5 | 构建：[Writer 消费可见性](contracts/render-player-writer.md#调用点与可见性) | [T-16](tasks/T-16.md) | 私有头不安装；CLI 不提交有损成品 |
| P6/§5 任务化 | 任务索引、唯一调度、阶段矩阵与交叉验证 | [T-01](tasks/T-01.md)、[T-09](tasks/T-09.md) | 七段卡结构、调度覆盖、A-01–A-09 回执；追溯见[本轮处置](findings.md#13-可实施性审查处置2026-11-21) |
| P6/§5 基线与台账 | 采用入口、基线记录、回滚与停止条件 | [T-01](tasks/T-01.md) | 基线绑定、已接受 D 项与实施记录分开；追溯同上 |

P3 的编译期和 prepare 期验收分别落在 T-12、T-13；旧接受集合裁定由 T-11 的合同差异固定。P1 的[旧 API 逐符号迁移](contracts/runtime-timing.md#rr-4-逐符号兼容迁移)由 [T-08](tasks/T-08.md)独立执行；旧宏成员退出、已保护渲染入口退役和 SDK 晋升按各组替代稳定性及发布裁定进入后继门禁；Judgement 晋升独立评估，不预排统一版本。
