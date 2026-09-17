# Cuexis Packed Chart v1

状态：candidate；Chart v5 Foundation 的物理存储提案；未实现、未冻结生产 wire format

更新日期：2026-09-05

依据：[玩法抽象模型](../architecture/GAMEPLAY_ABSTRACTION_MODEL.md)、
[CXT v2](CXT_V2_FORMAT.md)、
[Chart Format Foundation](../stage_plans/completed/chart-format-foundation/plan.md)、
[Chart v5 总工作包](../stage_plans/active/chart-format-update-for-v5/plan.md) 和
[CXC v1](CXC_FORMAT.md)。

## 1. 分层与设计选择

```text
Authoring
  Chart v5 JSON + CXT v2 + literal invocation parameters
    -> typed validation / finite expansion
Canonical Semantic Chart
  concrete entities, requirements, timing, references and schedules
    -> dictionary / archetype / typed stream encoding
Packed Chart
  portable binary file
    -> bounded decode / semantic validation
Playback prepare
  ChartRuntime / AnimationProgram / World
```

本提案选择**有目录的二进制表与稀疏字段流**，不选择短键 JSON、压缩 JSON、
运行时模板文件或内存结构直接 dump。理由是既要减少重复，又要在大规模内存分配前
验证结构、计数、引用和预算；压缩算法本身不能提供这些合同。

作者层仍使用可编辑 JSON/CXT。Packed 不保存 CXT AST，也不执行 Prototype、Pattern、
参数表达式或脚本。物理解码恢复的是已经具体化的值，不是再运行一次 CXT。

同一 source 使用另一组参数时必须重新展开并编译另一个 Packed artifact；不能在
加载已冻结的 v5 Packed 时通过 host ParameterSet 隐式重跑 CXT。若入口收到企图改变
这些冻结值的参数，必须明确拒绝或要求显式重编译，不能静默忽略。v4 的既有参数
prepare 路径不受此候选合同改写。

两个容易混淆的概念必须区分：

| 作者层 | 物理层 |
| --- | --- |
| CXT `Prototype`：槽位与字段生成规则 | Packed `Archetype`：具体 Component 集合与共享默认值 |
| CXT `Instance`：一次绑定调用 | Packed `EntityRecord`：一个已存在的具体实体 |

ADR 0014 对作者格式 Archetype 表的拒绝仍成立；这里不把表格编码倒灌回作者格式。
`packedVersion` 是 wire format 版本，不是 Chart v6，也不是 Runtime cache 版本。

## 2. 规范语义中间层

Canonical Semantic Chart 是内部逻辑合同，不是新增的 JSON 顶层分类数组：

```text
CanonicalSemanticChart {
  semanticVersion: 5
  chartId: UUID
  timing: { offsetMs, defaultBpm, tempoEvents[], stops[] }
  defaultCamera: resolved CameraData
  mainMusic: AssetId | none
  requiredFeatures: sorted (id, version)[]
  resourceRequirements: sorted typed references[]
  entities: sorted ConcreteEntity[]
  behaviorDefinitions: sorted typed definitions[]
  animationDefinitions: sorted typed clips and bindings[]
  effectDefinitions: sorted finite typed schedules[]
}

ConcreteEntity {
  identity: ExplicitIdentity | GeneratedIdentity
  parent: EntityIdentity | none
  components: fixed registered component map
  requirements: sorted (localId, kind, interval, domain, action, constraints, effects)[]
}
```

所有 CXT 槽位、参数、局部 Beat 偏移和父引用必须已冻结；所有 Component 默认值必须
已按语义 Schema 补齐。Requirement 列表只是单实体的逻辑数据，不新增作者层 `notes`、
`elements` 或 `decorations` 平行对象容器。

身份、Component presence、Beat、资源引用和未绑定但仍要求保留的语义定义不能在
Packed 中丢失。资源需求应包含规范要求的整个声明闭包，包括零权重/未使用动画中的
资源；不得仅按当前画面使用情况裁剪。metadata、编辑器 name、源路径和诊断位置属于
inspection，可放 DBG0 或 Source Project，不属于本中间层的求值语义。

静态 Transform 沿用当前 typed `TransformData` 的 binary32 Vec3/Quat；Camera、
Timing 和动画标量沿用各自的 binary64 合同。JSON/CXT 到 typed model 的数值转换
发生在语义边界，Packed Writer 不再舍入、不量化、不重新归一化 Quaternion。
静态 alpha 在此层保留 v5 作者整数 `u8`；向 Runtime 的 `/255` 仍只执行一次。

## 3. 容量与预算

### 3.1 16 MiB 与 40,000 的口径

```text
packedFileBytes =
  fixed header + section directory + all section bytes

packedFileBytes <= 16 * 1024 * 1024 = 16,777,216
semanticEntityCount = number of concrete entities after CXT expansion
```

DBG0 若存在也计入 16 MiB。CXC ZIP header、manifest、独立 source entry、模型、
纹理和音频不计入这个 Packed 文件限制，但分别受 CXC 包、Source Project 和资源闭包
预算约束。不得依靠 ZIP 压缩后的大小宣称通过 Packed 门禁。

40,000 是展开后的实体数，不是 CXT 节点数、Archetype 数或压缩块数。Requirement、
动画片段和效果不是实体的替代计数；它们有独立的总量预算。

