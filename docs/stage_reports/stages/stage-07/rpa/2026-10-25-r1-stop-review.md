# R1 观测拆分、C6 与 failed Close 停审证据

日期：2026-10-25。

状态：R1 已进入实施，**停在 C7/C7a 最小变更审查，尚未完成或退出**。本轮先补齐工作区文档并通过检查，再修改准入树观测与测试；生产许可判断未改。候选拒绝 patch 已生成并通过只读 apply check，尚未应用、编译或验证该候选修复。

## 1. 范围与起点

执行依据：[R1 实施与验证计划](../../../../stage_plans/active/stage-07/rpa-r1-implementation.md)、[整改总计划 §6](../../../../stage_plans/active/stage-07/plan-rpa-performance-remediation.md)；R0 保持[带残余关闭](2026-10-24-r0-observation-and-anchor.md)。

主树 `D:\Cuexis-worktree` / `stage-7` / HEAD `0157486800f8f40a4121ef0d5ab26bd041a08304`；准入树 `D:\Cuexis-pd5-40k-admission` / `codex/pd5-gameplay-40k-admission` / HEAD `3e5837f63f76397429c9f7603fd7450d3ce9855d`。主树原四条文档 dirty 保留；准入树原 64 条（22 modified + 42 untracked）全部保存起始字节，另保存 6 个预计触碰干净文件、4 个候选新增路径的 absent 记录及构建身份。未 reset/clean、提交、推送或合入主树。

独立证据根为准入树 `out/rpa-r1-20261025/`，不覆盖旧 `out/rpa-r1-20261024/`。`start/identity.json` 保存授权原文与两树身份，`start/manifest.json` 保存字节/hash，`start/preexisting.patch` 分离既有差异；主树文档起点另在 `start/main-docs/`。

## 2. 本轮改动

代码只在准入树，涉及原本已脏的七个路径：`execution_diagnostics.hpp`、`session_worker.hpp/.cpp`、`execution_host.hpp/.cpp`、`rpa_b5_host_tests.cpp`、`rpa_b5_session_tests.cpp`。

- worker 分别保存 cleanup、Close 软件退休、Close 步骤；观测回复返回三条独立记录，并只读导出当前 fence 身份以核对旧 fence 是否保留。
- host 分别捕获 cleanup/Close 软件退休，Close 步骤结束后再捕获步骤记录；原 reply 在途时不以观测覆盖它。`Close.fence` 现在只表示 Close 步骤，旧 R0 退休断言改为读取 `closeRetireFence`，原断言内容保留。
- Close 步骤观测支持五态；入口重置 attempted/applied，按实际 StepReceipt.status 判 applied，拒绝保留首错；同一 operation/decision 重传保留首次错误。该修订只影响观测，不改返回值或许可。
- 新增步骤拒绝/重试观测用例、真实 host C6，以及分别检查 retire/Close-step 的 C7/C7a。

`host_candidate.cpp::retireSoftware`、`PlaybackSession::retireExecutionStep` 与 worker 原生产许可谓词均未修复。当前 failed Close 缺口正是这些原行为的失败证据，不是被撤回的绿色结果。

## 3. C6：已接受旧非 Close 的实际 host 清理

在真实 ExecutionHost 中用 TestPresentationRenderer.markDeviceLost 使 Load 在 D2 已接受、software prepare 已完成、software fence 未建立时失败；返回 `host.rpa.context_stale` 并进入 suspended。随后一次 Close 完成旧 Load 的 cleanup permit，再以独立 operation/decision 关闭、退出与 join。

ON 单独运行 `[r1][c6]`：**1 case / 28 assertions 通过**。原 Load operation=1，cleanup path=cleanupPermit、retireApplied/referencesReleased=true、无旧 fence；新 Close operation=2、decision 大于旧 decision、attempts=1/reentries=0，最终 Closed。OFF 的相同场景包含于后述回归并通过。

这证明所构造的“已接受 Load 在 fence 前失败”场景，不宣称所有旧 job/typed-reply 中断已覆盖，也不替代第 1 类 archive/inspect 续行。本故障是可控测试 renderer 状态，不是现场设备根因。

## 4. C7/C7a：旧行为失败与停止

