# 任务 2：Chart/CXC parse-once 实施计划

状态：completed

更新日期：2026-08-31

所属计划：[260830-followup 维护计划](plan.md)

完成报告：[2026-08-31 任务 2 完成报告](../../../stage_reports/reviews/260830-followup/2026-08-31-task-2-chart-cxc-parse-once.md)

本计划承接 Full Review 对 CH-05、CH-06、CX-10 的 characterization 和 ownership/parity 安全切片。
目标不是简单减少 `json::parse` 调用，而是在保持已接受行为合同的前提下，消除同一输入在 Chart prepare
和 CXC package 路径中的重复解析。

## 1. 当前问题边界

### 1.1 `ChartLoader` 双重解析

`ChartLoader::load` 先解析 JSON 读取 `format`，随后调用 `CanonicalChartLoader::load(jsonText)`，
后者再次解析相同文本。

实现入口：[chart_loader.cpp](../../../../engine/chart/src/chart_loader.cpp)。

### 1.2 Chart v4 prepare 双重解析

`PlaybackSession::loadDocumentStage` 先调用 `ChartV4Loader::isV4`，判断成功后再调用
`ChartV4Loader::load`。两个入口各自执行完整 JSON parse。

实现入口：[playback_session.cpp](../../../../engine/playback/src/playback_session.cpp)。

`ChartV4Loader::isV4` 和 `ChartV4Loader::load` 的解析入口位于
[chart_v4_loader.cpp](../../../../engine/chart/src/chart_v4_loader.cpp)。

### 1.3 Chart v4 Resolver 重复解析

`ChartV4Resolver::resolve` 重新解析 `canonicalSource.canonicalText`，参数替换后序列化 JSON；
`makeConcreteChart` 再调用 `CanonicalChartLoader::load` 解析序列化结果。

实现入口：[chart_v4_resolver.cpp](../../../../engine/chart/src/chart_v4_resolver.cpp) 和
[chart_v4_resolver.cpp](../../../../engine/chart/src/chart_v4_resolver.cpp)。

### 1.4 CXC entry Chart 双重解析

CXC package 构建时，`isV4Chart` 先解析 entry Chart，随后 `ChartV4Loader::load` 再解析同一文本。
CXC 同时还负责资源闭包和 CXT import 验证，不能在 CXC 内复制 Chart parser。

实现入口：[cxc_package.cpp](../../../../engine/cxc/src/cxc_package.cpp) 和
[cxc_package.cpp](../../../../engine/cxc/src/cxc_package.cpp)。

### 1.5 Filesystem source 的重复使用

Filesystem `PlaybackSource` 为发现 CXT imports，当前会先调用 `ChartV4Loader::isV4`，再调用
`ChartV4Loader::load`。CXC package 已完成 Chart 验证后，`PlaybackSource` 仍保存文本，后续
Playback prepare 可能再次构建 Chart typed state。

该跨操作边界是否继续优化，必须先通过 B5 的 ownership 和性能数据决定，不能预先引入大对象缓存。

## 2. 总体设计

在 Chart 模块内部建立已解析输入上下文：

```text
原始文本
  -> json::parse 一次
  -> format/version inspection
  -> typed loader / projection / resolver 共享同一个 json::Value
```

建议新增未安装的内部类型，放在 `engine/chart/src/` 或未安装的
`cuexis/chart/detail/` 中：

```cpp
struct ParsedChartInput {
    json::Value value;
    std::string_view originalText;
    ChartInputVersion version;
};
```

设计约束：

- `json::Value` 不得出现在安装后的公共 Playback header。
- `engine/animation/` 不得接触 JSON、CXC 或 CXT 解析。
- 不能复制第二套 Chart parser。
- 公共字符串入口保持兼容，仅负责 parse 后转入内部入口。
- CXC 只负责 ZIP、manifest、project、Asset Index、closure 和资源引用验证。
- Chart 字段语义、版本路由和 typed reader 仍由 `cuexis_chart` 负责。

## 3. 实施批次

### B0：建立运行基线

先扩展 characterization，不改变生产行为。

已有测试入口：

- [tests/chart/parse_characterization_tests.cpp](../../../../tests/chart/parse_characterization_tests.cpp)
- [tests/cxc/parse_characterization_tests.cpp](../../../../tests/cxc/parse_characterization_tests.cpp)
- [tests/playback/preparation_characterization_tests.cpp](../../../../tests/playback/preparation_characterization_tests.cpp)

记录以下基线：

