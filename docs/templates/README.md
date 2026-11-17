# 文档模板

状态：current

更新日期：2026-11-17

<!-- cuexis-meta
id: INDEX-TEMPLATES
status: current
authority: index
-->

本目录是写作模板，不是合同，不参与路由权威。新文档复制对应模板后替换占位内容；
模板本身不要求与任何阶段的字段同步。

| 模板 | 用途 |
| --- | --- |
| [plan.md](plan.md) | 已启动阶段的详细计划 `docs/stage_plans/active/<stage>/plan.md`；项目开始时由规划模型一次填细 |
| [plan-outline.md](plan-outline.md) | 未启动阶段的提纲 `docs/stage_plans/future/<stage>/plan.md`；只写方向、入口条件与待决问题，不写任务看板/验收矩阵 |
| [report.md](report.md) | `docs/stage_reports/stages/<stage>/YYYY-MM-DD-<topic>.md` |
| [handoff.md](handoff.md) | `docs/HANDOFF.md`（对话级交接） |

规则要点：

- 模板中的固定 H2 标题是门禁检查的对象（active plan 与 2026-11-17 之后的新报告），不要改名。
- 报告必须带日期、状态、authority 与 `file:line` 证据；结论用词只允许
  “成立 / 部分成立 / 不成立 / 无法核实”。
- 交接页是对话级状态，不是阶段权威；阶段事实链接到 `CURRENT_STATUS.md`。
