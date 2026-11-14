# Stage 10 Implementation Plan: Studio and Authoring Workflow

状态：future；未开始

更新日期：2026-10-16（按实际提交阶段收窄 PD 预览失败保证）

归档来源：[旧 Stage 7 Studio 计划](../../active/stage-07/plan.md)、
[Chart v5 计划](../../active/chart-format-update-for-v5/plan.md) 和
[Stage 8 发行计划](../stage-08/plan.md)。

前置：

```text
Stage 7A Gameplay Foundation
Stage 7.5 capabilities selected for the first Studio release
Stage 8 Chart v5 / CXT v2 / Packed Chart
Stage 9 Presentation extensions needed by the first Studio release
```

## 1. 阶段目标

建立面向 Chart v5 的独立 Cuexis Studio。Studio 是作者工具，不建立第二套
Chart 编译、Judgement、Runtime 或 Presentation 求值路径。

## 2. 创作内核

支持：

```text
Chart v5 authoring document
CXT v2 Template / Prototype / Pattern
Parameter editing
Chart schema validation
Undo / Redo
atomic save
migration backup
resource reference diagnostics
```

Studio 不直接编辑 Packed Chart。Packed Chart 是确定性编译产物。

## 3. 预览工作台

Viewport、Timeline 和拖动时间预览必须复用 PlaybackSession：

```text
Playback
Input preview
Judgement preview
Score / Combo preview
Presentation Environment
Model / Geometry preview
FrameSnapshot
```

Studio 的判定结果必须与 Player 和 external consumer 一致。

### 作者版本与活动预览版本

采用 PD 基线时遵守[细则 §12.3](../../active/stage-07/04-pd/design.md)：作者
文档的 Undo/Redo 不使用 Runtime mutation guard 实现。编辑后沿共同 compile/prepare 构建
带文档 revision/请求身份的候选；编译/prepare 失败不替换旧软件预览。已进入控制的候选，
只有在所有相关 owner 确认终态未生效且没有其他控制副作用时，才可保证旧组合保持。
软件已提交后设备激活失败则保留实际软件版本和回执，进入 Suspended；结果不明时沿原操作
查询/收束，不能把超时或取消请求当作已回滚。替换预览继续履行既有资源/控制 fence。

作者文档版本、已提交软件预览版本、最后成功画面来源和可用状态分别显示，依据已有提交/
控制/呈现证据；旧 CPU 图不证明旧预览仍有设备许可。全域确认后物化或首次 present 失败，
也不撤回软件提交或作者编辑。保存仍是独立原子文件事务。
旧编译结果在软件安装前复核请求身份；已经产生副作用的旧操作必须收束，后续编辑不构成
自动取消。恢复复用原合同允许的重试/重建/关闭；设备阶段重试不得重做软件提交。重新选择
旧文档也须走合法新事务，不增加常驻第二套预览运行状态来保证设备失败时无缝回退。

拖动时间复用 Seek；未接纳 UI 请求可合并，已接纳可靠控制必须有实际终态。未来分段重建需
先接受显式任务/取消合同，不能把同步 Seek 的成功改成“已开始”。多视口不是本阶段隐含能力：
若产品需要，另行比较独立 session 与共享 session 多端的运行状态、控制和资源成本；两者都
使用同一求值实现，不能仅增加 ConsumerId 就宣称获得独立 Renderer。

## 4. 发行工作流

提供：

```text
Source Project validation
CXT finite expansion preview
Packed Chart compile
40,000 entity count
16 MiB Packed Chart check
CXC closure check
source/compiled identity inspection
playback entry validation
```

编译和打包失败不得破坏上一份有效文件或有效预览。

## 5. 资源工作流

支持：

```text
Asset Browser
portable Model import
Skybox/Environment resource
Material/Shader diagnostics
resource closure
importer profile
capability profile
```

StudioPreferences、布局、最近项目、自动保存和本机路径不得写入 ProjectConfig、
Chart 或 CXC 内容语义。

## 6. 验收标准

- Studio、Player、external consumer 使用同一 PlaybackSession/Runtime 路径。
- Chart v5 和 CXT v2 可以编辑、保存、校验和重新加载。
- Pattern 展开结果可预览，且与 Packed 编译结果一致。
- 40,000 实体和 16 MiB 门禁在编辑器内可诊断。
- CXC 打包产物可以直接被 Player 播放。
- Undo/Redo 遵守作者事务；保存/迁移失败不破坏上一份有效文件，预览准备阶段发现资源缺失不替换旧软件状态。运行期设备或资源失效按实际控制阶段恢复，不扩大作者事务的回滚范围。
- 预览替换按细则 §12.3 验证未提交失败、终态未生效、取消超时、SoftwareApplied 后设备失败，以及全域确认后首次构帧/呈现失败；作者、软件与画面版本及许可分别正确，不承诺跨设备原子回滚。
- Studio 不暴露 World、EnTT、OpenGL、JSON DOM 或 Packed 内部实体给公共 SDK。

## 7. 明确不包含

- 第二套 Runtime 或 Renderer。
- 直接修改 ChartRuntime/World/GPU 对象。
- 任意运行时脚本。
- 把编辑器本机配置写入项目内容。
