# 阶段 2：Behavior Event 与 Cuexis 表现能力实施计划

状态：实现与 Windows 本地验收已完成；最终跨平台验收待 hosted Linux CI

完成证据见[阶段 2 完成报告](../stage_reports/stage_2_completion_report.md)。

## 1. 目标与边界

阶段 2 的目标是让 Behavior 表达 Cuexis 谱面表现，并通过 `PlaybackSession` 在任意目标时间确定性采样。它不建设宿主任意代码、脚本、无界回调或通用游戏状态机。

阶段 1C 的 `behavior.transform.keyframe` version 1 保持原有采样语义。阶段 2 的新谱面表达采用 Behavior Event；运行时可以将其编译成内部 Segment 或等价结构，但公共谱面语义以事件为准。

## 2. Behavior Event 核心语义

连续属性事件使用以下字段：

```json
{
  "property": "transform.position.x",
  "startBeat": { "numerator": 16, "denominator": 1 },
  "durationBeats": { "numerator": 4, "denominator": 1 },
  "startValue": 0.0,
  "endValue": 10.0,
  "startSlope": 0.0,
  "endSlope": 0.0
}
```

求值规则：

```text
event 外：保持对象初始基准或前一事件终值
event 开始：应用 startValue，允许与当前基准不同
event 区间：按 Beat 计算归一化进度并插值
event 结束：保持 endValue，直到下一个事件
```

进度函数与 TimingMap 的 Tempo Event 相同，使用 `h(u) = (-2 + m0 + m1)u^3 + (3 - 2m0 - m1)u^2 + m0u` 的端点斜率三次 Hermite 函数。标量和 Vec3 按分量插值；Quaternion 使用 shortest-path slerp，并使用 Hermite 结果作为 slerp 进度。`startSlope` 和 `endSlope` 为归一化进度斜率，必须有限、非负，并满足 `startSlope + endSlope <= 3`。

`durationBeats = 0` 表示瞬时事件，并要求 `startValue == endValue`、`startSlope == 0`、`endSlope == 0`。值相等按解析后的 typed 值精确比较，Vec3/Quaternion 逐分量比较。非零事件使用半开区间 `[startBeat, startBeat + durationBeats)`；无后继事件时结束边界及其后保持终值，相邻事件在同一边界处由后一个事件优先。零持续事件在精确 `startBeat` 处应用其值并建立后续基准。同一属性的事件不得重叠，也不得具有相同 `startBeat`；零持续事件在冲突检测中占用其 Beat，不能位于同属性非零事件内部，可以位于前一事件的结束边界。输入数组顺序无语义，编译器按属性分组后按 Beat 排序。

`groupId` 的作用域为单个 Behavior，并匹配 `[A-Za-z0-9][A-Za-z0-9._-]{0,255}`；同一 ID 只能表示一组具有相同 `startBeat` 和 `durationBeats` 的事件。它是边界一致性声明，不改变整帧提交本身的事务性。

负 Beat 事件必须参与 Beat 0 的基准求值。Stop 内 Beat 固定，因此 Behavior Event 的进度也固定；Stop 结束后从同一 Beat 继续采样。

## 3. 离散属性边界

Visibility、Material 选择、ParentBinding 等离散属性不能直接使用连续插值。阶段 2 必须为它们定义受限 `Step Event`，或明确将其延后；不能通过对枚举、布尔或资源引用执行数值插值来隐式定义行为。Step Event 的字段、边界和冲突规则在实现前单独冻结。

## 4. 实施分段与依赖

实施顺序固定为：

```text
2A.1 Simple 格式退役门禁
-> 2A.2 Chart v3 格式管线
-> 2B TimingMap
-> 2C 连续 Behavior Event
-> 2D Visibility / Material 表现属性
-> 2E ParentBinding / BehaviorClip 决策门禁
-> 2F Capability / 诊断 / 调试
-> 2G 全链路验收与迁移交付
```

2A.1 必须在任何 v3 生产代码开始前完成，避免新格式继续携带 Simple 分支。2A.2、2B 和 2C 是后续阶段的强前置。2D 完成前不承诺完整的表现属性输出；2E 未冻结时不得把局部 Beat、循环或动态父级字段加入正式 Schema。2G 通过后阶段 2 才算完成。

### 2A.1：Simple 格式退役门禁

状态：已于 2026-08-05 完成。仓库内 Simple fixture 已有 canonical 对应物，项目所有者确认仓库外需保留的 Simple v1 资产为空，因此未创建一次性转换物。Loader、Importer、公开头、Schema、测试和 Player fixture 已删除，并保留 retired format 的稳定拒绝测试。

