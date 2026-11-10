# Gameplay V2 格式边界与谱面语义图提案

状态：candidate research；未接受、未实施

日期：2026-10-02

本文是 Gameplay V2 重设计的格式边界研究稿。它不修改现有 Chart v4、CXT v1、CXT v2、
CXC v1 或 Packed Chart 合同，也不表示任何新的 Reader、Writer、Playback 路径或 SDK API
已经存在。本文的目的，是在继续冻结 Gameplay Judgement 语义之前，重新审查谱面格式与
判定系统之间的耦合，并给出可以承载后续增量开发的候选分层。

现有格式合同仍以 [Chart v4](../../../formats/CHART_V4_FORMAT.md)、[CXT v1](../../../formats/CXT_FORMAT.md)、
[CXT v2 candidate](../../../formats/CXT_V2_FORMAT.md) 与 [CXC v1](../../../formats/CXC_FORMAT.md) 为准；
Gameplay 语义的权威**已不是** [Gameplay Judgement Spec](../../../formats/GAMEPLAY_JUDGEMENT_SPEC.md) 与
[ADR 0043](../../../adr/0043-gameplay-judgement-ruleset-convergence.md)（两者已于 2026-10-02 标为 `superseded`），
而是 [Gameplay V2 Spec](../../../formats/GAMEPLAY_V2_SPEC.md) 与 [ADR 0044](../../../adr/0044-gameplay-v2-semantic-kernel.md)。

> **版本取代声明（2026-10-02，D-4）**：本文 §5.1 与 §6 的 `"gameplay": { "version": 1 }` 是历史草案值，
> 已被 [Gameplay V2 Spec](../../../formats/GAMEPLAY_V2_SPEC.md) §2 的版本合同取代 —— 现行层级是外层
> `version = 5` + `gameplay.version = 2`，旧值 `1` 只能显式迁移或在最早可判定入口稳定拒绝。本文其余内容
> 仍作为 `revise` 级研究输入保留，**不整篇 supersede**（依据 RULING_WORKSHEET 第 1 轮 D-4 裁决）。
> §10 的 `"version": 1` 是 entry / wire 级物理 revision，与 `gameplay.version` 不是同一个字段，故不在本次标注范围内。

## 1. 评审结论

当前格式链条已经具备清晰的作者层、准备阶段和发行包边界，但 Gameplay 语义仍有四处
结构性耦合：

1. Chart v4 把 Object、Component、Animation、Behavior 和未来的 Requirement 放在同一个
   谱面语境中。`cuexis.transform` 既可能是显示位置，也可能被作者误认为判定位置；这使
   “删掉视觉对象是否会删掉音符”无法由格式本身机械判断。
2. CXT v2 的 `moduleKind=prototype|pattern|animation` 让实体生成、Requirement 生成和动画
   模板共享一个 `cuexis.animation-template` 命名空间。它可以表达很多内容，但不能保证
   判定闭包、表现闭包和资源闭包互相独立。
3. Gameplay Judgement 的 Requirement、Pattern、Measure、Ruleset 和 Replay identity 已经
   形成一套独立语义，却没有相应的独立发行 entry。结果是作者源、编译后语义和 CXC 物理
   编码容易被当成同一种内容。
4. 现有 `PreparedSemanticIdentity` 适合 prepare、资源闭包和 CXC 内容身份，但不适合作为
   Replay identity。只要把贴图、材质或动画绑定混进同一摘要，视觉改动就会无谓地失效旧
   Replay；如果反过来把判定域或窗口排除，又会造成静默分叉。

因此 V2 的核心改造不是再增加一个 `notes` 数组，而是增加一个独立的、可验证的
**Canonical Gameplay Graph**，并让 Chart 只负责把作者源和表现源显式关联到这张语义图。

## 2. 目标与非目标

### 2.1 目标

V2 格式边界应满足以下性质：

