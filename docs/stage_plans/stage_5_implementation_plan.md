# Stage 5 Implementation Plan: Material and Shader Pipeline

状态：future；S5-A/S5-B/S5-C/S5-D/S5-E/S5-F/S5-G 已完成；S5-H 尚未开始；Stage 4 已于 2026-08-27 关闭

更新日期：2026-08-28

归档来源：[旧版 PROJECT_GUIDE](../archive/PROJECT_GUIDE_LEGACY_2026-08-10.md) 与
[SDK transition plan 快照](../archive/CUEXIS_SDK_TRANSITION_PLAN_2026-08-10.md)。权威合同：
[ADR 0040](../adr/0040-stage-5-material-shader-contracts.md) 与
[MATERIAL_SHADER.md](../formats/MATERIAL_SHADER.md)。Portable Unlit v1 以
[PORTABLE_PRESENTATION.md](../formats/PORTABLE_PRESENTATION.md) 为准。历史设计输入见
[SHADER_PIPELINE.md](../proposals/deferred/SHADER_PIPELINE.md)。

旧提纲中的 Stage 5A/5B/5C 被本文件的 S5-A 至 S5-H 取代，对应关系见第 7 节。S5-A 已冻结 payload、
观察面、capability、预算和模块拓扑。S5-B 起才允许改生产代码。

## 1. 阶段目标

在 Portable Presentation v1 之上建立可由 Player、未来 Studio 和宿主 adapter 消费的版本化材质与
Shader 工作流，并明确三层能力：

```text
Portable Presentation Profile     跨宿主最低材质/属性契约（继续包含 Unlit v1）
Built-in Renderer Profile         Player/Studio 内建后端支持的 Shader、参数与 RenderState
Host-specific Extension           宿主 adapter 显式声明的能力，从不自动转换
```

本阶段把固定 Unlit Material 扩展为可引用 Shader、带声明式参数和有限 Variant 的工作流，但不建设
通用渲染引擎、Shader Graph 或 Studio 编辑器。headless Playback 继续在无 GPU、无 Shader 编译器的
情况下完成 load、update、Seek 和帧快照提取。

## 2. 前置条件

- Stage 4 已关闭；默认 Playback Session 可求值非空合法 Chart v4 / CXT Binding 动画。
- Portable Presentation v1、candidate manifest/acquisition、无 GPU Validation Sink 与 in-tree
  OpenGL adapter 保持权威。
- ADR 0021 / 0006 / 0024 / 0037 不重开产品方向：SPIR-V 为规范 IR；业务层不保存 GPU 对象；
  ImporterProfile / ShaderTargetProfile 拥有派生格式；Stage 5 从 Unlit 扩展而不重定义 Portable v1。
- 新增 shader 工具依赖必须同时更新 `vcpkg.json`、[DEPENDENCY_POLICY.md](../guides/DEPENDENCY_POLICY.md)
  与 `THIRD_PARTY_NOTICES.md`。

上述前置条件在 2026-08-27 均已满足。S5-A 规范包已于 2026-08-28 写入。S5-B 起才允许改生产代码。

## 3. 当前基线

仓库在 Stage 5 开工前的事实入口如下。批次必须接到这些入口，不得另起第二套材质观察面或在
Playback 热路径同步编译 Shader。

```text
PresentationResourceType 只有 Mesh=1、Texture2D=2、UnlitMaterial=3
PortableUnlitMaterial 含 baseColor、alphaMode、doubleSided、可选 base-color Texture2D
FrameSnapshot ObjectSnapshot 使用 PresentationResourceRef；FrameDigest 保持 v3
OpenGL presentation adapter 内联 GLSL 330 Unlit，按 uniform 上传颜色/纹理开关
Asset Index type 为 mesh / material / texture / audio；无 shader；material 内容是 Unlit payload
ProjectConfig 不引用 ImporterProfile / ShaderTargetProfile
vcpkg 无 shaderc、glslang、SPIRV-Tools、SPIRV-Cross
tools/asset_importer 仍是空 custom target
cuexis_playback 不依赖 SDL、OpenGL 或 Shader 编译器
PropertyId::RenderMaterial / MaterialOpacity / MaterialTint 已由 Stage 4 求值
SDK API 0.6.0；构建版本 26.08.01-1
```

