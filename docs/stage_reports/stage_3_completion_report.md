# Cuexis 阶段 3 验收报告

状态：3G 本地 Windows/WSL 验收完成；当前实现分支的 hosted Linux 发布门禁待关闭，阶段 3
尚未最终完成
报告日期：2026-08-08
完成版本：`26.07.18.18-1`（Debug：`26.07.18.18-1-dev`）
SDK preview API：`0.5.0`
FrameDigest algorithm：`3`

## 1. 验收结论

阶段 3A-3F 已交付 Portable Presentation Profile v1、候选与活动 manifest、拥有型资源获取、
`FrameSnapshot` portable refs、FrameDigest v3、公共 capability preflight、无 GPU Validation Sink、
OpenGL adapter、Player 真实 Mesh/Texture/Unlit 绘制，以及 Playback-only external consumer/package
闭包。

3G 已完成本地 Windows/MSVC GPU 与 package 矩阵，以及 WSL Ubuntu GCC/Clang、shared、sanitizer、
clang-tidy、coverage 和性能矩阵。Windows 与 Linux 本地结果对相同 fixture 产生一致的 Snapshot、
portable identity、FrameDigest 和 normalized command summary。

当前 `codex/stage-3` 工作树尚未提交或推送，分支没有 upstream，因此没有包含本次实现的 hosted
Linux run URL。旧计划分支或旧提交的 workflow 不能作为本次实现证据。本文证明 3G 本地验收完成，
不关闭阶段 3 完成定义中的 hosted Linux 发布门禁。

## 2. 已交付合同

- `CXPRES01` Portable v1 严格解析 Mesh、Texture2D 和固定 Unlit Material，并在分配或发布前执行
  version、type、offset、count、finite、index、dimension、依赖和 byte budget 校验。
- `PreparedPlayback` 提供 owning candidate manifest、candidate token 和 owning immutable resource
  acquisition；commit 后由活动 `PlaybackSession` 提供对应 active view。
- `FrameSnapshot` 是唯一公共权威帧。`ObjectSnapshot` 只携带稳定的 Mesh/Material ref，不暴露
  World、EnTT、ResourceManager、provider revision 或 GPU handle。
- Validation Sink 与 OpenGL adapter 使用同一规范化 Opaque/Transparent 顺序。透明帧 summary
  golden 为 `18316288860163381829`；Playback-only consumer 在 `625 ms` 的 FrameDigest v3 golden
  为 `8424169740673868033`。
- load/reload/capability/resource/upload 失败不发布半状态。旧 Playback、Validation candidate 和
  OpenGL active cache 保持可用；成功 reload 只在完整 candidate 准备后切换。
- warmed-up `PlaybackSession::update()`/`extractFrame()` 和 Validation Sink frame validation 由分配
  计数测试固定为零动态分配。

## 3. Windows/MSVC 矩阵

所有 Windows 配置使用 MSVC 19.51、Ninja 和 `x64-windows`。Windows symlink containment 测试按
既有平台条件跳过，没有失败。

| 配置 | Linkage | CTest | 结果 |
| --- | --- | ---: | --- |
| full Debug | STATIC | 272/272 | 通过 |
| full Release | STATIC | 272/272 | 通过 |
| headless Debug | STATIC | 239/239 | 通过 |
| headless Release | STATIC | 239/239 | 通过 |
| full Debug | SHARED | 275/275 | 通过 |
| full Release | SHARED | 275/275 | 通过 |
| headless Release | SHARED | 242/242 | 通过 |

3G 最终格式化后再次执行 standard Debug 与 Release fresh configure、clean build 和完整 CTest，
两者均为 `272/272`。七个 external consumer 模式覆盖基础、Playback-only、Core 和 AudioSDL 的
add_subdirectory/find_package；shared 配置额外验证 export surface、consumer imports、版本化 DLL、
配置/runtime 不匹配拒绝和 clean staging 运行。

## 4. Windows GPU 证据

Debug 与 Release 六帧 smoke 均使用：

```text
GPU: NVIDIA GeForce RTX 4060 Laptop GPU
Driver/OpenGL string: OpenGL 3.3.0 NVIDIA 596.36
Vendor: NVIDIA Corporation
```

真实像素与规范化结果：

- Opaque Unlit：`255,204,51,255`
- textured Transparent source-over：`51,32,71,192`
- invisible clear：`14,16,18,255`
- textured Transparent summary：`18316288860163381829`
- 失败 reload 与失败 adapter prepare 均保留旧 active cache；成功 reload 原子激活完整 cache。

本次格式化后复测的 `render_us`：

| 构建 | Frame 0 | Frame 1 | Frame 2 | Frame 3 | Frame 4 | Frame 5 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Debug | 1862.7 | 135837.7 | 462.8 | 480.3 | 552.3 | 472.2 |
| Release | 1774.4 | 133635.7 | 836.9 | 357.7 | 311.0 | 739.7 |

