# R3 第二批正式门测量（2026-11-01）

状态：test-only 测量；不改任何判据、范围或阈值。它**不是** R3 技术接受、设备验证或生产接受。

日期：2026-11-01　源码：`6256af8b7d349b14a6d5b006d7632711152e96fa`

返回 [RPA 报告索引](../README.md)。当前阶段状态只见
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md)。
证据附件见 [第二批门测量证据](2026-11-01-r3-gate-batch2-evidence/index.md)。

## 仪器与边界

用的是仓库自带的 PD5 脚本化 host 探针，编译进 `cuexis_execution_device_probe`
（`tests/pd/pd5_device_probe.cpp` 定义 `CUEXIS_PD5_DEVICE` 后包含 `pd5_host_probe.cpp`）。这一点很关键：
`cuexis_pd_host_probe` 那个可执行文件用的是 CPU 测试渲染器，**发不出** `gpu_render`；只有设备探针走真实
OpenGL 后端与 timer query。

| 门 | 边界 | 统计口径 |
| --- | --- | --- |
| Pause / Resume / resize / Reset / Close | 命令 → 状态确认 → 下一张可用画面 present-return | 计划规定控制类取窗口内全部预声明操作的最大值 |
| request→ready | 时钟轮询 → `advance` → `sample` 返回可用的帧 | 每进程 p99（n=1200 ≥ 计划要求的 1000） |
| GPU render | GPU 命令范围的 timer query 间隔 | 每进程 p99（n=1200） |

`present_call` 与 `request_to_present_return` 一并披露，但按计划单列：它们含 VSync 等待，不是任何门的替代。

## 结果

控制组：3 个新进程 × 20 个周期（每周期 pause、resume、pause、`reset_paused`、resume、
`reset_running`、resize）。

| 门 | 判据 | 读数 | 结论 |
| --- | --- | --- | --- |
| Pause | 每次 ≤50 ms | 每进程 40 次，最大 **184.929 ms**（三个进程的首次 pause 分别是 178.703 / 184.929 / 174.966 ms） | **超界** |
| Resume | 每次 ≤50 ms | 最大 2.248 ms | 通过 |
| resize | 每次 ≤50 ms | 最大 1.171 ms | 通过 |
| Reset（暂停态） | 每次 ≤1000 ms | 最大 70.666 ms | 通过 |
| Reset（运行态） | 每次 ≤1000 ms | 最大 31.351 ms | 通过 |
| Close | 每次 ≤1000 ms | 最大 1.537 ms | 通过 |

时间组：3 个新进程 × 1200 帧（预热 64 帧，240 Hz 节拍）。

| 门 | 判据 | 每进程 p99 | 结论 |
| --- | --- | --- | --- |
| request→ready | p99 ≤4.167 ms | 0.423 / 0.408 / 0.436 ms | 通过 |
| GPU render | p99 ≤4.167 ms | 0.0031 / 0.0031 / 0.0031 ms | 通过 |

参考读数（不是门）：`request_to_ready` 中位 257–261 µs、最大 713–1236 µs；`cpu_submit` 中位 129–153 µs、
p99 323–334 µs；`present_call` 中位 57–83 µs（有一个进程的 p99 达到 3 878 µs，是 VSync 等待）；
`request_to_present_return` 中位 487–659 µs。

## Pause 这一条要说清楚

三个进程各自恰好有**一条** pause 超过 50 ms，而且都是该进程的**第一次** pause；其余 39 次的中位数是
0.79–1.86 ms，最大也不到 2.3 ms。也就是说这不是"pause 普遍慢"，而是**首次冻结绘制**的一次性成本：
第一次 pause 触发的冻结画面路径要付出约 175–185 ms 的首次使用代价（与该路径首次编译/首用一致），之后
不再出现。判定按计划取窗口内全部预声明操作的最大值，所以这一条记 **超界**，同时把"每进程恰好一条、
且是第一条"这个结构如实登记：它直接指出可修的地方在首次使用，而不是稳态路径。

## 未判定与未覆盖

- **D2 / D3 / Snapshot 物化 / 排队**：本机没有任何仪器能给出可比较的无插桩子阶段边界。计划明确写着
  这种情况记未判定、且不能用探针时间戳顶替；PD-5 也是同一结论，所以本批不发明一个边界。四条仍是未判定。
- **适用 Seek ≤20 ms**：仍未判定。功能矩阵是 CPU 测试渲染器、33/129 记录、`formal:false`，设备探针的
  host 探针没有 seek 场景；计划禁止缩小适用范围，因此不拿矩阵读数充门。
- **初次 load、读取相对门**：已在第一批测过，本批不重复。

## 边界

- test-only；不提升任何生产门限，不替换冻结容量列表，不构成 R3/R4、设备或生产接受。
- 窗口在 `CUEXIS_PD5_SMOKE=1` 下隐藏；`vsync = true`，`present_call` 里的毫秒级停顿是 VSync 等待。
- 只在 1280×720 视口、本机、本构建与本次会话成立。
- 作废 1 次：控制组第一次尝试的驱动器在解析时崩溃（局部 `statistics()` 遮蔽了模块），读数从未打印或
  记录；该进程日志保留在 `controls.aborted-1/`，修正后按预注册重跑 3 个新进程。

## 复现

```powershell
python -B out/verification-20261101/r3-gate-batch2/run_pd5_gates.py
```
