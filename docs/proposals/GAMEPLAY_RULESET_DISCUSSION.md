# Gameplay Ruleset 设计讨论记录

状态：candidate（设计讨论记录，未接受，未实施）

更新日期：2026-09-29

本文记录对 Input / Judgement / "脚本"系统的重新设计讨论。它是设计输入，不是 ADR、Spec
或阶段计划；后续设计可能整体取代 [音乐游戏玩法抽象模型](../architecture/GAMEPLAY_ABSTRACTION_MODEL.md)
与 [Stage 7 计划](../stage_plans/future/stage-07/plan.md) 中的相应内容。讨论中已明确：与现有文档
（含"运行时脚本无限期延后"条款）的冲突不作为约束，待设计收敛后再统一修订。

## 1. 重新设计的动机

原模型的 `NoteRequirement = (timeInterval, judgementDomain, requiredAction, constraints, effects)`
与"声明式 Behavior / Effect Schedule"无法覆盖以下现象：

- 依赖会话数值的表现：连击提示、All Perfect 提示。
- 会影响结果的系统：血条、技能、角色。
- 跨越判定与表现的现象：Hold 中途断开后 Note 变灰。
- 复杂判定：maimai 星星以固定判定区拟合路径，且允许"跳区"（跳过一个或多个判定区，
  用于容忍街机硬件老化）。
- 移动判定域：osu!catch 由玩家移动盘子，判定是水果落入盘子范围。

对原模型的问题诊断（首轮评审）：

1. 名义锚点（Chart 语义）与判定窗口（策略）混在 `timeInterval` 中。
2. 单一 `requiredAction` 无法表达多阶段要求（Hold 的按下/保持/释放，Slide 的阶段序列）。
3. 缺少输入归属与仲裁（重叠窗口、note lock、多押、空按）。
4. 动作分为边沿事件（press/release/flick）与状态谓词（drag/hold 中段/arc 跟随），
   Input 需要以接触点为单位（`contactId + begin/move/end + 域坐标`）。
5. 判定域可能随时间变化（移动判定线、arc），这属于判定语义而非表现。
6. `effects` 位于 Requirement 内并进入 semantic hash，修改命中特效会使 Replay 失效。
7. `comboDelta`/`lifeDelta` 是计分策略输出，不是判定事实。
8. 确定性实现形态未定：折叠模型、整数时间边界、Seek 语义。

## 2. 已达成的共识

### 2.1 划界标准

边界按"是否影响结果"划分，而不是按"判定 vs 脚本"划分。结果包括要求是否满足、分数、
Combo、生命值、是否失败。

- 影响结果：属于玩法规则，必须确定性、可回放，并进入 Replay identity。
- 只改变画面与声音：属于表现绑定，只读玩法状态，不能写回。

示例：Hold 断开是判定事实 `HoldBroken(requirementId, t)`；变灰是表现层对该状态的
绝对时间查询。血条数值与归零失败属于规则，血条画法属于表现。技能生效属于规则，
技能特效属于表现。

### 2.2 结构

原"脚本系统"拆分为 Ruleset 程序（决定结果）与表现绑定（只读映射）：

```text
Input Stream（接触点 / 按键 / 轴）
  -> Ruleset（可编程、确定性、有界）
       Controller：由输入派生的状态（接水果盘子、光标）
       Requirement Program：每种要求的判定自动机
       Arbitration：输入归属与冲突
       Score / Combo / Life / Skill 折叠
     => Gameplay Event Log + GameplayState(T)
  -> Presentation Binding（只读、响应式）
  -> Presentation
```

规则：Ruleset 读取的任何内容都不能由 Presentation 产生。

### 2.3 判定程序的来源

- 谱面既可内置判定程序，也可引用外部判定程序，模式与 CXT 内置/外部引用一致。
- 内置、外部、SDK 标准库（Tap/Hold 等）使用同一种 IR 与同一个验证器，没有特权路径。
- "外部"指谱面文件之外，不指包之外：被引用的程序必须进入内容闭包，Replay 绑定内容 hash，
  不绑定名称；不允许运行时查找已安装程序。
- 程序语言版本是 capability，未知版本稳定拒绝，不降级执行。
- 谱面程序视为不可信内容，语言本身必须对恶意输入安全。

