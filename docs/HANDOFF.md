# 当前交接

状态：active

更新日期：2026-11-17

<!-- cuexis-meta
id: STATUS-HANDOFF
status: active
authority: owns
-->

本页是**对话级交接**：记录当前任务、下一步、阻塞与验证状态，供新对话直接接手。
阶段事实仍以 [CURRENT_STATUS](CURRENT_STATUS.md) 为准，问题清单以
[Stage 7-RR 登记册](stage_plans/active/stage-07-rr/findings.md) 为准。

## 当前阶段与工作项

- 阶段：Stage 7-RR
- 工作项：`stage_plans/active/stage-07-rr/plan.md`
- 登记册：`stage_plans/active/stage-07-rr/findings.md`
- 分支/PR：`stage-7.5` / PR #46

## 当前任务

模板与计划体系已收口：详细 plan 模板用于 active 阶段，`stage_plans/future/**` 只用提纲模板；
Stage 7-RR 的计划已按详细模板重写。当前没有进行中的实现批次，下一步回到文档遗留。

## 下一步

1. 继续文档遗留：53 篇证据缺 meta、35 条版本漂移、Chart 表示与转换点表。
2. 等 CI 结果；Version Gate 已在 `b34e6eb3` 通过。
3. 开工新的代码/架构批次前，先更新本页的当前任务与下一步。

## 阻塞与待决

- RR-F510（Judgement seek 无 active 权威入口）：需要 owner 决策。
- 版本漂移是否加“历史豁免”标记：需要 owner 决策。
- `engine/particles` 空壳、`cuexis_debug`（仅测试）是否清理：需要 owner 决策。

## 验证状态

- `check_docs.py`：0（737 篇）。
- `check_docs_facts.py`：0；交接页阴性对照（删掉“下一步”章节）会失败。
- `gen_docs_map.py --check`：0；`git diff --check`：0。

## 最近改动

- 新增：`docs/HANDOFF.md`、`docs/templates/`、`.github/pull_request_template.md`
- `docs/templates/plan.md`：扩展为项目起始的详细规划协议（规划问题清单、任务看板、入场门禁、
  验收矩阵、回滚/变更控制），对齐现有 plan 的实际写法
- 新增 `docs/templates/plan-outline.md`：未来阶段只写提纲，启动时再细化
- `docs/stage_plans/active/stage-07-rr/plan.md`：按详细模板重写（218 行，RR-0–RR-6 语义不变）
- 修改：`tools/verification/check_docs.py`、`tools/verification/check_docs_facts.py`、
  `tools/verification/gen_docs_map.py`、`AGENTS.md`、`docs/CURRENT_STATUS.md`
