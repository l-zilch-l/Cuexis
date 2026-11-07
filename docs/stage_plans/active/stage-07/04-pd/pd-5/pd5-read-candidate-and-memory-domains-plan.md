# PD-5 读取候选与分域内存观测实施方案

状态：active evidence plan；R0-a/R0-b 已执行并判定，B0 已接受、B1 静态工具完成；剩余 B2/B3 采数未授权，不登记正式测量，不替代已接受方案或原证据，不构成性能、内存或阶段接受。

更新日期：2026-10-26

归属：Stage 7-PD / PD-5。上游为[已接受的 PD-5 测量方案](pd5-measurement-proposal.md)、[C2 输出槽预登记](pd5-read-result-candidate.md)和[内部设计与验收细则](../design.md)；证据基线见[读取噪声与寿命诊断](../../../../../stage_reports/stages/stage-07/04-pd/pd-5/memory-attribution/2026-10-23-memory-lifetime-and-read-noise-diagnostics.md)、[Stage C 独立验收与退出缺口](../../../../../stage_reports/stages/stage-07/04-pd/pd-5/admission/stage-c/2026-10-23-stage-c-independent-acceptance-and-exit-gaps.md)和[C2 结果](../../../../../stage_reports/stages/stage-07/04-pd/pd-5/read-candidate/2026-10-21-c2-read-result-candidate.md)。

本文件只定义两项输入的候选实施路径、判据、失败分支和授权边界：读取侧的新机制假设，以及 40k 内存的按域观测。两项分别授权、分别取证，不合并为一次多候选择优。F1 工具包的 r4 管理员运行已通过且不得重跑；工具全绿只证明工具合同，不认证读取性能，也不认证 40k 内存。

本文件不改变已接受的判定口径：共同 CPU 工作的回归上界仍为 1.10，重复性仍为 15% 的噪声门（不是允许退化 15%），7 项物理预算、4 个 hash 上限、30 s 回复期限、3 个预声明窗口和 9 对正式定义均保持原样。插桩数字不得替代正常分配器的预算证据。

## 1. 读取侧：1.3742 与 1.10 之间的距离

### 1.1 已知事实

| 事实 | 数值 | 来源 |
| --- | --- | --- |
| F1/A 的 read64 主指标 | 1.3742（95% CI 约 1.3517–1.3970） | 诊断 §4 复算 |
| A 侧 / F1 侧重复性 | 1.64% / 0.66% | 同上 |
| 正式回归门 | 区间上界 ≤1.10 | 方案 §4 阈值表 |
| 固定/边际分解 | A：`c0 = 181.49 ns`、`c1 = 41.0936 ns/对象`；F1：`c0 = 205.65 ns`、`c1 = 57.5190 ns/对象` | 四点什么拟合，[R0-a 归因](../../../../../stage_reports/stages/stage-07/04-pd/pd-5/read-attribution/r0-a/2026-10-23-read-regression-static-attribution.md) §2 |
| 两项差 | 固定 +24.16 ns/轮（比值 1.1331），边际 +16.4253 ns/对象（比值 1.3997） | 同上 |
| 需要的降幅 | `1 − 1.10 / 1.3742 = 19.94%`，即 read64 均值口径 **12.96 ns/对象**；分解为边际回落 75.0%（−12.32 ns/对象）与固定回落 24.9%（−6.01 ns/轮） | 算术 |
| 尺寸不变性 | 比值随 N=1/8/64/256 由 1.2358 升到 1.3962，渐近 1.3997，N→0 下限 1.1331 | 同上 §2 |
| C2 实测 | 1.0214（95% CI 0.6515–1.6011），C2 侧重复性 38.12% | C2 结果表 |
| C2 自身保留门 | read64 上界 ≤0.95，即比 F1 快 5% | C2 预登记第 74 行 |

上述 read64 的 4.16 µs 是每轮 64 对象读取的中位数，不是单次调用耗时；把它除以 64 得到的 65 ns 是**均值**，不是边际成本（同口径 A 47.30、F1 60.82 ns），用它做换算会把可移除量算高约 40%。量级判断一律以 `c0`/`c1` 分解为准。频率、缓存和分支预测均未测量，cycle 换算不得当作测量值引用。

### 1.2 三个必须接受的推论

