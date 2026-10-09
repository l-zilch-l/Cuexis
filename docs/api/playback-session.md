# `PlaybackSession` 生命周期

状态：active

更新日期：2026-08-30

适用版本：SDK API `0.7.1`

文档角色：公共 API 参考

权威头文件：[playback_session.hpp](../../engine/playback/include/cuexis/playback/playback_session.hpp)

## 快速结论

| 项目 | 合同 |
| --- | --- |
| 主入口 | `cuexis::playback::PlaybackSession` |
| 内容输入 | `PlaybackSource` 或 Chart JSON 文本 |
| 每帧输入 | `RuntimeFrame` |
| 每帧输出 | `FrameSnapshot` |
| 推荐加载方式 | `prepareLoad` -> 检查候选 -> `commit` |
| 线程要求 | Session 与非空 `PreparedPlayback` 必须留在创建线程 |

## 标准流程

1. 创建 `PlaybackSession`。默认构造使用默认 capability；传入 `PlaybackCapabilitySet` 可建立裁剪 Session。
2. 构造 `PlaybackSource`，调用 `prepareLoad`。
3. 检查 `PreparedPlayback` 的内容信息、semantic identity 和 presentation manifest。
4. 接受候选结果后调用 `commit`。
5. 每个 tick 调用 `update(RuntimeFrame)`。
6. 需要绘制时调用 `extractFrame(FrameViewport)`。
7. 使用 `prepareReload` / `commit` 替换内容，或调用 `unload` 清空 Session。

## API 速查

| 操作 | API | 说明 |
| --- | --- | --- |
| 查询状态 | `state`、`capabilities` | 返回 `core::Result<T>`。 |
| 两阶段加载 | `prepareLoad`、`commit` | 提交前可检查候选内容。 |
| 两阶段重载 | `prepareReload`、`commit` | 失败不改变 active 内容。 |
| 便利加载 | `load`、`loadChart` | 等价于内部 prepare + commit。 |
| 便利重载 | `reload` | 适合不需要检查候选的调用方。 |
| 推进播放 | `update` | 消费一个 `RuntimeFrame`。 |
| 提取帧 | `extractFrame` | 可返回新 snapshot，也可复用 destination。 |
| 查询内容 | `chartInfo`、`contentInfo`、`semanticIdentity` | 读取 active 内容。 |
| 查询诊断 | `diagnostics`、`lastOperationDiagnostics` | 区分 active 状态与最近操作。 |
| 宿主覆盖 | `acquireHostOverride`、`releaseHostOverride` | 以 token 管理临时属性覆盖。 |

## 状态与事务

| `SessionState` | 含义 |
| --- | --- |
| `Empty` | 没有 active 内容。 |
| `Ready` | 内容已提交，可以开始更新。 |
| `Running` | 已执行播放更新。 |
| `Failed` | Session 本身进入不可继续状态；普通 prepare/reload 失败不应破坏 active 内容。 |

`PreparedPlayback` 只表示候选内容。只有 `commit` 会替换 active 状态。prepare 或 reload 失败时，既有
FrameSnapshot、semantic identity、active diagnostics 和 presentation 必须保持不变。

`ReloadPolicy::KeepChartTime` 保持目标 chart time；`RestartAtZero` 从零开始。参数通过
`PlaybackPrepareOptions::parameters` 在 prepare 时冻结，并参与最终语义结果。

## 每帧输入

| `RuntimeFrame` 字段 | 含义 |
| --- | --- |
| `chartTimeMs` | 当前 chart-local 时间。 |
| `simulationDeltaTimeMs` | 本帧模拟增量。 |
| `timeDiscontinuityId` | seek、stop 或时钟跳变后的不连续标识。 |

## S7A-7/8 candidate 组合事务 revision 1（2026-10-07）

2026-10-09实时架构重审的[宿主边界草案](realtime-host-boundary.md)尚未冻结/实施，不改变下面的
owner-thread和组合提交语义。未来若分开判定推进、表现采样或renderer准备，须先完成对应typed
合同与兼容证明；不能把本页的同步Prepared事务直接解释为多线程事务。

本节按本轮 owner 实施授权落定 C78-01–04/08/09、R78-01–05/10b 的消费合同；
它不表示实现验收、生产预算接受、owner API approval 或阶段关闭。
未启用 Gameplay 的原有方法及 SessionState 含义保持。显式 Gameplay 在 commit 后即可推进
timer；零输入到期产生 Miss，不能等首次输入激活。准备候选不产生 Fact。

