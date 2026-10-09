# Gameplay 预算与规模上界草案

> 目录状态：Gameplay research input。测量值均为候选证据，不构成生产 ABI 或冻结限额；预算原则已由 ADR 0043 / Spec 承接。

状态：candidate（设计草案，未接受，未实施）

更新日期：2026-10-01

本文把散在四份草案里的代价条目收进一处，并回应三个已登记的待决项：

```text
Fold §10.4   补的规模上界与 capability 划分
压测 L6      快照间隔策略
压测 L3      采样重型谱面的容量上限
```

代价条目此前分散在 [Program IR 原语草案](GAMEPLAY_PROGRAM_IR_DRAFT.md) §9、
[Bounded Fold Calculus 提案](GAMEPLAY_FOLD_CALCULUS_DRAFT.md) §7.1/§7.2、
[封闭性验证](GAMEPLAY_CALCULUS_CLOSURE_CHECK.md) V6、
[Ruleset Fold Language 草案](GAMEPLAY_RULESET_FOLD_DRAFT.md) §7 与压测 §4 里，
没有一处是完整的。实现者无法从任何单篇文档得知"这份谱面会不会被拒"。

## 1. 一条前置原则：三种成本不合并

既有容器已经有一条正确做法：`PackedChartLimits` 逐项独立，单位是各自的语义单位，
不折算成"分"（[PACKED_CHART_FORMAT.md](../formats/PACKED_CHART_FORMAT.md) §3.3）。
本设计沿用，并扩展到三类成本。

| 维度 | 何时发生 | 单位 | 失败的表现 |
| --- | --- | --- | --- |
| 容量 | prepare，一次性 | bytes / count | 谱面被拒，给出超额报告 |
| 稳态 | 演奏中，每 Tick | 基本运算次数 | 掉帧、判定抖动 |
| Seek | 拖动进度条、重放跳转 | 事件数 × 每事件代价 | 拖动不跟手 |

**不合并的理由**：三者的失败现象不同，修法也不同。一份谱面可能容量合格而稳态不合格
（少量 requirement，但全在 `every` 高采样态），也可能稳态合格而 Seek 不合格（状态巨大）。
折算成单一分数会让谱师不知道改哪里。

诊断因此必须**分维度报告**，并指向具体的 requirement 或子表达式。

## 2. 容量：prepare 一次性

### 2.1 沿用既有门禁

```text
Packed 文件        <= 16 MiB           既有，不变
展开实体数         <= 40,000           既有，不变
requirement 数     <= 40,000           既有，不变
preparePeakBytes   峰值内存            既有口径，见 PACKED_CHART_FORMAT.md §3.2
```

本文不改变这些数字。下面新增的条目是在它们之上的**额外**约束。

### 2.2 requirement 编码从定长元组变为 AST（封闭性验证 V6）

旧编码是定长元组，新编码是 `(标准库条目 ID | 内联 Pattern AST) + 参数 + 计量规格`。

```text
标准库条目   一个 ID 加几个参数，比旧元组更小      多数谱面走这条，无风险
内联 AST     体积明显更大                         单列预算与 capability
```

新增两项计数：

```text
maxInlinedPatterns       内联 Pattern 的个数，独立于 requirement 总数
maxInlinedPatternBytes   内联 Pattern 的编码字节总和
```

**这两项需要实测冻结**，不能拍数字。冻结前的行为是：Reader 按 section 的
`decodedSectionBytes` 与文件 16 MiB 门禁做预检，不额外放行内联 AST。

### 2.3 编译产物规模（新增）

编译后的自动机不进文件，但在 prepare 时产生，需要独立检查：

```text
状态数总和        所有 requirement 编译后自动机的状态数之和
边数总和          同上
确定化放大        仅含补的子表达式在确定化后的状态数
```

`确定化放大` 是唯一可能非线性的一项，处置见 §3.3。

## 3. 稳态：每 Tick 的代价

### 3.1 活动度

`activity(t) = |{r : arm(r) <= t < deadline(r)}|`，即瞬时存活的 requirement 数。

