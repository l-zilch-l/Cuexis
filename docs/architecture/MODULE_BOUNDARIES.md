# Cuexis Module Boundaries

状态：现行模块边界摘要

更新日期：2026-08-12

本文总结稳定依赖方向。构建时的精确 allowlist 和 architecture tests 仍由根 CMake 配置拥有。

## 主要模块

| 模块 | 职责 | 不应依赖或暴露 |
| --- | --- | --- |
| `cuexis_core` | Result、Error、Diagnostics、数学和基础值 | SDL、OpenGL、平台头 |
| `cuexis_json_support` | JSON DOM 隔离和 typed reader 支持 | 向其他公共 API 泄漏 JSON 类型 |
| `cuexis_project` | ProjectConfig、路径和项目准备 | World、Render、Audio backend |
| `cuexis_assets` | AssetDatabase、ResourceManager、Handle/Lease/Scope | OpenGL、Chart JSON |
| `cuexis_chart` | Chart/CXT typed model、校验、迁移和编译 | EnTT、World、SDL、OpenGL |
| `cuexis_behavior` | Behavior 数据和采样 | SDL、OpenGL、JSON |
| `cuexis_world` | EnTT World、Entity 和空间组件 | Chart 文档、SDL、OpenGL |
| `cuexis_runtime` | RuntimeSession、实例化和系统编排 | SDL、OpenGL、Audio backend |
| `cuexis_render` | 后端无关表现值和提取 | OpenGL calls |
| `cuexis_render_opengl` | OpenGL adapter | Playback 公共门面反向依赖 |
| `cuexis_audio` | 后端无关 Transport/Clock | SDL |
| `cuexis_audio_sdl` | SDL Audio adapter | Runtime/Chart 反向依赖 |
| `cuexis_playback` | 宿主公共门面和 Prepared transaction | 暴露 World、RuntimeSession、EnTT |

## 依赖原则

```text
Chart -> typed data only
World -> runtime entity state only
Runtime -> composes Chart + World + front-end systems
Playback -> public facade over internal Runtime and resources
Adapters -> consume public/portable values
Player and Studio -> compose Playback and optional adapters
```

Platform、AudioSDL 和 OpenGL adapter 都是可选叶子模块。Headless Playback 不要求 SDL、OpenGL、
窗口或物理音频设备。

## JSON 和第三方类型

nlohmann JSON 类型只能存在于 `engine/json_support/`。GLM 不得出现在安装公共头。除
`tl::expected` 通过 `cuexis::core::Result` 暴露外，第三方实现类型不能进入 Cuexis 公共 API。

## 格式阶段边界

Stage Chart Format Update 可以在 Chart/Playback prepare 边界解析和 lowering Chart v4/CXT，但：

```text
engine/animation does not parse JSON/CXC/CXT
CXC archive library types do not enter public Playback headers
format handlers do not create World/EnTT entities
pack and prepare do not execute scripts
```

ADR 0038 已把 CXC archive/manifest/closure 放入内部 target `cuexis_cxc`；CFU-C3 已创建该 target
和 owning package/content-domain 基线，并遵守：

```text
cuexis_cxc may depend on core/content/filesystem/project/chart/json support and an archive library
cuexis_cxc does not depend on playback/runtime/world/render/audio
playback may privately depend on cuexis_cxc
cxc tools reuse cuexis_cxc directly
cuexis_cxc is not an installed package component
archive library types never enter Cuexis public headers
```

入口 Chart/CXT 属于 PlaybackSource 的 project-document table；AssetId bytes 继续通过
`IContentProvider`。两种内容域不得通过保留 root ID 或伪 AssetId 混用。

## 线程和所有权

- PlaybackSession 由 owner thread 控制。
- Prepared candidate 绑定创建它的 Session/generation，跨 Session 或 stale commit 失败。
- Worker 只生成 CPU/Prepared 数据，不访问 EnTT、图形 Context、SDL Window 或实时音频流。
- Render/Audio 实时路径不读取可变 ChartDocument，也不执行文件 I/O 或格式化日志。

精确编码和线程规则见 [CODE_POLICY.md](../guides/CODE_POLICY.md)。
