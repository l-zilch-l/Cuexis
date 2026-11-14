# Stage 7-PD: Judgement/Presentation Decoupling Plan

状态：completed；owner 于 2026-11-14 接受 Stage 7-PD／PD-5 带限制退出及遗留责任交接；PD-5 历史“未通过但定案”保持，完整性能／内存及生产保证未获接受。

更新日期：2026-11-14。退出范围见[受限退出与缺陷交接](../../../../stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-14-limited-exit-and-defect-handoff.md)。以下实施授权与批次结果保留历史语境。

2026-10-16 历史实施授权：owner 要求“阅读接手文档并阅读相关文档，本次任务是完成PD计划”，随后要求先提交当前状态存档、后续显示 todolist。此前设计阶段关于不得启动实施的限制不再适用于本次已授权工作；正式合同接受、公共版本、逐批验收与发行门禁仍分别成立。内部接口名仍须以实际实现与证据核对。

本计划拥有目标、批次与退出条件；配套[内部设计与验收细则](design.md)拥有本计划的接口字段、状态转移、数据寿命、线程时序和测量方法。既有 Gameplay/RPA 语义仍以其正式 Spec/ABI 为准，细则不替代它们。

2026-10-21 历史读取候选的机制、F1 比较基线和事前保留条件见
[C2 输出槽预登记](pd-5/pd5-read-result-candidate.md)；C2 已关闭，该历史登记不构成正式测量注册或性能接受。

2026-10-23 C2 关闭后，读取侧归因与 40k 分域内存观测的候选路径见
[PD-5 读取候选与分域内存观测实施方案](pd-5/pd5-read-candidate-and-memory-domains-plan.md)；R0-a/R0-b 已执行并判定，B0 已接受、B1 静态工具完成；剩余 B2/B3 采数未授权，不登记正式测量，也不改变本计划的退出条件。

本子阶段处理判定结果、表现绑定、Runtime/World 求值和画面交付之间的实现耦合。2026-10-16 重审后，采用单一表现状态源、轻量提交修订描述、中性只读物化和独立交付绑定；首版不再要求每次更新保存完整结果副本。完整帧只为取得额度的请求生成，观察历史独立有界。方向比较、迁移成本和未来扩展影响见[设计重审记录](../../../../stage_reports/stages/stage-07/04-pd/design/2026-10-16-pd-transaction-and-delivery-design-review.md)。

本计划与下列工作保持独立：

- Stage 7A 负责 Gameplay v2 的 Requirement、Judgement、Ruleset、Score、Replay、Snapshot 和语义验收。
- Stage 7-RPA 负责实时捕获、时钟、控制、设备、资源 fence、回执和综合设备验收。
- Stage 7-PD 不修改 Gameplay v2 的判定语义，不重新选择 RPA 的控制协议，也不把表现重构结果写成 7A 或 RPA 的完成证据。

Stage 7-PD 可以在 7A/RPA 尚未关闭时进行内部实现准备和受控迁移，但正式退出必须消费两者已经接受的语义、上下文和资源身份合同。当前实现状态仍以 [CURRENT_STATUS.md](../../../../CURRENT_STATUS.md) 为准。

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

从依赖顺序看，在后续取得实施授权后，RPA-F1–F4 未退出不阻止 `PD-0`、`PD-1`；B3/B4 相关合同完成定向核对后可实施 `PD-2`。这不是本轮启动授权。`PD-3` 和 `PD-4` 只能在不改变 B4 资源、fence、receipt、关闭和故障语义的前提下实施；如果需要改变这些公共或宿主边界，必须先回到 RPA 对应 Spec/ADR 和版本差异记录，而不是在 PD 中绕过。

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

- [playback_session.cpp](../../../../../engine/playback/src/playback_session.cpp) 的 `publishGameplayProjection()` 直接读取 `judgement::PhaseOutcomeFact`，扫描绑定，并生成 `RenderVisible` 写入。
- [runtime_session.cpp](../../../../../engine/runtime/src/runtime_session.cpp) 的 `updateGameplay()` 将 Gameplay 写入解析为 World 属性层，并与 Runtime 时间推进、候选状态和回滚绑定。
- [playback_session.cpp](../../../../../engine/playback/src/playback_session.cpp) 的 `extractFrame()` 直接读取 World、EnTT、Transform、Appearance、Camera 和资源布局。
- [session_worker.cpp](../../../../../app/execution_host/session_worker.cpp) 在同一个 `SessionSample` 中顺序完成 Gameplay/Runtime 更新、帧提取、反馈登记和 latest 帧返回。
- [gameplay_internal.hpp](../../../../../engine/playback/src/gameplay_internal.hpp) 直接包含 Judgement 的 `src/diagnostic_policy.hpp`，使 Playback 依赖 Judgement 私有实现。

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
  -> PresentationProjectionAdapter
  -> projection transaction
  -> Runtime/World commit + presentation state revision
  -> demand-driven read + FrameSnapshot materialization
  -> confirmed delivery binding
  -> Renderer submit/present
  -> compact observation journal
