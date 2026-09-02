# Stage 6 Implementation Plan: Playback C++ API and Player Productization

状态：future；未开始

更新日期：2026-09-02

归档来源：[旧版 PROJECT_GUIDE](../../../archive/PROJECT_GUIDE_LEGACY_2026-08-10.md) 与
[SDK transition plan 快照](../../../archive/CUEXIS_SDK_TRANSITION_PLAN_2026-08-10.md)。2026-09-01
[阶段核验记录](../../../stage_reports/reviews/stage-verification-2026-09/2026-09-01-findings.md)
中的三个 open 问题已纳入本计划；在本阶段启动、实现、验证和处置证据完成前，它们保持 open。

本阶段排在 [chart-format-update-for-v5](../../active/chart-format-update-for-v5/plan.md) 完成之后，以已接受的
Chart v5 typed/portable 谱面作为 Playback 和 Player 的新增能力基线。Chart v4 仅作为只读兼容和显式迁移
输入保留，不再作为 Stage 6 新功能的作者层基线。

## 1. 阶段目标

在 Stage 1E 的外部消费和 Stage 3 的 portable presentation 基础上，稳定 Playback C++ 使用、
弃用和升级政策，并把独立 Cuexis Player 产品化。本阶段继续是 matching-toolchain C++ 边界，
不在 Judgement/Replay 完成前冻结稳定 C ABI。

Stage 6 同时关闭阶段核验发现的三个工程缺口：恢复可追溯的仓库版本门禁；建立 Player 与具体
OpenGL adapter 之间的后端中立表现渲染边界；为常见图片和音频建立可验证的导入或解码路径。
Chart v1-v3 退出仍是待项目所有者决策的 candidate 提案，不因本阶段启动而进入实施范围。

## 2. 工作流和批次

### S6-A：合同、基线和依赖决策

- 以已接受的 Chart v5 合同、当前 `master`、SDK API `0.7.0`、static/shared consumer 和 Player 行为建立
  Stage 6 基线；v5 的 Schema、typed Reader/Writer、迁移报告和 deterministic golden 必须先通过交接门禁。
- Stage 6 的 Playback 和 Player 新增路径只消费 v5 canonical/typed/portable 数据；v4 只通过明确的只读兼容
  或迁移入口进入，不在 Stage 6 中重新定义 v4 作者语义。
- 为 Playback C++ 弃用/升级政策、Player 配置、表现渲染抽象和常用媒体支持冻结明确合同；会改变
  已接受架构或公共 API 的部分先形成 ADR，不能只由实现细节决定。
- 对媒体支持记录格式范围、解码器选型、许可证、预算、安全限制和落点（离线导入/打包或运行时）；
  默认优先保持 Playback 只消费 canonical typed/portable data，运行时直解码必须有单独证据。
- 盘点渲染状态中真正后端中立的值、窗口表面与渲染器所有权、设备丢失/重建、present 失败和
  Player 生命周期，避免把 OpenGL 名称或 SDL 类型提升到公共 Playback API。

### S6-B：仓库版本和发行门禁

- 更新 [VERSIONING.md](../../../guides/VERSIONING.md)，明确每次合并至 `master` 的发行门禁都必须
  通过 `python -B tools/update_version.py yy.mm.dd-v` 递增显示版本：UTC 日期变化时 build 归 1，
  同一 UTC 日期再次发布时 build 加 1。
- 增加机器可执行的门禁或等价的受保护检查，验证候选相对上一已发布/合并基线的规范版本确已
  递增；`cmake/CuexisVersion.cmake` 与 `vcpkg.json` 一致但版本未前进时也必须失败。
- 把版本更新列为 release/merge checklist 的显式项目，并验证生成头、窗口标题、启动日志和安装包
  仍报告同一显示版本。该工作不隐式改变 `CUEXIS_SDK_API_VERSION`、内容格式或未来 C ABI。
- 使用更新工具纠正当前 `26.08.01-1` 相对近期合并历史落后的状态，并记录候选版本与门禁证据。

### S6-C：Playback SDK 和 Player 产品化

- 稳定 PlaybackSession C++ 所有权、线程、Result、兼容性、弃用和升级政策。
- 继续验证 static/shared package、真实宿主、升级路径、符号和部署。
- 建立至少一个真实宿主适配证明，但不把宿主 SDK 引入 Playback 核心。
- 完善安装包、许可证、Debug/Release 和升级文档。
- 完善主循环、播放生命周期和正式的加载、播放、暂停、停止、Seek 与 Reload 入口。
- 组合 ProjectConfig、UserPreferences、LaunchOptions 和 PreflightCapabilities。
- 在子系统创建前生成不可变 ResolvedAppConfig，并为 Session 派生最小 ResolvedSessionConfig。
- 实现用户配置目录、原子写入、版本迁移、损坏文件安全回退和配置来源日志。
- 定义最小版本化 AudioDeviceProfile；UserPreferences 只引用 profile ID。
- 子系统创建后收集 EffectiveSettings，并明确记录请求值、有效值和受控回退。
- 动态设置只通过显式 apply 生效；静态设置只通过明确重建 Session/backend 生效。
- Player 只通过 PlaybackSession 访问 Timeline、Renderer、材质/Shader 和调试信息。

### S6-D：后端中立表现渲染边界

- 在 `cuexis_render` 建立表现帧渲染接口（暂名 `IPresentationRenderer`），使 Player 面向
  `FrameSnapshot`/portable presentation 调用；OpenGL adapter 实现该接口，Player 不再直接调用
  `OpenGlBackend::renderPresentationFrame`。
