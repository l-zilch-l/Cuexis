# Chart v4 谱面编写指南

状态：现行 v4 手写指南

更新日期：2026-09-02

本指南面向需要直接手写 `cuexis.chart` v4 JSON 的作者。它把常用结构和写作顺序集中在一起，
但不替代字段权威：[CHART_V4_FORMAT.md](../formats/CHART_V4_FORMAT.md)。v4 是当前已接受并实现的
谱面格式；Chart v5 仍在独立阶段计划中，不能把 v5 的 Euler 旋转或 alpha 语义写入 v4。

## 1. 先记住四条规则

1. v4 是严格 JSON：不能写注释、尾逗号或未声明的核心字段。
2. 所有 Beat 都写成有理数对象，例如 `{"numerator": 3, "denominator": 2}`；保存时应约分。
3. 引用都写成带 `domain` 和 `id` 的对象，不能写字符串简写。
4. 先写静态 Object，再添加 Behavior、Animation、Template 和 CXT；每增加一层都运行校验器。

v4 的数组输入顺序没有运行语义。人工编写时仍建议按 ID、Beat 和属性排序，便于审阅和稳定 diff。

## 2. 可直接复制的最小骨架

下面的文件是一个静态 v4 Chart。它包含一个带 Transform 和 Note 的 Object，不依赖外部资源：

```json
{
  "format": "cuexis.chart",
  "version": 4,
  "chartId": "019f0000-0000-7abc-8def-000000000400",
  "metadata": {
    "title": "My Chart"
  },
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
  "objects": [
    {
      "id": "019f0000-0000-7abc-8def-000000000401",
      "name": "first_note",
      "parent": null,
      "components": {
        "cuexis.transform": {
          "version": 1,
          "position": [0.0, 0.0, 0.0],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.0, 1.0, 1.0]
        },
        "cuexis.note": {
          "version": 1,
          "beat": { "numerator": 4, "denominator": 1 }
        }
      },
      "extensions": {}
    }
  ],
  "requiredExtensions": [],
  "extensions": {}
}
```

顶层必需字段是：`format`、`version`、`chartId`、`metadata`、`timing`、`parameters`、`templates`、
`behaviors`、`animationTemplateImports`、`animationClips`、`objects`、`requiredExtensions` 和
`extensions`。`camera` 与 `audio` 可以省略；其余新增数组即使为空也必须保留。

`chartId` 使用 UUIDv7。手写新文件时，Object 和 Template ID 也使用 UUIDv7；不要复用同一个 ID。

## 3. 顶层字段速查

| 字段 | 写法和用途 |
| --- | --- |
| `format` | 固定为 `"cuexis.chart"` |
| `version` | 版本号，填写 `4` |
| `chartId` | 当前 Chart 的 UUIDv7，必须唯一 |
| `metadata` | 编辑和诊断信息；不用于时间求值 |
| `timing` | 全局 Offset、BPM、Tempo Event 和 Stop |
| `camera` | 默认透视相机；可省略并使用默认值 |
| `audio` | v2/v3 兼容的可选主音乐引用 |
| `parameters` | prepare 前由宿主提供的参数声明 |
| `templates` | Object 原型和派生模板 |
| `behaviors` | 使用 Chart 全局 Beat 的事件集合 |
| `animationTemplateImports` | 外部 `.cxt` 模板清单 |
| `animationClips` | Chart 内定义的可复用局部动画 |
| `objects` | 所有实体，包括 Note、Element、装饰物和相机实体 |
| `requiredExtensions` / `extensions` | 扩展能力声明和数据 |

## 4. 时间：Beat、BPM、Tempo 和 Stop

### 4.1 Beat

所有 Beat 使用以下结构：

```json
{ "numerator": 7, "denominator": 4 }
```

- `numerator` 是有符号整数，因此负 Beat 合法。
- `denominator` 必须大于 0。
- `0` 规范写成 `0/1`。
- `6/4` 应写成 `3/2`。
- Clip 内的局部 Beat 不得为负；Behavior 和实例的 Chart Beat 可以为负。

### 4.2 Timing

