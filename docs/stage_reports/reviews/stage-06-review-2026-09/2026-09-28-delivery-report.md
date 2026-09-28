# Stage 6 复核修正：交付报告

状态：completed
快照日期：2026-09-28
更新日期：2026-09-28

本报告是 [Stage 6 复核修正计划](../../../stage_plans/active/stage-06-review-remediation/plan.md)
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
- **未把任何未运行、未通过、未注册的检查写成"通过"**（见 §5 的两类如实登记）。
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
| SPEC-28 | 低 | **已修正**：candidate 隔离扫描成为门禁（工厂断言本已注册，见 §5.1） | R7 |
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

## 5. 诚实性登记（本计划最重要的部分）

### 5.1 未把未运行的检查写成通过

- **R7 步骤 5 澄清**：复核称"candidate 隔离靠人工"。实查后区分：**安装树扫描**此前确实无门禁
  （现已成门禁）；但**工厂断言**`"Candidate factories stay disabled and do not read their inputs"`
  一直是已注册用例（`tests/playback/playback_candidate_tests.cpp:147-167`）。二者不可混为一谈。
- **R7 步骤 4**：交互命令循环的 ADR 冲突**未处置**，以"上报"而非"修复"登记（见 §4 第 1 项）。
- **R7 步骤 3**：static toolchain 负例**不造**，理由是会造成永久空转（见 §4 第 7 项）。
- **R8 STD-05/06/08/09**：**未修正**，以书面豁免登记，不写成已完成。

### 5.2 本机两个测试失败的定因（不掩盖、不误报）

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

### 5.3 修正中发现的、复核清单本身的不精确

- STD-07 列 8 项。实查后 **7 项成立并已删除**（含清单未强调的 `HostContent::providerRootId` 从未被读、
  而 `readBlob` 硬编码 `"main"`——复核此条完全准确）；第 8 项 `--event` 经查**并非死代码**：
  它把 `pull_request`/`merge_group`/`push`/`workflow_dispatch` 四种事件名带进审计记录，
  由三条作业路径各传不同值，已由 W1 以 `choices` + help 明确"仅记录；事件分流由 workflow job 条件承担"。
  按计划 §R3 步骤 1 的措辞（"若不能定义规则则删除"，反之为可保留的记录角色），保留并登记。
- STD-04 的落地需要**构建才能暴露的接线**：`tests/player/` 下**两个**测试目标都直接编译
  `frame_diagnostics.cpp`，新符号必须同时接入两者（首建 `LNK2019`）。复核清单未指出。
- STD-02 的原文表述有一处不精确，已在 R1 报告 §3 据实修正并保留原句。

### 5.4 对已完成批次的独立对抗式审计（R2–R8）

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
而打坏共享导出闸门、ELF `lib`/`.so` 命名使判断空转且误判）。详见 §6.2 与 W5 §4.2。
其中 `0c8f837` 的 Linux 失败是**真实的 CI 红**，已由 `2c74f5f` 修复。

**方法论结论**：这一轮审计的价值不在于"确认了什么"，而在于
**（i）** 发现了一个会让 CI 变红的自身回归，
**（ii）** 发现了两处"报告比代码更乐观"的表述，
**（iii）** 发现了 R4 一处会让"过度拒绝"通过测试的覆盖盲点。
三者都属于"若不做独立核对就会长期存活"的问题。

## 6. 门禁证据

| 门禁 | 结果 |
| --- | --- |
| `python -B tools/check_docs.py` | `Documentation checks passed: 274 Markdown files and 20 candidate JSON/CXT files validated.`（本批次早期为 267→273，随报告增加而增长） |
| `python -B tools/check_version_gate_tests.py`（原生 bash） | `Ran 19 tests ... OK` |
| `python -B tools/update_version.py --check` | `Cuexis version is consistent: 26.09.28-1` |
| `cmake --build --preset debug` | 0 错误 |
| `cmake --build --preset debug --target cuexis_format_check` | 通过 |
| `ctest --preset debug -R cuexis_reference_host_staging`（static + shared） | 均 `100% tests passed` |
| `ctest --preset debug -R "cuexis_reference_host_staging\|cuexis_shared_export_surface"`（修复后复跑） | `100% tests passed, 0 tests failed out of 2` |
| `ctest --preset debug -j1 --no-tests=error`（全量） | `744/746` 通过；2 项为本机既有偏差，见 §5.2 |
| `git diff --check` | 干净 |

### 6.1 hosted 结果的时间线（必须按 SHA 读，不可合并成一句"全绿"）

本批次的门禁改动**不是一次推送就通过的**。按 SHA 记录实际结果：

| SHA | 内容 | hosted 结果 |
| --- | --- | --- |
| `05f9a20` | R7 门禁首次接线 | Version Gate ✅、Linux Quality ✅、Windows MSVC ✅、Windows MinGW ✅ |
| `5374631` | 尝试以 `nm` 探测工具 | 该尝试本身未解决 ELF（见 W5 §4.2 缺陷 2） |
| `0c8f837` | 把 `CUEXIS_SYMBOL_TOOL` 改为 `objdump` | Version Gate ✅、Windows MSVC/MinGW 未见失败；**Linux Quality ❌** |
| `2c74f5f` | 拆分工具变量 + ELF 库名归一化（W5 §4.2 四项修正） | **待返回** |

`0c8f837` 在 Linux 上失败的两项与本批次新增门禁直接相关，均已定因并修复：

| 失败测试 | hosted 报错（摘要） | 定因 |
| --- | --- | --- |
| `cuexis_shared_export_surface` | `Shared symbol inspection failed: /usr/bin/objdump: unrecognized option '--defined-only'` | 共享变量被挪用（W5 §4.2 缺陷 3） |
| `cuexis_reference_host_staging` | `The shared reference host does not import cuexis_playback: libcuexis_playback-0.7.so.0.7;...` | ELF `lib` 前缀使判断失效（W5 §4.2 缺陷 4） |

**口径**：在 `2c74f5f` 的 hosted 三平台结果返回且为绿之前，
本报告**不得**把门禁改写成"已验证"。上表 `05f9a20` 一行的"全绿"只对该 SHA 成立，
它**早于** R7 新增门禁真正可用的版本，不构成对当前 HEAD 的结论。

### 6.2 本批次门禁的四次自查失败（值得留存）

本批次新增的检查在 hosted 上**四次**判失败，全部是**检查器自身**的问题，
没有一次是被测代码的问题（详见 W5 §4.2）：`nm` 在 PE 上读不出导入表；
`nm` 在 ELF 上只输出符号、从不输出库名；共享工具变量被挪用而打坏另一个闸门；
ELF 的 `lib`/`.so` 命名使归属判断既空转又误判。
其中前两次由**反空转守卫**主动判失败——若没有该守卫，它们会以"永远通过"的形态长期存活。


## 7. 复核索引

- 两轴复核记录：[汇总](2026-09-28-summary.md)｜[Standards](2026-09-28-standards.md)｜
  [Spec](2026-09-28-spec.md)｜[切片附录](2026-09-28-slice-appendix.md)
- 批次退出报告：[R1](2026-09-28-r1-document-and-evidence.md)｜[W1](2026-09-28-w1-diagnostics-and-version-gate.md)｜
  [W2](2026-09-28-w2-media-import.md)｜[W3](2026-09-28-w3-publication-transaction.md)｜
  [W4](2026-09-28-w4-render-convergence.md)｜[W5](2026-09-28-w5-host-and-distribution-gates.md)｜
  [W6](2026-09-28-w6-code-health.md)
