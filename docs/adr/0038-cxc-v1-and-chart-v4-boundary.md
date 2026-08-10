# ADR 0038：CXC v1、Chart v4 与 CXT v1 动画序列化边界

日期：2026-08-10

状态：提议；CXT v1、播放前参数、Template Binding 与“运行时脚本无限期延后”子决策已于
2026-08-10 接受，CXC 载体及 Chart v4 其余合同仍待项目所有者接受

关系：细化 ADR 0024、0025、0026、0034、0036、0037 和 0019。CXC 容器与闭包权威见
[CXC_FORMAT.md](../CXC_FORMAT.md)，Chart v4 字段与 lowering 权威见
[CHART_V4_FORMAT.md](../CHART_V4_FORMAT.md)，CXT 文件语义权威见
[CXT_FORMAT.md](../CXT_FORMAT.md)。本决策不改变 Playback SDK、ProjectConfig、AssetDatabase、
ChartRuntime、World 或 FrameSnapshot 的既有所有权边界。

## 背景

Stage 3 已建立目录 Project、Asset Index、portable resource、PlaybackSource 和外部 consumer 闭环，但当前发布内容仍由多个文件组成。`cuexis.chart` v3 可以表达全局 Beat 上的 Behavior Event，却不能序列化 Stage 4 需要的可复用 AnimationClip、Layer、BlendGroup、实例权重、property mask 和循环绑定，也不能引用包内声明式动画模板或接收播放前冻结的 typed 参数。

“CXC”此前没有格式身份、文件扩展名、载体、版本、路径、安全预算或迁移语义。如果直接把它当作新的 Chart 文档，会复制 `cuexis.project` 与 Asset Index 的入口和资源职责；如果把它当作 Runtime cache，又会把编译器版本、平台 ABI 和内部布局错误地变成交换格式合同。

本 ADR 提议把外层交换载体和内层 Chart 语义分开冻结。

## 提议决策

### 三类 artifact

Cuexis 明确区分：

```text
Source Project
  可编辑目录；使用 cuexis.project.json、Asset Index、Chart JSON、CXT JSON 和源/导入资源

CXC Exchange Package
  自包含、只读、可验证的单文件交换/部署包；扩展名 .cxc

Compiled Runtime
  进程内 ChartRuntime、AnimationProgram、World 和缓存；不序列化为 CXC v1
```

CXC 不替代 `cuexis.chart`，也不是二进制 ChartRuntime cache。Studio 的权威编辑源仍是 Source Project；导出 CXC 是显式操作。导入/解包只能恢复包内的播放闭包，不能重建未打包的原始创作资产、Importer 中间数据或 Studio 历史；Studio 必须把它作为只读内容或建立新的编辑文档，不能把运行时状态反向写入包。

### CXC v1 身份与载体

CXC v1 使用：

```text
文件扩展名：.cxc
manifest format：cuexis.cxc
manifest version：1
物理载体：严格 ZIP32 Stored 子集
固定 manifest：cuexis.cxc.json
固定项目入口：cuexis.project.json
```

选择 ZIP32 Stored 是为了复用成熟归档实现和工具，同时避免自研容器、压缩器差异、解压膨胀和不必要的运行时格式绑定。v1 不允许 Deflate、ZIP64、加密、多卷、data descriptor、archive/entry comment、extra field、目录项、符号链接或其他特殊文件。

规范 writer 固定 entry 顺序和所有 ZIP metadata，使相同输入产生相同 `.cxc` bytes。reader 可以使用任意内部库，但第三方类型不得进入 Cuexis 公共 API。具体依赖选择仍须遵守 `docs/DEPENDENCY_POLICY.md`。

### CXC manifest

`cuexis.cxc.json` 的 v1 结构为：

```json
{
  "format": "cuexis.cxc",
  "version": 1,
  "project": "cuexis.project.json",
  "entries": [
    {
      "path": "assets/charts/main.cuexis.chart.json",
      "byteCount": 4096,
      "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    }
  ],
  "requiredExtensions": [],
  "extensions": {}
}
```

manifest 自身不出现在 `entries`。Archive 必须恰好包含 `cuexis.cxc.json` 和 `entries` 列出的文件；未列出、缺失、重复或 hash/size 不匹配的 entry 均失败。`entries` 按 `path` 的 portable ASCII bytes 升序排列，数组顺序是规范化要求，但不用于解决 ID 或语义冲突。

