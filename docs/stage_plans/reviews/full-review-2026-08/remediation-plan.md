# 260829 Full Review 整改实施计划

状态：completed；2026-08-30 Full Review 最终 SHA hosted 验证与 owner acceptance 已完成；本文件保留任务拆分和处置映射

更新日期：2026-08-30

来源：[260829 全项目代码审查](../../../stage_reports/reviews/full-review-2026-08/2026-08-29-review.md)

## 1. 文档定位

本文是针对 2026-08-29 全项目代码审查的实施计划。审查报告是问题证据和原始编号的唯一
来源；本文负责把问题拆成可交付给后续实现模型的批次、任务卡、依赖关系、实现边界和验收
门禁。本文不替代 ADR、格式规范、当前状态页或审查报告，也不把计划中的工作描述为已完成。

本计划明确遵守以下规则：

- 不修改 `docs/stage_reports/260829-full-review.md`。它是只读历史审查快照。
- 不因本计划改变 `docs/CURRENT_STATUS.md` 的 Stage 5 状态。只有实现、同 SHA 验证和项目
  所有者接受后，才可更新当前状态和新增完成报告。
- 每个已完成批次必须在 `docs/stage_reports/` 新增带日期的整改/复审报告，并逐条回指本计划
  和原审查 finding 编号。
- 不借整改之名提前实现 Studio、Judgement/Replay、稳定 C ABI、运行时脚本或其他未排期能力。
- 任何公共 API、格式、错误码、identity 或 digest 变化，都必须先经过本文的决策门，不得由
  实现模型自行猜测。

审查共确认 144 项 finding：1 项 P0、15 项 P1、56 项 P2、72 项 P3。架构 allowlist、JSON/GL/
GLM 隔离、公共头泄漏和实时音频回调等硬约束在审查时全部通过；因此本计划不把已经通过的
约束重列为待修复项，但每个涉及的批次仍须运行相应回归门禁。

## 2. 基线和不可破坏的不变量

后续实现模型开始任何任务前，必须先确认以下事实。若工作树或文档显示事实不同，应停止编码，
报告“基线漂移”，由维护者重新校准计划。

### 2.1 版本和阶段

- 审查证据基线是 commit `d380fc91e59ed893a3b17128d5d6481d924711e7`；后续仅文档提交不能
  被误称为 finding 已修复。
- SDK API 当前为 `0.7.0`，日期构建版本仍由 `cmake/CuexisVersion.cmake` 与 `vcpkg.json`
  共同决定。
- Stage 4 已关闭。Stage 5 的 S5-A 至 S5-G 已完成，S5-H 只有 local checkpoint，hosted
  验证和 owner acceptance 仍是 Stage 5 完成门。
- Runtime 脚本和逐帧回调无限期延后，不能在整改中加入字段、extension、capability、字节码
  或隐式执行入口。

### 2.2 公共观察面和边界

- 外部宿主只通过 `PlaybackSession`、`PlaybackSource`、`RuntimeFrame`、`FrameSnapshot`、
  `FrameDigest` 和 `ContentProvider` 观察或驱动 Playback。
- `RuntimeSession`、World、EnTT、SDL、OpenGL、JSON DOM 和 shader 编译器不能泄漏到安装的
  Playback 公共头或 Playback-only consumer。
- `cuexis_playback` 不链接 SDL、OpenGL、GLAD 或 shaderc；`cuexis_shader` 是可选内部实现，
  不成为 Playback 的传递依赖。
- FrameDigest v1-v3 的历史定义必须保持，除非先有明确 ADR/owner 决策；性能优化不能偷偷改变
  digest 字段、排序或 canonical bytes。

### 2.3 事务、线程和错误处理

- prepare/reload 失败不得改变 active World、resource scope、presentation、identity、frame
  或有效 diagnostics，除非已有合同明确规定失败后的状态（音频 replacement 的决策门另述）。
- 公共模块边界不能让异常穿越。返回 `Result` 的路径不能静默丢弃错误；显式忽略必须使用项目
  现有的 `static_cast<void>` 形式。
- `PlaybackSession`、RuntimeSession、SDL 窗口和音频 owner-thread 约束必须与实现真实行为一致。
- 热帧（已预热的 `update`/`extractFrame`）不能编译 shader、打开文件、扫描工程树或引入不受控
  的堆分配；若某一动画/参数化路径只能有界分配，必须在规范和测试中明确范围。

## 3. 执行协议：统一任务卡

每张任务卡都应单独执行、单独验证、单独报告。实现模型不得一次性跨越多个批次，除非本文
明确把它们列为同一任务的搭车项。

### 3.1 任务卡必填内容

交付给实现模型的提示词至少包含：

1. **任务 ID 和 finding**：例如 `B0-CH01`，列出原始 `CH-01` 和精确文件。
2. **前置条件**：需要哪个决策门、哪个批次已通过、是否允许改公共头/规范。
3. **目标行为**：用可观察的输入、输出、错误码、状态或复杂度描述，不只说“重构”。
4. **允许改动范围**：源文件、测试文件、必要的 CMake/文档文件；未列文件默认不改。
5. **实现步骤**：按检查、数据流、提交点、异常处理和所有权顺序写出。
6. **禁止事项**：不得改哪些 API/格式/digest/排序/默认 capability，不能删除哪些兼容路径。
7. **测试命令和通过标准**：至少一个聚焦测试，必要时加 Debug/Release/headless 门禁。
8. **输出物**：修改摘要、测试结果、golden 是否变化、未解决风险、下一步依赖。

### 3.2 停止条件

遇到以下任一情况必须停止并返回问题，不得猜测：

- 需要在 PB-01、CX-01、PB-04、CH-03 或 RT-04 的两种语义之间选择，而决策门尚未关闭。
- 现有测试与本文基线冲突，或审查行号对应的代码已经移动且无法确认等价位置。
- 需要修改已接受 ADR、格式字段、公共错误码、SDK minor/API 版本或 FrameDigest 定义。
- 需要改变默认 capability、安装组件、依赖 allowlist 或跨模块链接闭包，但任务卡没有明确授权。
- 发现第三方依赖版本、平台工具链或 hosted workflow 与当前门禁不一致。

### 3.3 推荐提交边界

- 第 0 批和第 0.5 批各自形成小提交，便于回滚。
- 第 1 批 identity/cache、math、docs/CI、reload 四条线分别提交。
- 第 2 批按 render、audio、resource-index 三个提交组拆分。
- 第 3 批的 prepare 分层和 parse-once 在同一功能分支逐步提交，但每一步都保持构建可用。
- 每个批次完成后固定运行 `git diff --check`，保存测试日志和实现 SHA。

## 4. 决策门

审查报告把五项问题列为 owner/spec decision。下面给出推荐选项、备选选项和影响。推荐选项
不是自动生效的产品决策；维护者须在任务卡中明确采用哪一个。

### D1：PB-01，Chart v4 与 legacy renderable

**问题**：v4 resolver 对 renderable 无条件产生资源需求，而非 `CXPRES01` 的 legacy payload
无法形成 presentation，最终以 `playback.identity.resource_missing` 等误导错误失败；v3 有
不同兼容路径，CHART_V4 没写明限制。

**推荐 A（保持安全边界）**：冻结“v4 中被实际渲染的 renderable 必须使用 portable
`CXPRES01` 资源”；在最早可判定位置发出专用码
`playback.chart.v4.requires_portable_presentation`，携带 object/asset 上下文；不把 legacy
payload 静默转换为 Unlit；补 CHART_V4 和 Playback spec 条款。

**备选 B（兼容 v3 语义）**：让 v4 对 legacy presentation 走与 v3 相同路径；必须证明
FrameSnapshot、identity、资源闭包和 validation sink 都不丢信息，并补 v4+legacy 组合测试。

**禁止**：只改错误文案而不决定是否支持；或在没有 spec 更新的情况下让 legacy 资源偶尔成功。

### D2：CX-01，Asset Index record 级 extensions

**问题**：实现保留 record 级 `extensions`，但 ADR 0026、Asset Index v2/v3 示例和未知字段规则
没有定义它。

**推荐 A（严格 Reader）**：拒绝 record 级 `extensions`，保留 document 级 opaque extensions；补拒绝
测试和 CXC pack/validate 传播测试。这条路径不扩大格式合同，风险最低。

**备选 B（正式扩展）**：在 ADR 0026、Asset Index schema、MATERIAL_SHADER、Reader/Writer 和预算表
同时定义字段、canonical 排序、最大字节数、identity 参与规则和未知子字段规则；不得只“补一句文档”。

### D3：PB-04，frame.value_invalid

**问题**：生产 extract 路径只检查 finite；`[0,1]` 范围在上游 World/Chart 边界执行，Validation
Sink 仍会发出该码，规范与生产发射点看似不一致。

**推荐 A**：保留 code 供 Validation Sink/未来边界使用，在 PORTABLE_PRESENTATION 明确“frozen range
在上游边界拒绝；extract 层只发 non-finite 诊断”。方向 4 的 taxonomy 只校验 code 归属和发射说明。

**备选 B**：把范围检查下移到 production normalize；必须保证错误顺序、性能和 golden 一致，不建议本轮采用。

### D4：CH-03，TimingMap 两参/四参 BPM 域

**问题**：两参构造只要求 finite 且大于 0，四参构造执行 `[1, 65536]`，规范和公共头没有说明。

**推荐 A**：保持现有兼容行为，明确“legacy 两参路径允许任意正 finite BPM；带显式 limits 的路径
执行 `[1,65536]`”，并补测试。

**备选 B**：两参也执行严格域，更新 legacy 测试、错误码和兼容说明；这属于行为变化，必须 owner 接受。

### D5：RT-04，legacy RenderFrame

**问题**：全仓无生产调用，真实主路径是 `renderPresentationFrame`，但公共注释仍称旧接口为现行契约。

**推荐 A**：标记 `RenderFrame/renderFrame` 为 legacy/diagnostic-only，补迁移说明和未调用断言；暂不删除。

**备选 B**：Stage 6 确认安装包和 external consumer 无引用后删除声明、override、实现及测试，走 API 版本门禁。

### D6：identity 迁移

采用审查推荐：v1/v2/v3 PreparedSemanticIdentity 使用 canonical 全保真 Chart bytes；timing
stops、tempoEvents、templates、keyframe tracks 等影响运行表现的字段必须改变 identity。一次性
重生成受影响 golden 和 cache，不保留双轨猜测逻辑。

### 4.7 兼容退出政策（ADR 0041）

2026-08-30 已接受 [ADR 0041](../../../adr/0041-legacy-format-exit-policy.md)。后续整改统一遵循：

- 新版 Writer、pack、prepare 不产生旧格式或 legacy presentation。
- Reader/prepare 遇到旧 payload、未定义 record-level extension 或不兼容组合时稳定拒绝，
  不进行隐式 fallback 或猜测式转换。
- Chart v1/v2/v3 Reader、显式迁移、document-level opaque extensions 和 SDK 0.7.0 已公开
  的旧 API 继续按原 ADR 保留；删除或收紧必须进入新的 SDK/API 版本门禁。
- identity 采用一次性 canonical 全量迁移，旧 cache 失效，不保留双轨算法。

本节是已接受的实施合同；它不改变 `docs/CURRENT_STATUS.md` 的 Stage 5 状态，也不授权删除
Chart v1/v2/v3 Reader、`RenderFrame` 或两参 `TimingMap::create`。

## 5. 批次总览和依赖

 ~~~text
D1-D6 决策
   |
第 0 批：P0/P1 快赢和安全契约声明
   |
第 0.5 批：纯文档/注释修正
   |
Chart reader 内聚性前置：先修 CH-01，再做纯 internal 拆分（无独立 finding）
   |
第 1 批双槽：identity/cache | math | docs-CI | reload
   |
第 2 批双槽：render hot path | audio | resource index
   |
第 3 批双槽：prepare 分层 + canonical 内聚性分解 + parse-once
   |
触发条件批：world/animation | CXC/JSON | RT-29 | Stage 6 API/tooling
 ~~~

依赖原则：

- 第 0 批不依赖第 1 批，先关闭真实 P0/P1 和公共边界异常风险。
- Chart reader 内聚性前置必须在 CH-01 修复和第 0.5 批门禁之后执行；它不改变行为，但应先于
  第 3 批 parse-once 和 CH-08/CH-14 搭车项。
- 第 1 批 identity 线必须先于第 3 批 prepare 重构，否则 identity 计算点会被重写两次。
- 第 2 批 render 的 summary/digest 语义必须保持，直到第 3 批 taxonomy 也通过。
- world/animation 大规模优化须先用探针证明收益；CXC/JSON 降本须有真实大包或 owner 授权。

## 6. 多智能体调度表（最多两个子智能体并发）

本节是主智能体的执行手册，不改变后文任务的技术范围。后文所说“四条可并行主线”是指依赖
关系允许交错推进，不表示可以同时启动四个子智能体。实际执行采用一个主智能体加最多两个
子智能体的双槽位模型，并在每个波次结束时设置集成屏障。

### 6.1 硬性并发模型

- 主智能体记为 `M`，负责任务选择、决策门、文件锁、集成、提交、全量验证和关闭报告；`M`
  不计入“两个子智能体”上限。
