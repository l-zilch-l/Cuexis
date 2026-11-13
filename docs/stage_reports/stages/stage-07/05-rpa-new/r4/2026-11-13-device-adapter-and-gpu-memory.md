# 设备单元 adapter 与 GPU 侧内存取证（2026-11-13）

状态：candidate evidence；设备 adapter 已实现未跑组，GPU 内存跑通一个用例但规模不匹配

日期：2026-11-13

返回 [R4 门账](2026-11-08-r4-gate-status.md)与[五类 Seek 结果](2026-11-13-seek-five-classes.md)。

## 1. 设备 adapter

`prepare_probe.py` 新增 `--device`：绑定 `cuexis_execution_device_probe.exe`，跑设备专用的
`PD5 renderer and capture workload matrix`，只接纳 `capture-1/2/4`（其他模式在设备构建里没有
renderer 端点，`pd5_matrix_probe.cpp` 会 `REQUIRE` 失败）。

**设备身份不是"CPU 身份 + gpu=true"**，这一点此前被误判。设备矩阵实际报告：

| 字段 | CPU | 设备 |
| --- | --- | --- |
| `harness` | `presentation-matrix` | `renderer-matrix` |
| `renderer_consumers` | 0 | 1 |
| `materializations` | frames × consumers | frames × (consumers + 1) |
| 计数 | 三项 | 另加 `renderer_delivery`、`cpu_submit`、`present_call` |

这些都在 `probe_units.py` 里按代码推导，所以设备日志能通过**未改动**的 runner 身份校验；
观测到 `gpu:false`（或任何 CPU 身份）会在写证明之前被拒绝。新增 6 项测试，套件 76 项通过。

**未启用**：`measurement_mode` 仍是 `normal`（"device" 需要观察器准入绑定，且 runner 只接受
`normal`），GPU timer 环境变量未设置。

**阻碍**：设备单元的 `stable_protocol_id` 只能是 `p4-capture-N-release`，与 CPU 单元共用同一个
attempt 台账键。`p4-capture-1` 的 CPU 额度已用尽，因此设备组当前无法 claim——需要先决定
backend 是否进入台账键。代码在分支 `r4-device-adapter`（`a9ca54be`）。

## 2. GPU 侧内存取证

用带分配观察器的设备构建（`--mode memory`，`CUEXIS_ALLOCATION_LIFETIME_OBSERVER=ON`）跑了
`gpu-stream-v1`：`PD5 scripted host timing probe`、`MODE=stream`、1 实体、14,400 帧、2400 预热、
smoke 关闭可见窗口，exit 0、67,224 条断言、`replay_equal=true`。

| 检查点 | 请求字节 | 活分配 | 判定 |
| --- | ---: | ---: | --- |
| before-load | 25,416 | 346 | 无超界 |
| **active** | **3,944,204** | **901** | **22 个声明域全部在内** |
| before-close | 53,328,666 | 102,984 | `state.current` 44.86 MB vs 4.26 MB |
| after-close | 210,376 | 403 | 无超界 |

`active` 的主要域：`command.session` 2.57 MB、`command.host` 590 KB、`steps` 292 KB、
`state.world` 140 KB、`state.current` 118 KB、`delivery.receipt` 30 KB；观察器自身成本 109,440 B。

`before-close` 的 `state.current` 超界**不是判定**：`r4-reference-n2048-v1` 的上限本来就被 CPU 的
n2048 容量跑法超出（`state.current` 13.25 MB、`command.session` 27.05 MB），而设备用例是 1 实体
/14,400 次推进，规模不匹配。

## 3. 两个结构性缺口

1. **设备探针表达不了 2048 实体容量用例**：`pd5_capacity_probe.hpp` 只在
   `#if !defined(CUEXIS_PD5_MATRIX_DEVICE)` 下编译，设备 TU 定义了该宏，构建产物里也没有那条
   测试名。要测设备侧 2048 实体内存，必须先让容量探针支持设备构建（改 C++）。
2. **`gpu` 域永远观测不到**：观察器只覆盖链接期 C++ `new/delete`，没有任何分配被标记为 gpu
   来源；67 MB 后端上限与 268 MB 驱动增量上限因此都未测。

## 4. 边界

- GPU 内存只有一个进程、一次运行，没有重复性；GPU timer 关闭、窗口隐藏、帧节奏关闭。
- 不覆盖 prepare-only 峰值、驱动/VRAM 记账、观察器扰动（每次分配 64 B 头 + 51,776 B 静态表）。
- 以上都不构成任何门通过；`gpu` 与驱动的声明上限仍是未测量状态。

证据：`out/evidence/r4/memory-gpu/gpu-stream-v1/`（`summary.json`、`attribution.json`、
`stdout.log`、`build/`）与 `out/evidence/r4/memory-gpu/README.md`。
