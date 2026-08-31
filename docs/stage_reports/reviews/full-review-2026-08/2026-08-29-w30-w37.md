# 260829 Full Review 整改 W30-W37

状态：local completed；2026-08-29 第 3 批 prepare 分层、诊断 taxonomy 与 parse-once 安全切片完成

本报告记录 Full Review 第 3 批 W30-W37 的本地实现与最终门禁证据。原始审查报告
`260829-full-review.md`、`docs/CURRENT_STATUS.md` 和整改计划均未修改；本报告不改变
Stage 4/5 状态。

## 基线与范围

- 实现分支：`260829-full-review`
- 实现提交：`01e6976801c41c466caa8342e57bf97a8249d366`
- W30：`AG-PREP-T`、`AG-PARSE-T` characterization
- W31-W35：`AG-PREP-CONTEXT`、`AG-PREP-STAGE1`、`AG-PREP-STAGE2`、`AG-PREP-DIAG`、
  `AG-PREP-TAXONOMY`
- W36：`AG-PARSE-OWNERSHIP` 的安全 ownership/parity slice
- W37：本地 Debug/Shared Debug/Release 批次门禁
- Findings：`PB-03`、`PB-04`（最终 code 归属）、`PB-14`、`PB-15`，以及 `CH-05`、`CH-06`、
  `CX-10` 的已验证 parse-once 前置切片。

## 实现与行为

- `PlaybackSession` prepare 现在通过内部 `PrepareContext`/`PrepareArtifact` 分组 source、
  typed chart、参数、resource leases、runtime/presentation candidate、frame 和 identity；
  candidate 在单点 commit 前不触碰 active state。
- prepare 的前后阶段明确了 load、parameter/capability preflight、animation/runtime、audio、
  presentation、frame/layout 和 identity 组装顺序；失败路径保持 candidate/active rollback。
- RAII diagnostics recorder 覆盖成功、早退、commit/layout failure 和异常转换；每次操作都会
  更新 `lastOperationDiagnostics`，保留稳定 code、cause、fieldPath 和排序。
- capability/limit 规则和 presentation taxonomy 采用表驱动校验；`frame.value_invalid` 保留
  为 boundary/Validation Sink code，production extract 只发实际 non-finite 诊断。
- Chart v4 canonical projection 增加已解析 `json::Value` 的 internal loader，避免
  serialize→parse 恢复 projection 的重复路径；新增 Chart/CXC diagnostics、canonical bytes
  和 source ownership characterization。

## 明确保留与未完成边界

FrameDigest v1-v3、PreparedSemanticIdentity、canonical bytes/order、golden、capability default、
Playback 公共观察面、owner-thread、异常模块边界和 runtime-script 无限期延后边界均保持不变。

本批没有宣称完整 ChartLoader/CXC parse-once 已关闭：characterization 仍记录
`ChartLoader` format peek、`ChartV4Resolver` concrete projection、`ChartWriter::writeV4` 和
CXC `isV4Chart` 的现有解析层次。后续 `AG-PARSE-CHART`/`AG-PARSE-CXC` 若要删除这些重复解析，
必须继续按 parity、diagnostics order、16 MiB ownership 和 hosted gate 单独实施。

## 验证

- Debug fresh configure/clean build 与最终 SHA 全量 CTest：`532/532` 通过；symlink 条件测试
  按平台跳过。
- Shared Debug fresh configure/clean build 与全量 CTest：`535/535` 通过；shared allocation
  注入按构型跳过，其他 shared/export/consumer gate 通过。
- Release fresh configure/clean build 与最终 SHA 全量 CTest：`532/532` 通过；symlink 条件
  测试按平台跳过。
- `cuexis_format_check`、`python -B tools/check_docs.py`、`python -B tools/update_version.py
  --check`、architecture/public-header/consumer gates 和 `git diff --check`：全部通过。

## 残余风险

D1 PB-01、D2 CX-01、D3 PB-04、D4 CH-03、D5 RT-04、D6 identity migration 仍 unresolved；
本批未改变任何决策门语义。完整 parse-once、identity migration、万级 render sorting、T1/T2/T3
触发批次、hosted Linux/MinGW/shader-tools revalidation 和 owner acceptance 仍是后续关闭条件。
