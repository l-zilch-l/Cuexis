# Cuexis 技术开发文档

## 0. 文档状态

本文档是项目的持续维护指南，同时记录已落地的工程状态、已确认的架构边界和后续阶段路线。阶段 0、阶段 1A、阶段 1B、阶段 1C、阶段 1D 与阶段 1E 已完成验收；阶段 2 实现和 Windows/MSVC 非图形门禁已完成，最终阶段验收待 GPU smoke 与 hosted Linux CI。260722 全量审查的 R01-R21 已于 2026-07-26/27 全部关闭并补齐构建、CTest、架构和 external consumer 证据。ADR 0027 已将长期产品方向调整为可嵌入 Cuexis Playback SDK + 独立 Player + 独立 Studio。

文档中的内容按以下方式理解：

```text
已实现：当前仓库已有对应代码、构建入口或测试入口
已决策：已经确认，后续实现应遵循
规划中：方向已经明确，但接口和数据格式仍需设计
待讨论：不得据此建立难以修改的公共接口
```

随着讨论推进，待讨论内容应转化为明确决策；重大技术决策还应同步形成 ADR。

当前仓库已完成阶段 0 工程闭环、阶段 1A 规范谱面与实例化闭环、阶段 1B 资源生命周期闭环、阶段 1C typed Behavior 与 headless Playback、阶段 1D 主音乐与 Audio，以及阶段 1E 可安装 static/shared Playback Core。阶段 2 已实现 Chart v3、Tempo/Stop TimingMap、Behavior/Step Event、Visibility/Material Snapshot、capability preflight、调试快照、FrameDigest v2 和 v1/v2 显式迁移工具；Windows/MSVC static/shared Debug/Release 与 headless 门禁已通过。阶段 2 的 GPU smoke 与 hosted Linux CI 仍待最终验收。正式 Judgement/Replay、Studio 和稳定 C ABI 尚未完成。

当前状态的权威顺序固定为：本文第 0、30、32 节记录产品与阶段状态；`docs/BUILDING.md` 记录当前可执行构建和安装入口；最新阶段审查/补充报告记录尚未关闭的问题。完成报告和早期验证报告是带日期的历史证据，若与后续审查冲突，以后续审查与本节当前状态为准，不得从历史报告反推当前验收状态。

文档职责：

```text
PROJECT_GUIDE.md  项目目标、模块边界、已确认规则和阶段计划
docs/adr/          已接受或拟议的重大技术决策及其背景
BUILDING.md       Windows/MSVC 标准构建、测试和排错流程
CHART_FORMAT.md   方案 A 规范谱面格式与最小 Schema
SIMPLE_CHART_FORMAT.md 已退役的 Simple Chart 历史格式说明
VERSIONING.md     版本来源、更新和一致性校验规范
RUNTIME_SESSION.md RuntimeSession 事务生命周期与错误恢复
TIMING_MODEL.md   BPM、Stop、offset 与时间域语义
ANIMATION_MIXING.md 动画 Layer 与 Gameplay/Studio Override
PARTICLE_TIMELINE.md 粒子确定性、Checkpoint 与 Seek
SHADER_PIPELINE.md 跨 OpenGL/OpenGL ES/Vulkan 的 Shader 管线
MOBILE_STRATEGY.md Android 资源、生命周期与输入延迟
DEPENDENCY_POLICY.md Apache-2.0 与第三方依赖政策
CODE_POLICY.md    编码、Result/Error、所有权与线程规则
PROJECT_REVIEW.md 当前文档各部分的合理性与风险评估
stage_plans/cuexis_sdk_transition_plan.md SDK 产品边界、阶段迁移与完成标准
stage_plans/stage_2_implementation_plan.md Behavior Event、TimingMap 与阶段 2 实施门禁
```

当专项文档建立后，本文只保留架构结论和链接，避免同一规则在多个文件中重复维护。

## 1. 项目概述

`Cuexis` 是一个基于 **C++20、CMake 和版本化数据格式** 的可嵌入谱面处理与播放 SDK。SDL3、OpenGL 和 EnTT 是当前实现或可选适配器依赖，不是宿主必须采用的公共产品边界。

项目定位为免费开源项目，维护目标不以商业盈利为导向。项目采用 Apache License 2.0；许可证允许下游商业使用，这与维护者自身不以盈利为目标并不冲突。通用能力优先复用成熟开源库，把开发资源集中在 Cuexis 的谱面、时间、行为和运行时模型上。许可证与依赖规则见 `docs/DEPENDENCY_POLICY.md`。

项目长期目标是让其他游戏引擎、应用和工具安全、确定地加载、播放和查询 Cuexis 谱面及资源，并获得后端无关的表现帧、判定计分结果和可复现回放。Cuexis Player 是独立参考播放器；Cuexis Studio 是独立编辑程序；两者使用同一 Playback SDK。

`Cuexis` 不以传统音游中的轨道、判定线、音符类型作为底层固定模型，而是将谱面对象抽象为统一的 Entity，并通过 Component、Behavior、Material、Timeline、Template 等数据驱动机制组合出不同玩法表现。

推荐命名体系：

```text
Cuexis              项目总名
Cuexis Playback SDK 可嵌入的谱面处理、播放、判定与回放核心
Cuexis Player       使用 SDK 的独立参考播放器
Cuexis Studio       使用 SDK 预览的独立编辑器
Cuexis internals    Runtime、World、Assets 与各内部前端模块
cuexis::            C++ 命名空间
```

## 2. 项目目标

核心目标：

```text
建立长期可维护、可安装和可嵌入的 C++ Playback SDK
内部使用 EnTT 管理运行时 Entity，但不向宿主暴露
使用数据驱动方式描述谱面、行为、材质和动画
支持 3D Transform、父子绑定、时间轴驱动
支持音符、元素、轨道、判定线、装饰物的统一建模
输出宿主可消费的 FrameSnapshot/RenderPacket，并提供可选 OpenGL 后端
支持标准 InputEvent、确定性判定计分、输入记录和回放
支持 Cuexis Player、Cuexis Studio 和外部宿主共享 Playback 核心
支持后续扩展 Entity 动画、粒子系统、Shader 编辑器
```

第一阶段目标：

```text
不接管宿主原始输入设备、主循环或游戏状态
阶段 1A 的 NullInput/NullJudge 继续作为历史占位
优先完成 PlaybackSession、headless 时间驱动、帧输出和外部消费闭环
正式判定计分与 ReplayData 在阶段 11 由必选 cuexis_judgement 交付
```

当前首要交付范围是关闭阶段 2 的 GPU/hosted CI 最终验收，并在不改变已冻结 Stage 2 格式语义的前提下进入阶段 3 表现前端规划。Cuexis Studio 的独立应用边界继续预留，但不阻塞 Playback SDK 外部消费。

## 3. 非目标

当前阶段不追求：

```text
通用商业游戏引擎和场景/玩法框架
宿主原始输入设备管理、游戏状态和判定 UI
复杂 Shader Graph
正式 Vulkan 后端
完整移动端发布
联网、账号、排行榜
复杂物理系统
大型资源打包系统
```

判定、计分、输入记录与确定性回放本身属于 Playback SDK 阶段 11 交付；完整玩法状态机、UI、排行榜和通用游戏流程仍由宿主负责。其他非目标不能阻塞 SDK 核心闭环。

## 4. 技术栈

基础技术栈：

```text
语言：C++20，后续可评估 C++23
构建系统：CMake
包管理：vcpkg
可选参考平台 adapter：SDL3
内部 ECS：EnTT，不进入 Playback 公共边界
可选内建渲染 adapter：OpenGL，未来可验证 Vulkan
独立 Studio UI：Dear ImGui（规划）
数据格式：JSON，后续可增加二进制缓存
音频前端：cuexis_audio
首个音频后端：SDL3 Audio Device + SDL_AudioStream
单元测试：Catch2 v3
测试运行与发现：CTest
```

当前 manifest 的直接依赖：

```text
SDL3
EnTT
glm
nlohmann-json
json-schema-validator
spdlog
fmt
glad
Catch2（仅测试 target）
tl-expected（C++20 Result 基础实现）
```

GLM 只在 Cuexis 自有数学类型的私有实现中使用；nlohmann-json 与 json-schema-validator 只由 `cuexis_json_support` 的实现边界消费，公共接口暴露 Cuexis 自有 JSON 值、Reader 和诊断类型。ImGui、stb 等仍是后续阶段候选依赖，引入时须按依赖政策重新评估并记录。

依赖原则：

```text
第三方库统一通过 vcpkg 管理
禁止把第三方库源码散落在工程目录中
新增依赖必须说明用途
优先采用成熟、持续维护且许可证兼容的开源库，避免重复实现通用系统
依赖收益必须覆盖构建时间、体积、升级、许可证和退出成本
核心模块尽量减少第三方库外泄
```

## 5. 版本规范

`Cuexis` 使用时间型版本号：

```text
yy.mm.dd.hh-v
```

含义：

```text
yy  两位年份
mm  两位月份
dd  两位日期
hh  两位小时，24 小时制
v   编译版本
```

示例：

```text
26.07.18.18-1
26.07.14.18-3
27.01.03.22-4
```

版本时间使用 UTC，避免多人协作时因时区导致版本顺序混乱。同一 UTC 小时内的编译版本 `v` 由维护者人工递增，从 `1` 开始。

构建类型后缀：

```text
正式构建：yy.mm.dd.hh-v
开发构建：yy.mm.dd.hh-v-dev
测试构建：yy.mm.dd.hh-v-test
内部构建：yy.mm.dd.hh-v-internal
实验构建：yy.mm.dd.hh-v-exp.name
```

示例：

```text
26.07.18.18-1-dev
26.07.14.18-2-test
26.07.18.18-1-exp.vulkan
```

版本信息必须写入：

```text
CMake project version（仅四段数字）
应用窗口标题
程序启动日志
错误报告 / 崩溃报告
构建产物名称，若适用
vcpkg.json 的 version-string（不带构建类型后缀的规范版本）
```

CMake 的 `project(VERSION ...)` 只保存四段数字版本，不包含编译版本和构建类型后缀：

```cmake
project(Cuexis VERSION 26.7.18.18 LANGUAGES CXX)
```

完整的 Cuexis 显示版本由四段 CMake 版本、人工维护的编译版本 `v` 和可选构建类型后缀组合，例如：

```text
CMake 项目版本：26.7.18.18
完整正式版本：26.07.18.18-1
完整开发版本：26.07.18.18-1-dev
```

不得把 `26.07.18.18-1` 直接传给 CMake 的 `project(VERSION ...)`。构建配置应将完整版本写入生成文件，供窗口标题、日志和构建产物使用。

版本各表示形式必须来自同一份受控版本配置；不得在多个 CMakeLists 中分别手工维护。`vcpkg.json` 记录 `26.07.18.18-1` 这类规范版本，因为同一份源码可以同时生成 Debug 和 Release；`-dev`、`-test` 等后缀只在具体构建配置和生成头文件中出现。版本测试必须检查四段数字、人工编译号和 manifest 版本是否一致。

推荐生成：

```text
${binaryDir}/generated/cuexis/version.hpp
```

生成头文件属于构建产物，不提交到源码目录。消费 target 通过构建目录的 include path 使用 `<cuexis/version.hpp>`。

内容示例：

```cpp
#pragma once

#define CUEXIS_VERSION_STRING "26.07.18.18-1"
#define CUEXIS_VERSION_BUILD 1
#define CUEXIS_VERSION_YEAR 26
#define CUEXIS_VERSION_MONTH 7
#define CUEXIS_VERSION_DAY 18
#define CUEXIS_VERSION_HOUR 18
```

## 6. 工程结构

推荐目录结构：

```text
Cuexis/
  CMakeLists.txt
  CMakePresets.json
  vcpkg.json
  vcpkg-configuration.json
  LICENSE
  NOTICE
  THIRD_PARTY_NOTICES.md

  app/
    player/
    studio/

  sdk/
    playback/

  adapters/
    content_filesystem/
    host/

  engine/
    core/
    json_support/
    project/
    platform/
    world/
    assets/
    chart/
    runtime/
    behavior/
    animation/
    particles/
    render/
    render_opengl/
    audio/
    audio_sdl/
    gameplay/
    judgement/
    debug/

  tools/
    asset_importer/
    chart_validator/

  assets/
    shaders/
    textures/
    meshes/
    charts/

  docs/
    adr/
    PROJECT_GUIDE.md
    PROJECT_REVIEW.md
    BUILDING.md
    CHART_FORMAT.md
    SIMPLE_CHART_FORMAT.md
    RUNTIME_SESSION.md
    TIMING_MODEL.md
    ANIMATION_MIXING.md
    PARTICLE_TIMELINE.md
    SHADER_PIPELINE.md
    MOBILE_STRATEGY.md
    DEPENDENCY_POLICY.md
    CODE_POLICY.md
    VERSIONING.md

  tests/
    core/
    json_support/
    project/
    platform/
    world/
    assets/
    chart/
    behavior/
    gameplay/
    animation/
    particles/
    runtime/
    render/
    render_opengl/
    debug/
    audio/
    playback/
    judgement/
    external_consumer/
```

应用命名：

```text
Cuexis Playback SDK：宿主、Player 与 Studio 共享的公共播放门面
Cuexis Player：独立参考播放器
Cuexis Studio：独立编辑器
Cuexis internals：不直接向宿主公开的模块实现
```

命名空间规范：

```cpp
namespace cuexis {
}

namespace cuexis::core {
}

namespace cuexis::render {
}

namespace cuexis::chart {
}

namespace cuexis::runtime {
}
```

## 7. CMake 规范

最低版本建议：

```cmake
cmake_minimum_required(VERSION 3.25)
```

推荐 target 拆分：

```text
cuexis_core
cuexis_json_support
cuexis_project
cuexis_playback
cuexis_platform_sdl
cuexis_content_filesystem
cuexis_world
cuexis_assets
cuexis_chart
cuexis_runtime
cuexis_behavior
cuexis_animation
cuexis_particles
cuexis_render
cuexis_render_opengl
cuexis_audio
cuexis_audio_sdl
cuexis_gameplay（阶段 1A 历史占位，不作为 Playback SDK 公共接口）
cuexis_judgement
cuexis_debug
cuexis_player
cuexis_studio
```

当前激活 target 的准确清单以 `docs/BUILDING.md` 和根 CMake 为准。阶段 1C 后新增了 `cuexis_filesystem` 与 `cuexis_content`；`cuexis_playback` 已提供 source-based load/update/extractFrame/reload/unload、FrameDigest、static/shared 安装包和两种 external consumer 门禁。ADR 0033 的 C++ shared preview、完整组件矩阵及部署/兼容门禁已经实现。`cuexis_judgement` 尚未交付；其余名称表示后续规划边界。

CMake 规则：

```text
禁止全局 include_directories
禁止全局 link_libraries
禁止把所有源码塞进一个 target
每个模块必须明确 public/private 依赖
第三方依赖必须通过 find_package 引入
构建目录必须 out-of-source
模块依赖方向必须体现在 target_link_libraries 中
顶层/子项目默认值必须区分，作为依赖时不自动构建 App、测试和 demo 资产
阶段 1E 后公共组件必须支持 install/export 和 find_package(Cuexis)
```

示例：

```cmake
project(Cuexis VERSION 26.7.18.18 LANGUAGES CXX)

set(CUEXIS_VERSION_BUILD 1 CACHE STRING "Build number within the UTC hour")
set(CUEXIS_VERSION_SUFFIX "dev" CACHE STRING "Optional build type suffix")

add_library(cuexis_core STATIC)

target_sources(cuexis_core
    PRIVATE
        src/log.cpp
        src/time.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/cuexis/core/log.hpp
            include/cuexis/core/time.hpp
)

target_compile_features(cuexis_core PUBLIC cxx_std_20)

# 完整版本字符串应由 CMake 生成到 generated/version.hpp，
# 不直接使用 PROJECT_VERSION 代替。
```

## 8. CMake Presets

项目必须提供 `CMakePresets.json`，统一构建方式。

