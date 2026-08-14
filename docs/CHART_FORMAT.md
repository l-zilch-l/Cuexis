# Cuexis Chart Format v1/v2/v3

状态：v1/v2/v3 已实现并继续作为 Playback 生产格式族。方案 B 已于阶段 2A.1 移除。ADR 0038
已接受 CXC v1、Chart v4 与 CXT v1。显式迁移默认仍输出 v3；`--target 4` 可将合法
v1/v2/v3 提升为静态空动画 v4。Chart v4 字段见 [CHART_V4_FORMAT.md](CHART_V4_FORMAT.md)，
CXC 容器见 [CXC_FORMAT.md](CXC_FORMAT.md)，CXT 文件语义见 [CXT_FORMAT.md](CXT_FORMAT.md)。

更新日期：2026-08-14

## 1. 范围

本文定义 `format: "cuexis.chart"` 的规范 ChartDocument 结构。它面向保存、迁移、Studio 编辑和 Playback SDK 编译，不是 ChartRuntime、World 或 FrameSnapshot 的内存布局。阶段 1C 已在阶段 1A/1B 的 format 路由、typed 结构读取、模板展开、资源事务与确定性编译基础上，激活 Behavior Track 的 typed 读取、编译和绝对时间求值。阶段 1D 按 ADR 0031 实现 v2 的可选主音乐引用、严格版本路由和 Playback 内容准备；v1 的字段和未知字段拒绝语义保持不变。

在 Stage Chart Format Update 的新合同完成并通过迁移门禁以前，`cuexis.chart` v1/v2/v3 是唯一受支持的生产谱面格式族。ADR 0035 已在阶段 2A.1 删除历史 `cuexis.chart.simple` 路由、Importer 和 Schema；该格式稳定报告 `chart.format.unsupported`。内部 Runtime、World 和各 System 只接收本文模型编译后的 ChartRuntime；嵌入宿主通过 PlaybackSession、FrameSnapshot、JudgementResult 和稳定查询接口交互，不接收 ChartRuntime 或 EnTT Entity。

## 2. 顶层结构

```json
{
  "format": "cuexis.chart",
  "version": 1,
  "chartId": "019b0000-0000-7abc-8def-000000000001",
  "metadata": {},
  "timing": {
    "offsetMs": 0.0,
    "defaultBpm": 120.0,
    "bpmChanges": [],
    "stops": []
  },
  "camera": {
    "type": "perspective",
    "fovY": 45.0,
    "near": 0.1,
    "far": 1000.0,
    "pitch": 0.0,
    "yaw": 0.0,
    "roll": 0.0,
    "defaultTransform": {
      "position": [0.0, 5.0, -10.0]
    }
  },
  "templates": [],
  "behaviors": [],
  "objects": [],
  "requiredExtensions": [],
  "extensions": {}
}
```

必需字段：`format`、`version`、`chartId`、`metadata`、`timing`、`templates`、`behaviors`、`objects`、`requiredExtensions`、`extensions`。`camera` 为可选字段，省略时使用默认透视相机（fovY=60, near=0.1, far=1000, pitch/yaw/roll=0）。v1 不接受顶层 `audio`。

顶层数组顺序不具有运行语义。所有 ID 在对应域中必须唯一，编译器必须产生与输入数组顺序无关的确定性结果。

## 2a. v2 主音乐

v2 保留全部 v1 结构，并允许一个可选 typed `audio` block：

```json
"audio": {
  "version": 1,
  "mainMusic": {
    "domain": "asset",
    "id": "audio.main"
  }
}
```

省略 `audio` 表示明确没有主音乐，只能选择 ChartClock。存在 block 时 `version` 和
`mainMusic` 都是必需字段，`domain` 必须为 `asset`；引用必须解析到 Asset Index v2 的 Audio
叶节点，只能选择 HostClock 或 CuexisAudio。内容解析使用 Required 策略，任何模式失败都不得
静默切换。`timing.offsetMs` 仍是 Beat 0 相对 source time 的唯一谱面 offset，设备配置、输出
延迟和用户校准不得写入 `audio`。

