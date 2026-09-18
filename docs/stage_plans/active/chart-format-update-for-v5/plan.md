# chart-format-update-for-v5：Chart v5 谱面格式与采样合同

Chart v5 只改变要求描述的字段合同和 Packed Chart 物理编码，不把某一种视觉轨道、
Note 表现或渲染后端提升为核心玩法语义。判定域、动作、有限行为和表现环境应保持
可分离，以便未来接入不同输入设备与 Presentation。

状态：active；跨阶段 Chart v5 总工作包；不是当前下一实施阶段。Foundation、Stage 6、
Stage 7A 和 Stage 8 分别按各自计划执行；正式发行门禁归 Stage 8

更新日期：2026-09-16

归档来源：[Stage Chart Format Update 完成计划](../../completed/chart-format-update/plan.md)、
[Chart v4 格式合同](../../../formats/CHART_V4_FORMAT.md)、[谱面格式审计记录](../../../stage_reports/reviews/stage-verification-2026-09/2026-09-01-findings.md)
与前置 [Stage 6 计划](../../active/stage-06/plan.md)。

## 计划定位

本文是 Chart v5 的跨阶段总工作包和详细设计汇总，用于维护 v5 的目标、格式方向、
容量门禁、迁移要求和跨阶段交接。它不直接决定当前应启动的阶段。

当前实施顺序是：

```text
已完成前置
  -> ../../completed/chart-format-foundation/plan.md
当前实施计划
  -> ../chart-format-foundation-hardening/plan.md
后续阶段（加固关闭后）
  -> ../../active/stage-06/plan.md
  -> ../../future/stage-07/plan.md（Stage 7A）
  -> ../../future/stage-08/plan.md
```

其中，`chart-format-foundation` 负责先完成 v5 Core/Packed 的基础、容量验证和 CXC
entry 设计；当前由 `chart-format-foundation-hardening` 补齐交接技术门禁后才进入 Stage 6。
本文继续保留 active；其中的 v5 正式发行内容要到 Stage 8 才按阶段计划实施。

字段与物理布局的候选权威分别是
[CXT v2](../../../formats/CXT_V2_FORMAT.md) 和
[Packed Chart](../../../formats/PACKED_CHART_FORMAT.md)。本文维护阶段交接与门禁，
不重复定义两份 Spec 的完整 wire 字段。

本阶段负责 Chart v5 的格式合同、对象层整数 alpha、扩展后的 CXT v2、在沿用 v4 旋转与相机姿态语义
前提下的作者可表达性、高密度谱面表达、迁移、默认 Writer 切换，以及旧格式的 Playback 兼容与弃用
窗口。Foundation 先交付 Core/Packed 物理模型，Stage 6 负责 v5-first candidate path，Stage 7A
冻结最小判定接口，Stage 8 完成正式发行和语义收敛。对象 alpha 是本阶段
确认的新可动画字段：作者层为整数 `[0,255]`，JSON 属性为 `render.alpha`，在 Reader 与显式迁移
边界 `/ 255` 一次后进入既有 `PropertyId::MaterialOpacity [0,1]` 采样路径。CXT v2 包含
Animation Extension 与 Chart Template/Pattern Core；Chart v5 只 import CXT v2。本阶段不新开 `[0,255]` Mixer，也不新增 World/Host
属性。旋转采样沿用 v4，不改写。Studio 编辑器实现与旧 Reader 删除不是本阶段关闭条件。本计划记录
已确认方向、实施顺序和验收门禁。V5-B ADR 只记录这些方向并附正例/失败例，不得改回。未过第 2.3 节
V5-0 门禁，不得把代码或文档写成正式发行支持；Foundation accepted 且通过 Stage 6 candidate
门禁后，可以实现和加载受限的 v5 Core/Packed candidate，但不能越过 Stage 8 的正式发行门禁。

## 1. 阶段目标

建立一个由 Stage 6 开始可验证消费、并在 Stage 8 正式发行的 Chart v5 谱面格式。Foundation
accepted 后，本计划可以为 v5 Core/Packed candidate path 实施必要的 Schema、Reader、Writer
和 validator；Stage 7A 负责冻结最小判定接口；Stage 8 关闭并经 owner acceptance 后，v5
成为默认发行格式，新增能力不再写入 v4 或更旧格式。在 Stage 8 之前，Chart v4 仍是默认
发行格式，v5 candidate 只能通过显式 candidate path 使用。v5 应当：

- 旋转与默认相机姿态沿用 Chart v4 语义，本阶段不修改。默认相机仍是具名 `pitch` / `yaw` / `roll` 与
  Tait-Bryan ZYX（Roll → Pitch → Yaw）；实体 `transform.rotation` 仍是 normalized Quaternion，Override
  为 shortest-path slerp，Additive 为既有 quaternion-log。不引入 Euler XYZ、匿名 vec3 旋转曲线，也不把
  实体旋转改写成 `pitch` / `yaw` / `roll` 动画。
- 将对象透明度定义为作者层的整数 alpha `[0,255]`：`cuexis.renderable` 可选字段 `alpha` 为静态初值，
  Clip v2 / `behavior.event` v2 / CXT v2 的可写属性为 `render.alpha`；省略静态字段时默认 `255`。
  `/ 255` 只发生在文档边界（v5 Reader、Chart 显式迁移、独立的 CXT v1→v2 工具）。
  AnimationSampler、Mixer、Behavior、HostOverride、PropertyResolver、Appearance 与
  `FrameSnapshot.materialOpacity` 继续使用既有 `[0,1]`。不新增 `world::PropertyId` 或
  `HostPropertyId`，`propertyCount` 保持 10。与材质/纹理 alpha 的合成规则不变。
- 交付包含 Animation Extension 与 Chart Template/Pattern Core 的 **CXT v2**。Chart v5 只 import
  CXT v2；Chart v4 只 import CXT v1。
  Chart v5 遇到 CXT v1 稳定拒绝，不在 prepare 做 v1 lowering。
- 将 v5 设为默认写出格式。Playback 继续直接求值未迁移的 v1–v4（结果与历史一致）；v4 仍走
  `material.opacity [0,1]`，不经 v5 alpha 路径。`--target 4` 在窗口内显式保留，且**不得**从 v5
  下调；`--target 3` 去掉。删除旧 Reader、Schema 和长期兼容路径不构成本阶段关闭条件，另按已接受
  的退出政策执行。
- v4 输入的采样结果、canonical identity 与 FrameDigest v3 不因 v5 采样器改写而变化。

## 2. 当前基线、前置条件与 V5-0 门禁

### 2.1 当前基线

开工时仓库事实如下。关闭后的后续开发基线是 Chart v5；二者不得混用。

```text
生产 Playback           Chart v4
默认 CLI Writer         仍为 v3（`--target 4` 才写出 v4）
SDK API                 0.7.0
CXT / CXC               v1 / v1
AnimationClip           v1（白名单随 Chart v4 / CXT v1）
behavior.event          version 1；连续白名单含 material.opacity [0,1]
FrameDigest             v1–v3；合法 v4 输入结果是兼容合同
对象透明度              material.opacity [0,1]；cuexis.renderable 无静态 opacity 字段
Runtime 透明度          PropertyId::MaterialOpacity [0,1] double
HostOverride            HostPropertyId::MaterialOpacity [0,1]；与 PropertyId 序号对齐
propertyCount           10
Hermite 斜率            归一化进度斜率；非负且 startSlope + endSlope <= 3
JSON 数值               Value 区分整数与浮点；readUInt64 拒绝 255.0
相机姿态                pitch / yaw / roll，Tait-Bryan ZYX
camera.fovY             (0,179)
实体旋转                Quaternion；Override shortest-path slerp；Additive quaternion-log
Additive opacity        已拒绝
```

