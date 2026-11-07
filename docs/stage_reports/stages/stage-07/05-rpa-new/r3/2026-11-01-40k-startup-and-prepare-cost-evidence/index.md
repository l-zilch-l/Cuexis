# 2026-11-01 40K 启动可观测性与准备成本冻结档案

本档案冻结 2026-11-01 的两组测量：**40K 极端压测项目**的启动阶段可观测性，以及同一项目上
**canonical 直接序列化改写**前后的 prepare 成本拆分。项目本身是 40 000 实体、静态呈现、
12 571 828 字节 chart 源，canonical 文本 32 180 288 字节。

- 归档构建器：`out/verification-20261101/evidence-staging/r3-40k-startup/build_startup_evidence.py`
- 归约工具：同目录 `tabulate_startup.py`，只读本档案内的文件，写出 `derived/`
- 逐文件绑定：`manifest.json`；构建时自检：`validation.json`；独立复算：`verify/verify_startup_evidence.py`

**这不是 240 FPS 容量测量，也不是任何阶段的正式验收。** 40K 是压测极端情况。本档案唯一与之相关的
产品门槛是 R3 的"初次 load ≤ 2000 ms"，而该门槛的标准场景是**普通 chart 的首次加载**；这里的数字是
压测项目上的、缓存不受控（不是磁盘冷启动）的测量，只能作为压力余量读，不能当作验收行。

## 1. 这份档案回答的两个问题

1. 40K 首次 load 还有多少余量，成本花在哪个阶段？
2. 启动阶段能不能从**进程外**看见（窗口、首帧、响应性），而不是靠进程内的自我报告？

第二个问题的直接动因：产品级 `cuexis_player.exe` 早就写出 `[player.startup_phase]` 行，缺的是
**探针**侧的阶段时间戳。补上探针之后，两条独立通道（进程内的 stage 戳、进程外的窗口观测）必须互相
对得上，这一点在本档案里被显式检查。

## 2. 一句话结论

- 直接序列化把首帧中位数从 **2 476 121 µs** 降到 **2 148 938 µs**（−327 183 µs，−13.2 %），距
  2000 ms 门槛 **148.9 ms**（7.4 %）；三个进程的离散度从 1.51 % 降到 0.24 %。省下的几乎全在
  32 MB canonical 文本的产出路径上：`chart_canonical_dump` −76.67 %、`chart_canonical_write` −47.02 %。
- 启动在进程外是可见的：默认顺序首个可见窗口中位数 **2.4468 s**、首帧 **2 460 775 µs**；把窗口提到
  prepare 之前（`CUEXIS_PLAYER_WINDOW_FIRST=1`）后窗口 **0.3631 s** 可见（−85.16 %），首帧只
  **+38 238 µs**（+1.55 %）。
- 但提前创建的窗口在 prepare 期间**不 pump 消息**：watcher 的 4 次探测超时、p99 **413.002 ms**。
  它买到的是"窗口存在"，不是"可交互"。该顺序默认关闭，且这一段与默认顺序**不是可互换的证据**。

## 3. 目录

| 路径 | 内容 |
|---|---|
| `instruments/` | 15 个被测工具与驱动脚本的原样副本：补丁、构建脚本、启动驱动、外部 watcher、归约工具 |
| `project/` | 40K 压测项目本体（project 描述、asset index、material、mesh、12.5 MB chart 源） |
| `runs/` | 10 组逐进程原始输出：`20261101-a`、`a2`、`b`、`c`、`w1`、`w2`、`watch`、`smoke`、`smoke-w2`、`serializer-direct-v1` |
| `prepare-cost/` | before/after 两臂的 stage 拆分 stamp、22 案例字节扫描、migrator 逐案例中间文件、构建与 ctest 日志 |
| `cleanup/` | 构建产物清理的清单与计划（**仓库卫生记录，不是测量**） |
| `derived/` | 归约结果：`startup-cost-table.json`（权威）与 `.md`（同内容的可读渲染） |
| `verify/` | 独立复算脚本，只依赖标准库 |
| `index.md` | 本文件，LF |
| `manifest.json` | 逐文件 sha256、来源路径、复制模式、绑定、环境 |
| `validation.json` | 构建时的自检结论；本档案为 `pass` |

复制模式有三种，`manifest.json` 的每条 `copied_files[]` 都记了 `mode`：

- `text`：统一规范为 CRLF，同时记 `sha256`（存档后字节）、`source_sha256`（源字节）、
  `content_sha256`（把存档字节按 LF 归一后的摘要，因此源文件即使混用行尾也能复算）。
