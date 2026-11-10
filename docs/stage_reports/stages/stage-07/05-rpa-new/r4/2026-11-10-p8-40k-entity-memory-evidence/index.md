# P8 40K 实体内存证据包

状态：candidate evidence；只登记原始字节位置与只读复核方式，不含接受裁定

日期：2026-11-10

返回 [本批报告](../2026-11-10-p8-40k-entity-memory.md)与 [New RPA 报告索引](../../README.md)。

## 1. 目录清单

| 目录 | 内容 | 说明 |
| --- | --- | --- |
| `out/evidence/r4/memory-40k/build-binding-3` | configure/build 日志、CMakeCache、compile_commands、构建绑定 | 候选 `2c4e1274` 的 observer 构建 |
| `out/evidence/r4/memory-40k/n40000-r4096-high-3` | 注册、stdout/stderr、进程结果、逐域归因 | 最终候选的 P8 运行，exit 0 |
| `out/evidence/r4/memory-40k/n40000-r4096-high-2` | 同上 | 前一 SHA（`67e905be`）的同内容复跑，数值一致 |
| `out/evidence/r4/memory-40k/run-case.py` | evidence 侧控制器（参与哈希） | 单进程、无替换、固定 600 s 超时 |
| `out/evidence/r4/memory-40k/index.md` | 目录与结论摘要 | 与报告 §2–§4 一致 |

## 2. 只读复核

~~~powershell
python -B -m tools.verification.execution_acceptance.memory_attribution --log out/evidence/r4/memory-40k/n40000-r4096-high-3/stdout.log --label active --budget tools/verification/execution_acceptance/budget-proposal.v1.json --profile r4-reference-n40000-v1 --output out/evidence/r4/memory-40k/n40000-r4096-high-3/attribution-recheck.json
python -B -c "import json,pathlib; d=json.loads(pathlib.Path('out/evidence/r4/memory-40k/n40000-r4096-high-3/attribution.json').read_text(encoding='utf-8')); print(d['observed_requested_cpp_bytes'], [r['id'] for r in d['over_declared']], d['partition']['untagged_live_count'])"
~~~

复核只读取已归档字节，不重新运行样本。

## 3. 边界

- 分区只覆盖已链接的 C++ `new`/`delete`；直接 malloc、SDL、驱动、GPU 与 OS 堆不在内。
- 对照上限是 proposed，不是已接受；`command.session` 的超出是“对提案的超出”。
- 内容行为 4,096 是本批的显式工作负载，正式 P8 内容口径需单独接受。
- 本批不改任何预算或接受记录。
