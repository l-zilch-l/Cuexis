# Stage Chart Format Update：谱面格式更新实施计划

状态：CFU-A/CFU-B、CFU-C0/C1/C2/C3/C4 已完成；CFU-D1/D2 已关闭；下一批次为 CFU-E；CFU-D3 等待 CFU-E；ADR 0038 已于 2026-08-11 整体接受

更新日期：2026-08-13

本阶段的稳定名称是 **Stage Chart Format Update**，位置为 Stage 3 与 Stage 4 之间。它使用独立名称，不建立数字别名。

## 1. 目标与边界

本阶段的目标是为下一阶段的表现动画和后续 Studio 工作建立可迁移、可验证、可被外部宿主消费的谱面格式合同。ADR 0038 把 `.cxc` 定义为严格 ZIP32 Stored 交换包，并让动画语义进入 `cuexis.chart` v4；CXT v1 以 UTF-8 JSON 保存声明式局部动画模板，Chart 在 prepare 前解析并绑定它们。合同已接受，但只有对应实现和验收门禁关闭后才能对外声明生产支持。

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
Stage 3 关闭时没有 CXC 实现、生产 Schema、Reader、Writer、正式 fixture 或公共 API；只有候选
评审示例
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
严格 ZIP32 metadata、entry sentinel、manifest canonical bytes 和 binary golden
CxcPackageIdentity 与跨 source PreparedSemanticIdentity 的分工
项目声明闭包是否保留全部 Asset Index record，pack 是否允许重写或裁剪
```

在 CFU-0 接受前，代码和文档只能称其为“候选 CXC”，不能声称已有可加载的 CXC 文件。

### CFU-1：文档模型与引用

冻结最小可表达模型：

```text
Chart、Template、Object、Behavior、Clip、Layer 和 Binding 的稳定 ID 及作用域
ChartParameter 的类型、默认值、范围、冻结时机和 normalized identity
CXT JSON 的身份、单模板导出、import path、application contract 和 lowering 规则
SourceChartDocument -> ResolvedChartDocument -> Runtime/AnimationProgramInput 的 typed 分层
PlaybackSource project-document table 与 per-prepare ParameterSet 的所有权
全局 Beat 与局部 Clip 时间的换算、时钟来源和 offset 归属
属性路径、Property ID、property mask 和可写类型白名单
Mesh/Texture/Material/Audio 等资源引用的 domain、版本和 identity
模板继承、实例覆盖、共享 Clip 和未绑定定义的保存/迁移规则
generated composite identity、canonical array order 和 Template animator patch 限制
未知字段与可选扩展的保留、警告和重新保存语义
```

字段形状不在计划中重复定义。CFU-B 的规范性候选字段见
[CHART_V4_FORMAT.md](../CHART_V4_FORMAT.md)、[CXT_FORMAT.md](../CXT_FORMAT.md) 和
[候选正反例](../examples/chart_format_update/README.md)。计划只维护实施顺序和门禁。

### CFU-2：时间、采样与跳转

至少建立以下确定性用例：

```text
在 Tempo Event 和 Stop 下，Clip 的局部时间能从绝对 chartTimeMs 重建
直接 Seek 到任意时间与从起点逐帧播放到该时间得到相同状态
循环边界、零持续事件、相邻事件和负 Beat 不依赖上一帧结果
timeDiscontinuity 后不复用旧的 Layer/Clip 混合状态
离散 Step/Behavior Event 通过绝对区间或目标 Beat 查询，不从属性最终值反推，也不增加回调字段
```

格式合同只能描述有界、可重放的数据；不得把帧率、宿主线程调度或遍历顺序写入语义。

### CFU-3：扩展、能力与安全预算

冻结：

```text
requiredExtensions 的 ID/version/capability 映射和稳定拒绝 code
可选扩展的保留、警告、重新保存和未知数据上限
Chart、Clip、Track、Key、Binding、引用和嵌套深度预算
资源总大小、单资源大小、诊断数量和每帧 Property Write 预算
全局 Track/Segment/Step/generated record 总量与 checked arithmetic
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
SourceChartDocument、ResolvedChartDocument、ChartRuntime 和 AnimationProgramInput 的边界
PlaybackSession 的 prepare/activate/reload/seek 事务边界
project-document table、CXC file/memory source 和 per-prepare ParameterSet 的公共观察面
内部 `cuexis_cxc` target、archive dependency 和 package-backed provider 的依赖方向
FrameSnapshot 保持不变、FrameDigest v3 保留及独立调试 provenance 的策略
capability preflight 如何报告格式要求与宿主能力缺口
外部 consumer 只依赖 Playback public headers，不接收 ChartRuntime/World/EnTT
```

Stage 4 的动画系统只能消费本阶段已冻结的 typed 数据；不得在 AnimationSystem 内部再解析 JSON 或 CXC。

### 决策状态快照

截至 2026-08-11，各门禁状态如下：

| 门禁 | 状态 | 进入实现前还需完成 |
| --- | --- | --- |
| CFU-0 身份与载体 | revised proposal | 项目所有者接受 CXC、ZIP32、闭包和 identity 合同 |
| CFU-1 文档模型与引用 | partial acceptance | CXT/参数/Binding 基础子决策已接受；其余 Chart v4 和 lowering 细节随 ADR 整体接受 |
| CFU-2 时间、采样与跳转 | revised proposal | 接受边界表和绝对采样合同 |
| CFU-3 扩展、能力与预算 | revised proposal | 接受 capability、诊断和总量预算 |
| CFU-4 兼容与迁移 | revised proposal | 接受 v3 -> v4 和 v1/v2 经 v3 的迁移路径 |
| CFU-5 运行时与公共观察面 | revised proposal | 接受 project-document table、prepare options、API/version 方向 |

ADR 0038 的状态只有项目所有者可以改变。项目所有者已于 2026-08-11 授权进入 CFU-C；后续批次
仍须逐项满足本计划门禁，不能自动进入 Stage 4。

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

CFU-A 已完成仓库内盘点。仓库外旧 Chart 是否存在仍是 CFU-D 迁移发布门禁；在项目所有者明确
确认前，不得删除 v3 入口、缩短兼容窗口或声称全部外部资产已经迁移。

### CFU-B：合同与 ADR

状态：ADR 0038、`docs/CXC_FORMAT.md`、`docs/CHART_V4_FORMAT.md`、`docs/CXT_FORMAT.md`、
`docs/ANIMATION_MIXING.md` 和候选正反例已于 2026-08-11 按收口方案修订；CXT v1、播放前参数与
Template Binding 子决策已接受，ADR 0038 整体已于 2026-08-11 接受。

修订并接受现有 ADR 0038 及其字段级规范，并在整体接受前完成：

```text
至少三个正例和三个负例的规范 JSON/伪格式，至少包含一个 `.cxt` import 和一个 ParameterSet
字段路径、版本路由和错误 code 表
时间线/循环/绑定的边界表
参数 resolution、CXT lowering、三层 weight、CXC 项目声明闭包和 ID 作用域规则
ZIP32 canonical metadata、entry sentinel、manifest bytes 与 binary fixture 清单
package identity、semantic identity 和 parameter identity 的规范输入
generated composite identity、Template animator patch 和 canonical sort 规则
project-document table、per-prepare options 和内部 cuexis_cxc target 边界
source/package/runtime artifact 的所有权图
Stage 4 所需字段与明确延期字段列表
```

CFU-B 已于 2026-08-11 整体接受。CFU-C 可以按 C0-C4 顺序修改 Schema、Loader、fixture 和构建图，
但在对应退出门禁关闭前不得把局部产物描述为完整 CXC/Chart v4 生产入口。

### CFU-C：格式管线

按已接受 ADR 实现 CXC/CXT/Chart Schema、typed model、Reader、Writer/Canonicalizer、import/
parameter resolver 和 validator。Schema、typed Reader、语义 validator 与 writer 必须由共享 fixture
交叉验证；未知核心字段、非法版本、非有限值、参数类型、缺失 CXT、越界引用和超预算输入都要
产生稳定 code 与字段路径。

CFU-C 内部实施顺序固定为：

```text
CFU-C1  promote candidate examples; Chart v4/CXT/CXC manifest Schema and typed source model
CFU-C2  canonical Writer, parameter identity/resolution, CXT import and deterministic lowering
CFU-C3  internal cuexis_cxc strict ZIP32 reader/writer and package-backed provider
CFU-C4  pack/validate/unpack CLI and binary/round-trip golden
```

CFU-C1-C2 不得把动画求值放入 Chart compiler；含动画数据只形成 typed
`AnimationProgramInput` 和 capability requirements。

### CFU-D：迁移

新增显式迁移 CLI/API，保留 v1/v2/v3 读取回归。迁移结果必须是规范化输出，报告必须可机器读取；无法证明旧语义与新语义等价时失败。迁移不得把运行期状态、上一帧值或宿主 override 写入 Chart 基线。

实施快照（2026-08-13）：D1/D2 代码、库测试、CLI `--target` 和 `VerifyChartTools` 扩展已写入工作树；
默认仍输出 v3。同日稍后取得本 worktree Debug 证据并关闭 D1/D2；下一批次为 CFU-E。
D3 仍等待 Playback 能加载 v4。

### CFU-E：Runtime/Playback 接入

只接入格式所需的 project-document table、per-prepare ParameterSet、prepare、compile、identity、
capability 和 diagnostics 路径。空动画 v4 可以使用现有 Runtime；任意非空 Clip/CXT/Binding/Layer 在
Stage 4 前复用 `playback.capability.unsupported` 稳定拒绝。不得在 Chart compiler 内实现第二套动画
求值器。

### CFU-F：消费者与确定性

至少有一个无 GPU external consumer 和 Player/headless consumer 使用同一 public Playback 路径。覆盖：

```text
相同输入、不同 JSON 数组顺序和不同 Entity 遍历顺序产生相同摘要
round-trip 不改变语义 identity
filesystem/memory/host/CXC 对相同规范内容和参数产生相同 PreparedSemanticIdentity
CXC writer 在 MSVC、MinGW 和 Linux 上产生相同 package bytes
reload 失败不替换 active 内容
seek、stop、循环和缺失 capability 的诊断稳定
历史 FrameDigest golden 按版本保留，不静默重写
```

### CFU-G：阶段交接

只有满足本文“退出条件”，才能把 Stage 4 标为可开始。交接包必须链接：新 ADR、格式规范、Schema、迁移报告 golden、测试矩阵、外部 consumer 运行证据和残余风险清单。

## 5. 详细工作包

### 5.1 CFU-C0：启动门禁与依赖证明

CFU-C0 是零功能启动门禁，不产生可加载的新格式。入口条件是 ADR 0038 已被项目所有者明确整体
接受，并且 CXC、Chart v4、CXT 与 Animation Mixing 文档中的状态和接受日期一致。

任务：

1. 记录实施基线 commit、目标分支、已接受 ADR/spec commit 和当前 SDK API/build version。
2. 建立 CFU-B 候选示例到正式 fixture 的逐文件映射，说明哪些文件原样复制、哪些转换为二进制
   CXC，哪些继续只作为文档评审输入。
3. 用最小技术验证检查候选 archive 库是否能读取 local/central header、EOCD、ZIP64 sentinel、
   entry range、overlap 和 trailing bytes；不能证明这些能力时不得选定依赖。
4. 若引入直接依赖，同步规划 `vcpkg.json`、`docs/DEPENDENCY_POLICY.md`、
   `THIRD_PARTY_NOTICES.md`、安装许可证清单和 static/shared package 闭包。
5. 冻结 `cuexis_cxc` target 的依赖 allowlist、安装策略和测试 target 名称。
6. 为 CFU-E0 准备公共 API sketch，但不在 C0 修改安装头。

产物：实施基线记录、依赖选择记录、fixture promotion 清单、target/allowlist 草图和 API sketch。

退出门禁：依赖能力有可重复测试，许可证和分发路径明确，ADR/spec 无未决的生产语义占位；否则
停止在 C0。

### 5.2 CFU-C1：Schema、正式 fixture 与 typed source model

状态：已于 2026-08-11 完成；证据见
[CFU-C1 Reader 报告](../stage_reports/260811-chart-format-update-c1-reader.md)。

目标是建立“同一输入由 Schema 和 typed Reader 得出相同接受/拒绝结论”的最小生产格式层，不接入
Playback 或 Runtime。

任务：

1. 新增以下生产 Schema，并注册到 Schema artifact tests：

   ```text
   schemas/cuexis.chart.v4.schema.json
   schemas/cuexis.animation-template.v1.schema.json
   schemas/cuexis.cxc.v1.schema.json
   ```

2. 将已接受的候选示例复制或转换到 `tests/fixtures/chart_format_update/`，至少分为 `valid/`、
   `invalid/`、`binary/` 和 `golden/`；`docs/examples/` 继续保留为评审入口。
3. 为 JSON/CXT fixture 固定 LF，为 `.cxc` 标记 binary，避免 Git checkout 改变跨平台 golden bytes。
4. 在 `cuexis_chart` 中新增 Chart v4 source model、CXT document model、limits 和 Reader；模型保留
   ParameterRef、import、Animator、扩展和原始字段路径，不创建 Runtime/World 对象。
5. `ChartLoader` 继续只按顶层 `format/version` 路由；v4 使用独立 Reader，不在 v1-v3 Reader 中按
   字段猜测或插入兼容分支。
6. 在 `cuexis_cxc` 之前先建立独立的 CXC manifest typed model/Reader 接口；archive bytes 尚不进入。
7. 固定诊断产生顺序、字段路径和 1024 条上限；Schema 失败、typed Reader 失败和语义失败不得使用
   archive library 私有文本。
8. 保持 nlohmann JSON 和 Schema validator 只存在于 `cuexis_json_support`。

测试：

- 所有正式 valid/invalid JSON/CXT fixture 同时经过 Schema 和 typed Reader。
- 未知核心字段、重复 key、错误 version/domain、非有限数、非法 Rational、超深度和超字符串预算。
- v1/v2/v3 原有 Schema、Loader、迁移和 example tests 不变。
- 新增模型/header 的 ASCII、architecture 和 target allowlist 检查。

退出门禁：C1 只证明文档可被严格读取并保存为 source model；不得声称 CXC 可加载、参数已解析或
动画可执行。

### 5.3 CFU-C2：Canonical Writer、参数解析、CXT import 与 deterministic lowering

状态：已于 2026-08-11 完成；证据见
[CFU-C2 lowering 报告](../stage_reports/260811-chart-format-update-c2-lowering.md)。

目标是从 source documents 生成确定的 `ResolvedChartDocument` 和 `AnimationProgramInput`，并形成
跨 source identity 输入；仍不实现动画采样和混合。

任务：

1. 实现 Chart v4 与 CXT v1 Canonical Writer，固定对象 key、数组排序、Rational 约分、有限数、
   `-0` 规范化、UTF-8/LF 和单个结尾换行。
2. 将现有迁移器中的 JSON 组装逻辑提取为可复用 Writer，不建立第二套手写字符串序列化器。
3. 实现内部 `ChartParameterInput`/resolved value 模型、默认值合并、unknown/missing/duplicate/type/
   range/use validation 和参数 identity 编码。
4. Reader/Resolver 接收独立 project-document lookup；CXT path 不通过 `IContentProvider` 或伪
   AssetId 获取。
5. 处理顺序固定为 template expansion、concrete Object、参数冻结、CXT import、Binding lowering、
   mask/component/conflict/budget validation 和 capability requirement derivation；这里的 capability
   步骤只推导内容要求，不检查宿主是否提供能力。
6. CXT import 验证 source、format/version、templateId、required extension 和全部资源引用。
7. Template Binding lowering 生成 concrete Clip、Layer、Group、Instance 和复合 generated identity；
   不生成截断 hash，不写回 Chart。
8. 对参数、imports、clips、bindings、layers、groups、instances、tracks、segments、steps 和 masks
   执行规范排序、重复检测和 checked aggregate budgets。
9. `ResolvedChartDocument` 只含 concrete typed value；`AnimationProgramInput` 不含 JSON DOM、CXC/CXT
   path、ParameterRef、World、EnTT 或运行时脚本 hook。
10. 计算 canonical Chart/CXT/parameter identity components 和规范化 resource requirement table；Chart
    层不得声称已经拥有实际 resource semantic identity，也不在此处组装最终
    `PreparedSemanticIdentity`。CXC package hash 不参与该值；最终 identity 在 CFU-E3 完成资源获取和
    typed resource validation 后，由 Playback 组合这些 components 与实际资源 identity manifest。

测试：

- 参数默认、宿主覆盖、缺失、未知、类型、范围、`-0`、Rational 约分和禁止用途。
- CXT import 缺失、ID mismatch、重复 source、错误 version、未知 required extension 和资源闭包。
- 同一 CXT 绑定到不同 Object/parent/startBeat，生成记录稳定且不发生 ID 截断碰撞。
- Animator 整体 patch 接受、内部 deep patch 拒绝；mask 重复/前缀重叠/priority 冲突稳定失败。
- Override/Additive、Layer/Group/Instance weight 和离散属性限制与
  `ANIMATION_MIXING.md` 一致。
- 输入数组置乱、JSON key 置乱和等价 Rational 输入产生相同 Writer bytes、resolved model、identity
  components 和 resource requirement order；最终 Prepared semantic identity 的跨 source 一致性留给
  CFU-E3/F3 验证。
- Read -> Write -> Read 语义相等；unknown optional extensions 按合同保留。

收口 review 冻结了以下实现口径：CXT import source 必须使用小写 `.cxt`；canonical array 的主键
相同时按已规范化 record 的 compact JSON bytes 决胜；prepared-content Track/Segment/Step 总量同时
计入 imported source CXT Clip 和每个 Binding 生成的 concrete Clip；CXT import 诊断携带
package-relative source、template/import identity 与字段路径。既有 v3 projection 继续由
`CanonicalChartLoader` 调用 `ChartCompiler` 完成语义验证，不建立第二次编译路径。

退出门禁：C2 已完成 finding-first review，字段、排序、identity、诊断与预算 finding 均已关闭；不得
把该检查点描述为完整 CXC、package API、Playback 支持或 Stage 4 动画执行。

### 5.4 CFU-C3：内部 cuexis_cxc 与严格 ZIP32

目标是把 CXC archive/manifest/closure 封装为内部 target，并提供 file/memory 等价的 owning package
source。该 target 不成为公共 SDK component。

任务：

1. 新增 `engine/cxc/` 和内部 `cuexis_cxc` static target，注册 `CUEXIS_ACTIVE_TARGETS`、依赖
   allowlist、format target 和 architecture rules。
2. 允许依赖 `core/content/filesystem/project/chart/json_support` 和选定 archive 库；禁止依赖
   Playback、Runtime、World、Render、Audio、SDL、OpenGL 或 host SDK。
3. 在 archive 库之前执行有界 envelope validation：single disk、Stored、flags、version、时间戳、
   extra/comment、ZIP64 sentinel、local/central 一致、EOCD span、range overlap 和 trailing bytes。
4. 解析并验证 manifest 的 format/version、entry order、portable path、大小写折叠冲突、size、CRC、
   SHA-256、missing/unlisted 和总预算。
5. 读取 ProjectConfig、全部声明 Asset Index record/source/dependency、入口 Chart、CXT imports，建立
   项目声明闭包；pack 不裁剪 Asset Index，也不执行迁移。
6. 提供 package-backed `IContentProvider` 读取 AssetId bytes，同时提供独立 project-document table
   读取入口 Chart/CXT；两者共享 owning archive state，但类型域不混用。
7. File 和 memory source 必须拥有输入或稳定文件 handle；调用方释放原始 buffer 后仍可 prepare。
8. 所有 offset/count/size 使用 checked arithmetic；在任何大分配、解包或 source publication 前
   完成结构预算校验。
9. 计算精确 `.cxc` bytes 的 `CxcPackageIdentity`，但不把它替代 resource 或 prepared semantic
   identity。
10. static package 中把 `cuexis_cxc` 作为内部实现链接闭包导出/安装；shared package 不暴露
    `Cuexis::Cxc`、公共 header 或独立 find_package component。

测试：

- committed binary fixtures 覆盖 ZIP64、data descriptor、compression、multi-disk、extra/comment、
  directory/symlink、header mismatch、overlap、trailing bytes、CRC/hash/size mismatch。
- portable path 覆盖 `..`、绝对路径、反斜杠、保留名称、尾随点/空格、case-fold conflict。
- project closure 覆盖未引用但已声明的 Asset record、CXT 引用、隐藏 payload、缺失 dependency 和
  pack 保留 Asset Index bytes。
- file/memory CXC 得到相同 manifest、documents、asset bytes、diagnostics 和 identities。
- malformed archive 在 ASan/UBSan 下无越界、整数溢出或 partial publication。

退出门禁：CXC Reader/Writer 的 raw metadata 和 binary golden 必须通过独立安全审查；只验证
“archive 库能打开文件”不算关闭 C3。

实施状态：CFU-C3 已于 2026-08-12 关闭。内部 `cuexis_cxc`、strict ZIP32 Stored Reader/Writer、
manifest/project closure、owning file/memory package、Asset ContentProvider、独立 project-document
table、package identity、binary fixtures 和安装泄漏门禁已完成。证据见
[CFU-C3 报告](../stage_reports/260812-chart-format-update-c3-cxc.md)。该检查点不得描述为完整 CXC、
公共 package API、Playback 支持或 Stage Chart Format Update 完成。

### 5.5 CFU-C4：CXC 工具、原子输出与 binary golden

新增并复用同一 `cuexis_cxc` 实现：

```text
cuexis_cxc_pack
cuexis_cxc_validate
cuexis_cxc_unpack
```

CLI 规则：

- exit `0` 表示成功，`1` 表示内容无效，`2` 表示参数、I/O 或目标提交失败。
- 诊断写 stderr，成功摘要写 stdout；字段路径和 package-relative path 稳定，不输出宿主绝对路径。
- pack 输入是 Source Project 根，输出是独立 `.cxc`；不得覆盖输入、迁移 Chart、裁剪 Asset Index 或
  执行脚本。
- validate 不写输出，必须执行 envelope、manifest、Project、Asset Index、Chart、CXT 和项目声明资源
  闭包的全部校验；它不执行 Playback、Runtime、动画求值或宿主 capability preflight。
- 若 CXC 合同要求某类资源 payload 的 typed validation 才能证明闭包，工具只能调用现有共享验证边界，
  不得在 `cuexis_cxc_validate` 或 CLI 中复制资源解析器；动画 capability 仍只在 Playback prepare
  中检查。
- unpack 只恢复包内 source tree；目标已存在且非空时拒绝，先写 sibling staging directory，全部
  校验和写入成功后再原子提交。
- pack 必须先把声明 entry 快照到有界 staging，计算 size/hash 后再写 manifest-first archive；不得
  在可变源目录上执行未校验的两次读取。
- 临时文件、目录和 backup 的清理不抛异常；失败时输入和已有目标保持不变。

工程门禁：

- 新增类似 `cmake/VerifyChartTools.cmake` 的 CMake script-mode CLI 测试。
- canonical writer 生成的 `.cxc` 与 committed SHA-256 golden 一致。
- pack -> validate -> unpack -> repack bytes 一致；noncanonical-but-valid reader 输入重新 pack 后得到
  canonical bytes。
- 工具关闭后才允许进入迁移和 Playback 接入。

实施快照（2026-08-13）：三个 developer-only CLI、bounded source snapshot、sibling staging、
atomic commit、no-overwrite、exit `0/1/2`、canonical/noncanonical round-trip 和失败清理已在
MSVC Debug/Release、MinGW Debug/Release、hosted GCC Release 与 Clang Shared Debug 门禁中验证。
最终 SHA `41ddb6a980b816b2c0b3b1e25df9268603bcc883` 的 Linux Quality、Windows MSVC 和 Windows
MinGW workflow 全部成功；CFU-C4 已关闭，下一批次为 CFU-D，不进入 CFU-E 或 Stage 4。

### 5.6 CFU-D：显式迁移与审计报告

CFU-D 分为三个工作包：

```text
CFU-D1  library migration and canonical v4 output
CFU-D2  CLI compatibility and atomic report/output
CFU-D3  runtime equivalence evidence after CFU-E
```

任务：

1. 保留 `ChartMigrator::migrateToV3` 和现有 CLI 调用行为；新增 v3 -> v4 路径，v1/v2 -> v4 必须先
   复用现有 v3 migration，不复制旧版本解析逻辑。
2. CLI 可增加显式 target version 选项，但旧参数组合默认继续输出 v3，避免破坏已有自动化。
3. v3 -> v4 只增加规范要求的空字段并提升 version；不生成 CXT、参数、Animator、Clip、Binding、
   capability workaround 或脚本。
4. 报告记录 source/target version、canonical identity、字段计数、基准修改、警告和诊断；不能把
   CXC pack 或 unpack 记录为 Chart migration。
5. 输入、输出和报告路径互不冲突；沿用现有双输出临时文件/backup/commit 事务。
6. 项目所有者必须在 CFU-D 关闭前确认仓库外旧 Chart 资产清单或明确记录“未提供外部资产”；未
   确认时保留全部 v1/v2/v3 Reader 和迁移入口。

门禁：结构/Writer golden 在 D1-D2 关闭；FrameSnapshot、FrameDigest v3 和 seek/stop 等价证据在
CFU-E 接入后由 D3 关闭。

实施快照（2026-08-13）：`ChartMigrator::migrateToV4`、CLI `--target 3|4`、v3 报告 golden 保持、
v4 chart golden 与结构性 report 检查已落地。本 worktree Debug 证据见
[CFU-D1/D2 报告](../stage_reports/260813-chart-format-update-d-migration.md)。D1/D2 已关闭；
整包 CFU-D 未关。仓库外旧 Chart 资产仍未确认，全部 v1/v2/v3 Reader 与迁移入口保留。

### 5.7 CFU-E：PlaybackSource、prepare options 与事务接入

#### CFU-E0：公共 API 与版本门禁

在修改安装头以前，先提交 API sketch 并单独评审。候选职责名称如下，确切拼写和签名由 E0 冻结：

```text
ChartParameterSet / PlaybackPrepareOptions
PreparedSemanticIdentity
owning project-document source type
PlaybackSource CXC file/memory factories
prepareLoad/prepareReload/load/reload options overloads
PreparedPlayback and active-session semantic identity observation
```

API 约束：

- 不改变现有 `TypedPlaybackProject` aggregate layout；需要多 document typed host source 时新增 owning
  类型或 factory，避免破坏 0.5.x aggregate initialization。
- 现有无 options overload 保留，并委托给 empty `PlaybackPrepareOptions`。
- Parameter value 必须保留 number/rational/weight type tag；公共头不得暴露 `chart::RationalBeat`、
  JSON DOM、archive handle 或内部 resolver 类型。
- 新 public types 使用 owning ASCII string/固定宽度整数/`std::array` 等稳定 C++20 类型，安装头纯
  ASCII，异常不跨边界。
- 这是候选 additive SDK surface，`0.6.0` 只是 E0 的候选 `CUEXIS_SDK_API_VERSION`；确切版本由
  项目所有者在 E0 冻结，在此之前不得修改版本或生成头。

#### CFU-E1：统一 PlaybackSource 内部形状

所有 factory 最终建立同一个内部 state：

```text
source identity/ownership
project-document table: project-relative path -> bounded owning UTF-8 bytes
optional AssetDatabase
owning IContentProvider
optional CxcPackageIdentity metadata
```

`fromChartText` 建立单 document source；现有 typed/filesystem factory 适配该形状；新增 CXC
file/memory factory 使用 `cuexis_cxc`。ParameterSet 不进入 source state。

#### CFU-E2：prepare、capability 与 resolved content

1. Playback 把 public ParameterSet 转换为 `cuexis_chart` 内部 typed input，再调用 C2 resolver。
2. 先完成 parse/semantic/import/parameter/预算校验，再执行 capability preflight，保证 malformed
   内容不会被笼统 capability 错误遮蔽。
3. 实现并声明 `cuexis.chart.v4`、`cuexis.source.cxc.v1`、`cuexis.source.cxt.v1` 格式能力；Stage 4
   前不声明 animation execution capability。
4. 空动画 v4 和静态/参数化 Transform/Camera 可以编译到现有 ChartRuntime；任意非空 CXT import、
   Clip、Binding、Layer 或 Instance 在 Stage 4 前以 `playback.capability.unsupported` 失败。
5. `AnimationProgramInput` 只保存在 candidate typed state，不能由 Chart compiler 自行采样或写 World。

#### CFU-E3：identity、reload 与失败原子性

1. Prepared candidate 保存 ParameterSet、resolved document、C2 产出的 semantic identity components、
   capability summary、AssetDatabase/provider、已获取且验证过的 resource identity manifest，以及现有
   presentation candidate。
2. 在资源获取和 typed resource validation 成功后，Playback 使用 canonical Chart/CXT/parameter
   components 加按 AssetId 排序的实际 resource identity manifest 组装最终 `PreparedSemanticIdentity`。
   同一 source + 同一规范参数跨 filesystem/memory/host/CXC 必须得到相同值；不同参数必须得到不同值。
3. `CxcPackageIdentity` 只用于包相等/传输缓存；更换 archive metadata 但保持规范内容时不能改变
   prepared semantic identity。
4. `initial load`、`reload`、`discard` 和 `commit` 沿用现有 owner/generation/candidate token；parse、参数、
   import、capability、资源或 identity 失败均不替换 active Playback。
5. active Session 的 `FrameSnapshot` 和 FrameDigest v3 结构不变；属性 provenance 只进入独立诊断或
   Debug snapshot。

#### CFU-E4：E 批次门禁

- 旧 `loadChart`、v1/v2/v3、filesystem/typed project、static/shared consumer 全部回归。
- empty v4 file/memory/host/CXC 全部成功且帧等价。
- nonempty animation v4 在 World/资源发布前稳定失败，旧 active 内容保持。
- 参数化 static v4 的默认/override/reload identity 和 FrameDigest 结果稳定。
- 安装公共头无内部 Chart/CXC/JSON/archive 类型；shared export surface 只增加 E0 已批准的 API symbol，
  其 SDK compatibility version 按 E0 最终结论处理。

### 5.8 CFU-F：消费者、确定性、安全与性能

CFU-F 分为：

```text
CFU-F1  headless and failure-path integration
CFU-F2  static/shared package and external consumers
CFU-F3  deterministic writer/identity cross-platform parity
CFU-F4  limits, sanitizer, allocation and performance evidence
```

任务：

- 建立不依赖 GPU 的 static v4 CXC reference project；非空动画 fixture 只用于稳定 capability 拒绝，
  不放入 Player 默认播放内容。
- external consumer 只包含安装的 Playback headers，覆盖 CXC memory/file、typed documents、prepare
  options、semantic identity 和失败 reload。
- static/shared、add_subdirectory/find_package、headless、adapter-disabled 均不传递 archive public
  header；static package 能解析内部 `Cuexis::InternalCxc` 链接闭包。
- 在 MSVC、MinGW、hosted GCC/Clang 上生成同一 canonical CXC 并比较 committed SHA-256；比较
  resolved identity、迁移 golden 和 diagnostics 顺序。
- sanitizer 覆盖 archive/parser/offset/closure；clang-tidy 覆盖 Playback + CXC + Chart 变更面；coverage
  报告单列 CXC/Chart v4 未覆盖分支。
- warmed-up v1-v3 和 empty v4 update/extract 不新增分配；CXC/参数/CXT 处理只发生在 source/prepare
  路径，不进入每帧更新。
- 最大合法总量、边界+1、整数溢出和诊断截断使用小型构造器/伪 header 测试，避免 CI 为每个错误
  fixture 实际分配 512 MiB。

退出门禁：本地结果不能替代 hosted CI；跨平台 golden 必须绑定最终实现 SHA，任何 pending/failed
job 都使 CFU-F 保持未关闭。

### 5.9 CFU-G：验收、封存与 Stage 4 交接

任务：

1. 对最终 SHA 运行 Debug/Release fresh configure、clean build、完整 CTest、format、architecture、
   public-header ASCII、version、license 和 `git diff --check`。
2. 运行 hosted Linux Quality、Windows MSVC、Windows MinGW；记录 run URL、job 和第一失败步骤，
   不用本地结果替代远端失败。
3. 创建 `docs/stage_reports/stage_chart_format_update_completion_report.md`，只记录最终实现和实际
   证据，不复制完整格式合同。
4. 把 ADR/spec 状态从 accepted candidate 更新为 implemented 的条件写入 CURRENT_STATUS；只有全部
   门禁通过后才更新状态，不能因代码合并提前宣称完成。
5. 生成 Stage 4 handoff：typed `AnimationProgramInput`、capability names、fixture、预算、diagnostics、
   未实现运行时职责和残余风险。
6. Stage 4 只在项目所有者接受完成报告后开始；格式阶段不得自动创建 AnimationSystem 实现提交。

## 6. 模块、target 与文件落点

目标依赖方向：

```text
cuexis_chart
  -> cuexis_core + cuexis_json_support
  -X-> cuexis_cxc / playback / runtime / world / animation

