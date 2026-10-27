# R3 implementation and verification

状态：in progress。用户于 2026-10-26 授权本次接手完成 R3。
本登记从 [总计划](../plan.md) §8 消费范围和依赖，不扩展到 R4 正式接受或 Stage 7 关闭。
返回 [New RPA 索引](../README.md)。

R3b 的首个候选见[显式可信恢复提案](trusted-restore-candidate.md)，已按总计划 §8.2 允许的最终 archive 工厂入口实施并完成本地核验；当前候选预算未通过，R3 未退出，按 §12.2 交裁定包。不再要求重复授权，也不声称自然 checkpoint 已覆盖首次 Seek。
2026-10-27 本地功能/兼容、反例、宏隔离和成本范围见[核验报告](../../../../../stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-27-r3a-recovery-and-prepare.md)。
后续可信前缀候选另见 [R3b 核验报告](../../../../../stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-27-r3b-certified-prefix-and-cost.md)，不复用 R3a 的性能身份。
后续 [空推进 reserve 限定尝试](../../../../../stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-27-r3-empty-reserve-attempt.md)
已取得局部收益，2048 的可信内核恢复仍约 168 ms。代理继续负责技术设计，预算未解决已及时告知，未要求用户代写方案。
历史 [高 requirements 耗时归因](../../../../../stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-27-r3-high-requirements-attribution.md)
确认主要成本为后缀最终化复制、完整结果比较和新内核准备；本批仅诊断，未进一步改产品或进入 R3c。

2026-10-27 用户另行授权在 `D:/Cuexis-redesign` / `Cuexis-redesign` 内重构，禁止涉及其他分支。
当前候选改为分页共享 phases/facts/matchers、共享不可变准备、自然日志锚点及封存来源精确 SHA 缓存；
见[重构核验报告](../../../../../stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-27-r3-redesign.md)。
首次 identity 成本完整计入请求，不通过预热或放宽 20 ms 预算宣告通过。下文历史起点和旧证据保持不变。
后续[首次身份归因](../../../../../stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-27-r3-cold-identity-attribution.md)
已确认完整归档重复结果及 SHA/FNV 全量扫描为主要成本；该轮只做独立诊断，未进一步改产品。

用户随后授权先提交恢复点再比较不同优化方案。`198ab96` 固定 SHA 加速和有界编码器。
当前内置宿主采用运行时 owning source binding，canonical SHA 路径保留作对照及外部消费。
来源绑定不是 validation/native recovery 凭证，wire 不变；candidate C++ 布局改变须整体 rebuild。
固定 33/129 条历史与 64/256/2048 requirements 的首次/重复 Seek 对照及最终回归见
[来源绑定实验](../../../../../stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-27-r3-source-binding-experiment.md)。
下表 S0 为原诊断路径，当前所选来源路径不执行 canonical 编码/hash。局部延迟改善不代表 R3/R4 退出。

后续[完整 Packed 索引优化](../../../../../stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-27-r3-packed-index-optimization.md)
已按精确归因替换 Graph/owner/声明重复查找；普通完整入口的 10K 准备为 2.90–3.01 s，
40K 为约 27.3 s，首次请求到 CPU 测试呈现约 16.2–16.5 ms。
四配置相关回归、字节指纹和差异报告对照通过；prepare 内部剩余成本待进一步细分，
本地规模点值不作为 R3/R4 退出或设备/生产接受。


## 1. 起点和独立差异

实施树 `D:/Cuexis-worktree`，分支 `stage-7`，起始 HEAD
`c4bc5d4456ffe4778a90f9bae410638c0016a8eb`。入口只读核验通过：两棵树、75 个绑定文件、
146 个归档后缺失项相符。已有 R2 接受和技术收尾文档保持原字节。
`out/rpa-r3-20261026/entry.json`、`entry-files.zip`、`entry-dirty.patch` 保存起始字节和 2186 路径清单。
原历史证据树不作为实施树，不重跑旧控制器。代码和观测分别保留可撤差异。

## 2. 相位和 owning 出口