1. **C2 的 2% 点估计不能当上界。** 38% 的重复性使置信区间覆盖 ±50% 以上，既不能推出“可移除的复制成本只有 2%”，也不能推出“可能接近 20%”。可回收比例只能由新的受控归因确定。
2. **5% 的保留门与 10% 的正式门不是同一个量。** 即使新候选让 read64 比 F1 快 5%，A/F1 仍是 `1.3742 × 0.95 ≈ 1.3055`，远高于 1.10。因此读取候选的准入条件只能是“能否把 A/F1 拉回 1.10”，即至少 19.94% 的降幅，分解为边际 −12.32 ns/对象（−75.0%）与固定 −6.01 ns/轮（−24.9%）；达不到就不应实现，否则只是把失败推迟到正式测量。
3. **这不是 C2 的重试。** C2 已明确关闭，不建 C3、不换样本、不在独立核验前合并。新候选必须是不同机制的新假设，且必须先证明其**上界**够得着 1.10，再决定是否实现。

### 1.3 尚未冻结的口径：read-long 主指标

方案第 38 行只登记到“read-long 每块至少 100 ms”，第 65 行规定每进程 8 个固定 trace/read 块，但对象数尚未冻结；C2 预登记把 read64 作为主指标。三种读法：

| 读法 | 含义 | 影响 |
| --- | --- | --- |
| (a) 延续 read64 | 与 C2 主指标连续 | 门槛就是 19.94%，最严格，也不易被指为按结果挑尺寸 |
| (b) 重新登记消费者 mix | 按真实消费者读取模式确定各档权重 | 需在测量前给出独立论证并冻结，不能看结果后选 |
| (c) 聚合块 | 一个 read-long 块含 read、updated、camera，按整块比 | 若 updated 占主导，整块可能已 ≤1.10，但把 read64 的回归藏进聚合与“预登记主指标逐项判定”（第 76 行）冲突 |

建议 (a)：延续 read64，1/8/256 作为次要项。若选 (b) 或 (c)，必须由 owner 在测量前书面论证并冻结；看到比值再改尺寸属于事后选择，必须拒绝。

尺寸不变性把这一争议降级：任何对象数下比值都不低于 1.18，N→0 的极限也是 1.1331，所以换尺寸既救不了 1.10，也不构成“按结果挑尺寸”的动机——它只会改变报告的差值大小，不改变通过与否。因此 (b)/(c) 的价值仅限于描述消费者真实 mix，不再被当作可能过关的口径。

## 2. 读取候选流水线

### 2.1 R0-a 静态归因（不运行、不改产品）

目标是拆出可移除成本并给出上界，不做候选实现。

1. **现有日志重算。** 对 A 与 F1 的 read 单元做 `t(N) = c0 + c1·N`（N=1/8/64/256）四点什么拟合：`c1` 为每对象边际成本，`c0` 为每轮固定成本；分别对 A、F1 拟合，得到固定成本差与每对象成本差。纯算术，不产生新样本。跨进程拟合只作归因，不作验收。
2. **生成代码审计。** 对两侧 `readObject` 读 COFF 与反汇编，逐项列出：`available()` 的 `thread::id` 比较与 `shared_ptr` 解引用、out-of-line `entries()` 调用与边界检查、`ObjectPresentation` 的 136 字节物化（其中 `Mat4` 默认成员初始化先写 64 字节恒等再被条件覆盖）、`Result` 构造与 sret、调用方移动、4 次 EnTT 查找（`valid`/`try_get`/`all_of`）。每项给指令数与依赖链。
3. **上界规则。** 只把四类计入可移除：每对象调用与返回（含 `Result`）、输出物化与冗余恒等写及调用方复制、每对象窗口与范围检查、每对象重复取 `entries()`。ECS 查找与组件读取不计入，动它们属于存储形态变更，不是本候选。
4. **不可静态判定清单。** 缓存命中、分支预测、分配器行为、频率必须显式列出，避免把静态估算当测量。

门 G1：若可移除项上界低于 19.94%，直接判定“无可提名候选”，出超界报告并停在 R0；若达到门槛或落在灰区（约 15%–25%），进入 R0-b。该分支须在实验前声明，防止用实验硬凑候选。

产出：`out/pd5-read-attribution-<nonce>/` 下的复算脚本、反汇编摘录、成本表、结论（含不可移除残余说明）。全部为只读证据，不进入产品树。

#### 2.1.1 执行结果（2026-10-23）

