# PD-5 P0 two-window findings and diagnostic handoff

日期：2026-10-20；2026-10-21 更正。状态：修复前 P0 两窗保留、窗口 3 停止；F1 已提交，只有有限诊断效果，尚无修复后的正式接受。下文实验数值保留其原范围；关于成本下限、IPO、宿主 total、拟合和对象数的旧解释由本次更正取代。

本报告记录有限测量与初步归因，不替代[已接受的测量协议](../../../../stage_plans/active/stage-07/pd5-measurement-proposal.md)，不接受完整 PD-5、生产性能或 Stage 7 关闭。

## 2026-10-21 证据更正

本次逐项复核源码、完整 IPO/F1 日志、scale-analysis.json 和宿主分析脚本。问题表、证据路径与缺项见
[门禁核查与实施接手记录](2026-10-21-pd5-followup.md)。原日志、JSON、二进制及冻结清单均不改写。
有限变体没有证明保语义实现的成本下限；1.10 仍是原约束。F1 后规模、真实更新和共同 ready 证据待补。

## 注册、完整性与停止边界

证据根为 `out/performance-formal-20261020/`。旧组
`owner-prepare-f5cea4c7ff06/registration-a6b2c5d78b16` 的窗口 1/2/3 分别完成 36/31/0 个进程，
按 owner 人工中断记未判定；原始日志和 341 个文件哈希保持一致，不补样、不替换。

owner 接受保留旧组并新开完整组后，新 preparation `owner-prepare-c422bfbfc996` 完成一次 12 进程 pilot，
注册 `registration-071516aef126`。新组窗口 1/2 各完成 36 个正式进程，共 72/108；全部进程、原始日志、
工作负载身份、领取顺序与证据链复核通过。北京时间分别为 14:50:54–15:22:27、15:32:38–16:04:18；
UTC/monotonic 间隔均约 611.86 秒。环境为本机 AC、固定电源策略、headless CPU，不使用 GPU/audio/VSync。

窗口 2 完整结束后按 owner 要求停止控制器；窗口 3 没有 claim，也没有样本或 summary。
主动停止控制器的退出码 1 不代表正式进程失败。`paused-after-window2.json` 和根目录
`pause-after-window2-evidence-hashes.json` 保留暂停状态与 442 个文件哈希。

随后 owner 要求先深究性能问题并由无上下文 GPT-6-Luna 独立核查，再在修复后重新测量。
因此两窗作为未完成固定组保留，不拼接修改后的第三窗；源码、构建或 HEAD 改变后旧 preparation 不能复用。

## 两窗观察

A 为冻结 pre-PD，B 为当前 PD-4 正常构建。以下为各窗三对进程的配对几何平均比；大于 1 表示 B 成本增加。

| 单元/指标 | 窗口 1 B/A | 窗口 2 B/A | 限制 |
| --- | --- | --- | --- |
| Release RPA read_long | 1.5362，95% 区间 [1.2543, 1.8815] | 1.5754，[1.4569, 1.7035] | 两窗及前六进程重复性合格；仍非完整九对结论 |
| Release legacy read_long | 1.5481 | 1.7338 | 窗口 2 A 波动 21.81%；合并 A/B 波动 27.16%/28.65%，不满足 15% |
| Release host request-to-ready | 2.1562 | 2.1973 | 窗口 2 A 中位数/均值波动 35.49%/31.33%，不满足 15% |
| Debug host request-to-ready | 1.2781 | 1.2736 | 第一窗区间下界高于 1.10，第二窗区间跨过 1.10 |

第一窗所有登记重复性指标合格；第二窗及两窗合并的重复性缺项如表。其余 10 项 owner 指标在两窗中
均满足已执行范围的重复性与逐窗回归区间门槛。Debug read_long 约改善 8%，不能概括为所有构建、所有执行路径都变慢。
宿主每进程只有 512 帧，不判定 n≥1000 的 p99 或 4.167 ms p99 预算。两窗不满足连续三窗的 owner 拆分触发条件。

## 初步归因与独立核查