```json
"timing": {
  "offsetMs": -25.0,
  "defaultBpm": 120.0,
  "tempoEvents": [],
  "stops": []
}
```

`defaultBpm` 以及 Tempo Event 的起止 BPM 必须在 `[1, 65536]`。`offsetMs` 必须有限。
Tempo Event 不得重叠，同一 Beat 不能有两个事件；`startSlope` 和 `endSlope` 必须在 `[0,3]`，
且两者之和不超过 `3`。Stop 的 `durationMs` 必须大于 `0`。

Stop 区间内 Beat 固定，Tempo、Behavior 和动画进度都不推进；Stop 结束后从同一 Beat 继续。

## 5. 相机

### 5.1 默认相机

```json
"camera": {
  "type": "perspective",
  "fovY": 60.0,
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

- `type` 当前只能是 `perspective`。
- `fovY` 是垂直视场角，v4 严格位于 `(0,179)`；`0` 和 `179` 都拒绝。
- `near`、`far` 都必须大于 `0`，并且语义上满足 `near < far`。
- `pitch`、`yaw`、`roll` 是度数，默认值都是 `0`。
- v4 按 Tait-Bryan ZYX 顺序合成：Roll -> Pitch -> Yaw。负 Pitch 表示相机向下看，正 Yaw 表示向右转。
- `defaultTransform` 只保存初始世界位置，不在其中重复写旋转。
- `aspectRatio` 不写入 Chart，由运行时根据实际视口计算。

### 5.2 相机实体

如果需要让 Behavior 驱动相机，把 Object 标记为相机实体：

```json
"cuexis.camera": {
  "version": 1,
  "type": "perspective",
  "fovY": 60.0,
  "near": 0.1,
  "far": 1000.0
}
```

相机实体通常同时拥有 `cuexis.transform`。它的旋转由 Transform 的 Quaternion 提供，FOV 由相机组件
或绑定到该对象的 Behavior 提供。驱动 `camera.fovY` 的 Behavior 必须绑定到带 `cuexis.camera` 的 Object。

## 6. Object、Parent 和 Components

### 6.1 直接 Object

直接 Object 必须有 `components`，不能同时写 `template` 或 `overrides`：

```json
{
  "id": "019f0000-0000-7abc-8def-000000000410",
  "name": "child_note",
  "parent": {
    "domain": "object",
    "id": "019f0000-0000-7abc-8def-000000000400"
  },
  "components": {
    "cuexis.transform": {
      "version": 1,
      "position": [0.0, 1.0, 0.0],
      "rotation": [0.0, 0.0, 0.0, 1.0],
      "scale": [1.0, 1.0, 1.0]
    }
  },
  "extensions": {}
}
```

`parent: null` 表示根 Object。子 Object 的 `parent` 必须引用同一 Chart 的另一个 Object，不能引用
Template、Behavior 或 Asset。父引用必须存在，层级不能成环；父关系在 Component 编译前解析。

Parent 只建立实体层级，不会自动创建子树，也不会让父对象继承子对象的组件。每个实体需要的组件都要
在它自己展开后的 Object 中存在。

### 6.2 Template Object

Template 实例不写 `components`，而是写 `template` 和 `overrides`：

```json
{
  "id": "019f0000-0000-7abc-8def-000000000411",
  "name": "templated_note",
  "parent": null,
  "template": {
    "domain": "template",
    "id": "019f0000-0000-7abc-8def-000000000420"
  },
  "overrides": [
    {
      "op": "replace",
      "path": "/components/cuexis.transform/position",
      "value": [2.0, 0.0, 0.0]
    }
  ],
  "extensions": {}
}
```

展开顺序固定为：

```text
父模板完整展开 -> 当前模板 patch -> 实例 overrides -> typed Component 校验 -> 语义校验
```

Patch 只支持 `add`、`remove`、`replace`。常用路径包括：

```text
/components/cuexis.transform/position
/components/cuexis.transform/rotation
/components/cuexis.transform/scale
/components/cuexis.renderable/mesh
/components/cuexis.renderable/material
/components/cuexis.behavior/behavior
/components/cuexis.note/beat
/components/cuexis.camera/fovY
/components/cuexis.animator
```

不要用数组下标修改 Animator 内部数据；v4 对 Animator 只允许整体替换 `/components/cuexis.animator`。

### 6.3 Template 定义

根 Template 使用 `extends: null` 和 `prototype`：

```json
{
  "id": "019f0000-0000-7abc-8def-000000000420",
  "name": "note_base",
  "extends": null,
  "prototype": {
    "components": {
      "cuexis.transform": {
        "version": 1,
        "position": [0.0, 0.0, 0.0],
        "rotation": [0.0, 0.0, 0.0, 1.0],
        "scale": [1.0, 1.0, 1.0]
      },
      "cuexis.note": {
        "version": 1,
        "beat": { "numerator": 4, "denominator": 1 }
      }
    }
  },
  "extensions": {}
}
```

派生 Template 改用非空 `extends` 和 `patch`，不能同时写 `prototype`。Template 本身不提供默认
`parent`；parent 永远由具体 Object 实例声明。

### 6.4 Components 速查

| Component | 必需字段 | 作用 |
| --- | --- | --- |
| `cuexis.transform` | `version`、`position`、`rotation`、`scale` | 位置、姿态和缩放 |
| `cuexis.renderable` | `version`、`mesh`、`material` | 引用 Mesh 与 Material Asset |
| `cuexis.behavior` | `version`、`behavior` | 将一个 Behavior 绑定到 Object |
| `cuexis.note` | `version`、`beat` | 判定用音符拍点 |
| `cuexis.element` | `version` | 标记 Element 实体 |
| `cuexis.camera` | `version`、`type`、`fovY`、`near`、`far` | 标记相机实体 |
| `cuexis.animator` | `version`、`templateBindings`、`layers` | 局部 Clip、Layer 和 CXT Binding |

所有 Component 的 `version` 都写 `1`。同一个 Object 中每种 Component 最多一个。

`cuexis.transform` 的单位和格式：

- `position` 使用米，顺序为 `[x, y, z]`。
- `rotation` 是有限、可归一化 Quaternion，顺序为 `[x, y, z, w]`；恒等旋转是 `[0,0,0,1]`。
- `scale` 是无量纲倍率，顺序为 `[x, y, z]`。

`cuexis.renderable` 的 `mesh` 和 `material` 都是 `asset` 引用；资源必须存在于 Project/CXC 闭包，
并提供 v4 要求的 portable presentation payload。

## 7. 引用写法

v4 不接受字符串简写：

```json
{ "domain": "object", "id": "019f0000-0000-7abc-8def-000000000400" }
{ "domain": "template", "id": "019f0000-0000-7abc-8def-000000000420" }
{ "domain": "behavior", "id": "behavior.note.intro" }
{ "domain": "asset", "id": "mesh.note.standard" }
```

字段决定允许的 domain：`parent` 只能是 `object`，Template 实例只能引用 `template`，Behavior
Component 只能引用 `behavior`，Renderable 和 Audio 只能引用 `asset`。不要把同一个文本 ID 当作不同
domain 的引用；语义域不同，解析表也不同。

## 8. Behavior：用全局 Beat 驱动物体

### 8.1 Behavior 定义

v4 使用 `behavior.event` version 1。Behavior 使用 Chart 全局 Beat，不在 Object 绑定中重复写开始时间：

```json
{
  "id": "behavior.note.intro",
  "type": "behavior.event",
  "version": 1,
  "events": [
    {
      "property": "transform.position.x",
      "startBeat": { "numerator": 0, "denominator": 1 },
      "durationBeats": { "numerator": 4, "denominator": 1 },
      "startValue": 0.0,
      "endValue": 4.0,
      "startSlope": 0.0,
      "endSlope": 0.0
    }
  ],
  "stepEvents": []
}
```

`events` 和 `stepEvents` 两个数组都必须存在，但不能同时为空。

### 8.2 连续属性

v4 连续属性白名单：

| Property | 值类型 | 目标 Component |
| --- | --- | --- |
| `transform.position.x/y/z` | finite number | `cuexis.transform` |
| `transform.rotation` | Quaternion `[x,y,z,w]` | `cuexis.transform` |
| `transform.scale` | finite Vec3 | `cuexis.transform` |
| `camera.fovY` | number in `(0,179)` | `cuexis.camera` |
| `material.opacity` | number in `[0,1]` | `cuexis.renderable` |
| `material.tint` | Vec3，每分量 `[0,1]` | `cuexis.renderable` |

每个连续事件包含：`property`、`startBeat`、`durationBeats`、`startValue`、`endValue`、
`startSlope` 和 `endSlope`。斜率必须在 `[0,3]`，总和不超过 `3`。

事件区间是 `[startBeat, startBeat + durationBeats)`。事件前保持 Object 初始值，事件后保持最后一个
事件的 `endValue`。同一属性的事件不得重叠，也不能拥有相同 `startBeat`。零持续事件用于在一个精确
Beat 设置值，且必须满足 `startValue == endValue`、两个 slope 都为 `0`。

### 8.3 离散属性

```json
"stepEvents": [
  {
    "property": "render.visible",
    "beat": { "numerator": 8, "denominator": 1 },
    "value": false
  },
  {
    "property": "render.material",
    "beat": { "numerator": 12, "denominator": 1 },
    "value": { "domain": "asset", "id": "material.note.red" }
  }
]
```

只支持 `render.visible`（Boolean）和 `render.material`（Asset reference）。Step 在指定 Beat 生效，
并保持到下一个 Step；同一属性不能在同一 Beat 有两个 Step。

### 8.4 绑定 Behavior

```json
"cuexis.behavior": {
  "version": 1,
  "behavior": {
    "domain": "behavior",
    "id": "behavior.note.intro"
  }
}
```

一个 Object 最多绑定一个 Behavior。Behavior 写入的每个属性都必须有对应 Component；例如，写入
`camera.fovY` 的 Object 必须有 `cuexis.camera`，写入 `material.opacity` 的 Object 必须有
`cuexis.renderable`。

### 8.5 v4 旋转限制

v4 作者层旋转不是 Euler 度数，而是 Quaternion。采样时使用 shortest-path slerp：

- 单个 Quaternion 事件表达的是两种姿态之间的最短弧，不能表达明确的累计圈数。
- 一段旋转最多可靠表达 `180` 度，超过这个范围不会自动沿作者预期方向绕行。
- 不能用一个 `0 -> 720` 的“角度事件”表达四圈旋转；v4 没有这样的角度字段。
- 如果必须在 v4 中近似较大转动，只能拆成多个经过验证的姿态段，每段都使用合法 Quaternion；这仍不
  等价于保存了未展开角度，且相反 Quaternion 的符号和恰好 `180` 度的情况需要额外测试。

不要把 v5 计划中的 Euler XYZ、负角度、多圈角度或 `[0,255]` alpha 写入 v4 文件。

## 9. AnimationClip 和 Animator

当多个 Object 需要复用局部动画时，使用 `animationClips` + `cuexis.animator`；单个全局对象的简单运动
通常直接使用 Behavior 更易读。

### 9.1 Clip

Clip 的时间是局部有理 Beat，不绑定 Object：

```json
{
  "id": "animation.note.pulse",
  "version": 1,
  "durationBeats": { "numerator": 4, "denominator": 1 },
  "tracks": [
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
  ],
  "stepTracks": []
}
```

Clip Continuous Track 支持 `transform.position.x/y/z`、`transform.rotation`、`transform.scale`、
`material.opacity` 和 `material.tint`。Step Track 只支持 `render.visible` 与 `render.material`。
Clip 必须至少有一个非空 Track；同一 Clip 对同一属性最多一个 Track。

间隙保持前一段的结束值，最后一段结束后保持到 Clip 结束。Clip 内局部 Beat 不得为负，重复
`startBeat` 或重复 Step Beat 都是错误。

### 9.2 Animator 层级

```text
cuexis.animator
  -> layers[]
      -> blendGroups[]
          -> instances[]
