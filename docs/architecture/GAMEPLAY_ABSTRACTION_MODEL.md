# 音乐游戏玩法抽象模型

状态：现行架构模型

更新日期：2026-09-05

本文定义 Cuexis 对狭义音乐游戏（以下简称“音游”）的抽象理解，是 Chart、
Input、Judgement、Playback 和 Presentation 后续设计的共同语义基础。本文不是
某一个 Chart 版本的字段合同，也不表示所有对应运行时模块已经完成。

## 1. 讨论范围

本文讨论的音游具有以下特征：

```text
场景主要由音乐播放时间驱动
玩家操作具有有限分支和有限随机性
操作要求可以被预先描述或确定性生成
核心目标是完成一段尽可能准确的“演奏”
```

本文不把所有“包含音乐的游戏”都视为音游。以躲避、跑酷、反应或连续移动为
主要目标、但没有以演奏准确性作为核心目标的游戏，可以复用 Cuexis 的时间、
输入或表现基础设施，但不自动成为本文模型中的音游。

“亚音游”可以部分包含本文所需的机制，但不作为本模型必须完整覆盖的对象。

## 2. 核心命题

音游的核心目标可以抽象为：

> 在正确的时间区间内，针对正确的判定位置或输入域，执行正确的动作。

形式化表示为：

```text
NoteRequirement =
    (timeInterval, judgementDomain, requiredAction, constraints, effects)
```

| 要素 | 含义 |
| --- | --- |
| `timeInterval` | 要求生效的时间区间，可以是瞬时区间、持续区间或一组区间 |
| `judgementDomain` | 输入与判定所处的域，例如轨道、触摸区域、键位或控制器通道 |
| `requiredAction` | 玩家必须执行的动作，例如按下、释放、持续、滑动或旋转 |
| `constraints` | 对时序、顺序、持续时间、方向、组合输入等的限制 |
| `effects` | 判定成功、失败或输入发生后触发的有限派生行为 |

“正确的位置”不是“屏幕上的某个像素”的同义词。它是判定系统能够识别的输入
位置或输入域，可能由屏幕空间、世界空间、离散轨道、键盘按键、鼠标、触摸点、
手柄按键或外部控制器通道表达。显示位置只是这种域的一种可视化方式。

“正确的时间”不是单个时间刻的强制要求。除非某个具体判定类型另有规定，它应
被理解为一个明确的时间区间：

```text
timeInterval = [start, end)
```

按下、释放、持续和结束可以分别定义边界与容差，但这些边界必须属于判定合同，
不能由帧率、上一帧状态或表现层偶然决定。

## 3. 模型层次

统一模型不是把所有音游画成同一套界面，而是把不同音游拆成可组合的语义层：

```text
Music / AudioClock / TimingMap
          |
          v
Chart / Gameplay Requirements
          |
          +--> Input Mapping --> Input Observation
          |
          v
Judgement
          |
          +--> JudgementResult
          |
          v
Behavior / Effect Schedule
          |
          v
Presentation / FrameSnapshot / Audio Feedback
```

### 3.1 Timing 与音乐时钟

Timing 系统把音乐播放时间、Chart Beat、Stop、Tempo Event 和播放倍率等概念
转换成统一的可采样时间轴。它回答“当前演奏进行到了哪里”，不回答玩家是否
操作正确。

要求必须能够在确定性的时间坐标中表达。Seek、暂停、Stop、reload 和播放设备
重建后，系统应根据目标时间重建结果，不能依赖上一帧最终值作为新的语义基线。

### 3.2 Chart／要求描述层

Chart 描述“玩家应该做什么”，而不是“应该怎样画出来”。它至少需要能够表达：

```text
要求何时生效
要求作用于哪个判定域
要求需要什么动作
要求是否持续、重复、组合或有方向
要求适用哪些判定约束
要求关联哪些有限的表现或反馈效果
```

Chart 可以继续使用 Cuexis 的统一 Object、Component、Template、Behavior、
Animation 和资源引用模型，但这些机制必须服务于要求描述或其有限表现。不得
因为增加新的音符品类，就重新增加平行的 `notes`、`elements`、`decorations`
顶层数组。

