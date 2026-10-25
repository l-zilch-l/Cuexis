# Cuexis Current Status

状态：current

更新日期：2026-10-26

本文只维护当前阶段、正在进行的任务和未解决的问题。实施与测量细节见带日期的阶段报告。

## 当前任务

**2026-10-26：两树整合后的唯一实施入口为 `D:/Cuexis-worktree`；`D:/Cuexis-pd5-40k-admission` 保留为只读历史证据树。R2 第三候选 direct borrowed Result 的源码已原样迁入；该候选于 2026-10-25 23:03:54 +08:00 停止，本次整合构建与正确性验证进行中，尚无第三候选性能结论。见[整合记录](stage_reports/stages/stage-07/rpa/2026-10-26-worktree-consolidation.md)。本次仅授权本地整合、构建测试及桌面接手更新，不授权新性能诊断、ETW、40k、pilot、正式窗口或新候选；R2/R4 未退出。**

**2026-10-25：R2 继续实施单行 typed visitor 第二候选；主代理亲自核对增量、实测 160 B 显式逻辑存储并运行聚焦正确性对照，五构建 47 项检查及 9 项消费者门通过；固定局部诊断四规模均 inconclusive 且有噪声限定，收益未确认。见 [R2 单行续做](stage_reports/stages/stage-07/rpa/2026-10-25-r2-single-row-continuation.md)。首版失败及其窗口保留，R2/R4 未退出。**

**2026-10-25：Stage 7-RPA R2 首版借用行候选已实现，五构建正确性回归、适用 staged consumers 和九路径精确回滚验证通过；局部诊断显示单对象退化约 25.3%、N64 收益区间跨 1，当前候选未接受、R2 收益门仍未完成。结果与范围见 [R2 实施记录](stage_reports/stages/stage-07/rpa/2026-10-25-r2-borrowed-presentation.md)。R4 正式接受及 RPA/PD/Stage 7 关闭仍独立。**