Reader 必须先按显式 `version` 路由，再使用对应字段表。v1 不接受 `audio`，v2 不得被伪装成
v1；loader 不自动迁移或写回。历史 `cuexis.chart.simple` 只有 v1，且已在阶段 2A.1 移除。只有 Chart 文件、没有 Project
和 Asset Index 的 `--chart` 入口遇到带主音乐的 v2 Chart 时必须明确失败。

## 2b. v3 Tempo Event

v3 保留 v2 的全部字段和主音乐语义，但将 `timing.bpmChanges` 替换为 `timing.tempoEvents`。
v3 不接受 `bpmChanges`；Reader 必须按显式顶层版本路由，不能把旧字段自动解释为新事件。

```json
"timing": {
  "offsetMs": 0.0,
  "defaultBpm": 120.0,
  "tempoEvents": [
    {
      "startBeat": { "numerator": 16, "denominator": 1 },
      "durationBeats": { "numerator": 4, "denominator": 1 },
      "startBpm": 120.0,
      "endBpm": 180.0,
      "startSlope": 0.0,
      "endSlope": 0.0
    }
  ],
  "stops": []
}
```

`defaultBpm` 和所有 `startBpm`/`endBpm` 必须位于 `[1.0, 65536.0]`。BPM 在事件区间内直接插值；不对 milliseconds-per-beat 插值。归一化进度和 BPM 值为：

```text
u = (beat - startBeat) / durationBeats
h(u) = (-2 + startSlope + endSlope) * u^3
     + ( 3 - 2 * startSlope - endSlope) * u^2
     + startSlope * u
bpm(beat) = startBpm + (endBpm - startBpm) * h(u)
```

`startSlope` 和 `endSlope` 是归一化进度曲线的端点斜率，必须有限、非负，并满足 `startSlope + endSlope <= 3`。`durationBeats` 必须非负；它允许为零，但此时起止 BPM 必须相同且两个斜率都为零。最早事件前使用 `defaultBpm`。非零事件的有效区间为 `[startBeat, startBeat + durationBeats)`；事件开始时应用 `startBpm`，允许它与此前有效 BPM 不同并产生跳变；无后继事件时，结束边界及其后保持 `endBpm`，相邻事件在同一边界由后一个事件接管。零持续事件同样可以把此前基准跳变为其相等的起止 BPM。Tempo Event 不得重叠，同一 Beat 的多个 Tempo Event 也是错误。冲突检测把零持续事件视为占用其 `startBeat`：它不能落在非零事件内部，可以位于前一事件的结束边界，但该 Beat 不能再有另一事件开始。事件输入顺序无语义，按 `startBeat` 排序。负 Beat 事件参与 Beat 0 的实际 BPM 基准。

零持续约束中的 BPM 相等按解析后的有限数值精确比较，不使用容差。

`stops` 继续使用 v2 的 `beat` 与 `durationMs` 结构。Stop 区间内 Beat 固定，Tempo Event 和 Behavior Event 的进度均不推进；Stop 结束后从该 Beat 继续使用有效 BPM。

v3 的 `timing` 必须包含 `offsetMs`、`defaultBpm`、`tempoEvents` 和 `stops`，不得同时包含 `bpmChanges`。v1/v2 到 v3 必须通过显式迁移，迁移工具必须保留原始数据和诊断信息。

v3 顶层最小骨架如下；`audio` 仍为可选字段，其结构沿用第 2a 节：

```json
{
  "format": "cuexis.chart",
  "version": 3,
  "chartId": "019b0000-0000-7abc-8def-000000000001",
  "metadata": {},
  "timing": {
    "offsetMs": 0.0,
    "defaultBpm": 120.0,
    "tempoEvents": [],
    "stops": []
  },
  "templates": [],
  "behaviors": [],
  "objects": [],
  "requiredExtensions": [],
  "extensions": {}
}
```

## 3. Beat

方案 A 使用规范化有理数表示拍数：

```json
{
  "numerator": 7,
  "denominator": 4
}
```

规则：