cuexis_cxc (internal static)
  -> core + content + filesystem + project + chart + json_support + private archive dependency
  -X-> playback / runtime / world / render / audio / SDL / OpenGL

cuexis_playback
  -> existing private modules + cuexis_cxc
  exposes only Cuexis-owned Playback types

cxc tools
  -> cuexis_cxc (+ chart/project only when orchestration requires)

Stage 4 cuexis_animation
  -> consumes AnimationProgramInput after this stage
  -X-> JSON / CXC / CXT parsing
```

`cuexis_cxc` 的 package 规则：

- 不提供 `Cuexis::Cxc` public alias、安装 header 或 find_package component。
- shared Playback 将实现私有链接进 Playback；archive DLL 不得成为未记录的运行时依赖。
- static Playback 把 `cuexis_cxc` archive 作为 `CUEXIS_STATIC_IMPLEMENTATION_TARGETS` 的内部导出
  闭包，使 `Cuexis::Playback` 可链接，但不把它宣传为可直接消费组件。
- 新 target 必须进入 `CUEXIS_ACTIVE_TARGETS` 和 `cuexis_verify_target_dependencies`；architecture tests
  必须禁止 archive/JSON 类型进入安装头。

预计职责落点：

| 职责 | 所有者 | 允许修改 | 禁止方向 |
| --- | --- | --- | --- |
| Chart v4/CXT Schema | `schemas/` + `tests/json_support` | 新增 versioned artifact 和正反例 | 改写 v1-v3 Schema |
| Source/Resolved model | `cuexis_chart` | 新 Reader/Writer/resolver/internal transfer model | 创建 World、解析 CXC |
| Parameter resolution | `cuexis_chart` internal | typed input、identity、limits、diagnostics | 依赖 Playback public type |
| Archive/manifest/closure | `cuexis_cxc` | strict reader/writer/provider/doc table | 暴露 public component |
| Source/API/transaction | `cuexis_playback` | additive factories/options/identity observation | 暴露 JSON/archive/Chart internals |
| Migration | `cuexis_chart` + existing migrator tool | v3 -> v4、报告、旧 CLI 兼容 | pack/unpack 或隐式迁移 |
| CLI | `tools/cxc_*` | 复用 library、原子输出、script-mode tests | 复制格式解析器 |
| Runtime animation | Stage 4 | 只消费 typed input | 在本阶段实现 |

建议文件布局（确切文件名可以在对应批次内按现有命名习惯调整，但职责不得移动）：

```text
engine/cxc/CMakeLists.txt
engine/cxc/src/archive_envelope.*
engine/cxc/src/cxc_manifest.*
engine/cxc/src/cxc_reader.*
engine/cxc/src/cxc_writer.*
engine/cxc/src/cxc_closure.*
engine/cxc/src/cxc_content_provider.*

