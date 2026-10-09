# 目标、依赖与安装图（S7A-0 准入）

状态：candidate；S7A-0 准入产出，待 owner acceptance

更新日期：2026-10-02

决策登记：§9 的五项模块 / target 决策已裁定（2026-10-02，编号 S1-04，owner 已接受）；本文件只登记
口径，CMake 与 allowlist 落地属 S7A-1 实施批次（见 §9）。

上级文档：[Gameplay V2 acceptance package](README.md) ·
[Stage 7A 实施计划](../../stage_plans/active/stage-07/plan.md)

文档角色：target / 依赖 / 安装图草案。它描述 Stage 7A 需要新增或改造的构建目标、允许依赖、
安装导出影响与必须同步修改的位置；它不是实施记录，也不表示任何 target 已经存在。

## 1. 现状（实测基线）

| 事实 | 值 | 出处 |
| --- | --- | --- |
| 现有模块 target | `engine/` 下 `add_library(cuexis_*)` 实测 23 个；`cuexis_particles` 未接入 `engine/CMakeLists.txt`，`cuexis_shader` 为条件定义 | `git grep`、`engine/*/CMakeLists.txt` |
| 其它 target | app/工具 10 个（`add_library`/`add_executable`）、测试可执行 40 个 | `git grep` |
| `engine/gameplay/` 现状 | **INTERFACE stub**，只有 `include/cuexis/gameplay/tags.hpp` 与 `CMakeLists.txt`，无 `src/` | `engine/gameplay/CMakeLists.txt`、`engine/gameplay/include/cuexis/gameplay/tags.hpp` |
| stub 内容 | `NoteTag` / `ElementTag` 两个空 tag；注释声明"真实判定系统属于后续阶段" | `tags.hpp` |
| stub 的依赖 | `cuexis::core`（INTERFACE） | `engine/gameplay/CMakeLists.txt` |
| stub 的现有消费者 | `cuexis_runtime`（PRIVATE）、`cuexis_runtime_tests` | `engine/runtime/CMakeLists.txt`、根 allowlist 段 |
| stub 是否安装 | 在 `CUEXIS_STATIC_IMPLEMENTATION_TARGETS` 内（只装 ARCHIVE，无公共头） | 根 `CMakeLists.txt` |
| 任何判定/输入模块是否存在 | **不存在**。全仓没有 `cuexis_judgement`、`cuexis_input` 或等价 target | `CUEXIS_ACTIVE_TARGETS` |
| 离线 assembler 工具 | **不存在**。`tools/` 内 `assembl*`、`chart-candidate` 零命中 | `git grep` |
| 既有约束 | `cuexis_judgement` 不得依赖 platform、audio、render backend 或宿主引擎 SDK | `AGENTS.md`「Future constraints」 |

## 2. 模块位置：两个候选

计划 §S7A-1 要求"选择模块位置和 target 方向"。以下是两个候选，**选择本身是 owner 决策**；
该决策已于 2026-10-02 裁定为**方案 A**（S1-04，owner 已接受），下表保留两个候选的原始对比。

| 方案 | 做法 | 优点 | 缺点 |
| --- | --- | --- | --- |
| A：新增 `engine/judgement/`（`cuexis_judgement`） | 新建 STATIC 模块，`cuexis_gameplay` stub 保持不变或退化为别名 | 与 `AGENTS.md` 已声明的未来约束同名；`engine/gameplay/` 的"Phase 1 placeholder"注释不被改写；职责单一 | `runtime` 现有的 `cuexis::gameplay` 依赖需另行处理 |
| B：把 `engine/gameplay/` 从 INTERFACE stub 升级为实模块 | 就地扩展为 `cuexis_gameplay`，新增 `src/`、公共头与 FILE_SET | 复用现有 target 名、allowlist 位与安装位；`runtime` 依赖不需迁移 | `tags.hpp` 与"Phase 1 placeholder"注释需重写；`cuexis_judgement` 命名约束落空 |

本 package 建议 **方案 A**，理由：`AGENTS.md` 已把 `cuexis_judgement` 写成受约束的未来 target，
且 Input/Judgement/Replay 需要独立于 Runtime 的依赖边界（计划 §S7A-1.1）。**已决定（2026-10-02，
S1-04，owner 已接受）：采用方案 A**——新建 `engine/judgement/`，作为内部 STATIC target
`cuexis_judgement`；现有 `cuexis_gameplay` stub 及其 Runtime 依赖**保留**，**不做别名**指向新内核
（见 §9 第 1 项）。