反例沿真实 SessionWorker 投递、poll、ack；先实际 Load，再由 D2 接受完整绑定的 Close。只在测试输入将 proof.status 设为 failed，其余 accepted request/decision/target/generation 不变；capture/audio 不参与，隔离前置因此合法不适用。不是伪造新的 accepted decision，也不将注入视为生产故障来源证明。

有旧 fence 场景保留未确认 Load 的真实 candidate fence；无旧 fence 场景先正常确认 Load。Close-step 场景先使用合法 preparing proof 完成真实退休，再仅以 failed proof 请求 Close 步骤，隔离验证该入口。

| 场景 | 原行为结果 | 合同结论 |
| --- | --- | --- |
| C7 无旧 fence 的 retire | 返回 host.rpa.control_pending，generation 2→2，ledger 无 applied | 本场景拒绝通过 |
| C7a 有旧 fence 的 retire | 返回 applied，referencesReleased=true，generation 2→3，ledger 新 applied，session Loaded→Empty | **失败**；旧 fence 身份仍保留，但错误地让 failed proof 进入退休 |
| C7 无旧 fence 的 Close step（合法退休后） | 返回 applied，Close ledger 新 applied | **失败**；步骤入口没有显式拒绝 failed proof |
| C7a 有旧 fence 的 Close step（合法退休后） | 同样 applied | **失败** |

ON：4 cases / 575 assertions，其中 1 case 通过、3 cases 失败，20 条断言失败；exit=42。OFF：4 cases / 507 assertions，同样 1 通过、3 失败，12 条断言失败；exit=42。ON/OFF 的 applied、错误码、generation 和 ledger 轨迹一致；ON 多出观测断言。失败原样保留，不删用例、不改期望、不重跑至通过。

根因链经源码与实际输入对应：worker 的 directedCleanup 排除 failed Close，但 `fence()` 先返回已有 fence，因而没有拒绝；底层 retireSoftware/retireExecutionStep 仍允许 kind=close；Close-step 的 binding/Empty/已退休校验不检查 failed status。无旧 fence 的退休日志证明“未走 bypass”只对该入口组合产生拒绝，不能推广到旧 fence 或 Close 步骤。

按实施计划 §4 和桌面交接 §7.2/§7.3/§11.2，此时停在最小变更审查；本轮未执行生产许可修复或四类续行。

## 5. 可审查但未应用的最小候选

准入树 `out/rpa-r1-20261025/review/proposed-failed-close-refusal.patch`：3 文件、4 个拒绝入口，3212 bytes，SHA-256 `A99873EA6626AAC9DD46644E63C48730C6D679B00ED81BCF97765019868072BF`。`git apply --check` 通过；候选字节与 active 字节 hash 分别保存在 `review/proposed-manifest.json`。候选收尾时将 Close-step 拒绝条件也限定为 kind=close && status=failed，旧草稿保存在 `review/proposed-failed-close-refusal-v1.patch`，不改变非 Close 错误路径。**它不是已验证修复。**

| 入口 | 候选改动与保留边界 |
| --- | --- |
| worker SessionSoftwareRetire | 退休观测初始化后、调用 fence/retire 前显式拒绝 kind=close && status=failed；保留旧 fence |
| worker SessionSoftwareClose | 原 binding 校验后、任何 close ledger start/副作用前拒绝 failed Close proof |
| HostCoordinationAccess::retireSoftware | 原 owner snapshot 校验后、查终态 ledger/退休前拒绝 failed Close |
| PlaybackSession::retireExecutionStep | 原完整 binding 校验后、unload 前拒绝 failed Close |

复用 `host.rpa.control_pending`，不加公开 API、ControlKind、协议/格式字段、fence、timeout 或预算。不改变正常 preparing/suspended Close、已接受旧非 Close 的受限清理、原绑定/cut/audio 校验。此候选专门收窄 failed Close，不把原 status 直接当许可。

若获该最小修复授权，先补 C7b 的三层同输入差异及相关 C2/C8/C9/C10 反例，逐个入口应用与验证，保持本次红色证据；检查原 ID 合法重传、旧代新请求拒绝、generation 只增一次。之后继续原 R1 的四类续行与生命周期门。不得以本候选的静态形状宣称底层已统一或完整 R1 可退出。

## 6. 本地检查与限制

