# P5 脚本化控制单元：登记、适配器与 pilot

状态：candidate evidence；一个通过的 pilot，不含正式组，不构成接受裁定

日期：2026-11-11

返回 [owner 裁定记录](2026-11-11-owner-decisions.md)与 [New RPA 报告索引](../README.md)。

## 1. 单元在做什么

`p5-controls` 在同一个进程里按声明的周期数执行脚本化控制：每个周期 2 次 Pause、2 次 Resume、
1 次"暂停中 Reset"、1 次"运行中 Reset"、1 次 Resize；全部周期结束后 1 次 Close。每次控制都记录
命令发出、确认返回和其后第一帧呈现三个时刻。

**这个夹具里的 Reset 不是控制 API**：当前产品没有 Reset 控制入口，探针用"关闭 + 重建宿主 + 重新加载"
表达它，因此 `reset_*` 读数是重建成本，不是一次控制往返；Resize 只到渲染器，不过会话。两条限制写在
记录里，避免读数被当成能力声明。

## 2. 登记形状

- 计数全部由周期数推导（10 周期）：`pause` 20、`resume` 20、`reset_paused` 10、`reset_running` 10、
  `resize` 10、`load_host` 1、`close` 1，合计 72 条样本；
- 单元用独立适配器 `controls`，不复用展示矩阵的 15 键身份——矩阵身份要求 9 个纯展示字段
  （`updates`/`materializations`/`capture_consumers` 等），控制负载没有对应物，伪造它们只会让记录失真；
- 运行器只为 `controls` 增加一条分支：绑定 `p5-controls` 门、用 `parse_probe` 解析、要求
  `mode`/`cycles`/`rate`/`counts` 四个身份键。阈值、计数、比较规则均未改动。

## 3. pilot 结果（2026-11-11，`pilot-3`）

- 退出码 0，344 条断言通过；`PD5_ID` 与声明逐字段一致（`mode=controls`、`cycles=10`、`rate=240`、
  `frames=512`、`warmup_frames=32`、`audio=false`、`closed=true`、`replay_equal=true`、计数合计 72）。
- `admission.validate_preparation` 接受了记录，`preparation.json` 写入，含三份 proof 与密封源身份。

单进程观测（不构成接受结论）：

| 指标 | n | 中位 | 最大 |
| --- | ---: | ---: | ---: |
| `pause` | 20 | 194.5 µs | 297 µs |
| `resume` | 20 | 323.5 µs | 447 µs |
| `resize` | 10 | 152.0 µs | 217 µs |
| `reset_paused` | 10 | 3,158.5 µs | 3,438 µs |
| `reset_running` | 10 | 2,935.5 µs | 3,264 µs |
| `load_host` / `close` | 1 / 1 | 4,008 µs / 570 µs | — |

## 4. 两次环境停止与结论

前两次 pilot 都停在冻结的 `sampled-windows-environment-v1` 策略上：第一次是机器预热阶段，第二次是
sample 阶段（31 个 warmup 采样里前景 pid 一直是 8428，sample 的 2 个采样里变成 3384）。

这不是夹具问题：该构建下探针不创建窗口（渲染器是 `TestPresentationRenderer`，SDL 窗口只在
`CUEXIS_PD5_DEVICE` 下创建），被计时区间也只含控制调用与呈现。改成"夹具"既不降低这项成本，也不消除
对安静前台的要求；owner 因此裁定保留探针形态（见[裁定记录](2026-11-11-owner-decisions.md) §1）。
第三次在安静窗口重试即通过。

## 5. 边界

- 只有一次 pilot；没有三窗口九进程的正式组，因此没有任何重复性或预算结论。
- 未覆盖 GPU 计时、音频设备、设备侧 Resize、A 侧对照；Reset 的重建语义见 §1。
- pilot 证据在 `out/evidence/r4/controls3/pilot-3/`（含 `stopped` 前两次运行）。
