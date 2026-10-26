# 2026-10-24 P8 40k 单侧完整规模定案表

**状态**：定案（证据登记与判定）；**运行**：否，本表不重跑 40k、不补窗口、不重建冻结构建。**PD-5 acceptance**：not passed。**Stage C / RPA / 生产 / Stage 7**：均不因本表关闭。

## 1 边界与本表位置

[测量方案](../../../../../../stage_plans/active/stage-07/04-pd/pd-5/pd5-measurement-proposal.md) §2 L46 把 P8 定义为「RPA-F3 回绑：40,000 两种 reuse 内容、Packed、Snapshot/Seek、Replay」，并要求「另行完整规模证据；不以 P0–P7 代替」。本表就是对这一项的单侧完整规模定案。

三条 owner 裁决构成本表的前提，全部为 2026-10-24 的决定，本表不重新讨论：

1. 40k 的 seek 计入「适用 Seek ≤20 ms」判据，并记录超界约 320×。
2. 不为 40k 补重复窗口，按单侧完整规模证据登记。
3. 静态内存上限账本（B0）先做，正式窗口后置。

「单侧」在这里是精确说法，不是修辞：完整规模只在一个时点上取证，low/high 两个进程各一次，是**两种 reuse 内容**而不是同一内容的重复。它因此能支持完整规模下的功能、格式与物理预算判据，以及 owner 已裁定计入的 seek 判据；它不能支持[测量方案](../../../../../../stage_plans/active/stage-07/04-pd/pd-5/pd5-measurement-proposal.md) §5 L129 的绝对延迟窗口统计（该窗口三个进程 p99 的中位数）与 L130 的相对回归配对区间。本表不产出任何窗口统计，也不使用 15% 重复性门——那两个门需要 3 窗口 × 3 对，40k 侧没有。

## 2 证据身份

| 身份 | 值 |
| --- | --- |
| 主树 | `D:/Cuexis-worktree`，`stage-7`，`eed36648a692333e47f798aa45be36ca850d08de`，干净 |
| 隔离树 | `D:/Cuexis-pd5-40k-admission`，`codex/pd5-gameplay-40k-admission` |
| 本表整理时的隔离树 HEAD | `3e5837f63f76397429c9f7603fd7450d3ce9855d`，含测试专用诊断插桩（未提交） |
| 40k 审计时的隔离树 HEAD | `08e145cad6eeff1f3c171b3c346fc1f1f608f946`，干净；原始功能结果以此为准 |
| native source | `54e8c2a1d770a90e1d7a2728f00510b1b19aabaf`，本批只有报告层差异 |
| 冻结构建 | `out/build/pd5-stage-c-snapshot-release-20261023-54e8c2a`；Release STATIC、candidate ON、lifetime observer ON，metrics/trace/execution diagnostics OFF |
| host probe SHA-256 | `6722c6e86a0908d6375ad0022b0a503d7b9a99b534b07e60e84f4cfe87b1cbfd` |
| 原始 invocation | `out/pd5-stage-c-cli-20261023-08e145c`，不改写 |
| 独立审计包 | `out/pd5-stage-c-audit-20261023-08e145c` |

冻结构建是只读输入，本表不得重建它。原始结果的独立核验与缺口见 [Stage C 独立验收报告](../admission/stage-c/2026-10-23-stage-c-independent-acceptance-and-exit-gaps.md)；内存分层的诊断归因见 [40k 内存包络归因](2026-10-23-40k-memory-envelope-attribution.md)；静态上限账本见 [B0 内存上限账本](2026-10-24-b0-memory-cap-ledger.md)。主树尚未集成隔离树的 R4/生命周期改动，因此这里的完整规模结果不能冒充主树已集成，也不能冒充正式测量已就绪。

## 3 物理与格式判据

七项物理门禁沿用 [B0 预算裁决](../admission/stage-b/2026-10-22-gameplay-admission-b0-budget-ruling.md) L40–41 的原值，本表不改动任何阈值。

| 判据 | 上限 | low | high | 判定 |
| --- | --- | ---: | ---: | --- |
| Packed file bytes | ≤ 16,777,216 | 6,818,424 | 6,746,664 | 通过 |
| decoded physical bytes | ≤ 16,777,216 | 6,817,880 | 6,746,120 | 通过 |
| 最大 section bytes | ≤ 16,777,216 | 3,725,942 | 3,654,182 | 通过 |
| entities | = 40,000 | 40,000 | 40,000 | 通过 |
| requirements | = 40,000 | 40,000 | 40,000 | 通过 |
| strings | ≤ 100,000 | 40,048 | 40,048 | 通过 |
| references | ≤ 100,000 | 13 | 13 | 通过 |
| `shaBytes`（四项 hash ceilings 之一） | ≤ 134,217,728 | 99,588,531 | 99,588,531 | 通过（74.2%） |

