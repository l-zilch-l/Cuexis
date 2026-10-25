# Stage 7 PD Evidence

本目录保存 Presentation Decoupling 的设计演进、跨阶段审查、实施和测量证据。当前状态唯一见 [CURRENT_STATUS](../../../../CURRENT_STATUS.md)。

## 实施与回归

- [PD-5 判定报告与 Stage 7-PD 退出清单](2026-10-24-pd5-verdict-and-stage-7-pd-exit.md)（判定：未通过但定案；退出清单待 owner 接受）

- [PD-5 B1 域归因工具完成](2026-10-24-b1-domain-attribution-tooling.md)（B1 门通过：29 项单测全绿、宏关闭零代码审计；未运行）

- [PD-5 B0 静态内存上限账本与逐域上限推导](2026-10-24-b0-memory-cap-ledger.md)（B0 门已于 2026-10-24 接受：G1/G3/G4/G7 关闭，G9 顺延 B1）

- [P8 40k 单侧完整规模定案表](2026-10-24-p8-40k-single-sided-complete-scale-ruling.md)

- [关闭路径 `host.rpa.control_pending` 静态定性](2026-10-24-close-path-control-pending-static-qualification.md)

- [PD-5 40k 内存包络的 owner 归因与需求/设计缺陷判定](2026-10-23-40k-memory-envelope-attribution.md)

- [PD-5 读取回归的有界归因实验（R0-b）](2026-10-23-read-regression-bounded-attribution.md)

- [PD-5 读取回归的静态指令归因（R0-a）](2026-10-23-read-regression-static-attribution.md)

- [PD-5 F1 工具包合并封包与三用例验收准备](2026-10-23-f1-tool-package-and-acceptance-preparation.md)

- [PD-5 最小 ETW 生命周期工具两项实机验收](2026-10-23-etw-minimal-lifecycle-acceptance.md)

- [PD-5 管理员 canary 磁盘阈值中断与最小验证 profile](2026-10-23-etw-canary-disk-stop-and-minimal-profile.md)

- [PD-5 正式测量前工具修复与方案准备](2026-10-23-preformal-tool-repair-and-readiness.md)

- [PD-5 DiagTrack 命名会话核验与预检规则修正](2026-10-23-etw-background-session-classification.md)

- [PD-5 管理员只读预检：已有 WPR 会话拦截与归属待查](2026-10-23-etw-existing-session-preflight-stop.md)

- [PD-5 只读管理员预检、工具组件验证与下一证据方案](2026-10-23-readonly-etw-preflight-and-next-evidence-plan.md)

- [PD-5 ETW 启动包装器失败与独立修正版](2026-10-23-etw-wrapper-failure-and-repair.md)

- [PD-5 小规模寿命诊断、读取噪声复算与下一步采集准备](2026-10-23-memory-lifetime-and-read-noise-diagnostics.md)

- [PD-5 Stage C 独立功能验收、内存责任与退出缺口](2026-10-23-stage-c-independent-acceptance-and-exit-gaps.md)

- [PD-5 独立绑定阶段 C：Catch discovery 参数失败，native test 未启动](2026-10-23-stage-c-bound-discovery-stop.md)

- [PD-5 Snapshot-bound 阶段 C：保护哈希门禁在 native 前停止](2026-10-23-stage-c-snapshot-bound-preflight-stop.md)

- [PD-5 Snapshot probe 测试预算最小修复与验证](2026-10-23-snapshot-probe-budget-repair.md)

- [PD-5 现有生命周期最小修复与有界关闭验证](2026-10-23-minimal-lifecycle-repair.md)

- [PD-5 执行内核受控诊断、修复与关闭合同阻塞](2026-10-23-execution-core-diagnostic-and-repair.md)

- [PD-5 阶段 C：原样 40k R4 功能验收阻塞交回](2026-10-22-gameplay-admission-stage-c.md)

- [PD-5 阶段 B：R4 metadata 拒绝顺序修复](2026-10-22-gameplay-admission-r4-metadata-order.md)

- [PD-4 按需交付、真实回收与紧凑观察记录](2026-10-19-pd4-delivery-retirement-and-observation.md)
- [PD-3-c host refinement and SDK decision proposal](2026-10-19-pd3c-contract-and-version-decision.md)
- [PD-3-b committed revisions, neutral read and unique materializer](2026-10-19-pd3b-state-read-and-materialization.md)
- [PD-3-a Runtime and World software transaction](2026-10-18-pd3a-software-transaction.md)
- [PD-2 prepared binding index and projection adapter](2026-10-18-pd2-projection-adapter.md)
- [PD-1 publication bridge and local verification](2026-10-18-pd1-publication-bridge.md)
- [PD-2 continuation handoff](2026-10-18-pd2-continuation-handoff.md)

## 设计与计划审查

- [PD-4 准备与 owner 决策清单](2026-10-19-pd4-readiness-and-owner-decisions.md)
- [PD design evolution](2026-10-16-design-evolution.md)
- [PD cross-stage design review](2026-10-16-pd-cross-stage-design-review.md)
- [PD registration and plan self-review](2026-10-16-pd-registration-and-plan-self-review.md)
- [PD transaction and delivery design review](2026-10-16-pd-transaction-and-delivery-design-review.md)

## 测量与基线

- [Capsule revision 4 入口接入与阶段 B 交回](2026-10-22-gameplay-admission-revision4-entries.md)
- [Capsule revision 4 阶段 B 实施记录](2026-10-22-gameplay-admission-stage-b.md)
- [40k Gameplay-host B0 预算裁决交回（接受记录见阶段 B）](2026-10-22-gameplay-admission-b0-budget-ruling.md)
- [40k Gameplay-host 准入阶段 A 定量交接](2026-10-21-gameplay-admission-stage-a.md)
- [C2 单候选验证与交回核验](2026-10-21-c2-read-result-candidate.md)
- [Gameplay-host 字符串字典与预算独立核查](2026-10-21-gameplay-host-string-budget.md)
- [PD-5 门禁核查、有限诊断与处置交接](2026-10-21-pd5-followup.md)

- [PD-5 P0 两窗结果、初步归因与诊断接手](2026-10-20-pd5-two-window-findings-and-diagnostic-handoff.md)
- [测量准备的注册完整性与容量短测修正](2026-10-20-measurement-preparation-integrity.md)
- [Playback 测量工具与内部观测实现的功能命名修正](2026-10-20-performance-tool-functional-naming.md)
- [PD-5 窗口协议、矩阵轨迹与设备盘点扩展](2026-10-19-pd5-window-and-matrix-preparation.md)
- [PD-5 已接受阈值与首批工具/profile 准备](2026-10-19-pd5-tools-and-profile-preparation.md)
- [PD-5 测量就绪检查](2026-10-19-pd5-measurement-readiness.md)（初始盘点，正式采样未开始）
- [PD-5 测量与阈值方案](../../../../stage_plans/active/stage-07/pd5-measurement-proposal.md)（本机范围/阈值/协议已接受）
- [PD-0 controlled environment, build binding and exit evidence](2026-10-18-pd0-controlled-environment-and-build-binding.md)
- [Measurement method history](2026-10-17-measurement-method-history.md)
- [PD-0 uninstrumented baseline](2026-10-17-pd0-uninstrumented-baseline.md)
- [PD-0 measurement variance resolution](2026-10-17-pd0-measurement-variance-resolution.md)
- [PD-0 fixed-core formal baseline and SMT diagnostic](2026-10-18-pd0-core-policy-and-contention.md)
- [PD-0 supplemental retests and archive](2026-10-17-pd0-supplemental-retests-and-archive.md)
