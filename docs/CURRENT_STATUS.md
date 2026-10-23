# Cuexis Current Status

状态：current

更新日期：2026-10-23

本文只维护当前阶段、正在进行的任务和未解决的问题。实施与测量细节见带日期的阶段报告。

## 当前任务

**最新执行状态（2026-10-23）**：[只读管理员预检与下一证据方案](stage_reports/stages/stage-07/pd/2026-10-23-readonly-etw-preflight-and-next-evidence-plan.md)已形成：14 项工具单元验证通过，新增 Job 超时/取消同步回收及实际 active=0 检查；实际 ETW 采集仍因看护/停止竞态保持 HOLD。v3 capture 被禁用；管理员只读预检已执行到 ETW 枚举，因已有 WPR 会话停止，名称/路径未记录。已另备只读 inventory 待管理员运行，详见[最新拦截报告](stage_reports/stages/stage-07/pd/2026-10-23-etw-existing-session-preflight-stop.md)；没有新 F1 样本、40k 重跑或正式注册。三个无上下文 Luna 分别完成一个低把握 ordered visitor 设计、内存分域采集方案和工具复审；候选未实施/登记，预算未改。主代理复算 31 项来源并保留一项原 binding 的手抄 SHA 错误。原 40k 功能及小规模最终释放证据保留，40k 分域内存与正式读取性能仍未通过；主树 F1/C2、历史证据不变。旧窗口3、公开停止接口、merge/push 继续停止。

**Stage 7-PD / PD-0 基线、PD-1/PD-2、PD-3-a/b/c 与 PD-4 内部实施及适用本地门禁/独立复审完成；SDK 0.9.0 已获接受。PD-5 原 40k R4 功能通过，当前增加小规模作用域最终释放证据和 F1/C1/C2 原数据复算；40k 完整内存与正常分配器正式性能仍未通过。F1 有限 read 对照仍超线，C1 撤回、C2 未达自检；未登记下一性能候选，先准备有界 F1-only 噪声采集。主树保持 F1，旧关闭风险未归因，停止接口暂停；Stage 7-PD、RPA、生产与整个 Stage 7 均未关闭。**

以下性能结论属于冻结的 PD-0 实施输入，不代表 PD-4 当前源码的正式性能验收。该固定环境与干净重建后的完整注册组满足原定 15% 门槛，owner 最大进程级波动 3.89%，宿主均值最大波动 11.11%。测量前修复并验证了 MSVC/Ninja 编码不一致造成的头文件依赖丢失；历史慢态根因仍未确定，旧失败组和源码绑定失败组均保留。该结论限于已登记的有限 testOnly profile，不接受生产预算或关闭 Stage 7。

