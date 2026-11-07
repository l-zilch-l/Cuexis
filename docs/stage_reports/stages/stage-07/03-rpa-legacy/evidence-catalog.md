# Legacy RPA Evidence Catalog

状态：current index

更新日期：2026-10-26

当前状态只以 [CURRENT_STATUS](../../../../CURRENT_STATUS.md) 为准；本页只负责定位，不新增实施、测量或阶段接受结论。

本页集中列出报告原文中的原始 out/ 与外部证据引用。它们保留登记时路径、worktree 身份和占位表达式，**不表示已逐个验证这些路径目前存在**；历史来源树只读保留。需要执行时回到原报告核对哈希、登记和接受边界。ZIP、JSON、LOG 等附件随报告原字节移动，未解包或替换内部路径。

## design

| 来源报告 | 原始证据引用 |
| --- | --- |
| [S7A-7/8 实时架构重审与规划证据](design/2026-10-09-realtime-architecture-review.md) | `out/realtime-architecture-audit/` |
| [S7A-7/8 实时架构重审与规划证据](design/2026-10-09-realtime-architecture-review.md) | `out/realtime-architecture-audit/probe-source/` |
| [RPA A1–A3设计与准入审查](design/2026-10-11-design-and-entry-review.md) | `out/detach伪称清理完成` |

## device

| 来源报告 | 原始证据引用 |
| --- | --- |
| [CRLF approval repair and device acceptance fixture](device/2026-10-09-crlf-and-device-fixture.md) | `out/crlf-device-candidate-debug-build.log` |
| [CRLF approval repair and device acceptance fixture](device/2026-10-09-crlf-and-device-fixture.md) | `out/crlf-device-linux-tests.log` |
| [CRLF approval repair and device acceptance fixture](device/2026-10-09-crlf-and-device-fixture.md) | `out/crlf-device-s7a78-candidate-release-build.log` |
| [CRLF approval repair and device acceptance fixture](device/2026-10-09-crlf-and-device-fixture.md) | `out/crlf-device-windows-tests.log` |
| [CRLF approval repair and device acceptance fixture](device/2026-10-09-crlf-and-device-fixture.md) | `out/device-acceptance/*-assemble.log` |
| [CRLF approval repair and device acceptance fixture](device/2026-10-09-crlf-and-device-fixture.md) | `out/stderr` |
| [CRLF approval repair and device acceptance fixture](device/2026-10-09-crlf-and-device-fixture.md) | `out/version-gate-crlf-local-crlf-fix.log` |
| [CRLF approval repair and device acceptance fixture](device/2026-10-09-crlf-and-device-fixture.md) | `out/version-gate-crlf-trusted-current.log` |
| [Real device retest: same Tick collision](device/2026-10-09-device-retest-same-tick-collision.md) | `out/stderr` |
| [Player discrete device input repair](device/2026-10-09-player-discrete-device-repair.md) | `out/device-input-exit-code-probe.log` |
| [Player discrete device input repair](device/2026-10-09-player-discrete-device-repair.md) | `out/device-input-green.log` |
| [Player discrete device input repair](device/2026-10-09-player-discrete-device-repair.md) | `out/device-input-red.log` |
| [Player readable four-lane device practice](device/2026-10-09-player-readable-practice.md) | `out/build/candidate-debug/bin/cuexis_chart_candidate.exe` |
| [Player readable four-lane device practice](device/2026-10-09-player-readable-practice.md) | `out/build/candidate-debug/ec/host/host-build/cuexis_reference_host.exe` |
| [Player readable four-lane device practice](device/2026-10-09-player-readable-practice.md) | `out/build/candidate-debug/s7a78-public-fixture` |
| [Player readable four-lane device practice](device/2026-10-09-player-readable-practice.md) | `out/device-practice-v2` |
| [Player readable four-lane device practice](device/2026-10-09-player-readable-practice.md) | `out/practice_final_validation.py` |
| [Player readable four-lane device practice](device/2026-10-09-player-readable-practice.md) | `out/verify_practice_window.ps1` |

## b1

| 来源报告 | 原始证据引用 |
| --- | --- |
| [B1.3/B1.4 原路径基线与初始测量](b1/2026-10-11-b1-baseline-and-measurements.md) | `out/build/headless-release` |
| [B1.3/B1.4 原路径基线与初始测量](b1/2026-10-11-b1-baseline-and-measurements.md) | `out/build/headless-release/bin/cuexis_s5h_performance_probe.exe` |
| [B1.3/B1.4 原路径基线与初始测量](b1/2026-10-11-b1-baseline-and-measurements.md) | `out/build/headless-release/bin/stage3_performance_probe.exe` |
| [B1.1/B1.2 观测身份与独立验证设施](b1/2026-10-11-b1-observation-and-oracle.md) | `out/build/mingw-headless-debug` |

