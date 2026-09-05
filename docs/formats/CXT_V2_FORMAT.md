# Cuexis Chart Template (CXT) v2

状态：candidate；Foundation 作者层模板提案；未实现，非生产 Schema

更新日期：2026-09-05

依据：[玩法抽象模型](../architecture/GAMEPLAY_ABSTRACTION_MODEL.md)、
[Chart v5 计划](../stage_plans/active/chart-format-update-for-v5/plan.md)、
[Packed Chart](PACKED_CHART_FORMAT.md) 和 [CXT v1](CXT_FORMAT.md)。

## 1. 设计目的

```text
Chart v5 JSON + CXT v2
  -> validate / freeze parameters / finite expansion
  -> Canonical Semantic Chart
  -> Packed Chart
  -> bounded decode / Playback prepare
```

CXT 减少作者层重复：公共 Component、槽位、时间/位置序列只需声明一次。Packed
负责发行层的字典化、索引化和字段差异编码。两者不能混为一种“运行时模板压缩”。
CXT Prototype 不直接成为 Packed Archetype，CXT Instance 也不是 Packed EntityRecord。

展开后的结果必须满足：

```text
没有 CXT AST、参数引用或未展开 Pattern
每个实体的 identity、Component、parent 和 Requirement 已经具体化
实体数、要求数、事件数和资源闭包均可计数
不依赖画面、输入流、Judgement 状态或宿主回调
```

展开仅在 source authoring prepare 或显式编译阶段发生，不在发行 Playback 或每帧
路径中发生。改变被冻结的 CXT 参数需要重新编译另一个 Packed artifact，不能向
发行 Playback 提交 host 参数要求隐式重跑模板。Chart v4 的既有参数路径不改写。

CXT v1 保持冻结；Chart v4 只接受 CXT v1，Chart v5 只接受 CXT v2。

## 2. Module 与导出

```text
extension       .cxt, exact lowercase
encoding        UTF-8 JSON, without BOM
format          cuexis.animation-template
version         2
moduleKind      prototype | pattern | animation
export count    exactly one
```

顶层字段均必需：

| 字段 | 合同 |
| --- | --- |
| `format`、`version` | 固定标识与整数版本 |
| `moduleId` | portable ASCII stable ID；Chart import ID 必须与之相等 |
| `moduleKind` | 本节列举的三种类型之一 |
| `metadata` | inspection 对象；不进入展开语义，但计入 source identity/bytes |
| `parameters` | 模块参数声明；ID 唯一 |
| `prototypes` | Prototype 定义；ID 唯一 |
| `patterns` | Pattern 定义；ID 唯一 |
| `animations` | Animation 定义；ID 唯一 |
| `exports` | 唯一元素为 `{ "kind": "...", "id": "..." }`，引用同文件声明 |
| `requiredExtensions`、`extensions` | 沿用版本化扩展边界；不能承载未知求值逻辑 |

`prototype` 模块只能有 prototypes；`pattern` 模块允许 prototypes 和 patterns；
`animation` 模块只能有 animations，且 parameters 必须为空。未使用的数组也必须以
空数组出现。export kind 必须与 moduleKind 相同。未知核心字段和重复 ID 均失败。

首版一个文件一个 export，不支持 nested import、跨模块 Prototype 调用或继承。
Chart 可以通过多个显式 import 组合模块；不允许按名字扫描 SDK/网络/宿主安装目录。
多 export 和跨模块调用如确有需求，另立 extension，不靠字符串拼接绕过本合同。

## 3. Parameter、Slot 与 ValueSource

### 3.1 两种输入

**Parameter** 是模块 invocation 输入；**Slot** 是 Prototype 输入。二者都只在
有限展开期间使用，不能成为运行时变量。参数声明：

```json
{
  "id": "groups",
  "type": "integer",
  "default": 4,
  "minimum": 1,
  "maximum": 10000
}
```

Slot 声明：

```json
{
  "id": "beat",
  "type": "beat",
  "required": true
}
```

