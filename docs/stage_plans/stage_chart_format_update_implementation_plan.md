# Stage Chart Format Update：谱面格式更新实施计划

状态：CFU-A 已完成；CFU-B 提案已形成；CXT v1、播放前参数、Template Binding 与运行时脚本
无限期延后子决策已于 2026-08-10 接受，ADR 0038 其余门禁仍待接受

更新日期：2026-08-10

本阶段的稳定名称是 **Stage Chart Format Update**，位置为 Stage 3 与 Stage 4 之间。它使用独立名称，不建立数字别名。

## 1. 目标与边界

本阶段的目标是为下一阶段的表现动画和后续 Studio 工作建立可迁移、可验证、可被外部宿主消费的谱面格式合同。ADR 0038 当前提议把 `.cxc` 定义为严格 ZIP32 Stored 交换包，并让动画语义进入 `cuexis.chart` v4；CXT v1 以 UTF-8 JSON 保存声明式局部动画模板，Chart 在 prepare 前解析并绑定它们。在 ADR 0038 整体接受以前，该方案仍不是生产合同。

本阶段交付：

```text
格式身份、版本策略和 source/package/runtime artifact 的边界
Chart 文档中时间轴、Clip、CXT import、Template Binding、参数、资源引用和扩展的可序列化语义
与现有 Chart v1/v2/v3 的显式兼容和迁移合同
规范化、确定性、大小预算、错误诊断和 capability 声明
Schema、typed Reader、Writer/Canonicalizer、Validator 和迁移工具的门禁
Playback prepare/compile 对新格式的明确接受或拒绝路径
供 Stage 4 使用的动画序列化输入，但不实现动画求值系统本身
headless、external consumer、round-trip 和 golden 验收证据
```

本阶段不交付：

```text
AnimationClip、AnimatorComponent、AnimationSystem 或 PropertyResolver 的运行时实现
通用角色状态机、宿主任意代码、无界回调或游戏对象逻辑
运行时脚本与逐帧脚本回调（无限期延后，不预留字段、capability、字节码或执行入口）
Shader、Particle、UI、Light、RenderGraph 或新的渲染后端
正式 Judgement/Replay、稳定 C ABI 或 Studio 编辑器实现
未经过 ADR 冻结的 .cxc 扩展名、二进制布局或压缩算法
```

现有 ChartRuntime、World、PlaybackSession 和 FrameSnapshot 的所有权边界继续有效。格式扩展必须先编译为内部模型，再由 Playback 公共门面暴露结果；格式处理器不得创建 EnTT Entity、调用渲染后端或执行宿主代码。

## 2. 当前基线

Stage 3 关闭时仓库的事实基线如下：

```text
canonical Chart v1/v2/v3 仍是当前唯一生产格式族
v1/v2/v3 按顶层 version 严格路由，不允许按字段猜测版本
v1/v2 -> v3 只能通过显式迁移，不自动写回源文件
未知核心字段是错误；未知可选 extensions 保留并警告
未知 requiredExtensions 在 prepare/compile 前稳定失败
v3 Behavior 使用全局 Beat 的 behavior.event version 1
局部 Beat、循环、BehaviorClip、多 Clip 混合和动态 ParentBinding 尚未进入正式 Schema
Stage 4 的 Layer、BlendGroup、HostOverride 混合规则已有设计文档，但尚无格式序列化合同
仓库当前没有 CXC 实现、Schema、Reader、Writer、fixture 或公共 API
```

因此，本阶段不能通过在 `extensions` 中偷偷增加一套动画字段来规避版本设计。若新语义改变 Chart 的可观察结果、迁移规则或宿主能力要求，必须提升格式/组件版本并形成独立 ADR。

## 3. 决策门禁

下列问题必须在任何生产代码、Schema 或文件扩展名落地前逐项接受。每一项都需要一个可审计的示例和失败示例。

### CFU-0：身份与载体

冻结以下合同：

```text
格式 ID（format 字符串）与文件扩展名是否相同
可编辑 source、可交换 package、Runtime cache 是否是同一种 artifact
是否允许一个 package 包含多个 Chart、Asset Index 和版本化资源
版本号是单一整数、能力版本集合，还是外层容器与内层 Chart 分离版本
文件编码、压缩、校验和/语义 identity 的计算输入
```

在 CFU-0 接受前，代码和文档只能称其为“候选 CXC”，不能声称已有可加载的 CXC 文件。

### CFU-1：文档模型与引用

冻结最小可表达模型：

```text
Chart、Template、Object、Behavior、Clip、Layer 和 Binding 的稳定 ID 及作用域
ChartParameter 的类型、默认值、范围、冻结时机和 normalized identity
CXT JSON 的身份、单模板导出、import path、application contract 和 lowering 规则
全局 Beat 与局部 Clip 时间的换算、时钟来源和 offset 归属
属性路径、Property ID、property mask 和可写类型白名单
Mesh/Texture/Material/Audio 等资源引用的 domain、版本和 identity
模板继承、实例覆盖、共享 Clip 和未绑定定义的保存/迁移规则
未知字段与可选扩展的保留、警告和重新保存语义
```