组合推进次序为 owner/reentry/state、完整 RuntimeFrame 有限性与非负 delta、
typed horizon/discontinuity/输入批次预检、输入准入、kernel seal、Fold、可发布投影、Runtime。
预检拒绝保持旧状态；成功准入的 future/零 Fact batch 是变更。seal 后的错误不回滚合法前缀。
返回回执必须区分 admitted、kernel/Fold 发布边界、Runtime 是否更新及表现诊断，
并保留 requested H、processed frontier、lastAdvanceHorizon 和 failedTick。
相同 batch 不能在准入成功后重试；调用者查询回执再选 query/reset/显式替换。

| 操作 | mutation generation / Prepared | Gameplay / projection |
| --- | --- | --- |
| prepare、query、archive、snapshot、预检拒绝、空 submit | 不变；候选仍有效 | 只读；faulted snapshot 按 J0 拒绝 |
| 非空 submit 成功（包括全部 future） | 增加；旧候选 stale | pending/lastObservedTick 按既有 kernel |
| advance 成功或开始执行后 fault | 增加；旧候选 stale | sealed 前缀保留；失败 Tick 不发布 token |
| update、override、Pause/Resume 成功变更 | 增加；旧候选 stale | 不推断 Seek，不伪造 input/Fact |
| commit、Seek/restore/reset、Stop、新内容替换、unload | 增加；旧候选 stale | 整体替换 transient projection scope |
| 拒绝的 control、重复的 Pause/Resume | 不变 | 旧组合保持 |

generation 是事务失效标记，不进入 JudgementIdentity。projection scope 是当前 Ledger 的临时
作用域，不随普通 submit 增加。Seek/restore/分支替换须暂存并整体 swap cursor、dedup、future
queue、aggregation、target bindings 和 tokens；同长度或 same-H 不是保留旧游标的理由。
load/commit/恢复候选失败保持旧 Frame、Fact、Fold、archive 与 projection。

Pause 禁止 submit/advance，保持已发布画面和真实 held contact；Resume 不补 release。
Stop/reset 创建同配置空 kernel 并清空 transient 状态，不生成 Fact；Stop 进入 Paused，
Reset 进入 Prepared。Stop 后只有 Resume 才继续推进；faulted Stop 拒绝。Pause 同时拒绝
普通 update，保持画面。重建保留原 Host lifetime，不消耗 RemainingFrames。Seek 必须显式指定 archive、
cut/H，按 F(H) 调用现有恢复路径；不从 double chartTimeMs 回退推断。
faulted 只允许 owning query/archive、reset/unload 或完整新会话替换；就地 Seek/snapshot 拒绝。
换内容不能沿用旧评分。仅换表现保持四分量，但重取当前有效 binding/resource closure 后重建；
坏必需资源原子拒绝，不能解释为 GameplayOnly。

Gameplay 状态独立查询为 Inactive/Prepared/Running/Paused/Faulted；faultStage 独立映射
Kernel/Fold/Control。它们不按内部 enum 数值强转，不等同 SessionState::Failed。
owning 查询与 ReplayEvaluation 保留完整结果，不安装内部 KernelProjection/Recovery 类型图；
空对象、移动后对象必须可析构且访问稳定拒绝，跨 static/shared 的释放由 owning API 承担。

GameplayOnly 仅省略可选表现投影，仍验证完整 CXC/project/hash 与判定闭包。
纯表现 target 缺失是空绑定及表现诊断，不能 fault Judgement 或修改 Fold。

## 失败与边界

- 所有 `core::Result<T>` 必须显式处理。
- 公共调用不能让异常跨越模块边界。
- 非空 `PreparedPlayback` 不得跨线程移动或离开创建线程析构。
- Session 不暴露 RuntimeSession、World、entity 或 renderer backend。
- 运行时脚本和逐帧 script callback 无限期延后，不存在 Playback hook。

相关内容：[输入来源](sources-and-content.md)、[帧输出](frames-digests-and-timelines.md)、
[诊断规则](diagnostics-identity-and-compatibility.md)。


## Candidate typed Packed source 首用（2026-10-07）

