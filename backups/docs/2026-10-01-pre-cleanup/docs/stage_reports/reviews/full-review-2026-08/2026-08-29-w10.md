# 260829 Full Review 整改 W10

状态：completed；2026-08-29 文档状态合同 C1 完成

本报告记录 Full Review 第 1 批 Lane C 的 `AG-C1-STATUS-CONTRACT`。原始审查报告
`260829-full-review.md` 和 `CURRENT_STATUS.md` 均未修改；本任务不改变 Stage 4/5 状态。

## 基线与范围

- 基线 SHA：`4cd3dbfcff6c3dd60685ff8848896a084fe3d88a`
- 实现 SHA：`a74c43fc5a5617fc9ba21a40618110bd322fc0ed`
- 实现分支：`260829-full-review`
- 任务：`AG-C1-STATUS-CONTRACT`（W10，主智能体）
- Finding：`AP-16`；`AP-17` 的本地 checker 前置条件。

## 实现

- 新增 `docs/status_contract.json`，将 CFU/Stage 当前状态的 required fragments、stale
  fragments、状态快照日期和 dated files 集中为单一机器可读合同。
- `tools/check_docs.py` 读取并验证合同：合同路径不得越出仓库、引用文件必须存在、日期必须为
  有效 ISO 日期，stale fragment 受 `minLength` 约束。状态失败同时指出受检文件、合同部分和
  预期条件。
- required/stale 匹配保留原有的 whitespace-normalized 行为；短而宽泛的 `F next` 与
  `G3 next` 已去除，由已存在的完整状态短语覆盖，避免误报。
- `tools/check_docs_status_contract_tests.py` 覆盖正常的空白归一化匹配、required/stale/date
  三类可定位失败，以及短 stale fragment 的合同拒绝。

## 验证

- `python -B tools/check_docs_status_contract_tests.py`：通过，3 个测试。
- `python -B tools/check_docs.py`：通过，177 Markdown 文件和 20 个 candidate JSON/CXT 文件。
- `python -B tools/update_version.py --check`：通过，版本 `26.08.01-1` 一致。
- `git diff --check`：通过。

本任务只修改文档检查器和其数据合同，不涉及 CMake、公共头、安装面或产品代码，因此未运行
C++ Debug/Release 构建、CTest 或 hosted 门禁。

## 保留合同与残余风险

未修改 `CURRENT_STATUS.md`、历史审查报告、公共 API、SDK 版本、格式、identity、FrameDigest、
canonical bytes/order、capability、Playback 边界或 architecture allowlist。

`AP-16` 的硬编码状态事实和手工 checkpoint 日期已收敛到 JSON 合同。`AP-17` 尚未关闭：
`AG-C3-LINUX-CI` 仍需把 `check_docs.py` 和 `update_version.py --check` 接入 Linux Quality；
`AG-C2-TARGET-EXPORT` 也仍待 C1 后实施。D1 PB-01、D2 CX-01、D3 PB-04、D4 CH-03、D5 RT-04、
D6 identity migration 继续 unresolved；未实施依赖这些决策的行为变化。
