# S7A-5 Implementation Handoff

状态：superseded planning snapshot；保留854efa3的单批接手内容，已由S7A-5/6联合接手取代

日期：2026-10-05

> 2026-10-06：owner澄清下一轮需要多个批次，并要求未定决策4–5套方案及选优。
> 当前执行入口为 [S7A-5/6联合接手](s7a-5-6-implementation-handoff.md) 与
> [方案比较和选择](s7a-5-6-design-selection.md)。以下全文只保留原排期快照，
> 其中“下一轮仅5、不实施6”和“字段以后再选”不再是当前指令。

## 1. 下一轮目标和基线

在 `D:\Cuexis-worktree`、现有 `stage-7` 分支完成Ruleset Fold、Score、Combo、Statistics
及其查询/reset/故障事务闭包，逐项退出S7A-5.1–5.5。
理由与其余五批的依赖评估见 [交付计划](s7a-5-9-delivery-plan.md)。
当前状态以 [CURRENT_STATUS](../../../CURRENT_STATUS.md) 为准。

代码行为基线是 `a0c8b7e4f783995bc19e626f3654ab11f845a0a7`；包含本接手文档的Git提交为规划锚点。
开始前读git status/log、核对基线仍为当前HEAD的祖先；不要为了匹配a0c8b7e而丢弃后续文档或用户改动。
S7A-3/4已完成受限功能验收；同源head SHA的7个CI runs成功，详见
[hosted/归档证据](../../../stage_reports/stages/stage-07/2026-10-05-s7a-3-4-hosted-and-handoff.md)。
SDK0.7.0、build26.10.05-1；容量整体保持S7A-9 INCOMPLETE GATE。
当前judgement模块仍是internal，不能由本批改成默认安装SDK产品入口。

首先读取：

1. AGENTS.md、CURRENT_STATUS、[Stage7计划](plan.md) 的5.1–5.5。
2. [V2 Spec](../../../formats/GAMEPLAY_V2_SPEC.md) §3.14–3.22、§7.2、§9.4，
   [V2 ABI](../../../api/GAMEPLAY_V2_ABI.md) 域5–7和S7A-5限定冻结范围。
3. [execution profile](../../../formats/gameplay-v2-execution-profile.md)、
   [execution types](../../../api/gameplay-v2-execution-types.md)、
   [第5轮裁定](../../../stage_reports/stages/stage-07/2026-10-03-s7a-5-6-gate-rulings.md)。
4. [kernel类型](../../../../engine/judgement/include/cuexis/judgement/kernel_types.hpp)、
   [session接口](../../../../engine/judgement/include/cuexis/judgement/judgement_session.hpp)、
   [kernel实现](../../../../engine/judgement/src/execution_kernel.cpp) 和既有独立S1/L2测试。
   typed动词已有3/4行为；无参数旧动词与snapshot/seek的拒绝不是本批可用成功入口。

## 2. 不变语义与本批范围

复用现有Fact而非重判Outcome/error/grade；Fact排序固定为
(commitTick, originKindPriority, canonicalOrdinal)，phasePriority只参与引擎排序，不作评分。
grade absent保持absent；不存在默认grade表或默认业务数值限额。

Ruleset仅为内置、静态注册、有限执行的Interface/模块；静态注册不允许IO、随机数、任意宿主回调、
脚本/字节码、动态插件、动态Requirement或隐式执行离线生成器。
拒绝cuexis.ruleset package时，不读取其hash/manifest/迁移；Life/correction保持稳定拒绝。
Register仅exclusive、commutative_monoid、ledger_derived；合成算子必须在声明carrier上成立。

事务边界不得混合：

| 失败位置 | 必须保持的状态 |
| --- | --- |
| S7A-4 kernel Tick尚未seal | 旧Fact/receipt/资源/游标/IDs前缀完整，失败Tick无半提交 |
| 已seal Fact后的Ruleset commit | 新Fact前缀不可撤回，旧Fold/Score/Combo/Statistics仍可查询；无fault Fact、无新RuleEffect/发布事件，session faulted |

成功路径依次Fact seal→全部Fold状态/RuleEffect原子校验并提交→从已提交Ledger与FactBinding投影。
失败后允许查询诊断/原因/旧状态及已seal Fact；submit/advance/snapshot/seek/replay/就地reload稳定失败。
离开faulted只能显式reset或替换新session，不能以旧快照补齐未提交delta。
S7A-4 kernel_transaction_failed和S7A-5 ruleset.transaction_failed的登记边界各自保留；
旧“Ruleset是唯一faulted路径”的历史表述不得覆盖当前execution profile/码表。