方案记录：[PD-0 测量波动方案实施](stage_reports/stages/stage-07/pd/2026-10-17-pd0-measurement-variance-resolution.md)。
最新证据：[受控环境、构建绑定与 PD-0 退出](stage_reports/stages/stage-07/pd/2026-10-18-pd0-controlled-environment-and-build-binding.md)。
PD-1 证据：[Judgement 发布桥、读取失效与包边界](stage_reports/stages/stage-07/pd/2026-10-18-pd1-publication-bridge.md)。
PD-2 证据：[prepare 绑定索引、候选投影与等价回归](stage_reports/stages/stage-07/pd/2026-10-18-pd2-projection-adapter.md)。
PD-3-a 证据：[Runtime/World 事务、派生状态与安装后失败](stage_reports/stages/stage-07/pd/2026-10-18-pd3a-software-transaction.md)。
PD-3-b 证据：[提交修订、中性读取与唯一 Snapshot 物化](stage_reports/stages/stage-07/pd/2026-10-19-pd3b-state-read-and-materialization.md)。
PD-3-c 实施与版本决定：[宿主细化与公共来源查询/版本决策](stage_reports/stages/stage-07/pd/2026-10-19-pd3c-contract-and-version-decision.md)。
PD-4 的 SDK 0.9.0、消费注册/交付、真实 CPU/attempt 回收、有界观察与 NoRetainedFrame 内部实施及适用本地门禁完成；见[实施与验证](stage_reports/stages/stage-07/pd/2026-10-19-pd4-delivery-retirement-and-observation.md)。[三项 owner 决策](stage_reports/stages/stage-07/pd/2026-10-19-pd4-readiness-and-owner-decisions.md)已接受；不代表物理设备、正式性能或发行接受。
设备观察：PD-4 Release 独立音乐 1 轮、DFJK 2 轮均退出 0；DFJK 首轮实际执行完整 terminal horizon，第二轮 Pause/Reset/Resume 正常。owner 报告控制附近 FPS 约 240→130 后恢复 240，待量化；音乐听感正常，underruns=2 经 owner 说明发生于切换 Codex 打字，不据此判定播放故障。见[PD-4 实体观察](stage_reports/stages/stage-07/rpa/device/2026-10-19-pd4-physical-music-and-keyboard-observations.md)。[旧版 DFJK 约 17.6 秒 capacity_exceeded/退出 1](stage_reports/stages/stage-07/rpa/device/2026-10-19-pd3c-physical-music-and-keyboard-observations.md)保持失败历史；设备失效/恢复与正式预算门禁仍未完成。
PD-5 准备：owner 已接受[本机范围、全部阈值和三窗口协议](stage_plans/active/stage-07/pd5-measurement-proposal.md)；[首批工具/profile](stage_reports/stages/stage-07/pd/2026-10-19-pd5-tools-and-profile-preparation.md)具备显式端点 collector、传递构建绑定、CPU/GPU 短流式与完整控制脚本验证。[窗口协议与矩阵扩展](stage_reports/stages/stage-07/pd/2026-10-19-pd5-window-and-matrix-preparation.md)已验证 P0 不可变 runner、七种独立 SDK/delivery 短轨迹与内部物化计数，完成稳定音频端点盘点和 Packed 规模工具回绑；[初始就绪检查](stage_reports/stages/stage-07/pd/2026-10-19-pd5-measurement-readiness.md)保持原时点证据。P0 已完成一次 12 进程 pilot 和两个各 36 进程的正式窗口；第三窗按 owner 要求暂停，后续转入核查与修复后重新测量。组合 renderer/captures、observer、完整内存、真实音频/输入及正常分配器规模 Seek 延迟仍待补齐，无完整正式接受或生产性能通过结论。

PD-5 Python 测量工具及 profile 已整理到 [tools/verification/playback_performance](../tools/verification/playback_performance/README.md)，测试和 C++ 探针留在 `tests/pd/`；非测试源码、工具与配置已按功能命名并更新引用，见[命名修正与验证](stage_reports/stages/stage-07/pd/2026-10-20-performance-tool-functional-naming.md)。测试目录的旧 Python 入口保留兼容；历史证据不改写，后续运行需重新 prepare。

[注册完整性与容量短测修正](stage_reports/stages/stage-07/pd/2026-10-20-measurement-preparation-integrity.md)补齐注册外层哈希、跨窗口/pilot 原始文件独立性及完整控制次数拒绝检查，64 实体 low/high 的 Snapshot/Seek/Replay 端到端工具短测通过；40k 功能现已独立核验；正常分配器性能、真实 Player/设备和完整测量就绪仍待验证。

PD-5 40k 准入：[阶段 A](stage_reports/stages/stage-07/pd/2026-10-21-gameplay-admission-stage-a.md)、[B0](stage_reports/stages/stage-07/pd/2026-10-22-gameplay-admission-b0-budget-ruling.md)、[阶段 B](stage_reports/stages/stage-07/pd/2026-10-22-gameplay-admission-stage-b.md)、[R4 entries](stage_reports/stages/stage-07/pd/2026-10-22-gameplay-admission-revision4-entries.md)和[metadata 顺序修复](stage_reports/stages/stage-07/pd/2026-10-22-gameplay-admission-r4-metadata-order.md)保持。历史 advance、Snapshot budget、protected-input 和 CLI discovery 失败各自保留。最新原 40k low/high 的功能与 wire 身份已独立核验，Snapshot 均 90,823,328 bytes；[本批报告](stage_reports/stages/stage-07/pd/2026-10-23-stage-c-independent-acceptance-and-exit-gaps.md)明确全进程内存包络超限、after-close 仍持有测试 owning 值、各域内存/最终释放及正常分配器 Seek 延迟未通过。七项 Packed 物理预算、四项 hash ceilings、deadline 和 1.10/15% 不变；这不是完整 PD-5 接受。

