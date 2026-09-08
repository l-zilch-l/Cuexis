# Stage 9 Implementation Plan: Presentation Environment and Model Extensions

状态：future；未开始

更新日期：2026-09-05

归档来源：[Chart v6 设计输入](../../deferred/chart-format-update-for-v6/plan.md)、
[Chart v7 设计输入](../../deferred/chart-format-update-for-v7/plan.md)、
[Portable Presentation v1](../../../formats/PORTABLE_PRESENTATION.md) 和
[Material/Shader v1](../../../formats/MATERIAL_SHADER.md)。

前置：

```text
Stage 8 owner acceptance
Chart v5 / CXT v2 / Packed Chart production baseline
Portable Presentation v1 and Material/Shader v1
Stage 7A core contract and any selected Stage 7B+ capability required by the first presentation batch
```

本阶段扩展可观察表现，不改变 Stage 7A 的 Judgement、Score 或 Replay 语义；未被表现批次
选定的 Stage 7B+ 能力继续沿 Gameplay 能力线演进。

## 1. 阶段目标

建立统一的 Presentation Environment 和模型表现路线：

```text
Presentation Environment
  -> Skybox / background / fog / environment lighting

Model v1
  -> static glTF import
  -> portable Model / Mesh / Material / Texture
  -> submesh slots

Geometry Presentation
  -> bounded deformation
  -> line
  -> finite model animation
```

Chart v6/v7/v8 只是这些能力的承载版本候选，不是独立于本阶段的主路线。

## 2. 子批次

### S9-A：Presentation Environment

冻结：

```text
solid-color
equirectangular skybox
cubemap skybox
environment rotation
intensity / tint
fog
environment capability
```

天空盒不进入普通 Chart Object、Renderable Mesh 或父子层级实体。它属于独立的
Presentation Environment，并通过独立资源引用进入 CXC closure。

必须明确：

```text
是否随相机平移
是否受 Chart 时间轴驱动
是否可由有限 Behavior 改变
纹理尺寸和内存预算
移动端降级
FrameSnapshot / FrameDigest 影响
```

### S9-B：Model v1

第一期只接受静态 glTF 2.0，并通过离线 importer 转为 portable Model：

```text
Mesh
Submesh
Node transform
Material
Texture
Model identity
source identity
importer profile
```

Playback/prepare 不解析 glTF。CXC 只打包已校验的 portable payload。

暂不支持：

```text
animation
skin
morph target
camera
light
运行时 tessellation
PBR
Shader Graph
```

### S9-C：Geometry Deformation

形变分为：

```text
Object Transform
Geometry Deformation
Model Animation
Postprocess
```

第一期只支持有限曲线和有限采样预算。禁止自由数学字符串、未版本化表达式、
顶点回调和脚本驱动网格修改。

### S9-D：Line 和高级模型表现

Line、submesh 槽覆盖和模型基本动画必须分别定义资源、Snapshot、Capability 和
预算，不得通过普通 `transform` 字段隐藏新语义。

## 3. 公共交接

必须冻结：

```text
PresentationManifest
Model/Environment capability
CXC resource closure
FrameSnapshot 可观察字段
FrameDigest 是否升级
OpenGL / Vulkan / Android adapter 边界
Studio import/preview 边界
```

同一 Presentation 输入在不同 adapter 中必须得到相同的语义帧数据。后端可以有
不同的 GPU 实现，但不能各自解释 Chart。

## 4. 验收标准

- 天空盒与普通判定对象完全分离。
- 静态 glTF 可离线导入为 portable Model，Playback 不链接 glTF parser。
- Model、Submesh、Material 和 Texture 资源正确进入 Asset Index/CXC closure。
- 超尺寸、损坏、非法拓扑、非法材质和不支持 glTF 特性稳定失败。
- 形变能够从绝对时间重建，Seek/Reload 不依赖上一帧。
- Presentation 变化不会改变 Judgement、Score 和 Replay。
- FrameSnapshot、FrameDigest、Capability 和预算合同有明确 golden。
- OpenGL、headless 和未来 adapter 消费同一 portable presentation。

## 5. 明确不包含

- 任意运行时脚本或 Shader 执行。
- 通过表现层改变判定域、动作或时间区间。
- Studio 完整产品。
- 无界粒子系统、骨骼动画、布料和运行时网格生成。
