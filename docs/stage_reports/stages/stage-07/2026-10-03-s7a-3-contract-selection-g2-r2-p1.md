# S7A-3 未闭合合同组合裁定：G2 + R2 + P1

日期：2026-10-03  
范围：S7A-3 第二半尚未实现的 prepared grace、资源 claim / ownership prepare 计划、以及 REF0 / Packed 首次写入边界。

## 裁定

本轮 owner 选择组合 **G2 + R2 + P1**，作为后续 S7A-3 实现的合同方向：

| 标识 | 选择 | 要求 |
| --- | --- | --- |
| G2 | 完整解析并冻结 `preparedGrace` | 显式值、继承值和 prepare 前冻结的默认声明都进入同一解析流程；最终值在 prepare 后只读；资源状态机的 `declaredGapGrace` 仍是独立字段，7A 只允许零值 |
| R2 | 生成不可变资源计划 | prepare 解析资源引用、claim policy、intent、唯一 slot 和稳定排序依据；运行期只执行已准备的确定性 policy，并在规范阶段提交实际 owner / lease |
| P1 | 新 candidate revision 加新必需 semantic section 组 | 新 Gameplay v2 语义必须与 Foundation `REQ0` / `CNS0` 可区分；旧 Reader 对未知必需 section / capability / requirement kind 稳定拒绝；REF0 物理字段、编号和编码在首次 Packed 写入时闭合 |

这是一项**合同选择记录**，不是实现完成记录，也不表示 S7A-3 或 Stage 7A 已完成。

后续来源、phase 消费、资源仲裁、Packed 表示与验收组织的讨论见
[S7A-3 后续合同讨论与决策登记](2026-10-03-s7a-3-decision-register.md)。
其中 T4 + K4 是当前推荐，尚未取得 owner 明确接受；该登记不修改本文已接受的 G2 + R2 + P1。

## G2：prepared grace

`preparedGrace` 是 prepare-time 的最终只读值。其解析顺序固定为：

1. 读取声明的解析 policy 和候选来源；
2. 对显式或继承候选执行精确量化；
3. 检查 `allowChartGrace` 与声明的 `[minimumCanonical, maximumCanonical]`；
4. 检查量化结果能否精确表示为 canonical `TickSpan`；
5. 生成最终 `PreparedGrace`，并把生效 policy 写入 prepared judgement identity。

本组合下的具体边界如下：

- 显式 chart 值只有在 `allowChartGrace` 允许时才可覆盖；
- 没有显式值时使用 prepare 前已冻结的继承或默认声明；声明缺失不能静默变成零；
- 量化采用既有精确整数规则，半数取偶；溢出、负值、窄化失败和范围越界稳定拒绝，不做钳制；
- `sticky` 与 `observe` grace override 稳定拒绝；
- 来源、原始字段和继承链进入 content identity / diagnostic context；最终 grace 与生效 policy 进入 judgement identity；
- `preparedGrace` 不等同于资源状态机的 `declaredGapGrace`，不自动开启 `gap`、`handoff` 或连续接触恢复；
- 后续运行时批次必须明确每个允许的 judgement phase 如何消费该只读值；在该消费合同闭合前，不得把它解释为任意的运行时宽限。

因此，G2 保留非零 prepared grace 的可配置性，同时把资源 gap / handoff 语义留在 7A 已冻结的零值边界内。

## R2：资源 claim / ownership prepare 计划

`resolveResourceClaims` 的输入必须能表达完整的 prepare 关系，而不能只携带一个容量数字和未关联的 intent 列表。实现时应至少消费：

- 资源引用与资源声明；
- 每个 Requirement 的 `ResourceClaimDeclaration`；
- `ResourceClaimIntent`、claim policy 和稳定 `claimKey` 声明；
- 已解析的 `PreparedGrace`；
- 单一 `capacity = 1`、唯一 slot 和终止后 `free` / `terminal` 策略。

输出是不可变的 prepared resource plan，至少包含：