### 2.2 前置与交接约束

- Foundation 必须先关闭 V5-0A 的物理模型、容量和 CXC entry 设计门禁。之后，Stage 6 可以启动
  v5 Core/Packed candidate path；Stage 6 同时保留 Chart v4、CXT v1 和 CXC v1 的兼容回退。
- Stage 7A 必须冻结 Chart v5 所需的 JudgementRequirement、InputEvent、JudgementResult 和
  Replay identity。Stage 7B+ 的高级判定能力不是本阶段或 Stage 8 的整体前置。
- Stage 8 关闭并经 owner acceptance 后，后续新增能力以 v5 typed/portable/Packed 谱面作为基线。
  兼容窗口内 Playback 仍直接求值未迁移的 v1–v4；默认写出与正式发行入口只在 Stage 8 关闭后切换到 v5。
- Stage 6 的 candidate path 可以在本计划关闭前存在，但必须明确标记 candidate，不得把候选读取
  路径写成默认发行支持。
- 对仓库内 v1/v2/v3/v4 fixture、示例、CXT、CXC 和外部 consumer 进行资产盘点。盘点与迁移报告是删除旧
  Reader 的前置，不是本阶段开工或关闭的前置。
- 在改变默认 Writer 前，完成 v5 Schema、typed Reader/Writer、canonical identity、capability preflight
  和独立的 v4->v5 migration report。
- 对象 alpha 的作者边界已确认为整数 `[0,255]`。其余数值范围沿用 v4；实现期用 golden 验证，不得把
  测试当成重新选择值域的入口。

### 2.3 V5-0：代码前门禁

任何正式发行 Schema、文件扩展名、Reader/Writer，以及 alpha 等新字段的正式采样路径，必须在本门禁
关闭后才能落地。Foundation accepted 后允许实现受限的 candidate Core/Packed path，但只能使用
candidate 标识和独立门禁。V5-A 的盘点和表征用例可以在门禁前进行。每一项剩余缺口都需要可审计
的正例和失败例。

**ADR 只记录、不得重开**（权威见第 3 节和第 4.1 节）：

```text
旋转 / 相机姿态 / Mixing / fovY (0,179) 沿用 v4
对象 alpha 作者层整数 [0,255]
  静态：cuexis.renderable 可选字段 alpha（component version 仍为 1）；省略默认 255
  动画/Behavior/CXT/mask：render.alpha
  canonical Writer 始终写出静态 alpha 整数（含 255）；identity 用 canonical bytes
/255 只在文档边界发生一次（v5 Reader、Chart 显式迁移、CXT v1→v2 工具）
Runtime 仍走 PropertyId::MaterialOpacity [0,1]；不新增 PropertyId / HostPropertyId
propertyCount 保持 10；HostOverride MaterialOpacity 仍为 [0,1]
v5 诊断与 propertyMask 使用 render.alpha；v4 路径继续 material.opacity
作者整数不得进入 Mixer；对现行 Hermite + Override lerp，先 /255 再混与在 [0,255] 上混再除一次
数学等价，不作为可观察不变量
Hermite 斜率仍是归一化进度斜率（非负，总和 <= 3）；Chart/CXT 迁移斜率照抄，禁止 * 255
v4 opacity 端点按 3.4 有损舍入：半数远离零 round(opacity * 255)；0.5 → 128 是规范例子不是等价例子
迁后因 opacity 导致的 FrameDigest 可以变；未迁移的合法 v4 digest 不变
JSON Schema integer，拒绝 255.0；canonical Writer 只写无小数整数
CXT v2 包含 Animation Extension 与 Chart Template/Pattern Core；Chart v5 只 import CXT v2；
Chart v4 只 import CXT v1
Chart v5 拒绝 CXT v1，不在 prepare 做 v1 lowering；CXC v1 不升版
AnimationClip：Chart v5 / CXT v2 为 clip.version 2；Chart v4 / CXT v1 仍为 clip v1
behavior.event：Chart v5 只接受 version 2（即使不含 alpha）；Chart v4 / v3 仍为 version 1
FrameDigest v1–v3 对合法 v4 输入不变
SDK API 0.8.0
capability：cuexis.chart.v5；CXT v2 import 时 cuexis.source.cxt.v2
  非空 v5 动画：cuexis.animation.clip.v2 + cuexis.animation.layers.v1
  非空 v5 Behavior：cuexis.behavior.event.v2
  默认 allCapabilities() 含上述新 ID；裁剪 Session 仍可拒绝
Playback 继续直接求值 v1–v4；不删除旧 Reader；不实现 Studio
Writer：先 --target 5，再切默认；允许从默认 v3 直接切到 v5（拒绝核验提案 1「先切默认 v4」）
窗口内保留 --target 4，仅用于写出 v4（v1/v2/v3→v4 或已是 v4）；v5 输入 --target 4 稳定失败
去掉 --target 3
--target 5 一条命令内部链式提升 v1/v2/v3/v4，不要求中间文件；打开文件不隐式迁移
迁 Chart 不改写 CXT bytes；有 CXT v1 import 时 Chart 迁移稳定失败，须先跑独立 CXT v1→v2 工具
```

**ADR 与生产 Spec 按上表记录，并须附正例/失败例：**

| 项 | 已选定 |
| --- | --- |
| Chart 顶层 | `format: "cuexis.chart"`，`version: 5` |
| 静态 alpha | `cuexis.renderable.alpha`，JSON 整数 `[0,255]`；省略默认 `255`；canonical Writer 始终写出；`renderable.version` 仍为 `1` |
| 动画 Property ID | `render.alpha`；v5 文档不得出现 `material.opacity`；映射到既有 `PropertyId::MaterialOpacity` |
| JSON 整数 | Schema `integer`，拒绝 `255.0`；Reader 走既有整数通道（如 `readUInt64`）；canonical Writer 只写无小数点整数 |
| 舍入 | 最近整数，半数远离零（`0.5 * 255 = 127.5 → 128`）；只作用于静态值与端点，不作用于斜率 |
| CXT v2 顶层 | `format: "cuexis.animation-template"`，`version: 2`；Clip 白名单与 Chart v5 相同 |
| AnimationClip 版本 | Chart v5 / CXT v2 的 `clip.version` 为 `2`；Chart v4 / CXT v1 仍为 `1` |
| Behavior Event 版本 | Chart v5 只接受 `behavior.event` version `2`；v3/v4 仍为 `1` |
| canonical identity 输入 | source/build 与 expanded semantic / artifact 分层；Header hash 依 Packed 规范，`alpha` 等具体值进入语义；v4 既有身份规则不改 |
| 诊断 code | 既有 v4 code 语义不变。v5 至少：`chart.v5.alpha_invalid`、`chart.behavior.version.unsupported`、`cxt.version.unsupported`（Chart v5 import CXT v1 时标在 import `source`）；capability 仍用 `playback.capability.unsupported` |
| `allCapabilities()` | 在 0.7 集合上增加 `cuexis.chart.v5`、`cuexis.source.cxt.v2`、`cuexis.animation.clip.v2`、`cuexis.behavior.event.v2`；裁剪 Session 仍可拒绝 |
| 公共观察面 | 见第 3.7 节 |
| 安全预算 | 沿用字段依 v4/CXT v1 原表；CXT Core 展开与 Packed decoded/峰值预算由 Foundation 独立冻结，不隐式放宽旧路径 |
| 默认 Writer | 先显式 `--target 5`，再切默认；从当前默认 v3 直接切到 v5。窗口内 `--target 4` 不得接受 v5 输入。去掉 `--target 3` |
| 弃用窗口 | SDK `0.8.0` 起默认写出 v5。Playback 继续直接求值未迁移的 v1–v4。窗口用 SDK 版本表达，本阶段不写日历删除日 |
| 迁移 | 旋转/相机/`fovY` 恒等复制。opacity/`render.alpha` 有损舍入。`--target 5` 一条链。Chart 迁移遇 CXT v1 import 则失败。迁 Chart 不改写 CXT bytes |

