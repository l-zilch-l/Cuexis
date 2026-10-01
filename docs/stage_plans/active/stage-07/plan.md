# Stage 7 Implementation Plan: Gameplay Foundation and Judgement Evolution

状态：active；Stage 7A 处于实施准备，Stage 7B+ 为可在 Stage 8 前后持续交付的能力线；产品实现未开始

更新日期：2026-10-02

归档来源：[旧版 PROJECT_GUIDE](../../../archive/PROJECT_GUIDE_LEGACY_2026-08-10.md)、
[SDK transition plan 快照](../../../archive/CUEXIS_SDK_TRANSITION_PLAN_2026-08-10.md) 和
原 Stage 11 Judgement 计划；当前字段与语义输入见本文列出的 ADR、Spec 和 ABI。

本计划只定义阶段目标、批次、依赖、交付物和验收门禁。字段与运行语义由 [Gameplay Judgement
Spec](../../../formats/GAMEPLAY_JUDGEMENT_SPEC.md) 负责，候选 C++ typed preview 边界由
[Gameplay Judgement ABI](../../../api/GAMEPLAY_JUDGEMENT_ABI.md) 负责，设计取舍由
[ADR 0043](../../../adr/0043-gameplay-judgement-ruleset-convergence.md) 负责，当前状态由
[CURRENT_STATUS](../../../CURRENT_STATUS.md) 负责。本文不把研究 spike、候选 ABI 或本计划本身
解释为已经实施的产品能力。

前置是 [Stage 6](../../completed/stage-06/plan.md) 的交接边界和已接受的剩余事项；主要内容
输入为 Chart v5 Core/Packed candidate，Chart v4 / CXT v1 / CXC v1 作为兼容与回退输入。共同
模型见 [音乐游戏玩法抽象模型](../../../architecture/GAMEPLAY_ABSTRACTION_MODEL.md)。

Stage 7A 是 Stage 8 的硬前置，只交付最小可玩的 Input / Judgement / Score / Replay kernel。
Stage 7B+ 是长期能力线；Stage 8 只接收明确列入发行矩阵并已完成本计划门禁的能力，不等待全部
7B+。任意运行时脚本、逐帧脚本回调、宿主字节码和通用 Script VM 始终不在 Stage 7 范围内。

## 阶段目标

Stage 7A 的目标是冻结并交付一个可由 Chart v5 candidate、Player、Headless consumer、Reference
Host 和后续 Studio 共同消费的最小判定闭环：输入规范化、要求准备、Tap/Hold/Release 求值、
Ruleset 折叠、Score/Combo/Statistics、Replay identity 以及无损 Snapshot/Seek。Stage 7A 关闭
必须同时收口 Stage 6 登记的四项前置，并把可核验的合同、golden、拒绝路径和预算证据交给 Stage 8。

Stage 7B+ 的目标是以独立 capability 扩展连续输入、方向动作、多接触点、复杂持续约束、校准和
Ruleset 演进。每项能力都有自己的版本、identity、Replay、预算、稳定拒绝和发行选入决策，不能
通过修改 7A 已冻结字段含义来获得隐式支持。

## 1. 产品边界和不变量

Stage 7 的稳定数据流为：

```text
InputEvent
  -> InputMapping / normalization
  -> prepared JudgementRequirement
  -> JudgementFact / JudgementResult
  -> Ruleset Fold
  -> Score / Combo / Statistics
  -> read-only Presentation or host feedback
```

必须保持以下边界：

1. L1 Input 只负责设备事件、映射、连续量重采样和 Replay 规范化；L2 Program 只负责
   Requirement 的 Pattern、Measure、归属声明和 Fact；L3 Ruleset 负责仲裁、折叠、模块和计分；
   L4 Presentation 只读结果和有限 Effect Schedule。
2. Judgement 不读取渲染帧率、GPU、World、EnTT、SDL、OpenGL、音频后端或宿主引擎类型。结果
   由绝对观测时间和显式快照重建，不以“上一帧结果”作为未声明的语义状态。
3. 判定域是抽象的 `InputDomain`、离散位置、区域、通道或经注册的几何域；公共 Judgement API
   不绑定 lane、Note、屏幕像素、键盘布局或具体渲染后端。
4. 任何新 RequirementKind、Pattern、Outcome、Hook、几何域或策略都必须有版本、capability、
   稳定拒绝诊断和独立 identity 影响说明；不支持内容不得静默降级为 Tap、Hold 或旧版本语义。
5. Score、Combo、Life（若启用）、Statistics、Gap、接触归属和 Replay 游标均属于会话状态，
   不回写 Chart、Behavior、World 或作者源。
6. 运行期不执行 IO、随机数、墙钟、宿主回调或动态 Requirement 生成。声明式 Behavior、有限
   Effect Schedule 和 Judgement Result Binding 可消费结果，但不能改变已经产生的判定事实。

### 1.1 权威输入和冻结顺序

实施前必须按以下顺序处理文档状态：

```text
ADR 0043 owner acceptance
  -> Gameplay Judgement Spec / ABI 由 ADR 引用的字段和语义冻结
  -> Stage 7A typed contract review
  -> 实现批次和 golden
  -> Stage 7A 关闭
```

如果实现阶段发现 Spec、ABI、研究证据或真实内容预算之间矛盾，先建立可复现失败报告并停止
受影响批次；不得在代码中隐式选择替代语义。修改四层边界、identity 分区、grace 语义、
Pattern 原语、仲裁顺序或拒绝策略，必须先更新 ADR/Spec，再重新验证全部受影响批次。

### 1.2 Stage 6 交接前置

Stage 6 关闭后由 Stage 7A 承接四项未完成内容。它们是关闭前置，不是“以后再补”的 backlog：

| 交接项 | Stage 7A 必须交付的结果 | 关闭时最低证据 |
| --- | --- | --- |
| 显式 candidate 与实验隔离 | `Cuexis_ALLOW_EXPERIMENTAL`、candidate flavor/安装元数据、Player `--candidate-entry` 与项目/CXC 入口、至少一个真实启用 candidate 的 preset 或 CI | 默认 OFF 与显式 ON 的构建/安装/运行矩阵；production consumer 证明不会误启用 candidate；错误入口稳定拒绝 |
| 离线 typed assembler 与 feature 派生 | 从 typed Chart/CXT/source 输入派生 feature、requirements、resource closure 和 capability 的离线入口；禁止测试注入 feature 作为生产后门 | 正例、缺字段、非法引用、预算、确定性 identity 和原子失败用例；Playback 不展开 CXT |
| 具名宿主六动词命令循环 | 对现有 Reference Host 的 load/play/pause/stop/seek/reload 交接回归，不重新实现 R9 | 同一 SHA 的 hosted 回归、命令状态矩阵、错误/旧状态保持证据 |
| SDK API `0.7.1` | 先修订 version-gate 的条件化放行机制，再按版本规范落实 additive API 版本；若 owner 接受例外，必须有 ADR/版本规范记录 | version gate 的默认拒绝、一次显式放行、0.7.0 consumer 兼容、fresh/clean build、安装元数据和 hosted 证据 |

四项中任一项既未实现也没有被 ADR 或所属 Spec 明示接受的例外，Stage 7A 不得关闭。Stage 6
的关闭事实不被改写；只把剩余证据归入当前阶段。

## 2. 总体依赖和发布路线

