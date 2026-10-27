# Cuexis Current Status

状态：current

更新日期：2026-10-27

本文只维护当前阶段、正在进行的任务和未解决的问题。实施与测量细节见带日期的阶段报告。

## 当前任务

- **R3 实施中**：R3a 当前恢复修复与准备去重已通过六套本地配置、STATIC/SHARED clean consumers 和宏隔离核验。
  最新[首次身份归因](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-27-r3-cold-identity-attribution.md)确认完整归档的重复结果写出、FNV 与 SHA 全量扫描是首次主要成本；该轮仅诊断，产品代码未改。
  R3b 首个候选的[裁定包](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-27-r3b-certified-prefix-and-cost.md)与后续[高 requirements 归因](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-27-r3-high-requirements-attribution.md)保持历史身份。当前分支已实施[分页共享状态、自然恢复锚点及精确身份缓存重构](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-27-r3-redesign.md)。局部内核恢复与重复 Seek 明显加快，但 2048 requirements 的 CPU 宿主首次请求到目标帧仍约 310 ms，20 ms 目标未通过。R3 未退出，R3c 尚未进入；局部结果不代替 R4、设备或生产接受，见[实施登记](stage_plans/active/stage-07/05-rpa-new/r3/plan.md)。
- **PD-5 定案与 Stage 7-PD 退出裁定**：判定报告已提出“未通过但定案”及退出清单，尚待 owner 接受；读取与 Seek 整改由 RPA 线承接，
  分域内存及正式/设备/生产验收缺口仍按下文跟踪。见[判定报告](stage_reports/stages/stage-07/04-pd/pd-5/exit/2026-10-24-pd5-verdict-and-stage-7-pd-exit.md)。

本重构分支的唯一实施入口为 `D:/Cuexis-redesign`，分支 `Cuexis-redesign`。
接手资料中的其他 worktree 和分支均为历史来源，本批不在其中执行操作。
已完成工作、候选处置和逐轮测试记录见[Stage 7 报告索引](stage_reports/stages/stage-07/README.md)。

## 阶段状态

| 阶段 | 当前状态 | 入口 |
| --- | --- | --- |
| Stage 7A | S7A-3/4、5/6、7/8 的受限功能验收完成；整体关闭仍待 RPA 收尾 | [Stage 7A 计划](stage_plans/active/stage-07/01-a/plan.md) |
| Stage 7-RPA | R1、R2 已退出；owner 于 2026-10-26 接受 R2 按原计划退出，受控局部结果及历史限制保留；R3 实施中，R4 正式、设备与生产接受未登记 | [整改计划](stage_plans/active/stage-07/05-rpa-new/plan.md)、[退出复核](stage_reports/stages/stage-07/05-rpa-new/r2/2026-10-26-r2-technical-closure-review.md) |
| Stage 7-PD | PD-0–4 内部实施及适用门禁完成；PD-5 判定为「未通过但定案」，定案与 Stage 7-PD 退出清单尚待 owner 批准。原 40k 功能与小规模最终释放已核验，40k 分域内存仍未测量、正常分配器正式性能未通过。B0 已接受、B1 静态工具完成且未采数，B2/B3 未授权；ETW r4 三用例通过只证明工具合同。读取优化归 RPA R2，Seek 优化归其后续整改，旧窗口 3 停止，子阶段未退出 | [PD 计划](stage_plans/active/stage-07/04-pd/plan.md)、[PD-5 判定报告](stage_reports/stages/stage-07/04-pd/pd-5/exit/2026-10-24-pd5-verdict-and-stage-7-pd-exit.md) |
| Stage 7B+ | 未开始，等待 RPA 完整退出 | [Stage 7B+ 计划](stage_plans/active/stage-07/06-b/plan.md) |

Stage 0–6 已完成；Stage 8–14 尚未开始。整体 Stage 7 未关闭。
阶段依赖与后续安排见 [Stage 7 总计划](stage_plans/active/stage-07/plan.md)和[路线图](ROADMAP.md)。