## 3. 建议新增 target

| target | 类型 | 位置 | 安装 | 说明 |
| --- | --- | --- | --- | --- |
| `cuexis_judgement` | STATIC（内部库，与 `cuexis_chart` 同级；类型已定） | `engine/judgement/`（**新建**，当前不存在） | 第一版**不**进 `CUEXIS_PUBLIC_EXPORT_TARGETS`；**进** `CUEXIS_STATIC_IMPLEMENTATION_TARGETS`（2026-10-02，S1-04，owner 已接受；见 §9 第 4 项） | Input 规范化、Requirement prepare、Fact Ledger、Ruleset 事务、Score/Combo/Statistics、Replay/Snapshot/Seek |
| `cuexis_judgement_tests` | EXECUTABLE | `tests/judgement/` | 否 | Catch2 用例，`catch_discover_tests` |
| `cuexis_gameplay_assembler` | STATIC 或 EXECUTABLE（离线工具库） | `tools/gameplay_assembler/`（**新建**，当前不存在；名称已接受，实现属后续批次） | 否 | 从 typed Chart/CXT/source 派生 feature、requirements、resource closure、capability（S7A-8.2） |
| `cuexis_chart_candidate` | EXECUTABLE | `tools/chart_candidate/`（**新建**，当前不存在；名称已接受，实现属后续批次） | 否 | 离线 CLI：assembler → encode → package，产出 closure report |
| `cuexis_judgement_headless_consumer` | EXECUTABLE | `tests/judgement_headless/` | 否 | 无 GPU/SDL/OpenGL 的 headless consumer（S7A-7.3） |
| `cuexis_judgement_capacity_probe` | EXECUTABLE | `tests/judgement/` | 否 | 预算测量探针（见预算计划 §9） |

原待决问题：是否把 Input 规范化拆成独立 `cuexis_input` target——拆开能强制 L1 边界，但会增加一个
allowlist 位与一次安装决策；合并进 `cuexis_judgement` 则 L1/L2 边界只能靠头文件组织约束。
**已决定（2026-10-02，S1-04，owner 已接受）：本阶段不拆 `cuexis_input`**，输入层以独立头文件分区
+ 架构检查约束 L1/L2 边界（见 §9 第 2 项）。离线工具目录名 `tools/gameplay_assembler/` 与
`tools/chart_candidate/` 已被接受，但**工具实现仍属后续批次**，本阶段不新建这两个目录
（见 §9 第 5 项）。

## 4. 允许依赖（草案）

下表是建议写入 `cuexis_verify_target_dependencies(... ALLOWED ...)` 的直接依赖清单。
规则：**必须列出全部直接项，包括 PRIVATE 项**（现有 `cuexis_chart` 就是如此）。

| target | ALLOWED 草案 | 依据 |
| --- | --- | --- |
| `cuexis_judgement` | `cuexis::core`、`cuexis::chart`（只用于已编译的 typed 输入；**不是** JSON 解析） | 计划 §S7A-1.1：不得依赖 SDL/OpenGL/Audio/World/EnTT/JSON DOM |
| `cuexis_judgement_tests` | `cuexis::judgement`、`Catch2::Catch2WithMain` | 测试 target 的既有写法 |
| `cuexis_gameplay_assembler` | `cuexis::core`、`cuexis::chart`、`cuexis::json_support`（读作者源）、`cuexis::cxc`（可选） | 离线工具可持有 JSON DOM，但不得把 DOM 类型带进 `cuexis_judgement` 公共头 |
| `cuexis_chart_candidate` | `cuexis::gameplay_assembler`、`cuexis::cxc_tool_common` | 复用既有 CXC 工具公共库 |
| `cuexis_judgement_headless_consumer` | `cuexis::playback`、`cuexis::judgement`（若 Playback 不导出判定类型，则只链接 `cuexis::playback`） | 见 §5 的导出决策 |
| `cuexis_judgement_capacity_probe` | `cuexis::judgement` | 同测试写法 |

明确**不允许**出现在 `cuexis_judgement` 的直接依赖里：`cuexis::world`、`cuexis::runtime`、
`cuexis::render`、`cuexis::render_opengl`、`cuexis::audio`、`cuexis::audio_sdl`、`cuexis::platform_sdl`、
`cuexis::behavior`、`cuexis::animation`、`cuexis::presentation_renderer`、`cuexis::player_support`、
`EnTT::EnTT`、`SDL3::SDL3`、`glad`、`spdlog::spdlog`（日志需走内部诊断通道）。

## 5. 安装与导出影响

