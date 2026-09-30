# Bounded Fold Calculus 提案（第二版）

状态：candidate（设计提案，未接受，未实施）

更新日期：2026-10-01

本文是对 [Gameplay Program IR 原语草案](GAMEPLAY_PROGRAM_IR_DRAFT.md) 的**替换性重设计**，
不是增量修订。动机见 [Gameplay 设计压测与缺陷登记](GAMEPLAY_STRESS_TEST_DRAFT.md) §5：原语数量
在收敛（10 → 3 → 0），但同期 10 条缺陷里有 6 条是语义未定义，每修一条就加一条规则。这说明
枚举的是原语，没有枚举规则。

本提案把生成集合从约 15 个概念压到 3 个。

**第二版是 [封闭性验证](GAMEPLAY_CALCULUS_CLOSURE_CHECK.md) 的结果。** 第一版通过了骨架检验
（34 项案例无一需要新核心概念），但丢掉了三样旧设计已有的关系性语义。第二版把它们补回，
删掉了第一版多余的一项，并新增了格式编码与采样敏感性两条。修订记录见 §11。

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

## 2. 核心

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

### 2.1 核心之外：关系性语义不在此列

第一版试图把一切都归约进核心，结果丢掉了三样东西。它们的共同点是**都不是"判定一个音符"的
语义，而是"多个音符之间"的语义**：

```text
仲裁       多条 requirement 争同一个输入          见 §5.1
归属       资源（接触点）的分配与可见性            见 §5.2
匹配策略   一个 Pattern 的多种合法匹配             见 §3.4
```

**正交化的边界应当在"单实体"与"跨实体"之间，而不是在"简单"与"复杂"之间。** 核心只描述
单条 requirement 的内部结构；凡是需要同时看两条以上 requirement 或两种以上匹配的语义，
一律留在核心之外，作为显式声明。

这不是退让，而是把边界划在正确的地方：核心可以做到同时正交与完备，前提是它只声称描述单实体。

## 3. 作者层：Pattern / Measure / Grading

```text
Requirement = (Pattern, Measure, Grading)

Pattern   有界时间模式：必须在输入轨迹里出现什么
Measure   对被匹配区间的有界折叠，可以是多个分量的积
Grading   按分量把 Measure 映射到 Ruleset 的等级集合
```

**Pattern 是一个有界时间正则表达式**，原子是轨迹谓词。算子集合经归约后是：

```text
atom(p, extent)       轨迹谓词在给定时间范围成立，extent ∈ {point, interval}
sequence   P · Q      先后
alternation P | Q     任一
repeat     P{lo, hi}  有界重复；hi 可为 ∞，由窗口给出上界
negation   ¬P         补
```

四个时序算子，加两项声明：

```text
bind       P as v     绑定变量（接触点、区域、通道），供守卫比较
strategy   leftmost-first | leftmost-longest        见 §3.4
```

**这个签名就是带补的 Kleene 代数**（`+` 择一、`·` 序列、`*` 重复、补），也是正则语言的标准
最小签名。空串用 `P{0,0}` 表达，通配用 `p | ¬p` 表达，两者都是派生。

**这不是"试了几个算子"，而是可达最小性**：给定正则表达力与补，这四个算子无法再删，删任何一个
都会掉出这个表达力类。归约过程见
[封闭性验证](GAMEPLAY_CALCULUS_CLOSURE_CHECK.md) §7。

### 3.1 算子的最小签名（第三版新增）

初稿的九项里有五项不是独立算子：

```text
interval    并入 atom —— extent = interval
instant     并入 atom —— extent = point，配电平谓词
skip        降级为语法糖 —— 等价于 · repeat 与通配的组合
sameContact 重新定义 —— 它不是时序算子，是变量绑定
```

前三项是同一件事的不同写法。`skip` 的具体展开：

```text
P .. Q within k   ≡   P · (any){0, k} · Q，其中 any ≡ p | ¬p
```