本批可做内部只读事件投影及恢复输入DTO验证，真实Playback/HostOverride bridge归7；
统计快照是owning query值，不等于6的session Snapshot。
本批不实施6–9、不修改candidate/production发行入口、不升SDK、不做merge/release或owner-only gate操作。

## 3. 按顺序实施的六张卡

### 5A：首用字段/profile准入

先输出限定S7A-5的Ruleset内存profile，逐项列明：

- Interface/Ruleset ID与revision、module/build identity、明确有语义的manifest顺序。
- arbitration/fold/programPolicy/outcomeScope/defaults/Loadout的表示与完整性规则。
- Register ID、kind、owner/贡献者、贡献identity、carrier、允许combine operator和direct-write政策。
- StateDelta目标、值表示、范围/重复写/顺序/提交边界；Score/Combo/Statistics完整字段。
- 模块显式初始值、增量、负值、累加溢出/饱和策略、Miss/combo break、grade absence与unknown grade政策。
- Hook/RuleEffect的owning结果、t+1可见性、fault查询/reset行为、ruleset identity投影。

以上字段仍由ABI追踪为待冻结，旧语义裁定不是字段已选定。本接手不预选新数值或字节编码。
下一实施会话应先完成普通表示选型及与既有裁定一致的Spec/ABI/typed补充，再写消费这些字段的代码；
实质改变语义须最小反例、影响分析与明确处置，不能默选默认值。
准入结果必须逐字段可实现且正负例明确；若首卡未通过，报告具体未决字段，不能转称5完成。
数值业务cap、Replay/Snapshot/FactId wire和maxSeekLatency不在本卡冻结范围。

### 5B：owning PreparedRuleset与configure/prepare

实现只读、长期自持有的prepared manifest/Interface，显式静态模块注册及capability检查。
验证缺module、未知outcome、Hook owner冲突、非法policy/scope、重复register/贡献声明、
Life/correction/package输入、配置与identity不匹配；失败不替换旧prepared/session且不fault。

沿用共同prepare的chart/session锚点，不给stage4实体/Pattern打补丁。
对manifest顺序、module/build语义变化和非语义source排列给出RulesetIdentity正反例；
四分量完整迁移/codec矩阵归6，但本批实际消费的ruleset分量必须真实生效。

### 5C：Fold、三类Register和事务集成

接在现有每Tick Fact seal之后；阶段2构造候选delta和全部Score/Combo/Statistics/RuleEffect，
校验通过后一次发布。阶段2失败保留新Fact与旧Fold前缀，禁止改变Fact IDs或补fault Fact。
对exclusive第二owner/第二写、未知operator、重复contribution、非法monoid、ledger_derived直接写、
范围/预算/分配失败设具体稳定诊断及失败注入。预算只消费已接受合同，不能添加隐藏默认cap。

避免把checked或饱和有符号sum直接声明为monoid；给出carrier闭包及不同置换/分组人工golden。
独立Fold参考实现只读已提交Fact与明确Ruleset声明，不调用生产reducer/combine/排序或事务helper。

### 5D：Score/Combo/Statistics与结果生命周期

按5A选定profile实现命中、Miss、grade absent、phase-local error、combo break、负值与合法边界、
长流溢出/饱和及reset。query自持有、只读；新run不能污染旧查询值。
逐字段对照增量Fold与从Ledger起点重建，保留所有phase/category，不以最终score/hash代替结构比较。

5.5的seek/replay关联语义由Ledger重建fixture验证并登记6的实际集成验收，
不删除总计划中跨批需求，也不假称生产seek/replay已接线。

### 5E：Hook、t+1 signal与失败查询

只在成功阶段2后发布本Tick RuleEffect和Hook贡献；同Tick不可观察新signal，下一Tick可见。
核对stage4内置signal测试与真实Ruleset Hook是不同消费点。
失败Tick不得发布新signal/RuleEffect/PresentationEvent；显式查询可按sealed Fact重建只读投影，
但不能偷跑失败Tick发布阶段。faulted诊断、reset、替换新session和旧查询保活逐项验证。
未实现的snapshot/seek/replay仍拒绝，不能用成功占位证明fault矩阵。

### 5F：验收、计数、证据与交接

