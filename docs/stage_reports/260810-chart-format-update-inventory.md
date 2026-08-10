# 2026-08-10 Stage Chart Format Update：CFU-A 盘点报告

状态：仓库内盘点和 CFU-A 用例清单完成；CXT v1/播放前参数/Template Binding 子决策已接受，
ADR 0038 其余门禁仍待项目所有者确认

## 1. 结论

仓库当前只有 `cuexis.chart` canonical JSON，没有 CXC 文件、Schema、Reader、Writer 或公共 API。仓库内共发现 9 个 Chart fixture：v1 五个、v2 一个、v3 三个。现有运行链路已经把 ChartDocument、ChartRuntime、PlaybackSession 和外部 consumer 分开；Stage Chart Format Update 可以在不改变该边界的前提下引入新格式合同。

当前不能把 `extensions` 视为动画格式的临时旁路，也不能把 `engine/animation` 的目标能力反推成已经存在的落盘字段。

## 2. 仓库内 Chart fixture

| 版本 | 文件 | objects | behaviors | templates | 用途 |
| --- | --- | ---: | ---: | ---: | --- |
| v1 | `assets/charts/stage1a_example.cuexis.chart.json` | 3 | 0 | 1 | canonical 基础示例 |
| v1 | `assets/projects/stage1b_project/assets/charts/stage1b_example.cuexis.chart.json` | 4 | 0 | 0 | 资源生命周期项目 |
| v1 | `assets/projects/stage1c_project/assets/charts/stage1c_example.cuexis.chart.json` | 3 | 3 | 0 | typed Behavior 与相机 |
| v1 | `tests/fixtures/stage1b_project/assets/charts/stage1b_example.cuexis.chart.json` | 4 | 0 | 0 | 测试项目镜像 |
| v1 | `tests/fixtures/stage2_migration_v1.cuexis.chart.json` | 3 | 3 | 1 | v1 -> v3 迁移输入 |
| v2 | `assets/projects/stage1d_project/assets/charts/stage1d_example.cuexis.chart.json` | 3 | 3 | 0 | 主音乐与 Audio |
| v3 | `assets/charts/stage2_example.cuexis.chart.json` | 1 | 1 | 0 | v3 canonical 示例 |
| v3 | `assets/projects/stage3_project/assets/charts/stage3_example.cuexis.chart.json` | 2 | 1 | 0 | Portable Presentation smoke |
| v3 | `tests/fixtures/stage2_migration_v3.golden.cuexis.chart.json` | 3 | 2 | 0 | 迁移 golden |

盘点只统计仓库源码树中的 `.cuexis.chart.json`，并排除了构建输出、外部依赖和版本控制目录。仓库外是否存在必须继续读取的 Chart 资产，仍需项目所有者明确确认；本报告不把“未在仓库中发现”解释为“仓库外为空”。

## 3. 现有格式和消费边界

当前规范与实现入口：

```text
schemas/cuexis.chart.v1.schema.json
schemas/cuexis.chart.v2.schema.json
schemas/cuexis.chart.v3.schema.json
    -> JSON Schema artifact tests
CanonicalChartLoader
    -> typed ChartDocument
ChartCompiler
    -> immutable ChartRuntime
PlaybackSession / RuntimeSession
    -> FrameSnapshot、capability、digest 和资源事务
cuexis_chart_validator
    -> 结构与语义校验 CLI
cuexis_chart_migrator
    -> v1/v2 到 v3 的显式迁移 CLI
```

主要入口见 `engine/chart/src/canonical_chart_loader.cpp`、`engine/chart/src/chart_runtime.cpp`、`engine/chart/src/chart_migrator.cpp`、`tools/chart_validator/main.cpp` 和 `tools/chart_migrator/main.cpp`。Player、headless Playback 和 `tests/external/consumer.cpp` 都通过 canonical `cuexis.chart` 输入验证公共路径；没有第二条 CXC 或动画 JSON 解析路径。

`engine/animation/CMakeLists.txt` 当前只有未接入构建图的 `INTERFACE` target，说明动画运行时仍属于阶段 4，不应在 CFU-A 直接实现。

## 4. 与动画设计的格式缺口

