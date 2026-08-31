# 阶段 1B 实施计划：资源生命周期闭环

状态：实现完成  
规划日期：2026-07-18  
前置基线：[阶段 1A 完成报告](../../../stage_reports/stages/stage-01/stage-1a-completion.md)
完成记录：[阶段 1B 完成报告](../../../stage_reports/stages/stage-01/stage-1b-completion.md)

SDK 转型说明：本文件保留阶段 1B 的历史实施事实。ADR 0027 和[SDK 转型方案](../../historical/sdk-transition/plan.md)不修改 ProjectConfig/Asset Index/Handle/Lease/Scope/事务回滚语义；ContentProvider、PlaybackSession 与安装包迁移分别在阶段 1C-1E 完成。

后续取代说明：本文涉及的 Simple Chart 路径已由 ADR 0035 在阶段 2A.1 移除；相关内容仅是阶段 1B 历史事实。

## 1. 已确认设计

- ProjectConfig 固定为 `<project-root>/cuexis.project.json`，格式为 `cuexis.project`。
- AssetId 来源使用每个资产根中的独立 `cuexis.asset-index.json`，格式为 `cuexis.asset-index`。
- ProjectConfig 的 `entry.chart` 使用 `{root, path}` bootstrap locator，不把路径当作 AssetId。
- `projectId` 采用规范 UUIDv7；共享 UUID 校验能力应从 Chart 私有实现提取到 Core，Chart 保留兼容包装。
- ProjectConfig 与 Asset Index 的运行时稳定诊断由 typed Reader/semantic validator 负责；Schema artifact 使用 Draft 7，避免依赖对 Draft 2020-12 的不完整支持。
- 配置加载永不自动写回；显式保存实现同目录临时文件、完整校验和平台原子 replace。
- 资产来源和路径采用 portable ASCII、正斜杠、严格相对路径；拒绝 reparse point 逃逸、重叠根和重复物理来源。
- 1B 资源内容先是有界 CPU blob 和元数据，正式 Mesh/Material/Texture 内容格式、导入器和 GPU 派生对象留到后续阶段。
- Demo 实例化真实 Mesh/Material/Texture Handle 和 ResourceScope，但继续以 DebugDraw 作为可运行图形输出，不提前实现 Mesh GPU 绘制。

详细格式和威胁模型见 [ADR 0025](../../../adr/0025-project-config-v1-and-path-security.md) 与 [ADR 0026](../../../adr/0026-asset-index-and-source-resolution.md)。

## 2. 实施批次

### 1B-0：契约与 fixture（已完成）

交付 `schemas/cuexis.project.v1.schema.json`、`schemas/cuexis.asset-index.v1.schema.json`、ProjectConfig/Asset Index 示例和 ADR 测试向量。先冻结字段路径、预算、UUID、路径、扩展、迁移和原子失败语义，再开始公共 C++ 类型。

### 1B-1：应用侧 Project 前端（已完成）

新增 `cuexis_project` 应用组合 target，包含：

- 固定文件定位和项目根解析。
- JSON parse、typed Reader、ProjectConfig 语义校验。
- portable path 规范化、root overlap/reparse 检查和入口 regular-file 检查。
- 内存中的 `PreparedProject`，完整成功后才发布 AssetDatabase 输入。
- 显式 `saveAtomic`，失败保留上一有效文件。

该模块不向 Runtime、Chart 或 Assets 公共接口暴露 JSON DOM，不创建全局配置单例。

### 1B-2：AssetDatabase 与 Asset Index（已完成）

将 `cuexis_assets` 从 INTERFACE 升级为静态库，建立不可变 AssetRecord 索引。实现固定索引读取、跨 root 重复检测、来源 containment、类型检查、稳定依赖图和有界文件读取接口。目录枚举不参与 AssetId 发现。

### 1B-3：ResourceManager、Lease 和 Scope（已完成）

实现 typed slot/state/generation/contentRevision/managerToken，及 Mesh、Material、Texture 的同步 CPU blob loader。ResourceLease 负责强引用，ResourceScope 对直接和传递依赖去重。补齐 Required/Fallback/Optional、固定类型 fallback、loader 错误、依赖 diamond/cycle 和旧 Handle 失效。

### 1B-4：RuntimeSession 事务接入（已完成）

RuntimeSession 构造时注入 ResourceManager。prepare 阶段先生成稳定资源需求，再获取临时 Scope，然后实例化 World；RenderableComponent 只保存 typed Handle，不保存 Lease。Prepared 绑定 owner Session/Manager，commit 拒绝跨 owner 使用；World 和 Scope 的释放顺序固定为 World -> Scope。Session 保存活动诊断，reload 失败保持旧诊断/旧资源状态。

### 1B-5：Player 与资源 demo（已完成）