```

具体目标如下：

- Judgement 提供稳定的只读发布视图，明确 sealed、publishable、control cursor 和许可，不依赖 World、EnTT、FrameSnapshot 或 Renderer。有 Fold 时只消费其成功前缀，无 Fold 的合法旧路径沿 sealed 前缀；faulted 查询不激活新效果。`PresentationEffectBatch` 只是 adapter 派生值，不作为第二个判定发布源。
- 表现绑定在 prepare 阶段建立索引，运行时只消费新增的 committed prefix；没有新增 Fact 时仍可按当前时间更新活动表现和生命周期。
- Runtime 将准备数据、事务状态和已提交表现值区分开。Projection、Runtime/World、cursor、token、生命周期和轻量状态描述形成一次逻辑提交；不新增成功后捕获失败的回滚窗口。
- 有消费者额度时，通过同 owner 中性只读桥直接生成 `FrameSnapshot`；无交付需求不捕获完整中间结果。数据来源与已确认交付许可分离，Pause 重绘不推进 Runtime。
- Renderer 只消费后端无关的 `FrameSnapshot`；PresentReceipt 进入独立的紧凑 observation journal，不持有完整帧。
- 保留单一 Playback owner、owner-thread 规则、控制 fence、resource generation、projection scope 和现有失败语义。
- 首版支持一个实际呈现端及独立 CPU 截图消费者；对多窗口、录制、增量交付和 owner 拆分列明可复用边界、必改模块及重新评估条件，不将接口可扩展写成已有产品能力。

### 2.2 非目标

- 不修改 Gameplay v2 的判定、Fact 因果序、Ruleset、Replay 或 Snapshot 语义。
- 不把 `FrameSnapshot` 与新的公共 `RenderPacket` 并列为第二套公共权威帧格式。
- 不在本子阶段引入任意脚本、动态插件、宿主回调或新的渲染后端。
- 不把 World 多版本化为公共协议，也不同时维护第二套独立的表现求值语义。
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

这是此前的中间方案，仅作为受限迁移桥；当前采用基线唯一见 §3.5。完整帧交换不得继续承担历史保留职责，也不得成为 Runtime 推进的条件。

### 3.3 选项三：判定 owner 与表现 owner 双阶段流水线

判定 owner 发布 committed prefix 或 PresentationEffectBatch，表现 owner 独立消费并生成 FramePacket，主线程只负责窗口、设备和 Renderer。优点是判定计算可以与表现求值并行，缺点是需要处理跨 owner 的 scope、generation、资源生命周期、快照一致性、迟到结果、Seek/Reload/Close 和故障恢复。

该选项只有在单 owner 分阶段方案完成后仍无法满足 Release/设备预算时才进入独立设计，不预先冻结线程数量或队列模型。

### 3.4 选项四：完整多阶段流水线

把判定、表现投影、Runtime 求值、资源准备和渲染提交分别放入多个长期运行的流水线阶段。它可能提供最高吞吐，但会引入最大的排队延迟、内存占用、跨阶段背压、控制切换和错误恢复成本，也最容易把实时音游的“当前状态”变成多个版本并存的协调问题。

当前不采用该选项。除非未来存在明确的多谱面并发、复杂 GPU 资源准备或设备级吞吐需求，否则其复杂度不符合 Cuexis 当前 SDK 和参考 Player 的维护目标。

### 3.5 采用基线：单一状态源、按修订物化与独立交付绑定

本计划采用**共享准备数据、单一状态提交、按修订只读物化、独立交付绑定**。当前已提交表现值首版仍由 Runtime/World 持有，通过内部中性读取边界消费；不再把 World 与每更新捕获的完整数组作为两份需要同步的结果存储。跨线程消费者只取得 owning 帧，不借用当前 World。

职责固定如下：

1. **准备数据**：曲线、动画定义、绑定索引、静态基线和资源描述在 prepare 或内容切换时构建。可共享部分使用不可变 owning 值；它不携带本次运行的 cursor、生命周期或设备句柄。
2. **事务状态**：projection cursor、active effects、future queue、group state、token、override lifetime 和时间连续性由 Playback/Runtime owner 串行提交。失败时保留旧状态。
3. **已提交表现状态**：当前 World 值和轻量提交描述属于同一成功点。描述记录内容/布局、状态修订、原 T/frame/已消费前缀和激活来源，不存当前设备许可。物化通过 `PresentationRead` 直接填 Snapshot，不重跑 evaluator 或扣减 `RemainingFrames`。
4. **交付绑定与帧**：既有全域确认的 HostContext/资源证明与已确认消费注册共同决定交付资格；绑定改变不改写画面来源。消费者先准入，再生成 owning 帧；最后成功画面可独立保留用于暂停重绘。
5. **观察记录**：紧凑 journal 记录实际 attempt、membership 和证据；不持有完整帧，不决定 CPU/GPU 释放，不承担实际 present 的执行去重。

```text
prepared definitions + transaction state + RuntimeFrame
                         |
                one evaluator / commit
                         |
        current World + committed state revision
                         |
             admission -> PresentationRead
                         |
                 owning FrameSnapshot
                         |
        confirmed DeliveryBinding -> renderer / CPU capture
                         |
                 compact observation journal
