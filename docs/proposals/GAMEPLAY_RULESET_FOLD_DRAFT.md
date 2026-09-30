# Ruleset Fold Language 草案（L3）

状态：candidate（设计草案，未接受，未实施）

更新日期：2026-09-29

本文回应 [Gameplay Ruleset 设计讨论记录](GAMEPLAY_RULESET_DISCUSSION.md) 下一步工作 A：L3
Ruleset Core 的语言与打包。前置决定见该文 §2.9 与 §2.10：四层分层、L3 是可分发内容、L3 不
复用 L2 的 IR。

## 1. 定位与信任模型

L3 是可分发的游戏模式内容，作者可能是第三方。因此 L3 与 L2 一样按不可信内容处理：全量
在 prepare 时验证，演奏中不出现程序错误。区别不在信任级别，而在权限与计算形状。

| | L2 Requirement Program | L3 Ruleset Fold |
| --- | --- | --- |
| 权限 | 只产出 Fact 与可观察状态 | 定义 Interface、Hook、窗口、计分、技能 |
| 形状 | 每实例一个自动机，实例间并发 | 对单一全序事实流的全局折叠 |
| 数量 | 数万实例 | 每个会话一个核心，加若干模块 |
| 冻结 | 最早、最严 | Interface 稳定，实现自由 |

L2 的静态验证只读取 L3 声明的 Interface，不读取 L3 的实现。这是两层解耦成立的关键，
也让 L2 的程序可以在不知道规则细节的情况下被验证。

## 2. 执行模型

### 2.1 单一事件队列

L2 实例、L3 核心和会话事件共用一条全序队列。队列元素有三类：

```text
定时器    由 L2 实例或 L3 核心声明的绝对 Tick
输入事件  归一化后的输入观察
会话事件  SessionStart、Seek、Reload、Finalize
```

同一 Tick 内的处理顺序沿用 L2 的约定并扩展：

1. 激活 `armTick == t` 的 L2 实例
2. 定时器，按 `(ownerKind, ownerId, edgeIndex)` 定序
3. 输入事件，按 sequence
4. 折叠：先执行 L3 自己的定时器状态推进，再按发出顺序折叠本 Tick 产生的 Fact

Hook 的写入在第 4 步，按 t+1 规则在下一个 Tick 对判定生效。这条规定是
[Bounded Fold Calculus 提案](GAMEPLAY_FOLD_CALCULUS_DRAFT.md) §5.6 链式依赖通道的实现
依赖，两份文档已核对一致（同文 §5.9）。

### 2.2 时间推进

L3 不接收"每帧回调"。它声明自己需要在哪些 Tick 被唤醒，引擎把它们并入队列。因此
"生命值随时间下降"这类规则会写出显式定时器，而不是依赖显示帧率。

`GameplayState(T)` 是导出字段在"最后一个不晚于 T 的事件处理完毕"后的值，是纯查询，不执行
程序。两次事件之间的连续变化属于表现层插值，不得反过来影响判定。

### 2.3 Seek 与快照

L3 的状态必须是可序列化的有界值。Seek 的做法与 L2 相同：从最近快照重新折叠。快照包含
L3 的全部状态、Hook 值、L2 实例状态和待触发定时器。

## 3. 语言形状

与 L2 共用表达式核、类型系统和 Fact 词汇表，只在下沉到语句层时分开。

```text
类型      Int[lo..hi]、Int64、Bool、Enum<E>、Tick、Array<T, N>、
          Table<E1, E2, T>   按声明枚举索引的定长表
值        只用整数。分数为 Int64；准确率用整数分子分母，展示时换算
语句      赋值、if/else、有界 for
处理器    on SessionStart / on Seek / on Finalize / on Timer / on Fact / on Signal
禁止      while、递归、函数指针、动态容器、浮点、宿主调用、IO、随机、墙钟
```

`Table<Category, Outcome, Int32>` 用来表达按类别和结果分类的统计，不需要动态容器。

