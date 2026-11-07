# R0 observation and anchor

- 状态：completed
- 日期：2026-10-24
- 范围：Stage 7-RPA 的 R0-observation-and-anchor，覆盖 Close 首因/拒绝路径证据、Seek 相位与工作量账本、当前 F1 上的 Q4 重锚。
- 结论：**R0 的观测与重锚工作已收尾；§5.2 的验证项均有实现映射和至少一项证据，但本批按“带残余”口径关闭，不是无保留退出。owner 于 2026-10-24 接受该口径。** 本报告不启动 R1，也不构成 Stage 7-PD、Stage 7-RPA 或生产验收结论。

## 1. 结论与边界

本批按 [性能整改方案](../../../../../stage_plans/active/stage-07/05-rpa-new/plan.md) §5 执行。没有启动 40k、ETW、pilot 或任何正式性能窗口；Q4 只是一次独立诊断重锚。

已完成并复核：

- Close 记录覆盖首因阶段、固定槽位、重入、fence 路径/适用性/拒绝位、清理许可、终态与 join；flush 失败归到 `closeFlush`，重入不清空首因。
- Seek 账本覆盖 S0–S6、包络/未归因时间、等待原因、58 个工作量字段，以及 RuntimeState、ReplayRecord 尾部、配置、结果对象和 canonical archive 的复制分类。
- ON/OFF 两套 Release 构建、完整执行宿主测试、宏 OFF 符号审计和公开操作轨迹等价性均通过；OFF 中 observation 符号为 0。
- 在当前 F1 上完成了登记过的 3 轮 A/B/Q4 诊断窗口，给出点估计和配对区间。

本批按“带残余”口径关闭。可撤回性已用独立起始快照验证，但以下残余仍未关闭：

1. **回滚 patch 是事后重建的，且只覆盖 R0 触及的 12 个路径。** 它由 R0 起始快照与当前字节生成，经隔离工作树反向应用验证；不是作者当时留下的 patch，也不覆盖 PD-5 其余 10 个脏路径或整棵 54/64 项工作树。
2. **旧 fence / cleanup permit 没有活路径证据。** 无 fence 的正常 Close 路径上的软件退休有活证据，但 `SessionSoftwareClose` 步骤本身没有被当前捕获点导出；`existingFence` 和 `cleanupPermit` 只有固定槽单元测试，没有真实旧 fence 复用或失败清理许可的现场。`permitDirected=true` 只说明 retire predicate 接受，分支证据必须看 `path`；cleanup 退休记录与 Close 路径的软件退休记录目前仍共用 worker 的最后一个 fence 槽，后写者可能覆盖前者。
3. **失败注入不是现场故障。** flush 与 export 分支由 observation build 的测试钩子触发，证明记录和返回值路径可用，不证明任何真实设备/宿主故障的根因。
4. **两个计数在公开边界不可得。** `configValues` 保持 0（配置标量数未暴露）；S0 的 sealed identity 只记录“存在/未知”，没有值。公开 DTO 没有独立的字节复制计数，S5 记录的是对象/Fact 量和 owning-session 分配。
5. **Q4 不是“原样 Q4”。** A 是 `324d874` 上的重建基线，不是历史 A；Q4 是手工迁移到当前 F1 的诊断消融，不是原提交的直接复用。

这些残余不改变“观测字段已实现、OFF 构建无观测实现、公开轨迹不变”的事实。R0 退出核对的第 8 项已通过（限定 12 路径），第 4、5 项仍为部分；owner 已接受按“带残余”口径关闭，而不是无条件完成。R1 仍未启动；R0→R4 的顺序不变。

## 2. 基线与身份

