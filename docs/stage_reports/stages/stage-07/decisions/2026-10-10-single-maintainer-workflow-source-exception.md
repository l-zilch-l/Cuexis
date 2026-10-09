# 单人维护仓库的workflow来源例外接受

状态：accepted decision record；治理修订，非阶段关闭

证据日期：2026-10-10（Asia/Shanghai）

## 明确决定

owner在解释来源风险后指示：“不过只有我会维护这个仓库，实在不行就改到不强制指定可信
workflow来源吧”。据此修订VERSIONING及plan-a，取代10-09“保留来源门禁未退出”的安排。
不强制来源是明确风险接受，不声称平台支持或App绑定实现了workflow身份锁定。

## 范围与补偿核对

合同归[VERSIONING](../../../../guides/VERSIONING.md#单人维护仓库的检查来源例外2026-10-10)，
执行卡归[plan-a §3.4.21](../../../../stage_plans/active/stage-07/plan-a.md#3421-单人维护来源要求修订2026-10-10)。
只针对当前个人单人仓库和普通PR。新增维护者、改变检查服务或仓库归属、启用queue时重审。
不安装新服务、不迁移仓库、不关闭真实版本校验、不增加bypass、不代合并/发行。

保留master PR保护、strict检查/App15368、禁止强推/删除，无绕过者。
门禁文件差异与实际执行日志核对代替平台来源锁定作为本范围的接受条件；
真实owner评论仍须准确绑定最终SHA/tree/base/SDK/UTC日期。
人工/代理日志核对不能提供不可伪造保证，同名检查被替换是已知接受风险。

## 实际基线与证据

接手branch为stage-7，HEAD `68845e785204ea8a6ea35afe2b5d247d81dc5db2`，工作区clean，
PR #32仍OPEN；base为 `3076948ce403ea0da6fbe389835b6a4c9defbcd3`。
本轮API确认collaborators仅 `l-zilch-l`，有admin/write权限；ruleset24799746仍active，
bypass为空，`current_user_can_bypass=never`。未修改远端保护配置。

上述HEAD的Version Gate run37955265421已success；其余平台CI仍有任务运行，未提前标成功。
这些结果只证明68845e7，不替代本次文档修订提交的审批/hosted。
人工确认本轮未修改任何workflow/checker/tests/updater/owners/CODEOWNERS或版本/公共API；
推送前使用updater按真实UTC再次核对，SDK维持0.7.1。
用户日期为10-10，本轮查询时真实UTC仍为10-09，构建号按UTC保留26.10.09-2，不能按本地午夜误升日期。

原始GET输出保存在 `out/s7a84-single-owner-exception/` 的pr.json、ruleset.json、collaborators.json；
验证输出、最终交付SHA及后续审批/CI在同目录和桌面接手绑定。
本轮仅合同/规划及dated证据修订，运行docs/status/section/target/version一致性与diff检查。
未重新构建C++或验收设备；产品输入未变。S7A-9、7-RPA和Stage 7A关闭未执行。

来源例外已接受，S7A-8.4不再因平台来源配置单项受阻；完整退出仍需最终交付证据，
该例外不构成合并、发行或整个Stage 7A关闭许可。
