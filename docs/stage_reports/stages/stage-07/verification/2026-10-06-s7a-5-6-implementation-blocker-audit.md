# S7A-5 / S7A-6 Implementation Blocker Audit

状态：dated planning audit；未实施产品代码

证据日期：2026-10-06

审阅基线：`c9ae4b27fdd88844935ddeed49af33621ac4d5e2`；工作区包含此前已完成的5/6规划。
现行目标与决策归 [主计划§3.2](../../../../stage_plans/active/stage-07/plan.md#32-s7a-5--s7a-6-联合实施目标决策和准入)，
备选与选优归 [实施输入§13–14](../../../../proposals/implementation-input/stage-07/s7a-5-6-design-selection.md)。

## 复核结论

仍有消费前阻塞：已有U01–U10尚未落入完整字段合同；本轮另确认U11–U13三个行为合同缺口。
十三项均已给出选定推荐，状态仍为待合同落定，不能写成无阻塞或功能已完成。
实施方可按既定5+6目标推进J0，在各首次消费点前补齐Spec/ABI/profile与独立golden。
本次没有发现必须先完成7–9、重新讨论T4/K4或等待新增owner授权才能继续J0的依赖。

| 新项 | 最小反例 / 缺口 | 选定处置 | 首次消费 |
| --- | --- | --- | --- |
| U11 | 合法录制以Ruleset fault结束，不能同时叫“文件无效”和“全部成功才接受”；请求horizon不等于执行成功前缀 | ReplayEvaluation分开验证结果与terminal状态；Seek重执行目标控制，健康候选才替换旧active | 6.2/6.4/6.5 |
| U12 | 原session销毁后只有identity，无法重建prepared graph/config/Ruleset；重算摘要不能证明checkpoint属于录制历史 | owning RecoveryInputs绑定并验证；外部Seek checkpoint与所属archive/cut从起点完整对照 | 6.1/6.3 |
| U13 | nextTick输入究竟在due Hook安装前后执行；kernel已消费Hook、Fold失败回滚队列会导致恢复重复消费 | Tick起点kernel候选安装；seal保留消费；Fold只发布未来输出，读集/版本显式登记 | 5.4/6.3 |

U11的测试例：窗口3，F(H)=H−3，advance(20)在Tick8阶段2失败。
目标horizon10只处理到frontier7；目标11包含Tick8并重现失败。不可比较horizon与failedTick就判可达。
这只是验证反例，不接受生产窗口或预算数值。

## 审阅依据

- [Spec](../../../../formats/GAMEPLAY_V2_SPEC.md) §3.14/3.15/3.18与§10：seal不回滚、三类Register、恢复闭包、同源Replay执行。
- [ABI](../../../../api/GAMEPLAY_V2_ABI.md) 域9：immutable graph与FactBinding经identity验证后重新取得；尚缺依赖提供接口。
- [execution kernel](../../../../../engine/judgement/src/execution_kernel.cpp) `ExecutionKernel::advance`：稀疏工作Tick、seal后的faultReserve、requestedHorizon/failedTick；现有signalVisibility是测试marker，不能替代真实Hook消费实现。
- [session](../../../../../engine/judgement/src/session.cpp)：typed advance失败返回错误而query可访问保留投影；Snapshot/Seek尚未有对应实现。
- [kernel types](../../../../../engine/judgement/include/cuexis/judgement/kernel_types.hpp)：Fact含PhaseOutcome与Receipt，stray/consumeEmpty不需要新增另一套统计事件来源。
- [input normalization](../../../../../engine/judgement/src/input.cpp)：`canonicalInteger = roundHalfToEven(quantity / scale)`；修正实施输入与桌面反例：scale2下raw2得到1，再把1当raw得到0，不是2。

## 已有台账和不构成前置阻塞的事项

U01–U05覆盖有限操作/profile接受集、grade、算术、稀疏Tick、Hook目标/载荷；
U06–U10覆盖canonical入口、live分支、count/frontier、codec/诊断、fault schema/恢复权限。
本次追加不替代这些项；每项的最早消费与退出证据仍查主计划。

Snapshot结构解码、prepared绑定验证、与Replay历史执行对照是不同验证层次。
独立Snapshot restore不得被报告成已通过Replay历史对照；未经对照的外部checkpoint不能直接用作Seek加速起点。
该规则不引入签名、安全产品或外部IO依赖。

SDK0.7.1、D-9/master与owner发行放行归8；生产预算与maxSeekLatency数值归9。
它们不阻止不消费这些承诺的5/6功能实现，也不因本轮计划修订而视作已接受。
Life、外部Ruleset package、correction与runtime script仍按现有拒绝/排除合同执行。

## 交付与验证界限

决策写回主计划，五方案追加到实施输入§14，当前状态摘要及桌面接手同步。
active/stage-07保持plan.md和legacy-paths.md两个文件。
本次仅修改文档，执行文档链接/状态/目标合同、版本只读检查与diff空白检查；
没有构建、运行C++测试或新增5/6实现证据。检查结果以本次交付回复为准。