`read_long` 只在同一状态上反复调用 `extractFrame(viewport, reused_snapshot)`，不含 Gameplay 更新或宿主交付。
Release RPA 各进程块中位数的描述性汇总约为 wall 0.206→0.328 us/op、线程 CPU 0.205→0.326 us/op，
cycles 约 775→1235。线程 CPU 同步增加支持真实执行成本增加，但计数不能唯一归因频率、缓存或某一调用。

冻结 A 在提取函数内直接读 Registry；B 通过 Runtime 读窗口、版本检查与唯一物化器，
state/object/camera 分别执行弱引用提升、owner/active/serial 检查，并传递中性结构与 Result。
现有 Release 目标文件反汇编确认成功路径保留引用计数原子操作及跨函数调用。
这些是优先候选，尚未通过隔离实验量化各自份额；错误分支中的分配不代表正常路径每次分配。
A/B 正式探针循环一致，同一 MSVC、Release `/O2 /Ob2 /DNDEBUG`，正常计时没有启用 metrics。

宿主 A 在 ready 前只有一次同步 Sample，present 后另同步等待 receipt ACK；B 在 ready 前分别同步 Update 和 RequestFrame，
present 异步投递 receipt。两版 544 帧的 pump_count 均约 1088；ready 端点变慢不能推出整帧总往返或总工作量翻倍。
ACK 与 present-return 的结束边界不同，也不能直接相加比较。新路径的交付准入、存储/attempt/payload、锁、
命令后 ledger 维护及异步 receipt 处理仍须独立量化。

用户指定的无上下文 GPT-6-Luna 子代理已独立复算并核查源码，支持上述判断，认为读取候选根因置信度中等。
证据与可复算脚本保存在 `out/performance-diagnosis-20261020/`，含 `two-window-analysis.json`、
`preliminary-diagnosis.md`、`independent-review.md` 与 Release 反汇编。
独立复核一致不替代因果实验；下节的受控隔离实验给出份额量化。根因定位完成，修复未实施、未验证。

## 受控隔离实验：读取边界成本分解

为把"待量化候选"变成份额，从同一 HEAD `254c8b4a` 建立四个单变量诊断构建，每次只改一处、
构建成功即存档补丁与可执行文件、随后 `git checkout` 恢复干净树。测量沿用同一探针、同一目标、
同一 `/O2 /Ob2 /DNDEBUG` 与 `CUEXIS_PD_V2_READS=2097152`，配对交错、affinity mask 4、
每进程预热 30 s、8 块，两轮共 16 个样本取中位数。六个二进制摘要值全为 `3815754596451481321`。
所有差额只在同一梯次内比较；跨梯次 A/B 漂移可达 5%（A 208.3–212.0 ns、B 316.0–328.1 ns）。

| 二进制 | 相对 B 的单变量差异 | ns/op | cycles/op | B/A | 1.10 门槛 |
| --- | --- | --- | --- | --- | --- |
| A | 冻结 pre-PD | 208.3 | 786.0 | 1.0000 | — |
| B | 当前 PD-4 正常构建 | 320.2 | 1206.4 | 1.5373 | 不通过 |
| Q2 | 读取窗口取消 `weak_ptr` 提升，保留全部校验 | 295.4 | 1114.0 | 1.4180 | 不通过 |
| V1 | 单次校验并复用句柄，保留中性结构 | 278.2 | 1046.9 | 1.3356 | 不通过 |
| Q4 | 保留窗口/门禁/提升，物化直读 Registry | 237.6 | 891.7 | 1.1404 | 不通过 |
| Q3 | 直读 Registry，完全去掉中性读取窗口 | 228.9 | 858.5 | 1.0988 | 通过 |

本梯次 B−A 为 111.9 ns。差额分解（单变量、非相加预算）：

- B−Q2 为 24.8 ns（22%）：`PresentationRead` 每个 `state`/`readObject`/`readCamera` 各做一次
  `weak_ptr::lock` 与控制块原子增删，一次 `extractFrame` 共三次。
- Q2−V1 为 17.2 ns（15%）：同一窗口被重复校验三次而不是一次，每次含 owner、active、serial 比较
  与跨函数调用。
- B−Q4 为 82.6 ns（74%）：中性逐对象/逐相机值路径；扣除其中两次冗余校验后约 65 ns（58%），
  与上一轮"提升之外剩余约 63 ns"同量级，仅支持继续调查该路径，不唯一确定此前未归因的固定成本。