engine/chart/.../chart_v4_reader.*
engine/chart/.../chart_writer.*
engine/chart/.../animation_template_reader.*
engine/chart/.../chart_parameter_resolver.*
engine/chart/.../animation_lowering.*
engine/chart/.../prepared_semantic_identity.*

tools/cxc_pack/
tools/cxc_validate/
tools/cxc_unpack/
tests/cxc/
tests/fixtures/chart_format_update/
cmake/VerifyCxcTools.cmake
```

## 7. 公共 API、版本与兼容策略

### 7.1 API 演进原则

- 保留现有 `fromChartText`、`fromTypedProject`、`fromFilesystemProject`、无 options prepare/load/reload
  和 v1-v3 行为。
- 不给现有 public aggregate 直接追加字段；使用新 owning type、factory 或 overload，降低 source/ABI
  破坏。
- ParameterSet 属于每次 prepare/reload，不能保存在 PlaybackSource、ProjectConfig、CXC manifest 或
  active Chart bytes 中。
- project-document table 是 source 内容；AssetId bytes 仍由 `IContentProvider` 提供。
- Prepared semantic identity 可以只读公开；CXC package identity 默认留在 inspection/tool/source
  metadata，不进入 FrameSnapshot 或 FrameDigest。
- public headers 只使用 ASCII 和 Cuexis-owned types；不安装 Chart/CXC internal headers。

### 7.2 版本计划

```text
Chart format version          4
CXT format version            1
CXC format version            1
FrameDigest                   remains 3
stable C ABI                  unchanged, still deferred to Stage 12
candidate SDK API target      0.6.0, subject to E0/project-owner approval
date-based build version      updated only through tools/update_version.py at release/merge gate
```

若 E0 采用 0.6.0，必须增加 package compatibility rejection tests、static/shared symbol/export review
和 clean consumer evidence；若 E0 最终证明无需安装头变化，项目所有者可以保留 0.5.x，但该结论
必须在 ADR/plan 中显式记录，不能由实现者默认为 patch-compatible。

### 7.3 Capability 计划

格式阶段实现后可以提供：

```text
cuexis.chart.v4
cuexis.source.cxc.v1
cuexis.source.cxt.v1
```

以下能力只能由 Stage 4 实现并声明：

```text
cuexis.animation.clip.v1
cuexis.animation.layers.v1
```

因此 CXT/Clip/Binding/Layer 的结构校验可以成功，但 Stage 4 前 prepare 必须缺 animation capability
失败。不得临时声明 capability 后忽略求值。

## 8. Fixture、测试与证据矩阵

### 8.1 Fixture 分层

```text
docs/examples/chart_format_update/
  candidate review inputs; never loaded by production tests before ADR acceptance

