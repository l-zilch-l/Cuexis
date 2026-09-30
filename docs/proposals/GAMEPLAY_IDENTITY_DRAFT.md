# Gameplay Identity 分层草案

状态：candidate（设计草案，未接受，未实施）

更新日期：2026-09-29

本文回应 [Gameplay Ruleset 设计讨论记录](GAMEPLAY_RULESET_DISCUSSION.md) 下一步工作 F：判定与内容
identity 的分层。前置决定见该文 §2.9、§2.10 与 §3.1。

## 1. 要解决的问题

经过分层与 Program IR 的设计，identity 已经牵出多个来源：谱面语义、程序内容、Ruleset
Interface 版本、Ruleset Build、Loadout、输入映射，以及生效后的表现绑定。

如果它们只合成一个摘要，会出现两类错误：

```text
过宽    换一张贴图就让全部已有 Replay 失效
过窄    改了判定线运动却不影响 Replay 绑定，重放出不同结果
```

因此要把"哪些字段影响判定结果"变成可机械检查的格式属性，而不是靠维护一张人工清单。

## 2. 字段分区

每个语义字段必须声明一个类别，类别完备且互斥：

| 类别 | 含义 | 例子 |
| --- | --- | --- |
| `judgment` | 影响判定结果 | requirement 的参数与区间、region、frame 轨道、判定程序、窗口、Loadout |
| `presentation` | 只影响外观与声音 | 材质、贴图、判定文字、按键音、L4 绑定 |
| `neutral` | 两者都不影响 | 作者名、注释、标签、目录顺序 |

分区由 Schema 作者声明，验证器做两件事：**每个字段都有类别**，以及**类别完整覆盖所有字段**。
改变某个字段的类别属于破坏性格式变更，需要新的格式版本。

分区必须**逐字段**判定，不能按"几何 vs 视觉"粗分。反例很直接：frame 轨道与 Slider 路径是
`judgment`，而相机与材质是 `presentation`，它们都是几何数据。

### 2.1 分类的可信性

分类若由内容自己声明，而内容是不可信的第三方包，就会出现一个健全性缺口。一个包可以把影响
判定的字段声明成 `presentation`：

```text
包声明：scoreMultiplier 是 presentation
实际：它影响折叠结果
结果：改数值后 JudgementIdentity 不变
      → 旧 Replay 仍然"合法"，却播放出不同分数
```

这既是静默分叉，也是一个作弊面。两条规则堵住它：

```text
1. 引擎自有的字段类型由引擎固定类别，包不能覆盖。
2. 包自定义的类型默认落在 judgment 类别。
```

默认方向统一取保守一侧：**过度判定只会让 Replay 多失效，判少了才会静默分叉。**

检查阶段随之确定：Schema 阶段是便利，**prepare 阶段是必须**，因为引擎不控制第三方包的
Schema。该检查是纯结构性的，可按 `ContentIdentity` 缓存，不进会话热路径。

### 2.2 Interface 也做字段分区

Ruleset Interface 的变化同样需要区分"对既有内容是否可见"，判据与 §2 完全一致：

**这个变化是否改变既有内容的判定结果，或者让既有内容变得非法。**

| Interface 变化 | 对既有内容的判定结果可见 | 是否需要 bump |
| --- | --- | --- |
| 末尾追加一个 grade | 否 | 否 |
| 中间插入一个 grade | 是（名次索引后移） | 是 |
| 放宽 Hook 的取值范围 | 否 | 否 |
| 收窄 Hook 的取值范围 | 是（既有程序或 Loadout 可能非法） | 是 |
| 新增一个 Hook | 否 | **否** |
| 新增 outcome 或 category | 否 | 否 |
| 改窗口表数值 | 不适用 | 这是 Build 变化，本来就不 bump |

由此得到两条结论：

- **grade 集合只能追加，不能插入。** 插入是破坏性变更。
- **新增 Hook 不 bump。** 否则每加一个技能都要重发整个曲库。

第 1 条与 §2.1 是同一条规则的两种应用，不是两套机制。

## 3. 两个摘要

```text
ContentIdentity
  覆盖全部字段（judgment + presentation + neutral）
  用途：交换、防篡改、缓存键、CXC entry identity、资源闭包

JudgementIdentity
  只覆盖 judgment 类字段，按来源分组
  用途：Replay 绑定、排行榜可比性、Studio 结果复现
```

两个而不是一个，因为它们的作用范围本来就不同。用 ContentIdentity 做 Replay 绑定会过宽，
用 JudgementIdentity 做内容身份会过窄。

