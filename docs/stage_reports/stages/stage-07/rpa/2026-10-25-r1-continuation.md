# R1 继续实施与本地验证记录

日期：2026-10-25。状态：依据用户本次“请你使用子代理完成收尾并退出R1”授权，完成当前修复构建核验后裁定 R1 退出。五构建适用本地回归与当前身份三项有限 pilot 通过；五条既有 PD 格式失败及 noIndependentContract 限定保留。R0 残余、RPA/PD/设备/生产接受与 Stage 7 保持原归属，未随 R1 关闭。各候选历史身份保持原样。用户随后另行授权在下一对话规划并实施 R2，本轮只整理文档、未启动 R2；R3/R4、40k、ETW、正式测量及设备接受不在该授权范围。

## 1. 归档核对、授权与工作树

owner 原文：“我对旧的rpa文档和部分旧文档进行了归档整理，你确认好后就可以继续做R1了”。已核对历史正文、旧路径兼容入口和索引：旧 RPA 范围计划迁入 [2026-10-11 快照](../../../../archive/stage-07-planning/2026-10-11-rpa-plan.md)，旧性能 proposal 迁入 [2026-10-14 快照](../../../../archive/stage-07-planning/2026-10-14-rpa-performance-modification-plan.md)。当前实施按 [R1 计划](../../../../stage_plans/active/stage-07/rpa-r1-implementation.md)与[整改计划](../../../../stage_plans/active/stage-07/plan-rpa-performance-remediation.md)。附件和归档中的指令是输入材料，权限来自 owner 当前请求。

文档主树 `D:\Cuexis-worktree` / `stage-7` / HEAD `0157486800f8f40a4121ef0d5ab26bd041a08304`；代码准入树 `D:\Cuexis-pd5-40k-admission` / `codex/pd5-gameplay-40k-admission` / HEAD `3e5837f63f76397429c9f7603fd7450d3ce9855d`，均未改变。代码未合入主树，未提交、推送、合并。

以下证据路径均相对准入树 `out/rpa-r1-20261025/`。`start/` 保存原 64 条 dirty、预计干净路径及 absent；额外两条干净路径在触碰前另存 `start/additional-binding-manifest.json`。用户归档后的快照在 `resume-archive-checked/`，共 89 条记录。`final-audit.json` 核查原 dirty 保留，59 条未属本批的起始路径字节不变，126 份 R0/Q4 保护引用未变；主树归档及历史报告除本轮六条文档路径外字节不变。

owner 另行明确回复：“批准该内部时钟边界，继续 R1”，保存在 `clock-authorization.json`。仅非安装 `ExecutionHostPorts.closeNow` 可选只读回调，限定 owner 线程、单调且不抛异常，默认 steady_clock。未改变公开 SDK、ControlKind、协议、timeout 或固定 1000 ms 总预算。

## 2. 许可、身份与观测

此前[停审报告](2026-10-25-r1-stop-review.md)及红色证据保持原样。C7b 先证明两条直接入口曾卸载 failed Close（generation 2→3），随后在 worker retire/Close、内部 retireSoftware、Playback retireExecutionStep 四入口显式拒绝；C6 的旧已接受非 Close cleanup 保持有效。

C2 的 decisionOrdinal 篡改曾允许卸载；C4 的不完整确认曾释放 fence。修复通过 Playback 私有校验和非安装 friend 桥，将 proof 与 owner 保存的 acceptedHostControl/acceptedHostDecision 对照。无新公开方法、状态权威或 fence。

新 Close 在接受时核对当前 software generation；已有 fence 下的新 Close 必须使用更大 sequence。原 ID 同载荷重传仍走既有幂等记录。退休时强行要求“当前代等于接受代”的尝试曾误伤合法 cut 后投递，保留失败日志，最终将新请求的代数检查放在接受点。两个旧 B4 fixture 的新请求改为填写真实当前代，未放宽断言。

cleanup、Close 软件退休、Close step 三类观测分开；Close step 记录未执行、在途、拒绝、applied、缺观测五态。只有实际 receipt.status=applied 才记 applied；有值但 failed/canceled 不算。诊断查询使用独立 raw job ID，不占生产 in-flight slot，不使诊断回执误作下一个生产 typed reply。独立查询失败时仍保留可靠的实际 Close 回执。