- 每条入口的 parse count。
- 成功、失败和预算边界输入的 diagnostics fingerprint。
- severity、`code`、`message`、`fieldPath`、context 和排序。
- Chart canonical bytes。
- `PreparedSemanticIdentity` 和 CXC package identity。
- FrameDigest v1-v3。
- candidate/active rollback。
- 16 MiB ownership、最大合法输入和峰值内存。

parse 统计使用内部 test-only probe 或 thread-local recorder，不修改公共 API，也不引入永久全局
计数器。静态源码计数只能作为辅助，不能替代运行时证据。

### B1：消除 `ChartLoader` 双重解析

涉及文件：

- `engine/chart/src/chart_loader.cpp`
- `engine/chart/src/canonical_chart_loader.cpp`
- `engine/chart/src/canonical_chart_loader_internal.hpp`
- `tests/chart/parse_characterization_tests.cpp`
- 必要时更新 `tests/chart/chart_loader_tests.cpp`

目标结构：

```text
ChartLoader::load(text)
  -> parse(text)
  -> inspect format/version
  -> detail::loadCanonicalValue(parsed, limits)
```

`CanonicalChartLoader::load(text)` 继续保留，但改为 parse 后直接调用
`detail::loadCanonicalValue(json::Value, limits)`。

验收：

- canonical Chart 成功路径只 parse 一次。
- 无效 JSON、错误类型和未知 format 的 diagnostics 与 B0 完全一致。
- Chart v1/v2/v3 Reader 和迁移入口行为不变。
- `ChartLoader` 不再通过原始文本调用会重新 parse 的 loader。

### B2：统一 Chart v4 Loader 与 `isV4`

涉及文件：

- `engine/chart/src/chart_v4_loader.cpp`
- Chart v4 内部 header 或等价 detail 文件
- `engine/playback/src/playback_session.cpp`
- `engine/playback/src/playback_source.cpp`
- `tests/chart/parse_characterization_tests.cpp`
- `tests/playback/preparation_characterization_tests.cpp`

新增或整理内部入口：

```cpp
inspectChartVersion(const json::Value&);
loadV4Value(json::Value, limits);
```

公共 API 仍保持兼容：

- `ChartV4Loader::isV4(text)` 仍可用，但自身只 parse 一次。
- `ChartV4Loader::load(text)` 只 parse 一次。
- Playback prepare 不再先 `isV4` 再 `load`，而使用统一 dispatch 结果。
- Filesystem source 创建阶段用同一个 v4 typed 结果发现 CXT imports。
- 无效 v4 Chart 的错误阶段和 source factory 行为保持不变。

建议的内部 dispatch 结果：

```cpp
struct ChartDispatchResult {
    ChartInputVersion version;
    std::optional<ChartDocument> legacyDocument;
    std::optional<ChartV4SourceDocument> v4Document;
    core::Diagnostics diagnostics;
};
```

该结果只供 Chart、Playback 和 CXC 的内部路径使用，不直接暴露给 SDK 宿主。

### B3：消除 Chart v4 Resolver 的 parse、serialize、parse 链

涉及文件：

- `engine/chart/src/chart_v4_resolver.cpp`
- `engine/chart/src/chart_writer.cpp`
- `engine/chart/src/canonical_chart_loader.cpp`
- `tests/chart/chart_v4_resolver_tests.cpp`
- `tests/chart/prepared_semantic_identity_tests.cpp`
- `tests/playback/playback_v4_prepare_tests.cpp`

目标结构：

```text
ChartV4Loader 已解析 Value
  -> Resolver 在同一个 Value 上完成参数替换
  -> 直接调用 typed canonical loader
  -> 生成 resolved ChartDocument
```

`makeConcreteChart` 不再执行：

```text
json::Value -> json::serialize -> CanonicalChartLoader::load(string)
```

而是直接调用内部 `detail::loadCanonicalValue(json::Value, limits)`。

如果 canonical writer 当前只能从文本入口开始，新增内部 `writeV4Value(json::Value)`，但不改变
公共 `ChartWriter` API。必须保持字段顺序、数字格式、空数组、扩展字段和 canonical bytes 完全一致。

特别注意：

- 不得把参数替换后的 JSON 错误地用来替代当前 source identity。
- 保持参数 identity 与 Chart identity 的既有组合规则。
- 失败 reload 不得改变 active identity、content、diagnostics 或 frame。

### B4：消除 CXC `isV4Chart` 重复解析

涉及文件：

- `engine/cxc/src/cxc_package.cpp`
- Chart 模块内部 dispatch 接口
- `tests/cxc/parse_characterization_tests.cpp`
- `tests/cxc/cxc_package_tests.cpp`
- 必要时更新 `tests/cxc/cxc_test_support.*`

