# Stage 7 RPA Evidence

本目录保存 RPA 子阶段的设计、实施、性能、设备和生产验收证据。RPA 与 PD 分开管理；RPA 报告不构成 PD 阶段退出结论。

## 设计与实施

- [Realtime architecture review](2026-10-09-realtime-architecture-review.md)
- [Realtime architecture evidence](2026-10-09-realtime-architecture-review-evidence.zip)
- [Realtime stage separation](2026-10-09-realtime-stage-separation.md)
- [Realtime stage separation evidence](2026-10-09-realtime-stage-separation-evidence.zip)
- [AD-02 split threshold](2026-10-11-ad02-split-threshold-brief.md)
- [AD-10 feedback visibility](2026-10-11-ad10-feedback-visibility-brief.md)
- [B0 contract revision](2026-10-11-b0-contract-revision.md)
- [B1 observation and oracle](2026-10-11-b1-observation-and-oracle.md)
- [B1 baseline and measurements](2026-10-11-b1-baseline-and-measurements.md)
- [B2 storage and lifetime](2026-10-11-b2-storage-and-lifetime.md)
- [Implementation readiness](2026-10-11-implementation-readiness.md)
- [Design and entry review](2026-10-11-design-and-entry-review.md)
- [P2 prepared sharing](2026-10-11-p2-prepared-sharing-brief.md)
- [Production architecture design](2026-10-11-production-architecture-design.md)
- [TB-06 tail clock](2026-10-11-tb06-tail-clock-brief.md)
- [B3 execution and clock](2026-10-12-b3-execution-and-clock.md)
- [B4 host and resources](2026-10-12-b4-host-and-resources.md)
- [Worker deployment](2026-10-13-worker-deployment.md)
- [DLL boundary repair](2026-10-14-dll-boundary-repair.md)
- [Final matrix and budget progress](2026-10-14-final-matrix-and-budget-progress.md)
- [Wait path optimization](2026-10-14-wait-path-optimization.md)
- [F1/F2 final matrix](2026-10-15-f1-f2-final-matrix.md)
- [F3 target baseline](2026-10-15-f3-target-baseline.md)
- [F3 final baseline](2026-10-15-f3-final-baseline.md)
- [Production acceptance inventory](2026-10-14-production-acceptance-inventory.md)
- [Retention progress and wait boundaries](2026-10-17-retention-progress-and-wait-boundaries.md)
- [Wait and retirement design review](2026-10-17-wait-and-retirement-design-review.md)
- [Performance remediation design review](2026-10-24-performance-remediation-design-review.md)
- [R0 observation and anchor](2026-10-24-r0-observation-and-anchor.md)
- [R1 observation, C6 and failed Close stop review](2026-10-25-r1-stop-review.md)：历史停审记录：当时 C7/C7a 在 ON/OFF 同样复现，最小拒绝候选未应用，R1 尚未退出；后续退出结论见下项 §11。
- [R1 继续实施与本地验证](2026-10-25-r1-continuation.md)：许可/身份、四类续行、时钟与保活、Debug 栈及首错复核修复；§10 为当前五构建复验、独立 red/green 与回滚证据。§11 为当前修复构建新登记三项有限 pilot 与按本次授权核验后的 R1 退出；旧 pilot 身份、五条 PD 格式与导出容量限定保留，RPA/PD/Stage 7 未关闭。前项停审报告保留历史状态。该记录生成时 R2 尚未启动，当前进度见下项；R3/R4、40k、ETW、正式测量与设备接受仍独立受限。
- [R2 单行借用候选续做](2026-10-25-r2-single-row-continuation.md)：父代理设计与亲自核验，移除批量数组后采用同步单行借用；160 B 显式逻辑存储，五构建/消费者通过；固定诊断四规模 inconclusive，收益门未退出。
- [R2 有界借用行实施与本地验证](2026-10-25-r2-borrowed-presentation.md)：首版内部借用行、原序 owning DTO 物化、真实 scratch 布局、五构建回归、适用消费者与独立回滚通过；固定局部诊断显示单对象退化、N64 收益区间跨 1，首版性能候选未接受，R2 收益门未完成。
- [Diagnostic phase-name audit](../diagnostic-formalization/2026-10-09-diagnostic-phase-name-audit.md)
- [Diagnostic phase-name audit evidence](../diagnostic-formalization/2026-10-09-diagnostic-phase-name-audit-evidence.zip)
- [N0 diagnostic inventory](../diagnostic-formalization/2026-10-10-n0-diagnostic-inventory-review.md)
- [N0 diagnostic inventory evidence](../diagnostic-formalization/2026-10-10-n0-diagnostic-inventory-evidence.zip)

## Device evidence

- [PD-4 独立音乐与实体 DFJK 观察](device/2026-10-19-pd4-physical-music-and-keyboard-observations.md)
- [PD-3-c 后独立音乐与实体键盘观察](device/2026-10-19-pd3c-physical-music-and-keyboard-observations.md)
- [CRLF and device fixture](device/2026-10-09-crlf-and-device-fixture.md)
- [CRLF and device fixture evidence](device/2026-10-09-crlf-and-device-fixture-evidence.zip)
- [Device same-tick collision retest](device/2026-10-09-device-retest-same-tick-collision.md)
- [Device same-tick collision evidence](device/2026-10-09-device-retest-same-tick-collision-evidence.zip)
- [Discrete device repair](device/2026-10-09-player-discrete-device-repair.md)
- [Discrete device repair evidence](device/2026-10-09-player-discrete-device-repair-evidence.zip)
- [Multi-transition tick repair](device/2026-10-09-player-multi-transition-tick-repair.md)
- [Multi-transition tick repair evidence](device/2026-10-09-player-multi-transition-tick-repair-evidence.zip)
- [Readable device practice](device/2026-10-09-player-readable-practice.md)
- [Readable device practice evidence](device/2026-10-09-player-readable-practice-evidence.zip)

## Handoff

- [RPA F4 handoff](2026-10-15-rpa-f4-handoff.md)
