# Stage 7A typed contract review（S7A-1 冻结顺序第 4 步）

状态：completed

快照日期：2026-10-02

后续关闭证据：本报告 §7 的门禁结论与 §8 的逐项关闭证据；本报告不改变任何 V2 文档、Gameplay I
文档或计划正文的状态与内容，只登记审查结论与未决点。

上级文档：[Stage 7A 实施计划](../../../stage_plans/active/stage-07/plan.md) ·
[S7A-0 owner 接受记录](2026-10-02-s7a-0-acceptance.md)

## 1. 审查对象与依据

**审查对象。** 冻结顺序的第 4 步"Stage 7A typed contract review"（计划 §1.1 的冻结顺序块第 4 行，
[plan.md:86](../../../stage_plans/active/stage-07/plan.md)；执行时点表见
[S7A-0 接受记录 §4](2026-10-02-s7a-0-acceptance.md) 第 78 行）。该步的目标是把计划 §S7A-1 的
"必须冻结的对象"清单与其在 V2 文档中的实际落点逐项对齐，并预检模块/target 边界与头文件约束，
**不是**新合同、不是字段定值。

**依据文档（本次全部实际读取）。**

| 角色 | 文档 |
| --- | --- |
| 阶段计划（冻结顺序、S7A-1 范围与验证） | [plan.md](../../../stage_plans/active/stage-07/plan.md) §1.1（73-109）、§S7A-1（216-264）、§5.3（614）、204、878 |
| V2 决策 | [ADR 0044](../../../adr/0044-gameplay-v2-semantic-kernel.md) |
| V2 字段与运行语义（唯一权威 Spec） | [GAMEPLAY_V2_SPEC.md](../../../formats/GAMEPLAY_V2_SPEC.md) |
| V2 typed 边界（类型清单、所有权、单位、诊断） | [GAMEPLAY_V2_ABI.md](../../../api/GAMEPLAY_V2_ABI.md) |
| 准入包与裁决协议 | [acceptance README](../../../proposals/gameplay-v2-acceptance/README.md)、[RULING_WORKSHEET.md](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)、[CONTRACT_MATRIX.md](../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md) |
| 边界与目标草案 | [TARGETS_AND_DEPENDENCIES.md](../../../proposals/gameplay-v2-acceptance/TARGETS_AND_DEPENDENCIES.md)、[FORMAT_ENTRY_AND_IDENTITY.md](../../../proposals/gameplay-v2-acceptance/FORMAT_ENTRY_AND_IDENTITY.md)、[SUPPORT_AND_REJECTION_MATRIX.md](../../../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md) |
| 历史对照（Gameplay I，`superseded` / `historical`） | [GAMEPLAY_JUDGEMENT_ABI.md](../../../api/GAMEPLAY_JUDGEMENT_ABI.md) |
| 仓库约束 | [AGENTS.md](../../../../AGENTS.md)（189-214、236-243）、[DOCUMENTATION_POLICY.md](../../../DOCUMENTATION_POLICY.md)（32-50）、根 [CMakeLists.txt](../../../../CMakeLists.txt)、[VerifyArchitecture.cmake](../../../../cmake/VerifyArchitecture.cmake) |

**本次审查不做的事。** 不为命名、整数宽度、所有权、异常边界、序列化定值（计划 228 行已把这些
交给"实现批次与 ABI 一起冻结"；[ABI:44](../../../api/GAMEPLAY_V2_ABI.md)、
[ABI:426](../../../api/GAMEPLAY_V2_ABI.md)）；不设计新类型；不改动任何被审查文档；不运行 C++
构建或 CTest（本报告的唯一证据是工作区文本与行号）。

## 2. 冻结对象逐项映射

计划 §S7A-1 的"必须冻结的对象"在
[plan.md:220-227](../../../stage_plans/active/stage-07/plan.md) 一行式列出，
[plan.md:228](../../../stage_plans/active/stage-07/plan.md) 说明这五类细化值由实现批次与 ABI
一起冻结。下表把该清单**逐项展开为 17 项**（不合并、不省略），并给出 V2 ABI 类型清单条目、
V2 Spec 章节与处置。

处置词只用：`同名保留` / `改名` / `拆分` / `不再适用（被取代）` / `缺少对应`。

