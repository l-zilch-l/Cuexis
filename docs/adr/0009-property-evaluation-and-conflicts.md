# ADR 0009：Behavior 与 Animation 的属性求值和冲突规则

日期：2026-07-16

状态：已接受

## 背景

Behavior 使用谱面时间控制表现，Animation 使用局部时间提供通用 Entity 动画。两者都可能写入 Transform、MaterialParam 和 Visibility。若 System 直接按更新顺序修改 Component，最终结果会依赖调用顺序、Track 顺序或 ECS 遍历顺序，时间轴跳转也可能产生累计误差。

## 决策

属性求值采用固定分层流程：

```text
Entity 初始值
  -> Behavior 绝对采样
  -> Animation 显式混合
  -> PropertyResolver
  -> 最终 Component
```

Behavior 和 Animation 输出由 `cuexis_world` 提供的共享 `PropertyWriteBuffer`，不通过先后执行直接覆盖目标 Component。写入可以引用当前 World 的会话期 Entity，但不得进入 ChartDocument 或 ChartRuntime，也不得包含平台或图形后端类型。`PropertyResolver` 由 `RuntimeSession` 持有，在每帧求值结束后统一提交最终结果。

Behavior 输出绝对值。Animation Track 必须显式选择：

```text
Override：替换 Behavior 求值结果
Additive：在 Behavior 求值结果上应用增量
```

Transform Additive 规则为：

```text
position = basePosition + deltaPosition
rotation = normalize(baseRotation * deltaRotation)
scale = baseScale * scaleFactor
```

其他属性只有在对应 PropertyBinding 定义了 Additive 运算时才能使用 Additive。

同一求值层中，对同一 Entity 的同一属性存在没有明确合并规则的多个写入时，谱面编译失败。系统不得使用 Track 数组顺序、Entity 创建顺序或 ECS 遍历顺序隐式决定优先级。

若无效或动态数据在运行时仍产生歧义，Resolver 丢弃该冲突层对目标属性的写入，保留本帧上一已完成层的值并报告错误。不得回退到上一帧最终值。

每帧从实例化时记录的 Entity 初始值重新求值，不使用上一帧混合后的最终值作为新基线。

## 备选方案

### AnimationSystem 在 BehaviorSystem 后直接覆盖 Component

拒绝。该方案只形成偶然的调用顺序约定，无法表达 Additive，也容易在重构系统顺序时产生行为变化。

### 每个属性只能由 Behavior 或 Animation 之一拥有

拒绝。它会限制谱面表现，无法在谱面驱动的基础运动上叠加局部动画。

### 默认按照 Track 数组顺序覆盖

拒绝。编辑器重排、格式迁移和并行求值都会改变结果，且冲突不易诊断。

### 使用上一帧最终值累加

拒绝。它会产生帧率相关结果和累计误差，也无法保证任意时间预览可复现。

## 影响

```text
RuntimeSession 需要持有 PropertyResolver 和 Entity 初始属性状态
BehaviorSystem 与 AnimationSystem 改为生成 PropertyWriteBuffer
Chart 编译器需要检查同层属性写入歧义和非法 Additive
Animation Track 数据格式必须保存 AnimationBlendMode
调试界面需要显示属性各层输入及最终解析值
测试需要覆盖 Override、Additive、冲突和任意时间重复采样
```

## 后续风险

多 AnimationClip、权重、遮罩和 Gameplay/Studio 临时覆盖已由 ADR 0019 定义。新增求值层仍必须明确相对顺序、冲突和调试信息，不能绕过 PropertyResolver。

运行期间永久修改 Entity 初始值使用 `BasePropertyCommand`，仅在 RuntimeSession 主线程安全点生效并递增 baseRevision。不得通过修改最终 Component 值隐式改变求值基线。
