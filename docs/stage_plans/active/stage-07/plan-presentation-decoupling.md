# Stage 7-PD: Judgement/Presentation Decoupling Plan

状态：proposal；独立于 Stage 7A 与 Stage 7-RPA 的 Stage 7 子阶段；2026-10-14；本文件只记录架构重构方案、实施批次和验收门禁，不表示产品代码已经修改、Stage 7A/RPA 已完成或任何发布门禁已关闭。

本子阶段处理判定结果、表现绑定、Runtime/World 和帧提交之间的实现耦合。它承接当前代码审查和候选架构比较，目标是先建立稳定的数据边界和可观测的帧交换，再用 Release 测量决定是否需要进一步拆分 owner。

本计划与下列工作保持独立：

- Stage 7A 负责 Gameplay v2 的 Requirement、Judgement、Ruleset、Score、Replay、Snapshot 和语义验收。
- Stage 7-RPA 负责实时捕获、时钟、控制、设备、资源 fence、回执和综合设备验收。
- Stage 7-PD 不修改 Gameplay v2 的判定语义，不重新选择 RPA 的控制协议，也不把表现重构结果写成 7A 或 RPA 的完成证据。

Stage 7-PD 可以在 7A/RPA 尚未关闭时进行内部实现准备和受控迁移，但正式退出必须消费两者已经接受的语义、上下文和资源身份合同。当前实现状态仍以 [CURRENT_STATUS.md](../../../CURRENT_STATUS.md) 为准。

## 0. RPA 依赖处置

Stage 7-RPA 不能整体舍弃后直接把本计划当作新的替代主线。RPA 已经拥有实时执行、控制、scope/generation、资源 fence、PresentReceipt、关闭和故障恢复的合同；这些边界是 FramePacket 和异步 observation 能够安全工作的前提。删除 RPA 会留下两个不可接受的结果：PD 重新定义一套实时协议，或在没有控制/资源/回执合同的情况下把内部性能重构误当成可部署架构。

可以采用“保留合同、拆分实施顺序”的处理方式：

| RPA 内容 | 对 Stage 7-PD 的处理 | 是否阻塞 PD 开始 |
| --- | --- | --- |
| B0 已闭合的执行、控制、版本和失败合同 | 作为边界输入保留 | 否，已具备 |
| B1/B2 观测、oracle、存储和寿命证据 | 作为基线和测试设施复用 | 否，已有可复用证据 |
| B3 判定执行、时间桥、Fact/Replay/Snapshot 语义 | 保留；PD 不重新定义 | PD-1/PD-2 前需完成所消费接口的定向核对 |
| B4 宿主/资源/fence/回执协议 | 保留；PD-3/PD-4 必须消费 | PD-3/PD-4 的合同退出是必要前提 |
| B5 非实体设备的 testOnly 部分 | 可作为内部验证输入，不能作为生产验收 | 不阻塞 PD 内部实现；阻塞生产部署结论 |
| B5 实体设备、真实听音、压力和同步 | 保留到 RPA 完成 | 不阻塞 PD 代码重构；阻塞最终实时验收 |
| F1-F4 综合收尾、生产预算和 Stage 8 交接 | 保留为 RPA 的最终责任 | 不阻塞 PD 开始；阻塞 Stage 7/RPA 关闭和发行交接 |

因此，本计划允许在 RPA-F1–F4 未退出时启动 `PD-0`、`PD-1`，并在 B3/B4 相关合同完成定向核对后实施 `PD-2`。`PD-3` 和 `PD-4` 只能在不改变 B4 资源、fence、receipt、关闭和故障语义的前提下实施；如果需要改变这些公共或宿主边界，必须先回到 RPA 对应 Spec/ADR 和版本差异记录，而不是在 PD 中绕过。

PD 的成功不替代 RPA 的实体设备、生产预算、F1–F4 或 Stage 7 关闭；RPA 的未完成项也不应被标记为“废弃”，除非建立单独的 owner 接受记录，明确替代合同、保留的失败语义、迁移证据和关闭责任。

