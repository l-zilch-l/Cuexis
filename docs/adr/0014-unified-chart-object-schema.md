# ADR 0014：统一 Chart Object 与 Component Schema

日期：2026-07-17

状态：已接受

## 背景

Cuexis 的底层对象统一为 Entity。如果规范谱面格式继续为 Note、Element、装饰物和未来语义分别增加数组，Schema、迁移器和编译器会随类型数量持续膨胀，并与运行时 Component 组合模型背离。

规范格式还需要确定 beat 精度、模板最小能力、外部引用和扩展注册边界。

## 决策

方案 A 使用单一 `objects` 数组。每个 Object 通过 `components` 对象映射组合语义，同一 Component ID 最多出现一次，每个 Component 数据具有独立 version。Note 和 Element 使用 Tag Component，不使用平行对象容器。

Object ID 使用 UUIDv7，parent 作为 Object 结构字段，Component 编译前先建立完整 ID 和层级图。数组顺序不具有运行语义。

方案 A v1 的 Behavior Key 使用规范化有理数；v3 的 Behavior Event 继续使用同一 Beat 表示。Chart 编译器将谱面 Beat 通过 TimingMap 转换为 Runtime 的 `chartTimeMs`。

Template v1 只描述单 Object 原型，使用单继承和 JSON Patch `add`、`remove`、`replace`。v1 不支持 Template 子树。

v1 只允许当前 Chart 内的 object、template、behavior 引用和 AssetId 引用。跨 Chart Object/Template 引用返回 UnsupportedFeature。

扩展通过稳定命名空间注册 validate、migrate、compile 和 diagnostics 处理器。未知可选扩展保留并警告，未知必需扩展编译失败。

Timing 不包含 speedChanges。Entity 运动速度属于 Behavior，整曲播放倍率属于 AudioTransport 与 Timeline。

## 备选方案

### 按 Note、Element 和 Decoration 拆分数组

拒绝。每增加语义类型都需要扩展顶层 Schema 和编译分支，且方案 B 已经承担手写友好格式的职责。

### 使用 Archetype 表格保存对象

拒绝。该结构适合编译缓存，但不利于编辑、版本迁移、差异合并和诊断。

### 使用浮点数保存 Beat

拒绝。三连音和边界比较会引入二进制浮点误差，影响确定性编译。

### v1 支持跨 Chart 对象引用

拒绝。它会提前引入跨文档生命周期、循环依赖、加载顺序和错误恢复问题。

## 影响

```text
新增 docs/CHART_FORMAT.md
CanonicalChartLoader 只生成统一 Object 模型
SimpleChartImporter 把分类语法转换为统一 objects/components
Chart 编译器需要有理数 Beat 和确定性 ID 索引
测试需要覆盖顺序无关、Component 唯一性、Template 展开和扩展缺失
```

## 后续风险

Component 数据仍需要逐类 Schema。应优先实现 Transform、Renderable、Behavior 和 Tag，不能因统一容器而一次性设计所有未来 Component。

有理数 Beat 必须设置实现层面的数值上限和溢出检查，具体限制在实现数据类型时补充。