**40,000 个实体并不限定每个实体附带多少动画或数据。** 因此 40k/16 MiB 必须以明确
复杂度的容量 profile 验收，不能承诺任意无限复杂的 40k 谱面都能装下。超出 profile
的输入仍须给出按 section/字段拆分的超额报告；不得静默删除事件或降精度。

### 3.2 独立计数

| 计数 | 含义 |
| --- | --- |
| `entityCount` | ENT0/IDN0 中具体实体数 |
| `requirementCount` | REQ0 中具体要求数，一个实体可有多个 |
| `eventCount` | 具体 Behavior 事件、Animation Segment/Step、Effect Schedule 定义记录总数 |
| `recordCount` | 每个 section 的物理顶层记录数，含义见第 5 节 |
| `decodedSectionBytes` | 解码 section codec 后的 bytes，不是 C++ heap 占用 |
| `decodedSemanticBytes` | 默认值和字典展开后的受控语义存储占用 |
| `preparePeakBytes` | source、展开记录、Packed、Runtime 等同时存活的峰值 |

重复使用同一 Clip 的事件定义只计一次，但其绑定数量、prepare 展开量和运行时每帧
写入量另外计数；不得以定义去重掩盖执行成本。Foundation profile 的 `eventCount=0`；
Reader 必须拒绝非零 `eventCount` 声明，不能当作「没有事件可读」而忽略。

Reader 必须在分配前检查 counts、UTF-8 bytes、数组乘积、偏移加法、解码占用和资源
引用数。Header 中的声明只用于预检，解码后必须重新计数比对。除 Packed 文件 16 MiB
外，其他新增数值上限由 CFF-D 的实测报告和 capacity profile 在 Foundation 接受前
冻结；没有明确 limits 的 Reader 配置不允许进入 Stage 6。已有 v4/v1 限制不隐式放宽。

## 4. 基础编码

### 4.1 标量

```text
u8/u16/u32/u64    unsigned fixed-width, little-endian
sv               signed int64, ZigZag then shortest unsigned LEB128
uv               unsigned LEB128; target width stated by field
f32/f64          IEEE-754 binary32/binary64 bits, little-endian
text             strict UTF-8, no BOM
UUID             16 bytes in canonical textual hex order, not platform GUID memory layout
```

无特别标注时，索引/count 的 `uv` 上限为 u32；Beat denominator 上限为正 int64。
`sv` 最多 10 bytes；uv32 最多 5 bytes。拒绝截断、非最短表示、溢出和非法高位。
所有浮点值必须有限，`-0` 在进入 canonical model 时规范为 `+0`；不按 C++ struct
alignment 添加 padding。所有索引从 0 起；`IndexPlusOne` 的 0 表示 none。

### 4.2 Beat 编码

每条 Beat 数值流先保存一个 descriptor，再在各记录的对应字段位置保存 atom：

| mode | Descriptor | Atom |
| --- | --- | --- |
| 0：Rational | `u8(0)` | `numerator: sv, denominator: uv64`，每条绝对值 |
| 1：GridDelta | `u8(1), D: uv64` | `scaledNumeratorDelta: sv` |

Rational atom 必须约分，零为 `0/1`。GridDelta 的前一个 scaled numerator 初值为 0，
累加使用 checked int64，值为 `N/D`，解码后约分。每个逻辑字段有独立状态，例如
startBeat 和 endBeat 不能共用前值；不出现的 endBeat 不推进该流。

Writer 尝试该流约分后分母的最小公倍数 D；只在 D、所有 scaled numerator 和相邻
delta 均可表示，且总编码 bytes 小于 Rational mode 时选择 GridDelta。否则使用
Rational mode，长度相等也选 Rational。空流使用 mode 0。不能为适应 grid 舍入 Beat，
也不能因可选 grid 的 LCM 溢出而拒绝可由 Rational 合法表示的谱面。

### 4.3 规范顺序

规范 Writer 对 section 按四个 ASCII code bytes 升序、字符串按 UTF-8 bytes 升序、
实体按规范 identity bytes 升序写出；引用、Archetype 和子表的顺序见下文。
Reader 可以接受不同的 section 物理次序，但字典、实体和记录的内部排序必须符合
合同。Writer 的唯一结果以相同 wire revision、相同 semantic model 和相同
inspection 保留策略为前提，不能对不同 compiler profile 笼统承诺字节相同。

Reader 必须实际校验这些内部次序，不能只依赖规范 Writer，也不能排序后静默接受乱序或
重复项：STR0 严格升序、REF0 按 `(kind, text)`、IDN0 的 scope/path/identity 子表、
ARCH 按 mask 升序、REQ0 按 `(entity ordinal, localId)`、CNS0 按 set payload。

## 5. 文件布局

### 5.1 Fixed Header：96 bytes