- 定义后端中立的创建、重建、提交、present、失败和销毁合同，分离窗口表面生命周期与渲染器
  生命周期；SDL/OpenGL/GLAD 类型继续留在 platform 或 adapter 私有边界。
- 把 `OpenGlDrawCommand`、`OpenGlDrawSummary` 等确属可移植的 draw 状态下沉到
  `cuexis_render`；像素探针或具体 API handle 等后端专属状态保留在 adapter。
- 收敛 legacy `RenderScene`/`DebugLine` 与 portable presentation 的平行路径，明确诊断绘制如何
  组合到同一帧合同；不得为未来 Vulkan 建立第三条 Player 渲染路径。
- 以 OpenGL adapter、headless/validation consumer 和可替换的测试 renderer 证明接口完整性；
  Stage 10 的 Vulkan adapter 仍保持 deferred，本工作不宣称 Vulkan 已实现。

### S6-E：常用图片和音频支持

- 最低图片范围覆盖 JPEG 和 PNG，能够通过已冻结的导入/打包或运行时边界生成 Player 可消费的
  `CXPRES01` RGBA8 texture；色彩空间、alpha、方向、尺寸/像素预算和损坏输入诊断必须确定。
- 最低音频范围覆盖 MP3、Ogg Vorbis 和 FLAC，能够通过已冻结边界生成现有音频路径可消费的
  WAV/PCM 或等价 typed PCM；采样率、声道、时长/解压预算和损坏输入诊断必须确定。
- 保留原始资源与生成资源的 identity、缓存失效和 CXC closure 可追溯性；同一输入和 profile 的
  canonical 输出必须可复现。
- 解码失败、资源超预算或格式不支持时必须稳定失败，不得提交部分输出或替换上一有效
  Playback/包。
- 若引入第三方库，同步更新 `vcpkg.json`、必要时的 `vcpkg-configuration.json`、
  [DEPENDENCY_POLICY.md](../../../guides/DEPENDENCY_POLICY.md) 和
  [THIRD_PARTY_NOTICES.md](../../../../THIRD_PARTY_NOTICES.md)，并验证 static/shared package 不泄露
  私有解码器类型或意外增加 Playback 传递依赖。

### S6-F：关闭、hosted 验证和交接

- 对 Stage 6 的公共 C++、Player、配置、渲染和媒体路径完成 focused、architecture、package、
  external consumer、失败回滚、文档与许可证检查。
- 在同一最终候选 SHA 上取得 Linux Quality、Windows MSVC 和 Windows MinGW 要求的 hosted
  证据；Player/渲染变更另取得所需 GPU smoke 证据。
- 在阶段报告中逐项引用版本门禁、渲染抽象和媒体支持的实现与测试证据，然后回填
  [2026-09-01 阶段核验记录](../../../stage_reports/reviews/stage-verification-2026-09/2026-09-01-findings.md)
  的处置记录并将对应状态改为 closed。没有证据时不得仅因写入计划而关闭 finding。
- 完成当前状态、路线图、API/构建/依赖文档和下一阶段交接；由项目所有者明确接受 Stage 6。

## 3. 验收标准

- Player 可从 ProjectConfig 启动，只加载 canonical Chart v5，并支持播放、暂停、Seek 和 Reload；v4 输入仅
  通过明确的只读兼容或迁移路径处理。
- Player、Studio Preview 和宿主使用唯一 PlaybackSession 到内部 RuntimeSession 路径。
- 加载失败、资源降级、音频 discontinuity 和 Shader 错误具有稳定诊断和回滚。
- 损坏或未来版本的用户设置不修改项目文件，并回退到单一来源的安全默认值。
- 显式选择的 AudioDeviceProfile 缺失、损坏或设备不匹配时初始化失败，不静默切换设备。
- 相同来源生成的 ResolvedSessionConfig 具有可复现的规范化 identity。
- 活动 Session 不受随后 UserPreferences 文件修改影响。
- UserPreferences 不得修改 Chart、Behavior、Animation 或 World 领域数据。
- external consumer 只使用安装公共头，static/shared、Debug/Release 和支持平台门禁通过。
- 包和文档明确本阶段不承诺稳定 C ABI。
- `master` 发行门禁能够拒绝未递增的显示版本；当前滞后版本已纠正，版本源、生成头、Player
  和安装包报告一致。
- Player 的正式表现路径只依赖后端中立渲染合同；OpenGL adapter 不向 Playback 或 Player 泄露
  OpenGL/GLAD 类型，测试 renderer 能消费同一帧合同。
- JPEG/PNG 与 MP3/Ogg Vorbis/FLAC 的受支持路径、预算、canonical 输出、错误诊断和 deterministic
  golden 均有测试；损坏和超预算媒体不会产生部分有效产物。
- 阶段核验记录中的问题 1、2、3 均有处置证据并已关闭；candidate 的 Chart v1-v3 退出提案保持
  独立状态，除非另有项目所有者决策和 ADR。

## 4. 明确不包含

- Studio 编辑器实现。
- InputProfile、CalibrationProfile、Judgement/Replay 或稳定 C ABI。
- Player 私有 Runtime 路径或宿主专用依赖进入 Playback 核心。
- Vulkan adapter 实现、Vulkan API 类型或为未来 Vulkan 建立未验证的公共占位接口。
- 未经合同和预算门禁的任意媒体格式探测、无限制解压或第三方类型进入公共 Playback API。
- Chart v1-v3 Reader/Writer/迁移的退出；该项仍按 ADR 0041 和独立 owner 决策处理。