| # | plan 220-227 的对象 | V2 ABI 条目（行号） | V2 Spec 章节 | 处置 |
| --- | --- | --- | --- | --- |
| 1 | `Tick` / 时间区间 | `Tick`（61，宽度待冻结）、`TimeInterval`（66）；四域见 62-65 | §5.2 `timebaseRef` 行（276）；运行语义正文缺失（见 F-05） | 同名保留（宽度与转换边界属第 2 轮，见 F-10） |
| 2 | `InputEvent` | 取代映射（546）；V2 载体为 `NormalizedObservation`（79） | §7.1（384-389）、§5.2（297） | 不再适用（被取代） |
| 3 | `InputDomain` | `InputDomain`（82，枚举集待冻结） | §6.4 `supportedDomains`（359-361） | 同名保留（枚举集属 P2-05，见 F-09） |
| 4 | `Action` | `InputAction`（83） | §7.1（384-389），无专章 | 改名 |
| 5 | 位置 / 通道 | 通道 `ChannelRef`（84）；位置归 `DomainAmount`（86"位置 / 量"） | §5.2 判定域与几何行（288）、§7.2 R-12（406） | 拆分 |
| 6 | `Amount` | `DomainAmount`（86，量程待冻结） | §5.2（288） | 改名 |
| 7 | `SourceIdentity` | **无同名 V2 类型**；对应字段为 `SourceClass`（87）；字段级映射表未登记该去向（552-562） | §5.2 `sourceMap`（293）、工具链 profile（294）；字段级对应见 CM-T05（CONTRACT_MATRIX:91） | 改名（目标名候选，见 F-04） |
| 8 | sequence | `IngressSequence`（81）、`EventSequence`（169）；映射（557、561） | §5.2 会话内计数器行（299） | 拆分 |
| 9 | `JudgementRequirement` | 取代映射（546）；V2 载体为 `RequirementRecord`（100） | §4（226-245） | 不再适用（被取代） |
| 10 | `PatternRef` | `PatternRef`（104） | §5.2（286）、§7.2 R-02/R-11（396、405） | 同名保留 |
| 11 | `MeasureSpec` | `MeasureSpec`（109，字段集待冻结） | §5.2（286） | 同名保留 |
| 12 | `GripPolicy` | 处置说明（120-121）、字段映射（558）；替代为 `PreparedGrace`（113）+ `GraceResolutionPolicy`（114）+ `ResourceClaimIntent`（115） | §5.2 `preparedGrace` 与 `resourceDecisionPolicyRef` 行（287） | 拆分 |
| 13 | `JudgementFact` | 取代映射（546）、字段映射（560）；V2 权威事实为 `FactRecord`（157） | §5.2（290-291）、§9.4（505-510） | 不再适用（被取代） |
| 14 | `JudgementResult` | `JudgementResult`（168，改为只读投影，见 172-173） | §10（533-536），无专章 | 同名保留 |
| 15 | `JudgementIdentity` | `JudgementIdentity`（210）、四分量保留（562）、另加三条 identity（207-217） | §5（247-319），尤其 §5.3（304-311） | 同名保留 |
| 16 | 诊断 | `JudgementError`（244）、`DiagnosticCode`（245）、`DiagnosticCategory`（246）、`DiagnosticSeverity`（247）、四层（362-372） | §9（478-515） | 拆分 |
| 17 | capability | `CapabilityId`（235）、`CapabilityRevision`（236）、`CapabilityRecord`（237）、`CapabilityState`（238）、`DeclaredCapabilitySet`（239）、`DerivedCapabilityClosure`（240） | §6（321-378） | 拆分 |

统计：同名保留 6 项（1、3、10、11、14、15）、改名 3 项（4、6、7）、拆分 5 项（5、8、12、16、17）、
不再适用（被取代）3 项（2、9、13）、缺少对应 0 项，合计 17 项。

**边界声明。** 处置列是本报告的审查结论，不是新合同。除 ABI 明确标 `冻结目标` 的类型名外，所有
候选名按 [ABI:44](../../../api/GAMEPLAY_V2_ABI.md) 与
[ABI:426](../../../api/GAMEPLAY_V2_ABI.md) 仍待冻结；本报告不为第 7 项的最终命名、任何整数宽度、
所有权、异常边界或序列化定值。

**规模核对（供范围界定用）。** ABI 的类型清单共 **9 域**（域头 57、75、95、123、153、175、199、
231、264），每域条目分别标注 `冻结目标` / `待冻结`（定义见
[ABI:52-55](../../../api/GAMEPLAY_V2_ABI.md)）；接受记录原称其 **144 条目**
（[接受记录:90](2026-10-02-s7a-0-acceptance.md)）。本次按表格行自查为 126 行，并复核了分域计数：
9 个域相加同为 **126**，故「144」应属口径误记，接受记录已据本报告订正为 126。个别行含两个候选名，
若要严格区分「行数」与「类型名数」应以 ABI 的实际条目为准（该差异不改变本报告的范围界定结论）。计划 220-227 的 17 项**不是**该清单的投影，两者的关系属未裁定项
（见 F-08）。

### 2.1 plan 冻结清单与 V2 文档的不一致（逐条）

1. **清单来源与实施基线冲突（最关键）。** plan:220-227 的 17 个对象与
   [GAMEPLAY_JUDGEMENT_ABI.md:27-68](../../../api/GAMEPLAY_JUDGEMENT_ABI.md) 的 Gameplay I
   候选类型/字段逐字对应（`Tick observationTime`、`Action action`、`ChannelOrPosition position`、
   `Amount amount`、`SourceIdentity source`、`uint64_t sequence`、`GripPolicy grip`、
   `JudgementFact`、`JudgementResult`、`JudgementIdentity`）。而同一计划
   [75-78 行](../../../stage_plans/active/stage-07/plan.md) 写明 Gameplay I 三份文档"仅作为历史
   候选基线和差异对照"，实施授权必须来自 V2；ADR 0044:15 同旨。即 S7A-1 的冻结清单文本仍是
   Gameplay I 词汇（见 F-01）。
2. **`GripPolicy`（plan:226）** 已被 [ABI:120-121](../../../api/GAMEPLAY_V2_ABI.md) 与字段映射
   [ABI:558](../../../api/GAMEPLAY_V2_ABI.md) 取代为三个类型（`PreparedGrace` /
   `GraceResolutionPolicy` / `ResourceClaimIntent`），plan 文本未同步（见 F-02）。
3. **`InputEvent`、`JudgementRequirement`、`JudgementFact`** 由
   [ABI:546](../../../api/GAMEPLAY_V2_ABI.md) 整行取代为 `NormalizedObservation` /
   `RequirementRecord` / `FactRecord`。
4. **`Amount` → `DomainAmount`**（ABI:86，量程待冻结）；**`Action` → `InputAction`**（ABI:83）。
   两者是改名而非新类型。