- 复算：直接使用既有 `out/pd5-followup-20261021-62ca993/diagnostic/scale-fit.json` 的四点拟合，不重跑探针、不启动可执行程序。
- 反汇编：`out/pd5-static-read-attribution-20261023/` 下 A/F1 两侧 `playback_session.cpp.obj` 与 F1 `presentation_materializer.cpp.obj` 的 llvm-objdump 产物；F1 `presentation_read.cpp.obj` 使用既有 dumpbin 产物 `out/pd5-followup-20261021-62ca993/f1-presentation-read.asm`（符号名与调用目标已解析）。目录名按实际 nonce 记录，与上段的 `<nonce>` 占位一致。
- 清单：F1 每对象热路径 239 条指令 + 4 次调用（另有 195 条只在失败路径执行）；相对 A 的额外工作 ≈147 条指令 + 4 次调用，可移除 ≈136 条（≈92%）。
- 投影：F1/A 在 1.0366（含死探测）/ 1.0824（按本节第 3 条排除 ECS 查找）/ 1.1656（保守：整条热路径都计为额外）之间。
- G1 判定：**灰区**。不判定“无可提名候选”，不提名 RB；下一分支为 R0-b，待授权。
- 证据、逐项归因与不可静态判定清单见[R0-a 归因报告](../../../../../stage_reports/stages/stage-07/04-pd/pd-5/read-attribution/r0-a/2026-10-23-read-regression-static-attribution.md)。

### 2.2 R0-b 有界归因实验（一次、短、仅诊断树）

目的：直接测候选上界，而不是候选本身。在独立临时 worktree 上对 `presentation_read.cpp` 加编译期变体，编出 5 个可执行：

| 变体 | 内容 | 测什么 |
| --- | --- | --- |
| A | 冻结的前 PD 实现，前提是仍有可运行的冻结可执行 | 同机对照锚；不可运行时只用存储比值 |
| V0 | F1 原样 | 实验有效性锚 |
| V1 | 批量物化写入调用方缓冲，去掉每对象调用、`Result` 和移动，保留同样的字段写与 ECS 查找 | 候选上界 |
| V2 | V1 跳过组件填充，只写 id 与索引 | 隔离 ECS 读取成本 |
| V3 | V2 去掉窗口与范围检查 | 隔离检查成本 |

另加 6 个原语微基准（`this_thread::get_id`、`entries()`、sparse `valid`、`try_get`、136 字节拷贝、136 字节恒等构造），在真实 40k registry 上执行，3 次重复，稳定门 5%。

协议：A（若可用）、V0、V1、V2、V3 交错各 3 次，同机器、同 content、同 viewport、每块至少 100 ms、30 s 热身；报各变体相对 V0 的中位比。

有效性门：若本次运行了 A，则 `V0/A` 必须落在 1.3742 ±10%，候选上界直接取 `V1/A`；若 A 不可运行，则要求 V0 的 read64 绝对中位数落在存储值 4.16/4.17/4.13 µs 的 ±10% 内，报告须披露 A 未同机重测，投影改用 `P = 1.3742 × (V1/V0)`。两种情形都只比相对值，不比绝对时间。

门 G2：以候选与 V0 的 3 次配对样本给出 P 的 95% 区间（df=2、t=4.302653），点估计与区间上界同时 ≤1.10 才提名；点估计 ≤1.10 但区间上界 >1.10 只算灰区，不得提名。否则记录“上界不足”，直接进入 R3 的未通过分支。变体只存在于诊断树，不进主树、不登记、不产生 A/B 结论；变体与插桩的绝对时间不作验收。

#### 2.2.1 执行结果（2026-10-23）

判定：门 G2 未通过（上界不足），读取候选不提名，R1 与 R2 均不启动。证据见[有界归因实验报告](../../../../../stage_reports/stages/stage-07/04-pd/pd-5/read-attribution/r0-b/2026-10-23-read-regression-bounded-attribution.md)。

