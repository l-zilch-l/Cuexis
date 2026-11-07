# R4 展示矩阵探针证据包

状态：candidate evidence；attempt-1 因会话锁定停止，不含正式裁定

日期：2026-11-08

返回 [本轮报告](../2026-11-08-probe-matrix-admission-and-environment-stop.md)与
[New RPA 报告索引](../../README.md)。本页只登记原始字节位置与只读复核方式，
不新增测量结论。

## 1. 目录清单

| 目录 | 内容 | 说明 |
| --- | --- | --- |
| `out/evidence/r4/probe-20261107/build-release-b1` | configure/build 日志、CMakeCache、compile_commands、compiler、命令文本、目标依赖清单、`cuexis_pd_host_probe`/`cuexis_pd_baseline_probe` 的 build binding | 候选 `00af7f90` 的正式构建身份 |
| `out/evidence/r4/probe-20261107/probe-pilot-v1` | `pilot-declaration.json`、每单元 warmup/sample 日志、原生环境、parsed、三份 proof、`preparation.json`、`formal.seal.json` | pilot 与封存身份；不进入正式样本 |
| `out/evidence/r4/probe-20261107/formal-run1` | 窗口 1 的 12 个完整进程、窗口 2 的 1 个完整进程与 1 个中止热身、每个进程的 before/after 环境与 runtime 环境采样、`stopped.json` | attempt-1 现场，失败保留 |
| `out/evidence/r4/probe-20261107/precheck-identity-20261108` | `p4-capture-2`、`p4-capture-4`、`p6-topology` 的全规模非正式身份预检与 `precheck.json` | 只证明登记形状，不构成计时结论 |
| `out/evidence/r4/probe-20261107/build-release` | 首次构建的 configure/build 日志与 `source.json` | 该次 binding 阶段因构建期间新增文件而拒绝；未生成 binding，保留为过程记录 |
| `out/evidence/r4/attempt-ledger` | 四个稳定单元键的 `attempt-1.json` 与 `attempt-1.seal.json` | 正式额度已消耗且不可重置 |

`out/evidence/r4/probe-20261107/evidence-manifest.json` 逐文件登记 193 个成员的
`path`/`bytes`/`sha256`（合计 261,491,981 B），并记录候选 SHA 与日期版本。

## 2. 只读复核

以下命令只读取已存在字节，不启动任何工作负载：

~~~powershell
python -B -c "import json,hashlib,sys; m=json.load(open('out/evidence/r4/probe-20261107/evidence-manifest.json',encoding='utf-8')); bad=[f['path'] for f in m['files'] if hashlib.sha256(open(f['path'],'rb').read()).hexdigest()!=f['sha256']]; print('files',len(m['files']),'mismatch',len(bad)); sys.exit(1 if bad else 0)"
~~~

`runner.py` 不提供重放模式；复核只针对已归档字节与 manifest 哈希，不重新运行样本。

## 3. 边界

- 本证据包不包含三窗口结果，因此没有任何重复性门、相对门或绝对门被裁定。
- 停止原因是 `RuntimeMonitor` 观测到前台进程变为 0（会话被锁定），属环境条件失效。
- pilot 与预检都不是正式样本，也不消费正式额度；只有 `attempt-ledger` 中的 claim 是额度记录。