## 当前问题与关闭前置


- **历史慢态及尾延迟未归因**：PD-0 最新登记组重复性通过，但不解释历史近两倍慢态；该组 Debug 宿主仍保留 15.93 ms 的单帧请求到回执尖峰。PD-5 本机有限 Release 门槛已接受，生产尾延迟预算未接受，不能据均值重复性通过推断生产延迟合格。
- **控制附近帧率波动待量化**：PD-4 实体 DFJK 完整课程与控制观察已完成，旧版容量错误本轮未复现；Reset/暂停恢复时约 240→130→240，持续时间与模块成本未测量。设备失效/恢复仍未验收。
- **局部偶发故障追踪**：B5 Pause 的 `capture_gap` 仍未定位。CXC unpack 目录提交已用不共享删除的读句柄复现阻塞机制，并加入有限原子重试与确定性测试；原始现场的具体持有者仍未知，详见[整合记录 §3.2](stage_reports/stages/stage-07/90-shared/worktree-consolidation/2026-10-26-worktree-consolidation.md#32-shared-probe-and-cxc-output-commit-regression-repairs)。
- **40k 功能已核验，内存仍未测量、正式性能未通过**：原 low/high 不重跑，原超包络记录保留；后续归因将其改记为观察器量程产物，不能据此判产品超界或内存通过。小规模最终释放不替代 40k 分域预算和最终释放。B0 静态账本已接受，B1 通过静态工具门但未运行；B2/B3 的 caps 规模口径、覆盖/阶梯缺口及采数仍待授权和裁定。F1/C1/C2 原回归与噪声结论保留，读取整改由 RPA R2 承接；ETW r4 三用例已过，工具结果不替代性能接受。见[40k 包络归因](stage_reports/stages/stage-07/04-pd/pd-5/memory-attribution/2026-10-23-40k-memory-envelope-attribution.md)、[B0 账本](stage_reports/stages/stage-07/04-pd/pd-5/memory-attribution/2026-10-24-b0-memory-cap-ledger.md)、[B1 工具](stage_reports/stages/stage-07/04-pd/pd-5/memory-attribution/2026-10-24-b1-domain-attribution-tooling.md)与[判定/退出清单](stage_reports/stages/stage-07/04-pd/pd-5/exit/2026-10-24-pd5-verdict-and-stage-7-pd-exit.md)。
- **RPA 验收未完成**：已取得 PD-4 受限实体键盘与听音观察；精确音画同步、设备失效与压力验证、生产参数及预算接受仍待完成。R0 的 Seek/ON-OFF、Close 软件退休观测、旧 fence 与导出容量 partial 均按[原报告](stage_reports/stages/stage-07/05-rpa-new/r0/2026-10-24-r0-observation-and-anchor.md)保留历史结论。Q4 仍为 `inconclusive`（历史 B 探针缺失）。R1 已新增真实 owner 许可反例、独立 Close step 观测及四类续行证据，未回填 R0；Debug 和首错缺口已解决并经当前五构建复验；当前修复身份有限 pilot 通过，R1 已按本次授权完成退出核验，RPA 最终接受仍未完成；历史全仓格式限定及 noIndependentContract 导出事实见[新记录](stage_reports/stages/stage-07/05-rpa-new/r1/2026-10-25-r1-continuation.md)。
- **预算证据缺项**：端到端 Seek 延迟、prepare-only 峰值堆和 decoded semantic heap 尚未完成有效测量与接受。

问题证据与剩余门禁见 [PD-0 复测归档](stage_reports/stages/stage-07/04-pd/pd-0/2026-10-17-pd0-supplemental-retests-and-archive.md)、
[RPA 矩阵](stage_reports/stages/stage-07/03-rpa-legacy/f1-f2/2026-10-15-f1-f2-final-matrix.md)及
[预算基线](stage_reports/stages/stage-07/03-rpa-legacy/f3/2026-10-15-f3-final-baseline.md)。

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