真实 host/worker 覆盖 C0–C10：完整/错误绑定、错 owner/decision/sequence/时钟/代数、旧 fence 与新 Close、迟到旧退休/确认、无 accepted binding、缺 cut/音频隔离、幂等重传、failed Close 有/无旧 fence。C7b 另保留直接入口验证，未替代 worker 证据。首错、generation、引用和 fence 副作用均有断言。

## 3. 四类续行与生命周期

`ExecutionHost` 保存单调 Close phase、原 job/step、prepared control、decision、owning evidence、completed proof 与退出责任。阶段只选择下一动作，授权继续由原 coordinator/owner ledger 决定，不引入第二套协议。

| 中断 | 实现与验证 |
| --- | --- |
| 旧 job 在飞 | 旧控制、archive、inspect 原 typed reply 消费/ACK 一次；archive/inspect 成功与 owner-side allocation 失败均覆盖。迟到错误保留为 Close 首因，不伪造 control cleanup，新 Close 独立接受 |
| 2a–2d | 分别停在准备未 post、post 未取 decision、typed reply 已就绪未 ACK、owner step 未完成；复用原 job/request/decision/step，真实 dispatch/ACK 和副作用计数证明不重发、不重新 accept |
| finish 后未确认 | 保留 completed proof，原 confirm 一次，之后才 shutdown |
| shutdown 后未 exit/join | 不发新业务，继续必要泵送，exit/join 全部完成才设 Closed |

最先 Close 在 cleanup 前建立显式 started/phase 和唯一 deadline，时间戳 0 合法。每段等待取总剩余预算与原 transport deadline 的较小值，不重置预算；已有 transport deadline 按剩余间隔一次映射到注入时钟。

可控时钟覆盖从 0 起、恰好 1,000,000 us 完成 join、1,000,001 us 超限、原 100 ms transport deadline、各阶段累加和超时后继续泵送。超时保持 Closing、原身份和依赖；迟到完成可安全 Closed，但第一次超预算/首错不会被覆盖。死锁保护的墙钟限时不作 1000 ms 接受证据。

非安装 `ExecutionHostLifetime` 已用于实际 player 调用方；异常展开时，在 callback captures、renderer 及相关依赖作用域内持续 drain，再销毁 host。用例在 worker 阻塞、总预算到期和异常退出下验证捕获对象一直存活到 exit/join；不使用 detach、全局 quarantine 或只保留裸 host 指针。

## 4. 首错修复前候选的 Debug / Release 矩阵与证据限定

MSVC x64、CMake 4.3.3、candidate ON、warnings-as-errors OFF，复用只读 vcpkg installed dependencies。最终五构建矩阵和源码/缓存/库/二进制身份在 `debug-followup/final-evidence.json`，原 Release 验证日志保持原样。Debug 和 Release 的四种 STATIC 配置同时构建实际 player；SHARED OFF 为 headless。

| 构建 | Host | 协议 B4 | Oracle | 架构 |
| --- | --- | --- | --- | --- |
| Debug STATIC observation ON | 67 cases / 38720 assertions | 62 / 121976 | 52 / 20801 | 1/1 |
| Debug STATIC observation OFF | 60 / 37836 | 62 / 121976 | 52 / 20801 | 1/1 |
| Release STATIC observation ON | 67 / 41911 | 62 / 121976 | 52 / 20801 | 1/1 |
| Release STATIC observation OFF | 60 / 41158 | 62 / 121976 | 52 / 20801 | 1/1 |
| Release SHARED headless observation OFF | 60 / 41086 | 62 / 121976 | 52 / 20801 | 1/1 |

均通过，最终日志为 `debug-followup/final-{debug-on,debug-off,release-on,release-off,shared-off}-{host,protocol,oracle,architecture}.log`。Host assertion 数受泵送轮数影响，不是预算指标。公开操作 transcript 五者一致：`b4608fbfd7a89706d5c982fa8e57132e4dbb12abde7fa019cc762bbf6c1a7bf2`。