```text
numerator 是有符号 64 位整数
denominator 是大于 0 的整数
保存时以最大公约数约分
零统一保存为 0/1
等价但未约分的输入可以迁移，但规范保存必须约分
```

阶段 1 历史方案 B 的十进制 beat 必须根据 JSON 原始十进制文本转换，不能先转成二进制浮点再猜测分数；该转换只用于移除前的一次性迁移。

## 4. Timing（v1/v2）

```json
{
  "offsetMs": 0.0,
  "defaultBpm": 120.0,
  "bpmChanges": [],
  "stops": []
}
```

`defaultBpm` 必须为有限正数。阶段 1A 已实现 `offsetMs` 与 `defaultBpm`；`bpmChanges` 和 `stops` 当前必须为空，非空时产生不支持诊断并拒绝加载，不得静默忽略。

v1/v2 只实现固定 BPM 与 offset，并拒绝非空 `bpmChanges`/`stops`；兼容行为见 `docs/TIMING_MODEL.md`。v3 启用新的 `tempoEvents`、Stop 和逆映射，正式字段和边界规则见第 2b 节。

Timing 不包含 `speedChanges`。Entity 移动速度由 Behavior 表达，未来整曲播放倍率由 AudioTransport 与 Timeline 表达。

## 4a. Camera

拍摄影机定义了谱面的默认观察视角。省略时使用透视默认值。

```json
{
  "type": "perspective",
  "fovY": 45.0,
  "near": 0.1,
  "far": 1000.0,
  "pitch": -15.0,
  "yaw": 0.0,
  "roll": 0.0,
  "defaultTransform": {
    "position": [0.0, 5.0, -10.0]
  }
}
```

| 字段 | 必需 | 说明 |
|---|---|---|
| `type` | 是 | v1 仅 `"perspective"` |
| `fovY` | 是 | 垂直视场角（度），范围 (0, 179) |
| `near` | 是 | 近裁剪面距离，必须 > 0 且 < far |
| `far` | 是 | 远裁剪面距离，必须 > 0 且 > near |
| `pitch` | 否 | 俯仰角（度），绕 X 轴，默认 0 |
| `yaw` | 否 | 偏航角（度），绕 Y 轴，默认 0 |
| `roll` | 否 | 翻滚角（度），绕 Z 轴，默认 0 |
| `defaultTransform` | 否 | 初始世界坐标位置，不含旋转（旋转由 pitch/yaw/roll 决定） |
| `defaultTransform.position` | 是（若提供了 defaultTransform） | `[x, y, z]` 世界坐标 |

Pitch/Yaw/Roll 按 Tait–Bryan (ZYX) 顺序合成旋转：Roll → Pitch → Yaw。负 Pitch 使相机向下看，正 Yaw 向右转。`aspectRatio` 不存入谱面，由运行时根据实际视口实时计算。

v1/v2 的历史事件结构仅用于兼容迁移，不能写入 v3：

```json
{
  "bpmChanges": [
    {
      "beat": { "numerator": 16, "denominator": 1 },
      "bpm": 180.0
    }
  ],
  "stops": [
    {
      "beat": { "numerator": 32, "denominator": 1 },
      "durationMs": 250.0
    }
  ]
}
```

## 5. Reference

方案 A 引用必须是结构化对象：

```json
{
  "domain": "object",
  "id": "019b0000-0000-7abc-8def-000000000010"
}
```

v1 支持：

```text
object    当前 Chart 中原生 UUIDv7 或历史迁移后保留 UUIDv5 的 Object
template  当前 Chart 中原生 UUIDv7 或历史迁移后保留 UUIDv5 的 Template
behavior  当前 Chart 中的 Behavior ID
asset     AssetDatabase 中的 AssetId
```

字段必须限制允许的 domain，例如 `parent` 只接受 `object`。方案 A 不允许字符串简写引用。

原生创建和保存的 canonical Object/Template ID 使用 UUIDv7；阶段 1 已转换为 canonical 的历史 Simple 结果可能使用 UUIDv5。Canonical loader 接受这两种明确来源的版本不等于继续支持 Simple Parser；`chartId` 本身始终要求 UUIDv7。