5. **"位置 / 通道"在 V2 被拆到两个类型**：通道 → `ChannelRef`（ABI:84）；位置 → `DomainAmount`
   （ABI:86 角色写"位置 / 量"）。plan 的单一条目在 V2 无单一对应项。
6. **`SourceIdentity` 在 V2 无同名类型，且字段级映射表（ABI:552-562）未登记其去向**（见 F-04）。
   V2 的 `SourceClass`（ABI:87，"设备类别（不含序列号）"）与
   [CONTRACT_MATRIX:91](../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md) 的
   `NormalizedObservation` 字段集（含 `sourceClass`）指向同一位置；但 V2 另有语义不同的
   `SourceBuildIdentity`（ABI:207），两者不得混用。
7. **`sequence` 在 Gameplay I 有两个含义**（`InputEvent.sequence` 见
   [GAMEPLAY_JUDGEMENT_ABI.md:35](../../../api/GAMEPLAY_JUDGEMENT_ABI.md)；
   `JudgementResult.eventSequence` 见同文件 59），plan:223 的单一 "sequence" 在 V2 被拆为
   `IngressSequence`（ABI:81、557）与 `EventSequence`（ABI:169、561）。
8. **"诊断和 capability"（plan:227）在 V2 是多类型域**：诊断 4 个类型（ABI:244-247）+ 四层结构
   （ABI:362-372）；capability 6 个类型（ABI:235-240）。plan 的并列名词不构成可冻结单元。
9. **`Tick` / 时间区间的宽度**：plan:220-227 要求 S7A-1 冻结，ABI:61 标宽度待冻结，ABI:575 与
   [RULING_WORKSHEET:104-105](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) 把
   Tick 宽度与 ChartTick↔ObservationTick 转换边界/ tie rule 排到 **第 2 轮**（S7A-2 前）。
10. **`InputDomain` 枚举集**：ABI:82 标"枚举集待冻结（P2-05）"，P2-05 在第 **6 轮**
    （[RULING_WORKSHEET:159](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)、
    Spec:586）。

## 3. 待冻结项清点

五类细化值的登记状态如下。"归属"列给出实际负责方；"本次不定值"是统一声明：本报告对五类均不
给出取值、宽度、所有权归属结论、异常类别或编码形式。

| 类别 | ABI 是否登记为 `待冻结` | 归属 | 本次审查不做什么 |
| --- | --- | --- | --- |
| 命名 | 是：ABI:44（候选名声明）、ABI:426（"最终命名…与 ABI 一起冻结"）、ABI:573（未决 #3）；状态列标 `待冻结` 的行见 67、69、85-86、108、110-111、132、134、141、143-144、162-164、182、186-189、216-217、237、245-247、270-272、275 | 实现批次（S7A-1 起）与 ABI 一起冻结（plan:228） | 不确认任何候选名，不为第 2 节第 7 项定名 |
| 整数宽度 | 是：ABI:61（`Tick` 宽度待冻结）、ABI:426、ABI:573；另见 ABI:341-349（不登记量程数值） | Tick 宽度属 ABI 批次，其转换边界/tie rule 属第 2 轮（ABI:575；CM-T03/CM-T04）；其余宽度属实现批次与 ABI | 不给任何宽度，不把研究切片数值写成宽度或量程 |
| 所有权与生命周期 | **部分**：规则已在 ABI:300-321（所有权表）与 ABI:408（7A 承诺项）写为承诺；但"仍待冻结"表（ABI:424-440）**未**登记所有权，ABI:573 也未列 | plan:228 归实现批次与 ABI；ABI 侧实际把规则写成了承诺而非未决项（见 F-03） | 不新增所有权规则，不判定 `GameplaySession`（候选名）的最终持有形式 |
| 异常边界 | **未登记为待冻结**：ABI:357-358 与 ABI:408-409 把它写成硬约束（`Result` 通道、异常不得跨模块公共边界），AGENTS.md:239 同；ABI:424-440 与 573 均未把它列为未决 | 同上（见 F-03） | 不改写该约束，不定义异常类别或错误码 |
| 序列化 | 是：ABI:428（"序列化字节与线格式"）、ABI:573；ABI:446 另声明不承诺字节布局 | 实现批次 + Packed/CXC candidate wire revision（FORMAT_ENTRY_AND_IDENTITY §7，127-136）；Replay/Snapshot 字段集属第 5 轮（ABI:435-436） | 不定义字节序、编码、wire revision 或 section 形状 |

**统一声明。** 五类细化值在本报告中**全部保持未冻结**。计划 228 行的授权是"由实现批次与 ABI
一起冻结"，本次审查只登记落点与归属，不构成冻结事件。

## 4. 模块与 target 边界预检

### 4.1 S7A-1 需要的模块位置与 target 方向

- 计划要求 S7A-1"选择模块位置和 target 方向"，使 Input/Judgement/Replay 不依赖 SDL、OpenGL、
  Audio、World、EnTT 或 JSON DOM，公共头只出现 Cuexis 类型与已批准的 `Result`
  （[plan.md:253-254](../../../stage_plans/active/stage-07/plan.md)）。
- 现状：`engine/gameplay/` 是 INTERFACE stub，只有 `tags.hpp` 与 `CMakeLists.txt`，无 `src/`；
  仓库不存在 `cuexis_judgement` / `cuexis_input`
  （[TARGETS:27](../../../proposals/gameplay-v2-acceptance/TARGETS_AND_DEPENDENCIES.md)）。