| 检查 | 结果 |
| --- | --- |
| 有效性门 `V0/A` | 1.4140 [1.3911, 1.4374]，相对存储的 1.3742 偏差 +2.90%（±10% 内），A 可用，主判直接取同机比值 |
| 补丁惰性 `V0/V0p` | 1.0098 [0.9573, 1.0651]，两端 24 行 digest 逐位相同 |
| 候选上界 `V1/A`（G2 主判） | 1.3728 [1.3579, 1.3878]：点估计、上界、下界均高于 1.10 |
| 替代投影 `P = 1.3742 × (V1/V0)` | 1.3341 [1.3201, 1.3482]，同样越界，判定不依赖投影尺度 |
| 批量收益 `V1/V0` | 0.9708（N=64 回收 131.1 ns，占超出量 1175.1 ns 的 11.2%）；拟合量边际 −1.92 ns/对象（目标 −12.32，15.6%）、固定 −2.89 ns/次（目标 −6.01，48.1%） |
| 提取块（N=64） | V0 3208.1 ns，占其 4003.3 ns 的 80.1%；该块 F1/A = 1.578；防御检查仅 30.9 ns（V2→V3） |
| 到 1.10 的距离 | 总成本须再降 19.7%；提取块须从 3076.9 降到 2315.8 ns（−24.7%），即 A 的 1.139 倍 |

结论：边界批量化不足以闭合读取回归。残余在逐对象提取块（组件检索、136 字节 DTO 重建、快照字段与矩阵写入）内；V2/V3 是删除而非改写，故不能据此断言改写检索形态亦不可达。owner 依预授权规则（优先归因、达不到预期则记录超界）选定 §2.5 第一项“接受偏差并记录”（2026-10-23）：读取项在 PD-5 中记为已量化超界、不视为通过，1.10 判据不改，本轮不再提名读取候选；§2.5 第二项（存储形态改写）留作独立立项，不在本轮启动。

### 2.3 R1 候选 RB：窗口内批量物化加真实消费者采用

机制：在既有窗口回调内把逐对象物化合并为一次批量物化，并由实际消费者采用同一条路径。

```cpp
// engine/runtime/internal/cuexis/runtime/presentation_read.hpp（内部头，不安装，无 SDK API 影响）
core::Result<void> readObjects(std::size_t first, std::size_t count,
                               std::span<ObjectPresentation> out) const;
```

语义与约束：

- 入口只校验一次窗口可用性；`count == 0` 立即成功；`out.size() < count` 返回新增内部错误码 `runtime.presentation.batch_output`；`first > size` 或 `count > size - first` 返回 `runtime.presentation.layout_stale`（与单读同码，且先做防溢出判断）。
- 全有或全无：任一前置检查失败时 `out` 一个字节都不写。
- 每对象字段与 `readObject` 逐字段相同，包括“entity 无效时只写 id 的成功返回”，以及无 transform 时矩阵为恒等。
- 单一物化函数由单读与批读共享；循环外提 `entries`、`size`、`registry`；不分配、不加锁、无异常路径、不新增状态、不改 owner、线程或 Close 协议。
- 位置对应不是新不变量：`runtime_session.cpp` 已按 `objects.entries()[index]` 与 `runtime.objects[index]` 一一对应使用（`buildSnapshotLayout` 也令 `runtimeIndex = layout.entities.size()`），RB 只是继续该既有假设，并用每对象 id 比对做漂移守卫。
- 窗口串行号只在入口校验一次。窗口字段在回调开始前一次性安装、`serial` 每窗只递增一次，回调返回后清空；回调期间唯一可能改动它们的是重入 `read()`，那会使外层视图失效（失败关闭）。重入行为须显式测试并写明不支持。
- 不采用两遍/预取设计，除非 R0-b 证明查找延迟主导，且决定发生在 R2 之前并记录；否则一律单遍。

消费者采用（准入条件）：[`fillObjects`](../../../../../../engine/playback/src/presentation_materializer.cpp) 目前逐个调用 `readObject` 后立即把字段拷入 Snapshot DTO，`ObjectPresentation` 只是中转。改为按预声明的固定块（256，栈上约 34 KB）循环调用 `readObjects`，第二段沿用现有字段拷贝与 id 的 `layout_stale` 校验。这样测量单元与产品走同一条路径，消除每对象的 `Result` 构造、sret、移动和重复窗口检查，且不新增保留缓冲。若产品消费者不采用，本候选只是探针优化，不得提名。

