# chart-format-update-for-v5：Chart v5 谱面格式与旧版本退出计划

状态：active；已于 2026-09-02 启动

更新日期：2026-09-02

归档来源：[Stage Chart Format Update 完成计划](../../completed/chart-format-update/plan.md)、
[Chart v4 格式合同](../../../formats/CHART_V4_FORMAT.md)、[谱面格式审计记录](../../../stage_reports/reviews/stage-verification-2026-09/2026-09-01-findings.md)
与后续 [Stage 6 计划](../../future/stage-06/plan.md)。

本阶段负责下一代 Chart v5 的格式合同、动画旋转语义、作者可表达性、迁移、默认 Writer 切换以及旧格式的
分期退出。本计划只记录已确认方向、实施顺序和验收门禁；最终字段合同必须由独立 ADR 和生产 Spec
接受后才能实现。

## 1. 阶段目标

建立一个以 Chart v5 为唯一前进方向的谱面格式，并逐步停止对旧版本格式的长期维护。v5 应当：

- 让旋转事件直接表达负角度、小数角度和多圈旋转，不通过拆分为多个 Quaternion 事件实现。
- 让一段旋转的缓动作用于完整的未展开角度轨迹，而不是作用于 Quaternion shortest-path slerp。
- 让实体旋转和默认相机旋转使用同一套 Euler XYZ degrees 语义。
- 保持 Quaternion 作为 Runtime、World 和 Renderer 的内部姿态表示，但不把它作为 v5 作者层的唯一旋转曲线。
- 在不全局升级 Runtime 数值类型的前提下，为角度提供可验证的数值精度和范围合同；初步稳定表达目标为每轴
  `[-10000, 10000]` degrees，最终边界以精度、性能和安全测试证据冻结。
- 将对象透明度定义为作者层的整数 alpha `[0,255]`，运行时转换为 `[0,1]` 的连续值，并与材质/纹理 alpha
  的合成规则保持一致。
- 将 v5 设为默认写出格式；旧格式先进入只读兼容和迁移窗口，完成外部资产盘点后再删除旧 Reader、Schema
  和长期兼容路径。

## 2. 前置条件与恢复条件

- Stage 6 必须在本阶段完成并经 owner acceptance 后，以 v5 typed/portable 谱面作为 Playback、AnimationSampler、
  AnimationMixer、Player 和 external consumer 的新增能力基线；v4 合同只保留只读兼容和显式迁移窗口。
- 项目所有者接受一份新的 v5 ADR，明确旋转表示、角度范围、相机顺序、alpha 合成、Additive 语义、迁移
  等价性和旧版本退出窗口。
- 对仓库内 v1/v2/v3/v4 fixture、示例、CXT、CXC 和外部 consumer 进行资产盘点；没有外部资产清单和迁移
  报告，不得删除旧格式 Reader。
- 在改变默认 Writer 前，完成 v5 Schema、typed Reader/Writer、canonical identity、capability preflight
  和独立的 v4->v5 migration report。
- 在确定数值边界前，完成 float/double 采样基准、三角函数精度测试、极值测试和跨平台 golden 验证。
- 如果 v5 的多轴 Euler 语义无法在固定顺序下稳定覆盖目标内容，应恢复为“Euler 为默认、Quaternion 为
  显式 exact 模式”的双表示方案，而不是静默丢失 v4 旋转路径。

## 3. 已确认的 v5 方向

### 3.1 旋转和采样

v5 的实体和相机旋转以 signed decimal degrees 保存，允许负值和小数值；角度不取模、不隐式 unwrap，
端点值代表未展开的作者轨迹。旋转顺序、内禀/外禀定义、乘法方向和 local 坐标语义必须在 ADR 中写死，
不能只写 `XYZ`。

采样语义为：

```text
u       = HermiteProgress(localBeat)
angles  = startDegrees + (endDegrees - startDegrees) * u
quat(u) = ComposeFixedOrderXYZ(angles)
```

采样器必须依据绝对 local Beat 计算角度，再组合归一化 Quaternion；不得在 Reader 阶段把一个大角度事件
烘焙成多个 Quaternion 事件，也不得依赖上一帧累加状态。这样 Seek、Reload、暂停和不同帧率下的目标时刻
结果仍可复现。

`core::Quat`、TransformComponent、World PropertyResolver 和 Renderer 继续使用 Quaternion。需要改变的是
Chart typed model、动画曲线采样以及必要时的旋转混合合同，而不是删除 Quaternion 数学实现。

### 3.2 数值合同

