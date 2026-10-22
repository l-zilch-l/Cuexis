# PD-5 执行内核受控诊断、修复与关闭合同阻塞交回

状态：kernel repair verified locally / lifecycle blocked / owner verification pending

快照日期：2026-10-23

本轮完成 small/medium low 的内部计时、执行内核复杂度修复及固定源码 Release 回归。
worker 关闭修复尚未完成：静态候选不能覆盖 Windows DLL 边界，跨 DLL 候选新增私有导出，
不符合现有合同，均已撤回并保存 diff。**没有重进阶段 C，没有运行新的 40k low/high。**
[原阶段 C 失败报告](2026-10-22-gameplay-admission-stage-c.md)及冻结证据保持原样。

## 固定基线与证据身份

开工重新核对隔离分支 `codex/pd5-gameplay-40k-admission`，实际 HEAD 为
`7dca8a013512905740f2ce16b4e2e5798f01a2cb`，工作树干净；未 reset/clean。
F1、C2 和 master 的 worktree 映射记录在 `baseline-worktrees.txt`。
本轮 evidence root 为 `out/pd5-execution-repair-20261023-7dca8a0/`，创建前确认不存在。
开始时冻结 12 个历史证据目录、6783 个文件；交回再次核对内容及文件集合未变。

诊断基线源码为 `099183e2cee4fbaf1cf898847edfb09fd5b94124`：
`ca77f48` 增加默认关闭、仅测试构建可开启的内部计时；`099183e` 串行化诊断与探针输出。
基线实际 source/build 字节已封存为 `baseline/source.zip`、`baseline/build.zip`。
其 loose build 后来用于开发 red/green，不能用该目录当前字节冒充基线；归档逐项哈希已验证。

保留的内核修复提交为 `be84f7a56449ed6b5e295119d0adee730d44dac4`。
最终被测源码为 `d539339bc1fc537127592b4b3e22b69ea7d91586`，与 `be84f7a` 的树一致。
`3df6046`、`9d51cd3` 是已撤回的 worker 候选，`d539339` 明确撤回两者；没有重写历史。
交回提交只新增本报告、更新 PD 索引与 CURRENT_STATUS。

最终两套全新构建分别是：

- `out/build/pd5-execution-core-diag-release-20261023/`：Release / STATIC，诊断 ON。
- `out/build/pd5-execution-core-release-20261023/`：Release / STATIC，诊断 OFF。

两者均绑定最终源码 SHA，warnings-as-errors、Chart-v5 candidate 与 allocation lifetime observer
开启，PD metrics 和 trace harness 关闭，使用已核验的 MSVC showIncludes override。
每套封存 2047 个源码文件；诊断构建 24 个载荷，关闭计时的 Release 构建 33 个载荷。
实际 Release 探针 SHA-256 为
`ba8ab79a13b3c897f17304ab57c05d36226fe540c193aef745669571d5af0d39`。
源码和被测构建在各自运行期间保持不变，绑定见 `optimized/binding.json` 与 `release/binding.json`。

另保留两次未成功的 configure 日志：一次未显式 STATIC，触发 observer 的既有配置拒绝；
一次 PowerShell 原生参数将 `$sha` 作为字面值传入，触发完整 SHA 拒绝。
它们均未构建或运行探针；修正命令后使用新的目录，没有覆盖失败日志。

## 受控复现与结论边界

每轮预声明 low 的 64、128、256、512、1024，逐级、各独立进程一次，首个配置失败即停止。
显式设置 entities、reuse=low、revision=4、expected source SHA、原件目录和 diagnostic-only。
保留原 fixture 构造段，走正式 R4 Writer、Reader、GameplayContent、Host load 和首次 advance；
诊断模式只在首次 advance 后关闭，不声称完成 Snapshot/Seek/Replay 或阶段 C。
内部记录 coherent dispatch/advance、awaitSession、kernel、worker/shutdown 的起止、关联 job ID、
timer/cursor、tick 和分段耗时。scope end 只代表函数离开；成功还需实际 Result 和探针阶段记录。

| 实体数 | 逻辑 timers | distinct ticks | 修复前 kernel ms | 修复后 kernel ms | 完整状态事务：前 → 后 |
| --- | --- | --- | --- | --- | --- |
| 64 | 256 | 65 | 21.5174 | 2.2284 | 65 → 3 |
| 128 | 512 | 129 | 84.5491 | 3.8147 | 129 → 3 |
| 256 | 1024 | 257 | 344.0297 | 6.7597 | 257 → 3 |
| 512 | 2048 | 513 | 1458.0390 | 14.0373 | 513 → 3 |
| 1024 | 4096 | 1025 | 未运行 | 28.6881 | 未运行 → 3 |

基线 512 的 advance 已成功，但 diagnostic-close 返回 `host.rpa.control_pending`，消息为
`Control cannot enter its software fence`；该进程正常报告失败并退出。因此基线 1024 未运行。
该消息与旧 40k 的通用 await 错误不同，日志没有 await deadline 事件；本轮不推断同一根因，
也未修改 clock/profile 使其通过。修复后五个配置均完成 advance 和诊断关闭。

核查纠正了“每个 timer 复制一次”的描述：原实现已按同 tick 批处理，复制和全量验证发生在
**每个不同 tick**；low 的 `timing.end += i` 导致 N+1 个 tick、4N 个逻辑 timer。
512 基线的 copy/fault-reserve/fold-copy 分段合计约 838 ms，validate 约 218 ms，timer 约 52 ms；
它们解释了大量重复状态工作。以上是有计时开销的墙钟观察，不是采样 CPU 归因或正式性能估计。
64–512 前后 R4 原件逐字节相同，全部 Packed 预算、semantic digest、单次 SHA 观测也相同。
1024 没有基线配对值。原始日志、失败、未运行状态和比较分别见 `baseline/`、`optimized/`、
`baseline-analysis.json`、`curve-comparison.json`。

