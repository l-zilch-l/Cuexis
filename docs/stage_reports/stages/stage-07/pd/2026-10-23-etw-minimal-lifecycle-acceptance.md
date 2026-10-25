# PD-5 最小 ETW 生命周期工具两项实机验收

日期：2026-10-23。结论：用户完成新封包的一次固定两项管理员运行，主代理按原始记录独立核验通过。验收限于最小 profile 的正常停止与 controller-death 回收，不包含 Power 性能诊断、F1 采样、正式性能或 PD-5/Stage 7-PD 退出。本批不再需要管理员脚本。

## 实际运行与绑定

封包：`out/pd5-etw-lifecycle-minimal-20261023`。运行组：`canary-20261023T111443Z-4234d8fdef04468a89c5d3daa0a258bf`。两项顺序为 normal → controller-death，各一次；`auto_retry=false`、`native_launches=0`、`formal=false`，总控制器退出 0。

独立复算封包 binding SHA-256 `947561b8b0d949f1a9baa393df810a1544859eec95214b2497072fe646b6a1ae`，launcher SHA-256 `27cbb3967b92cdf85ec6a20c0d069028cb8a4dc830589993c9bb3f33c56f384f`。9 项 Python 来源、固定 XML 及 Python/WPR 可执行文件均匹配冻结 hash；当前工具源码与实际交付副本相同。两个不同 fresh nonce 的 claim、guardian-ready 与 harness-attached 中 PID/creation-time/Job/claim-hash 逐项一致。

profile 为 `PD5ToolLifecycle.Light.File`，只有声明的 ProcessThread 与 WPR Control `0x40000`、原两项 logger suffix；没有回退到 Power。start-command-intent 绑定本封包 XML/name、filemode、本次 trace-temp 和相同 instance。两个 case 的 preflight 均无未分类会话冲突。

## 两项分开验收

| 项 | normal | controller-death |
| --- | --- | --- |
| controller / guardian exit | 0 / 0 | 预定 73 / 0 |
| WPR start | exit 0、Job0 | exit 0、Job0 |
| 边界证据 | 正常 WPR stop exit 0；无 takeover 或 raw-stop authority | benign worker ready，Job active=3；预定 exit73，before_stop_request=true；没有 normal stop intent/log |
| 结束路径 | released_clean=true，guardian 结束 | reason=controller exited，takeover=true，raw-only |
| 进程/ETW | Job active=0，最终 owned/exact 均空 | Job active=0；两项精确 STOP 各 code0；raw helper exit0 且已等待；最终 owned/exact 均空 |
| trace | merged ETL 3,932,160 bytes；临时 ETL 空 | raw Event 131,072 bytes，System 2,686,976 bytes；合计 2,818,048 bytes；不要求正常合并 |
| 独立最终查询 | guardian receipt 之后的外层 elevated query 再证 absence | guardian receipt 之后的外层 elevated query 再证 absence |

normal ETL SHA-256 `326dfb38fbe2a7ad3b5d528d581c128290adda73f29ae5f098cfcd1760eef570`，与原正常完成回执一致；哈希读取前后 size/mtime 稳定。stop 日志保存 `The trace was successfully saved.`。controller-death 的 Job active=3 是退出前的原始见证，随后归零；不能将计划退出 73 误当非预期失败，也不能把 raw-only 当正常合并。

每项最终清单中的四项已知 DiagTrack name/logfile/GUID 均与各自 preflight 相同。仅核对保存的同一管理员执行末尾证据，本批没有重新调用 ETW API、WPR 或启动任何进程采集；不使用非管理员空清单推断当前实时状态。

这些是短生命周期用例的结果，不是时延分布、磁盘峰值或硬实时保证。正常/异常用例保留体积不能代替未来 Power 或 420 秒 F1 诊断的容量证明，也不覆盖服务侧 merge wedge、全部权限/设备故障或同时失去 controller/guardian/harness 的情况。

## 独立证据与保护

主代理验收位于 `out/pd5-etw-lifecycle-acceptance-20261023`：

- `audit.py` 只读取既有证据、计算 hash，检查 claim/ACK、精确命令、退出码、worker 见证、STOP/helper、背景 identity、最终 absence 与 trace；不运行 collector。
- `run-inventory.json` 封存原组 67 个文件的集合、字节数与 SHA-256，不向原组写入。
- `acceptance.json` SHA-256 `0758bafc2440fbecf4f8101c4a948d3a6acde8b0b127621d9262b1fdba3145ac`，明确 `performance_acceptance=false`、`power_profile_certified=false`、`formal=false`。

无上下文 Luna 对原始 JSON、profile 和绑定的独立核验保存在 `out/pd5-etw-lifecycle-pass-review-20261023`；该复审不执行脚本、ETW、WPR 或 native。工具代码本批未改，无需重复前批 41 项工具测试；文档检查与 whitespace 检查另行执行。没有 C++ 产品改动或构建/CTest。

原 [Power normal 磁盘阈值失败](2026-10-23-etw-canary-disk-stop-and-minimal-profile.md)及其 211 MiB raw ETL、38 项原始文件清单和 source binding 保持失败历史，不被本次结果覆盖。也没有改写旧 WPR 包装器失败、原 40k/小规模寿命诊断、C1/C2、old golden 或旧正式组。主 F1 树仍干净，HEAD `eed36648a692333e47f798aa45be36ca850d08de`。

## 工具收口与后续边界

“最小生命周期工具修复与两项管理员实机验证”本批收口。可选 Power 采集仍单列未验收，F1 collector 未接入；不把它设为所有正常分配器、无 ETW 正式准备的前置，也不据本次通过自动启动 Power/F1/40k 或其他测量。

按此前六组剩余工作的原始范围，第一组“修好测量工具”整体仍为**部分完成**：实际 F1 collector 尚未接入修复后的控制器，其超时/中断、子进程回收、ETW 清理及退出确认的整条链路仍待集成验收。已通过的最小生命周期检查只完成其中基础部分；“本批收口”不等于第一组全部完成。完整 profile/正式矩阵仍归第四组，不在此扩大第一组范围；可选 ETW 也不成为无 ETW 工作的必需前置。

回到[正式准备方案](2026-10-23-preformal-tool-repair-and-readiness.md)：先处理一个 ordered visitor 读取候选的语义/错误顺序/代码生成与维护性验证，以及 worker-aware 分域内存观测、外部 owning 值和最终释放证明；随后补齐本机有限 P0–P8 profile、源码/build/输入/端点与适用构建消费者清单。候选、40k 新观测或正常分配器 Seek 都不能借用旧运行身份。

正式测量仍须新准备、一次 pilot、范围核实与注册/claim/seal 检查，再按原三窗、600 秒间隔和固定配对执行。七项物理预算、四项 hash ceilings、1.10/15% 与 runtime reply deadline 30 秒均不变；一轮只登记一个性能候选，旧窗口3不恢复。大型公开停止接口、merge/push 继续停止。本次工具收口不关闭 PD-5、Stage 7-PD、RPA、生产或整个 Stage 7。
