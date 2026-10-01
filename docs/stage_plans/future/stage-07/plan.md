# Stage 7 Implementation Plan: Gameplay Foundation and Judgement Evolution

状态：future；I 收敛文档基线已整理，产品实现未开始

更新日期：2026-09-05

归档来源：[旧版 PROJECT_GUIDE](../../../archive/PROJECT_GUIDE_LEGACY_2026-08-10.md)、
[SDK transition plan 快照](../../../archive/CUEXIS_SDK_TRANSITION_PLAN_2026-08-10.md) 和
原 Stage 11 Judgement 计划。

前置：[Chart Format Foundation](../../completed/chart-format-foundation/plan.md) 和
[Stage 6](../../completed/stage-06/plan.md)。主要输入为 Chart v5 Core/Packed candidate；Chart v4 /
CXT v1 / CXC v1 保留为兼容和回退输入。共同玩法模型见
[音乐游戏玩法抽象模型](../../../architecture/GAMEPLAY_ABSTRACTION_MODEL.md)。

Stage 7 不是一次性完成所有音游品类的阶段，而是一条持续的 Gameplay 能力线：Stage 7A
冻结最小公共内核，Stage 7B 及以后在不破坏 7A 公共身份和生命周期的前提下增加高级
Input/Judgement 能力。Stage 8 只依赖 Stage 7A，不等待全部 Stage 7B+ 完成。

## 1. 阶段目标

建立可被 Chart v5、Player、Headless consumer 和后续 Studio 共同消费的判定边界：

```text
InputEvent
  -> InputMapping
  -> JudgementRequirement
  -> Judgement
  -> JudgementResult
  -> Score / Combo / Statistics
  -> Player or host feedback
```

判定必须独立于渲染帧率、渲染后端、画面表现和 World Entity。Presentation、Behavior
和有限 Effect Schedule 可以消费结果，但不能反向改变同一事件的判定事实。

### 1.1 I 收敛输入（2026-10-01）

Gameplay 判定的设计基线已进入 I 收敛工作稿阶段，权威输入为 [ADR 0043](../../../adr/0043-gameplay-judgement-ruleset-convergence.md)、
[Gameplay Judgement Spec](../../../formats/GAMEPLAY_JUDGEMENT_SPEC.md) 和
[Gameplay Judgement ABI](../../../api/GAMEPLAY_JUDGEMENT_ABI.md)。它们统一了 L1-L4 分层、
Pattern/Measure/Fold、仲裁、prepared grace、四分量 identity、预算/快照和稳定拒绝路径。

这不代表 Stage 7A 已启动或完成：`engine/`、安装头、CMake、SDK API、Stage 7A 实现和
Chart v5 默认发行路径均未因 I 收敛工作稿而改变。owner acceptance、真实内容预算测量、
实现批次和 hosted 验证仍是后续门禁。

## 2. 分阶段范围

### Stage 7A：Minimum Input and Judgement Kernel

Stage 7A 是 Stage 8 的硬前置，冻结 Chart v5 Core 所需的最小接口和确定性行为：

```text
InputEvent
InputDomain
Action
InputMapping
JudgementRequirement
JudgementResult
Tap / Hold / Release
基础离散判定域
基础时间区间和容差
位置/通道匹配
Miss、重复输入和冲突输入
Score / Combo / Statistics
Replay identity 和最小预算
```

Stage 7A 必须只冻结跨品类稳定的概念，不把轨道、Note、屏幕坐标、键盘布局或某个
渲染后端写入公共 Judgement API。时间使用显式区间和绝对观测时间；位置可以是离散
判定位置、区域、输入通道或其他已注册 InputDomain。

#### Stage 7A 承接的 Stage 6 未完成项（2026-09-29 登记）

