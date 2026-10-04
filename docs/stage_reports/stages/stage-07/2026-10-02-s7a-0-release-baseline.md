# Stage 7A S7A-0：Release 基线证据

状态：completed（S7A-0 退出门禁的 Release 本地证据）

快照日期：2026-10-02

后续关闭证据：hosted 四作业（Linux Quality / Windows MSVC / Windows MinGW / Version Gate）结果、
MSYS2 本地复跑结果。**本次未运行**，见 §5。

上级文档：[Stage 7A 实施计划](../../../stage_plans/active/stage-07/plan.md)（§S7A-0 退出门禁）·
[owner 接受记录](2026-10-02-s7a-0-acceptance.md) · [Debug 执行基线](2026-10-02-s7a-0-baseline.md)

## 1. 目的

计划的 S7A-0 退出门禁要求"基线 Debug/Release、文档和既有 package/architecture gate 结果已记录"。
[Debug 基线](2026-10-02-s7a-0-baseline.md) 已记录同一工作树状态的 Debug 结果；本报告补齐 **Release** 一侧。
两份报告是同一天、同一 HEAD、同一工作树状态的独立证据，**不互相追写**。

## 2. 被验证的工作树状态

| 项 | 值 |
| --- | --- |
| HEAD | `449e864`（与 Debug 基线一致） |
| 分支 | `stage-7` |
| `master` 对照 | `5472c46` |
| D-9 候选补丁 | **存在且未提交**：工作区 `tools/check_version_gate_tests.py`（+119/−3）。因此本次 Release 结论适用于"含候选补丁"的工作树，不是干净 `HEAD` |
| 文档改动 | 未提交的 Markdown 与未跟踪的 `docs/proposals/gameplay-v2-acceptance/`、`docs/proposals/research/gameplay-v2/`、`docs/stage_reports/stages/stage-07/`；不参与 C++ 构建 |

## 3. 执行命令与结果

执行的命令序列（Visual Studio 18 Community `vcvars64.bat` 环境下）：

```powershell
cmake --preset release --fresh
cmake --build --preset release
ctest --preset release --no-tests=error
```

| 项 | 结果 |
| --- | --- |
| 命令总退出码 | `RELEASE_EXITCODE=0` |
| 配置 | `--fresh` 成功；`VCPKG_ROOT=D:\vcpkg`，vcpkg baseline `40f3c709db80acf154ac4b17a1f83c564ebd022e` |
| 构建 | 291 个构建步全部完成，无错误 |
| 测试总数 | **749** |
| 通过 | **749 / 749（100%）** |
| 失败 | **0** |
| 跳过 | 1 项：`69 - Secure file rejects physical containment escapes through symlinks (Skipped)` |
| CTest 总耗时 | `Total Test time (real) = 525.90 sec` |
| 输出目录 | `out/build/release/bin/` |

### 3.1 关键门禁在 Release 下的表现

| 测试 | 索引 | 结果 | 耗时 |
| --- | --- | --- | --- |
| `cuexis_contract_version_gate` | #748 | Passed | 12.61 sec |
| `cuexis_contract_s6_a2` | #749 | Passed | 0.09 sec |

版本门禁是 D-9 缺陷的落点：#748 在 Release 下同样需要 Posix shell 才能跑正例。
本机默认 `PATH` 下 Debug 曾因此失败（见 [Debug 基线](2026-10-02-s7a-0-baseline.md) §4.2），
而本次 Release 通过与"候选补丁已在工作区生效"这一事实绑定——
对不含补丁的干净 `HEAD`，Release 侧尚无独立证据。

### 3.2 耗时分布（Label Time Summary 摘录）

| 标签 | 累计耗时 | 测试数 |
| --- | --- | --- |
| `external` / `package` | 479.80 sec | 9 |
| `cxc` | 136.41 sec | 6 |
| `playback` | 134.97 sec | 6 |
| `cfu-f` | 134.89 sec | 5 |
| `cfu-f2` | 134.40 sec | 2 |
| `headless` | 134.45 sec | 3 |
| `s6-c4` | 66.03 sec | 3 |
| `reference-host` | 50.95 sec | 2 |
| `player-distribution` | 15.08 sec | 1 |
| `version-gate` | 12.61 sec | 1 |

外部消费者与打包类门禁占总时长的大头（约 91%），与 Debug 侧观察一致。

## 4. 与 Debug 基线的对照

| 项 | Debug | Release |
| --- | --- | --- |
| 测试总数 | 749 | 749 |
| 通过 | 749（100%） | 749（100%） |
| 失败 | 0 | 0 |
| CTest 总耗时 | 469.17 sec | 525.90 sec |
| 跳过项 | `68 - Secure file rejects physical containment escapes through symlinks` | `69 - 同一测试名` |
| `cuexis_contract_version_gate` | Passed（12.97 sec，索引 #748） | Passed（12.61 sec，索引 #748） |

**事实差异**：同一测试名在 Debug 下索引为 68、在 Release 下为 69。两侧总数都是 749，
索引整体一致（#748/#749 相同），本次未追查该单项索引偏移的原因，登记为本报告的未解释观察。

## 5. 未执行项（不得据本报告推断通过）

| 项 | 状态 |
| --- | --- |
| hosted Linux Quality / Windows MSVC / Windows MinGW / Version Gate | **未运行** |
| MSYS2 / MinGW 本地复跑 | 未运行 |
| `--clean-first` | **未使用**：AGENTS.md 建议 Release 在合并/版本化前用 `--clean-first`，本次为 `--fresh` + 普通构建。偏差已登记，是否需要在关闭前重跑由 owner 决定 |
| sanitize / coverage / clang-tidy 预设 | 未运行（已知仅 Linux/Clang/GCC 可用） |
| GPU / 真实设备 / 音频硬件 / 网络输入 | 未执行；`cuexis_player` 的 smoke 路径需要 GPU |
| 干净 `HEAD`（不含 D-9 候选补丁）的 Release 结果 | 未取得 |

## 6. 结论

Release 侧在**当前工作树（含未提交的 D-9 候选补丁）**上达成可重复的 749/749 全绿，
配置、构建、全部 CTest 门禁（含架构、目标依赖、安装清单、package 与版本门禁）均通过。
这补齐了 S7A-0 退出门禁中"Release 基线"的本地一半；**hosted 四个作业与干净 `HEAD`
的 Release 结果仍是空缺**，因此本报告不构成 S7A-0 退出的完整证据。

## 7. 相关索引

- [Debug 执行基线](2026-10-02-s7a-0-baseline.md)
- [owner 接受记录](2026-10-02-s7a-0-acceptance.md)
- [D-9 候选补丁与验证证据](2026-10-02-d9-shell-guard-candidate.md)
- [Stage 7A 实施计划](../../../stage_plans/active/stage-07/plan.md)