公开观察面继续是 `PlaybackSession`、`FrameSnapshot` 和 `FrameDigest` v3。宿主不得因本阶段获得
World、EnTT、RuntimeSession、GLuint、VkHandle 或 JSON DOM。

## 4. 实施范围

- 冻结并实现版本化 MaterialAsset、ShaderAsset、参数 Schema、有限 RenderState 和 Reflection 数据。
- 接入可选的 shaderc/glslang、SPIRV-Tools 与 SPIRV-Cross，完成 GLSL 450 → SPIR-V → GLSL 330 Core /
  GLSL ES 300。
- 实现声明式 Variant、显式 set/binding、属性 Schema 与 SPIR-V 反射的交叉校验。
- 定义版本化 ImporterProfile 与 ShaderTargetProfile；ProjectConfig 只引用 profile ID。
- 实现规范缓存键、导入缓存、失败保留上一有效 Pipeline，以及 Worker 编译与 Render safe point
  的原子替换。
- 让 OpenGL adapter 与 Player 消费派生缓存；Validation Sink 在无 GPU 下校验 identity、schema
  与 capability。
- 保持 Chart/FrameSnapshot 使用稳定资源身份，不暴露 uniform location、texture unit 或后端对象。

字段布局、预算数字、诊断码和 fixture 清单不在本计划复制。权威分别是
[PORTABLE_PRESENTATION.md](../formats/PORTABLE_PRESENTATION.md) 与
[MATERIAL_SHADER.md](../formats/MATERIAL_SHADER.md)。

## 5. 验收标准

- 修改合法材质参数可在 Player 中实时预览。Studio 编辑器不属于本阶段；本阶段必须提供可供未来
  Studio Inspector 消费的稳定 Reflection 数据，且不泄漏 GPU 对象。
- Shader 编译或热重载失败不会崩溃，也不会替换上一有效 Pipeline。
- ShaderAsset、MaterialAsset 和公共 Playback API 不绑定 OpenGL/Vulkan 专有类型。
- 目标 Shader 同时通过 GLSL 330、GLSL ES 300 和 SPIR-V 验证。
- 材质可被 Chart/Entity 通过稳定 AssetId 引用。
- 宿主 adapter 必须显式报告 capability；系统不承诺自动转换任意 ShaderAsset。
- profile 缺失、版本不支持或能力不兼容时稳定失败，不触发运行时隐式重导入。
- 修改 profile 只失效受影响缓存，且派生资产记录规范化 profile identity。
- headless Playback 不要求 GPU、Shader 编译器或内建渲染 adapter。
- Portable Unlit v1 的 manifest、identity、FrameDigest v3、Validation Sink 与 OpenGL 规范化摘要
  保持兼容。
- 缺少 Built-in Shader capability 的 Session 对需要该能力的内容稳定拒绝。

## 6. 明确不包含

- Shader Graph、任意运行时代码生成、未声明宏组合或通用表达式求值。
- 把宿主 Shader API、GLuint、VkDescriptorSet、uniform location 或 texture unit 暴露到 Chart、
  Material 或 Playback 公共头。
- 删除或平行替换 Portable Unlit Material v1。
- PBR、Light、Shadow、Particle、UI、RenderGraph、通用 Buffer/Pipeline/CommandList API。
- 在 `cuexis_playback` 链接闭包中加入 shaderc/glslang/SPIRV-Cross。
- 运行时脚本、逐帧脚本回调、Studio 编辑器实现、Judgement/Replay。
- 公共 CXC package API，或把 `cuexis_cxc` / 新 shader 模块提升为安装组件。
- Vulkan 产品化（Stage 10）、Android/KTX2 资源（Stage 9B）、稳定 C ABI（Stage 12）。
- 重新解析 ChartParameter、重新执行 Template Binding lowering，或在 shader 模块内解析
  JSON/CXC/CXT。

## 7. 批次顺序