```text
作者源可读、可重排、可保留元数据
Gameplay 语义可在不加载渲染器的情况下完整验证
表现对象可以缺失、复用或一对多映射而不改变判定要求
Ruleset 可以独立版本化、分发和复用
Packed entry 只改变物理编码，不改变语义
Replay 只绑定判定所消费的内容与规范化输入
每一次 source -> semantic -> packed 转换都有可比较的 identity
未知能力、悬空引用、无界展开和歧义迁移都稳定失败
```

### 2.2 非目标

本文不提出以下能力：

```text
运行时脚本、Lua/JavaScript/任意字节码或逐帧宿主回调
Playback 在运行时解析 CXT、展开 Pattern 或扫描 SDK/网络模板
以 Object parent/tree 代替 gameplay relation
由渲染像素、材质、动画或相机状态隐式产生 Requirement
把输入设备映射配置写入谱面生成模块
为延期能力预留未定义字段、extension、capability 或执行入口
```

这些能力若未来需要，必须通过新的语义模块、独立 ADR、版本和安全预算进入系统；不能借
助“扩展字段”绕过当前边界。

## 3. 当前设计的结构性问题

### 3.1 CXT v2 的模块职责过宽

CXT v2 同时允许 Prototype、Pattern 和 Animation。Prototype 又可以同时拥有
Components 与 Requirements，Requirement 还带有 `effects`。这使一个文件既可能是：

```text
表现模板       Object/Component/Animation 的复用单元
谱面生成器     Repeat/Emit 产生实体和 Requirement
判定声明       domain/action/pattern/measure 的来源
有限副作用     命中后可能关联表现或音频的入口
```

这些职责的资源闭包和 identity 不相同。判定应该依赖 domain、action、时间、约束和 Ruleset；
表现应该依赖对象、材质、动画和音频；有限反馈应该依赖 Fact 与 Effect Schedule。将它们放在
同一实体定义中并非无法实现，但会迫使每一次扩展同时回答三套兼容问题，长期会堵塞增量开发。

### 3.2 Object parent 不是 gameplay relation

Object parent 描述表现树的层级。`sameContact`、chord、sequence、chain、两手交替和
“同一输入资源不可被两条要求同时消费”描述的是判定关系。两者可能恰好有相同的树状图，
但它们的生命周期、排序和 identity 不相同。复用 parent 或数组顺序会导致以下问题：

```text
移动一个视觉父节点，意外改变判定关系
重排作者数组，改变仲裁 tie-break
删除装饰对象，误删拥有 Requirement 的实体
一个视觉对象无法承载多个独立 Requirement
```

V2 必须将 gameplay relation 与 presentation tree 分开。

### 3.3 “表现位置就是判定位置”不可靠

有些音游的判定区域可以由屏幕位置、轨道、相机变换或设备朝向决定；另一些音游使用完全
不可见的键位或控制器通道。格式不能从 `transform.position`、渲染层级或材质名称猜出
`judgementDomain`。如果旧 Chart 没有明确判定域，迁移必须报告 ambiguity，而不是自动把
表现几何提升为判定几何。

### 3.4 现有 identity 的职责不同

当前至少存在四种需要区分的身份：

| 身份 | 作用 | 应否绑定 Replay |
| --- | --- | --- |
| source/build identity | 作者 bytes、参数、编译器 profile | 否 |
| semantic identity | 展开后的完整语义图和声明闭包 | 作为 chart 分量 |
| artifact identity | Packed/CXC 的完整物理 bytes | 否 |
| judgement identity | engine、ruleset、chart、session 的判定投影 | 是 |

把四者压成一个 hash 会让诊断和兼容策略失去粒度。V2 保留域分隔摘要，但要求每个 entry
和 Replay 头部都能看到命名分量。

## 4. Chart v5 作为 V2 的格式基准

本路线已固定：Chart v5 外层保持 `version = 5`，Gameplay 语义使用 `gameplay.version = 2`。
Canonical Gameplay Graph 是唯一播放语义来源；Packed Chart 是其物理投影，CXC v1 允许独立
`gameplay-graph` Playback entry。该 entry 与 `packed-chart` entry 必须共享 prepared identity、
Judgement kernel、Ruleset、Snapshot、Replay 和 Presentation path。

