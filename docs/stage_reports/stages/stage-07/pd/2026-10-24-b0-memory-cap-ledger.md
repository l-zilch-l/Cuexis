# 2026-10-24 B0 静态内存上限账本与逐域上限推导

**状态**：B0 推导完成并经 owner 复核接受（2026-10-24，G1/G3/G4/G7 四项 blocking 缺口全部关闭，G9 顺延至 B1 计数）。**运行**：否，本步未执行任何工作负载，也未用任何实测值作为上限来源。**PD-5 acceptance**：not passed。

## 1 边界与判据

- 本步只做静态推导：逐单元上限 = `profiles.json` 已声明的容量 × 已审计元素尺寸 + 固定开销（[读候选与内存域方案](../../../../stage_plans/active/stage-07/pd5-read-candidate-and-memory-domains-plan.md) §3.5）。
- 实测值只能用于核对，不能作为上限来源；`global_cpp_new_envelope` 一类诊断包络不进入本账本。
- 0 与「无限制」都不得充当缺项占位：缺项必须写成 `NA:<原因>` 并进入 §4 缺口表。
- 本步不产生预算结论，也不改动[测量方案](../../../../stage_plans/active/stage-07/pd5-measurement-proposal.md) §5 的任何一项；它只把「清单未填齐」变成「逐项可判定：已声明，或明确缺什么」。

## 2 权威输入

| 输入 | 位置 | 摘要 |
| --- | --- | --- |
| 逐内容声明清单（14 项） | 测量方案 §5 L119–121 | 逐内容、逐单元 |
| 推导规则与 schema | 方案 §3.5 | `pd5.memory.caps.v1` |
| 容量注册表 | `tools/verification/playback_performance/profiles.json` | sha256 `834d9654…`，14 profiles，逐 profile 声明 `content_entities` |
| 推导脚本 | `tools/verification/playback_performance/memory_caps.py` | sha256 `68e5c4fd…`，生成物 `derived_from` 可复算 |
| 元素尺寸审计脚本 | `tools/verification/playback_performance/audit_element_sizes.py` | 编译期 `sizeof`，只读头文件 |
| 元素尺寸结果 | `tools/verification/playback_performance/element_sizes.v1.json` | 4,983 B，sha256 `6788ec77…`，`--check` 可复现 |
| 生成结果 | `tools/verification/playback_performance/memory_caps.v1.json` | 280,546 B，sha256 `a0fb8c68…`，`--check` 可复现 |

脚本只读 `profiles.json`、`element_sizes.v1.json` 与自身，输出固定顺序 JSON，无时间戳、无随机量；`--check` 断言生成结果与文件逐字节一致，因此 owner 复核的对象是可复算的推导，而不是一份手填表。元素尺寸审计脚本用与 Release profile 相同的 MSVC 工具链、语言标准和宏定义编译一个只含 `sizeof` 的翻译单元，`--check` 重新编译并比对，尺寸不是手工抄写的常数。

### 2.1 冻结声明（B0 不可变登记）

owner 接受之后本账本即冻结，构成 B0 的不可变登记：

- `profiles.json` 冻结在 sha256 `834d9654…`，`memory_caps.py` 冻结在 `68e5c4fd…`，生成物 `memory_caps.v1.json` 冻结在 `a0fb8c68…`（280,546 B），元素尺寸冻结在 `6788ec77…`。
- 任一项变化都会让 `memory_caps.py --check` 或 `audit_element_sizes.py --check` 失败；失败即表示账本已被改动，必须重新推导并重新复核，不得就地改 sha 把旧复核冒充为新复核。
- 冻结对象是**上限来源**，不是判据：测量方案 §5 的清单、1.10 与 15%、三窗口协议均未被本账本改动，也不因冻结获得任何放宽；本账本没有重跑、没有替代任何旧证据。
- 冻结不产生运行授权：pilot、3×3 正式窗口与 B2/B3 采集仍各自需要显式授权；B2/B3 的 `binding.json` 与不可变回执按方案 §3.7 在运行时另行产出，不由本账本预支。

## 3 账本口径与结果

锚点 profile 取 `cpu-capture-2-v1`（2 consumers、每帧 1 MiB 计费、8 MiB 域、content 1 实体），逐单元 28 个，其中 25 个已定界：19 个由已声明容量直接定界（下表），5 个 `state.*` 由 owner 接受的逐实体计费定界、`age.max` 由逐域 age 上限导出（§3.2、§5）。

