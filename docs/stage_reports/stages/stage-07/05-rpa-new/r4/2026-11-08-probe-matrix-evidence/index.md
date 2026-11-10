# R4 展示矩阵探针证据包

状态：candidate evidence；两次登记均因环境停止，不含正式裁定

日期：2026-11-08

返回 [本轮报告](../2026-11-08-probe-matrix-admission-and-environment-stop.md)与
[New RPA 报告索引](../../README.md)。本页只登记原始字节位置与只读复核方式，
不新增测量结论。

## 1. attempt-1（候选 `00af7f90`）

| 目录 | 内容 | 说明 |
| --- | --- | --- |
| `out/evidence/r4/probe-20261107/build-release-b1` | configure/build 日志、CMakeCache、compile_commands、compiler、命令文本、目标依赖清单、两个 build binding | 正式构建身份 |
| `out/evidence/r4/probe-20261107/probe-pilot-v1` | `pilot-declaration.json`、每单元 warmup/sample 日志、原生环境、parsed、三份 proof、`preparation.json`、`formal.seal.json` | pilot 与封存身份 |
| `out/evidence/r4/probe-20261107/formal-run1` | 窗口 1 的 12 个完整进程、窗口 2 的 1 个完整进程与 1 个中止热身、逐进程 before/after 与 runtime 环境采样、`stopped.json` | 锁屏停止现场 |
| `out/evidence/r4/probe-20261107/precheck-identity-20261108` | `p4-capture-2`、`p4-capture-4`、`p6-topology` 的全规模非正式身份预检与 `precheck.json` | 只证明登记形状 |
| `out/evidence/r4/probe-20261107/build-release` | 首次构建的 configure/build 日志与 `source.json` | binding 阶段因构建期间新增文件被拒；未生成 binding，保留为过程记录 |
| `out/evidence/r4/probe-20261107/evidence-manifest.json` | attempt-1 快照：193 文件 / 261,491,981 B 的逐文件哈希 | 复核该快照用 |

## 2. attempt-2（候选 `b7edc959`）

| 目录 | 内容 | 说明 |
| --- | --- | --- |
| `out/evidence/r4/probe-20261108/build-release-b2` | 与 b1 同规则的 configure/build 与两个 build binding | 正式构建身份 |
| `out/evidence/r4/probe-20261108/remedy-verify-v1.json` | 900 秒补救验证，采样间隔 2 秒 | 判不完整（间隔超过冻结策略上限），保留 |
| `out/evidence/r4/probe-20261108/remedy-verify-v2.json` | 900 秒补救验证，1 秒采样、240 秒一次 1 px 往返 | 通过；893 个采样前台恒为 24,212 |
| `out/evidence/r4/probe-20261108/probe-pilot-v2` | pilot、三份 proof、四份 `retry-disposition`、`preparation.json`、`formal.seal.json` | 重登记身份 |
| `out/evidence/r4/probe-20261108/formal-run2` | 窗口 1、2 的 24 个完整进程、窗口 3 的 1 个完整进程与 1 个中止进程、逐进程环境、`stopped.json` | 人工切窗停止现场 |
| `out/evidence/r4/probe-20261108/remedy-run-hold-v1.json` | 整组期间后台持有的逐秒环境记录（含 nudge 时点） | 运行期补救证据 |
| `out/evidence/r4/probe-20261108/evidence-manifest.json` | 两批合并清单，逐文件 `path`/`bytes`/`sha256` | 本批总复核入口 |

`out/evidence/r4/attempt-ledger` 保存四个探针单元的 `attempt-1`/`attempt-2` claim 与 seal
marker；两次额度均已消耗，不能重置。

## 3. 只读复核

以下命令只读取已存在字节，不启动任何工作负载：

~~~powershell
python -B -c "import json,hashlib,sys; m=json.load(open('out/evidence/r4/probe-20261108/evidence-manifest.json',encoding='utf-8')); bad=[f['path'] for f in m['files'] if hashlib.sha256(open(f['path'],'rb').read()).hexdigest()!=f['sha256']]; print('files',len(m['files']),'mismatch',len(bad)); sys.exit(1 if bad else 0)"
~~~

`runner.py` 不提供重放模式；复核只针对已归档字节与 manifest 哈希，不重新运行样本。

## 4. 边界

- 两次登记都没有三窗口结果，因此没有任何重复性门、相对门或绝对门被裁定。
- attempt-1 的停止原因是会话锁定（前台变为 0）；attempt-2 的停止原因是前台切到 `explorer`。
  两者都是环境条件失效，不是产品语义、身份或性能问题。
- pilot、身份预检与补救验证都不是正式样本，也不消费正式额度；
  只有 attempt-ledger 中的 claim 是额度记录。
