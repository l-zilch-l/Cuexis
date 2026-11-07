# 提交路径停顿的指令地址采样诊断：预注册计划

- 日期：2026-11-01
- 位置：`out/verification-20261101/r3-stall-mechanism-v1/`（临时验证输出，不是阶段报告，不是验收证据）
- 状态：诊断（diagnostic）。本计划在开始测量之前写成。
- 目的：回答一个具体问题——在 1080p 容量阶梯里，慢帧在 `backend.submit` 内多花的 2.4-3.8 ms 究竟花在**哪段代码**上。

## 1. 已经确定、不再重复验证的事实

1. 慢帧不是 GPU 变慢。GPU 时间戳显示 GPU 只占每帧 10-13% 的墙钟时间，且慢帧的 GPU 区间只比快帧高 1.6-7%（1800 渲染 4.1-4.6 倍那次只有 3 帧，n=3，不作为主要依据）。
2. 慢帧的额外时间落在 `submit_us` 里：2400 静态慢帧 `submit_us` 6375-6389 对比快帧 2993-3086；2800 静态 6235-6272 对比 3637-3824。
3. 每帧都已有一次同步管线等待：`open_gl_presentation.cpp` 第 1620-1622 行在 `pixelReadbackEnabled` 时执行 1 像素 `glReadPixels`。阶梯的每一帧都打开了这个开关，所以阶梯的 `submit_us` 本身就包含一次 GPU 排空等待。
4. 慢帧总时长在 2400-2800 三个点上聚在 7120-7171 µs，而快帧基线在 3025-4750 µs 之间变化，所以"额外时间固定约 7.15 ms"是阈值选择造成的表象，额外量在 2.4-3.8 ms 之间浮动（已在读数中记录，尚未发布更正）。

## 2. 本次要区分的假设与各自的预测签名

采样器在采样瞬间冻结目标线程、读出 RIP、再恢复。它回答"卡在哪里"，不回答"卡了多久"。因此判据是**慢簇内 RIP 的分布**，不是时长。

| 假设 | 机制 | 预测签名 |
| --- | --- | --- |
| H1 堆/页错误风暴 | 每帧约 0.5-3 MB 的向量周转触发 MSVC 段堆的 decommit/commit，慢帧要等 ~1700 次页错误 | 慢簇 RIP 集中在 `ntdll!RtlpHp*`/`RtlpAllocateHeap*`/`RtlpDecommitHeapPages`/`NtAllocateVirtualMemory`/`NtProtectVirtualMemory` 或 exe 内的 `memcpy`/`memset`；**慢簇内页错误率显著高于基线**（≳100 次/ms） |
| H2 驱动内阻塞 | 某个 GL 调用在驱动里等内部资源（pushbuffer/pool 补货） | 慢簇 RIP 在 `nvoglv64.dll`/`opengl32.dll` 内，或 `ntdll` 的等待桩（`NtWaitForSingleObject` 等）；页错误率≈基线 |
| H3 交换链/合成器等待 | `SDL_GL_SwapWindow` 因为上一帧仍在飞行或合成器未释放缓冲而阻塞 | 慢簇 RIP 在 `SDL3.dll!SDL_GL_SwapWindow` 的调用链里，最终停在 `ntdll` 的等待桩；页错误率≈基线；慢簇出现呈周期性 |
| H4 调度/抢占 | 线程没被阻塞，只是被外部抢了 CPU | **找不到长同址簇**；慢簇内模块占比与背景分布一致；帧时长整体变长而不是某一段固定拖延 |

H1 与 H2/H3 的分辨点是页错误率：采样器每个采样周期顺带记录目标进程的 `PageFaultCount`，可以在**不新增插桩**的情况下给出慢簇内外的页错误率比值。H4 的分辨点是"有没有簇"：如果额外时间被均匀摊到所有 RIP 上，簇检测器就找不到东西。

## 3. 注册的分组

