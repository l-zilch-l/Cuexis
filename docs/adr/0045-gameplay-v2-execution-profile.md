# ADR 0045: Gameplay V2 execution profile for S7A-3 / S7A-4

状态：candidate

决策状态：本轮方案已选定；作为下一对话的实施输入，不冒充既往 owner 裁定或阶段验收

更新日期：2026-10-05

实现状态：本 ADR 的新增合同未实施

## 背景与授权

owner 要求给出完整、健全、可交给后续模型实施的方案，以消除 S7A-3 余项和 S7A-4 的语义空白。
此前只登记[方案比较](../proposals/gameplay-v2-acceptance/S7A-4_RECOMMENDED_DESIGN.md)，
不能据此决定迟到边界、contact、Fact ordinal、phase 依赖失败或 authoring 承载。
本次授权是设计和文档工作，不包括产品实现、提交、推送、发布或整阶段关闭。

## 选择

1. 采用组合 B，并要求独立 reference evaluator：L1+L2、H2、F1+F3、R1、S2+S1。
   直接可用的字段与边界归[补充 Spec](../formats/gameplay-v2-execution-profile.md)，不在 ADR 复制。
2. 采用逻辑 Tick 的缓冲提交与最多一次向后转发；迟到参数均为 span，不把现有 TickSpan
   `maxQueueHop` 强行解释为次数。观测时间保留，转发不能重开 Fact 或回填覆盖。
3. 身份使用结构字段；资源 immutable draft 先校验后 seal 全 Tick。
   Fact 顺序保持已有三元组；逻辑 ordinal 用结构表示，物理 codec 留给 S7A-6。
4. 缺失的 phase target、atom binding 和独立实例 pair 必须显式，不允许从窗口/字段名猜测。
   为此新增 Capsule candidate revision 3；不在 revision 2 下添加字节或改写旧 golden。
5. authoring 使用严格的 candidate extension，inline 与 CXT 先 lower 到同一 owning typed source，
   然后共用 assembler/prepare；affine rank 只存在离线 authoring，不进入 runtime/Packed AST。
6. arm/deadline 包含性是执行硬门禁；可选状态数缺测继续独立登记。
   受限功能验收和 S7A-9 的生产容量接受分开，不能把缺测判通过。

## 未选方案与原因

hash-only runtime identity 会引入碰撞和不可审计 tie-break；不选。
dense ordinal 作为首版语义会混淆容器位置与逻辑因果；不选。
只实现 fast timers 没有独立 oracle，会让生产实现和测试共享同一个缺陷；不选。
沿用缺少 phase target 的 revision 2 并取窗口中点，会改变精确误差和未来 grade；不选。
将 authoring AST 带到运行期，或把未知源字段忽略，会绕开完全展开与 closure；不选。

## 影响与门禁

这是 candidate 的局部版本增量，不升级 chartVersion=5、gameplayVersion=2、packedVersion=1、
现有 SDK 或稳定 C ABI。revision 3 的结构 hash 使用新 domain separation；revision 1/2 保持。
当前执行 gate 只接受显式完整 profile；旧图可离线读取，但不得宣称兼容执行。
新协议名、详细诊断码、typed fields 必须先登记，再由后续批次实现。

本轮所有新增选择均已落入 Spec/ABI/Plan；本范围不保留“实施时任选”的语义项。
这不意味着 reference 证明、golden、capacity、hosted 或实设备证据已经存在。
若实施中发现本合同的真实矛盾，先给出最小反例与受影响条款并修订合同，不能静默改变语义。
S7A-5/6/7/9 的范围和 Stage 6 owner-only 版本门禁仍按各自计划执行。

同日后续[歧义复核](../stage_reports/stages/stage-07/verification/2026-10-05-s7a-3-4-ambiguity-audit.md)
补齐 partial advance、matcher/observer、真实 CXT、identity 与验收命令；选择仍由现行补充合同拥有。

## 对应权威

- [Gameplay V2 Spec](../formats/GAMEPLAY_V2_SPEC.md)：原有 T4/K4、总序与阶段边界。
- [执行补充 Spec](../formats/gameplay-v2-execution-profile.md)：本轮运行语义。
- [Author profile](../formats/gameplay-v2-author-profile.md)：本轮源字段和 lowering。
- [Capsule](../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md) §12：revision 3 物理字段。
- [Typed supplement](../api/gameplay-v2-execution-types.md)：表示、所有权、错误与生命周期。
- [历史实施交接](../archive/stage-07-planning/s7a-3-4-implementation-handoff.md)：原3/4顺序与验收门禁；当前排期归 [Stage 7计划](../stage_plans/active/stage-07/plan.md)。