推荐构建命令：

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --no-tests=error
```

推荐基础 presets：

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "base",
      "hidden": true,
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/out/build/${presetName}",
      "cacheVariables": {
        "BUILD_TESTING": "ON",
        "CMAKE_CXX_STANDARD": "20",
        "CMAKE_CXX_EXTENSIONS": "OFF",
        "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
        "CUEXIS_WARNINGS_AS_ERRORS": "OFF",
        "VCPKG_TARGET_TRIPLET": "x64-windows"
      }
    },
    {
      "name": "debug",
      "displayName": "Debug",
      "inherits": "base",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CUEXIS_VERSION_SUFFIX": "dev"
      }
    },
    {
      "name": "release",
      "displayName": "Release",
      "inherits": "base",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CUEXIS_VERSION_SUFFIX": ""
      }
    }
  ],
  "buildPresets": [
    {
      "name": "debug",
      "configurePreset": "debug"
    },
    {
      "name": "release",
      "configurePreset": "release"
    }
  ],
  "testPresets": [
    {
      "name": "debug",
      "configurePreset": "debug",
      "output": {
        "outputOnFailure": true
      },
      "execution": {
        "noTestsAction": "error"
      }
    },
    {
      "name": "release",
      "configurePreset": "release",
      "output": {
        "outputOnFailure": true
      },
      "execution": {
        "noTestsAction": "error"
      }
    }
  ]
}
```

## 9. vcpkg 规范

项目使用 vcpkg manifest mode。

当前仓库已经使用正式的 manifest mode，并在 `vcpkg-configuration.json` 中固定 builtin registry baseline。工具链通过 `VCPKG_ROOT` 定位，项目配置不包含开发机绝对路径。

根目录必须包含：

```text
vcpkg.json
vcpkg-configuration.json
```

当前阶段 1A manifest 的直接依赖结构为：

```json
{
  "name": "cuexis",
  "version-string": "<由统一版本文件生成的当前版本>",
  "dependencies": [
    "sdl3",
    "entt",
    "glm",
    "json-schema-validator",
    "nlohmann-json",
    "spdlog",
    "fmt",
    "glad",
    "catch2",
    "tl-expected"
  ]
}
```

建议固定 vcpkg baseline，保证不同开发环境构建结果一致。

首阶段通过 `VCPKG_ROOT` 定位工具链，并以 Windows/MSVC 对应的 triplet 作为受支持配置。其他平台和工具链在首阶段不作为验收要求。

依赖规则：

```text
新增依赖必须写入文档或 ADR
新增依赖必须记录版本、用途、许可证、上游地址和替代方案
免费或非盈利不免除 GPL、LGPL、MPL、NOTICE 等许可证义务
优先选择 MIT、BSD、Apache-2.0、zlib 等兼容性清晰的许可证
发布时必须维护 THIRD_PARTY_NOTICES 或等价依赖清单
不能绕过 vcpkg 手动复制库文件
不能让第三方库类型污染核心接口，除非该库是明确的基础依赖
平台层和渲染后端可以依赖 SDL/OpenGL
core/chart/behavior/world 不应依赖 SDL/OpenGL
```

依赖规模不是单独的否决理由。评估重点是它是否显著降低实现与长期维护成本，以及许可证、构建时间、二进制体积、安全更新和未来替换成本是否可接受。

## 10. 模块依赖规则

允许依赖方向：

```text
cuexis_player -> cuexis_playback + optional filesystem/SDL/OpenGL adapters
cuexis_studio -> cuexis_playback + editor modules + optional adapters
external host -> cuexis_playback + selected host adapters

cuexis_json_support -> cuexis_core
cuexis_project -> cuexis_core + cuexis_json_support（JSON 实现私有依赖）
cuexis_chart -> cuexis_core + cuexis_json_support（JSON 实现私有依赖）
cuexis_assets -> cuexis_core
cuexis_playback -> cuexis_core + cuexis_project + cuexis_chart + cuexis_assets + cuexis_runtime + cuexis_render + cuexis_judgement（按阶段只链接真实消费者）
cuexis_runtime -> cuexis_core + cuexis_chart + cuexis_world + cuexis_assets + cuexis_behavior + cuexis_animation + cuexis_particles + cuexis_render
cuexis_behavior -> cuexis_core + cuexis_world
cuexis_animation -> cuexis_core + cuexis_world
cuexis_particles -> cuexis_core + cuexis_world + cuexis_render
cuexis_world -> cuexis_core
cuexis_render -> cuexis_core + cuexis_assets
cuexis_render_opengl -> cuexis_render + cuexis_platform_sdl
cuexis_platform_sdl -> cuexis_core
cuexis_audio -> cuexis_core
cuexis_audio_sdl -> cuexis_audio + SDL3
cuexis_judgement -> cuexis_core + cuexis_chart（具体依赖在阶段 11 冻结，不依赖 World/后端）
cuexis_gameplay -> cuexis_core + cuexis_world（阶段 1A 历史占位，不作为未来完整玩法框架）
cuexis_debug -> cuexis_core + cuexis_world + cuexis_render
```

以上是允许的依赖方向，不表示 target 必须从建立之初链接列出的所有模块。`cuexis_runtime` 应只链接当前确实使用的前端模块；例如尚未进入阶段 1 的 Runtime 不得为了未来能力提前要求尚未实现的 Particle 模块。

禁止依赖：

```text
cuexis_core 依赖 SDL、OpenGL、ImGui
cuexis_chart 依赖 EnTT、cuexis_world、SDL、OpenGL
cuexis_world 依赖 cuexis_chart
cuexis_runtime 依赖 cuexis_platform_sdl 或 cuexis_render_opengl
cuexis_playback 依赖 cuexis_platform_sdl、cuexis_audio_sdl、cuexis_render_opengl 或特定宿主 SDK
cuexis_audio 依赖 SDL3 或暴露 SDL 类型
cuexis_judgement 依赖 SDL、音频/渲染后端或宿主引擎 SDK
cuexis_behavior 直接读取 SDL 输入
cuexis_world 直接调用 OpenGL
Component 保存 GLuint、SDL_Window、SDL_Texture 等后端对象
EditorDocument 直接等同于 Runtime Entity
安装后的 Playback 公共头暴露 EnTT、SDL、OpenGL、JSON DOM 或其他实现类型
```

核心原则：

```text
Core 不知道平台和渲染后端
World 不知道 OpenGL
Chart 完全不知道 EnTT、World 和渲染实现
Runtime 只依赖引擎前端模块，不依赖平台层和具体渲染后端
Playback 只编排前端模块和宿主契约，不依赖可选具体 adapter
Audio 前端不知道 SDL，SDL 音频实现只存在于 cuexis_audio_sdl
Render 前端不知道 SDL 事件
OpenGL 只存在于 cuexis_render_opengl
宿主输入经标准 InputEvent 进入 Judgement，不把宿主事件类型带入 SDK
```

## 11. 总体架构

推荐架构：

```text
Host / App
  External Engine or Project
  Cuexis Player
  Cuexis Studio

Playback SDK
  PlaybackSession
  Chart / Project / Assets
  Runtime / Behavior / Animation / Particles
  Render Frontend / FrameSnapshot
  Audio Frontend / Clock contract
  Judgement / Result / Replay

Optional Adapters
  Filesystem / SDL Platform / SDL Audio
  OpenGL / future host-specific adapters

Internal
  World / EnTT / Debug / backend caches
```

运行时主流程：

```text
Host/App supplies Content + Clock + InputEvent
  -> PlaybackSession
  -> ChartRuntime
  -> RuntimeSession / World / Systems
  -> FrameSnapshot / RenderPacket
  -> Host Render Adapter or Cuexis RenderBackend
  -> JudgementResult / Score / Statistics
  -> optional ReplayData
```

设计原则：

```text
EnTT 管运行时对象
ChartDocument 管编辑数据
ChartRuntime 管播放数据
RuntimeSession 管一次播放或预览会话
PlaybackSession 管宿主可见的播放生命周期并隐藏内部 Runtime/World
ResourceManager 管资源生命周期
ContentProvider 管受索引约束的字节来源
RenderBackend 管图形 API
Component 只存数据
System 处理逻辑
OpenGL 是当前后端，不是架构中心
Vulkan 是未来后端，不应影响 Chart / Behavior / World
Judgement 计算结果，不拥有宿主输入设备、游戏状态或 UI
```

### 11.1 Runtime 模块边界

`cuexis_runtime` 是编译后谱面数据与各内部运行时模块之间的组合层。它由 `cuexis_playback` 封装，不是外部宿主直接链接的高层产品门面。它可以依赖多个前端模块，但不得成为保存任意全局状态或绕过模块边界的通用容器。

主要职责：

```text
创建和销毁一次播放或预览所需的 RuntimeSession
通过 ChartWorldInstantiator 将 ChartRuntime 实例化到 EnTT World
记录谱面对象与运行时 Entity 的会话期映射
按确定顺序编排 Behavior、Animation、Particle 等 System
持有 PropertyResolver，并在每帧末统一提交属性求值结果
把运行时毫秒和帧 delta 传递给对应 System
为 PlaybackSession 提供唯一内部运行时入口
```

明确不负责：

```text
解析、校验或迁移 ChartDocument，这属于 cuexis_chart
定义和管理资源生命周期，这属于 cuexis_assets
定义 World 和通用 Entity 生命周期，这属于 cuexis_world
处理 SDL 事件或创建窗口，这属于 cuexis_platform_sdl
调用 OpenGL 或持有后端对象，这属于 cuexis_render_opengl
产生或控制 AudioClock，这属于 cuexis_audio adapter 与宿主/应用组合层
定义宿主公共 API、FrameSnapshot、JudgementResult 或 ReplayData，这属于 cuexis_playback/cuexis_judgement
保存或直接修改 EditorDocument
```

Component 按功能归属到模块，而不是全部定义在 `cuexis_world`：

```text
TransformComponent / HierarchyComponent -> cuexis_world
RenderableComponent                     -> cuexis_render
BehaviorComponent                       -> cuexis_behavior
AnimatorComponent                       -> cuexis_animation
ParticleEmitterComponent                -> cuexis_particles
NoteTag / JudgePlaceholder              -> cuexis_gameplay（当前占位）
JudgementEvent / Score / Replay         -> cuexis_judgement（阶段 11）
```

`cuexis_world` 只提供 World、通用 Entity 生命周期和基础空间组件。一个模块可以在同一 EnTT Registry 上定义并处理自己的 Component，而不要求 World 反向依赖该模块。

实际 `RenderableComponent` 保存 typed `MeshHandle` / `MaterialHandle`，`BehaviorComponent` 保存稳定的 `RuntimeBehaviorIndex`。阶段 1B 已由 Runtime 在准备事务中解析 Renderable 资源并实例化 Handle；Behavior 在阶段 1B 仍只建立引用，不求值 opaque Behavior tracks。

RuntimeSession 使用事务式 PreparedSession：先执行无资源 I/O 副作用的结构预验证，再在临时 ResourceScope 中获取资源并实例化临时 World，全部成功后才发布。Prepared 数据绑定 Session token 与 ResourceManager token；Reload 构建完整 Replacement，失败保留旧 World、Scope、映射和活动诊断，v1 不做 Entity 增量修补。销毁顺序固定为 `World -> ResourceScope`。完整状态、所有权、Unload 和错误规则见 `docs/RUNTIME_SESSION.md`。

正式数据流：

```text
ChartDocument
  -> cuexis_chart: validate / migrate / compile
  -> ChartRuntime（不包含 entt::entity）
  -> cuexis_runtime: ChartWorldInstantiator
  -> World / EnTT Entity + 各模块 Component
  -> cuexis_playback: FrameSnapshot + JudgementResult + ReplayData
```

具体接口名称可以在实现阶段调整，但上述所有权和依赖方向属于已决策内容。参见 `docs/adr/0007-chart-runtime-world-boundary.md` 与 `docs/adr/0027-playback-sdk-product-boundary.md`。

## 12. ECS 规范

使用 EnTT 作为运行时 ECS。

Component 只保存数据，不包含复杂行为。

推荐：

```cpp
struct TransformComponent {
    Vec3 position;
    Quat rotation;
    Vec3 scale;
};

struct HierarchyComponent {
    entt::entity parent;
};

struct RenderableComponent {
    MeshHandle mesh;
    MaterialHandle material;
};

struct BehaviorComponent {
    BehaviorClipHandle clip;
};

struct NoteTag {};
struct ElementTag {};
```

禁止：

```cpp
struct RenderableComponent {
    void draw();
};

struct BehaviorComponent {
    void update(float dt);
};
```

System 负责逻辑：

```cpp
behaviorSystem.evaluate(registry, chartTimeMs, propertyWrites);
animationSystem.evaluate(registry, deltaTimeMs, propertyWrites);
propertyResolver.resolve(registry, propertyWrites);
particleSystem.update(registry, deltaTimeMs);
renderSystem.extract(registry, renderScene);
```

Behavior 和 Animation 对可绑定属性执行求值时，不直接依靠 System 调用顺序覆盖 Component。它们输出 `PropertyWrite`，由 Runtime 持有的 `PropertyResolver` 统一解析并提交。不会与其他求值系统冲突的内部状态仍可由所属 System 直接更新。

### 12.1 坐标与 Transform 约定

Cuexis 使用以下统一空间约定：

```text
坐标系：右手坐标系
轴方向：+X 向右，+Y 向上，+Z 向后
默认相机观察方向：-Z
世界单位：1 unit = 1 meter
矩阵向量约定：列向量
局部矩阵：Local = Translation * Rotation * Scale
世界矩阵：World = ParentWorld * Local
运行时旋转：Quaternion
运行时角度计算：弧度
编辑器角度显示：度
```

图形后端的裁剪空间差异必须由渲染后端处理，不能改变 World、Chart 或 Behavior 的坐标约定。

### 12.2 父子层级规则

`HierarchyComponent::parent` 可以为空，空父级表示根 Entity。父子关系属于运行时数据；谱面文件通过稳定对象 ID 表达父级引用，不得保存 `entt::entity`。

层级必须满足：

```text
禁止 Entity 把自身设为父级
禁止直接或间接循环引用
TransformSystem 按父级优先顺序更新世界矩阵
TransformSystem 使用脏标记避免无变化层级的重复计算
```

Reparent 操作必须由调用方显式指定模式，不提供隐含默认值：

```cpp
enum class ReparentMode {
    KeepLocal,
    KeepWorld,
};
```

`KeepWorld` 需要重新计算局部 TRS。如果新的父级变换导致结果包含无法无损表示为 TRS 的剪切，操作必须失败并返回明确错误，不能静默产生近似结果。

删除带有子级的 Entity 时，调用方同样必须显式指定模式：

```cpp
enum class DestroyHierarchyMode {
    DestroySubtree,
    DetachChildrenKeepWorld,
};
```

不得直接销毁父 Entity 后遗留无效的 `entt::entity` 父引用。`DetachChildrenKeepWorld` 同样受 TRS 无损分解限制；无法完成时，删除操作必须在修改 World 前失败。

谱面编译器必须先为完整文档建立对象 ID 索引，再解析父子引用，因此父对象在文件中的前后顺序不影响结果。只有父 ID 在完整索引中确实不存在时，才采用兼容性降级而不是让整个谱面编译失败：

```text
产生 Warning 级别的结构化诊断
跳过该孤儿对象
递归跳过以该孤儿对象为祖先的整个子树
继续编译其余相互独立且有效的对象
整体编译结果可以成功，但必须携带警告
```

警告至少包含谱面来源、缺失父对象 ID、被跳过的子树根对象 ID 和跳过的对象数量。启动日志、Chart Validator 和运行时调试界面必须能够显示该警告。不得静默把孤儿对象挂到根节点，也不得只编译其后代而改变原谱面的空间语义。

父子循环引用无法形成合法的 Transform 求值顺序，仍属于编译错误，不适用上述兼容性降级。

音符和元素的建议关系：

```text
底层统一为 Entity
Note / Element 是谱面语义，不是复杂继承类
音符 = Entity + Transform + Renderable + Behavior + NoteTag + JudgePlaceholder
元素 = Entity + Transform + Renderable + Behavior + ElementTag
装饰物 = Entity + Transform + Renderable + Behavior
```

## 13. 谱面系统

谱面分为两层：

```text
ChartDocument
  面向编辑、保存、版本迁移、可读性

ChartRuntime
  面向播放、快速查询、引用已解析、等待实例化
```

推荐流程：

