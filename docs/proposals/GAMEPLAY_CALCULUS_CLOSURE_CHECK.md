# Fold Calculus 封闭性验证

状态：candidate（验证记录，未接受，未实施）

更新日期：2026-10-01

本文对 [Bounded Fold Calculus 提案](GAMEPLAY_FOLD_CALCULUS_DRAFT.md) 做封闭性验证。方法是
**只许用 Pattern / Measure 算子重写全部 34 个案例**，凡需要一条点名提到该案例的规则，即记为
不闭合。

这是提案 §7 要求的判据，也是它与"继续收集案例"的根本差别：**目标是从收集转为证明**。

## 1. 判据

```text
组合通过   该案例可仅由算子组合表达，且不需要新的核心概念
需新声明   可表达，但需要一条提案中尚未包含的声明（不是新原语，是缺失的规则）
不闭合     需要新的核心概念
```

三类的区别很重要：**"需新声明"是设计还没写完，"不闭合"是设计错了。**

## 2. 逐项结果

| # | 案例 | 组合 | 结论 |
| --- | --- | --- | --- |
| 1 | Tap | `atom press(lane) · scalar(t - anchor)` | 组合通过 |
| 2 | Hold 中途断开 | `interval held(lane) over [t0,t1] · gap(¬held, grace)` | 组合通过 |
| 3 | maimai 星星跳区 | `touch(z0) · (.. within k)* · touch(z_last)` | **需新声明 V3** |
| 4 | osu!catch 水果 | `instant at(t) : \|ctrl.x − x\| ≤ w` | 组合通过 |
| 5 | 判定区重叠 | Tap 的 Pattern + 外部仲裁 | **需新声明 V1** |
| 6 | 变宽 Slider | `interval contactIn(c, corridor(t)) over [t0,t1]` + `sameContact` | 组合通过 |
| 7 | 炸弹 | `¬atom press(lane) over [open, close)` | 组合通过 |
| 8 | Arcaea Arc | `interval sameContact(inCorridor) over [t0,t1] · duration` | 组合通过 |
| 9 | Taiko 连打 / DJMAX | `repeat press{0,∞} · count` | 组合通过 |
| 10 | Spinner | `interval motion(region) · sum(\|cross\|)` | **需新声明 V7** |
| 11 | Flick 方向 | `atom press · atom release · scalar(cross(dir, Δ))` | 组合通过 |
| 12 | 多指 Slide | n 个独立 requirement + fold 聚合 | 组合通过，**且可删去积**（V5） |
| 13 | 移动判定线 | region 绑 frame | 组合通过 |
| 14 | Taiko 气球 | `interval repeat press{0,∞} over [t0,t1] · count` | 组合通过 |
| 15 | Taiko 大音符 | `repeat press{2} · count` | 组合通过 |
| 16 | Taiko 鬼按惩罚 | `¬atom press(lane) over [a, b)` | 组合通过 |
| 17 | IIDX 转盘 | `axisStep` 通道或两个 atom | 组合通过 |
| 18 | IIDX note lock | 仲裁过滤 | **需新声明 V1** |
| 19 | IIDX keysound | 不在 L2，属 L4 输入路径 | 组合通过（不适用） |
| 20 | DDR 冻结箭 | `interval held(lane) over [t0,t1]` | 组合通过 |
| 21 | DDR 地雷 | 同炸弹 | 组合通过 |
| 22 | DDR Lift | `atom press · interval ¬held(lane) over [t0,t1]` | 组合通过 |
| 23 | Guitar Hero 和弦加拨弦 | `instant at(t) : held(f0) ∧ held(f1) ∧ press(strum)` | 组合通过 |
| 24 | Phigros Drag | `instant at(t) : active(region)` | 组合通过 |
| 25 | Phigros Flick | 同 11 | 组合通过 |
| 26 | Phigros 旋转判定线 | 同 13 | 组合通过 |
| 27 | Arcaea ArcTap | `atom press(inRegion(arcPoint(t)))` | 组合通过 |
| 28 | CHUNITHM 空气音符 | `instant at(t) : f(ctrl) ≥ x` | 组合通过 |
| 29 | osu!mania LN 尾判 | `press · interval held · release` 三段分别计量 | **需新声明 V2** |
| 30 | osu!mania OD | 窗口参数 | 组合通过 |
| 31 | osu! Slider tick | `repeat touch(anchor_i){0,n}` | 组合通过 |
| 32 | Autoplay / Replay | 合成输入流，走同一路径 | 组合通过 |
| 33 | Beat Saber | 3D | 超出范围（L2） |
| 34 | Rocksmith | 连续音高 | 超出范围（L2） |