它是每事件代价的乘数，**因此是容量与稳态之间的桥梁**。用扫描线在 prepare 时算出峰值：

```text
排序所有 (arm, deadline) 端点     O(R log R)，R <= 40,000，可接受
峰值 activityPeak
```

**40,000 是整曲计数，活动度是瞬时计数，两者相差约两个数量级，这是设计成立的关键。**
以 5 分钟曲目、40,000 条 requirement 计，平均每秒 133 条；若每条窗口 200 ms，
平均活动度约 27，而非 40,000。

**但有三类会破坏这个平均**：

```text
1  长时 requirement      maimai 星星、长 Arc 的窗口可达数秒
2  全曲覆盖的 requirement  背景判定区、始终存活的教学提示
3  技能带来的窗口放宽      窗口缩放上限越高，活动度峰值越高
```

第 3 类是本设计与技能体系的交叉点：**窗口缩放上限先影响活动度，再影响判定**。
因此 `windowScale` 的允许上界不能只按"判定公平性"定，还要过一遍活动度预算。
这条此前没有写在任何地方。

### 3.1a 连续宽限对活动度的影响

连续音符在 prepare 时得到最终 `grace(r)`。它不是新的实例类型，也不增加活动度乘数，但会
延长 `handoff` requirement 的静态寿命，因此必须进入扫描线：

```text
Hold     deadline(r) = end(r) + grace(r)
Slider   deadline(r) = t1(r) + max(W.max.late, grace(r))
```

显式谱面值使用解析后的 `grace(r)`；未覆盖音符使用当前 Interface / Loadout 的默认值。扫描线
和 Gap timer 数量都只读取这个 prepared 值，不读取运行中的 Hook。这样 `grace = 0`、最小值和
最大值都落在同一套活动度与 deadline 公式内，不需要另增预算项。

### 3.2 每事件代价

```text
cost(event at t) <= activity(t) * candidateEdges * (guardNodes + frameEvalCost)
```

各项：

```text
activity        §3.1 的峰值
candidateEdges  该实例当前状态下可迁移的边数
guardNodes      守卫表达式节点数
frameEvalCost   涉及几何时的二分求值，O(log N)，N 是分段数且有静态上界
```

要求：

```text
activityPeak * maxCandidateEdges * max(guardNodes + frameEvalCost) <= eventBudget
```

### 3.3 补的规模与 capability 划分（回应 Fold §10.4）

**裁决：补不进 capability，进预算。**

理由是要把两件一直被混在一起的事分开：

```text
capability  回答"引擎能不能做"。内容引用了引擎没有的能力 -> 拒绝，诊断指向引擎版本
预算        回答"这份内容能不能装下"。确定化是机械的，引擎一定能做，只是可能装不下
```

确定化（子集构造）是机械算法，没有引擎不支持的情况。因此补的规模是**复杂度**问题，
不是**能力**问题。把复杂度做成 capability 会产生荒谬后果：同一引擎对不同谱面宣称支持不同的
语言特性，版本号失去意义。

补的规模上界：

```text
编译顺序   Pattern -> NFA -> (仅当含补时) DFA -> 最小化
规模门     确定化后的状态数 <= patternStateBudget，超限拒绝
诊断       指向具体子表达式，而不是整条 requirement
```

"指向具体子表达式"是必需的：补通常只出现在 Pattern 的一小段里，报整条 requirement 会让谱师
无法定位。这也与 §1 的"诊断必须分维度"一致。

**真正的 capability 条目**（与 §2.2 的预算计数区分开）：

```text
定点表版本        角度表已定；exp / sinusoid 若启用则新增表
sampleAt 外推基   hold / linear / quadratic，扩展是纯增量
形状族版本        Program IR §5.4 的五类形状；将来新增 Prism / Cone 等是新版本
原语族版本        几何原语的集合
程序语言版本      未知版本稳定拒绝，不降级执行
设备能力          requiresOrientation / minInputRate
```

两类都是 prepare 时拒绝，但**诊断必须分开**：前者是"引擎太旧"，后者是"内容太大"。

