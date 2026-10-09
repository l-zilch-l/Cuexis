# Stage 8 Implementation Plan: Chart v5 Formal Release and Semantic Convergence

状态：future；未开始

更新日期：2026-10-02

归档来源：[Chart v5 格式计划](../../active/chart-format-update-for-v5/plan.md)、
[Stage Chart Format Update 完成计划](../../completed/chart-format-update/plan.md)、
[CXT v1 格式合同](../../../formats/CXT_FORMAT.md) 和 [CXC v1 格式合同](../../../formats/CXC_FORMAT.md)。

前置：

```text
Chart Format Foundation owner acceptance
Stage 6 owner acceptance
Stage 7A JudgementRequirement / InputEvent / Replay 合同冻结
Gameplay v2 (`gameplay.version = 2`) Canonical Gameplay Graph、Release/tail、single
capacity=1 exclusive resource、early/late Presentation bridge、TimebaseProfile 和 Ruleset
fault transaction 合同冻结
Stage 7-RPA子阶段已验收的实时边界、正式诊断命名和版本兼容交接
```

详细格式工作包见 [Chart v5 format plan](../../active/chart-format-update-for-v5/plan.md)。
本阶段负责把 Foundation 和 Stage 6 已验证的 v5 candidate path 收敛为正式发行合同，
而不是首次开始设计 Chart v5。Stage 7B+ 的未选定高级判定能力不属于本阶段硬前置。

[Stage 7-RPA子阶段](../../active/stage-07/plan-rpa.md)承接实时架构与正式命名迁移，
实体设备/实时同步验收直接由RPA完成，本阶段验证其发行兼容。Stage8以前序7A和RPA交付为前置，
不等待全部7B+；规划登记不表示这些前置已完成。

## 1. 阶段目标

交付 Cuexis 的高密度谱面正式发行链：

```text
Chart v5 authoring model
  -> CXT v2 template/pattern expansion
  -> Canonical Gameplay Graph (`gameplay.version = 2`)
  -> Packed Chart or independent Gameplay Graph Playback entry
  -> CXC v1 playback entry
  -> Chart v5 Playback / Stage 7A Judgement
```

硬性目标：

```text
40,000 个语义实体
Packed Chart <= 16 MiB
JSON / CXT / Packed / Runtime 语义等价
```

16 MiB 默认按 `16 * 1024 * 1024` bytes 计算。若产品所有者选择十进制限制，
必须在 ADR 中明确替换，不能在工具和文档中混用。

## 2. 格式职责

```text
Chart v5
  描述要求、判定域、动作、时间区间、Gameplay v2 phases/resources 和有限效果来源

CXT v2
  提供模板、Prototype、Instance、Pattern、参数和有限确定性展开

Packed Chart
  提供紧凑、确定性的物理编码

CXC v1
  提供发行容器、资源闭包、entry、identity 和校验
```

CXT v2 不执行任意代码；Packed Chart 不改变语义；CXC 不重新定义 Chart。

## 3. 工作批次

### S8-A：Chart v5 语义合同和发行矩阵

冻结：

```text
JudgementRequirement
timeInterval
judgementDomain
requiredAction
Tap / Hold / Release
gameplay.version = 2
Canonical Gameplay Graph identity
Release/tail as a phase of the same Requirement
single capacity=1 exclusive resource lifecycle
TimebaseProfile and causal Fact order
Ruleset transaction failure -> faulted session behavior
early/exact/late/Miss FactBinding presentation bridge
仅限本阶段发行矩阵明确选入的持续、释放、组合和顺序约束 capability；未选入的 7B+ 能力
必须继续走稳定拒绝路径，不得因为出现在候选格式字段中而获得默认支持
有限 Effect Schedule
  Chart v5 与 Stage 7A Judgement 的 typed 边界
  本次发行纳入的 RequirementKind / capability 矩阵
```