| Type | 值与约束 |
| --- | --- |
| integer | signed int64；必须声明 minimum/maximum |
| number | finite binary64；可声明 minimum/maximum |
| rational | 约分 int64 numerator / positive int64 denominator |
| beat | 同 rational，单位为局部 Beat |
| boolean | JSON Boolean |
| enum | 非空唯一字符串 values，default 必须属于 values |
| vector | 固定 length=2/3/4 的有限 number 数组 |

rational/beat 可声明相同类型的 minimum/maximum。vector 必须声明 length。
Parameter 必须有 default；Slot 的 `required:false` 必须有 default，`required:true`
禁止 default。范围、类型和字段白名单在声明与每次实际绑定时都要验证。

首版**不提供 reference 参数或引用槽位**。AssetId、domain/action reference、Component
类型/version、约束类型和 effect target 必须 literal。ID、parent、priority、旋转、
alpha、拓扑和 Animation Track 值也不能通过新 Core 参数间接修改。

Parameter 可以控制 Repeat 的展开数量，但不能改变源 AST、固定 Component 数组形状
或每次 emission 的父子关系规则。展开数量变化是明确受计数预算控制的例外，
不能用“数组结构不变”错误地禁止有限 Repeat。

### 3.2 值来源语法

所有来源都显式带 `kind`：

```text
AtomSource ::=
    { kind: "literal", value: TypedLiteral }
  | { kind: "parameter", id: ParameterId }
  | { kind: "slot", id: SlotId }
  | { kind: "index", id: RepeatIndexId }

ValueSource ::= AtomSource
  | { kind: "affine", input: AtomSource, scale: AtomSource, offset: AtomSource }
```

作用域和使用位置固定：

| 位置 | 允许来源 |
| --- | --- |
| Chart invocation parameters / slotBindings | 仅 literal；不能把未解析 ChartParameterRef 带入 CXT |
| Prototype fields / Requirement 数值 | literal、slot，以及只含 literal/slot 的 affine |
| Pattern emit bindings | literal、parameter、可见的 Repeat index，以及相应 affine；不接受 slot |
| Repeat count | literal integer 或 integer parameter |
| Component/Requirement 的结构、类型与引用 | 仅 literal |

Prototype 不读取调用者 lexical index；它只能通过 slot 接收值。禁止 Slot 引用另一
Slot 的定义形成求值图，Slot default 必须 literal。Index ID 在可见作用域中不能
shadow；引用不存在、跨作用域或未绑定的来源稳定失败。

affine 是受限的单层运算：

```text
result = input * scale + offset
```

禁止嵌套 affine、任意函数、两个 index 相乘或由 index 生成 scale/offset。
Pattern 中 scale/offset 只允许 literal 或已冻结 parameter；Prototype 中只允许
literal 或已绑定 slot。它们在当前 emission 求值前固定，不是运行时动态状态。

候选类型组合：

| 结果 | input / scale / offset |
| --- | --- |
| integer | integer / integer / integer |
| rational 或 beat | integer / 同结果类型 / 同结果类型 |
| number | number 或可精确转为 binary64 的 integer / number / number |
| vector | 固定长度 vector / number / 同长度 vector |

integer/Beat 使用 checked exact arithmetic；溢出失败、不 wrap、不转 double 猜分数。
number/vector 固定先乘后加、逐次 round-to-nearest ties-to-even，不允许 FMA 或重新结合；
拒绝非有限值，零规范为 +0。boolean/enum 不接受 affine，rotation 不作为 vector 来源。

### 3.3 参数化白名单

Foundation Core 首先开放 Requirement Beat、候选 lane 数值、静态 position/scale 和
Repeat count。其他注册字段如需参数化须有明确候选白名单，不能因为 type=number
就默认所有字段都可写。合法值还要通过既有 Component 值域验证。

传统 ChartParameter、Animator durationScale/weight 和 CXT Animation Extension
仍遵守各自合同；本节不是放开 CXT Track、alpha、Quaternion 或资源引用参数化。

## 4. Prototype

Prototype 定义**一个实体**的固定结构：

```text
Prototype {
  id: local stable ID
  slots: Slot[]
  components: { type, version, fields: { path, source }[] }[]
  requirements: Requirement[]
  extensions: {}
}
```

