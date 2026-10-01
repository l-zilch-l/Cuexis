# S6-C1 Candidate source、CXC 与 typed lowering

状态：in_progress（本地 MSVC 实现和测试已通过；本报告不是 C1 退出证据）

日期：2026-09-22

本报告只记录 Stage 6 计划中的 S6-C1 本地实现。它没有把未运行的 MinGW、Linux sanitizer、
coverage 或 hosted 检查写成通过，也没有宣称 Stage 6、SDK `0.7.1` 或 owner acceptance 完成。

## 1. 执行基线

| 项 | 本次事实 |
| --- | --- |
| 工作区 | `C:/Users/Zilch/.codex/worktrees/7596/Cuexis` |
| 分支 | `stage-06-workspace` |
| 实现前 HEAD | `c3d6251b9af2d27aff0768b0e0c7fa452a721f8b` |
| PR | #28，`OPEN`，未合并 |
| 目标基线 | `master` `46b65d1f2345f543b98e7e87fe5ec9ed735f10bf` |
| SDK API | `0.7.0`，未升级 |
| 日期版本 | 先保持 `26.09.21-2`；UTC `2026-09-22` 的 pre-merge 报 `version.release_date.stale` 后前进为 `26.09.22-1` |
| 本地编译器 | MSVC 19.51.36256，CMake Ninja，`VCPKG_ROOT` 指向本机 vcpkg |

构建和测试都使用本工作区的 `out/build/debug` 与 `out/build/debug-candidate`。没有改
`D:/Cuexis` 或其他工作树。

## 2. 实现边界

- `CUEXIS_ENABLE_CHART_V5_CANDIDATE` 默认 OFF。OFF 的三个显式工厂返回稳定的
  `playback.candidate.disabled`，并且不读取 locator 或 package bytes。
- 公开工厂是 `fromFilesystemProjectEntry`、`fromCxcFileEntry` 和 `fromCxcMemoryEntry`。
  旧的 v4 工厂签名和默认语义没有改。
- `cuexis_cxc` 拥有显式 entry 选择和唯一一次 `packed::decode()`。Playback prepare 复用
  source 里已经校验过的 typed semantic，不读取 v4 JSON，不展开 CXT v2，也不再次 decode。
- `tools/cxc_common` 改为调用生产 `cuexis::cxc::validateCandidateChartExtension`。
  Playback 不链接 tools target。
- typed lowering 生成 `v5g1:` execution ID，保留完整 identity、parent、Requirement 和
  resource closure，并把 alpha 除以 255 写入 `RuntimeObject::renderableOpacity`。
  Runtime instantiation 使用这个字段；v4 对象保持默认 `1.0`。
- candidate prepared identity 使用域 `cuexis.prepared-semantic.v5.candidate.1`。输入是
  semantic identity 和按 AssetId 排序的实际资源 identity。v4 identity 函数没有改。
- prepare 失败只返回 `Result` 错误。active Session 要到 `commit` 才替换，失败路径不发布
  部分 `CandidateChart` 或 `CandidateRuntimeArtifact`。

## 3. 本地证据

| 检查 | 结果 |
| --- | --- |
| `git diff --check` | 通过；仅有工作区既有的 LF/CRLF 提示 |
| MSVC Debug candidate OFF：`cuexis_chart_tests` | `215` cases，`42214` assertions，通过 |
| MSVC Debug candidate OFF：`cuexis_cxc_tests` | `61` cases，`769` assertions，通过 |
| MSVC Debug candidate OFF：`cuexis_runtime_tests` | `39` cases，`1012` assertions，通过 |
| MSVC Debug candidate OFF：`cuexis_playback_tests` | `121` cases，`10615` assertions，通过 |
| `ctest --preset debug` | `692` 项中 `685` 通过。`cuexis_external_consumer_*` 七项在缺少 MSVC `LIB` 的 shell 中因 `LNK1104 kernel32.lib` 失败 |
| 同一七项在 `vcvars64` 中重跑 `ctest --preset debug -R cuexis_external_consumer` | 通过 |
| MSVC `out/build/debug-candidate`，`CUEXIS_ENABLE_CHART_V5_CANDIDATE=ON` | `cuexis_playback_tests "[playback][candidate]"`：`5` cases，`95` assertions，通过 |
| candidate lowering / bridge / opacity 过滤测试 | chart `[candidate]` `5` cases `366` assertions 通过；cxc `[candidate]` `2` cases `24` assertions 通过；runtime opacity case 通过 |

ON 配置覆盖了 CXC file/memory 与 filesystem project 的同一 Packed 内容、v4 默认工厂不选
candidate、错误 artifact identity、host parameter 失败不改变 active identity，以及 metadata
在 prepare 和 commit 之后仍可读取。

`2eb90bd` 推到 PR #28 后，GCC Release / ASan 因 `-Werror` 失败，MinGW debug 因
`playback_source.hpp` 未通过 clang-format 失败。原因是关闭构建里的未使用常量、GCC 对
`std::optional` 的 maybe-uninitialized，以及只在 candidate ON 测试里使用的辅助函数。这些是
编译门禁，不是测试逻辑已经在 Linux 上跑通。后续提交专门修这几项；在新的 hosted 运行变绿之前，
仍不能把 C1 写成退出。

## 4. 尚未验证

以下各项不能用上面的 Windows MSVC 结果代替：

- MinGW candidate OFF 的 fresh configure 和 clean-first build。
- Linux sanitizer、coverage 和 hosted Quality。
- 同一候选 SHA 的 Windows MSVC、Windows MinGW 和 external-consumer hosted 运行。
- C4 的 experimental 安装前缀、库名 flavor 和 `Cuexis_ALLOW_EXPERIMENTAL`。
- Stage 6 完成或 owner acceptance。

C2 及以后的批次仍按计划依赖图保持未完成。
