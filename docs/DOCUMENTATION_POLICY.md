# Cuexis Documentation Policy

状态：已接受的文档整理政策

更新日期：2026-11-23

<!-- cuexis-meta
id: GUIDE-DOC-POLICY
status: accepted
authority: owns
-->

## 文档角色

每份文档必须拥有一个主要角色：

| 角色 | 允许内容 | 不应承担的内容 |
| --- | --- | --- |
| ADR | 背景、备选方案、决策、影响和接受门禁 | 当前构建结果、完整字段参考、逐次测试日志 |
| Spec | 字段、语义、版本、预算、错误和兼容合同 | 阶段进度、机器环境和一次性验证输出 |
| Plan | 目标、范围、批次、依赖、门禁和交接 | 重新定义已接受字段，复制全部 ADR 正文 |
| Task | 一个独立任务的目标、输入、写域、步骤、验收、证据和停止条件 | 重复维护阶段依赖或擅自改变计划/ADR 合同 |
| Report | 某个时间点的实施、审查和证据 | 把历史快照冒充当前状态 |
| Guide/Policy | 操作步骤、编码规则、依赖和版本维护规则 | 维护产品路线图的第二份副本 |
| Index/Status | 导航和当前摘要 | 复制完整技术合同 |
| API Reference | 已发布 SDK 的入口、生命周期、可观察结果、错误与兼容约定 | 把内部实现类型描述为宿主 API，或复制完整头文件声明 |

`AGENTS.md` 只写路由与硬约束，不复述阶段、版本、预算或能力状态；这些事实由
`docs/MAP.md`、`docs/CURRENT_STATUS.md`、代码与生成页承载。`AGENTS.md` 顶部的
`CUEXIS_STATE` 块是生成物，`check_docs.py` 只校验它的指针与规则片段是否存在。

## 权威顺序

同一事实冲突时，按以下顺序处理：

```text
代码与 schema -> 当前状态页 -> 已接受 ADR -> 生产 Spec -> 当前 Stage Plan -> 最新完成/审查 Report -> 历史材料
```

代码、schema、`cmake/CuexisVersion.cmake` 始终高于文档；文档之间才按上面的顺序裁决。
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
- 相对链接必须指向实际文件；单文件移动保留旧路径兼容入口，批量重组按下文的具名路径映射规则处理。
- 一个字段合同只能有一个权威 Spec。
- 摘要不得复制完整表格、完整诊断矩阵或当前阶段结论。
- 历史文档不得删除；应标记 `historical` 或 `superseded`，并链接替代文件。
- **行号锚只能作为带日期的定版证据，不得作为活合同来源。** 活引用一律写章节名或符号名；确需引用具体行时，
  必须写在**带日期的证据记录**里，并同时注明所依据的**文档版本或提交标识 / 快照哈希**，使该行号可被复核到
  某一个定版。理由有实测依据：`engine/judgement/src/source_codes.hpp` 的 13 处行号锚在并行实现线新增注释后
  **已全部漂移 1 行**（令牌仍在被引范围的最后一行），说明行号随文档增删必然失效。

## 目录和命名

稳定入口保持在 `docs/` 根目录：`README.md`、`CURRENT_STATUS.md`、`PROJECT_GUIDE.md`、
`HANDOFF.md`、
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

提案目录按生命周期继续细分为 `proposals/research/`、`proposals/implementation-input/` 和
`proposals/deferred/`；已并入主路线的旧阶段计划统一放在
`stage_plans/historical-inputs/`，不再混入 `future/` 或 `deferred/`。

阶段计划和阶段报告按阶段或跨阶段专题归档，而不是按生成日期平铺：

```text
docs/stage_plans/active/<stage>/
docs/stage_plans/completed/<stage>/
docs/stage_plans/future/<stage>/
docs/stage_plans/deferred/<stage>/
docs/stage_plans/reviews/<topic>/
docs/stage_plans/historical/<topic>/

docs/stage_reports/stages/<stage>/
docs/stage_reports/reviews/<topic>/
docs/stage_reports/sdk-transition/
```

新的 canonical Markdown 文件名使用小写 kebab-case。阶段目录中的主计划文件命名为 `plan.md`；
带日期的报告使用 `YYYY-MM-DD-topic.md`；ADR 继续使用 `NNNN-short-title.md`。根目录稳定入口和
旧路径兼容页是明确例外，不要求改名。

