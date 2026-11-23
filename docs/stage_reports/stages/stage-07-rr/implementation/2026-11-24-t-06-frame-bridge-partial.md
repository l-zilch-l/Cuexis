# T-06 帧对象与 GL 实验桥（部分）

状态：evidence

<!-- cuexis-meta
id: REPORT-stage-07-rr.t-06-frame-bridge-partial
status: evidence
authority: none
date: 2026-11-24
-->

## 结论

- 帧对象与提帧、非法画幅拒绝、实验安装面、共享逐符号导出、RR-F135 场景 1/2/3/5：**成立**。
- GL 实验桥的校验与借用 FBO 路径：**部分成立**。窗口路径只编译，没有真实提交对照。
- RR-F135 场景 4：**不成立**。持续 OOM 下 owning `core::Error` 的拷贝仍会分配并抛 `std::bad_alloc`，需要先裁定不分配的错误表示。
- ExecutionHost/Player 帧链、`rr_frame_layout_shared`、`rr_frame_diagnostics`、F207 与 F419/F427/F428/F421/F415：**未实施**，不能按通过计。

## 范围与方法

按 [T-06 卡](../../../../stage_plans/active/stage-07-rr/tasks/T-06.md) 实施 Playback 侧帧对象与 render_opengl 实验桥，承接 T-05 的几何与 `composeFrameProjection`，不重做几何。构建使用 `debug`（真实 GL）、`candidate-debug`（帧对象与能力检查）、`candidate-headless-debug`（宿主协议与观测用例）与两个 shared 预设（导出面）。命令统一经 vcvars 包装，精确用例名取自 `ctest -N`。

## 执行记录

| 命令 | 退出码 | 结果 |
| --- | --- | --- |
| `cmake --build --preset debug --target cuexis_render_opengl_tests cuexis_playback_tests` | 0 | 构建通过 |
| `cmake --build --preset candidate-debug --target cuexis_playback_tests` | 0 | 构建通过 |
| `cmake --build --preset candidate-headless-debug --target cuexis_execution_host_tests cuexis_host_protocol_tests` | 0 | 构建通过 |
| `cmake --build --preset shared-debug --target cuexis_player` / `candidate-shared-debug` | 0 | 两个 flavor 通过 |
| `ctest --test-dir out/build/debug -R '^rr_frame_layout'` | 0 | 6/6，含 `rr_frame_layout_gl_bridge` |
| `ctest --test-dir out/build/candidate-debug -R '^rr_laid_out_frame$|^rr_frame_layout'` | 0 | 4/4 |
| `ctest --test-dir out/build/candidate-headless-debug -R 'B4 real joint owners|R0 public operation|B5|C10|C4'` | 0 | 回退 F508 清零后 7/7 |
| `ctest --test-dir out/build/candidate-headless-debug -R '^rr_session_prepared_owner$|^rr_gameplay_only_accept$'` | 0 | 2/2 |
| 两次 install + surface 检查 | 0 | 实验头只进实验 flavor；默认不含 |

## 结果与证据

帧对象：`rr_laid_out_frame` 覆盖同一 owner 的布局、快照、来源与 token，Pout/fovY/viewport 计算，retained 重排不重读 Runtime，以及 `playback.presentation.layout.unsupported` 对非空表缺能力的拒绝。

GL 桥：`rr_frame_layout_gl_bridge` 覆盖缺能力/非法表拒绝、target/token/画幅成员校验、借用 FBO 绘制与状态归还；窗口路径只完成编译。

观测与故障：`rr_frame_observation_errors` 覆盖 F135 场景 1/2/3/5，固化 reason/stage 的故障槽读取 0 次分配；场景 4 以失败记录，没有伪装成通过。

导出与安装：`cuexis_shared_export_surface` 在两个 flavor 通过，dumpbin 直接列出新增符号；安装面由 `install-surface.txt` 记录。

详细日志与改动表在 `out/review/stage-07-rr/T-06/`（report.md 与 28 个日志/清单）。

## 限制与未做

- RR-F135 场景 4 卡在错误表示：owning `core::Error` 的拷贝分配，持续 OOM 下抛 `std::bad_alloc`。需要 D-04 裁定是否引入不分配的错误表示或明确该场景的降级语义。
- ExecutionHost/Player 帧链未接线：`FramePayload` variant、worker owner、`frame_delivery`、deadline 与 target-changed 弃帧都未改；`rr_frame_layout_shared`、`rr_frame_diagnostics` 因此没有真实 shared Player 证据。
- F207 五类资源去重与 F419/F427/F428/F421/F415 未触碰。
- `framebuffer_incomplete` 沿 T-05 记录为本机驱动不可达，本卡未复验。
- 本轮证据取自与 T-02/T-05/T-17 并行的混合工作树。

## 对登记册与合同的影响

登记册 §36 记录 T-06 的四档结论、F135 场景 4 的阻塞与 F508 的集成冲突。T-06 卡保持进行中，剩余范围转入后续任务；F508 需要 replay ledger 设计裁定后才能重新实施。