没有 `on Input` 处理器。输入属于 L2；表现层关心的按键反馈从 L1 的输入观察读取。

### 3.1 与 L2 共享的表达式核：逐项对齐（回应 §8.7）

Fold Calculus §5.7 已裁决"共享表达式核，不共享算子集"，但本文原只给了一个概述。
逐项对齐如下，**这是两层之间唯一的共享面，超出这张表的东西一律不共享**。

| 条目 | 共享 | 说明 |
| --- | --- | --- |
| `Int[lo..hi]` / `Int64` / `Bool` / `Tick` | 是 | 数值类型与区间分析规则同一套 |
| `Enum<E>` | 是 | 但**等级的枚举集合不同**：L2 的是 Interface 声明的 grade，L3 的是同一集合的变量 |
| `Array<T, N>` | 是 | 定长、长度静态 |
| `Table<E1, E2, T>` | **L3 独有** | 按声明枚举索引的定长表。L2 不需要：Pattern 是编译成自动机的，不写表 |
| 算术与比较 | 是 | 含溢出检查规则 |
| `fact` / `signal` / `counter` / `combo` / `accuracy` / `life` / `time` | **L3 独有** | 它们读的是会话级量，L2 看不到 |
| `level` / `held` 电平查询 | 是 | 技能的触发条件要读它，见 Skill Hooks §5。同一组查询，不是两套 |
| 几何原语 `vec` / `dot` / `cross` / `len2` / `sqrtApprox` | 是（原语本身），**但默认不可达** | 见下 |
| `region` / `frame` / `contactIn` | **不共享** | L2 做命中检测需要，L3 不做命中检测 |
| 派生 Hook 的合成算子 | **L3 独有** | L2 只读合成结果，不参与合成 |

**几何原语这一行需要单独说明**，因为它是唯一"共享实现但默认关闭"的一条。

```text
L3 默认不能读 region 与 frame    理由：L3 不做命中检测。若允许读，技能就可以自行
                              重算判定域，与"L2 是唯一做判定的地方"冲突
L3 可以用几何原语做普通算术      例如求两个导出坐标之间的距离，用于输出或技能条件
                              这不触碰判定，因为它的输入不是 contact
```

因此接口把两者分开：几何**函数**是共享的整数运算，几何**数据源**（region、frame、
contact）不向 L3 开放。前者是算术，后者才是判定。

**三种"不共享"的理由各不相同**，分开写以免被当成同一件事：

```text
Table               L2 的结构决定它不写表，不是禁止
fact/counter 等      它们的定义域是会话，L2 的实例没有会话视角
region/frame        这是权限边界，不是能力差异。L3 拿到了就能绕开 L2
```

**对齐的收益是只维护一套。** 两套类型检查器、两套区间分析、两套溢出规则会各自漂移，
而漂移的表现是"同一条表达式在 L2 合法、在 L3 不合法"——作者无法理解的诊断。
共享之后验证器的差异只剩两条：L3 多一个 `Table` 检查，L2 多一组 `region`/`frame` 检查。

### 3.2 L3 的量与预算（回应 §8.6）

**裁决：L3 与 L2 分开计量，不合并。**

理由来自两者的形状差异（§1 的表）：

```text
L2   每实例一个自动机，数万实例并发。代价随活动度变化，是"每 Tick 多少条实例在跑"
L3   每个会话一个核心加若干模块。代价是定值，与谱面规模无关
```

合并两者会得到一个随活动度变化的量加上一个定值，而这个和**掩盖了两个不同的失败现象**：
L2 超预算是"高密谱面掉帧"，L3 超预算是"模块写太多、每 Tick 都在跑"。修法完全不同，
所以按 [预算与规模上界草案](GAMEPLAY_BUDGET_AND_SCALE_DRAFT.md) §1 的"三种成本不合并"原则，
它们也不能合并。

L3 的预算条目：

