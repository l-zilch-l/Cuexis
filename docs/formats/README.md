# Cuexis Format Index

状态：现行格式索引

更新日期：2026-09-07

## Artifact 分层

所有 Chart 格式都遵守
[音乐游戏玩法抽象模型](../architecture/GAMEPLAY_ABSTRACTION_MODEL.md)定义的语义边界：
格式版本可以改变字段合同和物理编码，但不得把某一种轨道、Note 或渲染方式误认为
音游的核心语义。

```text
Source Project
  cuexis.project.json + Asset Index + Chart/CXT JSON + source/imported assets

CXT source
  作者层 Prototype/Slot/Binding/Pattern，经有限展开得到 Canonical Semantic Chart

Packed Chart
  对具体语义做字典、实体索引和字段差异编码；物理版本独立于 Chart 语义版本

CXC Exchange Package
  自包含、只读、可验证的交换和部署包；合同与实现已通过 CFU-F，CFU-G hosted 验证与 G6 封存已完成

Compiled Runtime
  ChartRuntime、AnimationProgram、World 和缓存，不是持久化交换格式
```

## 权威矩阵

| 内容 | 权威文档 | 状态 |
| --- | --- | --- |
| ProjectConfig v1 | [ADR 0025](../adr/0025-project-config-v1-and-path-security.md) | implemented |
| Asset Index v1/v2/v3 | [ADR 0026](../adr/0026-asset-index-and-source-resolution.md)、ADR 0031 与 [MATERIAL_SHADER.md](MATERIAL_SHADER.md) | implemented; v3 adds `shader` |
| Chart v1/v2/v3 | [CHART_FORMAT.md](CHART_FORMAT.md) | implemented |
| Chart v4 | [CHART_V4_FORMAT.md](CHART_V4_FORMAT.md) | accepted and implemented; C1–C4, CFU-D/E/F/G gates closed; Stage 4 animation runtime closed |
| Chart v5 | [Chart v5 工作包](../stage_plans/active/chart-format-update-for-v5/plan.md) | candidate；Foundation 先交 Core，Stage 6 candidate 消费，Stage 8 正式发行；40k/16 MiB 为关闭门禁 |
| Packed Chart v1 | [PACKED_CHART_FORMAT.md](PACKED_CHART_FORMAT.md) | candidate；Header/目录、字典/身份、Archetype 与实体差异流、无损 Beat；未实现 |
| CXC v1 | [CXC_FORMAT.md](CXC_FORMAT.md) | accepted and implemented internally; archive/tools and Playback source/prepare/identity gates closed; no public CXC package API |
| CXT v1 | [CXT_FORMAT.md](CXT_FORMAT.md) | accepted contract; Reader/Writer/lowering and prepare import/lookup implemented; CFU-F and G4 hosted gates closed; Stage 4 animation execution closed |
| CXT v2 | [CXT_V2_FORMAT.md](CXT_V2_FORMAT.md) | candidate；Foundation F2 integer/beat reader/expander 已实现；Animation Extension 在 Stage 8 收敛；非生产 Schema，未接入默认 Playback |
| Chart v6 / Model v1 | [chart-format-update-for-v6 plan](../stage_plans/deferred/chart-format-update-for-v6/plan.md) | deferred；静态 glTF 2.0、内置网格、默认扁平片、submesh 槽；无生产 Spec |
| Chart v7 | [chart-format-update-for-v7 plan](../stage_plans/deferred/chart-format-update-for-v7/plan.md) | deferred；曲线形变（最多两轴；仅贝塞尔）、line、`shader.json` 接口；后处理不实现；无生产 Spec |
| Chart v8 | [chart-format-update-for-v8 plan](../stage_plans/deferred/chart-format-update-for-v8/plan.md) | deferred；双轴贝塞尔、模型基本动画、内置后处理；无生产 Spec |
| Animation Mixing | [ANIMATION_MIXING.md](ANIMATION_MIXING.md) | accepted contract; format-stage gates closed; Stage 4 runtime closed |
| Portable Presentation v1 | [PORTABLE_PRESENTATION.md](PORTABLE_PRESENTATION.md) | implemented |
| Material/Shader v1 | [MATERIAL_SHADER.md](MATERIAL_SHADER.md) | accepted contract; S5-A through S5-H completed; Stage 5 closed and merged into `master` 2026-08-28 |

