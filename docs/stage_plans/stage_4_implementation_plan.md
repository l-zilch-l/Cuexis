# Stage 4 Implementation Plan: Cuexis Presentation Animation

状态：future；S4-A、S4-B、S4-C、S4-D、S4-E、S4-F 与 S4-G 本地门禁已通过，Stage Chart Format Update 已关闭并已解锁；整体仍为 unblocked / not started

更新日期：2026-08-27

归档来源：[旧版 PROJECT_GUIDE](../archive/PROJECT_GUIDE_LEGACY_2026-08-10.md) 与
[SDK transition plan 快照](../archive/CUEXIS_SDK_TRANSITION_PLAN_2026-08-10.md)。typed 输入、capability、
fixture、预算和残余风险以
[CFU-G2 Stage 4 typed handoff](../stage_reports/260816-chart-format-update-g2-stage4-handoff.md) 为准。

## 1. 阶段目标

实现 Cuexis 谱面和资源预览所需的确定性表现动画，使 Behavior、Animation、宿主覆盖和 Studio
预览通过统一属性求值路径协作。本阶段把格式阶段交付的 `AnimationProgramInput` 编译为 owning
runtime program，按绝对时间采样，并写入既有 FrameSnapshot 观察面。

本阶段不建设通用角色状态机、骨骼动画控制器、游戏对象脚本系统或运行时脚本。

## 2. 前置条件

- Stage Chart Format Update 的 Chart v4、CXT、Clip、Binding、Property ID、mask 和 lowering 合同关闭。
- Stage 4 只消费格式阶段交付的 typed 数据，不读取 JSON、CXC 或 CXT。
- [ANIMATION_MIXING.md](../formats/ANIMATION_MIXING.md)、[ADR 0009](../adr/0009-property-evaluation-and-conflicts.md)
  与 [ADR 0019](../adr/0019-animation-layers-and-runtime-overrides.md) 的混合和覆盖语义保持权威。
- [CFU-G2 Stage 4 typed handoff](../stage_reports/260816-chart-format-update-g2-stage4-handoff.md)
  已冻结 `AnimationProgramInput`、capability、fixture、预算、diagnostics、所有权和验收入口；项目所有者
  已接受 completion report，Stage 4 已解锁但尚未开始。
- [CFU-G4 hosted verification](../stage_reports/260824-chart-format-update-g4-hosted.md)
  已记录同 SHA Linux/MSVC/MinGW 全部成功；G5 report-SHA hosted revalidation 与 G6 owner acceptance
  已完成。

上述前置条件在 2026-08-24 均已满足。本计划保持 `future`，状态为 unblocked / not started。

## 3. 当前基线

仓库在 Stage 4 开工前的事实入口如下。批次必须接到这些入口，不得另起第二套求值或解析路径。

```text
cuexis_animation 仍是未接入构建图的 INTERFACE stub，不在 CUEXIS_ACTIVE_TARGETS
ChartV4Resolver::resolve() 已返回 owning AnimationProgramInput
Playback prepare 把 program 放入 PreparedPlayback staging，commit 不保留它
默认 Playback capability 含 cuexis.chart.v4 / source.cxc.v1 / source.cxt.v1
任意非空 Clip/CXT/Binding/Layer 在 preflight 以 playback.capability.unsupported 失败
RuntimeSession::updatePrepared() 只调用 BehaviorSystem，再提交 Transform/Camera/Appearance
world::TransformPropertyResolver 只覆盖 Transform；可见性与材质走独立 Appearance 路径
FrameSnapshot 已有 transform、visible、material、opacity、tint；FrameDigest 保持 v3
HostOverride / StudioPreviewOverride / BasePropertyCommand 尚无 runtime 实现
```

公开观察面继续是 `PlaybackSession`、`FrameSnapshot` 和 `FrameDigest` v3。宿主不得因本阶段获得
World、EnTT、RuntimeSession 或 JSON DOM。

## 4. 实施范围

