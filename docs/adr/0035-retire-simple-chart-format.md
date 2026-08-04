# ADR 0035：阶段 2A 移除 Simple Chart 格式

日期：2026-08-04

状态：已接受；计划于阶段 2A.1 实施

## 背景

`cuexis.chart.simple`（方案 B）用于 Studio 尚未完善时降低手写谱面成本。阶段 1 已实现 `SimpleChartImporter`、Simple v1 Schema、确定性 UUIDv5 转换和相关 fixture。Chart v3 将引入 Tempo Event、Behavior Event、Step Event 和更严格的能力声明；继续维护方案 B 会迫使每个新字段同时设计简写、迁移、诊断和测试路径。

## 决策

Cuexis 在阶段 2A.1 正式移除方案 B，不设计 `cuexis.chart.simple` v2，也不保留 Playback/Chart 主路径中的长期只读 Importer。此后唯一受支持的谱面格式族为：

```text
format: "cuexis.chart"
```

阶段 2A.1 删除：

```text
ChartLoader 的 cuexis.chart.simple 路由
SimpleChartImporter 及其 public header/source
cuexis.chart.simple.v1 Schema artifact
Simple importer 单元测试和 architecture/build target 输入
Player 的 Simple fixture 复制、命令和回归入口
把方案 B 描述为受支持输入格式的当前文档
```

## 迁移

删除前完成一次性迁移窗口。当前仓库只有把 Simple JSON 导入内存 `ChartDocument` 的
`SimpleChartImporter`，没有可将结果保存为 canonical JSON 的正式 CLI，因此不得把“使用当前
版本转换”描述为已经可执行的用户流程。

- 仓库内 Simple fixture 先转换并提交为 canonical Chart。
- 先盘点仓库外仍需保留的 Simple v1 文件；若盘点结果非空，在删除 Importer 前提供并验证一个固定于最后 Stage 1 格式的一次性转换物，将其转换为 canonical v1，再使用正式迁移器升级到 v3。
- 若确认不存在仓库外 Simple v1 文件，以可审计的盘点记录关闭该迁移任务，不为假设用户建立长期兼容层。
- 一次性转换能力不进入阶段 2 Playback 安装包，不形成长期公共 API 或兼容承诺。
- 已经保存为 canonical v1/v2 的文件继续按 canonical 兼容政策处理；其历史 UUID 来源不使其重新成为方案 B。

## 拒绝的方案

- 保留只读 Importer：仍需持续维护 JSON Reader、安全预算、诊断、依赖和测试矩阵。
- 新增 Simple v2：会重新建立第二套 Chart v3 表达并扩大后续 Studio/SDK 负担。
- 让 v3 Loader 自动识别 Simple 字段：违反显式 format/version 路由，并使错误输入产生歧义。

## 影响

阶段 2A.1 是删除边界；在其完成前，当前 Stage 1 构建仍可用于识别和验证待迁移输入，但不得为方案 B 增加功能，也不得被描述为已有落盘迁移工具。阶段 2A.1 完成后，`cuexis.chart.simple` 输入必须稳定报告不支持格式，所有新文档、示例、测试和工具只面向 canonical Chart。
