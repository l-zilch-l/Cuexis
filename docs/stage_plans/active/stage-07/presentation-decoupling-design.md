# Stage 7-PD 内部设计与验收细则

状态：proposal
更新日期：2026-10-16（事务、交付、注册生命周期与跨阶段兼容设计）

本文件是 [PD 主计划](plan-presentation-decoupling.md)的设计附件，补齐接口、状态、所有权、
失败边界和验收规则。它不表示接口已存在、产品已实现、生产预算已接受或批次获准启动。
本轮只修订文档；不得据此启动产品实现、计时测量或新的模拟实施。此前独立审查发现的问题作为
设计输入，不是实现失败的实测结果。备选方向、取舍与演进成本见
[本轮设计重审记录](../../../stage_reports/stages/stage-07/handoffs/2026-10-16-pd-transaction-and-delivery-design-review.md)。

## 1. 权威、术语与范围

既有语义继续由以下文档唯一拥有；本附件只规定 PD 如何消费，不另造一份 Gameplay/RPA 合同：

| 内容 | 权威来源 | PD 的责任 |
| --- | --- | --- |
| Fact、聚合、去重、冲突、missing target | [Gameplay Spec](../../../formats/GAMEPLAY_V2_SPEC.md) §3.23、§9.7 及“2026-10-08 FactBinding 聚合首次消费表示”；[ABI](../../../api/GAMEPLAY_V2_ABI.md) C78-07/09 | 提取索引和适配器，保持既有判定与表现语义 |
| 组合事务、错误、恢复、无 Fold 路径 | [Playback lifecycle](../../../api/playback-session.md) 的 S7A-7/8 组合事务；ABI 的 R78-01 失败回执 | 不把 sealed 前缀当成功 Fold；不重新准入已接纳输入 |
| 时钟、G1/Q1/K1、控制 cut、R2、FaultPrefix | [RPA 补充合同](../../../formats/rpa-execution-contract.md) | 继续使用既有许可与控制协议 |
| Prepared、资源、feedback、Close | [实时宿主边界](../../../api/realtime-host-boundary.md) §2–5 | CPU 交付与观察拆分；不改设备完成证据 |
| 实施现状 | [CURRENT_STATUS](../../../CURRENT_STATUS.md) | 不用本附件或历史交接覆盖当前状态 |

设计名称与最小含义如下；这些不是新的公共 SDK 类型：

| 名称 | 唯一含义与所有者 |
| --- | --- |
| PublicationRead | 同一 owner 操作内取得的 Judgement 只读发布视图；不是公共 owning GameplayResult |
| ProjectionCandidate | adapter 尚未安装的 cursor、活动效果、future queue、聚合与诊断变更 |
| PreparedPresentationData | 不可变定义、稳定对象索引、资源描述与闭包；不含 GPU 句柄或本局可变状态 |
| CommittedPresentationState | 当前表现状态的提交描述：修订号、内容/布局、已实际消费的前缀、T/frame 和激活来源；不再强制另存完整属性数组 |
| PresentationRead | Runtime 对当前已提交值的同 owner 只读视图；通过中性字段读取，不能带出 World/EnTT 或跨操作保存 |
| resultVersion | 同一 sessionScope 内单调、checked u64 的状态修订号；首次内容提交即建立，不是 Runtime 更新次数或缓存次数 |
| DeliveryBinding | 已确认控制/资源证明与当前消费注册共同约束的交付许可；不属于求值结果，注册确认不能代替控制/fence |
| RetainedFrame | 呈现端最后成功画面的 owning CPU backing 及原始状态身份；可比当前 resultVersion 更旧 |
| ConsumerId / RequestId | ConsumerId 是 SessionWorker 在 sessionScope 内单调签发的 checked u64 注册实例身份，失败/注销后不复用；RequestId 在该注册内单调签发。注册 incarnation 就是 ConsumerId，不另造并行代数 |
| DeliveryLease | 一次准入的 CPU 载荷额度；共享读取只保活数据与计数，不裁决 Gameplay/设备状态 |
| AttemptKey | 完整 HostContext + projectionScope + frameId；frameId 在 sessionScope 内唯一，ConsumerId/RequestId 是该记录的不可变关联而非第二套查找身份 |
| ObservationJournal | 有界观察证据；可记录版本号，不持有 FrameSnapshot、状态存储、Prepared 或设备对象 |

首版维持一个 Playback owner、一个实际呈现端和多个独立 CPU 截图消费者。内部数据边界区分
会话状态、消费者和 Renderer 身份；这不认证独立多窗口能力。PresentationRead 不跨操作、线程
或控制边界；跨线程只交付不可变 owning FrameSnapshot/FrameEnvelope。多 Renderer、跨 owner
物化和历史结果保留的已知改动范围见 §6.3，不能通过扩展一个 lease 暗中引入。

## 2. Judgement 发布视图（PD-1）

### 2.1 接口和字段

接口由 Judgement 维护，置于非安装的 internal bridge，Playback 通过显式私有 include 使用；
不再 include Judgement 的 `src/diagnostic_policy.hpp`，不暴露 KernelProjection 的容器布局。
不增加 Runtime/World/Renderer 反向依赖，不创建第二个 Fact 发布源。static/shared 构建均须通过
原有导出及包边界门禁；不得把此内部桥误装为 SDK 公共头。

| 设计操作 | 输入 | 输出与失败 |
| --- | --- | --- |
| `readPublicationSince` | session、期望 run identity、已消费的 exclusive cursor | Result<PublicationRead>；scope 不匹配、cursor 越界或 owner/reentry 不合法时拒绝，绝不隐式把 cursor 归零 |
| `readFact` | 有效 PublicationRead、区间内 ordinal | 带 tag 的只读 Fact 记录；phase outcome 提供稳定 FactId、完整 requirement identity、phase/outcome、可选 timing error；不按容器下标暴露可变存储 |
| `classifyPublicDiagnostic` | Judgement 错误记录 | 既有诊断映射策略元数据；Playback 保持公共 code/message、ordered context、sourceCode 与 cause 规则，不复制另一份私有码表 |

PublicationRead 必含 run identity/runScope、读取 epoch、sealed exclusive cursor、
publishable exclusive cursor、processed frontier、kernel work tick、可选 Fold work tick、
已发布 control cursor、是否有 Fold、fault stage/diagnostic、`mayProject` 和请求区间。
cursor 仍覆盖完整 Fact 序列；adapter 只跳过正式合同允许无需表现处理的记录，不能用筛选后的
数量充当 cursor。未知必需 tag/phase/outcome/聚合不允许跳过后推进，prepare 和运行拒绝见 §12.1。

| 状态 | publishable 范围及许可 |
| --- | --- |
| 有 Fold 且正常 | `[fromCursor, Fold.factCursor)`；采用同一次读取的 Fold work tick/control cursor |
| 无 Fold 的合法旧路径 | `[fromCursor, sealedFactCursor)`，采用 kernel work tick；不得虚构 Fold |
| Kernel/Fold/Control fault | 允许查询实际 sealed/Fold 前缀，`mayProject=false`；不借 query 激活新 token |
| 没有新增 Fact，但 control cursor 或 T 有变更 | 空 Fact 区间仍有效，不能据“cursor 相同”跳过控制或活动效果更新 |
| 纯 Playback、无 Gameplay | 不构造虚假 Judgement 视图；沿既有 Runtime-only 路径 |

### 2.2 一致性、有效期与性能

首次捕获元数据与读取区间在同一 owner 串行操作完成；期间不允许 Judgement mutation、
受控回调重入、异步保存或另一个线程访问。下一次 mutation、scope 替换、reset 或销毁使视图失效。
每次访问校验 read epoch；返回的短期 span/引用也不能越过该操作。该接口无锁、无等待，
但不是可跨线程的 lock-free 快照合同。

adapter 在本次操作结束前只把所需新增效果复制到 ProjectionCandidate；不复制完整 Fact 历史。
失败后重新读取新视图，不能持有旧 borrowed view 重试。公共 query/ReplayEvaluation 仍走既有
owning 结果路径并保留全量字段，不能用这个受限热路径替换其返回语义。
若以后确需跨 owner 读取，须另立 owning immutable-prefix 接口及容量方案。

## 3. Projection 的确定性处理（PD-2）

prepare 编译以完整 requirement identity、phase、outcome、timing 为键的绑定索引，另建
targetId→稳定布局索引、bindingId→来源谓词、规范化 groupMembers→组状态的索引。
哈希只用于查找；碰撞须完整比较，遍历次序不得决定结果。目标存在性在运行时按当前内容闭包复核，
不能因静态索引命中就接受已失效的目标。以下是权威规则的适配顺序，不是新聚合定义：

| 顺序 | 处理与提交规则 |
| --- | --- |
| 1. 预检 | 校验 scope、T、RuntimeFrame 和发布许可；同段 T 后退沿原拒绝/恢复合同，不能 clamp；fromCursor 不能越过 publishable cursor |
| 2. 读增量 | 按 Fact 原有 ordinal 顺序读取；来源谓词沿原 early/exact/late/Miss 规则；记录 scope 内已见 binding 成员 |
| 3. 分离记忆与活动 | `[start,end)` 的 end 到期只移除活动 token/候选，不删除已见组成员；future 按 effective T、Fact ordinal、规范化绑定顺序索引，不用墙钟过期 |
| 4. 判断聚合 | any 消费已匹配成员；all/groupCommit 等全部来源满足发布范围；每个成员仍独立检查 start/end，不因组 ready 延长 lifetime |
| 5. 目标与部分提交 | any/all 的缺目标只形成纯表现诊断；groupCommit 本次同时有有效和缺失成员时，允许有效成员进入本次候选，同时记 partial-group 和该组稳定拒绝；全部缺失不伪造“部分已提交” |
| 6. 保留已有成员 | 稳定拒绝只阻止该组新成员，已提交成员按原 lifetime 存活；Runtime 失败时本次有效成员、拒绝标记和诊断均不能先行安装 |
| 7. 去重和 resolver | token 幂等键仍是 scope 内 `(FactId,targetId)`，不是 bindingId、attempt 或消费者身份；相同写折叠。可能同时生效的相反值在 prepare 拒绝；运行中发现矛盾返回既有投影失败，不按遍历顺序挑赢家 |
| 8. 安装 | 仅 §5 的软件提交成功后推进 cursor、published 标记、future/group/token 状态和本次诊断 |

同 bindingId 的重复行、分段 lifetime 和不同 target 保留既有合法性规则，不能因为去重键相同就
提前删除尚未开始的合法窗口。`Initial < Behavior < Animation < GameplayOverride/FactBinding <
StudioPreviewOverride` 继续由[动画混合合同](../../../formats/ANIMATION_MIXING.md)拥有。
Seek/restore/内容替换整体重建 transient projection scope；从所选 Fact 前缀重建索引消费状态，
重新获取当前 target/resource，不沿用旧 scope 的 rejected/dedup 标记。

等价性材料必须覆盖 any/all/groupCommit、部分成功、全缺失、相同键重复、相反值、同 ID aliases、
future/expiry、无 Fact 更新、scope 重建和 Runtime 失败重试。沿用现有 fixtures 中的期望语义；
不因采用哈希索引重新生成 golden 作为正确性证明。

## 4. 提交状态、修订号与按需物化（PD-3）

### 4.1 数据权威与模块边界

本轮替换“每次更新都捕获完整 RuntimeResolvedValues”的首版实现选择。首版以当前 Runtime/World
的已提交值为权威，发布轻量提交描述；实际取得交付额度后才读取并生成 FrameSnapshot。
不能同时维护 World 和一份独立可写的 resolved-values 权威；不增加跨 owner 的当前状态借用。

