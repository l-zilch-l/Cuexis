# chart-format-update-for-v7：Chart v7 曲线形变、line 模型与后处理接口

状态：deferred；未排入当前实施序列

更新日期：2026-09-03

归档来源：[Chart v6 延期计划](../chart-format-update-for-v6/plan.md)、
[Chart v5 格式计划](../../active/chart-format-update-for-v5/plan.md)、
[CXC v1](../../../formats/CXC_FORMAT.md)、
[Portable Presentation v1](../../../formats/PORTABLE_PRESENTATION.md)、
[Material/Shader v1](../../../formats/MATERIAL_SHADER.md)、
[ADR 0008](../../../adr/0008-coordinate-transform-hierarchy.md)、
[ADR 0038](../../../adr/0038-cxc-v1-and-chart-v4-boundary.md) 与
[Stage 11](../../future/stage-11/plan.md)。

本文件是讨论后的延期格式计划，不是 ADR、生产 Spec 或实施中的阶段。未恢复为 active 且未关闭
V7-0 门禁前，不得新增生产 Schema/Reader/Writer，也不得把 Chart v7、曲线形变或 `shader.json`
写成已可加载或已实现后处理。本阶段属于 Stage 9 的 Geometry Deformation 设计输入，排在
Chart v6 关闭之后；日历排期另由项目所有者决定。

曲线形变是本阶段的重点，且 **会在后续谱面版本中继续更新**。后处理着色器只冻结包文件与公共接口，
**不实现** 效果管线。

## 1. 阶段目标

在 Chart v6 / Model v1 关闭之后，建立 Chart v7，继续细化对象层形变，并增加包级后处理着色器
接口。v7 应当：

- 把 **曲线形变** 做成作者可绑定的空间合同。映射轴取自局部 `{X, Y, Z}`，**全系列最多两轴**，
  永不做三轴体素形变。v7 先交付 **单轴** 映射；第二轴留给后续格式版本，复用同一套曲线表示。
  子对象坐标系随曲线标架一起变。
- 曲线表示在 v7 **只** 使用分段三次 **贝塞尔**。不实现数学公式 kind、表达式字符串或正弦闭式
  求值。公式白名单（含精确正弦）留给后续谱面版本。
- 增加内置 **line（细线）** 以及更多常见模型。line 可设渲染粗细，或按棱柱挤出渲染。
- 在 CXC 中增加独立文件 `shader.json`（允许为空）。空文件或省略时使用 SDK 内置 shader 设置。
  本文冻结基于时间的后处理 **基础语义和接口**；v7 **不实现** 后处理绘制。

关闭并经 owner acceptance 后，新增曲线形变、line 渲染模式和包级 `shader.json` 不再写入 v6 或
更旧格式。关闭前的生产基线仍是当时已关闭的 Chart 版本（v6 或更早）。

## 2. 恢复条件与排期约束

本计划保持 deferred，直到项目所有者同时接受下列条件：

- [Chart v6 计划](../chart-format-update-for-v6/plan.md) 已关闭并经 owner acceptance。v7 不在
  v6 未关闭时开工，也不把曲线形变偷写进 v6。
- 项目所有者把本计划改为 active，并指定实施批次。当前 **没有日历排期**，也不得因写入本文而视为
  已插入 Stage 6–10。
- 暂定完成约束：本阶段关闭早于 [Stage 11](../../future/stage-11/plan.md) 启动。该约束不是开工
  扳机。
- V7-0：任何生产 Schema、Reader/Writer、内置 line bytes、曲线帧实现或 `shader.json` 落地前，
  必须先有接受的 Chart v7 ADR、`CHART_V7_FORMAT.md`、曲线形变 Spec 和 `shader.json` 接口 Spec。
  后处理 Spec 只定义语义与接口，验收不要求 Player/adapter 执行用户后处理。

本阶段不是 Stage 6–10 的前置。Studio 编辑器、真实后处理实现和 Judgement 都不是本阶段关闭条件。

