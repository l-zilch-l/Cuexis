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
    （2026-10-01 修订：拆成来源轴 `programPolicy` 与产出轴 `outcomeScope`，
    产出轴默认封闭。见第 49 条。）
21. 模块合成靠消除共享写入实现，不靠定序：派生 Hook 用可交换可结合的算子对贡献集合一次
    合成，独占 Hook 与折叠表 key 都要求唯一写入者并在 Loadout 校验期拒绝冲突，处理器顺序
    冻结在包内，信号只在下个 Tick 投递。
22. 保护音符是 Interface 常量等级，由 L2 程序直接发出，不需要新机制。
23. 几何轨道用"形状静态 + 摆放采样"表达：frame 是一段随时间采样的 Transform 轨道，形状在
    frame 局部坐标中保持静态；角度用 1/65536 圈整数加引擎内置定点三角函数表；frame 运行期
    不可变，判定与表现都可读，表现不能写。
24. 自定义按键映射属于 L1，由 Player 或宿主提供，谱面与程序不需要感知。Replay 记录**映射后**
    的规范化事件，映射 identity 与校准参数只作为**信息性元数据**写入 Replay 头部，**不参与
    校验门**。（2026-09-29 修正：原文同时要求记录映射后事件与校验映射 identity，两者不能
    同时成立。理由与 TimingMap 对"不同来源的延迟不得合并"的规定一致。）
25. 数值模型取"数据浮点，决策投影到整数"：作者侧用 f32/f64 表达位置、路径与 frame，
    prepare 时按声明的单位量化一次（向偶舍入、单调、唯一），决策侧只看整数，表现侧回到浮点。
    边界判据是"写不出来就加一个闭式基或声明式能力"，不是放开浮点。
26. 输入模型为两类事件加一类状态：接触点事件（begin/move/end，带 contactId）、通道事件
    （press/release）、以及作为状态的轴与指针位置。轴不进事件流，`axisStep` 是 L1 越阈合成
    的边沿。映射、死区、阈值与接触点线性插值都在 L1，程序侧不可见。
27. 每个语义字段必须声明 `judgment` / `presentation` / `neutral` 类别，分区逐字段判定并做机械
    检查。在此之上定义两个摘要：`ContentIdentity` 覆盖全部字段，`JudgementIdentity` 只覆盖
    judgment 类字段并分为 engine / ruleset / chart / session 四个命名分量。Replay 绑定后者。
    既有 `PreparedSemanticIdentity` 继续服务 prepare 事务与 CXC 身份，不复用为 Replay 绑定。
28. 失配默认严格拒绝，并提供显式的诊断模式：可用当前内容重新折叠，结果标记为非权威，
    不计分、不写排行榜。
29. 认领策略拆成两层：`grip`（`sticky` / `handoff`）声明中途能否换手，属于判定语义；
    归属与可见性分离，`observe` 边始终可见。所有权由引擎维护，是边的隐式前提而不是运行期
    错误；引擎不在接触点结束时自动归还归属，句柄永不回收。
30. 连续谓词在两次输入事件之间的变化需要状态级 `every(period)` 周期采样。周期由 Ruleset
    声明基线与允许区间、内容在区间内覆盖，不写死梯级。语义定义为"采样得到"，解析求交将来
    只能是另一种带 capability 的模式。
31. 守卫表达式补一组整数几何原语：`vec` / `posAt` / `sub` / `dot` / `cross` / `len2` /
    `sqrtApprox`。`cross` 的符号判转向，`dot` 的正负判前进或回摆。这是 Flick 与方向动作的前置。
32. `claim` 的动作签名带槽位：`claim(contact, slot)` 与 `release(slot)`，用于多指判定。
33. 输入重采样采用方案 A：离散边沿保留原始时间戳绝不重采样，连续量按 Interface 声明的规范
    格点重采样，低于声明最低上报率的设备在上报率上稳定拒绝。重采样属于归一化，Replay 记录
    重采样之后的流。可保证"同一设备类别下不同上报率结果一致"，**不能**保证跨设备类别等价，
    也不能消除重建轨迹的插值误差。