PD-5 执行阻塞跟进：[受控诊断、内核修复与关闭合同交回](stage_reports/stages/stage-07/pd/2026-10-23-execution-core-diagnostic-and-repair.md)已完成低/中规模内部计时、保语义内核修复、1/64 × low/high 全链路 plumbing 和九组 Release 回归。worker 关闭候选因 Windows DLL 上下文传递及禁止私有导出合同已撤回；[公开操作级停止合同与接口方案](proposals/implementation-input/stage-07/pd5-operation-stop-contract.md)现已暂停，保持未接受、未实施候选，不再作为现有合同内最小修复的必需前置。[现有生命周期最小修复](stage_reports/stages/stage-07/pd/2026-10-23-minimal-lifecycle-repair.md)已随 57b604d 完成 factory 空 delivery 防护、Player 回调依赖析构顺序修正及有界原 Tail job 的 Close 验证；其后固定 54e8c2a 的原 40k low/high 已实际完成显式 close/reset 和进程退出。旧 40k 关闭阻塞仍未归因，本次成功只证明固定功能 profile，不构成一般生命周期或性能接受。

PD-5 本轮补充：[Gameplay-host 字典/预算核查](stage_reports/stages/stage-07/pd/2026-10-21-gameplay-host-string-budget.md)及[C2 单候选交接](stage_reports/stages/stage-07/pd/2026-10-21-c2-read-result-candidate.md)。C2 完整 Release CTest 1272 通过、1 跳过、0 失败，12 个固定被测进程完成；主指标及规模重复性未达冻结条件，未合入主树，交回核验后再决定处置；未新建正式 preparation/registration，旧窗口 3 继续停止。

PD-5 前轮证据：[两窗报告（2026-10-21 更正）](stage_reports/stages/stage-07/pd/2026-10-20-pd5-two-window-findings-and-diagnostic-handoff.md)及[有限核查与交接](stage_reports/stages/stage-07/pd/2026-10-21-pd5-followup.md)。修复前正式 RPA read_long 两窗超界；前轮 F1/A 的 1/8/64/256 对象长块 read 比值为 1.2368/1.3441/1.3742/1.3967，各诊断区间下界 >1.10。真实 update→ready 另列，不能代替孤立读取；公共 request→ready 的 A 进程波动 49.55%，不可判定。40k Gameplay-host 两种 reuse 均在 Packed 字符串预算处失败，未进入 Snapshot/Seek/Replay；独立原 Packed codec 40k 通过不替代它。唯一 C1 候选按事前标准放弃并撤回，有限尝试不证明成本下限。旧宿主 ACK/present-return 仍分项，enqueue/drain/GPU 未判。五批材料交回核验；旧窗口 3 保持停止，原 1.10/15% 与全部生命周期门禁保留，未作性能偏离接受或正式验收。

## 阶段状态

| 阶段 | 当前状态 | 入口 |
| --- | --- | --- |
| Stage 7A | S7A-3/4、5/6、7/8 的受限功能验收完成；整体关闭仍待 RPA 收尾 | [Stage 7A 计划](stage_plans/active/stage-07/plan-a.md) |
| Stage 7-RPA | testOnly 实施与自动化证据已具备；实体设备、生产预算和最终接受未完成 | [RPA 计划](stage_plans/active/stage-07/plan-rpa.md)、[交接状态](stage_reports/stages/stage-07/rpa/2026-10-15-rpa-f4-handoff.md) |
| Stage 7-PD | PD-0–4 内部实施及适用本地门禁完成；PD-5 原 40k 功能与小规模最终作用域释放已核验，40k 分域内存/正式性能/拆分触发审查未完。新增工具 Job 回收验证、一个未实施读取设计和内存方案；v3 仅交付管理员只读预检，采集仍 HOLD。F1 read 超线，C1 撤回、C2 不合入，主树保持 F1，子阶段未退出 | [PD 计划](stage_plans/active/stage-07/plan-presentation-decoupling.md)、[最新证据与未决项](stage_reports/stages/stage-07/pd/2026-10-23-readonly-etw-preflight-and-next-evidence-plan.md) |
| Stage 7B+ | 未开始，等待 RPA 完整退出 | [Stage 7B+ 计划](stage_plans/active/stage-07/plan-b.md) |

