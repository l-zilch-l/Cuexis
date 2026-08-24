# Stage Chart Format Update CFU-G2 Stage 4 Typed Handoff

状态：CFU-G2 complete；Stage 4 typed handoff 已冻结，Stage Chart Format Update 仍为 active

快照日期：2026-08-16

实现基线：`913639ca6049ce9c974a6d8fe210cd2d77ec4dd7` 加当前 G0/G1/G2 文档 worktree。
该基线不是 CFU-G 最终候选 SHA，也不是 Stage 4 实现。

权威合同：[ADR 0038](../adr/0038-cxc-v1-and-chart-v4-boundary.md)、
[Chart v4](../CHART_V4_FORMAT.md)、[CXT v1](../CXT_FORMAT.md)、
[CXC v1](../CXC_FORMAT.md)、[Animation Mixing](../ANIMATION_MIXING.md) 和
[Stage 4 实施计划](../stage_plans/stage_4_implementation_plan.md)。

## 1. 交接结论

Stage 4 的格式输入已经具备可直接消费的 owning typed 形态：
`cuexis::chart::ChartV4Resolver::resolve()` 在参数冻结、CXT import、Template Binding lowering、
引用/资源/预算校验和确定性排序完成后，返回
`ChartV4ResolvedArtifact::animationProgram`，类型为 `AnimationProgramInput`。

G2 只冻结这一交接边界、现有 capability、fixture、预算、diagnostics、验收入口和残余风险。它没有：

- 实现 `AnimationSystem`、采样器、混合器、`PropertyResolver` 或 OverrideToken；
- 让非空 Chart v4 动画通过 Playback prepare；
- 建立公共 CXC package API；
- 新增格式字段、extension、capability、ABI 或 FrameDigest 版本；
- 为运行时脚本、逐帧回调、表达式或 bytecode 预留入口。

因此 G2 完成后，Stage Chart Format Update 仍为 active，Stage 4 仍为 blocked / not started。
下一检查点是 G3 final candidate validation；只有 completion report 经项目所有者接受后，Stage 4
才能改为 unblocked but not started。

## 2. Typed 输入合同

生产定义位于 `engine/chart/include/cuexis/chart/chart_v4_resolver.hpp`。Stage 4 的格式适配入口必须
接收 `AnimationProgramInput` 或由它一次性编译出的 owning runtime program，不得接收
`ChartV4SourceDocument`、`AnimationTemplateDocument`、JSON DOM、CXC package 或 CXT 文本。

### 2.1 顶层和稳定 identity

```text
AnimationProgramInput
  clips:   vector<AnimationProgramClip>
  objects: vector<ObjectAnimationProgram>

AnimationRecordIdentity
  chart-local record: string ID
  generated record:   (objectId, bindingId, templateId, recordKind)
```

`GeneratedRecordKind` 固定为 `Clip`、`Layer`、`BlendGroup`、`ClipInstance`。Chart-local 记录使用原始
ID；每个 Template Binding 生成四条复合 identity 记录。Stage 4 必须用
`AnimationProgramClip::identity` 解析 instance 的 `clipIdentity`，不能用内嵌
`AnimationClip::id` 作为全局唯一键：generated Clip 的 `clip.id` 是 `templateId`，多个对象或 Binding
可以合法复用它。

resolver 已固定输出顺序：

- `clips` 按 `AnimationRecordIdentity` 升序；
- `objects` 按 `ChartObjectId.value` 升序；
- Object 内 Layer 按 `priority` 升序，再按 identity 升序；
- BlendGroup 和 ClipInstance 分别按 identity 升序。

运行时结果不得依赖 vector 插入顺序、Entity 遍历顺序或地址值。相同 priority 且 mask 重叠已在
resolver 阶段失败，Stage 4 不得另设隐式 tie-break。

### 2.2 Clip

`AnimationProgramClip` 包含 identity 和完整 owning `AnimationClip`：

