# R3 40K 启动可观测性与 prepare 成本

状态：有限本地诊断；两项实现已进入根树，附件已归档并自校验。这不是 R3 / R4 或 Stage 7 接受，
也不是 40K、240 FPS、首次加载门槛或全配置矩阵的通过。

日期：2026-11-01（本地）。返回 [New RPA 索引](../README.md)、
[启动可观测性与准备复用](2026-10-31-r3-startup-observability-and-prepare-reuse.md)、
[提交路径停顿归因](2026-11-01-r3-submit-stall-wait-attribution.md)。
原始证据见[启动与 prepare 成本证据附件](2026-11-01-40k-startup-and-prepare-cost-evidence/index.md)。
当前接受状态只见 [CURRENT_STATUS](../../../../../CURRENT_STATUS.md)。

## 1. 现场问题与本轮目标

上一轮把产品 Player 的启动阶段行 `[player.startup_phase]` 补上，并把内容准备移出了"窗口已可见"的
区间。那一轮留下两个没有回答的问题：

1. **测量侧没有可观测性。** `tests/player/gpu_throughput_probe.cpp` 的启动路径只有一个总的吞吐
   数字，40K 下只能看到"首帧 2.4 s"，说不出这 2.4 s 落在哪一段。产品有阶段行、探针没有，于是
   "启动可观测性"这件事只在产品侧成立，在测量侧不成立。
2. **prepare 里有一次多余的 32 MB 文本构建。** canonical 文本先由 `json::Value` 经 nlohmann DOM
   被 `dump` 出来，为了算 chart 身份又走了一遍字符串处理。40K 上这是一段可测的成本。

owner 对本轮的指示是逐字的：

> 40K压测属于测试极端情况，现在仅相差149ms，我认为不需要为了达到指标而做大量重构，你只需要做
> 最便宜的改动即可。

以及先落恢复点再优化的顺序（"我同意这个顺序。"），和开工前先清理旧构建产物的要求。因此本轮的范围
被收窄成：清理 → 先提交恢复点 → 只做最便宜的观测与序列化改动 → 新鲜分组测量 → 归档。

同时，owner 给出的推进顺序是：先解决 40K 测试启动阶段的可观测性和窗口响应，再继续压缩 CPU 中性
路径成本，最后才用新鲜、可重复的完整矩阵判断是否达到相应阶段门槛。**本报告只覆盖前一项**；后两项
仍是开放的，本报告不给它们任何结论。

明确不做的事：

- 把 `json_dom_parse` + `chart_typed_read` 改写成 SAX 式 typed reader。这是"大量重构"，owner 已
  明确否掉，本轮连原型都没有做。
- 任何把 prepare 挪出 owner 线程的方案。`PlaybackSession` / `PreparedPlayback` 由
  `core::ThreadChecker` 约束，在别的线程析构或移动直接 `std::terminate()`；本轮已经有一个
  之前留下的 `0xC0000409` 反例（见 §7 的 `smoke-wf-01`），不再重复。

## 2. 开工前的清理

清理只针对旧构建产物和临时文件，按 owner "先清理一下旧的构建产物、不需要的临时文件"的指示执行，
清单由子代理统计、由我逐条复核后再删。

| 项 | 内容 |
|---|---|
| 删除的构建树 | 10 个，合计 **77.853 GiB** |
| `__pycache__` | 138 个 |
| 临时文件 | 13 个 |
| D: 可用空间 | 9.60 → **87.41 GiB** |
| 保留 | `debug`、`release`、`rpa-candidate-release` |

后果必须写明：被删的 10 个树里有 8 个在自身之外被引用，因此 run-3 那批记录里的逐行
`binary_sha256` 现在是**历史标识**，不是可以从盘上重新推出的值；MSVC 构建本来也不是字节可复现的。
清理清单与引用扫描（`cleanup/build-inventory.json`、`cleanup/build-references.json`、
`cleanup/delete-plan.json`、`cleanup/temp-files.json`）随档案归档，标注 `role: repo-hygiene;
not a measurement`，不参与任何测量结论。

## 3. 本轮的两个提交

| 提交 | 内容 | 规模 |
|---|---|---|
| `451599f` Make the start-up phase observable and the 40K load cancellable | 探针 14 阶段、`prepare_trace.hpp`、`playback_session` / `chart_v4_loader` 门控埋点、可选窗口先行顺序、40K 可取消 | 5 文件，+449 −88 |
| `59e43da` Emit canonical JSON without a second value tree | `engine/json_support/src/serialize.cpp` + 测试 + 32 MB canonical 由一次 DOM 直接输出 | 7 文件，+648 −17 |