- Q4−Q3 为 8.7 ns（8%）：窗口注册写入、门禁比较、回调间接调用与 `read` 内的异常边界。
- Q3−A 为 20.6 ns（本梯；前一梯 9.0 ns，即 1.0423）：即使完全去掉中性窗口，B 仍比 A 慢。
  这部分是 `materialize` 前置检查（owner/re-entry/committed/视口）、对象映射间接与一次 Registry 锁，
  不在读取窗口内；这些差分不证明它不可回收或构成固定下限。

机制与代码结构一致：`extractFrame` → `materializeSnapshot`（`cuexis_playback`）→
`PresentationRead::state/readObject/readCamera`（`cuexis_runtime`）是三次跨翻译单元调用；
Release 只用 `/O2 /Ob2`、没有 `/GL`，因此无法内联，每个调用通过隐式返回存储传递
`Result<ObjectPresentation>`/`Result<CameraPresentation>`（不在此推定 ABI 字节数），再由 `fillObjects`/`fillCamera`
逐字段复制进 `FrameSnapshot`。pre-PD 的 A 把同一份工作内联在 `playback_session.cpp` 里，
既没有逐对象调用也没有中性结构体。

这些差额不是可相加预算（机制解释仍属假设）：24.8+17.2+65.4+8.7 比 111.9 超出约 4%，因为去掉一次调用边界会让 MSVC
重新内联相邻代码，Q2 省下的 24.8 ns 多于引用计数本身的成本。因此上表是区间证据，不是账本。

当时未见收益的有限候选：把中性结构体换成标量访问器、或"批量读取写入调用方存储"的原型（V2/V3/V4）相对 V1
不省反而多 5.7–10.2 ns（1.3290–1.3504，同一梯次 A 209.9、B 328.1、V1 1.3016），该轮未观察到改善；
取消异常边界（Q1）在同梯次只值 4.8 ns；对象 ID 的 36 字符分配不存在（`id`/`material` 为
`string_view`，正常路径不分配）。

Q3/Q4 只作诊断变体：它们用 const void* 句柄绕过中性值读取，Q3 还丢弃
owner/serial/过期 view/重入保证，不是可提交候选。Q2、V1 的有限点估计说明单独修复
当时未达到 1.10，不能证明进一步收益必须改变模块边界或物化归属。
2026-10-21 更正：这些互相影响的单变量差分不是成本下限；后续应在 F1 基础上验证
调用、复制和查表等保语义实现，IPO 及读取侧物化均不是穷尽的剩余选项。

补丁、可执行文件与原始日志保存在 `out/pd5-rootcause-20261020/`：`patch-Q1-no-eh.diff`、
`patch-Q2-no-promotion.diff`、`patch-Q3-direct-read.diff`、`patch-Q4-envelope-direct-data.diff`、
`probes/*.exe`、`ladder-Q1-Q3.log`、`ladder-final-decomposition.log`、`round2/`、`round3/`。
工作树在存档后恢复干净，未提交任何诊断代码。

## 绝对量级与宿主端点更正

历史梯次 A 208.3、B 320.2 ns/op，差 111.9 ns。仅在每帧一次相同提取的假设下，
占 240 Hz 周期约 0.0027%；F1 同轮差约 63 ns，占约 0.0015%。这是块平均成本的中位描述，
不是单帧 p99、SDK 全规模或用户无感证明。相对回归容忍 10% 与重复性 15% 是不同量，
不能比较两者大小来推导不可检测性；正式协议用独立进程配对区间与不可判定状态处理。

宿主历史输入为注册目录各窗 release-host-common-{0,1,2}-{A,B}-measure.log，
分析为 out/pd5-rootcause-20261020/analyze_host_ready.py 和 host-ready-phase-analysis.json。
该脚本把 A request→receipt ACK 与 B request→present-return 都叫 total，并计算了比值；
两者终点不同，此 total 比值及 phase2/present_call 的跨端点门禁标签无效，不作为同一成本或收益。
原输出保留，只按各侧独立观测解释。

窗口 1 两种不同 total 的进程中位数汇总为 46.55 / 57.30 µs，数值差 10.75 µs，
占 240 Hz 帧预算约 0.258%，数值增幅约 23.1%；不是“整帧只多 0.26%”。
由于终点不同，两种换算都不能证明共同整帧成本变化。