同一个 Component type 最多出现一次，不能通过改变 version 重复声明。
`path` 是注册 Schema 的字段路径，不是任意 JSON Patch。数组下标只用于固定 vector
叶子，例如 `position[0]`；不允许 `add/remove` 或以路径替换 Component 结构。
Quaternion、资源引用属于一个完整字段，必须整体 literal，不拆分其 domain/id。

未给出的字段只允许使用版本化 Schema 明确声明的默认值；必需且无默认的字段
遗漏即失败。Transform 的候选默认值明确为 position `(0,0,0)`、rotation `(0,0,0,1)`、
scale `(1,1,1)`，Renderable alpha 为 255；mesh/material 无默认，若有 Renderable 必须
声明两者。生成具体 Component 后仍走统一 typed validation。

Prototype **不支持 `extends`、继承或宽泛 overrides**。差异只能通过声明的槽位
绑定，不通过隐式 JSON merge。传统 Chart-local Template 的迁移规则归 Chart v5，
不能把它的 patch 规则自动带入 CXT v2。Prototype 不声明 entity ID 或 parent。

### 4.1 Requirement

候选字段：

```text
Requirement {
  id: Prototype-local stable ID
  kind: registered requirement kind
  interval: {
    kind: point | half-open-range
    startBeat: ValueSource
    endBeat: ValueSource, range only
  }
  judgementDomain: literal typed reference
  requiredAction: literal typed reference
  constraints: registered typed constraint records[]
  effects: finite typed effect references[]
}
```

requirement kind 和 interval kind 独立。Foundation 示例只登记 `tap + point` 与
候选离散 lane constraint；不是最终完整判定 Schema。`point` 保存名义目标 Beat，
不是零宽容差窗；时间容差仍属于判定 profile。range 必须 end>start，但具有这种
表达能力不代表 Foundation 已能判定 Hold/Release。

一个 Prototype 可以有多个不同 localId 的 Requirement，最终 identity 为
`(entityIdentity, requirementLocalId)`。Requirement 数量必须独立计数；它不是视觉
Component，也不因移除 Transform/Renderable 而消失。

constraint kind/domain/action/effect target 必须 literal；已登记的 constraint 数值
可使用 Slot。Foundation effects 必须为空，未支持的 kind 稳定拒绝，不保存任意 JSON。

## 5. Instance 与 Pattern

### 5.1 Emit

一次 emit 是一个 Prototype Instance，生成一个具体实体：

```json
{
  "op": "emit",
  "nodeId": "note",
  "prototype": "tap",
  "bindings": [
    {
      "slot": "beat",
      "source": { "kind": "literal", "value": { "numerator": 0, "denominator": 1 } }
    },
    { "slot": "lane", "source": { "kind": "literal", "value": 0 } }
  ],
  "parent": { "kind": "invocation-parent" }
}
```

上述为引用 `tap` Prototype 的节点片段，不是完整模块。bindings 的 slot ID 唯一且
必须已声明，所有 required slot 必须绑定。不接受 `overrides`、新增 Component、
变更 requirement kind、动态引用或动态 parent。

ParentSource 只有：

```text
{ kind: "root" }
{ kind: "invocation-parent" }
{ kind: "emission", nodeId: "static-peer-id" }
```

emission parent 只引用同一个 nodes/body 数组中的 emit sibling，并使用相同的祖先
Repeat index assignments。允许前向引用，不以遍历顺序决定能否解析；禁止跨层、
自引用和循环。invocation-parent 来自 Chart 的显式 parent 引用或 null。
拓扑检查在整个 invocation 的 identity 集合建立后进行。

### 5.2 Repeat grammar

```text
Pattern ::= { id, nodes: Node[], extensions: {} }
Node ::= Emit | Repeat
Repeat ::= {
  op: "repeat",
  nodeId: stable ID,
  count: literal integer | integer parameter,
  index: stable ID,
  body: nonempty Node[]
}
```

Pattern nodes 和 Repeat body 均非空，每个数组内 nodeId 唯一；`__direct__` 保留，
作者不能使用。count 允许 0，零次不会生成实体，但其中定义、引用和资源仍要校验。
index 是 `0..count-1` 的只读整数，lexical scope 限于 body；Repeat 深度/展开总数
受独立预算约束。