| 相位 | 实际路径 | 本批处理 |
| --- | --- | --- |
| S0 | Host 编码 Replay 和 SHA-256 | 保持原 canonical bytes/hash；sealedIdentityKnown 的旧未知量不改写 |
| S1 | Host capture/软件/renderer/audio fence 与确认 | 保留 R1 路径、代数、超时和当前线程拓扑 |
| S2 | Playback makeGameplaySession；内核 prepare；Impl 派生索引、RecoveryInputs、初始 reserve | 从真实调用处新增 prepare/index/配置复制计数及嵌套时间，区分被丢弃候选 |
| S3 | 全源 shape/eventCount/dialect；prefix 执行、Snapshot 恢复与比较 | 先验证早期 cut 闭包；外部输入继续全验证 |
| S4 | submit/control/advance；draft/reserve/Fold/finalization；32-record 尾块 | 实际复制点分类计数；嵌套 batch/admission/trace 计元素，不冒充 bytes |
| S5 | owning query、readSince、projection、swap 和旧 owner 释放 | 保护现有共享 query 和增量出版；不每帧 flatten |
| S6 | 新 clock/context、sample/目标帧 | 内核计时不能承担请求到目标画面的 20 ms 接受 |

PreparedGameplay、RecoveryInputs、query 的 RuntimeState、ReplayArchive、SnapshotPayload 和
Checkpoint 都是独立 owning 出口。query 共享已封存 active，captureState 明确物化全量 DTO，
archive 明确物化日志。旧结果持有自身 owner，关闭/替换不能修改其字节或复活旧 scope。

## 3. 动态写集与提交门

| 成员 | 写集和独占工作 | 成功前缀/故障处理 |
| --- | --- | --- |
| matchers、activeRequirements、timerCursor | activation/timer/input/full Tick | draft 独占；pre-seal 失败留旧前缀 |
| coverageHistory、contacts、ownership、phases/resources | input/timer/reconciliation/ledger，可覆写、删除 | 不视为 append-only；reserve 在 seal 前完成 |
| facts/receipts/observers、nextCommitId/exhaustion | ledger 提交和 ID 消耗 | 旧 owning projection 不变；Fold 失败仍保存已 seal kernel |
| Fold、RPA foldAtTickStart/baseTick/foldedControlCount | 同 Tick control 可回到 Tick 起始 Fold 并重算 | 第二 fault reserve 和 Fold 提交独立，不删除 reserve |
| accepted/quarantine/deferred/controls/pending | admission、resume、输入分派，有删除和跨未来输入关系 | active、pending、日志在各现有事务边界一致提交 |
| signalVisibility | 仅内部 checking signalTicks 注入写入，不是正常产品事件 | 普通路径为空；测试注入恢复并不由 DTO 自动闭合，不用其证明可信 checkpoint |
| recording/logTail、seekSource | 受控日志 append 和外部完整源引用 | logTail 是实际执行前缀，seekSource 是完整源；成功新操作清除 seekSource |

本表是现有表示审计，尚不授权把这些容器统一替换为持久容器。动态表示候选仍须逐成员成本、
故障、物化与生命周期证明。R3b 可信来源、全状态闭包、首次成本/容量门及 R3c 语义门继续适用。

## 4. 已复现的恢复链与最小修复

固定小 fixture 包含 press@900、未来 release@1000，H=903、pause/resume@cut900 及未来 advance。
四种目标为 partial H=902、H=903 的 2/3/4 条完整记录。原点参考直接执行相同前缀。
原源码下四种 Snapshot 均混入未来源历史，sameSnapshot 不等，decodeSnapshot 均失败：
`out/rpa-r3-20261026/early-cut-red.log`，1 case / 163 assertions，8 failures。

最小修复只让 captureState 从 logTail 物化已应用历史，包含真实 partial advance；archive()
仍返回完整源。未改变公开 DTO、wire revision、身份算法或截断源。首次修复验证为 1 case /
259 assertions，通过；后续补 Fold/故障/旧值和完整适用矩阵，不能凭此宣布 R3 已完成。

## 5. 验证与成本登记

先功能及结构计数，再普通 allocator 时间和独立内存取证。观测宏 ON 的计数/相位不接受正式性能；
普通构建必须编译宏 OFF。计数只记录真实发生的调用，旧 R0 粗粒度字段不改写历史定义。
时间字段嵌套，不求和。未知 configValues、公开 DTO 字节和嵌套字符串成本继续标缺证据。

小规模观测固定为一次结构用例；不重跑到通过，失败保留并按新源码身份继续功能修复。
普通时间对照在执行前另存源/工具/编译器/库/二进制/fixture 身份、固定顺序次数和停止条件。
40k 相位、ETW/WPR、设备及 R4 正式窗口不由这些小规模结果代替。

配置首尝试因 PowerShell 未引用 `exp.candidate` 失败，错误日志和失败目录保留；
新有效 Debug 构建为 `out/build/rpa-r3-entry-debug-v2-20261026`。
