# R3 启动可观测性与 prepare 复用

状态：有限本地启动诊断；相关实现已进入根树，但这不是 R3/R4 或 Stage 7 接受，也不是
40K、240 FPS、首次加载时延或全配置矩阵的门槛通过。

日期：2026-10-31 夜（`v8` 于本地 2026-11-01 00:01 补测，源码状态不变）。返回 [New RPA 索引](../README.md)、[R3 启动响应与 JSON 归因](2026-10-31-r3-startup-and-render-attribution.md)。
本报告的原始证据见[启动可观测性证据附件](2026-10-31-startup-observability-evidence/index.md)。
当前接受状态只见 [CURRENT_STATUS](../../../../../CURRENT_STATUS.md)。

## 现场问题与本轮目标

前一阶段确认：40K 夹具下产品 Player 在可见窗口出现后长时间不响应消息。当时诊断用的探针
（`tests/player/gpu_throughput_probe.cpp`）已改成建窗前完成 CPU 准备，但
“产品 Player 的同步加载路径并未随探针修改而修复”。owner 给出的优先顺序是：先解决 40K
测试启动阶段的可观测性和窗口响应，再谈 CPU 中性路径成本，最后才用新鲜完整矩阵判断阶段门槛。

因此本轮只做三件事：给产品启动路径加阶段记录；把准备移出可见窗口响应期；压缩准备里可被
证明的成本。命令、日志和失败目录全部保留，没有为了数字好看重跑失败点。

## 实际修改

四个提交，按落地顺序：

| 提交 | 内容 | 作用 |
| --- | --- | --- |
| `6ae7bd7` | `app/player/src/player_app.cpp`、`player_control.cpp/.hpp`：新增 `StartupPhases` 阶段记录与 `preparePlayerLoadBundle` / `adoptStartupLoad`，先准备 bundle，再创建窗口与设备 | 窗口一旦出现就已经可响应；启动不再只有外部门窗轮询 |
| `6217f00` | `engine/json_support`：新增 `scanTopLevelInteger` 与 `bounded_parse.cpp` 支持，`chart_v4_loader` 直接读版本成员 | 消除 v4 判别前为读一个整数而构建整棵树 |
| `f70d6ba` | `engine/json_support/src/parse.cpp`：`parse()` 不再走 nlohmann 回调 DOM，改为 `parseBounded` 单趟构建 Cuexis 值树 | 去掉每完成一个对象就回扫父容器的二次复杂度 |
| `466ff76` | `engine/chart`、`engine/playback`：typed legacy reader 改为借用已解析树，dispatch 结果携带 canonical 身份字节 | legacy prepare 只解析一次文档，身份字节来自同一次解析 |

阶段记录为 `player.startup_phase` 行，字段是 `content_resolved`、`content_prepared`、
`window_created`、`backend_created`、`content_committed`、`transport_playing`、
`first_frame_presented`（后者附带 `opaque` / `transparent` / `render_us`）。在 `6ae7bd7`
之前产品日志没有这些行，40K 启动只有 11 行日志、且没有任何阶段耗时；这是“可观测性”一项
的直接证据，不是从别处推断。

`466ff76` 的语义边界：canonical Writer 拒绝该值（例如 `chart.writer.number_non_finite`）时，
dispatch 不产出字节，`PlaybackSession::loadDocumentStage` 回落到原文本入口，因此该失败面的
诊断代码与位置保持不变；新增的 `legacyCanonicalText` 只在成功 legacy 路径上有值，
调用方不能凭 `hasValue()` 推断它。v4 dispatch 刻意不产出该字段。

## 测量方法与绑定

- 夹具：`fixture-40k`，v3 Chart，13,171,906 字节，
  sha256 `6e5746a31b22a0839a9bc12bc05bd6a1cc6ec6a9a1724bd67027b59b8365c14e`，
  40,001 个对象，canonical 29,652,435 字节，身份
  `936c32eede42abfbd136e217591c623b3ba3dc7093694fe60a356e070f656869`。
  由 `make_fixture.py` 生成；13 MB 夹具本体不入库，以哈希和生成脚本定位。
