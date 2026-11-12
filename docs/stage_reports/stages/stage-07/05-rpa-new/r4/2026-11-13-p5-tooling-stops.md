# P5 控制单元：两次尝试都停在工具缺陷，等待第三次额度（2026-11-13）

状态：stopped attempts；两次均未产生任何组结论，第三次需要 owner 豁免

日期：2026-11-13

返回 [控制探针单元](2026-11-11-p5-controls-unit.md)与
[R4 门账](2026-11-08-r4-gate-status.md)。

## 1. 两次停止都不是负载问题

| 尝试 | 停在哪 | 原因 | 是否产生样本 |
| --- | --- | --- | --- |
| attempt-1 | window-1 / pair-1-B 的解析 | `prepare_controls` 声明 72 条样本，而探针每个进程还额外写一条 `load` 样本，解析结果为 73，`runner.parse_result` 判"计数与登记不符" | 一个进程的原生日志完整保留 |
| attempt-2 | window-1 / pair-2-B 的语义比较 | 运行器对 `controls` 适配器仍走展示矩阵的 `require_comparable`，而控制身份没有 `harness`/`updates`/`materializations` 等键，判"缺少精确负载身份" | 两个进程完整保留 |

两次停止都发生在**测量结果齐备之后**的解析与比较阶段，不是控制负载失败、不是环境变化、也不是性能超界。

## 2. 两处缺陷都已修复并验证

1. `prepare_controls.sample_count()`：把探针实际写出的那条 `load` 样本计入声明（72 → 73），
   并把原因写进函数文档；
2. `runner.py`：为 `controls` 适配器增加专用身份比较（`mode`/`frames`/`warmup_frames`/`cycles`/
   `rate`/`audio`/`closed`/`replay_equal`/`counts`），不再借用展示矩阵的键集。

验证：

- 执行准入套件 **70 项全部通过**（`python -m unittest discover -s tools/verification/execution_acceptance -t .`）；
- 用被停止尝试留下的真实日志重放运行器的解析与比较路径，两个进程逐项一致
  （`out/evidence/r4/p5/dry-check.py`）：pause 193.5 / 202.5 µs、resume 306.5 / 297.5 µs、
  resize 144.5 / 158.5 µs、reset 3.26/3.25 ms 与 2.93/2.98 ms。

这一步是为了**不再用第三次尝试去发现同一个缺口**。

## 3. 额度状态

`p5-controls-release` 的默认额度是每个单元两次，已分别被 attempt-1 与 attempt-2 用掉，
台账 `out/evidence/r4/attempt-ledger/p5-controls-release/` 保留两次 claim 与 seal 标记。

第三次尝试需要 `rpa.execution.attempt-waiver.v1` 形式的 owner 豁免，其中 `granted_by` 必须是
`owner`、`stable_protocol_ids` 必须包含 `p5-controls-release`、`maximum_attempts` 必须大于 2。
代理不能自己签发这份豁免。

## 4. 组一旦获准就能直接跑

准备件已在本次源码上重建并通过 pilot：`out/evidence/r4/p5/preparation-v2/`；seal 与 claim 的
流程已验证可用（走 `admission.claim` + `admission.seal` + `runner.run`）。协议为 B-only、
三窗口、每窗三进程、窗口间隔 600 秒、每进程 72 条控制样本 + 1 条 load 样本，预计 26 分钟。

## 5. 边界

- 本文不构成对 P5 的任何通过或超界结论：两次尝试都没有产生组统计。
- 两次停止的现场（日志、解析对象、`stopped.json`）保留，不替换、不补样本。
