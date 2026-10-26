# Stage 7-RPA 性能整改候选设计审阅（2026-10-24）

状态：design review；本轮只读静态审阅，未构建、未运行 CTest、未登记或执行任何测量。
下文 §1–§7 保留送审版的发现与建议；同日后续五点裁定及调用链校准见 §8。候选方案已按 §8 修订，以下送审行数/hash 不代表修订后文件。
日期：2026-10-24（Asia/Shanghai）。
审阅对象：[Stage 7-RPA 性能整改方案](../../../../../stage_plans/active/stage-07/05-rpa-new/plan.md)，
660 行，SHA-256 `557F5DD489C06447CDCA93BB0FAD2D80ACCE15A0CC012B4B4A2308847F23A4C2`。
迁入前的外部设计稿为 655 行，SHA-256 `66CD46FF4F2FB76F78209C5D65562B7D57ADFC49F24EE1A6F88A9FC5BB49FED6`；
迁入只改状态行、路径表述与链接，技术结论未改写。
基线：准入树 `codex/pd5-gameplay-40k-admission`，HEAD `3e5837f63f76397429c9f7603fd7450d3ce9855d`，含 52 个未提交修改/未跟踪文件；
主树 `stage-7`，HEAD `eed36648a692333e47f798aa45be36ca850d08de`，工作区干净。
本报告只审阅设计与当前源码、合同、证据的一致性，不重开 PD-5，不改任何预算或判定线。

## 结论

方案可以作为 RPA 性能整改的 **candidate** 进入实施前评审：R0 先归因、R1 先修 Close 清理许可与续行、R2 用有界中性块处理读取、R3 按成本分三档、R4 才做集成的顺序，与
[plan-rpa](../../../../../stage_plans/active/stage-07/03-rpa-legacy/plan.md) 的 B0–B5 批次、现有冻结证据和“维护性优先”的取舍一致。
本轮未发现方向性错误，也没有发现需要推翻的既有裁定。

需要在接受前裁定的有四点，另有一点属于记录性偏差：

- **F1（阻塞 R1 退出口径）**：§6.2 把 suspended Close 的清理写成“建立或取得受限清理 fence”，但当前 R1 实现是**跳过**正常 fence、改用
  `retireSoftware` 的 accepted-binding 复核。两者不是同一机制，必须二选一：补受限 fence，或修订 §6.2 表述并把“无 fence”登记为证据缺口。
- **F2（R1 覆盖缺口）**：R1 只完成“清理许可”一半：§6.3 的四类中断续行机制均未实现，本轮只覆盖 suspended 状态的清理路径。
  542 条断言的 green 不能记 R1 完成。
- **F3（R2 入口口径）**：`136 B` 是 F1 冻结反汇编口径；当前头文件字段推算为 `128 B`，仓库内两数并存。R2 首个提交必须以
  `static_assert`/现场 `sizeof` 登记，不能把任一数字直接当 scratch 预算。
- **F4（记录性）**：`retireSoftware` 的 `directedCleanup` 仍接受任意 `failed`，收窄只在 `session_worker` 侧；谓词不一致，需记录，不能顺手扩大。
- **F5（可观测性）**：§5.2 所需的 Close 阶段、fence 拒绝和 flush 失败阶段没有独立观测字段；R0 需要内部诊断通道，
  不能从公开 Playback API 取得，也不能把“字段缺失”记成“未发生”。

## 1. 对设计 §2 事实纠正的逐条复核