四项 hash ceilings 的完整集合为 134,217,728 / 1,000,000 / 4,096 / 8,388,608，本表只用到与 40k Packed 直接相关的 134,217,728 一项；其余三项的适用性不变，本表不对它们做独立重验。两侧的 `shaBytes` 相同且都只求值 1 次，因为两种 reuse 内容的语义前像一致。

## 4 声明的 40k 运行时预算

以下五项是冻结 profile 的声明值，两侧相同，不是实测值：

| 声明项 | 值 |
| --- | --- |
| `packed_bytes` | 6,818,424（low 侧 artifact 实测值，与本项声明一致） |
| `snapshot_bytes` | 90,823,328 |
| `frame_byte_bound` | 163,842,128 |
| `global_payloads` / `global_payload_bytes` | 2 / 327,684,256 |
| decode budget | `max_elements` 67,108,864 / `max_bytes` 268,435,456 / `max_records` 100,000 / `test_only` true |

`max_elements` 是由 20,000,000 提高到 67,108,864 的 testOnly 声明修复，属于已披露的工具声明变更，不是生产预算接受；`max_bytes` 268,435,456 对 Snapshot 90,823,328 的占用为 33.8%，`frame_byte_bound` 163,842,128 对 `global_payload_bytes` 327,684,256 的占用为 50.0%。这两个比例只说明声明空间有余量，不说明真实 heap 占用。

## 5 功能与运行事实

| 项目 | low | high |
| --- | ---: | ---: |
| assertions / test case | 51 / 1 | 51 / 1 |
| 有序阶段 / begin-succeeded 事件 | 24 / 48 | 24 / 48 |
| native exit | 0 | 0 |
| stderr | 空 | 空 |
| 进程运行秒数（插桩） | 1,780.4 | 1,680.4 |
| Replay / cut / Seek 后 `sameResult` | true | true |
| 显式 close 与 `host.reset` | 成功 | 成功 |
| Snapshot 编码长度 | 90,823,328 | 90,823,328 |

链路为 encode → `GameplayContent::fromPacked` → Host load/advance/query → Snapshot 编码/解码 → archive/evaluateReplay 的 `sameResult` → pause/replayCut/seek → sample/present → Seek 后 `sameResult` → frame 释放 → 显式 close/host.reset → after-close checkpoint → 进程正常退出。快照解码断言是 `decodedSnapshot.valid()`；**没有**断言解码后重新编码逐字节相等，也**没有**以该 decodedSnapshot 执行独立 restore。Replay 与 Seek 的 `sameResult` 不得改名成这两项额外证明。

显式 close 成功只证明这个固定 profile，不推翻旧约 25 分钟关闭故障的未归因状态；本批第二次诊断 pass 又在 4 次 40k close 中出现 1 次 `host.rpa.control_pending` 失败。该故障的静态路径、可静态到达的前置条件和交回 owner 的三个口径选项见[关闭路径静态定性](../lifecycle/2026-10-24-close-path-control-pending-static-qualification.md)，本表不重复其结论，也不把它当作已解决。

## 6 Seek 判据

| 来源 | Seek→sample/present，微秒 | 对 20,000 微秒 | 判定 |
| --- | ---: | ---: | --- |
| 冻结 low | 6,372,182 | 318.6× | 超界 |
| 冻结 high | 6,089,257 | 304.5× | 超界 |
| 诊断 pass-2（插桩，非冻结链） | 6,454,398 | 322.7× | 超界 |

三点全部落在 304.5×–322.7×。这些是插桩观察值：原协议规定插桩时间不用于 20 ms Seek 判定，[Stage C 报告](../admission/stage-c/2026-10-23-stage-c-independent-acceptance-and-exit-gaps.md) L66 也照此写了限定。owner 2026-10-24 明确裁定 40k 的 seek 计入「适用 Seek ≤20 ms」并记录超界，因此本表按裁定登记为**超界**，同时保留原限定：这是完整规模下的插桩观察被裁定计入判据，不是正常计时窗口的 p99 已经测出。`seek_start_us` 一类的插桩绝对时间戳仍然不是证据，本表不使用。

## 7 内存项

全进程观察包络在两侧都超限，且 `exceeded` 在 active 与 after-close 两个 checkpoint 都是 true：