V2 不另起一套 Stage 7 谱面格式。Chart v5 是 Gameplay 的聚合入口，CXT v2 是其作者层模板与有限生成输入，Packed Chart 是其物理发行编码。V2 修改的是 Chart v5 的语义模型和扩展字段，不改变 “Chart v5 source -> validated semantic -> Packed” 这条主链。

```text
Chart v5 Author Source
  ├─ cuexis.chart v5           聚合清单、时间轴、显式 invocation 和 Gameplay 关联
  └─ CXT v2 imports             Prototype/Pattern/Animation 的作者层模板
        |
        | explicit validate + finite expansion + typed lowering
        v
Canonical Semantic Graph
  ├─ Gameplay Graph             Requirement、Pattern、Measure、Relation
  ├─ Presentation Graph         Object、Component、Animation、resource refs
  └─ Effect Graph               FactBinding、有限反馈和调度
        |
        | semantic equivalence + budgets + closure checks
        v
Judgement / Presentation Packages
  ├─ cuexis.ruleset              L3 Fold、Interface、table、module、defaults
  └─ cuexis.presentation-pack    可选 L4 资源/表现能力集合
        |
        | explicit pack
        v
CXC / Packed Distribution
  ├─ Chart v5 source entries    审查、迁移和复现
  ├─ semantic entries           Playback 可直接消费
  └─ packed entries             发行物理编码
```

这些是语义层，不要求未来增加新的顶层文件扩展名；关键要求是 Chart v5 内的 source、semantic、presentation 和 packed entry kind 必须可机械区分。

### 4.1 Author Source

作者源保存可读结构、模板参数、源映射和元数据。数组顺序、注释、作者名和 Studio 编辑
信息可以变化，而不应改变语义 identity。作者源允许有限生成，但生成必须是有界、纯的、
显式调用的，并在 prepare 或独立 compile 阶段结束。

### 4.2 Canonical Gameplay Graph

这是 Gameplay 的唯一语义来源。它不含 JSON DOM、CXT AST、未解析参数、隐式默认的表现
父节点或运行时 Entity handle。图中至少包含：

```text
RequirementId
时间锚点、区间和量化时间域
JudgementDomain / Action 的稳定引用
Pattern 的已验证表示或其规范化编译结果
Measure、Grading 和 prepared grace
资源归属、sameContact、sequence、choice 等 gameplay relation
Ruleset/Interface 所需的声明引用
有限的 source map（供诊断，不参与 judgement identity）
```

Canonical Graph 可以保存作者层 Pattern，也可以保存编译后的有限自动机；但 Playback entry
必须只接收已验证、可计数、可确定求值的表示。不要在运行期保留“以后再解释”的字符串表达式。

### 4.3 Presentation Graph

Presentation Graph 保存 Object、Component、Animation、Material、Camera、Audio 和表现资源
引用。它可以通过 `requirementRef` 指向一个或多个 Requirement，但不能创建、删除或改写
Requirement。允许以下映射：

```text
一个 Requirement -> 零个表现对象       可见性不是判定前提
一个 Requirement -> 多个表现对象       例如轨道、尾巴和命中标记
多个 Requirement -> 一个表现对象       例如共享一条滑条或键位
装饰对象 -> 没有 Requirement           纯 Presentation
```

表现树的 parent、layer 和 animation binding 只在这一层有意义。判定层若需要“同一资源”或
“同一接触点”，必须使用独立 Relation，而不能引用 Object parent。

### 4.4 Effect Graph

判定核心只产生 Fact；命中音效、闪烁、粒子、连击文本等属于有限反馈。Effect Graph 以
`FactBinding` 或受限事件触发为入口，引用 Presentation target 和资源。它不能回写 Requirement、
改变已经产生的 Fact，不能调用宿主代码，也不能在运行期生成新 Requirement。

将 `effects` 从 Requirement 主体移出，避免音频/贴图改动污染 judgement closure 和 Replay。