MSVC 14.51.36231 / x64 / Ninja；独立构建目录均在准入树。Release ON/OFF 都使用 STATIC、CUEXIS_ENABLE_CHART_V5_CANDIDATE=ON、CUEXIS_WARNINGS_AS_ERRORS=OFF，沿用既有依赖安装但不覆盖既有 R0 构建。命令与原始输出保存在证据根 logs/。

| 检查 | 结果 |
| --- | --- |
| 源码前文档检查 | 475 Markdown / 20 candidate JSON/CXT，通过；diff --check 通过 |
| 独立 Debug ON 构建 | 构建通过；运行新观测与既有 R0 回归都出现 SIGSEGV/stack overflow（exit -1073741571），默认 PE stack reserve=0x100000；未增大栈、改产品或重跑掩盖，Debug 测试不记通过 |
| Release ON 步骤观测 | 1 case / 167 assertions，通过 |
| Release ON 原 R0 回归 | 6 cases / 199 assertions，通过；不回填 R0 历史证据 |
| Release ON 回归（单列排除 C7/C7a 红色反例） | 47 cases / 36190 assertions，通过；**不是完整套件全绿** |
| Release OFF 同范围回归 | 43 cases / 35477 assertions，通过；ON-only 观测用例不编译 |
| ON C7/C7a | 1 passed / 3 failed，20 条断言失败；未修 |
| OFF C7/C7a | 1 passed / 3 failed，12 条断言失败；未修 |
| Release ON cuexis_architecture_tests | 1/1，通过 |

本轮 ON executable SHA-256：`68086006E0968301C515D08A23807BA1CE25B1E58B5349D5BABEBA717650E186`；OFF：`4F7E85A28E933C16D96B3AB5B79E8520BC060656D84C77D3A1C16D31FE83F3CC`。路径分别为 `out/build/rpa-r1-release-on-20261025/bin/cuexis_execution_host_tests.exe`、`out/build/rpa-r1-release-off-20261025/bin/cuexis_execution_host_tests.exe`。

G0 起点保全已做；G1 已实现并通过基础 Release 用例，但在途/缺观测等完整受控覆盖尚未全部证明，不能记完整 G1 退出。G2 未齐；G3 因上述反例停审。G4–G8 四类续行与生命周期/1000 ms 未实施，G9 导出容量合同事实尚未核查分类，G10 全量生产预算/续行等价未成立。ON/OFF 此次只支持本报告所列局部语义与反例轨迹，不支持正式预算结论。

## 7. 限定回滚与证据索引

本轮 checkpoint patch 只含观测与反例，另列主树文档 patch；不能冒充 R1 全批完成后的 G11。以本轮 start 起始字节生成，在一次性副本按起始 hash → 正向 check/apply → 结束 hash → 反向 check/apply → 起始 hash 恢复；新增文档反向后不存在。详细 hash/路径与结果在 rollback/manifest.json 和 verification.log，不在活动树回滚。

| 证据 | 准入树下路径 |
| --- | --- |
| 本轮起点字节、身份、既有 patch | out/rpa-r1-20261025/start/ |
| 观测基础用例 | logs/g1-release-observation.log、logs/g1-release-r0.log |
| Debug 栈溢出原始记录 | logs/g1-observation.log、logs/g1-r0-regression.log |
| C6 优先反例 | logs/c6-old-behavior.log |
| C7/C7a ON/OFF 红色证据 | logs/c7-c7a-old-behavior.log、logs/off-c7-c7a-old-behavior.log |
| 单列排除红色反例后的回归 | logs/on-regression-excluding-known-counterexamples.log、logs/off-regression-excluding-known-counterexamples.log |
| 架构检查 | logs/architecture.log |
| 未应用候选 | review/proposed-failed-close-refusal.patch、review/proposed-manifest.json |
| 本轮观测/反例 checkpoint patch 与验证 | rollback/r1-observation-counterexamples/ |
| 主树文档独立 patch 与验证 | rollback/r1-workspace-documents-final/（候选收尾后的最终文档；原 checkpoint 另存） |

不声明 R1 本地验证完成、R1 退出、pilot 通过、容量通过、设备根因、B4/B5 全退出、RPA 或 Stage 7 关闭。没有运行 pilot、40k、ETW、Q4 或正式窗口。下一步是 owner 裁定本节最小拒绝候选，再按原批次继续。
