# 260829 Full Review 整改 C2/C3

状态：completed；2026-08-29 Lane C target facts 与 Linux CI 接线完成

本报告记录 Full Review 第 1 批 Lane C 的 `AG-C2-TARGET-EXPORT` 与 `AG-C3-LINUX-CI`。
原始审查报告 `260829-full-review.md` 和 `CURRENT_STATUS.md` 均未修改；本任务不改变
Stage 4/5 状态。

## 基线与范围

- 基线 SHA：`3ab13402f15e6f31851df11248e6bd8dde8f214d`
- C2 实现 SHA：`70d8b7ca8ac7c8c721269bd082e10312210eae67`
- C3 实现 SHA：`531f28791ded63d41cc71c69284cde0f39d13823`
- 实现分支：`260829-full-review`
- 任务：`AG-C2-TARGET-EXPORT`、`AG-C3-LINUX-CI`（主智能体）
- Finding：`AP-16`；`AP-17` 的 Linux Quality 接线。

## 实现

- 顶层 CMake 以 `file(GENERATE)` 从最终的 `CUEXIS_ACTIVE_TARGETS` 生成
  `${PROJECT_BINARY_DIR}/generated/cuexis-targets.txt`，不向源码树写入生成事实。
- `BUILDING.md` 使用明确的 begin/end 标记 text block 记录默认 `debug` 配置的完整 active
  target 顺序；检查器逐项比较该块与生成文件，并在首个差异处报告 expected/found。
- `check_docs.py` 同时读取 `CUEXIS_SDK_API_VERSION`，校验 BUILDING 中所有当前
  `find_package(Cuexis <major.minor> ...)` 示例与 SDK minor 一致。
- `tools/check_docs_target_contract_tests.py` 覆盖 target 顺序漂移和 SDK minor 漂移的正反例。
- Linux Quality 新增独立 `documentation-contracts` job：先仅 configure 默认 `debug` 以生成与
  BUILDING 同配置的 target facts，不运行 C++ build；随后以 `CUEXIS_TARGETS_FILE` 运行
  `python3 -B tools/check_docs.py` 与 `python3 -B tools/update_version.py --check`。

## 验证

- `cmake --preset debug --fresh`：通过。
- `cmake --build --preset debug --clean-first`：通过；后续 `cuexis_format_check` 退出码 0。
- 静态 Debug CTest：`cuexis_architecture_tests` 和 7 个
  `cuexis_external_consumer_*` consumer gate 全部通过，包含 installed public-header ASCII、
  Playback-only isolation 与 package consumer 验证。
- `cmake --preset shared-debug --fresh`、`cmake --build --preset shared-debug --clean-first`：通过。
- Shared Debug CTest：4 个基础/Playback external-consumer gate、`cuexis_shared_export_surface`、
  `cuexis_shared_consumer_imports` 与 `cuexis_shared_playback_consumer_imports` 全部通过。
- `python -B tools/check_docs_status_contract_tests.py`：通过，3 个测试。
- `python -B tools/check_docs_target_contract_tests.py`：通过，2 个测试。
- `python -B tools/check_docs.py`：通过，178 Markdown 文件和 20 个 candidate JSON/CXT 文件。
- `python -B tools/update_version.py --check`：通过，版本 `26.08.01-1` 一致。
- workflow 静态断言：通过，确认 `documentation-contracts` job 在 `clang-tidy` 前，具备默认
  Debug facts、`CUEXIS_TARGETS_FILE` 与两条检查命令。
- `git diff --check`：通过。

## 保留合同与残余风险

未修改 `CURRENT_STATUS.md`、历史审查报告、公共 API、SDK 版本、格式、identity、FrameDigest、
canonical bytes/order、capability、Playback 边界或 architecture allowlist。

`AP-16` 的状态文本和 `BUILDING.md` target/SDK 事实现在均有机器检查。`AP-17` 的 workflow
实现已提交，但本次未执行 GitHub hosted Linux job，且本机未安装 `actionlint`；hosted Linux
运行和 workflow 专用语法检查仍是待取得的外部证据，不能在本报告中标为已通过。D1 PB-01、
D2 CX-01、D3 PB-04、D4 CH-03、D5 RT-04、D6 identity migration 继续 unresolved；未实施依赖
这些决策的行为变化。