未关闭 V5-0 时，文档和代码只能称 Chart v5 / CXT v2 为候选，不能声称已获得正式发行支持。
Foundation 和 Stage 6 可以在明确的 candidate 范围内提供可加载路径，但必须稳定拒绝未支持的
RequirementKind、Packed 变体和超预算输入。V5-B 负责把门禁表逐项打勾并产出 ADR、
`CHART_V5_FORMAT.md` 和 CXT v2 Spec。

### 2.4 V5-0A：物理存储模型门禁

当前 JSON 作者格式将继续作为 v5 的可读、可迁移和 Studio authoring 格式；本阶段同时必须冻结
一个与 v5 语义等价的 Packed Chart 物理编码方向，以解决高密度谱面在 16 MiB 输入预算内的可部署性。
Packed Chart 不是 Chart v6，也不新增 Chart/CXT 语义版本。

V5-0A 必须在生产 Packed Reader/Writer、CXC 打包支持或 Playback Packed 输入前关闭，并至少冻结：

```text
作者层 JSON 与 Packed Chart 的语义等价关系
Packed Chart 的 magic、version、section table 和 checksum
STR0、REF0、IDN0、ARCH、ENT0 与 typed component stream 的局部索引规则
Object/component mask、实体差异字段和 columnar component 布局
Beat grid、delta/varint 编码与 RationalBeat 的无损还原合同
16 MiB packed-file、decoded-size、section-size 和展开计数预算
JSON/Packed identity、迁移链与 deterministic round-trip
CXC 中 source Chart 与 Packed Chart 的 entry kind/encoding 标识
```

Packed Chart 的目标是改变物理编码，不改变 v5 的 alpha、旋转、动画白名单、判定语义或 Runtime
PropertyId。IDN0 必须保留实体的稳定语义 identity；DBG0 才能保存可删除的 source/build
provenance。Packed 内部关系默认使用局部索引，不得在每条记录中重复 UUID、AssetId 或
Component 字段名。压缩算法只能作为物理编码的附加层，不能替代 section 预算和 decoded-size
安全检查。

天空盒不进入普通 Object 数组；其候选方向是独立的 Presentation Environment 合同。复杂形变不在
v5 预留自由表达式、脚本或未版本化顶点回调，后续按 Geometry Deformation / Model Animation
专门格式阶段处理。

### 2.5 V5-0B：CXT v2 模板与有限生成门禁

CXT v2 不再只是 CXT v1 动画模板的 alpha 版本。它必须同时冻结：

```text
Animation Extension
  AnimationClip / Track / Segment / Step / Animator

Chart Template Core
  Prototype slots / Instance bindings / Parameter freeze
  ValueSource / Pattern / finite Repeat
```

CXT v2 的有限生成必须在 prepare/compile 阶段完成。展开顺序为：

```text
typed read
  -> module validation
  -> parameter freeze
  -> finite Pattern/Repeat expansion
  -> Prototype slot binding / Instance expansion
  -> concrete semantic requirements
  -> conflict/count/size validation
  -> canonical semantic identity
  -> Packed lowering
```

Playback 不读取 CXT AST，不执行未展开 Pattern，也不允许 CXT 访问 Judgement 内部状态、
上一帧状态、输入流、随机数或宿主 API。递归、任意表达式、任意循环、脚本、字节码和
逐帧对象生成均拒绝。

Core 使用 Slot/Binding/ValueSource 的白名单：Beat、lane、position/scale 和受限
Repeat count；不提供 reference 参数。固定 AST/Component 结构不能被参数改写，
Repeat 的实体数量变化另做 checked 计数；rotation、alpha 与 CXT Animation Track
值仍为 literal。旧 ChartParameter 白名单不因此扩大。
CXT 在编译时冻结参数，改变参数必须重新编译 Packed；v5 发行 Playback 不隐式
调用 source entry 重跑模板，v4 参数 prepare 兼容合同不变。

V5-0B 必须关闭后才能实现 CXT v2 formal-release Schema/Reader/Writer，并至少产生
[CXT v2 Spec](../../../formats/CXT_V2_FORMAT.md)、正反例、展开 golden 和安全预算报告。

### 2.6 V5-0C：40,000 实体 / 16 MiB 容量门禁

V5-0C 将容量目标从“表征用例”提升为 Stage 8 关闭门禁：

```text
semantic entity count       40,000
Packed Chart entry          <= 16 * 1024 * 1024 bytes
expanded decoded bytes      有独立上限
peak prepare memory         有独立上限
expanded event/write count  有独立上限
```

40,000 按展开后的语义实体计数，不能以 Packed record、CXT module 或 source entry
数量规避。它必须按声明的容量 profile 验收，而不是对任意复杂谱面作无条件保证。
必须分别记录 Authoring Budget、Packed Entry Budget、Chart Closure Budget
和 Expanded Runtime Budget。测试至少覆盖高重复 Pattern、低重复率输入、复杂动画、大量
资源、超展开数量、超 decoded bytes、超 Packed bytes、递归和 checked arithmetic 溢出。

V5-0C 必须在默认 Writer、正式 Packed Reader/Writer、CXC playback entry 或正式 Playback v5
能力声明之前关闭。未关闭前，40,000/16 MiB 只能称为目标；Foundation/Stage 6 的 candidate
路径可以用于验证，但不能称为正式支持。

## 3. 已确认的 v5 方向

### 3.1 旋转和采样

v5 旋转与默认相机姿态直接使用 Chart v4 合同，本阶段不修改字段形状、轴、合成顺序、插值或 Additive。
权威仍是 [CHART_V4_FORMAT.md](../../../formats/CHART_V4_FORMAT.md)、
[CHART_FORMAT.md](../../../formats/CHART_FORMAT.md) 的相机条款和
[ANIMATION_MIXING.md](../../../formats/ANIMATION_MIXING.md)：

```text
默认相机     pitch / yaw / roll 度数；Tait-Bryan ZYX（Roll → Pitch → Yaw）
实体旋转     transform.rotation 为 normalized Quaternion
Override     shortest-path slerp
Additive     quaternion-log delta
内部表示     core::Quat / Transform / World / Renderer 仍为 Quaternion
```

因此 v5 不把未展开多圈角度、Euler XYZ 或匿名 vec3 当作新的作者层旋转曲线。单段 Quaternion 仍然只保证
最短弧，不能作为 `0 -> 720` 的未展开轨迹；这是沿用 v4，不是回归缺陷。v4 → v5 对旋转和静态相机字段
做语义恒等复制，不把 Quaternion 动画改写成欧拉角。

