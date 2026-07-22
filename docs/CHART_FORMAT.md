# Cuexis Chart Format v1

状态：阶段 1C 已实现的方案 A v1 行为规范

更新日期：2026-07-22

## 1. 范围

本文定义 `format: "cuexis.chart"` 的规范 ChartDocument 结构。它面向保存、迁移、Studio 编辑和 Playback SDK 编译，不是 ChartRuntime、World 或 FrameSnapshot 的内存布局。阶段 1C 已在阶段 1A/1B 的 format 路由、typed 结构读取、模板展开、资源事务与确定性编译基础上，激活 Behavior Track 的 typed 读取、编译和绝对时间求值。

`cuexis.chart.simple` 属于兼容导入格式，必须按 `docs/SIMPLE_CHART_FORMAT.md` 转换成本文结构。内部 Runtime、World 和各 System 只接收本文模型编译后的 ChartRuntime；嵌入宿主通过 PlaybackSession、FrameSnapshot、JudgementResult 和稳定查询接口交互，不接收 ChartRuntime 或 EnTT Entity。

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

必需字段：`format`、`version`、`chartId`、`metadata`、`timing`、`templates`、`behaviors`、`objects`、`requiredExtensions`、`extensions`。`camera` 为可选字段，省略时使用默认透视相机（fovY=60, near=0.1, far=1000, pitch/yaw/roll=0）。

顶层数组顺序不具有运行语义。所有 ID 在对应域中必须唯一，编译器必须产生与输入数组顺序无关的确定性结果。

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

方案 B 的十进制 beat 必须根据 JSON 原始十进制文本转换，不能先转成二进制浮点再猜测分数。

## 4. Timing

```json
{
  "offsetMs": 0.0,
  "defaultBpm": 120.0,
  "bpmChanges": [],
  "stops": []
}
```

`defaultBpm` 必须为有限正数。阶段 1A 已实现 `offsetMs` 与 `defaultBpm`；`bpmChanges` 和 `stops` 当前必须为空，非空时产生不支持诊断并拒绝加载，不得静默忽略。

BPM Change、Stop、offset 符号和逆映射的正式语义见 `docs/TIMING_MODEL.md`，计划在阶段 2 启用。

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

预留事件结构：

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
object    当前 Chart 中原生 UUIDv7 或方案 B 导入 UUIDv5 的 Object
template  当前 Chart 中原生 UUIDv7 或方案 B 导入 UUIDv5 的 Template
behavior  当前 Chart 中的 Behavior ID
asset     AssetDatabase 中的 AssetId
```

字段必须限制允许的 domain，例如 `parent` 只接受 `object`。方案 A 不允许字符串简写引用。

原生创建和保存的方案 A Object/Template ID 使用 UUIDv7；`SimpleChartImporter` 的确定性导入结果使用 UUIDv5。Canonical loader 接受这两种明确来源的版本，`chartId` 本身始终要求 UUIDv7。

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
id          必需，原生 ChartObjectId UUIDv7 或方案 B 导入 UUIDv5
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

空 Track、重复 Beat、同一 Behavior 重复 Property、未知字段/easing、非有限值、非规范 Quaternion 和越界 FOV 都是错误。Simple v1 仍只提供标量位置/FOV Track，并在 Behavior、Track、Key 子树对未知字段严格报错；canonical 与 Simple 的其余未知字段继续按各自规范处理。

默认安全预算为每 Behavior 6 条 Track、每 Track 65536 个 Key、每 Chart 262144 个 Behavior Key、每帧 600000 个 Property Write 和 1024 条诊断。通用 Curve、循环、Material/Visibility/ParentBinding Track 和新 easing 留在阶段 2，不改变 v1 采样结果。

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

仓库提供 `schemas/cuexis.chart.v1.schema.json`、`cuexis_json_support` 的 JSON Schema adapter 及独立测试，但当前 `ChartLoader` / `CanonicalChartLoader` 尚未调用 adapter。loader 的现行结构权威是 typed Reader，随后由 Chart 代码完成语义校验；因此不能把一次加载描述为已执行 JSON Schema Validator。Schema artifact 必须随格式代码同步，未来接入 loader 时仍不能替代引用、层级、资源和属性冲突等 Cuexis 语义校验。

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

以下内容不属于 v1，使用时返回 UnsupportedFeature 或 Schema Error。未来加入必须提升格式/组件版本并形成 ADR：

```text
跨 Chart Object / Template 引用
Template 子树实例化
扩展处理器动态插件 ABI
二进制 ChartRuntime 缓存格式
```