`451599f` 是本轮的恢复点：先把它落成一个可回溯的提交，再在它之上做优化，两个动作不混在一个提交里。

### 3.1 观测部分（`451599f`）

- `engine/core/internal/cuexis_internal/prepare_trace.hpp`：`PrepareTraceScope`，由环境变量
  `CUEXIS_PREPARE_TRACE` 门控。**默认完全 no-op**：不加时间戳、不建字符串、不分配。因此在同一
  二进制上，历史上记录过的默认路径数字仍然成立，不需要重测也不能被本轮的插桩数字替换。
  `cuexis_core` 把 internal include 目录作为 `BUILD_INTERFACE` 导出，所以 chart 与 playback 都能
  用它，不需要新增公共头。
- 探针的 14 个启动阶段，按顺序：`fixture_source`、`playback_prepare`、`sdl_runtime`、
  `gl_context`、`window_create`、`backend_create`、`pixel_readback`、`renderer_prepare`、
  `session_commit`、`backend_activate`、`first_update`、`first_extract`、`first_submit`、
  `first_present`。每个阶段名包住一段确定的调用，不是估点。
- 探针新增 `CUEXIS_GPU_FIXTURE_DUMP`：12.5 MB / 40 000 实体的 chart 必须先落成 fixture，否则
  Player 的启动无法被逐段归因。
- `CUEXIS_PLAYER_WINDOW_FIRST`：把启动的两步调换——先建 runtime / 窗口 / surface / GL backend 并
  呈现一帧静止画面，再在同一线程上跑 prepare。默认关闭，`player_app.cpp` 里写明了理由：已记录的
  启动测量描述的是历史顺序，两种顺序的证据不可互换。

40K 的 prepare 在 trace 下能看到 19 个 scope，例如
`load_document` → `load_chart` → `json_dom_parse` / `chart_typed_read` / `chart_canonical_write`
→ … → `content_prepared`。这些名字就是这套仪器的词汇表：`a` / `a2` / `b` 是同一个二进制
（`c5e1c317…`，校验 1 已确认摘要相同），`w1` 与盘上那份是另外两个构建，跨构建只主张词汇相同
（14 个启动阶段名、19 个 prepare scope 名逐份比对一致），不主张数值可比。

Player 侧原有的 `[player.startup_phase]` 行（`content_prepared`、`content_committed`、
`first_frame_presented`）在归档里被逐个进程从 `.out.txt` 重新解析，并与驱动记录的汇总值比对，
不一致会被报出来而不是被抹平。本批全部一致。

### 3.2 序列化部分（`59e43da`）

`engine/json_support/src/serialize.cpp`（11 451 B，
`f3e549d742f23420bc6b4d9c310085738bb17c53f1617532938f1bc101cbd7a1`）把 `json::Value` 直接写成
文本，不再先构造 nlohmann DOM 再 dump。被判据钉住的输出细节：

- pretty / compact 两种布局，缩进 2 空格，键序保持原样；
- 转义集合与 `\u00xx` 形式与旧输出逐字节相同；
- 合法 UTF-8 原样保留，非法 UTF-8 仍然拒绝（错误码从库的 `type_error` 变为
  `json.serialize.failed`）；
- 数字用 `to_chars` 的最短往返写法；非有限数仍写 `null`。

**门槛是字节身份，不是 JSON 等价**：chart 身份就是 canonical 文本的 SHA-256
（`chart::canonicalBytesIdentity`）。因此：

- 12 个 golden 案例是从**改写前**的二进制捕获的（`baseline/serialize-capture.txt`），由
  `gen_serialize_golden.py` 生成为 `tests/json_support/serialize_golden.hpp`，新写入器的字节被
  旧输出钉住；
- 测试另加不动点性质 `serialize(parse(out)) == out`；
- 22 个仓库 chart fixture 在改写前后各跑一次 migrator，逐案例 `output_sha256` 全等，差异 0。

`chart_writer` 同时把 `chart_canonical_write` 拆成 `chart_canonical_normalize` 与
`chart_canonical_dump` 两个 scope，否则"写"这一个数会同时包含排序、探测和输出，无法判断改写到底
省掉了哪一部分。

