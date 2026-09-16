# Chart Format Foundation：Chart v5 前置基础

状态：completed；项目所有者确认完成，交接 Stage 6

更新日期：2026-09-16

归档来源：[Chart v5 格式计划](../../active/chart-format-update-for-v5/plan.md)、
[Stage 5 计划](../../completed/stage-05/plan.md)、[CXT v1 格式合同](../../../formats/CXT_FORMAT.md)、
[CXC v1 格式合同](../../../formats/CXC_FORMAT.md)。

2026-09-16 按项目所有者确认归档；合并记录与证据边界见
[关闭与交接记录](../../../stage_reports/stages/chart-format-foundation/2026-09-16-closure-and-handoff.md)。
下文保留阶段执行范围与门禁，不代表仍需从 F0 启动。

本计划的字段与 wire 细节以候选
[CXT v2](../../../formats/CXT_V2_FORMAT.md) 和
[Packed Chart](../../../formats/PACKED_CHART_FORMAT.md) 为准；本计划只决定实施范围和
门禁，不另行定义字段。

## 1. 阶段目标

在 Stage 6 之前解决当前 Chart 物理存储无法满足高密度谱面的风险，但不提前冻结
尚未成熟的完整 Judgement 语义。

本阶段交付：

```text
CXT v2 Core 的候选合同
Prototype slots / Instance bindings / Parameter freeze / ValueSource
Pattern / finite Repeat
有限确定性展开
Packed Chart 物理编码原型
CXC Packed entry 设计
声明容量 profile 下的 40,000 语义实体容量测试
16 MiB Packed Chart 门禁
JSON -> semantic model -> Packed 的等价证据
```

本阶段完成后，Chart v5 Foundation 可以被工具和 headless validator 使用，但不表示
Chart v5 已成为默认 Playback 格式，也不表示完整 v5 Judgement 已实现。

## 2. 与 Stage 6 的边界

Stage 6 采用双基线：v5 Core/Packed candidate 是主要开发和验证基线，以下内容是兼容
与回退基线：

```text
Chart v4
CXT v1
CXC v1
SDK 0.7.0
```

Foundation 必须为 Stage 6 提供可验证的 candidate artifact、容量报告和 Packed inspection
能力，但不切换默认 Writer、删除 v4 Reader 或改变 v4 的 FrameDigest。Stage 6 只加载
Foundation 已冻结并明确标记的 v5 subset；未覆盖的 v5 语义必须稳定拒绝。

## 3. 工作批次

### 3.0 执行规则与依赖

Foundation 必须按“合同先行、原型隔离、验证先于接线”的顺序实施：

```text
F0 基线与合同冻结
  -> F1 Canonical Semantic Chart typed model
  -> F2 CXT v2 Core validator/expander
  -> F3 Packed codec primitives
  -> F4 Packed tables/streams
  -> F5 Writer sizing and atomic output
  -> F6 Reader and prepare bridge
  -> F7 CXC candidate entry
  -> F8 capacity/security/parity
  -> F9 owner acceptance and Stage 6 handoff
```

F1 与 F3 可以并行；F2 依赖 F1；F4 依赖 F1/F3；F6 依赖 F4/F5；F7 依赖 F6；
F8 依赖所有前置实现；F9 只能在 F8 的原始数据和报告完成后开始。

所有任务都必须遵守：

```text
candidate 代码使用独立入口和显式 capability，不改 v4 默认路径
JSON/CXT/CXC/Packed 解析停留在 Chart/CXC prepare 边界
engine/animation/ 不增加任何 source-format parser 依赖
expand/decode/write 失败均不得发布部分 semantic model 或 Runtime
count、offset、byte length、Beat 运算全部使用 checked arithmetic
每项新能力必须同时有正例、反例、deterministic golden 和 handoff 说明
```

本阶段最初启动任务是 `F0`：完成基线、术语、candidate revision/profile、v4 回归
characterization 和 fixture 清单。`F0` 未关闭前，不开始 v5 默认路径、Packed
Playback 接入或 CXT v2 formal-release 接线。

### CFF-A：CXT v2 Core

冻结候选模型：