Stage 6 关闭并归档后，以下四项**没有任何阶段计划把它列入范围**：Stage 6 已关闭，Stage 8 计划
正文对四项零命中（只在主题名上以「已交付」措辞提到前两项），Stage 7A 此前也没有记录。经项目
所有者于 2026-09-29 指定，自本节起由 **Stage 7A 承接**。四项都是 Stage 6 冻结决策
（[ADR 0042](../../../adr/0042-stage-6-productization-boundaries.md)）的未完成部分，不是新需求：

| 项 | ADR 决策 | 未完成内容 | 现状证据（2026-09-29 实测） |
| --- | --- | --- | --- |
| 显式 candidate 与实验隔离 | S6-D01 | `Cuexis_ALLOW_EXPERIMENTAL` 无代码实现；candidate flavor 库名与安装元数据不存在；Player `--candidate-entry`（配 `--project`/`--cxc`）未实现；**没有任何 preset 或 CI 开启 `CUEXIS_ENABLE_CHART_V5_CANDIDATE`** | 该宏在非文档代码中只出现在两张禁止 token 表（`cmake/VerifyReferenceHost.cmake:396`、`tools/check_stage6_a2.py:252`）；根 `CMakeLists.txt:42` 默认 OFF；`git grep CHART_V5_CANDIDATE -- CMakePresets.json .github/` 零命中；复核已登记 SPEC-19b BLOCKED |
| 离线 typed assembler 与 feature 派生 | S6-D03 | 不存在派生 feature 与 resource closure 的 assembler；无 chart-candidate 离线工具入口；`CxtV2Loader::expand` 无生产调用方；candidate 正例仍是测试内注入 feature（ADR `:114` 明文禁止） | `git grep -in assembler -- engine/ tools/ tests/` 只命中 `assembleResourceIdentities`（资源身份装配，非 feature 派生） |
| 具名宿主六动词命令循环 | S6-D08 | `open`/`play`/`pause`/`seek`/`reload`/`quit` 未进入 `master` | `670cca8:examples/reference_host/src/` 无 `host_commands.*` 与 `host_clock.*`；实现仅存在于未合并的 R9 批次 |
| SDK API `0.7.1` | S6-D08（`:333`） | ADR 冻结的 Stage 6 SDK 目标未落地，且**被 Stage 6 自己建的版本门禁锁死**（见下） | `cmake/CuexisVersion.cmake:8` = `0.7.0`；同期确有 3 个 additive 公开工厂进入安装头 `engine/playback/include/cuexis/playback/playback_source.hpp:83-91` |

**`0.7.1` 的门禁阻塞（2026-09-29 实测）**：`tools/check_version_gate.py:213-218` 规定 SDK API 版本
一经变更即须 `--allow-sdk-api-change` 放行，否则报 `version.sdk_api.changed`；而该开关**没有任何工作流
传入**（`.github/workflows/version-gate.yml:74-83`、`:126`、`:175` 均未传），且 `:58-71` 把检查器**从
base commit 取出**再运行，故**候选分支无法自行开启它**。用工作流的默认参数实测：`0.7.0 → 0.7.1`
**失败**（`version.sdk_api.changed`），同版本对照**通过**。

因此 `0.7.1` **不是改一个数字即可落地**：必须先往 `master` 合入对 `version-gate.yml` 的修改，而这
改的是防篡改契约门禁本身——ADR 0042 `:321` 要求这类修改经**代码所有者复核**，本仓库**没有
CODEOWNERS**（SPEC-04 至今 BLOCKED），该复核无处可做。**技术上升版本本身是安全的**（`SameMinorVersion`
下 `0.7.0` 与 `0.7.1` 同 minor 兼容，与 ADR `:333` 的描述一致）：**阻塞来自门禁，不是兼容性。**
处置该阻塞需要一次条件化放行机制的设计（保持"每次变更都需一次刻意记录"的本意），属本阶段范围。