```text
ChartDocument
  -> validate
  -> compile
  -> ChartRuntime
  -> cuexis_runtime::ChartWorldInstantiator
  -> EnTT World
```

`ChartDocument` 不直接等于 EnTT Registry。`ChartRuntime` 也不保存 `entt::entity`，不依赖 EnTT，并且不能承担 World 实例化职责。

谱面文件必须包含版本号：

```json
{
  "format": "cuexis.chart",
  "version": 1,
  "chartId": "019b0000-0000-7abc-8def-000000000001",
  "metadata": {},
  "timing": {},
  "templates": [],
  "behaviors": [],
  "objects": [],
  "requiredExtensions": [],
  "extensions": {}
}
```

`format` 用于识别格式族，`version` 只表示该格式族自身的版本。加载器不得通过文件中出现了哪些字段来猜测格式。

类型使用稳定字符串 ID，不直接绑定 C++ 类名。v1/v2 的旧示例仍可用于兼容格式；v3 使用 `behavior.event`：

```json
{
  "type": "behavior.event",
  "version": 1
}
```

谱面系统必须支持：

```text
版本迁移
格式校验
错误报告
资源引用解析
模板实例化
运行时编译
```

### 13.1 时间模型

时间模型采用两种明确分离的表示：

```text
谱面文件：使用拍数（beat）描述事件位置
引擎运行时：使用毫秒（ms）作为标准时间单位
```

`ChartDocument` 中的拍数必须通过独立的时间映射层转换为运行时毫秒。不得假设拍数与毫秒之间是固定比例，也不得把拍数直接传给只接受运行时时间的 System。

v1/v2 不实现完整的节奏变化逻辑；v3 使用已定义的 Tempo Event。数据结构和调用边界必须保持独立：

```text
BPM/Tempo Event
停顿
谱面偏移
```

`TimingMap` 提供至少以下方向的转换能力：

```text
beat -> chartTimeMs
chartTimeMs -> beat
```

阶段 1A 支持有限 `offsetMs` 与单一有限正 `defaultBpm`，并要求 BPM Changes 和 Stops 为空；这些约束不能固化到 Behavior、Animation、Render 或 Gameplay 模块中。v3 Tempo Event 在其 Beat 区间内直接插值 BPM；Stop 在指定 Beat 冻结 Beat，结束后使用该 Beat 生效的 BPM。offset 符号、逆映射和边界规则见 `docs/TIMING_MODEL.md`。

TimingMap 不定义 `speedChanges`。音符和其他 Entity 的移动速度属于 Behavior；如果未来支持整首音乐的播放倍率，则由 AudioTransport 与 Timeline 协作处理。二者都不能作为 TimingMap 中的隐式速度事件。

### 13.2 唯一规范谱面格式

Cuexis 只继续维护一种谱面格式：

```text
cuexis.chart  唯一规范、保存、迁移和运行输入格式
```

阶段 1 曾实现 `cuexis.chart.simple`（方案 B）作为早期手写导入格式。ADR 0035 已在阶段 2A.1 删除 Loader 路由、`SimpleChartImporter`、Simple Schema、测试和 Player fixture，不设计 Simple v2。当前加载链固定为：

```text
cuexis.chart JSON
-> CanonicalChartLoader
-> typed ChartDocument
-> validate
-> compile
-> ChartRuntime
-> RuntimeSession / PlaybackSession
```

Chart loader 使用 `cuexis_json_support` 的 Cuexis-owned JSON Value、typed `Reader` 和 Chart 语义校验；诊断携带以 `$` 为根的稳定字段路径。仓库同时提供版本化 JSON Schema artifact 和独立的 Schema adapter/test，但当前 loader 尚未调用该 adapter，因此文档和测试不得把加载成功表述为“已经执行 JSON Schema Validator”。字段路径用于定位输入，不承诺严格等同于某一外部 JSON Pointer 标准的完整语法。

#### 13.2.1 Canonical Chart

原生创建和保存的 canonical Chart 使用 UUIDv7 作为长期稳定的 `ChartObjectId`。ID 创建后不因对象重命名、属性修改、层级移动或类型调整而改变；复制对象时必须生成新 ID。阶段 1 已迁移为 canonical 的历史文件可能包含确定性 UUIDv5；它们按 canonical 版本兼容政策处理，不要求保留 Simple Parser。

引用使用结构化格式，并显式声明引用域：

```json
{
  "parent": {
    "domain": "object",
    "id": "019b1234-5678-7abc-8def-0123456789ab"
  },
  "material": {
    "domain": "asset",
    "id": "material.note.standard"
  }
}
```

canonical v1 支持的引用域：

```text
object
template
asset
behavior
```

`external-chart` 作为未来保留域，v1 遇到时报告 `UnsupportedFeature`，不加载其他 Chart 中的 Object 或 Template。

canonical 模板初版只允许单继承，通过结构化 `extends` 引用父模板，并使用 JSON Patch 风格的 `add`、`remove`、`replace` 表达覆盖。禁止模板继承环。模板在 Chart 编译阶段完全展开，ChartRuntime 不处理继承。

未知字段采用严格核心字段和显式扩展区的组合策略：

```text
当前版本中的未知核心字段：错误
extensions 中未知的命名空间：原样保留并警告
缺少可选扩展处理器：忽略运行效果、保留数据并警告
缺少 requiredExtensions 声明的处理器：编译失败
高于当前支持版本的规范文件：不得进入 Runtime
```

Studio 保存 canonical Chart 时必须原样保留自己不能识别的扩展数据。

持久化与运行时标识必须分离：

```text
ChartObjectId：新建 canonical 对象使用长期稳定 UUIDv7；历史 canonical 文件可保留已迁移的 UUIDv5
AssetId：资源系统中的稳定资源名称或资源 ID
RuntimeEntityId：仅在一次 RuntimeSession 中有效的紧凑句柄
```

ChartRuntime 可以把 UUID 映射为连续索引或紧凑句柄，但不得用该运行时值覆盖或替代持久化 ID。

#### 13.2.2 方案 B 移除与迁移

```text
阶段 1 历史代码：只在内存中导入并验证 cuexis.chart.simple v1，不输出 canonical JSON
阶段 2A.1 盘点：仓库 fixture 已有 canonical 对应物；仓库外需保留资产为空
阶段 2A.1 已删除：Simple Loader/Importer/Schema/tests/assets
当前行为：cuexis.chart.simple 稳定报告 chart.format.unsupported
```

一次性转换能力不进入阶段 2 Playback 安装包，也不构成长期公共 API。历史格式字段和转换规则仅保留在 `docs/SIMPLE_CHART_FORMAT.md` 与 ADR 0010/0015 中，移除决定以 ADR 0035 为准。

盘点结果为空，因此阶段 2A.1 未创建一次性转换物；该能力不属于当前源码、工具或安装包。

### 13.3 统一对象与组件 Schema

方案 A 使用单一 `objects` 数组，Note、Element、装饰物和未来语义对象都通过 Component 组合表达，不建立 `notes`、`elements` 等平行对象容器：

```json
{
  "id": "019b0000-0000-7abc-8def-000000000010",
  "name": "intro_note",
  "parent": null,
  "components": {
    "cuexis.transform": {
      "version": 1,
      "position": [0.0, 1.0, 0.0],
      "rotation": [0.0, 0.0, 0.0, 1.0],
      "scale": [1.0, 1.0, 1.0]
    },
    "cuexis.note": {
      "version": 1,
      "beat": { "numerator": 4, "denominator": 1 }
    }
  },
  "extensions": {}
}
```

规则：

```text
components 是按稳定 Component ID 索引的对象映射
同一 Object 中每种 Component 最多出现一次
每个 Component 数据包含独立 version
name 只用于编辑和诊断，不参与引用
parent 是 Object 结构字段，编译器在处理 Component 前建立完整层级图
数组顺序不具有运行语义，编译器按稳定 ID 产生确定性结果
```

Quaternion 在文件中固定为 `[x, y, z, w]`。Transform 的 position 使用米，scale 是无量纲分量倍率。

方案 A 中的 beat 使用约分后的有理数，不使用浮点位置：

```json
{
  "numerator": 7,
  "denominator": 4
}
```

`numerator` 是有符号 64 位整数，`denominator` 是大于 0 的整数。阶段 1 历史方案 B 曾根据原始十进制文本确定性转换有理数；该历史转换实现已随阶段 2A.1 删除。

模板 v1 只描述单个 Object 原型，不生成层级子树。解析顺序固定为父模板、当前模板 patch、实例 overrides，完成展开后再执行 Schema 和语义校验。

完整字段、引用、Timing、Template、扩展注册和诊断规则见 `docs/CHART_FORMAT.md`。该文件是方案 A v1/v2/v3 的格式规范；v3 的 Behavior Event 与 Tempo Event 目前已完成设计、尚待代码和 Schema 实现。对应 Schema artifact 必须随格式变更保持同步，当前 loader 的结构和语义权威仍是 typed Reader 与 Chart 校验代码。

## 14. 行为系统

行为系统用于谱面时间驱动。谱面序列化使用 Beat；Playback Runtime 的目标时间仍使用毫秒，并在名称中包含 `Ms`。TimingMap 负责两者之间的确定性映射。

核心概念：

```text
BehaviorEvent
BehaviorClip（后续版本）
HermiteProgress
Sampler
PropertyBinding
```

行为输出目标：

```text
Transform
MaterialParam
Visibility
ParentBinding
CoordinateSpace
```

推荐属性轨道：

```text
transform.position.x
transform.position.y
transform.position.z
transform.rotation
transform.scale
material.baseColor
material.emissive
render.visible
```

不推荐大量继承：

```cpp
class MoveBehavior;
class RotateBehavior;
class ScaleBehavior;
```

谱面文档使用 Beat 事件。v3 Runtime 可以缓存 Segment 的 `chartTimeMs` 边界用于查找，但每帧必须先由 TimingMap 得到唯一 Beat sample，并以 Beat 计算事件归一化进度；BPM 曲线下不得直接用经过毫秒数替代 Beat 进度：

```text
event: transform.position.x
startBeat: { numerator: 0, denominator: 1 }
durationBeats: { numerator: 1, denominator: 1 }
startValue: 0.0
endValue: 10.0
startSlope: 0.0
endSlope: 0.0
```

行为系统必须满足：

```text
可序列化
可独立采样
可在任意 chartTimeMs 预览
可调试显示
可用于编辑器时间轴
```

Behavior Event 对目标属性输出绝对值，作为该帧 Animation 混合前的基础状态。同一 Behavior 求值层中，对同一 Entity 的同一属性存在多个没有明确合并规则的写入时，谱面编译失败，不能依赖 Event 数组顺序或 ECS 遍历顺序决定结果。

## 15. 动画系统

动画系统用于通用 Entity 动画，不直接等同于谱面行为。

区分：

```text
BehaviorSystem
  使用 chartTimeMs
  服务谱面表现

AnimationSystem
  使用 localTimeMs / deltaTimeMs
  服务通用 Entity 动画
```

二者可以共享：

```text
Curve
Track
Sampler
PropertyBinding
```

推荐结构：

```text
AnimationClip
AnimatorComponent
AnimationSystem
```

第一阶段支持：

```text
Transform 动画
Material 参数动画
播放 / 暂停 / 循环
显式 Animation Layer、BlendGroup、weight 和 property mask
```

### 15.1 属性求值与冲突规则

Behavior 与 Animation 使用统一的分层属性求值流程：

```text
Entity 初始值
  -> Behavior 绝对采样
  -> Animation 显式混合
  -> PropertyResolver
  -> 最终 Component
```

`PropertyWrite`、`PropertyWriteBuffer`、属性 ID 和通用属性值类型是 `cuexis_world` 提供的共享前端数据契约。它们可以引用当前 World 中的会话期 Entity，但不得序列化到 `ChartDocument` 或 `ChartRuntime`，也不得包含 SDL 或图形后端类型。Behavior 和 Animation 可以生成这些数据，但不能反向依赖 `cuexis_runtime`。`PropertyResolver` 属于一次 `RuntimeSession`，负责根据 PropertyBinding 把最终值提交到 World 中的 Component。

Animation Track 必须显式声明混合模式：

```cpp
enum class AnimationBlendMode {
    Override,
    Additive,
};
```

不得为缺少混合模式的 Track 推断默认行为。旧格式迁移器可以补入由对应格式版本明确规定的值，但迁移后数据必须是显式的。

模式语义：

```text
Override：Animation 值替换 Behavior 求值后的值
Additive：Animation 值作为增量作用于 Behavior 求值后的值
```

Transform 的 Additive 规则固定为：

```text
position = basePosition + deltaPosition
rotation = normalize(baseRotation * deltaRotation)
scale = baseScale * scaleFactor  // 分量相乘
```

非 Transform 属性只有在对应 PropertyBinding 明确定义 Additive 运算时才能使用该模式；否则谱面编译失败。颜色、材质参数等属性的 Additive 语义需要在各自绑定规范中定义，不能套用未经声明的通用算法。

冲突处理规则：

```text
不同层冲突：按“初始值 -> Behavior -> Animation”顺序解析
同一层、同一目标属性存在歧义写入：编译失败
运行时动态产生无法解析的歧义：丢弃冲突层写入，保留本帧上一已完成层的值并报告错误
不得用 Track 数组顺序、Entity 创建顺序或 ECS 遍历顺序打破平局
```

每帧必须从确定的 Entity 初始值重新求值，不得把上一帧已经混合后的 Component 值作为下一帧基础值。这样可以避免累计误差，并保证暂停、倒放和跳转到任意 `chartTimeMs` 时得到可复现结果。

初始值由 `ChartWorldInstantiator` 在创建 Entity 时记录。运行期间永久修改基线必须提交 `BasePropertyCommand`，在 RuntimeSession 主线程安全点应用并递增 baseRevision；不能通过直接修改最终求值 Component 隐式改变基线。

多 Clip 使用显式 priority Layer 和 BlendGroup。Gameplay 与 Studio Preview 通过有生命周期的 OverrideToken 进入 PropertyResolver，不能直接写最终 Component。确定性的 Quaternion、离散属性和 Additive 规则见 `docs/ANIMATION_MIXING.md`。

## 16. 粒子系统

粒子系统作为独立模块，不应塞进 Render 后端。

推荐结构：

```text
ParticleEmitterAsset
ParticleEmitterComponent
ParticleSystem
ParticleRenderData
```

第一版使用 CPU 粒子：

```text
CPU 模拟
Billboard 渲染
动态顶点数据上传
颜色随生命周期变化
大小随生命周期变化
速度、重力、发射率参数
```

后续可扩展：

```text
GPU 粒子
Compute Shader
Indirect Draw
Vulkan Compute
```

粒子系统规则：

```text
ParticleSystem 不直接调用 OpenGL
粒子渲染数据交给 RenderSystem
粒子资产可序列化
粒子发射器可以挂载到 Entity
粒子可跟随父级 Transform
```

ChartTime 粒子使用版本化随机种子和 120Hz 固定步长。向后或大幅 Seek 通过 Checkpoint 恢复后正向重放，不做负 delta 积分；超出单帧预算时进入 Rebuilding，完成后再发布精确状态。详见 `docs/PARTICLE_TIMELINE.md`。

## 17. 渲染架构

业务层不得调用 OpenGL。

推荐流程：

```text
EnTT Registry
  -> RenderSystem
  -> RenderScene
  -> Playback FrameSnapshot / RenderPacket
  -> Host Render Adapter 或 Cuexis RenderBackend
  -> optional OpenGLBackend
```

推荐渲染命令：

```cpp
struct RenderCommand {
    MeshHandle mesh;
    MaterialHandle material;
    Mat4 worldMatrix;
    RenderLayer layer;
};
```

渲染前端概念：

```text
RenderScene
RenderCommandList
FrameSnapshot / RenderPacket
宿主 Camera/Viewport input
Light
Material
Mesh
Texture
ShaderAsset
PipelineDesc
BufferDesc
TextureDesc
SamplerDesc
BindingSet
RenderPass
Framebuffer
CommandList
```

避免 OpenGL 状态机式接口：

```text
setUniform
bindTextureUnit
useProgram
```

这类接口会导致未来 Vulkan 后端很难接入。

推荐渲染阶段：

