# Chart Format Foundation：Chart v5 前置基础

状态：future；Stage 5 与 Stage 6 之间的下一实施门禁

更新日期：2026-09-05

归档来源：[Chart v5 格式计划](../../active/chart-format-update-for-v5/plan.md)、
[Stage 5 计划](../../completed/stage-05/plan.md)、[CXT v1 格式合同](../../../formats/CXT_FORMAT.md)、
[CXC v1 格式合同](../../../formats/CXC_FORMAT.md)。

## 1. 阶段目标

在 Stage 6 之前解决当前 Chart 物理存储无法满足高密度谱面的风险，但不提前冻结
尚未成熟的完整 Judgement 语义。

本阶段交付：

```text
CXT v2 Core 的候选合同
Prototype / Instance / Parameter / Pattern
有限确定性展开
Packed Chart 物理编码原型
CXC Packed entry 设计
40,000 语义实体容量测试
16 MiB Packed Chart 门禁
JSON -> semantic model -> Packed 的等价证据
```

本阶段完成后，Chart v5 Foundation 可以被工具和 headless validator 使用，但不表示
Chart v5 已成为默认 Playback 格式，也不表示完整 v5 Judgement 已实现。

## 2. 与 Stage 6 的边界

Stage 6 采用双基线：v5 Core/Packed candidate 是主要开发和验证基线，以下内容是兼容
与回退基线：

```text
Chart v4
CXT v1
CXC v1
SDK 0.7.0
```

Foundation 必须为 Stage 6 提供可验证的 candidate artifact、容量报告和 Packed inspection
能力，但不切换默认 Writer、删除 v4 Reader 或改变 v4 的 FrameDigest。Stage 6 只加载
Foundation 已冻结并明确标记的 v5 subset；未覆盖的 v5 语义必须稳定拒绝。

## 3. 工作批次

### CFF-A：CXT v2 Core

冻结候选模型：

```text
Prototype
Instance
Parameter
Pattern
Repeat
Transform
Bind
Override
Finite Expansion
```

参数类型只允许：

```text
integer
number
rational
boolean
enum
vector
reference
```

参数不得改变 Component 集合、对象类型、父子关系、AssetId、引用目标或资源闭包。
CXT 不执行脚本、表达式、随机、上一帧状态、Judgement 内部状态或宿主回调。

### CFF-B：Packed Chart 原型

建立与 v5 语义等价的物理编码原型：

```text
magic / version
section table
StringTable
AssetTable
PrototypeTable
local index
Component mask
prototype / instance
Beat delta / varint
RationalBeat 无损还原
checksum
```

本阶段只要求能够保存和还原当前已冻结的 semantic subset。完整 Judgement 扩展
字段在 Stage 8 冻结。

### CFF-C：CXC entry 设计

冻结 CXC v1 内部的 Chart v5 发行映射：

```text
source entry
compiled entry
playback entry
source semantic identity
compiled semantic identity
Packed artifact identity
compiler profile
```

CXC 容器版本保持 v1。Foundation 不要求 Stage 6 立即发行 v5，但必须确保 Stage 8
不需要重新设计 CXC 容器边界。

### CFF-D：容量和安全门禁

建立：

```text
40,000 semantic entity fixture
Packed Chart <= 16 * 1024 * 1024 bytes
expanded decoded bytes budget
expanded event count budget
peak prepare memory budget
```

覆盖高重复 Pattern、低重复率输入、大量资源、递归 Pattern、整数溢出、Beat 溢出、
超展开数量和超 Packed bytes。

## 4. 明确不包含

- Chart v5 默认 Writer。
- Chart v5 正式 Playback capability。
- 完整 Input/Judgement。
- Slide、Flick、多指和高级判定要求。
- CXT v2 Animation Extension 的最终完整合同。
- Studio 正式支持。
- 运行时脚本。

## 5. 验收标准

- CXT v2 Core 的候选 Spec、正反例和展开顺序完成。
- 输入顺序、参数顺序和引用顺序不影响 semantic identity。
- Packed 原型能够无损还原合法 semantic subset。
- 40,000 个语义实体的 Packed artifact 不超过 16 MiB。
- decoded bytes、展开数量、峰值内存和编译时间均有独立预算。
- 递归、随机、任意表达式、checked arithmetic 溢出和超预算输入稳定失败。
- CXC v1 的 source/compiled/playback 分层可以被工具验证。
- Stage 6 的默认发行和兼容回退仍可保持 Chart v4/CXT v1/CXC v1；其主要开发路径切换为
  Foundation 已验证的 v5 Core/Packed candidate。
- owner acceptance 完成后，Stage 6 才能进入实施。

## 6. 交接

交给 Stage 6：

```text
Packed format candidate
CXT v2 Core candidate
CXC entry mapping
capacity fixtures
security budget
semantic identity rules
```

交给 Stage 7：

```text
Chart v5 可引用的 JudgementRequirement 边界
Pattern 生成结果的 requirement identity 规则
Replay identity 需要包含的 semantic identity
```

交给 Stage 8：

```text
Chart v5 Foundation artifact
CXT v2 Core contract
Packed Chart section contract
40k / 16 MiB test harness
```