- `gzip`：确定性 gzip（`mtime=0`、无文件名、level 9、固定 10 字节头）。三份 32 MB 的 canonical
  文本、以及三个日志走这条路径。
- `binary`：原样复制。

`.gitattributes` 把上述后缀全部标成 `binary`，只有 `index.md` 和它自己按 LF 检出，所以任何一次
重新检出都不会改掉被哈希的字节。

## 4. 首帧成本：改写前后

两臂是同一个 40K 项目、同一个 3 进程计划、同一个 `PREPARE_TRACE=1`，**只有 player 二进制不同**：

- before：`8591b67a…` 5 676 544 B，进程标签 `writer-split-00/01/02`，首帧 2 506 576 / 2 476 121 / 2 469 176
- after：`cb575e15…` 5 674 496 B，进程标签 `player-40k-traced-00/01/02`，首帧 2 148 938 / 2 144 992 / 2 150 103
- 中位数 2 476 121 → 2 148 938；离散度 1.51 % → 0.24 %（门槛 15 %）
- 门槛 2 000 000 µs，缺口 **148 900 µs = 148.9 ms = 7.4 %**

### 4.1 逐阶段（3 进程中位数，单位 µs）

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

`chart_canonical_dump` 是 `chart_canonical_write` 的子区间（before 391 141 / 649 232，after
91 237 / 343 941）。`chart_canonical_write` 的 self 时间反而从 **74 677** 升到 **80 771** µs
（+8.2 %）：那部分不是写文本，是 `json::Value` DOM 的析构（约 1.12 M 节点），改写并没有让它消失。
`load_chart` 与 `load_document` 的 self 分别只有 199 → 194 µs 和 177 → 202 µs，说明这两层基本由
子区间构成，改写没有把成本挤到追踪范围之外。

### 4.2 字节身份没有被牺牲

22 个仓库 chart fixture 在改写前后各跑一次 migrator，逐案例 `output_sha256` 全等（差异 0）。40K
案例的 canonical 文本仍是 `0cf308c86e0dcd98d63a01b2a342867cb6911bc976362da779e4de2d42656f52`
/ 32 180 288 B。本档案存了三份该文本（`prepare-cost/before/40k-canonical.json.gz`、
`before/work/…-t3.json.gz`、`after/work/…-t3.json.gz`），三份必须互相一致、且等于这个摘要。

`gen_serialize_golden.py` 从改写前的捕获 `serialize-capture.txt`（12 个案例）生成测试的
golden 头，因此新写入器的逐字节行为是被旧输出钉住的，不是被新写入器自己钉住的。

## 5. 窗口与响应：外部 watcher

watcher 在进程外枚举窗口、取首个可见窗口时刻、对非渲染线程发同步探测消息、并在每次启动前记录
可执行文件摘要。所有进程都用同一个 40K 项目、同一个 release 二进制族。

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

- 可见窗口相对默认顺序提前 **2.0837 s**（−85.16 %），该分支自身可见时刻离散度 2.89 %（默认顺序
  2.32 %）。
- 首帧净推迟 **+38 238 µs**（+1.55 %），但这个净值掩盖了一次搬运：默认顺序在 prepare 完成后才建窗
  （`content_prepared` 2 082 340 → `window_created` 2 387 740，+305 400 µs），device-first 把
  建窗提到最前（324 942 µs），于是 prepare 完成的时刻反而晚 **355 558 µs**，而 prepare 之后不再
  需要建窗。两臂的首帧离散度分别是 0.40 % 与 1.88 %，1.55 % 的净变化大于任一臂的离散度，说明它
  是可分辨的、但远小于 15 % 重复性限的代价；两臂顺序不同，不能把它读成"device-first 更慢"，只能
  读成"同样的工作换了顺序，代价有界"。
- 代价是 prepare 期间消息泵不动：4 次探测在 2 s 预算内没有回应，p99 抬到 413 ms；同一分支 0.5 s
  窗口的 CPU 核比是 1.2562（同组默认顺序 0.9539），2 s 窗口仍高 9 %。
- 两条 close 记录必须成对读：`window-first-close1` 在 1.0025 s 投递 `WM_CLOSE` 时窗口已存在
  （0.3771 s 可见），进程自行退出，`close→exit` **1.6893 s**、rc 0；`untraced-close1` 在同一时刻
  投递，但默认顺序此时还没有窗口（2.4747 s 才可见），`PostMessageW` 从未发生，于是
  `close→exit 39.0001` 是 watcher 40 s 超时的余数、进程被强杀（rc 1）。后者**不是** close 延迟，
  它被单列为 `close_without_target` finding，并从所有 shutdown 结论中排除。
