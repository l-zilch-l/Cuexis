# Gameplay V2 表达力压测报告

状态：candidate；研究证据，未接受，未实施

更新日期：2026-10-02

本文对 [Gameplay V2 语义内核](SEMANTIC_KERNEL.md)、[Chart v5 Gameplay 修订案](CHART_V5_GAMEPLAY_AMENDMENT.md)、
[V2 输入与 Replay 约束](INPUT_REPLAY_EXTENSIONS.md) 进行系统表达力压测。它只评审设计能否
稳定表达玩法，不代表任何 `engine/`、Chart v5 Reader、Packed Reader、Ruleset 或 ABI 已经实现。

## 1. 压测问题

本轮不问“是否可以为某个案例写一段特例代码”，而问：

```text
给定 Chart v5 canonical gameplay graph、
有限 RequirementProgram、CoordinationGraph、
Ruleset state transaction 和 normalized observation，
是否能够在不增加未声明运行时自由度的前提下，
得到唯一、可预算、可快照、可重放的结果？
```

每个案例同时检查五个闭包：

1. 输入闭包：输入形态是否能变成规范 Observation，丢样本、设备断开和采样相位是否有明确结果。
2. 局部闭包：单个 Requirement 是否能在有限状态、寄存器和 timer 内产生 Candidate。
3. 协调闭包：多个 Candidate 的资源、顺序、基数和原子提交是否有唯一解。
4. 规则闭包：Fact 是否能按唯一因果序进入一个原子的 Ruleset State Transaction。
5. 格式闭包：Chart v5 inline 与 CXT v2 展开是否都能进入同一 Canonical Gameplay Graph，并
   经 Packed Chart 无损往返。

## 2. 结论标签

| 标签 | 含义 |
| --- | --- |
| `D` direct | 使用已定义语义即可表达，不需要新的语义能力。 |
| `C` composable | 使用现有语义组合可以表达，但需要明确的组合关系、预算或规则配置。 |
| `E` extension-required | 玩法边界清楚，但需要新的 typed capability、几何/采样/solver 或 wire revision。 |
| `B` blocked | 当前设计存在阻塞或语义未闭合；不能靠作者组合绕过。 |
| `O` out-of-scope | 明确超出当前模型目标，例如任意运行时脚本或三维物理。 |

`V2` 列表示完整候选模型的结论，`7A` 列表示当前 Stage 7A 最小闭环是否应纳入。
`later` 表示可作为后续 capability 研究，不能被当作 Stage 7A 已支持；`reject` 表示应稳定拒绝。

## 3. 压测矩阵

### A. 基本时间型

| ID | 玩法 | 局部 Requirement | 协调 / Ruleset | Chart v5 表达 | V2 | 7A | 缺口或风险 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| A01 | Tap | point + press + timing measure | 单事实归约 | inline requirement | `D` | `in` | 无 |
| A02 | Hold head/body | sequence + coverage measure | head/body 分阶段事实 | inline requirement | `D` | `in` | body 预算需单独计数 |
| A03 | Hold release/tail | release phase + end timer | tail fact 与主体分离 | inline requirement | `C` | `in` | release/tail phase 边界、deadline 与 outcome 必须逐字段冻结 |
| A04 | Hold 中途断开与 grace | Gap timer + same-owner recovery | break/recover 事实 | inline requirement | `C` | `in` | 7A 只允许同一 owner 的有限恢复；跨 owner handoff 稳定拒绝；`grace` 必须 prepare-time 冻结 |
| A05 | Hold tail-only | end-only phase | tail-only outcome | inline requirement | `E` | `later` | 需要独立 tail capability，不能把缺失 head 猜成 Hold |
| A06 | Lift | absence predicate + deadline | deadline fact | inline requirement | `D` | `in` | 空输入与设备断开必须区分 |
| A07 | Mine/Bomb | observe candidate + forbidden action | observe 不消费，Ruleset 扣分 | inline requirement | `D` | `later` | 鬼按策略由 `strayWhen` 决定 |
| A08 | 早按、晚按、空按 | timing window + LocalClose | stray / miss fold | inline requirement | `D` | `in` | 窗口端点必须固定为开闭区间 |
| A09 | 同一 Tick 多个输入 | 多 Candidate | ingress sequence + causal order | requirements + coordination | `C` | `in` | 不得依赖容器枚举顺序 |
| A10 | 同一输入竞争多个 Requirement | 多 Candidate claims | exclusive(resource) | coordination.resources/relations | `C` | `in` | 需要稳定 claim priority |
| A11 | Note lock | bounded owner / lock window | lock 与 exclusive 组合 | coordination.relations | `C` | `later` | lock 的释放时刻必须进入 identity |
| A12 | 判定窗内 N 选 M | bounded repeat / counter | quota(window, min, max) | coordination.relations | `C` | `later` | quota 的失败时点和部分成功要冻结 |