```text
moduleCount          启用的模块数
l3StateBytes         核心与全部模块的状态字节数之和
l3TimerCount         声明过的定时器总数
l3FactCostPerSecond  每 Fact 处理器代价 × 每秒 Fact 数
l3PeriodicWakeups    every 类事件的每秒唤醒次数
```

**`l3FactCostPerSecond` 是 L3 侧的主要风险项，值得说明它为什么不是常数。**
Fact 数与谱面密度成正比（一个 40,000 requirement 的谱面会产生同量级的 Fact），
因此 L3 的折叠代价实际**随谱面规模线性增长**——只是系数由折叠实现决定，而不是由模块数决定。

这与 §1 表里"L3 数每个会话一个核心"并不矛盾：**核心是一个，但它每 Tick 处理的 Fact 数
随谱面变化**。两者是不同的轴，不能互相抵消。

预算数值同样是候选值，与预算草案 §3.5 的表合并冻结，流程相同。

## 4. 声明块

一个 Ruleset 包由五个声明块组成。

### 4.1 interface

```text
grade     有序等级集合，索引即名次。例如 perfect / great / good / bad / miss
          可以在集合中声明常量等级（例如 protected），由 L2 程序直接引用，见 4.8
category  note / penalty / bonus / filler 等
outcome   结算结果枚举，例如 hit / miss / broken / avoided / detonated
phase     阶段枚举，例如 head / body / tail
window    命名窗口表，每个等级一个带符号区间，可被 Hook 缩放
          （即 Fold Calculus 提案 §3.6 的等级集合与 Grading 依据）
hook      类型化变量及其静态取值范围，例如 windowScale: Int[750..1250]
          每个 Hook 还必须声明 derived（带合成算子）或 owned（唯一所有者），见 4.7
region    判定域：离散 channel 集合、路径走廊、一维有界区域等
controller 形状与参数
field     导出给 GameplayState 的字段
module    模块清单：group 与 conflicts 声明，见 4.7
```

L2 的验证器只依赖本块。

### 4.2 arbitration

```text
candidates  consume
order by    claimKey, requirementId
lock        channel: oldest_unsettled
fanout      equal_anchor: all
```

只做过滤与排序，不读其他实例的内部寄存器。

### 4.3 fold

对事实流的处理器集合。声明式折叠表是它的一个预置模块：只填表不写代码即可得到一个常规
计分规则。

### 4.4 modules

技能与角色是模块。模块可以被启用、禁用、设置参数：

```text
module comboShield
  params   charges: Int[0..3] = 1
  hooks    missShield := charges
  on Fact(cat: note, outcome: miss)
    when hook.missShield > 0
    do   hook.missShield := hook.missShield - 1
         signal(shieldUsed)
```

Loadout 就是"启用哪些模块 + 参数取值"。它的 identity 进入 Replay。

### 4.5 defaults

供表现层使用的默认绑定与默认资源，属于 L4 的输入，不是判定的一部分。

### 4.6 结构与值：模块的可改范围

规则：**Interface 静态，值动态，导出增量。** 模块可以在 Interface 已经声明的槽位上写值，
不能新增或删除槽位。

| 结构 | 模块可改 | 说明 |
| --- | --- | --- |
| Interface：等级、结算结果、Hook 槽位、region、controller、窗口集 | 否 | 只能写值，且值必须落在 Interface 声明的范围内 |
| 导出字段（GameplayState） | 可新增 | 纯增量。L2 不读导出字段，只有 L3 与表现层读 |
| fold 处理器注册 | 可新增 | 只写导出字段、Hook 与信号，不能写 L2 |

这样 Interface 是与 Loadout 无关的包级静态合同，因此 L2 的静态验证结果不依赖 Loadout。
如果允许模块改 Interface 结构，会依次付出四笔代价：L2 的验证结果依赖 Loadout；
枚举扩展向两侧破裂（`grade()` 可能返回作者未知的值，作者也无法遍历完整等级集合）；
快照与 Replay 必须绑定"生效后的 Interface"，而不能只对包取摘要；
谱师与 Studio 失去静态可计算的边界。