## 4. 测量方法与绑定

- **组**：`a`、`a2`、`b` 是探针组（`a` 已被取代），`w1`、`w2` 是外部 watcher 组。每组每次全新
  进程；亲和掩码 65535，execution-QoS 关闭；不经过构建限速器；测量期间不构建、不改 tracked 文件。
- **before / after 是同一台仪器**：两臂都是 `run_prepare_split_set.py` 的 3 进程计划、同一个 40K
  项目、同一个 `PREPARE_TRACE=1`，**只有 player 二进制不同**：
  before `8591b67a…` 5 676 544 B（进程标签 `writer-split-00/01/02`），after `cb575e15…`
  5 674 496 B（进程标签 `player-40k-traced-00/01/02`），后者与盘上二进制一致。所以 §6 的逐阶段
  差值是同一工具、同一场景的配对，不是跨仪器拼接。
- **一次作废的绑定被保留**：`baseline/prepare-split.original.json` 把 after 臂的摘要绑到了 before
  臂的数字上，这是出处缺陷。它原样保留、不用于任何差值；正式使用的是重新生成、
  `before_arm_matches_after_original` 为 true 的 `baseline/prepare-split.json`。
- **探针二进制有三种**：`a/a2/b` 测的 `c5e1c317…` 已不在盘上；`w1` 测的 `ffc8ebf3…`
  5 744 640 B 也不在盘上；盘上现在是 `c718206d…` 5 730 816 B，因为序列化改动让探针重新链接。
  三者在字节上不同、词汇上相同。这一条被记为 finding `binding_divergence`，不拼数字。
- **watcher 绑定是"自述"而非推断**：`run_watch_set.py` 没有传 `--json`，但每个进程的摘要、命令行、
  env 覆盖和开始时刻都写在 `watch-set.json` 的 `samples[]` 里，所以两组的绑定来自各进程自己报告的
  摘要，而不是从相邻组猜出来的。
- **重复性**：先登记的、被结论比较的量（`first_present_us`、`stage:playback_prepare`）满足
  `(max−min)/median ≤ 15 %`；不满足的组不删数字、不重采样，而是被专门造的组替换或降级为 finding。
  本轮 39 项自检全过，另有 9 条 finding（见 §11）。

## 5. 结果一：prepare 的分段成本

3 进程中位数，单位 µs，同一个 40K 项目，只有 player 二进制不同：

| 阶段 | before | after | Δ | Δ % |
|---|---|---|---|---|
| `json_dom_parse` | 720 669 | 730 610 | +9 941 | +1.38 |
| `chart_typed_read` | 345 446 | 334 906 | −10 540 | −3.05 |
| `chart_canonical_normalize` | 181 025 | 172 823 | −8 202 | −4.53 |
| `chart_canonical_dump` | 391 141 | 91 237 | −299 904 | −76.67 |
| `chart_canonical_write` | 649 232 | 343 941 | −305 291 | −47.02 |
| `load_canonical` | 16 949 | 17 128 | +179 | +1.06 |
| `load_chart` | 1 711 145 | 1 408 323 | −302 822 | −17.70 |
| `load_document` | 1 728 265 | 1 425 483 | −302 782 | −17.52 |
| `chart_compile` | 90 734 | 88 435 | −2 299 | −2.53 |
| `runtime` | 206 751 | 208 222 | +1 471 | +0.71 |
| `snapshot_layout` | 19 522 | 19 676 | +154 | +0.79 |
| `presentation` | 2 125 | 1 994 | −131 | −6.16 |
| `assemble` | 1 632 | 1 655 | +23 | +1.41 |
| `content_prepared`（总） | 2 055 866 | 1 748 435 | −307 431 | −14.95 |
| traced 区间之外 | 5 950 | 5 611 | −339 | −5.70 |

读法：

- `chart_canonical_dump` 是 `chart_canonical_write` 的子区间（before 391 141 / 649 232，after
  91 237 / 343 941），所以 −299 904 与 −305 291 不是两笔独立收益。
- `chart_canonical_write` 的 **self 时间反而从 74 677 升到 80 771 µs**（+8.2 %）：那部分不是写
  文本，是 `json::Value` DOM 的析构（约 1.12 M 个节点）。改写没有让它消失，也不该假装消失。
  self 是逐进程算 `total − 子区间` 再取中位数，所以它不等于"两个中位数相减"：649 232 − 572 166
  与 74 677 的差是取中位数的次序造成的，不是漏项。