```

Layer 示例：

```json
{
  "version": 1,
  "templateBindings": [],
  "layers": [
    {
      "layerId": "layer.pulse",
      "priority": 10,
      "weight": 1.0,
      "propertyMask": {
        "properties": ["transform.scale"],
        "prefixes": []
      },
      "blendGroups": [
        {
          "groupId": "group.pulse",
          "mode": "override",
          "weight": 1.0,
          "instances": [
            {
              "instanceId": "instance.pulse",
              "clip": {
                "domain": "animation",
                "id": "animation.note.pulse"
              },
              "startBeat": { "numerator": 8, "denominator": 1 },
              "iterations": 2,
              "fillMode": "none",
              "weight": 1.0,
              "propertyMask": {
                "properties": ["transform.scale"],
                "prefixes": []
              }
            }
          ]
        }
      ]
    }
  ]
}
```

写作规则：

- `priority` 越大越晚应用；相同 priority 的 Layer 不得写入相交属性。
- `propertyMask` 是明确白名单；空 mask 不是 wildcard，也不会写入任何属性。
- `mode` 为 `override` 或 `additive`。不同 Group 的 effective property set 不能重叠。
- `iterations` 是 `1..65535` 或 `"infinite"`；`infinite` 必须配 `fillMode: "none"`。
- `fillMode` 是 `none` 或 `hold`；`none` 在最终边界停止写入，`hold` 保持 Clip 结束值。
- 离散属性只能使用 Override，并要求 resolved Layer/Group weight 都为 `1`。
- Additive scale 必须是有限正因子；Material opacity/tint 不支持 Additive。
- v4 的 Additive rotation 使用 Quaternion delta 语义，不要把它当作 Euler 角度相加。

### 9.3 何时使用 Behavior，何时使用 Animation

| 需求 | 建议 |
| --- | --- |
| 一个实体按全局谱面 Beat 移动 | `behavior.event` |
| 同一局部动作被多个 Object 复用 | `animationClip` + Animator |
| 同一动作需要不同开始 Beat 或循环次数 | Clip Instance |
| 使用外部 `.cxt` 模板 | import + Template Binding |
| 改变 parent、AssetId 或拓扑 | 直接改 Chart Object，不使用参数或动画绕过 |

## 10. 参数化

参数在 prepare 前由宿主提交并冻结，不是运行时表达式。声明示例：

```json
{
  "id": "layout.x",
  "type": "number",
  "default": 0.0,
  "constraints": {
    "minimum": -10.0,
    "maximum": 10.0
  }
}
```

引用示例：

```json
"position": [
  {
    "parameter": {
      "domain": "chart-parameter",
      "id": "layout.x"
    }
  },
  0.0,
  0.0
]
```

v4 首版允许参数化：

```text
cuexis.transform.position x/y/z
cuexis.transform.scale x/y/z
camera.fovY
Animator Layer / BlendGroup / Clip Instance weight
Template Binding durationScale / weight
```

不要参数化 ID、path、AssetId、parent、组件集合、数组长度、startBeat、priority、iterations、
fillMode、blendMode、property mask、Quaternion 或 Track/Segment/Step value。参数引用不是表达式，
不能写函数、条件、随机数或运行时变化。

## 11. CXT、Audio、资源和扩展

### 11.1 CXT

Chart 引用 CXT 时，先声明 import：

```json
"animationTemplateImports": [
  {
    "id": "motion.move-y",
    "source": "templates/move-y.cxt"
  }
]
```

`source` 相对于 Project/CXC 根，必须是存在的普通文件，并以小写 `.cxt` 结尾。CXT 内的
`templateId` 必须与 import `id` 完全相同。然后在 Object 的 Animator 中添加 `templateBindings`。
CXT 的完整写法见 [CXT_FORMAT.md](../formats/CXT_FORMAT.md)。

### 11.2 Audio

v4 的主音乐是可选的：

```json
"audio": {
  "version": 1,
  "mainMusic": {
    "domain": "asset",
    "id": "audio.main"
  }
}
```

`audio` 只能引用 Asset Index 中的 Audio 叶节点。设备、输出延迟和用户校准不写入 Chart。

### 11.3 Extensions

必需扩展写在 `requiredExtensions`，实际数据写在 `extensions`：

```json
"requiredExtensions": [
  { "id": "org.example.custom-note", "version": 1 }
],
"extensions": {
  "org.example.custom-note": {
    "version": 1,
    "data": {}
  }
}
```

未知可选扩展会保留并警告；未知 required extension 会使 prepare 失败。不要把旋转、脚本或动画字段
塞进 `extensions` 来绕过 v4 合同。

## 12. 旋转、透明度和 v5 边界提醒

当前 v4 的作者值是：

```text
transform.rotation   Quaternion [x,y,z,w]，shortest-path slerp
material.opacity     normalized number [0,1]
camera.fovY          (0,179)
```

不要在 v4 中使用以下 v5 计划字段或语义：

```text
Euler XYZ rotation degrees
negative/unwrapped multi-turn angle
object alpha [0,255]
camera.fovY = 180
```

需要这些能力时，应等待 v5 格式合同和迁移工具完成，而不是在 v4 的 `extensions` 或普通 Vec3 中自定义
一套不可迁移的表示。

## 13. 手写检查清单

提交前按以下顺序检查：

1. `format`/`version` 正确，`chartId` 和所有 Object/Template ID 唯一。
2. 顶层 v4 必需字段齐全，空数组也没有漏写。
3. Beat 已约分，所有 denominator 大于 `0`。
4. Camera 的 `fovY` 在 `(0,179)`，`near < far`。
5. 每个 `parent` 都引用存在的 Object，且没有环。
6. 直接 Object 没有 `template`/`overrides`；Template Object 没有 `components`。
7. 每个 Component 的 `version` 为 `1`，引用的 domain 正确。
8. Behavior 的事件属性都有目标 Component；同一属性没有重叠或重复 Beat。
9. Quaternion 有四个有限分量并可归一化；没有把 Euler 度数写进 rotation。
10. Animation 的 Clip、Layer、Group、Instance ID 在各自作用域唯一，mask 不为空且没有冲突。
11. 离散动画使用 Override，相关 weight 为 `1`；Additive scale 为正。
12. CXT import 的路径、templateId 和 Project/CXC 闭包一致。
13. 所有 Asset 引用都能在 Asset Index 和资源闭包中解析。
14. 没有未知核心字段、脚本字段、表达式或隐式 SDK 资源查找。

## 14. 验证命令和常见诊断

单个 Chart：

```powershell
cuexis_chart_validator --input <chart-v4.json>
```

从 v1/v2/v3 显式迁移为 v4：

```powershell
cuexis_chart_migrator --input <source.json> --output <chart-v4.json> `
  --report <migration-report.json> --target 4
```