### B. 序列和节奏型

| ID | 玩法 | 局部 Requirement | 协调 / Ruleset | Chart v5 表达 | V2 | 7A | 缺口或风险 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| B01 | 连打 / Roll | bounded repeat + count | quota 或累计 fold | requirements + quota | `C` | `later` | 事件数和 timer 数必须有上界 |
| B02 | Jack / 同键重复 | sequence + same channel | exclusive channel | requirements + resource | `C` | `later` | 同 Tick 重复边界需统一 |
| B03 | 交替键 | finite sequence | temporal(before/within) | coordination.relations | `C` | `later` | 长交替必须展开为有界实例 |
| B04 | 有界 Repeat | bounded repeat | 每次 emission 独立 commit | CXT v2 -> gameplay.requirements | `C` | `later` | count、身份和预算均需冻结 |
| B05 | 先后顺序 | sequence / temporal | before/after | coordination.relations | `C` | `later` | 关系不能依赖数组顺序 |
| B06 | 跳步 / 跳区 | choice + skip | quota 或 temporal | coordination.groups | `C` | `later` | 选择后的未选项结算策略要声明 |
| B07 | 保护音符 | bounded requirement + protected flag | quota / shield state | typed extension | `C` | `later` | 保护状态不能由表现 Effect 回写 |
| B08 | 窗口内 N 选 M | bounded candidates | quota(cardinality) | coordination.groups | `C` | `later` | 选择唯一性与 tie-break 必须可证明 |
| B09 | 气球 / 区间累计 N 次 | counter measure | quota + ledger fold | requirement + relation | `C` | `later` | 中途进度是否产生 PresentationEvent 要分离 |
| B10 | 鬼按惩罚 | observe + stray policy | `strayWhen=noCandidate` | ruleset projection | `C` | `later` | “考虑过但拒绝”不能依赖遍历顺序 |
| B11 | Spinner / bounded axis count | axis step + bounded repeat | quota(window) | capability relation | `C` | `later` | 轴步长、去抖和采样预算必须声明 |

### C. 多 Requirement 协调型

