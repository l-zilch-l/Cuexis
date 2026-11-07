# R3 high-requirements trusted Seek attribution

状态：diagnosis complete，2026-10-27。仅归因，未进一步修改产品逻辑或接受 R3/R4 预算。
返回 [New RPA 报告索引](../README.md)。当前候选来源为[空推进 reserve 尝试](2026-10-27-r3-empty-reserve-attempt.md)。

## 1. 已查清的主因

在本次 2048 requirements、33 条源记录、可信 checkpoint 第 17 条、目标第 33 条的样本中，
约 168 ms 的 fork 主要花在 **16 次后缀最终化全状态复制、16 次完整结果比较和一次新内核准备**。
可信 checkpoint 本身的状态还原约 11.3 ms，占 6.7%；不能把整段 fork 的 168 ms 都叫作 checkpoint 还原成本。

| 互不重叠的阶段 | 诊断均值 | 占 fork |
| --- | --- | --- |
| 16 次后缀最终化 | 74.521 ms | 44.41% |
| 16 次完整 KernelProjection 比较 | 35.308 ms | 21.04% |
| 新内核 prepare | 34.692 ms | 20.67% |
| certified state restore | 11.256 ms | 6.71% |
| ReplayData header 构造/复制与发布、旧 header 释放 | 8.096 ms | 4.82% |
| 其余析构、循环及 Session 包装 | 3.932 ms | 2.34% |
| 合计 | 167.806 ms | 100% |

这里采用同一调用内互斥区间，未将父子耗时相加。以下细项均嵌套在上表中。
按这组实测，当前 prepare + restore 本身已约 45.95 ms；只减少最终化副本也不足以使当前整条路径达到 20 ms。
这不是所有设计的数学下界，更不是 R4 正式窗口结论。

## 2. 复制的具体内容和成本

source 和 checkpoint 已经处理完本样本的 timer/input 工作，16 条后缀没有内部 Tick；每条只需要推进
processedFrontier/lastAdvanceHorizon，却仍在 `execution_kernel.cpp::advanceRecorded` 构造整个 RuntimeState 的 final 副本。
此前优化删除了这些调用中的第二份 reserve 副本，第一份 final 副本仍在。

本样本每份投影包含 6144 个 facts、6144 个 phases、1 个 resource。完整 fork 中有 19 次 projection 复制：
1 次初始 reserve（facts 为空）、1 次 checkpoint DTO 恢复、1 次恢复后的 reserve、16 次最终化；
其中 18 次复制完整的 6144 facts。新状态的 scope、private matcher/游标等仍按现有合同重建。

| 复制细项，嵌套统计 | 第一窗口均值 |
| --- | --- |
| facts vector 及嵌套值 | 59.556 ms |
| phases vector 及嵌套值 | 16.083 ms |
| matchers 及其 states | 2.028 ms |
| 其他 contacts/ownership/coverage/Fold/RPA 等 | 本 fixture 大多为空，不能外推其生产成本为零 |

facts 并非一组小编号。该 fixture 的 `PhaseOutcomeFact` 保存 requirement，TimerOrigin 又保存 requirement，
canonicalOrdinal.originScope 再保存相同 identity。`RequirementIdentity` 是多个 owning string 和 emissionPath vector 的值。
因此复制一条 fact 会连带复制三份完整 identity；这组样本平均产生 6 次嵌套分配。
`sizeof(KernelFact)=1152 B`，`sizeof(RequirementIdentity)=184 B` 均不含其堆内存。

独立 allocation-observer 进程记录每次 2048 的可信 fork：

| 范围（嵌套，不能相加） | 分配次数 | 累计 requested bytes |
| --- | --- | --- |
| 整个 fork | 977394 | 303758926 B |
| facts 复制 | 663570 | 151290558 B |
| phases 复制 | 233491 | 30819045 B |
| projection 全部复制 | 897118 | 230438725 B |

18 次非空 facts 复制各为 `6144 × 6 + 1 = 36865` 次分配；phase 复制各为 `6144 × 2 + 1 = 12289` 次。
该次数与计数器、类型关系相符，确认热点内存在大量 owning 值复制及小分配。
字段耗时同时包含分配、数据搬运和构造，未拆分 allocator 与 memcpy 的 CPU 排他份额。
字节数包含 STL 向全局 new 申请的分配/对齐空间，是累计申请量，不是峰值、RSS 或生产 cap；
观察器改变了 allocator 路径，其耗时不用于上表归因。

## 3. 完整比较与 identity 的额外成本

每条后缀执行后，`seekCandidate` 调用 `recovery.cpp::sameKernelResult`，依次比较所有 phases、resources、
facts、identity、Fold/RPA 和标量。对象是独立副本，不能依靠相同对象地址结束比较。
第二个独立细分窗口保留同一表达式的短路次序，得到：

| 比较细项 | 细分窗口均值 |
| --- | --- |
| facts 完整比较 | 25.531 ms |
| phases 完整比较 | 8.335 ms |
| PreparedIdentity 比较 | 2.215 ms |
| 其余 | 约 0.062 ms |
| 比较总计 | 36.143 ms |

细分窗口 fork 均值 166.338 ms。这里把原 comparator 表达式复制到 EKK 内按字段计时，可能改变
相对原 out-of-line 函数的内联/codegen；该表是字段级诊断拆分，不是原函数各字段的精确排他时间。
不能将该窗口的细项加到第一窗口 167.806 ms 上。
它确认了完整比较也是显著成本，尤其是含重复身份的 facts；后缀虽无新判定，仍逐 record 遍历完整旧状态。