| 单元 | cap_kind | 上限 | 来源 |
| --- | --- | --- | --- |
| `consumer.slots` | count | 2 | `capture_consumers` |
| `delivery.payload` | both | 8 个 / 8,388,608 B | `global_payloads` / `global_bytes` |
| `delivery.frame_bytes` | bytes | 1,048,576 B | `frame_byte_bound`（非累加） |
| `delivery.attempt` | count | 64 | `global_attempts`（字节别名到 payload） |
| `delivery.ready` | count | 2 | 每消费者一个 ready 槽（字节别名到 payload） |
| `delivery.receipt` | both | 128 个 / 29,696 B | `receipt_capacity` × 232 B |
| `journal.attempt` | count | 4 | `journal_attempts` |
| `journal.feedback` | count | 4 | `journal_feedback` |
| `journal.bytes` | bytes | 16,384 B | `journal_bytes` |
| `observation.tombstone` | count | 2 | `tombstones` |
| `observation.coverage` | count | 2 | `coverage_intervals` |
| `command.session` | count | 80 | `session_commands` + `control_reserve`（字节侧见 G9） |
| `command.host` | count | 16 | `host_commands` |
| `command.capture` | count | 80 | `capture_commands` + `capture_reserve` |
| `steps` | count | 160 | `steps` + `step_reserve` |
| `history.request` | both | 64 个 / 13,312 B | `capture_consumers` × `request_history` × 208 B |
| `retention.window` | duration | 1,000,000 µs | `retention_us` |
| `deadline.receipt` | duration | 5,000,000 µs | `receipt_deadline_us` |
| `deadline.reply` | duration | 5,000 ms | `reply_deadline_ms` |

已定界单元的字节上限之和为 **8,448,000 B**（payload 8,388,608 + receipt 29,696 + history 13,312 + journal 16,384）；4 个 host profile 走 `defaults`，同口径为 **67,208,192 B**。加上 §5 已接受的 `state.*` 13,312 B/实体与五项固定开销，proposal 时代的 8,448,000 B 变为 **8,788,992 B**（`cpu-capture-2-v1`），host profile 变为 **67,549,184 B**（`cpu-stream-v1`）。这些数**不是**进程内存预测，只是已声明上限的算术和：command 字节、GPU/audio 域仍不在其中。

逐 profile 差异只有四处，均可从 `profiles.json` 直接读出：矩阵 profile 覆写 `global_bytes` 为 8,388,608 B（`defaults` 为 67,108,864 B）；`capture_consumers` 取 0 / 1 / 2 / 4，它同时决定 history 槽数（每消费者 32 槽）；`update_ratio` 在 `low-delivery` 为 8；`content_entities` 逐 profile 声明，除 `cpu-sparse-v1` 为 200、`cpu-topology-v1` 为 201 外均为 1。矩阵 profile 的容量自洽可交叉验算：8 payload × 1 MiB = 8 MiB = `global_bytes`，即「每帧 1 MiB 保守计费」与「域字节上限」两个声明互相吻合，说明这组数值不是随手填的。

两条口径约束必须显式记录。第一，别名单元不重复计字节：attempt 与 ready 持有 payload 引用，其字节计入 `delivery.payload`，账本里明确标注 `alias_of`，分析器（B1）必须实现该口径，否则会重复计入。第二，账本是 profile 声明 `domains` 的超集：例如 `cpu-no-consumer-v1` 只声明 SDK 与 delivery 域，但账本仍给出 journal/receipt 上限；B1 对这些域的要求是「必须读数为 0」，而不是「不检查」。

### 3.1 元素尺寸审计（G2 闭合）

`delivery.receipt` 与 `history.request` 原来只有 count 上限：元素尺寸未审计，字节侧只能记 `NA`。本次用 `audit_element_sizes.py` 编译一个只含 `sizeof` 的翻译单元，尺寸取自真实类型定义，与 Release profile 同工具链、同标准、同宏定义：