- 子智能体只有 `W1`、`W2` 两个活动槽位。整个子智能体树中，处于 `DISPATCHED`、`RUNNING`、
  `WAITING_TOOL` 或等待主智能体答复状态的执行/审查智能体合计不得超过两个。
- 审查型、测试型和只读分析型子智能体同样占一个槽位。一个阻塞但尚未结束的子智能体仍占
  槽位；主智能体必须先取得交接并将其结束或标记 `BLOCKED`，才能补入下一项任务。
- 子智能体不得再创建孙级智能体。只有 `M` 可以创建、停止或替换子智能体，以避免并发数在
  子树中失控。
- 推荐使用“波次屏障”：同一波的两个槽位都完成交接后，`M` 审查 diff、处理冲突并顺序运行
  聚焦验证；未完成集成前不启动下一波。风险高或文件面大的任务允许只占一个槽位。
- `M` 在子智能体活动期间可以只读检查和准备下一张任务卡，但不得修改该子智能体持有锁内的
  文件。子智能体也不得修改未在任务卡中声明的文件。
- 共享工作树默认由 `M` 唯一执行 `git add`、`git commit`、`git rebase`、分支切换和整改报告
  更新。子智能体不得执行 `git reset`、`git checkout --`、清理未跟踪文件或回退他人改动。

### 6.2 任务状态和派发合同

每个原子任务只按以下状态流转；只有 `M` 可以把任务改为 `INTEGRATED`、`VERIFIED` 或
`CLOSED`。

| 状态 | 含义 | 是否占子智能体槽位 |
| --- | --- | --- |
| `READY` | 决策、依赖、文件锁和测试入口均已确认 | 否 |
| `DISPATCHED` | 已把完整任务卡交给 W1/W2，尚未收到首次确认 | 是 |
| `RUNNING` | 正在读代码、编辑或运行允许的聚焦测试 | 是 |
| `HANDOFF` | 已停止编辑并返回 diff、测试和风险，等待主智能体接收 | 是，接收后释放 |
| `BLOCKED` | 命中停止条件，未猜测语义且已返回证据 | 否，必须先结束该执行实例 |
| `INTEGRATED` | `M` 已审查并吸收修改，尚未通过完整门禁 | 否 |
| `VERIFIED` | 本任务及波次门禁已通过 | 否 |
| `CLOSED` | 整改报告、所需 hosted 证据和 owner acceptance 均齐备 | 否 |

每次派发必须同时给出以下字段，缺一项不得启动：

| 字段 | 必填内容 |
| --- | --- |
| 波次/槽位 | 例如 `W11/W1`，以及并行伙伴任务 ID 或 `none` |
| 任务边界 | 一个原子任务 ID、原始 finding、一个可观察结果 |
| 基线 | 当前 integration SHA、已有未提交改动清单、适用的决策门结果 |
| 写入集合 | 精确文件或目录，及下节中的独占锁；未列文件只读 |
| 禁止集合 | 原审查报告、CURRENT_STATUS、无关 public API、digest/golden 或共享构建目录等 |
| 实现顺序 | 先读哪些合同/测试、先写什么 characterization、再改哪个数据流 |
| 验收 | 聚焦命令、期望通过/预期 RED、必须保持不变的 identity/digest/排序 |
| 交接 | 文件清单、行为变化、测试结果、未决风险和建议下一任务 |
| 停止条件 | 需要 owner 决策、锁范围扩大、基线漂移、意外测试冲突时立即停止 |

原子任务默认只包含一个可观察结果、一个公共合同变化上限、1 至 3 个实现文件和必要的聚焦
测试。超过这个范围时，`M` 必须先拆成“characterization/测试”“最小实现”“迁移调用点”或
“集成门禁”，不得把大 Lane 原样交给一个较弱模型。

### 6.3 共享工作树文件锁

锁是任务卡的最低所有权边界，不代替精确文件列表。两个任务只有在锁集合和实际写入文件都
不相交时才能同波执行；只读访问不构成冲突。测试文件随其被测模块一起锁定，不能由两个
子智能体同时追加。

| 锁 ID | 默认覆盖 | 说明 |
| --- | --- | --- |
| `LK-PLAN` | 本计划、原审查报告、整改报告及报告索引、CURRENT_STATUS | 仅 `M` 可写；原审查报告始终只读 |
| `LK-DECISION` | ADR、格式语义、公共错误码、SDK API/version 决策记录 | 仅 `M` 在 owner 结论明确后写 |
| `LK-CHART` | `engine/chart/`、`tests/chart/`、Chart fixture/golden | CH-01、identity source、parse-once 和 Chart 搭车项共享 |
| `LK-PLAYBACK` | `engine/playback/`、`tests/playback/`、Playback identity/golden | PB-08、Lane A identity、PB-12、B3 prepare 互斥 |
| `LK-CORE` | `engine/core/`、`tests/core/` | math API、normalize、UUID、SHA 测试互斥 |
| `LK-BEH-ANIM` | `engine/behavior/`、`engine/animation/` 及对应测试 | 与 math 调用点迁移、T1 mixer 工作共用 |
| `LK-RUNTIME` | `engine/runtime/`、`tests/runtime/` | reload、math runtime 调用点和 runtime 搭车项互斥 |
| `LK-SHADER` | `engine/shader/`、`tests/shader/`、shader cache golden | cache key、compiler validation 和 shader 搭车项互斥 |
| `LK-RENDER` | `engine/render*/`、Player render 路径、presentation/render 测试 | pipeline cache、uniform、scratch、summary 和 GL helper 共用 |
| `LK-AUDIO` | `engine/audio*/`、`tests/audio*/` | HostClock 与 SDL replacement 工作互斥 |
| `LK-CXC-ASSET` | `engine/cxc/`、project/assets 相关实现和测试 | CXC peek、package context、portable path、resource scope 共用 |
| `LK-TOOLS` | `tools/`、工具测试、tool fixture | importer、tools_common、migrator staging 共用 |
| `LK-DOC-CI` | 普通文档、`tools/check_docs.py`、`docs/status_contract.json`、workflow | 可再按精确文件拆锁；同一 Markdown 文件不得双写 |
| `LK-CMAKE` | 根 `CMakeLists.txt`、`cmake/`、presets、vcpkg/build workflow | target 导出、依赖和 target 重命名必须串行 |
| `LK-BUILD-DEBUG` | `out/build/debug` 及其生成文件、CTest discovery/log | 任意时刻只允许一个执行者写或运行该构建树 |
| `LK-BUILD-RELEASE` | `out/build/release` 及其生成文件、CTest discovery/log | 由 `M` 在批次退出时独占 |

若平台为每个子智能体提供真正隔离的 worktree，`M` 可以把源码锁降为合并冲突检查，但
`LK-DECISION`、golden、报告和 hosted 关闭权仍不能下放。默认按本仓库的共享工作树模型执行。

### 6.4 主智能体专属任务

| 任务 ID | 时机 | 主智能体必须完成的结果 |
| --- | --- | --- |
| `M00-BASELINE` | 第一次派发前 | 记录当前 SHA、dirty files、审查基线、工具链可用性和 144 项映射完整性 |
| `M01-DECISIONS` | 相关任务进入 READY 前 | 分别冻结 D1-D6 的 owner 结论；未决定项保持 blocked，不把推荐项冒充批准 |
| `M02-LOCK-AUDIT` | 每一波派发前 | 展开两张任务卡的精确写入集合，确认无文件、golden、构建树和决策锁冲突 |
| `M03-WAVE-INTEGRATE` | 每一波两个槽位交接后 | 审查 diff、恢复遗漏格式、顺序运行聚焦测试、记录 task ledger |
| `M04-BATCH-GATE` | 每批最后一波后 | 独占构建目录运行该批全部门禁；失败时定位到任务并派发返工，不继续后批 |
| `M05-CLOSE` | 实现批完成后 | 同 SHA Release/hosted 验证、整改报告、索引和 owner acceptance；最后才更新状态页 |

`M00` 还必须记录派发前已经存在的用户改动。后续若出现任务卡外的新 diff，先判断是否是其他
槽位的合法产物；不能用回退命令“清干净”共享工作树。

### 6.5 原子任务目录

下面的任务 ID 是派发单位。技术细节仍以后文对应任务为准；表中的测试是最低聚焦门禁，完整
门禁由 `M04-BATCH-GATE` 统一执行。后缀 `-T` 是 characterization/失败测试任务，允许在交接时
保持预期 RED，但不得单独提交或标记 finding 已修复；对应 `-I` 通过后一起集成。

#### 第 0 批和第 0.5 批

| 任务 ID | 依赖 | 独占锁 | 单一交付结果 | 最低聚焦验收 |
| --- | --- | --- | --- | --- |
| `AG-B0-CH01-T` | `M00` | `LK-CHART` | 空/超长 mask property 的稳定失败测试 | `cuexis_chart_tests`，新用例预期 RED |
| `AG-B0-CH01-I` | 上一任务 | `LK-CHART` | 非法 property 有定位诊断且不形成 inert Animator | `cuexis_chart_tests` |
| `AG-B0-PB08-T` | `M00` | `LK-PLAYBACK` | 五个公共 owning-copy 方法的异常/状态 characterization | `cuexis_playback_tests`，新注入用例可 RED |
| `AG-B0-PB08-I` | 上一任务 | `LK-PLAYBACK` | 分配异常不越过公共边界且候选状态不泄漏 | `cuexis_playback_tests`、external consumer |
| `AG-B0-CM03` | 调用点审计完成 | `LK-AUDIO`、精确文档锁 | 冻结 HostClock 当前线程合同，不伪称已线程安全 | `cuexis_audio_tests`、ASCII/public-header 检查 |
| `AG-B0-DOC` | `M00` | 精确 `LK-DOC-CI` | AP-01/02、CM-01、AP-06、RT-02 文档事实修正 | `check_docs.py` |
| `AG-B0-AP19` | 无真实消费者 | `LK-CMAKE` | 删除死 export 变量且重新 configure 成功 | `cmake --preset debug --fresh` |
| `AG-B05-CH` | D4 | 精确 `LK-DOC-CI`、必要时 `LK-CHART` public header | CH-03/04/12 合同与实现一致 | docs check、Chart 聚焦测试 |
| `AG-B05-PB` | D3 | 精确 `LK-DOC-CI`、Playback public header | PB-04/05/06/10 的线程、generation、code 归属明确 | docs、ASCII、consumer |
| `AG-B05-RT` | D5 | 精确 `LK-DOC-CI`、Runtime/render public header | RT-05/06/08/25/33/34 注释和链接事实化 | docs、ASCII |
| `AG-B05-CM` | D4 已记录 | 精确 `LK-DOC-CI`、core/audio/platform header | CM 合同说明完整且不改变行为 | docs、ASCII、相关 unit tests |
| `AG-B05-CX` | D2 仅记录，不实现 | 精确 `LK-DOC-CI` | CX-03/04/06/16/26 说明与现状一致 | `check_docs.py` |
| `AG-B05-AP` | `AG-B0-DOC` | 精确 `LK-DOC-CI` | AP-03/04/05 示例和 unpack 边界正确 | docs check、工具聚焦测试 |

#### Chart 内聚性前置任务（无独立 finding）

这两项不增加第 4 节审查 finding 数量，也不能单独关闭 CH-14。它们用于消除
`chart_v4_reader_internal.cpp` 中已经证实的职责混杂，并降低后续 CH-08、CH-14 和 parse-once
修改同一文件的冲突风险。

| 任务 ID | 依赖 | 独占锁 | 单一交付结果 | 最低聚焦验收 |
| --- | --- | --- | --- | --- |
| `AG-CH-COHESION-T` | `AG-B0-CH01-I`、第 0.5 批 gate | `LK-CHART` | 为 V4 common reader、animation reader 和 project path 建立行为/诊断 parity 基线 | chart v4/CXT/path 测试全部先通过 |
| `AG-CH-COHESION-I` | 上一任务 | `LK-CHART`、精确锁定 `engine/chart/CMakeLists.txt` | 把三类职责移入独立 internal 文件，公共 API 和行为零变化 | chart tests、format、architecture、identity parity |

#### 第 1 批

