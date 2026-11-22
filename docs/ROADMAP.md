# Cuexis Roadmap

<!-- cuexis-meta
id: GUIDE-ROADMAP
status: active
authority: owns
-->

状态：现行路线图

更新日期：2026-11-23

产品边界由 [ADR 0027](adr/0027-playback-sdk-product-boundary.md) 冻结。
本文只维护阶段顺序、依赖关系、格式演进和交接规则，不记录当前实现状态、逐批次证据或
完整字段合同。当前状态唯一以 [CURRENT_STATUS.md](CURRENT_STATUS.md) 为准；阶段目标、
批次和门禁以对应 [Stage Plan](stage_plans/README.md) 为准。

## 路线原则

Cuexis 的路线围绕两个相互约束的目标展开：

- 先建立可玩的 Input / Judgement / Score / Replay 闭环，再扩大格式和表现能力。
- 让 Chart、CXT、Packed Chart、CXC 和 Presentation 按版本化合同演进，不把候选格式
  误当成生产格式。

总路线如下：

```text
Playback SDK 基线与 Chart v4 兼容边界
  -> Stage 7（内核、诊断、RPA／PD 及统一收尾）
      Stage 7A：既有Gameplay内核与7/8集成
        -> 诊断码正式化独立前置N0–N3（不新增阶段编号）
        -> Stage 7-RPA：实时架构、设备验收及原S7A-9统一收尾
  -> Stage 7-RR：Review and Refactoring After Stage-7（架构减重、完整结构清理与 Gameplay v3 实验验证）
  -> Stage 7.5：Gameplay Evolution（独立能力演进、局部重构与性能稳定）
  -> Stage 8：Chart v5正式发行与语义收敛
          -> Stage 9：Presentation Foundation 与 Chart v6 / Model v1
          -> Stage 10：Studio 创作与发行工作流
          -> Stage 11：Chart v7 几何表现
          -> Stage 12：规模化、桌面性能、Android 与 Vulkan
          -> Stage 13：Chart v8、高级表现与确定性粒子
          -> Stage 14：稳定 ABI 与 Playback SDK v1
```

Stage 7.5当前计划范围须在Stage 8前完成并接受；Stage 13 是可选的高级表现线，Stage 14 不必
等待 Stage 13 的全部能力。运行时脚本、逐帧脚本回调和任意宿主字节码不在这条路线中。
**稳定 C ABI 的唯一归属阶段是 Stage 14**：Stage 7A／7.5／8 只交付 C++ typed preview，稳定 C ABI 不在
Stage 12 落地（`AGENTS.md` 的 Stage 12 表述已于 2026-11-17 随 AGENTS 精简删除；
阶段号以 [Stage 14 计划](stage_plans/future/stage-14/plan.md) 与 [CURRENT_STATUS.md](CURRENT_STATUS.md) 为准）。

## 主实施路线

| 阶段 | 目标 | 前置条件 | 主要交付 |
| --- | --- | --- | --- |
| [Stage 7A](stage_plans/completed/stage-07/01-a/plan.md) | 冻结 Gameplay v2 最小可玩闭环和 Judgement Kernel | Stage 6 交接边界；Gameplay v2 contract | Input、Judgement、Score、Replay、Tap/Hold/Release-tail、单一 capacity=1 exclusive resource、early/late bridge、Graph/Packed 双入口，以及四项 Stage 6 遗留收口 |
| [Stage 7-RPA](stage_plans/completed/stage-07/03-rpa-legacy/plan.md) | 实时架构及原S7A-9统一收尾 | S7A-7/8受限基线；独立诊断前置N3退出先于新原型/A4–A6；首用合同裁定 | 时间/输入/音频/判定/表现解耦、资源与快照协议、设备与平台验收 |
| [Stage 7-RR](stage_plans/active/stage-07-rr/plan.md) | Stage 7 后复核与重构：把复核发现收敛为可复核的处置 | Stage 7 受限关闭交接与 owner 接受；复核基线 `aac83e5e` | 合同与正确性修正、单一会话与共享时间、Chart/应用/公共类型结构清理、v3 成品与 Player 实验闭环（详见[发现登记册](stage_plans/active/stage-07-rr/findings.md)） |
| [Stage 7.5：Gameplay Evolution](stage_plans/active/stage-07-5/plan.md) | 高级输入／判定、局部重构与性能稳定的独立验收 | Stage 7-RR 当前范围完成并接受；继承 7A 兼容合同与 RR 实际交接 | Slide、Flick、方向、连续轨迹、多指、校准、高级判定及复制／保活成本整改 |
| [Stage 8](stage_plans/future/stage-08/plan.md) | Chart v5 正式发行和语义收敛 | Stage 7 受限交接后，Stage 7-RR 与 Stage 7.5 依次完成并接受 | v5/Gameplay v3 Spec、CXT v2、Canonical Graph、Packed Chart、CXC Graph/Packed playback entries、迁移和默认 Writer |
| [Stage 9](stage_plans/future/stage-09/plan.md) | Presentation Foundation 与 Chart v6 | Chart v5 语义和资源边界 | Environment、静态 glTF、Model v1、内置网格和 submesh |
| [Stage 10](stage_plans/future/stage-10/plan.md) | Studio 创作与发行工作流 | Stage 9 的模型/表现边界 | 编辑、预览、编译、打包和资源闭包 |
| [Stage 11](stage_plans/future/stage-11/plan.md) | Chart v7 几何表现 | Chart v6 / Model v1 | 单轴 Bézier、line 和 `shader.json` 声明接口 |
| [Stage 12](stage_plans/future/stage-12/plan.md) | 规模化与目标平台 | 可测量的运行时和资源合同 | 桌面性能、40k 压测、Android 和 Vulkan |
| [Stage 13](stage_plans/future/stage-13/plan.md) | Chart v8 与高级表现 | Chart v7 能力和独立表现合同 | 双轴 Bézier、模型动画、内置后处理和确定性粒子 |
| [Stage 14](stage_plans/future/stage-14/plan.md) | 稳定 SDK v1 | 已接受的核心与平台边界 | 稳定 C ABI、公共生命周期和发布政策 |

