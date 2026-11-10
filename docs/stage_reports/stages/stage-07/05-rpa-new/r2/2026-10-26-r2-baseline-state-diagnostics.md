# R2 B 基线状态诊断及显式重跑

状态：local diagnostic complete；R2 收益未确认，R2/R4 未退出。

日期：2026-10-26（Asia/Shanghai）。当前状态由 [CURRENT_STATUS](../../../../../CURRENT_STATUS.md) 持有，
范围与停止门见[整改计划](../../../../../stage_plans/active/stage-07/05-rpa-new/plan.md)。

## 1. 本轮范围与身份

用户要求继续推进 R2，随后明确要求“重新运行B-only 诊断”。本轮先分析旧 raw，再执行一次独立
B-only 环境诊断；之后仅按明确重跑指令执行一次新的 B-only 诊断。两次均独立登记、各运行一个
读取进程；没有新增产品机制、修改源码或给已关闭的六进程窗口追加样本。

主树 HEAD 仍为 `cccbd47eda8ecd65e018ec31063ec027c39bd04e`，B 为上一轮绑定的真实 R1 读取
二进制。N1/N64 每块分别读取 2097152/262144 次，每规模八块、每块至少 100 ms，
fresh-session 64-step 预热、普通 allocator、公开 reusable extractFrame/read_long、native mask=4。
每次先有独立 30 秒机器热身，外部 PDH 观察器固定 mask=1，每秒采集一次；产品无插桩。
没有运行 ETW/WPR、40k、设备或正式 RPA 接受。

第一次诊断原始材料：`D:/Cuexis-worktree/out/r2-noise-20261026/`。
显式重跑原始材料：`D:/Cuexis-worktree/out/r2-noise-rerun-20261026T064152Z/`。
后者的 `rerun-request.json`、独立 registration/claim、raw、telemetry、results 和 verification
保留授权、身份及前后哈希。旧诊断和原六进程的文件未替换，当前登记绑定复核有效。

## 2. 旧日志定位与新观察

旧六进程中，首个 B 的 N1 第 0–2 块约 265 ns/次，第 3 块起约 331–340 ns/次，
线程 CPU 耗时和 QueryThreadCycleTime 周期计数也同步上升。单纯墙钟等待变长不足以解释该变化；
现有旧记录不能确定具体原因。逐块分析保存在第一次诊断的 `existing-raw-analysis.json`。

第一次 B-only 诊断复现 N1 前三块约 266 ns/次、后续 325–349 ns/次；同时目标核的
系统报告频率从 3722 MHz 降为 3040 MHz，Processor Performance 计数从约 97% 降至约 80%。
在随后的 N64 读取期间仍报告约 3040 MHz。目标核忙碌率大多为 100%，未在变化点观察到
对应的 DPC/中断峰值。这里只确认同一诊断中的时间关联，未证明旧窗口具有同一个原因，
也未确定系统电源策略、温度或硬件限制中的具体责任项。

显式重跑没有修改电源方案、进程 QoS 或优先级。开始前只读记录为平衡方案，
最大处理器状态 AC/DC 均 98%，最小 AC=80%、DC=5%。读取区间的 PDH 报告频率保持
3722 MHz，Processor Performance 为 97.26%–97.36%；这次没有观察到前一次读块中的频率状态下降。
温度与硬件有效时钟没有直接采集，不以当前配置快照回填旧窗口。

## 3. 两次独立 B-only 结果

| 规模 | 第一次中位数 ns/次 | 第一次块内范围/中位数 | 显式重跑中位数 ns/次 | 重跑块内范围/中位数 |
| --- | --- | --- | --- | --- |
| N1 | 329.27 | 25.28% | 273.19 | 11.23% |
| N64 | 5057.13 | 1.75% | 4246.76 | 3.85% |

两次热身与读取进程均 exit=0，各 16 个读取块满足时长门。第二次 raw 的全部 16 块已独立复算，
fixture identity/digest、源码/构建绑定、旧登记/结果哈希及观察器退出均通过核验。
当前第二次的两个规模块内范围低于 15%，但单进程块内稳定不等于跨进程重复性，更不等于 E/B 接受。

PDH 为外部观察器，采样及观察器运行都可能扰动执行，1 秒分辨率不能定位到单条指令或
亚秒块内的确切切换。QueryThreadCycleTime 的周期不能直接换算 CPU 频率或耗时，见
[微软 API 说明](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-querythreadcycletime)；
本轮一秒采样口径参照[性能计数器说明](https://learn.microsoft.com/en-us/windows/win32/perfctrs/about-performance-counters)。

## 4. 当前处置

本轮取得了“B 读取变慢与系统报告处理器状态下降同时出现”的新证据，显式重跑显示另一种
稳定在 3722 MHz 的状态。两种状态的来源和触发条件仍需确定；没有用频率归一化修正旧样本，
没有删除慢块、替换旧进程或将新 B-only 结果合入原三对区间。

下一步围绕处理器状态切换建立可控诊断：先解释其触发条件与可观测边界，再决定后续独立
验证安排。第三候选保留，原六进程的收益未确认结论不变；R2/R4 尚未退出，R3 未启动。
本轮全部产品代码、原错误/生命周期合同、15% 噪声门及原 1.10/95% 目标保持。
