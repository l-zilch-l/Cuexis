# R3 修复尝试证据（2026-11-01）

本目录保存一次**没有留下代码改动**的修复尝试的全部字节。它回答两个问题：读取路径的逐对象成本是否
来自"每对象构造 `Result` + 重复验证读窗口"（否），以及首次 Pause 的 179 ms 落在哪个阶段（落在第一次
冻结绘制）。

- [返回修复尝试报告](../2026-11-01-r3-gate-repair-attempt.md)

## 文件

| 文件 | 内容 |
| --- | --- |
| `attempt/comparison.json` | 补丁二进制与原二进制的三对交替比较：六个读数、三个比值、几何平均 1.0175、结论"假设被证伪、改动已回滚"；含原二进制 sha256 与补丁要点 |
| `attempt/scale-gap.json` | 50/200/800/1800 四个实体数上的两臂差，用来判断差距是逐对象还是每次调用 |
| `attempt/pause-split.json` | 控制组 run-00 首次 pause 的三段时刻：`pause()` 258 µs、冻结绘制 178 445 µs、总 178 703 µs |
| `attempt/build.log` | 补丁的增量构建记录 |

## 边界

- 这里的比较是**同一版本内的补丁对照**，不参与任何正式门读数，也不与 pre-PD 臂的比值相加或相除。
- 原二进制 `bin/baseline-B.exe`（5.7 MiB）留在工作目录、不入库；它的 sha256 记在 `comparison.json` 与
  `manifest.json` 里。
- 补丁本身没有保留成 patch 文件：改动已回滚，报告里逐条写明改了什么（`objectCount()` +
  `readSnapshotObjectInto`，`snapshotReadScratchBytes` 256 → 136）。这是本目录的已知缺口。