`external-chart` 是保留域，v1 使用时返回 `UnsupportedFeature`。v1 不加载其他 Chart 中的 Object 或 Template。

## 6. Object

```json
{
  "id": "019b0000-0000-7abc-8def-000000000010",
  "name": "intro_note",
  "parent": null,
  "components": {
    "cuexis.transform": {
      "version": 1,
      "position": [0.0, 1.0, 0.0],
      "rotation": [0.0, 0.0, 0.0, 1.0],
      "scale": [1.0, 1.0, 1.0]
    },
    "cuexis.renderable": {
      "version": 1,
      "mesh": { "domain": "asset", "id": "mesh.note.standard" },
      "material": { "domain": "asset", "id": "material.note.standard" }
    },
    "cuexis.note": {
      "version": 1,
      "beat": { "numerator": 4, "denominator": 1 }
    }
  },
  "extensions": {}
}
```

Object 字段：

```text
id          必需，原生 ChartObjectId UUIDv7 或历史 canonical UUIDv5
name        可选，仅编辑和诊断使用
parent      必需，null 或 object 引用
components  必需，Component ID 到 Component 数据的对象映射
extensions  必需，扩展数据对象
```

所有谱面语义对象都进入同一个 `objects` 数组。Note、Element 和装饰物由 Component 组合区分，不建立平行的 `notes`、`elements` 或 `decorations` 数组。

Component ID 是稳定字符串，不绑定 C++ 类名。每个 Component 数据必须包含自己的正整数 `version`。同一 Object 中每个 Component ID 最多出现一次。

`cuexis.note` v1 必须包含 `beat`，表示判定语义的谱面拍数。音符的空间移动仍由 Behavior 表达，不能从 note beat 隐式推导移动速度。

`cuexis.transform` 的 Quaternion 顺序固定为 `[x, y, z, w]`；position 使用米，scale 是无量纲分量倍率。

`cuexis.renderable` 的结构和 asset 引用在阶段 1A 可以被解析和编译，但 `ChartWorldInstantiator` 在资源生命周期尚未建立时明确返回 `runtime.chart.renderable_resources_unsupported`。阶段 1A 的 A/B 可运行示例因此不携带外部 Renderable，而是由 DebugDraw 根据 Transform 输出 XYZ 轴线；阶段 1B 才能把 Mesh/Material 资源实例化为可渲染组件。

`cuexis.camera` v1 将对象标记为相机实体。相机对象可通过 `cuexis.behavior` 绑定 behavior 事件，在播放期间驱动位置/旋转/FOV 变化。具体结构见第 8a 节。

`parent` 在 Component 编译前解析。缺失父引用、循环和子树跳过遵循 ADR 0008。

## 7. Template

v1 Template 只描述单个 Object 原型，不生成子树，也不提供默认 parent。

根模板包含 `extends: null` 和 `prototype`，不包含有实际操作的 `patch`。派生模板包含非空 `extends` 和 `patch`，不包含 `prototype`。两种结构互斥，避免同时合并 prototype 与父模板时产生隐式顺序。

```json
{
  "id": "019b0000-0000-7abc-8def-000000000020",
  "name": "standard_note",
  "extends": null,
  "prototype": {
    "components": {
      "cuexis.transform": {
        "version": 1,
        "position": [0.0, 0.0, 0.0],
        "rotation": [0.0, 0.0, 0.0, 1.0],
        "scale": [1.0, 1.0, 1.0]
      },
      "cuexis.note": { "version": 1 }
    }
  },
  "extensions": {}
}
```

派生模板示例：

```json
{
  "id": "019b0000-0000-7abc-8def-000000000022",
  "name": "red_note",
  "extends": {
    "domain": "template",
    "id": "019b0000-0000-7abc-8def-000000000020"
  },
  "patch": [
    {
      "op": "replace",
      "path": "/components/cuexis.renderable/material",
      "value": { "domain": "asset", "id": "material.note.red" }
    }
  ],
  "extensions": {}
}
```

Template 实例不直接携带 `components`，而是通过模板引用和 overrides 得到：