v5 新字段（对象 alpha）的采样仍按绝对 local Beat 计算，且不得依赖上一帧累加状态。旋转采样继续走既有
v4 路径；不得为“表达多圈”在 Reader/prepare 中把 Quaternion 烘焙成多段事件。

### 3.2 数值合同

- JSON Reader 可以使用 `double` 读取和验证**非 alpha** 数值，但 v5 不因作者字段而把整个 Runtime
  数值系统升级为 double。对象 alpha 的 JSON 走既有整数通道，不得先读成 `double` 再检查是否为整数。
- 本阶段新冻结的作者范围只有对象 alpha 整数 `[0,255]`。旋转、相机姿态、`camera.fovY`、position、scale
  和 tint 沿用 v4。不为未展开多圈角度新设 `[-10000,10000]` 一类预算。
- 对象 alpha 的 JSON 必须是 Schema `integer`：接受 `0`…`255`，拒绝 `255.0`、`-0.0` 和其它带小数的写法。
  canonical Writer 只写无小数点整数。
- 作者层范围与 Runtime 范围不同：JSON/Schema 校验 `[0,255]` 整数；Reader/迁移之后的采样、混合、
  HostOverride、PropertyResolver 和 Snapshot 校验既有 `[0,1]`。各自越界稳定失败，不静默 clamp。
- 值域分类以第 4 节为准，不在实现期把“审计”重新变成新的作者能力。

### 3.3 相机

默认相机的 `pitch` / `yaw` / `roll`、合成顺序、坐标空间和 `camera.fovY` 与 v4 相同。`fovY` 保持严格
开区间 `(0,179)`；`0`、`179` 和越界拒绝。v4 → v5 迁移复制这些字段，不做顺序重映射，也不把姿态改写成
Euler XYZ 或实体 Quaternion。接近 180 的后端稳定性不在本阶段用放宽作者值域解决。

### 3.4 透明度和 alpha

v5 对象层作者单位已确认为整数 alpha `[0,255]`：`0` 完全透明，`255` 完全不透明。这是 Chart / CXT
作者层与 Schema 的单位，不是 Runtime、HostOverride、Presentation 或材质/纹理的单位。

对象 alpha 同时是：

- `cuexis.renderable` 上的可选静态字段 `alpha`；`renderable.version` 仍为 1；省略时默认 `255`；
  canonical Writer 始终写出该整数
- AnimationClip v2、`behavior.event` v2 与 CXT v2 的可写属性 `render.alpha`，与静态字段同一
  `[0,255]` 作者单位；v5 `propertyMask` 使用 `render.alpha`

v5 文档只写这一字段，不得同时出现 `material.opacity`。Chart v5 不接受 `behavior.event` version 1，
也不 import CXT v1。无自定义 alpha、无透明度动画的空 v5 谱面，默认 `255` 经 `/ 255` 后与 v4 默认
`material.opacity = 1.0` 在 `FrameSnapshot.materialOpacity` 和 FrameDigest v3 上等价。

单位分层：

```text
作者层（Chart v5 / CXT v2 / Schema）
  静态值、关键帧端点   JSON 整数，闭区间 [0, 255]；拒绝 255.0、小数、负数和 256+
  Hermite 斜率         仍为归一化进度斜率；非负，总和 <= 3；不是 [0,255] 值域斜率

文档边界（只转换一次）
  runtime = authoring / 255.0
  0 → 0.0，255 → 1.0（1.0 在 binary32/64 中精确）
  1 → 1.0/255.0（golden 比较 Snapshot double 与 digest，不比较 GPU float）
  Source 文档持有整数；Resolved / AnimationProgram 持有 [0,1] double

Runtime（沿用 v4，不新开路径）
  PropertyId::MaterialOpacity [0,1]
  AnimationSampler / Mixer / Behavior / HostOverride / PropertyResolver
  AppearanceComponent::opacity
  FrameSnapshot.materialOpacity
  Additive                   不支持（沿用 v4 opacity）
```

作者整数不得进入 Mixer。对现行 Hermite 进度插值与 Override 标量 lerp，在 `[0,255]` 上混合再 `/ 255`
与先 `/ 255` 再混合数学等价，因此「禁止先归一化再混合」**不是**可观察不变量，也不作为本阶段采样器
合同。`/ 255` 发生在 v5 Reader、Chart 显式迁移和独立 CXT v1→v2 工具，不发生在 Presentation 提交，
也不发生在 Chart v5 的 prepare lowering（v5 不读 CXT v1）。

v4 `material.opacity` `[0,1]` 只作为 Chart 迁移与 CXT v1→v2 工具的作者输入。端点与静态值换算为：

```text
alpha = round_half_away_from_zero(opacity * 255)
```

`0.0 → 0`，`1.0 → 255`，`0.5 → 128`（`127.5` 半数远离零）。斜率**原样复制**，禁止 `* 255`。属性名
改为 `render.alpha`（静态字段为 `alpha`）。不得与对象 alpha 再乘一次。

该换算是**允许的有损语义变化**，不套用「可证明等价否则失败」。`128/255 ≠ 0.5`；迁后 FrameDigest
可以因 opacity 与源 v4 不同。未迁移的 v4 继续走 `material.opacity [0,1]`，digest 不变。迁移报告
保留源值与舍入值。

CXT v1 只被 Chart v4 import，求值保持 `[0,1]`。要给 Chart v5 用，必须先经独立 CXT v1→v2 工具写成
CXT v2（同一端点公式、斜率照抄、白名单改为 `render.alpha`）。Chart 迁移不改写 CXT bytes；若 Chart
仍引用 CXT v1，迁移稳定失败。

`render.alpha` 在 Reader 之后映射到既有 `PropertyId::MaterialOpacity`。材质 base color alpha、
纹理 alpha 和 `Opaque/Blend` alphaMode 仍按 [PORTABLE_PRESENTATION.md](../../../formats/PORTABLE_PRESENTATION.md)
的 `[0,1]` 合成；本阶段只改对象层作者单位，不改合成公式。不在作者层使用 `[0,1]` 透明度，也不做
阶梯式按整数量化的淡入淡出：关键帧是整数，Hermite 中间结果在 Runtime `[0,1]` 上连续。

### 3.5 混合和高级表示

旋转混合沿用 [ANIMATION_MIXING.md](../../../formats/ANIMATION_MIXING.md) 的 v4 合同：Override 为
shortest-path slerp，Additive 为 quaternion-log。本阶段不修订旋转混合，不引入按轴角度 Additive，
也不新增 Quaternion exact 作者模式（v4 作者层已经是 Quaternion）。

对象 alpha 在文档边界转为 `[0,1]` 后，Override 按 v4 标量 lerp 混合；Additive 拒绝。不新增
Runtime 属性，不把 Mixer 改成 `[0,255]`。除 alpha 与既有 v4 属性外，其它新的非旋转属性若进入
混合，必须在 ADR 中单独定义；不得借旋转路径改写 slerp 或 quaternion-log。

### 3.6 CXT、CXC、Digest 与 SDK

CXT 的 Track/Segment 字段权威就是外层 Chart / CXT 文件版本所绑定的 AnimationClip 白名单。Chart v5
增删改可动画属性（字段、单位或范围）时，CXT 必须同步升版；不能只改 Chart、让模板停在 v4 白名单，
也不能让 `clip.version: 1` 或 `behavior.event` version 1 同时表示两套白名单。CXC 容器和 FrameDigest
算法仍不因此升主版本。