| 任务 ID | 依赖 | 独占锁 | 单一交付结果 | 最低聚焦验收 |
| --- | --- | --- | --- | --- |
| `AG-A1-IDENTITY-T` | D6、第 0 批 gate | `LK-PLAYBACK`、`LK-CHART` fixture 只在明确授权时写 | tempo/stop/template/keyframe 差异测试能暴露旧 identity 投影 | playback identity 测试预期 RED |
| `AG-A1-IDENTITY-I` | 上一任务 | `LK-PLAYBACK` | v1-v3 identity 使用 canonical 全保真 Chart bytes | playback、CFU-F3 determinism |
| `AG-A2-IMPORTER` | `AG-A1-IDENTITY-I` | `LK-TOOLS` | cache-dir 必须获得合法 semantic identity，默认不再用 `main` | importer CLI tests |
| `AG-A2-CACHE-KEY` | `AG-A1-IDENTITY-I` | `LK-SHADER` | cache key 输入一次规范化，半空组合稳定拒绝 | shader cache tests |
| `AG-A3-PIPELINE-T` | A2 合同可读 | `LK-RENDER` | cache miss/compile fail/commit rollback/activate 顺序 characterization | render/Player tests，可 RED |
| `AG-A3-PIPELINE-I` | A2 两项和上一任务 | `LK-RENDER`、必要时 `LK-SHADER` 只读 | Player 在 prepare 事务中消费 ShaderPipelineCache | render、shader-tools、Playback-only consumer |
| `AG-B1-MATH-T` | 第 0 批 gate | `LK-CORE`、测试所需的精确调用点锁 | slerp/hermite/normalize/ZYX 的现状和目标测试 | core/behavior/animation tests，可 RED |
| `AG-B2-MATH-API` | 上一任务 | `LK-CORE` | Cuexis 自有类型的统一 math API，不泄漏 GLM | core tests、public-header check |
| `AG-B3-MATH-MIGRATE` | 上一任务 | `LK-BEH-ANIM`、`LK-RUNTIME` | behavior/animation/runtime 删除重复插值并保持错误包装 | behavior/animation/runtime parity |
| `AG-B4-NORMALIZE` | `AG-B2-MATH-API` | `LK-CORE` | 大数 quaternion 非有限长度稳定失败，不返回零 quaternion | core math tests |
| `AG-B5-CORE-AUX` | UUID 策略由 `M` 写入任务卡 | `LK-CORE` | CM-06/09/05 的 UUID、SHA、epsilon 测试/声明闭合 | core tests |
| `AG-C1-STATUS-CONTRACT` | 第 0.5 批 gate | `LK-DOC-CI` | 单点 JSON 状态合同和明确 checker 错误 | docs checker positive/negative tests |
| `AG-C2-TARGET-EXPORT` | 上一任务 | `LK-CMAKE`、`LK-DOC-CI` | configure 生成 active target 事实并校验 BUILDING 标记块 | fresh configure、docs check |
| `AG-C3-LINUX-CI` | 上一任务 | workflow 精确文件锁 | Linux Quality 在构建前运行 docs/version 检查 | workflow 静态校验、docs check |
| `AG-D1-RELOAD-T` | 第 0 批 gate | `LK-RUNTIME` | debug sample 失败污染 active 状态的回归测试 | runtime tests，可 RED |
| `AG-D2-RELOAD-I` | 上一任务 | `LK-RUNTIME` | reload candidate 在所有可失败步骤后单点提交 | debug/release runtime tests |

#### 第 2 批

| 任务 ID | 依赖 | 独占锁 | 单一交付结果 | 最低聚焦验收 |
| --- | --- | --- | --- | --- |
| `AG-R1-BOUNDS-T` | 第 1 批 gate | `LK-RENDER` | 资源缺失/非有限/正常 bounds 和 lookup 次数 characterization | render/presentation tests |
| `AG-R1-BOUNDS-I` | 上一任务 | `LK-RENDER` | bounds 在 prepare 缓存，帧内不再线性查找 | render tests、GPU smoke |
| `AG-R2-UNIFORM-T` | 第 1 批 gate | `LK-RENDER` | location 查询次数和可选 uniform 语义测试 | render tests，可含 mock |
| `AG-R2-UNIFORM-I` | 上一任务 | `LK-RENDER` | link 后缓存 location，热帧不再按名查询 | render tests、GPU smoke |
| `AG-R3-SCRATCH-T` | 第 1 批 gate | `LK-RENDER` | summary on/off、scratch 和 Player scene allocation 基线 | player/render allocation probe |
| `AG-R3-SCRATCH-I` | 上一任务 | `LK-RENDER` | scratch/scene 复用且 summary/digest 非空路径不变 | render parity、GPU smoke |
| `AG-AUD1-CLOCK-T` | B0 HostClock 合同 | `LK-AUDIO` | 跨线程 snapshot 自洽压力测试 | audio tests，可 RED |
| `AG-AUD1-CLOCK-I` | 上一任务 | `LK-AUDIO` | seqlock 发布完整 HostClock sample 且 snapshot 有界 | audio stress tests |
| `AG-AUD2-SDL-T` | `AG-AUD1-CLOCK-I` 可独立前置审计 | `LK-AUDIO` | effective/replacement/error/unload 状态矩阵 | audio_sdl tests，可 RED |
| `AG-AUD2-SDL-I` | 上一任务 | `LK-AUDIO` | effective snapshot 同步且 replacement 失败合同一致 | audio/audio_sdl tests |
| `AG-PB12-T` | 第 1 批 gate | `LK-PLAYBACK` | 1/2/65536 资源、重复键和 identity/order 测试 | playback/presentation tests |
| `AG-PB12-I` | 上一任务 | `LK-PLAYBACK` | transparent lookup 或 canonical flat index 消除 O(n²) | playback tests、determinism |

#### 第 3 批

| 任务 ID | 依赖 | 独占锁 | 单一交付结果 | 最低聚焦验收 |
| --- | --- | --- | --- | --- |
| `AG-PREP-T` | Lane A identity、第 2 批 gate | `LK-PLAYBACK` | PB-03 全失败矩阵先锁定 diagnostics 和 active rollback | playback tests，可 RED |
| `AG-PREP-CONTEXT` | 上一任务 | `LK-PLAYBACK` | 引入 internal PrepareContext/Artifact，不改变成功行为 | playback tests、architecture |
| `AG-PREP-STAGE1` | 上一任务 | `LK-PLAYBACK` | load 到 compileRuntime 的前半 stages 有明确 Result/所有权 | playback/chart tests |
| `AG-PREP-STAGE2` | 上一任务 | `LK-PLAYBACK` | acquire 到 assembleIdentity 的后半 stages 与单点 commit | playback/audio/presentation tests |
| `AG-PREP-DIAG` | 上一任务 | `LK-PLAYBACK` | RAII recorder 覆盖成功、全部早退和异常 | PB-03 matrix |
| `AG-PREP-TAXONOMY` | D3、`AG-PREP-DIAG` | `LK-PLAYBACK`，精确 helper 锁 | capability/limit/code 表驱动且发射归属明确 | playback validation、spec checker |
| `AG-PARSE-T` | Lane A identity、第 2 批 gate、`AG-CH-COHESION-I` | `LK-CHART`、精确 `LK-CXC-ASSET` fixture | parse count、code/path/order 和 canonical parity 基线 | chart/cxc tests |
| `AG-PARSE-CHART` | 上一任务 | `LK-CHART` | loader/resolver/writer 共享一个 internal parsed value | chart tests、identity golden |
| `AG-PARSE-CXC` | 上一任务 | `LK-CXC-ASSET`、`LK-CHART` 只读 | ChartLoader/CXC peek 不重复完整解析 | cxc/chart tests |
| `AG-PARSE-OWNERSHIP` | 前两项、PREP stages 已稳定 | 由 `M02` 精确分配，必要时单槽 | 生命周期、16 MiB/多 CXT 峰值和 determinism 证据闭合 | CFU-F1/F3/F4、S4、S5-H |

`canonical_chart_loader.cpp` 的进一步拆分不作为一张可直接派发的大任务。`AG-PARSE-T` 完成后，
`M` 必须先按 template/patch、behavior、component/object、timing/root orchestration 画出所有权图，
再生成 1 至 3 个实现文件规模的 `AG-CH-CANONICAL-I<n>` 任务。每项只允许纯 internal 提取；若
无法保持 diagnostics、canonical bytes 和 typed document 完全一致，则停止拆分并先完成
parse-once，不以“文件变短”为理由扩大行为改动。

### 6.6 推荐双槽位波次

下表是默认调度，不是绕过依赖的许可。`M02-LOCK-AUDIT` 若发现精确文件重叠，必须把 W2 延后，
不能为了填满两个槽位而扩大文件所有权。`-T` 与对应 `-I` 共同形成一个可提交单元。

| 波次 | W1 | W2 | 波次结束后的主智能体动作 |
| --- | --- | --- | --- |
| `W00` | 无 | 无 | `M00`、`M01`；建立 ledger，关闭可决定的 D1-D6 |
| `W01` | `AG-B0-CH01-T` | `AG-B0-PB08-T` | 审查预期 RED 只命中目标问题，不运行全量 gate |
| `W02` | `AG-B0-CH01-I` | `AG-B0-PB08-I` | 运行 chart/playback 聚焦测试和边界检查 |
| `W03` | `AG-B0-CM03` | `AG-B0-DOC` | docs/ASCII/audio 聚焦检查 |
| `W04` | `AG-B0-AP19` | 只读第 0 批审查或空闲 | `M04` 执行第 0 批完整退出门禁 |
| `W05` | `AG-B05-CH` | `AG-B05-CM` | 按精确文件审查 public header ASCII |
| `W06` | `AG-B05-PB` | `AG-B05-CX` | 确认 D2/D3 没有被子智能体自行选择 |
| `W07` | `AG-B05-RT` | `AG-B05-AP` | `M04` 执行第 0.5 批 docs/contract gate |
| `W08` | `AG-CH-COHESION-T` | 只读 Chart 职责边界审查或空闲 | 基线必须为 GREEN；记录 diagnostics/path/identity 指纹 |
| `W09` | `AG-CH-COHESION-I` | 非 Chart 只读审查或空闲 | 审查纯移动 diff，运行 Chart/full-format 门禁后再进第 1 批 |
| `W10` | `AG-A1-IDENTITY-T` | `AG-B1-MATH-T` | 记录两组 characterization；不提交 RED |
| `W11` | `AG-A1-IDENTITY-I` | `AG-B2-MATH-API` | 比较 identity/canonical bytes 与 public-header 边界 |
| `W12` | `AG-A2-IMPORTER` | `AG-B4-NORMALIZE` | importer/core 聚焦测试；确认写锁不含共享 CMake |
| `W13` | `AG-A2-CACHE-KEY` | `AG-B3-MATH-MIGRATE` | shader key 与 behavior/animation/runtime parity |
| `W14` | `AG-A3-PIPELINE-T` | `AG-C1-STATUS-CONTRACT` | render RED 范围和 docs checker 负例审查 |
| `W15` | `AG-A3-PIPELINE-I` | `AG-D1-RELOAD-T` | pipeline 事务审查；reload 测试仅触及 Runtime 锁 |
| `W16` | `AG-C2-TARGET-EXPORT` | `AG-D2-RELOAD-I` | fresh configure 与 runtime tests 顺序运行 |
| `W17` | `AG-C3-LINUX-CI` | `AG-B5-CORE-AUX` | workflow/docs/core 聚焦检查 |
| `W18` | Lane A/D 只读审查 | Lane B/C 只读审查 | 释放两槽后由 `M04` 独占 Debug/full gate |
| `W20` | `AG-R1-BOUNDS-T` | `AG-AUD1-CLOCK-T` | 审查 probe 是否测量目标热点/并发不变量 |
| `W21` | `AG-R1-BOUNDS-I` | `AG-AUD1-CLOCK-I` | render/audio 聚焦测试顺序运行 |
| `W22` | `AG-R2-UNIFORM-T` | `AG-AUD2-SDL-T` | 两组 RED 必须各自只命中目标合同 |
| `W23` | `AG-R2-UNIFORM-I` | `AG-AUD2-SDL-I` | GPU mock/smoke 与 audio 状态矩阵 |
| `W24` | `AG-R3-SCRATCH-T` | `AG-PB12-T` | 若 presentation 测试文件重叠，PB12 延后为单槽 |
| `W25` | `AG-R3-SCRATCH-I` | `AG-PB12-I` | 同上执行锁审计；比较 summary/digest/order |
| `W26` | render/PB 只读审查 | audio 线程安全只读审查 | `M04` 独占 Debug/full gate 和必要 GPU smoke |
| `W30` | `AG-PREP-T` | `AG-PARSE-T` | 确认测试文件不重叠；建立 B3 行为基线 |
| `W30C<n>` | `AG-CH-CANONICAL-I<n>` | 非 Chart 只读审查或空闲 | 每次只提取一个功能区并跑 canonical parity；全部完成后才进 W31 |
| `W31` | `AG-PREP-CONTEXT` | `AG-PARSE-CHART` | architecture、ownership 和 canonical parity |
| `W32` | `AG-PREP-STAGE1` | `AG-PARSE-CXC` | 确认 CXC 任务不写 Playback source/tests |
| `W33` | `AG-PREP-STAGE2` | 一个经下节批准的 Chart 搭车任务或空闲 | 高风险提交阶段优先单槽，运行 transaction tests |
| `W34` | `AG-PREP-DIAG` | 一个经批准的 CXC/assets 搭车任务或空闲 | PB-03 matrix 与 CXC 聚焦测试顺序运行 |
| `W35` | `AG-PREP-TAXONOMY` | 一个经批准的 Runtime/工具搭车任务或空闲 | code ownership、spec 表和 diagnostics fingerprint |
| `W36` | `AG-PARSE-OWNERSHIP` | 只读 determinism 审查或空闲 | 峰值证据与 CFU/S4/S5-H golden 比较 |
| `W37` | B3 spec/contract 只读审查 | B3 tests/architecture 只读审查 | `M04` 独占 Debug、Release 和 hosted 候选门禁 |