ON flavor 的 `PlaybackSource::fromGameplayPacked` 接受 owning sourceId、精确 entryPath、
完整 revision 3 Capsule bytes、显式 GameplayConfiguration、typed assets 与 owning provider。
它是显式 Gameplay 激活入口，默认 fromChartText/fromTypedProject 的 PlaybackOnly 行为不变。
入口先验证 portable ID/path、provider、assets，再按既有 Capsule 物理预算一次解码，保存
完整 CanonicalSemanticChart、Requirement owner、PreparedGameplay 与 immutable 配置。
prepare 直接消费该 owning typed carrier，不重读 bytes，不运行 author/CXT compiler。
空路径、无 provider、资产重复、错误 revision/profile/identity/闭包均沿既有稳定码拒绝。
表现 lowering 使用独立的、已由 Capsule 验证的 static presentation 图；不会把 Gameplay
requirements 塞进 Foundation tap lowering，也不会丢弃判定图。资源完整校验先于候选发布。
prepared 同时含 Runtime 与真实 kernel；commit 后零输入也会到期产生 Miss。
此 typed entry 的成功不证明 filesystem/CXC manifest 或 Graph JSON 入口完成。


## Candidate GameplayOnly 意图（2026-10-07）

Typed Packed factory 的 GameplayPrepareIntent 明示 Presentation 或 GameplayOnly；旧工厂
默认 PlaybackOnly，不能因表现实例化失败自动降级。GameplayOnly 仍解码并验证完整 Capsule、
resource closure、typed Asset index/provider、CPU资源与 exact content identity；省略 World、
表现求值和 FrameSnapshot 实例。已声明的必需资源错误照常拒绝，optional target 缺席不改判定。
其 prepare/commit/submit/advance/query/Replay/Snapshot/Seek 与 Presentation 模式使用相同
实际 kernel/Fold。H/T/RuntimeFrame 仍显式预检；RuntimeUpdated=false 明示没有 Runtime
图，而不是错误。Frame/update/HostOverride 沿已有无 World 稳定拒绝。该意图不增加数值预算。

## Candidate 安装 consumer 的首用边界

实验安装树只在显式 candidate ON flavor 安装 `gameplay_candidate.hpp` 并通过
`Cuexis::Playback` 传递 `CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE=1`；新方法不在 production OFF 的
公共声明/符号表面出现。旧 Entry OFF 稳定拒绝例外保留。
内部 KernelProjection / Recovery / Judgement execution headers 不安装，static 仅携带必要 link-only
实现 archive，shared 由匹配工具链的 Playback library 保有执行对象及跨模块释放。
独立 consumer 仅链接 `Cuexis::Playback`，通过公共 owning 值与 bytes 运行生命周期。
fixture generator 属测试，不能代替 author→Graph/Packed→Entry→生产 assembler 的交付证据。

## Mutation credential 的整数边界

Prepared 凭据是 owner token 与 mutation generation 的二元组；generation 为 u64，零不发布。
计数 wrap 时使用既有唯一 owner-token 分配器刷新 owner token，再从 generation=1 开始，
因此旧 Prepared 不会因为计数回到相同值而重新有效。此刷新不创建新判定分支、Fact 或 projection scope，
不进入任何 judgement identity；旧凭据仍经既有 wrong_session / stale 门禁拒绝。

## C78-02 / R78-01 发布失败阶段（2026-10-07 revision 1）

GameplayAdvanceReceipt 的 publicationFailureStage 是独立可选枚举 Result / Projection / Runtime，
只描述已准入 advance 后 owning 结果构造、表现规划或 Runtime 事务的失败位置；
不是 kernel 的 faultStage，不把 Running 改成 Faulted。成功时 absent。
Runtime 失败保持旧画面、projection cursor/queue/token 和表现 Tick，已成功 kernel/Fold 的
结果及 archive 保留。修复表现条件后可按当前 H 再推进发布；不得重新 submit 已准入输入。
回执 runtimeUpdated 为 false，error 保留实际模块诊断；只读 query 不能补发画面。
Result/Projection 的分配失败同样不能将已准入推进误报为预检拒绝。
本字段的追加仅在显式 ON candidate 头中，旧 Playback 状态和 faultStage 不扩展。


## S7A-7 Graph source 首用（2026-10-07）

Candidate ON 的 GameplayContent::fromGraph 与 PlaybackSource::fromGameplayGraph 明确消费
Compiled Graph v1；后者携带 owning byte vector、显式 sourceId/entryPath、GameplayConfiguration、
GameplayGraphDecodeBudget、typed assets/provider 与 prepare intent。
GameplayGraphDecodeBudget 为七项无默认 size_t：maxBytes/maxDepth/maxStringBytes/maxValues/
maxContainerElements/maxRowAtoms/maxRowDecodedStringBytes；configuration.testOnly 是唯一候选预算准入开关。
这些不是生产阈值。公共边界不暴露 json_support 或 Packed/Kernel 类型。
同一原始字节视图仅经 typed SAX 解码一次；Graph 不再编码成 Packed 后重读，不 fallback 到 author/JSON source。
Graph 与 Packed 共享实际 content/configure/prepare 与生命周期，完整 owning Capsule/Prepared 留在内部。
首用诊断沿 Graph 格式的中央登记及既有 prepare 公共投影，失败不发布部分 source 或改变旧 session。