需要新等级、新结算结果、新 region 或新 Hook 的真实需求，正确表达是新的 Interface 版本或
另一个包，而不是模块扩展。这与"一张谱面对应一种 Ruleset（兼容变更除外）"的既有约定一致。

逃生通道：需要 Loadout 改变 Interface 的包必须显式声明 `interfaceVariesWithLoadout = true`。
该标记使 L2 的验证无法在 prepare 时完成，代价在包级明文记录，默认关闭。

### 4.7 模块合成与互斥

目标不是给共享写入定序，而是**消除共享写入**。只要每个可写目标恰好有一个写入者，组合就
与顺序无关。

**派生 Hook（derived）**。窗口缩放、分数倍率这类"技能影响数值"的需求都是这一类。它不赋值，
只接受贡献：

```text
windowScale = combine( base, [贡献...] )
```

Interface 为每个派生 Hook 声明合成算子，并且只允许可交换、可结合的算子：`add`、`mul`、
`min`、`max`、`or`。合成必须对**整个贡献集合一次算完**，不能两两递推，因为整数取整会破坏
结合律。缩放类数值统一用千分比整数，除法取整规则由 Interface 声明。合成结果按声明的范围 clamp。

**独占 Hook（owned）**。`missShield`、`life` 这类会被读改写的状态变量，要求恰好一个所有者。
所有者可以是核心，或者被某个模块声明占用。两个启用中的模块声明写同一个独占 Hook 时，
**Loadout 校验直接拒绝**，不进运行期。

**折叠表按 key 单写者**。"免疫炸弹"这类技能用 `override` 声明覆盖某个
`(category, outcome)` 的处理。同一个 key 最多一个启用模块可以覆盖，违反者同样在校验期拒绝。

**处理器顺序冻结在包里**。顺序是：核心处理器，然后各模块按包清单的声明顺序，模块内部按
声明顺序。Loadout 只能决定启用哪些模块和参数取值，不能改变顺序；顺序写入包的内容 hash，
因此 Replay 绑定包即绑定顺序。同一 Tick 内的折叠逐条事实推进，后一条能看到前一条的写入，
所以同一模块在同一 Tick 的多个处理器不会丢失更新。

**信号不重入**。模块之间通信用信号，信号只在下一个 Tick 投递，不在同一 Tick 内级联。这使得
信号环在结构上不可能出现，不必做环检测。整个系统因此只有一条时序规则：世界每次只推进一个
Tick，任何东西都看不到同一 Tick 的写入。代价是 1 微秒量级的延迟。

**互斥**。顺序问题解决后，互斥是独立的一点，用声明表达：

```text
module lifeSystemA   group: lifeSystem
module lifeSystemB   group: lifeSystem
module skinX         conflicts: [lifeSystemB]
```

`group` 表示同组最多启用一个，`conflicts` 表示两者不能同时启用。

Loadout 校验只报四类错，全部是静态的、在 prepare 时暴露：

```text
独占 Hook 有两个所有者
折叠表 key 有两个覆盖者
同一 group 启用了多个模块
命中声明的 conflicts 对
```

**中途换装**。若游戏允许中途换角色或技能，不要让它成为运行期可变状态。把"何时启用哪个
模块"做成声明式启用表写入 Loadout，Loadout 本身仍在 prepare 前冻结。identity 不变，
中途切换仍可表达。

**代价**：两个模块不能真正共享一个资源计数器（要共享必须走信号，多一 Tick 延迟）；
模块看不到另一个模块在同一 Tick 对派生 Hook 的贡献（看到的永远是上一 Tick 的值）；
需要"两个技能同时改同一数值"时必须用派生 Hook 的合成算子表达，不能用"先 A 后 B"的顺序表达。