```text
组合通过   28
需新声明    4（案例 3、5/18、10、29）
不闭合      0
超出范围    2
```

**没有任何一项需要新的核心概念。** 但四项需要提案中缺失的声明，其中两项是提案**丢弃**了
旧设计中已存在的东西，而不是没能组合。这四条就是本轮的真实产出。

## 3. 发现的缺口

### V1 仲裁被整个丢弃（严重）

提案 §2 把 Effect 列为"写状态 | 绑定资源 | 发出 Fact | 终结"，但没有再提仲裁。旧草案 §7 的
一整节——候选集、`claimKey`、`lock`、`fanout`——在新提案里没有任何位置。

**仲裁不能被 Pattern 吸收。** 判定区重叠（案例 5）的本质是"一次按下，两个 requirement 的
Pattern 都匹配，只能有一个结算"，这是 requirement 之间的全局性质，不是任何单个 Pattern 的
性质。同理 note lock（案例 18）也是全局的。

处置：**仲裁保留为 Ruleset 级的声明**，与之前完全一致。提案必须显式说明它没有被归约掉。
这不是缺陷，但是遗漏，且必须写明，否则实现者会以为 Pattern 是全部。

### V2 Measure 是单值，无法多阶段计量（严重）

Hold 的头部按时间评级、尾部按覆盖率评级；mania LN 的头体尾三段各自计量。提案的
`Requirement = (Pattern, Measure, Grading)` 只有**一个** Measure 和一个 Grading。

**这是提案自己引入的回归。** 旧设计有 `phase` 概念（`Hit{phase, grade, errorTicks}`），
新提案把它悄悄删掉了，却没有替代。

处置：**Measure 可以是积，Grading 按分量分别应用，每个分量自带 phase 与 category。**

```text
Hold = (
  Pattern: touch · (interval held over [t0,t1] with gap) · release,
  Measure: (headErr = scalar(t_press − t0),
            coverage = duration(held) / (t1 − t0)),
  Grading: (head → 时间窗口表, body → 覆盖率表)
)
```

这是一个**乘积扩展，不是新核心概念**：Measure 的算子集合不变，只是允许并列多个。Fact 因此
变为"每个分量一条"，每条带自己的 phase 与 category。

### V3 非确定模式的匹配策略未定义（严重）

`skip`、`repeat`、`alternation` 会让同一个输入轨迹产生**多个合法匹配**，而 Measure 取决于
选中的那一个。

```text
输入    A B A B
模式    A .. B within 1
匹配    [A1,B1] / [A1,B2] / [A2,B2]     三个都合法
```

若 Measure 是 `scalar(t_B − anchor)`，三个匹配给出不同的值。**判定结果不唯一。**

旧设计用显式规则回避了这一点（"推进到第一个满足的下标"），新提案丢掉了它。

处置：**必须声明匹配策略**，二选一：

```text
leftmost-first    从左到右，每步取最早可行分支。可流式实现，推荐用于判定
leftmost-longest  取最长匹配。需要回看，实现代价高
```

判定应当选 `leftmost-first`（最早满足即确定），并且策略进入 Interface 与判定 identity，
因为它改变结果。

**这条是本轮最重要的发现**，因为它正是 D1–D10 那一类错误：一条没写的语义规则，会导致
结果不唯一，而且只在特定输入序列下暴露。

### V4 `sameContact` 与资源绑定的关系未定义（中）

Pattern 层的 `sameContact(P)` 是**匹配约束**（"这些原子指向同一接触点"），而 Effect 的
"绑定资源"是**资源分配**（"这个接触点归我"）。提案同时用到两者却没有说明关系。

具体缺口：两个 requirement 的 Pattern 都要求 `sameContact`，而只有一个接触点可用时，谁得到
它？这正是 V1 的仲裁问题。

处置：明确两层

```text
sameContact   Pattern 层：约束"这一条匹配必须由同一个接触点完成"
绑定资源      Effect 层：认领后独占消费权
归属裁决      仲裁层：Pattern 匹配之外的全局分配
```

同时保留旧草案 §4.5 的归属与可见性分离：`observe` 边始终可见（Arcaea Arc 需要"看到但拿不到"
才能判出换手即断开）。

### V5 积可以被删掉（简化，非缺口）

提案担心"补与积会让自动机指数增长"，并把多指 Slide 列为最吃力的地方。**重写后发现积是多余的。**

多指 Slide 就是 **n 个独立的 requirement**，各自一个 `interval held(lane_i) over [t0,t1]`。
它们之间不需要通信；"全部命中"是 fold 层读取 n 条 Fact 后的聚合，属于 L3。

