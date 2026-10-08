# Cuexis Roadmap

状态：现行路线图

更新日期：2026-10-02

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
  -> Stage 7A：Gameplay v2 最小 Input / Judgement / Score / Replay
  -> Stage 8：Chart v5 正式发行与语义收敛
  -> Stage 9：Presentation Foundation 与 Chart v6 / Model v1
  -> Stage 10：Studio 创作与发行工作流
  -> Stage 11：Chart v7 几何表现
  -> Stage 12：规模化、桌面性能、Android 与 Vulkan
  -> Stage 13：Chart v8、高级表现与确定性粒子
  -> Stage 14：稳定 ABI 与 Playback SDK v1
```

Stage 7B+ 是跨越 Stage 8 的持续能力线；Stage 13 是可选的高级表现线，Stage 14 不必
等待 Stage 13 的全部能力。运行时脚本、逐帧脚本回调和任意宿主字节码不在这条路线中。
**稳定 C ABI 的唯一归属阶段是 Stage 14**：Stage 7A/7B/8 只交付 C++ typed preview，稳定 C ABI 不在
Stage 12 落地（`AGENTS.md` 把稳定 C ABI 写作 Stage 12 的表述是孤例，由 owner 择时订正，见缺陷 `D-11`；
阶段号以 [Stage 14 计划](stage_plans/future/stage-14/plan.md) 与 [CURRENT_STATUS.md](CURRENT_STATUS.md) 为准）。

## 主实施路线

| 阶段 | 目标 | 前置条件 | 主要交付 |
| --- | --- | --- | --- |
| [Stage 7A](stage_plans/active/stage-07/plan-a.md) | 冻结 Gameplay v2 最小可玩闭环和 Judgement Kernel | Stage 6 交接边界；Gameplay v2 contract | Input、Judgement、Score、Replay、Tap/Hold/Release-tail、单一 capacity=1 exclusive resource、early/late bridge、Graph/Packed 双入口，以及四项 Stage 6 遗留收口 |
| [Stage 7B+](stage_plans/active/stage-07/plan-b.md) | 持续扩展高级输入与判定 | 7A 身份、生命周期和 capability 合同 | Slide、Flick、方向、连续轨迹、多指、校准和高级判定 |
| [Stage 8](stage_plans/future/stage-08/plan.md) | Chart v5 正式发行和语义收敛 | Stage 7A；不等待全部 7B+ | v5/Gameplay v2 Spec、CXT v2、Canonical Graph、Packed Chart、CXC Graph/Packed playback entries、迁移和默认 Writer |
| [Stage 9](stage_plans/future/stage-09/plan.md) | Presentation Foundation 与 Chart v6 | Chart v5 语义和资源边界 | Environment、静态 glTF、Model v1、内置网格和 submesh |
| [Stage 10](stage_plans/future/stage-10/plan.md) | Studio 创作与发行工作流 | Stage 9 的模型/表现边界 | 编辑、预览、编译、打包和资源闭包 |
| [Stage 11](stage_plans/future/stage-11/plan.md) | Chart v7 几何表现 | Chart v6 / Model v1 | 单轴 Bézier、line 和 `shader.json` 声明接口 |
| [Stage 12](stage_plans/future/stage-12/plan.md) | 规模化与目标平台 | 可测量的运行时和资源合同 | 桌面性能、40k 压测、Android 和 Vulkan |
| [Stage 13](stage_plans/future/stage-13/plan.md) | Chart v8 与高级表现 | Chart v7 能力和独立表现合同 | 双轴 Bézier、模型动画、内置后处理和确定性粒子 |
| [Stage 14](stage_plans/future/stage-14/plan.md) | 稳定 SDK v1 | 已接受的核心与平台边界 | 稳定 C ABI、公共生命周期和发布政策 |

阶段计划索引同时列出已完成计划、复核整改计划和历史输入；路线图不复制它们的关闭证据。

## 阶段依赖

```text
Stage 6 交接
  -> Stage 7A
      -> Stage 8
          -> Stage 9
              -> Stage 10
          -> Stage 11
              -> Stage 12
                  +--> Stage 13（可选高级表现线）
                  +--> Stage 14（稳定 ABI，不等待全部 Stage 13 能力）

Stage 7A
  -> gameplay.version = 2 / Canonical Gameplay Graph
  -> Tap/Hold/Release-tail + single exclusive resource
  -> Packed / CXC Graph dual playback entry
  -> early/late Presentation bridge + Snapshot/Replay
      -> Stage 7B+（持续能力线，可跨越 Stage 8 继续）
  -> Stage 8（只依赖 7A，不等待全部 7B+）
```

允许并行研究，但不得越过主链直接形成生产承诺：

- Stage 7A 的最小 Judgement 合同和实现准备。
- Stage 7B+ 的高级判定设计。
- Stage 8 发行格式的候选验证。
- Stage 12/13 的性能和高级表现研究。

研究、候选合同和阶段计划都不能单独授权公共 API、默认发行格式、发布或合并。

## 格式路线

```text
Chart v4 / CXT v1 / CXC v1
  -> 作为兼容和回退边界保留

Chart v5 Foundation
  -> CXT/Packed 候选、容量测量和 CXC entry 语义

Stage 7A
  -> 冻结 Chart v5 所需的 Gameplay v2 / Canonical Graph / Judgement / Input / Replay 合同

Stage 8
  -> 正式发行 Chart v5 / Gameplay v2 / CXT v2 / Canonical Graph / Packed Chart

Stage 9+
  -> 在既有判定语义之外扩展 Model、Geometry 和 Presentation
```

Chart v5 必须同时处理判定要求、Gameplay v2 phases/resources、有限 Behavior / Effect Schedule、
CXT 模板和参数、Packed 物理编码、分 entry 容量门禁以及 CXC 中的 Graph/Packed playback entry。作者 JSON、CXT source 和原始
导入资源只能作为 Source Project 或显式 source entry，不能冒充 v5 发行入口。

Chart v6、v7、v8 已纳入连续主路线，不再作为脱离阶段路线的独立计划：

| 能力 | 归属 |
| --- | --- |
| CXT 模板、Pattern、Packed Chart | Stage 8 / Chart v5 |
| Environment、天空盒、静态 glTF、Model、Mesh、submesh | Stage 9 |
| 曲线形变、line 表现和 `shader.json` 接口 | Stage 11 |
| 桌面性能、40k 压测、Android、Vulkan | Stage 12 |
| 双轴形变、后处理、模型动画、粒子 | Stage 13 |
| 对上述能力的编辑和打包 | Stage 10 |

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
