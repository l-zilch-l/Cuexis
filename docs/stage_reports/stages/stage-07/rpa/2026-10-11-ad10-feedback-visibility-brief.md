# AD-10 判定说明：首见反馈保证

状态：historical decision brief；保存早期选项依据，后续已选方向，未实施

日期：2026-10-11（Asia/Shanghai）

本文把 [生产架构方案](2026-10-11-production-architecture-design.md) §5 的「AD-10 首见反馈」
展开为可裁定的说明。**只做只读核对与选项界定，不改任何代码或合同。**

与前两份判定说明不同：AD-10 **不依赖测量**，它是纯产品取舍，可以现在裁定。

**后续决定（2026-10-11）：** owner授权剩余普通方案自行收敛；最终选择和测量gate归
[RPA计划](../../../../stage_plans/active/stage-07/plan-rpa.md)及
[RPA补充Spec](../../../../formats/rpa-execution-contract.md)。下文选项为当时比较输入，不再等待逐项点选。

## 0. 结论摘要

| 问题 | 核对结果 |
| --- | --- |
| 反馈未上屏是否可能？ | **是**。窗口期内若没有任何成功 present，该反馈从未显示 |
| 「合并」发生在哪里？ | 不在会话内。帧循环每轮提取快照并提交，无跨帧队列 |
| 现行能否知道 present 是否成功？ | **不能**。`present()` 由 Player 帧循环调用，Session 从不获知结果 |
| `present()` 能证明什么？ | 只能证明 `SDL_GL_SwapWindow` 成功——**不证明物理扫描输出或人眼可见** |
| 需要新公开 API 吗？ | F1 不需要；F2/F3 **都需要一条 present 回执通道** |

**推荐：F1 + F3-可观测（见 §4）——不承诺保证，但让"未上屏"可计数可报告。**
理由：F2 承诺的性质无法完整证明；而 F3 与 F2 的成本下限相同，F3 只取其中不需承诺的部分。

## 1. 只读核对的现行事实

### 1.1 反馈的可见性如何表达

| 环节 | 机制 |
| --- | --- |
| 判定事实 | `PhaseOutcomeFact` |
| 绑定 | `GameplayFactBinding{ phase, outcome, timing, target, visible, start, end, aggregation, groupMembers }` |
| 发布 | `publishGameplayProjection`：`active[b.target] = b.visible`，转为 `world::PropertyId::RenderVisible` 写入 |
| 表现 | `RuntimeSession::updateGameplay` → resolver 的 `GameplayOverride` 层 |
| 帧 | `extractFrame` 读回 `AppearanceComponent::visible` → `FrameSnapshot::ObjectSnapshot::visible` |
| 设备 | `renderer.submit(snapshot, ...)` → `renderer.present()` |

**有效期语义：** `erase_if` 在 `end && end->value <= t` 时丢弃该 scheduled 条目，
于是不再写 `RenderVisible`，可见性回退。因此反馈的可见期是 **`[start, end)` 在表现 T 上**。
`end` 缺席表示 `UntilReset`（永不到期）。

### 1.2 「合并」不在会话内

`extractFrame` 每轮重建快照：`snapshot.objects.resize(...)` 后逐项覆写，**没有跨帧队列**。
`RemainingFrames` 是 override 寿命计数，在 `updatePrepared` 中按 **Runtime 更新**递减
（`tickOverrideLifetimes`），**不是** GPU 呈现次数。

**推论：** 所谓"合并"是**帧循环的轮次跳过**，不是会话内的缓冲策略。
在 `player_control.cpp` 里：`submitted` 返回 `presentation.renderer.surface.zero_size`
时循环直接 `continue` —— **不提交、不 present、帧号不增加**。

### 1.3 可能整窗错过反馈的情形

| 情形 | 机制 | 用户是否在看 |
| --- | --- | --- |
| 表面尺寸为零 | `zero_size` → `continue`，跳过 present | 否（窗口最小化） |
| 窗口被遮挡/最小化 | 平台可能仍 present，或 drawable 为零 | 部分 |
| 长卡顿超过窗口 | 期间无成功 present | **是** |
| present 失败 / `markFailed` | 进入 `Failed` | 是 |
| Seek/scope 切换落在窗口内 | `projection` 整体替换，旧 scheduled 丢弃 | 是 |
| 窗口短于一帧 | `end - start` 小于帧间隔 | 是 |

### 1.4 关键约束：Session 不知道 present 结果

`IPresentationRenderer::present()` 由**帧循环**调用，`PlaybackSession` 从不接收其结果。
因此**任何**"按是否成功上屏来处置反馈"的方案——无论是保留、合并还是只计数——
都需要先建立一条 **present 回执通道**（设备提交域 → 表现/会话域）。

这是 AD-10 的真实成本下限。**F2 与 F3 在这一项上成本相同**，差别只在拿到回执后做什么。

### 1.5 `present()` 能证明的阶段