## 1. 审查结论与问题边界

### 1.1 已经成立的边界

`cuexis_judgement` 的 CMake 目标只依赖 `cuexis::core`，不依赖 Runtime、World、Render、SDL、OpenGL 或 Presentation Renderer。`IPresentationRenderer` 只接收 `FrameSnapshot`，不接收 Judgement Fact、Ruleset 或 JudgementSession。因此，渲染后端与判定核心之间不存在需要立即拆库或拆线程的直接耦合。

### 1.2 仍然存在的实现耦合

当前 Playback candidate 路径存在以下链路：

```text
Judgement Kernel/Fold
  -> PlaybackSession::publishGameplayProjection
  -> GameplayFactBinding 扫描与聚合
  -> RuntimeSession::updateGameplay
  -> World / EnTT 的 GameplayOverride
  -> PlaybackSession::extractFrame
  -> FrameSnapshot
  -> Renderer::submit/present
  -> FrameObservationLedger / PresentReceipt
```

主要证据位置：

- [playback_session.cpp](../../../../engine/playback/src/playback_session.cpp) 的 `publishGameplayProjection()` 直接读取 `judgement::PhaseOutcomeFact`，扫描绑定，并生成 `RenderVisible` 写入。
- [runtime_session.cpp](../../../../engine/runtime/src/runtime_session.cpp) 的 `updateGameplay()` 将 Gameplay 写入解析为 World 属性层，并与 Runtime 时间推进、候选状态和回滚绑定。
- [playback_session.cpp](../../../../engine/playback/src/playback_session.cpp) 的 `extractFrame()` 直接读取 World、EnTT、Transform、Appearance、Camera 和资源布局。
- [session_worker.cpp](../../../../engine/playback/src/session_worker.cpp) 在同一个 `SessionSample` 中顺序完成 Gameplay/Runtime 更新、帧提取、反馈登记和 latest 帧返回。
- [gameplay_internal.hpp](../../../../engine/playback/src/gameplay_internal.hpp) 直接包含 Judgement 的 `src/diagnostic_policy.hpp`，使 Playback 依赖 Judgement 私有实现。

问题不在 Renderer API，而在 Playback 内部同时承担了 Judgement 结果解释、表现绑定、Runtime 写入和帧生产。该结构造成以下维护风险：

1. 判定 Fact 的内部表示变化会直接影响 Playback 表现代码。
2. 新增表现效果容易继续扩展 World 属性，而不是扩展稳定的效果数据边界。
3. 表现投影、Runtime 求值或 Snapshot 提取变慢时，会同步阻塞判定后的下一帧生产。
4. 判定事实已提交而表现投影失败时，失败和恢复边界需要跨 Judgement、Runtime、Frame Ledger 协调。
5. 判定规则、表现映射和 Renderer 的测试被迫共享较大的 Playback 集成环境。

## 2. 目标与非目标

### 2.1 目标

Stage 7-PD 的目标是建立以下可验证关系：

```text
Judgement committed prefix
  -> immutable PresentationEffectBatch
  -> PresentationProjection
  -> immutable FramePacket
  -> Renderer consumes latest packet
  -> PresentReceipt returns asynchronously
```

具体目标如下：

- Judgement 只发布稳定的、只读的已提交前缀或表现效果，不依赖 World、EnTT、FrameSnapshot 或 Renderer。
- 表现绑定在 prepare 阶段建立索引，运行时只消费新增的 committed prefix，不重新扫描全部 Fact 与绑定。
- Runtime 继续负责 Chart/Behavior、相机、动画、资源和时间轴求值，但接收明确的表现投影数据。
- FramePacket 拥有自己的 `scope`、generation、presentation tick、Fact cursor、资源引用和 FrameSnapshot，不能借用可变 Runtime 状态。
- Renderer 只消费 FramePacket/FrameSnapshot；PresentReceipt 通过异步 observation 回传，不阻塞 Judgement 前进。
- 保留单一 Playback owner、owner-thread 规则、控制 fence、resource generation、projection scope 和现有失败语义。
- 为后续 owner 拆分建立可测量的触发条件，而不是预先冻结固定线程拓扑。

