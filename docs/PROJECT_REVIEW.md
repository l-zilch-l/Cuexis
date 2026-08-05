# Cuexis 项目文档审视报告（阶段 0 历史快照）

状态：历史审视；产品方向由 [ADR 0027](adr/0027-playback-sdk-product-boundary.md) 与 [SDK 转型方案](stage_plans/cuexis_sdk_transition_plan.md) 更新

审视日期：2026-07-18。SDK 转型复核日期：2026-07-20。2026-07-21 复核扩展至 ADR 0024-0028。本报告保留阶段 0 实现与原始设计的审计事实，不再作为现行产品定位或阶段 1C 之后路线的权威来源。表中关于长期保留 A/B 双格式或 Simple Importer 的评价已被 ADR 0035 取代；方案 B 已于阶段 2A.1 移除。

审视范围：`docs/PROJECT_GUIDE.md`、全部专项规范、`docs/adr/0001` 至 `0028`，以及阶段 0 的 CMake、vcpkg 和已激活 target 边界。

## SDK 转型复核

阶段 0 至 1B 的模块隔离、Chart/Runtime/World 边界、事务 Session、资源 Lease/Scope 和后端抽象可以直接作为 Playback SDK 内部基础保留。需要新增的关键边界是 PlaybackSession 公共门面、headless FrameSnapshot、ContentProvider、可选音频/渲染 adapter、组件化安装包，以及阶段 11 的必选 Judgement/Replay 交付。

原报告中“完整音乐游戏引擎”、Player/Studio 作为唯一组合者、阶段 6 仅 Player 产品化、阶段 9B 完整移动 Player 和阶段 11 仅最小模拟等方向性结论，均由 ADR 0027 和 SDK 转型方案替代。以下原始审计表只用于解释历史决策，不应用来反转新的 SDK 产品边界。

## 原总体结论

项目方向合理，阶段 0 已从工程骨架推进到可构建的最小实现闭环：Core、SDL Platform、World、Render 前端、OpenGL Backend、Player 与测试入口均已落地，并保持 Core/World 与图形后端隔离。（SDK 转型复核：ADR 0027 已将产品方向调整为 Playback SDK + 独立 Player + 独立 Studio，Judgement/Replay 为必选 SDK 交付）ChartDocument 与 Runtime 分离、Runtime 作为组合层、属性统一求值，以及规范格式与简易导入格式的关系仍是阶段 1 及后续实施必须遵守的已决策边界。

当前文档已经为阶段 0 至阶段 11 的已知公共边界提供推荐方案。阶段 0 的实现状态可由标准构建、CTest、格式检查和独立图形冒烟入口复核；本报告不替代特定环境中的实际验收记录。阶段 9B Android 验证与阶段 10 Vulkan 可行性验证明确延期；具体预算、性能阈值和异步 API 必须由对应阶段的测量数据决定。

评估标记：

```text
合理：边界清楚，可以按当前结论实施
基本合理：方向正确，实施前仍需补充专项规范
待决策：不能冻结公共接口
远期规划：当前只应保持隔离边界
```

## 分节评估

