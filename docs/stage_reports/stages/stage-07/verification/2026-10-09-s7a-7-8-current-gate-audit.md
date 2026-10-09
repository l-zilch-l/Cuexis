# S7A-7/8余项与当前提交门禁核对

状态：dated verification；只读核对与证据回填，未关闭批次

证据日期：2026-10-09（Asia/Shanghai）

## 基线与核对范围

本地与PR #32均为 `stage-7`，HEAD为
`c4b1f68db0d809b9a9c0c27b37d53538423417ff`，PR仍OPEN；base为
`3076948ce403ea0da6fbe389835b6a4c9defbcd3`，candidate tree为
`51c31380d02f79f1ceb976751d1367841a96e4f2`。
现有未提交审批助手与Stage 7-RPA文档保留，不属于当前线上SHA。

本轮复核plan-a、10-08逐行C78/R78/九目标与平台证据、后续设备/阶段迁移报告，并读取真实
GitHub PR/checks、workflow runs、版本成功日志、审批评论、master保护与rulesets。
本轮没有重新构建、执行产品测试、修改保护、重跑CI、发布审批、提交推送或接受阶段退出。

## 九项目标

| 项目 | 当前证据口径 | 7/8原范围余项 |
| --- | --- | --- |
| 7.1 Playback | 显式生命周期、实际submit/advance/query、Replay/Snapshot/Seek/control与组合事务已有受限验收 | 最终证据归档；实时协调改造归7-RPA |
| 7.2 内容入口 | typed source、Graph/Packed、Entry/CXC/filesystem到实际prepare已有受限验收 | 最终证据归档；正式格式发行归Stage 8 |
| 7.3 Headless | 同实际kernel/Fold、完整Replay及golden/独立oracle/故障矩阵已有受限验收 | 最终证据归档；新增实时oracle归7-RPA |
| 7.4 Player与反馈 | 实际SDL接线、Score/Combo/Miss、FactBinding及定向窗口golden已有受限验收 | 保留设备失败的移交证据，实体设备/同步在7-RPA验收 |
| 7.5 安装消费者 | static/shared、公开头/ASCII/泄漏/版本拒绝/candidate隔离已有受限验收 | 最终证据归档；后续API变更另做受影响矩阵 |
| 8.1 candidate隔离 | ON/OFF、wrong entry/flavor、hosted实际candidate消费已有证据 | 最终证据归档 |
| 8.2 assembler | 生产入口、完整闭包、确定性与原子发布已有证据 | 最终证据归档；生产预算接受归S7A-9 |
| 8.3 Reference Host | installed六动词加tick、失败保留旧active与完整Replay已有证据 | 最终证据归档；新增实时协议归7-RPA |
| 8.4 版本与治理 | patch差异证明、fresh/clean安装矩阵、可信bootstrap、当前SHA真实审批与pre-merge成功已有证据 | 可信检查来源保护仍未闭合；最终交付SHA证据与退出裁定仍需完成 |

八项达到既有受限实现/验收口径；这不是Stage 7A关闭，也不是生产预算或真实设备通过。
完整fixture、合同revision、实现SHA和原始构建记录继续归
[10-08联合报告](../implementation/2026-10-08-s7a-7-8-entry-assembler-progress.md)。
实体设备、真实听音、恢复、卡顿与同步、架构/诊断命名已由
[Stage 7-RPA分册](../../../../stage_plans/active/stage-07/plan-rpa.md)承接。

## 已满足的当前SHA门禁

| 证据 | 实际结果 |
| --- | --- |
| Linux Quality，PR run `37945013506` | completed/success，HEAD为c4b1f68 |
| Windows MSVC，PR run `37945013528` | completed/success，同HEAD |
| Windows MinGW，PR run `37945013543` | completed/success，同HEAD |
| Version Gate，PR run `37945013516`，attempt 2 | completed/success；2026-10-09 23:18:43 +08结束 |
| 审批评论 `6083720194` | API正文绑定base/candidate/tree、SDK 0.7.0→0.7.1、UTC 2026-10-09；created_at=updated_at，未编辑 |
| 同HEAD push事件 | MSVC/MinGW成功；Linux `37945004443`仍in_progress，media sanitizer及shader coverage两个job仍运行，不冒充全事件均完成 |

成功job原文：

```text
version.gate.pass: event=pull_request(recorded) context=live base=3076948ce403ea0da6fbe389835b6a4c9defbcd3(26.10.09-1) candidate=c4b1f68db0d809b9a9c0c27b37d53538423417ff(26.10.09-2) trusted_utc_date=2026-10-09 sdk_owner_approval_comment_id=6083720194
```

此前报告中的bootstrap缺失、当前审批不匹配及三平台尚未回填，是各自旧SHA/旧时点状态，
不能继续作为c4b1f68的当前缺项。后续推送或UTC日期变化仍按VERSIONING重新核对。

## 尚未闭合的保护与交付

读取master保护得到 `strict=true`、`enforce_admins.enabled=true`，required check为
`Version advancement (pre-merge)`，绑定 `app_id=15368`；force push与删除关闭。
读取 `rulesets?includes_parents=true` 得到空数组。

这证明required context及GitHub Actions App绑定已启用，不能证明检查来源被强制固定到可信
workflow；[VERSIONING](../../../../guides/VERSIONING.md)已有此明确边界。因此保护来源门禁仍未退出，
本次不修改保护或默示接受例外。需落实可审计的可信来源约束，或由owner明确接受并记录相应例外。

I78-5还需将最终交付版本的逐行证据与该门禁处置收敛成可审查的退出记录。
本轮回填解决当前c4b1f68的证据滞后；本地待交付文档/工具未推送，后续新SHA不能沿用本次审批。
版本输入若改变，仍须updater及fresh/clean-first/consumer；本轮没有公共API变更，不要求重做
无变化的产品矩阵来代替治理门禁。S7A-9最终预算/关闭与7-RPA实施不在本轮。

## 命令与原始输出

原始输出保存在 `out/s7a78-current-audit-20261009/`：`pr.json`、`runs.json`、
`version-log.log`、`approval.json`、`protection.json`、`rulesets.json`、`master-workflow.json`。
所有读取命令成功退出；查询使用本机7890代理，不修改GitHub状态。

```text
gh pr view 32 --repo l-zilch-l/Cuexis --json number,state,headRefName,headRefOid,baseRefName,baseRefOid,mergeStateStatus,reviewDecision,statusCheckRollup,url
gh api --method GET repos/l-zilch-l/Cuexis/branches/master/protection
gh api --method GET "repos/l-zilch-l/Cuexis/rulesets?includes_parents=true"
gh api --method GET repos/l-zilch-l/Cuexis/issues/comments/6083720194
gh api --method GET "repos/l-zilch-l/Cuexis/actions/runs?head_sha=c4b1f68db0d809b9a9c0c27b37d53538423417ff&per_page=100"
gh run view 37945013516 --repo l-zilch-l/Cuexis --log
gh api --method GET "repos/l-zilch-l/Cuexis/contents/.github/workflows/version-gate.yml?ref=3076948ce403ea0da6fbe389835b6a4c9defbcd3"
```
