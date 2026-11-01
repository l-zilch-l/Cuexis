# R3 第二批正式门测量附件（2026-11-01）

本目录保存第二批门的全部字节：预注册、驱动器、6 个进程的原始日志与派生结果，以及一次作废尝试。
仪器是仓库自带的 PD5 脚本化 host 探针，编译进 `cuexis_execution_device_probe`（`CUEXIS_PD5_DEVICE`），
所以渲染器是真实 OpenGL 后端、GPU render 走 timer query，而不是 CPU 测试渲染器。

- [返回本批报告](../2026-11-01-r3-second-gate-batch.md)

## 分组与证明范围

| 目录 | 内容 | 证明什么 | 明确不支撑 |
| --- | --- | --- | --- |
| `controls/` | 3 个新进程 × 20 个控制周期 | pause/resume/reset_paused/reset_running/resize/close 每一次的 命令→状态确认→可用画面 present-return | 不支撑 D2/D3/Snapshot/queue，也不支撑 seek |
| `stream/` | 3 个新进程 × 1200 帧（预热 64） | 每进程 `request_to_ready` 与 `gpu_render` 的 p99；同时披露 `present_call`、`cpu_submit`、`request_to_present_return` | present/VSync 不是门，不能替代 request→ready 或 GPU render |
| `controls.aborted-1/` | 第一次尝试第一个进程的日志 | 记录驱动器自身的解析缺陷（局部 `statistics()` 遮蔽模块）导致的作废；读数从未被打印或记录 | 不作任何读数使用 |
| `tools/` | 预注册与驱动器 | 判据、边界、统计口径在测量前写定，含两条补记 | 不是新的接受判据 |

## 关键事实

- 每个进程恰好 40 条 pause、40 条 resume、20 条 `reset_paused`、20 条 `reset_running`、20 条 resize、
  1 条 close；`controls/` 的 valid 判定就是按这个计数。
- `stream/` 每个进程五类时序指标各 1200 条；`PD5_ID` 报 `gpu_timer:true`、`frames:1200`、`warmup_frames:64`。
- 控制类的判定用窗口内全部预声明操作的最大值（计划 §9.2/§5 的口径），不是分位数；时间类的判定用每进程
  p99（n=1200，满足计划的 n≥1000 要求）。

## 读取顺序

1. `tools/preregistration.json`：判据、边界、机器状态与两条补记。
2. `results.json`：每组的环境变量、逐进程统计与 `verdicts`。
3. `controls/run-*.log.txt` 与 `stream/run-*.log.txt`：逐条 `PD5_SAMPLE`，可重算任何分位数。
4. `manifest.json`：每个归档文件的源路径、字节数与两侧哈希。

## 边界

- 全部 test-only：不构成 R3 技术接受、设备验证或生产接受。
- 窗口在 `CUEXIS_PD5_SMOKE=1` 下隐藏；`glConfig.vsync = true`，因此 `present_call` 中出现的毫秒级停顿是
  VSync 等待，按计划单列，不并入任何门。
- 只在 1080p/1280×720 视口、本机、本构建与本次会话成立。