- 两个候选方案（新增 `engine/judgement/` 的 `cuexis_judgement`，或把 `engine/gameplay/` 升级为实
  模块）与五项待 owner 决策见
  [TARGETS §2、§3、§9](../../../proposals/gameplay-v2-acceptance/TARGETS_AND_DEPENDENCIES.md)
  （31-46、47-64、174-231）。package 建议方案 A，但选择本身未裁定（见 F-12）。
- 建议新增 target 与 ALLOWED 草案（`cuexis_judgement` 只允许 `cuexis::core`、`cuexis::chart`）见
  [TARGETS §3、§4](../../../proposals/gameplay-v2-acceptance/TARGETS_AND_DEPENDENCIES.md)
  （47-64、65-83）；禁止依赖清单见同文件 79-82。
- 新增一个内部模块的 15 步落地顺序（含 `CUEXIS_ACTIVE_TARGETS`、安装列表、allowlist、
  `BUILDING.md` target 块同步）见
  [TARGETS §6](../../../proposals/gameplay-v2-acceptance/TARGETS_AND_DEPENDENCIES.md)（106-128）。

### 4.2 allowlist 更新点（已核实）

| 更新点 | 位置 | 校验机制 |
| --- | --- | --- |
| 新 target 登记 | 根 `CMakeLists.txt` `set(CUEXIS_ACTIVE_TARGETS`（117），测试 target 条件块（178-231） | `cuexis_verify_active_targets` 比较构建系统 target 集合与 allowlist，不等即 `FATAL_ERROR`（VerifyArchitecture.cmake:245-273；调用点 CMakeLists.txt:1163-1166） |
| 依赖 allowlist | 根 `CMakeLists.txt` 的 `cuexis_verify_target_dependencies(... ALLOWED ...)` 段（媒体/工具 934-961、测试 963-1161） | 校验 `LINK_LIBRARIES` / `INTERFACE_LINK_LIBRARIES` ⊆ ALLOWED，越界即 `FATAL_ERROR`（VerifyArchitecture.cmake:288-324） |
| 编译告警与格式 | warnings 循环（CMakeLists.txt:529-531）、`cuexis_format_check`（533-563） | 新 target 需在告警循环之前登记 |
| 安装边界 | `CUEXIS_PUBLIC_EXPORT_TARGETS` / `CUEXIS_STATIC_IMPLEMENTATION_TARGETS`；硬编码禁止内部 target 进入（VerifyArchitecture.cmake:219-229） | 安装组件目前只有 Core / Playback / Content / Audio / AudioSDL（TARGETS:97） |
| 文档 target 块 | `docs/guides/BUILDING.md` 的 `<!-- CUEXIS_ACTIVE_TARGETS_BEGIN -->` 块（18-84），与 `out/build/debug/generated/cuexis-targets.txt` 逐项比较 | `tools/check_docs.py` 的 target contract 检查（`TARGET_BLOCK_BEGIN` 见 check_docs.py:29-30，检查见 392 起） |

### 4.3 D-8 核实结果

**D-8 成立，且比登记时更具体。**

- 登记文本：`cuexis_media_import_tests` 在 active 列表但无 `cuexis_verify_target_dependencies`
  调用（[RULING_WORKSHEET:96](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)）；
  处置为并入 S7A-1 的 allowlist 批次、不单独先落（同文件第 51 行 Codex 裁决）。
- 核实：该 target 在根 [CMakeLists.txt:226-228](../../../../CMakeLists.txt) 条件进入
  `CUEXIS_ACTIVE_TARGETS`；全仓检索 `cuexis_verify_target_dependencies(cuexis_media_import_tests`
  **零命中**，而相邻的 `cuexis_asset_publish_tests` 有登记（同文件 954-961），
  `cuexis_media_import`（935-944）与 `cuexis_media_importer`（947-952）也都有。
- 影响面：`CUEXIS_BUILD_MEDIA_TOOLS` 打开时（CMakeLists.txt:170-176；preset
  `debug-media-tools`，[CMakePresets.json:56-61](../../../../CMakePresets.json)）该 target 激活，
  其直接依赖（`cuexis::core`、`cuexis::media_import`、`Catch2::Catch2WithMain`，
  [tests/media_import/CMakeLists.txt:10-15](../../../../tests/media_import/CMakeLists.txt)）
  不经 allowlist 门禁。默认 `debug` 树的
  `out/build/debug/generated/cuexis-targets.txt` 不含媒体 target，故
  [BUILDING.md:18-84](../../../guides/BUILDING.md) 的 target 块与该产物仍逐项相等。
- **处置：按已接受的第 1 轮裁决，本项归入 S7A-1 的 allowlist 批次，本次不单独先行落地**，也不
  构成 S7A-1 启动的前置阻塞项。

## 5. 头文件与安装边界约束清单（S7A-1 检查项）

以下约束来自 [AGENTS.md](../../../../AGENTS.md) 与
[VerifyArchitecture.cmake](../../../../cmake/VerifyArchitecture.cmake)，逐条可作为 S7A-1 的机械
检查项：

1. **命名与布局惯例**：类型 PascalCase、函数/变量 camelCase、命名空间 `cuexis::module`、文件名
   `snake_case.hpp`、公共头在 `include/cuexis/<module>/`（AGENTS.md:228-236）。
2. **安装公共头纯 ASCII**：`engine/*/include/cuexis/**/*.hpp` 不得含 CJK 注释或非 ASCII 标点，
   注释写英文（AGENTS.md:237；脚本检查 VerifyArchitecture.cmake:173-178）。