目标：在设计和实现 v3 Reader/Schema 前物理删除方案 B，使后续格式、诊断和测试只面对 canonical Chart。

任务：

- 移除 `cuexis.chart.simple` 路由、SimpleChartImporter、public header/source、Schema、测试和 Player fixture。
- 盘点仓库内外仍需保留的 Simple v1 文件；把仓库 fixture 转换为 canonical，并以语义 golden 验证结果。
- 当前没有 Simple -> canonical JSON 的落盘 CLI；若外部盘点非空，在删除 Importer 前提供并验证固定于最后 Stage 1 格式的一次性转换物。若盘点为空，以记录关闭此项，不建立长期兼容层。
- 更新当前规范、构建说明和示例；历史 ADR、阶段计划和完成报告只保留带取代说明的历史事实。

验收目标：

- `cuexis.chart.simple` 稳定报告不支持格式，构建图、安装头、测试发现和 Player 资产中不再包含 Simple importer/fixture。
- Simple 资产盘点及转换结果可审计；阶段 2 源码、工具、安装包和现行流程不依赖一次性转换物。
- canonical v1/v2 golden、Player、headless Playback 和 external consumer 结果不因删除 Simple 分支改变。
- 全仓搜索仅允许历史文档和明确的 unsupported-format 测试保留 `cuexis.chart.simple`。

### 2A.2：Chart v3 格式管线

状态：已完成。Chart v3 Schema、typed Reader、严格版本路由、事件预算、validator 和最小示例已交付。

目标：让 v3 成为可严格读取、校验和编译前置检查的独立格式版本，但暂不要求 Behavior 产生运行时属性写入。

任务：

- 新增 `schemas/cuexis.chart.v3.schema.json`，并让 Schema、typed Reader 和字段白名单一致。
- 为 `timing.tempoEvents`、`behavior.event`、连续事件值、`groupId` 和 v3 安全预算增加 typed Chart 数据结构。
- 按顶层 `version` 严格路由；v3 拒绝 `bpmChanges` 和 v1 Keyframe，v1/v2 拒绝 v3 字段。
- 在 Reader/Compiler 前置阶段校验 RationalBeat、数值范围、未知字段、事件数量和引用域。
- 冻结 v1/v2 -> v3 的迁移调用、报告和失败契约并建立 fixture 语料；完整转换要等待 2B/2C 的目标语义实现，最终在 2G 交付，且不得自动写回源文件。
- 更新 canonical Chart 文档、Schema artifact tests、validator 和最小 v3 示例。

验收目标：

- 合法最小 v3 Chart 可进入 typed `ChartDocument`；结构错误在资源请求和 World 发布前失败。
- Schema 和 typed Reader 对共享合法/非法 fixture 得出一致结论，语义错误仍由 Chart 校验层负责。
- v1/v2 golden、未知字段和版本路由测试保持不变；不存在按字段猜测版本或静默降级。
- 超预算事件、非法 Beat、NaN/Inf、非法类型和旧字段均产生稳定 code 与字段路径。
- 2A.2 不宣称尚未实现的 Timing/Behavior 迁移成功；相关 fixture 此时只能完成结构路由或产生稳定的“迁移能力尚未交付”诊断。

### 2B：Tempo Event、Stop 与 TimingMap

状态：已完成。固定分段积分、固定次数逆解、Stop/负 Beat/零持续语义及 4096/4096 预算已实现并测试。

目标：完成 v3 `Beat <-> chartTimeMs` 的确定性双向映射，为 Behavior 提供唯一的 `BeatSample`。

任务：

- 编译并排序 Tempo Event，校验重叠、同 Beat 冲突、BPM/斜率范围和零持续约束。
- 从 `defaultBpm` 建立最早事件前的基准；允许事件开始值跳变，并让事件终值成为间隙和后续事件前的基准。
- 直接对 BPM 执行 Hermite 插值，并对 `60000 / bpm(beat)` 进行有预算的确定性积分。
- 预编译常量区间、事件边界、累计时间和 Stop 边界表。
- 实现 `beatToChartTimeMs`、`chartTimeMsToBeat`、`inStop` 和 `stopProgress`。
- 逆映射使用固定次数或固定上限的二分，不依赖平台容差提前退出的 Newton 迭代。
- 覆盖负 Tempo Event、负 Beat Stop、Beat 0 基准、零持续事件、相邻事件、Stop 同 Beat 和 offset。
- 固定单 Chart 积分分段与边界表预算；Runtime 每帧查询不得分配。

