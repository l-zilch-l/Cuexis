# TB-06 判定说明：音轨结束后的尾段推进

状态：historical decision brief；保存早期选项依据，后续已选方向，未实施

日期：2026-10-11（Asia/Shanghai）

本文把 [生产架构方案](2026-10-11-production-architecture-design.md) §7 的「TB-06 尾段时钟」
展开为可裁定的说明。**只做只读核对与选项界定，不改任何代码或合同。**

**后续决定（2026-10-11）：** owner授权剩余普通方案自行收敛；最终选择和测量gate归
[RPA计划](../../../../stage_plans/active/stage-07/plan-rpa.md)及
[RPA补充Spec](../../../../formats/rpa-execution-contract.md)。下文选项为当时比较输入，不再等待逐项点选。

## 0. 结论摘要

| 问题 | 核对结果 |
| --- | --- |
| 音轨 EOF 后 H 会不会继续推进？ | **不会**。`Ended` 观察把 Player 从 `Playing` 置为 `Stopped` |
| 这是不是漏判？ | 是。EOF 后的到期 timer 永不结算，合法谱面会漏判 |
| 无音频路径的暂停是否累积？ | **不累积**。暂停期间帧循环不采样时钟；恢复由 `runPlay` 的 `pendingChartSeekMs_ = chartResumeMs_` 重建基准 |
| 现行是否有可复用的尾段推进机制？ | **有**。`ChartClock`（`HostClock` 模式）已提供可检查推进的无音频时钟 |
| 是否需要新公开 API？ | 推荐方案不需要 |

**推荐：A1-b（见 §4）——把「音轨结束后的推进来源」定义为显式的尾段时钟，
复用 `ChartClock` 语义，作为内容身份之外的会话级声明。** 数值（起点、速度、输入接收期限）
由 B1 测量后裁定，本文不给。

## 1. 只读核对的现行事实

### 1.1 音频 EOF 的直接后果

| 事实 | 位置 |
| --- | --- |
| 播完整个 clip 时 `updatePresentedFrame` 置 `state = Ended` 并暂停设备 | `sdl_audio.cpp` |
| `observeTransportState`：`Ended` → **仅当**当前为 `Playing` 时转为 `Stopped` | `player_command.cpp` |
| 注释原文：`// Only a playing application ends. A paused or stopped one keeps its position.` | 同上 |

**推论：** 音轨结束 → 应用转 `Stopped` → 帧循环不再推进 H。**EOF 之后到期的 timer
（含显式 tail 与 hard deadline）在 EOF 时刻未到期的话，就永不结算。**

### 1.2 反例的具体形态

设音轨止于 1000ms，合法谱面的最后 timer 在 1020ms：

1. 1000ms 处音频 `Ended`，应用 `Playing → Stopped`。
2. 帧循环此后不再调 `advanceGameplay`（应用非 `Playing`）。
3. 1020ms 的 timer 永不触发。
4. 该 Requirement 保持 Pending，**不产生 Hit 也不产生 Miss**，最终结果不完整。

这不是"少判一个音符"的显示问题，而是**判定结果不完整**：`needsFinalize` 的 Requirement
在半开启状态被留下，而现有契约要求非 observe 实例的每个声明 phase 恰好产生一次 Fact。

### 1.3 无音频路径已经是"显式时钟"，且暂停不累积

| 事实 | 位置 |
| --- | --- |
| 帧循环的无音频分支：`Playing` 时 `clock.nextChartTime(...)` 取 `base_ + elapsedMs()`；非 `Playing` 时 `clock.lastChartTime()` | `player_control.cpp` `advanceFrame` |
| `PlayerClock` 用 `std::chrono::steady_clock`，`nextChartTime` 每次重算 `base_ + elapsed()` | `player_control.cpp` `class PlayerClock` |
| `runPause`：`chartResumeMs_ = lastFrame_.chartTimeMs` | `player_control.cpp` |
| `runPlay`：`pendingChartSeekMs_ = chartResumeMs_`，然后 `markDiscontinuity()` | `player_control.cpp` |
| 帧循环消费 `pendingChartSeekMs_` 时调 `clock.rebase(*target)` | `player_control.cpp` `advanceFrame` |

**推论：** 暂停期间不采样时钟（非 `Playing` 走 `lastChartTime()`），恢复时 `rebase` 重建基准，
因此**暂停期间不累积**。**关键点：现有的无音频路径已经是一条"暂停期间停在原处、
结束后不再自行前进"的显式时钟——它正是 TB-06 所需的形态。**

### 1.4 现行有四种完成信息，且确实需要分开

| 完成信息 | 现行归属 | 是否已独立 |
| --- | --- | --- |
| 音轨结束（audio EOF） | `audio::PlaybackState::Ended` | 是 |
| 执行终态（H 不再推进） | 应用 `Stopped` 或显式 `Stop` control | 是 |
| 表现排空（待显示反馈处理完） | `RemainingFrames` 计数 | 是 |
| 归档/导出完成 | `archiveGameplay` / `evaluateGameplayReplay` 显式调用 | 是 |

**四条今天都已可分别观察**，缺的只是"EOF 之后由谁继续推进执行时间"这一条规则。