**2026-10-25：Stage 7-RPA R1 已退出。依据用户本次明确授权，完成首错修复后最终构建三项有限 headless pilot（3/3 首次 exit=0、Close + join 659 us）及退出门核验；当前五构建回归、适用消费者与独立补丁/回滚证据见[本地记录 §11](stage_reports/stages/stage-07/rpa/2026-10-25-r1-continuation.md#11-当前修复构建有限-pilot-与-r1-退出)。五条既有 PD 格式失败作为范围外限定保留，noIndependentContract 不算容量通过，R0 残余保持归属。RPA/PD/设备/生产接受及整个 Stage 7 未因此关闭，R2 当前进度见上条；R3/R4 未获授权，40k/ETW/正式窗口仍不在授权范围。PD-5 当前状态见下段，历史报告中的树身份与授权时点保留。**

**ETW 工具当前状态**：[F1 工具包验收记录](stage_reports/stages/stage-07/pd/2026-10-23-f1-tool-package-and-acceptance-preparation.md)保留 r2 解析失败、r3 死亡证明失败与 r4 一次管理员运行三项全过的各自身份；r2/r3/r4 均为已运行包，不重跑、不改名。最小生命周期验收与 r4 全绿只证明工具合同，不认证产品性能或内存；Power 原 128 MiB 阈值中断仍为失败。正式性能测量仍为无 ETW。**

**Stage 7-PD / PD-0–4 内部实施、适用本地门禁和独立复审完成，SDK 0.9.0 已获接受。PD-5 原 40k R4 功能通过，完整内存仍未测量（原包络超限改记为观察器量程产物），正常分配器正式性能未通过；F1 read 超线、C1 撤回、C2 未达自检，R0-a/b 未产生可提名候选，读取项按原判据记为已量化超界、不视为通过。冻结 PD 性能基线仍为 F1；当前整合树另含未接受的 RPA R2 第三候选，不套用旧性能身份。B0 静态内存账本已获接受，B1 域归因工具通过静态门但未采数，B2/B3 未授权。[PD-5 判定报告](stage_reports/stages/stage-07/pd/2026-10-24-pd5-verdict-and-stage-7-pd-exit.md)提出「未通过但定案」及 Stage 7-PD 退出清单，二者尚待 owner 批准；Stage 7-PD、RPA、生产与整个 Stage 7 均未关闭。读取与 40k Seek 优化由 RPA 性能整改线承接，原阈值及生命周期门不改，旧窗口 3 停止、公开停止接口暂停。**

读取侧归因与 40k 分域内存观测的候选路径见[PD-5 读取候选与分域内存观测实施方案](stage_plans/active/stage-07/pd5-read-candidate-and-memory-domains-plan.md)；其 §2.1（R0-a）与 §2.2（R0-b）已执行并判定：读取候选不提名（证据：[静态指令归因](stage_reports/stages/stage-07/pd/2026-10-23-read-regression-static-attribution.md)、[有界归因实验](stage_reports/stages/stage-07/pd/2026-10-23-read-regression-bounded-attribution.md)），owner 已依预授权规则把读取项记为已量化超界、不视为通过；§3（分域内存）的 [B0 静态上限账本](stage_reports/stages/stage-07/pd/2026-10-24-b0-memory-cap-ledger.md) 已由 owner 接受（G1 逐 profile `content_entities`、G4 `state.*` 13,312 B/实体、G3 逐域 age 上限、G7 逐内容参考包络 + fixture/product 分账），[B1 域归因工具](stage_reports/stages/stage-07/pd/2026-10-24-b1-domain-attribution-tooling.md) 已完成并通过静态门（未运行），B2 64/2048 与 B3 40k 采数、规模上限口径（caps 未登记 64/2048/40k 规模）待 owner 授权与裁定，内存项仍未测量。

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

PD-5 40k 准入：[阶段 A](stage_reports/stages/stage-07/pd/2026-10-21-gameplay-admission-stage-a.md)、[B0](stage_reports/stages/stage-07/pd/2026-10-22-gameplay-admission-b0-budget-ruling.md)、[阶段 B](stage_reports/stages/stage-07/pd/2026-10-22-gameplay-admission-stage-b.md)、[R4 entries](stage_reports/stages/stage-07/pd/2026-10-22-gameplay-admission-revision4-entries.md)和[metadata 顺序修复](stage_reports/stages/stage-07/pd/2026-10-22-gameplay-admission-r4-metadata-order.md)保持。历史 advance、Snapshot budget、protected-input 和 CLI discovery 失败各自保留。最新原 40k low/high 的功能与 wire 身份已独立核验，Snapshot 均 90,823,328 bytes；[本批报告](stage_reports/stages/stage-07/pd/2026-10-23-stage-c-independent-acceptance-and-exit-gaps.md)明确全进程内存包络超限、after-close 仍持有测试 owning 值、各域内存/最终释放及正常分配器 Seek 延迟未通过。七项 Packed 物理预算、四项 hash ceilings、deadline 和 1.10/15% 不变；这不是完整 PD-5 接受。该次"全进程内存包络超限"的后续归因见[40k 包络归因](stage_reports/stages/stage-07/pd/2026-10-23-40k-memory-envelope-attribution.md)：判定改记为未测量，原因是观测器量程，不是产品超界。

PD-5 执行阻塞跟进：[受控诊断、内核修复与关闭合同交回](stage_reports/stages/stage-07/pd/2026-10-23-execution-core-diagnostic-and-repair.md)已完成低/中规模内部计时、保语义内核修复、1/64 × low/high 全链路 plumbing 和九组 Release 回归。worker 关闭候选因 Windows DLL 上下文传递及禁止私有导出合同已撤回；[公开操作级停止合同与接口方案](proposals/implementation-input/stage-07/pd5-operation-stop-contract.md)现已暂停，保持未接受、未实施候选，不再作为现有合同内最小修复的必需前置。[现有生命周期最小修复](stage_reports/stages/stage-07/pd/2026-10-23-minimal-lifecycle-repair.md)已随 57b604d 完成 factory 空 delivery 防护、Player 回调依赖析构顺序修正及有界原 Tail job 的 Close 验证；其后固定 54e8c2a 的原 40k low/high 已实际完成显式 close/reset 和进程退出。旧 40k 关闭阻塞仍未归因，本次成功只证明固定功能 profile，不构成一般生命周期或性能接受。

PD-5 本轮补充：[Gameplay-host 字典/预算核查](stage_reports/stages/stage-07/pd/2026-10-21-gameplay-host-string-budget.md)及[C2 单候选交接](stage_reports/stages/stage-07/pd/2026-10-21-c2-read-result-candidate.md)。C2 完整 Release CTest 1272 通过、1 跳过、0 失败，12 个固定被测进程完成；主指标及规模重复性未达冻结条件，C2 未合入，历史交回核验身份保留；未新建正式 preparation/registration，旧窗口 3 继续停止。

PD-5 前轮证据：[两窗报告（2026-10-21 更正）](stage_reports/stages/stage-07/pd/2026-10-20-pd5-two-window-findings-and-diagnostic-handoff.md)及[有限核查与交接](stage_reports/stages/stage-07/pd/2026-10-21-pd5-followup.md)。修复前正式 RPA read_long 两窗超界；前轮 F1/A 的 1/8/64/256 对象长块 read 比值为 1.2368/1.3441/1.3742/1.3967，各诊断区间下界 >1.10。真实 update→ready 另列，不能代替孤立读取；公共 request→ready 的 A 进程波动 49.55%，不可判定。40k Gameplay-host 两种 reuse 均在 Packed 字符串预算处失败，未进入 Snapshot/Seek/Replay；独立原 Packed codec 40k 通过不替代它。唯一 C1 候选按事前标准放弃并撤回，有限尝试不证明成本下限。旧宿主 ACK/present-return 仍分项，enqueue/drain/GPU 未判。五批材料交回核验；旧窗口 3 保持停止，原 1.10/15% 与全部生命周期门禁保留，未作性能偏离接受或正式验收。

## 阶段状态

| 阶段 | 当前状态 | 入口 |
| --- | --- | --- |
| Stage 7A | S7A-3/4、5/6、7/8 的受限功能验收完成；整体关闭仍待 RPA 收尾 | [Stage 7A 计划](stage_plans/active/stage-07/plan-a.md) |
| Stage 7-RPA | R1 已退出，R0 残余、PD 格式及 noIndependentContract 限定保留。R2 前两候选正确性证据保留，但局部性能收益未确认；第三候选 direct borrowed Result 已停止并原样迁入，本次整合构建/正确性验证进行中，无第三候选性能结论。R2/R4 未退出；R3、正式测量、实体设备、生产预算和最终接受仍独立 | [RPA 性能整改](stage_plans/active/stage-07/plan-rpa-performance-remediation.md)、[R1 实施计划](stage_plans/active/stage-07/rpa-r1-implementation.md)、[整合记录](stage_reports/stages/stage-07/rpa/2026-10-26-worktree-consolidation.md) |
| Stage 7-PD | PD-0–4 内部实施及适用门禁完成；PD-5 判定为「未通过但定案」，定案与 Stage 7-PD 退出清单尚待 owner 批准。原 40k 功能与小规模最终释放已核验，40k 分域内存仍未测量、正常分配器正式性能未通过。B0 已接受、B1 静态工具完成且未采数，B2/B3 未授权；ETW r4 三用例通过只证明工具合同。读取优化归 RPA R2，Seek 优化归其后续整改，旧窗口 3 停止，子阶段未退出 | [PD 计划](stage_plans/active/stage-07/plan-presentation-decoupling.md)、[PD-5 判定报告](stage_reports/stages/stage-07/pd/2026-10-24-pd5-verdict-and-stage-7-pd-exit.md) |
| Stage 7B+ | 未开始，等待 RPA 完整退出 | [Stage 7B+ 计划](stage_plans/active/stage-07/plan-b.md) |

Stage 0–6 已完成；Stage 8–14 尚未开始。整体 Stage 7 未关闭。
阶段依赖与后续安排见 [Stage 7 总计划](stage_plans/active/stage-07/plan.md)和[路线图](ROADMAP.md)。

## 当前问题与关闭前置


- **历史慢态及尾延迟未归因**：PD-0 最新登记组重复性通过，但不解释历史近两倍慢态；该组 Debug 宿主仍保留 15.93 ms 的单帧请求到回执尖峰。PD-5 本机有限 Release 门槛已接受，生产尾延迟预算未接受，不能据均值重复性通过推断生产延迟合格。
- **控制附近帧率波动待量化**：PD-4 实体 DFJK 完整课程与控制观察已完成，旧版容量错误本轮未复现；Reset/暂停恢复时约 240→130→240，持续时间与模块成本未测量。设备失效/恢复仍未验收。
- **局部偶发故障未定位**：B5 Pause 的 `capture_gap` 与 CXC unpack 的 `Access Denied` 尚无根因结论。
- **40k 功能已核验，内存仍未测量、正式性能未通过**：原 low/high 不重跑，原超包络记录保留；后续归因将其改记为观察器量程产物，不能据此判产品超界或内存通过。小规模最终释放不替代 40k 分域预算和最终释放。B0 静态账本已接受，B1 通过静态工具门但未运行；B2/B3 的 caps 规模口径、覆盖/阶梯缺口及采数仍待授权和裁定。F1/C1/C2 原回归与噪声结论保留，读取整改由 RPA R2 承接；ETW r4 三用例已过，工具结果不替代性能接受。见[40k 包络归因](stage_reports/stages/stage-07/pd/2026-10-23-40k-memory-envelope-attribution.md)、[B0 账本](stage_reports/stages/stage-07/pd/2026-10-24-b0-memory-cap-ledger.md)、[B1 工具](stage_reports/stages/stage-07/pd/2026-10-24-b1-domain-attribution-tooling.md)与[判定/退出清单](stage_reports/stages/stage-07/pd/2026-10-24-pd5-verdict-and-stage-7-pd-exit.md)。
- **RPA 验收未完成**：已取得 PD-4 受限实体键盘与听音观察；精确音画同步、设备失效与压力验证、生产参数及预算接受仍待完成。R0 的 Seek/ON-OFF、Close 软件退休观测、旧 fence 与导出容量 partial 均按[原报告](stage_reports/stages/stage-07/rpa/2026-10-24-r0-observation-and-anchor.md)保留历史结论。Q4 仍为 `inconclusive`（历史 B 探针缺失）。R1 已新增真实 owner 许可反例、独立 Close step 观测及四类续行证据，未回填 R0；Debug 和首错缺口已解决并经当前五构建复验；当前修复身份有限 pilot 通过，R1 已按本次授权完成退出核验，RPA 最终接受仍未完成；全仓格式限定及 noIndependentContract 导出事实见[新记录](stage_reports/stages/stage-07/rpa/2026-10-25-r1-continuation.md)。
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