| 层 | 最小数据 | 所有权/禁止项 |
| --- | --- | --- |
| PreparedPresentationData | content identity、布局 revision、稳定对象/相机索引、mesh/material 描述与闭包、共享定义 | 所属模块准备并不可变共享；PreparedPlayback 仍 owner-bound，无设备句柄 |
| TransactionState | 时间连续性、Runtime 状态、token/lifetime、projection cursor、future/active/group/rejected 状态 | 唯一 owner；求值后的下一次状态不代替当前画面，例如已扣减为零的 RemainingFrames |
| CommittedPresentationState | sessionScope、resultVersion、content/layout/world revision、来源种类与原 T/frame/discontinuity、实际消费的 Fact/control cursor/work tick、已提交激活描述 | 一个当前描述和一个候选；来源元数据不随 query、drain、交付或控制重新贴标；不含当前 renderer/resource generation、交付 controlSequence |
| PresentationRead | 与上述修订匹配的对象 presence、world matrix、visible、material identity/opacity/tint、相机原始值/transform | Runtime 维护非安装的中性只读桥；不依赖 Playback/Renderer 类型，不外传 entity/registry；不要求完整属性数组中转 |
| DeliveryBinding | 已确认 HostContext、projectionScope、ConsumerId、呈现端 instance/generation、实际资源证明；CPU capture 为无设备分支 | 注册须沿 §6.3.1 确认，控制/资源证明仍完整校验；注册不新增能独立解除 fence 的凭据 |
| FrameEnvelope | FrameSnapshot、原状态身份和来源 T/frame/cursor、当前完整交付 context、ConsumerId/RequestId、frameId、membership、CPU portable resources | 普通新帧与 retained 重绘均保留不可变注册身份；admission 后构建；不改 Gameplay identity/Replay 格式 |

物化器只存在一份，由 Playback 按稳定布局从 PresentationRead 取中性值，直接填充目标 Snapshot；
当前 World 后端的遍历和组件映射封装在 Runtime 桥内。不先拷一份完整中间结果再拷一份 Snapshot。
对象顺序、字段缺失、资源错误、相机默认值和矩阵计算沿旧 extractFrame；viewport、资源匹配、
相机不可逆等错误仍是物化错误，不改 RuntimeUpdated 或 kernel/Fold fault。
稳定索引必须绑定 content/layout revision；内容重建不得把旧索引解释成新对象。
来源种类区分 Prepared、RuntimeUpdate、Restore 和既有同步 mutation；T/frame/Fold 只在源操作
实际提供时存在。首次 prepare/commit 后的初始 Snapshot 不伪造一次 update 或零值 Fold。
Runtime-only、GameplayOnly 与有 Gameplay 的读取沿既有许可区分，不能为了填 envelope 字段
生成虚假判定来源。内部载体需要 absence 分支的差异归 §11，不改公共 FrameSnapshot 格式。
轻量描述只标识当前 projection 激活表的修订/索引；该表仍由 projection 唯一拥有，不在每次
更新复制一份 membership。物化时核对同一修订后，才为该次 owning 帧复制精确成员。

| 内部读取操作 | 输入和结果 |
| --- | --- |
| `readPresentation` | 期望状态/布局修订；在既有 owner 操作内建立 PresentationRead，stale/重入/无 World 拒绝，不分配完整属性数组 |
| `readObject` | 有效视图、稳定布局索引；返回本操作内的中性字段及 presence；越界/旧布局拒绝，不通过 entity 值暴露 registry |
| `readCamera` | 有效视图；返回原始相机参数/transform 或合法 absence；不计算依赖 viewport 的投影 |
| `materializeSnapshot` | 有效视图、viewport、已准入目的存储；按旧字段/错误顺序填 Snapshot。原同步 extractFrame 用同一函数，调用方内存仍归其管理 |

### 4.2 修订、只读窗口与保留范围

PresentationRead 只在同一 owner 的一个不重入物化操作内有效；开始时核对完整状态修订，结束前
不得 mutation/控制切换，任何引用/span 不得离开该操作。借用生命周期与 §2 一致，但来源不同。
公共 extractFrame 走相同物化器；query/extract 不推进修订、mutation generation、Fact 或 lifetime。
读取桥必须使用真正的 const Runtime/World 访问；非 const 对象调用 `withWorld` 即使 lambda
接收 const World 也不能据此当作已选 const 重载。mutation guard 不能被只读物化误触发。

| 事件 | 状态修订 / 来源 | 显示与交付 |
| --- | --- | --- |
| 首次内容提交、恢复、Seek、Reset、替换 | 预留新 resultVersion；保存实际产生状态的来源 | 可在第一次 update 前提帧；GameplayOnly 沿无 World 拒绝 |
| 正常成功 Runtime 更新 | 新 resultVersion；update ordinal 增加一次 | 从已提交 World 读取，不能重跑 evaluator |
| 成功同步 override/可见 mutation | 新 resultVersion；按该操作既有语义记录来源 | 不冒充成功的普通 Runtime 更新 |
| 纯控制确认、Pause/Resume/EOF 换段 | 不改画面修订和原 T/frame/Fold 来源 | 只切换 DeliveryBinding，具体许可见 §4.4 |
| extract、receipt、保留画面重绘、viewport 改变 | 不改修订或 update ordinal | 仅物化/交付行为 |

计数在 mutation 开始前 checked reserve；耗尽拒绝，不能 wrap。首次提交也有有效修订，Empty
没有可读状态。首版不保留历史 World/结果队列；历史版本只在已经准入的 owning 帧中存在。
ExactVersion 已失去且调用方没有对应 retained handle 时返回内部 `VersionUnavailable`，不能
拿当前值冒充、重跑求值或静默 Seek。保留画面允许落后于当前修订，不被错误当成当前状态。

首版不建立跨消费者的结果缓存。相同 viewport 的重绘复用已计费的 RetainedFrame；新 viewport
依 §6 从保留 Snapshot 的完整对象值、view matrix 和相机参数物化，仍需新载荷额度。
将来若测量支持缓存中性值，缓存只可由完整修订键派生、有界且可丢弃；不能新增提交成功点。

### 4.3 mutation 的统一入口

更新、override、内容/布局替换和内部可变 World 访问统一经过 mutation guard：进入时检查计数
和许可、失效借用 epoch；退出时由同一发布步骤记录实际 revision。const 读取不改变 revision。
新增属性沿同一入口，不得在每个消费者中手工维护另一套 invalidation 清单。

任意既有可变受控回调不属于可回滚 PD 事务。调用前预留修订；无论正常返回还是异常返回，若已
进入可能写入的回调，都保守发布一次实际 World 的新修订并失效旧视图，不声称回调副作用已回滚。
纯 preflight 拒绝不增加修订。下一次物化只读取实际 World，沿原缺字段/相机错误返回；不偷偷修复
World 或重算 lifetime。这里不新增宿主回调、运行时脚本或每帧脚本入口。

在 Gameplay 带状态包装与 Runtime 原有同步入口间提取共同 mutation/finalize 路径；不能长期
维护两套求值、相机解释或缓存规则。无法覆盖的现有写入口必须在 PD-3 接入前列明并接入，
不能靠“使用者记得标 Dirty”作为已完成条件。

### 4.4 内容来源与控制许可

本节落实[宿主合同 §8 候选修订](../../../api/realtime-host-boundary.md)的内部接入，接受门禁见 §11。
源状态身份从不等于当前交付身份。
DeliveryBinding 的控制/资源部分仅从全域确认的控制回执或既有合法 EOF 换段证明更新；收到
命令、超时、单域成功都不能更新该证明。消费注册资格另按 §6.3.1 确认，两者必须同时有效。
控制切换不复活已撤销注册，注册成功也不解除控制/fence。切换只安装已预备元数据，不重跑
Runtime 或读新增 Fact。

| 操作 | 表现状态/作用域 | 绑定与保留画面 |
| --- | --- | --- |
| Pause 成功 | 保留状态修订、激活和 projectionScope；drain 的新前缀不冒充该画面的来源 | 更新确认 context；重绘 D5 最后成功画面。它可以落后于当前修订；没有保留画面则报告 NoRetainedFrame，不隐式发布 drain 结果 |
| Resume 成功 | R2 照常提交；仅此控制不消耗 Runtime lifetime、不创建新表现激活 | 更新绑定；下一次合法表现更新消费新增前缀。等待期间可重绘原保留画面 |
| 合法 EOF 仅换 clock segment | 保留表现作用域和当前状态；若同一操作另有实际表现更新，单独按更新规则处理 | 消费现有尾段证明；不能伪造普通控制确认 |
| Seek/Reset/Stop/内容或表现绑定替换 | 按各自合同发布新状态，替换 projectionScope；旧活动反馈按控制结论冻结 | 清除“可再次提交”的旧保留画面资格，已交付 owning 数据仍可存活 |
| device/resource generation 改变 | 不据设备变化推断 Gameplay 更新 | 旧凭据失效；必须走资源准备/激活证明后重新准入，首版不直接跨资源代复用旧保留帧 |
| 控制/设备结局不明确、Closing | 保留真实状态，不生成新已确认绑定 | 禁止新 present；只有 query/清理和原合同明确许可的动作 |

选择 projectionScope 仅随表现身份重建而变化，替换当前 Player 在每次 finish 都递增它的策略；
controlSequence/clockSegment 仍逐条校验。这样 Pause/Resume 不复制一套反馈、不重置 dedup，
也不把已经消费的 Fact 重新激活。旧 control 的 receipt 只按原 AttemptKey 更新已有义务的证据，
永远不能作为新控制确认；作用域真的替换后则只记旧作用域历史。

保留帧重绘必须由 owner 根据新绑定签发新 attempt；原 envelope 不可修改。校验源 session、
content/layout、projectionScope、资源代与保留 handle；重新确认上下文不改其原 T/frame/cursor。
viewport 不同仅重新计算相机投影及复制所需 Snapshot 字段。普通发送者不能拿任意旧帧重新贴标。

## 5. 软件事务与失败恢复（PD-3）

### 5.1 单一提交边界

原子范围为 projection、Runtime/World 软件状态、last frame/T、轻量提交描述和内部诊断。
Judgement seal/Fold 是独立的此前成功点；GPU/audio 沿既有控制协议。**结果捕获和帧构建全部
位于成功提交之后的只读交付面**，因此不新增“Runtime 已成功、捕获失败再撤回 World”的窗口。

| 阶段 | 操作与所有者 | 失败行为 |
| --- | --- | --- |
| Prepare | Playback 构建 ProjectionCandidate；预留描述、诊断、激活变更、计数和 Runtime 事务空间；核对 scope/control/revision | 丢弃候选，保留实际已提交 Fact/Fold 和旧表现状态 |
| Runtime candidate | 唯一 evaluator 计算候选；Runtime 内部完成全部可能失败的验证/分配和派生 transform 计划 | 保持旧状态；不预先推进 projection cursor |
| Install | Runtime 安装已准备的软件写入并成功返回；同一不重入 owner 连续执行 Playback 的 noexcept finalize，发布 cursor/state/描述 | Runtime 成功之后没有捕获、外部回调、分配、stale 检查或可失败消息写入 |
| Delivery | reserve → PresentationRead → FrameSnapshot → owning 交付 | 失败只影响本次交付；已成功更新和 lifetime 不撤回 |

控制在整个操作边界串行接入；不允许另一个线程在 Runtime 成功与 finalize 之间改变 epoch。
异步来源只排队命令，不能写当前状态。“安装前 stale”检查在进入 Runtime 候选操作前完成；
未来跨 owner 准备须重新设计提交凭据，不能把其额外失败点塞进本单 owner 成功尾部。

### 5.2 Runtime 内部的派生状态

现有 resolver/camera/appearance rollback 不能证明完整 World 恢复：`PropertyResolver::rollback`
只恢复局部 Transform；`updateWorldTransforms` 还更新 WorldTransform、拓扑缓存和 scratch。
PD-3 先提取该函数现有算法的 **prepare/apply** 边界，不另写层级求值器，不克隆整份 World：

1. 准备阶段读候选局部值，计算父子 world matrix、拓扑/dirty/cache 变更；在候选 scratch 中验证
   有限数、组件存在性、字符串、所有 storage 容量和新增组件所需空间，禁止发布缓存。
2. 稳定组件集合的热路径只准备受影响值/索引，随后写已验证的现有槽；不每帧复制整个 registry。
   首次派生组件或拓扑增删的冷路径，选用仅受影响组件类型的候选 storage 和候选拓扑缓存，
   准备完成后一次交换该存储；其安装必须保留 entity 身份、组件 presence 与既有访问语义。
   不复制整 World，也不在活动存储中提前插入可见占位。只 reserve vector 不能证明
   `emplace_or_replace` 无失败；storage 交换能力及回调边界必须在 PD-3-a 验证。
3. 安装区只能执行经过证明的组件赋值/交换和缓存交换，无分配、回调/通知、可失败查找；原值与
   派生值一同变成当前状态。必要的 World 私有 storage 安装原语是本批交付项，不是假定已有能力。
4. World 之外的 projection/描述已准备好，随后 finalize 不可失败；旧候选/缓冲的析构和回收仍
   计入 owner 工作预算，不能用 noexcept 掩盖销毁延迟。