request→ready 则具有公共意义：相同请求起点、逻辑工作和 ready 定义成立时，应保留其比较。
A 在 ready 前一次同步 Sample、之后等 ACK，B 在 ready 前依次 Update 和 RequestFrame，
之后异步 receipt；额外往返是待验证的原因，不能消解公共延迟增加。窗口 2 A 的 ready 波动约
35.5%，超过 15%；每进程 512 帧不足以判定 p99。enqueue、drain、ACK、present-return、GPU
完成须分项，不能给异步缺失项填零或合并成共同完成时间。

## 多对象放大：1/8/64/256 对象的 A/B 提取对照

上节与更早的成本分解都建立在"1 个 presentation 对象 + 1 个相机"的固定工作量上，无法区分"每次调用
贵一点"与"每个对象贵一点"。为回答这个问题，在两棵树的测试源里临时追加同一个不登记的探针单元
`PD neutral read scaling`（`[.][pd][scale-probe]`），构建后立即还原源码；产品代码、公共头文件与
构建配置均未改动。

方法：每个进程先跑 30 s 机器预热单元，再对 4 种对象数各做 3 个 2048 次读取块；每侧 2 个独立进程、
A/B 交替，绑定掩码 4，两侧同一份资产与同构图表（ids、transform、renderable、mesh/material 完全相同）。
A 侧源码为 pre-PD 提交 `d344088`（为构建另开的临时 worktree，测量后已移除），B 侧为当前 HEAD；两侧同为 Release
`/O2 /Ob2 /DNDEBUG`、`CUEXIS_LIBRARY_TYPE=STATIC`、`CUEXIS_ENABLE_CHART_V5_CANDIDATE=ON`，
都不开 `CUEXIS_PD_METRICS_TEST_ONLY`，因此差异只来自源码。

| 对象数 | A ns/次 | B ns/次 | B/A | 第 1 轮 | 第 2 轮 | A 噪声 | B 噪声 | 差额 ns/次 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 214.6 | 298.6 | 1.391 | 1.390 | 1.392 | 15.7% | 7.4% | +83.9 |
| 8 | 500.4 | 808.2 | 1.615 | 1.615 | 1.550 | 12.0% | 0.8% | +307.7 |
| 64 | 2854.7 | 4652.5 | 1.630 | 1.652 | 1.617 | 3.1% | 2.9% | +1797.8 |
| 256 | 10766.0 | 17765.6 | 1.650 | 1.649 | 1.633 | 6.5% | 1.1% | +6999.6 |

线性拟合（4 个中位点）：A 为 41.4 ns/对象 + 181 ns 固定，B 为 68.4 ns/对象 + 253 ns 固定，最大残差
22.8–25.8 ns；**每对象成本之比 1.654**。两侧 `materialized` 计数等于对象数，四个对象数的摘要
（`10618349604517880661`、`7298062694041131797`、`14081908852515621389`、`15548404266838521254`）
在 A/B 之间完全一致，说明探针比的是同一份输出，不是"少做了工作"。

2026-10-21 拟合更正：四点拟合差额为 `71.6765 + 27.0590×N` ns，
不是 `56 + 27×N`。N 是 Snapshot 对象数；固定 Gameplay 探针断言 Snapshot 为 1 对象，
不能以 registry 的 presentation/gameplay 实体数解释成正式 2 对象；宿主场景的 2 对象是另一个内容。
相机数另列（此规模内容为 1）。拟合只描述此修复前 A/B、1–256 范围，不外推 F1、40k 或尾延迟。

限制：每侧只有 2 个独立进程，每配置每进程 3 个 2048 次读取块；原 JSON 的
blocks_per_process=[6] 是跨对象聚合口径的混淆。小规模块远短于 100 ms，不是正式长块。
全样本保留，包括 N=1 A 的较慢首块；不以选取“干净”样本作门禁接受。
日志、探针源码块、脚本和 JSON 位于 out/pd5-rootcause-20261020/ 的 scale-*、
scale-probe-block.cpp.txt、analyze_scale.py；二进制为 probes/A-scale.exe、B-scale.exe。

## 修复尝试：单次窗口校验（F1）与 IPO 对照