Stage 0–6 已完成；Stage 8–14 尚未开始。整体 Stage 7 未关闭。
阶段依赖与后续安排见 [Stage 7 总计划](stage_plans/active/stage-07/plan.md)和[路线图](ROADMAP.md)。

## 当前问题与关闭前置


- **历史慢态及尾延迟未归因**：PD-0 最新登记组重复性通过，但不解释历史近两倍慢态；该组 Debug 宿主仍保留 15.93 ms 的单帧请求到回执尖峰。PD-5 本机有限 Release 门槛已接受，生产尾延迟预算未接受，不能据均值重复性通过推断生产延迟合格。
- **控制附近帧率波动待量化**：PD-4 实体 DFJK 完整课程与控制观察已完成，旧版容量错误本轮未复现；Reset/暂停恢复时约 240→130→240，持续时间与模块成本未测量。设备失效/恢复仍未验收。
- **局部偶发故障未定位**：B5 Pause 的 `capture_gap` 与 CXC unpack 的 `Access Denied` 尚无根因结论。
- **40k 功能已核验、内存和正式性能仍未通过**：原 low/high 不重跑、原超包络记录不改。新 64/2048 low/high 诊断中，释放 decision/result/Snapshot/Replay/content/fixture 并结束全部局部作用域后，tracked live 三项精确回到入口；这不替代 40k 分域预算/最终释放。F1/C1/C2 复算保留原回归与噪声结论；ETW 首次包装失败且无 F1 样本，修正版清理复审 HOLD。见[寿命诊断](stage_reports/stages/stage-07/pd/2026-10-23-memory-lifetime-and-read-noise-diagnostics.md)与[最新失败/修复状态](stage_reports/stages/stage-07/pd/2026-10-23-etw-wrapper-failure-and-repair.md)。
- **RPA 验收未完成**：已取得 PD-4 受限实体键盘与听音观察；精确音画同步、设备失效与压力验证、生产参数及预算接受仍待完成。
- **预算证据缺项**：端到端 Seek 延迟、prepare-only 峰值堆和 decoded semantic heap 尚未完成有效测量与接受。

问题证据与剩余门禁见 [PD-0 复测归档](stage_reports/stages/stage-07/pd/2026-10-17-pd0-supplemental-retests-and-archive.md)、
[RPA 矩阵](stage_reports/stages/stage-07/rpa/2026-10-15-f1-f2-final-matrix.md)及
[预算基线](stage_reports/stages/stage-07/rpa/2026-10-15-f3-final-baseline.md)。

## 产品边界

Cuexis 由可嵌入的 Playback SDK、独立 Cuexis Player 和 Cuexis Studio 构成。
外部宿主使用公共 Playback API；RuntimeSession、World、EnTT、SDL 和 OpenGL 属于内部或可选实现。

产品决策见 [ADR 0027](adr/0027-playback-sdk-product-boundary.md)，公共接口见 [API reference](api/README.md)。

## 当前格式与 SDK 合同

- 当前工作树 SDK API 为 **`0.9.0` preview**（PD-4 内部实施与适用本地门禁完成），日期构建为 **`26.10.19-1`**；这不代表已经发行。
- Gameplay v2 是当前实施基线，顶层合同仍为 candidate；局部实现不等于整批验收。语义与运行边界见
  [Gameplay V2 Spec](formats/GAMEPLAY_V2_SPEC.md)、[ABI](api/GAMEPLAY_V2_ABI.md)及[RPA 补充合同](formats/rpa-execution-contract.md)。
- Chart v1–v4 的既有兼容路径保留；Chart v5、CXT v2、Packed Chart 与 Canonical Gameplay Graph 仍为显式候选路径，正式发行收敛归 Stage 8。
- 稳定 C ABI 归 [Stage 14](stage_plans/future/stage-14/plan.md)，当前只提供 C++ typed preview。

各格式的生产/候选状态与权威文档见[格式索引](formats/README.md)。

## 延期和禁止边界

运行时脚本和逐帧 script callback 无限期延后，不预留字段、capability、bytecode、ABI 或 Playback hook。
离线 authoring generator 不会被 pack、prepare 或 Playback 隐式执行。

## 更新规则

只更新当前结论，完成或被替代的状态应就地更新或删除。
测试轮次、命令、提交经过、授权沟通和历史问题处置保存在阶段报告中，不向本文追加过程日志。