该边界也供原同步 update 使用。不得新增一份只在 PD 模式工作的 rollback 清单，或在失败后
尝试重新求值修复 World；修复本身可能失败。现有回滚路径可在迁移提交中保留，但完整语义等价、
无分配安装和父子派生值故障证据通过后，才允许将其替换；不能将测试不足的部分标为已关闭。
若出现安装不变量破坏，进入既有禁止推进/恢复路径，不能把部分已写状态报告为普通可重试失败。

### 5.3 错误面与重试

| 失败点 | 必须保留/报告 | 后续动作 |
| --- | --- | --- |
| seal/Fold/control fault | 实际前缀、首错、mayProject=false | 只执行原合同许可的 query/archive/reset/替换 |
| Projection/提交描述准备失败 | publicationFailureStage=Projection；RuntimeUpdated=false | 重新读同 scope 前缀，表现重试 |
| Runtime 候选或 World 写计划准备失败 | publicationFailureStage=Runtime；旧 World、派生缓存、cursor、T、版本均保留 | 修复后表现重试，不重新 submit 已接纳输入 |
| 安装后 PresentationRead/FrameSnapshot 失败 | 已成功操作的 RuntimeUpdated 保持原值；独立物化失败 | 同一当前版本可重新物化，已失去版本返回 VersionUnavailable |
| 控制绑定尚未确认/资源过期 | 交付拒绝，保留实际控制阶段和状态来源 | 查询/完成原控制，不借重绘重新求值 |
| Renderer 失败或 Unknown | 原始调用、CPU 结束、设备完成分别记录 | 按 RPA 清理，不回退 Gameplay |

表现重试仍要求合法生命周期、scope/control 与原 T/frame；新更新成功或 scope 替换使旧重试
stale。新的合法 Fact 前缀只有经明确新请求和时间校验才可消费，不能暗改失败请求时钟。
物化重试只读；故障注入须覆盖 reserve、候选层级、首次组件/拓扑变更、安装前拒绝及安装后
提帧失败，并比对局部/派生矩阵、缓存有效位、cursor、last frame、token/lifetime、版本和诊断。
这是待实施证据，本轮没有执行这些实验。

## 6. 消费者、额度和交付（PD-4）

### 6.1 最小接口与准入顺序

| 设计操作 | 规则 |
| --- | --- |
| `registerConsumer` | 声明模式、viewport、count/byte 上限、deadline；owner 预留额度并签发 ConsumerId。CPU capture 本地确认；呈现端还须取得 §6.3.1 的激活确认，不能把排队当注册成功 |
| `requestFrame` | ConsumerId/RequestId、期望 session/control/scope、Latest 或 ExactVersion、viewport；相同键同内容幂等，相同键不同内容 message conflict |
| `admitDelivery` | owner 先验证 Active 注册、上下文和版本，再 reserve 该消费者及全局 CPU 字节/对象额度；不成功就不构建 FrameSnapshot |
| `materialize` | 从匹配修订的 PresentationRead 按 viewport 直接构帧；异常返还 reservation。Worker 在发布 Ready 响应前分配呈现 frameId、登记 attempt 或 coverage loss，构造不可变 owning envelope；CPU capture 无 attempt |
| `pollDelivery` / `takeDelivery` | poll 只查状态/元数据；take 原子地从 Ready 槽移出 owning envelope 并置 Transferred，不分配 frameId 或写 journal。重复 take 不再发帧；移交与取消必须有唯一先后 |
| `requestRetainedPresent` | 传已登记 RetainedFrame 的 owning handle 与新 RequestId；按 §4.4 核对当前绑定和原内容来源。允许旧 resultVersion；同 viewport 复用 backing，新 viewport 需新 CPU 额度和物化，均生成新 attempt |
| `releaseDelivery` | 最后读者完成后返还 count/bytes；只更新存储计数，不调用 Playback/设备代码，也不要求等待 receipt |
| `unregisterConsumer` | Worker 先关闭该注册 admission，再按 §6.3.1 取得 Renderer 的拒绝后续调用确认；在途 lease 不撤销。未确认时保持 Revoking，剩余实占额度保留到真实释放 |

frameId 由 SessionWorker 在 sessionScope 内单调且 checked 分配，不随消费注销、控制切换或
journal 淘汰重置。呈现 envelope 在发布 Ready 响应前取得新 frameId，后来即使未提交也不复用；
同一状态的两次重绘具有不同 frameId，resultVersion 可相同。CPU capture 不分配假 attempt。
RequestId 用于请求重传，frameId 用于 present 幂等；同请求重传不得触发第二次 present。
首版每消费者最多一个未移交请求，处于处理中或唯一 Ready 响应槽中，等待领取也占该名额。
RealtimeLatest 可在移交前按原规则合并，旧请求明确终结，ExactCapture 不静默替换。已完成请求用单调
RequestId high-water 加有界元数据结果保留去重；旧请求超出窗口返回 `RequestHistoryUnavailable`，
不重新执行。精确历史不承诺无限保留。

request deadline 使用宿主共同单调时钟，限制本次请求最迟移交时间，不承诺实际 present 已完成。
准入前、发布 Ready 前及 take 的同步边界均检查；`now >= deadline` 且尚未移交时终结为
ExpiredRequest，之后不能领取。Worker 在交付维护额度内清理无人领取的过期 Ready；take/过期
与取消一样只有一个胜者，已登记且证明未移交的 attempt 可记 NotSubmitted。构帧期间过期
不撤回已提交更新；已移交的 owning 值不因请求过期被强制收回，后续执行仍核对注册和绑定。
因此请求时限、实际调用时间及 receipt 观察 deadline 是三个不同指标。

同一个 CPU backing 可被最后成功画面与本次 Renderer 读取共享，只按实际占用计 bytes；每次
attempt 的在途元数据仍单独受限。保留画面本身是读者，不能把一次 present 返回误当最后引用归还。
新 envelope 的 identity 不可变；内部 owning shared backing 避免同 viewport 重绘深拷贝，
不改变公共 FrameSnapshot 值语义。普通请求不能重包装失效凭据；只有 §4.4 的保留重绘路径可在
合法控制确认后签发新绑定，源身份和 membership 原样保留，绝不改成当前版本的来源。
resize 从原 Snapshot 的完整对象、view matrix、原始相机参数生成新 Snapshot，只重算 viewport
投影；所有复制计入该请求。若没有完整可重建字段则该路径不能通过 PD-3 字段等价门禁。

Renderer 在本地成功调用后，以预留槽替换唯一 last-success backing；不依赖可能丢失的观察
receipt 才知道哪幅画面成功。失败保持旧 backing。Pause 重绘由 Renderer 核对该保留身份；
最后成功画面、正在构建/提交的新画面同时存活的峰值必须可准入，不能靠暂时释放旧画面换额度。
注册保留画面的呈现消费者时，最低 count 为 2，bytes 至少覆盖两份声明的单帧保守上界和必要
元数据；不足则拒绝配置，不能接受一个只能显示首帧、以后永久 SkippedNoCredit 的 profile。
额外在途帧和异步释放 backlog 继续占额度，达到上限时跳过新的交付；不借此等待 GPU 或暂停更新。

Ready 响应持有 request identity、选定 resultVersion、不可变 envelope 与 lease；其暂存计入
该消费者预算。present attempt 在发布 Ready 前已经登记，但 dispatch 尚未移交；poll 只读
状态/元数据，不复制 owning 数据、不分配 attempt。take 与 Ready 取消/合并/注销在交付槽的
同一同步边界裁决：取消先发生则 take 不再取得 envelope，Worker 可记录 NotSubmitted；take
先发生则实际 owning envelope 已移交，之后取消只能按在途处理，不能直接记 NotSubmitted。
同步状态必须足以在 Worker 观察前先收到 receipt 时识别已经移交，且只有 Worker 写 journal；
交付槽只负责所有权和移交事实，不读写 Gameplay、编号器或 journal。
移交后 Worker 的请求去重记录只留身份/阶段/错误等有界元数据，不持有 envelope、Snapshot
或间接保活它的句柄。重复 request/take 只报告已移交或历史不可得；若调用者丢弃了自己的
owning 帧，需要新的合法请求，不能从历史记录恢复已失去版本。
移交不靠可丢 receipt 通知，也不新增逐帧等待 Worker 确认的往返。Renderer 只消费已 take 的
envelope；领取后的显式复制/共享及最后成功保留仍按实际读者计费。公共 FrameSnapshot 的
额外调用方深拷贝仍沿本节的预算范围，不把它重新计入宿主可控存储。
控制终态、CPU lease 归还和 present 证据使用不同消息/通道。
admission 的 bytes 使用 prepare 布局及字段长度的保守上界，包含 membership、字符串、响应
和 Snapshot 峰值；不为估算额度先分配完整帧。既有可变 World 写入若超过该布局允许的长度，
本次交付明确失败，不能越过预算；公共同步 extractFrame 的既有调用方存储语义不改成宿主额度。
CPU 额度由独立 owning storage account 管理，只含计数与分配器，不持有 worker/GL 指针；
会话关闭后仍可释放。排队回收的 bytes 仍计入容量，不能在物理内存尚占用时提前返还额度。
预算覆盖该宿主分配的存储及仍存活的交付 lease；调用者另行深拷贝公共 FrameSnapshot 的内存
属于调用者，不能据此承诺整个进程恒定内存。该范围和跨 Close 外部保留量须在测量报告中分列。

### 6.2 首版策略与扩展边界

| 消费者模式 | 版本和积压策略 | 达到上限时 |
| --- | --- | --- |
| RealtimeLatest（首版主窗口） | 普通 admission 取最新版本；冻结/显式保留重绘使用原保留身份；未移交请求可合并；一份最后成功画面计入额度 | 返回 SkippedNoCredit/ExpiredRequest；Gameplay 继续，不能拿旧图冒充最新 |
| ExactCapture（显式截图） | 若精确版本仍为当前，准入后持有其 owning 帧；已失去版本立即 VersionUnavailable | 请求明确失败；不退回 Gameplay、不伪造精确截图；窗口重绘不创建新反馈义务 |
| ReliableLive（未来实时录制消费者） | 只承诺从注册后的已声明采样点、成功准入帧顺序可靠；不声称保存每个 Runtime 更新 | 无法满足下一个承诺采样点时终止该捕获并报告缺口/不完整；不能默默丢帧后宣称完整，不能拖慢活动游戏 |
| OfflineExact（未来导出） | 独立 Playback/replay session，由离线任务按自己的节拍推进；可在更新前等待该任务额度 | 只背压离线任务；不得复用活动实时会话制造“无损且永不阻塞”的承诺 |

PD 首版交付 RealtimeLatest 与 ExactCapture 的内部接口和契约测试；ReliableLive/OfflineExact
固定上述失败边界作为未来接入条件，不在本阶段实现录制/导出产品。
同一 owner 每轮先处理可用的控制/故障，再完成已准入执行任务和必要的软件事务，随后在独立的
delivery 工作额度内按 ConsumerId 轮转，每消费者每轮最多物化一帧。request deadline 按单调时钟
检查，viewport/物化开销计入声明的单帧与每轮上限。不能让某个消费者抢占其他消费者的预留份额；
容量校验要求活动/待确认注册预留与退役实占之和不超过全局上限，详见 §6.3.1。
首版不跨消费者共享额度或设计优先级借贷。

一个物化操作不被控制打断，但必须满足已声明的单次执行预算；控制在其后的事务边界优先。
若工作集无法满足该预算，记录该准入配置不合格，而非宣称跨线程后自然满足实时要求。

### 6.3 呈现端边界与未来改动范围

ConsumerId 表示一次消费注册；renderer instance 表示设备 owner 的实际实例，两者不可混用。
首版注册呈现消费者时必须匹配当前唯一已确认 HostContext 的 renderer generation、实际 instance
和资源准备证明；第二个独立 renderer 返回内部 UnsupportedEndpoint，不伪装成额度不足。
多个 CPU capture 消费者不参加设备控制，不会因截图注册改变全局 renderer context。
首版至多一个 RealtimeLatest 呈现注册，包含尚在 Registering/Revoking 的保留位置；向同一端
申请第二注册返回内部 EndpointAlreadyBound，不替换正在处理的注册。注销确认与新注册的
顺序见 §6.3.1。新注册不继承旧 RequestId、帧执行标记或保留画面；Renderer 的注册身份前沿
仍保留，旧 lease 仍计费至真实释放。

内部保留独立 DeliveryBinding 入口，但不新增公共 HostContext 格式、通用 endpoint 插件或
任意参与者框架。未来扩展的成本明确如下，不能把“另行设计”称为无需重构：

