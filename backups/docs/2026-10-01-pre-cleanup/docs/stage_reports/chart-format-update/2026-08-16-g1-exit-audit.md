# Stage Chart Format Update CFU-G1 Exit Audit

状态：CFU-G1 complete；退出条件证据已逐项审计，未发现新的产品代码阻断

快照日期：2026-08-16

审计基线：`913639ca6049ce9c974a6d8fe210cd2d77ec4dd7`（`Close CFU-F after hosted
verification`）加当前 G0 状态校准 worktree。该基线不是 CFU-G 最终实现 SHA。

权威计划：[Stage Chart Format Update 实施计划](../../stage_plans/completed/chart-format-update/plan.md)
§11。

## 1. 结论

计划 §11 的 18 项退出条件逐项核对结果为：

```text
PASS       15
RERUN       1
DOC         2
BLOCKED     0
```

现有实现、fixture、测试和 CFU-C/D/E/F 报告没有暴露新的 C++、Schema、package 或工具功能缺口。
Stage Chart Format Update 仍不能关闭，因为最终 CFU-G 候选 SHA 尚未冻结和重跑 hosted workflow，
completion report 尚未创建并由项目所有者接受，Stage 4 typed handoff 尚未生成。

本报告是证据审计，不是新的 Debug/Release/CTest 或 hosted 执行结果。CFU-G 最终报告不得用本矩阵
替代最终候选 SHA 的实际日志和 run URL。

## 2. 分类规则

| 分类 | 含义 |
| --- | --- |
| PASS | 已有实现和可定位证据满足该合同；最终候选仍由全量回归防止退化 |
| RERUN | 功能证据存在，但必须绑定 CFU-G 最终候选 SHA 重新执行 |
| DOC | 实现基础存在，退出所需报告、交接或所有者记录尚未产生 |
| BLOCKED | 存在必须先修复的代码、合同或外部状态阻断 |

## 3. 退出条件矩阵