### 4.8 保护音符

街机音游中的"保护音符"（若在可判定区间内命中，结果必定为 perfect）不需要新机制。它是
Interface 声明的等级集合里的一个常量等级，由 L2 程序在匹配后直接发出，例如
`Hit(head, Grade.protected, err)`，而不是通过 `grade()` 从窗口算出。

判定的严格性因此可以按音符分别声明，而不是全局固定。这与引擎只把 Fact 当事实处理的设计一致：
引擎不认为 `Grade.protected` 比 `Grade.perfect` 更好，谁更好由 L3 的折叠规则决定。

一个需要显式确定的范围：保护音符能否缓冲突 Miss。做法是把这类行为放进 L3 的折叠规则
（例如把 `miss` 转换为 `perfect` 的模块），而不是让 L2 程序去改别的实例。落到哪个范围
写在 L3，L2 保持所见即所得。

### 4.9 programPolicy

```text
locked      只能使用包内的 L2 程序库
extendable  可以新增程序，但只能产出包内已声明的 category 与 grade（默认）
open        可以新增程序与新的 grade 产出行为
```

**本条只定义来源轴。产出轴是独立的 `outcomeScope`，见 §6.2。** 两个轴不能合并，
理由与裁决见该节。

默认 `extendable`，因为它是"允许谱师编写自己的判定形状"与"计分与统计仍然可比"之间的
平衡点。

## 5. 输出

L3 只能通过以下三个出口影响外部：

```text
导出字段   GameplayState(T) 的只读内容
信号       给表现层或统计的事件，不改变判定
命令       给 Controller，按 t+1 生效（例如 hyperdash）
```

L3 不能写回 L2 的实例状态、不能生成新的 Requirement、不能改变已发出 Fact 的内容。

## 6. 打包与兼容

Ruleset 是一个可分发的包。包内包含：interface 声明、fold 程序、L2 程序库、窗口表、
region 与 controller 声明、模块与默认参数、表现默认绑定、资源。本节给出五个打包决定的裁决，
包的完整构成、版本轴与 hash 覆盖范围见 §6.5。

谱面可以引用包内的 L2 程序库，也可以自带程序。自带程序的权限由 interface 的 `programPolicy`
与 `outcomeScope` 决定，取值见 §4.9 与 §6.2。`interfaceVariesWithLoadout`（§4.6）在这里生效。

### 6.1 内置原生模式与分发模式共用 Interface

**裁决：允许，而且应当成为常态而不是例外。**

这两种"模式"的区别此前被表述成表示形式的区别（引擎内置 vs 可分发包），但它实际上只是
**谁发行**的区别。把它们做成两种表示会带来一串不必要的分叉：两套加载路径、两套验证入口、
两套 identity 计算。

因此把内置模式也做成包——随引擎发行、但仍有自己的内容 hash——三件事同时简化：

```text
一种加载路径     引擎发行目录与用户目录只是两个来源，不是两条代码路径
一种验证入口     §7 的验证对两者同样强制
identity 分量不变  内置模式的 hash 仍落在 ruleset 分量，不落进 engine 分量
```

**第三条是这条裁决真正的价值，值得单独说明。** 若内置模式的 Build 直接编进引擎代码，
它的任何数值调整都会变成 engine 分量的变化，而 engine 分量是全局的——于是"给内置模式微调一个
窗口"会让**该引擎上全部已发行的 Replay**失效，包括其他模式的。

这正是 §2.5 把 Ruleset 拆成 Interface 与 Build 两层要避免的事。做成包之后，调整只改变那个包
的 hash，失效范围恰好是使用它的谱面。

**代价**：内置模式不能再"直接调用引擎内部函数"，必须走 L3 的公开表面。这与讨论记录 §2.10
已经确定的"L3 是可分发内容、拥有自己的语言"一致，不构成新限制。真正的内部能力（定点表、
几何原语、时间域）本来就通过 Interface 声明而不是通过函数调用暴露。