## S7A-7/8 Gameplay Entry source 工厂（2026-10-08）

candidate ON 的 `fromGameplayEntry` 接收 owning metadata/entry bytes、显式 configuration 与
metadata/Graph decode budgets、typed assets/provider 和 prepare intent；一次选择、一次载荷解码，
metadata 与重建的七项闭包完整比较后才返回 source。
`fromCxcFileGameplayEntry` / `fromCxcMemoryGameplayEntry` 接收既有 CXC locator/owning bytes、
明确 entryPath、configuration、两个 budget 与 intent。先沿原 CXC v1 loader 检查整个 package；
从 project optional `cuexis.gameplay-entry.v1` extension 的 entries 目录选择单个 metadata，要求 path 精确相等；
assets 来自 package 的真实 Asset Index，provider 持有完整 owning package。
`fromFilesystemGameplayEntry` 使用固定 project locator 与同一 metadata extension，
metadata/path/assets 校验一致；调用者必须传已捕获 generation 内的 project locator，
不能让该工厂逐文件重新选择 adopted。所有工厂无编译/缺省配置/旧 Foundation fallback。
错误保持当前 source/active session；返回 source 不等于 commit，资源消费仍经过实际 prepare。

## S7A-7/8 Reference Host 显式 Gameplay 首用（2026-10-08）

安装树 Reference Host 继续复用原 open/play/pause/seek/reload/quit 加 tick 命令循环。
candidate ON 以 --candidate-entry、--gameplay-configuration、--gameplay-config-budget（五个正整数）、
--gameplay-h-step 和 --gameplay-presentation-step（正 i64）显式准入；可选
--gameplay-observations 为宿主捕获的离散输入记录。只有 command mode 支持该组合。
H/T 是独立整数 Tick 计数，step 单位为所选 candidate profile 的 Tick；RuntimeFrame 仍沿原整数
宿主毫秒时钟转换成表现帧，绝不从其 f64 反推 H 或 observationTick，不声称生产时钟校准。
该 fixture bridge 仅验证已记录输入的公共路径，不代替 Player 真实设备时间捕获。
seek 的整数 target 为 H，T 同步重建到该显式 target，Runtime 时间由原命令时钟单独更新。
Graph budget 用同一显式 config budget 的 bytes/depth/string/values/elements，maxRowAtoms=elements、
maxRowDecodedStringBytes=string；它只是 caller 指定的 testOnly budget，不扩大物理界。

configuration 与 observations 全文件在任何SDK load前解析。输入行固定13列：observationTick:i64、
sequence:u64、press/release、channel、domain、sourceClass、device:i64、hostArrival:i64、audioFrame:i64、
renderFrame:i64、crossedSamplingGap:0/1、reconnected:0/1、droppedSamples:0/1；amount显式absent，
无第二normalize/评分。文本沿原command file bytes/records物理界，禁止尾列/非法整数与非递增sequence。
在新会话首个有效tick一次提交所有 owning observations，future pending由原kernel持有；暂停不提交。
open调用新Gameplay factory并actual prepare/commit；play/pause调用Gameplay控制；每tick实际advance/query
并保留完整ReplayEvaluation比较；Seek复用公开archive/cut/exact-H恢复；reload保持原Runtime时间但建立
新Gameplay session，H归零、输入提交标记重置、T继续当前表现位置，依transport显式Pause/Resume；
quit原unload。CXC locator及捕获generation内的filesystem project均使用公共SDK factory，无内部执行图。
所有SDK拒绝保留原code；load/prepare/commit/recovery失败不替换active；frame输出沿原manifest/digest路径。


### 2026-10-08 Player 首用合同补充

