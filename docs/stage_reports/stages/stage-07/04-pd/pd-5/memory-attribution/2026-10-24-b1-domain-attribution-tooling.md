# PD-5 B1 域归因工具完成（静态度量，未运行）

日期：2026-10-24。范围：PD-5 B1 静态工具与门（观察器、探针 tag、分析器、打包约定、单测、宏关闭审计）。
本报告不运行任何工作负载，不产生性能或内存验收证据，不改变 B0 已接受账本。

## 1. 结论

| B1 项 | 状态 | 证据 |
| --- | --- | --- |
| 观察器 owner×domain 轴 | 完成 | 7×20=140 单元，容量 256，`sizeof(Header)` 仍为 56 B |
| 产品与探针 tag | 完成 | 产品 4 处、探针 17 处；全部在 `CUEXIS_ALLOCATION_LIFETIME_OBSERVER` 内 |
| 分析器 `collect-domains`/`check-domains` | 完成 | 30 项单测全绿 |
| 打包约定（`binding.json`、不可变回执） | 完成（仅约定，未生成运行回执） | §8 |
| 单测 | 通过 | `Ran 30 tests ... OK` |
| 宏关闭零代码审计 | 通过 | 产品侧新增 20 行全在 guard 内、0 删除；宏关闭编译三 TU 退出 0 |

B1 门判定：**通过**。B2、B3、B4 未授权、未运行。B0 账本四个 SHA 未变（`memory_caps.py`
`68e5c4fd…`、`memory_caps.v1.json` `a0fb8c68…`）。

## 2. 观察器与轴

- owner 轴：`probe`、`main`、`playback-worker`、`renderer-owner`、`audio-owner`、`audio-worker`、`unknown`。
- domain 轴：`fixture`、`load`、`decode`、`prepare`、`snapshot`、`advance`、`current`、`candidate`、
  `scratch`、`index`、`ready`、`history`、`receipt`、`journal`、`replay`、`gpu`、`audio`、`retained`、
  `static`、`other`。两轴字符串与 C++ 表逐字一致（本次用脚本比对通过）。
- tag 复用冻结 header 的签名槽：`packed = (magic<<32) | tag(16) | flags(16)`，`flagTagged=1`；
  `Header` 仍 56 B，未改分配器布局、未加锁、未加分配。
- 未打 tag 的分配落入 `unknown.other`，因此 `sum == global` 按构造成立；`exact` 反映单元表与
  全局账是否一致，`PD5_MEMORY` 的 `exact_partition` 仍为 `false`（`observer.py` 的既有要求）。
- 观察器路径仍无新分配：单元表为静态数组，`partition()` 在既有 spinlock 内拷贝。

## 3. tag 落点

产品侧（4 处，编译期受 guard 控制）：

| 位置 | owner.domain | 理由 |
| --- | --- | --- |
| `runtime_session.cpp` `buildEvaluationState` | `main.candidate` | 候选求值状态归会话所有，非调用线程临时量 |
| `playback_session.cpp` `prepareProjection` | `main.index` | 投影索引为会话级，按 `state.index` 计上限 |
| `playback_session.cpp` `buildSnapshotLayout` | `main.candidate` | 布局缓存按 `state.candidate` 计上限 |
| `playback_session.cpp` `PresentationAccess::materialize` | `main.snapshot` | 抽帧物化为每次调用的 snapshot DTO，非会话常驻 |

探针侧（`tests/pd/pd5_capacity_probe.hpp`，17 处）：`fixture-construction`/`encode` → `fixture`；
`fromPacked`/`snapshot-decode` → `decode`；`host-load` → `load`；`host-create` → `static`；
`host-advance` → `advance`；`host-query`/`snapshot*`/`seek`/`sample` → `snapshot`；`archive` →
`journal`；`evaluateReplay*`/`replayCut` → `replay`；`present` → `ready`；`close` 与释放阶梯不打 tag。
19 个 checkpoint 调用已统一为 `memory::checkpoint(...)`。

## 4. 分析器契约

`tools/verification/playback_performance/memory_domains.py`：

- `collect-domains <log> --json <out>`：解析 `PD5_MEMORY_DOMAIN`/`PD5_MEMORY_PARTITION`，逐行校验
  schema、`cell == owner.domain`、重复单元、负数、`live ≤ peak`、`requested ≤ storage`、空单元未发射，
  并记录源文件字节数与 SHA-256（`pd5.memory.domains.v1`）。