```text
Prototype slots
Instance bindings
Parameter freeze
ValueSource
Pattern
Finite Repeat
Finite Expansion
```

候选合同列出的参数类型为：

```text
integer
number
rational
beat
boolean
enum
vector
```

Foundation F2 冻结并实现的类型子集只有 `integer` 与 `beat`。`number`、`rational`、
`boolean`、`enum`、`vector` 在 F2 schema/reader 中必须稳定拒绝，不得通过通用 JSON 或
opaque extension 绕过；这些类型需要后续 candidate revision 单独冻结 typed model、
值域、算术、golden 和 hosted 验证后才能开放。

参数不得改变 Component 集合、对象类型、父子关系、AssetId、引用目标或资源闭包。
Core 首批开放 Beat、lane、position/scale 和受限 Repeat count；不提供 reference 参数。
固定 AST/Component 结构不变，Repeat 展开数量单独受预算约束。rotation、alpha 和
Animation Track 值保持 literal。ValueSource 只允许 Spec 定义的有限仿射组合，
CXT 不执行脚本、任意表达式、随机、上一帧状态、Judgement 内部状态或宿主回调。
参数在编译时冻结；更改参数需要新的 Packed artifact，发行 Playback 不隐式重跑
CXT。v4 host ParameterSet 兼容路径保持原样。

### CFF-B：Packed Chart 原型

建立与 v5 语义等价的物理编码原型：

```text
96-byte candidate Header
32-byte Section Directory Entry
STR0 string table
REF0 typed reference table
IDN0 concrete semantic identity table
ARCH Packed Archetype defaults
ENT0 concrete entity index
typed component streams and sparse field masks
Beat GridDelta / Rational atoms
RationalBeat 无损还原
header/section CRC32
```

IDN0 是运行语义必需表，不能移入可删除的 DBG0。ARCH 只去重已经展开的具体
Component 默认值；它不是 CXT Prototype，ENT0 也不是 CXT Instance。CXT 必须先
展开为 Canonical Semantic Chart，Packed Reader 不执行 CXT AST。

本阶段只实现 Foundation candidate profile：静态 Tap、point requirement、
候选 `candidate.lanes4` domain、`press` action 和 `lane` constraint。REQ0 的
half-open range 只作为 wire 形状保留，不表示 Foundation 已实现 Hold/Release。
BEH0/BHD0/ANM0/FXS0 在没有独立 candidate revision、字段枚举、capability 和
golden 前稳定拒绝，不能退化为 opaque JSON。

### CFF-C：CXC entry 设计

冻结 CXC v1 内部的 Chart v5 发行映射：

```text
source entry (optional)
compiled entry
playback entry
source semantic identity
compiled semantic identity
Packed artifact identity
compiler profile
```

CXC 容器版本保持 v1。Foundation 不要求 Stage 6 立即发行 v5，但必须确保 Stage 8
不需要重新设计 CXC 容器边界。

分别定义 source/build provenance、expanded semantic identity 和 artifact identity；
Header 语义 hash 不直接沿用 v4 source JSON hash。source/build 的分帧算法须另附
golden；Stage 7A/8 再冻结结合资源内容与 Input/Judgement profile 的 prepared/replay
identity，不修改现有 v4 组合算法。

Foundation CXC candidate 要求 Packed compiled/playback entry 必须存在并可独立验证，
source entry 可以保留但不是 v5 Playback 入口。`cxc_pack` 与 `cxc_validate` 只封装或
验证已生成的 Packed artifact，不执行 CXT 读取、参数冻结或展开；需要展开时必须由
显式 compile/prepare 步骤先产生 Canonical Semantic Chart 和 Packed bytes。

### CFF-D：容量和安全门禁

建立并冻结绑定门禁：

```text
low-reuse profile with 40,000 semantic entities and independent identities
binding profile: Packed Chart <= 16 * 1024 * 1024 bytes
high-reuse / bounded-mixed shapes as non-binding design references
expanded decoded bytes budget
expanded event count budget
peak prepare memory budget
IDN0 size and resource-closure accounting
```