| Offset | Field | Type | Meaning |
| ---: | --- | --- | --- |
| 0 | `magic` | 8 bytes | `43 58 50 4b 35 00 00 00`，即 `CXPK5` 加三个零 |
| 8 | `packedVersion` | u16 | `1`，候选物理版本 |
| 10 | `headerBytes` | u16 | 必须为 `96` |
| 12 | `flags` | u32 | bit 0 为 candidate；其余 bit 必须为 0 |
| 16 | `totalBytes` | u32 | 完整文件长度 |
| 20 | `directoryOffset` | u32 | 必须为 `96` |
| 24 | `directoryCount` | u32 | section 数 |
| 28 | `directoryBytes` | u32 | 必须等于 `directoryCount * 32` |
| 32 | `semanticIdentity` | 32 bytes | 第 10 节的规范语义 SHA-256 |
| 64 | `entityCount` | u32 | 具体实体数 |
| 68 | `requirementCount` | u32 | 具体要求数 |
| 72 | `eventCount` | u32 | 第 3.2 节定义 |
| 76 | `decodedBytes` | u32 | 所有 section decodedBytes 的 checked sum |
| 80 | `stringCount` | u32 | STR0 字符串数 |
| 84 | `resourceReferenceCount` | u32 | REF0 中 asset 类引用数 |
| 88 | `candidateRevision` | u32 | 本草案为 `1`；正式发行时为 `0` |
| 92 | `headerCrc32` | u32 | 对 96 bytes Header 计算，本字段临时填零 |

本草案对应的 Foundation/Stage 6 Header 必须为 `flags=1, candidateRevision=1`，并通过
显式 candidate path 加载。Schema/wire 合同发生不兼容候选修改必须递增 revision；
Reader 只接受明确实现的 revision，不猜测兼容。Stage 8 才能接受 `flags=0` 的正式
发行合同；当前工具不得写出正式标志。

CRC 使用 CRC-32/ISO-HDLC：reflected polynomial `0xEDB88320`、init 和 xorout 都为
`0xffffffff`，对 bytes 顺序计算，结果 u32 little-endian；`123456789` 的检查值为
`0xcbf43926`。CRC 用于损坏检测，不是认证。整文件 SHA-256 属于外部 artifact identity，
不能把它直接写进被自身覆盖的 Header。

### 5.2 Section Directory：每项 32 bytes

| Offset | Field | Type | Meaning |
| ---: | --- | --- | --- |
| 0 | `type` | 4 ASCII bytes | 四字符 section code |
| 4 | `codec` | u8 | 本版只允许 0：uncompressed |
| 5 | `flags` | u8 | 0：semantic；1：inspection；其他值拒绝 |
| 6 | `reserved` | u16 | 必须为 0 |
| 8 | `offset` | u32 | 从文件起点的 byte offset |
| 12 | `encodedBytes` | u32 | section 的文件字节数 |
| 16 | `decodedBytes` | u32 | codec 解码后字节数；codec 0 时等于 encodedBytes |
| 20 | `recordCount` | u32 | 顶层记录数 |
| 24 | `sectionCrc32` | u32 | 对 encoded section bytes 计算，同上 CRC 算法 |
| 28 | `reserved2` | u32 | 必须为 0 |

目录紧接 Header，sections 紧接目录并按目录顺序连续存放；禁止 gap、重叠、重复
section type 和尾随 bytes。所有 range 用 checked arithmetic 验证。目录本身无独立
CRC，须逐字段验证；CXC 的整 entry SHA-256 覆盖目录。无 CXC 时仍须完成语义 hash 校验。

未知 semantic section 必须拒绝；未知 inspection section 可在检查长度和 CRC 后忽略，
但不能提供任何运行时必需数据。新 codec 不属于 Foundation；必须有新的明确 wire 合同。

Foundation revision 1 的 Reader 行为固定为：登记 semantic section 携带 `flags=1` 时以
flags 诊断拒绝（不得冒充「未知 section」）；未登记的 `flags=1` section 在长度与 CRC 校验后
忽略；未登记的 `flags=0` section 一律拒绝；未完成后续字段合同的登记 section 使用独立的
「拒绝」诊断。Writer、结构检查和语义解码必须使用同一注册表判定，不得各自维护清单。

### 5.3 Section 注册表

| Code | 内容 / recordCount | Foundation revision 1 |
| --- | --- | --- |
| META | 单个 Chart 全局记录 / 1 | 必需 |
| TIME | 单个 TimingMap / 1 | 必需 |
| STR0 | UTF-8 字符串 / stringCount | 必需，可空 |
| REF0 | typed references / referenceCount | 必需，可空 |
| IDN0 | 具体实体身份 / entityCount | 必需，可空 |
| ARCH | 物理默认值 / archetypeCount | 必需，可空 |
| ENT0 | 实体索引 / entityCount | 必需，可空 |
| TRN0 | Transform 差异行 / 对应实体数 | 存在 bit 0 实体时必需，否则省略 |
| REN0 | Renderable 差异行 / 对应实体数 | 存在 bit 1 实体时必需，否则省略 |
| REQ0 | 要求 / requirementCount | 必需，可空 |
| CNS0 | constraint sets / setCount | 必需，可空 |
| CAM0 | Camera 差异行 / 对应实体数 | 存在 bit 4 实体时必需，否则省略 |
| BEH0 | Behavior binding / 对应实体数 | 后续合同，Foundation 拒绝 |
| BHD0 | typed Behavior definitions / definitionCount | 后续合同，Foundation 拒绝 |
| ANM0 | typed Animation definitions/bindings | 后续合同，Foundation 拒绝 |
| FXS0 | finite Effect sets/schedules | 后续合同，Foundation 拒绝 |
| DBG0 | inspection bytes / byteCount | 可选，flags 必须为 1 |

