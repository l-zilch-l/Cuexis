# P8 40K 实体内存（内容行受限）

状态：evidence；给出 40K 实体下的实测与结构归因，不含上限接受裁定

日期：2026-11-10

候选：`2c4e1274b02415c288cc756dcd7dfef5c59a0f01`（`26.11.09-1`）

返回 [内存上限修订提案](2026-11-08-memory-budget-revision.md)与 [New RPA 报告索引](../README.md)。原始字节位置见\n[P8 证据包](2026-11-10-p8-40k-entity-memory-evidence/index.md)。

## 1. 为什么内容行是 4,096

`maxPackedStrings = 100000` 是冻结的 Foundation 门（`engine/chart/include/cuexis/chart/limits.hpp`
里 `min(requested, frozen default)`，调用方只能收紧）。原 40K 内容（40,000 实体 + 40,000 需求，
每行至少三个唯一字符串）在编码阶段越门并退出 42。

本批把**内容行数**显式降到 4,096，实体数仍来自源 chart 的 40,000：

- World 侧实体 = 40,000（`sparseSource(scale)`），直接决定 `state.world`/`state.current`；
- 需求/声明/owner 行 = 4,096（`CUEXIS_PD5_REQUIREMENTS`），决定 `steps`/`journal`/打包字符串；
- `maxFrameMembers` 仍为 40,000（每实体至多一个帧成员，P8 的声明口径）。

因此本报告只对**实体缩放**的域构成 P8 结论；行缩放域偏小，不能按 40K 行解读。

## 2. 运行

- 构建绑定：`out/evidence/r4/memory-40k/build-binding-3/`（Release，内存观察模式，源 SHA 同上）；
- 环境：`CUEXIS_PD5_ENTITIES=40000`、`CUEXIS_PD5_REQUIREMENTS=4096`、`CUEXIS_PD5_REUSE=high`、
  `CUEXIS_PD5_CAPSULE_REVISION=3`；
- 结果：exit 0，7.3 s，单进程未替换；分区精确（`exact=true`、`untagged=0`、33 cell、637,027 live），
  总量 365,084,102 B，未分类 21,568,715 B。

## 2.1 归属修正后的同一运行（2026-11-10 晚）

下面 §3 的 7.36× 是**修正前**的记账结果。逐分配追踪证明该域的 160.8 MB 里约 141 MB 是采样/前进期间物化的
执行内核状态，被 `dispatch()` 的单个命令级标签记到了命令信封名下（见 §4/§5）。把
`SessionUpdatePresentation` 与 `SessionSeek` 两个分支改为执行域作用域后，**同一份构建、同一用例、同一绑定**
（总字节数完全不变，仍是 365,084,102 B）得到：

| 域 | 观测 | 声明上限 | 倍率 |
| --- | ---: | ---: | ---: |
| `command.session` | 19,762,486 B | 21,854,208 B | **0.90，在声明内** |
| `execution.retained` | 141,080,535 B | 1,327,497,216 B | 0.11，在声明内 |
| `state.world` | 33,125,473 B | 163,905,536 B | 0.20 |
| `state.current` | 26,445,893 B | 81,985,536 B | 0.32 |
| `state.candidate` | 20,641,094 B | 163,905,536 B | 0.13 |
| `content.decoded` | 39,168,683 B | 1,319,108,608 B | 0.03 |

即 21 个域**全部在声明内**，`over_declared` 为空。字节没有被移动或减少，只是记到了它所属的域。

## 3. 结果（profile `r4-reference-n40000-v1`）

| 域 | 观测 | 声明上限 | 倍率 |
| --- | ---: | ---: | ---: |
| `command.session` | 160,834,198 B | 21,854,208 B | **7.36×** |
| `state.world` | 33,125,473 B | 163,905,536 B | 0.20 |
| `state.current` | 26,445,893 B | 81,985,536 B | 0.32 |
| `state.candidate` | 20,641,094 B | 163,905,536 B | 0.13 |
| `content.decoded` | 39,168,683 B | 1,319,108,608 B | 0.03 |

21 个域里只有 `command.session` 超出；World、快照与解码侧都在声明内。

## 4. `command.session` 的构成（逐分配实测）

2026-11-10 给观察器加了按域逐分配输出后（证据在 `out/evidence/r4/mem-trace/index.md`，见
[内存上限修订提案](2026-11-08-memory-budget-revision.md)），该域只有 118 个存活分配，构成可以直接逐项读出，
不需要拟合：

