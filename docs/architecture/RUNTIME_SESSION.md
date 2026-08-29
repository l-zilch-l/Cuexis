# Cuexis RuntimeSession 规范

状态：阶段 1C/1D 会话合同、阶段 2 Timing/Behavior/表现属性和阶段 3C portable Snapshot 均已实现

更新日期：2026-08-07

## 职责与所有权

一次 RuntimeSession 对应一次播放、Studio 预览或 headless SDK 会话的内部 Runtime 实例。它不是宿主直接集成的公共门面；ADR 0027 规定宿主、Player 和 Studio 统一通过 PlaybackSession 使用它。

Session 拥有：

```text
World / EnTT Registry
不可变 ChartRuntime
ChartObjectId -> entt::entity 映射
ResourceScope
PropertyResolver 与初始属性状态
每 Session 的 System 管线状态
最后处理的 timeDiscontinuityId
结构化 Diagnostics
可选且有界的 Runtime 属性调试快照
```

Session 不拥有 ChartDocument、EditorDocument、ResourceManager、AssetDatabase、ContentProvider、AudioTransport、Renderer、RenderBackend、窗口或输入设备。这些依赖由 PlaybackSession 组合层及其宿主/应用适配器持有，并按显式生命周期比内部 RuntimeSession 活得更久。

## PlaybackSession 公共门面

PlaybackSession 负责把宿主输入组织成单一播放会话，并隐藏 RuntimeSession、World 和 EnTT：

```text
typed/memory Project 或 Chart source
ContentProvider 与资源预算
RuntimeFrame 与标准化 InputEvent
RuntimeSession prepare/update/reload/unload
extractResult()：从 Session 开始累积的判定事件、分数、连击和统计快照
extractFrame()：后端无关的拥有型 FrameSnapshot 提取
startRecording()：按 chartTimeMs 记录全部 InputEvent
stopRecording()：停止记录并返回可序列化 ReplayData
loadReplay()：注入预记录 InputEvent 替代实时输入
阶段 11 的 JudgementResult 与 ReplayData
结构化 Diagnostics
```

PlaybackSession 不创建宿主窗口、不拥有宿主主循环，也不要求 SDL/OpenGL。当前 C++20 门面已提供 `loadChart(string_view)`、`update(RuntimeFrame)`、`extractFrame(FrameViewport)`、显式目标帧 `reload` 和 `unload`。`FrameSnapshot` 拥有对象 ID、World Matrix、Camera View/Projection、Visibility、Material asset/opacity/tint、Mesh/Material portable ref 和视口数据；Reload/Unload 不使已返回 Snapshot 悬空。`findEntity(entt::entity)`、`withWorld`、Runtime 调试快照和 Registry 访问只能作为内部或受限调试接口，不能进入安装后的 SDK 公共头。

ADR 0037 的 Stage 3A 合同及 3B/3C 实现保持 `FrameSnapshot` 为唯一公共权威帧。
`PreparedPlayback` 和活动 Session 提供 owning manifest 与按 ref 获取 owning immutable portable
resource 的接口；ref 只包含 Cuexis-owned type、AssetId 和 semantic identity。共同表现提取从
Snapshot 与 portable resources 派生 effective color/alpha、Pass、cull、depth state 和 canonical
sort record，但这些 adapter 内部记录不能成为第二套 Playback Runtime 输出。

Stage 2 的 Playback capability set version 1 使用
`cuexis.chart.v3`、`cuexis.behavior.event.v1`、`cuexis.render.visibility.v1` 和
`cuexis.material.snapshot.v1`。默认 Session 提供全部能力；显式 capability 集合在 prepare 的
资源请求和 World 发布前校验。缺少能力产生确定排序的 `playback.capability.unsupported`，不在
运行中静默跳过属性，也不复用 Chart `requiredExtensions`。

当前默认 FrameDigest algorithm version 3 在 version 2 的 Visibility、Material asset ID、opacity、
tint 等字段上，增加 Mesh/Material ref presence、type、AssetId 和 32-byte semantic identity，并继续
使用稳定 little-endian FNV-1a 64 编码。version 1/2 是保留不变的历史算法定义；比较历史 golden
时必须显式选择对应内部兼容路径，当前 public `computeFrameDigest()` 返回
`algorithmVersion == 3`。