- `check-domains <log> --caps … --profile … --entities … [--json <out>]`：
  1. 每个 label 的分区 `exact` 为真，三项 `sum_* == global_*`，`cells` 等于实际发射行数；
  2. `untagged_live_count` 等于 `unknown.other` 的 live 计数且不超过全局；
  3. 若同 label 存在 `PD5_MEMORY`，域账与进程账逐项相等（双账互证）；
  4. 同一单元跨 checkpoint 的峰值不得回退（瞬态不得丢失）；
  5. `--require-labels`（默认 `before-load,after-close`）与 `--expect-zero`（释放后归零）可脚本化；
  6. `--max-age-us`/`--max-untagged-bytes` 由调用方显式声明，不设默认值（不复用观察器 900 s 语义）；
  7. 给定 caps 时按**峰值**（非 live）对照 `cap_count`/`cap_requested_bytes`；`NA` 单元必须为空；
     `duration` 单元不参与字节/计数对照。

包络超限是**可归因状态，不是解析失败**：`PD5_MEMORY.exceeded` 为真只记 warning 和
`envelope_exceeded_labels`，只有 `--strict` 才判失败。B3 的 40k 运行预期超限（3 GB 量级），
所以 40k 用非 strict 归因加 caps 对照，B2 的 64/2048 才要求 strict。

分类是全函数：每个 `(owner, domain)` 必属 `mapped`/`excluded`/`unrated`/`harness`/`untagged` 之一，
未知轴值直接失败。`mapped` 共 10 个 product 单元（`state.candidate`、`state.current`、`state.scratch`、
`state.index`、`history.request`、`delivery.receipt`、`journal.bytes`、`delivery.ready`、
`gpu.inflight`、`audio.live`）；`probe.*` 一律归 harness；B0 决策排除的 content 与 Snapshot/Replay
归档归 `excluded`。`--strict` 下仍缺可观测单元的**字节型** caps 单元必须用 `--uncaptured-ok`
逐项声明，声明会写入判定文档。

## 5. 覆盖缺口（B1 发现，需在 B2 前处置）

| 缺口 | 现象 | 处置建议 |
| --- | --- | --- |
| `state.world` | 域轴无 `world` 值，该 caps 单元无法由分配轴观测 | 加 `world` 域值或把 world 状态并入 `state.current` 归属；属探针/轴变更，B2 前置 |
| `delivery.payload`、`delivery.frame_bytes` | 帧载荷由 harness 的 `probe.ready` 承担，产品侧无对应 tag | B2 决定是否把 host 交付缓冲标为 `main.ready`；否则在报告中声明为 harness 计量 |
| `command.session_bytes`（G9） | OwnerChannel `SessionCommand` 存储无域轴，80 slots 参考量级 ≈ 3.0 MB | 需新增 `command` 域或显式 `--max-untagged-bytes` 允许量；B2 前置 |
| 计数型 caps 单元（`command.*`、`consumer.slots`、`steps`、`observation.*`、`delivery.attempt` 等） | 结构槽位计数不是堆分配，分配轴不可见 | 由 `PD5_STRUCTURE` 覆盖，已在分析器中明确排除出「未覆盖字节单元」判定 |

## 6. 观测阶梯缺口

现有 label 与计划 §3.4 的阶梯不一致：缺 `after-load`、`after-prepare`、`after-snapshot`、
`after-advance`、`after-replay`、`after-all`；释放阶梯用 `after-decoded-snapshot-release` 等命名，
而计划写作 `after-release:decision`/`after-release:expected`/…；另有计划外的 `after-pause`、
`after-replay-cut`、`after-seek-same-result`、`active`。B2 建议先做仅探针的 label 对齐（不改产品、
不改判据），再采数；分析器已用 `--require-labels`/`--expect-zero` 承接，不需要为改名改工具。

## 7. 宏关闭零代码审计

- `git diff -U0` 统计：`playback_session.cpp` 新增 15 行、删除 0；`runtime_session.cpp` 新增 5 行、
  删除 0；逐行检查**全部**位于 `#if defined(CUEXIS_ALLOCATION_LIFETIME_OBSERVER)` 之内，guard 外新增 0 行。
