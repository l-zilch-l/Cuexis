# 260830-followup 任务 2：Chart/CXC parse-once

状态：completed

更新日期：2026-08-31

所属计划：[260830-followup 维护计划](../../../stage_plans/active/260830-followup/plan.md)

实施方案：[任务 2 实施计划](../../../stage_plans/active/260830-followup/task-2-chart-cxc-parse-once.md)

## 1. 结论

任务 2 已完成。ChartLoader、Playback v4 prepare 和 CXC entry Chart 的重复 JSON 解析已按
职责边界移除；Chart v1/v2/v3、Chart v4、CXC、CXT、identity、FrameDigest 和失败回滚合同保持不变。

本任务没有修改安装后的 Playback 公共 API，也没有把 JSON DOM 引入 `engine/animation/` 或公共头文件。

## 2. 实施结果

| 批次 | 实施结果 |
| --- | --- |
| B0 | 增加 test-only thread-local parse probe，并保留 Chart、CXC、Playback characterization。 |
| B1 | `ChartLoader::load` 解析一次后直接把 `json::Value` 交给 canonical loader。 |
| B2 | 增加 Chart 内部 dispatch 和 parsed v4 loader；Playback prepare 不再先 `isV4` 再 `load`。 |
| B3 | Resolver 直接消费 parsed value；`makeConcreteChart` 不再经过 serialize -> parse；新增内部 `writeV4Value` 保持 canonical writer 路径。 |
| B4 | CXC 删除 `isV4Chart`，entry Chart 统一经 Chart dispatch，Chart 语义仍由 `cuexis_chart` 负责。 |
| B5 | 不实施跨 `PlaybackSource` 的 DOM 缓存；保留 source factory 与 prepare 的独立 ownership 边界。 |

关键实现位置：

- `engine/chart/src/chart_loader.cpp`
- `engine/chart/src/chart_v4_loader.cpp`
- `engine/chart/src/chart_v4_resolver.cpp`
- `engine/chart/src/chart_writer.cpp`
- `engine/cxc/src/cxc_package.cpp`
- `engine/playback/src/playback_session.cpp`
- `engine/playback/src/playback_source.cpp`

## 3. Parse count 证据

旧基线按原调用链复核，运行时结果由新增 test-only probe 固定：

| 链路 | 旧基线 | 当前结果 | 说明 |
| --- | ---: | ---: | --- |
| Canonical `ChartLoader` 成功路径 | 2 | 1 | format inspection 与 canonical load 共享首次 parse。 |
| 静态 Chart v4 Playback prepare | 5 | 1 | 旧路径包含 `isV4`、v4 load、Resolver parse、concrete serialize -> parse、writer parse。 |
| 静态 v4 CXC package | 5 | 4 | manifest、ProjectConfig、Asset Index 各 1 次，entry Chart 从 2 次降为 1 次。 |
| 独立 `ChartV4Loader::load` | 1 | 1 | 公共字符串入口仍保持单次 parse。 |

当前测试直接断言：ChartLoader 为 1 次、共享 dispatch + parsed Resolver 为 1 次、静态 v4 CXC
package 为 4 次、Playback v4 prepare 为 1 次。CXT 文档仍按各自 JSON 合同单独解析，没有与 Chart
DOM 混用。

## 4. 行为与所有权 parity

- Chart canonical bytes 继续由同一 canonical serialization 规则产生。
- v4 source identity 仍基于参数替换前的 canonical source；参数 identity 和
  `PreparedSemanticIdentity` 组合规则未改变。
- diagnostics 的 code、message、fieldPath、context 和确定性排序保持既有路径。
- candidate/active commit、失败 reload、FrameDigest v1-v3、capability preflight 和 16 MiB
  ownership 门禁均通过现有测试。
- parsed v4 value 只在 Playback prepare 内部通过 `ParsedChartInput` 持有，Resolver 完成后立即释放；
  它不进入 `PlaybackSource` 的持久状态，也不进入安装公共 API。

## 5. B5 决定

Filesystem source discovery 现在对 v4 Chart 只 parse 一次，但 prepare 仍会从 source 保存的文本独立
parse 一次。该边界是有意保留的：source factory 需要完成项目发现和 CXT import discovery，prepare
需要拥有独立的候选生命周期；跨边界保存完整 DOM 会增加长期内存占用和 move/reload ownership
复杂度，却没有本轮真实 workload 证明足够收益。

因此本任务不引入跨 `PlaybackSource` 的 parsed DOM 或 typed artifact 缓存。后续若有真实大文件或
多次 prepare workload，应另行建立内存峰值、prepare latency 和 reload ownership 证据后再开任务。

## 6. 验证

- MSVC Debug fresh configure、clean build：通过。
- 任务 2 核心 parse/chart/cxc/playback 测试：相关测试全部通过。
- identity/digest/reload/rollback 聚焦测试：`25/25` 通过。
- Debug 全量 CTest：`540/540` 通过；Windows symlink 条件测试按平台跳过。
- `cuexis_format_check`：通过。
- `python -B tools/check_docs.py`：通过。
- `git diff --check`：通过。

本报告只记录本地验证，不宣称新的 hosted 或 owner-acceptance 证据。`260830-followup` 总计划仍
保持 `active`，等待任务 1 和任务 3 完成。