| 类型 | 字节 | 界定的账本单元 |
| --- | --- | --- |
| `std::optional<PresentReceipt>` | 232 | `delivery.receipt` 槽（`receiptCapacity` 定容） |
| `std::optional<RequestRecord>` | 208 | `history.request` 槽（每消费者 `requestHistory` 定容） |
| `PresentReceipt` | 224 | 上者去掉 optional 标记 |
| `DeliveryFingerprint` | 160 | `RequestRecord` 成员 |
| `DeliveryInfo` | 40 | `RequestRecord` 成员 |
| `SessionCommand` | 37,680 | `command.session` 队列元素（variant 最大分支） |
| `ControlReceipt` | 35,928 | 每操作控制证明（不按容量数组计费） |
| `ControlRequest` | 616 | 控制请求 |
| `StepRequest` | 616 | 每步请求 |
| `StepReceipt` | 872 | `ControlReceipt::steps` 槽 |
| `HostContext` | 80 | 上述类型的共同内嵌 context |

四条口径必须一并记录。第一，`RequestRecord` 是 `FrameDelivery` 的私有嵌套类型，审计用成员顺序完全相同的镜像聚合（`DeliveryFingerprint` + `DeliveryInfo`）；两者都是非虚标准布局聚合，标准保证大小与对齐一致，因此 208 B 是精确值而不是估计。第二，容器节点、`vector` 冗余容量、分配器头部不在元素尺寸内，由上层容量声明与实际分配行为分别承担；账本不自称覆盖分配器开销。第三，`history.request` 是**每消费者**一份定长 history（`next.history.resize(limits_.requestHistory)` 在消费者构造里），所以上限写成 `capture_consumers × request_history`，原先把 32 当总数会少算消费者倍数。第四，`ControlReceipt` 的 35,928 B 不是容量数组：它按每次控制操作存活，同时只有一个控制在飞，因此不产生 `128 × 35,928` 这样的单元；但它作为 `SessionCommand` 的 variant 分支进入 session 队列，使 80 槽的队列元素下界约 3.0 MB，这条进入 G9 而不是伪装成已定界。

### 3.2 逐域 age 上限（G3 闭合）

方案 §3.6 要求 age 判据使用**逐域声明值**，而不是观察器 `age_limit_us=900000000` 的全局默认：40k 单侧约 30 分钟的运行里，加载期对象的存活时间必然超过 900 s，沿用全局值会把合法稳态判成违规。因此每个域的 age 上限只从 `profiles.json` 已声明的期限推导：

| 域 | age 上限 | 来源与理由 |
| --- | --- | --- |
| `delivery.*`（payload / attempt / ready / receipt） | 5,000,000 µs | `receipt_deadline_us`：超期未确认的交付单元已是协议违约 |
| `journal.*` | 5,000,000 µs | `receipt_deadline_us`：attempt lease 与回执同批回收 |
| `consumer.slots` | 5,000,000 µs | `receipt_deadline_us`：槽由回执期限回收 |
| `retention.window` | 1,000,000 µs | `retention_us`：保留窗口本身即声明值 |
| `state.*` / `command.*` / `steps` / `observation.*` | 无时长上限 | session 生命周期：上限由「after-close 产品域归零」而非时长约束 |
| `history.request` | 无时长上限 | 定长环按位置覆盖：稳态 live 由 count 上限约束 |

`age.max` 因此从 `NA` 转为 `covered`，取各域上限的静态最大值 5,000,000 µs。**无时长上限不等于无上限**：脚本要求每个常驻分配域要么给出 `age_cap_us`，要么显式声明 `lifetime: session` 或 `lifetime: ring`，`validate()` 对两者都做断言，缺一即失败；报告里不能出现「没写就是不限」。

## 4 缺口表

3 个单元未定界（`gpu.inflight`、`audio.live`、`command.session_bytes`），归入 7 个缺口，其中 4 个已关闭。`blocking` 表示在该缺口关闭前不允许采样。

