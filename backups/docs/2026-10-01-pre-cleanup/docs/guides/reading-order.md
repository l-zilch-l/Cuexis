# Cuexis 阅读顺序

状态：current

更新日期：2026-09-29

本文给第一次接触本仓库的 AI 代理（或新成员）一条**按依赖关系排序**的阅读路线，使其在读完之后
能够理解 Cuexis 的产品边界、架构分层和格式体系。本文只回答"先读什么、后读什么、每份回答什么
问题、容易读错什么"，**不复制任何合同、字段或当前状态结论**——那些的唯一权威仍是各自的所有者。

## 0. 阅读前必须知道的三条规则

不先建立这三条，后面几乎一定会读错：

1. **权威顺序**（[DOCUMENTATION_POLICY.md](../DOCUMENTATION_POLICY.md) 「权威顺序」）：
   当前状态页 → 已接受 ADR → 生产 Spec → 当前 Stage Plan → 最新完成/审查 Report → 历史材料。
   同一事实冲突时按此顺序处理。
2. **一份事实只有一个所有者**：ADR 拥有决策，Spec 拥有字段与语义，Plan 拥有范围与门禁，
   Report 拥有带日期的证据。摘要可以存在，但必须链接权威文档，不得复制完整合同。
3. **历史报告是快照，不是现状**：不得从历史报告的"下一步"章节推断当前工作；
   当前工作只以 [`CURRENT_STATUS.md`](../CURRENT_STATUS.md) 为准。

## 1. 一分钟定位（先读这两份）

| 顺序 | 文档 | 回答什么问题 |
| --- | --- | --- |
| 1 | [CURRENT_STATUS.md](../CURRENT_STATUS.md) | 当前处于哪个阶段、什么已关闭、什么是延期或阻塞的 |
| 2 | [ROADMAP.md](../ROADMAP.md) | 阶段顺序与依赖、格式路线、脚本路线、允许的并行工作 |

读完这两份就应该知道：本仓库已完成到哪、下一阶段是什么、哪些边界不允许穿过。
**不要**在这里就下钻到阶段报告。

## 2. 产品与边界

| 顺序 | 文档 | 回答什么问题 |
| --- | --- | --- |
| 3 | [ADR 0027](../adr/0027-playback-sdk-product-boundary.md) | 什么是产品边界：SDK / Player / Studio 三者关系，宿主能用什么 |
| 4 | [PROJECT_GUIDE.md](../PROJECT_GUIDE.md) | 项目定位、核心目标、非目标、架构原则、Definition of Done |
| 5 | [architecture/OVERVIEW.md](../architecture/OVERVIEW.md) | 产品结构、宿主边界、内容流和时间确定性总览 |

关键认知：SDL3、OpenGL、EnTT 是实现或**可选 adapter**，不是宿主必须采用的公共边界；
宿主使用 `PlaybackSession`、`ContentProvider`、`RuntimeFrame`、`FrameSnapshot`，
不访问 `RuntimeSession`、`World` 或后端对象。

## 3. 文档治理（决定你以后怎么引用）

| 顺序 | 文档 | 回答什么问题 |
| --- | --- | --- |
| 6 | [DOCUMENTATION_POLICY.md](../DOCUMENTATION_POLICY.md) | 角色划分、状态词表、目录约定、脚本边界、自动检查 |
| 7 | [文档索引](../README.md) | 文档总索引、权威关系表、精简版推荐阅读顺序 |

新增或移动文档前必须先读第 6 份，否则会破坏链接可达性和状态词约定。

## 4. 玩法语义基础（做输入 / 判定 / 表现工作必读）

| 顺序 | 文档 | 回答什么问题 |
| --- | --- | --- |
| 8 | [architecture/GAMEPLAY_ABSTRACTION_MODEL.md](../architecture/GAMEPLAY_ABSTRACTION_MODEL.md) | 音游的统一语义：时间区间、判定域、动作，以及 Chart/Input/Judgement/Behavior/Presentation 的分层 |

这份是后续 Input、Judgement、Playback 和 Presentation 设计的**共同语义基础**。重点章节：