- 把 `cuexis_animation` 建成真实静态库，并注册 active target / 依赖 allowlist。
- 把 `AnimationProgramInput` 一次性编译为 owning、可重复 seek 的 runtime program。
- 实现 Clip 局部 Beat、`durationScale`、`iterations`、`fillMode` 和 discontinuity 重建。
- 对 continuous Track 与 step Track 做确定性采样；lookup 使用 `AnimationRecordIdentity`。
- 实现 Layer、BlendGroup、weight、priority、mask，以及 Override 与受限 Additive。
- 通过统一 PropertyResolver 合并 Initial、Behavior、Animation、HostOverride 和
  StudioPreviewOverride。
- 实现 HostOverride 与 StudioPreviewOverride Token，以及 `BasePropertyCommand`。
- 把 compiled animation state 纳入 prepare/commit/reload 事务；失败不改 active 状态。
- 在调试快照中公开属性来源、Layer、权重和最终贡献，且不泄漏 World/EnTT。
- 为安全上限、诊断截断和 warmed 热路径分配建立测试；应用配置不得改变动画语义。

字段、混合公式、预算数字和 fixture 清单不在本计划复制。权威分别是 Chart v4 / CXT / Mixing
规范与 G2 handoff。

## 5. 验收标准

- AnimationSystem 不依赖 Chart 文档、JSON、CXC、CXT、SDL 或图形后端。
- BehaviorSystem 和 AnimationSystem 可以同时作用，并按已冻结顺序提交到 PropertyResolver。
- 冲突属性只通过显式 blend mode、priority 和 mask 处理，不依赖 Entity 遍历顺序。
- 相同 Animation/Animator 数据和相同显式输入在 Seek、Stop、reload 和不同帧率下结果一致。
- HostOverride 或 StudioPreviewOverride 结束后，属性恢复为当前下层求值结果。
- 宿主只通过稳定 ObjectId、PropertyId 和 OverrideToken 操作，不访问 World 或最终 Component。
- warmed update/extract 路径满足阶段冻结的零分配或有界分配要求。
- headless Playback、Player 和 external consumer 对相同动画输入产生相同 FrameSnapshot/digest。
- `cuexis.animation.clip.v1` 与 `cuexis.animation.layers.v1` 只在上述能力真实可用后加入默认
  Playback 支持集合。
- invalid format 仍在 Stage 4 前失败；runtime 防御性 invalid typed input 有独立稳定诊断。

## 6. 明确不包含

- 运行时脚本、逐帧脚本回调、任意表达式执行或通用状态机。
- Shader、粒子、UI、骨骼动画和通用游戏对象生命周期。
- 在 `engine/animation/` 内解析或迁移 JSON、CXC、CXT、Project 或 Asset Index。
- 重新解析 ChartParameter 或重新执行 Template Binding lowering。
- 新增格式字段、extension、capability 名称、FrameDigest 版本或稳定 C ABI。
- 公共 CXC package API，或把 `cuexis_cxc` 提升为安装组件。
- Studio 编辑器、Judgement/Replay，或把 Preview Override 序列化进 Chart。

## 7. 批次顺序

只允许按批次推进。未关闭的批次不得把局部产物描述为完整 v4 动画 Playback。默认 Session 在
S4-F 前必须继续拒绝非空动画。

```text
S4-A  模块接线、compile 合同和诊断冻结
S4-B  owning compile 与绝对局部时间采样
S4-C  Layer / BlendGroup 混合
S4-D  PropertyResolver、Override Token 与 Runtime 编排
S4-E  Playback 事务所有权
S4-F  consumer、确定性与公开 capability
S4-G  安全、分配与性能
S4-H  hosted 验收与阶段关闭
```

### 7.1 S4-A：模块接线与 compile 合同

零功能启动门禁。不放开 Playback capability，不改变非空动画拒绝路径。

任务：

1. 把 `engine/animation/` 从 INTERFACE stub 改为真实 `STATIC` 库，并加入 `engine/CMakeLists.txt`。
2. 将 `cuexis_animation` / `cuexis_animation_tests` 写入 `CUEXIS_ACTIVE_TARGETS` 和
   `cuexis_verify_target_dependencies`。
3. 冻结允许依赖：`cuexis_core`、`cuexis_world`，以及读取 typed `AnimationProgramInput` 所需的
   `cuexis_chart`。禁止 JSON、CXC、CXT 解析、SDL、OpenGL、AudioSDL 和 Playback 公共头反向依赖。