测试与审计：批量与单读逐字段、逐字节等价（N=1/8/64/256，含无效 entity、无 transform、无 appearance、越界、`first == size`、`first + count` 溢出、span 过小）；错误码与消息一致；失败时 `out` 未写；`count == 0`；不分配（观察器 live 差为零）；窗口外、异线程、窗口已结束均返回 `view_unavailable`；重入用例；现有 PD 测试全绿；完整 Release CTest；语义审计覆盖布局顺序、错误顺序、窗口失效、单一物化器、无第二状态源；生成代码审计确认循环内无分配、无异常、无虚调用。新增错误码须检查是否有测试枚举错误码全集。

明确不做及理由：线程局部 scratch（第二状态与生命周期问题）、`ObjectPresentation` 缓存（失效与第二状态源）、暴露 registry 或 `ObjectEntityMap`（C1 已因存储指针撤回）、去掉 owner/serial 检查（保护丢失）、把单读改为出参（无谓改动）、LTO/IPO（全局构建变更，属另一类假设，需独立登记，不能与 RB 同批）。

### 2.4 R2 有限诊断（A′ 对 RB）

| 项 | 设计 |
| --- | --- |
| 单元 | read64 为主，1/8/256 为次；每块至少 100 ms；每进程每配置 3 块；64 次 update 加 extract 热身 |
| 对数 | 2 个预声明窗口 × 3 对 = 6 对（窗 1 AB/BA/AB，窗 2 BA/AB/BA），窗间至少 600 s；明确是诊断，不是正式 9 对注册 |
| 统计 | `r_i = median(B_i)/median(A_i)`，log-ratio 几何均值；合并 n=6、df=5、t=2.570582；逐窗 n=3、df=2、t=4.302653 |
| 主判 | read64 合并 CI 上界 ≤1.10；次判 1/8/256 与 updated 上界 ≤1.10 |
| 重复性 | 每侧每窗 `(max−min)/median ≤15%`；这是噪声门，不是允许退化 15% |
| 计时锚 | 两侧要么都有 per-block QPC 锚，要么都没有并在报告披露；不得只给一侧加 |
| 失败 | 区间跨线为 inconclusive；下界大于 1.10 为超界；不补样、不重跑、不换样本、不换统计量 |
| 产出 | 新建证据目录、报告，交回独立核验；核验通过后才讨论正式登记 |

### 2.5 R3 收口

通过（RB 的 read64 CI 上界 ≤1.10）：交独立核验（语义、生成代码、完整 CTest、统计），核验通过后才建议进入正式 9 对登记；正式测量仍无 ETW。

未通过：记录 read64 超界与残余解释（不可移除的 ECS 重建与存储形态），交 owner 三选一：接受偏差并记录、另立存储形态设计（大改，不属本轮）、维持超界并记录。不建 C3、不合并、不换样本。

## 3. 分域内存观测

### 3.1 前提纠正

40k 探针 [pd5_matrix_probe.cpp](../../../../../../tests/pd/pd5_matrix_probe.cpp) 在调用线程同步驱动 `PlaybackSession` 与 `FrameDelivery`，唯一 `<thread>` 用途是矩阵渲染的 `sleep_until`；没有 `SessionWorker`、audio 或 renderer owner 线程。因此“worker 分域”在 40k 上会退化为“调用线程 × 阶段”，不是真正的 worker 轴。真实 owner/worker 线程只出现在宿主装配单元：[pd5_host_probe.cpp](../../../../../../tests/pd/pd5_host_probe.cpp) 经 `ExecutionHost` 拉起 `SessionWorker`、`RendererOwner` 和 `AudioWorker`。

因此分两批：B2–B3 在 40k 上做“阶段 × 域”精确账本（owner 轴固定为 probe），B1 只建观察器与分析器；B4 在宿主探针上打开真实 owner 轴，需另行授权，不新建 harness。不得把 B2–B3 称为 worker 分域；准确说法是按所有者与阶段归因的精确分区账本。

### 3.2 账本模型

- 两轴：owner 取 `{probe, main, playback-worker, renderer-owner, audio-owner, audio-worker, unknown}`；domain 取 `{fixture, load, decode, prepare, snapshot, advance, current, candidate, scratch, index, ready, history, receipt, journal, replay, gpu, audio, retained, static, other}`。7 × 20 = 140，不超过 256 个格。
- 精确分区靠分配时打标：块头记录 tag，释放按块头 tag 回减，与释放线程无关，故跨线程释放安全。
- 双账：`live` 增量维护；checkpoint 时全链遍历核对并顺带计算每格最老年龄。两账不符即打印 `exact:false` 并置 `exceeded`（失败关闭），不静默通过。
- 基线语义：`before-load` 记录每格 live 作为基线，之后 checkpoint 打印绝对值，分析器计算差值；fixture 释放后 `fixture` 与 `unattributed` 的差值都必须为零，`static` 只允许进程级静态的单次增长并计入上限。
- 不重复计共享 backing：一个块只属于一个格；快照与编码共享缓冲按分配者归属，消费方只报引用不报字节。