```json
{
  "id": "019b0000-0000-7abc-8def-000000000021",
  "name": "note_instance",
  "parent": null,
  "template": {
    "domain": "template",
    "id": "019b0000-0000-7abc-8def-000000000020"
  },
  "overrides": [
    {
      "op": "replace",
      "path": "/components/cuexis.transform/position",
      "value": [1.0, 0.0, 0.0]
    }
  ],
  "extensions": {}
}
```

直接 Object 必须包含 `components` 且不包含 `template`、`overrides`。Template 实例必须包含 `template`、`overrides` 且不包含 `components`。

展开顺序：

```text
父模板完整展开
-> 当前模板 patch
-> 实例 overrides
-> Component typed 结构校验
-> 语义校验
```

Patch v1 只支持 JSON Patch 的 `add`、`remove` 和 `replace`。禁止继承环。数组字段应整体替换，v1 不建议依赖数组下标执行 Patch。

## 8. Behavior

谱面文档中的 Behavior Key 使用 Beat。`behavior.transform.keyframe` version 1 已由阶段 1C 激活；Reader、Schema 和 Compiler 使用同一白名单，非法 Track 不进入 Runtime。Track 输入顺序没有语义，编译器按 Beat 稳定排序并一次性转换为 `chartTimeMs`，offset 不重复计入。

```json
{
  "id": "behavior.note.intro",
  "type": "behavior.transform.keyframe",
  "version": 1,
  "tracks": [
    {
      "property": "transform.position.x",
      "keys": [
        {
          "beat": { "numerator": 0, "denominator": 1 },
          "value": 0.0
        },
        {
          "beat": { "numerator": 1, "denominator": 1 },
          "value": 10.0
        }
      ]
    }
  ]
}
```

同一求值层属性冲突和 Animation 混合遵循 ADR 0009。

v1 允许的 Property 为 `transform.position.x/y/z`、`transform.rotation`、`transform.scale` 和 `camera.fovY`。标量、Vec3 使用分量线性插值；Quaternion 使用 shortest-path slerp 并重新归一化；首尾时间钳制，单 Key 为常量。目标 Key 的 easing 只控制前一 Key 到当前 Key 的区间，支持 `linear`、`in_cubic`、`out_cubic`、`in_out_cubic`。`camera.fovY` 的值必须严格位于 (0, 179)，且绑定对象必须有 `cuexis.camera`。

空 Track、重复 Beat、同一 Behavior 重复 Property、未知字段/easing、非有限值、非规范 Quaternion 和越界 FOV 都是错误。阶段 2A 前的历史 Simple v1 只提供标量位置/FOV Track，并在 Behavior、Track、Key 子树对未知字段严格报错；这些规则仅用于迁移旧输入，不进入 v3。

v1 默认安全预算为每 Behavior 6 条 Track、每 Track 65536 个 Key；v3 默认安全预算为每 Behavior 65536 个 Event、每 Chart 262144 个 Event；两者共用每帧 600000 个 Property Write 和 1024 条诊断上限。`behavior.transform.keyframe` version 1 保持兼容；阶段 2 已选择 Behavior Event 作为新 Behavior 的谱面层表达，具体字段、版本和迁移规则见 `docs/stage_plans/stage_2_implementation_plan.md`。

### 8a. v3 Behavior Event

v3 使用 `behavior.event` version 1。一个 Behavior 包含多个属性事件；编译器先按属性分组，再按 `startBeat` 排序，数组顺序没有运行语义。v3 的 Behavior 使用 Chart 全局 Beat，不在对象绑定中重复声明时间偏移。

```json
{
  "id": "behavior.note.intro",
  "type": "behavior.event",
  "version": 1,
  "events": [
    {
      "property": "transform.position.x",
      "groupId": "intro.move",
      "startBeat": { "numerator": 16, "denominator": 1 },
      "durationBeats": { "numerator": 4, "denominator": 1 },
      "startValue": 0.0,
      "endValue": 10.0,
      "startSlope": 0.0,
      "endSlope": 0.0
    },
    {
      "property": "transform.position.y",
      "groupId": "intro.move",
      "startBeat": { "numerator": 16, "denominator": 1 },
      "durationBeats": { "numerator": 4, "denominator": 1 },
      "startValue": 2.0,
      "endValue": 5.0,
      "startSlope": 0.0,
      "endSlope": 0.0
    }
  ],
  "stepEvents": [
    {
      "property": "render.visible",
      "beat": { "numerator": 20, "denominator": 1 },
      "value": false
    }
  ]
}
```

