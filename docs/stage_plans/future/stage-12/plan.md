# Stage 12 计划（提纲）：规模化、桌面性能与目标平台

状态：future；未启动，仅为方向提纲

更新日期：2026-11-23

<!-- cuexis-meta
id: PLAN-future.stage-12.plan
status: future
authority: none
-->

本文件是**未启动阶段的提纲**，不是执行计划。启动本阶段时，用 [plan.md](../../../templates/plan.md)（详细规划协议）重写，并在变更记录里注明替换关系。

归档来源：[旧 Stage 9A 性能计划](../../historical-inputs/stage-09a/plan.md)、[Android 历史输入](../../historical-inputs/stage-09b/plan.md) 与 [Vulkan 历史输入](../../historical-inputs/stage-10/plan.md)。原 Stage 11 的规模化目标也并入本阶段。

## 阶段目标

候选，未承诺：验证 Chart、Playback、Gameplay 和 Presentation 合同在大规模内容、桌面设备、Android 以及可选 Vulkan adapter 上的可用性，形成可复现的性能、预算、生命周期和平台能力证据。本阶段不重新定义 Chart、Input、Judgement、Replay 或 FrameSnapshot 语义。

## 范围与边界

- 前置基线为 Stage 8 正式验收的 Chart v5 + Gameplay v3、CXT v2 / Packed Chart 与所选 Stage 7.5 能力、Stage 9 Presentation Foundation、Stage 10 中测量场景所需的 Studio 工作流，以及 [Stage 11](../stage-11/plan.md) 的 Chart v7 几何合同（用于任何被测表现 profile）；Stage 7A/旧 v2 按已承诺旧内容兼容范围承接。
- 基础架构承接 [RR §24](../../active/stage-07-rr/findings.md#24-统一实施前设计裁定2026-11-23)届时实际接受的时间、资源、构建分层和会话控制成果；RPA 历史材料保留原证据限制。本阶段只据真实瓶颈做规模与平台优化，不重建基础架构，也不沿用旧批次结果预认证新阈值、设备或后端。
- 与 Stage 14 的边界：本阶段交付 [Stage 14](../stage-14/plan.md) 所需的真实 consumer 与部署证据，但稳定 C ABI 唯一归属 Stage 14，本阶段不定义。

## 入口条件

- Stage 7A 与所选 Stage 7.5 能力已验收。
- Stage 8 正式 v3 Writer、播放组合及 Stage 9 已关闭并接受。
- Stage 10 中测量场景需要的 Studio 工作流可用。
- Stage 11 的 Chart v7 几何合同可供被测表现 profile 消费。
- 目标设备、测量方法与可接受的证据形式在启动时由 owner 接受。
- 所消费 RR 交付绑定实现/验收基线及保留限制；本阶段据 Stage 8 正式 v3 的实际基线开展平台验收，被测正式组合、旧内容兼容组合和额外实验组合分别具名。RR 实验验证不替代 Stage 8 正式验收，也不证明目标平台或默认 SDK 已接受；不再要求完整新交付 v2 正式化。

## 已确认约束

- 本阶段验证既有合同在真实内容、目标设备和可选后端上的可用性，不重新定义 Chart、Input、Judgement、Replay 或 FrameSnapshot 语义。
- 统计关闭后不得改变 FrameSnapshot、JudgementResult 或 Replay。
- 只验证原始输入时间戳和延迟链路；InputProfile/CalibrationProfile 只有在另行批准后才持久化。
- 不向 Playback、Chart、Judgement 或 FrameSnapshot 暴露 Vulkan 类型；不可用后端稳定诊断，回退只由显式策略决定。
- 沿 [RR 应用分层](../../active/stage-07-rr/contracts/render-player-writer.md#应用分层与内部入口f313f316)复用单一生产会话 owner；平台 Player/设备装配归应用侧，SDK 不依赖 app 支持库。Chart 基础/codec/tools 与五类 ResourceScope 共用加载流程沿已接受闭包复用，平台优化不得重新引入作者依赖、重复资源所有权或另一个控制器。
- [设备等待超时](../../active/stage-07-rr/contracts/render-player-writer.md#所有权暂停与收尾)由各平台 Player 自行实现和决定，可无期限或采用平台期限；SDK 不提供该机制、策略或默认值。平台/驱动真实失败照常报告，超时不能代替取消终态、清理、ACK 或安全恢复证明，不另建通用超时框架。
- 若采用 PD 基线，平台策略须覆盖 [PD 细则 §12.4](../../completed/stage-07/04-pd/design.md) 的保留资格撤销、真实 CPU 释放与 GPU fence 分离：内存压力不能强制回收外部 lease；Context/资源换代不能把旧暂停图直接贴成新许可；旧图丢失且仍暂停时明确无可重绘图，不能用当前较新修订替代或执行 frozen update；跨代精确暂停图恢复若成为产品要求，另行接受 portable 资源重新取得入口和来源校验；后台/恢复/低内存与保留加替换峰值均需真实设备证据。

## 候选方向（非承诺）

启动时可能按以下工作线细化，均未承诺：

- 桌面性能与大谱面：测量 Packed/CXC load、CXT expansion、prepare 峰值、Runtime 内存、Judgement query、Replay、FrameSnapshot、Seek/Reload、AudioClock、CPU/GPU frame time 和资源上传；覆盖大规模语义实体、Pattern 高低重复、复杂资源闭包、动画和 Judgement requirements；形成版本化 DesktopDeviceProfile，区分硬预算、软目标、用户偏好、EffectiveSettings 和降级原因。
- Android SDK 与宿主适配：验证 Android Playback SDK、headless、可选 OpenGL ES 3.0 adapter、APK/AAB、AssetManager、后台恢复、Context 丢失、内存压力和真实设备音频/输入链路；为 KTX2/Basis Universal、meshoptimizer、Ogg/Vorbis 和 GLSL ES 300 定义 target profile，派生资源记录 source hash、importer、profile 和压缩参数。
- Vulkan 可选 adapter：审查 RenderBackend、PipelineDesc、BindingSet、资源生命周期和 SPIR-V/cache 路径；在隔离的 LaunchOptions/RenderConfig 中验证显式 auto/opengl/vulkan 请求、capability、回退和 EffectiveSettings；如有真实需求实现最小 adapter；OpenGL/Vulkan 对同一 portable profile 产出等价的规范化表现摘要。

## 待决问题（启动前必须回答）

1. 目标桌面/Android 设备矩阵与数值阈值由谁定、何时冻结？由 owner 决定。
2. Vulkan 接受、延期还是拒绝产品化？由 ADR 接受。
3. 哪些统计进入硬门禁、哪些作为软目标或用户偏好？由 owner 决定。
4. target profile 的派生资源策略（KTX2/Basis Universal、meshoptimizer、Ogg/Vorbis、GLSL ES 300）是否全部纳入首个 profile？由 owner 与平台 owner 决定。
5. 是否采用 PD 基线；若采用，由哪个已接受批次提供 §12.4 的实现与设备证据？

## 验收标准

启动时定义。当前只写方向性判据：

- 获选的桌面、Android 和可选 Vulkan 矩阵绑定正式 v3 的实际实现、内容与播放组合，有可复现的真实测量，环境与设备限制明确记录；RR 的结构清理、实验夹具或其他平台结果不作为性能/平台认证。
- 大规模内容下加载、展开、判定、采样和渲染预算分别有硬门禁或明确降级。
- Android 生命周期、资源派生、音频、触摸时间戳和内存压力有真实设备证据。
- Vulkan 形成接受、延期或拒绝产品化的 ADR；不可用后端稳定诊断，回退只由显式策略决定。
- static/shared、external consumer、headless 与目标平台门禁通过，owner acceptance 完整。
- 复验应用/引擎和作者/运行闭包、资源释放及单一 owner；设备确认前后、真实失败、Player 无期限/自定期限及迟到回执均按同一合同处理。跨平台旧内容仅对具名语料与独立预期作兼容结论，不外推未测组合。

具体矩阵、命令、数值与证据形式在启动时由 owner 与规划模型制定，本提纲不承诺。

## 明确不包含

- 修改 Chart v5/v6/v7 核心语义或为单一设备修改 Judgement。
- 任意运行时脚本、宿主 UI、在线服务、编辑器 ABI 或稳定 C ABI。
- 把 Android、Vulkan 或性能 profile 写入 Chart 内容 identity。
