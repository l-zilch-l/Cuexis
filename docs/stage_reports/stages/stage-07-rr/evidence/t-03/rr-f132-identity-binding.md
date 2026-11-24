# RR-F132 身份映射与 T-10 迁移绑定

状态：evidence

更新日期：2026-11-23

<!-- cuexis-meta
id: EVIDENCE-stage-07-rr.t-03-identity-binding
status: evidence
authority: none
date: 2026-11-23
-->

本文件只读 `engine/judgement/src/execution_kernel_prepare.cpp` 与 identity factory 的实际构造，绑定
“改动前”的 token/preimage 结构。未运行测试，未取得十六进制身份字节；取字节命令留给 T-10 在迁移前执行。

## 1. 两层身份

`ExecutionKernel::prepare` 先校验配置与 prepared 声明（`execution_kernel_prepare.cpp:15-24`），再在
`c.ruleset` 存在时：

- engine 算法标记：`actual.engine.judgementSemanticRevision += ";gameplay.fold.finite.v1"`
  （`:61`）；这是 engine 分量的算法版本标记。
- ruleset 实际配置：`interfaceProjectionToken = rulesetIdentityToken(*c.ruleset)`、`buildHash =
  compiledBuild`、`moduleOrder = 各 module id`（按声明顺序）（`:63-68`）。
- session 实际配置：`actual.session.loadoutToken = loadoutId`（`:69`）。

存在 grade table 时：

- `actual.engine.factSemanticRevision += ";grade.signed-interval.v1"`（`:77`）。算法标识在 Spec 正文写
  `signed-interval/1`，但实际拼接的后缀是 `;grade.signed-interval.v1`；两者不是同一个字符串。

**不能把这两层压成一条**：`gameplay.fold.finite.v1` 是 engine 算法标记，进入
`judgementSemanticRevision`；Ruleset/session 的配置身份由其 token/build/module order/loadout 承担。
Spec §S7A-5/6 首用补充的“实际配置进入 ruleset/session identity”不否认 engine 标记。

## 2. preimage 写入次序（`gameplay_assembler_assemble.cpp`）

`preparedFromProjection` / `makePreparedIdentity` / `makeRuntimePreparedIdentity` 共用下列次序：

| 序 | 分量 | 字段 |
| --- | --- | --- |
| 1 | 头 | `cuexis.judgement.identity.prepared.v1` |
| 2 | `engine` | `judgementSemanticRevision`、`factSemanticRevision`、`fixedPointTableId`、`coordinationPhaseOrderToken`；若存在 execution profile/late token，再写 `execution.engine.v1` + 两个 bool/token |
| 3 | `ruleset` | `interfaceProjectionToken`、module order（count + 逐项）、`buildHash` |
| 4 | `chart` | chart projection（不含 header） |
| 5 | `session` | `loadoutToken`、`defaultGraceSourceToken`、`normalizationProfileToken`、`judgementConfigToken` |
| 6 | execution session | 运行身份追加 `cuexis.judgement.identity.session.execution.v1`、mapping profile/version/sourceClass、排序后的 domain 表、late 参数、calibration |

对应 `gameplay_assembler_assemble.cpp:10-38`（engine/ruleset 前缀）、`:41-49`（session）、`:51-83`
（execution session）、`:85-112`（projection 与 prepared 组装）。

## 3. 旧 fixture 与 token

两组现有 fixture：

- `tests/judgement/execution_test_fixture.hpp:15-56` 的 `executionFields`：engine 注册值
  `judgement.t4-k4.v1` / `fact.semantic.phase-local.v1` / `fixed-point.none.v1` /
  `coordination.six-eight.t4-k4.v1`，加 `executionProfileToken=gameplay.execution.t4-k4.v1`、
  `lateAlgorithmToken=late.window.logical.v1`（`:21-24`）。
- `tests/judgement/fold_recovery_fixture.hpp:51-102` 的 `FoldFixture`：`mapping/v1`、keyboard、
  `domain.binding.one`；ruleset `rules` 由 `declaration(hooks)` 生成；`configuration()` 传入
  `prepareRuleset` 结果与 `executionProfile`/late/fact token；`prepared()` 由
  `prepareResolvedGameplay` 生成。
- 既有期望：`tests/judgement/fold_recovery_seek_tests.cpp:341-383` 对 graded=false/true 分别构造
  `actual` 后缀并调用 `makeRuntimePreparedIdentity`，再与
  `query()->kernelView().judgementIdentity` 比较；同时反向检查不追加后缀的身份不同（`:379-382`）。

这组期望是**镜像实现**：`makeRuntimePreparedIdentity` 与生产 kernel 调的是同一 writer。它证明准备路径
与 writer 一致，不能独立证明“改字段后旧身份不变”。因此 T-10 必须保存旧字节，而不是只重跑该断言。

## 4. T-10 迁移前应取的证据（本轮未执行）

在旧代码上构建 `cuexis_judgement_tests` 后，对 graded=false 与 graded=true 各取：

```text
query()->kernelView().judgementIdentity.canonicalBytes().toHex()
query()->kernelView().judgementIdentity.canonicalBytes().bytes()  // 原始 bytes
```

同时记录：

- `engine.*` 实际值（含两个 `;` 后缀是否出现）；
- `ruleset.interfaceProjectionToken`、`buildHash`、`moduleOrder`；
- `session.loadoutToken`、`defaultGraceSourceToken`、`normalizationProfileToken`、
  `judgementConfigToken`；
- 旧回放结果（`JudgementSession::evaluateReplay` 的 projection 与 `sameKernelResult`）。

把上述字节与旧回放结果作为改动前的对照；新实现必须逐字段解释差异，不能以新实现生成的期望证明身份未
变。差异无法解释时按 D-04 冻结该条并交 owner 裁定。

## 5. 本卡边界

- 未搬移身份字段，未替换 `;gameplay.fold.finite.v1` 或 `;grade.signed-interval.v1` 后缀。
- 未改 `IdentityFactory`、`PreparedIdentity`、`SessionConfiguration` 或任何 wire。
- 文档已写实际映射（`GAMEPLAY_V2_SPEC` J0-5、`gameplay-v2-execution-profile` J0、
  `gameplay-v2-execution-types` §2）；实现与字节回归归 T-10。