| 字段 | typed 内容 | Stage 4 用途 |
| --- | --- | --- |
| `durationBeats` | 正有理 Beat | Clip 局部时间范围和循环边界 |
| `tracks` | continuous property + segments | 连续采样 |
| `stepTracks` | discrete property + steps | 离散 winner / hold 采样 |
| segment | start/duration Beat、start/end value、start/end slope | 按冻结的曲线合同采样 |
| step | Beat + typed value | 在绝对 local Beat 重建当前值 |
| `fieldPath` | 源诊断路径 | prepare/调试溯源，不是运行时语义 |

连续属性为 `transform.position.x/y/z`、`transform.rotation`、`transform.scale`、
`material.opacity`、`material.tint`；离散属性为 `render.visible` 和 `render.material`。值已经是
`double`、Cuexis `Vec3`/`Quat`、`bool` 或 `AssetId`，Stage 4 不得重新解释 JSON 字面量。

### 2.3 Object、Layer、Group 和 Instance

| typed 记录 | 冻结字段 |
| --- | --- |
| `ObjectAnimationProgram` | `objectId`, `layers` |
| `ResolvedAnimationLayer` | `identity`, `priority`, resolved `weight`, `propertyMask`, `blendGroups` |
| `ResolvedBlendGroup` | `identity`, `mode`, resolved `weight`, `instances` |
| `ResolvedClipInstance` | `identity`, `clipIdentity`, `startBeat`, `durationScale`, `iterations`, `fillMode`, resolved `weight`, `propertyMask` |

`iterations` 是有限 `1..65535` 或 infinite；`fillMode` 是 `none` 或 `hold`。`durationScale` 已解析为
正有理数，Chart-local Instance 固定为 `1/1`。Layer、Group 和 Instance weight 已冻结为有限
`[0,1]` 数值，mask 已是显式 property/prefix 列表。

Template Binding 已 lowering 为相同记录模型：

```text
Layer priority = Binding priority
Layer mask     = CXT Clip 实际属性
Layer weight   = 1
Group mode     = CXT application.blendMode
Group weight   = resolved Binding weight
Instance start = Binding startBeat
durationScale  = resolved Binding durationScale
iterations     = CXT application.iterations
fillMode       = CXT application.fillMode
Instance weight = 1
Instance mask   = CXT Clip 实际属性
```

Stage 4 不再读取 Binding 或 CXT document，也不得建立第二套 lowering。

## 3. 所有权和生命周期

`AnimationProgramInput` 及其子记录只含 Cuexis value types、`std::string`、`std::vector`、
`std::variant` 和 `std::optional`；没有 span、裸指针、JSON DOM、archive view、文件句柄或 provider
borrow。成功 `ChartV4ResolveResult` 的 `artifact` 完整拥有这些数据，调用方可把 program move 到
prepare candidate 或编译成独立 runtime program。源 Chart 文本、project-document table、CXT 文本和
CXC package 结束生命周期后，runtime program 仍必须有效。

当前 Playback 路径的实际边界是：

1. `PlaybackSession::prepare()` resolve Chart v4，并先把 program 放入函数内的 owning staging；
2. resolver 推导的两个 animation capability 当前不在 Playback 支持集合中，因此任意非空动画会在
   preflight 以 `playback.capability.unsupported` 失败，早于 `PreparedPlayback` candidate 构造；
3. 只有空 program 能沿现有路径进入 `PreparedPlayback::State::animationProgram`，而当前
   `PlaybackSession::commit()` 没有把该字段移入 active session，因为 Stage 4 尚未存在。

Stage 4 必须在宣称 animation capability 前，增加事务性的 compile/own/commit 路径：prepare candidate
拥有已验证的 compiled animation state；commit 原子替换 active state；失败 prepare/reload 不改变现有
Runtime、identity、diagnostics、frame 或 animation state。不得让 active runtime 借用
`ChartV4ResolvedArtifact` 或 `PreparedPlayback` 内存。