| 章节 | 评估 | 结论与注意事项 |
| --- | --- | --- |
| 0 文档状态 | 合理 | 已区分阶段 0 已实现内容、已决策边界与后续规划，避免把远期设计误认为现有功能。 |
| 1 项目概述 | 合理 | Entity 与数据驱动定位清楚，采用 Apache-2.0 并明确优先复用成熟开源库。 |
| 2 项目目标 | 合理 | 长期目标明确；首轮聚焦 Engine 与 Player，范围可控。 |
| 3 非目标 | 合理 | 能抑制编辑器、Vulkan、联网等方向过早扩张。 |
| 4 技术栈 | 合理 | 阶段 0 已实际接入 C++20、SDL3、EnTT、OpenGL、vcpkg 和 Catch2；SDL 音频与 ImGui 保持后续阶段规划。 |
| 5 版本规范 | 基本合理 | 单一 CMake 版本来源、生成头和 manifest 配置期一致性校验已经落地；人工编译号仍存在漏增风险。 |
| 6 工程结构 | 合理 | 阶段 0 只激活 Core、Platform、World、Render、OpenGL Backend、Player 和对应测试；未来模块目录未进入当前构建。 |
| 7 CMake 规范 | 合理 | target 化、依赖可见性、警告基线、格式 target 和 BUILDING.md 的 MSVC Developer Environment 说明已经落地。 |
| 8 CMake Presets | 基本合理 | Debug、Release、Build、Test preset 已形成标准入口；当前仅承诺 Windows/MSVC，未来跨平台时再增加继承层。 |
| 9 vcpkg | 合理 | 正式 manifest mode 与 baseline 已固定，阶段 0 依赖收敛为实际使用项；Catch2 后续可转为 tests feature。 |
| 10 模块依赖 | 基本合理 | 核心方向无环；Runtime 是高层聚合点，需要持续防止职责膨胀。 |
| 11 总体架构 | 合理 | RuntimeSession 和 ChartWorldInstantiator 解决了 Chart 与 EnTT 的所有权矛盾。 |
| 12 ECS | 合理 | Component 数据化、模块拥有 Component、层级显式失败策略均合理；Transform 缓存细节留到实现。 |
| 13 谱面系统 | 合理 | A/B 格式、统一 objects/components、有理数 Beat、v1 引用域和扩展边界已经形成最小规范。 |
| 14 行为系统 | 合理 | 绝对时间采样和可任意预览符合音游编辑需求；属性路径 Schema 尚需格式文档定义。 |
| 15 动画系统 | 合理 | Layer、BlendGroup 和 OverrideToken 消除了多 Clip 与 Gameplay 的隐式写入顺序。 |
| 16 粒子系统 | 合理 | 固定步长、版本化 RNG、Checkpoint 和正向重放定义了任意时间恢复。 |
| 17 渲染架构 | 基本合理 | 最小 RenderFrame/RenderBackend 与 OpenGL 3.3 Core Backend 已分层实现；完整 RenderGraph 仍应留到后续阶段。 |
| 18 Vulkan 预留 | 合理 | 只保持边界、不提前开发 Vulkan 的策略正确。 |
| 19 资源系统 | 合理 | Handle + Lease + Scope 明确了所有权、失效和热重载；第一版同步加载能控制实现范围，异步 API 延后。 |
| 20 音频系统 | 基本合理 | SDL3 AudioStream、后端隔离和采样帧时钟合理；正式判定前必须用实测验证输出时钟精度。 |
| 21 输入与判定 | 合理 | 阶段 1 只规划占位接口，尚未进入阶段 0 构建；不能把示例签名当作正式 Gameplay API。 |
| 22 Studio | 合理 | EditorDocument 与 Runtime World 分离是必要边界；具体命令和预览同步可延后。 |
| 23 Shader 与材质 | 基本合理 | SPIR-V 中心管线和成熟工具链已确定；portable subset 仍需由真实 Shader 持续验证。 |
| 24 主循环 | 合理 | 应用层组合 AudioClock、Timeline、输入、Runtime 和 Renderer 的边界正确。 |
| 25 调试能力 | 合理 | 与复杂数据驱动系统匹配，应随各模块同步实现而非最后补做。 |
| 26 代码规范 | 合理 | snake_case、Result/Error、日志封装、线程检查、格式和 MSVC 警告基线已在阶段 0 落地；未来实时线程规则仍按模块实施。 |
| 27 测试规范 | 合理 | Catch2 v3 + CTest 已按 Core、Platform、World 和 Render Backend 拆分，并加入架构及 Player 失败路径入口；实际运行结果由完成报告记录。 |
| 28 ADR | 合理 | 0001 至 0028 覆盖基础工程、运行时、格式、时间、后端、编码、配置、产品边界、相机与远期平台边界。 |
| 29 Definition of Done | 合理 | 阶段 0 已建立构建、测试、模块边界、日志、格式和冒烟入口；发布流程落地后还需增加打包验证。 |
| 30 阶段规划 | 合理 | 阶段 0 已提供最小可运行 Player；阶段 1、5、8 已拆为可独立验收的子阶段，阶段 6 再组合完整运行时形成稳定播放器，阶段 7 Studio 复用同一 Runtime。性能验证独立为阶段 9A，Android 与 Vulkan 延期，不阻塞输入、判定和计分模型设计。 |
| 31 风险控制 | 合理 | 已覆盖过度设计、后端泄漏、Runtime 膨胀、双格式和状态型时间轴风险。 |
| 32 当前优先级 | 合理 | 阶段 0 的 SDL/OpenGL、Player、World、Core 和测试闭环已经实现；按目标环境完成可重复验收并记录结果后，再进入阶段 1A。 |
| 33 已决策与复核 | 合理 | 已知公共边界均已决策，仅将需要测量数据的实现参数有意延后。 |
| 34 原则总结 | 合理 | 与正文决策一致，可作为代码审查的高层检查表。 |

## ADR 一致性