Chart v5 不绑定轨道、Note 模型或渲染后端。v4 到 v5 的迁移必须显式执行，
不能由 Playback 隐式升级。
Chart v5 outer `version = 5` 与 `gameplay.version = 2` 是不同版本层；不得以字段存在与否猜测
Gameplay semantics。Stage 8 只发行 V2 graph；旧 Gameplay revision 必须显式迁移或稳定拒绝。
Stage 7B+ 尚未纳入发行矩阵的 RequirementKind 必须保留稳定拒绝路径，不能为了关闭
Stage 8 而伪装成已支持能力。

### S8-B：CXT v2 Core

CXT v2 保留 `cuexis.animation-template` 顶层 format，但不再限制为动画模板。
一个 CXT 文件仍然是一个明确的 module，module 必须声明 export kind。字段合同见
[CXT_V2_FORMAT.md](../../../formats/CXT_V2_FORMAT.md)：

```text
animation
prototype
pattern
```

Core 最小构造：

```text
Prototype slots
Instance bindings
Parameter freeze
ValueSource
Pattern
Finite Repeat
Finite Expansion
```

参数类型限定为：

```text
integer
number
rational
beat
boolean
enum
vector
```

参数不得改变对象类型、Component 集合、父子层级、AssetId、引用目标、资源闭包
或固定 AST/Component 数组结构。Repeat count 可以改变展开数量，但须独立 checked
计数；不提供 reference 参数。Core 的 Beat/lane/position/scale 来源按 Spec 白名单，
rotation、alpha 和 Animation Track 值保持 literal。Pattern 只能生成有限、已注册的
Chart 语义记录。

### S8-C：CXT v2 Animation Extension

继续支持：

```text
AnimationClip
Track
Segment
Step
Animator
Layer
BlendGroup
```

动画扩展与 Chart Pattern Core 分开校验和计数。CXT v1 保持冻结，Chart v4 只接受
CXT v1；Chart v5 只接受 CXT v2。

### S8-D：有限展开

标准展开顺序：

```text
Chart/CXT typed read
  -> module validation
  -> parameter freeze
  -> finite Pattern expansion
  -> Template/Prototype expansion
  -> concrete semantic requirements and Release/tail/resource relations
  -> conflict and count validation
  -> Canonical Gameplay Graph (`gameplay.version = 2`)
  -> graph semantic identity and capability closure
  -> Packed lowering or direct Graph Playback entry
  -> Runtime compile
```

展开发生在作者源 prepare/显式 compile 阶段。发行 Playback 不读取 CXT AST，也不在运行时执行
未展开 Pattern。CXT 不访问 Judgement 内部状态，不能读取上一帧结果或生成运行时对象。
参数改变需要重新编译 Packed，不通过 source entry 隐式恢复运行时模板求值。
CXT Requirement `effects` 只保留 source-only 兼容字段；非空值必须显式 lowering 到
Presentation/FactBinding，且不得进入 Canonical Gameplay Graph、Ruleset state、Replay identity
或 Judgement outcome。无法证明 presentation-only 的 effects 稳定拒绝。

### S8-E：Packed Chart

至少冻结：

```text
magic / version
section table
checksum
96-byte candidate/formal Header
32-byte Section Directory Entry
STR0 / REF0 / IDN0
ARCH Packed Archetype defaults
ENT0 concrete entity index
typed component streams and sparse field masks
Beat GridDelta / Rational atoms
RationalBeat 无损还原
Gameplay V2 graph projection: requirements, phases, resource claims, relations, solver policy,
FactBinding and capability closure
compiled semantic identity and prepared judgement identity
decoded-size 和展开计数预算
```

IDN0 必须保存每个具体实体的 semantic identity；DBG0 只能保存可删除的 source/build
provenance。ARCH 是已展开 Component 的物理默认值表，不是 CXT Prototype；ENT0 是
具体实体索引，不是 CXT Instance。每实体不得重复写入完整 UUID、字符串 Component 名、
AssetId 或原型字段。详细物理布局见 [PACKED_CHART_FORMAT.md](../../../formats/PACKED_CHART_FORMAT.md)。
Packed Chart 是 v5 的物理编码，不是 Chart v6。

