# Cuexis Documentation Policy

状态：已接受的文档整理政策

更新日期：2026-08-10

## 文档角色

每份文档必须拥有一个主要角色：

| 角色 | 允许内容 | 不应承担的内容 |
| --- | --- | --- |
| ADR | 背景、备选方案、决策、影响和接受门禁 | 当前构建结果、完整字段参考、逐次测试日志 |
| Spec | 字段、语义、版本、预算、错误和兼容合同 | 阶段进度、机器环境和一次性验证输出 |
| Plan | 目标、范围、批次、依赖、门禁和交接 | 重新定义已接受字段，复制全部 ADR 正文 |
| Report | 某个时间点的实施、审查和证据 | 把历史快照冒充当前状态 |
| Guide/Policy | 操作步骤、编码规则、依赖和版本维护规则 | 维护产品路线图的第二份副本 |
| Index/Status | 导航和当前摘要 | 复制完整技术合同 |

## 权威顺序

同一事实冲突时，按以下顺序处理：

```text
当前状态页 -> 已接受 ADR -> 生产 Spec -> 当前 Stage Plan -> 最新完成/审查 Report -> 历史材料
```

“当前状态页”只负责当前摘要，不能替代 ADR 或 Spec。候选格式必须同时标记 candidate、提案
或未实现，不能使用“已支持”或“生产格式”。

## 状态字段

新文档顶部至少写明：

```text
状态：active / candidate / future / deferred / completed / historical / superseded / archived
更新日期：YYYY-MM-DD
```

ADR 还要写决策状态；Spec 还要写实现状态；Report 还要写快照日期和后续关闭证据。中文正文
可以保留，但状态值和文件名应使用稳定、可搜索的英文枚举或固定词汇。

## 链接和重复

- 每份文档必须从某个索引或权威文档可达。
- 相对链接必须指向实际文件；移动文件时保留旧路径跳转页至少一个整理周期。
- 一个字段合同只能有一个权威 Spec。
- 摘要不得复制完整表格、完整诊断矩阵或当前阶段结论。
- 历史文档不得删除；应标记 `historical` 或 `superseded`，并链接替代文件。

## 脚本边界

运行时脚本和逐帧脚本回调无限期延后。任何文档不得为它们预留字段、extension、capability、
字节码或隐式执行入口。离线创作生成器若未来进入讨论，必须作为单独的 authoring tool 合同，
不能混入 CXT、CXC 或 Playback 文档。

## 自动检查

文档移动、归档、阶段更名或候选示例修改后运行：

```powershell
python tools/check_docs.py
```

检查器验证 Markdown 相对链接、单一 H1、从 `docs/README.md` 可达、目录索引和完整阶段索引、
Stage Chart Format Update 名称、运行时脚本边界、Stage 4-12 计划的目标/验收/归档来源，以及
候选 JSON/CXT 的解析、CXC entry 顺序和 CXT import 一致性。