Illustrative only, not normative schema:

```json
{
  "format": "candidate.cuexis.chart",
  "version": 4,
  "clips": [{
    "id": "clip.note.pulse",
    "timeDomain": "beat",
    "tracks": [{
      "property": "render.opacity",
      "keys": [{"beat": 16, "value": 1.0}, {"beat": 20, "value": 0.25}]
    }]
  }],
  "bindings": [{"object": "object.note", "clip": "clip.note.pulse", "loop": 2}]
}
```

上例只用于检验概念是否能表达“对象在 Beat 16 到 20 淡出并循环两次”；字段名、版本号和载体必须等待 CFU-0/CFU-1 冻结。

### CFU-2：时间、采样与跳转

至少建立以下确定性用例：

```text
在 Tempo Event 和 Stop 下，Clip 的局部时间能从绝对 chartTimeMs 重建
直接 Seek 到任意时间与从起点逐帧播放到该时间得到相同状态
循环边界、零持续事件、相邻事件和负 Beat 不依赖上一帧结果
timeDiscontinuity 后不复用旧的 Layer/Clip 混合状态
事件回调（若保留）通过区间查询获得，不从属性最终值反推
```

格式合同只能描述有界、可重放的数据；不得把帧率、宿主线程调度或遍历顺序写入语义。

### CFU-3：扩展、能力与安全预算

冻结：

```text
requiredExtensions 的 ID/version/capability 映射和稳定拒绝 code
可选扩展的保留、警告、重新保存和未知数据上限
Chart、Clip、Track、Key、Binding、引用和嵌套深度预算
资源总大小、单资源大小、诊断数量和每帧 Property Write 预算
扩展处理器只能 validate/migrate/compile，不得执行脚本或创建 World 对象；CXT 不是执行入口
```

新格式在宿主能力不足时必须在资源获取或 World 发布前失败，不能静默删掉动画、切换资源或改变时间语义。

### CFU-4：兼容与迁移

迁移合同必须区分三类输入：

```text
v1/v2/v3 canonical Chart：继续读取并保持历史可观察结果
新格式的语义等价迁移：显式命令、独立输出和结构化报告
无法证明等价的输入：稳定失败，不使用近似或静默丢字段
```

迁移报告至少记录 source/target identity、版本、基准修改、模板展开、生成的 Clip/Binding/Event 数量、未绑定定义、丢弃字段和诊断。输入、输出和报告路径必须互不冲突，迁移器不得覆盖源文件。

### CFU-5：运行时和公共观察面

在格式实现前冻结以下影响面：

```text
新文档编译到现有 ChartRuntime，还是引入独立的 compiled document 层
PlaybackSession 的 prepare/activate/reload/seek 事务边界
FrameSnapshot 是否增加字段，以及 FrameDigest 版本升级策略
capability preflight 如何报告格式要求与宿主能力缺口
外部 consumer 只依赖 Playback public headers，不接收 ChartRuntime/World/EnTT
```

Stage 4 的动画系统只能消费本阶段已冻结的 typed 数据；不得在 AnimationSystem 内部再解析 JSON 或 CXC。

## 4. 实施批次

实施顺序固定为：

```text
CFU-A 现有格式/资产盘点与用例 fixture
-> CFU-B CXC 身份、文档模型和时间语义 ADR
-> CFU-C Schema、typed Reader、Writer 和 Canonicalizer
-> CFU-D v1/v2/v3 -> 新格式显式迁移工具
-> CFU-E ChartRuntime/Playback prepare、capability 和诊断接入
-> CFU-F headless、external consumer、round-trip 和 golden
-> CFU-G 跨平台验收、文档封存和 Stage 4 交接
```

### CFU-A：盘点与用例

状态：仓库内盘点和候选用例清单已于 2026-08-10 完成；证据见 `docs/stage_reports/260810-chart-format-update-inventory.md`。

目标是先验证格式需求，而不是先设计字段。至少建立以下 fixture：

```text
现有 v3 Chart 原样加载并重新保存
一个对象的 Transform/Opacity 动画
同一 Clip 绑定到两个对象且属性 mask 不同
循环、Stop、Seek 和负 Beat 边界
外部 Mesh/Texture/Material/Audio 引用及缺失资源
未知可选扩展 round-trip 与未知 required extension 拒绝
超过每个预算的拒绝 fixture
```

CFU-A 还必须记录仓库内外需要保留的旧 Chart 资产；没有盘点结论，不得删除 v3 入口或声称迁移完成。

### CFU-B：合同与 ADR

状态：ADR 0038、`docs/CXC_FORMAT.md`、`docs/CHART_V4_FORMAT.md`、`docs/CXT_FORMAT.md` 和
候选正反例已于 2026-08-10 起草；
CXT v1、播放前参数与 Template Binding 子决策已接受，ADR 0038 其余门禁仍待项目所有者确认。

修订并接受现有 ADR 0038 及其字段级规范，并在整体接受前完成：