同一波内某个子智能体先完成时，推荐保持槽位空闲直至波次屏障。只有当其交接已被 `M` 接收、
新任务与仍运行任务的锁完全不相交、且不会跨越尚未验证的依赖时，才可提前补位。

### 6.7 搭车项和触发项的派发规则

搭车项不能作为“顺手再做几个 finding”的开放许可。`M` 必须先检查父任务是否已经触及同一
数据结构；若增加超过 3 个实现文件、第二个公共合同或新的锁域，就拆为独立 `AG-CAR-*` 任务，
排入 W33-W35 的空槽或后续新波次。

| 搭车任务包 | 推荐父任务/时机 | 必须拆开的条件 |
| --- | --- | --- |
| `AG-CAR-CH07-08` | `AG-PARSE-CHART` 之后 | mask cache 与 parsed-value 传递不能在同一小 diff 中证明时拆成两个任务 |
| `AG-CAR-CH09-11` | `AG-PREP-TAXONOMY` code 表冻结后 | 需要新增/重命名公共 code 或多个 diagnostics golden 时独立串行 |
| `AG-CAR-CH13-16` | CH01/parse reader 已稳定后 | source index、reader helper、早退修改涉及不同文件时再按 finding 分组 |
| `AG-CAR-CH-CANONICAL` | `AG-PARSE-T` 后、`AG-PARSE-CHART` 前 | 必须由 `M` 按 template、behavior、component/object、timing/root 划分为多个 `I<n>`；禁止整文件单任务 |
| `AG-CAR-PB02-07` | `AG-PREP-STAGE1` 附近 | source/provider 继承与 CXT cause 不能由同一测试矩阵覆盖时拆开 |
| `AG-CAR-PB09-11-13` | `AG-PREP-CONTEXT` 后 | noexcept 初始化、死字段删除、AssetDatabase 生命周期分别持锁时拆开 |
| `AG-CAR-PB15` | `AG-PREP-TAXONOMY` | helper 合并改变错误顺序或跨 presentation 文件时独立派发 |
| `AG-CAR-X07-11` | `AG-PARSE-CXC` 后 | package BuildContext、OOM、owning text 任一需要结构改动时单独派发 |
| `AG-CAR-X13-21` | 触及对应 assets/project 文件时 | portable path、ResourceScope、provider category 必须分别有 characterization |
| `AG-CAR-X22-27` | Lane A shader 集成稳定后 | store 返回值、UTF-8、compiler limits 分属不同源文件时拆开 |
| `AG-CAR-RW1-3` | Lane B/D 或第 2 批 render 后 | 涉及 Runtime、World、OpenGL 两个以上锁域时禁止同任务 |
| `AG-CAR-AP` | importer/docs/render/CMake 父任务 | tools_common、triplet、target rename、player support 各自需要独立 CMake gate |

T1-T4 默认不进入 READY。`M` 只有在后文触发器有实际证据和 owner 授权后，才先派发一个只读/
probe 子智能体形成基线，再派发设计或实现任务；probe 和实现不得同波。未触发项目保持 deferred，
不能为了清空 finding 表而占用两个槽位。

### 6.8 冲突和并行判定

| 组合 | 判定 | 原因/要求 |
| --- | --- | --- |
| 任意两个任务持有相同 `LK-*` | 禁止 | 共享工作树会产生覆盖、混合 diff 或错误归因 |
| 两个任务都要 configure/build/ctest 同一 preset | 禁止 | `out/build/*`、CTest discovery 和日志是共享写资源 |
| Chart reader 与 Playback prepare | 条件允许 | 只有精确文件和 fixture 完全分离，且 prepare 不依赖尚未集成的 parse 结果 |
| core math API 与 behavior/animation 迁移 | 禁止同波 | 调用点任务必须基于已集成 API |
| shader cache key 与 importer CLI | 允许 | identity 合同已集成、无共享 helper/CMake 文件时可并行 |
| render/OpenGL 与 audio | 通常允许 | 锁域分离；最终构建和 smoke 仍由 `M` 串行 |
| docs/status checker 与 root CMake target 导出 | 禁止同波 | C2 同时依赖并修改两边，必须在 C1 后执行 |
| 代码任务与只读审查 | 仅在审查不针对活动 diff 时允许 | 审查活动文件会看到中间态，结论无效 |
| 任何任务与 owner/spec 决策写入 | 禁止 | 决策必须先由 `M` 冻结，再派发实现 |

### 6.9 子智能体交接和复审

子智能体完成时必须停止编辑并返回以下固定格式；缺少文件或测试证据时，`M` 将任务退回
`READY`/`BLOCKED`，不能直接集成：

~~~text
task / wave / slot
读取的权威合同和采用的已批准决策
实际修改文件（逐项）
可观察行为变化和明确保持不变的合同
运行的命令、退出码、失败测试首个错误
identity/digest/canonical bytes/golden 是否变化及原因
任务卡外发现但未修改的问题
未决风险、停止条件或下一任务建议
~~~

`M` 接收交接后依次执行：检查 `git diff --name-only` 是否超出写入集合；阅读完整 diff；确认没有
修改原审查报告或提前更新 CURRENT_STATUS；运行格式/docs/聚焦测试；再把任务记为
`INTEGRATED`。同波两个任务的测试使用同一构建目录时必须顺序执行。完整 Debug/Release/hosted
门禁仍只由 `M04/M05` 宣称。

推荐在每个主要 Lane 后使用两个只读审查槽：一个核对 spec/API/事务和错误码，另一个核对测试、
确定性、架构 allowlist 和漏改文件。审查子智能体不得直接修复；发现问题后返回带文件/行号的
返工任务，由 `M` 再生成新的原子任务卡。

### 6.10 可直接派发的提示词外壳

执行型子智能体使用下列外壳，并在末尾附上后文对应任务的完整技术内容：

~~~text
你是 Cuexis 整改波次 <WAVE-ID> 的子智能体 <W1|W2>。
整个任务最多同时有两个子智能体；你不得创建任何子智能体。
并行伙伴是 <PARTNER-TASK|none>，不要读取其未完成修改作为已冻结合同。

执行原子任务 <TASK-ID>，原始 finding 为 <FINDINGS>。
基线 SHA：<SHA>；已批准决策：<DECISIONS>；前置任务：<DEPENDENCIES>。
你独占的锁：<LOCKS>。
只允许写入：<EXACT FILES>；其余文件只读。
不要 commit、reset、checkout、rebase、清理工作树，也不要修改原审查报告、CURRENT_STATUS、
整改计划或批次关闭报告。

目标可观察行为：<ONE OUTCOME>。
实现顺序：<STEPS>。
必须保持：<API/IDENTITY/DIGEST/ORDER/ROLLBACK CONTRACTS>。
聚焦验证：<COMMANDS AND EXPECTED RESULT>。

遇到未批准的语义选择、需要扩大文件/锁范围、基线漂移、公共 API/格式/错误码变化、测试与
合同冲突或共享构建目录正被使用时，立即停止并交接，不要猜测或越界修改。
最后严格按本计划 6.9 的格式返回。
~~~

只读审查子智能体使用：

~~~text
你是 Cuexis 整改波次 <WAVE-ID> 的只读审查子智能体，占用一个并发槽位且不得创建子智能体。
审查任务 <TASKS> 的已完成 diff，不修改任何文件，不运行会写共享构建目录的命令。
按 <SPEC/API/TRANSACTION> 或 <TEST/DETERMINISM/ARCHITECTURE> 这一条审查轴检查：
1. 是否满足任务卡和原始 finding；
2. 是否越过已批准决策或改动公共观察面；
3. 是否缺少失败、rollback、边界或 determinism 测试；
4. 是否修改了任务锁以外文件。
先列按严重度排序的发现并给出文件/行号；无发现时明确写“未发现阻塞问题”，再列残余测试风险。
~~~

### 6.11 主智能体运行账本

`M` 应在自己的任务上下文中维护下表；它是运行记录模板，不是完成报告。任何时刻最多有两行
状态处于 `DISPATCHED/RUNNING/HANDOFF`。

| 波次 | 槽位 | 任务 | 基线 SHA | 写锁/精确文件 | 状态 | 交接与测试证据 | 下一动作 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `<Wxx>` | `W1` | `<TASK>` | `<SHA>` | `<LOCKS/FILES>` | `<STATE>` | `<SUMMARY>` | `<ACTION>` |
| `<Wxx>` | `W2` | `<TASK>` | `<SHA>` | `<LOCKS/FILES>` | `<STATE>` | `<SUMMARY>` | `<ACTION>` |

若任务命中停止条件，账本必须写清阻塞项和释放槽位的时间点。不得把 `HANDOFF` 当作
`VERIFIED`，也不得在两个子智能体仍运行时启动第三个“临时审查”或“快速测试”子智能体。

## 7. 第 0 批：正确性和公共边界快赢

目标是半天到一天内关闭一个 P0、三个直接契约风险和明显文档错误。不得借此批次做大型重构。

### B0-CH01：补齐非法 mask properties 诊断（CH-01，P0）

**问题和影响**：`readPropertyMask` 对空字符串或超长 property 只设置 `valid=false` 后继续，
没有调用 `addV4Error`；返回空值后 Layer/Instance 消费点静默跳过。唯一动画层因此可能悄悄
消失，capability 推导也看不到动画，宿主只能得到 inert 结果。

**允许文件**：

- `engine/chart/src/chart_v4_reader_internal.cpp`
- `tests/chart/` 中现有 v4 reader/animation 测试文件，必要时 `tests/playback/`

**实现步骤**：

1. 找到 property 条目的空串和长度预算分支，与同函数 prefix 分支的诊断写法对齐。
2. 对每个非法条目发出稳定 `chart.animation.mask_conflict`（或经决策门冻结的专用
   `*_limit` 码），包含 source path、条目索引和原因；不能只在循环结束补一条无位置错误。
3. 保持“该 mask 无效则整个 Layer/Instance 不进入结果”的拒绝语义，不能改成部分接受。
4. 审计 reader 内其他 `valid=false; continue`，只修漏报，不改变合法项顺序。
5. 确认 resolver capability 在错误输入上不会把动画误判为不存在。

**测试**：

- 空 property、长度超过上限 property 各一例，断言 load/resolve/prepare 失败或产生明确 diagnostics，
  且包含稳定 code 和 keyPath。
- 唯一动画层的 malformed mask 不得成功形成 inert Animator。
- 合法 mask、prefix、重复冲突和预算边界测试全部通过。

**完成标准**：没有静默跳过分支；diagnostics 排序稳定；合法 fixture 的 identity/digest 不变。

### B0-PB08：封闭五个公共方法的分配异常（PB-08，P1）

**问题和影响**：`capabilities()`、`contentInfo()`、`diagnostics()`、
`lastOperationDiagnostics()` 和 `acquireHostOverride()` 内的 vector/string 分配可让
`bad_alloc`/`length_error` 穿过 Playback 公共边界。

**实现步骤**：

1. 参照现有 `presentationManifest()`、`validatePresentation()` 和 `prepare()` 的
   异常处理范式，保留 owner-thread/reentry 检查顺序。
2. 在会产生 owning copy 的函数外围捕获分配异常，转换为既有稳定 allocation error；不要新造同义 code。
3. `acquireHostOverride` 必须覆盖 reserve、push_back、ownerId 拷贝和 value 映射；失败时不能
   注册半个 token。
4. 不要把 catch-all 吞掉 owner/reentry 逻辑，也不要给会分配的公共函数加 `noexcept`。
5. 成功路径仍返回独立副本，不能改成悬空引用。

**测试**：使用现有 fault-injection/分配测试设施（若缺失则至少覆盖边界和转换辅助函数），验证
五个方法不抛异常；override 分配/映射失败后 active frame、override 数量和下一帧结果不变；
static/shared external consumer 继续编译。

### B0-CM03：先冻结 HostClock 线程契约（CM-03，P1）

本批先在 `engine/audio/include/cuexis/audio/audio_transport.hpp` 明确 `HostClock::submit` 的
owner-thread 约束和当前 `snapshot` 读取限制，并在 SDK 文档和测试中说明。完整跨线程安全实现
放到 B2-CM21；不要用注释掩盖已存在的跨线程调用，先列出调用点并纠正或暂停任务。

### B0-DOC：事实和低风险文档项

- `AP-01/AP-02`：`docs/guides/BUILDING.md` 示例改为 SDK `0.7`，拒绝版本改为
  `0.6/0.8`，清除旧自相矛盾表述。
- `CM-01/AP-06`：`AGENTS.md` 改为 animation 是 active Stage 4 模块、particles 是
  INTERFACE stub；确认为空后删除 `tests/particles/`。
- `AP-19`：确认无消费者后删除死变量 `CUEXIS_PLAYBACK_EXPORT_TARGETS`，重新 configure。
- `RT-02` 文档侧：把“不分配”限定为无 Animation 会话，代码问题留到 T1。

**第 0 批退出门禁**：

 ~~~powershell
cmake --preset debug --fresh
cmake --build --preset debug --clean-first
ctest --preset debug --no-tests=error -R "cuexis_chart_tests|cuexis_playback_tests|cuexis_audio_tests"
python -B tools/check_docs.py
git diff --check
 ~~~

## 8. 第 0.5 批：纯文档、注释和契约声明