```text
OpaquePass
TransparentPass
ParticlePass
DebugPass
UIPass
```

`UIPass` 只属于 Player/Studio 应用或特定宿主 adapter，不进入 Playback 核心帧契约。第一版 SDK 优先冻结宿主可消费的表现数据与能力声明，不为通用游戏 UI 建立 RenderGraph。

第一版不强制实现完整 RenderGraph。可以先用固定 Pass，后续演进：

```text
固定 Pass 列表
-> 显式 RenderPass
-> 简单 FrameGraph
-> 完整 RenderGraph
```

## 18. Vulkan 预留原则

当前只实现 OpenGL 后端，但架构必须允许未来新增：

```text
cuexis_render_vulkan
```

Vulkan 未来主要影响：

```text
RenderBackend
Shader 编译流程
资源上传流程
Pipeline 管理
BindingSet 管理
RenderPass / Framebuffer 管理
```

不应影响：

```text
Chart
Behavior
Animation
World
Audio
Gameplay
EditorDocument
```

原则：

```text
不要把 OpenGL API 暴露给业务层
不要让 Material 存 GLuint
不要让 ShaderAsset 只服务 GLSL
不要在 Entity 中保存图形 API 对象
RenderBackend 抽象应更接近现代图形 API
```

Vulkan 正式开发不应过早开始。建议在 OpenGL 后端、材质系统、RenderCommand、Pipeline 抽象稳定之后，只做可行性验证。

Shader 的跨后端公共路径已经由 `docs/SHADER_PIPELINE.md` 定义；Vulkan 验证不得另建一套 ShaderAsset 或 Material Schema。

## 19. 资源系统

业务层不得直接持有文件路径或后端资源 ID。

统一使用 Handle：

```cpp
TextureHandle
MeshHandle
ShaderHandle
MaterialHandle
AudioClipHandle
ChartHandle
BehaviorClipHandle
ParticleEmitterHandle
```

资源加载流程：

```text
AssetDatabase
  -> ContentProvider
  -> Importer
  -> ResourceManager
  -> Runtime Handle
```

材质资产示例：

```json
{
  "name": "note_standard",
  "shader": "shader.note_standard",
  "properties": {
    "baseColor": [1, 1, 1, 1],
    "emissive": 0.0,
    "mainTexture": "texture.note"
  },
  "renderState": {
    "blend": "alpha",
    "depthTest": true,
    "depthWrite": true,
    "cull": "back"
  }
}
```

资源系统必须支持：

```text
资源引用
资源缓存
资源释放
错误占位资源
热重载预留
编辑器资源浏览预留
```

### 19.1 标识、Handle 与所有权

持久化资源标识和运行时访问必须分离：

```text
AssetId             可序列化的稳定资源标识
ResourceHandle<T>   当前进程内的类型化弱句柄，不拥有资源
ResourceLease<T>    RAII 强引用，保证资源存活
ResourceScope       批量持有多个 Lease
ResourceManager     资源槽位、状态、依赖和生命周期的唯一所有者
```

类型化 Handle 至少包含槽位索引和 generation：

```cpp
template<typename Tag>
struct ResourceHandle {
    std::uint32_t index;
    std::uint32_t generation;
    std::uint64_t managerToken;
};

struct TextureTag;
using TextureHandle = ResourceHandle<TextureTag>;
```

资源槽位被真正释放并复用时必须递增 generation。旧 Handle 查询失败，不能意外指向复用槽位中的新资源；非序列化的 manager token 还用于拒绝来自另一 ResourceManager 的同 index/generation Handle。Handle 不增加引用计数，也不得序列化到 ChartDocument、资产文件或缓存索引中。

Component 只保存 `ResourceHandle<T>`。它们不得保存 `ResourceLease<T>`、裸资源指针、`shared_ptr`、`entt::resource` 或图形后端 ID。

`ResourceLease<T>` 持有强引用。PlaybackSession 内部的 RuntimeSession、Studio Preview 和其他批量使用场景通过 `ResourceScope` 集中持有 Lease，因此大量 Entity 不会各自执行引用计数操作。Scope 销毁只释放强引用；ResourceManager 可以根据预算和缓存策略延后真正卸载，避免反复加载抖动。

错误占位资源和引擎内建默认资源由 ResourceManager 固定持有，不参与普通自动卸载。

### 19.2 状态、访问和依赖

资源槽位具有明确状态：

```cpp
enum class ResourceState {
    Unloaded,
    Loading,
    Ready,
    Failed,
    Reloading,
};
```

`ResourceManager::get(handle)` 或等价访问接口必须检查类型、index、generation 和状态。返回的 View、引用或指针只能在文档规定的资源同步点之前临时使用，调用方不得跨帧或跨资源更新长期缓存裸地址。

AssetDatabase 负责 `AssetId -> 资产描述/逻辑来源`，ContentProvider 负责按已校验来源提供有界字节，ResourceManager 负责 `AssetId -> 运行时槽位`。业务层不得把文件路径当作 AssetId，也不得绕过 AssetDatabase/Provider 读取任意内容。阶段 1B 现有物理读取在阶段 1E 提取为 FilesystemContentProvider。

Material、Template 等资源可以声明依赖。ResourceManager 在加载根资源时构建依赖图，由 ResourceScope 持有传递依赖的 Lease。依赖环属于加载错误，诊断必须列出完整环路。

资源引用声明加载策略：

```text
Required：缺失时 PlaybackSession/内部 RuntimeSession 或相关加载事务失败
Fallback：使用类型匹配的错误占位资源并产生警告
Optional：跳过对应能力并产生诊断
```

初始默认策略：

```text
主谱面、主音乐、BehaviorClip：Required
Mesh、Texture、Material、Shader：Fallback
装饰性粒子和可选音效：Optional
```

具体资产可以显式收紧策略，但不能把 Required 静默降级为 Optional。

### 19.3 第一版加载与线程边界

阶段 1 只实现同步加载，并且在内部 RuntimeSession 实例化 World 前完成：

```text
AssetId
  -> AssetDatabase
  -> ContentProvider
  -> Loader / Importer
  -> CPU Resource
  -> ResourceHandle + ResourceLease
```

第一版不提前冻结 `Future`、协程或任务系统 API，但保留状态枚举。未来异步加载必须遵守：

```text
Worker：Provider 读取、解压、解码和纯 CPU 解析
Resource Owner Thread：提交槽位状态与内容 revision
Render Thread：创建、替换和延迟销毁 GPU 对象
Audio Thread：不参与资源加载
```

ResourceManager 不直接调用 OpenGL。渲染后端以 ResourceHandle 和内容 revision 为键创建自己的后端对象，并在正确线程释放。CPU 资源卸载不能直接触发任意线程上的 OpenGL 删除。

### 19.4 热重载与失效

同一 AssetId 热重载成功时：

```text
保持 Handle 的 index 和 generation
递增 contentRevision
在资源同步点替换 CPU 内容
通知相关后端重建派生对象
```

热重载失败时保留上一份有效内容，恢复 `Ready` 状态并记录错误；不能把正在使用的资源替换为空对象。只有资源真正卸载、槽位失效并可复用时才递增 generation。

如果资源没有上一份有效内容且首次加载失败，则进入 `Failed`，由 Required、Fallback 或 Optional 策略决定上层结果。

### 19.5 第三方资源缓存

可以评估使用 EnTT Resource Cache 作为 `cuexis_assets` 内部的 CPU 缓存实现，因为 EnTT 已经是项目基础依赖。但 Cuexis 的公共头文件、Component 和 Chart 格式不得暴露 `entt::resource` 或把 EnTT 的引用计数语义当作项目公共生命周期规则。

模型、纹理、压缩和其他 Importer 优先评估成熟开源库。第三方 Importer 只负责数据转换，不决定 AssetId、Handle、Lease、Scope 或后端对象的所有权。

## 20. 音频系统

Cuexis 使用 `cuexis_audio` 定义后端无关的播放与时钟接口，首个可选实现为 `cuexis_audio_sdl`。独立 Player 使用 SDL3 Audio Device 和 `SDL_AudioStream`；嵌入宿主可以自行播放音乐并提供 HostClock，不要求安装或初始化 SDL Audio。

模块边界：

```text
cuexis_audio
  AudioClipHandle
  IAudioClock
  IAudioTransport
  PlaybackState
  AudioClockSnapshot
  不包含 SDL 类型

cuexis_audio_sdl
  SDL3 Audio Device
  SDL_AudioStream
  PCM 提交、格式转换、缓冲和设备管理

host / cuexis_player
  明确选择 HostClock 或 CuexisAudio 模式
  创建并控制 IAudioTransport
  读取 IAudioClock
  或直接提供宿主时间
  把统一 RuntimeFrame 传给 PlaybackSession
```

SDL 子系统的初始化和关闭顺序由选择该 adapter 的 Player/宿主持有。`cuexis_audio_sdl` 直接依赖 SDL3，不依赖 `cuexis_platform_sdl`；两个后端模块不得通过彼此传递 SDL 对象。PlaybackSession、Runtime 和 Judgement 均不依赖 `cuexis_audio_sdl`。

### 20.1 Transport 与 Clock

播放控制和时钟读取必须分离：

```cpp
enum class PlaybackState {
    Stopped,
    Playing,
    Paused,
    Ended,
    Error,
};

struct AudioClockSnapshot {
    std::int64_t presentedFrame;
    double positionMs;
    double estimatedOutputLatencyMs;
    PlaybackState state;
    std::uint64_t discontinuityId;
};

class IAudioClock {
public:
    virtual ~IAudioClock() = default;
    virtual AudioClockSnapshot snapshot() const = 0;
};

class IAudioTransport {
public:
    virtual ~IAudioTransport() = default;

    virtual AudioResult load(AudioClipHandle clip) = 0;
    virtual AudioResult play() = 0;
    virtual AudioResult pause() = 0;
    virtual AudioResult stop() = 0;
    virtual AudioResult seekMs(double positionMs) = 0;

    virtual const IAudioClock& clock() const = 0;
};
```

示例中的 `AudioResult` 只表示必须返回可检查错误，不冻结具体错误类型。Runtime、Behavior 和 Judgement 只消费 PlaybackSession 传入的 RuntimeFrame/InputEvent 时间域，不能控制播放设备。HostClock 与 CuexisAudio 的差异不能进入 Chart 或判定算法。

### 20.2 时间语义

AudioClock 内部使用整数采样帧计数，毫秒只在接口边界计算：

```text
positionMs = presentedFrame * 1000.0 / sampleRate
```

`positionMs` 表示从音频资源第一个采样开始、预计已经到达输出设备的播放位置。SDL 不能在所有平台提供精确硬件播放光标，因此第一版是经过缓冲延迟修正的估算值，并通过 `estimatedOutputLatencyMs` 暴露诊断信息。

AudioClock 不处理 BPM、谱面 offset、用户校准或输入延迟：

```text
HostClock.positionMs 或 AudioClock.positionMs
  -> 宿主/Player Timeline 应用谱面 offset
  -> Timeline 应用用户音画校准
  -> chartTimeMs
  -> TimingMap
  -> beat
```

阶段 11 正式 Judgement 开发前必须测量 SDL 参考后端和至少一个宿主时间链路的延迟与稳定性。如果 SDL 不能满足目标，可以新增平台专用 adapter；如果宿主自行播放音乐，则由宿主提供可追踪的时间/延迟来源。两种情况都不得修改 Chart、RuntimeFrame 或 Judgement 的确定性边界。

### 20.3 播放不连续与线程规则

以下事件递增 `discontinuityId`：

```text
Seek
Stop
重新加载音乐
音频设备丢失或重建
输出格式重建
```

Runtime 观察到 ID 改变后，从新的 `chartTimeMs` 完整重求值。Pause 只停止 Clock 前进，不递增 ID。Underrun 必须停止播放位置推进并输出结构化警告；是否重建设备由后端错误状态决定。

SDL 音频回调或实时供数路径禁止：

```text
文件读取
动态内存分配
格式化日志
可能阻塞的 mutex
资源数据库查询
JSON 解析
ECS 操作
```

跨线程状态通过预分配缓冲和原子快照传递。`IAudioClock::snapshot()` 必须线程安全且不阻塞音频线程。

### 20.4 第一版范围与后续解码

第一版 CuexisAudio adapter 只实现：

```text
一个 SDL 播放设备
一个主音乐 AudioStream
整首 WAV 预解码到内存
播放、暂停、停止和 Seek
采样帧时钟与输出延迟估算
设备错误和 underrun 日志
```

WAV 由 `audio_sdl::WavDecoder` 从已加载的有界内存扫描 RIFF chunk，并将 PCM/IEEE F32 转换为
AudioClip；解码不调用 SDL 设备 API。后续压缩格式通过独立 `IAudioDecoder` 扩展，优先评估成熟且
许可证兼容的开源解码库，不自行实现 OGG、MP3 或 FLAC 编解码器。Decoder 只输出 PCM 和音频
格式，不控制设备、播放状态或 AudioClock。

主音乐、音效和未来 Studio 预览可以绑定同一 SDL 播放设备的独立 AudioStream，但只有主音乐流可以成为 CuexisAudio 模式的谱面时间源；音效失败不得改变 `chartTimeMs`。HostClock 模式不创建 SDL 设备。

## 21. 输入、判定与回放边界

阶段 1A 已实现的 NullInput/NullJudge 只是历史占位。正式模型在阶段 11 由 Playback SDK 必选的 `cuexis_judgement` 交付：

```text
宿主/Player adapter 采集原始输入
-> 标准化 InputEvent/InputFrame
-> PlaybackSession::update(RuntimeFrame, inputEvents)
-> cuexis_judgement
-> JudgementEvent / Score / Combo / Statistics
-> PlaybackSession::extractResult()
-> JudgementResult snapshot（只读，宿主自行决定持久化与展示）
```

宿主负责输入设备、绑定入口、游戏状态和 UI；SDK 负责确定性判定、计分、输入记录和回放。纯播放/Studio 预览不提交 InputEvent 时，Judgement 休眠。

原则：

```text
BehaviorSystem 不直接依赖输入
SDL、Unity、Unreal 或其他宿主事件类型不进入公共判定模型
InputEvent 包含稳定时间戳、source、arrival time 和 sequence
相同 Chart、InputEvent、校准和 Session 配置产生相同结果
startRecording() 按 chartTimeMs 记录全部 InputEvent，stopRecording() 返回 ReplayData
loadReplay() 注入预记录事件替代实时输入，回放结果与实时播放确定一致
回放模式拒绝同时提交实时 InputEvent，Session 返回明确错误
Judgement/Replay 不依赖 World、SDL、音频或渲染后端
```

ReplayData 使用独立版本化 Cuexis 格式；具体 Schema、迁移、预算和完整性校验在阶段 11 随真实消费者冻结。

## 22. Cuexis Studio 编辑器

编辑器长期会成为核心工作量，必须从架构上和 Runtime 分离。

推荐关系：

```text
Editor 修改 ChartDocument
PlaybackSession 从文档/项目 source 准备内部 ChartRuntime/World
Viewport 使用 StudioPreviewOverride 提交预览输入
PlaybackSession 输出 FrameSnapshot
Viewport adapter 显示帧快照
```

禁止：

```text
编辑器直接把 EnTT Registry 当成保存格式
编辑器直接修改 Runtime Entity 后保存
Viewport 直接访问 RuntimeSession、World 或 EnTT Registry
Viewport 直接消费可变 RenderScene 容器（必须通过 FrameSnapshot）
Studio 建立独立于 PlaybackSession 的预览编译/实例化路径
```

编辑器核心模块：

```text
EditorDocument
SelectionSystem
CommandSystem
UndoRedo
HierarchyPanel
InspectorPanel
ViewportPanel
TimelinePanel
AssetBrowser
MaterialEditor
BehaviorEditor
ParticleEditor
ShaderEditor
```

第一版使用 Dear ImGui。

编辑器必须支持：

```text
保存 / 加载
撤销 / 重做
时间轴拖动
运行时预览
资源选择
属性检查
错误显示
```

## 23. Shader 与材质编辑

Shader 编辑分阶段实现。

第一阶段：材质参数编辑器。

```text
选择 Shader
编辑贴图
编辑颜色
编辑透明度
编辑混合模式
编辑深度测试
编辑剔除模式
实时预览
```

