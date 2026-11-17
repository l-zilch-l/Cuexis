# Stage 10 计划（提纲）：Studio 与创作/发行工作流

状态：future；未启动，仅为方向提纲

更新日期：2026-11-17

<!-- cuexis-meta
id: PLAN-future.stage-10.plan
status: future
authority: none
-->

本文件是**未启动阶段的提纲**，不是执行计划。启动本阶段时，用
[plan.md](../../../templates/plan.md)（详细规划协议）重写，并在变更记录里注明替换关系。

归档来源：[旧 Stage 7 Studio 计划](../../completed/stage-07/plan.md)、
[Chart v5 计划](../../active/chart-format-update-for-v5/plan.md) 与
[Stage 8 发行计划](../stage-08/plan.md)。

## 阶段目标

候选，未承诺：建立面向 Chart v5 的独立 Cuexis Studio。Studio 是作者工具，不建立第二套
Chart 编译、Judgement、Runtime 或 Presentation 求值路径；首个发行版纳入的能力集合由启动时的
发行范围决定。

## 范围与边界

- 前置基线为 Stage 8 的 Chart v5 / CXT v2 / Packed Chart 与 Stage 9 中首个 Studio 发行所需的
  Presentation 扩展；本阶段不重新验收这些合同。
- Studio 只编辑作者源（Chart v5 authoring document 与 CXT v2），不直接编辑 Packed Chart；
  Packed Chart 是确定性编译产物。
- 预览工作台、发行工作流和资源工作流都必须复用 Player 已验收的求值路径；本阶段不因 Studio
  需要而修改 Chart、Input、Judgement、Replay 或 FrameSnapshot 语义。
- 与 Stage 11–14 的边界：本阶段不做曲线/高级表现、平台规模化、稳定 C ABI 或发行后的能力扩展。

## 入口条件

- Stage 7A Gameplay Foundation 关闭。
- Stage 7.5 中首个 Studio 发行所选能力已被接受。
- Stage 8 Chart v5 / CXT v2 / Packed Chart 关闭。
- Stage 9 中首个 Studio 发行所需的 Presentation 扩展已交付并接受。
- 若采用 PD 基线，[PD 细则 §12.3](../../completed/stage-07/04-pd/design.md) 的预览与控制合同已接受。

## 已确认约束

- Studio 是作者工具，不建立第二套 Chart 编译、Judgement、Runtime 或 Presentation 求值路径；
  不直接编辑 Packed Chart。
- Viewport、Timeline 和拖动时间预览必须复用 PlaybackSession；Studio 的判定结果必须与 Player
  和 external consumer 一致。
- 拖动时间复用 Seek；未接纳的 UI 请求可合并，已接纳的可靠控制必须有实际终态。
- 采用 PD 基线时遵守 [PD 细则 §12.3](../../completed/stage-07/04-pd/design.md)：Undo/Redo 不使用
  Runtime mutation guard 实现；编辑后沿共同 compile/prepare 构建带文档 revision/请求身份的候选，
  编译/prepare 失败不替换旧软件预览；软件已提交后设备激活失败保留实际软件版本与回执并进入
  Suspended；结果不明时沿原操作查询/收束，不能把超时或取消请求当作已回滚；不承诺跨设备原子回滚。
- 作者文档版本、已提交软件预览版本、最后成功画面来源和可用状态分别显示；旧 CPU 图不证明旧
  预览仍有设备许可。全域确认后物化或首次 present 失败，不撤回软件提交或作者编辑。
- 保存仍是独立原子文件事务；编译和打包失败不得破坏上一份有效文件或有效预览。
- StudioPreferences、布局、最近项目、自动保存和本机路径不得写入 ProjectConfig、Chart 或 CXC
  内容语义。
- 多视口不是本阶段隐含能力：若产品需要，另行比较独立 session 与共享 session 多端的运行状态、
  控制和资源成本；两者都使用同一求值实现，不能仅增加 ConsumerId 就宣称获得独立 Renderer。
- Studio 不暴露 World、EnTT、OpenGL、JSON DOM 或 Packed 内部实体给公共 SDK。

## 候选方向（非承诺）

启动时可能按以下工作线细化，均未承诺：

- 创作内核：Chart v5 authoring document、CXT v2 Template/Prototype/Pattern、参数编辑、Chart
  schema 校验、Undo/Redo、atomic save、migration backup、resource reference diagnostics。
- 预览工作台：Playback、Input preview、Judgement preview、Score/Combo preview、Presentation
  Environment、Model/Geometry preview、FrameSnapshot。
- 发行工作流：Source Project validation、CXT 有限展开预览、Packed Chart compile、实体规模与
  Packed entry 上限的门禁诊断（门禁值以 Stage 8 与格式合同为准）、CXC closure check、
  source/compiled identity inspection、playback entry validation。
- 资源工作流：Asset Browser、portable Model import、Skybox/Environment resource、Material/Shader
  diagnostics、resource closure、importer profile、capability profile。
- 若未来纳入分段重建，须先接受显式任务/取消合同，不能把同步 Seek 的成功改成"已开始"。

## 待决问题（启动前必须回答）

1. 首个 Studio 发行版纳入哪些 Stage 7.5 与 Stage 9 能力？由 owner 在启动时批准。
2. 是否需要多视口；若需要，选独立 session 还是共享 session 多端？由产品所有者在启动时决定。
3. Studio MVP 与正式产品的边界由谁定义、何时冻结？由 owner 决定。
4. 发行基线是否包含 PD；若包含，由哪个已接受批次提供 §12.3 的实现与证据？
5. 设备阶段的重试/重建/关闭恢复策略采用哪种已接受形态？由 owner 与 Runtime owner 在启动时决定。

## 验收标准

启动时定义。当前只写方向性判据：

- Studio、Player 与 external consumer 使用同一 PlaybackSession/Runtime 路径，判定结果一致。
- Chart v5 与 CXT v2 可编辑、保存、校验和重新加载，Pattern 展开结果可预览且与 Packed 编译结果一致。
- CXC 打包产物可直接被 Player 播放，编译/打包失败不破坏上一份有效文件或有效预览。
- 保存/迁移失败不破坏作者文档；预览准备阶段发现资源缺失不替换旧软件状态；运行期设备或资源
  失效按实际控制阶段恢复，不扩大作者事务的回滚范围。
- 若采用 PD 基线，预览替换按 §12.3 验证未提交失败、终态未生效、取消超时、SoftwareApplied 后
  设备失败、全域确认后首次构帧/呈现失败，且作者、软件与画面版本及许可分别正确。
- Studio 不通过公共 SDK 暴露 World、EnTT、OpenGL、JSON DOM 或 Packed 内部实体。

具体矩阵、命令、数值与证据形式在启动时由 owner 与规划模型制定，本提纲不承诺。

## 明确不包含

- 第二套 Runtime 或 Renderer。
- 直接修改 ChartRuntime/World/GPU 对象。
- 任意运行时脚本。
- 把编辑器本机配置写入项目内容。