STATIC OFF 与 SHARED OFF 各自 clean add_subdirectory / staged find_package consumers 均 2/2 通过，见 `logs/final-static-consumers.log`、`logs/final-shared-consumers.log`。后续 Debug 修复只触碰应用内部和测试帮助函数，SDK 源码和 68 份已记录库文件哈希未变，原消费者结果仍适用，未声称重新运行。121 个公开头 ASCII 检查通过。STATIC OFF 的 execution_host_support、playback-candidate、judgement 库中 `@rpa@execution_diagnostics@` 符号均为 0，已在最终构建复核。

13 条 R1 路径 clang-format dry-run 全通过；两树 diff --check 通过。最终重跑全仓 `cuexis_format_check` 仍**失败**，仅五条既有 PD 文件：allocation_observer.hpp、execution_kernel.cpp、recovery.cpp、pd5_capacity_probe.hpp、pd5_memory_observer.cpp。其完整路径及与 R1 起点相同的哈希核查在 `debug-followup/final-evidence.json`；日志 `debug-followup/final-whole-format.log`。它们不属本批差异，未通过格式化改变冻结 PD 字节；此全仓限定交退出评审处置，不称全仓格式通过。Debug 故障已按 §9 解决，历史失败日志保留。

回归期间 access violation 已查明是 Ninja 头依赖前缀乱码，旧对象未随 ExecutionHostPorts 布局改变重编；完整 clean build 消除崩溃。仅本地 `msvc-dependency-prefix.cmake` 的 CMAKE_PROJECT_INCLUDE 设置实际中文 showIncludes prefix，重新 clean 建立依赖，未修改仓库 CMake。保留旧失败与重建日志。

## 5. 导出容量事实与依赖判定

分类：**noIndependentContract**，不是 verificationPassed，不算容量通过。核查 `capture_route.cpp::exportCloseEvidence` 及主树 api、formats、ADR、active plans 和归档 RPA 原合同：导出复制 pending、tentative、fault、trace、retained，没有独立显式导出上限合同。captureCapacity=4/reserve=1 的队列反例仍为原证据，不重复包装，不杜撰上限或越界写。

当前续行在导出失败时保留源 capture owner、原阶段、依赖和首因，成功取得后保留 owning evidence；不依赖“导出必在某独立容量内成功”的保证。allocation/flush 注入只证明可控失败保留与恢复，不证明实际最大复制量、永久分配失败能完成或设备根因。若 owner 要求上述容量保证作为 R1 退出条件，此项继续阻塞，需独立合同裁定，不借功能 green 关闭 R0 容量残留。

## 6. 首错修复前候选的独立补丁与回滚

最终 `rollback/r1-debug-dispatch-complete-code/r1-debug-dispatch-complete-code.patch` 相对 R1 原始起点，未混入 PD/R0 整树差异：13 文件、192225 bytes、3631 行、2207 additions / 374 deletions；SHA-256 `af78cbe86cdb512d112c571a826d1708e38eb023d271338044cb9903a3bf5202`。Debug 后续修复自身的三文件差异在 `rollback/r1-debug-dispatch-only/`，也单独正反向验证。

一次性副本验证：起始 hash → 正向 check/apply → 结束 hash → 反向 check/apply → 起始 hash 全部恢复，见同目录 `manifest.json` / `verification.log`。未在活动树 reset/clean。主树六条文档以用户**归档后** resume 快照为基线单列最终 `rollback/r1-debug-complete-documents/`，新报告反向后不存在。原 `r1-complete-code`、`r1-complete-documents`、`r1-pilot-complete-documents` 均保留，作为修复前候选 checkpoint，不冒充当前补丁；R0 patch/hash 未变。

## 7. G0–G11 对照与剩余退出门

| 门 | 当前结果 |
| --- | --- |
| G0 | 两树身份、dirty/clean/absent、追加路径起始字节及保护引用已核查 |
| G1 | 三类观测、五态、首错、OFF 无实现已验证 |
| G2/G3 | C0–C10 与各入口拒绝、合法重传/增代，最终五组适用回归通过 |
| G4/G5 | 旧 control/archive/inspect，以及 2a–2d 分别通过 |
| G6/G7 | 原 confirm、shutdown、exit/join 顺序和 Closed 门通过 |
| G8 | 独立时钟授权、固定预算、超时续行及实际调用方闭包保活通过 |
| G9 | noIndependentContract；功能续行不依赖独立导出容量保证，未接受容量边界 |
| G10 | 五构建公开 transcript 相同，OFF 生产修复与无观测符号已验证 |
| G11 | 13 路径代码正反向恢复通过；六路径文档单独回滚清单与日志 |