| 观测量 | low | high | 相对固定工具包络 |
| --- | ---: | ---: | --- |
| peak live allocations | 8,521,200 | 8,441,200 | count 2,097,152 的 4.06× / 4.03× |
| peak requested bytes | 2,960,025,594 | 2,941,182,894 | 1,073,741,824 的 2.76× / 2.74× |
| peak CRT storage bytes | 3,505,382,394 | 3,481,419,694 | 1,342,177,280 的 2.61× / 2.59× |
| after-close live / storage | 5,120,531 / 1,688,113,017 | 5,120,531 / 1,687,951,557 | — |

`Allocation envelope incomplete or exceeded` 是观察器幅面拒绝，不是产品超限判定：该固定 testOnly 包络（count 2,097,152 / requested 1,073,741,824 / storage 1,342,177,280）从来没有被接受为 40k 的全 SDK 生产总预算，[归因报告](2026-10-23-40k-memory-envelope-attribution.md) 也把峰值来源定位为 seek 瞬态与 harness 层，而不是产品稳态。因此 PD-5 的内存项在这份证据下是**未测量**，不是**已失败**。

分域证据仍然缺：World/current/candidate/scratch/index、payload/attempt/journal/receipt 的峰值与 age、retired 与最终释放，都没有 40k 分域账本。after-close 的 5,120,531 个 live 与 1.69 GB storage 主要是测试用例的局部 owning 对象（fixture、两份 Snapshot、ReplayArchive、两份 GameplayResult），对象持有范围已解释，但「分域预算与释放完成」未验收。进程正常退出不等于逐域零泄漏证明。补齐路径是 [B0 账本](2026-10-24-b0-memory-cap-ledger.md) 之后的 B1–B3，本表不替代它。

## 8 判定

| 判据组 | 结果 | 依据 |
| --- | --- | --- |
| 七项物理门禁 + `shaBytes` | **通过**（两侧） | §3 |
| 功能链路（Replay/cut/Seek `sameResult`、close、exit 0、51 assertions） | **通过**（两侧，限该固定 profile） | §5 |
| 适用 Seek ≤20 ms | **超界**（304.5× / 318.6×，诊断 322.7×） | §6，owner 裁定计入 |
| 全进程内存观察包络 | **未测量**（观察器幅面拒绝，非产品超限） | §7 |
| 分域内存 / age / retired / 最终释放 | **缺证据** | §7，待 B1–B3 |
| 关闭长时风险 | **未归因**（冻结两侧成功；诊断 pass-2 4 次中 1 次 `host.rpa.control_pending`） | §5 |

P8 因此不能标记为通过：完整规模的功能与物理判据通过，但适用 Seek 判据已记录为超界，内存项是未测量而非通过。这三条会以同一口径进入 PD-5 判定报告——seek 一条已经足以让 PD-5 的结论走向「未通过但定案」，内存一条决定它是否只能写成「未测量」，而不允许被写成「已通过」。

## 9 单侧证据的三条限制

1. **没有窗口统计**。low/high 各一次，没有 3 窗口 × 3 对、没有 ≥600 s 窗间隔、没有窗内三进程 p99 中位数，因此不产生绝对延迟分位数，也不产生相对回归的配对 95% 区间。1.10 与 15% 两个门在 40k 侧未被检验，不是被通过。
2. **两侧不是重复**。low/high 是两种 reuse 内容（Packed 6,818,424 对 6,746,664），差异来自内容而不是噪声，所以不能用两者的离散度冒充重复性，也不能把它们当两个独立重复样本合并。
3. **插桩值有边界**。seek 与运行秒数都来自插桩运行；owner 裁定使 seek 可以计入判据，但不因此把插桩 elapsed、分配器 storage 或插桩时间戳变成正常计时或正常分配器证据。

## 10 非声明

- 不重跑 40k，不补重复窗口，不重建冻结构建，不改写原始 invocation。
- 不撤销任何历史失败记录：旧约 25 分钟关闭故障、包络超限、诊断 pass-2 的 `host.rpa.control_pending` 全部原处保留。
- 不改任何阈值：七项物理门禁、四项 hash ceilings、1.10、15%、内部 reply deadline 30 秒、Seek 20 ms 全部保持原值。
- 不证明 decoded Snapshot 的独立 restore 或 re-encode 逐字节相等。
- 不证明 40k 内存合规，也不证明其违规；内存项在这份证据下是未测量。
- 不以 P0–P7 代替 P8，也不因 P0–P7 未齐而豁免 P8 的适用项。
- 不构成 RPA-F3 回绑验收、生产接受、Stage 7 或 Stage 7-PD 关闭，也不授权提交、推送或合并。
