# Gameplay Program IR 原语草案

状态：candidate（部分被取代，见「取代关系」；未接受，未实施）

更新日期：2026-10-01

本文回应 [Gameplay Ruleset 设计讨论记录](GAMEPLAY_RULESET_DISCUSSION.md) 待讨论项 1：定义判定
程序的执行模型、原语集合和静态验证规则，并用案例压测表达力。文中语法只用于说明，不是
最终编码；最终 IR 是类型化数据，作者语法另行讨论。

## 取代关系

[Bounded Fold Calculus 提案](GAMEPLAY_FOLD_CALCULUS_DRAFT.md) 是本文**部分内容的替换性
重设计**。本文**不整体废弃**：它承载的输入模型、表现绑定、静态验证与详细案例仍然有效，且被
calculus 依赖。

逐节处置如下。阅读顺序建议：先读 calculus 得到核心概念（Fold / Pattern / Measure），再回本文
读"仍然有效"的部分；"已被取代"的部分只作为设计过程记录保留。

| 本文节 | 处置 | 说明 |
| --- | --- | --- |
| §1 设计约束 | **仍然有效** | calculus 沿用全部五条约束 |
| §2 执行模型 | **已被取代** | Fold 与 Schedule 取代"程序 + 触发器"；Tick 内顺序仍有效 |
| §3 程序结构 | **已被取代** | `Program` 结构由 Fold 与 Pattern 取代 |
| §3.1 grip | **仍然有效** | 保持策略，calculus §5.2 引用 |
| §4 原语 | **已被取代** | 触发器、守卫、动作由 atom 与四个时序算子取代 |
| §4.3 几何原语 | **仍然有效** | 作为原子的谓词集合保留 |
| §4.5 归属与可见性 | **仍然有效** | calculus §5.2 引用并扩展为变量绑定 |
| §4.6–4.8 Fact / 折叠表 / 可观察寄存器 | **仍然有效** | calculus §5.4 引用 |
| §5 空间状态与输入 | **仍然有效** | 输入模型、重采样、Controller、frame、数值模型全部保留；§5.6 新增加法与不变量 |
| §6 表现绑定（L4） | **仍然有效** | 与程序模型无关，calculus 未触及 |
| §7 仲裁 | **内容有效，位置已变** | 提升为 Ruleset 级声明，见 calculus §5.1 |
| §8 案例压测 | **仍然有效** | 十二个案例的详细展开；calculus §3.6 只给算子形式 |
| §9 静态验证 | **仍然有效** | calculus 明确"验证在编译后的 Fold 上做，与上一版完全相同" |
| §10 结论与待决 | **部分过时** | 缺口处置已被 calculus 重新表述 |

**未被取代的三块内容是 calculus 的依赖项**：§5 定义原子与算子的定义域（谓词能读什么、几何
如何求值），§6 定义表现层边界，§9 定义验证规则。calculus 只替换了"如何描述单条
requirement 的内部结构"这一层。

## 1. 设计约束

```text
程序不可信          全部验证在 prepare 完成，演奏中不出现"程序错误"
必然结算            每个实例在静态可知的时刻前到达终态
状态有界            寄存器数量与取值范围固定，可快照、可 Seek
读写受限            只读：输入观察、时间、Controller、frame、Hook；只写：自身寄存器与 Fact
决策侧整数          程序中没有浮点、Beat 或宿主类型；作者的浮点数据在 prepare 量化一次
```

## 2. 执行模型

### 2.1 时间

`Tick` 是 chartTime 域内的 i64 微秒。Chart 锚点（Beat）在 prepare 时经 TimingMap 转为
chartTimeMs，再按唯一的舍入规则量化为 Tick，只量化一次。InputEvent 在映射时量化为 Tick。
程序看不到 Beat 和 double。

### 2.2 实例与生命周期

Chart 中每个 Requirement 对应一个 ProgramInstance：`(program, 冻结参数, 寄存器, 当前状态)`。

```text
Dormant --armTick--> Live(state) --settle(outcome)--> Settled
```

`armTick` 与 `hardDeadline` 是参数和 Ruleset 静态上界的仿射表达式，不读 Hook，因此在 prepare
时即可算出，用于活动实例数的扫描预算。技能对窗口的缩放必须落在 Ruleset 声明的静态上界之内。

`settle` 的结果是 Ruleset Interface 声明的枚举。引擎只要求"每个实例必须在 deadline 前结算"，
不规定哪个结果算好、哪个算坏。普通音符的 `hit`/`miss` 和炸弹的 `avoided`/`detonated` 是同一种
机制的不同取值。

### 2.3 同一 Tick 内的求值顺序

判定是离散事件模拟。同一 Tick `t` 内的固定顺序为：

1. 激活 `armTick == t` 的实例（按 Requirement identity 排序）
2. 触发到期的定时器与采样唤醒（按实例 identity，再按边的声明顺序）
3. 按 sequence 顺序派发 `t` 时刻的输入事件（见第 7 节仲裁）
4. Ruleset 按发出顺序折叠第 1–3 步产生的 Fact，并更新 Hook 值

程序在 `t` 时刻读取的 Hook 值，是第 `t-1` 步结束时的值。因此 `t` 时刻产生的 Fact
（比如技能被触发）最早在 `t+1` 时刻影响判定。因果环在结构上无法闭合。

Controller 在 `t` 时刻的采样值，只由严格早于 `t` 的输入决定。

### 2.4 周期采样

连续谓词在两次输入事件之间可能由真变假（接触点不动而走廊移动）。这类变化不产生输入事件，
所以程序不会被唤醒。修法是**在状态上声明周期采样**：

```text
state Track
  every(period)
  level
    guard contactIn(c, corridor)
  level
    guard !contactIn(c, corridor)
    do    emit Break(body, T); settle(broken)
```

`every` 与 `level` 是同一套重求值机制的两个触发源，共享同一条边求值路径。进入该状态时
调度第一次唤醒，离开状态或结算时取消。

**周期由两层声明确定，不写死。**

```text
Ruleset   声明基线 samplePeriod 与允许区间 [lo, hi]
内容      谱面或程序状态在允许区间内声明覆盖值
Interface 声明区间与静态上界，用于预算
```

允许区间由 Ruleset 定界，因此游戏作者保留完全控制权：想固定就声明单点区间，想放开就给范围；
谱面作者在范围内有自由。引擎只在 prepare 时检查范围，不解释具体数值。

采样周期影响判定结果（早采样一刻就可能早判出断开），所以它必须是**声明过的**，这也是它
必须进入 §4.1 的 Interface 投影与判定 identity 的原因。

**`level` 的触发条件是"本实例自身状态被写入后"重求值**，不只是状态机转移后。否则 Slide
连续跨过几个判定区时链不上。

**`every` 不能替代解析求交，也不能被解析求交替代。** 接触点线性运动、区域静态时，越界时刻
可以用整数除法精确解出。但若引擎精确求交，"断开时刻"就依赖引擎能力；按采样定义则只依赖
声明的格点。后者才与"结果只依赖声明过的东西"一致。因此解析求交将来只能是 Ruleset 声明的
**另一种模式**（带 capability），不能是同一模式下的实现优化，因为两者结果不同。

这是对 §5.1 一处论证的修正：该处曾论证不需要采样，理由是线性插值可以回答任意时刻的查询。
插值确实能回答查询，但它**不产生事件**，程序不会被唤醒。

## 3. 程序结构

```text
Program {
  params     类型化参数，由谱面提供，prepare 时冻结
  registers  有界寄存器，初值为常量
  states     有限状态集合，其中一个为初始状态
  edges      每条边：trigger + guard + actions + target
  arm        激活时刻表达式
  deadline   hardDeadline 表达式，以及必须提供的 onDeadline 动作
  claimKey   仲裁排序键表达式
  dispatch   consume | observe
  grip       sticky | handoff
  category   Ruleset Interface 声明的类别（note / penalty / bonus / filler 等）
}
```

参数和寄存器可以使用以下类型：

| 类型 | 说明 |
| --- | --- |
| `Tick` | i64 微秒 |
| `Int[lo..hi]` | 声明了取值范围的整数；越界在验证阶段按区间分析拒绝 |
| `Bool` | 布尔值 |
| `Enum<E>` | Ruleset Interface 中声明的枚举 |
| `Channel` / `Region` | Ruleset Interface 中声明的判定域引用，必须能解析 |
| `Vec2` | 整数坐标或向量，分量范围由声明的量化单位决定 |
| `Array<T, N>` | 定长数组，静态上界 N，只能用常量或有界下标访问 |
| `Contact` | 仅限寄存器，保存已认领的接触点句柄 |

### 3.1 grip：保持策略

`grip` 声明认领之后中途能否更换接触点。它是判定语义，不是引擎路由规则：

```text
sticky    认领后不可换手。接触点结束即断开，不能由另一个接触点接续
handoff   允许换手，中间受宽限时间约束
```

Arcaea 的 Arc 是 `sticky`：一旦跟随，换指视为断开，所以它永不 `release`。
§8.2 的 Hold 是 `handoff`：它的 `holdGrace` 是这条策略的参数，不是通用机制。

## 4. 原语

### 4.1 触发器

| 触发器 | 语义 |
| --- | --- |
| `input(kind, domainFilter)` | 边沿输入：press、release、contactBegin、contactMove、contactEnd、axisStep |
| `timer(expr)` | 在绝对 Tick `expr` 触发。`expr` 只能读参数、寄存器和 Hook，并且必须 ≤ deadline |
| `level` | 本实例发生状态变化后立即重新求值，用于电平条件。连续触发次数有静态上界 |

每次输入派发有两种方式，由 Requirement 自己声明。不能按判定域声明：同一轨道或同一判定区里
同时存在要抢输入的普通音符和不该抢输入的炸弹，按域划分无法区分二者。

- `consume`：参与仲裁竞争。经排序后，一次输入最多交给一个 consume 实例。典型是按键或触摸
  的按下。
- `observe`：不参与竞争。事件同时交给所有监听者，不消耗输入。典型是炸弹、maimai 传感器的
  开关，以及共用同一次触摸的多条 Slide。