"记录在案"之后，按上文的两部分要求实施了一项保持语义的低成本改动（F1），并对"剩余成本是否只是
跨 TU 调用开销"做了 IPO 对照。两者都不改变 owner、生命周期、serial/index、过期/逃逸 view、
跨线程与重入拒绝语义。

### F1 实现

读取窗口在 `PresentationAccess::read` 建立时校验一次；`PresentationRead` 改为按值持有一个强引用
与本次窗口代号，`readObject` / `readCamera` / `state` 每次只做三项标量判断（owner 线程、窗口是否
仍 active、代号是否仍是本次窗口），不再每次 `weak_ptr::lock()` 提升，也不再重复走一层
`checkedWindow()` + `Result` 中转。失败仍返回原有的 `runtime.presentation.view_unavailable`
错误码与文案，索引越界仍是 `runtime.presentation.layout_stale`。副本仍可逃逸，但窗口随回调结束
失效，逃逸副本的每次读取仍被拒绝，后一次窗口也不会重新激活旧句柄（`serial` 比较保留）。
每帧提升由 3 次降为 1 次，重复校验由 3 次降为内联比较。

改动只在内部头与实现，安装头、公共 SDK API、模块依赖与构建配置均不变：
`engine/runtime/internal/cuexis/runtime/presentation_read.hpp`、
`engine/runtime/src/presentation_read.cpp`。

验证：`cuexis_runtime_tests`（10603 断言 / 49 用例，含 `[pd][presentation-read]` 的跨线程拒绝、
逃逸副本拒绝、旧句柄不复用、零分配）、`cuexis_playback_tests`（10751 / 125）、
`cuexis_playback_allocation_tests`（394 / 9）、`cuexis_world_tests`（413 / 24）全部通过；
`cuexis_architecture_tests` 通过；两个改动文件 `clang-format 22.1.3` 无差异。

### F1 同轮诊断（非正式接受）

local_measure.py 每侧两进程、每进程 8 块，摘要汇总块中位数：

| 二进制 | 中位 ns/op | cycles/op | 相对 A 点估计 |
| --- | --- | --- | --- |
| A | 213.3 | 798.7 | 1.0000 |
| 修复前 B | 338.0 | 1215.1 | 1.5848 |
| F1 | 276.5 | 1036.8 | 1.2965 |

F1 回收该轮差额约 49%，仍差约 63 ns；不把另一梯次的 111.9 ns 当该轮分母。
三个 digest 均为 3815754596451481321。原日志 ladder-fix-F1.log 和 probes/F1.exe 保留。
旧 local_measure.py 的 PASS/FAIL 仅按汇总点比值打印，不能作为正式配对区间接受。
Q3 的 1.0988 同样仅是点估计；其绕过 owner/serial/过期保证，不能作为候选或门禁通过证据。

### IPO 探索（全部样本）

仅两个库临时开 IPO 的实验已还原，未提交构建配置变更。完整 ladder-fix-ipo.log 汇总
A/F1/F1ipo 为 211.8/276.0/334.7 ns，块波动约 28.26%/32.67%/24.08%；
ladder-fix-ipo-r2.log 为 267.4/359.7/484.3 ns，波动约 68.95%/22.21%/51.33%。
“约 7 ns”来自所选前段块，不能代表全部样本；外部负载归因未单独证实。
该探索噪声大，不证明内联无效、剩余成本不可回收或 1.10 不可达。

## 后续核查与验证边界

当前 PresentationWindow 保存 registry/object-map 指针、相机选择和状态；readObject 返回中性值，
并非先复制所有对象到窗口。有限失败变体没有穷尽保语义优化空间。
保持唯一 Playback 物化器、中性读取边界、owner/thread、active/serial、旧布局/版本、越界、
逃逸与重入保护；F1 的历史 Debug 局部验证不能代替当前候选 Release 全量验证。

旧两窗始终对应修复前代码；不续跑旧窗口 3，不拼入新实现。F1 后先作预声明有限诊断，
保留全部样本、输出等价和错误记录，再选择有依据的少量优化。正式重新 prepare/pilot/register
及三窗采样留待后续授权。1.10、15% 和所有绝对预算保持不变；PD-5、生产与阶段接受均未完成。