Foundation 以低复用显式实体 profile 作为绑定容量上界，覆盖身份/资源引用密集、递归、
整数与 Beat 溢出、超展开数量和超 Packed bytes。高复用 Pattern 与受控混合 shape
保留为设计参考，不作为独立关闭门槛；其编码占用预期不高于低复用 profile，但不据此
承诺任意复杂谱面。Stage 8 再加入动画/完整判定/效果 profile。

## 3.1 任务看板与依赖

| ID | 任务 | 前置 | 主要产物 | 完成定义 |
| --- | --- | --- | --- | --- |
| F0 | 基线与合同冻结 | 无 | Spec/ADR 对照表、fixture 清单、candidate profile | v4 回归基线、revision、术语和支持边界无冲突 |
| F1 | Canonical semantic model | F0 | typed entity/Requirement/identity model | 能表达 entity、parent、Requirement、资源闭包 |
| F2 | CXT v2 Core | F1 | v2 schema、reader、finite expander | integer/beat 子集、16 entity golden、40000 preflight、负例稳定 |
| F3 | Packed codec primitive | F0 | byte/varint/CRC/Beat codec | golden、截断/溢出/非最短编码拒绝 |
| F4 | Packed tables/streams | F1,F3 | STR0/REF0/IDN0/ARCH/ENT0/TRN0/REN0/CNS0/REQ0 | empty/tap/stair round-trip |
| F5 | Writer sizing | F2,F4 | sizing report、atomic writer | 超 16 MiB 不产生有效部分文件 |
| F6 | Reader bridge | F4,F5 | candidate decode/semantic validation | 仅接受登记 subset，未知内容拒绝 |
| F7 | CXC candidate entry | F5,F6 | pack/validate manifest mapping | semantic/artifact/count/profile 全匹配 |
| F8 | Capacity/security/parity | F2,F6,F7 | 40k low-reuse report、拒绝矩阵、回滚证据 | low-reuse profile 和 v4 parity 通过；高复用/混合仅作参考 |
| F9 | 交接与关闭 | F8 | dated completion report、Stage 6 handoff | owner acceptance、状态和索引同步 |

## 3.2 CFF-A：CXT v2 Core 细化任务

```text
CFF-A1 读取 moduleKind、exports、严格未知字段和 ID 唯一性
CFF-A2 建立 Parameter、Slot、Binding、ValueSource typed model
CFF-A3 验证 affine 类型/单位、Beat 约分、范围和 checked arithmetic
CFF-A4 验证 Prototype Component leaf/default/Requirement schema
CFF-A5 验证 Pattern/Repeat scope、count、node path 和 parent graph
CFF-A6 事务性展开为 Canonical Semantic Chart
CFF-A7 生成 entity/requirement identity，并完成顺序无关 golden
```

候选代码落点：

```text
engine/chart/include/cuexis/chart/cxt_v2_*.hpp
engine/chart/src/cxt_v2_*.cpp
tests/chart/cxt_v2_*_tests.cpp
schemas/cuexis.animation-template.v2.schema.json
```

若复用现有 `animation_template_*` 文件，不得让 v1 reader 通过“version >= 2”自动
兼容；必须保留显式版本路由。完成定义是：完整阶梯展开为 16 entities/16 requirements，
`groups=10000` 可在分配前预测 40000，lane 越界、缺 slot、动态 reference、递归和
超预算稳定失败，展开结果不含 AST/Parameter/Slot/Index。

## 3.3 CFF-B：Packed Chart 细化任务

```text
CFF-B1 byte reader/writer、LEB128/ZigZag、CRC32、bounds helper
CFF-B2 Header、Directory、section registry、candidate revision gate
CFF-B3 STR0、REF0、IDN0：排序、UTF-8、typed reference、identity tuple
CFF-B4 ARCH、ENT0、parent graph 和 component mask
CFF-B5 TRN0、REN0、CNS0、REQ0 的 Foundation subset
CFF-B6 sizing pass、per-section accounting、atomic output
CFF-B7 decode -> Canonical Semantic Chart parity bridge
```

候选代码落点：

```text
engine/chart/include/cuexis/chart/packed_chart_*.hpp
engine/chart/src/packed_chart_*.cpp
tests/chart/packed_chart_*_tests.cpp
```