| ID | 玩法 | 局部 Requirement | 协调 / Ruleset | Chart v5 表达 | V2 | 7A | 缺口或风险 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| C01 | 两键和弦 | 两个独立 Candidate | binding(cardinality=2, atomic) | coordination.groups | `C` | `later` | 原子成功/失败需一次性提交 |
| C02 | 三键和弦 | 三个独立 Candidate | binding(cardinality=3) | coordination.groups | `C` | `later` | solver 分支数进入预算 |
| C03 | 部分和弦成功 | 独立 phase / local miss | binding(cardinality=at-least) | coordination.groups | `C` | `later` | “未组成和弦”的成员不能重复计分 |
| C04 | 多段同时提交 | 多组件 Candidate | binding + componentIndex | requirements + groups | `C` | `later` | Fact 顺序必须保持组件因果 |
| C05 | 共享接触点 | contact claim | exclusive(resource) + binding | coordination.resources | `C` | `later` | contact 生命周期需明确 |
| C06 | 多个 Requirement 竞争同一 contact | 多 Candidate claims | exclusive(capacity=1) | coordination.relations | `C` | `later` | 稳定优先级不能来自 source order |
| C07 | Handoff | Gap candidate + new candidate | handoff 作为新竞争 | resource lease | `C` | `later` | 旧 lease 终止与新 lease 的同 Tick 顺序需冻结 |
| C08 | Sticky | 持有型 Candidate | terminal / no handoff | resource lease | `C` | `later` | 第二 contact 的 stray 结果需由策略决定 |
| C09 | Observe + consume 并存 | 同一 Observation 的两类 Candidate | observe 不占用 consume 资源 | requirements + relations | `C` | `later` | observe 是否能参与 binding 要明确禁止或定义 |
| C10 | Fanout 1 / fanout all | 多个可消费 Candidate | arbitration.fanout | ruleset projection | `C` | `later` | `fanout` 粒度必须进入 identity |
| C11 | 同一输入恢复旧 Requirement 并触发新 Requirement | 两个 Candidate | fanout + handoff + exclusive | coordination.relations | `C` | `later` | 两者均 consume 时需要唯一资源方案 |
| C12 | 需要全局最优的多指匹配 | 多个交叉 claims | solver capability | coordination.relations + extension | `E` | `later` | 稳定贪心不保证最大可行集合 |
| C13 | Handoff 与新和弦同 Tick | gap、new note、binding 同时存在 | 多窗口交叉提交 | coordination.relations | `B` | `blocked` | 当前没有明确“先结束 lease 还是先求和弦”的规范序 |

### D. 连续和空间型

| ID | 玩法 | 局部 Requirement | 协调 / Ruleset | Chart v5 表达 | V2 | 7A | 缺口或风险 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| D01 | Slider 连续轨迹 | pointer path + coverage | resource lease | typed geometry extension | `E` | `later` | 需要轨迹段与覆盖率的 canonical 表示 |
| D02 | 分段 Slider | 多段 path requirements | binding + ordered temporal | requirements + relations | `E` | `later` | 轨迹段边界与部分成功规则未冻结 |
| D03 | Slider 断开与恢复 | Gap + pointer continuity | handoff + grace | resource lease + extension | `E` | `later` | 采样空洞和恢复位置的关系未定义 |
| D04 | 轨迹覆盖率 / Slider tick | periodic measure | quota / scheduled sample | typed extension | `E` | `later` | 采样相位必须进入 snapshot 和 identity |
| D05 | 区域持续判定 | region predicate over time | quota / deadline | geometry extension | `E` | `later` | 两次 observation 之间越界如何解释未定 |
| D06 | Flick 方向 | begin/end vector + direction measure | optional velocity rule | geometry/action extension | `E` | `later` | 方向量化和最小弧度需注册 |
| D07 | 最小位移 | displacement register | local measure | geometry extension | `E` | `later` | 起点、重置和 discontinuity 的语义需固定 |
| D08 | 速度区间 | delta/time measure | ruleset grade | input resampling + extension | `E` | `later` | 速度由哪一组规范样本计算必须冻结 |
| D09 | 稀疏采样 | normalized update | sample schedule | input capability | `E` | `later` | 不能把稀疏输入静默视为静止 |
| D10 | 丢样本 / discontinuity | discontinuity observation | break or reject policy | input capability | `E` | `later` | 需要每种 requirement 的显式响应策略 |
| D11 | 动态判定区域 | bound geometry + frame | frame relation | gameplay geometry | `E` | `later` | 相机/表现变换不能隐式成为判定输入 |
| D12 | 旋转判定线上的音符 | orientation/frame observation | temporal frame binding | gameplay geometry | `E` | `later` | 单圈角可表达，多圈累计不是默认能力 |
| D13 | 设备朝向驱动轨迹 | orientation + pointer | bound controller | input + geometry extension | `E` | `later` | orientation discontinuity 和校准要进 session |
| D14 | 压力 / 按量阈值 | amount register | grade / threshold | action/amount extension | `E` | `later` | 设备类别间的 amount 不能假设等价 |