| 扩展 | 可保持的边界 | 必须修改和验证的部分 |
| --- | --- | --- |
| 第二个独立窗口/Renderer | Gameplay、Runtime 求值、状态修订、FrameSnapshot 物化 | RPA 将单一确认 context 扩展为会话控制与每端设备绑定；固定参与者集合、每端资源准备/激活/retire、加入/退出与 Close。独立 preview 是否可失败而不暂停主窗口须先裁定 |
| 录制/离线导出 | 中性读取与 owning 帧、来源身份 | 声明采样点、独立预算和 §6.2 缺帧失败；离线使用独立 session，不能改变实时运行节拍 |
| 将物化移到其他 owner | 物化规则与稳定内容/对象身份 | 先增加 owning immutable 状态捕获、保留/背压；当前 borrowed read 不能迁移。新增复制成本须重测 |
| 增量帧或中性值缓存 | 完整 Snapshot oracle、单一求值语义 | 基线版本、丢失后完整重同步、布局兼容、每端容量；禁止把 cache 变成第二个可写状态源 |
| 新表现属性/Studio 预览 | 公共状态来源、mutation guard、寿命与交付协议 | 属性的唯一 evaluator、World 安装、只读桥和 Snapshot 字段等价；默认不改 journal、控制器或消费者状态机 |

最后一行不是任意属性都能零改动：影响资源闭包/设备能力的属性仍需资源合同；新公共字段仍需
版本门禁。每次扩展列出必改模块与无需改变的既有语义，以实际影响范围审查维护性。

### 6.3.1 消费注册的确认、撤销和重新加入

注册键为 `(sessionScope, ConsumerId)`；生命周期消息另带 Renderer instance/generation、
动作与原目标 context/配置。消息只来自当前会话的既有 owner，不能凭一个较大数字取得许可。
发起注册的可靠请求沿宿主现有 operation/request 身份去重，接纳时绑定唯一 ConsumerId；
查询/重传返回该绑定，不因激活超时另签身份。当前前沿身份的不可变目标/配置保留至被较大
合法身份取代或所属域结束，不能在激活结果确认后就删除，仍须核对活动期同键异载荷；具体
操作结果沿可靠设施的确认规则退休。注册确认只使用当前待处理动作的结果，不能把传输确认当激活完成。
普通新帧、保留重绘、响应和 attempt 记录都绑定该注册键；CPU capture 不建立 Renderer 资格。
这些是内部宿主字段，不加入公共 HostContext、Gameplay identity 或 Replay。

| Worker 状态/事件 | 串行裁决与对外结局 |
| --- | --- |
| Registering | 预留该注册额度及有界生命周期/清理槽，签发身份后发送激活；Renderer 校验当前设备/context，在无旧活动注册时安装。Worker 收到匹配确认并复核目标仍有效后才转 Active |
| 激活拒绝、调用方取消或确认期间被控制替换 | 不开放 admission；未发送可本地结束，已发送进入 Revoking，须确认未激活或完成撤销。迟到的激活成功只补实际阶段，不能把 Revoking/Retired 改回 Active |
| Active → Revoking | Worker 先拒绝新请求，取消未移交交付，再发送该注册的撤销。发送/超时不等于 Renderer 已停止；对外仍为撤销待确认 |
| Renderer 撤销生效 | 在实际调用之间的 owner 边界清除该注册资格和 last-success，保存不可复活标记，再确认。已进入的同步调用先返回；此前已提交的 GPU 工作仍按原 fence/owner 负责 |
| Revoking → Retired | Worker 取得匹配撤销或终态未激活证明后，注销成功只表示不能再开始调用；同端才可申请新 ConsumerId。旧 CPU/GPU 存储可以继续存活，仍独立计费 |
| 确认超时、结果不明或 Close | 查询/重传同键，不换身份绕过；保持 Registering/Revoking 或 Closing 及缺失责任域。同端新注册拒绝，主线程继续服务确认/清理，不能先 join 等待它的 worker |

首版每个呈现端只保留当前 session 的最大已接入 ConsumerId、该身份的 Active/Inactive 标记
及当前注册描述/未确认操作结果；终态摘要沿可靠设施有界退休，已退休身份不另建永久集合。
合法激活只接受更大的新身份；同键
重传只返回当前已知阶段；合法激活被终态拒绝也登记 Inactive，不能在 context 改变后重用。
Inactive 不再次激活。激活前的合法撤销也先登记该身份为 Inactive，
从而拒绝其后到的激活。比前沿小的旧激活/撤销不改变当前注册；query 只能证明其已无资格，
精确历史已丢失则明确不可得，不能编造从未执行或原拒绝原因。
当前活动或尚未确认的旧注册占用同端位置；不同身份的激活/撤销不能覆盖它。消息同键目标
不同且仍有原描述可核对时走既有 message conflict；较旧身份的精确载荷历史已丢失时，只报告
已无资格/历史不可得，不声称载荷相同，也不为比较而重新激活。Worker 只用当前待处理动作的
匹配回执推进状态；旧确认不能激活或注销新身份。这个前沿不因普通控制、注销或 journal 淘汰清零；仅在所属 session/设备
owner 已终结、旧路由失效且新 context 独立确认后建立新域。

注册固定实际 Renderer instance/generation。原目标 context 是激活和消息一致性的依据，不是
之后每帧永久不变的控制许可：同端普通控制或仅 resourceGeneration 变化可保留 Active 注册，
但必须按 §4.4 更新已确认绑定，并使失效保留图不可重绘。Renderer instance/generation 更换
则须先取得旧注册撤销，或原 owner 终结且旧路由失效的终态证明，再为新端注册新 ConsumerId。
失联/超时不构成该证明；设备控制确认仍独立完成，旧 GPU 责任不因更换注册而消失。

注册步骤是宿主内部端点生命周期，不新增 RPA ControlRequest 或第二套全局控制终态。消息复用
已有可靠控制/故障设施的传输与保留能力，原控制/fault 优先，注册独占预留自己的转换槽；
保留与退休沿可靠设施确认规则，不走 PresentReceipt。
注册前预留必要撤销/结果容量，不另建无界队列；同一时间只有一个注册转换过程，撤销可终结
本次未确认的激活但不能丢弃其实际阶段。控制/故障优先于激活；只换控制时可保留 Active
注册身份，但仍先 fence 旧帧并完成原控制协议。撤销总能按原注册目标清理，不以旧控制
context 已过期为由拒绝清理；新帧的 context 校验不因此放宽。

注销的资格确认不等待 GPU 完成或外部 CPU 帧销毁；若既有设备错误禁止新使用，该限制继续
生效。未完成的 CPU reservation/响应先处理完，才能释放其未使用预留份额；退役注册仍活着
的载荷、元数据和回收积压继续占全局额度。新注册准入须覆盖活动/待确认注册预留与退役实占
之和；不能注销后重复承诺同一预算，也不要求外部 owning 值全部销毁才完成资格撤销。
CPU capture 注销只在其交付 owner 边界结束未移交响应；它不等待设备注册确认。

### 6.4 实际 present 的一次执行

Worker 的 RequestId 去重和 journal 的 receipt 去重不代替 Renderer 的执行保护。普通新帧
与保留重绘使用同一实际调用入口，依次核对：当前 session/Active ConsumerId → 完整绑定及
现有控制许可 → retained 身份（仅该路径）→ 本注册 frameId high-water/唯一活动 attempt。
所有检查通过后先登记执行标记，再调用 submit/present；同一 owner 内这段不能被撤销或重入
打断。注册匹配失败时不推进新注册的 frameId 前沿，也不替换其 last-success。

同一注册向呈现端按 frameId 递增移交；同 ID 重传返回已保留结局；结局已淘汰时返回未知/
历史不可得，不能重新调用。低于 high-water 的迟到未执行帧可以跳过，不能重排到新帧之后。
该帧执行标记独立于 observation journal，只在 §6.3.1 旧注册撤销已确认、新注册激活后为
新身份初始化；注册身份前沿不同时清零。Unknown 不允许同 ID 再执行，再次调用必须新 attempt。

拒绝一个迟到副本只证明本次未调用，不证明该 attempt 从未调用：若原结果仍可查则保留原
结论，否则返回历史不可得，不能因注册 stale 或帧号较旧伪造 NotSubmitted。只有明确的
未移交/owner 未调用终态证据才沿 §7.2 登记 NotSubmitted。旧注册的合法 receipt 仍按原
AttemptKey 和已登记 ConsumerId 更新原证据，不确认新注册、不替换新图，也不因注销自动
取消同 scope 的反馈义务。CPU capture 没有帧执行标记或反馈确认资格。

## 7. Observation journal 与历史（PD-4）

### 7.1 三种状态不得合并

| 维度 | 记录与管理责任 |
| --- | --- |
| CPU 载荷 | owner-held / transferred / readers-released；由 lease/最后读者证明，不由 receipt timeout 证明 |
| 设备使用 | 尚未使用 / in-flight / fence-confirmed-retired；由 Renderer owner 管理，Unknown 不能释放设备对象 |
| present 证据 | Pending、Succeeded、Failed、Unknown、NotSubmitted；由 attempt journal 记录；Succeeded 仅指调用成功 |

`UnknownInFlight`/`UnknownDetached` 是 present=Unknown 与 CPU 读取状态的组合诊断，不是新的
Gameplay/设备状态。Detached 表示 CPU 读者已经明确结束，GPU 仍可 in-flight；不能据此解除
resource fence。`LateObserved` 是晚到成功的附加事实，不覆盖原来 expiry/control 时的状态。

attempt 记录只含 AttemptKey、consumer/request identity、源 resultVersion、源 T、membership、dispatch
记录、call/receive monotonic time、receipt fingerprint、错误和 deadline。feedback 记录包含既有
FeedbackId、created、显式寿命分支、当前状态、冻结的到期/取消状态及 laterConfirmed 证据。
membership 是当次实际送入 Renderer 的完整集合，使用 owning 紧凑 ID；不能借用已释放结果数组。
集合与错误字符串也计字节预算，不能仅限制 attempt 条数。

寿命采用 `Finite(end: GameplayPresentationTick)` / `UntilReset` 两个分支；Finite 要求 end >
created，区间仍为 `[created,end)`。UntilReset 没有按 T 到期的转换，仍服从显式作用域替换/
取消；`T=INT64_MAX` 不使它到期。有限 end 恰为 INT64_MAX 与 UntilReset 必须可区分。
TTL 只管理证据，不把长期活动效果改成 ExpiredUnconfirmed。

当前安装头的 FeedbackObservation 使用整数 expiry，`gameplayFeedback` 将缺失 end 映射为
INT64_MAX，不能作为新 journal 的输入表示。选择在下一次经批准的不兼容 SDK minor 中将
expiry 改成显式 optional typed end（absent=UntilReset），同步该公共方法、ABI 与消费者；
不并行维护永久 V1/V2 反馈状态机。旧 0.8.x 构建可作历史基线，但不得混用头/库或宣称其 sentinel
支持完整新语义。§11 的公共合同/版本门禁须在此项实现前完成，本轮不修改头或版本。

### 7.2 状态转移和幂等