### 3.4 周期采样的稳态负载（回应压测 L3）

```text
sampleCost = 处于 every 状态的实例数 * 表达式节点数 / 周期
```

压测 L3 已算出：40,000 条 requirement 全部以 1 ms 采样在 5 分钟曲目上约 1.2e10 次求值，
远超预算。以 4 ms 估算，同时处于采样状态的实例数是**以百计而非以万计**。

这条必须写进作者文档，否则谱师会在容量测试里撞墙而不知道为什么。建议的表述：

```text
every 的周期下限由 Ruleset 声明；周期越短，可以同时采样的实例数越少。
两者是乘法关系，不是独立预算。
```

### 3.5 总量预算表（候选，待实测冻结）

下表数值**全部是候选**，与 PACKED_CHART_FORMAT.md §3.3 的冻结值不同。冻结需要实测报告，
流程沿用 CFF-D 的做法。

| 预算 | 单位 | 候选值 | 检查入口 |
| --- | --- | --- | --- |
| `activityPeak` | count | 待实测 | prepare 扫描线 |
| `eventBudget` | 每 Tick 基本运算 | 待实测 | 稳态容量测试 |
| `patternStateBudget` | count（单条 Pattern） | 待实测 | 编译期 |
| `totalAutomatonStates` | count（全谱面） | 待实测 | prepare |
| `maxInlinedPatterns` | count | 待实测 | Reader 预检 |
| `maxInlinedPatternBytes` | bytes | 待实测 | Reader 预检 |
| `sampleCostPerSecond` | 次/秒 | 待实测 | prepare |

**公式可以先定，数值后填。** §3.1–§3.4 的公式是有用的，因为它告诉实现者哪些量必须被追踪；
数值部分需要真实谱面才能定。这份文档的当前价值在公式，不在数字。

**第一批实测（2026-10-01，合成数据）**：见
[Fold Spike 报告](../stage_reports/reviews/gameplay-ruleset-2026-10/2026-10-01-fold-spike.md)。
据此给出**建议**候选值（约为合成负载实测最坏值的 10 倍），仍不是冻结值：

| 预算 | 实测最坏 | 建议候选值 |
| --- | --- | --- |
| `activityPeak` | 91（327 音符/s 的 40k 谱面，静态扫描） | 1,024 |
| `patternStateBudget`（确定化后） | 122（Tap，窗口 120 采样） | 4,096 |
| `sampleCostPerSecond` | 3,324 采样/s | 65,536 |
| `maxSeekLatency` p95（60 s 快照间隔） | 5.78 ms | 50 ms |

三条对公式的修正同时成立：

```text
1  activity 的数量级假设成立    40k 谱面实测最大存活 27-52，静态扫描是其 1.3-1.8 倍
2  规模门必须检查确定化后的状态  Tap 的 NFA 随窗口线性增长到 2,166，最小化后只有 122；
                               检查 NFA 会误伤宽窗口条目
3  eventBudget 的瓶颈不在判定侧  最重 profile 单 Tick 最多 94 次运算，五分钟曲目单线程
                               11-30 ms 跑完；本项暂不给建议值，运算单位尚未标定到 CPU
```

**这批数字的口径限制（2026-10-01 补记，D12 已复测）**：上表四项均不经过几何判定，
重新构建后 GCC 与 clang 逐位相同。D12 的几何差异已在 Fold Spike 中修复并复测；影响范围：

```text
可用    activityPeak、patternStateBudget、sampleCostPerSecond、maxSeekLatency
        —— 这四项不经过几何判定，两个编译器逐位相同
约束    任何将来由几何派生的预算项，仍须先完成判定域量程证明与跨实现复测
```

**代表性研究切片（2026-10-01）**：在连续性最小模型上补测一条 96 条 Hold / Slider、
8 lane、672 个输入事件的确定性切片。GCC 与 clang 均得到 `activityPeak=3`、`Gap timer peak=2`、
最大快照 4,777 bytes；Seek p95 分别为 0.014 ms / 0.015 ms，最大值为 0.107 ms / 0.119 ms，
快照恢复逐位无损。该切片仅验证 prepared grace、快照 / Seek 和 deadline 的联合路径，不是正式
作者谱面，不能替代真实内容分布，也不冻结本节候选值。

