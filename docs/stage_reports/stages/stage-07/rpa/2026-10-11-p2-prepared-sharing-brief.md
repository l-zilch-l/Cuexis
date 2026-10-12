# P2 判定说明：Prepared 内容与跨域资源准备

状态：historical decision brief；保存早期选项依据，后续已选方向，未实施

日期：2026-10-11（Asia/Shanghai）

本文把 [生产架构方案](2026-10-11-production-architecture-design.md) §7 的「Prepared 共享化（P2）」
展开为可裁定的说明。**只做只读核对与最小范围界定，不改任何代码或合同。**

**后续决定（2026-10-11）：** owner授权剩余普通方案自行收敛；最终选择和测量gate归
[RPA计划](../../../../stage_plans/active/stage-07/plan-rpa.md)及
[RPA补充Spec](../../../../formats/rpa-execution-contract.md)。下文选项为当时比较输入，不再等待逐项点选。

## 0. 结论摘要

| 问题 | 核对结果 |
| --- | --- |
| 表现资源侧是否已可跨域？ | **基本已可**。`PortableResourcePtr` 已是公开 owning 类型，`acquirePresentationResource` 已公开返回它 |
| 音频源侧是否有借用视图？ | 是（`MainMusicSourceView` 含 `span<const std::byte>`），但**现有流程不需要它跨域** |
| P2 是否阻塞 B4？ | **不阻塞**。B4 的最小范围比原判断小得多 |
| 是否需要扩公共 API？ | **不需要**（推荐方案） |

**推荐：P3-a（见 §4）——只把表现候选的校验输入物化为 owning 值，公共 API 零扩张。**
原判断「P2 阻塞 B4」修正为「P3-a 是 B4 的一个内部前置，范围远小于原描述」。

## 1. 只读核对的现行事实

### 1.1 表现资源的 owning 通道已存在

| 事实 | 位置 |
| --- | --- |
| `using PortableResourcePtr = std::shared_ptr<const PortableResource>;` 是**公开** owning 类型 | `presentation.hpp` |
| `PortableResource` 自持全部数据（`std::string`、`std::vector`），不含 provider / manager / GPU 状态 | `presentation.hpp` 头部注释明示 |
| `PreparedPlayback::acquirePresentationResource(const PresentationResourceRef&) const` 是**公开 const**，返回 `core::Result<PortableResourcePtr>` | `playback_session.hpp` |
| OpenGL 已在 `preparePresentation` 中逐条 `acquirePresentationResource(entry.reference)` 并存入候选集 | `open_gl_presentation.cpp` |
| `TestPresentationRenderer::prepare` 走同一条路径 | `presentation_renderer.cpp` |

**含义：** 渲染器获取资源的接口形状**已经是 owning 的**。它今天的问题不是"拿到的是借用视图"，
而是**调用必须发生在 PreparedPlayback 的 owner 线程上**（`presentationCandidateToken()` 与
`preparePresentation` 的线程检查）。

### 1.2 表现侧残留的三项耦合

渲染器 `prepare` 除了资源，还消费 PreparedPlayback 的另外三项：

| 耦合项 | 形态 | 为什么不能直接搬走 |
| --- | --- | --- |
| `presentationManifest()` | 返回**指针**（`const PresentationResourceManifest*`） | 指向 Prepared 内部存储，生命周期绑定对象 |
| `validatePresentation(capabilities, request)` | 需要 Prepared 内部的 manifest 与表现配置 | 校验逻辑在 Prepared 内 |
| `presentationCandidateToken()` | owner token + candidate generation | 语义上必须绑定创建它的 Session |
| `contentInfo()` | 值类型（`PlaybackContentInfo`） | 已可拷贝，非阻塞项 |

### 1.3 音频侧的借用视图，与它为什么不是阻塞项