已启动阶段的任务卡放在 `stage_plans/active/<stage>/tasks/T-xx.md`，使用七段任务模板；任务依赖只在同阶段 `schedule.md` 维护。新 ADR 使用 `docs/templates/adr.md` 起草；未接受前标记为 candidate/none，接受后同步更新决策状态、元数据权威级别及 ADR 索引。

单个文档移动可以保留旧路径兼容短页至少一个整理周期。兼容页只链接 canonical 文档，不复制正文，并标记
`状态：compatibility entry`。但是，批量 stage plan/report 重组不得用大量单页 stub 重新制造平铺目录；
应在相应目录的 `legacy-paths.md` 中把旧逻辑路径以代码文字映射到 canonical 文档。该映射不承诺旧的
逐文件 URL 继续存在。`docs/` 根目录不保留单文件 compatibility entry；其历史路径统一写入
[legacy-paths.md](legacy-paths.md)。

README 只用于稳定入口、顶层文档角色或包含多份需要独立导航的正文集合。单文件目录和由上级索引即可
清楚列出的叶目录不创建 README；上级索引直接链接 canonical 文档。所有 Markdown 仍必须从
[docs/README.md](README.md) 可达。

新建阶段报告目录及已迁入新布局的 RR 阶段使用用途分类：`readiness/`（准入与基线）、
`decisions/`（带日期裁定）、`implementation/`（实际修改与交付）、`verification/`（审查与验证）、
`handoffs/`（交接与收尾），原始附件归 `evidence/`。根目录只保留 README 和必要的 legacy-paths，
报告正文使用 `YYYY-MM-DD-topic.md`，日期必须是真实日历日期。混合报告按主要目的归一处，其他入口回链。
只创建实际有内容的分类目录，由阶段 README 直接导航，不机械新增分类 README 或每任务目录。
总报告索引只列阶段/专题入口，不复制报告计数、当前结论及测试矩阵。

既有已关闭阶段及 Stage 7 批次布局按具名历史路径保留；新增文件不会继承旧路径例外。
文件移动必须同批更新引用并登记旧路径映射，保持 ID、原日期和历史结论；导航引用的调整须有清单，
原始附件与封包字节不变。禁止借整理改写历史失败、回填自身 SHA 或伪造原验证结果。

写作模板在 [templates/](templates/README.md)：新 plan / 报告复制对应模板，模板中的固定 H2 标题
是 `check_docs.py` 的检查对象。`docs/HANDOFF.md` 是**对话级交接**，记录当前任务、下一步、阻塞与
验证状态，不是阶段权威；active 阶段超过 7 天未更新时 `check_docs_facts.py` 只给警告。

计划分两级：`stage_plans/active/**` 用详细模板（任务看板、入场门禁、验收矩阵、回滚条件）；
`stage_plans/future/**` 只用提纲模板（目标、边界、入口条件、待决问题），启动时再重写为详细计划，
避免提前写下会过期的批次与数值。

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

### 机器可读元数据块

合同、计划与证据文档在 H1 之后带一个 `cuexis-meta` 注释块，供 `tools/verification/gen_docs_map.py`
生成路由页与状态块，避免人肉维护索引：

```text
<!-- cuexis-meta
id: ADR-0027            # 稳定标识：ADR-<编号> / API-<名> / FORMAT-<名> / ARCH-<名> / GUIDE-<名> / PLAN-<路径> / EVIDENCE-<路径>
status: accepted        # 权威状态：current / active / accepted / candidate / future / deferred / completed / historical / evidence / unverified
authority: owns         # owns（拥有该主题）/ summarizes（摘要）/ references（引用）/ index / none（证据）
-->
```

- `status` 表达“是否还权威”，`authority` 表达“是否拥有该主题”。证据文档固定为
  `status: evidence` + `authority: none`；索引为 `authority: index`。
- 文件名带 `YYYY-MM-DD` 的证据报告额外写 `date`，取文件名中的日期，不改写正文时点。
- `docs/CURRENT_STATUS.md` 的元数据额外带 `current_stage`，它是“当前阶段”的唯一机器可读来源。
- 块由脚本批量补齐，正文一字不改；新增文档必须自行带上该块，否则 MAP 中显示 `unverified`。

真实文档的头部元数据 ID 全局唯一，块与字段键不得重复；状态及 authority 使用检查器的共享枚举。
模板、围栏中的示例及原始证据附件不注册为合同；索引为 `authority: index`，报告为 `evidence/none`。
模板自身仍须遵守小写 kebab-case 文件名，并由模板 README 登记；只排除示例身份，不排除文件管理。
模板复制后必须替换占位 ID、日期和范围。候选能力可以已有实验实现，实施、验证和正式发布分别表述。