没有 Pattern call 节点，没有递归、if、while、随机、上一个 emission 的值或任意
表达式。数组输入顺序不具语义，展开器按 nodeId canonical bytes 遍历；
同时刻冲突由 typed semantic validator 处理，不能以“后写覆盖先写”解决。

## 6. Chart v5 调用合同

以下是 Chart v5 新的**候选 Core 集成字段片段**，不是完整合法 Chart：

```json
{
  "cxtImports": [
    { "id": "pattern.stair", "source": "templates/stair.cxt" }
  ],
  "cxtInstances": [
    {
      "bindingId": "intro-stair",
      "module": "pattern.stair",
      "export": { "kind": "pattern", "id": "stair" },
      "startBeat": { "numerator": 16, "denominator": 1 },
      "parent": null,
      "parameters": [
        { "id": "groups", "value": 4 }
      ],
      "slotBindings": []
    }
  ]
}
```

import 的 ID/source 均唯一，source 遵守 project-relative portable path 和小写 `.cxt`
规则，并与 CXT moduleId 匹配。CXT 不伪装成 Asset record。bindingId 在整个 Chart 内
唯一；module/export 必须解析且类型一致。未绑定 Parameter 使用 default，未知参数、
重复参数和错误类型拒绝。parent 只允许显式 Chart object reference 或 null。

`startBeat` 是 rational literal，作为统一 invocation 时间平移，**只在生成
Requirement start/end Beat 时加一次**；不平移资源、Transform 或全局 Timing。
Prototype/Pattern 的 Beat 都是 invocation-local，负值可用但须符合最终 Chart 范围。
duration 值若未来开放不加 startBeat，必须注册为不同的时间单位。

Core cxtInstances 的 export kind 只接受 prototype/pattern。prototype invocation
生成一个实体，`slotBindings` 使用 `{ "slot": "...", "value": typed literal }`；
pattern invocation 的 slotBindings 必须为空，Pattern 自己完成 emit bindings。
这是一套新字段，不把 v4 `animationTemplateImports` 或 Animator Template Binding
误用为实体生成入口。Animation 模块依旧由版本化 Animator binding 集成，详见第 9 节。

生成实体与显式实体最终合并到同一 Canonical Semantic Chart；不新增作者层 notes
数组。首版不允许 Chart 字段用字符串猜测 generated ID 并跨 invocation 绑定它；
这种目标选择能力需要独立 typed reference 合同。

## 7. 生成身份和确定性

每个生成实体的语义身份：

```text
(chartId, bindingId, moduleId, exportId,
 [(nodeId, iterationIndexPlusOne), ...])
```

Repeat step 保存 index+1；普通 emit step 保存 0，路径从外向内排列。例：

```text
(chartId, intro-stair, pattern.stair, stair,
 [(groups, 1), (n2, 0)])
```

这是第一个 group 中的 n2，不是字符串 hash。直接 Prototype invocation 使用唯一
合成路径 `[("__direct__", 0)]`。显式 Chart object 继续使用原 UUID 身份，
generated tuple 有独立类型 tag，不假装 UUID，也不引入运行时 Entity handle。
tuple 的字节编码与顺序见 [Packed IDN0](PACKED_CHART_FORMAT.md)。

修改 Beat/lane 参数不改变同一路径 emission 的 identity；修改 count 只增加/移除相应
index 的实体，不重编号已有路径。nodeId/bindingId 的改名会改变身份，不能称为无影响
重构。Pattern/组件/参数数组的重排不改变 identity。

区分以下四类身份：

```text
source/build identity      source canonical bytes、参数配置、compiler profile
expanded semantic identity 已生成实体、要求、时序与声明资源闭包
Packed artifact identity   完整发行文件 bytes
prepared/replay identity   semantic + 实际资源内容 + 输入/判定配置
```

未使用参数或 inspection metadata 的变化可以改变 source/build identity，不应单凭
这种变化就改变 expanded semantic identity。参数改变实体数量、Beat、值或闭包时，
当然会改变展开语义。v4 已有 PreparedSemanticIdentity 规则保持不变，v5 组合合同
由 Stage 7A/8 独立版本化。