只允许按批次推进。S5-A、S5-B、S5-C、S5-D、S5-E、S5-F 与 S5-G 已关闭。S5-H 开始前禁止把
局部 Shader 编译描述为完整 Stage 5。OpenGL 内联 Unlit GLSL 330 保持不变。默认 Session 已包含
`cuexis.shader.asset.v1` 与 `cuexis.material.parameterized.v1`；显式裁剪 Session 仍拒绝。

```text
S5-A  合同冻结（无生产代码）
S5-B  模块接线、可选编译器依赖、诊断冻结
S5-C  版本化 Material 与 Unlit v1 共存
S5-D  ShaderAsset 源、SPIR-V、Reflection、声明式 Variant
S5-E  ImporterProfile / ShaderTargetProfile / 三层 capability
S5-F  规范缓存、失败保留上一 Pipeline、Worker 编译
S5-G  OpenGL adapter、Player 与 Validation Sink 消费；公开 capability
S5-H  安全、分配、hosted 验收与阶段关闭
```

与归档提纲的映射：旧 5A 材质资产落入 S5-C；旧 5B Shader 编译与 Profile 落入 S5-D/S5-E；旧 5C
缓存与热重载落入 S5-F。新增 S5-A 设计门禁、S5-B 接线、S5-G 消费面和 S5-H 关闭，对标 Stage 3A
与 Stage 4 的 S4-A/S4-F/S4-H。

### 7.1 S5-A：合同冻结

对标 Stage 3A。只允许阅读代码、更新 ADR/规范、编写公共 API sketch、冻结格式/预算/错误码和
package 拓扑。禁止改生产代码。

任务：

1. 冻结 MaterialAsset / ShaderAsset 的版本化 payload：扩展 `CXPRES01`，`Shader=4`，
   `ParameterizedMaterial=5`；派生缓存使用独立 `CXSCCH01`。
2. 冻结 Asset Index v3 增加 `shader`，以及 Material → Shader/Texture 的依赖闭包与预算。
3. 冻结观察规则：Unlit 保持 kind 3；Parameterized 保持 kind 5；禁止互相投影。
4. 冻结 SDK API `0.7.0` 与 FrameDigest v3。
5. 冻结 Playback capability 名称与 PresentationCapabilities version 2。
6. 冻结诊断 code、identity 输入集、缓存键与 truncation sentinel。
7. 冻结模块落点：`cuexis_shader` 可选；Playback 不链接编译器。
8. 冻结热重载范围：仅 Player Worker + Render safe point。
9. 冻结 v1 功能上限：vertex+fragment、最多 4 keyword、无 `#include`、无 PBR/Light/Mask。

退出门禁：规范包写入 ADR、生产 spec 与本计划细化。

实施快照（2026-08-28）：[ADR 0040](../adr/0040-stage-5-material-shader-contracts.md) 与
[MATERIAL_SHADER.md](../formats/MATERIAL_SHADER.md) 已冻结上述合同。证据见
[S5-A 报告](../stage_reports/260828-stage-5-s5-a-contracts.md)。生产代码未改。

### 7.2 S5-B：模块接线与可选依赖

零功能启动门禁。不改 `presentation.hpp` 布局，不解析 kind 4/5，不替换 OpenGL 内联 Unlit
shader，不改默认 `allCapabilities()`。

任务：

1. 新增内部静态库 `cuexis_shader`：`engine/shader/CMakeLists.txt`，`add_subdirectory` 受
   `CUEXIS_BUILD_SHADER_TOOLS` 保护。加入 `CUEXIS_ACTIVE_TARGETS` 与 allowlist。允许依赖仅
   `cuexis::core`（若 S5-B 证明需要路径工具，可加 `cuexis::assets` / `cuexis::project`，不得加
   playback/SDL/OpenGL/json_support）。
2. 在 `vcpkg.json` 增加可选 feature `shader-tools`，记录 shaderc、spirv-tools、spirv-cross
   （及 baseline 中实际传递的 glslang）。默认 features 保持 `player`/`tests`。同步
   [DEPENDENCY_POLICY.md](../guides/DEPENDENCY_POLICY.md) 与 `THIRD_PARTY_NOTICES.md`。不因本批
   升级 vcpkg baseline，除非当前 baseline `40f3c709db80acf154ac4b17a1f83c564ebd022e` 缺少 port。