| 缺口 | 影响单元 | 内容 | blocking | 状态 |
| --- | --- | --- | --- | --- |
| G1 | `state.*` 五项 | content 规模（entities）未在 `profiles.json` 声明，现值取自 probe harness（1 / 200 / 201） | 是 | **已关闭** 2026-10-24：逐 profile 声明 `content_entities`，脚本不再内置规模表，缺失即报错 |
| G3 | `age.max` | 逐域 age 上限未声明；不得沿用观察器 900 s 全局默认 | 是 | **已关闭** 2026-10-24：见 §3.2 |
| G4 | `state.*` 五项 | 逐实体计费值只是 proposal，需 owner 接受后才成为 cap | 是 | **已关闭** 2026-10-24：接受 13,312 B/实体，五项转为 `covered` |
| G5 | `delivery.payload` | storage charge 与 requested 的声明比例未登记；56 B/块是诊断实测，不能作 cap | 否 | 保持开放，分析器按 `requested` 判定，storage 只报告不判定 |
| G6 | `gpu.inflight` `audio.live` | 设备域按 B4 另行登记；本阶段不采样 | 否 | 保持开放，B4 前两项恒为 `NA` |
| G7 | 全部 | 40k 固定的 `global_cpp_new_envelope` 与逐域上限之和的关系未裁决（方案 §3.8 三选项） | 是 | **已关闭** 2026-10-24：选项 2+3，见 §6 |
| G8 | `delivery.attempt` `delivery.ready` | 别名不重复计口径需在分析器实现 | 否 | 保持开放，B1 实现要求 |
| G9 | `command.session` | session 队列元素是 `OwnerChannel` 的私嵌套 `Record`，静态不可达；variant 本体 37,680 B ⇒ 80 槽下界约 3.0 MB，字节上限未登记 | 否 | **顺延 B1**：由计数读出字节后补登，不阻塞 B1 |

G2（receipt、request history、queued command 的元素尺寸）已由 §3.1 关闭。G9 不属于测量方案 §5 的十四项，不阻塞采样，但 3.0 MB 量级必须由 B1 计数确认后在账本中补登。

方案 §5 的 14 项清单当前覆盖 13 项，仅 `gpu_inflight` 未覆盖（设备域，B4 登记）。这张覆盖表由脚本计算，不是手写结论。

## 5 `state.*` 的计费值（G4 已接受）

G1/G4 需要的具体数值在复核时被 owner 接受，因此这五项从 `proposed_cap_requested_bytes` 迁到 `cap_requested_bytes`、覆盖状态转为 `covered`。它们是**声明式设计预算**，不是实测高水位，也没有用 40k 归因数字回填。

| 单元 | 每实体 | 固定 | 依据 |
| --- | --- | --- | --- |
| `state.world` | 4,096 B | 65,536 B | 组件 sizeof 之和小于 1 KiB，乘 4 倍声明的 sparse-set/registry 余量 |
| `state.current` | 2,048 B | 65,536 B | 每实体一个已发布 runtime 句柄槽 |
| `state.candidate` | 4,096 B | 65,536 B | candidate 等于 current 加在途 transform plan |
| `state.scratch` | 2,048 B | 65,536 B | 每次 update 的临时 gather/scratch 缓冲 |
| `state.index` | 1,024 B | 65,536 B | target/binding/group 索引每实体一项 |

合计 **13,312 B/实体**；content 1 实体时为 340,992 B（含 5 × 65,536 B 固定开销）。`state.*` 的逐字段元素审计（`FrameSnapshot`、`GameplayClosedBatch` 等）仍未做，但已不再是 blocking 项：它只会把 4,096/2,048/1,024 这些声明值换成真实 `sizeof`，不会改变任何其他已定界单元，也不会改变本表口径。


## 6 与 40k 包络的关系（G7 已裁决）

40k 归因报告的分层是诊断结论，不是验收证据。[归因报告](2026-10-23-40k-memory-envelope-attribution.md) 给出的 product 稳态层为 503.6 MB，折合 **12,590 B/实体**；§5 的 13,312 B/实体按 40,000 实体折合 532.5 MB，相差 **+5.7%**。两者数量级与量级都吻合，说明 `state.*` 的计费尺度选对了量纲。

这条核对有三条限制，必须同时写明。第一，它是**事后核对**：计费值由静态审计先定，40k 数字没有参与选值，事后也不据此调整数值，否则上限就变成了实测的回填。第二，40k 峰值 2,960.0 MB 的其余分层——harness 副本 764.2 MB、decoded content 与 fixture 495.7 MB、pass-1 残留 100.5 MB、seek 瞬态 1,096.0 MB——都不落在 `state.*` 单元内，因此**本账本不能用来证明或反驳 40k 峰值**。第三，正因为不能，G7 必须由 owner 裁决。

owner 2026-10-24 的裁决是方案 §3.8 的**选项 2+3**：为每个 content 登记新的参考负载包络（新预算，不是豁免），并把 fixture 与 product 拆开计账。账本据此生成：

