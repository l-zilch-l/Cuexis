# 五类 Seek 正式组（2026-11-13）

状态：formal result；五类的 20 ms 绝对门全部通过，三类重复性通过、两类 inconclusive

日期：2026-11-13

返回 [R4 门账](2026-11-08-r4-gate-status.md)与 [`read_long` 裁定记录](2026-11-12-read-long-ruling.md)。

## 1. 五类怎么跑

每类单独准备、单独 claim、单独跑一组：三窗口、每窗三进程、窗口间隔 600 秒、每进程 30 秒机器热身，
`natural`/`hot`/`external`/`fallback` 每进程 16 次 seek、`cold` 每进程 1 次（新进程的首次 seek），
129 条记录，跑在 `cuexis_execution_device_probe`（RTX 4060 Laptop、OpenGL 后端、隐藏窗口，不需要
可见桌面）。每次请求都比对 oracle，逐字节相等才算有效样本——解析器会直接拒绝任何不一致的日志。

规模轴按**记录数**声明（129/1024，与 R3 实测口径一致），stable id 形如 `seek-natural-129-release`；
这条规则写进了 `admission.stable_id`，不再强套 64/2048/40000 的实体轴。

## 2. 结果

| 类 | `request_to_present` 全九波动 | `request_to_ready` 全九波动 | 单进程最大 seek | 判定 |
| --- | ---: | ---: | ---: | --- |
| natural | 8.02% 通过 | 5.34% 通过 | 777 µs | **通过** |
| hot | 7.63% 通过 | 9.09% 通过 | 752 µs | **通过** |
| external | 11.59% 通过 | 8.72% 通过 | 1,014 µs | **通过** |
| fallback | **20.27%** inconclusive | 11.50% 通过 | 981 µs | 绝对通过、重复性 inconclusive |
| cold | **39.92%** inconclusive | **35.92%** inconclusive | 665 µs | 绝对通过、重复性 inconclusive |

**绝对门**：五类的 20,000 µs 上限全部通过，最差一次 1,014 µs，余量约 19.7×；中位数 350–461 µs。
这一项不依赖重复性，可以单独声称。

**重复性**：`natural`、`hot`、`external` 三类在两侧、全九口径都低于 15%，是干净通过；
`fallback` 的 `request_to_present` 20.27%、`cold` 两项 39.92%/35.92% 超过 15%，按冻结规则记
inconclusive——不因绝对余量很大就改判。

## 3. `cold` 的波动是这一类本身的性质

`cold` 测的是新进程的第一次 seek：进程刚起、文件缓存未热、OpenGL 上下文首次建立。R3 的同类
登记也把它单列，正是因为它的成本由外部状态决定。本组 9 个进程的最大值 665 µs 反而低于其它类，
说明不是某一次异常拖慢，而是九个值分布在 461–665 µs 之间（跨度 40%）——这是首触成本的自然离散，
不是控制台干扰或环境异常（九个进程的环境采样全部 passed）。

`fallback` 的 20.27% 类似：它每次都要付完整的 origin 回放，回放的抖动直接进入读数。

## 4. 边界

- 只在 RTX 4060 Laptop + 当前驱动上测得；不含 240 Hz 显示链路、GPU timer、音频端点或实体操作。
- `cold` 没有做磁盘缓存控制，按登记口径不称 disk-cold。
- 两类 inconclusive 各自还剩一次登记额度（每单元默认两次，本轮各用一次）；是否重开由 owner 决定。
- 五类的 A 侧为 N/A（B-only），没有相对比值。

## 5. 工具改动（本轮为打通 Seek 所做）

| 改动 | 原因 |
| --- | --- |
| `measurement.parse_seek()` | 身份、请求数、oracle、退出码校验；6 项测试覆盖拒绝路径 |
| `prepare_seek.py` | 五类分别登记，各自带 semantic/pilot/environment 证明，`cold` 声明单请求与新进程 |
| `runner.py` 的 `seek` 分支 | 专用身份比较；`seek`（20 ms，max）与 `request_to_ready` 两条绝对门 |
| `pd5_host_probe.cpp` 的 `seek-end` 锚点 | seek 负载原本只有一个环境锚点，无法构成成对证据 |
| `formal_baseline.native_environment` | 接受带前缀的成对锚点（`seek-begin`/`seek-end`） |
| `admission.stable_id` 的 Seek 规模轴 | 记录轴 129/1024 才是这一类实际变化的规模 |

证据：`out/evidence/r4/seek/run-{natural,hot,external,fallback,cold}/`，
每进程 `sample.log` 与 `parsed.json`，以及各自 preparation 的 pilot 记录。