此批不改运行时行为、不改序列化字节、不改错误码含义。公共头新增注释必须纯 ASCII 英文。

### B05-CH：Chart 规则

- `CH-03`：按 D4 写清两参/四参 BPM 域；选择统一域时转入行为变更任务。
- `CH-04`：在 `docs/formats/CHART_V4_FORMAT.md` clip/segment 约束和边界表加入
  `segment.endBeat <= clip.durationBeats` 及拒绝 code。
- `CH-12`：修正 §1.1 处理顺序，使参数解析/冻结、模板 lowering、concrete 形成与实现一致。

### B05-PB：Playback 和 presentation

- `PB-05`：说明 `update()` 递增 generation，所有未提交 candidate 随后 stale。
- `PB-06`：ADR 0030 增加 2026-08-29 快照和 SDK 0.7 示例，保留历史背景。
- `PB-10`：在 playback public headers 写 owner-thread、跨线程析构/移动 terminate 契约。
- `PB-04`：按 D3 写清 frame.value_invalid 的 boundary-only 归属，不下移范围校验。

### B05-RT：Runtime/render

- `RT-05`：Runtime debug snapshot 链接 ANIMATION_MIXING 并列出 Stage 4 字段类别。
- `RT-06`：声明 chart_world_instantiator 的 bad_alloc OOM 传播边界。
- `RT-08`：说明 OpenGL configuration state 的主线程/上下文线程契约。
- `RT-25`：旧 renderFrame 描述标为 legacy/diagnostic-only，链接真实 presentation 路径。
- `RT-33`：写明 activate/discard 违约会 terminate 及 owner-thread 顺序。
- `RT-34`：注明 depthQuantization 等常量来源和改变后的排序影响。

### B05-CM：Core、audio、platform

- `CM-02`：CURRENT_STATUS 增加 audio/clock/timeline 摘要并链接权威文档。
- `CM-05`：说明 inverse 的绝对 determinant epsilon 与 nearlyEqual 的绝对容差。
- `CM-08`：说明 transformPoint affine-only、不会做透视除法。
- `CM-10`：说明 bounded Diagnostics append 的 sentinel 搬运语义。
- `CM-13/CM-14`：说明 SDL 全局状态和 Release 也会执行 terminate。
- `CM-20`：说明 Ended→Stopped 归零前必须 bump discontinuityId。
- `CM-24`：说明 relaxed counter 清零只影响呈现估计精度。
- `CM-27`：说明 MixValue::resourceView 借用 chart-owned string 的生命周期。

### B05-CX/AP：资源、工具、构建

- `CX-03`：MATERIAL_SHADER §11 列出真实 `cuexis_shader_cache` 拓扑。
- `CX-04`：CXC_FORMAT §9 解释 static archive/`InternalCxc` 链接闭包例外。
- `CX-06`：注明 Reader versionNeeded 0 到 20 的兼容上限，writer 固定写 10。
- `CX-16`：说明 saveAtomic 双 serialize/parse 是正确性校验，不得删除。
- `CX-26`：说明固定 `.tmp` 名和残留边界。
- `AP-03/AP-04`：BUILDING 补 cxc 工具、asset_importer、animation、cxc targets。
- `AP-05`：unpack 改为写入空的新/既有目录，不覆盖已有内容。

**第 0.5 批退出门禁**：

 ~~~powershell
python -B tools/check_docs.py
git diff --check
 ~~~

另外扫描新增/修改的 installed public header，确认没有非 ASCII 注释。

## 9. 第 1 批：四条逻辑上可并行主线

第 1 批是本轮最重要的基础批。四条线分别提交，完成后再合并。每条线都必须保持现有架构
allowlist 和 Playback-only consumer 边界。实际执行必须遵守第 6 节双槽位波次，任何时刻最多
两个子智能体工作。

### 9.1 Lane A：统一 identity 和 shader cache 键

**覆盖**：`CH-02`、`CX-02`、`CX-05`、`CX-23`。

#### A1：冻结 identity 语义

1. 采用 D6：v1/v2/v3 的 PreparedSemanticIdentity 使用 canonical 全保真 Chart bytes，不使用
   只写空 `timing.stops`/`tempoEvents`/`templates` 的投影 Writer。
2. 先写测试构造器：只改变 tempo event、stop、template 或 v1/v2/v3 keyframe tracks 之一，
   其余字段完全相同；断言 canonical bytes 和 PreparedSemanticIdentity 均变化。
3. 对照 migrator 的 `writeCanonicalJson`，集中命名 source semantic identity，避免
   投影 Writer 被误用。
4. 修改 `engine/playback/src/playback_session.cpp` identity 计算点后，一次性更新
   identity golden、CFU-F3 证据和受影响报告；不保留旧/新双轨猜测。

#### A2：消除 importer 与 Player 的键域分叉

推荐路径是“写 cache 时必须有 Playback/CXPRES semantic identity”：

1. `cuexis_asset_importer --compile` 无 `--cache-dir` 时仍可只编译并输出 artifact。
2. 带 `--cache-dir` 时要求 `--identity HEX64`，除非显式提供仅供开发调试的
   `--standalone-cache` 标志；默认路径不得再用硬编码 `"main"` 的源码哈希。
3. 若产品需要从 CXPRES payload 自动取 identity，另增读取 payload 入口并复用 Playback canonical
   parser；不得复制不完整 identity 算法。
4. `ShaderCacheKeyInput` 的 sourceIdentity、profiles、keywords、entry 和 tool versions
   必须由同一个 normalized input 生成。空 key 与非空 request keywords 的半空组合要么在构造时
   派生，要么立即返回 `shader.cache.key_invalid`。
5. hex 输入可接受大小写，但内部始终转为 32-byte binary；cache file name 继续使用 canonical
   SHA-256 hex。

#### A3：让 Player 消费 ShaderPipelineCache

1. OpenGL adapter prepare 阶段根据 manifest 的 shader identity/profile/keyword/entry/cache directory
   构造 `ShaderCacheKeyInput`。
2. `compileEnabled=false`：只读 cache，缺失返回 `shader.cache.missing`，绝不调用 shaderc。
3. `compileEnabled=true`：在既有 Worker/prepare 阶段形成 candidate；失败返回稳定 compile
   error 或 `shader.hot_reload.failed`，active program 保持不变。
4. 保持事务顺序：Playback candidate → presentation/resource acquire → adapter candidate →
   Playback commit → adapter `activate()`。Playback commit 失败要 discard adapter candidate。
5. `activate()` 只能在 owner/render safe point 做 noexcept swap；不能在 update、extractFrame
   或音频回调里编译、开文件或改变 cache map。
6. 保留 Unlit 内联 GLSL 330 和 FrameDigest v3；Parameterized 只在合法 cache/编译 artifact 后切换。

#### A4：Lane A 测试

- `tests/playback/playback_identity_tests.cpp`：timing/template/keyframe 差异 identity 不同。
- `tests/shader/shader_cache_tests.cpp`：半空输入拒绝；semantic identity 命中；工具/profile/
  keyword 变化只使对应键失效。
- importer CLI：无 identity + cache-dir 返回 usage/content error，不写文件；显式 identity 产生
  Player 可加载 cache。
- OpenGL/Player：失败候选保留 active；成功后只在 commit 后激活；Playback-only consumer 不链接
  shaderc/OpenGL。
- 删除 cache 后重建的 artifact 和 key bytes 确定性相同；既有 Unlit golden 不得无理由变化。

### 9.2 Lane B：统一 core::math 插值和数值边界

**覆盖**：`CM-04`、`CM-17`、`RT-16`；顺带 `CM-06`、`CM-09`。

#### B1：设计纯数学 API

在 `engine/core/include/cuexis/core/math.hpp`/对应实现中加入只依赖 Cuexis 自有类型的纯函数：

- `hermiteProgress(double, double, double)`；
- `lerp(Vec3, Vec3, double)`；
- shortest-path `slerp(Quat, Quat, double)`，统一 hemisphere、dot clamp、near-linear
  分支和最终 normalize；
- 如有调用需要，加入明确命名的 Euler/ZYX → Quat helper 和 pi 使用说明。

GLM 仍只能出现在 `engine/core/src/math.cpp`；公共头不暴露 GLM。

#### B2：迁移调用点

1. 删除 behavior、animation sampler、animation mixer 中重复 helper，改调用 core::math。
2. 行为采样失败仍包装为 `behavior.sample.quaternion_invalid`；动画路径不能丢弃 normalize
   的 Result。
3. 所有输入使用同一 clamp/rounding 顺序，避免 1 ULP 差异破坏 determinism。
4. 手写 ZYX 构造迁移时明确 yaw/pitch/roll 轴、组合顺序和右手坐标系，不能只改函数名。

#### B3：修复 normalize 溢出

计算 quaternion lengthSquared 后先检查 finite；乘法溢出成 inf 或非 finite 时返回稳定错误
（推荐 `core.math.quaternion_not_representable`，若已有相近 code 则复用），不能把
inverseLength 设为 0 后返回全零四元数。补大数量级、零长度、NaN/Inf 和正常值测试。

#### B4：低风险 core 测试

- `CM-06`：决定支持大小写 UUID 或明确 canonical lowercase-only；若支持，使用 ASCII fold，
  不受 locale 影响。
- `CM-09`：新增 NIST SHA-256 known-answer vectors、55/56/63/64/65 字节跨块边界和大输入，
  保留现有空串/abc。
- `CM-05`：若不改 inverse，头和测试固定 absolute epsilon；若改相对阈值，另开行为变更任务。

#### B5：Lane B 门禁

~~~powershell
ctest --preset debug --no-tests=error -R "cuexis_core_tests|cuexis_behavior_tests|cuexis_animation_tests|cuexis_runtime_tests"
ctest --preset debug --no-tests=error -R "cuexis_playback.*|cuexis_cfu_f3_determinism"
~~~

必须比较迁移前后的 behavior/animation golden；浮点末位变化也要确认是规范允许的统一结果，
再更新指纹并写原因。

### 9.3 Lane C：文档事实化并接入 CI

**覆盖**：`AP-16`、`AP-17`，并为 AP-01/AP-02/AP-03/AP-04 建立持续校验。

#### C1：机器可读状态合同

新增 `docs/status_contract.json`，建议包含 `snapshotDate`、按文件分组的 required 片段、
带 `minLength` 的 stale 片段和 datedFiles。`check_docs.py` 读取该文件，保留现有
whitespace-normalized 检查；日期只在 JSON 单点维护；失败信息指出具体文件、合同和片段。

不要为了减少 Python 行数删除真实 CFU/Stage 状态检查，也不要使用短到会误报的 stale phrase。

#### C2：从 CMake 导出 target 事实

configure 阶段用 `file(GENERATE)` 生成非源码的 `generated/cuexis-targets.txt`，内容
来自 `CUEXIS_ACTIVE_TARGETS`。`BUILDING.md` 约定带 begin/end 标记的 target 代码块；
checker 解析该块并比较生成文件，不能手工维护第二份事实源。

#### C3：接入 Linux Quality

在 `.github/workflows/linux-quality.yml` 加轻量 step：

~~~text
python3 -B tools/check_docs.py
python3 -B tools/update_version.py --check
~~~

它应在 C++/vcpkg 构建前尽早失败。Windows workflow 后续可复用同一命令。

#### C4：Lane C 门禁

- 现有 docs 检查和新增 JSON 字段错误均有明确提示。
- 人为增删 target 或改变 SDK API 版本时，CI 稳定检测 BUILDING 漂移。
- 旧 CFU/Stage 状态检查全部保持。

### 9.4 Lane D：修复 Runtime reload 事务语义（RT-01，P1）

**推荐方案**：把 debug 目标帧采样放到 replacement 发布新 World/Scope/Diagnostics 之前；采样失败
仍保留旧 active 状态，不能在已发布新状态后返回 `reloaded=false`。

**实现步骤**：

1. 在 `runtime_session.cpp` replacement context 中构建候选 Runtime/World 和 debug sample
   所需数据，但不修改 active members。
2. 对 target frame 执行 `sampleChartTimeMs`、discontinuity/clock 检查；失败只写候选 diagnostics。
3. 完整校验后以现有 `replaceWith` 单点提交；提交后只做不会失败的 bookkeeping。
4. 如必须提交后 debug，失败只能作为 warning 且返回 reloaded=true，并在规范写明例外。

**测试**：

- debug sampling 失败的 reload 保持 old World/Scope/active diagnostics/frame/identity。
- 成功 reload 的新状态、target frame 和 debug record 与原测试一致。
- debug/release 两种构建覆盖。

## 10. 第 2 批：热路径、音频和资源索引

### 10.1 B2-RENDER：RT-26/RT-27/RT-28 与 AP-08

目标是只优化已明确的每帧线性/字符串/分配热点，不改变排序和 digest 语义。

#### R1：Mesh bounds prepare-time cache（RT-26）

1. 在 `GpuMesh` 或等价 owning GPU resource 中保存经过 finite 校验的 local bounds/center。
2. 上传/prepare 时从 PortableMesh 读取一次；失败返回现有 frame/resource error。
3. `buildDraws` 只通过已解析 mesh pointer 读取缓存，不再对 resources 做 `find_if`。
4. 资源数组仍保持 canonical 排序和生命周期；不得缓存 candidate swap 后悬空指针。
5. 添加资源缺失、非 finite bounds、空 mesh 和正常中心点回归测试。

