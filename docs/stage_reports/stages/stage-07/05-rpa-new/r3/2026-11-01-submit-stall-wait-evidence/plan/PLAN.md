# GPU 份额诊断 —— 预注册计划（在测量之前写成）

对象：`docs/stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-01-r3-capacity-frontier-bracket.md`
记录的那个 `backend.submit` 内部停顿。前缘梯子（无 GPU 计时器）显示：停顿在 2300 static
起变成 4–15% 的帧，单次多花 2.3–3.6 ms，且慢帧的 `submit_us` 增长而 `present_us` 不变；
40K 时 GPU 区间 16.3 ms 只是上限读数。前缘报告没有回答"这 3 ms 是等 GPU，还是 CPU/驱动侧的
阻塞"。在动手切分 `submit` 之前，先用现成的 GPU 计时器把**每帧 GPU 工作量**量出来，因为
这两个假设的后续工作完全不同：

- 若 GPU 每帧时间在前缘处已经逼近 CPU 每帧时间，则前缘是 GPU 绘制调用吞吐的边界，
  停顿是背面压力（backpressure）的表现，杠杆是批处理/实例化，而不是 CPU 微优化。
- 若 GPU 每帧时间在前缘处仍远低于 CPU 时间，则 GPU 空闲，停顿只能是驱动或 CPU 侧
  （缓冲区映射/flush/分配器/page fault），杠杆在 submit 内部。

## 仪器

`out/verification-20261031/r3-40k-throughput-refresh-v1/run_stall_diagnosis.py`
（`30a34162…`），即前缘梯子 runner（`16a3f57a…`）的 GPU 计时器变体，四处差异已在文件头
写明。它是**独立仪器**，不是前缘梯子，也不是验收。probe 二进制
`cuexis_execution_device_probe.exe` = `daa0c3cf370c74a408073eac35334a4e4dd81c113f80928fcfd9744a23d6aca8`，
在本次工作树（head `04b6fa3`）上 `ninja: no work to do` 复核过，未重建。

## 分组（全部 1920×1080，每进程 20 s，每组 3 个新鲜进程）

| 目录 | entities | 模式 |
|---|---|---|
| `n1800` | 1800 | render-only, static |
| `n2400` | 2400 | render-only, static |
| `n2800` | 2800 | render-only, static |

选 1800 是因为它在前缘梯子上**没有**停顿（0/0/0），2400 static 是停顿最密处（11.4–12.0%），
2800 是两侧都越界的点。三个点足以给出 GPU 时间对 N 的斜率与截距。

## 预注册预测（先写下来，避免事后编故事）

1. **P1**：每帧 GPU 区间随 N 单调上升；1800 时中位数 ≲ 0.8 ms，2800 时 ≳ 2.2 ms。
   （若 GPU 时间在 2800 仍 ≲ 1.2 ms，则"前缘是 GPU 吞吐边界"被证伪。）
2. **P2**：慢帧（`cycle_us > 6000`）的 GPU 区间不超过同一进程快帧中位数的 +10%。
   即停顿不是 GPU 执行时间的尖峰。（若慢帧 GPU 区间显著抬高，则背面压力假设得到支持。）
3. **P3**：2400 static 的慢帧比例可复现 11–12%（即在 CPU 侧多一个每帧查询后，现象仍在）。
4. **P4**：1800 static 的慢帧比例仍 ≲ 0.2%（前缘以下的稀有事件级别）。

## 判定

- 本诊断只回答"停顿是等 GPU 还是不等"。它不改任何容量结论：注册容量仍是冻结的
  1800 render-only / 1300 static；前缘仍是区间 `[2400, 2600)` / `[2200, 2300)`。
- 计时器每帧多一个查询，本身就是额外同步点，因此本组数据不能与前缘梯子的 FPS/p99 交叉比较，
  只能在同一组内做慢帧/快帧对照与跨 N 的 GPU 时间斜率。
- 若 P2 成立（GPU 空闲），下一步才是切分 `backend.submit`；若 P2 被推翻（GPU 尖峰），
  下一步是量绘制调用本身的 GPU 成本，杠杆换成批处理。
