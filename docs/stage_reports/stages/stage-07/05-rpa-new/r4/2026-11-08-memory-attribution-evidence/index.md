# R4 内存归因证据包

状态：candidate evidence；逐域精确分区与 P8/40K 失败现场，不含接受裁定

日期：2026-11-08

返回 [本轮报告](../2026-11-08-memory-attribution.md)与
[New RPA 报告索引](../../README.md)。本页只登记原始字节位置与只读复核方式。

## 1. 目录清单

| 目录 | 内容 | 说明 |
| --- | --- | --- |
| `out/evidence/r4/memory-20261108/build-memory-v2` | configure/build 日志、CMakeCache、compile_commands、observer 构建绑定 | 候选 `51b0ebd5` 的 observer 构建 |
| `out/evidence/r4/memory-20261108/contracts` | 四个 observer 合同用例日志与小结（唯一下界 multiset `{0,176,784,8192,12841}`） | 合同不通过就不跑容量进程 |
| `out/evidence/r4/memory-20261108/n64-high`、`n64-low` | 注册、stdout/stderr、进程结果、逐域归因 | 精确分区，`untagged=0` |
| `out/evidence/r4/memory-20261108/n2048-high` | 同上 | 精确分区 |
| `out/evidence/r4/memory-20261108/n40000-high` | 注册、stdout/stderr、进程结果（退出码 42，`packed.budget.strings`） | P8/40K 阻断现场，不重试 |
| `out/evidence/r4/memory-20261108/authorization.json` | 范围、用例、超时与停止规则，绑定 source/binary sha | 额度豁免下的有限诊断 |
| `out/evidence/r4/memory-20261108/run-census.py` | evidence 侧控制器（参与哈希） | 契约、环境、停止条件都在脚本里 |

`tools/verification/execution_acceptance/memory_attribution.py` 是本批之后提升为可复用模块的
同一逻辑，带 `test_memory_attribution.py` 单元测试；它不参与 evidence 字节哈希。

## 2. 只读复核

~~~powershell
python -B -m tools.verification.execution_acceptance.memory_attribution --log out/evidence/r4/memory-20261108/n64-high/stdout.log --budget tools/verification/execution_acceptance/budget-proposal.v1.json --profile r4-reference-n64-v1 --output out/evidence/r4/memory-20261108/n64-high/attribution-tool.json
python -B -c "import json,pathlib; d=json.loads(pathlib.Path('out/evidence/r4/memory-20261108/n2048-high/attribution.json').read_text(encoding='utf-8')); print(d['over_declared'] and [r['id'] for r in d['over_declared']], d['partition']['untagged_live_count'])"
python -B -c "import json,pathlib; d=json.loads(pathlib.Path('out/evidence/r4/memory-20261108/n40000-high/process-result.json').read_text(encoding='utf-8')); print('exit', d['exit_code'], 'timed_out', d['timed_out'])"
~~~

复核只读取已归档字节，不重新运行样本。

## 3. 边界

- 分区只覆盖已链接的 C++ `new`/`delete`；直接 malloc、SDL、驱动、GPU 与 OS 堆不在内。
- 对照的上限是 proposed，不是已接受；超出是“对提案的超出”。
- 40K 的失败是 fixture 的打包字符串预算，不是内存观测器缺陷；按停止规则不重试。
- `unclassified` 字节（n64 0.38 MB、n2048 10.2 MB）来自 fixture/其他来源，不冒充产品域。