常见诊断：

| 现象 | 方向 |
| --- | --- |
| `chart.format.unsupported` | 检查 `format` 和显式 `version` |
| `chart.reference.missing` / `chart.animation.reference_missing` | 检查 domain、ID 和声明顺序无关的引用表 |
| `chart.parent.missing` 或层级冲突 | 检查 parent 是否引用 Object、是否存在环 |
| `chart.patch.path_unsupported` | 检查 Patch 路径，Animator 只能整体替换 |
| `chart.animation.track_conflict` | 检查同属性重叠、重复 Beat 或重复 Track |
| `chart.animation.mask_conflict` | 检查 Layer/Group/Instance mask 是否相交 |
| `chart.animation.additive_unsupported` | 检查 Additive 属性白名单和 scale 正值 |
| `chart.animation.discrete_weight_unsupported` | 离散属性的 Layer/Group weight 必须为 `1` |
| `playback.chart.v4.requires_portable_presentation` | 为 Renderable 资源提供 CXPRES01 payload |
| `playback.capability.unsupported` | 当前 Session 没有 Chart v4 所需 capability |

格式字段、完整预算和稳定诊断以 [CHART_V4_FORMAT.md](../formats/CHART_V4_FORMAT.md) 为准；
运行时混合细节以 [ANIMATION_MIXING.md](../formats/ANIMATION_MIXING.md) 为准。
