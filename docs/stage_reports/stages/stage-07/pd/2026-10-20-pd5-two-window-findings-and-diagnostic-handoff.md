# PD-5 P0 two-window findings and diagnostic handoff

日期：2026-10-20。状态：P0 已完成两个正式窗口，owner 要求先核查并修复性能问题，再重新测量；本组不继续自动执行窗口 3。根因尚未完全确认，未实施性能修复。

本报告记录有限测量与初步归因，不替代[已接受的测量协议](../../../../stage_plans/active/stage-07/pd5-measurement-proposal.md)，不接受完整 PD-5、生产性能或 Stage 7 关闭。

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
独立复核一致不替代因果实验；当前不宣称根因查明或修复完成。

## 后续核查与验证边界

下一步先建立独立诊断源码/构建，对受控窗口内批量读取与现有路径作单变量对照；保持 owner、生命周期、
serial/index、过期/逃逸 view 和重入拒绝语义。宿主分开观测 update/request 排队与执行、maintenance、
admission/storage、提取以及 receipt enqueue/drain，不将嵌套计时相加为独立成本。
可评估同一 worker 命令内先更新、再独立作交付准入，仍保留交付失败不回滚更新、无信用不阻止更新的语义。
未授权增加线程/owner、修改 Gameplay/Replay 合同或放宽测量阈值。

后续修复应运行适用语义/生命周期回归；新源码、新构建必须重新 prepare、一次 pilot、不可变 register，
再执行预声明完整三窗。当前慢样本、噪声失败与人工停止历史全部保留。
本轮存档前重新运行 PD-5 Python 测试 41 项通过；完整 C++ Debug/Release 矩阵及正式性能接受不由本次存档代替。