Ruleset 可以强制某个判定域整体为 `observe`，用于触摸区域这类输入，但不能把 `observe`
改成 `consume`。程序不能声明自己为 consume 却绕过仲裁。

### 4.2 守卫表达式

守卫是纯表达式，没有循环，也没有调用。可用的内容：

```text
算术          + - * min max abs clamp（checked，溢出在验证阶段按区间分析拒绝）
比较与逻辑    < <= == && || !
事件载荷      evt.t, evt.channel, evt.region, evt.contact, evt.pos
电平查询      held(channel), active(region), contactIn(contact, region)
Controller    ctrl.<name>.<field>        当前 Tick 的解析采样值
Hook          hook.<name>、win(<set>).<grade>.early/late
有界量词      some k in 0..K : expr      K 为静态常量或有界参数；动作中用 firstk 取回最小满足下标
```

### 4.3 几何原语

方向类判定需要向量运算。守卫语言补一组几何原语，全部是整数乘加，保持闭式与可判定：

```text
vec(x, y)            由两个整数分量构造向量
posAt(contact, t)    接触点在时刻 t 的位置，用 L1 的线性插值求值
sub(a, b)            差向量
dot(a, b)            内积
cross(a, b)          二维叉积，符号用于判转向与判方向
len2(a)              长度平方，避免开方
```

约定与范围：

- 分量是整数，单位由判定域声明的量化单位给出（§5.5）。
- `len2` 与平方后的阈值比较，不做开方。需要真实距离时可与 `sqrtApprox(x, N)` 配合，
  它是固定 N 次迭代的整数平方根近似；不使用浮点，也不调用平台数学库。
- `cross` 的符号即转向：正值表示逆时针，负值顺时针，零表示共线。
- 这些原语只读输入与参数，不读 Controller，也不读表现层。

方向判定的三类用法：

```text
flick 方向     cross(sub(posAt(c, tEnd), posAt(c, tBegin)), dirAxis) > 0
Spinner 累积   sum over moves of cross(sub(p1, p0), sub(p2, p1))   按符号判别累积
沿弧方向       dot(sub(posAt(c, t), path(t)), pathDir(t)) > 0
```

第一条是本轮新增的能力；第二条需要把每次 `move` 的叉积累加进一个有界寄存器，第三、四章
已具备该能力。

### 4.4 动作

| 动作 | 语义 |
| --- | --- |
| `set r := expr` | 写入本实例的寄存器 |
| `claim(evt.contact, slot)` | 把接触点认领到本实例的第 `slot` 个槽位 |
| `release(slot)` | 归还该槽位当前持有的接触点归属 |
| `emit Fact(...)` | 向 Ruleset 发出事实，Fact 类型见 4.6 |
| `settle(outcome)` | 进入 Settled 状态，释放全部认领，之后不再接收事件 |

每条边执行的动作数量有静态上限。一条边最多执行一次 `settle`。

### 4.5 归属与可见性

§4.4 里"被认领接触点的后续事件只发给认领者"这句话过宽，会让多音符判定区重合时出错。
正确的做法是把两件事分开：

```text
归属（ownership）    决定谁有权消费这个接触点的后续事件
可见性（visibility）  决定谁能观察到它
```

规则：

1. **归属只影响消费权。** 已认领接触点的 `contactMove` / `contactEnd` 只允许持有者消费。
2. **观察型边始终可见。** 任何实例都可以用 `observe` 的边观察已被认领的接触点，看到但抢不走。
   这与 §8.7 的炸弹共用同一机制。
3. **换手需要显式归还。** 持有者用 `release(slot)` 归还归属，之后其他实例才能认领。
   `grip = sticky` 就是声明本程序永不 `release`，于是"换手"与"不换手"成为同一机制的参数。
4. **冲突在按下时由仲裁解决。** 两个 `consume` 实例的重叠区同时命中一次按下时，仍由第 7 节
   的 `claimKey` 排序决定谁赢。认领只在按下时竞争，之后不再有跨实例仲裁。

所有权由引擎维护，不由程序用数组自己记录。这样"一个接触点被两个实例同时认领"是结构上
不可能出现的，而不是靠运行期检查。

**所有权是边的隐式前提，不是运行期错误。** 含 `claim(contact, slot)` 的边隐含
`owner(contact) == none`；含 `input(contactMove, slot k)` 的边隐含 `owner(contact) == (this, k)`。
因此演奏中不会出现"认领失败"这类错误：不属于自己的接触点，边根本不进候选集合。

**引擎不在 `contactEnd` 时自动释放归属。** 接触点结束时引擎标记该归属为已终止，归属保留到
显式释放或实例结算。否则 `sticky` 无法表达"结束即断开"以外的语义，而 `handoff` 的宽限时间
也需要在接触点已结束之后仍然保留归属才能表达。快照必须记录已终止的归属，否则 Seek 后状态
不一致。

**接触点句柄永不回收**，单调递增，用满即报错。若在结束时回收，旧句柄可能在程序还没来得及
比较时就被复用。句柄是 32 位整数，一场演奏的接触点数远达不到上限。

**不能抢占。** 归属独占至释放或接触点终止。需要交接的场合用 `observe`，或由持有者显式释放。

**换手是一次消费，进入候选集比较。** `grip = handoff` 的实例在接触点未结束时收到新接触点的
按下，是**用这个新接触点继续同一条 requirement 的同一阶段**。按 §4.5 第 4 条，它必须与同一
次按下上的其他 `consume` 边一起参加 `claimKey` 比较，而不是无条件接管。这对"滑动经过多个
判定区、其间两支手交替"是决定性的：若换手绕过仲裁，一个接触点就可能既续上旧 requirement
又开启新 requirement。

**由此得到一条要求**：`handoff` 的可换手窗口与 `sticky` 的不可换手都是**边上的谓词**，
一起进候选集计算，而不是先于候选集判断。Fold Spike 的场景 S3/S4 验证了这条：
同样的几何在 `sticky` 下主体断开、在 `handoff` 下主体续上并结算。

**Hold 的两个 Measure 分量各发一条 Fact。** 因此中途断开时，头部已经发出过 `Hit(head)`，
主体再发 `Broken(body)`。两者是独立事实，**不做互相撤销**：是否把断开的 Hold 的头部命中
计入连击由 L3 的折叠规则决定（例如"主体 Broken 时撤销头部的连击"写成 fold 处理器）。
引擎不替 Ruleset 做这个判断，这与"引擎不区分好坏等级"是同一条原则。

### 4.6 Fact 与等级

```text
Hit    { phase, grade, errorTicks }
Miss   { phase }
Break  { phase, t }
Signal { kind, payload: Int[] }      kind 与 payload 的形状由 Ruleset Interface 声明
```

每条 Fact 都带 Program 声明的 `category`。引擎在语义上不区分"命中是好事"和"命中是坏事"，
两者都是 Fact；一次被触发与未被触发的区别由程序所在的判定类型决定。

`phase` 是程序内声明的阶段枚举，例如 head、body、tail。

内置函数 `grade(err, windowSet) -> Enum<Grade> | none` 用 Ruleset 声明的窗口表，把带符号的
误差映射到 Ruleset 的等级集合；窗口表可以带上 Hook 缩放。程序不能自己构造等级值，只能
通过 `grade` 得到，或者使用 Ruleset 声明的常量。

### 4.7 折叠表

Ruleset 用一个声明式的折叠表定义 (category, outcome, grade) 对 Score / Combo / Life /
Accuracy 的效果。这张表是数据，不是代码；技能通过修改这一层的挂点（例如"免疫 penalty 类别"）
来表达，不需要写程序。

普通音符与惩罚音符的差别落在表里，而不是落在引擎判断 Fact 是好是坏。所以同一套 Fact 类型，
既能表达命中得分，也能表达炸弹被触发而扣分。Combo 分母、Accuracy 与 All Perfect 判定只统计
`note` 类别，`penalty` 不进分母，也不破坏 All Perfect。

### 4.8 可观察寄存器

寄存器可以标记为 `observable`。只有被标记的寄存器、实例状态和 Fact 会进入
GameplayState(T)，供表现层只读使用，例如 Slide 已经走过几段、Hold 是否已断开。
没有标记的寄存器属于内部实现，修改它们不影响表现层合同。

## 5. 空间状态与输入

判定需要读取的空间状态有三类：Controller（由输入派生）、frame（由谱面数据声明）、
以及输入观察本身。三者都是只读的，程序不能写。

### 5.1 输入模型

**两类事件加一类状态：**

```text
接触点事件   begin / move / end        带 contactId 与整数坐标
通道事件     press / release           离散键位或按钮
控制器状态   轴、陀螺仪、鼠标位置       由 L1 更新的状态，不是事件
```

**轴不进事件流。** 它是状态。`axisStep` 定义为"L1 检测到轴越过声明的阈值"而合成的边沿事件，
阈值与滞回区间由 L1 声明，避免抖动。

**鼠标与触摸分开处理。** 鼠标按键是通道事件，鼠标位置是 `PointerHold2D` 控制器；触摸是
接触点。两者各自简单，混在一起反而复杂。

**接触点标识**由 L1 分配，会话内单调递增，**永不回收**（§4.4），这样句柄在快照中安全。

**坐标量化**按 §5.5：每个判定域声明自己的整数单位，映射是 L1 里的仿射变换，显式、可测、
进 mapping identity。

**死区与阈值属于 L1 映射**，写在 Ruleset 里，同样进 mapping identity。

**自定义按键映射属于 L1**，由 Player 或宿主提供，谱面与程序不需要感知。记录的是映射后的
规范化事件，映射 identity 写入 Replay 头部；播放时校验 identity，不匹配稳定拒绝，不静默
改用其他映射。

**线性插值属于 L1。** 接触点在两次 `move` 之间按线性插值，所以"某时刻接触点是否在某区域内"
可以解析回答，不需要控制器参与。§5.3 的 Controller 因此不需要 `sampled` 逃生通道。

### 5.2 输入重采样