3. **安装公共头不得出现 GLM 类型**（AGENTS.md:200；VerifyArchitecture.cmake:179-182）。
4. **错误通道**：可恢复错误用 `cuexis::core::Result<T, E>`，不得忽略 `Result`；不得用异常表达可
   恢复失败（AGENTS.md:238；ABI:355-358）。
5. **异常边界**：异常不得跨模块公共边界；析构与实时路径不得抛异常（AGENTS.md:239；
   ABI:312、358）。
6. **第三方类型不得泄漏进公共 API**（AGENTS.md:240）。
7. **所有权**：不得跨模块边界 `new`/`delete`；非拥有观察用裸指针（AGENTS.md:241；ABI:311）。
8. **模块中立性（若判定模块安装公共头，将被并入下列既有扫描）**：Core 不得含 SDL/glad/GL/平台
   头（VerifyArchitecture.cmake:9-11）；Chart 不得含 EnTT/World/Audio/SDL（20-21）；Runtime 与
   Playback 不得含适配器/渲染器/着色器编译器头（92-105）；nlohmann 类型不得逃出
   `engine/json_support/`（156-160）；OpenGL 头与调用只允许在 `engine/render_opengl/`
   （162-170）；安装 Playback 头不得暴露 EnTT/SDL/OpenGL/JSON/spdlog/RuntimeSession/World
   （186-198）。
9. **判定模块的既有未来约束**：`cuexis_judgement` 不得依赖 platform、audio、render backend 或
   宿主引擎 SDK（AGENTS.md:214）。
10. **target 门禁**：新 target 必须同时进入 `CUEXIS_ACTIVE_TARGETS` 与依赖 allowlist；测试 target
    放入 `BUILD_TESTING` 条件块（AGENTS.md:206）。
11. **安装列表硬约束**：`cmake/VerifyArchitecture.cmake` 硬编码禁止
    `cuexis_presentation_renderer` / `cuexis_player_support` 进入两个安装列表（219-229）；新模块若
    保持内部，需要同样的硬约束或明确不进列表（TARGETS:99-100）。
12. **验收验证项（计划 263-264）**：头文件泄漏扫描、依赖 allowlist、架构脚本、静态/shared 安装
    consumer、异常与 ownership 编译检查、无 GPU 的空会话、prepare 失败与销毁路径。

本报告不判定第 8 项扫描是否需要扩展——安装与否是 [TARGETS §5、§9](../../../proposals/gameplay-v2-acceptance/TARGETS_AND_DEPENDENCIES.md)
（84-105、174-231）的未裁定决策。

## 6. 发现的问题

分类：`缺陷` / `未裁定` / `无问题`。每条给出 `file:line` 与证据要点。