以下为 §11 最终退出核验之前的历史评审状态，保留原文；当前结论以 §11 为准。当前提交实现及 §10 当前候选的 Debug/Release 适用本地验证供退出评审；§8/§9 有限 pilot 保留原构建身份，适用性需在退出评审明确。Debug 技术问题已解决；全仓格式与 noIndependentContract 容量限定需在接受结论中保留，不能写“全仓检查全部通过”。owner 尚未接受 R1 退出；R0 保持带残余关闭，RPA B4/B5、生产/设备接受和 Stage 7 均不随之退出。

## 8. 首次获批有限 pilot（修复前候选，历史保留）

具体 `pilot-plan.json` 与 `run-approved-pilot.py` 在请求运行许可前已准备、核对固定构建和源码哈希。owner 于 2026-10-25 明确回复“批准执行这 3 项有限 pilot”，授权原文和计划 hash 保存在 `pilot-authorization.json`。随后执行一次，使用 STATIC OFF 原生 steady_clock、普通 allocator、headless TestRenderer/FakeAudio，无观测插桩，不产生真实设备接受结论。原计划文件保持运行前字节，当前结果以 `pilot-results/result.json` 与 `pilot-verification.json` 为准。

固定二进制 `out/build/rpa-r1-release-off-20261025/bin/cuexis_execution_host_tests.exe`，SHA-256 `899cb6baa40ecfaaaaaa9358ef593a592a33d70ddb8d6782a42d965429cd6bb5`，绑定上述补丁与 13 条源码 hash。每项独立进程，最多 3 个，每进程 20 s 保护限时；Close 合同仍为 1000 ms，20 s 只终止失控测试进程。

1. `B5 host rejects unaccepted budgets and closes CPU preparation failure`。
2. `R1 C6 failed accepted Load cleans its own identity before independent Close`。
3. `B5 headless EOF tail preserves owners through Pause Resume and terminal execution`：单 requirement、Pause/Resume、terminal、Replay 同结果及 Close/join。

每个实际 case 须 exit=0 且报告通过；任一超时/断言/非零返回即停止、保留原失败并记 invalid-process，不自动替换样本。整窗最多一次重跑，须先审查原因，不重问已选处置口径。输出独立 `pilot-results/`，不得覆盖。

2026-10-25 09:03（Asia/Shanghai）开始，严格按上述顺序执行 3 个独立进程；首次全部通过，返回码全为 0，超时 0、重跑 0。执行前后固定二进制和 13 条源码哈希均未变，本次未修改产品代码。

| 样本 | 结果 | 原始日志 |
| --- | --- | --- |
| 准备失败后 Close | 1 case / 6 assertions，通过 | `pilot-results/01.log` |
| C6 旧 Load 清理后独立 Close | 1 case / 12 assertions，通过 | `pilot-results/02.log` |
| 单实体 EOF / Replay / Close | 1 case / 2339 assertions，通过；Replay evidenceValid/sameResult，Close + join 628 us | `pilot-results/03.log` |

628 us 是该 headless 样本原生时钟的单次 Close 记录，小于固定 1,000,000 us，不推断概率或性能分布；边界正确性仍由 §3 的可控时钟反例证明。第三项 assertion 数随泵送轮数变化，不作性能指标。

该次 pilot 绑定 Debug 修复前候选，未重标为最终构建的结果。原二进制另保存在 `debug-followup/start/approved-pilot-cuexis_execution_host_tests.exe`，原源码和报告在同级 start 快照，原计划、授权、日志和结果不覆盖。所有 pilot 仅为有限 headless 功能样本，不进入正式九对、不证明失败概率、设备或生产参数接受；未运行 40k、ETW、Q4 或正式窗口。

## 9. Debug 栈溢出的继续处理与该候选 pilot（首错修复前）

