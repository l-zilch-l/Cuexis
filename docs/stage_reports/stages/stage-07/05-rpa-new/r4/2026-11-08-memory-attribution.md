# R4 内存归因：逐域精确分区与 proposals 的落差

状态：candidate measurement；逐域归因已完成，声明的上限仍是 proposed（未接受），因此不是接受裁定

日期：2026-11-08

返回 [New RPA 报告索引](../README.md)。当前结论只以
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md) 为准。本文记录 owner 豁免额度后的内存
有限批次：方法、逐域结果、`state.current` 的超出量、以及 P8/40K 的具体阻断点。原始字节见
[内存证据包](2026-11-08-memory-attribution-evidence/index.md)。

## 1. 授权与范围

owner 于 2026-11-08 明确指示内存归因与 P8 可以继续、不考虑额度限制（记录在
[owner 豁免](2026-11-08-owner-attempt-waiver.json)）。计划 §3.4 要求“新的 memory/P8
workload/诊断另行裁定”，本批即为该裁定下的有限诊断，范围写在
`out/evidence/r4/memory-20261108/authorization.json`：每个（实体数, 复用）组合一个进程、
observer 构建、600 秒超时、不接受也不修改任何上限。

## 2. 方法与准入

1. 候选 `51b0ebd5` 全新 observer 构建（`CUEXIS_ALLOCATION_LIFETIME_OBSERVER` +
   `CUEXIS_EXECUTION_ALLOCATION_OBSERVER`，其余插桩关闭），绑定在
   `out/evidence/r4/memory-20261108/build-memory-v2`。
2. 先跑四个 observer 合同用例（`positive`、`unknown-root`、`root-capacity`、`page-overrun`），
   五条语义证明的唯一下界 multiset 为 `{0, 176, 784, 8192, 12841}`，与既有审计基线一致；
   合同通过后才允许容量进程。
3. 每个用例跑一次 `PD5 full content Snapshot Seek Replay boundary`，环境为
   `CUEXIS_PD5_ENTITIES`、`CUEXIS_PD5_REUSE`、`CUEXIS_PD5_CAPSULE_REVISION=3`。
4. 解析器只接受**精确分区**：`exact=true`、`untagged_live_count=0`、域字节之和等于全局
   分区、域计数之和等于全局计数。任一条不成立就拒绝归因，不把部分和当成整域。

## 3. 逐域结果

分区是“全部已链接的 C++ new/delete”，因此每个域的和就是该域在 `active` 检查点的全部 C++
请求字节（不含直接 malloc、SDL、驱动、GPU 与 OS 堆）。对照的是
[预算提案](../../../../../stage_plans/active/stage-07/04-pd/pd-5/pd5-measurement-proposal.md)
里 `r4-reference-nN-v1` 的 proposed 上限。

### 3.1 n64（high 与 low 一致）

总计 32,849,753 B（8,044 个活动分配），未分类 381,682 B。

| 域 | 观测（B） | 上限（B） | 比值 | 状态 |
| --- | ---: | ---: | ---: | --- |
| **state.current** | **427,015** | **196,608** | **2.172** | **超出** |
| command.session | 20,024,965 | 21,854,208 | 0.916 | 未超（最接近） |
| steps | 7,529,781 | 15,517,696 | 0.485 | 未超 |
| state.world | 202,768 | 327,680 | 0.619 | 未超 |
| 其余 19 个域 | — | — | ≤0.227 | 未超 |

`state.current` 在 n64 就超了 230,407 B（2.17 倍）。它不是测量噪声：high 与 low 两次的
该域字节完全相同（427,015 B）。

### 3.2 n2048

总计 96,859,264 B，未分类 10,151,117 B。

| 域 | 观测（B） | 上限（B） | 比值 | 状态 |
| --- | ---: | ---: | ---: | --- |
| **state.current** | **13,249,941** | **4,259,840** | **3.110** | **超出** |
| **command.session** | **27,049,016** | **21,854,208** | **1.238** | **超出** |
| content.decoded | 19,725,488 | 75,497,472 | 0.261 | 未超 |
| state.world | 1,826,538 | 8,454,144 | 0.216 | 未超 |
| 其余 19 个域 | — | — | ≤0.125 | 未超 |

`state.current` 的观测随实体数近似线性：n64 约 6.7 KiB/实体、n2048 约 6.5 KiB/实体，而提案
系数是 2 KiB/实体加 64 KiB 固定项。两个尺度都超出，说明问题在系数本身，不是某个窗口的噪声。

`command.session` 只在 n2048 超出：提案按“128 个命令槽 + 16 MiB 嵌套负载”给固定额度，
而实测嵌套负载随内容规模增长（n2048 时该域 27.0 MB）。这需要新的系数或一个随 N 增长的项。

## 4. P8/40K 的阻断点

`CUEXIS_PD5_ENTITIES=40000` 的进程在 29.8 秒内以退出码 42 结束，失败发生在内容构造阶段：

```
packed.budget.strings: Packed string budget exceeded
tests/rpa/execution_fixture.hpp(135)
```

根因是产品侧的打包字符串预算 `limits.maxPackedStrings = 100000`
（`engine/chart/include/cuexis/chart/limits.hpp`），而该 fixture 在 40K 实体下按实体生成唯一
字符串，去重后超过 10 万条。这是**内容设计**与产品上限的冲突，不是内存观测器的问题；
按登记的停止规则，本批不重试 40K，原样保留失败现场。

因此 P8 的 40K low/high 还没有测量结果；要取得结果，需要下列之一并由 owner 裁定：
把 fixture 的字符串改为可复用的池（不改产品上限），或经 caps 接受提高
`maxPackedStrings`（改变产品上限）。

## 5. 这些数字证明了什么、没证明什么

- **已证明**：在精确分区下，n64 与 n2048 的 `state.current` 都超过提案上限；n2048 的
  `command.session` 也超过。这是“观测值 > 声明值”的直接比较，不依赖下界外推。
- **未证明/未接受**：提案上限本身仍是 `status: proposed`，O1 只接受了延迟类数值；因此这些
  超出是“对提案的超出”，不是“对已接受预算的违反”。接受、修正系数或做存储整改都属于 owner
  的裁定范围。
- **未测量**：GPU 与驱动内存（`gpu` 域按设计不测量）、直接 malloc/SDL/OS 堆、prepare 峰值、
  observer 自身扰动。`unclassified` 在 n64 为 0.38 MB、n2048 为 10.2 MB，是 fixture/其他
  来源，不冒充产品域。
- **P8**：40K 未取得数据，阻断点已定名（打包字符串预算 100,000）。

## 6. 证据

| 内容 | 位置 |
| --- | --- |
| 授权与停止规则 | `out/evidence/r4/memory-20261108/authorization.json` |
| observer 构建与绑定 | `out/evidence/r4/memory-20261108/build-memory-v2` |
| observer 合同四用例与小结 | `out/evidence/r4/memory-20261108/contracts` |
| n64 high / low | `out/evidence/r4/memory-20261108/n64-high`、`n64-low` |
| n2048 high | `out/evidence/r4/memory-20261108/n2048-high` |
| 40K 失败现场（不重试） | `out/evidence/r4/memory-20261108/n40000-high` |
| 控制器（evidence 侧，参与哈希） | `out/evidence/r4/memory-20261108/run-census.py` |
| 可复用归因模块与测试 | `tools/verification/execution_acceptance/memory_attribution.py`、`test_memory_attribution.py` |
