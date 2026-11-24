# Stage 8 计划（提纲）：Chart v5 正式发行与语义收敛

状态：future；未启动，仅为方向提纲

更新日期：2026-11-23

<!-- cuexis-meta
id: PLAN-future.stage-08.plan
status: future
authority: none
-->

本文件是**未启动阶段的提纲**，不是执行计划。启动本阶段时，用 [plan.md](../../../templates/plan.md)（详细规划协议）重写，并在变更记录里注明替换关系。

归档来源：[Chart v5 格式计划](../../active/chart-format-update-for-v5/plan.md)、[Chart Format Update 完成计划](../../completed/chart-format-update/plan.md)、[CXT v1 格式合同](../../../formats/CXT_FORMAT.md)、[CXC v1 格式合同](../../../formats/CXC_FORMAT.md) 以及 [ROADMAP](../../../ROADMAP.md) 的 Stage 8 条目。

## 阶段目标

2026-11-23 owner 已接受 Chart v5 + Gameplay v3 为正式主线：RR 负责完整实现并实验验证 v3，Stage 7.5 按其完整范围实施、验收并获接受，Stage 8 从已验能力中选择发行组合并负责正式化，作为新创作默认及经验证的播放组合。以 RR 实际接受的源编译、统一时间与准备产物为基础，收敛 Capsule revision 5 / Graph2 两种成品入口及 CXC v1 发布合同。保留已承诺旧内容兼容，不再把完整 Gameplay v2 正式化列为新增交付目标。阶段尚未启动；具体正式 wire、支持行、默认能力及发行证据仍须本阶段验证接受。

候选的发行判据为 40,000 个语义实体、Packed Chart 不超过 `16 * 1024 * 1024` bytes，以及 JSON/CXT/Packed/Runtime 语义等价；具体矩阵与证据形式在启动时由 owner 复核。

## 范围与边界

- 消费 [Stage 7.5 计划](../../active/stage-07-5/plan.md) 的完成、owner 接受与 Stage 8 交接；Stage 7 的受限关闭不代替本阶段发行所需的具体生产准入。
- 诊断码正式命名由[诊断独立前置](../../completed/stage-07/02-diagnostics/plan.md)承接，实时架构与实体设备/实时同步验收由 [Stage 7-RPA 子阶段](../../completed/stage-07/03-rpa-legacy/plan.md) 承接；本阶段复用实际交接，完成 v3 正式化与发行兼容，不重复实时基础架构建设。
- 前置交接包含诊断独立前置、RPA、Stage 7-RR 与 Stage 7.5；RR 的当前实施安排由其计划管理，本提纲不重复调度。规划登记不表示这些前置已完成。
- 关闭后的交接边界：Stage 9 可消费稳定的 v5 Presentation/Requirement 边界，Stage 10 可开始正式 Studio authoring；本阶段不提前实现 Stage 9–14 能力。
- 详细格式工作包见 [Chart v5 format plan](../../active/chart-format-update-for-v5/plan.md)。

## 入口条件