验收目标：

- 常量 BPM、升降 BPM、跳变、负 Beat、零持续和 Stop 均有正向/逆向 golden。
- 全部合法输入范围内映射有限且单调；常用 BPM/长度范围满足冻结的往返误差预算。
- Stop 半开区间返回固定 Beat、`inStop=true` 和 `[0,1)` 的 `stopProgress`；精确结束边界返回同一 Beat、`inStop=false`。
- 事件输入顺序不改变编译结果；超预算或非法事件在 prepare 前稳定失败。
- 多次直接查询与不同帧率逐帧查询得到相同 Beat 结果，不通过帧增量累计时间。

### 2C：连续 Behavior Event Runtime 闭环

状态：已完成。Transform/Camera Event、绝对 Beat 采样、分组校验、Seek/Reload/Stop 和 v1 兼容路径已闭环。

目标：让 Transform 与 Camera 连续属性通过 Behavior Event 在任意时间绝对采样。

任务：

- 按 Property 编译事件 Segment，校验重叠、类型、值范围、Quaternion 和 `groupId` 一致性。
- `cuexis_runtime` 每帧只计算一次 `chartTimeMs -> BeatSample`，所有对象和 Behavior 复用该结果；Runtime 只把求值所需的 Beat 值传给 Behavior。
- 保持 `cuexis_behavior -> cuexis_core + cuexis_world` 依赖边界，不让 Behavior public header 引用 `cuexis_chart` 的 TimingMap/BeatSample 类型。
- 实现 scalar/Vec3 Hermite 进度插值和 Quaternion shortest-path slerp。
- 支持 `transform.position.x/y/z`、`transform.rotation`、`transform.scale` 和 `camera.fovY`。
- 从 prepare 时捕获的对象初始值重建候选属性；事件开始值允许与当前基准不同。
- 同组属性在同一帧原子提交；任何候选非法时不发布半帧结果。
- 支持 Stop、Seek、Reload、负 Beat 和 discontinuity 的绝对重采样。
- 保留 v1 Keyframe Runtime 路径和 golden，不用 Event 实现替换其采样函数。

验收目标：

- 覆盖事件前、开始、区间、结束、间隙、相邻跳变、零持续位于前一结束边界/活动区间内部和负 Beat 边界。
- Stop 内属性不推进；直接 Seek 到目标时间与从起点播放到目标时间结果相同。
- 标量、Vec3、Quaternion 和 FOV 有类型、范围、有限性及插值 golden。
- 同一 `groupId` 原子生效；重叠、组字段不一致和 Property 冲突在 prepare 阶段失败。
- Runtime 更新无动态分配，结果不依赖帧率、对象输入顺序或 EnTT 遍历顺序。
- Player、headless PlaybackSession 和 v1/v3 fixture 的 FrameSnapshot 回归均通过。

### 2D：Visibility 与 Material 表现属性

状态：已完成。Visibility/Material Step Event、Opacity/Tint 连续 Event、拥有型 Snapshot 输出和 FrameDigest v2 已交付。

目标：补齐阶段 2 可观察的离散/材质属性，不提前建设阶段 3 的通用渲染管线。

任务：

- 先冻结 Step Event 的公共字段、Beat 边界、基准保持和冲突规则，并同步 ADR、Chart 文档与 Schema。
- 在实现 Material/Visibility 前冻结它们所需的最小 capability ID、preflight 输入和失败契约；2F 在此基础上统一全部能力诊断。
- 实现 `render.visible` 的 Step Event，移除 FrameSnapshot 中始终为 `true` 的临时行为。
- 区分连续 Material 参数与离散 Material 资源选择：数值/向量参数使用连续 Event，资源引用使用 Step Event。
- 只定义 Cuexis 表现所需的最小 Material 属性白名单、类型和范围；不引入 Shader、Pipeline 或后端对象。
- 扩展 PropertyResolver、基线捕获和后端无关 FrameSnapshot 输出。
- 若 FrameSnapshot 可观察字段发生变化，同步升级 FrameDigest 算法版本并增加旧/新 digest golden；不得在版本 1 digest 中静默加入字段。
- 对宿主不能表示的 Material 属性使用明确 capability/preflight 失败或已冻结的受控降级。

验收目标：