逐行回填5.1–5.5，每行IV/IU/M/LB及具体断言；将6承担的产品恢复门禁具名保留。
记录module/register/贡献/Fact数、state/delta/事件峰值、fixture规模和耗时分布；
这些是S7A-9输入，未测production预算继续INCOMPLETE GATE。
形成带日期的完成报告和6的状态字段/提交游标/identity/Hook闭包交接。
若本会话获准commit/push，只验证新行为SHA，按owner要求决定是否等待CI，不沿用a0c8b7e成绩。

## 4. 最小验收矩阵

| 对应小目标 | 独立golden与负例 | 必须证明 |
| --- | --- | --- |
| 5.1 prepare | 显式静态manifest、顺序变更、缺module/owner/policy/scope | 拒绝原子性、owning视图、真实RulesetIdentity影响 |
| 5.2 transaction | kernel未seal失败；seal后delta冲突/溢出/预算/分配失败；多Tick成功前缀后失败 | 两类边界不同，新Fact保留、旧Fold保留、没有fault Fact/半个Tick/发布事件 |
| 5.3 score/combo | 各phase Hit/Miss、grade absent、显式分值、Miss断连、负值/端点/长流/reset | 逐字段数值、溢出策略、无默认grade、phasePriority不参与评分 |
| 5.4 register/hook | 三kind、贡献置换/分组、重复ID、第二写、未知operator、t/t+1 | 合成闭包、确定性、成功Hook延迟、失败无发布 |
| 5.5 statistics | 不同advance分段、source排列、Ledger重建、reset后新run、旧query存活 | owning只读统计、incremental/rebuild等价；实际seek/replay门禁登记6 |
| 统一边界 | Life/package/correction、未知profile与配置、faulted各动词 | 既有九类/R19与稳定码；prepare/submit失败不fault |
| 统一证据 | 固定seed及最小化失败trace、人工正负golden、跨工具链 | 非只比较hash/score；不将相互一致自动视为正确 |

保留当前T4/K4、L1/L2、S1/S2、author双路、Capsule revision2/3和全部旧golden。
为真实Ruleset cases登记独立label并核对非零清单；label名称可按现有命名选ruleset，
不能把executable名当Catch case regex。最终运行Debug/Release完整矩阵、headless/static/shared/MinGW
与Linux GCC/Clang及现有hosted质量要求；Windows不伪跑Linux-only sanitizer/coverage presets。
模块依赖、公共头ASCII、Result/异常、allowlist与JSON隔离按AGENTS执行。
文档变更运行check_docs、status/target contract tests和git diff --check。
若升日期build，必须fresh / clean-first；SDK0.7.0保持。

## 5. 本机证据与临时目录

先前3/4的raw logs、XML、manifest、脚本及CI JSON已迁出临时目录；位置、hash和清理清单见
[hosted与归档报告](../../../stage_reports/stages/stage-07/2026-10-05-s7a-3-4-hosted-and-handoff.md)。
out/build仍保留可复用构建/依赖缓存；清理不要求每次删除vcpkg或全量重建依赖。
WSL Clang直接配置曾使用Linux专用prefix，与Windows包不可混用；重用前查CMakeCache/toolchain，
重配命令可从证据ZIP恢复，或按现行Linux preset fresh配置，不能据旧build目录名猜环境。

## 6. 新对话可复制指令

> 在D:\Cuexis-worktree、现有stage-7分支按docs/stage_plans/active/stage-07/s7a-5-implementation-handoff.md
> 完整实施S7A-5.1–5.5。先读CURRENT_STATUS、AGENTS、剩余批次评估、Spec§3.14–3.22、ABI及execution profile。
> 先完成5A首用内存profile/字段选型并按既有裁定补充权威合同，再依次5B–5F；普通表示选择可在本批内完成，
> 真正改变既有语义须最小反例与明确处置。复用已seal Fact，严格分开kernel回滚与Fold失败后的Fact保留。
> 使用独立Fold oracle、人工golden和失败注入逐行验收，不以默认grade/数值cap、伪成功或临时typedef绕过。
> 不实施S7A-6–9、Life、外部Ruleset package、Playback产品桥或SDK0.7.1；容量整体保持S7A-9 INCOMPLETE GATE。
> 保留用户改动；未经对应会话明确授权不commit/push/merge/release，也不自行消费owner-only版本门禁。
> 已授予的授权不重复请求；完成本批受限功能验收后报告证据和6的状态闭包交接，停止扩展下一批。