### 3.3 观察器扩展

- 块头不涨：[pd5_memory_observer.cpp](../../../../../../tests/pd/pd5_memory_observer.cpp) 的 `Header` 现为 56 字节，把 `signature(32) + tag(16) + flags(16)`（共 64 位）打包进原 8 字节签名槽，`Header` 保持 56 字节。这样 40k 的 CRT storage 基线与旧 run 可比；若直接加 `uint16_t` 则每分配多 8 字节，必须重述基线并放弃逐字节比较，不推荐。已测得现状：该块头在 40k 峰值处构成 477.2 MB（8,521,200 × 56 B，占峰值 requested 的 16.1%），插桩整体墙钟开销约 7.5%（冻结 1,780.4 s 对插桩 1,913.4 s）；设计 D 不增加每块字节，故这两项不恶化。
- 静态表 256 格，每格含 owner 与 domain 字面量、live 计数、请求字节、CRT 可用字节、峰值与最老 born；注册表线性扫描只发生在进入 tag 作用域时，不是每次分配。
- `thread_local` 当前 tag，`tag(owner, domain)` 为 RAII、嵌套保存与恢复；注册满 256 时置 `exceeded` 并落到 `unknown`。
- allocate 与 release 在既有自旋锁内更新格计数：无新锁、无分配、不抛异常；`release` 的签名校验与 `std::terminate` 行为不变。
- checkpoint 的 `PD5_MEMORY` 行字段与顺序保持逐字节不变，旧解析器不受影响；其后每个非零格输出一行 `PD5_MEMORY_DOMAIN`（`schema: pd5.memory.domain.v1`），末尾一行 `PD5_MEMORY_PARTITION`（`schema: pd5.memory.partition.v1`，含 exact、sum_live、global_live、cells、coverage、notes）。
- 不重置阶段峰值：累计峰值在每个 checkpoint 打印，读者用相邻增量定位区间；多线程下重置会互相踩，故一律不重置。
- 覆盖声明：仅统计链接进进程的 C++ new/delete；直接 malloc、SDL、驱动与 OS 堆不计。请求字节、CRT 可用字节、进程工作集/提交、GPU 实占四层分开报告，不互相冒充。

### 3.4 探针作用域与 checkpoint 阶梯

| 格 | 打标位置 | 上限类型 |
| --- | --- | --- |
| `fixture/*` | fixture 与内容构造 | 字节 + 计数 |
| `load`、`decode`、`prepare` | 对应调用 | 字节 |
| `snapshot` | capture | 字节 + 计数 |
| `advance` | update 与 extract | 字节 |
| `current`、`candidate`、`scratch`、`index` | 产品内受保护 tag | 字节 |
| `ready`、`history`、`receipt`、`journal` | 产品内受保护 tag | 计数 + 字节 |
| `replay`、`seek` | 对应调用 | 字节 |
| `gpu`、`audio` | 设备域，无设备则 `NA` | 计数 + 字节 |
| `retained` | 合法外部持有 | 计数 + 字节 + 年龄 |
| 释放组 | 阶梯析构 | 归零 |

产品内 tag 只在 `CUEXIS_ALLOCATION_LIFETIME_OBSERVER` 下编译，正常构建为零代码；位置按责任边界放在 `PlaybackSession` 的 current/candidate/scratch/index、`RuntimeSession` 的提交以及 `fillObjects`，不得放进计时热路径。

阶梯沿用诊断 §3 的释放顺序（`swap` 不用 `clear`）：

```text
before-load → after-load → after-prepare → after-snapshot → after-advance
→ after-seek → after-replay → after-close
→ after-release:decision → after-release:expected → after-release:snapshots
→ after-release:replay → after-release:content → after-release:fixture → after-all
```

每个 checkpoint 打印全局、各域与分区。checkpoint 不得进入计时热路径：40k 下全链遍历数百万块约需百毫秒量级。