**`sameContact` 的重新定义值得单独说明。** 它约束的是"这次匹配绑定的是哪个接触点"，而不是
"什么时候发生了什么"——属于绑定，不属于时序。原来写成包裹算子，只覆盖了"全部原子同一接触点"
一种情形。改成显式绑定之后：

```text
inCorridor as c                  绑定走廊匹配到的接触点
interval (inCorridor as c) over [t0,t1]
  同一性由 c 的重复使用表达，不需要特殊算子
```

这比原写法更宽也更简单：区域与通道同样可以绑定与比较，而不只是接触点。

### 3.2 原子的两个属性：边沿与电平

每个原子必须声明它是边沿还是电平。这不是细节，它决定了谁会强制周期采样。

```text
edge  由输入事件触发        press(channel)、contactBegin、flick
level 只在求值时刻成立      held(channel)、active(region)、contactIn(...)
```

含电平原子的模式必须在电平变化可能被观察到的时刻被求值，因此**周期采样是编译产物，不是
作者声明**。作者只写 `atom(held(lane), interval[t0,t1])`；编译器判定它含电平原子，于是为它
生成 `every(period)`。

**"是否需要采样"与"采样周期取值"是两个问题，来源不同**（第二版修订，见 §11）：

```text
是否需要采样     编译产物。含电平原子即需要。这是对 D1 的构造性消除
采样周期取值     Ruleset 声明基线与允许区间，内容在区间内覆盖（讨论记录第 30 条）
两者关系         编译器检查声明的周期足以支撑 Pattern 的容差参数；
                 不足则拒绝该内容，并给出稳定诊断
```

第二版曾把两者合并为"周期由 Pattern 的时间界与 Ruleset 的声明共同导出"，这与第 30 条冲突。
现在按上表划分：构造性消除针对的是"决定是否采样"这件事，而周期的**取值**保留游戏作者与
谱面作者的自由。相位由锚点与时间界唯一确定，与前序输入无关。

### 3.3 Pattern 必须落在声明的窗口内

requirement 的 `arm` 与 `deadline` 由参数给出，Pattern 必须只在该窗口内匹配。这是验证规则：

```text
编译后自动机的全部接受路径必须落在 [arm, deadline] 内，否则拒绝该 requirement
```

缺少这条检查时，一个自相矛盾的 Pattern（要求窗口外的匹配）会让实现者得到两种不同行为：
"永远不满足"或"在窗口外满足"。两者都不可接受。

### 3.4 匹配策略

`skip`、`repeat`、`alternation` 会让同一条输入轨迹产生**多个合法匹配**，而 Measure 取决于
选中的那一个。

```text
输入    A B A B
模式    A .. B within 1
匹配    [A1,B1] / [A1,B2] / [A2,B2]     三个都合法
```

若 Measure 是 `scalar(t_B − anchor)`，三个匹配给出三个不同的值。**判定结果不唯一。**

因此匹配策略是必需声明，不是实现细节：

```text
leftmost-first    从左到右，每步取最早可行分支。可流式实现，判定默认
leftmost-longest  取最长匹配。需要回看，实现代价高
```

判定选 `leftmost-first`。策略进入 Interface 与判定 identity，因为它改变结果。

第一版丢掉了这条规则，而它属于原设计确已明确写过的那类语义（"推进到第一个满足的下标"）。
这与 D1–D10 是同一类错误：一条没写的语义规则导致结果不唯一，且只在特定输入序列下暴露。

### 3.5 容差算子的两种型

```text
gap   允许 A 为假的累计时长为 d
hole  允许 A 连续为假的单段时长不超过 d
```

Hold 要的是 `hole`（松手一次不能超过 grace，但可以松很多次）；整体覆盖率型判定要的是
`gap`。两者不能共用一个算子名，因为它们是不同语义。

### 3.6 全部标准类型是组合，不是新增概念