### 6.2 programPolicy 的两个轴（回应 §8.4）

`programPolicy` 的三个取值此前把两件不同的事压在一个轴上：

```text
来源轴   谱面的程序从哪来    包内库 / 可以新增
产出轴   谱面的程序能产出什么  既有取值 / 新行为
```

**裁决：拆成两个轴，各自封闭。**

```text
programPolicy  = locked | extendable | open      （来源轴，保持原义）
                 locked      只能用包内的 L2 程序库
                 extendable  可以新增程序（默认）
                 open        可以新增程序，且其判定规则由谱面自定

outcomeScope   = declared | extended             （产出轴，新增）
                 declared    只能产出包内已声明的 outcome 与 category（默认）
                 extended    可以声明新的 outcome 与 category
```

**两轴不是正交组合，产出轴被独立收紧**：`outcomeScope = extended` 只在
`programPolicy = open` 下允许声明，否则静态拒绝。

**为什么 outcome 必须保持封闭（这是 §8.4 的答案）。** 新增一个 `outcome` 等于新增一个折叠
键。§7 已有一条检查："所有可结算的事件类型都有处理器或落入显式忽略"。若谱面的程序能自创
outcome，这条检查要么失败（拒绝整张谱面），要么它必须退化成"忽略未知 outcome"——
而后者会让判定结果依赖 L3 是否恰好写了处理器，正是本设计一直在消除的那类隐式依赖。

因此答案取更窄的一侧：**`open` 只开放"新的 grade 产出行为"，不开放新的 outcome。**
`extendable` 与 `open` 的区别因此是：

```text
extendable  新增的程序只能通过 grade(err, windowSet) 或已声明的常量等级产出等级
open        新增的程序可以自定判定规则来产出等级（例如按音符分别声明严格性、
            保护音符、自定义的到达判定），但仍只能产出已声明的等级值
```

**真正需要新 outcome 或新 category 的情形，正确表达是新 Interface 版本或另一个包**，
与 §4.6 对"新等级、新 region、新 Hook"的处置一致。这不是限制能力，而是把"改变判定语义
的形状"留在包级声明里——那里它可见、可审查、可版本化。

### 6.3 包容器：复用 CXC 的载体，不复用它的 entry 分层（回应 §8.3）

**裁决：容器取 CXC v1 的 ZIP32 Stored 子集与 manifest 形状，`format` 取新值。**

```text
复用    ZIP32、Stored (method 0)、portable ASCII 路径、CRC32 + manifest SHA-256
        目录项与 metadata 的冻结值（flags 0、1980-01-01、version needed 10、无 extra field）
        路径规则（无重复、无 file/descendant 冲突）、entry 计数上限
        规范 writer 的确定性要求（相同输入产生相同字节）
新取    manifest format = "cuexis.ruleset"，version = 1
不复用  CXC 的 Chart v5 entry 分层（compiler profile、playback entry、实体与要求计数）
```

**复用载体的理由**是载体问题已经被解决过一次：冻结的 golden bytes、一个验证器、
三个工具（pack / validate / unpack）。造第二个容器意味着第二套验证器、第二套 golden，
以及"某个 ZIP 边界情况要修两处"。容器与内容是正交的两件事，没有理由让内容决定它。

**不复用 entry 分层的理由**是那部分不是容器，是 CXC 为 Chart v5 加的内容约定。Ruleset 包里
没有播放 entry，因此那些字段要么空置、要么含义被误读。直接不引入。

**诊断前缀天然分开**：CXC 的稳定诊断已经是 `cxc.*` 命名空间，本包用 `ruleset.*`。
复用载体不会产生诊断冲突，也不需要给现有诊断改名。

**预算的差异点**：CXC §8 的表里，与 Chart 相关的两项（`expandedEntityCount`、
`expandedRequirementCount`）不适用；本包新增的是模块数与 Hook 数。其余条目（包字节、
entry 字节、manifest 字节、entry 数、路径字节与深度、诊断数）直接沿用同一组数值，
理由相同：它们量的是载体，不是内容。

