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

落地 agent 工作规则：交接页、三份模板、PR 模板，以及 `check_docs.py` / `check_docs_facts.py`
的对应门禁。本批已完成并通过门禁，准备提交到 PR #46。

## 下一步

1. 提交并推送本批规则与模板到 PR #46。
2. 继续文档遗留：53 篇证据缺 meta、35 条版本漂移、Chart 表示与转换点表。
3. 等 CI 结果；Version Gate 已在 `b34e6eb3` 通过。

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
- 修改：`tools/verification/check_docs.py`、`tools/verification/check_docs_facts.py`、
  `tools/verification/gen_docs_map.py`、`AGENTS.md`、`docs/CURRENT_STATUS.md`
