# R4 普通 host 单元证据包

状态：candidate evidence；attempt-2 三窗口九进程完成，attempt-1 的停止现场保留

日期：2026-11-08

返回 [本轮报告](../2026-11-08-common-host-unit.md)与
[New RPA 报告索引](../../README.md)。本页只登记原始字节位置与只读复核方式，
不新增测量结论。

## 1. 目录清单

| 目录 | 内容 | 说明 |
| --- | --- | --- |
| `out/evidence/r4/host-20261108/build-release-h1` | configure/build 日志、CMakeCache、compile_commands、命令文本、目标依赖清单与两个 build binding | 候选 `f1c4d22e` 的正式构建身份 |
| `out/evidence/r4/host-20261108/host-pilot-v1` | `pilot-declaration.json`、warmup/sample 日志、原生环境、`pilot-parsed.json`、三份 proof、`preparation.json`、`formal.seal.json` | attempt-1 的 pilot；digest 与环境在此固定 |
| `out/evidence/r4/host-20261108/host-formal-run1` | 窗口 1、2 各 3 个完整进程、窗口 3 的中止热身、逐进程环境、`stopped.json`、`absolute-gate-recompute.json` | attempt-1 停止现场与只读复算 |
| `out/evidence/r4/host-20261108/host-pilot-v2` | `p0-host-common-retry-disposition.json`、复用 proof 的新 `preparation.json`、`formal.seal.json` | attempt-2 的重登记身份 |
| `out/evidence/r4/host-20261108/host-formal-run2` | 三窗口九进程的日志、parsed、环境采样与 `result.json` | attempt-2 的正式结果 |
| `out/evidence/r4/host-20261108/remedy-run-hold-h1.json`、`remedy-run-hold-h2.json` | 两次运行期逐秒环境记录（含 nudge 与切窗时点） | 运行期补救证据；h2 的切窗落在窗口间隔内 |
| `out/evidence/r4/attempt-ledger/p0-host-common-release` | `attempt-1`/`attempt-2` 的 claim 与 seal marker | 两次额度均已消耗 |
| `out/evidence/r4/host-20261108/evidence-manifest.json` | 本单元合并清单：189 文件 / 26,497,340 B 的逐文件哈希 | 总复核入口 |
| `out/evidence/r4/host-20261108/prepare-retry-disposition.py` | 一次性处置脚本（复用 attempt-1 的 proof，不重跑 pilot） | 不参与清单哈希，保留供复核 |

## 2. 只读复核

~~~powershell
python -B -c "import json,pathlib; p=pathlib.Path('out/evidence/r4/host-20261108/host-formal-run2/result.json'); d=json.loads(p.read_text(encoding='utf-8-sig'))['p0-host-common']; print('metrics',{k:v['status'] for k,v in d['metrics'].items()}); print('gates',sorted({g['gates']['request_to_ready']['status'] for g in d['per_process_absolute_gates']}), len(d['per_process_absolute_gates']))"
python -B -c "import json,hashlib,sys; m=json.load(open('out/evidence/r4/host-20261108/evidence-manifest.json',encoding='utf-8')); bad=[f['path'] for f in m['files'] if hashlib.sha256(open(f['path'],'rb').read()).hexdigest()!=f['sha256']]; print('files',len(m['files']),'mismatch',len(bad)); sys.exit(1 if bad else 0)"
~~~

`runner.py` 不提供重放模式；复核只针对已归档字节与 manifest 哈希，不重新运行样本。

## 3. 边界

- attempt-2 的九条 `request→ready` p99 全部低于 4.167 ms；这是逐进程绝对门，不等于
  整批 R4 通过。
- `ready→present-return` 的重复性为 inconclusive（约 4 µs 量级），保留不删。
- attempt-1 的停止原因是前台持续变化（桌面被使用），不是产品语义、身份或性能问题。
- 补救记录 h2 因“连续采样把窗口间隔内的切窗也计入”而判 failed；九个测量进程各自的环境
  采样全部 passed，两者标准不同，不能互相替代。