Frame 1 包含首次 Texture2D 上传/驱动准备，不代表 steady-state submit。成功 reload 后 Frame 4/5
验证完整 cache 的正常提交；这些墙钟数值只作趋势证据，不是跨机器硬阈值。

## 5. 本地 Linux 矩阵

WSL Ubuntu 使用 GCC 15.2.0、Clang/clang-tidy 21.1.8、gcovr 7.2 和 workflow 固定的 vcpkg commit
`40f3c709db80acf154ac4b17a1f83c564ebd022e`。Linux 验收源码镜像包含本次 Stage 3 实现；其后改动只
涉及文档和 Windows-only probe include 顺序。WSL 不是 GitHub-hosted runner。

| 配置 | CTest/目标 | 结果 |
| --- | ---: | --- |
| GCC headless Release | 237/237 | 通过 |
| GCC shared Release | 238/238 | 通过 |
| Clang shared Debug | 238/238 | 通过 |
| Clang ASan + UBSan | 237/237 | 通过，无 sanitizer 报告 |
| clang-tidy | `cuexis_playback` | 通过 |
| GCC coverage | 230/230 | 通过 |

gcovr 对 `engine/` 的结果为 lines `76.3%`、functions `91.1%`、branches `40.5%`，高于 workflow
冻结的 40% line baseline。GCC 首次构建发现并修复了 `ValidationCandidateResult` 缺少前置声明和
未使用测试辅助函数两个跨编译器问题；修复后的矩阵全部通过。

## 6. 性能与内存 probe

probe 使用最大合法 Texture2D：encoded `67108864` bytes、decoded `67108836` bytes、尺寸
`2799 x 5994`。测试目标为 `EXCLUDE_FROM_ALL`，必须显式构建；它不是 shipping SDK target。

| 指标 | Windows MSVC Release | WSL GCC shared Release |
| --- | ---: | ---: |
| prepare | 299097.600 us | 356811.060 us |
| manifest/acquisition | 1.200 us | 3.055 us |
| Validation candidate | 286746.600 us | 273515.596 us |
| warmed update/extract | 0.809 us/op | 0.542 us/op |
| warmed validate | 0.700 us/op | 0.598 us/op |
| reload prepare | 303266.600 us | 390432.196 us |
| reload peak delta | 134410240 bytes | 134479872 bytes |

Windows peak delta 使用 process peak working set，Linux 使用 `getrusage().ru_maxrss`。二者包含 allocator、
page commitment 和测量平台差异，只用于暴露数量级与回归趋势。确定性门禁仍由资源硬预算、零分配
测试和事务测试承担。

## 7. 静态、架构与 package 门禁

- `cuexis_format_check`：通过。
- `git diff --check`：通过。
- installed public header pure ASCII 扫描：通过。
- `cuexis_architecture_tests` 与 target dependency allowlist：通过。
- Playback-only clean install 不查找 SDL3、OpenGL 或 GLAD；显式 `COMPONENTS OpenGL` 稳定拒绝。
- static/shared export/import、package minimum version、generated version 和 component rejection：通过。
- 基础 package 与 AudioSDL 许可证/NOTICE 闭包：通过。
- public Playback headers 未暴露 EnTT、SDL、OpenGL/GLAD、JSON DOM、RuntimeSession 或 World 类型。

## 8. 开放门禁与残余风险

- 当前实现必须提交并推送后，由 `.github/workflows/linux-quality.yml` 在 hosted `ubuntu-latest` 上运行
  GCC Release、GCC shared Release、Clang shared Debug、Clang ASan+UBSan、clang-tidy、coverage 和
  package consumer。只有该 run URL 可关闭阶段 3 的跨平台发布门禁。
- GPU 证据只覆盖 Windows/NVIDIA/driver 596.36。AMD、Intel、Linux native OpenGL、macOS、Android
  和其他驱动仍未取得同等级像素与 reload 证据。
- 本地 Linux 是 WSL x86-64 小端环境，没有覆盖原生发行版差异、非 x86 架构或真实大端平台。
- Stage 3 仍是 matching-toolchain C++ preview，不承诺稳定 C ABI；稳定 C ABI 延后到阶段 12。
- OpenGL adapter 仍是仓库内 Player target，不是安装 component；外部宿主以公共 Portable v1 合同
  实现自己的 adapter。

## 9. 最终关闭条件

阶段 3A-3F 和 3G 本地工作已经完成。待当前实现分支获得可引用且全部通过的 hosted Linux run URL
后，才能把阶段 2 的遗留 Linux 前置和阶段 3 完成定义第 8 项同时关闭，并把阶段 3 状态改为最终完成。