| 设计中的纠正 | 复核结果 | 依据 |
| --- | --- | --- |
| 读取门方向是上界 ≤1.10，下界 >1.10 才超界 | **确认** | `measurement.py:46-56` 的 `status` 分支 |
| Reader 预算 `H=100000`、`max_records=100000` | **确认** | `tests/pd/pd5_capacity_probe.hpp:90,157` |
| 40k Seek 的 6.37 s 是外层口径 | **与已提交报告一致**；本轮未重读原始行情，仍按引用处理 | 准入树工作区报告《40k 单侧完整规模判定》（2026-10-24，未提交） |
| 布局容器是 `std::map` 且用透明比较器 | **确认** | `presentation_materializer.hpp:13` 的 `std::less<>` |
| `buildSnapshotLayout` 已绑定 runtime owner/layout | **确认** | `playback_session.cpp:917-1032`，owner/layout 写于 `:1026-1030` |
| PreparedGameplay 已经是共享不可变 storage | **确认** | `gameplay_prepare.hpp:54-72` |
| 日志已分块但尾块仍有容量 32 的嵌套 payload 复用 | **确认** | `execution_log.hpp:8-9,22-32` |
| full/idle Tick 两套动态复制并存 | **静态确认**：有 full 克隆与 idle 快路径；未运行 | `execution_kernel.cpp` 的 restore/clone 段 |

## 2. 主体逐节结论

| 章节 | 结论 | 关键依据或缺口 |
| --- | --- | --- |
| §1 推荐结论 | 成立 | 不承诺 20 ms；把 Close、读取、Seek 分成可独立证伪的三条线；PD-5 判定不动 |
| §3 不变量与成本表 | 成立 | 六条不变量与现有 owner/fence/不可变前缀语义相容；条件项已标注 |
| §4 批次映射 | 成立 | R0→B1、R1→B4/B5、R2→B1/B4、R3→B2/B3、R4→B3–B5/F1–F4，未新增编号；明确禁止跨树拼接成绩 |
| §5 R0 归因 | 成立，需补观测载体 | `presentation_materializer.cpp:53-109` 每对象一次 `readObject`、矩阵值复制、`hasRenderable` 未被 Snapshot 路径使用；`PartScope` 的 mutex/`fprintf`/`fflush` 扰动成立（`execution_diagnostics.hpp:60-125`） |
| §6 R1 Close | **部分成立** | 清理许可已实现并有 red→green；续行未实现；fence 口径见 F1/F2 |
| §7 R2 读取物化 | 成立，收益待测 | `PresentationAccess::read` 的 `void*` + 回调已支持批量；`ObjectPresentation` 为四个标志位 + 矩阵 + 材质/透明度/色调（`presentation_read.hpp:27-35`）；矩阵中转确实发生在 `readObject` 返回值上 |
| §8.1 R3a 去重 | 成立 | `makeGameplaySession` 在 `playback_session.cpp:3463-3475` 先 configure 再 prepare；`seekGameplay:4071` 每次新建会话，重复 prepare 落点确认 |
| §8.2 R3b 可信恢复 | 条件成立 | `seekCandidate:2717-2725` 的证书信任与 `restore:2734` 分离；`captureState:1950-1951` 整份复制 `rpaHistory`，§8.3 的早期 cut 反例是真实静态风险 |
| §8.3/§8.5 状态闭包与 sealed continuation | 成立，风险最高 | 依赖 `seekSource`/`rpaHistory` 的闭包证明；没有闭包就不具备启动条件 |
| §9 证据计划 | 成立 | 3 窗口×3 配对、15% 重复性、冻结 caps 与 p99 表均未被改写；F1 重锚按诊断窗口另行授权 |
| §10–§13 边界与记录 | 成立 | 非目标、停止条件、证据强度表与“静态不等于运行”的声明完整 |

## 3. 发现与处置建议

### F1 受限清理 fence 与实际实现不一致（R1 退出口径）

设计 §6.2 对 suspended Close 的要求是“允许建立或取得受限清理 fence；必须重验 accepted request、scope、decision/sequence/generation、
capture cut、audio stop/isolate 与 owner ledger”。R1 的实现是在 `session_worker.cpp:787-810` 对 `suspended`（以及非 Close 的 `failed`）
**跳过** `fence(job.proof)`，直接 `retireSoftware`；`host_candidate.cpp:226-244` 在退休前核对
`acceptedControl == proof.request`、`target == proof.request.target`、operationId、参与方、阶段，并检查 capture cut 与音频隔离，随后才启动 ledger。
也就是说：

- 已覆盖：request/operation/target 等式、参与方与阶段、cut、隔离、ledger；
- 未覆盖：fence 本身（没有 fence 位图、没有 fence generation），也没有显式的 `scope` 比较；
- 授权来源从“proof.status 自授权”改成了“accepted-binding 等式”，方向正确，但与设计文字不一致。