```text
Tap        atom(press(lane), point) · scalar(t − anchor)
Hold       atom(held(lane), interval[t0,t1]) with hole(¬held, grace) · release
           Measure: (head = scalar(t_press − t0), coverage = duration(held)/(t1 − t0))
Slide      atom(touch(z0), point) · any{0,k} · atom(touch(z_last), point) · last(t)
Bomb       ¬atom(press(lane), interval[open, close)) · none
Catch      atom(|ctrl.x − x| ≤ w, point) · boolean
Roll       repeat atom(press, point){0,∞} ⊆ [t0,t1] · count
Spinner    atom(motion(region), interval) · sum(|cross|)     （采样敏感，见 §7.3）
Flick      atom(press, point) · atom(release, point) · scalar(cross(dir, Δ)) · direction
Arc        atom(inCorridor as c, interval[t0,t1]) · duration
多指 Slide n 条独立 requirement + fold 聚合                  （无需积，见 §3）
```

其中 `any ≡ p | ¬p` 是通配，用于 `Slide` 的跳区。

**Hold** 在原设计里需要一个 `Gap` 状态加两个定时器；在这里是一个容差算子加一个双分量
Measure。这是归约强度的直接体现。

**多阶段计量**（V2 的修补）体现在 Hold 的两个 Measure 分量上：头按时间窗口评级，体按覆盖率
评级，各自带 `phase` 与 `category`，各自发一条 Fact。这恢复了原设计 `Hit{phase, grade,
errorTicks}` 的能力，且没有增加核心概念——只是允许 Measure 并列。

## 4. 编译目标仍是自动机

Pattern 不是新的运行时机制，它是**有限自动机的源语言**。有界时间正则表达式与有限自动机等价，
编译是机械的。

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

## 5. 核心之外的三件声明

第二版的核心边界见 §2.1。三件关系性语义各有归属。

### 5.1 仲裁（V1 的修补）

**仲裁不能被 Pattern 吸收。** 判定区重叠的本质是"一次按下，两条 requirement 的 Pattern 都
匹配，只能有一条结算"，这是 requirement 之间的全局性质，不是任何单条 Pattern 的性质。

第一版把整节仲裁丢掉了，属遗漏。第二版恢复为 **Ruleset 级声明**：

```text
candidates  consume | observe
order by    claimKey（由 Pattern 声明的排序键）, requirementId
lock        channel: oldest_unsettled
fanout      equal_anchor: all
```

它只做过滤与排序，不读其他实例的内部状态，因此程序之间保持零耦合——这条性质与第一版一致。

### 5.2 归属与可见性（V4 的修补）

变量绑定（Pattern 层）与"绑定资源"（Effect 层）是两层，必须分开说明：

```text
bind 变量     匹配约束：这一条匹配必须由同一个接触点（或区域、通道）完成
绑定资源      资源分配：认领后独占消费权
归属裁决      全局分配：落在 §5.1 的仲裁，不在 Pattern 内
```

旧草案的 `sameContact` 是绑定的一种特例。它在第二版被重新定义为通用变量绑定（§3.1），
两者通过 `bind` 统一：Pattern 里的 `x as c` 约束匹配，Effect 里的 `claim` 分配归属，
Guard 里的使用决定是否接受。

可见性规则沿用并保留：**观察型边始终可见**。已被认领的接触点对 `observe` 边仍然可读，
所以 `sticky` 类型的判定（换手即断开）才能**看到但拿不到**，从而判出断开。

### 5.3 格式编码（V6 的修补）

requirement 从一个定长元组（kind + interval + domain + action + constraints）变成 Pattern AST，
体积明显变大，而后者参与既有的 40k 实体 / 16 MiB 容量门禁。编码取：

```text
requirement := (标准库条目 ID | 内联 Pattern AST) + 参数 + 计量规格
```

```text
标准库条目   绝大多数谱面：一个 ID 加几个参数，比旧元组还小
内联 AST     自定义 Pattern：较大，单列预算与 capability
```

因此常见情形不会退化，容量风险只落在自定义 Pattern 上，可以独立计量与拒绝。

