# RR 文档治理交付

状态：evidence

更新日期：2026-11-23

<!-- cuexis-meta
id: EVIDENCE-stage-07-rr.documentation-governance
status: evidence
authority: none
date: 2026-11-23
-->

## 结论

成立：T-18 的规则、校验器、发现保护、RR 报告分类及关键承诺追踪已完成，并取得正反验证与独立复核。证明范围限本报告列明的文档治理；CMake target 事实缺输入项单列，产品任务及正式发行分别验收。

## 范围与方法

owner 已接受推荐方案并要求独立完成，本轮归 [T-18](../../../../stage_plans/active/stage-07-rr/tasks/T-18.md)。输入 HEAD 为 `ca5cbb9121b015ae0b3b5d681610ca94dac78184`，另含此前对话的工作树修订和未跟踪方案/模板；原始输入见[清单](../evidence/documentation-governance/manifest.json)、[原文封包](../evidence/documentation-governance/inputs.zip)和[补丁](../evidence/documentation-governance/working-tree.patch)。不能只 checkout 该 HEAD 冒充全部输入。

方法为确定性检查器、自测正反例、可信 Git 基线对照、报告逐篇导航差异比较及独立复核。原始发现和历史报告不因本次整理视为已修复，关键合同的产品证据按原责任任务取得。

## 执行记录

实施前运行 `python -B tools/check_docs.py` 与 `python -B tools/verification/check_docs_facts.py` 均退出 0；这只是旧门禁基线，不证明新增规则已经存在。

自测原始输出见[验证记录](../evidence/documentation-governance/verification.json)：登记册 35、治理 51、事实 13、状态 8、target 合同 2、章节合同 19、调度 27 项，共 155 项通过。记录绑定实际工作树的校验器哈希、命令、退出码和输出。

实际仓库总入口先运行正例，再仅在内存中注入重复文档 ID、重复发现定义、原始断言改写和非法模板命名；四个反例均退出 1 并命中预期错误，正本哈希不变。另以严格环境运行真实入口，缺 CMake 事实时退出 1。原始输出见[阴性对照](../evidence/documentation-governance/negative-controls.json)。收尾后刷新生成页，再验总入口、事实、新鲜度、调度、工具登记和空白，结果见[最终入口记录](../evidence/documentation-governance/final-gates.json)。

## 结果与证据

### 输入与迁移

[迁移清单](../evidence/documentation-governance/migration.json)逐篇记录原路径、目标路径、原/新字节及文本哈希、语义内容哈希和导航调整。对 13 篇报告保留链接文字及全部非导航内容，仅归一移动导致的目标；原日期、ID、结果和源码身份不变。另核对 6 个既有附件，当前字节逐项等于可信 Git blob 或其 checkout 物化形式（明确区分 Windows CRLF），本任务没有写入原附件路径。结果见[迁移复验](../evidence/documentation-governance/migration-validation.json)，旧路径见[映射](../legacy-paths.md)。

### 原始发现与当前处置

[独立字段核对](../evidence/documentation-governance/findings-validation.json)从封存原件逐行读取 149 个原定义，确认旧行每个信息列都保留在新表的受保护字段中，而非只用转换程序的同一摘要自证。登记册核验和处置状态分开；15 条文档修正依据当前正文及已提交快照保留已修，3 条保留已处置，2 条混合问题仅核实文档切片、实现残余仍开放。其余缺少本轮关闭依据的条目保留原说法和缺口；原始字段、责任及当前理由均可追踪。

### 规则与检查

正式规则归[文档政策](../../../../DOCUMENTATION_POLICY.md)，操作入口归 [BUILDING](../../../../guides/BUILDING.md)。文档结构、代码派生事实和实际功能验收分别报告；重复疑点不直接作错误，缺输入和未覆盖不能计为通过。

补齐 53 个原缺头部元数据的索引，仅改变导航身份，不改历史报告结论。真实模板和围栏例子不注册为文档；8 个关键主题在原拥有者中登记，规则只维护主题范围，不复制拥有者台账。旧布局和命名例外精确到既有 Git 路径，新文件不能自行取得旧例外。

重复疑点审查发现 Gameplay V2 ABI 与公共 Playback 页存在两段完整相同的公开合同，已在 ABI 页改为章节回链，保留其独有的内部恢复补充。此后当前扫描无重复疑点；这不证明任意同义表述不存在。当前状态合同只接受活指针 schema，旧短句 schema 明确拒绝；当前阶段、工作项和登记册互相核对，避免政策自动冻结旧状态。

### 功能承诺追踪

关键主题的实现与测试对应关系归[现有阶段验收](../../../../stage_plans/active/stage-07-rr/acceptance.md#合同与实际验证追踪)。本轮只核对映射和证据状态；尚待产品实施的正反行为保持待实施。

### 独立复核与订正

独立代码复核实际运行门禁和隔离反例，发现下列边界，均在本轮补强后再验。

| 发现 | 修订与验证 |
| --- | --- |
| 无事件基准时 CI 可能对提交自身比较 | 取消 tip 回退，手动运行要求 docs_base；解析后拒绝自比。实际提取 workflow shell，在临时 Git 仓运行缺基准、自比、旧基准和坏引用，结果分别失败、失败、通过、失败，见 [CI 基准对照](../evidence/documentation-governance/ci-history-controls.json) |
| 状态契约可指向另一个结构合法的页面 | 固定稳定路径与 STATUS 身份，增加伪页重定向和身份改变反例 |
| 正常索引可在同次降权并自造旧例外 | 例外还须证明可信基线本来就是 none，围栏伪字段不能作为旧依据 |
| 模板身份排除也跳过了命名 | 分开文件管理与身份登记；模板须命名合法且索引登记，示例 ID 仍排除 |
| 泛化基线词漏报当前版本，原型证据误报 | 当前声明优先拒绝漂移，只放行明确局部历史/原型证据；增量审查又发现后缀“历史/示例”优先级漏检，已调整顺序并补同句冲突反例 |

独立审查的[回执](../evidence/documentation-governance/independent-review.json)确认五项均可关闭，前四项沿用其上轮复核，最后一项重新读取并运行 facts 自测。该次真实 facts 检查发现收尾状态更新后地图尚未生成，退出 1；主代理随后刷新生成页并重新验证，最终结果归上述入口记录，不改写独立审查的原输出。

## 限制与未做

本轮只实施文档治理和 Python 短门禁，没有执行完整产品构建、安装消费者、设备/容量探针、hosted 或产品测试。当前本地没有新生成的 CMake target 事实文件；普通入口必须明确显示该项未检查，严格入口缺输入应失败。CI 接线只核对仓内配置，不能据此声明远端运行通过或已设必需检查。

主题归属只覆盖已登记主题；同义主题、重复是否越权及证据真实性仍需要审查。历史条目的原关闭证据若不可取得，则保留原说法及缺口，不补造提交、测试或 owner 接受。

## 对登记册与合同的影响

发现定义结构与当前核验/处置分开，原始字段受到 Git 对照保护。产品规则、格式接受集、兼容窗口和既有受限退出边界继续遵守原合同；本次无新增产品语义决定。
