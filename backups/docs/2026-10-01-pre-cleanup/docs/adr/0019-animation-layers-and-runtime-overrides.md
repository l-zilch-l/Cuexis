# ADR 0019：动画 Layer 与 Runtime Override

日期：2026-07-17

状态：已接受

## 背景

多 Clip、Gameplay 临时效果和 Studio 预览需要同时写入属性，隐式更新顺序会破坏确定性。

## 决策

属性按 Initial、Behavior、显式优先级 Animation Layer、GameplayOverride、StudioPreviewOverride 求值。Layer 使用 mask 和 BlendGroup；相同 priority 不允许属性重叠。

Override 采用确定性权重混合，Additive 按属性类型定义。Gameplay 和 Studio 通过有生命周期的 OverrideToken 写入 PropertyResolver，不直接修改最终 Component。

## 备选方案

按数组顺序混合和 Gameplay 直接写 Component 均会使结果依赖帧与遍历顺序，因此不采用。

## 影响

Animator、PropertyResolver、Chart Validator 和调试界面需要 Layer、weight、mask、group 和 Token 数据。

## 后续风险

Quaternion additive 和离散属性混合需要严格边界测试；未知属性不能获得隐式算法。

## SDK 转型补充（2026-07-20）

面向宿主的现行术语将 `GameplayOverride` 泛化为 `HostOverride`。它覆盖宿主临时效果和 SDK 判定表现，但仍使用有生命周期的 OverrideToken，并不得直接写最终 Component。StudioPreviewOverride 保持独立。该术语调整不改变已接受的层顺序、冲突和确定性规则。