- 观测：`watch_startup.ps1` 启动 `out/build/release/bin/cuexis_player.exe`，
  按 100 ms 间隔对主窗口投递消息并等待应答，逐次记录采样；`window_first_ms` 是第一次
  枚举到可见窗口的时刻，`first_responsive_ms` 是第一次收到应答的时刻，
  `unresponsive_probe_ms` 是**各次未应答探测耗时之和**（不是单次最大值），单次最长等待
  另由 `samples.csv` 的 `probe_ms` 列给出。被强制终止才算失败，正常关闭被记录。
- 归因探针：`prepare_attribution.exe`，单进程内依次计时
  `json.parse`、`writeCanonicalJson`、`ChartLoader.load`、`prepareLoad`，
  每档重复三次，峰值内存按进程读取。Release 库独立链接，只重编探针 translation unit。
- 判据不写成“通过/接受”：本轮只报告可复现的耗时与响应时刻。窗口出现时刻在设计变更前后
  **不可直接比较**（旧顺序是“先建窗、后准备”，窗口会早出现但随后冻结），可比的是窗口
  开始应答的时刻：35,141 → 2,821 ms，**12.5 倍**。

## 结果

同一夹具、同一 Release 构建配置，每次提交后重建、逐次全新进程：

| 运行 | 源码状态 | 窗口出现 (ms) | 首次应答 (ms) | 未应答探测合计 (ms) | 未应答采样 | 阶段总计 (µs) |
| --- | --- | --- | --- | --- | --- | --- |
| `run-head-before-v2` | `6ae7bd7` 之前 | 8,410 | 35,141 | 1,912 | 225 | 无阶段行 |
| `run-head-after-v1` | `6ae7bd7` | 26,270 | 26,270 | 61 | 1 | 26,270,511 |
| `run-head-after-v2` | `+6217f00` | 17,940 | 17,940 | 61 | 1 | 17,888,404 |
| `run-head-after-v3` | `+6217f00` | 20,823 | 20,823 | 61 | 1 | 20,767,791 |
| `run-head-after-v4` | `+6217f00` | 18,081 | 18,081 | 63 | 1 | 18,115,074 |
| `run-head-after-v5` | `+6217f00` | 18,613 | 18,970 | 124 | 2 | 18,597,276 |
| `run-head-after-v6` | `+f70d6ba` | 3,774 | 4,307 | 186 | 3 | 3,785,742 |
| `run-head-after-v7` | `+466ff76` | 2,858 | 3,029 | 62 | 1 | 2,780,598 |
| `run-head-after-v8` | `466ff76`（矩阵重建） | 2,821 | 2,821 | 186 | 3 | 2,820,918 |

`v8` 与 `v7` 是同一源码状态下的两次独立进程（`v7` 的二进制由 `out/build/release`
增量构建，`v8` 由矩阵第 `msvc-off-release` 行 `--clean-first` 全量重建）：首帧总计
2,780,598 → 2,820,918 µs（+1.4%），`content_prepared` 2,322,684 → 2,286,443 µs（−1.6%），
窗口出现 2,858 → 2,821 ms。两个方向的差异都在这一档的噪声内，不构成收益或退步。

`v2..v5` 是同一次提交后的四次重复，用来量噪声而不是量收益：`content_prepared`
为 17,409,343 / 20,287,989 / 17,631,645 / 18,114,570 µs，中位 **17,873,107 µs**，
相对极差 (20,287,989 − 17,409,343) / 17,873,107 = **16.1%**；首帧总计中位 18,356,175 µs，
相对极差 **15.7%**。两条都超过本仓库 15% 的重复性惯例，因此这一档只报中位数，
单次 2.9 秒以内的差异不当作收益。

绑定方式分两级。`v1..v7` 的 run 目录没有逐次记录可执行文件哈希（当时的观测脚本没有
采集），源码绑定靠提交正文里逐条写下的同一组数字（`6ae7bd7` 的 26.270 秒、`6217f00` 的
17.899 / 20.778 / 18.126 / 18.608 秒、`f70d6ba` 的 3,788/3,981 ms prepare、`466ff76` 的
2,739/2,878 ms 与 3,029 ms），加上未变过的夹具哈希（13,171,906 字节，
`6e5746a3…`）。`v8` 起改为直接绑定：可执行文件
`out/build/release/bin/cuexis_player.exe` 的 sha256 为 `e4148f55ba7e…`，由矩阵
`msvc-off-release` 行从 `466ff76` 全量重建（`--clean-first`）；`v8` 本地 00:00:59 启动、
00:01:29 结束，绑定记录在
`r3-40k-throughput-refresh-v1/after-matrix-status.json`（`head 466ff76`、
`worktree_dirty false`、`player_sha256 e4148f55…`）。该哈希只覆盖 `v8`，不回填 `v1..v7`。