### E. 状态和规则型

| ID | 玩法 | 局部 Requirement | 协调 / Ruleset | Chart v5 表达 | V2 | 7A | 缺口或风险 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| E01 | Combo threshold | 普通 Fact | state register predicate | ruleset package | `C` | `in` | threshold 读取时点要固定 |
| E02 | Life threshold | 普通 Fact | bounded life register | ruleset package | `C` | `in` | 饱和、下溢和失败边界需冻结 |
| E03 | 分数倍率 | grade/outcome Fact | monoid 或 exclusive reducer | ruleset package | `C` | `in` | 倍率是否影响同 Tick 后续 Fact 要声明 |
| E04 | 护盾 / 免 Miss | miss Fact | state transaction + next tick signal | ruleset package | `C` | `later` | 表现 Effect 不能直接消费护盾 |
| E05 | 技能冷却 | Fact + timer | bounded register/timer | ruleset package | `C` | `later` | timer ordinal 必须进入因果序 |
| E06 | 连续命中奖励 | ordered Fact stream | fold state | ruleset package | `C` | `in` | 必须依赖 Fact 顺序而非实例顺序 |
| E07 | Miss 后保护 | miss -> signal | next tick RuleEffectEvent | ruleset package | `C` | `later` | 同 Tick 保护是否生效必须禁止或明确 |
| E08 | 同 Tick 技能因果 | multiple Facts | atomic state transaction | ruleset package | `C` | `later` | 读取 Tick-start state，避免半提交 |
| E09 | 下一 Tick 信号 | Fact -> signal queue | deferred signal | ruleset package | `C` | `in` | signal queue 必须进 snapshot |
| E10 | correction Fact | append-only ledger | explicit correction reducer | ruleset package | `C` | `later` | correction 不能撤回旧 Fact，审计语义需固定 |
| E11 | 规则模块冲突 | 无需特殊 Requirement | prepare ownership validation | ruleset package | `D` | `in` | 冲突必须在 prepare 拒绝 |
| E12 | 不同 Loadout | 同一 graph | session projection | Chart v5 + session | `C` | `in` | 实际生效值而非声明范围进入 identity |

### F. Replay 和生命周期型

| ID | 玩法 | 输入 / 状态 | 协调 / Ruleset | Chart v5 表达 | V2 | 7A | 缺口或风险 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| F01 | 实时输入与 Replay 等价 | normalized observations | 同一 submit path | Chart semantic identity | `D` | `in` | 禁止 Replay 直发 Fact 快捷路径 |
| F02 | Snapshot / Seek | 完整程序和 ledger 状态 | 从已提交快照重放 | packed semantic graph | `D` | `in` | 表现缓存不能替代语义状态 |
| F03 | 迟到事件：reject / queue | late observation | 冻结 policy | session identity | `C` | `in` | `queue_next_tick` 的窗口上界需计入预算 |
| F04 | 迟到事件：reopen 未提交窗口 | pending candidates | reopen window | session capability | `B` | `blocked` | commitId、snapshot、Replay 与窗口重开关系未闭合 |
| F05 | pause / resume | timeline discontinuity | explicit session state | session metadata | `C` | `in` | 暂停是否停止 chartTick 必须统一 |
| F06 | reset / reload | transaction replacement | old session remains on failure | Chart v5 source | `D` | `in` | reset 不得复用旧 contact handle |
| F07 | 热插拔 / 设备变化 | discontinuity + capability | continue, pause or terminate | session capability | `C` | `later` | 三种结果必须由 adapter 明确选择 |
| F08 | 运行中修改输入映射 | mapping mutation | session transaction | session identity | `B` | `reject` | 运行中修改会改变 observation 解释，当前不允许 |
| F09 | 不同设备类别 | button/contact/axis/orientation | capability negotiation | input extension | `E` | `later` | “设备可连接”不等于满足最低能力 |
| F10 | 跨编译器 / 跨排序确定性 | same graph + same observations | canonical total order | Packed semantic identity | `D` | `in` | 需跨实现 evidence，不可只靠单实现推演 |
| F11 | Packed round-trip | source -> packed -> graph | decoder 不执行 solver | Packed Chart v5 | `D` | `in` | source 与 artifact identity 必须分离 |
| F12 | source reorder | reordered arrays | canonical sort | Chart v5 graph | `D` | `in` | relation 成员排序不能改变语义 |
| F13 | CXT v2 compile | finite expansion | canonical lowering | CXT v2 -> gameplay | `D` | `in` | CXT AST 不得进入 Playback |
| F14 | inline Requirement 与 CXT Requirement 等价 | same typed record | same graph path | `gameplay.requirements` | `D` | `in` | source map 不得进入 judgement identity |