## 8. 完整示例：四车道阶梯

本例是语法完整的候选 CXT 模块，但**不是现有 Reader 可加载的生产 fixture**。
默认 groups=4，每组四个 tap，lane 固定 0..3；每组一拍，组内间隔四分之一拍。
Transform 只是可移除的表现位置，lane constraint 才描述要求。

```json
{
  "format": "cuexis.animation-template",
  "version": 2,
  "moduleId": "pattern.stair",
  "moduleKind": "pattern",
  "metadata": {},
  "parameters": [
    { "id": "groups", "type": "integer", "default": 4, "minimum": 1, "maximum": 10000 }
  ],
  "prototypes": [
    {
      "id": "tap",
      "slots": [
        { "id": "beat", "type": "beat", "required": true },
        { "id": "lane", "type": "integer", "required": true, "minimum": 0, "maximum": 3 }
      ],
      "components": [
        {
          "type": "cuexis.transform",
          "version": 1,
          "fields": [
            {
              "path": "position[0]",
              "source": {
                "kind": "affine",
                "input": { "kind": "slot", "id": "lane" },
                "scale": { "kind": "literal", "value": 1.0 },
                "offset": { "kind": "literal", "value": 0.0 }
              }
            }
          ]
        }
      ],
      "requirements": [
        {
          "id": "hit",
          "kind": "tap",
          "interval": {
            "kind": "point",
            "startBeat": { "kind": "slot", "id": "beat" }
          },
          "judgementDomain": {
            "kind": "literal",
            "value": { "domain": "judgement-domain", "id": "candidate.lanes4" }
          },
          "requiredAction": {
            "kind": "literal",
            "value": { "domain": "action", "id": "press" }
          },
          "constraints": [
            { "kind": "lane", "value": { "kind": "slot", "id": "lane" } }
          ],
          "effects": []
        }
      ],
      "extensions": {}
    }
  ],
  "patterns": [
    {
      "id": "stair",
      "nodes": [
        {
          "op": "repeat",
          "nodeId": "groups",
          "count": { "kind": "parameter", "id": "groups" },
          "index": "g",
          "body": [
            {
              "op": "emit",
              "nodeId": "n0",
              "prototype": "tap",
              "bindings": [
                {
                  "slot": "beat",
                  "source": {
                    "kind": "affine",
                    "input": { "kind": "index", "id": "g" },
                    "scale": { "kind": "literal", "value": { "numerator": 1, "denominator": 1 } },
                    "offset": { "kind": "literal", "value": { "numerator": 0, "denominator": 1 } }
                  }
                },
                { "slot": "lane", "source": { "kind": "literal", "value": 0 } }
              ],
              "parent": { "kind": "invocation-parent" }
            },
            {
              "op": "emit",
              "nodeId": "n1",
              "prototype": "tap",
              "bindings": [
                {
                  "slot": "beat",
                  "source": {
                    "kind": "affine",
                    "input": { "kind": "index", "id": "g" },
                    "scale": { "kind": "literal", "value": { "numerator": 1, "denominator": 1 } },
                    "offset": { "kind": "literal", "value": { "numerator": 1, "denominator": 4 } }
                  }
                },
                { "slot": "lane", "source": { "kind": "literal", "value": 1 } }
              ],
              "parent": { "kind": "invocation-parent" }
            },
            {
              "op": "emit",
              "nodeId": "n2",
              "prototype": "tap",
              "bindings": [
                {
                  "slot": "beat",
                  "source": {
                    "kind": "affine",
                    "input": { "kind": "index", "id": "g" },
                    "scale": { "kind": "literal", "value": { "numerator": 1, "denominator": 1 } },
                    "offset": { "kind": "literal", "value": { "numerator": 1, "denominator": 2 } }
                  }
                },
                { "slot": "lane", "source": { "kind": "literal", "value": 2 } }
              ],
              "parent": { "kind": "invocation-parent" }
            },
            {
              "op": "emit",
              "nodeId": "n3",
              "prototype": "tap",
              "bindings": [
                {
                  "slot": "beat",
                  "source": {
                    "kind": "affine",
                    "input": { "kind": "index", "id": "g" },
                    "scale": { "kind": "literal", "value": { "numerator": 1, "denominator": 1 } },
                    "offset": { "kind": "literal", "value": { "numerator": 3, "denominator": 4 } }
                  }
                },
                { "slot": "lane", "source": { "kind": "literal", "value": 3 } }
              ],
              "parent": { "kind": "invocation-parent" }
            }
          ]
        }
      ],
      "extensions": {}
    }
  ],
  "animations": [],
  "exports": [ { "kind": "pattern", "id": "stair" } ],
  "requiredExtensions": [],
  "extensions": {}
}
```