**§6.1 的新增检查项仍须落实为设计规则**（判定域声明取值范围的约束与具体公式无关）；
本次复测解除的是 D12 的 spike 实现阻塞，不冻结未来尚未实测的几何派生预算数字。

## 4. Seek：时间成本（回应压测 L6）

### 4.1 无损性是正确性要求，不是设计选择

```text
snapshot(T) 恢复后的状态，必须与从 0 连续推进到 T 的状态逐位相同。
```

这条不是取舍。若做不到，判定结果就会依赖"是否 Seek 过"，那是灾难性的：
同一份 Replay 在拖动进度条后会得到不同结果。

### 4.2 必须进快照的状态清单

按 Fold §2 的三种寿命，加上三处容易漏掉的：

```text
per-requirement   匹配状态、寄存器
per-session       L3 折叠状态、导出字段、分数 / Combo / Life
per-device        Controller 运动学状态
Hook 值           全部程序可见 Hook 的当前值（含派生 Hook 的合成输入集合）
待触发定时器      全部未到期的定时器及其持有者
周期采样相位      every 的下一次采样时刻
```

**最后一项是一处交叉检查。** 若采样相位不进快照，Seek 后的采样时刻集合与连续播放不同，
直接复现缺陷 D1（采样相位决定判定宽限）。Fold §7.3 与压测 L1 已经确立"两套格点的相位必须
定死"，本清单把"相位必须进快照"补上。

### 4.3 间隔是自由参数，不是内容声明

**快照间隔不进 Interface，也不进 identity。** 理由是它是无损的加速结构：
改变间隔不改变任何判定结果，只改变内存与 Seek 延迟。

这与"内容不声明实现细节"一致，也回答了压测 L6 的"需要一个显式的预算参数"——
需要的不是内容侧的声明，而是**引擎侧的一个承诺**。

### 4.4 可行区间的推导

```text
seekCost(T) <= eventRate * interval * perEventCost        Seek 延迟
interval <= maxSeekLatency / (eventRate * perEventCost)   -> 上界
memory    = stateBytes * (duration / interval)
interval >= stateBytes * duration / memoryBudget          -> 下界
```

可行条件是两个不等式同时成立：

```text
stateBytes * duration / memoryBudget <= maxSeekLatency / (eventRate * perEventCost)
```

若不可行，说明状态相对事件率过大。三条出路，按优先级：

```text
1  增量快照      只存相对前一快照的变化。打破 memory 与 interval 的线性关系
2  提高折叠速度  降低 perEventCost
3  放宽承诺      提高 maxSeekLatency，或在长时间曲目上禁用拖动
```

**增量快照是唯一能真正打破权衡的手段**，但它的实现复杂度明显更高（需要差分与重建）。
本设计不预先指定，只规定：引擎必须公开 `maxSeekLatency` 作为能力声明，
容量测试必须验证它在最坏输入上成立。

## 5. 哪些参数进 identity，哪些不进

判据只有一条：**改变它会不会改变判定结果。**

| 参数 | 进 identity | 理由 |
| --- | --- | --- |
| `every` 的周期 | **进** | 改变 Spinner 类 Measure 的取值，见 Fold §7.3 |
| `minInputRate` | **进** | 改变重采样结果 |
| `windowScale` 等派生 Hook 的实际值 | **进** | 直接改变判定 |
| 单个 requirement 的 prepared `grace` | **进** | 改变断连接受区间与 deadline |
| `holdGrace` 的默认值 / 范围 / 覆盖开关 | **进** | 改变未覆盖 requirement 的 prepared 参数或合法性 |
| `graceResolutionPolicy` | **进** | 改变显式 / 继承值的解析语义 |
| `graceSource`（explicit / inherited） | 不进 JudgementIdentity | 来源只用于 ContentIdentity 与诊断；最终值已单独进入 |
| 快照间隔 | 不进 | 无损加速结构 |
| 活动度上限 | 不进 | 引擎能力上限，收紧只是拒绝更多谱面 |
| 补的规模门 | 不进 | 同上 |
| 内联 AST 字节预算 | 不进 | 同上 |
| L1 融合算法 | 不进 | 见身份草案 §6，与校准同级 |

