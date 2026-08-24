# Cuexis Chart Format v4

状态：accepted contract；CFU-C Reader/Writer/lowering、CFU-D migration/equivalence 与 CFU-E
prepare/identity 已实现或关闭；CFU-F consumer/determinism/safety gates 已关闭；G3 hosted 与
CFU-G4 已完成，G5/G6 pending；非空动画执行仍属于 Stage 4

更新日期：2026-08-24

依据：[ADR 0038](../adr/0038-cxc-v1-and-chart-v4-boundary.md)

## 1. 范围

本文是 `format: "cuexis.chart"` version 4 的字段权威。v4 在已实现的 v3 Timing、Tempo、
Stop、Behavior、Step Event、Object、Template、Audio 和 Extension 语义上增加：

```text
ChartParameter declarations
Animation Template imports
AnimationClip v1
cuexis.animator component v1
Template Binding
Layer / BlendGroup / Clip Instance
```

本文不定义 CXC ZIP/manifest、CXT 文件内部结构或 AnimationSystem 混合算法。它们分别由
[CXC_FORMAT.md](CXC_FORMAT.md)、[CXT_FORMAT.md](CXT_FORMAT.md) 和
[ANIMATION_MIXING.md](ANIMATION_MIXING.md) 负责。

### 1.1 Typed 处理层

Chart v4 固定以下数据流；名称表示职责，确切 C++ 类型名可在 CFU-C 按现有命名约定确定：

```text
SourceChartDocument
  保留 ParameterRef、CXT import、模板和规范化扩展数据，可由 Writer 重新保存

ResolvedChartDocument
  完成 template expansion、参数解析、CXT import/lowering 和冲突验证，只含 concrete typed value

ChartRuntime + AnimationProgramInput
  供 Runtime/Stage 4 使用，不含 JSON、CXC、CXT path 或未解析参数
```

处理顺序固定为：typed Reader、template expansion、concrete Object 形成、ParameterSet 冻结与解析、
CXT import、Template Binding lowering、capability/预算/冲突验证、Runtime compile。Format handler 不
创建 World/EnTT Entity。

## 2. 顶层

```json
{
  "format": "cuexis.chart",
  "version": 4,
  "chartId": "019f0000-0000-7abc-8def-000000000400",
  "metadata": {},
  "timing": {
    "offsetMs": 0.0,
    "defaultBpm": 120.0,
    "tempoEvents": [],
    "stops": []
  },
  "camera": {
    "type": "perspective",
    "fovY": 60.0,
    "near": 0.1,
    "far": 1000.0
  },
  "parameters": [],
  "templates": [],
  "behaviors": [],
  "animationTemplateImports": [],
  "animationClips": [],
  "objects": [],
  "requiredExtensions": [],
  "extensions": {}
}
```

`audio` 和 `camera` 保持 v3 的可选性；其余 v3 字段继续必需。新增 `parameters`、
`animationTemplateImports` 和 `animationClips` 必需且允许为空。v4 不接受 v1/v2
`bpmChanges` 或 Keyframe Behavior。

空 parameters/imports/clips 且没有 `cuexis.animator` 的 v4 Chart 与对应 v3 Chart 语义等价。

## 3. ChartParameter v1

Chart 声明参数；宿主只在 prepare 前提供 typed `ChartParameterSet`。参数在 prepare 开始时完成
校验、规范化和冻结，不修改 Chart/CXT bytes。ParameterSet 通过 per-prepare/reload options 提交，
不属于 `PlaybackSource` 内容；同一 source 可以用不同参数产生不同 PreparedPlayback。

### 3.1 类型和声明

| 类型 | 值 | 用途 |
| --- | --- | --- |
| `number` | finite JSON number | 明确允许参数化的 Chart 数值字段 |
| `rational` | 规范化 numerator/positive denominator | duration scale |
| `weight` | finite number `[0,1]` | Layer、Group、Instance 或 Template Binding weight |