完成定义是：Header 96 bytes、Directory entry 32 bytes；`empty`、`one-tap-lane2`、
阶梯 Packed golden 可读写；IDN0 每实体一条身份记录；ARCH 默认值和 sparse mask
可恢复完整 Component；REQ0 只接受登记的 tap/point/lane/press profile；未知
section/kind/revision 稳定拒绝。

## 3.4 CFF-C：CXC candidate entry 细化任务

```text
CFF-C1 扩展内部 typed manifest，区分 source/compiled/playback
CFF-C2 校验 encoding=packed-chart、candidate revision 和 semantic identity
CFF-C3 校验 exact entry artifact identity、expanded counts 和 compiler profile
CFF-C4 在 cxc_pack/cxc_validate 接入显式 candidate-v5 路径
CFF-C5 验证 source entry 可保留但不能作为 v5 Playback 入口
```

CFF-C 不升级 CXC 容器版本，不改变现有 v4 entry 三字段基础合同，不执行 CXT。
source/build provenance 没有独立排序算法和 golden 前，只能按分项字段记录，不得
宣称已经冻结跨工具 digest。

## 3.5 CFF-D：容量、安全和回滚细化任务

```text
CFF-D1 拆分 source/expansion/decoded/Packed/prepare 五类预算
CFF-D2 冻结低复用 40000 entity fixture；高复用/受控混合仅保留可选参考 shape
CFF-D3 建立截断、溢出、重叠、未知 section、identity mismatch fixture
CFF-D4 输出 section bytes、IDN0、default hit、field contributor report
CFF-D5 在 Debug/Release/headless 下重复容量和拒绝测试
CFF-D6 记录编译耗时、峰值内存、临时文件清理和失败回滚
```

建议新增独立 `PackedChartLimits`，而不是放宽现有 v4 `ChartLimits`。至少覆盖：

```text
maxCxtModuleBytes
maxCxtExpansionEntities
maxCxtExpansionRequirements
maxPackedFileBytes
maxPackedDecodedBytes
maxPackedSectionBytes
maxPackedStrings
maxPackedReferences
maxPackedEvents
maxPreparePeakBytes
```

低复用 profile 冻结为 40,000 个独立实体和独立 identity，不依赖 Pattern/Archetype
重复来满足实体数。该 profile 必须达到 40k/16 MiB；同时记录 source bytes、expanded
counts、Packed bytes、decoded bytes、IDN0 bytes、资源闭包、prepare peak 和耗时。高复用
与受控混合只作为后续优化或诊断输入，不构成 Foundation 关闭条件。
decoded bytes、section bytes、prepare peak 和编译耗时等额外预算在当前 Foundation 计划
中暂不冻结，先作为观测指标记录；不得据此放宽已冻结的输入、实体数或 Packed 文件上限。
不能把高复用 fixture 结果解释为任意复杂谱面的保证。

## 3.6 代码开始门槛

| 门槛 | 允许开始 | 禁止 |
| --- | --- | --- |
| F0 前 | 文档、盘点、fixture、characterization | v5 接入默认 Reader/Writer |
| F0 后 | typed model、独立 codec | 默认 v5、删除 v4 Reader |
| F1/F3 后 | CXT expansion、Packed tables | CXC playback、opaque JSON fallback |
| F4/F5 后 | candidate reader bridge | 未登记 RequirementKind、Hold/Release |
| F8 + owner acceptance 后 | Stage 6 candidate handoff | 声称 v5 formal release、切默认 Writer |

## 3.7 统一任务交付模板

每个任务的 PR 或本地变更说明必须包含：

```text
Scope / Contract / Implementation
Positive evidence / Negative evidence
Compatibility and rollback evidence
Changed limits/diagnostics/capability
Known gaps and next-task handoff
```

没有失败证据、兼容证据或明确 handoff 的任务不算完成。

## 4. 明确不包含

- Chart v5 默认 Writer。
- Chart v5 正式 Playback capability。
- 完整 Input/Judgement。
- Slide、Flick、多指和高级判定要求。
- CXT v2 Animation Extension 的最终完整合同。
- Studio 正式支持。
- 运行时脚本。