### 5.4 Fact 的形状（W4 的修补）

Measure 可以是"无"（炸弹的不触发分支）。此时 Fact 仍必须携带结果标识，只是没有等级与误差：

```text
outcome   必需。结算结果，取 Ruleset 声明的枚举
grade     可选。由 Grading 从 Measure 导出
measure   可选。Measure 的值，用于诊断与统计
phase     可选。多分量 Measure 时标识是哪一个分量
category  必需。取 Ruleset 声明的类别
```

因此"无 Measure"不是不产生 Fact，而是产生一条只有 `outcome` 与 `category` 的 Fact。

### 5.5 不可表达的一类（W1）

跨 requirement 约束（W1）**不进入设计**，并在此明确登记为"不可表达"，而不是"未实现"。

```text
W1  两条 requirement 之间必须存在关系，而不是各自独立可判
    例如：同一接触点同时完成两条、两手必须交替、一次滑动经过两个区域
```

它不可表达的原因**是结构性的，不是能力不足**：

```text
仲裁只决定"谁得到输入"，不产生"谁必须与谁共享"
归属是独占的，恰恰不允许两条 requirement 共用一次消费
折叠只看到结算后的 Fact，无法反过来约束匹配
```

近似方式：用同一接触点的多次认领与释放模拟，或合并成一条多阶段 requirement。

**登记为"不可表达"而不是"未实现"，是一项有意的设计选择**，它带来三个后果：

1. 目标游戏若确实需要这类语义，必须在设计阶段就发现，而不是在实现阶段。
2. 文档与 Studio 可以明确告诉作者"这条做不到"，而不是让作者撞墙。
3. 将来若决定支持，它需要新的 ADR——因为它改变的是核心边界，不是增加一条原语。

### 5.6 谓词读取集合（第二版新增）

原子与 instant 谓词能读什么，必须显式规定，否则"某个语义能否表达"没有判据。

```text
params   谱面提供的参数，prepare 冻结       必需
event    当前事件载荷                       必需
state    本 requirement 的状态              必需
ctrl     控制器采样                         必需
level    电平查询                           必需
hook     Ruleset 声明的程序可见 Hook        必需（见下）
```

**`hook` 必须在集合内。** 它恢复了原设计已有的能力，而代价已经付过：Skill Hooks 草案已经
定义了程序可见 Hook、静态取值范围与 Interface 投影。

由此得到一条重要的表达能力：**链式依赖可表达。**

```text
A settle(miss)
  -> fact(category: note, outcome: miss)
  -> L3 handler 置 hook.priorMissed
  -> B 的 instant predicate 读 hook.priorMissed
```

按 t+1 规则，A 的锚点早于 B 时，Hook 在 B 开机时已经就位。因此"前一个音符未命中则后一个
失效"这类跨音符影响不需要新机制。

两条限制必须写明：

```text
Hook 必须在 Interface 中已声明    因此谱面不能自创跨 requirement 通道，
                                  只能使用 Ruleset 提供的那一条
影响延迟一个 Tick                 跨音符的时序细节受此约束
```

**W1 与链式依赖的区别因此是清楚的**：前者要求两条 requirement 之间存在**关系**，没有任何
通道；后者是一条的**结果**影响另一条，可以经由 Hook。第二版初稿把两者并列为"不可表达"，
是逐条判断造成的误判，见 [封闭性验证](GAMEPLAY_CALCULUS_CLOSURE_CHECK.md) §6.6。



requirement 从一个定长元组（kind + interval + domain + action + constraints）变成 Pattern AST，
体积明显变大，而后者参与既有的 40k 实体 / 16 MiB 容量门禁。编码取：

```text
requirement := (标准库条目 ID | 内联 Pattern AST) + 参数 + 计量规格
```

```text
标准库条目   绝大多数谱面：一个 ID 加几个参数，比旧元组还小
内联 AST     自定义 Pattern：较大，单列预算与 capability
```