可复现的结论只有两条：

1. 窗口从“出现后 26.7 秒才应答”变为“出现即应答”。
   旧路径窗口 8,410 ms 出现、35,141 ms 才应答；`v6` 窗口 3,774 ms 出现、4,307 ms 应答（差 533 ms）；
   `v7` 2,858 → 3,029 ms（差 171 ms）；`v8` 2,821 → 2,821 ms（差 0 ms），单次最长等待 66 ms，
   3 次未应答探测各约 61–63 ms 且随后恢复。
2. 40K 准备（`content_prepared`）从 17,873,107 µs 中位降到 2,286,443（`v8`）/ 2,322,684（`v7`）µs，
   约 **7.7–7.8 倍**；首帧总计从 `v1` 的 26,270,511 µs 降到 2,780,598（`v7`）/ 2,820,918（`v8`）µs，
   约 **9.3–9.4 倍**。
   `v1` 的 26.27 秒里，准备占 17,778,337 µs，版本判别前的整树构建占 8,029,524 µs，
   其余约 0.46 秒是窗口、设备与提交阶段。

可归因的分解（每次只动一件事，且数字都写在对应提交正文里）：

- `6217f00`：`content_resolved` 从 8,029,523 µs 降到约 19,000 µs，即 v4 判别不再为读一个
  整数构建整棵树；总启动 26,271 → 17,888 ms（均为表内阶段总计）。
- `f70d6ba`：`content_prepared` 从 17.6–20.3 秒降到 3,191 ms（`json.parse` 10.05 s → 0.93 s），
  总启动 18,597 → 3,786 ms。
- `466ff76`：`content_prepared` 从 3,191 ms（`v6`）降到 2,323（`v7`）/ 2,286（`v8`）ms，
  即 −27% / −28%，与提交正文记录的“第二次解析约 0.9 秒”同量级；总启动 3,786 → 2,781 / 2,821 ms。
  探针侧对同一改动有两轮读数，跨轮不可比的原因见下节。

`v1` 与 `v2` 之间那一步（`6217f00` 的前身对象 `a5f0723`，同一 tree `a46f2302`，重排前
挂在另一个父提交上）已经不在分支历史上，只能从 `6ae7bd7` 的正文与 `v1`/`v2` 的数字对上；
这一点写在附件补充里，不改写历史提交。

`v7` 的阶段明细（µs，`elapsed_us` / `total_us`）：

| 阶段 | elapsed | total |
| --- | --- | --- |
| `content_resolved` | 19,074 | 19,074 |
| `content_prepared` | 2,322,684 | 2,341,758 |
| `window_created` | 356,987 | 2,698,746 |
| `backend_created` | 12,900 | 2,711,647 |
| `content_committed` | 995 | 2,712,642 |
| `transport_playing` | 256 | 2,712,899 |
| `first_frame_presented` | 67,698 | 2,780,598 |

首帧记录为 `opaque 40000 transparent 0`：首张呈现帧确实包含全部 40,000 条不透明命令，
不是空画面充数。该行的 `render_us 0` 表示首帧钩子没有采到渲染时间，本轮不据此声称任何
帧耗时。

## prepare 归因

同一 40K 夹具、同一 `prepareLoad` 入口，两轮独立读数。第一轮在 `466ff76` 的工作树改动
落地后立即测（`run-v5` / `run-v6`，21:59 / 22:03；提交本身在 22:03:34 落盘）。第二轮在
2026-10-31 22:06 启动的那轮矩阵（第 8 行因磁盘写满中止，见
`out/rpa-acceptance/disk-recovery-20261031.json`）做完 `msvc-off-release` 行 `--clean-first`
全量重建之后测：探针 00:02:08 重新编译并链接、00:03:27 测完，而新一轮完整矩阵 00:04:20
才开始，因此这一轮没有并发构建（归档名 `run-head-rebuild.log.txt`）。两轮都是同一进程
内三次重复：

