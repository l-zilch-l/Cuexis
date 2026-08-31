# 260829 Full Review 整改 W12/W13

状态：completed；2026-08-29 本地数学、core 辅助合同和 Runtime reload 整改完成

本报告记录 Full Review 第 1 批 W12/W13 中 Lane B 与 Lane D 的实现证据。原始审查报告
`260829-full-review.md`、`docs/CURRENT_STATUS.md` 和整改计划均未修改；本报告不改变 Stage 4/5
状态。

## 基线与范围

- 基线 SHA：`2fbcd2b8f776c2227d09442854fd1b78525cee53`
- 实现 SHA：`cbce029`（`fix: complete full review math and reload remediation`）
- 实现分支：`260829-full-review`
- 任务：`AG-B4-NORMALIZE`、`AG-B5-CORE-AUX`、`AG-B3-MATH-MIGRATE`、
  `AG-D1-RELOAD-T`、`AG-D2-RELOAD-I`
- Findings：CM-04、CM-05、CM-06、CM-09、CM-17、RT-01、RT-16

## 实现

- 删除 animation 内部重复数学 helper，统一使用 `core::hermiteProgress`、`core::lerp` 和
  shortest-path `core::slerp`；保留 behavior/animation 的既有错误包装、排序和采样语义。
- `core::normalize(Quat)` 在平方长度或归一化结果非有限时返回稳定的
  `core.math.quaternion_not_representable`，不再返回全零 quaternion。
- 明确 UUID validator 的 canonical lowercase 规则；新增 NIST SHA-256 known-answer、
  55/56/63/64/65 字节边界和百万字节流式测试；固定 inverse determinant 与 `nearlyEqual`
  的绝对容差合同。
- Runtime reload 在发布 candidate World/Scope/diagnostics 前完成 candidate frame/debug 采样；
  debug 采样失败保持旧 active 状态，成功后原子提交 candidate debug records。

## 验证

- Debug fresh configure、clean build：通过（MSVC x64 Developer environment）。
- Debug 全量 CTest：首次普通 PowerShell 运行有 7 个 external consumer 因未加载 VS 环境而找不到
  `kernel32.lib`；在同一 VS Developer 环境下重跑失败项 7/7 通过。其余 487 项首次通过，另有
  1 项 Windows symlink 条件测试跳过，含 architecture、core/behavior/animation/runtime、Playback、
  CXC、headless 和 performance gates。
- Release fresh configure、clean build、全量 CTest：495/495 通过；Windows symlink 条件测试按
  平台条件跳过。
- shared-debug fresh configure、clean build：通过；architecture、7 个 external consumers、
  shared export surface、shared consumer imports、shared Playback consumer imports 共 11/11 通过。
- `cmake -DCUEXIS_SOURCE_DIR="$PWD" -P cmake/VerifyArchitecture.cmake`：通过，包含源公共头
  ASCII、GLM 和 Playback header leak 检查；安装公共头 ASCII/Playback leak 由 find-package
  external consumer gate 覆盖。
- `cmake --build --preset debug --target cuexis_format_check`：通过。
- `python -B tools/check_docs.py`：通过（179 Markdown、20 candidate JSON/CXT）。
- `python -B tools/update_version.py --check`：通过（`26.08.01-1`）。
- `git diff --check`：通过。

## 保留合同与残余风险

FrameDigest v1-v3、PreparedSemanticIdentity、canonical bytes/order、golden、Playback 公共观察面、
默认 capability、安装边界、公共头 ASCII/GLM 约束、candidate/active rollback、owner-thread 合同
及 runtime-script 无限期延后边界均保持不变。未改变任何 identity、digest 或序列化字段。

M01 的决策门 D1-D6 仍没有独立 owner/spec acceptance；本批没有实施 identity 迁移或其他依赖
未关闭决策的产品选择。Lane A importer/cache-key/pipeline 任务继续阻塞；HostClock 跨线程同步、
Render/Audio 热路径和 Playback prepare/parse-once 任务仍按计划暂缓。Hosted Linux/MinGW 及 owner
acceptance 仍需在相应环境完成，本地未执行 push。