此候选 fixture 要求显式测试 profile 登记 `candidate.lanes4`（lane 0..3）和 `press`；
没有该 profile 的 Reader 必须拒绝，不能按名字猜测真实键盘键位。它验证数据展开，
不提供 Perfect/Good/Miss 判定规则。

使用第 6 节 startBeat=16 的 invocation，得到：

```text
semanticEntityCount = 4 * 4 = 16
requirementCount = 16
eventCount = 0
g = 0..3, lane = 0..3
absoluteBeat = 16 + g + lane/4
first/last target = 16 / (79/4)
```

其中一个具体实体的逻辑输出（不是新文件格式）：

```text
identity = (chartId, intro-stair, pattern.stair, stair, [(groups,1),(n2,0)])
parent = none
components.Transform = position(2,0,0), rotation(0,0,0,1), scale(1,1,1)
requirements = [
  { localId: hit, kind: tap, interval: point(33/2),
    domain: candidate.lanes4, action: press, constraints: lane(2), effects: [] }
]
```

将 groups 改为 10000 会生成 40,000 实体/要求；仍须通过独立内存、展开时间和
Packed 16 MiB 实测门禁，不能把 source 仍很小当作通过证据。

## 9. Animation Extension

`moduleKind:animation` 的唯一 export 指向 animations 中的定义：

```text
Animation {
  id
  application: { coordinateSpace, blendMode, iterations, fillMode }
  clip: { version: 2, durationBeats, tracks, stepTracks }
}
```

Clip 至少一个合法 track/stepTrack，局部时长为正 RationalBeat。application 保持
CXT v1 的 local coordinateSpace、override/additive 和 iterations/fillMode 合同。
iterations 的 `"infinite"` 是固定 Clip 的绝对时间循环采样，不是无限 CXT 实体生成；
不能把它当作 Repeat count，也不能在 prepare 无限复制事件。

Track/Segment/Step 内部时间、值和引用全部 literal；透明度改为 v5 `render.alpha`
整数 `[0,255]`，Quaternion、Hermite、混合和离散权重边界不重开。参数化
durationScale/weight 属于 Chart Animator Binding，不属于 CXT Clip 内部。

Core 与 Animation 分别校验/计数。Core 不直接调用 Animation，也不向新生成实体
隐式添加 Animator。Stage 6 如需把实体生成和动画绑定组合，必须先定义显式
generated-target binding 合同、候选版本和 golden；不能由 reader 猜测对象名称。
完整 Animator export/import 映射及其 ANM0 encoding 归 Stage 8 收敛，混合语义见
[ANIMATION_MIXING.md](ANIMATION_MIXING.md)。

## 10. 展开算法与资源闭包

```text
typed read and module/export validation
  -> index local declarations / validate source grammar
  -> resolve literal invocation parameters
  -> static checked count and budget preflight
  -> expand Repeat scopes / bind Prototype slots
  -> materialize registered Component defaults and Requirement values
  -> add invocation startBeat once
  -> assign stable entity/requirement identities
  -> resolve complete parent graph
  -> validate concrete values, conflicts, resource closure and actual counters
  -> publish all-or-nothing Canonical Semantic Chart
  -> optional Packed compile
```

固定结构允许在分配前用 checked sum/product 估算 emission 数和 Requirement 数；
实际展开后必须再次计数。count=0、未使用声明、零权重动画中的资源仍按源语义闭包
验证，不能借有限生成绕过缺失资源检查。

