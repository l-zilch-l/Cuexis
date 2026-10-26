# PD-5 管理员只读预检：已有 WPR 会话拦截

状态：preflight stopped；会话归属待查；capture 与 PD-5 acceptance 未通过。

更新日期：2026-10-23，Asia/Shanghai。前序准备与边界见
[v3 只读预检报告](2026-10-23-readonly-etw-preflight-and-next-evidence-plan.md)。

owner 运行 v3 默认只读入口后，回执
`out/pd5-f1-noise-control-v3-20261023-e6959a9/preflight-b49158ab537b4feba96beac386f9f539.json`
记录 NOT RUN，原因为 existing WPR ETW sessions found；WPR start/native launch 均 false。
主代理核验原文件及冻结源码：本路径已通过管理员检查、profile privilege 检查，并执行了
QueryAllTracesW；随后发现以 WPR_initiated_ 开头的会话并停止，没有触碰任何录制。
这不是本次权限不足，也不证明剩余的默认 WPR/profile 检查已通过。

原异常分支没有把局部 sessions 清单写入回执，导致无法从本次失败文件识别已有会话。
保留该回执和原 v3 工具字节，不补写历史结果；另立
`out/pd5-etw-session-inventory-20261023-c1fa5e9` 的只读 inventory 入口。

新入口先核对 Python hash 与已复审 v3 manifest/9 artifacts，再在管理员检查后调用相同的
QueryAllTracesW，保存完整可查询清单，并向终端显示 WPR 名称和 logfile。只比较旧 PD-5
nonce 的名称是否匹配；名称匹配本身不构成所有权证明或停止授权。没有 start、stop、cancel、
F1、40k、正式注册或自动提权路径。原始异常与会话归属未决状态都不豁免已有会话拦截。

三个小工具检查通过：旧名称与未知名称均不授权 stop；mutating CLI 参数拒绝；冻结 import
hash 校验通过。真实普通权限运行只产生 INVENTORY UNAVAILABLE 回执，四个动作 flags
均 false，未执行 ETW 枚举。管理员 inventory 尚待 owner 运行并独立核验。
无上下文 Luna 独立复审只读入口通过：3 项本地 artifact、Python、原 v3 manifest 与 9 项
import artifact 的实际 hash 一致。manifest 自身以独立复审记录外部固定；import 完整性失败
会在回执创建前直接退出，仍不执行查询，不将这条路径称为已有完整错误回执。

既有 `after-failure.etl` 仍为 7,027,556,352 bytes，修改时间未变；这只说明已保存文件稳定，
不能由非管理员状态或该文件稳定推断当前所有管理员 ETW 会话已清理。下一步先取得会话
名称和路径；不重跑采集、不停止未知/他人会话，不据 WPR 前缀自动清理。

本批只更新报告、索引和状态；产品、主树 F1、C2、old golden 和历史采样不变。七项物理
预算、四项 hash ceilings、1.10/15% 和 reply deadline30秒保持；大型停止接口、旧窗口3、
正式测量/注册及 merge/push 继续停止。