| # | §11 退出条件 | 状态 | 当前证据与结论 |
| ---: | --- | --- | --- |
| 1 | 格式身份、载体、版本、时间域、引用和扩展已有接受 ADR | PASS | [ADR 0038](../../adr/0038-cxc-v1-and-chart-v4-boundary.md) 已于 2026-08-11 接受；CXC、Chart v4、CXT 与 Animation Mixing 分离职责 |
| 2 | 规范、Schema、typed Reader、Writer/Canonicalizer 和 validator 一致 | PASS | [C1](2026-08-11-c1-reader.md) 与 [C2](2026-08-11-c2-lowering.md) 已关闭 Schema/Reader/Writer/lowering；生产 Schema 位于 `schemas/`，合同测试位于 `tests/json_support/` 与 `tests/chart/` |
| 3 | v1/v2/v3 历史读取与显式、非覆盖迁移 | PASS | [D1/D2](2026-08-13-d-migration.md)、[D3](2026-08-14-d3-equivalence.md) 和 [D close](2026-08-14-d-close.md) 保留旧 Reader、默认 CLI v3、显式 `--target 4`、原子输出与运行时等价 |
| 4 | 合法/非法、预算、缺资源、缺 capability 和未知扩展 fixture | PASS | `tests/fixtures/chart_format_update/` 提交 valid/invalid/binary/golden；`tests/chart/cfu_f4_limits_tests.cpp`、`tests/cxc/cfu_f4_safety_tests.cpp`、`tests/playback/playback_v4_prepare_tests.cpp` 覆盖精确上限、缺资源、capability 和扩展失败 |
| 5 | Playback prepare/activate/reload/seek 成功与失败 headless 证据 | PASS | [F1](2026-08-15-f1-headless.md)、[F2](2026-08-15-f2-package-consumers.md)、`tests/playback/playback_v4_prepare_tests.cpp` 与 `playback_migration_equivalence_tests.cpp` 覆盖事务成功、失败 reload、seek/stop 与 active-state 保持 |
| 6 | clean external consumer 只使用 public Playback headers | PASS | [F2](2026-08-15-f2-package-consumers.md)、`tests/external/playback_consumer.cpp` 和 `cmake/VerifyExternalConsumer.cmake` 覆盖 static/shared、add_subdirectory/find_package 与安装头隔离 |
| 7 | round-trip、FrameDigest/identity、数组和实体顺序确定性 | PASS | [F3](2026-08-15-f3-determinism.md) 与 `cmake/VerifyCfuF3Determinism.cmake` 固定 canonical CXC、migration、semantic identity、FrameDigest v3 和 diagnostics fingerprint |
| 8 | CXC metadata、entry order 和 package bytes 跨平台 golden | PASS | [F close](2026-08-16-f-close.md) 记录 GCC、Clang、MSVC、MinGW 六份 artifact 的 CXC 与 fingerprint 完全一致 |
| 9 | 空动画 v4 通过；非空动画稳定拒绝 | PASS | [E2](2026-08-14-e2-prepare.md) 与 F1/F2 consumer 证明静态/参数化 v4 成功，非空 Clip/CXT/Binding/Layer 以 `playback.capability.unsupported` 拒绝 |
| 10 | PlaybackSource/CXT/ParameterSet 所有权与 identity 边界 | PASS | [C3](2026-08-12-c3-cxc.md)、[E1](2026-08-14-e1-source.md)、[E3](2026-08-14-e3-identity.md) 与 architecture tests 证明 archive 类型不进入 Playback 公共头、CXT 不伪装为 AssetId、参数不进入 source identity |
| 11 | archive 许可证、静态/共享闭包、安装清单与退出路径 | PASS | `docs/DEPENDENCY_POLICY.md`、`THIRD_PARTY_NOTICES.md`、根 CMake 安装规则和 `VerifyExternalConsumer.cmake` 记录 minizip-ng 私有链接、许可证安装、shared 不泄漏与替换路径 |
| 12 | E0 API、SDK compatibility、symbol/export 和 consumer | PASS | [E close](2026-08-14-e-close.md) 关闭 API `0.6.0`、static/shared package、version rejection、export/import 与 Playback-only consumer |
| 13 | pack/validate/unpack 退出码与事务门禁 | PASS | [C4](2026-08-13-c4-tools.md) 和 `cmake/VerifyCxcTools.cmake` 覆盖 exit `0/1/2`、atomic output、no-overwrite、staging cleanup、restore failure 与 binary round-trip |
| 14 | 仓库外旧资产清单或“未提供外部资产”决策 | PASS | [D close](2026-08-14-d-close.md) 记录项目所有者于 2026-08-14 指示“未提供外部资产”，并保留全部兼容入口 |
| 15 | 最终实现 SHA hosted Linux/Windows 结果与失败记录 | RERUN | F 实现 SHA `8fcac15d...` 已有三套成功 workflow 和一次外部 TLS failure/rerun 记录；G0/G1 尚未形成最终候选 SHA，不能复用为 CFU-G 最终关闭证据 |
| 16 | completion report 已创建并由项目所有者接受 | DOC | `stage_chart_format_update_completion_report.md` 尚不存在；必须在最终本地/hosted 证据取得后创建并等待所有者接受 |
| 17 | Stage 4 动画字段、延期职责与风险已交接 | DOC | `AnimationProgramInput` 与格式字段已经实现并在规范中列明，但独立 Stage 4 handoff 尚不存在；不得把计划 §12 当作完成交接 |
| 18 | 权威状态与链接一致 | PASS | G0 已同步 `CURRENT_STATUS`、ROADMAP、PROJECT_GUIDE、格式/计划索引、Chart v4/CXT/CXC 状态；`tools/check_docs.py` 已增加当前 CFU 状态防回退门禁 |

## 4. 必须继续完成的门禁

### G2：Stage 4 typed handoff

创建独立 handoff，冻结 `AnimationProgramInput`、capability、fixture、预算、diagnostics、所有权、
未实现运行时职责和残余风险。AnimationSystem 不得在该批次实现，也不得读取 JSON/CXC/CXT。

### 最终候选验证

G0/G1 文档和门禁变更提交后冻结新的完整 SHA，执行 Debug/Release/shared 本地矩阵，并在同一 SHA
运行 Linux Quality、Windows MSVC 和 Windows MinGW。任何代码、Schema、fixture、CMake 或 workflow
修复都会使旧候选证据失效。

### Completion 与所有者接受

最终证据取得后创建 `stage_chart_format_update_completion_report.md`。只有项目所有者明确接受该报告
后，才能把格式阶段标为 completed，并把 Stage 4 从 blocked 改为 unblocked but not started。

## 5. 非阻断限制

- 本次没有 fresh CMake configure、build 或 CTest；G1 只审计仓库内实现和既有证据。
- 当前 worktree 包含未提交 G0/G1 文档与检查器变更，因此 `913639c` 不是最终候选 SHA。
- CXC 公共 CMake component/package API 不是 CFU-G 退出条件，也不会在本阶段交付。
- 非空 Chart v4 动画 Playback、AnimationSystem、PropertyResolver 和 OverrideToken 仍属于 Stage 4。
- runtime scripts、逐帧回调、表达式和 bytecode 继续无限期延后。

## 6. G1 结论

CFU-G1 退出审计完成，结果是 `15 PASS / 1 RERUN / 2 DOC / 0 BLOCKED`。下一执行检查点是 G2
Stage 4 typed handoff；格式阶段状态保持 active，不得宣称完整 CXC 产品支持、完整 v4 动画 Playback
或 Stage 4 已开始。
