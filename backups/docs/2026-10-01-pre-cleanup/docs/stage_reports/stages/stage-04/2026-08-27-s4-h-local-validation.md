# Stage 4 S4-H Local Validation

状态：S4-H 未关闭；本地 Debug/Release 门禁通过，最终 SHA 未发布，hosted 未跑；Stage 4 仍为 unblocked / not started

后续关闭证据：[Cuexis 阶段 4 验收报告](completion.md)。本文件是 hosted 发布前的本地快照，不代表最终 SHA。

执行日期：2026-08-27

权威计划：[Stage 4 实施计划](../../../stage_plans/completed/stage-04/plan.md) §7.8、§12。

## 1. 结论

S4-A 到 S4-G 的实现已在当前 worktree 上通过 S4-H 要求的本地 Debug/Release fresh configure、clean
build、完整 CTest、format、architecture、package ASCII/license、version、文档和 whitespace 门禁。
Linux ASan/UBSan 与三平台 hosted 仍未执行。

当前工作区相对 `stage-4` HEAD `1f8f0f7ce1706e42a848786178cf8780fa71ac8b`（S4-A 至 S4-D）还有未提交
的 S4-E/S4-F/S4-G 实现与文档。远端没有 `stage-4` 分支。因此没有可用于 hosted Linux Quality、
Windows MSVC、Windows MinGW 的最终 SHA。本报告不创建 Stage 4 completion report，不记录 owner
acceptance，不把 Stage 4 标为 completed，也不解锁 Stage 5。

## 2. 候选身份

| 项目 | 结果 |
| --- | --- |
| 本地分支 | `stage-4` |
| 已提交 HEAD | `1f8f0f7ce1706e42a848786178cf8780fa71ac8b` |
| HEAD 覆盖范围 | S4-A 至 S4-D |
| 未提交范围 | S4-E / S4-F / S4-G 与本 S4-H 本地证据 |
| 远端 `stage-4` | 不存在 |
| `origin/master` | `916552316ae3d0738af85ec56b572f2e789e03e2` |
| hosted final-SHA run | 未产生 |

GitHub API、`gh workflow list` 与 `git ls-remote`/`git push --dry-run` 在 2026-08-27 已恢复连通。
阻断是缺少已发布的完整 S4-A 至 S4-G SHA，不是 CI 权限或 workflow 缺失。

## 3. 本地验证

本地编译器为 MSVC 19.51、Ninja、`x64-windows`。Debug 使用 `--fresh` 后完整构建与 CTest；Release
使用 `--fresh` 与 `--clean-first`。

| 门禁 | 结果 |
| --- | --- |
| Debug fresh configure + build | 通过 |
| Debug CTest | `442 passed / 0 failed`；240.12 s |
| Release fresh configure + clean build | 通过 |
| Release CTest | `442 passed / 0 failed`；289.27 s |
| architecture | Debug/Release 各 `cuexis_architecture_tests` 通过 |
| format | `cuexis_format_check` 通过 |
| installed public-header ASCII / license | 由 7 个 external consumer / find_package 门禁覆盖，均通过 |
| version | `26.08.01-1` 一致 |
| documentation | `python -B tools/check_docs.py` 通过 |
| whitespace | `git diff --check` 通过；仅 core.autocrlf 的 LF/CRLF 提示 |

`cuexis_s4g_performance` 与 `cuexis_cfu_f4_performance` 在默认 CTest 中跳过探针本体并记为
Passed。Linux sanitizer、clang-tidy 与 coverage 仍由 hosted Linux Quality 执行。

## 4. 明确未关闭

- 最终 SHA 的 Linux Quality、Windows MSVC、Windows MinGW
- Stage 4 completion report 与项目所有者接受
- Stage 4 `completed` 状态切换
- Stage 5 unblocked

关闭 S4-H 之前必须：把完整 S4-A 至 S4-G 内容提交并推送到可观察分支，在该 SHA 上跑三套
workflow，记录 run URL 和第一失败步骤，成功后再写 completion report 并等待 owner acceptance。

本检查点不是完整 v4 动画 Playback 产品关闭，也不是 Shader、Studio 或 Judgement 开工。