## 3. 已锁定方向

下列条目由项目所有者于 2026-09-03 确认。恢复为 active 后，V7-B ADR 只许记录这些方向并附
正例/失败例，不得改回。字段名、capability、曲线帧公式和 tessellation 由 ADR/Spec 给出，本文
不充当生产 Spec。

### 3.1 曲线形变（本阶段重点，后续版本继续）

曲线形变把对象局部空间中 **至多两个** 轴映射到曲线，其余轴成为曲线局部标架中的欧氏偏移。
这是整条「曲线形变」产品线的上限，不因后续更新而放宽到三轴。

```text
mappingAxes  1 或 2 条，取自 {X, Y, Z}，互不相同
v7           只实现 count = 1（单参数 u 沿该轴）
Chart v8     实现 count = 2（仍用贝塞尔；见 v8 计划）
禁止         count = 3、任意顶点 morph、骨骼、自由数学字符串
```

v7 单轴语义：

```text
作者选择 deformAxis ∈ {X, Y, Z}
沿该轴的局部坐标 u 是曲线参数（规范空间与单位由 ADR 冻结）
另外两轴是曲线标架的法向 / 副法向偏移
顶点与子对象使用同一映射
```

后续两轴语义（只冻结上限，不在 v7 实现）：第二映射轴给出第二个参数 v；v7 与紧随其后的两轴更新
仍只求值贝塞尔。第三轴保持欧氏偏移。两轴的合成顺序由后续 ADR 单列；v7 Reader 遇到两个
mappingAxes 稳定拒绝。

锁定规则：

- v7 一个带曲线形变的模型 **只能** 沿 X/Y/Z **之一** 做参数映射。两轴映射是后续版本的合法扩展，
  不是 v7 交付物，也不是三轴体素形变。
- v6 的 `transform.scale` X/Y/Z 仍可用，且发生在曲线映射之前的局部空间（先 scale，再沿轴
  送入曲线）。不得用曲线形变替换 v6 缩放。
- 曲线来源是父对象的 line/曲线资源（内置或包内 portable 曲线），表示见第 3.1.1 节。
- 采样必须由绝对 Beat 与当前局部坐标一次算完，禁止依赖上一帧切线或累积旋转。Seek、reload、
  不同帧率下同一 `(chartTime, 局部 TRS)` 得到同一世界姿态。
- 标架在拐点处必须稳定。ADR 应冻结 **平行移动（parallel transport）** 一类无 Frenet 翻转的
  标架；不得把拐点处的法线跳变当成作者特性。Bezier 折点/C0 连接处是必测 golden。

规范例子（写入 ADR 的 golden；全部是贝塞尔控制点，不是闭式正弦）：

```text
父：分段三次贝塞尔路径，deformAxis = X（控制点由 ADR 冻结）
子：cube，parent 为该曲线对象
cube 的局部 X 从 0 运动到 100，Y=0，Z=0
世界轨迹 = 该贝塞尔在参数对应 X∈[0,100] 上的点
cube 的局部 +X 对齐曲线切线，局部 +Y 对齐曲线标架法向（精确轴由 ADR 用矩阵写下）
```

不得把有限段贝塞尔标成「精确正弦」。若后续版本增加 `formula:sine`，必须另开 golden，且不得与
v7 贝塞尔 identity 混成同一资源。

### 3.1.1 曲线表示规范：v7 仅贝塞尔

v7 的作者层与 portable 曲线资源 **只有** 分段三次贝塞尔。不设 `formula` kind。

```text
v7 曲线     分段三次贝塞尔；每段 4 个控制点
维度        映射平面内的二维路径，或三维路径（由 ADR 冻结一种默认并附失败例）
连接        C0 必须；是否要求 C1 由 ADR 冻结
identity    canonical 控制点 bytes；不经公式求值器
后续版本    可追加 tagged `formula`（白名单 id + 系数）；v7 Reader 遇到 formula 稳定拒绝
禁止        自由数学字符串、脚本、未版本化表达式树、无界次多项式
```