| 输入 | attempt 处理 | feedback/资源影响 |
| --- | --- | --- |
| 已登记但未移交的 latest 被替换或取消 | take/取消同步证明从未移交时为 NotSubmitted，记录原因，释放本地 payload；CPU capture 无 attempt | 不确认任何反馈；不是 Unknown；已生效效果仍按 T/lifetime 结束 |
| Ready 响应成功 take，移交给 Renderer owner | 已登记 attempt 的 dispatch 转已移交，present 仍 Pending；先到 receipt 不得被后来读到的旧 Ready 状态降级 | CPU 由读者 lease 保活，设备使用另记；仅写入 Ready 槽不算已经移交 |
| 匹配的 Succeeded | 保存不可变 receipt；Pending/Unknown 可得到成功证据 | 当前 scope 的 Created 反馈记 ConfirmedPresent；ConfirmedPresent 保持；UnknownEvidence/已到期/取消或旧 scope 只记 laterConfirmed，不修改原结论 |
| 匹配的 Failed | 保存失败终态，Unknown 可被实际失败补足 | 不确认反馈；是否释放 CPU/设备取决于各自完成证明 |
| Unknown、receipt 丢失或观察 deadline 到期 | 记 Unknown，保留已知证据；可继续接收迟到回执 | 受影响反馈 UnknownEvidence；不等于“确定未呈现”，不暂停 Gameplay |
| 完全相同的重复 receipt | 按完整 fingerprint 幂等返回 | 不重加确认/缺失计数、不重放设备动作 |
| 相同键但终态/字段冲突 | message conflict，保留先前证据和冲突诊断 | 不覆盖终态、不释放资源；迟到 Unknown 不能覆盖已知终态 |
| 已移交后请求取消 | 等所属 owner 的未执行确认或实际调用证据，不能直接变 NotSubmitted | Unknown/Pending 均不是已取消或可释放证明 |
| 注册注销、旧 envelope/生命周期消息迟到 | 按 §6.3.1/§6.4 拒绝再次取得执行许可；拒绝副本不改写原 attempt 终态 | 原合法 receipt 仍只更新其已登记注册/attempt；不替换新注册 last-success，不释放旧 GPU 责任 |
| control/segment 仅换许可 | 停止旧交付；按 §4.4 更新绑定，保留同 scope 的已有义务 | 旧 receipt 仍只访问原 AttemptKey，可补原义务证据，不能解除新 fence 或新建激活 |
| projectionScope 真正替换 | 停止旧交付；旧未完成反馈冻结控制结论 | 旧 receipt 只更新旧 scope 历史，不能污染新 scope 或激活新 token |
| 日志已超出保留范围 | HistoryUnavailable，返回可用覆盖范围与缺失类别 | 不能重建 membership、复活帧或声称精确失败；不当作新作用域结果 |

发生冲突的 fingerprint 比较包括 status、原始 call time、结构化 error/ordered context；
receive time 不参与等同性。新一次实际调用必须新建 frameId，不能用同一 frameId 重试 present。
ConsumerId/RequestId 是 attempt 登记的不可变关联；frameId 在 sessionScope 内唯一，receipt
仍以既有 AttemptKey 和完整 context 查原记录，不要求在回执新增注册字段。若已有回执字段与
原记录不符则报告冲突，不能按当前注册重新解释或投递到新注册。注册退休不替代
projectionScope 的取消/到期语义。
乱序接收的先前 Unknown 在已知终态后只记为旧观察，不把已知终态降级；两个相反的确定终态才是冲突。
NotSubmitted 必须有交付槽证明的“从未移交”或 Renderer owner 的终态未调用证据，
不能被后来的 Succeeded 自动覆盖；
两者并存是协议冲突。Pending 遭遇确定 Failed 后，该 feedback 仍可等待另一合法 attempt 成功，
不能因一次 Failed 直接判整个反馈 ExpiredUnconfirmed。

反馈状态的确定转移保持既有观察语义：

| 原状态与事件 | 新记录 |
| --- | --- |
| Created，当前 scope 收到合格成功 | ConfirmedPresent |
| Created，关联 attempt Unknown 或 coverage 缺失 | UnknownEvidence；后到成功只附 laterConfirmed |
| Created，T 到期且证据完整/无未知提交 | ExpiredUnconfirmed，并冻结到期记录 |
| Created，T 到期但有未决 attempt/覆盖缺失 | UnknownEvidence，并冻结到期记录 |
| Created，控制替换 scope | CanceledByControl，并冻结控制时记录 |
| ConfirmedPresent/ExpiredUnconfirmed/CanceledByControl/UnknownEvidence，后来到期、取消或成功 | 不重写原状态；补独立到期/控制快照或 laterConfirmed；从不复活 token |

反馈义务由已提交的 projection activation 产生，**不由构帧产生**。即使无消费者或没有额度，
created/expiry 仍可观察；journal 无额度则记 coverage loss，不让完整帧承担补充历史责任。
membership 依据本次结果中实际参与表现的反馈，不因截图、重绘、Pause 反复分配新 activationOrdinal。
精确截图/离线导出默认不是 Gameplay present 观察者；仅声明的呈现消费者可以提供确认。
首版只有一个呈现端；未来多呈现端接入后，总体 ConfirmedPresent 仍只表达至少一次合格调用
成功，不承诺所有窗口可见。暂停重绘沿原激活身份，只记录新尝试；已到期/取消的来源成员只能
补 laterConfirmed，不能复活。激活成员表属于已提交投影，journal 淘汰不改变它。

Finite 的 expiry 使用 typed presentation T，区间为 `[created,expiry)`；history TTL、receipt deadline
使用固定 host monotonic clock，二者不能换算替代。到期冻结当时已知状态，晚到成功另记；
不按后来收到的 call time 倒改“到期时已确认”。无呈现但证据覆盖完整才可 ExpiredUnconfirmed；
有缺失、在途结果未知或覆盖不完整时为 UnknownEvidence。
expiry 使用已提交的表现 T 前沿；保留画面的源 T 可以较旧，不能作为新 update 或使该前沿
倒退。已过期成员仍可出现在保留画面的来源 membership 中，但收到成功只补历史证据。

### 7.3 保留与耗尽

journal 分别配置 attempt/feedback/tombstone 条数、总 bytes、retention duration 和最多 coverage
区间数。任何一个上限先到都可以压缩/淘汰最旧证据，必须同步收缩可查询覆盖范围。
Pending/Unknown 历史可在超出保留范围后忘记细节，但不影响独立 CPU lease 和 Renderer 设备追踪。
未到期反馈若因预算被移除，标记不完整覆盖；不得在到期后声称它没有被呈现。

保留窗口内以完整键去重；范围外只能回答 HistoryUnavailable。覆盖区间本身也有上限，空间不足
时合并为更保守的不完整区间，不建无限缺口集合。缺失计数使用饱和计数并报告是否精确；
无法保留身份去重时不把收到消息次数称为独立丢失反馈数。
session/projection/frame/request 序号的唯一性由 checked issuer 保证，不能为了证明“不复用”
永久保存所有旧 scope。当前身份常量空间，旧 scope 诊断与 journal 一同受限。

| 耗尽责任域 | 行为 |
| --- | --- |
| delivery count/bytes/请求额度 | 按 §6 拒绝或合并本次交付，返还未消费 reservation，不改变 Gameplay |
| observation journal / present receipt 通道 | 记 Unknown/coverage loss 与高水位；继续 Playback；不得把纯观察不足转成 Suspended |
| CPU lease 完成登记 | 由独立 storage account 无分配返还额度，不依赖可丢消息；不能遗失归还后永久占槽 |
| 可靠输入/控制/Fact/Replay、必要资源生命周期记录 | 沿既有 host.rpa.capacity_exceeded、拒绝/捕获 gap/Suspended 合同；不丢可靠数据伪造成功 |
| GPU 在途上限或清理未确认 | 停止新的设备交付或按既有 device failure 恢复；保留真实未清理责任，不能用 observation 淘汰释放设备资源 |

当前 FrameObservationLedger 将纯观察满槽返回 capacity_exceeded 的耦合是迁移对象，不是 PD
必须保留的期望结果；外部可见错误/状态映射仍须在首次实现前按正式 Spec/ABI 差异检查。

## 8. Worker、消息顺序与关闭（PD-4）

### 8.1 owner 与队列

| 职责 | 首版 owner / 消息 |
| --- | --- |
| D1 capture、窗口/输入 | 主线程，沿原闭批/时钟协议送可靠输入；不由帧请求驱动捕获 |
| D2 + D3 Judgement/Projection/Runtime | 同一 SessionWorker，顺序提交；不在执行一半时处理控制 |
| admission/PresentationRead/物化/journal | 同一 SessionWorker；独立于推进的请求，受每轮工作额度限制；无需求不遍历 World 捕获表现副本 |
| Renderer/GL 与设备资源 | 主线程；串行处理注册确认/撤销；消费 owning envelope，核对当前注册及完整 context 后实际调用；不读 World/PublicationRead |
| audio/协调控制 | 沿现有 owner 和控制阶段，PD 不重新分配音频线程 |
| CPU 存储释放 | 最后读者或独立 reclaimer；仅执行不抛出的 CPU 释放/计数，不向已关闭 worker 发命令 |

SessionSample 的迁移必须分开“逻辑更新”和“请求交付”。保留旧同步兼容包装时，包装顺序执行
一次更新和一次交付；新主循环不得再同时调用包装和独立更新，以免一次采样消耗两次 lifetime。
冻结状态的呈现只使用最后成功画面；显式 CPU 截图仍可只读当前修订，须标明其来源。
两者都不能再以 frozen sample 执行 Runtime 更新，不能把当前截图当作暂停窗口最后成功画面。
SessionPresentReceipt 改为无等待观察消息；Gameplay advance/query 的确认仍沿可靠协议，不能
把“异步 receipt”扩大为不确认控制结果。

```text
主线程/D1          SessionWorker(D2/D3)             主线程/Renderer
可靠捕获/推进  ->  G1/Q1/K1 -> seal/Fold
                  projection/Runtime/install(修订描述)
交付请求       ->  校验绑定/修订 -> reserve -> 只读构帧
                  记录 attempt / Ready 槽       -> take；校验注册/scope/control/generation
                                                  submit/present
                  <- 无等待 PresentReceipt        结束 CPU 读取，归还 lease
                  独立记录证据                    GPU fence 由设备 owner 追踪
```

receipt 早于 worker 下一次工作轮到达没有关系：attempt 在 envelope 移交前登记；若 journal
无法接纳，则先记 coverage loss，后续 receipt 返回 HistoryUnavailable，不去查已经释放的 payload。
协议有四类独立能力：可靠执行通道、保留控制/故障通道、有界交付响应、可缺失观察通道；
控制不能竞争最后一个普通槽，lease 释放也不能依赖观察队列有空间。
注册/撤销确认沿可靠生命周期设施；Ready 的 take/取消是交付容器同步，不写 Gameplay 状态，
不能依赖观察通道回传来释放槽。Renderer 的注册前沿、帧执行前沿与 Worker 请求去重分别负责
不同输入，不合并成一个 journal；每者有界，字段和维护职责见 §6。
控制推进优先于新交付，历史 receipt 可以越过控制晚到，但只能修改原键历史。
控制 fence 时禁止发布尚未全域确认的新 context 帧；在调用 renderer 前再次核对 context。

### 8.2 Close 的完整有向顺序

1. 停止新请求并应用 K1；关闭 admission，取消尚未移交的交付和未执行逻辑请求，记录实际阶段。
   Registering/Active 注册进入撤销清理，不再接受重新注册；Ready 的取消仍与 take 串行裁决。
2. 固定可取得 archive；候选按 B1 Apply/Cancel 终态处理，不能把 Unknown 当取消成功。
3. 停音频并确认 callback 隔离；保留尚未完成责任域。
4. 各 owner 清理活动对象；主线程处理 GPU/资源 fence、retire 和任何仍需主线程完成的清理。
   Renderer 按 §6.3.1 确认注册资格撤销，迟到激活不能复活它；资格确认与 GPU 完成分别收齐。
   内部交付读者/Ready 槽停止后释放其 CPU lease，请求历史不保留帧，journal 留下受限证据/覆盖范围。
5. 确认 worker 不再依赖主线程服务，再 request shutdown 并 join；不得先同步等待 worker，
   同时让它等待主线程回收资源。回收队列中的设备对象必须在所属 owner 退出前处理。
6. 释放协调器并报告 Closed；外部 CPU owning 帧可继续存活，其 storage account 无 worker 依赖。

Close 超时或设备完成不可确认，保持 Closing 与缺失责任域；不 detach、不借 TTL 强制释放内存。
device lost 只能使用设备 owner 能证明的安全退出机制；缺失证据不能转换为成功 fence。
此序列只是[宿主合同 §5](../../../api/realtime-host-boundary.md)的 PD 落点，不能替代其音频、
资源、候选和控制清理义务。

## 9. 容量 profile、测量与退出（PD-0 / PD-5）

### 9.1 不依赖任意默认值的容量配置