**与语言版本的关系**：容器版本（外层信封）与 Interface 版本（包内声明）是两条独立的轴。
容器决定"能不能打开"，Interface 决定"能不能运行"。两者都需要，且不互相蕴含。

### 6.4 独占 Hook 的所有权是静态表，不可转交（回应 §8.5）

**裁决：不可转交。所有权由包清单静态指定，运行期只读取。**

```text
hookOwnership {
  <hookId>: <core | moduleId>
  <hookId>: { owner: moduleId, fallback: core }      可选
}
```

**为什么不做转交**：§4.7 的全部校验力量来自"每个可写目标恰好有一个写入者"是**集合运算**
而不是图可达性分析。允许转交之后，判定"两个模块是否可能同时写"需要遍历模块图与启用组合，
校验从一次查表变成一次搜索；而搜索的结论还会依赖 `group` / `conflicts` 的交互，
出现"这个组合安全、那个组合不安全"的结论。

**真正需要转交的情形，已有更好的表达**：

```text
想让它只在某些 Loadout 下归属别的模块     用 fallback。owner 模块未启用时回落到 fallback，
                                         Hook 永远不会无主
想让它被多个模块影响                     用派生 Hook 的合成算子，本来就不需要独占
想让它在一个 Tick 内先后被两个模块改写      用信号，多一个 Tick 延迟，顺序问题消失
```

**fallback 这一条值得强调**：它使"禁用某个模块"不可能让 Hook 处于无主状态。没有它，
`owner` 指向一个被 Loadout 禁用的模块时，Hook 要么无主（判定读什么？）要么隐式回落
（回落规则没写）。两种都不可接受，所以 `fallback` 是必需的而不是可选的糖。

**静态验证因此只多一条**：

```text
每个独占 Hook 的所有者必须存在，且 owner 与 fallback 不得指向同一模块
```

### 6.5 包的构成与版本

```text
interface 声明        §4.1，含 programPolicy 与 outcomeScope
hookOwnership          §6.4 的静态表
fold 程序              §4.3
L2 程序库              §4.9 的 programPolicy 限定其可用范围
只读数据              窗口表、region、controller 声明、定点表引用（按 tableId，见引擎冻结合同草案）
模块与默认参数          §4.4
表现默认绑定             §4.5，属 L4 输入
资源                   与谱面资源同一套闭包规则
```

```text
requiredFeatures     引擎缺少所需能力位即拒绝，沿用 Packed 的 requiredFeatures 模式
Interface 版本       谱面绑定它，决定能否运行
Build 内容 hash      Replay 绑定它，决定结果能否复现
Loadout             启用模块与参数，进入 Replay identity
```

**hash 的覆盖范围**必须写在包规范里而不是留给实现：Build 内容 hash 覆盖
interface 声明、hookOwnership、fold 程序、L2 程序库、只读数据与模块声明；
**不覆盖**表现默认绑定与资源（它们是 `presentation` 类，见身份草案 §2）。

这条分区不是形式要求：若 Build hash 覆盖了资源，换一张贴图就会让全部 Replay 失效，
而那正是身份草案 §3 引入两个摘要要消除的事。

## 7. 静态验证

```text
类型与引用    所有 Enum、region、controller、hook 必须已声明
取值范围      区间分析，可能越界或溢出即拒绝
终止性        语句层只有有界 for，折叠天然终止；核心每 Tick 的执行次数有上界
结算完备      所有可结算的事件类型都有处理器或落入显式忽略
预算          每 Fact 代价、模块数、状态字节数、定时器数
              （L3 侧的完整条目与推导见
               [预算与规模上界草案](GAMEPLAY_BUDGET_AND_SCALE_DRAFT.md)）
禁止项        无宿主调用、无 IO、无随机、无墙钟、无浮点
```