**离散边沿与连续量分开处理。**

```text
离散边沿   press / release / contactBegin / contactEnd
           保留原始时间戳，绝不重采样。按键时刻的精度正是音游判定要测量的东西
连续量     接触点位置、轴、指针位置
           按声明的规范格点重采样，采样值为相邻原始事件之间的线性插值
```

**规范格点由 Interface 声明，按判定域给定。**

```text
inputRate   该域的规范采样率，例如 4 ms
minPollRate 该域要求的最低硬件上报率
```

**低于 `minPollRate` 的设备在上报率上被稳定拒绝**，而不是静默产生不同结果。这与本设计
其余部分一致：宁可稳定拒绝，不要静默降级。这条把"设备差异"从"难以复现的行为差异"变成
"prepare 时的一句明确诊断"。

**语义**：重采样的作用是**规范化评估时刻集合**。判定程序被唤醒的时刻由此只取决于声明的
格点，不再取决于设备恰好上报了多少个原始事件。轨迹本身仍由线性插值重建，因此在任意时刻
的位置查询结果不变，变的只是"哪些时刻会触发求值"。

**重采样属于归一化，所以 Replay 记录的是重采样之后的流。** 由此 L1 继续被排除在
[Gameplay Identity 分层草案](GAMEPLAY_IDENTITY_DRAFT.md) §6 之外，判定侧看到的流与 Replay
记录的内容完全一致。代价是改变 `inputRate` 不能被旧 Replay 复用，这是正确的：改变它就会
改变判定结果。

**与 §2.4 的关系**：重采样给连续量提供了规范格点，`every(period)` 则覆盖"完全没有新事件"
的情形（手指静止）。两者互补：前者规范"有事件时的求值时刻"，后者提供"无事件时的唤醒"。

**诚实的边界。** 重采样能保证的是：

```text
可以    同一设备类别下，不同上报率得到相同的评估时刻集合与相同的表示
可以    结果的可复现性（同一 Replay 在任何机器上一致）
不能    不同设备类别之间等价（绝对坐标触摸与相对鼠标、不同上报延迟）
不能    消除重建轨迹的插值误差
```

最后一条需要展开。相邻两个原始事件之间用直线近似真实运动，125 Hz 设备的重建轨迹比
1000 Hz 设备粗糙。重采样让两者的**表示**与**求值时刻**一致，但**不能凭空恢复硬件没有
采到的信息**。因此"完全相同的判定结果"只在两种情况下成立：

```text
1  该域声明为已离散输入（例如只接受离散按键）
2  真实运动在较粗设备的采样间隔内确实是分段线性的
```

对于连续域，可保证的是"差异被限制在插值误差内"，而不是"差异为零"。这一点必须写进作者
文档，因为"统一重采样"很容易被误读为"任何设备都一样"。

**代价**：规范格点产生的样本数 = 域时长 ÷ `inputRate` × 并发接触点数。以 5 分钟曲目、
4 ms、10 个接触点计约 750,000 个样本，与大型谱面的 Requirment 量级相当，需要单列预算条目。
每个样本都要经过第 7 节的派发循环，因此它直接进入每事件代价上界。


但插值只能回答**已知时刻的查询**，它**不产生事件**。如果一个连续谓词在两次输入事件之间
发生了变化，程序不会被唤醒，这次变化就看不见了。§2.4 的状态级 `every(period)` 解决了这一点，
§8.12 记录了完整处置。

### 5.2b L1 朝向归一化（回应 §10.8）

§5.4b 的 `frame ... source bound(ctrl.deviceYaw)` 依赖 L1 提供一个**归一化朝向角**。
该角由陀螺仪与加速度计融合得到，融合算法属于 L1 实现、**不进 identity**
（身份草案 §6：与校准同级）。但它直接决定 §5.4b 的判定结果，因此不能留给实现自由发挥——
"不进 identity"不等于"怎么写都行"。

**它是 L1 与 identity 分离原则的一个特例，必须显式说明。** 一般规则是"改变判定结果的
都进 identity"。朝向融合改变判定结果，却不在 identity 内。两者不矛盾，理由是通道：

```text
L1 的输出是归一化朝向角，它进入判定        —— 这部分是判定输入，有确定语义
L1 的内部是从原始传感器到该角的融合过程     —— 这部分等价于"校准"，被烘进事件流
```

与输入映射是同一条处理：映射后的规范化事件进判定，映射本身只作信息性元数据。
朝向融合的位置与映射相同，因此待遇也相同。

**接口只保证三件事**：

```text
1  输出是角度         单位 1/65536 圈，与 §5.4 的角度量化同族，便于直接比较
2  值域是单一圈        [-32768, 32768)，不存在多圈累积。需要多圈计数的玩法自己维护圈数
3  单调性             设备同向旋转时输出不倒退（允许在 0/2π 边界翻转一次）
```

第 2 条是有意收窄的：**不给"原始角速度"或"累计角度"**。理由与 §5.2 的重采样裁决一致——
积分一旦发生在判定侧，漂移就成了判定模型的一部分，而漂移量依赖设备与融合算法，
恰好是本设计一直在消除的那类差异。

**为什么不做成 identity 的一部分**：融合算法的差异**已经进不了判定**
——因为判定读到的是融合后的角，任何融合差异都表现为归一化事件流本身的差异，
与"玩家这次转动得快一点还是慢一点"不可区分。因此把它当作校准处理是正确的：
信息性元数据、可诊断、不可校验。

**规范必须固定的部分**（这些属于 L1 合同，不是实现细节）：

```text
输出单位与值域     见上三条
采样与上报         归一化后的角进入 §5.2 的连续量通道，按域声明的 `inputRate` 重采样
最低上报率         由 Interface 的 `minInputRate` 声明（§5.4b），低于此值稳定拒绝
零点与方向         由 L1 校准给定，烘进归一化后的值；校准参数作信息性元数据
融合算法           不固定。属 L1 实现，不进 identity、不进合同
```

**允许的实现自由**（写明是为了避免作者依赖具体行为）：

```text
互补滤波 / 卡尔曼 / 厂商融合库   任选
滤波器参数、收敛速度、抖动抑制     任选
```

因此一条判定规则若把结果推到"两次采样之间的快速转动"上，它就在依赖融合算法，
而不是依赖声明的语义。**这属于作者应避免的写法**，需要写进作者文档。
与压缩包类玩法不同，这里没有可声明的替代——物理上就只有融合后的角可用。

**与 §5.2 的一致性检查**：朝向是连续量，因此它按 §5.2 的规范格点重采样，
离散边沿规则不适用（朝向没有边沿）。这条有一个副作用需要写明：**朝向的变化只有在
采样格点上被观察**，与 `active(region)` 这类电平谓词受同一条限制。若某玩法要求
"转动超过阈值即刻触发"，它必须靠 `every(period)` 提高采样密度，而不是指望设备事件。

### 5.3 Controller

Controller 是由输入派生的连续状态，在第 2.3 节规定的时刻采样。它是 Ruleset 声明的全局
对象，不属于任何 Requirement 实例。

Controller 有两个函数，分界不在复杂度而在求值时机：

```text
onEvent     输入事件到达时更新本控制器的状态
sampleAt(t) 任意 Tick 求值，供守卫与定时器读取
```

`sampleAt` 必须对任意 Tick 可求值，因为守卫不只在输入事件时求值，定时器触发时也要读它。
因此设计取"**事件侧可编程，时间侧封闭**"：

- `onEvent` 由作者用 L3 语句编写，只能改本控制器自己的状态，可以读距上次事件的间隔。
- `sampleAt` 的每个字段从三个外推基里选一个，再可选地套一个后处理算子。

**外推基**（三者都是分段多项式，都是闭式）：

| 基 | 多项式次数 | 参数 |
| --- | --- | --- |
| `hold` | 0 | 常量 |
| `linear(v)` | 1 | 初值与一阶导 |
| `quadratic(v, a)` | 2 | 初值与一阶、二阶导 |

**后处理算子**（作用在求值结果上，不是外推基）：

```text
clamp(lo, hi)
```

原来的形状表因此降级为**预验证的标准库**，不再是一个封闭集合：

| 形状 | 等价写法 |
| --- | --- |
| `Steady1D` | `onEvent` 设方向，`sampleAt` 用 `linear` |
| `Ramp1D` | `onEvent` 设 v 与 a，`sampleAt` 用 `quadratic` |
| `Ballistic` | 同 `Ramp1D`，a 固定 |
| `PointerHold2D` | `onEvent` 记位置，`sampleAt` 用 `hold` |
| `Clamp` | 在求值结果上套 `clamp` |
| `Damped` | 见下面的扩展路径二 |

osu!catch 的盘子是 `Steady1D` 加 dash 倍率。参数由 Ruleset 作者填写，不需要写代码。

状态仍然有界、可快照，每输入事件 O(1)，进静态预算。代价是控制器不能在两次事件之间
自己拐弯，而这本来就无法表达为闭式。

**外推基可扩展，且扩展是纯增量。** 三条路径：

1. 加阶数：`cubic(v, a, j)` 仍然闭式，每事件多几次乘加，验证器更新常数上界即可。
2. 加有闭式解的基：`exp(v, halfLife)`、`sinusoid(v, period)`，需要引擎冻结对应的定点表，
   与 §5.4 的角度表是同一机制。
3. 分段拼接：外推基只在两次事件之间生效，事件到达即重新初始化，所以"先加速后匀速"
   这类行为靠中间事件即可表达，不需要扩展语言。

**扩展判据**：如果三个基写不出某个游戏的控制器，那是"该加一个基函数"的信号，
**不是"该放开浮点"的信号**。外推基集合声明为 capability，需要 `cubic` 的 Ruleset 包在
缺少该能力的引擎上稳定拒绝，不静默降级为 `quadratic`。

不需要 `sampled` 逃生通道。它原本要解决的问题是"接触点在两次事件之间是否出界"，
而那由 L1 的线性插值回答，与控制器属于同一类问题，见 §5.1。