tests/fixtures/chart_format_update/valid/
  production JSON/CXT/manifest fixtures

tests/fixtures/chart_format_update/invalid/
  one-primary-error semantic fixtures with expected diagnostics

tests/fixtures/chart_format_update/binary/
  malformed and noncanonical CXC envelope fixtures

tests/fixtures/chart_format_update/golden/
  canonical Chart/CXT/manifest/CXC bytes, identities and migration reports
```

每个 invalid fixture 必须记录 primary diagnostic；如果输入有多个错误，验证顺序和截断规则也必须
固定。Binary golden 不通过文本换行转换，JSON/CXT canonical golden 固定 LF。

### 8.2 测试矩阵

| 范围 | 必须覆盖 | 主要 test owner |
| --- | --- | --- |
| Schema | valid/invalid、unknown core、required fields、version/domain | `tests/json_support` |
| Reader/Writer | round-trip、key/array order、Rational、extensions、non-finite | `tests/chart` |
| Parameters | default/override/missing/type/range/use/identity | `tests/chart` |
| CXT | import/path/ID/version/extensions/resources/lowering | `tests/chart` |
| Animator | patch、mask、priority、weights、discrete、generated identity | `tests/chart` |
| ZIP envelope | header/EOCD/sentinel/range/overlap/trailing/metadata | `tests/cxc` |
| Manifest | order/path/case/duplicate/size/hash/missing/unlisted | `tests/cxc` |
| Closure | Project/Asset Index/CXT/resource/dependency/hidden payload | `tests/cxc` |
| Tools | usage/exit code/atomic output/no overwrite/round-trip | CMake script tests |
| Migration | v1/v2->v3->v4、report、path conflict、semantic equivalence | `tests/chart` |
| Playback | source parity、options、capability、commit/reload rollback | `tests/playback` |
| Identity | package vs semantic、provider parity、parameter changes | Chart/CXC/Playback tests |
| Package | static/shared、add_subdirectory/find_package、version reject | `tests/external` |
| Architecture | allowlist、public includes、exports、ASCII、no archive leak | CMake architecture tests |
| Cross-platform | MSVC/MinGW/GCC/Clang byte and diagnostic parity | hosted workflows |
| Safety/performance | sanitizer、overflow、budget+1、allocation、prepare peak | Linux Quality + probes |

### 8.3 每批最小验证

每个生产代码批次至少执行：

```powershell
cmake --preset debug --fresh
cmake --build --preset debug
ctest --preset debug --no-tests=error
cmake --build --preset debug --target cuexis_format_check
python -B tools/check_docs.py
python -B tools/update_version.py --check
git diff --check
```

修改公共 API、package 或 Release 行为的批次还必须执行 fresh/clean Release、static/shared external
consumer 和 package install。最终关闭依赖 hosted Linux/Windows 全部 workflow 的精确 SHA 证据。

## 9. 安全、事务、性能与提交门禁

### 9.1 不可信输入与路径

- CXC、Chart、CXT、Project、Asset Index 和资源 bytes 都按不可信输入处理，来源类型不改变预算。
- Envelope validation 先于 archive library extraction；manifest/hash/path/closure 先于 source publication。
- pack 对声明文件建立一次性有界快照，防止 hash 后源文件被替换；symlink/special file 直接拒绝。
- unpack 使用 staging directory，拒绝路径别名、case conflict、junction/symlink escape 和已有非空目标。
- 任何相加、相乘、offset、count 和 decompressed/listed total 都使用 checked arithmetic。
- 诊断达到 1024 条后停止追加并加入稳定 truncation diagnostic，不继续无界遍历错误上下文。

### 9.2 所有权与事务

```text
factory success
  -> owning PlaybackSource