`.cxc` 文件整体 SHA-256 是包内容缓存 identity。Chart、Mesh、Texture、Material 和 Audio 的语义 identity 继续由各自格式决定；CXC 不用 archive offset、CRC、时间戳或文件顺序替代资源 identity。

### CXC 路径与闭包

所有路径使用正斜杠分隔的 portable ASCII 相对路径，并继承 ADR 0025 的拒绝规则。额外拒绝：

```text
大小写折叠后重复的 entry path
目录 entry、symlink、junction、device、FIFO 或其他特殊类型
项目或 Asset Index 指向包外、绝对路径、URL 或未列出的 entry
一个 entry 被多个不同 path alias 指向
archive 中存在未由项目/索引闭包使用的隐藏 payload（manifest 自身除外）
```

CXC v1 恰好包含一个根 `cuexis.project.json`。ProjectConfig v1 的 `projectId`、`assetRoots` 和 `entry.chart` 语义保持不变；所有 root 和 source 只在包的逻辑命名空间解析，不执行物理文件系统 canonicalization。每个 root 仍使用固定 `cuexis.asset-index.json`，AssetId 和依赖规则保持不变。

CXC v1 是完整闭包，不支持外部 URL、另一个 CXC、宿主隐式资源或运行时下载。宿主仍可绕过 CXC，直接使用 typed/memory Project 和自己的 ContentProvider。

Chart `animationTemplateImports` 引用的 `.cxt` 是闭包内的普通 JSON entry。CXC 必须包含确切
bytes 和 manifest hash；Playback 不得按模板名称从 SDK 安装目录、网络或宿主隐式路径补全模板。

### Chart v4

`cuexis.chart` 保持规范 Chart 文档身份。v4 继承 v3 的 Timing、Tempo Event、Stop、Behavior Event、Step Event、Object、Template、Audio、Extension 和未知字段语义，并新增：

```text
顶层必需 parameters 数组，允许为空
顶层必需 animationTemplateImports 数组，允许为空
顶层必需 animationClips 数组，允许为空
顶层 AnimationClip version 1 定义
Object/Template 可选 cuexis.animator component version 1，包含 templateBindings 与 layers
Reference domain 新增 chart-parameter、animation-template 和 animation
capability cuexis.chart.v4
存在 animationTemplateImports 时需要 cuexis.source.cxt.v1
内容非空时需要 cuexis.animation.clip.v1 与 cuexis.animation.layers.v1
```

空 `parameters`、`animationTemplateImports`、`animationClips` 且没有 `cuexis.animator` 的 v4 Chart
与对应 v3 Chart 语义等价。Chart v4 不改变 v3 Behavior；迁移器不得把 Behavior Event 自动改写
为 AnimationClip 或 CXT。

### ChartParameter v1

Chart v4 只声明参数类型、默认值和范围；宿主在 prepare 前提供 typed `ChartParameterSet`。v1
参数类型为 finite `number`、规范化 `rational` 和 `[0,1]` `weight`。参数引用使用独立
`chart-parameter` domain，且只允许出现在格式明确标记的 numeric、duration scale 和 weight
字段。

v1 不支持表达式字符串、通用 AST、函数、随机数、条件分支或字段读取。ParameterSet 在 prepare
开始时完成未知/缺失/重复 ID、类型、finite、范围和用途校验，随后冻结并按 ID 规范化。
`CXC content identity + normalized parameter identity` 共同参与 PreparedPlayback、cache 和 reload
identity。参数不得改变 Object topology、parent、组件集合、AssetId、引用、资源闭包、startBeat、
priority、iterations、fillMode、blendMode 或 property mask。

### CXT v1 与 Chart import

CXT v1 使用 `.cxt` 扩展名和 UTF-8 JSON：

```text
format      cuexis.animation-template
version     1
导出        每个文件恰好一个 Animation Template
时间域      Clip 局部有理 Beat
参数        CXT 内只保存 literal，不直接引用 ChartParameter
```

Chart 顶层 `animationTemplateImports[]` 使用稳定 `id` 和相对 Source Project/CXC 根的 canonical
`source` path。Loader 必须验证 source 存在、format/version、CXT `templateId` 与 import ID 一致、
ID/path 唯一、预算和 required extension。CXT 不提供继承、嵌套 import、多模板导出、Object 创建
或脚本执行。

CXT 保存固定 `application`：v1 `coordinateSpace` 只允许 local，并保存 blendMode、iterations 和
fillMode；`clip` 复用 AnimationClip v1 Track/Segment/Step 结构但不重复保存 Clip ID。相同模板可
绑定到不同 Object 或 Object Template；每个具体 Object 的 parent 和初始 Transform 独立保存。