## 4. Capability 交接

当前 Playback 默认支持并公开格式/载体常量：

```text
cuexis.chart.v4
cuexis.source.cxc.v1
cuexis.source.cxt.v1
```

`cuexis.source.cxc.v1` 由 CXC PlaybackSource 添加；resolver 对 v4 和 CXT import 分别推导
`cuexis.chart.v4` 与 `cuexis.source.cxt.v1`。

resolver 对任意非空 CXT import、AnimationClip、Template Binding 或 Layer 推导：

```text
cuexis.animation.clip.v1
cuexis.animation.layers.v1
```

这两个名称已经是格式 requirement，但当前不在 `PlaybackSession` 默认 capability 集合中，也没有
对应的 public Playback 常量。Stage 4 只有在 typed compile、update、seek/stop/reload、
PropertyResolver、FrameSnapshot 和 consumer 门禁全部通过后，才能实现并声明它们。在此之前必须继续
以 `playback.capability.unsupported` 稳定拒绝，不能忽略未绑定 import、weight 0 或当前不可见动画。

G2 不新增第三个 animation capability，不升级 capability set version，不更改 FrameSnapshot 或
FrameDigest v3。

## 5. Fixture 和测试入口

Stage 4 首批测试应直接复用生产 fixture，不复制为新的 JSON/CXT 变体。

### 5.1 合法输入

- `tests/fixtures/chart_format_update/valid/chart_v4_animation.json`
- `tests/fixtures/chart_format_update/valid/chart_v4_cxt_template_binding.json`
- `tests/fixtures/chart_format_update/valid/chart_v4_template_animator.json`
- `tests/fixtures/chart_format_update/valid/chart_v4_parameterized_transform.json`
- `tests/fixtures/chart_format_update/valid/chart_v4_parameterized_rational.json`
- `tests/fixtures/chart_format_update/valid/templates/move-y.cxt`
- `tests/fixtures/chart_format_update/source_project/`

### 5.2 非法和边界输入

- `tests/fixtures/chart_format_update/invalid/chart_v4_mask_conflict.json`
- `tests/fixtures/chart_format_update/invalid/chart_v4_mask_overlap.json`
- `tests/fixtures/chart_format_update/invalid/chart_v4_additive_material.json`
- `tests/fixtures/chart_format_update/invalid/chart_v4_discrete_partial_weight.json`
- `tests/fixtures/chart_format_update/invalid/chart_v4_cxt_missing_import.json`
- `tests/fixtures/chart_format_update/invalid/chart_v4_cxt_id_mismatch.json`
- `tests/fixtures/chart_format_update/invalid/chart_v4_cxt_parameter_type.json`
- `tests/fixtures/chart_format_update/invalid/move_y_runtime_script.cxt`
- `tests/fixtures/chart_format_update/binary/`

### 5.3 Golden 和既有合同测试

- `tests/fixtures/chart_format_update/golden/move-y.canonical.cxt`
- `tests/fixtures/chart_format_update/golden/cxc_v1_v4_cxt.cxc`
- `tests/fixtures/chart_format_update/golden/cfu_f3_determinism.txt`
- `tests/chart/chart_v4_loader_tests.cpp`
- `tests/chart/chart_v4_resolver_tests.cpp`
- `tests/chart/chart_v4_c2_contract_tests.cpp`
- `tests/chart/cfu_f4_limits_tests.cpp`
- `tests/playback/playback_v4_prepare_tests.cpp`
- `tests/cxc/cfu_f4_safety_tests.cpp`

格式层 invalid fixture 必须继续在 Reader/resolver/package 阶段失败；Stage 4 单元测试应以手工构造的
typed program 覆盖 evaluator 防御性错误，不得为了测试 AnimationSystem 而在
`engine/animation/` 解析这些文件。

## 6. 预算交接

### 6.1 Chart v4 / CXT