- `load_chart` 与 `load_document` 的 self 只有 199 → 194 µs 与 177 → 202 µs，说明这两层基本由
  子区间构成，改写没有把成本挤到追踪范围之外。
- `json_dom_parse`（+1.38 %）、`chart_typed_read`（−3.05 %）、`chart_canonical_normalize`
  （−4.53 %）三段都没有被这次改动触及。前三段的抖动落在量级内，**不主张**序列化影响了它们。

首帧（进程启动 → 第一帧有效画面）：

| | before | after |
|---|---|---|
| 逐进程 µs | 2 506 576 / 2 476 121 / 2 469 176 | 2 148 938 / 2 144 992 / 2 150 103 |
| 中位数 | 2 476 121 | 2 148 938 |
| 离散度 | 1.51 % | 0.24 % |

中位数变化 −327 183 µs（−13.2 %）。到 R3 的首次加载钳位 2 000 000 µs 仍差 **148 900 µs =
148.9 ms = 7.4 %**。

这个差值是**距离陈述，不是门槛结论**：本臂是缓存不受控（cache-uncontrolled）测量，不是磁盘冷
加载，而且 40K 是极端压力项目，不是那次门槛定义的那个场景。owner 已判定 149 ms 的差距不值得为此
做大量重构，本轮据此停手。

## 6. 结果二：窗口响应

watcher 在进程外枚举窗口、取首个可见窗口的时刻、对非渲染线程发同步探测消息
（`SendMessageTimeoutW` + `SMTO_BLOCK|SMTO_ABORTIFHUNG`），并在每次启动前记录可执行文件摘要。
所有进程用同一个 40K 项目、同一个 release 二进制族。

| 组 | 分支 | n | 可见 s | 首帧 µs | 探测 p99 ms | 超时 | rc |
|---|---|---|---|---|---|---|---|
| w1 | `player-40k-untraced` | 3 | 2.4587 | 2 478 375 | 53.371 | 0 | 0,0,0 |
| w1 | `player-40k-traced` | 3 | 2.4461 | 2 476 635 | 44.163 | 0 | 0,0,0 |
| w1 | `probe-40k-static-untraced` | 1 | 2.4529 | — | 59.493 | 0 | 0 |
| w1 | `probe-40k-static-traced` | 1 | 2.4709 | — | 55.351 | 0 | 0 |
| w2 | `player-40k-untraced` | 3 | 2.4468 | 2 460 775 | 43.957 | 0 | 0,0,0 |
| w2 | `player-40k-window-first` | 3 | **0.3631** | 2 499 013 | **413.002** | **4** | 0,0,0 |
| w2 | `player-40k-window-first-close1` | 1 | 0.3771 | — | 413.474 | 4 | 0 |
| w2 | `player-40k-untraced-close1` | 1 | 2.4747 | 2 480 702 | 52.264 | 0 | 1（被墙钟杀掉） |

**结论一：40K 的"长时间不响应"发生在窗口存在之前，不是窗口出现之后被阻塞。** 默认顺序下窗口在
2.4587 s 才可见，首帧只比它晚约 20 ms。也就是说这条顺序上"窗口出现"不是响应期的开始，窗口一出现
准备就已经做完了。抖动离散度：可见 2.32 %、首帧 2.15 %。

**结论二：trace 臂的 p99 更低不是 trace 改善了响应。** 53.371 → 44.163 ms 的原因是 trace 让
prepare 更慢、窗口更晚出现、探测覆盖的可交互时间因而更短；`close→exit` 在 0.2258 s 与 0.2293 s
之间基本不动，说明退出路径不受 trace 影响。这个差值只说明测量窗口变了。

**结论三：窗口先行换到的是"窗口是否存在"，不是"准备期间是否泵消息"。** `w2` 两条分支配对：

- 可见窗口从 2.4468 提前到 0.3631 s（早 2.0837 s，−85.16 %），该分支自身离散度 2.89 %；
- 首帧净推迟 **+38 238 µs（+1.55 %）**。这个净值掩盖了一次搬运：默认顺序在 prepare 完成后才建窗
  （`content_prepared` 2 082 340 → `window_created` 2 387 740，+305 400 µs），窗口先行把建窗提到
  最前（324 942 µs），于是 prepare 完成的时刻反而晚 355 558 µs，而 prepare 之后不再需要建窗。
