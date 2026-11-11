# R4 owner 裁定记录（2026-11-11）

状态：accepted decision record；记录 owner 在审阅中给出的四项裁定，不改写任何已测字节

日期：2026-11-11

返回 [New RPA 报告索引](../README.md)与 [R4 门账](2026-11-08-r4-gate-status.md)。

## 1. P5 采用探针形态，不做夹具改造

裁定：`p5-controls` 保留现有脚本化探针（`tests/pd/pd5_host_probe.cpp` 的 `controls` 模式），
不为它定义独立夹具。

依据：[控制探针设计与 pilot](2026-11-11-p5-controls-unit.md)记录的事实——该构建下探针**不创建窗口**
（渲染器是 `TestPresentationRenderer`，SDL 窗口只在 `CUEXIS_PD5_DEVICE` 下存在），且被计时的区间只覆盖
控制调用本身，不含任何窗口成本。因此"自建探针比夹具更贵"的前提不成立，夹具只会增加工程量而不消除
对安静前台的要求。

## 2. 接受内存归属修正

裁定：接受 [#34](https://github.com/l-zilch-l/Cuexis/pull/34) 的归属修正结果，即
采样/前进期间物化的执行内核状态改记到 `execution.retained`。

同一 40,000 实体 + 4,096 行运行的实测（总字节不变，仍 365,084,102 B）：

| 域 | 修正前 | 修正后 | 声明上限 | 倍率 |
| --- | ---: | ---: | ---: | ---: |
| `command.session` | 160,834,198 B | 19,762,486 B | 21,854,208 B | 0.90，在声明内 |
| `execution.retained` | 8,823 B | 141,080,535 B | 1,327,497,216 B | 0.11，在声明内 |
| `state.world` | 33,125,473 B | 未变 | 163,905,536 B | 0.20 |

`over_declared` 为空：21 个域全部在其声明上限内。**没有修改** `budget-proposal.v1.json`、
O1 接受记录或任何上限数值；被接受的是记账归属。

## 3. P8 内容口径改为正式口径

裁定：P8 的正式内容口径是 **40,000 实体 + 4,096 内容行**，即
[P8 报告](2026-11-10-p8-40k-entity-memory.md) §1–§2.1 的那一档。

扩充规则（书写为口径的一部分，不是新的裁定）：

- 口径按 `(entities, rows)` 二维声明；上限由实测系数推算后用一次抽查确认，不重做全量归因：
  实体轴 `state.world` 828 B、`state.candidate` 516 B、`delivery.payload` 368 B、执行内核 3,524 B；
  内容行轴 `state.current` 6,456 B、`content.decoded` 9,563 B、`codec.buffers` 2,262 B、
  `owning.results` 3,382 B；固定项 `command.session` 19,766,750 B。
- 内容行受冻结的 `maxPackedStrings = 100000` 约束（每行至少 3 个唯一串，实际上限约 33K 行）；
  实体数只受夹具规模限制。
- 提高实体数只需要新增 profile 档位与其上限；40,000 只是当前的基准档。

## 4. `read_long` 允许一次正式组

裁定：批准重开一次正式 `read_long` 组（legacy 与 RPA 各一个 profile）。

使用条件（沿用 [超界处置](2026-11-07-read-long-over-budget-disposition.md) §2 方案 A）：
先在有界修复后跑预登记的单变量诊断，两个 profile 各改善至少 5% 才消费这次正式额度；
否则额度保留，并把结果写回处置报告。

## 5. 边界

- 本文件只记录裁定，不替代任何测量证据；上表数字均有对应的原始日志与只读复算。
- 未被本记录覆盖的仍是待裁事项：O2 实体操作、P7 听音、F3 生产参数、O3/PD 退出。
- 本记录不构成合并、发行或阶段关闭。
