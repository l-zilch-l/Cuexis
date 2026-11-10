# PD-5 管理员 canary 磁盘阈值中断与最小验证 profile

日期：2026-10-23。结论：原 Power normal 用例失败，紧急回收证据通过；第二项未运行。新最小生命周期工具包完成源码修正、只读解析与 41 项验证，尚未执行管理员记录。正式性能、PD-5 和 Stage 7-PD 均未接受。

## 原始执行与独立裁决

原封包：`out/pd5-etw-tool-canary-20261023-3e5837f`；用户实际执行组：`canary-20261023T105430Z-8d7faec2f9024d418f229d8c90643f53`。源码 binding SHA-256 仍为 `818c9400b043ff1c180b05708988c96fe98a09372c280f0d4d71fbdfdebf61cf`；原源码、回执及两个 raw ETL 不修改，不重跑。

| 记录 | 原始证据与结论 |
| --- | --- |
| WPR start | `normal/start-result.json` exit 0、Job active=0；两项精确 owned logger 随后可见 |
| 正常 stop | `normal/stop.log` 于 10:54:34.6720971 UTC 创建，但没有 stop-result 或 merged canary.etl；不能认定正常停止成功 |
| 中断原因 | guardian 明确记录 `tool-only 128MiB disk cap`；takeover 位于 case 起点后 11.4595754 秒 |
| controller / guardian | 外层回执记录 exit 1 / 0；normal passed=false，released_clean=false，takeover=true |
| 紧急 STOP | 只针对 `PD5F1Noise_c0b9b553326a46879af51d18310bee8e` 的 Event/System 两个精确 logger，均返回 0 |
| 回收证明 | Job active=0；raw helper exit=0 且已等待退出；guardian 与外层最后的 elevated QueryAllTraces + 两项精确 query 均确认 owned absent |
| 后台保护 | 最后清单中的四项 DiagTrack name/logfile/GUID 与本组管理员 preflight 一致；不以事件计数不变作要求 |
| 第二项 | `controller-death` 目录不存在；没有继续或补样 |

raw Event ETL 最终 148,897,792 bytes（142 MiB），System ETL 72,351,744 bytes（69 MiB），合计 211 MiB。它们是停止后保留的最终文件长度，不是 guardian 触发瞬间的精确总量；旧代码没有保存触发文件清单。两个文件当前哈希已登记，读取哈希期间 size/mtime 稳定。保存的管理员末尾查询支持当次清理已完成；本会话没有用非管理员空清单冒充新的实时 ETW 查询。

直接触发来自本工具的磁盘阈值。正常停止在 guardian 介入前尚未给出完成证明，因此这次既不证明 normal stop 内在故障，也不能作为 normal stop 通过。此前所谓“空载”只表示未启动 Cuexis，Power 仍采集整机事件。主代理在没有实测核对 Power 与 128 MiB 工具阈值适配的情况下交付了该验证配置，准备不足；保留失败，并缩小后续工具验证内容，不提高阈值。

没有 Cuexis/F1/40k、pilot 或正式窗口。紧急回收的实测证据不能替代预声明的 controller-death 用例：本次发生于正常 stop 已请求之后，而预声明第二项是在请求 stop 之前退出 controller。

## 窄范围修复

仅改隔离树[ETW 工具](../../../../../../../tools/verification/playback_performance/etw_safety/README.md)，不改产品源码或物理/哈希/性能预算。旧封包和历史输入保持原样。

- 新增固定 `lifecycle.wprp`：SystemProvider 只选 ProcessThread；EventProvider 只选 Microsoft-Windows-Performance-Recorder-Control 的 `0x40000`。两个 collector 仍使用原精确 suffix，每个声明 64 KiB / 16 buffers，没有 CPU sampling、CSwitch、DPC 或 stacks。
- binding v2 纳入 XML hash；bootstrap 在任何本地模块 import 前核对。固定 profile/name 同时用于 preflight 与 start，没有 Power fallback。XML 还核对 provider/collector/keyword/mode/reference 合同。
- 保留 128 MiB 周期磁盘触发、20 秒 active 周期触发、120 秒 controller/guardian 期限及其他回收限制；不以扩大包络绕过本次失败。声明的 buffer 配置不等于实测磁盘/内核内存峰值，实际容量仍待验证。
- 新增 start/stop command intent 与 spawn 回执；下一次磁盘触发会记录总字节数、阈值和文件清单；终端会给出 guardian reason，避免仅显示通用失败。
- 继续保持单一停止权、Job zero 后才可 raw STOP、每 case 一次 raw helper、同一 token 的精确 absence 证明、首败停止和不补样。

profile 结构采用 [Microsoft 自定义 WPR profile 的 System/Event collector 示例](https://devblogs.microsoft.com/performance-diagnostics/authoring-custom-profiles-part-2/)与[provider 定义](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/2-system-and-event-provider-definitions)。这里刻意缩小事件范围以验证生命周期；不是 Power 性能诊断的替代数据，也不证明可选 F1 Power 采集器已经就绪。

## 验证与封包

新工具目录：`out/pd5-etw-lifecycle-minimal-20261023`。binding SHA-256 `947561b8b0d949f1a9baa393df810a1544859eec95214b2497072fe646b6a1ae`；launcher SHA-256 `27cbb3967b92cdf85ec6a20c0d069028cb8a4dc830589993c9bb3f33c56f384f`。没有执行 launcher。

`out/pd5-etw-canary-adjudication-20261023` 保存 `failed-run-inventory.json`、`adjudication.json` 与原始验证日志。`verification.json` SHA-256 `cb083df9340a13138298ef327980126ae5c2c631810547c1afc6810261f7dc9e`，记录：

- `python -B -m unittest discover -s tests/pd -p 'pd5_etw*_tests.py' -v`：41 项通过，包含 8 项实际 Windows Job/Python 子进程验证；没有真实 ETW API 或 WPR recording。
- 本机 WPR `-profiledetails <sealed lifecycle.wprp>!PD5ToolLifecycle` exit 0，解析为 Light.File、两个预期 suffix、ProcessThread 与 WPR Control `0x40000`；这是只读配置解析。
- sealed help exit 0；在新副本篡改 XML 后，bootstrap exit 2 且在 import 前拒绝；原封包不变。
- 原失败组完整文件集合/bytes/hash 清单；四项已知后台 identity 保留；最终清理与整体失败分项裁决。

无上下文 Luna 的独立核验保存在 `out/pd5-etw-canary-failure-review-20261023`。管理员两项最小 profile 用例仍未执行，正常 stop、正常 merged ETL、controller-death 用例和此新 profile 的实际体积均未验收。没有 C++ 改动，本批不构建或运行 CTest。

## 当前边界

现在不需要依据本组证据再手工停止任何 logger。将来若执行新工具，必须使用新目录的一次固定两项检查；原失败组保持失败，任何失败即停止，不自动重跑。新最小 profile 即使通过，也只验证生命周期，不认证未来 Power 诊断或正式性能。

无 ETW 正式 profile 的候选、内存归因和 P0–P8 准备见[前批方案](../preparation/2026-10-23-preformal-tool-repair-and-readiness.md)。可选 ETW 不能扩大为所有正式准备的前置。主树仍是受保护的 F1 `eed36648a692333e47f798aa45be36ca850d08de`；C2、old golden、原 40k low/high 与历史证据不作为本轮写入目标。七项物理预算、四项 hash ceilings、1.10/15% 与 reply deadline 30 秒不变；旧窗口3、公开停止接口、merge/push 继续停止。