```json
{
  "id": "motion.duration-scale",
  "type": "rational",
  "default": { "numerator": 1, "denominator": 1 },
  "constraints": {
    "exclusiveMinimum": { "numerator": 0, "denominator": 1 },
    "maximum": { "numerator": 1, "denominator": 1 }
  }
}
```

`default` 可省略；省略时宿主必须提供。`constraints` 必需，只允许与类型匹配的
`minimum`、`exclusiveMinimum`、`maximum`、`exclusiveMaximum`。`weight` 始终执行
`[0,1]` 固有范围。

### 3.2 参数引用

```json
{
  "parameter": {
    "domain": "chart-parameter",
    "id": "motion.weight"
  }
}
```

v1 参数引用不是表达式。它不支持 expression string、通用 AST、函数、字段读取、随机数、条件
分支或运行时变化。宿主需要组合值时，必须在提交 ParameterSet 前计算最终 typed 值。

Scalar 字段使用 `literal | ParameterRef` 联合。允许逐轴参数化的 vec3 仍保持三元素数组，每个元素
独立使用该联合：

```json
{
  "position": [
    { "parameter": { "domain": "chart-parameter", "id": "layout.x" } },
    0.0,
    0.0
  ]
}
```

`camera.fovY`、Layer/Group/Instance weight 和 Template Binding durationScale/weight 使用同一
ParameterRef 外形。Quaternion、Asset reference、ID、path、parent、组件集合和数组结构不得接受
ParameterRef。

首版允许参数化：

```text
cuexis.transform.position x/y/z
cuexis.transform.scale x/y/z
camera.fovY
Animator Layer / BlendGroup / Clip Instance weight
Template Binding durationScale / weight
```

ID、path、AssetId、引用、parent、组件集合、数组长度、startBeat、priority、iterations、fillMode、
blendMode 和 property mask 必须保持 literal。Quaternion、资源字段和 CXT Track/Segment/Step
value 也保持 literal。

prepare 拒绝未知 ID、重复 ID、缺失 required、类型错误、非有限值、范围错误和不允许的引用用途。
解析结果按 ID 排序并规范化。Parameter identity 使用以下域分隔二进制编码的 SHA-256：

```text
domain tag        "cuexis.parameter-set.v1\0"
entry order       portable ASCII ID ascending
ID                uint32 little-endian byte length + ASCII bytes
type              fixed one-byte tag: number=0x01 / rational=0x02 / weight=0x03
number / weight   IEEE-754 binary64 bits, little-endian, -0.0 normalized to +0.0
rational          reduced signed int64 numerator + positive int64 denominator, little-endian
```

`PreparedSemanticIdentity` 使用域分隔 SHA-256 组合 canonical Chart identity、按 import ID 排序的
CXT identity、按 AssetId 排序的实际 resource identity manifest 和 parameter identity。CXC 的精确
package hash、archive metadata、Provider revision 和 source path 不参与该值，因此 filesystem、
memory、host 与 CXC source 对相同规范内容和参数必须得到相同结果。

最终 digest 是下列规范二进制的 SHA-256：

```text
domain            "cuexis.prepared-semantic.v1\0"
chart             32-byte canonical Chart identity
cxtCount          uint32 little-endian
each CXT          uint32 little-endian importId byte length + ASCII bytes + 32-byte CXT identity
                  (importId portable ASCII ascending)
resourceCount     uint32 little-endian
each resource     uint32 little-endian AssetId byte length + ASCII bytes + 32-byte content identity
                  (AssetId portable ASCII ascending)
parameter         32-byte parameter identity
```

资源条目使用 prepare 期间实际获取并 typed 校验后的内容 identity，不是 C2 的
`resourceRequirements` 需求表。同一 AssetId 即使有多种用途也只写一条。缺资源或同一 AssetId
对应不同内容 identity 时，prepare 失败且不发布 candidate。

资源内容 identity：

```text
Mesh / Texture2D / UnlitMaterial
  PresentationContentIdentity（见 PORTABLE_PRESENTATION.md）

MainMusic
  SHA-256("cuexis.prepared-audio.v1\0" + exact fetched bytes)
  不使用 contentRevision、Provider revision、path 或 lease token
```

