# Cuexis API 参考

状态：active

更新日期：2026-08-30

适用版本：SDK API `0.7.0`

文档角色：公共 API 导航

## 先记住这五条

1. 宿主播放入口只有 `cuexis::playback::PlaybackSession`。
2. 需要检查候选内容时使用 `prepareLoad` / `prepareReload`，确认后再 `commit`。
3. 可恢复失败全部通过 `core::Result<T>` 或 diagnostics 返回，不能忽略。
4. `PlaybackSession` 和非空 `PreparedPlayback` 受 owner-thread 约束。
5. Runtime、World、EnTT、SDL、OpenGL、JSON DOM 和内部 CXC 类型不是宿主 API。

## 按任务选择文档

| 当前任务 | 入口 |
| --- | --- |
| 创建 Session、加载、重载、逐帧更新 | [PlaybackSession 生命周期](playback-session.md) |
| 选择输入来源、接入资源读取 | [PlaybackSource 与 ContentProvider](sources-and-content.md) |
| 读取帧、计算摘要、处理时钟 | [帧、摘要与时间线](frames-digests-and-timelines.md) |
| 检查表现资源、renderer capability | [表现资源与能力预检](presentation-and-capabilities.md) |
| 处理错误、diagnostics、identity 和兼容性 | [诊断、身份与兼容性](diagnostics-identity-and-compatibility.md) |
| 判断实现应落在哪个内部模块 | [内部模块速查](internal-module-catalog.md) |

## 公共边界

宿主只依赖安装后的 `cuexis/playback/*.hpp` 及其明确公开的 Core 值类型。公共 API 的产品边界以
[ADR 0027](../adr/0027-playback-sdk-product-boundary.md) 为准；格式字段与语义以
[格式索引](../formats/README.md) 为准。

本目录解释 API 的使用顺序、所有权和稳定行为，不复制完整头文件声明。公共头文件、SDK API 版本、
capability 或行为合同变化时，必须同步更新对应页面并运行 `python -B tools/check_docs.py`。