### AnimationClip v1

AnimationClip v1 使用局部有理 Beat：

```text
id             Chart 内稳定唯一 ID
version        固定 1
durationBeats  严格大于 0
tracks         连续属性 Track
stepTracks     离散属性 Track
```

连续 Track 由有序 Segment 构成，字段与 v3 Behavior Event 的插值核心一致：`startBeat`、`durationBeats`、`startValue`、`endValue`、`startSlope`、`endSlope`。Segment Beat 是 Clip 局部 Beat，必须位于 `[0, durationBeats]`，同一属性不得重叠或具有相同开始 Beat。输入数组顺序无语义；编译器按 Property ID 和 Beat 排序。

求值规则：

```text
第一个 Segment 开始前：该 Track 不写入
Segment 区间：按规范 Hermite progress 插值
Segment 间隙：保持前一 Segment endValue
最后 Segment 结束后：保持 endValue 到 Clip 结束
零持续 Segment：在精确 Beat 建立新的保持值
```

Step Track 使用 `beat` 和 typed `value`；首个 Step 前不写入，之后保持最后 Step 值。Clip 的所有 Track 在一次 Layer 求值中原子提交候选写入。

v1 连续属性白名单：

```text
transform.position.x/y/z
transform.rotation
transform.scale
material.opacity
material.tint
```

v1 Step 属性白名单：

```text
render.visible
render.material
```

`camera.fovY` 继续由 Behavior 表达，不进入 AnimationClip v1。Additive v1 只允许 position、rotation 和 scale；Material、Visibility 和 Material selection 只能使用 Override。

所有 Chart-local AnimationClip 和 imported CXT 中出现的 Asset reference 都参与 Chart 资源闭包和
CXC 打包校验，包括未绑定 Clip、weight 0 Instance/Template Binding 或当前 mask 排除的 Track。
实现不得根据当前时间、权重、绑定活动状态或宿主能力省略候选资源；Stage 3 的 candidate
manifest/reload 原子性继续适用。

### cuexis.animator v1

Animator component 保存初始、确定的 Chart 调度，不保存 Session 运行状态：

```text
templateBindings[]
  bindingId
  animation-template reference
  startBeat
  durationScale = positive rational literal | rational ParameterRef
  weight = [0,1] literal | weight ParameterRef
  priority

layers[]
  layerId
  priority
  weight = [0,1] literal | weight ParameterRef
  propertyMask { properties[], prefixes[] }
  blendGroups[]
    groupId
    mode = override | additive
    weight = [0,1] literal | weight ParameterRef
    instances[]
      instanceId
      clip reference
      startBeat
      iterations = positive integer | "infinite"
      fillMode = none | hold
      weight = [0,1] literal | weight ParameterRef
      propertyMask { properties[], prefixes[] }
```

Clip 定义本身不绑定 Object。Animator Instance 把 Clip 绑定到拥有该 component 的 Object；同一 Clip 可以被多个 Object/Template 实例复用。对 Layer/Instance mask 过滤后的 effective property 执行组件校验：Transform 属性要求 `cuexis.transform`，Material/Render 属性要求 `cuexis.renderable`。被 mask 排除的 Clip 属性不构成该 Object 的缺失组件错误。

Template Binding 是面向 Chart 的简化引用。prepare 将每个 Binding 确定性 lowering 为一个 concrete
Clip、一个 generated Layer、一个 generated BlendGroup 和一个 generated ClipInstance：priority
来自 Binding，Layer/Group weight 固定 1，Instance weight 来自 Binding，mode/iterations/fillMode
来自 CXT application，mask 是 CXT Clip 实际写入的属性集合。generated ID 由 Object ID、bindingId
和 templateId 确定派生，不写回 Chart，也不能被其他 Chart 字段引用。

`durationScale` 的语义固定为：

```text
effectiveDuration = cxtDuration * durationScale
localBeat = (chartBeat - startBeat) / durationScale
```

Template Binding 与显式 Layer 进入同一 priority/mask 冲突检查。相同 priority 的相交 mask 失败，
不得使用 `templateBindings`、`layers` 或 Object 输入顺序打破冲突。

