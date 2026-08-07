# ADR 0010：规范谱面格式与简易导入格式

日期：2026-07-16

状态：历史决策；方案 B 保留部分已被 ADR 0035 取代

## 背景

> 2026-08-05：ADR 0035 已在阶段 2A.1 移除方案 B。本 ADR 以下内容仅保留为阶段 1 的历史设计事实。

Cuexis 需要稳定、可迁移并适合 Studio 编辑的正式谱面格式。但在 Cuexis Studio 尚未完善时，开发者仍需要能够方便地手写谱面。让两套格式直接进入 Runtime 会造成两条校验、编译和兼容路径，并迫使所有后续功能同时维护两种表达。

## 决策

`cuexis.chart` 是唯一规范格式和唯一内存 `ChartDocument` 模型。它使用 UUIDv7 稳定对象 ID、结构化引用、单模板继承和显式扩展命名空间。

`cuexis.chart.simple` 是早期手写谱面的受限兼容导入格式。它可以使用可读字符串 ID 和 `domain:id` 简写引用，但必须先由 `SimpleChartImporter` 转换为标准 ChartDocument，之后才能校验、编译和运行。

格式通过顶层 `format` 和各自的 `version` 明确识别，不允许启发式猜测。

方案 B 必须包含 `chartId`。字符串 ID 按以下方式确定性转换：

```text
对象 UUID = UUIDv5(chartId, "object:" + readableId)
模板 UUID = UUIDv5(chartId, "template:" + readableId)
```

方案 B 是方案 A 的严格功能子集。未知 B 字段产生警告，保存在 `extensions["cuexis.simple.unknown"]` 中，但不影响 Runtime。方案 B 不得拥有只在自身存在的运行时语义。

Cuexis Studio 成熟后冻结方案 B，不再为它增加新特性。后期保留只读导入器还是完全移除，由新的 ADR 决定。

## 备选方案

### Runtime 同时支持 A 和 B

拒绝。它会让格式兼容逻辑扩散到 Runtime、World 和各 System，并形成长期双实现。

### 每次导入 B 时随机生成 UUID

拒绝。同一文件重新加载后对象身份会变化，破坏日志定位、引用缓存和 Studio 导入结果的稳定性。

### 根据字段形状自动判断格式

拒绝。随着两个格式演进，启发式判断会产生歧义，并使错误文件进入错误的解析路径。

### 让 B 与 A 同步支持所有新功能

拒绝。B 的目的只是早期手写便利，不应成为第二套需要永久维护的规范格式。

## 影响

```text
新增 CanonicalChartLoader 和 SimpleChartImporter 的明确边界
ChartDocument、ChartRuntime 和 RuntimeSession 只理解方案 A
需要 UUIDv7 生成和 UUIDv5 确定性映射能力
Importer 必须输出结构化转换诊断
Studio 导入 B 后默认保存为 A
格式测试需要覆盖确定性 ID、引用转换、未知字段和不支持功能
```

## 后续风险

方案 B 若在早期加入过多便利语法，冻结成本会快速增加。每次扩展 B 前必须确认该能力能无损转换为现有方案 A，并且确实是手写谱面当前所必需的。

规范格式的对象容器布局、外部谱面引用解析和扩展处理器注册机制仍需在具体格式文档中进一步定义。