Chart 不应保存运行时状态，例如当前连击、上一帧输入、当前判定结果、宿主覆盖
值或已经执行过的副作用。

### 3.3 Input 层

Input 层把平台输入转换成统一的输入观察值，负责处理输入设备差异，而不决定
谱面如何显示。

输入观察值至少应能描述：

```text
observationTime
inputDomain
action
position or channel
pressure / amount（如果设备支持）
source identity（如果需要重放或诊断）
```

屏幕触摸、键盘按键和外部控制器通道可以映射为同一个 `judgementDomain` 与
`requiredAction`。输入映射必须显式、可测试，并在需要确定性回放时进入 Replay
或 PreparedPlayback 的身份计算。

### 3.4 Judgement 层

Judgement 层比较 Chart 要求和输入观察，产生判定结果。它负责：

```text
时间区间匹配
位置或输入域匹配
动作匹配
持续与释放边界
组合输入和前置状态
容错、优先级和冲突处理
```

判定可以依赖上下文，但上下文只是 `constraints` 和 Judgement 状态的一部分，
不能把判定规则转移到渲染器或某个 Note 的视觉实现中。

判定结果应是数据，而不是直接修改画面：

```text
JudgementResult =
    (requirementId, outcome, timingError, positionError,
     actionMatch, comboDelta, lifeDelta, diagnostics)
```

`Perfect`、`Good`、`Miss` 等名称不是模型核心；核心是结果由要求和输入的确定性
比较得到。

### 3.5 Presentation 层

Presentation 层把 Chart、当前时间、输入状态和 JudgementResult 映射为画面、
声音和交互反馈。它可以决定：

```text
轨道如何显示、Note 使用什么模型或材质
位置如何投影到屏幕、判定线如何运动
按键或触摸如何反馈
轨道是否倾斜、旋转或变形
成功、失败和连击如何表现
```

同一个要求可以被表现为下落 Note、横向 Note、固定键位、空间目标或完全不同的
视觉形式，只要它们仍然映射到同一组时间、判定域和动作语义。

没有画面表现时，要求、输入和判定仍然可以完整运行。视觉表现是帮助玩家预测
和执行要求的反馈系统，不是音游核心目标的必要条件。

## 4. 行为与“脚本”边界

仅有表现系统和判定系统还不足以表达全部音游效果，因此 Cuexis 需要一个受约束
的 Chart Behavior／Effect Schedule 层。它用于描述由预先声明的事件触发的有限
行为，例如：

```text
未在判定区间内按键时产生惩罚
触摸或命中某个区域时显示提示线
按键命中时播放指定 key sound
根据输入位置对轨道施加小幅倾斜
根据判定结果改变材质、可见性或动画参数
```

这一层的正确定位是声明式行为和效果调度，不是任意运行时脚本。它应当具有：

```text
有限的事件类型
明确的输入和输出类型
可验证的资源与对象引用
确定性的时间和优先级规则
可计算的安全预算
可在无渲染环境中测试的结果
```

建议的抽象结构为：

```text
Trigger
  -> Condition / Context Filter
  -> Effect Action
  -> Target / Property / Audio Feedback
```

例如，`on-hit` 可以触发有限时长的材质闪烁或一个音效引用，但不能调用任意
C++、Lua、JavaScript 或宿主函数。运行时脚本、逐帧脚本回调、未知字节码和隐式
执行的外部代码仍然属于明确排除项，除非未来通过独立 ADR 建立新的安全和兼容合同。

因此，工程实现中的“脚本系统”应优先理解为：

```text
Chart Behavior / Event / Effect Schedule
```

它可以由 Behavior Event、Animation、Step Track、判定结果绑定和专用效果组件
组合实现，不要求出现一个通用 Script VM。

## 5. 语义与表现的分离规则

后续设计必须遵守以下规则：