`sampleAt` 的外推基不开放自定义：引擎只接受声明的基，这是"时间侧封闭"的含义。
自定义只发生在 `onEvent`。若作者需要引擎尚未提供的基，正确做法是给引擎补一个基并更新
capability，而不是绕开限制；这条与 §5.4 的边界判据是同一条规则。

Controller 可以接收 Ruleset 发出的命令，用于表达 hyperdash 这类"判定结果影响移动"的机制。
命令同样遵守 t+1 规则。

### 5.4 几何轨道（frame）

Phigros 的判定线会移动和旋转，Arcaea 的 Arc 位置随时间变化，osu! 的 Slider 路径随时间推进。
这些都不是静态判定区，但也不该变成表现层的实现细节。

**设计要点是把形状和摆放分开。** 形状在 frame 的局部坐标里是静态的；摆放是一段随时间采样的
`Transform`。命中检测因此变成"把输入点变换到局部坐标一次，再做静态判定"，每次查询代价 O(1)，
不需要给每个判定区写动画逻辑。

```text
frame <name> {
  source  track(Segment[]) | bound(ctrl.<name>.<field>)
  units   Point 与角度的整数量化单位
}

Transform = (origin: Point2, angle: QuantizedAngle, scale: Int)

Segment:
  Const  常量
  Lerp   线性
  Ease   固定曲线集合之一（线性、二次、三次、回退、弹性等）
```

评定使用二分查找定位分段，再对该段求值，复杂度 O(log N)，N 有静态上界。整条轨道是
谱面或 Ruleset 的数据。

**旋转必须量化。** 角度以 1/65536 圈的整数表示，正弦余弦取自引擎内置的定点表。那张表随
引擎版本冻结，不使用平台数学库，否则跨平台结果会不一致。表的版本进入 capability 集合。

**frame 是派生数据，不是被写入的数据。** 此前写的"运行期不可变"过严，见 §5.4b。真正要保证
的是：**没有任何东西在演奏中写 frame**，判定与表现都只读。这一条解决了一个原有冲突——
移动判定线到底属于判定还是表现。它是一个被两层同时读取的数据对象，不属于任何一层的运行期
状态。判定读它做命中检测，表现读它做绘制，表现不能写。

**区域声明获得一个可选的 frame 绑定：**

```text
region <name> {
  frame   <frameName> | static
  shape   Segment | Strip | Rect | Disc | Corridor
}
```

`contactIn(contact, region)` 的语义固定为：按当前 Tick 把 `contact.pos` 变换到该 region 的
局部坐标，然后做静态判定。静态 region 等价于绑定一个恒等 frame。

形状集合至此收敛为五类，全部在局部坐标中是静态的：

| 形状 | 用途 |
| --- | --- |
| `Segment` | 离散轨道或判定段，例如 Phigros 判定线上的一段 |
| `Strip` | 单轴有界的无限长条 |
| `Rect` / `Disc` | 有界二维区域 |
| `Corridor` | 以路径为中心、半宽可变的走廊，路径与半宽本身也是轨道 |

**近似。** frame 在采样 Tick 上被冻结。两次输入事件之间 frame 仍在移动，而默认只在事件发生
的时刻做检测。这与 Slider 的近似是同一条：**事件频率就是默认采样率**。当某个判定不能接受
这个近似时（例如手指不动而走廊移出），用 §2.4 的 `every(period)` 显式提高采样密度，代价
也随之声明。不再需要靠"提高输入上报率"这类隐含手段。

**代价。** frame 求值进入每事件的代价预算，在 prepare 时静态计入。跨 frame 的区域重叠由
第 8 节的仲裁处理，`claimKey` 可以包含位置误差项。

### 5.4b frame 绑定 Controller（设备绑定型玩法）

**此前把这一类整个排除，是过严的。** 原文写"frame 的位置由运行期状态驱动不在本设计内，
那会让 frame 变成运行期状态，破坏不可变规则"。该论证把两件事混在一起：

```text
被写入       有东西在演奏中修改 frame            —— 必须禁止
依赖会话状态  frame(T) 是折叠状态的函数          —— 一直成立，且无害
```

`frame(T)` **本来就**是会话状态的函数（依赖时钟 T）。绑定 Controller 只是扩大了这个函数的
定义域，没有改变它的种类。而定量的检查如下：

| 不变量 | 数据轨道 frame | 绑定 Controller 的 frame |
| --- | --- | --- |
| 无人写入 | 成立 | 成立 |
| 判定不读渲染 | 成立 | 成立（Controller 只由输入驱动） |
| 快照与 Seek | 成立 | 成立（Controller 状态本来就在快照内） |
| 自引用 | 不适用 | 无（t 时刻取严格早于 t 的输入，与 §5.3 一致） |
| 求值代价 | O(log N) | O(1)，更便宜 |

因此：**frame 的来源有两种，`track` 与 `bound`，接口都是 `frameAt(t) -> Transform`。**

```text
frame playfield { source bound(ctrl.deviceYaw) }

program rotateNote
  params    anchor: Tick, theta: QuantizedAngle, w: QuantizedAngle
  instant   at(anchor) : press(lane) ∧ |ctrl.deviceYaw − theta| ≤ w
```

Rotaeno、Tone Sphere 一类"判定线朝向由设备旋转驱动"的玩法由此可表达。它们此前被排除，
不是因为需要新机制，而是因为一条写得过严的不变量。

**L1 提供的是归一化朝向角，不是原始角速度。** 这一点是必须的：若把陀螺仪速率积分交给判定
侧，漂移就成了判定模型的一部分；放在 L1 则与"校准烘进事件流"是同一条处理（见
[Gameplay Identity 分层草案](GAMEPLAY_IDENTITY_DRAFT.md) §6）。融合算法属于 L1 实现，
不进 identity。**接口只保证什么、不保证什么，见 §5.2b。**

**设备能力声明。** 这类玩法需要 Interface 声明所需硬件：

```text
requiresOrientation   无相应传感器时拒绝启动，并给出稳定诊断
minInputRate          沿用 §5.2 的每域最低上报率
```

与 `minPollRate` 同形：把"设备差异"变成 prepare 时的一句明确诊断，而不是静默产生不同结果。

**这一类仍然不覆盖：**

```text
3D 空间玩法        Beat Saber 一类，需要 Vec3 原语族，见压测登记 L2
连续音高           Rocksmith 一类真实乐器输入
私有外设协议       需要宿主适配器，不属于 L1 的声明式映射
```

### 5.5 数值模型：数据浮点，决策投影到整数

判定决策必须逐位可复现。达成它的手段是**把浮点限制在数据与表现两侧，决策侧只看一次性
量化投影**，而不是要求作者写整数。

```text
作者数据   f32 / f64        谱面、路径、frame、资源：任意精度表达
prepare    量化一次          向偶舍入，单位由判定域声明
决策      整数               L2 程序、仲裁、折叠：逐位可复现
表现       浮点              渲染与音频：自由，不受判定纪律约束
```

这与时间域已经采用的规则是同一条：Beat 在 prepare 时经 TimingMap 转成 chartTimeMs，
再量化成 Tick，只量化一次。

**为什么不是全浮点。** 破坏确定性的不是浮点本身，而是它的三种自由度：

| 问题 | 表现 | 处理 |
| --- | --- | --- |
| FMA 收缩 | 编译器把 `a*b+c` 合成一条指令，结果不同 | MSVC `/fp:contract-`；GCC/Clang `-ffp-contract=off` |
| 扩展精度 | x87 80 位中间值 | 避免 x87 目标，x64 默认 SSE |
| libm 超越函数 | `sin`、`exp` 各实现不同 | 决策路径禁用，改走引擎冻结的定点表 |

再加一条：禁用 fast-math。这些都可控，但本仓库要同时支持 MSVC、MinGW、Linux GCC/Clang，
恰是浮点差异最大的组合，标志需要在三个工具链上各自钉住并持续维护。

更关键的是**边界比较**。同一锚点在两个平台上算成 99.9999999 与 100.0000001 时，判定结果
分叉，只能引入容差；而容差会重新引入"容差怎么比较、容差之间怎么排序"的复杂度，正是本
设计一直在避免的东西。整数在边界上有唯一答案。

**量化规则要求**：

```text
向偶舍入        与 IEEE-754 的默认舍入一致，可用硬件指令
单调不降        量化保持序关系，不会把先发生的排在后面
唯一            同一输入在任何平台得到同一格点
单位由域声明     位置 1/1024 或更细，时间 1 微秒，角度 1/65536 圈
```

量化的代价只有格点之下的精度，而格点远小于人的感知。

**边界判据**：如果某个判定写不出来，那是"该加一个闭式基或一个声明式能力"的信号，
不是"该放开浮点"的信号。§5.3 的外推基扩展与 §5.4 的定点表机制都是这条路。

### 5.6 扩展路径：3D 空间玩法与私有外设协议

这两类现在**不做**，也**不预留结构**（本仓库禁止为延期能力预留字段、extension、capability 或
隐式执行入口）。能做的、也必须做的，是写清它们各自依赖的不变量，使将来能用**加法式**扩展
加入：不修改既有语义，不重排 identity 分量，不让已发布的 Replay 失效。

这不违反禁止预留，因为下面列出的不变量**现在就已经成立**，删掉它们会立刻损坏当前设计。它们
本来就是当前设计正确性的理由，只不过这些理由同时对这两个方向有用。

#### 5.6.1 3D 空间玩法

先把"已经与维数无关"的部分和"确实是二维"的部分分开。

与维数无关（三维直接沿用，不需要改）：

```text
区域语义      "把输入点变换到 region 局部坐标一次，再做静态判定"
采样规则      §2.4 的 every(period) 与 §5.4 的"事件频率即默认采样率"
frame 来源    §5.4b 的 track / bound 两种来源
因果与仲裁    t+1 规则、claimKey 排序、grip
量化原则      §5.5 的"单位由域声明"，不写死 1/1024
```

确实是二维的（三维需要**新增**，不是**修改**）：

```text
Point2 / Transform = (Point2, angle, scale)
§4.3 的 vec / dot / cross / len2
§5.4 的五类形状 Segment / Strip / Rect / Disc / Corridor
```