## b2

| 来源报告 | 原始证据引用 |
| --- | --- |
| [B2 存储与寿命实施证据](b2/2026-10-11-b2-storage-and-lifetime.md) | `out/build/mingw-headless-debug` |

## b3

| 来源报告 | 原始证据引用 |
| --- | --- |
| [Stage 7 B3 执行与时钟实施记录](b3/2026-10-12-b3-execution-and-clock.md) | `out/b3-clock-measurements.json` |
| [Stage 7 B3 执行与时钟实施记录](b3/2026-10-12-b3-execution-and-clock.md) | `out/b3-final-debug-gates.log）` |
| [Stage 7 B3 执行与时钟实施记录](b3/2026-10-12-b3-execution-and-clock.md) | `out/b3-final-off-gates.log` |
| [Stage 7 B3 执行与时钟实施记录](b3/2026-10-12-b3-execution-and-clock.md) | `out/b3-final-shared-installed.log` |
| [Stage 7 B3 执行与时钟实施记录](b3/2026-10-12-b3-execution-and-clock.md) | `out/b3-review-debug-installed.log` |
| [Stage 7 B3 执行与时钟实施记录](b3/2026-10-12-b3-execution-and-clock.md) | `out/b3-review-debug-judgement.log` |
| [Stage 7 B3 执行与时钟实施记录](b3/2026-10-12-b3-execution-and-clock.md) | `out/b3-review-debug-playback.log` |
| [Stage 7 B3 执行与时钟实施记录](b3/2026-10-12-b3-execution-and-clock.md) | `out/b3-review-debug-rpa.log` |
| [Stage 7 B3 执行与时钟实施记录](b3/2026-10-12-b3-execution-and-clock.md) | `out/b3-review-off-gates.log` |
| [Stage 7 B3 执行与时钟实施记录](b3/2026-10-12-b3-execution-and-clock.md) | `out/b3-review-release-*.log` |
| [Stage 7 B3 执行与时钟实施记录](b3/2026-10-12-b3-execution-and-clock.md) | `out/b3-review-release-rpa.log` |
| [Stage 7 B3 执行与时钟实施记录](b3/2026-10-12-b3-execution-and-clock.md) | `out/b3-review-shared-installed.log` |
| [Stage 7 B3 执行与时钟实施记录](b3/2026-10-12-b3-execution-and-clock.md) | `out/b3-review-targeted.log` |
| [Stage 7 B3 执行与时钟实施记录](b3/2026-10-12-b3-execution-and-clock.md) | `out/b3-shared-ctest.log` |
| [Stage 7 B3 执行与时钟实施记录](b3/2026-10-12-b3-execution-and-clock.md) | `out/b3-source-manifest.json` |
| [Stage 7 B3 执行与时钟实施记录](b3/2026-10-12-b3-execution-and-clock.md) | `out/b3-source.diff` |

## b4

| 来源报告 | 原始证据引用 |
| --- | --- |
| [Stage 7 B4 宿主与资源分离实施记录](b4/2026-10-12-b4-host-and-resources.md) | `out/b4-*.log` |
| [Stage 7 B4 宿主与资源分离实施记录](b4/2026-10-12-b4-host-and-resources.md) | `out/b4-independent-source-manifest.json` |
| [Stage 7 B4 宿主与资源分离实施记录](b4/2026-10-12-b4-host-and-resources.md) | `out/b4-pre-independent-source-manifest.json` |
| [Stage 7 B4 宿主与资源分离实施记录](b4/2026-10-12-b4-host-and-resources.md) | `out/b4-root-independent-confirm.log` |
| [Stage 7 B4 宿主与资源分离实施记录](b4/2026-10-12-b4-host-and-resources.md) | `out/b4-source.diff` |

## deployment