关键决策：**判定类型是否进入安装公共头**。**已决定（2026-10-02，S1-04，owner 已接受）：采用
选项 1**——不安装判定公共头、不新增公开组件或 Playback 方法，本批次不触发 SDK API 升版
（见 §9 第 3 项）。

| 选项 | 含义 | 影响 |
| --- | --- | --- |
| 选项 1（推荐，保守；**已采纳**） | `cuexis_judgement` 第一版不安装公共头；Playback 只通过已有 `PlaybackSession` 暴露显式判定入口，判定类型留在内部模块 | 不改变 SDK API 版本；`0.7.1` 只在需要新增 Playback 公共方法时触发 |
| 选项 2 | 安装 `cuexis_judgement` 公共头并加入 `Cuexis` 组件 | 需新增 `find_package` 组件、ASCII/泄漏门禁覆盖、SDK API `0.7.1`；与 S7A-8.4 的版本门禁阻塞耦合 |

相关既有事实：

- 安装组件只有 `Core`、`Playback`、`Content`、`Audio`、`AudioSDL`（`cmake/CuexisConfig.cmake.in`）。
  新增组件需要改配置模板、负例门禁与文档。
- `cuexis_presentation_renderer` 与 `cuexis_player_support` 被 `cmake/VerifyArchitecture.cmake`
  **硬编码禁止**进入两个安装列表；新模块若也要保持内部，需要同样的硬约束或明确不进列表。
- 公共头 ASCII / GLM 泄漏扫描覆盖 `engine/*/include/*.hpp`，Playback 另有 EnTT/SDL/GLAD/JSON/spdlog
  泄漏扫描。判定模块若安装公共头，必须同时纳入这两套扫描。
- `docs/guides/BUILDING.md` 的 `<!-- CUEXIS_ACTIVE_TARGETS_BEGIN -->` 块必须与
  `out/build/debug/generated/cuexis-targets.txt` **逐行相等**，因此每个新 target 都要同步该块。

## 6. 新增一个内部模块的标准步骤（仓库现有写法）

顺序即为依赖顺序；每步都是 configure 期或 docs 门禁的失败点。

```text
1  engine/<module>/include/cuexis/<module>/*.hpp      纯 ASCII 英文注释；不得出现 GLM/nlohmann/EnTT/SDL/GL
2  engine/<module>/src/*.cpp
3  engine/<module>/CMakeLists.txt                     add_library / ALIAS / EXPORT_NAME Internal<Name>
                                                       target_sources FILE_SET HEADERS
                                                       target_include_directories / compile_features / link
4  engine/CMakeLists.txt                              add_subdirectory，位置晚于全部依赖模块
5  根 CMakeLists.txt CUEXIS_ACTIVE_TARGETS            加入 target（顺序决定 generated 清单顺序）
6  根 CMakeLists.txt 安装列表                          STATIC 安装 / PUBLIC 导出（二者互斥语义不同）
7  根 CMakeLists.txt cuexis_verify_target_dependencies ALLOWED 全量直接依赖
8  tests/<module>/<subject>_tests.cpp
9  tests/<module>/CMakeLists.txt                      add_executable / link Catch2 / catch_discover_tests
10 tests/CMakeLists.txt                               add_subdirectory
11 根 CMakeLists.txt CUEXIS_ACTIVE_TARGETS            加入测试 target
12 根 CMakeLists.txt 测试 allowlist                    cuexis::<module> + Catch2::Catch2WithMain
13 docs/guides/BUILDING.md target 块                  与 generated/cuexis-targets.txt 逐行同步
14 cmake/VerifyArchitecture.cmake                     仅当新模块需要 include 黑名单时
15 vcpkg.json / DEPENDENCY_POLICY.md / THIRD_PARTY_NOTICES.md   仅当新增第三方依赖时
```

## 7. 依赖与安装图

```text
                   ┌───────────────────────────┐
                   │ cuexis_gameplay_assembler │  离线工具（可含 json_support / cxc）
                   │  typed source → feature   │
                   │  requirements / closure   │
                   └─────────────┬─────────────┘
                                 │ 产出 canonical typed graph（不产出运行期状态）
                                 v
  Chart v5 source ─┐   ┌──────────────────────┐   ┌───────────────────────┐
  CXT v2 source  ──┼──>│  cuexis_judgement    │<──│ cuexis_chart（typed） │
  Packed entry   ──┤   │  L1 Input / L2       │   └───────────────────────┘
  CXC graph entry ─┘   │  Coordination / L3   │
                       │  Fact Ledger / Replay│
                       └───────┬──────────────┘
                               │ 只读 FactBinding / JudgementResult
                               v
                    ┌──────────────────────────┐
                    │ cuexis_playback          │  PlaybackSession 显式判定入口
                    │ （不暴露判定内部类型）    │
                    └───────┬──────────────────┘
                            │
              ┌─────────────┴──────────────┐
              v                            v
     cuexis_player（GPU/窗口）    headless / external consumer（无 GPU）
```