空 CXT 列表与空资源 manifest 仍写入 count `0`。v1/v2/v3 成功 prepare 使用 canonical Writer
bytes 的 Chart identity、空 CXT 列表、空 parameter identity 和同样的实际资源 manifest。

## 4. Animation Template import

```json
"animationTemplateImports": [
  {
    "id": "motion.move-y",
    "source": "templates/move-y.cxt"
  }
]
```

Import record 只允许 `id` 和 `source`。`source` 相对于 Source Project/CXC 根，不相对于 Chart
文件。Importer 不执行目录扫描、名称猜测或 SDK 安装目录回退。

加载必须验证：

```text
import id 唯一
source path 唯一、满足 CXC portable path 规则并以小写 `.cxt` 结尾
source 存在且是普通文件
CXT format/version 正确
CXT templateId 与 import id 完全相同
requiredExtensions 可满足
```

模板引用使用：

```json
{ "domain": "animation-template", "id": "motion.move-y" }
```

`animation-template` 和 `animation` 是不同 reference domain，可以复用相同文本 ID。Import ID 在
Chart import table 内唯一，AnimationClip ID 在 Chart clip table 内唯一；任何未带正确 domain 的
引用失败。ID、bindingId、layerId、groupId 和 instanceId 均使用 portable stable ID 规则。

## 5. AnimationClip v1

### 5.1 定义

```json
{
  "id": "animation.note.pulse",
  "version": 1,
  "durationBeats": { "numerator": 4, "denominator": 1 },
  "tracks": [],
  "stepTracks": []
}
```

`durationBeats` 严格大于 0。`tracks` 与 `stepTracks` 至少一个非空。Clip ID 在 Chart 内唯一。
Clip 使用局部有理 Beat，不绑定 Object。

### 5.2 Continuous Track

```json
{
  "property": "transform.scale",
  "segments": [
    {
      "startBeat": { "numerator": 0, "denominator": 1 },
      "durationBeats": { "numerator": 2, "denominator": 1 },
      "startValue": [1.0, 1.0, 1.0],
      "endValue": [1.25, 1.25, 1.25],
      "startSlope": 0.0,
      "endSlope": 0.0
    }
  ]
}
```

| Property | 类型 | Override | Additive |
| --- | --- | --- | --- |
| `transform.position.x/y/z` | scalar | finite | delta |
| `transform.rotation` | quaternion | normalized finite quaternion | delta quaternion |
| `transform.scale` | vec3 | finite | positive factor |
| `material.opacity` | scalar | `[0,1]` | 不支持 |
| `material.tint` | vec3 | each `[0,1]` | 不支持 |

Segment 复用 v3 Behavior Event 的 Hermite progress、零持续、重叠、相邻边界、Quaternion
shortest-path 和 slope 范围。不同点是 Beat 属于 Clip 局部域，首 Segment 前不写入；间隙保持前一
Segment endValue，最后 Segment 后保持到 Clip 结束。

### 5.3 Step Track

```json
{
  "property": "render.visible",
  "steps": [
    {
      "beat": { "numerator": 1, "denominator": 1 },
      "value": false
    }
  ]
}
```

| Property | 类型 | 模式 |
| --- | --- | --- |
| `render.visible` | Boolean | Override only |
| `render.material` | Asset reference | Override only |

首 Step 前不写入，之后保持最后值。相同 Beat、Clip 范围外 Beat、错误引用域和 Additive 使用均失败。

所有 Chart-local Clip 与 imported CXT Clip 的 Asset reference 都进入资源闭包，包括未绑定 Clip、
weight 0 Instance/Binding 和被 mask 排除的 Track。

### 5.4 规范化

同一 Clip 对同一 Property ID 最多有一个 Track；重复 Property Track、重复 Segment startBeat 或重复
Step beat 直接失败，不静默去重。Reader 不赋予输入数组顺序语义，Writer 使用：

```text
tracks / stepTracks    Property ID ascending
segments               startBeat ascending
steps                  beat ascending
animationClips         clip ID ascending
animationTemplateImports import ID ascending
parameters             parameter ID ascending
```

