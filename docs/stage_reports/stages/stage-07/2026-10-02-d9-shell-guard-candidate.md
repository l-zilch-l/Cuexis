# D-9 候选补丁与验证证据：version-gate 的 POSIX shell 判据

状态：candidate patch + dated evidence（**未提交、未落 `master`**；等待 owner 按 ADR 0042 复核）

更新日期：2026-10-02

上位文档：[S7A-0 执行基线与合同表征](2026-10-02-s7a-0-baseline.md)（§4.2 记录了本缺陷的原始失败）、
[未决问题清单 D-9](../../../proposals/gameplay-v2-acceptance/OPEN_QUESTIONS.md)

本文只记录 D-9 的**候选修复与其验证证据**。它不宣称门禁已修好：按 ADR 0042，修改门禁本身须经
代码所有者复核并配独立负例，而本仓库没有 `CODEOWNERS`；并且该文件是 workflow 的
6 个 trusted-copy 文件之一，修复必须先存在于可信基线（已合入 `master`）才会在 CI 中生效。

## 1. 裁决来源

本轮修复方案不是自拟的，而是向同工作区的 Codex 顾问（`ask-codex` skill）请求裁决后按其
决策卡执行。裁决记录：

| 项 | 值 |
| --- | --- |
| 通道 | ai-link CLI（`health` = 通道正常，无告警） |
| thread | `d9-version-gate-shell-guard` |
| 模型 | `gpt-6-astra`（`difficulty=hard`、`reasoning=medium`） |
| workspace | `D:\Cuexis-worktree`（卡内 workspace 校验 = `Codex cwd = D:\Cuexis-worktree`，与提问一致） |
| verdict / confidence | `adopt` / `0.98` |
| 输入的取证简报 | `.dsh/scratch/d9-codex-brief.md`（工作区内的临时材料，`.dsh/` 已 gitignore） |

Codex 的结论（原文摘要）：采用 **B + A**，作为一次小的、**master 优先**的可信基线修复，
并配一条独立的跨文件系统视图负例与具名 owner 复核证据。

## 2. 变更内容

只修改一个文件：`tools/check_version_gate_tests.py`（+119 / -3）。
其余 5 个 trusted-copy 文件（`tools/check_version_gate.py`、`tools/update_version.py`、
`.github/workflows/version-gate.yml`、`cmake/CuexisVersion.cmake`、`vcpkg.json`）**逐字未动**，
可用 `git diff --stat` 复核。

| 组件 | 对应 Codex action | 做法 |
| --- | --- | --- |
| B：视图一致性探针 | 1 | 新增 `reads_marker_through_native_path(shell)`：在临时目录写标记文件，要求候选 shell 用**本机绝对路径写法**读到它（`test -f <path>`）。Git Bash/MSYS2 会翻译 `C:\...`，Linux 的 POSIX 路径原样可读，两者都保持可用；WSL 无法解析该字符串，因此被拒绝 |
| A：显式黑名单（纵深防御） | 2 | 新增 `is_system_bash(shell)`，把候选 shell 解析后的绝对路径与 `%SystemRoot%\System32\bash.exe` 做大小写不敏感比较；与既有的"文件名含 `wsl`"检查并列 |
| 判据接入 | 1、2 | `usable_posix_shell()` 在原有 `exit 0` 与 `git --version` 两个探针之后追加视图探针；任一探针失败仍返回 `None`，即保持"无可用 shell → `skipTest`"的既有语义 |
| 独立负例 | 3 | 新增 `test_rejects_a_shell_that_cannot_read_the_workspace_path`：在临时目录造一个 stand-in（Windows 用 `bash.cmd`，POSIX 用 `#!/bin/sh`），它对平凡 `-c` 探针返回 0、对标记探针返回 1，断言它**必须被拒绝**。该负例自带 stand-in，不依赖宿主机是否真的有 WSL |
| 可诊断性 | — | 既有 `skipTest` 消息补充了新判据（"or it cannot read a marker through the native workspace path"），使跳过原因可分辨 |

**为什么不是其它方案**（Codex 已裁决，未自行推翻）：C（固定解释器）会无必要地收窄支持环境；
D（用 Python 复刻 bootstrap 语义）与"正例必须证明 workflow 里的 shell 块真能执行"直接冲突，
会让负例变成空转；E（把 trusted-copy 校验整体移入 checker）改的是门禁架构，超出 D-9 范围。

## 3. 验证证据

命令均在 `D:\Cuexis-worktree` 下执行。

### 3.1 判据的区分力（改文件之前的取证）

| shell | `test -f '<本机路径>'` | `git --version` |
| --- | --- | --- |
| `C:\Program Files\Git\bin\bash.exe` | rc=0（读得到） | rc=0 |
| `C:\Windows\System32\bash.exe`（WSL 启动器） | **rc=1**（读不到） | **rc=0**（旧判据因此放过它） |

### 3.2 四种 shell 场景（补丁后）

| 场景 | PATH 条件 | 结果 |
| --- | --- | --- |
| (a) 本机默认 | `bash` = `C:\Windows\System32\bash.exe` | `Ran 20 tests ... OK (skipped=2)`：bootstrap 正例**正确跳过**（不再假失败），新负例**真实执行并通过** |
| (b) Git Bash 优先 | `C:\Program Files\Git\bin` 置首 | `Ran 20 tests ... OK`（**0 跳过**）：bootstrap 正例真实执行、materialize 6 个文件并通过；新负例通过 |
| (c) 无 bash | PATH = Python 目录 + `C:\Program Files\Git\cmd` | `Ran 20 tests ... OK (skipped=2)`：**保持跳过而非失败**（约束 4） |
| (d) 门禁 CTest | 默认 | `cuexis_contract_version_gate ... Passed`；`100% tests passed, 0 tests failed out of 1` |

