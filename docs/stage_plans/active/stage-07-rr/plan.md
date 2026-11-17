# Stage 7-RR Plan: Review and Refactoring After Stage-7

状态：active；Stage 7 与 Stage 7.5 之间的独立复核与**架构减重重构**阶段

更新日期：2026-11-17

<!-- cuexis-meta
id: PLAN-active.stage-07-rr.plan
status: active
authority: owns
-->

返回[阶段计划索引](../../README.md)；报告入口见[独立阶段报告索引](../../../stage_reports/stages/stage-07-rr/README.md)。
审查发现逐条登记在[发现登记册](findings.md)；当前进度唯一以 [CURRENT_STATUS](../../../CURRENT_STATUS.md) 为准。

## 阶段定位

2026-11-17 的全项目只读复核（10 个并行无上下文子代理 + 抽查）发现：Stage 7 受限退出之后，
仓库里同时存在文档-实现漂移、格式互操作缺陷、诊断码不一致，以及结构层面的能力冗余。
owner 要求把这批发现登记为位于 Stage 7 与 Stage 7.5 之间的独立阶段，命名
**Stage 7-RR：Review and Refactoring After Stage-7**。

同一天 owner 进一步明确本阶段的方向：

- **以架构减重重构为主**：现状被描述为"2 条产品线 × 5 条格式线 × 3 套时间 × 2 条渲染路径"，
  过于臃肿，必须重新深入分析后减重。
- **取消文件拆分工作**：全仓只有 1 个 `.cpp` 超过 800 行（`tests/pd/execution_memory_observer.cpp`，
  917 行），不再为此投入；行数预算只作为事实记录保留在[发现登记册 §4](findings.md)。
- 文档与代码注释同样纳入减重范围：现行文档臃肿且存在与实现不符的描述。

`RR` 后缀只表达路线位置（Stage 7 之后、Stage 7.5 之前），**不是** Stage 7 的子阶段，
也不改变 Stage 8–14 编号；与 `Stage 7-RPA`（Stage 7 内的实时架构工作线）不是同一对象。

## 阶段目标

在**不扩大**任何格式、capability、SDK 或生产保证范围的前提下：

1. 用代码证据回答"结构到底臃肿在哪里"：模块依赖、格式线、时间源、渲染路径、会话层次、
   公共 API 面各自有几套实现、各自由谁消费、各自的真实成本。
2. 在这些证据之上做出减重决策：合并、下沉、删除或显式标注为 candidate/deferred；
   每个被减掉的对象都要写明它支撑的已接受合同与历史证据如何处理。
3. 消除现行状态与文档对版本、分支、阶段归属和格式状态的事实冲突。
4. 修正"文档承诺存在而实现不存在"或"实现比文档更严/更松"的合同项；无法在本阶段实现的，
   显式标注为未实现或待裁定。
5. 让同一失败在文档、实现和宿主观测面得到同一个稳定码；收敛静默失败。
6. 把结论、处置理由与残留缺口写进 Stage 7.5 / Stage 8 交接，而不是留在会话记录里。

## 前置基线与入场条件

- 复核基线：`aac83e5e`（2026-11-16），SDK API `0.9.1`，日期版本 `26.11.16-1`。
- Stage 7 已按 R4 受限退出交接
  （[受限退出与缺陷交接](../../../stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-14-limited-exit-and-defect-handoff.md)）；
  本阶段不重开其判定，不改写历史失败、未认证范围或 testOnly 状态。
- Stage 7.5 尚未开始产品实施，因此本阶段排在其前；Stage 7.5 的入场前置增加
  "本阶段当前计划范围完成并取得 owner 接受"。
- **合同先于实现**：任何改变 wire 字节、接受集合或稳定诊断码语义的修复，必须先更新对应
  ADR/Spec 取得接受，再由实现跟进。不得以"代码已经是新行为"为由回写合同，
  也不得让 Reader 双向静默接受两种互不兼容的解释。
- **减重不等于删证据**：删除或合并某条实现线时，其历史失败、未认证范围与 hosted 证据保持原样；
  只允许新增"该线已退出"的结论，不允许把旧结果改写为通过。

## 研究批次（RR-0，先行）

减重决策必须建立在代码证据上，因此先做一轮架构研究，产出六份专题：

