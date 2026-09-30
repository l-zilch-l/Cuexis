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

Hook 的写入在第 4 步，按 t+1 规则在下一个 Tick 对判定生效。

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

Ruleset 是一个可分发的包，沿用仓库既有的容器约定。包内包含：interface 声明、fold 程序、
L2 程序库、窗口表、region 与 controller 声明、模块与默认参数、表现默认绑定、资源。

```text
requiredFeatures   引擎缺少所需能力位即拒绝，沿用 Packed 的 requiredFeatures 模式
Interface 版本     谱面绑定它，决定能否运行
Build 内容 hash    Replay 绑定它，决定结果能否复现
Loadout            启用模块与参数，进入 Replay identity
```

谱面可以引用包内的 L2 程序库，也可以自带程序。自带程序的权限由 interface 的 `programPolicy`
决定，取值见 §4.9。`interfaceVariesWithLoadout`（§4.6）在这里生效。

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

1. 引擎内置的原生模式与分发模式是否允许共用同一个 Interface。
2. 是否需要比"有界 for"更强的聚合能力。如果需要，优先考虑扩展 `Table` 和声明式折叠表，
   而不是引入通用循环。
3. Ruleset 包是否直接复用 CXC v1 容器，还是需要自己的容器版本。
4. `open` 档 programPolicy 下，谱面自带程序能否产出新的结算结果，还是只能是新的 grade
   产出行为。
5. 独占 Hook 能否转交所有权（一个模块把一个 Hook 让给另一个模块），还是只能由包清单
   静态指定。前者需要更复杂的校验规则。
6. L3 快照的字节数上限与模块数上限，与 L2 的预算合并还是分开计。见
   [预算与规模上界草案](GAMEPLAY_BUDGET_AND_SCALE_DRAFT.md) §7.1：该文的候选值全部待实测，
   L3 侧尚未量化。
