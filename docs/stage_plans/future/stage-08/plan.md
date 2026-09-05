# Stage 8 Implementation Plan: Chart v5 Formal Release and Semantic Convergence

状态：future；未开始

更新日期：2026-09-05

归档来源：[Chart v5 格式计划](../../active/chart-format-update-for-v5/plan.md)、
[Stage Chart Format Update 完成计划](../../completed/chart-format-update/plan.md)、
[CXT v1 格式合同](../../../formats/CXT_FORMAT.md) 和 [CXC v1 格式合同](../../../formats/CXC_FORMAT.md)。

前置：

```text
Chart Format Foundation owner acceptance
Stage 6 owner acceptance
Stage 7A JudgementRequirement / InputEvent / Replay 合同冻结
```

详细格式工作包见 [Chart v5 format plan](../../active/chart-format-update-for-v5/plan.md)。
本阶段负责把 Foundation 和 Stage 6 已验证的 v5 candidate path 收敛为正式发行合同，
而不是首次开始设计 Chart v5。Stage 7B+ 的未选定高级判定能力不属于本阶段硬前置。

## 1. 阶段目标

交付 Cuexis 的高密度谱面正式发行链：

```text
Chart v5 authoring model
  -> CXT v2 template/pattern expansion
  -> canonical semantic model
  -> Packed Chart
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
  描述要求、判定域、动作、时间区间和有限效果

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
持续、释放、组合和顺序约束
有限 Effect Schedule
  Chart v5 与 Stage 7A Judgement 的 typed 边界
  本次发行纳入的 RequirementKind / capability 矩阵
```

Chart v5 不绑定轨道、Note 模型或渲染后端。v4 到 v5 的迁移必须显式执行，
不能由 Playback 隐式升级。
Stage 7B+ 尚未纳入发行矩阵的 RequirementKind 必须保留稳定拒绝路径，不能为了关闭
Stage 8 而伪装成已支持能力。

### S8-B：CXT v2 Core

CXT v2 保留 `cuexis.animation-template` 顶层 format，但不再限制为动画模板。
一个 CXT 文件仍然是一个明确的 module，module 必须声明 export kind：

```text
animation
prototype
pattern
```

Core 最小构造：

```text
Template
Prototype
Instance
Parameter
Pattern
Repeat
Transform
Bind
Override
Finite Expansion
```

参数类型限定为：

```text
integer
number
rational
boolean
enum
vector
reference
```

参数不得改变对象类型、Component 集合、父子层级、AssetId、引用目标、资源闭包
或数组结构。Pattern 只能生成有限、已注册的 Chart 语义记录。

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
  -> concrete semantic requirements
  -> conflict and count validation
  -> canonical semantic identity
  -> Packed lowering
  -> Runtime compile
```

展开发生在 prepare/compile 阶段。Playback 不读取 CXT AST，也不在运行时执行
未展开 Pattern。CXT 不访问 Judgement 内部状态，不能读取上一帧结果或生成运行时对象。

### S8-E：Packed Chart

至少冻结：

```text
magic / version
section table
checksum
StringTable
AssetTable
PrototypeTable
Template/Behavior table
local index
Component mask
prototype / instance layout
Beat delta / varint
RationalBeat 无损还原
decoded-size 和展开计数预算
```

每实体不得重复写入完整 UUID、字符串 Component 名、AssetId 或原型字段。
Packed Chart 是 v5 的物理编码，不是 Chart v6。

### S8-F：CXC v1 Packed Entry

CXC v1 manifest 必须能够区分：

```text
source entry
compiled Packed Chart entry
playback entry
entry encoding
source semantic identity
compiled semantic identity
Packed artifact identity
compiler profile
expanded entity/event counts
```

Chart v5 的 Playback entry 必须是已验证的 Packed Chart。作者 JSON、CXT source
和原始导入文件只能作为 Source Project 或显式 source entry 保留，Playback 不读取
这些 source entry。

### S8-G：迁移与默认 Writer

完成：

```text
v4 -> v5 显式迁移
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
CXT -> Packed Chart
Packed Chart -> Runtime
```

在以下方面一致：

```text
entity count
timing
JudgementRequirement
action
input domain
Effect Schedule
resource closure
semantic identity
Replay identity
```

## 7. 验收标准

- Chart v5、CXT v2 和 Packed Chart 的 Spec、Schema、Reader、Writer 和 validator 具备一致合同。
- 40,000 个语义实体可以在 `16 * 1024 * 1024` bytes Packed entry 内通过硬门禁。
- CXT 展开、Packed lowering、Runtime compile 和 CXC playback entry 的 identity 关系可验证。
- CXC source entry、compiled entry 和 playback entry 不混淆。
- JSON、CXT、Packed 和 Runtime 在时间、判定要求、效果、资源闭包和 Replay identity 上等价。
- 超展开数量、超 decoded bytes、超 Packed bytes、递归、溢出和非法引用均稳定失败。
- Stage 7A 的 core external consumer、Headless 判定和 Replay golden 全部通过；Stage 7B+
  仅对本次发行矩阵明确纳入的能力提供对应证据，不要求全部未来判定能力完成。

## 8. 交接

Stage 8 关闭后：

```text
Chart v5 / CXT v2 成为生产格式
Packed Chart 成为 Chart v5 CXC Playback entry
CXC 仍为 v1
SDK 进入 0.8.0
Stage 9 可以消费稳定的 v5 Presentation/Requirement 边界
Stage 10 可以开始正式 Studio authoring
```

Stage 8 关闭前，Chart v5 只能称为 candidate，不能被 Player 默认作为发行格式加载。
Stage 8 关闭不代表 Stage 7B+ 全部完成；后续高级 Judgement 能力继续沿 Stage 7B+ 能力线
以版本化 capability 方式增加。

## 9. 明确不包含

- 天空盒、Model v1、静态 glTF 和复杂 Geometry Deformation。
- Studio 正式产品。
- 任意运行时脚本、通用 Script VM 或宿主回调。
- 改变 CXC 容器版本。
- 把 Packed Chart 当作 Chart v6。