```text
S7A-0 基线和合同准入
  -> S7A-1 类型/模块边界
  -> S7A-2 输入规范化和映射
  -> S7A-3 Requirement prepare / typed assembler
  -> S7A-4 Kernel lifecycle、Tap/Hold/Release、仲裁
  -> S7A-5 Fold、Score、Combo、Statistics
  -> S7A-6 Identity、Replay、Snapshot、Seek
  -> S7A-7 Playback/Chart candidate/Player/Headless 集成
  -> S7A-8 四项 Stage 6 交接收口
  -> S7A-9 跨平台硬化、Stage 8 handoff、owner acceptance

S7A-1 + S7A-2 + S7A-3
  -> S7B-0 capability registry / extension harness
      -> S7B-1 连续输入和 Slide/handoff
      -> S7B-2 Flick / direction / velocity
      -> S7B-3 多接触点、和弦和资源占用
      -> S7B-4 复杂 Hold、尾判、Roll/Count/Spinner 类约束
      -> S7C-1 calibration / device policy
      -> S7C-2 ruleset package / module / replay evolution

S7A-9 ------------------------------> Stage 8
已选 7B+ capability + 独立发行矩阵 ------> 可选地加入 Stage 8
未选 7B+ capability -------------------> 稳定拒绝，继续在 Stage 8 后交付
```

允许并行：S7A-2 的设备适配研究、S7A-3 的 typed prepare 表征、S7B-0 的 capability 目录可以
在合同准入后并行准备。涉及同一公共头、CMake target、安装导出、Replay 版本或 identity 算法的
集成必须串行；下游 golden 通过不能替代上游合同、失败路径和架构检查。

Stage 8 的发行矩阵必须在其启动前写出：对每项 7B+ capability 标注 `included`、`candidate`
或 `rejected/deferred`，并附 capability、Packed/CXC 表达、Replay/identity、预算和拒绝测试
入口。未写入矩阵的 7B+ 能力不得被 v5 默认 Writer 或 Player 默认入口隐式启用。

## 3. Stage 7A 交付批次

### S7A-0：实施准入、基线和合同表征

**目的。** 把候选 ADR/Spec/ABI 转成实施者可执行的合同清单，并确认实现前的仓库和工具链基线。

**工作内容。**

1. 记录执行时 HEAD、分支、工作区、UTC 日期、SDK API、显示版本、工具链、preset、依赖和
   hosted workflow 入口；不要把历史报告 SHA 当作本次证据。
2. 对 ADR 0043 的四层边界、Tick 顺序、`leftmost-first`、`consume/observe/claim`、
   `fanout`、`sameContact`、prepared grace、四分量 identity、快照字段和拒绝诊断建立逐项
   contract matrix。
3. 由 owner 接受 ADR 0043，并明确哪些字段仍为候选；对生产预算只登记待测量的上界，不把
   96 条研究切片、4,777 字节快照或建议值写成 ABI 常量。
4. 以现有 Core `Result<T,E>`、公共头 ASCII、target allowlist、架构检查和静态/shared
   package gate 为实现约束；列出新增 target、依赖和安装组件。
5. 为每个后续批次指定正例、负例、诊断类别、golden、执行命令、预期证据文件和停止条件。

**输出。** dated baseline report、accepted contract checklist、支持/拒绝矩阵、依赖与安装图、
预算测量计划、四项 Stage 6 交接台账、未决问题清单。

**退出门禁。** ADR/Spec/ABI 状态已明确；所有未决语义均有 owner 决策或阻塞项；无“实施时再
决定”的公共字段；基线 Debug/Release、文档和既有 package/architecture gate 结果已记录。

### S7A-1：Typed Kernel 边界和模块骨架

**目的。** 建立不泄露实现依赖的内部/preview C++ 边界，不先接入 Player 或渲染器。

**必须冻结的对象。** `Tick`/时间区间、`InputEvent`、`InputDomain`、`Action`、位置/通道、
`Amount`、`SourceIdentity`、sequence、`JudgementRequirement`、`PatternRef`、`MeasureSpec`、
`GripPolicy`、`JudgementFact`、`JudgementResult`、`JudgementIdentity`、诊断和 capability。
实际命名、整数宽度、所有权、异常边界和序列化由实现批次与 ABI 一起冻结。

**工作内容。**

1. 选择模块位置和 target 方向，使 Input/Judgement/Replay 不依赖 SDL、OpenGL、Audio、World、
   EnTT 或 JSON DOM；公共 header 只出现 Cuexis 类型和已批准的 `Result`。
2. 建立 create/configure/prepare/submit/advance/query/snapshot/seek/reset 生命周期骨架；prepare
   失败不得产生半准备会话，运行期不得修改 requirement 集合、grace、仲裁策略或 Interface 投影。
3. 为所有可变状态标注 owner thread、读取时点、快照归属和重置行为；析构、实时路径和公共
   边界不得抛异常。
4. 为诊断建立稳定 code/category、字段路径、requirement/identity 分量和预算上下文；不把
   unsupported capability 映射成旧 kind。

**验证。** 头文件泄漏扫描、依赖 allowlist、架构脚本、静态/shared 安装 consumer、异常和
   ownership 编译检查；无 GPU 的空会话、prepare 失败和销毁路径。

### S7A-2：输入规范化、映射和时钟边界

**目的。** 使设备输入进入唯一、可回放、与渲染解耦的规范流。

**工作内容。**

1. 定义离散边沿与连续量的边界：离散 press/release 保留原始观测时间；连续量按 Interface
   声明的格点线性重采样；最低上报率不足时稳定拒绝。
2. 实现显式 `InputMapping`，覆盖键盘、鼠标、触摸、手柄和外部控制器的最小映射；映射的
   source identity、版本、量化和默认值进入 session identity。
3. 规定到达时间、设备时间、音频/Chart 时间、渲染帧时间和 `observationTime` 的转换；时间
   回退、越界、重复 sequence、跨 discontinuity 和负时间都要有明确行为。
4. 确定联合采样相位：连续量重采样格点与 `every`/周期 predicate 的唤醒相位必须有可计算的
   最坏延迟；不得只验证各自相位而遗漏叠加延迟。
5. 使实时输入和 Replay 规范事件走完全相同的 normalize/submit 路径；原始设备事件可以留在
   诊断中，但不是 Replay 语义输入。

**验证。** 同一设备类别不同上报率、相位、时间抖动、输入顺序、重复 sequence、低于最低
   上报率、采样边界和 discontinuity 的 golden；跨 GCC/clang/MSVC 的规范事件逐字节一致。

### S7A-3：Requirement prepare、编译和离线 typed assembler

**目的。** 把 Chart/CXT typed 数据变为只读、可测量、带 identity 的 prepared requirement；
Playback 不读取 CXT AST，也不在运行时展开未编译 Pattern。

**工作内容。**

1. 实现 typed Chart/CXT/candidate 输入到 Requirement/Pattern/Measure/Effect 的离线 assembler，
   派生 feature、capability 和 resource closure；Writer 不替调用方修复缺失 feature。
2. 实现 Pattern 原语 atom、sequence、choice、bounded repeat、skip、sameContact、instant、
   complement；删除积；非确定匹配固定为 `leftmost-first`。检查 Pattern 与 arm/deadline 的包含
   关系、终止性、状态数、展开数量和时间/内存预算。
3. Measure 允许多个 phase/category 分量，逐分量套用 grading；Hold 至少覆盖 head/body，
   Release/tail 仅在明确声明时进入 7A 或 capability。
4. 对 `handoff` Hold 和接触跟随 Slider 只在 prepare 阶段解析 grace：量化、范围、继承、
   `allowChartGrace`、sticky/observe 非法覆盖和 `graceResolutionPolicy` 均稳定诊断；准备后
   只读最终值。
5. 生成 chart/content/prepared identity 的规范字节；相同最终 grace 与 policy 即共享 judgement
   identity，显式/继承来源只进入 content identity 和诊断。