| 分配 | 大小 | 归属 |
| --- | ---: | --- |
| 128 × `SessionChannelSlot`(39,664) + 39 | 5,077,031 B | `main.command-session`，命令信封本身 |
| 保留的 coherent 前进记录 65,536 × 224 + 39 | 14,680,103 B | `playback-worker.command-session`，实时前进的保留窗口 |
| 其余小容器/字符串 | 9,616 B | 同上 |
| **固定合计** | **19,766,750 B** | 与早先的两点拟合（19.83 MB）相差 0.3% |
| 每实体 7 个连续数组 × (N+1) | **3,524 B/实体** | `playback-worker.command-session`，执行内核状态的物化 |

每实体的 3,524 B = 3,096 + 200 + 64 + 64 + 48 + 44 + 8；40K 时最大一项是
`40,001 × 3,096 + 39 = 123,843,135 B`，n2048 时同一项是 `2,049 × 3,096 + 39 = 6,343,743 B`。
七项在同一次 `sample` 调用区间内出现：`after-seek` 检查点时该域仍是 19.8 MB，`after-sample` 时已是
145.7 MB。

**更正**：本报告早先把这项归因为“交付缓冲按 `maxFrameMembers` 声明容量分配”是错的。
`FrameStorage` 只记账不分配（`app/execution_host/frame_storage.hpp` 的构造函数只保存 `maxCount_`/
`maxBytes_` 与 `StorageMetrics`），`frameByteBound` 在整个仓库里只被比较和返回，从未作为缓冲大小使用。
3,894 B/实体与 4,096 B/实体“同阶”是巧合，不是同源。

## 5. 这个超限是什么性质

- 字节是真的：它们确实存活、确实被记账，并在 `close` 后归零（`after-close` 只剩 5,072 B）。
- 但归属是粗的：`session_worker_dispatch.cpp` 的 `dispatch()` 用**一个**命令级
  `ExecutionTagScope{playback-worker, command.session}` 包住所有命令，于是采样/前进期间物化的执行内核状态
  也被记到了“命令信封”名下。
- 定位过程：先按检查点二分——`after-seek` 时该域只有 19.8 MB，`after-sample` 时是 145.7 MB，缺口在
  `host->sample()` 区间内；再逐分支加执行域作用域验证，`SessionUpdatePresentation` 一次性迁出
  125,951,111 B，`SessionSeek` 迁出 15,129,424 B，合计 141,080,535 B，余下 19,762,486 B 正是命令槽
  与保留前进记录本身。中间试过的 realtime-start、advance、request-frame 三个分支都没有迁移任何字节。
- 结论：`command.session` 的 7.36× 不是“命令信封需要 160 MB”，而是“约 141 MB 的执行内核状态被记在了
  命令信封名下”。修正后同一运行 21 个域全部在声明内（§2.1）。

因此本批**不写**新的数值上限，也**不修改** `budget-proposal.v1.json` 或 O1 接受记录：修正的是记账归属，
不是声明额度。是否接受这份归属修正，仍属 owner 的签署范围。

## 6. 边界

- 只覆盖已链接的 C++ `new`/`delete`；直接 malloc、SDL、驱动、GPU 与 OS 堆不在内
  （`gpu` 域为 `device_domain_not_measured`）。
- 内容行 4,096 在本批是显式选择；2026-11-11 owner 将 `(40000, 4096)` 定为 **P8 正式内容口径**
  （[裁定记录](2026-11-11-owner-decisions.md) §3），扩充规则见该记录：二维声明、按系数推算上限、
  内容行受 `maxPackedStrings` 约束。
- 未做 low 复用变体；未做 40K 行（会再次触发冻结字符串门）。
- 数值与 `out/evidence/r4/memory-40k/n40000-r4096-high-3/attribution.json` 逐字节一致；
  同一内容在前一 SHA（`67e905be`）上的复跑结果相同，说明该量可重复。

## 2026-11-14 字节来源勘误

本节修正归属修正数字的证据引用，不撤销 2026-11-11 owner 已接受的命令／执行域归属修正及
P8 (40000,4096) 内容口径，也不修改设计 caps。旧 high-2/high-3 的 attribution.json 字节相同，
仍记录修正前 command.session=160,834,198 B、execution.retained=8,823 B、total=365,084,102 B；
high-3 绑定不能直接证明正文中的修正后数字。修正后 command.session=19,762,486 B、
execution.retained=141,080,535 B 的直接原始来源为独立同 SHA f5f91916 的 low/high-control 对，
其 23 个声明行差值为零，总差 6,060 B 归于 unclassified fixture。详见
[容量与 Player 有限证据](2026-11-14-capacity-and-player-bounded-evidence.md)。

最早 high 的 controller 原注册 3178 B，现路径为 3280 B 且哈希不同，原注册字节未从该路径恢复；
最早可识别失败 packet 的 exit 42 对应 misses == scale 断言，与另述 packed-string 失败分开。
历史五份 invocation packet 及额外 high 调用次数处置保持各自身份，不因本勘误恢复额度或取得正式准入。
