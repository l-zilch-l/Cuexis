# Stage Chart Format Update CFU-C4 Tools Report

状态：CFU-C4 本地实现与门禁完成；hosted 跨平台收口待验证

快照日期：2026-08-13

实现提交：`629a6df Fix MinGW unused test helper`；该 SHA 是本轮 hosted workflow 的验证基线。

权威计划：[Stage Chart Format Update 实施计划](../stage_plans/stage_chart_format_update_implementation_plan.md)

## 1. 结论

本轮完成 CFU-C4 的 developer-only CXC 工具和本地验收闭环：

```text
cuexis_cxc_pack
cuexis_cxc_validate
cuexis_cxc_unpack
```

工具复用现有内部 `cuexis_cxc` Reader/Writer 和 closure 验证，不创建公共 `Cuexis::Cxc`
component，不接入 Playback/Runtime，不解析 `engine/animation/`，也不执行 migration、authoring
generator 或运行时脚本。当前 hosted workflow 尚未完成，本报告不把 CFU-C4 写成最终跨平台
complete。

## 2. 交付范围

### 2.1 CLI 合同

- exit `0`：成功。
- exit `1`：CXC 内容无效。
- exit `2`：参数、I/O、路径冲突或目标提交失败。
- 成功摘要写 stdout；诊断写 stderr。
- 诊断只包含稳定 code、field path 和 package-relative path，不输出宿主绝对路径。

### 2.2 Pack

- 输入为 Source Project 根，输出为独立 `.cxc`。
- 先读取并保存 ProjectConfig、全部声明 Asset Index、全部声明 asset source、入口 Chart 和
  Chart 引用的 CXT，形成 bounded source snapshot。
- 保留 source bytes，不迁移 Chart、不裁剪 Asset Index、不执行脚本。
- Writer 输出先写 sibling staging file，reload 完整验证后再替换目标；失败清理 staging 并恢复
  原有 output。
- 输入目录和 output package 冲突时拒绝。

### 2.3 Validate

- 只读输入，不生成或修改输出。
- 直接调用现有 `CxcPackageLoader`，覆盖 envelope、manifest、Project、Asset Index、Chart、CXT
  和 resource closure 验证。

### 2.4 Unpack

- 先完整验证 package，再恢复包内 source tree。
- 已存在且非空目标拒绝；现有目标内容不被覆盖。
- 先创建 output sibling staging directory，逐 entry 写入并重新读取校验，成功后原子提交。
- staging 写入或 commit 失败时清理临时目录，已有目标保持不变。

## 3. 测试与本地证据

### 3.1 CMake script-mode gate

新增 `cmake/VerifyCxcTools.cmake` 和 `cuexis_cxc_tools` 测试，覆盖：

- canonical source pack 与 committed golden byte equality；
- validate success、invalid content、missing input 和 argument error 的 exit `0/1/2`；
- validate 不改变工作目录文件；
- pack input/output conflict；
- invalid Project diagnostic 和 linked/reparse source-root rejection；
- unpack non-empty target/no-overwrite；
- pack -> validate -> unpack -> repack byte equality；
- valid noncanonical package unpack 后重新 pack 得到 canonical bytes；
- staging failure、commit failure、pack replacement failure 的 cleanup/rollback；
- rollback restore failure 的稳定诊断和原始 backup 保留；
- stdout/stderr 不泄露 source/build host path，包括 Windows native separator/case 变体。

### 3.2 本地验证矩阵

| 检查 | 结果 | 说明 |
| --- | --- | --- |
| Debug fresh configure | passed | `cmake --preset debug --fresh` |
| Debug complete build | passed | Visual Studio Developer environment |
| Debug full CTest | passed | `341` passed；1 个既有 Windows symlink 能力测试跳过 |
| Debug CXC/C4 filter | passed | `1` 个 `cuexis_cxc_tools` 测试 + `18` 个 CXC cases（共 19 个测试） |
| Release fresh configure | passed | `cmake --preset release --fresh` |
| Release clean build | passed | `cmake --build --preset release --clean-first` |
| Release CXC/C4 filter | passed | `1` 个 `cuexis_cxc_tools` 测试 + `18` 个 CXC cases（共 19 个测试） |
| Debug/Release format check | passed | `cuexis_format_check` |
| Documentation check | passed | 117 Markdown、20 candidate JSON/CXT |
| Version check | passed | `26.08.01-1` |
| Diff check | passed | 仅既有 LF/CRLF 转换警告 |

### 3.3 Hosted 尚未形成的证据

截至报告生成时，`4581289` 的 hosted workflow 尚未完成，因此以下仍未形成有效证据：

- hosted Linux Quality、GCC/Clang、ASan/UBSan、coverage；
- MinGW 和 hosted Windows C4 tool build/test；
- 当前实现 SHA 对应的跨平台 canonical CXC SHA-256 比较。

这些未验证项阻止将 CFU-C4 标记为最终跨平台 complete，但不否定本地实现和门禁结果。

## 4. 兼容边界

本轮保持以下边界：

```text
无公共 Cuexis::Cxc component
无 PlaybackSource / prepare / reload CXC 接入
无 Chart v3 -> v4 migration
无 Stage 4 animation runtime
无 runtime scripts、per-frame callbacks 或 bytecode
```

下一步是推送本轮提交并在对应 hosted Linux/Windows workflow 上跟踪 C4 门禁；在这些
证据形成前，不进入 CFU-D、CFU-E 或 Stage 4。