prepare success
  -> owning PreparedPlayback candidate
adapter/resource validation
  -> Playback commit
  -> noexcept activation
```

任一步失败都销毁 candidate/staging，不修改 active Session、ResourceManager、presentation cache、输出
文件或目标目录。析构、discard、rollback 和临时清理不得抛异常。

### 9.3 线程与性能

- v1 首版保持同步 owner-thread API，不增加 callback、future、取消 token 或隐式 worker ABI。
- JSON/CXT/CXC、hash、closure、parameter resolution 和 lowering 只发生在 factory/prepare/migrate/tool
  路径，不进入 `update()`、`extractFrame()`、audio/render 实时路径。
- empty v4 warmed update/extract 必须保持既有零分配/稀疏工作保证。
- 性能 probe 记录合法最大内容的 prepare wall time、peak memory、hash/write throughput 和 reload peak；
  数值用于回归趋势，不替代预算/正确性门禁。

### 9.4 Review 与提交检查点

| 检查点 | 可提交内容 | 必须通过后才能继续 |
| --- | --- | --- |
| P0 | 接受后的 ADR/spec/status 更新 | 项目所有者明确接受；docs/version/diff checks |
| P1 | C1 Schema、fixture、typed Reader | Schema/Reader parity、v1-v3 回归、独立 review |
| P2 | C2 Writer/resolver/lowering/identity | round-trip/determinism/diagnostic review |
| P3 | C3-C4 CXC target、tools、binary fixtures | security review、sanitizer、tool atomicity |
| P4 | D migration | old CLI compatibility、report golden、no overwrite |
| P5 | E public API/Playback integration | E0 approval、SDK/package/external consumer、rollback |
| P6 | F-G acceptance/docs | all local + hosted exact-SHA evidence |

每个检查点使用可审查的独立 commit；不得把大规模无关重构、Stage 4 runtime 或新的脚本议题混入。
某一检查点失败时修复根因并重跑该检查点及受影响下游，不通过删除测试、放宽预算或改写 golden
掩盖差异。

## 10. 预期变更面

实施时预计涉及以下边界；本阶段计划不预先修改这些文件。每一项都必须在对应工作包中说明
实际新增、删除或仅作注册/验证改动，避免把候选能力误报为已经存在：

```text
权威文档与状态
  docs/CHART_FORMAT.md、docs/CHART_V4_FORMAT.md、docs/CXC_FORMAT.md、docs/CXT_FORMAT.md
  docs/ANIMATION_MIXING.md、docs/adr/0038-cxc-v1-and-chart-v4-boundary.md
  docs/CURRENT_STATUS.md、docs/ROADMAP.md、docs/stage_plans/README.md
  docs/stage_reports/stage_chart_format_update_completion_report.md（仅在完成门禁后新增）
  docs/DEPENDENCY_POLICY.md、THIRD_PARTY_NOTICES.md（仅在依赖闭包变化时）

