# Stage 7 Implementation Plan: Cuexis Studio Core

状态：future；未开始

更新日期：2026-08-10

归档来源：[旧版 PROJECT_GUIDE](../archive/PROJECT_GUIDE_LEGACY_2026-08-10.md) 与
[SDK transition plan 快照](../archive/CUEXIS_SDK_TRANSITION_PLAN_2026-08-10.md)。

## 1. 阶段目标

建立可用的独立 Cuexis Studio 核心。Studio 编辑 ChartDocument 和项目配置，但运行时预览必须
复用 PlaybackSession，不建立第二套 Chart 编译、Runtime 或渲染求值路径。

## 2. 实施范围

- 实现 EditorDocument、选择模型和命令式 Undo/Redo。
- 实现 Hierarchy、Inspector、Viewport、Timeline 初版和 Asset Browser。
- 实现 ChartDocument 加载、保存、原子写入和迁移备份。
- 复用 SDK/Player 的 ProjectConfig 解析和迁移，不建立第二套配置 Schema。
- 实现 StudioPreferences，保存布局、最近项目、自动保存和快捷键等本机设置。
- 分区编辑项目设置与用户偏好，保留未知可选扩展。
- Viewport 和拖动时间预览只使用 PlaybackSession。
- 接入材质参数、Shader 编译诊断和目标宿主 Profile 兼容性显示。

## 3. 验收标准

- 编辑器操作 ChartDocument，不直接修改 Runtime Entity、World 或最终 Component。
- Chart 支持保存、重新加载、撤销、重做和拖动时间预览。
- Viewport、Player 和 external consumer 对相同输入使用同一 PlaybackSession/Runtime 路径。
- 材质参数可编辑和预览，Shader 编译错误可定位且不破坏上一有效预览。
- Studio 与 Player 对同一 ProjectConfig 产生相同 typed 项目解析结果。
- StudioPreferences 不写入 ProjectConfig，本机路径和布局不污染项目仓库。
- 写入失败时上一有效文件保持不变；迁移失败恢复备份并产生诊断。
- ProjectConfig 未知可选扩展在 Studio 读取和保存后往返保留。
- Studio 显示目标宿主 Profile 和不兼容表现能力。
- Studio 不链接 Player CLI、窗口或 UserPreferences 实现。

## 4. 明确不包含

- Studio 私有 Runtime/World 路径。
- 直接编辑编译后 ChartRuntime、AnimationProgram 或 GPU 对象。
- 把编辑器布局、本机路径或自动保存状态写入项目格式。
