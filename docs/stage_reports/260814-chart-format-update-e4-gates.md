# Stage Chart Format Update CFU-E4 Playback Gates

状态：completed

快照日期：2026-08-14

实施基线：`stage-ChartFormatUpdate` worktree on commit
`1af1606faf301ac5f534ff5ce6bd98403046ebb6` plus the uncommitted CFU-E3/E4 surface
closed by this report

权威计划：[Stage Chart Format Update 实施计划](../stage_plans/stage_chart_format_update_implementation_plan.md)

Identity 基线：[CFU-E3 报告](260814-chart-format-update-e3-identity.md)

## 1. 结论

CFU-E4 已关闭。计划 5.7 的本地 E 批次门禁已取得：旧 v1/v2/v3 与 typed/filesystem
source 回归、empty static v4 跨 filesystem/memory/host/CXC 成功且 identity/帧稳定、非空
动画在 World 发布前拒绝并保持 active identity、参数化 static v4 的默认/override/reload
identity 与 FrameDigest v3 稳定、static/shared consumer 观察 `semanticIdentity`，shared
export surface 只增加 E0 已批准的公共 symbol。

该关闭是本地 MSVC Debug/Release/shared 证据，不是 hosted Linux/Windows/MinGW CI、
CFU-D3 运行时等价、完整 CXC 产品支持、公共 package API、完整 v4 动画 Playback 或
Stage 4。CFU-F/CFU-G 仍拥有跨平台与最终产品关闭。

## 2. E4 覆盖面

empty static v4 CXC golden 现为
`tests/fixtures/chart_format_update/golden/cxc_v1_v4_static.cxc`，与既有参数化
`cxc_v1_v4.cxc` 分离。Playback identity 测试覆盖：

```text
chart-text / typed / memory / filesystem / CXC file/memory 同一 static identity
default vs override parameter identity 不同
约分 rational 得到同一 identity
失败动画 reload 保持 active identity 与抽出帧
成功 options reload 改变 identity 与 FrameDigest v3
v1/v2/v3 成功 prepare 都有 identity
```

Playback-only consumer 在 candidate/commit 后观察 `semanticIdentity()`，并断言失败
动画 CXC reload 不改 active identity。`cuexis_shared_export_surface` 要求导出
`semanticIdentity`，继续拒绝 RuntimeSession、AssetDatabase、World、EnTT 和 Playback
detail symbol。安装公共头扫描仍禁止 Chart/CXC/JSON/archive 类型。

## 3. Local verification

工具链：Visual Studio 2026 Community 18 / MSVC 19.51.36248.0，CMake 4.3.3，Windows
x64。

```text
Debug full build                                             passed
ctest --preset debug --no-tests=error                        362/362 passed
Release fresh configure + clean-first full build             passed
ctest --preset release --no-tests=error                      362/362 passed
shared-debug clean-first full rebuild                        passed
shared-debug package/export/import focused gates             10/10 passed
cuexis_format_check                                          passed
tools/check_docs.py                                          123 Markdown / 20 JSON-CXT passed
tools/update_version.py --check                              passed (26.08.01-1)
git diff --check                                             passed
```

Debug/Release full suites include architecture、CXC tools、migration、Playback、Player
diagnostics 和七个 external consumer tests。shared focused gates include 全部七个
external consumers、`cuexis_shared_export_surface` 以及 generic/Playback-only consumer
import-table checks。

本报告没有 hosted Linux/Windows/MinGW、sanitizer 或 coverage 结果；那些数字属于
CFU-F/CFU-G。

## 4. Handoff

CFU-E 本地批次已关。下一批次是 CFU-D3：证明 lift 后的 v4 与旧 v1/v2/v3 在
FrameSnapshot / FrameDigest v3 / seek-stop 上运行时等价。CFU-F 继续拥有 hosted
consumer、跨平台 golden 与 sanitizer；CFU-G 拥有最终产品关闭。不得从 E4 本地门禁
推断动画采样、公共 CXC package API 或 Stage 4 已开始。