| 事实 | 位置 |
| --- | --- |
| `struct MainMusicSourceView { std::string_view assetId; double timingOffsetMs; std::uint64_t contentRevision; std::span<const std::byte> bytes; }` —— **借用视图**，在**已安装**的 `playback_session.hpp` 中 | `playback_session.hpp` |
| `PreparedPlayback::mainMusicSource()` 返回 `std::optional<MainMusicSourceView>` | 同上 |
| Player 的用法：取 `source->bytes` → 立即 `WavDecoder::decode` → `store.registerClip(...)` → 把 `AudioClipHandle` 交给设备 | `player_assembly.cpp` `preparePlayerAudioClip` |

**关键点：音频内容的跨域形式已经存在且已是 owning 的。**
解码发生在事务的 prepare 阶段（主线程），产物是 `audio::AudioClipHandle` —— 一个对
`AudioClipStore` 的 owning 句柄。`MainMusicSourceView` 的借用只在解码那一刻有效。
因此当前同owner路径可以立即decode；P-B迁移后必须先在Prepared owner物化owning数据，
再交audio owner，不得把MainMusicSourceView借用跨域。具体规则已归宿主边界§3。

### 1.4 底层 lease 的实际寿命语义

| 事实 | 位置 |
| --- | --- |
| `ResourceLease<Tag>` **不可拷贝、只可移动**（copy ctor / copy assign 已 delete） | `resource_manager.hpp` |
| 数据由 `std::shared_ptr<const Resource>` 保活，`CpuResource` 内含 `std::shared_ptr<const AssetBlob> blob` | `resource_manager.hpp` |
| control 只持 `std::weak_ptr<ResourceManagerState>` | `resource_manager.cpp` |
| `ResourceManager::load` 有 owner-thread 检查（`assets.resource.not_owner_thread`） | `resource_manager.cpp` |

**含义：** 底层数据**已经自足**（shared_ptr 保活，manager 死后 blob 仍活）。真正的限制是
两条：lease 不可拷贝（不能"一份留 owner、一份给 worker"），以及 `load` 绑定 owner 线程。
这两条都不需要改公共 API 来解决——见 §4。

## 2. 问题的最小真实形态

把上面拼起来，跨域资源准备的**唯一**真实障碍是：

> 渲染器 owner 要构造自己的 GPU 候选，但它需要三项信息（manifest、校验结果、候选凭据），
> 而这三项今天只能通过「在 PreparedPlayback 的 owner 线程上调用其方法」获得。

资源数据本身已经不是问题。所以 P2 的范围是**把这三项输入物化为可移动的值**，
不是"把 PreparedPlayback 变成可共享对象"。

## 3. 明确排除（防止范围蔓延）

以下**不在** P2 范围，且不应被本判定顺手带进来：

| 排除项 | 理由 |
| --- | --- |
| 把 `PreparedPlayback` 整体变成可跨线程共享/可拷贝 | 会破坏现行 owner 契约与 `std::terminate` 保证；非必要 |
| 让 `ResourceManager` 变成线程安全或可在任意线程 `load` | 现行 owner-thread 检查是声明的契约，不是缺陷 |
| 把 `ResourceLease` 变成可拷贝 | 会让"同一资源两份所有权"变得可能，掩盖真实依赖；无必要 |
| 改 `MainMusicSourceView` 的公共形态 | §1.3 证明无必要；改它是公共 API 变更 |
| 引入跨线程 Prepared 公共 API | 与 ADR 0046 保留 SDK 单 owner 的方向冲突 |
| 让 renderer 直接访问 ContentProvider 或 AssetDatabase | 会把内容域知识泄漏进设备适配器 |

**特别注明：** §1.3 的结论意味着**现有同owner立即decode路径不要求公共共享Prepared；迁移audio owner仍须闭合owning解码数据、ClipStore和callback寿命**。原方案 §7 把「P2 阻塞 B4」
写成一条整体依赖，现在修正为：P3-a 只覆盖表现候选输入。

## 4. 三个可裁定方案