3. 记录真实 CMake imported target 名称（例如 `unofficial::shaderc::shaderc`）于 shader
   CMakeLists 与依赖政策；这些名称不得出现在安装的 `CuexisConfig.cmake` Playback 闭包中。
4. `cuexis_playback` / `cuexis_runtime` / `cuexis_chart` / `cuexis_animation` allowlist 不得加入
   shader 工具。`cuexis_asset_importer` 在 developer tools 打开且 shader-tools 打开时链接
   `cuexis_shader`；本批只建立可执行入口与 `--help`/exit code，不写 cache。
5. 扩展 `cmake/VerifyArchitecture.cmake`：`engine/shader/` 不得包含 SDL、glad、GL、nlohmann、
   minizip、`cuexis/playback/` 公共头、`cuexis/render_opengl/`；`engine/playback/` 不得包含
   shaderc/spirv/glslang 头。
6. 在 `cuexis_shader` 内部头冻结 compile diagnostic 出口：`shader.compile.failed`、
   `shader.reflect.mismatch`、`shader.diagnostics.limit_exceeded`。本批实现可返回固定
   “not compiled” 占位，但 code 字符串必须稳定。
7. 未定义 `CUEXIS_BUILD_SHADER_TOOLS` 时，Debug 与 adapter-disabled headless 完整 CTest 数量
   不得因本批减少。打开 shader-tools 时至少有一个 `cuexis_shader_tests` 链接成功。

退出门禁：无 shader-tools 的 Debug/headless 配置构建通过；显式打开后 `cuexis_shader` 可链接；
architecture/allowlist 通过；默认 Unlit 路径不变。

实施快照（2026-08-28）：可选 vcpkg feature `shader-tools`、`CUEXIS_BUILD_SHADER_TOOLS`（默认
`OFF`）、内部静态库 `cuexis_shader`、`cuexis_shader_tests`、`cuexis_asset_importer --help` 与
preset `debug-shader-tools` 已接线。Imported target 为 `unofficial::shaderc::shaderc`、
`spirv-cross-core` / `glsl` / `reflect`、`SPIRV-Tools-static`。compile 返回稳定
`shader.compile.failed` 占位。证据见
[S5-B 报告](../stage_reports/260828-stage-5-s5-b-shader-tools.md)。未改 `presentation.hpp`。

### 7.3 S5-C：版本化 Material 与 Unlit 共存

本批首次改 Playback 公开类型，因此同时把 SDK API 更新为 `0.7.0`。不启用默认 shader
capability，不调用编译器。

任务：

1. `PresentationResourceType` 增加 `Shader = 4`、`ParameterizedMaterial = 5`。扩展
   `PortableResourceValue`、identity、decoded-byte 与 manifest 排序。
2. 实现 `CXPRES01` kind 4/5 Reader：envelope、LF 源码、schema、binding、parameter 顺序、预算
   与依赖闭包。未知 kind 仍拒绝。kind 3 Unlit 路径字节级保持。
3. Asset Index 与 `assets::AssetType` 增加 v3 `shader`。v1/v2 JSON 继续拒绝该 type。v3
   `material` 记录根据 payload kind 成为 Unlit 或 Parameterized。
4. ObjectSnapshot.material 只允许 UnlitMaterial 或 ParameterizedMaterial。Shader 只能作为
   manifest 依赖。RenderMaterial step 仍只切换已准备 AssetId。
5. 闭包含 kind 4 时要求 `cuexis.shader.asset.v1`；含 kind 5 时两者都要求。默认 Session 缺少
   这两项，故合法 Parameterized fixture 在默认路径以 `playback.capability.unsupported` 失败。
6. 公开常量写在 `playback_session.hpp`，但 **不** 写入 `allCapabilities()`。测试使用显式
   opt-in `PlaybackCapabilitySet`。
7. Unlit fixture、FrameDigest v3 golden、CFU-F consumer 与 Validation Sink 的 Unlit 分支保持。
   新 identity 测试覆盖 shader 源变化与纹理变化会改变 ParameterizedMaterial identity。
