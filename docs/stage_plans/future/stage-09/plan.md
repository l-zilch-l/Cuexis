# Stage 9 计划（提纲）：Presentation Foundation 与 Chart v6

状态：future；未启动，仅为方向提纲

更新日期：2026-11-23

<!-- cuexis-meta
id: PLAN-future.stage-09.plan
status: future
authority: none
-->

本文件是**未启动阶段的提纲**，不是执行计划。启动本阶段时，用 [plan.md](../../../templates/plan.md)（详细规划协议）重写，并在变更记录里注明替换关系。

归档来源：[Chart v6 历史设计输入](../../historical-inputs/chart-format-update-for-v6/plan.md)、[Portable Presentation v1](../../../formats/PORTABLE_PRESENTATION.md) 与 [Material/Shader v1](../../../formats/MATERIAL_SHADER.md)。原 Stage 9 的环境/模型目标已收敛为同一条 Presentation Foundation。

## 阶段目标

候选，未承诺：以一条 Presentation Foundation 承载三项交付——Presentation Environment（solid/equirectangular/cubemap skybox、fog、环境参数）、Model v1（静态 glTF 2.0 离线导入为 portable Model/Mesh/Material/Texture）和 Chart v6（内置基础网格、默认扁平片、submesh 材质槽）。Chart v6 是模型与默认网格的承载版本，不改变 Judgement、Score 或 Replay 语义；Stage 10 Studio 只依赖本阶段已交付的稳定子集。

## 范围与边界

- 前置基线为 Stage 8 的 Chart v5＋Gameplay v3 / CXT v2 / 正式成品 baseline，以及 Portable Presentation v1 与 Material/Shader v1。
- 消费 Stage 8 承接的 [RR 架构与结构清理成果](../../active/stage-07-rr/plan.md)：离线 importer 接入工具侧，成品读取保持独立；表现接入已有会话、资源生命周期和帧提交入口，不增加模型专用播放控制链。
- 继承 Stage 8 接受的 Gameplay v3 与旧版本兼容边界；只有在某个表现批次确实需要时才消费已接受的 Stage 7.5 capability。Chart v6 不另改玩法版本，也不回写 Stage 7A 的旧规则。
- 环境是独立 Presentation 资源，不是普通 Chart Object 或 Renderable Mesh；Model/Submesh 资源进入 CXC closure，能力缺失时稳定拒绝。
- 与 Stage 11 的边界：曲线形变、line 和 shader 执行不在本阶段，Stage 11 仅在本阶段关闭后接收曲线表现扩展；与 Stage 10 的边界：本阶段只交付 Studio 可消费的稳定子集，不交付 Studio 产品。

## 入口条件

- Stage 8 关闭并交付 Chart v5 production baseline。
- Portable Presentation v1 与 Material/Shader v1 合同冻结。
- Stage 8 正式玩法及旧内容兼容合同已接受；若某表现批次需要 Stage 7.5 能力，该能力已被接受。
- 若采用 PD 基线，PD 有类型表现域边界已接受。

## 已确认约束

- Chart v6 是模型与默认网格的承载版本，不改变 Judgement、Score 或 Replay 语义。
- 只接受静态 glTF 2.0；拒绝 animation、skin、morph target、camera、light 和运行时扩展。离线 importer 生成 portable Model v1，Playback/prepare 不解析 glTF；CXC 只打包 portable payload，source glTF 只能保留为 Source Project/source entry。
- 环境是独立 Presentation 资源，不是普通 Chart Object 或 Renderable Mesh。
- 非相机、带 renderable 且省略 mesh 的对象默认使用 sheet；相机永不获得默认网格。
- 沿用 `transform.scale` 与既有 Behavior/AnimationClip 白名单，复用 RR 提前承担、Stage 8 正式收敛的 CXT2 Animation 导入和 Animator 绑定；只新增模型所需资源适配，不建立另一套动画运行时或 morph/骨骼系统。
- 合法 v4/v5 输入的既有 FrameDigest 保持不变；FrameSnapshot、FrameDigest、capability 与预算变化必须独立版本化。
- 采用 PD 基线时，Environment 与 Model/Submesh 接入 [PD 有类型表现域边界](../../completed/stage-07/04-pd/design.md) §12.2，复用单一求值与 Snapshot 物化协调入口；环境不能被硬塞成普通对象，也不建立动态 provider 或另一套 Renderer 求值；新增 submesh/资源字段同步更新 Snapshot/Digest/capability 与单帧/峰值预算。
- Stage 11 仅在本阶段关闭后接收曲线表现扩展。

## 候选方向（非承诺）

启动时可能按以下工作线细化，均未承诺：

- Presentation Environment：solid/equirectangular/cubemap skybox、fog 与环境参数；冻结相机平移、时间驱动、有限 Behavior、纹理/内存预算、移动端降级和 Snapshot/Digest 影响。
- Model v1 与导入：节点 TRS 在导入阶段处理为 portable submesh 数据；importer 只产出 portable payload，不把 glTF 语义带入 Playback。
- 内置网格与 Chart v6：triangle、quad、sheet、cube、uv-sphere、prism、cylinder 的 bytes、identity 和分段常数；默认网格规则与对象/相机区别。
- Submesh 与观察面：有序 submesh 槽和静态材质覆盖，未覆盖槽使用 Model 默认材质；digest、capability 与预算随字段扩展同步版本化。

## 待决问题（启动前必须回答）

1. Chart v6 / Model v1 交付 candidate 还是正式 capability？由 owner 在启动时决定。
2. 内置网格目录是否只包含上述七种基础网格，还是按首批内容扩展？由 owner 与格式 owner 决定。
3. 移动端降级以及纹理/内存预算阈值由谁定，是否进入本阶段硬门禁？
4. 发行基线是否包含 PD；若包含，由哪个已接受批次提供 §12.2 的接入合同与证据？

## 验收标准

启动时定义。当前只写方向性判据：

- Chart v6 ADR、Model v1 Spec、Schema、Reader/Writer 和 importer 合同先于生产实现被接受。
- 非法 glTF、缺资源、错误拓扑、缺 capability 和超预算均稳定失败。
- portable Model、内置网格和 submesh closure 可跨平台复现，且 Snapshot/Digest 变化经版本门禁。
- 对支持的 Seek/Reload，表现从该内容版本的既有时间映射重建，不依赖上一帧或 GPU 状态；缺失操作按版本稳定拒绝，不能为新表现域私建时钟。
- Player、headless、external consumer 与 CXC package gates 通过，owner acceptance 完整。

具体矩阵、命令、数值和证据形式在启动时由 owner 与规划模型制定，本提纲不承诺。

## 明确不包含

- 曲线形变、line 和包级后处理（Stage 11/13）。
- 骨骼、morph、PBR、Shader Graph。
- Studio 完整产品；Studio MVP 由 Stage 10 负责。
- 任意运行时脚本，或把表现几何当作 Judgement 几何。