6. 失败必须原子：非法引用、未知 capability、pattern 不终止、范围溢出、资源冲突、预算超限
   或 identity collision 时不发布半成品、不改变已有 active session。

**验证。** typed file/memory source 一致性；CXT/Candidate 正例和负例；输入数组、参数和引用
   顺序置换不改变 canonical bytes；真实内容预算测量；Pattern 参考谓词与编译产物差分；宽整数
   范围证明覆盖平方、乘法、加法和窄化；不把研究切片测量直接变成生产限额。

### S7A-4：Judgement Kernel 生命周期、Tap/Hold/Release 和仲裁

**目的。** 交付最小、无渲染、确定性的可玩判定。

**固定 Tick 顺序。**

```text
1. 激活到期 requirement
2. 处理到期 Gap / hard-deadline timer
3. 应用规范化输入事件
4. 每个事件重建候选并按 claimKey / requirementId / fanout 仲裁
5. 按 (requirementId, phase, outcome) 折叠前规范排序 Fact
6. Tick 末提交 Hook/signal，下一 Tick 才可见
```

**工作内容。**

1. 实现 Tap 的时间、域、动作和位置匹配；明确 `[start,end)`、严格边界、重复输入、迟到输入、
   stray 和 Miss。
2. 实现 Hold 的 head、body、deadline、release 和 coverage；头部和主体是可同时存在的独立
   Fact，不互相撤销；未进入 7B 的复杂尾判不得偷渡为默认 Hold 语义。
3. 实现 `observe`、`consume`、`claim`、`sameContact`、槽位复用、`strayWhen`、`fanout`、
   note lock 等仲裁规则；handoff 恢复按普通 consume 候选集竞争，不能绕过新音符。
4. 实现 `handoff + Gap` 的基础生命周期和 prepared grace；`event.t < releasedAt + grace`，
   `grace=0` 不恢复，Hold deadline 为 `end+grace`。Slider continuity 只有在 S7B-1 进入时才
   可用，不在 7A 默认 capability 中打开。
5. 处理同 Tick 输入、实例枚举、输入数组顺序和 timer 顺序，保证规范排序消除实现容器顺序影响。

**验证。** Tap/Hold/Release 全边界矩阵；同域重叠、重复/冲突输入、sticky/handoff、observe、
   fanout、槽位复用、stray 两种政策、暂停/Seek/reload/audio discontinuity；渲染帧率、World
   遍历和后端替换不会改变 Fact。

### S7A-5：Ruleset Fold、Score、Combo 和 Statistics

**目的。** 将 Fact 以声明式、可重放的 Ruleset 逻辑折叠为会话结果。

**工作内容。**

1. 实现 Ruleset Interface、arbitration、fold、modules、defaults、Hook ownership、programPolicy
   和 outcomeScope 的只读 prepared 视图；模块顺序冻结在 package manifest。
2. 明确同一写目标唯一 owner；派生 Hook 使用可交换、可结合的合成算子；信号只在下一 Tick
   可见；折叠不得生成/删除 Requirement 或改写已产生 Fact。
3. 定义 `JudgementOutcome`、grade、timing error、ScoreState、ComboState、可选 LifeState 和
   `StatisticsSnapshot` 的初始值、增量、溢出/饱和及 reset/seek 行为。
4. 把内容、Ruleset、Loadout 和 session 配置对结果的影响写入 identity；表现资源、默认绑定和
   UI 文案不进入 judgement identity。

**验证。** 同 Tick 多 Fact 顺序置换、模块顺序变更、Hook 冲突、信号延迟、combo 断连、life
   边界、长局累计/饱和、reset/seek/replay 结果一致性；Ruleset 缺 capability、未知 outcome、
   非法 owner 和 package hash 冲突稳定失败。

### S7A-6：Identity、Replay、Snapshot 和 Seek

**目的。** 使结果可验证、可重放、可定位并能在不依赖渲染的情况下恢复。

**工作内容。**

1. 落实四分量 identity：`engine`（判定语义、tableId、阶段顺序）、`ruleset`（Interface 投影、
   模块/折叠顺序、Build hash、能力）、`chart`（编译 Pattern/Measure、锚点、量化 grace）、
   `session`（Loadout、默认 grace、重采样格点和 JudgementConfig）。
2. Replay 头部包含格式版本、四分量 identity、规范化事件数、字节数和解码预算；事件流使用
   S7A-2 的规范事件，不保存不可复现的设备私有对象。
3. 实现非法版本、identity mismatch、事件乱序、时间回退、截断、重复、超事件数/字节数、
   解码时间超限和未知 capability 的稳定错误。
4. 快照无损保存活动实例、Pattern 状态、接触归属、Gap timer、releasedAt、prepared grace、
   采样相位、Hook 快照、Fold 状态、统计和 signal queue；第一版采用全量快照。
5. Seek 从最近快照推进，必须逐位等于从起点运行；快照间隔为引擎内部参数，不进入 identity，
   `maxSeekLatency` 是引擎承诺，会话只能收紧。

**验证。** 录制/回放、跨进程/跨工具链 golden、截断和篡改、不同快照间隔、任意 seek 点、
   pause/reload/discontinuity、长事件流和预算峰值；Replay 与实时输入的 Result/Score/Combo/
   Statistics 逐项一致。

### S7A-7：Playback、Chart candidate、Headless、Player 和 external consumer 集成

**目的。** 将 kernel 接入既有产品边界，不把 Judgement 类型倒灌到 Runtime 或 Presentation。

**工作内容。**

1. 在 PlaybackSession/Prepared Playback 中增加显式创建和查询入口，保持 owner-thread、Result、
   生命周期和公共头 ASCII 规则；旧 Playback 入口行为不因无输入而改变。
2. 让 Chart v5 Core/Packed candidate 提供 S7A-3 所需的 prepared requirements 和 capability；
   v4/CXT v1 回退继续工作，未支持的 v4/v5 内容稳定拒绝。
3. 无 InputEvent 的纯播放和 Studio Preview 中 Judgement 保持休眠，不能改变 FrameSnapshot、
   FrameDigest 或 Presentation candidate。
4. Player 仅显示/播放 JudgementResult、Combo、Score、Miss 和基础反馈；不得在 Player 内维护
   私有 Runtime 或第二套判定路径。
5. 建立不带 GPU 的 headless consumer、安装树 static/shared consumer 和具名 Reference Host
   回归；实时输入与 Replay 必须经过同一公共入口。

**验证。** v4、v5 candidate、file/memory source、Playback/Headless/Player/external consumer
   的生命周期矩阵；默认 production 与 candidate opt-in 隔离；无 SDL/OpenGL/EnTT/JSON DOM 泄露；
   prepare/submit/advance/snapshot/seek/reset 的成功和失败事务均覆盖。

### S7A-8：Stage 6 交接收口和版本门禁

**目的。** 在不重开 Stage 6 的前提下，完成四项登记的处置和 SDK/实验入口的可追溯性。

**工作内容。**

1. 实现并验证 candidate 构建/安装 flavor、显式 entry、Player `--candidate-entry`、项目/CXC
   metadata 选择和默认 OFF 保护；至少一个 preset 或 CI 真实开启 candidate 并被测试消费。
2. 接入离线 typed assembler 和 feature/resource closure 派生；移除测试注入 feature 的生产后门，
   记录 candidate 输入到 prepared requirement 的完整 identity 链。
3. 对 Reference Host 的六动词命令循环做同 SHA 交接回归，覆盖成功、拒绝、旧 active 状态和
   reload/seek/discontinuity；不把旧报告复制成新证据。
4. 先提交 version-gate 的条件化放行设计：默认阻止 SDK API 变化，只有显式、可审计的放行输入
   才允许 0.7.1；再运行 update_version、fresh configure、clean-first build、安装 consumer 和
   hosted gate。若 owner 接受不升版本，必须在 ADR/版本文档记录例外和后续归属。