| ID | 分类 | 位置 | 证据与说明 |
| --- | --- | --- | --- |
| F-01 | 缺陷 | [plan.md:220-227](../../../stage_plans/active/stage-07/plan.md) vs [GAMEPLAY_JUDGEMENT_ABI.md:27-68](../../../api/GAMEPLAY_JUDGEMENT_ABI.md) | S7A-1 的"必须冻结的对象"清单逐字来自 Gameplay I 候选类型/字段（`Tick observationTime`、`Action`、`ChannelOrPosition`、`Amount`、`SourceIdentity`、`sequence`、`GripPolicy grip`、`JudgementFact`、`JudgementResult`、`JudgementIdentity`），与本计划 75-78 行"Gameplay I 仅作历史基线"及 ADR 0044:15 冲突。这是 plan 文本与 V2 文档不一致，不是语义未决 |
| F-02 | 缺陷 | [plan.md:226](../../../stage_plans/active/stage-07/plan.md) vs [ABI:120-121](../../../api/GAMEPLAY_V2_ABI.md)、[ABI:558](../../../api/GAMEPLAY_V2_ABI.md) | `GripPolicy` 已被 `PreparedGrace` + `GraceResolutionPolicy` + `ResourceClaimIntent` 取代（一拆三），plan 清单未反映 |
| F-03 | 缺陷 | [plan.md:228](../../../stage_plans/active/stage-07/plan.md) vs [ABI:573](../../../api/GAMEPLAY_V2_ABI.md)、[ABI:424-440](../../../api/GAMEPLAY_V2_ABI.md) | plan:228 列出五类待冻结（命名/整数宽度/所有权/异常边界/序列化）；ABI 未决 #3（573）只列四类（命名/宽度/字段集/序列化），"仍待冻结"表（424-440）未登记所有权与异常边界——ABI 反而把它们写成了 7A 硬承诺（300-321、357-358、408-409）。两侧文本对"哪些仍需冻结"不一致 |
| F-04 | 缺陷 | [ABI:552-562](../../../api/GAMEPLAY_V2_ABI.md) vs [GAMEPLAY_JUDGEMENT_ABI.md:34](../../../api/GAMEPLAY_JUDGEMENT_ABI.md) | ABI 的字段级映射表登记了 `observationTime`/`sequence`/`grip`/`measures`/`JudgementFact` 四字段/`eventSequence`/四分量，但**未登记 `InputEvent.source`（Gameplay I 的 `SourceIdentity`）的去向**；V2 侧对应字段为 `SourceClass`（ABI:87）与 `sourceClass`（CONTRACT_MATRIX:91）。替代关系不完整 |
| F-05 | 缺陷 | [Spec:46](../../../formats/GAMEPLAY_V2_SPEC.md) vs Spec 章节体（20-596） | Spec §1.1 第 2 项声明拥有"运行语义：时间域、Tick 阶段、仲裁、资源、Fact Ledger、Ruleset 事务、表现桥接"，但文档没有任何对应章节（各 `##`/`###` 标题见 20、41、89、149、226、247、321、380、435、478、517、574、596）；最近覆盖为 §9.3/§9.4（494-510）与 §10（517-547）。Spec:584 还把"Tick 阶段对应"列为第 4 轮受影响部分，指向不存在的章节。ABI 的域 1/3/4/5/6 类型因此缺少 Spec 章节可对 |
| F-06 | 缺陷 | [CMakeLists.txt:226-228](../../../../CMakeLists.txt)、[tests/media_import/CMakeLists.txt:10-15](../../../../tests/media_import/CMakeLists.txt)、[CMakePresets.json:56-61](../../../../CMakePresets.json) | D-8 成立：media-tools 配置下 `cuexis_media_import_tests` 激活但无 allowlist 登记（全仓零命中；对照 954-961 的 `cuexis_asset_publish_tests`）。按 [RULING_WORKSHEET:51、96](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) 并入 S7A-1 allowlist 批次 |
| F-07 | 未裁定 | [RULING_WORKSHEET:180](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)、[plan.md:878](../../../stage_plans/active/stage-07/plan.md) | D-11 稳定 C ABI 阶段归属不一致（Stage 12 vs Stage 14）仍为第 7 轮登记项；本次只核实 plan:878 写 Stage 14 与该登记行的存在，未核实 AGENTS.md 侧行号 |
| F-08 | 未裁定 | [plan.md:220-227](../../../stage_plans/active/stage-07/plan.md) vs [ABI:57-279](../../../api/GAMEPLAY_V2_ABI.md) | S7A-1 的冻结范围未界定：plan 列 17 项，ABI 类型清单 9 域（ABI 类型清单 9 域、126 条目；接受记录原称 144，已订正为 126）。是"只冻结这 17 项"还是"冻结 ABI 清单"，直接决定 S7A-1 的验收口径；RULING_WORKSHEET 的 7 轮均未登记该问题 |
| F-09 | 未裁定 | [ABI:82](../../../api/GAMEPLAY_V2_ABI.md)、[RULING_WORKSHEET:159](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)、Spec:586 | `InputDomain` 枚举集待冻结，其裁定项 P2-05 在第 6 轮（进入 S7A-7 前），而 plan:220-227 要求 S7A-1 冻结 `InputDomain`。需 owner 明确 S7A-1 是否只冻结类型存在与角色 |
| F-10 | 未裁定 | [ABI:61、575](../../../api/GAMEPLAY_V2_ABI.md)、[RULING_WORKSHEET:104-105](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)、[CONTRACT_MATRIX:89-90](../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md) | `Tick` 宽度与 `ChartTick`↔`ObservationTick` 转换边界/ tie rule 属第 2 轮（S7A-2 前），而 plan:220-227 要求 S7A-1 冻结 `Tick` / 时间区间。同 F-09 的时点冲突 |
| F-11 | 未裁定 | [plan.md:84-85](../../../stage_plans/active/stage-07/plan.md)、[ADR 0044:3、158、163](../../../adr/0044-gameplay-v2-semantic-kernel.md)、[RULING_WORKSHEET:259](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)、[接受记录:88-94](2026-10-02-s7a-0-acceptance.md)、Spec:3-8、578 | 冻结顺序第 2 步的"冻结"缺完成判据：三份 V2 文档仍是 `candidate` 且自我声明"冻结前只作为冻结目标、不作为实施授权"（ADR 0044:3；ABI:41-44），但第 3 步（标注 Gameplay I 为 `superseded`）已执行（接受记录:93），而 ADR 0044:158 把"标注必须晚于冻结"写成门禁；接受记录:94 又记"前置 2 与 3 已满足"。两种读法给出相反的 S7A-1 授权结论，需 owner 裁定 |
| F-12 | 未裁定 | [TARGETS:182-189](../../../proposals/gameplay-v2-acceptance/TARGETS_AND_DEPENDENCIES.md)、[plan.md:253-254](../../../stage_plans/active/stage-07/plan.md) | 模块位置（方案 A/B）、是否拆 `cuexis_input`、判定类型是否进安装公共头（含是否触发 SDK API `0.7.1`）、是否进 `CUEXIS_STATIC_IMPLEMENTATION_TARGETS`、工具目录命名五项仍是待 owner 决策，未登记到任何裁定轮次 |
| F-13 | 无问题 | [ABI:543-550](../../../api/GAMEPLAY_V2_ABI.md)、[ABI:552-562](../../../api/GAMEPLAY_V2_ABI.md) | ABI §与 Gameplay I ABI 的关系中，§1-§6 处置行与字段级映射行（除 F-04 缺口）逐条可核实；`GripPolicy`、`JudgementFact` 四字段、`eventSequence`、四分量 identity 的替代方向明确 |
| F-14 | 无问题 | [CONTRACT_MATRIX:239-252](../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md)、[RULING_WORKSHEET:33、88-92、259](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) | S7A-1 前必须裁定的 5 项 `open`（CM-V06/V07/V13/I06/X06）已由第 1 轮全部裁定，语义前置无遗留；第 2-7 轮登记仍为"待填"（RULING_WORKSHEET:260-265） |
| F-15 | 无问题 | [VerifyArchitecture.cmake:245-273、288-324](../../../../cmake/VerifyArchitecture.cmake) | 两道门禁本身有效：target 集合相等校验与直接依赖子集校验均会 `FATAL_ERROR`；D-8 属"某 target 缺登记"，不是门禁失效 |