### 2.4 程序与 Ruleset 的职责

谱面程序只产出判定事实（阶段 k 在时刻 t 满足、误差 e、等级 G），G 必须取自 Ruleset
定义的等级集合。Score / Combo / Life 只由 Ruleset 折叠计算，谱面程序不能直接写入。

### 2.5 谱面与 Ruleset 的关系

一张谱面只在一种 Ruleset 下运行；兼容的 Ruleset 变更（修复映射、微调判定区间）除外。
为区分"能运行"与"结果相同"，拆为两层：

```text
Ruleset Interface  判定域、动作、等级集合、程序 ABI、谱面参数 Schema
                   -> 谱面绑定 Interface 版本（能否运行）
Ruleset Build      具体实现，含窗口数值与映射修复
                   -> Replay 绑定 Build 内容 hash（结果能否复现）
```

跨 Build 的排行榜可比性是游戏运营策略，不属于引擎语义。

### 2.6 技能与角色

- 技能与角色只由 Ruleset 提供；谱面最多声明禁用。
- 技能可以改变判定窗口、判定结果与计分；角色和技能可以影响血条等表现。
- 结果来源为四项：Ruleset、Program Library、Chart、Loadout（角色/技能选择，prepare 前冻结，
  identity 进入 Replay）。
- Loadout / Ruleset 带来的资源形成独立内容闭包，不混入谱面闭包。
- 技能是数据而非代码：`(触发, 条件, 挂点, 数值, 持续时间)`，作用于 Ruleset 声明的挂点
  （窗口缩放、生命值增减倍率、分数倍率、等级升降、Miss 保护次数等）。新增技能/角色只增加
  数据；仅当需要新挂点或需要谱面提供新标注类型时才升级 Ruleset Interface。
- 待验证风险：挂点体系是否足够决定技能扩展是否需要改 Interface（用户对此尚不确定），
  需用目标游戏的技能设计压测。

### 2.7 因果与确定性

- 判定定义为纯折叠：`state(T) = fold(requirements, inputs[t < T], T)`。Miss 由时间推进触发。
- 技能引入的因果环（连击 -> 技能 -> 窗口 -> 判定 -> 连击）按严格时间因果求值：t 时刻的效果
  只作用于 t 之后；同时刻顺序由固定优先级规则决定，该规则必须写入合同。
- 判定边界比较使用整数时间（输入规范化与锚点转窗口时各量化一次）。
- Seek 为从快照重新折叠；跳过的要求如何记账（skipped / miss / 练习模式）进入配置。

### 2.8 判定程序语言不采用图灵完备形式

采用有界时间自动机 / 事件模式 IR。理由不只是安全（沙箱加 fuel 即可解决安全），而是图灵
完备程序的行为性质无法静态判定，会失去以下性质：

1. 错误在 prepare 时暴露，而非演奏中途耗尽预算。
2. 每个要求必然在有限时间后结算（状态图可检查）。
3. 状态有界且可序列化，支持快照、Seek 与 Replay。
4. 输出合法性（等级集合、声明的判定域）是语法约束而非运行时检查。
5. Studio 可以可视化、静态模拟程序。
6. 语义可完整写入 Spec 并跨平台冻结，Replay 不绑定解释器实现。

信任分层：谱面判定程序使用有界自动机 IR；Ruleset 逻辑由可信开发者编写，是否允许更强的
语言暂不决定，待原语集合压测后再议。

### 2.9 分层与共享边界（2026-09-29 修订）

上一版曾建议"折叠与判定共用同一套 IR"，该建议已撤回。理由如下。

判定程序 IR 是冻结的内容 ABI，谱面与 Replay 长期绑定它，改动即破坏兼容。若 Ruleset 逻辑
也跑在这套 IR 上，则每次新增游戏模式都会推动这份冻结合同变化，IR 会变成通用扩展面。
此外还有三点差异：信任级别不同（谱面不可信，Ruleset 作者可信）、代价模型不同（判定按
输入事件、Ruleset 折叠按事实且在热路径）、演进速度不同（玩法逻辑迭代快得多）。

改为四层，各自独立的实现方式与版本号：