### G. 格式和作者层型

| ID | 玩法 | 局部 Requirement | 协调 / Ruleset | Chart v5 表达 | V2 | 7A | 缺口或风险 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| G01 | 无表现 Requirement | gameplay only | no presentation dependency | gameplay.requirements | `D` | `in` | 表现缺失不能删除判定 |
| G02 | 一个 Requirement 多个表现 | one requirement | presentation fanout | requirementRef[] | `D` | `in` | EffectBinding 只读 |
| G03 | 多个 Requirement 一个表现 | many requirements | explicit refs | presentation graph | `D` | `in` | 不得用 parent 推断 binding |
| G04 | 装饰对象无 Requirement | no gameplay node | presentation only | presentation instances | `D` | `in` | 删除装饰不改 judgement identity |
| G05 | 表现 parent 与 gameplay relation 分离 | independent graph | coordination relation | separate refs | `D` | `in` | parent 不能替代 exclusive/binding |
| G06 | 只改皮肤保持 Replay | same gameplay graph | same judgement identity | presentation closure | `D` | `in` | presentation 资源不进入 judgement identity |
| G07 | 修改判定区域使 Replay 失效 | changed geometry | changed judgement projection | gameplay geometry | `D` | `in` | 必须改变 chart/session judgement identity |
| G08 | CXT v2 参数冻结 | finite parameter binding | prepare-only | cxtInstances + gameplay | `D` | `in` | 运行时不能重新展开 |
| G09 | 超预算展开 | finite expansion | prepare rejection | capability/budget diagnostics | `D` | `in` | 不得截断或生成半成品 |
| G10 | 未知 capability | no runtime fallback | stable reject | extensions/capability registry | `D` | `in` | 未知、未启用、预算不足需分码 |
| G11 | 非终止模板 | invalid source | compile rejection | CXT v2 validation | `D` | `in` | 不得把非终止 AST 带入 Packed |
| G12 | v4/v5 歧义迁移 | missing domain/geometry | explicit ambiguous report | v5 migration output | `D` | `in` | 不得猜测 render geometry 是 judgement geometry |

### H. 明确超出范围

本组仍统一使用 `O` 表示“不属于当前 Gameplay V2 语义闭包”。但支持政策分为两类：
H01-H05、H07 是明确的能力边界，不会再开发，也不受支持；H06、H08 是未来可能独立
立项的方向，当前不支持，不能进入现有 Chart v5 或 Stage 7A 合同。