**区分说明。** F-01 至 F-06 是"计划文本/ABI 文本/文档完整性与 V2 文档不一致"的**缺陷**（可由文档
编辑关闭，不需新语义），F-07 至 F-12 是"第 2-7 轮尚未裁定或根本没有归属轮次"的**未裁定**语义或
范围问题。

## 7. 门禁结论

**结论：S7A-1 不能无条件开始；条件性通过。**

**可以确认的部分。** S7A-1 前必须裁定的 5 项 `open` 合同项（CM-V06、CM-V07、CM-V13、CM-I06、
CM-X06，[CONTRACT_MATRIX:246](../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md)）
已由第 1 轮全部裁定（[RULING_WORKSHEET:33、88-92](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)），
第 1 轮的批次结论是"语义前置已满足"（同文件 259 行）。冻结顺序第 3 步已完成
（[接受记录:93](2026-10-02-s7a-0-acceptance.md)）。本报告即第 4 步的产出。

**必须先裁定的最小集合（每条给出归属）。**

| # | 必须先裁定的事项 | 归属 | 依据 |
| --- | --- | --- | --- |
| 1 | 冻结顺序第 2 步"冻结"的完成判据与可核查记录（三份 V2 文档当前均为 `candidate`，且自我声明冻结前不作为实施授权；而第 3 步已执行） | **无 RULING_WORKSHEET 轮次**；属 plan §1.1 第 2 步与 acceptance package §2.1 的收口动作（owner 决策） | plan:84-85；ADR 0044:3、158、163；ABI:41-44；Spec:3-8、578；接受记录:88-94 |
| 2 | S7A-1 的冻结范围界定：plan:220-227 的 17 项，还是 ABI 类型清单（9 域） | **无轮次**；需 owner 在 S7A-1 启动前确认 | F-08；ABI:57-279；接受记录:90 |
| 3 | 模块位置（方案 A/B）与 target 方向五项决策 | **无轮次**；plan:253-254 的直接输入 | TARGETS:182-189 |
| 4 | `InputDomain` 枚举集（第 6 轮 P2-05）与 `Tick` 宽度/转换边界（第 2 轮 CM-T03/CM-T04）同 plan:220-227 冻结要求的时点冲突 | 第 6 轮 P2-05 / 第 2 轮 CM-T03、CM-T04；若第 2 项裁定"S7A-1 只冻结类型存在与角色"，本项随之关闭 | RULING_WORKSHEET:104-105、159；ABI:82、575 |

**已核实不构成 S7A-1 前置的项。** plan:228 的命名/整数宽度/所有权/异常边界/序列化（与实现批次
和 ABI 一起冻结）；F-01 至 F-06 的文本缺陷（可由文档编辑关闭）；D-8（已并入 S7A-1 的 allowlist
批次，[RULING_WORKSHEET:96](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)）；
第 2-7 轮的其余未决项（[RULING_WORKSHEET:260-265](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) 仍待填）。

**本报告不下判的部分。** identity projection 表的表体（CM-I06）、`candidateRevision` 的
artifact/interchange 兼容矩阵（Q-01 Codex 修订）、诊断码表（CM-D04）、Snapshot/Replay 字段集
（CM-K07 / CM-K01）等仍按 ABI:571-586 与各自轮次推进；本报告不代替它们的裁定。

## 8. 后续关闭证据

| 项 | 关闭所需证据 |
| --- | --- |
| F-01、F-02（plan 冻结清单为 Gameplay I 词汇） | plan §S7A-1 的"必须冻结的对象"改为 V2 类型或显式指向 ABI 域；改动后 `python -B tools/check_docs.py` 与 `git diff --check` 通过 |
| F-03（五类 vs 四类） | plan:228 与 ABI 未决项表的类别集合一致；若保留"所有权/异常边界"，须指明它们已在 ABI 写为承诺而非未决项 |
| F-04（`InputEvent.source` 无映射） | ABI 字段级映射表补该行，或 Spec/ABI 明确 `SourceClass` 与 `SourceBuildIdentity` 的分工 |
| F-05（Spec 运行语义缺章） | Spec 建立时间域/Tick 阶段/仲裁/Fact Ledger/Ruleset 事务的对应章节，或 §1.1 第 2 项的归属声明改为指向实际章节 |
| F-06（D-8） | 根 `CMakeLists.txt` 增加 `cuexis_verify_target_dependencies(cuexis_media_import_tests ...)`；`debug-media-tools` 下 configure 通过且 allowlist 校验执行 |
| F-07、F-09 至 F-12 | 对应轮次在 [RULING_WORKSHEET §9](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) 登记裁定结果；无轮次的三项（冻结判据、冻结范围、模块/target 决策）需 owner 决策记录或新登记行 |
| §4.2 的 allowlist 与安装落地 | 新 target 的登记位置、安装列表归属、`BUILDING.md` target 块与 `generated/cuexis-targets.txt` 逐项相等 |
| §5 的头文件约束 | 头文件泄漏扫描、依赖 allowlist、架构脚本、静态/shared 安装 consumer、异常与 ownership 编译检查、无 GPU 空会话/prepare 失败/销毁路径的结果（计划 263-264） |
| 第 4 步本身 | 本报告被索引（`docs/stage_reports/README.md`）并从主索引可达；`python -B tools/check_docs.py` 通过 |

## 9. 关闭复核（2026-10-02 追记）

