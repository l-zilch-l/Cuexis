# `read_long` 正式三窗九对组（2026-11-12）

状态：formal result；`legacy` 通过，`RPA` 因重复性为 inconclusive

日期：2026-11-12

返回 [相机逆矩阵整改](2026-11-11-read-long-camera-inverse.md)与
[owner 裁定记录](2026-11-11-owner-decisions.md) §4。

## 1. 这一组怎么跑起来的

冻结的 pre-PD 基线目录（`out/pd0-fresh-bound-baseline-20261018`）在本机不存在，`owner_runner`
无法取得 A 侧，因此这一组用的是**重建 A + 重新绑定**：由 `out/sources/r4/prepd-original`
（`d3440882`）在同一工具链、同一 Release 选项下重建 `cuexis_pd_baseline_probe`。两份 binding 的
`toolchain_signature`、`options`、`config` 逐字段相同（`config=Release`、`CUEXIS_LIBRARY_TYPE=STATIC`）。

控制器不是 `owner_runner`，而是 `out/evidence/r4/readlong/run-read-long-formal.py`；协议照旧：
一次机器热身、四个 pilot 进程、三窗口、每窗三对、`AB/BA/AB` 与 `BA/AB/BA` 交替、`read_long`
每块 1,048,576 次读、每进程 8 块、窗口间隔 ≥600 秒、不替换任何进程。登记文件在证据目录的
`registration.json`，其中写明 `controller` 不是 `owner_runner`、"frozen A absent"。

## 2. 结果

每对取进程内 8 块的中位数，比值 = B/A；组值是九对比值的几何平均，区间是 20,000 次 bootstrap。

| 单元 | 九对比值几何平均 | 95% 区间 | A 侧波动 | B 侧波动 | 判定 |
| --- | ---: | --- | ---: | ---: | --- |
| legacy | **0.91913** | [0.90916, 0.93081] | 11.62% | 9.44% | **pass** |
| RPA | 0.92582 | [0.90748, 0.94914] | 9.34% | 19.25% | **inconclusive**（重复性） |

两个单元的区间上界都远低于 1.10。`legacy` 的 A/B 波动都在 15% 门内，判 pass；`RPA` 的 B 侧
19.25% 超出 15%，按规则只能记 inconclusive。

## 3. RPA 的重复性来自一个进程

九个 B 侧读数里八个落在 0.1994–0.2080，只有一个例外：

| 窗口/对 | 顺序 | A（µs/读） | B（µs/读） | 比值 |
| --- | --- | ---: | ---: | ---: |
| w1 pair2 | BA | 0.23662 | **0.23808** | 1.00617 |
| 其余八对 | — | 0.2155–0.2366 | 0.1994–0.2172 | 0.876–0.931 |

同一次 BA 配对里 A 也偏高（0.23662，而其余 A 多在 0.216 附近），说明那一对期间机器整体变慢，
两侧都被抬高，B 侧恰好抬得更多。这不是 B 侧的系统性回归——其余八对的一致性是 0.88–0.93。

## 4. 能声称与不能声称

**能声称**：在重建 A 的绑定下，两个单元的相对成本都比 pre-PD 低约 8%，且区间上界低于 1.10；
`legacy` 同时满足 A/B 重复性，是一个完整通过的正式单元。

**不能声称**：`RPA` 单元不是通过的正式结果——它只差重复性这一项。也不能把这组结果与
2026-11-07 那次（A 为原冻结二进制）直接合并统计：两次的 A 侧来源不同（原冻结 vs 由源码重建）。

## 5. 需要 owner 裁定的两件事

1. **RPA 单元怎么办**：接受"点估计与区间都远低于 1.10、仅一个受环境干扰的进程使重复性超界"的
   证据，还是再批一次只跑 RPA 的正式组（12 个进程，约 25 分钟，不含窗口间隔）？
2. **A 侧口径**：本组用重建 A 并重新绑定。若接受，这组数值可以作为 R4 的 `read_long` 结论；
   若不接受，需要恢复原冻结目录后用同一协议重跑。

## 6. 证据

`out/evidence/r4/readlong/formal-20261112-063933/`：`registration.json`、`pilot.json`、
每窗口每对每侧的 `process.json` 与原始日志、`results.json`、`summary.json`。
机器条件未做亲和性绑定时 `cpu_capture` 会记录错误原因；本次 `affinity_mask` 已写入登记。