- 即使有效的 1.6893 s 也不能算通过：close 是在 prepare 结束后才被响应，超过 R3 的 Reset/Close
  ≤1000 ms 门槛。这里记录的是该顺序的限制，不是一次合格的 close 延迟。

## 6. 追踪本身扰动了多少

`PREPARE_TRACE=1` 会往热路径插桩，所以必须先证明它不影响被测量的量。

| 组 | 排布 | 每臂 n | prepare 配对中位数 Δ | Δ % | 首帧配对中位数 Δ | Δ % |
|---|---|---|---|---|---|---|
| `a2` | 严格 ABAB，15 次切换 | 8 | −4 714.0 µs | −0.23 | −7 555.6 µs | −0.304 |
| `a` | 6 个一臂再 6 个另一臂 | 6 | −31 636.9 µs | −1.528 | −38 033.1 µs | −1.516 |

`a` 的批量排布无法把"插桩效应"与"机器随时间漂移"分开，因此它被 `a2` 取代，只作为**被取代的尝试**
保留，不参与任何结论；它的离散度也确实越过了 15 % 限。`a2` 的 −0.23 % 落在 1 % 门槛内，配对差
最大绝对值 118 656.4 µs 说明单对差值仍可到 ±5 %，所以本档案一律用**中位数**而不是单次值说话。

## 7. 实体规模

| 实体数 | prepare µs | µs/实体 | 首帧 µs | 来源 |
|---|---|---|---|---|
| 1 800 | 78 258.5 | 43.4769 | 382 731 | b |
| 2 400 | 120 143.2 | 50.0597 | 438 279.9 | b |
| 2 800 | 137 279.3 | 49.0283 | 443 595.3 | b |
| 10 000 | 514 186.1 | 51.4186 | 849 520.6 | b |
| 40 000 | 2 059 251.1 | 51.4813 | 2 494 430.1 | a2 |

最大/最小单位成本比 **1.184**（限 1.5）。1800 点偏低是因为固定开销还没被摊开；2400 以上三点都落在
50 µs/实体附近，所以这张表只能读成"单位成本没有随规模坍塌"，不能读成线性外推。

## 8. 九条 finding

自检 39 项通过、0 项失败，另有 9 条 finding。finding 不是失败，是必须连同数字一起读的限制：

| kind | 内容 | 为什么要记 |
|---|---|---|
| `binding_divergence` | 盘上的 probe 二进制不是分组测过的那一个 | probe 也链接 `cuexis_json_support`，序列化改动让它重新链接（`c5e1c317` → `c718206d`）。测过的源码未变，二进制摘要变了 |
| `superseded_group` | `a` 组是批量排布 | 无法与漂移分离，已被 `a2` 取代，只保留 |
| `superseded_group` | `a` 组越过 15 % 重复性限 | 同上；不删数字，但不用它下结论 |
| `instrument_limit` | 设备握手类 stage 宽于 15 % | 2 个毫秒级 stage，没有任何结论依赖它们 |
| `instrument_limit` | probe 组内的 traced prepare 拆分噪声过大 | 3 个 trace stage 超限；trace 拆分只用 `prepare-cost` 的 3 进程 stamp，不用 probe 组 |
| `instrument_limit` | 亚毫秒 stage 无法满足 15 % 相对限 | 92 个量中位数低于 1000 µs；相对限对它们没有意义 |
| `count_metric` | w1 默认分支的探测**计数**离散度越限 | 是计数不是时长；时长的重复性检查通过 |
| `close_without_target` | 1 s 的 close 投递没有落点 | 见 §5；该字段是超时余数，不是延迟 |
| `behavioural_limit` | device-first 窗口可见但不 pump | 探测超时与 413 ms p99 的成因，也是该顺序默认关闭的理由 |

## 9. 怎么复算

```
python -B verify/verify_startup_evidence.py            # 在本档案根目录
python -B verify/verify_startup_evidence.py <archive>  # 或显式给路径
```

它做的是：逐条比对 `manifest.json` 里每个文件的 `bytes` / `sha256` 并确认文件存在；对每个 gzip
重新解压并**重新压缩**以验证确定性（字节必须全同）与固定头；对每个 text 副本确认没有裸 LF、无 BOM、
且 `content_sha256` 复算一致；确认 `index.md` 是 LF 且摘要等于 `generated_files` 里记的值；确认
`.gitattributes` 含全部 9 条规则；确认没有多余或缺失文件；确认三份 32 MB canonical 文本互相一致且
等于 `binding.canonical_40k`；确认 `project/` 里的 chart 源解压后等于它自己的 `source_sha256`；
确认 `derived/startup-cost-table.json` 里 39 项 check 全为 true，且它的摘要同时等于
`manifest.derived_files[].content_sha256` 与 `binding.tabulated_sha256`；最后做摘要闭包——正文里引用
的每个 64 位十六进制串，必须要么在 manifest 里、要么在 `binding` 里，否则判失败。

