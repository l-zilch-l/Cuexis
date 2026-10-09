# ADR 0040：Stage 5 Material、Shader 与能力分层合同

日期：2026-08-28

状态：已接受

关系：细化 [ADR 0021](0021-spirv-centered-shader-pipeline.md)、[ADR 0006](0006-render-backend-abstraction.md)、
[ADR 0024](0024-configuration-ownership-and-staged-formats.md) 与
[ADR 0037](0037-stage-3-portable-presentation-contracts.md)；不重开 SPIR-V 中心管线、前后端分离或
Portable Presentation v1。字段、预算和诊断码以
[MATERIAL_SHADER.md](../formats/MATERIAL_SHADER.md) 为准。

## 背景

Stage 3 已冻结 Mesh、Texture2D 与 Unlit Material 的跨宿主最低合同。OpenGL adapter 用内联
GLSL 330 绘制 Unlit。Stage 5 需要版本化 Shader 与参数化材质，但不能把 shaderc 带进
headless Playback，也不能把任意 ShaderAsset 自动转换成 Unlit 或宿主引擎材质。

归档提纲中的 5A/5B/5C 没有冻结 payload kind、Asset Index 版本、观察面、SDK/digest 或模块
链接方向。这些一旦写进 `presentation.hpp` 就很难改。

## 决策

### 合同先冻结

S5-A 只产生 ADR、生产 spec 和计划细化。S5-B 开始前禁止修改 public header、C++、CMake
target、Schema、fixture 或测试。

### 三层能力，不自动转换

```text
Portable Presentation Profile     Unlit Material v1；跨宿主最低契约
Built-in Renderer Profile         Cuexis Player/Studio 内建 Shader 与参数化材质
Host-specific Extension           宿主 adapter 显式声明的能力 ID
```

ParameterizedMaterial 不是 Unlit 的投影。需要跨宿主最低能力的内容继续使用 Unlit payload。
缺少 Built-in 或所需 host extension 时 candidate 稳定失败，不静默降级。

### 扩展 CXPRES01，不另起源资产 envelope

Shader 与 Parameterized Material 使用现有 `CXPRES01` envelope，新增 `resourceKind`：

```text
Shader = 4
ParameterizedMaterial = 5
```

UnlitMaterial = 3 保持不变。派生编译缓存使用独立 `CXSCCH01` envelope，不是 Asset Index
源资产，也不进入 FrameSnapshot。

### Asset Index v3 增加 shader

`cuexis.asset-index` version 3 增加 `shader` type。`material` 仍表示 Chart 可引用的材质；
payload kind 区分 Unlit 与 Parameterized。v1/v2 reader 继续拒绝 `shader` 与未知 kind。
CXC v1 容器字段不改；新资源只是包内资产。

### Playback 解析源，编译器保持可选

`cuexis_playback` 解析并拥有源 payload 与 semantic identity，不链接 shaderc、glslang、
SPIRV-Tools 或 SPIRV-Cross。编译、反射和目标代码生成属于内部可选模块 `cuexis_shader` 与
developer importer。headless extract 不要求缓存或 GPU。

### 公开 API 与 digest

新增公开类型和方法时 SDK API 升到 `0.7.0`。ObjectSnapshot 仍使用 `PresentationResourceRef`，
FrameDigest 保持 v3。默认 Playback capability 在 S5-G 前不加入 Shader/Parameterized 名称。

### Profile 身份的第一消费者是 ShaderAsset

Stage 5 冻结 toolchain 拥有的 profile ID，并写入 ShaderAsset 与缓存键。ProjectConfig 保持
v1，不复制编译标志。Stage 6 若需要项目级 profile 选择，再增加 ProjectConfig 引用；DeviceProfile
仍不属于本阶段。这是 ADR 0024 的分阶段消费，不是把编译细节写进项目文件。

### v1 功能上限

Vertex + Fragment、声明式参数、最多 4 个 keyword、显式 set/binding、Opaque/Blend 与
double-sided。禁止 `#include`、Shader Graph、PBR、Light、Mask、Compute/Geometry 和通用
Pipeline API。热重载只在 Player 证明 Worker + Render safe point；不实现 Studio 编辑器。

## 备选方案

### 为 Shader 另建源资产 envelope

拒绝。它会复制 magic/version/budget 校验，并使 Playback acquisition 分叉。kind 扩展足够
表达新源资产；派生缓存才需要独立 envelope。

### 把 ParameterizedMaterial 投影为 Unlit

拒绝。自动投影就是 ADR 0021 禁止的隐式转换，也会造成同一 AssetId 的双观察面。

### 在 Playback 或默认 vcpkg feature 中链接 shaderc

拒绝。headless 与 adapter-disabled 门禁不能依赖 GPU 或编译器。

### 把 Shader 只放进 OpenGL adapter

拒绝。那会把 Stage 5 做成后端私有格式，阻塞 Studio、外部宿主和 Stage 10 Vulkan 验证。

## 影响

- S5-B 必须把 shader 工具放进可选 vcpkg feature，并扩展 architecture/allowlist 测试。
- Asset Index v3 与 CXPRES kind 4/5 是格式增量，不重开 Stage Chart Format Update。
- 外部 Portable-only consumer 继续只处理 Unlit；遇到 ParameterizedMaterial 按 capability 失败。
- 第三方 shader 工具许可证在 S5-B 记入 `THIRD_PARTY_NOTICES.md`，不进入安装公共头。

## 后续风险

跨编译不能保证任意 GLSL 可移植。v1 必须维持 portable subset 与 capability 测试，不能依赖
SPIRV-Cross 偶然生成可用代码。
