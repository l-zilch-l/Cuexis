# ADR 0030: Playback Preview API Version and Result Boundary

状态：已接受

日期：2026-07-27

## 背景

Cuexis 的项目显示版本使用 `yy.mm.dd.hh-v`，适合标识一次源码或发行构建，但 CMake
`project(VERSION)` 无法表达同一小时内的 `v`，也不能代表 consumer 所需的源码兼容性。
同时，现有 Playback C++ 公共接口返回 `cuexis::core::Result<T, E>`，该别名由
`tl::expected` 实现。若文档一面禁止所有第三方公共依赖、一面安装这些头和 package
dependency，consumer 无法判断真实契约。

## 决策

Playback Core preview 采用独立的 C++ SDK API 版本，从 `0.1.0` 开始：

```text
CUEXIS_VERSION_DISPLAY   构建身份，例如 26.07.18.18-1
CUEXIS_SDK_API_VERSION   C++ package/source API，例如 0.1.0
C ABI version            阶段 12 建立，当前不存在稳定版本
```

`CuexisConfigVersion.cmake` 使用 SDK API 版本和 `SameMinorVersion` 兼容规则。package config
将 `Cuexis_VERSION` 与 `Cuexis_API_VERSION` 设为 SDK API 版本，并通过
`Cuexis_VERSION_DISPLAY` 保留构建身份。日期或 build 序号变化不自动改变 SDK 兼容性。

阶段 1D 的 `SourceClockSample`、`RuntimeTimeline`、Prepared Playback、
`PlaybackContentInfo`/`MainMusicSourceView` 和 Audio package components 已扩展公开 preview
契约。因此当前 `CUEXIS_SDK_API_VERSION` 为 `0.2.0`，package version、生成头、外部 consumer
和兼容性测试均已同步更新；不得继续宣称 `0.1.x` 兼容。

preview C++ 公共签名允许使用 Cuexis 自有别名 `cuexis::core::Result<T, E>`，不得直接写出
`tl::expected`。这会形成已记录的 `tl-expected` 头文件源码依赖，但不构成长期 ABI 承诺。
其他第三方类型仍不得进入 Playback 公共签名。阶段 12 的稳定 C ABI 不得暴露
`tl::expected`、标准库所有权类型或跨模块异常。

公共 `FrameSnapshot` 是拥有型值对象。成功提取后，已有 Snapshot 不借用 Session、World、
Registry 或 RenderScene 的存储；后续 update、reload、unload 和 Session 销毁不得使它悬空。
内部借用视图只能用于单次受控调用，并在相应状态替换或销毁时失效。

## 影响

- 当前 external consumer 必须带最低 SDK API 版本调用 `find_package(Cuexis 0.2 ...)`。
- SDK API 兼容变化和日期构建发布需要分别评审、更新和测试。
- 安装包必须继续导出 `tl-expected` dependency，直到公共 C++ Result 表示发生显式迁移。
- Snapshot 生命周期测试必须保留 reload、unload 和 Session 销毁后的读取覆盖。
- 完整 Playback SDK v1 与稳定 C ABI 仍以阶段 11 和阶段 12 门禁为前提。

## 被拒绝的方案

### 使用日期版本作为 CMake package version

拒绝。它丢失同小时 build 序号，并会把每次构建身份变化误报成 SDK API 兼容性变化。

### 声称 preview 公共头没有第三方依赖

拒绝。`core::Result` 的实际定义要求 consumer 可见 `tl-expected`；隐瞒依赖会让安装包契约
与代码不一致。

### 在阶段 1E 冻结稳定 C ABI

拒绝。Judgement/Replay 和完整生命周期尚未经过外部宿主验证，冻结会固化不完整边界。
