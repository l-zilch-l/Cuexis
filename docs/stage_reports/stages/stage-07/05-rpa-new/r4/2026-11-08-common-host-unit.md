# R4 普通 host 固定组（B-only）：已接受绝对预算通过

状态：candidate；attempt-2 三窗口九进程完成，request→ready p99 预算 9/9 通过；一条披露腿重复性 inconclusive

日期：2026-11-08

返回 [New RPA 报告索引](../README.md)。当前结论只以
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md) 为准；本文保存 `p0-host-common`
单元的两次登记、执行与裁定。原始字节见
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
| 输出身份 | 确定性帧 digest `2053032455835124554`（pilot 固定，正式进程逐个复现） |
| 比较 | B-only（见 §1） |
| 已接受绝对预算 | request→ready p99 ≤ 4.167 ms（`LIMITS_US['request_to_ready']`） |
| 仅披露 | ready→present-return、request→present-return、present-call→return |

选 1,024 帧而不是默认 512 帧的原因是可判性：冻结的 `budget()` 要求 n≥1000 才给 p99，
512 帧只会得到 `insufficient_samples`，无法触及已接受的绝对预算。1,024 是探针自身声明的
有限扩展批，内容、对象数与视图都不变；这不是新增负载，也不是事后延长采样。

## 3. 两次登记

| 项 | attempt-1 | attempt-2 |
| --- | --- | --- |
| 候选 | `f1c4d22e` | `f1c4d22e`（未改候选；仅新增文档与停止记录修复） |
| pilot | `host-pilot-v1`，digest 与身份固定 | 复用同一 pilot 的 proof、环境与 binary binding，不重跑 pilot |
| 处置 | 无（首次登记） | `rpa.execution.retry-disposition.v1` 绑定 attempt-1 的 claim sha 与已验证补救 `remedy-verify-v2.json` |
| 执行 | 窗口 1、2 完成，窗口 3 前停止 | 三窗口九进程全部完成 |
| 现场 | `host-formal-run1` | `host-formal-run2` |

attempt-1 的停止原因是第三窗口前前台在 24,212 与 9,456/19,352/23,088 等进程间持续变化
（桌面被使用），不是产品、身份或性能问题；其 `stopped.json` 与 6 个完成进程原样保留。

attempt-2 期间后台补救记录 `remedy-run-hold-h2.json` 逐秒采样 1,985 次、8 次空闲重置，
只在第 1,644 秒（两个窗口之间的 600 秒间隔内）看到一次前台切到 20,116、16 秒后切回，
没有锁定、没有构建冲突。九个**测量进程**各自的 `*-runtime-environment.json` 全部
`passed`；也就是说这次切窗落在间隔里，未污染任何样本。补救记录本身按连续采样判为
failed，这一差异在 §5 说明。

## 4. 裁定

### 4.1 已接受绝对预算

`request→ready` 逐进程 p99（每进程 1,024 样本）：

| 窗口/对 | p99（µs） | 预算（µs） | 状态 |
| --- | ---: | ---: | --- |
| 1/1 | 92.2 | 4166.7 | pass |
| 1/2 | 97.4 | 4166.7 | pass |
| 1/3 | 95.4 | 4166.7 | pass |
| 2/1 | 92.1 | 4166.7 | pass |
| 2/2 | 87.9 | 4166.7 | pass |
| 2/3 | 89.9 | 4166.7 | pass |
| 3/1 | 99.1 | 4166.7 | pass |
| 3/2 | 91.8 | 4166.7 | pass |
| 3/3 | 91.9 | 4166.7 | pass |

九个进程全部通过已接受预算，最坏 p99 为 99.1 µs，仅为预算的 2.4%。按 R4 §9.2 的
“每个进程仍分别披露预算合格性”，本单元的绝对门是 **pass**；这不等同于整批 R4 通过。

### 4.2 计时组重复性

