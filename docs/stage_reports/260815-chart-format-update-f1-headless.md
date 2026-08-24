# Stage Chart Format Update CFU-F1 Headless Integration

状态：completed locally

快照日期：2026-08-15

实施基线：`codex/cfu-f` on commit `819064556e7f6eb0fe32a075cbcf7b13e141d8a4`
plus the uncommitted CFU-F1 surface closed by this report

权威计划：[Stage Chart Format Update 实施计划](../stage_plans/stage_chart_format_update_implementation_plan.md)

## 1. 结论

CFU-F1 已完成本地关闭。仓库现在拥有一个只链接 public `cuexis::playback` target、无需 GPU、
可在 adapter-disabled 构建中运行的 static Chart v4 CXC reference consumer。它证明同一 reference
project 通过 filesystem、CXC file 和 CXC memory source 得到相同 Prepared semantic identity 与
FrameDigest v3 trace，并覆盖 prepare、资源获取、commit、Stop、向后 seek 和失败 reload 原子性。

该关闭不是 external installed-package consumer、static/shared package 闭包、跨平台 CXC byte
parity、sanitizer/coverage、完整 CFU-F、公共 package API、完整 v4 动画 Playback 或 Stage 4。
CFU-F2 为下一批次。

## 2. Reference Project

`tests/fixtures/chart_format_update/cfu_f_reference_project` 是 CFU-F 专属 reference project。
Chart v4 保留 Behavior Event、Step Event 和 Stop 语义，`animationTemplateImports`、
`animationClips` 与对象 animation layers 均为空，因此只投影到既有 Runtime，不引入 Stage 4
动画求值。

生产 `cuexis_cxc_pack` 生成的
`tests/fixtures/chart_format_update/golden/cfu_f_v4_reference.cxc` 包含 7 个 entry、6905 bytes，
package SHA-256/identity 为：

```text
1cb2fcbf7a852a4db2ed9119359c68ff1cc4a06d1d41d18018fff89cf737d723
```

## 3. Headless Consumer

新增 `cuexis_cfu_f_headless_consumer` 与 CTest `cuexis_cfu_f_headless_reference`。target 直接依赖仅为
`cuexis::playback`，标签为 `cfu-f;cfu-f1;headless;playback;cxc`。正向路径覆盖：

```text
filesystem project / CXC file / CXC memory source
prepareLoad candidate identity and four-entry presentation manifest
acquire every declared presentation resource
commit and active semantic identity
0 ms, 625 ms, 250 ms backward seek, 1250 ms sampling
cross-source PreparedSemanticIdentity and FrameDigest v3 equality
```

固定 semantic identity 为：

```text
6d01494c126f3ae8fc9420259dc92873233022dec9dd6bf9caf04b217f100cc5
```

625 ms Stop sample 的固定 FrameDigest v3 value 为 `11596562486377158370`。

失败路径先加载 reference CXC，再执行两次失败 reload：

```text
nonempty animation CXC -> playback.capability.preflight_failed
NaN target frame       -> playback.session.reload_sample_failed
```

动画拒绝诊断顺序固定为：

```text
playback.capability.unsupported@$/animationClips#cuexis.animation.clip.v1
playback.capability.unsupported@$/objects#cuexis.animation.layers.v1
```

两种失败均逐字段验证 active semantic identity、完整 `PlaybackContentInfo`、active diagnostics 和
FrameDigest version/value 未改变。动画 fixture 只用于拒绝路径，不成为 Player 默认播放内容。

## 4. Local Verification

工具链：Visual Studio 2026 Community 18 / MSVC 19.51.36248.0，CMake 4.3.3，Windows x64。

```text
Debug fresh configure + clean-first full build              passed (200 build steps)
final Debug CFU-F1 target rebuild                            passed
ctest --preset debug --no-tests=error                        368/368 passed
Release fresh configure + clean-first full build             passed (200 build steps)
ctest --preset release --no-tests=error                      368/368 passed
headless-debug fresh configure + clean-first full build      passed (158 build steps)
ctest --preset headless-debug --no-tests=error               334/334 passed
cuexis_cxc_validate CFU-F1 reference package                 passed (7 entries, 6905 bytes)
cuexis_format_check                                          passed
tools/check_docs.py                                          127 Markdown / 20 JSON-CXT passed
tools/update_version.py --check                              passed (26.08.01-1)
git diff --check                                             passed
```

本报告没有 hosted Linux/Windows/MinGW、sanitizer、coverage、clang-tidy、static/shared installed
package consumer 或跨平台 package-byte 结果。这些仍属于 CFU-F2 至 CFU-F4 和 CFU-G。

## 5. Handoff

CFU-F2 下一步建立 static/shared、`add_subdirectory`/`find_package` 的 clean external consumers，
覆盖安装后的 Playback headers、CXC file/memory、typed documents、prepare options、semantic
identity、失败 reload 和内部 static package 链接闭包。F1 不改变 public SDK API `0.6.0`，不公开
CXC archive 类型，也不解析或执行 v4 动画。