| 方案 | 内容 | 公共 API 影响 | 代价 |
| --- | --- | --- | --- |
| P3-a **推荐** | Session owner（P-B为Playback worker）把 manifest、校验结果、候选凭据物化为一个**内部** owning 值；渲染器 owner 消费该值，不再调用 PreparedPlayback 方法。资源仍走既有 `acquirePresentationResource` 形态 | **无**（内部 `src/` 值类型） | 需定义内部候选值与其失效规则；`validatePresentation` 需要 renderer capabilities，存在一次主线程→worker 的输入传递 |
| P3-b | 在 P3-a 基础上额外新增**公共**的 owning prepared 句柄，使外部宿主也能独立准备 | 新增公共类型与方法 | 公共面扩张、安装兼容、版本判定；需 owner API approval |
| P3-c | 维持现状：资源准备继续在 PreparedPlayback 的 owner 线程执行，B4 只拆「执行 vs 表现」的状态与接口，不拆资源准备 | 无 | 资源准备仍在主线程；若它成为主线程卡顿源，B5 会暴露 |

**推荐 P3-a**，理由：

1. **它够用。** §1.1 证明资源侧已是 owning 形态，缺口只在三项输入的物化。
2. **它不动公共面。** 在"公共差异决定 patch/minor"的口径下，P3-a 本身不增加公共共享Prepared；RPA其它公共变化仍触发版本与安装兼容审查，
   而 P3-b 会——为一个尚无外部消费者证明的需求付这个代价不划算。
3. **它可回退。** 内部值类型的形态可以重写而不影响任何已发布契约。
4. **P3-c 是真实备选**，不是稻草人：如果 B1 测量显示资源准备不是主线程卡顿的主要来源，
   那么拆它的收益就不明确，维持现状是对的。**这一点应由测量决定，不由设计偏好决定。**

## 5. P3-a 的边界条件（若被选中）

1. **内部值不跨 Session。** 其身份须含 Session token 与 candidate generation，
   使旧值在新事务后**稳定失效**，不得静默用于新组合。
2. **不搬运借用视图。** 内部值只能持 owning 数据（manifest 值、资源句柄、凭据）。
   不得把指向 Prepared 内部存储的指针或 span 塞进去。
3. **capabilities 的方向。** renderer capabilities 来自 GL 上下文（主线程），
   而校验结果被 worker 消费。须明确：capabilities 由主线程**读取并随请求传入**，
   而不是让 worker 去查询设备。
4. **提交仍由 fence 保护**（方案 AD-11）：主线程 renderer 完成的 GPU 候选不得在普通推进使其失效后
   仍被提交。
5. **GPU 资源回收仍属渲染器 owner**（主线程）。P3-a 不改变这条。
6. **`validatePresentation` 的归属须裁定**：它在主线程做（校验只是对 manifest 与 capability
   的比较），还是随候选值下移到 worker。这决定 P3-a 是"传 manifest"还是"传校验结果"。
   后续采用**在Prepared owner（Playback worker）做校验**；主线程提供owning capabilities，
   校验与acquire不跨Prepared owner，GPU创建/激活/销毁留主线程。
7. **不新增测试之外的生产默认值**；容量与上限保持待定。

## 6. 本裁定的准入效果

| 若选 | 对 B4 的影响 |
| --- | --- |
| P3-a | B4 可开工；资源准备拆为「Playback worker 校验/acquire + 主线程 GPU 候选构造」两段 |
| P3-b | B4 可开工，但先需公共 API 审批、版本判定与安装消费者证据 |
| P3-c | B4 可开工且范围更小；资源准备保留在主线程，B5 若暴露卡顿再回到本裁定 |

**三种选择都不阻塞 B4**，只是 B4 的范围与后续风险不同。这是本次核对修正的最重要一条：
原方案把 P2 写成 B4 的硬阻塞，实际不是。

## 7. 未执行项

本轮只做只读核对与文档，未修改任何源码、头文件或合同；未运行 C++ 构建/CTest、
未做跨线程实验、未测量资源准备耗时。文中所有"已可/已存在"的判断来自源码阅读，
不是运行验证；`validatePresentation` 的成本与 `acquirePresentationResource` 的拷贝成本
均**未测量**，属 B1 范围。