连续事件字段：

```text
property       属性 ID
groupId        可选的同组 ID；同组事件必须具有相同 startBeat 和 durationBeats
startBeat      事件开始 Beat，可以为负数
durationBeats  非负持续拍数
startValue     事件开始值
endValue       事件结束值
startSlope     归一化进度的起始斜率
endSlope       归一化进度的结束斜率
```

事件外保持对象初始基准或前一事件终值；事件开始时应用 `startValue`，允许它与当前基准不同；非零事件的有效区间为 `[startBeat, startBeat + durationBeats)`，区间内使用上述 Hermite 进度 `h(u)` 插值；标量/Vec3 使用分量插值，Quaternion 使用 shortest-path slerp，`h(u)` 只作为 slerp 进度。无后继事件时，结束边界及其后保持 `endValue`，相邻事件在同一边界由后一个事件接管，因此允许跳变。零持续事件在精确 `startBeat` 处应用其值，并作为后续基准直到下一个事件。同一属性的事件不得重叠，也不得具有相同 `startBeat`；冲突检测把零持续事件视为占用其 Beat，因此它不能位于同属性非零事件内部，可以位于前一事件的结束边界，但该 Beat 不能再有同属性事件开始。事件输入顺序无语义。

`durationBeats = 0` 是合法的瞬时事件，但必须满足 `startValue == endValue`、`startSlope == 0` 和 `endSlope == 0`。标量和 Vec3 按分量插值；Quaternion 使用 shortest-path slerp，Hermite 结果只作为 slerp 进度；FOV 必须位于 `(0, 179)`。所有数值必须有限，Quaternion 必须可归一化。`startSlope`/`endSlope` 必须有限、非负，并满足 `startSlope + endSlope <= 3`。

零持续约束中的值相等按解析后的 typed 值精确比较；Vec3 和 Quaternion 逐分量比较，因此符号相反但表示同一旋转的两个 Quaternion 不满足该约束。

`groupId` 的作用域是单个 Behavior，必须匹配 portable ASCII 模式 `[A-Za-z0-9][A-Za-z0-9._-]{0,255}`；同一 ID 在该 Behavior 内只表示一个事件组。它声明多个属性共享事件边界并接受一致性校验；同组事件的开始 Beat 和持续时间必须完全相同。缺少 `groupId` 的事件彼此独立。整帧提交本身始终是事务式的，`groupId` 不改变未分组事件的提交原子性。任何属性冲突、重叠、同属性相同起始 Beat、类型不匹配或同组字段不一致都是格式错误。

v3 连续属性白名单为 `transform.position.x/y/z`、`transform.rotation`、`transform.scale`、
`camera.fovY`、`material.opacity` 和 `material.tint`。`material.opacity` 的范围为 `[0,1]`，
初始基准为 `1.0`；`material.tint` 是各分量位于 `[0,1]` 的 Vec3，初始基准为
`[1,1,1]`。

Step Event 使用 `property`、`beat`、typed `value` 和可选 `groupId`。事件在 `beat` 处含边界
生效并保持到下一事件，首事件前保持对象初始基准。同一 Property 的 Step Event 不得位于
同一 Beat；输入按 Property 和 Beat 排序。支持的离散属性仅为：

```text
render.visible   Boolean；初始基准 true
render.material  Asset reference；初始基准 cuexis.renderable.material
```

`groupId` 与连续 Event 共享单 Behavior 作用域和字符规则；同组 Step Event 必须具有完全相同
的 Beat。连续 Event 与 Step Event 不能共用同一 `groupId`。`behavior.event` 必须同时包含
`events` 与 `stepEvents`；两个数组可以为空，但两者不能同时为空。ParentBinding 不属于 v3，
不得用连续插值或未声明的 Step Property 近似。