#### R2：数值 uniform location prepare-time cache（RT-27）

1. 在 `GpuParameterizedProgram`/`GpuMaterial` 保存按 parameter name 或声明顺序的
   `GLint` location；link 成功后一次调用 `glGetUniformLocation`。
2. 保留 location < 0 的跳过语义；可选 uniform 不存在不使整个 draw 失败。
3. `setNumericUniform` 改为接收 location 和 typed value，热帧不构造 name string、不做 GL 查询。
4. location 生命周期绑定 program；program 替换时整组丢弃。
5. 用 mock/GL smoke 统计 prepare 查询次数和每帧查询次数为零；类型和 Texture2D 行为不变。

#### R3：帧 scratch、summary 按需和 Player scene 复用（RT-28/AP-08）

1. presentation/backend state 持久拥有 opaque、transparent、debug vertices scratch；每帧 clear，
   必要时按上限 reserve。
2. `buildDraws` 增加 `needSummary` 等价布尔；只有调用方提供 summary 时才填 command
   copies 和 digest。无 summary 时不额外遍历字符串。
3. summary 非空时保持 opaque/transparent 顺序、objectId、depth key 和 digest 域完全不变。
4. Player 把 `RenderScene` 提升到帧循环外，清空并复用；`appendSnapshotAxes` 预留容量。
5. 运行 player diagnostics、GPU smoke 和 allocation probe；只在被测试覆盖的路径宣称零分配。

#### R4：暂不做 RT-29

不透明 pass 的状态分组排序会改变状态顺序，需要同步 Validation Sink 和
`cuexis.validation.summary.v1` digest。除非有万级对象基准和 owner 许可，保留当前 objectId
主序，把 RT-29 放入触发批。

### 10.2 B2-AUDIO：CM-21/CM-22/CM-23/CM-24 与 CM-03 完整修复

#### A1：HostClock 跨线程发布

推荐复用 `SdlAudioTransport` 已验证的 seqlock：

1. writer 在 owner 线程奇数序列写完整 sample，再以偶数序列发布；reader 前后序号一致才接受。
2. sequence 使用合适的 atomic 类型和 memory order；不要用一个 relaxed flag 代替发布协议。
3. `submit` 仍执行 segment/discontinuity/position 合同；同步不放宽数值校验。
4. `snapshot` 保持 noexcept，在竞争下使用有界重试或最后完整快照策略，不能死循环。

#### A2：SdlAudio effective settings 和 replacement

1. 把 `effective` 设置并入同一 published seqlock 或等价 atomic snapshot，消除 CM-23 裸读。
2. 删除 `activateReplacement` 中“恢复 previous 后立即 Error”的死回滚；冻结“openLease
   失败 -> Error，需 unload 才恢复”。
3. `updatePresentedFrame` 显式执行 min(max(candidate, presented), upperBound)，防御不变量破坏。
4. stop/seek 的 relaxed counter 清零只影响呈现估计精度；注释或改清晰发布协议。

#### A3：测试

扩展 `tests/audio_sdl/sdl_audio_tests.cpp`：prepareReplacement 成功/失败、activate 成功/失败、
cancel、underrun、Error 后 unload、effective settings 跨线程 snapshot；增加 HostClock 多线程压力测试，
检查每个快照自洽。保留 fake transport 的 RuntimeFrame trace parity。

### 10.3 B2-PB12：Presentation resource transparent lookup

**问题**：`PresentationResourceKey` 使用非透明 comparator，prepare 对每个 object/material/step
event 线性扫描，最大 65,536 条资源时退化为 O(n²)。

**实现步骤**：

1. 给 key comparator 提供 `is_transparent` 和 typed/string_view overload，或建立 canonical
   flat index；比较规则是 portable bytes，不做 locale fold。
2. manifest extraction 时验证 assetId/type 唯一，把重复诊断前移。
3. `findPresentationResource` 改为 lower_bound/heterogeneous find，保留 nullptr/invalid-reference
   错误语义。
4. `assembleResourceIdentities` 也使用同一索引，不另建线性副本。
5. 资源排序、identity bytes、manifest owning copy 不变。

**测试**：1、2、65,536 条资源；同 AssetId 不同 type；重复 key 拒绝；manifest order 和 FrameDigest
golden 不变。

## 11. 第 3 批：Playback prepare 分层和 Chart parse-once

这是最大结构性批次，必须先完成第 1 批 identity 语义。目标是减少错误路径分叉、重复解析和诊断
漂移，同时不改变成功输入的可观察结果。

### 11.1 B3-PREPARE：prepare 分阶段

#### P1：内部上下文和 artifact

在 `engine/playback/src/playback_session.cpp` 或同目录 internal header 建立仅内部使用的
`PrepareContext` 和 `PrepareArtifact`。建议 artifact 按以下字段分组，并明确所有权：

~~~text
source state / canonical text
parsed or typed chart document
resolved parameters and parameter identity
CXT identities and imported documents
resource requirements and acquired leases
runtime prepared candidate
presentation candidate and manifest
snapshot layout
chart/resource/semantic identity components
diagnostics
~~~

不要把 JSON DOM、World、EnTT 或 shader compiler 类型放进公共头；artifact 只能在 prepare 内部移动，
commit 前不得触碰 active state。

#### P2：固定 stage 顺序

建议拆为：

1. `loadDocument`：读取/识别 source，保留原始 cause。
2. `resolveParameters`：Chart v4 参数校验、冻结、identity。
3. `preflightCapabilities`：parse 成功后、资源获取前检查 Playback capability。
4. `compileAnimation`：只消费 typed Chart data，保持 Stage 4 错误顺序。
5. `compileRuntime`：构造候选 RuntimeSession，不发布。
6. `acquireAudio`：获取并校验主音乐。
7. `prepareRuntime`：在候选 session 上采样 target frame。
8. `preparePresentation`：解析/校验 portable payload 和 capability。
9. `commitFrame`：候选内部 frame/layout 组装，不改变 active。
10. `buildLayout`：生成 Snapshot layout 和资源引用索引。
11. `assembleIdentity`：组装 Chart/CXT/resource/parameter identity。

每个 stage 使用类似 `core::Result<void> stage(const PrepareContext&, PrepareArtifact&)` 的签名；
可选结果放 artifact 的明确 optional 中，不使用多个隐式 out parameter。

#### P3：RAII DiagnosticsRecorder

1. prepare 入口创建 recorder，立即开始本次 operation diagnostics。
2. recorder 析构时无条件把排序后的本次 diagnostics 写入 `state_->lastOperationDiagnostics`，
   覆盖显式 error、source invalid、commit/layout failure 和 exception。
3. 成功也要定义规则：记录 warnings/空 diagnostics，不能留下上一操作旧值。
4. catch bad_alloc/标准异常时先转稳定 error，再由 recorder 写入；析构不能再分配或抛异常。
5. 保持 append、cause、fieldPath 和 deterministic sort；不能重复写同一错误。

#### P4：表驱动 capability 和错误码 taxonomy

1. 把 `validatePresentation` 的 capability flag、required ID、field path、limit 放进
   `CapabilityRule`/`LimitRule` 常量表，表顺序即诊断顺序。
2. 采用 D3：`frame.value_invalid` 作为 boundary-only code，production extract 不凭空新增。
3. `resourceTypeName`、material/texture 解析和 PB-15 重复逻辑收敛为内部 helper；死防御
   分支删除或用断言说明不变量。
4. `parseShader` 抽出 counted identifier、ASCII、长度、重复集合 helper；每个 helper
   返回带 path 的 Result/diagnostic，不静默 continue。
5. 建立静态/脚本检查：规范 code 表中的每个 code 有明确 production/Validation Sink/boundary-only
   归属；仅存在于 spec 的 code 必须标注。

#### P5：PB-03 测试矩阵

逐一覆盖 ChartWriter 失败、mode/content mismatch、commit 失败、buildSnapshotLayout 失败、
source invalid、所有 catch。每个测试断言：

- lastOperationDiagnostics 有值且只代表本次操作；
- code/cause/path 稳定；
- active state、identity、frame、manifest 未被失败候选污染；
- 成功下一次操作没有上一失败 diagnostics 残留。

### 11.2 B3-PARSE：parse-once

#### Q1：Chart loader 共享解析结果

1. `ChartV4Loader::load` 的内部结果携带已解析 `cuexis::json::Value` 或 internal
   parsed-document handle，同时保留 typed source document。
2. `ChartV4Resolver::resolve` 增加 internal parsed-value overload；字符串入口只 parse 一次后委托。
3. `ChartWriter::writeV4` 增加 internal value overload；canonical serialization 继续走同一
   sorting/number normalization。
4. `makeLegacyProjection`、`makeConcreteChart` 和 identity 计算消费同一 parsed value
   的 owning copy/view，不能 serialize → parse 恢复原文。
5. CXT import 每个文本仍按自己的 JSON 预算 parse 一次；Chart 和 CXT DOM 不混用。

#### Q2：ChartLoader 和 CXC peek

1. `ChartLoader::load` 采用“parse once then inspect format”或提供只读
   `peekChartVersion(text, limits)`，不先嗅探再交给会重新 parse 的 loader。
2. `cxc_package.cpp::isV4Chart` 使用 peek/header，不再完整 parse 两次；保留 envelope/hash/closure 顺序。
3. 解析 code、fieldPath 和诊断顺序必须与旧入口兼容；先写 parity test 再删除旧 parse。

#### Q3：所有权和确定性

- parsed value 生命周期须覆盖 loader → resolver → writer，但 public string_view 不得指向临时 DOM。
- 对 16 MiB Chart、多 CXT 和失败早退做峰值内存/分配 probe；不引入无界缓存。
- 复跑 CFU-F1/F3/F4、Stage 4、Stage 5-H identity/digest golden，要求 canonical bytes、semantic
  identity、FrameDigest 和 diagnostics fingerprint 逐项一致。

## 12. 搭车任务：按邻近改动执行

“搭车”不是免验收清单。它表示任务应在已经触及同一文件、同一数据结构或同一诊断 helper
时完成，而不是单独发起一项高风险大重构。实现模型每完成一个搭车项，仍须在提交说明和整改
报告中引用原始 finding、修改理由和聚焦测试。

C-R0 是唯一预先批准的结构性例外：它已有独立原子任务、双槽波次和零行为变化门禁，用来先
恢复 Chart reader 的文件所有权边界。它不授权其他搭车项仿照其名义单独扩大范围。

### 12.1 Chart 内部一致性

#### C-R0：Chart reader 职责拆分（无独立 finding，受控结构任务）

**判断**：有重构必要，但必要性来自不同的变更原因和测试维度，而不是单纯因为文件行数大。
`engine/chart/src/chart_v4_reader_internal.cpp` 当前同时承担以下三类职责：

1. V4 通用读取和诊断：stable ID、RationalBeat、required extensions、稳定错误封装。
2. Animation/CXT 读取：number/vector/quaternion、parameter source、property mask、clip、Animator、
   blend/fill/iterations。
3. Project document path：portable path 字符约束、`.cxt` 后缀和 ASCII case-fold key。

第三类职责位于文件末尾并被 loader/resolver 用于工程文档索引，与 animation reader 的调用者、
变化原因和测试合同不同；internal header 也同时暴露两组接口。这已经构成低内聚，而非纯审美
问题。后续 CH-01、CH-08、CH-14 和 parse-once 都会触及 reader 数据流，如果继续集中在同一
文件，两个子智能体难以获得互斥且可审查的文件所有权。

**执行顺序**：必须先完成 `AG-B0-CH01-I`，让 P0 修复在原位置以最小 diff 关闭；随后执行
`AG-CH-COHESION-T/I`，再允许 identity/parse-once 或 Chart 搭车任务继续。不得把 P0 行为修复、
文件移动和 CH-14 helper 合并压进同一个提交。

**目标文件边界**：名称可以按仓库最终约定微调，但职责不得重新混合。

- `chart_v4_common_reader_internal.*`：`addV4Error`、portable stable ID、V4 rational、required
  extensions 等跨 V4 loader/resolver 的通用读取合同。
- `chart_v4_animation_reader_internal.*`：animation property/value/mask、clip、Animator、
  blend/fill/iterations，以及只服务这些读取器的 finite/vector/quaternion helper。
- `chart_project_path_internal.*`：portable project path、CXT path 和 ASCII case key。当前先留在
  `cuexis_chart` 私有实现；只有执行 CX-14 的跨模块 portable-path 统一任务时，才讨论上提。
- 原 `chart_v4_reader_internal.*` 要么删除，要么缩为单一明确职责的兼容 internal facade；不得
  保留所有旧声明再转发，否则只是移动实现而没有恢复所有权边界。

**禁止事项**：

- 不新增 public header，不改变 `cuexis_chart` target、依赖 allowlist 或安装面。
- 不趁移动代码改变错误码、message、fieldPath、诊断排序、预算、路径字符集或大小写规则。
- 不把 canonical 与 V4 的 number/vector/quaternion reader 直接合成一个带大量布尔开关的 helper；
  CH-14 的共享必须先区分“解析机制”和“版本/错误策略”。
