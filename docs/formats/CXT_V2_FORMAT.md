# Cuexis Chart Template (CXT) v2

状态：candidate；Stage 8 前置格式合同；未实现

更新日期：2026-09-05

依据：[Stage 8 计划](../stage_plans/future/stage-08/plan.md)、
[音乐游戏玩法抽象模型](../architecture/GAMEPLAY_ABSTRACTION_MODEL.md) 和
[Chart v5 计划](../stage_plans/active/chart-format-update-for-v5/plan.md)。

## 1. 范围

CXT v2 是 Chart v5 使用的声明式模块格式。它在保留 CXT v1 动画模板能力的同时，
增加可复用的实体原型、参数化实例和有限 Pattern 生成，用于减少高密度谱面中的
重复语义。

```text
extension     .cxt
encoding      UTF-8 JSON
format        cuexis.animation-template
version       2
module kind   animation | prototype | pattern
time domain   local rational Beat
```

CXT v1 保持完全冻结。Chart v4 只接受 CXT v1；Chart v5 只接受 CXT v2。
CXT v2 不是脚本、插件、任意表达式语言、ObjectRuntime、ChartRuntime cache 或
宿主回调接口。

## 2. 语义分层

```text
CXT Core
  Prototype
  Instance
  Parameter
  Pattern
  Repeat
  Transform
  Bind
  Override
  finite expansion

CXT Animation Extension
  AnimationClip
  Track
  Segment
  Step
  Animator
  Layer
  BlendGroup
```

Core 生成的是有限的 Chart semantic records；Animation Extension 生成的是
确定性的 AnimationProgram 输入。两者分别校验、计数和报告预算。

## 3. 顶层结构

```json
{
  "format": "cuexis.animation-template",
  "version": 2,
  "moduleId": "pattern.stair",
  "moduleKind": "pattern",
  "metadata": {},
  "parameters": [],
  "prototypes": [],
  "patterns": [],
  "animations": [],
  "exports": [],
  "requiredExtensions": [],
  "extensions": {}
}
```

每个 module 必须声明一个唯一 `moduleId` 和 `moduleKind`。`exports` 中的导出记录
必须引用本 module 内已声明的 Prototype、Pattern 或 Animation。

## 4. 参数

允许的参数类型：

```text
integer
number
rational
boolean
enum
vector
reference
```

参数必须声明默认值、类型、范围和使用白名单。参数可以用于：

```text
Beat
时间间隔
离散 lane/domain
位置和有限 Transform
动画权重
有限重复次数
有限效果参数
```

参数不得改变：

```text
对象类型
Component 集合
父子层级
AssetId
引用目标
资源闭包
数组结构
模块导出类型
```

参数解析在 prepare 前冻结。Chart、CXT、Packed 和 Replay identity 必须包含实际
解析后的规范参数 identity。

## 5. Prototype 与 Instance

Prototype 示例：

```json
{
  "id": "tap.default",
  "components": {
    "cuexis.note": {
      "kind": "tap",
      "judgementDomain": "lane"
    },
    "cuexis.transform": {
      "position": [0, 0, 0],
      "scale": [1, 1, 1]
    }
  }
}
```

Instance 只描述差异：

```json
{
  "prototype": "tap.default",
  "startBeat": { "numerator": 16, "denominator": 1 },
  "overrides": {
    "lane": 2,
    "position": [1, 0, 0]
  }
}
```

Instance 不得覆盖未声明的 Component、资源引用或对象关系。展开后必须得到
普通 Chart semantic record，并使用稳定的生成 identity 参与诊断和 canonical identity。

## 6. Pattern

Pattern 是有限的声明式生成器。首版允许：

```text
Tap pattern
Hold pattern
Release pattern
Lane sequence
Beat sequence
Finite repeat
Finite transform
Finite animation event
```

示例：

```json
{
  "id": "stair",
  "base": "tap.default",
  "events": [
    { "beat": 0, "lane": 0 },
    { "beat": "1/4", "lane": 1 },
    { "beat": "1/2", "lane": 2 },
    { "beat": "3/4", "lane": 3 }
  ],
  "repeat": {
    "count": 16,
    "beatStep": 1,
    "laneOffset": 0
  }
}
```

Pattern 必须是有限的。`count`、嵌套深度、事件数和生成实体数都受预算限制。
Pattern 不读取上一帧、Judgement 内部状态、输入流、随机数或宿主 API。

## 7. 展开顺序

```text
typed read
  -> module validation
  -> parameter freeze
  -> Pattern validation
  -> finite Pattern expansion
  -> Prototype/Instance expansion
  -> concrete semantic records
  -> conflict/count/size validation
  -> canonical semantic identity
  -> Packed lowering
```

展开只发生在 Chart/Playback prepare 或离线编译工具中。Playback 不读取 CXT AST，
不执行未展开 Pattern，也不在每帧生成对象。

## 8. 确定性与安全

禁止：

```text
任意数学表达式
while/for 任意循环
递归 Pattern
运行时随机
上一帧状态
Judgement 结果读取
宿主函数回调
Lua / JavaScript / C++
字节码
逐帧对象生成
```

必须验证：

```text
输入数组顺序无关
参数顺序无关
引用解析顺序无关
Beat 精确有理数运算
checked arithmetic
最大展开深度
最大展开记录数
最大展开字节数
最大 prepare 时间
```

## 9. 预算

CXT v2 至少受以下预算限制：

```text
module count
parameter count
prototype count
pattern count
animation count
Pattern nesting depth
generated semantic entity count
generated event count
decoded bytes
peak prepare memory
```

Stage 8 的硬验收目标是：

```text
40,000 semantic entities
Packed Chart <= 16 MiB
```

40,000 按展开后的语义实体计数，不按 Packed record 或 source module 数量计数。

## 10. 与 CXC 的关系

CXT source 可以作为 Source Project 或 CXC 的显式 source entry，但不是 Chart v5
Playback entry。Chart v5 的发行入口必须是经过 CXT 展开、语义验证和 Packed 编译的
Packed Chart。

CXC manifest 必须能够关联：

```text
CXT source identity
Chart source semantic identity
expanded semantic identity
Packed artifact identity
compiler profile
```

## 11. 与脚本的边界

CXT v2 不提供任意脚本能力。命中反馈、Miss 惩罚、提示线、key sound 和轨道倾斜
应由 Chart Requirement、Judgement Event、Effect Schedule、Animation 和有限
Presentation 组件组合表达。

## 12. 版本与兼容

```text
Chart v4 -> CXT v1
Chart v5 -> CXT v2
Chart v5 rejects CXT v1
Chart v4 rejects CXT v2
```

CXT v2 的 Core 语义一旦由 Stage 8 接受，Chart v6/v7/v8 不得重新解释其模板、
参数、Pattern 或有限展开规则。需要新生成能力时，必须增加明确的 extension/version
并建立独立门禁。