- JSON Reader 可以使用 `double` 读取和验证角度，但 v5 不因作者字段而把整个 Runtime 数值系统升级为 double。
- v5 首要目标是每轴 `[-10000,10000]` degrees 内的稳定表达，包含负值、小数值、720°/7200° 多圈和全局缓动。
- 角度差值、中间乘法、`sin/cos` 参数约化、Quaternion 归一化和输出符号的确定性必须有明确合同。
- 超出稳定保证范围时必须稳定失败；不得声称 `DBL_MAX` 级别角度仍能保持相同小数精度。
- 位置、scale、FOV、alpha 和动画中间结果的存储精度、float 可表示性和溢出诊断在本阶段统一审计。

### 3.3 相机

v5 默认相机和实体旋转使用同一 Euler XYZ degrees 语义、同一旋转顺序和同一坐标空间。v4 的
`pitch/yaw/roll` 到 v5 的迁移必须执行显式的顺序转换，不能复制三个数值字段。

v5 `camera.fovY` 使用严格开区间 `(0,180)`。`180` 度仍然拒绝，因为透视投影在该边界退化；接近 180 度的
后端数值稳定性必须通过 capability/preflight 或稳定诊断表达，不能静默 clamp。

### 3.4 透明度和 alpha

v5 采用对象层整数 alpha `[0,255]` 作为作者层语义：`0` 完全透明，`255` 完全不透明。动画仍然在采样时
执行连续插值，随后归一化为 `[0,1]`，不产生阶梯式淡入淡出。

v5 必须明确对象 alpha、材质 base color alpha、纹理 alpha 和 `Opaque/Blend` alphaMode 的合成顺序。当前
v4 的 `material.opacity` `[0,1]` 是兼容输入，不应与 v5 alpha 字段同时隐式相乘两次；迁移器必须生成报告
并保留可追溯的源值。

### 3.5 混合和高级表示

- v5 Euler rotation 的 Override 路径必须支持多圈轨迹。
- v5 Euler rotation 不得直接复用当前 quaternion-log Additive 语义；初版可以稳定拒绝 Euler Additive，
  或另行定义角度增量累加和旋转顺序。
- 是否保留 v5 的显式 Quaternion exact 模式是 ADR 待决策项；若保留，它必须有独立的 interpolation mode，
  不得与 Euler 轨迹混用后再猜测作者意图。
- Override 混合可以在每个输入先采样为 Quaternion 后继续使用现有姿态混合，但必须文档化：混合多个来源
  时只保证当前姿态，不保证跨来源的累计转数。

## 4. v4 数值边界审计与 v5 处理方向

本阶段不应只修复旋转。下列 v4 限制必须进入 v5 的作者能力矩阵、Schema 诊断和测试：

- `transform.position` 和普通数值最终受 float 可表示范围约束；Reader、采样器和 World 提交阶段的边界必须统一。
- 普通 `transform.scale` 允许有限值，但 Additive scale 要求严格正因子；是否允许镜像和零缩放需要明确决定。
- `camera.fovY` 从 v4 `(0,179)` 调整为 v5 `(0,180)`，并覆盖默认相机和实体相机组件。
- v4 的 `material.opacity` 和 `material.tint` 使用 `[0,1]`；v5 改用明确的 alpha 作者单位，并决定 tint 是否保持
  normalized linear RGB。
- Hermite slope 目前要求非负且总和不超过 3，进度会 clamp 到 `[0,1]`；v5 必须明确这是否仍是唯一曲线合同，
  以及 bounce、overshoot、往返和非单调曲线如何表达。
- Rational Beat 受 numerator/denominator 预算影响，采样阶段又会转为 double；极大 Beat 和极小 Beat 间隔必须
  有精度测试。
- Clip iterations、Layer/Group weight、离散属性的 weight=1 约束、PropertyMask 冲突和 Additive 属性白名单
  都要进入作者指南和失败示例。
- 参数化目前不能覆盖 Quaternion、rotation、startBeat、priority、parent、AssetId 和拓扑；v5 要么保留该边界，
  要么在参数合同中逐项扩展，不得通过通用表达式绕过校验。

## 5. 实施批次

### V5-A：基线和作者用例

- 盘点 v4 的格式、Runtime、Studio 预期和迁移输入。
- 建立单轴负角度、小数角度、720°/7200°、全局缓动、多轴旋转、相机旋转和 alpha 用例。
- 建立 v4 限制矩阵，区分“必须拆段”“固定值域”“精度边界”“拓扑限制”和“暂不支持”。

### V5-B：ADR 和格式合同

- 接受 v5 顶层版本、能力 ID、旋转顺序、角度单位、未展开规则、数值预算和 FOV `(0,180)`。
- 接受 alpha `[0,255]`、材质/纹理 alpha 合成和 v4 opacity 迁移规则。
- 接受 Euler Override、Additive、Quaternion exact 模式和迁移等价性边界。
- 冻结未知字段、canonical identity、参数化白名单、错误诊断和安全预算。

