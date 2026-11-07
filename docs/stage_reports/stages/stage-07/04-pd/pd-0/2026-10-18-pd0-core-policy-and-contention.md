# PD-0 固定核心正式组与 SMT 竞争诊断

状态：历史正式组 inconclusive；本组不作为 PD-0 退出证据

日期：2026-10-18（Asia/Shanghai）

本组的失败结论与全部样本保持不变。后续经 owner 提供受控环境、修复构建绑定并重新登记的完整组已通过，PD-0 的最新退出证据见[受控环境与构建绑定报告](2026-10-18-pd0-controlled-environment-and-build-binding.md)。下文“当前”均指本历史组及诊断结束时的状态，不代表后续工作的结论。

本轮继续完成接手时尚未执行的第二窗口。历史结果见
[测量波动方案](2026-10-17-pd0-measurement-variance-resolution.md)和
[补充复测](2026-10-17-pd0-supplemental-retests-and-archive.md)，其结论不变。
本报告记录新的完整固定组，不用较好的第二窗口或后续诊断替换失败样本。

## 注册与独立核验

注册：`out/pd0-core-policy-formal/registration-20261017T174313Z.json`，SHA-256 为
`af5de633918c3a0ebd83e832ae61312bc64a9576b90e3d87f791deb98f9f0275`。
HEAD 为 `ef7fa30d5b578896ccc165622fe256c553a5641c`，未提交源码库存及二进制散列随注册保存。
正式核心在读取正式结果前固定为拓扑首个非 CPU 0 候选，逻辑 CPU 2、掩码 `4`；
完整物理核掩码为 `12`，二者不能混称。pilot 分数不用于事后择优。

两个窗口：

- `out/pd0-core-policy-formal/window-1-20261017T174331Z-ed0690/`
- `out/pd0-core-policy-formal/window-2-20261017T180839Z-114b04/`

第二窗口于本地 02:08:39 开始，02:14:00 完成。按第一窗口完成时间与第二窗口 claim
时间计算间隔为 **1115.675 秒**；启动前单调时钟检查也满足登记的 600 秒下限。
沿用原注册、源码与冻结二进制，Release/Debug、legacy/RPA，正常分配器、编译插桩关闭、无 ETW。
没有改变优先级、电源策略或后台程序，没有追加、替换或删除正式样本。

36 个测量进程全部成功：24 个单 owner、12 个宿主；576 个 owner 块、6144 个宿主测量帧。
192 个长读取块均超过 100 ms，最短 **361.821 ms**。
原始日志、语义身份、QPC 边界、源码/二进制绑定及电源指纹检查通过。
独立审计从日志重算每进程中位数、宿主均值、窗口和合并跨度，与 state 一致。

审计入口为 `out/audit_pd0_core_policy.py`；产物为
`out/pd0-core-policy-formal/independent-audit-20261018.json`，保留注册、state、claim、日志与 manifest 的散列。
冻结的宿主二进制没有原生亲和性字段；审计只核对其 child 命令的默认允许 CPU 掩码，
不宣称已得到宿主原生亲和性或 worker 连续调度观测。runner 检查 WPR 未录制，
但未保存每次状态响应；该留存限制沿用原协议报告。

## 正式重复性结果

以下都是 `(max-min)/median` 的进程级百分比，两个三进程窗口及合并六进程均须 ≤15%。
`validated_trace` 包含逐步验证和 FrameDigest，`playback_only` 只计推进，
`read_long` 为固定长读取块摊销成本；三者分别判定。

| 配置/profile | 指标 | 窗口一 | 窗口二 | 合并六进程 |
| --- | --- | ---: | ---: | ---: |
| Release legacy | validated_trace | 2.30% | 3.35% | 3.53% |
| Release legacy | playback_only | 1.28% | 4.08% | 4.11% |
| Release legacy | read_long | 17.75% | 4.50% | 21.86% |
| Debug RPA | validated_trace | 37.09% | 4.22% | 43.86% |
| Debug RPA | playback_only | 48.20% | 3.16% | 64.07% |
| Debug RPA | read_long | 45.40% | 2.17% | 72.09% |
| Release RPA | validated_trace | 2.91% | 0.74% | 2.93% |
| Release RPA | playback_only | 4.95% | 7.33% | 7.39% |
| Release RPA | read_long | 15.80% | 3.78% | 17.46% |
| Debug legacy | validated_trace | 8.54% | 3.61% | 31.86% |
| Debug legacy | playback_only | 19.87% | 3.38% | 39.95% |
| Debug legacy | read_long | 19.29% | 2.08% | 43.00% |