增加 `--project <目录|cuexis.project.json>`，默认加载复制到二进制目录旁的 1B project fixture。保留 `--chart` 作为阶段 1A 无资源回归入口，并明确与 `--project` 的互斥关系。demo index 登记至少一组 Mesh、Material、Texture 及一层依赖，日志输出 root 数、Ready 资源数、Renderable 数、3 objects、9 Debug commands 和 3 frames。

### 1B-6：门禁与交付（已完成）

同步更新 CMake active-target/dependency allowlist、架构扫描、BUILDING、PROJECT_GUIDE、RUNTIME_SESSION、Chart/Asset 文档和第三方记录。创建 `docs/stage_reports/stage_1b_completion_report.md`，记录实际测试数量、资源预算、GPU 输出、失败路径和剩余 1C 边界。

## 3. 测试矩阵

| 范围 | 必须覆盖 |
| --- | --- |
| ProjectConfig | 定位、固定文件名、损坏 JSON、重复键、format/version、核心未知字段、UUID、扩展、原子写失败 |
| 路径安全 | 绝对/相对、`.`/`..`、反斜杠、保留名、root overlap、大小写别名、symlink/junction 越界、入口类型 |
| Asset Index | 重复 ID、类型冲突、重复来源、缺失 source、依赖顺序稳定、依赖环、数量/深度上限 |
| ResourceManager | 同资源去重、状态、invalid index/generation、manager token、generation 重用、loader 失败、固定 fallback |
| Policy | Required 失败、Fallback warning + typed placeholder、Optional skip + diagnostic |
| Lease/Scope | move/释放、传递依赖闭包、diamond 去重、scope 清理、无泄漏和跨线程拒绝 |
| Runtime | prepare/commit owner 检查、Renderable Handle、prepare/reload 失败回滚、World -> Scope 顺序、unload |
| Player | `--project` 定位、与 `--chart` 冲突、缺失项目/索引/入口、旧 1A chart、原有四条失败路径 |

阶段 1A 的 `113` 项 CTest 是最低回归基线。阶段 1B Debug 与 Release fresh/clean build 均实际发现并通过 `153/153` 项，其中包含 145 个 Catch2 case、1 个架构扫描和 7 条 Player CLI/失败路径。

## 4. 验收门禁

Debug 和 Release 都必须执行 fresh configure、clean build、完整 CTest、架构扫描和 clang-format。canonical/simple × Debug/Release 继续执行四次 OpenGL smoke。1B 不要求真实 Mesh 像素绘制，但必须证明 RenderableComponent 获得有效 Handle、Scope 在 Session 销毁后释放强引用，旧 Handle 不会命中新槽位。

## 5. 明确非目标

```text
Behavior Track 求值、Transform Keyframe、chartTimeMs、Seek 重采样
AudioClock、输入、判定、UserPreferences、DeviceProfile
异步 Future/协程/取消、文件监听热重载、LRU 和设备预算
正式 Mesh/Material/Shader 内容 Schema、Importer、Shader variant
真实 Mesh GPU 绘制、Studio、编辑器 UI 和资源浏览器
```

## 6. 依赖与顺序约束

ProjectConfig/Asset Index 的应用前端必须先于 AssetDatabase 发布；AssetDatabase 和 ResourceManager 必须先于带 Renderable 的 RuntimeSession；RuntimeSession 必须在 RenderBackend/平台能力确认后按 ADR 0024 的组合顺序创建。Runtime 不依赖 SDL 或 OpenGL backend，ResourceManager 不调用 OpenGL，Chart 不依赖 Assets/World。

## 7. 实际完成快照

阶段 1B 完成版本为 `26.07.18.18-1`。`cuexis_project`、两份 Draft 7 Schema、独立 Asset Index、不可变 AssetDatabase、同步 ResourceManager、move-only Lease、ResourceScope、RuntimeSession 资源事务和默认项目 demo 均已进入正式构建。默认 fixture 包含 1 个资产根、3 个 CPU blob、3 个 Renderable，且 `material.basic -> texture.white` 提供一层传递依赖。

Debug 与 Release 均已通过 fresh configure、clean build、`153/153` CTest、架构扫描和全仓格式门禁。两种配置下的默认阶段 1B Project 与阶段 1A canonical/simple 共 6 组 GPU smoke 全部通过，实测结果见完成报告。

## 8. 后续边界

阶段 1C 已完成 BehaviorSystem、Transform Keyframe、`chartTimeMs`、Seek 绝对重采样、PlaybackSession
第一版与 headless FrameSnapshot。阶段 1D 已完成 ChartClock/HostClock/CuexisAudio、RuntimeTimeline
与 Prepared Playback；阶段 1E 已完成 ContentProvider、安装包和 external consumer 边界。Stage 3
随后交付 Portable Mesh/Texture2D/Unlit Material 与 OpenGL adapter。异步加载、文件监听热重载、
Judgement/Replay 与 Studio 仍属于后续阶段；当前状态见
[CURRENT_STATUS.md](../../../CURRENT_STATUS.md)。
