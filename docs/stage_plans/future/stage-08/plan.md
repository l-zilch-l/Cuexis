# Stage 8 计划（提纲）：Chart v5 正式发行与语义收敛

状态：future；未启动，仅为方向提纲

更新日期：2026-11-17

<!-- cuexis-meta
id: PLAN-future.stage-08.plan
status: future
authority: none
-->

本文件是**未启动阶段的提纲**，不是执行计划。启动本阶段时，用
[plan.md](../../../templates/plan.md)（详细规划协议）重写，并在变更记录里注明替换关系。

归档来源：[Chart v5 格式计划](../../active/chart-format-update-for-v5/plan.md)、
[Chart Format Update 完成计划](../../completed/chart-format-update/plan.md)、
[CXT v1 格式合同](../../../formats/CXT_FORMAT.md)、
[CXC v1 格式合同](../../../formats/CXC_FORMAT.md) 以及 [ROADMAP](../../../ROADMAP.md) 的 Stage 8 条目。

## 阶段目标

候选，未承诺：把 Chart Format Foundation 与 Stage 6 已验证的 Chart v5 candidate path 收敛为
正式发行合同：Chart v5 authoring model → CXT v2 模板/Pattern 展开 → Canonical Gameplay
Graph（`gameplay.version = 2`）→ Packed Chart 或独立 Gameplay Graph Playback entry → CXC v1
playback entry → Chart v5 Playback / Stage 7A Judgement。本阶段不首次设计 Chart v5，也不承诺
任何能力默认发行；发行矩阵在启动时决定。

候选的发行判据为 40,000 个语义实体、Packed Chart 不超过 `16 * 1024 * 1024` bytes，以及
JSON/CXT/Packed/Runtime 语义等价；具体矩阵与证据形式在启动时由 owner 复核。

## 范围与边界

- 消费 [Stage 7.5 计划](../../active/stage-07-5/plan.md) 的完成、owner 接受与 Stage 8 交接；
  Stage 7 的受限关闭不代替本阶段发行所需的具体生产准入。
- 诊断码正式命名由[诊断独立前置](../../completed/stage-07/02-diagnostics/plan.md)承接，实时架构
  与实体设备/实时同步验收由 [Stage 7-RPA 子阶段](../../completed/stage-07/03-rpa-legacy/plan.md)
  承接；本阶段只做发行兼容，不重做这些工作。
- 实施顺序为诊断独立前置 → RPA → Stage 7.5 → Stage 8；规划登记不表示这些前置已完成。
- 本阶段关闭后的交接边界：Stage 9 可消费稳定的 v5 Presentation/Requirement 边界，Stage 10 可
  开始正式 Studio authoring；本阶段不提前实现 Stage 9–14 的能力。
- 详细格式工作包见 [Chart v5 format plan](../../active/chart-format-update-for-v5/plan.md)。

## 入口条件

- Chart Format Foundation 与 Stage 6 owner acceptance 可得。
- Stage 7A 的 JudgementRequirement / InputEvent / Replay 合同冻结。
- Gameplay v2 的 Canonical Gameplay Graph、Release/tail、single capacity=1 exclusive resource、
  early/late Presentation bridge、TimebaseProfile 与 Ruleset fault transaction 合同冻结。
- 诊断码正式化独立前置 N0–N3 退出；Stage 7 受限关闭及 RPA／PD／原 S7A-9 交接完成
  （原未认证范围保留）。
- 随后独立 Stage 7.5 当前计划范围完成、owner 接受并交接本阶段。

## 已确认约束

- Chart v5 outer `version = 5` 与 `gameplay.version = 2` 是两个版本层，不得以字段存在与否猜测
  Gameplay 语义；本阶段只发行 V2 graph，旧 Gameplay revision 必须显式迁移或稳定拒绝。
- v4 → v5 迁移必须显式执行，不能由 Playback 隐式升级；Chart v4 只接受 CXT v1，Chart v5 只接受
  CXT v2（CXT 字段见 [CXT_V2_FORMAT](../../../formats/CXT_V2_FORMAT.md)）。
- 格式职责固定：CXT v2 不执行任意代码，Packed Chart 不改变语义，CXC 不重新定义 Chart
  （[CXC v1 合同](../../../formats/CXC_FORMAT.md)）；CXC 仍为 v1。
- Packed Chart 是 Chart v5 的物理编码，不是 Chart v6；`packed-chart` 与 `gameplay-graph` 两个
  Playback entry 都恢复同一 Canonical Gameplay Graph，并进入同一
  Judgement/Ruleset/Snapshot/Replay/Presentation 路径，graph entry 不是第二套判定实现。
- Packed Chart 不得改变 Release/tail phase、single capacity=1 resource lifecycle、Fact causal
  order、FactBinding 或 Ruleset fault semantics
  （[PACKED_CHART_FORMAT](../../../formats/PACKED_CHART_FORMAT.md)）。
- Stage 7.5 未纳入发行矩阵的 RequirementKind/capability 必须继续稳定拒绝，不得因出现在候选格式
  字段而获得默认支持，也不得为关闭本阶段伪装成已支持能力。
- Packed entry 上限默认按 `16 * 1024 * 1024` bytes 计算；若产品所有者选择十进制限制，必须在
  ADR 中明确替换，工具和文档不得混用。