其中只有一处是真正的结构限制：`cross` 只在二维有一个标量结果，三维的转向由叉积向量表达，
因此**"左转还是右转"在三维里需要一个投影平面**才有定义。三维玩法必须声明它的判定平面
（例如取刀轨迹与地面的交线），否则转向判定无解。这是三维玩法的第一个新决策，不是一次维数
替换。

加法式扩展路径：

```text
1  判定域声明自己的维数与单位；Point2 之外新增 Point3，二维声明不受影响
2  形状集合新增 Prism / Cone / Plane / Sweep，五类二维形状保留
3  Transform 增加朝向分量（四元数或轴角），量化单位仍取 1/65536 圈一族
4  几何原语族新增 vec3 / dot3 / cross3 / len3 与平面投影原语
5  L1 的输入状态新增姿态类（六自由度），与轴并列，仍不进事件流
```

三条不变量使上述全部可行，且现在就必须成立：

```text
A  几何原语是纯函数、有界整数运算、不依赖 libm
B  仿射变换是单次量化操作，禁止在同一表达式里串联多次旋转
C  region 是局部的静态形状，摆放由 frame 提供
```

**真正昂贵的地方不在"多一个坐标"。** 三维带来的新成本有四项，每项都是独立决策：

```text
1  接触点数量级   双手各多个接触点是常态，活动度预算与槽位模型的常数需要重估
2  输入率数量级   姿态流常在 1000 Hz 以上，§5.2 的规范格点量级与预算按位置流标定，需要重算
3  扫掠体判定     刀是带时长的运动体。"某一时刻在区域内"不再是玩法要求的判定；
                  需要"一段轨迹与区域的相交"。它引入一个时间维形状，
                  不是 Vec3 能解决的，也不是四个时序算子能组合出来的
4  Controller 外推  sampleAt 的三个外推基（hold / linear / quadratic）是为位置设计的，
                  旋转的外推不是线性外推的逐分量推广（球面插值不是线性），
                  需要新的外推基，或显式声明用轴角分量线性外推并给出误差界
```

第 3 项最容易被低估，也最可能推翻"扩展而非新增"的预期。诚实的判断是：**三维玩法可能不只是
加一维；扫掠体判定很可能需要一套新的判定形状与新的时序算子**。这不改变现有设计，但意味着
"留出空间"的承诺只覆盖到坐标、形状、量化与 L1 输入类别这一层，**不覆盖判定形状层**。

#### 5.6.2 私有外设协议

L1 的定义是"引擎 + 宿主适配器"（讨论记录 §2.9）。私有协议走的就是这条既成通道，不需要在
L1 的声明式映射里开洞。

已有三个先例，形状一致：

```text
§5.1       轴不进事件流，axisStep 是 L1 越阈合成的边沿
§5.4b      L1 提供归一化朝向角，不提供原始角速度
Identity §6  映射与校准只作信息性元数据，不进校验门
```

共同形状可以写成一个不变量：

```text
外设只能通过"归一化后的输入类别"影响判定，不能通过"设备身份"影响判定。
```

判定结果不依赖"这是哪个厂商的板子"，只依赖它归一化后上报了什么。这条现在就已经成立
（§5.1 把映射、死区、阈值、插值全部压在 L1），私有协议不需要破坏它。

加法式扩展路径：

```text
1  适配器实现 L1 的输入源接口，出口只能是既有的四类：
   接触点事件、通道事件、轴状态、控制器状态（外加 §5.4b 的朝向类）
2  适配器不得向 L2 投递设备原始量；§5.2 的重采样在适配器出口处仍然生效
3  Interface 声明所需输入类别与最低上报率；缺能力时 prepare 稳定拒绝
   （§5.4b 的 requiresOrientation / minInputRate 是第一个实例，形式可直接推广）
4  适配器实现不进 identity；进 identity 的是它提供的类别与参数范围
```

需要新决策的地方（同样不是预留）：

```text
1  输入类别的登记表：现在是固定的几类。私有协议可能需要新类别（压力、滑杆阵列、多段脚踏）。
   新增类别是 Interface 的加法式扩展，但需要一个统一的登记点；
   现有 §5.4b 的 requiresOrientation / minInputRate 只是临时落点。
2  校准的可见性：Identity §6 把校准降为信息性元数据。私有外设的校准误差可能大到改变判定
   结果，引入私有协议时这条需要重审。现在不改，只标记。
3  多设备共存：同一会话同时接入多个私设时，类别命名与冲突规则需要定义。
```

#### 5.6.3 明确不做的事

为避免本节被读成"已经留好位置"，列出不做的：

```text
不预留字段            判定域、region、Transform、Controller 的现有结构中不添加三维或设备字段
不预留 capability 位   能力位由使用它的那次扩展自行新增
不预留 IR 操作码       几何原语按需新增，不在现有原语上留扩展位
不预留执行入口         没有"自定义判定钩子"这类口子
```

将来真要做时，加入的形式是统一的：**新增 capability 位 + 新增 Interface 版本 + 新增原语或
形状族**。既有二维内容一行不改，因为 Interface 版本是加法式兼容的
（[Gameplay Identity 分层草案](GAMEPLAY_IDENTITY_DRAFT.md) §2.2：新增不 bump）。

## 6. 表现绑定（L4）

L4 把谱面数据、时间、输入观察和判定事实映射为画面与声音。它只读，不能写回 L1、L2、L3。

### 6.1 三类读入口

只有状态查询不足以支撑表现。"命中闪光"是瞬时事件，不是状态；状态查询回答不了"刚刚发生过
什么"。所以 L4 有三类读入口：

```text
状态查询   GameplayState(T)                当前值
事件窗口   最近一段 Tick 内产生的 Fact      有界环形缓冲，长度由 L4 声明并计入预算
连续量     frame(T)、输入观察流             几何与输入
```

事件窗口必须有界。Seek 之后它由重新折叠自然填回，不需要单独的正确性处理。

### 6.2 三条驱动路径

| 路径 | 读什么 | 典型用途 |
| --- | --- | --- |
| 时间与谱面 | Chart、Timing、frame | Note 下落、判定线绘制、轨道变形 |
| 输入观察 | L1 的输入事件 | 按键音、按键高亮、按触摸位置倾斜轨道 |
| 判定事实 | Fact 流与事件窗口 | 判定文字、命中特效、连击提示 |

**按键音（key sound）必须分成两种，不能合并。**

- 输入驱动的按键音：在按下瞬间播放，**不经过判定**。理由是延迟：若等判定结果再发声，
  至少要经过一个 Tick 再经过折叠，手感会明显变差；而且按下但未命中时，多数游戏仍要发声。
  这条路径是 `L1 -> L4 -> 音频`。
- 判定驱动的音效：命中音、断开音、Miss 音效。由 Fact 流驱动。

同一次按下可以同时触发两条路径，它们各说各话、互不干扰。按键音是输入反馈，不是判定声明。
这一点需要写进合同，因为直觉上容易把按键音挂在"命中"上。

### 6.3 绝对时间采样

命中闪光不能按帧累加。它应当表示为"从 Tick `t` 开始、持续 200 ms 的衰减包络"，
渲染时按 `T - t` 求值。这与模型层"支持绝对时间采样"的规则一致，也是 Seek 后画面正确的
前提。音频播放本身有延迟，但判定不依赖它，所以不影响确定性。

### 6.4 分层与覆盖策略

表现的来源有四个，从低到高：

```text
engine    引擎内置兜底
ruleset   Ruleset 包的 defaults 块，定义整个游戏的外观基调
chart     谱面自带的绑定与资源
skin      玩家或平台选择的皮肤（可选层，由 Ruleset 决定是否开放）
```

默认规则是**高优先层覆盖低优先层**，也就是谱面可以覆盖 Ruleset 默认。这符合谱师需要为
具体谱面调手感的需求。

### 6.5 覆盖必须按"绑定键"逐项进行

"谱面覆盖 Ruleset 默认"如果理解成"整个表现配置二选一"，就会丢掉统一性：谱面只想改按键音，
却顺带把判定线样式也换成了自己的。所以覆盖的粒度必须是**绑定键**，不是整层。

```text
绑定键 = (目标对象, 属性, 触发源)
```

生效规则：对每个绑定键，取优先级最高的那一层提供者。没有提供者的层不参与。因此谱面只提供
自己关心的那几个键，其余全部落回 Ruleset 默认。

这样一来，"谱面覆盖默认"和"同一游戏玩法表现统一"不再冲突：**统一性来自 Ruleset 默认覆盖
的键集合足够完整，而谱面只被允许覆盖其中被标为可覆盖的键。**

### 6.6 overridable

Ruleset 在每个绑定键上声明 `overridable`，默认 `true`：

```text
overridable = true    谱面可以覆盖这个键
overridable = false   谱面覆盖该键时在 prepare 阶段被拒绝，并给出稳定诊断
locked = true         该键连 skin 也不能改，用于必须保持品牌或公平性的表现
```

这是把"我希望谱面能调手感"和"我希望游戏风格统一"同时表达出来的机制。美术风格、判定文字
字体、命中特效主色这类键设成 `false`，谱师就只能在开放的那些键上调手感，无法破坏统一性。

拒绝发生在 prepare，不静默忽略，也不降级：与 L2 程序、L3 模块冲突的处置方式一致。

覆盖关系进入内容 identity：谱面里实际生效的绑定参与谱面语义 identity 的计算，但
**Ruleset 提供的默认值不参与**，否则一次美术调整会让全部已有 Replay 失效。

### 6.7 资源闭包

按键音、特效材质等都属于资源。归属按提供者划分：由谱面声明的进谱面闭包，由 Ruleset 包的
`defaults` 声明的进 Ruleset 包闭包。谱面覆盖某个键时，被覆盖的默认资源仍按源语义计入
Ruleset 包闭包，不因未被使用而消失。

## 7. 仲裁

输入事件 `e` 到达时，按以下步骤分配：