- §2 核心命题：`NoteRequirement = (timeInterval, judgementDomain, requiredAction, constraints, effects)`
- §3 模型层次：`Timing -> Chart -> Input -> Judgement -> Behavior/Effect -> Presentation`
- §4 行为与"脚本"边界（**最容易误解的一节**，见第 12 节陷阱清单）
- §5 语义与表现的七条分离规则
- §8 典型映射：固定键位音游与触摸区域音游如何落到同一套语义
- §9 当前实现阶段：哪些已有、哪些规划、哪些延期

## 5. 架构与模块边界

| 顺序 | 文档 | 回答什么问题 |
| --- | --- | --- |
| 9 | [architecture/MODULE_BOUNDARIES.md](../architecture/MODULE_BOUNDARIES.md) | 模块职责、依赖方向、格式阶段边界 |
| 10 | [architecture/RUNTIME_SESSION.md](../architecture/RUNTIME_SESSION.md) | 内部事务式运行时生命周期 |
| 11 | [architecture/PLAYER_APPLICATION.md](../architecture/PLAYER_APPLICATION.md) | 参考播放器的源文件职责、窄接口与关闭顺序 |
| 12 | [architecture/STAGE6_PRODUCTIZATION_BOUNDARIES.md](../architecture/STAGE6_PRODUCTIZATION_BOUNDARIES.md) | candidate、renderer、Player support、media importer 和安装分层 |

配套 ADR（按需要查，不必通读）：[0007](../adr/0007-chart-runtime-world-boundary.md) Chart/Runtime/World 边界、
[0014](../adr/0014-unified-chart-object-schema.md) 统一 Object Schema、
[0017](../adr/0017-transactional-runtime-session.md) RuntimeSession 事务、
[0023](../adr/0023-code-error-and-thread-policy.md) 编码/错误/线程政策。

仓库根部的 `AGENTS.md` 是**工程约束的实操清单**：构建命令、架构约束（由 CTest 强制）、
target allowlist 规则、C++ 命名与公共头 ASCII 规则、依赖变更流程、文档检查命令。
动手改代码前必须读它。

## 6. 格式与运行语义

| 顺序 | 文档 | 回答什么问题 |
| --- | --- | --- |
| 13 | [formats/README.md](../formats/README.md) | **先读这份权威矩阵**：每个格式是 implemented 还是 candidate/deferred |
| 14 | [formats/TIMING_MODEL.md](../formats/TIMING_MODEL.md) | TimingMap、Beat 到 chartTimeMs 的映射与绝对时间采样 |
| 15 | [formats/CHART_FORMAT.md](../formats/CHART_FORMAT.md) | Chart v1/v2/v3 生产格式 |
| 16 | [formats/CHART_V4_FORMAT.md](../formats/CHART_V4_FORMAT.md) | Chart v4 接受合同 |
| 17 | [formats/CXT_FORMAT.md](../formats/CXT_FORMAT.md) | CXT v1 声明式模板 JSON |
| 18 | [formats/CXC_FORMAT.md](../formats/CXC_FORMAT.md) | CXC v1 容器、manifest 与闭包 |
| 19 | [formats/PORTABLE_PRESENTATION.md](../formats/PORTABLE_PRESENTATION.md) | 可移植表现资源 |
| 20 | [formats/ANIMATION_MIXING.md](../formats/ANIMATION_MIXING.md) | 动画层、Override 与混合 |
| 21 | [formats/MATERIAL_SHADER.md](../formats/MATERIAL_SHADER.md) | Material/Shader v1 字段合同 |

候选（**读时必须记住未发行**）：[formats/CXT_V2_FORMAT.md](../formats/CXT_V2_FORMAT.md)、
[formats/PACKED_CHART_FORMAT.md](../formats/PACKED_CHART_FORMAT.md)、
[formats/CHART_ENTRY_V1_FORMAT.md](../formats/CHART_ENTRY_V1_FORMAT.md)、
[formats/STAGE6_CONFIG_AND_MEDIA.md](../formats/STAGE6_CONFIG_AND_MEDIA.md)。

Gameplay I 收敛工作稿另见：[Gameplay Judgement Spec](../formats/GAMEPLAY_JUDGEMENT_SPEC.md)、
[Gameplay Judgement ABI](../api/GAMEPLAY_JUDGEMENT_ABI.md) 和
[ADR 0043](../adr/0043-gameplay-judgement-ruleset-convergence.md)。三者仍是 candidate，未实施，
待 owner acceptance；研究推导和压测仍从 [proposals/README.md](../proposals/README.md) 进入。