`derived/startup-cost-table.md` 由同一份 JSON 渲染，是阅读用副本；两者数值冲突时以 JSON 为准。

## 10. 这份档案不建立什么

1. 不建立任何 240 FPS 容量点，不建立 R3 或 R4 的验收结论。
2. 不建立"40K 达到 2000 ms 门槛"。缺口 148.9 ms 是**未达到**，如实记录。
3. 不建立普通 chart 的初次 load。40K 是压测项目，且本测量不是磁盘冷启动。
4. 不建立 SDK 的初次 load。启动项目走的是产品级 player 路径，不是 SDK 安装消费者。
5. 不建立 device-first 顺序应该上线。它默认关闭，且与默认顺序不是可互换的证据。
6. 不建立"窗口可见即响应"。恰恰相反，§5 记录的是可见但不可交互。
7. 不建立 probe 二进制可复现。MSVC 链接不是逐字节可复现的，所以本档案绑定的是**源码与摘要**，
   不是二进制本身。

## 11. 已知缺口与未解释现象

- **`instruments/run_watch_set.py` 的存档副本比它驱动的两组新**（03:19:44Z 写入，w1/w2 已跑完）。
  它是同一作者的后一版；存档即事实，不假装是驱动那两组的字节。报告里列出差异。
- **before 分支没有启动脚本**。它由 watcher 直接拉起，命令行、cwd、环境变量只记在每个
  `raw/writer-split-NN.json` 的 `summary` 里；因此它是被"逐进程记录"绑定，不是被脚本绑定。
- **probe 二进制在 b 组与 w1 组之间涨了 13 824 字节，原因未归因**。能核验的是两点：19 个 prepare
  scope 的名字与顺序完全一致，14 个 stage 名字与源码完全一致；不能核验的是那次重链接的触发原因。
- **没有一次构建日志正好是产出 after 二进制的那个构建**。`build-before-serializer.log` 与
  `build-after-serializer.log` 都是 link-only 的增量过程，如实标注。
- **v4 chart 无法过 migrator**（11 个案例 `unsupported`），所以 22 案例字节扫描覆盖的是 v1→v3 的
  canonical 文本；v4 的写入器覆盖来自 `cuexis_chart_tests` 的 golden canonical fixture，本档案不
  重复它。
- **`untraced-close1` 的 `wm_close_to_exit_s` 字段本身是工具缺陷的产物**。字段会被无条件写入，即使
  投递从未发生；本档案不修工具（会改变已冻结工具的行为），而是把前提取出来单独判定。

## 12. `manifest.json` 与 `validation.json`

`manifest.json` 的键：`scope`、`source_run`、`runs[]`、`timer_runs[]`、`walks[]`、`copied_files[]`、
`attached_files[]`、`derived_files[]`、`instrument_versions[]`、`superseded_attempts[]`、
`superseded_notes[]`、`excluded[]`、`excluded_files[]`、`generated_files{}`、`binding{}`、
`environment{}`、`totals{}`。每个 `copied_files[]` 条目带 `source`（工作树路径）、`archive`（本档案
路径）、`mode`、`source_bytes`、`bytes`、`sha256`、`source_sha256`、`source_eol`、`eol`，text 模式
另带 `content_sha256`。

`binding{}` 里的每个值都是从本档案内的文件读出来的，没有手写常量：HEAD 与提交标题、probe 的源码
摘要与字节数、三组各分支记录的可执行文件摘要、序列化改动所在的提交、40K 案例的输入/输出摘要、
归约文件的摘要。

`validation.json` 记录构建时的全部自检：文件哈希、gzip 往返、gzip 确定性、纯 CRLF、内容往返、
三份 canonical 文本一致、归约 check 全通过、摘要闭包，以及文件集合无多余无缺失。任何一项不过，
构建器拒绝写出 `pass`，本档案也就不会存在。

### 被取代的尝试

保留而不删除，并在 `superseded_attempts[]` 里点名：`prepare-cost/before/prepare-split.original.json`
（第一版 stamp 把 after 二进制的摘要记到了 before 分支上，修正后保留原件）、`runs/20261101-a`
（批量排布）、`runs/smoke-w2`（device-first 的两次冒烟，其中一次是 `0xC0000409` 的失败尝试）。
