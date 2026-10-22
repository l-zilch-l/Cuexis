# PD-5 Snapshot probe 测试预算最小修复

状态：focused local regression passed / stage C not passed / uncommitted review

日期：2026-10-23（Asia/Shanghai）。起点为干净
`57b604d995f41563652724523d895b36dedf7059`，隔离分支
`codex/pd5-gameplay-40k-admission`。本轮仅修改测试、探针和当前状态文档。

## 最新失败与根因

[现有生命周期最小修复](2026-10-23-minimal-lifecycle-repair.md)已随 57b604d 完成。
其后的固定 57b604d 阶段 C low 运行通过 Packed 编码、Reader/内容构建、Host load、
advance、query、Snapshot 及编码，在 `snapshot-decode` 返回 `codec.budget_exceeded`。
阶段 C 未通过；high、archive/Replay、Seek、显式 close 和 after-close checkpoint 未到达。
旧 [2026-10-22 阶段 C 报告](2026-10-22-gameplay-admission-stage-c.md)保持历史原文。

既有独立诊断 `out/pd5-snapshot-budget-diagnostic-20261023-57b604d/` 的
`measurements.json`、`diagnostic-run.log`、`source/`、保存 wire 和 `verification.json`
共同给出：Snapshot 为 **90,823,328 bytes**，低于原 `maxBytes=268,435,456`；
首个 `Reader::count` 在 wire offset 38 读取 `header.identity` 的 **37,670,961** 个字节元素，
已经超过原 `maxElements=20,000,000`。完整 header 为 **37,671,009**，body 为
**26,111,197**，总计 **63,782,206 elements**。诊断逐字节核对 wire，复制 Reader 保留原 guard。
本轮只读取并绑定这些旧证据，没有重新运行诊断或解码保存的 40k wire。
根因为测试 workload 声明不足，不是恢复格式缺陷；这些计数不是性能接受证据。

## 最小改动

- `tests/pd/pd5_capacity_probe.hpp`：仅新构建的 **R4 TEST ONLY** Snapshot 解码声明从
  `{268435456, 100000, 20000000, true}` 改为
  **`{268435456, 100000, 67108864, true}`**，以有限 `64 * 1024 * 1024` 覆盖已测
  63,782,206。这项测试元素声明确实提高了；revision 3 保持原声明。
- 在真正调用解码前刷新 `PD5_C_SNAPSHOT_DECODE` 日志，保存实际 Snapshot bytes、
  `maxBytes`、`maxRecords`、`maxElements` 和 `testOnly`，失败也能保留准入证据。
- `tests/judgement/fold_recovery_tests.cpp`：复用 FoldFixture、Writer/Reader，增加两个小测试。
  手算 identity 字节、嵌套字符串和多个 body 容器累计 10 elements；验证精确上限成功、
  少一个拒绝、identity 单独超限，以及真实小 Snapshot 的 header/body 联合计数、
  `sameSnapshot` 和逐字节 re-encode 相等。没有新增生产计数器或 40k 合成测试。
- 更新 CURRENT_STATUS 与本目录索引，记录最新失败点和已完成的最小生命周期修复。

产品 codec、API/SDK、身份、Gameplay/Snapshot 语义、七项 Packed 物理上限、四项 hash
ceilings、deadline 与 1.10/15% 门禁未改；生产预算没有获得接受。

## 实际验证

全新 `out/build/pd5-snapshot-probe-repair-release-20261023-57b604d/`，
MSVC/Ninja、Release STATIC、Chart-v5 candidate、warnings-as-errors，最大并行 4。
读取既有环境脚本，将 showIncludes override 复制到新证据目录，核对新 Ninja 的实际前缀。
所有被测二进制及两个改动测试源文件均在新树重新编译，未链接旧冻结库。
首次编译遗漏 Catch2 matcher header，补齐后第二次编译通过；失败日志原样保留。

| Release 选择性回归 | cases | assertions |
| --- | ---: | ---: |
| 新 element-budget 边界 | 2 | 64 |
| Judgement 其余非隐藏用例 | 220 | 36306 |
| Gameplay Packed | 45 | 5476 |
| Chart（排除 capacity / 40000） | 223 | 2400 |
| Playback | 174 | 28011 |
| RPA（排除 measurement） | 50 | 20407 |
| Host protocol | 59 | 121916 |
| ExecutionHost（排除 measurement） | 34 | 34366 |
| PD owner-wait / retirement | 6 | 3537 |
| 合计 | **813** | **252483** |

以上全部通过，覆盖现有格式、语义、golden、恢复和 re-encode 回归。
另有 Python evidence validator 4/4、架构 CTest 1/1、全仓 format、docs 和 diff 检查通过。
没有运行完整 Release CTest、Debug/shared、外部 consumer、GPU 或 Linux 矩阵。
R4 新日志已编译，尚未通过运行 capacity case 作动态验证。

**范围偏差**：既有 `B5 headless EOF tail preserves owners through Pause Resume and terminal execution`
功能测试未标 `measurement`，随筛选后的 ExecutionHost 回归实际输出了 1,188 条 CPU timing
observations 及 close 时间。本任务原定禁止性能采样；该筛选遗漏及完整输出保存在
`execution-host-regression.log`，不删除、不重跑、不用于性能结论。

## 证据与停止边界

原始命令、两次 build、测试及检查日志、`baseline.json`、`source-inventory.json`、
`review.diff` 和 `verification.json` 保存于
`out/pd5-snapshot-probe-repair-20261023-57b604d/`。receipt 绑定基线 HEAD、完整补丁、
源码清单、新二进制/对象、结果日志和既有诊断 measurements/wire SHA256；
未提交补丁身份由 base HEAD 加 diff 哈希表示，不能只用 probe 内的基线 SHA 代替。

保存 wire 的修复后解码交由 parent 审查补丁后安排。没有运行 40k native/capacity、high 或阶段 C
重入，没有新建正式 preparation/registration，没有 commit、push、merge 或删除旧文件。
[公开操作级停止方案](../../../../proposals/implementation-input/stage-07/pd5-operation-stop-contract.md)
继续暂停、未接受、未实施。旧窗口 3、正式测量和主树合并继续停止；最新失败运行未验证显式
Close，旧关闭阻塞仍未归因。PD-5、RPA、生产接受与 Stage 7 关闭均未完成。