第二阶段：Shader 源码编辑。

```text
GLSL 热重载
编译错误显示
Uniform / Property 映射
失败回退
```

第三阶段：Shader Graph。

```text
节点编辑
中间表示
生成规范 GLSL
复用 SPIR-V 编译、反射和目标变体管线
```

Shader 规范输入为受约束 GLSL 450，经 shaderc/glslang 生成 SPIR-V，再使用 SPIRV-Cross 反射并生成 GLSL 330 Core 与 GLSL ES 300；Vulkan 直接使用 SPIR-V。Variant、Binding、缓存和热重载规则见 `docs/SHADER_PIPELINE.md`。

ShaderAsset 不应只绑定 OpenGL：

```json
{
  "name": "note_standard",
  "stages": {
    "vertex": "shaders/note.vert",
    "fragment": "shaders/note.frag"
  },
  "properties": [
    { "name": "baseColor", "type": "vec4", "default": [1, 1, 1, 1] },
    { "name": "emissive", "type": "float", "default": 0.0 },
    { "name": "mainTexture", "type": "texture2D" }
  ],
  "renderState": {
    "blend": "alpha",
    "depthTest": true,
    "depthWrite": true,
    "cull": "back"
  }
}
```

## 24. 主循环建议

运行时主循环应尽早区分不同时间概念。

```cpp
while (host.running()) {
    auto inputEvents = inputAdapter.poll();
    auto runtimeFrame = timeline.nextFrame(hostClockOrAudioClock);

    playbackSession.update(runtimeFrame, inputEvents);

    auto frame = playbackSession.extractFrame();
    renderAdapter.render(frame);

    auto result = playbackSession.extractResult();
    host.consumeJudgement(result);
}
```

PlaybackSession 内部调用 RuntimeSession，并按已确定顺序编排 Behavior、Animation、Particle、帧提取和 Judgement。宿主/Player 仍拥有平台事件、Clock 和具体 Render adapter。示例中的类型与函数名只表达职责，不是已经冻结的 API。

Timeline 在暂停时把 `simulationDeltaTimeMs` 设为 `0`，避免状态型系统继续推进。Runtime 发现 `timeDiscontinuityId` 改变时按 `docs/RUNTIME_SESSION.md` 通知所有状态型 System，并执行确定性重采样或重建。

没有 CuexisAudio 时可以使用 HostClock、ChartClock 或模拟时钟。正式判定只依赖归一化 RuntimeFrame 和 InputEvent 时间域，不要求某一种具体 AudioClock 实现。

## 25. 调试能力

每个复杂系统都必须有调试入口。

推荐调试功能：

```text
显示 Entity 列表
显示 Transform
显示父子层级
显示 RenderCommand 数量
显示 DrawCall 数量
显示 FPS / frame time
显示 chartTimeMs
显示行为曲线采样值
显示属性各求值层输入、混合模式和最终解析值
显示资源加载状态
显示 AudioClock 位置、估算输出延迟、播放状态和 discontinuityId
显示音频缓冲量与 underrun 计数
显示 ContentProvider 来源、请求和预算诊断
显示 PlaybackSession 状态、FrameSnapshot 大小和复制次数
显示 Judgement、Score、Combo、Statistics 和 Replay 模式
显示粒子数量
显示材质和 shader 状态
显示谱面编译警告和被跳过的层级子树
```

音频诊断必须拆分为确定性帧轨迹和设备遥测。前者只包含规范化 RuntimeFrame、
discontinuity 与后端无关 FrameSnapshot hash，可用于 HostClock/CuexisAudio parity；后者包含
wall clock、presented source position、估算延迟、queue 和 underrun，只用于 drift 与设备趋势。
两类数据按 frameIndex 关联但不得逐列混比，采集必须固定容量并显式报告 droppedRows，CSV 只能
在 owner thread 离线导出。导出同时生成记录 schema/version、构建/API 版本、mode、行数、
droppedRows 和截断状态的 metadata sidecar；具体字段、公式和门禁见阶段 1D 实施计划。

空间调试：

```text
坐标轴
包围盒
相机视锥
粒子发射器范围
对象名称
未来判定区域可视化
```

## 26. 代码规范

命名建议：

```text
类型名：PascalCase
函数名：camelCase
变量名：camelCase
命名空间：cuexis::module
宏：CUEXIS_UPPER_CASE
文件名：snake_case
```

开发规则：

```text
Component 只存数据
System 写逻辑
Core 不依赖平台和渲染后端
资源必须走 ResourceManager
业务代码不能直接调用 OpenGL
新增文件格式必须有 version
新增大决策必须写 ADR
可预期错误通过 cuexis::core::Result 返回
异常不得跨模块公共边界
线程敏感 API 必须声明并检查所属线程
```

禁止：

```text
把所有代码放进 main.cpp
为了方便跨模块反向依赖
在 Component 析构里释放 OpenGL 资源
在 ChartDocument 中保存后端资源 ID
绕过 ResourceManager 加载资源
```

完整 Result/Error、所有权、线程和日志规则见 `docs/CODE_POLICY.md`。

## 27. 测试规范

Cuexis 统一使用：

```text
单元测试框架：Catch2 v3
测试运行器与 CMake 集成：CTest
```

不得在同一项目中同时维护另一套通用单元测试框架。若未来确实需要 GoogleMock 等额外能力，必须先通过 ADR 说明 Catch2 和简单 Fake 无法满足的具体需求。

基础 CMake 接入方式：

```cmake
include(CTest)

if(BUILD_TESTING)
    find_package(Catch2 3 CONFIG REQUIRED)
    add_subdirectory(tests)
endif()
```

每个测试可执行 target 使用 `Catch2::Catch2WithMain`，并通过 `catch_discover_tests` 注册到 CTest：

```cmake
add_executable(cuexis_core_tests
    core/version_tests.cpp
    core/time_tests.cpp
)

target_link_libraries(cuexis_core_tests
    PRIVATE
        cuexis_core
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_core_tests)
```

Catch2 必须只作为测试 target 的 `PRIVATE` 依赖。SDK 公共头、内部运行时库、Player 和未来 Studio 不得包含 Catch2 头文件或暴露 Catch2 类型。

测试 target 按引擎模块拆分，而不是把所有测试放入一个可执行文件：

```text
cuexis_core_tests
cuexis_json_support_tests
cuexis_project_tests
cuexis_platform_sdl_tests
cuexis_world_tests
cuexis_assets_tests
cuexis_chart_tests
cuexis_behavior_tests
cuexis_gameplay_tests
cuexis_animation_tests
cuexis_runtime_tests
cuexis_render_tests
cuexis_debug_tests
cuexis_render_opengl_tests
cuexis_audio_tests
cuexis_playback_tests
cuexis_content_provider_tests
cuexis_judgement_tests
cuexis_external_consumer_tests
```

阶段 1A 已建立 `cuexis_core_tests`、`cuexis_json_support_tests`、`cuexis_platform_sdl_tests`、`cuexis_world_tests`、`cuexis_assets_tests`、`cuexis_chart_tests`、`cuexis_behavior_tests`、`cuexis_gameplay_tests`、`cuexis_render_tests`、`cuexis_debug_tests`、`cuexis_runtime_tests` 和 `cuexis_render_opengl_tests`，并在 CTest 中加入架构扫描及 Player 参数、谱面文件和 SDL 初始化失败路径检查。需要真实窗口和 GPU 的 canonical 示例三帧 OpenGL 冒烟测试按 `docs/BUILDING.md` 单独执行，不混入算法单元测试；阶段 1A 的历史 A/B 结果以[阶段 1A 完成报告](stage_reports/stage_1a_completion_report.md)为准。

尚未实现的模块不需要提前创建空测试 target。测试名称应描述行为和条件，不能只重复被测函数名。

优先测试：

```text
Behavior Event/Hermite 采样
Tempo Event/BPM 积分与逆映射
ChartDocument 解析
统一 Object/Component Schema、Beat 有理数和引用域校验
canonical Chart ID、Beat、Template 和版本迁移
ChartRuntime 编译
资源 Handle 生命周期
Handle generation、Lease/Scope 生命周期和热重载 revision
采样帧到毫秒转换、AudioClock 暂停与不连续事件
TimingMap BPM/Stop 边界、负 Beat、offset 和逆映射
PreparedRuntimeSession 失败回滚、Reload 保留旧 Session 和销毁顺序
Transform 父子层级
行为系统属性输出
Animation Layer 权重、Quaternion、离散属性和 OverrideToken
粒子跨帧率、Checkpoint 和 Seek 确定性
Shader Reflection、Variant 缓存键和多目标编译
PlaybackSession headless 生命周期、多 Session、FrameSnapshot 和 Player parity
Filesystem/Memory/Host ContentProvider 等价性、错误与预算
安装后公共头、add_subdirectory/find_package external consumer
InputEvent、JudgementResult、计分、记录与 ReplayData 往返确定性
版本号生成
```

推荐测试目录：

```text
tests/
  core/
  json_support/
  platform/
  assets/
  chart/
  behavior/
  gameplay/
  animation/
  particles/
  runtime/
  world/
  render/
  debug/
  audio/
  playback/
  judgement/
  external_consumer/
```

测试原则：

```text
核心数据结构必须有单元测试
谱面格式必须有解析失败测试
行为采样必须覆盖边界时间
资源系统必须测试有效 Handle
渲染后端可先做集成 demo 验证
算法和数据格式测试必须可重复，不依赖真实时间、随机设备、窗口或 GPU
接口协作者优先使用小型 Fake；不要为了调用次数断言过早引入复杂 Mock
测试必须能通过 ctest --preset debug 或对应构建目录中的 ctest 执行
```

## 28. ADR 规范

重大技术决策必须写 ADR。

当前已接受 ADR：

```text
docs/adr/0001-project-name-cuexis.md
docs/adr/0002-use-vcpkg.md
docs/adr/0003-use-cmake-presets.md
docs/adr/0004-version-format-yy-mm-dd-hh.md
docs/adr/0005-use-entt.md
docs/adr/0006-render-backend-abstraction.md
docs/adr/0007-chart-runtime-world-boundary.md
docs/adr/0008-coordinate-transform-hierarchy.md
docs/adr/0009-property-evaluation-and-conflicts.md
docs/adr/0010-canonical-and-simple-chart-formats.md
docs/adr/0011-use-catch2-and-ctest.md
docs/adr/0012-use-sdl3-audio-backend.md
docs/adr/0013-resource-handle-lease-and-scope.md
docs/adr/0014-unified-chart-object-schema.md
docs/adr/0015-simple-chart-format.md
docs/adr/0016-use-apache-2-license.md
docs/adr/0017-transactional-runtime-session.md
docs/adr/0018-timing-map-semantics.md
docs/adr/0019-animation-layers-and-runtime-overrides.md
docs/adr/0020-deterministic-particle-timeline.md
docs/adr/0021-spirv-centered-shader-pipeline.md
docs/adr/0022-android-resource-and-input-strategy.md
docs/adr/0023-code-error-and-thread-policy.md
docs/adr/0024-configuration-ownership-and-staged-formats.md
docs/adr/0025-project-config-v1-and-path-security.md
docs/adr/0026-asset-index-and-source-resolution.md
docs/adr/0027-playback-sdk-product-boundary.md
docs/adr/0028-camera-projection-and-events.md
docs/adr/0029-behavior-track-v1.md
docs/adr/0030-playback-preview-api-version-and-result.md
docs/adr/0031-main-music-content-format-v2.md
docs/adr/0032-playback-clock-and-prepared-audio-transaction.md
```

每份 ADR 至少包含：

```text
背景
决策
备选方案
影响
后续风险
```

## 29. Definition of Done

每个功能完成时必须满足：

```text
代码可以编译
核心路径可运行或可测试
没有破坏模块依赖方向
没有业务层 OpenGL 泄漏
没有 SDK 公共头泄漏 EnTT、SDL、OpenGL 或实现依赖
资源生命周期明确
错误日志清晰
必要文档已更新
必要测试或 demo 已补充
新增依赖已完成许可证记录与 THIRD_PARTY_NOTICES 更新
```

阶段完成时必须满足：

```text
可以通过 cmake --preset debug 配置
可以通过 cmake --build --preset debug 构建
可以通过 ctest --preset debug 运行测试
vcpkg manifest 依赖完整
版本号正确写入日志
至少有一个可运行 demo
模块边界没有被破坏
涉及 SDK 公共边界时，headless 和 external consumer 门禁通过
```

正式构建还要求：

```text
版本号符合 yy.mm.dd.hh-v 及对应构建类型后缀规则
构建产物包含 Cuexis 名称
启动日志打印 Cuexis 版本
文档与版本同步
THIRD_PARTY_NOTICES 与实际分发依赖一致
SDK 安装包版本、组件依赖和公共头与文档一致
```

## 30. 阶段规划

以下月份仅用于表达阶段相对规模，不是固定发布日期。进入实施后以阶段验收标准作为完成依据，并根据实际进度更新估算。

### 配置整合路线（跨阶段）

配置能力不作为一个独立大阶段一次性实现，也不建立可被任意模块读写的全局 `EngineConfig` 单例。Player 与 Studio 调用共享 Project/Config 前端；嵌入宿主可以提交 typed/memory project source、HostCapabilities 和会话配置。Playback SDK 与内部模块只接收已经解析、校验并确定所有权的 typed config 或不可变快照，不自行读取应用配置目录。

以下名称首先表示配置职责，不代表所有格式都要一次性冻结。阶段 1A 的配置 ADR 冻结了跨阶段所有权、覆盖/约束关系、失败与诊断原则，以及 ProjectConfig 的文件定位和格式身份；阶段 1B 已随真实 Project/Asset 消费者冻结 ProjectConfig v1 与 Asset Index v1 的字段、Draft 7 Schema、路径安全、版本和原子保存规则。其余类别的具体 C++ 类型名、文件名、`format` ID、Schema 和迁移版本仍在首次消费阶段通过专项 ADR 或等价设计评审确认：

| 配置类别 | 所有者与用途 | 持久化原则 |
| --- | --- | --- |
| ProjectConfig | Player 与 Studio 的标准项目身份、资产根、入口内容和项目默认策略；嵌入 SDK 可使用等价 typed/memory source | 可随项目提交，必须带格式版本和迁移规则 |
| UserPreferences | 窗口、音量、已经实现的 Audio/Input/Calibration profile ID、最近项目和编辑器布局等偏好 | 按应用保存在用户目录，不单独保存音频输出设备或选择 DeviceProfile，不预留尚无消费者的 profile ID，也不复制 profile 内的设备身份与校准字段 |
| DeviceProfile | 创建前能力匹配条件、资源硬预算、兼容的 ImporterProfile/ShaderTargetProfile ID 或约束，以及可接受上限 | 随发行包提供，由 PreflightCapabilities 匹配选择；不复制目标格式或本次运行的探测事实，用户设置不能突破硬上限 |
| PreflightCapabilities / HostCapabilities | 创建 Window/Backend/Audio 前可知的平台事实，或宿主明确提供的内容/时间/渲染能力 | 不是文件；区分宿主能力与 Cuexis 内建 adapter 实际值 |
| Device/Calibration Profiles | 音频输出校准、输入绑定/延迟和用户主观时序校准，各自具有唯一字段所有者 | 用户本机版本化数据；UserPreferences 只保存所选 profile ID，不复制校准值 |
| LaunchOptions | 项目路径、测试、诊断和本次应用/宿主会话的显式启动选项 | 只存在于当前进程，不自动写回 |
| ResolvedAppConfig | 将当前可用来源解析为 Window、Render、Audio、Resource/ContentProvider 等应用或宿主 adapter 请求值的不可变快照 | 不是文件，不允许模块运行时反向修改配置来源 |
| ResolvedSessionConfig | 只保存一次播放/预览中影响 Runtime、输入和确定性结果的配置子快照 | 不是后端所有者；必要时记录身份或内容 hash 供回放和测试复现 |
| EffectiveSettings | Window、Render、Audio 等子系统创建后报告的实际 Context/设备能力、生效值和协商回退结果 | 只读运行时诊断，不写回 ProjectConfig 或覆盖请求来源 |
| JudgementResult | PlaybackSession 的只读判定、分数、连击和统计快照 | 不是配置；所有权交给宿主消费，不写入 Chart |
| ReplayData | InputEvent 序列与确定性 Session 配置快照 | 阶段 11 定义独立版本化格式；由宿主决定保存位置 |