candidate Player Gameplay 显式要求配置文件、五项配置解码预算、整数 H/T 每帧步长和
重复 `--gameplay-key scancode:channel:domain` 映射。此 testOnly 离散采样桥是受限功能验收输入，
不承诺生产时钟校准、音频设备或帧率无关节奏。2026-10-09 多转换修订：在无控制变化的实际
Playing 帧中，按 SDL poll 保留的真实转换顺序，为每个映射转换捕获一个独立工作 Tick；
第 i 个转换的 observationTick = 帧前 H + i × 显式 hStep（i 从1开始）。未映射键不占 Tick。
无映射转换时仍占一个工作 Tick；帧末 H = 帧前 H + max(1,映射转换数) × hStep，
T 始终只按显式 tStep 每帧推进一次。乘加均 checked；溢出先于 submit，整帧无准入。
全部 owning observations 一次原子 submit，再沿同一实际 kernel/Fold 按 F(H) advance 到最终 H。
不丢转换、不保留跨帧待提交队列、不改内核一个 Tick 至多一个输入的规则；此为显式 testOnly
采样时钟，不宣称同帧 chord 同时性或生产设备校准。SDL 时间戳仅为原始 provenance。不得从 RuntimeFrame
的 f64 chartTimeMs、设备或音频时间戳反推 Tick。无输入帧仍推进实际 kernel/Fold 并产生到期 Miss。

SDL 忽略自动重复，保留每个真实 scancode 的 press/release；显式映射键不兼任传输控制。
focus lost 先暂停并丢弃该 poll 批次，不制造 release 或重新连接 hit。暂停期间不提交输入，恢复只
接收新的真实离散转换。应用传输边界不是输入 trajectory discontinuity：新转换不得因 Load、
Pause/Resume、Stop/reload 或 Seek 被自动标 `crossedSamplingGap=true`。SDK 显式传入的跨 gap /
重连 / 丢样声明仍按 ABI 返回 `input.continuous_unsupported`，不能清除这些声明来绕过拒绝。
SDL adapter 只捕获新 press/release，不声明连续重建；控制帧旧批次隔离规则不变。Playback Pause/Resume/Stop 为公共
Gameplay control；内容替换的新 session 重置输入序号和 H，保留或重置 T 按 reload 的 Runtime
策略。typed seek 通过公共 Replay/cut/seek 重建，在应用层拒绝把旧 f64 Seek 隐式用于 Gameplay。
Player 输出 score/combo/hit/miss 和完整实时/Replay 结果比较；该 Replay 比较是路径一致性检查，
独立 oracle 和人工 golden 仍由独立验收 fixture 提供。

2026-10-09 可选 `--gameplay-guide` 为 Player 私有 testOnly 练习显示，不是新的 Chart/CXC 字段、
SDK API 或判定入口。启用时窗口从 Ready 开始，Space 显式启动；四轨音符的位置只由当前
整数 H 与 guide 的 typed target Tick 派生，不反向驱动 Gameplay。Hit/Miss 只能读取实际
FrameSnapshot 中对应 FactBinding outcome 标记的 visibility，分数读取公共 query，不能按按键
或目标到线自行声称命中。Pause/Seek/reload/Stop 从当前实际 snapshot/H 重取画面，无独立结果缓存。
guide v1 是 ASCII token 文本：首行 `cuexis-player-guide-v1`，随后 exact unsigned count；每行
`tap|hold D|F|J|K headTick tailTick headHit headMiss bodyHit bodyMiss tailHit tailMiss`。
Tick 为 exact非负 i64；Tap tailTick=0、后四个对象标记为`-`，Hold tailTick>headTick且六个标记必需。
对象标记是该 fixture 的 explicit objectId，必须存在于实际 snapshot；记录 key 必须有对应映射。
guide 字节/记录数复用显式配置 budget 的 maxBytes/maxContainerElements；非法 count/字段/尾列/映射
在启动拒绝，不接受新生产限额。guide 必须与生成它的包一起使用；它不拥有任何 judgement identity。
默认无 guide 的 Playback/Preview/Player 行为保留；ON/OFF 准入沿既有 candidate 开关。


Player 窗口适配按同一 poll 批次做保守隔离：focusLost 帧抑制除 Quit 外全部控制动作和判定输入；
任何控制动作帧丢弃该批判定按键，只在原本 Playing 且没有控制变化的帧提交。聚合窗口队列不
声称保留按键与传输动作的交错顺序。左右 seek 键对 Gameplay 使用当前整数 H 加减一个显式
H step，并以同一公共 Replay/cut 重建；旧 f64 Seek 命令稳定拒绝。音频 control 拒绝时不变更
Gameplay；音频已成功而 Gameplay control 拒绝时应用进入 Failed，不能报告旧 bundle 保持成功。