- **CXT**：本阶段交付 **CXT v2**，Clip 属性白名单与 Chart v5 AnimationClip v2 **相同**（含
  `render.alpha` 整数 `[0,255]`，旋转仍是 Quaternion）。CXT v1 Schema 冻结，不把新属性偷加进
  `version: 1`。Chart v5 **只** import CXT v2，遇到 CXT v1 以 `cxt.version.unsupported` 稳定拒绝，
  不在 prepare lowering。Chart v4 继续只接受 CXT v1，遇到 CXT v2 稳定拒绝。新模板默认写 CXT v2。
  独立 CXT v1→v2 文件工具按第 3.4 节转换端点并照抄斜率，不绑在 Chart 迁移命令上，也不由 Chart
  迁移隐式调用。
- **AnimationClip 版本**：Chart v5 本地 Clip 与 CXT v2 的 `clip.version` 为 **2**。Chart v4 与
  CXT v1 仍为 **1**，白名单仍是 v4（含 `material.opacity`）。Clip 白名单跟外层 Chart/CXT 版本走，
  不出现“clip v1 两套白名单”。
- **Behavior Event 版本**：Chart v5 只接受 `behavior.event` version **2**，白名单与 AnimationClip
  v2 的对象 alpha 一致。v3/v4 仍为 version **1**（`material.opacity [0,1]`）。v5 迁移把 v1 Behavior
  升到 v2：透明度端点按第 3.4 节舍入，斜率照抄。不让 `behavior.event` v1 同时表示两套白名单。
- **CXC v1**：容器、manifest、ZIP32 Stored 和闭包规则不变。CXC v1 可以包含 Chart v5 与其闭包内的
  CXT v2，也可以包含未迁移的 Chart v4 与 CXT v1；pack / validate / Playback 按内层文档
  `format`/`version` 路由。Chart v5 不得把 CXT v1 留在自己的 import 闭包里。本阶段不交付 CXC v2。
  v5 ADR 只补充“CXC v1 允许内层 Chart v5 / CXT v2”，不重开载体决策。
- **FrameDigest v1-v3**：合法 v4 输入的 digest 结果不得变化。v5 新作者值优先映射到现有
  `FrameSnapshot` 字段（对象透明度仍进入已有 opacity）；不因 Chart v5 单独升级 digest 版本。
  仅当公开 snapshot 布局必须新增被哈希字段时才允许新 digest 版本，且须在 ADR 中单列。
- **Capability**：v5 文档要求 `cuexis.chart.v5`。import CXT v2 要求 `cuexis.source.cxt.v2`。非空
  v5 AnimationClip / CXT import / Binding / Layer / Instance 要求 `cuexis.animation.clip.v2` 与
  `cuexis.animation.layers.v1`。非空 v5 `behavior.event` 要求 `cuexis.behavior.event.v2`。默认
  `allCapabilities()` 包含这些新 ID。只声明 `cuexis.chart.v4`、`cuexis.animation.clip.v1` 或缺少
  对应 CXT capability 的裁剪 Session 稳定拒绝，不得把 v5 白名单当成 clip v1。
- **SDK API**：当 Playback prepare 接受 Chart v5 / CXT v2、安装契约出现对应 capability，以及默认
  Writer 切到 v5 时，将 `CUEXIS_SDK_API_VERSION` 从 `0.7.0` 提升到 `0.8.0`。日期构建版本门禁仍归
  Stage 6，本阶段不纠正 `26.08.01-1` 滞后。删除旧 Reader 不在本阶段，故不为删除再升一次 SDK。

只懂 v4 的已安装宿主必须在 Reader 或 prepare 最早可判定点稳定拒绝 v5，带版本与 capability 诊断，
不得半加载或降级为 v4 语义。

### 3.7 公共观察面

实现前冻结宿主可见面；本阶段不增加 Playback 公共 C++ 类型，也不把 ChartRuntime / World / EnTT 暴露给
external consumer。

```text
PlaybackSession prepare / activate / reload / seek 事务边界不变
FrameSnapshot 布局不新增 camera 或 object 字段
不新增 HostPropertyId；HostPropertyId::MaterialOpacity 仍为 [0,1] double
不新增 world::PropertyId；propertyCount 保持 10
对象透明度经文档边界 /255 后进入既有 snapshot.materialOpacity [0,1]；255/255 为精确 1.0
camera.pitch / yaw / roll 与 fovY 继续按 v4 含义出现在 snapshot
FrameDigest v1–v3 继续哈希既有字段；合法 v4 输入结果不变
project-document table、CXC file/memory source、per-prepare ParameterSet 的所有权不变
缺 cuexis.chart.v5、cuexis.source.cxt.v2、cuexis.animation.clip.v2 或 cuexis.behavior.event.v2 时，
  在资源获取和 World 发布之前稳定失败（按文档实际需要）
find_package(Cuexis 0.7) 的 SameMinorVersion 不得把 v5 prepare 当成已支持契约
Playback 继续直接求值未迁移的 v1–v4；打开 v1–v3 不拒绝
```

Seek、循环、零持续、相邻边界和 timeDiscontinuity 沿用既有绝对 Beat 规则。对象 alpha 必须同样不依赖
上一帧。旋转采样器不改写。V5-A/C 必须先有 v4 旋转、v4 opacity 与 digest 表征，证明 v5 只在文档
边界转换整数 alpha，不改 Mixer 单位。

## 4. 数值与作者能力：本阶段冻结 / 沿用 v4 / 不实现

本阶段不把 v4 的每一条限制都重开成 v5 ADR。分类如下。

### 4.1 本阶段冻结

- 对象层整数 alpha `[0,255]`：静态字段 `cuexis.renderable.alpha`；动画/mask 为 `render.alpha`。
  省略默认 `255`；canonical Writer 始终写出静态字段。`/ 255` 只在文档边界发生一次；Runtime 映射到
  既有 `PropertyId::MaterialOpacity [0,1]`。不新增 `PropertyId` / `HostPropertyId`，`propertyCount`
  保持 10。v4 `material.opacity` 按第 3.4 节有损迁移（端点舍入，斜率照抄，属性改名为 `render.alpha`）。
- JSON Schema `integer`，拒绝 `255.0`；canonical Writer 只写无小数点整数。
- 舍入：最近整数、半数远离零；只作用于静态值与端点，不作用于斜率。
- Chart v5 / CXT v2 使用 AnimationClip v2：v4 可动画列表减去作为作者字段的 `material.opacity`，
  加上 `render.alpha`；旋转仍是 Quaternion。白名单两者相同。Chart v4 / CXT v1 仍使用
  AnimationClip v1。
- Chart v5 只接受 `behavior.event` version 2（即使事件不含 alpha）；v3/v4 仍为 version 1。
- Chart v5 只 import CXT v2；Chart v4 只 import CXT v1。
- 传统 ChartParameter 白名单沿用 v4（position、scale、`camera.fovY`、Layer/Group/Instance weight、Binding
  durationScale/weight）。对象 alpha 本阶段保持 literal，与 v4 未参数化 `material.opacity` 一致。
  Quaternion、rotation、Chart binding startBeat、priority、parent、AssetId、拓扑和 CXT Animation
  Clip 内部值仍禁止 ParameterRef。CXT Core 的局部 Beat/Slot/Repeat 依第 2.5 节独立合同，
  不与既有 ChartParameterRef 混淆。
