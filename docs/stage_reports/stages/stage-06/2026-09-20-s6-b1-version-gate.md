# S6-B1 版本比较与发行门禁

状态：completed（bootstrap、受保护 required check、同 SHA hosted 验证与合并后审计已完成）

日期：2026-09-21

本报告只记录 Stage 6 计划中的 S6-B1。B1 已将版本比较器、focused 反例测试、trusted-baseline
workflow 和发行 checklist 落盘，并完成当前工作区的本地验证；它没有把本地绿色测试写成
受保护仓库门禁，也没有宣称 Stage 6 或 owner acceptance 完成。

## 1. 执行基线

| 项 | 本次事实 |
| --- | --- |
| 工作区 | `C:/Users/Zilch/.codex/worktrees/7596/Cuexis` |
| 起始 HEAD | `e7d6c1ea3ff950226b7a3582eb128cb2d01800fb` |
| B1 实现提交 | `4fd46adf2eff454bc6e43fee78db36e3310254aa` |
| 起始状态 | detached HEAD；B1 报告创建前未创建 PR、未合并、未发布 |
| 可信 UTC 日期 | `2026-09-20` |
| 日期版本变化 | `26.08.01-1` -> `26.09.20-1` |
| SDK API | `0.7.0`，未提前升级到 `0.7.1` |
| 可信合并基线 | `e3c4589c62af36f29ae5e8ebda62fe37fadbf7e7` |
| 环境 | Windows x64、MSVC 19.51.36256、CMake 4.3.3、Ninja 1.13.2、`VCPKG_ROOT=D:/vcpkg` |

所有命令和构建产物均限制在本工作区；没有读取或复用 `D:/Cuexis` 或其他 worktree 的构建产物。

## 2. B1 产物

| 产物 | 位置 | 实施边界 |
| --- | --- | --- |
| 版本比较器 | `tools/check_version_gate.py` | 比较 trusted baseline 与 candidate commit，不从候选树选择 checker |
| focused 测试 | `tools/check_version_gate_tests.py` | 正例、负例、历史复验、Git ref 和 workflow trust 合同 |
| CI workflow | `.github/workflows/version-gate.yml` | PR、merge queue、合并后 push 审计和显式 historical revalidation |
| 发行 checklist | `docs/guides/VERSIONING.md` | 本地命令、hosted 边界、SDK 与日期版本分离 |
| 初始 B1 版本源 | `cmake/CuexisVersion.cmake`、`vcpkg.json` | 两处规范版本同步为 `26.09.20-1` |

比较器按规范化 `(year, month, day, build)` 比较，不比较显示字符串或 Debug suffix。它拒绝
无效或缺失的完整 SHA、非祖先基线、CMake/manifest 漂移、基线未来日期、候选过期或未来日期、
同日未精确递增、跨日非 build 1，以及未经显式接受的 SDK API 变化。`--context historical`
只使用记录的 UTC 日期，不把历史复验当作新的发行。

workflow 的 pre-merge 检查使用事件提供的目标分支 SHA 和 `github.sha`，开启完整历史；
checker、测试、`update_version.py` 和 workflow 自身从 trusted baseline materialize。若基线
缺少任一文件，workflow 以 `version.bootstrap.required` 失败，禁止回退到候选树执行检查。
合并后 push 只做漏审计；`workflow_dispatch` 使用明确传入的 baseline、candidate 和记录日期。
focused 测试在 trusted 临时目录执行时通过 `GITHUB_WORKSPACE` 使用真实 Git 根目录，并通过
`CUEXIS_TRUSTED_ROOT` 读取 trusted workflow；三个 workflow 入口都导出该变量，避免测试因
临时 materialized 目录不是 Git checkout 而失效，也避免误读候选树 workflow。

## 3. 版本与兼容结果

本次按可信 UTC 日期 `2026-09-20` 将显示版本从 A1 基线的 `26.08.01-1` 修正为
`26.09.20-1`。日期版本的变化没有隐式改变 SDK API、内容格式或 ABI；生成头和安装 package
继续报告 SDK API `0.7.0`。`0.7.1` 仍只是满足 ADR 0042 兼容条件后的目标，Stage 8 不预留
`0.8.0`。

Release 安装树已核对：

| 元数据 | 实际值 |
| --- | --- |
| display/canonical version | `26.09.20-1` |
| SDK/package version | `0.7.0` |

## 4. Focused 验证矩阵

`python -B tools/check_version_gate_tests.py` 实际运行 11 个测试，结果为 `11/11 passed`。
覆盖内容包括：

- 合法日历解析和非法 trusted 日期；
- 同日 build 必须精确加一，跨日必须为 build 1；
- unchanged、skipped、backward、日期倒退、跨日错误 build 的拒绝；
- live context 对 stale/future release date 的拒绝；
- historical context 使用记录日期；
- SDK API 变化默认拒绝，显式允许路径单独验证；
- CMake 与 manifest 漂移拒绝；
- 非完整 SHA、缺失 commit、非祖先基线拒绝；
- workflow 使用事件基线、完整历史、bootstrap 失败和不信任候选测试文件。