| 配置域 | 必须声明的量 | 上限检查/缺失处理 |
| --- | --- | --- |
| 内容准备 | content/layout bytes、对象/资源数、最大可变字符串、target/binding/group 索引大小 | 沿已有内容预算，新增索引成本单列；超限 prepare 拒绝 |
| 事务/状态 | 当前 Runtime/World、候选状态/变换计划、scratch、当前/候选提交描述 bytes | current + candidate + scratch 完整峰值入账；首版不强制完整结果副本，无历史 World 队列 |
| 消费者 | 最大注册数、每消费者 payload count/bytes、Ready 槽、元数据去重保留、在途 attempt 数、单帧 bytes、request deadline、每轮物化次数/时间预算 | 活动/待确认预留 + 退役实占不大于全局预算；保留加替换峰值入账，复用载荷的重绘仍受 attempt 上限约束 |
| 注册生命周期 | 同端一个转换过程、激活/撤销/确认所需预留槽与 bytes、查询/确认时限、注册前沿与当前结果 | 复用可靠设施且不竞争 Close/故障最后容量；超时保持实际未确认阶段，不回收前沿或凭超时重注册；精确历史受限 |
| observation | attempt/feedback/tombstone/coverage 条数、membership/error bytes、总 bytes、retention duration、receipt deadline | 最先达到的维度触发 §7；TTL 不能用于释放资源 |
| 设备/回收 | GPU 在途数/bytes、各 owner 回收队列数/bytes、CPU 待释放 bytes | 分域度量；异步回收 backlog 不能藏在“已经退休”计数之外 |
| 性能判据 | 目标平台/设备、每指标单位、分位数或最大值、窗口长、允许回归比例/绝对差、测量噪声界、通过条件 | 同时约束逻辑更新、单次物化、控制等待和回收时间；不填写则不能宣称生产或 PD-5 判定通过 |

以上数值由显式 testOnly profile 或经接受的 production profile 提供，均不得以 0/无限制替代缺项。
无消费者时允许禁用 delivery 域；GameplayOnly 不要求配置 GPU 域；启用的域必须完整。
纯观察域可明确禁用，但查询必须声明无覆盖，不能默认提供完整反馈历史。
测试用的小容量是故障/边界输入，不是生产默认；生产 profile 缺项按既有 profile_incomplete 拒绝启动。
已接受的性能阈值与调度工作配额分开，CPU 超时监测不能回滚已安装更新。

### 9.2 基线与对照方法

PD-0 的第一份产物是测量 manifest，包含代码 SHA/差异 hash、工具链/配置、candidate 开关、
static/shared、OS/设备/驱动、电源、VSync、content/hash、输入/Replay hash、时钟序列、viewport、
消费者模式和 profile。旧实现没有共同结果/版本/交付计数时标记 not applicable，不能为满足计时
列表先实施新结构或把缺项填 0。保持逻辑更新节拍一致，交付采样策略单独记载。

每个比较单元预声明 warm-up、采样窗口、样本数和统计口径；最低采用 3 组相同 workload 的独立
配对重复，A/B 交替次序。报告每次及总体 median、p95、p99、max、样本数、分配/复制 bytes、
live bytes/high-water、版本年龄和 coverage loss；样本不足以支持某分位数则明确不可判定。
噪声界和允许回归线必须在候选结果出现前登记；重复结果波动超过噪声界时结果为 inconclusive，
不能反复测到一次较快就通过。测试持续时长和历史压力按声明 profile，稳态覆盖远超交付容量的
发布次数及 retention window，不能仅运行到刚好槽位未满。

正确性使用相同输入、content、时间序列、viewport 和逻辑采样点（T、RuntimeFrame、publishable
cursor、成功更新序号）比较；新路径另记 resultVersion，旧路径不伪造版本。Replay、Fact、
Snapshot payload/digest 与 FrameDigest 使用各自既有版本的**精确相等**，不新增浮点容差掩盖差异。
绘制输出受设备差异影响的证据另列，不用像素差取代逻辑值等价。分段耗时使用不重叠计时边界，
不得将嵌套 Runtime/Projection 时间重复求和。GPU 时间须用适当设备证据，CPU submit 时间不是 GPU 完成时间。

平台矩阵消费 RPA-F1/F2 的 Debug/Release、MSVC/MinGW、candidate ON/OFF、static/shared、headless
及 Linux 支持的 sanitizer/quality 门禁；只对本次受影响路径主张新证据，历史 SHA 通过不代表新 SHA。
真实 GPU/音频/听音/同步与生产 profile 接受仍归 RPA；本次文档修改不运行这些检查。

### 9.3 现有目标、冻结时点和缺失后果

| 项目 | 当前依据 | 后续责任与门禁 |
| --- | --- | --- |
| 内容 / Packed | [RPA F3 基线](../../../stage_reports/stages/stage-07/rpa/2026-10-15-f3-final-baseline.md)：40,000 展开实体、40,000 requirements、16 MiB entry | PD 后同口径重测；不是 PD 后生产限额已接受 |
| Seek | 同一报告的适用 Seek ≤20 ms 目标；端到端测量仍缺 | PD 后补端到端测量边界和报告；不得用序列化耗时或局部 Seek 代替 |
| 分段 CPU/等待/交付/内存预算 | 当前无本架构已接受的生产数值 | PD-0 记录 baseline/testOnly profile；首次实现相应域前确定测试边界；生产交付前由 RPA-F3/owner 接受生产 profile |
| 等价性与故障行为 | §2–8 和既有 Spec/ABI | 各批次必须通过；不因尚无生产预算就允许改变语义 |
| PD-5 拆分触发 | 预登记的 accepted profile、配对测量、持续窗口 | 未有阈值或噪声未受控则“证据不足”，不进入自动架构升级 |

PD-5 只有在 PD-0–4 相应证据齐备、没有已知生命周期/正确性缺陷时，才判断**同一个预算项**
是否在 3 个连续且预声明的有效窗口超界。D2、D3、消费者等待分别比较，不以某次 A 超界、下次 B
超界拼成连续失败。积压增长还需同时超过该消费者已声明的 age/count/bytes 上限；增长本身仅是
调查信号。满足条件只触发独立设计报告，不能自动授权增加 owner/线程；不满足则继续单 owner。

### 9.4 性能取舍与架构回退条件

首版不承诺“新增层一定更快”。令 U 为成功更新数，F 为实际新 Snapshot 物化数，R 为同 viewport
保留重绘数；成本必须分开归因：U×候选状态/World 安装，F×读取/资源映射/复制，R×校验/提交，
另计 receipt、候选销毁与控制等待。不把 Runtime 内的层级计算和外层 update 时间重复相加。
不新增 U×完整 resolved capture，是本轮按需方案相对每更新保存结果方案的结构选择。

| 负载 | 本轮选择的收益假设 | 需要防止的退化 / 再决策条件 |
| --- | --- | --- |
| 无消费者或 U 远大于 F | 不做无用 Snapshot/完整结果捕获 | 描述与候选准备仍有成本，必须单独计数；不能把常量小元数据称为零开销 |
| 单窗口，每更新一帧 | 直接读取并填 Snapshot，避免额外完整中间数组 | 桥接逐对象调用、查表和 staging scratch 可能变慢；与旧路径同 U/F 比较，不用少画帧掩盖回归 |
| 多个截图请求同一修订 | 正确性和独立额度清楚 | 首版可能重复物化。只有重复扫描成为受控瓶颈时再选择有界中性缓存/共享 backing，不能直接新增 owner |
| 长时间 Pause / resize | 同 viewport 重用 backing；不推进 Runtime | resize 可能复制完整 Snapshot；保留旧图与新图峰值、UI 控制响应必须达标 |
| 稀疏变化 / 大场景 | 共享静态定义，保留现有变换 dirty 优化 | prepare/apply 不应无条件复制整个 World/cache；测量触及对象数、scratch 和 topology rebuild |
| 拓扑变更 / 新资源 / 退出 | 可失败工作集中在准备或资源 owner | 冷路径峰值、析构、回收积压及 Close 耗时仍入账，不能藏到“帧外” |

优先优化所有权不变的存储布局、复用缓冲和冗余扫描。若 staged transform 无法在已接受预算内
实现，暂停切换并保留旧实现/已完成的独立批次，形成局部替代设计；不自动使用整 World 克隆、
更大回滚日志或多线程掩盖问题。性能证据可推翻本轮存储选择，但不能推翻语义和设备寿命合同。

## 10. 批次门禁与问题闭合映射

下表表示设计决定已经落到文档，**不是实施已通过**。每批开始前核对所消费的正式接口/诊断差异，
需要改变公共/宿主合同的项目先修订对应 Spec/ABI/版本记录；不得靠本附件覆盖正式合同。

| 审查项 | 本次补齐 | 后续批次必须提供的证据 |
| --- | --- | --- |
| R1 发布视图 | §2 类型、范围、有效期、no-Fold/fault、诊断桥 | PD-1：同源前缀与公共 owning query 对照、失效拒绝、包/依赖边界 |
| R2 投影确定性 | §3 权威来源、处理次序、二元去重、partial-group、过期 | PD-2：既有 golden 等价、索引碰撞、无新 Fact、scope 重建 |
| R3 结果与所有权 | §4 单一状态源、修订描述、中性只读桥、控制绑定 | PD-3：初次提交可提帧、全字段对照、mutation 失效、无历史伪造、提帧不推进修订 |
| R4 事务与恢复 | §5 捕获移出事务、Runtime prepare/apply 与连续 finalize | PD-3：父子矩阵/缓存、storage 安装、逐失败点全部状态和 R2/Seek/Reload/FaultPrefix 回归 |
| R5 交付与背压 | §6 请求/额度/归还、独立消费者、可靠捕获失败边界 | PD-4：无额度不构帧、重复请求不重复 present、注销/Close 后 lease 释放 |
| R6 journal | §7 三维状态、显式有限/UntilReset、迟到/冲突与 coverage | PD-4：极值寿命、长稳态/冻结/旧 scope/丢回执、独立 CPU/GPU/历史上界 |
| R7 线程和关闭 | §8 owner、消息顺序、控制优先、完整 Close | PD-4：无重复更新、无主线程互等、Pending/Unknown 清理、设备失效 |
| R8 判据与退出 | §9 profile 字段、统计法、既有目标、冻结时点、PD-5 条件 | PD-0 基线；PD-3/4 A/B；PD-5 判定报告；RPA-F3 生产接受独立 |
| 本轮 S1 派生回滚缺口 | §5.1–5.2 不增加成功后回滚；派生值在 Runtime 内准备/安装 | PD-3-a 必须先于新的只读物化切换；旧成功点之后无失败操作 |
| 本轮 S2 控制与保留画面 | §4.4、§6.1 独立绑定；源修订允许落后；控制不重建 scope | PD-3/4：Pause drain、Resume、EOF、旧图 resize、回执丢失与资源换代 |
| 本轮 S3 UntilReset | §7.1 显式寿命与公共结构迁移 | PD-4 前先关闭 §11 的 ABI/minor 门禁；有限极值与无期限分别验证 |
| 本轮 S4 多 Renderer | §6.3 首版单端、CPU 多消费者；列明未来协议改动 | PD-4：第二独立 renderer 明确拒绝；截图不改设备绑定；不宣称多窗口已交付 |
| 复审 S5 注册执行许可 | §6.3.1、§6.4 的 ConsumerId、激活/撤销与执行前核对 | PD-4：普通旧帧、旧激活/确认、撤销先到、同端再注册、Close 和 GPU 仍在途；旧副本拒绝不伪造 NotSubmitted |
| 自查 S6 响应与请求历史 | §6.1 的 Ready/take、取消/过期顺序和只含元数据的去重记录 | PD-4：最低双载荷连续换帧、take/取消/过期竞争、重复请求及未领取响应；去重记录不保活旧 Snapshot |
| 跨阶段 X1 能力与发行 | §12.1、§12.7 | PD-2 绑定支持表；7B+/Stage 8 接收实际合同 revision 与迁移证据 |
| 跨阶段 X2 表现域 | §12.2 | PD-3-b 覆盖全部既有字段；后续能力各自提供 typed 读取、完整性、Digest 和预算证据 |
| 跨阶段 X3 Studio | §12.3 | PD 不引入作者事务；Stage 10 验证编辑版本、预览失败和拖动时间取消 |
| 跨阶段 X4 设备与内存 | §12.4 | PD-4 验证保留资格撤销和真实释放；Stage 12 补平台与设备证据 |
| 跨阶段 X5 重建 | §12.5–12.6 | PD 保持同步语义且拒绝未支持能力；Stage 13 另行接受任务、时钟、失败与重建预算合同 |
| 跨阶段 X6 稳定 ABI | §12.7 | PD 标记私有类型与释放责任；Stage 14 接受 opaque handle/allocator/取消/跨 Close 释放合同 |

PD-1/2 可以在后续获得实施授权后消费本附件；PD-3/4 同样须先有授权和相应前置证据。
没有生产阈值不等于接口未定义；它是有明确负责批次和缺失行为的测量门禁。
没有实现证据的批次保持未通过，不能以“已写清设计”关闭 Stage 7-PD。

## 11. 正式合同差异与迁移顺序