因为 L3 是分发内容，这一套验证和 L2 一样是强制的，不是信任基础上的省略。

## 8. 待决

1. ~~引擎内置的原生模式与分发模式是否允许共用同一个 Interface~~ 已在 §6.1 裁决：**允许，
   且应当成为常态**。两者的区别只是"谁发行"，不是表示形式。内置模式也做成包之后，
   它的 Build 变化落在 ruleset 分量而不是 engine 分量，失效范围从"该引擎上全部 Replay"
   收窄到"使用它的谱面"。
2. ~~是否需要比"有界 for"更强的聚合能力~~ 已裁决：**不需要，且把它登记为一条有触发条件
   的设计边界**，而不是留一个开放项。

   ```text
   裁决      不引入任何强于"有界 for"的循环或聚合
   替代      需要聚合时，先扩展 Table 与声明式折叠表
   触发条件  只有当某类需求无法用 Table + 折叠表 + 有界 for 表达时，才重新讨论
   ```

   **为什么现在就能定**：§3.1 已经确立了 L3 需要的聚合只有两种形状。

   ```text
   按枚举键分类统计    Table<Category, Outcome, Int32> 已经覆盖
   对事实流做有界折叠   §4.3 的 on Fact 处理器按发出顺序推进，天然是有界的
   ```

   两种都不需要"遍历一个可变长度的集合"。真正的风险不是"能力不够"，而是**过早地引入
   通用循环**：一旦有了它，§7 的终止性检查就从"语句层只有有界 for"退化为"需要对循环做
   上界分析"，而后者在存在间接寻址时不可判定。

   **"触发条件"这一条是本次裁决的产出**：它把"暂不支持"变成"什么情况下必须重新讨论"，
   这样将来遇到具体案例时，判断依据是"它是否落在两种形状之外"，而不是凭感觉。
3. ~~Ruleset 包是否直接复用 CXC v1 容器~~ 已在 §6.3 裁决：**复用载体，不复用 entry 分层**。
   ZIP32 Stored 子集、manifest 形状、路径规则、writer 确定性全部沿用；
   `format` 取 `cuexis.ruleset`；不带入 CXC 的 Chart v5 entry 字段。诊断前缀 `ruleset.*`
   与既有的 `cxc.*` 天然分开。
4. ~~`open` 档下谱面自带程序能否产出新的结算结果~~ 已在 §6.2 裁决：**不能**。
   `programPolicy` 拆成来源轴与产出轴，产出轴默认封闭；新增 outcome 等于新增折叠键，
   会让"结算完备"检查退化成"忽略未知 outcome"。`open` 只开放新的 grade 产出行为。
5. ~~独占 Hook 能否转交所有权~~ 已在 §6.4 裁决：**不可转交**。所有权是包清单里的静态表，
   每项带必需的 `fallback`，因此禁用模块不会让 Hook 无主。转交会让校验从集合运算变成
   图搜索，且真实需求已有三种更好的表达（fallback / 派生合成 / 信号）。
6. ~~L3 快照的字节数上限与模块数上限，与 L2 的预算合并还是分开计~~ 已在 §3.2 裁决：
   **分开计**。L3 的核心是定值而 L2 随活动度变化，合并会掩盖两种不同的失败现象。
   新增五项 L3 预算条目，其中 `l3FactCostPerSecond` 说明 L3 的折叠代价实际随谱面规模
   线性增长——核心是一个不等于它每 Tick 处理的 Fact 数是常数。
7. ~~表达式核的逐项对齐~~ 已在 §3.1 完成：一张"共享 / L3 独有 / 不共享"的三态表，
   覆盖类型构造子、算术、读取集合与几何原语。几何原语是唯一"共享实现但默认关闭"的一条
   ——几何函数共享，几何数据源（region / frame / contact）不向 L3 开放，因为后者是权限
   边界而不是能力差异。