| 层 | 谁实现 | 载体 | 冻结程度 |
| --- | --- | --- | --- |
| L1 Input 归一化 | 引擎 + 宿主适配器 | 数据类型 | 稳定 |
| L2 Requirement Program | 谱师（不可信） | 有界自动机 IR | 最严格，最早冻结 |
| L3 Ruleset Core | 模式作者（可分发内容） | 折叠语言（见 §2.10） | 接口稳定，实现自由 |
| L4 Presentation Binding | 谱师 / 美术 | 声明式数据 | 宽松 |

层间只通过声明过的最小表面通信：`L1 -> L2` 为输入流与 Controller 采样；`L2 -> L3` 为
Fact 流与 observable 状态；`L3 -> L4` 为只读的 `GameplayState(T)`。

仍然必须全局唯一的只有：Fact 词汇表与阶段语义、Tick 时间域、等级枚举、InputEvent 模型、
`GameplayState(T)` 查询接口。共享类型不等于共享引擎。

### 2.10 L3 定为可分发内容

L3 的形态由"游戏模式是否为可分发的数据"这一问题决定。2026-09-29 决定：模式是可分发内容，
谱师与游戏作者都能提供完整的 Ruleset 包。因此 L3 需要自己的语言，不复用 L2 的 IR。

L2 与 L3 的计算形状确实不同：L2 是"每实例一个自动机、实例间并发"，L3 是"对单一全序事实流
的全局折叠"。但两者共用表达式核、类型系统、Fact 词汇表和验证器框架，只在下沉到语句层时
分开。这样避免同时维护两套解释器与两套类型检查。

L2 的静态验证只针对 L3 声明的 Interface（含每个 Hook 的静态取值范围），不针对 L3 的实现，
这是两层解耦成立的关键。

## 3. 当前进度

### 3.1 已确定

第一轮（边界与职责）：

1. 划界标准是按"是否影响结果"，不是按"判定 vs 脚本"。
2. 结构为 Ruleset（决定结果）+ 只读表现绑定。
3. 判定程序可由谱面内置，也可外部引用，两者同一种 IR，同一验证器；外部程序进入内容闭包，
   Replay 绑定内容 hash。
4. 谱面程序只产出判定事实，计分由 Ruleset 折叠。
5. 谱面与 Ruleset 的关系是一对一，例外是兼容的 Ruleset 变更；Ruleset 拆成 Interface 与 Build 两层。
6. 技能与角色由 Ruleset 提供，是作用在挂点上的数据。
7. 因果按 t+1 规则，边界用整数时间，Seek 为重新折叠。
8. 判定程序语言不用图灵完备形式。

第二轮（Program IR）：

9. 程序载体是有界时间自动机：有限状态、有界寄存器、纯表达式守卫、动作为赋值/认领/发事实/结算。
10. 时间用整数微秒 Tick；同一 Tick 内固定顺序为激活 → 定时器 → 输入 → 折叠。
11. 派发方式由 Requirement 声明（`consume` / `observe`），不按判定域声明；炸弹因此不会抢输入。
12. 折叠是声明式折叠表，(category, outcome, grade) 组成键；结算结果是 Ruleset 声明的枚举，
    引擎不区分好坏。
13. Controller 分 `onEvent` 与 `sampleAt` 两个函数：事件侧可编程，时间侧封闭。`sampleAt` 的
    每个字段从三个外推基（`hold` / `linear` / `quadratic`）里选一个，再可选地套一个后处理
    算子 `clamp`。外推基集合可扩展且扩展是纯增量，并声明为 capability。
14. 等级只能通过 `grade(err, 窗口集)` 或 Ruleset 常量取得，程序不能自己构造。
15. 程序之间零耦合：唯一能看别人状态的地方是 Ruleset 的仲裁过滤。

第三轮（分层）：

16. 撤回"折叠与判定共用一套 IR"。改为四层，见 §2.9。
17. L3 是可分发内容，拥有自己的折叠语言，见 §2.10。
18. L2 只按 L3 的 Interface 声明做静态验证。Ruleset 包同时携带 Interface、Build 与程序库。
19. L3 的结构与值分离：Interface 静态（模块只能写值），导出字段增量（模块可新增），
    fold 处理器可新增。逃生通道为 `interfaceVariesWithLoadout`，默认关闭。