- 单组：`entities=2800`、`mode=static`、1080p、每进程 20 s、3 个全新进程、采样 26 s（覆盖启动后到测量结束）。
- 探针环境与阶梯一致：`CUEXIS_GPU_BARRIER=1`、`CUEXIS_GPU_PIXEL_READBACK=1`、`CUEXIS_GPU_WIDTH=1920`、`CUEXIS_GPU_HEIGHT=1080`、`CUEXIS_GPU_SECONDS=20`、`CUEXIS_GPU_WARMUP=8`、`CUEXIS_GPU_WARMUP_MS=2000`；**`CUEXIS_GPU_TIMER=0`**（定时器会增加每帧 2 次 `glQueryCounter` + 2 次 `glGetError`，本次不需要）。
- 进程控制：亲和性 65535、`BelowNormal`、execution throttling 关闭，与阶梯一致；采样器自身 `BelowNormal` 且亲和性限制在单个核心（掩码 0x8000），避免抢占探针。
- 二进制度量：探针 `out/build/rpa-candidate-release/bin/cuexis_execution_device_probe.exe`，`sha256=daa0c3cf370c74a408073eac35334a4e4dd81c113f80928fcfd9744a23d6aca8`；源码 `tests/player/gpu_throughput_probe.cpp`，`sha256=e98f1dac026e51cfebdfb331c657bdb9a6e77e6896a493f6c57a8040fdcd07f0`。二者与 `r3-stall-mechanism-v1/n2800/registration.json` 记录的一致。
- 采样器：`sample_rip.py`，采样形状为每轮 20 次连采后休眠 1 ms，实测约 13 kHz/线程。

## 4. 有效性判据（先写后测）

1. **控制量**：探针自己的日志必须仍然呈现慢帧。以 `cycle_us > 6000` 计，2800 静态在无采样诊断里的慢帧率中位数为 13.11%（12.54-13.88%）。若本次慢帧率掉到 1% 以下，说明采样本身就是病因掩盖者，本组作废，改为降低采样率重跑，并在记录里写明。
2. 采样失败数（`GetThreadContext` 失败或 RIP 为 0）必须为 0；非零则该进程作废。
3. 三个进程的模块占比量级必须一致（top 模块相同）；若不一致，只报告一致的结论。
4. 采样器只定位，不出时长结论；**不得**用本次采样的任何数值去比对阶梯或诊断的 FPS/p99。
5. 本诊断不改变已冻结的 1800 渲染/1300 静态容量，不构成任何阶段的验收。

## 5. 报告与归档

- 结果写入同目录新的日期报告（`docs/stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-01-r3-submit-stall-rip-sampling.md`），明确标注 post-hoc diagnostic，并与阶梯结果表分开陈述。
- 运行目录（含 `registration.json`、日志、采样流、模块快照、分析 JSON）按现有归档约定冻结；报告只引用哈希，不复述未归档的中间数字。

## 6. 已知的仪器局限

1. 每次采样是一次 `SuspendThread`/`GetThreadContext`/`ResumeThread` 往返，会改变指令节奏；13 kHz 意味着每 77 µs 打断一次。这正是为什么要用探针自己的慢帧率做控制量。
2. 挂起发生在用户态返回点：被内核等待阻塞的线程，其 RIP 停在该系统调用桩内，所以"在 `ntdll` 里"意味着"在等某个内核对象"，但**不**直接给出是哪个 GL 调用发起的。若需要在调用方定位，需要另做栈回溯，不在本次范围。
3. 采样器看到的 `PageFaultCount` 是进程级累计值，分辨率约 1 ms；只能给出"慢簇内 vs 慢簇外"的比值，不能给出单帧归因。
4. `nvoglv64.dll` 没有本地导出符号，落在其中的 RIP 只能报模块内偏移；`ntdll.dll`/`kernel32.dll`/`SDL3.dll` 的 RIP 会附最近导出符号。