**退出门禁。** 四项均有实现和门禁证据，或有已接受的明确例外；任何一项不能以“Stage 6 已关闭”、
   “未来 Stage 8 会处理”或本地脚本单独通过替代。

### S7A-9：最终硬化、跨平台证据、Stage 8 handoff 和关闭

**目的。** 把所有实现证据收敛成可审查的 Stage 7A 关闭包。

**工作内容。**

1. 运行 Debug/Release/shared/headless/MinGW 以及 Linux Quality、Windows MSVC、Windows MinGW
   hosted 矩阵；sanitize/coverage/clang-tidy 只按仓库支持的平台执行，不能伪造 Windows 等价证据。
2. 对代表性真实内容和最坏内容测量 requirement 数、activity、Pattern 状态、Fact、Replay、
   快照、Seek、prepare 峰值和运行时间；分别记录硬门禁、软目标和环境限制。
3. 运行架构、target allowlist、公共头泄露、static/shared package、external consumer、version
   gate、docs、git diff check 和许可证检查；GPU/真实设备/音频证据单独标记。
4. 建立 Stage 8 handoff：S7A typed contract、v4/v5 输入映射、requirement/Replay identity、
   golden fixture、capability/拒绝表、预算 profile、迁移注意事项和未纳入 7B+ 清单。
5. 形成 dated completion report、问题 disposition、未执行项、回滚路径和 owner acceptance；未
   满足任何硬门禁时状态保持 active/blocked，不缩小矩阵宣称完成。

### 3.1 Stage 7A 小目标核验台账

下表把 S7A 批次拆成可以独立分派和独立退出的小目标。每个小目标必须单独记录实现 SHA、测试
fixture、正例、负例、命令、原始输出和未覆盖项；“依赖批次已完成”不能替代本行核验。

