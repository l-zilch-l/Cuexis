# New RPA Reports and Evidence

状态：current index

更新日期：2026-11-02

性能整改工作线，设计与 R0、R1、R2 分组；停止、失败和后续报告保留在同批目录。

当前状态只以 [CURRENT_STATUS](../../../../CURRENT_STATUS.md) 为准；本页只负责定位，不新增实施、测量或阶段接受结论。

先按下表定位批次；同批报告与附件放在同一目录。完整原始路径直接查 [证据清单](evidence-catalog.md)，无须逐篇翻阅。

表内数字是 `Markdown 文件数 / 非 Markdown 文件数`，按目录递归统计、含证据目录自带的 `index.md` 与
`manifest.json`；它只度量归档体量，不度量批次质量，也不因为数字变大而表示批次更重要。

证据目录里的日志是**按捕获字节保存**的：原始输出里本来就有的行尾空白与末尾空行都留在提交里，其中 6 份 `.log` 按 CRLF 提交。
后果之一是 `git diff --check` 在这些证据提交上必然报错——2026-11-01 的三份附件合计 198 个文件、320 处（192 份 LF 文本的真实尾随空格、6 份 CRLF 日志），
先例 `9c392af` 同样被报 61 处。同一批 2026-11-01 追问的两份附件（[第四次矩阵](r3/2026-11-01-rpa-acceptance-matrix-run4-evidence/index.md)、[前缘重推](r3/2026-11-01-capacity-frontier-rederive-evidence/index.md)）由提交 `d0c35b4` 发布，那次被报 131 个文件、131 处（120 处是日志末尾空行、11 处真实尾随空格），性质相同，统计记在各自证据目录 `validation.json` 的 `diff_over_commit` 字段里。
该检查是源码洁净门，不是证据门；裁掉这些字节会破坏每份文件的 `sha256` 绑定，因此这里只记录事实，不把它当成通过项。

返回 [Stage 7 阶段总表](../README.md)。计划入口：[对应阶段计划](../../../../stage_plans/active/stage-07/05-rpa-new/README.md)。

工作树整合与本次文档整理记录见 [共享材料](../90-shared/README.md)。

## 批次定位（实施推进顺序）

