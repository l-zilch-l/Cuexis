# Stage 6 复核修正：R1 文档与证据链修正

状态：completed
快照日期：2026-09-28
更新日期：2026-09-28

本报告是 [Stage 6 复核修正计划](../../../stage_plans/active/stage-06-review-remediation/plan.md)
批次 **R1**（纯文档，零代码风险）的退出记录。它逐条处置
[两轴复核记录](2026-09-28-summary.md) 中的文档类发现项，并给出可审计的命令输出。

**修正原则**：只改状态词、指针与码名，**不改写历史现象与证据原文**；凡原文有误的，
一律以"追加订正说明"的方式保留原句。本批次不夹带任何实现改动。

## 1. 批次边界

- 起始 SHA：`041ccac`（`stage-06-review-workspace` 的原 HEAD）。
- 本批次是纯文档改动，不修改 `engine/`、`app/`、`tools/`、`cmake/` 的任何源码。
- 例外：为关闭 STD-02 而**新增**的 `examples/reference_host/README.md` 是文档文件
  （Markdown），不是代码；它不进入任何编译或安装目标。
- 版本滚动与 CI 证据见本报告 §4。

## 2. 逐条处置

| 复核 ID | 严重度 | 现象（原文摘要） | 本批次修正 | 证据 |
| --- | --- | --- | --- | --- |
| STD-02 | 高 | C4 实现报告 §2 声称交付 `examples/reference_host/README.md`，该文件不存在 | **补写该 README**（主循环、命令循环、provider 构造、构建/运行步骤），并在 C4 报告该行追加订正说明 | `examples/reference_host/README.md`（新增）；`...s6-c4-reference-host-and-player-distribution.md:28` |
| STD-03 | 高 | Stage 6 状态词与"已关闭并归档"冲突（plan/completion/ROADMAP/VERSIONING） | 只改状态词，不改证据；同步 `更新日期` | `stage_plans/completed/stage-06/plan.md:51`/`:104`/`:106`；`stage_reports/stages/stage-06/completion.md:46`；`ROADMAP.md:35-36`/`:113`；`guides/VERSIONING.md:175` |
| SPEC-26 | 高 | `completion.md` 的 `S6-G13` 行 SHA↔run 归属与 C4 退出报告 §8 不一致 | 按 §8 的 run 表重写归属（把 `d7980bd` 放回、补 `cb56e62` 的 run），并按 `S6-F2` 第 1 条追加订正说明 | `completion.md` 的 `S6-G13` 行与其后的「订正说明」 |
| SPEC-32 | 中 | 关闭报告 §8 未承接 5 条批次自认残余 | 在 §8 追加一行承接，并注明**未随 §0 的 2026-09-27 接受一并被接受** | `completion.md` §8 新增行 |
| SPEC-10 | 低 | C1 退出报告把缺资源写成 `candidate.identity.resource_conflict` | 追加订正说明，给出正确码 `playback.identity.resource_missing` | `2026-09-22-s6-c1-exit.md` 的订正段 |
| SPEC-17 | 低 | C2 退出报告把注入设备列表的单测写成热拔插验证 | 在该条后补注**证据级别** | `2026-09-23-s6-c2-exit.md` §2 第 6 条 |
| SPEC-31 | 低 | preset 计数在 plan（7）与 completion（8）之间不一致 | 按 F1 记录的实际 preset 数统一为 8 | `stage-06/plan.md:104` |
| SPEC-29 | 低 | 插桩转发与 MinGW 运行库复制是"计划外机制"，未记录 | 写入 `BUILDING.md` 的门禁章节，使其成为已记录机制 | `BUILDING.md`「具名参考宿主」节 |
| STD-01（报告侧） | 高 | C1 导出报告的预算码表述与契约不符 | 追加订正说明（实现侧修正由 R2 承担） | `2026-09-22-s6-c1-exit.md` 的订正段 |

## 3. 修正中发现的偏差（必须记录，不得静默）

复核记录 STD-02 的表述有一处不精确，本批次据实修正并记录：

- 复核称「`docs/guides/BUILDING.md:468` 已把该 README 当作既有入口引用」。
  **父代理核实后不成立**：`BUILDING.md:468` 是「具名参考宿主（Reference Host）」小节的正文，
  它引用的是 `examples/reference_host/` 这个**目录**，不是 `README.md`；
  `BUILDING.md:504` 的 `README.txt` 是 **Player 分发目录**自带的说明文件，与宿主 README 无关。
- 因此 **STD-02 的唯一真实悬空引用是 C4 实现报告第 28 行的交付物清单**。
  本批次按该事实处置（补写 README + 订正该行），**不修改 `BUILDING.md` 第 468 行的措辞**，
  因为那里本就没有悬空引用。
- 复核记录中的原始表述保留不改写（按计划 §11 与 §4 的要求）。

这条偏差说明：复核记录本身也是文档，其"证据指针"同样需要逐条核实。
本报告把核实结果如实登记，而不是照抄复核表述。

## 4. 门禁与命令输出

本批次的版本滚动与门禁在 PR #30 上完成，实际执行的命令与结果：

| 命令 | 结果 |
| --- | --- |
| `python -B tools/check_version_gate.py --repo-root . --base-ref <origin/master> --candidate-ref <HEAD> --trusted-utc-date <date -u +%F> --context live --event pull_request` | 修正前：`Version gate failed: version.unchanged: candidate version is unchanged from the baseline`（本地复现）；滚动版本后：通过 |
| `python -B tools/check_docs.py` | 通过 |
| `python -B tools/update_version.py --check` | 通过 |
| `git diff --check` | 通过（exit 0） |

版本滚动：`python -B tools/update_version.py <yy.mm.dd>-<build>`。
本批次执行时的受信 UTC 日期见 PR #30 的 Version Gate run；滚动规则为
「同一 UTC 发布日 = 基线 build + 1；日期前进 = build 1」
（见 [VERSIONING.md](../../../guides/VERSIONING.md)）。

hosted 证据（同最终 SHA）：见 PR #30 的 Version Gate / Linux Quality / Windows MSVC /
Windows MinGW run，均为 success。

## 5. 残余与未核对

- 本批次**未复跑** C++ 构建与 CTest（纯文档批次，按计划 §8.1 只跑文档门禁三连）。
  因此不宣称任何实现门禁通过。
- `BUILDING.md:468` 的原文未被修改（见 §3 的理由）；若后续复核仍坚持那里存在悬空引用，
  应作为**新的**发现项重新登记，而不是回流到本批次。
- STD-05…STD-13 的 smell / 可移植性 / 卫生项不在 R1 范围（属 R8）。
- STD-01、SPEC-02、SPEC-06 的实现侧修正属 R2，不在本批次。