### 3.5 上限登记

新 schema `pd5.memory.caps.v1`：每个 content 与每个格给出 `cap_kind`（count、bytes 或 both）、`cap_count`、`cap_requested_bytes`、`cap_storage_bytes`、`basis`（静态推导公式或文本）、`retention`（允许的稳态 live 与年龄上限）、`coverage`（covered 或 `NA:<原因>`）。

静态推导：配置容量乘以审计过的元素尺寸再加固定开销；实测值只能用于核对，不能作为上限来源，避免用 pilot 定阈值。推导脚本输出、owner 复核、冻结后才允许运行。

禁止 0 与无限制占位；不可观测域写 `NA` 并给原因（GPU/audio 无设备、journal 未触发等），不写 0。

与方案 §5 清单逐项对照：CPU payload、attempt、GPU in-flight、World/current/candidate/scratch、索引、Ready/history/receipt/journal、retention/deadline、年龄。缺项必须显式列出；这决定 reference-workload 能否登记，不能靠“跑一次看看”补。

### 3.6 分析器

新增 `tools/verification/playback_performance/memory_domains.py` 及 `check-domains`、`collect-domains` 子命令；不改 [observer.py](../../../../../../tools/verification/playback_performance/observer.py) 的 `memory()`，因为它要求 `exact_partition is False`，与精确分区互斥。

校验项：checkpoint 顺序与必需集；schema 与 scope；每个 checkpoint 的 `sum(cells) == global live`；每格峰值不超过已登记上限；最终产品格 live 为零，或处于声明的有界稳态（不超过上限且年龄不超上限）；fixture 域独立计入、不与产品域合并；`unattributed` 差值为零；缺上限或缺域即失败，不跳过。年龄使用逐域声明值，不使用 900 s 全局默认：40k 运行约 30 分钟，加载期块在工作结束时必然超过 900 s，旧默认会把合法稳态判失败；当前 40k 的首个拒绝原因仍是 `exceeded` 为真，年龄检查尚未成为主因。

测试：新增 `tests/pd/pd5_memory_domains_tests.py`，用最小合成日志覆盖每种拒绝路径，并保留一个通过样例。

### 3.7 阶段与门

| 阶段 | 内容 | 运行 | 门 |
| --- | --- | --- | --- |
| B0 | 静态持有账本、上限推导、缺口表 | 否 | owner 复核推导 |
| B1 | 观察器、探针 tag、分析器、打包与单测 | 否 | 单测全绿，宏关闭时零代码审计 |
| B2 | 64 与 2048 的低/高规模作用域审计 | 是，一次 | 分区精确、最终归零、上限通过 |
| B3 | 40k 低/高内存采集（仅功能与内存，不跑性能窗） | 是，一次 | 同上；独立目录，不与旧功能 run 混用 |
| B4 | 宿主探针真实 owner 轴与 GPU/audio | 另行授权 | owner 接受 |

打包：新建目录加 `binding.json`（源清单、编译与链接参数、输入 hash、工具 SHA）、不可变回执、一次性运行、不重试。产品源变化即产生新 binding，运行前完成完整 Release CTest。B3 按功能 run 约 28–30 分钟/侧估计，加 checkpoint 开销约 30–40 分钟/侧。

### 3.8 与既有 40k 包络的关系

新账本能解释“超限属于谁、发生在哪个阶段”，但不能把 `global_cpp_new_envelope` 的 `exceeded=true` 变假：固定常量就在观察器文件里（计数 2,097,152、请求 1 GiB、CRT 存储 1,342,177,280），重跑只会再次触发。三个选项：

1. 保留固定包络，把超限作为开放 finding，本轮内存验收仍不通过；
2. 按 content 登记新的 reference-workload 包络（逐域上限之和加声明余量，复核后冻结），这是新预算，不是豁免；
3. 把 fixture 与 product 分开登记，global 包络只覆盖 product 域。

建议 2 加 3，但必须由 owner 明确接受“新包络替代旧固定常量”，且旧 run 的失败记录保留、不撤销。无论选哪种，40k 功能 run 不重跑，旧证据原处保留。

## 4. 交叉规则

