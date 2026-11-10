# ADR 0036：Stage 2 Timing、Step Event 与可观察表现合同

日期：2026-08-05

状态：已接受

## 背景

ADR 0034 已冻结 Chart v3、Tempo Event 和连续 Behavior Event 的方向，但数值算法、
Stop 边界、离散事件、最小 Material 输出、能力协商和局部时间语义仍未确定。若这些事项在
实现过程中隐式决定，Schema、迁移器、Runtime 和 Playback SDK 会产生互不兼容的语义。

## 决策

### TimingMap 数值合同

- Tempo Event 直接插值 BPM。每个事件按端点 BPM 比值确定性预编译为最多 16 个几何分段；
  每个分段使用固定 16 点 Gauss-Legendre 求积。事件完整区间和各分段的积分在编译时缓存，
  运行时不按帧累计时间。内部几何 BPM 边界使用固定 64 次二分定位。
- `chartTimeMsToBeat` 使用固定 64 次二分。不得使用依赖容差提前退出的 Newton 迭代。
- 常用范围（BPM `[30, 360]`、单事件不超过 4096 Beat）正反向往返误差目标为
  `1e-7 Beat` 和 `1e-5 ms`；完整合法范围的有限性、单调性和往返误差上限为
  `1e-6 Beat` 和 `0.05 ms`。
- 单 Chart 最多 4096 个 Tempo Event 和 4096 个 Stop。单个 Runtime 查询不得分配。

Stop 使用字段 `beat` 和 `durationMs`。`durationMs` 必须有限且严格大于零。同一 Beat 最多
一个 Stop，输入顺序无语义。Stop 时间区间为 `[startTimeMs, endTimeMs)`；区间内返回该
Stop 的 Beat、`inStop=true` 和 `[0,1)` 的 `stopProgress`。精确结束边界返回相同 Beat、
`inStop=false`、`stopProgress=0`。`beatToChartTimeMs(stopBeat)` 返回 Stop 开始时间；大于
该 Beat 的查询包含完整 Stop 时长。负 Beat Stop 相对 Beat 0 反向定位，Beat 0 的
`chartTimeMs` 仍为零。

同 Beat 的零持续 Tempo Event 先建立该 Beat 起后使用的 BPM，Stop 随后冻结该 Beat。
Tempo 曲线在 Stop 内不推进。

### Behavior Event 和 Step Event

`behavior.event` version 1 同时包含 `events` 和 `stepEvents` 两个数组；两个数组都允许为空，
但 Behavior 至少包含一个事件。连续 Event 使用 ADR 0034 的字段和语义。

Step Event 仅包含：

```json
{
  "property": "render.visible",
  "beat": { "numerator": 8, "denominator": 1 },
  "value": false,
  "groupId": "optional-group"
}
```

- 事件在 `beat` 处（含）生效并保持到下一事件；首事件前保持对象初始基准。
- 同一 Property 不得在同一 Beat 出现多个事件。输入按 Property 和 Beat 排序。
- `groupId` 与连续 Event 共享同一 Behavior 作用域和字符规则。Step Event 组内 Beat 必须
  相同；连续 Event 与 Step Event 不能共用同一 `groupId`。
- Step Event 只支持离散值，连续 Event 只支持连续值；类型不匹配必须在读取或 prepare
  阶段失败，不能隐式转换。

### Visibility 与最小 Material 输出

Stage 2 冻结以下表现属性：

| Property | 事件 | 值与范围 | 初始基准 |
|---|---|---|---|
| `render.visible` | Step | Boolean | `true` |
| `render.material` | Step | Asset reference | `cuexis.renderable.material` |
| `material.opacity` | Continuous | finite number `[0,1]` | `1.0` |
| `material.tint` | Continuous | Vec3，各分量 `[0,1]` | `[1,1,1]` |

`FrameSnapshot::ObjectSnapshot` 输出 `visible`、`materialAssetId`、`materialOpacity` 和
`materialTint`。这些字段仅表示可移植的最终表现值，不定义 Shader、Pipeline、纹理槽或
后端对象。所有 Step Event 引用的 Material 在 prepare 时解析；缺失的动态 Material 是
稳定失败，不在播放中回退。

新增字段进入 FrameDigest version 2；version 1 的算法定义不改变。

### Capability、诊断与调试

Playback capability 集合 version 1 使用以下稳定 ID：

- `cuexis.chart.v3`
- `cuexis.behavior.event.v1`
- `cuexis.render.visibility.v1`
- `cuexis.material.snapshot.v1`

默认 `PlaybackSession` 提供全部 Stage 2 capability；显式配置的 Session 只提供传入集合。
prepare 从 Chart 内容推导需要的 capability，在资源获取和 World 发布前，以稳定诊断拒绝
缺失能力。它不复用 `requiredExtensions`。

Runtime 的调试快照为显式启用、容量固定的内部接口。每项记录对象、Property、初始基准、
命中事件、归一化进度、Behavior 输出和最终值。关闭时不记录也不分配；容量耗尽时以
`truncated` 标志报告，不把内部指针、EnTT 或 World 类型放入 Playback `FrameSnapshot`。

### 明确延期

ParentBinding、局部 Beat、`startBeat`、`repeat`/`pingPong`、多 Clip、priority、weight 和
Override/Additive 混合不进入 Stage 2。Chart v3 Schema 和 typed Reader 不预留这些字段，
输入通过未知字段或未知 Property 的稳定诊断拒绝。

## 迁移合同

`ChartMigrator` 是 `cuexis_chart` 内部 API，`cuexis_chart_migrator` 是独立 CLI。CLI 必须
显式接收输入、输出和报告路径，输出与源路径相同即失败，任何失败都不创建或替换目标。

v1/v2 Keyframe 迁移规则如下：

- 对象初始基准改写为每个 Track 的首 Key 值；共享 Behavior 的每个绑定对象分别改写。
- `linear`、`in_cubic`、`out_cubic` 分别映射为斜率 `(1,1)`、`(0,3)`、`(3,0)`。
- `in_out_cubic` 在两个 Beat 的精确有理数中点拆成 `(0,3)` 和 `(3,0)` 两个 Event，中点值
  由旧采样器在 `t=0.5` 的 typed 值计算。中点无法在 RationalBeat 预算内精确表示时失败。
- 单 Key Track 通过基准改写完成，不生成无意义事件。未绑定 Behavior 仍转换其多 Key Track，
  并在报告中标为未绑定；模板实例在迁移输出中展开，迁移输出不保留模板继承。
- Quaternion 迁移使用相同 shortest-path slerp；可观察分量误差上限为 `1e-6`。

## 影响

Stage 2 会扩展 Chart、Runtime 和 Playback 的 C++ 结构，因此 SDK API 提升到 `0.4.0`。
Chart v1/v2 和 Behavior Keyframe v1 的读取与采样路径保持原样；迁移只由显式 API/CLI 触发。