### 8b. v3 Behavior 绑定

v3 暂时沿用 v1 的对象绑定结构，事件使用 Chart 全局 Beat：

```json
"cuexis.behavior": {
  "version": 1,
  "behavior": { "domain": "behavior", "id": "behavior.note.intro" }
}
```

一个对象最多绑定一个 Behavior。缺少目标属性组件、相机组件或行为引用时，Chart 在 prepare 阶段失败。局部 Beat、循环和多 Clip 混合不属于 v3 首版格式；需要这些能力时必须引入新的 Behavior binding version，不得改变 v3 的全局 Beat 语义。

### 8c. v1 到 v3 迁移

`behavior.transform.keyframe` version 1 不在 v3 中隐式解释为 Event。迁移工具按每个 Track 的相邻 Key 生成连续事件：前一 Key 为 `startValue`，后一 Key 为 `endValue`，Beat 差值为 `durationBeats`。`linear`、`in_cubic` 和 `out_cubic` 可分别精确映射为一个端点斜率为 `(1,1)`、`(0,3)` 和 `(3,0)` 的 Event。

v1 的分段 `in_out_cubic` 在精确 Beat 中点拆成两个语义等价的 Event：前半段使用 `(0,3)`，后半段使用 `(3,0)`，中点值为原区间 0.5 进度的值；Quaternion 使用原区间的 shortest-path slerp 中点。Beat 中点无法在有理数预算内表示，或 typed 中点值无法按迁移实现前冻结的误差预算序列化时，迁移失败，不得无界近似。v1 与迁移后 v3 的 Quaternion 浮点采样按该迁移误差预算比较，不要求 FrameDigest 位级相同。

v1 Track 在首 Key 之前已经输出首 Key 值，而 v3 Event 在首事件前保持对象基准。迁移器必须把每个已绑定对象或模板的对应初始属性改写为首 Key 值；单 Key Track 只需改写基准，不生成事件。共享 Behavior、模板实例和未绑定 Behavior 必须有确定性迁移报告，无法证明等价时失败。末 Key 之后由最后事件终值自然保持。对象初值改写或模板展开必须显式列入迁移报告，不得静默执行，也不得丢弃未绑定数据。

仓库提供显式迁移和校验工具。默认目标仍是 v3；旧调用组合继续只走 `migrateToV3`：

```powershell
cuexis_chart_migrator --input <v1-or-v2.json> --output <v3.json> --report <report.json>
cuexis_chart_migrator --input <v1-or-v2-or-v3.json> --output <v4.json> --report <report.json> --target 4
cuexis_chart_validator --input <chart.json>
```

`--target` 只接受 `3` 或 `4`。缺省或 `--target 3` 拒绝已经是 v3 的输入。`--target 4`
接受合法 v1/v2/v3：v1/v2 先复用现有 v3 迁移，再对规范化 v3 JSON 做 lift；v3 直接 lift。
v4 或其他 version 稳定失败，不写 artifact。v3 → v4 只插入空 `parameters`、
`animationTemplateImports`、`animationClips` 并提升 `version`，不生成 CXT、参数、Animator、
Clip、Binding 或脚本。

迁移器不自动写回源文件。输入、输出和报告路径必须互不冲突；迁移先写同目录临时文件，
只有 Chart 与报告都完成后才替换目标。失败时删除临时文件并恢复目标备份，因此不得留下半份
Chart 或报告。v3 报告字段与既有 Stage 2 golden 保持一致：源/目标版本、基准改写、模板展开、
生成事件数和未绑定 Behavior。v4 报告在这些计数之外增加 source/target canonical identity、
`discardedFields`、生成的 Clip/Binding/参数计数、字段计数以及稳定 diagnostics/warnings。
v2 的 `audio` block 原样迁移到 v3 再 lift 到 v4；v1 没有该字段。
FrameSnapshot / FrameDigest 等价属于 CFU-D3，已由 Playback 层测试关闭；v1 Quaternion 采样继续按
迁移误差预算比较，不要求与源 v1 的 FrameDigest 位级相同。整包 CFU-D 已由项目所有者记录
“未提供外部资产”并关闭；兼容窗口不缩短。证据见
[CFU-D 关闭报告](stage_reports/260814-chart-format-update-d-close.md)。

