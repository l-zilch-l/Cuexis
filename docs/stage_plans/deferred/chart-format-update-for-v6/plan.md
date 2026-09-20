# chart-format-update-for-v6：Chart v6 模型、内置网格与 submesh 合同

状态：deferred；未排入当前实施序列

更新日期：2026-09-20

归档来源：[Chart v5 格式计划](../../active/chart-format-update-for-v5/plan.md)、
[CXC v1](../../../formats/CXC_FORMAT.md)、
[Portable Presentation v1](../../../formats/PORTABLE_PRESENTATION.md)、
[Material/Shader v1](../../../formats/MATERIAL_SHADER.md)、
[Chart v4](../../../formats/CHART_V4_FORMAT.md)、
[ADR 0038](../../../adr/0038-cxc-v1-and-chart-v4-boundary.md) 与
[Stage 11](../../future/stage-11/plan.md)。

本文件是讨论后的延期格式计划，不是 ADR、生产 Spec 或实施中的阶段。未恢复为 active 且未关闭
V6-0 门禁前，不得新增生产 Schema/Reader/Writer，也不得把 Chart v6 / Model v1 写成已可加载。
本阶段属于 Stage 9 的 Model/Environment 设计输入；恢复为 active、指定实施批次和日历排期
另由项目所有者决定。

## 1. 阶段目标

在 Chart v5 关闭并成为生产基线之后，建立 Chart v6 作为下一格式基线，使谱面能引用包内静态模型、
SDK 内置基础网格，以及按 submesh 槽绑定材质。v6 的对象层「形变」由既有 Behavior 驱动，不新开
morph/骨骼系统：非相机实体默认网格为扁平片，并沿用 `transform.scale` 的 X/Y/Z 缩放；其余
Behavior 属性、采样和混合规则与当时生产基线共用。

关闭并经 owner acceptance 后，新增模型/网格能力不再写入 v5 或更旧格式。关闭前 Chart v5（若已
关闭）或 Chart v4 仍是当时的生产 Playback 基线。

## 2. 恢复条件与排期约束

本计划保持 deferred，直到项目所有者同时接受下列条件：

- [Chart v5 格式计划](../../active/chart-format-update-for-v5/plan.md) 已关闭并经 owner
  acceptance；接手基线为 Chart v5 / CXT v2 / Stage 8 关闭报告记录的实际 SDK 发行版本，
  不预留固定 minor；版本选择遵循 [版本规范](../../../guides/VERSIONING.md)。
- 项目所有者把本计划改为 active，并指定 Stage 9 的具体实施批次。当前没有生产排期，也不得因
  写入本文而视为 Chart v6 已可加载。
- Stage 7 Judgement 不依赖本阶段；模型只改变 Presentation，不改变 Note/Requirement 的判定语义。
- V6-0：任何生产 Schema、Reader/Writer、新 Asset 类型或内置网格目录落地前，必须先有接受的
  Chart v6 ADR、`CHART_V6_FORMAT.md` 和 Model v1 Spec。

本阶段不是 Stage 6 媒体导入、Stage 7 Gameplay、Stage 8 Chart 发行或 Stage 10 Studio 的前置。
Studio 编辑器实现不是本阶段关闭条件。

## 3. 已锁定方向

下列条目由项目所有者于 2026-09-03 确认。恢复为 active 后，V6-B ADR 只许记录这些方向并附
正例/失败例，不得改回。字段形状、capability 字符串和冻结 tessellation 常数由 ADR/Spec 给出，
本文不充当生产 Spec。

### 3.1 Model v1 只接受静态 glTF 2.0

- 作者源格式 **只** 支持静态 glTF 2.0。拒绝带 animation、skin、morph target、camera、light 或
  运行时扩展的 glTF。
- 离线 importer 把合法静态 glTF 转为 Cuexis portable Model v1（`CXPRES01` 新 kind，具体编号由
  ADR 分配）。Playback / prepare **不** 解析 glTF，也不链接 glTF 库。
- CXC 只打包已经校验的 portable Model/Mesh/Material/Texture bytes。Pack 仍遵守 ADR 0038：不把
  作者 glTF 原文件当作 Playback 输入，不在 pack 时隐式转换。Source Project 可以保留 glTF；交换
  包内是 portable 载荷。
- 导入时把 glTF mesh primitive 变成 Model 的 submesh；节点局部 TRS 在导入时烘焙进 submesh
  顶点。v6 不在 Playback 中保留 glTF 场景图。
- 容器仍可以是 CXC v1。Asset Index 增加 `model` 类型（具体 index 版本由 ADR 决定）。Chart v6
  引用 Model 时，包闭包必须包含该 portable 载荷及其材质/纹理依赖。

### 3.2 SDK 内置基础网格

SDK 提供冻结的 `CXPRES01` Mesh bytes 目录，不在运行时生成几何。各网格有稳定 ID 和冻结 content
identity；FrameDigest 哈希这些 identity，禁止各宿主自行 tessellate。

第一期内置目录：