- 不把 project path helper 下沉到 `core`；它是格式/包策略，不是通用数学或基础设施合同。

**验收**：重构前先保存合法/非法 Chart、CXT import、case-fold duplicate、mask/Animator、预算边界
的 diagnostics fingerprint。重构后要求 code/message/fieldPath/context/order、typed document、
canonical bytes、semantic identity、capability 和所有 golden 完全一致；新增源文件只需登记到
`engine/chart/CMakeLists.txt`，根 allowlist 不应变化。

`canonical_chart_loader.cpp` 约 2055 行，是第二层职责聚合：它同时处理通用读取、component/
camera、patch/template inheritance、keyframe/event behavior、objects、timing 和顶层 load/compile。
它同样值得分解，但风险高于上述三分拆。只能在第 3 批 `AG-PARSE-T` 已冻结 parity 后，由 `M`
按功能区生成多个 `AG-CH-CANONICAL-I<n>` 原子任务，逐个提取 internal 模块；不得把整个文件交给
一个较弱模型，也不得让“拆文件”与 parsed-value overload、CH-16 早退或诊断 taxonomy 同时发生。

#### C-R1：`CH-07`，缓存展开后的 mask

- 在 `validateProgram` 进入两两 layer 比较前，为每个 layer 计算一次 canonical expanded mask。
- 缓存必须按 layer 的稳定 identity 索引，不能用 unordered iteration 影响诊断顺序。
- 比较循环只读取缓存；保留现有 O(L²) 语义和冲突优先级。
- 用接近最大 layer 数的 fixture 测试 CPU 工作量和诊断顺序；不要改变合法冲突结果。

#### C-R2：`CH-08`，避免 Animator 双重完整解析

- 让 loader 产出的已验证 typed Animator 结果传给 resolver；如果生命周期不允许，增加只读
  internal view，而不是再次从 JSON 读取。
- resolver 仍可做轻量防御性检查，但不能重复发出同一 source error。
- invalid Animator 的 diagnostics 必须只出现一次，合法 Animator 的 identity 和 capability
  必须不变。

#### C-R3：`CH-09` 至 `CH-11`，错误码 taxonomy

- `CH-09` 统一 format invalid/unsupported 的使用边界；版本不支持仍使用 version code。
- `CH-10` 范围错误使用 `chart.parameter.out_of_range` 类码，类型错误保留 type mismatch。
- `CH-11` mask 条目数预算使用专用 limit code，或者在不新增 code 的决定下明确现有 code
  的 limit 文案；禁止同一条件在不同入口产生不同 code。
- 更新 spec code 表、测试和任何 diagnostics fingerprint。

#### C-R4：`CH-13` 至 `CH-16`，路径、常量和早退

- `CH-13` 的 keyPath 使用输入顺序或显式 source index，不能使用排序后下标伪装源位置。
- `CH-14` 触及 reader helper 时，合并 addError、number/vector/quaternion/camera 校验；
  先写 parity test，再删重复函数。
- `CH-15` 用具名 `opaqueTracks` 判定；在 binary_search 前显式排序
  `emptyBehaviorIds`。
- `CH-16` runtime 对象/behavior 超限立即停止该类循环，同时保留诊断上限 sentinel。

### 12.2 Playback 内部一致性

#### P-R1：`PB-02`、`PB-07`

- 文本和 `PlaybackSource` 两个 `prepareReload` 重载必须采用同一 provider、AssetDatabase 和
  package metadata 继承规则；补带 main music 的 source 重载测试。
- `fromFilesystemProject` 读取 CXT 失败时立即返回带 cause 的 source error；不要把它降级为
  后续 entry_missing。

#### P-R2：`PB-09`、`PB-11`、`PB-13`

- 若保持 `PlaybackSession() noexcept`，把默认 capability 初始化改成不分配或明确的静态只读表；
  不能让 OOM 在 noexcept 构造中无说明地 terminate。
- 删除 `activeChartJson`、`parameters`、prepared parameters、`PlaybackSource::State::sourceId` 前，
  先用全仓搜索和编译确认无读者；不要顺手删掉仍参与 identity 的 source bytes。
- reload 复用或共享只读 AssetDatabase 时，candidate 仍须拥有自己的 provider/lease 生命周期；
  失败不能污染 active。

#### P-R3：`PB-15`、`PB-16`

- 合并 `resourceTypeName`、material lookup 和 texture size 校验 helper；死防御分支要么删除，
  要么用断言记录不变量。
- `PB-16` 只形成 Stage 6 SDK 0.8.0 设计记录：shared owning manifest view、旧 API deprecation、
  文档和 minor version 门禁。本轮不改旧 public signature。

### 12.3 CXC、assets 和 shader

#### X-R1：`CX-07` 至 `CX-11`

- `CX-07` 的 `CxcContentProvider::readBlob` 捕获 bad_alloc，转成稳定
  `content.cxc.read_failed` Result；工厂函数的 OOM 策略单独记录。
- `CX-08` 统一 diagnostics discard 风格，所有 `[[nodiscard]]` 调用都有显式处理。
- `CX-09` 触及 `buildPackage` 时按 CXC_FORMAT §6 的 ZIP、manifest、project、roots、asset
  index、dependency、Chart、CXT、closure 九阶段拆 BuildContext，保持早退和诊断顺序。
- `CX-10` 已纳入 B3 parse-once；不得在 CXC 内复制一套 Chart parser。
- `CX-11` 用 move 或受控单一 owning storage 减少 Chart/CXT 文本副本；先做生命周期图和
  峰值 probe，不能让 string_view 指向释放后的 archive buffer。

#### X-R2：`CX-13` 至 `CX-21`

- `CX-13` 只有在同文件改动时优化 entry index/metadata storage 和 text copy；canonical order 不变。
- `CX-14` 建立单一 `cuexis_internal/portable_path` 校验，project/cxc/assets 通过调用侧注入
  diagnostics；严格字符集按 ADR 0025。
- `CX-15` 记录 maxAssetRoots=16 的当前可接受复杂度，不为理论 O(n²) 引入不必要结构。
- `CX-16` 仅补设计注释，保留 saveAtomic 的双重写前/写后校验。
- `CX-17` 以已有 `requestDirect<Tag>` 为样板，把五路 `ResourceScope::request` 模板化；
  逐 case 对比 lease、fallback、rollback 和 error。
- `CX-18` 只有 probe 证明 handle 反查是热点时才增加反向索引，并记录内存代价。
- `CX-19` 只做职责拆分，保留 bounded DFS、cycle path 和 limit。
- `CX-20` 用结构化 provider error category 取代错误码字符串比较，并保留旧映射测试。
- `CX-21` 确认 AssetType 唯一 owner；过渡期至少加静态映射/编译期断言。

#### X-R3：`CX-22` 至 `CX-27`

- `CX-22` 让 `ShaderCacheStore::store` 返回已规范化 record，消除 prepareCandidate 往返
  encode/decode，但保持 CXSCCH01 bytes。
- `CX-23` 已纳入 Lane A；任何 key/request 半空组合必须在构造期规范化或拒绝。
- `CX-24` 若仍每次新建线程，文档准确写明调用方阻塞；只有真实异步需求才引入持久 worker。
- `CX-25` 在 shaderc 前做完整 UTF-8 扫描，BOM/CR/non-UTF8 code 与 Playback 对齐。
- `CX-26` 只补 cache `.tmp` 残留和同 key 竞争边界；真正唯一临时名可在工具批次处理。
- `CX-27` 直接调用 ShaderCompiler 也校验 entry、parameters <= 32、bindings <= 16，不能只依赖
  Playback CXPRES parser。

### 12.4 Runtime、World、Render 和媒体

#### R-W1：`RT-03`、`RT-09` 至 `RT-16`

- `RT-03` 把 `BeatSample` 从 updatePrepared 传给 debug 路径，保证每帧只做一次 timing map sample。
- `RT-09` 提取 checked `findBehaviorIndex`，所有调用点传播错误，不依赖隐式 prepare 前置。
- `RT-10` 复用 override scratch；`RT-11` 若开启 debug，建立 entity→objectId 反向索引。
- `RT-12` 未知 BehaviorProperty/easing 返回稳定 error，禁止静默回退。
- `RT-13` 用 `std::as_const` 或等价委托合并 const/non-const withWorld/withRegistry。
- `RT-14` 记录当前错误后的有限无效请求为接受现状，除非同文件重构时可安全提前 break。
- `RT-15` 明确 override lifetime tick 与提交失败回滚的顺序；若语义要求事务性，改为 commit
  成功后扣减，否则在 Runtime spec 写清当前行为。
- `RT-16` 已纳入 Lane B；pi 和 ZYX 旋转 helper 统一来源并写组合顺序。

#### R-W2：`RT-17` 至 `RT-25`

- `RT-17` 至 `RT-20` 归入 T1 大规模优化，当前只保留 probe 和设计草案。
- `RT-21` 合并 RenderMaterial 校验逻辑，确保 store/parse 两入口错误一致。
- `RT-22` 标记 applyLayer(duplicateIsError=false) 和 production 不可达的 prepare 分支为
  test-only；删除前确认测试用途。
- `RT-23` 说明 float exact equality 是 transform cache 的前提；不要在本任务引入 epsilon。
- `RT-24` 将 CameraComponent::type 改 enum 需要 API/serialization 决策；否则明确 reserved。
- `RT-25` 已在 B05 标注旧 renderFrame 主路径，链接真实 presentation path。

#### R-W3：`RT-30` 至 `RT-34`

- `RT-30` 在触及 debug pipeline 时迁移到既有 Unique* RAII，保证 GL context owner 正确。
- `RT-31` 共享 DebugVertex、shader log、program log 和 compile helper，先做 parity test。
- `RT-32` 用结构体/tuple 作为 program de-dup key；若 bytes identity 参与排序，保持 length-aware
  比较，不使用 locale。
- `RT-33` 已在 B05 写 terminate/owner 顺序；代码整理不得把 contract violation 改成可恢复错误。
- `RT-34` 已在 B05 写透明深度量化常量来源；改变数值必须重新评估 summary/digest。

### 12.5 App、工具、构建和测试

- `AP-08` 在 B2-RENDER 处理；`AP-09` 合并 stateName/audioStateName；`AP-12` 提炼
  requireValue，保留 duplicate/missing/invalid exit code。
- `AP-14/AP-15` 建立共享 tools_common，统一 diagnostics formatter、IO/usage/content exit code
  和安全 staging；chart_migrator 不再使用时间戳临时文件。
- `AP-18` 将 VCPKG triplet 从通用 base 拆到 windows-base；Linux workflow 继续显式设置 x64-linux。
- `AP-21` 重命名 demo asset target 为 `player_demo_assets`，同步 CMake 和文档。
- `AP-22` 给 asset_importer main 增加顶层异常转换；机器输出只使用稳定 code/message/context。
- `AP-23` 若测试与生产 app source 的编译宏可能漂移，抽出共享 player support target；至少用
  CMake 条件确保同一实现文件。
- `AP-24` 把重复最小 Chart JSON 移入 `tests/fixtures`/test support，保持测试 fixture 角色。

## 13. 触发条件批次

这些项目在审查时被明确建议“等触发”。触发前只能补注释、探针、设计草案和回归基线；不得
为了让清单看起来全绿而提前引入高风险结构。

### T1：World/Animation 大规模热帧

**覆盖**：`RT-02` 代码侧、`RT-10`、`RT-11`、`RT-17`、`RT-18`、
`RT-19`、`RT-20`、`CM-25`。

**触发器**：实际内容达到数千实体或大量 active layers，或 Studio 开始连续编辑预览。触发前
先用 performance probe 记录 CPU、allocation、resident memory 和 FrameDigest trace。

**实现顺序**：

1. `PropertyResolver::Entry` 增加 `lastTouchEpoch`；`beginFrame` 递增 epoch，`markTouched` 变为
   O(1)，处理 uint64 回绕。
2. prepare 先收集 entity IDs，最后一次排序和建 Entry，消除胖 Entry 的 `vector::insert` O(N²)
   memmove。
3. override candidates 和 Runtime scratch 复用，按既定上限 clear/resize。
4. `AnimationCompiler` 预计算 object→有序 layer index、property blend groups、mask bitset；
   `AnimationMixer::evaluate` 改索引寻址，删除每帧 string-keyed map/sort。
5. animation baseline 在 prepare 建骨架，帧内 in-place 更新；reload/seek 或 binding 集合改变时才重建。
6. 用旧/新双实现对照 mixer tests、debug source/layer/conflict、seek/stop/discontinuity 和 FrameDigest；
   对照完成后才删除旧路径。

**禁止**：把 World/EnTT 暴露到 Playback；用 unordered iteration 破坏确定性；没有 probe 就宣称零分配。

### T2：大 CXC/JSON 输入

**覆盖**：`CX-12`、`CM-11`，可带 `CX-11`、`CX-13`。

**触发器**：真实包超过 100 MiB、Studio 大内容或可复现性能投诉，并取得 owner 对 CFU-F 独立
archive cross-check 的决定。

**推荐顺序**：

