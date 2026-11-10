# R4-6 F1/F2 证据包（最终候选）

状态：candidate evidence；只登记原始字节位置与只读复核方式，不含接受裁定

日期：2026-11-10

返回 [本批报告](../2026-11-10-f1-f2-final-candidate.md)与 [New RPA 报告索引](../../README.md)。

## 1. 目录清单

| 目录 | 内容 | 说明 |
| --- | --- | --- |
| `out/evidence/r4/f1rows/<row>` | 每行的 `manifest.json`、configure/build/ctest 日志与命令 | 七行首次即通过的记录 |
| `out/evidence/r4/f1rows-retry/msvc-on-release` | 同一行的重跑记录 | 首次链接文件锁，重跑 `local_pass` |
| `out/evidence/r4/f1rows-short/<row>` | 三个 headless/shared 行的短路径 worktree 记录 | 同一 commit（`2c4e1274`），canary 路径问题在此不复现 |
| `out/evidence/r4/f1rows*.log` | 逐行 `exit`/用时汇总 | 与 `f1-summary.md` 一致 |
| `out/evidence/r4/f1-summary.md` | 11 行的状态、用时与证据路径总表 | 生成脚本只读 manifest |
| `out/evidence/r4/f1rows/<row>/<stamp>/manifest.json` | 每行的 source head、worktree 哈希、机器与预设 | 行级绑定 |

## 2. 只读复核

~~~powershell
python -B -c "import json,pathlib; [print(p.parent.parent.name, json.loads(p.read_text(encoding='utf-8'))['rows'][0]['status']) for p in sorted(pathlib.Path('out/evidence/r4').glob('f1rows*/*/*/manifest.json'))]"
python -B -c "import pathlib; print(pathlib.Path('out/evidence/r4/f1-summary.md').read_text(encoding='utf-8')[:400])"
~~~

复核只读取已归档字节，不重新运行行。

## 3. 边界

- 短路径 worktree 只改变 checkout 位置，commit、文件哈希与预设不变；manifest 中的 `source`
  记录同一 head 与 worktree 哈希。
- 失败尝试保留在原目录，未删除、未覆盖；`f1-summary.md` 只把 `local_pass` 的那次记为行结果。
- hosted 结果属于 GitHub Actions，不在本目录内；引用的是 `2c4e1274` 的 workflow 运行。