owner 提醒“那你为什么停下来不继续了呢？”后，继续完成原授权内的 Debug 技术工作，未把可处理问题留作 owner 裁定。`debug-followup/start/` 在修改前保存 24 条源码、报告、缓存和二进制身份；后续只改 app/player 的 execution_host.cpp/.hpp 和 rpa_b5_session_tests.cpp，未扩大原 R1 的 13 路径集合。

当前源码干净 Debug 构建先复现原 observation 失败，日志 `current-reproduction.log`；LLDB 捕获第一现场 `lldb-current-stack-crash.log`：主线程在测试 load() 入口 __chkstk 溢出。sizeof 实测 ControlReceipt=36888、SessionCommand=38680、SessionReply=3624；Debug 不内联时，多次临时 proof/variant 在整个调用方栈帧保留空间。反汇编同时发现 close() 与 cleanup() 叠加也超过默认栈，不能仅归为旧测试噪声。

修复将 private session/closeSession 的 variant 构造移到短生命周期模板发送函数，实际 post/await、原 ID/回执、预算和 owner 规则不变，未新增生产堆分配或缓存。测试 load() 改用已有短生命周期 proof 发送帮助函数，保留所有原行为断言，并检查查询 Result。没有增加栈、timeout、容量或公开接口，也未删除失败用例。

| 栈帧 | 修复前 bytes | 修复后 bytes |
| --- | --- | --- |
| 测试 load() | 576440 | 86984 |
| ExecutionHost::close() | 854856 | 313176 |
| cleanup() | 336120 | 181352 |

PE 头确认最终五构建默认栈 reserve 1048576 bytes；Debug 原失败用例先转绿，再完成 §4 全套复验。sizeof、反汇编、栈 reserve 证据和全部 red/green 日志均在 `debug-followup/`；诊断工具的临时运行库只位于 ignored out 下，未改项目依赖。

最终构建身份改变后，沿用 owner 已明确批准的**同三项、同限时、失败即停**范围验证，不声称取得新的 owner 批准或退出接受。授权沿用依据、原授权哈希和新计划哈希在 `debug-followup/pilot-authorization-continuation.json`；新 `pilot-plan.json` 在执行前固定新构建和源码身份，原通过组保留。无自动重跑或扩展故障/规模矩阵。

最终 STATIC Release OFF 二进制 SHA-256 `86fe39a45241360f4bab24f20e513b5a9bacfa13ca7dd843f0cd14cd899b82d7`；完整代码补丁 SHA-256 如 §6。2026-10-25 09:47（Asia/Shanghai）开始，在独立 `debug-followup/pilot-results/` 执行，每进程保护限时仍为 20 s，Close 总预算仍为 1000 ms。

| 样本（与 §8 同名） | 最终构建结果 |
| --- | --- |
| 准备失败后 Close | 1 case / 6 assertions，通过 |
| C6 旧 Load 清理后独立 Close | 1 case / 12 assertions，通过 |
| 单实体 EOF / Replay / Close | 1 case / 2287 assertions，通过；Close + join 649 us |

三进程均首次 exit=0、超时 0、重跑 0；执行前后源码/二进制身份不变，结果和日志哈希在 `debug-followup/pilot-verification.json`。649 us 仅为该样本单次功能记录，不作概率、设备或生产性能结论。技术实施不再因已解决的 Debug 问题停留；剩下的是 owner 对本报告限定范围的退出接受，不含未经授权的提交、推送或合并。

## 10. 复核首错缺口的最小修复与当前候选

owner 要求使用无上下文的 `gpt-6.1-sol` 子代理完成修复；子代理仅改 `app/player/src/execution_host.cpp` 和 `tests/rpa/rpa_b5_host_tests.cpp`，主代理独立审阅差异并复验。代码仍在准入树，未提交或合入主树。

原 `close()` 的 flush catch 只写 `closeDeliveryError_`，Close 可继续成功却没有生产 `closeFirstError_`；后续阶段再失败时，后错会被登记成首错。修复增加两行：仅当首错槽为空时，保存实际 delivery 失败。返回值、suspend/退休/续行、ID、1000 ms 总预算、公开 API 均沿用原行为，不扩展同步阶段预算或析构策略。