除 DBG0 外本表 flags 必须为 0。未完成后续字段合同的 section 不是任意 payload 的
扩展口；即使为空也不能被 Foundation Reader 当作支持。空必需表仍保存其明示的表头。
`BEH0`/`BHD0`/`ANM0`/`FXS0` 属于此类，Foundation revision 1 必须显式拒绝；`DBG0` 是唯一
登记的 inspection section，Reader 接受并忽略其 payload，不从中读取判定、身份或资源信息。
下述无内部 count 的表由目录 recordCount 定界，必须恰好消费全部 section bytes。

## 6. 字典和全局记录

### 6.1 STR0

```text
u32 count
u32 dataBytes
u32 offsets[count + 1]
byte data[dataBytes]
```

`count` 必须与 Header/目录一致。offsets 单调不减，首项 0、末项 dataBytes，各区间
恰为一个完整 UTF-8 字符串。字符串去重并按 bytes 严格升序；稳定 ID 再验证 portable
ID 规则。高频记录只存字符串索引，不重复写字段名和 AssetId。

### 6.2 REF0

每行：`kind: u8, stringIndex: uv32`。

| Kind | Domain |
| ---: | --- |
| 1 | asset |
| 2 | behavior |
| 3 | animation |
| 4 | judgement-domain |
| 5 | action |
| 6 | effect |
| 7 | extension/capability |

按 `(kind, string bytes)` 去重排序。kind 不匹配、未知 kind 或越界引用均失败。
实体 parent 和 target 使用 entity ordinal，不把 object identity 伪装成字符串引用。
行为/动画引用还必须有对应定义；判定域/动作必须由显式声明的 gameplay profile 解析，
不能回退到宿主碰巧存在的同名函数。Foundation 只接受其登记的演示 profile，
不声称已具有 Stage 7A 的输入映射和判定实现。

### 6.3 META

```text
chartId                      16 UUID bytes
semanticVersion              u16 = 5
mainMusicRefIndexPlusOne      uv32, 0 means none
defaultCamera                CameraPayload + pitch/yaw/roll (3*f64) + position (3*f32)
featureCount                 u32
features[featureCount]       featureRef: uv32, version: u32
```

CameraPayload 见第 7.3 节。Chart 默认 Camera 总是保存完整 resolved 值，不能遗漏
pitch/yaw/roll 或 defaultTransform。mainMusic 必须是 asset reference 并通过音频类型验证。
features 按引用文本 ID 排序且唯一，包含会改变求值/解码要求的显式能力及 gameplay profile。
资源需求从完整 REF0 和 typed definition 验证，不包含 source path 或实际资源 bytes。

### 6.4 TIME

```text
offsetMs, defaultBpm          2*f64
tempoCount, stopCount        2*u32
tempoStartCodec              Beat descriptor
tempoDurationCodec           Beat descriptor
stopBeatCodec                Beat descriptor
tempo[tempoCount]            startBeat atom, durationBeat atom,
                            startBpm, endBpm, startSlope, endSlope (4*f64)
stops[stopCount]             beat atom, durationMs: f64
```

Tempo 和 Stop 分别按 Beat 排序。值域、Stop 边界、Hermite 与重叠检测沿用
[TIMING_MODEL.md](TIMING_MODEL.md)，Packed 不改变时间算法。

### 6.5 IDN0：不可删除的实体身份

完整 identity 不得只保存在 DBG0。每个实体都有一条 IDN0 记录；实体 UUID 不在多个
component 行中重复。显式实体使用已有 UUID；生成实体保留 CXT 的 typed tuple。

```text
u32 scopeCount
scopes[] {
  chartId: UUID16
  bindingIdStr, moduleIdStr, exportIdStr: uv32 each
}
u32 pathCount
paths[] {
  stepCount: uv32
  steps[] { nodeIdStr: uv32, indexed: u8 }   // indexed is 0 or 1
}
u32 identityCount
identities[] {
  tag: u8
  tag 0: uuid: UUID16
  tag 1: scopeIndex: uv32, pathIndex: uv32,
         iterationIndex: uv32 for each indexed step
}
```

identityCount 必须等于 Header/目录的 entityCount。scopes 按 decoded tuple 排序去重，
paths 按逐步 `(nodeId bytes, indexed)` 排序去重。路径只是身份标签的前缀共享，
没有 Prototype 数据、count、body、表达式或可执行生成规则。每个 identity 仍须显式
保存，Reader 不从一个路径记录自动补出一批实体。

完整 generated tuple 为：

```text
(chartId, bindingId, moduleId, exportId,
 [(nodeId, iterationIndexPlusOne), ...])
```

非 indexed step 的 iterationIndexPlusOne 为 0；indexed step 为 index+1。
数值相加必须检查范围。生成路径遵守 CXT 的结构规则，最后一个 step 必须为非 indexed
的 emit 标签。顶层直接 Prototype invocation 的合成标签由 CXT 规范定义。
scope chartId 必须与 META 一致。显式和生成 identity 的 tag 构成不同域，不会互相覆盖。

