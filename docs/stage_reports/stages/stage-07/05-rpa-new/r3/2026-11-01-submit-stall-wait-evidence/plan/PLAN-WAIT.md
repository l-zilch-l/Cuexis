# 提交路径停顿的等待归因采样：预注册计划

- 日期：2026-11-01
- 位置：`out/verification-20261101/r3-stall-mechanism-v1/`（临时验证输出，不是阶段报告，不是验收证据）
- 状态：诊断（diagnostic）。本计划在开始测量之前写成。
- 目的：回答一个具体问题——慢帧里那 33%-36% 的渲染线程采样落在 `ntdll` 等待桩上，**这个等待是每帧固定发生的、与停顿无关的背景，还是只在停顿点才出现的病因**。

## 1. 已经确定、不再重复验证的事实

1. `PLAN-RIP.md` 注册的指令地址采样已完成（`rip-2800-static`，`sample_rip.py` v1，27.6 kHz/线程，663 113 个采样）：慢簇 RIP 集中在 `nvoglv64.dll` 内部与 `ntdll` 等待桩，页错误率与基线无显著差异，`RtlpHp*`/`RtlpAllocateHeap*` 未见主导 ⇒ H1（页错误风暴）作为**病因**被否定，H2（驱动内阻塞）保留。
2. 探针源码不可能发出这个等待：`tests/player/gpu_throughput_probe.cpp` 只有第 246 行一处 `std::this_thread::sleep_for`，位于 `collect(true)` 的排空分支，且 `if (timing)` 之外的稳态帧不会进入；`CUEXIS_GPU_TIMER=0` 使 `GpuTimer` 整个不参与。全仓库对 `WaitForSingleObject|GetCurrentProcess` 的检索在 `engine/*/src`、`engine/*/internal`、`app/player`、`tests/player` 下命中数为 0。
3. 用 `StackWalk64` + 模块回填（`VirtualQueryEx` + `GetMappedFileNameW`）对 34 个线程做的**权威调用者归因**（不是启发式）：渲染线程在 `ntdll` 等待桩里的直接调用者是 `nvoglv64.dll +0xa1cd8a`，旁边还有 `nvoglv64.dll +0xa1ca32` 发出的 `KERNELBASE.dll +0xbb3e8`（`GetExitCodeThread`，只可能出现在 `+0x38` 偏移处）与 `nvoglv64.dll +0x7a46fd` 发出的 `NtSetEvent`。等待对象在句柄表里解析为**自身进程**（`Process id=<probe pid>`），不是事件、不是节、不是合成器。
4. 一次**探索性、未预注册**的读回对照（同一二进制，只改 `CUEXIS_GPU_PIXEL_READBACK` 1/0，各 6 次 walk）显示：关掉每帧 1 像素 `glReadPixels` 之后 `nvoglv64 +0xa1cd8a` 自等待**依然出现**。据此否定"等待就是那次读回"的假设。该对照只作机制排除，不作任何时长度量。
5. 一次**探索性、未预注册**的 40000 实体 walk（`walk-40000`）显示：40K 静态下渲染线程在 40 次抽样里只有 4 次停在 `ntdll` 桩（10%），57.5% 的采样落在 exe 自身代码里，其中一条 24 帧的链是 `free()` 经 `ntdll` 堆路径到 `NtFreeVirtualMemory`。这说明**该停顿不是 40K 的瓶颈**，40K 的成本在 exe 的每实体路径上。

## 2. 本次要区分的假设与各自的预测签名

判据是**等待发生率**（每秒进入 `NtWaitForSingleObject` 的次数）与**停顿时长占比**在两个注册组之间的比值，不是绝对值。

| 假设 | 机制 | 预测签名 |
| --- | --- | --- |
| H5 背景型 | 驱动每帧都在做固定次数的内部同步（命令缓冲补货、与自身 worker 线程握手），与停顿无关 | 1800 静态（阶梯 0 停顿）的**每帧等待率**与 2800 静态同量级（≥0.75 倍），而两组的慢帧占比差 10 倍以上 |
| H6 病因型 | 等待只在队列/资源紧张时发生，等待本身制造了慢帧 | 1800 静态的每帧等待率显著低于 2800（≤0.5 倍），且与慢帧占比同向变化 |

