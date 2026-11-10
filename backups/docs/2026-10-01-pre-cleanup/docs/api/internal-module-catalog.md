# 内部模块速查

状态：active

更新日期：2026-08-30

适用版本：SDK API `0.7.0`

文档角色：内部技术参考，不是宿主 API

权威边界：[Module Boundaries](../architecture/MODULE_BOUNDARIES.md)

## 快速结论

- 外部宿主从 `playback` 开始，不直接接入 Runtime、World 或 adapter。
- 格式解析归 `chart` / `cxc`，动画运行时不得解析 JSON、CXC 或 CXT。
- OpenGL 代码只能存在于 `render_opengl`。
- SDL 依赖只能位于平台或可选 adapter 层。
- Playback 热路径不依赖 shader compiler。

## 按任务定位模块

| 任务 | 首选模块 | 不应放入 |
| --- | --- | --- |
| 公共播放生命周期 | `playback` | `runtime` 私有入口、Player 专用路径 |
| Chart/CXT 解析与 canonical identity | `chart` | `animation`、`runtime` |
| CXC package/manifest | `cxc` | 公共独立 package SDK |
| 资源读取与缓存 | `content`、`assets`、`project` | renderer backend |
| 动画编译、采样与混合 | `animation` | JSON/CXC/CXT parser |
| World 实例化与帧求值 | `runtime`、`world` | public Playback header |
| 时钟与音频传输 | `audio` | SDL backend 细节 |
| SDL 音频实现 | `audio_sdl` | `audio` core |
| backend-neutral render scene | `render` | OpenGL 调用 |
| OpenGL adapter | `render_opengl` | `playback`、`runtime` |
| offline shader 编译与缓存 | `shader` | Playback 每帧路径 |
| 参考宿主流程 | `app/player` | private Runtime path |

## 模块边界速查

| 模块 | 主要责任 | 禁止依赖或泄露 |
| --- | --- | --- |
| `core` | Result、错误、诊断、数学、基础所有权 | SDL、OpenGL、平台头 |
| `chart` | Reader/Writer、TimingMap、迁移、语义 identity | EnTT、SDL、GL、World、Audio |
| `cxc` | strict ZIP32 Stored package 与 closure | 公共 package ABI |
| `animation` | typed program 编译和采样 | JSON、CXC、CXT 解析 |
| `runtime` | internal session 与求值编排 | SDL、GL、Audio、public host API |
| `playback` | prepare/commit、source、snapshot、presentation | SDL、AudioSDL、OpenGL adapter |
| `render_opengl` | OpenGL backend 与 GPU 对象 | 向公共头泄露 GL 类型 |
| `shader` | 可选 offline compiler/cache | Playback 热路径依赖 |

## 查阅顺序

1. 先查看 [API 导航](README.md)，确认问题是否属于公共合同。
2. 格式问题查看 [formats/README.md](../formats/README.md)。
3. 产品或取舍问题查看对应 ADR。
4. 只有在修复实现、验证未记录行为或更新 API 文档时才进入源码。

运行时脚本和逐帧 script callback 无限期延后。任何内部模块都不得为其添加字段、capability、bytecode、
ABI 或 Playback hook。