1. 如果 `e.contact` 已被某个实例认领，直接交给该实例，跳过仲裁。
2. 候选集合：所有处于 Live 状态、且存在一条可触发边的实例。可触发指触发器匹配、守卫为真。
   这一步只求值守卫，不产生副作用。候选集合按 `dispatch` 分成两组。
3. 可消耗组（`consume`）：Ruleset 的 `ArbitrationPolicy` 先过滤、再排序。
   - 过滤，例如 note lock：同一 channel 上只保留 arm 最早且尚未结算的实例。
   - 排序：按 `(claimKey, Requirement identity)` 字典序。`claimKey` 由程序给出，
     因为不同判定类型需要的键不同：Tap 看 `abs(evt.t - anchor)`，Slider 可能看位置距离。
4. 可消耗组的第一名执行它的边，其余 consume 实例与本次事件无关。
5. 观察组（`observe`）的全部实例都执行它们匹配的边，不消耗输入。炸弹不会被消耗掉，
   也不会把输入从普通音符手里抢走。
6. 可消耗组为空时，Ruleset 收到 `StrayInput(e)`。空按是否惩罚由 Ruleset 折叠规则决定，
   不由程序判断。

`ArbitrationPolicy` 是 Ruleset Build 的一部分，它的 identity 进入 Replay。它只做过滤和排序，
不读其他实例的内部寄存器，因此程序之间保持零耦合。

`fanout` 只在锚点完全相同这类需要"一次输入同时结算多个音符"的场合使用（多押）。
它是 Ruleset 策略，和上文的 consume 竞争不冲突：fanout 决定在可消耗组里取前几名。

规则：程序不能读另一个实例的寄存器。唯一例外是 Ruleset 的过滤步骤可以读候选的 `arm`
与守卫结果。这条限制使每个实例可以独立验证，也让仲裁的代价保持在可计算范围内。

`fanout` 只在锚点完全相同这类需要"一次输入同时结算多个音符"的场合使用（多押）。
它是 Ruleset 策略，和上文的 consume 竞争不冲突：fanout 决定在可消耗组里取前几名。

`ArbitrationPolicy` 是 Ruleset Build 的一部分，它的 identity 进入 Replay。

## 8. 案例压测

下面用 `W` 表示 Ruleset 声明的窗口集合，`W.max` 表示它在技能缩放后的静态上界。

### 8.1 Tap

```text
program tap
  params    anchor: Tick, lane: Channel
  arm       anchor - W.max.early
  deadline  anchor + W.max.late
  claimKey  abs(evt.t - anchor)

  state Wait
    on input(press, lane)
      guard grade(evt.t - anchor, W) != none
      do    emit Hit(head, grade(evt.t - anchor, W), evt.t - anchor)
            settle(hit)

  onDeadline  emit Miss(head); settle(miss)
```

结论：只用到一个状态、一个输入触发器和截止时刻处理。

### 8.2 中途可断开的 Hold

头部判定和 Tap 相同。中段允许短暂松开：在宽限时间 `hook.holdGrace` 内重新按下即可继续。
超过宽限时间，Hold 断开。尾部采用"按住到结束即成功"的街机式规则。其他尾部规则（按释放时刻
判定）可以作为同一程序的一个参数分支，或者写成另一个程序。

```text
program hold
  params     start: Tick, end: Tick, lane: Channel
  registers  releasedAt: Tick
             observable headGrade: Enum<Grade>, broken: Bool := false
  arm        start - W.max.early
  deadline   end + holdGrace.max
  claimKey   abs(evt.t - start)
  grip       handoff

  state Head
    on input(press, lane)
      guard grade(evt.t - start, W) != none
      do    claim(evt.contact, 0)
            set headGrade := grade(evt.t - start, W)
            emit Hit(head, headGrade, evt.t - start)
      goto  Body
    on timer(start + W.late)
      do    emit Miss(head); settle(miss)

  state Body
    on timer(end)
      do    emit Hit(tail, Grade.best, 0); settle(hit)
    on input(contactEnd, slot 0)
      do    release(0); set releasedAt := evt.t
      goto  Gap

  state Gap
    on input(press, lane)
      guard evt.t < releasedAt + hook.holdGrace
      do    claim(evt.contact, 0)
      goto  Body
    on timer(releasedAt + hook.holdGrace)
      do    set broken := true
            emit Break(body, releasedAt); settle(broken)
    on timer(end)
      do    emit Hit(tail, Grade.best, 0); settle(hit)

  onDeadline  emit Miss(tail); settle(miss)
```

`on input(contactEnd, slot 0)` 的边隐含 `owner(contact) == (this, 0)`，所以它天然只对**自己**
持有的接触点触发，不会误接别的实例的手指。释放是显式的 `release(0)`，这正是
`grip = handoff` 的表达方式：引擎不在接触点结束时自动归还归属，否则宽限时间内无法区分
"同一个接触点被重新按住"与"另一个接触点接手"。

  state Gap
    on input(press, lane)
      guard evt.t < releasedAt + hook.holdGrace
      do    claim(evt.contact, 0)
      goto  Body
    on timer(releasedAt + hook.holdGrace)
      do    set broken := true
            emit Break(body, releasedAt); settle(broken)
    on timer(end)
      do    emit Hit(tail, Grade.best, 0); settle(hit)

  onDeadline  emit Miss(tail); settle(miss)
```

Hold 变灰由表现层绑定实现，程序不参与：

```text
material.tint = outcome(req) in {miss, broken} at T ? gray : normal
```

`outcome(req)` 是对 GameplayState(T) 的绝对时间查询，所以 Seek 到断开前后都能得到正确画面。

发现的问题：

1. `timer` 表达式需要读取寄存器（`releasedAt`）和 Hook（`holdGrace`），因此必须要求
   `holdGrace` 在 Ruleset 中声明静态上界，否则算不出 deadline。
2. 需要按 contact 过滤的输入（`contactEnd, c`）。只按 lane 过滤不够：同一 lane 上可能有
   另一根手指。
3. Gap 期间同一 lane 上可能落下一个新 Tap。这次按下交给 Hold 还是交给 Tap，由第 8 节的
   仲裁排序决定。Ruleset 需要为"接回 Hold"和"新音符"规定优先级，这是一个真实的策略参数。

### 8.3 带跳区的 maimai 星星

路径拟合是数据变换，不属于运行时。谱面给出路径形状，比如"1-5 直线"或某个曲线编号，
Ruleset 的**路径形状库**在 prepare 时把形状展开为判定区序列 `zones`。程序只负责按顺序匹配
这些判定区。（这里指的是谱面路径的形状库，与 §5.3 的 Controller 外推基无关。）

传感器输入属于观察型输入：同一时刻多条 Slide 可以共用同一次触摸，不需要认领。

```text
program slide
  params     zones: Array<Region, 32>, n: Int[1..32], skip: Int[0..3],
             start: Tick, end: Tick
  registers  observable i: Int[0..32] := 0      // 已完成的判定区数量
  arm        start
  deadline   end + Wslide.max.late
  grip       handoff
  dispatch   observe

  state Track
    level
      guard  some k in 0..skip : i + k < n && active(zones[i + k])
      do     set i := i + firstk + 1
             if i == n: emit Hit(tail, grade(evt.t - end, Wslide), evt.t - end)
                        settle(hit)
      progress i

  onDeadline  emit Miss(tail); settle(miss)
```

语义：当前判定区到后面 `skip` 个判定区中，任何一个被触摸，进度就推进到该判定区之后，
中间被跳过的判定区不再要求。"跳区"只是参数 `skip`。

`grip` 在这里是 `handoff`，因为星星允许换指；`dispatch` 是 `observe`，因为传感器不独占输入
（§8.3 开头）。`level` 的触发条件是"本实例自身状态被写入后"重求值，所以 `i` 被推进后即使
手指已经停在下一个判定区里也会继续推进，直到不再满足条件。链长以 `i` 的取值范围为界。

发现的问题：

1. 需要"找到第一个满足条件的下标"，由 4.2 的 `some k in 0..K` 与 `firstk` 提供。
2. 需要按电平检查判定区当前是否被触摸，而不只是检查"进入判定区"这个边沿：手指可能在
   上一步推进时已经停在下一个判定区里。这就是 `level` 触发器存在的原因。
3. 为了防止 `level` 在同一 Tick 内无限触发，每条 `level` 边都必须声明 `progress` 度量：
   一个有界寄存器，每次触发都严格增加。验证器检查这个性质，得到每个 Tick 最多触发 n 次。
4. 在 `level` 边中 `evt.t` 表示当前 Tick。
5. `if` 只能出现在动作列表中，而且只能分支到"发出 Fact 和 settle"。它是语法糖，
   等价于拆成两条守卫互斥的边。

### 8.4 osu!catch 水果

Ruleset 声明 Controller：

```text
controller catcher: Steady1D
  inputs  left/right 边沿决定方向，dash 电平决定速度倍率
  params  speed、dashFactor、bounds 由 Ruleset Build 给定
  units   x 为 1/1024 个 playfield 单位（整数）
```

```text
program fruit
  params    x: Int, anchor: Tick
  arm       anchor
  deadline  anchor

  state Wait
    on timer(anchor)
      guard abs(ctrl.catcher.x - x) <= hook.catchHalfWidth
      do    emit Hit(head, Grade.catch, 0); settle(hit)
    on timer(anchor)
      do    emit Miss(head); settle(miss)
```

同一个触发器上有多条边时，按声明顺序选取第一条守卫为真的边。

Hyperdash 的写法：带 hyperdash 标记的水果被接住时，程序发出
`Signal(hyperdash, [targetX, targetTick])`，Ruleset 据此向 catcher 下发命令，按 t+1 规则生效。

发现的问题：

1. 判定域不是固定判定区，而是 Controller 的采样值。Controller 必须能在任意 Tick 做闭式求解，
   并且只使用整数运算，否则跨平台结果会不一致。
2. 在 `anchor` 同一 Tick 到达的输入不影响这次采样，这是第 2.3 节"只由严格早于 t 的输入决定"
   的直接结果。这一点需要写进作者文档。
3. Droplet、Banana 可以用同一个程序加不同的等级常量表示，或者只发 Signal 不计入 Combo。
   是否计入 Combo 由 Ruleset 的折叠规则决定。

### 8.5 判定区重叠（Phigros 竖直线）

Phigros 的判定区是垂直于判定线的无限长条，不同时间落下的音符区域会互相重叠。判定要求是：
只把输入交给完美判定时刻最接近的那个音符。

这个需求由第 8 节的 `claimKey` 直接满足，不需要新原语。

```text
program tap              // 与 7.1 相同
  claimKey  abs(evt.t - anchor)
  dispatch  consume
