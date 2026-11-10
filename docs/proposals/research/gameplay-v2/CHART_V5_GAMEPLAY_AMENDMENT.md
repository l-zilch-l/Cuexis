# Chart v5 Gameplay 修订案

状态：candidate；对 Chart v5 当前候选定义的研究性修订，未接受，未实施

更新日期：2026-10-02

本稿是 Chart v5 Gameplay 增量候选；资源生命周期、solver、迟到事件、Fact 因果总序、
Ruleset 事务和 early/late 表现桥接的细化以 [Gameplay V2 初步设计方案](PRELIMINARY_DESIGN.md)
为准。路线决定已经固定为 `gameplay.version = 2`、允许 CXC v1 独立
`gameplay-graph` Playback entry、Release/tail、early/late bridge 和单一
`capacity=1` exclusive resource；两者仍是 candidate，未接受、未实施。

## 1. 修订目标

Stage 7 从 Chart v5 Core/Packed candidate 开始，因此 Gameplay V2 不应再引入第二套平行谱面格式。本文把 V2 语义直接落到 Chart v5：Chart v5 继续负责时间轴、CXT v2 import、Presentation 关联和 Packed lowering，同时增加可验证的 Gameplay semantic 区段。

当前 Chart v5 的 CXT v2 合同已经能表达局部 Requirement、Prototype、Pattern 和有限 Repeat；缺口在于跨 Requirement 关系、原子提交、因果 Fact 和判定闭包没有正式字段。修订重点是补这些字段，不改变 Chart v5 的基础容器、CXC v1 载体或 Packed 物理编码方向。

## 2. Chart v5 顶层增量

在现有 `format: "cuexis.chart"`, `version: 5` 和 `cxtImports`/`cxtInstances` 之外，增加：

```json
{
  "gameplay": {
    "version": 2,
    "requirements": [],
    "coordination": {
      "resources": [],
      "groups": [],
      "relations": []
    },
    "extensions": {}
  }
}
```

其中：

- `requirements` 是 Chart v5 inline Requirement 的 typed records；CXT v2 展开结果也必须进入同一 canonical 表。
- `resources` 声明接触点、按键槽位、判定轨道或其他可独占资源。
- `groups` 声明 binding/cardinality/atomicity 关系。
- `relations` 声明 exclusive、temporal、quota 等跨 Requirement 约束。
- `extensions` 只接受已注册的 capability namespace；未知字段稳定拒绝。

这些字段是语义源数据，不允许引用 Presentation parent、运行时 Entity ID、数组下标或宿主对象。

## 3. CXT v2 的新边界

CXT v2 继续作为 Chart v5 的 source module。它可以生成局部 `Requirement` 和 local relation，但不能独立生成 Chart v5 外部可见的全局资源或 Coordinator。CXT 展开后必须：

1. 解析成 Chart v5 canonical Requirement records；
2. 使用完整 `(chartEntryId, invocationId, emissionPath, localRequirementId)` 生成稳定身份；
3. 将跨 emission 关系写入 Chart v5 `gameplay.coordination`；
4. 在 prepare 前完成数量、时间、预算、量程和 solver 上界验证。

CXT v2 的 Animation 部分仍只生成 Presentation Graph；Requirement 内不再保留任意 `effects`。命中反馈通过 Chart v5 的 FactBinding/Effect Graph 关联表现目标。

## 4. Packed Chart 增量

Packed Chart 仍是 Chart v5 的物理编码，不成为新的语义版本。其 typed semantic sections 需要能够无损表示：

- Canonical Requirement records；
- resource/group/relation tables；
- prepared timing/geometry/grace values；
- capability and solver declarations；
- semantic identity and source/build provenance separation。

Packed decoder 不执行 CXT、Pattern、脚本或 Coordination solver。solver 只在 compile/prepare 阶段使用；Playback 读取已验证的 canonical coordination tables。

## 5. Identity 和回放

Chart v5 的 `semanticIdentity` 必须覆盖 Gameplay semantic projection，包括 Requirements、CoordinationGraph、量化值、判定域/动作、引用的 engine table 和 Ruleset projection。Presentation resources、source order、CXT 注释和 Packed 压缩选择不进入 judgement identity。

Replay 绑定：

```text
engine identity
ruleset identity
chart v5 gameplay semantic identity
session judgement identity
normalized observation stream
```

换皮肤、表现对象 parent、CXT source 排版或 Packed 字典顺序不应失效 Replay；修改 Gameplay relation、判定区域、窗口、grace 或 Coordination 策略必须改变对应判定身份。

## 6. 与 Stage 7 的关系

Stage 7A 只需要 Chart v5 `gameplay.version = 2` 的最小子集：Tap、Hold，以及同一
Requirement 内显式声明的 Release/tail phase；再加一个 `capacity=1` exclusive resource、
Fact ledger、typed Ruleset transaction 和 early/late Presentation bridge。Chord、交替、quota、
handoff、capacity>1、连续轨迹、方向输入和全局最优 solver 作为后续 capability，不应通过修改
已发行字段含义隐式加入。

Stage 7 的 CXT v2/Packed 消费路径必须验证：

- source Chart v5 与 Packed Chart 展开得到相同 Canonical Gameplay Graph；
- CXT v2 invocation 与 inline Requirement 使用同一 semantic contract；
- relation 顺序、数组重排和 Packed 压缩不改变结果；
- 不支持能力、越界、非终止展开、solver 超预算和悬空引用稳定拒绝。

## 7. 兼容和迁移

旧 Chart v5 candidate 若没有 `gameplay` 区段，不能猜测其跨 Requirement 语义。可在显式迁移器中把已有 Requirement 提取到 `gameplay.requirements`；无法判断资源共享、表现几何是否为判定域或对象 parent 是否代表 gameplay relation 时输出 `ambiguous`。

Chart v4/CXT v1 仍按现有回退路径处理。迁移到修订后的 Chart v5 必须先生成 Chart v5 source，再经过同一 canonical compile/pack 流程；Playback 不在运行时做隐式迁移。

## 8. 不修改的部分

- CXC v1 ZIP32 Stored 容器形状；
- CXC v1 manifest 可以在既有容器内区分 `packed-chart` 与独立 `gameplay-graph` Playback entry；
  两者共享 Canonical Gameplay Graph、prepared judgement identity 和运行时 kernel；
- Chart v5 Packed 的物理压缩目标和独立预算；
- CXT v1/v2 的版本分界；
- 运行时脚本、逐帧脚本回调、IO、随机数和动态 Requirement 生成禁令；
- Stage 7A 之前 Chart v4 的兼容回退。