## 5. Chart v5 Gameplay Semantic 扩展

这是对现有 Chart v5 定义的增量修订。Stage 7 不新增平行的 `cuexis.gameplay-module` 发行入口；Gameplay 的聚合、跨 Requirement 关系和 Canonical Graph 归属于 Chart v5。CXT v2 可以继续提供局部 Prototype/Pattern/Requirement，但其展开结果必须进入 Chart v5 的 `gameplay` 语义区。

### 5.1 Chart v5 顶层形状

> **版本取代（2026-10-02，D-4）**：下列 JSON 中的 `gameplay.version = 1` 为历史草案值，已被
> [Gameplay V2 Spec](../../../formats/GAMEPLAY_V2_SPEC.md) §2 的版本合同取代（现行为 `gameplay.version = 2`，
> 外层 `version = 5`）。JSON 原文保留不改。

```json
{
  "gameplay": {
    "version": 1,
    "requirements": [],
    "coordination": { "resources": [], "groups": [], "relations": [] },
    "extensions": {}
  }
}
```

`requirements` 可以来自显式 Chart v5 records，也可以来自已冻结的 CXT v2 invocation；两者在 compile 后必须得到同一类 typed Requirement。`coordination` 只允许引用当前 Chart v5 canonical RequirementId，不得在运行期扫描对象树或查找名称。

### 5.2 CXT v2 输出边界

```text
GameplayEmission {
  source: cxt-v2 | chart-v5-inline
  localId
  requirements
  localRelations
}
```

每个 Requirement 的 `localId` 只在 CXT v2 Prototype/Pattern 内稳定；展开到 Chart v5 后 identity 使用完整 invocation/emission path。
Requirement 的域、动作、Pattern、Measure、Grading 和约束引用必须是 typed field。资源引用
只允许指向 judgement closure 中登记的 domain、action、geometry 或表，不允许任意 AssetId。

### 5.3 Chart v5 coordination 输出

Chart v5 `gameplay.coordination` 只允许有限、可静态计数的结构，例如：

```text
exclusive(resource, members, capacity)
binding(group, members, cardinality, atomicity)
temporal(before | after | within, left, right, tolerance)
quota(window, members, min, max, policy)
```

不支持递归、while、随机、运行时表达式、隐式扫描、跨模块动态调用或由输入结果生成新
Requirement。所有 CXT Repeat count、参数范围、Requirement 数、关系数和 Coordination solver 上界都要在 prepare 前可计算。

### 5.4 与 CXT v2 的关系

建议的 V2 关系为：

```text
Chart v5 + CXT v2 -> Canonical Gameplay Graph
CXT v2 Animation  -> Presentation Graph
Chart v5 gameplay -> Coordination Graph 与判定闭包
```

Chart v5 仍拒绝 CXT v1；CXT v2 的 Gameplay 部分不直接成为 Playback AST。若未来需要独立
复用 Gameplay 模块，可以作为 Chart v5 source entry 的一种来源，但不能绕过 Chart v5
canonical graph 或形成第二种 judgement contract。独立 `gameplay-graph` entry 只是同一
Canonical Graph 的另一种物理入口，不是另一套 Playback 语义。

## 6. 修订后的 Chart v5 聚合清单

> **版本取代（2026-10-02，D-4）**：本节 JSON 中的 `gameplay.version = 1` 为历史草案值，已被
> [Gameplay V2 Spec](../../../formats/GAMEPLAY_V2_SPEC.md) §2 的版本合同取代（现行为 `gameplay.version = 2`）。
> JSON 原文保留不改。

建议在现有 Chart v5 顶层增加 Gameplay 区段：