**这四项是 Stage 7A 的关闭前置条件**：Stage 7A 必须在关闭前**逐项处置**——或已实现并有门禁/用例
证据，或经 [ADR](../../../adr/README.md) 或所属 Spec 的变更**明示为被接受的例外**；任一项既未实现
又无例外登记，Stage 7A 不得关闭。判据见 §6「Stage 7A 关闭标准」的最后一条。

本节同时声明：登记为关闭前置**不代表它们已实现**，也不改变 Stage 6 已关闭的事实。前两项同时是
Stage 8 的输入（Stage 8 计划此前只以主题名提到、未列入范围）；后两项在本次登记前无任何阶段归属。

### Stage 7B：Advanced Judgement Capabilities

Stage 7B 及其后续批次负责增加更复杂的要求与输入约束：

```text
Slide / 连续轨迹
handoff Hold / 接触跟随型 Slider 的 continuity grace
Flick / 方向动作
多指和指间关系
复杂 Hold、分段持续和 Release 约束
方向、速度、压力、距离和轨迹约束
输入占用、优先级和冲突解析
```

每项能力都必须成为版本化的 `RequirementKind`、约束集合或 capability。高级能力可以
在 Stage 8 前后持续开发；只有明确纳入某次 v5 发行的能力才需要进入 Stage 8 的选定
发行矩阵。未纳入的能力必须稳定拒绝，不能静默降级为 Tap、Hold 或 v4 语义。

其中 continuity grace 的候选语义已经在 I 收敛工作稿中闭合：它复用 `handoff + Gap`，使用
prepare 时冻结的 per-requirement `grace`，不新增运行期 Hook、Fact 或 Pattern 原语。它仍是
Stage 7B+ capability，除非后续发行矩阵明确选择，否则不成为 Stage 7A 或 Stage 8 的隐含前置。

### Stage 7C：Judgement Policy and Device Evolution

后续批次可以增加不改变核心事件身份的策略能力：

```text
校准和设备延迟补偿
Timing offset / input latency policy
判定等级和窗口策略
设备能力协商
Replay 版本演进
更丰富的 Score / Life / Accuracy policy
```

这些策略必须通过显式、可复现的 `JudgementConfig`、`CalibrationProfile` 或 capability
表达，不读取上一帧隐式状态，不把宿主时钟或设备私有类型带入语义合同。

## 3. Stage 7A 工作批次

### S7A-A：统一输入模型

定义 typed `InputEvent`、`InputDomain`、`Action` 和 `InputMapping`：

```text
observationTime
inputDomain
action
position or channel
pressure / amount（可选）
source identity（可选）
sequence
```

输入事件时间必须与到达时间、渲染帧时间分离。键盘、鼠标、触摸、手柄和外部控制器
通过显式映射进入统一输入域。

### S7A-B：JudgementRequirement

建立不依赖渲染的要求模型：

```text
requirementId
timeInterval
judgementDomain
requiredAction
constraints
```

Tap 的区间、Hold 的开始/持续/结束和 Release 的边界必须有明确的半开区间规则。判定
不能读取像素、GPU 状态、渲染顺序或 World Entity。

### S7A-C：Judgement Engine

实现 Stage 7A 的最小求值：

```text
时间区间匹配
输入域匹配
动作匹配
Hold 持续和 Release
重复输入和冲突输入
判定优先级
Miss 结算
```

所有求值必须基于绝对时间和显式状态快照。状态快照用于表达已确认的生命周期，不能
把上一帧的最终结果当作未声明的语义基线。

### S7A-D：Score、Combo 和 Statistics

定义：

```text
JudgementOutcome
ScoreState
ComboState
LifeState（若启用）
StatisticsSnapshot
```

Score、Combo 和 Life 属于运行时会话状态，不回写 Chart、Behavior 或 World 持久化数据。

### S7A-E：Replay

Replay 至少记录：

```text
规范化 InputEvent
Chart semantic identity
Timing identity
InputMapping identity
JudgementConfig identity
```

实时输入和 Replay 输入必须进入同一 Judgement 路径。Replay 必须有事件数、字节数和
解码预算，并在 identity 不匹配时稳定失败。