| 专题 | 要回答的问题 |
| --- | --- |
| 依赖与分层 | 真实模块图是什么？哪些边是政策、哪些是意外？哪些模块消费者≤1 但体量很大？哪些是历史残留目录？ |
| 双产品线 | `PlayerController` 中立链与 `execution_host` candidate 链各自覆盖什么？同语义实现重复多少？合并要付什么代价？ |
| 格式线 | 到底有几条格式线？哪些只有 Reader 无 Writer、只被测试消费、或只有文档？最小可用格式集是什么？ |
| 时间源 | ChartClock / HostClock / CuexisAudio / tick / delta 之间如何转换？有没有重复转换与死字段？能否收敛为一个时钟接口 + 一条时间轴？ |
| 渲染路径 | 后端中立 `presentation_renderer` 与 OpenGL 直连各自支撑什么？功能对等性如何？只留一条要改什么？ |
| 会话层次 | RuntimeSession / PlaybackSession / execution_host SessionOwner 是三层内核还是三份重复事务？能否"一个内核 + 两个薄适配"？ |

研究同时覆盖两项横向内容：公共 API 面（安装头、candidate 泄漏、零消费者符号）与
文档/注释失真（重复承载、可机械断言的命中率、注释与实现不符）。

研究结论进入[发现登记册](findings.md)的第二版；**研究本身不修改任何产品代码**。