因此常见情形不会退化，容量风险只落在自定义 Pattern 上，可以独立计量与拒绝。

## 6. 原缺陷的处置

按 [Gameplay 设计压测与缺陷登记](GAMEPLAY_STRESS_TEST_DRAFT.md) 的编号：

| 原缺陷 | 新设计下 |
| --- | --- |
| D1 采样相位依赖前序输入 | **构造性消除**：采样相位由 Pattern 的时间界导出，时间界由锚点量化一次 |
| D2 timer 表达式冻结时点 | **构造性消除**：作者不再写定时器表达式 |
| D4 `level` 链定序 | **构造性消除**：状态是编译产物，不是作者概念 |
| D5 Controller 采样时点 | 归入 Schedule 的求值语义，一条规则 |
| D7 `consume` 与 `fanout` 矛盾 | 统一在 §5.1 的仲裁声明里，矛盾消失 |
| D9 槽位复用 | 归入 §5.2 的资源绑定生命周期，一条规则 |
| D3 窗口集单调性 | **保留**，并升格为通用不变量规则（§6.1） |
| D6 取整、D8 投影口径、D10 stray | **保留**，它们是独立的轴 |

六条消失，四条保留。消失的六条里，三条是构造性消除而不是规则补充。

### 6.1 通用不变量规则

原 D3 是"窗口集在 Hook 作用后必须有序"这个特例。升格为：

```text
每个派生量必须声明它在输入域上的不变量，验证器保守检查。
窗口集有序只是它在窗口集上的实例。
```

## 7. 已知的限制与风险

### 7.1 补的规模

补保留，它的规模可控：在区间谓词上就是"区间内原子恒假"。爆炸式增长的主要来源（积）已在
§3 删除。

### 7.2 多接触点仍是语法最重的地方

多指判定现在表达为 n 条独立 requirement 加 fold 聚合，语义清晰，但作者要写 n 条。缓解：提供
一个命名的语法糖，把它展开为 n 条，**糖不改变语义，也不改变自动机规模**。

### 7.3 Measure 对采样周期敏感（V7 的修补）

Spinner 的 `sum(|cross|)` 是**按采样点求和**，它逼近的是弧长，逼近精度取决于声明的采样周期。
周期进入 Interface 与 identity，所以结果是确定的，但**同一谱面在不同采样周期下会得到不同的
Spinner 评级**。

这不是非确定性，是近似敏感性。处置：在 Pattern 层显式标注"该 Measure 对采样周期敏感"，并
写进作者文档；Ruleset 若要固定评级，就把周期区间声明为单点。

### 7.4 这是一次重写

L2 草案的 §2–§4 会被整体替换。已完成的裁决（几何原语、归属与可见性、重采样、数值模型、
frame）全部保留，但位置改变：从"原语"变成"原子与算子的定义域"。

## 8. 判据：如何知道归约做够了

```text
消去测试   对每个原语问：它的全部现有用法能否用其余原语组合表达？能就删掉它
派生测试   对每个案例问：它是组合出来的，还是需要一条点名提到它的规则？
           后者就是特例，要么核心缺概念，要么它属于标准库或核心外的声明
```

**停止条件：新案例只需要新增标准库条目或核心外的声明，从不需要改动核心。**

第一版按此判据通过（34 项案例无一需要新核心概念）。第二版尚未重跑，见 §10。

## 9. 收益与代价

```text
收益  核心概念 15 -> 3（Fold / Pattern / Measure）
      10 条缺陷中的 6 条消除，其中 3 条是构造性消除
      作者不再面对状态机，Studio 可以可视化 Pattern 而不是自动机
      验证机制几乎原样保留，编译后仍在 Fold 上做
      删掉积，第一版最大的风险项消失

代价  一次性重写 L2 草案的 §2–§4
      核心之外多了三件声明（仲裁、归属、匹配策略）
      多接触点语法最重，需要语法糖
      标准库成为必需品：核心太低层，作者必须通过标准库工作
```

