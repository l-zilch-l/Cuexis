# Cuexis 阶段 1E 完成报告

状态：实现与 Windows/MSVC、Windows/MinGW、Linux GCC/Clang 自动化验收完成
报告日期：2026-07-29
完成版本：`26.07.18.18-1`（Debug：`26.07.18.18-1-dev`）
SDK preview API：`0.3.0`

## 1. 完成结论

阶段 1E 已把 Playback Core 从仓库内静态库整理为可由仓库外 C++20 工程消费的
static/shared preview。宿主通过 `PlaybackSource`、`PlaybackSession`、`RuntimeFrame`、
`FrameSnapshot`、ContentProvider 和 Audio contracts 工作，不再构造 `AssetDatabase`、
`ResourceManager`、`RuntimeSession` 或 World。

shared 支持严格限定为匹配 SDK minor、编译器版本、标准库、架构、配置和运行时的重编译消费。
它不是稳定 C ABI，也不允许跨工具链替换 binary。稳定 C ABI、正式 Judgement/Replay 和语言绑定
仍保留到阶段 11/12。

## 2. 公共 API 与事务边界

- `PlaybackSource::fromChartText` 提供无资源 Chart source。
- `PlaybackSource::fromTypedProject` 组合 typed logical index 与 Cuexis-owned ContentProvider。
- `PlaybackSource::fromFilesystemProject` 保留 ProjectConfig、Asset Index、路径 containment 和预算。
- `PlaybackSession` 的 load/reload/prepare 路径只接受 Playback-owned source；公共
  `AssetDatabase` 构造入口已移除。
- `PreparedPlayback` 持有候选 provider、ResourceManager 和 RuntimeSession；只有 `commit()`
  替换活动内容，失败 prepare/reload 保留上一有效会话。
- `FrameSnapshot` 继续是拥有型值对象，Session update/reload/unload/销毁不使旧 Snapshot 悬空。

公共 `FrameDigest` algorithm version 1 使用 FNV-1a 64、little-endian 整数编码、`-0`
归一化和有限浮点校验。外部 consumer golden digest 为 `6726938620466257503`，static/shared、
internal headless 和 Player diagnostics 共用同一实现。

## 3. Binary 与 Package

`CUEXIS_LIBRARY_TYPE=STATIC|SHARED` 是唯一 linkage 选择，默认 `STATIC`；同一 build tree/install
prefix 不支持混装。公共安装 target 固定为：

```text
Cuexis::Core
Cuexis::Content
Cuexis::Audio
Cuexis::Playback
Cuexis::AudioSDL（可选）
```

shared 只生成上述公共 binary。Windows 文件名使用 `cuexis_<module>-0.3.dll`，Debug 使用
`cuexis_<module>-0.3d.dll`；Linux 使用相同 stem、Debug postfix 和 `SOVERSION 0.3`。内部
Filesystem/JSON/Project/Assets/Chart/Behavior/Gameplay/Render/Runtime/World 以 PIC static
implementation library 吸收，不生成受支持 DLL 或 package component。

static package 可以安装完成链接所需的内部 archive 和 implementation target，但不安装内部模块
头。static/shared 安装公共头 allowlist 均只包含 Core、Content、Audio、Playback、可选
AudioSDL 和生成的 version/export headers。

shared package config 只传播公共 `tl-expected` 头依赖；基础包不查找 EnTT、GLM、JSON/schema
validator、SDL3、glad 或 spdlog。只有显式请求 AudioSDL 才查找 SDL3。配置会验证 compiler ID
和精确版本、pointer size、system/processor、Debug/Release，并在 MSVC 上拒绝 `/MT`。

## 4. 自动化门禁

external consumer 同时覆盖 add_subdirectory 与 install/find_package、基础包与 AudioSDL、
STATIC 与 SHARED。find_package consumer 从独立 staging install 运行；shared runtime 只从该
staging closure 部署。门禁额外验证：

- 安装树不存在内部 headers、shared internal targets 或 internal DLL/shared objects。
- 基础包不存在 AudioSDL/SDL artifact，Playback consumer 不导入 SDL3。
- Playback shared export 含 PlaybackSession、PlaybackSource、RuntimeTimeline 和 FrameDigest，
  不含 RuntimeSession、AssetDatabase、World 或 EnTT symbols。
- Windows consumer import table 指向版本化 Core/Content/Playback DLL。
- Debug/Release 与 `/MD`/`/MDd` 不匹配 consumer 在 configure 时稳定失败。
- 全部安装公共头为纯 ASCII。

## 5. Windows/MSVC 验收结果

| 配置 | Linkage | CTest |
| --- | --- | ---: |
| full Debug | STATIC | 226/226 |
| full Debug | SHARED | 228/228 |
| headless Debug | STATIC | 195/195 |
| headless Debug | SHARED | 197/197 |
| full Release `/WX` | STATIC | 226/226 |
| full Release `/WX` | SHARED | 228/228 |
| headless Release `/WX` | STATIC | 195/195 |
| headless Release `/WX` | SHARED | 197/197 |

Windows 不支持创建符号链接的 filesystem case 按既有条件跳过。一次 Release 并行链接遇到
vcpkg applocal 文件锁，增量重试通过；没有代码诊断被忽略。shared Release 暴露的 C4251 仅在
明确的 matching-toolchain preview 类型周围使用局部 warning push/pop，没有全局禁用。

## 6. 跨平台 CI 验收

`linux-quality.yml` 已增加 GCC `headless-shared-release` 与 Clang
`headless-shared-debug`，并复用相同 build/install/external-consumer、ELF export 和 clean staging
部署门禁。GitHub Actions run `30424864440` 已通过 GCC Release、GCC shared Release、Clang
shared Debug、Clang ASan+UBSan、clang-tidy 与 GCC coverage 全部作业。相同修复提交的 Windows
MinGW Debug/Release run `30424864443` 与 Windows MSVC Debug/Release run `30424864492` 也已通过。
阶段 1E 所要求的 Windows/Linux static/shared 自动化验收证据已经齐备。

## 7. 非目标与交接

阶段 1E 不承诺无需重编译的 binary compatibility，不支持同 prefix 混装 linkage、任意 C++
继承插件 ABI、LoadLibrary/dlopen 插件生命周期、异步 ContentProvider、正式语言绑定或稳定 C
ABI。阶段 2 及之后的表现能力必须继续通过 external consumer 与 FrameDigest parity；阶段 11
完成 Input/Judgement/Replay 后，阶段 12 才能冻结稳定 C ABI。