| 项 | 值 |
| --- | --- |
| 主工作树 | `D:\Cuexis-worktree`，分支 `stage-7`，HEAD `0157486800f8f40a4121ef0d5ab26bd041a08304` |
| 主工作树父提交 | `f0c7e32`（仅文档），与准入树 merge-base `eed3664` |
| 准入党工作树 | `D:\Cuexis-pd5-40k-admission`，分支 `codex/pd5-gameplay-40k-admission` |
| 准入党 HEAD | `3e5837f63f76397429c9f7603fd7450d3ce9855d` |
| 准入分支关系 | 不是主分支祖先；R0 没有提交 |
| R0 证据目录 | 准入树下 `out\rpa-r0-20261024\`（新目录，未提交） |
| Q4 证据目录 | 准入树下 `out\rpa-r0-q4-20261024\`（新目录，未提交） |
| 构建类型/库型 | Release / STATIC，`CUEXIS_BUILD_TESTS=ON` |

起始工作树快照为 54 项（12 modified + 42 untracked），状态名/状态集 SHA-256 为 `392BED6607019029437FC2535424DCA364030C9E8FFE792DE60AD448D24C0553`；结束时为 64 项（22 modified + 42 untracked），SHA-256 为 `82E43D858246F0B7AD3660EF295F47587CE322D16D43192A288F16F0989C7D37`。起始 54 个路径全部保留；新增 10 个 modified 路径由本批产生：

- `CMakeLists.txt`
- `app/execution_host/capture_route.cpp`
- `app/execution_host/host_candidate.cpp`
- `app/execution_host/session_worker.hpp`
- `app/player/src/execution_host.cpp`
- `app/player/src/execution_host.hpp`
- `engine/core/internal/cuexis_internal/execution_diagnostics.hpp`
- `engine/judgement/src/execution_kernel.cpp`
- `engine/judgement/src/recovery.cpp`
- `tests/rpa/rpa_b5_host_tests.cpp`

另有 `app/execution_host/session_worker.cpp` 和 `engine/playback/src/playback_session.cpp` 在起始时已 dirty，本批继续修改。两者的 R0 前字节已从 DSH workspace 的独立变更捕获中找回，SHA-256 与 `start-identity.json` 完全一致；其余 10 个 R0 路径在起始时是 clean，起始内容就是准入党 HEAD `3e5837f`。

整个脏工作树为 **22 个文件、2,872 insertions / 139 deletions**。R0 触及的 12 个路径相对起始快照的单批差异为 **2,245 insertions / 70 deletions**；相对 HEAD 的整文件差异是 2,278/73，其中 33/3 是那两个文件在 R0 前已有的 PD-5 脏改动，不属于 R0。因此可撤回 patch 以“起始快照 → 当前”为准，而不是以 HEAD 为准。

| 文件 | 起始字节 | 起始 SHA-256 | 最终字节 | 最终 SHA-256 |
| --- | ---: | --- | ---: | --- |
| `app/execution_host/session_worker.cpp` | 62,572 | `A40ADEB31D5764041A2F39DFB8A2C2FA11EA3A730A21B2670DA2E035356CD065` | 68,673 | `DEFDB404B475E5F384A7FBBBD1C36C817BDF04F16AC9D797541F0210260A08FF` |
| `engine/playback/src/playback_session.cpp` | 238,450 | `B56950B50404FD2FE2A9EC3BD45D19E4B15728F65C07A1508EF81981A48397BA` | 250,847 | `C82B51D0DB435CA413F5A8FB6FAE0B5386621F765F3D00319A18BA5D6D0769F2` |

在隔离工作树 `D:\Cuexis-r0-rollback-20261024`（detached 于 `3e5837f`）中，先把 12 个路径恢复为起始字节，再应用 `out\rpa-r0-20261024\rollback\r0-observation.patch`（12 文件、2,245/70，SHA-256 `D97A6DB44775028633BF1EF31BE604676F75BD6DEB6D60DD45DFF8D77F1C114B`）：`git apply --check` 和 `git apply` 均返回 0，12 个 end 哈希全部等于准入树当前文件；`git apply --reverse --check` 和 `--reverse` 也返回 0，12 个 start 哈希全部恢复。完整命令、逐文件哈希和恢复结果见 `out\rpa-r0-20261024\rollback\rollback-verification.json`、`rollback-verification.log`、`rollback-verification-raw.log`、`verify-rollback.ps1` 和 `pre-r0\`。

这不是作者当时留下的 patch，也不是从整棵脏工作树 diff 中手工拆出的 hunk，而是以独立起始快照为基线重建的可审计 patch；它证明**对 R0 触及的 12 个路径可整批撤回**，不证明整棵 54/64 项工作树或 PD-5 其余 10 个脏文件可一键回滚。

## 3. 宏隔离与构建证据

`CMakeLists.txt:74` 新增 `CUEXIS_RPA_OBSERVATION_TEST_ONLY`，默认 `OFF`；`:75` 起只有同时启用 `CUEXIS_BUILD_TESTS` 才允许打开，并定义同名编译宏。release 宿主测试使用两套独立构建目录：

| 构建 | 目录 | 选项 |
| --- | --- | --- |
| ON | `out\build\rpa-r0-observation-20261024-3e5837f` | `CUEXIS_RPA_OBSERVATION_TEST_ONLY=ON` |
| OFF | `out\build\rpa-r0-observation-off-20261024-3e5837f` | `CUEXIS_RPA_OBSERVATION_TEST_ONLY=OFF` |

最终二进制身份：

| 产物 | 字节 | SHA-256 |
| --- | ---: | --- |
| ON `cuexis_execution_host_tests.exe` | 5,794,816 | `0E9B1558879A5FC324FD39365E8361A1D65748CF6319002AB248B9F07CAB28B5` |
| OFF `cuexis_execution_host_tests.exe` | 5,672,960 | `B17D5B6CA9F92672385D2D777840E243A91F9F00858C4CEFDB3707435B0E19F1` |
| ON `cuexis_execution_host_support.lib` | 21,229,978 | `ABB783F225A477223AAAB26CBEDBAD3E18FB352EDDA4D1B787378B209422B7F2` |
| OFF `cuexis_execution_host_support.lib` | 19,413,558 | `CA7176D90C70FF1460360E0951E71B4933A7DC4BE2A566A8E5199DD43455B9AF` |
| ON `cuexis_judgement.lib` | 32,371,506 | `8CB02DAC6BFBD8B60C761374A116A022AA76A6E8EF10EB8465A94A57CA39D0F0` |
| OFF `cuexis_judgement.lib` | 32,358,904 | `B490F73A8D4C3A578959E850D07C1F55CD346D65FED76C91A6BA01ED9442BB23` |
| ON `cuexis_playback-candidate.lib` | 30,113,332 | `BA35414AA57007C6FB3850276CCE94236990386436FBF8CDCE7696AAD36E7BC7` |
| OFF `cuexis_playback-candidate.lib` | 30,090,644 | `CBE0FC8339A47C96AB2AFD2B824E4ED04CB252C799FEA0AC53BBA9CD1A457F2D` |

符号审计用 `dumpbin /symbols` 精确匹配修饰名 `@rpa@execution_diagnostics@`，避免宽泛的 `rpa@` 误命中既有 `controlRpa`/`RpaControlRecord`：

| 构建 | judgement | playback | host_support | 合计 |
| --- | ---: | ---: | ---: | ---: |
| ON | 9 | 31 | 2,977 | 3,017 |
| OFF | 0 | 0 | 0 | 0 |

测试结果：

| 运行 | 用例 | 断言 | 结果 |
| --- | ---: | ---: | --- |
| ON 完整套件 | 45 | 36,591 | 全部通过 |
| OFF 完整套件 | 42 | 36,168 | 全部通过 |
| ON `[rpa][r0]` | 6 | 193 | 全部通过 |
| OFF 等价性单测 | 1 | 26 | 全部通过 |

ON 比 OFF 只多三个 observation-only 用例：

- `R0 close observation keeps first cause and bounded slots`
- `R0 close observation records a normal complete Close path`
- `R0 seek ledger keeps envelope, nesting and merge accounting`

没有非观测用例被增加、删除或改名。断言总数依赖活体夹具的数据分支，不能作为跨运行稳定常量；判定单位是用例集合和“零失败”。复核日志见 `out\rpa-r0-20261024\final-verification-20261024.log`。

## 4. Close 只读观测与拒绝门

### 4.1 字段到分支

记录类型在准入树 `engine/core/internal/cuexis_internal/execution_diagnostics.hpp:243` 中固定布局，禁止分配、格式化、锁和 stdio；只有 owning worker 通过 RAII 写入。字段分组与来源：

| 组 | 代表字段 | 写入位置/分支 |
| --- | --- | --- |
| 请求与预算 | `requestedUs`、`deadlineUs`、`firstOverBudget`、`waits` | `ExecutionHost::close()` 入口和 cleanup 等待 |
| 阶段与首因 | `marks[]`、`firstCauseStage`、`firstCause`、`firstErrorCode` | Close 各边界、flush catch、export catch |
| 操作身份 | `operationId`、`decisionOrdinal`、`scope`、`generation`、`clockSegment`、`runEpoch`、`resourceGeneration`、`rendererGeneration`、`contentIdentity` | `begin(ControlKind::close, ...)` 与 `rpaSyncIdentity()` |
| fence | `path`、`applicability`、`existingFence`、`existingIdentity`、`rejection`、`permit*`、`retire*`、`binding` | host fence、worker fence、capture retire |
| 终态 | capture/software/renderer/audio 终态、coordinator/facade、receipt/finish/confirm/shutdown/join、`closedFlag` | Close 主路径与 cleanup |
| 证据归属 | `evidenceDeliveries`、`evidenceOwning`、`unackedDeliveries`、`captureCut` | capture 路由、证据导出、Close cut |

`Identity` 的六个上下文字段（`clockSegment`、`runEpoch`、`resourceGeneration`、`rendererGeneration`、`contentIdentity`、`scope`）在正常 Close 上与公开 `context()` 逐项相等，测试在准入树 `tests/rpa/rpa_b5_host_tests.cpp:499` 校验。

### 4.2 首因与重入

`close()` 只在第一次调用时创建记录，后续调用保留 `requestedUs` 和首因；准入树 `app/player/src/execution_host.cpp:1176` 用 `requestedUs == 0` 守卫初始化。flush 失败在 `execution_host.cpp:1288` 先设置 `rpaStage_ = closeFlush`，再写 `rpaCause`，避免沿用 `captureCut`。

活体/注入证据：

| 场景 | 结果 | 证据 |
| --- | --- | --- |
| 正常完整 Close 路径上的软件退休 | `firstCause=none`，`attempts=1`，`reentries=0`，`fence.path=normalControl`，无旧 fence，fence 成功，终态关闭 | 正常 Close 路径的软件退休用例与终态断言；该记录不是 `SessionSoftwareClose` 步骤记录 |
| flush 注入失败 | 首因 `deliveryFailed`，阶段 `closeFlush`，错误 `host.rpa.capacity_exceeded`，coordinator 保持 `closing`，Close 仍完成 | `R0 close flush fault records delivery cause and still completes` |
| export 注入失败 | 首次 `operationFailed`/`exportEvidence`；重试 `control_pending`，`attempts=2`、`reentries=1`，首因不被覆盖；重试不能完成 | `R0 close export capacity failure preserves the first cause on reentry` |
| 满 capture 队列、suspended Close | 无首因，cleanup 查询 `complete`，4 个未确认交付全部转为 evidence，Close 完成 | `B5 full capture queue preserves control cut space and owning fault evidence` |

flush 注入只存在于 ON 构建；OFF 构建不编译 R0 观测段和注入钩子，OFF 测试体只验证同一个公开 Close 仍完成，没有“关掉开关后走另一条分支”的代码。

### 4.3 fence 适用性与旧 fence

- 默认值是 `missingEvidence`，不是 `notApplicable`；只有 owner 在决定路径后才会写 `applicable` 或 `notApplicable`。
- 正常 Close 路径的软件退休没有既有 fence 时，`path=normalControl`、`applicability=applicable`、`existingFence=false`、`attempted/succeeded=true`。
- 固定槽单元测试覆盖 `cleanupPermit` 下的 `notApplicable`、`existingFence` 下的旧 identity 保留，以及 `missingEvidence` 默认值。
- 没有活的旧 fence 复用路径，也没有可执行的 cleanup permit 绕过路径。`completeExecutionClose` 仍只要求新 close 序号大于旧 fence 序号，“旧 fence 跨代”风险没有被本批消除，只是被记录字段暴露。
- `permitDirected=true` 只证明 `retireSoftware`/`retireExecutionStep` 的 directed 谓词接受；它既不是 fence 路径，也不是“已绕过 fence”的证据。分支证据只认 `path`：本批所有活体 Close 路径上的软件退休 `path` 都是 `normalControl`，`cleanupPermit` 只出现在固定槽单元测试里；这不是 `SessionSoftwareClose` 步骤的路径证据。
- cleanup 的软件退休记录和 Close 路径的软件退休记录目前都写入 worker 的同一个 `rpaFence` 槽，再由 `rpaCaptureFence()` 读取/替换为 `rpaClose_.fence`。后发生的 Close 软件退休可能覆盖先发生的 cleanup 退休；因此本批不把“cleanup 与 Close 分别留证”当作已实现，也不据 `permitDirected` 推断 cleanup 路径。
- **捕获点更正（2026-10-24 文档收尾，不改变 R0 历史结论口径）**：`rpaCaptureFence()` 在 `execution_host.cpp:1330` 的 `SessionSoftwareRetire` 之后、`:1386` 的 `SessionSoftwareClose` 之前调用。因此当前 `rpaClose_.fence` 是 Close 路径上的**软件退休** fence；`SessionSoftwareClose` 分支在 `:925/:949` 写入的 `closeAttempted/closeApplied` 没有被之后的捕获点导出。`path=normalControl` 只能证明“Close 路径的软件退休走了正常 fence”，不能证明 `SessionSoftwareClose` 步骤的路径。该限制归入 R0 第 4 项残余，由 R1 在 Close 步骤后独立捕获销项；不把 R0 第 1 项改写成新结论。

### 4.4 导出容量失败

`capture_route.cpp` 的导出钩子在 ON 下触发一次 `FaultSlot::Reason::allocation`，再由 host 映射为 `host.rpa.capacity_exceeded`。这验证了“导出分配/容量失败 → 首因保留 → 重试被拒”的路径，**不是**对固定回复缓冲区溢出边界的字面测试。

本批没有把真实有界导出资源填到恰好满、再让下一次操作按契约失败；也没有为了制造失败而越界写。因此第 5 项仍是“部分”，只保留注入错误路径的证据；设备/宿主的真实根因需要独立的实机证据，不能由本夹具替代。

### 4.5 可撤回性（12 路径）

第 2 节已给出起始快照来源、patch 身份和反向应用结果。此处只强调范围与结论：

- 隔离工作树没有 `git reset --hard`、`git clean` 或对准入树的撤回操作；只使用 `git apply` / `git apply --reverse`。
- 12 个路径先恢复起始字节（两个 dirty-at-start 文件来自独立捕获，其余 10 个等于 HEAD），应用 patch 后全部等于准入树当前字节；反向应用后全部恢复起始哈希。
- 该验证覆盖 R0 对这些路径的整批撤回，不覆盖 PD-5 其余 10 个脏路径，也不证明二进制级可重复构建。
- patch 是事后以独立起始快照为基线重建的，不是作者当时提交的制品，也不是从整树 diff 手工拆分；报告把它当作可审计撤回证据，不当作历史原样 patch。

## 5. Seek 相位与工作量账本

### 5.1 S0–S6

`SeekPhase` 与 58 个 `SeekWork` 字段定义在准入树 `engine/core/internal/cuexis_internal/execution_diagnostics.hpp:378`。零值语义是“该边界没有可用值”或“该分支未发生”，不是“没有工作量”。

| 相位 | 记录内容 | 活体证据 |
| --- | --- | --- |
| S0 host identity | archive 字节、encode 调用/分配/复制、sealed identity 存在/已知 | `archiveBytes>0`，`encodeCalls>=1`，`encodeAllocations=1`，`encodeCopies=1`，`sealedIdentityPresent=1`，`sealedIdentityKnown=0` |
| S1 host protocol | operation/generation、participants、prepare/fence/activate/confirm、等待样本/原因 | operation ID 非零；`participants>=3`；prepare/fence/activate/confirm 由参与方集合写入（带 audio 时为 4/4/3/1）；等待字段由 `seekWait()` 写入，未在本夹具逐项断言 |
| S2 candidate prep | candidate 调用/有效/丢弃、entities/requirements/phaseOffsets/matcher/timer、prepared handles、config 复制 | `validCandidates/preparedHandles=1`，entities/requirements 实测 `>=1`；timer、phaseOffsets、matcher 字段由候选投影写入，夹具未逐项断言非零；`configCopies=1`，`configValues=0` |
| S3 validation | requirementSets/actualRecords/batchInputs/controlHorizons、sameKernel/sameSnapshot 调用与比较元素 | 计数器代码在 `recovery.cpp`；活体 cut 没有可比较的已记录结果行，因此本夹具的 same* 计数为 0 |
| S4 internal execution | fullTicks/idleTicks/timers/matcherSteps/folds/facts、RuntimeState/ReplayRecord 复制与峰值 | fullTicks、matcherSteps、runtimePeakElements、recordPeakElements 实测 `>=1`；timers/folds/facts 由投影写入，夹具未逐项断言 |
| S5 publish swap | objects/publishedFacts/copyAllocations/releases/oldOwnersRetained | 本夹具 `publishedFacts=0`、`objects=2`、`copyAllocations=1`，`releases=1`、`oldOwnersRetained=1` 为发布交换计数 |
| S6 clock present | clockValid/targetGeneration/targetPresentationTick/endToEndUs | 时钟有效，目标 generation/tick 非零，端到端微秒值非零 |

包络与嵌套由 `SeekEnvelope`/`SeekScope`/`SeekLedger` 维护：嵌套相位只进自己的 slot，不增加 `seekRootCount`；未归因时间是 `envelopeTotalUs - seekRootUs`；`seeksWithoutPhases` 单独计数；`mergeSeek` 只合并 worker 账本，不制造新包络；`fetchCount/fetchTotalUs` 把观测读取本身列为扰动，不混入 seek 相位。

等待字段由 `seekWait()` 在准入树 `app/player/src/execution_host.cpp:155` 写入，记录 reply/deadline/exited/pump 原因；零值表示该边界没有等待样本。

### 5.2 复制分类

| 类别 | 字段 | 说明 |
| --- | --- | --- |
| RuntimeState | `runtimeCopies`、`runtimeElements`、`runtimePayloadBytes`、`runtimePeakElements` | `1 + restores + replayPrefixCopies`，容量按实际容器容量计 |
| ReplayRecord 尾部 | `recordPeakElements` | `max(recordElements, maxBatch, elements)` |
| RecoveryInputs/config | `configCopies`、`configValues` | 配置对象复制 1 次；标量值数未暴露，保留 0 |
| 结果对象 | `nestedResultElements`、`nestedResultBytes` | projection 各容器元素与容量字节 |
| canonical bytes | `archiveBytes`、`encodeCalls`、`encodeAllocations`、`encodeCopies` | 公开 Replay 句柄只暴露字节和材料化次数 |
| 公开 DTO | `objects`、`publishedFacts`、`copyAllocations`、`releases`、`oldOwnersRetained` | 记录对象/Fact 量和 owning session 分配；没有独立的 DTO 字节复制计数 |

最后一行是残余，不是“已测为 0”。本批没有把公开 DTO 的复制量单独建成一个字段。

## 6. ON/OFF 公开轨迹等价性

等价性用例 `R0 public operation transcript is identical with observation on and off` 在两种构建下运行同一夹具，使用固定速率时钟：

- `fixedRate = {1, 1000000}`，位置 `{tick, 1}`，使公开视界精确等于 `tick * 1,000,000`，消除实时采样漂移。
- 轨迹包含 resize/load 上下文、32 次 advance 与 disposition/error、canonical replay 字节（32 条记录、41,183 B）、ReplayEvaluation 分数/漏判/Fact、完整 Snapshot（矩阵、资源、相机、clear、viewport）、present 状态、Close 状态与 Close 后错误。
- `sessionScope` 只归一化为“非零”，不比较绝对 session 句柄；其余字段逐项序列化后取 SHA-256。
- ON 和 OFF 的 digest 同为 `b4608fbfd7a89706d5c982fa8e57132e4dbb12abde7fa019cc762bbf6c1a7bf2`。

该等价性覆盖公开返回值、消息、canonical replay、评估和 Snapshot，不覆盖私有内存布局、堆峰值或所有诊断字段。它也证明 R0 记录在这些路径上没有改变公开结果，但不能替代预算/许可的计时测试。

## 7. 当前 F1 上的 Q4 重锚

### 7.1 登记与身份

窗口登记文件为 `out\rpa-r0-q4-20261024\window-registration.json`，创建于 `2026-10-24T10:24:14.693717+00:00`。三棵树在登记时的身份：

| 树 | 路径 | HEAD | 登记时 dirty | 作用 |
| --- | --- | --- | ---: | --- |
| A | `D:\Cuexis-pd5-q4-20261024-a` | `324d874f56eb466f1c8533550e80794bf106a02b` | 2 | 重建的 pre-PD 对照 |
| B | `D:\Cuexis-pd5-40k-admission` | `3e5837f63f76397429c9f7603fd7450d3ce9855d` | 46 | 当前 F1，宏 OFF |
| Q4 | `D:\Cuexis-pd5-q4-20261024` | `3e5837f63f76397429c9f7603fd7450d3ce9855d` | 52 | 当前 F1 + 迁移的 Q4 直接材料化消融 |

探针身份：

| 构建 | 字节 | SHA-256 |
| --- | ---: | --- |
| A | 4,520,448 | `455AB6E6076F3D50447C4C174C4E879A721F95179B3B64A622EF98E9B1DE01D4` |
| B | 4,699,136 | `8AFC7FD0EF4494D2FE0DBB6EEEF9E2EFE1CAF7014065100776459F2AA8D0D7E3` |
| Q4 | 4,697,600 | `0AF4963BCBE0B7A1949BB736707D4E9680BD8DF828DD6B507B8155BF5A04E54D` |

B 探针在后续重链接后已不在原构建目录，无法重新哈希；本报告保留登记时的身份，不用新文件替换旧身份。

### 7.2 协议与结果

协议为 2,097,152 reads、CPU affinity mask 4、每进程 8 个 block、正常分配器、无其他观测；3 轮顺序为 `A,B,Q4`、`B,Q4,A`、`Q4,A,B`。停止条件是进程非零、未输出 8 个 block，或三个构建的 `PD_FORMAL_ID` digest 不一致。三棵树 digest 均为 `3815754596451481321`，窗口完整。

24 个 block 的中位数：

| 构建 | ns/read |
| --- | ---: |
| A | 213.01991939544678 |
| B | 287.17219829559326 |
| Q4 | 237.9685401916504 |

配对对比（3 对，几何比与 95% 区间）：

| 对比 | 几何比 | 95% 区间 | 诊断状态 |
| --- | ---: | --- | --- |
| B/A | 1.3628428987432093 | [1.3039766617, 1.4243665712] | `over_budget` |
| Q4/A | 1.120928919368255 | [1.0780951774, 1.1654644865] | `inconclusive`（跨 1.10） |
| Q4/B | 0.8224931284463946 | [0.7702242411572208, 0.8783090822033072] | `pass` |

点中位数比分别为 B/A `1.3481`、Q4/A `1.1171`、Q4/B `0.8287`。1.19 是调查触发线，只适用于 Q4/A：Q4/A 的几何比 `1.1209` 低于 1.19，但其 95% 区间 `[1.0781,1.1655]` 跨过 1.10，状态只能是 `inconclusive`；既不能写成“通过”，也不能用 Q4/B 的 `0.8225` 作为该触发的有效对照。这里没有追加轮次，没有把 Q4 晋级为候选，也没有把任何诊断时间当作验收证据。

### 7.3 重锚限制

- A 是在 `324d874` 重建的 pre-PD 对照，不是历史 A 二进制的原样复用。
- Q4 是在当前 F1 上手工迁移的消融（见 `q4-delta.diff`），不是“原样 Q4”。
- 这不是正式 9 对 read 门；1.10 状态只作诊断，不产生接受结论。
- 登记时三棵树的状态哈希分别为 A `D976F4728097DCCA832AE03825F0844D786649592E696979D3BEAB1E1E371ACD`、B `D776107CAE41554681DF243B70261CCB7F26461EC39FA71E448A1B76B82FC8F0`、Q4 `8729985F532E15A18BAA24AD43950E58C4741F55CC1E775A595B579CC8737D29`。
- A 的可执行文件为 4,520,448 B `455AB6E6076F3D50447C4C174C4E879A721F95179B3B64A622EF98E9B1DE01D4`；Q4 为 4,697,600 B `0AF4963BCBE0B7A1949BB736707D4E9680BD8DF828DD6B507B8155BF5A04E54D`。两者保留登记身份，没有用重建后的新文件替换。
- B 探针登记为 4,699,136 B `8AFC7FD0EF4494D2FE0DBB6EEEF9E2EFE1CAF7014065100776459F2AA8D0D7E3`，但已不在登记路径/两棵工作树中，无法重新哈希；本报告保留该登记身份，不把任何新文件当成 B。
- Q4/A 的区间跨 1.10，故为 `inconclusive`；Q4/B 的 `0.8225` 不是 Q4/A 触发线的有效对照，不能用它反推 Q4/A 已通过。

## 8. R0 退出核对

| # | 条件 | 结果 | 证据 |
| ---: | --- | --- | --- |
| 1 | 字段到分支映射 | 通过 | 第 4.1 节表 + 代码位置 + Close 路径软件退休活体断言；该映射限 cleanup/正常退休，`SessionSoftwareClose` 步骤的 `closeAttempted/closeApplied` 当前无活体导出，归入第 4 项残余 |
| 2 | 首因不被覆盖 | 通过 | flush 注入、export 失败重试，`attempts=2`/`reentries=1` 仍保留首因 |
| 3 | `missingEvidence` 与 `notApplicable` 可区分 | 通过 | 默认值断言 + permit/old-fence 固定槽单元测试 |
| 4 | 有/无旧 fence | **部分** | 无 fence 的 Close 路径软件退休活体通过；旧 fence 仅单元级，无现场复用路径；`SessionSoftwareClose` 步骤未导出 |
| 5 | 导出容量失败 | **部分** | 注入 allocation 失败并保留首因；不是字面缓冲区溢出测试 |
| 6 | 宏 OFF 构建差异 | 通过 | OFF 符号 0、OFF 完整套件通过、ON/OFF 公开轨迹 digest 相同 |
| 7 | 诊断不改变返回/消息/预算/许可 | 通过（语义） | 公开轨迹 digest 相同；没有单独的预算/许可计时对照 |
| 8 | 独立 observation patch 可整批撤回 | 通过（限定 12 路径） | 起始快照 → patch → 反向应用，12/12 哈希恢复；见第 2、4.5 节 |

因此，R0 的**实现与证据收尾**可以关闭，但第 4、5 项仍是部分，故本批按“带残余”关闭，不是无保留退出。第 8 项通过的范围仅是 R0 触及的 12 个路径；它不覆盖 PD-5 其余 10 个脏路径，也不证明整棵工作树可一键回滚。上述未闭合项不在本批重开；按 §8.1 由 R1 及后续对应批次承接，R0 的历史记录保持原样。

### 8.1 Owner 裁定与残留归属

2026-10-24 owner 接受 R0 按“带残余”口径关闭：第 8 项限定通过，第 4、5 项保留 partial，维持停在 R1 之前。owner 同时裁定：**R0 保持关闭，残留项由后续对应批次承接**。R0 已记录的 partial、`inconclusive` 和未知量保持原样，不以后续批次结果回填，也不追改历史结果。

| 残留项 | 后续归属 | 阻塞什么结论 | 处理办法与销项证据 |
| --- | --- | --- | --- |
| 旧 fence/cleanup permit 只有固定槽/局部证据；cleanup 与 Close 路径的软件退休共用同一 `Fence` 槽；`rpaCaptureFence()` 在 Close 步骤前捕获，只导出软件退休记录，`closeAttempted/closeApplied` 未导出 | **R1 必须解决** | **R1 退出**；不得声明旧 fence 跨代、暂停清理、错误 binding/decision 或 Close 步骤本身已被真实 owner 路径覆盖 | 分别保存 cleanup、Close 路径软件退休与 `SessionSoftwareClose` 步骤观测；Close 未执行记 `notExecuted`，已发出未取得记 `executing/resultPending`，只有查询也失败记 `observationMissing`，已执行被拒记 `rejected` 并保留 `closeAttempted=true`、`closeApplied=false`、身份和首错；Close 步骤入口先置 `closeAttempted=true`、`closeApplied=false`，只有 receipt.status 为 applied（含明确合法的幂等 applied 回执）才置 true，`Result` 有值不等于 applied，`StepStatus::failed` 回执不得置 true，拒绝不得沿用旧值。`path` 只作软件退休/cleanup 路由证据，不作 Close 步骤分支证据；Close 步骤用 `closeAttempted/closeApplied` 五态判定。用真实 owner 路径覆盖正常 permit、错误 binding/decision/generation、旧 fence 与新 Close、正常退休增代、合法重传、迟到/重复回执。`permitDirected=true` 不能替代分支证据；不新增 fence，不扩大 failed Close，failed Close 必须有/无旧 fence 分别拒绝。 |
| `exportCloseEvidence()` 只有注入 allocation 失败；`pending_` 队列容量已有真实反例，但导出复制没有独立显式上限，真实导出容量边界未确认 | **优先与 R1 的 Close 失败留证/续行一并验证** | 不得声明“导出容量边界通过”；若 R1 的失败留证或续行依赖该容量证据，则**阻塞 R1 退出** | 队列边界已由真实 capture 反例覆盖（未满、恰好满、下一次拒绝），不重复造。导出侧先核查是否存在独立上限合同，且只归入 `noIndependentContract`/`contractUnverified`/`verificationFailed`/`verificationPassed` 之一；只有 `verificationPassed` 才算容量通过。合法填满队列后执行导出，核对复制量、拒绝码、首因和状态保留。不制造上限，不用越界写补证据。 |
| 注入 fault（flush/export）不等于现场设备/宿主根因 | **RPA B5 及后续真实设备/宿主故障验证** | 不自动阻塞全部 R1 功能验证；阻塞“设备/宿主根因已定位”的结论 | 取得首个原因、操作身份、owner 相位和真实设备侧证据，形成因果链。注入继续作为可控回归，不冒充现场故障。 |
| `configValues=0`、S0 sealed identity 未知、缺少独立公开 DTO 字节复制计数 | **R2/R3，仅当对应决策依赖时** | 阻塞“配置复制成本已测得”“产品没有身份校验”“DTO 复制成本已量化”等依赖结论；不阻塞无关优化 | 在对应决策前补测字段、字节或分配；未知量保持“不可得”，不得解释成零成本。R3b 若以 S0 身份未导出推断产品无身份校验，不成立；可信恢复仍须独立证明来源与绑定。 |
| Q4 `inconclusive`；历史 B 探针缺失，不能重新哈希或冒充 | **历史限制保留；R2 建新对照，R4 做正式读取验收** | 不得声明 Q4 已复现、旧窗口已通过或正式 read gate 已过；R4 正式验收前该结论保持未闭合 | R2 对当前生产候选重新登记 A/B/Q4 的树、构建、二进制和窗口；新 B 不套旧身份。R4 按既有 95% 配对门槛和失败保留规则重做正式矩阵，不靠重跑旧窗口改判。 |
| 独立 observation patch 只覆盖 12 路径 | **R0 已限定关闭；R1 起由各批自保** | 不得声明整树或 PD-5 其余脏路径可一键回滚；不阻塞各批自身的限定回滚 | R1 起每批保存起始字节、既有脏差异、新文件清单，生成自己的 patch，并验证限定反向恢复。保留 R0 的 patch/快照，不再扩到 R0 未触及的脏文件。 |

第 8 项接受依据保持不变：两份原有脏文件已恢复并匹配原始登记，12 路径的起始快照、结束文件和 patch 身份已核对，哈希链与正、反向 `--check` 通过；apply→reverse 循环以已记录日志为依据。该证据只证明 12 路径的源码字节级撤回，不延伸为整树回滚或旧二进制可复现。

另有两个边界继续保留：

- `failed` 在 worker 的受限清理候选与底层 `retireSoftware`/`retireExecutionStep` 的许可范围仍有差异。必须通过真实路径反例核查，包含 failed Close 无旧 fence、failed Close 有旧 fence 两种场景；Close 步骤入口本身也必须显式拒绝 failed proof，不能只靠 retire 层；`fence()` 返回 existing 不能当作许可。涉及统一或下沉检查时，单独审查最小变更。未闭合前，不得声明 failed Close 的清理许可已经统一，也不得声明 failed Close 已被拒绝。
- R2 首个提交仍须现场登记源 SHA/dirty diff、编译器/标准库、目标架构、构建类型和编译选项，并取得 `sizeof/alignof(ObjectPresentation)`、`Result<ObjectPresentation>`、`ObjectVisualRead` 及完整 scratch aggregate 的实际值。136 B 是历史口径，128 B 是推算，二者都不是当前已测布局；该门不属于 R1。

R1 的执行顺序固定为：**保全起点 → 补观测与可控反例 → 做最小修复 → 验证完整续行和生命周期**。§6.3 的四类中断必须分别实现、分别留证；542 条断言不能替代其中任何一类。R1 仍未启动。

## 9. 非声明

- 不声明 Stage 7-RPA 已退出、Stage 7-PD 已关闭、生产验收已完成或 R1 已启动。
- 不声明观测通道可以进入正式性能构建；ON 构建始终带 `CUEXIS_RPA_OBSERVATION_TEST_ONLY=1`，只能用于归因。
- 不声明注入故障等同于现场故障；flush/export 钩子只证明记录路径。
- 不声明 `configValues`、S0 sealed identity 值或公开 DTO 字节复制数已测得；它们分别保留为不可得、未知和未单独计量。
- 不声明 Q4 是“原样 Q4”、A 是历史 A，或该窗口是正式 read gate。
- 不声明所有诊断绝对时间可作验收证据；本报告没有用它们计算预算或接受线。
- 不声明 `cleanupPermit`/`existingFence` 已有活体分支证据；`permitDirected=true` 不是分支证据，`path` 才是。也不声明 `SessionSoftwareClose` 步骤本身已被当前捕获点留证；当前 `rpaClose_.fence` 是 Close 路径上的软件退休记录，捕获点在 Close 步骤之前。
- 不声明 R1 已完成；R1 的 542 条断言只是本地 cleanup-permit 证据，四类中断续接类别尚未实现，R1 不能退出。
- 不声明整棵 54/64 项工作树或 PD-5 其余 10 个脏文件可一键回滚；只声明 R0 触及的 12 个路径可由 `r0-observation.patch` 反向应用恢复起始哈希。工作树没有提交、推送或合并。

## 10. 证据索引

| 证据 | 路径 |
| --- | --- |
| 最终构建/符号/测试审计 | `out\rpa-r0-20261024\macro-audit.txt` |
| 最终复核复跑与哈希 | `out\rpa-r0-20261024\final-verification-20261024.log` |
| ON/OFF 完整测试日志 | `out\rpa-r0-20261024\on-tests-final.log`、`off-tests-final.log` |
| R0 子集日志 | `out\rpa-r0-20261024\on-r0-final.log` |
| 构建日志 | `out\rpa-r0-20261024\on-rebuild61.log`、`off-rebuild19.log` |
| 符号明细 | `out\rpa-r0-20261024\symbol-final.txt` |
| 起始/结束工作树身份 | `out\rpa-r0-20261024\start-identity.json`、`end-identity.json` |
| R0 单批撤回 patch | `out\rpa-r0-20261024\rollback\r0-observation.patch` |
| 撤回验证清单/日志/复跑 | `out\rpa-r0-20261024\rollback\rollback-verification.json`、`rollback-verification.log`、`rollback-verification-raw.log`、`verify-rollback.ps1` |
| 12 路径起始字节归档 | `out\rpa-r0-20261024\rollback\pre-r0\` |
| Q4 登记/结果 | `out\rpa-r0-q4-20261024\window-registration.json`、`results.json` |
| Q4 迁移差异/原始日志 | `out\rpa-r0-q4-20261024\q4-delta.diff`、`raw\` |

当前阶段摘要见 [CURRENT_STATUS](../../../../../CURRENT_STATUS.md)；RPA 报告索引见 [RPA README](../../03-rpa-legacy/README.md)。
