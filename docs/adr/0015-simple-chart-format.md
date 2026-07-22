# ADR 0015：方案 B 简易谱面格式

日期：2026-07-17

状态：已接受

## 背景

Cuexis Studio 完善前需要可手写谱面格式。方案 A 的 UUIDv7、结构化引用、Component Map、有理数对象和 JSON Patch 对人工编辑较冗长，但 Runtime 同时支持两套格式会造成长期双实现。

## 决策

方案 B 使用 `format: "cuexis.chart.simple"`，只作为受限导入格式。

Template、Behavior 和 Object 使用以可读 Simple ID 为键的对象映射。Simple ID 限制为小写 ASCII 模式 `[a-z][a-z0-9._-]{0,127}`。

Object 通过固定 `kind` 和 `transform`、`render`、`behavior` 等简写字段表达。v1 kind 只支持 note、element 和 decoration，不支持自定义 Component。

引用使用 `domain:id`。Beat 使用字符串整数、分数或十进制文本，并精确转换为方案 A 有理数。Transform 可以使用度数欧拉角，按 `Rz * Ry * Rx` 转换为规范 Quaternion。

方案 B Template 不继承、不组合，只提供单模板引用和递归字段覆盖；数组整体替换，null 删除可选字段。Importer 把结果转换为方案 A 根模板和 JSON Patch overrides。

Object 与 Template UUID 使用 chartId 命名空间的 UUIDv5 确定性生成。相同输入不得因键顺序、路径、时区或随机数产生不同规范文档。

方案 B v1 不支持 BPM Changes、Stops、speedChanges、跨 Chart 引用、Material Track、复杂 PropertyBinding 或新 Component。Studio 成熟后冻结，不再获得方案 A 新特性。

## 备选方案

### 直接手写方案 A

未选择为唯一早期路径。它最完整，但 UUID、结构化引用和 JSON Patch 会显著降低早期谱面编写效率。

### 使用 YAML、JSON5 或自定义 DSL

未选择。它会增加解析器、错误定位和格式维护成本；JSON 已经是项目基础格式。

### 让 Runtime 直接理解方案 B

拒绝。所有方案 B 内容必须先转换成方案 A，避免两套编译和运行语义。

### 让方案 B 支持任意 Component

拒绝。它会逐渐复制方案 A，并破坏冻结兼容格式的目标。

## 影响

```text
新增 docs/SIMPLE_CHART_FORMAT.md
SimpleChartImporter 需要确定性 UUIDv5、Beat 和欧拉角转换
方案 A cuexis.note v1 明确包含 beat
方案 B 测试覆盖键顺序无关、模板合并和未知字段诊断
Studio 导入 B 后默认保存 A
```

## 后续风险

递归字段覆盖必须由固定 Schema 驱动，不能实现成接受任意未知结构的通用合并器。未知字段只允许进入兼容扩展区，不能影响 Runtime。

冻结后仍需维护 Importer 的安全和兼容修复，但不得借修复名义加入新表现能力。