处置建议（二选一，R1 退出前必须写入报告并落到合同或 ADR 修订）：

1. **补受限 fence**：新增只对 accepted control 的 decision/sequence/generation/cut 放行的 fence 谓词，`proof.status` 不参与授权；
2. **修订文字**（v1 推荐）：承认 `retireSoftware` 的完整 accepted-binding 复核取代 fence，同时在 §5.2 把 Close 路径的 fence 位图记为“不适用”，
   不能记“通过”。选此路时，§6.2 的“取得受限清理 fence”必须改成“取得受限清理许可”，否则读者会以为已实现 fence。

### F2 R1 续行未实现，green 不能记完成

R1 当前只有“清理许可”这一半：suspended Close 可以跳过正常 fence 完成软件退休，4 个 Close 用例覆盖该路径。
设计 §6.3 的四类中断续行没有实现与测试；`failed` Close 的变体也没有用例。
处置：R1 退出报告必须把 continuation 列为未实施项，并把“542 assertions / 4 test cases green”限定为清理许可单元证据；
若 owner 要按设计完成 R1，需另立 continuation 批次并重新授权，不能把本轮 green 外推成“RPA Close 已修复”。

### F3 136 B 与 128 B 口径未统一

[pd5-read-result-candidate.md](../../../../../stage_plans/active/stage-07/04-pd/pd-5/pd5-read-result-candidate.md):10 记 F1 返回点写 128 bytes 中性值；
准入树工作区报告 `2026-10-23-read-regression-static-attribution`（未提交）:69,73,80 记 136 B DTO。设计 §1 与 §7 使用 136 B。
按当前头文件字段推算（两个 `string_view` 32 + 四个标志位 `hasTransform`/`hasAppearance`/`hasRenderable`/`visible` + `Mat4` 64 + `double` 8 + `Vec3` 12 + 对齐）为 128 B；
136 B 更可能对应 F1 冻结版本的多标志/对齐差异。两者都不是本轮可复算的常量。
处置：R2 首个提交加 `static_assert(sizeof(...))` 并用现场值登记 scratch 预算与报告口径；不得把 136 B 写成“已核实字节数”，
也不得用 128 B 反推旧证据错误。

### F4 retireSoftware 的 failed 谓词比 worker 宽

`host_candidate.cpp:226-228` 的 `directedCleanup` 接受 `kind == close || status == suspended || status == failed`；
`session_worker.cpp:787-810` 已收窄为 `suspended || (failed && kind != close)`。当前唯一调用方是 worker，所以运行路径一致；
但谓词留在 host 侧，未来新增调用者可能绕过 §6.2 “Close failed 首版不放行”的裁定。
处置：在 R1 报告记录该不一致；若新增调用者，先把收窄下沉到 `retireSoftware` 或统一谓词，单独立项评审，不在 R1 顺手改。

### F5 §5.2 的观测字段需要载体映射

内部 `ControlReceipt`（`execution_control.hpp:111-118`）已有 status、decisionOrdinal、4×10 的 StepReceipt、
cleanupComplete/softwareApplied，但 fence 拒绝原因、flush 失败阶段和 Close 阶段推进没有独立字段；公开 Playback API 也不暴露这些内部回执。
处置：R0 开始前先列“字段 → 载体 → 采集方式”表；缺字段的项按 §5.2 末尾原则记“归因证据不足”，不得用“没有记录”推断“没有发生”。
该通道应是内部诊断接口，不新增公开 API/ABI。

## 4. 本轮不能确认的结论

- 历史 Close 失败的根因；§6.1 的 fence 矛盾只是可达路径，不是已复现根因。
- 任何收益区间或阶段占比；1.1404、1.3728、131.1 ns 等只能作诊断锚点，不能当整改后的预测。
- 40k Seek 的 6.37 s 在序列化、准备、内部执行、比较、投影、发布、时钟与呈现之间的分解。
- R2/R3a 的语义等价与真实注册表行为；需要实现后的定向测试，不是静态审阅能证明的。
- §8.4 的周期与区间选择；设计自己标注为未定，不能预填。
- §9.4 的 14 profiles 是否具备正式证据地位；B0 冻结的只有 1/200/201 三档。

