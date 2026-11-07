# R3 Pause 预热与适用 Seek 证据（2026-11-01）

本目录保存两项门修复的全部字节：Pause 的交换链预热读数、预热后的初次 load 读数、适用 Seek 的新探针与
12 个进程的 seek 读数。

- [返回本轮报告](../2026-11-01-r3-pause-prime-and-seek-gate.md)

## 分组

| 路径 | 内容 |
| --- | --- |
| `pause/` | 3 进程 × 20 周期的 PD5 controls：`results.json` 与逐进程日志（含每条 `PD5_SAMPLE`） |
| `load/` | 预热后 9 个新进程的 `cuexis_player --smoke-test`：每进程的 load 读数、golden 行与日志 |
| `seek/` | 2 个回放规模 × 6 进程 × 16 次 seek：`results.json` 与逐进程 `PD5_SEEK` 行 |
| `stages/` | D2/D3/snapshot/queue 的阶段归因：插桩树 3 进程 × 1200 帧的逐次数组（JSONL）与分位数汇总 |
| `tools/` | Seek 的预注册与两个驱动器 |

## 关键数值

- Pause：预热后最大 **16.754 ms**（判据 50 ms）；预热前为 174.966–184.929 ms。
- 初次 load：预热后 580.0–603.2 ms（判据 2000 ms，余量 3.3×）；预热前 394.8–424.9 ms。
- 适用 Seek：129 记录最差 941 µs、1024 记录最差 1012 µs（判据 20 ms）。
- CTest：663.41 s、rc=0、1 项跳过；player smoke golden `Frame 5 summary=18316288860163381829`。

## 边界

- test-only；不构成 R3 技术接受、设备或生产接受。
- 预热把一次性 swap 停顿搬进 load，不消除它。
- Seek 只覆盖"同档重复热使用"与"冷进程"两类。