| 阶段 (ms) | 第一轮（改动后立即） | 第二轮（矩阵全量重建后） | 说明 |
| --- | --- | --- | --- |
| `json.parse` | 991.20 / 957.09 / 903.85 | 759.15 / 743.65 / 706.71 | 唯一一次解析，投影与身份共用 |
| `writeCanonicalJson`（参考） | 1,906.80 / 1,874.43 / 1,804.56 | 1,457.49 / 1,452.92 / 1,407.72 | 含自身解析；prepare 内已不再调用 |
| `ChartLoader.load`（参考） | 1,558.83 / 1,501.26 / 1,458.80 | 1,179.25 / 1,178.89 / 1,126.32 | 含自身解析的类型化读取 |
| `prepareLoad` | 2,878.02 / 2,876.81 / 2,738.64 | 2,202.27 / 2,171.37 / 2,087.55 | 完整准备 |
| `startup path total` | 2,908.31 / 2,905.16 / 2,766.80 | 2,224.42 / 2,193.72 / 2,109.61 | 加 `fromFilesystemProject` 22–30 ms |
| 峰值工作集 (MiB) | 343.5 / 345.0 / 350.5 | 343.8 / 344.5 / 359.5 | 三次重复各自记录 |
| 峰值私有 (MiB) | 255.5 / 256.3 / 306.1 | 254.7 / 256.4 / 315.3 | 三次重复各自记录 |

两轮内部都满足重复性：第一轮 `prepareLoad` 相对极差 (2,878.02 − 2,738.64) / 2,876.81 = 4.8%，
第二轮 5.3%；身份字节 `936c32ee…`、`canonical bytes 29652435`、`objects 40001` 在两轮共六次
读数里完全一致，说明语义没变。

两轮之间**不跨轮比较绝对耗时**，理由在数据里：库内阶段按同一比例下降
（`json.parse` ×0.77–0.78、`ChartLoader.load` ×0.76–0.79、`writeCanonicalJson` ×0.76–0.78、
残差 ×0.62–0.80），而探针 TU 内以头文件编译的 nlohmann 路径只降 3–6%
（`nlohmann + observer` 9,067.66 / 9,532.28 / 9,434.24 → 8,742.54 / 8,932.86 / 9,102.91 ms，
×0.94–0.97）。同一进程里“只发生在库代码上”的等比下降既不能归给源码（`466ff76` 没动
`engine/json_support`，`json.parse` 却同步变快），也不能归给 CPU 频率或内存压力（那会连
nlohmann 路径一起变慢），只能归给库的编译产物性质：第一轮用增量构建的库，第二轮是
`--clean-first` 全量重建的库。因此本节只用复合值判断方向，不把任何单阶段差值挂到具体
提交上。

改动本身的证据取两处复合值，方向与量级一致：

- 探针 `prepareLoad`：改动前 `run-v4-onepass`（两次）3,981.30 / 3,788.66 ms；改动后第一轮
  2,858.97 / 2,752.00 ms（`run-v5`）与 2,878.02 / 2,876.81 / 2,738.64 ms（`run-v6`）。改动后
  所有测点都低于改动前最低值 3,788.66 ms，最坏配对 3,981.30 → 2,878.02 = **−27.7%**。
- 产品 `content_prepared`：3,191 ms（`v6`，`f70d6ba`）→ 2,323（`v7`）/ 2,286（`v8`）ms，
  即 −27% / −28%，与提交正文记录的“第二次解析约 0.9 秒”同量级。

第二轮（当前 HEAD 的干净构建）给出 2,087.55–2,202.27 ms，比第一轮再低 24%，但按上面的
理由**不计入任何提交的收益**，只作为“当前源码状态的 prepare 上限”使用。