| 批次目录 | 报告 / 附件 | 直接入口 |
| --- | --- | --- |
| `design` | 1 / 0 | [2026-10-24 · Stage 7-RPA 性能整改候选设计审阅（2026-10-24）](design/2026-10-24-performance-remediation-design-review.md) |
| `r0` | 1 / 0 | [2026-10-24 · R0 observation and anchor](r0/2026-10-24-r0-observation-and-anchor.md) |
| `r1` | 3 / 0 | [New RPA / r1](r1/README.md) |
| `r2` | 9 / 5 | [New RPA / r2](r2/README.md) |
| `r3` | 73 / 2384 | [R3a 恢复闭包与准备去重](r3/2026-10-27-r3a-recovery-and-prepare.md)、[R3b 可信前缀与成本核验](r3/2026-10-27-r3b-certified-prefix-and-cost.md)、[空推进 reserve 限定尝试](r3/2026-10-27-r3-empty-reserve-attempt.md)、[高 requirements 耗时归因](r3/2026-10-27-r3-high-requirements-attribution.md)、[分页与自然锚点重构](r3/2026-10-27-r3-redesign.md)、[首次身份与体积归因](r3/2026-10-27-r3-cold-identity-attribution.md)、[哈希与编码器恢复点](r3/2026-10-27-r3-hash-and-wire-checkpoint.md)、[来源绑定实验](r3/2026-10-27-r3-source-binding-experiment.md)、[完整 Packed 准备归因](r3/2026-10-27-r3-packed-prepare-attribution.md)、[完整 Packed 索引优化](r3/2026-10-27-r3-packed-index-optimization.md)、[prepare 声明查找优化](r3/2026-10-27-r3-prepare-declaration-optimization.md)、[2026-10-28 全量核验与优化登记](r3/2026-10-28-r3-redesign-verification-and-optimization.md)、[SDK 0.9.1 与架构同步](r3/2026-10-28-sdk-091-architecture-sync.md)、[已冻结 prepare 复制消除](r3/2026-10-28-r3-resolved-prepare-copy.md)、[setup 与首次 Seek 轻量归因](r3/2026-10-28-r3-seek-noise-attribution.md)、[零绑定投影修复与通用方案撤回](r3/2026-10-29-r3-projection-empty-and-general-attempt.md)、[40K 实体真实 GPU 吞吐核验](r3/2026-10-29-r3-40k-physical-gpu-throughput.md)、[1080P 实体容量与 240 FPS 边界](r3/2026-10-29-r3-1080p-entity-capacity.md)、[Runtime 外观重复查询优化](r3/2026-10-29-r3-runtime-appearance-lookup.md)、[Runtime 本帧触及集合优化](r3/2026-10-29-r3-runtime-touched-membership.md)、[六项实施与验证账本](r3/2026-10-29-r3-six-item-completion-work.md)、[暂停现场与未选用实验归档](r3/2026-10-30-r3-pause-and-candidate-handoff.md)、[候选附件索引](r3/2026-10-30-candidate-archive/index.md)、[恢复实施与预算验证](r3/2026-10-30-r3-resume-and-budget-verification.md)、[MSVC Ninja 依赖修复](r3/2026-10-30-r3-dependency-tracking.md)、[Seek 发布归因与批量候选](r3/2026-10-30-r3-publication-attribution.md)、[activation与活跃索引实验](r3/2026-10-30-r3-activation-and-active-sequence.md)、[启动响应、JSON准备与渲染归因](r3/2026-10-31-r3-startup-and-render-attribution.md)、[启动批次附件](r3/2026-10-31-startup-evidence/index.md)、[中立命令转移优化](r3/2026-10-31-r3-neutral-command-transfer.md)、[命令优化附件](r3/2026-10-31-draw-transfer-evidence/index.md)、[启动可观测性与准备复用](r3/2026-10-31-r3-startup-observability-and-prepare-reuse.md)、[启动证据附件](r3/2026-10-31-startup-observability-evidence/index.md)、[40K 240 FPS 与预算判定](r3/2026-10-31-r3-40k-240fps-and-budget-verdict.md)、[40K 判定证据附件](r3/2026-10-31-40k-verdict-evidence/index.md)、[同源验收矩阵](r3/2026-11-01-r3-same-source-acceptance-matrix.md)、[矩阵证据附件](r3/2026-11-01-rpa-acceptance-matrix-evidence/index.md)、[第二次运行与 MinGW 分配探针极限](r3/2026-11-01-r3-run2-and-mingw-allocation-probe-limit.md)、[MinGW 分配探针证据附件](r3/2026-11-01-mingw-allocation-probe-evidence/index.md)、[同源验收矩阵第三次运行](r3/2026-11-01-r3-acceptance-matrix-third-run.md)、[第三次运行证据附件](r3/2026-11-01-rpa-acceptance-matrix-run3-evidence/index.md)、[1080p 容量前缘区间](r3/2026-11-01-r3-capacity-frontier-bracket.md)、[容量前缘证据附件](r3/2026-11-01-capacity-frontier-evidence/index.md)、[提交路径停顿归因](r3/2026-11-01-r3-submit-stall-wait-attribution.md)、[停顿归因证据附件](r3/2026-11-01-submit-stall-wait-evidence/index.md)、[40K 启动可观测性与 prepare 成本](r3/2026-11-01-r3-40k-startup-observability-and-cost.md)、[启动与 prepare 成本证据附件](r3/2026-11-01-40k-startup-and-prepare-cost-evidence/index.md)、[中立路径 CPU 杠杆](r3/2026-11-01-r3-neutral-path-cpu-lever.md)、[中立路径杠杆证据附件](r3/2026-11-01-neutral-path-lever-evidence/index.md)、[第四次同源验收矩阵](r3/2026-11-01-r3-acceptance-matrix-fourth-run.md)、[第四次运行证据](r3/2026-11-01-rpa-acceptance-matrix-run4-evidence/index.md)、[容量前缘重新推导](r3/2026-11-01-r3-capacity-frontier-rederive.md)、[重新推导证据](r3/2026-11-01-capacity-frontier-rederive-evidence/index.md)、[第一批正式门测量](r3/2026-11-01-r3-first-gate-batch.md)、[门测量证据附件](r3/2026-11-01-r3-gate-batch1-evidence/index.md)、[第二批正式门测量](r3/2026-11-01-r3-second-gate-batch.md)、[第二批证据附件](r3/2026-11-01-r3-gate-batch2-evidence/index.md)、[修复尝试：读取路径与首次 Pause](r3/2026-11-01-r3-gate-repair-attempt.md)、[修复尝试证据](r3/2026-11-01-r3-gate-repair-attempt-evidence/index.md)、[两项超界的根因分析](r3/2026-11-01-r3-gate-cause-analysis.md)、[根因证据](r3/2026-11-01-r3-gate-cause-evidence/index.md)、[两项超界的重构探索](r3/2026-11-01-r3-gate-refactor-round.md)、[重构探索证据](r3/2026-11-01-r3-gate-refactor-round-evidence/index.md)、[相对读取门：材料化器裁剪后通过](r3/2026-11-01-r3-read-gate-materializer-trims.md)、[裁剪证据](r3/2026-11-01-r3-read-gate-trim-evidence/index.md)、[Pause 预热与适用 Seek 门](r3/2026-11-01-r3-pause-prime-and-seek-gate.md)、[预热与 Seek 证据](r3/2026-11-01-r3-pause-prime-and-seek-evidence/index.md)、[owner 四项门裁定](r3/2026-11-02-r3-owner-gate-rulings.md) |

## 原始材料与历史位置

- [按批次列出的原始证据路径](evidence-catalog.md)
- [重组前路径映射](../legacy-paths.md)