### 2.2 非目标

- 不修改 Gameplay v2 的判定、Fact 因果序、Ruleset、Replay 或 Snapshot 语义。
- 不把 `FrameSnapshot` 与新的公共 `RenderPacket` 并列为第二套公共权威帧格式。
- 不在本子阶段引入任意脚本、动态插件、宿主回调或新的渲染后端。
- 不直接拆出固定的四线程或多 Playback owner 架构。
- 不以一次 Debug FPS 提升替代 Release、跨平台、设备和长时运行验收。

## 3. 候选架构与采用基线

本子阶段保留此前探索过的架构选项，供实施和复核时对照。选项不是已实施状态，也不改变 Stage 7A 或 RPA 的语义合同。

### 3.1 选项一：同步单 owner 链

```text
advance -> projection -> Runtime update -> extractFrame -> present -> receipt
```

优点是状态一致性最简单、回滚路径集中、现有代码复用最高。缺点是判定、表现投影、Runtime 求值和帧生产互相阻塞，Fact/绑定扫描和 Snapshot 提取的成本会直接落到显示节拍上。该选项仅作为行为基线和回退路径，不作为性能重构的目标架构。

### 3.2 选项二：单 owner 分阶段、不可变帧交换

判定提交、表现投影和 FramePacket 构建仍由同一个 Playback owner 顺序执行，但阶段之间只通过不可变数据传递，Renderer 消费 latest packet，PresentReceipt 异步回传。它保留 owner-thread、控制 fence、资源代际和事务回滚，同时切断表现代码对 Judgement Fact 容器和 World 即时状态的直接依赖。

这是本计划的**推荐实施基线**，因为它在维护性、确定性、回滚复杂度和性能收益之间最平衡，也能为后续 owner 拆分提供真实测量数据。

### 3.3 选项三：判定 owner 与表现 owner 双阶段流水线

判定 owner 发布 committed prefix 或 PresentationEffectBatch，表现 owner 独立消费并生成 FramePacket，主线程只负责窗口、设备和 Renderer。优点是判定计算可以与表现求值并行，缺点是需要处理跨 owner 的 scope、generation、资源生命周期、快照一致性、迟到结果、Seek/Reload/Close 和故障恢复。

该选项只有在单 owner 分阶段方案完成后仍无法满足 Release/设备预算时才进入独立设计，不预先冻结线程数量或队列模型。

### 3.4 选项四：完整多阶段流水线

把判定、表现投影、Runtime 求值、资源准备和渲染提交分别放入多个长期运行的流水线阶段。它可能提供最高吞吐，但会引入最大的排队延迟、内存占用、跨阶段背压、控制切换和错误恢复成本，也最容易把实时音游的“当前状态”变成多个版本并存的协调问题。

当前不采用该选项。除非未来存在明确的多谱面并发、复杂 GPU 资源准备或设备级吞吐需求，否则其复杂度不符合 Cuexis 当前 SDK 和参考 Player 的维护目标。

## 4. 推荐基线的边界

### 4.1 判定边界

`cuexis_judgement` 保持现有职责：Input normalization、Requirement、Kernel、Fold、Score、Replay、Snapshot 和诊断。它只能发布以下两类数据之一：

- 已提交 Fact 前缀及稳定 cursor；或
- 已由判定层完成语义解释的 immutable `PresentationEffectBatch`。

判定层不发布 `World` entity、EnTT handle、`PropertyOverrideWrite`、`RenderVisible` 或 `FrameSnapshot`。表现效果必须带有稳定的 requirement/effect identity、run scope、commit cursor、phase/outcome/timing 和明确的 target identity，但不携带 Runtime 私有对象。

### 4.2 表现适配边界

在 Playback 内建立内部 `PresentationProjectionAdapter`，先作为私有实现，不立即形成新的公共 SDK 组件。它负责：