相关 ADR：[0038](../adr/0038-cxc-v1-and-chart-v4-boundary.md) CXC/Chart v4/CXT 边界、
[0018](../adr/0018-timing-map-semantics.md) TimingMap 语义、
[0041](../adr/0041-legacy-format-exit-policy.md) Legacy 格式退出政策。

写谱面的人先读 [Chart v4 谱面编写指南](CHART_V4_AUTHORING.md)。

## 7. 阶段计划与未来范围

| 顺序 | 文档 | 回答什么问题 |
| --- | --- | --- |
| 22 | [stage_plans/README.md](../stage_plans/README.md) | 计划索引、`active`/`future`/`deferred`/`reviews` 目录约定 |
| 23 | [future/stage-07/plan.md](../stage_plans/future/stage-07/plan.md) | **Input / Judgement / Score / Replay 的唯一范围来源**：7A 最小内核、7B 高级能力、7C 策略与设备演进；I 收敛输入见 §1.1 |
| 24 | [future/stage-08/plan.md](../stage_plans/future/stage-08/plan.md) | Chart v5 / CXT v2 / Packed 正式发行与语义收敛 |
| 25 | [active/chart-format-update-for-v5/plan.md](../stage_plans/active/chart-format-update-for-v5/plan.md) | Chart v5 跨阶段总工作包 |

Stage 7 有两条容易忽略的性质：它是一条**持续能力线**而非一次性阶段（7A 冻结最小公共内核，
7B+ 在不破坏 7A 身份与生命周期的前提下持续增加能力，Stage 8 只依赖 7A）；以及其计划 §2
登记的**四项 Stage 6 未完成项**已被指定为 Stage 7A 的**关闭前置条件**。

后续阶段按需查 `future/stage-09`…`stage-14` 的 `plan.md`。

## 8. 公共 SDK 视角

| 顺序 | 文档 | 回答什么问题 |
| --- | --- | --- |
| 26 | [api/README.md](../api/README.md) | 已发布 Playback SDK 的入口、生命周期、线程、资源、帧观察、诊断与兼容边界 |

相关 ADR：[0030](../adr/0030-playback-preview-api-version-and-result.md) Preview API 与 Result、
[0032](../adr/0032-playback-clock-and-prepared-audio-transaction.md) Clock 与 Prepared Audio、
[0033](../adr/0033-cpp-shared-library-preview-boundary.md) C++ Shared Preview 边界。

## 9. 证据（最后读，按需读）

| 顺序 | 文档 | 回答什么问题 |
| --- | --- | --- |
| 27 | [stage_reports/README.md](../stage_reports/README.md) | 报告索引 |
| 28 | [stages/stage-06/completion.md](../stage_reports/stages/stage-06/completion.md) | 最近一个已关闭阶段的关闭报告与交接清单 |
| 29 | [reviews/stage-06-review-2026-09/2026-09-28-delivery-report.md](../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-delivery-report.md) | Stage 6 关闭后复核的交付证据与残留 |

**证据文档只在需要核对"某个结论凭什么成立"时读**，不要用它建立对项目现状的第一印象。

## 10. 延期与候选（不得当成现状）

| 顺序 | 文档 | 回答什么问题 |
| --- | --- | --- |
| 30 | [proposals/README.md](../proposals/README.md) | 候选合同与延期设计输入索引 |
| 31 | [proposals/deferred/README.md](../proposals/deferred/README.md) | 粒子、Android/移动端等延期方向 |
| 32 | [archive/README.md](../archive/README.md) | 历史格式、历史计划与过期审视材料 |

旧阶段计划（v6/v7/v8、Stage 9B、旧 Stage 10、旧 Stage 9A）在 `stage_plans/deferred/` 与
`stage_plans/future/stage-09a/`；它们是已并入新主阶段的历史输入，**不等于已实施**。

## 11. 三条最小路径

- **只想建立整体模型（6 份）**：`CURRENT_STATUS.md` → `ROADMAP.md` → `ADR 0027` →
  `PROJECT_GUIDE.md` → `architecture/GAMEPLAY_ABSTRACTION_MODEL.md` → `formats/README.md`。
- **面向 Input / Judgement / 玩法**：第 4 节全部 → `stage_plans/future/stage-07/plan.md`
  → `architecture/RUNTIME_SESSION.md` → `formats/TIMING_MODEL.md`
  → `formats/CHART_V4_FORMAT.md` → `api/README.md`。