规范 identity bytes：先写 tag；tag 0 后写 UUID16；tag 1 后写 UUID16、三个
`u32 length + ASCII bytes`、`u32 stepCount`，再逐步写
`u32 nodeIdLength + nodeId bytes + u32 iterationIndexPlusOne`。按这些 bytes 升序排列
identities，严格去重；这同时定义 entity ordinal 和 ENT0 的顺序。

## 7. 实体与 Component 流

### 7.1 ARCH：物理默认值

每行：

```text
componentMask: u64
defaultsByteCount: u32
defaults: component payloads in increasing bit order
```

archetype index 是行号。candidate component mask：

| Bit | Component / 默认 payload |
| ---: | --- |
| 0 | Transform / TransformPayload |
| 1 | Renderable / RenderablePayload |
| 2 | Requirement presence / 无默认 payload，由 REQ0 表独立承载 |
| 3 | Behavior binding / 后续合同 |
| 4 | Camera / CameraPayload |
| 5 | Element marker / 无 payload |
| 6 | Animator / 后续合同 |
| 7 | Effect binding / 后续合同 |

bits 8..63 未定义。Foundation Reader 只接受 bit 0/1/2/4：bit 3/6/7 与 bits 8..63 一律
拒绝，不能静默丢弃未实现 Component 的声明。每行的 defaultsByteCount 必须和实际
payload 长度一致。Archetype 不保存实体 ID、parent、requirement identity 或 CXT。

candidate canonical Writer 为每个不同 componentMask 建立**恰好一个** Archetype，
按 mask 升序排列；对每个可默认化 Component 选出现次数最多的完整 Component 值作为
默认值，同频时按该 Component 规范字段 bytes 升序决胜。Quaternion 是完整原子值，
不能分量各选众数后拼出非法旋转。所有实体拥有相同 mask 时无需重复四万个原型。
其他优化分组策略必须属于明确的新 writer profile，不以任意启发式破坏可复现写出。

### 7.2 ENT0

每行：`parentOrdinalPlusOne: uv32, archetypeIndex: uv32`。

行号即 entity ordinal，与 IDN0 一一对应，不再存冗余 UUID/identity index。
parent 0 表示 root；其他值减一必须小于 entityCount。前向 parent 引用允许，
但构建完整关系图后必须拒绝缺失、自引用和循环。名称属于 DBG0，不影响运行身份。

### 7.3 静态 Component payload 和差异行

Component 的完整字段顺序与差异 mask 使用相同的 field index：

| Payload | Field indices / type |
| --- | --- |
| Transform | 0..2 position x/y/z：f32；3 rotation `[x,y,z,w]`：4*f32；4..6 scale x/y/z：f32 |
| Renderable | 0 meshRef：uv32；1 materialRef：uv32；2 alpha：u8 |
| Camera | 0 type：u8（1=perspective）；1 fovY：f64；2 near：f64；3 far：f64 |

mesh/material 必须指向 asset 类型 REF0，并通过对应资源类型验证。Camera 沿用 v4
值域，不因物理化扩大 FOV。Transform 旋转的规范化容差与 typed Reader 一致，
Decoder 不重新 normalize。

TRN0/REN0/CAM0 每行：

```text
entityOrdinalDelta: uv32
changedFieldMask: uv32
changedValues: typed fields in increasing field index
```

首个 ordinalDelta 为绝对 ordinal，后续 delta 必须严格大于 0。每个有该 component
bit 的实体必须恰好一行；没有该 bit 的实体不得出现在此表。mask=0 合法，表示完全使用
Archetype default；未知 mask bit、重复行和缺失行都失败。

每个 set bit 必须携带完整 field atom，unset bit 不占 bytes；Quaternion 不能只写一部分。
规范 Writer 只把与默认值不同的字段设为 changed。Decoder 先复制该实体的 Archetype
默认值，再应用差异，最后验证具体 Component。不从前一个实体继承字段。

示例：共享 Transform 默认位置 `(0,0,0)`、单位旋转/缩放的第一个实体，仅 x=2：

```text
TRN0 row = 00 01 00 00 00 40
           |  |  +---------- f32 2.0
           |  +------------- changed mask bit 0
           +---------------- first ordinal 0
```

下一实体使用全部默认值：`01 00`，即 ordinal delta=1、mask=0。这是字段差异编码，
不是 CXT Instance binding，也不是以“上一个位置”作为语义默认值。

### 7.4 REQ0：一个实体可有多个要求

section 先写 `startBeatCodec, endBeatCodec`，随后按 `(entity ordinal, localId bytes)`
严格升序保存目录指定数量的记录：

```text
entityOrdinalDelta           uv32
localIdStr                   uv32
requirementKind              u8
intervalKind                 u8
startBeat                    Beat atom
endBeat                      Beat atom, only when intervalKind=1
judgementDomainRef           uv32
requiredActionRef            uv32
constraintSetIndexPlusOne    uv32
effectSetIndexPlusOne        uv32
```

首 ordinalDelta 为绝对值；后续允许 0，因为同一实体可以有多个 localId。完整
RequirementIdentity 为 `(EntityIdentity, localId)`。重复 identity 失败；Reader 必须校验
`(entity ordinal, localId bytes)` 严格升序，乱序或重复都以稳定诊断拒绝。
拥有 bit 2 的实体必须有至少一条 REQ0，其他实体不能拥有 REQ0。

