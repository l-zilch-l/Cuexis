# 统一实施前设计裁定与任务安排

状态：evidence

更新日期：2026-11-23

<!-- cuexis-meta
id: REPORT-stage-07-rr-design-decisions-and-schedule
status: evidence
authority: none
-->

## 结论

**成立**：owner 已完成本轮需要的范围和行为裁定，决定及逐项承接见[登记册 §24](../../../stage_plans/active/stage-07-rr/findings.md#24-统一实施前设计裁定2026-11-23)。v5 候选旧动画特许已订正，F203 采用登记前整批验证，完整结构清理与 T-10–T-16 纳入本轮。

**成立**：统一任务安排把 T-09 放在全部任务之后，阶段验收覆盖 T-01–T-17 的实现证据。设计接受、任务采用、运行通过与发布继续分别记录。具体结构检查结果见执行记录。

**无法核实**：本轮未实现产品改动；不能据文档检查判定 SDK、格式、设备或性能已经通过验收。

## 范围与方法

源码基线为 `db6042b659139ac86bb52192e18132309c2aab27`，包含会话中已有未提交文档修订。复用两个无父上下文子代理，分别完成残余处置/公共面/资源去重与 v5 表现/Chart 隔离专题；主代理核对关键源码、应用边界、裁定、调度和验收。

静态依据包括原 v5 计划、CXT2 Foundation schema/loader、Runtime override 登记与求值、Chart 源组和实际消费者、app/engine CMake、公共安装与内部入口。本轮仅修改文档及生成路由，未构建、未运行产品测试，未把历史 out 结果作为当前通过证据。

## 执行记录

```text
python -B tools/verification/check_plan_schedule.py
PASS: 17 active, 0 candidate, 0 completed
python -B tools/verification/test_check_plan_schedule.py
PASS: 27 tests
python -B tools/verification/gen_docs_map.py
wrote docs/MAP.md and AGENTS.md state block
python -B tools/check_docs.py
PASS: 771 Markdown, 20 candidate JSON/CXT
python -B tools/verification/check_docs_facts.py
PASS: version-drift findings 0
python -B tools/verification/gen_docs_map.py --check
PASS: 770 documents
git -c core.safecrlf=false diff --check
PASS: no whitespace errors
```

首次文档检查指出新报告从模板继承的 `historical` 与证据目录要求的 `evidence` 不符；已订正报告元数据、重新生成地图，复跑通过。临时链接核对使用现有 `check_docs.heading_anchors`，检查当时 36 篇变更 Markdown 的 1021 个本地路径/章节链接，0 错误；未新增常驻检查器。

独立子代理在统一排期上复核 plan/schedule/acceptance/T-14，T-09 前置闭包覆盖其余 16 卡，未发现漏前置或全面清理重新延期。F302 的静态源表核对覆盖原 Chart 47 个 TU、无重复归属；这只验证设计源表，不替代实际 CMake、链接或安装测试。

## 结果与证据

| 结论 | 证据 | 状态 |
| --- | --- | --- |
| v2 正式目标与 v3 实验实现分别明确 | [范围与授权](../../../stage_plans/active/stage-07-rr/contracts/runtime-timing.md#范围与授权) | 成立 |
| v5 表现语法遵循原设计，CXT2 adapter 与能力拒绝有责任任务 | [源输入](../../../stage_plans/active/stage-07-rr/contracts/prepared-artifact.md#源输入与-lowering)、[CXT2 映射](../../../formats/CXT_V2_FORMAT.md#91-chart-v5-动画导入与-animator-映射)、T-11/T-13 | 成立 |
| F203 行为已接受，非法批次不发布覆盖 | [Runtime 修订](../../../architecture/RUNTIME_SESSION.md#临时覆盖登记修订f203已接受未实现)、T-17 | 成立 |
| 全面结构清理有范围、落点与正反验收 | [当前残余处置](../../../stage_plans/active/stage-07-rr/findings.md#残余问题的实际承接)、T-04/T-06/T-08/T-14 | 成立 |
| 旧问题逐条分类，未统一包装成延期 | 同上；F404/F435/F438 等按现有接口/政策收窄原断言，F406/F407 核完整账本与参与方 | 成立 |
| 旧谱兼容绑定独立预期，成品无需源工程重放 | T-01/T-10/T-13，最终由 T-09 核对实际证据；候选夹具与正式交付分开 | 成立 |
| 调度与验收覆盖全部任务 | [调度](../../../stage_plans/active/stage-07-rr/schedule.md)、[A-01–A-09](../../../stage_plans/active/stage-07-rr/acceptance.md)、执行记录 | 成立 |

## 限制与未做

设计与源码静态核对不能证明后续构建、跨平台一致性、设备恢复、运行内存或性能达标。新 schema/target/测试由任务创建后验收。旧发行语料的存在性与独立预期须在 T-01 绑定，缺失时按具体材料登记，不能预认证。

旧公共 API 退役需要实际过渡发布与替代稳定性；未满足时保留对应兼容入口。它是已决定的条件，不是未裁定的产品规则。本轮未提交、推送、发布或启动生产实现。

## 对登记册与合同的影响

当前处置追加到 findings §24，保留旧轮次结论。原 v5/Stage8 计划同步实验分支与 T-13 的提前承接；Runtime、三份技术合同、责任卡、统一调度与验收同步。当前状态与下一步由 HANDOFF/CURRENT_STATUS 维护；以后实施若发现新实质冲突，按原 D-04 停止受影响切片并登记。
