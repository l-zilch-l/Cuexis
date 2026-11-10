# 1080p 容量前缘阶梯证据（2026-11-01）

本目录保存一次**预注册**的 1080p p99 预算交叉阶梯的全部字节：预注册、驱动与汇总脚本、每个实体数
两种模式的逐进程日志、逐点登记，以及判定结果。它回答的问题只有一个——HEAD 上 p99 第一次越过
4.166667 ms 的区间在哪里；它**不是**容量接受、性能门通过或 R3/R4 接受。

## 文件与证明范围

| 文件 | 证明什么 |
|---|---|
| `manifest.json` | 本归档自己：每条复制文件的源路径、字节数、归档哈希与源哈希（文本按 CRLF 归一），绑定 head、探针二进制哈希、点数、预注册哈希与判定状态 |
| `tools/frontier-bracket-preregistration.json` | 测量前写定的问题、阶梯、执行顺序、通过判据与两个预测模型；判定不得越过它 |
| `tools/run_frontier_bracket.py` | 按注册顺序逐点驱动测量、拒绝已存在目录、二进制哈希变化即中止 |
| `tools/run_40k_refresh.py` | 真正执行测量的运行器（进程数、秒数、预热、亲和、通过规则都在它里面） |
| `tools/summarize_frontier.py` | 只读预注册与实测，产出区间、单调性与锚点漂移判定 |
| `entities-<N>/registration.json` | 该点的 head、工作树、探针源码哈希、运行器哈希、二进制哈希与计划 |
| `entities-<N>/NN-<mode>-<N>.log.txt` | 该进程的原始逐帧日志（预算、帧数、每秒窗口、`over_budget`） |
| `entities-<N>/results.json` | 该点的逐进程统计与分组中位数 |
| `frontier-bracket-summary.json` | 阶梯级汇总：已完成/未完成的实体数、每点两种模式的中位数与相对范围（归档路径 `derived/frontier-bracket-summary.json`） |
| `frontier-bracket-verdict.json` | 判定：区间状态、单调性、锚点漂移结论与报告用表格（归档路径 `derived/frontier-bracket-verdict.json`） |
| `tools/analyze_stall_shape.py` | 只重读已录日志的“慢帧形状”分析（阈值 6 ms 与预算 4.166667 ms 写在脚本里），不产生新测量 |
| `tools/run_stall_diagnosis.py` | 附加诊断的运行器：与阶梯运行器只差四处（开 GL 时间戳等），哈希因此不同 |
| `diagnosis/runs/entities-<N>/` | 该诊断点的登记、结果与逐进程日志（2300、2800 各 3 进程 × 20 s，40000 为 2 进程 × 120 帧） |
| `diagnosis/shapes/*.json` | 慢帧形状：逐进程的慢帧占比、连片长度、间隔分布与快/慢帧相位中位数 |
| `index.md` | 本文件（生成，不入 `manifest.json`） |

## 附加诊断（不是阶梯的一部分）

阶梯只测每帧的 CPU 墙钟时间，说不清时间花在哪里。归档在 `diagnosis/` 下的两件事补上这一点，并且
**故意与阶梯的记录分开**：

- `diagnosis/shapes/` 是**零新测量**的重读：脚本把同一批逐帧日志按 `cycle_us > 6000` 划成慢帧，
  报告它们的占比、连片长度、间隔分布，以及慢帧与快帧各自的 `update/extract/submit/present` 中位数。
  它证明“越界是约 7.15 ms 的孤立停顿、间隔中位数 4 帧、只落在 `submit` 里”，不改任何点的判定。
- `diagnosis/runs/` 是**另一台仪器**的独立测量：`run_stall_diagnosis.py` 打开探针的
  `CUEXIS_GPU_TIMER`，在 `backend.submit` 前后各写一个 GL 时间戳，于是同一帧能同时看到“CPU 在这段花了
  多久”与“GPU 自己的间隔”。它的登记里 `timer` 为 `true`，运行器哈希与阶梯不同，`manifest.json` 的
  `diagnosis.ladder_runner_sha256` / `diagnosis.runner_sha256` 把两者的差异固定下来。

这两部分的数字只作为标注过的诊断引用：它们不进入任何阶梯点的通过/失败判定，不改变区间端点，也不构成
对冻结 40K 判定的修正（40K 那个点是 2 进程 × 120 帧，只用于读 GPU 间隔的上界）。

## 主张与支撑

- “p99 在 X 与 Y 之间第一次越过预算”只由 `frontier-bracket-verdict.json` 的状态与
  `frontier-bracket-summary.json` 的逐点中位数支撑；本索引不给结论。
- 每点的通过/失败只按该点 `registration.json` 里登记的计划与运行器的通过规则判定，不看四舍五入后的
  报告数字。
- 阶梯“同一次构建”只由每点 `registration.json` 的 `binary_sha256` 是否一致支撑；`manifest.json`
  的 `binding.probe_binary_sha256` 记录被核对的那个值。
- 判定为“无区间”（不单调、锚点漂移超带或没有通过点）时，该结论本身也是本归档的结果，不得改写成
  某个更整齐的区间。

## 作废的前序尝试

工作目录（`frontier-bracket-4`）里的文件全部被复制，没有从数据里删掉任何一项。另外三次尝试没有产出
可用的阶梯，它们的处置是：

| 尝试 | 停在哪 | 原因 | 归档位置 |
|---|---|---|---|
| `frontier-bracket-1` | 起跑前 | 六个测试仪器文件尚未提交，逐进程守卫拒绝脏工作树；一个进程都没起 | 无文件（只剩空目录） |
| `frontier-bracket-2` | 第 2 点 | 运行器把“跑满 20 s 但低于登记速率”误判为“进程无效”，触发停止规则；该分类已修 | `attempts/frontier-bracket-2/` |
| `frontier-bracket-3` | 第 2 点 | 同一守卫拒绝：阶梯进行中有人修改了受版本控制的文档；编辑已回滚，整条阶梯重跑 | `attempts/frontier-bracket-3/` |

`manifest.json` 的 `superseded_attempts` 逐条记录每次的状态、逐字原因与它归档了哪些文件，`excluded`
为空是因为这次一个文件都没有被丢掉。前两次作废的尝试各自跑完了 1800 这个点，因此它们的文件整组保留，
但**只在“同一个点三次独立观测”的意义上被引用**（报告“边界”的重复性一条）；其余点不参与任何判定，
区间判定只读 `frontier-bracket-4`。这样报告里引用的每个 1800 中位数都能在本归档内重算。

## 读取顺序

1. `manifest.json` 的 `binding`：head、探针哈希、点数、预注册哈希、判定状态。
2. `tools/frontier-bracket-preregistration.json`：先看判据，再看数字。
3. `frontier-bracket-summary.json` 的逐点中位数与相对范围。
4. `frontier-bracket-verdict.json`：区间与单调性。
5. 需要复核某个点时，读该点的 `registration.json` 与逐进程日志。
6. 想复核“为什么会越界”时，先读 `diagnosis/shapes/*.json`（零新测量的重读），再读
   `diagnosis/runs/` 的登记与日志（另一台仪器的独立测量）。

## 边界

- 诊断证据：不替换冻结的容量下界（仅渲染 1800、静态 1300），不提升任何生产门限，也不构成
  R3/R4、设备或生产接受。
- 每点三个新进程、每个二十秒，只够支撑“该点在预算内/外”，不足以支撑点间差异的显著性结论。
- 只在 1080p、该探针二进制与该夹具上成立；换分辨率、夹具或构建即属另一次测量。