4. 冻结 compile API：输入为 `AnimationProgramInput` 或一次性编译出的 owning program；输出不得
   借用 resolver artifact、JSON 文本或 CXC package。
5. 冻结 lookup 键为 `AnimationRecordIdentity`。禁止把 generated `clip.id` 当作全局唯一键。
6. 冻结本阶段新增的 machine-readable compile/update diagnostic code、identity context、排序和
   truncation sentinel。格式层已有 code 仍由 resolver/Playback preflight 拥有。
7. 扩展 architecture tests：`engine/animation/` 不得出现 nlohmann JSON、CXC archive、SDL 或
   `glad/GL` 头。

退出门禁：目标可配置可链接，architecture/allowlist 测试通过；Playback 默认 capability 与空动画
回归保持不变。

实施快照（2026-08-24）：`cuexis_animation` 已从 INTERFACE stub 改为 STATIC 库并注册 allowlist。typed
输入生产定义移到 `animation_program_input.hpp`，resolver 继续返回同一 `AnimationProgramInput`。
compile API、`AnimationRecordIdentity` lookup、compile/update diagnostic code 和 truncation
sentinel 已冻结。默认 Playback capability 与非空动画拒绝路径未改。证据见
[S4-A 报告](../stage_reports/260824-stage-4-s4-a-module-wiring.md)。

### 7.2 S4-B：owning compile 与绝对采样

任务：

1. 编译 Clip/Layer/Group/Instance 为可重复 seek 的 runtime lookup，保留 resolver 已确定的排序。
2. 由 Runtime 提供的绝对 Chart/Session 时间生成 `localSampleTime`；AnimationSystem 不读取
   Chart、CXT 或 TimingMap。
3. 实现 `startBeat`、`durationScale`、有限/无限 `iterations` 和 `none/hold` fill。
4. 对 continuous segment 按冻结曲线合同采样；对 step Track 按绝对 local Beat 重建当前值。
5. `timeDiscontinuity`、Seek 和 Stop 后从绝对时间重建，不使用上一帧混合结果。
6. 用手工构造的 typed program 覆盖 exact-boundary、负 start、有限最终边界和不同帧率；合法生产
   fixture 只通过 Chart resolver 进入，不在 animation 模块解析 JSON。

退出门禁：相同 program 与相同显式时间在 Seek/逐帧到达/不同帧率下采样值一致；缺失 clip identity
等防御性错误有稳定 code。

实施快照（2026-08-25）：`AnimationSampler` 从显式 Chart Beat 生成局部 Beat，并覆盖
`startBeat`、`durationScale`、有限/无限 `iterations`、`none/hold` fill、continuous Hermite 与
step Track。Seek/Stop/不同帧率从目标 Beat 重建。默认 Playback capability 与非空动画拒绝路径未改。
证据见 [S4-B 报告](../stage_reports/260825-stage-4-s4-b-absolute-sampling.md)。

### 7.3 S4-C：Layer 混合

任务：

1. 按 Mixing 合同实现 Override Group：线性属性两步 lerp，quaternion shortest-path slerp，离散
   winner 为最大 instance weight、相同 weight 取最小 `instanceId`。
2. 实现受限 Additive：position 加权 delta、正 scale product、rotation 切空间加权后乘到 base。
3. Layer/Group weight 为 0 时不写入；Additive `effectiveWeight` 已含 Group/Layer weight，不再二次
   Layer 混合。
4. 保持 resolver 的 mask / 同 priority 不重叠规则；runtime 发现非法重叠时丢弃该冲突层写入并报告
   错误，不得用数组顺序或 Entity 遍历破平。
5. 固定 quaternion hemisphere 与 Additive permutation golden。

退出门禁：Behavior 之后的 Animation 输出只通过 `PropertyWriteBuffer`；结果不依赖插入顺序。

实施快照（2026-08-26）：`AnimationMixer` 按 Mixing 合同混合 Override/Additive Layer，写入
`PropertyWriteBuffer`。quaternion hemisphere 与 Additive permutation golden 不依赖插入顺序。
默认 Playback capability、非空动画拒绝路径和 Runtime 编排未改。证据见
[S4-C 报告](../stage_reports/260826-stage-4-s4-c-layer-mixing.md)。