- `camera.fovY` 保持 v4 `(0,179)`。`material.tint` 保持 v4 每分量 `[0,1]` normalized linear RGB。
- 未知核心字段失败；沿用字段的安全预算依 v4/CXT v1 原表，不因 alpha 提高上限；
  Core 新增展开量、IDN0、decoded 与峰值预算须在 Foundation 单独冻结。
- 作者层与 Runtime 各自使用第 3.2 节的范围，越界稳定失败。

### 4.2 沿用 v4，写入作者指南和失败示例

下列合同进入 v5 指南与测试，但不在本阶段重开：

- `transform.position` 与普通数值：有限值，最终受 float 可表示范围约束。
- Override `transform.scale` 为有限值；Additive scale 为严格正因子。不新开镜像或零缩放。
- Hermite：`startSlope` / `endSlope` 是归一化进度斜率，非负且总和不超过 3，进度 clamp 到
  `[0,1]`。值等于 `lerp(start, end, h(u))`。斜率不随 alpha 作者单位缩放。
- Rational Beat 的 numerator/denominator 预算；采样转为 double 的既有行为。
- Clip iterations、Layer/Group weight、离散属性 weight=1、PropertyMask 冲突、Additive 属性白名单。

### 4.3 本阶段不实现

- 未展开多圈角度、Euler XYZ、把实体旋转改成 `pitch` / `yaw` / `roll` 曲线。
- bounce、overshoot、往返、非单调曲线，或放宽 Hermite clamp。
- 把 `camera.fovY` 放宽到 `(0,180)`。
- 扩展参数化覆盖 rotation、拓扑、CXT Track 值或对象 alpha。
- 为 position/scale/Beat 新设与 v4 不同的作者值域。
- 打开 v1–v3 即拒绝；删除 `--target 4`；迁 Chart 时改写 CXT 源文件。
- Chart v5 import CXT v1，或在 prepare 对 CXT v1 做 lowering。
- 从 v5 输入 `--target 4` 下调写出。
- 让 `clip.version: 1` 或 `behavior.event` version 1 同时表示 v4 与 v5 两套白名单。
- 在 Mixer / HostOverride / PropertyResolver 中使用 `[0,255]` 作为 Runtime 单位。
- 新增 `world::PropertyId` 或 `HostPropertyId`，或把 `propertyCount` 改为 11。
- 迁移或 CXT v1→v2 时把 Hermite 斜率 `* 255`。
- 把 `material.opacity` 有损舍入套进「可证明等价否则失败」。
- 核验提案 1 的第一步：先把默认 Writer 切到 v4。本阶段从默认 v3 直接切到 v5。

## 5. 实施批次

### V5-A：基线和作者用例

- 盘点第 2.1 节基线：格式、Runtime、默认 Writer（v3）、SDK `0.7.0`、Studio 预期和迁移输入。
- 建立高复用、低复用、动画密集、身份密集和资源闭包密集的容量 profile，至少包含
  40,000 个展开语义实体；分别记录 JSON source bytes、Packed candidate bytes、IDN0
  bytes、decoded bytes、对象/事件数量和 prepare 峰值，作为 V5-0A 的容量基线。
- 建立 v4 旋转、相机姿态、v4 opacity 和 FrameDigest v3 表征用例，证明后续改动不改变未迁移 v4
  结果；另建立对象 alpha 用例（含默认 `255`、`0`/`255` 端点、半数远离零、`255.0` 拒绝、斜率不缩放、
  Chart v5 拒绝 CXT v1、`render.alpha` 映射到 `MaterialOpacity`）。
- 按第 4 节建立限制矩阵：冻结项、沿用 v4 项、本阶段不实现项。不把未展开多圈旋转或 Hermite 放宽列入
  必须交付的作者能力。

### V5-B0：物理存储模型

- 在正式发行 Schema/Reader/Writer 之前完成 Packed Chart 的 ADR/Spec 草案和 V5-0A 门禁；
  Foundation/Stage 6 可以在此基础上实现受限 candidate lowering。
- 冻结 authoring JSON、canonical semantic model 与 Packed Chart 的职责、等价性和 identity 关系。
- 设计 96-byte Header、32-byte Section Directory、STR0/REF0/IDN0/ARCH/ENT0、
  typed component streams、component mask、Beat GridDelta/Rational atoms、CRC 和
  checked decoded-size 预算。IDN0 保留具体实体 semantic identity；DBG0 只保存可删除
  source/build provenance。
- 明确 16 MiB 是 Packed file budget；JSON source、decoded chart、CXC package 和 runtime memory
  分别使用独立预算，不以压缩比替代安全限制。
- 规定 `v4 JSON -> v5 JSON -> Packed v5` 的迁移/编译链；Packed bytes 不反向作为作者层输入。
- 为 JSON 与 Packed 的语义 parity、canonical identity、失败回滚和跨平台 round-trip 建立测试计划。

### V5-B：关闭 V5-0（ADR 和格式合同）

- 产出 v5 ADR、`CHART_V5_FORMAT.md` 和 CXT v2 Spec，将第 2.3 节门禁表逐项打勾。
- 已锁定方向只许记录进 ADR，不得改回 Euler XYZ、未展开多圈、`fovY (0,180)`、参数化 alpha、CXT v1
  塞新属性、Chart v5 import CXT v1、打开 v1–v3 即拒绝、clip v1 或 `behavior.event` v1 一名两义、
  Runtime `[0,255]` 混合、新 PropertyId、斜率 `* 255`、v5→v4 下调，或先切默认 Writer 到 v4。
- 将第 2.3 节已选定表写入 ADR 与 Spec，每项附正例和失败例，包括 `render.alpha`、`cuexis.renderable.alpha`、
  Chart v5 拒绝 CXT v1、`--target 5` 单链、`--target 4` 拒绝 v5 输入。
- 每个缺口附正例和失败例。第 4.2 节只写入指南，不重开 ADR。
- Foundation/V5-0A 关闭前不得开始 v5 candidate lowering 接线；V5-0 关闭前不得开始 formal-release
  Schema、Reader/Writer 和正式 Playback entry。

### V5-C：typed model 和 Runtime 采样

- v5 candidate path 在 Foundation/V5-0A 关闭并通过 Stage 6 candidate 门禁后开始；formal-release
  path 在 V5-0 关闭后开始。
- 旋转 typed 表达与 AnimationSampler 路径沿用 v4 Quaternion / 相机姿态实现，不引入新的欧拉曲线类型。
- 对象 alpha 在 v5 Reader 转为 `[0,1]` 后，按绝对 local Beat 走既有 `MaterialOpacity` 采样与
  Override 混合，不生成中间事件，不依赖上一帧累加状态，不把 Mixer 改成 `[0,255]`。
- 保持 World、Transform 和 Renderer 的最终 Quaternion 边界，且不修订旋转 Override/Additive。
- 为对象 alpha（含 `1/255` Snapshot double 往返、`255/255 = 1.0`、默认 `255` 与 v4 默认 opacity
  等价）以及“与 v4 旋转结果一致”建立 deterministic golden。
- v4 输入的采样结果、canonical identity 和 FrameDigest v3 不得因 v5 文档边界转换而变化。未迁移的
  v4 继续走 `material.opacity [0,1]`，不经 v5 整数 alpha 路径。

### V5-D：Schema、Reader、Writer 和 prepare