新增 `[first-error]` 用例通过真实 `SessionWorker::beforeDispatch` 在 `SessionCaptureDelivery` 抛出 allocation 失败，得到 `host.rpa.capacity_exceeded`，同时适用于 observation ON/OFF；不是设备根因或容量上限证明。三种顺序分别验证：仅 delivery 失败后 Close 成功仍保留首错；delivery 后 export 失败并续行时后错不覆盖；更早 cut 失败后发生 delivery 失败时保持原首错。还断言 Closed、owners exited、controlSequence 只增加一次、acceptance 与 finish 各一次。

本节证据根为准入树 `out/rpa-r1-first-error-fix-20261025/`。主代理在 `isolated-red/` 用精确保存的旧源码另编译、另链接，未替换活动源码/对象；同一新增回归在 Debug ON/OFF 均 exit 42，90 assertions 中 87 passed / 3 failed，失败正是首错为空或被 export 后错占用。当前修复后五组各 91 assertions / 1 case 全通过，完整独立复验如下：

| 当前构建 | Host cases / assertions | 协议 B4 | Oracle | 架构 |
| --- | --- | --- | --- | --- |
| Debug STATIC observation ON | 68 / 38583 | 62 / 121976 | 52 / 20801 | 1/1 |
| Debug STATIC observation OFF | 61 / 37806 | 62 / 121976 | 52 / 20801 | 1/1 |
| Release STATIC observation ON | 68 / 41601 | 62 / 121976 | 52 / 20801 | 1/1 |
| Release STATIC observation OFF | 61 / 40675 | 62 / 121976 | 52 / 20801 | 1/1 |
| Release SHARED headless observation OFF | 61 / 41288 | 62 / 121976 | 52 / 20801 | 1/1 |

日志与退出码在 `parent-verification-final/results.json`，构建/源码/库/二进制哈希在 `final-evidence.json`。四组 STATIC 的实际 player 同步重建；每条命令前及整组结束核查 13 条源码哈希未变。五组公开 transcript 仍为 §4 的相同 digest；13 路径格式、两树 diff --check、121 个公开头 ASCII 通过。Debug/Release STATIC OFF 的三类库观测 namespace 符号均为 0。SDK 源码与 68 份库哈希未变，原 STATIC/SHARED 各 2/2 clean consumers 仍适用，未重跑。全仓格式未因两文件修复重复运行，§4 五条既有 PD 失败路径哈希仍未变，该限定继续保留。

补 red 期间，子代理曾临时恢复活动源码，引发并行父验证构建混用旧源码；原 `parent-verification/` 和两份不含 `final` 的 first-error 补丁仅保留审计，不纳入接受证据。`excluded-evidence.json` 与相应 `EXCLUDED.json` 明确废弃范围。恢复修复后重新锁定源码，完成上表全套复验；隔离 red 不修改活动构建输入，未把失败日志删除或当成通过。

当前完整 R1 补丁为 `out/rpa-r1-20261025/rollback/r1-first-error-final-complete-code/r1-first-error-final-complete-code.patch`：13 文件、2296 insertions / 374 deletions、197633 bytes，SHA-256 `76f8b57daa2bae958c82cf8590795ce9ba0c1b9740a63652970df0ca2278012b`。本次独立增量为同级 `r1-first-error-final-only-code/`：2 文件、89 insertions、5783 bytes，SHA-256 `4dc956cdfcc685a6f28584018d4db42f99d4baf298cee172ec1a0008262e8946`。两者均经起始哈希、正向 check/apply、结束哈希、反向 check/apply、恢复起始哈希验证；旧候选补丁及 R0 patch 原样保留。本节五条文档增量另存 `rollback/r1-first-error-only-documents/`，只验证这五条路径，不扩大为整树回滚。

§8/§9 两组有限 pilot 保持各自原身份；§9 的 649 us 不标为本次修复构建的结果。修复前 Release OFF 测试二进制已保存到 `start/builds/rpa-r1-release-off-20261025/bin/cuexis_execution_host_tests.exe`，SHA-256 仍为 `86fe39a45241360f4bab24f20e513b5a9bacfa13ca7dd843f0cd14cd899b82d7`。本次完整 Host 回归覆盖其中三个同名 case，但没有新登记 pilot；后续退出评审须明确原 pilot 对当前两行修复的适用性，若要求当前身份 pilot，按既有获批同范围另登记，不能重标旧结果。本轮未运行 40k、ETW、设备或正式窗口。