格式、Schema 与 fixture
  schemas/cuexis.chart.v4.schema.json、schemas/cuexis.animation-template.v1.schema.json
  schemas/cuexis.cxc.v1.schema.json（若最终保留独立 manifest Schema）
  docs/examples/chart_format_update/（评审输入，不直接成为生产 fixture）
  tests/fixtures/chart_format_update/{valid,invalid,binary,golden}/
  .gitattributes（JSON/CXT LF 与 CXC binary 规则）

构建、依赖、安装与版本
  CMakeLists.txt、engine/CMakeLists.txt、tools/CMakeLists.txt、tests/CMakeLists.txt
  engine/{chart,cxc,playback}/CMakeLists.txt、tools/{chart_migrator,chart_validator,cxc_*}/CMakeLists.txt
  cmake/CuexisConfig.cmake.in、cmake/CuexisVersion.cmake
  cmake/VerifyCxcTools.cmake、cmake/VerifyArchitecture.cmake、cmake/VerifyExternalConsumer.cmake
  cmake/VerifySharedExports.cmake、cmake/VerifySharedConsumerImports.cmake
  vcpkg.json、vcpkg-configuration.json（仅在 baseline 变化时）

实现模块与工具
  engine/cxc/*（内部 cuexis_cxc target，不作为安装 component）
  engine/chart/include/cuexis/chart/*、engine/chart/src/*
  engine/playback/include/cuexis/playback/*、engine/playback/src/*（仅 E0 批准的 additive API）
  tools/chart_validator/*、tools/chart_migrator/*、tools/cxc_*
  app/player/*（只接入现有 Playback public path 的 headless/diagnostic fixture）

测试、消费者与 hosted CI
  tests/chart/*、tests/cxc/*、tests/playback/*、tests/runtime/*、tests/external/*
  tests/external/{add_subdirectory_playback,find_package_playback}/*
  .github/workflows/linux-quality.yml、windows-msvc.yml、windows-mingw.yml
  assets/charts/*、assets/projects/*（仅经项目所有者批准的正式 fixture）
```

`engine/animation/`、`engine/behavior/` 和 `docs/ANIMATION_MIXING.md` 在本阶段只接收格式边界与交接说明；动画求值实现仍属于 Stage 4。

## 11. 退出条件

Stage Chart Format Update 完成必须同时满足：

```text
格式 ID、载体、版本、时间域、引用和扩展合同已有接受的 ADR
规范文档、Schema、typed Reader、Writer/Canonicalizer 和 validator 一致
v1/v2/v3 仍按历史语义读取，显式迁移有可审计报告且不覆盖源文件
新格式的合法/非法、预算、缺资源、缺 capability 和未知扩展 fixture 完整
Playback prepare/activate/reload/seek 的成功与失败路径有 headless 证据
至少一个 clean external consumer 只使用 public Playback headers
round-trip、FrameDigest/identity 和数组/实体顺序确定性 golden 通过
CXC binary metadata/entry order/package bytes 跨平台 golden 通过
空动画 v4 通过；非空动画在 Stage 4 前以既有 capability code 稳定拒绝
PlaybackSource 不暴露 archive 类型，CXT 不伪装成 AssetId，ParameterSet 不进入 source identity
archive 依赖的许可证、静态/共享链接闭包、安装许可证清单和退出路径已审计
E0 批准的公共 API、SDK compatibility version、symbol/export 与 clean external consumer gate 全部通过
pack/validate/unpack 的 exit code、原子输出、no-overwrite、staging cleanup 和 round-trip golden 通过
仓库外旧 Chart 资产清单已得到项目所有者确认，或明确记录未提供外部资产及兼容窗口决策
最终实现 SHA 对应的 hosted Linux/Windows workflow 已完成，记录 run URL、job 结果和第一失败步骤（如有）
completion report 已创建并由项目所有者接受；报告只引用实际证据，不用计划文字替代结果
Stage 4 所需动画字段已列明，延期字段不会被隐式实现
docs/PROJECT_GUIDE.md、cuexis_sdk_transition_plan.md、CHART_FORMAT.md、CHART_V4_FORMAT.md、
  CXC_FORMAT.md、CXT_FORMAT.md 和 ANIMATION_MIXING.md 的状态与链接一致
```

任何一项未满足时，状态仍为“规划中”或“实现中”，不得以“格式更新已完成”或“CXC 已支持”对外描述。

## 12. Stage 4 交接边界

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
