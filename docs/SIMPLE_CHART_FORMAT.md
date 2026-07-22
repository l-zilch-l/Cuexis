# Cuexis Simple Chart Format v1

状态：阶段 1C 已实现的方案 B v1 兼容导入格式

更新日期：2026-07-22

## 1. 范围

`cuexis.chart.simple` 用于 Cuexis Studio 完善前的手写谱面。它不是 Runtime 格式，也不是与方案 A 平级的规范格式。

```text
SimpleChartDocument
  -> SimpleChartImporter
  -> cuexis.chart canonical JSON
  -> CanonicalChartLoader typed 结构与语义校验
  -> ChartDocument
  -> ChartCompiler / PlaybackSession（内部使用 RuntimeSession）
```

方案 B 是方案 A 的严格子集。Studio 成熟后冻结其最高版本，不再增加只属于方案 B 的新能力。外部宿主不需要实现方案 B Importer；它通过 Cuexis Playback/Chart 公共组件获得与 Player、Studio 相同的确定性转换结果。

仓库同时提供 `schemas/cuexis.chart.simple.v1.schema.json` 和独立 JSON Schema adapter/test；阶段 1A 的 `SimpleChartImporter` 当前使用 typed Reader 并复用 CanonicalChartLoader 的语义校验，尚未在 loader 路径调用 Schema validator。

## 2. 顶层结构

```json
{
  "format": "cuexis.chart.simple",
  "version": 1,
  "chartId": "019b0000-0000-7abc-8def-000000000001",
  "metadata": {
    "title": "Example",
    "artist": "Example Artist",
    "charter": "Example Charter",
    "audio": "asset:audio.example"
  },
  "timing": {
    "offsetMs": 0.0,
    "bpm": 120.0
  },
  "camera": {
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
  },
  "templates": {},
  "behaviors": {},
  "objects": {},
  "extensions": {}
}
```

`camera` 为可选字段，与方案 A 相同的结构和语义（见 `CHART_FORMAT.md` §4a）。省略时使用透视默认值。

`templates`、`behaviors` 和 `objects` 使用对象映射，键即方案 B 的可读 ID。映射键顺序不具有运行语义。

方案 B v1 不支持 BPM Changes、Stops 或 `speedChanges`。Entity 运动速度由 Behavior 表达。

## 3. Simple ID

ID 必须匹配：

```text
[a-z][a-z0-9._-]{0,127}
```

示例：

```text
lane.main
note.intro.001
note.standard
note.enter
```

同一 ID 域内不得重复。`chartId` 必须是 UUIDv7；Importer 以它作为命名空间，确定性生成导入 UUID：

```text
Object UUID   = UUIDv5(chartId, "object:" + simpleId)
Template UUID = UUIDv5(chartId, "template:" + simpleId)
```

Behavior ID 保持为当前 Chart 内的稳定可读 ID，并进入独立 behavior 域。

原生方案 A 创建和保存的 Object/Template ID 使用 UUIDv7；方案 B 导入产生的 Object/Template ID 使用 UUIDv5。生成输入只包含 `chartId` 和带 domain 前缀的 Simple ID；Canonical loader 明确接受原生 UUIDv7 和导入 UUIDv5。

## 4. Simple Reference

引用使用 `domain:id`：

```text
object:lane.main
template:note.standard
behavior:note.enter
asset:mesh.note.standard
```

字段限制允许的 domain：

```text
parent    -> object
template  -> template
behavior  -> behavior
mesh      -> asset
material  -> asset
audio     -> asset
```

Importer 不根据字符串形状猜测 domain。方案 B 不支持跨 Chart 引用。

## 5. Beat String

Beat 只允许 JSON 字符串：

```json
"4"
"7/4"
"1.25"
```

转换结果：

```text
"4"    -> 4/1
"7/4"  -> 7/4
"1.25" -> 5/4
```

Importer 必须根据字符串的十进制文本精确转换并约分，不得先转换为二进制浮点。JSON 数值形式的 Beat 属于错误。

## 6. Object

```json
{
  "objects": {
    "lane.main": {
      "kind": "element",
      "transform": {
        "position": [0.0, 0.0, 0.0],
        "rotationDeg": [0.0, 0.0, 0.0],
        "scale": [1.0, 1.0, 1.0]
      },
      "render": {
        "mesh": "asset:mesh.lane.standard",
        "material": "asset:material.lane.standard"
      }
    },
    "note.intro.001": {
      "kind": "note",
      "parent": "object:lane.main",
      "beat": "4",
      "transform": {
        "position": [0.0, 0.0, 5.0]
      },
      "render": {
        "mesh": "asset:mesh.note.standard",
        "material": "asset:material.note.standard"
      },
      "behavior": "behavior:note.enter"
    }
  }
}
```

`kind` v1 固定支持：

```text
note        -> cuexis.note
element     -> cuexis.element
decoration  -> 无语义 Tag
```

未知 kind 是错误。方案 B 不提供自定义 Component 或 kind；新语义只进入方案 A。

字段映射：