```

`FramePacket` 仍可作为宿主内部的短生命周期交付包装，但它不是 Playback 协议状态、不是完整帧历史，也不是 Runtime 的推进凭据。共享引用计数可用于不可变准备数据、确实共享的交付结果或资源引用；引用归零不能单独表示控制完成、设备安全完成、Unknown 终结或 Close 完成。涉及 SDL、GL、设备或回收队列的析构必须回到所属 owner。

### 3.6 其他方案的位置

- **每次更新保存完整中性结果**不再作为首版必需路径。它有利于跨 owner 物化，但增加无消费者时的捕获、复制与存储；只有实际需要 owning 状态传输或测量证明多消费者重复读取更贵时，再基于同一中性字段与容量合同引入。
- **活动 World 全量撤销日志**不作为新方向。新属性会增加恢复项，派生缓存容易遗漏；首版消除捕获后的撤销需求，并在 Runtime 内提取 prepare/apply。
- **整 World 双缓冲或多版本存储**可隔离失败，但需要复制组件、对象身份与生命周期；当前不承担这一通用重构，不能用它掩盖一次安装边界未定义。
- **同步单 owner 链**保留为行为基线和回退路径。它状态一致性最好，但会把投影、Runtime 求值和快照复制的成本直接压到显示节拍。
- **单 owner 分阶段加完整帧 latest-value ledger**可作为过渡实现，但不再是目标架构。它只有在交付对象有明确额度、完整帧不进入历史、且逻辑更新不依赖消费者归还时才可采用。
- **场景增量传输**是交付层优化。它需要版本化基线、丢失后的完整重同步和每消费者额度，不能成为 Runtime 的第二种表现语义。
- **判定 owner/表现 owner 流水线**只有在单 owner 的数据边界稳定且 Release 证据显示仍有必要时才单独设计。
- **事件日志驱动 Renderer 或 World 多版本化**暂不采用。它们会把表现规则、追赶、版本保留和恢复逻辑复制到每个消费者，维护成本高于当前问题的收益。

## 4. 推荐基线的边界

### 4.1 判定边界

`cuexis_judgement` 保持现有职责：Input normalization、Requirement、Kernel、Fold、Score、Replay、Snapshot 和诊断。它只发布已提交 Fact 前缀及稳定 cursor。`PresentationEffectBatch` 如需存在，由 Playback 内的 adapter 从该前缀按稳定 identity 派生，并且必须能从同一前缀重新构建。

`PublicationRead` 是同一 owner 操作内的借用视图，下一次 mutation/reset/替换后失效；读取 epoch、from/publishable cursor、no-Fold/fault/control 分支及诊断桥见[细则 §2](design.md)。公共 owning query/ReplayEvaluation 保持原有完整结果，不以热路径视图代替。

判定层不发布 `World` entity、EnTT handle、`PropertyOverrideWrite`、`RenderVisible` 或 `FrameSnapshot`。表现效果必须带有稳定的 requirement/effect identity、run scope、commit cursor、phase/outcome/timing 和明确的 target identity，但不携带 Runtime 私有对象。

### 4.2 表现适配边界

在 Playback 内建立内部 `PresentationProjectionAdapter`，先作为私有实现，不立即形成新的公共 SDK 组件。它负责：

- prepare 阶段把 `GameplayFactBinding` 编译为按 requirement identity、phase、outcome、timing 和 aggregation 索引的表。
- 运行时只读取从上次 cursor 到本次 committed prefix 的新增结果。
- 处理 group commit、冲突、目标缺失、窗口结束和 projection scope 切换。
- 生成独立的 immutable `PresentationProjection`，而不是直接修改 World。
- 把缺失目标、冲突和容量错误映射成表现层诊断，不改变已经提交的 Judgement Fact。

初期可以把 `PresentationProjection` 转换为现有 `PropertyOverrideWrite`，以复用 Runtime 的事务和层级解析；这一步是兼容桥，不是最终边界。

聚合与冲突规则消费 Gameplay Spec 的 FactBinding 首用合同，不重新裁定：scope 内 `(FactId,targetId)` 去重、prepare 拒绝不能证明互斥的相反值、partial-group 保留有效成员并稳定拒绝新成员。索引、expiry、无新 Fact 和失败安装顺序见[细则 §3](design.md)。

### 4.3 Runtime 提交、状态读取与帧边界

Runtime 保留 owner-thread 和旧失败状态不变的语义。先准备 Projection、描述、计数、诊断和 Runtime 候选；Runtime 完成全部可失败工作，再安装 World/派生缓存并连续完成 Playback finalize。结果读取与 Snapshot 构建在这之后进行；GPU/audio 仍沿独立控制协议。

这条边界解决两个问题：

- Projection 不能先推进 cursor、再等待 Runtime 成功；否则 Runtime 失败会丢失可重试的 Fact。
- 不能在生成 FrameSnapshot 时重新执行一次会消耗生命周期的 Runtime 更新；同一成功更新的结果可以被一个或多个交付消费者读取。

现有 resolver 回滚只覆盖局部 Transform，不能替代派生矩阵和缓存的原子性。PD-3 先提取 World transform 的 prepare/apply，把 storage/拓扑/字符串等可失败工作留在准备阶段；不能仅标 noexcept 或克隆整 World。Runtime 成功到 finalize 之间无捕获、分配、stale 检查、外部回调或设备动作。失败阶段和证据要求见[细则 §5](design.md)。

当前 World 是首版权威存储，`PropertyOverrideWrite` 可作过渡输入。Runtime 中性只读桥封装组件访问，Playback 保留唯一物化器；不另写表现引擎。mutation guard 统一失效借用并发布修订，任意既有可变回调不假称可回滚。viewport 相机矩阵与原 extractFrame 错误留在物化边界，不能因交付失败改变 RuntimeUpdated。

提交描述与交付许可不得混在一个“结果身份”中：

```text
state source: sessionScope + resultVersion + content/layout/world revision
              source T / RuntimeFrame / discontinuity / consumed cursors / activation provenance
delivery:     confirmed HostContext + projectionScope + active ConsumerId + renderer instance / resource proof
attempt:      frameId + consumer / RequestId + source state identity + exact membership
```

首次内容提交即建立有效状态修订。Pause/Resume/合法 EOF 仅换交付许可时，不改 source T/cursor，不重建表现作用域；Seek/Reset/内容或表现绑定替换才重建作用域。该宿主策略变更必须先消费[细则 §11](design.md)的正式合同门禁。`FrameEnvelope` 是不可变 owning 值，journal 不反向持有它。

#### 4.3.1 载荷、资源和观察记录的独立寿命

2026-10-15 的受控复现证明，当前 `FrameObservationLedger` 的 frame slot 在 `takeLatest`、成功 `PresentReceipt`、`expire` 和 scope 切换后仍可能占用，容量 4 在第 5 帧、容量 4096 在第 4097 帧耗尽。这个问题说明完整帧不能兼任历史账本；单纯扩大槽位或给旧状态加 tombstone 不能构成修复。

以下寿命矩阵与[细则 §4、§6–8](design.md)共同约束后续实现；需要测量的容量数值另归细则 §9：

- **准备数据**：不可变定义按 content identity 共享，CPU owning 数据在最后使用者释放后销毁；设备对象不放入准备数据，按 resource generation 由设备 owner 单独退休。内容切换本身不证明旧使用者已结束。
- **事务状态**：由唯一 owner 持有；提交失败保留旧状态，成功提交替换旧状态。它不因 Renderer 慢而等待。
- **已提交表现状态**：首版只保留当前状态与必要候选/scratch，不强制完整结果副本。历史只在已准入的 owning 帧中保留；失去版本返回 VersionUnavailable。读取只访问已提交值，不重跑 evaluator 或推进 lifetime。
- **CPU 帧载荷**：只由实际交付对象和其读者持有。成功提交后可按请求生成；被丢弃、未取得消费额度或读取完成的载荷立即解除 owner 持有。CPU 析构不得调用 GL/SDL。
- **GPU/设备资源**：由 Renderer owner 管理 `resourceGeneration` 和设备使用完成。GPU fence 只决定设备资源何时可释放，不决定 CPU 帧或 Gameplay 是否推进。
- **反馈与 receipt journal**：只保存紧凑 identity、membership、状态、时间和必要错误/tombstone。它有独立的条数、字节和时间保留上界；完整 FrameSnapshot 不得因迟到 receipt 留在 journal 中。
- **Unknown 与迟到**：CPU 读者、GPU 使用和 present 证据是三个独立维度。UnknownInFlight/UnknownDetached 只描述结果未知时 CPU 是否已脱离；Detached 不表示 GPU 已结束。LateObserved 追加迟到证据，不覆盖到期状态；超出窗口报告 history unavailable，不复活帧或污染新 scope。
- **引用计数与回收器**：CPU 物理保活由读者 lease/最后引用管理，设备对象另由其 owner-issued retirement 与 fence 管理；回收 backlog 计入 bytes。CPU 引用归零不等待 receipt，也不单独证明设备/控制退休；回收器不能写 Playback/Gameplay 状态。
- **提交尝试**：每次实际 present 有独立 identity；CPU 截图没有假 present。Pause 重绘最后成功画面，允许它落后于当前状态；由确认后的绑定签发新 attempt，原数据来源与激活身份不变。
- **注册与响应**：普通帧和重绘均校验注册实例；注销确认只撤销后续执行资格，不代替存储释放。Ready 响应只持有尚未移交帧，领取后请求历史只留元数据；取消/移交与注册确认的详细规则归细则 §6.1、§6.3.1。

### 4.4 Worker 与 Renderer 边界

默认仍保留一个 Playback owner，但将逻辑职责拆开：

```text
D1 capture
  -> D2 admission/advance and committed prefix
  -> D3 projection transaction + Runtime evaluation
   -> committed state revision
   -> consumer admission / read / FrameSnapshot
   -> confirmed binding / renderer or CPU capture
  -> compact observation journal