1. **语义优先于画面**：先定义时间区间、判定域和动作，再定义显示方法。
2. **表现不得成为隐式判定**：模型、材质、摄像机或动画不可决定要求是否存在。
3. **判定不得读取渲染细节**：判定不依赖像素位置、GPU 状态或渲染顺序。
4. **输入映射必须显式**：屏幕坐标、键位和控制器通道的转换不可隐式猜测。
5. **行为效果必须可追踪**：效果应能追溯到触发条件、目标和时间范围。
6. **运行时状态不得回写 Chart**：连击、生命值、动画进度和已触发事件属于会话状态。
7. **支持绝对时间采样**：Behavior、Animation 和表现效果不能依赖逐帧累积。

## 6. 与 Cuexis 架构的对应

| 抽象层 | Cuexis 对应方向 |
| --- | --- |
| Timing | `TimingMap`、`AudioClock`、`RuntimeTimeline` |
| Chart 要求 | ChartDocument、ChartRuntime、Object/Component/Template |
| 输入 | 未来的 Input 系统与宿主输入适配器 |
| 判定 | 未来的 Judgement 系统、`JudgementResult` 和 Replay |
| 行为与效果 | Behavior Event、Animation、PropertyResolver、有限效果组件 |
| 表现 | FrameSnapshot、Portable Presentation、Player/Studio Adapter |
| 资源 | Asset Index、CXT、CXC 与 Portable Presentation |

Chart、Runtime 和 World 的边界仍由
[ADR 0007](../adr/0007-chart-runtime-world-boundary.md) 约束。统一 Object 模型由
[ADR 0014](../adr/0014-unified-chart-object-schema.md) 约束。时间与事件边界由
[TIMING_MODEL.md](../formats/TIMING_MODEL.md)、[CHART_FORMAT.md](../formats/CHART_FORMAT.md)
和各 Chart 版本合同约束。

## 7. 对格式设计的要求

任何新的 Chart 版本或 Packed Chart 物理编码，都必须在不依赖具体表现后端的情况下
保留以下信息：

```text
要求的稳定 identity
时间区间或可确定地生成时间区间的参数
判定域及其稳定引用
动作和动作约束
持续、释放、组合和顺序语义
与判定结果关联的有限行为
可验证的资源闭包和安全预算
```

Packed Chart 只能改变这些语义的物理编码，不能把表现数据压缩成无法恢复的隐式
约定，也不能因采用原型、实例、索引表或 delta 编码而改变判定结果。作者层 JSON、
canonical semantic model 和 Packed Chart 之间必须保持明确的语义等价关系。

未来的天空盒、环境、复杂形变和更丰富的后处理，原则上应作为 Presentation
Environment、Geometry Deformation、Model Animation 或 Postprocess 等独立表现域
演进，不能强行变成普通可判定对象。只有当某个环境或形变本身改变玩家必须完成
的时间、判定域或动作时，才应进入 Chart 要求或 Judgement 语义。

## 8. 典型映射

### 8.1 固定键位音游

```text
Chart:
  [beat 16, beat 16] + keyboard lane D + press

Input:
  key D pressed at beat 16.02

Judgement:
  timing interval matched -> Perfect

Presentation:
  lane note, key flash, key sound, combo update
```

### 8.2 触摸区域音游

```text
Chart:
  [beat 24, beat 26] + touch region R + hold

Input:
  touch begins in R, remains in R, releases at beat 26.01

Judgement:
  begin/continuous/end constraints evaluated independently

Presentation:
  moving line, touch trace, deformation and release effect
```

这两个例子可以拥有完全不同的画面和输入设备，但都符合“时间区间、判定域和
动作”的核心表达。

## 9. 当前实现阶段与非目标

本文是后续实现的抽象基线，不表示所有层已经在 SDK 中完成：

```text
已有：Timing、Chart、Behavior、Animation、Presentation 和 Playback 基础
规划：完整 Input、Judgement、Replay 以及面向音游的结果接口
延期：任意运行时脚本、逐帧回调、通用 Script VM
```

实现新功能时，若无法判断它属于 Chart 语义、判定规则、输入映射、有限行为还是
表现环境，应先补充 ADR 或模型说明，再增加格式字段。不得仅因为某个音游需要
某种画面效果，就把该效果提升为所有 Chart 的核心语义。
