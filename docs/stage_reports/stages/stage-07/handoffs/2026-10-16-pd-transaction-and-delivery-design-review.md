# Stage 7-PD 事务、交付与演进成本重审

状态：dated design review；设计选择尚未实施

日期：2026-10-16

本记录比较开发、维护、性能和扩展方向，解释对 [PD 主计划](../../../../stage_plans/active/stage-07/plan-presentation-decoupling.md)
及[设计细则](../../../../stage_plans/active/stage-07/presentation-decoupling-design.md)的修订。
代码依据为 `stage-7` / `d076240a3f66fae9193195c2343afb1aedf94926`；计划依据含该提交上的未提交文档。
本轮为文档设计推敲与源码核对，没有启动产品代码、性能实验或新的代理模拟实施。
当前实施状态仍由 [CURRENT_STATUS](../../../../CURRENT_STATUS.md)拥有。

## 1. 输入与评价次序

此前按用户指定的 `gpt-6.1-sol`、high、无上下文子代理独立审查，得到四个需要补齐的接入问题：
现有回滚不足以恢复新增失败窗口后的派生矩阵；Pause 后缺少合法结果绑定；UntilReset 被整数
expiry 混同；多消费者不能直接接入现有单一 Renderer context。本记录不是这些问题的运行复现。

评价先检查状态权威、协调/恢复和日常改动成本，再比较单帧/冷路径开销及未来扩展范围。
不能只因为一个设计能修复这四项，就认为它值得长期保留。也不能把没有测量的性能收益或
代码复用比例作为定论。

## 2. 状态与事务方向比较

| 方向 | 开发与维护 | 性能/容量代价 | 未来扩展 | 本轮选择 |
| --- | --- | --- | --- | --- |
| 活动 World 暂写，补齐全量撤销 | 前期改动较少；每种新组件、派生缓存都要增加恢复项，失败恢复本身成为维护对象 | 记录旧值、恢复写入和峰值随事务扩大 | 新属性/拓扑令恢复清单持续增长 | 不作为新增失败窗口的解决方向 |
| 整 World 双缓冲/克隆后交换 | 原子发布直观；需处理 registry、对象身份、回调和非复制对象，改造面大 | 全量复制和双份存储，不能只按 numeric 值估算 | 隔离能力强，但把专门问题升级为通用 World 重构 | 当前不选 |
| 每更新生成 owning 中性结果 | 跨 owner 读取清晰；World、状态与结果需同步安装，新增捕获错误面 | 每次更新都扫描/复制，即使没有消费者；完整输出和候选空间常驻 | 多线程物化较方便，仍须版本保留/背压 | 从首版必需路径改为有触发条件的后续选择 |
| 持久化/COW 多版本状态 | 可支持历史读；块身份、共享、写屏障、回收与版本失效均需设计 | 少量变化可能节省复制；写放大、元数据和最慢读者存储成本需测 | 历史/并发读较强；当前没有这样的首版交付需求 | 当前不选，不预留公共版本存储协议 |
| 当前已提交状态 + 修订描述 + 按需中性读取 | 一份属性权威；明确提交与交付失败面；需提取读取桥并验证原事务完整性 | 无消费者不捕获完整结果；有请求才遍历，多个请求可能重复读取 | 可替换内部存储；跨线程仍需新增 owning 捕获，成本明确 | 作为首版选择 |

选定方向不是返回旧同步 SessionSample 链。逻辑更新、交付 admission、物化与观察已经分开；
Judgement 只读视图和投影索引继续独立，Renderer 不读取 World，消费者积压不决定 Gameplay
推进。首版保留同 owner 是为了避免为尚未要求的并行物化支付版本同步成本。

也不把“读取按需”当成 Runtime 原子性已经解决。源码中的 `updatePrepared` 先提交局部值，
再调用 `updateWorldTransforms`；后者维护派生矩阵、拓扑缓存、scratch 和组件。新方向将
完整结果捕获放到事务外，同时要求 PD-3-a 把原 transform 算法拆成 prepare/apply：准备全部
可能失败的存储和验证，然后安装原值、派生值与缓存。具体边界见设计细则 §5。