```json
{
  "format": "cuexis.chart",
  "version": 5,
  "chartId": "song.example.expert",
  "timing": { "source": "timing/main.json" },
  "cxtImports": [
    { "id": "pattern.stair", "source": "templates/stair.cxt" }
  ],
  "cxtInstances": [
    {
      "invocationId": "intro-stair",
      "module": "pattern.stair",
      "export": { "kind": "pattern", "id": "stair" },
      "startBeat": { "numerator": 16, "denominator": 1 },
      "parameters": [],
      "slotBindings": []
    }
  ],
  "gameplay": {
    "version": 1,
    "requirements": [],
    "coordination": { "resources": [], "groups": [], "relations": [] },
    "extensions": {}
  },
  "presentationImports": [],
  "presentationInstances": [],
  "effectBindings": [],
  "extensions": {}
}
```

Chart v5 不直接保存一套绕过 typed contract 的平行 `notes` 数组。显式 CXT invocation 与
`gameplay` records 经过 compile 后，所有 Gameplay emission 合并为一张 Canonical Gameplay Graph；表现 invocation
另行生成 Presentation Graph。`startBeat` 是统一的 gameplay invocation time transform，
只作用于生成 Requirement 的时间字段，不隐式平移全局 TimingMap、资源或表现动画。

### 6.1 关联规则

Presentation instance 只能使用显式的稳定引用：

```text
requirementRef     指向一个或多个 Canonical RequirementId
presentationAnchor 指向表现图中的对象/轨道锚点
effectTarget       指向表现图目标或音频资源
```

不允许用字符串前缀、数组下标、运行时 Entity ID 或当前展开顺序推断关系。无法解析的引用
必须在 prepare 失败。

### 6.2 判定域与表现几何

`judgementDomain` 是 Gameplay Graph 的稳定引用；显示位置可以引用同一 domain 作为可视化
提示，但不反向定义 domain。若一个游戏确实使用动态 frame 或区域几何进行判定，几何必须作为
Judgement Graph 的显式 typed record，并进入 chart judgement identity；相机和材质仍属于
Presentation。

## 7. Canonical Requirement identity

生成 Requirement 不使用截断 hash 字符串、运行时 Entity handle 或数组序号。候选 tuple 为：

```text
(chartEntryId,
 invocationId,
 moduleId,
 exportId,
 emissionPath,
 requirementLocalId)
```

`emissionPath` 是由稳定 `nodeId` 和 Repeat index 构成的有序路径，例如：

```text
("intro-stair", "pattern.stair", "stair",
 [("groups", 1), ("n2", 0)], "hit")
```

这里的 tuple 是 Requirement 的稳定寻址身份，不是判定语义摘要。其性质是：

```text
改变 Beat/lane/参数值：Requirement identity 不变，canonical/judgement identity 改变
改变 Repeat count：只增加或移除对应 index 的 emission
重排源数组：Requirement identity 和 canonical identity 不变
修改 nodeId、invocationId、moduleId 或 localId：Requirement identity 改变
删除 Presentation：Requirement identity 和 judgement identity 不变
```

Source map 可以记录旧字段路径、模块和 Studio 节点，但不进入 judgement identity。这样既能
支持诊断和迁移，又不会把作者排版细节误当成判定语义。

## 8. 四类闭包

V2 prepare/pack 必须分别计算和验证以下闭包：

| 闭包 | 内容 | 进入何种身份 |
| --- | --- | --- |
| author/source closure | 源 Chart、模块、参数、源映射和元数据 | source/content |
| judgement closure | domain、action、判定几何、定点表、Ruleset/Interface 引用 | semantic/judgement |
| presentation closure | Object、Component、Animation、Material、Audio、Skin | content/presentation |
| tool/compiler closure | compiler profile、schema/compiler 版本、打包器信息 | source/artifact |

Pack 不按当前是否 active、是否可见或权重是否为零裁剪声明闭包。未绑定模块、被 mask 排除
的资源和权重为零的表现仍需按各自闭包规则验证，防止不同入口对同一 source 生成不同语义。

`judgement closure` 不应包含贴图、材质和命中音效；`presentation closure` 不应偷偷携带
改变判定的窗口、域或 action。自定义类型若无法证明是纯表现，默认归入 judgement，宁可使
更多 Replay 失效，也不能让结果变化而 identity 不变。

## 9. Ruleset 与 Gameplay Graph 的绑定