Stage 7.5 的小数编号仅表达顺序，其计划与报告目录均与 Stage 7 同级；旧 7B+ 名称及 S7B/S7C ID 只作追溯。
Stage 7-RR 的 `RR` 后缀同样只表达路线位置（Stage 7 之后、Stage 7.5 之前），不是 Stage 7 的子阶段，
也不改变 Stage 8–14 编号。
阶段计划索引同时列出已完成计划、复核整改计划和历史输入；路线图不复制它们的关闭证据。

## 阶段依赖

```text
Stage 6 交接
  -> Stage 7A既有成果
  -> 诊断码正式化独立前置N0–N3
  -> Stage 7-RPA（实时架构/设备及原S7A-9收尾）
  -> Stage 7-RR（架构与结构清理、v3 实验闭环）
  -> Stage 7.5（当前计划范围逐项验收、关闭交接）
  -> Stage 8（发行矩阵与正式格式收敛）
            -> Stage 9 -> Stage 10
            -> Stage 11 -> Stage 12
                             +-> Stage 13（可选）
                             +-> Stage 14（稳定ABI，不等待全部Stage13）
```

Stage 7-RPA属于Stage 7，基于7A既有内核和7/8受限集成基线实施。诊断码审计及正式命名
由[独立前置N0–N3](stage_plans/completed/stage-07/02-diagnostics/plan.md)管理，
不再属于RPA内部工作线，其N3在新架构原型及A4–A6前退出；原S7A-9全部剩余矩阵、预算、交接和关闭
归RPA-F1–F4，不再安排RPA结束后返回7A。实体设备与实时同步的整改和最终验收也归RPA。
Stage 7 受限关闭交接获接受后，先由独立 Stage 7-RR 完成其当前范围的架构改造、完整结构清理与 v3 实验验证，
再由 Stage 7.5 在该交接上扩展高级输入能力并处理剩余及新增的性能成本；
Stage 7.5 范围完成并接受后进入 Stage 8。
发行矩阵决定正式选入哪些能力，不替代Stage 7.5 阶段验收。
范围迁移不将旧设备失败标通过；它们作为RPA待办和基线证据保留。

允许并行研究，但不得越过主链直接形成生产承诺：

- Stage 7A 的最小 Judgement 合同和实现准备。
- Stage 7.5 的高级判定设计。
- Stage 8 发行格式的候选验证。
- Stage 12/13 的性能和高级表现研究。

研究、候选合同和阶段计划都不能单独授权公共 API、默认发行格式、发布或合并。

## RR 成果与后续阶段的衔接

