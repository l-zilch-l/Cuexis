# Cuexis Animation Template (CXT) v1

状态：accepted contract；CFU-C Reader/Writer 已实现；CFU-E 已接入 prepare import/lookup，非空动画在 Stage 4 前稳定拒绝

更新日期：2026-08-14

依据：[ADR 0038](adr/0038-cxc-v1-and-chart-v4-boundary.md)

## 1. 范围

CXT v1 是可被 Chart v4 引用的声明式动画模板 JSON：

```text
extension     .cxt
encoding      UTF-8 JSON
format        cuexis.animation-template
version       1
exports       exactly one Animation Template per file
time domain   local rational Beat
```

Chart import `source` 必须满足 portable project path 规则并以精确小写 `.cxt` 结尾；`.CXT`、`.json`
或无扩展名路径均失败。独立 CXT bytes Reader 不接收路径，扩展名门禁由 Chart import/Source Project
边界执行。

CXT 不是脚本、插件、Object prefab、ChartRuntime cache 或可执行字节码。Source Project/CXC 必须
包含 Chart 引用的确切 CXT bytes；Playback 不按名称查找 SDK、网络或宿主隐式实现。

CXT 只负责可复用的局部时间动画及固定应用语义。Object parent、初始组件、Chart startBeat、
duration scale、weight 和 priority 属于 Chart v4 Template Binding，权威见
[CHART_V4_FORMAT.md](CHART_V4_FORMAT.md)。

## 2. 处理边界

```text
Source Project / CXC
  Chart JSON
  referenced CXT JSON
  typed ChartParameterSet supplied by host
        |
        | typed read + template expansion + parameter freeze + import validation
        v
ResolvedChartDocument
  concrete AnimationClip + generated Layer/BlendGroup/Instance
        |
        v
ChartRuntime / AnimationProgram
```

`engine/animation/` 不读取 JSON、CXC 或 CXT。Import、参数解析和确定性 lowering 属于
Chart/Playback prepare 边界。`PlaybackSource` 通过独立 project-document table 持有入口 Chart 与
CXT 的 project-relative path/UTF-8 bytes；AssetId bytes 继续由 `IContentProvider` 提供。CXT 不得
伪装成 Asset record。

## 3. 顶层

```json
{
  "format": "cuexis.animation-template",
  "version": 1,
  "templateId": "motion.move-y",
  "metadata": {
    "name": "moveY"
  },
  "application": {
    "coordinateSpace": "local",
    "blendMode": "additive",
    "iterations": 1,
    "fillMode": "hold"
  },
  "clip": {
    "version": 1,
    "durationBeats": { "numerator": 1, "denominator": 1 },
    "tracks": [
      {
        "property": "transform.position.y",
        "segments": [
          {
            "startBeat": { "numerator": 0, "denominator": 1 },
            "durationBeats": { "numerator": 1, "denominator": 1 },
            "startValue": 0.0,
            "endValue": 1.0,
            "startSlope": 1.0,
            "endSlope": 1.0
          }
        ]
      }
    ],
    "stepTracks": []
  },
  "requiredExtensions": [],
  "extensions": {}
}
```

所有顶层字段必需，未知核心字段失败。

| 字段 | 规则 |
| --- | --- |
| `format` | 固定 `cuexis.animation-template` |
| `version` | 固定 `1` |
| `templateId` | portable stable ID，必须与 Chart import ID 相同 |
| `metadata` | v1 只允许可选 `name`，不参与求值 identity |
| `application` | 固定 coordinate space、blend mode、iterations 和 fill mode |
| `clip` | 复用 AnimationClip v1，但不重复保存 Clip ID |
| `requiredExtensions` | 模板要求的 extension ID/version |
| `extensions` | 未知可选扩展保留并受预算限制 |

`coordinateSpace` v1 只允许 `local`。相同模板绑定到不同 parent 时产生相同局部属性写入，但最终
world-space 结果可以不同。v1 不提供 world/screen/lane-space 隐式转换。

`blendMode` 为 `override` 或 `additive`，并受 AnimationClip 属性白名单约束。`iterations` 是
`1..65535` 或 `"infinite"`；`fillMode` 为 `none` 或 `hold`，infinite 只允许 none。

CXT Clip 的 Beat、value、应用模式和引用全部是 literal。CXT 不接受 ChartParameterRef。Clip、
Track、Segment 和 Step 的字段权威见 [CHART_V4_FORMAT.md](CHART_V4_FORMAT.md)。