当前树检查结果：

| 命令 | 结果 |
| --- | --- |
| `python -B tools/check_version_gate_tests.py` | 通过，11 tests，退出码 0 |
| `python -B tools/check_version_gate.py --check-current --json` | 通过，`26.09.20-1` / SDK `0.7.0` |
| `python -B tools/update_version.py --check` | 通过，CMake 与 manifest 一致 |
| `python -B tools/check_stage6_a2.py` | 通过；A2 schemas、fixtures、goldens 和边界未回归 |
| `git diff --check` | 通过；仅有 Git 的 LF/CRLF 转换提示 |

## 4A. Hosted bootstrap and protected evidence

Bootstrap PR #26 was merged as `4545742ed63ae2d8f11ad07e80930ce5b88fa0ce`. Its pre-merge
Version Gate intentionally failed with `version.bootstrap.required` because the trusted
baseline predated the checker; this was the documented bootstrap exception. `master` was then
protected with strict latest-base enforcement, admin enforcement, and required check
`Version advancement (pre-merge)`.

Candidate PR #27 used candidate SHA `d4697549a50e9c517ac393c27786826aa43ce9cc` and trusted
baseline `4545742ed63ae2d8f11ad07e80930ce5b88fa0ce`. Protected Version Gate run `35586930775`
passed. The same candidate SHA passed Linux Quality, Windows MSVC, and Windows MinGW, and PR #27
merged as `master` SHA `46b65d1f2345f543b98e7e87fe5ec9ed735f10bf`. The resulting master push
passed Version Gate post-merge audit run `35590201888`.

在修正 trusted materialized test 的 workspace 路径后，使用非 Git 临时目录模拟 hosted
`TRUSTED_ROOT`，并设置真实 `GITHUB_WORKSPACE`，`check_version_gate_tests.py` 再次通过
`11/11 passed`。这验证了测试读取 trusted workflow，同时从真实 checkout 读取 Git 历史。

## 5. 构建、安装和 CTest 结果

版本源变化后执行了 Release fresh configure 和 clean-first build；随后在修正 workflow trusted
路径后重新执行了 Debug fresh configure 和 clean-first build：

```powershell
cmake --preset release --fresh
cmake --build --preset release --clean-first
ctest --preset release --no-tests=error
```

结果：两套配置的 configure/build 通过；Release 与最终 Debug 均为 `683/683 passed`，各有
1 个相同的 Windows symlink containment 用例按环境跳过。Debug 首次
全量调用曾因 `cuexis_cxc_tools` staging commit 的环境调用失败；补充正确的
`VCPKG_ROOT=D:/vcpkg` 后独立重跑通过，未修改测试、golden 或实现来掩盖该环境性失败。

两套配置均实际覆盖 architecture、external consumer、package、headless、CXC、Playback、
FrameDigest v1-v3、portable presentation、prepare/reload 及既有 v4 回退测试。Windows 不
替代 Linux Quality、sanitizer、coverage 或 hosted MinGW 证据。

## 6. Hosted / 保护结果

本地实现和本地验证之外，B1 已取得受保护仓库和同 SHA hosted 证据。最终只读快照为：

| 项 | 结果 |
| --- | --- |
| `origin/master` | `46b65d1f2345f543b98e7e87fe5ec9ed735f10bf` |
| `bootstrap PR #26` | merged as `4545742ed63ae2d8f11ad07e80930ce5b88fa0ce` |
| `candidate PR #27` | merged as `46b65d1f2345f543b98e7e87fe5ec9ed735f10bf` |
| required check | `Version advancement (pre-merge)` |
| strict latest-base | enabled |
| admin enforcement | enabled |
| post-merge audit | Version Gate run `35590201888`, success |

GitHub API 在本次收尾查询中间歇性 TLS 超时，但重试后已取得 PR、run、保护配置和合并
ref 证据。bootstrap push audit 的失败是预期的旧基线审计结果；正式 protected pre-merge
Version Gate 和合并后 Version Gate audit 均成功。

## 7. 结论与下游边界

B1 的本地实现、正反例、版本源一致性、安装元数据、受保护 Version Gate 和同 SHA hosted
回归均已取得证据；B1 已完成。没有修改 ADR 0042、
R5 允许消费范围、旧 v4 identity、FrameDigest v1-v3 golden、稳定 C ABI 或正式 v5 Writer。

Stage 6 仍不能因此关闭。C1、D1、C2、E1/E2、C3、C4 仍按计划依赖图推进；B1 的本地脚本可以作为
后续批次的版本变化门禁，但不能替代其各自的合同、失败路径和 external consumer 验收。