intervalKind 的编码为 `0=point, 1=half-open-range`；range 必须满足 end>start。
point 是名义目标 Beat，不意味着容差窗为零；允许窗口由 typed constraint/判定
profile 定义，不能误把 `[t,t)` 空区间作为目标。requirementKind 与 intervalKind
独立：Foundation 示例仅登记 `1=tap` 并要求 point；Hold/Release 和复合要求必须
经过 Stage 7A/7B+ 的独立规则合同，不因有一个 range 字段便声称已支持。

domain/action 的 REF0 kind 分别为 4/5。Foundation effectSetIndexPlusOne 必须为 0。
v4 `cuexis.note.beat` 不能由 Packed Writer 自行猜成 tap/press；迁移必须在明确规则或
用户配置下由上游产生完整要求，未覆盖的旧语义应明确报不支持。

### 7.5 CNS0

每个 constraint set：

```text
constraintCount: uv32
constraints[] {
  kind: u8
  kind 1: lane: uv32
}
```

Foundation 只登记候选离散 `lane` constraint。每 set 至少一项、kind 唯一并按 kind
升序；sets 按完整规范 payload bytes 去重排序，Reader 必须校验该次序（本 revision 即
按 lane 严格升序）而不是沿用首次出现次序。none 使用 REQ0 的 0，不保存空 set。
lane 的有效范围必须由引用的候选 judgement domain 校验；它不是屏幕 x 值。
新的约束必须先有 typed schema 和独立 capability，不允许任意 JSON 进入 CNS0。

### 7.6 Foundation 演示 profile

| 项 | revision 1 登记值 |
| --- | --- |
| META feature ID / version | `cuexis.gameplay.candidate.lanes4` / `1` |
| judgement-domain | `candidate.lanes4`，离散整数 lane `[0,3]` |
| action | `press`，仅为动作标签 |
| requirement | kind `tap`（wire=1），interval `point`（wire=0） |
| constraints | 恰好一个 `lane`（kind=1），无额外 constraint |
| effects | 必须为空 |

存在 REQ0 时须声明该 feature，所有要求都必须满足本表。没有 REQ0 时可以不声明。
Foundation revision 1 不接受其他 gameplay profile 或 domain/action 组合。
Writer 与 Reader 必须用同一组条件判定本表，并使用同一稳定的 profile 诊断族：未声明或
未登记的 feature ID/version、requirement kind 非 `tap`、interval 非 `point`、domain/action
不匹配、约束不是恰好一个 `lane`、lane 超出 `[0,3]`、非空 effect set。profile 判定必须在
semanticIdentity 比对之前完成，诊断不得伪装成身份不符。
该 profile 只证明时刻、域、动作和约束能被无损存储，不定义键位映射、容差窗口、
计分或 Hit/Miss 算法，不能用它冒充 Stage 7A 的可玩判定。

## 8. 后续 section 与扩展边界

Foundation 不提前杜撰全部判定、动画和效果的完整 wire layout：

| Section | 必须在启用前补齐的合同 | 拥有阶段 |
| --- | --- | --- |
| BEH0/BHD0 | 绑定和定义分表；property registry；Beat、端点、slope、step、引用和事件计数 | Stage 6 所需 subset / Stage 8 收敛 |
| ANM0 | literal Clip v2；Layer/Group/Instance 独立表；共享 Clip 与绑定计数；离散权重和无限采样循环边界 | Stage 6 所需 subset / Stage 8 |
| FXS0 | 有限 trigger/filter/effect/target；调度 identity；引用和重入/Seek/Replay 规则 | Stage 7A/8，扩展随 7B+ |
| REQ0/CNS0 扩展 | Hold、Release、组合、容差、顺序和 profile 版本 | Stage 7A/8，高级类型随 7B+ |

未完成字段表、enum 分配、边界正反例与 golden 之前，Reader 必须拒绝相应 section/
kind，Writer 不得用 opaque JSON、源码路径或 CXT AST 代替。Stage 6 需要消费的能力
必须先补该能力的 candidate revision；不能拿本草案当成“所有 v5 已可编码”的证明。
正式发行仍归 Stage 8，而非等待全部 Stage 7B+。

天空盒/Environment、模型引用与形变后续采用独立 typed Presentation section/profile，
不能更改 TRN0 静态坐标含义或把模型 bytes 塞进每个 ENT0。新增语义 section 对旧
Reader 是明确的不支持；inspection flag 不能用于绕过这个门禁。

DBG0 可保存 source mapping、名称、字段路径和作者提示。本草案不冻结其内部格式，
因此只作受界限约束的 opaque inspection bytes；不读取其中任何信息作判定、身份、
资源解析或 Runtime 构建。发行 canonical Writer 默认省略所有 inspection sections。

## 9. Reader / Writer 和失败合同

解码顺序：

```text
16 MiB file gate
  -> Header size/magic/revision/CRC
  -> directory bounds/count/type/codec and checked arithmetic
  -> required sections and all section CRCs
  -> STR0 / REF0 / IDN0
  -> META / TIME / ARCH
  -> ENT0 and parent graph
  -> static component streams / CNS0 / REQ0
  -> declared supported schedule sections
  -> complete semantic validation and actual budget counters
  -> semanticIdentity verification
  -> publish typed candidate for Runtime prepare
```

