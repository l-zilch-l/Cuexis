# 2026-10-26 Worktree consolidation and verification

状态：实施与验证进行中。本报告记录用户授权的工作树整合、工具合同修正和本地构建验证，
不构成 R2 收益、PD-5、RPA、设备、生产或 Stage 7 接受。

## 1. Authorization and scope

2026-10-26 用户明确授权：先备份重要文件，再将
D:/Cuexis-pd5-40k-admission 整合到 D:/Cuexis-worktree，更新指定的两份桌面接手文件，
并构建、测试。后续实施入口只有 D:/Cuexis-worktree。
旧准入树保留为只读历史来源，不继续在那里修改或构建；没有删除其 out，
没有改写旧登记、claim/seal、raw、ETL、失败窗口或既有封包。
本次未授权新的性能窗口、40k、ETW、设备运行、候选机制或推送/发行。

## 2. Verified backup and source integration

完整备份根：D:/Cuexis-worktree-evidence/integration/20261026-consolidate-9c2e7b。
verified.json、两份 source ZIP、逐文件 manifests、Git bundle、管理目录/index、
二进制 diff 与两份原始桌面文件均已保存并校验。该备份包含全部受版本控制和未跟踪的源码、
主树本地环境脚本及关键登记；被忽略的原始 out 仍在原处保留。

| Identity | Before consolidation |
| --- | --- |
| Destination | stage-7; 0157486800f8f40a4121ef0d5ab26bd041a08304 |
| Historical implementation | codex/pd5-gameplay-40k-admission; 3e5837f63f76397429c9f7603fd7450d3ce9855d |
| Common ancestor | eed36648a692333e47f798aa45be36ca850d08de |
| Unique commits | Destination 2; implementation 48 |
| Existing working changes | Destination 21 document paths; implementation 77 paths |
| Destination document checkpoint | 59b743383eebedc23044ce327ae12799079815ea |
| History merge | 1048e4ea3f17e32d20e438f359eab03627ab465a |
| Imported implementation checkpoint | 372ec94045d4d39586f24cbdd3d4ae762cd2fca2 |

历史合并保留两边提交及撤回历史。CURRENT_STATUS.md 的同路径冲突按主题整合；
其余 76 个既有未提交路径先按备份原字节导入并单独 checkpoint，再作限定的整合修正。
out/consolidation-20261026-9c2e7b/import-manifest.json 登记原始导入内容。

## 3. Documentation and tool contracts

保留主树 RPA 旧计划的归档、compatibility 和 legacy 导航、六份 RPA 报告及准入树二十九份 PD 报告，
整合索引与当前状态；不把历史通过、失败或未测量改成新结论。
补齐第三候选停止状态，清理 R2 未启动、B0 尚未完成和 ETW HOLD 等旧现行摘要。

工具修正限于：

- 旧独立 canary 准备入口明确退役，在创建目录前拒绝并指向统一封包入口。
- 三项 deadline 必须全部相等，补三种单项漂移反例。
- 尺寸审计支持明确的 --generated，多候选时不再依据修改时间猜选。
- 真实失败 F1 日志作为固定、可溯源且保留原字节的测试 fixture，避免迁移后静默 skip。
- ETW、内存解析与准入工具的离线合同测试接入 CTest。

旧 B0 尺寸 JSON/caps 保留原审计身份，不因本次脚本修正而改写历史哈希或预算。
新工具实现不认证旧审计为新源码的预算测量；后续审计需显式输入和新的身份记录。
Windows Job 单测可启动有界 benign Python 子进程；本次不运行 WPR/ETW 角色。

## 4. R2 and evidence boundaries

第三候选 direct borrowed Result 源码按原停止状态迁入，不自动回退第二候选。
原停止记录在历史准入树 out/rpa-r2-direct-row-20261025/stopped-state.json：
2026-10-25 23:03:54（Asia/Shanghai），构建被用户中断，未登记或运行第三候选性能测量。
前两候选的旧正确性通过和 inconclusive 性能结论仍属于各自身份。
本次新构建与回归即使通过，也不能代替 R2 收益门或 R4 正式接受。

历史准入树的 raw、登记、回滚包与封包继续只读保留。
已清理十四个临时目录的映射仍在
D:/Cuexis-worktree-evidence/cleanup/20261025-worktree-cleanup-7e49c2/README.md。
40k 内存第一遍 EXE 的登记原路径现在对应第二遍二进制，第一遍独立副本未确认；
保留两份原登记与 patch，不通过新构建或重跑补写旧身份。

## 5. Local verification

本次证据根为 out/consolidation-20261026-9c2e7b。
构建、测试和最终源码身份在该根记录；当前尚在进行，未宣称通过。
计划覆盖候选功能开启的 Debug/Release STATIC、RPA observation ON/OFF，
以及适用的 SHARED/消费者边界；所有新产物放在主工作树。
配置保留仓库的告警策略，不以降低告警级别或测试门槛换取通过。

## 6. Handoff and rollback

两份桌面接手材料保留原文件名作为兼容入口，内容与现场 JSON 更新到本次整合身份。
当前实施、构建与下一步命令只指向主工作树；历史准入路径仅供读证据。

原桌面文件和源码位于本报告 §2 的完整备份；本地备份分支为
codex/backup-stage7-20261026-9c2e7b 与 codex/backup-admission-20261026-9c2e7b。
回退须核对目标身份，不在合并后的现场直接反向应用旧 R0/R1/R2 局部补丁，
也不执行无范围的 reset/clean。