Layer、BlendGroup 和 Instance 的输入数组顺序无语义，分别按稳定 ID 规范化。相同 Layer priority 的 mask 不能重叠；同一 Layer 的不同 BlendGroup 不能写入同一属性。Override/Additive、Quaternion、离散值和 tie-break 规则继续由 `docs/ANIMATION_MIXING.md` 定义。

实例时间使用 Chart 全局 Beat：

```text
elapsed = chartBeat - startBeat
elapsed < 0：不写入
活动迭代：localBeat = elapsed modulo clip.durationBeats
有限迭代精确结束：fillMode none 不写入；fillMode hold 采样 Clip 结束值
infinite：持续循环，不允许 fillMode hold
Stop：chartBeat 固定，因此 localBeat 固定
Seek/discontinuity：直接从目标 chartBeat 重建，不读取上一帧状态
```

v1 不保存 playback-rate `speed`、reverse、ping-pong、动态 weight 曲线、状态机、事件回调或暂停
状态。Template Binding 的 `durationScale` 是 prepare-time frozen rational，不是运行时 speed command。
Stage 4 的播放/暂停控制属于 Session-local typed command，不写回 Chart，也不进入 CXC identity。
HostOverride 和 StudioPreviewOverride 永不序列化。

运行时脚本与逐帧脚本回调无限期延后，不属于 Stage Chart Format Update、Stage 4 或任何其他已排期
阶段。本 ADR 不为其预留 Chart/CXT/CXC 字段、extension、capability、字节码、模块 ABI 或 Playback
执行入口。只有新的明确产品决策和独立 ADR 才能重新启动该议题。

### Playback 接入

CXC loader 必须把已验证的包映射到现有逻辑边界：

```text
CXC bytes/file
-> validated manifest and entry table
-> typed ProjectConfig + Asset Index + Chart/CXT text
-> package-backed IContentProvider
-> validate/freeze ChartParameterSet and resolve CXT imports
-> PlaybackSource
-> existing prepare / validate / commit / activate transaction
```

它不得建立第二条 Runtime、ResourceManager 或 presentation 路径。确切 C++ factory 名称在实现批次冻结；公共输入仍必须是 owning source，不暴露 ZIP library 对象、JSON DOM、archive offset 或可变文件句柄。

格式阶段可以读取和验证含动画内容的 Chart v4，但在 Stage 4 capability 尚未实现时，prepare 必须以稳定的 capability 诊断失败，不能跳过动画后继续播放。

### 兼容与迁移

兼容策略：

```text
Chart v1/v2/v3 Reader 和可观察结果保持不变
v3 -> v4：显式迁移，只增加空 parameters、animationTemplateImports、animationClips 并提升顶层 version
v1/v2 -> v4：通过现有 v3 迁移再执行 v3 -> v4
CXC pack：只打包并验证，不执行 Chart 或资产迁移
CXC unpack：只验证并恢复 source tree，不自动升级内容
```

迁移和 pack/unpack 都不覆盖输入。Chart v3 与迁移后的空动画 v4 必须有 FrameSnapshot/FrameDigest 等价证据；因为 v4 capability 集合不同，不要求 capability summary 字节完全相同。

## 预算提案

| 项目 | CXC v1 上限 |
| --- | ---: |
| `.cxc` 文件和所有 uncompressed entries 总量 | 512 MiB |
| 单个 entry | 64 MiB |
| manifest | 1 MiB |
| entry 数量 | 65,536 |
| path bytes | 4,096 |
| archive path depth | 64 |
| CXT imports / Chart | 10,000 |
| 单个 CXT document | 4 MiB |
| ChartParameter declarations | 256 |
| host ParameterSet entries | 256 |
| animationClips / Chart | 10,000 |
| tracks / Clip | 256 |
| segments or steps / Track | 65,536 |
| Template Binding / Animator | 256 |
| Layer / Animator | 64 |
| BlendGroup / Layer | 64 |
| Instance / BlendGroup | 256 |

既有 Chart、Asset、Audio 和 Portable Presentation 的更严格预算继续同时生效。任何 checked arithmetic、计数、offset 或总量溢出均在分配、资源获取和 World 发布前失败。

## 稳定诊断类别

实现至少提供：

