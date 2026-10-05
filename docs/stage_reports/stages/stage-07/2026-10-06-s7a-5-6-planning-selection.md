# S7A-5 / S7A-6 Planning Selection and Handoff Revision

状态：dated planning record；多个批次选择与未定实现方案选优完成，无新增产品实现

日期：2026-10-06

## 1. owner澄清与范围修订

854efa3先前把下一轮限定为S7A-5，并把Ruleset字段选型留到实施会话。
owner随后明确要求：选几个批次作为下一轮目标、规划实施方法、
对阻碍实施的未定决策给出4–5套方案并选优。

本次比较五套组合：仅5、5+6、5+6+7、5–8、5–9；
选定 **S7A-5.1–5.5 + S7A-6.1–6.5** 同一会话串行实施、联合验收。
5的内部状态门禁仍是6的消费前置；并批减少状态交接与5.5恢复验收的悬挂，
不允许混淆kernel/Fact commit与Fold commit的两个失败边界。

## 2. 九项决策、各五套方案和推荐

| 规划ID | 推荐方案 | 尚须实施会话完成 |
| --- | --- | --- |
| P56-01 | R1 owning typed + 有限内置操作 | PreparedRuleset/Interface/module/register/delta完整字段与owner |
| P56-02 | G2 signed tick error区间grade table | phase-local求值、缺失/Miss边界、新旧profile及identity |
| P56-03 | N1 i64/u64 + 显式策略 + 已证明monoid | Score/Combo/Statistics表示、初值与算术golden |
| P56-04 | T2 独立Fold候选、fault reserve、单次发布 | 两cursor/perTick原子失败与t+1信号 |
| P56-05 | S2 三族owning全量DTO | 私有ingress/matcher、Spec十四项闭包、恢复校验 |
| P56-06 | W2 有序section + LE定宽record + 摘要 | 完整wire/tag/版本表、Reader/Writer与跨工具链bytes |
| P56-07 | E2 accepted事件/control journal + Ledger校验 | late admission复算与共用真实输入路径 |
| P56-08 | K2 target cut + full snapshot有序索引 | 同horizon晚到/任意点恢复与独立起点oracle |
| P56-09 | B2 测量状态与预算接受分离 | pending descriptor、生产数值拒绝与test-only门禁 |

完整45套实现方案及五套批次组合的优缺点和选择理由见
[方案比较与选择](../../../stage_plans/active/stage-07/s7a-5-6-design-selection.md)。
其推荐是设计输入，不是Spec/ABI生产冻结或实现验证。
已裁定的三阶段、Fact总序、Register三类、faulted、Life/package/correction边界不重新开放；
owner-only D-9/SDK门禁归8，生产容量/Seek承诺数值归9。

## 3. 新的接手入口

[联合接手](../../../stage_plans/active/stage-07/s7a-5-6-implementation-handoff.md) 按八张卡执行：
J0完整合同补充→J1 prepare/grade→J2 Fold/Hook→J3 identity/codec/Replay→
J4 full Snapshot→J5 Seek→J6联合故障/回归→J7完整矩阵与退出报告。
十个小目标分别列出实际实现卡和退出证据，5.5真实恢复不再整行推给另一会话。

同步CURRENT_STATUS、Stage7总计划、交付评估与索引；
旧单批接手保留为superseded planning snapshot，入口显式指向新联合接手。
既有dated hosted/归档报告保留历史单批排期，不重写其当时状态或借用其SHA验证本次文档。

## 4. 本轮证据与清理边界

本轮只读取相关既有typed/private状态声明以评估恢复缺口，并修改文档；
没有新增产品代码、CMake/CI/SDK/version、fixture或依赖，没有C++/CTest/性能/容量运行。
本轮文档门禁均以退出码0通过：

| 检查 | 结果 |
| --- | --- |
| check_docs.py | 368个Markdown、20个candidate JSON/CXT通过 |
| check_docs_status_contract_tests.py | 4 tests通过 |
| check_docs_target_contract_tests.py | 2 tests通过 |
| update_version.py --check | 26.10.05-1一致 |
| git diff --check | 无空白错误 |

以上是文档证据，不是5/6实施、codec正确性、C++或容量验收证据。

此前归档/清理事实仍见
[3/4 hosted与归档报告](2026-10-05-s7a-3-4-hosted-and-handoff.md)。
本轮不新建out临时脚本或日志；现有构建缓存/用户备份保留。
文档提交和推送保持stage-7可交接，实际SHA由Git记录；不等待CI完成。
S7A-5/6产品尚未实现，Stage7A仍active，state-budget/容量整体仍INCOMPLETE GATE。
