# S7A-5 / S7A-6 Plan Consolidation and Desktop Handoff

状态：dated documentation record；联合目标、残余细节选优和目录整理，无新增产品实现

日期：2026-10-06

## 1. owner目标与当前基线

owner明确下一轮以 **S7A-5.1–5.5 + S7A-6.1–6.5** 为目标，要求复核未定决策、
写入plan/对应文档、保持active/stage-07干净，当前接手放到桌面。
本次整理开始时HEAD为b5594cf0dcdaff4a64c803ff693645f9391afea2，工作区干净；
代码行为基线仍为a0c8b7e4f783995bc19e626f3654ab11f845a0a7。
S7A-3/4受限功能验收完成，容量整体证明仍未完成；不借旧hosted证据认证本次或下一轮行为。

## 2. 决策与执行安排归位

主计划新增 [§3.2](../../../stage_plans/active/stage-07/plan.md#32-s7a-5--s7a-6-联合实施目标决策和准入)：
联合5+6目标、五批相对评估、九项选定方向、U01–U10待合同落定台账、J0–J7执行卡。
每项具名首次阻塞/关闭证据；规划编号不新增CONTRACT_MATRIX条目、R编号或诊断类别。
[实施输入](../../../proposals/implementation-input/stage-07/s7a-5-6-design-selection.md)
保留原45套方向方案，并在§13为十项残余细节各比较五套处置、选优和给最小反例。

复核的具体细节包括：

| 项 | 推荐与实施门禁 |
| --- | --- |
| U01 | typed registry、policy/scope/Loadout与新旧profile接受集，Fold不重选T4/K4 winner |
| U02 | Measure/engine/ruleset分属投影，grade缺表/Miss/error absence显式分支 |
| U03 | 逐规范Factchecked/clamp与maxCombo峰值，每Tick最终exclusive write |
| U04 | 空Tick单位元、显式模块trigger，零Fact但有signal/timer的工作Tick仍执行 |
| U05 | Hook目标/载荷/贡献键、持久值与pending语义队列、t+1不可表示失败 |
| U06 | raw live normalize后与Replay canonical records汇合；禁止再次量化或伪造raw来源 |
| U07 | Seek保留原archive，首次live接受/持久发布才fork；保留seal/fault的advance失败不能丢分支证据；跨目标advance物化至目标 |
| U08 | normalized/event/Fact各count与两cursor、两optional frontier分别定义 |
| U09 | 完整wire/tag/section/revision/byte覆盖与拒绝顺序，摘要不代替语义验证 |
| U10 | typed fault诊断闭包与restore权限分开，faulted不隐式变healthy |

已选架构不等于消费字段已经冻结；状态均为“待合同落定”，J0及首次消费卡完成字段与验证后关闭。
生产阈值/maxSeekLatency数值归9，D-9 master/具名接受/SDK放行归8，7的产品集成另轮消费。

## 3. 目录、兼容与桌面交接

active/stage-07只保留 **plan.md、legacy-paths.md**。
按DOCUMENTATION_POLICY批量stage重组规则，在legacy-paths中以旧路径代码文字映射canonical正文，
不为每个历史接手另建stub。四份完整旧规划来自b5594cf，归档时只新增历史标记并修正链接。

| 迁出文件 | 新位置 |
| --- | --- |
| s7a-3-4-implementation-handoff.md | [历史3/4接手](../../../archive/stage-07-planning/s7a-3-4-implementation-handoff.md) |
| s7a-5-implementation-handoff.md | [历史单批5接手](../../../archive/stage-07-planning/s7a-5-implementation-handoff.md) |
| s7a-5-6-implementation-handoff.md | [历史联合接手](../../../archive/stage-07-planning/s7a-5-6-implementation-handoff.md) |
| s7a-5-9-delivery-plan.md | [历史五批评估](../../../archive/stage-07-planning/s7a-5-9-delivery-plan.md) |
| s7a-5-6-design-selection.md | [现行候选实施输入](../../../proposals/implementation-input/stage-07/s7a-5-6-design-selection.md) |

新增归档索引；同步stage_plans/proposals/implementation-input/archive/stage_reports索引、
CURRENT_STATUS及相关链接。旧dated报告保留当时排期/验证，只修正文档URL。
execution profile撤下“未实施本补充合同”的陈旧状态断言，指向CURRENT_STATUS；实施安排归主计划。
本次不修改研究原稿、owner-only决定或产品合同的既有语义。

系统Desktop路径为`C:\Users\Zilch\Desktop`，当前接手文件为
`Cuexis-S7A-5-6-接手文档-2026-10-06.md`。
桌面文件是当前plan/方案输入的执行导出，使用绝对文件链接，含基线、阅读顺序、九方向、十残余项、
八张卡、十行退出矩阵和可复制的新对话指令。决策后续写回仓库，不只修改桌面副本。

## 4. 文档门禁与清理证据

本次只改文档，无C++/fixture/Schema/依赖/CMake/CI/版本输入改动，不运行C++/CTest/性能/容量矩阵。
文档门禁与迁移核对均通过：

| 检查 | 结果 |
| --- | --- |
| check_docs.py | 371个Markdown、20个candidate JSON/CXT通过 |
| check_docs_status_contract_tests.py | 4 tests通过 |
| check_docs_target_contract_tests.py | 2 tests通过 |
| update_version.py --check | 26.10.05-1一致 |
| git diff --check及staged diff --check | 无空白错误，含staged新增文件 |
| 目录/迁移/桌面文件链接 | active两项；旧active接手Markdown链接0；四份归档从首个正文小节起去除URL后与源SHA逐字相等；桌面11个绝对文件链接均存在、UTF-8/LF且一个H1 |

另核对主计划U条目恰为10项，实施输入§13恰为10项×5方案=50套；
这验证文档台账和导出完整性，不是codec/恢复/评分的实现测试。

五次文件迁移都验证绝对source/destination在仓库docs范围内、来源为普通文件、目标尚不存在；
使用原生PowerShell Move-Item，无递归删除。没有新建out临时脚本/日志，已有构建缓存和用户备份保留。
此前138个顶层临时目标及2618文件的证据归档/清理事实仍见
[hosted/归档报告](2026-10-05-s7a-3-4-hosted-and-handoff.md)，本次不重做该清理。
桌面接手不进入Git；仓库文档提交/推送到stage-7，按owner要求推送后不等CI。
本报告不关闭5/6、容量门禁或Stage 7A。