```

首版 D2/D3 和物化在同一 owner 执行，呈现端为一个，CPU 消费者独立额度。无额度只跳过构帧/交付，不跳过已承诺 Runtime 更新。控制在事务边界优先，跨线程只传 owning 帧/候选。旧 SessionSample 包装不能与新的独立更新重复调用。未来独立 Renderer 必须扩展 RPA 参与者及设备绑定，不能仅增加 ConsumerId；具体影响范围见[细则 §6.3](design.md)。

## 5. 实施批次

### PD-0：基线与行为表征

回复等待与请求身份回收缺陷作为已授权的独立前置修复处理，不扩张本批次“只观测”的范围。
修复使用持久回复终态、调度器唯一回收进度及幂等退役确认；合同归 RPA Spec/宿主边界，
实施证据见[前置修复报告](../../../../stage_reports/stages/stage-07/03-rpa-legacy/performance/2026-10-17-retention-progress-and-wait-boundaries.md)。
修复后重新绑定源码、binary 和测量组，保留旧 inconclusive 结论；不提前实施 PD-4 帧寿命或异步 receipt。

本批次先登记测量 manifest、testOnly profile、统计方法与噪声界；只增加内部计时和计数，不改变行为：

- Judgement advance、projection、Runtime candidate/install、只读物化、Renderer submit/present 分段耗时；首版不新增每更新完整结果捕获。
- Fact cursor、绑定扫描数量、投影写入数量、RuntimeEvaluationState 复制字节、Snapshot 对象数量、分配次数和复制字节数。
- 每个消费者的交付请求、跳过构建次数、交付对象年龄、版本滞后、receipt 延迟和 Unknown 状态。
- 记录判定结果、Replay digest、FrameDigest、资源 generation、scope、resultVersion 和窗口状态。

旧路径没有的结果版本或交付指标记 not applicable，不为补计数提前实现新结构。退出条件是按[细则 §9](design.md)取得相同内容/输入/时钟的 Debug/Release 可重复基线；性能对照至少 3 组配对重复，分位数、样本数、噪声和未测项明确。该批次不产生已接受生产预算。

### PD-1：稳定的 Judgement 发布视图

在 Playback 与 Judgement 之间增加只读的内部 publication view，禁止 Playback 继续依赖 Judgement 的 `src/diagnostic_policy.hpp` 和 Fact 容器布局。视图必须区分 sealed prefix、成功 Fold prefix、work tick、run identity 和读取有效期；不能把“可查询 Fact”直接等同于“可用于表现的 Fact”。

验收要求：[细则 §2](design.md)的读取范围和失效规则落实；Judgement 单元测试不链接 Runtime/World/Renderer；内部热路径与原 owning 查询同源但不改变公共结果；错误码和 sourceCode 映射一致；有 Fold 时 sealed 但未 Fold 的 Fact、以及 faulted 查询均不激活新 token；合法 no-Fold 路径保留。

### PD-2：表现绑定索引与 Projection Adapter

把绑定索引从每帧线性扫描改为 prepare 阶段构建；运行时维护 `projectionCursor`、active effects、group state、future queue 和 rejected group state。投影从 cursor 到成功可发布的 Fact 前缀构建候选事务，按稳定 identity 去重；不能在 Runtime 尚未成功时推进 cursor。

验收要求：按[细则 §3](design.md)对照 Fact 顺序、groupCommit/partial-group、aliases/去重、冲突、目标缺失、窗口到期、无新增 Fact 的活动更新和 scope 切换；结果与权威合同及既有 golden 一致。新增表现绑定只影响 adapter，不修改 Judgement Kernel。prepare 明确能力 ID/revision 与绑定支持表；未知必需 phase/outcome/聚合不能静默跳过，7B+ 扩展准入见细则 §12.1。

### PD-3：事务完整性、状态读取与交付绑定

按以下三个子批次顺序推进，分别可审查和回退：

1. **PD-3-a 事务完整性**：提取现有层级算法的 prepare/apply；预留 storage/拓扑/组件写入，覆盖派生矩阵和缓存，确定 Runtime 成功后连续且无失败的 Playback finalize。其证据先于新物化路径切换。
2. **PD-3-b 状态读取**：发布轻量修订描述，增加 Runtime 中性 PresentationRead 与唯一 Snapshot 物化协调入口；按已有表现域拆分有类型的读取/填充职责。首版不建立每更新的完整结果缓存；update/override/恢复/受控 mutation 使用共同 revision guard。
3. **PD-3-c 控制绑定**：在正式宿主差异接受后，接入来源身份与确认许可分离、控制操作矩阵、保留画面身份和完整 context 校验。Pause 不用 Runtime 更新刷新图像，不重建反馈义务。

不要求立即删除 World 或重写整个 RuntimeEvaluationState，但新增事务/读取边界不能变成永久第二套求值实现。静态定义共享与 scratch 复用单独测量，不用未经证明的零复制或回滚承诺作为退出条件。

验收要求：父子矩阵、缓存/有效位、组件增删、安装前失败、安装后物化失败分别覆盖；首次提交可提帧、query 不推进修订、RemainingFrames 不重扣；同源 Snapshot/FrameDigest 精确等价。Pause drain/Resume/EOF、当前状态领先最后成功画面、resize 和设备换代都验证来源/绑定，失败面按[细则 §4–5](design.md)固定。

### PD-4：按需帧交付与紧凑观察记录

消费者先取得交付额度，再从当前状态或明确的 retained handle 物化。无额度/过期请求可以合并或跳过，不回退 projection/Runtime。首版一个呈现端与多个 CPU capture 消费者；独立第二 Renderer 明确拒绝。

PD-4 必须先实现并验证 4.3.1 的寿命矩阵。具体容器可以选择固定槽、节点池或带异步物理回收的 owning storage，但不得让完整帧承担历史保留职责。

Frame delivery 记录与 observation journal 分离：

- delivery 对象拥有 CPU 读者 lease 和本次 attempt identity；读者完成即解除载荷。
- journal 只保存 attempt identity、上下文、feedback membership、receipt 状态、时间、错误和有界 tombstone。
- Renderer 设备资源和 GPU fence 不通过完整帧历史回收；后端按自身机制管理 generation 和安全释放。
- 共享引用计数、节点池或异步 reclaimer 可作为物理实现，但不得成为 Gameplay/receipt 协议的隐式状态机。
- 反馈历史保留范围必须显式报告；范围外的 receipt/query 返回 history unavailable 或 UnknownEvidence，不能伪造精确历史。
- UntilReset 使用显式无 end 分支，不能借 INT64_MAX 表示；公共 FeedbackObservation 的布局迁移先消费细则 §11 的 Spec/ABI/SDK minor 门禁。
- Renderer 自身维护一次执行标记与最后成功 backing，不依赖可丢 receipt 或可淘汰 journal 决定是否再次 present。
- 保留资格撤销先消费宿主候选 §8.4 的正式接受门禁，再验证可靠通知、迟到重绘拒绝和真实引用释放；内部 NoRetainedFrame 不当作已有公共错误码。
- 普通帧与 retained 重绘统一绑定 ConsumerId；激活/撤销经 Renderer owner 确认，旧注册失效后才能接纳同端新注册。消费宿主候选 §8.3 与细则 §6.3.1，不新增公共 HostContext 代数。
- 响应领取与取消/过期有唯一先后；deadline 限制最迟移交时间，已移交帧按自身寿命结束。请求去重缓存不持有完整帧，重复 request/poll/take 不再次交付或 present。

暂停使用最后成功画面，允许其 source resultVersion 落后于当前版本；同 viewport 复用载荷，resize 按独立额度物化。确认新控制绑定才允许新 attempt，原 T/frame/cursor/membership 不改写，不创建反馈义务。

验收要求：落实[细则 §6–8](design.md)的 RealtimeLatest/ExactCapture、三维寿命、显式有限/无期限到期、去重及 Close。验证丢回执后暂停重绘、journal 淘汰后重复 envelope 不再执行、消费者注销后内存仍入账、极值 T 和多设备拒绝。按细则 §12.4 验证保留资格撤销、NoRetainedFrame 与设备失效；平台压力策略的真实证据归 Stage 12。未来录制/导出的接入条件保留，不计为已交付产品；观察缺失不反向阻塞 Gameplay。

补充验证注册 A 注销/B 同端加入期间的旧普通帧、旧激活/撤销/确认、撤销先到、未确认激活遇
Close，以及 CPU/GPU 仍持有旧载荷的情形；拒绝旧副本不伪造原 attempt 的 NotSubmitted。
同端控制/资源换代与 Renderer 实例或代数更换分别覆盖，不能把旧注册许可带入新呈现端；
当前注册描述的保留、同键异载荷和精确历史淘汰后的拒绝行为也须验证。
以最低双载荷 profile 覆盖连续领取、取消/过期/领取竞争和请求历史保留，确保已领取响应不因去重
窗口长期占槽。设计核对与本轮边界见[注册与响应自查记录](../../../../stage_reports/stages/stage-07/04-pd/design/2026-10-16-pd-registration-and-plan-self-review.md)。

### PD-5：后续 owner 拆分触发审查

PD-0 至 PD-4 完成后按[细则 §9](design.md)运行 Release 与目标设备测量；仅同一预算项在 3 个预声明有效窗口持续超界才触发独立拆分设计报告。阈值未接受或噪声未受控则记录证据不足。报告经 owner 接受并另有实施授权后才可新增 owner，不预冻结线程数量。

2026-10-19 owner 授权测量就绪检查和阈值方案；[现场检查](../../../../stage_reports/stages/stage-07/04-pd/pd-5/preparation/2026-10-19-pd5-measurement-readiness.md)完成，
[本机测量协议与阈值](pd-5/pd5-measurement-proposal.md)已获 owner 明确接受。2026-10-20 P0 已完成两窗各 36 个正式进程，owner 要求先核查与修复局部性能回归，再重新测量；[两窗证据与诊断接手](../../../../stage_reports/stages/stage-07/04-pd/pd-5/measurement/2026-10-20-pd5-two-window-findings-and-diagnostic-handoff.md)保留未完成固定组及根因证据缺口。第三窗不自动续跑，完整三窗接受及连续三窗拆分触发均未成立；旧/新宿主 ACK 与异步 present-return 边界不能混用。

## 6. 保持不变的语义和兼容性

- Gameplay v2 的 observationTick、dispatchTick、commitTick、processed frontier、sealed/folded cursor 和 advance horizon 继续分开。
- Fact 的提交、Fold、Replay、Snapshot、Seek 和 FaultPrefix 语义不因表现帧丢弃而改变。
- `projectionScope`、`rendererGeneration`、`resourceGeneration`、`frameId` 和 control sequence 继续共同验证帧归属；PD 内部增加有效 ConsumerId 的一致检查，不改变这些既有身份的含义。
- Renderer 仍只消费后端无关的 `FrameSnapshot`，不把 Judgement 或 Runtime 私有类型泄漏到公共头。
- Runtime 更新成功次数、`RemainingFrames`、Fact cursor、Replay、Snapshot 和 FaultPrefix 不因消费者丢帧或观察记录压缩而改变。
- Runtime/World 是首版当前表现值存储；上层只通过中性读取边界消费，不直接依赖 EnTT 布局。未来更换存储须复用求值和物化语义，不能建立第二个可写结果源。
- 若公共头字段、公共错误码或 SDK API 改变，必须另行更新相应 ADR/Spec/ABI 和版本差异记录。

## 7. 验收门禁

### 功能与确定性

- Headless Judgement 在无 Runtime、无 Window、无 Renderer 环境下完成相同 Replay 和 Snapshot 结果。
- 同一输入与同一 content identity 下，判定结果、Fact cursor、Score、Replay digest 和 FrameDigest 与基线一致。
- Presentation Projection 的 group、timing、phase、outcome、conflict、missing target、expiry 和无新增 Fact 时的时间推进有独立测试。
- 单一状态提交、来源元数据和 FrameSnapshot 在相同 viewport/逻辑采样点下等价；正常候选失败保留旧状态。已成功 Runtime 更新后的物化失败不冒充 Runtime rollback。
- Frame delivery、attempt identity、consumer lease、资源 generation 和 compact observation journal 有独立测试；journal 不持有完整 FrameSnapshot。
- 验证未来可靠录制/离线导出的接入和失败边界与实时窗口隔离，慢消费者不改变活动 Gameplay 更新语义；本阶段不交付录制/导出产品。

### 交付寿命、历史与容量

- 长期运行中，已完成读取的 CPU payload 不会再次耗尽交付额度；测试记录 live payload、consumer lease、result version 和高低水位。
- 无消费者请求或准入时请求已过期，不构建完整帧；构建后尚未移交即过期则结束该请求。后续合法请求可以物化届时仍为当前的已提交状态，不承诺恢复已失去的历史修订。
- 长暂停的最后成功画面不会无限产生新的 Gameplay 更新、FrameSnapshot 或 feedback obligation。
- 被合并/丢弃的交付、Unknown、迟到成功、重复回执和旧 scope 回执都幂等、可审计，不复活已释放载荷或污染新 scope。
- CPU payload、GPU in-flight、当前状态/候选 scratch、反馈/receipt journal 和 scope/generation 历史分别测量并有独立上界；每个高水位、容量耗尽和 history unavailable 行为可观察。
- payload/consumer 背压不得改变 Gameplay；纯 observation/journal 耗尽报告 Unknown/coverage loss；可靠输入/控制/Fact/Replay 和必要生命周期记录耗尽才沿既有 `host.rpa.capacity_exceeded`/`Suspended` 合同。
- PD 完成后重新绑定 RPA-F3 的内容、稳态、Snapshot/Seek、Replay 和 Packed 测量，并以新的实现 SHA 记录；既有 RPA 数值不被直接当作 PD 后结论。

### 性能与容量

- 分别报告 Judgement、Projection、Runtime、Snapshot、Renderer 和等待时间；不能只报告端到端 FPS。
- 比较绑定扫描数量、RuntimeEvaluationState 复制字节数、FrameSnapshot 构建分配、消费者交付请求/跳过数、结果版本年龄、worker 往返和 receipt 延迟。
- 单独报告 CPU payload、GPU in-flight、World/候选状态/scratch、轻量描述与 journal 的 live bytes；覆盖无消费者、U≫F、每更新一帧、多截图、Pause/resize、稀疏变化和拓扑变更，具体成本模型见细则 §9.4。
- Debug 结果只能作为定位证据；Release、目标平台、真实设备和长时运行证据才可形成预算。
- 细则 §9 的 manifest、testOnly/production profile、配对样本/噪声界、精确等价、现有 40,000 实体/requirements、16 MiB Packed 与适用 Seek 20 ms 目标分别记录。未测生产数值不填默认值；缺项有明确负责批次和未通过状态。

### 故障与恢复

- Projection 分配失败不撤销已经提交的 Judgement prefix。
- Runtime 投影失败保留既有有效表现状态，并提供明确的 failure stage。
- Renderer submit/present 失败不伪造 Judgement 完成，也不把失败帧重新标记为已确认。
- Pause/Resume、Seek、Reload、Reset、Device lost 和 Close 的旧 scope 不能污染新 scope。
- Close 必须遵守[实时宿主边界 §5](../../../../api/realtime-host-boundary.md)：停止新请求并应用 K1、停止新交付 -> 固定可取得归档并取消候选 -> 停音频且确认 callback 隔离 -> 完成设备 fence/retire 和所属线程清理 -> 排空内部 lease/观察通道并 join worker -> 释放协调器并报告 Closed。主线程在 join 前持续处理其清理任务；任何未确认责任域保持 Closing。外部 CPU owning 结果可跨 Close 存活，其析构不得依赖已关闭的 worker。

## 8. 回退与提交边界

每个批次独立提交，推荐顺序如下：

1. `perf: add gameplay presentation stage counters`
2. `refactor: add judgement publication view`
3. `refactor: index gameplay presentation bindings`
4. `refactor: introduce runtime presentation projection`
5. `refactor: stage runtime world writes and finalize presentation state`
6. `refactor: materialize frames through revision-bound presentation reads`
7. `refactor: separate presentation sources from confirmed delivery bindings`
8. `perf: add demand-driven frame delivery and consumer admission`
9. `refactor: observe typed feedback and delivery attempts in bounded journal`

每个提交必须能单独回退。PD-1 至 PD-4 期间保留旧同步 FrameEnvelope 路径作为故障回退或测试对照；当新路径通过功能、确定性、交付寿命和 Release 基线后，才删除重复路径。共享引用计数、节点池或异步 reclaimer 若被采用，只位于 payload/resource 的实现层：CPU 存储按读者 lease 释放，设备资源按所属 owner 的 retire/fence 释放，不能改变 Gameplay 或 receipt 协议。回退选择在新 session/prepare 边界生效，不在活动事务中热切换两套可变状态。

## 9. 开发、迁移与维护成本

撤下没有计算依据的代码复用百分比。以下是需验证的工程影响范围，不能据此估算工期：

| 范围 | 保留的语义/实现 | 必须改变的边界与主要风险 |
| --- | --- | --- |
| Judgement | Kernel、Fold、Replay、Snapshot | 发布视图与诊断桥；短期借用与 shared/static 链接边界 |
| Runtime/World | 唯一 evaluator、既有层级算法、字段语义 | prepare/apply、组件/storage 准备、派生缓存完整性；本轮最高风险，先独立交付 |
| Playback projection | FactBinding/聚合/lifetime | 索引、候选、统一 finalize；不可提前推进 cursor 或以读取重扣寿命 |
| 物化与同步 SDK | FrameSnapshot、相机/资源错误面 | 中性只读桥、修订 guard、唯一物化器；控制来源不得混进数据版本 |
| 宿主控制/Renderer | 既有设备协议与实际 submit/present | 绑定派生、scope 策略、最后成功画面和一次执行标记；不能只改 worker 而漏掉实际调用端 |
| journal/安装消费者 | 反馈含义、receipt 历史规则 | 显式寿命、公共结构 minor 迁移、有限存储；需要外部 consumer 验证 |

维护目标是每种事实只有一个写入权威：Runtime 提交值、Playback 投影/来源、宿主控制许可、
Renderer 实际执行、存储 lease 和 journal 证据分别有 owner。派生视图不拥有新状态机。
新增属性接入 evaluator/安装/读取/字段对照，不再增加逐属性回滚清单或每消费者失效逻辑。

中性读取首版仍与 World 同 owner，未来跨线程物化明确需要 owning 状态捕获；多 Renderer
明确需要 RPA 参与者重构。两项成本已写入细则 §6.3，不能称为无成本扩展。它们不要求重写
Gameplay 或建立第二套表现规则，但必须重新评估复制、控制延迟、资源峰值和错误传播。

迁移对照分支只保留到功能/故障/容量/Release 门禁通过；之后同步 API 包装共享同一 update/
materialize，不长期维护新旧双引擎。性能不足先定位采样、复制、storage 或协调成本，再按
证据选择缓存或存储变更；不默认增加线程、通用插件或永久双轨。

## 10. 与后续 owner 拆分的关系

多-owner 拆分不是默认目标。判定规则唯一见[细则 §9.3](design.md)：推荐基线的正确性/寿命证据齐备后，在同内容/工具链/Release 配置下，对同一个已接受预算项检查 3 个连续、预声明且噪声有效的窗口。D2、D3、交付等待分别比较，不能拼接不同指标的超界。

版本年龄或积压增长本身只是调查信号，须同时超过该消费者声明的 age/count/bytes 上限。阈值缺失时记录证据不足，不能按一次峰值或单个平台失败自动升级架构。超界报告只触发独立设计，owner 接受与后续实施授权仍分开。

进入多-owner方案时，必须另行提交线程/owner 拓扑、跨线程状态快照、资源生命周期、控制 fence、故障恢复和容量预算设计。不能因为 PD-2 或 PD-3 的局部耗时增加就直接引入固定四线程模型。

## 11. 退出与交接

Stage 7-PD 退出需要：

- PD-0 至 PD-4 的必需实现与逐批证据通过；未实施批次必须记录原因并保持未通过，不能用缺项说明替代退出。
- [细则 §10–12](design.md)中 R1–R8、S1–S6、跨阶段 X1–X6 和正式合同差异逐项可追溯；当前 PD 义务与后续接入条件分开，设计补齐、合同接受与实施验收分别记录。
- 判定、表现、Runtime、Renderer 和 receipt 的前后行为对照结果。
- Debug/Release/目标平台的分段性能数据，以及未执行的 hosted、实体设备和生产容量检查。
- 后续 owner 拆分的触发审查结论：继续推荐基线、暂停、回退，或经 owner 授权进入多-owner方案。
- 4.3.1 数据寿命矩阵、PD-4 delivery/observation 状态机、容量/背压/history unavailable 证据，以及长稳态、冻结帧、恢复、重置、
  丢弃、迟到/重复/旧代回执和最后资源释放的内存有界性证据。
- 准备数据、事务状态、修订读取、交付绑定和按需 FrameSnapshot 的等价性、借用失效与保留重绘证据；重复迁移路径的删除记录。
- 更新 [CURRENT_STATUS.md](../../../../CURRENT_STATUS.md) 和 Stage 7 交接记录；不得把本计划的存在写成 Stage 7A、RPA 或 Stage 7 整体已关闭。

本计划完成后，Stage 7A 继续拥有 Gameplay 语义和判定验收，Stage 7-RPA 继续拥有实时协调和设备验收，Stage 7-PD 只拥有判定到表现的内部架构重构与其证据。

## 12. 跨阶段兼容与交接责任

逐项设计与反例见[细则 §12](design.md)，备选取舍见
[跨阶段设计记录](../../../../stage_reports/stages/stage-07/04-pd/design/2026-10-16-pd-cross-stage-design-review.md)。
本节不增加未来阶段的产品实现授权，不以未来能力已经实现作为 PD 退出条件。

| 后续计划 | PD 当前必须保留的边界 | 后续阶段首次接入时必须完成 |
| --- | --- | --- |
| 7B+ / Stage 8 | 单一 Fact 发布源、能力/绑定支持表、Graph/Packed 同路、正式合同差异清单 | 新 phase/outcome/聚合的明确支持或拒绝；所用 PD 基线的 SDK/宿主/反馈寿命与预算交接 |
| Stage 9 / 11 | typed 表现域读取、单一完整修订与物化协调入口 | Environment、Model/Submesh、形变等真实类型；资源闭包、Snapshot/Digest/capability 版本与上界；不建立动态 provider 框架 |
| Stage 10 | Playback 公共运行入口、候选 prepare/资源事务、精确来源 | 作者、已提交软件预览与画面来源分离；未生效证明才保证旧组合保持，已生效/未知沿控制恢复；多视口按需求单独选型 |
| Stage 12 | CPU 保留、再次呈现资格与设备完成独立；跨代旧图不能直接重绑 | Android 后台/压力、Context 丢失及后端恢复策略和设备证据；无旧图时明确不可重绘，不用 frozen update 补图 |
| Stage 13 | 同步 API 成功只表示完整提交；物化不推进有状态效果 | 先接受显式任务/取消/时钟合同，再做候选内有界重建；目标、进度和完整可交付修订分开；同一目标所有必需域完成后一次提交 |
| Stage 14 | 私有借用不外泄、owning 值与 Close 后 CPU 释放边界 | C opaque handle/allocator/有效期/取消合同、跨 CRT/卸载政策；C/C++ 调用同一语义实现 |

当前首版仍不支持多 Renderer、粒子重建或跨资源代暂停图恢复。新增能力不得借“可扩展”改写
首版保证；未具备正式合同、实现和验收的项目保持不支持或未交付。

RPA-F4 接受前若集成 PD，其最终矩阵/预算直接覆盖该实现；F4 接受后再集成 PD，则在原报告
之外补受影响路径的新 SHA 验证和 owner 接受记录。独立 Stage 7.5 产品集成、Stage 8 发行消费实际基线的
补充交接；不反向要求 PD 等待未来阶段完成，也不允许使用旧 F4 证据关闭新实现门禁。
这些规则只约束受影响的集成与发行，不改变 RPA 受限交接 → Stage 7.5 → Stage 8 的既定产品顺序。

## 13. 实施待办与进度

此清单保留 PD 历史实施结果并补受限退出处置；当前实现状态总入口仍为 [CURRENT_STATUS](../../../../CURRENT_STATUS.md)。
勾选表示相应交付及验证完成，进行中不等于退出。每批记录结果后更新清单并在对话中同步。

- [x] 读取接手、核对分支/HEAD/脏文件，保留既有设计和证据。
- [x] 保存实施前工作区快照，登记初始采样参数。
- [x] **PD-0**——受控环境及经依赖核验的干净重建程序完成两窗口与合并六进程重复性；全部 owner/宿主项满足原定 15% 门槛，同源码阶段/存储/交付观测核验通过，见[退出证据](../../../../stage_reports/stages/stage-07/04-pd/pd-0/2026-10-18-pd0-controlled-environment-and-build-binding.md)。历史 inconclusive 与源码绑定失败组完整保留；历史慢态未归因，生产预算、尾延迟、帧账本回收及设备验收未关闭。
- [x] PD-1——Judgement publication view、读取失效与诊断桥；本地 static/shared、Release、candidate OFF 和适用消费者门禁完成，证据见[PD-1 报告](../../../../stage_reports/stages/stage-07/04-pd/pd-1/2026-10-18-pd1-publication-bridge.md)。不表示跨平台或设备验收完成。
- [x] PD-2——prepare 绑定索引、Projection Adapter、候选状态与既有 golden 对照；static/shared、Release、candidate OFF 及适用消费者本地门禁完成，见[PD-2 报告](../../../../stage_reports/stages/stage-07/04-pd/pd-2/2026-10-18-pd2-projection-adapter.md)。不表示性能收益、完整 World 事务或设备验收完成。
- [x] PD-3-a——Runtime/World prepare/apply、storage 安装、派生状态故障和安装后提帧失败的内部实施及本地适用门禁完成，见[PD-3-a 报告](../../../../stage_reports/stages/stage-07/04-pd/pd-3/a/2026-10-18-pd3a-software-transaction.md)；2026-10-19 资源准备 OOM 门禁已补正，GPT-6-Luna 复审 PASS。
- [x] PD-3-b——状态修订、中性 PresentationRead、唯一物化器与内部 ExactVersion；本地四配置回归、static/shared 包消费者及 GPT-6-Luna 独立复审通过，见 [PD-3-b 报告](../../../../stage_reports/stages/stage-07/04-pd/pd-3/b/2026-10-19-pd3b-state-read-and-materialization.md)。不表示控制绑定、按额度交付或设备验收完成。
- [x] PD-3-c——正式宿主差异、确认绑定与保留画面来源；已形成[来源查询和版本决策方案](../../../../stage_reports/stages/stage-07/04-pd/pd-3/c/2026-10-19-pd3c-contract-and-version-decision.md)，owner 已接受，控制/保留路径已接入，适用本地门禁已完成，新建无上下文 GPT-6-Luna 独立审核通过。
- [x] PD-4——消费注册、按需交付、显式反馈寿命、紧凑 journal 与真实回收；三项合同/SDK 0.9.0 已获接受，内部实施、本地适用门禁与新的无上下文独立复审完成；[实施与验证](../../../../stage_reports/stages/stage-07/04-pd/pd-4/2026-10-19-pd4-delivery-retirement-and-observation.md)。实体设备、正式预算与子阶段退出另行验收。
- [x] PD-5 判定及受限退出——历史结论「未通过但定案」保持；owner 于 2026-11-14 接受 Stage 7-PD 退出及遗留责任交接。后续 RPA 结果和内存／GPU／生产未认证范围见[已接受交接](../../../../stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-14-limited-exit-and-defect-handoff.md)；不将旧失败或未测量项改为通过。
- [x] 最终回归及受限交接——d195 最终 F1/F2 与适用消费者结果已封存，owner 接受准确限制；后继关闭／阶段拆分／日期版本候选独立验证，不外推旧 SHA 或生产保证。

当前快照证据见[PD 设计演进记录](../../../../stage_reports/stages/stage-07/04-pd/design/2026-10-16-design-evolution.md)。

40k Gameplay-host 的隔离定量实施见 [PD-5 准入登记](pd-5/pd5-gameplay-admission.md)：revision 4 的准入预算/合同已获接受，原 40k 功能已核验；这不接受完整 PD-5 内存或正式性能。后续 B0 静态账本已接受、B1 静态工具完成，B2/B3 采数仍未授权，见[读取归因与分域内存计划](pd-5/pd5-read-candidate-and-memory-domains-plan.md)。当前实施入口统一为 `D:/Cuexis-worktree`，准入树只读保留；历史报告及绑定中的绝对路径不改写。
