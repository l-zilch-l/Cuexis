# Stage 7-RPA R1 实施与验证计划

状态：R1 已退出（2026-10-25）；依据用户本次授权完成最终修复构建三项同范围有限 pilot 与退出门核验，结论见[本地记录 §11](../../../stage_reports/stages/stage-07/rpa/2026-10-25-r1-continuation.md#11-当前修复构建有限-pilot-与-r1-退出)。五条既有 PD 格式失败、noIndependentContract 及 R0 残余保持限定；RPA/PD/Stage 7 未关闭，R2 已由用户另行授权在下一对话规划并实施，本轮仅整理文档、未启动 R2；R3/R4、40k、ETW、正式测量与设备接受未因此获授权。下文保留原 R1 实施计划与门禁的历史语境。

更新日期：2026-10-25。

## 1. 来源、范围与执行顺序

本计划把桌面 `Cuexis-Stage7-RPA-R1-实施接手文档-2026-10-24.md` 的执行清单纳入仓库。
该附件是输入材料，不自行授予执行权限；本轮授权来自 owner 的“阅读接手文档，你的任务是接手完成R1”，顺序约束来自“先核对接手文档和工作区的计划文档，补全工作区文档后，再开始实施”。

按以下顺序阅读：

1. [CURRENT_STATUS](../../../CURRENT_STATUS.md)：唯一当前状态摘要。
2. [R0 报告 §8.1](../../../stage_reports/stages/stage-07/rpa/2026-10-24-r0-observation-and-anchor.md#81-owner-裁定与残留归属)：关闭状态、残留归属和历史限制。
3. [整改总计划 §4.1 / §6](plan-rpa-performance-remediation.md)：R1 的范围、许可与续行设计。
4. 本计划：执行顺序、可控反例、停审点和交付核对。
5. [RPA 历史范围与验收](../../../archive/stage-07-planning/2026-10-11-rpa-plan.md)、[RPA 语义](../../../formats/rpa-execution-contract.md)、[宿主边界](../../../api/realtime-host-boundary.md)：保留的原合同与验收门；归档不重新授予历史指令效力。

执行顺序固定为：**文档核对与补齐 → 文档检查 → 保全起点 → 只补观测 → 旧行为反例 → 停审裁定（若触发）→ 最小修复 → 四类续行 → 生命周期/预算 → OFF 等价与适用回归 → 独立回滚验证 → 退出评审**。

R0 保持带残余关闭，不重开、不回填。R1 只直接承接旧 fence/cleanup permit 与 Close 观测；导出容量按依赖判定。R2/R3/R4、Q4、40k、ETW、正式窗口、真实设备根因不在本批。R1 保持一个批次，不把未完成工作改名为新子批以提前退出。

## 2. 起点、工作树与保全

2026-10-25 已只读复核下列身份，与桌面交接一致；实施前若改变，必须重新解释并锚定，不覆盖原快照。

| 对象 | 起点 |
| --- | --- |
| 文档权威工作树 | `D:\Cuexis-worktree`，`stage-7`，HEAD `0157486800f8f40a4121ef0d5ab26bd041a08304`；四条既有文档 dirty 保留 |
| 代码实施工作树 | `D:\Cuexis-pd5-40k-admission`，`codex/pd5-gameplay-40k-admission`，HEAD `3e5837f63f76397429c9f7603fd7450d3ce9855d`；64 条 = 22 modified + 42 untracked |
| 主树 CURRENT_STATUS 起始 SHA-256 | `0A9A3DA36394525BCEE7947DBF60AEABE7227E0853C65DC91737F423DF7AC387` |
| 主树 R0 报告起始 SHA-256 | `C0344E11AA8A8391F146AC522488B2BE694E65CC0650D8141D664100EFFB4B50` |
| 主树整改总计划起始 SHA-256 | `3BA9CB10FD5768735D6777F9C683BADDEC459B15B69B1820C3A0F75801D0EA90` |
| R0 patch（只读） | 准入树 `out/rpa-r0-20261024/rollback/r0-observation.patch`；SHA-256 `D97A6DB44775028633BF1EF31BE604676F75BD6DEB6D60DD45DFF8D77F1C114B` |
| 本次独立证据根 | 准入树 `out/rpa-r1-20261025/`；不覆盖已有 `out/rpa-r1-20261024/` 日志 |

`start/` 保存授权原文、时间、两树身份、完整 porcelain 清单、64 条 dirty 起始字节/hash、既有 binary diff、预计触碰的干净文件字节、候选新增路径的 absent 记录、构建缓存/二进制身份及 R0/Q4 只读哈希清单。已保存 64 条 dirty；干净文件和新增路径按 manifest 核对。新增触碰路径须先保存起始字节，不能事后补造。

主树本轮文档改动前另存 `start/main-docs/`；既有四条文档改动不提交。代码仅在准入树编辑，不把 R0/R1 merge/rebase 到主树，不提交、推送、合并。准入树中的历史文档不替代主树当前计划；实施直接读取本计划。

## 3. G1：先拆观测，不改生产行为

分别保存 cleanup、Close 软件退休、`SessionSoftwareClose` 三类记录；worker 回复返回明确标签或三个独立记录，host 分别捕获，禁止“最后一条”覆盖。cleanup 与 Close 软件退休的 `path` 只说明该退休路由；`permitDirected` 只是谓词，不能证明 Close 步骤或许可通过。

Close 步骤单独采用五态：`notExecuted`、`executing/resultPending`、`rejected`、`applied`、`observationMissing`。进入步骤前记录未执行，发送时记录在途；步骤入口设置 `closeAttempted=true`、`closeApplied=false`，只在 receipt.status 为 applied（含合法幂等 applied）时置 true；`Result` 有值、`StepStatus::failed` 都不是 applied。拒绝保留身份、首错及 attempted=true/applied=false；只有查询也未取得记录才记 observationMissing。

在 Close 步骤成功、拒绝或异常结束后单独捕获，不能用先前的软件退休记录代替；在途仍保留在途状态。读观测不参与生产决策。OFF 不编译观测实现；预算、续行和 failed Close 拒绝属于生产行为，不能藏在观测宏内。

## 4. C0–C10 反例与先后门

所有新反例先在 ON 归因构建运行，保留旧行为失败，再决定最小修复；ON 耗时不作预算接受证据。真实 worker/host 路径不可用固定槽或底层单元调用替代。

| ID | 可控场景与行为断言 |
| --- | --- |
| C0 | cleanup 后正常 Close；三类记录独立且 Close 步骤有 attempted/applied，拒绝与缺证据不同 |
| C1 | 已接受非 Close suspend/failed 后的真实 cleanup permit；原 ID 退休、释放引用；Close 独立接受 |
| C2 | 分别错 owner、participant、scope、operation、target、decisionOrdinal、controlSequence、clockSegment、runEpoch、resourceGeneration、rendererGeneration、contentIdentity 中进入判断的字段；拒绝、首错保留，不 unload、不释放引用 |
| C3 | 旧正常 fence + 更大 sequence 新 Close；旧 job 按旧 ledger 清理且保留旧 fence，新 Close 用自己的完整绑定 retire/close/finish，最终 confirm 才解除旧 fence |
| C4 | 同 sequence 且同一 Close 完整绑定；只允许完整同绑定 leave/confirm，不建第二 fence，不重复 prepare/cut/pauseEpoch/evidence |
| C5 | 新 Close 接受后注入旧迟到回执，不释放新引用；重复 retire 幂等且 generation 不二次增加 |
| C6 | 第一优先：旧非 Close 在进入 fence 前失败，failed() suspend，close() 先 cleanup；原 typed reply 消费一次、旧操作 cancel-retire 一次、新 Close 接受一次 |
| C7 | failed Close 无旧 fence，其他 binding/cut/audio 有效；retire 与 Close 步骤均显式拒绝，无引用释放、无新 applied、generation 不变 |
| C7a | failed Close 有旧 fence，其他条件有效；同 C7，且旧 fence 身份不清空、不替换、不成为清理许可 |
| C7b | 同输入分别经 worker、直接 retireSoftware、直接 retireExecutionStep；记录许可、首错、副作用差异，只记差异，不据此统一/下沉检查，也不替代 C7/C7a |
| C8 | suspended/failed 但无已接受 binding/decision；不得执行或 unload，状态本身不是许可 |
| C9 | 正常 preparing fence 缺 capture cut → host.rpa.control_pending；directed retire 缺 cut 或音频未 stop/isolate → host.rpa.device_pending；均不得退休/释放引用/新 applied，不强行统一错误码 |
| C10 | 正常 retire 只增代一次；原 ID/载荷重传返回原 ledger 结果且幂等；旧代新请求或不同 binding 被拒，不误伤合法重传 |

C0–C5 必须有真实 host/worker 路径；C3/C4 分开，C5 的迟到注入发生在新 Close 接受之后。C7/C7a 除 retire 外还要在软件已退休、其他前置满足时验证 `SessionSoftwareClose` 对 failed proof 的拒绝，并保留 rejected 观测。各场景共同核对首因、原 ID、无第二次 prepare/accept、无重复副作用、未提前 Closed。

**先执行 C6；若失败，停审旧已接受操作 cancel-retire/unload 的内部有限许可。C7/C7a 在旧行为下任一未能证明拒绝，同样先留失败证据并停审。** 具体最小差异、影响入口、保留合同和验证计划完成后才请求相应授权；候选修复表不是授权。不得靠 failed 一律可清理、新 fence、删除 preparing 条件或扩大 status 许可越过门禁。

## 5. 最小修复与四类续行

仅修已复现的问题，一条修复对应一条失败；新增进度仅 owner-local、单调、可查询，复用原 coordinator/owner ledger，不引入第二套控制协议。许可来源仍是 D2 accepted request/decision 与 owner ledger，proof.status 不自授权。

| 类别 | 必须独立实现与留证 | 禁止替代 |
| --- | --- | --- |
| 1：Close 接受前旧 job 在飞 | 非 Close 控制按旧 operation/decision/target 消费原 typed reply 并清理一次；至少一个 archive、一个 inspect 变体消费一次，无 control 身份不伪造 cleanup；之后 Close 独立接受一次，使用自己的 ID | 旧 Tail 测试不能替代 archive/inspect；不得丢回复或复用旧 ID |
| 2a：request 已准备未 post | 保存原 prepare/request，仅 post 一次 | 不得第二次 begin/prepare |
| 2b：已 post 未取得 decision | 查询并消费原 decision，保留原 job | 不得重发或另造 decision |
| 2c：已接受 decision、原 typed reply 未消费 | 原回复恰好消费一次，沿用 operation/decisionOrdinal/step | 不合成 decision，不重新 accept |
| 2d：已接受、owner step 未完成 | 按原 step/ledger 继续 cut/delivery/isolation/retire/close，各副作用一次 | 不以 2d 代替 2a–2c，不 generic completeCleanup(Close) |
| 3：finish 后 confirm 未确认 | 保存 completed proof/finalized context；未 post 才发一次，已发就等待原回复；确认后 shutdown，再 exit/join | 协议 final 不是线程退出，不重复 archive/inspect/accept/retire/confirm |
| 4：shutdown 后未 exit/join | 泵必要事件、检查 exit、joinExited；全部完成才 facade Closed | 不向退出 worker 发新业务，不重建 Close、不 detach、不提前 Closed |

证据逐类记录原 request/job/operation/step、post/accept/typed-reply 消费次数、decisionOrdinal、ledger 终态、generation、副作用计数、首错、shutdown/exit/join 顺序与最终 facade 状态。保存已取得的 owning archive/cut/evidence；迟到结果按原类型归属与 ACK，不交给 generic cleanup 丢弃。

## 6. 生命周期与唯一总预算

首个 close 请求在 cleanup 之前用显式 started/阶段位建立 **1000 ms** 总 deadline；不是 options.replyDeadline，也不以时间戳 0 作首次哨兵。每段等待受 `min(总剩余时间, 原 transport replyDeadline)` 限制，保留原 transport deadline，不逐段重新起总预算。

总预算覆盖前置 cleanup、archive/retained、renderer/audio/software retire/close、finish、confirm、shutdown、exit、join。超时保留 Closing、原阶段/ID 和全部依赖，后续 owner 调用仍可非阻塞查询、泵送和推进；最终回收不得覆盖第一次超预算事实。Closed 只在必需退出/join 完成后可见，suspended 不掩盖 Closing。

用可控时钟覆盖从 0 起、恰好 1000 ms、略超、分段等待叠加、超时续行；不能用墙钟或 ON 插桩耗时替代。如果无可注入时钟，先提交最小注入边界审查。证明原宿主 owner 连同 callback captures 闭包在超时后继续存活与泵送；不能 close 返回错误就 HostLifetime 析构 host.reset，不以只保留 unique_ptr 或全局 quarantine 代替证明。无法证明即停在生命周期裁定门。

2026-10-25 边界裁定已完成：owner 明确回复“批准该内部时钟边界，继续 R1”。仅非安装 ExecutionHostPorts 的可选主线程只读回调，默认 steady_clock，不改变预算；边界、原 transport deadline 与实际 player 生命周期验证见上述本地记录及准入树 `out/rpa-r1-20261025/clock-authorization.json`。

## 7. 导出容量：事实与依赖分开

已有 captureCapacity=4/reserve=1 的队列未满/恰好满/下一次拒绝反例不重复包装。核查 `exportCloseEvidence()` 的独立容量合同，只归入下列一种：

| 分类 | 含义 |
| --- | --- |
| noIndependentContract | 核查来源后确无独立导出合同；不是容量通过 |
| contractUnverified | 有合同尚未验证 |
| verificationFailed | 已验证失败，保留失败，不能改写为无合同 |
| verificationPassed | 按合同合法填满后导出，复制量/拒绝码/首因/状态保留符合；只有此项记容量通过 |

不制造上限，不越界写，不把注入 allocation/flush 失败当容量边界或现场根因。若 R1 失败留证/续行依赖它，而结果不是 verificationPassed，则停审；无该依赖时记录事实后继续，退出评审由 owner 裁定。

## 8. G0–G11 本地门与完整退出

| 门 | 必备证据/条件 |
| --- | --- |
| G0 | §2 全部起始字节、dirty/absent/build 身份、R0 只读 hash；缺失不实施 |
| G1 | §3 三类独立观测、Close 后捕获、五态、rejected 首错、OFF 无观测实现 |
| G2 | C1–C5、C10 真实 owner 路径，合法重传与增代分开证明 |
| G3 | C6/C7/C7a/C7b/C8/C9；C6 或 failed Close 拒绝失败先停审 |
| G4 | 第 1 类控制/archive/inspect，各原 typed reply 一次，Close 独立接受 |
| G5 | 第 2 类 2a–2d 四个停顿点分别续行，无重复副作用 |
| G6 | finish 后原 confirm 一次，随后 shutdown/exit/join |
| G7 | shutdown 后泵送、exit、join、Closed 顺序正确 |
| G8 | §6 可控时钟、1000 ms 边界、超时 Closing/续行与 callback 闭包保活 |
| G9 | §7 四类事实与依赖判定；只有 verificationPassed 算容量通过 |
| G10 | OFF 同样具有生产修复；ON/OFF 返回、首错、消息、预算、许可等价，仅观测字段/开销不同 |
| G11 | R1-only patch 与一次性副本的起始 hash → 正向 check/apply → 结束 hash → 反向 check/apply → 起始 hash 恢复；新增文件反向后不存在 |

R1-only patch 以本轮 start 字节为基线，不能用混有 R0/PD-5 的整树 diff 代替；主树文档差异另列。保存 bytes/行数/文件数/增删/SHA-256 与正反向日志，不在活动树 reset/clean，不覆盖 R0 patch。

G0–G11 完成时只能写“**实现及本地验证完成，提交 R1 退出评审**”，不等于完整退出。完整退出还需：

1. 适用新增反例、RPA B4/B5 回归、文档检查和约定构建测试；历史 542 assertions / 4 cases 不替代新证据。
2. 架构、target allowlist、公开头 ASCII、STATIC/SHARED clean consumer 与 external consumer/find_package 适用门；不扩大公开 API。
3. 具体有限 pilot 计划、构建身份、功能范围、≤1000 ms Close 边界、停止条件交 owner，**另获明确授权后**才运行；失败记 invalid-process、整窗最多重跑一次，保留原失败。pilot 不进入正式九对，不证明概率或设备接受。
4. R1 报告与证据索引，覆盖四类续行、预算、容量事实、独立 patch/回滚、未闭合项归属，交 owner 退出评审。

不提交、不推送、不合并、不跑 pilot/40k/ETW/正式窗口，除非有另外明确授权。不得声称 RPA B4/B5 全退出、RPA/Stage 7 关闭或设备根因已定位。

## 9. 核对结果与停止条件

桌面交接与总计划的许可路线、四类续行、R0 残留和禁止扩张一致。此次补入此前未集中登记的：C0–C10 逐项映射、C7/C7a 停审、Close 五态和 applied 判据、2a–2d 分别验证、显式 started/固定 1000 ms、导出四类事实、G0–G11 与完整退出的区别、独立 pilot 授权和本次起点身份。原计划历史编写/核查快照保留，不把它们当当前授权或状态。

下列情况停止依赖它的实施，保留首错、原始日志、现场 hash 与具体最小变更包：身份差异无法解释；C6/C7/C7a 失败需审查；需要改公共 API/ControlKind/协议/格式/门槛；需新 fence/延长 timeout/删除 preparing 条件；时钟注入边界或 callback 保活不能证明；导出验证非通过且 R1 依赖；续行必须第二次 begin 或重跑整个 Close；只能靠墙钟、ON 耗时或放松断言变绿。不得删除失败用例、改期望或靠重跑制造通过。
