# PD-5 DiagTrack 命名会话清单与预检规则修正

状态：管理员 inventory 已核验；只读分类已修正；实际 capture 与完整预检仍未通过。

更新日期：2026-10-23，Asia/Shanghai。前序见
[已有 WPR 会话预检拦截](2026-10-23-etw-existing-session-preflight-stop.md)。

## 原始清单与判定

owner 返回只读 inventory 回执
`out/pd5-etw-session-inventory-20261023-c1fa5e9/inventory-92bd2abc05ac45129e772d3970656b84.json`。
主代理核对 administrator=true、INVENTORY COMPLETE、script/v3 binding SHA，全部动作
flags 与 stop_authorized=false。原始 SHA 为
`78f76ea21de91e4cdabf5ca578dec951cb5b28c073f25fc420718dfbbb4f0d61`。
该次可查询快照共 39 个会话，其中四个 WPR 名称为：

- `WPR_initiated_DiagTrackMiniLogger_OneTrace_User_Logger_20261016_1_EC_0`
- `WPR_initiated_DiagTrackMiniLogger_WPR System Collector`
- `WPR_initiated_DiagTrackAotLogger_WPR System Collector`
- `WPR_initiated_DiagTrackAotLogger_Hello - Core`

四项均未报告 logfile，且旧 `PD5F1Noise_1c28748529334603b99715aec1219127` 的两个确切
logger 名均不在此次清单。本机只读服务查询另见 DiagTrack 以 LocalSystem 运行，显示名
Connected User Experiences and Telemetry；这与名称所指的诊断背景一致，但不是逐会话
creator PID 的直接证明。Windows 诊断组件背景见
[Microsoft 文档](https://learn.microsoft.com/en-us/windows/privacy/configure-windows-diagnostic-data-in-your-organization)。

v3 对所有 WPR_initiated_ 前缀一律拒绝，在该快照中产生了过宽拦截。不能据此要求停止
DiagTrack、关闭诊断服务或执行全局 WPR stop/cancel；原始回执、服务配置和后台会话均不改。
此结论也不证明空闲环境、频率稳定或 15% 噪声条件已满足。

## 新的窄范围规则与验证

新工具位于 `out/pd5-etw-background-policy-20261023-0654129`。仅把本次四个确切
name/empty-logfile tuple 列为已记录后台；保留原始行、lost/buffer 计数。未知 WPR 名称、
相同名称但 logfile 改变、带或不带 WPR 前缀的 PD5 名称均继续拦截。名称分类不授予 stop
或 capture 权限，不把累计 lost 计数当作某个未执行测量窗口内的丢失量。

离线复算该真实快照得到 NO UNCLASSIFIED WPR CONFLICT IN SNAPSHOT，仍显式保持
capture/stop/quiet/full-preflight=false。这没有把历史 v3 的失败改成通过。

新只读入口整合该规则，并在异常前保留全部 inventory、分类、已执行只读命令的原始字节/
退出码、source/binding SHA，避免前一版遗漏会话名称。仅允许 whoami /priv、WPR status、
Power profiledetails 和 ETW query；没有采集入口。最终 11 项测试通过，覆盖真实 39 行 fixture、
未知会话阻止及完整部分证据、默认 WPR active 失败、capture 参数拒绝和分类边界。

独立复审发现的 raw PD5 名称过滤顺序、local policy 模块应在 hash 校验后导入、命令异常前
应先记录尝试三项已修复；补测 timeout 的部分 stdout/stderr 和启动失败记录。原评审与
各次修复前脚本/binding 单独保留，最终修复由主代理检查并运行上述 11 项测试。
最终普通权限验证仍停止在 token guard，无 ETW 查询/变更。新入口没有管理员
实际执行结果，原 v3 的默认 WPR/status 和 Power/profile 检查也尚未补齐，不能宣称完整
预检通过。当前不要求 owner 再执行命令；先完成采集清理设计复审再安排必要的管理员步骤。

## 保留范围

本批没有停止后台会话、改变系统策略、运行 F1/40k 或正式窗口。原 v3/inventory 工具和
两次管理员回执保持原字节；主树 F1、C2、old golden、原 40k 与历史封包保持。
产品代码未变；报告、索引和当前状态执行文档检查。原预算、1.10/15%、reply deadline30秒
不变，公开停止接口/旧窗口3/正式注册/merge/push 继续停止，Stage 7-PD 尚未退出。