- 代价可分辨：探测 p99 43.957 → **413.002 ms**，2 s 预算内超时 0 → **4** 次，>100 ms 的探测
  1 → 5 次；0.5 s 窗口的 CPU 核比 0.9539 → **1.2562**，2 s 仍高 9 %（0.9830 → 1.0711）。
- 两臂首帧离散度分别是 0.40 % 与 1.88 %，1.55 % 的净变化大于任一臂的离散度，所以它可分辨；但两臂
  顺序不同，只能读成"同样的工作换了顺序，代价有界"，不能读成"窗口先行更慢"。

**结论四：两条 close 记录必须成对读。** `window-first-close1` 在 1.0025 s 投递 `WM_CLOSE` 时窗口
已存在（0.3771 s 可见），进程自行退出，`close→exit` **1.6893 s**、rc 0；
`untraced-close1` 在同一时刻投递，但默认顺序此时还没有窗口（2.4747 s 才可见），`PostMessageW` 从未
发生，于是 `close→exit 39.0001` 是 watcher 40 s 超时的余数、进程被强杀（rc 1）。后者**不是** close
延迟：`watch_window.py` 在 `target is None` 时也无条件写 `close_posted_at`，该字段没有前提校验，
因此被单列为 finding `close_without_target` 并从所有关闭结论中排除。

即使有效的 1.6893 s 也不能算通过：close 是在 prepare 结束后才被响应，超过 R3 的 Reset/Close ≤1000 ms
门槛。这里记录的是该顺序的限制，不是一次合格的关闭延迟。

**结论五（能力边界）：** 让可见窗口在准备期间真正响应，需要 SDK 级两段式 prepare——把"可以开始泵消息
的窗口"与"内容准备"拆成两个可交错的阶段。当前公共 API 不可达，本轮也不做的第二种线程方案已经用
`0xC0000409` 证明会炸（`smoke-wf-01` 保留在档案里，`smoke-wf2-01` 是无锁修复，两者都留档）。

因此窗口先行默认关闭：它把窗口的存在时间提前了 2 s，但没有把可交互性提前。

## 7. 结果三：追踪本身扰动了多少

`PREPARE_TRACE=1` 会往热路径插桩，所以必须先证明它不影响被测量的量。

| 组 | 排布 | 每臂 n | prepare 配对中位数 Δ | Δ % | 首帧配对中位数 Δ | Δ % |
|---|---|---|---|---|---|---|
| `a2` | 严格 ABAB，15 次切换 | 8 | −4 714.0 µs | −0.23 | −7 555.6 µs | −0.304 |
| `a` | 6 个一臂再 6 个另一臂 | 6 | −31 636.9 µs | −1.528 | −38 033.1 µs | −1.516 |

`a` 的批量排布把"插桩效应"与"机器随时间漂移"混在一起，无法分离，因此被 `a2` 取代，只作为被取代的
尝试保留、不参与任何结论；它的离散度也确实越过了 15 % 限。`a2` 的 −0.23 % 落在 1 % 门槛内，但配对
差的最大绝对值 118 656.4 µs（5.70 %）说明单对差值仍可到 ±5 %，所以本报告一律用**中位数**说话，不用
单次值。

## 8. 结果四：实体规模

| 实体数 | prepare µs | µs/实体 | 首帧 µs | 来源 |
|---|---|---|---|---|
| 1 800 | 78 258.5 | 43.4769 | 382 731 | b |
| 2 400 | 120 143.2 | 50.0597 | 438 279.9 | b |
| 2 800 | 137 279.3 | 49.0283 | 443 595.3 | b |
| 10 000 | 514 186.1 | 51.4186 | 849 520.6 | b |
| 40 000 | 2 059 251.1 | 51.4813 | 2 494 430.1 | a2 |

最大/最小单位成本比 **1.184**（限 1.5）。1800 点偏低是固定开销还没被摊开；2400 以上三点都落在
50 µs/实体附近。这张表能读出的只有一句：**40K 的成本是"实体数 × 约 51 µs"的线性区，不是 40K 特有的
异常**。所以"40K 差 149 ms"不能靠消掉某个 40K 专有分支解决，只能靠降低单位成本或降低实体路径本身的
工作量。

## 9. 归因：normalize 剩下的成本，以及下一个最便宜改动

