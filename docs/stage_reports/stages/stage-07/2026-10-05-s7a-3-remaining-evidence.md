# S7A-3 剩余实施与验收证据核对

状态：dated evidence audit；S7A-3 尚未整批验收

日期：2026-10-05（Asia/Shanghai）

上级文档：[CURRENT_STATUS](../../../CURRENT_STATUS.md) ·
[Stage 7 计划](../../../stage_plans/active/stage-07/plan.md)

## 1. 范围与结论

owner 要求记录 S7A-4 推荐方案，并询问 S7A-3 的实际剩余项。本轮只读核对既有源码、
测试、报告与 hosted run，再维护文档；不实施、不在本机重新构建 C++ 或运行 CTest。
核对行为 SHA 为 `6b11d102f6dcac4d9782cd03d0bae15add54af28`，分支 stage-7，编辑前干净。

“S7A-3 尚未整批验收”保留，但不能再笼统解释成 grace identity 修正、Capsule Reader/Writer、
file/memory 桥接或 Clang 聚焦测试尚未完成。它们已有实现、带日期聚焦测试和当前 SHA hosted 证据。
主要余项是 authoring 双路接线与逐字段等价、K4 affine lowering 的实现/证明覆盖，
以及完整 E1 的逐项证据归档。状态预算缺测独立保留，不在本轮冻结生产阈值。

本报告纠正较早摘要和报告内部的过时未完成表述，保留历史报告原文，
不把既有执行改写为本轮新跑的测试，也不宣称 S7A-3 已关闭。

## 2. 已有实现与证据

| 项目 | 当前核对所得 | 证据边界 |
| --- | --- | --- |
| grace 来源身份分区 | gameplay_assembler.cpp 的 writeChartProjection 写最终 preparedGrace 与 localClosePolicyToken；已有来源变化同 judgement identity 的测试 | 只读检查写入点和测试；本轮未重新执行 |
| Capsule Reader/Writer | gameplay_capsule.cpp、公共 typed 入口和 round-trip / hostile / atomic / golden 测试已存在 | 不再登记为整个模块未实现；完整 E1 覆盖仍需逐项核对 |
| file/memory 与原子 publication | Capsule 测试包含 file/memory bridge 与旧 artifact 保持；prepare 测试覆盖 active 保持 | 已有测试不等于所有 E1 负例均覆盖 |
| 三工具链 focused tests | 2026-10-04 报告 §5 已列 MSVC Debug、WSL GCC、WSL Clang 两目标通过 | 各工具链历史 focused 数字不是本轮复跑数字 |
| 包含性装配消费点 | 既有报告记录 gateIncomplete 通过 assembly/prepare 原子拒绝；对应 assembler/prepare/Pattern 测试已存在 | 不再记成未接线；与 state-budget 缺测分开 |
| 当前 SHA hosted | Version Gate、Linux Quality、Windows MSVC、Windows MinGW 成功；所读日志含 Capsule golden | 现有测试矩阵通过不自动证明未实现的 authoring/affine 或完整 E1 |

源码/测试定位：`engine/judgement/src/gameplay_assembler.cpp` 的 writeChartProjection；
`engine/gameplay_packed/src/gameplay_capsule.cpp`；
`tests/judgement/gameplay_identity_tests.cpp`、`gameplay_prepare_tests.cpp`、
`gameplay_assembler_tests.cpp`；`tests/gameplay_packed/gameplay_capsule_tests.cpp`。
它们是定版定位，不另立语义权威。

## 3. 真正剩余的实施与验收项

| 项目 | 状态与关闭证据 | 阶段归属 |
| --- | --- | --- |
| Chart v5 inline / CXT v2 emission authoring adapter | 2026-10-04 报告仍登记待接线；本轮已看到 typed entry 等价测试，但未取得真实双路 authoring 完整接线证据。须由两路 authoring 得到同一 canonical graph 和 derived capability closure，逐字段 semanticDiff 为空，数组/参数/引用置换不变 | S7A-3 的 P1-15 / E1；Playback manifest 不代替此证据 |
| K4 affine mixed-radix lowering | 显式实例 pair、claim key 与冲突已有测试；本轮在 judgement 源码/测试限定检索未发现 affine/rank-block lowering 或端到端用例。此为待完成或待证明项，不以关键词未命中断言所有潜在路径不存在。须给出合法展开、radix/path 歧义、block overlap、checked u64 溢出、缺声明/未分配实例及与显式实例表等价证据 | S7A-3 prepare/lowering；held/slot 运行行为归 S7A-4 |
| 完整 E1 对账与验收归档 | Capsule §11 的八组要求须逐条映射到实现、fixture、断言和同一行为 SHA 执行证据。已有 framing 负例和一个固定 golden 不代替全部 opcode/tag/ref/closure/ordered-list 及 authoring 等价要求 | S7A-3 E1；不是再笼统要求“先跑 CI” |
| 状态预算缺测 | 保留各维度 measured/缺测、已证下界和适用范围；不能把缺测当通过，也不能把缺测一律当超限。生产阈值与测量能力匹配由 S7A-9 实测后接受 | 独立 INCOMPLETE GATE；研究 4096 或整数最大值不成为 S7A-3 新阈值 |