| ID | 玩法 / 能力 | 结论 | 支持政策 | 原因 |
| --- | --- | --- | --- | --- |
| H01 | 任意运行时脚本 VM | `O` | 能力边界，不会再开发，不受支持 | 会引入未声明状态、资源、执行时间和 Replay 语义。 |
| H02 | 运行时动态生成 Requirement | `O` | 能力边界，不会再开发，不受支持 | 破坏 prepared graph、snapshot 闭包和 identity。 |
| H03 | 随机生成谱面或随机判定 | `O` | 能力边界，不会再开发，不受支持 | 当前没有随机源、种子、预算和 Replay 合同。 |
| H04 | 墙钟依赖 | `O` | 能力边界，不会再开发，不受支持 | 判定必须由 chart/observation 时间驱动，不能读取系统时间。 |
| H05 | IO / 网络直接参与判定 | `O` | 能力边界，不会再开发，不受支持 | 外部副作用不能成为 Fact 因果来源。 |
| H06 | Beat Saber 类三维物理斩击 | `O` | 未来开发，现不支持 | 需要三维扫掠体、碰撞和轨迹相交语义，不是加一个 Vec3 字段。 |
| H07 | Rocksmith 类连续音高 | `O` | 能力边界，不会再开发，不受支持 | 需要音高估计、连续频率容差和设备校准闭包。 |
| H08 | 任意自由体感传感器融合 | `O` | 未来开发，现不支持 | 需要独立传感器融合、校准、漂移和可信度语义。 |

## 4. 统计结果

本轮共检查 96 个案例：

| 结论 | 数量 | 占比 | 解释 |
| --- | ---: | ---: | --- |
| `D` | 26 | 27.1% | 现有 V2 语义可以直接承载。 |
| `C` | 42 | 43.8% | 通过局部程序、协调关系和 Ruleset 组合承载。 |
| `E` | 17 | 17.7% | 需要新增、但边界可以独立冻结的 capability。 |
| `B` | 3 | 3.1% | 当前有结构性阻塞，不能宣称“可表达”。 |
| `O` | 8 | 8.3% | 明确超出本模型目标。 |

因此有三种不能混为一谈的覆盖率：

```text
当前语义直接覆盖       = D                  = 26 / 96 = 27.1%
不新增 capability 的覆盖 = D + C              = 68 / 96 = 70.8%
允许后续有界扩展的覆盖   = D + C + E          = 85 / 96 = 88.5%
存在明确候选路径的案例 = D + C + E          = 85 / 96 = 88.5%
```

最后一个数字不能被解释为“当前已经支持 88.5%”。其中 17 项 `E` 仍需独立的输入、几何、
solver、预算、Packed、Replay 和跨实现证据；3 项 `B` 在没有修订语义前不应进入实现。

Stage 7A 的合理边界仍然是：

```text
Tap、Hold head/body、Release、prepared grace、
单一 exclusive resource、Fact Ledger、typed Ruleset transaction、
Replay、Snapshot、Seek 和明确拒绝路径。
```

和弦、Roll、交替、复杂 quota、连续轨迹、方向/速度、多设备类别和全局最优匹配属于后续
capability。它们不能因为能在表格中写出 Requirement 就被标为 7A 已支持。

## 5. 结构性攻击结果

### 5.1 稳定贪心不是一般协调求解器

构造以下候选：

```text
resource A: capacity 1
resource B: capacity 1

candidate c1 claims A
candidate c2 claims A, B
candidate c3 claims B
```

如果 `c2` 排在最前，稳定贪心只得到一个候选；如果先选择 `c1` 和 `c3`，则得到两个候选。
当玩法要求最大成功集合、最小未完成数量或指定和弦完整度时，排序键并不能替代全局求解。

当前设计的“贪心 + 有界回溯”可以承载一部分固定小图，但必须补充：

1. `solverProfile`：`greedy`、`bounded_backtracking` 或带版本的最优匹配能力。
2. 目标函数：最大基数、最大权重、优先完整 binding，不能只写“可行集合”。
3. 静态证明：候选数、资源数、分支数和最坏 fuel。
4. 无法证明唯一结果时稳定拒绝，而不是运行时截断。