```

要正确工作，需要下面几条一起成立：

1. 候选过滤先执行。窗口不含 `evt.t` 的音符在排序前就被守卫剔除，否则"最近的锚点"可能
   落在一个窗口外的音符上。
2. 锚点完全相同时，取最小值会丢掉一个音符。这时改用 `fanout = all`，让同一次输入同时
   结算所有同名锚点，用来支持多押。
3. `claimKey` 相同时按 Requirement identity 破平局，保证确定性。
4. 重叠判定区在准备阶段就要算进活动度预算：同一时刻可能有多个实例处于 Live。

发现的问题：

1. 无限长条本身只是区域形状的一种，属于 Ruleset Interface 的区域声明，不需要新原语。
   但它可以有开放式边界，例如 `x` 方向无限延伸、`y` 方向有范围。区域类型需要支持"只在一维上有界"。
2. 如果判定线本身会移动，静态区域就不够用了。这时区域也要按时间采样，也就是需要一个
   几何轨道（geometry track）概念。该机制由 §5.4 提供：区域绑定一个 frame，形状在 frame
   局部坐标中保持静态。
3. 排序键只解决"一次输入给谁"，不解决"一个音符被多次输入命中"。重复输入的规则由程序决定，
   例如结算后就 `settle`，之后不再接收事件。

### 8.6 宽度随时间变化的 Slider（类 Malody 无轨）

Slider 是一条带时间参数的路径，加上一个随时间变化的宽度：

```text
区间 [t0, t1]，段数 m
path(T)   在锚点列表上按时间分段线性插值得到的坐标
width(T)  同样分段线性，允许每段不同
```

```text
program slider
  params     t0: Tick, t1: Tick, m: Int[1..16],
             anchors: Array<Point, 16>, widths: Array<Int, 16>
  registers  c: Contact, seg: Int[0..15]
             observable following: Bool := false
  arm        t0 - W.max.early
  deadline   t1 + W.max.late
  claimKey   abs(evt.t - t0)
  dispatch   consume

  state Head
    on input(press, inRegion(sliderArea))
      guard grade(evt.t - t0, W) != none
      do    claim(evt.contact, 0); set seg := 0
            emit Hit(head, grade(evt.t - t0, W), evt.t - t0)
      goto  Follow
    on timer(t0 + W.late)
      do    emit Miss(head); settle(miss)

  state Follow
    level
      guard seg < m - 1 && evt.t >= segEnd(seg)
      do    set seg := seg + 1
      progress seg
    level
      guard evt.t >= t1
      do    emit Hit(tail, grade(evt.t - t1, W), evt.t - t1); settle(hit)
      progress seg
    on input(contactMove, c)
      guard inside(c.pos, path(seg, evt.t), width(seg, evt.t))
      do    set following := true
    on input(contactMove, c)
      do    set following := false
    on input(contactEnd, c)
      do    set following := false
            emit Break(body, evt.t); settle(broken)

  onDeadline  emit Miss(tail); settle(miss)
```

`path(seg, T)` 与 `width(seg, T)` 是段内的仿射函数，只用整数运算。整段的宽度变化只是
`width` 从常量变成变量，IR 层面没有区别。Hold 是这个程序在 `m = 1`、宽度恒定时的特例。

发现的问题：

1. 位置谓词只在实际到达的输入事件上求值。两次 move 之间接触点短暂离开又回来检测不到。
   这是有意的近似：事件频率就是采样率。需要写进作者文档。如果某个 Ruleset 要求严格
   不出界，需要可选的按固定速率采样模式，并声明相应代价，这属于后续能力。
2. `inside` 需要能用"点 + 中心线 + 半宽"表达，因此区域形状要支持"以路径为中心、半宽可变的
   走廊"，而不只是静态多边形。
### 8.7 惩罚音符（炸弹）

炸弹是"被触发即惩罚、不被触发即无事"的要求。它不抢输入，也不进 Combo 分母。

```text
program bomb
  params     anchor: Tick, lane: Channel
  arm        anchor - Wb.max.early
  deadline   anchor + Wb.max.late
  dispatch   observe
  category   penalty

  state Wait
    on input(press, lane)
      guard held(lane) && evt.t >= anchor - Wb.bad && evt.t <= anchor + Wb.bad
      do    emit Hit(head, Grade.detonated, evt.t - anchor)
            settle(detonated)

  onDeadline  settle(avoided)
```

要点：

1. `dispatch = observe` 决定炸弹不参与仲裁：它不会把输入从同轨道的普通音符手里抢走，
   但仍然能收到事件并判定。这就是 4.1 从"按判定域声明"改为"按 Requirement 声明"的原因：
   炸弹和普通音符通常同域。
2. `category = penalty` 让折叠表知道它既不计入 Combo 分母，也不破坏 All Perfect。
3. `settle(avoided)` 必须是一个合法的结算结果。这说明结算结果枚举由 Ruleset 声明，
   引擎不规定哪个结果算好。
4. `Grade.detonated` 是 Ruleset 声明的常量等级，不是由 `grade()` 从窗口算出来的。

发现的问题：

1. 如果炸弹和普通音符锚点相同，observe 的炸弹和 consume 的音符会同时收到事件，
   这是正确的行为，两边都按自己的规则结算。
2. "免疫炸弹"这类技能不需要写程序：它改的是折叠表对 `penalty` 类别的处理，或者改
   `Wb` 的挂点。安全性来自"技能是数据"这条设计。
3. 反向的惩罚音符（碰到就加分）也是同一形状，只是 category 不同，程序不需要改。

### 8.8 Arcaea 的 Arc 与天空音符

天空音符是走廊上的可判定点，形状与 Tap 相同（`point` 区间加位置约束），可表达。

Arc 中段跟随与 §8.6 的 Slider 同形：`level` 边守着 `inside(c.pos, path(T), width(T))`。
但 Arc 暴露了 §8.6 没有暴露的一种情形：**接触点不动，走廊移动**。

此时谓词从真变假，但**没有任何输入事件发生**，`level` 不会被唤醒，断开永远检测不到。

§5.1 说"线性插值可以回答任意时刻的查询"，这没有错，但**插值只回答查询，不产生事件**。
程序只在输入事件到达或定时器到期时被唤醒。这是缺口一（§8.12）。

### 8.9 Taiko 连打与 DJMAX 长按连打

连打等于在时间区间内累计命中次数，按次数评级或通过。

```text
program roll
  params     start: Tick, end: Tick, sink: Channel
  registers  observable count: Int[0..256] := 0
  arm        start
  deadline   end
  dispatch   consume

  state Rolling
    on input(press, sink)
      guard count < 256
      do    set count := count + 1
    on timer(end)
      do    emit Hit(tail, gradeFromCount(count), 0); settle(hit)

  onDeadline  emit Hit(tail, gradeFromCount(count), 0); settle(hit)
```

**不需要新原语**：有界寄存器累加加定时器结算即可。DJMAX 的长按连打同理，把累加换成在已知
刻度上检查 `held(channel)`。

这一条验证了"有界寄存器 + 定时器"对累计型判定是够用的。

### 8.10 osu! 的 Spinner 与方向判定

Spinner 要求累积旋转角度达到阈值。累积需要每次 `move` 事件算出相邻两次位置的转角增量
并判断转向，而守卫语言的算术只有 `+ - * min max abs clamp`，没有向量运算。表达不了。

flick 同属一类（Stage 7B 的 Flick 与方向动作）：方向来自
`cross(evt.pos - beginPos, dirAxis)` 的符号。

这是缺口二（§8.12）：守卫表达式需要一组几何原语，且都必须整数可实现。

```text
dot(a, b)     内积
cross(a, b)   二维叉积，符号用于判转向
len2(a)       长度平方，避免开方
```

`cross` 的符号判转向，`dot` 的正负判前进或回摆，`len2` 与平方阈值比较代替开方。全部是
整数乘加，保持闭式与可判定。

### 8.11 多指 Slide

一条 Slide 要求同时跟踪多个接触点。§8.6 的 `c: Contact` 是单值，需要改成定长数组。

寄存器类型已允许 `Array<Contact, N>`，配套需要两件事：

```text
认领到槽位    claim(evt.contact, slot)
按 contact 找槽   some k in 0..N-1 : slot[k] == evt.contact
```

有界搜索已经具备。所以多指 Slide 在现有原语上可表达，只需把 `claim` 的动作签名从单值扩展
为带槽位。这是措辞修正，不是新原语，但它属于缺口三，因为它影响所有涉及多接触点的程序。

### 8.12 本轮压测发现的三个缺口及处置

三个缺口都已设计完成，正文位置如下。

```text
缺口一  连续谓词在两次输入事件之间的变化不产生事件
        -> 已设计：§2.4 的 every(period) 状态级周期采样

缺口二  守卫表达式缺少几何原语
        -> 已设计：§4.3 的 vec / posAt / sub / dot / cross / len2 / sqrtApprox

缺口三  claim 的动作签名是单值
        -> 已设计：§4.4 的 claim(contact, slot) 与 §4.5 的归属与可见性模型
