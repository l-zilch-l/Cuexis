# 260829 Full Review 整改 W11

状态：部分完成；2026-08-29 本地 W11 数学 API 已验证，Lane A identity 保持阻塞

本报告记录 Full Review 第 1 批 W11 的两项任务交接。原始审查报告
`260829-full-review.md` 与 `CURRENT_STATUS.md` 均未修改；本报告不改变 Stage 4/5 状态。

## 基线与决策

- 波次：`W11`；实现槽位 `W2` 为 `AG-B2-MATH-API`。
- W2 基线 SHA：`9a19854e7cdee427a3ea41654a627b79f6e1b421`。
- W2 实现 SHA：`d481b114fd89366ed358069c90b5599cf5332776`。
- 原始 finding：CM-04、CM-17、RT-16；顺带覆盖 Lane B 的统一数学 API 前置。
- `AG-A1-IDENTITY-I` 未进入实现：D6 identity migration 没有独立 owner/spec acceptance，
  因此 v1-v3 identity 迁移保持 blocked。

## W2 实现

- 在 `core::math` 增加 Cuexis 自有类型的 `hermiteProgress`、`Vec3 lerp` 和 shortest-path
  `Quat slerp` API；实现只在 core 源文件使用 GLM，公共声明不泄漏 GLM。
- 保持 hemisphere 对齐、dot clamp、near-linear 分支和最终 normalize 的既有插值语义。
- 为避免新增 core API 经 ADL 与匿名命名空间 helper 冲突，重命名 animation sampler/mixer
  与 behavior 内部 helper；调用行为、错误包装、排序和 golden 不变。
- W11 同时保留 B1 大数 quaternion characterization；`AG-B4-NORMALIZE` 才负责把该预期
  RED 修复为稳定的非有限长度错误。

## 验证

- MSVC shared-debug 聚焦构建（core/behavior/animation/runtime targets）：通过。
- core API CTest：2/2 通过（Hermite、Quaternion slerp）。
- Behavior/Animation/Runtime 聚焦 CTest：6/6 通过；直接测试分别为 23、8789、975 assertions。
- B1 characterization：1 个预期失败（31 cases / 132 assertions，失败为
  `tests/core/math_tests.cpp:46` 的 `normalize` 大数溢出行为），未视为 W11 回归。
- `cuexis_architecture_tests`：通过。
- `cuexis_format_check`：通过（clang-format dry-run）。
- `python -B tools/check_docs.py`：通过（178 Markdown、20 candidate JSON/CXT）。
- `python -B tools/update_version.py --check`：通过（`26.08.01-1`）。
- `git diff --check`：通过。
- `cuexis_external_consumer_find_package_core`：通过。
- shared export/import 三项：`cuexis_shared_export_surface`、`cuexis_shared_consumer_imports`、
  `cuexis_shared_playback_consumer_imports` 均通过。
- 曾有一次并行 external gate 竞争共享 vcpkg/构建目录而失败，随后在 MSVC 环境中串行复跑
  通过；该环境失败不归因于产品代码。

## 保留合同与残余风险

FrameDigest v1-v3、canonical bytes/order、PreparedSemanticIdentity、默认 capability、
Playback 公共观察面、安装边界、公共头 GLM/ASCII 边界、candidate/active rollback、
owner-thread 合同和 runtime-script 无限期延后边界均保持不变。未改变任何 identity、digest、
golden 或格式字段。

`AG-A1-IDENTITY-I` 仍需 D6 owner/spec acceptance 后重新派发；`AG-B3-MATH-MIGRATE`、
`AG-B4-NORMALIZE` 和 `AG-B5-CORE-AUX` 尚未开始。W11 不关闭 CM-04，直到 B4 修复并通过完整
Lane B 门禁。