- 用 `out/build/pd5-stage-c-snapshot-release-20261023-54e8c2a/compile_commands.json` 的真实 Release
  参数去掉 `-DCUEXIS_ALLOCATION_LIFETIME_OBSERVER=1` 重编 `playback_session.cpp`、`runtime_session.cpp`、
  `pd5_matrix_probe.cpp`：退出 0，无告警升级。
- 宏关闭时 `TagScope` 为空结构、`checkpoint()`/`structure()` 为空函数、`partition()` 仅声明，
  不产生任何分配、分支或字符串。

## 8. 打包约定与 B2/B3 运行配方（未授权、未执行）

`binding.json` 字段（新建运行目录，一次性运行，不重试）：

- `source`：两树路径、分支、HEAD、`git status --porcelain` 摘要、产品源文件 SHA-256 清单；
- `tools`：`memory_caps.py`、`memory_caps.v1.json`、`memory_domains.py`、探针/观察器 SHA-256；
- `build`：构建目录、CMake preset 与完整编译/链接参数、编译器版本、`compile_commands.json` SHA-256；
- `inputs`：profile 名、`content_entities`、低/高规模、fixture 与内容 hash、`PD5_MEMORY` 上限参数；
- `run`：单次运行的 argv、开始/结束时间、退出码、日志 SHA-256；
- `receipt`：上述内容的规范化 JSON 加 SHA-256，写入后只读；失败即作废重来，**不重试到通过**。
- 产品源变化即生成新 binding；运行前完成完整 Release CTest。

B2/B3 采数后（授权后）的判定命令形如：

```text
python -B tools/verification/playback_performance/memory_domains.py check-domains <log> \
  --caps tools/verification/playback_performance/memory_caps.v1.json \
  --profile <profile> --entities <n> --expect-zero <final-label> [--strict] --json <verdict>
```

**B2 入口待裁**：caps 只登记了 14 个参考 profile（1、200、201 entities），没有 64/2048/40k 规模。
计划 §3.7 的 B2 门「上限通过」在当前登记下不可评估。三个选项：
(1) 新增派生工具，仅 import B0 冻结常量生成 `derived_scale` caps（`formal_registration:false`，
不改 B0 两文件），B2/B3 用它做归因对照、不作为接受证据；
(2) 把 B2/B3 的门收窄为「分区精确 + 最终归零 + 未打 tag 不增长」，不比 caps；
(3) 在 `profiles.json` 注册 64/2048 规模 profile（改动测量协议登记表，最重）。
建议 (1)。同一裁定还需覆盖 40k：P8 已判内存「未测量」，B3 的用途是补齐归因，不是翻案。

## 9. 证据

| 文件 | SHA-256（前 8） | 规模 |
| --- | --- | --- |
| `tools/verification/playback_performance/memory_domains.py` | `d2c3e22e` | 21,609 B / 440 行 |
| `tests/pd/pd5_memory_domains_tests.py` | `52a283bc` | 16,948 B / 311 行 |
| `engine/core/internal/cuexis_internal/allocation_observer.hpp` | `82681bc3` | 4,429 B |
| `tests/pd/pd5_memory_observer.cpp` | `4e3921cc` | 16,136 B |
| `tests/pd/pd5_capacity_probe.hpp` | `ba90e5d0` | 14,467 B |
| `engine/playback/src/playback_session.cpp` | `b56950b5` | 238,450 B |
| `engine/runtime/src/runtime_session.cpp` | `44488b6a` | 84,996 B |

单测：`python -B -m unittest discover -s tests/pd -p 'pd5_memory_domains_tests.py'` → 30 项通过。
分析器仅对合成日志做过端到端冒烟，**没有**对任何真实观察器日志运行过。

## 10. 非声明

- 未运行 64/2048/40k 采数，未产生分区精确、归零或上限通过的运行证据。
- 未重新测量性能，不改变 PD-5「未通过但定案」的判定。
- 未修改 B0 账本、caps 文件、判据阈值或冻结验收实现。
- 产品 tag 只影响观察器构建的归因，不改变产品行为；宏关闭时为零代码。
- 本报告不构成 Stage 7-PD、Stage 7、RPA 或生产的关闭或接受。