关键合同在拥有者元数据的 `topics` 中登记含版本的稳定主题键。同一有效主题只有一个拥有者，
新旧版本可分别登记；已登记主题不能通过删字段、改权限或改名逃避检查，转移须有替代拥有者和具名依据。
尚未登记的合同主题明确为未覆盖；同义主题和证据充分性仍须审查。相同大表格/正文块的检测只报告疑点，
历史报告、模板和生成视图中的合法重复不直接判错；当前摘要确认越权复制时改为权威链接。

## 发现登记与历史保护

发现定义表前加 `<!-- cuexis-findings: definitions -->`，只作用于紧邻的下一张表；其他表和正文只引用 ID。
定义字段为 ID、原始发现、原始依据、核验状态、处置状态、责任/任务、当前处置依据。
定义编号唯一，引用必须存在；原始发现、ID 和首次证据与可信 Git 基线比较，不删除或重写。
事实纠正追加订正并指明旧断言，合法换行和表格转义不改变语义。

核验状态分“未复核/已核实”，处置分“开放/处理中/已修/已处置/延后/撤回”。已核实须有具名复核者和证据，
已修须有改动修订与实际验证；已处置/撤回须有理由和决定/反证；延后须有责任阶段、恢复条件及依据。
旧关闭证据无法取得时保留历史说法，当前处置如实保持开放，不能补造确认。
当前责任和处置可以更新，但前次处置仍须可追踪；历史关闭报告的新结果另作后续记录。

首次结构迁移逐条核对冻结原文，记录转换依据；后续用 Git 中的受保护字段比较。
CI 基线由工作流事件或显式手动输入确定，解析为固定提交并拒绝与当前受验提交相同；本地工作树默认对照 HEAD，也可明确传入同一基准修订。Git 历史缺失必须失败或标明未检查，
不得报历史保护通过。命名/布局遗留例外只列具体旧路径和理由，不能以目录前缀豁免未来文件。

## 脚本边界

运行时脚本和逐帧脚本回调无限期延后。任何文档不得为它们预留字段、extension、capability、
字节码或隐式执行入口。离线创作生成器若未来进入讨论，必须作为单独的 authoring tool 合同，
不能混入 CXT、CXC 或 Playback 文档。

## 自动检查

文档移动、归档、阶段更名或候选示例修改后运行：

```powershell
python tools/check_docs.py
```

检查器验证链接/章节、可达性、目录、命名、元数据与身份、登记册与历史保护、调度及候选示例。
`status_contract.json` 校验 CURRENT_STATUS 的阶段/工作项/登记册指针及其归属，不能硬锁某个旧阶段短句。
检查通过只覆盖实际运行项；重复疑点和未覆盖主题仍需相应审查。

生成物与事实检查（修改任何 `docs/` 文档后运行）：

```powershell
python -B tools/verification/gen_docs_map.py          # 重新生成 docs/MAP.md 与 AGENTS.md 状态块
python -B tools/verification/gen_docs_map.py --check   # 只校验是否过期
python -B tools/verification/check_docs_facts.py       # 校验生成物新鲜度与版本字面量漂移
```

- `docs/MAP.md` 与 `AGENTS.md` 的 `CUEXIS_STATE_BEGIN/END` 块是**生成物**，不得手改；`--check` 过期即失败。
- `check_docs_facts.py` 对照生成页、版本、预算、诊断、参数及相应代码派生事实；当前版本声明漂移导致失败。
  历史值需有局部明确语境，不能靠整行出现“旧”字豁免当前断言。发现登记原断言按历史证据解释。
- 证据目录（`docs/stage_reports/**`、`docs/archive/**`）不参与版本字面量检查：它们是当时的证据，不是当前事实。
- agent 完成文档变更前执行适用检查，即使本轮不提交或推送也应执行。相同规则由本地入口与 CI 复用；
  缺少生成事实或实际运行证据时显示未检查，严格入口不允许把缺失计入通过。hook 只是辅助，实际安装情况单独核实。
- 功能承诺在既有任务/验收表中链接合同、实现、正反测试、实际修订与结果。未实现能力保持待实施；
  结构检查和字符串一致不证明功能成立，完整构建、hosted 与设备长任务仍遵守仓库执行约定。