| 宿主配置 | 窗口一均值跨度 | 窗口二均值跨度 | 合并六进程均值跨度 |
| --- | ---: | ---: | ---: |
| Release | 12.34% | 6.30% | 17.69% |
| Debug | 1.33% | 4.75% | 10.42% |

owner 和宿主独立状态均为 **inconclusive**。第二窗口自身较稳定，不能消除窗口一及跨窗口失败。
宿主失败项是均值的重复性；尾延迟预算未接受，不能将诊断参考阈值称为尾延迟预算超界。
各类交付延迟的描述统计仍保存在 state 中。

## 有界诊断与资料核对

接手时的假设是单逻辑 CPU 掩码未排除 SMT 竞争。Microsoft 明确
[进程亲和性](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setprocessaffinitymask)
只决定进程可使用哪些逻辑处理器，不为其保留物理核。因此掩码 `12` 允许同核两个逻辑 CPU，
不能声称隔离了其他程序或禁用了 SMT。
[Google Benchmark 的波动说明](https://google.github.io/benchmark/reducing_variance.html)
列出 SMT、竞争、频率及缓存等可能来源，不能仅凭症状确定其中之一。

新增 Windows 诊断入口 `tests/pd/diagnose_core_contention.py`，固定 8 个 Debug/RPA 进程，
掩码顺序为 `4,12,12,4,12,4,4,12`，其余固定工作量、正常分配器及无编译插桩二进制不变。
一次 30 秒机器预热，不按中间结果改变顺序、增加进程或挑选核心。
诊断注册、原始日志和系统计数位于
`out/pd0-core-diagnostic/core-diagnostic-20261017T181425Z-aaae06/`。

系统计数使用语言无关的
[PdhAddEnglishCounterW](https://learn.microsoft.com/en-us/windows/win32/api/pdh/nf-pdh-pdhaddenglishcounterw)，
覆盖 16 个逻辑 CPU 的忙碌、报告的性能/频率、DPC 和中断计数，共 80 项。
按 [Microsoft 的性能计数使用边界](https://learn.microsoft.com/en-us/windows/win32/perfctrs/about-performance-counters)
每秒采集一次；QPC 边界用于对应整个进程中的完整采样区间，不作亚毫秒调用归因。
采集自身开销未单独校准，数据只作诊断，不能混入无观察器的正式结果。
线程 cycles 不转换成 GHz，符合
[QueryThreadCycleTime 的限制](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-querythreadcycletime)。

8 个诊断进程全部正常退出，源码、二进制、电源及跨进程语义身份核验通过。
245 次系统采样无计数错误，单次 PDH 收集最长 1.494 ms；这只是收集调用耗时，
不是已校准的总观察器开销。汇总脚本为 `out/analyze_pd0_core_diagnostic.py`，
最终结果保存在 `telemetry-summary-final.json`，原始证据另有 `evidence-hashes.json`。
运行中的六进程/213 次采样派生摘要曾被分析器覆盖；它不是原始测量数据。
归档核验时已逐字节恢复与原散列完全相符的中间摘要 `telemetry-summary.json`，
将最终分析另存并记录 `analysis-provenance.json`。原始日志、计数和 manifest 均未改写，
原证据散列清单重新核验全部一致；分析器现以独占创建方式防止覆盖既有摘要。

| 掩码 | validated_trace 四进程跨度 | playback_only 四进程跨度 | read_long 四进程跨度 |
| --- | ---: | ---: | ---: |
| 4：单逻辑 CPU | 4.56% | 3.28% | 2.32% |
| 12：同物理核两个逻辑 CPU | 0.84% | 0.35% | 2.24% |

只取原生进程 QPC 锚点内完整的系统采样区间，掩码 `4` 的四个进程中，
CPU 3 平均忙碌分别为 **0.79%、1.37%、0.88%、1.32%**。
掩码 `12` 时 CPU 2/3 之间可迁移，两个逻辑 CPU 的忙碌百分比之和约为一个线程的负载，
不是两条满载工作线程。八进程中 CPU 2 报告的平均 Processor Performance 约为 **97.36%–97.37%**；
不能据此推出原生频率恒定、零竞争或历史窗口的状态相同。

两种掩码在本次诊断中都较稳定，没有重现原正式组的严重慢态。
因此只能得到“本轮未观察到明显的持续 SMT 兄弟争用及报告性能水平漂移”，
**不能把 SMT、频率或系统调度写成历史根因，也没有依据直接改正式核心策略或重注册一个较好时段**。
此诊断只有 Debug/RPA、单时段和系统观察器，不能充当正式四单元两窗口证据。

## 验证与后续边界

诊断前通过原有六项正式 runner 单元测试，以及 80 项原生 PDH 计数的现场校验。
完成后重跑正式 runner、电源/QPC 对齐和统计测试，并执行文档检查及 `git diff --check`；
具体命令与结果保存在 `out/pd0-core-policy-validation-20261018.log`。
本轮只新增诊断 Python 和文档，没有修改或重新构建产品 C++；未将这些检查称为整个 SDK 矩阵通过。

当前没有可审计的、已验证能消除跨窗口波动的环境修复，PD-0 继续未退出。
当前进程不是 Windows 管理员，不能在该会话中启动所需内核调度追踪；没有尝试取消现有追踪或绕过权限。
后续优先在能够保留慢态证据的管理员环境中采集 CPU 调度/Power 追踪，或转固定电源与散热条件的性能环境。
仅固定 HighQoS 也只是候选条件；
[Microsoft QoS 文档](https://learn.microsoft.com/en-us/windows/win32/procthread/quality-of-service)
说明它会影响电源与调度，当前 0/0 显式标志仍允许系统自行分类，不能据此宣称没有自动 QoS 影响。
未验证前不更改系统策略、注册输入或再追加正式样本。

正式重复性、长块、同源码存储/阶段观测及受影响门禁均满足后才能关闭 PD-0。
PD-1 至 PD-5、RPA 实体设备/生产预算与 owner 接受均未因本轮证据关闭。

## 同进程时序复核与下一次追踪准备

随后继续复核全部 576 个原始 owner 块，未新增测量、筛选样本或改变正式统计。
结果保存在 `out/pd0-core-policy-formal/within-process-analysis-20261018.json`。
窗口一的 Debug/legacy 进程 PID 4788，其八个 `read_long` 块为
16.324、19.664、11.096、11.042、21.519、21.987、16.601、15.847 μs/操作，
同进程最大/最小比为 **1.991**；Release/legacy PID 8140 同指标比为 **1.884**。
部分慢态在同一进程和固定工作量内变化，因此“仅由静态进程地址布局差异造成”不足以解释数据。
这是对现有数据的诊断推断，不排除内存/缓存因素，也未区分执行速度、SMT、调度及动态系统策略。

旧 `collect_system_runtime.py` 引用的
`out/pd0-scheduling-v2/calibrate-20261017T043818Z-453e2a/manifest.json` 已不存在，
该历史入口不能直接用于本轮冻结二进制。为后续实际归因新增
`tests/pd/capture_formal_runtime.py`，独立准备一次三进程 CPU/Power 诊断：

- 顺序固定为 Debug/RPA、Release/RPA、Debug/legacy，各一个完整进程，沿用正式批长和掩码 `4`；
- 正常分配器、阶段编译插桩关闭；ETW 只用于诊断，不能作为正式重复性通过证据；
- 导出并冻结当前 WPR 合并配置及可执行文件散列；现场检查配置包含 CSwitch、ReadyThread、SampledProfile、Power、DPC、Interrupt；
- 每个记录使用独立命名会话；已有录制或状态不明时拒绝开始，start 失败不 stop/cancel，工作失败后只保存自己成功启动的会话；
- 采集完成仍须检查 ETL 丢失事件、QPC 时间覆盖、慢态是否重现及栈/Power 归因，不能将 `diagnostic_collected` 当作根因已确定。

命名会话、配置导出和停止保存语法依据
[Microsoft WPR CLI](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/wpr-command-line-options)，
本机 `wpr -help start` 也确认支持 `-instancename`。
七项会话归属/失败清理测试通过；当前权限下实际执行 capture 入口，会在读取注册、
创建 claim 或启动预热/追踪之前以 PermissionError 拒绝。
`whoami /priv` 确认当前令牌不含 SeSystemProfilePrivilege，Administrators 组为 deny-only，
也不属于 Performance Log Users；不是仅凭 IsUserAnAdmin=0 作权限判断。
[ETW 控制权限](https://learn.microsoft.com/en-us/windows/win32/etw/controlling-event-tracing-sessions)
及当前令牌均指向需要外部提供合适的采集权限，未修改用户组或系统权限。

准备好的注册为：
`out/pd0-uninstrumented-runtime/registration-c3dfce69c1684b5c8b79defae1ffe1dd.json`。
已核对源码、二进制、电源、WPR 和导出配置绑定，claim 尚未创建，**实际 ETW 采集尚未执行**。
在合适的管理员 Windows 终端中可执行：

```powershell
Set-Location D:\Cuexis-worktree
python -B tests/pd/capture_formal_runtime.py capture --registration out/pd0-uninstrumented-runtime/registration-c3dfce69c1684b5c8b79defae1ffe1dd.json
```

如果执行前实现或环境发生变化，入口会拒绝开始；必须核对变化后重新准备诊断注册，
不能为越过检查改写既有注册。此准备没有追加正式采样，PD-0 状态不变。
