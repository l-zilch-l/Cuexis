# Cuexis Documentation

状态：现行文档入口

更新日期：2026-08-30

本文档是 Cuexis 文档的导航页，不复制产品合同、阶段证据或完整字段定义。当前项目状态只
以 [CURRENT_STATUS.md](CURRENT_STATUS.md) 为准。

## 推荐阅读顺序

1. [当前状态](CURRENT_STATUS.md)
2. [项目指南](PROJECT_GUIDE.md)
3. [产品与模块边界](architecture/README.md)
4. [ADR 索引](adr/README.md)，先读 ADR 0027、0024、0025、0026、0037、0038 和 0040
5. [格式索引](formats/README.md)
6. [260830-followup 当前计划](stage_plans/active/260830-followup/plan.md)
7. [指南索引](guides/README.md)
8. [API 参考](api/README.md)
9. [构建与验证](guides/BUILDING.md)

## 文档权威关系

| 文档类型 | 唯一职责 | 当前入口 |
| --- | --- | --- |
| ADR | 记录重大决策的背景、取舍和接受状态 | [adr/README.md](adr/README.md) |
| Format/System Spec | 定义稳定或候选的数据和运行语义 | [formats/README.md](formats/README.md) |
| Stage Plan | 定义阶段目标、批次、门禁和交接 | [stage_plans/README.md](stage_plans/README.md) |
| Stage Report | 保存带日期的实施、审查和验证证据 | [stage_reports/README.md](stage_reports/README.md) |
| Proposal | 记录候选合同和延期设计输入 | [proposals/README.md](proposals/README.md) |
| Example | 提供评审和验证样例，不代表生产支持 | [examples/README.md](examples/README.md) |
| Guide/Policy | 说明构建、编码、依赖和版本操作规则 | [guides/README.md](guides/README.md) |
| API Reference | 说明已发布 SDK 的入口、生命周期、诊断和兼容边界 | [api/README.md](api/README.md) |
| Current Status | 唯一的当前阶段和实现状态摘要 | [CURRENT_STATUS.md](CURRENT_STATUS.md) |
| Archive | 历史格式、历史计划和过期审视材料 | [archive/README.md](archive/README.md) |

同一事实只能有一个权威拥有者。其他文档可以有一段摘要，但必须链接权威文档，不得复制
完整字段、完整验收矩阵或当前阶段结论。

## 当前产品与架构

- [项目指南](PROJECT_GUIDE.md)
- [架构总览](architecture/README.md)
- [模块边界](architecture/MODULE_BOUNDARIES.md)
- [RuntimeSession](architecture/RUNTIME_SESSION.md)
- [项目路线图](ROADMAP.md)
- [文档整理政策](DOCUMENTATION_POLICY.md)
- [Playback SDK API 参考](api/README.md)

## 格式和运行语义

- [格式索引](formats/README.md)
- [Chart v1/v2/v3 生产格式](formats/CHART_FORMAT.md)
- [Chart v4 接受合同](formats/CHART_V4_FORMAT.md)
- [CXC v1 接受合同](formats/CXC_FORMAT.md)
- [CXT v1 接受合同](formats/CXT_FORMAT.md)
- [TimingMap](formats/TIMING_MODEL.md)
- [Animation Mixing](formats/ANIMATION_MIXING.md)
- [Portable Presentation v1](formats/PORTABLE_PRESENTATION.md)
- [Material/Shader v1](formats/MATERIAL_SHADER.md)

## 工程指南和政策

- [指南索引](guides/README.md)
- [构建、安装和质量门禁](guides/BUILDING.md)
- [编码、错误和线程政策](guides/CODE_POLICY.md)
- [依赖政策](guides/DEPENDENCY_POLICY.md)
- [版本政策](guides/VERSIONING.md)

## 延期设计输入

这些文档描述方向或研究输入，不是当前生产 API 或格式合同：

- [延期设计索引](proposals/deferred/README.md)
- [移动端策略](proposals/deferred/MOBILE_STRATEGY.md)
- [粒子时间轴](proposals/deferred/PARTICLE_TIMELINE.md)
- [Shader 管线历史输入](proposals/deferred/SHADER_PIPELINE.md)（字段合同见
  [MATERIAL_SHADER.md](formats/MATERIAL_SHADER.md)）

运行时脚本和逐帧脚本回调已无限期延后，不属于当前或已排期阶段。该决定及其格式影响见
[ADR 0038](adr/0038-cxc-v1-and-chart-v4-boundary.md) 和
[CXT_FORMAT.md](formats/CXT_FORMAT.md)。

## 旧路径兼容入口

根目录只保留稳定入口、状态合同和集中迁移清单；格式、指南、架构和延期设计的旧逻辑路径见
[legacy-paths.md](legacy-paths.md)。该清单映射旧名称到 canonical 文档，但不承诺旧的逐文件 URL。

## 阶段、证据和示例

- [阶段计划索引](stage_plans/README.md)
- [260830-followup 当前计划](stage_plans/active/260830-followup/plan.md)
- [Stage Chart Format Update 计划](stage_plans/completed/chart-format-update/plan.md)
- [Stage 4 计划](stage_plans/completed/stage-04/plan.md)
- [Stage 5 计划](stage_plans/completed/stage-05/plan.md)
- [260829 Full Review 整改计划](stage_plans/reviews/full-review-2026-08/remediation-plan.md)
- [阶段报告索引](stage_reports/README.md)
- [Full Review 最终关闭](stage_reports/reviews/full-review-2026-08/2026-08-30-final.md)
- [候选示例索引](examples/README.md)
- [历史文档归档](archive/README.md)
