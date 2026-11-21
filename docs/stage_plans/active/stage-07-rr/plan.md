# Stage 7-RR Plan: Review and Refactoring After Stage-7

状态：active；Stage 7 与 Stage 7.5 之间的独立复核与架构减重重构阶段

更新日期：2026-11-21

<!-- cuexis-meta
id: PLAN-active.stage-07-rr.plan
status: active
authority: owns
-->

本计划覆盖 Stage 7 受限退出之后、Stage 7.5 产品实施之前的复核、合同收敛、架构减重与交接；不覆盖 Stage 7.5 的能力演进、GE-R 性能整改与发布。阶段进度以 [CURRENT_STATUS](../../../CURRENT_STATUS.md) 为准，当前任务与下一步以 [HANDOFF](../../../HANDOFF.md) 为准；问题清单以[发现登记册](findings.md)为准，报告入口见 [Stage 7-RR 报告索引](../../../stage_reports/stages/stage-07-rr/README.md)。

归档来源：2026-11-17 全项目只读复核的 10 份子代理报告与 owner 抽查记录（会话内记录，未逐份入库；结论逐条转写进 [findings](findings.md)并标注是否经独立复核）；同日 8 份架构研究子代理报告（依赖分层、双产品线、格式线、时间源、渲染路径、会话层次、文档与注释、公共 API 面）；上一轮[源码拆分安全网独立审计](../../../stage_reports/stages/stage-07/90-shared/source-split-audit/index.md)；[Stage 7 受限退出与缺陷交接](../../../stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-14-limited-exit-and-defect-handoff.md)。本阶段已落盘的研究、证据与审计报告见第 11 节；返回[阶段计划索引](../../README.md)。

## 规划前必须回答的问题
1. 可验收成果是什么、哪些明确不做？见第 2、13 节。
2. 依赖哪些冻结合同、证据与构建？不满足时的停工判据是什么？见第 3、10 节。
3. 哪些事项 owner 已决定、哪些必须再问？见第 4 节。
4. 关键路径与批次依赖是什么、哪些批次可并行？见第 6 节。
5. 不可逆点、迁移点与兼容断点在哪，失败如何回滚？见第 5、10 节。
6. 每个批次的完成定义、正反例证据与验收命令是什么？见第 7、9 节。
7. 预算、上限与版本数字的权威源在哪？本计划只链接，不复述。
8. 会产出哪些报告、放在哪、用哪个模板？见第 11 节。
## 1. 计划定位与边界
`RR` 只表达路线位置（Stage 7 之后、Stage 7.5 之前），不是 Stage 7 的子阶段，也不改变 Stage 8–14 的编号；与 [Stage 7-RPA 计划](../../completed/stage-07/05-rpa-new/plan.md)不是同一对象。本阶段不重开 Stage 7 的受限退出判定，不改写其历史失败、未认证范围或 testOnly 状态。

```text
Stage 7（受限退出） -> Stage 7-RR（本阶段） -> Stage 7.5（Gameplay Evolution） -> Stage 8
```
[Stage 7.5 计划](../stage-07-5/plan.md)的入场前置已包含“本阶段当前计划范围完成并取得 owner 接受”；本阶段只交付复核、合同收敛、减重决策与交接，不决定 Stage 7.5 的能力范围或实施顺序。

- [发现登记册](findings.md)是问题与证据状态的唯一清单；本计划不复制其正文，只用 ID 引用。`docs/stage_reports/stages/stage-07-rr/` 下的报告是证据，不是权威，不据报告的“下一步”推断当前任务。
- 本计划是执行合同：批次范围、先后、验收与停止条件以本计划为准；发现问题仍回写 findings。本计划不决定 SDK minor、发行版本号、合并与发布；稳定 C ABI 归 Stage 14，需要修订 ADR/Spec 时按第 10 节由 owner 接受。
## 2. 阶段目标
### 可验收成果
1. **结构证据与减重决策**：交付依赖分层、格式线、时间源、渲染路径、会话层次与公共 API 面的代码证据，并对每个合并/下沉/删除/标 candidate 的对象交付具名决策、受影响合同、历史证据处置与回归证据；判定：RR-0 研究/证据报告落盘，RR-3/RR-4 退出报告与删除/合并清单可逐项核对。
2. **状态与合同一致**：现行文档不再与版本、分支、阶段归属、格式状态和 SDK 陈述冲突，历史值显式标为历史；“文档承诺存在而实现不存在”或“实现比文档更严/更松”的条目被修正，无法实现的标为未实现；判定：RR-1 文档门禁全过且无事实冲突，RR-2 每条有 Spec/ADR 裁定或“未实现”标签。
3. **失败语义统一**：同一失败在文档、实现与宿主观测面得到同一个稳定码，静默失败被消除或显式登记；判定：RR-2 的正反 fixture 与受影响测试通过。
4. **交接可接手**：结论、处置理由与残留缺口写入 Stage 7.5/Stage 8 交接，不留在会话记录里；判定：RR-6 退出报告与 HANDOFF/CURRENT_STATUS 指针一致。
### 非目标
- 不追求“所有 P2 清零”或“结构无技术债”；P2 可 deferred，但须写明归属阶段与恢复条件。
- 不提升 gameplay、渲染、音频或性能能力；不新增格式、capability、SDK 或生产保证范围。
- 不为行数预算拆文件；owner 已取消纯行数拆分，行数只作为 findings §4 的事实记录。
- 不删减历史证据来让文档“看起来瘦”；减重不等于删证据。
## 3. 前置基线、依赖与入场门禁