剩余成本按第二轮内部差值读作（推导，不是分档直测）：解析 0.71–0.76 秒（已合并为一次）、
类型化投影 0.42–0.44 秒（`ChartLoader.load` 1.13–1.18 秒减去其中自带的解析）、
canonicalize 加 nlohmann 整树副本与转储 0.70–0.71 秒、lowering 与 world 0.26–0.32 秒。
第一轮的比例结构与此几乎相同（0.90–0.99 / 0.54–0.57 / 0.90–0.92 / 0.38–0.46 秒），
所以“canonical 写出约占 `prepareLoad` 的 65–67%（第一轮 65–66%、第二轮 66–67%）”在两轮都
成立：它是目前最大的一块，也是唯一还能靠纯 CPU 工作压缩的一块。再往下压只能动
`serializeCanonical` 内部的整树 `toNlohmann` 副本，需要逐字节等价的差分语料才能尝试；
本轮没有做，也没有把它算进上面任何一个数字。

产品侧 `content_prepared`（`v8` 的 2,286,443 µs）与探针 `prepareLoad`（第二轮
2,087.55–2,202.27 ms）不是同一作用域：前者是 bundle 准备内部的一行，后者是 SDK 调用本身，
两者不互相换算、也不取平均。

## 与既有 parse-once 原型的关系

`2026-10-31-startup-evidence` 里已冻结一版未选用原型（`unselected-legacy-parse-once-v1.patch`），
机制相同：typed reader 借用同一棵树，再从该树产出 canonical 字节。本轮没有复用它的
out 参数形状，而是让 dispatch 结果携带字节、由调用方显式检查；原审查记录的两个限制
（writer 早于 caller 诊断、输出参数不能由 `hasValue()` 推断）在这里分别由“回落到文本入口”
和“显式 `has_value` 判断”消除。

数字不合并：原型登记的 40K prepare 是 17.555 → 2.028 秒，绑定的是它自己的夹具与
`legacy-parse-once-measure-v1` 回执；本轮 40K prepare 是 3,788/3,981 → 2,739/2,878 ms
（第一轮，改动后立即）与 2,088/2,202 ms（第二轮，矩阵全量重建），
夹具为本文 13,171,906 字节 v3 文档。两者是不同的登记，不能相加、相减或互相替代。
原型当时因“普通各规模 ≥30% 与重复性筛选”未选用，本轮也没有据此宣称普通规模接受：
本轮只声明 40K 启动路径的可观测性与响应改善。

## 记忆与 128 MiB 预算的关系

本轮没有新增第二棵活树：身份字节产自同一次解析的树，`serializeCanonical` 期间仍同时存在
树、约 29.7 MB 文本和一次 nlohmann 副本。40K 准备期进程峰值私有 254.7–315.3 MiB（六次
读数，最小 254.7 MiB），已经高于实验用的 128 MiB 预算（128 MiB 是最小峰值的 50.3%）；因此“128 MiB 预算能否支撑 40K 测试”的答案是否定的，
细节与另一条独立证据（密集单点记费 244,028,048 字节、实际额外内存仅 323,303 字节）
留在容量与预算结论里，不在本文重述。

## 复现

```powershell
# 产品启动观测（仓库根目录，Git Bash / PowerShell 均可）
powershell -NoProfile -ExecutionPolicy Bypass -File `
  out\verification-20261031\r3-product-startup-v1\watch_startup.ps1 `
  -Project out\verification-20261031\r3-product-startup-v1\fixture-40k `
  -OutDir out\verification-20261031\r3-product-startup-v1\run-<name> -Seconds 30  # v1..v7 用 25

# prepare 归因（先构建 release，再单独构建探针）
$env:PATH = "D:\Cuexis-redesign\out\build\release\bin;" + $env:PATH
out\verification-20261031\r3-prepare-attribution-v1\prepare_attribution.exe `
  out\verification-20261031\r3-product-startup-v1\fixture-40k\assets\charts\stage3_example.cuexis.chart.json `
  out\verification-20261031\r3-product-startup-v1\fixture-40k 3
```

## 剩余边界

本批没有测量普通帧 240 FPS、GPU 帧成本、Seek、完整播放内存与保留上界、Debug/Release、
STATIC/SHARED、MinGW、安装消费。这些仍在 R3 账本里开放；同一源码状态下的 11 行完整矩阵
在矩阵归档里单独登记，不并入本文任何结论。
“2.82 秒窗口出现”不是“进程到首画面 ≤2 秒”通过：它包含设备创建，且不是首次冷加载。
40K 在 240 FPS 下是否可行、128 MiB 预算是否够用，属于另一条结论，不由本文任何数字支撑。