禁止边（必须由 allowlist 与既有架构脚本共同保证）：

```text
cuexis_judgement -> SDL / OpenGL / glad / Audio / AudioSDL / World / EnTT / JSON DOM / Runtime
cuexis_judgement -> Presentation renderer / Player support / 宿主引擎类型
Packed decoder   -> CXT 解析 / Pattern 展开 / solver 执行
Playback         -> 直接读 CXT AST 或在运行期展开 Pattern
```

## 8. headless 与配置约束

- `headless-debug` / `headless-release` preset 关闭 Player、SDL、AudioSDL、OpenGL 与开发者工具；
  判定模块必须在这些 preset 下可配置、可构建、可测试（计划 §S7A-7.3）。
- 外部 consumer 工程自身强制关闭适配器；`cuexis_external_consumer_*playback*` 带 `headless` label。
- 判定模块**不得**要求 GPU、窗口、音频设备或真实输入设备；这些证据单列为"未执行/环境限制"。

## 9. 模块与 target 决策登记（原"待 owner 决策"）

**状态：五项全部已决定（2026-10-02，编号 S1-04，owner 已接受）。** 登记依据：owner 于 2026-10-02
接受 Codex（`gpt-6-astra`，hard / medium，verdict `reject`，confidence 0.94，thread
`s7a1-admission-rulings`）修订后的处置，即**方案 A**；这正是
[Stage 7A typed contract review](../../stage_reports/stages/stage-07/readiness/2026-10-02-s7a-1-typed-contract-review.md)
F-12 指出的"未登记到任何裁定轮次"的五项前置。

| §9 项 | 原待决问题（逐字摘要） | 决策（2026-10-02，S1-04） | 依据 / 落点 |
| --- | --- | --- | --- |
| 1 | "模块方案 A（新建 `engine/judgement/`）还是方案 B（升级 `engine/gameplay/`）" | **方案 A**：新建 `engine/judgement/`，作为**内部 STATIC** target `cuexis_judgement` | 计划 §S7A-1 工作内容 1；`AGENTS.md`「Future constraints」已把 `cuexis_judgement` 写成受约束的未来 target；本文件 §2、§3 |
| 1（同项第二半） | 方案 A 行内原写"`cuexis_gameplay` stub 保持不变**或退化为别名**" | **保留**现有 `cuexis_gameplay` stub 及其 Runtime 依赖；**不做别名**指向新内核 | 别名会让 `runtime` 的既有 `cuexis::gameplay` 依赖语义漂移；本文件 §1、§2 |
| 2 | "Input 规范化是否拆成独立 `cuexis_input` target" | **本阶段不拆** `cuexis_input`；输入层以独立头文件分区 + 架构检查约束 L1/L2 边界 | 本文件 §3；计划 §S7A-1 工作内容 1 |
| 3 | "判定类型是否进入安装公共头（选项 1 或 2），以及由此是否触发 SDK API `0.7.1`" | **选项 1**：不安装判定公共头、不新增公开组件、不新增 Playback 方法；本批次**不触发 SDK API 升版** | 本文件 §5；安装组件现状见 `cmake/CuexisConfig.cmake.in` |
| 4 | "`cuexis_judgement` 是否进入 `CUEXIS_STATIC_IMPLEMENTATION_TARGETS`" | **进入**；沿用"仅静态包导出内部 archive、无头文件"的既有机制 | 根 `CMakeLists.txt:586`（列表起始）、`:587-598`（12 项）、`:607-612`（仅 STATIC 下 `install(TARGETS ... ARCHIVE ...)`） |
| 5 | "离线 assembler 与 CLI 的工具目录名（`tools/gameplay_assembler/` + `tools/chart_candidate/`）是否符合既有工具命名惯例" | **接受**这两个目录名；**工具实现仍属后续批次**，本阶段不新建 | 既有工具目录命名惯例见 `tools/` 下的 `asset_importer`、`chart_validator`、`cxc_pack` 等 |

