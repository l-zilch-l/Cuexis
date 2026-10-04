# Gameplay Judgement ABI（候选）

状态：superseded as typed preview input；retained as design-input history。Gameplay I 的候选 typed 输入，未实施，从未冻结为稳定 C ABI；typed 内部 / preview 边界现为 [GAMEPLAY_V2_ABI.md](GAMEPLAY_V2_ABI.md)，其语义权威为 [GAMEPLAY_V2_SPEC.md](../formats/GAMEPLAY_V2_SPEC.md)、决策依据为 [ADR 0044](../adr/0044-gameplay-v2-semantic-kernel.md)。本文件保留为历史候选输入与字段级差异对照，不删除、不再更新结论。

日期：2026-10-01

本文定义 Stage 7A 可实现的 C++ typed preview 边界，不改变当前 SDK API `0.7.0`，也不宣称
已有安装包暴露这些类型。稳定 C ABI 仍属于 Stage 14。

## 1. 生命周期

```text
create configuration
  -> prepare(chart, ruleset, loadout)
  -> submit normalized InputEvent
  -> advance to absolute time
  -> read JudgementResult / ScoreState / StatisticsSnapshot
  -> snapshot / seek / replay
```

`prepare` 冻结 Chart、Ruleset、Loadout、InputMapping、Timing 和 JudgementConfig。失败返回 typed
diagnostic，不产生半准备会话。准备后的运行期不允许修改 requirement 集合、prepared grace、
仲裁策略或 Interface 投影。

## 2. 候选类型

```cpp
struct InputEvent {
    Tick observationTime;
    InputDomain domain;
    Action action;
    ChannelOrPosition position;
    Amount amount;
    SourceIdentity source;
    std::uint64_t sequence;
};

struct JudgementRequirement {
    RequirementId id;
    TimeInterval interval;
    DomainId domain;
    Action requiredAction;
    PatternRef pattern;
    MeasureSpec measures;
    GripPolicy grip;
    std::optional<Tick> preparedGrace;
};

struct JudgementFact {
    RequirementId requirement;
    Phase phase;
    Outcome outcome;
    Grade grade;
    Tick error;
};

struct JudgementResult {
    JudgementFact fact;
    std::uint64_t eventSequence;
};

struct JudgementIdentity {
    Digest engine;
    Digest ruleset;
    Digest chart;
    Digest session;
};
```

实际公开命名、整数宽度、序列化和异常边界必须在实现批次中与现有 Core `Result<T,E>` 规范一并
冻结。第三方类型、JSON DOM、SDL、OpenGL、World 和 EnTT 不得出现在公共头文件。

## 3. 会话操作

候选操作为 `prepare`、`submit`、`advance`、`snapshot`、`seek`、`reset` 和只读查询。时间只能
单调推进；显式 Seek/Reload 通过事务替换状态。实时输入与 Replay 输入必须进入同一提交路径。

`snapshot` 包含 Spec §10 的完整状态；恢复后同一输入尾部必须得到相同 Fact、Score、Combo 和
Statistics。`maxSeekLatency` 是能力声明，会话可以给出更小的目标。

## 4. 诊断

最小诊断类别：`identity_mismatch`、`unsupported_capability`、`invalid_requirement`、
`invalid_grace`、`range_overflow`、`budget_exceeded`、`time_regression`、`ownership_conflict`、
`non_terminating_program` 和 `replay_format_error`。诊断必须指出分量、requirement 或字段，
但不得把不支持能力静默解释为旧 RequirementKind。

## 5. 兼容与版本

Judgement semantic version 与 SDK API version 独立。Replay 头部保存四个 identity 分量、格式
版本、事件数和字节预算。缺少所需 engine/ruleset capability 时稳定拒绝。公开 C++ preview 可以
在 Stage 7A 增加 additive 类型；破坏性变化必须在 ADR、Spec、Replay 版本和版本门禁中同时记录。

## 6. 非目标

本文不实现任何 engine 模块，不定义稳定 ABI 符号，不承诺当前 Player/Studio 已调用判定内核，
不把研究 spike 的测量值变成公共常量，也不开放运行时脚本或宿主回调。