物理次序不决定依赖次序。区间、循环、type 错误和 hash 不匹配都须在 Runtime/World
发布前失败。profile 校验属于「complete semantic validation」，其拒绝必须先于
semanticIdentity 比对，使一个 hash 自洽但超出登记 subset 的产物报告 profile 原因。失败不发布半份新 Chart；会话切换时旧的已提交 active chart 按原事务
合同保留，但不能被冒充为本次成功结果。诊断至少含 section、record、字段和错误类别。

Writer 只接受已展开且引用完整的 canonical model。在写盘前通过无副作用 sizing pass
计算确切 bytes、count 和 checked arithmetic，超过 16 MiB 不产出部分有效文件。
输出使用临时文件和原子替换，文件操作错误也必须保留上一次有效产物。

安全失败例至少覆盖：截断 Header/varint、超长字典、UTF-8 错误、偏移溢出、目录重叠、
CRC 错误、重复身份、父循环、mask 不匹配、缺行、多个 Requirement 的重复 localId、
LCM 溢出 fallback、无效 Rational、未知 required section/revision、预算超限和语义 hash 不符。

## 10. Identity 和规范 round-trip

身份分四层，不能只用一个含糊的 “Chart hash”：

| Identity | 覆盖 |
| --- | --- |
| source/build identity | 作者 Chart/CXT canonical source、冻结参数和 compiler profile |
| semanticIdentity | 展开后的规范语义，不含 AST、源路径、inspection 和物理字典选择 |
| artifactIdentity | 完整 Packed bytes 的 SHA-256 |
| prepared/replay identity | semanticIdentity + 实际资源内容 + 输入/判定配置等执行合同 |

source/build 不相同的输入可以生成相同语义；未使用参数不应仅因值不同就使展开语义
不同。参数影响 Beat、实体数量、值或闭包时，自然进入 semanticIdentity。生成 identity
中的 module/binding/node 标签本身是稳定语义键，不等同于保留整个 source module。
v4 的 `PreparedSemanticIdentity` 不改写；v5 prepared/replay 的具体组合由 Stage 7A/8
版本合同冻结。

source identity、build identity 可以进一步各自记录；本表将其并列只是为了区别于
展开语义，不要求把源文件 hash 和编译配置合并成唯一一种存档身份。
它们的完整分帧/排序算法尚未在本草案冻结，归 CFF-C 与 Stage 8 的 manifest/build
provenance 合同；在算法和 golden 接受前只能记录分项源文件与参数配置，不得当作
跨工具互验的统一 digest。Header 当前只强制使用以下明确的 semanticIdentity。

### 10.1 Foundation 语义 hash 字节合同

预映像所有整数均为固定宽度 little-endian，**不使用 varint**。基础编码：

```text
S(text)       u32 UTF-8 byte length + exact UTF-8 bytes
R(reference)  u8 kind + S(id)
B(beat)       i64 reduced numerator + u64 positive denominator
I(identity)   u32 byte length + canonical identity bytes from 6.5
NoParent      u32(0)
```

i64 为二进制补码。按以下顺序直接串接，之后计算 SHA-256：

| 顺序 | Bytes |
| ---: | --- |
| 1 | ASCII `cuexis.chart.semantic.v5.candidate.1` + NUL，再写 u16(5) |
| 2 | META chartId：UUID16 |
| 3 | mainMusic：无音乐写 u8(0)；有音乐写 u8(1)+R(asset) |
| 4 | defaultCamera：u8(type)、fovY/near/far/pitch/yaw/roll 共 6*f64、position 3*f32 |
| 5 | u32 featureCount；每项按 ID 升序写 R(feature)+u32(version) |
| 6 | TIME offsetMs/defaultBpm：2*f64；u32 tempoCount + u32 stopCount |
| 7 | 按 Beat 升序每项 Tempo：B(start)+B(duration)+4*f64(bpm endpoints/slopes) |
| 8 | 按 Beat 升序每项 Stop：B(beat)+f64(durationMs) |
| 9 | u32 resourceCount；按 ID 升序写各 R(asset)，包括规范资源需求闭包 |
| 10 | u32 entityCount；每个实体按 identity-byte 顺序使用下表 |
| 11 | Behavior/Animation/Effect 定义数量：三个 u32(0) |

实体预映像：

| 顺序 | Bytes |
| ---: | --- |
| 1 | I(entityIdentity)；parent 为 NoParent 或 I(parentIdentity) |
| 2 | u64 componentMask |
| 3 | 按 bit 升序写完整 Component：Transform 原始 10*f32；Renderable 为 R(mesh)+R(material)+u8(alpha)；Camera 为 u8(type)+3*f64；bit 2/5 无内联 payload |
| 4 | u32 requirementCount |
| 5 | 每项按 localId 排序：S(localId)+u8(requirementKind)+u8(intervalKind)+B(start)，range 时再写 B(end) |
| 6 | 每项要求接着写 R(domain)+R(action)+u32 constraintCount；按 kind 升序写 u8(kind)+u32(lane) |
| 7 | 每项要求最后写 u32(0) 表示空 effect set |

实体表的 5..7 步对每项 Requirement 连续执行，不拆成三列。浮点保持完整 f32/f64
bits，所有 zero 已规范为 +0；不散列 Runtime 指针、字典索引、Archetype 编号或
压缩选择。新语义/调度 payload 启用时须更新 revision 和对应的 hash 字段合同。