这些证据支持此受控 workload 的算法复杂度判断；没有直接重现旧 40k 的 30 秒 await 分支，
没有证明任意 workload 均线性，也没有据此接受 1.10/15% 性能门禁。

## 保留的内核改动与语义验证

requirements/timers 使用准备后不变的数据预建实例内索引；phase 通过已验证布局的 offset 查找。
准备及不可信 Snapshot restore 仍验证布局与身份顺序，旧恢复拒绝路径保持。
同一 activation tick 先追加 active requirements，再排序一次，输入处理前顺序与原实现一致。

只有已经完成一次无新 Fact 的 Fold、没有 pending input/contact/ownership/hook，且后续仅是
已结束 phase 的 hardDeadline 时，才复用本次操作内未外借的私有可写状态。
active 与 fault reserve 同步逐 tick 更新 cursor、frontier、workTick 和 RPA Fold 起始状态；
逻辑 timer 没有漏计。故障注入、signal、重复 phase/register 等路径保留完整事务。
持有旧 query 的调用方看到的状态不改；状态变化 tick 仍走原复制、验证、seal、Fold 顺序。
缓存是执行内核实例的派生索引，沿原执行状态寿命释放；没有增加 capsule hash 工作区或第二编码器。

新增四个有界回归覆盖 generic/RPA、完整 DTO、Snapshot/Replay 字节、split advance、旧视图保活、
hook/input、五个 tick 故障点、finalization、重复 phase、register、signal、decode/recover/evaluate/seek。
优化与强制原事务路径比较完整状态；64 requirements 的逻辑 timer=256，完整事务 ≤4，
idle deadline tick ≥60。原实现 red 为 65 次完整事务、0 个 idle tick，保留失败日志；修复后通过。
独立 Standards/Spec 对保留的内核与诊断差异均未发现可行动项，审阅范围及限制保存在 `reviews.md`。

## worker 关闭：未完成，停止边界保留

未改变 Host 30 秒 deadline、超时后原 job ID 查询和 Closing 语义，没有 timeout-as-Cancel、
detach、强制释放 owner 状态或假报 Closed。

候选 `3df6046` 用线程局部借用的 stop flag 在 tick/timer/Fold 检查点放弃未 seal 草稿，
仅显式 requestShutdown 生效。静态测试验证首 tick/后续 tick 的 fault prefix、旧视图和 binding 寿命；
red/green、221 个 Judgement 用例与 33 个 Host 用例的开发日志保留。
该候选的 header TLS 在 Windows EXE 与 Playback DLL 各有实例，不能证明 shared worker 能中断。

候选 `9d51cd3` 将上下文与诊断状态集中到 Core 内部访问器，修复 TLS 分裂，
但新增了实际的私有 DLL 导出。依据
[ADR 0033](../../../../adr/0033-cpp-shared-library-preview-boundary.md) 的 export 规则和
[realtime host §7](../../../../api/realtime-host-boundary.md)，
不能用“头不安装”或既有内部 helper 推定本轮获准新增私有导出。
该项经 Spec 审阅指出后，两份候选已全部撤回；最终源码没有这套 stop API、检查点、测试 gate 或新增导出。

候选 diff、提交序列与完整源码仍在 `worker-candidate-*.patch`、`worker-candidate-source.zip`，
可具体核验。**人工阻塞的 Host deadline/worker 退出复现和 shared 验证均未执行，不能报告通过。**
需要 owner 单独接受跨 DLL 的停止信号及终止结果合同，之后才能继续关闭实现；本轮未升级 SDK/API。
即便后续采用协作停止，也不能承诺打断任意阻塞 provider、分配器或 OS 调用。
现有长 advance 后等待 worker join 的风险因此仍保留，不授权重新进入阶段 C。

## 最终实际验证与交回

关闭内部计时的全新 Release 上：

- 1/low、1/high、64/low、64/high 各独立进程一次，完整 R4 encode/fromPacked/load/advance/query、
  Snapshot encode/decode、archive/evaluateReplay/sameResult、pause/replayCut/seek、sample/present、
  seek 后 sameResult、close/reset、真实 after-close memory checkpoint 均通过。
- 原有 1/low、64/high 的 R4 文件与冻结阶段 C plumbing 原件相同；不替代 40k 验收。
- 九组相关 Release 默认套件：Packed 45、Playback 174、Chart 227、Judgement 220、CXC 64、
  RPA 52、Host protocol 59、ExecutionHost 33、PD host 7，共 **881 cases / 295584 assertions**，全通过。
  未宣称全仓 CTest；隐藏 capacity 测试由前述显式配置单独执行。
- Catch 实际发现与 `ctest -N` 留档；据发现结果运行 `cuexis_architecture_tests`，1/1 通过。
- 全仓 `cuexis_format_check`、4 个 Python evidence-validator 测试、`check_docs.py`、
  `git diff --check` 通过。完整命令、退出码和原始日志在本轮 evidence root。

保留原 40k 构造段、共享 fixture 默认 R3、旧 golden、旧 revision/wire/语义身份、七项物理预算、
四维 accepted ceilings、F1/C2 和历史证据。未创建 preparation/registration，未恢复窗口 3，
未合并/推送，未修改主树或 SDK/版本。源/二进制归档、逐项绑定、最终 provenance 和 packet receipt
共同固定本次交回；验证器只读，不重跑 native。

本轮只提交内核修复与受控证据，关闭问题保持具体阻塞。阶段 C 仍 failed/stopped；
不声明原样 40k 全链路通过、性能接受、PD-5 完成、RPA/生产接受或 Stage 7 关闭。
