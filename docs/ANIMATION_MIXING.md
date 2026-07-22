# Cuexis Animation Mixing 规范

状态：阶段 4 设计已接受

更新日期：2026-07-20

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

每个 Clip Instance 具有稳定 `instanceId`、Clip、localTimeMs、weight、Override/Additive mode 和可选 mask。

## Override BlendGroup

同一 BlendGroup 内的 Override weight 归一化。总 weight 为 0 时不写入。

```text
float/vector/color  分量线性加权
quaternion          以最小 instanceId 为半球参考，符号对齐后加权并归一化
bool/enum/discrete  选择最大 weight；相同 weight 选择最小 instanceId
```

该规则使结果不依赖遍历顺序。跨 BlendGroup 的同属性写入是错误。

## Additive BlendGroup

Additive 在当前 Layer 的 Override 结果之后应用：

```text
position  加权 delta 求和
scale     product(lerp(1, positiveFactor, weight))
rotation  Quaternion delta 转切空间，求加权和后指数映射并乘到 base
```

Additive scale factor 必须为有限正数。其他属性只有 PropertyBinding 明确定义 Additive 算法时才可使用。

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

Clip 必须支持从 localTimeMs 绝对采样。timeDiscontinuity 后重新计算 Layer 和 Clip local time，不使用上一帧混合结果。状态机事件回调需要用区间查询单独处理，不能从 Property 混合顺序推断。

## 基线修改

运行期永久修改 Entity 初始属性只能提交 `BasePropertyCommand`。命令以 ChartObjectId、Property ID 和新值寻址，在 RuntimeSession 主线程安全点应用并递增 `baseRevision`；随后从新基线完整求值。

HostOverride、Animation 和 Editor Preview 不得用直接写 Component 的方式改变基线。Studio 对文档的编辑应重建 Replacement Session，不把 Runtime 命令反向保存成 ChartDocument。

## 调试

调试界面或 SDK 诊断快照显示属性的 Initial、Behavior、各 Animation Layer、Host/Preview Override、权重、mask、冲突和最终值。FrameSnapshot 只包含宿主渲染所需的最终表现数据，不泄漏 PropertyResolver 或 World 内部状态。
