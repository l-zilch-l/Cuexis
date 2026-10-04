# V2 设计审查矩阵与迁移路径

状态：candidate；研究提案，未接受，未实施

更新日期：2026-10-02

## 1. 当前设计缺陷到 V2 的处置

| 当前缺陷 | 后果 | V2 处置 | 是否新增核心语义 |
| --- | --- | --- | --- |
| Requirement 直接发 Fact | 跨 Requirement 关系无法原子表达 | Candidate + CoordinationCommit | 是 |
| Pattern 承担全局关系 | Chord/共享资源/交替被迫塞进 L3 | CoordinationGraph | 是 |
| Fact 排序不含输入因果 | 容器/实例顺序可能影响结果 | FactLedger causal total order | 是 |
| Effect 混入 Requirement | 表现资源污染判定身份 | FactBinding/EffectEvent 分离 | 是 |
| Hook 既像接口又像模块状态 | Interface 扩展压力和 identity 不清 | L2 visible parameter / L3 local state 二分 | 否，属于边界重写 |
| CXT 同时承担动画和 Gameplay 来源 | Runtime/source/closure 耦合 | Chart v5 `gameplay` 区段与 Presentation Graph 分离，CXT v2 只作 source input | 是格式语义 |
| Replay identity 与内容资源过宽 | 换皮肤使回放失效 | canonical judgement identity | 否，重用已有分层但改变绑定对象 |
| late input 语义未闭合 | 网络/设备差异不可复现 | late-event policy | 是 |
| 能力只用一个拒绝码 | 无法区分未知、未启用和预算不足 | capability registry 状态机 | 是 |

## 2. 迁移阶段

### M0：语义冻结前的双轨研究

保留当前 Gameplay I 工作稿作为历史基线；以同一批 Tap/Hold/连续/多接触/技能案例分别编译为当前模型和 V2 Graph。任何结果差异都必须标记为“语义变化”或“当前模型未定义”，不能用兼容措辞掩盖。V2 路线已固定为 `gameplay.version = 2`，因此 M0 不再比较版本方案，而是比较迁移结果与差异。

### M1：Canonical Graph 研究编译器

只实现 source-to-graph 的离线工具和 reference evaluator，不接入 Player。验证稳定 RequirementId、Coordination 交易、Fact 总序、closure 和 semantic diff。

### M2：Stage 7A 最小闭环候选

先选 Tap/Hold/Release 和单一 exclusive resource；禁止在同一批次加入所有高级玩法。Replay 使用规范化 Observation，Ruleset 使用 typed state transaction。通过后再逐项加入 binding/quota/temporal。

### M3：格式与包边界

扩展 CXC manifest entry kind、`gameplay-graph` 与 `packed-chart` Playback entry 和 ruleset package。两种 Playback entry 都只消费 compiled graph projection，并进入同一 prepare/Judgement path；Studio 才消费 source module。完成 v4/v5/CXT 到 V2 的显式迁移器和 ambiguous 报告。

### M4：能力选入与旧路径共存

未纳入 Stage 8 的能力保持 candidate capability 和稳定拒绝。旧 Chart v4/v5 继续走既有路径；任何自动迁移必须显式生成 V2 graph 并保存 source map，不能在 Playback 中隐式猜测。

## 3. 验收证据

每项 V2 能力必须同时提供：

- reference evaluator 与独立实现的逐 Fact 对照；
- 输入/候选/提交顺序置换测试；
- snapshot/seek/replay 等价性；
- source reorder 与 pack/unpack semantic identity 稳定性；
- capability、预算、截断、溢出、非终止和未知引用负例；
- 当前模型的差异报告，明确“旧模型未定义”与“V2 有意改变”。

## 4. 实施前仍需细化的问题

1. Stage 7A 只启用 `greedy_v1` coordinator policy；有限最优 solver 属于后续 capability，需独立 fuel/identity 合同。
2. `reopen_uncommitted_window` 的适用范围和网络输入的最大等待界限。
3. Canonical Graph 的规范编码与 source map 外置 sidecar 规则。
4. Fact correction 是否需要独立的审计工具，还是仅由 Ruleset ledger-derived state 消化。
5. CXC v1 manifest 的 `entryKind`、closure/budget 和 stable reject 细节；不需要重开 CXC v2 载体决策。

这些问题不应通过默认值隐式决定；在 ADR 接受前保持 candidate/blocked 语义，并用最小 reference slice 测量。