设备相关字段必须保持唯一所有者：AudioDeviceProfile 保存输出设备匹配身份与输出校准，InputProfile 保存输入绑定和输入延迟，CalibrationProfile 保存用户主观时序偏移。UserPreferences 只保存已经实现的当前 profile ID，不另存原始音频设备 ID；谱面 offset、音频估算输出延迟和判定规则不进入这些 profile。最小 AudioDeviceProfile 在阶段 6 随 Player 的真实音频消费者首次实现；InputProfile 与 CalibrationProfile 在阶段 11 首次实现，在此之前不得为其添加持久化占位字段。

派生资源与 Shader 目标格式的唯一权威是阶段 5 定义的版本化 ImporterProfile / ShaderTargetProfile，ProjectConfig 只引用其 ID。DeviceProfile 只能声明兼容 ID 或能力约束，不能复制格式字段；项目所选目标与设备约束不兼容时必须稳定失败，不得在运行时静默改写项目配置、重新导入或切换缓存身份。

配置整合遵循以下规则：

```text
只有已经存在消费模块和明确行为的字段才能进入配置，不添加空占位字段
ProjectConfig 的确定性内容不能被 UserPreferences 隐式覆盖
DeviceProfile 依据 PreflightCapabilities 匹配，并提供能力要求与硬预算约束，不是探测结果副本或普通的最后写入覆盖层；UserPreferences 不参与选择 DeviceProfile
LaunchOptions 只提供命名明确的覆盖，不提供任意 key/value 注入
每个持久化格式必须定义 format、version、Schema、迁移和未知字段策略
所有可写持久化文件必须原子替换，写入失败时保留上一有效文件
损坏或版本不支持的 ProjectConfig 必须使项目加载失败，不能用默认项目静默替代
损坏的 UserPreferences 可以保留诊断后回退安全默认值；损坏的 DeviceProfile 只能失败或回退到显式命名的内建 profile
每类所选 profile 的缺失、损坏、未来版本和回退策略必须在首次消费阶段冻结；不得静默替换已经显式选择的 profile，最终选中身份必须进入来源诊断和对应运行时快照
每个字段必须区分启动期静态、可动态应用或需要重建 Session/Backend
解析结果记录来源和最终有效值，但日志不得泄露本机敏感路径或数据
配置默认值只有一个代码来源，应用层不得重复抄写模块默认值
每次新增持久化字段必须在同一阶段覆盖默认、解析、范围、版本迁移、未知字段和来源追踪测试
影响确定性结果的配置必须进入 ResolvedSessionConfig，运行中不得回读可变 UserPreferences
```

Filesystem Player/Studio 的推荐启动解析顺序：

```text
解析最小 LaunchOptions 以定位项目
-> 加载、校验并迁移 ProjectConfig
-> 探测创建前可知的 PreflightCapabilities；阶段 9 前使用模块保守默认值，阶段 9 后选择匹配的 DeviceProfile
-> 加载对应应用的 UserPreferences
-> 生成各模块 typed config
-> 应用 PreflightCapabilities、可用 DeviceProfile 与安全上限，产生只含已校验请求值的 ResolvedAppConfig
-> 记录配置来源和请求值
-> 创建 Window、RenderBackend 与 Audio
-> 从各子系统收集 EffectiveSettings，记录创建后实际能力、生效值、回退和与请求值的差异
-> 校验实际能力与所选 DeviceProfile 的硬要求和允许回退；不满足时初始化失败，不静默重选 profile 或循环重建子系统
-> 校验通过后创建 ContentProvider、AssetDatabase 与 PlaybackSession
```

嵌入宿主使用等价顺序，但可以用 typed/memory source 替代物理 Project 定位，用 HostCapabilities 替代应用平台探测，并跳过未选择的 SDL/OpenGL adapter。无论入口如何，PlaybackSession 创建前都必须完成相同的格式、预算、来源和确定性配置校验。

Chart、Behavior、Animation、Material、Shader 和 Particle 的领域参数仍属于各自版本化文档或资产，不得为了复用配置加载器而迁入 ProjectConfig 或 UserPreferences。结构化解析基础可以共享，Schema、迁移和所有权不能共享成无边界的通用字典。

阶段 1B 的实际配置整合止于应用侧 `cuexis_project` 前端、`cuexis.project.json`、每资产根独立 `cuexis.asset-index.json` 和 `{root, path}` bootstrap locator。加载器输出 typed `PreparedProject`，不向 Runtime/Assets 公共接口暴露 JSON DOM；引擎模块仍不读取配置文件。阶段 1C 不新增持久化配置格式，时间与 Behavior 参数继续属于 Chart/Behavior 数据。阶段 1D 只增加内存 typed AudioConfig 与只读 EffectiveAudioSettings；主音乐 AssetId 属于版本化 Chart/Asset 内容，不属于 ProjectConfig 或设备偏好。

### 阶段 0：工程骨架，0-2 个月

状态：实现完成。本文保留任务和验收标准作为阶段边界；具体工作区、编译器和显卡环境中的实际执行结果由阶段完成报告记录，不在本指南中固化。

目标：建立长期可维护的 C++ 工程基础。

任务：

```text
建立 CMake 工程
加入 Apache-2.0 官方 LICENSE 与 THIRD_PARTY_NOTICES
建立 CMakePresets.json
建立 vcpkg.json
建立 vcpkg-configuration.json 并固定 baseline
接入 SDL3
接入 EnTT
接入 spdlog / fmt
接入 tl-expected，并建立 cuexis::core::Result / Error
建立 cuexis_core
建立 cuexis_platform_sdl
建立 cuexis_world
建立 cuexis_render
建立 cuexis_render_opengl
建立 cuexis_player
接入 Catch2 v3 和 CTest
建立 cuexis_core_tests 并至少覆盖版本号生成
建立统一格式配置、警告基线和线程 Debug 断言
创建 SDL3 Window
创建 OpenGL Context
实现版本号系统
启动日志打印 Cuexis 版本
```

阶段 0 交付时的落地范围（当前正式构建已由阶段 1A 扩展）：

```text
阶段 0 边界内激活 cuexis_core、cuexis_platform_sdl、cuexis_world、cuexis_render、cuexis_render_opengl 和 cuexis_player
Core 提供 Result/Error、日志封装和 ThreadChecker，不依赖 SDL/OpenGL
Platform 以 RAII Runtime、Window 和 Window Lease 管理 SDL 主线程及生命周期
World 以受线程检查的回调访问 EnTT Registry，禁止返回 Registry 内部指针或引用
Render 前端只定义最小帧契约；OpenGL 3.3 Core Context 和调用封装在 cuexis_render_opengl
Player 组合窗口、World 和 OpenGL Backend，并提供严格渲染三帧的 --smoke-test
Catch2/CTest、格式检查、架构扫描和 Player 失败路径均有标准构建入口
```

验收标准：

```text
Cuexis Player 可以启动窗口
日志正常输出
版本号正常显示
Core 不依赖 SDL/OpenGL
OpenGL 调用只存在于 cuexis_render_opengl
项目可通过 preset 构建
CTest 可以发现并运行阶段 0 单元测试
vcpkg manifest 与 baseline 配置完整
```

首阶段只要求 Windows/MSVC 构建配置。`Cuexis Studio` 和 ImGui 集成可以保留规划位置，但不阻塞阶段 0 验收。

#### SDK 转型追溯要求（阶段 1E 前必须补齐）

根据 ADR 0027 与 SDK 转型方案 §11.1，阶段 0 的基础工程必须在阶段 1E 交付前追溯完成以下 SDK 导向的改造：

```text
把 app 构建改为可选，通过 CMake 组件选项控制
建立 install/export/package 基础（CuexisTargets.cmake、CuexisConfig.cmake）
定义 public/private header 界限和符号可见性规则
架构扫描增加"安装后公共 SDK 头不得包含 EnTT、SDL、OpenGL、JSON DOM 或实现类型"
增加独立 external consumer fixture，不直接访问仓库私有头
日志初始化从 SDK 全局行为移到应用或宿主注入
```

这些改造不重写 Core、World 或 OpenGL Backend；它们从"完整引擎基础"重新定位为 SDK 内部模块与可选参考后端。具体验收在阶段 1E 执行。

### 阶段 1：最小 Playback SDK，2-5 个月（相对规模）

目标：从谱面数据创建内部 Entity，以时间驱动表现，并在 1E 形成可由仓库外工程消费的 headless Playback SDK。阶段按顺序拆分；每个子阶段均须保留可运行 demo 和独立测试，未完成后续子阶段不阻塞前一子阶段验收。

#### 阶段 1A：规范谱面与实例化闭环

状态：实现完成。实际 Debug/Release、格式、架构和 GPU 验证结果以[阶段 1A 完成报告](stage_reports/stage_1a_completion_report.md)为准。

```text
在编码前完成配置 ADR，确认跨阶段配置分类、所有权、覆盖/约束关系、失败与诊断原则、按消费阶段冻结格式的规则，以及阶段 1B ProjectConfig 的文件定位和格式身份
随 Chart JSON 解析建立可复用的结构化读取与字段路径诊断基础，但不实现无类型递归配置合并
定义 TransformComponent、HierarchyComponent、RenderableComponent、BehaviorComponent 和 NoteTag / ElementTag
建立 cuexis_chart、cuexis_runtime、cuexis_behavior、cuexis_gameplay 和最小 cuexis_assets
实现方案 A 统一 objects/components 最小 Schema、有理数 Beat、基础 TimingMap 和 SimpleChartImporter
实现 ChartRuntime 编译、ChartWorldInstantiator、RuntimeSession 最小生命周期和 NullInput / NullJudge
实现 RenderScene / RenderCommand、OpenGL 基础渲染、DebugDraw 坐标轴和 NullClock
```

验收标准：

```text
配置 ADR 已接受，跨阶段职责边界、失败原则、各格式首次冻结阶段和 ProjectConfig 的近期落点明确，且未提前冻结阶段 6/9/11 的具体 Schema
结构化读取对缺失字段、类型错误、非法版本和未知字段产生带字段路径的确定性诊断
可以加载方案 A chart JSON，且编译结果不依赖 objects 数组顺序
方案 B 基础谱面可以确定性转换为方案 A
可以显示多个具有父子 Transform 的 3D Entity
业务代码不直接调用 OpenGL
```

当前落地范围：

```text
ChartLoader 按显式 format 路由方案 A/B，并以 typed Reader、字段路径诊断和语义校验生成 ChartDocument
原生方案 A 使用 UUIDv7；方案 B 以 chartId 命名空间确定性生成 UUIDv5
Schema artifact 与 JSON Schema adapter 已建立并独立测试；当前 loader 不直接调用 Schema validator
ChartCompiler 输出按稳定 ID 排序的 ChartRuntime；RuntimeSession 支持 prepare/commit/unload/reload
World 原子更新父子世界矩阵；A/B 示例均创建 root -> child -> grandchild 三对象层级
阶段 1A 交付时 Player 默认加载 canonical 示例，也可通过 --chart 加载 simple 示例，并由 DebugDraw 为每个 Transform 输出 XYZ 轴线；当前默认入口已由阶段 1B Project 替代
外部 Renderable 资源在阶段 1A 实例化时明确失败；资源句柄与加载生命周期属于阶段 1B
Behavior track 在阶段 1A 只作为 opaque 数据保留；采样和 Transform 驱动属于阶段 1C
默认 Chart 解析限制包括 16 MiB 输入、64 层嵌套和单个 JSON key/string 1 MiB
```

#### 阶段 1B：资源生命周期闭环

状态：实现与验收完成。详细交付、Debug/Release 验证、6 组 GPU smoke 和残余风险见[阶段 1B 完成报告](stage_reports/stage_1b_completion_report.md)；原实施批次与边界保留在[阶段 1B 实施计划](stage_plans/stage_1b_implementation_plan.md)。ProjectConfig v1 与路径安全遵循 ADR 0025，AssetId 来源与 `entry.chart` bootstrap locator 遵循 ADR 0026。

```text
实现最小 ProjectConfig v1 及其加载器，具体文件名和 format ID 遵循已接受的配置 ADR
ProjectConfig 第一版只包含项目身份、资产根、最小入口内容和扩展区，不包含用户偏好或设备预算
规范化并校验资产根和项目内路径，由应用组合层把解析结果交给 AssetDatabase
实现基础 Mesh / Material / Texture Handle
实现同步 ResourceManager、ResourceLease、ResourceScope 和 Required / Fallback / Optional 策略
实现资源诊断、Scope 释放和 generation 失效测试
```

验收标准：

```text
有效 ProjectConfig 可以建立 AssetDatabase 并定位阶段 1 demo 的入口内容
缺失、损坏、版本不支持、重复资产根和越界路径均产生稳定诊断，不发布半初始化项目
ProjectConfig 不保存用户目录、窗口位置、音频设备等本机偏好
资源缺失按引用策略产生确定结果
销毁 RuntimeSession 后 Scope 释放全部强引用，旧 Handle 不会指向复用资源
```

当前落地范围：

```text
cuexis_project 提供 ProjectConfig/Asset Index typed Reader、固定定位、路径与物理 containment 校验和显式原子保存
每个资产根以独立 cuexis.asset-index.json 建立不可变 AssetDatabase；目录枚举不参与 AssetId 发现
ResourceManager 同步加载有界 Mesh/Material/Texture CPU blob，并实现 manager token、generation、contentRevision、Lease 和 Scope
RuntimeSession 在临时 Scope 中解析直接/传递依赖，再发布带 typed Handle 的 World；失败 prepare/reload 不改变活动状态
Player 默认加载阶段 1B project fixture，--project 与 --chart 互斥，并保留阶段 1A 方案 A/B 无资源回归入口
阶段 1B 仍使用 DebugDraw 输出；正式资源内容格式、Importer、GPU 派生对象、异步加载、LRU 和文件监听不在本阶段
```

#### 阶段 1C：时间、基础行为与 Headless Playback 闭环

状态：实现与最终验收完成。`behavior.transform.keyframe` v1、RuntimeFrame、PlaybackSession 和 Player 迁移按[阶段 1C 实施计划](stage_plans/stage_1c_implementation_plan.md)与 ADR 0028/0029 落地；[260722 全量审查](stage_reports/260722-1c-review.md)的 R01-R21 已全部关闭。原完成报告正文保留 2026-07-22 执行数据，第 10 节记录 2026-07-27 最终关闭证据。

```text
实现基础 BehaviorSystem 与 Transform Keyframe（已完成）
以 chartTimeMs 驱动位置、旋转、缩放和 camera.fovY，并支持绝对时间重采样（已完成）
建立第一版不暴露 RuntimeSession/World/EnTT 的 PlaybackSession 门面（已完成）
支持宿主直接提交 RuntimeFrame，并输出不依赖 SDL/OpenGL 的 FrameSnapshot（已完成）
Player 改为 PlaybackSession 的薄组合层；确定性 smoke 使用 stage1c_project（已完成）
```

验收标准（实现部分已完成，门禁结果见阶段报告）：

```text
Entity 可以随 chartTimeMs 移动、旋转、缩放
Seek 或模拟时钟跳转后 Runtime 从目标时间重新求值，不依赖上一帧状态
Timing、BPM 和 Behavior 参数保存在 Chart/Behavior 数据中，不进入全局项目或用户配置
关闭 SDL/OpenGL 时，headless consumer 可以完成 load/update/seek/extract/unload
Player 与 headless consumer 对相同 Chart/RuntimeFrame 产生相同帧结果
```

#### 阶段 1D：主音乐内容与可选音频适配器闭环

状态：1D-0 至 1D-6 已实现并完成本地自动化、GPU 与物理默认音频设备脚本门禁。Chart/Asset
Index v2、三种时钟模式、Prepared Playback、音频所有权与 reload 错误边界由 ADR 0031/0032
冻结并落地，继续以 1C 的 RuntimeFrame/绝对重采样为基础。详见
[阶段 1D 实施计划](stage_plans/stage_1d_implementation_plan.md)和
[阶段 1D 完成报告](stage_reports/stage_1d_completion_report.md)。