8. 安装公共头保持 ASCII。static/shared consumer 必须在新类型存在时仍能编译；未处理的
   variant 替代项不得在官方 consumer 中导致未检查的 visit。

退出门禁：Unlit 回归 100% 保持；kind 4/5 的 headless 解析/identity 测试不链接 OpenGL 或
shaderc；默认 Session 拒绝 Parameterized 内容。

实施快照（2026-08-28）：`PresentationResourceType` Shader=4 / ParameterizedMaterial=5、Asset
Index v3 `shader`、kind 4/5 Reader、公开 capability 常量（不进默认集合）与 SDK API `0.7.0`
已落地。证据见 [S5-C 报告](../stage_reports/260828-stage-5-s5-c-material-unlit.md)。未调用
编译器，未改 OpenGL Unlit shader。

### 7.4 S5-D：ShaderAsset 编译与 Reflection

依赖 S5-B 的 shader-tools 与 S5-C 的源 payload。Playback 仍不链接编译器。

任务：

1. `cuexis_shader` 实现 `cuexis.importer.shader.v1`：shaderc 将 GLSL 450 编译为 SPIR-V
   （Vulkan 1.1 / SPIR-V 1.3，优化级别 0），SPIRV-Tools validate，SPIRV-Cross 反射并生成
   GLSL 330 Core 与 GLSL ES 300。
2. 仅为选中 keyword 定义 `#define NAME 1`。未声明 keyword、`#include`、占用 `(set=0,binding=0)`
   的用户 binding、schema 与反射不符，分别使用冻结诊断 code。
3. 规范化 reflection：按 name 排序的 parameter/binding 表，供后续 cache 与 OpenGL 映射。不得
   把 texture unit 写进 reflection 公共结构。
4. `cuexis_asset_importer` 读取 ShaderAsset 源并调用 `cuexis_shader`；输出仍可不落盘到
   `CXSCCH01`（那是 S5-F），但必须能在测试中返回 SPIR-V 与 reflection。
5. 黄金测试：同一 LF 源在 Windows/Linux 上 SPIR-V 字节与 reflection canonical encoding 相同。
   覆盖合法 Unlit-like sprite shader、重复 binding、未声明 keyword、非法 `#include`。
6. `engine/shader/` 不解析 JSON、CXC、CXT。生产 fixture 若以 Chart/CXC 出现，只经 Playback
   prepare 或 importer CLI 进入。

退出门禁：shader-tools 开启时 compile/reflect 测试通过；无 shader-tools 构建不编译这些测试；
Playback 热路径零编译调用。

实施快照（2026-08-28）：`cuexis.importer.shader.v1` 已在可选 `cuexis_shader` 中落地：shaderc
Vulkan 1.1 / SPIR-V 1.3 opt 0、SPIRV-Tools validate、SPIRV-Cross GLSL 330 Core / ES 300、
按 name 排序的 reflection（无 texture unit）。`cuexis_asset_importer --compile` 读取 GLSL 源并
返回 SPIR-V/reflection，不写 `CXSCCH01`。证据见
[S5-D 报告](../stage_reports/260828-stage-5-s5-d-shader-compile.md)。Playback 仍不链接编译器。

### 7.5 S5-E：Profile 与 capability

任务：

1. 实现 toolchain profile ID 字符串常量与校验：`cuexis.importer.shader.v1`、
   `cuexis.target.spirv.v1` / `glsl330.v1` / `glsles300.v1`、`cuexis.renderer.builtin.v1`。
   ShaderAsset.requiredRendererProfile 不匹配则
   `playback.presentation.shader.profile_unsupported`。
2. `PresentationCapabilities` version 2 字段落地。version 1 adapter 遇到 ParameterizedMaterial
   以 `playback.presentation.capability.required_missing` 失败。limit 字段低于 spec 预算时
   `playback.presentation.capability.limit_insufficient`。
3. `hostExtensionIds` 必须包含 ShaderAsset.requiredHostExtensions 的全部 ID。Cuexis OpenGL
   adapter 只报告 builtin.v1，不发明宿主 ID。