H5 若成立，则这个等待是所有高实体点共有的常数项：它占掉每帧墙钟的约三分之一（慢帧与快帧一起摊），因此**降低 40K 成本必须靠减少每实体提交，而不是消除等待**。H6 若成立，则等待是可控的容量杠杆。

## 3. 注册的分组

两个组，各 3 个全新进程，共 6 个进程，串行执行：

| 组 | 目录 | `entities` | `mode` | 预期 | 作用 |
| --- | --- | --- | --- | --- | --- |
| A 停顿组 | `wait-2800-static` | 2800 | `static` | 阶梯同点慢帧率 12.54-13.88%（`cycle_us > 6000`） | 测停顿点的等待率 |
| B 对照组 | `wait-1800-static` | 1800 | `static` | 阶梯同点慢帧 0 个、347.233 FPS、p99 3.5700 ms | 测无停顿点的等待率 |

- 每进程：1080p、20 s 测量、`CUEXIS_GPU_FRAMES=120`、`CUEXIS_GPU_WARMUP=8`、`CUEXIS_GPU_WARMUP_MS=2000`；**`CUEXIS_GPU_TIMER=0`**。
- 探针环境与阶梯一致：`CUEXIS_GPU_BARRIER=1`、`CUEXIS_GPU_PIXEL_READBACK=1`、`CUEXIS_GPU_WIDTH=1920`、`CUEXIS_GPU_HEIGHT=1080`。
- 进程控制：探针亲和性 65535、`BelowNormal`、execution throttling 关闭；采样器 `BelowNormal`、亲和性单核（掩码 0x8000）。
- 二进制度量：`out/build/rpa-candidate-release/bin/cuexis_execution_device_probe.exe`，`sha256=daa0c3cf370c74a408073eac35334a4e4dd81c113f80928fcfd9744a23d6aca8`；源码 `tests/player/gpu_throughput_probe.cpp`，`sha256=e98f1dac026e51cfebdfb331c657bdb9a6e77e6896a493f6c57a8040fdcd07f0`。
- 采样器：`sample_wait.py`（v4），采样形状为每轮 20 次连采后休眠 1 ms，实测约 1.0-2.8 万采样/秒（全部线程合计）。**`--stack-bytes 0`**：不抓栈字，因为它把吞吐砍半（25 769/s → 9 950/s）且会改变被测负载；调用者归因已由 `walk_once.py` 独立完成，不靠采样器。
- 句柄解析：注册组**不开** `--handles`（每采样一次 `GetProcessId`/`QueryFullProcessImageNameW` 是额外扰动）。句柄身份另用 1 个独立进程 `wait-2800-static-handles` 解析，只作身份证据，不参与任何比率。

## 4. 有效性判据（先写后测）

1. **控制量**：A 组每个进程自身的慢帧率（`cycle_us > 6000`）必须仍在 5% 以上。若掉到 1% 以下，说明采样器本身就是病因掩盖者，该进程作废并在记录里写明。
2. 采样失败只允许出现在进程退出之后（`reason=process_exit`，失败码 `suspend 5`）。退出之前出现任何失败即该进程作废。
3. 采样器的线程快照必须包含渲染线程，且渲染线程的 exe-RIP 占比是全部线程里最高的（自识别规则与 `walk_once.py` 一致）。
4. **禁止**用本组任何数值去比对阶梯或定时诊断的 FPS/p99：采样会降吞吐（已实测 175.7 FPS 对阶梯 195.9 FPS），本组只读"等待率"与"慢帧占比"两个比值。
5. 三个进程的等待率量级必须一致（极差/中位数 ≤ 15%，与全项目可重复性口径一致）；不一致则只报告一致的结论。
6. 本诊断不改变已冻结的 1800 渲染/1300 静态容量，不构成任何阶段的验收。

## 5. 报告与归档

- 报告：`docs/stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-01-r3-submit-stall-wait-attribution.md`。
- 归档：`docs/stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-01-submit-stall-wait-evidence/`，含本计划、两个采样器、分析器、驱动、walk 工具、6 个进程的日志/采样/模块表/结果、4 次 walk 的输出、`manifest.json` 与 `index.md`。