**进展（2026-11-17）：RR-0 研究已完成。** 8 份专题结论已整理为
[架构与实现研究](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-research.md)，
条目并入[发现登记册 §6](findings.md#6-架构层发现2026-11-17-研究)。研究给出 6 个减重选项
（R1 构建隔离、R2 候选面降级、R3 会话收敛、R4 时间收敛、R5 渲染收敛、R6 文档减重）
与建议路线 `R1 → R5 → R2 → R3 → R4 → R6`；**选项尚未被 owner 选定，RR-3 未开始**。

## 重构批次

| 批次 | 范围 | 退出条件 |
| --- | --- | --- |
| RR-1 状态与文档漂移 | P0/P1 中的版本、分支、阶段归属、格式状态与 SDK 版本陈述 | 现行文档不再与 `CuexisVersion.cmake`、`vcpkg.json`、分支事实矛盾；历史基线标注为历史 |
| RR-2 合同与诊断收敛 | Packed Header CRC/GridDelta、Chart Beat 上限、CXC playback entry、CXT v2 §6 未实现面；`same_tick_collision` 码互换、`catch(...)` 三态、`player.execution.test_only` 单码、`packed.io.*` 双入口 | 每条先有 Spec 裁定或"标注未实现"结论；同一失败只有一个稳定码；"静默成功"路径被消除或显式登记 |
| RR-3 架构减重 | 按 RR-0 结论处理：格式线最小集、双产品线合并、时间源收敛、渲染路径收敛、会话层收敛 | 每个被删除/合并的对象有具名决策、受影响合同清单与回归证据；不破坏受限退出边界 |
| RR-4 公共面收缩 | candidate 入口移出生产安装面、`cuexis_internal/` 不外泄、零消费者符号处理、版本权威单一化 | production 构建的安装头不再暴露 candidate 入口；缩面不影响 external consumer / reference host 门禁 |
| RR-5 文档与注释减重 | 同一合同只保留一处权威、过程文档归档出仓库、注释与实现对齐 | 机械断言类文档内容有测试保护或被删除；注释失真样例被清除并加入检查 |
| RR-6 退出与交接 | 汇总处置、残留缺口、Stage 7.5 / Stage 8 交接清单 | owner 接受退出报告；CURRENT_STATUS、AGENTS、ROADMAP 与索引同步；不声称未取得的 hosted/跨平台结论 |

顺序：RR-0 结果先于 RR-3 的任何删除动作；RR-3 的"合并/删除"与 RR-2 的"合同修正"不得在同一
对象上同时动工（先定语义，再动结构）。RR-1、RR-4、RR-5 可与 RR-2/RR-3 并行。

## 验收标准

每个批次退出时都要同时具备：

```text
发现条目的具名处置（fixed / deferred with reason / rejected with evidence）
实现与 focused tests（或"只改文档"的明确说明）
失败与回滚路径不被本次修改削弱
文档、状态与交接同步
```

本阶段整体退出还需：

- [发现登记册](findings.md) 中每条 P0/P1 都有处置结论；P2 允许"本阶段 deferred"，
  但必须写明归属阶段与恢复条件，不能只标 TODO。
- 减重后的结构有**删除清单**：每个被移除的实现线列出它服务的 ADR/Spec、受影响的测试与证据、
  以及"为什么现在可以移除"的裁定依据。
- 不改变已接受语义：受影响测试二进制全绿；改变接受集合的修复附前后 fixture 对照。
- `python -B tools/check_docs.py`、`git diff --check`、`cuexis_format_check` 通过。
- 不声称任何未实际取得的 hosted、跨平台、GPU 或设备结论。

## 排除项

- **不做纯行数拆分**：全仓只有 1 个 `.cpp` 超 800 行，owner 已取消该项工作；不为凑预算拆文件。
- 不实现 Stage 7.5 的 S7B-0–4 / S7C-1/2 能力，不做 GE-R 性能整改与预算测量。
- 不发布、不合并、不提升 SDK minor，不稳定 C ABI（归 Stage 14）。
- 不重跑、不改写历史失败与未认证范围，不提高 caps，不补造证据。
- 不修改判定窗口、格式 wire 布局或 identity 语义本身——这类变更若必要，先立 ADR/Spec 修订，
  由 owner 接受后另行排期，不塞进重构批次。
- 不把"复核发现"写成"已修复"；本计划登记的是问题与决策，不是修复声明。

## 进展记录

- 2026-11-17：RR-0 架构研究完成（[摘要](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-research.md)、
  [证据](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-evidence.md)），6 个减重选项待 owner 选定。
- 2026-11-17：RR-1/RR-5 的**文档治理机械层**完成（[报告](../../../stage_reports/stages/stage-07-rr/2026-11-17-documentation-refactor.md)）：
  170 篇合同/计划补元数据、生成 `docs/MAP.md` 与 `AGENTS.md` 状态块、424 篇证据报告加统一头、
  计划状态纠偏、7 处假断言订正、新增 `check_docs_facts.py` 并接入预推送。
  合同语义重写与 R3/R4/R5 相关正文仍未开始。
- 2026-11-17：**假保证与假实现扫描**完成（[报告](../../../stage_reports/stages/stage-07-rr/2026-11-17-false-guarantees-scan.md)）：
  7 个子代理 → 48 条候选，26 条经复核确认（其中 4 条属文档过期，已就地修正）。
  最高危为 `ChartWriter` 保存丢 timing/模板/keyframe（RR-F401）与三个恒为常量的诊断字段
  （RR-F419/F420）。**未开始修复**；处置顺序见报告末节。
- 2026-11-17：**登记册全量核查**：6 个无上下文子代理分批核查 91 条未决项（内部 58 + 外部 33），
  结果写回 `findings.md`（成立 56 / 部分成立 33 / 不成立 1 / 无法核实 1）；净推翻或收窄 18 处断言，
  含 RR-F125、RR-F143、RR-F202、RR-F206、RR-F315 与外部 T-02、D-25 等会误导修复方向的说法；
  全册 112 条均有结论列，“未复核”归零。
- 2026-11-17：文档治理收尾：`AGENTS.md` 由 357 行精简为 50 行（只保留路由与硬约束，
  事实改为指向 MAP/CURRENT_STATUS/guides），`status_contract.json` 与 `check_agents_guide`
  的必需片段同步；ADR 0027 增补 Judgement 内部 STATIC 模块边界。随后按两个无上下文子代理的
  验证结果修复生成块死链（AGENTS 链接加 `docs/` 前缀、新增 `current_register` 与 AGENTS 链接门禁），
  并归档外部 59 条审查报告、补登记 RR-F501–F510。

## 归档来源：

- 2026-11-17 全项目只读复核的 10 份子代理报告与 owner 抽查记录（会话内，未入库逐份文本；
  结论逐条转写进[发现登记册](findings.md)，并标注是否经独立复核）。
- 同日派出的 8 份架构研究子代理报告（依赖分层、双产品线、格式线、时间源、渲染路径、
  会话层次、文档与注释、公共 API 面），结论并入[发现登记册](findings.md)第二版。
- 上一轮源码拆分及其独立审计：[源码拆分安全网独立审计](../../../stage_reports/stages/stage-07/90-shared/source-split-audit/index.md)。
- Stage 7 受限退出与缺陷交接：
  [R4 交接](../../../stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-14-limited-exit-and-defect-handoff.md)。
