# Cuexis Animation Mixing 规范

状态：accepted contract；格式 lowering 由 Stage Chart Format Update 实现，运行时求值已由 Stage 4 关闭

更新日期：2026-08-27

Stage Chart Format Update 位于 Stage 3 与阶段 4 之间。它负责冻结 CXC/Chart/CXT 的持久化边界；
本文只定义运行时求值与混合语义。ChartParameter、Clip、Layer、Binding、Property ID、mask、时间域
和 lowering 的字段权威是 [CHART_V4_FORMAT.md](CHART_V4_FORMAT.md)，CXC 容器与闭包由
[CXC_FORMAT.md](CXC_FORMAT.md) 定义，CXT 文件语义由 [CXT_FORMAT.md](CXT_FORMAT.md) 定义。
其中格式合同已接受并开始实现。阶段 4 的 AnimationSystem 只能消费格式阶段交付的 typed
数据，不得自行解析 JSON/CXC/CXT。

运行时脚本和逐帧脚本回调无限期延后。本文不定义脚本状态、事件调度、沙箱、字节码、Seek 恢复或 Replay 合同，也不为这些能力预留扩展入口。

## 求值层

固定顺序：

```text
Entity Initial State
-> Behavior
-> Animation Layers（priority 升序）
-> HostOverride
-> StudioPreviewOverride（仅 Studio）
-> PropertyResolver commit
```

任何层不得绕过 PropertyResolver 直接写入同一可绑定属性。

## Animation Layer

```text
layerId       稳定 ID
priority      显式整数，越大越晚应用
weight        [0, 1]
propertyMask  明确的 Property ID 集合或前缀集合
blendGroups   Clip Instance 分组
```

相同 priority 的不同 Layer 不允许写入相同属性；发现重叠即验证失败。不得使用数组顺序打破平局。

每个 BlendGroup 具有 mode 和 `[0,1]` group weight。每个 Clip Instance 具有稳定 `instanceId`、
Clip、绝对 `localSampleTime`、`[0,1]` instance weight 和可选 mask。CXT Template Binding 在 prepare
时 lowering 为同一 typed Clip/Layer/Group/Instance 模型；其 Binding weight 写入 generated Group，
generated Instance weight 固定为 1。

`localSampleTime` 是带显式 domain 的 typed 值，Chart v4/AnimationClip v1 只使用局部 Beat。Runtime
负责从 Chart/Session 时间生成该值，AnimationSystem 不读取 Chart、CXT 或 TimingMap。

Group weight 控制一个 Group 相对该 Layer 输入值的贡献，Layer weight 控制整个 Layer 相对上一
求值阶段的贡献。Instance weight 只控制 Group 内 Clip 的相对或 additive 贡献。Layer/Group weight
为 0 时该层级不写入。

## Override BlendGroup

同一 BlendGroup 内对当前属性有有效采样且 instance weight 大于 0 的 Instance 参与 Override。
Instance weight 总和为 0 时该 Group 不写入；否则先按总和归一化并计算 `groupValue`：

```text
float/vector/color  分量线性加权
quaternion          以最小 instanceId 为半球参考，符号对齐后加权并归一化
bool/enum/discrete  选择最大 instance weight；相同 weight 选择最小 instanceId
```

对于 float/vector/color，单个 Override 属性固定使用：

```text
groupOutput = lerp(layerInput, groupValue, groupWeight)
layerOutput = lerp(layerInput, groupOutput, layerWeight)
```

Quaternion 的两步混合都使用 shortest-path slerp。对线性属性，总贡献等价于
`groupWeight * layerWeight`。不同 Group 的属性集合不重叠，因此不存在 Group 间应用顺序。

离散属性不定义部分 Group/Layer weight。写入 bool/enum/resource selection 的 Group 必须是 Override，
且 resolved Group weight 和 Layer weight 都等于 1；否则验证失败。Group 内仍按最大 Instance weight
和最小 `instanceId` 选择 winner。

该规则使结果不依赖遍历顺序。跨 BlendGroup 的同属性写入是错误。

## Additive BlendGroup

Additive 在当前 Layer 的 Override 结果之后应用。每个 Instance 的有效 additive weight 是：

```text
effectiveWeight = instanceWeight * groupWeight * layerWeight
```

按 effective weight 求值：

```text
position  加权 delta 求和
scale     product(lerp(1, positiveFactor, weight))
rotation  Quaternion delta 转切空间，求加权和后指数映射并乘到 base
```

Additive scale factor 必须为有限正数。Additive weight 不归一化；多个 Instance 表示多个显式 delta
贡献。`effectiveWeight` 已包含 Group/Layer weight，Additive 结果不再执行第二次 Layer weight 混合。
其他属性只有 PropertyBinding 明确定义 Additive 算法时才可使用。

## Template Binding lowering

Template Binding 固定 lowering 为：

```text
Layer priority / mask  = Binding priority / CXT actual properties
Layer weight           = 1
Group mode             = CXT application blendMode
Group weight           = resolved Binding weight
Instance weight        = 1
Instance timing        = Binding startBeat/durationScale + CXT iterations/fillMode
```

Generated identity 是 `(objectId, bindingId, templateId, recordKind)` 复合键。若 CXT 包含离散 Track，
resolved Binding weight 必须为 1。

## HostOverride

宿主或 SDK 内部判定表现的临时覆盖通过 `OverrideToken` 注册：

```text
ownerId
priority
propertyMask
lifetime
writes
```

Token 释放或 lifetime 结束后覆盖消失，下一帧从基础层重新求值。相同 priority 的 Token 写同一属性是运行时错误，冲突写入被丢弃。宿主和 Judgement 不直接修改最终 Component，也不需要访问 World/EnTT。

StudioPreviewOverride 使用相同协议但只存在于 Studio Session，不序列化到 Chart。

## 时间跳转

Clip 必须支持从 `localSampleTime` 绝对采样。timeDiscontinuity 后重新计算 Layer 和 Clip local time，不使用上一帧混合结果。Chart v4 v1 不序列化状态机、运行时脚本或事件回调。

## 基线修改

运行期永久修改 Entity 初始属性只能提交 `BasePropertyCommand`。命令以 ChartObjectId、Property ID 和新值寻址，在 RuntimeSession 主线程安全点应用并递增 `baseRevision`；随后从新基线完整求值。

HostOverride、Animation 和 Editor Preview 不得用直接写 Component 的方式改变基线。Studio 对文档的编辑应重建 Replacement Session，不把 Runtime 命令反向保存成 ChartDocument。

## 调试

调试界面或 SDK 诊断快照显示属性的 Initial、Behavior、各 Animation Layer、Host/Preview Override、权重、mask、冲突和最终值。FrameSnapshot 只包含宿主渲染所需的最终表现数据，不泄漏 PropertyResolver 或 World 内部状态。