选择理由：贝塞尔可作者、可哈希、不需要公式求值器，适合 v7 的单轴路径形变。闭式正弦/直线公式
会引入第二套求值与 identity 规则，放到后续版本，避免 v7 同时维护两种曲线语义。

内置直线 line 是冻结的贝塞尔（两端点，或等价的退化三次段），不是 `formula:linear`。

子对象的坐标系随曲线形变改变，因此 `World = ParentWorld * Local` 在 **带曲线形变的父对象上**
不再是单一父矩阵。这是相对 [ADR 0008](../../../adr/0008-coordinate-transform-hierarchy.md) 的
**v7 显式例外**：

- 只作用于声明了曲线形变的 v7 父对象。
- v1–v6 以及未声明曲线形变的 v7 对象，仍使用单一 `ParentWorld * Local`。
- 子对象的局部 TRS 仍是作者字段；世界姿态 = 在子对象原点处采样的曲线标架 × 局部 TRS。
- 不得把曲线剪切静默塞进 Quaternion。若某操作需要 KeepWorld 且无法表示为局部 TRS，该操作
  失败（沿用 ADR 0008 的剪切拒绝），不在曲线父上发明第二种静默降级。

未声明曲线形变时，v6 默认扁平片与 scale Behavior 不变。

### 3.2 内置 line 与更多常见模型

在 v6 内置目录之上，v7 **必须** 增加：

| 建议 AssetId | 形状 | 备注 |
| --- | --- | --- |
| `cuexis.builtin.mesh.line` | 沿局部 X 的单位细线 | 冻结直线贝塞尔；粗细见 3.3 |

「更多常见模型」由 ADR 冻结扩充目录，不得运行时 tessellate。建议第一期至少再评估并冻结：

```text
cone、capsule、torus、pyramid、ring
```

ADR 可删减该建议列表，但不得删掉 `line`。不在 v7 内置正弦 line；作者波浪路径写贝塞尔控制点。
正弦闭式预设若需要，归后续公式 kind，不塞进 v7 目录。

内置网格仍是 CXC 闭包例外，capability 沿用并扩展 `cuexis.mesh.builtin.v1`（或 v7 新 ID，由
ADR 单列）。禁止宿主用 GL_LINES 宽度冒充 identity。

### 3.3 line 渲染：粗细或棱柱

line 作者可选择一种渲染模式，二者都是确定性世界空间结果，不是屏幕像素线宽：

```text
stroke   沿 line 生成固定世界粗细的带状三角网（厚度单位：米）
prism    沿 line 按 v6 正三棱柱截面挤出
```

- 粗细是对象/组件上的静态或可动画标量，范围与默认值由 ADR 冻结；必须有限且严格为正。
- **禁止** 使用图形 API 的 line width 作为可观察合同。
- prism 模式复用 v6 棱柱截面合同，沿 line 切线挤出；不新开任意 N 边。
- 当前 Mesh v1 只有 triangle list。line 的 portable 表示由 ADR 选择：导入/内置阶段生成三角网，
  或新增受预算约束的拓扑并在提交前展开为三角网。Playback 热路径仍只提交 portable 三角网。
- 粗细变化不得改写第 3.1 节的曲线参数轴；它只改变截面，不改变子对象沿曲线的 u。

### 3.4 子对象绑定与 Behavior

- 沿用 Chart 既有 `parent` 引用。曲线父上的子对象自动进入曲线标架，不要求第二套绑定表。
- 子对象仍可用 v5/v6 Behavior：位移、旋转、缩放、`render.alpha` 等当时白名单。规范例子里
  cube 沿局部 X 的运动就是既有 `transform.position.x`。
- 子对象可以再挂孙对象；每层若自身没有曲线形变，只继承父级已经变形后的标架。v7 第一期不要求
  多层曲线形变嵌套；若子对象也声明曲线形变，ADR 必须单独定义合成顺序，否则稳定拒绝嵌套曲线。