Ruleset 是独立的 `cuexis.ruleset` package。Chart 只引用 Ruleset 的稳定 Interface 和需要
的 capability；它不复制 Ruleset Fold、Hook 合成、窗口表或模块实现。

Prepare 应生成以下绑定摘要：

```text
engine identity       判定阶段顺序、定点表和语义版本
ruleset identity      Interface projection、Fold/module order、Build hash
chart identity        Canonical Gameplay Graph 的 judgement projection
session identity      Loadout、默认 grace、输入重采样和 JudgementConfig
```

表现资源、源数组顺序、作者元数据和映射前的设备事件不进入 Replay judgement identity。Replay
记录规范化后的 InputEvent 流，并保存逐分量 identity 供诊断；缺失所需语义 entry 或 capability
时稳定拒绝，不降级成 Tap、旧 Pattern 或空表现。

## 10. CXC / Packed entry 建议

CXC v1 可以继续作为 ZIP32 Stored 载体，但 manifest entry 应明确区分语义角色。候选字段：

```json
{
  "path": "compiled/gameplay.packed",
  "entryKind": "gameplay-graph | packed-chart | author-source",
  "format": "cuexis.gameplay-graph | cuexis.packed-chart | cuexis.chart",
  "version": 1,
  "playback": true,
  "compiledSemanticIdentity": "...",
  "judgementCapabilities": ["..."],
  "sourceOf": ["source/gameplay.gpm"]
}
```

规则如下：

1. `author-source` 供审查、迁移和复现；Playback 不能把它当成 v2 发行入口。
2. `gameplay-graph` 是已验证的 Canonical Gameplay Graph；在明确 `playback=true` 时可作为
   独立 Playback entry。
3. `packed-chart` 是同一语义图的物理编码；必须携带可比较的 compiled semantic identity。
4. Pack、unpack、migrate 和 prepare 是不同操作；任何一个都不能隐式执行另外三个。
5. CXC 容器版本与内层 gameplay module/graph/ruleset 版本独立；新增语义模块不应通过提高
   ZIP 载体版本解决。

Packed Chart 只能改变字典、索引、delta、实体差异流和资源引用编码，不能把 Requirement
或 Relation 变成不可恢复的表现约定。Canonical Graph 与 Packed entry 必须有独立 round-trip
semantic comparison。

## 11. 迁移策略

### 11.1 v4/v5 到 Gameplay V2

迁移必须是显式工具链，而不是 Playback prepare 的隐式猜测：

```text
旧 Chart source
  -> 提取候选 gameplay records
  -> 人工或规则绑定 domain/action/geometry
  -> 写出修订后的 Chart v5 gameplay 区段或 CXT v2 invocation
  -> compile Chart v5 Canonical Gameplay Graph
  -> 生成 source map 与 migration report
```

以下情况必须输出 `ambiguous` 并停止该 Requirement 的自动迁移：

```text
只存在 render position，但没有明确 judgementDomain
同一视觉对象可能代表多个不同要求
Behavior/Animation 改变了表现但无法证明不影响判定
旧字段同时承载视觉和判定含义且没有版本化标签
Object parent 与 gameplay relation 不一致
```

迁移工具不能把 CXC pack 误称为语义迁移，也不能用 `ChartWriter` 的 v4/v5 投影伪造新的
Gameplay Graph。每条迁移记录应保存 source identity、target semantic identity、字段计数、
unsupported/ambiguous diagnostics 和人工确认点。

### 11.2 CXT v2 到 Chart v5 Gameplay 区段

CXT v2 中只含 Prototype/Pattern/Requirement 的子集可以由独立转换器提取为 Chart v5
`gameplay.requirements` 和 `gameplay.coordination`；包含 Animation、Component、Object parent
或 `effects` 的文件不能直接改名或把全部字段复制到 Gameplay 区段。转换器应：

```text
提取纯 Gameplay 字段
把 Component/Animation 保留在 Chart v5 Presentation Graph
把 effects 转为 Effect Graph binding
为两张图建立稳定 requirementRef/source map
无法分离时报告 unsupported，而不是复制所有字段到新格式
```