## 5. 与冻结边界的一致性

- 未改 1.10 上界、20 ms 适用 Seek、15% 重复性、D2/D3/snapshot/queue/request→ready/GPU/Pause/Resume/resize/Reset-Close/load 预算。
- 未改 B0 的 caps 来源、`memory_caps.py`/`memory_caps.v1.json`、`observer.py::memory()`，未重跑冻结构建或 F1 r4。
- 未新增公共 API/ABI/格式变更；R2 的批量访问走内部 `PresentationAccess`，不改变已安装头。
- 未把 PD-5 改判为通过；未宣称 Stage 7-PD、RPA、生产或 Stage 7 关闭。

## 6. 建议的接受方式

以 candidate 文档入库，owner 评审只需裁定四件事：

1. R1 采用 F1 的哪条路（补受限 fence 或修订为“受限清理许可”）；
2. R1 在 continuation 未实现时记为“清理许可完成、续行未实施”，还是拆成两个批次；
3. R2 是否接受“现场 `sizeof` 登记 + K=32 有界 scratch”作为启动前置；
4. R0 的内部诊断通道范围（只读、不新增公开 API）。

在这些裁定前，本文不构成实施授权，也不改变任何批次门、预算或验收标准。

## 7. 引用与保全说明

- 设计文档从外部稿迁入时只改状态行、路径表述和链接；正文技术结论与其内部分节编号未改，可按 §1–§13 对读。
- 本轮只读源码、合同、冻结工具与已保存证据；未修改产品、未构建、未运行 CTest、未登记或执行测量、未提交。
- 2026-10-23/24 的 PD 40k、读取归因与 B0/B1 报告目前只在准入树工作区，尚未进入主树；本报告按文件名引用它们，不建仓库链接，以免产生断链。
- 主树改动仅为本文、[候选设计](../../../../../stage_plans/active/stage-07/05-rpa-new/plan.md)与三处索引/交叉引用（计划总索引、RPA 分册、RPA 证据索引）；该轮未提交，两者随后由 `f0c7e3230e3abd479fda18e009a7688df89c7672` 入库。`git diff --check` 与 `tools/check_docs.py` 以最终工作区为准。

## 8. F1–F5 后续裁定（2026-10-24）

本节响应 owner 提出的五点裁定请求。复核起点为主树 `stage-7` / `f0c7e3230e3abd479fda18e009a7688df89c7672`，开始时干净；准入树仍为 `3e5837f63f76397429c9f7603fd7450d3ce9855d`，有 54 个修改/未跟踪文件。session_worker.cpp 与 rpa_b5_session_tests.cpp 的局部清理许可改动只在准入树，尚未合入主树。本轮仅修改候选方案与本审阅记录，不更改产品源码、原有未提交差异、冻结证据或裁定门槛，不构成后续实施/实验授权。

### 8.1 裁定表

| 发现 | 本轮裁定 | 对接受/退出的影响 |
| --- | --- | --- |
| F1 | 选择“受限清理许可”，不补一套清理 fence。正常 fence 条件不动，退休走 accepted binding/owner ledger/cut/isolation 复核 | 机制与 §6.2 文字已统一；原“无 fence 位图”仍为观测缺口。只有采集证明无 fence 路径才将未执行检查标不适用，不能填 0/通过；R1 仍须关闭完整绑定与生命周期证据 |
| F2 | 保留 R1 一个批次，台账区分清理许可局部候选与四类续行未实施，不用拆批来提前退出 | 542 assertions / 4 test cases 仅引用审阅记录/本轮反馈中的清理许可单元结果；本轮未重跑或核对完整原始运行绑定。它不证明许可全矩阵、Close 续行或 R1 完成 |
| F3 | 136 B 历史反汇编口径与 128 B 头文件推算并列保留来源；当前尺寸不裁定为二者之一 | R2 首个提交以真实类型现场 sizeof/alignof 登记 ObjectPresentation、Result 包装、借用行及完整 scratch，并加目标布局/预算 static_assert；K=32 保留，缺登记不通过入口 |
| F4 | 记录底层 failed 谓词比 worker 无 fence 分支宽，本轮不改变代码或扩大许可 | 新增调用者/扩大路径/关闭该缺口前，单独审查最小统一或下沉谓词；failed Close 有无旧 fence 均列反例，不能声称 worker 已提供全路径拒绝 |
| F5 | 采用 owner-local、定长、编译期关闭、只读导出的内部诊断通道；字段映射已写入 §5.2 | Close 阶段、flush 首次失败阶段、fence 实际拒绝和清理许可结果仍待实现。没有字段就是证据不足；不新增公开 API/ABI，不改变 ACK/控制或时间预算 |