以下是明确选定、尚未实施的修订，不覆盖现有已安装合同。新增设计名不自动成为公共类型；
本表没有授予版本变更、产品实施、合并或发布许可。

| 差异 | 权威文档和受影响入口 | 在何处完成 |
| --- | --- | --- |
| UntilReset 不再使用整数 sentinel | 实时宿主边界 §8.2 候选修订、Gameplay ABI 的 PD 迁移节、FeedbackObservation / gameplayFeedback、SDK 版本差异记录 | PD-4 实现前正式接受显式 optional expiry 的不兼容迁移；按 VERSIONING 提升当时 SDK minor、重建 static/shared 和 installed consumer；不预填未来版本号 |
| control-only 不轮换 projectionScope；允许确认后保留帧重绑 | 实时宿主边界 §8.1 候选修订；RPA Pause/Resume/EOF 保持原控制义务；Player finish 与帧校验 | PD-3 控制接入前接受宿主细化；测试证明旧控制帧仍拒绝、同作用域反馈不重建、原来源不改写 |
| 描述/来源与交付许可分离；呈现端本地一次执行 | 宿主边界 §8.1/8.3 候选修订；内部 FrameEnvelope 的来源 absence、SessionSample/Renderer adapter | PD-3/4；完整 HostContext 校验继续存在。若改变安装头或公共错误，先关闭版本门禁，不以“内部优化”豁免 |
| 显式撤销保留画面的后续使用资格 | 宿主边界 §8.4 候选修订；保留身份、可靠生命周期通知与窗口降级状态 | PD-4 暴露该行为前接受压力触发、可观察结局和恢复规则；公共状态/错误若改变，先走版本与消费者门禁。NoRetainedFrame 目前只是内部设计名 |
| 消费注册与普通帧实际执行绑定、响应移交与历史分离 | 宿主边界 §8.3 候选修订；内部 ConsumerId/DeliveryBinding/FrameEnvelope、Renderer 入口与请求响应 | PD-4 实现前接受内部宿主细化并核对接口/诊断差异；不改变公共 HostContext 或 Gameplay identity。若安装头或公共错误实际改变，另走版本/消费者门禁 |

主计划中的 PD-3 分为：a) 事务边界与派生状态完整性；b) 中性只读桥和修订描述；c) 控制绑定/
保留来源接入。PD-4 再接消费者、显式寿命和观察迁移。每个中间提交必须保持旧公开行为或在
新 session/prepare 开关后启用，不允许活动会话并跑两套可变状态。

旧同步包装最终只调用共同的 update 与 materialize；独立旧实现仅在迁移对照期间保留。通过
等价/故障/容量/Release 门禁后删除重复分支，并给出删除清单；历史基线保存在提交和报告中。
延期项必须有重新评估条件与受影响模块，不能以永久双轨降低眼前开发难度。

## 12. 与后续阶段的兼容与设计约束

本节补齐对 [7B+](plan-b.md)、[Stage 8](../../future/stage-08/plan.md) 至
[Stage 14](../../future/stage-14/plan.md) 的依赖核对；选型理由见
[跨阶段设计记录](../../../stage_reports/stages/stage-07/handoffs/2026-10-16-pd-cross-stage-design-review.md)。
这里区分 PD 当前交付义务与未来能力的接入条件，不把未来算法、公共错误名或任务类型写成现有接口。
§2–11 的同步事务、来源与交付规则继续成立；后续阶段不能直接靠本节改变正式 Spec/ABI。

### 12.1 能力演进与 FactBinding

PublicationRead 的 cursor 覆盖完整 Fact 历史；§2 的跳过规则只适用于正式合同明确为无需
表现处理的记录，不授权忽略内容声明必需但 adapter 尚不支持的 tag/phase/outcome/聚合。
新增能力不得把旧 Tap/Hold 的匹配假设外推为通用规则，也不得按枚举未知值走默认成功分支。

prepare 使用所属能力合同的 ID/revision、合法 phase/outcome、绑定 target/effect、聚合规则和
寿命建立明确支持表；不另造 semanticRevision 或预留任意扩展槽。已知但未启用、未知、revision
不匹配、非法组合均在 prepare 稳定拒绝。运行时遇到违背已准备支持表的必需记录时停止本次
projection 提交、保留旧 cursor/状态并报告合同错误；不能跳过后仍声称整个前缀已经消费成功。
capability 不带表现绑定且合同明确允许无表现时，可以不生成视觉效果，不伪装成不支持判定。

7B+ 每份准入包增加“判定能力是否改变发布记录、FactBinding、聚合、Snapshot/Replay、表现
预算”的影响表。新增判定能力由 Judgement 实现，绑定适配由 Playback 实现；不让 Renderer
认识新的判定类型。Stage 8 对 Graph/Packed 两种入口恢复同一绑定支持表与来源 identity，
并验证未知能力拒绝、旧绑定 golden 和 presentation-only effects 不进入判定/Replay identity。

### 12.2 有类型的表现域与唯一物化入口

保留一个 Snapshot 物化协调入口，按现有模块职责拆分有类型的读取/填充函数；不建设动态
provider registry、type-erased 属性包或公开插件接口。PD 只整理已有对象/相机路径；Environment、
Model/Submesh、曲线、后处理、粒子在所属阶段首次接入时增加实际类型和依赖，不提前创建空模块。

| 扩展职责 | 必须具备的合同 | 不能转移的责任 |
| --- | --- | --- |
| Runtime/表现域求值 | 准备定义、唯一 evaluator、候选与无失败安装；依赖域及确定顺序 | Snapshot 读取不推进动画、粒子、RNG、Checkpoint 或 lifetime |
| 中性读取桥 | 同一提交修订的 typed 值、presence、资源描述；覆盖对象之外的环境/几何 | 不泄漏 World/EnTT，不用 JSON、任意字典或设备对象传递未知字段 |
| Playback 物化 | 在同一只读窗口填目标 Snapshot；有序调用域函数，集中 viewport/资源映射与错误次序 | Renderer 不重新求值或合并不同时间的域；某域失败则本次完整交付失败 |
| Snapshot/Digest/后端 | 安装字段、capability、规范顺序、资源闭包、预算与各自版本 | Snapshot 扩展不隐含修改旧 Digest 算法；不支持内容不能静默忽略 |

一次完整可交付修订要求全部内容声明必需的表现域来自同一逻辑采样点和兼容内容/布局。
未变化的域可以共享不可变数据，但其适用性由同一次提交证明；不能拼接“相机 T2 + 粒子 T1”
并贴成 T2 完整帧。域元数据是该次提交的完整性说明，不是每域独立发布队列或版本历史。
无某能力的旧内容不要求该域，不分配对应运行状态；合法旧输入继续使用原 Digest 版本与值。
这里的完整性不保证任意既有可变回调都产生合法可交付数据；§4.3 仍发布回调后的实际状态，
物化仍可因缺字段、资源或相机错误失败，不能把完整域要求当成自动修复或回滚任意回调的承诺。

Stage 11 的绝对 Beat 曲线由唯一 evaluator 求值或输出已冻结的 portable 参数；是否由后端执行
几何展开须由该阶段 Spec 定义、用 headless oracle 证明一致，不能因此给 Renderer 判定权限。
Stage 13 粒子推进由 §12.5 的候选过程负责，读桥只读完整结果。viewport 投影允许由同一保留
值重算；其他依赖分辨率的效果必须声明输入、额度和重算规则，不默认继承当前仅相机的 resize 路径。

此选择增加显式字段接入工作，但把变化限制在所属域、协调入口和正式 Snapshot/后端消费者。
首版不为每对象进行动态虚调用，也不引入每消费者独立 evaluator；以后只有测量证明重复读取
成为瓶颈才评估有界派生缓存，缓存不得新增写入权威。

### 12.3 Studio 作者事务与预览事务

Stage 10 的权威作者文档、Undo/Redo 和保存，与 Playback 的运行状态分别拥有各自事务。
mutation guard 只保护已有运行写入口，不能提供作者撤销或允许 Studio 直接写 World。

作者编辑先产生新的文档 revision，再用共同 typed compile/prepare 路径构建候选预览。
候选必须携带文档 revision、资源/编译 profile 和请求身份；软件安装前在提交 owner 边界复核
仍是当前请求。尚未进入提交的过期候选不覆盖活动预览；一旦开始产生控制副作用，就必须按
原操作身份收齐实际阶段，不能因作者又编辑了新版而假定旧操作已取消。作者编辑与离线编译
可继续；运行候选的 prepare、接纳与安装仍服从既有生命周期、控制串行规则及容量，
不覆盖尚未收束的操作。

“保留旧预览”只保证失败没有替换旧软件状态，不额外保证旧设备始终可用。内容替换沿
[宿主合同 §3](../../../api/realtime-host-boundary.md)的资源准备、激活与控制协议，分阶段处理：

| 失败或完成位置 | 软件、设备与预览结局 |
| --- | --- |
| 离线编译/prepare 失败或尚未进入提交的候选被替代 | 不替换旧软件预览；旧画面仅在自身注册、context 和资源许可仍有效时可用 |
| 提交前取消/拒绝，所有相关 owner 已确认终态未生效，且没有其他控制副作用 | 旧组合保持；按原协议解除相应 fence，不把发出 Cancel 当作未生效证明 |
| 激活/取消结果不明、超时或已经发生控制副作用 | 保留实际阶段与未确认责任，按既有控制合同等待查询/清理或进入 Suspended；不恢复旧组合的运行/呈现许可 |
| 软件已提交新预览，随后 GPU/audio 激活失败 | 保留新软件提交及失败回执，进入 Suspended；不回滚到旧软件预览，也不把旧 CPU 图当作合法活动预览 |
| 全域激活确认成功，但新帧物化或首次 present 失败 | 新组合保持；按交付/设备失败合同处理，不回滚作者文档或软件提交，不冒充已有新画面成功 |

作者文档 revision、实际已提交的软件预览 revision、最后成功画面的来源及当前可用状态分别
追踪；它们复用已有提交描述、控制回执和 last-success 证据，不再建立 Studio 自有提交状态机。
UI 可显示“文档已更新、预览待恢复/尚无成功画面”，不能把最后成功帧的版本当作当前软件版本。
编译、激活或呈现失败均不自动撤销合法作者编辑；保存事务沿原子输出，失败不破坏上一份有效文件。

结果不明时先按原操作查询/收束；恢复只使用既有合同允许的设备重试、Stop/reset、重新准备
或 Close。允许重试已提交操作的设备阶段时绑定原操作及代数，不重复软件提交或 Gameplay 历史。
选择旧文档重新建立预览也须走新的合法 prepare/控制事务，不是撤销已发生副作用；本阶段
不为“失败时旧预览必定继续可用”增加常驻第二 session/Renderer 或跨设备回滚协议。

同一内容内拖动时间走既有 Seek；若未来接入可取消重建任务，再沿 §12.5 处理。合并尚未接纳
的 UI 请求可以减少工作；已接纳的可靠控制必须有实际终态，不能把后来的拖动解释为此前
Seek 自动取消成功。取消旧候选不取消已经提交的 Judgement/Replay 历史。

Studio MVP 可使用当前单呈现端；多视口需求需先选择：独立 session 使用同一求值实现，代价是
重复运行状态与同步；或共享 session 增加独立端，代价是 §6.3 的 RPA 参与者与资源协议扩展。
本计划不把多视口定为 Stage 10 隐含前置，也不提前选其中一种。两个方向均不得创建第二套语义。

### 12.4 保留画面、设备失效与内存压力

保留 CPU backing、允许再次 present、GPU 使用完成是三个独立条件。内存压力不允许强制释放
外部 lease 或设备在途资源；只能由各 owner 撤销后续使用资格并释放其自己的引用。

| 情形 | 允许动作与可观察结局 |
| --- | --- |
| 控制和资源不变的普通 Pause/resize | 沿 §4.4、§6 保留最后成功画面；额度不足不换成当前状态冒充旧画面 |
| 明确的内存压力策略要求丢弃保留画面 | owner 在串行边界停止其新重绘、取消未移交重绘、释放自身引用；在途/外部读者继续计费。后续返回 NoRetainedFrame，不能承诺精确保留画面仍可重建 |
| device/context/resource generation 失效 | 立即撤销旧呈现资格；CPU 值即使仍在也不能跨代直接重贴绑定。设备 owner 按现有失效/清理规则处理 GPU，不把失效当 fence 完成 |
| 新设备已完成全部资源/控制确认，运行状态允许更新 | 新合法更新后可物化当前状态并签发新 attempt；期间不把旧 backing 冒充已恢复呈现 |
| 新设备已确认，但仍处于 Pause 或精确保留版本已不存在 | 首版保持“无可呈现保留画面”的显式状态；宿主可按平台规则清除失效表面或显示内容外的恢复提示，不能登记 Gameplay present 成功，也不调用 frozen Runtime 更新 |