| ID | 小目标 | 必要产物 | 核验标准 |
| --- | --- | --- | --- |
| S7A-0.1 | 固定执行基线 | dated baseline、HEAD/分支/工具链/preset 表 | 工作区状态、版本、SDK API、依赖和 hosted 入口均可复现；重新运行同一命令得到同类基线；历史 SHA 未被冒充为当前证据 |
| S7A-0.2 | 合同逐项台账 | ADR/Spec/ABI → 字段、行为、诊断、owner 的映射表 | 每个公共字段都有来源、消费者、默认值、失败路径和 identity 影响；不存在“实现时决定”的开放项 |
| S7A-0.3 | 预算与证据计划 | 内容、稳态、快照/Seek、Replay、Packed 五类 profile 草案 | 每类预算有计数口径、测量输入、硬/软属性和超限动作；研究切片数值明确标为非生产限额 |
| S7A-0.4 | ADR 0043 准入 | owner acceptance 记录和 revision | owner 接受前禁止进入公共头和实现批次；若有例外，例外包含范围、期限、替代门禁和责任人 |
| S7A-1.1 | 公共 typed 类型 | 头文件、Result/诊断枚举、生命周期声明 | 外部 consumer 可编译；公共头无 SDL/OpenGL/EnTT/World/JSON DOM/第三方实现类型；非 ASCII 扫描通过 |
| S7A-1.2 | 会话状态所有权 | owner-thread、borrow/own、快照字段表 | 每个可变字段有唯一 owner 和读取时点；并发/重复调用/销毁测试不产生数据竞争或悬空引用 |
| S7A-1.3 | prepare/运行期事务 | create→prepare→submit→advance→query→reset 骨架 | prepare 任何失败均无半准备会话；运行期修改 requirements、grace、规则投影被稳定拒绝；旧 active 状态保持不变 |
| S7A-1.4 | 诊断合同 | code/category、字段路径和 identity 分量表 | identity mismatch、unsupported、invalid、budget、time、overflow 等最小类别逐一可触发；同一错误在不同容器顺序下诊断稳定 |
| S7A-1.5 | 模块与依赖门禁 | CMake target/allowlist/安装图 | architecture test、target allowlist、static/shared 安装 consumer 和 headless 配置通过；不得由测试 target 偷渡依赖 |
| S7A-2.1 | 绝对时间域 | Tick、区间、单调推进和 discontinuity 规则 | `[start,end)`、负时间、回退、相等边界、超范围和 pause/seek/reload 均有正反例；渲染帧时间不影响结果 |
| S7A-2.2 | InputMapping | 键盘/鼠标/触摸/手柄/外设最小映射 fixture | 相同映射规范 bytes 和 identity 一致；未知 device/action/channel、重复绑定和越界量程稳定拒绝 |
| S7A-2.3 | 离散事件规范化 | press/release/hold source fixture | 离散边沿保留原始观测时间；到达顺序、sequence、重复事件和跨设备 source identity 的处理符合 Spec |
| S7A-2.4 | 连续量重采样 | 1/4/8 ms 和最低上报率 fixture | 同设备类别在声明最低上报率以上得到相同格点；低于下限拒绝；插值、边界和最大延迟有可测上界 |
| S7A-2.5 | 联合采样相位 | 重采样格点与周期 predicate 组合 fixture | 最坏唤醒/检出延迟按组合路径计算并记录；不存在只测单一采样相位的漏证据 |
| S7A-2.6 | 规范事件编码 | canonical event bytes 和 replay input adapter | 实时与 Replay 输入经过同一 normalize/submit；字段、顺序、量化和 source 版本跨工具链逐字节一致 |
| S7A-3.1 | typed source 读取 | Chart/CXT/candidate typed adapter | file/memory 输入等价；JSON DOM 不越过 json_support；缺失、重复、未知字段按合同诊断 |
| S7A-3.2 | Pattern 编译 | Pattern IR/compiled state、reference predicate | atom/sequence/choice/repeat/skip/sameContact/instant/complement 均有最小和边界 fixture；非终止、窗口外匹配和状态预算稳定拒绝；短轨迹穷举与参考谓词一致 |
| S7A-3.3 | Measure/phase 编译 | 多分量 Measure、phase/category、grading fixture | Hold head/body 的 Fact 分离且等级独立；分量顺序置换不改变规范结果；未声明 tail/release 不被隐式启用 |
| S7A-3.4 | prepared grace | explicit/inherited/zero/min/max/sticky/observe fixture | `allowChartGrace`、范围、量化、policy 和来源诊断正确；prepare 后 Hook 改变不追溯已准备值；最终值和 policy 正确影响 judgement identity |
| S7A-3.5 | canonical identity | chart/content/prepared identity 预映像 | 输入数组、参数、资源引用顺序置换不改变 canonical identity；改变语义字段必改变相应分量；表现资源不污染 judgement identity |
| S7A-3.6 | prepare 原子性和预算 | overflow、reference、closure、budget failure fixture | 任一失败不发布部分 prepared chart、不改变 active session；诊断指出字段/requirement 和实际计数；无 checked arithmetic 溢出 |
| S7A-4.1 | Tick scheduler | 固定六步顺序的 trace fixture | 每一步产生的可见状态、timer 和 signal 与 Spec 一致；同 Tick 的输入不会看到本 Tick 新 Hook；阶段顺序变更会改变 engine identity 或被拒绝 |
| S7A-4.2 | Tap | 时间/域/动作/位置边界 fixture | 命中、早到、晚到、正好边界、重复、冲突、无候选和 Miss 的 Fact、eventSequence、stray 完全符合预期 |
| S7A-4.3 | Hold | head/body/release/deadline/coverage fixture | 头部与主体可同时存在；中途断开、末端释放、超 deadline、零 grace 和严格 `<` 边界均稳定；不依赖帧率 |
| S7A-4.4 | 基础仲裁 | overlap、claimKey、fanout、observe、strayWhen fixture | 每事件候选逐次重算；实例枚举和输入数组置换不改变赢家；observe 可见但不消费；`noCandidate` 与 `consumeEmpty` 区分正确 |
| S7A-4.5 | Gap/handoff 基础 | sticky/handoff、恢复竞争、timer fixture | 恢复只进普通 consume 候选；`event.t < releasedAt + grace`；timer 只产生一次 Break；新头部可按 claimKey 获胜 |
| S7A-4.6 | 生命周期重建 | pause/seek/reload/audio discontinuity fixture | 同一绝对目标时间重建出的活动实例、Fact 和 timer 与连续播放一致；不把上一帧结果作为新基线 |
| S7A-5.1 | Ruleset prepare | interface/arbitration/fold/module manifest | 缺 module、unknown outcome、Hook owner 冲突、programPolicy/outcomeScope 非法均在 prepare 拒绝；manifest 顺序进入 ruleset identity |
| S7A-5.2 | Fact fold | 同 Tick 多 Fact 顺序置换 fixture | `(requirementId, phase, outcome)` 排序后结果稳定；fold 不生成/删除 Requirement、不改写 Fact；同一写目标只有一个 owner |
| S7A-5.3 | Score/Combo/Life | 命中、Miss、断连、饱和、reset fixture | 初始值、增量、负值、饱和、连击断开和可选 Life policy 明确；长局无未定义溢出 |
| S7A-5.4 | Hook/signal 可见性 | 同 Tick/下 Tick signal fixture | Hook 在 Tick 末提交、下一 Tick 可见；可交换/可结合派生值顺序无关；非交换冲突稳定拒绝 |
| S7A-5.5 | Statistics snapshot | 统计查询和快照 fixture | 查询不修改状态；reset/seek/replay 后计数逐项一致；统计字段与 identity/版本关系明确 |
| S7A-6.1 | 四分量 identity | engine/ruleset/chart/session golden | 改变各分量的语义字段只影响对应分量；同最终 grace/policy 的 explicit/inherited judgement identity 相同，content/diagnostic 保留来源差异 |
| S7A-6.2 | Replay header/codec | 版本、identity、事件/字节预算 fixture | 正常编码可往返；截断、乱序、重复、未知版本、超限和 mismatch 稳定失败；解码不执行 IO/脚本 |
| S7A-6.3 | 全量 snapshot | 活动实例、Gap、采样、Hook、Fold、统计 fixture | snapshot bytes 可独立校验；恢复后同一尾部输入 Fact/Score/Combo/Statistics 逐位一致 |
| S7A-6.4 | Seek | 多快照间隔、多目标时间 fixture | 任意 seek 点与从起点运行结果一致；`maxSeekLatency` 实测与承诺分开记录；目标时间非法时旧状态保持 |
| S7A-6.5 | Replay 与实时等价 | 同一事件的双路径对照 | 实时和 Replay 的 normalize、Fact、Score、Combo、Statistics 和错误完全一致，不仅最终分数相等 |
| S7A-7.1 | Playback 生命周期入口 | PlaybackSession/Prepared Playback integration | 无输入时旧 FrameSnapshot/FrameDigest 不变；submit/advance/query/snapshot/seek/reset 线程和寿命符合既有合同 |
| S7A-7.2 | v5 candidate 消费 | typed Core/Packed candidate fixture | source→prepared→runtime 的 requirements、identity、resource closure 一致；不支持 candidate 稳定拒绝；v4 fallback 保持通过 |
| S7A-7.3 | Headless consumer | 无 GPU 的 executable/CTest fixture | 在无 SDL/OpenGL/GPU 环境中完成 prepare、判定、Replay、Seek；输出与 Player/实时路径 golden 一致 |
| S7A-7.4 | Player 反馈 | 最小 feedback fixture | Player 只消费 JudgementResult/Score/Combo/Miss；不创建私有 Runtime 或第二判定路径；无输入纯播放行为不变 |
| S7A-7.5 | 安装 external consumer | clean staging static/shared consumer | 安装树外编译、正确链接、公共头泄漏/ASCII/版本拒绝通过；candidate 入口不由默认 production target 暴露 |
| S7A-8.1 | candidate 隔离 | default-OFF、explicit-ON、wrong-entry fixture | 默认构建/安装不能加载 v5 candidate；显式 entry 可用；错误 flavor、裸 Packed 和缺 metadata 稳定拒绝；至少一个 preset/CI 真正开启并消费 |
| S7A-8.2 | 离线 assembler 生产入口 | chart-candidate tool 与 closure report | feature/resource closure 由 typed assembler 派生；测试注入后门被检测；输入到 prepared identity 有完整链路 |
| S7A-8.3 | Reference Host 交接 | 同 SHA 六动词命令回归 | load/play/pause/stop/seek/reload 状态矩阵、失败保持旧 active、discontinuity 和重载证据完整；不复制历史报告代替当前运行 |
| S7A-8.4 | SDK 0.7.1 门禁 | version-gate change、放行记录、consumer matrix | 默认变更仍失败；显式一次性放行可审计；0.7.0 consumer source-compatible；fresh/clean build、安装元数据和 hosted gate 一致 |
| S7A-9.1 | 本地最终矩阵 | Debug/Release/shared/headless/MinGW logs | 所有批次 fixture、architecture、allowlist、package、docs 和 version gate 在最终 SHA 运行；失败按责任归属，不沿用旧日志 |
| S7A-9.2 | hosted 三平台矩阵 | Linux Quality、Windows MSVC、Windows MinGW evidence | 同一最终行为 SHA 被验证；本地与 hosted 配置差异列明；缺 GPU/设备项单列为未执行或独立证据 |
| S7A-9.3 | 预算和回归报告 | machine-readable measurements、regression report | 内容/稳态/快照/Replay/Packed 计数口径分开；实测与阈值分开；容量变化能追溯到 fixture/commit |
| S7A-9.4 | Stage 8 handoff/owner acceptance | handoff checklist、completion report | 交接物、未纳入 7B+、回滚路径和开放问题齐全；owner 明确接受；否则计划保持 active |

## 4. Stage 7A 关闭标准

Stage 7A 只有同时满足以下条件才能标记 completed：

1. 相同 Chart、InputEvent、InputMapping、Timing、Ruleset、Loadout 和 JudgementConfig 产生逐位
   一致的 Fact、JudgementResult、Score、Combo 和 Statistics。
2. 渲染帧率、World/Entity 遍历顺序、输入数组顺序、容器顺序、后端替换和不同快照间隔不改变
   结果；Seek/Reload/Pause/Audio discontinuity 后从同一目标时间重建得到相同结果。
3. Tap/Hold/Release 的半开区间、严格边界、重复输入、冲突输入、Miss、stray、observe、claim、
   fanout、slot reuse 和 prepared grace 都有正反 golden；未纳入 7B 的能力稳定拒绝。
4. 实时输入与 Replay 走同一 Judgement 路径，Replay identity、版本、事件/字节预算和篡改/截断
   失败可诊断；快照无损，恢复后尾部输入结果逐位一致。
5. 判定系统不暴露 World、EnTT、SDL、OpenGL、JSON DOM、宿主 SDK 或私有 Player 类型；安装公共
   头纯 ASCII，static/shared、headless 和 external consumer 生命周期测试通过。
6. Chart v5 Core/Packed candidate 能消费 S7A requirements 并与 typed source 得到相同判定结果；
   v4 回退和无输入 Playback 行为保持兼容。
