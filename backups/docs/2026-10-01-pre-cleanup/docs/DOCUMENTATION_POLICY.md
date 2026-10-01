# Cuexis Documentation Policy

状态：已接受的文档整理政策

更新日期：2026-08-30

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
| API Reference | 已发布 SDK 的入口、生命周期、可观察结果、错误与兼容约定 | 把内部实现类型描述为宿主 API，或复制完整头文件声明 |

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

## 目录和命名

稳定入口保持在 `docs/` 根目录：`README.md`、`CURRENT_STATUS.md`、`PROJECT_GUIDE.md`、
`ROADMAP.md` 和本文。根目录不承载格式、阶段计划、阶段报告或 API 正文。其余现行文档按角色归档到
以下目录：

```text
docs/architecture/  Runtime、模块和宿主边界
docs/formats/       Chart、CXC、CXT、动画和表现格式
docs/guides/        构建、编码、依赖和版本指南
docs/adr/           架构决策记录
docs/stage_plans/   阶段计划
docs/stage_reports/ 阶段证据
docs/proposals/     候选与延期设计
docs/examples/      评审和验证样例
docs/archive/       历史材料
docs/api/           已发布 SDK 与内部技术参考
```

阶段计划和阶段报告按阶段或跨阶段专题归档，而不是按生成日期平铺：

```text
docs/stage_plans/active/<stage>/
docs/stage_plans/completed/<stage>/
docs/stage_plans/future/<stage>/
docs/stage_plans/deferred/<stage>/
docs/stage_plans/reviews/<topic>/
docs/stage_plans/historical/<topic>/

docs/stage_reports/stages/<stage>/
docs/stage_reports/chart-format-update/
docs/stage_reports/reviews/<topic>/
docs/stage_reports/sdk-transition/
```

新的 canonical Markdown 文件名使用小写 kebab-case。阶段目录中的主计划文件命名为 `plan.md`；
带日期的报告使用 `YYYY-MM-DD-topic.md`；ADR 继续使用 `NNNN-short-title.md`。根目录稳定入口和
旧路径兼容页是明确例外，不要求改名。

单个文档移动可以保留旧路径兼容短页至少一个整理周期。兼容页只链接 canonical 文档，不复制正文，并标记
`状态：compatibility entry`。但是，批量 stage plan/report 重组不得用大量单页 stub 重新制造平铺目录；
应在相应目录的 `legacy-paths.md` 中把旧逻辑路径以代码文字映射到 canonical 文档。该映射不承诺旧的
逐文件 URL 继续存在。`docs/` 根目录不保留单文件 compatibility entry；其历史路径统一写入
[legacy-paths.md](legacy-paths.md)。

README 只用于稳定入口、顶层文档角色或包含多份需要独立导航的正文集合。单文件目录和由上级索引即可
清楚列出的叶目录不创建 README；上级索引直接链接 canonical 文档。所有 Markdown 仍必须从
[docs/README.md](README.md) 可达。

`docs/api/` 以发布的 Playback SDK 为首要对象，说明入口、生命周期、线程、资源、帧观察、诊断、
capability 和兼容边界。内部模块资料必须显式标为 internal，不能把 Runtime、World、EnTT、SDL、
OpenGL 或 JSON DOM 写成宿主可依赖的 API。

API 文档使用中文标题、章节和解释性文字；类型名、函数名、枚举值、capability ID 和其他代码标识保持
源码拼写并使用反引号。专题页依次给出元数据、权威头文件、“快速结论”、速查表或标准流程、失败与边界；
索引页先给核心规则，再按任务导航。不得使用完整英文自然语言标题或在各页发明不同的章节结构。

## 元数据约定

现行 Spec、Guide、Index、Status、Plan 和 API Reference 使用：

```text
状态：<stable English value>
更新日期：YYYY-MM-DD
```

ADR 使用其决策日期和决策状态；历史 Report 使用其证据日期或快照日期及后续关闭证据。整理不得把
历史日期改写为当前日期，也不得以统一字段名掩盖原始证据时点。

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
