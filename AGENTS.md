# Cuexis Agent Guide

<!-- CUEXIS_STATE_BEGIN -->
> 本块由 `tools/verification/gen_docs_map.py` 生成，勿手改；由 `check_docs_facts.py` 在预推送校验。

- 当前阶段：**Stage 7-RR**；当前工作项：[stage_plans/active/stage-07-rr/plan.md](docs/stage_plans/active/stage-07-rr/plan.md)；当前登记册：[stage_plans/active/stage-07-rr/findings.md](docs/stage_plans/active/stage-07-rr/findings.md)；进度以 [CURRENT_STATUS](docs/CURRENT_STATUS.md) 为准。
- SDK API：`0.9.1`；日期构建：`26.11.16-1`；权威定义在 `cmake/CuexisVersion.cmake`。
- 文档入口：[docs/MAP.md](docs/MAP.md)（先读它，再按任务打开 1–3 篇）。
- 权威规则：事实以代码/schema/CMake 为准；`docs/stage_reports/**` 与 `docs/archive/**` 是证据，不是权威；`candidate`/`future`/`deferred` 不代表已实现。
- 本文件正文只写路由与硬约束，不复述事实；冲突时以上块、`docs/MAP.md` 与 `docs/CURRENT_STATUS.md` 为准。
<!-- CUEXIS_STATE_END -->

## 本文件只写“怎么做”，不写“是什么”

阶段、版本、模块规模、能力状态、格式字段一律去文档取；本文件只保留路由与硬约束。
顶部生成块是唯一例外（版本与阶段由 `--check` 保证新鲜），正文不复述事实。

## 先读什么

1. [docs/MAP.md](docs/MAP.md)：文档路由，按状态列出合同与计划；只打开与任务相关的 1–3 篇
   （复杂任务最多 5 篇，不要通读）。
2. [docs/CURRENT_STATUS.md](docs/CURRENT_STATUS.md)：唯一当前状态；阶段顺序见
   [ROADMAP](docs/ROADMAP.md)。
3. [docs/DOCUMENTATION_POLICY.md](docs/DOCUMENTATION_POLICY.md)：文档角色、状态词、元数据与生成物规则。

**权威顺序**：代码、`schemas/`、`cmake/CuexisVersion.cmake` > 合同文档（`docs/adr`、`docs/api`、
`docs/architecture`、`docs/formats`、`docs/guides`）> 计划 > 证据。
`docs/stage_reports/**` 与 `docs/archive/**` 是**证据**，不是权威；不得据其推断当前实现，也不得改写历史结论。
生成块、MAP、CURRENT_STATUS 三者冲突时：**阶段与遗留**以 CURRENT_STATUS 为准，路由以 MAP 为准，
其余事实按上面的权威顺序。

文档承诺与代码不一致时，以代码为准，并把差异登记到**当前登记册**（路径见生成块的
“当前登记册”）；不要先改代码再改合同。

## 硬约束

- 安装头必须是纯 ASCII；理由与写法见 [CODE_POLICY](docs/guides/CODE_POLICY.md)。
- `core::Result` 不得静默丢弃；异常不得跨模块边界；析构与实时路径不得抛（同上）。
- 公共头不得泄漏第三方类型、EnTT、SDL、GL/GLAD、JSON DOM；边界图与门禁入口见
  [architecture/OVERVIEW](docs/architecture/OVERVIEW.md) 与 [BUILDING](docs/guides/BUILDING.md)。
- 依赖增删必须同步 [DEPENDENCY_POLICY](docs/guides/DEPENDENCY_POLICY.md) 要求的多处记录。
- 构建、测试、格式检查、静态分析命令在 [BUILDING](docs/guides/BUILDING.md)；版本与发版规则在
  [VERSIONING](docs/guides/VERSIONING.md)。
- 改 `docs/` 后跑 `python -B tools/check_docs.py`；改地图/预算/诊断码/版本后重新生成并跑
  `python -B tools/verification/check_docs_facts.py`（生成器清单见 DOCUMENTATION_POLICY）。
- 长任务（完整构建、容量/内存探针、hosted 重跑）先说明范围、能否做、预计耗时，等 owner 选定后执行。
- PR 纪律（owner 要求）：一条 PR 尽量承载多件事；只有“不开新 PR 就进行不下去”或“已有证据会作废”才另开。

## 不要

- 不要在本文件或新文档里复述版本、预算、状态数字；链接到权威源或生成页。
- 不要用证据报告的“下一步”推断当前工作。
- 不要把 `candidate` / `future` / `deferred` 描述成已实现能力。