- 相机不得作为曲线形变网格的默认主体；相机父到曲线对象时，ADR 须写明是跟随曲线标架还是拒绝。
  建议：允许相机作为子对象跟随曲线，但相机本身不带 line 网格。

### 3.5 CXC `shader.json`：后处理接口，不实现

CXC 增加包根下的独立文件 **`shader.json`**（用户给定文件名；不是 Stage 5 的 Shader asset）。

```text
路径        shader.json（包根，portable ASCII）
允许为空    是：空对象 {}、空文件、或省略（ADR 须把三种等价为 SDK 内置默认）
默认        SDK 内置 shader 设置（identity / 无用户后处理）
本阶段      只提供基础语义和公共接口
本阶段不做  编译、绑定、绘制或缓存用户后处理 shader
```

基础语义（写入接口 Spec，仍不实现管线）：

- 时间输入是 Playback 的 **chart 时间**（与 `RuntimeFrame.chartTimeMs` 同一时钟），不是墙钟、
  不是随机、不是脚本。
- 描述后处理通道的声明式结构：输入颜色、可选分辨率、时间 uniform 名称与单位。
- 未知字段失败；不允许运行时脚本或 `#include` 任意源。

公共接口方向：

- 安装的 Playback 头可出现后处理 **描述类型** 与 capability（例如
  `cuexis.presentation.postprocess.v1`），使宿主能读取包内声明。
- Cuexis Player 与 in-tree OpenGL adapter 在 v7 **不执行** 这些通道：prepare 校验 JSON，
  空/默认通过；非空声明若当前 Session 未实现后处理，则按 capability 稳定拒绝或显式 no-op，
  由 ADR 二选一并附失败例。不得半应用一张用户 shader。
- 本文件不进入 Stage 5 的 `CXPRES01` Shader/ParameterizedMaterial 路径，也不把
  `cuexis_asset_importer --compile` 接到后处理。
- 容器是否因此升到 CXC v2 由 ADR 决定；锁定的是「有独立 `shader.json`」。不得把后处理 JSON
  塞进 Chart 文档或 CXT。

### 3.6 与既有合同的边界

- 不重开 Chart v5 alpha、CXT v2 白名单，以及 v6 的静态 glTF / 默认扁平片 / submesh 槽。
- 曲线形变若需要新的静态组件字段，优先放在 renderable/model 组件上，而不是新开与
  `transform.scale` 平行的连续 PropertyId。若必须动画「是否启用曲线」或粗细，ADR 单列属性并
  升高 Clip/`behavior.event`/CXT 版本，禁止一名两义。
- 合法 v4–v6 输入的既有 FrameDigest 版本结果不变。曲线标架若进入 snapshot，必须单列 digest
  新版本。
- 运行时脚本仍然无限期延后。`shader.json` 不是脚本入口。

## 4. 实施范围（恢复为 active 之后）

- 盘点 v6 关闭时的内置网格、parent 矩阵、Mesh triangle-list 和 Stage 5 shader 边界。
- 产出 Chart v7 ADR、曲线形变 Spec（v7 仅贝塞尔、映射轴上限）、line 渲染 Spec 和 `shader.json`
  接口 Spec。
- 冻结贝塞尔段预算、控制点有限性、标架公式、stroke/prism 展开规则和 `shader.json` Schema。
  明确 v7 拒绝两轴 mappingAxes，并拒绝 `formula` kind。
- 实现 Chart v7 Reader/Writer、内置 line、曲线父标架、子对象跟随，以及 `shader.json`
  的 parse/validate/默认等价；**不** 实现后处理 GPU 通道。
- Playback 继续直接求值未迁移的既有 Chart 版本。删除旧 Reader 仍按 ADR 0041。

## 5. 验收标准

- V7-0 在生产 Schema/Reader 之前关闭；ADR/Spec 覆盖第 3 节，且未改回三轴映射、v7 内的公式
  kind、自由表达式字符串、GL line width、或 v7 内实现用户后处理绘制。