| 预算 | 默认上限 |
| --- | ---: |
| Chart input | 16 MiB |
| one CXT input | 4 MiB |
| nesting depth | 64 |
| string bytes | 1 MiB |
| diagnostics | 1,024 |
| identifier bytes | 256 |
| CXT template name bytes | 256 |
| parameters | 256 |
| CXT imports / Chart | 10,000 |
| animation clips / Chart | 10,000 |
| tracks + step tracks / Clip | 256 |
| segments or steps / Track | 65,536 |
| Template Bindings / Animator | 256 |
| Layers / Animator | 64 |
| BlendGroups / Layer | 64 |
| Instances / BlendGroup | 256 |
| mask entries | 256 |
| total animation tracks / prepared content | 65,536 |
| total segments + steps / prepared content | 1,048,576 |
| generated records / prepared content | 100,000 |
| Property Writes / frame | 600,000 |

prepared-content 总量包含 imported source Clip、Chart-local Clip，以及每个 Binding lowering 后的
concrete Clip；未绑定 import 也计入。resolver 使用 checked accumulation，并在超限或溢出时以
`chart.animation.generated_limit` / `cxt.budget.exceeded` 失败。Stage 4 不得再次展开 Binding，且
compiled representation 的数量必须受这些已验证总量和 Property Write 上限约束。

### 6.2 CXC

| 预算 | 默认上限 |
| --- | ---: |
| package / listed entry bytes | 512 MiB |
| one entry | 64 MiB |
| manifest | 1 MiB |
| entries including manifest | 65,534 |
| portable path bytes | 4,096 |
| path depth | 64 |
| diagnostics | 1,024 |

CXC 预算只属于 source/package preparation。Stage 4 不接收 archive bytes、entry views 或 manifest，
也不得把 package 大小当作 animation runtime 预算。

## 7. Diagnostics、排序和截断

格式 handoff 前的稳定诊断至少包括：

```text
chart.animation.clip_invalid
chart.animation.reference_missing
chart.animation.template_reference_missing
chart.animation.template_binding_conflict
chart.animation.track_conflict
chart.animation.mask_conflict
chart.animation.additive_unsupported
chart.animation.discrete_weight_unsupported
chart.animation.generated_limit
cxt.format.unsupported
cxt.version.unsupported
cxt.template.invalid
cxt.template.id_mismatch
cxt.import.missing
cxt.import.duplicate
cxt.budget.exceeded
playback.capability.unsupported
```

`core::Diagnostics::sortDeterministically()` 按
`(fieldPath, severity, code, message)` 稳定升序。bounded diagnostics 到达容量后停止接收，并用调用方
提供的 sentinel 替换最后一个已接受项；Chart sentinel 为 `chart.diagnostics.limit_exceeded`，CXC
使用 `cxc.budget.exceeded`。`maxDiagnostics == 0` 在 Chart 入口以 `chart.limits.invalid` 提前拒绝。

Stage 4 必须保留 prepare 阶段已有 diagnostics 顺序和上下文，不得按地址或遍历顺序重排。G2 不预留
新的 animation runtime code；Stage 4 若增加 compile/update diagnostics，必须先冻结 machine-readable
code、identity context、排序和 truncation 测试。运行期错误不得伪造 JSON field path；能在 resolver
发现的格式错误仍由 resolver 拥有。

## 8. Stage 4 运行时职责

Stage 4 接收 typed program 后负责：

1. 把 record identity 编译为 owning、可重复 seek 的 runtime lookup；
2. 从绝对 Chart/Session 时间得到 local Beat，并实现 start、durationScale、iterations、fill 和
   discontinuity 重建；
3. 对 continuous/step Track 进行确定性采样；
4. 按 [Animation Mixing](../ANIMATION_MIXING.md) 实现 Override、受限 Additive、priority、weight、
   mask 和离散 winner；
5. 通过统一 PropertyResolver 合并 Initial、Behavior、Animation、HostOverride 和
   StudioPreviewOverride；