选择这种局部提取，是因为能保留唯一求值算法，又不让每个消费者或每种新属性维护独立回滚。
其风险是 storage 安装原语与临时空间成本，必须单独验证；不能仅写 noexcept 就宣告完成。

## 3. 内容来源与控制身份比较

| 方向 | 主要问题 | 选择 |
| --- | --- | --- |
| 每次控制都重新发布完整结果并换表现作用域 | 重建未变内容；暂停可能重复反馈；若重新求值则可能消耗 lifetime | 不选 |
| 允许旧 envelope 直接跨控制提交 | 不能证明新设备/资源许可；可能使旧帧绕过 fence | 不选 |
| 把全部 HostContext 拆成新公共多端协议 | 能表达多设备，但当前就要重构控制参与者、资源和所有消费者 | 作为多 Renderer 前置，不在 PD 首版强行展开 |
| 内容来源保持不变，从已确认控制派生新交付绑定 | 需要明确准入矩阵；可以复用旧图而不伪造新来源，继续完整 HostContext 校验 | 选择 |

因此，状态修订回答“这幅画面由哪次操作产生”，projectionScope 回答“这些激活是否属于同一段
表现身份”，DeliveryBinding 回答“现在允许交给哪个已确认呈现端”。三个问题不能共用一个版本号。
Pause/Resume/仅换段 EOF 不重建表现身份；Seek/Reset/内容或表现绑定替换才重建。该策略不同于
当前 Player `finish()` 每次递增 projectionScope，已列为正式宿主合同差异，不能静默改变。

保留画面也不能强制等于当前状态。例如当前状态为 v12，最后成功呈现为 v10；暂停后保持的是
v10。若 viewport 改变，则用 v10 Snapshot 中的完整值和相机参数重建，仅修改投影，不用 v12
冒充旧图，也不回放 Gameplay。原始 T/frame/cursor/membership 必须保留。

Renderer 的最后成功 backing 和实际调用 high-water 位于设备 owner，独立于可丢 receipt。
因此 journal 淘汰不会导致重新 present；丢失观察回执也不会让 Pause 不知道本地最后成功画面。

## 4. 扩展范围与兼容方向比较

**Renderer 扩展。** 只增加 ConsumerId 无法绕过现有唯一 confirmedHostContext。首版明确一个
实际呈现端和多个 CPU capture 消费者；内部绑定入口不把设备身份写入求值。未来第二 Renderer
需要会话控制与每端资源绑定、固定参与者集合、加入/退出和 Close 规则；不能宣称零重构接入。
这些改动集中于 RPA/设备协调，不要求新建 Gameplay 或表现求值语义。

**UntilReset。** 继续整数 sentinel 会丢失合法极值的区别；只在内部修复而向公共查询继续返回
sentinel 会形成两套含义；永久新增 V2 查询又增加维护面。选择在经批准的新 SDK minor 中统一
为显式 optional end，完成 installed consumer 迁移。旧构建作为历史基线保存，不与新布局混用。
本轮只记录选择和门禁，不修改头文件、版本值或声称公共迁移完成。

**共享与回收。** 仍允许不可变准备数据、owning 帧 backing 和资源引用采用共享计数；物理释放
可以异步，但 backlog 计入容量。没有理由让读者最后一次释放参与 Gameplay/控制状态提交。
首版不引入通用 reclaimer 框架，也不把多线程当成内存或复制问题的默认答案。

## 5. 场景反查与代价

以下为方案约束检查，未执行实现模拟或测试：

