# Chart Format Foundation: Handoff Review and Hardening Decision

日期：2026-09-16

状态：review recorded；加固阶段 active，技术门禁尚未关闭

## 复核范围与方法

本记录承接同日的 [owner 关闭与交接记录](2026-09-16-closure-and-handoff.md)，
保存之后的静态技术复核及项目所有者要求建立独立加固阶段、将 Stage 6 移回 future 的决定。
原 Foundation 完成确认不撤销；新的实现与验证缺口由加固阶段处置。

复核实现基线为 PR #24 合并提交 `13dab93`；本轮仅检查源码、测试与报告，未重新构建 C++、
执行恶意输入或独立核验远端 CI。以下为需在 R0 复现的静态发现，不是新增运行时测试结果。

## 发现

| ID | 优先级 | 静态证据 | 影响与处置批次 |
| --- | --- | --- | --- |
| H01 | P1 | `packed_chart_tables.cpp` Writer 写零 semantic hash，Reader 未重算比对；`cxc_candidate.cpp` 仅校验 compiledSemanticIdentity 字符串格式 | 语义身份未闭环；R1 |
| H02 | P1 | REQ0 decoder 允许 interval=1，但 Spec 的 Foundation profile 仅登记 point | 超出登记 subset；R2 |
| H03 | P1 | 40k 测试主要执行 size；已有报告未提供完整最终 SHA hosted/往返/回滚证据链 | 技术退出证据不足，不代表 CI 已失败；R4/R5 |
| H04 | P2 | 原计划对峰值/耗时预算有冲突表述，maxPackedSectionBytes 已声明但未落实检查 | 预算承诺与执行不一致；R3 |

实现入口：[Packed tables](../../../../engine/chart/src/packed_chart_tables.cpp)、
[CXC validator](../../../../tools/cxc_common/src/cxc_candidate.cpp)、
[容量测试](../../../../tests/chart/chart_foundation_capacity_tests.cpp)、
[Packed limits](../../../../engine/chart/include/cuexis/chart/limits.hpp)。

## 设计评价与决策

typed semantic model、CXT 展开与 Packed 编码分层、显式 candidate revision、保留 v4 回退，
以及把最小 Judgement 与正式格式发行分别放在 Stage 7A/8，方向保留，不重新立项设计 v5。
问题集中在合同落实和验证深度，不能通过归档或合并状态抵消。

项目所有者要求建立
[Chart Format Foundation Hardening](../../../stage_plans/completed/chart-format-foundation-hardening/plan.md)，
按 R0-R5 执行；[Stage 6](../../../stage_plans/active/stage-06/plan.md) 暂回 future。
`chart-format-update-for-v5` 保留 active，仍为跨阶段总工作包。

本决策不宣称任何 H01-H04 已修复，也不宣称已取得新的 hosted PASS。
各问题在加固报告中追加复现、修复和最终证据；本历史记录不改写为后续结果。
