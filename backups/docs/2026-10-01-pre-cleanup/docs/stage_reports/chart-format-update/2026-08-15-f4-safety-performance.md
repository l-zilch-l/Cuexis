# Stage Chart Format Update CFU-F4 Safety and Performance

状态：completed locally

快照日期：2026-08-15

后续关闭：CFU-F4 与整包 CFU-F 已于 2026-08-16 在最终实现 SHA 的 hosted Linux/MSVC/MinGW
全部成功后关闭，见 [CFU-F 关闭报告](2026-08-16-f-close.md)。本文件保留本地实施快照。

实施基线：`codex/cfu-f` on commit `819064556e7f6eb0fe32a075cbcf7b13e141d8a4`
plus the uncommitted CFU-F1/CFU-F2/CFU-F3/CFU-F4 surface closed by this report

权威计划：[Stage Chart Format Update 实施计划](../../stage_plans/completed/chart-format-update/plan.md)

## 1. 结论

CFU-F4 已完成本地实现与 MSVC 验证。新增门禁覆盖 CXC manifest、strict ZIP32 Stored package、
project Asset closure、Chart v4 CXT import 和 resolved animation aggregate budget 的精确上限与边界
+1；覆盖 `uint64` pseudo total、central/local offset 与 data range 溢出，并固定 diagnostics 截断签名。
非法上限测试使用小型 JSON 构造器和伪 header，不创建或提交数百 MiB 的无效 fixture。

Playback allocation test 在每个 empty Chart v1–v4 prepare 完成并热身后，验证 128 次 `update()` 加
复用 `FrameSnapshot` 的 `extractFrame()` 不新增分配。独立最大内容探针使用 64 MiB 合法 Texture，
记录 CXC writer/hash-load、CXC source、prepare/reload、热帧时间与进程 resident/peak 内存。该探针
默认跳过，只有显式设置 `CUEXIS_RUN_PERFORMANCE_PROBE=1` 才执行；结果是趋势证据，不设机器相关
pass/fail 阈值。

Linux Quality 已接入最终 SHA 绑定的 ASan/UBSan F4 日志、Chart/CXC/Playback clang-tidy、CXC 与
Chart v4 branch coverage 文本报告，以及 headless Release 最大内容性能 artifact。当前报告没有
hosted run URL 或最终实现 SHA 结果；因此 CFU-F1–F4 虽均已本地完成，CFU-F 整体仍未关闭。

该本地关闭不是完整 CXC、公共 package API、完整 v4 动画 Playback、CFU-G 或 Stage 4。

## 2. Safety and Allocation Gates

新增的 9 个默认执行 test case 覆盖：

- manifest listed-byte exact/+1 与 `uint64` 加法溢出；
- 固定三条 diagnostics 的截断顺序与 field-path signature；
- ZIP32 EOCD central range 和 local payload range 的 pseudo overflow；
- package byte/entry exact/+1 与 project Asset closure exact/+1；
- Chart v4 animation-template import exact/+1；
- resolved animation track、segment/step aggregate exact/+1；
- warmed empty Chart v1/v2/v3/v4 update/extract zero-allocation。

第 10 个 F4 CTest 是默认跳过的趋势探针。普通 Debug/Release 全量 CTest 不分配 64 MiB payload；
显式性能运行才生成该资源，并在进程结束后释放。

## 3. Maximum-Content Trend Evidence

本地 Debug 与 Release 都显式运行 64 MiB 最大合法资源探针。输入 content 为 `67,113,275` bytes，
生成 package 为 `67,115,719` bytes。以下结果来自同一 Windows x64 worktree；implementation SHA
显示基线 commit，因为 F1–F4 尚未提交。

| Metric | Debug | Release |
| --- | ---: | ---: |
| CXC write | 3.919 MiB/s | 46.276 MiB/s |
| CXC hash-load | 5.665 MiB/s | 70.007 MiB/s |
| prepare | 5,752,371.1 us | 343,313.0 us |
| reload prepare | 5,794,695.1 us | 359,093.4 us |
| warmed update + extract average | 53.777 us | 0.974 us |
| peak after prepare | 280,698,880 bytes | 275,382,272 bytes |
| peak after reload | 482,762,752 bytes | 477,032,448 bytes |
| reload peak delta | 201,863,168 bytes | 201,605,120 bytes |

完整证据分别保存在 `out/build/debug/cfu-f4/performance.txt` 与
`out/build/release/cfu-f4/performance.txt`。这些数值不能作为不同 CI runner 之间的硬回归阈值；
hosted artifact 用于保存最终 SHA 上的可追溯基线。

## 4. SHA-bound Hosted Evidence

Linux Quality workflow 的最终 SHA 证据包括：

- `headless-sanitize` 全量 CTest 后独立重跑 `^CFU-F4`，保存 ASan/UBSan 日志；
- `headless-clang-tidy` 构建 `cuexis_chart`、`cuexis_cxc` 与 `cuexis_playback`，保存完整输出；
- GCC coverage 单列 `engine/cxc/` 与 Chart v4 loader/resolver/lowering/identity 文件的 branch report；
- `headless-release` 显式启用最大内容探针并上传 `performance.txt`。

所有 artifact 名称包含 `${{ github.sha }}`，文件正文也记录 implementation SHA。Windows MSVC、
Windows MinGW 与 Linux matrix 的完整 CTest 会执行 F4 limits/allocation tests；F3 deterministic
artifact 继续承担 canonical bytes、migration、identity、digest 与 diagnostics 跨平台 parity。

## 5. Local Verification

工具链：Visual Studio 2026 Community 18 / MSVC 19.51.36248.0，CMake 4.3.3，Windows x64。

```text
Debug fresh configure + full build                              passed
ctest --preset debug --no-tests=error                           379/379 passed
Release fresh configure + clean-first full build with /WX      passed
ctest --preset release --no-tests=error                         379/379 passed
CFU-F4 focused tests                                            10/10 passed
Debug maximum-content performance probe                        passed (63.01 s)
Release maximum-content performance probe                      passed (5.17 s)
cuexis_format_check                                             passed
tools/check_docs.py                                             passed
tools/update_version.py --check                                 passed (26.08.01-1)
git diff --check                                                passed
```

Linux sanitizer、clang-tidy、coverage 与 Release performance workflow 只能由 hosted Linux 执行；
本地 MSVC 结果不能替代这些 pending 证据。

## 6. Handoff

CFU-F 不再有本地实现批次。下一步是在最终实现 SHA 上运行 Linux Quality、Windows MSVC 与
Windows MinGW，收集 sanitizer、clang-tidy、coverage、performance 与 F3 deterministic parity
artifact；任何 pending/failed job 都使 CFU-F 保持未关闭。通过后再由项目所有者决定是否关闭
CFU-F 并进入 CFU-G，不得据此越界实现 Stage 4 动画求值。
