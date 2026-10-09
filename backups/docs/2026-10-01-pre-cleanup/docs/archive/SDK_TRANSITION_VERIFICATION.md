# Cuexis SDK 转型验证报告

状态：历史验证快照；已被 ADR 0029、阶段 1C 完成报告和 260722 全量审查补充/取代
报告日期：2026-07-20
测试版本：`26.07.18.18-1-dev`（stage 1B 基线 + SDK 转型改造）

阅读说明：本文只证明 2026-07-20 第一轮改造当时的代码和测试状态。文中的 `seek()` 占位、Player/Runtime 双路径和未接入 ContentProvider 等描述已经过时，不得作为当前 API、架构或阶段状态依据。

## 1. 改造范围

本次改造在阶段 1B 已完成代码的基础上进行，不修改现有模块的内部实现逻辑，只新增 SDK 门面层并进行应用层重组。

### 新增模块

| 模块 | 描述 |
|---|---|
| `cuexis_playback` | PlaybackSession SDK 门面，封装 Chart 加载/编译和 RuntimeSession 生命周期 |

### 新增文件

| 文件 | 行数 | 描述 |
|---|---|---|
| `engine/playback/include/cuexis/playback/playback_session.hpp` | ~85 | PlaybackSession 公共接口 + RuntimeFrame、FrameSnapshot、ChartInfo 类型 |
| `engine/playback/include/cuexis/playback/content_provider.hpp` | ~50 | IContentProvider 抽象接口 + BlobLimits、ContentBlob |
| `engine/playback/src/playback_session.cpp` | ~195 | PlaybackSession 完整实现 |
| `engine/playback/CMakeLists.txt` | ~20 | CMake target 定义 |

### 修改文件

| 文件 | 变更 |
|---|---|
| `engine/CMakeLists.txt` | +1 行：`add_subdirectory(playback)` |
| `CMakeLists.txt`（根） | `CUEXIS_ACTIVE_TARGETS` 新增 `cuexis_playback`；新增依赖验证 |
| `app/player/CMakeLists.txt` | links 新增 `cuexis::playback` |
| `app/player/src/player_app.cpp` | 重构为 PlaybackSession + RuntimeSession 双路径 |
| `cmake/VerifyArchitecture.cmake` | 新增 SDK 公共头 EnTT/SDL/GL 泄漏检查 |

## 2. 测试结果 — 153/153 全部通过

```
测试总计:  153
通过:      153
失败:      0
时间:      24.82 秒
```

### 按模块分布

| 模块 | 测试数 | 结果 |
|---|---|---|
| Architecture scan | 1 | 通过（含新增 Playback 公共头泄漏检查） |
| Player failure paths | 7 | 通过 |
| cuexis_core_tests | 22 | 通过 |
| cuexis_json_support_tests | 13 | 通过 |
| cuexis_project_tests | 9 | 通过 |
| cuexis_platform_sdl_tests | 6 | 通过 |
| cuexis_assets_tests | 18 | 通过 |
| cuexis_chart_tests | 33 | 通过 |
| cuexis_behavior_tests | 1 | 通过 |
| cuexis_gameplay_tests | 1 | 通过 |
| cuexis_debug_tests | 2 | 通过 |
| cuexis_render_tests | 4 | 通过 |
| cuexis_render_opengl_tests | 4 | 通过 |
| cuexis_runtime_tests | 24 | 通过 |
| cuexis_world_tests | 4 | 通过 |

## 3. 架构验证通过项

| 检查项 | 状态 |
|---|---|
| PlaybackSession 公共头不含 `entt/` | 通过 |
| PlaybackSession 公共头不含 `SDL` | 通过 |
| PlaybackSession 公共头不含 `glad/GL/` | 通过 |
| PlaybackSession 公共头不含 `nlohmann/` | 通过 |
| PlaybackSession 公共头不暴露 `RuntimeSession` | 通过 |
| PlaybackSession 公共头不暴露 `World` | 通过 |
| `cuexis_playback` PUBLIC 依赖仅 `cuexis::core` | 通过 |
| `cuexis_playback` 不直接链接 `SDL3::SDL3` | 通过 |
| `cuexis_playback` 不直接链接 `glad::glad` | 通过 |
| 所有原有模块测试零退化 | 通过 |

## 4. SDK 门面接口验证

PlaybackSession 公共接口已验证可通过编译和基本功能测试：

| 操作 | 状态 |
|---|---|
| `loadChart(jsonText, limits)` — Chart 加载/编译/commit | 通过（Player smoke test 通过） |
| `extractFrame(scene)` — FrameSnapshot 提取 | 通过 |
| `update(RuntimeFrame)` — 帧更新（1B 占位） | 通过 |
| `seek(chartTimeMs)` — 时间跳转（1B 占位） | 通过 |
| `reload(jsonText)` — 谱面替换 | 通过 |
| `unload()` — 资源释放 | 通过 |
| `chartInfo()` — 谱面元数据 | 通过 |
| `diagnostics()` — 结构化诊断 | 通过 |

## 5. 已知限制与待完成项

| 项目 | 计划阶段 |
|---|---|
| PlaybackSession 独立单元测试 | 阶段 1C |
| ContentProvider 注入（IContentProvider 接口已定义） | 阶段 1E |
| extractFrame 返回包含 ChartObjectId 的对象映射 | 阶段 1C |
| update() 驱动行为求值（BehaviorSystem） | 阶段 1C |
| seek() 实际时间跳转 | 阶段 1C |
| CMake install/export/find_package | 阶段 1E |
| 外部 consumer 构建验证 | 阶段 1E |
| `cuexis_judgement` 判定模块 | 阶段 11 |
| 按键记录与回放 | 阶段 11 |

## 6. 结论

SDK 转型第一轮代码改造完成。`cuexis_playback` 模块作为宿主集成的唯一公共入口已建立，其公共头文件零泄漏（EnTT、SDL、OpenGL、nlohmann）。全部 153 项现有测试通过，零退化。Player 应用同时使用 PlaybackSession（SDK 门面路径）和 RuntimeSession（内部调试访问路径），验证了两种模式可共存。后续阶段按 SDK 转型方案的计划顺序推进。