- candidate Reader/Writer 和 prepare 路径在 Foundation accepted 后可以实现，但必须标记
  candidate 并受独立容量、identity 和拒绝门禁约束；formal-release 路径在 V5-0 关闭后开始。
- 实现 v5 Schema、严格 Reader、canonical Writer、identity/digest 参与规则和 capability preflight。
- 实现 CXT v2 Schema/Reader/Writer；Chart v5 只 import CXT v2，拒绝 CXT v1；Chart v4 拒绝 CXT v2。
- 实现 `behavior.event` v2 Schema/Reader；Chart v5 拒绝 `behavior.event` v1。
- CXC v1 pack/validate 按内层文档版本路由 Chart v5 与 CXT v2；发行包的 Chart v5 入口必须是
  Packed Chart，作者层 JSON 只作为 Source Project 输入或显式 source entry 保留。
- 若 V5-0A 已关闭，增加 Packed Chart 编译/验证路径；Packed 输入必须先完成 header、section、
  count、decoded-size、identity 和 capability 校验，再进入既有 typed prepare。
- Foundation 只保证其已验收静态候选 profile；Stage 6 需要的 BEH0/BHD0/ANM0 或生成
  实体动画目标绑定，必须先补候选 revision、字段枚举和 golden。未支持 section/引用
  不得通过 opaque JSON、CXT AST 或隐式命名约定继续播放。
- 统一作者层 `[0,255]` 整数与 Runtime `[0,1]` 的范围及溢出诊断；转换只发生在文档边界。
- 确保对象 alpha 经 `/ 255` 进入 Portable Presentation `[0,1]` 后不改变 alphaMode、资源 identity 和
  FrameDigest v3 对既有 snapshot opacity 字段的参与规则。
- 缺少 `cuexis.chart.v5` 或未知 Chart 版本时稳定拒绝，不降级。Playback 对未迁移的 v1–v4 继续求值。

### V5-E：迁移、默认 Writer 和弃用窗口

- 实现 v4 -> v5 显式迁移；旋转与静态相机 `pitch` / `yaw` / `roll` 按 v4 语义恒等复制，不把
  Quaternion 动画改写成欧拉角。`material.opacity` 改为 `render.alpha` 并按第 3.4 节有损换算端点；
  斜率照抄，禁止 `* 255`。静态 opacity 初值写入 `cuexis.renderable.alpha`。`behavior.event` v1 升为
  v2。迁后 digest 允许因 opacity 与源 v4 不同。
- 迁 Chart **不改写** CXT 文件。若源 Chart 仍 import CXT v1，迁移稳定失败并列出路径；须先运行独立
  CXT v1→v2 工具（端点舍入、斜率照抄、`clip.version` 2、属性改为 `render.alpha`），再迁 Chart。
  该工具不是 Chart 迁移的隐式步骤。
- `--target 5` 是一条命令：内部复用既有 v1/v2→v3→v4 提升再走 v4→v5，不要求用户写出中间文件；
  不得在打开文件时隐式迁移。
- 先将 v5 Writer 作为显式 `--target 5`，再切换为默认 Writer；从当前默认 v3 直接切到 v5（拒绝核验
  提案 1 的「先切默认 v4」）。窗口内保留 `--target 4`，仅接受写出 v4 的输入；v5 输入加 `--target 4`
  稳定失败。去掉 `--target 3`（v1–v3 不再写出）。
- SDK `0.8.0` 起默认写出 v5。Playback **继续直接求值** 未迁移的 v1–v4：v4 走 `material.opacity
  [0,1]`；v1–v3 结果与历史一致，文档标明不再承接新字段并推荐迁 v5，打开不拒绝。
- 盘点外部资产、提交迁移报告，并在 SDK `0.8.0` 合同中记录弃用窗口；删除时间点另立退出，不在本阶段关闭。

### V5-F：作者指南和工具链

- **新建** Chart v5 authoring guide，说明 Object/Template/Parent、相机、动画、`render.alpha`、
  CXT v2 与 Chart v5 同一白名单、资源闭包、值域和诊断。不修改现有
  [CHART_V4_AUTHORING.md](../../../guides/CHART_V4_AUTHORING.md)（该文件只服务手写 v4）。
- Validator、Migrator 和 Player 使用同一 v5 语义和示例。Player / Playback 仍直接加载未迁移的 v4。
- 不实现 Studio 编辑器或 Studio Preview；Studio 约束见第 8 节。

### V5-G：关闭和交接

- 完成 focused、architecture、package、external consumer、跨平台和 deterministic 验证。
- 在同一候选 SHA 上完成 hosted 验证。精度/确定性证据针对：`255/255 = 1.0`、`1/255` 的 Snapshot
  double 往返、半数远离零端点、斜率不缩放、默认 `255` 与 v4 默认 opacity 等价、合法 v4 输入的
  FrameDigest v3 不变。不再收集已撤销的大角度 `[-10000,10000]` 预算证据。
- 完成 v5 completion report，更新格式索引、状态页、路线图、API 兼容说明和旧路径映射。
- 验证 `CUEXIS_SDK_API_VERSION` 为 `0.8.0`，且 external consumer 以 `find_package(Cuexis 0.8 ...)`
  消费 v5；`0.7` SameMinorVersion 不得把 v5 prepare 当成已支持契约。
- 旧格式删除不作为本批次或本阶段关闭门禁；只记录弃用窗口与后续退出所需的盘点/报告入口。

## 6. 验收标准

- V5-0 在正式发行 Schema/Reader 之前关闭；ADR 与 Spec 覆盖第 2.3 节全部缺口，且未重开第 3/4.1 节已锁定方向。
- v5 对相同旋转与相机姿态输入的求值与 v4 一致；Seek、Reload、逐帧播放和不同采样帧率下，v4 旋转
  golden 仍然成立。
- v4 → v5 迁移后，静态相机 `pitch` / `yaw` / `roll` 与实体 `transform.rotation` Quaternion 保持语义恒等。
- v5 `camera.fovY` 与 v4 相同，接受 `(0,179)`，拒绝 `0`、`179` 和越界值。
- v5 对象 alpha 静态字段为 `cuexis.renderable.alpha`，动画/Behavior/CXT/mask 为 `render.alpha`，
  省略默认 `255`；静态值和关键帧端点为 JSON 整数 `[0,255]`（拒绝 `255.0`）。`/ 255` 只在文档
  边界发生一次，之后走既有 `MaterialOpacity [0,1]` 采样与 Override；不新增 Runtime/Host 属性。
  与材质/纹理 alpha 按既有公式合成，不与 `material.opacity` 乘两次。`0.5 → 128` 是有损迁移规范，
  迁后 digest 可以变。斜率照抄。空 v5 默认 `255` 与 v4 默认 opacity `1.0` 在 snapshot/digest 上等价。
- 不为表达多圈而把 Quaternion 烘焙成多段事件；旋转相关 identity 不因烘焙策略变化。
- Playback 继续直接求值未迁移的 v1–v4；v4 不经 v5 alpha 路径。默认 Writer 为 v5；`--target 4` 保留
  且拒绝 v5 输入；`--target 3` 去掉。`--target 5` 一条链。迁 Chart 不改写 CXT。删除旧 Reader 不是
  本阶段验收项。
- 新建的 v5 作者指南、Schema、Reader、Writer、Validator 和 Player 对旋转（沿用 v4）、相机姿态（沿用
  v4）和 alpha 的语义一致。既有 v4 手写指南不被改写。
