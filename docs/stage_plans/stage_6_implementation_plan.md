# Stage 6 Implementation Plan: Playback C++ API and Player Productization

状态：future；未开始

更新日期：2026-08-10

归档来源：[旧版 PROJECT_GUIDE](../archive/PROJECT_GUIDE_LEGACY_2026-08-10.md) 与
[SDK transition plan 快照](../archive/CUEXIS_SDK_TRANSITION_PLAN_2026-08-10.md)。

## 1. 阶段目标

在 Stage 1E 的外部消费和 Stage 3 的 portable presentation 基础上，稳定 Playback C++ 使用、
弃用和升级政策，并把独立 Cuexis Player 产品化。本阶段继续是 matching-toolchain C++ 边界，
不在 Judgement/Replay 完成前冻结稳定 C ABI。

## 2. Playback SDK 工作

- 稳定 PlaybackSession C++ 所有权、线程、Result、兼容性和弃用政策。
- 继续验证 static/shared package、真实宿主、升级路径、符号和部署。
- 建立至少一个真实宿主适配证明，但不把宿主 SDK 引入 Playback 核心。
- 完善安装包、许可证、Debug/Release 和升级文档。

## 3. Player 产品化工作

- 完善主循环、播放生命周期和正式的加载、播放、暂停、停止、Seek 与 Reload 入口。
- 组合 ProjectConfig、UserPreferences、LaunchOptions 和 PreflightCapabilities。
- 在子系统创建前生成不可变 ResolvedAppConfig，并为 Session 派生最小 ResolvedSessionConfig。
- 实现用户配置目录、原子写入、版本迁移、损坏文件安全回退和配置来源日志。
- 定义最小版本化 AudioDeviceProfile；UserPreferences 只引用 profile ID。
- 子系统创建后收集 EffectiveSettings，并明确记录请求值、有效值和受控回退。
- 动态设置只通过显式 apply 生效；静态设置只通过明确重建 Session/backend 生效。
- Player 只通过 PlaybackSession 访问 Timeline、Renderer、材质/Shader 和调试信息。

## 4. 验收标准

- Player 可从 ProjectConfig 启动，只加载 canonical Chart，并支持播放、暂停、Seek 和 Reload。
- Player、Studio Preview 和宿主使用唯一 PlaybackSession 到内部 RuntimeSession 路径。
- 加载失败、资源降级、音频 discontinuity 和 Shader 错误具有稳定诊断和回滚。
- 损坏或未来版本的用户设置不修改项目文件，并回退到单一来源的安全默认值。
- 显式选择的 AudioDeviceProfile 缺失、损坏或设备不匹配时初始化失败，不静默切换设备。
- 相同来源生成的 ResolvedSessionConfig 具有可复现的规范化 identity。
- 活动 Session 不受随后 UserPreferences 文件修改影响。
- UserPreferences 不得修改 Chart、Behavior、Animation 或 World 领域数据。
- external consumer 只使用安装公共头，static/shared、Debug/Release 和支持平台门禁通过。
- 包和文档明确本阶段不承诺稳定 C ABI。

## 5. 明确不包含

- Studio 编辑器实现。
- InputProfile、CalibrationProfile、Judgement/Replay 或稳定 C ABI。
- Player 私有 Runtime 路径或宿主专用依赖进入 Playback 核心。