`OpenGlBackend::present()` 做的是：检查 owner 与 `frameSubmitted_`，
`SDL_GL_MakeCurrent` + **`SDL_GL_SwapWindow`**。这是它最强可证明的阶段。

它**不证明**：物理扫描输出、垂直同步实际发生、窗口是否可见、用户是否看见。
设备丢失（`deviceLost_`）与失败各有独立终态，不伪报为已呈现。

## 2. 问题的最小真实形态

> 一条 `visible=true` 的绑定在表现 T 的 `[start, end)` 内可见。
> 若该区间内**没有任何**成功 present，用户从未看到它。
> 今天系统既无法阻止这件事，也无法报告它发生了。

## 3. 明确排除

| 排除项 | 理由 |
| --- | --- |
| 把"用户看见了"作为可验证承诺 | 无法证明（§1.5）；写进合同会让每次验收都要为不可证明的性质辩护 |
| 把 `RemainingFrames` 改成 GPU 呈现次数 | 改变既有表现语义；与 IN-8 冲突 |
| 在会话内建跨帧反馈队列 | 与现行"每轮重建快照"结构冲突；会把表现状态重新拉回会话 |
| 用渲染器常数实现"最大年龄" | profile 拥有阈值，不藏在渲染器里 |
| 让反馈可见性与判定结果耦合 | Hit/Miss 已封闭；可见性是表现处置，不得回写判定 |

## 4. 三个可裁定方案

| 方案 | 内容 | 新契约 | 取舍 |
| --- | --- | --- | --- |
| **F1** | 维持现状：反馈的显示机会是"尽力"，不作为保证 | 无 | 零成本；但"未上屏"不可观测，无法据此改进 |
| F2 | 完整呈现义务：帧 ID + 反馈 ID 的 present 回执、到期、取消、最大年龄、合并时保留未确认反馈 | 需要 present 回执通道 + 公共义务类型 + 状态机 | 承诺了无法完整证明的性质；状态最多 |
| **F3-可观测 推荐** | **不承诺保证**，但建立 present 回执通道并**计数/报告**"窗口到期而未确认上屏"的反馈 | 需要 present 回执通道（内部即可） | 与 F2 成本下限相同，但只取可证明的部分 |

**推荐 F1 + F3-可观测**，理由：

1. **F2 承诺的东西无法证明。** §1.5 说明 `present()` 最强只到 `SDL_GL_SwapWindow` 成功。
   把它包装成"保证用户看见"是把不可验证的断言写进产品契约。
2. **F3 不承诺，但让问题可测量。** 它把"反馈可能没上屏"从**不可见的行为**变成
   **可报告的计数**。这与本项目"先有证据再承诺"的一贯口径一致，也给出将来重新裁定的数据路径。
3. **若将来产品确实需要保证**，F3 已经建好回执通道，F2 是它的自然扩展，不是重做。
4. **F1 单独保留太消极**：零成本，但也意味着这个问题永远无法被证伪或量化。

**反向情形（应选 F2）：** 如果产品明确要求"每次命中反馈必须在屏幕上出现过至少一次"，
并接受该保证只能声明为"已成功提交给显示系统"，那么应选 F2。这是一项真实的产品取舍，
本文不替 owner 决定，只说明它的代价与可证明边界。

## 5. 边界条件（若选 F3-可观测）

1. **回执通道只传可证明阶段。** 回执含义是"一次 present 调用成功"，不是"用户看见"。
   状态命名须与此一致，不得用 `Seen`、`Displayed` 一类词。
2. **回执不得进入判定身份。** 它不影响 Hit/Miss、Replay 或 archive；「完整 Replay 不依赖
   实时设备」这一要求保持——Replay 复现判定结果，不试图复现"当时是否上屏"。
3. **计数是诊断，不是保证。** 对外报告的语义是"有多少反馈窗口在未经确认的情况下到期"，
   不是"保证为零"。
4. **`RemainingFrames` 语义不变**（仍按 Runtime 更新递减）。
5. **不因回执缺失而阻塞。** 回执通道不得成为渲染同步点；缺失只记未知，不推断为已呈现。
6. **Seek/scope 切换取消旧窗口**，并按取消而非过期计数，两者分开。

## 6. 本裁定的准入效果

| 若选 | 对批次的影响 |
| --- | --- |
| F1 | 不阻塞任何批次；须在验收标准中显式记为已知限制 |
| F3-可观测 | 需在 B4（表现接口分离）内建 present 回执通道；B5 的观测与验收须覆盖该计数 |
| F2 | 同上，另需公共契约审批、版本判定与安装消费者证据 |

**三种选择都不阻塞 B1–B3。**

## 7. 未执行项

本轮只做只读核对与文档：未运行 C++ 构建/CTest、未做真机最小化/遮挡实测、
未统计反馈窗口的实际分布、未实现回执通道或计数。
§1 全部判断来自源码阅读；"最小化期间是否有 present 调用"依赖平台行为，
**未经实测确认**，属 B1 范围。