```text
至少三个正例和三个负例的规范 JSON/伪格式，至少包含一个 `.cxt` import 和一个 ParameterSet
字段路径、版本路由和错误 code 表
时间线/循环/绑定的边界表
参数 resolution、CXT lowering、CXC 闭包和模板 ID 冲突规则
source/package/runtime artifact 的所有权图
Stage 4 所需字段与明确延期字段列表
```

CFU-B 整体未接受时，CFU-C 只能在实验分支中进行，不得更新安装公共头或默认 fixture。已接受的
CXT 子决策不构成 CXC/Chart v4 生产入口，也不解除该门禁。

### CFU-C：格式管线

按已接受 ADR 实现 CXC/CXT/Chart Schema、typed model、Reader、Writer/Canonicalizer、import/
parameter resolver 和 validator。Schema、typed Reader、语义 validator 与 writer 必须由共享 fixture
交叉验证；未知核心字段、非法版本、非有限值、参数类型、缺失 CXT、越界引用和超预算输入都要
产生稳定 code 与字段路径。

### CFU-D：迁移

新增显式迁移 CLI/API，保留 v1/v2/v3 读取回归。迁移结果必须是规范化输出，报告必须可机器读取；无法证明旧语义与新语义等价时失败。迁移不得把运行期状态、上一帧值或宿主 override 写入 Chart 基线。

### CFU-E：Runtime/Playback 接入

只接入格式所需的 prepare、compile、capability 和 diagnostics 路径。若动画运行时尚未实现，新格式中的动画定义可以被验证并以“能力未提供”稳定拒绝，但不得在 Chart compiler 内实现第二套动画求值器。

### CFU-F：消费者与确定性

至少有一个无 GPU external consumer 和 Player/headless consumer 使用同一 public Playback 路径。覆盖：

```text
相同输入、不同 JSON 数组顺序和不同 Entity 遍历顺序产生相同摘要
round-trip 不改变语义 identity
reload 失败不替换 active 内容
seek、stop、循环和缺失 capability 的诊断稳定
历史 FrameDigest golden 按版本保留，不静默重写
```

### CFU-G：阶段交接

只有满足第 6 节的退出条件，才能把 Stage 4 标为可开始。交接包必须链接：新 ADR、格式规范、Schema、迁移报告 golden、测试矩阵、外部 consumer 运行证据和残余风险清单。

## 5. 预期变更面

实施时预计涉及以下边界；本阶段计划不预先修改这些文件：

```text
docs/CHART_FORMAT.md
docs/CHART_V4_FORMAT.md
docs/CXC_FORMAT.md
docs/CXT_FORMAT.md
docs/adr/0038-cxc-v1-and-chart-v4-boundary.md
schemas/cuexis.chart.*.schema.json、CXC Schema 与 cuexis.animation-template.v1 Schema
engine/chart/include/cuexis/chart/*
engine/chart/src/canonical_chart_loader.cpp
engine/chart/src/chart_migrator.cpp
tools/chart_validator/*、tools/chart_migrator/*
tests/chart/*、tests/runtime/*、tests/external/*
assets/charts/*、assets/projects/*
```

`engine/animation/`、`engine/behavior/` 和 `docs/ANIMATION_MIXING.md` 在本阶段只接收格式边界与交接说明；动画求值实现仍属于 Stage 4。

## 6. 退出条件

Stage Chart Format Update 完成必须同时满足：

```text
格式 ID、载体、版本、时间域、引用和扩展合同已有接受的 ADR
规范文档、Schema、typed Reader、Writer/Canonicalizer 和 validator 一致
v1/v2/v3 仍按历史语义读取，显式迁移有可审计报告且不覆盖源文件
新格式的合法/非法、预算、缺资源、缺 capability 和未知扩展 fixture 完整
Playback prepare/activate/reload/seek 的成功与失败路径有 headless 证据
至少一个 clean external consumer 只使用 public Playback headers
round-trip、FrameDigest/identity 和数组/实体顺序确定性 golden 通过
Stage 4 所需动画字段已列明，延期字段不会被隐式实现
docs/PROJECT_GUIDE.md、cuexis_sdk_transition_plan.md、CHART_FORMAT.md、CHART_V4_FORMAT.md、
  CXC_FORMAT.md、CXT_FORMAT.md 和 ANIMATION_MIXING.md 的状态与链接一致
```

任何一项未满足时，状态仍为“规划中”或“实现中”，不得以“格式更新已完成”或“CXC 已支持”对外描述。

## 7. Stage 4 交接边界

Stage 4 可以直接消费本阶段的 typed Clip/Binding/Property 数据，但必须继续遵守：

```text
AnimationSystem 不读取 JSON/CXC/CXT，不访问应用配置文件或执行脚本
Behavior、Animation、HostOverride 和 StudioPreviewOverride 通过统一 PropertyResolver 求值
时间跳转从绝对时间重建状态，不依赖上一帧混合结果
宿主通过稳定 ID/OverrideToken 操作，不访问 World 或最终 Component
```

Stage 4 和当前路线均不包含运行时脚本、通用状态机或任意回调。不得通过 extensions、CXT、
capability 或宿主隐式实现绕过该边界；重新启动运行时脚本议题必须先取得新的明确产品决策，
再建立独立 ADR 和阶段计划。