字符串键重复、Component/字段/槽位重复、Requirement localId 重复均失败，不覆盖。
没有前一个实体的默认状态。任一错误不得发布部分展开结果；外层会话的旧 active
状态按既有事务合同保留，但不能冒充本次成功。

Canonical model 的具体逻辑结构和 identity 字节规则由
[PACKED_CHART_FORMAT.md](PACKED_CHART_FORMAT.md) 定义。必须验证：

```text
expand(source A reordered) == expand(source A)
decode(pack(expand(cxt))) == expand(cxt)
```

模型比较包含实际实体/要求身份、parent、Component presence/value、Beat、domain/action、
constraints 和资源需求，不比较 source JSON 排版。

## 11. 预算、失败与验收

必需预算：module bytes、JSON depth、module/parameter/slot/Prototype/Pattern 数量、
Repeat depth、AST nodes、实体/要求/事件数量、decoded bytes、peak prepare memory。
编译时间须单独测量；工作量限额与外层取消策略不能依赖随机帧率。

沿用范围内的 CXT v1 module bytes/JSON depth 等现有限制不隐式放宽；Core 新的展开
预算由 Foundation CFF-D 明确数值并经接受后启用。40k/16 MiB 是发行容量目标，不是
允许任意 40k 实体附带无限动画的承诺，详见 Packed capacity profile。

| 正反例 | 期望 |
| --- | --- |
| 完整四车道阶梯默认参数 | 16 entities / 16 requirements，lane 不越界 |
| groups=10000 | 展开计数 40000，随后独立进行全部容量门禁 |
| 重排模块/节点/参数数组 | 同 expanded semantics 与 identity |
| 修改起始 Beat | identity 路径不变，要求 Beat 和 semanticIdentity 改变 |
| 缺 required slot / 重复 binding | 稳定失败 |
| lane=4，用于 lanes4 | 稳定失败，不截断到 lane=3 |
| count=0 | 不生成实体，仍验证定义和资源闭包 |
| count 溢出 / 超 budget | 分配前失败 |
| parent sibling 前向引用 | 完整图解析，若无环则允许 |
| parent 自引用/跨 scope | 稳定失败 |
| reference 参数/任意 override/递归调用 | 语法或用途门禁拒绝 |
| alpha/Quaternion/CXT Clip 值参数化 | 拒绝，不借 number/vector 绕过 |
| 未注册 gameplay/constraint/effect profile | 拒绝，不降低为普通 tap |

诊断至少区分 module/export、parameter type/range、slot missing、ValueSource use、
Prototype field、Pattern count/scope、parent reference、identity conflict、unsupported
kind 和 budget exceeded。保留 moduleId、bindingId、node path、字段路径；不输出本机
绝对路径。不允许脚本、字节码、宿主回调、运行时随机、输入流或 Judgement 状态读取。

## 12. 分阶段冻结

| 阶段 | 交付 |
| --- | --- |
| Foundation | 本文 Core grammar、Chart invocation、候选 tap/point 展开、稳定 identity、正反例和容量原型 |
| Stage 6 | 显式 candidate source compile/Packed consume；仅使用已接受 subset，必要动画组合先补候选合同 |
| Stage 7A | 最小 Input/Judgement/Replay 规则，替代候选演示 profile |
| Stage 8 | Chart v5 正式 Schema、CXT Core 与 Animation Extension、Packed/CXC 发行及迁移 |

Stage 7B+ 高级判定类型单独按 version/capability 演进，不阻塞全部 v5 发行。
未冻结的数值预算、生成实体动画目标引用、完整 animation integration 仍是显式实施
门禁，不能因为本文列了名字就宣称生产支持。

CXT source 可作为 CXC 显式 source entry 保留，不是 v5 playback entry；发行入口必须
是验证后的 Packed，展开参数和 source/build provenance 可通过 manifest 关联。
运行时脚本和逐帧回调无限期延后，不预留隐藏入口。未来天空盒、模型和复杂形变先
有独立的 Presentation typed contract，再讨论声明式复用，不进入自由表达式。
