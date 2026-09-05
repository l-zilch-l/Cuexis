# chart-format-update-for-v8：Chart v8 双轴贝塞尔形变、模型动画与内置后处理

状态：deferred；未排入当前实施序列

更新日期：2026-09-03

归档来源：[Chart v7 延期计划](../chart-format-update-for-v7/plan.md)、
[Chart v6 延期计划](../chart-format-update-for-v6/plan.md)、
[Material/Shader v1](../../../formats/MATERIAL_SHADER.md)、
[ADR 0008](../../../adr/0008-coordinate-transform-hierarchy.md) 与
[Stage 11](../../future/stage-11/plan.md)。

本文件是讨论后的延期格式计划，不是 ADR、生产 Spec 或实施中的阶段。未恢复为 active 且未关闭
V8-0 门禁前，不得新增生产 Schema/Reader/Writer，也不得把 Chart v8、双轴形变或内置后处理写成
已可加载。本阶段属于 Stage 11 的高级 Presentation 设计输入，排在 Chart v7 关闭之后；
日历排期另由项目所有者决定。

v8 落实 v7 留给后续版本的 **双轴贝塞尔映射**，并为模型提供基本动画行为；同时把 v7 只冻结接口
的 `shader.json` **补全为可运行的内置后处理**。v8 **仍不** 实现 `formula` kind。

## 1. 阶段目标

在 Chart v7 关闭之后，建立 Chart v8。v8 应当：

- 实现 **双轴** 分段三次贝塞尔曲线形变（`mappingAxes` count = 2）。单轴 v7 路径继续可用。全系列
  上限仍是两轴，永不三轴。
- 在双轴标架下为模型提供 **基本动画行为**：作者用既有 Behavior / Clip 驱动两映射轴上的位移、
  旋转、缩放与可见性；SDK 提供少量内置动画预设。不引入骨骼或 morph。
- **完善 shader**：兑现 v7 `shader.json` 接口，提供 SDK **内置后处理效果** 并在 Player / in-tree
  adapter 中实际执行。时间输入仍是 `chartTimeMs`。

关闭并经 owner acceptance 后，双轴贝塞尔与内置后处理不再写入 v7 或更旧格式。关闭前的生产基线
仍是当时已关闭的 Chart 版本。

## 2. 恢复条件与排期约束

本计划保持 deferred，直到项目所有者同时接受下列条件：

- [Chart v7 计划](../chart-format-update-for-v7/plan.md) 已关闭并经 owner acceptance。v8 不在
  v7 未关闭时开工，也不把双轴映射或后处理绘制偷写进 v7。
- 项目所有者把本计划改为 active。当前 **没有日历排期**，不得因写入本文而插入 Stage 6–10。
- 暂定完成约束：本阶段关闭早于 [Stage 11](../../future/stage-11/plan.md) 启动。
- V8-0：生产 Schema/Reader/Writer、双轴求值或后处理绘制落地前，必须先有接受的 Chart v8 ADR、
  `CHART_V8_FORMAT.md`、双轴贝塞尔 Spec 和内置后处理效果目录 Spec。

本阶段不是 Stage 6–10 的前置。Studio 编辑器、`formula` kind 和 Judgement 都不是本阶段关闭条件。

## 3. 已锁定方向

下列条目由项目所有者于 2026-09-03 确认。恢复为 active 后，V8-B ADR 只许记录这些方向并附
正例/失败例，不得改回。

### 3.1 双轴贝塞尔形变

v8 实现 v7 第 3.1 节已冻结的两轴上限，曲线表示仍 **只有** 分段三次贝塞尔。

```text
mappingAxes     恰好 2 条，取自 {X, Y, Z}，互不相同
参数            (u, v) 分别沿这两条局部轴
曲线            两轴共享一张映射平面上的贝塞尔，或每轴一条贝塞尔（ADR 冻结一种并附失败例）
第三轴          欧氏偏移，不参与曲线映射
v7 单轴文档     继续求值；不得改其贝塞尔 identity
formula         v8 仍拒绝
count = 3       稳定拒绝
```

合成顺序由 ADR 单列并附矩阵。建议默认（可被 ADR 改写，但必须写死一种）：

```text
先沿第一 mappingAxis 做与 v7 相同的单轴贝塞尔映射
再在得到的标架里沿第二 mappingAxis 做第二条贝塞尔映射
子对象与顶点使用同一 (u, v) → 标架
```

锁定规则：

- 采样由绝对 Beat 与当前局部坐标一次算完，禁止累积切线。Seek 后同一 `(chartTime, 局部 TRS)`
  得到同一世界姿态。
- 标架仍用平行移动，C0 必须；C1 策略沿用 v7 ADR。
- 子对象坐标系随双轴标架变化；这是 ADR 0008 单一父矩阵在 **声明了曲线形变的对象** 上的既有
  例外向两轴的延伸，不是新的第三套 Transform。
- 未声明双轴的 v7 单轴对象、以及无曲线形变的对象，行为与关闭时的 v7 一致。

规范例子（ADR golden；全是贝塞尔，不是正弦公式）：

```text
mappingAxes = {X, Y}
父：两条（或一张平面）分段三次贝塞尔
子：cube
cube 局部 X、Y 在矩形域内运动时，世界轨迹落在双轴映射后的曲面上（或曲网折线上）
局部 +X / +Y 对齐 ADR 写下的双轴标架
```

### 3.2 模型的基本动画行为

「基本动画」指在 v8 模型与双轴标架上，用 **既有** Behavior / AnimationClip 白名单驱动对象，而不是
新开骨骼系统。

v8 必须保证下列行为在双轴形变父、普通 v6 网格和内置模型上语义一致（除曲线标架本身外）：