`ContentIdentity` 沿用既有的域分隔 SHA-256 合成方式，不新造机制。

## 4. JudgementIdentity 的组成

四个来源，每个是**命名分量**，最后按域分隔合成：

```text
engine     判定语义版本、定点表版本（角度表等）、Tick 分辨率
ruleset    Interface 版本（等级集合、category、outcome、程序可见 Hook 及静态范围、窗口表）
           Build 的 judgment 投影（Controller、region、仲裁策略、fold、模块骨架）
chart      judgment 投影：requirement 集（参数已解析）。
           每条 requirement 在 Fold Calculus 下是
           （标准库条目 ID | 内联 Pattern） + 参数 + 计量规格
           另含被引用的 region 与 frame、谱面自带的判定程序
session    Loadout（启用模块与参数取值）
```

**命名分量而不是单一摘要，是为了诊断**：失配时能指出是 engine、ruleset、chart 还是 session
变了，而不是只报"身份不符"。这也让既有 `PreparedSemanticIdentity` 的域分隔合成方式可以
直接扩展为分量表。

### 4.0 术语对应

本文写作时间早于 [Bounded Fold Calculus 提案](GAMEPLAY_FOLD_CALCULUS_DRAFT.md)，个别名词与其
不同但概念对应：

| 本文 | Fold Calculus | 说明 |
| --- | --- | --- |
| Hook | 程序可见 Hook（calculus §5.6） | 同一概念；calculus 把 `hook` 列入谓词读取集合 |
| 窗口集 | 窗口表 / 等级集合（calculus §3.6） | 同一概念；Grading 用它把 Measure 映射到等级 |
| requirement 集 | Pattern / 标准库条目 + 参数 + 计量规格 | 同一概念，表达形式改变 |
| 判定程序 | Pattern（编译为 Fold） | 作者写 Pattern，引擎编译为自动机 |

引用本文的结论时，以本表换算到 calculus 的名词。

### 4.1 Interface 投影（use-based）

ruleset 分量不直接使用 Interface 版本的内容清单，而是使用 **Interface 投影**：内容与 Loadout
**实际引用**了哪些 Hook、窗口表、region 与 grade。

理由见 [Skill Hooks 与模块扩展性草案](GAMEPLAY_SKILL_HOOKS_DRAFT.md) §7：Interface 版本是
"兼容集合"，只回答内容能否运行；投影才回答"实际用到哪些"。两者分开之后，

- 新增一个 Hook 且无人引用：版本不变，投影不变，没有 Replay 失效。
- 新增一个 Hook 且被某谱面引用：版本不变，只有该谱面的投影变化。

这与 §2 的字段分区是同一条原则，只是从"字段"推进到"实际使用"。

## 5. 显式排除清单

显式排除与显式包含同等重要，否则会意外耦合：

```text
渲染后端、帧率、Entity 遍历顺序
时钟来源（ChartClock / HostClock / CuexisAudio）
音频输出设备与延迟
表现资源、皮肤、L4 绑定
输入映射与校准
诊断与日志
作者元数据、注释、标签
```

时钟来源被排除不是新决定，而是与 `TIMING_MODEL.md` 已有的规定一致：三种时钟必须对相同的
规范化输入产生相同的判定结果。这说明分区原则与既有合同自洽。

L1 归一化整体被排除，因为 Replay 记录的是**规范化之后**的事件。见 §6。

## 6. 输入映射与校准的位置

原始设备输入先经过 L1 归一化、映射和校准，成为规范化事件。Replay 记录的就是这些事件。

因此：

- **映射 identity 与校准参数都是信息性元数据**，写入 Replay 头部用于诊断，但不参与校验门。
- 若把事件记录在映射之前，则映射与校准必须进入 identity，且校验失败要拒绝播放。这会带来
  一个不必要的结果：玩家换键盘、换手台或改校准之后，旧 Replay 不再可播。
- Replay 的本质是"在给定谱面与规则下，重放这段规范化输入能得到相同结果"。它只依赖判定
  折叠所消费的东西。

这条修正了讨论记录第 24 条的措辞。该条原文同时要求"记录映射后的事件"与"校验映射 identity"，
两者不能同时成立：映射若已烘进数据，校验只会错误拒绝有效 Replay。

## 7. 变更影响表