### S7A-F：Player 与 Headless 闭环

Player 提供最小反馈：

```text
判定结果
连击
分数
Miss
基础按键/触摸反馈
```

Headless consumer 必须可以执行相同的 Judgement 和 Replay，不要求 GPU。

## 4. 7A 交接给 Stage 8 的合同

Stage 7A 关闭时必须交付：

```text
JudgementRequirement typed model
InputEvent typed model
InputMapping identity
JudgementResult lifecycle
Score/Combo result model
ReplayData identity and budget
Tap/Hold/Release boundary rules
Headless golden fixtures
基础错误、预算和 identity mismatch 诊断
```

Stage 8 可以在此合同上正式收敛 Chart v5；不得重新发明一套与 Stage 7A 不兼容的输入
或判定模型。Stage 7B+ 的能力若未进入 Stage 8 的发行矩阵，不得成为 Stage 8 的隐性
前置。

## 5. 7B+ 能力演进规则

- 7A 的 `InputEvent`、`JudgementResult`、Replay identity 和生命周期在兼容窗口内保持稳定。
- 新的 `RequirementKind`、约束字段和策略必须版本化，并有 capability 查询和稳定拒绝路径。
- 高级能力应提供 headless golden、确定性 replay、预算和迁移/拒绝测试。
- 高级能力可以在 Stage 8 关闭后继续交付；不能通过修改既有字段含义破坏已发行 v5。
- Presentation 只能订阅结果和 Effect Schedule，不能把提示线、Key Sound、轨道倾斜等
  表现需求伪装成 Judgement 结果；它们由脚本/表现声明合同另行消费。

## 6. 验收标准

### Stage 7A 关闭标准

- 相同 Chart、InputEvent、映射、Timing 和判定配置得到完全一致的结果。
- 不同渲染帧率、Entity 遍历顺序和渲染后端不改变判定结果。
- Seek、Pause、Reload 和 Audio discontinuity 后结果仍然确定。
- 实时输入与 Replay 的 JudgementResult、Score、Combo 和 Statistics 一致。
- 判定系统不暴露 World、EnTT、SDL、OpenGL 或宿主引擎类型。
- 无 InputEvent 的纯播放和 Studio Preview 中 Judgement 休眠且不改变 FrameSnapshot。
- 非法输入、时间回退、预算超限和 identity 不匹配均稳定失败。
- external consumer、static/shared package 和 headless 路径均通过生命周期测试。
- Chart v5 Core/Packed candidate 可以消费 Stage 7A 要求并获得相同的判定结果。
- §2 登记的「Stage 7A 承接的 Stage 6 未完成项」四项——显式 candidate 与实验隔离（含 `Cuexis_ALLOW_EXPERIMENTAL`、candidate flavor、Player `--candidate-entry`、以及某个被配置的 preset 或 CI 确实开启 candidate）、离线 typed assembler 与 feature 派生、具名宿主六动词命令循环、SDK API `0.7.1`——**逐项处置完毕**：或已实现并有门禁/用例证据，或经 ADR 或所属 Spec 的变更明示为被接受的例外。**任一项既未实现又无例外登记时，本阶段不得关闭。**

### Stage 7B+ 批次标准

- 每个新增能力有版本化语义、capability、拒绝路径和边界测试。
- 高级要求不改变 7A 已发行字段的含义，并能在没有 Presentation 的 headless 路径复现。
- 每个纳入发行的能力都有 deterministic golden、Replay identity 和预算证据。

## 7. 明确不包含

Stage 7A 不包含：

```text
完整滑动、复杂 Flick、多指、旋转和宿主扩展判定
Studio 编辑器
运行时脚本、通用状态机和宿主回调
```

Stage 7 全线仍不包含任意运行时脚本。高级 Judgement 能力属于版本化 Gameplay capability，
不是对 Chart/CXT/CXC 注入脚本执行权的理由。
