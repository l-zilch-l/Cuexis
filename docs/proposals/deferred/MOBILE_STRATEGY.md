# Cuexis Android 与移动端策略

状态：deferred proposal；Stage 9B SDK 与宿主适配方向已接受，尚未进入实施

更新日期：2026-08-10

阅读入口：[Deferred Design Proposals](README.md)。当前排期以
[ROADMAP.md](../../ROADMAP.md) 和 [Stage 9B plan](../../stage_plans/stage_9b_implementation_plan.md)
为准。

## 首个目标

首个移动目标为 Android。阶段 9B 验证 Android Playback SDK 构建、ContentProvider、宿主生命周期和可选 OpenGL ES 3.0 内建 adapter，不要求 Cuexis 自己提供完整移动端游戏外壳或商店发布流程。

## 目标资源 Profile

源资产不直接进入发布包。Asset Importer 按目标 Profile 生成派生资源：

```text
Texture  -> KTX2 + Basis Universal；运行时转码 ASTC/ETC2，桌面可转 BC
Mesh     -> meshoptimizer 优化后的顶点/索引和 LOD 数据
Music    -> Ogg Vorbis；libvorbis 解码为 PCM，AudioClock 仍按采样帧工作
Shader   -> SHADER_PIPELINE.md 定义的 GLSL ES 300 变体
```

优先使用 KTX-Software、Basis Universal、meshoptimizer、libogg 和 libvorbis 等成熟开源实现，并按依赖政策审查许可证。

每个派生资源记录源 hash、Importer 版本、target profile 和压缩参数。设备不支持首选纹理格式时使用同 KTX2 内容的受支持转码目标，不回退到运行时读取源 PNG。

## 内存与加载

定义可配置 DeviceProfile，而不是把预算写死在业务代码：

```text
CPU resource budget
GPU texture/buffer budget
audio decode/ring buffer budget
particle checkpoint budget
maximum transient upload budget
```

ResourceManager 按 Scope、Lease、LRU 和资源优先级执行回收。超过硬预算且无法释放 Required 资源时，加载事务失败并显示诊断，不能由系统随机杀死资源。

## 分辨率与安全区域

World 使用逻辑相机和 viewport，不按物理像素改变游戏空间。UI 使用 DPI scale 和 safe area inset。旋转、窗口大小和系统栏变化只能更新 viewport/UI，不重建 ChartRuntime。

## 输入时间戳

宿主 adapter 或可选 SDL 平台层把原始事件时间戳规范化为单调 `eventTimeNs`，同时记录 arrival time 和 sequence：

```text
device/source
eventTimeNs
arrivalTimeNs
sequence
payload
```

`cuexis_judgement` 使用事件发生时间映射到 Timeline，不使用处理该事件的渲染帧时间。宿主 adapter 维护单调时钟与 HostClock/AudioClock/Timeline 的相关性；时钟重建产生 discontinuity。宿主只负责采集和规范化输入，判定与计分仍使用 SDK 统一模型。startRecording() 按 chartTimeMs 记录 InputEvent，stopRecording() 返回版本化 ReplayData，loadReplay() 注入预记录事件。

输出音频延迟、触摸输入延迟和用户主观校准分别记录，不能合并成一个无法诊断的常数。校准值按设备/profile 保存。

## Android 生命周期

```text
后台/失焦：宿主暂停 Timeline；使用 CuexisAudio 时同时暂停 AudioTransport
恢复：重建 HostClock 或 AudioClock correlation 并产生 discontinuity
图形 Context 丢失：宿主 adapter 或内建 RenderBackend 从 Handle + CPU/缓存资源重建 GPU 对象
内存压力：请求 ResourceManager 释放无 Lease 缓存
```

生命周期事件不得直接修改 ChartDocument。无法恢复 Required 资源时 Session 进入可诊断 Failed 状态。

## 性能验证

Android 验证阶段记录 PlaybackSession update/extract、FrameSnapshot 交换、CPU/GPU frame time、draw calls、Entity、粒子、资源内存、音频 underrun、输入到判定时间和 Shader variant。验收设备与预算在实现阶段用 DeviceProfile 固定并记录，并区分宿主能力与 Cuexis 内建 adapter 能力。
