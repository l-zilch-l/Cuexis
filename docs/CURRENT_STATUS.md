# Cuexis Current Status

状态：current

更新日期：2026-11-05

本文只维护当前阶段、正在进行的任务和未解决的问题。实施与测量细节见带日期的阶段报告。

## 当前任务

- **RPA 当前承接为 R4 集成准备**：R1、R2 已退出，R3 已于 2026-11-04 获 owner 裁定关闭；
  [R4 实施与验证方案](stage_plans/active/stage-07/05-rpa-new/r4/plan.md)已按 owner 指定的推荐组合编制，
  owner 于 2026-11-05 授权“独立完成到 O1”。B3/B4/B5 审计与本地正确性回归、
  [具体预算与参数提案](stage_plans/active/stage-07/05-rpa-new/r4/caps-proposal.md)、工具和两平台预检已完成，
  owner 于 2026-11-05 裁定采用方案 B，接受三档 reference 设计预算；生产目标仍待校准验证。
  [预注册草案](stage_plans/active/stage-07/05-rpa-new/r4/formal-registration.md)保持未登记；
  R4-4 已实现新 binding/claim/seal、普通组 runner 与 extended observer；原 pre-PD A 已重建并通过构建依赖核验，
  共同固定轨迹语义检查一致。owner 随后批准保持预算的语义整改 A 批，新增两轮 64/high 已执行完：
  首轮工具无效保留，第二轮证实 active World 唯一下界 358,241 B 超过原 327,680 B，至少超出 30,561 B。
  本批停止 memory/P8，未消耗正式额度；完整语义图、观察开销与 native/GPU 覆盖仍未闭合。
  普通 Debug 目标回归 268 cases 通过，尚非新候选完整 F1/F2；正式、实体设备与生产接受未完成。见
  [语义整改结果及后续方案](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-05-semantic-memory-remediation.md)、
  [O1 后工具准入与覆盖核验](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-05-post-o1-tool-admission.md)。
  必要实体设备证据、最终退出及条件性版本批准保持独立接受节点。
- **R3 已关闭，证据范围保持**：门账为 15 项通过、0 项部分通过、0 项未判定；最终本地同源矩阵
  在 `d70df15a` 上为 11/11 行 `local_pass`、13,635 个用例、0 失败。
  owner 接受矩阵点到 hosted 点 `de73aa2c` 的限定 delta，后者 Linux Quality、Windows MSVC、
  Windows MinGW 验证通过。该裁定仅关闭 R3，不覆盖 R4、设备、生产接受或 Stage 7。
  R4 及以后仍要求同 SHA hosted 验证，除非另有 owner 裁定。见
  [R3 退出裁定](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-04-r3-hosted-delta-and-exit-ruling.md)、
  [最终矩阵证据](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-04-rpa-acceptance-matrix-final-evidence/index.md)。
- **R3c 候选不采用**：正式采用单元因执行失败未形成有效样本，结论为 `inconclusive`；
  R3c 补丁与 128 MiB quota 均未进入产品。R3b 可信恢复边界不重开；五类适用 Seek 的本地门已通过，
  不替代 R4 集成后的正式取证。见
  [最终验证与候选处置](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-04-r3-final-validation-and-r3c-decision.md)。
- **适用范围与预算裁定继续有效**：40K 不再作为 240 FPS 的适用规模；1080p 唯一登记容量仍为
  仅渲染 1800、静态提帧 1300，后续前缘阶梯只报告实测括区，不发布新容量值。
  128 MiB 不再作为 R3 上限判据，也未设新数字上限；R4/PD 的正式 profile/caps 仍须注册并获接受。
  D2/D3/Snapshot/排队四项沿用可开关计时、归因与开销披露的限定例外，其余预算不外推。见
  [owner 四项门裁定](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-02-r3-owner-gate-rulings.md)、
  [最新容量前缘诊断](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-01-r3-capacity-frontier-rederive.md)。
- **PD-5 与 Stage 7-PD 退出仍待裁定**：历史结论为“未通过但定案”，退出清单尚待 owner 接受；
  读取、Seek 与关闭整改由 RPA 承接，旧 PD-5 窗口及其失败不追改。见
  [PD-5 判定与退出清单](stage_reports/stages/stage-07/04-pd/pd-5/exit/2026-10-24-pd5-verdict-and-stage-7-pd-exit.md)。