### 7.4 S4-D：PropertyResolver、Override 与 Runtime 编排

任务：

1. 把当前 `TransformPropertyResolver` 加 Appearance/Camera 分支，收敛为统一 PropertyResolver。
   固定顺序为 Initial -> Behavior -> Animation Layers -> HostOverride -> StudioPreviewOverride。
2. 任何层不得绕过 Resolver 直接写同一可绑定属性。
3. 实现 `OverrideToken`：`ownerId`、priority、propertyMask、lifetime、writes。Token 释放或
   lifetime 结束后下一帧从下层重新求值。相同 priority 写同一属性是运行时错误，冲突写入被丢弃。
4. StudioPreviewOverride 使用同一协议，但只存在于 Studio/preview Session，不进入 Chart 或
   FrameDigest 语义。
5. 实现 `BasePropertyCommand`：只在 RuntimeSession 主线程安全点修改初始属性并递增
   `baseRevision`，随后从新基线完整求值。
6. 在 `RuntimeSession::updatePrepared()` 中于 Behavior 求值之后、Resolver commit 之前调用
   AnimationSystem。
7. 扩展内部 debug snapshot：Initial、Behavior、各 Layer、Host/Preview Override、权重、mask、
   冲突和最终值。该快照不得进入安装公共头，也不得泄漏 World/EnTT。

退出门禁：Behavior 与 Animation 同时作用；Override 结束后恢复下层结果；debug 记录有界且可截断。

公开 HostOverride API 只允许稳定 `ObjectId`、`PropertyId` 和 `OverrideToken`。若必须新增 Playback
头，保持 SDK API `0.6.0` 的 additive 预览合同，不升级 FrameDigest，不引入稳定 C ABI。

实施快照（2026-08-27）：统一 `PropertyResolver` 按 Initial -> Behavior -> Animation ->
HostOverride -> StudioPreviewOverride 提交；`RuntimeSession::updatePrepared()` 在 Behavior 之后、
commit 之前调用 `AnimationSystem`。`OverrideToken` 覆盖 lifetime、mask、同 priority 冲突丢弃；
`BasePropertyCommand` 在主线程安全点改 Initial 基线、递增 `baseRevision` 并从新基线完整求值。
内部 debug snapshot 记录各层值、Animation Layer 权重/mask 与冲突，不进入安装公共头。默认
Playback capability 与非空动画拒绝路径未改。证据见
[S4-D 报告](../stage_reports/260827-stage-4-s4-d-property-resolver.md)。

### 7.5 S4-E：Playback 事务所有权

任务：

1. prepare candidate 在 capability preflight 之后编译并拥有 animation state；不得让 active runtime
   借用 `ChartV4ResolvedArtifact` 或 `PreparedPlayback` 内存。
2. commit 原子替换 active compiled animation state；失败 prepare/reload 不改变 Runtime、identity、
   diagnostics、frame 或 animation state。
3. 空 program 继续走现有静态 v4 路径，且 commit 后仍可观察。
4. 默认 `allCapabilities()` 仍不含 animation capability。需要求值的测试构造显式
   `PlaybackCapabilitySet`，证明事务路径而不提前对外部宿主宣称支持。
5. 保留跨 source `PreparedSemanticIdentity`；animation 求值不得改写 identity 输入集合。

退出门禁：opt-in capability 下，`chart_v4_animation.json`、CXT Binding 和 template animator 可
prepare/commit/update/extract；失败 reload 保持 active 状态。默认 Session 仍拒绝非空动画。

实施快照（2026-08-27）：Playback prepare 在 capability preflight 之后编译 owning
`AnimationProgram`，并把它交给 candidate RuntimeSession。commit 原子替换 active Runtime；失败
prepare/reload 不改变 identity、frame 或 animation state。空 program 继续静态 v4 路径。默认
`allCapabilities()` 仍不含 animation capability。证据见
[S4-E 报告](../stage_reports/260827-stage-4-s4-e-playback-ownership.md)。

### 7.6 S4-F：consumer、确定性与公开 capability

只有 S4-B 到 S4-E 的求值、事务和 FrameSnapshot 观察通过后，才能把下列常量加入默认支持集合：

```text
cuexis.animation.clip.v1
cuexis.animation.layers.v1
```

