# 260829 Full Review 整改 shared ABI gate

状态：completed；2026-08-30 本地实现与门禁完成

本报告记录 A2 完成后发现并修复的 MSVC shared-DLL 导出门禁问题。原始审查报告
`260829-full-review.md` 与 `docs/CURRENT_STATUS.md` 未修改；本报告不改变 Stage 4/5 状态。

## 基线与范围

- 基线 SHA：`2235fef9fe54a143a378adb9a2bc5b1a73a22ed2`
- 实现 SHA：`0ebb07f798d0fc0f121ba664715d9ecc3489a675`
- 实现分支：`260829-full-review`
- 任务：shared Release ABI gate correction
- 触发：MSVC `/WX` 下导出 `HostClock` 类直接包含 `std::atomic` 成员，产生 C4251

## 实现与合同

- `HostClock` 保持非导出类布局；仅 `submit()` 和 `snapshot()` 两个跨 DLL 方法带
  `CUEXIS_AUDIO_API`。
- seqlock、owner-thread 约束、并发 snapshot 语义、错误码和返回类型均未改变。
- Playback 公共观察面、FrameDigest v1-v3、canonical bytes/order、identity、capability
  默认值、candidate/active rollback 和 Playback-only consumer 边界均保持不变。

## 修改文件

- `engine/audio/include/cuexis/audio/audio_transport.hpp`

## 验证

- `shared-release`：VS 2026 x64 Developer Prompt 下 fresh configure/build 与完整 CTest
  `539/539` 通过；Windows symlink 和 PB-08 allocation 条件测试按平台/配置跳过。
- `shared-debug`：VS 2026 x64 Developer Prompt 下 fresh configure/build 与完整 CTest
  `539/539` 通过；同上条件测试跳过。
- `debug`：fresh configure/build 与完整 CTest `536/536` 通过；symlink 条件测试跳过。
- `release`：fresh configure/build 与完整 CTest `536/536` 通过；symlink 条件测试跳过。
- Shared external/package consumers、architecture、installed Playback leak 和 public-header
  ASCII gates 均在 Developer Prompt 中通过。
- `cuexis_format_check`、`python -B tools/check_docs.py`、
  `python -B tools/update_version.py --check` 和 `git diff --check` 通过。

普通 PowerShell 曾因未加载 MSVC `LIB` 环境导致 external consumer 找不到 `kernel32.lib`；
在同一 VS Developer Prompt 中串行重跑后全部通过。该环境问题不是代码回归。

## 残余风险

Linux、MinGW、MSVC 和 shader-tools hosted revalidation 仍需在最终实现 SHA 上执行；未执行
push，也未更新 `docs/CURRENT_STATUS.md`。Chart/CXC parse-once、RT-29、大规模 World/Animation
优化和大包解析优化仍按已接受策略保持 deferred。