因此 C12 标为 `E`，不是 `C`。

### 5.2 DAG 不能承载未展开的循环关系

有限 Roll、交替和周期动作可以通过 CXT v2 repeat 或有限窗口 quota 展开。但以下关系不能
直接作为 DAG：

```text
A after B after A
同一资源在多个阶段循环交接
未预先知道长度的重复节奏
```

候选修法是编译期有界展开，或者新增 `boundedRelationInstance`，其中明确 `count`、每次
实例的 source path、资源状态和窗口。不能把运行时循环偷偷放进 `CoordinationGraph`，也不能
把“图无环”检查降级为实现细节。

### 5.3 Append-only Fact Ledger 与迟到事件冲突

`reject_late` 和 `queue_next_tick` 可以保持 append-only 账本。`reopen_uncommitted_window`
则要求在窗口尚未提交时改变 Candidate 集合，这会影响：

```text
commitId 分配
CoordinationCommit 的可见性
snapshot 中未提交窗口的版本
Replay 对“何时最终提交”的重建
```

在没有 `windowEpoch`、最终化边界、重开次数上限和 Replay 记录之前，F04 不能被称为可表达。
建议 Stage 7A 只接受前两种策略；`reopen_uncommitted_window` 保持 candidate capability，
并要求重开只发生在未产生 Fact 的窗口。

### 5.4 Fact 总序仍需补“因果来源”而不是只补排序字段

当前总序已经比 `(requirementId, phase, outcome)` 完整，但
`sourceEventSequenceOrTimerOrdinal` 仍可能在不同 timer 域、不同输入域或不同窗口中重号。
实现前应把它规范化为带来源类型的 tuple：

```text
causalOrigin = (
  originKind: observation | timer | coordination | correction,
  originId,
  localOrdinal
)
```

随后再按 `chartTick -> phasePriority -> causalOrigin -> commitId -> requirementId ->
componentIndex -> emissionIndex` 排序。`originId` 必须由引擎分配，不能由宿主提供。否则同 Tick
的多个 timer、correction Fact 和跨窗口 commit 仍可能依赖分配顺序。

### 5.5 资源生命周期需要显式阶段

`owner`、`leaseStart`、`leaseEnd` 和 `handoffPolicy` 还不足以解决以下同 Tick 情形：

```text
旧 Hold 进入 grace
新 Tap 申请同一 contact
第三个 Requirement 观察该 contact
和弦 binding 等待两个资源
```

必须明确资源状态至少包含 `free`、`held`、`gap`、`handoff_pending`、`terminal`，以及每个状态
能产生 Candidate 的 phase。旧 lease 的终止、gap 的恢复和新 claim 的竞争应在同一个
Coordination window 中通过显式优先级求解。否则 C13 的结果依赖“先遍历哪个 Requirement”。

### 5.6 连续输入不能只记录离散 Observation

`NormalizedObservation` 能保存 position、amount 和 discontinuity，但它本身没有回答：

```text
两次 update 之间是否穿过区域？
轨迹越界后又返回，是否算连续覆盖？
低采样率导致的长线段应按直线、阶梯还是拒绝？
orientation 在断连后能否直接作为下一角度？
```

连续玩法需要独立 capability 声明轨迹重建、交叉检测、采样相位、最低上报率、discontinuity
处理和预算。否则 D01-D14 只能保持 `E`，不能被“有 pointer/update 字段”提前降级为 `C`。

### 5.7 Ruleset 事务需要读写集和失败语义

`exclusive`、`monoid` 和 `ledger_derived` 已经解决了部分写入冲突，但还需要明确：

```text
每个模块的 read set / write set
StateDelta 的溢出、冲突和预算失败是否使整个 Tick 回滚
correction Fact 是否允许再次触发 correction
同一 Tick 的 derived register 是否只能读取 Tick-start state
PresentationEvent 是否能观察回滚前的临时值
```