`chart_canonical_normalize` 现在是 172 823 µs。下面的数字**不是仪器读数**，是对归档保存的那份输入图
做一次只读遍历、再对照 `engine/chart/src/chart_writer.cpp` 的 `canonicalize` 数出来的（命令见 §14）：

- 40K 输入图（`247cb8ef…`，12 571 828 B）有 **280 012 个对象、680 039 个键**。它就是被规范化的那份
  DOM：这张图是 `version: 1`，`loadChartForPlayback` 因此走非 v4 分支，把同一棵 `parsed` 树移交给
  `writeCanonicalValue`（`chart_v4_loader.cpp:895`–`:910`）；`chart_canonical_write` 只在该分支打点，
  它在 trace 里出现本身就说明走的是 legacy 路径；
- 其中 **240 010 个**对象不在 `extensions` / `metadata` 之下；只有这部分会被逐键探测，因为
  `canonicalize` 对 `opaque` 子树在递归后直接 `return`（`chart_writer.cpp:501`）；
- 每个被探测的对象做 **21 次** `find`：`normalizeRational` 的 `numerator` / `denominator` 2 次
  （`:335`、`:336`），`stringSortFields` 13 条（`:511`–`:525`），`segments` / `steps` /
  `tempoEvents` / `stops` 4 次（`:531`–`:546`），`properties` / `prefixes` 2 次（`:565`、`:570`）；
- 合计 **5 040 210 次**哈希查找，是键数的 **7.4 倍**；
- 这里传的是 `writeCanonicalValue` 的 `sortBehaviorArrays=false`（`chart_writer.cpp:700`），
  `events` / `stepEvents` 那 2 次**不执行**；走 `writeV4` 时才会变成 23 次；
- 另一半成本在排序：`objects` 数组有 40 000 条、`id` 全不相同（最大相等段 = 1），
  `sortByStringKey` 的比较器每次比较 2 次 `find`（`:397`、`:398`），按 n log₂ n ≈ 6.1×10⁵ 次比较
  估算约 **1.2 M 次**查找。40K 上不触发 decorate / serialise 分支（`:365` 的相等段长度都 ≤1），
  所以那段代码在这里不计费。估算部分已标明，不当作计数。

可省的下一个最便宜改动因此是明确的，但它**本轮没有实施**，只登记：

1. 把 21 次逐键 `find` 探测合并成一次键分类扫描（查找次数约降到 1/7.4）；
2. 排序装饰化：40K 上不触发相等段，所以只对普遍情形有意义，对 40K 几乎无收益——这条降级；
3. 可选：`chart_canonical_dump` 的 91 237 µs / 32 MB ≈ 350 MB/s 仍可换成指针游标写入器。

不实施的理由是优先级：这些属于 40K 极端情况的 prepare 成本，而 owner 的第 2 优先是 CPU 中性路径
（每 draw call 1.374 µs/实体的那条成本），第 3 优先才是用新鲜完整矩阵判断阶段门槛。把它们记进档案
比先做一半更有用，否则下一个接手的人会以为 normalize 已经"优化过了"。

## 10. 未采用的方向

| 方向 | 为什么不做 |
|---|---|
| SAX 式 typed reader 替换 DOM 解析 + typed read（合计约 1.07 s） | owner 明确否掉"为了指标做大量重构"；这是本轮最大的一块潜在收益，但也是最贵的一块 |
| 公共 `PlaybackPrepareOptions` 增加准备回调 / 分片钩子 | 会把内部阶段暴露成公共 API 契约，且 40K 是极端场景，不值得为它扩公共面 |
| 复用 `CUEXIS_PD_METRICS_TEST_ONLY` + `pd::Timer` 做启动埋点 | 那套设施服务于另一条测量线，混用会让两条线的数字互相引用，且它在 release 产品二进制里是关掉的 |
| prepare 放到 worker 线程 | `core::ThreadChecker` 使 off-thread 析构/移动直接 `std::terminate()`；已有 `0xC0000409` 反例 |
| 空读回的容量括号（`CUEXIS_GPU_PIXEL_READBACK=0`） | 是另一条诊断线，必须单独登记、单独跑，不能并入已冻结的 1080p 前缘 |

## 11. 九条 finding

自检 39 项通过、0 项失败，另有 9 条 finding。finding 不是失败，是必须连同数字一起读的限制：

