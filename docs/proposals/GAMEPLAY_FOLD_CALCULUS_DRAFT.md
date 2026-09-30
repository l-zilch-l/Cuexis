# Bounded Fold Calculus 提案

状态：candidate（设计提案，未接受，未实施）

更新日期：2026-10-01

本文是对 [Gameplay Program IR 原语草案](GAMEPLAY_PROGRAM_IR_DRAFT.md) 的**替换性重设计**，
不是增量修订。动机见 [Gameplay 设计压测与缺陷登记](GAMEPLAY_STRESS_TEST_DRAFT.md) §5：原语数量
在收敛（10 → 3 → 0），但同期 10 条缺陷里有 6 条是语义未定义，每修一条就加一条规则。这说明
枚举的是原语，没有枚举规则。

本提案把生成集合从约 15 个概念压到 3 个。

## 1. 诊断：三处重复

对当前全部原语做消去分析，发现它们其实是同一件事的三次出现。

```text
重复一  三套调度   input / timer / level / every   都在回答"这条边何时被求值"
重复二  两处绑定   dispatch（是否独占）与 grip（能否迁移）  都是"程序与输入的关系"
重复三  三处存储   params / registers / Controller 状态  类型、区间分析、快照规则完全相同
```

重复三是最深的：**Controller 的状态推进、判定的匹配、计分的累计，是同一个运算**。三者都是
"把一个事件流，用有界状态，折叠成一个值"。

判定的输入流是输入事件，状态是每条 requirement 的匹配状态；计分的输入流是 Fact，状态是会话
分数；Controller 的输入流是输入事件，状态是运动学状态。形状完全一样，差别只有**寿命**。

## 2. 核心：一个演算，三种寿命

```text
Fold = (State, Schedule, Guard, Effect)

State     有界、可序列化的类型化存储
Schedule  何时求值：onInput | at(expr) | every(period)
Guard     纯表达式，只读
Effect    写状态 | 绑定资源 | 发出 Fact | 终结
```

四种调度合成一种。`level` 就是 `every(0)` 加一条 progress 证明；`input` 是带过滤的
`onInput`；`timer` 是 `at`。一套求值路径，一条验证规则。

三种寿命，同一个演算：

| 寿命 | 输入流 | 状态 | 产生 |
| --- | --- | --- | --- |
| per-requirement（判定） | 输入事件 | 匹配状态 | Fact |
| per-session（计分） | Fact | 分数、Combo、Life | 状态变更、信号 |
| per-device（Controller） | 输入事件 | 运动学状态 | 可采样值 |

这不是"三层语言"，是**一个演算的三个实例**。版本与信任边界仍然分开冻结（这是上一轮已经
论证过的，见讨论记录 §2.9），分开的是接口，不是语义核心。

## 3. 作者层：Pattern 与 Measure

直接暴露演算仍然太低层。作者层只给两个概念：

```text
Requirement = (Pattern, Measure, Grading)

Pattern   有界时间模式：必须在输入轨迹里出现什么
Measure   对被匹配区间的有界折叠：从命中里取出什么数值
Grading   Measure -> Outcome，经 Ruleset 声明的等级表
```

**Pattern 是一个有界时间正则表达式**，原子是轨迹谓词：

```text
atom        p                  轨迹谓词在一点成立
interval    p over [a, b]      谓词在区间内成立，可带容差约束
sequence    P · Q              先后
alternation P | Q              任一
repeat      P{lo, hi}          有界重复
skip        P .. Q  within k   允许跳过多达 k 步
bind        sameContact(P)     全程同一接触点身份
instant     at(t) : expr       某时刻的瞬时谓词（位置、控制量）
```

### 3.0 原子的两个属性：边沿与电平

每个原子必须声明它是边沿还是电平。这不是细节，它决定了谁会强制周期采样。

```text
edge  由输入事件触发        press(channel)、contactBegin、flick
level 只在求值时刻成立      held(channel)、active(region)、contactIn(...)
```

含电平原子的模式必须在电平变化可能被观察到的时刻被求值，因此**周期采样是编译产物，不是
作者声明**。作者只写 `interval held(lane) over [t0,t1]`；编译器判定它含电平原子，于是为它
生成 `every(period)`，周期由 Pattern 的时间界与 Ruleset 的采样声明共同导出。