另一个明确的值复制来源是 `PreparedIdentity::CanonicalIdentityBytes` 内的 owning byte vector：
本 fixture 在 64/256/2048 时分别为 81091 / 319135 / **2542263 B**。
第二窗口中，仅 19 次 projection 内该身份字段复制约 **7.222 ms**，已包含在 projection/最终化等父区间中。
这与 checkpoint 工厂的 archive SHA-256 是不同载体；本次可信 fork 没有重新生成工厂的 archive SHA-256。

`recordingCandidate()` 每条还 `make_shared<ReplayData>(*recording)`：记录表可以为空，header 的 PreparedIdentity
仍会复制。第一窗口该区域总计 6.817 ms，其中尾块复制仅 0.134 ms；其余主要是 header 构造/identity 值复制。
随后 log_publish 约 1.279 ms，包括旧 header 释放。
因此“尾部复制 120 条 ReplayRecord 很慢”不符合本样本；它们主要是空 batch 和 shared result 指针，
真正大的额外载体在 header 身份字节和 projection payload。

## 4. 一次准备与恢复中的重复工作

prepare 34.692 ms 内：

- phaseOffsets / requirement map / timerRequirements 构建 **15.285 ms**；
- makeRuntimePreparedIdentity 生成身份 **7.966 ms**；
- 其余约 **11.441 ms**，包括校验、初始 phases/matchers、初始 reserve、配置/RecoveryInputs、构造和临时释放。

PreparedGameplay 仍然是已有不可变共享 handle，没有把这一项误报为 prepared graph 深复制。
还原 11.256 ms 中，checkpoint 的 kernel projection 复制约 4.681 ms，新 fault reserve 约 4.668 ms；
两者之外还有 matcher 重建、配置/记录基座等，并在替换时释放刚 prepare 的初始 active/reserve。
其中 restore_publish 约 1.415 ms 包括该初始状态释放，属于 restore 的子区间，不另加到总和。
整个 fork 内 3 次 RuntimeState 析构约 **4.843 ms**，分布在 restore 和首次后缀 advance 中，同样已入账。

## 5. 返回后的释放另有成本

计时边界是 `JudgementSession::forkForSeek`；探针后续 Snapshot 物化及 sameSnapshot oracle 均在该边界外。
返回会话保存 logTail 中 16 条后缀 owning results，不能把它们当作已在 fork 内释放。

本次另外计量了“先释放临时 Snapshot，保留外部 source/checkpoint，再释放返回的 session”：
2048 的普通计时诊断为 **64.640 ms**，其中 17 次 RuntimeState 析构约 **64.510 ms**。
独立观察器对应 870619 次 free、218189933 B requested 释放；其中状态节点本身 870524 次 free。
该 64.640 ms **不属于前述 167.806 ms**，也不包含外部 source/checkpoint 的最后释放。
此前 source unbind/reset 的约 139/146 ms 是其他 owning 边界，仍保持原记录，不混成同一个析构时间。

## 6. 方法、扰动和保全

原始目录：`D:/Cuexis-worktree/out/rpa-r3-attribution-20261027/`。
入口 2191 个源/文档字节与上一候选绑定相符；本次所有产品源保持原字节。

第一窗口在运行前登记 normal-before / profile / allocation / normal-after 四个进程；
N=64/256/2048，每规模固定 3 对 origin/hint，33 条 advance、checkpoint 第 17 条 H=1119、目标 H=1135，
普通测时 allocator、宏 OFF、mask=4、显式 native HighQoS。第二窗口为额外的细分诊断，另登记 1 个进程、18 次测时调用，
只拆比较/身份字段，没有混入第一窗口均值。登记中的 no_additional_samples 指每个窗口不超过其已登记次数，
不表示全批没有新增第二窗口。
五个进程各 219 assertions，通过；均检查完整 Snapshot 与原点结果一致和真实 base=17。

| requirements | 普通前置进程均值 | 普通后置进程均值 | 第一诊断进程均值 | 诊断/两普通均值 |
| --- | --- | --- | --- | --- |
| 64 | 5.093 ms | 5.365 ms | 5.727 ms | 1.0951 |
| 256 | 18.112 ms | 18.146 ms | 18.888 ms | 1.0418 |
| 2048 | 165.618 ms | 162.129 ms | 167.806 ms | 1.0240 |

高规模诊断相对两普通均值约 +2.40%，数据支持上述近似归因；低规模的诊断扰动更大。
没有按该比例归一化原始值，也没有将这些有限均值写成正式性能 CI、15% 噪声门或端到端接受。
计时为 wall-clock；没有 CPU 栈采样或独立线程 CPU 排他量，不将其宣称为硬件计数器/ETW 结论。

诊断只在 out 中复制 EKK translation unit 和日志 helper：
固定 thread-local 聚合，无计时内日志/锁；copy ctor/dtor guard 只加在 TU-local RuntimeState，
未改变公共 KernelProjection 定义。guard 增加少量对象元数据/计时开销，上表单独披露扰动。
所有父子 exclusive 槽之和逐样本等于 root inclusive，不能把父子时长重复相加。
链接 MAP 逐函数确认 prepare/seek/restore/advance 来自诊断 object，普通进程来自原库；
未用 WHOLEARCHIVE，也没有改写或重建已冻结的静态库。

证据入口：`registration.json`、`result.json`、`analysis.json`、四份 `*-raw.log`；
`refine/registration.json`、`refine/result.json`、`refine/raw.log`、独立复审处置 `audit.json`；
生成器/源副本、编译命令、MAP 和最终 `binding.json` 保留在同一目录。
无 40k、ETW/WPR、设备运行、正式窗口、提交或预算变更。

结论只针对这组合法已封存源、单 hint 和零工作后缀；有输入、control、Fold、partial 目标和不同历史长度
必须另做适用性检查。前次批处理候选仍未实施；本次诊断不默认启动任何表示重构。