**引擎能力上限的收紧是兼容性破坏，不是 identity 变化。** 两者必须分开：
能力上限往下调，会让原本能跑的谱面被拒（兼容门失败），但**凡是能跑的地方，结果必须逐位一致**。
所以它不进 `JudgementIdentity` 的任何分量，包括 engine 分量。

这条区分值得单独记住，因为它容易被写反：如果按"引擎变了就 bump engine 分量"处理，
每次调预算都会失效全部既有 Replay，而实际上判定结果一个字都没改。

## 6. 静态验证的整合清单

把 Program IR §9、Fold §7、Ruleset Fold §7 与本文合并后的完整清单：

```text
类型与引用      Enum、region、controller、hook、frame 必须已声明
取值范围        区间分析，可能越界或溢出即拒绝
必然结算        所有路径在静态可知时刻前到达终态
终止性          语句层只有有界 for；核心每 Tick 执行次数有上界
结算唯一        每条路径上最多一次 settle
规模            状态数、边数、寄存器数、表达式节点数、数组长度、轨道分段数
活动度          扫描线峰值 <= activityPeak                            本文 §3.1
每事件代价      activityPeak * candidateEdges * guardCost <= eventBudget   本文 §3.2
确定化规模      含补的子表达式确定化后 <= patternStateBudget           本文 §3.3
采样负载        sampleCostPerSecond <= 预算                            本文 §3.4
连续宽限        prepared grace 在 Interface 范围内；deadline 按 §3.1a 计算
Gap timer       每个 handoff requirement 至多一个宽限 timer，计入 timer / candidateEdges
内联 AST        个数与字节 <= 预算                                    本文 §2.2
轨道单调性      分段按 Tick 严格升序、不重叠、无空洞、覆盖 [start,end)
采样周期区间    every(period) 落在 Ruleset 声明的允许区间内
槽位下标有界    claim 的槽位下标由有界搜索或常量给出
派生量不变量    每个派生量声明其在输入域上的不变量，保守检查（Fold §6.1）
```

`frame 不可变` 与 `无人写 frame` 是结构保证，不需要检查项。

### 6.1 「取值范围」这一项的范围要扩大（2026-10-01，缺陷 D12）

上表的「取值范围」原本只覆盖**作者写下的值**——内容里的坐标、尺寸、周期。实测发现这只覆盖了
一半：**引擎自己定义的那些判定公式同样会溢出**，而它们不是内容，所以从未经过这一项检查。

```text
已覆盖    内容声明的值        区间分析，越界即拒绝
未覆盖    引擎定义的判定式    几何原语的平方比较、距离比较、角度归一化……
```

D12 的具体表现是几何的平方比较（[Fold Spike 报告 §9.7](../stage_reports/reviews/gameplay-ruleset-2026-10/2026-10-01-fold-spike.md)）：
`cross²` 达到 4.4e20，超出 `int64` 上限 9.22e18，两个编译器各选一条 UB 分支。

因此本项拆成两条，并新增一条**按判定域**的检查：

```text
取值范围（内容）  不变，仍按区间分析
取值范围（引擎）  每个判定域声明坐标与尺寸的取值范围                    新增
判定式量程        每条判定不等式的每一项都能由该声明推出上界，且已证明不溢出   新增
```

新增项与既有各项的区别在于**它不是对内容的检查，而是对引擎常量表的检查**：它是设计
审查中发现一次、之后永久成立的东西，不需要随每份内容重跑。这也是它没被既有的静态验证
清单覆盖的原因——清单检查的是内容。

## 7. 待决

1. **§3.5 的全部数值需要实测冻结。** 这是本文最大的一项未完成工作，需要真实谱面
   （理想是 34 项案例各一份的最小可运行样本）。没有它，本文只能作为实现者的追踪清单。