| 建议 AssetId | 形状 | 备注 |
| --- | --- | --- |
| `cuexis.builtin.mesh.triangle` | 单位三角形 | XY 平面，正面朝默认视线 |
| `cuexis.builtin.mesh.quad` | 单位方形 | XY 平面 |
| `cuexis.builtin.mesh.sheet` | 扁平片 | 非相机实体默认网格 |
| `cuexis.builtin.mesh.cube` | 单位立方体 | 轴对齐 |
| `cuexis.builtin.mesh.uv-sphere` | UV 球面 | 经纬分段在 ADR 冻结 |
| `cuexis.builtin.mesh.prism` | 正三棱柱 | 侧面分段在 ADR 冻结；与 cube 区分 |
| `cuexis.builtin.mesh.cylinder` | 圆柱 | 轴向与圆周分段在 ADR 冻结 |

几何约定（写入 ADR 时不得改坐标系）：

- Cuexis 空间仍是右手系、米制、`+X` 右、`+Y` 上、`+Z` 后，默认视线朝 `-Z`。
- 扁平片/quad/triangle 位于 XY 平面，正面朝 `-Z`，原点在几何中心。X/Y 缩放改变面内尺寸，Z
  缩放改变厚度方向；扁平片本身是零厚度三角网，面积必须非零。
- 棱柱与圆柱的高度默认沿 `+Y`。
- UV-sphere / cylinder / prism 的分段数量是冻结常数，不是作者参数。v6 不提供运行时「N 边棱柱」。

内置网格是 CXC 闭包的显式例外：Chart 引用 `cuexis.builtin.mesh.*` 时，包内不必重复该 blob。
缺对应 capability 的裁剪 Session 稳定拒绝。建议 capability：`cuexis.mesh.builtin.v1`。默认
`allCapabilities()` 在 v6 SDK 中包含它；ADR 可另列内置 Unlit 材质（绘制默认扁平片所必需）。

### 3.3 默认扁平片与 Behavior 形变

- Chart v6 中，**除相机外** 的实体若带 `cuexis.renderable` 且省略 `mesh`，默认使用
  `cuexis.builtin.mesh.sheet`。相机对象不得获得该默认网格。
- 没有 `cuexis.renderable` 的对象仍不绘制。v6 不给纯 Transform/Note 对象自动插入 Renderable。
- 「形变」由 Behavior 控制，使用既有 `transform.scale` 的 X/Y/Z 分量（Override 有限值，Additive
  为正因子）。不新增 `PropertyId`、不把 Mixer 改成网格空间、不引入 morph/骨骼。
- 其余 Behavior / AnimationClip 属性、Hermite、Seek、绝对 Beat 采样与当时生产白名单 **共用**，
  不另开一套形变 Behavior。v5 的 `render.alpha` 整数作者层合同继续沿用，不在本阶段重开。
- 显式 `mesh` 或 Model 引用覆盖默认扁平片；覆盖后仍可用同一套 scale Behavior。

### 3.4 Submesh 材质槽

- Model v1 拥有有序 submesh 槽。每个槽绑定一块 portable Mesh 范围和默认 Material。
- Chart 对象可覆盖槽材质，引用已有 Unlit 或 ParameterizedMaterial。不在本阶段引入 PBR。
- 单 mesh 的内置网格视为 **一个槽**。`cuexis.renderable.material` 覆盖该槽。
- 多槽覆盖写在 renderable 的槽表中；未覆盖的槽使用 Model 内默认材质。
- 若 v6 允许按槽动画材质，必须使用带槽索引的离散 step 属性，并同步升高 AnimationClip /
  `behavior.event` / CXT 版本，禁止一名两义。若第一期只做静态槽表，CXT 可继续与 Chart v5 的
  可动画白名单对齐，但仍须在 ADR 中写明「无新可动画属性则不升 CXT」。

### 3.5 公共观察面（恢复实施时冻结）

实施前由 ADR 冻结。预期方向，不是现行 SDK 合同：

- Playback 不增加 glTF 类型。
- 内置网格以稳定 AssetId + content identity 出现在 presentation manifest。
- 单槽对象可继续用一个 mesh ref + 一个 material ref。
- 多槽 Model 需要 snapshot 能观察槽列表或 Model ref + 槽覆盖；若因此增加被哈希字段，必须单列
  FrameDigest 新版本，且 **不得** 改变合法 v4/v5 输入的既有 digest。
- 默认扁平片不得改变相机 snapshot 字段。

### 3.6 Presentation Environment 预留

天空盒不作为普通 Chart Object、Renderable Mesh 或父子层级实体实现。它属于独立的
Presentation Environment，候选字段和资源类型应在后续 ADR 中单独冻结：

```text
skybox mode       solid-color | equirectangular | cubemap
resource          typed Asset reference
orientation       environment rotation, without camera translation/parallax
appearance        intensity/tint, subject to presentation capability
```

v6 不要求交付天空盒，但不得把天空盒语义塞入 `cuexis.renderable`、默认扁平片或普通透明排序。
未来环境配置应能由 Chart 声明播放语义，由 Presentation/Project 提供资源和能力；若环境状态进入
公共 FrameSnapshot 或 FrameDigest，必须形成独立版本合同。Packed Chart 也应将环境区段与普通
ObjectTable 分离。