4. `PresentationRequest` / `EffectivePresentationSettings` version 2 增加
   `enableShaderCompile` 与 `enableShaderHotReload`。headless Validation Sink 保持 version 1
   请求即可验证 Portable；验证 Parameterized 源时用 version 2 但 compile/hot-reload 为 false。
5. ProjectConfig 保持 v1。不得把编译标志写入 `cuexis.project.json`。
6. 默认 `allCapabilities()` 仍不含 shader/parameterized。opt-in 测试证明 preflight 顺序：
   parse → playback capability → presentation capability。

退出门禁：profile/capability 缺失稳定失败；headless 不要求 Built-in Renderer Profile；Unlit
version 1 validate 路径不变。

实施快照（2026-08-28）：toolchain / renderer profile ID 已公开在 `presentation.hpp`，并在
`cuexis_shader` 中重复相同字符串。`PresentationCapabilities` / request / effective version 2
字段接在 version 1 布局之后。Parameterized 候选在 version 1 或 `parameterizedMaterial == false`
时以 `playback.presentation.capability.required_missing` 失败；shader limit 低于 spec 预算时
`limit_insufficient`；`hostExtensionIds` 必须覆盖 `requiredHostExtensions`。Cuexis OpenGL 报告
`builtin.v1` 且 `parameterizedMaterial == false`，不发明宿主 ID。headless Validation Sink 保持
version 1 Unlit 路径；Parameterized 验证使用 version 2 且 compile/hot-reload 为 false。默认
`allCapabilities()` 仍不含 shader/parameterized。证据见
[S5-E 报告](../stage_reports/260828-stage-5-s5-e-profile-capability.md)。

### 7.6 S5-F：缓存、诊断与热重载

任务：

1. 实现 `CXSCCH01` Writer/Reader 与规范缓存键（源 identity、importer/target profile、keyword、
   entry、工具版本元组）。缓存目录由 importer/Player 显式传入，不扫描工程树。
2. importer 默认写出 SPIR-V + GLSL 330 + GLSL ES 300 + reflection。删除缓存后同一键必须重建
   出相同字节。
3. 修改工具版本或 profile 只使匹配键失效；`shader.cache.tool_mismatch` 不得复用旧缓存。
4. Player：`enableShaderCompile=false` 时 adapter prepare 缺缓存即
   `shader.cache.missing`，不编译。`true` 时 Worker 编译，owner-thread 在 Render safe point
   noexcept swap。失败走 `shader.hot_reload.failed`，active pipeline 不变。
5. 源 payload 变化必须走 Playback `prepareReload`（identity 变）。仅缓存重建不得改变
   `PreparedSemanticIdentity` 或 FrameDigest。
6. 诊断满 1,024 条后以 `shader.diagnostics.limit_exceeded` sentinel 截断。
7. 禁止运行时把任意字符串当宏；keyword 只能来自 ShaderAsset 声明集。

退出门禁：失败热重载不崩溃、不替换 active pipeline；删缓存后可确定性重建；Playback
`update()` 无编译器调用。

实施快照（2026-08-28）：`CXSCCH01` Writer/Reader 与规范缓存键已在可选 `cuexis_shader` 落地。
键为 length-prefixed SHA-256（domain NUL、源 identity、importer/target、keyword、entry、
工具 name/version）。目录由 importer/Player 显式传入，不扫描工程树。importer
`--cache-dir` 写出 SPIR-V + GLSL 330 + GLSL ES 300 + reflection；删除后同一键重建相同字节。
`shader.cache.tool_mismatch` 不复用旧工具版本。`ShaderPipelineCache` 在
`compileEnabled=false` 且缺缓存时返回 `shader.cache.missing` 且不编译；`true` 时 Worker
编译，`activate()` 为 owner-thread noexcept swap。失败热重载返回
`shader.hot_reload.failed` 并保留 active。诊断容量 1,024，sentinel
`shader.diagnostics.limit_exceeded`。OpenGL/Player 尚未消费缓存。证据见
[S5-F 报告](../stage_reports/260828-stage-5-s5-f-cache.md)。

### 7.7 S5-G：消费面与公开 capability