阶段 1D 已增加 move-only、owner-thread 的 Prepared Playback load/reload。Prepared 对象内部持有
候选 Runtime、AudioSourceLease 和 optional Prepared Presentation；公开 `PlaybackContentInfo`、调用期
有效的 `MainMusicSourceView`、candidate token、owning manifest 与 owning resource acquisition。
宿主和 adapter 不得看到 ResourceManager slot、Handle 或 Lease。
确定性准备全部成功并完成音频激活后，Playback commit 只执行无分配、不可失败的状态交换。
`cuexis_playback` 可以依赖后端无关的 `cuexis_audio` 以提供 RuntimeTimeline，但仍不得依赖
`cuexis_audio_sdl` 或 SDL。

阶段 1B 的 `RuntimeSession` 可显式注入一个不可移动的外部 `ResourceManager`。无资源 Chart 仍可使用默认构造；含 Renderable 的 Chart 在没有 Manager 时稳定返回 `runtime.chart.renderable_resources_unsupported`。PlaybackSession 在持有 AssetDatabase/ContentProvider 时内部拥有 ResourceManager，并保证 RuntimeSession/World/Scope 先于 Manager 销毁；无资源 1C fixture 与 Stage 1B Renderable Project 都走同一 Playback 门面。

`withWorld()` 是 Runtime 内部/受限调试 callback 边界，只能在 Session owner thread 同步调用。
callback 不得重入同一 Session，不得在返回后保留 World/Registry 引用或指针；阻塞 callback 会
阻塞 owner thread。返回值始终为单层 `Result`，非 OOM 异常稳定转换为
`runtime.session.callback_exception`。World 的 Registry callback 遵守同一规则，并转换为
`world.callback.exception`。

## 状态

```text
Empty -> Preparing -> Ready -> Running
Ready/Running -> Reloading -> 原活动状态
任意活动状态 -> Closing -> Empty
运行期不可恢复错误 -> Failed
```

首次准备失败不发布半初始化 Session，只返回 Diagnostics。
Playback 的 SessionState 不包含 Paused；暂停属于 SourceClockSample/AudioTransport，并通过
`simulationDeltaTimeMs = 0` 的 RuntimeFrame 表达。

## PreparedRuntimeSession

创建是事务：

```text
标准 ChartDocument 校验与编译
-> 无副作用检查 ChartRuntime 排序、parent 和 Behavior 引用
-> 生成不可变 ChartRuntime 和资源依赖清单
-> 临时 ResourceScope 获取资源
-> 临时 World 实例化
-> 建立层级、初始属性和对象映射
-> 检查 World 不变量
-> 主线程帧安全点 commit
```

失败时按以下顺序清理：

```text
停止临时 System
-> 销毁临时 World
-> 释放临时 ResourceScope
-> 返回 Diagnostics
```

World 必须在 Scope 前销毁，确保 Component 清理期间资源 Handle 仍受 Lease 保护。

Prepared 数据同时绑定创建它的 Session token 和 ResourceManager token。跨 Session、跨 Manager、已经消费或 Session 地址复用后的提交均失败。Handle 除 index/generation 外还必须属于当前 Manager；Runtime 不接受只通过 `valid()` 但来源不同的 Handle。

阶段 1B 的资源准备采用同步主线程路径。Mesh/Material 使用 Fallback 策略，Scope 对直接引用和
Material 等资源的传递依赖闭包去重；Component 只保存 Handle。准备诊断固定最多 1024 条，成功
commit 后成为 Session 的活动 Diagnostics。

该 Fallback 行为是内部 opaque 资源合同。Stage 3 Portable Presentation v1 不把现有 built-in
fallback blob 解释为 Mesh/Material/Texture2D；候选 presentation 闭包必须严格成功，并在
Playback commit 前拒绝 fallback。通用 ResourceManager 策略与阶段 1B 回归保持不变。

## Update 与时间不连续

```cpp
struct RuntimeFrame {
    double chartTimeMs;
    double simulationDeltaTimeMs;
    std::uint64_t timeDiscontinuityId;
};
```

ID 未变化时执行普通帧更新。ID 改变时，Behavior 绝对重采样，PropertyResolver 从初始值重算，状态型 System 接收 `onTimeDiscontinuity`。任何 System 不得继续累计跳转前的 delta。