## 9. Extensions

```json
{
  "requiredExtensions": [
    {
      "id": "org.example.custom-note",
      "version": 1
    }
  ],
  "extensions": {
    "org.example.custom-note": {
      "version": 1,
      "data": {}
    }
  }
}
```

扩展 ID 使用所有者控制的稳定命名空间。扩展处理器未来注册到 Chart 层，负责对应版本的 validate、migrate、compile 和 diagnostics。阶段 1A 尚无扩展处理器。

未知可选扩展原样保留并警告。未知 requiredExtension 编译失败。扩展处理器不得直接创建 EnTT Entity 或调用渲染后端，只能参与 ChartDocument 校验和 ChartRuntime 编译。

## 10. Unknown Fields

```text
当前版本未知核心字段：错误
未知可选 extensions 数据：保留并警告
缺少 requiredExtension：错误
高于当前支持的 format version：不得编译
```

Studio 保存文档时必须保留自己不能识别的可选扩展数据。

## 11. Validation and Diagnostics

阶段 1A 的实际加载链为：

```text
JSON 输入大小、嵌套深度、单字符串大小、重复键与语法检查
-> Cuexis JSON Value
-> typed Reader 结构读取、未知字段与字段路径诊断
-> ID 与引用索引
-> Template 展开
-> Component 与扩展校验
-> 层级、资源和属性语义校验
-> ChartRuntime 编译
```

仓库提供 `schemas/cuexis.chart.v1.schema.json`、`schemas/cuexis.chart.v2.schema.json` 和
`schemas/cuexis.chart.v3.schema.json`、`cuexis_json_support` 的 JSON Schema adapter 及独立测试。
当前 `ChartLoader` / `CanonicalChartLoader` 尚未调用 adapter。loader 的现行结构权威是 typed
Reader，随后由 Chart 代码完成语义校验；因此不能把一次加载描述为已执行 JSON Schema
Validator。Schema artifact、typed Reader、validator 和迁移器通过共享合法/非法 fixture
保持字段集合一致；Schema 仍不能替代引用、层级、资源和属性冲突等 Cuexis 语义校验。

诊断至少包含：

```text
severity
code
以 `$` 为根的稳定字段路径
相关 Object / Template / Behavior ID
可读消息
```

不得用数组顺序、JSON 对象键顺序或 Entity 创建顺序解决冲突。

字段路径示例为 `$/objects/0/components/cuexis.transform`。路径段会进行必要转义，用于稳定定位诊断；当前契约不宣称它覆盖或严格等同于外部 JSON Pointer 标准的全部语法。

默认解析和编译预算包括：输入 16 MiB、嵌套深度 64、单个 JSON object key 或 string value 1 MiB、Template 10000、Behavior 10000、Object 100000、每个 Template Patch 256、扩展 256、诊断 1024。调用方可以传入更严格的 `ChartLimits`，但不得通过放宽预算改变格式语义。

## 12. v1 明确不支持

以下内容不属于现行 v1/v2/v3，使用时返回 UnsupportedFeature 或 Schema Error。CXT import 和
Template Binding 只属于尚未生产化的 Chart v4 候选；未来加入必须提升格式/组件版本并形成 ADR：

```text
跨 Chart Object / Template 引用
Template 子树实例化
扩展处理器动态插件 ABI
二进制 ChartRuntime 缓存格式
```

## 13. Future versions

本文只定义已实现的 Chart v1/v2/v3。Chart v4 候选字段已移至独立的
[CHART_V4_FORMAT.md](CHART_V4_FORMAT.md)，避免把生产格式和未实现提案混为同一规范。

ADR 0038 整体接受以前，Loader 不得按字段猜测 v4、把未知 CXC 输入降级为 v3，或在 extensions
中加入动画字段绕过版本、迁移和 capability 评审。v1/v2/v3 Reader、Schema、迁移、诊断和可观察
结果保持不变。