| kind | 内容 | 为什么要记 |
|---|---|---|
| `binding_divergence` | 盘上的 probe 二进制不是分组测过的那一个 | probe 也链接 `cuexis_json_support`，序列化改动让它重新链接（`c5e1c317` → `c718206d`）。测过的源码未变，二进制摘要变了 |
| `superseded_group` | `a` 组是批量排布 | 无法与漂移分离，已被 `a2` 取代，只保留 |
| `superseded_group` | `a` 组越过 15 % 重复性限 | 同上；不删数字，但不用它下结论 |
| `instrument_limit` | 设备握手类 stage 宽于 15 % | 2 个毫秒级 stage（如 `stage:backend_create` 17.4 %），没有任何结论依赖它们 |
| `instrument_limit` | probe 组内的 traced prepare 拆分噪声过大 | 3 个 trace stage 超限（如 `trace:assemble` 16.63 %）；trace 拆分只用 `prepare-cost` 的 3 进程 stamp，不用 probe 组 |
| `instrument_limit` | 亚毫秒 stage 无法满足 15 % 相对限 | 92 个量中位数低于 1000 µs；相对限对它们没有意义 |
| `count_metric` | w1 默认分支的探测**计数**离散度越限 | 是计数不是时长；时长的重复性检查通过 |
| `close_without_target` | 1 s 的 close 投递没有落点 | 见 §6；该字段是超时余数，不是延迟 |
| `behavioural_limit` | 窗口先行的窗口可见但不泵消息 | 探测超时与 413 ms p99 的成因，也是该顺序默认关闭的理由 |

## 12. 非断言

1. 不是 240 FPS、1080p 容量前缘或任何 GPU 计时结论。本轮没有跑 GPU 计时、没有跑前缘括号。
2. **148.9 ms 不是"首次加载 ≤2000 ms"的通过或不通过**。它是 40K 极端压力项目上、缓存不受控测量
   到一个钳位值的距离；门槛只被引用来说明距离。
3. 不是 R3 / R4 / Stage 7 接受。D2 / D3 / Snapshot / 排队 p99、request→ready 与 GPU render p99、
   Pause / Resume / resize、Reset / Close、适用 Seek、正式读取相对门（Student-t 95 % CI 上界
   ≤1.10）、R3c 六步、P6 全矩阵都**没有**在本轮测量。
4. 两种启动顺序不可互换，窗口先行默认关闭，也没有被设为产品默认。
5. 探针盘上摘要 ≠ 测量摘要（`binding_divergence`）；本轮没有拼接、没有把两个构建的数字相加。
   probe 重建的触发**未归因**（b 与 w1 之间 +13 824 B），只靠 14/14 与 19/19 的词汇一致来支持
   "形式可比"。
6. 字节身份只覆盖被 migrator 处理的 11 个可规范化的 v3 图（22 个 fixture 中另 11 个是
   `unsupported` 的 v4 图，无法经 migrator 规范化）。v4 writer 的覆盖来自 `cuexis_chart_tests` 的
   golden canonical fixtures，不经 migrator；这个缺口写在档案正文里。
7. `chart_canonical_dump` 的 −76.67 % 与 `chart_canonical_write` 的 −47.02 % 是父子区间，不可相加。
8. 清理删掉的 8 个构建树在自身之外被引用，因此 run-3 的逐行 `binary_sha256` 是历史标识，不可再推出。
9. `untraced-close1` 的 `close→exit 39.0001 s` 是 watcher 超时余数，不是关闭延迟，已排除。

## 13. 已知缺口

- `run_watch_set.py` 没有传 `--json`：每进程摘要、命令行、env 和开始时刻都在
  `watch-set.json samples[]` 里，但 `events[]` 没有落盘。已记录，尚未改。
- 空读回分支（`CUEXIS_GPU_PIXEL_READBACK=0`）没跑。
- 40K 之外的窗口尺寸（480p / 1080p）没有在本轮复测。
- §9 的三项 normalize / dump 改动只登记、未实施。
- 本文档目录的计数（`../README.md` 的 r3 行）沿用上一批的增量约定记账；该列的绝对值此前已经漂移，
  本轮只做增量、不顺带重算历史行。

## 14. 复算

```
python -B verify/verify_startup_evidence.py                        # 在档案根目录
python -B verify/verify_startup_evidence.py <archive>              # 或显式给路径
python -B <archive>/instruments/tabulate_startup.py                # 在仓库根重算 staging 的 derived/
python -B out/verification-20261101/evidence-staging/r3-40k-startup/build_startup_evidence.py
```

