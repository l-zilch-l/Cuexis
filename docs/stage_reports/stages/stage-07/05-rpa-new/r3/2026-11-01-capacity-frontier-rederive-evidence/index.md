# 1080p 容量前缘重新推导证据（2026-11-01）

本目录保存**同一份源码**上重新推导 240 FPS 前缘的全部字节：预注册、登记补记、驱动与汇总脚本、
十个实体数两种模式的逐进程日志、逐点登记，以及判定结果。它回答的问题只有一个——HEAD 上 p99 第一次
越过 4.166667 ms 的区间在哪里；它**不是**容量接受、性能门通过或 R3/R4 接受。

它与 2026-10-31 的冻结阶梯是两次独立测量：那次是 `8e275a5`、探针 `daa0c3cf…`；这次是
`6256af8` 的实现（登记时）与 `a93c979` 的工作树（实际运行时，差别只在 `docs/` 与文档检查器），
探针 `6b01a30f…`。两份归档互不覆盖，任何跨归档的差值都不构成本归档的结论。

## 文件与证明范围

| 文件 | 证明什么 |
|---|---|
| `manifest.json` | 本归档自己：每条复制文件的源路径、字节数、归档哈希与源哈希（文本按 CRLF 归一），绑定 head、探针哈希、点数、预注册与登记补记哈希、判定状态 |
| `tools/frontier-rederive-preregistration.json` | 测量前写定的问题、阶梯、执行顺序、通过判据与两个预测模型；判定不得越过它 |
| `tools/frontier-rederive-registration-addendum.json` | 为什么 HEAD 在首个测量点之前从 `6256af8` 移到 `a93c979`、这次移动只含 `docs/` 与 `tools/verification/check_docs.py`、探针哈希未变；预注册文件的字节未被改写 |
| `tools/run_frontier_bracket.py` | 按注册顺序逐点驱动测量、拒绝已存在目录、二进制哈希变化即中止 |
| `tools/run_40k_refresh.py` | 真正执行测量的运行器（进程数、秒数、预热、亲和、通过规则都在它里面） |
| `tools/summarize_frontier.py` | 只读预注册与实测，产出区间、单调性与锚点漂移判定；本次未改动一行 |
| `tools/analyze_stall_shape.py` | 只重读已录日志的“慢帧形状”分析，不产生新测量；本批未运行 |
| `entities-<N>/registration.json` | 该点的 head、工作树、探针源码哈希、运行器哈希、二进制哈希与计划 |
| `entities-<N>/NN-<mode>-<N>.log.txt` | 该进程的原始逐帧日志（预算、帧数、每秒窗口、`over_budget`） |
| `entities-<N>/results.json` | 该点的逐进程统计与分组中位数 |
| `derived/frontier-rederive-summary.json` | 阶梯级汇总：已完成/未完成的实体数、每点两种模式的中位数与相对范围 |
| `derived/frontier-rederive-verdict.json` | 判定：区间状态、单调性、锚点漂移结论与报告用表格 |
| `index.md` | 本文件（生成，不入 `manifest.json`） |

## 十个点与判定

注册顺序为 `1800, 3000, 2400, 3400, 2600, 3600, 2800, 3800, 3200, 4000`（升序与降序交错，避免机器
漂移被读成规模效应）。十个点全部有效、每个点只跑一次、没有重采样，也没有作废尝试，因此
`manifest.json` 的 `superseded_attempts` 为空、`excluded` 为空。

判定只由 `derived/frontier-rederive-verdict.json` 给出，本索引不重复数字，只登记它的状态：

- `render-only`：bracketed，区间 `[3600, 3800)`。
- `static`：bracketed，区间 `[2600, 2800)`。

预注册里两个预测的结局不同，这本身就是结果，不得改写成更整齐的说法：

- `render-only` 预测 `[3600, 3800)`，实测与之一致。
- `static` 预测 `[3000, 3400)`，实测是 `[2600, 2800)`，预测被否证。预注册自己写下的 caveat 说明
  这个模型把冻结阶梯的静态悬崖按实体数等比平移，而冻结阶梯已经显示静态带有一份与实体数无关的
  固定每帧成本；实测把这份 caveat 变成了可量化的偏差。

## 锚点与跨仪器边界

预注册的锚点规则是**方向性**检查，不是效应量：如果 1800 的仅渲染 p99 中位数不低于冻结阶梯的
2.7479 ms，则本次阶梯不主张任何容量延伸，只保留冻结列表。实测 1800 仅渲染 p99 中位数为 2.3452 ms，
低于该阈值，因此本次阶梯的括区可以作为**本源码、本探针**上的诊断括区报告。

0.4027 ms 的差值与 `derived/*/drift_vs_refresh_1800` 里对刷新会话（`3210.5 µs` / `3725.1 µs`）的
相对差都**不能**当作杠杆的效应量：源码、构建、会话与仪器长度同时不同。`verdict.json` 的
`binary_matches_refresh` 为 `false`，含义就是这一句。

## 主张与支撑

- “p99 在 X 与 Y 之间第一次越过预算”只由 `derived/frontier-rederive-verdict.json` 的状态与
  `derived/frontier-rederive-summary.json` 的逐点中位数支撑。
- 每点的通过/失败只按该点 `registration.json` 里登记的计划与运行器的通过规则判定，不看四舍五入后的
  报告数字；每点三个进程全部通过才算该点通过。
- 阶梯“同一次构建”只由每点 `registration.json` 的 `binary_sha256` 是否一致支撑；`manifest.json`
  的 `binding.probe_binary_sha256` 记录被核对的那个值。
- 判定为“无区间”（不单调或没有通过点）时，该结论本身也是本归档的结果，不得改写成某个更整齐的区间。

## 读取顺序

1. `manifest.json` 的 `binding`：head、登记 head、探针哈希、点数、预注册哈希、判定状态。
2. `tools/frontier-rederive-preregistration.json`：先看判据与预测，再看数字。
3. `tools/frontier-rederive-registration-addendum.json`：HEAD 为什么在测量前移动过。
4. `derived/frontier-rederive-summary.json` 的逐点中位数与相对范围。
5. `derived/frontier-rederive-verdict.json`：区间与单调性。
6. 需要复核某个点时，读该点的 `registration.json` 与逐进程日志。

## 边界

- 诊断证据：不替换冻结的容量结论（1080p 仅渲染 1800、静态 1300），不提升任何生产门限，也不构成
  R3/R4、设备或生产接受。
- 每点三个新进程、每个二十秒，只够支撑“该点在预算内/外”，不足以支撑点间差异的显著性结论。
- 只在 1080p、该探针二进制与该夹具上成立；换分辨率、夹具或构建即属另一次测量。
- 十个点都不做重采样；任何一个点无效都会如实报告为缺口，而不是补跑。
- 逐点相对范围全部低于 15% 的登记阈值，因此没有任何点需要按“只用于通过/失败、不作为速度读数”
  的规则降级引用。