34. frame 的来源有两种：`track`（谱面数据）与 `bound(ctrl)`（会话状态驱动），接口同为
    `frameAt(t) -> Transform`。此前把后者整个排除是过严的——它把"被写入"与"依赖会话状态"
    混为一谈，而 `frame(T)` 本来就依赖时钟。设备绑定型玩法（Rotaeno、Tone Sphere 的旋转
    判定线）由此可表达，需要 Interface 声明所需硬件与最低输入上报率，L1 提供归一化朝向角
    而非原始角速度。仍不覆盖 3D 空间玩法与连续音高输入。

第四轮（预算与规模）：

35. 容量、稳态、Seek 三类成本不合并，各自独立预算与诊断。沿用 `PackedChartLimits`
    的逐项独立做法。
36. 活动度（瞬时存活的 requirement 数）是容量与稳态之间的桥梁，也是每事件代价的乘数。
    40,000 是整曲计数，活动度是瞬时计数，两者相差约两个数量级——这是设计成立的关键前提。
    技能带来的窗口放宽**先影响活动度，再影响判定**，因此 `windowScale` 上限要过活动度预算。
37. 补的规模进预算，不进 capability。确定化是机械算法，没有引擎不支持的情况，它是复杂度
    问题而非能力问题；做成 capability 会让同一引擎对不同谱面宣称支持不同的语言特性。
38. 快照必须无损（`snapshot(T)` 与连续推进逐位相同），间隔是引擎自由参数、不进 identity。
    周期采样相位必须进快照，否则 Seek 会复现缺陷 D1。
39. Pattern 与 Measure 共享表达式核（读取集合、类型系统、几何原语、绑定变量、窗口表），
    但不共享算子集：Pattern 算子生成语言，Measure 算子归约区间。方向相反、代价模型不同、
    编译目标不同，三条理由任何一条都足以否定合并。
40. 标准库分两层：引擎核心库（随引擎发行、ID 永久稳定、版本进 capability）与包扩展库
    （随 Ruleset 包发行、由 Interface 声明签名）。两个命名空间独立，包库不得遮蔽引擎库，
    否则 engine 前缀"ID 永久稳定"失效。
41. L3 不使用 Pattern / Measure，但两者都是 Fold 的实例。L2 与 L3 真正共享的是 Fold，
    不是 Pattern；Pattern 只是 L2 面向作者的源语言。
42. 定点表的身份是语义内容而非可变版本：表 id 即语义，改进是新增条目而不是提升版本，
    各条目永久并存。引擎的表集合只增不减，代价是每张表永久维护与永久测试。
43. 缓动曲线先定准入判据（闭式整数求值、误差可静态界定、不调用宿主数学库），
    再给九条起始集合。`elastic` 等需要三角表的曲线在表到位之前不可用。
44. 判定语义版本的清单逐项列出，门禁采用三来源原则：执行器取自 base commit
    （防止候选分支自我放行）、声明取自 candidate commit（必须出现在 PR diff 里）、
    放行取自 workflow 事件。放行按清单项而非全局布尔。
45. L1 朝向归一化的接口只保证输出单位与值域、单调性、单圈值域三件事，融合算法留作
    L1 实现且不进 identity——与输入映射同级，都"烘进事件流"。
46. Interface 投影的单位是 (hookId, 贡献来源)，且只取"生效贡献集合"与谱面实际读取集合的
    交集。它比逐 Hook 更细因而更安全，成本却由 Loadout 决定、不随谱面规模增长。
47. Fact 携带 `requirementId`。身份本来就在系统里（仲裁按它排序），只是此前没有随 Fact
    暴露；补上之后"按音符的例外"可由 fold 处理器表达。这不破坏 W1 的边界，因为方向单向。
48. 引擎内置的原生模式也做成包，与分发模式共用同一套 Interface、加载路径与验证入口。
    关键收益是失效范围：内置模式的 Build 变化落在 ruleset 分量而不是全局的 engine 分量，
    否则"给内置模式微调一个窗口"会让该引擎上全部已发行的 Replay 失效。
49. `programPolicy` 拆成两个轴：来源轴（`locked` / `extendable` / `open`）与产出轴
    （`outcomeScope = declared` / `extended`，默认封闭）。`open` 只开放新的 grade 产出行为，
    **不开放新的 outcome**——新增 outcome 等于新增折叠键，会让"结算完备"检查退化成
    "忽略未知 outcome"，即判定结果依赖 L3 是否恰好写了处理器。真正需要新 outcome 的
    正确表达是新 Interface 版本或另一个包。
