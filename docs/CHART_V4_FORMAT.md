# Cuexis Chart Format v4 Candidate

状态：candidate；CXT v1、播放前参数、Template Binding 与运行时脚本无限期延后子决策已于
2026-08-10 接受；ADR 0038 整体仍待接受，尚未实现

更新日期：2026-08-10

依据：[ADR 0038](adr/0038-cxc-v1-and-chart-v4-boundary.md)

## 1. 范围

本文是 `format: "cuexis.chart"` version 4 的候选字段权威。v4 在已实现的 v3 Timing、Tempo、
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
校验、规范化和冻结，不修改 Chart/CXT bytes。

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
解析结果按 ID 排序并规范化；CXC content identity 与 normalized parameter identity 共同参与
PreparedPlayback、cache 和 reload identity。

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
source path 唯一并满足 CXC portable path 规则
source 存在且是普通文件
CXT format/version 正确
CXT templateId 与 import id 完全相同
requiredExtensions 可满足
imported template ID 不与 Chart-local AnimationClip ID 冲突
```

模板引用使用：

```json
{ "domain": "animation-template", "id": "motion.move-y" }
```

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
  weight = 1

Generated ClipInstance
  startBeat = binding.startBeat
  durationScale = resolved binding.durationScale
  iterations/fillMode = CXT application
  weight = resolved binding.weight
```

generated ID 从 Object ID、bindingId 和 templateId 确定派生，不写回 Chart，也不能被其他 Chart
字段引用。诊断保留原始 Object ID、bindingId、templateId 和字段路径。Generated records 与显式
Layer 使用完全相同的 priority/mask 冲突规则。

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
`cuexis.source.cxt.v1`。存在非空 Clip、Template Binding 或显式 Instance 时还要求两个 animation
capability。Stage 4 未实现时，这类内容必须稳定失败，不能忽略动画。

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
```

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
```

## 11. 迁移

`v3 -> v4` 只增加空 `parameters`、`animationTemplateImports`、`animationClips` 并提升 version。
迁移不生成 CXT、Template Binding、参数声明或运行时脚本。迁移必须显式执行、输出到独立路径并
生成结构化报告。

## 12. 候选示例

正反例见 [examples/chart_format_update](examples/chart_format_update/README.md)。ADR 0038 整体接受
并建立 Schema 以前，它们不是生产 fixture，也不被当前 Loader 接受。
