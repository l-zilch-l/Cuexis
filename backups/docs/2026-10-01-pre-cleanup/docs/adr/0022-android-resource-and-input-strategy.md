# ADR 0022：Android 资源与输入延迟策略

日期：2026-07-17

状态：已接受

## 背景

Android 需要受控包体、GPU 格式、内存预算、生命周期和事件时间戳，桌面源资产不能原样复制。

## 决策

首个移动目标为 OpenGL ES 3.0 Android。纹理使用 KTX2/Basis Universal，Mesh 使用 meshoptimizer，音乐使用 Ogg Vorbis/libvorbis，Shader 使用 GLSL ES 300 派生变体。

DeviceProfile 定义资源和瞬时预算。输入保留单调事件时间戳并映射到 Timeline，输出、输入和用户校准延迟分开记录。后台恢复和设备重建产生 time discontinuity。

## 备选方案

直接分发 PNG/WAV/源 Mesh 会增加包体和运行时成本；按渲染帧时间判定输入会引入一帧以上抖动，因此不采用。

## 影响

Asset Importer 增加目标 Profile，ResourceManager 响应预算和内存压力，性能面板增加输入与音频指标。

## 后续风险

设备碎片化要求真实设备矩阵验证；具体预算必须按验收设备测量，不能只依赖模拟器。

## SDK 转型补充（2026-07-20）

阶段 9B 的 Android 目标调整为 SDK、ContentProvider、宿主生命周期和可选 OpenGL ES adapter 验证，不要求 Cuexis 提供完整移动端游戏外壳。原始输入由宿主/SDL adapter 转换为标准 InputEvent，`cuexis_judgement` 仍负责判定、计分、记录和回放。