BEH0/BHD0/ANM0/FXS0 与 generated-target animation binding 必须先有完整字段/
枚举、capability 和 golden；候选修改按 revision 区分，正式发行才解除 candidate
标识。未支持内容稳定拒绝，不以 opaque JSON 或 CXT AST 替代编码。
Packed 只是 Canonical Gameplay Graph 的物理投影；它不能改变 Release/tail phase、single
capacity=1 resource lifecycle、Fact causal order、FactBinding 或 Ruleset fault semantics。

### S8-F：CXC v1 Graph/Packed Playback Entries

CXC v1 manifest 必须能够区分：

```text
source entry
compiled Packed Chart entry
compiled Gameplay Graph entry
playback entry
entry encoding
source semantic identity
compiled semantic identity
artifact identity
compiler profile
expanded entity/event counts
Ruleset binding
required capability closure
source-of relationship
```

Chart v5 的 Playback entry 可以是已验证的 `packed-chart` 或独立的 `gameplay-graph`。
两者都必须在 prepare 前验证 `gameplay.version = 2`、compiled semantic identity、Ruleset
binding、capability closure、resource closure 和 artifact identity，并恢复同一 Canonical
Gameplay Graph，随后进入同一 Judgement/Ruleset/Snapshot/Replay/Presentation path。
作者 JSON、CXT source 和原始导入文件只能作为 Source Project 或显式 source entry 保留，
Playback 不读取这些 source entry。Graph entry 不是第二套判定实现，也不能绕过共享的
语义校验、capability 拒绝矩阵或 Graph 自身的 closure/decoded/runtime 预算；16 MiB Packed-file
门禁只适用于 `packed-chart` artifact，不能错误套用到独立 `gameplay-graph` entry。

### S8-G：迁移与默认 Writer

完成：

```text
v4 -> v5 显式迁移
旧 Gameplay revision -> gameplay.version = 2 显式迁移或稳定拒绝
CXT v1 -> v2 独立迁移
--target 5
默认 Writer 切换
--target 4 兼容窗口
v5 输入拒绝下调到 v4
```

迁移、Packed 编译和 CXC 打包必须使用原子输出；失败不得发布部分有效产物。

## 4. 容量与预算

必须分别定义：

```text
Authoring Budget
  JSON/CXT source 的编辑和迁移预算

Packed Entry Budget
  Packed Chart 的 16 MiB 硬限制

Chart Closure Budget
  CXC 中 Chart、CXT source 和关联资源的总预算

Expanded Runtime Budget
  展开实体、事件、字符串、decoded bytes、峰值 prepare 内存和每帧写入
```

40,000 个实体必须按语义实体计数，不能用 Packed record 数或隐藏 Pattern 事件规避。
每个必验 capacity profile 均需达到 40k/16 MiB，其实体内动画、要求、身份长度与
引用复杂度须预先声明并经 owner acceptance。不能事后缩小测试输入以宣称关闭，也
不能把“40k 实体”解释为允许附带无限事件/资源。分项报告包括 IDN0、动画和引用成本。

source/build provenance、expanded semantic、Packed artifact 和 prepared/replay
identity 分开验证。最后一层结合实际资源内容和 Stage 7A Input/Judgement profile，
不得直接用 v4 source JSON hash 代表 Packed 语义；完整组合算法须独立版本化。

## 5. 安全和确定性门禁

禁止：

```text
任意表达式
Lua / JavaScript / C++
while/for 任意循环
递归 Pattern
运行时随机
上一帧状态
Judgement 内部状态读取
宿主 API
逐帧对象生成
未版本化顶点回调
```

必须验证：

```text
输入数组顺序无关
参数和引用解析顺序无关
跨平台 canonical identity 一致
checked arithmetic 无溢出
展开深度有限
展开数量有限
展开时间有限
decoded bytes 有界
```