| 来源报告 | 原始证据引用 |
| --- | --- |
| [B5 线程部署与自动验证记录](deployment/2026-10-13-worker-deployment.md) | `out/b5-*.log` |
| [B5 线程部署与自动验证记录](deployment/2026-10-13-worker-deployment.md) | `out/b5-device-basics-20261013` |
| [B5 线程部署与自动验证记录](deployment/2026-10-13-worker-deployment.md) | `out/b5-local-measurements.json` |
| [B5 线程部署与自动验证记录](deployment/2026-10-13-worker-deployment.md) | `out/b5-source-manifest.json` |
| [B5 线程部署与自动验证记录](deployment/2026-10-13-worker-deployment.md) | `out/build` |
| [B5 线程部署与自动验证记录](deployment/2026-10-13-worker-deployment.md) | `out/build-cleanup-evidence-20261013` |
| [B5 线程部署与自动验证记录](deployment/2026-10-13-worker-deployment.md) | `out/build-cleanup-inventory-20261013.json` |
| [B5 线程部署与自动验证记录](deployment/2026-10-13-worker-deployment.md) | `out/build-cleanup-result-20261013.json` |
| [B5 线程部署与自动验证记录](deployment/2026-10-13-worker-deployment.md) | `out/content-cleanup-20261014/` |
| [B5 线程部署与自动验证记录](deployment/2026-10-13-worker-deployment.md) | `out/device-acceptance-cleanup-check/` |
| [B5 线程部署与自动验证记录](deployment/2026-10-13-worker-deployment.md) | `out/entity-practice` |
| [B5 线程部署与自动验证记录](deployment/2026-10-13-worker-deployment.md) | `out/entity-v5/dynamic` |
| [B5 线程部署与自动验证记录](deployment/2026-10-13-worker-deployment.md) | `out/entity-v5/dynamic/source-manifest.json` |
| [B5 线程部署与自动验证记录](deployment/2026-10-13-worker-deployment.md) | `out/entity-v5/final-*-tests.log` |
| [B5 线程部署与自动验证记录](deployment/2026-10-13-worker-deployment.md) | `out/entity-v5/judgement/gameplay-assemble.log` |
| [B5 线程部署与自动验证记录](deployment/2026-10-13-worker-deployment.md) | `out/execution-practice` |
| [B5 线程部署与自动验证记录](deployment/2026-10-13-worker-deployment.md) | `out/execution-practice/implementation.json` |
| [B5 线程部署与自动验证记录](deployment/2026-10-13-worker-deployment.md) | `out/execution-practice/ready-display.png` |
| [B5 线程部署与自动验证记录](deployment/2026-10-13-worker-deployment.md) | `out/execution-practice/rollback-backup` |
| [B5 线程部署与自动验证记录](deployment/2026-10-13-worker-deployment.md) | `out/execution-practice/runs/20261013-191430-003` |
| [B5 线程部署与自动验证记录](deployment/2026-10-13-worker-deployment.md) | `out/qa-stderr` |
| [B5 线程部署与自动验证记录](deployment/2026-10-13-worker-deployment.md) | `out/restart-investigation` |
| [RPA DLL 边界方案 A 修复与验证](deployment/2026-10-14-dll-boundary-repair.md) | `out/` |
| [RPA DLL 边界方案 A 修复与验证](deployment/2026-10-14-dll-boundary-repair.md) | `out/rpa-dll-fix-20261014/boundary-summary.json` |

## f1-f2

| 来源报告 | 原始证据引用 |
| --- | --- |
| [Stage 7-RPA 自动矩阵、预算测量与生产验收准备](f1-f2/2026-10-14-final-matrix-and-budget-progress.md) | `out/rpa-final-20261014/matrix/20261014-201610-9596cd60/` |
| [Stage 7-RPA 自动矩阵、预算测量与生产验收准备](f1-f2/2026-10-14-final-matrix-and-budget-progress.md) | `out/rpa-final-20261014/release-preflight-recheck.log` |
| [RPA F1/F2 Matrix Evidence](f1-f2/2026-10-15-f1-f2-final-matrix.md) | `out/rpa-final-20261015` |
| [RPA F1/F2 Matrix Evidence](f1-f2/2026-10-15-f1-f2-final-matrix.md) | `out/rpa-final-20261015/mingw-release-retest-20261015-135757/20261015-135757-b77bc2aa` |

## f3

| 来源报告 | 原始证据引用 |
| --- | --- |
| [RPA F3 Baseline Measurement](f3/2026-10-15-f3-final-baseline.md) | `out/rpa-final-20261015/f3-chart-capacity-mingw-release.json` |
| [RPA F3 Baseline Measurement](f3/2026-10-15-f3-final-baseline.md) | `out/rpa-final-20261015/f3-chart-capacity-msvc-release.json` |
| [RPA F3 Baseline Measurement](f3/2026-10-15-f3-final-baseline.md) | `out/rpa-final-20261015/serial-budgets-053d3848/measurements.json` |

## performance