| 场景 | 设计必须给出的结果 | 仍需后续证明的代价 |
| --- | --- | --- |
| 无窗口持续更新 | 只有必要事务和描述；不构建完整结果或 Snapshot | 描述/诊断、候选销毁和 World 安装成本 |
| 每更新一帧 | 直接中性读取并填 Snapshot，无完整中间数组 | 桥接调用/索引不导致不可接受回归 |
| 父子变换准备失败 | 原局部值、world matrix 和缓存一致保留 | 私有 storage 的无失败安装及峰值 |
| 更新成功、构帧分配失败 | 已提交更新和 lifetime 不撤销，交付明确失败 | 额度归还不依赖 receipt |
| Pause drain 新 Fact，当前状态领先保留画面 | 不激活新 Fact；重绘原保留来源，控制绑定更新 | 旧图与新 viewport Snapshot 同时存活的峰值 |
| Pause 后旧 receipt 迟到 | 只补原 attempt/义务证据，不确认新控制 | 当前/历史键隔离、冻结结论保持 |
| UntilReset 与有限最大 Tick | 两者在 T 最大值有不同且可表示的结论 | 公共布局/包版本迁移 |
| 第二个独立 Renderer 注册 | 明确不支持，不隐式复用第一设备许可 | 未来 RPA 多参与者改动范围已登记 |
| 长期回执丢失、journal 淘汰、Close | 观察可缺失；CPU/GPU 独立回收，外部 owning 值可存活 | 执行去重、账本有界和各 owner 清理 |

这轮选择牺牲首版跨 owner 物化和历史状态读取的便利，减少每更新完整结果复制和第二份状态
同步。多消费者同修订的重复扫描是已知限制；若出现真实负载，再用有界缓存或 owning 结果
解决，先测局部成本，不预先实现多版本引擎。性能测量不只看平均 FPS，还覆盖 p99、控制等待、
回收/析构时间、临时空间和同 U/F 条件下的更新与交付成本。

## 6. 迁移与记录处置

PD-3 顺序调整为事务完整性 → 修订/只读物化 → 控制绑定；PD-4 接消费者、显式寿命和 journal。
正式合同差异集中在设计细则 §11；布局变更先完成 Spec/ABI/版本接受，旧完整帧路径只作为
受限迁移对照，门禁通过后删除。主计划不再用没有依据的代码复用百分比代替影响分析。

本记录细化并替换[上一份架构记录](2026-10-15-pd-state-result-architecture-selection.md)中的
“每次更新捕获完整当前结果”首版选择，保留准备数据、单 owner、按需交付和独立观察的职责。
它不证明本方案已通过编译、故障注入、设备/性能或长稳态验收，也不关闭 Stage 7-PD/RPA。

## 7. 源码核对入口

- [RuntimeSession](../../../../../engine/runtime/src/runtime_session.cpp)：`updateGameplay`、`updatePrepared`、camera/appearance commit/rollback。
- [World transform](../../../../../engine/world/src/transform_system.cpp)：`updateWorldTransforms` 的拓扑、派生矩阵、缓存及组件安装。
- [PropertyResolver](../../../../../engine/world/src/property.cpp)：`rollback` 的实际恢复范围。
- [PlaybackSession](../../../../../engine/playback/src/playback_session.cpp)：`extractFrame`、`validateExecutionFrameContext`、`gameplayFeedback` 与控制确认。
- [Player ExecutionHost](../../../../../app/player/src/execution_host.cpp)：`finish`、sample 和 present 的上下文/作用域归属。
- [Frame observation](../../../../../app/execution_host/frame_observation.hpp)：FrameEnvelope、实际 present 桥和历史载荷耦合。

## 8. 本轮文档验证

`python -B tools/check_docs.py` 通过：449 个 Markdown、20 个 candidate JSON/CXT；
`git diff --check` 通过，仅有既有 LF/CRLF 转换提示。主计划、细则、选型记录和候选合同还核对了
UTF-8、单 H1、代码围栏及尾空白。检查只证明文档结构和链接，不证明事务实现或性能。
本轮未修改产品代码、公共头、版本值或 CURRENT_STATUS 的实施结论，未提交 Git commit。

后续同日补齐的 7B+/Stage 8–14 兼容、候选重建和保留资格撤销见
[跨阶段设计记录](2026-10-16-pd-cross-stage-design-review.md)；其文档验证独立记录，
不改写本节的历史检查范围或证明新增设计已经实施。