当前工作树为 `D:/Cuexis-worktree`，执行分支 `codex/r4-integration`，起点为本地 `master` 的 `8b499a86`。
原 `D:/Cuexis-redesign` 工作树与同名分支已删除；R4 构建分别使用独立的 `out/build/r4/pre-o1/` 与 `out/build/r4/post-o1/`。
迁移后的原始运行字节位置见[证据清单](stage_reports/stages/stage-07/05-rpa-new/evidence-catalog.md)。
源码拆分已完成，审计与复核材料见
[源码拆分安全网独立审计](stage_reports/stages/stage-07/90-shared/source-split-audit/index.md)；
逐轮实施、候选处置和测试记录从[Stage 7 报告索引](stage_reports/stages/stage-07/README.md)导航。

## 阶段状态

| 阶段 | 当前状态 | 入口 |
| --- | --- | --- |
| Stage 7A | S7A-3/4、5/6、7/8 的受限功能验收完成；整体关闭仍待 RPA 收尾 | [Stage 7A 计划](stage_plans/active/stage-07/01-a/plan.md) |
| Stage 7-RPA | R1、R2、R3 已退出；R4 O1 方案 B 有效，World 语义下界证实超界，新增诊断额度用尽，memory/P8 停止；其他准入和最终接受未完成 | [整改计划](stage_plans/active/stage-07/05-rpa-new/plan.md)、[R4 语义整改](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-05-semantic-memory-remediation.md)、[R3 退出裁定](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-04-r3-hosted-delta-and-exit-ruling.md) |
| Stage 7-PD | PD-0–4 内部实施及适用门禁完成；PD-5“未通过但定案”，退出清单待 owner 接受；分域内存及正式性能缺口由 RPA/PD 后续取证承接 | [PD 计划](stage_plans/active/stage-07/04-pd/plan.md)、[PD-5 判定报告](stage_reports/stages/stage-07/04-pd/pd-5/exit/2026-10-24-pd5-verdict-and-stage-7-pd-exit.md) |
| Stage 7B+ | 未开始，等待 RPA 完整退出 | [Stage 7B+ 计划](stage_plans/active/stage-07/06-b/plan.md) |

Stage 0–6 已完成；Stage 8–14 尚未开始。整体 Stage 7 未关闭。
阶段依赖与后续安排见[Stage 7 总计划](stage_plans/active/stage-07/plan.md)和[路线图](ROADMAP.md)。

## 当前问题与关闭前置