### V5-C：typed model 和 Runtime 采样

- 为 v5 引入一等 Euler RotationCurve/Segment typed 表达，不把角度退化为普通 Vec3。
- 在 AnimationSampler 中按绝对 local Beat 计算角度并组合 Quaternion，不生成中间事件。
- 保持 World、Transform 和 Renderer 的最终 Quaternion 边界。
- 为采样、Seek、Reload、Stop、循环和 discontinuity 建立 deterministic golden。

### V5-D：Schema、Reader、Writer 和 prepare

- 实现 v5 Schema、严格 Reader、canonical Writer、identity/digest 参与规则和 capability preflight。
- 统一 Reader、prepare、Runtime 和提交阶段的数值范围及溢出诊断。
- 确保 v5 alpha 到 Portable Presentation `[0,1]` 的转换不改变 alphaMode 和资源 identity。

### V5-E：迁移、默认 Writer 和弃用窗口

- 实现 v4 -> v5 显式迁移；对无法证明轨迹等价的 Quaternion 动画稳定失败或要求明确模式。
- 先将 v5 Writer 作为显式 target，再切换为默认 Writer。
- v4 进入只读兼容窗口；v1-v3 只保留迁移入口和稳定拒绝/诊断路径，不再承接新字段。
- 盘点外部资产、提交迁移报告，并在新版本/SDK minor 合同中记录旧格式退出时间点。

### V5-F：作者指南和工具链

- 新增集中式 Chart authoring guide，说明 Object/Template/Parent、相机、动画、alpha、资源闭包、值域和诊断。
- Studio 只消费 v5 typed authoring model，不从 Quaternion 反推未展开 Euler 角度，不静默拆分多圈事件。
- Validator、Migrator、Player 和 Studio Preview 使用同一 v5 语义和示例。

### V5-G：关闭和旧格式退出

- 完成 focused、architecture、package、external consumer、跨平台和 deterministic 验证。
- 在同一候选 SHA 上完成 hosted 验证，并记录性能/精度预算证据。
- 旧格式只有在迁移窗口、外部资产盘点、稳定拒绝测试和 owner acceptance 全部完成后才可删除。
- 完成 v5 completion report，更新格式索引、状态页、路线图和旧路径映射。

## 6. 验收标准

- 一个 v5 单段旋转可以表达负角度、小数角度、720°和 7200°，且不扩展为多个 Quaternion 事件。
- `0 -> 720` 旋转的整体 Hermite 缓动在整段上生效；Seek、Reload、逐帧播放和不同采样帧率的目标时刻结果一致。
- 每轴 `[-10000,10000]` degrees 内的稳定表达范围有跨平台精度和性能证据；越界输入稳定失败。
- 实体和默认相机对相同 Euler XYZ 输入产生相同旋转语义；v4 `pitch/yaw/roll` 迁移有明确报告。
- v5 `camera.fovY` 接受 `(0,180)`，拒绝 `0`、`180` 和越界值；接近 180 的后端能力不足有明确诊断。
- v5 alpha `[0,255]` 的静态值和连续动画正确转换为 `[0,1]`，与材质/纹理 alpha 合成一致。
- v5 Runtime 不依赖多段 Quaternion 烘焙来表达多圈旋转，且事件、曲线和 canonical identity 不因烘焙策略变化。
- v4 兼容输入的迁移、默认 Writer 切换、弃用窗口和最终删除均有可审计证据。
- 作者指南、Schema、Reader、Writer、Validator、Player 和 Studio Preview 对旋转、相机和 alpha 的语义一致。

## 7. 明确不包含

- Stage 6 的版本门禁、Player 产品化、后端中立渲染和常用媒体支持；本阶段只消费其稳定基线。
- Studio 完整编辑器实现；Studio 集成只要求 v5 authoring model 和预览边界明确。
- Vulkan adapter、Judgement/Replay、稳定 C ABI 和运行时脚本/逐帧回调。
- 通过隐式表达式、脚本、随机数或运行时回调扩展 v5 的曲线能力。
- 在没有迁移报告、外部资产盘点和 owner acceptance 的情况下直接删除 v1-v4 Reader。

## 8. 交接

阶段关闭后，v5 是唯一默认写出和新增能力的格式。旧版本只按已接受的兼容窗口提供读取或显式迁移；窗口
结束后，旧 Reader、Schema、迁移入口和 fixture 按 ADR 规定删除。Stage 7 Studio 必须以 v5 typed authoring
model 为编辑对象，并通过 PlaybackSession 预览，不得维护第二套旋转、相机或 alpha 求值语义。