revision 1 的 Header 必须携带按本节预映像计算出的摘要。Reader 在结构、预算和语义校验
之后重算并比对，任何不一致（包括全零摘要）都必须稳定拒绝，不允许为兼容而「零值即跳过」。
在本合同实现之前生成的零 hash 候选产物必须显式重新生成，不能继续作为有效 artifact 消费。

### 10.2 候选 hash golden

以下仅验证本节预映像编码，不代表生产 Reader 或完整 Packed 文件已实现：

```text
shared:
  chartId = 019b0000-0000-7abc-8def-000000000001
  no mainMusic / no resources
  camera = perspective, fovY=60, near=0.1, far=1000
  pitch=yaw=roll=0; position=(0,0,0)
  timing = offsetMs=0, defaultBpm=120, no tempos/stops
  no Behavior/Animation/Effect definitions

empty:
  no features / no entities

one-tap-lane2:
  feature = cuexis.gameplay.candidate.lanes4 version 1
  one explicit entity = 019b0000-0000-7abc-8def-000000000010
  no parent; componentMask=4; no static Component payload
  one requirement: localId=hit, kind=tap, interval=point at 0/1
  domain=candidate.lanes4, action=press, one lane constraint with lane=2
  no effects
```

| 输入 | Preimage bytes | SHA-256 |
| --- | ---: | --- |
| empty | 165 | `9372e8f76da7234fd6f3d31c835bc24a98b4c65c53bf4e9ca37fbe672507d178` |
| one-tap-lane2 | 312 | `9b1714dc3087fddd2a73ad2d947c1f9854e17f5964135085a232edb31003a091` |

### 10.3 Round-trip

必须建立：

```text
decode(write(canonical)) == canonical
decode(pack(expand(cxt))) == expand(cxt)
semanticIdentity(decoded) == header.semanticIdentity
write(decode(validPacked), defaultProfile) == canonicalPackedBytes
```

等价比较包含实体身份、parent、Component presence/value、要求区间/动作/域/约束、
时序/资源和已支持的调度，不比较 source JSON 空白。删除 DBG0 不改变 semanticIdentity，
但会改变 artifactIdentity。CRC 或 SHA 校验成功都不能代替语义校验。

## 11. 容量算账与门禁

16,777,216 / 40,000 约为每实体 419 bytes，且这笔空间还要分给 Timing、字典、
身份和动画。仅用 CXT 减少 source JSON 的行数，不能证明最终 Packed 能通过。

以下是成本示例，不是已实现的测量结果：

| 项目 | 40,000 实体时的示意成本 |
| --- | --- |
| 显式 UUID identity | 40,000 * 17 = 680,000 bytes，另有 IDN0 表头 |
| ENT0，无 parent、单 Archetype | 40,000 * 2 = 80,000 bytes |
| TRN0，仅 x 不同 | 每行通常 6 bytes，总计约 240,000 bytes |
| REN0，共用全部默认资源/alpha | 每行通常 2 bytes，总计约 80,000 bytes |
| REQ0、constraint sets | 随 ID/引用宽度、Beat、要求数量变化，必须分别实测 |
| ANM0/FXS0/资源字典 | 独立计费，不可遗漏或按“模板只出现一次”推断为零 |

低重复 Transform 最多写完整 40-byte payload 加 ordinal/mask；不能只测全默认实体。
多个 requirements、长稳定 ID、大量不同资源或每实体独立动画可能成为真正瓶颈。
Writer 必须提供每 section bytes、dictionary/default 命中量和 top field contributors。

Foundation 接受前必须交付三个公开 fixtures/profile：高复用 CXT、低复用显式实体、
受控混合复杂度；均按展开后 40,000 实体验证 Packed <=16 MiB，并附 source bytes、
decodedSemanticBytes、preparePeakBytes、编译时间报告和原始数据。Stage 8 在同样
计数口径上增加动画/判定/效果 profile，不得以 Foundation 静态结果替代正式发行验收。
尚无实测时，这仍是目标和候选预算模型，不能标为“已支持 40k/16 MiB”。

## 12. 交付与 CXC

Foundation 交付本草案的 Header/目录、字典/身份、静态 Component、候选 tap requirement、
无损 Beat 和受预算控制的 Reader/Writer 原型及 golden。Stage 6 在 accepted Foundation
上以显式 candidate path 消费，保留 v4 兼容回退；未登记的语义稳定拒绝。

Stage 8 冻结正式 wire/profile、Stage 7A 要求集、Animation Extension、迁移和默认 Writer。
CXC v1 的发行映射必须明确：

```text
entry encoding = packed-chart
playback entry points to validated Packed bytes
artifact identity = SHA-256(exact entry bytes)
compiled semantic identity = Packed header.semanticIdentity
compiler profile and expanded counts are declared and verified
```

这些是 [CXC_FORMAT.md](CXC_FORMAT.md) 的候选 Chart entry extension 映射，不是给
现行 manifest 三字段 entry 任意添加字段。CXC 不重新定义 Packed layout；Playback 不
读取包内 CXT source 补齐缺失信息。Source Project/source entries 可选保留作者资料，
资源内容仍由 CXC/Asset 闭包合同提供。