任务：

1. 在 Playback 公共头增加对应 `string_view` 常量，并写入 `allCapabilities()`。
2. 复用 G2 合法 fixture 与 CFU-F consumer 路径：filesystem、CXC file、CXC memory、typed
   project-document。
3. 固定 Seek、Stop、reload failure、time discontinuity、不同帧率和数组 permutation 的
   FrameSnapshot / FrameDigest v3 golden。
4. headless Playback、Player 与只链接 `cuexis::playback` 的 external consumer 对相同动画输入输出
   一致。
5. 非法 format fixture 继续在 Reader/resolver/package 失败；不得为了测 AnimationSystem 而在
   `engine/animation/` 解析它们。

退出门禁：默认 Session 接受非空合法动画；缺少 animation capability 的显式裁剪 Session 仍稳定拒绝。

实施快照（2026-08-27）：公开头增加 `capabilityAnimationClipV1` / `capabilityAnimationLayersV1`，
并写入默认 `allCapabilities()`。默认 Session 接受合法非空动画；显式裁剪 Session 仍以
`playback.capability.unsupported` 拒绝。filesystem / CXC file / CXC memory / typed
project-document 对同一 CXT 输入输出相同 `PreparedSemanticIdentity` 与 FrameDigest v3。
CFU-F1/F2 失败 reload 改用裁剪 capability，以保持 CFU-F3 diagnostic golden。FrameDigest 保持
v3，SDK API 保持 `0.6.0`。证据见
[S4-F 报告](../stage_reports/260827-stage-4-s4-f-consumer-capability.md)。

### 7.7 S4-G：安全、分配与性能

任务：

1. compiled representation 受 G2 已校验的 Clip/Track/Segment/generated record 总量和每帧 600,000
   Property Write 上限约束。
2. warmed `update()` 与复用 `FrameSnapshot` 的 `extractFrame()` 为零新增分配，或满足本阶段书面冻结
   的有界分配合同；应用配置不得改变动画语义。
3. 复用 F4 最大内容方法，记录最大合法 program 的 allocation、热帧时间和进程内存趋势，不设机器
   相关硬阈值。
4. 诊断到达容量后停止接收，并用 sentinel 替换最后一项；运行期错误不得伪造 JSON field path。

退出门禁：allocation/limit 测试、sanitizer 和既有 architecture/package 门禁通过。

实施快照（2026-08-27）：compiler 按 ChartLimits 与每帧 600,000 write 上限拒绝越界 program；诊断满容量后
以 `animation.diagnostics.limit_exceeded` sentinel 截断，field path 使用 clip 路径或
`$/animationProgram`。空 Chart v1–v4 与 Stage 2/3 warmed 热路径保持零新增分配；非空 CXT 冻结为
连续两窗口 `second <= first` 的有界合同。F4 风格探针默认跳过，`CUEXIS_RUN_PERFORMANCE_PROBE=1`
记录趋势、不设机器硬阈值。Linux sanitizer 留在 S4-H。证据见
[S4-G 报告](../stage_reports/260827-stage-4-s4-g-safety-allocation.md)。

### 7.8 S4-H：hosted 验收与关闭

任务：

1. 最终 SHA 运行 Debug/Release fresh configure、clean build、完整 CTest、format、architecture、
   public-header ASCII、version、license 和 `git diff --check`。
2. 运行 hosted Linux Quality、Windows MSVC、Windows MinGW；记录 run URL 和第一失败步骤。
3. 创建 Stage 4 completion report，只记录实现与证据，不复制 Mixing/Chart 字段合同。
4. 项目所有者接受报告后，才把本计划与 `CURRENT_STATUS.md` 改为 completed；Stage 5 才能标为
   unblocked。

退出门禁：三平台 hosted 成功，owner acceptance 已记录。关闭本阶段不表示 Shader 管线、Studio 或
Judgement 已开始。

## 8. 模块、target 与文件落点

依赖方向：