- Visibility 在事件前后、零持续、Seek、Reload 和 Stop 中得到确定的 FrameSnapshot 值。
- Material 连续参数和资源切换有类型、边界、缺失资源与能力不支持测试。
- 已返回的 FrameSnapshot 继续拥有自身数据，后续 update/reload/unload 不使其悬空。
- Headless consumer 能读取最终表现值，不需要访问 World、EnTT、Material 实现或图形 API。
- Material/Visibility capability 在 prepare 时可查询且不支持时稳定失败；2D 不依赖尚未交付的 2F 调试界面。
- 阶段 3/5 的 Shader、Pipeline 和 Portable Material Profile 未被阶段 2 私自冻结。

### 2E：ParentBinding、局部 Clip 与循环决策门禁

状态：已关闭。ParentBinding、局部 Beat、循环、多 Clip、priority/weight 和混合明确延期，Chart v3 不预留字段并稳定拒绝输入。

目标：先决定高风险时间语义是否进入阶段 2，再实施；不允许以未声明行为混入 v3。

任务：

- 先决定 ParentBinding 是否进入阶段 2；若接受，再冻结 Step Event 的 parent/null 值、同 Beat 切换、世界/局部空间保持策略和失败语义。
- 若接受 ParentBinding，定义动态父级的全时间环检测或边界区间验证算法，并设置事件/图遍历预算；若延期则不预留字段，并稳定拒绝输入。
- 决定是否在阶段 2 增加 `cuexis.behavior` 新版本以支持局部 Beat、`startBeat`、`none/repeat/pingPong` 和显式循环长度。
- 冻结循环端点、精确周期边界、负局部 Beat、零长度和 Stop 交互。
- 决定多 Clip 是否延期；阶段 2 不实现 priority、weight、Override/Additive 混合。
- 若任一能力延期，更新 ADR、Schema 和诊断，使 v3 明确拒绝对应字段。

验收目标：

- 本小阶段必须以“已实现并通过测试”或“明确延期并稳定拒绝”之一结束。
- 若实现 ParentBinding：所有事件边界的 active parent graph 无环，失败不发布半帧层级。
- 若实现局部 Clip/循环：直接采样、跨周期播放、Seek、Stop 和 discontinuity 结果一致。
- v3 既有全局 Beat + 单 Behavior 绑定的 Chart 结果不因新 binding version 改变。
- 不出现依赖数组顺序的父级冲突、循环边界或多 Clip 隐式混合。

### 2F：Capability、诊断与调试闭环

状态：已完成。四个 Stage 2 capability、preflight、稳定诊断和有界内部调试快照已交付。

目标：让宿主在播放前知道 Chart v3 所需能力，并让 Studio/Player 能定位最终属性来源。

任务：

- 汇总并版本化 Behavior/Property capability 集合，包括 2D 已冻结的最小集合；不能直接复用当前全部 unsupported 的 `requiredExtensions` 机制。
- 在 prepare/preflight 阶段汇总格式版本、Property、Material/Visibility 和可选 Clip 能力要求。
- 固定 unsupported、冲突、预算、迁移和采样错误的诊断 code、字段路径与排序。
- 增加内部调试快照：初始基准、命中事件、归一化进度、Behavior 输出和最终解析值。
- 调试数据不得进入普通 FrameSnapshot，也不得暴露 World、EnTT 或内部指针。
- 为诊断和调试缓冲设置硬预算；Runtime 不调用宿主任意脚本或无界回调。

验收目标：

- 不支持的必需能力在 World/资源发布前失败，不会运行中静默跳过。
- 相同输入产生相同诊断集合和顺序；数组顺序不影响错误选择。
- 调试快照可以解释一个属性从 Initial 到 Behavior 再到最终值的来源。
- 关闭调试时不产生额外每帧分配；预算溢出产生有界且可识别的诊断。
- 安装后的 Playback 公共头继续不暴露 EnTT、JSON DOM、SDL 或图形后端类型。

### 2G：全链路验收与迁移交付

状态：实现、迁移 CLI、Windows/MSVC static/shared Debug/Release、headless、format、architecture、external consumer、零分配和 Debug/Release GPU 门禁已完成；Windows MinGW headless Release 也已通过。当前工作树未提交或推送，`gh run list --commit` 无结果，hosted Linux GCC/Clang、sanitizer 和 package consumer jobs 必须在最终跨平台验收时补齐，不能以 Windows 结果替代。

目标：完成格式迁移、SDK 消费、性能和跨平台门禁，形成可进入阶段 3 的稳定基线。

任务：

