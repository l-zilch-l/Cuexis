# Documentation Role and Stage 7 Report Classification

状态：dated documentation record

日期：2026-10-06

## 1. 范围与基线

owner要求整理文档并分类。本次从干净的stage-7工作区、HEAD
2b0a68c77bd770f728c6b61daa3f35904f33731e开始，按现有DOCUMENTATION_POLICY整理导航和报告目录。
全局已按ADR、Spec/API、plan、report、guide、候选输入/样例和archive分区，本次增强任务导航，
集中整理32份平铺的Stage 7报告；不迁移已有稳定格式/API/ADR入口，不改变产品合同或阶段结论。

## 2. 分类与索引

| Stage 7分类 | 数量 | 用途 |
| --- | --- | --- |
| readiness | 5 | 准入接受、Debug/Release基线、typed合同审视及D-9候选证据 |
| decisions | 13 | 准入/实施裁定、合同选择、落地位置、诊断分类 |
| implementation | 4 | 设计收口、执行方案和实施进度 |
| verification | 7 | 独立复验、功能验收、歧义/E1审计、CI优化、文档恢复 |
| handoffs | 3 | hosted交接、5/6排期和桌面接手整理 |

新 [Stage 7证据索引](../../stages/stage-07/README.md) 直接列出32份报告，并注明其证据日期。
总报告索引只保留阶段入口和跨阶段专题入口，撤下历史“本轮设计尚未实施”的置顶状态叙述；
报告中的当时进度保留，当前进度仍由CURRENT_STATUS维护。
根文档索引新增“当前工作入口”和“按用途查找”；文档政策登记按用途分类及索引职责。

## 3. 迁移和机器引用

移动前逐项核验32对绝对source/destination在Stage 7报告范围内，源为普通文件，目标不存在。
原生PowerShell Move-Item完成迁移，无递归删除；统一重定位Markdown相对链接。
旧逻辑路径保留于 [阶段legacy-paths](../../stages/stage-07/legacy-paths.md)，
总报告legacy-paths链接该批量映射，不创建32个stub。

文档检查器仅在DIRECTORY_INDEXES和STAGE_NAVIGATION_INDEXES登记新增的阶段README，
没有放宽任何规则。集中诊断JSON唯一内容改动是evidenceRecord.path改指新decisions路径；
不改诊断码、category、severity、faulted、revision或计数。
桌面`Cuexis-S7A-5-6-接手文档-2026-10-06.md`中的两个报告文件链接同步更新，决策仍归plan/对应合同。

## 4. 验证与清理

以下门禁与迁移对照通过：

| 检查 | 结果 |
| --- | --- |
| Markdown/候选样例文档检查 | 374份Markdown、20份candidate JSON/CXT通过 |
| 状态合同 / 目标合同检查 | 分别4 tests、2 tests通过 |
| 版本一致性 / whitespace | 26.10.05-1一致；工作区及staged diff无空白错误 |
| 32报告正文与源HEAD对照 | 统一行尾并去除Markdown URL后，全文与2b0a68c逐字相同；旧报告链接0 |
| 诊断JSON路径之外逐字段一致 | 唯一差异为/sourceAnchorPolicy/evidenceRecord/path；新目标存在 |
| active/stage-07目录 / 桌面链接 | 仍为plan.md、legacy-paths.md；桌面11个文件链接均存在 |

文档检查器与源HEAD的文本对照确认，只新增两个索引登记行。
这类一致性核对属于文档迁移证据，不是C++/codec/Gameplay运行期测试。

不新增临时脚本/日志到out；原构建缓存、用户备份和证据archive保留。
active/stage-07仍只保留plan.md和legacy-paths.md。
无C++/构建输入/fixture/依赖/SDK/版本改动，无新产品实现、性能或容量验证。
这是分类整理证据，不关闭5/6、state-budget或Stage 7A；按会话授权提交/推送，推送后不等待CI。