50. Ruleset 包复用 CXC v1 的**载体**（ZIP32 Stored 子集、manifest 形状、路径规则、
    writer 确定性），但**不复用**它的 Chart v5 entry 分层。`format` 取 `cuexis.ruleset`，
    诊断前缀 `ruleset.*`。容器与内容是正交的两件事。
51. 独占 Hook 的所有权是包清单里的静态表，**不可转交**；每项带必需的 `fallback`，
    因此禁用模块不会让 Hook 无主。转交会让校验从集合运算变成图搜索，而真实需求已有
    三种更好的表达：fallback、派生 Hook 的合成算子、信号。
52. Ruleset 包的 Build 内容 hash 覆盖 interface、hookOwnership、fold 程序、L2 程序库、
    只读数据与模块声明，**不覆盖**表现默认绑定与资源。否则换一张贴图就会让全部 Replay
    失效，正是身份草案引入两个摘要要消除的事。

### 3.2 草案状态

[Bounded Fold Calculus 提案](GAMEPLAY_FOLD_CALCULUS_DRAFT.md) 是**替换性重设计**，不是增量
修订。它把核心概念从约 15 个归约到 3 个（Fold / Pattern / Measure），并把 10 条缺陷中的 6 条
消除（其中 3 条是构造性消除）。归约的动机来自压测登记的观察：原语数量在收敛，规则数量没有。

[Gameplay Program IR 原语草案](GAMEPLAY_PROGRAM_IR_DRAFT.md) **部分被取代**：它的 §2、§3、§4
（执行模型、程序结构、原语）由 calculus 替换，而 §3.1、§4.3、§4.5–4.8、§5–§9 **仍然有效**，
且是 calculus 的依赖项（定义原子与算子的定义域、表现层边界、验证规则）。逐节处置表见该文
开头的「取代关系」一节。

[Ruleset Fold Language 草案](GAMEPLAY_RULESET_FOLD_DRAFT.md) 已完成第一版。
[Gameplay 预算与规模上界草案](GAMEPLAY_BUDGET_AND_SCALE_DRAFT.md) 已完成第一版，
代价条目不再分散在四处。Fold Calculus 提案的 §5 已扩为"核心之外的声明与裁决"，
把表达式核、标准库与 L3 的关系三项裁决一并写入（§5.7–§5.9）。
[Gameplay Identity 分层草案](GAMEPLAY_IDENTITY_DRAFT.md)、
[Skill Hooks 与模块扩展性草案](GAMEPLAY_SKILL_HOOKS_DRAFT.md) 与
[Gameplay 设计压测与缺陷登记](GAMEPLAY_STRESS_TEST_DRAFT.md) 仍是各自主题的载体；
前两份使用的术语（Hook、窗口集）在 calculus 下名称不同但概念对应，需要一次术语复核。

### 3.2b 阅读顺序

按核心概念 → 依赖细节 → 主题文档的顺序：

```text
1  Bounded Fold Calculus 提案      核心概念：Fold / Pattern / Measure
2  Fold Calculus 封闭性验证        归约与封闭性的证据
3  Gameplay Program IR 原语草案    原子与算子的定义域、表现边界、验证规则、案例细节
4  Gameplay 设计压测与缺陷登记     缺陷与局限的完整登记
5  Gameplay Identity 分层草案      身份与分区
6  Skill Hooks 与模块扩展性草案    技能扩展与核心外声明
7  Ruleset Fold Language 草案      L3 规则集语言与打包
8  预算与规模上界草案              容量 / 稳态 / Seek 的公式与待实测的候选值
```

第 3 份的 §2、§3、§4 只作为设计过程记录阅读，见该文的「取代关系」。

### 3.3 下一步工作

按依赖排序，前几项是设计工作，最后才进入实施。