- 40,000 语义实体容量基线在 JSON source、Packed file、decoded chart 和 prepare 峰值四个维度均有报告；
  16 MiB 只对 Packed file 作为发行输入门禁，并另有 Chart Closure/Expanded Runtime 预算。
- CXT v2 包含 Chart Template/Pattern Core 与 Animation Extension；Chart v5 只 import CXT v2，拒绝 CXT v1。
  Chart v5 只接受 `behavior.event` v2。Chart v4 拒绝 CXT v2。CXC v1 可包含 Chart v5 与 CXT v2。
  FrameDigest v1-v3 对合法 v4 输入不变。Playback 安装契约为 SDK API `0.8.0`，默认 capability 含
  `cuexis.chart.v5`、`cuexis.source.cxt.v2`、`cuexis.animation.clip.v2` 与
  `cuexis.behavior.event.v2`。
- `FrameSnapshot` 布局未增字段；不新增 `HostPropertyId`；对象 alpha 进入既有
  `materialOpacity [0,1]`；`0.7` consumer 不能把 v5 prepare 当成已支持契约。沿用字段预算与
  v4/CXT v1 原表相同，Core/Packed 新增预算独立验收。`propertyCount` 仍为 10。

## 7. 明确不包含

- Chart v6 / Model v1（静态 glTF、内置网格、默认扁平片、submesh 槽）。该项见
  [延期计划](../../deferred/chart-format-update-for-v6/plan.md)，不在本阶段实施。
- Chart v7 曲线形变、line 模型与 `shader.json` 后处理接口。该项见
  [延期计划](../../deferred/chart-format-update-for-v7/plan.md)，不在本阶段实施。
- Chart v8 双轴贝塞尔、模型基本动画与内置后处理。该项见
  [延期计划](../../deferred/chart-format-update-for-v8/plan.md)，不在本阶段实施。
- Stage 6 的版本门禁、Player 产品化、后端中立渲染和常用媒体支持。本阶段不包含这些工作；Stage 6
  先以 v5-first candidate path 完成并保留 v4 回退，随后本阶段消费 Stage 6 的稳定 Playback/Player 合同。
- Studio 编辑器、Viewport、Timeline 和 Preview 实现。本阶段只把 v5 typed authoring model 与“必须经
  PlaybackSession 预览”写入交接，不在本阶段实现 Studio。
- Vulkan adapter、Judgement/Replay、稳定 C ABI 和运行时脚本/逐帧回调。
- 通过隐式表达式、脚本、随机数或运行时回调扩展 v5 的曲线能力。
- 修改 Chart v4 的旋转表示、默认相机姿态合成、shortest-path slerp 或 quaternion-log Additive；不为
  未展开多圈角度引入新的作者层旋转曲线。
- CXC v2、FrameDigest v4（除非 snapshot 布局经 ADR 必须扩展）以及 Stage 6 的日期构建版本门禁。
- 把 v5 新属性写进 CXT v1，让 Chart v5 import CXT v1，或让 `clip.version: 1` /
  `behavior.event` version 1 同时表示 v4 与 v5 两套属性白名单。
- 在 Mixer、HostOverride 或 PropertyResolver 中以 `[0,255]` 为 Runtime 单位；新增
  `world::PropertyId` 或 `HostPropertyId`；迁移或 CXT v1→v2 时把 Hermite 斜率 `* 255`。
- 打开未迁移的 v1–v3 即拒绝；去掉 `--target 4`；从 v5 `--target 4` 下调；迁 Chart 时改写 CXT
  源文件；打开文件时隐式迁移。
- 第 4.3 节：Hermite 放宽、`fovY (0,180)`、镜像/零缩放、参数化扩展到 rotation/拓扑/alpha，以及为
  position/scale/Beat 新设与 v4 不同的作者值域。
- 未关闭 V5-0 即落地正式发行 Schema、Reader/Writer，或把候选 Chart v5 写成已获得正式支持。
- 放宽 v4/CXT 安全预算，或为 v5 新增 Playback 公共 C++ 类型、FrameSnapshot 字段或 FrameDigest v4。
- 删除 v1-v4 Reader、Schema、迁移入口或长期兼容路径。没有独立退出 ADR、迁移报告、外部资产盘点和
  owner acceptance 不得删除；即使具备这些，删除也不构成本阶段关闭条件。
- 修改现有 [CHART_V4_AUTHORING.md](../../../guides/CHART_V4_AUTHORING.md)。

## 8. 交接

阶段关闭后，v5 是唯一默认写出和新增能力的格式，并作为 Stage 8 之后的开发基线。Playback 继续直接
求值未迁移的 v1–v4；窗口结束后的 Reader、Schema、迁移入口和 fixture 删除另立退出门禁，按
[ADR 0041](../../../adr/0041-legacy-format-exit-policy.md) 与后续版本决策执行，不在本阶段完成。

交接给 Stage 8 之后的开发阶段的接手清单：

```text
SDK API 0.8.0；默认写出 Chart v5；新模板 CXT v2；CXC 仍为 v1，发行入口为 Packed Chart
AnimationClip v2（随 Chart v5 / CXT v2）；v4 / CXT v1 仍为 clip v1
behavior.event v2（随 Chart v5）；v3 / v4 仍为 v1
cuexis.chart.v5 / cuexis.source.cxt.v2 / cuexis.animation.clip.v2 / cuexis.behavior.event.v2
默认 Session 含上述 v5 ID；layers.v1 仍用于非空动画
Chart v5 只 import CXT v2；Chart v4 只 import CXT v1
旋转与相机姿态沿用 v4；fovY (0,179)
render.alpha / renderable.alpha 作者层 [0,255]；省略默认 255；canonical 始终写出静态 alpha
文档边界 /255 一次 → 既有 PropertyId::MaterialOpacity [0,1] / snapshot.materialOpacity
不新增 PropertyId / HostPropertyId；propertyCount = 10
Hermite 斜率仍为进度斜率；Chart/CXT 迁移斜率照抄
opacity 迁移有损；未迁移 v4 digest 不变
FrameSnapshot 不增字段；FrameDigest v1–v3 对合法 v4 输入不变
Playback 继续直接求值 v1–v4；v4 走 material.opacity [0,1]
Writer 默认 v5；--target 4 保留且拒绝 v5 输入；已去掉 --target 3
迁 Chart 不改写 CXT；CXT v1→v2 为独立工具；Chart 迁移遇 CXT v1 import 则失败
不删除 Reader
日期构建版本门禁、后端中立渲染、常用媒体支持仍归 Stage 6
纹理 pixel alpha 不得改写 Chart 对象 alpha 合同
```

上述 SDK `0.8.0` 清单是 Stage 8 关闭后的交接目标，不是 Stage 6 启动基线。
Stage 6 的 v5-first candidate path 使用 Foundation 已接受的 subset，保留 SDK `0.7.0`
兼容基线；不得把主要开发基线改回 v4。正式默认 Writer 和发行入口仍要等 Stage 8 关闭。

Stage 10 Studio 必须以 v5 typed authoring model 为编辑对象，通过 PlaybackSession 预览。旋转与默认相机
姿态必须与 v4/Playback 同一套求值，不得另造欧拉曲线或从 Quaternion 反推未展开角度，也不得维护第二套
alpha 求值语义或 `[0,255]` Mixer。Studio 排在 Stage 6 的 Playback/Player 路径稳定之后。