| 指标 | 逐窗极差/中位 | 前六 | 全九 | 状态 |
| --- | --- | --- | --- | --- |
| request→ready | 2.8% / 5.2% / 5.5% | 5.2% | 7.0% | pass |
| request→present-return | 5.0% / 5.5% / 6.0% | 6.9% | 9.4% | pass |
| ready→present-return | 21.6% / 12.2% / 4.8% | 22.4% | 22.6% | **inconclusive** |

前两条腿的重复性通过；`ready→present-return` 的每进程中位只有 3.75–4.7 µs，已接近
微秒计时分辨率，第一窗口与前六/全九都超过 15%，因此该腿按冻结规则为 inconclusive。
这是**已知的口径限制**，不是产品回归：它不改变 §4.1 的逐进程绝对门，也不能被读成通过。

两窗口披露的每进程中位（µs）：

| 窗口 | request→ready | ready→present-return | request→present-return |
| --- | --- | --- | --- |
| 1 | 43.50, 42.30, 42.80 | 4.70, 4.40, 3.75 | 50.00, 47.60, 47.80 |
| 2 | 43.70, 43.75, 41.50 | 4.50, 4.00, 4.10 | 48.95, 49.35, 46.65 |
| 3 | 40.75, 43.00, 41.25 | 4.10, 4.30, 4.20 | 45.50, 48.30, 46.50 |

### 4.3 结果文件里的两个字段

`runner.py` 给每个 B-only 单元写 `scope: relative_cost_and_repeatability_only` 与
`overall_acceptance: requires_all_registered_absolute_and_other_gates`，这两句是冻结输出的
固定文本。对本单元的正确读法是：**没有相对比值**（B-only，A 为 N/A）；已登记的绝对预算
按 §4.1 逐进程通过；其他门（GPU/audio/设备/F1/F2）不在本单元范围。

## 5. 工具缺口与修复

- 整组停止时，原 `runner.py` 只把每进程中位写入 `stopped.json`，已算出的 `absolute_gates`
  被丢弃。本轮把 `partial_absolute_gates` 一并写入停止记录；attempt-1 的六条 p99 另用
  只读复算补齐（`absolute-gate-recompute.json`，不启动任何负载），两者结论一致。
- 补救工具按“连续采样”判定环境：它把 600 秒间隔内的切窗也算作失败，而正式判据只看
  测量进程内的 `RuntimeMonitor`。两者标准不同，本次记录并列披露，不互相替代。

## 6. 边界

- 本单元只覆盖无 GPU、无 audio 的普通 host 路径的 request→ready 绝对预算与两条披露腿的
  重复性；不代替 P1–P7、五类 Seek、设备、分域内存、P8、F1/F2 或生产接受。
- `ready→present-return` 的 inconclusive 保留，不通过缩短采样、换窗口或删除该腿消解。
- 该单元额度已用尽（attempt-1、attempt-2），不再重开。

## 7. 证据

| 内容 | 位置 |
| --- | --- |
| 候选构建与两个目标的 build binding | `out/evidence/r4/host-20261108/build-release-h1` |
| attempt-1 pilot、proof 与 preparation | `out/evidence/r4/host-20261108/host-pilot-v1` |
| attempt-1 现场与停止记录 | `out/evidence/r4/host-20261108/host-formal-run1` |
| attempt-1 逐进程绝对门只读复算 | `out/evidence/r4/host-20261108/host-formal-run1/absolute-gate-recompute.json` |
| attempt-2 retry disposition 与 preparation | `out/evidence/r4/host-20261108/host-pilot-v2` |
| attempt-2 三窗口九进程与 `result.json` | `out/evidence/r4/host-20261108/host-formal-run2` |
| 两次运行期补救记录 | `out/evidence/r4/host-20261108/remedy-run-hold-h1.json`、`remedy-run-hold-h2.json` |
| 附件索引与只读复核 | [本轮证据](2026-11-08-host-unit-evidence/index.md) |
