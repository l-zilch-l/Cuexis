# Stage 7 Implementation Plan: Gameplay Foundation and Judgement Evolution

状态：future；未开始

更新日期：2026-09-05

归档来源：[旧版 PROJECT_GUIDE](../../../archive/PROJECT_GUIDE_LEGACY_2026-08-10.md)、
[SDK transition plan 快照](../../../archive/CUEXIS_SDK_TRANSITION_PLAN_2026-08-10.md) 和
原 Stage 11 Judgement 计划。

前置：[Chart Format Foundation](../../completed/chart-format-foundation/plan.md) 和
[Stage 6](../../active/stage-06/plan.md)。主要输入为 Chart v5 Core/Packed candidate；Chart v4 /
CXT v1 / CXC v1 保留为兼容和回退输入。共同玩法模型见
[音乐游戏玩法抽象模型](../../../architecture/GAMEPLAY_ABSTRACTION_MODEL.md)。

Stage 7 不是一次性完成所有音游品类的阶段，而是一条持续的 Gameplay 能力线：Stage 7A
冻结最小公共内核，Stage 7B 及以后在不破坏 7A 公共身份和生命周期的前提下增加高级
Input/Judgement 能力。Stage 8 只依赖 Stage 7A，不等待全部 Stage 7B+ 完成。

## 1. 阶段目标

建立可被 Chart v5、Player、Headless consumer 和后续 Studio 共同消费的判定边界：

```text
InputEvent
  -> InputMapping
  -> JudgementRequirement
  -> Judgement
  -> JudgementResult
  -> Score / Combo / Statistics
  -> Player or host feedback
```

判定必须独立于渲染帧率、渲染后端、画面表现和 World Entity。Presentation、Behavior
和有限 Effect Schedule 可以消费结果，但不能反向改变同一事件的判定事实。

## 2. 分阶段范围

### Stage 7A：Minimum Input and Judgement Kernel

Stage 7A 是 Stage 8 的硬前置，冻结 Chart v5 Core 所需的最小接口和确定性行为：

```text
InputEvent
InputDomain
Action
InputMapping
JudgementRequirement
JudgementResult
Tap / Hold / Release
基础离散判定域
基础时间区间和容差
位置/通道匹配
Miss、重复输入和冲突输入
Score / Combo / Statistics
Replay identity 和最小预算
```

Stage 7A 必须只冻结跨品类稳定的概念，不把轨道、Note、屏幕坐标、键盘布局或某个
渲染后端写入公共 Judgement API。时间使用显式区间和绝对观测时间；位置可以是离散
判定位置、区域、输入通道或其他已注册 InputDomain。

### Stage 7B：Advanced Judgement Capabilities

Stage 7B 及其后续批次负责增加更复杂的要求与输入约束：

```text
Slide / 连续轨迹
Flick / 方向动作
多指和指间关系
复杂 Hold、分段持续和 Release 约束
方向、速度、压力、距离和轨迹约束
输入占用、优先级和冲突解析
```

每项能力都必须成为版本化的 `RequirementKind`、约束集合或 capability。高级能力可以
在 Stage 8 前后持续开发；只有明确纳入某次 v5 发行的能力才需要进入 Stage 8 的选定
发行矩阵。未纳入的能力必须稳定拒绝，不能静默降级为 Tap、Hold 或 v4 语义。

### Stage 7C：Judgement Policy and Device Evolution

后续批次可以增加不改变核心事件身份的策略能力：

```text
校准和设备延迟补偿
Timing offset / input latency policy
判定等级和窗口策略
设备能力协商
Replay 版本演进
更丰富的 Score / Life / Accuracy policy
```

这些策略必须通过显式、可复现的 `JudgementConfig`、`CalibrationProfile` 或 capability
表达，不读取上一帧隐式状态，不把宿主时钟或设备私有类型带入语义合同。

## 3. Stage 7A 工作批次

### S7A-A：统一输入模型

定义 typed `InputEvent`、`InputDomain`、`Action` 和 `InputMapping`：

```text
observationTime
inputDomain
action
position or channel
pressure / amount（可选）
source identity（可选）
sequence
```

输入事件时间必须与到达时间、渲染帧时间分离。键盘、鼠标、触摸、手柄和外部控制器
通过显式映射进入统一输入域。

### S7A-B：JudgementRequirement

建立不依赖渲染的要求模型：

```text
requirementId
timeInterval
judgementDomain
requiredAction
constraints
```