ADR 记录选择理由，格式文档记录字段和语义。手写 v4 谱面可先阅读
[Chart v4 谱面编写指南](../guides/CHART_V4_AUTHORING.md)。CXC 不重新定义 Chart/CXT；CXT 不重新定义
ChartParameter、Template Binding 或 Animator；Animation Mixing 不重新定义序列化字段。

## 生产与实施边界

Playback 继续保留 `cuexis.chart` v1/v2/v3 生产路径，并已能 prepare 静态/参数化 v4 与求值非空合法
动画。S4-F 已把 animation capability 加入默认 Playback 集合；S4-G 已关闭本地安全、分配与性能门禁；
S4-H 已关闭 hosted 验收，Stage 4 已完成。CFU-C1/C2 已提供 CXC manifest、Chart v4 和 CXT 的生产 Schema、内部 typed
source Reader、Chart/CXT canonical Writer、参数解析/identity、CXT import 与 deterministic
lowering。CFU-C3 已提供内部 strict ZIP32 archive/package、owning file/memory loader、
package-backed Asset ContentProvider 和独立 project-document table；CFU-C4 已提供
developer pack/validate/unpack tools 和 round-trip gates。CFU-D1/D2 已关闭：显式
`migrateToV4` / `--target 4` JSON lift，默认 CLI 仍输出 v3。CFU-E 已关闭公共 API、source
factory、prepare/capability 与 `PreparedSemanticIdentity`。CFU-D3 已关闭 Playback
FrameSnapshot / FrameDigest v3 / seek-stop 等价。整包 CFU-D 已于 2026-08-14 经项目所有者
记录“未提供外部资产”并关闭；兼容窗口不缩短。CFU-F 已关闭最终实现 SHA 的 hosted consumer、
确定性、安全与性能门禁；CFU-G hosted 验证、completion report 和 owner acceptance 已完成，格式阶段已封存。
该检查点仍不是完整 CXC 产品支持
或公共 package API。评审示例位于
[examples/chart_format_update](../examples/chart_format_update/README.md)。

Chart v5 与 CXT v2 仍是候选，正式发行归入 Stage 8。Foundation accepted 后，Stage 6 可以
通过显式 candidate path 加载受支持的 Chart v5 Core/Packed subset；在 Stage 8 关闭并经 owner
acceptance 前，默认发行基线仍是 Chart v4、CXT v1 和 CXC v1，默认 Writer 与正式 Playback
entry 不切换到 v5。
Chart v5 的发行入口必须是经过验证的 Packed Chart；Packed Chart 不是独立语义版本。
Chart v5 的 CXT v2 合同见 [CXT_V2_FORMAT.md](CXT_V2_FORMAT.md)，物理与容量细节见
[PACKED_CHART_FORMAT.md](PACKED_CHART_FORMAT.md)。40,000 个展开语义实体和 16 MiB
Packed entry 按明确的复杂度 profile 验收，decoded/峰值内存和资源闭包另有预算。
CXT 参数在编译时冻结，更换参数需要新的 Packed artifact，不在发行 Playback 重跑模板。
天空盒、模型和形变归入 Stage 9 Presentation 扩展，不作为普通 Chart Object。
Chart v6 / Model v1 见 [延期计划](../stage_plans/deferred/chart-format-update-for-v6/plan.md)，
作为 Stage 9 设计输入；Chart v7 作为 Stage 9 形变设计输入；Chart v8 作为 Stage 11
高级表现设计输入。

运行时脚本和逐帧脚本回调无限期延后，不是任何 Cuexis 格式的隐藏扩展点。