本轮没有逐一审计完整 Reader 负例和所有 Pattern/Measure 断言，因此未把具体未核对的
E1 子项直接宣判为“未实现”。完整 E1 归档应明确已证、待实现、待验证及后续 runtime 归属，
不能仅用一个测试总数表示全部要求已满足。

## 4. 不应混入 S7A-3 前置的后续事项

- T4 的早释放、bodyEnd/tail/deadline 与实际 Fact/resource 生命周期，K4 的 held 不抢占、
  同 Tick free slot 复用和 runtime coordinator：S7A-4。S7A-3 验收 prepare 合法性与 immutable plan，
  不能靠静态测试宣称 runtime 已通过；E1 条目保留并标明消费阶段，不删除要求。
- 公共 Snapshot/Replay、FactId/CommitId 与 runtime state codec：S7A-6。
- Playback/Player/CXC entry、manifest 与生命周期集成：S7A-7。
- 新预算阈值、默认 SolverProfile 限额与最终阶段硬化：S7A-9。

## 5. 当前 SHA hosted 复核

本轮执行 gh run list，并读取以下 run 的元数据、jobs 与选择性日志：

| 工作流 | run | 本轮核对 |
| --- | --- | --- |
| Version Gate | [37211597339](https://github.com/l-zilch-l/Cuexis/actions/runs/37211597339) | 同 SHA completed/success（run list） |
| Linux Quality | [37211597307](https://github.com/l-zilch-l/Cuexis/actions/runs/37211597307) | 同 SHA success；GCC Release/Shared、Clang Shared Debug、ASan/UBSan 等 jobs 成功；所读 Clang Shared Debug 日志含 Capsule canonical byte/structural preimage golden Passed，主 CTest 803/803 |
| Windows MSVC | [37211597458](https://github.com/l-zilch-l/Cuexis/actions/runs/37211597458) | 同 SHA debug/release 的 Configure/Build/Test 成功；所读 release 日志含 Capsule golden Passed、主 CTest 941/941 |
| Windows MinGW | [37211597321](https://github.com/l-zilch-l/Cuexis/actions/runs/37211597321) | 同 SHA debug/release jobs 成功；所读 release 日志含 Capsule golden Passed、主 CTest 941/941 |

Linux 所读 GCC media-tools 日志亦含 Capsule golden Passed，主 CTest 833/833。
golden 测试对固定 Writer bytes 与结构 preimage 校验长度和 SHA-256；这些日志证明已有 golden
在对应矩阵通过，不等于本轮导出并逐字节比较了所有 E1 输入的工具链产物。
本轮未下载 artifact、未验证全部 job 的原始断言、未补跑 clean-first 或 runtime golden。

因此，2026-10-04 报告 §4 的“Clang 对照仍未取证”已被同报告 §5 后续聚焦结果取代；
“hosted 尚未运行”也不是当前 SHA 的事实。CURRENT_STATUS 中把来源修正、Reader/Writer 和
file/memory 全列成未完成任务的摘要由本报告收窄，实施验收仍保持未完成。

## 6. 后续关闭顺序

1. 完成或给出 authoring 双路与 K4 affine lowering 的可定位实现证据。
2. 按 Capsule §11 形成完整 E1 对账，把已存在证据复用并补齐真正缺失的 fixture/断言。
3. 在最终行为 SHA 上运行受影响的聚焦检查与必需矩阵，归档证据和未覆盖项；不移用旧 SHA 数字。
4. 单独登记状态预算缺测及 S7A-9 责任，再按批次验收规则判断 S7A-3 是否可以关闭。

相关设计方向见 [S7A-4 推荐登记](../../../proposals/gameplay-v2-acceptance/S7A-4_RECOMMENDED_DESIGN.md)。

## 7. 本轮文档验证

| 检查 | 结果 |
| --- | --- |
| python -B tools/check_docs.py | exit 0；351 Markdown / 20 candidate JSON/CXT |
| python -B tools/check_docs_status_contract_tests.py | exit 0；4 tests，OK |
| python -B tools/check_docs_target_contract_tests.py | exit 0；2 tests，OK |
| git diff --check | exit 0；仅 Git LF/CRLF 配置提示，无 whitespace error |
| 两份新增文档的显式 whitespace/conflict-marker 扫描 | 0 问题；补足 git diff 不覆盖 untracked 的边界 |

本轮只新增本报告与 S7A-4 推荐登记，修改两处索引、Stage 7 计划及 CURRENT_STATUS；
没有修改 C++、CMake、CI、公共头或 AGENTS.md，没有提交或推送。
