# 2026-10-24 关闭路径 `host.rpa.control_pending` 静态定性

**状态**：completed（静态定性）；**运行**：否。**PD-5 acceptance**：not passed。本文件不修代码、不改判据、不撤销历史失败，也不把未归因的关闭故障说成已解决。

## 1 触发事实

| 运行 | 关闭结果 | 说明 |
| --- | --- | --- |
| 冻结验收 low / high | 2 / 2 成功 | 固定 profile，`out/build/pd5-stage-c-snapshot-release-20261023-54e8c2a` |
| 诊断 pass-2（40k） | 3 / 4 成功，1 次失败 | `assertions: 52 \| 51 passed \| 1 failed`，native exit 42，进程运行 1,913.1 s |
| 512 基线（内核修复前） | 1 次失败，基线 1024 因此未运行 | 同码同消息；`2026-10-23-execution-core-diagnostic-and-repair.md` §诊断 |
| 修复后 64–1,024 五配置 | 5 / 5 成功 | 同一诊断脚本，`advance` 与诊断关闭全部完成 |
| 规模阶梯 64 / 2,048 / 8,192（本探针） | 全部成功 | 同一探针，规模小，未出现 |

失败原文（行号是该次二进制内的行号，当前探针源码已因诊断插桩而前移）：

```
pd5_capacity_probe.hpp(184): failed: unexpected exception with message:
'host.rpa.control_pending: Control cannot enter its software fence'
```

失败点是探针的 `trace.take(host->close())`。该次运行在冻结探针之上又加了诊断插桩，因此这次失败不能直接归到冻结二进制；冻结二进制的两侧关闭都成功。这也是目前为止唯一一次 40k 关闭失败的直接记录，样本是 1/4。

## 2 代码路径

关闭由宿主 owner 线程发起，软件围栏由 SessionWorker 进入：

1. `ExecutionHost::close()`（`app/player/src/execution_host.cpp` L1040–1107）先 `begin(ControlKind::close, …)`，把 close 记录发布为 `ControlStatus::preparing`，并在宿主协调器里留下该 `operationId`。
2. 随后依次做 capture `cut`、交付 `flush`、audio `stop` / `isolate`，再发 `SessionSoftwareRetire`。
3. SessionWorker 的 `fence(proof)`（`app/execution_host/session_worker.cpp` L249–259）对非 load/reload 种类调用 `HostCoordinationAccess::enterControlFence(session, proof)`（`app/execution_host/host_candidate.cpp` L72–76）。
4. 后者进 `PlaybackSession::enterExecutionFence`（`engine/playback/src/playback_session.cpp` L1689–1710），在这里返回被观察到的错误码与文案。

`proof()` 的实现是 `host_->query(control_.operationId)`（`app/player/src/execution_host.cpp` L171–173），即**每一步都重新从宿主协调器读当前记录**，不是开始时的快照。这一点决定了下面哪条前置条件最可能被触发。

## 3 前置条件逐条定性

`enterExecutionFence` 在这些条件成立时抛出同一条消息：

```cpp
if (!s.ownerThread.isCurrent()) → ownerError("control_fence")          // 不同错误码，排除
if (s.operationActive || s.hostFence || !s.acceptedHostControl ||
    *s.acceptedHostControl != proof.request || proof.status != ControlStatus::preparing ||
    proof.request.kind == ControlKind::load || proof.request.kind == ControlKind::reload)
    → Error{"host.rpa.control_pending", "Control cannot enter its software fence"}
```

| 前置条件 | 在本次关闭路径上的可达性 |
| --- | --- |
| 非 owner 线程 | 抛 `ownerError("control_fence")`，错误码不同，**排除** |
| 种类为 load / reload | 本次是 close，**排除** |
| `s.operationActive` | 需要同时有另一个 PlaybackSession 操作在飞。SessionWorker 串行执行会话命令，静态上只剩重入或 pump 期间插入的可能，**未见指标** |
| `s.hostFence` 已存在 | 需要同一次关闭被围栏两次（例如围栏成功后重试）。本次只发一次 retire，**未见指标** |
| `!s.acceptedHostControl` | 需要 close 记录已被清空或从未接受。`begin` 与 accept 都成功才会走到 probe 的 `close`，**未见指标** |
| `*s.acceptedHostControl != proof.request` | 需要在 close 之后又有控制被接受并覆盖 `acceptedHostControl`。探针在 close 前最后一次控制是 `seek`，之后只有读与采样，**未见指标** |
| `proof.status != preparing` | **在路径内可静态到达**，见 §4 |

六条「未见指标」不是「已排除」：失败日志只带共用文案，没记录 `operationId`、实际 `status`、`operationActive` 或 `hostFence` 的取值，也没有失败瞬间的协调器状态转储。因此在静态范围内只能缩小到「最可能」，不能定案。