旧 CXT 的 canonical identity 与新 Gameplay semantic identity 不相等；转换报告必须同时记录
二者，避免把“字节兼容”误称为“判定兼容”。

## 12. 可扩展性规则

V2 新增语义必须遵循以下顺序：

1. 先确定它属于 Gameplay、Presentation、Effect、Ruleset 或 Container。
2. 为该职责增加独立 typed record、format/version、capability 和稳定拒绝诊断。
3. 定义它进入哪些闭包与 identity；默认未知自定义类型进入 judgement。
4. 定义 source -> canonical -> packed 的等价检查和预算。
5. 定义迁移、回放和 seek 的行为；无法定义时不要加入字段。

以下变化可以 additive 演进，前提是既有语义不改变：

```text
增加未被引用的 domain/action/Hook/表现类型
增加 presentation-only metadata 或新资源格式
增加 Packed entry 的压缩编码
增加 source map 字段
```

以下变化必须新版本、显式放行或新增类型：

```text
改变既有 Requirement 的时间/域/action/Pattern 含义
收窄既有字段值域或改变判定几何
重排有序 grade、改变仲裁/折叠顺序
把 presentation 字段升级为会影响判定的字段
改变 Requirement identity tuple 的编码或排序
让原本有界的生成器可以读取运行时输入或宿主回调
```

## 13. 必须建立的验证门

在任何 V2 实施前，设计阶段至少应冻结这些性质：

```text
Graph validation 无需 SDL/OpenGL/World/EnTT/JSON DOM
Source 重排、注释和 presentation-only 改动不改变 chart judgement projection
Canonical Graph round-trip 后 Requirement/Relation/closure 逐项相等
Packed decode 不执行 Pattern 或脚本
同一 InputEvent 流在不同实例枚举顺序下产生相同 Fact/Score
判定域每项数值运算有量程证明，不依赖未定义溢出
所有生成数量、Relation 数量、资源引用和快照成本可在 prepare 计数
失配诊断可以指出 engine/ruleset/chart/session 分量和 source field path
```

这些门是格式语义门，不等同于代码测试；未来实现仍需以跨编译器、跨枚举顺序和真实内容
预算证据验证它们。

## 14. 取舍与未决项

本提案明确选择：

| 议题 | V2 候选选择 | 放弃的方向 |
| --- | --- | --- |
| Gameplay 与 CXT | 修改 Chart v5 `gameplay` 区段，CXT v2 作为 source input | 继续把跨 Requirement 语义塞进 animation-template |
| 语义载体 | Canonical Gameplay Graph | Playback 运行时保留未展开 AST |
| 表现关联 | 显式 `requirementRef` | 从 Object parent/数组顺序推断 |
| 副作用 | 独立 Effect Graph | Requirement 内嵌任意 effects |
| 发行入口 | semantic/packed entry | Playback 隐式编译 author source |
| Replay | judgement identity + normalized input | 绑定完整 PreparedSemanticIdentity |
| 迁移 | 显式、可报告 ambiguity | 自动猜测 render geometry 是判定区域 |

仍需在更高层设计中确认：

1. Canonical Gameplay Graph 是独立文件格式，还是 CXC 内部 typed entry；本文只要求语义边界
   独立，不预先冻结物理扩展名。
2. Pattern 的 canonical form 是保留受限 AST、有限自动机，还是两者并存；两者都必须保证
   source map、预算和 identity 可追踪。
3. Gameplay domain 的几何是否共享现有 Chart frame/region 类型；如果共享，必须明确哪些
   字段被投影到 judgement closure，哪些仅用于 presentation。
4. Effect Graph 是否与 Behavior Event v2 共用物理编码；共享编码不能共享隐含执行语义。

这些问题不应通过在现有 Chart v4/CXT v2 增加临时字段解决。只有在 Canonical Gameplay Graph
和闭包/identity 规则稳定后，才适合编写新的生产 Schema、ABI 和 Stage 计划。


