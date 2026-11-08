# R4 普通 host 固定组（B-only）与第三次环境停止

状态：candidate；attempt-1 在第三窗口前因前台变化停止，逐进程绝对门保留，无整组裁定

日期：2026-11-08

返回 [New RPA 报告索引](../README.md)。当前结论只以
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md) 为准；本文保存 `p0-host-common`
单元的登记、执行、停止现场与只读复算。原始字节见
[本轮证据](2026-11-08-host-unit-evidence/index.md)。

## 1. 为什么是 B-only

pre-PD A 的 host harness 测的是 **request→receipt ACK** 边界，并输出另一套帧 digest；
`measurement.require_comparable` 只允许跨 schema 比较 `request_to_ready`，仍要求输出身份
（digest/source/viewport/objects/warmup/measured/audio/gpu）逐字段一致。A 与 B 的帧内容
不同，因此不满足“精确输出等价”，本单元按 `B-only` 登记：A 记 N/A，不填 0、不造比值。
这与 [PD-5 提案](../../../../../stage_plans/active/stage-07/04-pd/pd-5/pd5-measurement-proposal.md#2-工作负载与不可混用项)中
“旧 host 的 request→receipt ACK 与 B 的 request→present-return 不同，禁止直接计算收益”
一致，也解释了为什么本单元只能用绝对门裁定。

## 2. 工作负载与已接受预算

| 项 | 值 |
| --- | --- |
| 载体 | `cuexis_pd_host_probe` 的 “PD host delivery and receipt acknowledgement baseline” |
| 内容 | `assets/projects/stage3_project`，2 objects，1280×720，无 audio、无 GPU、TestPresentationRenderer |
| 每进程帧数 | 1,024 measured（探针自带的有限扩展批 `CUEXIS_PD_HOST_DIAGNOSTIC_1024=1`） |
| 原生预热 | 32 帧；每进程另加 30 秒机器热身 |
| 比较 | B-only（见 §1） |
| 已接受绝对预算 | request→ready p99 ≤ 4.167 ms（`LIMITS_US['request_to_ready']`） |
| 仅披露 | ready→present-return、request→present-return、present-call→return |

选 1,024 帧而不是默认 512 帧的原因是可判性：冻结的 `budget()` 要求 n≥1000 才给 p99，
512 帧只会得到 `insufficient_samples`，无法触及已接受的绝对预算。1,024 是探针自身声明的
有限扩展批，内容、对象数与视图都不变；这不是新增负载，也不是事后延长采样。

## 3. 登记与执行

`prepare_host.py` 先做一次单进程 pilot：核对 measured=1024、objects=2、audio/gpu=false、
closed=true、1024 个样本，并固定确定性输出 digest
`2053032455835124554`；`formalize.py` 一次性 claim/seal `p0-host-common-release` 的
attempt-1；`runner.py` 按三窗口×每窗三进程、窗间 600 秒执行。运行期间同一补救工具
（`remedy-run-hold-h1.json`）持有显示并逐秒记录环境。

| 阶段 | 时间（UTC） | 结果 |
| --- | --- | --- |
| 窗口 1 | 10:22:48–10:24:06 | 3/3 进程完成 |
| 窗口 2 | 10:34:45–10:36:03 | 3/3 进程完成 |
| 窗口 3 | 10:46 起 | 第 1 对热身阶段停止 |

## 4. 结果

六个完成进程的 `request→ready` 逐进程 p99（只读复算，见
`absolute-gate-recompute.json`）：

| 窗口/对 | p99（µs） | 预算（µs） | 状态 |
| --- | ---: | ---: | --- |
| 1/1 | 89.8 | 4166.7 | pass |
| 1/2 | 92.8 | 4166.7 | pass |
| 1/3 | 98.2 | 4166.7 | pass |
| 2/1 | 93.1 | 4166.7 | pass |
| 2/2 | 90.7 | 4166.7 | pass |
| 2/3 | 87.4 | 4166.7 | pass |

B-only 计时组的两窗口披露（不足以形成整组裁定）：

| 指标 | 窗口 1 三进程中位（µs） | 窗口 2 三进程中位（µs） |
| --- | --- | --- |
| request→ready | 42.00, 43.35, 43.20 | 41.05, 42.85, 42.05 |
| ready→present-return | 4.60, 4.20, 4.25 | 3.70, 4.60, 4.10 |
| request→present-return | 47.65, 49.15, 48.40 | 46.30, 48.40, 47.65 |

`ready→present-return` 只有约 4 µs，相对波动超过 15%，即使三窗口齐备也只会得到
inconclusive；这不影响逐进程绝对门，也不能被读成通过。整组因缺第三窗口没有
`repeatability()` 门，也没有 `verdict()` 结论。

## 5. 第三次环境停止

第三窗口第 1 对热身阶段的 `RuntimeMonitor` 记录前台进程在 24,212 与
9,456（`explorer`）、19,352、23,088 等多个进程之间变化，同一分钟内出现 7 个不同前台 pid；
交流供电、电源方案与构建进程集不变。运行期补救记录同一分钟由 pass 变为 failed。
这与 attempt-1 的**锁屏**不同，也与 attempt-2 的单次切窗不同：这是一段持续的人机交互，
任何 20–30 分钟的固定组都会在类似的桌面上被切断。

三次连续停止（两次探针组、一次 host）都指向同一结构问题：冻结的
`sampled-windows-environment-v1` 把“前台进程恒定”当作采样有效性的必要条件，而在这台
正在被使用的机器上，该条件与正常工作方式冲突。代理不自行改判据，也不再把剩余额度
消耗在同一种环境上。

## 6. 工具缺口与修复

整组停止时，原 `runner.py` 只把每进程中位写入 `stopped.json`，已经算出的
`absolute_gates` 被丢弃。本轮把 `partial_absolute_gates` 一并写入停止记录，使“已判定的
逐进程绝对门”在失败现场仍可核对；本次六条 p99 另用只读复算补齐（不启动任何负载），
两者一致。该修复不改变判据、阈值或统计函数。

## 7. 边界与待裁决

- 本单元只覆盖无 GPU、无 audio 的普通 host 路径；不代替 P1–P7、五类 Seek、设备、
  分域内存、P8、F1/F2 或生产接受。
- `p0-host-common-release` 的 attempt-2 仍可用，但继续执行需要 owner 先决定
  [探针批报告 §9](2026-11-08-probe-matrix-admission-and-environment-stop.md#9-当前阻断与待-owner-裁定的方向)
  中的方向；其中“只改 CPU-only 单元的环境判据”同样适用于本单元。
- 已完成的六个进程是**披露与逐进程门**，不是整组裁定；没有第三窗口就没有重复性结论。

## 8. 证据

| 内容 | 位置 |
| --- | --- |
| 候选构建与两个目标的 build binding | `out/evidence/r4/host-20261108/build-release-h1` |
| pilot、三份 proof 与 preparation | `out/evidence/r4/host-20261108/host-pilot-v1` |
| 固定组现场与停止记录 | `out/evidence/r4/host-20261108/host-formal-run1` |
| 逐进程绝对门只读复算 | `out/evidence/r4/host-20261108/host-formal-run1/absolute-gate-recompute.json` |
| 运行期补救持有记录 | `out/evidence/r4/host-20261108/remedy-run-hold-h1.json` |
| 附件索引与只读复核 | [本轮证据](2026-11-08-host-unit-evidence/index.md) |
