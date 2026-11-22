# Cuexis Current Status

<!-- cuexis-meta
id: STATUS
status: current
authority: owns
current_stage: Stage 7-RR
current_item: stage_plans/active/stage-07-rr/plan.md
current_register: stage_plans/active/stage-07-rr/findings.md
-->

状态：current

更新日期：2026-11-22

当前任务、下一步与阻塞见 [交接页](HANDOFF.md)。

本文只维护当前阶段、正在进行的任务和未解决的问题。实施与测量细节见带日期的阶段报告。

## 当前任务

- **当前为 Stage 7-RR：Review and Refactoring After Stage-7**：owner 要求把 2026-11-17 全项目只读复核
  （10 个并行子代理按模块审查 + 抽查）的发现登记为一个位于 Stage 7 与 Stage 7.5 之间的独立阶段，
  路线为 Stage 7 → Stage 7-RR → Stage 7.5 → Stage 8，后续编号不变。计划见
  [Stage 7-RR 计划](stage_plans/active/stage-07-rr/plan.md)，逐条问题、证据状态与拆分预算事实见
  [发现登记册](stage_plans/active/stage-07-rr/findings.md)。登记结果为 P0 两项（本文件的日期版本与
  分支／PR 陈述过期，已就地订正）、P1 一批（文档-实现合同、格式互操作、诊断码与主机构建面）与
  P2 一批。owner 同日确定本阶段方向为**架构减重重构**（现行"2 条产品线 × 5 条格式线 × 3 套时间 ×
  2 条渲染路径"过于臃肿），并取消超行文件拆分（全仓仅 1 个 `.cpp` 超 800 行）；
  8 份架构研究已于同日完成，结论与 6 个减重选项见
  [架构与实现研究](stage_reports/stages/stage-07-rr/2026-11-17-architecture-research.md)；
  RR-3 范围已按[计划 D-05](stage_plans/active/stage-07-rr/plan.md#4-已确认方向与决策台账)限定为
  R1/R5/R6，R2/R3/R4 延期；本轮已订正文档并补齐提案。
  两名 Astra [首次独立复审](stage_reports/stages/stage-07-rr/2026-11-22-astra-executor-review.md)及其[九项修订复验](stage_reports/stages/stage-07-rr/2026-11-22-astra-review-resolution.md)保留当时范围。随后独立 checkout 复审发现合同接线与任务检查仍有缺口，当前处置、实际验证与限制见[本轮报告](stage_reports/stages/stage-07-rr/2026-11-22-checkout-review-resolution.md)。
  生产未实施；从 [T-01](stage_plans/active/stage-07-rr/tasks/T-01.md)核验修订输入，再按[启动授权](stage_plans/active/stage-07-rr/plan.md#唯一派单授权入口)执行，`NEXT` 包独立采用。
  上一轮完成了[候选合同与任务](stage_plans/active/stage-07-rr/design-readiness.md)对齐、四组冷启动复验及主代理最终组合文档检查；
  当时的结论、通过记录和输入封存入口保留在[上一轮报告](stage_reports/stages/stage-07-rr/2026-11-22-design-readiness-review.md)。
- **文档治理第一批已完成（2026-11-17）**：文档入口新增生成式路由页 [docs/MAP.md](MAP.md)；
  170 篇合同/计划、424 篇证据报告、70 篇提案/示例/历史文档带 `cuexis-meta` 元数据块；
  `AGENTS.md` 顶部改为生成状态块；新增 `tools/verification/gen_docs_map.py`（生成 MAP 与 AGENTS 块）
  与 `tools/verification/check_docs_facts.py`（生成物新鲜度与版本字面量），已接入
  `tools/check_pre_push.ps1`。规则见 [DOCUMENTATION_POLICY.md](DOCUMENTATION_POLICY.md)；
  批次报告见[文档治理重构](stage_reports/stages/stage-07-rr/2026-11-17-documentation-refactor.md)。
  `docs/stage_reports/**` 与 `docs/archive/**` 为证据、非权威；结构保持不变。
- **假保证与假实现扫描已完成（2026-11-17）**：7 个无上下文子代理按模块比对「承诺 vs 实现」，
  48 条候选、26 条经复核确认、23 条未复核分类。最高危为 `ChartWriter` 保存丢 timing/模板/keyframe
  与三个恒为常量的诊断字段；报告见[扫描结果](stage_reports/stages/stage-07-rr/2026-11-17-false-guarantees-scan.md)。
  **修复未开始**，处置顺序见报告末节。
- **R4／Stage 7-RPA 已带限制退出**：owner 在审查九项推荐后接受裁决方向，O3 退出裁定已完成。
  PD-5／Stage 7-PD 的受限退出及原 Stage 7A 收尾包交接同时获接受；原失败、未认证范围和 testOnly 保留。
  唯一关闭及遗留责任入口见[受限退出与缺陷交接](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-14-limited-exit-and-defect-handoff.md)。
- **历史实现基线验证已完成，本次发布候选独立绑定**：已验证代码为 `d1951326b0f2683801fa5fe168f09c8dac43f957`，
  tree 为 `55ff54c9160c52de3cdb318687d5fd6bb8ba9623`。F1 十一行 local_pass、13,632 项执行、
  26 项 skip、0 失败；同 SHA 三平台 F2 及当时适用版本门通过。其日期版本为 `26.11.13-1`（历史），
  不代表本日新触发的 live 版本门通过。P5／设备正式测量仍绑定 `ce2a9a69`，原封存包不改。
  那轮发布（2026-11-14）只包含关闭／Stage 7.5 规划、文档检查器及日期版本 `26.11.14-1`（历史）；没有产品源码、SDK、
  profile 或 caps 变化。当前工作树日期版本已提升为 `26.11.18-1`（UTC 2026-11-18），本轮只有文档登记，
  不外推旧十一行 F1 或旧 hosted 结果到新日期版本。
- **P7 听音／O2 实体操作已获主观范围接受**：owner 本次反馈“没有出现异常”。
  不外推未提供细目的设备拔插、仪器或精确 AV 数值；此前持续降帧和卡顿记录保持。
- **Stage 7.5：Gameplay Evolution 仍为独立规划**：原 7B+ 已完整移出 Stage 7，计划和报告目录为
  `stage-07-5`。能力实施、局部重构与性能稳定独立管理，产品实施未开始；入场前置增加
  “Stage 7-RR 当前计划范围完成并取得 owner 接受”。

当前工作树为 `E:/Cuexis-worktree`，分支为 `stage-7-rr`；HEAD 以 `git rev-parse --short HEAD` 为准，不在此页固化。
PR #44 已于 2026-11-14（`564e0d90`）合并，PR #45 已于 2026-11-16（`aac83e5e`）合并；
`codex/r4-closeout` 只作历史分支保留。实施交接见 [HANDOFF](HANDOFF.md#下一步)；未取得 merge／release 授权。
Stage 7 的历史发布证据与评论回读仍保存在 `out/evidence/stage7-publication-20261114/`，
原样保留，不为回填自身 SHA 改写旧候选。
逐批历史与原始证据见[Stage 7 报告索引](stage_reports/stages/stage-07/README.md)及
[证据清单](stage_reports/stages/stage-07/05-rpa-new/evidence-catalog.md)。

## 阶段状态

| 阶段 | 当前状态 | 入口 |
| --- | --- | --- |
| Stage 7A | S7A-3/4、5/6、7/8 既有受限功能及原 S7A-9 收尾包已随 RPA 获受限交接接受，不另回挂收尾任务；生产与未认证范围延期 | [Stage 7A 计划](stage_plans/completed/stage-07/01-a/plan.md) |
| Stage 7-RPA | R1–R4 已退出；R4 为 owner 接受的带缺陷／未认证范围退出，F3 生产仍延期，testOnly 保留 | [整改计划](stage_plans/completed/stage-07/05-rpa-new/plan.md)、[关闭与交接](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-14-limited-exit-and-defect-handoff.md) |
| Stage 7-PD | 带限制退出及遗留交接已接受；PD-5 历史“未通过但定案”保持 | [PD 计划](stage_plans/completed/stage-07/04-pd/plan.md) |
| Stage 7 | 按迁出原 7B+ 后的既有范围受限收束；历史失败、未认证范围与 testOnly 不改 | [Stage 7 计划](stage_plans/completed/stage-07/plan.md) |
| Stage 7-RR：Review and Refactoring After Stage-7 | RR-0 与文档治理第一批完成；D-05 限定 RR-3 为 R1/R5/R6，R2/R3/R4 延期；独立 checkout 复审缺口按本轮报告处置，生产未实施 | [计划](stage_plans/active/stage-07-rr/plan.md)、[可实施性索引](stage_plans/active/stage-07-rr/design-readiness.md)、[本轮处置](stage_reports/stages/stage-07-rr/2026-11-22-checkout-review-resolution.md) |
| Stage 7.5：Gameplay Evolution | 独立阶段规划与交接准备；继承受限基线，产品实施未开始；入场前置含 Stage 7-RR 接受 | [独立计划](stage_plans/active/stage-07-5/plan.md) |

Stage 0–6 已完成；Stage 7 按拆分后的范围受限收束，原高级能力不回写为已实施；Stage 7-RR 为当前阶段；
Stage 7.5 未开始实施；Stage 8–14 尚未开始。
阶段依赖与后续安排见[Stage 7 总计划](stage_plans/completed/stage-07/plan.md)、
[Stage 7-RR 计划](stage_plans/active/stage-07-rr/plan.md)和[路线图](ROADMAP.md)。

## 当前遗留问题与生产关闭前置

- **输入累积降帧与状态复制／保活**：已定位 Gameplay evaluation 深复制、累积输入历史复制和 Replay
  历史状态保活；密集实体输入诊断末段约 120 FPS。原明显卡顿、renderer submit 差额及长期影响未全部解释。
  整改由独立 Stage 7.5 的 GE-R 在能力工作线后集中处理；期间若阻断功能实施只做必要最小修复，
  不得外推高频输入／长活动性能保证。
- **计时重复性限制保持**：普通 host 外层、P5 Close/load_host、P3 三项外层、capture-4 及 Seek
  fallback/cold 仍 inconclusive，本轮停止补跑。P5 已用 4/5，第五次未用；旧 CPU/device/P8 台账不重置。
- **完整内存／GPU／P8 未认证**：完整 ownership/lifetime 图、原点超额处置、prepare-only／释放边界、
  n2048 residency 与 GPU observer 准入仍缺；正式 memory/P8 qualification 继续 blocked，原 caps 保持。
  P8 额外 high 调用与 controller 来源缺口按历史例外结案，不追认、不恢复额度、不补造原字节。
- **原 P7 自动门仍有失败与缺证据**：旧 DFJK 40 分／19 hit／2 miss 未达 46／21／0；
  normal 三窗与适用 ExecutionHost ≥600 秒活动证据缺失。按未认证范围延期，新课程不替换原失败；
  音乐 retention 适用性独立。
- **F3 校准与生产参数延期**：保持 testOnly；代理准备逐 profile 的参数、设备／规模／retention、
  余量与超额行为，owner 在实际校准及证据齐备后接受。精确 AV 等未证实项目在需要对应保证时补证。
- **既有历史限制继续有效**：PD-0 慢态、R0 partial／旧 Q4 inconclusive、B5 capture_gap／重复 Pause、
  CXC 现场持有者未知等不因退出视为已修复。40K 不作为 240 FPS 适用规模；登记容量及既有例外不外推。

以上均已作为退出限制处置，不是原门全部通过。逐项责任、证据及恢复条件集中保存在
[受限退出与缺陷交接](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-14-limited-exit-and-defect-handoff.md)，
后续责任与实施入口归[Stage 7.5](stage_plans/active/stage-07-5/plan.md)，
扩大保证范围或生产接受前须取得对应新证据，不重写旧结果。

## 产品边界

Cuexis 由可嵌入的 Playback SDK、独立 Cuexis Player 和 Cuexis Studio 构成。
外部宿主使用公共 Playback API；RuntimeSession、World、EnTT、SDL 和 OpenGL 属于内部或可选实现。

产品决策见[ADR 0027](adr/0027-playback-sdk-product-boundary.md)，公共接口见[API reference](api/README.md)。

## 当前格式与 SDK 合同

- 当前工作树 SDK API 为 **`0.9.1` preview**，日期构建为 **`26.11.18-1`**（UTC 2026-11-18 提升；
  此前 `26.11.14-1` 与 `26.11.13-1` 为历史构建身份，不作为当前值引用）。
  历史实现点 `d1951326` 的 F1/F2 绑定 `26.11.13-1`；`26.11.14-1` 那次关闭落档形成独立发布候选，
  实际版本门与 hosted 结果按当时记录绑定核验。
  不把后继 SHA 写成旧十一行 F1 的受验点，原通过不外推为发行结论。
  R3 来源绑定新增 candidate 类型和 `StepTarget` 字段，消费者须整体重建，不能混用旧二进制。
  PD-4 历史基线仍为 0.9.0。版本决定见[版本政策](guides/VERSIONING.md#091-r3-candidate-来源绑定)，
  STATIC/SHARED、安装消费者及兼容门的本轮证据见关闭记录绑定的 d195 最终矩阵。
- Gameplay v2 是当前实施基线，顶层合同仍为 candidate；局部实现不等于整批验收。语义与运行边界见
  [Gameplay V2 Spec](formats/GAMEPLAY_V2_SPEC.md)、[ABI](api/GAMEPLAY_V2_ABI.md)及[RPA 补充合同](formats/rpa-execution-contract.md)。
- Chart v1–v4 的既有兼容路径保留；Chart v5、CXT v2、Packed Chart 与 Canonical Gameplay Graph 仍为显式候选路径，正式发行收敛归 Stage 8。
- 稳定 C ABI 归[Stage 14](stage_plans/future/stage-14/plan.md)，当前只提供 C++ typed preview。

各格式的生产/候选状态与权威文档见[格式索引](formats/README.md)。

## 延期和禁止边界

运行时脚本和逐帧 script callback 无限期延后，不预留字段、capability、bytecode、ABI 或 Playback hook。
离线 authoring generator 不会被 pack、prepare 或 Playback 隐式执行。

## 更新规则

只更新当前结论，完成或被替代的状态应就地更新或删除。
测试轮次、命令、提交经过、授权沟通和历史问题处置保存在阶段报告中，不向本文追加过程日志。