```text
建立 cuexis_audio 和 cuexis_audio_sdl
实现 cuexis_audio 的 SourceClockSample，以及 cuexis_playback 的 RuntimeTimeline
定义后端无关的 AudioConfig typed config，明确缓冲、音量、设备请求和输出延迟诊断的语义边界
阶段 1D 只实现默认配置和显式内存注入；设备偏好延后到阶段 6，输出/输入/主观校准分别由专用 profile 持有
实现 WAV 主音乐播放、暂停、停止、Seek、AudioClockSnapshot 和 discontinuity
PlaybackSession Prepared load/reload 内部持有 AudioSourceLease，只公开 MainMusicSourceView
cuexis_playback 依赖 cuexis_audio，但不依赖 cuexis_audio_sdl；正式支持 ChartClock、HostClock 和 CuexisAudio
导出 Cuexis::Audio 与可选 Cuexis::AudioSDL，并将 preview SDK API 提升到 0.2.0
```

验收标准：

```text
AudioConfig 在创建 AudioTransport 前完成校验，非法值不会留下半初始化设备
cuexis_audio 公共配置不暴露 SDL 类型或不稳定的后端句柄
主音乐可以驱动 chartTimeMs，且 Seek 后 Runtime 能重新求值
AudioClock 的位置、估算输出延迟、播放状态和不连续事件可在调试界面或日志中诊断
HostClock 与 CuexisAudio 对相同 SourceClockSample/control script 产生相同 RuntimeFrame 和表现结果
未选择 SDL adapter 的 SDK consumer 不链接或初始化 SDL Audio
```

#### 阶段 1E：SDK 封装与外部消费闭环

状态：实现与 Windows/MSVC、Windows/MinGW、Linux GCC/Clang 自动化验收完成。实施证据见[阶段 1E 实施计划](stage_plans/stage_1e_implementation_plan.md)和[阶段 1E 完成报告](stage_reports/stage_1e_completion_report.md)。

```text
实现 cuexis_playback 的正式安装公共头边界
实现 Filesystem、Memory 和 Host ContentProvider
完成 CMake install/export/package、组件化构建开关和版本查询
实现 `CUEXIS_LIBRARY_TYPE=STATIC|SHARED`、公共 binary 拓扑、导出宏与 runtime deployment
建立仓库外 add_subdirectory 与 find_package consumer
验证无 App、SDL、OpenGL 和物理音频设备的 headless 完整闭环
记录第一版 C++ SDK 兼容性、所有权、线程、错误和 shared 重编译政策
```

验收标准：

```text
外部工程只使用已安装公共头即可加载、更新、Seek、Reload、提取帧和卸载
公共头不暴露 EnTT、SDL、OpenGL、JSON DOM 或其他实现类型
多个 Session 可以使用独立 Clock 与 ContentProvider
销毁 Session 后不留下全局线程、设备、日志或宿主 Context
Player、headless 与 external consumer 的确定性结果一致
static/shared consumer 在干净部署目录中运行，且不泄漏内部符号或私有开发依赖
```

### 阶段 2：Cuexis Behavior 表达能力强化

状态：实现与 Windows/MSVC 非图形门禁已完成；GPU smoke 与 hosted Linux CI 待最终验收。精确证据见[阶段 2 完成报告](stage_reports/stage_2_completion_report.md)。

目标：让 Behavior 成为 Cuexis 谱面表现的核心表达能力，不扩张为宿主任意脚本或通用游戏逻辑系统。

任务：

```text
阶段 2A.1 已移除方案 B Loader/Importer/Schema/tests/Player fixture，只保留 canonical Chart
Chart v3 已交付版本化 Tempo/Stop、Behavior Event 和 Step Event Schema/typed Reader
TimingMap 已实现固定预算积分、Beat/时间逆映射、Stop 和负 Beat 语义
阶段 1C Keyframe v1/v2 采样保持不变，并由显式 migrator 转换为 v3 Event
Transform/Camera、Visibility、Material 资源/Opacity/Tint 已进入后端无关 Snapshot
PlaybackSession 已提供四个版本化 capability 的资源前 preflight
FrameDigest v2、内部有界调试快照和零分配 update/extract 门禁已交付
ParentBinding、局部 Beat、循环、多 Clip、priority/weight 和混合明确延期
Behavior 不执行宿主任意代码、脚本或无界回调
```

验收标准：

```text
行为可以独立采样
行为可以序列化
行为可以在任意 chartTimeMs 预览
行为错误有日志
曲线采样有单元测试
TimingMap 边界、Stop 区间和负 Beat 有单元测试
同一 Behavior 数据和显式采样输入产生相同结果，BehaviorSystem 不读取应用配置文件
外部 consumer 与 Player 对同一 Behavior 数据产生相同 FrameSnapshot
```

### 阶段 3：可移植表现前端与渲染适配，8-10 个月

目标：冻结宿主可消费的 FrameSnapshot/RenderPacket，并把 OpenGL 与特定宿主引擎彻底隔离为 adapter。

任务：

```text
稳定并扩展 RenderScene / RenderCommandList
冻结 FrameSnapshot/RenderPacket 所有权、有效期和大小预算
定义宿主 Camera/Viewport 输入与坐标转换契约
仅在 Cuexis 表现有真实消费者时定义 LightComponent
定义 PipelineDesc
定义 TextureDesc
定义 BufferDesc
定义 MaterialAsset
定义后端无关的 RenderConfig，区分请求值、实际有效值以及启动期静态/运行时动态字段
由宿主/应用 adapter 把 RenderConfig 映射为后端配置，业务与项目文件不暴露后端枚举
定义 Portable Presentation、Built-in Renderer 和 Host-specific capability 分层
建立不执行 GPU 绘制的验证 Sink
实现 OpaquePass
实现 TransparentPass
实现 DebugPass
实现基础排序
实现材质参数上传
```

验收标准：

```text
RenderSystem 只生成 RenderScene
OpenGL 只存在于 cuexis_render_opengl
材质系统不暴露 GLuint
Playback 公共头和 FrameSnapshot 不暴露任何图形 API 类型
透明对象有稳定绘制顺序
可以显示基础 debug geometry
无效 RenderConfig 在创建 Backend 前失败；有效配置可查询并诊断实际生效值
切换有效 RenderConfig 请求只影响渲染子系统，不改变 Chart、Behavior 或 World 数据
宿主能力不足时稳定失败或执行项目显式允许的受控降级
```

### 阶段 4：Cuexis 表现动画系统，10-12 个月

目标：补充 Cuexis 谱面和资源预览所需动画，不建设通用角色状态机或游戏对象脚本系统。

任务：

```text
实现 AnimationClip
实现 AnimatorComponent
实现 AnimationSystem
复用 Curve / Track / Sampler
支持 Transform 动画
支持 Material 参数动画
支持播放、暂停、循环
实现 Animation Layer、BlendGroup、weight 和 property mask
实现 HostOverride 与 StudioPreviewOverride Token
实现确定性 Override / Additive 混合
Animation 参数保存在 AnimationClip/Animator 数据中；只有安全上限和诊断策略可以进入 typed config
```

验收标准：

```text
AnimationSystem 不依赖 Chart
BehaviorSystem 和 AnimationSystem 可以同时作用
冲突属性通过 PropertyResolver 按显式混合模式求值
相同输入与权重在不同 Entity 遍历顺序下产生相同结果
Host Override 结束后属性恢复到下层求值结果
动画可在调试面板查看
相同 Animation/Animator 数据和显式求值输入产生相同结果，AnimationSystem 不读取应用配置文件
宿主通过稳定 ID/OverrideToken 操作，不访问 World 或最终 Component
```

### 阶段 5：材质与 Shader 管线，12-14 个月

目标：建立可由 Studio、Player 和宿主 adapter 消费的分层材质与 Shader 工作流；Shader Graph 不属于本阶段。

#### 阶段 5A：材质资产与参数

```text
实现 MaterialAsset、MaterialHandle、参数 Schema 和 RenderState
实现材质参数上传、默认 Shader 引用和运行时材质预览
定义跨宿主最低 Portable Material Schema
```

#### 阶段 5B：ShaderAsset 与跨目标编译

```text
接入 shaderc/glslang、SPIRV-Tools 和 SPIRV-Cross
实现 GLSL 450 -> SPIR-V -> GLSL 330/ES 300 管线
实现 Reflection、声明式 Variant、Binding 与属性 Schema 校验
定义版本化 ImporterProfile / ShaderTargetProfile，作为派生资源与 Shader 目标格式的唯一权威；ProjectConfig 只引用 profile ID，不复制后端编译细节
允许 Built-in Renderer 与 Host-specific Extension 显式声明高级能力
```

#### 阶段 5C：缓存、诊断与热重载

```text
实现 Variant 缓存键、导入缓存、完整编译诊断和失败回退
将 profile、工具版本和有效目标能力纳入缓存键，禁止运行时任意字符串宏覆盖
实现 Worker 编译与 Render safe point Pipeline 替换
```

验收标准：

```text
修改材质参数可实时预览
Shader 编译失败不会导致程序崩溃，并保留上一有效 Pipeline
ShaderAsset 不绑定 OpenGL 专有概念
目标 Shader 在 GLSL 330、GLSL ES 300 和 SPIR-V 验证通过
材质资产可以被谱面和 Entity 引用，并向 Studio Inspector 提供反射数据
宿主 adapter 显式报告 capability，不承诺自动转换任意 ShaderAsset
profile 不存在或版本不支持时导入失败；修改 profile 只失效受影响的派生缓存
派生资产和 Shader 缓存记录规范化 profile 身份；后续 DeviceProfile 只能校验兼容 ID/约束，不能改变格式或复用不兼容缓存
```

### 阶段 6：Playback C++ API 与独立 Player 产品化，14-16 个月

目标：在阶段 1E 的外部消费基础上稳定 C++ 使用、弃用和升级政策，同时把独立 Player 产品化；Player 继续是参考应用，但不是唯一 SDK 消费者。本阶段不在必选 Judgement/Replay 之前冻结 C ABI。

任务：

```text
稳定 PlaybackSession C++ 使用、兼容性和弃用政策
在已支持的 shared preview 基础上继续验证真实宿主、C++ 升级和弃用政策，但不冻结 opaque handle C ABI
稳定并扩展 Cuexis Player 应用层组合、主循环和播放器生命周期
实现配置解析/组合层，加载 ProjectConfig、Player UserPreferences、命名明确的 LaunchOptions 和 PreflightCapabilities；DeviceProfile 尚未定型时使用模块保守默认值
在 Window、RenderBackend、Audio、ContentProvider、AssetDatabase 和 PlaybackSession 创建前生成不可变 ResolvedAppConfig，并为每次播放派生最小 ResolvedSessionConfig
实现用户配置目录、原子写入、损坏文件诊断/安全回退、版本迁移和配置来源日志
作为首个持久化音频设备偏好消费者，定义并实现最小版本化 AudioDeviceProfile，作为输出设备匹配身份与输出校准的唯一持久化所有者；Player UserPreferences 只引用其 ID，不另存设备 ID，也不预留 InputProfile 或 CalibrationProfile 字段
UserPreferences 缺失或安全重置时可采用文档化的具名内建 AudioDeviceProfile ID；已经显式选择的 profile 缺失、损坏、版本过新或无法匹配当前设备时音频初始化失败，不静默切换设备或校准
子系统创建后收集 EffectiveSettings 并校验实际能力、协商回退和模块硬要求；不满足时稳定结束初始化，不把实际值伪装成创建前能力
定义静态设置重启/重建设备规则，以及音量、诊断开关等动态设置的显式应用路径
将阶段 1D 已有的谱面与主音乐加载、卸载、播放、暂停、停止、Seek 和 Reload 能力接入正式用户入口
只通过 PlaybackSession 接入 Timeline、Renderer、材质/Shader 与调试信息展示
完善安装包、许可证、符号、Debug/Release 和升级说明
建立至少一个真实宿主适配证明，但不把宿主 SDK 带入 Playback 核心
建立最小示例谱面、资源与 Player 冒烟测试流程
```

验收标准：

```text
Player 只加载 canonical Chart，并可播放、暂停、Seek 和 Reload
Player、Studio 预览和宿主使用唯一的 PlaybackSession -> internal RuntimeSession 路径
加载失败、资源降级、音频不连续和 Shader 错误可诊断
Player 可以从 ProjectConfig 启动，并可用 UserPreferences 和受控 CLI 选项覆盖允许的表现层字段
损坏或未来版本的用户设置不会破坏项目文件，且可回退到单一来源的安全默认值
AudioDeviceProfile 的缺失设备、损坏版本、非法校准值和具名内建默认均有稳定诊断；显式选择失效时音频初始化失败，输入绑定和主观时序校准尚未进入 Player 持久化格式
启动日志可以在子系统创建前显示非敏感的配置来源/请求值，并在创建后显示 EffectiveSettings 和回退差异
相同来源生成的 ResolvedSessionConfig 具有可复现的规范化身份或内容 hash；创建 Session 后修改 UserPreferences 文件不会改变活动 Session 或确定性结果
动态字段只经显式 apply 路径生效，静态字段只在明确重建 Session/Backend 后生效；UserPreferences 的允许映射不会修改 Chart、Behavior、Animation 或 World 领域数据
不引入 Studio、编辑器文档或编辑器 UI 依赖
文档和包元数据明确本阶段仍是 C++ 源码兼容边界，不宣称稳定 C ABI
```

### 阶段 7：Cuexis Studio 核心，16-20 个月

目标：建立可用的内部编辑器。

任务：

```text
实现 EditorDocument
实现 Hierarchy Panel
实现 Inspector Panel
实现 Viewport Panel
实现 Timeline Panel 初版
实现 Asset Browser
实现选择系统
实现 Undo / Redo 命令系统
实现 ChartDocument 保存和加载
复用 SDK/Player 的 ProjectConfig 解析和迁移，不建立第二套配置 Schema 或加载器
实现 StudioPreferences，保存布局、最近项目、自动保存和快捷键等本机设置
实现项目设置与用户偏好的分区编辑、校验、原子保存、迁移备份和未知扩展保留
只通过 PlaybackSession 实现运行时预览
接入材质参数与 Shader 编译诊断编辑界面
```

验收标准：

```text
编辑器操作 ChartDocument，不直接操作 Runtime Entity
支持保存和重新加载
支持撤销重做
支持拖动时间预览
Viewport、Player 和 external consumer 使用同一 PlaybackSession/Runtime 路径
可编辑并预览材质参数，且可显示 Shader 编译错误
Studio 与 Player 对同一 ProjectConfig 产生相同项目解析结果
StudioPreferences 不写入 ProjectConfig，编辑器布局和本机路径不会污染项目仓库
配置写入失败时上一有效文件保持不变；迁移失败恢复备份并报告诊断
ProjectConfig 的未知可选扩展经过 Studio 读取和保存后可以往返保留
Studio 显示目标宿主 Profile 与不兼容表现能力
```

### 阶段 8：可选 Cuexis 粒子表现扩展，20-22 个月

目标：在已有材质、Shader 与 Studio 基础上，实现可编辑、可确定性恢复并可由宿主消费的 Cuexis CPU 粒子表现，不建设通用游戏粒子引擎。

#### 阶段 8A：基础发射器与渲染

```text
定义 ParticleEmitterAsset、ParticleEmitterComponent、ParticleSystem 和 ParticleRenderPacket
实现 CPU 粒子、Billboard 渲染、生命周期颜色/大小、速度和重力参数
```

#### 阶段 8B：确定性时间轴

```text
实现 120Hz 固定步长、版本化随机种子、Checkpoint、LRU、正向重放 Seek 和 Rebuilding 状态
```

#### 阶段 8C：编辑、调试与预算

```text
实现粒子调试面板和 Studio 发射器参数编辑
记录粒子数量、Checkpoint 内存和重建耗时
定义粒子 Checkpoint 与单帧重建预算的 typed config 和诊断，但具体默认预算等待阶段 9A 测量
```

验收标准：

```text
Entity 可以挂载并编辑粒子发射器，粒子可以跟随父级 Transform
粒子不直接依赖 OpenGL，渲染通过 FrameSnapshot 中的 ParticleRenderPacket
不同渲染帧率和 Checkpoint 布局产生相同目标粒子状态
暂停、倒放和 Audio Seek 后粒子能恢复正确状态
调整预算只影响重建耗时和缓存布局，不改变目标时刻的粒子结果
宿主不支持时按 capability 稳定失败或使用项目显式允许的受控降级
```