- prepare 阶段把 `GameplayFactBinding` 编译为按 requirement identity、phase、outcome、timing 和 aggregation 索引的表。
- 运行时只读取从上次 cursor 到本次 committed prefix 的新增结果。
- 处理 group commit、冲突、目标缺失、窗口结束和 projection scope 切换。
- 生成独立的 immutable `PresentationProjection`，而不是直接修改 World。
- 把缺失目标、冲突和容量错误映射成表现层诊断，不改变已经提交的 Judgement Fact。

初期可以把 `PresentationProjection` 转换为现有 `PropertyOverrideWrite`，以复用 Runtime 的事务和层级解析；这一步是兼容桥，不是最终边界。

### 4.3 Runtime 与帧边界

Runtime 保留 owner-thread 和事务回滚，但入口改为显式消费 `PresentationProjection`。`updateGameplay()` 的兼容签名可以在第一批保留，内部将 `PropertyOverrideWrite` 归入 adapter 产生的投影；后续再评估是否把 GameplayOverride 从通用 World 属性层迁移为独立的 presentation state。

FramePacket 初期可为 Playback 内部的拥有型包装，至少包含：

```text
HostContext
projectionScope
frameId
presentationTick
runtimeFrame
committed/folded Fact cursor
folded work tick（如适用）
FrameSnapshot
portable resources
feedback membership
```

公共 SDK 继续以现有 `FrameSnapshot` 为权威帧值；FramePacket 只负责 owner、代际、反馈和资源生命周期。

### 4.4 Worker 与 Renderer 边界

推荐基线保留一个 Playback worker，但将任务拆成逻辑阶段：

```text
D1 capture/advance
  -> D2 committed prefix / effect publication
  -> D3 projection + Runtime evaluation + FramePacket build
  -> latest-value exchange
  -> main-thread Renderer submit/present
  -> asynchronous PresentReceipt observation
```

D2 和 D3 可以先在同一个 owner-thread 执行；切分要求是数据不可变、阶段可测量、失败边界清晰。控制、Pause/Resume、Seek、Reload、Close 和资源 fence 继续使用同步协调。

## 5. 实施批次

### PD-0：基线与行为表征

只增加内部计时和计数，不改变行为：

- Judgement advance、projection、Runtime update、extractFrame、Frame Ledger、Renderer submit/present 分段耗时。
- Fact cursor、绑定扫描数量、投影写入数量、Snapshot 对象数量和复制字节数。
- 每帧 worker 往返、latest frame 丢弃数、PresentReceipt 延迟和 Unknown 状态。
- 记录判定结果、Replay digest、FrameDigest、资源 generation、scope 和窗口状态。

退出条件是得到 Debug/Release 同一内容、同一输入、同一时钟的可重复基线。该批次不产生生产预算。

### PD-1：稳定的 Judgement 发布视图

在 Playback 与 Judgement 之间增加只读的内部 publication view，禁止 Playback 继续依赖 Judgement 的 `src/diagnostic_policy.hpp` 和 Fact 容器布局。优先保留 Fact cursor 和稳定 accessor，只有在内部事实表示确实阻碍扩展时才增加 `PresentationEffectBatch`。

验收要求：Judgement 单元测试不链接 Runtime/World/Renderer；Playback 可以通过 publication view 完成原有结果查询；错误码和 `sourceCode` 映射保持一致。

### PD-2：表现绑定索引与 Projection Adapter

把绑定索引从每帧线性扫描改为 prepare 阶段构建；运行时维护 `lastPublishedCursor`、active effects、group state 和 rejected group state。投影结果先生成 immutable `PresentationProjection`，再通过兼容桥交给 Runtime。

验收要求：Fact 顺序、same-tick group commit、冲突、目标缺失、窗口到期和 scope 切换与基线一致；新增绑定只影响 adapter 测试，不需要修改 Judgement Kernel。

### PD-3：Runtime 投影入口与 FramePacket