## 2. 选项

| 选项 | 内容 | 代价与风险 |
| --- | --- | --- |
| **A1-a** | 音轨 EOF 时**立即把所有未完成 Requirement 结为 Miss**，不再推进 | 最省实现；但**会误判**：EOF 前已开始、EOF 后才到期的合法 timer 被判 Miss；且与"EOF 后可能仍有合法输入窗口"的内容不兼容。**不推荐** |
| **A1-b 推荐** | 引入**显式尾段时钟**：EOF 后由会话级声明的时钟继续推进 H，直到尾部条件满足 | 需定义起点、速度、输入接收期限、终止条件；四个量都需测量。复用现行 `ChartClock` 语义，实现面小 |
| **A1-c** | 由内容声明"EOF 后还需多少 Tick / 多长尾段" | 需新增 Chart/CXC 字段与格式修订，属 Stage 8 量级；**超出本阶段范围** |
| **A1-d** | 维持现状（EOF 即停止推进，不结算余下 timer） | 零改动；但漏判持续存在，且与"完整结果"契约冲突。作为**已知限制**可暂留，不能作为终态 |

**推荐 A1-b。** 理由：

1. **它复用已有机制。** §1.3 的无音频路径已经是"显式时钟 + 暂停不累积"，尾段时钟是同一形态
   在 EOF 后的延续，不是新概念。
2. **它不引入格式修订。** A1-c 要动 Chart/CXC 字段，属格式阶段；本阶段不应开这个口。
3. **它和 A1-a 的差别是正确性。** A1-a 会把合法的尾部 timer 误判为 Miss，
   这与现行"每个声明 phase 恰好一次 Fact"的口径冲突。
4. **A1-d 是诚实的降级**，但它必须被登记为已知限制而不是"实现完成"。

## 3. 需随 A1-b 一起裁定的四个量（本文不给数值）

| 量 | 为什么必须有 | 数值从哪来 |
| --- | --- | --- |
| 尾段时钟**起点** | 是 EOF 时刻，还是"最后一个已提交 Tick"？两者在晚到输入下不同 | B1 测量：EOF 时刻的已提交前沿与实际误差 |
| 尾段时钟**速度** | 实时（1×）还是更快？影响用户等待与输入窗口 | 产品决定 + B1 的尾部时长分布 |
| **输入接收期限** | EOF 后是否还接受真实按键？接受到何时？ | 产品决定（关系到 TB-07 的恢复语义） |
| **终止条件** | 何时算尾段结束并冻结最终 horizon | 由内容的最晚 timer/deadline 派生，需 B1 统计 |

**这四项不能现在给数。** 它们取决于音乐库中"音轨结束到最晚 timer 的间隔"的真实分布，
而该分布**今天没有测量**。给数字就是编造。B1 的产物须包含这份分布。

## 4. 边界条件（若选 A1-b）

1. **尾段时钟是会话级声明，不进内容身份。** 它与 `calibrationIdentityToken` 同类：
   影响运行轨迹但由会话配置供给，不写进 Chart/CXS 字节。
2. **尾段期间暂停不累积**（沿用 §1.3 的既有性质），resume 由 rebase 重建基准。
3. **尾段期间不伪造输入。** 尾段时钟只推进时间，不产生 press/release；
   是否接受真实输入由"输入接收期限"裁定（§3）。
4. **失效音频不得静默 fallback。** 从"音频时钟"切到"尾段时钟"必须显式、
   有回执、可观测；设备丢失与正常 EOF 走不同路径（前者撤销推进许可，后者按声明继续）。
5. **不与 Seek/Replay 冲突。** 尾段时钟影响最终 horizon，因此 `archive`/`cut` 必须包含
   尾段时钟的声明与实际应用范围；完整 Replay 不依赖实时设备即可复现相同结果。
6. **表现排空与执行终态分开。** 尾段时钟只解执行终态；`RemainingFrames` 与归档完成仍各自判定。
7. **不新增生产默认值。** 尾段上限、超时等只能作为显式 testOnly 参数。

## 5. 本裁定的准入效果

| 若选 | 对批次的影响 |
| --- | --- |
| A1-a | 不阻塞 B5，但会把误判写进语义；F1/F3 的完整结果验收需接受该已知偏差 |
| **A1-b** | **阻塞 B5 的验收口径**（尾段是最终 horizon 的一部分），但**不阻塞 B1–B4 开工**；B1 须产出 §3 的分布 |
| A1-c | 超出本阶段；须移入 Stage 8 或作为新的格式提案 |
| A1-d | 不阻塞任何批次，但必须在验收标准中显式记为已知限制 |

**四种选择都不阻塞 B1–B4。** 只有 B5 的验收口径需要它确定。

## 6. 未执行项

本轮只做只读核对与文档：未运行 C++ 构建/CTest、未做音频 EOF 实测、
未统计音轨结束到最晚 timer 的间隔分布、未验证尾段时钟的任何实现。
§1 全部判断来自源码阅读；`Ended` 行为虽在代码中明确，但**未经设备实测确认**
在真实播放器上的表现（尤其缓冲耗尽与真实结束的区分）属 B1/B5 范围。