1. 两件事不合并成多候选择优：读取侧每批最多一个候选；内存侧只建账本，不改读取路径。
2. 不改 1.10/15%、7 项物理预算、4 个 hash 上限、30 s 回复期限、3 窗 9 对的正式定义。
3. 不启动正式测量、不自动起任何采集、不提交、不推送；所有新产物使用新建目录。
4. 不重跑 C2、r2/r3/r4 和 40k 功能 run；失败记录原处保留。
5. 插桩数字不作预算，仅测试工具或测试树可插桩。

## 5. 风险与对策

| 风险 | 影响 | 对策 |
| --- | --- | --- |
| R0-b 投影比高于 1.10（最可能） | 没有可提名候选 | 已设为合法终点：出超界报告与残余解释，不硬凑 |
| R0-b 变体扰动测量 | 误判上界 | 有效性门（`V0/A` 或 V0 绝对值）；交错运行；只比相对值 |
| RB 只在探针生效 | 指标失去产品意义 | 准入条件强制真实消费者采用同一路径 |
| 40k 新采集昂贵且一次性 | 浪费一轮 | 先把产品 tag 作用域一次性放全，避免第二次 40k |
| 上限静态推导被否 | 无法登记包络 | B0 先出推导与缺口表，owner 复核后才进 B1 |
| 观察器开销与误差 | 归因失真 | 分区与全局双账；宏关闭零代码；报告插桩开销上限 |

## 6. 待决与下一步

按优先级需要 owner 决定：

1. read-long 正式主指标口径：延续 read64（建议）、重新登记消费者 mix、聚合块。后两者须在测量前书面论证并冻结。
2. 读取回归的处置已依预授权规则落定：R0-a 判灰区、R0-b 判上界不足，两轮均未达预期，故取“记录超界”的退路——owner 选定 §2.5 第一项“接受偏差并记录”（2026-10-23），读取项记为已量化超界、不视为通过，判据 1.10 不改，本轮不再提名候选、不做存储形态大改。R0-b 的授权已用尽，不再有 R1/R2 分支。
3. B1 已完成受宏保护的产品/探针 tag 与域分析器；其 World、delivery、command 覆盖和 label/释放阶梯缺口仍待 B2 前裁定，不把工具静态门当作逐域内存接受。
4. B0 已接受逐内容参考包络与 fixture/product 分账；原包络记录保留，40k 内存项仍记未测量。64/2048/40k 的 caps 规模口径尚未登记，B2/B3 前仍须裁定。
5. F1-only ETW 噪声控制仍仅是可选诊断，不是门；本次整合未授权该采集或重跑 r2/r3/r4。
6. B0 静态账本已于 2026-10-24 获接受（G1/G3/G4/G7 关闭，G9 顺延 B1）；B1 观察器/tag/分析器完成并通过静态门，30 项单测通过，未运行工作负载。B2 的 64/2048、B3 的 40k 采数与 caps 规模口径、覆盖/阶梯缺口仍待 owner 授权和裁定。

本文件落盘不等于剩余批次获授权。R0-a/R0-b 已执行并判定，本 PD 读取归因批次不再提名候选；RPA R2 为另行授权的整改线。B0 已接受，B1 静态完成，B2/B3 未获采数授权。当前只在 `D:/Cuexis-worktree` 实施，准入树只读保留；本次两树整合构建/正确性验证不授权新性能诊断、ETW、40k、pilot、正式窗口或新候选。

## 证据位置

| 主题 | 位置 |
| --- | --- |
| 已接受的测量范围与阈值 | [pd5-measurement-proposal.md](pd5-measurement-proposal.md) |
| C2 预登记（已关闭） | [pd5-read-result-candidate.md](pd5-read-result-candidate.md) |
| 读取噪声、寿命审计与复算 | [2026-10-23 诊断](../../../../../stage_reports/stages/stage-07/04-pd/pd-5/memory-attribution/2026-10-23-memory-lifetime-and-read-noise-diagnostics.md) |
| 40k 包络、所有权与退出缺口 | [Stage C 报告](../../../../../stage_reports/stages/stage-07/04-pd/pd-5/admission/stage-c/2026-10-23-stage-c-independent-acceptance-and-exit-gaps.md) |
| C2 结果与未保留结论 | [C2 结果](../../../../../stage_reports/stages/stage-07/04-pd/pd-5/read-candidate/2026-10-21-c2-read-result-candidate.md) |
| 当前状态摘要 | [CURRENT_STATUS.md](../../../../../CURRENT_STATUS.md) |