- 完成 canonical v1/v2 -> v3 显式迁移工具和迁移报告；`linear`/`in_cubic`/`out_cubic` 精确映射为单 Event，`in_out_cubic` 在 Beat 中点精确拆为两个 Event，并覆盖首 Key 基线改写、单 Key、共享 Behavior、模板实例和未绑定 Behavior。
- 验证仓库 Simple fixture 已在 2A 前转换为 canonical，阶段 2 交付物不再包含 Simple 迁移 API。
- 建立 v1/v2/v3 canonical fixtures、迁移 golden 和 Player/headless/external consumer parity。
- 覆盖 load/update/seek/reload/unload、失败回滚、多 Session 和旧 FrameSnapshot 生命周期。
- 运行恶意输入、预算、长谱面、负 Beat、极端合法 BPM 和事件数量上限测试。
- 测量 TimingMap 编译、PlaybackSession update/extract、FrameSnapshot 大小和每帧分配。
- 更新 Chart/Timing/Behavior/SDK 文档、示例、validator 和安装包 consumer。
- 评审所有公开 C++ 结构变化并按版本政策更新 `CUEXIS_SDK_API_VERSION`；FrameSnapshot/FrameDigest 变化必须更新 digest 版本、package consumer 和兼容性测试。
- 执行 Debug/Release、format、architecture、static/shared package 和受支持平台 CI 门禁。

验收目标：

- v1/v2 行为和 golden 不变；合法 v3 与迁移后 v3 产生规定的确定结果。
- v1 与迁移后 v3 的标量/Vec3/Quaternion 曲线满足冻结的迁移等价预算；同一 v3 在不同 Playback 模式间仍必须通过 FrameDigest 位级 parity。
- Player、内部 Playback、add_subdirectory 与 find_package consumer 的 RuntimeFrame/FrameSnapshot parity 通过。
- adapter-disabled、无 SDL/OpenGL、无物理音频设备的 headless v3 闭环通过。
- Runtime 更新路径无动态分配，编译/内存/帧成本不超过阶段 2 冻结预算。
- Schema、typed Reader、迁移器和规范文档对同一字段集合没有漂移。
- 所有必需构建、CTest、架构、格式和外部消费门禁通过，并形成阶段 2 完成报告。

本地关闭证据：MSVC static `249/249`、headless `216/216`、shared `251/251`，Debug/Release
全部通过；Release 启用 `/WX`。MinGW headless Release `211/211` 通过。普通与全不可见场景的
Debug/Release GPU smoke 均完成 3 帧，其中全不可见场景为 `Objects: 4, debug commands: 0`。
hosted Linux CI 尚无 run URL，因此 2G 的本地门禁已关闭，跨平台发布门禁仍保持打开。

## 5. 阶段 2 总体验收门禁

- 同一 Chart、同一 Beat/时间输入在 Player、PlaybackSession 和 external consumer 中产生相同结果。
- 覆盖常量区间、连续事件、跳变、相邻事件、零持续事件、负 Beat、Stop、Seek 和 Reload。
- 覆盖标量、Vec3、Quaternion 的有限性、单调性和边界采样。
- v1 Keyframe golden 结果保持不变，迁移结果有独立 golden。
- Behavior 同属性相同起始 Beat、零持续 typed equality、`groupId` 作用域和分组边界均有拒绝/接受 golden。
- 恶意或超预算 Chart 不导致无界积分、分配或回调。
- 不支持的离散属性不会被静默近似或数值插值。

## 6. 已冻结的 v3 格式

- Chart 顶层版本为 `3`，v3 使用 `timing.tempoEvents`，不接受 `timing.bpmChanges`。
- 新 Behavior 类型为 `behavior.event` version `1`，事件使用 Chart 全局 Beat。
- 同组连续事件使用可选 `groupId`；同组事件必须具有相同开始 Beat 和持续时间。
- `groupId` 仅在单个 Behavior 内生效并使用已冻结的 portable ASCII 模式；它声明边界一致性，不改变整帧事务提交。
- v3 对象暂时沿用 `cuexis.behavior` version `1` 的单 Behavior 绑定结构。
- v3 及后续版本只支持 `cuexis.chart`；不存在 `cuexis.chart.simple` v2 或 v3。

## 7. 已关闭的设计门禁

- TimingMap 使用固定 16 点 Gauss-Legendre 积分和 64 次二分逆解；误差及事件预算见 ADR 0036。
- v1/v2 -> v3 的 typed 中点、Quaternion 误差和有理数溢出失败合同已冻结。
- Step Event 仅支持 `render.visible` 与 `render.material`；Material 连续属性仅支持
  `material.opacity` 与 `material.tint`。
- ParentBinding、局部 Beat、循环和多 Clip 明确延期；v3 不预留字段并稳定拒绝输入。