方向与 R0→R4 次序维持，PD-5“未通过但定案”及各项冻结预算不变。上述是当前设计选择，不是产品修复、性能接受或整个方案已经获得 owner 接受的声明。

### 8.2 F1/F4 调用链校准

F1 的二选一结论成立，但原 §3“也没有显式 scope 比较”只描述 retireSoftware 的局部代码，不能外推整个退休路径。准入树 `retireExecutionStep`（playback_session.cpp:1879–1898）先调用 `validateExecutionControlBinding`（:1583–1595），显式检查 owner、request.key.sessionScope 与 accepted control 的 scope、operationId 和 target；随后再比较 accepted request、cut 与音频隔离。这些检查不能被方案改名删掉。

同时，scope 已有检查不等于 decision/generation 全部获证。当前 `retireSoftware` 的 request/target 等式并不单独证明 `proof.decisionOrdinal` 已与 `acceptedHostDecision` 比较、或退休前当前代数已与 expected software generation 比较。需按 §6.2 分别映射来源、检查点、错值反例和终态重传；缺项继续阻塞 R1 退出，不增加 fence 来掩盖证据缺口。

F4 的“当前唯一调用方”限定为非测试产品调用方：搜索确认 worker 的调用在 session_worker.cpp:806；tests/rpa/rpa_b4_tests.cpp:1092、1159 另有直接测试调用。host_candidate.cpp:226–228 与 Playback retireExecutionStep 的 failed 状态条件更宽，仍须 accepted binding 和 cut/isolation，不能误写成任意状态可绕过全部授权。

worker 的 `suspended || (failed && kind != close)` 只选择是否跳过 fence；非该分支会调用 fence()，而该函数可能直接返回已有 fence（session_worker.cpp:248–251）。因此“failed Close 不走无 fence 分支”不等于“failed Close 在所有路径已被拒绝”。本轮将该情形列为静态待证反例，不声称已运行复现，也不顺手改宽/改窄底层谓词。

### 8.3 核查范围与保全

本轮核对了两树实时身份、准入树两处新增代码/测试差异、retireSoftware → retireExecutionStep → validateExecutionControlBinding 调用链、fence 复用与 confirmation 分支、ObjectPresentation/Mat4/Vec3 字段、历史 136 B 归因记录、主树 128 B 返回路径记录、诊断字段及实时宿主边界 §7。没有运行 C++ 构建、CTest、542 条断言、sizeof 探针、pilot、ETW、40k 或任何新测量。没有声称历史 Close 根因已闭合、四类续行已实现或性能达标。

准入树 54 个修改/未跟踪文件按本轮开始快照保全；文档检查采用 `python -B tools/check_docs.py` 与 `git diff --check`，其结果仅说明文档满足仓库检查，不替代 R0/R1/R2 产品验证。当前执行摘要仍以 CURRENT_STATUS.md 为唯一权威；本节只限定本候选的审阅裁定与证据范围。

修订后的候选方案为 700 行，SHA-256 `872D69ABA1509472B9FE2638AA02E2AAFC3648CB91AF82F0127BA6DDA24BF867`；该值只标识 §1.1/§5.2/§6.2/§7.1 修订后的当前文本，不冻结后续修改，本文不标注自身哈希。