**对净收益的诚实说明。** 第一版声称"消除 6 条缺陷"，这个数字成立，但它同时新引入了 3 条
（仲裁缺失、多阶段计量缺失、匹配策略缺失），净收益被高估。第二版补上这三条之后，核心仍是
三个概念，但**核心外的声明面增加了**。

所以真实的结论是：**作者侧的复杂度确实降下来了，引擎侧的规则没有同比例减少。** 归约的目标
"低成本表达绝大多数语义"达成了一半——达成的是"作者低成本"，未达成的是"引擎低成本"。

## 10. 待决

1. **第二版已重跑封闭性验证**，结果见
   [封闭性验证](GAMEPLAY_CALCULUS_CLOSURE_CHECK.md) §6。四条 V 缺口全部修复且无回归，
   核心骨架维持。
2. **逐类核对发现上一轮的处置有误。** 链式依赖（L4）曾被与 W1 并列归入"不可表达"，但经由
   Hook 它是可表达的（§5.6）。§5.5 的排除清单现在只保留 W1。

   §6.5 的六类清单中，第 4 类排除、第 5 类可表达，其余四类已声明。
3. **Pattern 的算子集合已经归约到最小签名**（§3）：四个时序算子（序列、择一、有界重复、补）
   加两个声明（绑定、匹配策略）。归约过程见
   [封闭性验证](GAMEPLAY_CALCULUS_CLOSURE_CHECK.md) §7。

   仍待冻结的是**签名的编码**，不是签名本身：`atom` 的 `extent` 取值、`repeat` 的无穷上界与
   窗口的关系、`bind` 对哪些原子类型适用。
4. 补的规模上界与 capability 划分。
5. Measure 的算子集合与 Pattern 是否共享表达式核（应当共享，与 L3 的表达式核是同一个）。
6. 标准库是否随引擎发行、随 Ruleset 包发行，还是两者都可以提供。
7. 与 L3 的关系：L3 的 fold 是否也用 Pattern/Measure 表达，还是保持"事件处理器 + 有界 for"。
   倾向后者：L3 的输入是 Fact 流，不是有界的输入轨迹，Pattern 的时间界概念不适用。
   但 §5.6 的 Hook 通道依赖 L3 能写 Hook，这条通道需要与 L3 草案核对。
8. 谓词读取集合纳入 `hook` 之后，需要在 Ruleset Interface 里明确哪些 Hook 对谱面可见。
9. W2 的划分已并入 §3.2；W3 已并入 §3.3；W4 已并入 §5.4。

## 11. 第二版修订记录

```text
删除  积（验证发现对全部 34 个案例多余）
保留  补，并说明其规模可控
增加  匹配策略声明，默认 leftmost-first（原 V3）
增加  Measure 允许并列，Grading 按分量应用，分量自带 phase 与 category（原 V2）
增加  仲裁作为 Ruleset 级声明，并说明它不属于核心（原 V1）
增加  归属与可见性、sameContact 的两层关系（原 V4）
增加  requirement 的格式编码（原 V6）
增加  Measure 采样敏感性的标注（原 V7）
增加  §2.1 核心边界原则：在"单实体"与"跨实体"之间划界
增加  §5.6 谓词读取集合，并把 hook 纳入
修正  §9 对净收益的表述，承认第一版高估了收益
修正  L4 的处置从"不可表达"改为"经由 Hook 可表达"（逐类核对的产出）
```

## 12. 第三版修订记录

```text
归约  算子集合从九项降到四个时序算子加两个声明
删除  interval  并入 atom 的 extent 参数
删除  instant   并入 atom 的 extent 参数
删除  skip      降级为语法糖，等价于 · repeat 与通配的组合
重定义 sameContact -> 通用变量绑定（它不是时序算子，是绑定）
保留  sequence / alternation / repeat / negation，构成带补的 Kleene 代数
说明  可达最小性：给定正则表达力与补，这四个算子无法再删
```