- **R4 集成与正式证据未完成**：B3/B4 可控机制、B5 实际部署和 F1–F4 对应门须按依赖核实与取证。
  正式矩阵沿用共同 A 和三窗口九对协议，重复性口径及每进程请求数按
  [整改计划 §9](stage_plans/active/stage-07/05-rpa-new/plan.md#9-r4集成后取证区分时间内存与设备)
  登记；R1/R2 局部结果与 R3 本地门不能拼接成 R4 接受。
- **分域内存与上限接受缺证据**：40K 功能已核验，但完整分域预算和最终释放未完成接受；
  prepare-only 峰值堆与 decoded semantic heap 仍缺有效测量。既有局部峰值和小规模最终释放不代验。
  B0/B1 历史边界保持；O1 已接受新的 64/2048/40K reference 设计预算，尚非分析堆上界或生产预算接受。
  原两轮 origin 诊断不判语义预算；后续获准的两轮语义整改已用尽，第二轮以实际 owning 子集证明
  state.world 下界超出原上限。完整图、其他域、prepare 与设备覆盖仍缺证据，正式 memory/P8 未准入。
  原 low/high 和无效首轮均保留。owner 已批准按组件采用 `page_size=128`，并已实现、通过 Debug/Release
  World/Runtime/Playback 正确性回归；构建版本为 `26.11.05-2`。该整改未运行 workload 或重新测内存，
  不改变既有超界结论；memory/P8 保持停止，等待独立准入裁定。设计与实施记录见
  [World 页存储整改设计](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-05-world-storage-remediation-design.md)、
  [实施及验证报告](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-05-world-storage-remediation-implementation.md)；
  带限制暂停或范围/预算评审仍是替代路线。见
  [语义失败与三个后续方案](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-05-semantic-memory-remediation.md)、
  [B0 账本](stage_reports/stages/stage-07/04-pd/pd-5/memory-attribution/2026-10-24-b0-memory-cap-ledger.md)、
  [B1 工具](stage_reports/stages/stage-07/04-pd/pd-5/memory-attribution/2026-10-24-b1-domain-attribution-tooling.md)、
  [40K 包络归因](stage_reports/stages/stage-07/04-pd/pd-5/memory-attribution/2026-10-23-40k-memory-envelope-attribution.md)。
- **真实设备与生产接受未完成**：已有 PD-4 受限实体 DFJK 键盘及听音观察，精确音画同步、设备失效/恢复、
  压力验证、生产参数与预算仍待验收；Reset/暂停恢复附近的帧率波动持续时间及模块成本尚未量化。
  R3 初次 load 的已通过读数不代表极端 40K 启动达标；40K 启动与 prepare 的有限证据见
  [启动可观测性与成本](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-01-r3-40k-startup-observability-and-cost.md)。
- **历史限制与偶发故障保留**：PD-0 历史慢态及尾延迟未归因；R0 的 partial 和旧 Q4 `inconclusive`
  不因 R1/R2/R3 退出而回填。B5 Pause 的 `capture_gap` 仍未定位；R3c 正式采用尝试另保留重复 Pause
  失败记录。CXC unpack 已加入有限原子重试和确定性测试，原现场具体句柄持有者仍未知。见
  [PD-0 归档](stage_reports/stages/stage-07/04-pd/pd-0/2026-10-17-pd0-supplemental-retests-and-archive.md)、
  [R0 记录](stage_reports/stages/stage-07/05-rpa-new/r0/2026-10-24-r0-observation-and-anchor.md)、
  [整合记录 §3.2](stage_reports/stages/stage-07/90-shared/worktree-consolidation/2026-10-26-worktree-consolidation.md#32-shared-probe-and-cxc-output-commit-regression-repairs)。

## 产品边界

Cuexis 由可嵌入的 Playback SDK、独立 Cuexis Player 和 Cuexis Studio 构成。
外部宿主使用公共 Playback API；RuntimeSession、World、EnTT、SDL 和 OpenGL 属于内部或可选实现。

产品决策见[ADR 0027](adr/0027-playback-sdk-product-boundary.md)，公共接口见[API reference](api/README.md)。

## 当前格式与 SDK 合同

- 当前工作树 SDK API 为 **`0.9.1` preview**，日期构建为 **`26.11.05-2`**；这不代表已经发行。
  R3 来源绑定新增 candidate 类型和 `StepTarget` 字段，消费者须整体重建，不能混用旧二进制。
  PD-4 历史基线仍为 0.9.0。版本决定见[版本政策](guides/VERSIONING.md#091-r3-candidate-来源绑定)，
  STATIC/SHARED、安装消费者及兼容门的最新本地证据见上方 R3 最终矩阵。
- Gameplay v2 是当前实施基线，顶层合同仍为 candidate；局部实现不等于整批验收。语义与运行边界见
  [Gameplay V2 Spec](formats/GAMEPLAY_V2_SPEC.md)、[ABI](api/GAMEPLAY_V2_ABI.md)及[RPA 补充合同](formats/rpa-execution-contract.md)。
- Chart v1–v4 的既有兼容路径保留；Chart v5、CXT v2、Packed Chart 与 Canonical Gameplay Graph 仍为显式候选路径，正式发行收敛归 Stage 8。
- 稳定 C ABI 归[Stage 14](stage_plans/future/stage-14/plan.md)，当前只提供 C++ typed preview。

各格式的生产/候选状态与权威文档见[格式索引](formats/README.md)。

## 延期和禁止边界

运行时脚本和逐帧 script callback 无限期延后，不预留字段、capability、bytecode、ABI 或 Playback hook。
离线 authoring generator 不会被 pack、prepare 或 Playback 隐式执行。

## 更新规则

只更新当前结论，完成或被替代的状态应就地更新或删除。
测试轮次、命令、提交经过、授权沟通和历史问题处置保存在阶段报告中，不向本文追加过程日志。