只有 S5-C 到 S5-F 通过后，才能把下列常量写入默认 `allCapabilities()`：

```text
cuexis.shader.asset.v1
cuexis.material.parameterized.v1
```

任务：

1. OpenGL presentation：Unlit 对象继续走现有内联 GLSL 330 程序，规范化摘要与 Stage 3 golden
   一致。Parameterized 对象绑定 `CXSCCH01` 的 GLSL 330、`CuexisObject` UBO 与 set/binding →
   texture unit 映射。不得把 GLuint 写回 Playback 类型。
2. Player 事务顺序保持：Playback candidate → acquire 源资源 → adapter prepare（cache 或
   opt-in compile）→ Playback commit → adapter activate。commit 失败丢弃 adapter candidate。
3. Validation Sink 增加 kind 4/5 identity、schema、capability 与失败诊断；不读 SPIR-V，不
   安装为 SDK component。Portable-only 模式对 Parameterized 对象失败。
4. Playback-only external consumer 不链接 shaderc/OpenGL。显式裁剪 capability 的 Session 对
   Parameterized CXC 失败且不破坏 active Unlit 状态。
5. Player 可对合法 ParameterizedMaterial 修改已声明数值参数并在后续帧看到颜色/纹理变化；
   该预览不得经过第二套 Runtime。
6. 此时才把两个 capability 写入 `allCapabilities()`。CFU-F3 类 fingerprint 测试若受默认集合
   影响，必须更新 golden 并记录原因。

退出门禁：默认 Session 接受合法 Built-in Shader 内容；裁剪 Session 仍拒绝；Unlit 回归保持。

### 7.8 S5-H：安全、分配与关闭

任务：

1. 强制 spec 预算：源 262,144/stage、keyword 4、variant 16、parameter 32、binding 16、每资源
   64 MiB。全部 checked arithmetic。越界不得分配巨型 fixture。
2. warmed Unlit `update()` / `extractFrame()` 继续 Stage 4 零新增或有界合同。Parameterized
   热帧不得编译、不得打开文件、不得增长 cache map；若无法零分配，必须书面冻结有界合同。
3. `CUEXIS_RUN_PERFORMANCE_PROBE=1` 记录最大合法 shader/material 的 allocation 与内存趋势，
   不设机器硬阈值。
4. 最终 SHA：Debug/Release/`--fresh`、clean build、完整 CTest、adapter-disabled headless、
   shader-tools ON 聚焦测试、format、architecture、public-header ASCII、version、license、
   `check_docs.py`、`git diff --check`。
5. hosted Linux Quality、Windows MSVC、Windows MinGW 同 SHA；记录 run URL 与第一失败步骤。
   Linux sanitizer 必须覆盖 shader-tools 测试。
6. 写 Stage 5 completion report。项目所有者接受后才把本计划与 `CURRENT_STATUS.md` 改为
   completed。

退出门禁：三平台 hosted 成功，owner acceptance 已记录。关闭不表示 Studio、Judgement、公共
CXC API、Vulkan 或 Shader Graph 已开始。

## 8. 模块、target 与文件落点

依赖方向：

```text
cuexis_playback / cuexis_chart
  解析并拥有 portable/typed Material 引用与 identity
  -X-> shaderc / SPIRV-Cross / SDL / OpenGL

cuexis_shader          (optional; CUEXIS_BUILD_SHADER_TOOLS + vcpkg feature shader-tools)
  GLSL 450 -> SPIR-V -> reflection / GLSL 330 / ES 300
  -> cuexis_core (+ assets/project only if S5-B allowlist records them)
  -X-> Playback public headers / SDL / OpenGL / JSON DOM

cuexis_render_opengl
  消费 owning portable values + 派生 GLSL 330 缓存
  映射 set/binding -> texture unit / UBO
  -X-> 把 GLuint 写回 Material / Chart / Playback

tools/asset_importer
  使用 cuexis_shader 写导入缓存；不在 Playback 热路径运行
```

建议文件布局（确切文件名可按现有 snake_case 调整，职责不得移动）：

