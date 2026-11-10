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

## 3. 结果（profile `r4-reference-n40000-v1`）

| 域 | 观测 | 声明上限 | 倍率 |
| --- | ---: | ---: | ---: |
| `command.session` | 160,834,198 B | 21,854,208 B | **7.36×** |
| `state.world` | 33,125,473 B | 163,905,536 B | 0.20 |
| `state.current` | 26,445,893 B | 81,985,536 B | 0.32 |
| `state.candidate` | 20,641,094 B | 163,905,536 B | 0.13 |
| `content.decoded` | 39,168,683 B | 1,319,108,608 B | 0.03 |

21 个域里只有 `command.session` 超出；World、快照与解码侧都在声明内。

## 4. `command.session` 的构成

固定部分是 128 × `SessionChannelSlot`（39,664 B）= 5,076,992 B，与 `main` 观测的 5,077,031 B
只差 39 B 容器头。

`playback-worker` 侧 155,757,167 B ≈ 3,894 B/实体。该量与
`frameByteBound(objects, resources, members)`（`app/execution_host/frame_delivery.cpp`）同构：

- `objectBytes = 2·sizeof(ObjectSnapshot) + 4·(2·256 + 32) = 2·320 + 2,176 = 2,816 B`
  （`ObjectSnapshot` = 320 B，已在 `element-sizes-msvc-release.v1.json` 的 `FrameObject` 行登记）；
- `memberBytes = 2·sizeof(FeedbackId) + 2·(2·256 + 32) = 192 + 1,088 = 1,280 B`；
- 合计 **4,096 B/实体**，与观测的 3,894 B/实体同阶。

即交付缓冲按 `maxFrameMembers` 声明容量分配，`command.session` 的声明上限缺了这一项。n2048 观测到的
约 3.5 KB/实体增长是同一项的线性部分。

## 5. 还没有写进上限的原因

把 `command.session` 的声明式上限改成“固定槽位 + k·maxFrameMembers”需要先确定 k 的**实例数**
（同一进程里同时存在几份这样的交付缓冲、在哪个检查点物化）。当前观察器只输出域级总计与世界侧
分配行，没有 `command.session` 的逐分配记录，因此本批不写新的数值上限，也不修改
`budget-proposal.v1.json` 或 O1 接受记录。需要的下一步是给该域增加逐分配归因后重跑一次 n2048 与
40K（同一候选、同一绑定方式）。

## 6. 边界

- 只覆盖已链接的 C++ `new`/`delete`；直接 malloc、SDL、驱动、GPU 与 OS 堆不在内
  （`gpu` 域为 `device_domain_not_measured`）。
- 内容行 4,096 是本批的显式选择；把它写成 P8 的正式内容口径需要单独接受。
- 未做 low 复用变体；未做 40K 行（会再次触发冻结字符串门）。
- 数值与 `out/evidence/r4/memory-40k/n40000-r4096-high-3/attribution.json` 逐字节一致；
  同一内容在前一 SHA（`67e905be`）上的复跑结果相同，说明该量可重复。