## 5. 验收标准

- CXT v2 Core 的候选 Spec、正反例和展开顺序完成。
- 输入顺序、参数顺序和引用顺序不影响 semantic identity。
- Packed 原型能够无损还原合法 semantic subset。
- 低复用 profile 在 40,000 个展开语义实体下满足 Packed artifact <=16 MiB；高复用和
  受控混合不作为独立关闭条件。超预算输入有稳定拒绝和分项容量报告。
- decoded bytes、展开数量、峰值内存和编译时间均有独立预算。
- 递归、随机、任意表达式、checked arithmetic 溢出和超预算输入稳定失败。
- CXC v1 的 source/compiled/playback 分层可以被工具验证。
- Stage 6 的默认发行和兼容回退仍可保持 Chart v4/CXT v1/CXC v1；其主要开发路径切换为
  Foundation 已验证的 v5 Core/Packed candidate。
- owner acceptance 完成后，Stage 6 才能进入实施。

### 5.1 验收矩阵

| 维度 | 必须证明 | 不通过时 |
| --- | --- | --- |
| 语义 | JSON/CXT/Packed 的 entity、parent、Requirement、Beat、引用和 identity 等价 | 不进入 CXC candidate |
| 物理 | Header、目录、section、CRC、varint 和 Beat 可跨平台还原 | 拒绝 Packed candidate |
| 安全 | count、offset、UTF-8、CRC、Beat 和预算检查先于分配 | 修复 Reader，不放宽上限 |
| 容量 | 低复用声明 profile 达到 40k/16 MiB，并有分项报告 | 报告绑定 profile 失败，不降低实体数 |
| 兼容 | v4/CXT v1/CXC v1、FrameDigest 和默认路径回归不变 | 不切换默认路径 |
| 事务 | expand/decode/write 失败无部分发布，旧 active 状态保留 | 不允许 Stage 6 消费 |
| CXC | source/compiled/playback、semantic/artifact/count/profile 一致 | package validation 失败 |
| 范围 | 未支持 section、RequirementKind、profile 和 capability 稳定拒绝 | 禁止 opaque fallback |

### 5.2 关闭报告

关闭时必须创建一份带日期的 Stage Report，至少记录：

```text
candidate revision、commit SHA、编译器和平台
支持的 section、RequirementKind、gameplay profile 和 capability
每个 fixture 的 source bytes、expanded counts、Packed bytes、decoded bytes、peak bytes
各 section size、IDN0/default hit、字符串/引用数量和 top field contributors
semantic/artifact identity、round-trip 和 CXC entry 校验结果
失败矩阵、稳定 diagnostics、v4 regression 和 headless/release 验证
已知未实现项，以及 Stage 6 允许/禁止消费的内容
```

没有原始容量数据、失败证据和 handoff 清单，不能将计划从 `active` 移入完成目录。

Foundation 关闭必须等待同一 candidate revision 在 hosted MSVC、MinGW、Linux 三平台
完成构建、测试和拒绝矩阵验证；单一本地工具链通过不能替代跨平台证据。

## 6. 交接

交给 Stage 6：

```text
Packed format candidate
CXT v2 Core candidate
CXC entry mapping
capacity fixtures
security budget
semantic identity rules
candidate Header flags/revision and supported profile
IDN0/ARCH/ENT0/component stream contracts
```

Stage 6 handoff 必须明确：

```text
允许：flags=1、candidateRevision=1、Foundation 登记 profile、静态 Tap/point/lane、
      STR0/REF0/IDN0/ARCH/ENT0/TRN0/REN0/CNS0/REQ0
禁止：正式 v5 default Writer、正式 CXC playback release、Hold/Release、
      Slide/Flick、多指、未定义 BEH0/BHD0/ANM0/FXS0、CXT runtime expansion
```

交给 Stage 7：

```text
Chart v5 可引用的 JudgementRequirement 边界
Pattern 生成结果的 requirement identity 规则
Replay identity 需要包含的 semantic identity
```

交给 Stage 8：

```text
Chart v5 Foundation artifact
CXT v2 Core contract
Packed Chart section contract
40k / 16 MiB test harness
```