**项数与决策数的差异（如实映射）。** §9 原文是 **5 项**，owner 接受的逐条处置是 **6 条**：第 1 条决策
（新建 `engine/judgement/` 内部 STATIC `cuexis_judgement`）与第 2 条决策（保留 `cuexis_gameplay` stub、
不做别名）**都落在 §9 第 1 项**上——因为原文第 1 项自身把"stub 保持不变或退化为别名"写在同一项里
（见 §2 表方案 A 行）。其余 4 条与 §9 第 2、3、4、5 项**一一对应**，无合并、无拆分，也没有被丢弃的项。
RULING_WORKSHEET 的 S1-04 行把第 1 项的那两条合并为同一句，因此该行自述"5 条文本"；两种计数指向
同一组决策，不构成差异。

**裁定来源与登记状态。** 五项均已登记于 [RULING_WORKSHEET.md](RULING_WORKSHEET.md) §7A 的 **S1-04**
行（§7A"第 1 轮补充：S7A-1 准入闭合"，2026-10-02，owner 已接受）。本次核对确认该行文本与上表逐条
一致：新建 `engine/judgement/` 内部 STATIC `cuexis_judgement`、保留现有 `cuexis_gameplay` stub 与
Runtime 依赖且不做别名、本阶段不拆 `cuexis_input`、采用安装选项 1、`cuexis_judgement` 加入
`CUEXIS_STATIC_IMPLEMENTATION_TARGETS`、接受 `tools/gameplay_assembler/` 与
`tools/chart_candidate/` 名称且实现归后续批次；其退出判据亦与本文件 §9 末段一致。

**登记口径，不是实施。** 本节只登记决策口径；实际 CMake 改动（`engine/judgement/CMakeLists.txt`、
`engine/CMakeLists.txt` 的 `add_subdirectory`、根 `CUEXIS_ACTIVE_TARGETS`、安装列表
`CUEXIS_STATIC_IMPLEMENTATION_TARGETS` 与 `cuexis_verify_target_dependencies(... ALLOWED ...)`
allowlist）以及 `docs/guides/BUILDING.md` 的 `<!-- CUEXIS_ACTIVE_TARGETS_BEGIN -->` 块同步，一律属
**S7A-1 实施批次**（落地顺序见 §6）。RULING_WORKSHEET 第 1 轮的 **D-8**（`cuexis_media_import_tests`
补 `cuexis_verify_target_dependencies` 登记）**也在该批次内落地，不提前**。

**本次登记复核的事实（2026-10-02）。**

- `engine/gameplay/CMakeLists.txt` 当前**仍是 INTERFACE stub**：第 1 行
  `add_library(cuexis_gameplay INTERFACE)`，第 2 行 `cuexis::gameplay` ALIAS，第 12 行仅链接
  `cuexis::core`；文件共 12 行，目录内只有 `CMakeLists.txt` 与
  `include/cuexis/gameplay/tags.hpp`，无 `src/`。
- 根 `CMakeLists.txt` 中 `CUEXIS_STATIC_IMPLEMENTATION_TARGETS` **机制存在**：第 586 行
  `set(CUEXIS_STATIC_IMPLEMENTATION_TARGETS`，第 587-598 行列 12 个 target（含第 596 行
  `cuexis_gameplay`），第 607-612 行在 `CUEXIS_LIBRARY_TYPE STREQUAL "STATIC"` 下
  `install(TARGETS ${CUEXIS_STATIC_IMPLEMENTATION_TARGETS} EXPORT CuexisTargets ARCHIVE DESTINATION ...)`
  ——只导出内部 archive，不安装头文件，即决策 5 要沿用的既有机制。`CUEXIS_PUBLIC_EXPORT_TARGETS`
  在同文件第 580-585 行，含 `FILE_SET HEADERS` 安装（第 605 行），故 `cuexis_judgement` 不进该列表。
- 三个待建位置当前**均不存在，属新建**：`engine/` 下 24 个子目录中没有 `judgement`；`tools/` 下
  11 个子目录中没有 `gameplay_assembler` 或 `chart_candidate`。

**S7A-1 退出判据（本节决策的验收面）。** active-target 清单（`CUEXIS_ACTIVE_TARGETS` 与
`docs/guides/BUILDING.md` target 块逐行相等）、依赖 allowlist、架构检查（`cuexis_architecture_tests`）
与静态 / shared consumer 验证**全部通过**；安装树中**没有**判定头文件——即 `cuexis_judgement` 在
两个安装列表下都只以内部 archive 出现，`install` 树内不出现 `cuexis/judgement/**`（判定类型不进入
公共头，见上表第 3 项"选项 1"）。