后续阶段以 [RR 当前计划](stage_plans/active/stage-07-rr/plan.md)及其实际验收交接为输入。
已接受的范围裁定见[登记册 §24](stage_plans/active/stage-07-rr/findings.md#24-统一实施前设计裁定2026-11-23)；
设计接受不代表实现或发布已经完成。

| 后续阶段 | 消费 RR 成果后的职责 |
| --- | --- |
| Stage 7.5 | 复用会话、时间、版本派发及结构清理成果；新增高级输入能力，核对剩余复制/保活成本并完成新增负载的性能验收 |
| Stage 8 | 以 Chart v5＋Gameplay v3 为正式主线，复用 RR 源导入与成品链路，完成默认 Writer、正式编码与发行门禁；保留已承诺旧内容兼容 |
| Stage 9/11/13 | 将新表现域接入已有求值、时间和帧提交边界；只扩展该阶段的模型、几何或粒子能力 |
| Stage 10 | Studio 接入已有预览、离线编译和 Writer；编辑器承担损失提示与覆盖决定 |
| Stage 12 | 在既有模块和会话边界上做性能及平台适配；各平台 Player 自行实现设备等待超时 |
| Stage 14 | 消费公共类型分类、替代 API 与消费者证据，按实际发布过渡收敛稳定 ABI 和 SDK v1 |

完整 Chart 构建隔离、资源加载去重、公共类型分类与可兼容清理、应用分层均由 RR 负责实施并验收，
后续阶段承接实际交付并验证新增依赖，不再将这些整体安排为未来首次清理。
2026-11-23 owner 续议已将 Stage 8 正式目标改为 Gameplay v3，替代先前的 v2 正式目标。
RR 实验验收仍不等于正式发行或默认 SDK 晋升；
旧公共 API 的实际删除仍逐组遵守过渡发布和消费者验证条件。后续阶段绑定自己的实现、内容和平台证据，
不能把 RR 实验结果直接写成正式发行或新平台认证。

## 格式路线

```text
Chart v4 / CXT v1 / CXC v1
  -> 作为兼容和回退边界保留

Chart v5 Foundation
  -> CXT/Packed 候选、容量测量和 CXC entry 语义

Stage 7A
  -> 冻结 Chart v5 所需的 Gameplay v2 / Canonical Graph / Judgement / Input / Replay 合同

Stage 7-RR
  -> Chart v5 / Gameplay v3 实验实现与验证，提前承担 CXT v2 Animation 导入及既有 Animator 绑定

Stage 8
  -> 复用已验收成果，正式发行 Chart v5 / Gameplay v3 / CXT v2 / Canonical Graph / Packed Chart

Stage 9+
  -> 在既有判定语义之外扩展 Model、Geometry 和 Presentation
```

Chart v5 的正式主线采用 Gameplay v3 的判定等级、计分与共享时间规则，并承接已选入的 phases/resources、有限 Behavior / Effect Schedule、
CXT 模板和参数、Packed 物理编码、分 entry 容量门禁以及 CXC 中的 Graph/Packed playback entry。作者 JSON、CXT source 和原始
导入资源只能作为 Source Project 或显式 source entry，不能冒充 v5 发行入口。

Chart v6、v7、v8 已纳入连续主路线，不再作为脱离阶段路线的独立计划：

| 能力 | 归属 |
| --- | --- |
| CXT 模板、Pattern、Packed Chart | RR 承担已纳入的实验实现；Stage 8 / Chart v5 负责正式格式、默认 Writer 与发行收敛 |
| Environment、天空盒、静态 glTF、Model、Mesh、submesh | Stage 9 |
| 曲线形变、line 表现和 `shader.json` 接口 | Stage 11 |
| 桌面性能、40k 压测、Android、Vulkan | Stage 12 |
| 双轴形变、后处理、模型动画、粒子 | Stage 13 |
| 对已交付能力的编辑和打包 | Stage 10 首版消费 Stage 8/9 已接受子集；后续能力按实际支持再扩展 |

Chart 的版本表示谱面数据与表现能力；Gameplay 版本表示判定、计分和玩法时间规则，两者独立演进。
本次选择将 v3 作为 Stage 8 的正式玩法目标，后续 Chart v6/v7/v8 不因此自动提升 Gameplay 版本或重算旧谱。
CXT 是作者模板语言，Packed/Graph 是编译成品，CXC 是发行包容器；它们也不随 Chart 版本同步改号。

| 阶段 | Chart 路线与更新内容 |
| --- | --- |
| Stage 7-RR / 7.5 | 在 Chart v5 候选上实现 Gameplay v3，随后扩展高级输入能力；实际验收后交给 Stage 8 |
| Stage 8：Chart v5 | 正式 Gameplay v3、作者判定等级与计分模式、共享拍位时间与 Stop、画幅声明；CXT2 模板与有限展开、Clip2/Event2/alpha、编译成品、默认 Writer 与显式迁移完成正式交付 |
| Stage 9：Chart v6 | 环境/天空盒/雾、静态 glTF 离线导入、Model v1、基础网格、默认平面及子网格材质槽 |
| Stage 10 | Studio 编辑、预览、保存、编译与打包；本阶段不单独提高 Chart 版本，首版只消费实际接受的能力 |
| Stage 11：Chart v7 | 单轴 Bézier 曲线形变、line 几何及粗细/棱柱表现、声明式 shader.json 接口；此阶段不执行用户 shader |
| Stage 12 | 桌面规模与性能、Android、Vulkan 评估/适配；不因平台变化单独提高 Chart 版本 |
| Stage 13：Chart v8（可选） | 双轴 Bézier 形变、模型基本动画预设、SDK 内置后处理及确定性粒子；扩展表现，不改变旧判定与计分 |
| Stage 14 | 稳定 C ABI 与 SDK v1；不单独提高 Chart 版本，也不等待 Stage 13 全部可选能力 |

以上是规划归属；v6/v7/v8 的首批目录、预算和正式支持范围在各阶段启动时细化。旧内容按其受支持版本直接读取和运行，
显式迁移另行核验，不把旧候选样例扩张成新的正式兼容保证。

旧格式计划仍保留为历史输入，集中见 [historical-inputs](stage_plans/historical-inputs/)；
它们不能被解释为独立阶段或已实施能力。

## CXC 路线

```text
Foundation
  -> entry 映射、Packed 验证原型和容量门禁

Stage 6 交接
  -> 验证 v5 candidate playback entry，同时保留 v4 compatibility entry

Stage 8
  -> 在 CXC v1 内正式发行已验证的 Chart v5 `packed-chart` 或独立 `gameplay-graph` playback entry；
     二者共享 Canonical Graph、prepared identity 和 Judgement kernel

Stage 9/10
  -> 扩展 portable Model / Environment 资源和 Studio 打包工作流

Stage 11/13
  -> 以版本化 capability 扩展几何和高级表现
```

CXC v1 不因 Chart v5 自动升级容器版本。容器版本、entry kind、resource closure、identity
和发行校验必须分别遵循其 ADR、Spec 与阶段计划。

## 脚本边界

当前路线只实现有固定类型、确定性采样、资源闭包和安全预算的声明式能力：

```text
Behavior Event
Animation
Step Track
Judgement Result Binding
有限 Effect Schedule
```

任意运行时脚本、逐帧回调、字节码、宿主函数调用和动态对象生成无限期延后。重新启动
脚本议题必须先建立独立 ADR、威胁模型、ABI、预算和阶段计划；任何现有格式或 Playback
文档都不为它们预留隐式执行入口。

## 阶段退出规则

每个阶段要同时具备：

```text
接受的合同
实现和 focused tests
失败与回滚路径
公共边界和架构检查
跨平台或目标平台证据
文档、状态和交接更新
owner acceptance
```

未满足退出条件时，只能使用 `candidate`、`future` 或 `deferred` 等状态词，不能称为生产支持。
退出证据归档在 [stage_reports/README.md](stage_reports/README.md)；路线图只保留依赖和入口。

## 历史输入与延期方向

- [Chart v6 历史输入](stage_plans/historical-inputs/chart-format-update-for-v6/plan.md)
- [Chart v7 历史输入](stage_plans/historical-inputs/chart-format-update-for-v7/plan.md)
- [Chart v8 历史输入](stage_plans/historical-inputs/chart-format-update-for-v8/plan.md)
- [桌面性能历史输入](stage_plans/historical-inputs/stage-09a/plan.md)
- [Android 历史输入](stage_plans/historical-inputs/stage-09b/plan.md)
- [Vulkan 历史输入](stage_plans/historical-inputs/stage-10/plan.md)
- [延期设计索引](proposals/deferred/README.md)

这些材料用于追溯设计来源或等待恢复条件，不改变主路线顺序，也不构成当前实现声明。

## 维护规则

- 当前实现状态只更新 [CURRENT_STATUS.md](CURRENT_STATUS.md)。
- 阶段目标、批次、依赖和门禁只更新对应 Stage Plan。
- 格式字段和运行语义只更新对应 Spec 或 ADR。
- 完成、审查和 hosted 结果只写入 Stage Report。
- 路线变化需要同步路线图、阶段索引、相关计划和当前状态，但不把状态快照复制回路线图。
- 文档移动后运行 `python -B tools/check_docs.py` 和 `git diff --check`。