## 4 主假设：交付 flush 失败把 close 记录挂起

`ExecutionHost::close()` 在软件围栏之前唯一会改动宿主协调器记录的语句是交付 flush 的失败分支（`app/player/src/execution_host.cpp` L1068–1078）：

```cpp
if (capture_ && capture_->fault().reason == FaultSlot::Reason::none && lastObservation_) {
    try { flush(*lastObservation_, false); }
    catch (const Failure& failure) {
        if (sessionJob_) throw;
        closeDeliveryError_ = failure.error;
        take(host_->suspend(control_.operationId));   // ← 记录状态改为 suspended
        // 不伪造 ACK
    }
}
```

`HostCoordinator::suspend`（`app/execution_host/host_protocol.cpp` L705–712）把该记录的 `receipt.status` 置为 `ControlStatus::suspended`，close 种类下宿主状态保持 `HostState::closing`，所以后续步骤仍会继续。随后代码仍继续 audio `stop`/`isolate` 与 `SessionSoftwareRetire`；worker 进入 `fence(proof)` 时，`proof()` 重新查询到的是 `suspended`，于是 `proof.status != preparing` 成立，抛出被观察到的同一条消息。

这条链路与本例的形态吻合：它只在交付侧出现拒绝时发生，而交付拒绝与规模、时序、诊断插桩都相关，因此表现为偶发而不是必现；它也不需要任何并发条件，单线程复现即可解释。冻结验收的两侧没有触发 flush 失败，所以关闭成功。

未定案的原因同样明确：失败日志没有 `closeDeliveryError_` 的值，也没有 `suspend` 的调用记录。要证伪或证实这条假设，最小成本是在关闭失败时输出 `closeDeliveryError_`、`proof().status` 与协调器状态，而不是重跑 40k；本文件不做这个改动，也不为此新建运行。

## 5 对正式矩阵的风险

正式 9 对在 reference profile 上跑，不是 40k。关闭失败不能按规模解释：512 之前的基线（内核修复前）就出过同码同消息，而修复后 64–1,024 的五个配置全部关闭成功、40k 冻结两侧也成功。所以现在把它定性为**已登记的预声明风险**，不是矩阵阻塞项——但样本总量只有「40k 4 次中 1 次失败、512 旧构建 1 次、其余全成功」，区间很宽，不能反过来宣称 reference 规模安全。

量级要摆明：若故障率真是 1/4，9 个进程全部干净关闭的概率约 7.5%（0.75⁹）。也就是说，一旦它在 reference 规模复现，矩阵在「不允许重试直到通过」的原规则下基本不可执行。这是必须在采样前解决的口径问题，不是采样后能补的解释。

另有一条本轮无法排除的干扰：内核修复后的成功集合与失败集合分属不同构建，修复本身可能改变了关闭路径的时序（`2026-10-23-execution-core-diagnostic-and-repair.md` 记录修复后五配置关闭全过）。因此不能把旧 512 的失败当作同一个未修缺陷，也不能当作已修好。

## 6 交回 owner 的三个选项

1. **保持无重试**：任一进程关闭失败即该进程无效，窗口按无效处理并在报告中记失败。口径最干净，但若故障率如上，PD-5 会以「无法完成正式窗口」收尾。
2. **预声明「无效进程 + 整窗一次重跑」**：仅当失败原因是工具/关闭故障（非性能超限）时，整窗重跑一次，失败窗口与重跑窗口都完整保留、都进报告，且不按结果择一。这不是「重试直到通过」：重跑次数封顶一次、触发条件预先写死、两次结果都必须披露。采用这种方式必须在采样**之前**写进登记，不能事后补。
3. **先 pilot 判别**：先用一次有限 pilot 观察 reference 规模的关闭行为，再决定 1 或 2。pilot 本身已在待授权清单里，不需要为这个判断新增运行。

建议按 3 走：pilot 是既有计划内的下一步，能直接判别故障是否与规模绑定；若 pilot 干净，1 与 2 的差别对矩阵影响很小，按 1 更干净；若 pilot 复现，则只能按 2，并且必须在正式窗口登记前落地。

## 7 非声明

- 本文件没有修代码、没有新增日志、没有重跑 40k、没有重跑 pilot 或任何窗口。
- 没有把 `host.rpa.control_pending` 定性为根因已找到：主假设可静态到达，但缺少能证伪它的运行时字段。
- 没有把冻结两侧的关闭成功当作关闭路径已稳定；旧约 25 分钟关闭故障仍未归因，本文件不覆盖它。
- 没有改变「不重试直到通过」「不替换慢样本」「不撤销历史失败」三条规则。
- 不构成 P5 通过、PD-5 通过、RPA、生产或 Stage 7 关闭，也不授权提交、推送或合并。