| 项 | 值 |
| --- | --- |
| 包络定义 | 逐 content：`covered`、无 `unit`、`additive` 的字节上限之和 + 声明余量 1/8 |
| `cpu-stream-v1`（host `defaults`） | 67,549,184 B + 8,443,648 B = **75,992,832 B** |
| `cpu-capture-2-v1` | 8,788,992 B + 1,098,624 B = 9,887,616 B |
| `cpu-sparse-v1`（200 实体） | 11,431,424 B + 1,428,928 B = 12,860,352 B |
| `cpu-topology-v1`（201 实体） | 11,444,736 B + 1,430,592 B = 12,875,328 B |
| 不计入包络 | 测试用例的 decode/fixture owning 对象、Snapshot/Replay 归档副本、观察器块头与结构表 |
| 旧固定常量 | `global_cpp_new_envelope`：count 2,097,152 / requested 1,073,741,824 / storage 1,342,177,280 —— 新运行**被替代**，历史运行保留原失败记录且不重跑 |

三点必须写明。第一，**这不是豁免**：新包络仍是预算，超过即失败；被替换的是「一次性全局常量」，不是「有没有上限」。余量 1/8 是声明值（覆盖未登记单元、分配器头部与容器冗余），不是对任何观测的拟合。第二，fixture 与 product 分账要求分析器能做域分类，这是 B1 的实现要求；旧 run 里 content+fixture 同层的 495.7 MB 不能直接与新包络相减来「证明」合规。第三，40k 不在上表中：40,000 实体不是参考 profile，其单侧完整规模证据由 P8 定案表与 B3 承担，本账本不替它登记上限。

同理，40k 的 6,818,424 B Packed、90,823,328 B snapshot、163,842,128 B frame bound 都不是本账本的上限来源，它们属于 P8 的完整规模证据，另行判定。

## 7 owner 复核记录

2026-10-24 复核结论逐项记录（owner 选择「全部按建议接受」）：

| 项 | 决定 | 对账本的影响 |
| --- | --- | --- |
| G1 | content 规模写入 `profiles.json`（逐 profile `content_entities`） | 脚本内置规模表删除，缺失即报错；`cpu-sparse-v1` 200、`cpu-topology-v1` 201，其余均为 1 |
| G4 | 接受 13,312 B/实体（5 项 × 65,536 B 固定开销） | `state.*` 五项转入 `cap_requested_bytes` 并转 `covered`，清单 5 项同时转绿 |
| G3 | 逐域 age 上限，弃用 900 s 全局默认 | 每单元新增 `retention`（稳态 live + age 上限 + 生命周期类别），`age.max` 转 `covered` |
| G7 | 方案 §3.8 选项 2+3 | 新增逐内容参考包络与 fixture/product 分账口径，旧固定常量标记为「新运行被替代」 |
| G5 / G6 / G8 | 一并接受，不阻塞采样 | 保持开放，作为 B1 分析器实现要求 |
| G9 | 不在 B0 登记字节上限 | 顺延 B1，由计数读出字节后补登 |

B0 门因此关闭，阶段顺序不变：B1（观察器 tag、`memory_domains.py` 分析器、宏关闭零代码改动、单测全绿）→ B2（64 / 2,048 各一次低/高运行）→ B3（40k 低/高内存采集）→ B4（设备域，需单独授权）。**一次 pilot、3×3 正式窗口与 immutable 登记仍各自需要显式授权**；本次复核不构成运行授权。

## 8 非声明

- 本账本不是内存测量：没有任何运行、没有高水位、没有峰值。
- 本账本不证明 40k 合规，也不证明 40k 违规；40k 的 `global_cpp_new_envelope` 被撑破是观察器幅面问题，PD-5 的内存项仍是「未测量」，不是「已失败」。
- 已定界上限之和（8,788,992 B / 67,549,184 B）与逐内容包络（最宽 75,992,832 B）不是进程内存预测，不得当作验收判据；它们是逐域判定的上限来源。
- §3.1 的元素尺寸是编译期 `sizeof`，不是分配实测：它给出的是「一个槽占多大」，不是「实际分配了多少」。容器冗余容量、分配器头部与碎片都不在其中，这也是包络要留 1/8 声明余量的原因。
- 覆盖表只说明「哪些单元现在能判定」，不说明「采样已经可以做」：B1 的观察器与分析器尚未实现，B2 之前不得采样。已关闭的 G1/G3/G4/G7 是**上限来源**的缺口，不是测量缺口。