```text
transform.position / rotation / scale
render.visible / render.alpha
render.material（含 v6 槽覆盖，若当时已交付）
```

另外提供 **少量 SDK 内置动画预设**（CXT 或等价 Clip，具体打包方式由 ADR 冻结），例如：

```text
沿第一映射轴往返
沿第二映射轴往返
双轴矩形扫描
静止呼吸缩放（沿用 v6 scale，不经曲线）
```

预设是冻结的贝塞尔/Clip bytes，走既有 capability/闭包例外或包内资源，不引入脚本。作者仍可写
自己的 Behavior；预设只是默认「基本动作」，不是唯一动画入口。

不在 v8 动画：控制点逐帧编辑器、公式曲线、骨骼、morph、运行时生成贝塞尔。

### 3.3 完善 shader：内置后处理效果

v7 已冻结包根 `shader.json`（可空 = SDK 默认）和基于 `chartTimeMs` 的接口，但 **不绘制**。
v8 把该接口补全为可执行管线。

```text
空 / 省略 / {}     仍等于 SDK 内置默认（identity 或 ADR 冻结的默认效果链）
非空               只能引用 SDK 内置后处理 id + 有类型的参数
绘制               Player 与 in-tree OpenGL adapter 必须执行
时间               RuntimeFrame.chartTimeMs，不是墙钟
```

第一期内置效果目录由 ADR 冻结，建议至少包含可开关、可调强度的：

```text
identity（空操作，作为默认）
fade / 透明度叠化
vignette
color-multiply（与对象 tint 分离，只作用于后处理）
```

锁定规则：

- 未知效果 id、未知参数、越界强度稳定拒绝，不得半应用。
- 内置效果的 GLSL/SPIR-V 属于 SDK，走既有 Stage 5 可选编译工具链或冻结 blob；**不** 让
  `shader.json` 内嵌用户 shader 源，也不在 Playback 热路径编译。
- 后处理不进入 Chart 对象 PropertyId，不增加 `propertyCount` 的动画通道，除非 ADR 为「效果
  强度」单列且升高 Clip 版本。第一期强度建议只写在 `shader.json` 静态参数里，可用时间公式
  仅限 ADR 白名单（例如线性 fade），仍禁止脚本。
- capability 例如 `cuexis.presentation.postprocess.builtin.v1`。裁剪 Session 在非空且非
  identity 的 `shader.json` 上稳定拒绝。
- 合法无后处理的 v4–v7 输入，既有 FrameDigest 版本结果不变。启用后处理若改变 snapshot 颜色，
  必须单列 digest 或明确后处理 **不进入** 既有 digest（ADR 二选一，建议：后处理不进入
  FrameDigest，保持表现校验与谱面校验分离）。

### 3.4 与既有合同的边界

- 不重开 v5 alpha、v6 默认扁平片/submesh、v7 单轴贝塞尔 identity。
- v8 仍拒绝 `formula` kind 与自由数学字符串。
- 运行时脚本仍然无限期延后。内置后处理不是脚本入口。
- Stage 5 的物体 ParameterizedMaterial / Shader asset 合同不因后处理而改字段。

## 4. 实施范围（恢复为 active 之后）

- 盘点 v7 单轴贝塞尔、`shader.json` 校验与 Stage 5 shader 工具链。
- 产出 Chart v8 ADR、双轴贝塞尔 Spec、内置动画预设表和内置后处理目录 Spec。
- 实现双轴 mappingAxes、子对象跟随、基本动画预设、以及 Player/OpenGL 内置后处理绘制。
- v7 单轴 golden 与空 `shader.json` 回归必须通过。
- Playback 继续直接求值未迁移的既有 Chart 版本。

## 5. 验收标准

- V8-0 在生产 Schema/Reader 之前关闭；未把公式 kind、三轴映射或用户后处理源码塞进 v8。
- 双轴贝塞尔 golden：子对象在 (u,v) 矩形域运动的世界轨迹与 ADR 矩阵一致；Seek 不漂移。
- v7 单轴贝塞尔文档在 v8 Playback 下 identity 与轨迹不变。
- `mappingAxes` 不是 2、重复轴、或含第三轴时稳定拒绝（单轴文档走 v7 路径，不算 v8 双轴）。
- 基本动画预设可绑定到内置/包内模型，并与手写 Behavior 共用白名单。
- 空 `shader.json` 与 v7 默认等价。引用内置效果时 Player 可见后处理；未知 id 失败。
- 既有 v4–v7 digest 在未启用改变颜色的后处理时不变。
- 本阶段关闭早于 Stage 11 启动的暂定约束被核对，或由项目所有者书面改写。

## 6. 明确不包含

- 把双轴映射或后处理绘制并入 Chart v7。
- `formula` kind、闭式正弦、自由表达式、脚本。
- 三轴映射、骨骼、morph、控制点运行时生成。
- 用户在 `shader.json` 里写 GLSL/HLSL、Shader Graph、任意全屏 pass 图。
- 把后处理参数做成与 v5 alpha 平行的第二套对象透明度。
- 修改 Stage 11 判定语义。
- 未关闭 V8-0 即落地生产 Schema，或把候选 Chart v8 写成已可加载。
- 日历排期、立即改为 active，或让 Stage 6–10 等待 v8。

## 7. 交接

```text
Chart v8；双轴贝塞尔 mappingAxes count = 2
v7 单轴贝塞尔 identity 不变
模型基本动画：既有 Behavior/Clip + 少量 SDK 预设
shader.json：内置后处理可执行；仍无用户 shader 源
formula kind：仍后续
三轴：永不
未迁移的既有 Chart 继续直接求值
```

`formula` 与更多后处理效果若还需要，另立后续谱面版本。Stage 11 仍不得把曲线轨迹默认当成判定线。
)