按 owner 于 2026-10-02 接受的 Codex 修订（第 1 轮补充 **S1-02**，thread `s7a1-admission-rulings`，
verdict `reject`，confidence 0.94）要求，为本报告补一次关闭复核。**注意**：本报告产出后，§6 的
F-01 / F-02 / F-03 / F-05 与条目计数已被修复，因此**报告中的部分行号与描述已落后于现行文本**；
下表以**现行文本**为准重核（这是 Codex 风险条 ③ 的直接应对）。本报告对 ABI / Spec / 计划等文档的
**行号已于 2026-10-02 逐条按现行文件重新核实并更新**：更新方式一律是**先打开目标文件确认该行内容确为
被引用对象、再改数字**，不做机械偏移；加偏移后内容不符的引用按内容重新定位，仍无法定位的保留原值并
在回报中单列。本报告**不再新增 `#L` 行锚**，跨文件引用只保留既有的 `文件:行号` 纯文本与带相对路径的
Markdown 链接两种写法（链接目标一律不变，只有标签内的行号随本次核实更新）。

| 编号 | 现状 | 复核依据（现行文本） |
| --- | --- | --- |
| F-01 | **已修复** | plan §S7A-1 的冻结对象清单已改为以 V2 ABI 类型清单为权威来源，并逐项给出 V2 候选名 |
| F-02 | **已修复** | 同处已用 `PreparedGrace` / `GraceResolutionPolicy` / `ResourceClaimIntent` 取代 `GripPolicy` |
| F-03 | **已修复** | ABI"仍待冻结"表已补"所有权与生命周期、异常边界"一行，并注明 [plan.md §S7A-1](../../../stage_plans/active/stage-07/plan.md) 列五类 |
| F-04 | **未闭合** | ABI 字段级映射仍缺 `InputEvent.source` 的去向；需要一个归属判断，留待第 2 轮输入域合同 |
| F-05 | **已修复** | Spec §1.1 第 2 项已标明七项运行语义尚未写成章节并按轮次归属；§11 第 4 轮行改为指向该项 |
| F-06（=D-8） | **登记为批次内项** | 按 S1-04 并入 S7A-1 allowlist 实施与退出验收，不提前落地 |
| F-07…F-12 | **转入裁定登记** | 无轮次归属的 4 项已由第 1 轮补充 S1-01…S1-05 覆盖；D-11 归第 7 轮 |
| 条目计数 | **已订正** | 9 域相加为 **126** 条（原"144"为口径误记），接受记录与本报告均已订正 |

**关闭结论。** 本报告 §7 的"条件性通过"仍然成立，且条件已具体化为 S1-01…S1-05 五条；其中
F-04 是唯一仍开放的**文本缺陷**，不阻塞 S7A-1 的角色级冻结，但在第 2 轮输入域合同闭合前不得据
ABI 映射表声称 `InputEvent.source` 已有归属。

**正文的时点性质（重要）。** §2 / §3 / §6 保留的是**审查当时**的事实陈述，不随修复回改；其中下列条目的
**前提已因修复而变化**，引用时以本表与现行文本为准：F-01 / F-02（现行 plan §S7A-1 已改为以 V2 ABI 为
权威来源并写明取代 `GripPolicy`）、F-03（所有权与异常边界已补入"仍待冻结"表）、F-05（Spec §1.1 第 2 项
已标注尚未写出并按轮次归属；"没有运行语义正文章节"这一事实本身仍成立）、F-11（ADR 0044 门禁 1 已含
偏差记录、S1-02 已登记，"两种相反读法"的前提已由 S1-01 消除）、F-12 与 §4.1（TARGETS §9 五项已由
S1-04 裁定）。**F-04 的前提没有变化**，仍是开放缺陷。

## 10. 相关索引

- [Stage 7A 实施计划](../../../stage_plans/active/stage-07/plan.md)：冻结顺序、S7A-1 范围与验证
- [S7A-0 owner 接受记录](2026-10-02-s7a-0-acceptance.md)：本步的前置与执行时点表
- [S7A-0 执行基线与合同表征](2026-10-02-s7a-0-baseline.md)：同批次基线证据
- [ADR 0044：Gameplay V2 语义内核与冻结边界](../../../adr/0044-gameplay-v2-semantic-kernel.md)
- [Gameplay V2 字段与运行语义规范](../../../formats/GAMEPLAY_V2_SPEC.md)
- [Gameplay V2 ABI（候选）](../../../api/GAMEPLAY_V2_ABI.md)
- [Gameplay V2 acceptance package](../../../proposals/gameplay-v2-acceptance/README.md)
- [未决语义分轮裁决清单](../../../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)：轮次编号与裁定登记
- [S7A-0.2 合同逐项台账](../../../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md)：114 项合同与 `open` 清单
- [目标、依赖与安装图](../../../proposals/gameplay-v2-acceptance/TARGETS_AND_DEPENDENCIES.md)：模块/target 候选与五项待决策
- [格式、入口与 identity 矩阵](../../../proposals/gameplay-v2-acceptance/FORMAT_ENTRY_AND_IDENTITY.md)：identity 与 Packed 承载
- [支持 / 拒绝矩阵与 capability registry 草案](../../../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md)
- [Gameplay Judgement ABI（`superseded` / `historical`）](../../../api/GAMEPLAY_JUDGEMENT_ABI.md)：本次映射的历史对照
- [内部模块速查](../../../api/internal-module-catalog.md)：模块归属边界
- [Agent 指南](../../../../AGENTS.md)：C++ 约定、架构约束与 target allowlist
- [文档整理政策](../../../DOCUMENTATION_POLICY.md)：状态词枚举与报告必填字段
- [阶段报告索引](../../README.md)