目标流程：

```text
CXC entry Chart bytes
  -> Chart 内部统一 dispatch
  -> v4：复用已解析的 ChartV4SourceDocument
  -> v1/v2/v3：复用已解析的 ChartDocument
  -> CXC 执行资源闭包和 import 检查
```

CXC 继续负责 ZIP32、manifest、project、Asset Index、entry path、closure 和资源引用；Chart
parser 和 Chart 字段语义不迁入 CXC。

验收：

- v4 entry Chart parse count 从两次降为一次。
- v1/v2/v3 路径不引入额外 parse 或错误。
- `cxc.project.invalid`、`chart.*`、`cxt.*` 的 fieldPath 和排序不漂移。
- package identity 和 entry bytes 不变。

### B5：评估 `PlaybackSource` 跨边界缓存

本批先测量，不预设一定实施。

重点检查：

- CXC package loader 已验证 Chart，`PlaybackSource::fromCxcMemory` 当前只保存文本。
- Filesystem source 创建阶段可能已加载 v4 Chart，prepare 阶段又加载一次。
- `PrepareContext` 当前从 `entryChart->utf8Text` 开始。

只有同时满足以下条件才允许跨边界缓存：

- 能通过真实 prepare 测量证明收益。
- 不把 JSON DOM 泄露到公共 API。
- 不让 `PlaybackSource` 持有过大的重复 typed/DOM 副本。
- 不破坏 source move、reload、identity 和 16 MiB ownership。
- CXC validation 与 Playback prepare 仍共享同一语义验证路径。

优先考虑内部 move-only parsed artifact；默认不保存完整 JSON DOM。若最终不实施，必须在任务 2
报告中记录测量数据和不实施理由。

## 4. 必须保持的行为合同

所有批次都必须保持：

- Chart v1/v2/v3 Reader 和迁移入口。
- Chart v4 参数解析、Template Binding 和 Animation Program。
- canonical bytes、字段顺序和确定性输出。
- D6 已接受的 `PreparedSemanticIdentity` 规则。
- FrameDigest v1-v3。
- default capability 和显式裁剪 capability。
- candidate/active rollback。
- reload 失败时 active content、identity、diagnostics 和 frame 不变。
- 16 MiB ownership 和 `ChartLimits`/`CxcPackageLimits`。
- diagnostics 的 code、message、fieldPath、context 和稳定排序。
- `engine/animation/` 不解析 JSON/CXC/CXT。
- Playback-only consumer 和 installed public-header ASCII 边界。

## 5. 验证矩阵

每个批次至少运行：

```powershell
ctest --preset debug -R "parse|chart|cxc|playback"
ctest --preset debug -R "identity|digest|reload|rollback"
python -B tools/check_docs.py
git diff --check
```

每条链路完成后运行：

```powershell
cmake --preset debug --fresh
cmake --build --preset debug --clean-first
ctest --preset debug --no-tests=error
```

最终按影响范围补充：

- Release 全量 CTest。
- adapter-disabled headless。
- static/shared external consumer。
- CXC binary fixture 和 canonical golden。
- sanitizer/coverage。
- hosted Linux Quality、Windows MSVC、Windows MinGW。

## 6. 完成判定

任务 2 不能只以 `json::parse` 调用数量减少作为完成条件，必须同时满足：

1. Chart prepare 和 CXC entry Chart 均有 parse count 前后数据。
2. 重复解析确实被移除，而不是转移到另一层。
3. diagnostics fingerprint 完全一致。
4. canonical bytes、semantic identity、FrameDigest 和 capability parity 一致。
5. candidate/active rollback 和异常边界一致。
6. ownership 峰值没有因缓存 typed/DOM 恶化，16 MiB 门禁仍有效。
7. B5 若不实施，已有测量结果和明确理由。
8. 新增任务 2 报告，记录每批修改、测试结果、性能数据和剩余边界。

推荐顺序：**B0 -> B1 -> B2 -> B3 -> B4 -> B5 评估**。

其中 B2/B3 是 Playback prepare 的核心，B4 单独处理 CXC，避免把 Chart parser 逻辑重新复制进
CXC。任务 3 的分支覆盖可以与 B0 的 characterization 并行，但涉及相同模块的生产代码和测试
必须串行整合。

## 7. 实施结果

B0-B4 已完成，B5 评估结论为不跨 `PlaybackSource` 缓存完整 parsed DOM；详细 parse count、
ownership、parity 和验证结果见[任务 2 完成报告](../../../stage_reports/reviews/260830-followup/2026-08-31-task-2-chart-cxc-parse-once.md)。