| 改动 | ContentIdentity | JudgementIdentity |
| --- | --- | --- |
| 换材质 / 贴图 | 变 | 不变 |
| 改 Note 位置或判定线运动 | 变 | 变 |
| 改作者名 / 注释 | 变 | 不变 |
| 改谱面判定程序 | 变 | 变 |
| 改 Ruleset 窗口数值 | 变 | 变 |
| 只改 Ruleset 的按键音 | 变 | 不变 |
| 换 Loadout | 不变 | 变 |
| 换输入映射或校准 | 不变 | 不变（已烘进事件） |
| 换渲染后端 / 帧率 / 时钟来源 | 不变 | 不变 |
| 引擎升级改了判定语义 | 不变 | 变 |

第六行要求 **Ruleset 包同样做字段分区**。分区原则是通用的，不只适用于谱面。

## 8. 失配策略

```text
组件级比对     identity 是命名分量的记录，诊断能指出是哪个分量变了
默认严格拒绝   不静默用错的身份重放
诊断模式       显式开启后可用当前内容重新折叠，结果标记为非权威
```

诊断模式用于 Studio 与谱面校验：显示"哪里变了"、"结果差多少"，帮助作者定位改动的影响。
它是非权威路径，产生的分数不计分、不写排行榜、不进入任何证据。

## 9. 与既有实现的关系

现有 `PreparedSemanticIdentity` 由 canonical Chart identity、各 CXT identity、各资源内容
identity 和 parameter identity 合成。它**包含资源内容 identity 是正确的**，因为它服务的是
prepare 事务与 CXC 身份。

但它**不能直接复用为 Replay 绑定**：包含资源内容 identity 会让一次换贴图失效全部 Replay。
需要的是"新增一个 judgment 投影摘要"，而不是"修改 PreparedSemanticIdentity"。这一点的
措辞应精确到"不复用"，而不是"它是错的"。

## 10. 判定语义版本

判定语义版本与引擎版本必须**分开编号**。理由：引擎升级若只改渲染、后端适配或日志，不应让
`JudgementIdentity` 变化，否则每次发版都会清空全部已有 Replay。

代价是需要配套两件事：

1. 一份"什么算判定语义"的清单。范围是参与折叠的部分：窗口比较、锚点量化、仲裁、fold
   求值、外推基求值、定点表。L1 归一化不在内，因为 Replay 记录的是规范化之后的事件（§6）。
2. 一个"判定语义未变"的门禁，要求变更必须刻意放行并留痕。本仓库已有同类机制：
   `tools/check_version_gate.py` 对 SDK API 版本采用"一经变更即需 `--allow-sdk-api-change`
   放行"的模式，判定语义版本照此办理。

该脚本目前是 Stage 7A 的一处已知阻塞：开关没有工作流传入，而检查器从 base commit 取出运行，
所以候选分支无法自行开启。设计时应一并规划放行开关的传递路径，避免重复同一个坑。

## 11. 定点表版本

定点表（角度表，以及将来可能的 `exp` / `sinusoid` 外推基表）的版本**必须进入 engine 分量**。
否则表被改进时，角度相关的判定结果会变而 identity 不变，属于 §2.1 的静默分叉。

是否允许同一张表多版本并存，是第二个问题，可以后置。若允许并存，表的版本就成为一个
capability，需要它的内容在缺少该表的引擎上稳定拒绝。

定点表需要**一个统一的登记点**：一张表清单一处声明版本，而不是每加一张表就多一处要记得
写进身份。这与 Fold 草案待决项 3 是同一件事。

## 12. Replay 头部格式

失配诊断要能指出"是哪个分量变了"，因此 Replay 头部必须存**逐分量的 identity**，而不是只有
一个合成摘要。这是格式决定，晚定就要改 Replay 格式版本。

对照 §6，映射 identity 与校准参数同样写进头部，但性质不同：它们是信息性元数据，不参与校验。

"结果差多少"的量化报告属于 Studio 工具功能，可以后置。

## 13. 待决

1. Ruleset Interface 版本的 bump 细则。§2.2 给出了判据与主要条目，但完整清单需要与
   Interface 的字段集合一起冻结。
2. 定点表是否允许同一张表多版本并存，还是永久冻结、只能新增。
3. Studio 的诊断模式需要多详细的可视化，以及"结果差异"报告的量化粒度。
4. 判定语义的清单如何与 `check_version_gate.py` 的现有机制合并，以及放行开关的传递路径。
5. Interface 投影的粒度：按 Hook 标识逐项，还是按"判定域 + 参数族"聚合。粒度太细会让
   identity 计算量与内容规模线性相关。与
   [Skill Hooks 与模块扩展性草案](GAMEPLAY_SKILL_HOOKS_DRAFT.md) §10 是同一问题。