```text
kind        -> Tag Component
beat        -> cuexis.note.beat
transform   -> cuexis.transform
render      -> cuexis.renderable
behavior    -> cuexis.behavior
```

`note` 必须包含 beat。其他 kind 不得包含 beat。

`render` 可以确定性转换为规范 `cuexis.renderable`，但阶段 1A 尚无外部 Mesh/Material 资源生命周期，`ChartWorldInstantiator` 会以 `runtime.chart.renderable_resources_unsupported` 明确拒绝其实例化。阶段 1A 自带的 A/B 示例只使用三对象父子 Transform，并通过 DebugDraw 显示 XYZ 轴线；外部 Renderable 的运行闭环属于阶段 1B。

## 7. Transform

默认值：

```text
position    [0, 0, 0] 米
rotationDeg [0, 0, 0] 度
scale       [1, 1, 1] 无量纲倍率
```

欧拉角转换固定为：

```text
旋转应用顺序：X -> Y -> Z
矩阵组合：Rz * Ry * Rx
规范输出：Quaternion [x, y, z, w]
```

Transform 数值必须有限。Importer 不能根据运行平台改变旋转顺序。

## 8. Behavior

```json
{
  "behaviors": {
    "note.enter": {
      "tracks": [
        {
          "property": "transform.position.z",
          "keys": [
            { "beat": "0", "value": 10.0 },
            { "beat": "4", "value": 0.0, "easing": "out_cubic" }
          ]
        }
      ]
    }
  }
}
```

阶段 1C importer 校验 Track 的 `property`、`keys`、有限标量 `value`，并支持 `linear`、`in_cubic`、`out_cubic`、`in_out_cubic` easing；它把字符串 Beat 精确转换成方案 A 有理数 Beat。Behavior、Track、Key 子树的未知字段是错误；其他 Simple 未知字段仍产生 warning 并保留。Material Track、循环和复杂 PropertyBinding 不加入方案 B。

所有 key 的 Beat 使用第 5 节的字符串语法。Importer 输出方案 A Behavior；Canonical Compiler 将 Beat 排序并编译为 `chartTimeMs`，PlaybackSession 可直接按绝对时间采样位置和相机 FOV。Simple v1 不扩展为 Quaternion/Vec3 Track，通用 Curve、循环和新 Property 留在阶段 2。

## 9. Template

Template 使用与 Object 相同的简化字段，但不得包含 `parent`、`beat` 或 `template`：

```json
{
  "templates": {
    "note.standard": {
      "kind": "note",
      "transform": {
        "scale": [1.0, 1.0, 1.0]
      },
      "render": {
        "mesh": "asset:mesh.note.standard",
        "material": "asset:material.note.standard"
      }
    }
  }
}
```

实例通过模板引用和普通字段覆盖：

```json
{
  "objects": {
    "note.intro.001": {
      "template": "template:note.standard",
      "beat": "4",
      "transform": {
        "position": [1.0, 0.0, 5.0]
      }
    }
  }
}
```

合并规则：

```text
Template 基础字段
-> Object 字段递归覆盖
-> 对象字段覆盖模板同名字段
-> 数组整体替换
-> null 删除可选字段
```

方案 B 不支持模板继承或多模板组合。Importer 把 Template 转换为方案 A 根模板，并把实例差异转换为 JSON Patch overrides。

## 10. Unknown and Unsupported Fields

```text
非法 ID：错误
错误 reference domain：错误
无效 Beat：错误
未知 kind：错误
已知但方案 B 不支持的功能：错误
未知字段：警告并保存到 extensions["cuexis.simple.unknown"]
缺失父对象：继续转换，由方案 A 编译器执行子树跳过规则
```

未知字段不得产生 Runtime 行为。导入诊断必须保留方案 B 中以 `$` 为根的稳定字段路径和转换后的规范 ID。字段路径用于定位输入，不宣称严格等同于外部 JSON Pointer 标准的完整语法。

## 11. Deterministic Conversion

转换顺序：

```text
解析 format 与 version
-> 校验 chartId 和 Simple ID
-> 为完整文档建立 ID 映射
-> 转换 Template
-> 转换 Behavior
-> 合并 Object 简写与 Template
-> 生成 cuexis.chart ChartDocument
-> 执行 CanonicalChartLoader typed 结构与 Chart 语义校验
```

转换结果不得依赖 JSON 对象键顺序、文件路径、系统时区或随机数。相同方案 B 文档必须产生相同方案 A ID、引用和语义数据。

方案 B 与方案 A 共用默认 Chart 预算，包括 16 MiB 输入、64 层嵌套、单个 JSON object key 或 string value 1 MiB、10000 个 Template、10000 个 Behavior、100000 个 Object 和 1024 条诊断。Importer 还限制 Simple ID/引用与 Beat 文本长度，以及每个 Behavior 的 Track/Key 数量；调用方可以传入更严格的 `ChartLimits`。

## 12. 生命周期

Studio 导入方案 B 后默认保存为方案 A，不保证反向导出。Studio 成熟后冻结方案 B v1，不再加入方案 A 的新 Component 或表现能力。
