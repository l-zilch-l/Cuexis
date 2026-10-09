# Version Gate复核与Stage7B+/RPA验收归属订正

状态：dated verification；文档修订，线上owner门禁未通过

证据日期：2026-10-09（Asia/Shanghai）

后续归属订正：RPA按owner最新指令归入Stage 7子阶段，唯一计划为
[plan-rpa](../../../../stage_plans/active/stage-07/plan-rpa.md)，见
[迁移记录](../handoffs/2026-10-09-rpa-stage7-substage.md)。本文保留当时的Version Gate证据；
实体设备/实时同步归RPA、S7A-9原范围和7B+能力线的划分继续有效。

## 1. 接手与范围

接手stage-7、HEAD d33c64b15575a945e50948ad70acb88f05117dce，现有PR32，
保留已有未提交架构/诊断规划文档和证据。用户要求先处理Version Gate及文档，
路线保留Stage7B+，实体设备与实时同步直接由RPA承担。

本轮不实施实时架构或重命名。版本门禁通过读取可信base与实际GitHub记录复现，
不代发owner approval、不修改可信master/保护、不新建PR或合并发行。

## 2. Version Gate失败的实际原因

可信master为3076948ce403ea0da6fbe389835b6a4c9defbcd3，当前PR仍为d33c64b。
最新owner记录6078336026于2026-10-09 09:37:00 UTC创建，未编辑，使用LF：
批准candidate 1afe382a91198a22090619529c958d499f759103、tree42508cc6fce580ff823ac0ad4d8b657932525d47。
当前候选SHA和tree已改变。由可信base提取checker并使用真实GitHub API执行，
稳定复现`version.sdk_api.approval_mismatch: approval does not bind this change`。
它与当前run37924467045的原始失败一致；不是CRLF问题，也不是可信基线缺失。

现行VERSIONING要求准确绑定repository/PR/base/candidate/tree/from/to/UTC date，
最新无效审批不可回退旧审批。本轮保留该规则：最终文档提交与推送完成后，
从最终Git对象生成LF审批草稿；由owner本人新发未编辑评论，再重跑同候选pre-merge。
评论事件不会自动触发当前workflow；不改CI触发结构来掩盖缺失的实际授权。

草稿是待签文本，不是授权证据。新SHA只有真实owner记录通过可信checker后才可写作Version Gate成功。
任何之后的提交（含文档）或UTC日期变化均需重新核对tuple和版本；无需重做已通过且输入未变的C++矩阵。

## 3. 路线与验收归属

| 工作线 | 范围及出口 |
| --- | --- |
| Stage7A | 最小kernel/Fold、typed输入与公共消费者、Replay/Seek、原预算/跨平台、7/8版本门禁及S7A-9原范围验收 |
| Stage7B+ | Slide/Flick、方向/轨迹/多指、S7C高级校准等，独立能力准入，可跨Stage8继续演进 |
| Stage RPA | 实时架构、正式诊断命名，以及实体DFJK/focus/control、真实听音、设备恢复、卡顿/刷新率/输入延迟和同步的实施及最终验收 |
| Stage8 | 消费7A与RPA已验收成果和已选入7B+能力；不等待全部7B+ |

RPA可以基于7/8现有集成先设计；S7A-9不再新增等待RPA完成的前置。
设备旧失败保留为RPA待办证据，不改记通过，不由S7A-9代验。
需要实时宿主边界的具体7B+能力消费RPA对应合同，其余研究按自身依赖推进。
本轮不开始RPA、7B+/S7C实施，不接受生产阈值或阶段关闭。

## 4. 检查与证据

[原始证据ZIP](2026-10-09-version-gate-and-rpa-routing-evidence.zip)包含真实owner评论API快照、
可信base文件及哈希、执行命令/退出码、真实API拒绝输出、版本与文档检查结果。
候选工具Windows 26 tests通过（2个平台shell项跳过）。可信base旧测试在Windows运行19项中
1个bootstrap shell夹具失败，提取的shell块出现空变量；具体Windows环境差异未进一步诊断。原始输出保留，
不把该Windows结果冒充与Linux hosted相同环境；同一可信base工具在WSL/Linux重验19/19通过。

推送前运行update_version.py；SDK保持0.7.1，日期身份按可信master同日build1→候选build2。
若版本输入未变化，不重复宣称fresh/clean-first构建；最终SHA与审批草稿通过桌面接手绑定。

文档与只读版本检查：401 Markdown/20 candidate JSON通过，status4/4、section5/5、target2/2，
check-current与updater --check通过，diff检查无错误。update_version.py执行后版本输入没有差异，
本轮未运行C++构建/设备验收，不以此前构建或CI成功证明RPA新行为。