`docs/ANIMATION_MIXING.md` 已冻结运行时求值顺序、Layer、BlendGroup、Override/Additive、HostOverride 和绝对采样原则，但现有 Chart v3 没有以下持久化合同：

```text
AnimationClip/Layer/BlendGroup 的稳定 ID 和保存作用域
Clip 到 Object/Template 的绑定、循环和局部时间
Property ID、property mask 与可写类型的文件表达
动画引用的资源 identity 和 capability 声明
动画定义与 Behavior 事件的冲突/优先级声明
编辑源、可交换 package、运行时缓存之间的版本关系
```

这些缺口是 Stage Chart Format Update 的设计输入，不是本报告对新 Schema 的预先决定。

## 5. CFU-A 必须覆盖的最小用例

| 用例 | 要验证的格式问题 | 运行时预期 |
| --- | --- | --- |
| A1：现有 v3 原样 round-trip | 旧格式保留与规范化 | FrameDigest/语义 identity 不变 |
| A2：单对象 Transform Clip | Clip、Property、时间域最小表达 | 任意 seek 得到同一 typed 状态 |
| A3：Opacity + Material 参数 | 连续属性与资源引用边界 | 不把资源选择当数值插值 |
| A4：同一 Clip 两个绑定 | 绑定作用域、mask、实例 ID | 遍历顺序不影响结果 |
| A5：循环、Stop、负 Beat | 局部/全局时间和 discontinuity | 不依赖上一帧混合结果 |
| A6：缺失资源或 capability | prepare/preflight 失败边界 | 不发布半帧或静默降级 |
| A7：未知可选/必需扩展 | 扩展保留与稳定拒绝 | 可选数据 round-trip；必需能力失败 |
| A8：超预算与损坏输入 | 安全边界和诊断路径 | 在资源获取/World 发布前失败 |

## 6. 待确认事项

除已接受的 CXT/参数/Template Binding 子决策外，ADR 0038 仍对以下产品选择提出具体建议，
并需要项目所有者确认：

```text
CXC 是新的顶层 format，还是 cuexis.chart 外层 package
是否需要一个文件携带多个 Chart/Asset Index/资源 blob
编辑源是否允许 JSON，运行交换是否使用二进制或归档
旧 v1/v2/v3 的支持窗口和是否提供 v3 -> CXC 落盘迁移
资源是仅引用 Asset Index，还是允许受预算约束的嵌入 payload
Stage 4 首版是否需要局部 Clip Beat、循环、跨对象绑定和模板内 Clip
FrameSnapshot/FrameDigest 是否因动画字段增加而升级版本
```

在这些选择被 ADR 整体接受前，实施只能停留在 fixture、伪格式和诊断合同，不得提交公共格式字段。

### 2026-08-10 子决策补充

项目所有者已接受以下 CFU-B 子决策：

```text
.cxt 是 UTF-8 JSON 文件，format=cuexis.animation-template，version=1
每个 .cxt 恰好导出一个声明式 local-Beat Animation Template
Chart 使用 animationTemplateImports[] 和 cuexis.animator.templateBindings[] 引用它
宿主参数仅在 prepare 前冻结，并只改变显式允许的 number、durationScale 和 weight 字段
```

CXT 文件合同已写入 [CXT_FORMAT.md](../CXT_FORMAT.md)，Chart v4 字段与 lowering 已写入
[CHART_V4_FORMAT.md](../CHART_V4_FORMAT.md)，CXC 容器与闭包已写入
[CXC_FORMAT.md](../CXC_FORMAT.md)，并同步到 ADR 0038。该子决策不等于 ADR 0038 整体接受，
也不解除 CFU-C 生产 Schema/Reader/Writer 门禁。

## 7. 下一步

CFU-A 的仓库内证据和用例清单已具备。CFU-B 已新增 ADR 0038、`docs/CXC_FORMAT.md`、
`docs/CHART_V4_FORMAT.md`、`docs/CXT_FORMAT.md` 和 `docs/examples/chart_format_update/` 下的
CXC/Chart/CXT 正反例。下一门禁
是项目所有者接受 ADR 0038 其余合同；在整体接受前不修改 `schemas/`、`engine/chart` 公共头或默认
项目 fixture。
