# Stage 10 Implementation Plan: Studio and Authoring Workflow

状态：future；未开始

更新日期：2026-09-05

归档来源：[旧 Stage 7 Studio 计划](../stage-07/plan.md)、
[Chart v5 计划](../../active/chart-format-update-for-v5/plan.md) 和
[Stage 8 发行计划](../stage-08/plan.md)。

前置：

```text
Stage 7A Gameplay Foundation
Stage 7B+ capabilities selected for the first Studio release
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
- Undo/Redo、保存失败、迁移失败和资源缺失均有安全回滚。
- Studio 不暴露 World、EnTT、OpenGL、JSON DOM 或 Packed 内部实体给公共 SDK。

## 7. 明确不包含

- 第二套 Runtime 或 Renderer。
- 直接修改 ChartRuntime/World/GPU 对象。
- 任意运行时脚本。
- 把编辑器本机配置写入项目内容。