7. candidate 与 production 隔离、离线 typed assembler、Reference Host 交接回归和 SDK API `0.7.1`
   版本门禁四项已逐项实现并有证据，或已由 ADR/Spec 明示接受例外。
8. 真实内容预算、跨平台 hosted、架构/package/version/docs 检查和 owner acceptance 完整；研究
   spike 只作为背景，不被误写成产品实现或公共限额。

## 5. Stage 7B+ 能力线的统一规则

### 5.1 新能力准入包

每个 7B+ 能力在开工前必须有一份 capability proposal，至少包含：

```text
capability name and stable ID
RequirementKind / Pattern / Measure / Ruleset ownership
prepare-time fields and quantization
runtime state additions and snapshot representation
InputDomain / Action / mapping requirements
arbitration and ownership interaction
timing, geometry and numeric range
identity components and Replay version impact
budget profile and worst-case limits
unsupported/rejection diagnostics
v4/v5/CXC/Packed representation decision
headless golden and migration strategy
Stage 8 inclusion decision: included / candidate / deferred / rejected
```

能力不能通过修改 7A 已发行字段含义来“顺便支持”。如果不能保持旧 Replay 或旧 v5 语义，必须
创建新的 semantic version/revision，并在 capability negotiation 和 replay header 中显式出现。

### 5.2 统一实现和退出门禁

每个能力都必须经过：

```text
proposal + threat model
  -> independent reference predicate / model
  -> typed prepare and static validation
  -> runtime implementation
  -> deterministic golden and differential tests
  -> Replay / identity / snapshot tests
  -> budget and cross-toolchain tests
  -> capability / rejection / migration tests
  -> selected release matrix and owner acceptance
```

研究性实现、手写条目、单平台结果和只测正例的 golden 都不能单独退出。实现与参考模型必须
至少有短轨迹穷举；长轨迹可用固定种子随机化补充，但若结果依赖自动机/Pattern 编译，须增加
编译产物与独立谓词的等价性检查。跨编译器结果差异先归为阻塞，不能以“整数运算应当确定”
解释；所有几何平方、乘法和加法必须有范围证明或显式拒绝。

## 6. Stage 7B+ 分批规划

### S7B-0：Capability registry、版本和扩展测试骨架

**目标。** 在加入高级判定前，先建立可查询、可拒绝、可重放的扩展框架。

**范围。** capability ID/revision、RequirementKind 注册、支持矩阵、prepare preflight、稳定
拒绝、identity/replay revision、测试 fixture registry、Packed/CXC entry 标记和运行期只读查询。

**不做。** 不实现具体 Slide/Flick；不开放动态插件、运行时脚本、宿主回调或未审查的 Ruleset
字节码。

**退出。** 未知 capability、已知但未启用 capability、revision 不匹配、预算不足和旧 Replay
在新引擎上的行为均稳定；7A capability 查询和拒绝回归保持全绿。

### S7B-1：连续输入、区域采样、Slide 和 handoff continuity

**目标。** 在 7A `InputEvent` 和 `handoff + Gap` 基础上支持接触轨迹和 Slider，不改变 Tap/Hold
基础语义。

**范围。** 连续 position/amount 规范格点、区域/轨迹域、局部坐标到判定域的量化、segment 进度、
handoff Hold、Slider continuity grace、尾部 deadline、sameContact 和多段 coverage。

**固定边界。** `event.t < releasedAt + grace`；prepared per-requirement grace；恢复按普通
consume 仲裁；Hold deadline 为 `end+grace`；Slider deadline 为 `t1+max(window.maxLate,grace)`；
Gap 中保留段号和进度；不新增运行期 Hook、`reconnect` Fact 或 Pattern 原语。

**必须验证。** 1/4/8 ms 上报率、重采样相位、插值误差、轨迹边界含/不含、连续/断开/恢复、
sticky 与 handoff、多个 Gap 重叠、Slider 段不回退、C10-C14 矩阵、快照恢复、新头部竞争、
跨编译器宽整数一致性和真实几何量程。

**退出产物。** capability Spec/ABI 增量、区域/轨迹 golden、Replay identity、预算 profile、
Packed 表达或稳定拒绝、Stage 8 inclusion 建议。连续 Slider 不能因“研究 spike 通过”自动进入
Stage 8 默认发行。

### S7B-2：Flick、方向、速度和位移约束

**目标。** 支持具有明确方向和连续量约束的动作。

**范围。** press/release 或 press/motion 的动作窗口、方向向量/角度、最小位移、速度区间、
方向容差、一次性量化、设备能力声明和输入丢样本处理。

**固定边界。** 所有几何和速度计算在 prepare 声明的整数域完成；不使用平台浮点容差、墙钟或
未声明的设备采样率；不能把 Flick 失败降级成 Tap 命中。

**必须验证。** 方向正交/反向/边界、零位移、过快/过慢、press-release 顺序、采样稀疏、设备
最低上报率、跨坐标系、量程溢出、重复事件和 replay identity；独立参考谓词与 runtime 差分。

**不包含。** 3D 方向斩、连续音高、自由体感控制；这些超出当前 L2 范围，需另立 ADR。

### S7B-3：多接触点、和弦、资源占用和跨 requirement 聚合

**目标。** 支持多指/多通道输入，同时保留 requirement 间仲裁的确定性。

**范围。** 接触点 identity、生命周期、并行 requirement、chord 聚合、fanout、claimKey、槽位
复用、observe/consume 可见性、同接触点约束和有限跨 requirement fold。

**设计边界。** 多指 Slide 默认是多个独立 requirement 加 L3 聚合，不恢复 Pattern 积；跨 requirement
共享/交替等约束必须作为显式 Ruleset/aggregate capability，不能隐藏在 Pattern 或 claim side effect。
跨实体约束若无法在现有 L2/L3 表达，先登记 W 类缺口并稳定拒绝。

**必须验证。** 同锚点 fanout 1/all、交错接触、接触点复用前后、未终止归属阻挡、observe 同时
可见、同 Tick 多手、输入顺序置换、聚合部分成功/失败、快照中接触归属和设备上限预算。

### S7B-4：复杂 Hold、尾判、计数和连续动作族

**目标。** 在不改写 7A Hold 的前提下增加明确分阶段或有界重复语义。

**候选子能力。** Hold tail/release、Lift、Roll/连打、有限计数、Spinner/累积量、分段 Slider
tick。每个子能力单独编号和 capability，不把它们捆成一次大版本。

**共同门槛。** phase/category 独立 Fact；计量和 grading 可分别定义；累积型 measure 必须声明
采样周期敏感性，端点型 measure 必须给出一个周期的最坏延迟；计数、循环和状态数有 checked
预算；超出范围稳定拒绝。

**不自动承诺。** 研究案例可表达不等于全部进入 v5；复杂规则若需要跨 requirement 约束、3D
几何或连续音高，归入 W 类缺口或超出范围。

### S7C-1：校准、设备延迟和 Judgement Policy

**目标。** 将 timing offset、input latency、窗口等级和设备能力变成显式、可复现的策略。

**范围。** `JudgementConfig`、`CalibrationProfile`、输入/输出 offset 的单位和符号、应用时点、
设备最低上报率、等级窗口、Life/Accuracy policy 和 capability 协商。

**边界。** 校准不得读隐式历史或宿主私有类型；Chart offset、输入 offset、输出估算不能重复
叠加；profile 变更必须通过 prepare/session transaction；设备不满足能力时拒绝或按显式策略降级。

**验证。** 相同 profile 跨机器一致、正负 offset、seek/reload、profile 迁移、热插拔/设备变化、
窗口边界、identity 变化和 Replay 重放；未经批准不把设备序列号或硬件私有信息写入 Chart identity。

### S7C-2：Ruleset package、模块扩展和 Replay 演进