| ADR | 评估 | 说明 |
| --- | --- | --- |
| 0001-0006 基础工程 | 合理 | 项目名、vcpkg、Presets、版本、EnTT 和渲染抽象已补录。 |
| 0007 Chart/Runtime/World | 合理 | 与模块依赖和阶段 1 路径一致。 |
| 0008 坐标与层级 | 合理 | 已与 0010 的 UUIDv7 决策交叉对齐。 |
| 0009 属性求值 | 合理 | 确定性较强，但多动画混合仍按计划延后。 |
| 0010 A/B 谱面格式 | 合理 | Importer 隔离兼容逻辑是关键；必须控制 B 的功能增长。 |
| 0011 Catch2/CTest | 合理 | 与 vcpkg、CMake Presets 和阶段 0 验收一致。 |
| 0012 SDL3 音频 | 基本合理 | 边界清楚；输出延迟和时钟稳定性仍需 Windows 实测。 |
| 0013 资源生命周期 | 合理 | generation Handle、Lease/Scope 和后端线程边界形成可实施闭环。 |
| 0014 统一 Chart Schema | 合理 | 与 ECS 组合模型一致，并把跨 Chart 引用和 Template 子树延后。 |
| 0015 简易 Chart 格式 | 合理 | 手写语法受限且转换确定，不会形成第二套 Runtime 语义。 |
| 0016 Apache-2.0 | 合理 | 与免费开源目标和成熟依赖策略一致，LICENSE、NOTICE 和第三方清单已建立。 |
| 0017 RuntimeSession | 合理 | 事务准备和 Replacement 避免半初始化及 Reload 破坏。 |
| 0018 TimingMap | 合理 | offset、BPM 和 Stop 边界已统一 Player、Studio 与 Behavior。 |
| 0019 动画 Layer | 合理 | 权重、mask 和 OverrideToken 具有确定性冲突规则。 |
| 0020 粒子时间轴 | 合理 | Checkpoint 不影响结果，适合 Studio Seek。 |
| 0021 Shader 管线 | 基本合理 | 工具链成熟，但移动 Shader portable subset 必须持续测试。 |
| 0022 Android 策略 | 基本合理 | 格式与时间戳方向正确，具体预算依赖真实设备。 |
| 0023 编码与线程 | 合理 | C++20 Result、错误转换和线程所有权可直接指导阶段 0。 |
| 0024 配置所有权与分阶段格式 | 合理 | 阶段 1A 前冻结跨阶段配置分类、所有权、覆盖/约束与失败规则。 |
| 0025 ProjectConfig v1 与路径安全 | 合理 | 阶段 1B 随真实消费者冻结 cuexis.project.json 定位、portable path 与物理 containment。 |
| 0026 Asset Index 与来源解析 | 合理 | 每资产根独立 cuexis.asset-index.json、不可变 AssetDatabase、不依赖目录枚举发现 AssetId。 |
| 0027 Playback SDK 产品边界 | 合理 | 将产品方向调整为 Playback SDK + 独立 Player + 独立 Studio，Judgement/Replay 为必选 SDK 交付。 |
| 0028 相机投影模型与 FrameSnapshot 契约 | 合理 | CameraComponent 归属 cuexis_render，顶层相机配置与对象相机组件；多相机选择/切换策略尚未冻结。

## 阶段实施检查点

阶段 0 已落实：

```text
正式 CMake/vcpkg manifest 和固定 baseline
阶段 0 直接依赖的用途、版本、许可证、分发方式和退出路径记录
单一版本配置来源、生成头和 manifest 一致性校验
BUILDING.md 的 Windows/MSVC 标准入口、格式检查与独立图形冒烟流程
Core/SDL Platform/World/Render/OpenGL Backend/Player 及模块化测试 target
```

阶段 0 的精确构建、CTest 和图形冒烟结果必须来自当前目标环境的实际执行记录；本审视报告只确认入口和边界已落地，不固化测试数量或硬件结果。

阶段 1 实施时：

```text
按 RUNTIME_SESSION.md 实现事务 prepare/commit、Unload 和 Reload
```

## 评估结论

文档不存在需要推翻总体架构的严重问题，已知剩余公共边界均有推荐方案。阶段 0 已按最小闭环实现，且未把 Runtime、Audio、完整 RenderGraph 或 Studio 提前带入构建。下一阶段的主要风险仍是远期规范诱导范围扩张；阶段 1 应继续按子阶段验收，只实现当前闭环所需代码，并把专项规范当作兼容约束而不是一次完成全部系统的要求。