这是对原设计 D1 的构造性消除：原设计里"该不该采样、什么时候采样"是作者的声明，相位因此
依赖前序输入；现在它是编译结果，相位由锚点与时间界唯一确定。

`gap(A, d)` 是一个派生算子，表示"允许 A 为假的累计时长为 d"：

```text
gap(¬held(lane), grace)     即 Hold 的中途断开
```

它需要声明是**累计**容差还是**单次**容差，两者是不同语义，见 §9。

**Measure 是一个有界折叠算子**：

```text
count(p)          计数
duration(p)       区间长度
sum(expr)         求和
max/min(expr)     极值
last(t) / first(t) 端点时刻
scalar(p, w)      由 p 导出带符号标量（时间误差、方向、距离）
```

**Grading** 把 Measure 映射到 Ruleset 的等级集合，用声明的窗口表。

### 3.1 全部标准类型是组合，不是新增概念

```text
Tap        atom press(lane) · scalar(t - anchor) -> err
Hold       interval held(lane) over [t0,t1] · duration -> coverage
Hold 断开  同一间隔离加 tolerated(¬held, ≤ grace) —— 参数，不是新原语
Slide      sequence of touch(zi) with skip ≤ k · last(t) -> completion time
Bomb       ¬atom press(lane) over [open,close) · none -> avoided/detonated
Catch      instant at(t) : |ctrl.x - x| ≤ w · boolean
Roll       repeat press .. · count -> n
Spinner    interval motion(region) · sum(|cross|) -> angle
Flick      atom press · atom release with scalar(cross(dir, Δ)) -> direction
Arc        interval sameContact(inCorridor) over [t0,t1] · duration
多指 Slide  product of patterns sharing a window   （见 §6 的限制）
```

**Hold 的中途断开**在原设计里需要一整个 `Gap` 状态加两个定时器；在这里是区间谓词的一个
容差参数。这是归约强度的直接体现。

## 4. 编译目标仍是自动机

关键点：Pattern 不是新的运行时机制，它是**有限自动机的源语言**。有界时间正则表达式与
有限自动机等价，编译是机械的。

```text
Pattern  --compile-->  Fold（状态、迁移、guard）
```

于是没有失去可验证性，只是把可写性提高了：

```text
验证在编译后的 Fold 上做，与上一版完全相同
作者写 Pattern，引擎写自动机，作者永远看不到状态
```

**这消除了原设计的一整类缺陷。** 原设计把自动机状态暴露给作者，于是产生了"`level` 链在同一
Tick 内怎么定序"（缺陷 D4）这类问题；作者现在不写状态，就没有这个问题。

## 5. 原缺陷的处置

按 [Gameplay 设计压测与缺陷登记](GAMEPLAY_STRESS_TEST_DRAFT.md) 的编号：

| 原缺陷 | 新设计下 |
| --- | --- |
| D1 采样相位依赖前序输入 | **构造性消除**：采样相位由 Pattern 的时间界导出，时间界由锚点量化一次 |
| D2 timer 表达式冻结时点 | **构造性消除**：作者不再写定时器表达式 |
| D4 `level` 链定序 | **构造性消除**：状态是编译产物，不是作者概念 |
| D5 Controller 采样时点 | 归入 Schedule 的求值语义，一条规则 |
| D7 `consume` 与 `fanout` 矛盾 | 绑定统一为 Pattern 上的一个声明，矛盾消失 |
| D9 槽位复用 | 归入资源绑定的生命周期，一条规则 |
| D3 窗口集单调性 | **保留**，并升格为通用不变量规则（§5.1） |
| D6 取整、D8 投影口径、D10 stray | **保留**，它们是独立的轴 |

六条消失，四条保留。消失的六条里，三条是构造性消除而不是规则补充——这是归约有效的判据。

### 5.1 通用不变量规则

原 D3 是"窗口集在 Hook 作用后必须有序"这个特例。升格为：

```text
每个派生量必须声明它在输入域上的不变量，验证器保守检查。
窗口集有序只是它在窗口集上的实例。
```