20. 谱面自带 L2 程序的权限由 `programPolicy` 决定，默认 `extendable`。
21. 模块合成靠消除共享写入实现，不靠定序：派生 Hook 用可交换可结合的算子对贡献集合一次
    合成，独占 Hook 与折叠表 key 都要求唯一写入者并在 Loadout 校验期拒绝冲突，处理器顺序
    冻结在包内，信号只在下个 Tick 投递。
22. 保护音符是 Interface 常量等级，由 L2 程序直接发出，不需要新机制。
23. 几何轨道用"形状静态 + 摆放采样"表达：frame 是一段随时间采样的 Transform 轨道，形状在
    frame 局部坐标中保持静态；角度用 1/65536 圈整数加引擎内置定点三角函数表；frame 运行期
    不可变，判定与表现都可读，表现不能写。
24. 自定义按键映射属于 L1，由 Player 或宿主提供，谱面与程序不需要感知。做法是记录**映射后**
    的规范化事件，同时把映射 identity 写入 Replay 头部；播放时校验 identity，不匹配稳定拒绝，
    不静默改用其他映射。理由与 TimingMap 对"不同来源的延迟不得合并"的规定一致。
25. 数值模型取"数据浮点，决策投影到整数"：作者侧用 f32/f64 表达位置、路径与 frame，
    prepare 时按声明的单位量化一次（向偶舍入、单调、唯一），决策侧只看整数，表现侧回到浮点。
    边界判据是"写不出来就加一个闭式基或声明式能力"，不是放开浮点。
26. 输入模型为两类事件加一类状态：接触点事件（begin/move/end，带 contactId）、通道事件
    （press/release）、以及作为状态的轴与指针位置。轴不进事件流，`axisStep` 是 L1 越阈合成
    的边沿。映射、死区、阈值与接触点线性插值都在 L1，程序侧不可见。

### 3.2 草案状态

[Gameplay Program IR 原语草案](GAMEPLAY_PROGRAM_IR_DRAFT.md) 已完成第二版，含执行模型、
原语、空间状态与输入、表现绑定、仲裁、静态验证，以及八个案例压测：Tap、中途断开的 Hold、
带跳区的 maimai 星星、osu!catch 水果、重叠判定区、变宽 Slider、炸弹、移动判定线。
八个案例全部可表达。

[Ruleset Fold Language 草案](GAMEPLAY_RULESET_FOLD_DRAFT.md) 已完成第一版。

### 3.3 下一步工作

按依赖排序，前几项是设计工作，最后才进入实施。

```text
A. Ruleset 折叠与技能的 IR 表达 —— 已完成初稿
   [Ruleset Fold Language 草案](GAMEPLAY_RULESET_FOLD_DRAFT.md)：与 L2 共用表达式核与类型
   系统，语句层分开；Interface 与 fold 程序同包分发。

B. 几何轨道 —— 已完成
   见 [Gameplay Program IR 原语草案](GAMEPLAY_PROGRAM_IR_DRAFT.md) §5.3：区域绑定 frame，
   形状在局部坐标中静态。不改变程序 IR。

C. Controller 自定义程度 —— 已完成
   见同文 §5.2：事件侧可编程，时间侧封闭；三个外推基加一个后处理算子；`sampled` 逃生通道
   取消。

D. Input 形状 —— 已完成
   见同文 §5.1：两类事件加一类状态；映射、死区、阈值、线性插值都在 L1。

E. 表现绑定 —— 已完成
   见同文 §6：三类读入口、三条驱动路径、按键音分两种、按绑定键的覆盖与 `overridable`。

F. identity 分层
   gameplay identity、chart semantic identity、Ruleset Build、Loadout、presentation。

G. 继续压测
   Arcaea 的 Arc 与 Sky Note、Taiko 连打、osu! Slider、DJMAX 长按连打、多指 Slide。

H. 技能挂点体系的覆盖范围
   决定新技能/角色是否需要升级 Ruleset Interface。

I. 收敛
   ADR（含威胁模型）、Spec、预算与 ABI、修订 Stage 7 范围。
```

### 3.4 尚未登记的设计输入

1. 目标游戏清单：用于压测全部抽象，尚未提供。
2. 作者语法：JSON IR、文本 DSL、还是 Studio 可视化编辑，尚未选择。
3. 定点表清单与版本管理：角度表已定，`exp` / `sinusoid` 外推基若启用会带来新的表。