暂停、Stopped 或时间不连续后的首帧由 RuntimeTimeline 传入
`simulationDeltaTimeMs = 0`。Session 不自行控制 AudioTransport。ChartClock、HostClock 与
CuexisAudio 都必须先归一化为 SourceClockSample，再由同一 RuntimeTimeline 生成
RuntimeFrame。`chartTimeMs` 必须有限（允许为负），delta 必须有限且非负；同一
discontinuity ID 下的向后时间移动拒绝，ID 变化后从目标绝对时间重采样而不消费跳转前 delta。

Stage 2 的 Runtime 每帧只执行一次 `chartTimeMs -> BeatSample`，所有 Behavior 和对象复用同一
Beat、`inStop` 与 `stopProgress`。更新顺序固定为 Behavior evaluate -> PropertyWriteBuffer ->
Transform/FOV/Visibility/Material resolver -> World transform update。Resolver 每帧从 prepare
时捕获的初始值重建稀疏属性；所有候选必须全部校验通过后才一次提交，不发布半帧结果。
连续 Event、Step Event、Stop、Seek、Reload 和负 Beat 都按目标绝对时间重采样，不依赖帧率
或 EnTT 遍历顺序。对于不包含 Animation 的会话，预热后的 `PlaybackSession::update()` 与复用
destination 的 `extractFrame()` 不分配；包含 Animation 的会话遵循 Stage 4 已定义的有界分配
合同，本节不作全路径零分配承诺。

## Stage 2 调试快照

内部 `RuntimeSession::configureDebug()` 显式启用固定容量调试记录，容量上限为 65536。每条
`RuntimeDebugRecord` 保存 ChartObjectId、Property、初始基准、命中事件索引、归一化进度、
Behavior 输出和最终解析值。`debugSnapshot()` 返回拥有型副本；容量耗尽时设置 `truncated`。

调试关闭时不记录也不产生额外帧分配。该接口属于内部 Runtime/Studio 诊断边界，不进入
Playback `FrameSnapshot`，也不暴露内部指针。宿主只通过 capability、结构化 Diagnostics 与
最终 FrameSnapshot 观察结果。

## Reload

Chart Reload v1 使用完整替换，不进行 Entity 增量修补：

```text
旧 Session 继续运行
-> 准备 Replacement
-> 成功后在帧安全点交换
-> Renderer 放弃旧提取数据
-> 销毁旧 Session
```

准备失败保留旧 World、Scope 和活动 Diagnostics，并单独返回本次失败诊断。成功 Replacement 一次发布新 World、Scope 和 Diagnostics，再按 `World -> Scope` 顺序释放旧状态。

调用方必须显式选择：

```text
KeepChartTime
RestartAtZero
```

Reload 后旧 `entt::entity` 全部失效。跨 Reload 定位只使用 ChartObjectId。

ResourceManager 的内容热重载保持 Handle，不要求重建 Session；只有 Chart/Component 结构变化才走 Replacement。

阶段 1B 只保留 `contentRevision` 和 Reloading 状态扩展点，尚未实现文件监听或资源内容热重载 API。

## Unload

Unload 仅在主线程帧安全点执行：

```text
停止 update
-> 停止提取新 RenderScene
-> 使指向内部 World、Registry、Entity 或 RenderScene 的借用视图失效
-> 销毁 World
-> 释放 ResourceScope
-> 清空映射与诊断
-> Empty
```

GPU 对象延迟销毁由 RenderBackend 管理，Session 不直接等待或调用图形 API。

公共 `FrameSnapshot` 是拥有型值对象；成功返回后，其对象 ID、矩阵、相机和视口数据不再
借用 Session 内存。后续 `update`、`reload`、`unload` 或 Session 销毁不得使已有 Snapshot
悬空。调用方主动复用 destination Snapshot 时，只有被本次调用覆盖的同一个对象发生变化。

## 错误等级

```text
Warning：Fallback、缺失 Optional、未知可选扩展；继续运行
Recoverable：资源热重载或 Replacement 失败；保留上一有效状态
Fatal：World 不变量破坏或 Required 资源不可恢复失效；停止更新并进入 Failed
```

Failed 可以保留最后一份 RenderScene 供错误 UI 使用，但不能继续改变 World。

## 线程

阶段 1 的 prepare、commit、swap 和 destroy 全部在 Session owner thread 同步执行。独立 Player/Studio 通常把它绑定到应用主线程；嵌入宿主可以为不同 Session 指定不同 owner thread，但同一 Session 不提供隐式并发访问。未来 Worker 只能生成 Prepared 数据；最终 World 发布和销毁仍在帧安全点，不为 Session 替换引入细粒度锁。