```text
engine/shader/CMakeLists.txt
engine/shader/include/cuexis/shader/...
engine/shader/src/...

engine/playback/include/cuexis/playback/presentation.hpp
  仅在 S5-A 授权后 additive 扩展公开类型
engine/playback/src/presentation.cpp
  解析新 payload / identity；不调用编译器

engine/render_opengl/src/open_gl_presentation.cpp
  从内联 Unlit program 扩展为消费派生 GLSL 330

tools/asset_importer/
tests/shader/
tests/presentation/
tests/playback/
```

新 shader 模块不是安装 package component。shared Playback 若需要实现细节，只私有链接不传播
编译器。静态 consumer 只看到 `Cuexis::Playback`。基础 `Cuexis::Playback` package 继续不查找
SDL、OpenGL、GLAD 或 shaderc。

## 9. 公共 API 与版本

- FrameDigest 保持 v3。ObjectSnapshot 仍编码 `PresentationResourceRef`；ParameterizedMaterial
  identity 递归包含 shader 与纹理。
- S5-C 落地公开类型时 SDK API 从 `0.6.0` 升到 `0.7.0`。新字段接在 PresentationCapabilities
  version 1 布局之后，默认零值。
- 稳定 C ABI 仍延期到 Stage 12。
- 日期构建版本只通过 `tools/update_version.py` 在 merge/release 门禁更新。
- 默认 capability set version 保持 1；S5-G 已追加 `cuexis.shader.asset.v1` 与
  `cuexis.material.parameterized.v1`。
- 安装公共头必须纯 ASCII。

## 10. 测试与证据

Material/Shader 单元测试使用手工 typed 输入或导入工具产物。Playback/consumer 测试通过 public
prepare 路径加载合法 fixture，不在 shader 模块解析 JSON/CXC。

最低证据矩阵：

```text
Unlit v1 回归（manifest, identity, FrameDigest v3, Validation Sink, OpenGL summary）
新 Material payload 合法/非法/预算
Shader schema 与 SPIR-V reflection 匹配与冲突
未声明 variant 拒绝
profile 缺失与 capability 不足
失败热重载保留上一 Pipeline
headless 无 shader-tools 全量 CTest
shader-tools 开启时的 compile/cache 确定性
Playback-only consumer 不链接 shaderc/OpenGL
architecture / allowlist / public-header ASCII
hosted Linux ASan/UBSan、MSVC、MinGW
```

## 11. 残余风险关闭要求

下列风险在对应批次关闭，不能带到 S5-H：

| 风险 | 关闭批次 |
| --- | --- |
| 未冻结合同就改 `presentation.hpp` | S5-A 已关闭 |
| shaderc 进入默认构建或 Playback 链接闭包 | S5-B |
| 新 Material 与 Unlit 双观察面语义分叉 | S5-C |
| 过早公开 Built-in Shader capability | S5-G 之前默认集合不加新能力 |
| Playback 热路径同步编译 | S5-D/S5-F |
| 任意 ShaderAsset 被静默转成 Unlit | S5-C/S5-E |
| 编译失败替换上一 Pipeline | S5-F |
| OpenGL 假抽象把 Shader 做成后端私有格式 | S5-D/S5-G |
| Asset Index v1/v2 误收 `shader` | S5-C |
| 热帧分配回退 | S5-H |
| 运行期诊断缺 identity / 截断 | S5-F/S5-H |

## 12. 阶段退出条件

Stage 5 只有在下列全部成立后才能标为 completed：

1. S5-A 到 S5-G 退出门禁关闭。
2. Portable Unlit v1 行为与 Stage 3/4 golden 保持兼容。
3. Built-in Shader Material 可在 Player 预览；编译失败不崩溃且不替换上一 Pipeline。
4. 宿主观察面仍只有 Playback/FrameSnapshot/digest；World/EnTT/GPU 对象未泄漏。
5. headless Playback 不要求 GPU 或 Shader 编译器。
6. hosted Linux Quality、Windows MSVC 和 Windows MinGW 在同一最终 SHA 成功。
7. completion report 经项目所有者接受。

关闭后的下一阶段是 [Stage 6](stage_6_implementation_plan.md)。