为 Runtime 增加明确的 presentation projection 入口，并保留旧 `PropertyOverrideWrite` 路径作为过渡。FrameSnapshot 生成完成后封装为拥有型 FramePacket，验证 context、generation、scope 和资源闭包，再发布到 latest-value exchange。

验收要求：Runtime rollback、FaultPrefix、Snapshot、FrameDigest、Seek、Reload 和旧内容兼容性不变；Packet 丢弃不会回退判定 cursor，也不会提交过期 scope。

### PD-4：异步 PresentReceipt 与反馈账本

保留 FrameObservationLedger 的 receipt、Unknown、迟到确认和 expiry 语义，但把 observation 从 D3 的同步热路径移出。Renderer 仍在主线程提交和 present；receipt 只携带 frame identity、scope、generation、状态和时间戳，之后异步回写 ledger。

验收要求：重复 receipt、Unknown 后成功、迟到 receipt、scope 切换、窗口最小化、device lost 和 close 排空与现有协议一致。

### PD-5：后续 owner 拆分触发审查

PD-0 至 PD-4 完成后运行 Release 与目标设备测量。如果判定提交和表现构建仍互相阻塞，或 latest-value exchange 长时间积压，则形成独立的多-owner 架构评估报告。该评估只在 owner 接受报告后进入实施，不在本计划中预先冻结线程数量。

## 6. 保持不变的语义和兼容性

- Gameplay v2 的 observationTick、dispatchTick、commitTick、processed frontier、sealed/folded cursor 和 advance horizon 继续分开。
- Fact 的提交、Fold、Replay、Snapshot、Seek 和 FaultPrefix 语义不因表现帧丢弃而改变。
- `projectionScope`、`rendererGeneration`、`resourceGeneration`、`frameId` 和 control sequence 继续共同验证帧归属。
- Renderer 仍只消费后端无关的 `FrameSnapshot`，不把 Judgement 或 Runtime 私有类型泄漏到公共头。
- 旧 Runtime/World 属性层可在 PD-2/PD-3 期间作为兼容实现，但新代码不得继续把它当作 Judgement 到表现的公共边界。
- 若公共头字段、公共错误码或 SDK API 改变，必须另行更新相应 ADR/Spec/ABI 和版本差异记录。

## 7. 验收门禁

### 功能与确定性

- Headless Judgement 在无 Runtime、无 Window、无 Renderer 环境下完成相同 Replay 和 Snapshot 结果。
- 同一输入与同一 content identity 下，判定结果、Fact cursor、Score、Replay digest 和 FrameDigest 与基线一致。
- Presentation Projection 的 group、timing、phase、outcome、conflict、missing target 和 expiry 行为有独立测试。
- FramePacket 的 scope、generation、resource closure、丢弃、重复提交和过期 receipt 有独立测试。

### 性能与容量

- 分别报告 Judgement、Projection、Runtime、Snapshot、Renderer 和等待时间；不能只报告端到端 FPS。
- 比较绑定扫描数量、复制字节数、FramePacket 丢弃数、worker 往返和 receipt 延迟。
- Debug 结果只能作为定位证据；Release、目标平台、真实设备和长时运行证据才可形成预算。

### 故障与恢复

- Projection 分配失败不撤销已经提交的 Judgement prefix。
- Runtime 投影失败保留既有有效表现状态，并提供明确的 failure stage。
- Renderer submit/present 失败不伪造 Judgement 完成，也不把失败帧重新标记为已确认。
- Pause/Resume、Seek、Reload、Reset、Device lost 和 Close 的旧 scope 不能污染新 scope。

## 8. 回退与提交边界

每个批次独立提交，推荐顺序如下：

1. `perf: add gameplay presentation stage counters`
2. `refactor: add judgement publication view`
3. `refactor: index gameplay presentation bindings`
4. `refactor: introduce runtime presentation projection`
5. `perf: publish latest frame packets`
6. `refactor: observe presentation receipts asynchronously`