## 6. 验收矩阵

至少包含：

```text
40,000 Tap
40,000 Hold
40,000 mixed requirements
高重复 Pattern
低重复率输入
无 Pattern 的最坏情况
大量 AssetId / Material
大量动画事件
嵌套深度边界
超出展开数量
超出 decoded bytes
超出 Packed bytes
递归 Pattern
Beat / integer 溢出
```

每个合法 fixture 必须证明：

```text
JSON -> semantic model
CXT -> semantic model
JSON/CXT -> Canonical Gameplay Graph
Graph -> Packed Chart
Graph -> CXC gameplay-graph Playback entry
Packed Chart -> Runtime
Graph/Packed -> same prepared Judgement kernel
```

在以下方面一致：

```text
entity count
timing
JudgementRequirement
action
input domain
Release/tail phase
single exclusive resource lifecycle
Fact causal order and TimebaseProfile
FactBinding / early-exact-late-Miss Presentation
resource closure
semantic identity
Replay identity
```

## 7. 验收标准

- Chart v5、CXT v2、Canonical Gameplay Graph 和 Packed Chart 的 Spec、Schema、Reader、Writer
  和 validator 具备一致合同；Graph 与 Packed 进入同一 prepared Judgement kernel。
- 40,000 个语义实体可以在 `16 * 1024 * 1024` bytes Packed entry 内通过硬门禁。
- CXT 展开、Packed lowering、Runtime compile 和 CXC playback entry 的 identity 关系可验证。
- CXC source entry、compiled entry、`packed-chart` playback entry 和 `gameplay-graph` playback
  entry 不混淆。
- JSON、CXT、Canonical Graph、Packed、Graph entry 和 Runtime 在时间、判定要求、Release/tail、
  resource、FactBinding、资源闭包、prepared identity 和 Replay identity 上等价。
- Ruleset transaction failure 保留已提交 Fact、拒绝半个 StateDelta，并可重建 PresentationEvent；
  faulted session 的 Snapshot/Seek/Replay 行为有负例证据。
- 超展开数量、超 decoded bytes、超 Packed bytes、递归、溢出和非法引用均稳定失败。
- Stage 7A 的 core external consumer、Headless 判定和 Replay golden 全部通过；Stage 7B+
  仅对本次发行矩阵明确纳入的能力提供对应证据，不要求全部未来判定能力完成。

## 8. 交接

Stage 8 关闭后：

```text
Chart v5 / CXT v2 成为生产格式
Packed Chart 与 Canonical Gameplay Graph 都成为 Chart v5 CXC Playback entry；二者共享
Canonical Graph、prepared identity 和 Judgement kernel
CXC 仍为 v1
SDK 使用基于实际集成基线和公共合同差异批准的 v5 发行版本
Stage 9 可以消费稳定的 v5 Presentation/Requirement 边界
Stage 10 可以开始正式 Studio authoring
```

SDK 版本遵循 [版本规范](../../../guides/VERSIONING.md)，不与 Stage 编号绑定，也不预留
`0.8.0`。发行前必须纳入 Stage 7A 等前序 SDK 变更，批准具体版本与兼容/迁移决策，
将该版本落实到安装包、consumer 最低要求、版本拒绝测试及关闭报告；未落实不得关闭。

Stage 8 关闭前，Chart v5 只能称为 candidate，不能被 Player 默认作为发行格式加载。
Stage 8 关闭不代表 Stage 7B+ 全部完成；后续高级 Judgement 能力继续沿 Stage 7B+ 能力线
以版本化 capability 方式增加。

## 9. 明确不包含

- 天空盒、Model v1、静态 glTF 和复杂 Geometry Deformation。
- Studio 正式产品。
- 任意运行时脚本、通用 Script VM 或宿主回调。
- 改变 CXC 容器版本。
- 把 Packed Chart 当作 Chart v6。
- H06/H08：未来开发，现不支持。
- H01-H05/H07：能力边界，不会再开发，不受支持。