Rational Beat 在比较和写出前约分；JSON object key 使用 canonical writer 顺序。数组 record 的主
排序键相同时，以已完成 Rational/number/object-key 规范化的 compact JSON bytes 按 portable bytes
升序决胜；Writer 输出不依赖等价主键 record 的输入顺序。

## 6. cuexis.animator component v1

```json
{
  "cuexis.animator": {
    "version": 1,
    "templateBindings": [],
    "layers": []
  }
}
```

一个 Object/Template 至多一个 Animator。`templateBindings` 和 `layers` 均必需并允许为空；两者
同时为空时 component inert，不要求 animation capability。

Clip Instance 把 Chart-local Clip 绑定到拥有 Animator 的 Object。Template Binding 把 imported CXT
绑定到该 Object。相同 Clip/CXT 可以由多个 Object 或 Object Template 复用，每个具体 Object 的
parent 和初始组件仍由 Chart 保存。

对 Layer/Instance mask 过滤后的 effective property 执行组件校验：Transform 属性要求
`cuexis.transform`，Material/Render 属性要求 `cuexis.renderable`。

Object/Template 的 v1 patch 只允许对整个 `/components/cuexis.animator` 执行 add/remove/replace。
不得 patch Animator 内部数组元素、priority、weight 或 mask；需要变化时替换整个 component。
违反该边界时沿用现有 `chart.patch.path_unsupported`。

### 6.1 Template Binding

```json
{
  "bindingId": "move-y",
  "template": { "domain": "animation-template", "id": "motion.move-y" },
  "startBeat": { "numerator": 8, "denominator": 1 },
  "durationScale": {
    "parameter": { "domain": "chart-parameter", "id": "motion.duration-scale" }
  },
  "weight": {
    "parameter": { "domain": "chart-parameter", "id": "motion.weight" }
  },
  "priority": 10
}
```

`bindingId` 在该 Animator 内唯一。`durationScale` 是正 rational literal 或 compatible ParameterRef；
`weight` 是 `[0,1]` literal 或 compatible ParameterRef。`startBeat` 和 priority 必须是 literal。
Binding 不能覆盖 CXT 的 coordinateSpace、blendMode、iterations、fillMode 或属性集合。

### 6.2 确定性 lowering

prepare 把 Binding 展开为 concrete Clip 和 generated records：

```text
Generated Layer
  priority = binding.priority
  weight = 1
  propertyMask = CXT Clip actual properties

Generated BlendGroup
  mode = CXT application.blendMode
  weight = resolved binding.weight

Generated ClipInstance
  startBeat = binding.startBeat
  durationScale = resolved binding.durationScale
  iterations/fillMode = CXT application
  weight = 1
```

Binding weight 放到 generated Group，避免单 Instance Override Group 在 weight 归一化后丢失
Binding 的整体贡献强度。

Generated identity 是内部复合键 `(objectId, bindingId, templateId, recordKind)`，按 portable bytes
逐字段比较，不生成截断 hash 字符串，不写回 Chart，也不能被其他 Chart 字段引用。诊断保留原始
Object ID、bindingId、templateId、recordKind 和字段路径。Generated records 与显式 Layer 使用
完全相同的 priority/mask 冲突规则。

### 6.3 Layer

```json
{
  "layerId": "layer.pulse",
  "priority": 10,
  "weight": 1.0,
  "propertyMask": {
    "properties": ["material.opacity", "transform.scale"],
    "prefixes": []
  },
  "blendGroups": []
}
```

priority 是 signed integer，越大越晚应用。weight 是 `[0,1]` literal 或 `weight` ParameterRef。
prefix 必须以 `.` 结束并匹配至少一个已知属性。空 mask 不允许写入，不是 wildcard。同一 Animator
中相同 priority 的不同 Layer 不能具有相交 mask；输入顺序无语义。

Mask 的 `properties` 和 `prefixes` 分别按 portable bytes 升序规范化。重复项、一个 property 同时被
多个 prefix 覆盖、或同一 mask 中互相覆盖的 prefix 都失败，不静默折叠。Instance mask 必须在
展开 prefix 后成为 Layer mask 的真子集或相等集合。