- 资源引用到唯一 prepared slot 的绑定；
- `observe`、`consume`、`claim` 的已验证语义；
- 参与候选排序的稳定 policy / claim identity；
- 需要在运行期由引擎生成的 lease / contact identity 的生成规则；
- 非法 capacity、owner 集合、并列 slot、非零 resource gap grace、handoff / `gap` 和 sticky / observe 覆盖的稳定拒绝结果。

职责边界固定为：

- prepare 负责解析、验证、排序依据和唯一性 / 可执行性证明；
- runtime coordinator 只执行 prepared deterministic policy；
- 实际 owner、lease 和最小 contact handle 由引擎在规范阶段分配；
- `observe` 只产生 Observation，不占用 slot、不生成 lease、不改变 owner；
- 多个 Requirement 引用同一资源不自动构成 prepare 冲突；冲突在运行期按 prepared policy 仲裁；
- `capacity > 1`、owner 集合、并列 slot、handoff 和连续接触迁移继续稳定拒绝。

这使资源计划成为 S7A-3 的确定性输出，而不是把运行时状态提前伪造进 prepared graph。

## P1：REF0 / Packed 首次写入

P1 选择“新 candidate revision + 新必需 semantic section 组”的隔离方式。实现必须满足：

1. Foundation `REQ0` / `CNS0` 的既有语义不被重解释；
2. 新 Gameplay v2 entry / section 不能被旧 Reader 当作旧 kind 接受；
3. 新 section 的必需性、未知 section / capability / requirement kind 的拒绝路径必须稳定；
4. REF0 只承载 Spec §3.8.6 判定闭包中的引用；纯表现引用进入 manifest / presentation closure，diagnostic map 不进入 REF0 或 identity；
5. REF0 行的 kind 编号、字段布局、section 名称、candidate revision 数值、长度编码和端序必须在首次 Packed 写入前作为同一合同闭合；
6. 内部 `ReferenceKind` 枚举值不得未经明确登记直接充当 wire 编号；
7. Reader / Writer、file / memory、未知必需项、重复项、悬空引用、计数溢出和旧 Reader 拒绝都必须有正反 fixture。

当前记录只选择隔离策略，不预先发明具体 revision 数字、section 名称或 wire 编号。那些字段属于 P1 的实现消费面，必须在同一批次一次性登记，避免出现“语义已改、物理编码仍靠默认值”的半闭合状态。

## 实现顺序与停止条件

后续实现应按以下顺序推进：

1. 先扩充 `GraceResolutionInputs` 和资源 resolution inputs，使所有声明来源都能被显式表达；
2. 为 G2、R2 补齐 typed 正例、边界例和稳定拒绝例；
3. 在 REF0 首次写入前登记 P1 的 revision / section / field / encoding 表；
4. 再实现 Packed Reader / Writer 和跨工具链 golden；
5. 每一步保持 prepare 原子性，失败不得发布半准备 chart 或改变 active publication。

出现以下任一情况时必须停止并重新裁定：需要引入未声明的默认值或哨兵、需要把 `preparedGrace` 当作 resource gap grace、需要用数组顺序或字符串字典序代替 claim policy、需要让旧 Reader 静默接受新语义、或需要把尚未登记的内部枚举值写成 wire 编号。

本记录不关闭状态预算 `INCOMPLETE GATE`，也不关闭 S7A-3 的整体完成门禁。

## 2026-10-04 后续裁决说明

以上保存 2026-10-03 选择时的方向与停止条件。之后 owner 授权自主裁决并选择“仅文档”；
完整组合 **G2-S2 + G2-T4 + R2-K4 + P1-W1 + E1** 现已完成设计收口，不再等待 T/K 或物理值选择。
现行语义见 [Spec §3.8.10–§3.8.12](../../../formats/GAMEPLAY_V2_SPEC.md)，
物理字段仅见 [Gameplay Capsule v2 format](../../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md)。
共享 REF0 保留 Foundation 表现资源，Gameplay judgement subset 才沿 §3.8.6 分区；
旧段落“REF0 只承载判定引用”不能用于删除 Foundation asset rows。
新增证据与未完实现门禁见 [设计收口报告](2026-10-04-s7a-3-design-closure.md)。
本说明不改写历史实施结果，不代表 S7A-3 实现关闭。