```text
cxc.format.unsupported
cxc.version.unsupported
cxc.archive.invalid
cxc.archive.feature_unsupported
cxc.entry.path_invalid
cxc.entry.duplicate
cxc.entry.missing
cxc.entry.unlisted
cxc.entry.size_mismatch
cxc.entry.hash_mismatch
cxc.budget.exceeded
cxc.project.invalid
cxt.format.unsupported
cxt.version.unsupported
cxt.template.invalid
cxt.template.id_mismatch
cxt.import.missing
cxt.import.duplicate
cxt.budget.exceeded
chart.parameter.unknown
chart.parameter.missing
chart.parameter.type_mismatch
chart.parameter.out_of_range
chart.parameter.use_not_allowed
chart.animation.clip_invalid
chart.animation.reference_missing
chart.animation.template_reference_missing
chart.animation.template_binding_conflict
chart.animation.track_conflict
chart.animation.mask_conflict
chart.animation.additive_unsupported
playback.capability.missing
```

精确字段路径和 context 在实现前由共享正反例冻结。CXC 诊断不得泄漏宿主绝对路径或 archive library 私有错误文本。

## 被拒绝的方案

### 用 CXC 替换 cuexis.chart

拒绝。它会把单 Chart 文档、Project bootstrap、Asset Index 和资源闭包混成一个身份，并破坏现有 Chart v1/v2/v3 迁移与 memory source 路径。

### 把 CXC 定义为 ChartRuntime 二进制缓存

拒绝。内部 Runtime 会随编译器、平台和阶段演进，不能成为长期交换格式或 Studio 权威文档。

### 允许任意 ZIP

拒绝。压缩方法、ZIP64、extra field、重复 entry、symlink、时间戳和路径别名会扩大攻击面并破坏确定性。

### 只打包目录而没有 manifest/hash

拒绝。无法在发布 World 或资源前证明闭包完整，也无法稳定检测损坏、隐藏 payload 和 cache identity。

### 把动画字段放入 extensions

拒绝。动画会改变 FrameSnapshot 和 capability 要求，属于核心版本化语义，不能伪装成未知可选数据。

### 自动把 Behavior Event 转换成 AnimationClip

拒绝。Behavior 是全局 Chart Beat 上的表现事件；AnimationClip 是可复用局部时间和混合实例。自动转换会改变绑定、基线、优先级和可观察结果。

### 把 moveY/emerge 定义为运行时脚本

拒绝并无限期延后。运行时脚本会引入沙箱、ABI、Seek、Replay、资源闭包、确定性和调试合同。
CXT v1 是 typed JSON 数据，并在 prepare 时 lowering 为 concrete program。离线 authoring generator
可以生成 Chart/CXT，但不进入 CXC，也不由 pack、prepare 或 Playback 隐式执行。

### 由 SDK 按模板名称隐式提供内置实现

拒绝。相同 CXC 在不同 SDK 安装内容下可能得到不同结果，也无法把模板纳入 package hash 和资源
闭包。SDK/Studio 可以分发模板库，但 pack 必须复制被引用 `.cxt` 的确切 bytes。

## 影响

ADR 0038 其余门禁接受后，CFU-C 可以新增 CXC manifest typed model、strict archive reader/writer、
CXT/Chart v4 Schema/Reader/Writer、ChartParameter/Import/TemplateBinding、AnimationClip/Animator 文档
类型和候选 fixture。`engine/animation` 仍要等 Stage 4 才实现求值；格式阶段只编译 typed 数据和
capability 要求。

新增归档依赖前必须记录上游、版本、许可证、vcpkg/CMake 集成、分发清单和退出路径。基础 Playback public headers 不得传播该依赖。

## 接受门禁

项目所有者接受本 ADR 即确认以下七项方向：

```text
1. .cxc 是严格 ZIP32 Stored 自包含交换包，不是 Chart 替代品或 Runtime cache
2. 包内复用 cuexis.project / Asset Index / cuexis.chart，Chart 动画语义进入 v4
3. AnimationClip v1 只使用局部有理 Beat 和固定 Segment/Step Track
4. Chart 内 Animator v1 只保存确定性初始调度；动态控制和 Override 不持久化
5. .cxt 是包内 UTF-8 JSON 声明式 Animation Template，不是脚本或 SDK 隐式内置实现
6. 宿主参数只在 prepare 前冻结，并只改变显式允许的数值、duration scale 和 weight
7. 运行时脚本无限期延后，不预留格式字段、capability、字节码或 Playback 执行入口
```

第 5-7 项及其 `templateBindings` 引用/下沉方向已由项目所有者于 2026-08-10 明确接受。第 1-4 项
和 ADR 整体状态仍待确认；在整体接受前不开始 CFU-C 生产实现。任何已接受子决策需要改变时，
必须先修订 ADR、字段规范和正反例并再次取得接受。