这比原缺陷值钱，因为它能挡住将来同类的错，而不是只挡住这一条。

## 6. 已知的限制与风险

三条，必须写明。

**组合激增。** Pattern 的补（`¬`）与积（并发）会让自动机规模指数增长。需要：

```text
声明式规模上界，编译后超过上界即拒绝
补与积的使用计入 capability
```

这不是可以绕开的问题：Bomb 用补，多指用积。

**多接触点是唯一真正吃力的地方。** 多指 Slide 要求 n 个接触点同时满足各自的子模式。正则可
表达为积自动机，但作者侧语法会变重。缓解：提供一个命名的并发组合子，并把它限定为"共享
同一窗口的定长子模式并集"，不做无限并发。

**这是一次重写，不是修订。** L2 草案的 §2–§4 会被整体替换。已完成的裁决（几何原语、归属与
可见性、重采样、数值模型、frame）全部保留，但它们的位置改变：从"原语"变成"原子与算子的
定义域"。

## 7. 判据：如何知道归约做够了

```text
消去测试   对每个原语问：它的全部现有用法能否用其余原语组合表达？能就删掉它
派生测试   对每个案例问：它是组合出来的，还是需要一条点名提到它的规则？
           后者就是特例，要么核心缺概念，要么它属于标准库
```

**停止条件：新案例只需要新增标准库条目，从不需要改动核心。**

按 §3.1 检查，十二个主要案例全部可由算子的组合表达。**但这十二项是已经实际写出来的，剩
下二十二项尚未逐项用算子重写**——这是本提案唯一未经完整验证的声明，见 §9 待决 1。

诚实地说，这一条应当由一次"只许用算子组合"的完整重写来确认，而不是由列举来确认。列举只能
说明方向正确，不能证明封闭性。这正好是下一轮该做的事，也是本提案相对于继续加案例的差别：
下一轮的目标是**证明封闭**，不是继续收集。

## 8. 收益与代价

```text
收益  核心概念 15 -> 3（Fold / Pattern / Measure）
      10 条缺陷中的 6 条消除，其中 3 条是构造性消除
      作者不再面对状态机，Studio 可以可视化 Pattern 而不是自动机
      验证机制几乎原样保留，编译后仍在 Fold 上做

代价  一次性重写 L2 草案的 §2–§4
      补与积的规模控制需要新的预算条目
      多接触点语法的复杂度上升
      标准库成为必需品：核心太低层，作者必须通过标准库工作
```

最后一条是真实的：正交核心的代价是**作者侧必须靠标准库**。这是可接受的，因为它正是"低成本
表达绝大多数语义"的实现方式——绝大多数人写标准库条目，少数人扩展核心。

## 9. 待决

1. Pattern 的完整算子集合需要冻结。§3 的八项是推导结果，需要再压一轮"只许用算子组合"的
   练习来确认没有遗漏。
2. 补与积的规模上界取值，以及它们的 capability 划分。
3. Measure 的算子集合与 Pattern 是否共享表达式核（应当共享，与 L3 的表达式核是同一个）。
4. 标准库是否随引擎发行、随 Ruleset 包发行，还是两者都可以提供。
5. 与 L3 的关系：L3 的 fold 是否也用 Pattern/Measure 表达，还是保持"事件处理器 + 有界 for"。
   倾向后者：L3 的输入是 Fact 流，不是有界的输入轨迹，Pattern 的时间界概念不适用。
6. `gap(A, d)` 必须声明是**累计**容差还是**单次**容差。两者是不同语义：

```text
累计  A 为假的累计总时长不超过 d          适合"总时长受限"
单次  连续为假的单段时长不超过 d          适合 Hold 的宽限时间
```

   Hold 要的是单次（松手一次不能超过 grace，但可以松很多次）；某些"整体覆盖率"型判定要的
   是累计。两者不能共用一个算子名，因此这一条决定了 `gap` 是一个算子还是两个。

7. 由周期采样引入的单次容差，其**实际余量与相位**由 §3.0 的对齐规则决定，该规则需要与
   Ruleset 的采样声明一起冻结。这条是原 D1 的残留部分：构造性消除覆盖了"要不要采样"，但
   "采样格点对齐到哪里"仍是一条需要写死的规则。