```text
cuexis_chart
  继续拥有 AnimationProgramInput 的生产定义
  -X-> cuexis_animation / runtime / world

cuexis_animation
  -> cuexis_core + cuexis_world + typed chart input
  -X-> JSON / CXC / CXT / SDL / OpenGL / AudioSDL / Playback public headers

cuexis_runtime
  私有依赖 cuexis_animation
  在 Behavior 之后、Resolver commit 之前求值

cuexis_playback
  继续私有依赖 runtime/chart
  公开门面只增加 ObjectId/PropertyId/OverrideToken 所需的 Cuexis-owned 类型
```

建议文件布局（确切文件名可按现有 snake_case 调整，职责不得移动）：

```text
engine/animation/CMakeLists.txt
engine/animation/include/cuexis/animation/animation_program.hpp
engine/animation/include/cuexis/animation/animation_system.hpp
engine/animation/src/animation_compiler.cpp
engine/animation/src/animation_sampler.cpp
engine/animation/src/animation_mixer.cpp

engine/world/.../property.hpp          统一 Resolver 与 Override 写入
engine/runtime/src/runtime_session.cpp 编排 Behavior + Animation + Resolver
engine/playback/src/playback_session.cpp
  compile/own/commit animation state；默认 capability 仅在 S4-F 追加

tests/animation/
tests/runtime/
tests/playback/
```

`cuexis_animation` 不是安装 package component。shared Playback 把实现私有链接进 Playback；静态
consumer 只看到 `Cuexis::Playback`。

## 9. 公共 API 与版本

- FrameDigest 保持 v3；不因动画结果增加 digest 字段。现有 Snapshot 的 transform、visible、
  material、opacity、tint 足以观察 v1 动画。
- SDK API 保持 `0.6.0`，除非公开头不得不新增 HostOverride 合同。若新增，使用 additive overload
  或独立 owning type，不给现有 aggregate 直接追加字段。
- 稳定 C ABI 仍延期到 Stage 12。
- 日期构建版本只通过 `tools/update_version.py` 在 merge/release 门禁更新。
- 默认 capability set version 保持 1；只追加两个已冻结的 animation capability 名称，不新增第三个
  animation capability。

## 10. 测试与证据

首批测试直接复用 G2 列出的生产 fixture，不复制为新的 JSON/CXT 变体。animation 单元测试使用手工
typed program；Playback/consumer 测试通过 public prepare 路径加载合法 fixture。

最低证据矩阵：

```text
compile/identity lookup
absolute local Beat, fill, iterations, discontinuity
Override / Additive / discrete winner / mask
Behavior + Animation 同帧
HostOverride lifetime 与冲突丢弃
prepare/commit/failed reload 事务
filesystem / CXC file / CXC memory / typed source 一致性
headless / Player / external consumer FrameDigest v3
warmed allocation 与 600,000 write 上限
Linux ASan/UBSan、clang-tidy、MSVC/MinGW hosted
```

## 11. 残余风险关闭要求

G2 §9 的风险在对应批次关闭，不能带到 S4-H：

| 风险 | 关闭批次 |
| --- | --- |
| commit 不保留 prepared animation state | S4-E |
| generated `clip.id` 非全局唯一 | S4-A/S4-B |
| Beat 边界、负 start、不同帧率 | S4-B/S4-F |
| Quaternion additive / hemisphere | S4-C |
| 离散 winner 与部分 weight | S4-C；非法输入仍归 resolver |
| runtime 第二套 mask 冲突语义 | S4-C 只做防御，不改格式规则 |
| step material 资源 identity | S4-D/S4-E 只使用已准备资源 |
| 最大 program 成本 | S4-G |
| 过早公开 capability | S4-F 之前默认集合不加 animation |
| 运行期诊断缺 identity / 截断 | S4-A 冻结，S4-G 验证 |

## 12. 阶段退出条件

Stage 4 只有在下列全部成立后才能标为 completed：

1. S4-A 到 S4-G 退出门禁关闭。
2. 默认 Playback Session 可求值非空合法 Chart v4 / CXT Binding 动画。
3. 宿主观察面仍只有 Playback/FrameSnapshot/digest；World/EnTT 未泄漏。
4. hosted Linux Quality、Windows MSVC 和 Windows MinGW 在同一最终 SHA 成功。
5. completion report 经项目所有者接受。

关闭后的下一阶段是 [Stage 5](stage_5_implementation_plan.md)。Stage 5 在本阶段完成前保持
blocked。