### 6.4 BlendGroup

```json
{
  "groupId": "group.pulse",
  "mode": "override",
  "weight": 1.0,
  "instances": []
}
```

Group ID 在 Layer 内唯一。`mode` 为 `override` 或 `additive`。不同 Group 的 effective property set
不能重叠。

写入 `render.visible` 或 `render.material` 的 Group 必须是 Override，且 resolved Layer weight 与
Group weight 必须都等于 1。Instance weight 只用于 Group 内离散 winner 选择；部分 Layer/Group
weight 对离散值没有隐式阈值语义，违反时报告 `chart.animation.discrete_weight_unsupported`。

### 6.5 Clip Instance

```json
{
  "instanceId": "instance.pulse",
  "clip": { "domain": "animation", "id": "animation.note.pulse" },
  "startBeat": { "numerator": 16, "denominator": 1 },
  "iterations": 2,
  "fillMode": "none",
  "weight": 1.0,
  "propertyMask": {
    "properties": ["material.opacity", "transform.scale"],
    "prefixes": []
  }
}
```

`iterations` 是 `1..65535` 或字符串 `"infinite"`。`fillMode` 为 `none` 或 `hold`；infinite
只允许 none。Instance mask 必须是 Layer mask 的子集；空 mask 不写入。

`templateBindings`、`layers`、`blendGroups` 和 `instances` 分别按 bindingId、layerId、groupId 和
instanceId 的 portable bytes 升序规范化。所有 ID 在其声明作用域内唯一；重复 ID 直接失败。

## 7. 时间和循环

Clip 使用局部有理 Beat。v1 不支持 millisecond domain、运行时 speed、reverse 或 ping-pong。

```text
elapsed = chartBeat - startBeat
elapsed < 0: no write
iterationIndex = floor(elapsed / durationBeats)
localBeat = elapsed - iterationIndex * durationBeats
```

Template Binding 使用：

```text
effectiveDuration = CXT clip.durationBeats * durationScale
localBeat = elapsed / durationScale
```

内部迭代结束边界进入下一迭代的 localBeat 0。有限实例最终边界：none 停止写入；hold 继续采样
Clip 结束值。Stop 固定 chartBeat；Seek/reload 从目标 Beat 重建。负 startBeat 合法，Clip 内局部 Beat
不得为负。

| 边界 | v1 结果 |
| --- | --- |
| `elapsed < 0` | Instance 不写入 |
| 内部迭代精确结束 | 下一迭代 `localBeat = 0` |
| 有限最终边界 + `none` | 不写入 |
| 有限最终边界 + `hold` | 采样 `localBeat = durationBeats` |
| `infinite` | 持续循环，`fillMode` 必须为 `none` |
| Stop | chartBeat/localBeat 均保持 |
| Seek/discontinuity | 只从目标 chartBeat 重建 |
| negative startBeat | 合法；目标时间可能已位于后续迭代 |
| zero-duration Segment | 精确 Beat 建立保持值，不创建零时长 Clip |

## 8. 混合和运行时边界

固定求值顺序、Override/Additive、Quaternion、scale、离散属性和 tie-break 由
[ANIMATION_MIXING.md](ANIMATION_MIXING.md) 定义。Chart 只保存确定性初始调度，不保存
HostOverride、StudioPreviewOverride、暂停状态、当前 localBeat、上一帧结果或 Runtime Token。

运行时脚本与逐帧脚本回调无限期延后。v4 不预留脚本字段、extension、capability、字节码或
Playback 执行入口。

## 9. Capability

```text
cuexis.chart.v4
cuexis.source.cxt.v1
cuexis.animation.clip.v1
cuexis.animation.layers.v1
```

空 parameters/imports/animation 的 v4 只要求 `cuexis.chart.v4`。存在 CXT import 时还要求
`cuexis.source.cxt.v1`。任意非空 CXT import、AnimationClip、Template Binding、Layer 或 Instance
还要求两个 animation capability；即使定义未绑定或 weight 为 0，也不得根据当前使用状态省略。
Stage 4 未实现时，这类内容必须以 `playback.capability.unsupported` 稳定失败，不能忽略动画。