- Chart Format Foundation 与 Stage 6 owner acceptance 可得。
- 消费 [RR 裁定 §24](../../active/stage-07-rr/findings.md#24-统一实施前设计裁定2026-11-23)对应的实际验收与交接：绑定实现提交、通过/拒绝组合、CXT2 Animation/Animator 与 capability 正反例、旧谱独立预期和 Writer 损失/迁移证据，列明未覆盖项。当前裁定及候选合同不能代替这些成果；缺项作为本阶段对应工作包的入口缺口登记。
- Stage 7A 已承诺旧内容的输入、判定、Replay 与错误语义有具名兼容基线；其历史合同不自动成为新 v3 运行格式。
- RR 的 v3 作者编译、共享时间、准备运行表、等级/计分、资源与 FactBinding、源/两种成品一致性完成实际验收；Stage 7.5 须完成其计划完整范围的验收与接受，发行仅选其中已验能力并按该正式化主线交接，不能以发行暂未选入为由跳过其验收。旧 v2 及其他旧组合的规则、文件 hash、标准输入与独立预期分别可核实。
- 诊断码正式化独立前置 N0–N3 退出；Stage 7 受限关闭及 RPA／PD／原 S7A-9 交接完成（原未认证范围保留）。
- 随后独立 Stage 7.5 当前计划范围完成、owner 接受并交接本阶段。

## 已确认约束

2026-11-23 责任衔接：CXT2 Animation 源导入与既有 Animator 绑定由 [RR T-13](../../active/stage-07-rr/tasks/T-13.md)提前交付；取得实际验收后，Stage 8 复用同一适配及 BHV1/ANI1 成品路径，完成正式 v5/v3 默认能力、Writer 与发行验收。旧 ANM0 等只按已承诺兼容的实际需要保留，不为旧 v2 正式目标再强制建设一套新动画编码。正式 wire 仍须接受，当前不称已实现。

2026-11-23 owner 最新裁定取代同日此前“v2 正式目标、v3 仅实验”的发行安排；[RR 决策](../../active/stage-07-rr/findings.md#24-统一实施前设计裁定2026-11-23)与[运行合同](../../active/stage-07-rr/contracts/runtime-timing.md#范围与授权)承接实施和实验验收，正式化归本阶段。该决定不自动把实验构建变成发行支持，也不自动晋升 Judgement 默认 SDK 或删除旧 API；成熟度、实际过渡发布和逐组退役仍独立取证。

- Chart v5 outer `version = 5` 与 `gameplay.version = 3` 分层，CXT2/Clip2/Event2 版本不随之改变。旧 v2 保留具名读取与执行路径，无源旧成品按原规则直接读取，不自动升级、不按 v3 重编或重算；不支持组合稳定拒绝，迁移只能显式执行。
- v4 → v5 迁移必须显式执行，不能由 Playback 隐式升级；Chart v4 只接受 CXT v1，Chart v5 只接受 CXT v2（字段见 [CXT_V2_FORMAT](../../../formats/CXT_V2_FORMAT.md)）。
- 格式职责固定：CXT v2 不执行任意代码，Packed Chart 不改变语义，CXC 不重新定义 Chart（[CXC v1 合同](../../../formats/CXC_FORMAT.md)）；CXC 仍为 v1。
- Packed Chart 是物理编码，不是 Chart v6；正式主线以 RR 实际接受的 Capsule rev5 / Graph2 为收敛基础，两个入口恢复同一拥有型表现/时间/玩法运行表并进入共同 prepare/执行路径。Graph2 不通过旧 v2 作者图重新装配；正式 header、encoding/profile、身份与拒绝集合在本阶段冻结。
- 成品编码不得改变 v3 的时间量化、窗口/等级、计分、资源与触发表现语义；旧 v2 的 Release/tail、TimebaseProfile、Fact/Ruleset 行为在原版本兼容路径独立复验，不能以新规则改写旧结果。
- Stage 7.5 未纳入发行矩阵的 RequirementKind/capability 必须继续稳定拒绝，不得因出现在候选格式字段而获得默认支持，也不得为关闭本阶段伪装成已支持能力。
- 正式发行包中，作者 JSON、CXT source 与原始导入文件作为 Source Project 或显式 source entry 保留，正式 Playback entry 不靠 source 重编译；RR 的显式源编译入口按其[成品合同](../../active/stage-07-rr/contracts/prepared-artifact.md#源输入与-lowering)独立验收，不因本条被取消或自动成为正式播放入口；16 MiB Packed-file 门禁只适用于 `packed-chart` artifact，不套用到独立 `gameplay-graph` entry。
- CXT 不访问 Judgement 内部状态、不读取上一帧结果、不生成运行时对象；参数改变须显式重编。旧 CXT Requirement `effects` 仅按其版本合同作为 source-only 字段处理，非空值须显式 lowering 到表现；不能由旧兼容字段反推 v3 接受集或把表现写入判定/计分。
- 展开禁止任意表达式、Lua/JavaScript/C++、任意 while/for 循环、递归 Pattern、运行时随机、上一帧状态、Judgement 内部状态读取、宿主 API 与逐帧对象生成；合同规定为无序的输入与参数/引用解析须确定，保留声明序参与的身份，不把所有数组一律重排；checked arithmetic 无溢出，展开深度/数量/时间与 decoded bytes 有界。
- 正式表现编码以 RR 实际验收的 SCN1/BHV1/ANI1 及 generated-target 绑定为基础，补齐适用字段、capability、golden 与正式拒绝矩阵。BEH0/BHD0/ANM0/FXS0 不再作为旧 v2 正式化的新建必交项；仅保留有实际承诺和证据的兼容需要，未实现或不支持项继续拒绝。
- Packed entry 上限默认按 `16 * 1024 * 1024` bytes 计算；若产品所有者选择十进制限制，必须在 ADR 中明确替换，工具和文档不得混用。
- Stage 8 关闭前 Chart v5 只能称为 candidate，不能被 Player 默认作为发行格式加载。
- SDK 版本遵循[版本规范](../../../guides/VERSIONING.md)，不与 Stage 编号绑定，也不预留 `0.8.0`。
- 若发行基线包含 [Stage 7-PD](../../completed/stage-07/04-pd/plan.md)，须消费其已接受的宿主/反馈寿命合同、实际 SDK 版本、安装消费者矩阵与新 SHA 预算交接；未接受的 proposal 不能作为发行行为（[PD 细则 §12.1、§12.7](../../completed/stage-07/04-pd/design.md)）。该要求不要求提前实现 Stage 9–14 能力。

## 候选方向（非承诺）

启动时按以下工作线细化；v5/v3 正式主线已接受，具体支持组合与发行准入仍须取证：

- 语义合同与发行矩阵：以 RR v3 的作者拍位、共享 TimingMap、编译运行表、等级/计分、资源与 FactBinding 为基线，接入 Stage 7.5 实际获选能力。新正式组合与旧兼容组合分别列明身份、预算、能力、普通播放/判定支持及拒绝行为；候选旧组合不因仍可读取而扩张为正式兼容保证。
- CXT v2 Core 与 Animation Extension：消费 RR T-13 实际通过的 module/export/Animator 绑定和拒绝集合，复用适配器，补正式 v5/v3 与 CXT Writer 差量；保留 `cuexis.animation-template` format、既有参数白名单及 Core/Animation 独立校验、计数，不引入另一套作者语言或运行时。
- 有限展开与确定性：源 typed read → module/绑定校验 → 有限展开 → 共享时间与玩法编译 → 完整规范化声明/运行表 → 身份终结 → Capsule rev5 或 Graph2。纯成品 Reader 只校验/准备已编译表，不读取 CXT AST 或重跑作者编译；源与两种成品使用同一执行实现。
- 成品物理合同：基于 RR [成品合同](../../active/stage-07-rr/contracts/prepared-artifact.md)实际验收的 Capsule rev5/Graph2、SCN1/BHV1/ANI1/TIM1/JPR1/RES1 等统一收敛，正式接受其适用 header/目录、校验、identity、索引与预算。不得为了旧 v2 正式目标强制并行新建旧编码；实验通过也不免除正式 wire 和跨消费者验证。
- 容量与预算分层：Authoring、Packed Entry、Chart Closure 与 Expanded Runtime 四类预算分别定义；40,000 按语义实体计数，不能以 Packed record 数或隐藏 Pattern 事件规避；source/build provenance、expanded semantic、Packed artifact 与 prepared/replay identity 分开验证。
- CXC v1 entry 区分：source entry、compiled Packed Chart entry、compiled Gameplay Graph entry、playback entry、source/compiled/artifact identity、compiler profile、expanded entity/event counts、Ruleset binding、capability/resource closure；正式 Playback entry 不读取 source entry，显式源编译仍按 RR 的独立入口合同处理。
- 迁移与默认 Writer：消费 [RR T-16](../../active/stage-07-rr/tasks/T-16.md)实际内部 Writer、字段损失与迁移/原子提交验证，继续交付正式 v5 Writer；T-16 不代表该 Writer 或默认切换完成。v4 → v5、旧 Gameplay revision、CXT v1 → v2 的显式迁移或稳定拒绝，`--target 5` 与默认 Writer 切换、`--target 4` 兼容窗口；迁移、编译和打包使用原子输出。

## 待决问题（启动前必须回答）

1. 发行矩阵纳入哪些 Stage 7.5 RequirementKind/capability、哪些保持稳定拒绝？由 owner 在启动时批准。
2. Packed entry 上限采用二进制 `16 * 1024 * 1024` bytes 还是十进制限制？若改十进制需 ADR，由产品所有者决定。
3. 发行基线是否包含 Stage 7-PD；若不包含，缺失的 PD 合同如何在发行矩阵与证据中登记？由 owner 与 PD 前置共同决定。
4. 本阶段实际采用的 SDK 版本及兼容/迁移策略是什么？按版本规范由 owner 在发行前批准。
5. 40,000 语义实体 / 16 MiB 的必验 capacity profile（实体组成、动画与身份长度、引用复杂度）如何预先声明并经 owner 接受？

## 验收标准

启动时定义。当前只写方向性判据：

- 正式 v5/v3 的源 schema、编译器、Capsule/Graph Reader/Writer、CXC entry、validator 和 Playback 一致，普通播放与判定分别覆盖经批准的支持行；在发行配置重跑 RR 三路等价、拒绝与生命周期向量。正式 wire/能力/SDK 消费证据接受后才标为正式支持。
- JSON/CXT 源、Capsule rev5、Graph2 与 Runtime 的语义等价可验证，包括 compiled/prepared identity、时间/等级/计分、资源闭包、表现及适用 Replay identity；不以旧 v2 图重装配代替新成品解码。
- 未纳入发行矩阵的能力走稳定拒绝路径；超展开数量、超 decoded bytes、超 Packed bytes、递归、溢出和非法引用均稳定失败。
- v3 运行失败、当次输出交付、取消/关闭和准备失败沿其已接受合同取证；旧 v2 的 Ruleset transaction failure（已提交 Fact、拒绝半个 StateDelta、可重建 PresentationEvent）仅在仍承诺的旧组合中按原规则复验。
- CXT2 Animation/既有 Animator 的 RR 正反例在正式 v5/v3 配置复验，覆盖 BHV1/ANI1、正式默认 capability 与安装消费者；所需 v2 表现能力不能被 v1 替代，作者规则仍为 CXT2/Clip2/event2/render.alpha。
- 正式 canonical v5 Writer、新建 Gameplay v3 默认、显式迁移和默认切换分别有输出与失败保留证据；RR 内部 Writer 不代验正式 Writer。旧 v2 无源成品按固定文件/hash、原规则、同输入和独立预期直读运行；缺资料如实登记，候选夹具不能冒充正式发布语料。
- 前置阶段（诊断、RPA、RR、Stage 7.5）的实际完成与 owner 接受记录、以及本阶段发行矩阵和 SDK 版本的批准记录可得。

v5/v3 主线已接受；具体矩阵、命令、数值与证据形式在启动时细化并验收，本提纲不声明已交付或已发行。

## 明确不包含

- 天空盒、Model v1、静态 glTF 和复杂 Geometry Deformation（Stage 9/11）。
- Studio 正式产品（Stage 10）。
- 任意运行时脚本、通用 Script VM 或宿主回调。
- 改变 CXC 容器版本。
- 把 Packed Chart 当作 Chart v6。
- 完整 Gameplay v2 正式化或为其强制新建第二套动画/成品实现；承诺内旧路兼容仍保留。
- H06/H08：未来开发，现不支持。
- H01-H05/H07：能力边界，不会再开发，不受支持。