**目标。** 允许内置和分发 Ruleset 复用同一 Interface，同时保持不可信包的静态安全边界。

**范围。** `cuexis.ruleset` package manifest、interface projection、module/fold 顺序、Hook owner、
`programPolicy`、`outcomeScope`、package identity、签名/完整性（如另行批准）和 Replay revision。

**边界。** Ruleset 包复用 CXC v1 ZIP32 Stored 载体形状但不伪装成 Chart entry；不得包含任意
脚本、宿主 API、IO、随机数、递归/无限循环或动态 Requirement；模块冲突在 prepare 拒绝。

**验证。** 包字节/清单置换、未知模块/outcome/hook、独占 owner 冲突、可交换聚合、程序策略、
包升级和旧 Replay 兼容/拒绝；恶意/超预算包的时间、内存和 decoded bytes 限制。

### 6.1 Stage 7B+ 小目标核验台账

7B+ 的每一行都可以单独立项，但不能跳过 S7B-0 的 capability 和拒绝框架。若一个能力同时需要
多个小目标，只有表中所有依赖行退出后，能力才可进入 Stage 8 的 `included` 候选。

| ID | 小目标 | 必要产物 | 核验标准 |
| --- | --- | --- | --- |
| S7B-0.1 | capability registry | 稳定 ID/revision、RequirementKind 表和查询 API | 已知/未知/编译但未启用三种状态可区分；查询为只读且不改变会话；ID/revision 进入 capability identity |
| S7B-0.2 | prepare preflight | capability、预算、依赖和拒绝检查器 | 缺 capability、错误 revision、依赖未满足、预算超限和非法组合在 prepare 失败；无半准备状态 |
| S7B-0.3 | 扩展版本策略 | semantic revision、Replay header 和迁移/拒绝表 | additive 与 breaking 变化分类明确；旧 Replay/旧 v5 在新引擎上的接受或拒绝稳定；禁止静默降级 |
| S7B-0.4 | extension fixture harness | reference predicate、差分 runner、固定种子和报告格式 | 短轨迹穷举、边界负例、长轨迹固定种子、跨编译器逐行对照可重复；研究模型与产品 binary 分开 |
| S7B-1.1 | 连续量规范格点 | 连续 position/amount event fixture | 同设备类别在最低上报率以上格点相同；插值误差、相位和最大延迟有声明上界；低速设备稳定拒绝 |
| S7B-1.2 | 区域/轨迹判定域 | 2D region、corridor、segment 的 typed domain | 边界含义、局部坐标、半宽/长度量程和转换 identity 冻结；平方/乘加不溢出，超量程明确拒绝 |
| S7B-1.3 | handoff Gap capability | prepared grace、Gap state、timer 和 snapshot 增量 | C10-C14 全部通过；严格 `<`、零 grace、一次 Break、普通仲裁恢复和 Hook 不追溯修改均可复现 |
| S7B-1.4 | Slider 段进度 | segment index、progress、head/body/tail Fact | 断开恢复后段号不回退、head 只产生一次、尾 deadline 使用 `max(window.maxLate, grace)`；新头部竞争按 claimKey 决定 |
| S7B-1.5 | 连续能力预算 | activity/Gap/sample/geometry profile | 真实和压力内容分别测量 activity peak、Gap timer、sample count、snapshot bytes 和 Seek；研究 96 条切片不直接冻结限额 |
| S7B-1.6 | Slide release/拒绝 | 支持/不支持的 Slide 表和 Packed/CXC 决策 | 缺轨迹、错误接触点、段顺序、超密度和未声明采样策略稳定拒绝；不降级成 Hold/Tap |
| S7B-2.1 | 方向量化 | direction vector/angle typed projection | 正交、反向、零向量和边界方向得到规范整数结果；平台浮点库和未声明容差不参与决策 |
| S7B-2.2 | Flick 生命周期 | press→motion→release 状态和窗口 | 正确顺序、早/晚动作、重复 release、无 press、窗口边界和超窗口均有稳定 Fact/diagnostic |
| S7B-2.3 | 位移/速度约束 | min/max displacement、speed/amount measure | 量程、单位、除零、过快/过慢和累计误差有 checked arithmetic；同一规范事件跨工具链结果一致 |
| S7B-2.4 | 稀疏输入和丢样本 | sparse/duplicate/drop fixture | 声明最低上报率以下拒绝或按批准策略处理；不能把缺样本假设成静止或成功 Flick |
| S7B-2.5 | Flick replay/identity | identity、Replay 和 migration golden | 改变方向表、量化、窗口或采样策略只影响规定 identity 分量；旧 Replay 不被静默解释为 Tap |
| S7B-2.6 | Flick capability 选入 | Stage 8 matrix entry 或 deferred record | 编码、预算、拒绝、headless、external consumer 和回滚证据齐全后才能标记 included/candidate |
| S7B-3.1 | 接触点生命周期 | contact begin/move/end、source identity fixture | 接触点唯一性、重用、越界、同时结束和 device limit 有明确状态；顺序置换不改变已声明语义 |
| S7B-3.2 | 多点 claim 仲裁 | claimKey/fanout/observe/slot fixture | 同锚点、交错输入、未终止归属、已终止复用和 observe 可见性与 Ruleset 一致；每事件消费数符合 fanout |
| S7B-3.3 | chord 聚合 | 多 requirement + L3 aggregate capability | 部分成功、超时、成员顺序置换、重复成员和缺成员均有稳定结果；聚合不回写 L2 Requirement |
| S7B-3.4 | 跨 requirement 约束审计 | W-class gap record 或显式 aggregate contract | 同指/交替/共享资源等无法表达的关系不得藏入 Pattern；无法安全表达时生成稳定 unsupported 诊断 |
| S7B-3.5 | 多点 snapshot/replay | contact ownership 和 aggregate snapshot | 任意 contact/Gap/aggregate 恢复后尾部输入逐位一致；Replay 不依赖设备私有 pointer 或线程顺序 |
| S7B-3.6 | 多点容量 profile | max contacts、activity、Fact 和 snapshot measurements | 设备上限、最坏候选数、聚合状态和 snapshot bytes 分别计量；超限保留旧状态并稳定失败 |
| S7B-4.1 | Hold tail/release/Lift | phase-specific requirement 和 grading | head/body/tail 或 Lift 的时间/电平边界独立计量；7A Hold golden 不改变；未声明尾判明确拒绝 |
| S7B-4.2 | Roll/有限计数 | bounded repeat/count register | count 上限、重复输入、超计数、窗口和饱和均有边界；循环有 checked state/event budget，不允许无限 repeat |
| S7B-4.3 | Spinner/累积 measure | sample-period declaration、accumulator 和 grading | 累积型采样敏感性进入 capability/identity；周期变化结果可解释；溢出和过量采样稳定拒绝 |
| S7B-4.4 | Slider tick/分段事件 | finite tick anchors、coverage 和 arbitration | tick 数量、段间隙、重复命中、缺段、同接触点和段顺序有 golden；不生成未声明的动态 requirement |
| S7B-4.5 | 复杂动作族独立选入 | 每个子能力独立 ID、Spec、matrix entry | 每个子能力单独有 Replay、budget、migration、拒绝和 owner decision；禁止把“动作族全部完成”作为模糊结论 |
| S7C-1.1 | CalibrationProfile 数据 | profile schema、单位/符号/范围和 identity 表 | 正负 offset、零值、越界、未知版本和迁移稳定；Chart/input/output offset 不重复叠加 |
| S7C-1.2 | 应用时点和事务 | prepare/session apply 状态矩阵 | profile 只在批准时点生效；活动会话不回读文件；失败保留旧 effective profile 和旧 identity |
| S7C-1.3 | 窗口/Accuracy policy | JudgementConfig、grade table 和 life policy | 窗口严格边界、等级映射、Life/Accuracy 累计和 reset/seek/replay 一致；表 ID 只增不改 |
| S7C-1.4 | 设备能力协商 | capability negotiation 和 fallback policy | 不满足最低上报率、触摸/压力/方向能力或设备变化时按显式拒绝/降级；不写入未经批准的硬件私有 identity |
| S7C-1.5 | 校准 Replay/迁移 | profile identity、Replay fixture、旧版本矩阵 | 同 profile 跨机器相同；profile 变化造成预期 identity 变化；旧 Replay 兼容性有明确接受或拒绝 |
| S7C-2.1 | Ruleset package manifest | `cuexis.ruleset` manifest、interface projection 和 hash | package 字节/清单顺序规范化；缺字段、未知字段、重复 module 和 hash mismatch 在 prepare 失败 |
| S7C-2.2 | Module/fold ownership | module order、Hook owner、combine operator 表 | 独占写目标只有一个 owner；派生值使用可交换/可结合算子；冲突不能靠实例枚举顺序决定 |
| S7C-2.3 | programPolicy/outcomeScope | locked/extendable/open 和 declared/extended fixture | 非法组合、未声明 outcome、未声明 Hook 和 open 之外的扩展稳定拒绝；结果集合进入 ruleset identity |
| S7C-2.4 | Ruleset package sandbox | budget、decoded bytes、termination 和 no-IO checks | 不可信包不能执行 IO、随机、宿主回调、无限循环、递归或动态 Requirement；超时/超内存保留旧 session |
| S7C-2.5 | Ruleset replay evolution | package identity、Replay revision 和 migration | 包顺序、模块、Hook 或 fold 语义变化影响预期 identity；旧 Replay 不因“模块名称相同”而误接收 |
| S7C-2.6 | package/candidate publication | CXC v1 Stored entry、entry kind 和 production/candidate matrix | Ruleset entry 不伪装成 Chart entry；source/compiled/playback 分层清晰；默认 Writer 不发布未接受 package |

