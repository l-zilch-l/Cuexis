# AD-02 判定说明：执行与表现拆分阈值

状态：historical decision brief；保存早期选项依据，后续已选方向，未实施

日期：2026-10-11（Asia/Shanghai）

本文把 [生产架构方案](2026-10-11-production-architecture-design.md) §5 的「AD-02 拆分阈值」
写成短说明。与其他判定说明不同，**本项现在没有可选方案集**——它是一条待定的测量判据。
本文只界定判据的形式、所需数据与依赖顺序。

**后续决定（2026-10-11）：** owner授权剩余普通方案自行收敛；最终选择和测量gate归
[RPA计划](../../../../stage_plans/active/stage-07/plan-rpa.md)及
[RPA补充Spec](../../../../formats/rpa-execution-contract.md)。下文选项为当时比较输入，不再等待逐项点选。

## 0. 结论摘要

| 问题 | 核对结果 |
| --- | --- |
| 现在能裁定一个阈值吗？ | **不能**。阈值需要测量输入和明确的产品目标，不能凭空给数 |
| 能否现在就说清判据的形式？ | **能**，且项目已有现成习惯可复用 |
| 判定所需的数据今天就有吗？ | **没有**，且有一个硬依赖：**判据在 B3 之后才可判** |
| 近似表述 | 当「表现最坏耗时使 H 落后于校准时钟」超过 profile 声明的界时，必须拆 D3 |

**结论：AD-02 不是一条待 owner 选的方案，而是一条待 B1/B3 产出数据后自动判定的规则。**
现在能做的是把它的**形式**钉死，使将来不需要再讨论一次。

## 1. 为什么它不能现在选一个数

拆 D3 的收益是"动画最坏耗时不阻塞判定推进"，成本是"多一套跨线程可靠前缀、控制 fence
与关闭状态机"。两侧都是**分布量**：

- 收益侧取决于表现最坏耗时（Behavior+Animation+resolver）的**尾部**，不是均值。
- 成本侧取决于在途队列、迟到回执与关闭枚举的复杂度，也是实测才知。

在没有这两条分布之前选一个阈值，等于把猜测写进 profile——正是本项目一贯拒绝的做法
（「生产阈值由实测及 owner 裁定」「不给数值推荐」）。

## 2. 判据的形式：复用项目已有的 `MeasuredParameter` 习惯

现行代码里已经有一模一样的形态，见 `engine/judgement/include/cuexis/judgement/timebase.hpp`：

```cpp
template <typename Value> class MeasuredParameter final {
    static constexpr auto pendingMeasurement() noexcept -> MeasuredParameter;
    static constexpr auto measured(Value value) noexcept -> MeasuredParameter;
    [[nodiscard]] constexpr auto isPendingMeasurement() const noexcept -> bool;
    [[nodiscard]] constexpr auto measuredValue() const noexcept -> const Value*;
    //  Returns nullopt while the parameter is pending measurement. There is deliberately no
    //  "value or default" accessor.
};
```

`LatePolicyParameters` 的四个字段都用它，初态 `pendingMeasurement()`，缺失或未测量在 **prepare
稳定拒绝**（类别 `late_policy_incomplete`）。

**建议 AD-02 的界沿用同一习惯**，即：

1. 界是一个**已声明参数**，不是渲染器常数、不是代码里的字面量。
2. 初态为 `pendingMeasurement()`，**没有默认值**，也没有 "value or default" 访问器。
3. 未测量时相关路径**稳定拒绝**，而不是静默使用某个兜底值。
4. 后续B0已裁定归宿主SchedulingProfile，不改变判定身份。

这样它就不是"以后再说"，而是一条**已声明的缺口**——未测量就不能跑，而不是悄悄放行。

## 3. drift 的可观测定义

判据不能是"感觉卡"，必须可计算。定义：

```text
drift(H) = (校准时钟导出的应有 Tick) − (kernel 实际已提交的 Tick)
```

两个量都要能从现有接口读出：

| 量 | 现行来源 |
| --- | --- |
| 应有 Tick | B3 之后的 capture-time 映射从校准时钟导出 |
| 实际已提交 | `GameplayAdvanceReceipt` 的 `processedFrontier` / `lastAdvanceHorizon` / `foldedWorkTick` |
| 表现消费位置 | `publishGameplayProjection` 的 `projection.lastTick`（表现 T） |
| 未消费事实年龄 | 最老未消费Fact的同域Tick差或单调发布年龄；cursor差仅是数量 |

方案 AD-09 已要求对外报告 `audioTick`、`presentationTick`、`committedFactTick`、
`presentationLead`、`factLag` 与最老未消费事实年龄。**drift 是这些量的组合，不是新概念。**

## 4. 硬依赖：判据在 B3 之后才可判

这一条是本说明里最重要的发现。

**今天 H 不是时钟导出的。** `advanceGameplay(h, t, frame)` 由调用方同时给出 H 与 T；
现行 Player 采样桥按「每个映射转换 `h += hStep`、无输入帧再 `+=`」构造 H。

因此**今天无法测量 drift**——H 是由帧数与按键数合成的，它没有"落后于时钟"这回事，
只有"与时钟无关"。

**结论：AD-02 的判定前提是 B3 已退出**（capture-time 的 H 由校准时钟导出）。
在此之前做任何 D3 拆分实验，测的是合成 H 的行为，不能作为选型依据。
这一点须写入 B3 的退出条件，避免有人提前跑实验并误用结论。

## 5. 需要的数据（B1/B3 产物）

| 数据 | 用途 | 来源 |
| --- | --- | --- |
| 表现最坏耗时分布（p50/p95/p99/max） | 收益侧 | B1 探针，按 Behavior/Animation/resolver 分段 |
| drift 的时间序列（同轨迹多渲染节奏） | 收益侧 | B3 之后 |
| 最老未消费事实年龄分布 | 收益侧（第二个触发量） | B1/B3 |
| 在途队列深度、迟到回执率 | 成本侧 | B1 |
| 关闭状态可枚举性证据 | 成本侧 | B4 设计期 |

每条 run 须绑定源码/dirty 差异、配置、语义 revision、硬件与观测开销（沿用 A2 口径）。
所有阈值保持待定；故障注入值不得转为生产预算。

## 6. 触发规则（数据到位后自动判定）

```text
若 (drift 尾部分位数 或 最老未消费事实年龄) 在目标内容与配置下
   持续超过 profile 声明的界，且有证据确认原因是D3阻塞
则 开启受限D3拆分工作包并重新验收
否则 维持 D2+D3 同 worker，并保留本判据待内容/配置变化后重评
```

「持续超过」与「尾部分位数取哪个」也须先声明，不得事后按结果选口径。

## 7. 本裁定的准入效果

- **现在裁定**：只钉死判据形式（§2）与依赖顺序（§4）。
- **不阻塞** B1–B4 开工；只影响 B5 的部署选择。
- 若 B1/B3 数据显示 drift 在界内，**维持同 worker 是正确的结论**，不是"没做够"。

## 8. 未执行项

本轮只做只读核对与文档：未运行 C++ 构建/CTest、未做任何表现耗时或 drift 测量、
未确定界归属的 profile、未实现 `MeasuredParameter` 形式的界。
§2 引自现有源码，§3 的量出自现有接口，均未运行验证。