场景 (a) 的 2 个跳过是同一原因的两处表现：宿主机只有 WSL shim，没有可用 POSIX shell。
`test_usable_posix_shell_rejects_a_shell_that_cannot_run` 依赖"存在一个可用 shell"，因此也跳过——
这是该用例既有的 `skipTest` 分支，不是新引入的失败。

### 3.3 全量本地回归

`ctest --preset debug --no-tests=error`（同一工作区，含本补丁）：

- `100% tests passed, 0 tests failed out of 749`；`Total Test time (real) = 469.17 sec`；退出码 0；
- `748/749 Test #748: cuexis_contract_version_gate ... Passed 12.97 sec`——即 S7A-0 基线中
  **749 项里唯一失败的那一项**已由红转绿；
- 基线同批的既有跳过项仍在：`68 - Secure file rejects physical containment escapes through symlinks (Skipped)`。

必须说清楚这条绿的来源：本机默认 PATH 下只有 WSL shim，bootstrap 正例是**按设计跳过**（§3.2 场景 (a)），
门禁项因此不再假失败。**正例真正执行并通过的证据来自场景 (b)**（Git Bash 优先、0 跳过），
两者不可互相替代。

### 3.4 失败的第一次实现（如实保留）

负例的第一版在 Windows 上用裸 `findstr`，在场景 (c) 的裁剪 PATH 下 `findstr` 不可解析，
stand-in 于是对标记探针也返回 0，导致 `AssertionError: 0 != 255` 的**假失败**。
已改为 `"%SystemRoot%\System32\findstr.exe"` 绝对路径调用，使 stand-in 不依赖调用方 PATH 能解析
任何外部工具。该情形已写入代码注释，避免后人回退。

## 4. Codex 提出的风险与本轮处置

| 风险 | 处置 |
| --- | --- |
| 路径探针必须使用与 shell 相符的写法，否则 Git Bash/MSYS2 与 Linux 会变成**假跳过** | 探针使用"本机绝对路径字符串"（正是 bootstrap 块收到的写法），并用 `shlex.quote` 引用；场景 (b) 证明 Git Bash 仍被接受，场景 (a) 证明 WSL 被拒；Linux 下路径本身是 POSIX 形式，无需转换 |
| 不得把 `git --version` 或平凡退出当作"工作区可见"的证据 | 视图探针作为**独立的最后一道**判据，与两个平凡探针并列而非替代 |
| 六个 trusted-copy 文件需保持字节一致，并重跑四条 job 路径与无 shell 跳过路径 | 实测只有 `tools/check_version_gate_tests.py` 变化（§2）；本地覆盖了默认 / Git Bash / 无 bash / CTest 四条路径（§3.2）；**hosted 的四条 job 未重跑**（§5） |

## 5. 未执行项（不得据此推断通过）

| 项 | 状态 | 原因 |
| --- | --- | --- |
| 落到 `master`（Codex action 4） | **未执行** | 需要提交与 PR；本会话按 owner 选择不提交。且按仓库纪律，门禁改动须先进入可信基线 |
| 具名 owner 复核证据（Codex action 5） | **未执行** | 需要 owner 对确切 SHA 作出具名批准；仓库无 `CODEOWNERS`，ADR 0042 的复核通路本身即 D-9 之外的独立阻塞 |
| Linux Quality / Windows MSVC / Windows MinGW / Version Gate 四条 hosted job | **未执行** | 本机无法触发 hosted；本地四条路径只是代理证据 |
| MSYS2（MinGW）本地复跑 | **未执行** | 本机可用的 MinGW preset 与 hosted 环境不同，未纳入本轮 |

owner 处置决定（2026-10-02）：**保持未提交**，等候按 ADR 0042 具名复核后再定落库路径。
本轮不提交、不落 `master`，工作区改动即为评审对象。

## 6. owner 复核所需材料（对应 Codex action 5）

1. 本报告 §2 的改动清单与 `git diff -- tools/check_version_gate_tests.py`（+119 / -3，单文件）；
2. §3.2 的四种 shell 场景原始输出；
3. §3.3 的全量本地 CTest 结果（同一 SHA，聚焦项由红转绿且无新增失败）；
4. 新负例 `test_rejects_a_shell_that_cannot_read_the_workspace_path` 的独立通过记录；
5. 确认"无可用 shell 仍为跳过"（场景 (c)），未被改成失败；
6. 确认其余 5 个 trusted-copy 文件未改动（`git diff --stat`）。

## 7. 复现与回滚

```powershell
# 复现四种场景
python -B tools/check_version_gate_tests.py                                  # (a) 默认
$env:PATH = 'C:\Program Files\Git\bin;' + $env:PATH
python -B tools/check_version_gate_tests.py                                  # (b) Git Bash 优先
# (c) 无 bash：PATH 只留 Python 目录与 C:\Program Files\Git\cmd
ctest --preset debug -R cuexis_contract_version_gate                         # (d) 门禁项

# 回滚（本补丁尚未提交，回滚即丢弃工作区改动）
git checkout -- tools/check_version_gate_tests.py
```
