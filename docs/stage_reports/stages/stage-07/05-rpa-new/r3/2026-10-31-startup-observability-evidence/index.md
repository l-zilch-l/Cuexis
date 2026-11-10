# 启动可观测性与 prepare 复用附件

状态：冻结证据索引。返回 [批次报告](../2026-10-31-r3-startup-observability-and-prepare-reuse.md)。

- [复制来源、原始字节及哈希](manifest.json)
- [身份、取舍与限制补充](identity-and-selection-supplement.json)
- [产品启动观测脚本](scripts/watch_startup.ps1.txt)、[40K 夹具生成](scripts/make_fixture.py.txt)、[运行汇总脚本](scripts/collect_runs.py.txt)
- [40K 夹具项目与资产索引](fixture/project.json)
- [prepare 归因探针源码](prepare-attribution/prepare_attribution.cpp.txt)、[构建脚本](prepare-attribution/build_probe.py.txt)

## 目录

| 目录 | 内容 | 证明什么 |
| --- | --- | --- |
| `runs/run-head-before-v2/` | 变更前产品运行（无阶段行） | 40K 启动曾只有 11 行日志、窗口出现后 26.7 秒才应答 |
| `runs/run-head-after-v1/` | 仅 `6ae7bd7` | 阶段记录上线；`content_resolved` 8.030 秒、总启动 26.271 秒 |
| `runs/run-head-after-v2..v5/` | `+6217f00` 的四次重复 | 版本探测不再建树；准备中位 17.87 秒（首帧总计 18.36 秒），四次极差 16.1% |
| `runs/run-head-after-v6/` | `+f70d6ba` | 单趟解析；准备 3.19 秒、总启动 3.786 秒 |
| `runs/run-head-after-v7/` | `+466ff76`（增量构建） | 树复用；准备 2.32 秒、总启动 2.781 秒 |
| `runs/run-head-after-v8/` | `466ff76`（矩阵 `--clean-first` 重建） | 同一源码的第二次读数：准备 2.29 秒、总启动 2.821 秒；首次与可执行文件 sha256 直接绑定 |
| `runs/run-head-invalid-fixture-v1/` | 早期无效夹具尝试 | 工具/夹具错误独立保留，不当性能失败 |
| `prepare-attribution/run-v*.log.txt` | 同进程分阶段计时（第一轮，改动后立即） | 解析、canonical 写、类型化读取、完整准备的耗时与峰值内存 |
| `prepare-attribution/run-head-rebuild.log.txt` | 同进程分阶段计时（第二轮，矩阵重建后） | 同一源码在干净构建下的同类读数；跨轮不可比的原因见补充第 6 节 |

每个 run 目录含 `summary.json.txt`（窗口出现/首次应答/未应答探测合计/采样分类）、
`phases.json.txt`（日志中的阶段行解析结果）、`samples.csv.txt`（100 ms 一次的应答记录）、
`player.out.log.txt` / `player.err.log.txt`。归档时所有文本文件加了 `.txt` 后缀，
这样浏览器和编辑器不会把它们当成可执行内容。`summary.json` 的
`unresponsive_probe_ms` 是各次未应答探测的耗时之和，单次最长等待要看 `samples.csv`
的 `probe_ms` 列。

## 主张与支撑对照

| 主张 | 支撑 | 明确不支撑 |
| --- | --- | --- |
| 产品启动路径现在有阶段记录 | `run-head-after-v1..v8` 的 `player.startup_phase` 行 | `run-head-before-v2` 没有这些行，不能反推它的阶段耗时 |
| 窗口出现后即可应答 | 首次应答减窗口出现：v5 0.36 秒、v6 0.53 秒、v7 0.17 秒、v8 0 秒；单次最长等待 66 ms（v8），v5–v7 为 62–63 ms；v8 的 3 次未应答探测合计 186 ms 且随后恢复 | 不覆盖渲染帧调度，也不等于所有建窗后操作实时 |
| 40K 准备从 17.87 秒中位降到 2.29 秒 | v2..v5 四次重复与 v7/v8 的 `content_prepared`；`prepare-attribution/run-v6`（第一轮）与 `run-head-rebuild`（第二轮） | 不是首次冷加载；不是“进程到首画面 ≤2 秒”通过；v6→v7/v8 是单次对照，另由探针约 0.9 秒的反差印证 |
| legacy prepare 只解析一次 | `tests/playback/preparation_characterization_tests.cpp` 的计数用例 | 只覆盖 legacy 路径；v4 dispatch 不走该复用 |
| canonical 身份未变 | 探针六次（两轮各三次）`936c32ee…` 一致，`objects 40001` | 不证明其他规模或 v4 文档的身份 |
| canonical Writer 失败面未改 | `loadDocumentStage` 回落文本入口，`chart.writer.*` 诊断保留 | 不证明所有失败路径都有测试覆盖 |

## 基线对照

| 运行 | 源码状态 | 窗口出现 (ms) | 首次应答 (ms) | 未应答探测合计 (ms) | 未应答采样 | 阶段总计 (µs) |
| --- | --- | --- | --- | --- | --- | --- |
| `run-head-before-v2` | `6ae7bd7` 之前 | 8,410 | 35,141 | 1,912 | 225 | 无阶段行 |
| `run-head-after-v1` | `6ae7bd7` | 26,270 | 26,270 | 61 | 1 | 26,270,511 |
| `run-head-after-v5` | `+6217f00` | 18,613 | 18,970 | 124 | 2 | 18,597,276 |
| `run-head-after-v6` | `+f70d6ba` | 3,774 | 4,307 | 186 | 3 | 3,785,742 |
| `run-head-after-v7` | `+466ff76` | 2,858 | 3,029 | 62 | 1 | 2,780,598 |
| `run-head-after-v8` | `466ff76`（全量重建） | 2,821 | 2,821 | 186 | 3 | 2,820,918 |

`6ae7bd7` 改变了建窗时机，所以“窗口出现”一列在设计变更前后不可直接比较；
`run-head-before-v2` 的那一行只用来证明旧路径出现过 26.7 秒的冻结。

## 读取顺序

1. `runs/run-head-after-v8/summary.json.txt` 与 `player.out.log.txt`：当前状态（含 exe 哈希绑定）。
2. `runs/run-head-before-v2/summary.json.txt`：变更前的冻结证据。
3. `prepare-attribution/run-v6-tree-reuse.log.txt`（第一轮）与 `run-head-rebuild.log.txt`（第二轮）：
   prepare 剩余成本的构成，以及跨轮不可比的原因。
4. `manifest.json`：每个复制文件的原始路径、字节数与 sha256（含源哈希与归档哈希）。
5. `identity-and-selection-supplement.json`：绑定、取舍、审查限制与不合并的数字。

文本证据（日志、json、csv、脚本源码）在归档时统一为 CRLF，使新检出得到相同字节；
`.bin` 夹具资产按二进制原样保存。两者在 `manifest.json` 里分别记录 `source_sha256`
与 `sha256`，`eol` 字段标明处理方式。
