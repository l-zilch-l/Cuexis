# S7A-3 / S7A-4 complete execution design handoff

状态：completed；仅本轮设计文档交付，不是实施验收

更新日期：2026-10-05

快照：`stage-7` / `6b11d102f6dcac4d9782cd03d0bae15add54af28`，新增设计在工作区未提交。
历史当前 SHA 实现/hosted 证据由[余项报告](2026-10-05-s7a-3-remaining-evidence.md) 拥有；
本报告不把该证据移用于新 execution profile。

后续同日[再次歧义复核](2026-10-05-s7a-3-4-ambiguity-audit.md)发现并修订本版遗漏；
下面检查数字保留为当时证据，最新语义按现行补充 Spec/ABI/Plan 和后续复核报告读取。

## 1. 授权与交付

owner 要求一套完整、健全、能交给后续模型实施的方案，消除 S7A-3 余项和 S7A-4 的不明确语义。
本轮只写文档与设计，不修改 C++/CMake/Schema/CI、不提交/推送或实施新功能。
设计选择不冒充第 1–7 轮旧裁定，也不宣称两批或整阶段完成。

- [ADR 0045](../../../adr/0045-gameplay-v2-execution-profile.md)：完整方案选择、版本增量与影响。
- [Execution Spec](../../../formats/gameplay-v2-execution-profile.md)：late 边界、输入/contact/资源、
  considered/consumed、Pattern、T4 失败传播、timer/ranks/Fact、atomic seal 与 Fold 边界。
- [Author profile](../../../formats/gameplay-v2-author-profile.md)：严格 candidate extension、双路 endpoint、
  全实例稳定 ID、真实 CXT finite emission/Binding、affine lowering、错误与等价范围。
- [Typed supplement](../../../api/gameplay-v2-execution-types.md)：字段宽度、optional/owning、接口/状态矩阵、
  详细诊断登记和私有 tools target。
- [Capsule §12](../../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md)：revision3 完整新增物理字段/hash/互拒，
  revision2 合同与已有 golden 保留。
- [实施交接](../../../archive/stage-07-planning/s7a-3-4-implementation-handoff.md)：A–G 顺序卡、
  正反 golden、失败注入、逐项 E1/后续 runtime/预算证据、命令和新对话指令。

## 2. 本轮明确补齐的相互影响

maxQueueHop 保持 TickSpan，时间跨度和一次转发次数分开；commitTick 为逻辑提交 Tick，
advance horizon/observationTick/dispatchTick 分开。保留同 Tick distinct 输入拒绝，
不能以 arrival order 多输入仲裁。原始时刻的 early release 不补回 body 覆盖。

owner 控制先路由，避免 free contender 吞掉 owner release；已 seal body outcome 不重写。
Tap 瞬时 lease 不伪造 contact/Held；physical contact 和 lease 生命周期分离。
timers 先执行与 Ledger observation-before-timer rank 均保留，以全 Tick immutable seal 保持一致。
无观测的 timer/依赖 error absent；新 phase targets 明确消除窗口中点/end 猜测。

运行新增字段不能塞进旧 revision2，因此明确新增 candidate revision3 与独立 semantic preimage。
旧产品/旧 candidate golden/default entry/SDK 不因此切换。source 和 runtime 关闭证据均待实施。
arm/deadline 包含性是 prepare 硬 gate；状态数缺测不当超限或通过，仍由 S7A-9 接受生产阈值。

## 3. 验证证据与限制

本轮已经运行：

| 检查 | 结果 |
| --- | --- |
| check_docs.py，写报告前 | exit0；356 Markdown / 20 candidate JSON/CXT |
| check_docs_status_contract_tests.py | exit0；4 tests OK |
| check_docs_target_contract_tests.py | exit0；2 tests OK |
| git diff --check | exit0；只有 LF/CRLF 配置提示 |
| 五份新核心文档 whitespace/conflict 扫描 | 0 问题，覆盖 untracked 文件 |
| 文档算例校核 | 5 个 late 边界 + 6 个 affine ranks 与列出的期望完全相同 |

最终导航、本报告与 Desktop 最新交接加入后复验：check_docs exit0（357 Markdown / 20 candidate
JSON/CXT），状态合同 4 tests OK，目标合同 2 tests OK，git diff --check exit0；
全部 8 份 untracked Markdown 显式 whitespace/conflict 扫描 0 问题。
Desktop 原文保留，末尾加入最新入口与适用范围；本次未修改 memory。
算例校核不是 L2 或 kernel runtime 验证；本轮没有 C++ builds、CTest、GPU、设备、容量或新 hosted。
“方案完整”指本范围所有 identified semantic choices 都有唯一合同与验收入口，
不承诺尚未实施的代码没有 bug 或尚未测量的性能达到生产阈值。

后续由交接 A–G 实现并给证据；S7A-5/6/7/9、Stage 6 owner-only gate 和整阶段关闭保持原归属。