| 依赖 | 当前状态 | 权威指针 | 不满足时的动作 |
| --- | --- | --- | --- |
| 历史研究绑定 `aac83e5e`（2026-11-16） | 保留历史基线 | [架构研究](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-research.md)、[发现登记册](findings.md) | 按对象核对后继差异；有变化时重跑对应证据，不外推旧结论 |
| 本轮文档修订依据 `ed1537fd`（2026-11-21） | 已作有限差异核对，非构建验收 | [提案复核记录](findings.md#12-提案复核记录2026-11-21) | 构建图、安装面和当前版本的验证须另取证，不继承历史构建通过 |
| SDK API `0.9.1` | 已固定 | [CuexisVersion.cmake](../../../../cmake/CuexisVersion.cmake) | 停止安装面/ABI 批次，先取得版本决定 |
| 日期构建 `26.11.18-1` | 当前权威 | 同上；早期报告中的构建字面量是历史写作时点 | 以版本文件为准，不回写旧报告 |
| Stage 7 受限退出交接 | 已接受 | [R4 交接](../../../stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-14-limited-exit-and-defect-handoff.md) | 不重开判定；受影响批次改为只登记、不改写 |
| Stage 7.5 入场前置 | 待本阶段接受 | [Stage 7.5 计划](../stage-07-5/plan.md) | 本阶段未取得 owner 接受前，7.5 不开始产品实施 |
| 文档门禁与证据 | 已接入 | [BUILDING](../../../guides/BUILDING.md)、[发现登记册](findings.md)、[报告索引](../../../stage_reports/stages/stage-07-rr/README.md) | 门禁失败即停该批；条目未复核或报告缺失时，不据此删代码、不改合同 |

### 入场门禁（代码开始门槛）

| 门禁 | 允许开始 | 禁止 |
| --- | --- | --- |
| 基线与合同冻结 | 文档订正、证据补证、非 wire 的合同澄清 | 改变 wire 字节、接受集合或稳定码语义；未取得 ADR/Spec 接受就动实现 |
| RR-0 完成且 D-05 已裁定 RR-3 范围 | 逐项满足本节其余门禁后，才开始相应实施 | 范围裁定本身不授权启动实现；延期选项须另获 owner 决定；RR-2 与 RR-3 不得同时改同一对象 |
| 批次取得 Spec/ADR 裁定 | 实现 + focused tests + 正反 fixture | 以“代码已经是新行为”回写合同；让 Reader 静默接受两种解释 |
| 证据路径冻结 | 新增报告、追加 findings 处置 | 移动证据套件目录、改写已归档报告或回填旧 SHA |

### 历史进展快照（2026-11-17）
- RR-0 已完成：8 份专题结论见[研究](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-research.md)与[证据](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-evidence.md)，6 个减重选项 R1–R6 与建议路线已登记；当前路线选择见 §4 的 D-05，RR-3 尚未开始。
- RR-1/RR-5 的文档治理机械层已完成（[报告](../../../stage_reports/stages/stage-07-rr/2026-11-17-documentation-refactor.md)）：170 篇合同/计划补元数据、生成 MAP/AGENTS 块、424 篇证据报告加统一头、计划状态纠偏、7 处假断言订正、`check_docs_facts.py` 接入；合同语义重写与 R3/R4/R5 相关正文仍未开始。
- 假保证扫描与登记册全量核查已完成：前者 48 条候选、26 条经 owner 复核确认（4 条文档过期已就地修正），最高危的 `ChartWriter` 保存丢数据（RR-F401）与三个常量诊断字段（RR-F419/F420）未开始修复，处置顺序见扫描报告末节；后者条目全部有结论，复核中推翻或收窄了若干断言，未复核项已清零；具体计数与分级以 findings 为准。
- tools/ 审计完成（[报告](../../../stage_reports/stages/stage-07-rr/2026-11-17-tools-audit.md)）：120 个脚本审计完成，证据套件原位冻结、门禁修补、报告来源登记，RR-F511–RR-F520 的已确认缺陷已修或登记；日期构建以版本文件为准，本阶段早期报告的构建字面量是历史值。
## 4. 已确认方向与决策台账

| 决策 ID | 决策 | 日期 | owner | 影响 |
| --- | --- | --- | --- | --- |
| D-01 | Stage 7-RR 是 Stage 7 与 Stage 7.5 之间的独立阶段；`RR` 只表达路线位置，不改变 Stage 8–14 编号 | 2026-11-17 | owner | 全计划；与 Stage 7-RPA 区分 |
| D-02 | 本阶段以架构减重重构为主；文档与代码注释纳入减重范围 | 2026-11-17 | owner | RR-3、RR-5 |
| D-03 | 取消文件拆分工作；行数预算只作为事实记录保留在 findings §4 | 2026-11-17 | owner | 第 13 节排除项；RR-3/RR-4 不做拆分 |
| D-04 | 合同先于实现：改变 wire、接受集合或稳定码语义前，先取得对应 ADR/Spec 接受 | 2026-11-17 | owner | RR-2、RR-3、RR-4 |
| D-05 | RR-3 限于 R1 构建隔离、R5 渲染收敛、R6 文档减重；R2/R3/R4 延期至本阶段退出后再评估。范围裁定本身不授权启动实现 | 2026-11-17 | owner | RR-3 范围；RR-4 是独立任务，其前置见 §6 |
| D-06 | 单一 Player 承接普通播放与实时游玩，普通播放不开判定；共享改动分别验收两种用途。仅拆出的运行时判定内核只依赖 `core`（头文件与链接）；Playback 统一会话内三种时间含义的换算，每条规则只有一个权威实现 | 2026-11-18 | owner | [方向与时间换算](architecture-design.md)；作者编译器依赖按 D-21 |
| D-07 | 仅启动时显式请求且校验通过才启用实时判定；未请求则普通播放。参数名、其余交互与失败处理仍待设计 | 2026-11-18 | owner | [显式请求边界](architecture-design.md)；必需能力不支持的拒绝规则见 D-17 |
| D-08 | 同一会话控制者决定播放、暂停、恢复并发布结果；具体串行控制规则由 D-09 补充 | 2026-11-18 | owner | [单一控制权](architecture-design.md) |
| D-09 | 每会话一个执行线程串行推进播放与可选判定；Pause/Resume 明确、重复合流且幂等，独立暂停原因分别解除，必要参与方均确认才成功；旧操作与回执不改变当前局 | 2026-11-19 | owner | [控制语义与验收](architecture-design.md)；设备、UI/渲染与 I/O 保留各自线程归属 |
| D-10 | Player 与测试共用可独立于窗口/CLI/SDL 主循环编译的会话控制实现；拆出的运行时判定内核在头文件和链接上仅依赖 `core`，作者编译器另按 D-21 分离 | 2026-11-19 | owner | [构建与双用途行为验收](architecture-design.md)；未来目标，实现未验 |
| D-11 | 按行为迁移或复用 `ExecutionHost`，验证同一生产实现后撤销旧控制权；仅具名且不能同步迁移的消费者可临时转发到共享实现 | 2026-11-19 | owner | [迁移与退役边界](architecture-design.md) |
| D-12 | 设备中断且位置/结算边界不明时暂缓判定；证实同局且安全后由玩家恢复。首版无法安全确认时由玩家选重开或退出，中断不自动清局或记 Miss | 2026-11-19 | owner | [安全续局与故障边界](architecture-design.md) |
| D-13 | 目标 `R=Retry`，Reload 移到另一键，F5 仅为推荐；当前源码 `R=Reload` 尚未迁移。Retry 复用已加载内容与健康音频，安全就绪后从零直接开播。Reload 先停 A 并保留，B 完整验证、加载、初始化和必要设备接入成功后才切换；旧操作不再引用 A 后释放，B 从零暂停待 Play。失败保 A 停播，供显式 Retry | 2026-11-19 | owner | [D-13 及局间保留续议](architecture-design.md)；成功切换后的故障按实际阶段处理 |
| D-14 | 纯画面重建保留当前内容与本局进度；未暂停时不额外控制，已暂停时由唯一控制者只解除画面原因。普通播放按同局、原播放状态和其他原因决定续播；实时游玩显示冻结画面后等显式 Resume，继续键不参与判定 | 2026-11-19 | owner | [D-14 及恢复续议](architecture-design.md)；合并首次登记与续议，历史见 §12 |
| D-15 | Stop 与自然结束共用一次结束流程：已开玩实机局交付独立有效的真实成绩（含零分），随后清理局状态；未开玩与普通播放不交付成绩。保留不可变内容与健康音频供 Retry，至会话结束或成功替换才释放。有音轨自然结束与 Stop 截止规则按附页执行 | 2026-11-19 | owner | [D-15 及局间保留续议](architecture-design.md)；2026-11-20 续议取代每局卸载，具体验收见附页 |
| D-16 | 普通无来源启动显示可关闭的「无资源」窗口；显式来源失败须诊断。选谱与判定由启动参数表达，保留显式开发/测试入口 | 2026-11-19 | owner | [普通启动与无资源窗口](architecture-design.md#d-16-普通启动与无资源窗口) |
| D-17 | 格式与玩法语义独立演进；只对具名且验证过的发行组合分别承诺普通播放与显式判定，未知必需能力诊断并拒绝；保留对应成品与判定回归样例 | 2026-11-19 | owner | [格式版本与发行兼容](architecture-design.md#d-17-格式版本与发行兼容)；沿用 ADR 0041 |
| D-18 | 按消费者划分默认/实验构建及打包工具，共用 Player 源码、会话、时钟与渲染路径；正式 CXC 包校验共享，候选解码显式隔离，默认构建拒绝实验请求 | 2026-11-19 | owner | [用途构建与整包校验](architecture-design.md#d-18-按用途编译-player)；归入 D-05 R1 |
| D-19 | 官方 Player 与未来 Studio 是 SDK 参考应用。开发者可用公开 SDK 编写自己的播放器和编辑器。 | 2026-11-19 | owner | 默认 SDK 长期维护，实验接口先由独立宿主试用。[SDK 示例与宿主试用](architecture-design.md#d-19-sdk-示例与宿主试用) |
| D-20 | 对仍正式支持的规则复现逐音符结果与结算。已证实的错判按原规则修正，调整玩法行为时区分新旧规则。 | 2026-11-19 | owner | [旧谱判定兼容](architecture-design.md#d-20-旧谱判定兼容) |
| D-21 | 独立 `timing` 仅依赖 `core`，Chart 与玩法作者编译器依赖 `timing`；拆出的运行时判定内核在头文件和链接上仅依赖 `core`，消费编译好的 tick。 | 2026-11-20 | owner | [时间基准、模块边界与待定事项](architecture-design.md#d-21-谱面表现与判定共用拍位换算) |
| D-22 | 将谱师自定义判定等级列为下一版判定内核目标。 | 2026-11-20 | owner | [能力边界与验收方向](architecture-design.md#d-22-谱师自定义判定等级) |
| D-23 | 作者声明宽高比；SDK 在宿主实际像素区域内计算并传递内容矩形，旧谱缺省沿用原投影行为。 | 2026-11-20 | owner | [比例、呈现接线与待审取整](architecture-design.md#d-23-谱面画面比例与宿主尺寸) |
| D-24 | Playback 可选接入判定，当次推进直接交付新增结果；默认与实验构建的共享公开类保持同一定义。 | 2026-11-20 | owner | [当次输出与 SDK 扩展边界](architecture-design.md#d-24-sdk-判定接入与实验扩展)；具体接口与迁移待审 |
| D-25 | 旧渲染入口逐项替代并按适用兼容门禁退出；Packed CRC 采用前 92 字节算法，CXC playback encoding 按具名 profile 固定，两处格式合同已更正。 | 2026-11-20 | owner | [兼容入口与格式合同收敛](architecture-design.md#d-25-兼容入口与格式合同收敛)；实现与互操作验证另记 |

本表只保留已接受方向的摘要，详细边界与验收以所链附页为准；D-13/D-15 的局间保留按续议执行，被取代的历史安排在 §12 标明。

写回原则已获接受：写入前报告具体已知数据损失，覆盖决定归 Studio／第三方编辑器；具体接口、损失表示和目标版本仍待审，见[提案 P5](simplification-proposal.md#p5-写回与诊断)。

以上只列有证据的已决项。共享 `PlaybackSession` 预览路径的架构设想见[架构总览](../../../architecture/OVERVIEW.md)。Studio 的未来设计边界见下方；不纳入本阶段功能或实现授权。RR-F510、版本漂移策略与 particles/debug 清理已在 2026-11-17 按 owner 决定处置，见变更记录与 findings。

### 排期与实施状态

D-05 只限定 RR-3 的 R1/R5/R6 范围，R2/R3/R4 继续延期；D-06–D-25 的已接受设计不改变该范围或授权状态。RR-4 是独立任务，须先取得 RR-2 的 candidate 合同裁定并完成 R1 隔离，再满足 §3 门禁，不能据其列入看板就启动实现。D-16 的 Player 外壳事项仍单列跟进，实施遵守相同门禁。本轮授权是订正文档冲突、补齐提案，P1–P6 新增技术推荐仍待审，重构实现未开始、未验收。

### 设计开放项

以下是设计细节或后续方向，不能解释为排期批准；讨论与证据在[架构设计附页](architecture-design.md)持续积累：

- Player、Playback 与判定模块的具体职责/API 落点及线程绑定仍待设计。D-11 已定按行为迁移/复用 `ExecutionHost`、验证后撤旧控制权及有限的临时转发条件；具体消费者、迁移与验证安排属于实现细节。D-12 已定设备中断期间的暂缓判定、安全恢复条件及首版无法确认时的玩家处置；具体确认机制属于实现细节。D-09 的控制语义与 D-11/D-12 的细节见[架构设计附页 §6](architecture-design.md)，均不构成实施授权。
- D-13 已定目标 `R=Retry`，Reload 另键，F5 仅推荐；来源读取、零点启动与保留 A 后准备 B 的边界已接受。待审的是具体 Reload 键、新 API、运行中用途切换、设备安全确认与其他恢复细节，见[提案 P4](simplification-proposal.md#p4-player-与旧控制权)。D-14/D-15 的画面恢复与局间保留按[附页](architecture-design.md)执行。
- Packed、CXT v2、Chart v5 等候选格式的后续状态；用途构建原则见 D-18，具体发行组合仍待决定。
- D-21 已定时间库和运行内核的依赖边界；具体类型归属、头文件/target 拆分、成品装载校验与新时间映射细化仍待审，见[提案 P1](simplification-proposal.md#p1-数据与-sdk)、[P3](simplification-proposal.md#p3-时间与会话时钟)。
- D-23 已定宽高比与内容矩形职责，取整和帧布局迁移待审；D-24 已定当次输出和共享公开类跨构建同定义，具体接口、安装集与旧宏方法迁移待审；D-25 已定旧渲染入口逐项退出和格式合同收敛，兼容范围、版本及互操作证据另取。详见[附页 D-23–D-25](architecture-design.md#d-23-谱面画面比例与宿主尺寸)及[提案](simplification-proposal.md)。
- 写入前的损失报告与编辑器覆盖决定归属已接受，具体损失接口、目标版本和诊断合同仍待审，见[提案 P5](simplification-proposal.md#p5-写回与诊断)。
- 实时判定约束之外的玩法语义。
- Studio 未来预览、首版从头实机与跳段 Miss 边界已记录在[架构设计附页](architecture-design.md)；宿主定位按 D-19，会话与局间寿命按 D-13/D-15。后续用途切换及 no-fail 仍属未来方向。

## 5. 假设、约束与风险

| 假设/风险 | 失效信号 | 缓解/回滚 | 责任 |
| --- | --- | --- | --- |
| 历史研究事实须逐项核对后才能用于当前工作树 | 对象源码、schema、构建或安装边界相对历史基线变化 | 记录有限差异范围；变更对象重取证，旧通过不外推；见[提案复核记录](findings.md#12-提案复核记录2026-11-21) | RR-0/RR-1 |
| findings 的“已核实/未复核”状态足以支撑处置 | 未复核条目被直接用于删代码或改合同 | 先补证并写入报告；未补证不动 | RR-2/RR-3 |
| 减重不会破坏受限退出边界与历史证据 | 证据 manifest 路径变化、报告被改写、回归失败 | 删除/合并清单 + 合同清单 + 回归证据；失败回滚新增入口 | RR-3/RR-6 |
| RR-2 的合同裁定能及时给出 | 某条目长期无 Spec/ADR 裁定 | 先标“未实现/待裁定”，不把结构改动塞进同一批 | RR-2 |
| 文档门禁通过等于文档正确 | 机械检查通过但正文仍与实现不符 | 批次报告人工复核 + 事实检查 + 阴性对照 | RR-1/RR-5 |
| 默认构建的测试覆盖足以证明回归 | 关键测试只在 candidate 宏下注册或零生效 | 按报告记录的手工复现命令；在报告中显式写未覆盖 | RR-2/RR-6 |

**不可逆点**：证据套件相对路径已进入验收哈希账本（`binding.py`、`prepare_*` 的 glob、`etw_safety` 逐文件封包哈希），移动目录会让历史验收记录永久失配；`docs/stage_reports/**` 与 `docs/archive/**` 是冻结证据，不重写历史结论、不回填自身 SHA；SDK `0.9.1` 的安装头/对象布局与 C ABI 一旦发布不能静默改变；格式 wire、语义 identity 与稳定诊断码一经接受即成为互操作契约，变更必须先有 ADR/Spec 并以 fixture/hash 固化；`findings.md` 的 ID 与结论列不删除、不重编号，处置只追加。
## 6. 工作分解与依赖
### 任务看板

| ID | 任务 | 前置 | 主要产物 | 完成定义 |
| --- | --- | --- | --- | --- |
| RR-0 | 架构研究（已完成） | 无 | [研究](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-research.md)、[证据](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-evidence.md)、findings §6 | 8 个专题有结论；6 个选项 R1–R6 与建议路线有证据；不改产品代码 |
| RR-1 | 状态与文档漂移 | 无（可与 RR-2/RR-3 并行） | 当前文档订正、历史基线标注、版本漂移处置 | 现行文档与 `CuexisVersion.cmake`、`vcpkg.json`、分支事实不冲突；历史值标为历史 |
| RR-2 | 合同与诊断收敛 | 每条先有 Spec/ADR 裁定 | 合同修订、focused tests/fixtures、findings 处置 | 每条有裁定或“未实现”标签；同一失败一个稳定码；静默成功消除或登记 |
| RR-3 | 架构减重 | RR-0 完成 + D-05 范围裁定 + 逐项满足 §3 入场门禁 | 对象决定、删除/合并清单、合同影响清单、回归证据 | 每个对象有具名决策 + 合同清单 + 回归；不破坏受限退出边界 |
| RR-4 | 公共面收缩 | RR-2 对 candidate 入口的裁定；按 D-18 完成 R1 构建隔离 | 安装头/安装集/版本权威订正 | production 安装头不暴露 candidate 入口；external consumer/reference host 门禁通过 |
| RR-5 | 文档与注释减重 | 无（可与 RR-2/RR-3 并行） | 单一权威、过程文档处置、注释对齐与检查 | 机械断言类内容有测试保护或被删除；注释失真样例清除并加入检查 |
| RR-6 | 退出与交接 | RR-1–RR-5 退出或有明确 deferral | 退出报告、删除/合并清单、Stage 7.5/Stage 8 交接 | owner 接受退出报告；CURRENT_STATUS/AGENTS/ROADMAP/索引同步；不声称未取得的结论 |

### Plan 文档拆分试点（规划中，尚未实施）

先在 Stage 7-RR 试点拆分规划文档；试点对象为 RR-0–RR-6。目标结构：

```text
stage_plans/active/<stage>/
  plan.md          # 总览：定位/目标/边界/前置基线/任务索引（ID+链接）/验收入口
  schedule.md      # 调度图：ID、波次、前置、并行/独占
  tasks/T-xx.md    # 每任务一卡，子代理可独立执行
  acceptance.md    # 阶段验收矩阵，引用任务 ID
```

- `schedule.md` 只允许四列：`波次 | 任务 ID | 前置 ID | 可并行/独占`；不得写任务名、描述、状态、owner 或验收正文，可附 Mermaid 依赖图。每个任务 ID 在表中恰好出现一次；前置只能引用更早波次中的任务；独占波次只能有一个任务。依赖关系只以 `schedule.md` 为权威，任务卡不得另写 `depends_on`。
- `tasks/T-xx.md` 必须自包含，包含目标、输入、写域、步骤、任务级验收、证据与停止条件；不复制阶段验收规则。
- `acceptance.md` 只定义阶段验收边界与矩阵、证据清单和 deferral，并引用任务 ID；交叉核验要求主 agent 重跑验收，另由独立子代理抽查并执行阴性对照，不重复任务卡细项。
- `check_plan_schedule.py` 目前只是拟建检查器，不表示已存在或已实现；拟校验 ID 覆盖、前置存在及波次顺序、独占单任务和调度表不含任务正文。
- 试点完成拆分后，以真实委派验证“只给任务卡即可完成对应任务”。此项尚未执行；当前计划尚未拆成上述文件，也不声称任务卡独立性已验证。
- `stage_plans/future/**` 继续使用提纲模板；阶段启动前不创建 `tasks/` 或 `schedule.md`。

关键路径：RR-0 → D-05 范围裁定（已完成）→ RR-3〔仅 R1/R5/R6，逐项满足 §3 入场门禁〕→ RR-6。RR-4 独立以前置 RR-2 candidate 合同裁定与 R1 隔离为门槛；RR-1、RR-5 可与 RR-2/RR-3 并行，RR-2 与 RR-3 同对象须先定语义再改结构。RR-0 与 RR-1/RR-5 的机械层已完成；其余实现继续按各自门禁和授权推进。
## 7. 分批细化
### RR-0：架构研究（已完成）
- **范围与任务**：只读研究依赖分层、双产品线、格式线、时间源、渲染路径、会话层次、文档/注释与公共 API 面；汇总实测计数与违规边，提出 R1–R6 与建议路线，条目并入 findings §6。
- **落点与证据**：[研究](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-research.md)、[证据](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-evidence.md)、[findings §6](findings.md#6-架构层发现2026-11-17-研究)。
- **退出条件**：研究/证据报告落盘；选项有证据；owner 未选定前 RR-3 不进代码。
### RR-1：状态与文档漂移
- **范围与任务**：P0/P1 中的版本、分支、阶段归属、格式状态与 SDK 陈述；对齐当前事实，历史基线标为历史，按 owner 决定处置剩余版本漂移；不含合同语义重写。
- **落点与证据**：[CURRENT_STATUS](../../../CURRENT_STATUS.md)、[ROADMAP](../../../ROADMAP.md)、`docs/formats/README.md`、`docs/api/*`、[LIMITS](../../../guides/LIMITS.md)、`check_docs_facts.py`；证据为[文档治理报告](../../../stage_reports/stages/stage-07-rr/2026-11-17-documentation-refactor.md)与 RR-1 退出报告。
- **退出条件**：`check_docs.py`、`check_docs_facts.py`、`gen_docs_map.py --check` 通过；现行文档无事实冲突；历史值显式标为历史；owner 接受版本漂移处置。
### RR-2：合同与诊断收敛
- **范围与任务**：Packed Header CRC/GridDelta、Chart Beat 上限、CXC playback entry、CXT v2 §6 未实现面；`same_tick_collision` 码互换、`catch(...)` 三态、`player.execution.test_only` 单码、`packed.io.*` 双入口；每条先取 Spec/ADR 裁定，再改实现或标“未实现”，补前后 fixture 与 focused tests，处置回写 findings §9。
- **落点与证据**：`docs/formats/*`、`docs/adr/*`、`docs/api/*`；`engine/chart`、`engine/playback`、`engine/judgement` 与 `tests/`；证据为批次报告 + 前后 fixture + findings 状态回写。
- **退出条件**：每条有裁定或“未实现”标签；同一失败只有一个稳定码；“静默成功”路径被消除或显式登记；受影响测试通过，失败/回滚路径不被削弱。
### RR-3：架构减重
- **范围与任务**：仅按 D-05 处理 R1 构建隔离、R5 渲染收敛与 R6 文档减重，R1 方向见 D-18。列删除/合并/下沉清单（对象、服务的 ADR/Spec、受影响测试与证据、移除理由）；逐项满足 §3 门禁并获实施授权后落实、回归并更新合同。产品线、时间源和会话层的长期设计另行审阅，不借本批启动延期项；不与 RR-2 同对象并行。
- **落点与证据**：`CMakeLists.txt`、`CMakePresets.json`、`engine/**`、`app/**`、`docs/architecture/*`、`docs/formats/*`；证据为 RR-3 退出报告 + 删除/合并清单 + 回归测试/预设结果。
- **退出条件**：每个被删/合并对象有具名决策、合同清单与回归证据；不破坏 Stage 7 受限退出边界；不把历史失败改写为通过；未取得 hosted/跨平台证据的结论不声称。
### RR-4：公共面收缩
- **范围与任务**：独立以前置 RR-2 candidate 合同裁定和 R1 隔离为门槛，另按 §3 取得实施授权。核查 candidate 安装面、`cuexis_internal/`、零消费者符号与版本权威；具体迁移按 D-24 保持共享公开类同定义，并验证 external consumer/reference host；不决定 SDK minor 与发行。
- **落点与证据**：`include/cuexis/playback/*`、安装集与 `cmake/VerifySharedExports.cmake`、`cmake/VerifyArchitecture.cmake`、`cmake/CuexisVersion.cmake`、`examples/reference_host/*`、`docs/api/*`；证据为安装头 diff + 构建/外部消费者日志 + RR-4 报告。
- **退出条件**：production 构建的安装头不暴露 candidate 入口；缩面不影响 external consumer/reference host 门禁；版本只有一个权威源；满足 D-24 与[ADR 0042](../../../adr/0042-stage-6-productization-boundaries.md) 的共享公开类跨构建同定义约束，取得适用版本和迁移验收。
### RR-5：文档与注释减重
- **范围与任务**：同一合同只保留一处权威、过程文档按 owner 政策处置、注释与实现对齐；合并/去重合同表，按决定处置过程文档，清理失真注释，为可机械断言内容加保护，复核 7 处已修假断言不复发；不做无证据删文档。
- **落点与证据**：`docs/formats/*`、`docs/api/*`、`docs/architecture/*`、`docs/guides/*`、代码注释、`tools/check_docs.py`、`tools/verification/check_docs_facts.py`；证据为 RR-5 报告 + 检查器输出 + 注释抽样记录。
- **退出条件**：机械断言类内容有测试保护或被删除；注释失真样例被清除并加入检查；owner 接受过程文档处置。
### RR-6：退出与交接
- **范围与任务**：汇总处置、残留缺口、Stage 7.5/Stage 8 交接；逐条列 P0/P1 处置，P2 写归属阶段与恢复条件，汇总删除/合并清单，更新 CURRENT_STATUS、AGENTS、ROADMAP、索引与 HANDOFF，写退出报告；不新增修复。
- **落点与证据**：`docs/stage_reports/stages/stage-07-rr/` 的退出报告、[CURRENT_STATUS](../../../CURRENT_STATUS.md)、[ROADMAP](../../../ROADMAP.md)、[HANDOFF](../../../HANDOFF.md)、阶段索引；证据为退出报告 + findings 最终状态 + 第 9 节命令输出。
- **退出条件**：owner 接受退出报告；P0/P1 全部有处置；不声称未取得的 hosted/跨平台/GPU/设备结论；文档门禁全过。
## 8. 统一任务交付模板
每个任务/PR 必须包含：

```text
Scope / Contract / Implementation
Positive evidence / Negative evidence
Compatibility and rollback evidence
Changed limits, diagnostics, capability
Known gaps and next-task handoff
```

没有失败证据、兼容证据或明确 handoff 的任务不算完成；只改文档的任务必须写“只改文档”并列出所跑检查。
## 9. 验收标准
每个批次退出时同时具备：发现条目的具名处置（fixed / deferred with reason / rejected with evidence）；实现与 focused tests（或“只改文档”的明确说明）；失败与回滚路径不被本次修改削弱；文档、状态与交接同步。本阶段整体退出还需：P0/P1 全部有处置，P2 的 deferred 写明归属阶段与恢复条件；有删除/合并清单；受影响测试全绿且改变接受集合的修复附前后 fixture；不声称未取得的 hosted、跨平台、GPU 或设备结论。
### 验收矩阵

| 验收项 | 命令/方法 | 期望 | 证据类型 | 接受者 |
| --- | --- | --- | --- | --- |
| 文档结构与链接 | `python -B tools/check_docs.py` | 退出码 0；无坏链、无缺章节 | 终端输出 | owner/CI |
| 生成物与版本事实 | `python -B tools/check_docs_facts.py` | 退出码 0；版本漂移警告按 owner 决定处置 | 终端输出 | owner/CI |
| 地图新鲜度 | `python -B tools/verification/gen_docs_map.py --check` | 退出码 0 | 终端输出 | owner/CI |
| 空白错误 | `git diff --check` | 退出码 0，无 whitespace error | 终端输出 | owner/CI |
| 代码格式 | `cuexis_format_check`（按 [BUILDING](../../../guides/BUILDING.md)） | 通过 | 日志 | owner/CI |
| 批次报告落盘 | 报告路径存在，且含结论/范围与方法/执行记录/结果与证据/限制与未做/对登记册与合同的影响 | 报告可链接、结论按四档分级 | 报告 | owner |
| findings 状态回写 | 逐条核对 P0/P1 的 fixed/deferred/rejected 与证据链接 | 无“未复核”或“已修复”无证据 | findings diff | owner |
| 代码批次回归 | 按 [BUILDING](../../../guides/BUILDING.md) 执行受影响 target/CTest | 全部通过；失败与回滚证据入报告 | CTest/日志 | owner/CI |
| 外部消费者/参考宿主 | 按 BUILDING 的 external consumer/reference host 门禁 | 通过 | 日志 | owner/CI |
| Player 普通启动（D-16） | 本次 RR 后续实施时，按[架构设计附页 D-16](architecture-design.md#d-16-普通启动与无资源窗口)覆盖四种启动情形 | 具体判据以附页为准，本表不复述 | 启动验收记录 | owner |

### 正例与反例
- 正例：新增/保留模板要求的 H2 后 `check_docs.py` 为 0；反例：删掉“验收标准”章节会失败。
- 正例：findings 条目带报告链接、命令输出与前后 fixture 后标 fixed；反例：只写“已修复”会被退回。
- 正例：证据套件原位保留并被报告引用；反例：移动目录导致历史 manifest 失配，必须回滚。
- 正例：合同修订先有 ADR/Spec 接受；反例：实现先行、事后回写合同会被拒绝。
## 10. 回滚、阻塞与停止条件
1. 合同变更未取得接受：停止受影响实现，保留最小复现，回到最后接受的 revision。
2. 确定性失败（跨编译器/平台差异、未声明浮点、溢出、容器顺序依赖）：阻塞该批，不以“环境差异”放行。
3. 预算/容量失败：区分内容、稳态、快照与 Replay；不得事后缩小 fixture 或改口径。
4. 安全失败（不可信输入导致未界定循环/IO/过量分配）：稳定拒绝并保留完好的旧加载版本；涉及 D-13 Reload 时仍须停止播放与判定，保留版本不表示继续运行。
5. 集成失败：回滚新增入口，不删除旧路径；旧路径的退出按 [ADR 0041](../../../adr/0041-legacy-format-exit-policy.md) 单独排期。
6. owner 未接受 ADR/Spec/例外：状态保持 planned/active/blocked，不把“待决定”写成已完成。
7. RR-3 需要超出 D-05 范围：停止该对象的删除/合并/下沉，只做研究、文档与 RR-2 的合同裁定，直至 owner 另作决定；不得仅凭 D-06 改变延期项状态。
8. 证据路径移动或已归档报告被改写：立即停止并回滚；不得用更新 manifest 来“修复”历史证据。
## 11. 证据与报告计划

| 报告 | 触发 | 路径 | 模板 |
| --- | --- | --- | --- |
| 报告索引 | 持续 | [README](../../../stage_reports/stages/stage-07-rr/README.md) | 索引页 |
| 架构与实现研究 | RR-0（已完成） | [2026-11-17-architecture-research.md](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-research.md) | `docs/templates/report.md` |
| 架构与实现探索证据记录 | RR-0（已完成） | [2026-11-17-architecture-evidence.md](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-evidence.md) | `docs/templates/report.md` |
| 文档治理重构 | RR-1/RR-5 机械层（已完成） | [2026-11-17-documentation-refactor.md](../../../stage_reports/stages/stage-07-rr/2026-11-17-documentation-refactor.md) | `docs/templates/report.md` |
| 假保证与假实现扫描 | RR-0/RR-2 输入（已完成） | [2026-11-17-false-guarantees-scan.md](../../../stage_reports/stages/stage-07-rr/2026-11-17-false-guarantees-scan.md) | `docs/templates/report.md` |
| 外部审查（59 条） | RR-0/RR-2 输入（已完成） | [2026-11-17-external-findings-review.md](../../../stage_reports/stages/stage-07-rr/2026-11-17-external-findings-review.md) | `docs/templates/report.md` |
| tools/ 目录审计 | RR-1/RR-4 输入（已完成） | [2026-11-17-tools-audit.md](../../../stage_reports/stages/stage-07-rr/2026-11-17-tools-audit.md) | `docs/templates/report.md` |
| 后续批次报告 | 每批退出 | `docs/stage_reports/stages/stage-07-rr/` 下带日期报告 | [report.md](../../../templates/report.md) |

报告只记录实际执行的命令、结果与限制；结论只来自证据报告，批次退出、删除/合并、合同变更、版本漂移豁免、RR-F510、particles/debug 清理必须由 owner 接受。不得用报告“下一步”推断当前工作；当前任务只看 [CURRENT_STATUS](../../../CURRENT_STATUS.md) 与 [HANDOFF](../../../HANDOFF.md)。
## 12. 交接与变更控制
- 每批结束或暂停时更新 [HANDOFF](../../../HANDOFF.md)：当前任务、下一步、阻塞与验证状态。计划变更必须先在本表登记；未登记的范围变化不算计划变更。

| 日期 | 变更 | 原因 | 决策依据 |
| --- | --- | --- | --- |
| 2026-11-17 | 按[详细规划模板](../../../templates/plan.md)重写本计划：新增规划问题、任务看板、分批细化、验收矩阵、回滚与变更控制；保留 RR-0–RR-6 语义、目标、排除项与归档来源 | 模板升级为详细规划协议；原计划缺少任务级交付与验收矩阵 | owner 2026-11-17 的模板与工作规则 |
| 2026-11-17 | 登记 D-04/D-05；关闭 RR-F510、版本漂移、particles/debug 三项待决；RR-3 只做 R1/R5/R6，R2/R3/R4 defer | owner 2026-11-17 统一决策 | owner |
| 2026-11-18 | 登记 D-06，区分已定路线与设计开放项；将 RR-0–RR-6 文档拆分试点规则纳入 §6，状态标为尚未实施 | owner 接受产品主线、双用途验收与时间换算方向；拆分方案仅作阶段试点规划 | [架构设计附页](architecture-design.md)；owner 2026-11-18 |
| 2026-11-18 | 登记 D-07，并将 Player 启动时判定请求的已定规则与未决细节写入架构设计附页 | 记录 owner 对显式判定请求及普通播放默认行为的裁定；不改变 D-05、公共 CLI 合同或生产代码 | [架构设计附页 §2、§6](architecture-design.md)；owner 2026-11-18 |
| 2026-11-18 | 登记 D-08，并将 Studio 预览、首版实机与未来跳转约束写入 §4 | 记录会话控制权与已沟通的未来设计边界；不改变 D-05，不将未来边界纳入本阶段功能或实现授权 | owner 2026-11-18 |
| 2026-11-19 | 登记 D-09，确认单会话执行串行化、唯一状态/结果控制权与明确 Pause/Resume 行为 | owner 采纳 D-08 的后续具体方案；保留 D-08 原确认日期与含义，不改变 D-05 范围/实施授权或 Studio 约束 | [架构设计附页 §6](architecture-design.md)；owner 2026-11-19 |
| 2026-11-19 | 登记 D-10，将可复用会话控制及未来构建/行为验收门槛写入附页 | 记录 owner 对 Player 外壳与共享控制实现分离的共识；不改变 D-05 或授权实施 | [架构设计附页 §6](architecture-design.md)；owner 2026-11-19 |
| 2026-11-19 | 登记 D-11/D-12，收窄执行控制迁移与实时游玩设备中断的设计开放项 | 记录 owner 对唯一行为控制者、受限临时转发及不确定期间判定/恢复原则的裁定；不改历史证据、不改变 D-05 或扩大实施授权 | [架构设计附页 §6](architecture-design.md)；owner 2026-11-19 |
| 2026-11-19 | 登记 D-13，明确 Retry 使用已加载版本、Reload 重读同一来源，以及成功替换后从零开局和阶段性故障边界 | 记录 owner 对重开与重载语义的裁定；不改变 D-05 或授权实施 | [架构设计附页 §6](architecture-design.md)；owner 2026-11-19 |
| 2026-11-19 | 修订 D-13 的 Retry/Reload 结果语义；保留前一条登记供追溯，不新建决议号 | 明确 Reload 成功后从零暂停待显式 Play、失败停播停判但保留完好旧版本供显式 Retry；Retry 用已加载版本且安全就绪后从零直接开播；晚期故障按阶段报告，不虚称回滚或新局成功 | owner 2026-11-19；详细验收由[架构设计附页 §6](architecture-design.md)补充；不改变 D-05 或实施授权 |
| 2026-11-19 | 登记 D-14，确认纯渲染资源重建恢复已加载版本和本局进度的边界 | 区分纯画面恢复与会读取来源、处理音频的现有 Rebuild；不裁定画面恢复后的自动/手动恢复，不改变 D-05 或授权实施 | [架构设计附页 §6](architecture-design.md)；owner 2026-11-19 |
| 2026-11-19 | 补充 D-14 续议的画面中断恢复规则，保留首次登记原记录 | 确认渲染器只报告当前会话画面可显示，由唯一会话控制者清除画面不可用原因；普通播放按原播放状态、同局身份及其他暂停原因决定自动续播，实时游玩等冻结画面显示后显式 Resume，音频中断仍满足 D-12。首版无时长阈值、倒计时或配置项；不扩大 D-05、不改 API/线程或实施授权 | owner 2026-11-19；[架构设计附页 §6](architecture-design.md) |
| 2026-11-19 | 登记 D-15，明确 Stop 与自然结束共用正常结束处理，以及游玩结果交付后卸载谱面的顺序 | 记录 owner 对结束触发/时点、结果交付边界、普通播放卸载及故障停播分类的裁定；导出仅指向调用方提供结果，承载/UI/持久化未定，未来共用规则；不改变 D-05 或授权实施 | owner 2026-11-19；每局卸载安排已由 2026-11-20 续议取代，见[附页 D-15](architecture-design.md) |
| 2026-11-19 | 补充 D-15 自然结束条件、Stop 竞合和职责边界，保留首次登记 | 普通播放只由播放路径报告音轨自然结束；实时游玩须等待播放与判定路径报告的两项本局完成事实，且不得截断合法尾段或 EOF 批量 Miss。Stop 以受理时成绩走共用结束流程并保证竞合只处理一次；未裁定无音轨终点，也不新增 API、固定等待或谱面字段 | owner 2026-11-19；[架构设计附页 §6](architecture-design.md)；候选 RPA 尾段参考见[rpa-execution-contract §7](../../../formats/rpa-execution-contract.md#7-需要测量的后续门禁)；Stop 截止口径已由同日复审补充 |
| 2026-11-19 | 复审补充 D-15，明确 Stop 的本局截止、已生效判定、开玩门槛及结果交付/卸载顺序，并隔离竞合与旧局通知 | 收窄“受理时成绩”的当前含义：按控制者确定的截止位置处理已接收且届时应生效的输入/判定；实际开玩才交付成绩，包括零分。其他结束边界见附页 §6；不改变 D-05 或实施授权 | owner 2026-11-19；[架构设计附页 §6](architecture-design.md)；其中每局卸载安排已由 2026-11-20 续议取代 |
| 2026-11-19 | 登记 D-16：普通启动无来源即显示可关闭的「无资源」窗口，来源失败须诊断；选谱与判定请求由启动参数表达 | 确认默认内容选择边界，保留显式开发/测试入口；不改变 D-05、§3 门禁或实施授权，并路由本次 RR 后续 Player 外壳工作与验收 | owner 2026-11-19；[架构设计附页 D-16](architecture-design.md#d-16-普通启动与无资源窗口) |
| 2026-11-19 | 登记 D-17：格式版本与发行兼容 | 明确格式有效性、直接版本/能力依赖和按具名验证组合分别承诺播放模式的原则；保留 ADR 0041 决定，不预选发行组合或载体，不改变 D-05、排期或实施授权 | owner 2026-11-19；[架构设计附页 D-17](architecture-design.md#d-17-格式版本与发行兼容) |
| 2026-11-19 | 登记 D-18：按用途编译 Player | 确认默认与显式实验构建、打包工具的职责边界及共享执行路径；具体 target 与 SDK 安装面/ABI 不由 D-18 裁定，发行组合由 Stage 8 决定，不改变 D-05 或授权实施 | owner 2026-11-19；[架构设计附页 D-18](architecture-design.md#d-18-按用途编译-player) |
| 2026-11-19 | 登记 D-19 | 确认官方参考应用、第三方接入和实验接口纳入默认 SDK 的条件。 | owner 2026-11-19。[架构设计附页 D-19](architecture-design.md#d-19-sdk-示例与宿主试用) |
| 2026-11-19 | 登记 D-20 旧谱判定兼容 | 记录发行时保留的判定样例、错判修正依据和历史成绩的处理。 | owner 2026-11-19。[架构设计附页 D-20](architecture-design.md#d-20-旧谱判定兼容) |
| 2026-11-20 | 登记 D-21：谱面表现与判定共用拍位换算规则 | 同日续议选定独立谱面时间库；D-05 继续有效。 | owner 2026-11-20；[架构设计附页 D-21](architecture-design.md#d-21-谱面表现与判定共用拍位换算) |
| 2026-11-20 | 登记 D-22：谱师自定义判定等级 | 记录下一版内核目标与兼容验收方向。 | owner 2026-11-20；[架构设计附页 D-22](architecture-design.md#d-22-谱师自定义判定等级) |
| 2026-11-21 | 订正 D-06/D-10 的运行内核限定、D-13/D-15 续议与按键台账，补登 D-23–D-25 和已接受写回原则；同步范围、状态与历史基线边界 | 本轮先对齐已接受共识与文档冲突；保留首次决定的详细约束与历史登记，P1–P6 新技术推荐仍待审，实现未验 | owner 本轮文档订正授权；[架构设计附页](architecture-design.md)、[提案复核记录](findings.md#12-提案复核记录2026-11-21) |
| 2026-11-21 | 补齐 P1–P5 五项设计缺口的待审推荐，同步复核与交接状态 | owner 授权补完设计；新增选择仍待审，实施遵守 D-05 与原门禁 | [提案](simplification-proposal.md#整体推荐方案待审)、[提案复核记录](findings.md#12-提案复核记录2026-11-21) |

## 13. 排除项与旧路径
- **不做纯行数拆分**：owner 已取消；行数预算只作为 findings §4 的事实记录，不为凑预算拆文件。
- **不做 Stage 7.5 能力演进**：不实现 S7B-0–4 / S7C-1/2，不做 GE-R 性能整改与预算测量。
- **不发布、不合并、不提升 SDK minor**：稳定 C ABI 归 Stage 14；不把 preview 承诺扩大。
- **不重跑、不改写历史失败与未认证范围**：不提高 caps，不补造证据，不把旧失败改成通过。
- **不修改判定窗口、格式 wire 布局或 identity 语义本身**：若必要，先立 ADR/Spec 修订，由 owner 接受后另行排期，不塞进重构批次。
- **不把复核发现写成已修复**：本计划登记问题与决策，不是修复声明；修复结论只写入带日期的报告。
- **不移动 tools 证据套件目录、不改写证据 manifest 路径、不重写已归档报告**；证据套件原位冻结。
- **旧路径**：[stage_plans/legacy-paths.md](../../legacy-paths.md)只登记旧路径映射，不在本计划重写合同；Stage 7 的 completed 计划与报告保持原位。
- **旧格式退出**：Chart v1–v4、CXT v1、CXC v1 等由 [ADR 0041](../../../adr/0041-legacy-format-exit-policy.md) 约束；不缩短兼容窗口，除非 owner 另有决定。