空 `cuexis.animator` inert，不额外要求 animation capability。Chart v4 不改变 FrameSnapshot 字段；
现有 FrameDigest v3 已包含所有会被 v1 动画修改的公开表现值，因此本阶段不升级 digest 版本。

## 10. 预算与诊断

```text
ChartParameter declarations                 256
host ParameterSet entries                   256
CXT imports / Chart                      10,000
animationClips / Chart                    10,000
tracks + stepTracks / Clip                256
segments or steps / Track                 65,536
TemplateBindings / Animator                256
Layers / Animator                           64
BlendGroups / Layer                         64
Instances / BlendGroup                     256
total Animation Tracks / prepared content   65,536
total Segments + Steps / prepared content 1,048,576
generated records / prepared content       100,000
```

prepared-content Track/Segment/Step 总量按 prepare 峰值内容计算：每个 imported CXT 的 source Clip
计一次，每个 Template Binding lowering 后生成的 concrete Clip 再计一次，Chart-local Clip 计一次。
因此未绑定 import 仍进入总量，一个被多个 Object/Binding 复用的 CXT 还会按每个 concrete Clip
分别计数。该口径覆盖 prepare 同时持有 source document 与 generated program 的内存/处理上界。

现有 16 MiB Chart 输入、1024 diagnostics 和 600,000 Property Write/frame 上限继续生效。

稳定诊断至少包括：

```text
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
chart.animation.discrete_weight_unsupported
chart.animation.generated_limit
```

现有 `playback.capability.unsupported` 用于 capability preflight，不新增意义重复的
`playback.capability.missing`。所有总量使用 checked arithmetic，并在资源获取、Runtime prepare 和
World 发布前失败。

## 11. 迁移

`v3 -> v4` 只增加空 `parameters`、`animationTemplateImports`、`animationClips` 并提升 version。
该空字段合同不因实现批次而改变。迁移不生成 CXT、Template Binding、参数声明、Animator、Clip、
Binding、capability workaround 或运行时脚本。迁移必须显式执行、输出到独立路径并生成结构化报告。

实现入口是内部 `ChartMigrator::migrateToV4` 与 `cuexis_chart_migrator --target 4`。默认 CLI
仍输出 v3。v1/v2 → v4 必须先复用现有 `migrateToV3`，再对规范化 v3 JSON 做 lift；不得把
`ChartWriter::write` 的 v3 投影当作 v3 → v4 输入，也不得从 typed `ChartDocument` 手填
`ChartV4SourceDocument`。已是 v4 的输入报告 `chart.migration.source_version_unsupported`。
v4 报告记录 source/target canonical identity（Writer canonical bytes 的 SHA-256）、字段计数、
生成计数和稳定 diagnostics；不能把 CXC pack 误称为迁移。

迁移后的静态 v4 与源 v3 必须产生相同 FrameSnapshot 和 FrameDigest v3。Chart format version 和
capability summary 可以不同。该运行时等价证据属于 CFU-D3，已由
[CFU-D3 报告](../stage_reports/260814-chart-format-update-d3-equivalence.md) 关闭。D1/D2 的关闭条件是
结构/Writer golden 与 CLI 合同通过本地 Debug 验证；该验证已在本 worktree 取得，D1/D2 已关闭。
整包 CFU-D 已由项目所有者于 2026-08-14 记录“未提供外部资产”并关闭；兼容窗口不缩短，
全部 v1/v2/v3 Reader 与迁移入口保留。证据见
[CFU-D 关闭报告](../stage_reports/260814-chart-format-update-d-close.md)。

## 12. 候选示例

正反例见 [examples/chart_format_update](../examples/chart_format_update/README.md)。CFU-C1 已将接受副本
提升到 `tests/fixtures/chart_format_update/`，由生产 Schema 与独立 Chart v4 typed Reader 验证；
评审目录本身仍不作为生产测试输入。