1. 先改 minizip 比对为流式，保留独立校验，减少每 entry 临时 64 MiB vector。
2. 再评估 JSON SAX 直构 `cuexis::json::Value`；duplicate-key、depth、string-limit、number
   normalization 和 field path 必须与现有 DOM parser 一致。
3. 包内 span/shared ownership 涉及 public layout，另开 ADR，不作为性能顺手改。

**门禁**：所有 parse/schema/CXC/Chart identity golden、ASan/UBSan、最大合法资源和失败早退 probe
通过；若 error code/path/order 变化，先停止并重新设计。

### T3：万级对象 render state sorting

**覆盖**：`RT-29`。

**触发器**：万级对象或明确需要减少 GL state changes。设计排序键为
`(programKey, cull, textureKey, objectId)`，透明 pass 以 depth key 为主序。

必须同步更新 OpenGL `buildDraws`、`tests/presentation/validation_sink.cpp` 第二实现、
`cuexis.validation.summary.v1` digest/排序合同、Player GPU parity、summary golden 和 frame diagnostics。
如果 digest 和 validation 不能一起更新，保留旧排序。

### T4：Stage 6/API 和 Player 剧本

- `PB-16`：Stage 6 SDK 0.8.0 增加 shared manifest owning view，旧 API deprecated，走版本工具和
  external consumer 门禁。
- `AP-07`：只有新增 smoke/audio/reload 剧本时才把 `player_app::run` 拆为 frame→step 表和 cleanup
  state machine；不要为抽象而抽象。
- `AP-11`：新增跨 timingOffset reload fixture 时同步重建 ChartClock；当前同项目 smoke 只记录为
  潜伏一致性测试。
- Chart parameter typed 化（审查方向 11）：等 Stage 6 参数化字段扩展，再把 position/scale/camera.fovY
  的字符串 field path 改为 `NumberSource` typed variant。

## 14. 全量 finding 处置映射

下面的表是审查报告 144 个编号的闭环索引。实现模型必须引用“批次/任务标签 + 原始编号”；
表中的“记录”或“接受”不表示代码已修复。

| 处置路径 | finding |
| --- | --- |
| 第 0 批 | CH-01、AP-01、AP-02、CM-01、AP-06、PB-08、AP-19、CM-03、RT-02（文档侧） |
| 第 0.5 批 | CH-03、CH-04、CH-12、PB-05、PB-06、PB-10、PB-04（文档归属说明）、RT-05、RT-06、RT-08、RT-25、RT-33、RT-34、CM-02、CM-05、CM-08、CM-10、CM-13、CM-14、CM-20、CM-24（注释）、CM-27、CX-03、CX-04、CX-06、CX-16、CX-26、AP-03、AP-04、AP-05 |
| 第 1 批 Lane A identity/cache | CH-02、CX-02、CX-05、CX-23 |
| 第 1 批 Lane B math | CM-04、CM-06、CM-09、CM-17、RT-16 |
| 第 1 批 Lane C docs/CI | AP-16、AP-17 |
| 第 1 批 Lane D reload | RT-01 |
| 第 2 批 render | RT-26、RT-27、RT-28、AP-08 |
| 第 2 批 audio | CM-21、CM-22、CM-23、CM-24（代码侧） |
| 第 2 批 resource index | PB-12 |
| 第 3 批 prepare/taxonomy | PB-03、PB-04（最终 code 归属）、PB-14、PB-15 |
| 第 3 批 parse-once | CH-05、CH-06、CX-10 |
| 搭车 Chart | CH-07、CH-08、CH-09、CH-10、CH-11、CH-13、CH-14、CH-15、CH-16 |
| 搭车 Playback | PB-02、PB-07、PB-09、PB-11、PB-13；PB-16 仅记录 Stage 6 设计 |
| 搭车 CXC/assets/shader | CX-07、CX-08、CX-09、CX-11、CX-13、CX-14、CX-17、CX-18、CX-19、CX-20、CX-21、CX-22、CX-24、CX-25、CX-27 |
| 搭车 runtime/world/render | RT-03、RT-07、RT-09、RT-12、RT-13、RT-15、RT-21、RT-22、RT-23、RT-24、RT-30、RT-31、RT-32 |
| 搭车 core/media | CM-07、CM-12、CM-15、CM-16、CM-18、CM-26 |
| 搭车 app/tools/build/tests | AP-09、AP-12、AP-14、AP-15、AP-18、AP-21、AP-22、AP-23、AP-24 |
| 等触发 T1 | RT-02（代码侧）、RT-10、RT-11、RT-17、RT-18、RT-19、RT-20、CM-25 |
| 等触发 T2 | CX-12、CM-11 |
| 等触发 T3 | RT-29 |
| 等触发 T4 | AP-07、AP-11、Chart 参数化 typed 化（无独立编号） |
| 需要 owner/spec 决策 | PB-01、CX-01、CH-03（若选统一 BPM 域）、RT-04；PB-04 若选择 production emission |
| 接受现状，仅记录 | CM-19、CX-15、RT-14、AP-10、AP-13、AP-20 |

映射中的重复出现是有意的：CM-03 先在第 0 批声明契约、再在第 2 批实现同步；RT-02 先修
文档、触发后修代码；PB-04 先决定归属、再由第 3 批 taxonomy 固定。一个 finding 的最终状态
只能由整改报告判定，不能由表格位置推断。

`AG-CH-COHESION-*` 和 `AG-CH-CANONICAL-I<n>` 是为降低 CH-08、CH-14、CH-16 与 parse-once
实施风险而增加的相邻结构任务，没有独立 finding 编号，不改变 144 项映射。其完成只说明
内部所有权边界改善；只有对应 finding 的行为、测试和报告门禁全部满足时，才能关闭 CH 编号。

## 15. 测试和验证总矩阵

### 15.1 每批必跑

~~~powershell
python -B tools/check_docs.py
python -B tools/update_version.py --check
git diff --check
~~~

代码批次还要在 Visual Studio Developer shell 中运行：

~~~powershell
cmake --preset debug --fresh
cmake --build --preset debug --clean-first
ctest --preset debug --no-tests=error
~~~

### 15.2 结构和边界门禁

- cuexis_architecture_tests：任何 include、模块依赖或公共头改动都必须跑。
- cuexis_format_check：C++ 修改必须跑；clang-format 不可用时报告环境阻塞，不能换格式工具。
- public-header ASCII、installed Playback leak、static/shared external consumer：公共头、CMake
  install、SDK/API、依赖或错误边界变化时必跑。
- adapter-disabled headless：Playback identity、parse-once、diagnostic、capability 或 shader
  cache 变化时必跑。
- shader-tools 开启、Linux sanitizer shader-tools：cuexis_shader、缓存、编译器或 OpenGL 消费
  路径变化时必跑。

### 15.3 聚焦测试

| 批次 | 最低聚焦测试 |
| --- | --- |
| 第 0 批 | cuexis_chart_tests、cuexis_playback_tests、cuexis_audio_tests、docs check |
| 第 0.5 批 | check_docs.py、public-header ASCII、git diff --check |
| Chart 内聚性前置 | cuexis_chart_tests、Chart v4/CXT path parity、format、architecture、identity fingerprint |
| Lane A | cuexis_playback_tests、cuexis_shader_tests（shader-tools）、CFU-F3 determinism、importer/Player cache hit |
| Lane B | cuexis_core_tests、cuexis_behavior_tests、cuexis_animation_tests、runtime parity |
| Lane C | check_docs.py、update_version.py --check、Linux workflow 文档 step |
| Lane D | cuexis_runtime_tests、debug/release reload failure tests |
| B2 render | render/player diagnostics、GPU smoke、allocation probe、presentation parity |
| B2 audio | cuexis_audio_tests、cuexis_audio_sdl_tests、fake transport、cross-thread snapshot |
| B2 PB12 | presentation validation、manifest lookup、identity/duplicate-key tests |
| B3 | full chart/cxc/playback、diagnostics failure matrix、CFU-F1/F3/F4、S5-H |

### 15.4 Determinism 和性能要求

- identity/cache 变化：比较 canonical bytes、PreparedSemanticIdentity、FrameDigest v3、cache key
  和 diagnostics fingerprint；旧 golden 变化必须有原因记录。
- render 变化：summary 非空时 command 顺序和 digest 不变；summary 为空时只验证无额外 digest
  工作，不能把“更快”当作语义证明。
- audio 变化：所有 snapshot 满足 segment/discontinuity/position 不变量；跨线程测试不能只
  断言“未崩溃”。
- parse-once 变化：对空、最小、最大合法和最大+1/非法输入比较 error code/path/order；峰值
  内存只做趋势证据，不设机器相关硬阈值。

### 15.5 Release 和 hosted 退出门禁

跨公共契约、identity、依赖或第三批结构性改动完成后，在同一最终 SHA 运行：

~~~powershell
cmake --preset release --fresh
cmake --build --preset release --clean-first
ctest --preset release --no-tests=error
~~~

随后等待既有 hosted Linux Quality、Windows MSVC、Windows MinGW workflow。shader-tools 改动还
必须包含 Linux sanitizer shader-tools。整改报告记录 implementation SHA、workflow run URL、
每个平台首个失败步骤和环境重试，不得只写“CI 通过”。

## 16. 整改报告和关闭规则

每个批次完成后，在 docs/stage_reports/ 新增带日期的报告，例如
2608xx-full-review-remediation-b0.md。报告至少包含：

~~~text
snapshot date
implementation SHA
原始 finding 编号和本计划任务 ID
实际修改文件
行为、identity、digest 是否改变
聚焦测试和全量门禁结果
hosted 结果（如适用）
已知残余风险和未关闭 finding
owner acceptance（若需要）
~~~

报告必须链接回 260829-full-review.md，但不能改写该历史文件。只有代码、测试、规范/文档和
必要 hosted 证据都完成，才能把 finding 标为 closed；“已安排”“已注释”或“本地通过”不能
冒充关闭。

### 16.1 批次完成检查表

1. 任务卡列出的文件之外没有无关改动。
2. 诊断 code、message、fieldPath、cause 和排序已经与现有合同比对。
3. active state、candidate rollback、owner-thread 和异常边界有测试。
4. identity、FrameDigest、canonical order、capability default 没有意外变化。
5. architecture/allowlist/install/public-header 门禁通过。
6. 文档链接、单一 H1、Stage 名称和脚本边界检查通过。
7. 报告已记录 SHA、日志、残余风险和下一批依赖。

### 16.2 状态更新顺序

1. 先完成代码和本地门禁。
2. 再做同 SHA hosted 验证、安全和性能证据。
3. 对决策项取得 owner/spec acceptance。
4. 新增整改/复审报告并更新报告索引。
5. 最后才由维护者更新 CURRENT_STATUS.md 或 Stage Plan 的完成状态。

## 17. 给较弱实现模型的提示模板

第 6.10 节给出了多智能体外壳。主智能体可直接复制下列完整模板，再填入一个具体原子任务；
不要把多个 Lane 拼成一条提示，也不要省略并发槽位和写锁。

~~~text
你是 Cuexis 260829 Full Review 整改波次 <WAVE-ID> 的子智能体 <W1|W2>。
最多只能有两个子智能体同时工作；你不得创建任何子智能体。
并行伙伴：<PARTNER-TASK|none>。不要把伙伴的未完成修改当成已冻结合同。

你只执行原子任务 <TASK-ID>。
原始 finding：<ID>。
基线 SHA：<SHA>；已批准决策：<DECISIONS>；已完成前置任务：<DEPENDENCIES>。
独占锁：<LOCKS>。
只允许修改：<FILES>。
只读参考：<READ-ONLY FILES>。
目标行为：<OBSERVABLE CONTRACT>。

先读任务涉及的实现、公共合同和现有测试，再做最小修改。
保留现有公共 API、FrameDigest、canonical order、架构 allowlist、Playback-only 边界和兼容路径，
除非任务卡明确授权。
不要顺手重构未列出的模块，不要把计划/本地通过写成已关闭。不要 commit、reset、checkout、
rebase、清理工作树，也不要修改原审查报告、CURRENT_STATUS、整改计划或关闭报告。

完成后运行：<FOCUSED COMMANDS>，
仅在没有占用共享构建目录时运行任务卡授权的构建/CTest。文档检查、版本检查和完整批次门禁由
主智能体在波次集成时顺序补齐；子智能体必须报告自己实际运行的命令，不能代替主智能体宣称
全量通过。

返回：
1. task / wave / slot；
2. 读取的权威合同和采用的已批准决策；
3. 实际修改文件、修改摘要和可观察行为；
4. 测试命令、退出码及首个失败；
5. identity/digest/canonical bytes/golden 是否变化；
6. 任务卡外发现但没有修改的问题；
7. 未解决风险、停止条件和建议下一任务。

若遇到未关闭的 owner/spec 决策、需要扩大文件/锁范围、基线漂移、公共 API/格式/错误码变化、
测试冲突、环境缺失或共享构建目录正被使用，立即停止并报告，不要自行选择语义或越界修改。
~~~

本计划的目的，是让每一次后续实现都可定位、可回滚、可验证，而不是把 144 项问题压成一次
无法审查的大重构。任何新增 finding 应先更新本计划的映射和依赖，再进入实现批次。
