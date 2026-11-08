# R4 普通 host 单元证据包

状态：candidate evidence；attempt-1 在第三窗口前停止，不含整组裁定

日期：2026-11-08

返回 [本轮报告](../2026-11-08-common-host-unit.md)与
[New RPA 报告索引](../../README.md)。本页只登记原始字节位置与只读复核方式，
不新增测量结论。

## 1. 目录清单

| 目录 | 内容 | 说明 |
| --- | --- | --- |
| `out/evidence/r4/host-20261108/build-release-h1` | configure/build 日志、CMakeCache、compile_commands、命令文本、目标依赖清单与两个 build binding | 候选 `f1c4d22e` 的正式构建身份 |
| `out/evidence/r4/host-20261108/host-pilot-v1` | `pilot-declaration.json`、warmup/sample 日志、原生环境、`pilot-parsed.json`、三份 proof、`preparation.json`、`formal.seal.json` | pilot 固定 digest 与环境；不进入正式样本 |
| `out/evidence/r4/host-20261108/host-formal-run1` | 窗口 1、2 各 3 个完整进程、窗口 3 的中止热身、逐进程 before/after 与 runtime 环境、`stopped.json`、`absolute-gate-recompute.json` | attempt-1 现场与只读复算 |
| `out/evidence/r4/host-20261108/remedy-run-hold-h1.json` | 整组期间逐秒环境记录（含 nudge 与失败时点） | 运行期补救证据 |
| `out/evidence/r4/attempt-ledger/p0-host-common-release` | `attempt-1.json` 与 `attempt-1.seal.json` | 正式额度已消耗一次，attempt-2 仍可用（需 owner 先定方向） |

`out/evidence/r4/probe-20261108/evidence-manifest.json` 的两批合并清单不包含 host-20261108；
host 目录内另有 `absolute-gate-recompute.json`，它是由已归档 `parsed.json` 只读复算得到。
该次 `stopped.json` 是**修复前**写出的，因此 `partial_absolute_gates` 为空（0 条）；
修复后的 runner 会把已算出的逐进程绝对门一并写入停止记录，本条只记录这个时间差。

## 2. 只读复核

~~~powershell
python -B -c "import json,pathlib,hashlib; p=pathlib.Path('out/evidence/r4/host-20261108/host-formal-run1'); r=json.loads((p/'absolute-gate-recompute.json').read_text(encoding='utf-8')); print('rows',len(r['rows']),'statuses',sorted({x['status'] for x in r['rows']}))"
python -B -c "import json,pathlib; p=pathlib.Path('out/evidence/r4/host-20261108/host-formal-run1/stopped.json'); d=json.loads(p.read_text(encoding='utf-8')); print('stopped',d['utc'],'partial gates',len(d.get('partial_absolute_gates',{}).get('p0-host-common',[])))"
~~~

`runner.py` 不提供重放模式；复核只针对已归档字节，不重新运行样本。

## 3. 边界

- 只有两个完整窗口，没有 `repeatability()` 或 `verdict()` 结论。
- 六个完成进程的 `request→ready` p99 逐进程低于 4.167 ms 预算，这是逐进程绝对门，
  不是整组通过。
- 停止原因是前台在多个进程之间变化（桌面被持续使用），不是产品语义、身份或性能问题。
