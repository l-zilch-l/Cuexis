# Gameplay Ruleset：I 收敛整理报告

状态：I convergence working report；文档收敛已启动，产品实现未启动

日期：2026-10-01

## 1. 入口依据

用户已解除“I 收敛前停止”要求。本次工作承接 [I 收敛前置审查](2026-10-01-i-entry-readiness.md)、
持续音符 C10-C14 证据和 [Fold Spike](2026-10-01-fold-spike.md)。研究性 spike 在 GCC/clang
下通过；该证据用于验证设计可行性，不被写成产品实现或生产限额。

## 2. 文档归属

| 内容 | 收敛后权威 | 研究证据 |
| --- | --- | --- |
| 取舍、威胁模型、版本边界 | [ADR 0043](../../../adr/0043-gameplay-judgement-ruleset-convergence.md) | 讨论记录、缺陷登记 |
| 字段与运行语义 | [Gameplay Judgement Spec](../../../formats/GAMEPLAY_JUDGEMENT_SPEC.md) | Program IR、Fold Calculus |
| 候选 typed C++ 边界 | [Gameplay Judgement ABI](../../../api/GAMEPLAY_JUDGEMENT_ABI.md) | Stage 7 plan |
| 预算与快照原则 | Spec §10；预算草案保留为研究输入 | C10-C14、代表性切片 |
| Hold/Slider grace | Spec §4；ADR 0043 §2 | Fold Spike §10、C10-C14 |

## 3. 已整理的决策

1. Hold/Slider 使用 prepared per-requirement grace，显式值可在 Ruleset 允许时覆盖默认值。
2. 严格边界、Slider `max(late, grace)` deadline、普通 consume 仲裁和 Hook 不追溯均已统一。
3. Pattern/Measure/L3 Fold 的层次、leftmost-first、Fact 规范排序、Hook 下一 Tick 生效已统一。
4. Identity 四分量、source 与最终值的分离、定点表登记和判定语义版本已统一。
5. 输入重采样、量程证明、快照无损、预算分层、Ruleset 包和稳定拒绝路径已统一。

## 4. 明确仍未完成

- 没有修改 `engine/`、公共安装头、CMake、Stage 7A 实现或 SDK API 版本。
- 真实内容预算、`back`/`sinusoid` 表参数、内联 AST 编码和作者 DSL 仍属于实现/测量工作。
- ADR 0043、Spec 和 ABI 仍是工作稿，需 owner acceptance 后才能成为接受合同。
- 稳定 C ABI、完整 Stage 7B 能力和 Chart v5 正式发行仍不在本报告范围。

## 5. 验证

本轮仅做文档一致性整理；完成 `python -B tools/check_docs.py` 与 `git diff --check` 后，才可
把本报告标记为文档批次完成。C++ spike 的既有 GCC/clang 结果不在本报告重复宣称。
