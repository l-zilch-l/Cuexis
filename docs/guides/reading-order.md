# Cuexis 阅读顺序

状态：current

更新日期：2026-10-01

本文给第一次接触 Cuexis 的 AI 代理、新成员和文档维护者一条可执行的阅读路线。
它只说明先读什么、每份文档回答什么问题、什么时候应停止下钻；合同、状态和证据仍由
各自的权威文档负责。

## 先记住四条规则

1. 当前状态只看 [CURRENT_STATUS.md](../CURRENT_STATUS.md)，不要从历史报告的“下一步”
   推断现在正在做什么。
2. 权威顺序是：当前状态页 → 已接受 ADR → 生产 Spec → 当前 Stage Plan →
   最新 Report → 历史材料。发生冲突时按此顺序处理。
3. ADR 负责决策，Spec 负责字段和语义，Plan 负责范围和门禁，Report 负责带日期的证据。
   阅读顺序不能替代这些文档的职责。
4. `candidate`、`future`、`deferred` 都不是已实现能力；阶段关闭也不自动授权下一阶段、
   PR、合并或发布。

## 主线：先建立整体模型

按下面的顺序阅读，通常足以建立正确的项目地图：

1. [当前状态](../CURRENT_STATUS.md)
   当前产品边界、已关闭阶段、当前阶段和明确的未完成项。
2. [项目指南](../PROJECT_GUIDE.md)
   产品目标、非目标、架构原则和完成标准。
3. [ADR 0027：Playback SDK 产品边界](../adr/0027-playback-sdk-product-boundary.md)
   SDK、Player、Studio 和宿主之间谁拥有哪一层。
4. [架构总览](../architecture/OVERVIEW.md)
   内容、时间、运行时、表现和宿主的整体流向。
5. [音乐游戏玩法抽象模型](../architecture/GAMEPLAY_ABSTRACTION_MODEL.md)
   `Timing -> Chart -> Input -> Judgement -> Behavior/Effect -> Presentation` 的共同语义。
6. [格式索引](../formats/README.md)
   哪些格式是生产合同，哪些仍是 candidate 或 future。
7. [API 参考](../api/README.md)
   宿主真正可以依赖的 Playback SDK 入口、生命周期、帧观察和诊断。
8. [阶段计划索引](../stage_plans/README.md)
   当前路线、阶段依赖、历史输入和复核工作包的位置。

读完主线后，应该能回答三件事：Cuexis 对外提供什么、当前实现到哪里、下一步计划在哪里。
如果这三件事还不清楚，先回到第 1、3、6 份，不要直接跳进历史报告。

## 按任务继续

### 做玩法、Input 或 Judgement

1. [玩法抽象模型](../architecture/GAMEPLAY_ABSTRACTION_MODEL.md)
2. [Gameplay Judgement Spec](../formats/GAMEPLAY_JUDGEMENT_SPEC.md)
3. [Gameplay Judgement ABI](../api/GAMEPLAY_JUDGEMENT_ABI.md)
4. [Stage 7 计划](../stage_plans/active/stage-07/plan.md)
5. [Gameplay 提案索引](../proposals/README.md) → [Gameplay research](../proposals/research/gameplay/README.md)

Gameplay Judgement Spec、ABI 和 ADR 0043 是 I 收敛后的 candidate 工作稿，仍待 owner
acceptance，不能写成已实施 API。研究稿用于追溯推导、压测和取舍，不是第二份规范。

### 做格式、谱面或打包链

1. [格式索引](../formats/README.md)
2. [TimingMap](../formats/TIMING_MODEL.md)
3. [Chart v1-v3](../formats/CHART_FORMAT.md)
4. [Chart v4](../formats/CHART_V4_FORMAT.md)
5. [CXT v1](../formats/CXT_FORMAT.md)
6. [CXC v1](../formats/CXC_FORMAT.md)
7. [Chart v5 跨阶段计划](../stage_plans/active/chart-format-update-for-v5/plan.md)
8. [Stage 8 计划](../stage_plans/future/stage-08/plan.md)

需要作者视角时，再读 [Chart v4 谱面编写指南](CHART_V4_AUTHORING.md)。
`CXT_V2_FORMAT.md`、`PACKED_CHART_FORMAT.md` 和 `CHART_ENTRY_V1_FORMAT.md` 仍是候选输入，
不能与 v1/v4 生产合同混读。

### 做 SDK、Player 或宿主集成

1. [API 参考](../api/README.md)
2. [模块边界](../architecture/MODULE_BOUNDARIES.md)
3. [RuntimeSession 内部生命周期](../architecture/RUNTIME_SESSION.md)
4. [Player 应用结构](../architecture/PLAYER_APPLICATION.md)
5. [构建与验证指南](BUILDING.md)
6. [Stage 6 关闭报告](../stage_reports/stages/stage-06/completion.md)
7. 需要核对具体结论时，再进入 [阶段报告索引](../stage_reports/README.md)

