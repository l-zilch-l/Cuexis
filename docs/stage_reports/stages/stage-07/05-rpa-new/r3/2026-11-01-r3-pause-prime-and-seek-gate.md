# R3 Pause 与适用 Seek：预热交换链与首个 seek 门（2026-11-01）

状态：local gate readings；test-only。本轮把 **Pause 门从超界变为通过**，并把**适用 Seek 门在已覆盖的场景
上测出通过**；D2/D3/Snapshot/排队仍然未判定，原因写在 §4。

日期：2026-11-01　源码：`84277a63` + 本报告的两处改动（工作树提交为本次提交）

返回 [RPA 报告索引](../README.md)。Pause 的根因见[根因分析](2026-11-01-r3-gate-cause-analysis.md)。

## 1. Pause：在 backend 创建时预热交换链

根因（七个判别实验）是：窗口与上下文创建后的**第二次 `SwapWindow`** 会阻塞 170–190 ms，GPU 工作约 1 µs，
与 vsync、窗口可见性、冻结标志都无关，只有"前两次 present 的间隔 ≥200 ms"才能让它消失。

因此修法不是改 pause 路径，而是**在 `OpenGlBackend::create` 返回前先清屏并连续 present 两次**，把这次
一次性停顿吸收进 backend 创建（也就是 load），让首张内容帧与首次用户 pause 都不再撞上它。

改动：`engine/render_opengl/src/open_gl_backend.cpp`，13 行（两次 `glClear` + 两次 `SDL_GL_SwapWindow` 与
注释）。播放器与设备探针都经由这个入口，因此两侧同时受益。

### 读数

| 门 | 判据 | 预热前 | 预热后 | 判定 |
| --- | --- | --- | --- | --- |
| Pause | 每次 ≤50 ms | 首次 174.966–184.929 ms | **最大 16.754 ms**（3 进程 × 20 周期 = 120 次读数） | **通过** |
| Resume | ≤50 ms | 2.248 ms | 4.844 ms | 通过 |
| resize | ≤50 ms | 1.171 ms | 3.978 ms | 通过 |
| Reset（暂停/运行） | ≤1000 ms | 70.666 / 31.351 ms | 17.062 / 16.270 ms | 通过 |
| Close | ≤1000 ms | 1.537 ms | 5.059 ms | 通过 |
| 初次 load | ≤2000 ms | 394.8–424.9 ms | **580.0–603.2 ms**（9 进程，中位 587.4） | 通过（余量 3.3×） |
| request→ready p99 | ≤4.167 ms | 0.430 ms | 0.439 / 0.474 / 0.444 ms | 通过 |
| GPU render p99 | ≤4.167 ms | 3.1 µs | 3.1 µs | 通过 |

代价是明确的：约 180 ms 从"首帧之后的一次冻结"搬到了"backend 创建期间"，load 因此从 ~410 ms 涨到
~590 ms，仍在 2000 ms 预算内。用户可见的行为改善是：窗口出现的第一帧之后不再卡顿。

回归：全量 Release CTest `rc=0`、663.41 s、仅 1 项按仓库设置跳过；player smoke 的
`Frame 5 summary=18316288860163381829` golden 保持。

## 2. 适用 Seek：新增设备探针场景并测出通过

此前只有功能矩阵（CPU 测试渲染器、33/129 记录、`formal:false`），不能代替 20 ms 门。本轮在设备探针里
新增 `PD5 scripted seek timing probe`（`tests/pd/pd5_host_probe.cpp`，96 行）：真实 OpenGL 后端、真实回放
归档、`pause → replayCut → seek → sample → present`，逐次打印 `request_to_ready_us` 与
`request_to_present_us`。

预注册（测量前写定）：两个回放规模（129 / 1024 记录），每规模 6 个新进程，每进程 16 次 seek，判据为
"每次 ≤20 ms"，判定取每进程最大值。

| 回放规模 | 进程数 | 每进程最大 request→present | 最差 | 判定 |
| --- | ---: | --- | ---: | --- |
| 129 记录 | 6（全部有效） | 709 / 756 / 811 / 819 / 722 / 941 µs | 941 µs | 通过 |
| 1024 记录 | 6（全部有效） | 956 / 735 / 996 / 779 / 1012 / 805 µs | 1012 µs | 通过 |

**覆盖范围**：本装置覆盖"同档重复热使用"与"冷进程"两类；**不覆盖**外部归档首次认证、自然 checkpoint
首次使用、无合法 checkpoint 回退三类——它们需要不同的夹具（外部 replay、checkpoint 注入、证书缺失），
本轮没有做，因此 20 ms 门只在这两个类上记通过，不外推到全部适用场景。

## 3. 过程中修掉的探针问题

- 反复 seek 会耗尽 host 的控制预留（`host.rpa.capacity_exceeded`）：把探针的 `hostCapacity`、
  `captureCapacity`、`captureReserve` 提到 256/256/64，并把每进程 seek 数定为 16。
- 首次编译错误：`advance` 需要 `ClockObservation` 值而不是 `Result`；`replayCut`/`seek` 取
  `GameplayTick`；`cut` 是值不是 `Result`。均已修正。

## 4. D2 / D3 / Snapshot / 排队：仍未判定，原因具体

这四项需要一个**可比较的无插桩子阶段边界**。仓库里唯一的逐阶段计时设施是
`engine/core/internal/cuexis_internal/presentation_metrics.hpp` 的 `Stage`（含 judgement / projection /
runtime / snapshot / queue）与 `Timer`，但它：

1. 由编译期开关 `CUEXIS_PD_METRICS_TEST_ONLY` 控制，开启即改变二进制；
2. 计划明确写着"缺少无侵入计时能力时只能作独立归因探针，不能据插桩时间直接接受预算"；
3. 没有任何探针或工具把它打印出来（没有 dump/report 入口），要读数还得再写取出代码。

因此本轮**没有**给出这四项的读数：给出任何数字都只能是被禁止的那一类。要闭合它们，需要 owner 授权在
产品侧引入可开关的阶段计时并接受"归因探针 + 开销披露"的登记方式。

## 5. 门的总账（本次提交后的 HEAD）

| 门 | 读数 | 判定 |
| --- | --- | --- |
| 初次 load | 580.0–603.2 ms | 通过 |
| 正式重复性（load 组） | 3.95 % | 通过 |
| request→ready p99 | 0.439–0.474 ms | 通过 |
| GPU render p99 | 3.1 µs | 通过 |
| Pause / Resume / resize | 16.754 / 4.844 / 3.978 ms | 通过 |
| Reset（暂停/运行）/ Close | 17.062 / 16.270 / 5.059 ms | 通过 |
| 相对读取门 | 1.0405 [1.0191, 1.0624] | 通过（见[裁剪报告](2026-11-01-r3-read-gate-materializer-trims.md)） |
| 适用 Seek（已覆盖两类） | 最差 1012 µs | 通过（范围有限，见 §2） |
| D2 / D3 / Snapshot / 排队 | — | 未判定（见 §4） |

## 6. 边界

- test-only：不构成 R3 技术接受、设备验证或生产接受。
- 预热是产品行为改动：backend 创建期间多两次 present；它把一次性停顿搬进 load，不消除该停顿本身。
- 本次没有重跑 11 行同源矩阵；矩阵绑定 `6256af8`，需新的矩阵才能把"同源"重新闭合。
- Seek 的两类覆盖已写明，不能读成"全部适用场景通过"。