6. 把最终 transform/visibility/material 值写入既有 Runtime/FrameSnapshot 路径；
7. 保证 warmed update/extract 零分配或满足 Stage 4 明确冻结的有界分配合同；
8. 保持 prepare/reload/commit 事务性、跨 source semantic identity 和 external consumer 行为。

Stage 4 不得：

- 读取或迁移 JSON、CXC、CXT、Project 或 Asset Index；
- 重新解析 ChartParameter 或重新执行 Template Binding lowering；
- 执行运行时脚本、逐帧回调、表达式、bytecode 或离线 generator；
- 让 Host/Studio/Judgement 直接访问 World、EnTT 或最终 Component；
- 根据数组顺序、帧率或上一帧混合结果决定当前帧；
- 隐式增加格式字段、extension、capability、ABI 或 digest 版本。

## 9. 残余风险

| 风险 | Stage 4 关闭要求 |
| --- | --- |
| prepare staging / 当前成功的空 candidate 持有 program，但 commit 不保留它 | 在声明 capability 前完成 owning compiled state 的原子 commit/reload |
| generated `clip.id` 非全局唯一 | 所有 lookup 和 debug identity 使用 `AnimationRecordIdentity` |
| Beat 到 local Beat 的边界、负 start、有限最终边界 | exact-boundary、seek/stop/discontinuity 和不同帧率 golden |
| Quaternion additive / hemisphere 选择 | 按规范实现并固定 permutation golden |
| 离散 winner 和部分 weight | 最大 instance weight、最小 identity tie-break；Layer/Group 必须为 1 |
| mask prefix 展开与跨 Group/同 priority 冲突 | 保持 resolver 规则，runtime 不引入第二套冲突语义 |
| step material 资源 | prepare 闭包已包含 AnimationMaterial；runtime 只使用已准备的 resource identity |
| 最大合法 program 的内存和热帧成本 | 复用 F4 最大内容方法，记录 allocation、时间和内存趋势 |
| public capability 过早暴露 | 只有 headless/Player/external consumer 全链路成功后加入默认支持集合 |
| diagnostics 来源不完整 | compile/update code 必须带 record/object identity，并有稳定排序/截断测试 |

## 10. Stage 4 验收入口

Stage 4 开始后，首个实现批次至少满足以下入口门禁：

- `cuexis_animation` 和任何新 target 已注册到 active target/依赖 allowlist，且不依赖 JSON、CXC、
  CXT、SDL、OpenGL 或平台 backend；
- AnimationSystem 只消费 typed/compiled program，并通过绝对时间 API 求值；
- `chart_v4_animation.json`、CXT Binding 和 template animator 可以通过 Playback prepare/commit/update/
  extract，且不再由 capability preflight 拒绝；
- static/shared、filesystem/CXC file/CXC memory/typed source、headless/Player/external consumer 输出一致；
- seek、stop、reload failure、time discontinuity、不同帧率和数组 permutation 结果一致；
- FrameSnapshot 现有 transform、visible、material、opacity、tint 字段足以观察全部 v1 动画结果；
- invalid format 仍在 Stage 4 前失败，runtime 防御性 invalid typed input 有独立稳定诊断；
- warmed path、最大 program、600,000 write 上限和 sanitizer/analysis 门禁通过；
- `cuexis.animation.clip.v1` 与 `cuexis.animation.layers.v1` 只在上述能力真实可用后加入 Playback 支持。

## 11. G2 结论

CFU-G2 typed handoff 已完成。Stage 4 获得一个确定性、owning、参数已冻结、CXT 已 lower、资源和预算
已校验的 `AnimationProgramInput` 合同，以及 capability、fixture、diagnostics、验收入口和风险清单。
格式阶段不实现求值系统，Stage 4 仍未开始；CFU-G 下一检查点是 G3 final candidate validation，之后
仍需 completion report 和项目所有者接受。
