# R3 未选用候选的实验材料归档

状态：archived；2026-10-30 暂停现场附件，不是产品实现或接受决定

更新日期：2026-10-30

返回 [暂停与候选交接报告](../2026-10-30-r3-pause-and-candidate-handoff.md)。当前状态只见 [CURRENT_STATUS](../../../../../../CURRENT_STATUS.md)。

本目录随文档提交保存 P1 待确认候选 v2、R3c 预算原型 v2 的源码差异与有限证据。归档不表示正式选用，也不将补丁应用到 engine/app/tests。源码基准为 `b1534a1`，产品实现恢复点为 `ff93484`。

## 源码与清单

- [P1 待确认候选 v2 源码差异](p1-remaining-v2.patch)

- [R3c 预算原型 v2 源码差异](r3c-budget-v2.patch)

- [来源、逐文件哈希及只读补丁适用性核对](manifest.json)

[目录属性](.gitattributes) 将冻结附件按原始字节存储，避免 Windows checkout 换行转换破坏哈希。
index.md 继续作为普通 Markdown 导航维护；该属性不影响目录外的产品源码。

补丁按 UTF-8/LF 文字保存，manifest 同时记录原现场字节 SHA 与规范化 LF SHA。跨 checkout 的 CRLF 变化需要新的源码/构建绑定，不能只凭源码可恢复就复用旧 binary 或成绩。

完整原始失败组、二进制、对象、依赖和 raw bytes 继续保存在 `out/verification-20261029/r3-completion/`，本次未删除或改写。脚本以 `.txt` 保存原字节，属于不可自动发现执行的历史材料；其绝对路径/固定目录仍有历史含义。恢复必须先审阅脚本、创建新绑定，不直接在冻结成功目录重跑。

## 附件索引

| 附件 | 原始位置（相对统一证据根） |
| --- | --- |
| [r3c-sealed-tick-design-v3.md.txt](r3c-sealed-tick-design-v3.md.txt) | `r3c-sealed-tick-design-v3.md` |
| [native.py.txt](native.py.txt) | `native.py` |
| [build_packed_remaining_v2.py.txt](build_packed_remaining_v2.py.txt) | `build_packed_remaining_v2.py` |
| [run_packed_remaining_v2.py.txt](run_packed_remaining_v2.py.txt) | `run_packed_remaining_v2.py` |
| [analyze_packed_remaining_v2.py.txt](analyze_packed_remaining_v2.py.txt) | `analyze_packed_remaining_v2.py` |
| [resource_probe.cpp.txt](resource_probe.cpp.txt) | `resource_probe.cpp` |
| [prepare_tick_budget.py.txt](prepare_tick_budget.py.txt) | `prepare_tick_budget.py` |
| [prepare_tick_budget_v2.py.txt](prepare_tick_budget_v2.py.txt) | `prepare_tick_budget_v2.py` |
| [build_tick_budget.py.txt](build_tick_budget.py.txt) | `build_tick_budget.py` |
| [resume_tick_budget_v1.py.txt](resume_tick_budget_v1.py.txt) | `resume_tick_budget_v1.py` |
| [build_tick_budget_v2.py.txt](build_tick_budget_v2.py.txt) | `build_tick_budget_v2.py` |
| [prepare_tick_fault.py.txt](prepare_tick_fault.py.txt) | `prepare_tick_fault.py` |
| [run_tick_screen_40k.py.txt](run_tick_screen_40k.py.txt) | `run_tick_screen_40k.py` |
| [run_tick_writer.py.txt](run_tick_writer.py.txt) | `run_tick_writer.py` |
| [p1-v2-binding.json](p1-v2-binding.json) | `packed-remaining-v2/binding.json` |
| [p1-v2-commands.json](p1-v2-commands.json) | `packed-remaining-v2/commands.json` |
| [p1-v2-test-baseline.log.txt](p1-v2-test-baseline.log.txt) | `packed-remaining-v2/test-packed-baseline.log` |
| [p1-v2-test-candidate.log.txt](p1-v2-test-candidate.log.txt) | `packed-remaining-v2/test-packed-candidate.log` |
| [p1-v1-measure-registration.json](p1-v1-measure-registration.json) | `packed-remaining-measure-v1/registration.json` |
| [p1-v1-measure-results.json](p1-v1-measure-results.json) | `packed-remaining-measure-v1/results.json` |
| [p1-v1-measure-decision.json](p1-v1-measure-decision.json) | `packed-remaining-measure-v1/decision.json` |
| [p1-v1-byte-comparison.json](p1-v1-byte-comparison.json) | `packed-remaining-measure-v1/comparison.json` |
| [r3c-budget-v2-source-binding.json](r3c-budget-v2-source-binding.json) | `tick-prototype-v1/budget-build-v2/source-binding.json` |
| [r3c-budget-v2-results.json](r3c-budget-v2-results.json) | `tick-prototype-v1/budget-build-v2/results.json` |
| [r3c-budget-v2-complete.json](r3c-budget-v2-complete.json) | `tick-prototype-v1/budget-build-v2/complete.json` |
| [r3c-budget-v2-test-rpa.log.txt](r3c-budget-v2-test-rpa.log.txt) | `tick-prototype-v1/budget-build-v2/test-cuexis_rpa_tests.log` |
| [r3c-budget-v2-test-judgement.log.txt](r3c-budget-v2-test-judgement.log.txt) | `tick-prototype-v1/budget-build-v2/test-cuexis_judgement_tests.log` |
| [r3c-budget-v2-test-host.log.txt](r3c-budget-v2-test-host.log.txt) | `tick-prototype-v1/budget-build-v2/test-cuexis_execution_host_tests.log` |
| [r3c-budget-v2-test-packed.log.txt](r3c-budget-v2-test-packed.log.txt) | `tick-prototype-v1/budget-build-v2/test-cuexis_gameplay_packed_tests.log` |
| [r3c-unbudgeted-40k-registration.json](r3c-unbudgeted-40k-registration.json) | `tick-screen-40k-v1/registration.json` |
| [r3c-unbudgeted-40k-results.json](r3c-unbudgeted-40k-results.json) | `tick-screen-40k-v1/results.json` |
| [r3c-unbudgeted-40k-decision.json](r3c-unbudgeted-40k-decision.json) | `tick-screen-40k-v1/decision.json` |

## 恢复边界

P1 仍缺 v2 最终固定性能组；R3c 预算 v2 的 426 个相关功能用例不代替真实分配失败、普通 Seek/writer、实际内存或正式门。无预算版的 2.7385 ms 和实验 128 MiB 都不能自动成为新预算版产品结论。

原型含等待 stdin 的非隐藏 `[r3c-writer]` 用例；既有功能回归使用 `~[r3c-writer]` 排除它。归档脚本/补丁没有产品执行授权，owner 暂停仍有效。