宿主只依赖 Playback 公共边界。SDL3、OpenGL、EnTT、World、RuntimeSession 和 JSON DOM
属于内部或可选实现细节；修改代码前还必须阅读仓库根部的 `AGENTS.md`。

## 架构细读顺序

当主线不够用时，按下面的层次下钻，避免从实现细节倒推产品合同：

| 层次 | 入口 | 重点 |
| --- | --- | --- |
| 产品 | [ADR 0027](../adr/0027-playback-sdk-product-boundary.md) | SDK / Player / Studio / 宿主边界 |
| 语义 | [玩法抽象模型](../architecture/GAMEPLAY_ABSTRACTION_MODEL.md) | 时间区间、判定域、动作和效果 |
| 模块 | [模块边界](../architecture/MODULE_BOUNDARIES.md) | 依赖方向和禁止依赖 |
| 运行时 | [RuntimeSession](../architecture/RUNTIME_SESSION.md) | 内部事务、资源和帧生命周期 |
| 格式 | [格式索引](../formats/README.md) | 生产、候选和延期格式 |
| 公共 API | [API 参考](../api/README.md) | 宿主可观察入口和兼容边界 |
| 计划 | [阶段计划索引](../stage_plans/README.md) | 范围、门禁、交接和恢复条件 |
| 证据 | [阶段报告索引](../stage_reports/README.md) | 日期化实现与验证记录 |

## 文档治理与维护

修改或移动文档前，先读 [DOCUMENTATION_POLICY.md](../DOCUMENTATION_POLICY.md)。
它规定角色、状态词、目录、旧路径映射、脚本延期边界和自动检查。

当前目录结构的几个容易混淆点：

- `stage_plans/active/`：跨阶段或当前执行中的计划。
- `stage_plans/future/`：已规划但尚未实施的主阶段。
- `stage_plans/deferred/`：尚未排期、等待触发条件的输入。
- `stage_plans/historical-inputs/`：已并入主路线、仅供追溯的旧计划。
- `stage_plans/reviews/`：复核整改计划，即使已完成也保留在复核主题下。
- `proposals/research/`：研究推导和压测。
- `proposals/implementation-input/`：仍被工具或历史实施报告引用的候选输入。
- `proposals/deferred/`：延期设计输入；它们不是当前生产 API。
- `stage_reports/`：带日期的实施、审查和验证证据，不负责重新定义当前状态。

新增或移动 Markdown 后运行：

```powershell
python -B tools/check_docs.py
python -B tools/check_docs_status_contract_tests.py
git diff --check
```

## 证据与历史材料

只有在需要回答“这个结论凭什么成立”时，才进入报告：

- [Stage 6 关闭报告](../stage_reports/stages/stage-06/completion.md)
- [Stage 6 复核修正交付报告](../stage_reports/reviews/stage-06-review-2026-09/2026-09-28-delivery-report.md)
- [Gameplay I 收敛报告](../stage_reports/reviews/gameplay-ruleset-2026-10/2026-10-01-i-convergence.md)

历史报告保留当时的日期、环境和未完成判断；它们不是当前状态页的替代品。
已并入新路线的旧阶段计划见 [historical-inputs](../stage_plans/historical-inputs/)；
旧路径映射见 [stage plan legacy paths](../stage_plans/legacy-paths.md) 和
[stage report legacy paths](../stage_reports/legacy-paths.md)。

## 最小阅读路径

- **整体理解**：`CURRENT_STATUS.md` → `PROJECT_GUIDE.md` → `ADR 0027` →
  `architecture/OVERVIEW.md` → `formats/README.md` → `api/README.md`
- **玩法实现**：玩法抽象模型 → Gameplay Judgement Spec → Stage 7 计划 →
  Gameplay research
- **格式实现**：格式索引 → TimingMap → Chart v4 → CXT v1 → CXC v1 →
  Stage 8 计划
- **SDK 集成**：API 参考 → 模块边界 → Player 应用结构 → 构建指南 →
  Stage 6 关闭报告

## 最容易读错的地方

1. 历史报告中的“下一步”不代表当前路线。
2. `future` 和 `deferred` 不代表已实现；`candidate` 不代表已发布。
3. Stage 7A 进入 active 不等于产品实现已经开始，也不等于发布授权。
4. “脚本”在当前设计中主要指受约束的 Behavior / Event / Effect Schedule，不是通用
   Script VM；运行时脚本和逐帧回调已无限期延期。
5. 判定必须独立于渲染帧率、渲染后端、Presentation 和 World Entity。
6. 安装后的公共头必须保持纯 ASCII；否则非 UTF-8 代码页的外部消费者可能无法编译。
7. 摘要文档只负责导航；字段、决策、范围和证据必须回到各自的权威文档。

## 维护边界

本文只维护阅读顺序、入口和解释性提示。产品合同、字段、当前状态、阶段范围和验证结论
发生变化时，应修改其权威文档，并只在这里调整链接或阅读提示。