因此：

```text
删掉积算子
多指类案例全部变为"n 个独立 requirement + fold 聚合"
补仍然需要（炸弹、Lift、鬼按惩罚），但补的规模可控：它就是"区间内原子恒假"
```

这消除了提案 §6 最主要的风险项。**补保留，积删除。**

### V6 Packed 格式的 requirement 编码需要重新设计（中）

旧格式的 requirement 是一个定长元组（kind + interval + domain + action + constraints），
参与 40k 实体 / 16 MiB 的容量门禁。新设计里 requirement 是一个 Pattern AST，体积明显更大。

处置：**编码为 `(标准库条目 ID | 内联 Pattern AST)` + 参数 + 计量规格。**

```text
标准库条目   绝大多数谱面：一个 ID 加几个参数，比旧元组还小
内联 AST     自定义 Pattern：较大，但有独立预算
```

因此常见情形不会退化，容量风险只落在自定义 Pattern 上，可以单列预算与 capability。

### V7 Measure 作用于电平原子时依赖采样周期（中）

Spinner 的 `sum(|cross|)` 是**按采样点求和**，它逼近的是弧长，逼近精度取决于声明的采样周期。
周期进入 Interface 与 identity，所以结果是确定的，但**同一个谱面在不同采样周期下会得到不同
的 Spinner 评级**。

这不是非确定性，是近似敏感性。处置：在 Pattern 层显式标注"该 Measure 对采样周期敏感"，并在
作者文档中说明；Ruleset 若要固定评级，就固定周期区间为单点。

## 4. 结论：闭合性的真实状态

**没有一项需要新的核心概念**，因此三个概念（Fold / Pattern / Measure）的骨架成立。但四类
声明缺失，其中两类是提案丢弃了旧设计已有的东西：

```text
V1  仲裁：旧设计有，提案丢掉        -> 恢复为 Ruleset 级声明
V2  多阶段计量：旧设计有 phase，提案丢掉 -> Measure 改为允许积
V3  匹配策略：旧设计有显式规则，提案丢掉 -> 声明 leftmost-first
V4  归属与可见性：旧设计有，提案未接  -> 明确两层关系
V5  积：提案有，实测多余            -> 删掉
V6  格式编码：需要重新设计
V7  采样敏感性：需要标注
```

**这是一个有价值的失败。** 提案声称"10 条缺陷消除 6 条"，这个数字仍然成立；但它同时**新引入
了 3 条**（V1、V2、V3 都是语义未定）。净收益因此低于提案的声称。

诚实地说：归约做对了核心，做丢了三样必要的东西。而丢掉的这三样有一个共同点——

> **它们都不是"判定一个音符"的语义，而是"多个音符之间"的语义。**

仲裁（多个 requirement 争一个输入）、多阶段计量（一个 requirement 的多个方面）、匹配策略
（一个 pattern 的多种匹配）全部是**关系性**的。归约把注意力放在单条 requirement 的内部结构
上，因此把跨实体与跨匹配的部分一并简化掉了。

这是本轮真正学到的东西：**正交化的边界应当在"单实体"和"跨实体"之间，而不是在"简单"和
"复杂"之间。**

## 5. 对提案的修订建议

```text
保留  Fold / Pattern / Measure 三概念骨架（验证通过）
保留  原子、序列、择一、有界重复、跳步、同接触点、瞬时
保留  补（¬），用于炸弹类
删除  积（V5）
增加  Measure 允许积，Grading 按分量应用，分量自带 phase 与 category（V2）
增加  匹配策略声明，默认 leftmost-first（V3）
增加  仲裁作为 Ruleset 级声明，明确不属于 Pattern（V1）
明确  sameContact 与资源绑定、归属裁决的两层关系（V4）
设计  requirement 的格式编码（V6）
标注  Measure 的采样敏感性（V7）
```

修订后核心仍然是三个概念，但**声明面增加了三条**。这说明"低成本表达"的目标达成了一半：
作者侧的复杂度确实降下来了（Pattern 比自动机好写得多），但引擎侧的规则没有同比例减少。

## 6. 下一步

```text
1  按 §5 修订提案，产出版本二
2  版本二需要重跑本验证，确认 V1–V3 的修补没有引入新缺口
3  只有第二版也通过之后，才进入 I 收敛（ADR / Spec / 预算与 ABI）
```

不进入下一步的原因是：本轮的失败模式很清楚是"归约时丢掉了关系性语义"，而不是"某个案例
没想到"。如果不重跑一遍，同类遗漏很可能还有一处没被发现。