2. ~~`maxSeekLatency` 的默认值与它属于引擎还是会话配置~~ 已裁决：**引擎能力声明加会话
   可收紧**，沿用 `PackedChartLimits` 的"调用方只能收紧，不能放宽"。

   ```text
   引擎声明  本引擎能保证的拖动响应上界。它是可验证的承诺，容量测试必须覆盖
   会话收紧  宿主可以给一个更紧的值（例如移动端），取 min
   内容声明  不参与。它不改变任何判定结果
   ```

   **默认值属实测范畴**，与 §3.5 的表一同冻结。但它的**量纲**可以先定：
   以"从静止状态 Seek 到曲目任意位置"的 95 分位响应时间表示，而不是平均值——
   平均值会掩盖最坏情况，而用户感知的正是最坏情况。这与 L3 与 L2 预算分开计是同一条
   原则：报告的量必须与实际失败现象对应。
3. ~~增量快照是否要在第一版实现~~ 已裁决：**第一版只做全量快照**。
   理由：增量快照的唯一收益是打破 §4.4 的内存与延迟权衡，而该权衡在第一版的数据规模下
   **尚未被证明会破裂**。先做全量快照，用真实内容测出 `stateBytes` 与 `eventRate`，
   若可行区间不成立再加——那时才有数据判断该优化是否值得。反过来（先做增量）会带来
   差分与重建的实现复杂度，而它在多数内容上可能永远用不到。

4. 内联 Pattern AST 的编码形式未定。**保持不变**：与作者语法（讨论记录 §3.4 第 2 条）
   同属推迟到实现阶段的决定。本项只确定它的**预算口径**（§2.2 的两项计数），
   不预先规定编码。
5. ~~窗口缩放上限与活动度的耦合需要一个具体的检查规则~~ 已裁决：**在 prepare 的扫描线
   里发现，不在 Interface 里预检**。三条理由：

   ```text
   1  两者在同一个时刻都已知    Loadout 在 prepare 前冻结，Interface 的 windowScale
                              区间也在那时可用。扫描线可以同时代入，不需要预检
   2  Interface 预检做不到保守  它只有区间，没有该 Loadout 的具体取值。按区间上界预检
                              会把"这个 Loadout 恰好不用最大缩放"的谱面一并拒绝
   3  诊断更好                 扫描线能指出是哪些 requirement 在叠加上界后越界，
                              Interface 预检只能说"上限太高"
   ```

   由此得到一条实现要求：**活动度扫描线必须接收 Loadout 的窗口缩放取值作为输入**，
   而不是只读 Interface 的声明区间。这条此前没有写在任何地方，而它改变了扫描线的签名。

   **实测后的加强**（Fold Spike 报告 §3.4、§4.2）：后果不只是"活动度估低"，而是
   **判定结果改变**。`arm` 若按基础缩放计算，实例在放宽窗口的前沿尚未激活，提前按下找不到
   候选、变为 stray，音符随后 Miss。定向构造中第 101 个 Tap 因此由 Good 变为 Miss。
   结论：**`arm` 是判定语义，不只是预算输入**，它进 identity 的方式与窗口值相同。

## 8. 与既有文档的关系

```text
新增    本文之前不存在，代价条目分散在四处
不改动  Program IR §9 的检查项继续有效，本文只增补五项
不改动  PACKED_CHART_FORMAT.md §3.3 的冻结值不做修改，本文在其上增加
不改动  Fold §6.1 的通用不变量规则，本文 §3.1 是它的一个实例
回应    Fold §10.4：补进预算不进 capability，裁决见 §3.3
回应    压测 L6：间隔是引擎自由参数，可行区间与出路见 §4.4
回应    压测 L3：采样负载与活动度是乘法关系，见 §3.4
```

补一条对压测 L3 的修正：L3 说"实际可用上限很低"，方向正确，但漏了它与活动度预算
**共享同一个乘数**。两者不是两个独立上限，而是同一份每 Tick 预算的两个消耗方。