Writer 按 Property ID 排序 Track、按 Beat 排序 Segment/Step，并约分 Rational Beat。主排序键相同
时按已规范化 record 的 compact JSON bytes 决胜。重复 Property Track、重复 Segment startBeat 或
重复 Step beat 失败，不静默去重。规范 CXT identity 是 canonical CXT JSON bytes 的 SHA-256；
metadata `name` 不参与 Animation 求值，但仍进入 CXT document identity。

## 4. Chart 集成

Chart v4 拥有以下集成合同：

```text
parameters[]
animationTemplateImports[]
cuexis.animator.templateBindings[]
durationScale and weight ParameterRef
deterministic lowering to Clip/Layer/BlendGroup/Instance
generated IDs and conflict diagnostics
```

CXT 不复制这些 Chart 字段。Chart import ID 必须与 `templateId` 完全相同；Template Binding 不能
覆盖 CXT 的 coordinateSpace、blendMode、iterations、fillMode 或属性集合。

Binding lowering 固定为 generated Layer weight `1`、generated Group weight 等于 resolved Binding
weight、generated Instance weight `1`。Generated identity 是内部复合键
`(objectId, bindingId, templateId, recordKind)`，不生成 hash 字符串，也不写回 Chart。

若 CXT Clip 写入离散属性，Binding 的 resolved weight 必须等于 `1`；否则 generated Group 的部分
weight 没有 v1 离散语义，并以 `chart.animation.discrete_weight_unsupported` 失败。

## 5. CXC 闭包

被 Chart import 的 CXT 必须作为普通 Stored entry 出现在 CXC manifest，并受 byteCount/SHA-256
校验。CXT 内所有 Asset reference 都进入资源闭包，包括 weight 0、被 mask 排除或没有 active
binding 的内容。

SDK/Studio 可以分发模板库，但 pack 必须复制被引用模板的确切 bytes。Playback 不允许按 `moveY`、
`emerge` 等名称回退到安装目录、网络或宿主隐式文件。

存在 CXT import 即要求 `cuexis.source.cxt.v1`、`cuexis.animation.clip.v1` 和
`cuexis.animation.layers.v1`，即使 import 尚未绑定或最终 weight 为 0。Stage 4 前 Playback 可以
读取和验证 CXT，但必须在 capability preflight 稳定失败，不能忽略模板继续播放。

## 6. 预算与诊断

| 项目 | 上限 |
| --- | ---: |
| CXT imports / Chart | 10,000 |
| CXT document bytes | 4 MiB |
| tracks + stepTracks / CXT Clip | 256 |
| segments or steps / Track | 65,536 |
| JSON depth | 64 |
| extension members | 256 |
| diagnostics / prepare | 1,024 |

稳定诊断至少包括：

```text
cxt.format.unsupported
cxt.version.unsupported
cxt.template.invalid
cxt.template.id_mismatch
cxt.import.missing
cxt.import.duplicate
cxt.budget.exceeded
```

从 Chart import 边界产生的 CXT 诊断必须包含 package-relative `source`、`template_id`、`import_id`
和字段路径；Binding lowering 诊断另含 `binding_id`、Object ID 与 generated record kind。诊断不输出
本机绝对路径。

## 7. 明确不支持

```text
runtime or per-frame parameter mutation
parameter changes to topology, parent, AssetId, references or resource closure
ChartParameterRef inside CXT
parameterized CXT Track/Segment/Step values
inheritance, extends, nested import or multiple exports
scripts, expressions, callbacks, state machines or random numbers
world/screen/lane-space templates
Object or subtree creation
implicit SDK template lookup
```

运行时脚本与逐帧脚本回调无限期延后，不属于任何已排期阶段，也不预留 Chart/CXT/CXC 字段、
extension、capability、字节码或 Playback 执行入口。只有新的明确产品决策和独立 ADR 才能重新
启动该议题。

离线 authoring generator 可以作为独立工具生成 Chart/CXT，但不进入 CXC，也不由 pack、prepare
或 Playback 隐式执行。

## 8. 候选示例

候选 CXT、Chart 引用和拒绝例见
[examples/chart_format_update](examples/chart_format_update/README.md)。CFU-C1 已将接受副本提升到
`tests/fixtures/chart_format_update/`，由生产 Schema 与 CXT typed Reader 验证；评审目录本身仍不
作为生产测试输入。