| 来源报告 | 原始证据引用 |
| --- | --- |
| [Stage 7 RPA Wait Path Optimization Evidence](performance/2026-10-14-wait-path-optimization.md) | `out/perf-investigation-20261014/` |
| [Stage 7 RPA Wait Path Optimization Evidence](performance/2026-10-14-wait-path-optimization.md) | `out/perf-investigation-20261014/build_deep.py` |
| [轻量回收进度、幂等退役与回复等待边界实施](performance/2026-10-17-retention-progress-and-wait-boundaries.md) | `out/pd-20261017/analyze_final_inconclusive.py` |
| [轻量回收进度、幂等退役与回复等待边界实施](performance/2026-10-17-retention-progress-and-wait-boundaries.md) | `out/pd-20261017/final-measurement-20261017T043805Z/manifest.json` |
| [轻量回收进度、幂等退役与回复等待边界实施](performance/2026-10-17-retention-progress-and-wait-boundaries.md) | `out/pd-20261017/private-installed-consumer.log` |
| [轻量回收进度、幂等退役与回复等待边界实施](performance/2026-10-17-retention-progress-and-wait-boundaries.md) | `out/pd-20261017/retention-guards-20261017T040225Z/` |
| [轻量回收进度、幂等退役与回复等待边界实施](performance/2026-10-17-retention-progress-and-wait-boundaries.md) | `out/pd-20261017/retention-installed-consumer-devshell.log` |
| [轻量回收进度、幂等退役与回复等待边界实施](performance/2026-10-17-retention-progress-and-wait-boundaries.md) | `out/pd-20261017/retirement-build-{release,debug}.log` |
| [轻量回收进度、幂等退役与回复等待边界实施](performance/2026-10-17-retention-progress-and-wait-boundaries.md) | `out/pd-20261017/retirement-regression-20261017T035703Z/` |
| [轻量回收进度、幂等退役与回复等待边界实施](performance/2026-10-17-retention-progress-and-wait-boundaries.md) | `out/pd-20261017/retirement-regression-20261017T035902Z/` |
| [轻量回收进度、幂等退役与回复等待边界实施](performance/2026-10-17-retention-progress-and-wait-boundaries.md) | `out/pd-20261017/retirement-regression-20261017T040001Z/` |
| [轻量回收进度、幂等退役与回复等待边界实施](performance/2026-10-17-retention-progress-and-wait-boundaries.md) | `out/pd-20261017/unsupported-reply-move-injection.txt` |
| [轻量回收进度、幂等退役与回复等待边界实施](performance/2026-10-17-retention-progress-and-wait-boundaries.md) | `out/pd-20261017/）` |
| [轻量回收进度、幂等退役与回复等待边界实施](performance/2026-10-17-retention-progress-and-wait-boundaries.md) | `out/pd0-host-delivery/accept-20261017T040350Z-e497fa/` |
| [轻量回收进度、幂等退役与回复等待边界实施](performance/2026-10-17-retention-progress-and-wait-boundaries.md) | `out/pd0-host-delivery/accept-20261017T043813Z-0d1b01` |
| [轻量回收进度、幂等退役与回复等待边界实施](performance/2026-10-17-retention-progress-and-wait-boundaries.md) | `out/pd0-host-delivery/calibrate-20261017T040343Z-1f4814/` |
| [轻量回收进度、幂等退役与回复等待边界实施](performance/2026-10-17-retention-progress-and-wait-boundaries.md) | `out/pd0-host-delivery/calibrate-20261017T043810Z-676179` |
| [轻量回收进度、幂等退役与回复等待边界实施](performance/2026-10-17-retention-progress-and-wait-boundaries.md) | `out/pd0-owner-wait/20261017T040334Z-9984b4/` |
| [轻量回收进度、幂等退役与回复等待边界实施](performance/2026-10-17-retention-progress-and-wait-boundaries.md) | `out/pd0-owner-wait/20261017T043805Z-e9fe1c` |
| [轻量回收进度、幂等退役与回复等待边界实施](performance/2026-10-17-retention-progress-and-wait-boundaries.md) | `out/pd0-scheduling-v2/calibrate-20261017T043818Z-453e2a` |
| [轻量回收进度、幂等退役与回复等待边界实施](performance/2026-10-17-retention-progress-and-wait-boundaries.md) | `out/pd0-scheduling-v2/validate-20261017T043856Z-a8d517` |
| [回复等待与请求退役回收方案复审](performance/2026-10-17-wait-and-retirement-design-review.md) | `out/pd-20261017/retirement-build-{release,debug}.log` |
| [回复等待与请求退役回收方案复审](performance/2026-10-17-wait-and-retirement-design-review.md) | `out/pd-20261017/retirement-red.log` |
| [回复等待与请求退役回收方案复审](performance/2026-10-17-wait-and-retirement-design-review.md) | `out/pd-20261017/retirement-regression-20261017T034027Z/` |