前两条在档案内；第三条用的是档案里的 `instruments/` 副本，但它读的是
`out/verification-20261101/` 下的原始分组，所以必须在仓库根跑，写出的是 staging 的 `derived/`，
不是档案里的那两份；第四条只在 staging——`build_startup_evidence.py` 不在档案里，因为它的作用
就是写出档案本身。

本轮按这四条复算过一次。重跑写出的 `.json` 与 `.md` 除 `generated_at`
（`2026-11-01T05:45:54+00:00` → 本次 `2026-11-01T06:18:25+00:00`）之外与归档副本逐字节相同：
把所有时间戳掩掉后两边相等。因此摘要随生成时刻变（本次 `e26b9b03…` / `63074532…`），内容里
没有随机量。复算后 staging 的派生文件已按归档副本的 LF 字节写回（`4ec48d66…` / `1523e1da…`），
档案未被改动。这一条只说明派生物能从原始分组重建，不是新测量，也不进任何结论。

`derived/startup-cost-table.json` 是判据，阅读副本是
[derived/startup-cost-table.md](2026-11-01-40k-startup-and-prepare-cost-evidence/derived/startup-cost-table.md)，
两者冲突时以 JSON 为准。逐进程的命令
行、env 覆盖与摘要都在 `runs/<stamp>/` 的 `startup-set.json` / `watch-set.json` 里，重跑必须用同样的
路径和同样的保护变量。

§9 的对象 / 键 / 探测点计数（只读那份输入图，打印
`objects keys probed 21*probed objects_in_array distinct_ids`）：

```powershell
python -B -c @'
import json, pathlib
doc = json.loads(pathlib.Path("out/verification-20261101/r3-startup-observability-v1/"
                              "player-40k-project/assets/charts/stress_40k.cuexis.chart.json").read_bytes())
stack, objects, keys, probed = [(doc, False)], 0, 0, 0
while stack:
    node, opaque = stack.pop()
    if isinstance(node, dict):
        objects += 1
        keys += len(node)
        probed += 0 if opaque else 1
        stack.extend((v, opaque or k in ("extensions", "metadata")) for k, v in node.items())
    elif isinstance(node, list):
        stack.extend((item, opaque) for item in node)
ids = [o.get("id") for o in doc["objects"]]
print(objects, keys, probed, 21 * probed, len(ids), len(set(ids)))
'@
```

输出 `280012 680039 240010 5040210 40000 40000`。这是对**输入图**的计数，也就是被规范化那棵 DOM。
不要把 `prepare-cost/before/40k-canonical.json`（32 180 288 B，`0cf308c8…`）的对象/键数混进来：那是
migrator 把同一张 v1 图升到 v3 的输出，不是同一份文档（`version` 1→3、`timing/bpmChanges` 变成
`timing/tempoEvents`、behaviour 类型被改写），两者只共享"同一张图"这个来源。

## 15. 归档

附件：`2026-11-01-40k-startup-and-prepare-cost-evidence/`，**229 个文件**。

| 项 | 值 |
|---|---|
| 原始字节 → 归档字节 | 113 087 767 → 5 443 309 |
| 拷贝模式 | 215 text / 8 gzip / 2 binary |
| 自检 | **671** 项，失败 0 |
| 三份 40K canonical | 互相一致，且等于 `0cf308c8…` |
| tabulation 检查 | 39 项通过 / 0 失败，9 findings |
| 摘要闭包 | 正文引用的 14 个摘要全部有归属，未归属 0 |
| 独立校验器 | `verify_startup_evidence.py` 打印 `PASS` |

该档案重建了四次才同时通过自身检查和独立校验；四次的原因都写在 `manifest.superseded_notes` 里（两次
中途失败、一次自查不过、一次独立校验拒绝 `derived_files[].content_sha256`）。被撤回的上一棵树按
rename 保留在 `out/verification-20261101/evidence-staging/r3-40k-startup/` 下，没有入库，也没有
materialise 到 `docs/`。

三条 canonical 副本（`prepare-cost/before/40k-canonical.json.gz`、`before/work/…-t3.json.gz`、
`after/work/…-t3.json.gz`）是同一份 32 180 288 B 文本的三个独立副本，用来证明改写前后写出的字节
没有变化。归档里没有任何可执行文件：重建会原地覆盖，所以二进制只按摘要登记
（`binding.recorded_executables`，14 条全部可归属）。