- **面向格式 / 编译链**：`formats/README.md` → `TIMING_MODEL.md` → `CHART_FORMAT.md` →
  `CHART_V4_FORMAT.md` → `CXT_FORMAT.md` → `CXC_FORMAT.md` → `PORTABLE_PRESENTATION.md`
  → `stage_plans/active/chart-format-update-for-v5/plan.md`。

## 12. 最容易读错的地方

1. **不要从历史报告推断当前工作**。只信 `CURRENT_STATUS.md`。
2. **不要把 candidate 当 implemented**。Chart v5、CXT v2、Packed Chart、Stage 6 entry/config
   合同都是候选；默认发行基线仍是 Chart v4 / CXT v1 / CXC v1。
3. **不要假设下一阶段已启动**。阶段关闭**不等于**下一阶段启动，也不等于 PR、合并或发布授权。
4. **"脚本系统"在本仓库有特定含义**。工程上的"脚本"应优先理解为**声明式 Behavior / Event /
   Effect Schedule**（见 `GAMEPLAY_ABSTRACTION_MODEL.md` §4），不是通用 Script VM。
   任意运行时脚本、逐帧回调、未知字节码属于**明确排除项**。
5. **不要为延期能力预留接口**。文档不得为运行时脚本预留字段、extension、capability、字节码或
   隐式执行入口；重新启动该议题必须**先**建立独立 ADR、威胁模型、ABI、预算和阶段计划
   （见 `ROADMAP.md` 「脚本路线」）。
6. **判定的独立性是硬约束**。判定必须独立于渲染帧率、渲染后端、画面表现和 World Entity；
   Presentation、Behavior 和 Effect Schedule 可以消费判定结果，但不能反向改变同一事件的判定事实。
7. **不要给公共头引入非 ASCII**。安装后的公共头必须纯 ASCII，否则在非 UTF-8 代码页的消费方
   会编译失败（原因见 `AGENTS.md`）。
8. **状态词有固定含义**（见 `DOCUMENTATION_POLICY.md` 「状态字段」）：`active`/`candidate`/
   `future`/`deferred`/`completed`/`historical`/`superseded`/`archived`。看到 `future` 就意味着
   未实现。
9. **摘要不是权威**。本仓库刻意让每份文档只承担一个角色；引用结论时请引用所有者文档。

## 13. 术语速查

| 术语 | 含义 | 权威 |
| --- | --- | --- |
| Playback SDK | 可嵌入的播放 SDK，宿主接入面 | [ADR 0027](../adr/0027-playback-sdk-product-boundary.md) |
| Player / Studio | 两个独立应用；Studio 尚未接入构建 | [PROJECT_GUIDE.md](../PROJECT_GUIDE.md) |
| Chart v1–v4 | 谱面语义格式；v4 为当前接受版本 | [formats/README.md](../formats/README.md) |
| CXT | 作者层声明式模板 JSON（v1 生产、v2 候选） | [CXT_FORMAT.md](../formats/CXT_FORMAT.md) |
| Packed Chart | Chart v5 的物理编码（候选，未实现） | [PACKED_CHART_FORMAT.md](../formats/PACKED_CHART_FORMAT.md) |
| CXC | 自包含只读交换/部署包，容器版本 v1 | [CXC_FORMAT.md](../formats/CXC_FORMAT.md) |
| TimingMap | Beat 与事件到统一可采样时间轴的映射 | [TIMING_MODEL.md](../formats/TIMING_MODEL.md) |
| InputDomain / JudgementRequirement | 判定域与要求模型（Stage 7A 范围） | [stage-07 plan](../stage_plans/future/stage-07/plan.md) |
| Behavior / Effect Schedule | 受约束的声明式行为与效果层 | [GAMEPLAY_ABSTRACTION_MODEL.md](../architecture/GAMEPLAY_ABSTRACTION_MODEL.md) |
| FrameSnapshot | 唯一的公共帧输出 | [api/README.md](../api/README.md) |

## 14. 本文的维护规则

本文只维护**阅读顺序**和"某份文档回答什么问题"。任何合同、字段、状态结论发生变化时，
应修改其所有者文档（ADR / Spec / Plan / Report / `CURRENT_STATUS.md`），而不是在本文同步一份副本。
本仓库遵循"同一事实只能有一个权威拥有者"。