```text
A. Ruleset 折叠与技能的 IR 表达 —— 已完成初稿
   [Ruleset Fold Language 草案](GAMEPLAY_RULESET_FOLD_DRAFT.md)：与 L2 共用表达式核与类型
   系统，语句层分开；Interface 与 fold 程序同包分发。

B. 几何轨道 —— 已完成
   见 [Gameplay Program IR 原语草案](GAMEPLAY_PROGRAM_IR_DRAFT.md) §5.4：区域绑定 frame，
   形状在局部坐标中静态。不改变程序 IR。

C. Controller 自定义程度 —— 已完成
   见同文 §5.3：事件侧可编程，时间侧封闭；三个外推基加一个后处理算子；`sampled` 逃生通道
   取消。

D. Input 形状 —— 已完成
   见同文 §5.1 与 §5.2：两类事件加一类状态；映射、死区、阈值、线性插值都在 L1；
   离散边沿保留原时间戳，连续量按声明格点重采样。

E. 表现绑定 —— 已完成
   见同文 §6：三类读入口、三条驱动路径、按键音分两种、按绑定键的覆盖与 `overridable`。

F. identity 分层 —— 已完成初稿
   见 [Gameplay Identity 分层草案](GAMEPLAY_IDENTITY_DRAFT.md)：字段分区为
   judgment / presentation / neutral，两个摘要（ContentIdentity 覆盖全部，
   JudgementIdentity 只覆盖 judgment），四个命名分量，显式排除清单与失配策略。
   同时修正了第 24 条：映射 identity 与校准降为信息性元数据，不做校验门。

G. 继续压测 —— 已完成
   见 [Gameplay 设计压测与缺陷登记](GAMEPLAY_STRESS_TEST_DRAFT.md)：案例扩到 34 项，
   新增 22 项中没有一项需要新的判定原语；同时登记 10 条设计缺陷（D1–D10）与 7 条局限
   （L1–L7），并给出复杂度收敛判断。

H. 技能挂点体系的覆盖范围 —— 已完成初稿
   见 [Skill Hooks 与模块扩展性草案](GAMEPLAY_SKILL_HOOKS_DRAFT.md)：结论是新增技能或角色
   不需要升级 Interface。关键区分是程序可见 Hook 与模块本地状态，加上 L3 模块代码这条
   扩展路径。三边界全部禁止（变速、谱面变换、运行期生成 Requirement、非确定效果）。

I. 收敛
   ADR（含威胁模型）、Spec、预算与 ABI、修订 Stage 7 范围。

M. L3 打包与权限 —— 已完成
   见 [Ruleset Fold Language 草案](GAMEPLAY_RULESET_FOLD_DRAFT.md) §6.1–§6.5：内置模式也
   做成包、`programPolicy` 拆两轴、复用 CXC 载体不复用其 entry 分层、独占 Hook 所有权
   静态且带 fallback、Build hash 的覆盖范围。该草案的待决项 1 / 3 / 4 / 5 同时关闭。

L. 投影粒度与 Fact 身份 —— 已完成
   见 [Gameplay Identity 分层草案](GAMEPLAY_IDENTITY_DRAFT.md) §4.2 与
   [Fold Calculus 提案](GAMEPLAY_FOLD_CALCULUS_DRAFT.md) §5.4：投影单位确定为
   (hookId, 贡献来源) 与读取集合的交集；Fact 补入 `requirementId`，压测 L5 与 L7 同时关闭。

K. 引擎冻结合同 —— 已完成初稿
   见 [引擎冻结合同草案](GAMEPLAY_ENGINE_FROZEN_CONTRACTS_DRAFT.md)：定点表登记点与
   "id 即语义"裁决、缓动曲线的准入判据与九条起始集合、判定语义版本的逐项清单与门禁
   三来源原则；另含 L1 朝向归一化接口（Program IR §5.2b）。

J. 预算与规模上界 —— 已完成初稿
   见 [预算与规模上界草案](GAMEPLAY_BUDGET_AND_SCALE_DRAFT.md)：容量 / 稳态 / Seek 三类成本
   不合并；活动度是容量与稳态之间的桥梁；补进预算不进 capability；快照间隔是引擎自由参数
   而非内容声明；并给出进 / 不进 identity 的完整清单。**候选值全部待实测冻结**，其中三项
   回应了 Fold §10.4、压测 L3 与压测 L6。
```

### 3.4 尚未登记的设计输入

1. 目标游戏清单：用于压测全部抽象，尚未提供。
2. 作者语法：JSON IR、文本 DSL、还是 Studio 可视化编辑，尚未选择。
3. 定点表清单与版本管理：角度表已定，`exp` / `sinusoid` 外推基若启用会带来新的表。