Tap 的区间、Hold 的开始/持续/结束和 Release 的边界必须有明确的半开区间规则。判定
不能读取像素、GPU 状态、渲染顺序或 World Entity。

### S7A-C：Judgement Engine

实现 Stage 7A 的最小求值：

```text
时间区间匹配
输入域匹配
动作匹配
Hold 持续和 Release
重复输入和冲突输入
判定优先级
Miss 结算
```

所有求值必须基于绝对时间和显式状态快照。状态快照用于表达已确认的生命周期，不能
把上一帧的最终结果当作未声明的语义基线。

### S7A-D：Score、Combo 和 Statistics

定义：

```text
JudgementOutcome
ScoreState
ComboState
LifeState（若启用）
StatisticsSnapshot
```

Score、Combo 和 Life 属于运行时会话状态，不回写 Chart、Behavior 或 World 持久化数据。

### S7A-E：Replay

Replay 至少记录：

```text
规范化 InputEvent
Chart semantic identity
Timing identity
InputMapping identity
JudgementConfig identity
```

实时输入和 Replay 输入必须进入同一 Judgement 路径。Replay 必须有事件数、字节数和
解码预算，并在 identity 不匹配时稳定失败。

### S7A-F：Player 与 Headless 闭环

Player 提供最小反馈：

```text
判定结果
连击
分数
Miss
基础按键/触摸反馈
```

Headless consumer 必须可以执行相同的 Judgement 和 Replay，不要求 GPU。

## 4. 7A 交接给 Stage 8 的合同

Stage 7A 关闭时必须交付：

```text
JudgementRequirement typed model
InputEvent typed model
InputMapping identity
JudgementResult lifecycle
Score/Combo result model
ReplayData identity and budget
Tap/Hold/Release boundary rules
Headless golden fixtures
基础错误、预算和 identity mismatch 诊断
```

Stage 8 可以在此合同上正式收敛 Chart v5；不得重新发明一套与 Stage 7A 不兼容的输入
或判定模型。Stage 7B+ 的能力若未进入 Stage 8 的发行矩阵，不得成为 Stage 8 的隐性
前置。

## 5. 7B+ 能力演进规则

- 7A 的 `InputEvent`、`JudgementResult`、Replay identity 和生命周期在兼容窗口内保持稳定。
- 新的 `RequirementKind`、约束字段和策略必须版本化，并有 capability 查询和稳定拒绝路径。
- 高级能力应提供 headless golden、确定性 replay、预算和迁移/拒绝测试。
- 高级能力可以在 Stage 8 关闭后继续交付；不能通过修改既有字段含义破坏已发行 v5。
- Presentation 只能订阅结果和 Effect Schedule，不能把提示线、Key Sound、轨道倾斜等
  表现需求伪装成 Judgement 结果；它们由脚本/表现声明合同另行消费。

## 6. 验收标准

### Stage 7A 关闭标准

- 相同 Chart、InputEvent、映射、Timing 和判定配置得到完全一致的结果。
- 不同渲染帧率、Entity 遍历顺序和渲染后端不改变判定结果。
- Seek、Pause、Reload 和 Audio discontinuity 后结果仍然确定。
- 实时输入与 Replay 的 JudgementResult、Score、Combo 和 Statistics 一致。
- 判定系统不暴露 World、EnTT、SDL、OpenGL 或宿主引擎类型。
- 无 InputEvent 的纯播放和 Studio Preview 中 Judgement 休眠且不改变 FrameSnapshot。
- 非法输入、时间回退、预算超限和 identity 不匹配均稳定失败。
- external consumer、static/shared package 和 headless 路径均通过生命周期测试。
- Chart v5 Core/Packed candidate 可以消费 Stage 7A 要求并获得相同的判定结果。

### Stage 7B+ 批次标准

- 每个新增能力有版本化语义、capability、拒绝路径和边界测试。
- 高级要求不改变 7A 已发行字段的含义，并能在没有 Presentation 的 headless 路径复现。
- 每个纳入发行的能力都有 deterministic golden、Replay identity 和预算证据。

## 7. 明确不包含

Stage 7A 不包含：

```text
完整滑动、复杂 Flick、多指、旋转和宿主扩展判定
Studio 编辑器
运行时脚本、通用状态机和宿主回调
```

Stage 7 全线仍不包含任意运行时脚本。高级 Judgement 能力属于版本化 Gameplay capability，
不是对 Chart/CXT/CXC 注入脚本执行权的理由。