- 贝塞尔 golden 成立：子 cube 沿局部 X 从 0 到 100 的世界轨迹与冻结控制点路径一致；切线/法向
  按 ADR 矩阵对齐。Seek 后轨迹不漂移。
- v7 只接受单一 deformAxis ∈ {X,Y,Z}，且曲线仅为分段三次贝塞尔；两个 mappingAxes、三轴或
  `formula` kind 稳定拒绝。后续版本最多升到两轴；公式 kind 另立后续版本，不得在 v7 偷加。
- 未声明曲线形变的对象仍服从 ADR 0008 单一父矩阵；合法 v4–v6 digest 不变。
- `line` 支持 stroke 粗细与 prism 模式；同一输入跨平台 identity 一致。
- `shader.json` 可省略或为空，并与 SDK 内置默认等价。非空文件能被 prepare 校验。Player/OpenGL
  不绘制用户后处理；接口类型与 capability 有头文件与测试，但没有效果实现断言。
- 内置 `line` 可引用且不必打进 CXC。
- 本阶段关闭早于 Stage 11 启动的暂定约束被核对，或由项目所有者书面改写。

## 6. 明确不包含

- 把曲线形变、line 或 `shader.json` 并入 Chart v5、v6、Stage 5 Shader 管线或 Stage 6–10。
- 在 v7 实现后处理绘制、全屏 pass、bloom/tonemap 产品效果或用户 GLSL 编译。
- 把 `shader.json` 当作运行时脚本、Shader Graph 或 Chart 内嵌 shader 源。
- 三轴曲线映射、morph target、骨骼、布料、累积顶点缓存。
- 在 v7 实现 `formula` kind、闭式正弦求值、自由数学字符串或脚本。
- 在 v7 实现两轴 mappingAxes 或内置后处理绘制（该项见
  [Chart v8 延期计划](../chart-format-update-for-v8/plan.md)）。
- 把有限段贝塞尔标称为精确正弦，或内置 `line-sine-xy` 作为 v7 必选目录。
- 用 GL/D3D/Vulkan line width 作为粗细合同；屏幕空间像素粗细。
- 每帧朝向相机的 line billboard（stroke 带状网在曲线标架内，不追相机）。
- 多层曲线形变嵌套（除非 ADR 另文定义）；静默把曲线剪切写成 Quaternion。
- 修改 Stage 11 判定语义。曲线只影响表现空间，不改变 Note beat / ObjectId。
- 未关闭 V7-0 即落地生产 Schema，或把候选 Chart v7 写成已可加载。
- 日历排期、立即改为 active，或让 Stage 6–10 等待 v7。

## 7. 交接

恢复并关闭后，Chart v7 是曲线形变、line 渲染模式和 `shader.json` 接口的作者基线。交给后续
阶段的方向：

```text
Chart v7；曲线形变：v7 单轴映射；全系列最多两轴，永不三轴
v7 曲线：仅分段三次贝塞尔
formula kind / 闭式正弦：后续版本，v7 拒绝
golden：冻结贝塞尔控制点 + child cube 沿局部 X
内置：v6 目录 + line（直线贝塞尔）+ ADR 冻结的常见模型
line 模式：stroke 世界粗细 | prism 挤出
shader.json：可空；默认 SDK 内置；时间语义=chartTimeMs
后处理：接口与校验已有；绘制未实现
v1–v6 未声明曲线的父矩阵仍是 ADR 0008
未迁移的既有 Chart 继续直接求值
两轴 mappingAxes 与内置后处理绘制交给 Chart v8
formula kind 仍更后
```

双轴贝塞尔与内置后处理见 [v8 计划](../chart-format-update-for-v8/plan.md)。追加 `formula` 时不得
改写已冻结的 v7 贝塞尔 identity。Stage 11 不得把曲线空间轨迹当成判定线，除非未来 Judgement ADR
显式引用。
)
