# Gameplay V2 字段与运行语义规范

2026-10-09 实时架构规划登记：[宿主边界草案](../api/realtime-host-boundary.md)的TB-01–07
属于首用待定问题；§3.7.3唯一canonical来源、同Tick拒绝、既有late-policy与恢复语义继续有效。
草案不许可从raw设备时间回溯生成Tick，也不承诺任意输入交付延迟均与无延迟结果相同。

## S7A-7/8 消费修订 revision 1（2026-10-07）

本轮按 owner 实施授权采用 plan-a P78 推荐及 R78 推荐；限于首次消费合同。
字段/所有权表归 [ABI 首用补充](../api/GAMEPLAY_V2_ABI.md#s7a-78-公共首用补充-revision-12026-10-07)，
操作、失败及 mutation generation 表归 [Playback lifecycle](../api/playback-session.md)。
旧“无输入休眠”限定为未启用 Gameplay 的纯播放/Preview。显式 Gameplay commit 后空输入
advance 仍运行到期 timer，Miss 经实际 kernel/Fold 发布；不改变 3/4/5/6 的规则。

表现发布使用成功 Fold factCursor/workTick，无 Fold 使用 seal prefix；失败 Tick 已 sealed Fact
可查询但不得激活 Runtime token。faulted 只读重建与实时发布是不同操作。
projection scope 绑定当前 Ledger；Seek/restore/分支重建全部临时 cursor/dedup/queue/group/token 后
一次 swap；失败保持旧画面。纯表现变动不进入判定四分量，恢复重取当前有效 binding/resource。

resolver 层域固定 Initial < Behavior < Animation < GameplayOverride < HostOverride <
StudioPreviewOverride。priority 仅在同层比较；不使用 Host magic priority 模拟 Gameplay。
Gameplay PresentationTick 使用显式 signed i64 绝对区间：有限 [start,end)，start<end；
或显式 UntilReset。T 上先去除 end<=T，再激活 start<=T<end；越过整段不补发瞬时画面。
同 Tick 不按 queue 插入顺序选赢家；同 Fact/target 等值折叠，异值且不能静态证明互斥的绑定
prepare 拒绝。旧 Host RemainingFrames/UntilChartTimeMs 含义和布局保持。

Graph parsing 必须在 json_support 在容器分配前检查 grammar/depth/count/bounds/重复键，
i64/u64 完整保真且不经 f64。新增生产阈值未经接受保持 gate incomplete，不能机械套 Packed
界；physical/profile/revision 由 Entry/Capsule 合同拥有。Graph/Packed 进入同一实际 kernel。
filesystem 多产物使用内容派生 immutable generation、现有 manifest 单一 commit 点；旧 generation
不自动 GC；单 CXC 保持现有原子发布。原子可见性不承诺断电持久性。

状态：candidate

更新日期：2026-10-05

本 Spec 是 Stage 7A 的候选合同正文，不是字段、公共头、Schema、Replay 或 Snapshot 的实现证据。

**2026-10-05 首次执行补充。** S7A-3 余项与 S7A-4 采用
[execution profile](gameplay-v2-execution-profile.md) 和
[author profile](gameplay-v2-author-profile.md) 的完整选定方案，选择理由见
[ADR 0045](../adr/0045-gameplay-v2-execution-profile.md)。补充拥有本次新增运行字段/语义，
[Capsule §12](GAMEPLAY_CAPSULE_V2_FORMAT.md) 拥有其 candidate revision3 wire。
旧限定冻结中的“首次消费时补齐”在这些明确覆盖字段上已有落点；不表示产品实现或整批验收。
局部实现与整批验收须区分，当前进度只由 [CURRENT_STATUS](../CURRENT_STATUS.md) 拥有。

上级文档：[Stage 7A 实施计划](../stage_plans/active/stage-07/plan-a.md) ·
[Gameplay V2 研究索引](../proposals/research/gameplay-v2/README.md) ·
[Gameplay V2 acceptance package](../proposals/gameplay-v2-acceptance/README.md)

文档角色：**V2 gameplay 字段与运行语义的唯一权威 Spec**。本文件只承载字段、语义、版本、
兼容、预算口径与错误合同。它不承载决策理由、备选方案与威胁模型（属
[ADR 0044](../adr/0044-gameplay-v2-semantic-kernel.md)），不复述 typed C++ 边界、类型宽度、
所有权与生命周期（属 [Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md)），不承载阶段进度与
批次门禁（属 [Stage 7A 实施计划](../stage_plans/active/stage-07/plan-a.md)）。

## 0. 术语与缩写

| 缩写 | 文档 |
| --- | --- |
| PD | [Preliminary Design](../proposals/research/gameplay-v2/PRELIMINARY_DESIGN.md) |
| SK | [Semantic Kernel](../proposals/research/gameplay-v2/SEMANTIC_KERNEL.md) |
| DD | [Design Decisions](../proposals/research/gameplay-v2/DESIGN_DECISIONS.md) |
| FB | [Format Boundary](../proposals/research/gameplay-v2/FORMAT_BOUNDARY.md) |
| AM | [Chart v5 Gameplay Amendment](../proposals/research/gameplay-v2/CHART_V5_GAMEPLAY_AMENDMENT.md) |
| IR | [Input / Replay Extensions](../proposals/research/gameplay-v2/INPUT_REPLAY_EXTENSIONS.md) |
| AR | [Chart v5 Alignment Review](../proposals/research/gameplay-v2/CHART_V5_ALIGNMENT_REVIEW.md) |
| ST | [Expressiveness Stress Test](../proposals/research/gameplay-v2/GAMEPLAY_EXPRESSIVENESS_STRESS_TEST.md) |
| CM | [S7A-0.2 合同逐项台账](../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md) |
| SR | [支持 / 拒绝矩阵与 capability registry 草案](../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md) |
| FE | [格式、入口与 identity 矩阵](../proposals/gameplay-v2-acceptance/FORMAT_ENTRY_AND_IDENTITY.md) |
| RW | [未决语义分轮裁决清单](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) |

术语：**准备值（prepared value）** 指 prepare / 离线 assembler 结束后只读生效的最终值；
identity 判定一律以准备值为准，不以作者源表达形式为准。**合法物理入口** 指 Chart v5 JSON
`gameplay` 区段、Packed Chart entry 与 CXC `gameplay-graph` entry 三种载体。

### 0.1 时间域术语

本小节固定四个时间域名与两条时间算术规则；语义正文见 §3.7（第 2 轮裁定，2026-10-03）。

| 术语 | 定义 |
| --- | --- |
| `ChartTick` | 谱面锚点、Requirement 窗口与 Coordination 窗口所在的**有符号 64 位整数**时间域；单位由 `TimebaseProfile` 声明，不由类型名声明 |
| `judgementTick` | prepare 期由作者 RationalBeat 精确映射并冻结、运行期只读的**有符号 64 位整数**判定时间域；判定路径只按它定位 |
| `observationTick` | 输入进入判定管线时捕获的**校准后会话时钟（calibrated session clock）**所对应的 `judgementTick`；是该观测的 canonical 判定时间 |
| `commitTick` | **事务提交发生的 Tick**；该 Tick 上的 Fact 与状态增量在同一事务内提交（P2-03） |
| commit window | **相邻两个可提交 Tick 之间的观察区间**；事实**只能**在 `commitTick` 提交，未提交窗口内的内容不构成事实 |
| 半数取偶（round-half-to-even） | 精确有理数在恰好落于两整数中点时取**偶数**一侧的舍入；与其它取值一样**对负值对称**，并使用精确有理数运算，不使用浮点或宿主数学库 |
| canonical ordinal | 同一 Tick 内由规范键派生的确定性次序分量（`canonicalOrdinal`）；它由 canonical 窗口 / 观测 identity 派生，宿主**不得**提供，也不取自 ingress 到达顺序 |

## 1. 范围与权威

### 1.1 本 Spec 拥有的内容

1. gameplay 字段集合与字段语义；
2. 运行语义：时间域、Tick 阶段、仲裁、资源、Fact Ledger、Ruleset 事务、表现桥接。
   **这七项按轮次分批写成章节**：时间域与 Tick 阶段（第 2 轮，已裁定）、仲裁与资源（第 4 轮，已裁定）、
   Fact Ledger / Ruleset 事务 / Score 与 Snapshot 字段集（第 5 轮，已裁定）、表现桥接与诊断（第 6 轮，
   **已裁定**，2026-10-03；见 §11）。已裁定部分的语义正文分别见 §3.7、§3.9–§3.13、§3.14–§3.22、
   §3.23–§3.25 与 `## S7A-1` … `## S7A-7` 各节；
   **第 1–7 轮已全部裁定（无待裁定轮次）**（第 7 轮"收尾澄清与缺陷"由 Codex 2026-10-03 裁定，见 §11 与
   `## S7A-7 之后的收尾` 一节；16 行 / 18 项、编号 `S7A7-R01…R16`）。本 Spec 对这些语义的授权范围以各
   `## S7A-n 限定冻结范围与登记规则` 节为准；**裁定完成不等于实现完成**，已有局部实现不代表 S7A-3 整批验收；
3. 版本层级与兼容/互换合同；
4. 唯一 identity projection 表（§5）；
5. capability 派生与闭包合同（§6）；
6. 支持集合与稳定拒绝清单（§7）；
7. 预算上界与计数口径（§8）；
8. 错误与诊断合同（§9）；
9. Replay / Snapshot / Schema 的契约边界登记（§10）。

### 1.2 本 Spec 不拥有的内容

| 内容 | 权威位置 |
| --- | --- |
| 决策、备选方案、威胁模型、接受门禁 | [ADR 0044](../adr/0044-gameplay-v2-semantic-kernel.md) |
| typed 内部/preview C++ 边界：类型、命名、整数宽度、所有权、异常边界、生命周期、序列化布局 | [Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) |
| 阶段目标、批次、依赖、门禁、冻结顺序 | [Stage 7A 实施计划](../stage_plans/active/stage-07/plan-a.md) |
| W 类缺口的编号规则与登记位置 | [Stage 7B+计划](../stage_plans/active/stage-07/plan-b.md#53-w-类缺口记录w-class-gap-record) §5.3（登记与引用规则） |
| W 类缺口的字段定义与具体登记项 | [支持 / 拒绝矩阵](../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md) §6（D-3 落地的分工；见 §1.3） |
| Packed 已冻结物理预算表 | [Packed 候选物理合同 §3.3](PACKED_CHART_FORMAT.md) |
| S7A-3 首次 Packed 消费的 candidate 物理字段、REF0 子域与结构 hash | [Gameplay Capsule v2 format](GAMEPLAY_CAPSULE_V2_FORMAT.md) |
| 四层共同语义基础 | [音乐游戏玩法抽象模型](../architecture/GAMEPLAY_ABSTRACTION_MODEL.md) |

### 1.3 W 类缺口的定义位置

W 类缺口（W-class gap）的编号规则、字段定义与登记位置**不在本 Spec**。按 D-3 落地后的分工
（2026-10-03 第 3 轮裁定，见 [RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §3 与
[CONTRACT_MATRIX.md](../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md) §12）：**字段定义与具体登记项**的
权威在 [SUPPORT_AND_REJECTION_MATRIX.md](../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md) §6；
**定义、登记与引用规则**在 [Stage 7B+计划](../stage_plans/active/stage-07/plan-b.md#53-w-类缺口记录w-class-gap-record) §5.3（该计划现存正文引用
均显式指向 §5.3）。本 Spec 只做两件事：

1. 声明 W 类缺口不构成 capability，不得据此冻结任何 gameplay 字段；
2. 引用上述两处权威位置，不复制字段表。**本 Spec 不重新定义 W 类缺口的字段，也不设登记表附录**，
   以避免出现第二个权威定义处。

### 1.4 与其它文档的边界

- 一个字段合同只有一个权威 Spec。Gameplay V2 的字段与运行语义以本文件为唯一权威；其它文档
  引用时应链接本文件而不是复制正文。
- 本 Spec 的字段级合同以已接受的第 1 轮裁决为下限与上限：既不放宽，也不加码（见
  [RW §0.2 与 §1](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)）。
- 本 Spec 不拥有物理字段布局或公共头；明确的分批授权优先于早期概括性“未冻结”措辞。
  S7A-2 的整数时间域与 S7A-3 的 [Capsule candidate](GAMEPLAY_CAPSULE_V2_FORMAT.md) 已限定闭合；
  这不授权其它 runtime、Snapshot、Replay 或公共序列化编码，也不冻结新的数值上限。
- 本 Spec 不把研究切片数值写成 ABI 常量或生产限额（计划第 188 行、§S7A-0.3）。

## 2. 版本层级与兼容

### 2.1 唯一合法版本组合

| 字段 | 归属 | 合法取值 | 处置 |
| --- | --- | --- | --- |
| 外层 `version` | `cuexis.chart` | `5` | 唯一接受值；其它值在最早入口稳定拒绝 |
| `gameplay.version` | Chart v5 `gameplay` 区段 | `2` | 唯一接受值 |
| `packedVersion` | Packed 载体 | `1`（candidate） | 保留现有候选值 |
| `candidateRevision` | Packed header | `2`（既有静态 Capsule） / `3`（execution profile） | 2 保留原 round-trip；3 完整执行字段见 Capsule §12；不自动迁移 |
| gameplay revision | Packed header | 显式携带 | 必填；Reader 不得推断 |

**唯一合法组合是外层 `version = 5` 与 `gameplay.version = 2`。** 该组合取代
[FORMAT_BOUNDARY](../proposals/research/gameplay-v2/FORMAT_BOUNDARY.md) §5.1 / §6 中的
`gameplay.version = 1` 草案取值（该研究稿的 §5.1 与 §6 与 PD §4.1 / AM §2 冲突，见 CM-V05）。
被取代的只是版本条款本身；研究稿的其余内容按 `revise` 处置保留，不整篇 supersede
（[RW 第 1 轮 D-4](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)）。

### 2.2 旧 `gameplay.version = 1` 的处置

1. 旧 gameplay `v1` **不在最早入口被接受**；只在**显式离线迁移**时被读取，否则在最早可判定
   入口稳定拒绝（CM-V04，拒绝码 `format.gameplay_version_unsupported`）。
2. Reader 不得通过"`resources` / `factBindings` 等字段是否存在"猜测版本（PD §4.1、AR P0-01）。
3. 迁移器属离线工具范围，归 S7A-8.2；Playback 不做运行期隐式迁移（计划 §S7A-8.2）。
4. 不得重新引入 `semanticRevision` 作为 `gameplay.version` 的同义替代字段（CM-V03、计划 §5.1）。

### 2.3 三种 identity 判定与互换性

`candidateRevision` **不进入** `compiledSemanticIdentity`，只改变 artifact identity；
但它**必须进入** artifact compatibility / interchange 判定。**不可互换的 Packed 产物不得仅凭
`compiledSemanticIdentity` 判等价**（[RW 第 1 轮 Q-01](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)）。

| 判定 | 覆盖 | 不覆盖 |
| --- | --- | --- |
| semantic identity | Canonical Gameplay Graph 与派生闭包的语义投影 | 作者源排版、物理顺序、工具链、载体 |
| artifact identity | 载体的物理 bytes 与物理编码参数 | 语义等价性 |
| interchange / compatibility | 工具链与载体差异：gameplay revision 与 `candidateRevision`、Packed wire revision、entry kind（`packed-chart` / `gameplay-graph`）、section 组集合与顺序、pack 工具链 profile/版本、capability closure 与预算 envelope 的可比性 | 语义判定结论本身 |

### 2.4 兼容矩阵

兼容性分四层，与 CM-X03 一致：**格式层 / 语义层 / 判定层 / 设备层**。向后兼容只在对应层存在
显式声明时成立。互换性至少需要覆盖下列维度的矩阵：

1. **工具链差异**：pack 工具链 profile 与版本；
2. **载体差异**：Packed 与 `gameplay-graph` 之间的同一语义图承载；
3. **wire 差异**：candidate wire revision 与 section 组；
4. **预算差异**：capability closure 与预算 envelope 是否在同一上界内。

> 未决：上述互换性判定的矩阵细则（可互换性判定的最小充分条件）**尚未冻结**，其裁定轮次见
> [RW §1 的 Q-01 行](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)与 §11。本 Spec
> 只登记该矩阵必须存在且必须覆盖工具链与载体差异，不在此冻结矩阵条目。

### 2.5 版本与兼容的稳定失败

1. 未知必需 section、未知 capability、新 requirement kind：一律稳定拒绝，**不得**静默解释成
   tap 或旧语义（AR P0-07、SR R-16）。第 3 轮先冻结了该行为；2026-10-04 的 S7A-3 设计裁决
   又冻结了首次 Packed 消费的具体表示，详见 §3.8.9 与 [Gameplay Capsule v2 裁决](../proposals/gameplay-v2-acceptance/S7A-3_PACKED_W1_CANDIDATE.md)。
2. 未知语义字段一律"未知即拒绝"；CXC `extensions` 只允许命名空间化的 inspection metadata，
   且必须证明不影响判定（CM-X08）。
3. 版本不匹配、identity 不匹配与 capability 不足都必须给出稳定诊断，不得回落到 v4 语义。

## 3. Canonical Gameplay Graph

### 3.1 归属裁决

1. **Chart v5 是 gameplay typed graph 的唯一聚合语义模型**（CM-V13 / P2-01）。
2. Chart v5 JSON `gameplay` 区段是**语义视图**；CXC `gameplay-graph` 是**可选物理 entry**
   （CM-V06 / Q-02）。
3. 所有合法物理入口必须规范化并恢复为**同一** Canonical Gameplay Graph。JSON 区段与 CXC entry
   是可选且**可互相替代**的载体，**不是并存的语义**。
4. 该归属是**语义唯一、物理入口可多**，不是物理入口唯一；若按物理唯一解释，将与
   `gameplay-graph` entry 直接冲突。

### 3.2 图的组成

Canonical Gameplay Graph 是 gameplay 的唯一运行时语义来源，包含：

```text
graphRevision
timebaseRef
rulesetRef
requirements[]
resources[]
groups[]
relations[]
solverProfiles[]
factBindings[]
capabilities[]
sourceMap          (仅诊断)
```

字段语义见 §5 的 projection 表；各字段的内部表示、命名与编码属
[Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) 与实现批次（计划 §S7A-1）。

### 3.3 禁止内容

图中禁止：JSON DOM、CXT AST、未解析参数、对象数组下标、运行时 Entity handle、隐式默认的判定
几何，以及 Requirement 内嵌的 `effects`（PD §4.1、PD §4.2、CM-L04）。

**Q-05（第 3 轮，2026-10-03）。** Requirement **不含** Gameplay `effects`；非空 `effects` 只能**显式
lowering 为不影响 Judgement / Replay 的 Presentation 数据**，否则**拒绝**（语义正文见 §3.8.1，
拒绝面见 §7.2 的 R-18 与 §9.3 的 prepare 原子失败条件）。

### 3.4 规范化与恢复

| 载体 | 规范化要求 | 恢复要求 |
| --- | --- | --- |
| Chart v5 JSON `gameplay` 区段 | 经 typed prepare 编译为 canonical graph | 与 Packed 恢复同一 graph、同一 prepared judgement identity、同一判定结果 |
| Packed Chart entry | 只解码已验证表；不执行 CXT、Pattern、脚本或 solver | 同上 |
| CXC `gameplay-graph` entry | 校验 `gameplay.version`、Ruleset binding、capability closure、resource closure 与 compiled semantic identity | 同上 |

三个入口必须进入同一 typed prepare、Judgement、Ruleset、Snapshot、Replay 与 Presentation
路径，不得形成第二套判定实现（计划 §S7A-7.2、计划第 23-25 行）。round-trip 需要独立的
semantic 比较，**不能只比 hash**（FE §2 硬要求 1）。

### 3.5 图级等价性定义

两条 authoring 路径（Chart v5 inline 与 CXT v2 emission）以及两个物理 entry
（`packed-chart` 与 `gameplay-graph`）的等价定义为：

```text
等价 := prepare 后的 canonical graph 与 derived capability closure 逐字段相等
忽略 := sourceMap 与物理顺序（输入数组顺序、参数顺序、引用顺序、Packed 压缩/字典顺序）
```

逐字段 semantic diff 是 S7A-3 / S7A-7 的 golden 证据；只比较 hash 不构成等价证据
（P1-15、FE §2）。

### 3.6 载体与 entry 的角色

| 载体 / entry | format | 语义角色 | 可 Playback | 7A 状态 |
| --- | --- | --- | --- | --- |
| Chart v5 source | `cuexis.chart` v5 | 作者聚合清单（时间轴、`cxtImports`/`cxtInstances`、`gameplay` 区段、presentation 关联） | 经 typed prepare | 接受（candidate path） |
| CXT v2 source | `cuexis.animation-template` v2 | 作者层模板与有限生成输入 | 否（不是 Playback AST） | 接受为 source input |
| Packed Chart entry | `cuexis.packed-chart` | Canonical Graph 的物理编码 | 是（`playback = true`） | 接受 |
| Canonical Gameplay Graph entry | `cuexis.gameplay-graph`（候选） | 已验证的 Canonical Graph | 是（显式 `playback = true` 时） | 接受（第 6 轮，2026-10-03，`S7A6-R01`）：`entryKind` 为 `gameplay-graph`；manifest closure / 预算 / 拒绝编码的**字段清单**已冻结、**具体数值与编码仍不冻结** |
| `author-source` entry | `cuexis.chart` | 审查、迁移、复现 | **否** | 必须显式非 Playback（第 6 轮确认：**不是** Playback entry） |
| Ruleset package | `cuexis.ruleset` | prepare 输入 | 否（不是第三种 Playback entry） | 第 5 轮已裁定：7A **只用内置 Ruleset**，package 输入命中既有 **R-09**（`S7A5-R05`）；第 6 轮再次确认**不是第三种 entry**（`S7A6-R01`） |

`playback = true` 必须带明确 `entryKind`；Pack、unpack、migrate、prepare 是四个不同操作，
任何一个都不得隐式执行另一个。

**第 6 轮冻结（2026-10-03，`S7A6-R01`；首次消费 S7A-7）。** **入口集合是闭集**：`playback = true` 时
7A **只允许** `packed-chart` 与 `gameplay-graph` 两种 `entryKind`；`author-source` **不是** Playback entry
（只能用于审查 / 迁移 / 复现）；**Ruleset package 不是第三种 Playback entry**（与第 5 轮
`S7A5-R05` 的内置 Ruleset 决定一致——它是 prepare 输入，且 7A 遇到 package 输入直接稳定拒绝）。
**manifest 必须记录七项**：① entry kind、② **compiled semantic identity**、③ artifact identity、
④ Ruleset binding、⑤ **capability closure**、⑥ resource / presentation closure、⑦ `sourceOf`。
其中 **compiled semantic identity 与 capability closure 必须同时携带且可比较**（详见 §5.6 与 §6.4）；
**manifest 的字段清单已冻结，但逐字段的物理编码与预算数值仍不冻结**（属后续序列化批次 / S7A-9）。
逐条裁定与"不得消费"清单见本文 `## S7A-7 限定冻结范围与登记规则（2026-10-03）` 节。

### 3.7 时间基、Tick 与 commit 边界（第 2 轮裁定，2026-10-03）

本节承载第 2 轮（时间域与迟到策略）裁定 1–6 的语义，术语见 §0.1。provenance：thread
`s7a1-freeze-bindings`，verdict `adopt`，confidence 0.99，日期 2026-10-03，模型 `gpt-6-astra`；
七条裁定**全部**登记为**阻塞 S7A-2 的门禁**，共同基线标识见本文 `## S7A-2 限定冻结范围与登记规则（2026-10-03）`。

#### 3.7.1 Tick 宽度、单位来源与溢出

1. `ChartTick`、`judgementTick`、`observationTick`、`commitTick` **均为有符号 64 位整数**。
2. **单位由 `engine.tick.us.v1` 的 `TimebaseProfile` 声明**，不写死在类型名、枚举名或转换函数名里。
3. **禁止隐式单位换算**：边界上不提供"秒 → Tick"或"Beat → Tick"的便捷转换，也不存在隐式零值或默认单位。
4. **溢出稳定拒绝**：任何超出该宽度可表示范围的 Tick 值、差值与位移一律稳定拒绝并给出诊断码，
   **不得**饱和、截断或落入未定义行为。

#### 3.7.2 RationalBeat → `judgementTick` 的精确映射与 tie rule（Q-04 / CM-T03；2026-10-03 实现复核修订）

1. 映射使用**精确有理数运算**（prepare 期一次性完成），**不使用浮点、不使用宿主数学库**。
2. 舍入规则：**四舍五入到最近整数，半数取偶**；`roundHalfToEven` 的取值**对负值对称**
   （`roundHalfToEven(-x) = -roundHalfToEven(x)`）。该对称性是**舍入函数本身**的性质，**不是**整个
   Beat → Tick 映射关于 `originBeat` 的**奇对称**：存在 `stop` 时后者**不成立**（见第 3 条）。
3. **tempo、stop 与负 Beat 使用同一精确映射，不设例外路径。** 映射是定义在有理数上的**精确累计
   函数** `F`：
   - 定义 `F(originBeat) = 0`。`stop` 在 `[startBeat, endBeat)` 内使**累计速率为零**（该区间内映射
     冻结在同一累计值），并在 `endBeat` 处把**声明的 `duration` 加入精确累计值**，即
     `F(endBeat) = F(startBeat) + duration`；
   - 最终 **`tick(beat) = roundHalfToEven(F(beat))`**：**全程只舍入一次**，舍入发生在精确累计域上；
   - **不得**解释为整数 Tick 上的递推 `tick(endBeat) = tick(startBeat) + duration`：该式先对
     `tick(startBeat)` 取整、再加 `duration`，会引入**第二次舍入**，与"同一精确积分、同一舍入"
     冲突。反例：`tempo = 1/1`、`originBeat = 0`、`stop = [1/2, 3)`、`duration = 3` 时
     `tick(1/2) = roundHalfToEven(1/2) = 0`（半数取偶），而 `tick(3) = roundHalfToEven(1/2 + 3)
     = roundHalfToEven(7/2) = 4`；整数递推给出 `0 + 3 = 3`，**错误**；
   - `tempo` 变化只改变累计速率（即 Beat → Tick 的比例），**不改变**舍入与 tie rule；`stop` 同样
     **不改变**舍入与 tie rule；
   - **`stop` 区间内映射非单射**：`[startBeat, endBeat)` 内多个 Beat 落到同一 Tick（区间内累计值
     冻结）。这是本模型的**固有性质**，不是实现缺陷；
   - **半开跳变方向**：正向（`beat > originBeat`）的累计区间是 `(originBeat, beat]`，负向
     （`beat < originBeat`）是 `(beat, originBeat]`；`endBeat` 落在该区间内的 `stop` 才计入该侧。
     因此**镜像 `stop` 配置下映射不再关于 `originBeat` 奇对称**（例：`stop = [0,2)` 时长 4 时
     `tick(1) = 0`，而镜像的 `stop = [-2,0)` 时长 4 时 `tick(-1) = -4 ≠ -tick(1)`）；
     **有向端点关系与单调性在两侧仍成立**（映射仍非递减、每个 `stop` 的有向端点等式仍成立）。
4. **同 Tick 碰撞按规范键 `(tick, originKind, canonicalOrdinal)` 排序**；`canonicalOrdinal` 由 canonical
   窗口 / 观测 identity 派生。**禁止按 ingress 顺序排序**，也禁止按容器顺序或线程完成顺序排序。
5. **上述映射语义属于 S7A-2 必须冻结的部分。** 精确累计函数 `F`、`stop` 塌缩、非单射与半开跳变方向
   决定**同 Tick 碰撞**（第 4 条的规范键排序）、**late-policy 窗口**与 **`commitTick`** 行为，因此
   **不因**"映射的代码形状 / 参考实现 / golden 文件属实现批次"而推迟冻结。
6. **Tick → Beat 反查契约（登记为阻塞项）。** 塌缩模型下 Beat → Tick **非单射**，因此**任何
   Tick → Beat 反查不得返回单一 Beat**：它必须返回**区间 / 集合**，或对歧义**稳定拒绝**。该契约登记为
   **首次消费 Tick → Beat 反查的后续批次**的阻塞项（`S7A-3` / `S7A-4` / `S7A-5` 均不消费它；见 §3.7.7 表）。

#### 3.7.3 `observationTick` 的唯一来源与 S7C-1 边界（Q-04 / CM-T04）

1. `observationTick` 的**唯一 canonical 来源**是**输入进入判定管线时捕获的校准后会话时钟
   （calibrated session clock）**。
2. 设备时间、宿主到达时间、音频时间与渲染帧时间**只保留为诊断上下文**，不参与判定时间定位，
   也不构成第二套时间语义（与 ABI「时间单位与表达」的 tick / 原始时间戳分离一致）。
3. **S7C-1 只能扩展 `CalibrationProfile` 参数**（校准模型、参数集与测量口径），**不得替换 canonical
   source**，也不得让未校准的原始时间成为 `observationTick`。
4. **负值合法（2026-10-03 输入半批复核）。** `observationTick` 在**有符号 64 位域内取负值是合法的**，
   并**照常参与单调排序**；**只有校准会话时钟回退（calibrated session clock regression）被稳定拒绝**。
   该回退属**时间次序关系错误**，映射 `invalid_relation`，**不是** `budget_exceeded`；**负值本身不触发
   任何拒绝码**。
5. **不可表示的 calibrated clock 值复用既有 tick 域表示失败。** 校准后的会话时钟值若**无法在有符号 64
   位 tick 域内表示**，按 §3.7.8 表的 Tick 溢出行处置（类别 `budget_exceeded`），**不保留**任何独立的
   7A 拒绝令牌。入口若**违反"先捕获校准会话时钟"的顺序**（先规范化、后捕获），只走 §9.3 的既有
   `invalid_relation` 原子失败路径，同样**不保留**独立令牌。

#### 3.7.4 迟到策略的 typed 参数化（CM-T08 / P1-04）

1. `finalizationWatermark`、queue hop、窗口 open/close 与重复排队条件**均为 typed 参数**，由
   `TimebaseProfile` / ruleset 提供；它们的类型与角色在本批次冻结，**数值不在本批次冻结**。
2. 默认值**只在 profile registry 登记为 `pending_measurement`**；**不得**成为 ABI 常量、隐式零值或
   研究切片限额（§8.3）。
3. 缺失或未测量的参数值在 **prepare 稳定拒绝**（诊断类别 `late_policy_incomplete`）。
4. **重复排队稳定拒绝并返回诊断码**（诊断类别 `late_policy_incomplete`）；重复检测的判据是 canonical
   观测 identity，不是 ingress 序号或到达顺序。
5. **`validatePrepare` 只检查"已声明且已测量"，不检查参数量级关系。** 它只验证 late-policy 参数**已声明
   且已测量**（第 3 条），**不检查** `finalizationWatermark` / queue hop / 窗口开闭阈值之间的**量级
   关系**（序关系、上界与相互一致性），因为那些数值尚不属本批次。**"参数量级关系的校验"登记为阻塞
   S7A-4 的 late-window / 判定消费门禁**：S7A-4 在按 late policy 消费窗口与判定之前必须补齐该校验。
   **具体实测数值与最终限额**仍由 **S7A-9 的硬化与测量门禁**承载；上述两项都不阻塞 S7A-2 的语义冻结。
   2026-10-05 的完整选定关系、边界与准入算法见 [execution profile §3](gameplay-v2-execution-profile.md)；
   后续实现应增加该执行 gate，不能将旧 helper 的历史验证结果冒充新增 gate 已通过。

#### 3.7.5 `commitTick` 与 commit window（P2-03）

1. `commitTick` 是**事务提交发生的 Tick**。
2. commit window 是**相邻两个可提交 Tick 之间的观察区间**。
3. **事实只能在 `commitTick` 提交**；窗口内累计的候选、信号与未提交状态不构成事实，也不进入
   Fact Ledger 或 Replay / Snapshot 语义输入（§10）。

#### 3.7.6 InputDomain 的 `AmountSpec` 与越界处置（CM-T13，第 2 轮新增）

1. 每个 `InputDomain` **必须携带 typed `AmountSpec`**，声明 canonical **整数量化**、**scale**、
   **可表示范围**与**边界策略**。
2. 量化使用**精确整数运算**，**最近值半数取偶**（与 §3.7.2 同一规则，含负值对称）。
3. **超范围、窄化与溢出稳定拒绝并返回诊断码**，不得饱和、不得截断、不得回落成默认量。
4. **本轮不冻结具体业务量程数值或限额**；量程由真实内容测量后单独接受（§8.3、§8.4）。
5. **canonical 整数宽度明文冻结（2026-10-03 输入半批复核，CM-T13）。** `AmountSpec` 的 canonical 整数表示为有符号 64 位整数；该宽度是 S7A-2 冻结的规范表示，并与 Tick 的有符号 64 位域一致；任何无法在该域内精确表示的量化、乘法或窄化稳定拒绝。
   该宽度**必须明文冻结**，**不得**靠"复用 §3.7.1 的 Tick 宽度"默示。
6. **判定顺序：先验证 canonical 可表示性，再验证声明范围与边界策略（2026-10-03 输入半批复核）。**
   先判断该量能否在**有符号 64 位 canonical 整数域**内**精确**表示，再判断它是否落在 `AmountSpec`
   声明的 `minimum` / `maximum` 内并满足 `boundaryPolicy`：
   - **可表示但越界** → `judgement.s7a2.input.amount_out_of_range`，类别 `budget_exceeded`；
   - **根本不可表示**（分子或分母乘积溢出、无法执行精确量化）→
     `judgement.s7a2.input.amount_narrowed`，类别 `budget_exceeded`；
   - **`exactNumerator == INT64_MIN` 按"不可表示"拒绝**：`|INT64_MIN|` 在**有符号 64 位**内**无表示**，
     因此精确域内的半数取偶（round-half-to-even）在该点不可执行，报 `amount_narrowed`。
   顺序**不得**颠倒：把不可表示报成"越界"会把它误报为声明范围内的值问题。

#### 3.7.7 本批次不冻结的部分

以下事项**不在第 2 轮冻结**，并按登记进入后续批次：

| 事项 | 处置 | 阻塞批次 |
| --- | --- | --- |
| 具体 late-policy 数值（watermark、queue hop、窗口开闭阈值） | 只登记为 `pending_measurement`；提供具体值者稳定拒绝 | 后续批次 |
| **late-policy 参数量级关系**（`finalizationWatermark` / queue hop / 窗口开闭阈值之间的序关系与一致性） | `validatePrepare` **只**检查"已声明且已测量"，**不校验**量级关系；把该校验登记为 **late-window / 判定消费门禁**（§3.7.4 第 5 条） | **S7A-4**（late-window / 判定消费）；最终数值与限额记 **S7A-9** |
| **Tick → Beat 反查契约**（塌缩模型下不得返回单一 Beat：返回区间 / 集合或稳定拒绝歧义） | 只登记契约为阻塞项，不在本批次冻结实现（§3.7.2 第 6 条） | **首次消费 Tick → Beat 反查的后续批次**（`S7A-3` / `S7A-4` / `S7A-5` 均不消费它） |
| 具体业务量程与限额（`AmountSpec` 的取值数值） | 只冻结类型与越界处置；数值由测量后单独接受 | 后续批次 |
| S7C-1 的 `CalibrationProfile` 扩展字段 | 只冻结"可扩展参数、不可替换 canonical source"的边界 | S7C-1（后续批次） |
| 连续输入能力（连续轨迹 / Slider、`minimumReportRate`、reconstruction） | 7A 稳定拒绝，**复用既有 ABI 冻结码** `input.continuous_unsupported`（类别 `capability_disabled`，`capabilityId = input.trajectory.v1`，`remediation = S7B-1`），以 `field.path = continuityCapability` 指认；映射到既有 **R-05**，**不新增 ABI 码、不新增 R 条目**；能力准入属 S7B-1 | 后续批次（S7B-1） |
| **不连续表示**（跨 gap / 重连 / 丢样，`DiscontinuityFlag`） | 7A 稳定拒绝，**复用同一** `input.continuous_unsupported`（类别 / `capabilityId` / `remediation` 同上），以 `field.path = discontinuity` 与连续能力区分；映射到既有 **R-05**，**不新增 ABI 码、不新增 R 条目** | 后续批次（S7B-1） |
| 所有预算数值、上限与限额 | S7A-9 前保持未冻结 | S7A-9 |

#### 3.7.8 稳定拒绝的登记方式（本批次不新增 R 条目）

本节新增的六类稳定拒绝场景**全部映射到既有条目**，§7.2 仍为 **19 条**（判定过程见
[第 2 轮门禁报告](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-2-gate-rulings.md)；三分法见 §9.3）：

| 新增拒绝场景 | 映射到的既有条目 | 候选拒绝码 / 类别 |
| --- | --- | --- |
| Tick 溢出（宽度越界、差值与位移溢出） | §9.2 类别 `budget_exceeded` + §9.3 的"越界即原子失败"路径 | `budget_exceeded`（稳定诊断；不饱和、不截断） |
| `DomainAmount` 量越界 / 窄化 / 量化溢出（§3.7.6） | 同上（§9.2 `budget_exceeded` + §9.3 越界路径）；R-01…R-19 不覆盖输入量项，故不进拒绝面 | `budget_exceeded`（稳定诊断码） |
| late-policy 参数缺失或未测量（§3.7.4） | §9.2 类别 `late_policy_incomplete` + §9.3 prepare 原子失败条件（"late policy 缺少 `finalizationWatermark`、queue hop 或 snapshot 规则"） | `late_policy_incomplete` |
| 重复排队（§3.7.4） | §9.2 类别 `late_policy_incomplete` + §9.3 同一原子失败条件 + §5.2 条件行（能改变 Fact Ledger 内容或顺序者进 judgement identity） | `late_policy_incomplete`（稳定诊断码） |
| **声明结构不完整**（缺 `profileId`、缺 `unitToken`（含空值）、未声明 `initialTempo`）（§9.3 三分法第 1 类） | §9.2 类别 `invalid_relation` + §9.3 的 prepare 原子失败条件（声明结构 / 次序非法） | `invalid_relation`（稳定诊断；**不是**能力拒绝） |
| **已声明但声明域内数值非法**（非正 `tickScale` / tempo / `stop` duration）（§9.3 三分法第 2 类） | §9.2 类别 `budget_exceeded` + §9.3 越界路径（声明完整、值不可满足） | `budget_exceeded`（稳定诊断码） |

**输入规范化半批的三条原子失败映射（2026-10-03 实现复核，`adopt` 0.97 / 0.99）。** 下列三条是
**对既有六类场景的精确归属补写**，不是新增类别、不新增 R 条目、不新增 ABI 码；它们与连续 / 不连续
表示的复用（§3.7.7 表）一并写入 §9.3：

| 输入半批拒绝场景 | 映射到的既有条目 | 类别 / 拒绝码 |
| --- | --- | --- |
| **同一 canonical 身份在同一 Tick 再次入场**（`judgement.s7a2.input.same_tick_collision`，§3.7.4 第 4 条的 canonical 观测 identity 判据） | §9.3 的 prepare 原子失败条件（**输入身份 / 次序关系非法**） | `invalid_relation`（**不是** `late_policy_incomplete`） |
| **`AmountSpec` 窄化 / 不可表示**（`judgement.s7a2.input.amount_narrowed`，§3.7.6 第 6 条） | §9.2 类别 `budget_exceeded` + §9.3 越界路径；**判定顺序上先于越界** | `budget_exceeded` |
| **校准会话时钟回退**（`judgement.s7a2.timebase.time_reversal`，§3.7.3 第 4 条） | §9.3 的 prepare 原子失败条件（**时间次序关系错误**） | `invalid_relation`（**不是** `budget_exceeded`） |

**同一 canonical 身份在同一 Tick 再次入场属于输入身份 / 次序关系非法**，稳定拒绝并映射
`invalid_relation`。它与**迟到策略的重复排队**必须分开：后者（§3.7.4 第 4 条）判据是迟到队列中
**重复的 canonical 观测 identity**、属**策略状态**问题，映射 `late_policy_incomplete`；二者
**不得**互相顶替。**负 `observationTick` 不在此表**（§3.7.3 第 4 条：负值合法、参与单调排序，只有
**回退**被拒）；**不可表示的 calibrated clock 值**同样不在此新增行内，它按上表 Tick 溢出行复用
`budget_exceeded`（§3.7.3 第 5 条，**不保留**独立令牌）。

**注意区分"拒绝面"与"越界处置"。** R-01…R-19 是**能力级**拒绝面（能力、禁止项与迁移歧义），
上述六类都不在其中；它们是**已声明范围 / 已声明能力内的值或参数不可满足**（第 5、6 行则是**声明结构
不完整**），因此按 §9.2 的类别与 §9.3 的 prepare 原子失败条件处置，并对重复排队附加 §5.2 的 identity
约束。**若把越界、量化失败或结构不完整记成 R 条目，等于把它误报为"能力被拒绝"**，会与 §7.1 支持集合与
capability 派生链冲突；这也是本节选择映射而非新增的原因。R 清单的条数因此**保持 19 条**，§9.2 的类别
**保持九类**。**输入半批的三条原子失败映射同样不新增条目**：`same_tick_collision` / `time_reversal` 走
既有 `invalid_relation`，`amount_narrowed` 走既有 `budget_exceeded`，连续 / 不连续表示复用既有 R-05 的
`input.continuous_unsupported`；**不新增 ABI 码、不新增 R 条目、不新增第十类**。

### 3.8 prepare、装配与 entry 的 7A 边界（第 3 轮裁定，2026-10-03）

本节承载第 3 轮（prepare、装配与 entry；进入 S7A-3 前）裁定的语义。轮次总览与逐条登记见
[RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §3 与 §9；provenance：thread
`s7a3-prepare-rulings`（模型 `gpt-6-astra`，consult），三卡 verdict `need_info` / `reject` / `adopt`，
confidence 0.8 / 0.88 / 0.99，文件名时间戳为 2026-10-02T19:4xZ（UTC）、统一按**本地日期 2026-10-03** 登记；
带日期证据见 [第 3 轮门禁报告](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-3-gate-rulings.md)。共同基线标识
见本文 `## S7A-3 限定冻结范围与登记规则（2026-10-03）` 一节。

#### 3.8.1 Requirement 不含 Gameplay `effects`（Q-05）

1. `RequirementRecord` **不含** Gameplay `effects`；canonical graph 禁止 Requirement 内嵌 `effects`（§3.3）。
2. 非空 `effects` **只能显式 lowering 为 Presentation 数据**，且该投影必须证明**不影响 Judgement 与
   Replay**：lowering 只进 presentation closure，不改变 Fact Ledger 的内容或顺序，也不改变 judgement
   identity（§5.2、§6.5）。
3. 无法给出该 lowering 证明的非空 `effects` 一律**拒绝**：不得静默丢弃、不得当作判定输入、不得回落旧语义。

#### 3.8.2 `REQ0` / `CNS0` 的兼容边界（Q-07）

1. Packed `REQ0` / `CNS0` 保持 **Foundation 语义**；本阶段不重新解释其既有含义。
2. 7A 新语义必须使用**可与旧语义区分的 entry / section 边界**；旧 Reader 不得把新语义读成旧 kind。
3. 未知必需 section、未知 capability 与新 requirement kind **稳定拒绝**（§2.5、§7.2 的 R-16）。
4. 第 3 轮原先只冻结行为；该遗留已由 2026-10-04 S7A-3 设计裁决闭合：首次 Packed 消费采用
   `candidateRevision = 2` 与 `GPH0/GPR0/GPD0/GRC0/REF0` 必需 semantic section 组，字段布局、
   长度编码、端序、canonical order 与 REF0 kind 8--13 见 §3.8.9 及其裁决记录。实现仍须通过
   golden、负例和跨工具链验证，未通过前不得声称实现完成。

#### 3.8.3 Release / tail 的显式声明（Q-18 / P2-10）

1. Release / tail **仅作为同一 Requirement 显式声明的可选 phase**，**不自动生成独立 Requirement**（CM-R06）。
2. 内容要求 tail 语义却**未显式声明** phase 时**拒绝**；不得按隐式 legacy profile 推断（CM-R07）。
3. `P2-10` 是 `Q-18` 的重述，随 `Q-18` 关闭，**不另设选择**。

#### 3.8.4 有界循环：完全展开，不接受 `boundedRelationInstance`（CM-C10 / P1-03）

1. 7A 的 prepare-time 有界循环（`bounded repeat`）**完全展开**，展开序由 prepare 唯一确定。
2. **展开结果唯一决定 identity / snapshot / fact order**；同一内容不得有第二种 identity 或第二种 fact order。
3. `boundedRelationInstance` **不进 7A 合同**：遇到该表示**稳定拒绝**。它登记为 **7B+ / S7C 的候选**，
   只阻塞**首次拟消费它的后续批次**，**不阻塞 S7A-3**；其具体 `stableRejectCode` 与新的 wire 表示一并
   在该候选表示首次被消费的后续批次前闭合；S7A-3 不消费该表示，也不为它冻结新的 wire（§3.8.2 第 4 条）。
4. 运行期 repeat 计数、累积器与动态 Requirement 生成不在 7A（§7.2 的 R-14）。
5. **份数序与接受集**（S7A-3 实现批次登记，2026-10-03）：完全展开的**份数序**唯一确定为**递增的份数
   下标**；接受集是可允许份数上的**并集**。**不要求每份消费元素**：操作数能匹配空轨迹时该循环仍
   **完全展开**，**不得**因此稳定拒绝；展开在位置集**不动点**处终止，不按声明上界逐份行走。
6. **静态上界**（同上）：`maximum` 是静态声明记录，**不产生**运行期计数器、累积器或动态
   Requirement（第 4 条）。取最大可表示值时同样终止；此时其完全展开计数记为**测量缺口**（§8.2 第 1
   项），**不单独构成**拒绝。该计数仍**是经证明的下界**（受检算术溢出 ⇒ 真值 ≥ 2^64），故**存在已接受
   上限**时按 §8.2 界分第 2 句以 `budget_exceeded` 拒绝；**无**已接受上限时**不比较、不拒绝**（第 3
   轮复审改正，2026-10-03）。
7. **实现验收风险（第 2 轮实现复审，2026-10-03；语义已决定）。** 声明的身份投影目前仍按**原始** `repeat`
   上下界写出，而本条要求的是**完全展开**语义：**装配在消费该投影之前必须证明**快照与事实序与第 1、2
   条的完全展开一致（份数序、并集语义、不动点语义），否则投影与实际语言不符。该风险**不改变** §7.2 的
   19 条拒绝面，也**不**授权在此前消费该投影。

#### 3.8.5 S7A-3 不得消费清单（实现须逐条对齐）

按 plan §S7A-3 的验证要求与 S1-05 的"不得用默认值、临时 typedef、序列化编码或伪成功实现绕过阻塞"
口径，S7A-3 实现**不得消费**：

1. 旧 revision 的隐式 V2 解释；
2. 旧 `REQ0` / `CNS0` 对新 kind 的兼容解释；
3. 未定的新 wire 布局或编号；已授权的 §3.8.9 Gameplay Capsule v2 candidate 静态合同不属于“未定 wire”，
   但该例外不扩展到 runtime resource state、Snapshot/Replay、FactId/CommitId 或公共 proof codec；
4. Gameplay `effects`（未显式 lowering 为不影响判定的 Presentation 数据者）；
5. 隐式 Release / tail；
6. `boundedRelationInstance`；
7. 运行期 repeat 计数或动态 Requirement；
8. `sameContact`；
9. 连续轨迹；
10. 跨 Requirement relation / resource migration；
11. handoff Hold；
12. 接触跟随 Slider；
13. 未闭合的数值限额、默认枚举、临时整数 typedef 与公共序列化编码。

该清单同时写入 [Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) 的 `## S7A-3 限定冻结范围与登记规则（2026-10-03）`
一节与本 Spec §7.2，供实现逐条对齐。

#### 3.8.6 REF0 / manifest / 诊断 source map 归属表（P1-14，第 5 轮裁定，2026-10-03，`S7A5-R11`）

本节承载第 5 轮（Ruleset、事实序、Score 与 Snapshot；进入 S7A-5 / S7A-6 前）裁定中的 `P1-14`，作为
**附录**冻结 resource closure 与 Packed reference closure 的归属规则与**具体归属矩阵**。provenance：同一
Codex 话题 thread `01a0fe49-cfe5-7933-8d03-3d0a65d34c8b`（consult），主裁定卡 verdict `adopt`、confidence
0.95，处置词与计数确认卡 verdict `adopt`、confidence 0.99，ABI 状态格首词与追踪计数追问卡 verdict
`adopt`、confidence 0.98；文件名时间戳为 2026-10-02T20:2x–20:3xZ（UTC），统一按**本地日期 2026-10-03**
登记（与第 2、3、4 轮先例一致）。逐条登记见
[RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §5 与 §9；带日期证据见
[第 5 轮门禁报告](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-5-6-gate-rulings.md)。

**归属规则（冻结）。**

1. 任一 gameplay 引用必须**唯一**归入下列三类之一，**不得跨类、不得同时登记**：
   - **REF0 / judgement closure**：**判定必需**的引用——缺失或改变会改变 Fact、Score、Combo、Statistics、
     Fact 顺序、identity 或拒绝语义；
   - **manifest / presentation closure**：**纯表现**引用——只影响渲染与资源解析，不改变判定；缺它允许
     headless judgement 继续；
   - **诊断 / source map**：只服务诊断、编辑器与迁移定位，不进入任何 closure，也不参与 identity。
2. 判定必需引用**必须**进 REF0（及对应的 judgement / semantic closure）；**不得**只进 manifest，也**不得**
   只留在 source map。
3. 纯表现引用**只进** manifest / presentation closure，**不得**进 REF0：把表现依赖放进 REF0 会破坏
   "判定闭包可独立校验"（§6.5、§6.6）。
4. `sourceMap` **仅诊断**：它不进入 judgement identity，也不是 REF0 的内容（§4 的 `sourceMap` 行与 §5.2
   同属这一口径）。
5. 归属**由 prepare 判定**并随 canonical graph 一并确定：同一输入必须得到**同一**归属（确定性），
   不得由宿主、表现层或 Session 事后改变。
6. 归属无法唯一确定或出现跨类引用时**拒绝**：按 §9.3 的 prepare 原子失败条件与 §9.2 的既有类别处置，
   **不新增 R 编号**（§7.2 仍 19 条）。

**归属矩阵（冻结）。**

| 引用 / 内容 | 归属 | 依据 / 说明 |
| --- | --- | --- |
| Requirement / Pattern / Measure / resource / claimKey / emission 的判定引用 | **REF0 / judgement closure** | 缺失或改变即改变判定（§4、§5.2） |
| Requirement identity 六元组与 `compiledSemanticIdentity` 的判定投影 | **REF0 / judgement closure** | identity 变更即判定变更（§2.3、§4） |
| timebase / late policy / normalization profile 的判定投影 | **REF0 / judgement closure** | 改变同 Tick 映射或迟到处置（§3.7） |
| prepared solver profile 与 coordination policy 的判定投影 | **REF0 / judgement closure** | 改变候选排序与资源提交（§3.12） |
| FactBinding 的**判定侧存在性**引用 | **REF0 / judgement closure**（仅存在性与判定域判定） | 判定闭包内悬空必须失败；绑定细节属表现侧 |
| Presentation / Effect graph、材质、Shader、动画模板、音频与表现资源 bytes | **manifest / presentation closure** | 纯表现；缺它允许 headless judgement（第 6 轮 `Q-15` 细化） |
| `render.visible` 等表现 override、UI 文案与默认绑定 | **manifest / presentation closure** | 不进入 judgement identity（§5.2） |
| `sourceMap`、旧字段路径、Studio 节点与模块定位 | **诊断 / source map** | 只服务诊断 / 编辑器 / 迁移（§4） |

**落点与首次消费。** 本表是 `P1-14` 的裁决落点（**Spec 附录**，即本节），并在 §S7A-3 节与本 Spec §11 引用。
**REF0 的物理字段、编号与编码**由 **S7A-3 的首次 Packed 写入**消费——本节只拥有**归属规则与矩阵**，
具体 candidate 表示已由 [Capsule §4](GAMEPLAY_CAPSULE_V2_FORMAT.md) 闭合；**S7A-7** 负责 entry / manifest 集成。`P1-14` **不阻塞 S7A-5 / S7A-6**；ABI 侧只有
**指向性引用**、**不新增类型行**（见 [Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) §未决项 与 §S7A-5 节）。

#### 3.8.7 S7A-3 准入门禁：判定域、CXT 合并与两路等价（第 7 轮裁定，2026-10-03）

本节承载第 7 轮（收尾澄清与缺陷）裁定中的 `P1-01`（`S7A7-R01`）、`P1-02`（`S7A7-R02`）与 `P1-15`
（`S7A7-R06`）——三项的**共同首次消费批次均为 S7A-3**，因此统一登记为 **S7A-3 的准入门禁**。轮次总览与
逐条登记见 [RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §7、§8 与 §9；
provenance：Codex 话题 thread `01a0fe61-b7a3-7853-a380-0793fee14e14`（consult，模型 `gpt-6-astra`，
verdict `adopt`、confidence **0.96**）与口径确认卡 thread `01a0fe61-3476-7882-9674-5f6b05035237`
（verdict `adopt`、confidence **0.99**），文件名时间戳为 2026-10-02T20:52:37.188Z 与
2026-10-02T21:08:34.383Z（UTC），统一按**本地日期 2026-10-03** 登记；带日期证据见
[第 7 轮收尾裁定与缺陷处置记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-7-wrapup-rulings.md)。
**以下三组文本是准入门禁的规范文本**（与工作表"建议处置"列的给定文本一致）。

**判定域的 typed contract（`P1-01`，冻结）。**

1. 7A **只接受静态 typed 判定域记录**：判定域是 canonical graph 的一部分，随 prepare 一并确定。
2. **几何属 Gameplay closure**：判定几何（判定区、窗口、边界、量程）进 judgement closure / REF0
   （§3.8.6、§6.5），**不进** presentation closure。
3. **坐标系统一在判定域内声明**：判定域自带坐标系与量程声明，**不得**依赖宿主视口、渲染分辨率或
   表现层 transform。
4. **与 Presentation transform 完全隔离**：判定**不得继承、不得依赖**任何 Presentation transform；
   改表现层 transform 不改变 Fact、Score、Combo、Statistics 或 identity。
5. **未来动态 frame**（随视口 / 相机动态变化的判定帧）属 **7B+ 候选**，7A **稳定拒绝**（§7.2 的既有
   拒绝面，不新增 R 条目）。

**CXT local relation 的全局合并规则（`P1-02`，冻结）。**

1. **合并在 prepare 编译期完成**；运行期不做 relation 合并。
2. **稳定 ID = 来源文档 identity + 声明序号**：跨文档唯一，与物理顺序无关。
3. **同名不合并而是拒绝**：两个来源声明同名 relation 时**稳定拒绝**（不得静默合并、不得后者覆盖前者）。
4. **跨 invocation 引用必须显式**：隐式跨 invocation 解析**拒绝**。
5. **重排不改变合并结果**：合并按稳定 ID 排序，输入数组顺序、参数顺序与引用顺序的置换不改变产出。

**Chart v5 inline 与 CXT v2 emission 的等价性（`P1-15`，冻结）。**

1. **等价 = prepare 后 canonical graph 与 derived capability closure 逐字段相等**。
2. **忽略** `sourceMap` 与**物理顺序**（输入数组顺序、参数顺序、引用顺序、Packed 压缩 / 字典顺序）。
3. **逐字段 semantic diff 作为 S7A-3 的 golden 证据**：两条 authoring 路径（Chart v5 inline 与 CXT v2
   emission）必须互相校验，diff 必须为空（§3.5 的图级等价性、§3.8.6 的归属规则同时适用）。
4. 本项**不改变** §7.2 的 19 条拒绝面，也**不新增** R 条目或 ABI 类型行。

#### 3.8.8 Pattern 原语语义与包含性门禁状态（S7A-3 实现批次登记，2026-10-03；第 2、3 轮复审改正，2026-10-03）

本节登记 S7A-3 实现批次（第一、二半）在第一轮实现复核中裁定的**语义澄清**与**门禁状态变更**。
provenance：Codex 话题 thread `s7a3b-implementation-review`，verdict `reject`、confidence 0.94，日期
2026-10-03，模型 `gpt-6-astra`；**第 2 轮实现复审**（同一话题 thread
`01a0fe91-416c-7f31-a48a-d1890a0e68d1`，verdict `reject`、confidence **0.91**，裁决卡时间戳
2026-10-02T22-33-54-872Z，模型 `gpt-6-astra`）改正了第 5、6 条，并改正了 §8.2 的计数与拒绝判据；
**第 3 轮实现复审**（同一 thread `01a0fe91-416c-7f31-a48a-d1890a0e68d1`，Codex 会话
`01a1006a-d7ca-7300-b6d9-67dedf427789`，verdict `reject`、confidence **0.93**，裁决卡时间戳
`2026-10-03T06-20-24-581Z`，模型 `gpt-6-astra`）改正了第 5 条并重申第 8 条：**可证明下界超过
已接受上限**恢复为 `budget_exceeded` 内容拒绝，**仅**构造失败与**非下界**计数记为测量缺口；
第 3 条的包含性门禁**仍 incomplete** 且其表示形式不变。逐条处置与落地证据见
[S7A-3 实施裁定记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-3-implementation-rulings.md)。
本节的登记**不新增** ABI 类型、诊断类别、R 条目或契约计数（§7.2 仍 19 条、§9.2 仍九类、契约处置词不变）。

1. **`skip` 与 `instant` 语言相同、身份不同**：两者都接受空轨迹且不消费元素，语言完全一致；区别只在
   **声明身份**（两者不互相归一化，各自是独立的 interned 子模式）。因此"语言相同"**不得**被读成
   "同一子模式"，也**不得**据此合并二者的 identity 投影。
2. **`complement` 是前缀消费的、可组合的**：`complement(X)` 接受**所有操作数都到达不了的前缀**；
   `sequence(complement(X), Y)` 保留每一个切分点，不是一个把余下轨迹整体吞掉的后缀 / 结尾锚定读法。
   补的接受集可以无有限最长匹配，此时"无有限上界可用（未测出有限上界）"与"已证明无界"是两件事，
   报告里**只**能写前者。
3. **arm / deadline 包含性门禁状态：显式状态并已接入装配。** S7A-3 的 `Pattern` 是否能被声明的
   `maxArmElements` / `maxDeadlineElements` 容纳由 `ContainmentStatus` 表示：`contained` 表示已证明包含，
   `gateIncomplete` 表示缺席、待测或无法取得有限最长长度；后者不是内容超限诊断。门禁检查**全部可接受长度**
   （最短与最长），不得只读最短匹配；已测容量被证明超出时才使用既有码 `arm_bound_exceeded`。
   `gateIncomplete` 在装配路径中被显式、原子地拒绝，不发布半成品，也不改变既有 active publication。
   **与预算缺口仍是两道独立门禁。** 预算维度的测量缺口不构成本条的拒绝理由，本条的不可判定状态也不占用预算
   维度 path；状态预算门禁仍按 §8.2 保持 incomplete，直至后续批次闭合。
4. **grade 的携带与求值分界**：编译后的 Measure **只携带**声明里写下的 grade token（不透明内容），
   **不求值** grade——不套默认、不做聚合、不把缺席升级为某个等级。grade 的**求值**不在本批次
   （grade 标度与聚合规则见 §3.20 / CM-S10 / P1-07，仍未接受默认值）。
5. **计数缺口不是拒绝理由（第 2 轮复审改正；第 3 轮复审改正）。** 完全展开计数、每分支上界与 §8.2 状态数
   **都没有可表示值**时记为**测量缺口**（访问器缺席）：缺口本身**不得**构成任何内容拒绝，**不得**用别的计数
   顶替，也**不得**被读成"已验证不超限"。但这**不**等于"无从比较"：该维度的**已测得计数**、或其**可证明
   下界**（精确受检算术与单调累加的溢出 ⇒ 真值 ≥ 2^64；最长可接受轨迹长度 `L` 有限 ⇒ 状态数 ≥ `L + 1`）
   超过**已接受**上限时，按 §8.2 界分第 2 句以该维度既有 path 与既有类别 `budget_exceeded` **拒绝**；无已接受
   上限时**不比较、不拒绝**。状态预算门禁因此**保持 incomplete**（既不是"已验证"，也不是"已拒绝"）。内部构造
   （测量工作集）上界**只**是本模块的测量能力上界，**不得**充当内容阈值。
6. **包含性门禁的装配消费点：已落地，仍需随整批 S7A-3 收口核验。** 装配路径已使用需求级的真实 arm /
   deadline 容量调用该门禁，并把 `gateIncomplete` 转为原子 prepare 拒绝；已测超限仍走既有
   `arm_bound_exceeded`。缺席 / 待测容量不会生成可发布身份，既有 active publication 在失败时保持不变。
   该实现满足进入后续批次的包含性硬前置，但不代表 S7A-3 整批完成；第三、第四部分及其对应证据仍须闭合。
7. **码表登记不得永久豁免（第 2 轮复审，耐久规则）。** **S7A-3 的 src-only 前缀不得被判为永久豁免**：
   任何码在获得**公共映射**（ABI 文本、schema、trace / 诊断的对外承诺）之前，必须由引入它的批次**逐条
   登记**（码、类别、severity / faulted、path、首次消费批次）并在集中码表校验器 CTest
   `cuexis_gameplay_diagnostics_codes` 中**通过**；"首次消费批次"以**首次写入公共映射**的批次为准。
   本批次仍为模块内部前缀、**不新增**公共映射，故不改集中码表计数。
8. **状态预算门禁保持 incomplete（第 2 轮复审，耐久规则；第 3 轮复审重申）。** 状态数测不出时**不得**声称
   `maxStateCount` 已验证、**不得**声称内容"已证明在状态预算内"；该缺口**本身****也不得**成为拒绝理由。该
   维度的拒绝**只**来自两处：**已测得**的状态数超过已接受上限，或**声明自身推出的可证明下界**（最长可接受
   轨迹长度 `L` 有限 ⇒ 状态数 ≥ `L + 1`）超过已接受上限；二者都走既有 `budget_exceeded` 与
   `pattern.maxStateCount`（第 5 条、§8.2 界分第 2 句）。因此"缺口"既**不**等于"已验证不超限"，也**不**等于
   "已拒绝"：门禁**保持 incomplete**。

#### 3.8.9 S7A-3 合同组合与 Gameplay Capsule v2 裁决（2026-10-04）

S7A-3 第二半的三处合同选择为 **G2 + R2 + P1**，完整裁定见
[S7A-3 未闭合合同组合裁定](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-3-contract-selection-g2-r2-p1.md)。

1. **G2：完整解析并冻结 `preparedGrace`。** 显式、继承和 prepare 前冻结的默认声明进入同一解析流程；候选先精确量化，再检查 `allowChartGrace`、声明范围和 `TickSpan` 可表示性；缺失默认、溢出、窄化、负值、越界和 `sticky` / `observe` 覆盖稳定拒绝。最终值与生效 policy 进入 judgement identity，来源进入 content identity / diagnostic context。
2. `preparedGrace` 是 prepare-time 只读值，不等同于资源的 `declaredGapGrace`；它不自动开启 `gap`、`handoff` 或连续接触恢复。7A 资源 gap grace 仍只允许零值；各 phase 的 T4 消费方式已由 §3.8.11 闭合，运行时代码与证据仍属 S7A-4。
3. **R2：生成不可变资源计划。** prepare 消费完整资源声明、Requirement 侧 claim declaration、intent、policy、稳定 claim key 和 `PreparedGrace`，产出唯一 slot、确定性排序依据和运行期 identity 生成规则；实际 owner / lease / contact 仍由引擎在规范阶段分配。`observe` 不占用资源；capacity > 1、owner 集合、并列 slot、handoff、`gap` 和非法覆盖稳定拒绝。
4. **P1：新 candidate revision 加新必需 semantic section 组。** 新 Gameplay v2 语义必须与 Foundation `REQ0` / `CNS0` 可区分，旧 Reader 对未知必需 section / capability / requirement kind 稳定拒绝。2026-10-04 已裁决为 Gameplay Capsule v2：`candidateRevision = 2`；必需 `GPH0`、`GPR0`、`GPD0`、`GRC0`、`REF0`；四个 Gameplay section 的 schemaRevision 为 1、`rowCodec = 1`、`flags = 0`；标量 little-endian，索引/计数 unsigned LEB128，有符号 priority 使用 ZigZag LEB128；REF0 kind 8--13 分配为 pattern、measure、resource、claim-key、emission、gameplay-capability。唯一完整字段表、kind 13 固定投影子域、结构 hash 与拒绝边界见 [Gameplay Capsule v2 format](GAMEPLAY_CAPSULE_V2_FORMAT.md)，提案页只保留选择历史。
5. G2-T4 与 R2-K4 同时闭合：有 tail 时 `preparedGrace` 只形成 checked hard deadline `D = end + preparedGrace`，无 tail 时 `D = end`；不扩大成功窗口、不产生 gap/handoff；资源运行时采用 `coordinator.policy.greedy_v1`，竞争键为唯一 `(priority, tieRank)`，observe 不占 slot，capacity 固定为 1。该组合不关闭状态预算 `INCOMPLETE GATE`，也不表示 S7A-3 或 Stage 7A 完成。

**完整性与权威边界。** 四个 Gameplay section 有 16-byte section-local header；REF0 仍为
Foundation raw rows、count 来自目录，**没有 Gameplay header**。完整 typed graph 必须保留
六元组、phase ordinal、capacity presence、精确 timing/timebase/late policy、Pattern operands、
Measure tokens、静态域几何、merged declarations、资源/关系/solver/FactBinding 存在性与能力闭包。
`observe/consume/claim` 三种 intent 不合并；slot 为每个 resource 的唯一 slot，不能按 claim 行递增。
physical indices 不作语义身份。Reader 重建 owning candidate，再走同一 prepare、包含性/时间/唯一性
验证与原子 publication；不信任文件里的闭包、竞争顺序或 hash 来替代语义验证。
§3.8.6 的归属矩阵不变，不删除 Foundation 的表现 asset 引用。本次没有新增公共 ABI 类型或诊断码。

#### 3.8.10 G2-S2：来源、解析与身份分区

1. 三种来源必须明确选择其一：**显式精确 duration**、**单层命名 duration 引用**、
   **prepare 前显式冻结的 default duration**。命名引用只指向直接 duration 声明，
   不再继承；缺名、缺值、环、链、冲突候选拒绝，不能退回 default。
2. `allowChartGrace=false` 时拒绝 chart supplied override，不把被禁止的值偷偷当 default。
   同一输入不得同时提供 rational duration 与 legacy tick candidate；sticky/observe override 拒绝。
3. 选中来源后以精确有理数计算 duration×unitInTicks，并且**只执行一次 half-even 量化**：
   恰半整数取偶数。使用 checked/wide integer，检查非负、canonical min≤max、量程和 signed i64
   可表示性；非法负 duration 不因舍入为零而合法。无 float、钳制、截断或隐式零。
4. 来源类别与 resolver algorithm revision、**有效时间消费 policy**分别存储。显式/继承/default
   的来源 token、引用名字、诊断路径不是 runtime 行为，不进入 chart judgement 投影；
   最终 `preparedGrace`、T4 policy、受其约束的窗口/终止语义进入该投影。
   相同执行上下文、相同最终值与有效 policy，来源不同也必须具有相同 judgement identity。
   这不删除 session 的其他配置 identity；不得把 default source 名称用作隐藏竞争顺序。
5. Packed 保存最终值与 content-only provenance，不保存 CXT AST、不二次量化。
   来源信息可改变 content/artifact，但不能改变胜者、Fact 或 prepared graph 的执行投影。

#### 3.8.11 G2-T4：成功窗口、覆盖与终止表

**时间定义。** 每个可执行 Requirement 显式提供 `end`、phase-keyed 半开成功窗口 `[lo,hi)`。
Hold body 覆盖为 `[b0,b1)`；显式 tail 的 hard deadline 为
`D=checked(end+preparedGrace)`。无 tail 的 Requirement 使用 `D=end`，携带非零 grace
本身不构成拒绝，但该值对不存在的 tail 不生效，不生成额外 phase 或等待。
非零 grace 不是容差窗口、body 延长或 continuity grace。

**prepare 一致性证明。** 每条 window 引用已声明 phase，非空、同 phase 不重叠，`hi≤D`，
且存在实际可观测的离散 Tick；所有加减法可表示。Hold body 满足 `b0<b1≤end`，
head 必须能够在覆盖起点前或该 Tick 建立持有；本 T4 subset 不对晚于 b0 的 head 回填覆盖。
tail window 每个 Tick 均在 body 完成后，故 `lo≥b1` 且 `hi≤D`。
窗口矛盾、无可成功 Tick 或 end/body 不一致整体拒绝，**不裁切、不加一 Tick**。
准备验证是静态合法性，不保证玩家一定命中。

| 事件 / 条件 | phase 与 Requirement | 资源与 Fact |
| --- | --- | --- |
| head 窗口内合格输入 | head 成功，建立 body 追踪 | 原子 commit 获得唯一 lease；失败竞争不抢占已有 owner |
| `release t<b1` | body 立即失败，未完成的依赖 tail 失败，Requirement 终止 | 同一 commit 终止 owned resource；已成功 head Fact 不回滚、不重发 |
| bodyEnd timer `t=b1`，覆盖成立 | body 成功；有 tail 则等待显式 tail，否则 Requirement 成功终止 | 有 tail 时保留 held 以等待合法 release；无 tail 时立即 free/terminal |
| body 完成后、tail 窗口内且 `t<D` 的合法 release | tail 成功、Requirement 终止 | 立即 free/terminal；不无意义地保留到 D |
| release 已在 body 后但不在 tail 窗口 | 未完成 tail 失败，Requirement 终止 | 立即终止；不进入 gap，不允许重新按下恢复 |
| 最后成功窗口的关闭 timer，已无可能完成未决 phase | 该未决 phase 失败，Requirement 终止 | 可在 D 之前终止；grace 不强制等待 |
| hard deadline timer `t=D` | 只 finalize 尚未完成的必需 phase；若 body 覆盖不成立则失败 | 原子终止，已 emitted phase Fact 不重复 |
| observe | 仅 observation，不作 owner | 不得终止/释放其他 Requirement 的资源 |

同 Tick 的 timer 在 input 前。bodyEnd 与 deadline/窗口关闭相等时，先结算覆盖，再处理仍未
完成 phase 的到期终止；相同事件都服从既有 Fact total order，timer 操作顺序不是新 Fact 排序键。
若 `b1=D` 且有 tail，静态窗口无解，prepare 已拒绝，不能指望同 Tick release 补救。
`preparedGrace=0` 时 D=end，`t=end` release 不能挽救 tail；可成功 tail 必须位于 end 之前，
且覆盖已完成。成功配置例：end=1000、grace=40、body=[900,1000)、tail=[1000,1020)；
release1000 先完成覆盖再成功，release1020 已错过，1040 截止。零 grace 合法例：
end=1000、body=[900,990)、tail=[990,1000)，999 成功、1000 不成功。

资源终止由 ResourceRecord 的 `terminalAfterTermination` 选择 free 或永久 terminal；
owner/lease 的终止与 phase finalization 同一 CoordinationCommit，不能先对外发 Fact 再释放。
同 Tick 后续 canonical input event 可以使用已 free 的 slot，terminal 永不再分配。
一个 commit 不跨事件回填 winner；不 preempt、不 handoff、不跨 Requirement 恢复。

#### 3.8.12 R2-K4：展开键、排序与局部唯一性证明

**竞争计划。** `priority` 为 signed i64、`tieRank` 为 unsigned u64，按二者**升序**；
同资源所有 occupying 实例的完整 pair 必须唯一，claimKey 也必须唯一。
不因两个窗口看似不交叠而允许 collision；K6 互斥放宽没有被选中。
observe 的 pair/claimKey/slot 缺席，consume 与 claim 保留不同 intent。
claimKey 是寻址 ID，**不是排序 fallback**；也不以 Requirement identity/hash/字典序补胜者。
跨 resource 按 resourceId bytes 定序，fanout=1；Fact total order 仍为 §3.9 的既定 tuple。

**声明展开允许两种表达，prepare 后只有显式 pair：**

| 表达 | 合同 |
| --- | --- |
| 全展开实例表 | 以 E 六元组显式映射 priority/tieRank/claim namespace；所有 occupying 实例恰好一条，无未知实例 |
| 有限 affine rank block | 每个 invocation/emission family 显式提供 priority、base、positive stride、稳定 nodeId 顺序与每维有限 radix；repeat index 组成 mixed-radix expansionIndex，tieRank=checked(base+stride×expansionIndex) |

mixed-radix 按显式 outer-to-inner node 顺序从零累计 `index=index×radix+repeatIndex`；
每项 index<radix，所有乘加 checked u64。零实例 block 不产出 candidate。
多个 local Requirement 使用各自显式 block，不能偷偷追加源数组 position。
block 交叠、溢出、缺声明、同 invocation 的路径歧义都在 prepare 拒绝。
全局 flatten 后再按 resource 检查 pair 与结构 claimKey 唯一；
显式 block/table 是 authoring lowering 方式，不是新 runtime capability 或 Packed AST。
已量化的 time/Measure/grade/来源改变不重新生成 rank；只有作者改业务 pair 才改变该竞争次序。

claimKey 的结构为 `(explicitNamespace,E)`；canonical 字符串分帧见
[Capsule §4](GAMEPLAY_CAPSULE_V2_FORMAT.md)，不使用不转义的 delimiter 拼接。
唯一 slot 的结构是 `(resourceId,slotToken,0)`，所有该资源候选共用它；
claim 行号不分配 slot，actual lease/contact 由运行时引擎生成。

**闭世界 proof 责任。** 支持的显式 profile 为 algorithm=`coordinator.policy.greedy_v1`、
objective=`[first-eligible]`、tieBreak=`[priority.asc,tieRank.asc]`、
rejectIfNonUnique=true。这些是**本 subset 的显式准入值，不是缺省列表**；
不能把未声明的 objective 自动补全，其他/global objective 继续稳定拒绝。
prepare 核验全部声明、有限 candidate 集、capacity=1、无迁移/抢占、key 唯一、时窗合法；
形成本地 evidence（资源候选 key 表、T4 deadline/window 检查、pair collision 检查结果）。
资源 free 时任何非空 eligible 子集都有唯一最小 pair；空集唯一“不提交”；
held 时只能路由合法 owner update 而非重新竞争；terminal 时不产生新 lease。
以规范 event 次序归纳可得该 objective 下唯一提交轨迹，**不声称其他优化目标的最优解**。
runtime 只执行 immutable plan，不重跑 solver、不搜索 proof。
Packed Reader 重新做上述验证，不信持久化“已证明”标志；本地 evidence 不设公共 proof codec。

Pattern 的完全展开、timer/emission logical path 与 identity/snapshot/fact-order 一致性仍须由
S7A-3 实现给出 reference 对照；允许有限的符号化编译与不动点计算，不能靠逐份枚举巨大空匹配
repeat 才终止，也不能把原 repeat bounds 的 hash 当作展开证明。
此 proof 不抹掉包含性门禁，更不把状态预算未测维度判成通过：
**state-budget INCOMPLETE GATE 保持独立**，新数值上限仍属 S7A-9。

### 3.9 六步 Tick 顺序与八阶段 Coordination window 的唯一映射（第 4 轮裁定，2026-10-03）

本节承载第 4 轮（仲裁、资源与事实序；进入 S7A-4 前）裁定中的 `Q-16` / `CM-C05`。轮次总览与逐条登记见
[RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §4、§8 与 §9；provenance：
同一 Codex 话题 thread `s7a4-arbitration-rulings`（模型 `gpt-6-astra`，consult），主裁定卡 verdict `adopt`、
confidence 0.97，处置词与计数确认卡 verdict `adopt`、confidence 0.99；两张卡的文件名时间戳为
2026-10-02T20:0xZ（UTC），统一按**本地日期 2026-10-03** 登记（与第 2、3 轮先例一致）；带日期证据见
[第 4 轮门禁报告](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-4-gate-rulings.md)。共同基线标识见本文
`## S7A-4 限定冻结范围与登记规则（2026-10-03）` 一节。

**唯一映射。** 外层是计划 §S7A-4 的**六步 Tick 顺序**，内层是 Preliminary Design §5.3 的**八阶段
Coordination window**；**八阶段是外层第 4 步的内部完整展开**，不是与六步并列的第二套顺序，也不存在
第二个映射。

| 外层步骤（Tick 顺序，规范） | 内层展开（Coordination window 阶段） |
| --- | --- |
| 1. 激活到期 requirement | —（无内层展开） |
| 2. 处理到期 Release / tail 或 hard-deadline timer | —（无内层展开） |
| 3. 规范化并应用输入 | 八阶段第 1 步：收集本窗口内的 Observation |
| 4. 每个事件重建候选并按 resource / 显式 K4 pair / fanout 仲裁 | 八阶段第 2–7 步：生成 Candidate；结算到期 `gap` 与 recovery；处理 `handoff_pending` 竞争；求解仍为 `free` 的资源与新的 binding / quota；以不可变 `CoordinationCommit` 一次性提交；7A 的竞争总序细化见 §3.8.12，claimKey/requirementId 仅寻址 |
| 5. 由 commit 生成 Fact，随后按 causal total order 规范排序 | 八阶段第 8 步：从 commit 生成 Fact（不允许中途向 Ruleset 或 Presentation 可见） |
| 6. Tick 末提交 Hook / signal，下一 Tick 才可见 | —（无内层展开） |

规则：

1. 上表是本 Spec 与 [Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) 中**唯一**的两级顺序映射；实现、trace
   与 golden 均按此解释，不得另立第三套顺序。
2. **7A 对 `gap` / recovery / handoff 输入稳定拒绝**：内层第 3、4、5 步在 7A **不被消费**，遇到
   `gap` / `handoff_pending` / 非零资源 `declaredGapGrace` / handoff 声明时按 §7.2 的 R-04 与 §3.10 **稳定拒绝**，不得用
   空实现或默认值绕过。
3. **外层第 5 步的 causal total order 由第 5 轮（2026-10-03，`S7A5-R01`）定案**：Fact 的**唯一**规范总序
   为 **`(commitTick, originKindPriority, canonicalOrdinal)`**。`canonicalOrdinal` 是引擎从
   `(originScope, originOrdinal, localOrdinal, phasePriority, factKindPriority)` 派生的**规范序**；
   **不得使用 `ingressSequence`、容器顺序或线程完成顺序**。Fact 的 `tick` **即** `commitTick`；
   `observationTick` 只定位 observation origin 并参与其 `originOrdinal` 派生，**不是**总序首键；
   `commitTick` 是提交事务的 Tick（§3.7.5）。**原含 `ingressSequence` 的旧 tuple 已被本条取代，不得再
   作为实现、trace 或 golden 依据**；其**物理字节合同**（`FactId` / `CommitId` 编码、varint / endianness、
   reference-evaluator byte fixture）属 **S7A-6** 的首次序列化消费（`CM-F06` / `Q-12`，见 §3.17 与
   §11.1 第 6 条）。
4. 外层第 6 步的 Hook / signal 只在下一 Tick 可见，与本 Spec §7 的 Ruleset 事务边界一致。

**归属层级。** 八阶段的**顺序与阶段语义**进入 **`judgement identity` 的 engine 组件**，**不进入
chart/content identity**：改变阶段顺序或阶段语义必须改变 judgement identity，而重排源数组、改
`sourceMap`、换表现资源或改 Packed 压缩顺序都不改变它（§5.2、§5.3 的同一口径）。

**S7A-4 的冻结强度。** S7A-4 **只冻结阶段名称、顺序、映射与语义边界**。**不冻结**阶段编号、序列化编码、
预算或窗口数值；这些留给首次消费它们的后续批次（见 §S7A-4 节与 §11.1 第 6 条）。

**五步变体已被取代。** [SEMANTIC_KERNEL.md](../proposals/research/gameplay-v2/SEMANTIC_KERNEL.md) §4.2 的
"一个 Coordination window 的求值分为五步"是**非权威推导**，**已被本节与 §S7A-4 节的六步外层 / 八阶段内层
规范取代**，不得继续作为实现依据；该研究稿只在其 §4.2 加标注，其技术内容与历史论证**保留不改**。

### 3.10 7A 资源子集与身份规则（第 4 轮裁定，2026-10-03）

本节承载第 4 轮裁定中的 `Q-11` / `CM-C09`（`S7A4-R02` / `S7A4-R05`），并据此改写
[CONTRACT_MATRIX.md](../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md) `CM-C04` 的"只启用前三态"表述。

**7A 资源状态子集（冻结）。** 7A 只接受 `capacity = 1` 的 exclusive resource，其状态子集为
**`free` / `held` / `terminal`**：

| 转移 | 语义 |
| --- | --- |
| `free` + claim 被提交 | → `held`（生成新的 lease） |
| `held` + 合法 update | → `held`（保留 owner 与 contact） |
| 终止后按资源策略 | → `free`（槽位可复用）或**永久 `terminal`**（槽位不可复用、后续 claim 稳定拒绝） |

**稳定拒绝清单（7A）。** 下列表示与行为在 7A **一律稳定拒绝**，不得降级、不得占位实现：

1. `gap` 状态与非零资源 `declaredGapGrace`（**资源 gap grace=0 是 7A 的唯一合法值**，
   不限制 Requirement 的 `preparedGrace`；后者见 §3.8.10–§3.8.11）；
2. `handoff_pending` 状态与 handoff（含 handoff Hold）；
3. `capacity > 1`；
4. owner 集合（多 owner 共享同一资源）；
5. 并列 slot（同一资源存在多个可竞争槽位）。

拒绝面映射：第 1、2 项与 §7.2 的 **R-04**（`resource.handoff_unsupported`）一致；第 3、4、5 项与 §7.2 的
**R-01**（`resource.capacity_unsupported`）一致。**两类都不新增 R 编号**，本清单仍为 **19 条**。

**身份规则（冻结）。**

1. `resourceId` **只在 prepared canonical graph 的资源命名空间内**解释；它不是全局字符串命名空间，也不得
   被宿主、表现层或 Session 重新解释。
2. `capacity = 1` 资源使用**唯一 slot**；slot 是资源在 prepared graph 中的稳定身份，不是运行时容器位置。
3. **slot identity、lease identity、最小 contact handle identity、claim identity 进入 canonical graph /
   prepared judgement inputs**；它们都参与 judgement identity（§5.2 的口径）。
4. lease 与 contact 由**引擎按 §3.9 的规范阶段**分配；宿主不能提供 lease / contact identity。
5. **`contact end` 不复用旧 handle**：设备端 contact 结束后其 handle 作废，新的接触必须取得新 handle；
   contact handle 的复用只能通过资源的终止后**槽位复用**发生，且必须由 §3.9 的规范阶段提交。
6. **`observe` 只产生 Observation，不占用、不改变资源 owner**：`observe` 不进入资源状态表，不生成 lease，
   也不需要 slot。
7. 同 Tick 的阶段顺序**严格服从 §3.9 的唯一映射**。

**不冻结的部分。** 本节的**语义边界**已冻结；`terminal` 的**编码**、资源状态表的逐字段表示与
serialization 不在本轮冻结，登记为首次消费它们的后续批次阻塞项（见 §S7A-4 节与 §11.1 第 6 条）。

### 3.11 早 / 晚判定与 error 记录（P1-12，第 4 轮裁定，2026-10-03）

本节承载第 4 轮裁定中的 `P1-12`（`S7A4-R06`），冻结 Fact Ledger 内容的早 / 晚判定边界：

1. **Exact 由判定窗口半宽定义。** Exact **不要求** `error = 0`：Exact 的判定窗口以声明的半宽表达，
   落在半宽内的观测（含量化误差）判为 exact。**具体半宽数值不在本批冻结**（属后续预算 / profile 批次），
   但"Exact 由半宽定义而非零 tick"这一语义边界已冻结。
2. **窗口外早击不产生 Fact，但产生诊断。** 早于判定窗口的输入**不得**生成 Hit / Miss Fact；它必须产生
   **稳定诊断**（复用 §9.2 的既有类别与稳定码，不新增 R 编号），且不得被静默丢弃或计为 Miss。
3. **未观测完成的 Miss / absence 由到期 timer 产生 Fact。** 尚未终止的 requirement 在
   deadline（含 T4 最后可成功窗口关闭）到期时 finalize，沿 §3.9 外层第 2 步处理。
   T4 的显式早释放或非法 tail release 可以在此之前失败终止；它是实际观测导致的 phase failure，
   不是把窗口外早击计为 Miss，之后 deadline 不重复发 Fact（§3.8.11）。
4. **Hold 的 head / body / tail 分别记录有符号 error，不合并。** 三个 phase 各自保留
   `observationTick - chartTick` 的**有符号整数 tick 差**（符号表达 early / exact / late），
   **不得**合并成单个 error，也**不得**只保留其中一段。
5. 上述 error 的单位与符号表达与 [Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) §单位与量程的
   `TimingError`（有符号整数 tick 差）一致；ABI 只承载单位与符号，不重定义本节的窗口语义。

### 3.12 solver / coordinator 边界与 `SolverProfile` 语义（第 4 轮裁定，2026-10-03）

本节承载第 4 轮裁定中的 `Q-03`（`S7A4-R01`）与 `CM-C06` / `CM-C07` 的语义闭合。

**边界（冻结）。**

1. **prepare / compile solver** 负责展开、验证、**上界证明**、**唯一性证明**，并生成 **prepared profile**；
   solver 在 prepare 期运行，不进入运行期。
2. **不解析/编译 solver；runtime coordinator 只执行已准备的确定性 policy**：它只执行 prepared profile
   指定的**确定性候选排序、资源检查与提交**（即 §3.9 的外层第 4 步与内层第 2–7 步在 7A 允许的部分）。
   该措辞即 `P2-02` 的规范替换文本（第 4 轮 `S7A4-R01` 确认改写方向，第 7 轮 `S7A7-R07` 逐处落地并关闭，
   2026-10-03）。
3. 名称边界：若仍需区分该策略，采用 **`coordinator.policy.greedy_v1`**；**不得**把 runtime 行为称为
   solver。
4. `greedy_v1` 只接受**能证明唯一解**的图；超 fuel、目标缺失或无法证明唯一时，在 **prepare 稳定拒绝**
   （`CM-C07` 的 `accept` 处置不变）。

**`SolverProfile` 的冻结强度（S7A-4）。**

| 项 | S7A-4 是否冻结 |
| --- | --- |
| 字段**语义**（`solverId` / `revision` / `algorithm` / `objective` / `tieBreak` / 预算字段 / `rejectIfNonUnique` 各自的含义） | **冻结** |
| **缺失 / 歧义 / 无法证明唯一**时的 **prepare 拒绝语义** | **冻结** |
| `objective` / `tieBreak` 为**有序语义列表**（顺序有意义，不是集合） | **冻结** |
| 默认列表（默认目标函数清单、默认 tie-break 顺序） | **不冻结**（S7A-9 / 后续预算批次） |
| 具体算法预算、`K` / fuel / `maxCandidates` / `maxBranches` / `maxFuel` 等 `max*` **数值** | **不冻结**（S7A-9 / 后续预算批次测量后单独接受） |
| 序列化表示（字节布局、编码） | 第 4 轮不冻结；2026-10-04 的 S7A-3 只闭合 Capsule 的静态 Solver 行；runtime state/proof codec 仍未授权 |

**唯一性证明的编码不在本批。** 第 4 轮只冻结"必须由 prepare 证明唯一、否则 prepare 稳定拒绝"。
2026-10-04 的 K4 已在 §3.8.12 闭合本 subset 的局部证明算法与本地 evidence 责任；
公共 proof 编码仍登记为首次消费它的后续批次阻塞项，不能由“已经排序”冒充其它 objective 的证明。

### 3.13 S7A-4 不得消费清单（实现须逐条对齐）

按 plan §S7A-4 的验证要求与 S1-05 的"不得用默认值、临时 typedef、序列化编码或伪成功实现绕过阻塞"
口径，S7A-4 实现**不得消费**：

1. runtime solver 或 global solver（**不解析/编译 solver；runtime coordinator 只执行已准备的确定性 policy**，
   见 §3.12）；
2. `max_cardinality`；
3. bounded backtracking；
4. `capacity > 1`；
5. Chord / `binding`；
6. `gap` / handoff；
7. `sameContact`；
8. 连续轨迹（含 Slider continuity）；
9. 非零资源 `declaredGapGrace`；不包括 §3.8.10–§3.8.11 的 Requirement `preparedGrace`；
10. `boundedRelationInstance`；
11. 运行时脚本、IO、随机与墙钟；
12. 默认数值限额；
13. 未冻结枚举或整数 typedef；
14. 未获限定授权的 runtime/proof/identity 序列化字节布局；已授权 Capsule 静态输入不在此禁令内；
15. 把 `observe` 解释成占用资源的实现。

该清单同时写入 [Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) 的
`## S7A-4 限定冻结范围与登记规则（2026-10-03）` 一节，并在 §S7A-4 节汇总，供实现逐条对齐。

### 3.14 Ruleset Tick 三阶段与 `faulted` 行为矩阵（第 5 轮裁定，2026-10-03）

本节承载第 5 轮（Ruleset、事实序、Score 与 Snapshot；进入 S7A-5 / S7A-6 前）裁定中的 `Q-08`（`S7A5-R03`）。
轮次总览与逐条登记见
[RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §5、§8 与 §9；provenance：
同一 Codex 话题 thread `01a0fe49-cfe5-7933-8d03-3d0a65d34c8b`（consult），主裁定卡 verdict `adopt`、
confidence 0.95，处置词与计数确认卡 verdict `adopt`、confidence 0.99，ABI 状态格首词与追踪计数追问卡
verdict `adopt`、confidence 0.98；文件名时间戳为 2026-10-02T20:2x–20:3xZ（UTC），统一按**本地日期
2026-10-03** 登记（与第 2、3、4 轮先例一致）；带日期证据见
[第 5 轮门禁报告](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-5-6-gate-rulings.md)。共同基线标识见本文
`## S7A-5 限定冻结范围与登记规则（2026-10-03）` 与 `## S7A-6 限定冻结范围与登记规则（2026-10-03）` 两节。

**Ruleset Tick 的三阶段（冻结）。**

| 阶段 | 内容 | 失败时的行为 |
| --- | --- | --- |
| ① Fact commit | **先追加并封存已排序的 Fact Ledger**：本 Tick 的 Fact 按 §3.9 第 3 条与 §3.17 的规范总序排序后追加；已提交 Fact **不可撤回** | 本阶段不参与第二阶段的回滚；失败时不得留下"半个 Tick"的 Fact |
| ② Ruleset state commit | **再原子校验并提交全部** StateDelta、Score、Combo、Statistics 与 RuleEffect | **整 Tick 不提交**：**不追加 fault Fact**、**不发布** RuleEffect / PresentationEvent、**保留旧 state**；session 进入可查询 `faulted` |
| ③ Presentation projection | **只从已提交 Fact Ledger 与 FactBinding 投影**只读 PresentationEvent（不读未提交的 StateDelta 或 Ruleset 内部状态） | 第二阶段失败时本阶段**不执行**，也不发布任何 PresentationEvent |

规则：

1. 三阶段的**顺序固定**，不得重排、不得跨界：第三阶段只能读**已提交**的 Fact Ledger 与 FactBinding，
   不能读未提交 state，也不能写任何状态。
2. **第二阶段失败不追加 fault Fact**：fault 本身**不产生 Fact**，因此不参与规范总序、不改变 Fact Ledger；
   失败原因只经诊断（§9.1 的 `code` / `category` / `severity`）与可查询的 `faulted` 状态暴露。
3. 第二阶段失败时**不发布** RuleEffectEvent 与 PresentationEvent；**已提交的 Fact 仍然保留**，第三阶段
   可在**显式查询或恢复**时按已提交 Fact Ledger 重建**只读** PresentationEvent——这与"不发布本 Tick 的
   表现事件"不矛盾：重建是查询 / 恢复路径，不是把失败 Tick 的表现事件发布出去。
4. `faulted` **行为矩阵（冻结）**：

| 操作 | `faulted` session 的行为 |
| --- | --- |
| `submit` | **稳定失败**（不接受新的 judgement mutation） |
| `advance` | **稳定失败** |
| `seek` | **稳定失败** |
| `replay` | **稳定失败** |
| 就地 `reload` | **稳定失败** |
| `snapshot`（新建或取得新快照） | **稳定失败**（`faulted` **不可新建 snapshot**） |
| 查询（诊断 / 原因 / 状态） | **允许**：`faulted` 是**可查询**状态 |
| 显式 `reset` | **允许**：清空为**新 session** 的状态（按 §3.20 不产生 Fact） |
| 创建**替换新 session** 的 `reload` / recovery | **允许**：这是显式 reset 之外唯一能离开 `faulted` 的路径 |

5. 离开 `faulted` 的**唯一**路径是**显式 reset** 或**创建替换新 session 的 reload / recovery**；
   **不得恢复或伪造未提交的 StateDelta**：不得用旧快照补齐本 Tick 未提交的状态，也不得把 fault 前的
   中间态写成已提交。
6. `SessionState` 的 `faulted` 是本 Spec 的语义；ABI 只承载状态名与查询面（域 6 的 `SessionState`）。
7. 本节的"`faulted` **不可新建 snapshot**"与 §10 第 6 条、ABI §S7A-5 节一致，用于消除旧表述中
   "faulted snapshot"的歧义：**快照的状态闭包包含 fault 诊断 / 状态**（§3.18），但**处于 `faulted` 的
   session 不能新建或取得新快照**。

### 3.15 `RegisterKind` 三类语义与冲突策略（第 5 轮裁定，2026-10-03）

本节承载第 5 轮裁定中的 `CM-S02`（`S7A5-R04`），是 §3.14 第二阶段（Ruleset state commit）的**合成算子
分类**。三类语义与冲突策略**冻结**，7A **只接受这三类**。

| `RegisterKind` | 允许的贡献者 | 合成 / combine operator | 冲突与失败 |
| --- | --- | --- | --- |
| `exclusive` | **只能有一个声明 owner** | 无（单写者寄存器） | **第二 owner 或第二写入稳定失败** |
| `commutative_monoid` | **只能由已声明贡献者**贡献 | 必须是**已声明**且**可交换、可结合**的 combine operator | **未知 operator**、**重复 contribution identity**、非交换或非结合的合成**稳定失败** |
| `ledger_derived` | **无直接写入者** | 由**已提交 Fact Ledger** 确定性重建 | **禁止直接 StateDelta 写入**；直接写入**稳定失败** |

规则：

1. 7A 只接受上表三类；**其他 kind**（未声明、扩展或宿主自定义）**稳定拒绝**。拒绝映射到 §7.2 的
   **既有条目**与 §9.3 的既有类别，**不新增 R 编号、不新增拒绝类别**（§7.2 仍 19 条、§9.2 仍九类）。
2. 三类是 **state commit 的合成算子分类**，不是评分语义：它们决定该寄存器如何合成，不改变 Score /
   Combo / Statistics 的语义，也不改变 Fact 的内容或顺序。
3. "同一写目标唯一 owner" 对三类都成立：`exclusive` 是单 owner；`commutative_monoid` 的每个贡献者仍须
   **声明**且同一 contribution identity 不得重复；`ledger_derived` 不产生任何直接 StateDelta 写入。
4. Hook 的"下一 Tick 可见"不变（§3.9 第 4 条）：`RegisterKind` 不改变 Hook / signal 的可见性与提交时点。
5. 字段表示与编码**不在本轮**（ABI 域 6 的 `RegisterKind` 首词保持 `待冻结`，见 §S7A-5 节）。

### 3.16 Ruleset 输入边界：7A 只用内置 Ruleset（第 5 轮裁定，2026-10-03）

本节承载第 5 轮裁定中的 `Q-17` / `CM-S08`（`S7A5-R05`）。

1. 7A **只接受内置、静态注册的 Ruleset Interface 与模块**；模块顺序冻结在 **prepared manifest**
   （S7A-5 的只读 prepared 视图），**不来自**任何外部 package。
2. 任何 `cuexis.ruleset` package 输入命中**既有 R-09**，诊断 `ruleset.package_unsupported`（§7.2）。
   拒绝时**不得读** package hash、manifest、迁移矩阵或任何 package 内部字段。
3. package 的**形状、identity、完整性与版本迁移**归 **S7C-2**；本节**不冻结**它们的任何表示，也不因
   本节存在而获得授权。
4. 拒绝面映射到既有条目，**不新增 R 编号**（§7.2 仍 19 条）。

### 3.17 Fact 身份、commit 分配与 revision 归属（第 5 轮裁定，2026-10-03）

本节承载第 5 轮裁定中的 `Q-12` / `CM-F06`（`S7A5-R01` + `S7A5-R02`）。`S7A5-R01` 的规范总序是
§3.9 第 3 条，本节不重复其全文；本节冻结**身份生成**与**语义 revision 归属**。

1. **规范总序**：见 §3.9 第 3 条（`(commitTick, originKindPriority, canonicalOrdinal)`）；
   `canonicalOrdinal` 由引擎从 `(originScope, originOrdinal, localOrdinal, phasePriority, factKindPriority)`
   派生，**不得**使用 `ingressSequence` / 容器顺序 / 线程完成顺序。
2. **`originId`**：**不透明**，由 `(originKind, originScope, originOrdinal)` **唯一确定**；`originScope` 与
   `originOrdinal` 由引擎派生（基于 canonical window 与 observation identity），**宿主不能提供**；同一
   三元组的重复出现必须得到**同一** `originId`。
3. **`commitId`**：按规范总序在**会话内单调分配**；同一 commit 内的全部 Fact 共享同一 `commitId`；
   seek / replay 从已提交 Fact Ledger 重建，**不重新分配**。
4. **`factId`**：`(commitId, localOrdinal)` 的**语义身份**——Fact 在 commit 内的稳定定位；它不是全局
   字符串，也不是可自由构造的宿主值。
5. **`phasePriority`**：engine phase registry 的**稳定语义 rank**，**只**用于派生 `canonicalOrdinal`；
   **绝非评分**——不得进入 Score / Combo / Statistics / grade，也不得被内容、宿主或表现层覆盖。
6. **timer ordinal**：由 **prepared timer identity** 派生；**seek / replay 不重新编号**——重放路径中
   timer Fact 的 identity 必须与实时路径逐位一致。
7. **correction**：7A **关闭**，出现即由 **R-08** 拒绝；因此 correction **不能影响同 Tick 排序**
   （不存在参与规范总序的 correction Fact）。
8. **语义 revision 归属**：上列生成语义与 phase registry 进 **`FactSemanticRevision`**，它属于
   **`JudgementIdentity.engine`** 的语义版本；改变总序派生、origin / commit 分配或 phase registry
   **必须**改变它。它与 `stateSchemaRevision` 是两个独立轴（§3.18 第 3 条）。
9. **留给 S7A-6 的部分**：**物理字节布局、varint / endianness、`FactId` / `CommitId` 编码与
   reference-evaluator byte fixture** 不在本轮；本轮**不冻结**这些，也不得据本节声称线格式已定。

### 3.18 Snapshot / Replay header 字段集、`SnapshotPayload` 闭包与不得保存清单（第 5 轮裁定，2026-10-03）

本节承载第 5 轮裁定中的 `Q-13` / `CM-K01` / `CM-K07`（`S7A5-R06` + `S7A5-R07`）。

**全量 Snapshot header 字段集（冻结）**：`formatVersion`、四分量 `JudgementIdentity`（`engine` /
`ruleset` / `chart` / `session`）、`factSemanticRevision`、`stateSchemaRevision`、normalized event count、
Fact count、payload byte count、typed byte-budget descriptor。

**Replay header 字段集（冻结）**：`formatVersion`、四分量 identity、`NormalizationProfile` metadata、
effective late-policy metadata、`eventCodecId`、normalized event count、encoded byte count、
`ReplayDecodeBudget`。

规则：

1. **拼写规范**：类型为 **`EventCodecId`**，字段为 **`eventCodecId`**（小写首字母）。该规范修正 ABI 中
   类型条目与 `ReplayHeader` 字段写法的不一致；两处必须统一到本条。
2. `factSemanticRevision` 是 **Fact / 排序 / 折叠解释语义**（§3.17 第 8 条），**进 engine identity**。
3. `stateSchemaRevision` **只**标识 `SnapshotPayload` 的**无损状态结构**，**不进** judgement identity；
   它与 `factSemanticRevision` 是两个独立轴，任一变化都不得冒充另一轴的变化。
4. **`SnapshotPayload` 闭包（冻结）**必须包含：
   - 已提交 Fact Ledger 至 **Fact cursor** 的**前缀**及 cursor；
   - 活动实例（active instance）状态；
   - Pattern 状态；
   - Release / tail phase 状态；
   - exclusive resource 状态；
   - **finalization watermark**；
   - **未提交窗口**；
   - `preparedGrace`；
   - 离散 sequence 状态；
   - Hook snapshot；
   - Fold / Score / Combo / Statistics 状态；
   - **pending signal queue**；
   - `SessionState`；
   - fault 诊断 / 状态。
   **FactBinding 与 prepared immutable graph** 由 header identity 验证后**重新取得**，**不重复保存**。
5. **不得保存清单（冻结）**：Presentation / Effect cache、Animation 临时值、HostOverride token、
   **未提交 StateDelta**、连续采样相位（7A 无连续重采样格点或采样相位）。
6. typed byte-budget **descriptor 是字段**，**不是**已接受的数值承诺；具体预算数值仍属 **S7A-9**（§8.4），
   不得据本轮声称限额已冻结。
7. **增量 snapshot / 压缩 ledger / 跨 minor 迁移**在无全量 golden 前**不进公共合同**。
8. 本轮只冻结**字段集与语义**；两套 header 的**字节布局**（编码、字段顺序、对齐、varint / endianness）
   由**首次序列化消费它的 S7A-6** 冻结。

### 3.19 `SeekLatencyCommitment`（第 5 轮裁定，2026-10-03）

本节承载第 5 轮裁定中的 `CM-K08`（`S7A5-R08`）。

1. **类型**：`SeekLatencyCommitment` 是带 **measurement-profile 引用**的 typed **engine commitment**。
2. **承诺语义**：从**最近可用快照**恢复并**推进到目标**的 **wall-clock seek latency**。它不是判定正确性
   指标，也不是 tick 距离；它不改变 seek 的结果，只约束延迟。
3. **收紧关系**：会话可以声明**更严格**（更小上界）的值，**不得放宽** engine 值。
4. **测量口径入口**：
   [BUDGET_AND_EVIDENCE_PLAN.md](../proposals/gameplay-v2-acceptance/BUDGET_AND_EVIDENCE_PLAN.md) §5 的
   restore time、Seek p95 / max 与**无损性口径**。本 Spec 不复制其测量表。
5. **数值禁令**：**具体 `maxSeekLatency` 数值禁止**进入 Schema、header、运行时约束或对外承诺；提供
   具体 commitment 值的输入**稳定拒绝**——不得把它降级为默认值、隐式零值或"不限制"。
6. **正确性不变**：seek 仍须满足**无损正确性**（§10 第 5 条：逐位等于从起点运行）；承诺语义只约束
   延迟，不放松正确性。
7. 数值的**首次接受在 S7A-9**（测量后单独接受）；本轮只冻结类型、承诺语义、收紧关系与测量口径入口。

### 3.20 Outcome、`FactCategory`、grade 与 `TimingError`（第 5 轮裁定，2026-10-03）

本节承载第 5 轮裁定中的 `CM-S10` / `P1-07`（`S7A5-R09`）。

1. `Outcome` 的 7A 集合**只有** **`{hit, miss}`**；任何"局部结果"都**不是** `Outcome` 的成员。
2. **Hold 的 head / body / tail** 各自是**附属的 phase-local outcome / error**，**不能扩充** `Outcome`；
   它们不产生第二个结果维度，也不得被当成新的 Outcome 值。
3. `FactCategory` 与 **phase 一一对应**，集合为 **`{tap, hold_head, hold_body, hold_tail}`**；category
   由 phase **派生**，不得由调用方任意赋值，也不得出现"有 category 无 phase"的 Fact。
4. **grade table 可选**：缺失时 grade 为 **absent**，只报 Outcome；**绝不以 error、category 或默认表隐式
   升级** grade，也不得用默认表补齐缺失的 grade。
5. `TimingError` 存在时 = **`observationTick - chartTick` 的有符号整数 tick 差**：符号表达 early / late，绝对值是
   tick 距离；不做单位换算、不取绝对值、不饱和（单位与符号的 ABI 承载见 ABI §单位与量程）。
6. **统计规则**：`seek` / `replay` 从**已提交 Fact Ledger** 重建 Score / Combo / Statistics（不依赖未提交
   state、不依赖表现事件）；`reset` 清空**新 session** 的状态且**不产生 Fact**（不写 Fact Ledger、不产生
   correction、不改变旧 session 的历史）。
7. 以上语义冻结；**字段表示、溢出 / 饱和表示与编码**仍属首次消费它的后续批次（ABI 域 5 / 域 6 的首词保持
   `待冻结`，见 §S7A-5 节）。

### 3.21 Life 在 7A 关闭（第 5 轮裁定，2026-10-03）

本节承载第 5 轮裁定中的 `CM-S11`（`S7A5-R10`；该编号**只**用于 `LifeState`）。

1. 7A **不实现** Life；`LifeState` 只在 ABI 侧保留**类型追踪与拒绝说明**。
2. 任何 **Life capability 声明、Life policy、`LifeState` 初值或占位默认值**一律以
   **`capability.disabled` 稳定拒绝**；**不创建、不更新** `LifeState`。
3. 不得用"默认满值""默认零值"或"未启用即跳过"实现绕过；拒绝必须发生在**任何 `LifeState` 创建或更新
   之前**，并给出诊断（§9.1 的 `code` / `category` / `severity`）。
4. 关闭 Life **不改变** Score / Combo / Statistics 的语义，也**不新增 R 编号**（§7.2 仍 19 条）。

### 3.22 S7A-5 / S7A-6 不得消费清单（实现须逐条对齐）

按 S1-05 的"不得用默认值、临时 typedef、序列化编码或伪成功实现绕过阻塞"口径，第 5 轮登记两份清单。
两份清单与第 2 轮（§3.7）、第 3 轮（§3.8.5）和第 4 轮（§3.13）的清单**只做叠加、不冲突**。

**S7A-5 不得消费**：

1. ingress 排序（含 `ingressSequence`、容器顺序与线程完成顺序）；
2. correction（仍由 R-08 拒绝）；
3. Ruleset package（含 package hash / manifest / 迁移矩阵）；
4. Life（Life capability、Life policy 与 `LifeState` 的任何初值）；
5. 默认 grade 表与默认数值限额；
6. 未冻结的字节编码；
7. 未测量的 seek 数值；
8. 表现资源 / UI / 默认绑定对 judgement 的影响。

**S7A-6 在 S7A-5 清单之外另不得**：

1. 保存表现缓存、Animation 临时值与 HostOverride token；
2. 保存未提交 delta；
3. 保存或恢复连续采样状态（连续重采样格点与采样相位）；
4. 保存 snapshot interval identity；
5. 把 typed byte-budget descriptor 解释为**已接受的数值承诺**。

两份清单同时写入 [Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) 的
`## S7A-5 限定冻结范围与登记规则（2026-10-03）` 与
`## S7A-6 限定冻结范围与登记规则（2026-10-03）` 两节，供实现逐条对齐。两份清单都**不得**把
`phasePriority` 用作评分（§3.17 第 5 条）；`P1-14` 的归属表（§3.8.6）**不被** S7A-5 / S7A-6 消费，
其首次消费是 **S7A-3** 的 REF0 首次写入。

### 3.23 发布粒度、表现桥接与聚合（第 6 轮裁定，2026-10-03）

本节承载第 6 轮（发布粒度、表现桥接与诊断）裁定中 `Q-06`、`Q-09`、`Q-15` 与 `CM-P04` / `CM-P06` /
`CM-P08` 的语义。provenance：Codex 话题 thread `01a0fe61-3476-7882-9674-5f6b05035237`（consult，模型
`gpt-6-astra`），单卡 verdict `adopt`、confidence **0.97**，按本地日期 2026-10-03 登记；编号推导见
[RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §6。**全部首次消费批次为
S7A-7**；逐条清单与不得消费项见本文 `## S7A-7 限定冻结范围与登记规则（2026-10-03）` 节。

**第 1 条（发布入口，`S7A6-R01`）。** 入口集合是**闭集**：`playback = true` 时 7A 只允许
`packed-chart` 与 `gameplay-graph`；`author-source` **不是** Playback entry；Ruleset package **不是**
第三种 Playback entry。manifest **必须记录七项**：① entry kind、② compiled semantic identity、
③ artifact identity、④ Ruleset binding、⑤ capability closure、⑥ resource / presentation closure、
⑦ `sourceOf`。**字段清单冻结、物理编码与预算数值不冻结**（见 §3.6 与 §5.6）。

**第 2 条（判定闭包与表现投影的粒度，`S7A6-R04`）。** **Gameplay graph 是必需判定闭包**，
**Presentation / Effect 是可选只读投影**。因此：

1. **缺 Presentation 允许 headless judgement**——判定结果不因缺少 Presentation 数据而改变；
2. **判定闭包内 target / domain / action / table 缺失 = prepare 原子失败**，类别为
   `identity_closure_incomplete` 或 `invalid_relation`（§9.2 的既有九类，**不新增第十类**）；
3. **纯表现 target 缺失**：允许**空绑定**，**丢弃该投影事件**，并发出稳定诊断
   **`presentation-target-missing`**；**不 fault session**、**不改写 Fact Ledger**、**不改变判定**。

**第 3 条（表现桥接的命名层与 precedence，`S7A6-R03`）。** 7A 用**独立的
Gameplay-to-Presentation adapter**，把**已提交的 FactBinding** 转成带 source Fact 的宿主覆盖令牌。
适配器使用的命名层是 **`GameplayOverride`**，precedence **命名常量**为：

```text
Initial < Behavior < Animation < GameplayOverride/FactBinding < StudioPreviewOverride
```

**`render.visible = false` 是 `GameplayOverride` 层的显式值**（不是"缺少覆盖"）；`GameplayOverride`
与 `FactBinding` 在 precedence 中**同一层**。AnimationSystem 不读 Judgement / Chart JSON / CXT /
Ruleset，只接收 typed animation input（与 §3.6、ADR 0044 决策 2 一致）。

**第 4 条（adapter 生命周期与幂等，`S7A6-R03` / `S7A6-R05`）。**

1. **只消费已提交 FactBinding**——未提交 delta 不产生任何 token；
2. **当前有效立即应用**；**未来生效的排队在 `effectivePresentationTick`**（不做隐式单位换算）；
3. **去重键 `(factId, targetId)`**：同一键重复应用**幂等**（重复 Hit / Miss / late Hit 不产生第二次
   效果）；`aggregation` 三值 **`any` / `all` / `groupCommit` 均幂等**；
4. **`groupCommit` 部分提交不回滚**：已提交部分保留，记稳定诊断 **`partial-group`**，
   并**进入稳定拒绝路径**；**correction 仅能影响未提交窗口**；
5. **seek / replay / reload 一律从 Fact Ledger 重建 token**（不依赖表现缓存、不保存 HostOverride
   token，与 §3.18 的 `SnapshotPayload` 不得保存清单一致）；
6. **终止条件**：声明的 lifetime 到期，或 reset / session replacement（替换为新 session）；
7. **表现失败绝不回写 Fact Ledger**——表现层是只读投影，任何表现侧失败都不得改变判定、Fact 内容或
   Fact 顺序。

### 3.24 P1-08：ABI 类型与 canonical model 的映射（第 6 轮裁定，2026-10-03）

本节承载第 6 轮 `P1-08`（`S7A6-R07`）的语义；首次消费批次 **S7A-7**。

| ABI 位置 | 映射规则 |
| --- | --- |
| `InputEvent.observationTime` | **拆为两个字段**：`observationTick`（整数 tick）**+ 原始时间戳**。`observationTick` **仍唯一来自校准会话时钟**（§3.7.3 的 Q-04 / CM-T04 结论不变）；原始时间戳只作溯源，不参与 canonical 排序 |
| `JudgementFact.error` | **有符号整数 tick 差**（与第 5 轮的 `TimingError` 同一口径） |
| `JudgementResult.eventSequence` | **会话内单调序号**（用于会话内定位，**不是** §3.9 第 3 条的规范事实总序键） |

**不做隐式单位换算**：任何跨单位表达（tick ↔ 时间戳、tick ↔ beat）必须显式，禁止在 ABI 边界上
"顺手换算"，否则会出现第二套时间定义。ABI 侧的语义正文见
[Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) 域 1 / 域 2 / 域 5。

### 3.25 P2-04…P2-09：分类层、独立字段、资源三分拆与 Replay 归属（第 6 轮裁定，2026-10-03）

本节承载第 6 轮 `P2-04`、`P2-05`、`P2-06`、`P2-08`、`P2-09`（`S7A6-R08`…`S7A6-R12`）的语义；
首次消费批次**均为 S7A-7**。

**第 1 条（分类层与结果层，`P2-04` / `S7A6-R08`）。** `phase` / `category` 是**分类层**，`outcome` /
`grade` 是**结果层**；`localOutcome` **只**用于 Hold 段分解；`reasonCode` **只**解释拒绝 / 降级。
**`FactCategory` 与 phase 一一对应**：`{tap, hold_head, hold_body, hold_tail}`（与第 5 轮同口径）；
**`outcome` 仅 `{hit, miss}`**；**`grade` 可选、缺失即 absent**（不隐式升级、不填默认值）。
**同层互斥、跨层可组合**：同一层内不得同时出现两个互斥取值；跨层字段可以组合出现。

**第 2 条（三个独立字段，`P2-05` / `S7A6-R09`）。** `action`（**输入侧动词集合**，取值
`press` / `release` / `update` / `absence` / `step`）、`requiredAction`（**requirement 侧声明**）、
`domain` / `InputDomain`（**作用域绑定**）三者**独立、不互相推导**；任何一方都不得从另一方推断或替代。

**第 3 条（`resources` 三分拆，`P2-06` / `S7A6-R10`）。** `resources` 拆为三项、**分别归属不同层**：
① **`resourceRef`**（引用）、② **`claimPolicy`**（申请策略）、③ **resource record**（全局定义）。
资源状态使用 S7A-4 已冻结的子集 **`free` / `held` / `terminal`**（`gap` / `handoff_pending` / 非零
grace / handoff / `capacity > 1` / owner 集合 / 并列 slot 仍稳定拒绝，见 §3.10）；**本轮不新增资源状态**。

**第 4 条（Replay / Snapshot 归属，`P2-08` / `S7A6-R11`）。** **只有 Fact Ledger 进 Replay /
Snapshot**；Effect Graph 与 `RuleEffectEvent` 属**规则集投影**，`PresentationEvent` 属**表现投影**，
三类投影事件**都不进** Replay / Snapshot；**`EffectEvent` 只是投影内部名**（共同基类名），
**不是**可序列化的对外类型。与 §3.18 的 `SnapshotPayload` 闭包 / 不得保存清单一致。

**第 5 条（未知字段与 `extensions`，`P2-09` / `S7A6-R12`）。** **语义字段一律"未知即拒绝"**——未知
语义字段、未知 capability、未知必需 section 一律稳定拒绝（与 §3.6、§7 一致）；**`extensions` 只允许
命名空间化**的 inspection metadata，且**必须证明不影响判定**（不改变 Fact Ledger、不改变结果、不改变
顺序）。**输入 / 几何三码不可重复扩展**：`input.continuous_unsupported`、`input.direction_unsupported`、
`geometry.inference_rejected` 保持唯一，不得再定义同义码（见 §9.6）。

## 4. canonical Requirement identity

Requirement 的稳定寻址身份（CM-I03、FB §7）：

```text
(chartEntryId, invocationId, moduleId, exportId, emissionPath, requirementLocalId)
```

`emissionPath` 是由稳定 `nodeId` 与 Repeat index 构成的有序路径。

| 变化 | Requirement identity | canonical / judgement identity |
| --- | --- | --- |
| 改 Beat / lane / 参数值 | 不变 | 改变 |
| 改 Repeat count | 只增删对应 index 的 emission | 改变 |
| 重排源数组 | 不变 | 不变 |
| 改 `nodeId` / `invocationId` / `moduleId` / `localId` | 改变 | 改变 |
| 删除 Presentation | 不变 | 不变 |

每一条性质必须逐条在实现批次中验证（FE §5）。`sourceMap` 记录旧字段路径、模块与 Studio 节点，
只服务诊断、编辑器与迁移，**不进入** judgement identity。

## 5. identity projection 表

### 5.1 表结构

本表是**唯一**的 identity projection 表（CM-I06 / Q-10）。列定义：

| 列 | 含义 |
| --- | --- |
| `字段 / 组` | gameplay 字段或字段组 |
| `source` | 是否进入 source / build identity |
| `semantic` | 是否改变 canonical semantic identity |
| `artifact` | 是否进入 artifact identity |
| `judgement` | 是否改变 judgement identity |
| `presentation` | 是否只影响 presentation |
| `interchange` | 是否参与 interchange / compatibility 判定 |

取值：`是` / `否` / `条件`（条件见该行备注）。所有判定一律以**实际生效的准备值
（prepared value）**为准。**凡能改变 Fact Ledger 的内容或顺序者，必须改变 judgement
identity**（Q-10）。

#### 5.1.1 三项命名的统一定义（`P2-07`，第 7 轮裁定，2026-10-03，`S7A7-R08`）

本表涉及的三个概念**必须**按下述**三项独立命名**使用，**不得**再混用 `sourceMap`、source-build
identity 或 content identity 作为它们的同义替代（`P2-07`；provenance：thread
`01a0fe61-b7a3-7853-a380-0793fee14e14`，verdict `adopt`，confidence **0.96**，2026-10-03）：

| 统一名称 | 定义 | 归属 | 现名 / 曾用名 |
| --- | --- | --- | --- |
| **`source closure`** | 源 Chart、模块、参数、源映射与元数据的**闭包集合** | **source / content** 分量（§6.5） | 曾与 "source-build identity"、"`sourceMap`" 混用 |
| **`diagnostic map`** | 只服务诊断、编辑器与迁移定位的**映射**；**不进入任何 closure**，也不参与 identity | 仅诊断侧（§3.8.6 第 1 条第三类） | 即本表 `sourceMap` 行的语义（**保留字段名 `sourceMap`，但概念名统一为 `diagnostic map`**） |
| **`content-artifact identity`** | 内容产物的**物理身份**：由载体 bytes 与物理编码参数决定 | **artifact** 分量（§5.3 第 2 条） | 曾与 "content identity"、"source-build identity" 混用 |

1. 三项**各自独立命名与归属**，**不得**互相替代，也**不得**合并成一项。
2. 本表 §5.2 的 `sourceMap` 行语义即 **`diagnostic map`**；§6.5 的 `author / source closure` 行即
   **`source closure`**；`tool / compiler closure` 与 artifact 分量共同构成
   **`content-artifact identity`** 的判定输入。
3. **`P1-14` 的 REF0 / manifest / diagnostic 归属仍以 §3.8.6 为准**，**不因本命名统一而改归属**
   （`P1-14` 由第 5 轮 `S7A5-R11` 裁定，本节不改其矩阵）。
4. ABI 侧对应 **`SourceBuildIdentity`**、**`CompiledSemanticIdentity`**、**`ArtifactIdentity`** 三行
   （域 7）：本轮只把语义文字对齐到本节命名，**不新增类型行**、**维持 9 域 / 126 = 96 + 30**
   （见 [Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) 域 7）。

### 5.2 逐字段归属

| 字段 / 组 | source | semantic | artifact | judgement | presentation | interchange | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 外层 `version = 5` | 是 | 否 | 是 | 否 | 否 | 是 | 版本门禁；不匹配即稳定拒绝 |
| `gameplay.version = 2` | 是 | 否 | 是 | 否 | 否 | 是 | 唯一合法组合，见 §2.1 |
| `packedVersion` | 否 | 否 | 是 | 否 | 否 | 是 | 载体版本 |
| `candidateRevision` | 是 | **否** | **是** | 否 | 否 | **是** | 只改 artifact identity；必进互换性判定（Q-01） |
| `graphRevision` | 是 | 是 | 是 | 是 | 否 | 是 | 语义图修订 |
| `timebaseRef` | 是 | 是 | 是 | 是 | 否 | 是 | `TimebaseProfile` 进 judgement（CM-T02、Q-04）。**第 2 轮（2026-10-03）确认**：进入 judgement identity 的是**准备值**，含 `TimebaseProfile` 声明的单位、offset、calibration 与 late policy（typed 参数；其**数值**可否进入 identity 以 §3.7.4 的 `pending_measurement` 登记为准——未测量的参数在 prepare 即稳定拒绝，不产生半成品 identity） |
| `rulesetRef` + Interface 投影 | 是 | 是 | 是 | 是 | 否 | 是 | 规则集绑定；Build hash 属 ruleset 分量 |
| `requirements[]` | 是 | 是 | 是 | 是 | 否 | 是 | 见 §4 与下列各字段 |
| `resources[]`（资源声明） | 是 | 是 | 是 | 是 | 否 | 是 | `capacity=1` exclusive 子集 |
| `groups[]` / `relations[]` | 是 | 是 | 是 | 是 | 否 | 是 | 跨 Requirement 关系；7A 仅 `exclusive` 子集 |
| `solverProfiles[]` | 是 | 是 | 是 | 是 | 否 | 是 | 含 `rejectIfNonUnique` 与 fuel 声明 |
| `factBindings[]`（纯表现部分） | 是 | 条件 | 是 | 条件 | 是 | 是 | 仅当 `aggregation` 改变判定事实时进 judgement（CM-P05） |
| `capabilities[]`（source 声明） | 是 | 是 | 是 | 是 | 否 | 是 | 只声明最低必需集合，见 §6 |
| derived capability closure | 否 | 是 | 是 | 是 | 否 | 是 | 排序后的派生闭包；写入 Packed header。**第 6 轮（2026-10-03，`S7A6-R02`）**：**必须与 `compiledSemanticIdentity` 同时携带且可比较**；逐字段（`semanticKind` / `requiredFormat` / `staticBudget` / `snapshotCost` / `replayImpact` / `supportedDomains` / `stableRejectCode`）的 identity 归属见 §5.6 |
| `phases[]` 与 phase/category 表 | 是 | 是 | 是 | 是 | 否 | 是 | 含 phase 注册表 |
| 图形原语选择与编译产物 | 是 | 是 | 是 | 是 | 否 | 是 | Pattern/Measure 编译结果 |
| `preparedGrace` 与 `resourceDecisionPolicyRef` | 是 | 是 | 是 | 是 | 否 | 是 | 相同最终 grace/policy 即共享 judgement identity（CM-R11） |
| 判定域与判定几何（typed record） | 是 | 是 | 是 | 是 | 否 | 是 | 静态 typed 判定域；与 Presentation transform 完全隔离 |
| late policy（`reject_late` / `queue_next_tick`） | 是 | 是 | 是 | 是 | 否 | 是 | 策略写入 session judgement identity（CM-T07） |
| Fact 排序键与 `originKindPriority` | 是 | 是 | 是 | 是 | 否 | 是 | observation 0 / timer 1 / coordination 2 / correction 3；改变须提升 Fact semantic revision（CM-F04） |
| `originId` / `commitId` / `factId` / `phasePriority` | 是 | 是 | 是 | 是 | 否 | 是 | 生成与排序字节合同属 CM-F06 |
| Ruleset 模块/fold 顺序与 Hook 合成 | 是 | 是 | 是 | 是 | 否 | 是 | 冻结在 package manifest 并进 ruleset 分量（CM-S07） |
| `sourceMap`（统一概念名 **`diagnostic map`**，§5.1.1） | 是 | 否 | 否 | 否 | 否 | 否 | 只服务诊断/编辑器/迁移（FB §7）；**不进入任何 closure、不参与 identity**（`P2-07`） |
| pack 工具链 profile / compiler profile | 是 | 否 | 是 | 否 | 否 | **是** | 归 tool/compiler closure，进 source/artifact（FB §8） |
| Packed 压缩顺序与字典顺序 | 否 | 否 | 是 | 否 | 否 | 是 | 进 artifact identity（PD §9.2、CM-I04） |
| Presentation 资源/材质/动画/Audio | 是 | 否 | 是 | 否 | 是 | 否 | 不进 judgement closure（FB §8） |
| InputMapping profile 与校准 | 否 | 否 | 否 | 是 | 否 | 是 | 属 session 分量；运行期修改一律稳定拒绝（CM-T09、CM-X09）。**2026-10-03 输入半批复核（CM-T09）**：session identity 分量为 `profileId`、`profileVersion`、`sourceClass`，**加规范化后的域 token 集合与每项域声明的完整 `AmountSpec` 字段（`scale` / `minimum` / `maximum` / `boundaryPolicy`）**；**域声明顺序不承载语义**（重排不是 identity 变化），**增删任何域或改动其 `AmountSpec` 是 identity 变化**；**7A 不引入 per-domain 版本字段**——会话级版本由 `profileVersion` 承载（**不得**推迟到 S7A-4） |
| Loadout / 默认 grace 来源 / JudgementConfig | 否 | 否 | 否 | 是 | 否 | 是 | 属 session 分量（计划 §S7A-6.1） |
| 会话内计数器（`eventSequence` / Replay 游标 / 快照间隔） | 否 | 否 | 否 | 否 | 否 | 否 | 会话状态，不回写 Chart |
| Replay header 字段集 | 否 | 否 | 是 | **是** | 否 | 是 | 重建同一判定的必需项；字段集属 CM-K01 |
| Snapshot header 字段集 | 否 | 否 | 是 | **是** | 否 | 是 | 字段集未定，见 §10 |
| 未知自定义类型 | 是 | 是 | 是 | **是**（默认） | 否 | 是 | 无法证明纯表现即默认归 judgement（CM-I05） |

### 5.3 三条判定的分别声明

1. **semantic identity 判定**：由 Canonical Gameplay Graph 与派生 capability closure 的语义投影
   决定；忽略 `sourceMap`、作者源排版、输入数组顺序与物理编码选择。
2. **artifact identity 判定**：由载体的物理 bytes 与物理编码参数决定，包含 `candidateRevision`、
   `packedVersion`、压缩与字典顺序、pack 工具链 profile；**不**蕴含语义等价。
3. **interchange / compatibility 判定**：由 §2.3 的维度集合决定；两个产物只有在语义等价
   **且**互换性判定成立时才可互换。

### 5.4 条件行说明

| 行 | 条件 |
| --- | --- |
| `factBindings[]` 的 semantic / judgement 归属 | 仅当 `aggregation` 改变判定事实时进 judgement；纯表现映射只进 presentation |
| `capabilities[]` 的 judgement 归属 | 以 assembler 派生且实际生效的 closure 为准，不以 source 声明字面为准 |
| Replay / Snapshot header | 以实际生效的 prepared value 与四分量投影为准 |

### 5.5 identity 的规范字节边界（第 3 轮裁定，2026-10-03）

本节落地计划 §S7A-3 第 5 条"生成 chart / content / prepared identity 的规范字节"与 S1-05"不得冻结
序列化编码"之间的张力点裁定（provenance：thread `s7a3-prepare-rulings`，verdict `need_info`，confidence 0.8，
2026-10-03；见 [RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §9 第 3 轮行）：

1. **规范字节是 prepare 内部确定性派生物**：identity 的规范字节由 prepare / 离线 assembler 的确定性算法
   在内部派生，**不是公共 ABI**，也**不是 Packed 编码**。
2. **冻结的是语义等价、排序与身份域边界**：字段进入哪一条 identity（§5.2）、相等判定所依据的语义等价
   （§3.5）、以及规范排序（§3.7.2、§5.3）在本轮冻结；**编码本身暂不冻结**。
3. **同一算法、相同字节**：S7A-3 实现必须用**同一算法在各工具链产出相同字节**，并以**跨工具链 golden、
   输入置换（数组 / 参数 / 引用顺序）与 file / memory 对照**验证（§3.5；plan §S7A-3 的验证要求）。
4. **编码实现须在 S7A-3 消费前闭合**；在此之前不得把任一内部字节序当作已冻结合同，也不得据它单独做
   产物互换性判定（§2.3：互换性还须覆盖工具链与载体差异）。
5. ABI 侧**仅声明对外 identity 的不透明性**，不承诺内部字节格式（见
   [Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) 的 `## S7A-3 限定冻结范围与登记规则（2026-10-03）`）。

### 5.6 capability closure 与 `CapabilityRecord` 的逐字段 identity 归属（第 6 轮裁定，2026-10-03）

本节承载第 6 轮 `Q-19` + `CM-X01`（`S7A6-R02`）的语义；首次消费批次 **S7A-7**。provenance：Codex
话题 thread `01a0fe61-3476-7882-9674-5f6b05035237`（consult，`gpt-6-astra`），单卡 verdict `adopt`、
confidence **0.97**，本地日期 2026-10-03。

**第 1 条（携带要求）。** **`compiledSemanticIdentity` 与完整 capability closure 必须同时携带、且
必须可比较**——缺任一项或两者不可比较的产物在 prepare 稳定拒绝（类别 `identity_closure_incomplete`）。
"可比较"指：两个产物能仅凭所携带的 compiled semantic identity 与 closure 判定"同语义 / 不同语义"，
**不得**要求读取表现缓存、运行时状态或未冻结的内部字节。

**第 2 条（`CapabilityRecord` 七字段与逐字段 identity 归属）。** 一节字段集如下；"投影"列即 §5.1 的
identity 维度归属，**进 `judgement semantic hash` 与否是本条的核心判定**：

| 字段 | 含义 | 投影 / 归属 | 进 judgement semantic hash |
| --- | --- | --- | --- |
| `semanticKind` | 能力语义种类 | **semantic projection** | **是**（改变即改变语义 identity） |
| `requiredFormat` | 所需格式与 revision | **semantic projection** | **是** |
| `staticBudget` | 静态预算描述 | **closure 与 interchange compatibility** | **否**（不进 semantic hash） |
| `snapshotCost` | 快照成本描述 | **closure / interchange** | **否**（不进 semantic hash） |
| `replayImpact` | Replay 影响枚举 | **semantic projection** | **是** |
| `supportedDomains` | 域集合 | **semantic projection** | **是** |
| `stableRejectCode` | 稳定字符串拒绝码 | **只进诊断码表 / closure** | **否**（**不进 identity**） |

**第 3 条（与 §5.2、§6.4 的一致性）。** 本条是 §5.2 中 `capabilities[]` 与 derived capability closure
两行的**逐字段细化**，并承接第 4 轮 §3.10 的资源子集与第 5 轮 `P1-14` 的归属表口径：`staticBudget` /
`snapshotCost` 属 **closure 与互换性**维度（与 §2.3 的 interchange 判定一致），`stableRejectCode` 属
**诊断码表**（见 §9.6），三者**都不得**因为"看起来像语义"而进入 semantic hash。**具体预算数值仍然
不冻结**（§8 与 §S7A-7 节的不得消费清单第 ① 项）。

## 6. capability 派生与闭包

### 6.1 五来源与唯一派生链

能力声明的五处来源：Packed META `requiredFeatures`、CXT v2 `requiredExtensions`、
Chart v5 `gameplay.capabilities`、Stage 7 本地 registry、Session capability 协商
（AR P0-14、SR §7）。唯一派生链（CM-X06 / Q-14）：

```text
source（Chart v5 / CXT v2）      只声明最低必需的 capability / profile
  -> 离线 assembler              从 canonical graph、Ruleset、presentation closure 派生完整闭包
  -> Packed header               保存排序后的 derived closure
  -> CXC manifest                复制 artifact-required capability，供早期拒绝
  -> Session                     只报告四态，不改写 graph
```

### 6.2 稳定失败与失败时点

1. 声明**少于** assembler 派生的完整 closure 时**稳定失败**；**不允许** compiler 静默补齐
   （Q-14，Codex 修订后文本）。
2. 失败时点固定在**最早可确定 closure 不足的编译 / 装配阶段**；不得推迟到运行期。
3. Session 侧的 capability 查询只报告四态，不得改写 graph、不得提升声明集合。

### 6.3 四态查询

| 状态 | 含义 | 候选拒绝码 |
| --- | --- | --- |
| 未知 | 本地 registry 不认识该 `capabilityId` | `capability.unknown` |
| 已知但未启用 | registry 认识，但本次 session/编译未启用 | `capability.disabled` |
| 已启用但资源/预算不足 | 能力可用，但静态预算或设备能力不满足 | `capability.budget_insufficient` |
| 可用 | 通过 prepare 前全部校验 | 无（成功） |

额外两态（V2 新增，不等同于四态）：`capability.revision_mismatch`（内容声明 revision 与引擎
支持区间不相交）、`capability.permanently_unsupported` / `capability.future_development`
（H 类边界）。四态的查询只读，不改变会话。

### 6.4 capability registry 字段（第 6 轮裁定，2026-10-03）

`capabilityId` / `revision` / `semanticKind` / `requiredFormat` / `staticBudget` /
`snapshotCost` / `replayImpact` / `supportedDomains` / `stableRejectCode`（SR §4、CM-X01）。

**第 6 轮（2026-10-03，`S7A6-R02`，首次消费 S7A-7）**：字段集与**逐字段 identity 归属**已冻结，
详见 §5.6 第 2 条；`capabilityId` / `revision` 的语义沿用 §6.1（语义变化必须新增 ID 或 revision）。
**`staticBudget` / `snapshotCost` 进 closure 与 interchange compatibility、不进 judgement semantic
hash；`stableRejectCode` 只进诊断码表 / closure、不进 identity。** 类型名与逐字段物理表示（含
`capabilityId` 的编码）仍属后续序列化批次 / S7A-9；**判定的语义边界已冻结**。

### 6.5 四类闭包

| 闭包 | 内容 | 进入何种身份 |
| --- | --- | --- |
| **`source closure`**（旧写作 `author / source closure`） | 源 Chart、模块、参数、源映射、元数据 | source / content |
| judgement closure | domain、action、判定几何、定点表、Ruleset/Interface 引用 | semantic / judgement |
| presentation closure | Object、Component、Animation、Material、Audio、Skin | content / presentation |
| tool / compiler closure（与 artifact 分量共同构成 **`content-artifact identity`**） | compiler profile、schema/compiler 版本、打包器信息 | source / artifact |

Pack 不按"当前是否 active / 是否可见 / 权重是否为零"裁剪声明闭包。`judgement closure` 不得包含
贴图、材质与命中音效；`presentation closure` 不得携带改变判定的窗口、域或 action。

### 6.6 待裁定项

- **"声明多于派生"的处置已裁定（第 1 轮 `Q-14` / `CM-X06`；第 7 轮 2026-10-03 按第 1 轮裁定补齐处置词
  与正文——五项 `CM-V*` / `CM-I06` 的补齐说明见
  [RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §7 的"五条 `CM-V*`
  处置词补齐说明"段，不占 `S7A7-R01…R16` 编号）**：capability 的**派生来源唯一**（唯一派生链见 §6.1），
  **声明集合不得多于派生闭包**；**声明少于派生即稳定失败**（§6.2），**多于派生同样不成立**——不得把多余
  声明降级为"可选"或静默忽略；该面**不新增 R 条目**（§7.2 仍 19 条），也**不新增** capability registry 字段。
- 互换性矩阵细则与 closure 数值阈值同样未冻结（矩阵细则属后续批次；数值阈值属 **S7A-9** 实测后单独接受）。

## 7. 支持集合与稳定拒绝

### 7.1 支持集合

Stage 7A **唯一**允许的实现范围是 [SR §2](../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md)
的支持集合：离散输入规范化与映射、`TimebaseProfile`、两种迟到策略、Tap / Hold head-body /
显式 Release-tail、单一 `capacity = 1` exclusive resource、`greedy_v1` coordination（**runtime 侧的名称
采用 `coordinator.policy.greedy_v1`**，见 §3.12）、
append-only Fact Ledger、typed Ruleset 事务、Score / Combo / Statistics、全量 Snapshot / Seek /
Replay，以及基于 FactBinding 的 early/exact/late/Miss 表现桥接。不在表内的能力必须命中 §7.2 的
稳定拒绝。

### 7.2 稳定拒绝清单（19 条）

| ID | 被拒绝的能力 | 候选拒绝码 |
| --- | --- | --- |
| R-01 | `capacity > 1` 资源、owner 集合、并列 slot | `resource.capacity_unsupported` |
| R-02 | Chord / `binding` 关系 | `coordination.relation_unsupported` |
| R-03 | `temporal` / `quota` 关系、有限交替、保护窗口 | `coordination.relation_unsupported` |
| R-04 | Handoff Hold、`gap` / `handoff_pending`、`resource.handoff.v1` | `resource.handoff_unsupported` |
| R-05 | 连续轨迹、Slider、区域覆盖、`input.trajectory.v1`、最低上报率、reconstruction | `input.continuous_unsupported` |
| R-06 | Flick、方向、速度、`input.direction.v1` | `input.direction_unsupported` |
| R-07 | 全局最优 / `max_cardinality` / bounded backtracking solver | `solver.profile_unsupported` |
| R-08 | Fact correction、`correctionDepth > 1` | `fact.correction_unsupported` |
| R-09 | `cuexis.ruleset` package 作为 7A 输入（**第 5 轮已裁定：7A 不含 package**，只用内置 Ruleset） | `ruleset.package_unsupported` |
| R-10 | `reopen_uncommitted_window` 迟到策略 | `late.reopen_unsupported` |
| R-11 | `sameContact` 约束、跨 Requirement relation 藏在 Pattern 内 | `pattern.relation_unsupported` |
| R-12 | 由渲染位置、材质、相机或动画隐式推断 `judgementDomain` | `geometry.inference_rejected` |
| R-13 | 运行中修改 InputMapping / Loadout / Ruleset 投影 | `session.mutation_rejected` |
| R-14 | 运行时脚本、逐帧回调、宿主字节码、动态 Requirement 生成、随机、墙钟、IO | `capability.permanently_unsupported` |
| R-15 | 三维物理斩击、任意自由体感传感器融合 | `capability.future_development` |
| R-16 | 未知必需 section、未知 capability、新 requirement kind 被解释成 tap | `capability.unknown` |
| R-17 | 旧 `gameplay.version = 1` 未显式迁移 | `format.gameplay_version_unsupported` |
| R-18 | 非空 CXT v2 `effects` 直接进入 `gameplay.requirements` | `migration.ambiguous` |
| R-19 | 迁移期无法判断判定域 / 资源共享 / 表现与 Gameplay 关系 | `migration.ambiguous` |

拒绝规则：

1. R-01 至 R-11 是**后续 capability**，拒绝时必须给出 capabilityId 与替代路径；
2. R-12 至 R-16 是**禁止**，不得通过 `extensions` 字段预留入口；
3. R-17 至 R-19 是**迁移**，必须由离线工具显式处理，Playback 不做隐式猜测；
4. 任何拒绝都不得回落到 Tap / Hold / v4 语义（计划 §1.4、§11）。

**候选拒绝码的九类 category 定案（第 6 轮 `CM-D04` / `P1-09` 的首次消费后续补齐，决策卡
2026-10-02T21:03:39.462Z，`adopt` 0.96）。** 本清单的**条数不变（仍 19 条）**，只把上表的候选拒绝码落到
§9.2 的九类 category；逐条登记见 `schemas/cuexis.gameplay-diagnostics.v2.codes.json`：

| 码 | category | 说明 |
| --- | --- | --- |
| `pattern.relation_unsupported`（R-11） | `invalid_relation` | 结构约束：跨 Requirement relation 藏在 Pattern 内 |
| `geometry.inference_rejected`（R-12） | `invalid_relation` | 隐式推断判定域是**关系 / 声明约束错误**，**不是**"未知 capability"（已由本裁定改正） |
| `session.mutation_rejected`（R-13） | `invalid_relation` | 生命周期禁止：运行中修改会话投影 |
| `capability.permanently_unsupported`（R-14） | `capability_disabled` | 已知但**永久**未启用的能力边界 |
| `capability.future_development`（R-15） | `capability_disabled` | 已知但 7A **未启用**、留待未来开发的能力边界 |
| `capability.revision_mismatch` | `unknown_capability` | 引擎不支持的 revision 即"引擎不认识的能力" |
| `format.gameplay_version_unsupported`（R-17） | `ambiguous_migration` | 九类中无"版本不支持"类，归迁移族 |
| `capability.budget_insufficient` | `budget_exceeded` | 已启用但预算不足 |

**首次消费批次补齐（同一次裁定）。** 上表八条中有六条此前没有任何 `BATCH_GATES.md` 批次声明，现统一登记
首次消费为 **S7A-3**：`capability.budget_insufficient`、`capability.revision_mismatch`、
`capability.permanently_unsupported`、`capability.future_development`、`input.direction_unsupported`
（**R-06**）、`format.gameplay_version_unsupported`（**R-17**）。**本清单仍 19 条、§9.2 仍九类。**

**第 2 轮新增的四类稳定拒绝不新增 R 条目（本清单仍为 19 条）。** Tick 溢出、`DomainAmount`
越界 / 窄化 / 量化溢出、late-policy 参数缺失或未测量、重复排队这四类，按 §3.7.8 的映射表**全部映射
到 §9.2 的既有诊断类别**（`budget_exceeded` 与 `late_policy_incomplete`）与 §9.3 的 prepare 原子失败
条件（重复排队另加 §5.2 的 judgement identity 条件行）；它们**不是**能力级拒绝，故不进入本清单；判定
依据见 [第 2 轮门禁报告](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-2-gate-rulings.md)。**本清单的
条数不因第 2 轮而改变**，改变它必须另立裁决并登记新的 R 编号。

**输入规范化半批同样不新增 R 条目（本清单仍为 19 条，2026-10-03 实现复核）。** 不连续表示（跨 gap /
重连 / 丢样）与连续输入能力（trajectory / `minimumReportRate` / reconstruction）**都复用**既有
**R-05** 的 `input.continuous_unsupported`（类别 `capability_disabled`，`capabilityId = input.trajectory.v1`，
`remediation = S7B-1`），靠 `field.path`（`continuityCapability` vs `discontinuity`）区分；
`same_tick_collision` 与 `time_reversal` 映射到既有 `invalid_relation` 原子失败路径，
`amount_narrowed` 映射到既有 `budget_exceeded`（映射表见 §3.7.8 与 §9.3）。**不新增 ABI 码、不新增 R
条目、不新增第十类**；本清单**仍为 19 条**。

**第 3 轮的三类拒绝同样不新增 R 条目（本清单仍为 19 条）。** 第 3 轮冻结的是 prepare 与 entry 的
**行为**，三类拒绝按既有条目与 §9.3 的 prepare 原子失败条件处置：

| 第 3 轮拒绝场景 | 处置落点 | 候选拒绝码 / 类别 |
| --- | --- | --- |
| 非空 `effects` 未显式 lowering 为不影响 Judgement / Replay 的 Presentation 数据（§3.8.1，Q-05） | **R-18**（非空 CXT v2 `effects` 直接进入 `gameplay.requirements`） | `migration.ambiguous` |
| 内容要求 tail 语义却未显式声明 Release / tail phase（§3.8.3，Q-18） | §9.3 的 prepare 原子失败条件（Requirement / phase 声明不足以确定语义） | 稳定码从 §9.2 / §9.3 的既有码表择一（**不得新增 R 编号**；不按隐式 legacy profile 推断） |
| `boundedRelationInstance` 或未展开循环表示（§3.8.4，CM-C10 / P1-03） | §9.3 的"未终止的 CXT 展开或未界定循环"原子失败条件 + §9.2 类别 `non_terminating_source` | 具体 `stableRejectCode` 与新的 wire 表示在该候选首次被消费的后续批次前闭合；S7A-3 不消费该表示（§3.8.2 第 4 条） |

（R-18 的稳定码在 §7.2 表中登记为 `migration.ambiguous`；上表另两行的稳定码本身尚未指定，按"行为冻结、
编码后置"的同一口径留给 S7A-3 消费前闭合。）

它们**不是**能力级拒绝（R-01…R-19 的拒绝面是后续 capability / 禁止项 / 迁移歧义），因此不进入本清单；
判定依据见 [第 3 轮门禁报告](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-3-gate-rulings.md)。**改变条数
必须另立裁决并登记新的 R 编号。**

**S7A-3 的不得消费清单见 §3.8.5**，并同时写入
[Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) 的 `## S7A-3 限定冻结范围与登记规则（2026-10-03）` 一节。

**第 4 轮的五类拒绝同样不新增 R 条目（本清单仍为 19 条）。** 第 4 轮冻结的是仲裁、资源与事实序的
**语义边界**，五类拒绝按既有条目与 §9.3 的 prepare 原子失败条件处置：

| 第 4 轮拒绝场景 | 处置落点 | 候选拒绝码 / 类别 |
| --- | --- | --- |
| `gap` / `handoff_pending` / 非零资源 `declaredGapGrace` / handoff（§3.10，`S7A4-R02` / `S7A4-R05`） | **R-04**（Handoff Hold、`gap` / `handoff_pending`、`resource.handoff.v1`） | `resource.handoff_unsupported` |
| `capacity > 1`、owner 集合、并列 slot（§3.10） | **R-01**（`capacity > 1` 资源、owner 集合、并列 slot） | `resource.capacity_unsupported` |
| Chord / `binding`（§3.10 与 §7.1 的支持集合不变） | **R-02**（Chord / `binding` 关系） | `coordination.relation_unsupported` |
| runtime solver / global solver / `max_cardinality` / bounded backtracking（§3.12、§3.13） | **R-07**（全局最优 / `max_cardinality` / bounded backtracking solver） | `solver.profile_unsupported` |
| "`observe` 占用资源"的解释（§3.10 第 6 条、§3.13 第 15 项） | **不是**能力级拒绝：它是**语义解释错误**，按 §9.3 的 prepare 原子失败条件与 S7A-4 门禁处置 | 稳定码从 §9.2 / §9.3 的既有码表择一（**不得新增 R 编号**） |

（判定依据见 [第 4 轮门禁报告](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-4-gate-rulings.md)。**改变条数
必须另立裁决并登记新的 R 编号。**）

**S7A-4 的不得消费清单见 §3.13**，并同时写入
[Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) 的 `## S7A-4 限定冻结范围与登记规则（2026-10-03）` 一节。

**第 5 轮的四类拒绝同样不新增 R 条目（本清单仍 19 条）。** 第 5 轮冻结的是 Ruleset 事务、事实序、Score 与
Snapshot 的**语义边界**，四类拒绝按既有条目与 §9.3 / §9.4 的既有条件与类别处置：

| 第 5 轮拒绝场景 | 处置落点 | 候选拒绝码 / 类别 |
| --- | --- | --- |
| `cuexis.ruleset` package 输入（§3.16，`S7A5-R05`） | **R-09**（`cuexis.ruleset` package 作为 7A 输入） | `ruleset.package_unsupported` |
| 7A correction（§3.17 第 7 条，`S7A5-R02`） | **R-08**（Fact correction、`correctionDepth > 1`） | `fact.correction_unsupported` |
| `RegisterKind` 三类之外的 kind、重复 contribution identity、非交换 / 非结合合成、`exclusive` 的第二 owner / 第二写入、`ledger_derived` 的直接 StateDelta 写入（§3.15，`S7A5-R04`） | §9.4 的运行期事务失败路径（第二阶段原子校验不通过即整 Tick 不提交并进入 `faulted`）与 §9.2 的既有类别 | 已定案为 **`ruleset.transaction_failed`**（`invalid_relation` / `session_faulted`，§9.4 末段；**不新增 R 编号**） |
| Life capability / Life policy / `LifeState` 初值或占位默认值（§3.21，`S7A5-R10`） | §9.2 / §9.3 的能力拒绝面，配合 ABI 的 `capability.disabled` | `capability.disabled` |

（判定依据见 [第 5 轮门禁报告](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-5-6-gate-rulings.md)。**改变条数
必须另立裁决并登记新的 R 编号。**）

**S7A-5 与 S7A-6 的不得消费清单见 §3.22**，并同时写入
[Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) 的 `## S7A-5 限定冻结范围与登记规则（2026-10-03）` 与
`## S7A-6 限定冻结范围与登记规则（2026-10-03）` 两节。

### 7.3 H 类支持政策（锁定）

H01（任意运行时脚本 VM）、H02（运行时动态生成 Requirement）、H03（随机生成谱面或随机判定）、
H04（墙钟依赖）、H05（IO / 网络直接参与判定）、H07（连续音高）是**能力边界，不再开发**，
不受支持（`capability.permanently_unsupported`）；H06（Beat Saber 类三维物理斩击）、
H08（任意自由体感传感器融合）是**未来开发，现不支持**（`capability.future_development`）。
政策文字不得弱化，不得为这些能力预留未定义字段或执行入口。

### 7.4 Chart v4 / CXT v1 回退路径

Chart v4 / CXT v1 继续走既有回退路径，行为不得因本阶段新增能力而改变；无 InputEvent 的纯播放
中 Judgement 保持休眠，不得改变 FrameSnapshot、FrameDigest 或 Presentation candidate。

## 8. 预算

### 8.1 唯一已冻结的生产预算

仓库内唯一已冻结的生产预算表是 [Packed 候选物理合同 §3.3](PACKED_CHART_FORMAT.md) 的 7 项：
`maxPackedFileBytes`、`maxPackedDecodedBytes`、`maxPackedSectionBytes`、`maxPackedEntities`、
`maxPackedRequirements`、`maxPackedStrings`、`maxPackedReferences`。本 Spec **不改动、不放宽**
它们；V2 新增的 gameplay semantic section 必须复用同一套逐 section 预算与 `referenceCount`
口径，不得新开绕过 envelope 预检的入口。`PackedChartLimits` 的 `0` 是字面上限，不表示"不限制"。

### 8.2 计数口径登记（上界，不冻结数值）

五类 profile 的计数口径与属性由
[预算与证据计划](../proposals/gameplay-v2-acceptance/BUDGET_AND_EVIDENCE_PLAN.md) 拥有；本 Spec
只登记其存在与口径归属，不重复其表体、不冻结限额：

| Profile | 计数对象 | 默认属性 | 超限动作 |
| --- | --- | --- | --- |
| 内容 | 编译后图规模（requirement / relation / resource / emission / Pattern 状态 / factBinding） | 硬门禁（待冻结阈值） | prepare 原子拒绝，保留旧 active session |
| 稳态 | 每 Tick 活动度、候选重算、solver fuel、Fact 数、reducer 调用、signal 队列 | 硬门禁（待冻结阈值） | 进入 `faulted`，不提交半个 Tick |
| 快照 / Seek | 快照字节、快照数、恢复耗时、Seek 延迟 | 软目标 + 引擎承诺 | 记录回归；引擎承诺不得事后放宽，会话只能收紧 |
| Replay / 解码 | 事件数、字节数、解码耗时、错误类别 | 硬门禁（Replay 头部自描述） | 稳定 `replay_format_error` 类诊断，整体拒绝 |
| Packed wire | 文件 / section / 实体 / 要求 / 字符串 / 引用计数 | 已冻结（§8.1） | 稳定 `packed.budget.*` 诊断 |

计数语义要点：Pattern 状态数按**确定化并最小化之后**的状态数计，不按 NFA 规模计；
`activityPeak` 是同一 Tick 内同时活动的 Requirement 实例数峰值；候选重算数按每个规范化输入
事件重建的 Candidate 数计，不是每次 Tick 的总和。

**Pattern 的三项计数口径（S7A-3 实现批次登记，2026-10-03；第 2、3 轮复审改正，2026-10-03；不改变本表任何一行、不冻结任何数值）。**
实现批次登记三项**互不替代**的计数，禁止互相顶替或改名上报：

1. **完全展开计数**：prepare-time 完全展开后，按声明可允许的份数求和得到的图形原语实例数；其
   **每分支上界**是 `maximum × 单份展开数`。求和无**可表示**值时记为**测量缺口**（缺席），
   缺口本身不是拒绝理由；该计数是**经证明的下界**（真值 ≥ 2^64），存在已接受上限时按下方界分第 2 句拒绝。
2. **interned 子模式数**：编译内部表示中**不同**子模式的个数；结构相同的子模式共享一个条目。
   它既不等于第 1 项，也不等于第 3 项。
3. **状态数**：即上文"确定化并最小化之后"的计数，按需测出。测不出时（测量工作集上界内无法构造）
   记为**测量缺口**（缺席），**不得**以第 1 项或第 2 项代替，也**不得**写入替代数值。缺口本身**不构成**
   拒绝；该维度的拒绝判据见下方界分第 2 句（已测得计数**或声明自身推出的可证明下界**超过已接受上限）。
   缺口使**状态预算门禁保持 incomplete**：它**不**表示该维度"已验证不超限"，也**不**表示该维度"已拒绝"。

**超范围计数与拒绝的界分（同上，第 2 轮复审改正；第 3 轮复审改正，2026-10-03）。**

判据是"该计数是否**可证明**已超过**已接受**上限"，**不是**"该计数是否溢出"，也**不是**"测量是否缺席"。
三项计数与一切上界比较一律使用**受检算术**：**不得**回绕、饱和或截断；溢出表示该计数**没有可表示值**。
三句对照（逐句可判定）：

1. **缺席 / 非下界 ⇒ 测量缺口，且不构成预算拒绝。** 计数没有**可表示**值（受检算术溢出）、计数**不是**
   经证明的下界（例如可被零份数乘掉的部分展开计数）、或计数因测量自身的工作集上界而**构造失败**时，一律
   记为该维度的**测量缺口**（访问器缺席）：**不得**用别的计数顶替、**不得**写入替代数值、**不得**据此拒绝。
   测量工作集上界是本模块的**测量能力**上界，**不是**内容阈值。
2. **已测得 或 可证明下界 超过已接受上限 ⇒ `budget_exceeded`。** 该维度**存在已接受上限**（上限必有
   `uint64` 表示）时：计数**已测得**且大于上限 ⇒ 拒绝；计数缺席但**可证明为下界**且该下界大于上限 ⇒
   同样拒绝。属此列的下界证明包括：精确受检算术的溢出与单调累加（编译工作计数、记账大小）的溢出
   （真值 ≥ 2^64）；以及**声明自身推出的状态数下界**——最长可接受轨迹长度 `L` **有限**时，最小自动机
   至少有 `L + 1` 个状态（最长接受轨迹的路径每步进入一个不同状态，否则可泵出更长的接受轨迹），故
   `L + 1` 超过已接受上限即为确实超限；`L` 本身**有限但无可表示值**时同理（计数 ≥ 2^64）。拒绝一律走该
   维度**既有** path 与 §9.2 **既有**类别（`pattern.maxStateCount` / `pattern.maxExpansionCount` /
   `pattern.maxEvaluationSteps` / `pattern.maxCompiledBytes` / `pattern.maxDeclarationDepth`），
   **不新增**诊断码。
3. **无已接受上限 ⇒ 不比较、不拒绝（门禁保持 incomplete）。** 该维度没有已接受上限时**不比较**任何数值、
   **不**产生拒绝；状态维度在此状态下仍**保持 incomplete**：既**不**声称"已验证不超限"，也**不**声称
   "已拒绝"。
4. 需要与已接受上限比较的维度采用**相对预算的早停比较**：在累加 / 累乘过程中一旦可判定超过**已配置
   上限**即判超限拒绝；无上限可比较时**不比较、不拒绝**。零因子（如 `repeat(X, 0, 0)`）使乘积精确为
   零，**不得**把操作数内部的缺口传播成该计数的缺口，否则零计数会被误判为超限。
5. 本界分**只**适用于本节的**预算维度**：§3.8.8 第 3 条的 arm / deadline **包含性**门禁在测量缺口时的
   保守拒绝是**另一道门禁**，**不是**预算诊断，也**不**受本界分约束（第 1 至 3 句不适用于它）。

### 8.3 研究观察值（禁止用作限额）

下列数值**只能**作为"待测量 / 研究观察"出现，**不得**写成 ABI 常量、公共头、Schema、默认
`PackedChartLimits`、capability 声明或 Replay 头部预算，也不得改写成"已验证上限"：
96 requirement、672 输入事件、activity peak 3、Gap timer peak 2、单快照 4,777 字节、
单快照最大 1,289 字节、Seek p95 0.014 / 0.015 ms、建议候选值 `activityPeak` 1,024、
`patternStateBudget` 4,096、`maxSeekLatency` 50 ms、`sampleCostPerSecond` 65,536，以及对抗性
Pattern 在 n=10 处超过 4,096 预算被拒绝的观察（计划第 188 行、AR 第 455 行、预算计划 §8.2）。

### 8.4 未冻结状态

Gameplay 运行时预算在 S7A-9 前**保持未冻结**，实现只提供计数与报告能力。未接受阈值时不得用
`0` 或"不限制"表示跳过。预算失败必须区分上述五类，不能用一个 `budget_exceeded` 覆盖全部。
GPU、真实设备、音频和网络输入无法在当前环境复现时，单列为"未执行 / 环境限制"。

### 8.5 预算分层、成本归属与最坏值计数入口（`P1-10`，第 7 轮裁定，2026-10-03，`S7A7-R05`）

本节承载第 7 轮（收尾澄清与缺陷）裁定中的 `P1-10`，**首次消费批次为 S7A-9**。provenance：thread
`01a0fe61-b7a3-7853-a380-0793fee14e14`（consult，模型 `gpt-6-astra`，verdict `adopt`、confidence
**0.96**）与口径确认卡 thread `01a0fe61-3476-7882-9674-5f6b05035237`（verdict `adopt`、confidence
**0.99**），2026-10-03；带日期证据见
[第 7 轮收尾裁定与缺陷处置记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-7-wrapup-rulings.md)。

1. **五类 profile 是唯一分层**：沿用
   [预算与证据计划](../proposals/gameplay-v2-acceptance/BUDGET_AND_EVIDENCE_PLAN.md) §2 的五类
   ——**内容**（prepare-time）/ **稳态**（每 Tick）/ **快照与 Seek** / **Replay 与解码** /
   **Packed wire**（已冻结部分）。本 Spec §8.2 的同一张表即这五类的口径登记，两者不得出现第六类，
   也不得合并其中两类（预算失败必须区分五类，见 §8.4）。
2. **Packed / Runtime 对齐**：V2 新增的 gameplay 语义 section **复用** §8.1 的 Packed envelope
   逐 section 预算与 `referenceCount` 口径；运行期计数项与 Packed 计数项**不得**各自新开一套上限。
3. **新增成本的归属按"谁产生谁计数"**：Candidate / branch / lease / timer / Fact / state /
   Presentation / Snapshot / Seek 等新增成本由**产生该成本的分量**计数并写入对应 profile 的证据文件；
   **不得**把成本记到不产生它的分量，也**不得**用全局汇总掩盖分量缺口。
4. **7A 只登记计数与上界**：本 Spec 只登记计数对象、口径与**上界的存在**；**禁止**在此阶段冻结任何
   具体限额。
5. **最坏值计数入口作为 S7A-9 的证据项**：每个 profile 的**最坏值计数入口**（在哪一层取最坏值、以哪个
   fixture 触发）登记为 **S7A-9** 的证据项；**具体阈值必须等 S7A-9 实测后单独接受**（§8.4 的
   "未冻结状态"不因本节改变）。
6. 本节**不改变** §8.3 的研究观察值地位（仍**禁止**用作限额），也**不新增** §7.2 的 R 条目、
   ABI 类型行或 §未决项条目。

## 9. 错误与诊断

### 9.1 层级

诊断统一为四层（CM-D04）：稳定 `code`（字符串）+ `category`（枚举）+ `severity` +
`faulted` 行为。`source path` 与 `identity component` 是上下文而非稳定码。诊断必须指出字段路径、
requirement / identity 分量与实际计数（CM-D05）。诊断码表集中为一份机器可读文件并注册为 CTest
校验项。

**第 6 轮冻结（2026-10-03，`S7A6-R06`，首次消费 S7A-7）。** 本节四层模型是**最终模型**，不再变更：
稳定字符串 `code` + `category`（§9.2 的九类）+ `severity` + `faulted` 行为；`source path` 与
`identity component` **永远只是上下文**，**不得**升级为稳定码。**集中码表**固定落在
**`schemas/cuexis.gameplay-diagnostics.v2.codes.json`**，并新增一个 **CTest 校验项**（候选名
`cuexis_gameplay_diagnostics_codes`，由工具侧在 CMake 注册；正式名以工具侧注册为准），校验：① `code`
唯一性；② `category` / `severity` / `faulted` 映射完整性（每条码都能映射到九类之一、有 severity 与
明确的 `faulted` 行为）；③ `stableRejectCode` 已登记；④ **输入 / 几何三码不可重复扩展**。**除已冻结的
`input.continuous_unsupported`、`input.direction_unsupported`、`geometry.inference_rejected` 及既有
R-09 映射外，诊断码字符串必须先在集中码表登记并通过该 CTest 校验，才能进入公共 ABI**（详见 §9.6）。

**`severity` 与 `faulted` 的取值集（第 6 轮 `CM-D04` / `P1-09` 的首次消费后续补齐，决策卡 2026-10-02T21:03:39.462Z，`adopt` 0.96）。** 本条只补齐 §9.1 四层模型中
后两层的**取值集**，不新增层、不新增类别、不新增 R 条目：

| 层 | 取值集（闭集） | 含义 | 依据 |
| --- | --- | --- | --- |
| `severity` | `info` / `warning` / `error` | 与仓库已发布面 `engine/core/include/cuexis/core/diagnostic.hpp` 的 `DiagnosticSeverity{Info, Warning, Error}` 一一对应（小写拼写为码表与文档中的规范写法） | SPEC §9.1 四层模型 + core 既有面 |
| `faulted` | `session_unaffected` / `session_faulted` | `session_unaffected`：诊断不改变已有 active session、不进入 `faulted`；`session_faulted`：session 进入可查询 faulted；未 seal kernel 失败保留旧 state/ledger，§9.4 Fold 失败保留已 seal Ledger 与旧 fold state；本轮补充见 execution profile §7 | SPEC §9.3、§9.4 第 1 条、§3.14 ② |

**7A 的已有稳定拒绝与原子失败一律取 `severity = error`**（`info` / `warning` 为保留取值，本轮没有码使用
它们）。既有 `session_faulted` 承载者为 §9.4 的 `ruleset.transaction_failed`；本轮 execution profile
另选定未 seal kernel 失败的详细码，见 [typed supplement §3](../api/gameplay-v2-execution-types.md)。
新增码尚须实现批次登记/校验后消费；其他已有拒绝保持 `session_unaffected`。两个值集与逐码取值由集中码表 `schemas/cuexis.gameplay-diagnostics.v2.codes.json`
持有，并由 CTest 校验项 `cuexis_gameplay_diagnostics_codes` 校验完整性。

### 9.2 类别

V2 九类（PD §11）：`unknown_capability`、`capability_disabled`、`budget_exceeded`、
`ambiguous_migration`、`non_terminating_source`、`non_unique_solution`、`invalid_relation`、
`late_policy_incomplete`、`identity_closure_incomplete`。Gameplay I 的 ABI §4 另有十类；两者必须
合并并给出旧码到新码的映射，**不得出现两个同义码**（CM-D02）。

**第 6 轮冻结（2026-10-03，`S7A6-R06`）。** 九类**保持不变**（**不新增第十类**）；本轮新增的稳定码
（`presentation-target-missing`、`partial-group`）以及复用既有类别的 prepare 原子失败
（`identity_closure_incomplete` / `invalid_relation`）**都映射到既有九类**，不扩类。类别枚举的
完整性由 §9.1 的集中码表 + CTest 校验项保证。

### 9.3 prepare 阶段的原子失败条件

未知 / 未启用 / 预算不足的 capability；Requirement、resource、group、relation、factBinding 的
悬空引用；未终止的 CXT 展开或未界定循环；solver profile 缺失、目标函数不完整、fuel 超限或无法
证明唯一结果；资源状态机非法转移、同 Tick policy 未声明或 grace 量化不一致；late policy 缺少
`finalizationWatermark`、queue hop 或 snapshot 规则；Ruleset 读写冲突、correction 递归、reducer
不满足声明的结合性/交换性；连续输入声明不足以解释采样空洞、断点、轨迹重建或预算；**判定闭包内**
Presentation target 缺失（**纯表现** target 缺失见 §9.7，不属原子失败）、`aggregation` 不合法或
离散属性 override 权重不为 1；v4/v5 迁移无法判断判定域、
资源共享或表现/Gameplay 关系。任一条件出现时 prepare 必须原子失败：不发布半成品、不改变已有
active session。

**第 6 轮补充（2026-10-03，`S7A6-R04` / `S7A6-R02`，首次消费 S7A-7）。** 上段"Presentation target
缺失"须按 §9.7 的分界读：**判定闭包内**的 target / domain / action / table 缺失（含
`compiledSemanticIdentity` 与 capability closure 缺一或不可比较）→ **prepare 原子失败**，类别
`identity_closure_incomplete` 或 `invalid_relation`；**纯表现** target 缺失 → **不属本节的原子失败**，
允许空绑定 + 丢弃该投影事件 + 稳定诊断 `presentation-target-missing`（§9.6 第 4 条、§9.7）。
本节其余原子失败条件**不变**，仍**不新增第十类**、**不新增 R 条目**。

**拒绝类别的三分法（第 2 轮语义经 2026-10-03 实现复核修订）。** 下列三类必须分开，**不新增第十类**
（§9.2 仍九类）、**不新增 R 条目**（§7.2 仍 19 条）；映射落点另见 §3.7.8 表：

| 情形 | 判据 | 类别 |
| --- | --- | --- |
| 声明**结构不完整** | 缺 `profileId`、缺 `unitToken`（含空值）、未声明 `initialTempo` | `invalid_relation` |
| 已声明但**声明域内数值非法** | 非正 `tickScale`、非正 tempo、非正 `stop` duration，以及数值越出其声明范围 | `budget_exceeded` |
| **late-policy 缺失 / 未声明 / 未测量** | 参数缺失、未声明，或停在 `pending_measurement` | `late_policy_incomplete` |

第 1 类与第 2 类**必须区分**：前者是**声明本身不完整**（结构 / 次序问题），后者是**声明完整但值在
其声明域内不可满足**（值问题）。第 3 类**与第 1、2 类明确区分**：late-policy 的缺失 / 未测量**不是**
profile 结构缺陷，也**不是**数值越界，而是"策略参数尚未被声明或测量"这一独立状态；因此**不得**把
缺失 late-policy 记成 `invalid_relation`，也**不得**把缺失 `initialTempo` 或空 `profileId` 记成
`late_policy_incomplete`。

`profileId` / `unitToken` **保持字符串类型**（**不改为 optional**）：**空值表示"未声明"**，由 prepare 以
`invalid_relation` 稳定拒绝；**空值不是默认单位，也不是可用的 profile**。

**输入半批的原子失败映射（2026-10-03 实现复核，`adopt` 0.97 / 0.99）。** 三分法不变（**不新增第十类**、
**不新增 R 条目**），下列三条明确归属（映射表另见 §3.7.8）：

| 原子失败条件 | 判据 | 类别 |
| --- | --- | --- |
| **同一 canonical 身份在同一 Tick 再次入场** | 输入身份 / 次序关系非法（`same_tick_collision`） | `invalid_relation` |
| **`AmountSpec` 无法精确表示（窄化）** | 先验 canonical 可表示性：不可表示 → `amount_narrowed`；可表示但越界 → `amount_out_of_range` | `budget_exceeded`（两条路径同类别） |
| **校准会话时钟回退** | 时间次序关系错误（`time_reversal`） | `invalid_relation` |

三条的判据与口径如下：

1. **同一 canonical 身份在同一 Tick 再次入场属于输入身份 / 次序关系非法**，稳定拒绝并映射
   `invalid_relation`；它与迟到策略的**重复排队**（§3.7.4 第 4 条，映射 `late_policy_incomplete`）
   **分开登记**——前者是"同一 Tick 的输入身份 / 次序关系"问题，后者是"迟到策略状态"问题。
2. **窄化先于越界**：先验证 canonical 可表示性（§3.7.6 第 6 条），再验证声明范围与边界策略；
   `exactNumerator == INT64_MIN` 因 `|INT64_MIN|` 无有符号 64 位表示而按"不可表示"（`amount_narrowed`）
   拒绝。两条路径都属 `budget_exceeded`，但**判定顺序不得颠倒**。
3. **负 `observationTick` 合法**（§3.7.3 第 4 条）：有符号 64 位域内的负值参与单调排序，**只有校准会话
   时钟回退被拒**，且属**时间次序关系错误** → `invalid_relation`（**不是** `budget_exceeded`）。
   **不可表示的 calibrated clock 值**复用既有 **tick 域表示失败**（`budget_exceeded`），**不保留**独立
   令牌；入口违反"先捕获校准会话时钟"的顺序同样只走既有 `invalid_relation` 原子失败路径
   （§3.7.3 第 5 条）。
4. **不连续表示与连续输入能力复用既有码**（§3.7.7 表）：跨 gap / 重连 / 丢样与
   trajectory / `minimumReportRate` / reconstruction **都复用**既有 ABI 冻结码
   `input.continuous_unsupported`（类别 `capability_disabled`，`capabilityId = input.trajectory.v1`，
   `remediation = S7B-1`），靠 `field.path`（`continuityCapability` vs `discontinuity`）区分，
   映射到既有 **R-05**；**不新增 ABI 码、不新增 R 条目**。

### 9.4 运行期失败

按第 5 轮裁定（`S7A5-R03`，正文见 §3.14）的 **Ruleset Tick 三阶段**处置：

1. 稳态预算超限或 Ruleset 事务校验失败发生在**第二阶段（Ruleset state commit）**时：Fact Ledger
   **已提交且不可回滚**；StateDelta、Score / Combo / Statistics 与 RuleEffect **全部通过校验才提交**，
   否则**整 Tick 不提交**、**保留旧 state**，session 进入**可查询**的 `faulted` 状态并停止接受新的
   judgement mutation。
2. 失败时**不追加 fault Fact**、**不发布** RuleEffectEvent 与 PresentationEvent；失败原因只经诊断（§9.1）
   与 `faulted` 状态暴露。
3. **`faulted` 不可新建 snapshot**：处于 `faulted` 的 session 上 `submit` / `advance` / `seek` / `replay` /
   **就地 `reload`** 与 **`snapshot`** 一律**稳定失败**；只有**显式 reset** 或**创建替换新 session 的
   reload / recovery** 可以离开 `faulted`，且**不得恢复或伪造未提交的 StateDelta**（§3.14 第 4、5 条）。
4. PresentationEvent 可在**显式查询或恢复**时由已提交 Fact Ledger 与 FactBinding 重建；这不是把失败
   Tick 的表现事件发布出去（§3.14 第 3 条）。

**第二阶段 Ruleset state commit 失败的专用稳定码（第 6 轮 `CM-D04` / `P1-09` 的首次消费后续补齐，
决策卡 2026-10-02T21:03:39.462Z，`adopt` 0.96）。** 上文第 1 条的运行期失败**统一**使用已登记的稳定码
**`ruleset.transaction_failed`**（`category = invalid_relation`、`severity = error`、
`faulted = session_faulted`、首次消费 **S7A-5**），**不再**表述为"从 §9.2 / §9.3 的既有码表择一"。该码的
触发条件是第二阶段（Ruleset state commit）的原子校验不通过——含三类之外的 `RegisterKind`、重复
contribution identity、非交换 / 非结合合成、`exclusive` 的第二 owner / 第二写入、`ledger_derived` 的直接
StateDelta 写入，以及该阶段的稳态预算 / 溢出 / 冲突校验失败。本条的**行为不变**：Fact Ledger **已提交且
不可回滚**；StateDelta、Score / Combo / Statistics 与 RuleEffect **全部通过校验才提交**，否则**整 Tick
不提交**、**保留旧 state**，session 进入**可查询**的 `faulted` 并停止接受新的 judgement mutation；失败时
**不追加 fault Fact**、**不发布** RuleEffectEvent 与 PresentationEvent（第 2 条），`faulted` 上
`submit` / `advance` / `seek` / `replay` / 就地 `reload` / `snapshot` 一律稳定失败（第 3 条）。本码只登记为
**运行期 `faulted` 专有码**，**不新增 R 条目**（§7.2 仍 19 条）、**不新增类别**（§9.2 仍九类）、
**不新增 ABI 类型条目**；登记与校验落
**`schemas/cuexis.gameplay-diagnostics.v2.codes.json`** + CTest 校验项 `cuexis_gameplay_diagnostics_codes`。

### 9.5 待裁定项

诊断码表的**文件位置与校验门禁**已冻结（第 6 轮 `S7A6-R06`，2026-10-03）：机器可读表固定为
**`schemas/cuexis.gameplay-diagnostics.v2.codes.json`**，含每条稳定码的 `code` / `category` / `severity` /
`faulted`，并由 CTest 校验项 **`cuexis_gameplay_diagnostics_codes`** 校验 code 唯一性、`category` /
`severity` / `faulted` 映射完整性、`stableRejectCode` 已登记与输入 / 几何三码不可重复扩展（校验脚本
`cmake/VerifyGameplayDiagnosticsCodes.cmake`，可在 CMake 脚本模式下直接运行）。**仍未冻结/未闭合**的是：
该表的**逐条内容继续增补**（新增码仍须经登记 + 该 CTest 校验，输入 / 几何三码与九类不得扩展）、Replay
十类稳定错误与 V2 九类的**合并映射表**（见 §11）。

**引用口径与公共性判据（第 6 轮首次消费后续补齐，2026-10-03，决策卡 2026-10-02T21:51:38.591Z，
question `diagnostics-code-table-disposition`）。** ①**引用口径**：对本码表的引用一律用**章节引用**
（如 §9.1 / §9.6 / §9.9 与 ABI §错误与诊断映射），**行号只允许作为带日期的证据指针**、且**不得**用于
单行事实（行号会随文档增删漂移）；逐条替换见
[第 6 轮诊断分类学后续补齐记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-diagnostics-taxonomy-rulings.md)。
②**公共性判据**：一条诊断码串是否公开，只取决于它有没有**已文档化的公共映射行**（§7.2 拒绝条目行 /
§3.7.8 映射表行 / ABI 域 8 具名行），**`judgement.s7a2.*` 前缀不决定公开性**；判据、五条公共码的
"码 → 来源映射行 → 域 8 承载面"清单与"不新增 ABI 域 / 类型 / 错误枚举"的约束见 **§9.9**。

**第 6 轮更新（2026-10-03，`S7A6-R06`）。** 上句的**码表位置与校验方式已冻结**：集中码表固定落在
**`schemas/cuexis.gameplay-diagnostics.v2.codes.json`**，并新增一个 **CTest 校验项**
**`cuexis_gameplay_diagnostics_codes`**（§9.1 第 ①–④ 项；运行方式见 §9.1）。**逐条内容随各轮裁定逐条增补**
（本轮已增补 `ruleset.transaction_failed`、`presentation-target-missing`、`partial-group` 三条）；
仍然**未冻结**的是：Replay 十类稳定错误与 V2 九类的**合并映射表**（见 §11），以及尚未登记的候选字符串
（它们只能停留在 src-only，见 §9.6 第 3 条）。因此"码表文件位置 + 校验门禁 + 已裁定码的逐条登记"已闭合，
**尚未登记的字符串**仍属后续批次。

**第 6 轮首次消费后续补齐（2026-10-03，决策卡 2026-10-02T21:51:38.591Z，question
`diagnostics-code-table-disposition`）。** 该卡对码表落地后的处置给出逐项裁定，其中两项落在本节：
①**码串替代行号的引用口径**——本 Spec / ABI 对该码表的引用改为**章节引用**，行号只允许作为**带日期的
证据指针**且不得用于单行事实；②**公共性判据**——诊断码是否公开只取决于有无**已文档化的公共映射行**
（`judgement.s7a2.*` 前缀不决定公开性），判据与逐条清单见 **§9.9**。本节的"逐条内容继续增补"与
"合并映射表仍待裁定"两项结论**不因该卡改变**。

### 9.6 诊断码的登记约定（第 6 轮裁定，2026-10-03）

本节承载第 6 轮 `CM-D04` / `P1-09`（`S7A6-R06`）与 `P2-09`（`S7A6-R12`）的登记约定；首次消费批次
**S7A-7**。provenance：Codex 话题 thread `01a0fe61-3476-7882-9674-5f6b05035237`（consult，
`gpt-6-astra`），单卡 verdict `adopt`、confidence **0.97**，本地日期 2026-10-03。

**第 1 条（集中码表）。** 诊断码表是**唯一**的机器可读文件：
**`schemas/cuexis.gameplay-diagnostics.v2.codes.json`**。它承载每条稳定码的 `code`、`category`、
`severity` 与 `faulted` 行为。**不得**在公共头、ABI 文本或实现内再维护第二份码表。

**第 2 条（CTest 校验项）。** 与该文件配套新增**一个** CTest 校验项，校验四件事：① `code` 在表内
**唯一**；② **`category` / `severity` / `faulted` 映射完整**（每条码都有九类之一的 category、合法
severity 与明确的 `faulted` 行为）；③ 所有 `CapabilityRecord.stableRejectCode`（§5.6）**已在表中登记**；
④ **输入 / 几何三码不可重复扩展**——`input.continuous_unsupported`、`input.direction_unsupported`、
`geometry.inference_rejected` 各**只允许一条**，不得新增同义码或别名。

**第 3 条（进入公共 ABI 的前置）。** **除已冻结的 `input.continuous_unsupported`、
`input.direction_unsupported`、`geometry.inference_rejected` 与既有 R-09 映射外**，任何诊断码字符串
**必须先经集中码表登记并通过 CTest 校验**，才能进入公共 ABI（含 ABI 类型行与 §错误与诊断映射表）。
未登记的码字符串只能作为 internal / src-only 令牌，**不得**对外承诺。

**第 4 条（本轮登记的新码）。** 下列三条的**语义与登记义务**在本轮冻结，其**码表条目内容**由工具侧
随码表文件落地：

| 码 | 类别（九类之一） | 触发条件 | faulted |
| --- | --- | --- | --- |
| `identity_closure_incomplete` | `identity_closure_incomplete`（既有类） | 判定闭包内 target / domain / action / table 缺失，或 `compiledSemanticIdentity` 与 capability closure 未同时携带 / 不可比较 | 否（prepare 原子失败） |
| `presentation-target-missing` | `invalid_relation`（既有类） | **纯表现** target 缺失：允许空绑定、丢弃该投影事件 | **否**（不 fault session、不改 Fact Ledger） |
| `partial-group` | `invalid_relation`（既有类） | `groupCommit` 部分提交：已提交部分保留、进入稳定拒绝路径 | 否（correction 仅影响未提交窗口） |

**第 5 条（本轮码串、`severity` / `faulted` 与首次消费批次的定案；第 6 轮 `CM-D04` / `P1-09` 的首次消费
后续补齐，决策卡 2026-10-02T21:03:39.462Z，`adopt` 0.96）。** 上表两个表现码的**最终码串原样保留**为
`presentation-target-missing` 与 `partial-group`（**不加族前缀**、不改拼写）；二者与
`identity_closure_incomplete` 的最终登记取值如下，`severity` / `faulted` 的取值集见 §9.1：

| 码 | category | severity | faulted | 首次消费 |
| --- | --- | --- | --- | --- |
| `identity_closure_incomplete` | `identity_closure_incomplete` | `error` | `session_unaffected` | S7A-7 |
| `presentation-target-missing` | `invalid_relation` | `error` | `session_unaffected` | S7A-7 |
| `partial-group` | `invalid_relation` | `error` | `session_unaffected` | S7A-7 |
| `ruleset.transaction_failed` | `invalid_relation` | `error` | `session_faulted` | S7A-5 |

`ruleset.transaction_failed` 是 §9.4 运行期唯一 `faulted` 路径的专用码（见 §9.4 末段），**不新增 R 条目、
不新增类别**。四个码与其余已登记码一并落在 `schemas/cuexis.gameplay-diagnostics.v2.codes.json`，由 CTest
校验项 `cuexis_gameplay_diagnostics_codes` 校验。

### 9.7 判定闭包失败与表现失败的分界（第 6 轮裁定，2026-10-03）

现行kernel/Fold fault来源的适用范围见§9.8新增提示；本节表现失败不fault、不回写的边界保持。

本体承载 `Q-15` / `CM-P06` / `P1-13`（`S7A6-R04`）与 `CM-P08` / `P1-11`（`S7A6-R05`）的分界；
首次消费批次 **S7A-7**。

| 情形 | 处置 | 码 | 是否 fault session | 是否改 Fact Ledger |
| --- | --- | --- | --- | --- |
| **判定闭包**内 target / domain / action / table 缺失 | **prepare 原子失败**（不发布半成品、不改已有 active session） | `identity_closure_incomplete` 或 `invalid_relation` | 否（属 prepare 失败，不是运行期 `faulted`） | 否（未产生任何 Fact） |
| `compiledSemanticIdentity` 与 capability closure 缺一或不可比较 | **prepare 原子失败** | `identity_closure_incomplete` | 否 | 否 |
| **纯表现** target 缺失 | 允许**空绑定**、**丢弃该投影事件** | `presentation-target-missing` | **否** | **否** |
| `groupCommit` 部分提交 | 已提交部分**保留**（不回滚）、进入稳定拒绝路径 | `partial-group` | 否（进入**稳定拒绝路径**，不是 `faulted`） | 否（不回写；correction 仅影响未提交窗口） |

**判据。** "是否在判定闭包内"是唯一分界：**在判定闭包内**的悬空必须**原子失败**；**只在表现投影内**
的缺失允许**降级 + 稳定诊断**。任何表现侧失败（含 `presentation-target-missing` 与 `partial-group`）
**绝不回写 Fact Ledger**、**绝不改变判定结果或 Fact 顺序**、**绝不进入 `faulted`**（`faulted` 只由
§9.4 的 Ruleset state commit 失败产生）。

### 9.8 运行期 `faulted` 与表现失败的区分（第 6 轮裁定，2026-10-03）

**现行适用范围提示（2026-10-07）。** 下文“只有第二阶段Ruleset失败”的表述限定于原第5/6轮范围，
不能作为后来execution路径的全局fault白名单。现行kernel失败见[execution typed补充](../api/gameplay-v2-execution-types.md) §3，
kernel/fold/control的FaultStage及完整结果比较见本文J0-6首用补充，实际code/faulted映射以集中码表为准。
表现失败不fault、不回写Fact的边界保持；Playback的公共SessionState映射仍须在S7A-7首次消费合同中明确。

第 5 轮的 §9.4 `faulted` 行为**保持不变**（`S7A5-R03`）：只有**第二阶段 Ruleset state commit** 失败
才使 session 进入**可查询**的 `faulted`，且 `faulted` 上 `submit` / `advance` / `seek` / `replay` /
就地 `reload` / `snapshot` 一律稳定失败。**第 6 轮补充**：**表现层失败不产生 `faulted`**——
`presentation-target-missing`（纯表现 target 缺失）与 `partial-group`（`groupCommit` 部分提交）
都**不是** `faulted`：前者允许空绑定并丢弃该投影事件，后者进入**稳定拒绝路径**；两者都
**不得**把 session 置为 `faulted`、**不得**阻止后续 `submit` / `advance`、**不得**回写 Fact Ledger。
把表现失败误判为 `faulted`（或反之）是本节的显式反例。

### 9.9 诊断码公开性的判定依据（2026-10-03，question `diagnostics-code-table-disposition`）

本节是**诊断码公开性**的独立裁定正文。它不新增 layer、不新增类别、不新增 R 条目，也不新增 ABI 域 /
类型 / 错误枚举；它只回答"**哪些诊断码串是公共码，凭什么**"。

**第 1 条（判定依据：具名码串、适用场景与九类映射可在正典文本中明确相互追溯）。** 一个诊断码串是
**公共诊断码**，当且仅当它的**具名码串**、**适用场景**与**九类映射**在正典文本中**可明确相互追溯**——
即三者都能在本文 §7.2 的拒绝条目行、本文 §3.7.8 的映射表行或 ABI 域 8 的具名行中读到，且彼此**有明确的
交叉指向**。**场景行与码串行分处不同章节是允许的**（例如场景行在 §3.7.8、码串行在 §9.3 或 ABI），只要
二者**显式互相指向**、读者可无歧义地把它们对起来。**名称前缀不决定公开性**：`judgement.s7a2.*` 前缀既不使
其私有，也不使其公开。**不设 owner 命名条件**——公开性不由任何人的命名行为决定。反之，**无法**如此追溯的
码串**不得**作为公共码——它只能作为 internal / src-only 令牌存在。因此本规范里"公开"的判据是**正典文本中
可追溯的相互对应**，不是命名，也不是位置。

**第 2 条（公开码的既有承载面，不新增任何 ABI 面）。** 公共诊断码一律通过**既有**诊断码承载面消费：
`JudgementError` / `DiagnosticCode`（**ABI 域 8**）与 7A 既有的稳定拒绝载荷；**不新增 ABI 域、不新增
类型、不新增错误枚举**。`DiagnosticCategory` 仍取 §9.2 的九类之一，`severity` / `faulted` 仍取 §9.1 的
闭集。本轮的 29 条已登记码全部适用本条，其中 §9.9 第 4 条单独列出五条的核对结果。

**第 3 条（其余已登记码的公共性依据）。** 第 4 条五条之外，集中码表 `codes[]` 其余 24 条已登记码的公共性
依据是本文 §7.2 的 R 条目行、§3.7.8 的映射表行或 ABI 域 8 的具名行，或 `2026-10-02T21:03:39.462Z` /
`2026-10-02T21:51:38.591Z` / `2026-10-02T22:02:31.461Z` 三张裁定卡的登记；逐条依据见
**`schemas/cuexis.gameplay-diagnostics.v2.codes.json`**（机器可读权威，由 CTest 校验项
`cuexis_gameplay_diagnostics_codes` 校验），本 Spec 不复制其内容。

**第 3 条附则（公共承载面与 src-only 令牌的区分，必须与第 3 条一起读）。** 该机器可读表用
`publicConsumptionSurface` **区分**二者：**非空**的 `publicConsumptionSurface` **只**出现在 `codes[]`
的已登记公共码上；`pendingRegistration[]` 的**每一条**都写 `publicConsumptionSurface: null`——无论它是
13 条 src-only 令牌（`status = not_registered_src_only`）还是 7 条尚未选出码串的空位（`code = null`）。
因此"公共承载面字段覆盖 29/29"**不得**被读成"29 条以外也公开"或"表内一切都公开"：**公共码 29 条、
src-only 令牌 13 条、空位 7 条是三个不同数字**，校验器分别检查并要求两侧一致。

**第 4 条（"码 → 来源映射行 → 域 8 承载面"逐条核对清单）。** 下表是本条的**核对结果**；每一行都已在
本轮回原文逐条核对。

| 码 | 来源映射行（已文档化） | 既有承载面 | 该行给出的类别 |
| --- | --- | --- | --- |
| `judgement.s7a2.timebase.tick_overflow` | §3.7.8 映射表 **Tick 溢出行** | `JudgementError` / `DiagnosticCode`（ABI 域 8） | `budget_exceeded` |
| `judgement.s7a2.timebase.time_reversal` | §3.7.8 映射表 **校准会话时钟回退**行（"时间次序关系错误 → `invalid_relation`"结论亦见 §9.3 输入半批映射表第 3 行） | `JudgementError` / `DiagnosticCode`（ABI 域 8） | `invalid_relation` |
| `judgement.s7a2.input.amount_out_of_range` | §3.7.8 映射表 **`DomainAmount` 量越界 / 窄化 / 量化溢出**行（行文以场景命名、未逐字带码串；码串见 ABI §数值域与量程"判定顺序：可表示性先于范围"） | `JudgementError` / `DiagnosticCode`（ABI 域 8） | `budget_exceeded` |
| `judgement.s7a2.input.amount_narrowed` | 同上（同一"可表示性先于范围"映射行；§9.3 输入半批映射表第 2 行给出"窄化 → `budget_exceeded`"） | `JudgementError` / `DiagnosticCode`（ABI 域 8） | `budget_exceeded` |
| `judgement.s7a2.input.same_tick_collision` | §3.7.8 映射表 **同一 canonical 身份在同一 Tick 再次入场**行（行文以场景命名、未逐字带码串；码串见 §9.3 输入半批映射表第 1 行） | `JudgementError` / `DiagnosticCode`（ABI 域 8） | `invalid_relation` |

**第 5 条（核对中的两处口径说明，必须随本条一起读）。** ①第 2、5 行的 §3.7.8 行文给的是**场景**、不是码串
本身，码串逐字出现在 §9.3 的输入半批映射表；②第 3、4 行的 §3.7.8 行文同样以场景命名，码串逐字出现在
ABI §数值域与量程的"判定顺序：可表示性先于范围"段。**两处都不构成"缺映射行"**：场景行与码串行指向同一
映射，且都在已冻结的 §3.7.8 / §9.3 / ABI 正文内。**除这两条口径说明外，本轮未发现任何一条缺真实映射行**；
因此**未新增、也不得新增任何映射行**。

## 10. Replay / Snapshot / Schema 的契约边界

以下工件**尚不存在**，其创建属接受后的独立工作项，**不是**建立替代关系的最低前置
（[RW §0.2 执行顺序裁决](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)）：

| 工件 | 现状 | 本 Spec 的登记 |
| --- | --- | --- |
| Chart v5 / `gameplay` 区段 Schema | 不存在 | 待创建；字段合同以本 Spec 为准 |
| Replay header + 事件流 Schema（或等价 typed 合同） | 不存在 | 待创建；**字段集已由第 5 轮冻结**（`S7A5-R06`，见 §3.18）；字节布局属 S7A-6 |
| Snapshot header + 全量语义状态 Schema | 不存在 | 待创建；**字段集与 `SnapshotPayload` 闭包已由第 5 轮冻结**（`S7A5-R06` / `S7A5-R07`，见 §3.18）；字节布局属 S7A-6 |
| diagnostics 码表 | **已落地**为 `schemas/cuexis.gameplay-diagnostics.v2.codes.json`（第 6 轮冻结位置与校验门禁，`S7A6-R06`；逐条内容随各轮裁定增补） | 机器可读码表 + CTest 校验项 **`cuexis_gameplay_diagnostics_codes`**（校验脚本 `cmake/VerifyGameplayDiagnosticsCodes.cmake`；校验 code 唯一性、`category` / `severity` / `faulted` 映射完整性、`stableRejectCode` 已登记、输入 / 几何三码不可重复扩展）；**未登记的码字符串只能停留 src-only**（CM-D04、§9.1、§9.5、§9.6） |
| Ruleset package Schema | 不存在（**第 5 轮已裁定 7A 不含 package**，见 §3.16） | package 形状 / identity / 迁移推迟到 S7C-2 |
| Schema 校验入口 | 既有入口不含 V2 | 待新增或复用并注册为 CTest |

契约边界（在工件创建前仍然有效）：

1. **Replay 只保存规范化 Observation**，不允许直接注入 Fact；解码不执行 IO / 脚本 / Pattern /
   solver。Replay 头部的字段集见 §3.18（**第 5 轮冻结**）：`formatVersion`、四分量
   `JudgementIdentity`、`NormalizationProfile` metadata、effective late-policy metadata、
   `eventCodecId`、normalized event count、encoded byte count 与 `ReplayDecodeBudget`。
2. **实时输入与 Replay 输入走同一** normalize / submit / advance 路径；原始设备事件可留在诊断，
   但不是 Replay 语义输入。
3. **Snapshot 采用全量语义快照**；必须无损保存活动实例、Pattern 状态、Release/tail phase、
   exclusive resource 状态、finalization watermark、未提交窗口、`preparedGrace`、离散事件
   sequence 状态、Hook 快照、Fold 状态、统计、pending signal queue、Fact cursor 与
   session / fault state。**字段集的权威列表与不得保存清单见 §3.18 第 4、5 条**（第 5 轮冻结）。
4. **表现缓存、Animation layer 临时值与 HostOverride token 不进入 Snapshot**；Seek 后由已提交
   Fact Ledger 与 FactBinding 重建 PresentationEvent（不得保存清单见 §3.18 第 5 条）。
5. **Seek 必须逐位等于从起点运行**；快照间隔是引擎内部参数，不进入 identity；`maxSeekLatency`
   是引擎承诺，会话只能收紧，不得事后放宽。**具体 `maxSeekLatency` 数值禁止进入 Schema / header /
   运行时约束 / 对外承诺**（`S7A5-R08`，见 §3.19）。
6. **`faulted` 不可新建 snapshot**：处于 `faulted` 的 session 上 `snapshot` 稳定失败，`submit` /
   `advance` / `seek` / `replay` 与就地 `reload` 同样稳定失败（§3.14 第 4 条）；session 只能被**显式
   reset** 或**创建替换新 session 的 reload / recovery** 替换；恢复旧快照不得伪造未提交的
   StateDelta。**快照的状态闭包仍包含 fault 诊断 / 状态**（§3.18 第 4 条）——那是**快照的内容**，
   不是"`faulted` 可以产出新快照"。

## S7A-1 限定冻结范围与登记规则（2026-10-02）

**顶层状态仍为 `candidate`。** 本 Spec 不改为 `frozen` / `active`：授权来自**范围精确的限定冻结**，
不来自状态词。按 [Stage 7总计划](../stage_plans/active/stage-07/plan.md#11-权威输入和冻结顺序) §1.1 冻结顺序第 2 步的
**分批冻结**口径（2026-10-02 owner 接受第 1 轮补充 S1-01…S1-05，登记见 [RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §7A；
该次准入咨询 thread `s7a1-admission-rulings` 记入 [S7A-0 接受记录](../stage_reports/stages/stage-07/readiness/2026-10-02-s7a-0-acceptance.md) §4.2），
本 Spec 只在下列四项范围内为 S7A-1 骨架提供依据：①权威来源与版本层级；②类型角色与边界；③模块与
安装边界；④既有所有权与异常边界承诺。**共同基线标识**：`RULING_WORKSHEET §7A` 的 `S1-01…S1-05`（[ADR 0044](../adr/0044-gameplay-v2-semantic-kernel.md) 的"接受门禁"第 6 条引用同一编号）。

**范围精确的授权表述。** 在**本节限定的范围内**授权 S7A-1 骨架实施；范围外（含未列出的字段表示、
整数宽度、序列化字节与线格式、枚举集、数值/限额，以及第 6、7 轮尚未裁定的运行语义；第 2、3、4、5 轮已
裁定的运行语义由各自小节授权，同样不在本节范围内）仍不授权。

| 范围 | 本 Spec 中属本批次的部分 | 本批次之外 |
| --- | --- | --- |
| ① 权威来源与版本层级 | §1.1 第 1 / 3 项、§1.2、§1.4；§2.1–§2.3、§2.5；§3.1 图归属；§4 Requirement identity 元组 | §2.4 互换性矩阵细则；§3.6 中 `gameplay-graph` entry 的 manifest closure / 预算 / 拒绝编码（第 6 轮 Q-06）；Chart v5 `gameplay` 区段与 Replay / Snapshot / diagnostics Schema 工件 |
| ② 类型角色与边界 | §5.1 表结构与 §5.3 三条判定的分别声明（第 1 轮 Q-10 已接受）；§6.1 五来源与唯一派生链、§6.2 稳定失败 | §5.2 逐字段归属表各行的最终取值（随 §11 表对应轮次补全）；§6.4 registry 字段命名；枚举集、字段集与整数宽度 |
| ③ 模块与安装边界 | 不属本 Spec（属 plan §S7A-1 与 [TARGETS_AND_DEPENDENCIES.md §9](../proposals/gameplay-v2-acceptance/TARGETS_AND_DEPENDENCIES.md)）；本 Spec 不重复该内容 | 同左；target / allowlist 与安装树证据在批次内核对 |
| ④ 所有权与异常边界承诺 | 不属本 Spec（属 [Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) §所有权与生命周期） | 所有权细分与快照字段表（与实现批次一起冻结） |

**§1.1 第 2 项标注的运行语义不在本批次冻结范围内。** 时间域与 Tick 阶段（第 2 轮）、仲裁与资源
（第 4 轮）、Fact Ledger 与 Ruleset 事务（第 5 轮）、表现桥接（第 6 轮）在对应章节写出并裁定前，
本 Spec 只登记边界与身份归属，不构成可实施定义，也不因 S7A-1 的受限授权而进入实现。轮次与批次的
对应关系见 [未决语义分轮裁决清单 §0.1 与 §9](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)
与本文 §11。预算数值同样不在本批次（§8.2–§8.4，S7A-9 前保持未冻结）。

## S7A-2 限定冻结范围与登记规则（2026-10-03）

**顶层状态仍为 `candidate`。** 本 Spec 不改为 `frozen` / `active`：授权来自**范围精确的限定冻结**，
不来自状态词。按 [Stage 7总计划](../stage_plans/active/stage-07/plan.md#11-权威输入和冻结顺序) §1.1 冻结顺序第 2 步的**分批冻结**
口径，第 2 轮（时间域与迟到策略）在 2026-10-03 由 Codex 裁定并经 owner 落进本文；该轮是进入 S7A-2 的
**准入门禁**，其语义正文见 §0.1、§3.7 与 §7.2 的映射段。

**provenance。** 决策卡 thread `s7a1-freeze-bindings`，verdict `adopt`，confidence 0.99，日期 2026-10-03，
模型 `gpt-6-astra`；补充确认卡 thread `cm-x01-disposition`，verdict `adopt`，confidence 0.99，日期
2026-10-03（`CM-X01` 保持 `open`，不改任何计数）。**本轮的 stop 语义经 2026-10-03 实现复核由两张续裁卡
修订**（同一 thread `s7a1-freeze-bindings`，verdict 均为 `adopt`，confidence **0.98** / **0.99**）：
`stop` 模型由"比例替换"修订为**塌缩 / 跳变 + 精确累计函数**（§3.7.2 第 3 条），并补三分法与阻塞项登记
（§3.7.4 第 5 条、§3.7.7 表、§9.3）；修订不改变计数（§7.2 仍 19 条、§9.2 仍九类、契约处置词不变）。
**本轮的输入规范化半批语义另经 2026-10-03 两张续裁卡复核与补充**（同一 thread `s7a1-freeze-bindings`，
verdict 均为 `adopt`，confidence **0.97** / **0.99**）：`AmountSpec` 的 canonical 整数宽度**明文冻结**为
有符号 64 位（§3.7.6 第 5 条）、判定顺序为**可表示性先于范围**（§3.7.6 第 6 条）、
`same_tick_collision` / `time_reversal` → `invalid_relation` 与 `amount_narrowed` → `budget_exceeded`
三条原子失败映射（§3.7.8 表、§9.3）、负 `observationTick` 合法且只拒回退（§3.7.3 第 4、5 条）、
**域声明集合与完整 `AmountSpec` 字段进 session identity**（§5.2 行）、**不连续表示复用既有
`input.continuous_unsupported`**（§3.7.7 表、§7.2）。复核同样**不改变任何计数**（§7.2 仍 19 条、
§9.2 仍九类、契约处置词不变）。
带日期的落地证据见
[第 2 轮门禁报告](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-2-gate-rulings.md)、
[第 2 轮实现裁定记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-2-implementation-rulings.md)与
[第 2 轮输入半批实现裁定记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-2-input-implementation-rulings.md)。

**共同基线标识**：`RULING_WORKSHEET §7A` 的 `S1-01…S1-05`（[ADR 0044](../adr/0044-gameplay-v2-semantic-kernel.md)
的"接受门禁"第 6 条引用同一编号），与 S7A-1 节共用同一基线，不另立第二套编号。

**范围精确的授权表述。** 在**本节限定的范围内**授权 S7A-2 按第 2 轮裁定实施时间域与迟到策略语义；
范围外（含 §3.7.7 表列各项：具体 late-policy 数值、具体业务量程限额、S7C-1 扩展字段、连续输入能力、
所有预算数值，以及第 6、7 轮尚未裁定的运行语义）仍不授权（第 3、4、5 轮的语义另见
`## S7A-3 限定冻结范围与登记规则（2026-10-03）`、`## S7A-4 限定冻结范围与登记规则（2026-10-03）` 与
`## S7A-5` / `## S7A-6` 两节，
**不在本节授权范围内**）。

| 范围 | 本 Spec 中属本批次的部分 | 本批次之外 |
| --- | --- | --- |
| ① Tick 宽度、单位与溢出（Q-04） | §0.1 四域术语；§3.7.1 有符号 64 位整数、单位由 `engine.tick.us.v1` 的 `TimebaseProfile` 声明、禁止隐式换算、溢出稳定拒绝 | 序列化字节与线格式；逐字段表示的最终命名（与实现批次一起冻结） |
| ② RationalBeat 映射与 tie rule（Q-04 / CM-T03） | §3.7.2 **精确累计函数 `F`**（`F(originBeat) = 0`、`F(endBeat) = F(startBeat) + duration`、`tick(beat) = roundHalfToEven(F(beat))`、**只舍入一次**）、`stop` 塌缩与**非单射**、半开跳变方向（正向 `(originBeat, beat]`、负向 `(beat, originBeat]`）与"有 stop 时失去奇对称"、精确有理数、半数取偶、tempo/stop/负 Beat 同一规则、`(tick, originKind, canonicalOrdinal)` 排序、禁止按 ingress 顺序排序；Tick → Beat 反查契约登记为**首次消费它的后续批次**阻塞项（§3.7.2 第 6 条） | 映射的代码形状、参考实现与 golden 文件（属 S7A-2 实现批次）；Tick → Beat 反查的**实现** |
| ③ `observationTick` 来源与 S7C-1 边界（Q-04 / CM-T04） | §3.7.3 校准后会话时钟为唯一 canonical source；设备 / 到达 / 音频 / 渲染帧时间仅诊断上下文；S7C-1 只能扩展 `CalibrationProfile` 参数；**负值在有符号 64 位域内合法并参与单调排序，只有校准时钟回退被拒且映射 `invalid_relation`**；**不可表示的 calibrated clock 值复用 tick 域表示失败（`budget_exceeded`）、不保留独立令牌**，入口顺序违规只走既有 `invalid_relation` 路径（2026-10-03 输入半批复核） | S7C-1 的扩展字段集与 `CalibrationProfile` 逐参数定义（后续批次阻塞项）；**负 tick 的拒绝路径**不属本批次（负值不拒绝） |
| ④ late policy 类型化（CM-T08 / P1-04） | §3.7.4 typed 参数、默认值只在 profile registry 记 `pending_measurement`、缺失或未测量在 prepare 稳定拒绝（`validatePrepare` **只**检查"已声明且已测量"）、重复排队稳定拒绝 + 诊断码 | **具体 late-policy 数值**（watermark、queue hop、窗口开闭阈值）：后续批次阻塞项，不得写成 ABI 常量、隐式零值或研究切片限额；**参数量级关系的校验**登记为**阻塞 S7A-4 的 late-window / 判定消费门禁**（§3.7.4 第 5 条、§3.7.7 表），最终数值与限额记 **S7A-9** |
| ⑤ `commitTick` / commit window（P2-03） | §0.1 与 §3.7.5 的定义：`commitTick` 是事务提交发生的 Tick；window 是相邻两个可提交 Tick 之间的观察区间；事实只能在 `commitTick` 提交 | 提交事务的字段布局与 Snapshot / Replay 承载（第 5 / 6 轮） |
| ⑥ `AmountSpec` 与越界处置（CM-T13） | §3.7.6 typed `AmountSpec`（canonical 整数量化 / scale / 可表示范围 / 边界策略）、**canonical 整数宽度明文冻结为有符号 64 位**（第 5 条）、精确整数运算、最近值半数取偶、**判定顺序＝可表示性先于范围**（第 6 条）、超范围 / 窄化 / 溢出稳定拒绝 + 诊断码；**域声明集合与完整 `AmountSpec` 字段进 session identity、域重排不改变 identity、7A 不引入 per-domain 版本字段**（§5.2 行，2026-10-03 输入半批复核） | **具体业务量程数值与限额**：后续批次阻塞项，本轮不冻结；**per-domain 版本字段**不在 7A（会话级版本由 `profileVersion` 承载） |
| ⑦ 稳定拒绝登记（§7.2） | §3.7.8 既有六类全部映射到既有条目，**加输入半批的三条原子失败映射**（`same_tick_collision` / `time_reversal` → `invalid_relation`、`amount_narrowed` → `budget_exceeded`）；不连续表示与连续输入能力复用既有 `input.continuous_unsupported`（R-05，`field.path` 区分）；§7.2 仍为 19 条 | 新增 R 编号必须另立裁决；**不新增 ABI 码** |

**S7A-2 不得消费清单（实现须逐条对齐）。** 按 S1-05 的"不得用默认值、临时 typedef、序列化编码或
伪成功实现绕过阻塞"口径，S7A-2 实现**不得消费**：浮点或宿主数学库参与 Tick 判定；隐式秒 / Beat → Tick
转换；未校准原始时间作为 `observationTick`；按 ingress 顺序解决同 Tick 冲突；late-policy 数值默认常量；
饱和或未定义溢出；未声明的 `DomainAmount` 范围 / 量化；临时整数 typedef；未冻结序列化编码；连续轨迹 /
minimumRate / reconstruction / discontinuity 表示；S7C-1 扩展字段；研究切片限额。逐条判定与证据见
[第 2 轮门禁报告](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-2-gate-rulings.md)。

**输入规范化半批的登记（2026-10-03 输入半批复核）。** 本批次输入半批的语义裁定与落点如下（provenance：
同一 thread `s7a1-freeze-bindings`，两张续裁卡 verdict 均为 `adopt`，confidence **0.97** / **0.99**，
按**本地日期 2026-10-03** 登记；带日期证据见
[第 2 轮输入半批实现裁定记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-2-input-implementation-rulings.md)）：

1. `AmountSpec` 的 canonical 整数宽度**明文冻结**为有符号 64 位（§3.7.6 第 5 条），判定顺序为
   **可表示性先于范围**（第 6 条）；**不得**靠"复用 Tick 宽度"默示。
2. 三条原子失败映射写入 §3.7.8 表与 §9.3：`same_tick_collision` → `invalid_relation`、
   `amount_narrowed` → `budget_exceeded`、`time_reversal` → `invalid_relation`；**同一 canonical 身份
   在同一 Tick 再次入场属于输入身份 / 次序关系非法**，与迟到策略的重复排队（`late_policy_incomplete`）
   分开。
3. 负 `observationTick` 合法并参与单调排序；只有**校准会话时钟回退**被拒（`invalid_relation`）；
   **不可表示的 calibrated clock 值**复用既有 tick 域表示失败（`budget_exceeded`），**不保留独立令牌**；
   入口顺序违规只走既有 `invalid_relation` 路径（§3.7.3 第 4、5 条）。
4. **域声明进 session identity**（§5.2 行）：分量为 `profileId` / `profileVersion` / `sourceClass` +
   规范化后的域 token 集合与每项域声明的完整 `AmountSpec` 字段；**域重排不改变 identity**，增删域或
   改动其 `AmountSpec` **是** identity 变化；**7A 不引入 per-domain 版本字段**，**不得**推迟到 S7A-4。
5. 不连续表示与连续输入能力**都复用**既有 `input.continuous_unsupported`（`capability_disabled` /
   `capabilityId = input.trajectory.v1` / `remediation = S7B-1`），靠 `field.path` 区分，映射既有 **R-05**；
   **不新增 ABI 码、不新增 R 条目**（§3.7.7 表、§7.2）。
6. **计数不变**：§7.2 仍 **19 条**、§9.2 仍**九类**、契约处置词不变。

**§1.1 第 2 项标注的运行语义仍不完整。** 本节只冻结时间域与迟到策略；Tick 阶段与仲裁、资源（第 4 轮，
已裁定并见 `## S7A-4 限定冻结范围与登记规则（2026-10-03）`）、Fact Ledger 与 Ruleset 事务（第 5 轮）、
表现桥接（第 6 轮）在各自章节写出并裁定前，本 Spec 仍只登记边界与身份归属，不构成可实施定义。
预算数值同样不在本批次（§8.2–§8.4，S7A-9 前保持未冻结）。

## S7A-3 限定冻结范围与登记规则（2026-10-04 更新）

**顶层状态仍为 `candidate`。** 本 Spec 不改为 `frozen` / `active`：授权来自**范围精确的限定冻结**，
不来自状态词。按 [Stage 7总计划](../stage_plans/active/stage-07/plan.md#11-权威输入和冻结顺序) §1.1 冻结顺序第 2 步的**分批冻结**
口径，第 3 轮（prepare、装配与 entry）于 2026-10-03 由 Codex 裁定并经 owner 落进本文；该轮是进入 S7A-3 的
**准入门禁**，其语义正文见 §1.3、§2.5 第 1 条、§3.3、§3.8 与 §5.5，拒绝面映射见 §7.2 的第 3 轮段。

**provenance。** 三张决策卡来自同一 Codex 话题 thread `s7a3-prepare-rulings`（模型 `gpt-6-astra`，consult）：
①主裁定卡 verdict `need_info`，confidence 0.8（本轮 5 条裁定与规范字节边界）；②补答 `CM-X05` 的卡 verdict
`reject`，confidence 0.88（`CM-X05` 处置与配套动作）；③计数一致性确认卡 verdict `adopt`，confidence 0.99
（`open` 16 / `accept` 36 / `revise` 26，总数仍 114；§0.1 末列仍保留 `CM-X05`、`CM-C10`）。三卡的文件名时间戳
为 2026-10-02T19:4xZ（UTC），本轮统一按**本地日期 2026-10-03** 登记（与第 2 轮先例一致）。带日期的落地证据见
[第 3 轮门禁报告](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-3-gate-rulings.md)。

**共同基线标识**：`RULING_WORKSHEET §7A` 的 `S1-01…S1-05`（[ADR 0044](../adr/0044-gameplay-v2-semantic-kernel.md)
的"接受门禁"第 6 条引用同一编号），与 S7A-1 / S7A-2 两节共用同一基线，不另立第二套编号。

**范围精确的授权表述。** 在**本节限定的范围内**授权 S7A-3 按第 3 轮及 2026-10-04 设计裁决实施 prepare、装配与 entry 的
**行为**边界以及 Gameplay Capsule v2 的 Packed 物理合同；范围外（含 §3.8.4 的 `boundedRelationInstance` 候选表示、
§5.5 的内部规范字节编码实现、§3.8.5 表列各项，以及第 6、7 轮尚未裁定的运行语义）仍不授权（第 4、5 轮的
语义另见 `## S7A-4 限定冻结范围与登记规则（2026-10-03）` 与 `## S7A-5` / `## S7A-6` 两节，
**不在本节授权范围内**）。

| 范围 | 本 Spec 中属本批次的部分 | 本批次之外 |
| --- | --- | --- |
| ① Requirement 不含 Gameplay `effects`（Q-05） | §3.3 的禁止内容与 Q-05 段；§3.8.1 的三条规则（不含 `effects`；非空 `effects` 只能显式 lowering 为不影响 Judgement / Replay 的 Presentation 数据；否则拒绝） | `effects` 的 lowering 表与 Presentation / Effect Graph 的逐字段映射（属 presentation closure 与后续批次） |
| ② `REQ0` / `CNS0` 兼容边界（Q-07） | §2.5 第 1 条与 §3.8.2 的行为规则；2026-10-04 起 Gameplay Capsule v2 的 revision、section、字段、编码与拒绝矩阵 | `boundedRelationInstance` 候选表示及后续 codec；实现证据仍待 S7A-3 完成 |
| ③ Release / tail 显式声明（Q-18 / P2-10） | §3.8.3：Release / tail 仅作同一 Requirement 显式声明的可选 phase、不自动生成独立 Requirement、内容要求 tail 语义却未声明即拒绝、`P2-10` 随 `Q-18` 关闭 | Release / tail 的 Snapshot / Replay 承载与逐字段 phase 表（第 5 / 6 轮） |
| ④ 有界循环完全展开（CM-C10 / P1-03） | §3.8.4：prepare-time 完全展开、展开结果唯一决定 identity / snapshot / fact order、`boundedRelationInstance` 不进 7A 合同且遇则稳定拒绝 | **`boundedRelationInstance` 候选表示**（7B+ / S7C）与具体 `stableRejectCode`：只阻塞首次拟消费它的后续批次；表示与拒绝码在该候选首次消费前闭合，S7A-3 不消费 |
| ⑤ identity 规范字节边界（张力点裁定） | §5.5：规范字节是 prepare 内部确定性派生物、不是公共 ABI 或 Packed 编码；冻结语义等价 / 排序 / 身份域边界；同一算法跨工具链产出相同字节并以跨工具链 golden、输入置换、file / memory 对照验证 | **编码本身暂不冻结**；**编码实现须在 S7A-3 消费前闭合**；ABI 仅声明对外 identity 的不透明性 |
| ⑥ `CM-X05` 的 D-3 落点（W 类缺口） | §1.3：字段定义与登记项在 SUPPORT §6、定义 / 登记 / 引用规则在 plan §5.3、**本 Spec 仅引用、不设登记表附录**；`CM-X05` 已由 D-3 落地关闭、不再阻塞 S7A-3 | 具体 W 缺口在**首次消费前**由消费批次补全字段并按 `S1-03` 标注裁决编号与首次消费批次（[SUPPORT §6.1](../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md)） |
| ⑦ 拒绝面登记（§7.2） | §7.2 第 3 轮段：三类拒绝不新增 R 条目，**本清单仍为 19 条** | 新增 R 编号必须另立裁决 |
| ⑧ S7A-3 准入门禁（第 7 轮，`P1-01` / `P1-02` / `P1-15`） | §3.8.7 的三组规范文本：**静态 typed 判定域记录**（几何属 Gameplay closure、坐标系在判定域内声明量程、与 Presentation transform 完全隔离、动态 frame 为 7B+ 候选并稳定拒绝）；**CXT local relation 合并规则**（prepare 编译期完成、稳定 ID = 来源文档 identity + 声明序号、同名拒绝、跨 invocation 引用显式、按稳定 ID 排序使重排不改结果）；**Chart v5 inline 与 CXT v2 emission 的等价性**（canonical graph 与 derived capability closure 逐字段相等、忽略 `sourceMap` 与物理顺序、逐字段 semantic diff 作为 S7A-3 的 golden 证据） | 动态 frame 判定（7B+）、CXT 合并的**编码表示**与 semantic diff 的**机器格式**（属实现与后续批次；diff 的 golden 内容由 S7A-3 产出） |

**S7A-3 不得消费清单（实现须逐条对齐）。** 见 §3.8.5；该清单与
[Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) 的 `## S7A-3 限定冻结范围与登记规则（2026-10-03）` 一节一致。
逐条判定与证据见 [第 3 轮门禁报告](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-3-gate-rulings.md)；
第 7 轮的准入门禁（`P1-01` / `P1-02` / `P1-15`，`S7A7-R01` / `S7A7-R02` / `S7A7-R06`）见 §3.8.7 与
[第 7 轮收尾裁定与缺陷处置记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-7-wrapup-rulings.md)。

**§1.1 第 2 项标注的运行语义已于第 7 轮补齐。** 本节冻结 prepare / 装配 / entry 的行为边界与 identity 的
规范字节边界；Tick 阶段与仲裁（第 4 轮）、Fact Ledger 与 Ruleset 事务（第 5 轮）、表现桥接与诊断（第 6 轮）
与收尾澄清（第 7 轮）均已在各自章节写出并**已裁定**（**第 1–7 轮无待裁定轮次**）。**裁定完成不等于实现完成**：
本批次仍需按 §3.8.5 的不得消费清单与 §3.8.7 的准入门禁实施，预算数值同样不在本批次（§8.2–§8.5，S7A-9 前
保持未冻结）。

**实现批次（第一、二半）的语义与门禁登记。** S7A-3 实施过程中裁定的语义澄清与门禁状态变更（`skip` /
`instant` 的语言同一与身份区分、`complement` 的前缀消费与可组合性、有界循环的份数序与可空操作数展开、
三项 Pattern 计数口径、arm / deadline 包含性门禁**未完成**、grade 只携带不求值）登记在 §3.8.4 第 5、6 条、
§3.8.8 与 §8.2，并附带日期证据
[第 1、2 半实现裁定记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-3-implementation-rulings.md)。
登记**不改变任何计数**（§7.2 仍 19 条、§9.2 仍九类、契约处置词与 ABI 行不变）。

**第 2 轮实现复审的改正（2026-10-03）。** 同一记录 §15 登记第 2 轮复审（thread
`01a0fe91-416c-7f31-a48a-d1890a0e68d1`，verdict `reject`，confidence **0.91**，裁决卡
`2026-10-02T22-33-54-872Z`，模型 `gpt-6-astra`）的四项改正：①**计数缺口不是拒绝理由**——完全展开计数、
每分支上界与 §8.2 状态数无可表示值时只记**测量缺口**，**只有已接受上限被确实超过**才按 `budget_exceeded`
拒绝（§8.2、§3.8.8 第 5 条）；②**状态预算门禁保持 incomplete**（§3.8.8 第 8 条）；③**包含性门禁的装配
消费点**是 S7A-3 验收与进入 S7A-4 之前的**硬前置**（§3.8.8 第 6 条）；④**码表登记不得永久豁免**
（§3.8.8 第 7 条）。两条已登记风险（声明树身份投影与完全展开语义、`complement` 无有限上界时的保守拒绝）
见 §3.8.4 第 7 条与 §3.8.8 第 3 条；二者均为**实现验收风险**，不是待 owner 选择的语义项。本轮**不新增** R 条目、ABI 类型或诊断类别，诊断码
相对本批次基线**净增 0 条 / 净删 2 条**。

**第 3 轮实现复审的改正（2026-10-03）。** 同一记录 §16 登记第 3 轮复审（同一 thread
`01a0fe91-416c-7f31-a48a-d1890a0e68d1`，Codex 会话 `01a1006a-d7ca-7300-b6d9-67dedf427789`，verdict
`reject`，confidence **0.93**，裁决卡 `2026-10-03T06-20-24-581Z`，模型 `gpt-6-astra`）的两项改正：
①**可证明下界已超过已接受上限**的路径**恢复**为 **`budget_exceeded` 内容拒绝**（**仅**构造失败与**非下界**
计数记为测量缺口、不得拒绝；无已接受上限则不比较、不拒绝）——§8.2 界分第 1 至 3 句、§3.8.8 第 5、8 条、
§3.8.4 第 6 条；②**包含性门禁的保守拒绝与预算缺口分成两道独立门禁**，前者不是内容超限诊断、也不改变状态
预算门禁的 incomplete 记录——§3.8.8 第 3 条、§8.2 界分第 5 句。**判据是"该计数是否可证明为下界"，不是
"是否溢出"**；状态维度的下界由**声明**推出（最长可接受轨迹长度 `L` 有限 ⇒ 状态数 ≥ `L + 1`），不依赖本
模块的构造。本轮**不新增不删除**诊断码（复用既有 `budget_exceeded`），故诊断码相对本批次基线仍**净增 0 条 /
净删 2 条**；R 条目仍 19、§9.2 仍九类、ABI 行不变。**两卡局限如实记录**：该卡 `resumed: false`（新建 Codex
会话，非续接上一轮会话）且 `toolCount: 0`（未独立读取工作区、未复跑验证）。

## S7A-4 限定冻结范围与登记规则（2026-10-03）

**顶层状态仍为 `candidate`。** 本 Spec 不改为 `frozen` / `active`：授权来自**范围精确的限定冻结**，
不来自状态词。按 [Stage 7总计划](../stage_plans/active/stage-07/plan.md#11-权威输入和冻结顺序) §1.1 冻结顺序第 2 步的**分批冻结**
口径，第 4 轮（仲裁、资源与事实序）于 2026-10-03 由 Codex 裁定并经 owner 落进本文；该轮是进入 S7A-4 的
**准入门禁**，其语义正文见 §3.9（六步 / 八阶段唯一映射）、§3.10（7A 资源子集与身份规则）、§3.11（早 / 晚
判定与 error 记录）、§3.12（solver / coordinator 边界与 `SolverProfile` 语义）、§3.13（不得消费清单），
拒绝面映射见 §7.2 的第 4 轮段。

**provenance。** 两张决策卡来自同一 Codex 话题 thread `s7a4-arbitration-rulings`（模型 `gpt-6-astra`，
consult）：①主裁定卡 verdict `adopt`，confidence 0.97（本轮六项裁定；"以六步 Tick 为外层、八阶段为第 4 步
内部规范"，7A 仅支持 `free` / `held` / `terminal` 的 `capacity = 1` exclusive 资源与 prepare 后确定性
coordinator，未冻结数值与编码全部排除在 S7A-4 外）；②处置词与计数确认卡 verdict `adopt`，confidence 0.99
（`CM-C05` / `CM-C09` 均 `open` → `accept`；最终 `open` 14 / `accept` 38；§0.1 末列仍保留两条）。两张卡的
文件名时间戳为 2026-10-02T20:0xZ（UTC），本轮统一按**本地日期 2026-10-03** 登记（与第 2、3 轮先例一致）。
带日期的落地证据见
[第 4 轮门禁报告](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-4-gate-rulings.md)。

**共同基线标识**：`RULING_WORKSHEET §7A` 的 `S1-01…S1-05`（[ADR 0044](../adr/0044-gameplay-v2-semantic-kernel.md)
的"接受门禁"第 6 条引用同一编号），与 S7A-1 / S7A-2 / S7A-3 三节共用同一基线，不另立第二套编号。

**范围精确的授权表述。** 在**本节限定的范围内**授权 S7A-4 按第 4 轮裁定实施仲裁、资源与事实序的
**语义边界**（阶段名称 / 顺序 / 映射 / 语义边界与资源子集的身份规则）；范围外（含阶段编号、序列化编码、
预算与窗口数值、`SolverProfile` 的默认列表与 `max*` 数值、唯一性证明的 proof 编码、`terminal` 编码、
资源状态表的逐字段表示、后续 `gap` / handoff / `capacity > 1` 语义，以及第 6、7 轮尚未裁定的语义）
仍不授权（第 5 轮的语义另见 `## S7A-5` / `## S7A-6` 两节，**不在本节授权范围内**）。

| 范围 | 本 Spec 中属本批次的部分 | 本批次之外 |
| --- | --- | --- |
| ① 六步 / 八阶段唯一映射（Q-16 / CM-C05） | §3.9 的映射表与四条规则：六步＝外层、八阶段＝第 4 步内部完整展开；第 3 步＝八阶段第 1 步、第 4 步＝八阶段第 2–7 步、第 5 步＝八阶段第 8 步后按 causal total order 排序、第 6 步 Tick 末提交 Hook / signal；7A 对 `gap` / recovery / handoff 输入稳定拒绝 | **阶段编号、序列化编码、预算与窗口数值**：后续批次；Fact 因果总序的**语义**已由第 5 轮冻结（`S7A5-R01`，见 §3.9 第 3 条与 §3.17），其**字节合同**属 **S7A-6**（`CM-F06` / `Q-12`） |
| ② 归属层级（Q-16） | §3.9 的"归属层级"：八阶段顺序与阶段语义进 `judgement identity` 的 engine 组件，不进 chart/content identity | chart/content identity 的字段归属按 §5.2 的既有表行推进 |
| ③ 7A 资源子集（Q-11 / CM-C09） | §3.10 的状态子集（`free` / `held` / `terminal`）与稳定拒绝清单（`gap` / `handoff_pending` / 非零资源 `declaredGapGrace` / handoff / `capacity > 1` / owner 集合 / 并列 slot） | **`terminal` 编码、运行期资源状态表逐字段表示与 serialization**：后续批次；Capsule 只拥有静态资源声明；后续 `gap` / handoff / `capacity > 1` 语义属 S7B+/S7C 候选 |
| ④ 资源身份规则（Q-11 / CM-C09） | §3.10 的七条身份规则：`resourceId` 命名空间、唯一 slot、slot/lease/最小 contact handle/claim identity 进 canonical graph 与 prepared judgement inputs、引擎按规范阶段分配 lease/contact、`contact end` 不复用旧 handle、`observe` 不占用资源、同 Tick 阶段服从 §3.9 | 各 identity 的**字节表示**与字段布局：与实现批次一起冻结（ABI §未决项 #3） |
| ⑤ 早 / 晚判定与 error（P1-12） | §3.11 的四条：Exact 由窗口半宽定义、窗口外早击不产生 Fact 但产生诊断、Miss / absence 由 deadline 产生 Fact、Hold head/body/tail error 分别记录不合并 | **判定窗口的具体半宽数值**：后续预算 / profile 批次；证明与测量入口按 §8.4 保持未冻结 |
| ⑥ solver / coordinator 与 `SolverProfile`（Q-03 / CM-C06 / CM-C07） | §3.12 的边界与冻结强度表：prepare/compile solver 负责展开 / 验证 / 上界证明 / 唯一性证明并生成 prepared profile；runtime coordinator **不解析/编译 solver；runtime coordinator 只执行已准备的确定性 policy**（第 7 轮 `S7A7-R07` 收官该措辞，2026-10-03）；名称采用 `coordinator.policy.greedy_v1`；`SolverProfile` **字段语义**、**prepare 拒绝语义**与 `objective` / `tieBreak` **有序语义列表**在本批冻结 | **默认列表、具体算法预算、`K` / fuel / `max*` 数值**（S7A-9 / 后续预算批次）、**序列化表示**（首次消费它的后续批次）、**唯一性证明的 proof 编码**（首次消费它的后续批次） |
| ⑦ 拒绝面登记（§7.2） | §7.2 第 4 轮段：五类拒绝**不新增 R 条目**，本清单仍为 19 条 | 新增 R 编号必须另立裁决 |

**S7A-4 不得消费清单（实现须逐条对齐）。** 见 §3.13；该清单与
[Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) 的 `## S7A-4 限定冻结范围与登记规则（2026-10-03）` 一节一致。
逐条判定与证据见 [第 4 轮门禁报告](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-4-gate-rulings.md)。

**后续批次阻塞项（首次消费它们的批次）。** 第 4 轮留下的阻塞项及首次消费批次：

| 阻塞项 | 首次消费批次 |
| --- | --- |
| `SolverProfile` 默认列表与具体算法预算、`K` / fuel / `max*` 数值 | **S7A-9**（与后续预算批次；数值测量后单独接受） |
| 唯一性证明的 proof 算法与证据形式（proof 编码） | 首次消费它的后续批次（S7A-9 / 审计工作项） |
| wire / serialization 表示（阶段编码、资源状态表、identity 字节） | 首次消费它的后续批次（S7A-6 起按 §未决项 #3） |
| `terminal` 编码 | 首次消费它的后续批次 |
| 后续 `gap` / handoff / `capacity > 1` 语义 | **S7B+ / S7C** 候选（不阻塞 S7A-4） |
| 判定窗口半宽的具体数值 | 后续预算 / profile 批次 |

**§1.1 第 2 项标注的运行语义已于第 7 轮补齐。** 本节只冻结仲裁、资源与事实序的语义边界；Fact Ledger 与
Ruleset 事务、Score 与 Snapshot 字段集（第 5 轮，已裁定，见 `## S7A-5` / `## S7A-6` 两节）、表现桥接与
诊断（第 6 轮，已裁定）与收尾澄清与缺陷（第 7 轮，**已裁定** 2026-10-03）均已在各自章节写出；
**第 1–7 轮无待裁定轮次**。**裁定完成不等于实现完成**：本批次仍需实施，预算数值同样不在本批次
（§8.2–§8.5，S7A-9 前保持未冻结）。

## S7A-5 限定冻结范围与登记规则（2026-10-03）

**目的。** 把第 5 轮（Ruleset、事实序、Score 与 Snapshot；进入 S7A-5 / S7A-6 前）裁定的**语义正文**登记为
S7A-5 的**文档准入门禁**，并明确本批次**授权什么、不授权什么**。本节是 [RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)
§9 第 5 轮行在 Spec 侧的对应节，与 `## S7A-4` 同构。

**顶层状态仍为 `candidate`。** 本 Spec 不改为 `frozen` / `active`：授权来自**范围精确的限定冻结**，不来自
状态词。按 [Stage 7总计划](../stage_plans/active/stage-07/plan.md#11-权威输入和冻结顺序) §1.1 冻结顺序第 2 步的**分批冻结**
口径，第 5 轮于 2026-10-03 由 Codex 裁定并经 owner 落进本文；该轮与 `## S7A-6` 共为进入 S7A-5 的
**准入门禁**。语义正文见 §3.14（Ruleset Tick 三阶段与 `faulted` 行为矩阵）、§3.15（`RegisterKind` 三类与
冲突策略）、§3.16（Ruleset 输入边界）、§3.17（Fact 身份、commit 分配与 revision 归属）、§3.20（Outcome /
`FactCategory` / grade / `TimingError` 与统计）、§3.21（Life 关闭）、§3.22（不得消费清单）；事实序的规范
总序见 §3.9 第 3 条；拒绝面映射见 §7.2 的第 5 轮段；运行期失败见 §9.4。

**provenance。** 三张决策卡来自同一 Codex 话题 thread `01a0fe49-cfe5-7933-8d03-3d0a65d34c8b`（consult）：
①主裁定卡 verdict `adopt`，confidence 0.95（`S7A5-R01…R11` 逐条结论；`S7A5-R10` 只用于 `LifeState`，
`P1-14` 另登记为 `S7A5-R11`）；②处置词与计数确认卡 verdict `adopt`，confidence 0.99（`CM-F06`、`CM-S08`、
`CM-S10`、`CM-K07` 四条 `open` → `accept`；最终 `open` 10 / `accept` 42；§0.1 末列仍为 21；`CM-S11`、
`CM-K08` 不改变任何计数）；③ABI 状态格首词与追踪计数追问卡 verdict `adopt`，confidence 0.98（9 行状态格
首词全部保持 `待冻结`；追踪计数 126 = 96 + 30 与 9 域分项不变；**不新增 §未决项 #21、不新增 §类型清单条目**）。
三张卡的文件名时间戳为 2026-10-02T20:2x–20:3xZ（UTC），本轮统一按**本地日期 2026-10-03** 登记（与第 2、3、4
轮先例一致）。带日期的落地证据见
[第 5 轮门禁报告](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-5-6-gate-rulings.md)。

**共同基线标识**：`RULING_WORKSHEET §7A` 的 `S1-01…S1-05`（[ADR 0044](../adr/0044-gameplay-v2-semantic-kernel.md)
的"接受门禁"第 6 条引用同一编号），与 S7A-1 / S7A-2 / S7A-3 / S7A-4 四节共用同一基线，不另立第二套编号。

**范围精确的授权表述。** 在**本节限定的范围内**授权 S7A-5 按第 5 轮裁定实施 Ruleset 折叠与 Score / Combo /
Statistics 的**语义边界**（三阶段事务与 `faulted` 行为、`RegisterKind` 三类与冲突策略、内置 Ruleset 边界、
Fact 身份生成语义、Outcome / category / grade / `TimingError` 语义、统计 reset / seek / replay 规则、Life
关闭）；范围外（含序列化编码、字节布局、全部预算与限额数值、`maxSeekLatency` 数值、package 形状与迁移、
`P1-14` 的 REF0 物理字段，以及第 6、7 轮尚未裁定的语义）仍不授权（S7A-6 的字段集与闭包另见
`## S7A-6`，**不在本节授权范围内**）。

| 范围 | 本 Spec 中属本批次的部分 | 本批次之外 |
| --- | --- | --- |
| ① 三阶段与 `faulted`（Q-08 / `S7A5-R03`） | §3.14 的三阶段表与七条规则、`faulted` 行为矩阵、§9.4 的运行期失败处置、§10 第 6 条 | 序列化编码、诊断码表汇总（第 6 轮）；快照**字节布局**属 S7A-6 |
| ② `RegisterKind` 三类（CM-S02 / `S7A5-R04`） | §3.15 的三类表与五条规则；7A 只接受三类、其他 kind 稳定拒绝 | 字段表示与编码（首次消费它的后续批次）；ABI 域 6 首词保持 `待冻结` |
| ③ 内置 Ruleset 边界（Q-17 / CM-S08 / `S7A5-R05`） | §3.16 的四条规则；`cuexis.ruleset` package 命中 R-09、诊断 `ruleset.package_unsupported`；§7.2 的 R-09 行 | **package 形状 / identity / 完整性 / 迁移**归 S7C-2；**不新增 R 编号** |
| ④ Fact 身份与总序（Q-12 / CM-F06 / `S7A5-R01` + `S7A5-R02`） | §3.9 第 3 条的**规范总序**与 §3.17 的九条规则（`originId` / `commitId` / `factId` / `phasePriority` / timer ordinal / correction 拒绝 / `FactSemanticRevision` 归属） | **物理字节布局、varint / endianness、`FactId` / `CommitId` 编码与 reference-evaluator byte fixture** 属 **S7A-6** |
| ⑤ Outcome / category / grade / error（CM-S10 / P1-07 / `S7A5-R09`） | §3.20 的七条规则：`Outcome` = `{hit, miss}`、Hold 三段为 phase-local、`FactCategory` 与 phase 一一对应、grade 可选且缺失即 absent、`TimingError` 为有符号整数 tick 差、seek / replay 从 Fact Ledger 重建统计、reset 不产生 Fact | 字段表示、溢出 / 饱和表示与编码（首次消费它的后续批次）；数值限额记 S7A-9 |
| ⑥ Life 关闭（CM-S11 / `S7A5-R10`） | §3.21 的四条规则；`capability.disabled` 稳定拒绝、不创建 / 不更新 `LifeState` | `LifeState` 的类型表示与编码（后续批次）；**本轮不新增任何 Life 语义** |
| ⑦ 不得消费清单（`S7A5-R03…R10` 汇总） | §3.22 的 S7A-5 清单（8 项） | 清单内的每一项都**不得**在本批次被消费或绕过（S1-05 口径） |

**S7A-5 不得消费清单（实现须逐条对齐）。** 见 §3.22；该清单与
[Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) 的 `## S7A-5 限定冻结范围与登记规则（2026-10-03）` 一节一致。
逐条判定与证据见 [第 5 轮门禁报告](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-5-6-gate-rulings.md)。

**后续批次阻塞项（首次消费它们的批次）。** 第 5 轮留下的阻塞项及首次消费批次：

| 阻塞项 | 首次消费批次 |
| --- | --- |
| 两套 header 的字节布局与 Event / Fact codec 编码、`FactId` / `CommitId` 物理编码、varint / endianness、reference-evaluator byte fixture | **S7A-6**（首次序列化消费） |
| §3.18 的 typed byte-budget descriptor 的**具体数值** | **S7A-9**（或在首次序列化消费者处；测量后单独接受） |
| 具体 `maxSeekLatency` 数值 | **S7A-9**（测量后单独接受；在此之前禁止进入 Schema / header / 运行时约束 / 对外承诺） |
| 全部数值限额、late-policy 数值与业务量程限额 | 后续预算批次 / **S7A-9**（各自首次消费批次） |
| `cuexis.ruleset` package 形状 / identity / 迁移 | **S7C-2** |
| `P1-14` 的 REF0 物理字段 / 编号 / 编码 | **S7A-3**（首次 Packed 写入）；entry / manifest 集成属 **S7A-7** |
| 后续 `gap` / handoff / `capacity > 1` 语义 | **S7B+ / S7C** 候选（不阻塞 S7A-5） |

**§1.1 第 2 项标注的运行语义已于第 7 轮补齐。** 本节只冻结 Ruleset 事务、事实序、Score 与 Snapshot 的语义边界；
Snapshot / Replay 的字段集与闭包（第 5 轮，见 `## S7A-6`）、表现桥接与诊断（第 6 轮，已裁定）、收尾澄清与
缺陷（第 7 轮，**已裁定** 2026-10-03）均已在各自章节写出；**第 1–7 轮无待裁定轮次**。**裁定完成不等于实现
完成**：本批次仍需实施，预算数值不在本批次（§8.2–§8.5，S7A-9 前保持未冻结）。

## S7A-6 限定冻结范围与登记规则（2026-10-03）

**目的。** 把第 5 轮裁定中属 Snapshot / Replay / Seek 的**语义正文**登记为 S7A-6 的**文档准入门禁**，并
明确本批次**授权什么、不授权什么**。与 `## S7A-5` 共用同一 baseline、同一三张决策卡的 provenance 与同一
provenance 段落（见上一节），本节不重复 provenance 全文。

**顶层状态仍为 `candidate`。** 授权来自**范围精确的限定冻结**，不来自状态词。第 5 轮于 2026-10-03 由 Codex
裁定并经 owner 落进本文。语义正文见 §3.18（Snapshot / Replay header 字段集、`SnapshotPayload` 闭包与不得
保存清单）、§3.19（`SeekLatencyCommitment`）、§3.17（Fact 身份与 `FactSemanticRevision`）、§3.14（`faulted`
与 snapshot 的消歧）、§3.22（不得消费清单）；§10 第 1–6 条为契约边界；拒绝面映射见 §7.2 的第 5 轮段。

**范围精确的授权表述。** 在**本节限定的范围内**授权 S7A-6 按第 5 轮裁定实施 Snapshot / Replay / Seek 的
**字段集与语义**（两套 header 的字段集、`SnapshotPayload` 闭包、不得保存清单、`SeekLatencyCommitment` 的
类型与收紧关系、`faulted` 与 snapshot 的关系）；范围外（含两套 header 的**字节布局**与编码、具体预算数值、
Event / Fact codec 编码、`maxSeekLatency` 数值、连续重采样格点与采样相位，以及第 6、7 轮尚未裁定的
语义）仍不授权。

| 范围 | 本 Spec 中属本批次的部分 | 本批次之外 |
| --- | --- | --- |
| ① Snapshot header 字段集（CM-K07 / `S7A5-R06`） | §3.18 的字段集（八项）：`formatVersion`、四分量 `JudgementIdentity`、`factSemanticRevision`、`stateSchemaRevision`、normalized event count、Fact count、payload byte count、typed byte-budget descriptor | **字节布局、字段顺序、对齐与编码**；**预算数值**记 S7A-9 |
| ② Replay header 字段集（CM-K01 / Q-13 / `S7A5-R06`） | §3.18 的字段集（八项）；**拼写规范：类型 `EventCodecId`、字段 `eventCodecId`** | **codec 编码与字节布局**；`ReplayDecodeBudget` 的**数值**记 S7A-9 |
| ③ `SnapshotPayload` 闭包与不得保存（`S7A5-R07`） | §3.18 第 4、5 条的十四条闭包项与五项不得保存项；FactBinding 与 prepared immutable graph **重新取得、不重复保存** | **逐字段表示**（与实现批次一起冻结）；增量 snapshot / 压缩 ledger / 跨 minor 迁移在无全量 golden 前不进公共合同 |
| ④ revision 归属（`S7A5-R06`） | §3.18 第 2、3 条：`factSemanticRevision` 进 engine identity；`stateSchemaRevision` 不进 judgement identity | 两个 revision 的**取值与编码**（后续批次） |
| ⑤ `SeekLatencyCommitment`（CM-K08 / `S7A5-R08`） | §3.19 的七条规则：类型、承诺语义、会话只能收紧、测量口径入口、数值禁令、无损正确性不变 | **具体 `maxSeekLatency` 数值**记 **S7A-9**；测量表本身在 [BUDGET_AND_EVIDENCE_PLAN.md](../proposals/gameplay-v2-acceptance/BUDGET_AND_EVIDENCE_PLAN.md) §5 |
| ⑥ `faulted` 与 snapshot（`S7A5-R03`） | §3.14 第 4、5、7 条与 §10 第 6 条：**`faulted` 不可新建 snapshot**、`snapshot` 稳定失败；快照闭包仍**包含** fault 诊断 / 状态 | 恢复事务的具体表示与诊断码表（第 6 轮） |
| ⑦ 不得消费清单（`S7A5-R06…R08` 汇总） | §3.22 的 S7A-6 清单（S7A-5 的 8 项 + 另 5 项） | 清单内的每一项都**不得**在本批次被消费、保存或绕过 |

**S7A-6 不得消费清单（实现须逐条对齐）。** 见 §3.22；该清单与
[Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md) 的 `## S7A-6 限定冻结范围与登记规则（2026-10-03）` 一节一致。
逐条判定与证据见 [第 5 轮门禁报告](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-5-6-gate-rulings.md)。

**后续批次阻塞项（首次消费它们的批次）。** 第 5 轮留下的阻塞项及首次消费批次（与 `## S7A-5` 的同一张表
一致，此处只列属 Snapshot / Replay / Seek 的部分）：

| 阻塞项 | 首次消费批次 |
| --- | --- |
| 两套 header 的字节布局与编码、Event / Fact codec 编码、`FactId` / `CommitId` 物理编码、reference-evaluator byte fixture | **S7A-6 自身**（在本批次内冻结，但不得据第 5 轮声称已冻结） |
| 具体预算数值（事件数 / 字节数 / 解码时间）与 `maxSeekLatency` 数值 | **S7A-9**（测量后单独接受） |
| 连续重采样格点 / 采样相位与连续输入能力 | **S7B-1**（不阻塞 S7A-6） |
| diagnostics 码表汇总与恢复事务表示 | **第 6 轮（已裁定，2026-10-03，`S7A6-R06`）**：码表位置固定为 `schemas/cuexis.gameplay-diagnostics.v2.codes.json` 并配套一个 CTest 校验项；**逐条码表内容**仍属工具侧落地与后续批次（见本文 `## S7A-7` 节与本 Spec §9.6） |

**§1.1 第 2 项标注的运行语义已于第 7 轮补齐。** 本节只冻结 Snapshot / Replay / Seek 的字段集与语义边界；
表现桥接与诊断（第 6 轮，**已裁定** 2026-10-03）、收尾澄清与缺陷（第 7 轮，**已裁定** 2026-10-03）均已在各自
章节写出；**第 1–7 轮无待裁定轮次**。**裁定完成不等于实现完成**：本批次仍需实施，两套 header 的字节布局与
codec 编码在本批次内冻结。

## S7A-7 限定冻结范围与登记规则（2026-10-03）

**provenance。** 单张决策卡，Codex 话题 thread `01a0fe61-3476-7882-9674-5f6b05035237`（consult，模型
`gpt-6-astra`）：verdict **`adopt`**，confidence **0.97**；第 6 轮 15 行登记 / 按项计 17 项，编号
`S7A6-R01…R12`。签发的四类动作、provenance、以及"`S7A6-R01…R12` ↔ 15 行映射由本 package **推导**"的
声明见 [RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §6 与 §9 第 6 行。
卡的文件名时间戳为 2026-10-02T20:51:47.167Z（UTC），按**本地日期 2026-10-03** 登记。带日期落地证据见
[第 6 轮门禁报告](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-6-gate-rulings.md)。

**共同基线标识。** 本节与 `## S7A-1` … `## S7A-6` 各节共用 `RULING_WORKSHEET §7A` 的 `S1-01…S1-05`
基线标识（[ADR 0044](../adr/0044-gameplay-v2-semantic-kernel.md) 的"接受门禁"第 6 条引用同一编号），
**不另立第二套编号**。本轮**不新增 R 编号**（§7.2 仍 **19 条**）、**不新增诊断类别**（§9.2 仍**九类**）。

**范围精确的授权表述。** 在**本节限定的范围内**授权后续实现消费第 6 轮冻结的**文档合同与门禁**：
发布入口与 manifest 字段清单（§3.6、§3.23）、判定闭包 / 表现投影粒度（§3.23、§9.7）、
`CapabilityRecord` 七字段与逐字段 identity 归属（§5.6、§6.4）、诊断四层模型与集中码表 + CTest
校验项（§9.1、§9.2、§9.6、§9.8）、`P1-08` 的 ABI 映射（§3.24）与 `P2-04`…`P2-09` 的分类层 / 独立
字段 / 资源三分拆 / Replay 归属（§3.25）。**范围外**（含：具体预算数值与 `maxSeekLatency` 数值、
未冻结的 wire / codec / `FactId` / `CommitId` 字节编码、Ruleset package 形状 / identity / 迁移、
Presentation 缓存与 HostOverride token 的 snapshot 持久化、未提交 delta 与连续采样相位、运行时脚本 /
逐帧回调、Life capability 与任何第二判定路径，以及**第 7 轮已裁定但需在各自批次实施的语义**）仍不授权。

**本轮冻结的六类语义（摘要）。**

| 类别 | 冻结内容 | 首次消费 |
| --- | --- | --- |
| 发布入口（`S7A6-R01`） | `entryKind` 闭集（`packed-chart` / `gameplay-graph`）；`author-source` 非 Playback entry；Ruleset package 非第三种 entry；manifest 七项字段清单 | S7A-7 |
| identity 与闭包（`S7A6-R02`） | `compiledSemanticIdentity` 与完整 capability closure **同时携带且可比较**；`CapabilityRecord` 七字段逐字段 identity 归属（`staticBudget` / `snapshotCost` / `stableRejectCode` **不进** semantic hash / identity） | S7A-7 |
| 表现桥接（`S7A6-R03`） | `GameplayOverride` 命名层；precedence 常量 `Initial < Behavior < Animation < GameplayOverride/FactBinding < StudioPreviewOverride`；`render.visible = false` 为显式值；adapter 只消费已提交 FactBinding；当前有效立即 / 未来 `effectivePresentationTick`；`(factId, targetId)` 幂等；seek / replay / reload 从 Fact Ledger 重建 token；lifetime 到期或 reset / session replacement 终止；表现失败不回写 | S7A-7 |
| 粒度分界（`S7A6-R04` / `S7A6-R05`） | 判定闭包悬空 = prepare 原子失败（`identity_closure_incomplete` / `invalid_relation`）；纯表现 target 缺失 = 空绑定 + 丢弃投影事件 + 稳定 `presentation-target-missing`；`groupCommit` 部分提交不回滚 + 稳定 `partial-group` + 稳定拒绝路径；correction 仅影响未提交窗口 | S7A-7 |
| 诊断（`S7A6-R06`） | 四层模型 + 九类 category + `faulted` 行为；集中码表 `schemas/cuexis.gameplay-diagnostics.v2.codes.json` + 一个 CTest 校验项（code 唯一性、映射完整性、`stableRejectCode` 已登记、输入 / 几何三码不可重复扩展）；未登记码不得进公共 ABI | S7A-7 |
| ABI 映射（`S7A6-R07`…`S7A6-R12`） | `P1-08`：`observationTick` + 原始时间戳、有符号整数 tick 差的 `error`、会话内单调 `eventSequence`、无隐式换算；`P2-04`…`P2-09`：分类层与结果层、三独立字段、`resources` 三分拆、只有 Fact Ledger 进 Replay / Snapshot、`extensions` 仅命名空间化且须证明不影响判定 | S7A-7 |

**S7A-7 不得消费清单（表示与行为，11 项）。** S7A-7 **不得**消费：① 具体预算数值；②
`maxSeekLatency` 具体数值；③ 未冻结的 wire / codec 字节编码；④ 未冻结的 `FactId` / `CommitId`
字节编码；⑤ Ruleset package 形状 / identity / 迁移；⑥ Presentation 缓存或 HostOverride token 的
snapshot 持久化；⑦ 未提交 delta；⑧ 连续采样相位；⑨ 运行时脚本 / 逐帧回调；⑩ Life capability 或
任何第二判定路径；⑪ 把语义字段的"未知"降级为默认值。该清单与第 2 / 4 / 5 轮清单**不冲突**
（不重复、不放松）。

**后续批次阻塞项（本轮登记，不属于 S7A-7 门禁）。**

| 阻塞项 | 首次消费批次 |
| --- | --- |
| 诊断码表的**逐条内容**（除已冻结三码与既有 R-09 映射外）、`severity` 取值域与 `faulted` 逐码判定 | 工具侧随 `schemas/cuexis.gameplay-diagnostics.v2.codes.json` + CTest 校验项落地（并行工具线） |
| manifest / Packed header 的逐字段物理编码与预算数值 | 后续序列化批次 / **S7A-9** |
| `stableRejectCode` 与 `CapabilityRecord` 的 ABI 类型行表示 | 对应 ABI 实现批次（本轮只冻结语义文本） |

**表述纪律。** 本节冻结的是**文档合同与门禁**；**不得**把本轮写成"Judgement 实现完成"或
"Stage 7A 完成"，也**不得**据本节声称任何 hosted / GPU / 真实设备 / 音频证据已通过。

## S7A-7 之后的收尾：第 7 轮裁定与无待裁定轮次（2026-10-03）

**provenance。** 第 7 轮（收尾澄清与缺陷）由 Codex 的主卡 thread
`01a0fe61-b7a3-7853-a380-0793fee14e14`（consult，模型 `gpt-6-astra`，verdict `adopt`、confidence
**0.96**）与同议题小追问的口径确认卡 thread `01a0fe61-3476-7882-9674-5f6b05035237`（verdict `adopt`、
confidence **0.99**）裁定；文件名时间戳为 2026-10-02T20:52:37.188Z 与 2026-10-02T21:08:34.383Z（UTC），
统一按**本地日期 2026-10-03** 登记。逐条登记见
[RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §7、§8 与 §9；带日期证据见
[第 7 轮收尾裁定与缺陷处置记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-7-wrapup-rulings.md)。

**第 7 轮共 16 行 / 18 项（编号 `S7A7-R01…R16`）**，语义正文已写入本文如下位置：

| 编号 | 项 | 本文落点 | 首次消费 |
| --- | --- | --- | --- |
| `S7A7-R01` | `P1-01` 判定域 typed contract | §3.8.7 第一组、§5.2 判定域行、`## S7A-3` 节 ⑧ | S7A-3 |
| `S7A7-R02` | `P1-02` CXT local relation 合并规则 | §3.8.7 第二组、`## S7A-3` 节 ⑧ | S7A-3 |
| `S7A7-R03` | `P1-05` `input.trajectory.v1` | §7.2 既有连续输入拒绝条目（**不新增 R 条目**） | 阻塞 S7B-1 |
| `S7A7-R04` | `P1-06` Ruleset package 边界 | §3.16、§7.2 既有 R-09 映射 | 阻塞 S7C-2 |
| `S7A7-R05` | `P1-10` 预算分层与成本归属 | §8.5 | S7A-9 |
| `S7A7-R06` | `P1-15` 两路 authoring 等价性 | §3.8.7 第三组、`## S7A-3` 节 ⑧ | S7A-3 |
| `S7A7-R07` | `P2-02` 措辞 | §3.12 第 2 条、§3.13 第 1 条 | S7A-4 |
| `S7A7-R08` | `P2-07` 三项命名统一 | §5.1.1、§5.2 `sourceMap` 行、§6.5 | S7A-3 |
| `S7A7-R09` | `D-2` / `D-7` 六动词命名（合并行，2 项） | 文档订正（plan；ADR 0042 与实现不改） | — |
| `S7A7-R10` | `D-5` 编号引用纪律 | 文档订正（工作表与本轮报告） | — |
| `S7A7-R11` | `D-6` `tools/check_stage6_a2.py` 行号 `:259` | 文档订正（引用处注记，不改历史报告正文） | — |
| `S7A7-R12` | `D-9` 版本门禁 shell 守卫 | **owner-only**；候选补丁未提交，须 owner 按 ADR 0042 具名复核后落 `master` | S7A-8 |
| `S7A7-R13` | `D-11` 稳定 C ABI 阶段归属 | **Stage 14 为唯一阶段号**；`AGENTS.md` 的 Stage 12 为孤例、由 owner 择时订正 | — |
| `S7A7-R14` | `D-12` 研究稿指向行 | **待 owner 确认**；本轮只登记、不编辑指向行 | — |
| `S7A7-R15` | `D-13` 格式门禁（含 `AGENTS.md`:110 glob 描述附带子项，2 项） | 已处置：`cuexis_format_check` 156 → 0；`AGENTS.md`:110 描述已订正 | — |
| `S7A7-R16` | `D-14` `CM-X01` 台账冲突 | 已按方案 (a) 绑到第 6 轮；`CONTRACT_MATRIX` 处置词 `accept` | — |

**行 / 项口径（16 行 / 18 项）。** 上表为 **16 行**；按"合并行与附带子项各记 2 项"计为 **18 项**：
`D-2 / D-7`（`S7A7-R09`）是**合并行**（同一行裁定两个编号，1 行 / 2 项），`D-13` 的
**`AGENTS.md`:110 glob 描述附带子项**（`S7A7-R15`）是**独立处置对象**（格式修复与 glob 描述订正各自独立，
计为独立项）。逐轮折算据此自洽：第 1 轮 14 行 / 15 项、第 2 轮 6 行 / 7 项、第 3 轮 6 行 / 7 项、
第 4 轮 6 行 / 6 项、第 5 轮 13 行 / 14 项、第 6 轮 15 行 / 17 项、第 7 轮 16 行 / 18 项 ⇒ 逐轮合计
**76 行 / 84 项**，与 §0.1 的绑定合计逐字一致。**第 6 轮门禁报告中"76 行 / 83 项"是当时按"第 7 轮
16 行 / 17 项"误推所致；该带日期的报告正文不改写**，订正以本节、[ADR 0044](../adr/0044-gameplay-v2-semantic-kernel.md)
的指针说明与 [RULING_WORKSHEET.md](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) §7 为准。

**五条 `CM-V*` 处置词补齐（不占 `S7A7-R01…R16` 编号）。** 五条残余 `open` 合同项（`CM-V06`、`CM-V07`、
`CM-V13`、`CM-I06`、`CM-X06`）按第 1 轮裁定补齐处置词与裁定正文（`open` → `accept`），`open` 集合为空；
最终 **`accept` 52 / `revise` 26 / `supersede` 17 / `retain` 17 / `open` 0 / `reject` 2 = 114**。**计数不变声明**：
§7.2 仍 **19 条**、§9.2 仍**九类**、ABI 仍 **9 域 / 126 = 96 + 30**、§未决项条目数不变；本节与 §3.8.7、
§5.1.1、§8.5 **不新增** R 编号、诊断类别、identity 分量或 ABI 类型行。

**收尾门禁（各批次的实施门禁，非"实现完成"声明）。** S7A-3 的 typed prepare / Chart-v5↔CXT-v2 golden diff /
REF0 首次写入；S7A-4 的 kernel 与 coordinator 边界；S7A-5 / S7A-6 的生命周期、snapshot / replay、codec 与
不得消费清单；S7A-7 在 `CM-P04` / `CM-P06` / `CM-P08` / `CM-D04` / `CM-X01` 闭合后集成；S7A-8 的候选 /
版本门禁、同 SHA 交接与 **owner-only 的 `D-9`**；S7A-9 的本地矩阵（Debug / Release / headless / MinGW）、
hosted 同 SHA（Linux / MSVC / MinGW）、五类预算实测（**阈值另行单独接受**）与交接；以及 docs、format、
version、architecture / allowlist 与 CTest 门禁。

**表述纪律。** 本节**只**把第 7 轮的裁定与计数写进文档合同；**不得**写成"Judgement 实现完成"或
"Stage 7A 完成"——实现批次 S7A-3…S7A-9 仍未完成，`D-9` 仍为 owner-only、`D-12` 仍待 owner 确认。

## 11. 未决项

本 Spec 只登记已接受裁决的字段合同；下列未决点按
[未决语义分轮裁决清单](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md) 的轮次推进。
**第 1–7 轮已全部裁定（无待裁定轮次）；本 Spec 可据此冻结相应批次的文档合同，但实现批次
S7A-3…S7A-9 仍未完成。**（第 1 轮、第 1 轮补充、第 2 轮、第 3 轮、第 4 轮、第 5 轮、第 6 轮与第 7 轮均已
裁定；其语义正文分别见本文 `## S7A-1 限定冻结范围与登记规则（2026-10-02）`、
`## S7A-2 限定冻结范围与登记规则（2026-10-03）`、`## S7A-3 限定冻结范围与登记规则（2026-10-03）`、
`## S7A-4 限定冻结范围与登记规则（2026-10-03）`、`## S7A-5 限定冻结范围与登记规则（2026-10-03）`、
`## S7A-6 限定冻结范围与登记规则（2026-10-03）`、`## S7A-7 限定冻结范围与登记规则（2026-10-03）` 与
`## S7A-7 之后的收尾：第 7 轮裁定与无待裁定轮次（2026-10-03）` 八节，授权范围以各节为准。）

| 轮 | 本 Specification 受影响的部分 | 主要未决编号 |
| --- | --- | --- |
| 2（进入 S7A-2 前）**已裁定（第 2 轮，Codex 2026-10-03；stop 语义与输入规范化半批语义均经 2026-10-03 实现复核修订 / 补充）** | §0.1 时间域术语、§3.7 时间基 / Tick / commit 边界全文（§3.7.2 第 3 条为**精确累计函数**表述、§3.7.3 第 4、5 条为**负 tick 合法与 clock 不可表示复用**、§3.7.4 第 5 条与 §3.7.7 表补**参数量级关系门禁**与**连续 / 不连续表示复用**、§3.7.6 第 5、6 条为 **64 位 canonical 冻结与可表示性先于范围**、§3.7.8 表补**输入半批三条原子失败映射**、§9.3 补**三分法**与输入半批映射）、§5.2 identity 表的 `timebaseRef` 行与 **InputMapping 行（域声明进 session identity）**、§7.2 的六类映射与**输入半批不新增 R 条目**段、§9 诊断 | Q-04、CM-T03、CM-T04、CM-T08 / P1-04、P2-03、CM-T13 已裁定并落进本文；F-04 已闭合于 [ABI 字段级映射表](../api/GAMEPLAY_V2_ABI.md)；修订两张续裁卡见 [第 2 轮实现裁定记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-2-implementation-rulings.md)，输入规范化半批的两张续裁卡（`adopt` 0.97 / 0.99）见 [第 2 轮输入半批实现裁定记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-2-input-implementation-rulings.md) |
| 后续批次（第 2 轮留下的阻塞项） | §3.7.4 的 late-policy 数值、§3.7.4 / §3.7.7 的 late-policy 参数量级关系校验、§3.7.6 / §3.7.7 的业务量程限额、§3.7.2 / §3.7.7 的 Tick → Beat 反查契约、S7C-1 的 `CalibrationProfile` 扩展、连续输入能力（S7B-1）、S7A-9 预算数值 | 具体 late-policy 数值、具体业务量程限额、S7C-1 校准扩展、连续输入能力；**均不阻塞 S7A-2 的语义冻结**；**late-policy 参数量级关系的校验**登记为**阻塞 S7A-4 的 late-window / 判定消费门禁**，**Tick → Beat 反查契约**登记为**首次消费它的后续批次**的阻塞项 |
| 3（进入 S7A-3 前）**已裁定（第 3 轮，Codex 2026-10-03）** | §1.3 W 类缺口的引用边界、§2.5 第 1 条、§3.3 的 Q-05 段、新增 §3.8 prepare / 装配 / entry 边界全文、新增 §5.5 identity 规范字节边界、§7.2 的第 3 轮映射段 | Q-05、Q-07、Q-18 / P2-10、CM-C10 / P1-03 已裁定并落进本文；CM-X05 由 D-3 落地关闭（处置词 `revise`） |
| 后续批次（第 3 轮遗留项） | §3.8.4 的 `boundedRelationInstance` 候选表示与 `stableRejectCode`、§5.5 的 identity 内部规范字节编码实现、[SUPPORT §6.1](../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md) 的 `W-01…W-05` 候选项 | Gameplay Capsule v2 的新 wire 编号 / 布局 / 序列化编码已由 S7A-3 设计裁决闭合；其实现证据与 identity 编码仍属 S7A-3 工作；`boundedRelationInstance` 只阻塞首次拟消费它的后续批次；W 候选项按 SUPPORT §6.1 的门禁归属登记 |
| 4（进入 S7A-4 前）**已裁定（第 4 轮，Codex 2026-10-03）** | 新增 §3.9 六步 / 八阶段唯一映射、§3.10 7A 资源子集与身份规则、§3.11 早 / 晚判定与 error 记录、§3.12 solver / coordinator 边界与 `SolverProfile` 语义、§3.13 不得消费清单；§7.2 的第 4 轮映射段；新增 `## S7A-4 限定冻结范围与登记规则（2026-10-03）` | Q-03、Q-11、Q-16、CM-C05、CM-C09、P1-12 已裁定并落进本文；`CM-C04` / `CM-C06` / `CM-C07` 的处置词不变、只更新未决点文字 |
| 后续批次（第 4 轮留下的阻塞项） | §3.9 的阶段编号 / 编码 / 预算 / 窗口数值与 Fact 因果总序字节合同（第 5 轮 `CM-F06` / `Q-12`）、§3.10 的 `terminal` 编码与资源状态表逐字段表示、§3.11 的判定窗口半宽数值、§3.12 的 `SolverProfile` 默认列表与 `max*` 数值（S7A-9）、proof 编码、wire / serialization，以及后续 `gap` / handoff / `capacity > 1` 语义 | **数值与默认列表只阻塞首次消费它们的批次**（S7A-9 / 后续预算批次）；proof 编码、`terminal` 编码与 wire / serialization 由**首次消费它们的后续批次**阻塞；后续 `gap` / handoff / `capacity > 1` 语义属 **S7B+ / S7C 候选**；**均不阻塞 S7A-4** |
| 5（进入 S7A-5 / S7A-6 前）**已裁定（第 5 轮，Codex 2026-10-03）** | 新增 §3.14 Ruleset Tick 三阶段与 `faulted` 行为矩阵、§3.15 `RegisterKind` 三类语义与冲突策略、§3.16 Ruleset 输入边界、§3.17 Fact 身份 / commit 分配 / revision 归属、§3.18 Snapshot / Replay header 字段集与 `SnapshotPayload` 闭包、§3.19 `SeekLatencyCommitment`、§3.20 Outcome / `FactCategory` / grade / `TimingError`、§3.21 Life 关闭、§3.22 不得消费清单；新增 §3.8.6 REF0 / manifest / 诊断 source map 归属表（`P1-14`）；修订 §3.9 第 3 条的规范总序、§9.4 运行期失败、§10 第 1–6 条；§7.2 的第 5 轮映射段；新增 `## S7A-5` 与 `## S7A-6` 两节 | Q-08、Q-17、CM-S08、Q-12、CM-F06、Q-13、CM-K01、CM-K07、CM-S10 / P1-07、CM-S11、CM-K08、CM-S02、P1-14 已裁定并落进本文（`S7A5-R01…R11`）；`CM-S02` / `CM-K01` 处置词不变（`revise`）、只更新未决点文字；`CM-S11` / `CM-K08` 不进入 CONTRACT_MATRIX |
| 后续批次（第 5 轮留下的阻塞项） | Fact 因果总序与 Fact 身份的**物理字节布局**（`FactId` / `CommitId` 编码、varint / endianness、reference-evaluator byte fixture）、§3.18 两套 header 的**字节布局**与 Event / Fact codec 编码、§3.19 的**具体 `maxSeekLatency` 数值**与全部预算数值、§3.16 的 `cuexis.ruleset` package 形状 / identity / 迁移 | **字节布局与 codec 由首次序列化消费它们的 S7A-6 冻结**；**数值与预算记 S7A-9**（测量后单独接受）；**package 形状归 S7C-2**；`P1-14` 的 REF0 首次写入属 **S7A-3**、entry / manifest 集成属 **S7A-7**；**均不阻塞 S7A-5 / S7A-6** |
| 6（进入 S7A-7 前）**已裁定（第 6 轮，Codex 2026-10-03，thread `01a0fe61-3476-7882-9674-5f6b05035237`，单卡 `adopt` 0.97，`S7A6-R01…R12`）** | §3.6 entry kind 与 manifest closure 七项、新增 §3.23 发布粒度 / 表现桥接 / 聚合、新增 §3.24 `P1-08` 映射、新增 §3.25 `P2-04`…`P2-09`、新增 §5.6 `CapabilityRecord` 逐字段 identity 归属、§6.4 registry 字段冻结、§9.1 / §9.2 四层与九类冻结、新增 §9.6 码表登记约定、新增 §9.7 判定闭包 / 表现失败分界、新增 §9.8 表现失败与 `faulted` 区分、§10 第 7 行、新增 `## S7A-7 限定冻结范围与登记规则（2026-10-03）` | Q-06、Q-09、Q-15、Q-19、CM-P04、CM-P06 / P1-13、CM-P08 / P1-11、CM-D04 / P1-09、P1-08、P2-04、P2-05、P2-06、P2-08、P2-09 已裁定并落进本文；`CM-P04` / `CM-P06` / `CM-P08` / `CM-D04` / `CM-X01` 处置词 `open` → `accept`；§7.2 仍 **19 条**、§9.2 仍**九类**、ABI 仍 **126 = 96 + 30** |
| 7（收尾澄清与缺陷）**已裁定（第 7 轮，Codex 2026-10-03，thread `01a0fe61-b7a3-7853-a380-0793fee14e14`，主卡 `adopt` 0.96 + 口径确认卡 `adopt` 0.99，`S7A7-R01…R16`，16 行 / 18 项）** | 新增 §3.8.7 S7A-3 准入门禁（判定域 typed contract、CXT local relation 合并规则、Chart v5 inline 与 CXT v2 emission 等价性）、新增 §5.1.1 三项命名统一、修订 §3.12 / §3.13 的 `P2-02` 措辞、修订 §5.2 `sourceMap` 行、新增 §8.5 预算分层与成本归属、新增 `## S7A-7 之后的收尾：第 7 轮裁定与无待裁定轮次（2026-10-03）` | P1-01、P1-02、P1-05、P1-06、P1-10、P1-15、P2-02、P2-07、D-2 / D-7、D-5、D-6、D-9、D-11、D-12、D-13、D-14 已裁定并落进本文；`CM-V06` / `CM-V07` / `CM-V13` / `CM-I06` / `CM-X06` 处置词 `open` → `accept`，**`open` 集合为空**（`accept` 52 / 114）；§7.2 仍 **19 条**、§9.2 仍**九类**、ABI 仍 **126 = 96 + 30**；`D-9` owner-only、`D-12` 待 owner 确认 |
| 后续批次（第 7 轮之后） | **无待裁定轮次**：五条 `CM-V*` 已按第 1 轮裁定补齐；第 7 轮的实现消费点分别落在 S7A-3（`P1-01` / `P1-02` / `P1-15` / `P2-07`）、S7A-4（`P2-02`）、S7A-8（`D-9`）与 S7A-9（`P1-10`） | `P1-05` 阻塞 **S7B-1**、`P1-06` 阻塞 **S7C-2**；`D-9` 须 owner 按 ADR 0042 具名复核后落 `master`；`D-12` 的 8 处指向行须先取得 owner 对历史稿修改的确认 |

### 11.1 本 Spec 明确留空的裁定

1. §2.4 的**互换性矩阵细则**：本 Spec 只登记该矩阵必须覆盖工具链与载体差异，矩阵条目待裁定。
2. §6.6 的**声明多于派生 closure** 的处置：第 1 轮只裁定"声明少于派生即稳定失败"，反向情形
   未被单独裁定。
3. §10 的 Replay / Snapshot / Schema 与 diagnostics 码表：工件尚不存在，**Replay 与 Snapshot header 的
   字段集已由第 5 轮冻结**（`S7A5-R06` / `S7A5-R07`，见 §3.18），其**字节布局**属 S7A-6；diagnostics
   码表的**文件位置与校验门禁**已由**第 6 轮**冻结（`S7A6-R06`：`schemas/cuexis.gameplay-diagnostics.v2.codes.json`
   + 一个 CTest 校验项，见 §9.1 / §9.6），**逐条码表内容**随各轮裁定增补并仍属后续批次；**码表文件与校验项
   本身已落地**（`schemas/cuexis.gameplay-diagnostics.v2.codes.json` + `cuexis_gameplay_diagnostics_codes`，
   见 §9.1 / §9.5）。
4. §3.7.7 表列的**具体 late-policy 数值、具体业务量程限额、S7C-1 扩展字段与连续输入能力**：第 2 轮
   只冻结类型、边界与越界处置，这些数值与能力**留作后续批次阻塞项**，不阻塞 S7A-2 的语义冻结。
   同表在 2026-10-03 实现复核中新增两行：**late-policy 参数量级关系的校验**登记为**阻塞 S7A-4 的
   late-window / 判定消费门禁**（最终数值与限额仍记 S7A-9）；**Tick → Beat 反查契约**登记为**首次消费
   它的后续批次**的阻塞项（`S7A-3` / `S7A-4` / `S7A-5` 均不消费它）。
5. §3.8.2 的**新 wire 表示**（编号 / 布局 / 序列化编码）、§3.8.4 的 `boundedRelationInstance` 候选表示与
   具体 `stableRejectCode`、以及 §5.5 的**内部规范字节编码实现**：第 3 轮只冻结行为、语义等价、排序与
   身份域边界；wire 表示与候选表示**只阻塞首次拟消费它的后续批次**，而 **identity 的编码实现须在
   S7A-3 消费前闭合**。具体 W 缺口在首次消费前由消费批次补全字段并按 `S1-03` 标注裁决编号与首次消费批次
   （[SUPPORT §6.1](../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md)）。
6. §3.9 的**阶段编号 / 序列化编码 / 预算 / 窗口数值**与 Fact 因果总序的**字节合同**（总序与身份生成的
   **语义**已由第 5 轮冻结，`S7A5-R01` / `S7A5-R02`，见 §3.9 第 3 条与 §3.17；**字节合同**属首次序列化
   消费它的 **S7A-6**）、§3.10 的 **`terminal` 编码与资源状态表逐字段表示**、§3.11 的**判定窗口半宽数值**、
   §3.12 的 **`SolverProfile` 默认列表与 `K` / fuel / `max*` 数值（S7A-9）以及唯一性证明的 proof 编码**、
   wire / serialization 表示，以及后续 **`gap` / handoff / `capacity > 1` 语义**：第 4 轮只冻结阶段
   名称 / 顺序 / 映射 / 语义边界、7A 资源子集与身份规则、早 / 晚判定边界、solver 边界与 `SolverProfile`
   字段语义；上列各项**只阻塞首次消费它们的后续批次**（数值与默认列表记 **S7A-9**；proof 编码、`terminal`
   编码与 wire / serialization 由首次消费它们的批次阻塞；后续 `gap` / handoff / `capacity > 1` 属
   **S7B+ / S7C 候选**），**均不阻塞 S7A-4**。
7. §3.18 的**两套 header 字节布局**与 Event / Fact codec 编码、§3.17 的 **`FactId` / `CommitId` 物理字节
   布局与 varint / endianness、reference-evaluator byte fixture**、§3.19 的**具体 `maxSeekLatency` 数值**、
   §3.16 的 **`cuexis.ruleset` package 形状 / identity / 迁移**：第 5 轮只冻结**字段集、语义、归属与拒绝
   路径**；字节布局与 codec **由首次序列化消费它们的 S7A-6 阻塞**，数值与预算记 **S7A-9**（或在首次
   序列化消费者处），package 形状归 **S7C-2**；`P1-14` 的 REF0 首次写入属 **S7A-3**、entry / manifest
   集成属 **S7A-7**。上列各项**均不阻塞 S7A-5 / S7A-6**。
8. §3.7.6 的 **per-domain 版本字段**：**7A 不引入**该字段，本节因此为该表示**留空**（不冻结任何
   per-domain 版本字段；会话级版本由 `profileVersion` 承载，域声明集合与完整 `AmountSpec` 字段已进
   session identity，§5.2 行）；同样**不保留**任何独立于既有 tick 域表示失败的 "calibrated clock
   不可表示" 拒绝令牌（§3.7.3 第 5 条）。**输入规范化半批的两张续裁卡**
   （同一 thread `s7a1-freeze-bindings`，verdict 均为 `adopt`，confidence **0.97** / **0.99**，按本地日期
   2026-10-03 登记）已裁定 `AmountSpec` 的 64 位 canonical 宽度、可表示性先于范围、三条原子失败映射与
   域声明进 identity；**不新增 R 条目、不新增 ABI 码、不新增第十类**，本 Spec 的计数不变（§7.2 仍 19 条、
   §9.2 仍九类）。带日期证据见
   [第 2 轮输入半批实现裁定记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-2-input-implementation-rulings.md)。

## 12. 相关索引

- [ADR 0044：Gameplay V2 语义内核](../adr/0044-gameplay-v2-semantic-kernel.md)：决策、备选方案、威胁模型与接受门禁
- [Gameplay V2 ABI](../api/GAMEPLAY_V2_ABI.md)：typed 内部 / preview C++ 边界
- [Stage 7A分册](../stage_plans/active/stage-07/plan-a.md)：批次与首用门禁；[总计划](../stage_plans/active/stage-07/plan.md)：依赖与冻结顺序；[Stage 7B+分册](../stage_plans/active/stage-07/plan-b.md)：W类缺口定义
- [Gameplay V2 acceptance package](../proposals/gameplay-v2-acceptance/README.md)：准入包与接受清单
- [S7A-0.2 合同逐项台账](../proposals/gameplay-v2-acceptance/CONTRACT_MATRIX.md)：114 项合同项与 `open` 清单
- [支持 / 拒绝矩阵](../proposals/gameplay-v2-acceptance/SUPPORT_AND_REJECTION_MATRIX.md)：支持集合、19 条拒绝、capability registry 草案
- [格式、入口与 identity 矩阵](../proposals/gameplay-v2-acceptance/FORMAT_ENTRY_AND_IDENTITY.md)：版本层级、8 类载体、四类身份
- [预算与证据计划](../proposals/gameplay-v2-acceptance/BUDGET_AND_EVIDENCE_PLAN.md)：五类 profile 与测量计划
- [未决语义分轮裁决清单](../proposals/gameplay-v2-acceptance/RULING_WORKSHEET.md)：第 1-7 轮裁定登记（**第 1 至第 7 轮均已裁定，无待裁定轮次**；`open` 集合为空）
- [第 6 轮诊断分类学后续补齐记录](../stage_reports/stages/stage-07/decisions/2026-10-03-s7a-diagnostics-taxonomy-rulings.md)：§9.9 的判据、公共性逐条清单与引用口径的带日期证据
- [Packed 候选物理合同](PACKED_CHART_FORMAT.md)：§3.3 唯一已冻结预算表
- [CXC 格式合同](CXC_FORMAT.md)：ZIP32 Stored 载体与 manifest
- [CXT v2 格式合同](CXT_V2_FORMAT.md)：作者层模板与有限展开
- [CXC Chart Entry Extension v1](CHART_ENTRY_V1_FORMAT.md)：Stage 6 candidate entry 边界
- [音乐游戏玩法抽象模型](../architecture/GAMEPLAY_ABSTRACTION_MODEL.md)：共同语义基础
- [Gameplay Judgement Spec](GAMEPLAY_JUDGEMENT_SPEC.md)：Gameplay I 历史候选基线（本 Spec 是其字段与运行语义的替代者；标注动作晚于三份 V2 文档冻结）

## S7A-5/6 首用补充：J0 消费合同（2026-10-06）

本节明确覆盖旧“待冻结”在本轮消费的表示；不接受业务默认数值或生产预算。
组合B、R1/G2/N1/T2/S2/W2/E2/K2/B2保持，U01–U13采用重审推荐。

### J0-5：有限 compiled registry 与事务

首版 Interface `cuexis.ruleset.finite` revision `1`、compiled build
`cuexis.finite-fold.1` 绑定实际编译实现，不接受 caller 自称另一 build。
有序 manifest 必须为 score/1、combo/1、statistics/1，可追加 hook/1；每项 build 同上。
本首版有效Loadout的支持字段为显式owning loadoutId，归session投影。
ScoreConfiguration/ScoreRule/HookDeclaration是本finite Ruleset的显式有效配置，归ruleset投影；
grade→score不能混入chart，Loadout值不能混入ruleset。
programPolicy 为 locked/extendable/open；outcomeScope declared/extended，extended 仅 open。
本版均只执行已声明 hit/miss；未知枚举拒绝。package/Life/step route 保持 capability_disabled。
legacy t4-k4.v1 无 Ruleset 时 Fold absent，不能显示零分成功；有 Ruleset 时使用
`gameplay.fold.finite.v1` 首用语义，其实际配置进入 ruleset/session identity。

| module | readSet / 时点 | writeSet / reducer / trigger |
| --- | --- | --- |
| score | sealed phase-local Fact、旧 committed Fold bonus | i64 score；exclusive 单 owner，每工作 Tick 一次最终写；Fact/due |
| combo | sealed phase-local Fact | u64 combo/maxCombo；exclusive；逐Fact保存峰值；Fact |
| statistics | sealed phase-local Fact | u64 hits/misses；exclusive；ledgerDerivedCount 从成功 Fold 前缀重建；Fact |
| hook | sealed phase-local Fact、声明常量 | u64 monoidValue，max或bit-or，单位元0、全u64闭合；规范贡献去重后合成；仅非空 phase Fact |

Score 初值/min/max、checked或clamp、initialCombo、逐phase/outcome/optional-grade映射均必须显式提供。
逐规范Fact检查：MAX,+1,-1在+1即checked失败；clamp先比较数学方向，禁止溢出后clamp。
没有对应ScoreRule拒绝prepare；无grade匹配absent分支，不回退其它grade。

MeasureComponent 的 optional `gradeTable` 由 chart 拥有：有序闭区间(minimum,maximum,grade)，
engine 算法 signed-interval/1。无表所有profile合法，opaque tokens仍不是表。
有表需端点有序/无交叠/无空洞，覆盖该phase实际可达 signed error，grade引用已声明token；
允许多个区间引用同一grade。seal前只在真实error存在时求值一次；timer Miss等无error保持absent；真实body break Miss有error时按body表求grade。

首版实际支持值型 `fold.bonus.u64`，consumer=Fold，读旧 committed view，值为u64，
贡献键(moduleId,FactId,target,localOrdinal=0)，贡献者仅hook，按max/bit-or合成，
同target每Tick只发布一个最终值，t+1安装到Fold候选。due零Fact仍消费，无Fact禁止再发future。
Kernel/shared/step route尚无已闭合实际consumer，必须显式拒绝，禁止用visibility marker假装支持。
未来增加真实kernel route时消费随seal；Fold-only消费仅随阶段2提交，失败旧Fold逐成员不变。
producer不可变记录与consumer cursor独立；持久bonus不等于pending队列。
仅实际输出时checked(t+1)；无输出INT64_MAX不检查t+1。

新schema成员 reset/初值：Fold由显式配置初始化，counts/cursors/monoid/bonus为代数单位元0，
workTick absent、produced空；所有状态session owner thread，query只读owning。
Kernel watermark、kernelWorkTick、Fold workTick、sealedFactCursor、Fold factCursor、成功horizon、
failedTick/requestedHorizon互相独立；空推进不伪造workTick。
阶段2失败稳定 `ruleset.transaction_failed` / invalid_relation / faulted=true；
旧Fold保留且已seal Fact留存，无fault Fact/新effect。

最小人工预期：无表hit grade absent；[-2,-1]和[0,2]可同grade；显式缺口拒绝；
checked MAX,+1拒绝；clamp MAX,+1,-1得到MAX-1；combo hit/hit/miss峰值2、末值0；
Tick5输出bonus3在Tick6才安装，Tick6无Fact消费且不产生Tick7记录。

## S7A-6 首用补充：J0-6 全量恢复与 W2（2026-10-06）

U06–U12以重审为准。RecoveryInputs owning持有实际SessionConfiguration、有效PreparedRuleset和
PreparedGameplay；重算四分量identity后取依赖，不能仅拿hash。独立restore不绑定历史。

| owning族 | 完整成员 / 初值与读写 |
| --- | --- |
| Kernel | KernelProjection全部phases/resources/contacts/ownership/observers/receipts/Facts；matcher states/epsilonAttempted、coverageHistory、activeRequirements、timerCursor、nextCommitId/exhausted；由prepare初始化，工作Tick候选随seal发布 |
| Ingress | accepted canonical key+sequence、自持有tokens、lastObservedTick、nextId/exhausted、admitted pending含dispatch/forwarding/admission frontier/horizon；初始空/absent/0/false，accepted batch原子提交 |
| Fold | optional完整FoldProjection，包括Score/Combo/maxCombo/hits/misses/factCursor/workTick/produced/consumer cursor/bonus/monoid/ledgerDerived；阶段2原子提交；无Ruleset为absent |

KernelProjection的runScope/session线程/指针不进入wire、identity或状态比较。fault包含code/category/severity/
faulted和完整query状态；失败Evaluation不包含可恢复Snapshot。合法faulted DTO仅在显式new recovery保持faulted，
faulted receiver禁止snapshot/Seek/就地restore。Snapshot stateSchemaRevision=1不进identity，
FactSemantic标识属于engine。identity来源是实际prepared graph、mapping/timebase/late/config与实际compiled registry。

canonical submit共用prepareIngressBatch的序列/碰撞/时间/ID检查，amount直接按canonical范围检查，
不再除scale；sourceClass必须等于mapping。新Fold profile的live/canonical/Writer/Reader共同UTF8准入；
不normalization合法token、不更改legacy准入。raw校准/量化拒绝不作canonical Replay证据。
Replay record为accepted完整batch+复算admission或advance(H)+稳定终态+完整owning query anchor。
record容量、outcome anchor在mutation前预留；发布/fault记录路径无编码buffer分配。

Seek exact H按F(H)执行，不强制finalize H内输入；cut按journal位置，不按observationTick过滤。
precut已接受future/sequence/lastObservedTick全部保留。partial control物化advance(H)，不与原大H故障比较；
complete control必须逐字段对照全部结果。成功same-H control也保存；空submit无fork。
候选健康才替换healthy receiver；原archive继续owning有效。首次accepted live或持久advance/fault发布
惰性分支；仅预检失败不fork。checkpoint必须绑定archive内容、completeRecords/partialHorizon与H，
首次从起点复算完整Snapshot后才进入缓存；失败从起点fallback。索引同时满足cut和horizon，
独立restore无archive不能隐式Seek；外部archive须显式传入并校验。

### W2 有序字节合同

LE、无padding、不用native memcpy。u8 tag/bool/enum，u32 format/schema版本，u64 count/length/ID，
i64 Tick/Score/error使用二补码。bool和optional presence只接受0/1，variant按C++声明顺序0起，
enum按ABI声明顺序0起且Reader检查闭合集。UTF8 string=u64字节长+原始字节，无终止NUL；
vector=u64个数+record顺序；optional=u8 presence+值；variant=u8 tag+record。
所有length/count在转换和乘加前检查，element数量与剩余实际byte bounds交叉校验。

Envelope：magic 8字节、formatVersion u32=1、kind u8(Replay=1/Snapshot=2)、payloadLength u64、
payloadDigest u64（FNV-1a64，offset14695981039346656037，prime1099511628211）；随后payload。
payload count包括section framing，envelope总字节另为29+payloadLength。未知version/tag/截断优先于
摘要，摘要优先于identity/引用/全状态，摘要不证明语义或历史。

| ordered section | framing和逐record成员（顺序即wire） |
| --- | --- |
| 1 header | sectionId u8=1 + length u64；四分量actual canonical identity bytes、FactSemantic string、stateSchema u32=1、eventCount u64、FactCount u64；规范化/late元数据由actual session identity及重新取得的配置逐字段验证；eventCodecId=canonical.discrete.le.v1 |
| 2 Replay records | id=2+length；vector records：batch vector(key,sequence)、admission vector、horizon optional、result optional完整KernelProjection |
| 2 Snapshot state | id=2+length；KernelProjection、matchers vector(states u64 vector,epsilon bool)、coverageHistory vector、activeRequirements u64 vector、timerCursor u64、nextCommitId u64、exhausted bool、IngressSnapshot(accepted vector,lastObservedTick optional,nextId u64,exhausted bool)、pending vector |

key=observationTick i64/domain/source/channel string/action u8/amount optional i64；admission=
key/dispatch i64/forwarded bool/frontier optional i64/horizon optional i64；CanonicalInput=key/sequence u64。
RequirementIdentity依既有六元组顺序；Origin按Observation/Timer/Coordination声明tag与typed成员顺序；
Candidate/Contact/Slot/Lease/Ownership/Fact/Phase/Resource/ObservationRecord/InputReceipt均按
kernel_types.hpp具名成员声明顺序（不含runScope）；KernelProjection按该头声明顺序，identity在header
校验后重新绑定且不重复写；fold按ruleset.hpp成员声明顺序。fault编码稳定code/message/context
key-value vector/cause optional；禁止未提交delta。附加未知section与尾随bytes拒绝。

解码预算状态pending与testOnly accepted fixtures分开；仅显式testOnly descriptor可声明数值，
生产传入数值稳定拒绝；无已接受阈值时按实际输入字节有限界及受检count运行，不声称生产容量通过。
人工bytes、重算摘要伪状态、版本/截断/UTF8/count、完整结果偏差与历史错绑是J6/J7证据，
本合同不是实现完成或容量接受。


### J0-6 接受面补齐

faultStage为u8 kernel/fold/control，随稳定诊断进入完整结果及wire，不进identity；healthy必须absent。
coverageHistory必须逐项等于成功head Fact派生的contact/lease/acquiredTick历史；不能清空后恢复。
Ingress accepted保留全部历史，初始ID=0且连续分配，因此nextId严格等于accepted数量、
可表示的载体exhausted=false；任意seed跳号或自称exhausted不属于生产Snapshot接受面。
full cut的最后完整advance必须为目标H，partial checkpoint不得替代cut中的控制记录。
checkpoint优化不得改变无checkpoint时的接受/拒绝与结果。
Reader完成结构/摘要/identity校验后，accepted batch通过同一canonical入口及admission复算；
header eventCount必须等于全部accepted batch总数。完整终态比较由ReplayEvaluation执行，
带注入故障的源结果仍需同注入环境，不将结构合法等同执行等价。


StatisticsCount的规范键为(phase,outcome,optional grade)，u64 count，覆盖全部显式ScoreRule键；
prepare初值全部0，按键排序；不存在的映射仍prepare拒绝，不动态补业务默认值。
FoldProjection另有u64 strayCount/consumeEmptyCount初值0，从真实receipt Fact计数。
各计数与score/combo统一阶段2提交、受检+1；全字段进入query/Snapshot/W2/full Replay比较。
wire在FoldProjection既有ledgerDerivedCount之后追加counts vector、strayCount、consumeEmptyCount。
Snapshot kernel闭包从prepare与真实receipt dispatch重执行至同watermark，验证所有kernel投影和matcher，
H由watermark+windowCloseThreshold计算；finalizationWatermark是输入final界，不可替代该close间隔。
ScoreRule是唯一映射集合，prepare按(phase,outcome,grade)规范排序，数组置换不改变身份/初始统计；
模块manifest仍是有序声明，重排不被registry默许。


### 首用诊断登记

本轮codec码均category=invalid_relation、severity=error、faulted=false；完整原因随Core Error owning保存。
`codec.magic_invalid/version_unsupported/section_invalid/tag_invalid/truncated/length_invalid/trailing_bytes/utf8_invalid`
登记载体结构/版本/tag/bounds/编码拒绝；`codec.digest_mismatch`登记摘要；
`codec.identity_mismatch/count_mismatch/record_invalid`登记依赖/count/canonical admission；
`codec.budget_unaccepted`登记生产数值预算未接受，`codec.budget_exceeded`仅testOnly已接受fixture超限；
`codec.allocation_failed`登记预发布存储失败。
Ruleset的`registry_invalid/configuration_invalid/hook_invalid`为prepare拒绝，
`ruleset.package_unsupported`与`capability.disabled`保留明确拒绝面；运行Fold
`ruleset.transaction_failed`保存faulted=true及faultStage=fold，与seal前kernel故障分开。
Replay/Seek/Snapshot闭包仍复用`judgement.s7a4.execution.relation_invalid`，
依赖缺失用`judgement.s7a4.execution.profile_incomplete`（identity_closure_incomplete）；
不新增capability，不把普通结构拒绝冒充业务预算。


Fold bonus consumer把u64 bonus作为数学非负增量加到i64 score；不先窄化成i64。
真实反例：bonus=UINT64_MAX且score=4，clamp应取显式maximum，不能按“窄化失败”Fault；
score=INT64_MIN加UINT64_MAX则数学结果INT64_MAX，checked必须允许。
先比较u64增量与数学可用空间(MAX-score)，随后以受检表示取得结果，再应用声明范围/策略。
无效min>max或未知算术策略先拒绝，任何clamp调用均满足范围前提。


U05真实极值出口：显式合法close=0使F(MAX)=MAX，观测选MAX−12以满足完整cell，
已Hit的deadline MAX无Fact不产生Hook而成功；无输入的deadline MAX产生真实Miss，
Hook的MAX+1检查失败，保留seal后的Fact与旧Fold。默认close=3的MAX horizon仍保留MAX deadline pending。

### C78-05 / R78-08 JSON 分配边界（2026-10-07 revision 1）

compiled Graph 沿已选 R78-08 A：json_support 内专用 SAX Reader 直接构造受检 typed Graph DTO，
在 owning Graph 容器扩张前检查字段/tag/count/bounds，key 回调即时拒绝重复与未知必需字段。
输入限额由所属格式显式传入并逐项证明维度可比；零限额配置非法，不接受生产默认数值。
整数指定 i64/u64 分类和范围，不经 f64。Graph 物理字段和 grammar 上界仍须补齐后首用。

通用 json::parseBounded 仅作为 JSON 边界实现与测试材料，当前显式参数为 raw bytes、
container depth、decoded string bytes、总 Value 数（含根和容器）、单容器成员数。
它在每个值进入 owning Value 树前检查总数与父容器容量，在容器入栈前检查深度，
key 回调即拒绝重复；分类保留 i64/u64/f64。这个通用 Value 树不能当作已选方案的 Graph DTO，
不得将“先通用 DOM 再 typed lowering”暗记为 R78-08 A 已实现或 Graph 入口已退出。
JSON lexer 的 token 临时存储仍受 raw bytes 上界；不声称 decoded string 检查发生在 lexer 分配前。
失败返回 json.parse.* 稳定内部码；公共 Graph 首用投影仍须写入对应码表。


## 2026-10-08 FactBinding 聚合首次消费表示

纯表现 binding 新增闭集 aggregation=`any`/`all`/`groupCommit` 与 owning groupMembers：
UTF-8 bindingId 集合，规范化为字节序、无重复。空集合只允许 any 独立绑定；非空必须包含自身，
每个成员都存在且声明相同 aggregation/完整成员集合，成员来源仍各自为已提交 Fact/phase/outcome。
这些字段只进 presentation closure，不进 judgement 四分量。any 对已有来源幂等投影；all 与
 groupCommit 等所有成员匹配的已封存且 Fold 发布范围内 Fact 后才允许投影，各成员 start/end
仍独立按 typed T 判断。已见成员在 scope 内记忆，到期只移除 token，不抹去聚合完成记录。

 groupCommit 尝试时缺纯表现目标：有效成员仍可提交，缺失成员发 presentation-target-missing；
若同次存在有效和缺失成员，发 partial-group 并把该成员集合置为 scope 内稳定拒绝。已提交
成员保持其 lifetime；之后不提交该组新成员。Runtime 整体事务失败则本次有效成员也不标记
已发布。Seek/restore/session replacement 重建整个 scope、已见成员、拒绝与已发布记录，重新
获取当前有效 target/resource。pure missing/partial 不 fault Gameplay，不改 Fact/Fold/Replay。

同优先级相反值若 lifetime 重叠，除能证明同一来源同一 phase 的 outcome/timing 互斥外，
在 prepare 拒绝；跨 Requirement 不能用遍历次序选择值。投影去重仍为 (FactId,targetId)，
 groupMembers/bindingId 不成为新判定身份或 token 去重键。


组成员 bindingId 标识一个来源谓词，而非任意声明行。相同 bindingId 的所有行必须有相同
aggregation 与规范化 groupMembers；非空组还须具有相同 source/phase/outcome/timing。
允许同 ID 的不同 target 或不相交 lifetime，仍以 (FactId,targetId) 去重。这样既保留原合法重复
和分段 lifetime，也不允许同 ID 的组外 Hit 行使组内 Miss 成员提前完成。mixed aliases 不受
遍历顺序影响，在 prepare 稳定拒绝；这是首用表示缺口的合同修订，不改变既有 kernel/Fold。