最后一行是首版明确限制：例如当前 v12、最后成功 v10，v10 backing 丢弃后不使用 v12 替代。
同代缺图在合法 Resume/更新后恢复；跨代恢复精确暂停画面若成为产品要求，需另行接受从完整
portable backing 重新取得资源的入口、来源/membership 校验、容量与宿主合同，不在本轮隐含授权。
无保留图不等同于控制已确认、设备已恢复或直接续跑资格恢复；这些仍取决于 RPA。

撤销在 Renderer owner 的串行边界生效。每个保留身份绑定 ConsumerId（注册 incarnation）和单调保留序号，
不复用旧身份；实际调用前同时复核其仍是有效 last-success、来源和当前 DeliveryBinding。
保留序号只区分该注册内的最后成功画面，不是第二套消费注册代数；普通帧同样受 §6.4 的
注册校验约束。仅撤销保留画面不注销仍有效的消费注册，后续合法新帧可重新建立保留身份。
Worker 尚未收到撤销通知也不能使已撤销身份再次 present。取消未移交重绘；已移交但尚未调用
的 attempt 在 Renderer 复核时拒绝；已经进入调用的操作沿自己的 CPU lease 和设备责任结束。
通知沿已有可靠生命周期/控制通道，以有界记录传递身份和原因并确认处理，不借可丢 receipt
传播资格变化；通道耗尽沿可靠通道既有失败策略，不能丢弃通知后继续承诺重绘。通知不附带
完整帧。双方释放各自保留的引用，外部/在途引用仍独立计费；只有真实释放后才归还相应 bytes。
被撤销身份的迟到消息不能恢复资格，也不能清除后来建立的新身份；通知只作用于匹配身份。
后续合法更新的实际成功呈现才建立新的保留身份。

注册时最低双载荷额度依旧适用；若平台只能提供更小预算，首版拒绝该模式的配置。
未来不保留画面的模式须另定注册、控制与保证，不能从压力降级隐式产生。
不能先接受其保证再在普通稳态随机驱逐。压力策略是可观察的降级，不是成功释放的保证。
后台恢复、Context 丢失、跨后端切换的真实证据归 Stage 12；PD-4 只承担本架构的失效/额度语义。

### 12.5 有界重建与完整可交付修订

本节是 Stage 13 的接入约束，不给现有同步 update/Seek 增加 Pending/rebuilding 返回值。
PD 当前只有完整提交；不分配粒子状态、Checkpoint 或任务队列。未来引入有界重建必须先在
所属 ADR/Spec/ABI 定义显式任务入口、查询/取消、错误和版本迁移，不能更改旧 API 的成功含义。

选定“候选内分段重建、完整后一次提交”的方向。目标描述固定内容/布局、目标 ChartTime 或
LocalTime、时间域/discontinuity、请求身份、所消费 Fact 前缀及相关资源/capability revision。
重建进度、固定步长/RNG/Checkpoint 由该域唯一拥有；progress 不增加 resultVersion、不推进
projection cursor、不扣 RemainingFrames，也不能形成可成功提取的当前帧。
分片只写候选，不暂写可见 World。候选可以复用受影响域的独立存储，不要求整 World 克隆或
每次普通更新复制全部值；与未变化域共享的数据必须不可变，且其目标适用性需在提交前验证。
最终 prepare 包含同一目标的无状态域和有状态候选、派生缓存及全部可失败工作，之后才安装。

| 任务结局 | 软件状态与交付 |
| --- | --- |
| 准入/准备失败 | 保持原完整状态；报告未接纳或实际失败；不推进目标时间 |
| building | 同一 owner 每个有界工作片后处理控制；候选域状态、scratch 与仍保留旧状态全部计费；公共只读桥仍看原完整修订 |
| ready → commit | 一次性验证目标/控制仍有效；全部必需域准备完成，再走 §5 的无失败安装/finalize，发布一个新修订；进度步数不作为成功 Runtime 更新次数 |
| 取消/被新目标取代 | 安装前丢弃候选并报告实际终态；已提交后取消返回已完成，不能撤回；不影响实际已提交 Fact/Fold |
| 超过总工作/内存/deadline 或域失败 | 明确失败并保持旧完整值；来源/控制已经失效时旧值也不可呈现；不无限循环重试同一任务或默默省略粒子 |

每会话最多一个重建候选；尚未接纳的最新 UI 目标可以合并。接纳新目标必须先终结旧候选、
核实其清理额度，不能保留无限排队任务。终态裁决与物理销毁分开；大候选的取消/销毁须受
owner 单次工作预算约束，必要的 CPU 回收积压有独立上界并继续计费，不能取消后立即返还
字节额度。设备清理仍在设备 owner，不能把任意析构丢到通用回收线程。Checkpoint 可丢弃且
有界，不能作为 Replay 语义来源；
重建输出须由相同输入、版本化 RNG 和目标绝对时间复现。对同一候选重建的继续工作不重复
执行已提交 Gameplay 更新或消耗帧寿命；最终时间推进的具体语义由 Stage 13 明确一次。
重建占用期间与候选冲突的可变操作须由可靠控制裁决为等待、替换或拒绝，不能暗中改变候选
依赖后仍继续提交。非冲突读取可继续；若现有运行模式必须持续接受冲突推进，则该任务模式
不得准入。恢复/Seek 的最终来源不能冒充普通 RuntimeUpdate，进度步数不增加旧 update ordinal。

显示旧完整帧仅在其原来源和当前控制仍合法时可沿 retained 路径进行，并标明它不是目标完成。
Latest 指最新且符合请求 session/control/scope 的已提交状态，不指未完成目标；Exact 目标尚未
提交时不可得。公共读取可能仍看到旧实际值，但不能为其改贴目标 T/context 来满足交付请求。
Seek/Reload 已替换作用域后旧帧不能跨作用域重绑；此时内容区无目标成功画面，任务进度只属于
宿主 UI。重建完成也不自动清除设备 fence，仍须取得 DeliveryBinding。

实时与离线策略分开：离线/显式预览可以有界等待；活动实时正常更新保持同步与既定预算，
不能因粒子重建默默延迟可靠输入、音频或推进承诺。准入须覆盖最坏正常推进成本；运行超界
必须报告并沿所属已接受的失败/暂停恢复策略处理，不自动冻结旧画面而继续声称整局实时合格。
实时 Seek 的显式重建仍由 RPA 控制管理冻结/时钟与输入许可；任务不独自决定继续游戏。

### 12.6 设计约束的反例检查与后续验收

以下是文档层面的反例及预期，不是新模拟实施、测试或性能证据：

| 反例 | 必须得到的结果 | 首次提供实施证据的责任 |
| --- | --- | --- |
| 新 capability 产生 adapter 不懂的必需 phase | prepare 拒绝；运行不变量破坏则本次不提交，不静默跳过 cursor | PD-2 / 7B+ |
| Submesh 数增长但仍用旧单帧字节上界 | prepare/准入预算更新，否则拒绝；不能构帧后越额补账 | Stage 9 |
| 相机已到目标 T2、粒子候选仍在 T1 | 不发布 T2 完整修订，不混合跨目标数据 | Stage 13 |
| 拖动 A 的慢编译在 B 已安装后返回 | A 的文档/请求身份失效，丢弃候选，B 保持活动 | Stage 10 |
| 文档编辑成功但预览 compile 失败 | 文档与预览版本分别可见，旧预览不被破坏 | Stage 10 |
| 旧预览 A 被 B 软件提交替换后，B 的 GPU/audio 激活失败 | 保留 B 的软件版本和实际失败回执，Suspended；不声称 A 仍是活动组合，不撤销作者编辑 | Stage 10 |
| 作者 C 编辑到来时 B 已产生控制副作用，取消 B 超时 | 收束原 B 的实际阶段，不把超时当未生效；C 不覆盖未收束控制，不重复 B 软件提交 | Stage 10 |
| 新组合全域确认后，第一次物化或 present 失败 | 新软件版本保持；画面成功状态独立报告，不以旧帧来源冒充当前预览 | Stage 10 |
| v10 保留图丢弃、当前 v12、仍暂停 | NoRetainedFrame，不冒充 v10，不执行额外更新 | PD-4 / Stage 12 |
| 保留身份已撤销，但旧重绘/撤销通知晚到 | 调用前拒绝旧重绘，通知只作用于匹配身份，不清除后来新图 | PD-4 |
| 旧设备仍有在途使用且内存压力到来 | 仅撤销资格/释放自有 CPU 引用；缺 fence 证据不能回收 GPU | PD-4 / Stage 12 |
| 重建期间新 Seek/Close 到来 | 有界片后接控制、候选有终态，已提交状态不回滚；清理责任纳入 Close | Stage 13 |
| 旧目标取消后其大候选尚未物理释放，新目标到来 | 旧存储继续计费，只有清理与新候选额度均满足才准入；不在 owner 上无界析构 | Stage 13 |
| 查询跨 Close 保留的 C handle | 沿所属 allocator 与明确有效期读取/释放，不访问已关闭 worker/设备 | Stage 14 |

### 12.7 跨阶段交接、版本与维护成本

PD 退出交接包必须列出实际采用的合同 revision、SDK/安装消费者矩阵、未接受提案、删除的迁移
分支、性能/内存 manifest 及限制。后续阶段按实际集成基线消费；不能读到计划中的 proposal
就假定旧安装 SDK 已提供这些行为。

| 接收阶段 | 可复用的边界 | 必须增加的工作 / 依赖处置 |
| --- | --- | --- |
| 7B+ / Stage 8 | 唯一 Fact 发布源、ProjectionAdapter、共同 Graph/Packed 路径 | 绑定支持表及能力 revision；若所用基线含 PD，须纳入 PD 合同/版本/预算重验，不复用旧 F4 作为新实现证据 |
| Stage 9 / 11 | 单一 evaluator、typed read、Snapshot 物化协调入口 | 环境/模型/形变对应域；正式 Snapshot/Digest/capability 与资源闭包。增加类型和调用点，不建通用插件框架 |
| Stage 10 | Playback prepare/控制/帧协议 | 作者、软件预览与画面来源分开；按未生效/已生效/未知阶段处理失败与取消；多视口按需求另行选择并重测内存/控制成本 |
| Stage 12 | 数据来源与设备许可分离、独立额度/释放 | Android 压力与后台策略、Context 丢失、后端恢复及设备实测；更换后端不改逻辑 identity |
| Stage 13 | 完整提交、单一读取与来源身份 | 接受 §12.5 的任务语义、域候选安装、Checkpoint/重建/时钟合同；需要保留旧完整值和候选的额外峰值，不是零成本接入 |
| Stage 14 | owning 值、owner/重入规则、跨 Close CPU 释放边界 | opaque handle/allocator/取消/有效期和纯 C consumer；不把 shared_ptr、span、PresentationRead 或 C++ optional 的二进制布局当成稳定 ABI |

RPA-F4 与 PD 不建立互相等待的环：若 PD 在 F4 之前进入最终基线，F4 消费该基线重测并交接；
若 F4 已接受后再集成 PD，则保留旧报告并补新 SHA 的受影响矩阵/预算和 owner 接受记录。
7B+ 对受影响接口的产品集成、Stage 8 对该基线的正式发行，必须消费完整补充交接，不能以
“内部优化”豁免；研究和未受影响的设计准备不新增全局串行依赖。PD 不以 Stage 8–14 实现为
退出条件，后续能力也不得反过来要求 PD 预先实现它们。

PD 内部类型保持私有，公共字段变更先走 §11。未来稳定 ABI 可将 owning 存储包在不透明
handle 下，但具体 allocator、释放函数、线程与取消结局仍须 Stage 14 接受；内部引用计数
不自动保证跨 CRT/动态库卸载安全。未来 C++ wrapper 与 C consumer 共同调用同一语义路径。

维护成本的验收不是“接口足够通用”：新能力需列出必改模块、依赖域、预算和错误传播。
没有证据的未来扩展保持未交付；出现跨域安装无法有界或控制无法在片间响应的需求，先重审
局部存储/任务合同，再评估 owning 捕获与 owner 拆分，不直接升级为多线程或整 World 多版本。
