# R3 empty-advance reserve attempt

状态：in progress，2026-10-27。局部改动正确且有收益，整体 20 ms 目标仍未解决，R3 未退出。
返回 [New RPA 报告索引](../README.md)。起点为已冻结的 [R3b 候选](2026-10-27-r3b-certified-prefix-and-cost.md)。
本记录不回填原 R3b 的源/二进制/样本身份。

## 1. 本次负责范围与限制

用户要求由代理提出解决方案，并明确要求尝试后不能解决应及时告知。技术方案无需用户从零提供；
保持原公开语义、适用范围和预算的限定整改继续按既有授权推进。只有需要改变这些合同或进入 R3c
独立语义门时才提交具体产品决定。本次已在成本结果出来后立即告知：这项尝试不足以解决整体预算，
目前没有已验证的完整达标方案。

目标是删除一次可证明的重复副本，不引入通用容器、服务线程或新观测字段。
新目录 `D:/Cuexis-worktree/out/rpa-r3-empty-reserve-20261027/` 保存 `entry.json`、两个入口源副本和设计。
原 R3b 构建/控制器不修改。baseline probe 是新链接的二进制，读取已冻结 R3b 静态库；没有重跑旧登记控制器。

## 2. 最小实现及故障证明

正常 RPA 的 private faultReserve 与 active 的完整 RuntimeState 保持对应，仅状态为 Faulted 且预置 failure error。
prepare、submitRpa、control、restore、Tick/Fold 均发布对应 reserve；只有内部 checking injection 可单独修改 active。

`advanceRecorded` 增加局部 executedTick，选择任何内部 Tick 时立即置位。
仅在本次没有内部 Tick、RPA、没有 testControls、reserve 唯一持有时，保留原 final 状态全量副本，
复用未发布的 reserve。final 复制等可失败工作全部完成后，才修改 reserve 的 processedFrontier 和
lastAdvanceHorizon 两个不分配标量并发布。其他路径仍使用 reserveFault(final)。
因此既未删除 fault reserve，也没有改变 seal/Fold 边界；final 复制失败时旧 reserve 仍是原成功前缀。
checking injection 即使以后去掉 nextCommitId 参数，testControls 仍存在，持续走原完整路径。

无上下文 max gpt-6-luna 完成设计和实际差异审查，未发现所有权/故障漏洞；主代理亲自执行检查。
审查补充的反例是“复用后立即失败，不能先由真实 Tick 重建 reserve 掩盖错误”，现已单列测试。

## 3. 实际验证

- 普通 STATIC Release：Judgement 224 cases / 36458 assertions；RPA 62 cases / 23972 assertions，通过。
- observation STATIC Release：RPA 63 cases / 24013 assertions，通过。
- 新增两个定向用例：普通 892、观测 904 assertions，通过。观测确认普通空推进 reserve 副本=0、
  注入的原路径=1；并比较完整 Snapshot/Replay 及编码字节、旧 owning query/Snapshot、后续真实工作、
  控制、ID 注入和紧接空推进后的 fault 前缀。
- 首稿测试将 empty resume trace 的 finalTraceCut 错写为 1，6 个变体均拒绝。红色 `targeted.log` 保留；
  修正该无效测试输入为 0，后续证据为 `*-final-targeted.log`，未改产品拒绝规则。

新候选普通 STATIC Release 的 Playback 180 cases / 34480 assertions、Host 61 cases / 41173 assertions
以及 architecture 检查均通过。相关日志与两个 Release 构建位于本批新目录。
未以原 R3b 的 Debug/SHARED/consumer 通过，冒充本候选重新验证；本轮未运行这些矩阵。

## 4. 一次固定普通构建成本对照

`registration.json` 在运行前绑定源/工具/库/二进制。baseline/candidate 各新进程一次，固定顺序 B-C，
普通 allocator、宏 OFF、mask=4、显式 HighQoS；每进程 N=64/256/2048，33 条 advance，
第 17 条 H=1119 的显式 checkpoint，目标第 33 条 H=1135，每规模 3 对 origin/hint。
双方各 219 assertions，通过；无追加样本。两个进程共 15.91 秒，原始日志/result/analysis 均保留。

| requirements | baseline 可信恢复，3 样本 | candidate 可信恢复，3 样本 |
| --- | --- | --- |
| 64 | 6.614–6.745 ms | 4.802–5.421 ms |
| 256 | 27.916–30.044 ms | 17.348–18.136 ms |
| 2048 | 300.977–305.827 ms | 168.033–168.568 ms |

这是有限机制诊断，没有 CI/15% 正式噪声门，不是 R4 三窗口；不能把不同日期的旧样本混入。
边界仍是 forkForSeek，未包含 Host/clock/sample/present。256 的本次内核样本低于 20 ms 也不接受端到端。
2048 的 source unbind advance 138.563 ms，reset/base release 146.387 ms；该历史释放成本仍未解决。
本次未重新采内存，不能将旧内存值改标为新候选实测。

## 5. 已确定的下一项技术设计

不能以删除这一次 reserve 副本宣称整体解决。保留当前 20 ms 判据，下一项应先审查 Seek 未发布候选内
连续零内部工作 horizon records 的处理：逐 record 验证完整结果和顺序，保留每条真实日志，仅延迟候选
中相同大 payload 的重复物化，在遇到实际 timer/input/control/Fold、partial 目标或最终发布前形成完整状态。
这是待评审设计，不是已实现或已证明收益；不得删除 suffix 比较、合并公开 Replay records 或复用旧 runScope。
它需要分别证明 frontier/H、FaultPrefix、pending/Fold due 判断、日志截断、坏后缀拒绝和 owning 生命周期。

准备派生索引与 canonical 身份工作、长首条 record、退休释放还须独立处理。上述候选不能绕过这些成本，
也不能自动变成 record 内 continuation。当前困难是保留完整 owning projection 与逐 record 结果的同时，
降低动态副本和退休成本；不是再给检查点设一个更密间隔。本轮如实保留未达标结论，不要求用户代写技术方案。

本批 `product.patch` 在隔离目录完成反向/正向逐字节恢复，绑定另见 `binding.json`。
该检查不修改主树，也不宣称 rollback variant 已重新编译。仅修改本批两个代码/测试路径和导航/状态文档；
R3b 原始产物与 inherited R2 材料保持原字节。没有提交、推送或合并。