- Stage 8 关闭前 Chart v5 只能称为 candidate，不能被 Player 默认作为发行格式加载。
- SDK 版本遵循[版本规范](../../../guides/VERSIONING.md)，不与 Stage 编号绑定，也不预留 `0.8.0`。
- 若发行基线包含 [Stage 7-PD](../../completed/stage-07/04-pd/plan.md)，须消费其已接受的宿主/反馈
  寿命合同、实际 SDK 版本、安装消费者矩阵与新 SHA 预算交接；未接受的 proposal 不能作为发行行为
  （[PD 细则 §12.1、§12.7](../../completed/stage-07/04-pd/design.md)）。该要求约束实际采用的实现，
  不要求提前实现 Stage 9–14 能力。

## 候选方向（非承诺）

启动时可能按以下工作线细化，均未承诺：

- 语义合同与发行矩阵：JudgementRequirement、timeInterval、judgementDomain、requiredAction、
  Tap/Hold/Release、Gameplay v2 identity、Release/tail 作为同一 Requirement 的 phase、resource
  lifecycle、TimebaseProfile 与 Fact causal order、Ruleset fault transaction、early/exact/late/Miss
  FactBinding presentation bridge、有限 Effect Schedule，以及本阶段发行矩阵明确选入的持续、释放、
  组合和顺序约束 capability。
- CXT v2 Core 与 Animation Extension：保留 `cuexis.animation-template` 顶层 format，module 声明
  export kind（animation/prototype/pattern）；Prototype slots、Instance bindings、Parameter freeze、
  ValueSource、Pattern、Finite Repeat/Expansion；参数限于既有类型白名单，不得改变对象类型、
  Component 集合、父子层级、AssetId、引用目标或资源闭包；动画扩展与 Chart Pattern Core 分开校验
  和计数。
- 有限展开与确定性：typed read → module validation → parameter freeze → finite Pattern expansion →
  Template/Prototype expansion → conflict/count validation → Canonical Gameplay Graph →
  capability closure → Packed lowering 或直接 Graph Playback entry → Runtime compile；展开只发生在
  作者源 prepare/显式 compile 阶段，Playback 不读取 CXT AST。
- Packed Chart 物理合同：section table、checksum、Header/Section Directory、STR0/REF0/IDN0、
  ARCH/ENT0、typed component streams、Beat GridDelta/Rational atoms、Gameplay V2 graph projection、
  compiled/prepared identity、decoded-size 与展开计数预算。
- CXC v1 entry 区分：source entry、compiled Packed Chart entry、compiled Gameplay Graph entry、
  playback entry、source/compiled/artifact identity、compiler profile、expanded entity/event counts、
  Ruleset binding、capability/resource closure；Playback 不读取 source entry。
- 迁移与默认 Writer：v4 → v5、旧 Gameplay revision、CXT v1 → v2 的显式迁移或稳定拒绝，
  `--target 5` 与默认 Writer 切换、`--target 4` 兼容窗口；迁移、编译和打包使用原子输出。

## 待决问题（启动前必须回答）

1. 发行矩阵纳入哪些 Stage 7.5 RequirementKind/capability、哪些保持稳定拒绝？由 owner 在启动时批准。
2. Packed entry 上限采用二进制 `16 * 1024 * 1024` bytes 还是十进制限制？若改十进制需 ADR，由产品所有者决定。
3. 发行基线是否包含 Stage 7-PD；若不包含，缺失的 PD 合同如何在发行矩阵与证据中登记？由 owner 与 PD 前置共同决定。
4. 本阶段实际采用的 SDK 版本及兼容/迁移策略是什么？按版本规范由 owner 在发行前批准。
5. 40,000 语义实体 / 16 MiB 的必验 capacity profile（实体组成、动画与身份长度、引用复杂度）如何预先声明并经 owner 接受？

## 验收标准

启动时定义。当前只写方向性判据：

- Chart v5、CXT v2、Canonical Gameplay Graph、Packed Chart 与 CXC entry 的 Spec、Schema、Reader、
  Writer 和 validator 合同一致，且 Graph 与 Packed 进入同一 prepared Judgement kernel。
- JSON、CXT、Canonical Graph、Packed、Graph entry 与 Runtime 的语义等价可验证，包括 semantic
  identity、resource closure 与 Replay identity。
- 未纳入发行矩阵的能力走稳定拒绝路径；超展开数量、超 decoded bytes、超 Packed bytes、递归、
  溢出和非法引用均稳定失败。
- Ruleset transaction failure 的 faulted session 行为（已提交 Fact、拒绝半个 StateDelta、可重建
  PresentationEvent）有负例证据。
- 前置阶段（诊断、RPA、Stage 7.5）的完成与 owner 接受记录、以及本阶段发行矩阵和 SDK 版本的批准
  记录可得。

具体矩阵、命令、数值与证据形式在启动时由 owner 与规划模型制定，本提纲不承诺。

## 明确不包含

- 天空盒、Model v1、静态 glTF 和复杂 Geometry Deformation（Stage 9/11）。
- Studio 正式产品（Stage 10）。
- 任意运行时脚本、通用 Script VM 或宿主回调。
- 改变 CXC 容器版本。
- 把 Packed Chart 当作 Chart v6。
- H06/H08：未来开发，现不支持。
- H01-H05/H07：能力边界，不会再开发，不受支持。
