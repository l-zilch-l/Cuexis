# Stage 6 复核修正：交付报告

状态：completed
快照日期：2026-09-28
更新日期：2026-09-28

本报告是 [Stage 6 复核修正计划](../../../stage_plans/reviews/stage-06-review-remediation/plan.md)
的**总交付记录**。它汇总 R0–R8 全部批次的结论、逐条处置两轴复核的发现项、
列出所有**未关闭项及其归属与恢复条件**，并给出可复核的门禁证据。

## 1. 交付范围

| 项 | 内容 |
| --- | --- |
| 分支 | `stage-06-review-workspace` |
| PR | [#30](https://github.com/l-zilch-l/Cuexis/pull/30)，base `master`（`eaaf375`） |
| 基线 | `eaaf375`（PR #29 合并点 = `master`） |
| 复核范围 | `13dab93`（PR #24）→ `eaaf375`：312 个文件、+39278 / −2320 行、82 个提交 |
| 版本 | `26.09.27-1` → `26.09.28-1`（SDK API 仍 `0.7.0`，无版本跳跃） |

**边界遵守**（计划 §2 硬边界，逐条自检）：

- ADR 0042 冻结决策正文**未修改**（R7 发现的冲突以上报处置，见 §4）。
- 未实现 Stage 7A/8/9–12 任何交付物；未实现提案 1（Chart v1–v3 退役）。
- 历史报告的现象与证据原文**未改写**；凡原文有误处一律以"追加订正说明"保留原句。
- 无 `force push`、无 git config 变更、无远程分支删除、未跳过任何 hook。
- **未把任何未运行、未通过、未注册的检查写成"通过"**（见 §6 的两类如实登记）。
- 未越过版本门禁；每批一次版本滚动、串行合并。

## 2. 批次总表

| 批次 | 目标 | 状态 | 退出记录 |
| --- | --- | --- | --- |
| R0 | 4 项决策门禁的 owner 裁定记录 | completed | 计划 §6「R0 裁定记录」 |
| R1 | 文档与证据链订正 | completed | [R1](2026-09-28-r1-document-and-evidence.md) |
| R2 | 诊断码契约对齐 | completed | [W1](2026-09-28-w1-diagnostics-and-version-gate.md) |
| R3 | 版本门禁加固 | completed（SPEC-04 登记 BLOCKED） | [W1](2026-09-28-w1-diagnostics-and-version-gate.md) |
| R4 | 媒体导入修正 | completed | [W2](2026-09-28-w2-media-import.md) |
| R5 | 发布事务原子化 | completed | [W3](2026-09-28-w3-publication-transaction.md) |
| R6 | 渲染收敛 + 配置语义 | completed（SPEC-19b 登记 BLOCKED） | [W4](2026-09-28-w4-render-convergence.md) |
| R7 | 宿主与分发门禁补齐 | completed（3 项登记，1 项上报） | [W5](2026-09-28-w5-host-and-distribution-gates.md) |
| R8 | 代码健康度 | completed（4 类书面豁免） | [W6](2026-09-28-w6-code-health.md) |

## 3. Standards 轴：13 项逐条处置

| ID | 严重度 | 处置 | 归属 |
| --- | --- | --- | --- |
| STD-01 | 高 | **已修正**：改用契约码 `cxc.candidate.budget_exceeded`；`packed.field.wire_range` 独立成码 | R2 |
| STD-02 | 高 | **已修正**：补齐 `examples/reference_host/README.md` | R1 |
| STD-03 | 高 | **已修正**：状态词与已归档事实对齐（plan/completion/ROADMAP/VERSIONING） | R1 |
| STD-04 | 低（范围外） | **已修正**：两份逐字相同的级联合并为 `playbackStateName` | R8 |
| STD-05 | 中 | **书面豁免**：8 组重复实现中，最大两组是 renderer 中立层与 adapter 的逐字重复，收敛等于重构 Renderer 分层，计划 §4 明确排除 | 残余，见 §4 |
| STD-06 | 中 | **书面豁免**：重复 switch 属结构性重构，计划 §4 排除 | 残余，见 §4 |
| STD-07 | 中 | **已修正 7 项**：6 项死代码删除 + `HostContent::providerRootId`（声明后从未被读，`readBlob` 反而硬编码 `"main"`）。第 8 项 `--event` 判为**有真实角色**（把事件类型带进审计记录，三条作业路径各传不同值），已在 W1 以 `choices` + help 明确"仅记录"，非死代码 | R8 / W1 |
| STD-08 | 低 | **书面豁免**：纯转发层剥离属重构，计划 §4 排除 | 残余，见 §4 |
| STD-09 | 中 | **书面豁免**：类型强化是**公共 API 变更**，计划 §2 声明本计划不新增公共类型 | 残余，见 §4 |
| STD-10 | 中 | **已修正**：`_wfopen_s` 改按 `_MSC_VER`（补齐 `0753e6a` 只修一半的同类缺陷） | R8 |
| STD-11 | 低 | **已修正**：`.gitattributes` 覆盖 `tests/fixtures/stage6_e/**` 与 `stage6_a2/**` | W2 |
| STD-12 | 低 | **已修正**：`ENV{PATH}` 恢复不可再被 `FATAL_ERROR` 跳过；分发脚本 GLOB 行为已记录 | W5 / R8 |
| STD-13 | 低（范围外） | **已修正**：两个 `.obj` 去跟踪 + `*.obj` 规则 | R8 |

## 4. Spec 轴：32 项逐条处置

复核共 32 项（高 5 / 中 11 / 低 16），另有 1 项（SPEC-09）经二轮核实**撤回**。逐项处置如下；
每项的证据在其批次退出报告中。

| ID | 严重度 | 处置 | 批次 |
| --- | --- | --- | --- |
| SPEC-18 | 高 | **已修正**：按 R0-3 (a) 移除 `content_mismatch` 后的自动重试，改为显式 `Open`/`Rebuild` | R6 |
| SPEC-20 | 高 | **已修正**：FLAC 伪造时长改为无条件拒绝，负例可执行 | R4 |
| SPEC-21 | 高 | **已修正**：按 R0-2 (a) 拒绝线性 `gAMA`（`media.image.gamma_unsupported`），不改 canonical bytes、不重冻结 golden | R4 |
| SPEC-23 | 高 | **已修正**：单次原子替换消除双 rename 窗口；恢复逻辑还原 backup 而非删除 | R5 |
| SPEC-26 | 高 | **已修正**：按 C4 §8 的 run 表重写 `S6-G13` 归属，并追加订正说明 | R1 |
| SPEC-03 | 中 | **已修正**：`--context` 真正选择日期规则（historical 用其记录的发布上下文），`live` 语义保留 | R3 |
| SPEC-04 | 中 | **登记 BLOCKED**：门禁自身修改保护需跨阶段编号与 protection 规则裁定 | R3 |
| SPEC-05 | 中 | **已修正**：四条可执行负例（缺基线、缺 checker/test/workflow、PR 竞争、历史上下文复验） | R3 |
| SPEC-07 | 中 | **已修正**：A2 表征补齐冻结决策内容，`check_stage6_a2.py` 注册为 CTest `cuexis_contract_s6_a2` | R3 |
| SPEC-15 | 中 | **已修正**：`EffectiveSettings` 三字段改为真实协商值，不再是 requested 镜像 | R6 |
| SPEC-16 | 中 | **已修正**：四个宿主字段按可回读性改名/新增回读；`LaunchOption` 删除 | R6 |
| SPEC-19 | 中 | **已修正**（19b 登记 BLOCKED）：decoder 故障注入用例补齐；19a/19c 以变异反证 | R6 |
| SPEC-22 | 中 | **已修正**：JPEG 损坏 marker fixture 补齐；interlaced PNG 显式拒绝 | R4 |
| SPEC-24 | 中 | **已修正**：pair 锁覆盖两个目标父目录；幂等重发报告真实闭包字节 | R5 |
| SPEC-32 | 中 | **已修正**：关闭报告 §8 承接 5 条批次自认残余，并注明未随 2026-09-27 接受一并被接受 | R1 |
| SPEC-13 | 中 | **已修正**：`buildPresentationCommands` 成为唯一排序/摘要来源，adapter 删除约 16.7 KB 重复实现 | R6 |
| SPEC-01 | 低 | **登记残余（未处置）**：CXT→canonical lowering 新增的 `validateFoundationProfile` 硬拒入口**未在任何批次报告中落盘**。行为方向与加固计划一致，风险低，属"报告须对应实际调用路径"的缺口（见 §5 第 10 项） | — |
| SPEC-02 | 低 | **已修正**：`packed.field.wire_range` 独立成码，不再复用预算码 | R2 |
| SPEC-06 | 低 | **已修正**：修正 `version.build.exhausted` 抢在 `version.unchanged` 之前的诊断语义 | R3 |
| SPEC-08 | 低 | **已修正**：`--event` 加 `choices` 与 help，明确"仅记录；事件分流由 workflow job 条件承担" | R1 |
| SPEC-10 | 低 | **已修正**：追加订正说明，给出正确码 `playback.identity.resource_missing` | R1 |
| SPEC-11 | 低 | **登记残余（未处置）**：C1 专用的零上限、C1 专用 stale、`candidate.identity.resource_conflict` 独立用例**仍缺**。该项原已被 C1 退出报告自认、列入关闭报告 §8"残余（有意保留）"并经 owner 2026-09-27 接受，本计划按已接受残余保留可见性（见 §5 第 11 项） | — |
| SPEC-12 | 低 | **登记残余（未处置）**：两条计划外新增约束（`lowerCandidateRuntime` 按 execution ID 重排 `RuntimeObject`；`candidateResourceRequirements` 额外拒绝 audio+presentation 混合资产）**未落盘**。风险低，属未记录的行为扩张（见 §5 第 12 项） | — |
| SPEC-14 | 低 | **登记残余（未处置）**：D1 的线程/borrow-owning 寿命语义**仍无测试**。复核二轮已降级并指出计划只要求**定义**（接口注释已定义），验收段要求的 renderer 覆盖已具备。处置需 owner 选择：补 focused 用例，或在计划里明确它只是文档约定（见 §5 第 9 项） | — |
| SPEC-17 | 低 | **已修正**：补注**证据级别**（单测 ≠ 热拔插验证） | R1 |
| SPEC-25 | 低 | **已登记残余**：磁盘满/只读介质/配额失败仍未取证（见 §5 第 8 项） | R5 |
| SPEC-27 | 低 | **已修正**：符号级检查与非 shared 门禁接线（见下）；toolchain 与分发范围按口径登记 | R7 |
| SPEC-28 | 低 | **已修正**：candidate 隔离扫描成为门禁（工厂断言本已注册，见 §6.1） | R7 |
| SPEC-29 | 低 | **已修正**：两处计划外机制写入 `BUILDING.md`，成为已记录机制 | R1 |
| SPEC-30 | 低 | **已登记残余**：分发门禁仅 Windows（门禁本身无平台锁定，见 §4 第 6 项） | R7 |
| SPEC-31 | 低 | **已修正**：preset 计数统一为 8 | R1 |
| SPEC-09 | — | **已撤回**：二轮核实后不成立 | — |

**SPEC-27 的落地**：符号级检查（宿主导入表）、SDK minor 拒绝负例、candidate 零命中扫描
三项均已在 `cuexis_reference_host_staging` 内注册并在 static/shared 实跑；
toolchain 拒绝仅在 shared（因 static 包无该检查块，见 §4 第 7 项）；
交互命令循环的 ADR 冲突已上报（§4 第 1 项）。

## 5. 未关闭项：归属与恢复条件

本计划**没有**把任何未处置项静默丢弃。以下每项都有明确归属与可执行的恢复条件。

| # | 项 | 为什么未在本 PR 处置 | 归属与恢复条件 |
| --- | --- | --- | --- |
| 1 | **ADR 0042 `:350-351` 与实现的冲突** | ADR 承诺宿主支持 `open/play/pause/seek/reload/quit`，实际参考宿主只有 argv 解析、**零 stdin**（全仓 `git grep stdin` 只命中 `README.md`）。ADR 正文冻结，本计划 §1 禁止修改 | **需 owner 裁定**：(a) 订正 ADR `:350-351` 到脚本式宿主口径，或 (b) 在 Stage 8 授权 play/pause/quit 并相应放宽 A2 契约。在此之前 A2 表征冻结在**当前**脚本式面上（`tools/check_stage6_a2.py:294-377`） |
| 2 | SPEC-04（阶段验收号） | 需跨阶段编号口径裁定 | BLOCKED，恢复条件见计划 §9 |
| 3 | SPEC-19b（`CUEXIS_ENABLE_CHART_V5_CANDIDATE`） | 该开关在所有预设与 CI 中为 `OFF`，**无法注册**对应门禁，属构建隔离证据缺口 | BLOCKED；恢复条件：在至少一个预设中开启该选项并注册门禁 |
| 4 | STD-05/06/08（重复实现、重复 switch、中间人） | 属重构，计划 §4/§5 排除把 smell 当独立重构立项 | 残余；最大两组（renderer 中立层 vs adapter）随 **R6 的渲染收敛项移交 Stage 8** |
| 5 | STD-09（原始沉迷） | 类型强化变更公共 API，计划 §2 排除 | 残余；登记为后续设计项，需独立 ADR |
| 6 | 分发门禁在 Linux 未注册 | `CMakePresets.json:98-102` 的 `headless-*` 系列设 `CUEXIS_BUILD_PLAYER: "OFF"`，Linux 门禁不构建 Player。**门禁本身没有 Windows 锁定**（受 `if(CUEXIS_BUILD_PLAYER)` 保护） | 登记为 **Stage 8 输入**；恢复条件：在 Linux 启用 `CUEXIS_BUILD_PLAYER` |
| 7 | static 无 toolchain 拒绝负例 | static 安装包的 `CuexisConfig.cmake` 本身**不含**兼容性检查块（在 `if(CUEXIS_LIBRARY_TYPE STREQUAL "SHARED")` 内），故不存在可拒绝的面 | 已按口径登记（不造"永远空转"的负例）；static 的拒绝面由**与 flavor 无关**的 SDK minor 负例承担，且该负例在 static 下真实运行 |
| 8 | SPEC-25：磁盘满/只读介质/配额失败未取证 | 需真实或可注入的介质故障环境，本机与 CI 均不提供 | 残余（W3 已登记）；恢复条件：引入可注入的写入失败端口后补故障注入用例 |
| 9 | SPEC-14：D1 线程/borrow-owning 寿命语义无测试 | 复核二轮已降级：计划只要求**定义**（接口注释已定义），验收段要求的 renderer 覆盖已具备 | **需 owner 选择**：(a) 补 focused 用例把线程语义当合同，或 (b) 在计划中明确它只是文档约定 |
| 10 | SPEC-01：lowering 的 `validateFoundationProfile` 硬拒入口未落盘 | 属**记录缺口**而非行为缺陷（行为方向与加固计划一致）；本计划的批次报告未补登 | 残余；恢复条件：在 R2 或 Stage 8 的记录中补登该入口。**不写成已修正** |
| 11 | SPEC-11：C1 零上限 / C1 专用 stale / `resource_conflict` 独立用例 | 该项为**已接受残余**：C1 退出报告自认、列入 Stage 6 关闭报告 §8「残余（有意保留）」，owner 于 2026-09-27 接受 | 残余（保持可见）；归属 Stage 7A/8；恢复条件：补三条 focused 用例 |
| 12 | SPEC-12：两条计划外新增约束未落盘 | 同为**记录缺口**（execution ID 重排、audio+presentation 混合拒绝），spec 未写但风险低 | 残余；恢复条件：在 Spec 或 C1 报告中落盘这两条约束。**不写成已修正** |

## 6. 诚实性登记（本计划最重要的部分）

### 6.1 未把未运行的检查写成通过

- **R7 步骤 5 澄清**：复核称"candidate 隔离靠人工"。实查后区分：**安装树扫描**此前确实无门禁
  （现已成门禁）；但**工厂断言**`"Candidate factories stay disabled and do not read their inputs"`
  一直是已注册用例（`tests/playback/playback_candidate_tests.cpp:147-167`）。二者不可混为一谈。
- **R7 步骤 4**：交互命令循环的 ADR 冲突**未处置**，以"上报"而非"修复"登记（见 §4 第 1 项）。
- **R7 步骤 3**：static toolchain 负例**不造**，理由是会造成永久空转（见 §4 第 7 项）。
- **R8 STD-05/06/08/09**：**未修正**，以书面豁免登记，不写成已完成。

### 6.2 本机两个测试失败的定因（不掩盖、不误报）

本机全量 `ctest` 结束时有两个失败，均已定因，并**证明与本 PR 无关**：

1. **`cuexis_contract_version_gate`**：失败用例断言 workflow 抽取出的 shell 块可执行。
   根因：本机 `C:\Windows\system32\bash.exe` 是 **WSL 启动器**，不向子进程传递 Windows 环境变量，
   导致 `CUEXIS_BASE_SHA` 在脚本内为空。把原生 bash（`C:\msys64\usr\bin`）前置后 **19/19 全绿**。
2. **`cuexis_player_distribution`**：`VerifyPlayerDistribution.cmake:217` 报"packaged Player failed
   without a stable diagnostic"。根因：本机 `out/build/debug/bin/` 不含任何 DLL（本地 `.buildenv.ps1`
   把 vcpkg DLL 放在 `PATH` 而非 `bin/`），门禁**故意净化 `PATH`** 后 `spdlogd.dll`/`fmtd.dll`
   解析失败，进程以 `0xC0000135` 退出。

**这两项在未修改的 baseline 上同样复现**（已用 stash + 重建验证），且 hosted 三平台在
`05f9a20` 上全部 `success`。因此它们是**本机沙箱既有偏差，不是本 PR 的回归**；
本 PR 不对它们做任何"通过"声明。

### 6.3 修正中发现的、复核清单本身的不精确

- STD-07 列 8 项。实查后 **7 项成立并已删除**（含清单未强调的 `HostContent::providerRootId` 从未被读、
  而 `readBlob` 硬编码 `"main"`——复核此条完全准确）；第 8 项 `--event` 经查**并非死代码**：
  它把 `pull_request`/`merge_group`/`push`/`workflow_dispatch` 四种事件名带进审计记录，
  由三条作业路径各传不同值，已由 W1 以 `choices` + help 明确"仅记录；事件分流由 workflow job 条件承担"。
  按计划 §R3 步骤 1 的措辞（"若不能定义规则则删除"，反之为可保留的记录角色），保留并登记。
- STD-04 的落地需要**构建才能暴露的接线**：`tests/player/` 下**两个**测试目标都直接编译
  `frame_diagnostics.cpp`，新符号必须同时接入两者（首建 `LNK2019`）。复核清单未指出。
- STD-02 的原文表述有一处不精确，已在 R1 报告 §3 据实修正并保留原句。

### 6.4 对已完成批次的独立对抗式审计（R2–R8）

本批次全部退出后，对 R2–R8 的**代码**做了一次独立对抗式审计：六个审计单元各自
只拿"批次报告声称什么"与"仓库实际是什么"作比对，**不采信报告文本**，
并要求每条声明给出真实命令与其逐字输出。结论分三类：

**（a）关键代码确实已修复（审计确认）**：R2 的契约码（`cxc.candidate.budget_exceeded` /
`packed.field.wire_range`，含 `CHECK_FALSE` 钉住旧码与 16 MiB 两侧边界）；
R3 的 `--context` 真实分流日期规则、四个负例可执行且非空转、A2 已注册为 CTest；
R4 的四条行为声明（线性 `gAMA` 拒绝、FLAC 伪造时长无条件拒绝、隔行 PNG 拒绝、
损坏 JPEG 标记夹具）；R5 的原子替换（`asset_publish.cpp:1048` 为唯一触碰目标处）、
备份恢复、双父目录配对锁；R6 的适配器重复实现确实删除且改为消费中立实现、
`LaunchOption` 删除、`content_mismatch` 自动重试删除并接入 `--mode`；
R7 的三个门禁确实存在并注册、minor 负例断言的是真实文本；
R8 的 7 项死代码删除、状态名合并、`_MSC_VER` 选择、工作流空白最小改动，全部逐条落实。
**审计未发现任何"报告说已修而代码未修"的情况。**

**（b）报告表述被高估，必须更正（已按"只追加、不改写历史证据"处理）**：

| 批次 | 被高估的表述 | 更正后的实际 |
| --- | --- | --- |
| R4 | "6 条媒体负例、逐条被变异推翻" | 报告正文只有 **3** 条变异记录；无任何计数等于 6 |
| R4 | （未提交的覆盖盲点） | 无 `gAMA=45455` 夹具，gamma 的**接受**分支从未执行：一个拒绝**全部** `gAMA` 的实现会通过全套测试 |
| R5 | "`closureBytes` 幂等已修" | **只修了一半**：`asset_publish.cpp:849` 另一条幂等路径仍返回 `0` |
| R5 | 变异例 1 可推翻旧缺陷 | 旧代码在优雅替换失败时会回滚备份，该变异下用例**仍通过**；它只覆盖"无回滚的挪开" |
| R6 | "`resolveGpuDraw` 只做 GPU 句柄解析" | 它同时做四类校验并返回 `frameError`（`:793`/`:802`/`:817`/`:824`） |
| R6 | `appliedGain` 为"真实读回值" | 它是构造时传入并经 schema `[0,1]` 约束的**请求值镜像**，非设备读回 |
| R8 | STD-05 残余把 render_opengl 与 presentation_renderer 的逐字重复列为主要未修项 | 该两组**已被 R6 的 `dcc4232` 删除**，而 `dcc4232` 是 R8 提交的祖先——R8 不可能留下它们 |
| 多份 | 若干行号/路径/预设数/测试目标名引用有误 | 已在各批次报告的追加章节逐条列出并给出正确值 |

**（c）审计发现并已修复的真实缺陷（最重要的一类）**：R7 新增门禁在 hosted 上
**四次**判失败，全部是检查器自身的问题（`nm` 读不出 PE/ELF 导入表、共享工具变量被挪用
而打坏共享导出闸门、ELF `lib`/`.so` 命名使判断空转且误判）。详见 §7.2 与 W5 §4.2。
其中 `0c8f837` 的 Linux 失败是**真实的 CI 红**，已由 `2c74f5f` 修复。

**方法论结论**：这一轮审计的价值不在于"确认了什么"，而在于
**（i）** 发现了一个会让 CI 变红的自身回归，
**（ii）** 发现了两处"报告比代码更乐观"的表述，
**（iii）** 发现了 R4 一处会让"过度拒绝"通过测试的覆盖盲点。
三者都属于"若不做独立核对就会长期存活"的问题。

## 7. 门禁证据

| 门禁 | 结果 |
| --- | --- |
| `python -B tools/check_docs.py` | `Documentation checks passed: 274 Markdown files and 20 candidate JSON/CXT files validated.`（本批次早期为 267→273，随报告增加而增长） |
| `python -B tools/check_version_gate_tests.py`（原生 bash） | `Ran 19 tests ... OK` |
| `python -B tools/update_version.py --check` | `Cuexis version is consistent: 26.09.28-1` |
| `cmake --build --preset debug` | 0 错误 |
| `cmake --build --preset debug --target cuexis_format_check` | 通过 |
| `ctest --preset debug -R cuexis_reference_host_staging`（static + shared） | 均 `100% tests passed` |
| `ctest --preset debug -R "cuexis_reference_host_staging\|cuexis_shared_export_surface"`（修复后复跑） | `100% tests passed, 0 tests failed out of 2` |
| `ctest --preset debug -j1 --no-tests=error`（全量） | `744/746` 通过；2 项为本机既有偏差，见 §6.2 |
| `git diff --check` | 干净 |

### 7.1 hosted 结果的时间线（必须按 SHA 读，不可合并成一句"全绿"）

本批次的门禁改动**不是一次推送就通过的**。按 SHA 记录实际结果：

| SHA | 内容 | hosted 结果 |
| --- | --- | --- |
| `05f9a20` | R7 门禁首次接线 | Version Gate ✅、Linux Quality ✅、Windows MSVC ✅、Windows MinGW ✅ |
| `5374631` | 尝试以 `nm` 探测工具 | 该尝试本身未解决 ELF（见 W5 §4.2 缺陷 2） |
| `0c8f837` | 把 `CUEXIS_SYMBOL_TOOL` 改为 `objdump` | Version Gate ✅、Windows MSVC ✅；**Linux Quality ❌ 且 Windows MinGW ❌** |
| `2c74f5f` | 拆分工具变量 + ELF 库名归一化（W5 §4.2 四项修正） | 被取代后取消（仍带 MinGW 编译错误） |
| `7ac37f8` | 审计发现的两处代码缺陷（`closureBytes`、坏 shebang）+ gamma 正向夹具 | 被取代后取消（仍带 MinGW 编译错误） |
| `c6f1e45` | 审计更正（W2/W3/W4/W5/W6 + 本报告） | 被取代后取消（仍带 MinGW 编译错误） |
| `18c9272` | MinGW 宽路径打开修正（W6 §6） | **Version Gate ✅、Linux Quality ✅、Windows MSVC ✅、Windows MinGW ✅（push 与 pull_request 两组事件均绿）** |

`0c8f837` 的 MinGW 失败是**第二个独立的真实红**，与 Linux 的两项根因不同：
`publish_fs_internal.cpp` 编译失败——

```text
error: cannot convert 'const std::filesystem::__cxx11::path::value_type*'
       {aka 'const wchar_t*'} to 'const char*'
```

该行**此前一直只在本机 MSVC 上验证**（MSVC 走 `_wfopen_s` 分支，改动是空操作），因此本机
「构建 0 错误 + 332/23 全绿」对该分支**没有任何证明力**，W6 §4 行 D 已据此更正。
根因是 R8 的 STD-10 把 `_WIN32` 改成 `_MSC_VER` 后，MinGW 落入窄 `fopen` 回退，而 MinGW 的
`path::value_type` 同为 `wchar_t`。现行修正与验证见 W6 §6。`2c74f5f` 及以后各 SHA 因此
**仍带该 MinGW 编译错误**，直到 W6 §6 的修正提交落地。

`0c8f837` 在 Linux 上失败的两项与本批次新增门禁直接相关，均已定因并修复：

| 失败测试 | hosted 报错（摘要） | 定因 |
| --- | --- | --- |
| `cuexis_shared_export_surface` | `Shared symbol inspection failed: /usr/bin/objdump: unrecognized option '--defined-only'` | 共享变量被挪用（W5 §4.2 缺陷 3） |
| `cuexis_reference_host_staging` | `The shared reference host does not import cuexis_playback: libcuexis_playback-0.7.so.0.7;...` | ELF `lib` 前缀使判断失效（W5 §4.2 缺陷 4） |

**口径**：本批次**构建与行为**的已验证 SHA 是 `18c9272`——它是最后一个改动构建输入的提交，
上面四个门禁在 push 与 pull_request 两组事件上均为 `success`。此后仅追加**文档**更正
（按 `AGENTS.md`，文档-only 变更不改变构建输入、无需重跑构建与 CTest），
但该文档提交若改变 HEAD，其自身的 hosted 结果仍须单独核对，不得用 `18c9272` 的绿替代。
上表 `05f9a20` 一行的"全绿"只对该 SHA 成立，它**早于** R7 新增门禁真正可用的版本，
不构成对 `18c9272` 的结论；`18c9272` 的绿来自它自己那一组运行。

**中间 SHA 的取消是主动清理，不是失败遮蔽**：`2c74f5f`、`7ac37f8`、`c6f1e45` 在
`18c9272` 推送后被取消，原因是三者都带 `0c8f837` 引入、由 `18c9272` 修复的 MinGW 编译错误，
其 hosted 结果对最终 SHA 不再有信息量。`0c8f837` 与 `2c74f5f` 的真实失败记录保留在上表，
未被删除。

### 7.2 本批次门禁的四次自查失败（值得留存）

本批次新增的检查在 hosted 上**四次**判失败，全部是**检查器自身**的问题，
没有一次是被测代码的问题（详见 W5 §4.2）：`nm` 在 PE 上读不出导入表；
`nm` 在 ELF 上只输出符号、从不输出库名；共享工具变量被挪用而打坏另一个闸门；
ELF 的 `lib`/`.so` 命名使归属判断既空转又误判。
其中前两次由**反空转守卫**主动判失败——若没有该守卫，它们会以"永远通过"的形态长期存活。


## 8. 复核索引

- 两轴复核记录：[汇总](2026-09-28-summary.md)｜[Standards](2026-09-28-standards.md)｜
  [Spec](2026-09-28-spec.md)｜[切片附录](2026-09-28-slice-appendix.md)
- 批次退出报告：[R1](2026-09-28-r1-document-and-evidence.md)｜[W1](2026-09-28-w1-diagnostics-and-version-gate.md)｜
  [W2](2026-09-28-w2-media-import.md)｜[W3](2026-09-28-w3-publication-transaction.md)｜
  [W4](2026-09-28-w4-render-convergence.md)｜[W5](2026-09-28-w5-host-and-distribution-gates.md)｜
  [W6](2026-09-28-w6-code-health.md)

## 9. 追加订正（2026-09-29）：批次 R9

本节为**追加**内容，不改动本文 §2–§8 的任何原文、现象与证据。

本文 §2 批次总表覆盖 R0–R8。批次 R9 在本文写成之后经 owner 另行裁定开启，因此不在该表内；其记录见
[R9 参考宿主命令循环与 play/pause](2026-09-28-r9-reference-host-command-loop.md)
与规范 [R9-reference-host-command-loop.md](../../../stage_plans/reviews/stage-06-review-remediation/R9-reference-host-command-loop.md)。

本文 §5 第 1 项与 §7 把「交互命令循环」与 ADR 0042 `:350-351` 的冲突记为**未处置、需 owner 裁定**。
该记录在写下时准确。owner 其后裁定并开启批次 R9，须追加订正三点：

1. **零 stdin 不是缺陷**。参考宿主按设计从命令文件读取指令，不读标准输入；把「零 stdin」本身当作
   缺口是对 ADR 的误读。
2. **缺口是命令不可由外部下达，以及缺少 play/pause 语义**。ADR 0042 `:350-351` 要求宿主支持
   `open` / `play` / `pause` / `seek` / `reload` / `quit`，而当时宿主只有 argv 解析，七动词中
   `play` / `pause` / `quit` 无法被外部下达（`SPEC-27`）。
3. **原计划允许本批次补实现，不只限 Stage 8**。因此 R9 在 Stage 6 复核修正包内实现该循环，属计划内
   处置，而非越过阶段边界。

R9 状态：实现完成、本地验证完成，并在最后行为 SHA `71de8b1` 上通过 hosted 四工作流验证；**尚未获 owner
接受退出**，`SPEC-27` 仍为 open。

### 9.1 本文 §4 的 `SPEC-27` 行不等于"SPEC-27 已关闭"

本文 §4 表中 `SPEC-27` 一行写「**已修正**：符号级检查与非 shared 门禁接线（见下）；toolchain 与分发
范围按口径登记」。该格在写下时覆盖的是符号级检查与 minor 拒绝两项，**不含**交互命令循环子项；而同一
文件 §5 第 1 项写「已上报」、§7 写「**未处置**」，本节上方又写「`SPEC-27` 仍为 open」。

[双轴复核汇总](2026-09-28-summary.md) 的 `SPEC-27` 条目本身含多个子项。R7 关掉了其中两项，交互命令循环
子项被上报并转由批次 R9 处置。因此：

- 只读 §4 表会得出"`SPEC-27` 已关闭"的**错误**结论；
- §4 表的"已修正"应理解为**该子项已修正**，而非整条 `SPEC-27` 已关闭；
- `SPEC-27` 的最终关闭仍待 owner 接受 R9 退出。

§4 表原文保留不改，本小节为其限定范围。

### 9.2 本文 §5 的完整性声明与各批次自身登记的关系

本文 §5 开头写「本计划**没有**把任何未处置项静默丢弃」，其表内为 12 项。该声明应理解为
"R0–R8 各批次**报告内登记过的**项已按批次给出归属与恢复条件"，**而非**"全部残余都已收进该表"。
若干残余只登记在各自批次报告内，未进本表，例如
[W2](2026-09-28-w2-media-import.md) §4/§6 的三个缺口（见其 §13）与
[W3](2026-09-28-w3-publication-transaction.md) 追加审计 §9 登记的 pair 事务崩溃与 `.rollback.tmp.*`
永久泄漏两项。
这些项**仍然有效**，其归属与恢复条件以各自批次报告为准；本节不把它们补进 §5 表，以免事后改写该表。

本节同时指向一处本文自己已登记、但**至今未在源头更正**的账面数字：本文 §6「诚实性登记」把
「6 条媒体负例、逐条被变异推翻」登记为高估（原文即写"报告正文只有 **3** 条变异记录；无任何计数等于 6"）。
该数字的源头是 [修正计划](../../../stage_plans/reviews/stage-06-review-remediation/plan.md) 的 §5 R4 行与
§6 R4 预期结果；两处已于 2026-09-29 改为 **4 条**负例断言（其中 **3 条**有变异反证记录），
依据是 [W2](2026-09-28-w2-media-import.md) 追加审计 §2 的计数论证。

## 10. 追加核查（2026-09-29）：ADR 0042 的 S6-D01–S6-D08 实施状态

[ADR 0042](../../../adr/0042-stage-6-productization-boundaries.md) 的状态行原写「尚未实现」。该词在
2026-09-20 冻结当日是准确的（该 ADR 自己注明"不代表 A1 基线、A2 合同落盘/表征或任何实现批次已经通过"），
但自冻结起逐字未变——Stage 6 关闭时唯一的改动是把"适用范围"链接从 `active/` 改为 `completed/`——到今天
会把 8 条决策中已落地的成果一并抹掉。**其状态行已按本节结论订正；ADR 的 S6-D01–S6-D08 决策正文未作
任何改动。** 本节承担证据角色（ADR 拥有决策，报告拥有日期化证据）。

核查分两个基线，因为二者结论不同：**Stage 6 关闭基线 `eaaf375`** 与 **当前 `master` `670cca8`**
（PR #30 合并，比关闭点晚 43 个提交）。R9 只在未合并分支上，不计入本节。

### 10.1 逐条状态

| 决策 | 状态 | 关键证据 |
| --- | --- | --- |
| S6-D01 显式 source 与候选隔离 | **部分实现** | 开关默认 OFF `CMakeLists.txt:42-43`；3 个工厂在安装 `FILE_SET` 内 `engine/playback/CMakeLists.txt:37,44`、实现 `engine/playback/src/playback_source.cpp:792,852,895`；entry 表 `schemas/cuexis.chart-entry.v1.schema.json`。**缺口**：`Cuexis_ALLOW_EXPERIMENTAL` 在非文档代码中**只出现在两张禁止 token 表**（`cmake/VerifyReferenceHost.cmake:396`、`tools/check_stage6_a2.py:252`），无实现；candidate flavor 库名/安装元数据不存在；Player `--candidate-entry` 与 `--cxc` 均不存在（`git grep` 于 `app/` 零命中）；**无任何 preset 或 CI 开启 candidate** |
| S6-D02 身份与 lowering | **已实现** | 域/NUL/`v5g1:` `engine/chart/src/candidate_lowering.cpp:49-65`；重复拒绝 `:163`、`:352`；golden `tests/fixtures/stage6_a2/golden/execution_identity.json`、`prepared_identity.json`；断言 `tests/chart/candidate_lowering_tests.cpp:134` |
| S6-D03 A16 feature 与 resource closure | **部分实现（关键缺口）** | **不存在派生 feature 的 typed assembler**：`git grep -i assembl` 在 `engine/`、`tools/` 只命中身份装配（`assembleCandidatePreparedSemanticIdentity`、`assemblePreparedSemanticIdentity`、`assembleResourceIdentities`、`assembleIdentityStage`）与注释，无 assembler；feature 从 wire 读出、由调用方写入；closure 在 decode 期派生；`tools/` 无 chart-candidate 工具入口；candidate 正例为测试内注入 feature（ADR `:114` 明文禁止该做法） |
| S6-D04 renderer 分层 | `670cca8` **已实现**；`eaaf375` **未满足** | 见 §10.2。实测 `eaaf375:engine/render_opengl/src/open_gl_presentation.cpp`：`SummaryHash` 4 处、`hashCommand` 3 处、`buildDraws` 4 处、`buildPresentationCommands` **0 处**；`670cca8` 与当前 HEAD：0/0/1/**2** |
| S6-D05 配置、设备与事务 | 主体**已实现**；`eaaf375` 含 1 处**违反 ADR** | 类型与用例见 `engine/player_support/`、`tests/player_support/player_support_tests.cpp`；6 步事务 `app/player/src/player_control.cpp:586,613,647,655,677,686`。**偏差**见 §10.2 |
| S6-D06 离线媒体栈与发布 | **已实现** | 默认 OFF `CMakeLists.txt:41`；预算表与 ADR 逐项一致 `tools/media_import/include/cuexis/media_import/media_import.hpp:25-36`；CLI 门禁 `cmake/VerifyMediaImporter.cmake`；golden `tests/fixtures/stage6_e/media/`。关闭后查出 4 处真实缺陷，R4 已修（本文 §6） |
| S6-D07 版本门禁 | 主体**已实现**；1 项登记 BLOCKED | `.github/workflows/version-gate.yml:31-83`（受信任拷贝清单 `:58-71`）、`:85-119+`、`:160+`；CTest `cuexis_contract_version_gate`。**未满足**：`:321` 的"修改门禁本身需代码所有者复核"——仓库**无 CODEOWNERS**，SPEC-04 BLOCKED（本文 §5 第 4 项） |
| S6-D08 SDK 版本与具名宿主 | **部分实现** | 宿主与 clean-staged `find_package` 消费由 `tools/check_stage6_a2.py:316-338` 钉住、CTest `cuexis_contract_s6_a2`。**缺口**：`:350-351` 的六动词命令循环在 `670cca8` 上**不存在**（该 SHA 的 `examples/reference_host/src/main.cpp` 只有 argv 解析），仅由未合并的 R9 分支实现；`:333` 冻结的 SDK 目标 `0.7.1` 未落地（`cmake/CuexisVersion.cmake:8` 仍 `0.7.0`） |

### 10.2 关闭基线 `eaaf375` 上的两处偏差

两处在 `eaaf375` 上**都未满足 ADR 正文**，但**披露程度完全不同**，不能混为一谈。

**S6-D04：已披露，但未按 `:369` 处理**

ADR `:145` 要求"draw 排序/分 pass/summary 构造在新层共用，OpenGL 只消费命令并实现 GPU 上传和
绘制"，`:159` 要求旧入口"共用新实现，不继续维护独立算法"。`eaaf375` 的
`engine/render_opengl/src/open_gl_presentation.cpp` 实测：对 `presentation_renderer` 与
`buildPresentationCommands` 的引用数为 **0**——它整套自造：自有 `class SummaryHash` `:810`、
`hashCommand` `:877`、`buildDraws` `:923`，并自带 opaque/transparent 两处排序 `:1100`、`:1104`，
即排序、分 pass 与 summary 构造都不在新层共用。

**但关闭报告披露了它，且没有把它算作已完成**：`completion.md:145` 的说明列写"D2 报告记录该项未做……
**不把该重构项算作已完成**"，对应的验收行 `S6-G09` 状态为"**通过（历史缺口与残余见 §8）**"。
需要说明的是，`S6-G09` 的判据是"中立 renderer 覆盖候选事务和统一帧，Player 正式循环不依赖具体
adapter"，与 ADR `:145` 并非同一条要求，因此该行记"通过"本身不构成矛盾。
仍未满足的是 `:369`——该例外**没有先变更 ADR 或所属 Spec**。独立复核把对应项评为**高**，R6 才
收敛（[W4](2026-09-28-w4-render-convergence.md)）。

**S6-D05：未披露**

ADR `:210` 要求"不实现后台异步 prepare、取消、自动重试或自动热重载"，`:214` 要求"不把 reload 的
content mismatch 当作模式探测"，`:242` 把"模式失败后自动重试"列入拒绝清单。
`eaaf375:app/player/src/player_control.cpp:403-421` 在 **`load` 路径**上正是这么做的：当
`prepareLoad` 以 `playback.mode.content_mismatch` 失败、且调用方未显式给出 mode 时，它**重新读取
source、把 mode 置为 `CuexisAudio`、再 prepare 一次**（`:414`、`:418`、`:420`）。其 `:412-413` 的注释
写"The mode is never guessed from a failed file"，而代码恰恰是从失败码推定了模式。
**需要精确区分**：同一文件 `:446-451` 的 `reload` 路径反而是**合规**的——它把 `content_mismatch`
转成 `player.command.mode_change_requires_load` 并拒绝，符合 `:213`。违规只在 `load` 路径。

**与 D04 不同，这一处在关闭时既没有对应验收行覆盖，也没有在 `completion.md` 中披露**（对该文件检索
"重试"/"content_mismatch"/"mode" 零命中）。它由**关闭后**的独立复核登记为
`SPEC-18 [高 · 未声明 · 本轮新增]`（[spec](2026-09-28-spec.md):271），并按
[R0-3](../../../stage_plans/reviews/stage-06-review-remediation/plan.md)（`:137`）选择了"改为显式
`Open`/`Rebuild` 并新增 `--mode`"，而非修订 ADR `:242` 保留原行为。

两者在 `670cca8` 上均已收敛：`SummaryHash`/`hashCommand` 归零、该文件改为在 `:1055` 与 `:1537` 调用
`presentation_renderer::buildPresentationCommands`，仅保留诊断用的 `probeBuildDraws` `:1013`；
`load` 路径的自动重试分支消失。

### 10.3 变更控制未满足项

ADR `:369` 写「**每个例外都必须先变更本 ADR 或所属 Spec**」。§10.2 的两处例外**都没有变更 ADR 或所属
Spec**，但两者在这一点上的经过并不同：

- **S6-D04** 以"残余（有意保留）"记入 `completion.md` §8，并经 owner 于 2026-09-27 接受该残余清单——
  即走了**披露与接受**，只是没有走 `:369` 要求的 ADR/Spec 变更。
- **S6-D05** 未进入任何残余清单，也没有对应验收行，是**关闭后**才由独立复核发现的（`SPEC-18`，
  标注"未声明"）。

因此 `:369` 的未满足有两层：已披露的例外没有落到 ADR/Spec，而未披露的例外根本不在关闭时的视野内。

ADR `:362-375` 另要求 A2 完成六项
交付物——这六项**已满足**（字段级 Spec/Schema、API 草案、模块/安装图、golden、表征脚本已注册为
CTest `cuexis_contract_s6_a2`）。

### 10.4 公共 SDK 契约与 SDK API 版本（按实际实现记录）

关闭报告的原文与实际实现相反，此处按实现记录：

- **实际实现**：`engine/playback/include/cuexis/playback/playback_source.hpp:83-91` 新增了 3 个公开静态
  工厂（`fromFilesystemProjectEntry`、`fromCxcFileEntry`、`fromCxcMemoryEntry`），且该头在安装
  `FILE_SET HEADERS` 内（`engine/playback/CMakeLists.txt:37,44`）。即**本阶段确实新增了 additive 的
  公共 SDK 名字**。
- **实际版本**：`cmake/CuexisVersion.cmake:8` = `0.7.0`，`docs/api/README.md:7` 同样写"适用版本：SDK API
  `0.7.0`"。ADR `:333` 冻结的 Stage 6 目标 `0.7.1` **没有落地**。
- **结论**：`completion.md` §4 与 §7 的"本阶段没有新增公共 SDK 契约"**与实现相反**，已在该报告追加的
  §12.1 订正（append-only，未改原文）。

仍待处置的只有**版本是否补进到 `0.7.1`**：ADR `:333` 为这类 additive 新名预留了该位次，而 `:370` 又
写"SDK/库版本本轮不改代码、不新增实际依赖"（该句处在 A2 交付物的语境中）。本节按实现记录事实，
是否补进版本属 owner 决定，本节不代作裁定。

### 10.5 本次核查未能核实的事项

- **受保护分支、串行合并与 required check 的当前有效性**：只有 `2026-09-20-s6-b1-version-gate.md:140-143`
  的当日快照；本次只读、未访问 GitHub API。
- **真实安装树中是否存在 candidate flavor 与 experimental 元数据**：需 configure/install 实验树；
  本次只读未构建，结论由 CMake 源码推断（**证据强度高，但非实测**）。
- **candidate ON 的端到端行为**：无 preset/CI 配置该宏，且本次不允许构建，无法运行。
- **S6-D06 的跨平台逐字节一致**：仅能引用 hosted 报告自述，无法本地复现（Windows 主机、只读）。
- **S6-D02 的 typed requirements 逐字段对照**：只核对载体结构与重复拒绝路径，未把全部字段与
  Packed Spec §6.5 逐项比对（**证据强度：中**）。

### 10.6 SDK API 版本变更被本阶段自己的门禁锁死（2026-09-29 实测）

§10.4 把"是否补进到 `0.7.1`"留给 owner。在补做这项处置时发现一个**与版本号本身无关的阻塞**，实测如下。

`tools/check_version_gate.py:213-218` 规定 `CUEXIS_SDK_API_VERSION` 一经变更即须 `--allow-sdk-api-change`
放行，否则失败：

```text
version.sdk_api.changed: SDK API changed from 0.7.0 to 0.7.1 without explicit acceptance
```

而该开关**没有被任何工作流传入**——`version-gate.yml:74-83`、`:126`、`:175` 三处调用均不含它；全仓库
仅 `check_version_gate.py` 自身与 `check_version_gate_tests.py:237`（以 `True` 调用）引用该名字。更
关键的是 `version-gate.yml:58-71` 把检查器**从 `CUEXIS_BASE_SHA` 取出**再运行，且 `:62` 连工作流文件
本身也从 base 取——**这是正确的防篡改设计，其副作用是候选分支无法自行开启该开关**。

以工作流相同的默认参数直接调用 `compare_snapshots` 的结果（含同版本对照）：

| 场景 | `allow_sdk_api_change` | 结果 |
| --- | --- | --- |
| `0.7.0 → 0.7.1` | `False`（工作流默认） | **失败** `version.sdk_api.changed` |
| `0.7.0 → 0.7.1` | `True` | 通过，`sdk_api_change_explicitly_allowed=True` |
| `0.7.0 → 0.7.0`（对照） | `False` | 通过 |

**结论**：`0.7.1` 无法由本批次落地。要落地须先往 `master` 合入对 `version-gate.yml` 的修改，而这改的是
**防篡改契约门禁本身**——ADR 0042 `:321` 要求这类修改经代码所有者复核，而本仓库**没有 CODEOWNERS**
（SPEC-04 至今 BLOCKED），该复核无处可做；无条件传入该开关则会**永久**放开 SDK API 变更保护。

需要区分的是：**版本变更本身在契约上没有问题**。`CMakeLists.txt:645-646` 以
`COMPATIBILITY SameMinorVersion` 写出安装包版本，`0.7.0` 与 `0.7.1` 同 major.minor，正是 ADR `:333`
为 additive 新名预留的位次。**阻塞全部来自门禁的放行通路未接线，不是兼容性问题。**

经项目所有者于 2026-09-29 决定：**本批次不升版本**；`0.7.1` 连同本节实测的阻塞一并作为 Stage 7A 的
关闭前置条件登记（[Stage 7 计划](../../../stage_plans/future/stage-07/plan.md)）。

## 11. 追加订正（2026-09-29）：owner 接受 R9 退出，`SPEC-27` 记为 closed

本节**只追加**，上文任何字句与其当时判断均不改动。

本文 §9、§9.1 与 w5 报告 §7.2 都写着 R9「尚未获 owner 接受退出」、`SPEC-27`「仍为 open」。这些
表述在写入时是准确的，现已被后续事实取代：**owner 于 2026-09-29 接受 R9 退出**。

按 R9 规范文档 §0.2，`SPEC-27` 的关闭要求共六项，至此**逐条满足**：

| 要求（R9 规范 §0.2） | 状态 |
| --- | --- |
| owner 批准语义 | 满足（设计在实现前已获接受） |
| H1–H7 实测与变异 | 满足（16 条变异捕获 15 条，1 条记录为预期存活） |
| 旧契约未退化 | 满足（R9 报告 §7） |
| 最终 SHA 门禁 | 满足（见下） |
| 新报告与追加订正 | 满足（R9 报告 §9 与本文 §9/§10） |
| **owner 接受退出** | **2026-09-29 满足** |

**最终 SHA 的证据**：`71de8b1` 之后的文档提交把 tip 推到 `cc14fcd`，该 tip 在 push 与 pull_request
两个事件上共 **7 个运行全部通过**（Version Gate `36574246285`；Linux Quality `36574246145` /
`36574240797`；Windows MSVC `36574246154` / `36574240523`；Windows MinGW `36574246280` /
`36574240525`）。push 事件不触发 Version Gate 属设计如此（其 push 作业只在 `refs/heads/master` 上
做合并后审计），故上表用的是 pull_request 的那一次。已写入 R9 报告 §10。

**当时接受不做什么**（本节写作时）：不关闭修正工作包本身（其关闭尚需 owner 接受关闭报告，且 R9 报告
§5.1 记录的冗余 tick 预算守卫合并裁定当时未裁）；不构成 Stage 7A / Stage 8 的实现授权、发布授权或
合并授权。本文 §5 中与 `SPEC-27` 交互命令循环相关的未关闭项由此转为已处置，其余未关闭项不受影响。

订正：本节原把该残余写作"§4.1 的重复 tick 预算守卫"——**该节在 R9 规范文档与修正计划中都不存在**，
出处是 R9 报告 §5.1 与其 §8。原文保留于上方引号外的叙述中，以本行为准。

## 12. 追加订正（2026-09-29）：工作包关闭与 R9 契约接受

本节**只追加**，上文任何字句与其当时判断均不改动。

owner 于 2026-09-29 一并接受了两项，使上节所列的两处未决项全部消解：

| 项 | 出处 | owner 裁定 |
| --- | --- | --- |
| R9 契约，**含**冗余 tick 预算守卫 | R9 报告 §5.1 与 §8 | **接受；两个守卫保持现状、不合并**（零代码改动） |
| 本修正工作包的关闭报告 | 本文 §11 与 [修正计划](../../../stage_plans/reviews/stage-06-review-remediation/plan.md) | **接受；工作包关闭，状态词转 `completed`** |

R9 报告 §5.1 把该冗余描述为"契约属性而非覆盖漏洞"：单独移除 per-command 守卫时套件仍绿，是因为单条
超预算 tick 同时把累计值推过上限、两个守卫在同一行报同一个诊断码、而 `detail` 措辞未在任何断言中被
固定；同时移除两者会被 `n05d` 抓住。因此"是否合并"被明确留作 owner 的契约问题。owner 的裁定是
**保留两者**，即接受已实现的契约，而非删除检查。

**工作包关闭后的状态**：`stage-06-review-remediation` 状态词为 `completed`，其档案位置**不变**——
按 [文档政策](../../../DOCUMENTATION_POLICY.md) 的 `stage_plans/reviews/<topic>/` 归属规则，
评审类工作包无论开闭都留在 `reviews/` 下（先例：`stage_plans/reviews/full-review-2026-08/`
为 `completed` 且原位保留）。**未发生目录搬迁，故无兼容页需求。**

**本次接受仍不做什么**：不重开 Stage 6；不构成 Stage 7A / Stage 8 的实现授权、发布授权或合并授权；
不改变本文与 R9 报告所记录的**任何历史证据**。