每个提交必须能单独回退。PD-1 至 PD-4 期间保留旧同步 FrameEnvelope 路径作为故障回退或测试对照；当新路径通过功能、确定性和 Release 基线后，才删除重复路径。

## 9. 难度与代码复用评估

以下为基于当前代码结构的规划估计，不是实施后的度量：

| 范围 | 预估难度 | 可复用程度 | 判断 |
| --- | ---: | ---: | --- |
| Judgement 核心 | 低 | 85%–95% | 保留 Kernel、Fold、Replay、Snapshot 和输入语义；主要增加 publication accessor |
| Presentation Renderer | 低 | 90%–100% | 继续消费 FrameSnapshot；通常只需适配 FramePacket 解包和 receipt 时序 |
| Runtime/World | 中 | 60%–75% | 保留时间轴、动画、相机、资源和事务回滚；新增 projection 入口，后续可再拆 presentation state |
| Playback projection | 高 | 45%–65% | 现有绑定语义可复用，但 `publishGameplayProjection()` 的扫描、状态和失败边界需要重组 |
| Playback worker/ledger | 中高 | 65%–80% | context、scope、generation、receipt 和容量协议可复用；SessionSample 顺序和发布方式需要调整 |
| Player host loop | 中 | 70%–85% | 输入、窗口、GL owner 和控制流程可保留；sample/present 之间增加 latest packet 与异步 receipt |

整体难度评估为**中高**，约为一次跨 Playback、Runtime、Judgement adapter 和 Player 协调层的结构性重构。难点不是判定算法本身，而是以下四项同时保持一致：

1. Fact/Replay/Snapshot 确定性不能被表现帧丢弃破坏。
2. Runtime 的 owner-thread、候选状态和回滚不能被异步 packet 生命周期破坏。
3. scope、generation、resource fence 和 receipt 必须能识别迟到或过期消息。
4. 旧 `FrameSnapshot` 和外部 Playback consumer 不能被无意地改成第二套 API。

复用策略应是“保留语义核心，替换桥接和调度”：Judgement、Renderer、Runtime 求值和大部分控制协议保留；重点重构 `publishGameplayProjection()`、绑定索引、FrameEnvelope 发布路径和 PresentReceipt 观察路径。

## 10. 与后续 owner 拆分的关系

多-owner 拆分不是本计划的默认目标。只有在推荐基线完成后出现以下证据之一，才进入独立评估：

- Release 中 D2 Judgement commit 与 D3 Projection/Frame build 持续互相等待，且等待占比达到预算主要部分。
- latest-value exchange 持续积压，帧丢弃导致可观察的表现滞后或反馈证据失真。
- Runtime/Projection 的 owner-thread 事务成本在拆分数据边界后仍然超过可接受预算。
- 推荐基线已能证明可独立恢复、回退和观测，但单 owner 仍无法满足目标设备的实时预算。

进入多-owner方案时，必须另行提交线程/owner 拓扑、跨线程状态快照、资源生命周期、控制 fence、故障恢复和容量预算设计。不能因为 PD-2 或 PD-3 的局部耗时增加就直接引入固定四线程模型。

## 11. 退出与交接

Stage 7-PD 退出需要：

- PD-0 至 PD-4 的实现和逐批测试证据，或明确记录未实施批次与原因。
- 判定、表现、Runtime、Renderer 和 receipt 的前后行为对照结果。
- Debug/Release/目标平台的分段性能数据，以及未执行的 hosted、实体设备和生产容量检查。
- 后续 owner 拆分的触发审查结论：继续推荐基线、暂停、回退，或经 owner 授权进入多-owner方案。
- 更新 [CURRENT_STATUS.md](../../../CURRENT_STATUS.md) 和 Stage 7 交接记录；不得把本计划的存在写成 Stage 7A、RPA 或 Stage 7 整体已关闭。

本计划完成后，Stage 7A 继续拥有 Gameplay 语义和判定验收，Stage 7-RPA 继续拥有实时协调和设备验收，Stage 7-PD 只拥有判定到表现的内部架构重构与其证据。