### 阶段 9A：SDK 与宿主性能验证，22-24 个月

目标：在桌面与外部 consumer 上完成 SDK、表现、资源与时间精度的测量闭环，为阶段 11 判定/回放和后续宿主适配提供依据。

任务：

```text
添加性能统计面板、帧时间分析、Entity 数量、DrawCall、粒子数量与资源内存统计
测量 PlaybackSession update/extract、FrameSnapshot 大小/复制和宿主回调成本
测量多 Session、ContentProvider、shared library/C ABI 数据交换开销
记录音频 underrun、AudioClock 输出延迟和时钟稳定性
实现并测量输入事件时间戳到 Timeline 的映射
基于测量结果定义版本化 DesktopDeviceProfile，建立可配置的 CPU、GPU、音频、粒子 Checkpoint 和瞬时上传预算，并声明兼容的 ImporterProfile/ShaderTargetProfile ID 或能力约束
区分硬预算、软目标和用户画质偏好，记录 profile 来源、能力探测与实际有效值
```

验收标准：

```text
性能面板可显示核心指标并可导出或记录诊断
为目标桌面设备记录帧时间、内存、音频与输入时间基线
判定时间链路可使用事件发生时间而非处理帧时间
DesktopDeviceProfile 的默认值有测量依据，越过硬预算会产生确定性降级或加载失败
同一 profile 可以由 Player、Studio 性能预览和兼容宿主共同使用
profile 版本不支持、能力不匹配和多个候选同优先级时产生确定性选择或稳定失败
ProjectConfig 引用的 ImporterProfile/ShaderTargetProfile 与 DesktopDeviceProfile 不兼容时稳定加载失败，不触发运行时重导入或缓存身份替换
硬预算、软目标和用户画质偏好的优先级有边界测试，EffectiveSettings 可说明裁剪来源
```

### 阶段 9B：Android SDK 与宿主适配验证（延期）

本阶段延期执行，不作为 Studio、性能验证或输入/判定设计的前置条件。目标是验证 Android SDK 构建、ContentProvider、宿主生命周期和可选内建 adapter，而非交付完整移动端游戏外壳。详见 `docs/MOBILE_STRATEGY.md`。

恢复条件与任务：

```text
建立 Android Playback SDK 构建验证；内建渲染 adapter 以 OpenGL ES 3.0 为最低图形能力
在阶段 5 的 ImporterProfile/ShaderTargetProfile 中新增 KTX2/Basis Universal、meshoptimizer、Ogg Vorbis 和 GLSL ES 300 目标，并定义只引用兼容目标 ID/约束与预算的版本化 Android DeviceProfile
根据 PreflightCapabilities 匹配选择受控 Android DeviceProfile，并复用阶段 6 的 AudioDeviceProfile 保存输出设备校准
触摸 InputProfile 和用户主观时序 CalibrationProfile 的持久化整合以阶段 11 完成为前置条件；若阶段 9B 先执行，只验证原始输入时间戳与延迟链路，不提前定义或保存这两类格式
验证 APK/AAB/AssetManager ContentProvider、后台恢复、Context 丢失、内存压力和真实设备预算
```

验收标准：

```text
Android DeviceProfile 可以复现兼容目标约束、预算和 profile 匹配选择，ProjectConfig 的目标不兼容时稳定失败，且用户偏好不能突破硬件硬上限
宿主不使用内建 renderer/audio 时可以只构建和运行对应 headless/host adapter 组件
不支持或损坏的 profile 产生可诊断失败，不静默使用不受控预算
AudioDeviceProfile 校准值与输出设备身份绑定；阶段 11 已完成时，InputProfile/CalibrationProfile 也按其既有 Schema 完成移动端整合，且不与谱面 offset 或判定规则混为一个常数
```

### 阶段 10：Vulkan 可行性验证（延期）

本阶段延期执行，不作为当前功能路线的前置条件。目标是验证可选内建渲染 adapter 是否适合 Vulkan，而不是改变 Playback SDK 或正式替换宿主渲染器。

任务：

```text
梳理 RenderBackend 接口
检查 PipelineDesc 是否足够表达需求
检查 BindingSet 设计
检查 ShaderAsset 到 SPIR-V 的路径
仅在实验性 LaunchOptions/RenderConfig 中验证 auto/opengl/vulkan 后端请求、能力检查、回退诊断和实际有效后端；Vulkan ADR 接受前不写入正式 ProjectConfig
后端专用实验选项使用隔离命名空间，不进入 Chart、Behavior、World 或通用 Material 数据
实现最小 VulkanBackend 原型，若时间允许
渲染一个三角形或基础 Mesh
记录架构缺陷
```

验收标准：

```text
明确哪些接口需要调整
明确 OpenGL 后端哪些实现是假抽象
Chart / Behavior / World 不需要修改
形成 Vulkan ADR 文档
切换后端只影响应用/宿主 adapter、Render 前端/后端和派生 Shader 缓存，不要求迁移项目内容
FrameSnapshot/RenderPacket、Chart、Behavior、World 和 Judgement 不需要修改
请求后端不可用时产生稳定诊断；是否允许回退由显式配置决定
```

### 阶段 11：输入、判定、计分与确定性回放

前置条件：完成阶段 7 Studio 核心和阶段 9A 性能验证。目标是交付 Playback SDK 必选的 `cuexis_judgement`，由宿主提交标准化输入，SDK 计算并返回判定、分数、连击和统计，同时记录和回放 InputEvent，并据此完成稳定 ABI 前所需的最终 C++ 公共生命周期。完整玩法状态机、UI 和游戏流程仍由宿主拥有。

任务：

```text
定义带单调时间戳、来源、arrival time 和 sequence 的 InputEvent / InputFrame
定义事件时间到 Timeline 的映射，并分离输出延迟、输入延迟、主观校准和谱面 offset
定义版本化 InputProfile、CalibrationProfile 和 JudgementConfigSnapshot
实现 cuexis_judgement：Tap/Miss、JudgementEvent、Score、Combo 和 Statistics
PlaybackSession 接收 RuntimeFrame + InputEvent，并发布累积 JudgementResult 快照
定义宿主可查询的 Note/Event 时间流和稳定 ObjectId
实现 startRecording/stopRecording，完整记录 InputEvent、chartTimeMs 和 frameIndex
定义版本化 ReplayData，包含事件序列和确定性 Session 配置快照
实现 loadReplay，回放模式替代实时输入并拒绝混合提交
为 Hold、Slide、多指规则和宿主自定义判定保留版本化扩展点
```

`arrivalTime` 与 `frameIndex` 是审计/诊断元数据，不能决定判定或回放采样结果；事件语义顺序由规范化事件时间、chartTimeMs 和 sequence 决定。JudgementResult 与 ReplayData 必须有输入/事件/字节预算，`extractResult` 不得在每帧复制无界历史。

验收标准：

```text
相同 Chart、InputEvent、校准和配置产生确定一致的判定、分数、连击和统计
Judgement/Replay 不暴露 World、EnTT、SDL、音频/渲染后端或宿主引擎类型
纯播放和 Studio 预览不注入 InputEvent 时 Judgement 休眠
宿主在停止或运行期间可以取得有明确有效期的完整结果快照
记录事件与宿主提交的原始规范化 InputEvent 完全一致
实时播放与 Replay 模式的 JudgementResult 和 FrameSnapshot 确定一致
ReplayData 序列化/反序列化往返保持结果，损坏或不支持版本稳定失败
Replay 配置快照与活动 Session 不一致时加载失败并给出稳定诊断
回放模式提交实时 InputEvent 时返回明确错误且不部分应用
```

### 阶段 12：稳定 ABI 与正式 Playback SDK v1

前置条件：阶段 11 的 Input/Judgement/Replay 公共契约、实现、external consumer 和确定性回放门禁全部完成。

```text
冻结 opaque handle C ABI、allocator、字符串、数组、回调与快照有效期及长期二进制兼容政策
定义独立 C ABI version、符号可见性、兼容/弃用和 capability 查询
验证 Windows CRT、Debug/Release、static/shared 与支持平台矩阵
提供薄 C++ RAII wrapper 和至少一个正式宿主 adapter
发布完整 Playback SDK v1 集成、升级、许可证和部署文档
```

项目显示版本、C++ SDK API 版本、C ABI 版本与 Chart/Project/Asset/ReplayData 内容格式版本保持独立；任一版本变化不得隐式升级其他版本。

## 31. 风险控制

主要风险：

```text
架构过度设计导致长期无可运行成果
渲染抽象过于贴近 OpenGL
行为系统表达力强但不可调试
编辑器直接操作 Runtime Entity
资源系统过晚建立导致路径和生命周期混乱
Runtime 聚合过多职责并演变为 God Object
PlaybackSession 聚合过多职责或泄漏 Runtime/World
宿主 VFS 绕过路径安全（ContentProvider 打开任意路径或跳过预算/格式校验）
三种 Clock 时间语义分裂（必须先归一化为 SourceClockSample，再由同一 RuntimeTimeline 生成 RuntimeFrame）
Player 与 SDK 行为分叉（Player 绕过 PlaybackSession 直接使用内部 Runtime 路径）
Studio 形成第二套 Runtime（Viewport 不使用同一 PlaybackSession 和编译路径）
为支持所有宿主过早过度抽象（未用真实 consumer 验证前就建立动态插件框架）
SDK 公共 API/ABI 在真实 consumer 前过早冻结
ContentProvider 或宿主回调引入不受控线程、重入和生命周期
宿主渲染差异导致 FrameSnapshot 与 Shader 能力承诺失真
ReplayData 输入质量影响回放可信度（记录丢失、时间戳漂移或宿主注入合成事件）
规范谱面与简易格式长期并行演进
音频后端的估算时钟无法满足后续判定精度
状态型粒子在时间轴跳转后无法确定性恢复
Vulkan 过早投入导致维护成本翻倍
谱面格式过早复杂化
```

应对原则：

```text
每个阶段都必须有可运行 demo
每个复杂系统都必须有调试面板
每个资产格式都必须有版本号
每个模块都必须能解释自己的边界
不为未来功能破坏当前闭环
不让临时代码穿透架构边界
每个 SDK 阶段都必须有 headless 与 external consumer 证据
Player、Studio 和宿主必须使用同一 PlaybackSession 路径
```

## 32. 当前最高优先级

阶段 1A 与阶段 1B 已完成的实现闭环：

```text
配置 ADR 0024 已接受，并冻结阶段 1B ProjectConfig 的固定文件名 cuexis.project.json 与 format 身份 cuexis.project
Cuexis-owned JSON Value、typed Reader、字段路径诊断、解析预算和独立 JSON Schema adapter
方案 A/B loader、有理数 Beat、基础 TimingMap、模板展开、确定性 UUIDv5 导入和 ChartRuntime 编译
ChartWorldInstantiator、事务式 RuntimeSession、父子 Transform 与稳定 Object/Entity 映射
RenderScene/RenderCommand、DebugDraw XYZ 轴线和 OpenGL 3.3 Debug Line 管线
Cuexis Player 的阶段 1A canonical/simple --chart 回归入口与严格三帧冒烟能力
阶段 1A targets 的 Catch2/CTest、失败路径、架构扫描、格式和警告基线
ProjectConfig/Asset Index v1、固定文件定位、portable path 与物理 containment、原子保存和独立 cuexis.asset-index.json
AssetDatabase、同步有界 CPU blob ResourceManager、typed Handle、manager token、generation/contentRevision、Lease/Scope 和三种引用策略
RuntimeSession 资源事务、Session/Manager owner 校验、活动 Diagnostics、World -> Scope 销毁顺序和 reload 回滚
Player 默认阶段 1B Project、--project/--chart 互斥、3 个 Renderable 和 Mesh/Material/Texture 依赖 demo
```

上述方案 A/B loader 与 canonical/simple 回归入口是阶段 1 的历史完成事实；ADR 0035 已在阶段 2A.1 删除方案 B，不能把它们继续作为阶段 2 之后的验收要求。

阶段 1C、阶段 1D 与阶段 1E 已完成最终验收；阶段 1E 的 `0.3.0` static/shared Playback Core preview 已通过 Windows/MSVC、Windows/MinGW 和 Linux GCC/Clang 自动化矩阵。阶段 2 已把 preview API 提升到 `0.4.0`，实现 Chart v3、Tempo/Stop、Behavior/Step Event、Visibility/Material Snapshot、capability、FrameDigest v2 和迁移工具，并通过本地 Windows/MSVC static/shared Debug/Release、headless、format、architecture 与 external consumer 门禁。当前最高优先级是补齐 GPU smoke 和 hosted Linux CI 后关闭阶段 2 最终验收；随后进入阶段 3。稳定 C ABI 必须在正式 Judgement/Replay 公共契约完成后再冻结。

每次交付仍须按 `docs/BUILDING.md` 在目标环境执行 Debug 配置、构建、CTest、格式检查和图形冒烟；Chart 回归只使用 canonical 输入，并保留 retired Simple 的 unsupported-format 测试。Release 或后端相关改动还须验证 Release。精确结果记录在对应阶段报告，不固化在本指南中。

当前不应投入：

```text
阶段 11 的正式判定、计分和 ReplayData
宿主原始输入设备管理和复杂游戏流程
稳定 C ABI、Unity/Unreal 官方插件
Shader Graph
正式 Vulkan 后端
复杂编辑器
复杂粒子
移动端完整发布
```

## 33. 已决策边界与实施期复核

ADR 0027 已冻结 Playback SDK + 独立 Player + 独立 Studio 的产品边界、headless 要求、宿主职责、Judgement/Replay 必选交付和阶段 1E。ADR 0024 继续冻结跨阶段配置规则；ADR 0025 与阶段 1B 实现冻结 ProjectConfig v1 文件/Schema/路径安全，ADR 0026 冻结 Asset Index v1；ADR 0031 新增 Asset Index/Chart v2 主音乐格式，ADR 0032 冻结三种 Clock、RuntimeTimeline 与 Prepared Playback；ADR 0033 冻结阶段 1E 的同工具链 C++ shared preview binary topology、重编译政策、依赖闭包与验收边界。SDK 转型不修改历史 v1 格式语义，而是在阶段 1E 增加 typed/memory source 与 ContentProvider。UserPreferences、DeviceProfile、Input/Calibration Profile、ReplayData Schema 和稳定 C ABI 仍按首次消费阶段确认。

以下细节有意延后到真实实现和测量数据出现后决定，它们不能用于改变现有模块边界：

```text
异步 ResourceManager 的任务、取消和优先级 API
PlaybackSession 的具体 C++ 函数、FrameSnapshot 内存布局和 C ABI handle
第一版真实宿主 adapter 目标
跨 Chart Object/Template 引用与 Template 子树
扩展处理器的动态插件 ABI
压缩音频的流式缓冲与设备热插拔细节
具体 Android 验收设备和 DeviceProfile 数值预算
ProjectConfig v1 之后的扩展字段与迁移版本，以及 UserPreferences、DeviceProfile 与设备/校准 Profile 在各自首次消费阶段才能确定的具体文件名、format ID、Schema 字段和迁移版本
ReplayData 的具体 format ID、Schema、迁移、大小预算和完整性校验
完整商店发布、签名和分发流程
```

实施发现现有决策无法满足需求时，必须先更新专项规范和 ADR，再修改公共接口。不得以“实现细节”为理由绕过已经确定的所有权、时间、后端或格式边界。

## 34. 项目原则总结

`Cuexis` 的核心工程原则：

```text
数据驱动优先
模块边界优先
调试能力优先
Playback SDK 是产品边界，Runtime/World 是内部实现
宿主拥有主循环、原始输入设备、游戏状态和 UI
Judgement/Replay 属于 SDK，但不依赖平台和后端
headless 与外部 consumer 是正式门禁
运行时数据和编辑器文档分离
渲染前端和渲染后端分离
OpenGL 是当前实现，不是架构中心
EnTT 管对象，不管资产和文档
Component 存数据，System 写逻辑
每个阶段必须交付可运行结果
每个长期决策必须有 ADR
```
