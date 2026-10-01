# Stage 9 Implementation Plan: Presentation Foundation and Chart v6

状态：future；未开始

更新日期：2026-10-01

前置：

```text
Stage 8 Chart v5 / CXT v2 / Packed Chart production baseline
Portable Presentation v1 and Material/Shader v1
Stage 7A core contract and selected 7B+ capability only when a presentation batch needs it
```

本阶段吸收原 `chart-format-update-for-v6` 延期计划，并把原 Stage 9 的环境/模型目标收敛为
同一条 Presentation Foundation。Chart v6 是模型与默认网格的承载版本，不改变 Judgement、Score
或 Replay 语义；Stage 10 Studio 只依赖本阶段已交付的稳定子集。

## 1. 阶段目标

```text
Presentation Environment
  -> solid/equirectangular/cubemap skybox、fog、环境参数
Model v1
  -> 静态 glTF 2.0 离线导入为 portable Model/Mesh/Material/Texture
Chart v6
  -> 内置基础网格、默认扁平片、submesh 材质槽
```

## 2. 工作批次

### S9-A：Presentation Environment

- 环境是独立 Presentation 资源，不是普通 Chart Object 或 Renderable Mesh。
- 冻结相机平移、时间驱动、有限 Behavior、纹理/内存预算、移动端降级和 Snapshot/Digest 影响。
- 资源进入 CXC closure；能力缺失时稳定拒绝。

### S9-B：Model v1 与导入

- 只接受静态 glTF 2.0；拒绝 animation、skin、morph target、camera、light 和运行时扩展。
- 离线 importer 生成 portable Model v1；Playback/prepare 不解析 glTF。
- CXC 只打包 portable payload；source glTF 只能保留为 Source Project/source entry。
- 节点 TRS 在导入阶段处理为 portable submesh 数据。

### S9-C：内置网格与 Chart v6

- 冻结 triangle、quad、sheet、cube、uv-sphere、prism、cylinder 的 bytes、identity 和分段常数。
- 非相机、带 renderable 且省略 mesh 的对象默认使用 sheet；相机永不获得默认网格。
- 沿用 `transform.scale` 与既有 Behavior/AnimationClip 白名单，不建立 morph/骨骼系统。

### S9-D：Submesh 与观察面

- Model v1 支持有序 submesh 槽和静态材质覆盖；未覆盖槽使用 Model 默认材质。
- FrameSnapshot、FrameDigest、capability 和预算变化必须独立版本化。
- 合法 v4/v5 输入的既有 digest 保持不变。

## 3. 关闭标准

- Chart v6 ADR、Model v1 Spec、Schema、Reader/Writer 和 importer 合同先于生产实现。
- 非法 glTF、缺资源、错误拓扑、缺 capability、超预算均稳定失败。
- portable Model、内置网格和 submesh closure 可跨平台复现。
- Seek/Reload 后表现由绝对时间重建；不依赖上一帧或 GPU 状态。
- Player、headless、external consumer 和 CXC package gates 通过，owner acceptance 完整。

## 4. 明确不包含

- 曲线形变、line、包级后处理、骨骼、morph、PBR、Shader Graph。
- Studio 完整产品；Studio MVP 由 Stage 10 负责。
- 任意运行时脚本或把表现几何当作 Judgement 几何。

## 5. 交接

交付 Chart v6 / Model v1 candidate 或正式 capability、portable 模型闭包、内置网格目录和
Presentation Environment 观察面。Stage 11 仅在本阶段关闭后接收曲线表现扩展。