本次首错缺口与适用本地验证已闭合；owner 退出接受、全仓格式限定、noIndependentContract 导出事实及 R0 历史残余仍按原边界处理，RPA/PD/生产接受和 Stage 7 不随本次修复关闭。

## 11. 当前修复构建有限 pilot 与 R1 退出

用户本次明确要求“请你使用子代理完成收尾并退出R1”。本节按该授权完成核验后裁定 R1 退出，不声称用户在运行后另作接受。沿用 §8 已获批的同三项有限 headless 功能范围，首错修复后另登记身份，旧计划、授权、结果及 628/649 us 历史记录原样保留。本次证据根为准入树 `out/rpa-r1-final-pilot-exit-20261025/`。

运行前固定 `pilot-plan.json`（SHA-256 `ee16a431a43eeaa31bd6444defcce6f46658c22e024ab972cd6eb8ad5b853ae0`）、授权沿用依据和全部 13 条源码身份。最终 STATIC Release observation OFF 测试二进制 SHA-256 `9b65653f7987e9fb9b124a2e08b67255538b090a454db612c4efefc2bf3802ef`，匹配 §10 `final-evidence.json`。源码与完整补丁沿用 §10，执行前后哈希相同。每项独立进程，20 s 保护限时、最多三进程、Close 总预算仍为 1000 ms；任一失败/超时立即停止，不自动重跑或替换样本。

2026-10-25 19:48:40（Asia/Shanghai）开始，三进程均首次 exit=0、超时 0、重跑 0；完整结果在 `pilot-results/result.json`，原始日志为 `pilot-results/01.log`、`02.log`、`03.log`。

| 同范围样本 | 当前构建结果 |
| --- | --- |
| 准备失败后 Close | 1 case / 6 assertions，通过 |
| C6 旧 Load 清理后独立 Close | 1 case / 12 assertions，通过 |
| 单实体 EOF / Replay / Close | 1 case / 2067 assertions，通过；native Close + join 659 us |

659 us 仅为该有限 headless 样本的单次功能记录，低于固定 1,000,000 us，不作设备、概率或性能分布结论；总预算边界仍由原可控时钟反例证明。第三项保留 Replay evidenceValid/sameResult 与 owners exit/join 断言。准备脚本曾因引号语法失败，未启动测试进程或登记样本；修正工具写法后仅执行上述三进程，不构成样本重跑。

完整退出门核对：G0–G11 与四类续行沿用 §2–§7、当前首错 red/green 和五构建回归采用 §10；架构/allowlist、121 公开头 ASCII、OFF 符号和相同 transcript 均有对应证据。SDK 源码及 68 份库哈希未变，STATIC/SHARED 各两项 clean consumers 继续适用，未重跑。本节新登记 pilot 补齐当前构建身份；报告与索引同步维护。全仓五条既有 PD 格式失败作为范围外限定保留，本批 13 路径格式通过，不写全仓格式通过。导出为 noIndependentContract，不算容量通过；当前失败留证/续行不依赖独立容量成功保证，因此本次 R1 退出不附加或宣称该保证。

主代理已独立核对本次计划、二进制、13 条源码及三份原始日志，证据为 `parent-independent-pilot-verification.json`。运行前冻结计划中的 `previousPilot` 字段沿用旧描述，不作为本次相对路径索引；两组历史证据的准确位置另记于 `historical-pilot-index.json`。

本次新增六文档增量的 before/after 字节、独立补丁及正向/反向恢复验证保存在证据根 `documents/`，只覆盖这六条路径；不冒称整树回滚，既有脏改动及 R0/PD 冻结材料保持原样。验证结果与最终身份在 `exit-verification.json`。R1 在此有限实现及验证范围内退出；R0 历史 partial 不回填，Seek/ON-OFF/Q4、导出容量、R2/R3/R4 及设备/生产接受仍归原批次，RPA、PD 与整个 Stage 7 均未因此关闭。未执行 40k、ETW、设备或正式窗口，未提交、推送、合并。