## 4. 实施范围（恢复为 active 之后）

- 盘点 v5 关闭时的 Mesh/Material/Asset Index/CXC 闭包与 `propertyCount`。
- 产出 Chart v6 ADR、`CHART_V6_FORMAT.md`、Model v1 Spec 和静态 glTF 2.0 importer 合同。
- 冻结内置网格 bytes、分段常数、capability 与闭包例外。
- 实现 Model v1 Schema/Reader、importer、Chart v6 默认扁平片、submesh 槽覆盖和 capability
  preflight。
- 将 `--target 6` 与默认 Writer 切换规则写入 ADR（恢复实施时决定是否从当时默认 v5 直切 v6）。
- Playback 继续直接求值未迁移的既有 Chart 版本；删除旧 Reader 仍按 ADR 0041，不在本阶段关闭。

## 5. 验收标准

- V6-0 在生产 Schema/Reader 之前关闭；ADR/Spec 覆盖第 3 节已锁定方向，且未改回 glTF 运行时解析、
  morph/骨骼或运行时 tessellate。
- 静态 glTF 2.0（无 animation/skin/morph）可离线导入为 Model v1；非法 glTF 稳定拒绝。Playback
  热路径不调用 glTF 解析器。
- CXC 中的模型是 portable 载荷；缺依赖或非 portable 作者文件作为 Playback 输入时稳定失败。
- 七种内置网格均可被 Chart 引用且不必打进 CXC；裁剪 builtin capability 时稳定拒绝。同一 SDK
  版本的内置 bytes 与 content identity 跨平台一致。
- 非相机、带 `cuexis.renderable` 且省略 `mesh` 的对象绘制为扁平片；相机永不默认扁平片。
- X/Y/Z 缩放由既有 `transform.scale` Behavior/Clip 驱动；与 v5 共用的其它 Behavior 属性在未迁移
  语义上保持可用。Seek 后 scale 仍由绝对 Beat 得到，不依赖上一帧。
- Model 多槽材质可被 Chart 覆盖；未覆盖槽使用 Model 默认材质。单槽内置网格仍是一对 mesh+material。
- 合法 v4/v5 输入的 FrameDigest 既有版本结果不变。
- 本阶段关闭早于 Stage 11 启动的暂定约束被核对，或由项目所有者书面改写该约束。

## 6. 明确不包含

- 把本工作并入 Chart v5、Stage 6 媒体导入、Stage 7 Gameplay、Stage 8 Chart 发行或 Stage 10 Studio。
- 在 Playback 中解析或打包未转换的 glTF/OBJ/FBX 作为运行时网格。
- 带动画、蒙皮、morph target 的 glTF；运行时骨骼、布料、顶点缓存累加。
- 运行时按 N 生成棱柱/圆柱/球面；宿主自定义 tessellation。
- PBR、新光照模型、Shader Graph。
- 给没有 `cuexis.renderable` 的对象自动插入网格。
- 每帧朝向相机的 billboard（扁平片是固定局部网格，靠 Transform/Behavior 转向）。
- 新增与 `transform.scale` 平行的第二套形变 PropertyId。
- 重开 Chart v5 的 alpha 单位、CXT v2 白名单或 `propertyCount = 10` 锁（除非 ADR 为槽动画单列
  新离散属性，且不得把 v5 clip 一名两义）。
- 因本计划修改 Stage 11 判定语义。默认扁平片只影响表现，不改变 Note beat / ObjectId 合同。
- 未关闭 V6-0 即落地生产 Schema，或把候选 Chart v6 写成已可加载。
- 日历排期、立即把本计划改为 active，或把 Stage 6–10 标为等待 v6。
- 曲线形变、line 粗细/棱柱渲染、包级 `shader.json` 后处理接口。该项见
  [Chart v7 延期计划](../chart-format-update-for-v7/plan.md)。

## 7. 交接

恢复并关闭后，Chart v6 是新增模型/默认网格/submesh 的作者基线。交给后续阶段（含 Stage 11）的
方向：

```text
Chart v6；Model v1（静态 glTF 2.0 -> portable）
内置网格：triangle / quad / sheet / cube / uv-sphere / prism / cylinder
非相机 renderable 省略 mesh => sheet
形变：Behavior transform.scale X/Y/Z；其余 Behavior 与当时白名单共用
submesh 槽：静态覆盖；按槽动画须升 Clip/CXT/behavior 版本
CXC v1 可含 portable Model；内置网格不进包闭包
Playback 不解析 glTF
未迁移的 v1–v5 继续直接求值
曲线形变 / line / shader.json 接口交给 Chart v7
```

Stage 11 不得假设 v4/v5 的「无默认网格」作者习惯仍然成立，也不得把内置扁平片当成判定几何。
Chart v7 在本阶段关闭之后才恢复；见 [v7 计划](../chart-format-update-for-v7/plan.md)。
)