## 7. 7B+ 与 Stage 8 的选入规则

Stage 8 只在其发行矩阵中明确选入某项 7B+ 时消费该能力。选入必须同时满足：

- Stage 7B+ capability 和 typed prepare 合同已接受；
- Chart v5/CXT v2/Packed/CXC 有明确编码、identity 和 resource closure；
- 默认 Writer、candidate Writer、Playback preflight 和 Player 入口的支持/拒绝一致；
- headless golden、Replay、snapshot/seek、budget、cross-toolchain 和 external consumer 证据齐全；
- 迁移、旧版本拒绝和回滚路径可执行；
- owner 在 Stage 8 发行矩阵中明确接受。

未选入的能力仍可在 Stage 8 前开发，但只能通过显式 candidate/capability 入口使用；Stage 8
默认生产入口必须稳定拒绝，不能把它当作 Tap/Hold 或 v4 兼容语义。Stage 8 关闭后，7B+ 可以
继续增加兼容 capability；不得以修改已发行 v5 字段含义的方式追补。

## 验收标准

Stage 7 的验收由 Stage 7A 硬前置和每个 7B+ capability 的独立退出标准组成。任何研究性 spike、
单平台结果、只测正例的 golden 或历史报告都不能替代本计划要求的实现和验证证据。

## 8. 统一验收矩阵和证据要求

每个批次至少维护以下列，不以“测试通过”一句话替代：

| 维度 | 必须记录 |
| --- | --- |
| 合同 | Spec/ADR/ABI 版本、字段和 capability/revision |
| 正例 | 最小、边界、组合、真实内容和压力 fixture |
| 负例 | 未知值、越界、溢出、冲突、预算、身份、截断和非终止 |
| 确定性 | 输入/实例/容器顺序、跨编译器/平台、seek/replay 对照 |
| 生命周期 | prepare、submit、advance、pause、seek、reload、reset、销毁和失败事务 |
| 身份 | engine/ruleset/chart/session、content/prepared/replay identity 及变更原因 |
| 预算 | prepare 峰值、稳态 tick、activity、Fact、Replay、快照、Seek 和 decoded bytes |
| 边界 | headless、static/shared、external consumer、candidate/production、GPU/设备限制 |
| 证据 | 命令、SHA、配置、环境、原始日志/机器可读结果、报告路径和未执行项 |

必须区分：本地验证与 hosted 验证、离线/UI 验证与真实设备/GPU 验证、配置阈值与实测结果、
研究模型与产品实现。任意不可用工具、连接失败或缺少设备只降低证据覆盖，不能推断通过。

## 9. 回滚、阻塞和停止条件

1. 合同变更：停止受影响实现，保留最小复现，回滚到最后接受的 ADR/Spec revision，重新估算
   identity、Replay、Packed 和 Stage 8 影响后再继续。
2. 确定性失败：任何跨编译器/平台差异、未声明浮点、整数溢出或容器顺序依赖都阻塞批次；不以
   固定编译器、排序偶然性或缩小输入规避。
3. 预算失败：区分内容预算、运行稳态、快照/Seek、Replay/解码和 Packed wire；只允许调整已
   接受的 profile，不得事后缩小 fixture 或把隐藏事件排除计数。
4. 安全失败：不可信 Chart/Ruleset/Replay 导致未界定循环、IO、宿主回调、动态生成或过量分配时，
   立即稳定拒绝并保留旧 active session。
5. 集成失败：candidate、v4 fallback、旧 Replay 或无输入 Playback 受影响时，回滚新增入口，
   不删除旧路径，不把 Player smoke 分支当成集成完成。
6. owner 未接受 ADR/Spec/例外时，状态保持 planned/active/blocked；不得用文档措辞把候选能力
   写成 implemented 或 production。

## 10. 交接物和后续阶段关系

### 交给 Stage 8

```text
Stage 7A typed Input / Requirement / Judgement / Score / Replay contract
Tap/Hold/Release and prepared-grace semantics
InputMapping and four-part judgement identity
Headless golden, replay fixtures, snapshot/seek guarantees
v4 fallback and v5 candidate consumption map
candidate/production isolation and offline assembler evidence
capability/rejection table for all 7B+ items
selected 7B+ release matrix, if any
measured gameplay budgets and unresolved W-class gaps
```

Stage 8 不得重新发明输入、判定、Replay 或 Score 模型；只把已选 capability 编入 Chart v5/CXT
v2/Packed/CXC 正式发行，并保留未选能力的拒绝路径。

### 交给 Stage 9/10/12/14

- Stage 9 只消费稳定的 Judgement/Presentation 观察面；模型、环境和几何表现不得改变判定事实。
- Stage 10 只消费已冻结的 typed authoring/prepare/capability 合同；Studio 不能成为新的判定实现。
- Stage 12 消费真实内容的 gameplay、Replay、Seek 和设备 profile 测量，不修改单一设备的判定语义。
- Stage 14 再决定稳定 C ABI；Stage 7 的 C++ typed preview 不等于稳定 ABI。

## 11. 明确不包含

Stage 7A 不包含完整 Slide、复杂 Flick、多指、旋转、3D 方向斩、连续音高、Studio 编辑器、
运行时脚本、通用状态机、宿主回调、稳定 C ABI 或 Chart v5 默认 Writer。

Stage 7B+ 仍不包含任意 Script VM、逐帧脚本 callback、宿主字节码、无限 Pattern、运行时随机、
IO、动态对象/Requirement 生成或未版本化的插件执行权。超出当前 2D/离散和声明式模型的能力必须
另立 ADR、威胁模型、预算、ABI 和阶段计划。