V2 当前选择“任何一项失败则整个 Tick 不提交”，这是正确的保守方向，但应把读写集和
禁止递归 correction 写进 prepare 验证，否则模块组合仍可能形成隐式顺序依赖。

### 5.8 Chart v5 与 CXT v2 的关系需要 canonical merge 规则

CXT v2 只能安全地产生局部 Requirement 和 local relation；Chart v5 才拥有全局
`resources/groups/relations`。多个 invocation 合并时必须定义：

1. local relation 如何获得全局稳定 ID；
2. 同名 resource 如何拒绝或显式合并；
3. emission path 改变时 source map 如何保留；
4. CXT 数组重排和 Chart invocation 重排为何不改变 graph；
5. 一个 CXT local relation 跨 invocation 引用时是拒绝还是提升为 Chart-level relation。

在这些规则冻结前，CXT v2 到 Chart v5 的“可编译”只能指局部案例，不能宣称全量格式闭包。

## 6. 必须在下一版设计中补的内容

在 ADR 0043 或新的 V2 ADR 接受前，至少应补以下条款：

| 优先级 | 修订项 | 目的 | 影响范围 |
| --- | --- | --- | --- |
| P0 | 资源状态机和同 Tick lease/handoff 顺序 | 解除 C13，避免 contact 竞争依赖遍历顺序 | CoordinationGraph、Fact identity、Snapshot |
| P0 | causalOrigin 总序 | 使 timer、correction、跨窗口 Fact 有唯一因果键 | Fact Ledger、Replay |
| P0 | solverProfile 和目标函数 | 区分稳定贪心与全局最优求解 | coordination extension、预算、identity |
| P0 | late-event policy 的最终化边界 | 解除 append-only 与 reopen 冲突 | Input、Snapshot、Replay |
| P1 | boundedRelationInstance 或编译期循环展开合同 | 覆盖有限循环，同时保留 DAG 可验证性 | CXT v2、Chart v5 relations |
| P1 | 连续轨迹/采样 capability | 说明 update 之间的路径语义和丢样本处理 | Input、geometry extension、Packed |
| P1 | Ruleset read/write set 与 correction 递归禁令 | 使 Tick 事务真正可组合 | Ruleset package、Snapshot |
| P1 | CXT local relation 的全局 merge 规则 | 保证 inline/CXT canonical graph 等价 | Chart v5 gameplay、source map |
| P2 | extension closure checklist | 防止新增 capability 只有字段没有 Replay/Packed/预算 | capability registry、format gate |

## 7. 验收建议

正式进入 Stage 7A 实施准备前，应建立一份与本文一一对应的 reference evaluator 计划：

```text
每个 D/C 案例至少有一个正例和一个边界负例；
每个 E 案例至少有 capability、预算、identity 和稳定拒绝样例；
每个 B 案例必须先有设计修订和反例；
每个 O 案例必须确认不会通过未定义 extension 偷渡；
```

每个案例都应执行以下置换：

1. Requirement、relation、source 数组重排；
2. Candidate 产生顺序和实例枚举顺序重排；
3. 同 Tick observation 的 ingress sequence 置换；
4. Snapshot / Seek / Replay 与从头运行对照；
5. Chart v5 inline、CXT v2 展开和 Packed round-trip 对照；
6. 至少两套编译器或独立求值实现的 Fact Ledger 对照。

本文的主要结论是：V2 已经从“单 Requirement 能否命中”推进到“跨 Requirement 能否形成
唯一交易”，因此其表达力足以覆盖绝大多数二维、有限状态、节奏驱动玩法；但 88.5% 这个数字
只表示在明确拒绝 8 个目标外、并暂不计 3 个阻塞后，候选模型有路径可达。它不是实现覆盖率，
也不是授权把所有 `E` 案例塞进 Stage 7A。P0 修订完成前，和弦资源交接、全局多指匹配、
迟到窗口重开和因果总序仍不应进入生产合同。