```

**缺口一最重要，因为它修正了一个此前的判断。** §5.1 曾论证"不需要 `sampled` 逃生通道，
因为线性插值可以回答任意时刻的查询"。该论证漏了一步：插值回答查询，但不产生事件。这是
两个不同的问题，而 Arcaea 的 Arc（手指不动、走廊移动）正是踩中的情形。

处置要点：

- 采样声明在**状态**上（`every`），不是程序实例上，所以只在需要的状态付费。
- 周期由 Ruleset 声明基线与允许区间，内容在区间内声明覆盖值。**不写死梯级**，游戏作者
  与谱面作者都保留自定义空间；但周期必须被声明，因为它影响判定结果，所以要进 Interface
  投影与判定 identity。
- 语义定义为"采样得到"，不是"精确求交"。若引擎精确求交，断开时刻就依赖引擎能力；按采样
  定义则只依赖声明的格点。解析求交将来只能是 Ruleset 声明的另一种模式（带 capability），
  不能是同一模式下的实现优化，因为两者结果不同。
- 与 §5.3 的 `sampled` 区分：那是控制器求值的降级模式，这是程序实例的周期性唤醒。

它不削弱"判定独立于帧率"：采样时刻是预先声明的绝对刻度，与渲染帧率无关。

**缺口二**是纯增量：新增的几何原语都是整数乘加，验证器的区间分析按同样规则处理，代价上界
按乘加次数更新。它同时是 Stage 7B 的 Flick 与方向动作的前置。`cross` 的符号判转向，
`dot` 的正负判前进或回摆，`len2` 与平方阈值比较代替开方。

**缺口三**不只是动作签名，还需要归属与可见性分离（§4.5）。只改签名会在多音符判定区重合时
出错：`sticky` 的实例（例如 Arc）需要**看到**已被他人认领的接触点才能判出"换手即断开"，
所以"观察型边始终可见"是必需的，不是可选优化。

## 9. 静态验证

prepare 时逐个程序、逐个实例检查以下各项，任一项失败都拒绝整张谱面。

| 检查 | 方法 |
| --- | --- |
| 类型和引用 | Channel、Region、Grade、Hook、Controller、frame 必须在 Ruleset Interface 或谱面中已声明 |
| 取值范围 | 对所有算术和寄存器写入做区间分析，可能越界或溢出即拒绝 |
| 必然结算 | 必须有 `onDeadline`，而且它会 settle；所有 timer 表达式的上界 ≤ deadline |
| 同 Tick 终止 | 输入和 timer 边每次事件最多触发一次；`level` 边必须声明 progress 度量并严格增加 |
| 结算唯一 | 每条路径上 `settle` 最多执行一次；settle 之后不可达的边给出警告 |
| 规模 | 状态数、边数、寄存器数、表达式节点数、数组长度、轨道分段数都有上限 |
| 活动度 | 用 arm 和 deadline 做扫描线，算出最大同时存活的实例数，要求 ≤ 预算 |
| 单事件代价 | 活动度 × 候选边数 × （表达式代价 + frame 求值代价），要求 ≤ 预算 |
| 轨道单调性 | 轨道分段必须按 Tick 严格升序且不重叠、无空洞，覆盖 `[start, end)` |
| frame 不可变 | 没有任何程序能写 frame；这是结构上的保证，不需要额外检查 |
| 采样周期下限 | `every(period)` 的周期必须落在 Ruleset 声明的允许区间内；周期唤醒总量计入每 Tick 代价上界 |
| 槽位下标有界 | `claim(contact, slot)` 的槽位下标必须由有界搜索或常量给出 |

单事件代价现在包含 frame 求值。每个涉及几何的守卫求解时是一次二分加常数次定点运算，
复杂度 O(log N)，N 是分段数且有静态上界，所以这一项仍可静态预算。

`every(period)` 的代价按"实例数 × 在 `every` 状态内的窗口 / 周期"计算，进入预算。
它不削弱"判定独立于帧率"：采样时刻是预先声明的绝对刻度，与渲染帧率无关。

## 10. 结论与待决问题

十二个案例中，九个可表达，三个压出了缺口。

```text
8.1  Tap                 可表达
8.2  中途可断开的 Hold     可表达
8.3  带跳区的 maimai 星星  可表达
8.4  osu!catch 水果       可表达
8.5  判定区重叠           可表达
8.6  变宽 Slider          可表达
8.7  惩罚音符             可表达
8.8  Arcaea Arc 与天空音符 可表达，但暴露缺口一
8.9  Taiko / DJMAX 连打    可表达，不需要新原语
8.10 osu! Spinner 与方向    不可表达，暴露缺口二
8.11 多指 Slide            可表达，但暴露缺口三
8.12 移动判定线           由 §5.4 的 frame 机制覆盖，不改变程序 IR
```

上述十二项中，"移动判定线"是 Phigros 的案例，由 §5.4 覆盖；其余十一项是本节 8.1 至 8.11
的实例案例。本轮新增的是 8.8 至 8.11 四项。

为此在第一版原语上补充了以下内容：

```text
contact 过滤的输入              来自 Hold
timer 读寄存器/Hook + 静态上界   来自 Hold
some k / firstk 有界搜索        来自 Slide
level 触发 + progress 度量      来自 Slide
观察型输入                      来自 Slide
解析求值的 Controller           来自 Catch
同触发器多边的声明顺序           来自 Catch
dispatch 下放到 Requirement     来自 炸弹（不能按判定域声明）
Ruleset 声明的 category         来自 炸弹
Ruleset 声明的结算结果枚举       来自 炸弹
every(period) + 几何原语 + 槽位  来自 §8.12 的三个缺口
```

**缺口一是对 §5.1 一处论证的修正，值得单独记住。** 该节曾论证"不需要 `sampled` 逃生通道，
因为线性插值可以回答任意时刻的查询"。论证漏了一步：插值回答查询，但不产生事件。连续谓词
在两次输入事件之间由真变假时，程序不会被唤醒。结论仍然成立——不需要 `sampled`——但理由
不成立，正确的修法是状态级 `every(period)`，而不是靠插值。

后续三轮讨论收敛了四项，均已写入正文：

```text
几何轨道        §5.4   形状静态 + 摆放采样，不扩展程序 IR
数值模型        §5.5   作者侧浮点，决策侧整数，prepare 量化一次
输入模型        §5.1   两类事件加一类状态，映射与插值都在 L1
输入重采样      §5.2   离散边沿保留原时间戳，连续量按规范格点重采样
Controller 自定义 §5.3  事件侧可编程，时间侧封闭，三个外推基加一个后处理算子
```

待决：

1. ~~Hook 是否允许在演奏中动态改变判定窗口~~ 已裁决：**允许，且必须声明静态上界**。
   完整论证与代价分摊如下。

   ```text
   允许      窗口表可被派生 Hook 缩放与整体偏移，Hook 可在演奏中变化
   必须声明  每个派生 Hook 的取值区间在 Interface 中静态给出（Ruleset Fold 草案 §4.1）
   代价落在  1) L2 验证用区间分析而非具体值，因此验证结果与 Loadout 无关（同草案 §4.6）
             2) 窗口集在区间内任意组合下必须保持严格有序（Fold Calculus §6.1 的通用不变量）
             3) 缩放上限还受活动度预算约束（预算草案 §3.1 第 3 类）
   ```

   第 3 条是本项此前没有写出的代价：**窗口缩放上限先影响活动度，再影响判定**。
   放宽窗口会延长休眠 requirement 的活动期，从而提高每 Tick 的代价乘数。
   因此声明范围时不能只考虑判定公平性，`windowScale` 的上界要同时过活动度预算。
2. ~~缓动（`Ease`）曲线的最终集合~~ 已在
   [引擎冻结合同草案](GAMEPLAY_ENGINE_FROZEN_CONTRACTS_DRAFT.md) §4 裁决：先定准入判据
   （必须有闭式整数求值、误差可静态界定、不调用宿主数学库），再给九条起始集合
   （linear / quad in-out-inout / cubic in-out-inout / backIn / backOut，其中 inOut 是派生）。
   `elastic` 因需要 `sinusoid` 表而被排除，不是永久禁止——等该表进入登记表即可加入。
3. Ruleset 自身的逻辑见
   [Ruleset Fold Language 草案](GAMEPLAY_RULESET_FOLD_DRAFT.md)。§9 的检查项在
   [预算与规模上界草案](GAMEPLAY_BUDGET_AND_SCALE_DRAFT.md) §6 与其余三份草案合并为一份
   完整清单，并增补了活动度、确定化规模、采样负载与内联 AST 四项。
4. 作者语法：面向谱师的写法，可以是 JSON 形式的 IR、类似上文的文本 DSL，或者 Studio 的
   可视化编辑，IR 只是它们共同的编译目标。
5. ~~由运行期状态驱动的 frame 不在本设计内~~ 已在 §5.4b 处置：frame 的来源可以是 `track`
   或 `bound(ctrl)`，两者都满足不可变与快照要求。仍不覆盖的是 3D 空间玩法与连续音高输入；
   两者的扩展路径与不变量见 §5.6，其中 §5.6.1 指出扫掠体判定超出"加一维坐标"的范围。
6. ~~还需要继续压测的案例~~ 已关闭。本项列的五项全部进入了
   [Gameplay 设计压测与缺陷登记](GAMEPLAY_STRESS_TEST_DRAFT.md) §2 的 34 项案例：
   Arcaea Arc 与 Sky Note（案例 27 与本文 §8.8）、Taiko 连打（14、15）、
   DJMAX 长按连打（15）、多指 Slide（本文 §8.11）、osu! Slider（31 与本文 §8.6）。
   新增的 22 项中没有一项需要新的判定原语，见该文 §5 的收敛趋势。
7. ~~定点表的集合与版本~~ 已在
   [引擎冻结合同草案](GAMEPLAY_ENGINE_FROZEN_CONTRACTS_DRAFT.md) §2–§3 处置：
   统一登记点为 `tableRegistry`，每个条目带 tableId、量化域、分辨率、误差界与
   一致性测试向量；表 id 即语义内容，只增不减。
8. ~~L1 的朝向融合算法需要独立规范~~ 已在 §5.2b 处置：接口只保证输出单位与值域、单调性、
   单圈值域三件事，融合算法属 L1 实现并不进 identity。§5.2b 同时说明为什么"改变判定结果
   却不进 identity"不矛盾——朝向融合与输入映射是同一类处理，都烘进事件流。
