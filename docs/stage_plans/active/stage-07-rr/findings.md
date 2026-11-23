# Stage 7-RR 发现登记册

状态：active；登记原始发现、核验与处置，不代替修复证据或实施授权

更新日期：2026-11-23

<!-- cuexis-meta
id: PLAN-active.stage-07-rr.findings
status: active
authority: owns
-->

返回 [Stage 7-RR 计划](plan.md)；阶段报告入口见[Stage 7-RR 报告索引](../../../stage_reports/stages/stage-07-rr/README.md)；
当前进度唯一以 [CURRENT_STATUS](../../../CURRENT_STATUS.md) 为准。

## 阅读索引

本册按问题对象分三块，按此顺序阅读：

1. **现行问题（已实施）**：§1–§11，2026-11-17 全项目复核、假保证扫描、外部审查与工具审计的发现，以及修复清单与复核方法。
2. **候选设计与文档问题（未实施）**：§17–§21 为前轮设计层审查，§22 维护旧问题承接，§23 记录正式实施前检查，§24 记录统一实施前的设计裁定；历史待裁状态以随后具名处置为准。
3. **处置记录（按日期）**：§12–§16，历轮审查的处置过程，按当时时点成立，不改写结论。

编号规则：`RR-Fxxx` 按登记时点递增，不因重组重排；部分章节号因此不连续。严重度与证据状态见 §0。

## 0. 口径与证据状态

严重度只表达"对已接受合同的破坏程度"，不表达修复成本：

| 级别 | 含义 |
| --- | --- |
| P0 | 现行权威文档陈述与仓库事实直接冲突，会误导接手者或消费者做出错误判断 |
| P1 | 合同承诺与实现不一致，或互操作/接受集缺陷：按文档构造的输入被拒、按文档实现的外部工具不兼容、同一失败得到不同稳定码 |
| P2 | 未文档化的收紧/放宽、未登记的受限行为、死字段或死码、可观测性缺失 |
| P3 | 冗余、风格不一致、门禁过宽或过窄，不改变行为 |

核验状态固定为 **未复核 / 已核实**；已核实只表示有具名复核者和证据，不代表关闭。处置状态固定为 **开放 / 处理中 / 已修 / 已处置 / 延后 / 撤回**，不把核验与关闭混用。

真正定义表以 `<!-- cuexis-findings: definitions -->` 标记，并使用七列。原始发现、原始依据和旧 ID 受保护；修复清单、承接表和历史处置表只引用定义。原严重度、判定与旧处置文字保存在原字段内，不因当前状态变化覆盖。事实纠正追加具名订正，保留旧断言及替代关系。

当前处置依据使用可检查的 `依据：` 字段；已核实另写 `复核者：`、`复核证据：`。已修另写 `改动修订：`、`实际验证：`、`理由：`、`决定依据：`；已处置或撤回写核验、理由与决定/反证；延后写 `责任阶段：`、`恢复条件：`、`决定依据：`。字段存在只证明记录完整，证据是否充分及接受是否有效仍须人工判断。

### G02 首次迁移核对（2026-11-23）

本轮仅转换真正定义表，原章节、发现、位置及旧结论保留。冻结输入见仓内[原件快照](../../../stage_reports/stages/stage-07-rr/evidence/documentation-governance/inputs.zip)、[输入清单](../../../stage_reports/stages/stage-07-rr/evidence/documentation-governance/manifest.json)及[原工作树补丁](../../../stage_reports/stages/stage-07-rr/evidence/documentation-governance/working-tree.patch)；适用 Git 输入为 `ca5cbb9121b015ae0b3b5d681610ca94dac78184`。逐行按旧 ID 对照原表，七列中的原始发现与原始依据共同保存旧行各信息列；§23 的旧处置保存在原始依据，F547 从 §24 原议题行提取，原表继续保留引用。不是按新摘要哈希自行批准转换。

本轮按旧 ID 对照 §22 与 §23–§26，当前责任列采用其中仍有效的具名任务；T-18 承接本轮文档门禁，无法确认者由 T-09 核证。对 30 条旧修复说法逐条区分文档、工具/构建及混合问题：15 条文档订正在当前正文和 ca5 已提交对应段核对后恢复已核实/已修，3 条按已接受的文档收窄恢复已核实/已处置；F121、F209 的文档切片已核实但因实现残余保持开放。其余旧工具、构建或运行声明没有本单元完整关闭证据，保留原说法及缺口，不一律伪造验证或重新裁定设计。

本轮具名复核者为 Codex-G02（2026-11-23）：直接读取当前文档，并独立读取 Git ca5 对应段作定向断言；执行 `check_docs_facts.scan` 的三篇版本文档检查及 `limits_is_current()`、`player_options_are_current()`、`frame_digest_is_current()`，均成立。ca5 只作为包含既有订正的已提交快照，不把本轮未提交的转换当修复提交。原核验、原处置和原证据留在受保护字段；当前已修只涵盖该行写明的文档问题，不能外推产品运行、旧消费者或正式发行通过。未闭合产品项保持原已接受设计与任务，后续按真实证据更新处置。

`tools/verification/check_findings.py` 比较可信 Git 基线的旧表或七列表，不接受当前旧结构、删标记或删定义。默认基线取 `CUEXIS_DOCS_BASE_REF` 或 HEAD；CI 基准由集成入口提供，无法取得 Git 输入时报未检查并失败。

## 第一部分 现行问题（已实施）

本部分登记已实施、已发布或已交付对象的缺陷：现行文档与仓库事实冲突、实现与合同不一致、工具链与证据问题。编号按登记时点排列。

### 1. P0（状态文档与仓库事实冲突）

<!-- cuexis-findings: definitions -->
| ID | 原始发现 | 原始依据 | 核验状态 | 处置状态 | 责任/任务 | 当前处置依据 |
| --- | --- | --- | --- | --- | --- | --- |
| RR-F001 | 自称日期版本 `26.11.14-1`（历史记录），实际 `cmake/CuexisVersion.cmake:1-4` 与 `vcpkg.json:3` 均为 `26.11.16-1`（2026-11-16 提升后未回写） | `docs/CURRENT_STATUS.md:5`、`:80`<br>原证据状态：已修（2026-11-22 复核：CURRENT_STATUS 已写 26.11.18-1，与版本文件一致） | 已核实 | 已修 | T-02 | 依据：原字段与 §22 历史处置；复核者：Codex-G02（2026-11-23）；复核证据：[当前状态](../../../CURRENT_STATUS.md)（当前正文与 ca5 已提交对应段）；理由：当前摘要保留版本权威引用，未继续使用过期构建身份；只恢复文档订正状态；决定依据：§22 既有文档处置与原字段记录；实际验证：当前/ca5 状态字段与版本权威引用定向断言通过；改动修订：ca5cbb9121b015ae0b3b5d681610ca94dac78184（包含原文档订正的已提交快照） |
| RR-F002 | 自称"执行分支 `codex/r4-closeout`；唯一集成 PR 为 #44，Ready for review"；实际分支 `stage-7.5`，PR #44（`564e0d90`，2026-11-14）与 #45（`aac83e5e`，2026-11-16）均已合并 | `docs/CURRENT_STATUS.md:26`<br>原证据状态：已修（2026-11-22 复核：现写分支 stage-7-rr、#44/#45 已合并） | 已核实 | 已修 | T-02 | 依据：原字段与 §22 历史处置；复核者：Codex-G02（2026-11-23）；复核证据：[当前状态](../../../CURRENT_STATUS.md)（当前正文与 ca5 已提交对应段）；理由：已删除旧执行分支及 PR 就绪的当前断言；ca5 已有后继分支及合并历史说明；未据此授权发布；决定依据：§22 既有文档处置与原字段记录；实际验证：当前/ca5 状态正文定向读取与旧错误断言缺席检查通过；改动修订：ca5cbb9121b015ae0b3b5d681610ca94dac78184（包含原文档订正的已提交快照） |

两条都与同文件 `:22` 的"当前为 Stage 7.5"自相矛盾，属"唯一状态权威失效"，因此列 P0。

### 2. P1

#### 2.1 状态与文档漂移

<!-- cuexis-findings: definitions -->
| ID | 原始发现 | 原始依据 | 核验状态 | 处置状态 | 责任/任务 | 当前处置依据 |
| --- | --- | --- | --- | --- | --- | --- |
| RR-F101 | 已修：AGENTS 精简后原句与 `:261` 均不存在；`ROADMAP.md:48`、`GAMEPLAY_V2_SPEC.md:2595` 仍称“AGENTS 写 Stage 12”，本轮同日订正 | `AGENTS.md`（2026-11-17 前版本）<br>原证据状态：已核实（已修） | 已核实 | 已修 | T-02 | 依据：原字段与 §22 历史处置；复核者：Codex-G02（2026-11-23）；复核证据：[路线图](../../../ROADMAP.md)、[V2 Spec](../../../formats/GAMEPLAY_V2_SPEC.md)（当前正文与 ca5 已提交对应段）；理由：原 AGENTS Stage 12 说法已改为带日期的删除历史，未再冒充现行指令；决定依据：§22 既有文档处置与原字段记录；实际验证：当前/ca5 两处订正与 AGENTS 定向断言通过；改动修订：ca5cbb9121b015ae0b3b5d681610ca94dac78184（包含原文档订正的已提交快照） |
| RR-F102 | 已修：AGENTS 精简后“Stage 7A 是当前实施准备线”“Stage 4-12 都是 future/deferred”均已删除 | `AGENTS.md`（2026-11-17 前版本）<br>原证据状态：已核实（已修） | 已核实 | 已修 | T-02 | 依据：原字段与 §22 历史处置；复核者：Codex-G02（2026-11-23）；复核证据：[AGENTS](../../../../AGENTS.md)（当前正文与 ca5 已提交对应段）；理由：原 Stage 7A 当前线和 Stage 4–12 全 future/deferred 的断言均已移除；决定依据：§22 既有文档处置与原字段记录；实际验证：当前/ca5 AGENTS 原错误短句缺席检查通过；改动修订：ca5cbb9121b015ae0b3b5d681610ca94dac78184（包含原文档订正的已提交快照） |
| RR-F103 | 仍写"当前分支 SDK API 为 `0.8.0`"；实际 `cmake/CuexisVersion.cmake:8` 为 `0.9.1`，同目录其他页已写 0.9.1 | `docs/api/diagnostics-identity-and-compatibility.md:22`、`:64`；`docs/api/GAMEPLAY_V2_ABI.md:69`<br>原证据状态：已修（2026-11-22 复核：两页现均写 0.9.1，`check_docs_facts` 无版本漂移） | 已核实 | 已修 | T-02 | 依据：原字段与 §22 历史处置；复核者：Codex-G02（2026-11-23）；复核证据：[诊断身份合同](../../../api/diagnostics-identity-and-compatibility.md)、[V2 ABI](../../../api/GAMEPLAY_V2_ABI.md)（当前正文与 ca5 已提交对应段）；理由：只恢复当前 SDK 版本文字订正，不外推 SDK 发布或运行通过；决定依据：§22 既有文档处置与原字段记录；实际验证：当前/ca5 版本段核对与 check_docs_facts.scan 定向版本扫描通过；改动修订：ca5cbb9121b015ae0b3b5d681610ca94dac78184（包含原文档订正的已提交快照） |
| RR-F104 | 仍写“当前分支为 0.7.5”，实际 `CuexisVersion.cmake:8` 为 0.9.1（`CURRENT_STATUS.md:115` 同） | `docs/formats/MATERIAL_SHADER.md:632`<br>原证据状态：已修（文档：改为 0.9.1） | 已核实 | 已修 | T-02 | 依据：原字段与 §22 历史处置；复核者：Codex-G02（2026-11-23）；复核证据：[Shader 合同](../../../formats/MATERIAL_SHADER.md)（当前正文与 ca5 已提交对应段）；理由：历史类型基线与当前 SDK 已分开描述；决定依据：§22 既有文档处置与原字段记录；实际验证：当前/ca5 当前版本段核对与 check_docs_facts.scan 定向扫描通过；改动修订：ca5cbb9121b015ae0b3b5d681610ca94dac78184（包含原文档订正的已提交快照） |
| RR-F105 | 权威矩阵把 Packed Chart v1 标为"未实现"；实际 Header/目录、字典/身份、ARCH/ENT0、TRN0/REN0/CAM0、REQ0/CNS0、语义 hash、Reader/Writer 与 40k 测试均已实现并接入 CMake | `docs/formats/README.md:40`<br>原证据状态：已修（2026-11-22 复核：README 已改 candidate 并写明 Reader/Writer 已实现；同行误引 RR-F201 已改为 RR-F105） | 已核实 | 已修 | T-02 | 依据：原字段与 §22 历史处置；复核者：Codex-G02（2026-11-23）；复核证据：[格式索引](../../../formats/README.md)（当前正文与 ca5 已提交对应段）；理由：索引已写 candidate、Reader/Writer 已实现并引用 F105；本轮仅核文档订正，不重验 Reader/Writer 或规模测试；决定依据：§22 既有文档处置与原字段记录；实际验证：当前/ca5 Packed 行状态及 F105 回指断言通过；改动修订：ca5cbb9121b015ae0b3b5d681610ca94dac78184（包含原文档订正的已提交快照） |

#### 2.2 格式互操作与接受集

<!-- cuexis-findings: definitions -->
| ID | 原始发现 | 原始依据 | 核验状态 | 处置状态 | 责任/任务 | 当前处置依据 |
| --- | --- | --- | --- | --- | --- | --- |
| RR-F111 | 原文规定完整 Header 置零计算，与 Reader/Writer 的前 92 字节算法不一致。owner 已接受按现行实现更正合同，Header 总长仍为 96 字节；兼容后果见 [D-25](architecture-design.md#d-25-兼容入口与格式合同收敛)。 | [Packed §5.1](../../../formats/PACKED_CHART_FORMAT.md#51-fixed-header96-bytes)；`packed_chart_tables_writer.cpp` / `packed_chart_tables_inspect.cpp`<br>原证据状态：合同更正已完成；实现未变；真实外部消费者及互操作情况未核实 | 未复核 | 开放 | T-03 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F112 | Writer 硬写 descriptor 0；Reader 把 TIME mode 1 判 `packed.time.invalid`、REQ0 判 `packed.requirements.codec`。`writeBeatDescriptor/readBeatDescriptor` 全仓 0 调用者；GridDelta codec 仅测试调用 | `packed_chart_tables_sections.cpp:55-57,489-490`；`decoder_tables.cpp:195-197`；`decoder_model.cpp:340-344`<br>原证据状态：已修（文档：删除 mode 1 与 Writer 选择段） | 已核实 | 已处置 | T-03 | 依据：原字段与 §22 历史处置；复核者：Codex-G02（2026-11-23）；复核证据：[Packed 合同](../../../formats/PACKED_CHART_FORMAT.md)（当前正文与 ca5 已提交对应段）；理由：已按仅 mode 0 的已实现接受集收敛文档；不承诺实现 GridDelta 或 mode 1；决定依据：§22 既有文档处置与原字段记录；实际验证：当前/ca5 mode 0 表与非 0 拒绝段核对通过 |
| RR-F113 | v1/v2/v3 生产 Reader 额外拒绝 `|分子| > 1e12`、`分母 > 1e9`（`chart.beat.limit_exceeded`）；`chart.beat.out_of_range` 来自 v4 reader 与 `rational_beat.cpp` 算术，不是这两个上限。两个上限已由 `guides/LIMITS.md:62-63` 生成登记，但未进格式合同 | `limits.hpp:58-59`；`canonical_chart_loader_primitives.cpp:325-330`；`CHART_FORMAT.md:159-167`<br>原证据状态：已修（2026-11-22 复核：已写入 CHART_FORMAT 的 Beat 上限段；改引具名常量归 T-03） | 已核实 | 已修 | T-03 | 依据：原字段与 §22 历史处置；复核者：Codex-G02（2026-11-23）；复核证据：[Chart 格式](../../../formats/CHART_FORMAT.md)、[生成限额](../../../guides/LIMITS.md)（当前正文与 ca5 已提交对应段）；理由：原 Beat 上限及稳定码已进格式合同；T-03 的具名常量回指仍按任务执行；决定依据：§22 既有文档处置与原字段记录；实际验证：当前/ca5 Beat 上限段断言和 check_docs_facts.limits_is_current() 通过；改动修订：ca5cbb9121b015ae0b3b5d681610ca94dac78184（包含原文档订正的已提交快照） |
| RR-F114 | 原合同对 candidate playback encoding 的范围不一致。owner 已接受按具名 profile 对齐：静态 profile 必须为 `packed-chart`，Behavior profile 必须为 `source-json`；普通 CXC v1 默认入口继续沿用原路径。 | [CXC](../../../formats/CXC_FORMAT.md)；[Chart Entry §7](../../../formats/CHART_ENTRY_V1_FORMAT.md#7-behavior-source-candidate-profile)；`cxc_candidate.cpp`<br>原证据状态：两份合同已对齐并移除冲突提示；实现未变；本轮未重跑候选播放验收 | 未复核 | 开放 | T-03 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F115 | `cxtImports`/`cxtInstances` 全仓 0 命中；§6 写成合同未标未实现；§12:690 的“Chart invocation”同样过度声明。展开只由工具显式传参触发 | `CXT_V2_FORMAT.md:317,323,326,690`<br>原证据状态：已修（文档：§6 移入 proposals，§12 收窄） | 已核实 | 已处置 | T-07（文档状态）；T-11/T-13（后继能力按原卡另验） | 依据：原字段与 §22 历史处置；复核者：Codex-G02（2026-11-23）；复核证据：[CXT v2](../../../formats/CXT_V2_FORMAT.md)（当前正文与 ca5 已提交对应段）；理由：当前 §6 及 Chart invocation 明确标为候选设计、未实现；原“移入 proposals”说法保留但不作为本次动作描述；后继能力按 T-11/T-13 验收；决定依据：§22 既有文档处置与原字段记录；实际验证：当前/ca5 两处未实现状态断言通过 |
| RR-F116 | 文档声明 16 码中 2 个零产出（`resource_closure_invalid` 只有异命名空间的 `playback.candidate.*`）；实现另发 **12 个**未登记码（原文写 5 个，计数订正）。文档写“minimum stable set”，额外码本身不违约 | `CHART_ENTRY_V1_FORMAT.md:126-145`；`cxc_candidate.cpp`<br>原证据状态：已修（文档：删除 2 个零产出码） | 已核实 | 已修 | T-03（诊断合同）；T-09（核旧处置） | 依据：原字段与 §22 历史处置；复核者：Codex-G02（2026-11-23）；复核证据：[Chart Entry](../../../formats/CHART_ENTRY_V1_FORMAT.md)（当前正文与 ca5 已提交对应段）；理由：minimum stable set 已删无产出条目；本轮只恢复诊断表订正，不声称新互操作测试通过；决定依据：§22 既有文档处置与原字段记录；实际验证：当前/ca5 表核对及当前 14 条最低诊断行计数通过；改动修订：ca5cbb9121b015ae0b3b5d681610ca94dac78184（包含原文档订正的已提交快照） |

#### 2.3 SDK 与表现合同

<!-- cuexis-findings: definitions -->
| ID | 原始发现 | 原始依据 | 核验状态 | 处置状态 | 责任/任务 | 当前处置依据 |
| --- | --- | --- | --- | --- | --- | --- |
| RR-F121 | 文档主句已改为“公开 API 不接受版本入参”；残留：同文件 `:67` 仍要求“指定版本 digest”的 parity，安装 API 不可达；测试把 `src/frame_digest_version.cpp` 编进测试目标 | `frames-digests-and-timelines.md:56,67`；`frame_digest.hpp:18-20`；`frame_digest_internal.hpp:9-12`<br>原证据状态：已修（2026-11-22 复核：文档主句已改，测试重复编译归 RR-F214/T-04） | 已核实 | 开放 | T-04/T-07 | 依据：§22 与 §24 的分项承接；复核者：Codex-G02（2026-11-23）；复核证据：[帧与摘要](../../../api/frames-digests-and-timelines.md)；理由：公开签名及 parity 说明已核对，重复编译残余由 T-04 按 F214 验收；本轮没有该构建证据；核验边界：仅确认文档订正及残余归属，不证明产品修复 |
| RR-F122 | `materializePresentation` 无 `#if CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE` 门；production 构建的安装头暴露 RPA candidate 入口，错误码命名空间 `host.rpa.*` 也随之出现在 OFF 构建 | `include/cuexis/playback/playback_session.hpp:250-254`<br>原证据状态：已核实 | 未复核 | 开放 | T-08 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F123 | `builtInRendererProfileVersion` 只有写入点与测试读取，无任何生产读取点；`requiredRendererProfile` 与 adapter profile 不匹配时不构成失败，合同项静默通过 | `docs/formats/MATERIAL_SHADER.md` §9；`engine/render_opengl/src/open_gl_presentation.cpp:105`<br>原证据状态：已核实 | 未复核 | 开放 | T-09（核 profile 合同与实际读取，再确定实现责任） | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F124 | 实现额外要求 host-extension profile 必须出现在 `requiredHostExtensions` 中（文档无此规则）；同时 profile ID 走 `isUserIdentifier`，禁止 `.` 与 `cuexis` 前缀，与 §9 带点 ID 风格、`profileByteCount [1,64]` 三处互相矛盾 | `docs/formats/MATERIAL_SHADER.md` §5；`engine/playback/src/presentation_parse.cpp:404-414`<br>原证据状态：已核实 | 未复核 | 开放 | T-09（核 shader profile 接受集与既有裁定） | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F125 | 子集预检只覆盖大小/BOM-CR/`#version 450`/`#include`；geometry/tess/compute 等子集项落到 shaderc → `shader.compile.failed`。但 `playback.presentation.shader.subset_invalid` **并非零产出**：`shader_compiler_sources.cpp:128,133`、`presentation_reader.cpp:549,563`、`presentation_parse.cpp:198,204` | `MATERIAL_SHADER.md` §5、§12；`shader_compiler_sources.cpp:113-138`<br>原证据状态：部分修（文档不再承诺“完整预检”；实现仍只做部分预检） | 未复核 | 开放 | T-09（核文档订正后的实现残余及具名处置） | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F126 | `shaderHotReloadEnabled` 由四个 capability 标志（version==2 且三份源码）推导；`render_opengl` 只调 `prepareCandidate()`/`candidate()`，全目录 0 处 `activate()`/`active()`，candidate 永不换入 active | `MATERIAL_SHADER.md` §9；`playback_session_prepared.cpp:322-347`；`shader_pipeline_cache.cpp:148-153`<br>原证据状态：已修（文档：删除 warning/effective false 承诺） | 已核实 | 已处置 | T-07（文档边界）；T-09（确认未承诺热重载实现） | 依据：原字段与 §22 历史处置；复核者：Codex-G02（2026-11-23）；复核证据：[Shader 合同](../../../formats/MATERIAL_SHADER.md)（当前正文与 ca5 已提交对应段）；理由：合同已明确 capability 推导且没有 warning/生效保证；本项按文档收窄处置，热重载实现未被宣称完成；决定依据：§22 既有文档处置与原字段记录；实际验证：当前/ca5 热重载边界说明断言通过 |

#### 2.4 运行时、判定与失败语义

<!-- cuexis-findings: definitions -->
| ID | 原始发现 | 原始依据 | 核验状态 | 处置状态 | 责任/任务 | 当前处置依据 |
| --- | --- | --- | --- | --- | --- | --- |
| RR-F131 | 同一 canonical 身份在同一 Tick 再次入场，Spec 要求 `judgement.input.same_tick_collision` / `invalid_relation`，实现返回 `late.duplicate_queue_entry` / `late_policy_incomplete`；同 Tick 不同身份又被报成 `same_tick_collision`。两个稳定码被互换，且被 `tests/judgement/execution_profile_tests.cpp:276-285` 固化 | `docs/formats/GAMEPLAY_V2_SPEC.md:467-473`；`engine/judgement/src/input.cpp:182-184,191-205`<br>原证据状态：首次登记；入口与首错判断已在 [§15](#astra-九项设计修订) 纠正 | 未复核 | 开放 | T-03 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F132 | 实现把 `;gameplay.fold.finite.v1` 追加到 `judgementSemanticRevision`、`;grade.signed-interval.v1` 追加到 `factSemanticRevision`；Spec 规定前者应进 ruleset/session identity，后者写作 `signed-interval/1`（拼写不同） | `gameplay-v2-execution-types.md:77-85`；`execution_kernel_prepare.cpp:61,77,92-93`<br>原证据状态：已核实（补充第一条位置偏差） | 未复核 | 开放 | T-03 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F133 | 旧 `chartTimeMs` 重载 `beat` 保持 0，含 v3 event/step track 的程序经该重载求值会按 Beat 0 采样。仅当全部事件 `startBeat>0` 时才是“基准值”；`startBeat≤0` 的事件会在 Beat 0 返回其值。头文件无限制声明、测试不覆盖 track | `CHART_FORMAT.md:483,496-497`；`behavior_system.cpp:174-177,210-238`<br>原证据状态：已核实（“一律基准值”收窄） | 未复核 | 开放 | T-07/T-12（旧采样限制说明与完整 sample 仓内迁移） | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F134 | `activateReplacement()` 只 close/open，不重置 `clockAnchorUs/presentedSampleUs/clockErrorUs`，也不推进 discontinuity；`presentedClockObservation()` 在 Playing 下直接返回旧样本，`min(*clockAnchorUs, observedAnchor)` 让旧锚持续胜出 | `execution-coordination.md:39-44`；`sdl_audio_transport.cpp:490-518`<br>原证据状态：已核实（补充未推进 discontinuity） | 未复核 | 开放 | T-14 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F135 | 三种互斥处置并存；`bad_alloc` 被冒充为 `host.rpa.capacity_exceeded`（与预留故障槽同码，仅 message 不同），或被吞成成功 | `frame_observation.cpp:72,96,124,288,420,524` 对比 `:335,426,447` 对比 `:550,598,613`<br>原证据状态：已核实（补 420/524；同码问题更严重） | 未复核 | 开放 | T-06 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F136 | candidate 执行路径在 `PlayerController` 之前分流，完全绕过 `PlayerCommand` 命令表与固定事务顺序；`PLAYER_APPLICATION.md:30,213` 的"所有控制面产生同一个 PlayerCommand"只对中立路径成立 | `app/player/src/player_app.cpp:177-179`<br>原证据状态：已核实 | 未复核 | 开放 | T-15 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |

#### 2.5 主机构建面与文档覆盖

<!-- cuexis-findings: definitions -->
| ID | 原始发现 | 原始依据 | 核验状态 | 处置状态 | 责任/任务 | 当前处置依据 |
| --- | --- | --- | --- | --- | --- | --- |
| RR-F141 | 源文件清单只列 3 个；实际另有 4 个基础 .cpp + 条件编译的 `player_execution.cpp` + 5 个 `execution_host*.cpp`。选项表 6 项 vs 19 个 flag。`:124` 的“重复都返回 `player.arguments.*`”不成立：4 个布尔选项无重复检查，`:53/:93/:125` 返回 `player.candidate.disabled` | `PLAYER_APPLICATION.md:83-89,115-122,124`<br>原证据状态：已修（文档：选项表 19 项、源文件 13 个） | 已核实 | 已修 | T-07 | 依据：原字段与 §22 历史处置；复核者：Codex-G02（2026-11-23）；复核证据：[Player 应用](../../../architecture/PLAYER_APPLICATION.md)、[选项生成页](../../../guides/PLAYER_OPTIONS.md)（当前正文与 ca5 已提交对应段）；理由：基础与候选源文件分列，选项回链生成页；只恢复说明订正；决定依据：§22 既有文档处置与原字段记录；实际验证：当前/ca5 文件与选项段断言及 check_docs_facts.player_options_are_current() 通过；改动修订：ca5cbb9121b015ae0b3b5d681610ca94dac78184（包含原文档订正的已提交快照） |
| RR-F142 | 该 `error()` 辅助的 9 类失败统一为 `player.execution.test_only`；文件另有 7 个 `player.execution.*` 码，`:632` 在 fault 自带码时透传 | `player_execution.cpp:36-38` 及 `:110,115,120,139,608,630,632,642`<br>原证据状态：已核实（“统一”限定为这 9 类） | 未复核 | 开放 | T-15 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F143 | `check_docs.py` 在 docs 有 251 处引用，“零引用”不成立；真正零/极少引用的是 `check_pe_export_gate.py`（仅 findings）与 `gnu_asset_mirror.py`。worker 逐字节 `static_cast<wchar_t>`，任何非 ASCII narrow 路径都会 mojibake | `check_docs.py`、`check_pe_export_gate.py`、`gnu_asset_mirror.py`；`worker_process.cpp:22-31,49,76,83`<br>原证据状态：已核实（“零引用”收窄） | 未复核 | 开放 | T-16 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |

### 3. P2（登记但不阻塞本阶段退出）

<!-- cuexis-findings: definitions -->
| ID | 原始发现 | 原始依据 | 核验状态 | 处置状态 | 责任/任务 | 当前处置依据 |
| --- | --- | --- | --- | --- | --- | --- |
| RR-F201 | 属性求值顺序漏掉 `GameplayOverride`（代码在 Animation 与 HostOverride 之间插入该层；`ANIMATION_MIXING.md` 正确） | `docs/architecture/OVERVIEW.md:97-106`<br>原证据状态：已核实 | 未复核 | 开放 | T-07 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F202 | 文档 7 态机在代码中不存在；`RuntimeSession` 无 state 枚举，`onTimeDiscontinuity` 全仓 0 命中，`ResourceState::Reloading` 从未赋值。但 `timeDiscontinuityId` 存在并在 evaluation 比较；`PlaybackSession::SessionState` 只有 4 态且无 Preparing/Reloading/Closing 迁移 | `RUNTIME_SESSION.md:89-94`；`runtime_frame.hpp:10`；`runtime_session_evaluation.cpp:666`<br>原证据状态：已修（文档：删除 7 态机与回调描述） | 已核实 | 已修 | T-07 | 依据：原字段与 §22 历史处置；复核者：Codex-G02（2026-11-23）；复核证据：[Runtime 会话](../../../architecture/RUNTIME_SESSION.md)（当前正文与 ca5 已提交对应段）；理由：状态段已明确无持久枚举，旧回调不存在；F203 后续已接受修订仍独立待实现；决定依据：§22 既有文档处置与原字段记录；实际验证：当前/ca5 状态与 discontinuity 段断言通过；改动修订：ca5cbb9121b015ae0b3b5d681610ca94dac78184（包含原文档订正的已提交快照） |
| RR-F203 | `acquireOverride` 只校验线程/reentry/world/kind/空 writes/对象/mask，不校验 `PropertyValue` 值域；应用点在 `runtime_session.cpp:404-431`，失败即返回且 token 仍在 `evaluation_->hostOverrides`；只能靠 `releaseOverride` 恢复，错误不指出 token | `runtime_session_overrides.cpp:41-102`；`runtime_session.cpp:404-431`<br>原证据状态：已核实（改为“无自动回滚、错误不定位 token”） | 未复核 | 开放 | T-17 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F204 | 层域优先级是 Initial<Behavior<Animation<Gameplay<Host<Preview，枚举值只有 Gameplay 错位，不是“相反”；`seenLayers` 仅作同层重复写检测与 layerValue，不参与排序；实际顺序靠 `runtime_session.cpp:372-431` 显式调用序 | `property.hpp:63-70`；`GAMEPLAY_V2_SPEC.md:31`；`property_helpers.hpp:31-33`<br>原证据状态：已核实（“相反”改为“Gameplay 枚举值与优先级位置不符”） | 未复核 | 开放 | T-07 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F205 | `add/append` 返回 bool 但未标 `[[nodiscard]]`；engine+app 135 处调用中 88 处裸丢弃。但截断有兜底：达上限插入 limit diagnostic 且 `limitReached()` 公开（20 处调用）。CODE_POLICY 引用有误 | `diagnostic.hpp:63,66`；`diagnostic.cpp:56-60,104-106`<br>原证据状态：已核实（删“无从得知”；修正引用） | 未复核 | 开放 | T-07 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F206 | 三处 `shared_ptr<X>{new X{...}}`，同文件已有 `make_shared<Data>`；但标准保证控制块分配失败会 delete 指针，不是“非异常安全”；真实问题是多一次控制块分配，且构造函数私有使 make_shared 不能直接替换 | `content_provider.cpp:144-145,213,241`、`:197`<br>原证据状态：已核实（改为“多一次分配”，删除异常安全断言） | 未复核 | 开放 | T-07 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F207 | `request()` 394 行；五类 acquire 分支 `:218-307` 与五段 fallback `:332-465` 除 handle 类型外近乎逐字重复；小组件 `requestMesh` 等只是薄包装 | `resource_scope.cpp:79-472`<br>原证据状态：已核实（补重复行数） | 未复核 | 开放 | T-06 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F208 | capability 预检先于 chart 预算（compile 内 `chart_runtime.cpp:546-552`）与 presentation 预算；presentation 解析后还有第二次 capability 检查，文档阶段顺序未分列 | `playback_session_prepare.cpp:270-279,292-297,352-358`；`playback_session_prepare_stages.cpp:443-467`<br>原证据状态：已核实（位置与分列说明） | 未复核 | 开放 | T-04 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F209 | `sessionState` 只有 Running/Empty/Ready 写入点，无 Failed 赋值；文档赋予“不可继续”语义；参考宿主存在等待 Failed 的死分支 | `playback_session.hpp:170-175`；`playback_session.cpp:165,347`；`docs/api/playback-session.md:55-62`<br>原证据状态：已修（文档：Failed 标为从不进入；宿主死分支待代码修） | 已核实 | 开放 | T-07/T-08 | 依据：§22 与 §24 的分项承接；复核者：Codex-G02（2026-11-23）；复核证据：[Playback 会话](../../../api/playback-session.md)；理由：Failed 保留枚举且当前从不进入的文档已核对；宿主死分支仍由 T-07/T-08 承接，本轮没有实现验收；核验边界：仅确认文档订正及残余归属，不证明产品修复 |
| RR-F210 | `ShaderStage`、`shaderGlslEs300` 全仓仅声明处命中；`playback.presentation.shader.stage_unsupported` 在 C++ 0 命中，仅 `MATERIAL_SHADER.md:606` 说明未产出 | `presentation.hpp:41-44,208`<br>原证据状态：已核实 | 未复核 | 开放 | T-07/T-08 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F211 | `snapshot()` 的 `for(;;)` 无尝试上限，同类实现用 8 次；但 `publish()` 为 noexcept 且奇偶 sequence 间无可抛调用，正常路径不会永久停在奇序号，风险限于发布线程被永久挂起 | `sdl_audio_transport.cpp:371-399`、`:425-426`；`audio_transport.cpp:70-71`<br>原证据状态：已核实（收窄自旋条件） | 未复核 | 开放 | T-14 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F212 | validator 拒绝 Empty，但 transport unload 会发布 Empty 且 snapshot 原样返回；公共头未写清二者关系。`HostClock::snapshot` 的 thread_local 缓存以 `this` 为键，地址复用且 8 次全失败时返回旧对象 | `audio_transport.cpp:29-31,59-89`；`sdl_audio_transport.cpp:291,297,392-393`<br>原证据状态：已核实（收窄“接口声明不得 Empty”） | 未复核 | 开放 | T-14 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F213 | 两者非 owner 线程析构都 `std::terminate()`；但窗口公共头 `sdl_window.hpp:66-75,100-105` 已明确记录该约束，audio 子系统头没有 | `sdl_window.cpp:281-287`；`sdl_audio.cpp:78-82`<br>原证据状态：已核实（按类型拆分：platform 已记录、audio 缺说明） | 未复核 | 开放 | T-14 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F214 | 测试把 `frame_digest_version.cpp`、`presentation_extraction.cpp` 编进测试目标又链 playback；两文件本就在库源中，当前无宏差异故行为未分叉，属测试卫生问题 | `tests/playback/CMakeLists.txt:25-26,57,117`<br>原证据状态：已核实（补“当前无宏差异”） | 未复核 | 开放 | T-04 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F215 | 测试建窗并请求全屏，仅靠 `SDL_VIDEODRIVER=dummy` 缓解，无 CTest LABELS；规则出处是 `BUILDING.md:444`“算法单元测试不得依赖窗口”，平台适配器测试是否适用存疑 | `tests/platform/sdl_window_tests.cpp:39-167`；`tests/platform/CMakeLists.txt:12-14`；`BUILDING.md:444`<br>原证据状态：已核实（修正引用） | 未复核 | 开放 | T-15 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F216 | observer 门禁在 `CUEXIS_LIBRARY_TYPE` 定义前执行，全新 configure 未显式传参时必然 FATAL；已有 build dir 因 CMakeCache 有值可通过；官方 profile 脚本显式传参规避 | `CMakeLists.txt:92-97,155`；`CuexisLibraryType.cmake:1`<br>原证据状态：已核实（补“仅全新 configure 必败”） | 未复核 | 开放 | T-04 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F217 | 默认构建下 `cuexis_shader_cache` 无条件存在，而 shader 测试只在 `if(TARGET cuexis_shader)` 且 SHADER_TOOLS 时编入 → 默认构建遗留零测试 target | `engine/CMakeLists.txt:18-20`；`engine/shader/CMakeLists.txt:31-34`；`tests/CMakeLists.txt:30-31`<br>原证据状态：已核实 | 未复核 | 开放 | T-04 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F218 | 只有 custom target / INTERFACE，均未 `add_subdirectory`，也不在 active 清单；一旦接入会触发 target allowlist FATAL | `app/studio/CMakeLists.txt:1-2`；`engine/particles/CMakeLists.txt:1-3`；`VerifyArchitecture.cmake:579-606`<br>原证据状态：已核实 | 未复核 | 开放 | T-07 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F219 | core 规则只匹配 `SDL|glad|GL/`，不覆盖 `<windows.h>/<intrin.h>/<cpuid.h>`；引用出处是 `MODULE_BOUNDARIES.md:19`（不是 AGENTS）；core 已有 intrin/cpuid 先例，当前无 windows.h | `VerifyArchitecture.cmake:117`；`MODULE_BOUNDARIES.md:19`；`sha256.cpp:11,13`<br>原证据状态：已核实（修正引用与“可漏入”） | 未复核 | 开放 | T-10/T-12 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F220 | 两处 allowlist 条目宽于直接 LINK_LIBRARIES（shader→core、playback_tests→assets），但都在传递闭包内；门禁仍拒绝任何未预置的新依赖，属许可超集而非漏洞 | `CMakeLists.txt:1035-1044,1486-1497`<br>原证据状态：已核实（改“冗余但非漏洞”） | 未复核 | 开放 | T-04 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |

### 4. 源码拆分预算与超长文件
#### 4.1 预算事实

> **2026-11-17 owner 决定：取消文件拆分工作。** 本阶段转为架构减重重构（见
> [plan.md](plan.md)）；本节只作为事实记录保留，不再安排拆分批次。`.cpp ≤ 800`、`.hpp ≤ 500`
> 也不再作为门禁目标。

- 预算数值来自上一轮拆分（2026-11-03）的状态记录：`.cpp ≤ 800` 行、`.hpp ≤ 500` 行，当时"终检 0 个"。
  该表述现已不在任何现行文档中，只留在历史证据
  （`stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-05-pre-o1-evidence/raw/baseline/tracked.patch`
  与 `source-split-audit/index.md` 的引用里）。
- **没有任何自动门禁**检查该预算（`cmake/**`、`.github/workflows/**` 均无），也从未写进
  `docs/guides/CODE_POLICY.md`。规则实际只存在于提交说明。
- 现状终检（2026-11-17，`engine/`、`app/`、`tools/`、`tests/`）：
  `.hpp` 全部合规（最大 `engine/playback/include/cuexis/playback/playback_session.hpp`，488 行）；
  `.cpp` 唯一越限 `tests/pd/execution_memory_observer.cpp` = **917 行**。

#### 4.2 唯一越限文件

| 文件 | 行数 | 现状 | 拆分方向 |
| --- | --- | --- | --- |
| `tests/pd/execution_memory_observer.cpp` | 917 | 单一 TU 同时承担：分配账本（`Header`/`Structure`/`Snapshot` 计数）、语义根与 census（`recordSemanticPhase`/`recordSemanticCensus`/`release`）、checkpoint 连接、以及全局 `operator new/delete` 替换。经根 `CMakeLists.txt:214` 同时链入 `cuexis_pd_host_probe` 与 `cuexis_execution_device_probe` | 抽内部头承载共享状态（`inline` 变量保持单一实例），拆为"分配账本 + 全局 operator 替换"与"语义根/ census"两个 TU；全局 `new/delete` 必须仍只定义一处 |

注意：这是测试专用 interposition 单元，拆分风险高于普通源文件（ODR、单一 `operator new` 定义、
匿名 namespace 状态）。RR-4 必须先给出等价性说明再动。

#### 4.3 700+ 行与 600+ 行的候选深拆清单（非本阶段强制）

若 owner 决定把阈值下调到 600/700，以下为各模块报告给出的可分离单元（行数为当次统计）：

- `judgement`：`timebase.cpp` 799（有理/积分内核 vs 公共契约 API）、`execution_kernel_restore.cpp` 635（快照生成 vs 恢复校验）。
- `chart`：`chart_writer.cpp` 779、`chart_runtime.cpp` 717、`packed_semantic_identity.cpp` 685、
  `packed_chart_tables_sections.cpp` 657、`chart_migrator.cpp` 614、`cxt_v2_loader_parse_nodes.cpp` 677。
- `playback`：`playback_session_execution.cpp` 719、`playback_session.cpp` 630、`playback_session_gameplay.cpp` 623。
- `cxc`：`zip32_envelope_internal.cpp` 733、`cxc_package.cpp` 687、`cxc_candidate.cpp` 683。
- `runtime/world/assets`：`runtime_session_evaluation.cpp` 731、`runtime_session.cpp` 699、
  `asset_database.cpp` 694、`property_resolver.cpp` 679（`build()` 单函数 330 行）、`resource_scope.cpp` 601。
- `shader/render`：`shader_cache.cpp` 778、`open_gl_backend.cpp` 715。
- `audio/animation`：`animation_mixer.cpp` 687、`audio_import.cpp` 656。
- `app`：`session_worker_dispatch.cpp` 677（一个 `std::visit` + ~50 个 `if constexpr` 分支）、
  `player_execution.cpp` 645、`frame_observation.cpp` 616。
- `tools/tests`：`check_docs.py` 881、`gameplay_author.cpp` 695、以及 `tests/` 下 11 个 700 行附近的用例文件。

### 5. 复核方法与已知边界

- 方法：10 个并行无上下文子代理按模块只读审查 + owner 抽查 8 处关键结论（本册"已核实"项）。
- 未覆盖：Release/候选预设构建、完整 CTest 矩阵、跨平台与外部消费者复验、GPU/设备路径实测。
  本册任何"实现如此"的判断都只对当次工作树文本成立。
- 已知偏差：不同计数方式对 `execution_memory_observer.cpp` 给出 910/917 两个数字；
  本册采用 `917`（`Get-Content .Count`、Python 行数、LF 计数三者一致），
  910 来自 `Measure-Object -Line` 对空行的处理。
- 本册不重复 `docs/stage_reports/stages/stage-07/90-shared/source-split-audit/` 已记录的
  五项安全网缺口；那是拆分门禁本身的审计，与本册的文档-实现审查互补。

### 6. 架构层发现（2026-11-17 研究）

8 个无上下文研究子代理的结论已整理为
[架构与实现研究](../../../stage_reports/stages/stage-07-rr/verification/2026-11-17-architecture-research.md)；
本节只登记条目，细节与证据见该报告对应小节。

<!-- cuexis-findings: definitions -->
| ID | 原始发现 | 原始依据 | 核验状态 | 处置状态 | 责任/任务 | 当前处置依据 |
| --- | --- | --- | --- | --- | --- | --- |
| RR-F301 | 候选支线默认 OFF，却按第二产品线全量编译与挂接（约 27k 行：engine 候选面 4.4k + execution host 9.6k + 相关测试大头） | 首次证据定位：未知；原证据状态：已核实（规模与宏计数抽查） | 未复核 | 开放 | T-04 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F302 | `cuexis_chart` 无条件编译 packed/cxt_v2/candidate，40.9% 默认不可达 | 首次证据定位：未知；原证据状态：已核实（`engine/chart/CMakeLists.txt:6-60`） | 未复核 | 开放 | T-04 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F303 | 模块 4,821 非空行 / 5,009 物理行，无条件编译；产品目标 0 条链接边；playback 边只在 candidate 块；默认开发者工具 `gameplay_author` 有 1 条 PUBLIC 边 | `engine/gameplay_packed`；`engine/CMakeLists.txt:25`；`engine/playback/CMakeLists.txt:47-54`<br>原证据状态：已核实（补注行数口径与“产品 0 边、默认工具 1 边”） | 未复核 | 开放 | T-10/T-12/T-13 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F304 | 只有 `CxtV2Loader::expand`、无 v2 Writer；playback 对 cxt_v2 0 引用；文档以 candidate 登记并自述 reader/expander、未接入默认 Playback——“冒充 active”不成立 | `cxt_v2_loader.hpp:49-52`；`docs/MAP.md:126`；`docs/formats/README.md:51`<br>原证据状态：已核实（改写“按合同登记”的暗示） | 未复核 | 开放 | T-13 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F305 | 三层各一份的只有 5 项（prepared、commit、active/candidate、诊断投影、线程守卫）；generation、matchingStep、token、presentation revision 仅两层重复（matchingStep 归一化后 IDENTICAL） | `prepared/commit/generation/token/presentation revision` 对照表见报告 §3.3<br>原证据状态：已核实（拆为 5 项三层 / 4 项两层） | 未复核 | 开放 | T-06/T-13 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F306 | 公共类被一个宏切成两半：`PlaybackSession` 在同一安装头里按 `CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE` 提供两套类定义（OFF 39 条方法声明 / ON 90 条，其中 49 个方法名仅 ON 存在，两侧零重名），与 ADR 0042 S6-D01「开关不改变公开类定义或布局」直接冲突 | 首次证据定位：未知；原证据状态：已核实（`playback_session.hpp` 488 行；宏块 24-28/279-381/465-475；逐行计数） | 未复核 | 开放 | T-08/T-09 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F307 | 非 RPA 扣两次（outputCorrection + `TimingMap.offsetMs`），RPA 只扣 A 与 inputCompensation；`offsetMs` 与 A 无等式校验。重合条件是 `offsetMs + consumedCorrection == A`，不是全为 0 | `player_control_frame_loop.cpp:95-103`；`runtime_timeline.cpp:43`；`player_execution.cpp:116`；`realtime_clock.cpp:161-171`<br>原证据状态：已核实（重合条件订正） | 未复核 | 开放 | T-14 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F308 | 4 个时间实现；Timebase 单列则为 5 个；两种舍入（llround vs half-even）；两份反向 seek；`simulationDeltaTimeMs` 生产侧只校验/哈希/CSV/置 0 | `player_control_frame_loop.cpp:32`；`realtime_clock.hpp:68`；`runtime_timeline.hpp:14`；`timing_map.hpp:41`；`timebase.hpp:97`<br>原证据状态：已核实（补“4 或 5 取决于是否单列 Timebase”） | 未复核 | 开放 | T-10/T-12 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F309 | legacy 渲染链只剩启动清屏一个调用点、零测试；`renderPresentationFrame` 绕过帧校验 | 首次证据定位：未知；原证据状态：已核实（`player_app.cpp:284`、`player_smoke.cpp:339,346`） | 未复核 | 开放 | T-06 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F310 | 公共面 47/90 方法仅 candidate 构建存在；约 96 个类型无宿主消费者；`ShaderStage` 仅声明处命中 | 首次证据定位：未知；原证据状态：已核实（抽查 3 项） | 未复核 | 开放 | T-08 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F311 | 70.3% 可复现（当前同口径 70.8%）；基线时 5/22 合同含错，样例已在本轮订正；22 篇清单未留存，当前非计划非 ADR 合同为 43 篇，不得再按 5/22 描述当前状态 | `2026-11-17-architecture-evidence.md:261-273`；`2026-11-17-documentation-refactor.md:43-49`<br>原证据状态：已核实（改为历史基线 + 已订正） | 未复核 | 开放 | T-02/T-09 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F312 | 代码 banner 注释失真 33%（抽样 12 条推翻 4 条）；`world.hpp:3` "thread-safe" 与实现相反 | 首次证据定位：未知；原证据状态：已核实（`world` 内 mutex/atomic 命中 0） | 未复核 | 开放 | T-07 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F313 | 架构门禁不查 `cuexis_internal` 越界、通用 `src/` include、安装集合本身；文档事实已有生成页新鲜度与版本漂移（仅警告），合同正文引用仍不校验 | `cmake/VerifyArchitecture.cmake:109-563`；`check_docs_facts.py:99-144`；`check_docs.py:531-560`<br>原证据状态：已核实（第四条限定到具体门禁） | 未复核 | 开放 | T-08/T-14 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F314 | chart_candidate 无条件链接 playback（用 PlaybackSession），但不直编 src；gameplay_author 的链接与直编 `engine/playback/src/gameplay_entry.cpp` 仅在 `CUEXIS_ENABLE_CHART_V5_CANDIDATE` 内 | `tools/chart/chart_candidate/CMakeLists.txt:4-6`；`tools/gameplay/gameplay_assembler/CMakeLists.txt:9-14`<br>原证据状态：已核实（拆分两件事并加开关限定） | 未复核 | 开放 | T-10/T-12 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F315 | 死重量成立：particles（无源码、未接入）、debug（仅测试链接）、gameplay（9 行 tags，runtime 仍 PRIVATE 依赖）、app/studio（仅 custom target）。不成立：content 是 canonical 公共模块（进 `CUEXIS_PUBLIC_EXPORT_TARGETS`），不是门面；player_support（1,215 行）只 app+tests 用，属职责错位而非死码 | 各模块 CMake 与消费者统计<br>原证据状态：已核实（删 content 表述；player_support 改为“应下沉 app”） | 未复核 | 开放 | T-09（核原处置及缺失证据，按实际结果具名交接） | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F316 | **应用层对 engine 无纪律（双向）**：① `app/` 直接 include `engine/playback/src/replay_identity.hpp` 6 处、`cuexis_internal/` 至少 11 处；② `cuexis_execution_host_support` 作为 app/ 库 PUBLIC 依赖 playback 并复制会话逻辑（app/execution_host 6,835 行 + app/player/src 7,794 行）；③ engine/ 内反向存放应用职责 `player_support`（唯一消费者是 app/player 与其测试）；④ 架构门禁对 app 只做 nlohmann/GL 全局扫描与 6 个 player 文件的 adapter 禁止，**不禁止** src/internal include | 首次证据定位：未知；原证据状态：已核实（include 命中与 CMakeLists 链接关系逐条查证） | 未复核 | 开放 | T-08/T-14 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |

已推翻的子代理结论（避免按错情报动手）：`ChartTick` **不是**死类型（34 处使用）；
`acquireHostOverride` 并非全仓 0 引用（5 个测试文件在用，但宿主消费者为 0）。

### 7. 假保证与假实现（2026-11-17 专班扫描）

7 个无上下文子代理按模块扫描「承诺 vs 实现」，合并去重后 48 条候选，其中 26 条越过 owner 复核。
完整证据、未复核清单与阴性对照见[扫描报告](../../../stage_reports/stages/stage-07-rr/verification/2026-11-17-false-guarantees-scan.md)。

#### 7.1 已复核（owner 亲自 grep/读码确认）

<!-- cuexis-findings: definitions -->
| ID | 原始发现 | 原始依据 | 核验状态 | 处置状态 | 责任/任务 | 当前处置依据 |
| --- | --- | --- | --- | --- | --- | --- |
| RR-F401 | 保存把 `templates`/`timing.stops`/`tempoEvents` 写成空、`version` 恒 3，载入再保存丢数据 | `engine/chart/src/chart_writer.cpp:676-685`<br>原判定：假实现（丢数据） | 未复核 | 开放 | T-16 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F402 | `writeV4/writeCanonical(limits)` 的 `limits` 是死参数 | `engine/chart/src/chart_writer.cpp:693,699`<br>原判定：半实现 | 未复核 | 开放 | T-16 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F403 | Chart 侧无 `min(requested, frozen)` 钳制，只有 Packed 有 | `engine/chart/include/cuexis/chart/limits.hpp:3-6`<br>原判定：假保证 | 未复核 | 开放 | T-16 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F404 | `validateAgainstSchema` 诊断后恒返回 Ok | `engine/json_support/src/schema.cpp:50-51`<br>原判定：易误用（已自述） | 未复核 | 开放 | T-07 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F405 | `materializePresentation` 在 candidate 宏外声明并导出 `host.rpa.*`，与 ADR 0042 冲突（这是**反方向**泄漏：candidate 入口漏进 OFF 面；类级切分见 RR-F306） | `engine/playback/include/.../playback_session.hpp:250-254`<br>原判定：假保证 | 未复核 | 开放 | T-08/T-09 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F406 | `FeedbackStatus` 唯一写入恒 `created` | `engine/playback/src/playback_session_execution.cpp:484`<br>原判定：假实现 | 未复核 | 开放 | T-07/T-14 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F407 | `executionStepEffects()` 只填 4/9 字段 | `engine/playback/src/playback_session_execution.cpp:542-548`<br>原判定：半实现 | 未复核 | 开放 | T-07/T-14 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F408 | `shaderGlslEs300`/`builtInRendererProfileVersion` 零消费 | `engine/playback/include/.../presentation.hpp:203,208`<br>原判定：死字段 | 未复核 | 开放 | T-07/T-08 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F409 | 每条记录 `make_shared<RuntimeState>(*active)` 深拷贝，与 O(1) 注释相反 | `engine/judgement/src/execution_kernel_archive.cpp:332`<br>原判定：假保证 | 未复核 | 开放 | T-07 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F410 | `KernelSessionState::Created/Configured` 无写入/读取 | `engine/judgement/include/.../kernel_types.hpp:202`<br>原判定：死实现 | 未复核 | 开放 | T-10/T-12 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F411 | 注释“永远 false”，实际 `prepared=true` 已写 | `engine/judgement/include/.../judgement_session.hpp:143-147`<br>原判定：过期陈述 | 未复核 | 开放 | T-07 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F412 | `SessionState::Failed` 全模块无写入 | `engine/playback/include/.../playback_session.hpp:170-175`<br>原判定：死实现 | 未复核 | 开放 | T-07/T-08 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F413 | “thread-safe”与 0 mutex/atomic 矛盾 | `engine/world/include/cuexis/world/world.hpp:3`<br>原判定：假保证 | 未复核 | 开放 | T-07 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F414 | `transformCacheValid_` 由调用方维护，World 层机制缺失 | `engine/runtime/include/.../runtime_session.hpp:274`<br>原判定：假保证 | 未复核 | 开放 | T-07 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F415 | `mutationEpoch` 只写不读 | `engine/core/internal/.../presentation_revision.hpp:15`<br>原判定：半实现 | 未复核 | 开放 | T-06 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F416 | `commit/rollback` 生产零调用、`rollback` 空转 | `engine/world/src/property_resolver.cpp:494,531`<br>原判定：死实现 | 未复核 | 开放 | T-07 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F417 | 析构排空是无终止条件的重试循环 | `app/player/src/execution_host.cpp:71-78`<br>原判定：假保证 | 未复核 | 开放 | T-14 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F418 | `--frame-stats` 在执行线被静默忽略 | `app/player/src/player_app.cpp:177-180`<br>原判定：半实现 | 未复核 | 开放 | T-15 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F419 | 首帧 `render_us` 恒 0（钩子早于赋值） | `app/player/src/player_control_frame_loop.cpp:374,384`<br>原判定：假实现 | 未复核 | 开放 | T-06 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F420 | `exactLossCount` 是字面量 `false`。owner 2026-11-22 说明设计意图：新会话如实报告帧观测记录的缺失，并标明缺失计数是否精确，避免把不完整观测当成完整证据；按 D 并入新设计，归 T-14。旧路径恒 false 只是保守下界 | `app/execution_host/frame_observation.cpp:240`<br>原判定：成立（未实现，归 T-14） | 未复核 | 开放 | T-14 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F421 | `capabilities()` 非主线程返回默认零能力集 | `engine/render_opengl/src/open_gl_presentation_frame.cpp:226-241`<br>原判定：假保证 | 未复核 | 开放 | T-06 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F422 | “worker 线程编译”实为 `thread` + 立即 `join()` | `engine/shader/src/shader_pipeline_cache.cpp:32-39`<br>原判定：假保证 | 未复核 | 开放 | T-07 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F423 | 文档写三参 digest，公开 API 两参 | `docs/api/frames-digests-and-timelines.md:62`<br>原判定：假保证（已修） | 已核实 | 已修 | T-07 | 依据：原字段与 §22 历史处置；复核者：Codex-G02（2026-11-23）；复核证据：[帧与摘要](../../../api/frames-digests-and-timelines.md)（当前正文与 ca5 已提交对应段）；理由：公开双输入与内部版本选择已区分；不把 F121 重复编译残余一并关闭；决定依据：§22 既有文档处置与原字段记录；实际验证：当前/ca5 digest 段断言及 check_docs_facts.frame_digest_is_current() 通过；改动修订：ca5cbb9121b015ae0b3b5d681610ca94dac78184（包含原文档订正的已提交快照） |
| RR-F424 | 写当前 SDK 0.8.0，实际 0.9.1 | `docs/api/diagnostics-identity-and-compatibility.md:28,70`、`GAMEPLAY_V2_ABI.md:75`<br>原判定：过期陈述（已修） | 已核实 | 已修 | T-02 | 依据：原字段与 §22 历史处置；复核者：Codex-G02（2026-11-23）；复核证据：[诊断身份合同](../../../api/diagnostics-identity-and-compatibility.md)、[V2 ABI](../../../api/GAMEPLAY_V2_ABI.md)（当前正文与 ca5 已提交对应段）；理由：当前 SDK 文字订正同 F103；没有另造产品证据；决定依据：§22 既有文档处置与原字段记录；实际验证：当前/ca5 版本段核对与 check_docs_facts.scan 定向扫描通过；改动修订：ca5cbb9121b015ae0b3b5d681610ca94dac78184（包含原文档订正的已提交快照） |
| RR-F425 | `startRecording/loadReplay` 全仓 0 命中却被写成现行入口 | `docs/formats/TIMING_MODEL.md:52`<br>原判定：过期陈述（已修） | 已核实 | 已修 | T-07 | 依据：原字段与 §22 历史处置；复核者：Codex-G02（2026-11-23）；复核证据：[时间模型](../../../formats/TIMING_MODEL.md)（当前正文与 ca5 已提交对应段）；理由：startRecording/loadReplay 已标旧概念名并明确无同名入口，不宣称新 API 已实现；决定依据：§22 既有文档处置与原字段记录；实际验证：当前/ca5 旧概念与实际入口说明断言通过；改动修订：ca5cbb9121b015ae0b3b5d681610ca94dac78184（包含原文档订正的已提交快照） |
| RR-F426 | 只匹配 `std::size_t`，漏两个 `std::int64_t` Beat 上限 | `docs/guides/LIMITS.md` 生成器<br>原判定：生成缺陷（已修） | 未复核 | 开放 | T-18 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |

#### 7.2 未复核（子代理声称已读码，动代码前必须复核）

<!-- cuexis-findings: definitions -->
| ID | 原始发现 | 原始依据 | 核验状态 | 处置状态 | 责任/任务 | 当前处置依据 |
| --- | --- | --- | --- | --- | --- | --- |
| RR-F427 | `detachCandidate` 是唯一没有 owner 校验的实现；唯一入站路径是公开值类型 `PreparedPresentation` 的析构/move-assign，跨线程时静默改 `pending/outstanding`，而 `activate/discard` 同场景会 terminate | `presentation_renderer.cpp:521-536`<br>原证据状态：已核实（潜在，非已发生） | 未复核 | 开放 | T-06/T-14 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F428 | `rebuild()` 无 closed 守卫并置 `closed=false`；OpenGL 后端 close 后 context/window 已销毁，rebuild 仍返回成功，下一次 present 报 `render.opengl.backend_unavailable`；测试把该行为编码为预期 | `presentation_renderer.cpp:464-480`；`open_gl_presentation_frame.cpp:417-454`；`open_gl_backend.cpp:680,706-711`<br>原证据状态：已核实（两处假成功） | 未复核 | 开放 | T-06/T-14 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F429 | 注释承诺“非主线程泄漏日志”，实现两处静默 return；真正带告警的是 `:691-695`；两个入口都先做线程检查，故静默分支当前不可达，实质后果是错误线程 close 只返错、GPU 资源泄漏且无告警 | `open_gl_backend.cpp:663-670`；文件头 `:6`<br>原证据状态：已核实（注释与实现不符） | 未复核 | 开放 | T-07 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F430 | 8 次重试全失败时返回 thread-local 上次成功样本（陈旧）；首次调用返回 `Stopped/0` 的默认样本，不是 Empty；头文件只说“不分配不抛”，未声明 stale 回退 | `audio_transport.cpp:59-89`；`audio_transport.hpp:62-64`<br>原证据状态：已核实 | 未复核 | 开放 | T-14 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F431 | 动画条数预算超限只记 `chart.animation.generated_limit` 诊断、继续解析全部 clip，直到 `:185-188` 才整体拒绝；预算不约束工作量。行号订正为 132-155 | `chart_v4_loader.cpp:132-155`<br>原证据状态：已核实（行号订正） | 未复核 | 开放 | T-13 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F432 | assembler 用未钳制的 `maxPackedEntities` 前检，先全量插入，再由 `encode` 的 `min(requested, frozen)` 在 `:60-63` 拒绝；`effectiveLimits` 已存在但未调用；风险是放宽上限时内存/工作量不受 40,000 约束 | `candidate_assembler.cpp:32-38`；`packed_limits_internal.hpp:23-34`<br>原证据状态：已核实 | 未复核 | 开放 | T-13 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F433 | `discardedFields = {}` 是全仓唯一写点，从无 push_back/非空赋值；迁移报告恒为空数组，与 writer 丢 templates 互相印证 | `chart_migrator.cpp:216`；`chart_migrator.hpp:43`<br>原证据状态：已核实 | 未复核 | 开放 | T-16 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F434 | v1/v2 仅测试可达；`version_unsupported` 连测试都不覆盖、全仓无调用方可触发，是纯防御死分支 | `frame_digest.cpp:8-15`；`frame_digest_version.cpp:77-82`<br>原证据状态：部分成立（比“仅测试可达”更弱） | 未复核 | 开放 | T-04/T-07 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F435 | `:422` 无条件置 Ready；只有 GameplayOnly 候选路径能造出“Ready 且无 World”；RuntimeFrame 管线的 update/extractFrame/chartInfo/presentationManifest 全被 `playback.session.empty` 拒绝，但 gameplay tick 仍可用并把状态推进到 Running | `playback_session_prepare.cpp:422`；`playback_session_gameplay.cpp:180-185,318-322`<br>原证据状态：部分成立（“逐帧操作全被拒”限定为 World 帧管线） | 未复核 | 开放 | T-17 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F436 | read 本身有 `weak_ptr+epoch` 校验，但 `metadata()`/`fact()` 交出的借用（`PreparedIdentity&`、`core::Error*`、`RequirementIdentity*`）无任何类型/运行时强制，只能靠注释 | `publication.hpp:45-47`；`publication.cpp:14-38`<br>原证据状态：已核实（限定：只保护 read 本身） | 未复核 | 开放 | T-10/T-12 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F437 | 恰好 5 个 const 观察函数仅 `assertCurrent()`，Release 下 assert 被 NDEBUG 编译掉 → 跨线程静默 UB | `runtime_session_overrides.cpp:243,375,380,385,390`；`thread_checker.cpp:19-22`<br>原证据状态：已核实 | 未复核 | 开放 | T-07 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F438 | `noexcept` 函数内构造带堆存储的 `Error`，OOM 时 terminate；这是 math.hpp 全部 Result 入口的共同约定，不是单点 | `math.hpp:57,65-66,70,71-72,75`；`math.cpp:111-135`<br>原证据状态：已核实（面扩大到全部 Result 入口） | 未复核 | 开放 | T-07 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |

### 8. 外部审查交叉核对（2026-11-17）

来源：[外部审查报告（59 条）](../../../stage_reports/stages/stage-07-rr/verification/2026-11-17-external-findings-review.md)，
基线 `aac83e5e`（与本阶段一致），只读审阅，未运行构建。下表只登记**本轮独立核对过证据**的条目；
外部报告里未核对的条目保留原文，不得据其直接改代码。

#### 8.1 已有对应（外部 → 本登记册）

| 外部 | 本登记册 | 核对结果 |
| --- | --- | --- |
| C-02 | RR-F412、RR-F209 | 一致：`SessionState::Failed` 全模块无写入点 |
| C-03 | RR-F401 | 一致：`ChartWriter::write` 丢 timing/templates |
| D-02 | RR-F419 | 一致：`render_us` 恒 0 |
| D-03 | RR-F417 | 一致：close 超预算后析构自旋 |
| S-01 | RR-F305 | 部分：F305 记三层重复，外部补充三份手工字段清单的具体漏项（见 F508） |
| S-02 | RR-F306、RR-F405 | 一致 |
| S-03 | RR-F316 | 一致：应用层无纪律 |
| D-10 | RR-F313、RR-F316 | 一致：门禁 app 侧只覆盖 6 个文件 |
| DOC-01 | RR-F105 | 部分：`formats/README.md` 行已改，`PACKED_CHART_FORMAT.md:3` 仍写“未实现” |
| DOC-05 | RR-F103、RR-F424 | 已修 |
| DOC-06 | RR-F121、RR-F423 | 已修 |
| DOC-08 | 文档治理批次 | 部分处置（MAP/元数据/证据头） |
| DOC-09 / A-09 | **本轮已处置** | ADR 0027「依赖方向」已登记 Judgement 边界 |

#### 8.2 本轮新核实并补登记

<!-- cuexis-findings: definitions -->
| ID | 原始发现 | 原始依据 | 核验状态 | 处置状态 | 责任/任务 | 当前处置依据 |
| --- | --- | --- | --- | --- | --- | --- |
| RR-F501 | GameplayOnly 分支不建 RuntimeSession，`valid()` 恒 false；`commit()` 不校验，测试只测 commit | `playback_session_prepared.cpp:109-111`；`playback_session_prepare.cpp:337-342`<br>原结论：已核实（S1） | 未复核 | 开放 | T-17 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F502 | `componentMask()` 的 bit2 由 `requirements.empty()` 反推，检查条件恒假，双向校验不存在 | `packed_chart_tables_decoder_model.cpp:436`；`canonical_semantic_chart.hpp:156-157`<br>原结论：已核实（S2） | 未复核 | 开放 | T-13 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F503 | `normalizePresentationFrame` 仅测试调用；生产 renderer 另写一套，量化常量两份 | `presentation_extraction.cpp:163`；`presentation_renderer/src/draw_command.cpp:24,480`<br>原结论：已核实（S2） | 未复核 | 开放 | T-06（既有合同范围内归一化；其余差异交 T-09 登记） | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F504 | `ConstructionBudget` 在节点循环内构造，预算是每节点而非整次调用 | `pattern_dfa_node.hpp:49-52`<br>原结论：已核实（S2） | 未复核 | 开放 | T-10/T-12 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F505 | `mapBeatToTick` 仅头文件与测试引用，生产零调用 | `timebase.cpp:707`<br>原结论：已核实（S2） | 未复核 | 开放 | T-12 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F506 | candidate 分支在 `content` 被使用前返回，`openConfiguredPlaybackSource` 二次调用，首次结果丢弃 | `player_app.cpp:171`、`:177-180` 对比 `:164-169`<br>原结论：已核实（S1） | 未复核 | 开放 | T-15 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F507 | 两个 `find_if` 结果未判 `end()` 即解引用；当前不可达 | `execution_kernel_impl_phases.cpp:80-86`<br>原结论：已核实（S1） | 未复核 | 开放 | T-10 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F508 | `unload()` 未清 `hostFence`、`acceptedHostControl`、`confirmedHostContext`、`executionMapping`、`tailTransition` | `playback_session.cpp:319-348`；`playback_session_internal.hpp:217,218,221,245,246`<br>原结论：已核实（S1） | 未复核 | 开放 | T-17 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F509 | `PreparedPlayback::State::owner` 为裸指针并在 materialize 时解引用，Session 先销毁即 UAF | `playback_session_internal.hpp:268`；`playback_session_prepared.cpp:36`<br>原结论：已核实（S1） | 未复核 | 开放 | T-17 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F510 | Judgement seek 有实现（`playback_session.hpp:348,352`，仅在 `CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE` 下编译），权威合同是 candidate：RPA 合同第 4 节、V2 Spec 的 Fact Ledger 重建段、V2 ABI 域 9。三份 historical 文档已于 2026-11-17 补路标、2026-11-22 订正语义指向；§3.19 只覆盖延迟承诺 | `docs/MAP.md` 路由表；ADR 0043、`api/GAMEPLAY_JUDGEMENT_ABI.md`、`formats/GAMEPLAY_JUDGEMENT_SPEC.md`<br>原结论：部分成立（无 active 合同属候选状态，路标已订正） | 未复核 | 开放 | T-08/T-09 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |

#### 8.3 外部条目核查覆盖

外部报告 59 条已全部核查：D/DOC 类 13 条见 8.4 的 D1，P/T/A 类 20 条见 8.4 的 D2；
其余 C/S 类与既有登记册的对应关系见 8.1/8.2。8.4 中的“部分成立”已按核查结果改写断言，
不得再按外部原文引用。

#### 8.4 分批核查结果（2026-11-17）

##### D1 批次：外部报告 D/DOC 类（13 条）

| 外部 | 判定 | 校正与证据 |
| --- | --- | --- |
| D-06 | 成立 | `player_app.cpp:91` 无保护使用 `getenv_s`；engine 侧同款均带 `_MSC_VER` + `std::getenv` 回退；Player 只在 windows preset 编译 |
| D-07 | 成立 | `capture_route.cpp:467-470` 按值返回整份 `pending_`；`execution_host_frame.cpp:130` 每帧 `take()`；积压放大仅在 `take()` 抛错中断 flush 时成立 |
| D-08 | 部分成立 | 文件 677 行、单函数 667 行（`session_worker_dispatch.cpp:9-675`）；`if constexpr` 41 个正确；`prepared.reset()` 是 **4 处**（429/491/606/647），不是 3 处 |
| D-09 | 成立 | 函数实际 `execution_kernel_restore.cpp:14-60`；`flattenLog` 仅 rpa 分支；118.64 MiB 出处是 R4 交接 `:100-101`，不在 CURRENT_STATUS |
| D-14 | 成立 | 第二份在 `gameplay_graph_render.cpp:292-297`，7 个字段逐项一致 |
| D-20 | 成立 | `activeChartJson` 全仓仅声明/写/清三处、`parameters` 无读；candidate 来源时 `chartJson` 为空 |
| D-24 | 成立 | `canonical_chart_loader.cpp:216` 校验编译后丢弃；`playback_session_prepare.cpp:294` 再编；migrator 216/430/534 三次均为校验后丢弃 |
| D-25 | **部分成立（关键修正）** | O(n²) 成立，位置 `cxt_v2_loader.cpp:240-249`（另 220-237 有第二处线性扫描）；但“40k 探针未覆盖 CXT 展开”**不成立**：`chart_capacity_probe.cpp:426-454` 的 `high-reuse-v1` 已是 `cxt-v2-source` + `groups=10000`；真实缺口是 `expand()` 耗时未计入 `measureProfile`（268-329） |
| D-26 | 成立 | 行号 124-127；`canonicalBytesIdentity` 裸 sha256、无域分隔与长度框 |
| DOC-02 | 部分成立 | 选项表实际 **6 行**（`PLAYER_APPLICATION.md:115-122`），不是 7；解析器 19 个 flag、缺 13 个两个数字正确 |
| DOC-03 | 成立 | 文档 `:104`（另 36 行需同改）；实际链是 `preparePlayerLoadBundle` → `adoptStartupLoad` |
| DOC-04 | 成立 | 文档 `:207` 写 `KeepChartTime`，代码是 `RestartAtZero`（`player_control.cpp:151-154`） |
| DOC-07 | 成立 | `MODULE_BOUNDARIES.md:5` 日期 2026-10-28；全文无 `execution_host` |

统计：成立 10 / 部分成立 3 / 不成立 0 / 无法核实 0。D1 的净产出是三条校正：D-25 的探针结论、
D-08 的计数、DOC-02 的表格行数。

##### D2 批次：外部报告 P/T/A 类（20 条）

| 外部 | 判定 | 校正与证据 |
| --- | --- | --- |
| P-01 | 成立 | `capture_route.cpp:467-470` 按值返回整份 `pending_`；`execution_host_frame.cpp:130` 每帧 `flush()`；判定侧 `input.cpp:264-265` 追加、仅 `input_mapping.cpp:195-197` 清空，积压随会话保活 |
| P-02 | 成立 | `execution_kernel_restore.cpp:14-57` 全量复制并 `flattenLog`；`NativeRecoveryPoint` 保活；118.64 MiB 是对照实验值，不是生产实测 |
| P-03 | **部分成立（与 D1 冲突，按 D1）** | O(n²) 成立（`cxt_v2_loader.cpp:220-237,238-250`）；但“40k 探针不含 CXT 展开”**不成立**——D2 只搜了 `tests/pd` 与 `tools/verification/execution_acceptance`，漏了 `tests/chart/chart_capacity_probe.cpp:426-454`（`high-reuse-v1` = `cxt-v2-source` + `groups=10000`）。真实缺口同 D-25：`expand()` 未计入 `measureProfile` |
| P-04 | 成立 | v4 路径 `chart_v4_loader.cpp:255-258` → `canonical_chart_loader.cpp:216` 编译后丢弃、prepare 再编；v1–v3 migrator 三次（216/430/534）。需区分 v4 不重复 |
| P-05 | 成立 | 冷 seek `execution_kernel_archive.cpp:215-280` 逐 record 重放，`recovery.cpp:63-82` 全量比较 → O(records×projection) |
| T-01 | 部分成立 | “0 个独立 case”不实：`tests/pd` 直接 include execution_host 头、至少 37 个 Catch2 case；`rg session_worker_dispatch tests` 为 0，677 行 dispatch 与 close 仍只有 `tests/rpa` 集成覆盖 |
| T-02 | **部分成立** | 只有 allocation observer 与 ETW 是 Windows 专用；`tests/pd` 门只要求 candidate+STATIC，Linux CI 的 `candidate-headless-debug` 也构建 baseline pd。外部“只在 Windows 跑”不成立 |
| T-03 | 成立 | `player_execution.cpp` 643 行，`runExecutionPlayer` 约 536 行，tests 0 命中 |
| T-04 | 成立 | GameplayOnly 无访问器用例；但“测试只验证 commit()”措辞不精确，还有 advance/query |
| T-05 | 部分成立 | `check_docs.py` 有窄口径契约检查（如 SDK API 只比对 BUILDING 的 find_package），但没有通用的选项/签名/启动流程一致性门禁 |
| A-01 | 成立 | 候选 47 / 公共 81 = 58.0%；测试是共用 `tests/playback` 按 tag 而非独立目录 |
| A-02 | 成立 | 5 条格式线均在树（Chart v1–v4 + v5 candidate、CXT v1/v2、Packed、CXC v1、Capsule/Graph） |
| A-03 | 成立 | 五个核心模块 >30,000 B 恰 14 个；前三 42,092 / 38,261 / 35,561 B。全仓口径不同（22 个），报告未写口径 |
| A-04 | 部分成立 | 81 个公共方法正确；`PlaybackSession::State` 是 44 个字段（25 生产 + 19 候选），不是“30 多个” |
| A-05 | 部分成立 | 计数精确（717 篇 127,323 行；代码 108,626 行）；“维护文档时间超过写代码”无仓库证据，属判断 |
| A-06 | 成立 | observer/ETW 平台限制、24 个 preset、hosted/本地差异均有据 |
| A-07 | 部分成立 | 版本门禁对全部 PR 生效、无 paths 过滤；但引用版本已过期，当前为 `26.11.16-1`（历史记录） |
| A-08 | 部分成立 | `OVERVIEW.md`（136 行）已是现成总览，“缺架构快照”过强；缺的是按阶段刷新（更新日期 2026-10-28） |
| A-09 | 不成立（已处置） | `MODULE_BOUNDARIES.md:18` 已有约束；ADR 0027 本轮已补 Judgement 边界句；`judgement/CMakeLists.txt:63-66` 仅链 core |
| A-10 | 无法核实 | 数字均可核（10.8 万/12.7 万/24 preset/4 workflow）；“单人产能配平台级验收”属人力判断，仓库无数据 |

统计：成立 11 / 部分成立 7 / 不成立 1 / 无法核实 1。净产出是四条校正：T-02 的平台范围、T-01 的用例数、
P-03 的探针覆盖（按 D1 裁定）、A-04/A-07/A-08 的计数与现状。

### 9. 修复清单（2026-11-17 核查后）

排序依据：后果是否不可逆或静默 → 触发条件是否可达 → 修复成本 → 能否不构建验证。
“需构建”指必须跑 CTest/构建才能确认修复；其余可先做静态审查与单测设计。

#### 9.1 第一批：数据正确性与挂死（P0，需构建）

| ID | 缺陷 | 触发 | 修复动作 | 验证 |
| --- | --- | --- | --- | --- |
| RR-F401 | `ChartWriter::write` 恒写 v3，丢 timing/templates | 任意保存 | 改名 `writeV3Projection` 或加目标版本参数；v4 显式报错 | v4 round-trip 测试 |
| RR-F501 | GameplayOnly 被 `valid()` 判无效 | `prepareGameplayLoad` 后读访问器 | `valid()` 增加 `gameplayContent` 条件 | GameplayOnly 访问器测试 |
| RR-F506 | `openConfiguredPlaybackSource` 调两次 | candidate 执行路径 | 复用第 171 行已解析的 `content` | 读盘计数测试 |
| RR-F508 | `unload()` 漏清 5 个候选字段 | unload → commit | 引入 `CandidateState::reset()`，三处共用 | 跨 unload 状态泄漏测试 |
| RR-F509 | `PreparedPlayback::State::owner` 裸指针 | Session 先于 Prepared 销毁 | 改 `weak_ptr`/generation 校验 | 生命周期测试 |
| RR-F507 | `find_if` 未判 `end()` | 资源表不同步（当前不可达） | 两处补 `end()` 检查 | 单元测试（构造缺失资源） |
| RR-F417 | Close 超预算后析构自旋 | Close 首次超预算 | 析构加上限与放弃策略 | 假时钟 close 测试 |

#### 9.2 第二批：假保证与死分支（P1，部分需构建）

| ID | 缺陷 | 修复动作 | 验证 |
| --- | --- | --- | --- |
| RR-F428 | close 后 `rebuild()` 假成功 | `rebuild`/`resize` 统一拒绝 closed；OpenGL 后端在 context 已销毁时返回错误 | renderer 单测 + 假后端 |
| RR-F427 | `detachCandidate` 无 owner 校验 | 首行 `if (!owner.isCurrent()) return/terminate`，与 `activate/discard` 一致 | 跨线程析构测试（debug） |
| RR-F429 | 注释承诺泄漏日志，实现静默 | 在 close 的 not-main-thread 分支补 `logWarning`，或删注释 | 日志断言 |
| RR-F430 | `HostClock::snapshot` 返回陈旧样本 | 注释写明 stale 语义，或返回带版本/optional | 并发失败注入测试 |
| RR-F434 | `version_unsupported` 不可达 | 删除分支或补越界版本测试 | 单测 |
| RR-F435 | 无 World 会话仍置 `Ready` | `:422` 区分 gameplay-only 状态；文档说明 World 帧管线限制 | feasibility 单测 |
| RR-F432 | assembler 不用 `effectiveLimits` | `:32` 改用 `packed::limits_detail::effectiveLimits(limits)` | 放宽上限测试 |
| RR-F431 | 预算超限继续解析 | 超限立即返回或截断循环 | 生成 40k+ clip fixture |
| RR-F433 | `discardedFields` 恒空 | 至少记录 v3 `templates` 的丢弃 | migrator 报告测试 |
| RR-F436 | 借出指针无生命周期强制 | 改按值返回或带租约 token | 借用逃逸测试 |
| RR-F437 | 5 个 const 观察函数仅 Debug 断言 | Release 下返回错误/空值，或写入契约 | Release 构建测试 |
| RR-F438 | `noexcept` + 分配型 `Error` | 契约写明 OOM=terminate，或改不分配错误码 | 契约评审（无需构建） |

#### 9.3 第三批：文档与契约（P2）

DOC-02、DOC-03、DOC-04、DOC-07 已于 2026-11-17 本轮修复（选项表补到 19 项、启动链改为
`preparePlayerLoadBundle` → `adoptStartupLoad`、R 键改为 `RestartAtZero`、模块边界补
`cuexis_execution_host` 行与依赖原则）。剩余 P2 项：RR-F104（MATERIAL_SHADER 已修）、
35 条版本字面量漂移、RR-F510（Judgement seek 无 active 权威入口，待契约决策）。

### 10. 工具链审计（2026-11-17）

覆盖 `tools/` 120 个脚本（5 个无上下文子代理）。核心结论：**不能按“目录搬迁”整理**。
R4 / PD-5 套件的相对路径已进入验收哈希账本（`binding.py:73`、六处 `prepare_*` 的 glob、
`etw_safety` 封包按文件哈希），搬目录会让历史验收记录永久失配。整理改为
“**原位冻结 + 修门禁 + 登记报告来源**”。

#### 10.1 新发现（已核实）

<!-- cuexis-findings: definitions -->
| ID | 原始发现 | 原始依据 | 核验状态 | 处置状态 | 责任/任务 | 当前处置依据 |
| --- | --- | --- | --- | --- | --- | --- |
| RR-F511 | `execution_acceptance` 的 22 个测试注册为 CTest，但整块被默认 OFF 的 `CUEXIS_ENABLE_CHART_V5_CANDIDATE` 包住；CI 的 `debug`/`release` 不打开，预推送只跑 `debug`/`release` → 实际生效数为 **0**；而 `tools/README.md` 又把它标为“冻结证据工具，不进 CI” | `tests/pd/CMakeLists.txt:54-58`；`CMakeLists.txt:44-45`；`CMakePresets.json`<br>原结论：已修（owner 2026-11-17 决定移除注册；复现改用人工 `unittest discover`） | 未复核 | 开放 | T-04 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F512 | candidate preset 的测试过滤为 `[Cc]andidate\|reference_host`，`cuexis_gameplay_assembler_cli` 与 `cuexis_gameplay_installed_consumer` 的名字不匹配 → 候选构建里也不跑 | `CMakePresets.json:527-555`<br>原结论：已修（三个 candidate preset 的过滤加入 `assembler_cli\|installed_consumer`） | 未复核 | 开放 | T-04 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F513 | `cuexis_asset_importer_tool_tests` 需要 `DEVELOPER_TOOLS` + `SHADER_TOOLS`，但 hosted shader-tools preset 显式 `DEVELOPER_TOOLS=OFF` → 仅本地可达；BUILDING 漏写该前提 | `tools/CMakeLists.txt`；`CMakeLists.txt:173`；shader-tools presets；`BUILDING.md:139-140`<br>原结论：已修（顶层与 `tools/CMakeLists.txt` 改为 `SHADER_TOOLS` 单独可进；importer 测试随 preset 可达） | 未复核 | 开放 | T-04 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F514 | 生成 `schemas/candidate/*.schema.json` 合同产物，但无生成器新鲜度检查 | `tools/gameplay/gameplay_assembler/generate_author_schema.py`<br>原结论：已修（新增 `--check` 并接入预推送） | 未复核 | 开放 | T-18 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F515 | README 只登记 Python 门禁；C++ 门禁 target（cxc/chart/media/gameplay）未登记，`cuexis_gameplay_assemble` 也不在 BUILDING 的 target 清单 | `tools/README.md`；`BUILDING.md:52-59`<br>原结论：已修（README 重写并补 C++ target） | 已核实 | 已修 | T-18 | 依据：原字段与 §22 历史处置；复核者：Codex-G02（2026-11-23）；复核证据：[工具职责](../../../../tools/README.md)、[构建指南](../../../guides/BUILDING.md)（当前正文与 ca5 已提交对应段）；理由：C++ 门禁 target 和候选 assembler 已登记；只恢复工具文档遗漏的订正；决定依据：§22 既有文档处置与原字段记录；实际验证：当前/ca5 工具 target 登记与 BUILDING 候选前提断言通过；改动修订：ca5cbb9121b015ae0b3b5d681610ca94dac78184（包含原文档订正的已提交快照） |
| RR-F516 | 无测试、无 `docs/` 引用、只服务已关闭的 C2 候选 → 唯一 archive 候选 | `tools/verification/playback_performance/compare_read_candidates.py`<br>原结论：已归档到 `tools/archive/verification/playback_performance/`（2026-11-17） | 未复核 | 开放 | T-09（核原处置及缺失证据，按实际结果具名交接） | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F517 | 文件头无“复现哪份报告”；两处硬编码本机 VS/vcpkg 路径，违反 `tools/README.md` 自有规则 | R4 套件 14 个归档文件；`audit_layout.py`、`build.py`<br>原结论：已修（14 个文件加复现映射头；`--vcvars/--vcpkg` 改为参数或环境变量） | 未复核 | 开放 | T-09（核原处置及缺失证据，按实际结果具名交接） | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F518 | CI 的 `documentation-contracts` 只跑 `check_docs.py`、`update_version.py --check`、`gnu_asset_mirror_tests.py`，**不跑** `check_docs_facts.py`；生成块却写“`--check` 会在 CI/预推送校验” | `linux-quality.yml:279-282`；`gen_docs_map.py:208`<br>原结论：已修（CI 加跑 `check_docs_facts.py`；生成文案改为“由 `check_docs_facts.py` 在预推送校验”） | 未复核 | 开放 | T-18 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F519 | 三个活门禁的回归测试没有任何 CI/CTest/预推送入口，只能人工运行；其中 section 测试已因 stage-07 迁移漂移出 11 个失败 | `check_docs_{status,target,section}_contract_tests.py`<br>原结论：已修（接入预推送 Quick；section 测试路径同步 `completed/stage-07`） | 未复核 | 开放 | T-18 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F520 | 生成器把 `executionProfile` 写成单个 `const`，但已提交 schema 与合同允许 `gameplay.execution.t4-k4.rpa.v1`；直接重新生成会删掉该 enum（真实漂移，被新加的 `--check` 首次运行抓到） | `generate_author_schema.py:91`；`schemas/candidate/*.schema.json`；`execution-coordination.md:12`<br>原结论：已修（生成器改为两项 enum；`--check` 进预推送） | 未复核 | 开放 | T-18 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |

#### 10.2 分类统计

| 套件 | keep-gate | keep-maintainer | archive | delete-candidate |
| --- | --- | --- | --- | --- |
| `execution_acceptance`（50） | 22 | 14 | 14 | 0 |
| `playback_performance`（37 py） | 19 | 17 | 1 | 0 |
| `cxc`/`chart`/`gameplay`/`shader`/`media`（54） | 51 | 3 | 0 | 0 |

“delete-candidate”为 0 的直接原因：每个脚本都至少被一份证据报告按路径引用；但引用不等于需要，
真正可退役的是 `compare_read_candidates.py`（RR-F516）。

### 11. S-06 / S-07 独立核查（2026-11-17）

两个无上下文子代理只读核查外部报告的两条架构断言。结论均为**部分成立**。

#### 11.1 S-06 “Chart 有五种表示”

- 被点名的 5 个类型全部存在，定义集中在 `engine/chart/include/cuexis/chart/`；`ChartRuntime`
  是编译结果、`CanonicalSemanticChart` 是另一输入族的语义图，职责对得上。
- “五种”是低估：实际 **7 个内容型表示**（另有 `ResolvedChartDocument`、`CandidateRuntimeArtifact`）、
  **6 个载体**（`PrepareArtifact` 同时持有 ChartDocument/v4 源/v4 解析产物）、**3 类物理编码**。
- 五者不是一条流水线：**三条输入族**（v1–v3 JSON / v4 JSON / Packed-CXT-candidate）共用
  `ChartDocument` 与 `ChartRuntime` 两个节点。
- “容易漏转”有实证：v4→v3 投影逻辑写了两份（`chart_v4_loader_projection.cpp:197-201` 与
  `chart_v4_resolver_patching.cpp:256-260`）；`ChartWriter::write` 丢 timing/templates/behavior tracks
  （RR-F401）。
- 文档缺口：**没有**“表示与转换点”表；`CHART_V4_FORMAT.md:36-49` 用占位名
  `SourceChartDocument`；`PACKED_CHART_FORMAT.md:68-80` 列出的 `semanticVersion` /
  `behaviorDefinitions` 等字段在 `canonical_semantic_chart.hpp:180-188` 不存在。
- 处置：新增“Chart 表示与转换点”表（文档）；F401 仍按 §9 P0 修代码。

#### 11.2 S-07 “只加不减”

- 四个对象都还在：`cuexis_render`、Chart v1/v2/v3 wire、Packed（CXPk5 rev1）、旧 loader。
- “Stage 7 没有删除任何模块”**成立**：模块级 0 删除/合并，反而新增 3 个
  （judgement、gameplay_packed、execution_host）。
- 但“只有加”不准确：代码路径删除 19 个文件（6 个真删、13 个拆分替换），另有 15 个文件跨模块
  搬到 `app/execution_host`。
- 把 `render` 当遗留物是误判：它是生产 ECS 数据面（runtime/playback/opengl/player 都链接）；
  真正零生产消费者的是 `engine/particles`（空壳）与 `cuexis_debug`（仅测试）。
- “没有删除清单”成立；但限制不是散注释，而是 **ADR 0041** 的明确决策（Chart v1–v3 保留、
  `RenderFrame` 标 legacy 暂不删）。
- 处置：阶段收尾把“删除/合并清单”列为交付物（文档政策）；空壳与测试专用模块登记为清理候选。

<!-- cuexis-findings: definitions -->
| ID | 原始发现 | 原始依据 | 核验状态 | 处置状态 | 责任/任务 | 当前处置依据 |
| --- | --- | --- | --- | --- | --- | --- |
| RR-F521 | 空 INTERFACE 库，`engine/CMakeLists.txt` 从不 `add_subdirectory` | `engine/particles/`（仅 `CMakeLists.txt:1-3`）<br>原结论：已修（2026-11-17 删除目录；无构建引用） | 未复核 | 开放 | T-07 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F522 | 唯一链接方是 `cuexis_debug_tests`；`DebugDraw` 生产 0 引用 | `engine/debug/`（87 行）<br>原结论：已处置（保留并标 test-only；模块删除不在 D-05 的 R1/R5/R6 范围，另行评估） | 未复核 | 开放 | T-07 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |

## 第二部分 候选设计与文档问题（未实施）

本部分登记尚未实施的新设计、候选合同、任务卡与文档描述问题。任何条目都不表示产品已完成、已发布或已获派单授权。

## 17. 候选设计缺陷（未实施）

本节登记尚未实施的新设计与候选合同缺陷。输入为四组只读诊断（主干计划、三份候选合同、任务卡与验收、索引与交叉引用）与主代理原文复核。缺陷清单见 §17.1，逐条方案见 §17.2；口径与定义冲突（RR-F530–RR-F532）见 §18，方案层与门禁覆盖（RR-F533–RR-F538）见 §19，文档描述层待办见 §20，本轮撤回与核对见 §21。本节各项由主代理读原文核对，owner 复核另行记录。本节不是修复声明，不改变 D-05、合同语义、任务图或实施授权；每条处置须 owner 裁定。

### 17.1 缺陷清单（RR-F523–RR-F529）

<!-- cuexis-findings: definitions -->
| ID | 原始发现 | 原始依据 | 核验状态 | 处置状态 | 责任/任务 | 当前处置依据 |
| --- | --- | --- | --- | --- | --- | --- |
| RR-F523 | 原严重度：P1<br>保存丢数据（RR-F401）在 §9.1 有修法，两个常量诊断字段（RR-F419/F420）只在 §7.1 登记；三项在 T-01–T-16 中均无明文承接。计划自述"未开始修复"，阶段目标第 3 条只能靠"显式登记"兜底。P0 数据丢失因此不在本阶段任何验收口径内<br>原结论：成立 | 本节 §9.1、[计划](plan.md) §6 任务索引、[计划](plan.md#历史进展快照2026-11-17) | 未复核 | 开放 | T-06/T-14/T-16/T-09 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F524 | 原严重度：P1<br>旧渲染入口退出要求"历史安装面"证据、仓外宿主验证与至少一个实际发布的过渡版本，而附页自述历史安装面尚未核实、计划明确本阶段不发布。三条合起来使 `RenderFrame`／`renderFrame` 在可预见路径上无法退出，"逐项替代并按门禁退出"退化为永久保留<br>原结论：成立 | [附页](architecture-design.md) §6 D-25、[验收](acceptance.md) §发布与迁移门禁、[计划](plan.md) §13 | 未复核 | 开放 | T-08/T-09 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F525 | 原严重度：P1<br>三处等待型规则（必要参与方确认、安全续局"最终"判定、设备就绪）都只写"条件成立才继续"，全文无"超时/放弃"表述；唯一时间界是向量 P4-03 的"位置未知 500 ms 内零推进"。规则只能从测试反推，实现会各写魔数<br>原结论：部分成立 | [计划](plan.md) §4 D-09、[附页](architecture-design.md) §6 D-12／D-13、[呈现合同](contracts/render-player-writer.md) 向量 P4-03 | 未复核 | 开放 | T-14/T-15 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F526 | 原严重度：P1<br>误差定义为 `observationTick − target`，而输入补偿、设备校正与谱面 offset 在会话换算链上应用，尾段分支还写明"输入和显示偏移各在分支转换时应用一次"；正文未说 `observationTick` 是校正前还是校正后。补偿方向一变，Early／Exact／Late 的分布整体平移<br>原结论：成立 | [运行合同](contracts/runtime-timing.md) §事件与成绩、[附页](architecture-design.md) §6 D-21、[呈现合同](contracts/render-player-writer.md) 尾段分支 | 未复核 | 开放 | T-14 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F527 | 原严重度：P2<br>D-06／D-09／D-10 把普通播放与实机游玩收进一条执行线，D-14 自动续播、D-15 双事实结束、D-22／D-24 判定接入又按"用途"分叉，共四处；设计没有显式的"判定活跃"状态，合并后用途成为隐式条件<br>原结论：部分成立 | [附页](architecture-design.md) §2、§6 D-14／D-15、[运行合同](contracts/runtime-timing.md) 判定接入 | 未复核 | 开放 | T-14 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F528 | 原严重度：P2<br>跨局保留当前内容与健康音频，Reload 要求 A、B 并存且明令"禁止提前卸载 A 来腾空间"。owner 接受该开销但无上界，计划又明确不做预算测量，验收矩阵无内存项，只剩"资源不足即失败"兜底<br>原结论：成立 | [附页](architecture-design.md) §6 D-13／D-15（续议）、本文 §4 | 未复核 | 开放 | T-15 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F529 | 原严重度：P2<br>附页写"不裁定无音轨自然终点"，同一合同已给出完整计算规则（连续行为取末点、无限绑定须显式 Stop、空内容尾为零）。两页对同一问题给出相反状态，读者无法判断是否已定<br>原结论：成立 | [附页](architecture-design.md) §6 候选规则参考、[呈现合同](contracts/render-player-writer.md) 无音轨终点 | 未复核 | 开放 | T-14 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |

### 17.2 候选完善方案（待审）

方案按最小改动与独立采用边界给出，互斥项各自列出；推荐只表示派单顺序建议，不代表已接受。

**RR-F523（P0 无卡承接）**

- **方案 A**：并入 T-03。修法与合同/诊断收敛同批，代价是代码修复落进文档批次，正反证据不可分离。
- **方案 B**：新增 T-17「P0 数据保留修复」，写域为 `engine/chart/src/chart_writer.cpp` 与对应图表测试，排在 T-03 之后、T-07 之前。代价是任务图与调度变更，须按 §4 范围变更登记。
- **方案 C**：延后至 Stage 7.5，A-05 登记归属、恢复门槛与 owner 接受记录，阶段目标第 3 条改为"静默失败已显式登记"。
- 推荐 B；owner 不扩任务图时选 C。A 不建议，因为代码修复与合同裁定共用一批会让 A-02／A-03 的判定互相牵连。

**RR-F524（退役门禁不可满足）**

- **方案 A**：把判据换成源码消费裁定，由 owner 认定该入口历史发布仅支持源码消费，安装宿主门禁记"不适用"，保留版本目标与迁移说明后按 ADR 0041 退出。风险是以裁定替代证据。
- **方案 B**：本阶段不裁定退出。A-04 只核兼容新增、迁移与过渡发布计划，删去"核验旧声明与符号退出"，退出移交 Stage 14 与稳定 C ABI 一并处理。风险是该入口长期保留。
- **方案 C**：先做一次仓外宿主取证尝试，不可得时由 owner 按 D-04 例外程序裁定。
- 推荐 B 作为阶段口径、A 作为后续裁定路径；C 只在确需本阶段退出时启用，因为取证本身依赖发布。

**RR-F525（等待态活性）**

- **方案 A**：统一无时限，只允许用户显式退出或故障报告，向量 P4-03 的 500 ms 改为"不设界限"。
- **方案 B**：统一 observation deadline，把 500 ms 上升为规则，超时转为独立暂停原因并给出可重试诊断。
- **方案 C**：分层。设备接入有界且可重试，运行期设备事实无界但必须可退出。
- 推荐 C。设备接入失败有明确重试动作，运行期事实超时无法自动推断安全，强行设界会改变计分责任。无论选哪条，界限或"不设界限"都要写进 D-12／D-13 正文并与向量对齐。

**RR-F526（误差与输入补偿）**

- **方案 A**：`observationTick` 定义为会话换算后、含输入补偿的观测 tick，显示提前不参与判定；误差、Exact 与方向在同一域比较。
- **方案 B**：观测保持原始，把补偿加到目标侧，效果等价但换算分散在两处。
- **方案 C**：引入独立"判定时间域"，所有窗口与误差在编译成品域计算，设备补偿只影响播放时刻。
- 推荐 A，它与 D-21"会话校正不改逻辑谱面时刻"一致，且只需一句定义加一处向量。该条影响成绩可观测值，须 owner 确认后写入合同。

**RR-F527（按用途分叉）**

- **方案 A**：会话状态机引入显式 `judgementActive`，由启动显式请求与内容能力共同决定，四处规则都以该状态为条件。
- **方案 B**：保留用途参数，在合同里列出启动、暂停恢复、结束、交付四处的分叉条件表，作为合并前的过渡。
- **方案 C**：不建模，靠向量覆盖两条路径，待 D-10 合并时再收敛。
- 推荐 A，B 可作为 A 落地前的过渡；C 会把隐式条件留到合并当天。

**RR-F528（跨局内存峰值）**

- **方案 A**：设预算上限（按内容规模倍数），超限先释放 A 的可重解码音频。代价是要改"禁止提前卸载"，触碰已接受边界。
- **方案 B**：不设预算，验收增加峰值观测（只记录不设阈），数据交给 Stage 7.5 的 GE-R 统一定预算。
- **方案 C**：允许在 B 资源已分配、切换前释放 A 的可重建资源，保留不可变内容。
- 推荐 B，A 留给 GE-R；C 会改变 D-13 的失败原子性，不建议在本阶段做。

**RR-F529（无音轨终点状态相反）**

- **方案 A**：附页改为"已由合同裁定"并链到合同小节。
- **方案 B**：合同标注"代理技术推荐、未获 owner 逐字接受"，附页保留未决。
- **方案 C**：把它提为 D-26，取得 owner 接受后两页同步。
- 推荐 C；至少执行 A，否则读者会把已定规则当成未决项。

### 17.3 处置（2026-11-22）

本轮按最小改法落地，只改文档与卡片措辞，未改实现、wire、接受集或诊断码。事实订正一处：§9.1 只为 RR-F401 写了修法，RR-F419/F420 只在 §7.1 登记；F420 与 PD-4 的既定语义冲突，不按假实现排修。

| ID | 采用改法 | 落点 | 状态 |
| --- | --- | --- | --- |
| RR-F523 | RR-F419 排入 T-06 验收（首帧非零、后续帧非负，并说明 0 的含义变化）；RR-F401 由 T-16 承接并在 T-09 记具名延后；RR-F420 语义待定（计数精度读法 vs 保守恒 false），待 owner 裁定 | `tasks/T-06`、`T-16`、`T-09` | F419 已排，F401 待 T-16 采用，F420 待裁 |
| RR-F524 | A-04 限定为兼容新增、迁移计划与目标版本；退役核验随稳定 C ABI 归 Stage 14 | `acceptance.md` 发布与迁移门禁 | 已落地 |
| RR-F525 | 附页 D-09 补等待态无自动超时，点名 D-12、D-13 与 P4-03；呈现合同 P4-03 加观测窗口说明 | `architecture-design.md` D-09、`render-player-writer.md` P4-03 | 待 owner 复核 |
| RR-F526 | 误差定义处回指 Playback 时钟的 observationTick 换算 | `runtime-timing.md` 事件与成绩 | 已落地 |
| RR-F527 | D-09 命名显式判定活跃状态，四处分支只读它 | `architecture-design.md` D-09 | 已落地 |
| RR-F528 | 峰值内存只在 T-15 P4-02 记观测（只记录不设阈，预算归 Stage 7.5 GE-R）；D-13 正文不改 | `tasks/T-15` | 已落地，预算待 Stage 7.5 |
| RR-F529 | 附页改为候选规则见合同、本段不另作裁定 | `architecture-design.md` §6 | 已落地 |

待 owner 追认的三处：等待态不设超时（F525）、判定活跃命名（F527）、峰值只观测不设阈（F528）。其余为回指或状态订正，不改语义。

承接点名（2026-11-22）：T-04 点名 RR-F208/F214，T-06 点名 RR-F135/F508/F420，T-12 点名 RR-F505，T-13 点名 RR-F432/F502，T-15 点名 RR-F418/F506/F142，T-16 点名 RR-F401/F433/F402；T-17 单独立项承接 RR-F501/F508/F509（未启动实施）；RR-F001/F002/F103/F105/F113/F121 已在本轮复核后标为已修，RR-F522 的"留待 RR-3"改为按 D-05 范围另行评估。

## 18. 口径与定义冲突（未实施设计）

<!-- cuexis-findings: definitions -->
| ID | 原始发现 | 原始依据 | 核验状态 | 处置状态 | 责任/任务 | 当前处置依据 |
| --- | --- | --- | --- | --- | --- | --- |
| RR-F530 | 原严重度：P1<br>计划写"候选表示未实施、未发布"，ADR 写"已有 candidate 实施、未发行"，代码里 `app/player/src/execution_host_frame.cpp`、`engine/playback/src/playback_session_realtime.cpp` 即该批实施；16 张卡又用同一词表示"不进本阶段派单"<br>原结论：成立 | [计划](plan.md) §4 排期与实施状态、[ADR 0046](../../../adr/0046-realtime-playback-coordination.md) | 未复核 | 开放 | T-07 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F531 | 原严重度：P1<br>旧 API 分组迁移规则在六处各写一遍，作用范围不同：计划只限 T-06／T-08，验收与运行合同推广到全部旧 API 组；且都与"本阶段不发布"并存<br>原结论：成立 | [附页](architecture-design.md) D-19、[计划](plan.md) §4 兼容窗口、[验收](acceptance.md) 发布与迁移门禁、[运行合同](contracts/runtime-timing.md)、[呈现合同](contracts/render-player-writer.md)、[方案导读](simplification-proposal.md) | 未复核 | 开放 | T-08/T-09 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F532 | 原严重度：P2<br>合同已写基线"当前 R 是 Reload"并闭合 D-13 的目标 R=Retry，但未说明这属已发布输入契约变更、需同步哪几页；现行为文档与源码仍是 R=Reload<br>原结论：部分成立 | [呈现合同](contracts/render-player-writer.md) 命令与启动、`docs/architecture/PLAYER_APPLICATION.md` | 未复核 | 开放 | T-07/T-15 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |

处置建议：RR-F530 与 RR-F531 待 owner 定口径后统一改写并回填本节；RR-F532 由 owner 决定是否按契约变更排入 T-15 并同步应用文档。

## 19. 方案层与门禁覆盖（未实施设计）

<!-- cuexis-findings: definitions -->
| ID | 原始发现 | 原始依据 | 核验状态 | 处置状态 | 责任/任务 | 当前处置依据 |
| --- | --- | --- | --- | --- | --- | --- |
| RR-F533 | 原严重度：P2<br>约定禁表内写状态、计划称候选波次不占排期、检查器强制四列，三者叠在一起使 active 的 1–8 波与 candidate 的 9–15 波混排且无法标注，读者会按表派候选卡<br>原结论：成立 | [HANDOFF](../../../HANDOFF.md) Plan 拆分约定、[计划](plan.md) §4、[调度](schedule.md)、`tools/verification/check_plan_schedule.py` | 未复核 | 开放 | T-18 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F534 | 原严重度：P2<br>同一页既写 `out/` 不能作为唯一可取得的通过证据，又把四组审查的"最终封存"指向 `out/review/stage-07-rr/T-01/final-20261122/`；独立 checkout 或清理构建目录后 A-01 证据不可取得<br>原结论：成立 | [验收](acceptance.md) 证据清单 | 未复核 | 开放 | T-01/T-09 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F535 | 原严重度：P2<br>A-02–A-05 的接受者列写"CI 验证可机械项"，但矩阵与 T-09 都没有机械项清单；A-N 明确不证明产品完成、不允许开工，实际不裁定任何对象<br>原结论：成立 | [验收](acceptance.md) 阶段验收矩阵 | 未复核 | 开放 | T-09 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F536 | 原严重度：P3<br>长期文档结构约定写在滚动交接页，而规则已落地为 `tasks/`、`schedule.md` 与检查器；下一次交接改写时无人维护该规则<br>原结论：成立 | [HANDOFF](../../../HANDOFF.md) Plan 文档拆分 | 未复核 | 开放 | T-18 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F537 | 原严重度：P2<br>合同 id 三种写法、运行合同缺"更新日期"、附页 11 处代码行号锚，均违反政策条目，而 `check_docs.py` 仍 exit 0；门禁未覆盖政策实际要求<br>原结论：成立 | 三份候选合同的 `cuexis-meta` 与附页行号锚、`docs/DOCUMENTATION_POLICY.md` | 未复核 | 开放 | T-18 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F538 | 原严重度：P2<br>调度检查器经 `tools/check_docs.py` 随预推送与 CI 运行，但其回归测试只在 `tools/README.md` 与 T-01 手工运行，同类 `check_docs_*_contract_tests.py` 均已入门禁<br>原结论：成立 | `tools/verification/test_check_plan_schedule.py` | 未复核 | 开放 | T-18 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |

## 20. 文档描述层待办（本轮，不新登记 ID）

本轮描述层审查共 58 条：45 条属写法订正（P 类），14 条需 owner 定口径（O 类）。P 类不逐条入册，它们在同一批"单一权威落点"改写里完成，改完以 diff 与门禁回执复核，逐条入册只会淹没问题清单。O 类如下，裁定后回填处置并同步到对应文档；原第 15 项（按键契约）已并入 RR-F532。

2026-11-22 收尾：P 类由两组评审按当前文本重新提取并应用 42 条；O1–O14 按 owner 决定落地。其中 O2 用四类标记图例与标题映射、O6 用术语定义、O12 用计划文字说明波次类型，未改附页逐节状态行与调度表结构。

| 编号 | 待裁定项 | 位置 | 建议 |
| --- | --- | --- | --- |
| O1 | 三份候选合同头部不同形：更新日期、实现状态、标题与 `cuexis-meta` id、authority 各异 | 三份合同、`docs/DOCUMENTATION_POLICY.md` | 统一头部与 id 形状，并裁定运行合同是否改 `owns` |
| O2 | 状态标注无统一词表：附页 §4 有六种标记、§6 各节十余种写法，D-16–D-20 无状态词 | 附页 §4、§6 | 收敛为现状／owner 已接受／候选／延期四类，每节首句标注 |
| O3 | 减重选项 R1–R6 与工作项 RR-0–RR-6 同页并排 | 计划 §2、§4 | 选项写"选项 R5"，工作项写"RR-5" |
| O4 | "采用"与"接受"未定义且混用 | 全册 | 定义：接受＝owner 裁定，采用＝派单时绑定 |
| O5 | "候选"与"实验"交替指同一构建，preset 名与头命名空间不一致 | 计划、T-04、T-08、合同 | 正文统一"候选构建"，`experimental` 只作标识符 |
| O6 | "成品"兼指旧 Packed 产物与后继拥有型准备内容 | 附页 §6、合同 | 定为"编译成品"与"准备内容" |
| O7 | 附页与合同对同一规则各写一遍（Retry/Reload 顺序、量化、计分、Stop 与 ACK、GP-01 fixture） | 附页 §6、三份合同 | 附页留意图与链接，可测规则归合同 |
| O8 | 计划 §12 变更表无状态列，历史与现行混排 | 计划 §12 | 加"现行／已取代／仅记录"列 |
| O9 | 可实施性索引 P6.x 与 §5.x 共 11 行重复计划与验收矩阵 | design-readiness | 收成两行，追溯留本文 §13 |
| O10 | 方案导读把附页结论再压缩一遍，无路标作用 | simplification-proposal | 改成"基线索引"两列，或改名 |
| O11 | 附页缺 D-06／D-07／D-08 小节，计划 §4 十行只链整页 | 附页 §6、计划 §4 | 补三节并加锚点 |
| O12 | 调度表无法区分 active 与 candidate 波次 | schedule.md | 与 RR-F533 一并裁定 |
| O13 | 登记册的处置状态与修复声明口径未定义 | 本文 §0 | 本轮已补阅读索引与新旧分离说明，口径仍待定 |
| O14 | `NEXT` 既作范围词又作执行主体，任务卡头无范围码栏 | 合同、任务卡 | 卡头加范围码，正文写"T-13（NEXT）" |

## 21. 本轮撤回与核对记录

- D-15 的 Stop 截止"缺输入全序"一项撤回：合同以串行 owner 事务加 `acceptedIngressThrough` 给出真实上界，规则闭合，不足只在附页摘要未写。
- 27 篇阶段文档的 761 条相对链接与全部标题锚经两例独立脚本核对，0 失效、0 锚点缺失；引用问题仅剩代码行号锚与重复定义。
- 判分与时钟向量（GP-01、CLK-02／03、P4-05）的算术抽查自洽；D-13 源码基线与 `player_assembly.cpp` 一致。
- `check_docs.py`、`check_docs_facts.py`、`check_plan_schedule.py` 在本轮工作树上均 exit 0；调度检查器经 `tools/check_docs.py` 接入预推送与 `linux-quality.yml`，先前"未接门禁"的判断有误，已撤回。
- 本节的候选方案与口径建议均为待审材料，不构成决定、修复声明或派单授权。

## 22. 旧问题承接（2026-11-22）

本节把第一部分仍开放的问题按承接方归组，供 T-09 退出时引用；逐条结论仍以各节原文为准，编号不重排。标"延后"的条目要由 T-09 写成具名延后，写明责任阶段与恢复条件。

| 组 | 承接方 | 范围与例外 |
| --- | --- | --- |
| 已修已关 | — | F001、F002、F103、F105、F113、F121 于本轮关闭；F101、F102、F104、F112、F115、F116、F126、F141、F202、F311、F522 早前已处置 |
| 评审合同 | T-03 | F111、F112、F114、F131；F113 的具名常量归 T-03，F121 的测试重复编译归 T-04 |
| 文档事实 | T-02 | §2.1 的版本与状态漂移按 T-02 写域核对；§3 的 F201／F204 文档说明随 T-07，其余未被卡面覆盖的 P2 项由 T-09 逐条登记处置，不视为 T-02 已承接 |
| 构建隔离 | T-04 | F208、F214、F301 的默认/候选闭包；F121 的测试重复编译按 F214 同一验收核对 |
| 帧链路 | T-06 | F135、F419；F305 只核本卡同次帧提取与交付切片，F309 只替换仓内具名旧调用，F503 核对采用的归一化规则与旧摘要；未覆盖部分交 T-09 具名登记 |
| 文档与注释 | T-07 | F409、F411、F413、F414、F422；F201／F204 的链与枚举 |
| 公共面 | T-08／T-09 | T-08 只承担逐符号兼容新增、具名仓内协调入口与 F122/F405 的窗口记录；F306 的宏类定义统一、F310 的全面公共面清理、F316 的应用分层残余及 F406/F407 的诊断行为不由窄迁移自动完成，T-09 须分别提交具名处置并取得接受 |
| 内核与时间 | T-10／T-12 | F303、F314、F410、F436、F504、F507、F219；F308 的时间统一与 F505 的旧入口随 R4 |
| 成品 | T-13 | F432、F502、F303、F304、F305 的残余；F431 明确覆盖旧 loader 与后继源入口的超限停止验证 |
| 会话与观察 | T-14 | F417、F420、F307、F430、F134、F211–F213、D-07 |
| Player | T-15 | F136、F142、F418、F506、F215 |
| Writer | T-16 | F401、F402、F433、F403 |
| 会话正确性 | T-17 | F501、F508、F509 |
| 无卡，T-09 具名延后 | T-09 | F302、F313、F404、F408、F412、F415、F416、F421、F427、F428、F429、F434、F435、F437、F438、F143、F203、F205–F207、F209、F210、F216–F218、F220、F132、F133 |
| 后继阶段 | Stage 14／R2／R4／7.5 | 退役核验（F405、F408、F412 的删除）、R2 公共面、R4 时间统一、Stage 7.5 预算；F510 为候选能力，待 T-10／T-14 采用时定是否晋升 |

2026-11-23 续议：上表保留当日承接快照；无卡、后继及晋升的当前处置由[§24](#24-统一实施前设计裁定2026-11-23)取代，不再自动延期。

## 23. 正式实施前计划检查（2026-11-22）

输入为 `db6042b6`；方法、逐项落点与实际门禁见[检查报告](../../../stage_reports/stages/stage-07-rr/readiness/2026-11-22-pre-implementation-plan-check.md)。本轮只订正文档承接和验收，不启动生产实现。下表的“成立”指所列基线缺口存在；文档补齐不代表对应产品缺陷已修。既有 RR-F 与 D 项编号保持原样。

<!-- cuexis-findings: definitions -->
| ID | 原始发现 | 原始依据 | 核验状态 | 处置状态 | 责任/任务 | 当前处置依据 |
| --- | --- | --- | --- | --- | --- | --- |
| RR-F539 | 原严重度：P2<br>[调度](schedule.md)中 T-17 没有进入 T-09 的前置闭包，但 A-05 要求 A-06 证据<br>原结论：成立 | 原处置与剩余条件：已核实；补 T-09 对 T-17 的直接前置，波次不变。修订后闭包覆盖其余全部现行卡 | 未复核 | 开放 | T-09 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F540 | 原严重度：P2<br>[T-06](tasks/T-06.md)与[T-17](tasks/T-17.md)重复承接 F508；T-17 使用默认关闭候选功能的 headless 配置验证候选字段<br>原结论：成立 | 原处置与剩余条件：已核实；五字段卸载清理唯一归 T-17，T-06 保留本卡画幅清理；T-17 按候选配置核验注册、构建和执行 | 未复核 | 开放 | T-06/T-17 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F541 | 原严重度：P2<br>T-06 的 F419 测试未进入执行命令，首帧必非零与合法零耗时冲突；F135 缺具名故障注入与预期<br>原结论：成立 | 原处置与剩余条件：已核实；在现有测试目标补执行入口和可控时间对照，F135 的稳定码修订已于 2026-11-23 按 D-04 接受，当前处置见下文。历史报告不按新读法改写 | 未复核 | 开放 | T-06 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F542 | 原严重度：P2<br>§22 把 F306/F310/F316/F405–F407 全归 T-08，但卡只做兼容新增；其他卡也有 ID 归口宽于实际步骤、验收的问题<br>原结论：成立 | 原处置与剩余条件：已核实；§22 明确各卡切片与残余，现有范围内补追踪；T-09 对未覆盖项逐条提交具名处置，不能以窄迁移或候选替代自动关闭旧问题。D-20 发行语料单列交接 | 未复核 | 开放 | T-08/T-09 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F543 | 原严重度：P2<br>[T-11](tasks/T-11.md)在[T-12](tasks/T-12.md)创建共享时间实现前要求量化后几何及运行结果验收；表 ID 唯一性与源/成品规范顺序分工未完整写入验收<br>原结论：成立 | 原处置与剩余条件：已核实；原始字段与几何归 T-11，共享量化、关系校验和实际输出归 T-12，成品规范顺序拒绝归 T-13；不提前创建或复制时间换算 | 未复核 | 开放 | T-11/T-12/T-13 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F544 | 原严重度：P2<br>[T-10](tasks/T-10.md)未逐项验收合同的预检优先级、状态/槽不变及封定后结果交付不分配保证<br>原结论：成立 | 原处置与剩余条件：已核实；复用已有推进用例补拒绝矩阵、交付段分配观察与拥有型结果寿命。内部 Fold 等已有分配边界保持原合同 | 未复核 | 开放 | T-10 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F545 | 原严重度：P2<br>F431 在 §22 分给 T-11/T-13，但两卡缺少原动画解析超限后停止后续工作的验收<br>原结论：成立 | 原处置与剩余条件：已核实；T-13 点名原问题路径及预算边界用例。新成品的限额检查不能替代原路径的修复证据 | 未复核 | 开放 | T-13 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |
| RR-F546 | 原严重度：P2<br>[T-14](tasks/T-14.md)仅点名 F420，未执行已有精确计数/保守下界规则；D-12 音频对齐负例、D-13 Retry 各入口、D-14 双用途恢复亦缺明确卡级断言<br>原结论：成立 | 原处置与剩余条件：已核实；在 T-14/T-15 既有用例内补齐，不新建观察系统、时间阈值或结果队列 | 未复核 | 开放 | T-14/T-15 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |

文字订正不另分配 ID：计划的当前任务、目录路由、变更表缺列、T-09 对 F420 的过期待裁陈述和交接的当前 HEAD 均按现行事实订正。O2 改为区分接受状态与实现进度；O6 的现有定义和 O12 的计划说明足以用于本轮追踪，未增加术语表或调度列。

RR-F525 的当前决定以 2026-11-23 owner 续议为准：设备操作的等待计时、超时判断和超时后的处置由各平台 Player 实现并决定；SDK 提供操作状态与实际结果，不实现设备等待超时机制、策略或默认期限。Player 可选择无期限或适合本平台的期限，系统或驱动已返回的错误仍按实际失败处理。该决定取代 2026-11-22 的“不新增统一超时、保留设备原有规则”；附页 D-09 与呈现合同已同步。T-14 验无期限和两种不同 Player 期限下的同一设备响应，T-15 验等待及超时后的窗口操作；500ms 只作无期限对照的观测窗口，测试期限不成为发布默认。窗口复用既有通道非阻塞查询，owner 可先接收重开或退出意图；超时及用户请求均不能证明取消完成、设备回滚或安全恢复，仍须按原事务确认、清理和 ACK 收束。§17.3 的待复核记录保留原时点状态，以本条为当前处置。其余 `NEXT` 技术细化保留原采用门槛。F501 与 F135 的后续裁定见下文。

2026-11-23 owner 接受 F501 推荐：`valid()` 按实际内容类别识别合法候选，GameplayOnly 无需占位 Runtime；内容信息、语义身份及适用音频访问器可用，依赖表现的操作另验能力。空、移后及错线程对象仍无效。当前代码的 `commit()` 独立检查会话归属、generation、生命周期和 fence，因此早期“commit 不校验”只适用于它没有调用 `valid()`，不能理解为没有提交门禁。早期 §9.1 路径简称 `prepareGameplayLoad` 亦不作为本轮复现入口；T-17 使用 typed Packed factory 的 GameplayOnly 意图后调用 `prepareLoad`。有效对象不保证提交成功，验收须区分对象有效性与提交资格。权威语义见[有效性合同](../../../api/playback-session.md#preparedplayback-有效性修订f501已接受未实现)，落实归 T-17/A-06；缺陷尚待实现与运行验证。

同日 owner 接受 F135 推荐：观测桥操作因实际分配失败而未完成时使用 `host.rpa.allocation_failed`，真正预算/容量拒绝保留 `host.rpa.capacity_exceeded`；复用预留故障槽，包含错误交付在内的报告路径不得再次分配。§2.4 的“三种互斥处置”和“吞成成功”需按职责区分：单纯观测丢失允许原执行继续，但须留下不完整/Unknown 标记；它不能证明提交或呈现成功，实际失败操作仍须报告失败。catch-all 也不等于已确认的分配失败。权威语义见[错误合同](../../../api/realtime-host-boundary.md#f135-观测桥错误分类修订已接受未实现)，T-03 绑定映射、T-06 实施和注入验收；范围限于观测桥，缺陷尚待实现与运行验证。两项已满足本次 D-04 设计接受条件，不再等待相同裁定；其他合同变更继续按 D-04 办理。

## 24. 统一实施前设计裁定（2026-11-23）

owner 要求先解决影响 T-01–T-17 一同实施的未裁定事项，再安排实施顺序与实施安排。本节登记这一步的决定及必要事实条件；本轮裁定完成后已重排调度，生产实施与发布均未启动。既有 §23 的设计修订继续有效。 本轮核验记录见[裁定与任务安排报告](../../../stage_reports/stages/stage-07-rr/decisions/2026-11-23-design-decisions-and-schedule.md)。

<!-- cuexis-findings: definitions -->
| ID | 原始发现 | 原始依据 | 核验状态 | 处置状态 | 责任/任务 | 当前处置依据 |
| --- | --- | --- | --- | --- | --- | --- |
| RR-F547 | owner 明确沿用原 [Chart v5 设计](../chart-format-update-for-v5/plan.md)，判定新候选的旧语法特许为错误；[成品合同](contracts/prepared-artifact.md#源输入与-lowering)、T-11/T-13 与夹具须统一为 CXT v2、Clip v2、Behavior Event v2 和 render.alpha，旧格式独立兼容 | 原议题：v5 作者表现语法（RR-F547）<br>原状态：2026-11-23 已裁定，合同与任务已订正；产品未实施 | 未复核 | 开放 | T-11/T-13 | 依据：[§22 归口](#22-旧问题承接2026-11-22)与[§24 后续处置](#24-统一实施前设计裁定2026-11-23)；迁移说明：保留原始字段，本轮未取得实现关闭证据；原设计接受继续有效，T-09 按证据核退出 |

| 议题 | 当前依据与处理 | 状态 |
| --- | --- | --- |
| 新旧玩法与发行范围 | owner 在后续路线续议中接受 v3 为 Stage 8 正式主线，替代先前 A 方案的 v2 正式目标；RR 仍完整实现并实验验证 v3，Stage 8 完成正式化，已承诺旧内容兼容保留。详见下文及[运行合同](contracts/runtime-timing.md#范围与授权) | 2026-11-23 续议已接受；实现与正式验收未完成 |
| v5 作者表现语法（RR-F547） | owner 明确沿用原 [Chart v5 设计](../chart-format-update-for-v5/plan.md)，判定新候选的旧语法特许为错误；[成品合同](contracts/prepared-artifact.md#源输入与-lowering)、T-11/T-13 与夹具须统一为 CXT v2、Clip v2、Behavior Event v2 和 render.alpha，旧格式独立兼容 | 2026-11-23 已裁定，合同与任务已订正；产品未实施 |
| SDK 公开与旧 API 退役 | 沿用 [D-19](architecture-design.md#d-19-sdk-示例与宿主试用)已接受的实验入口、逐组兼容新增、实际过渡发布与稳定后退出规则；未发布、未取得消费者证据时不能宣布满足退出条件 | 既有设计已决；具体版本与退出证据在实际发布时核定 |
| 临时覆盖失败语义（F203） | owner 选择登记前整批验证；失败不发布 token、不改变已有覆盖，合法 token 生命周期保持原规则，落实归 T-17 | 2026-11-23 已接受、未实现 |
| 旧问题与兼容语料 | D-20 绑定 T-01/T-10/T-13；owner 选择方案 5，F302、F310、F313/F316、F207 的全面结构清理全部纳入本轮，补齐设计、任务与验收；旧公共 API 仍按既有兼容窗口退出 | 2026-11-23 范围已接受，技术细化与任务验收已纳入；未实施 |

v5 作者语法以 owner 原设计及本次明确裁定为准，Gameplay v3 不增设旧动画特许。新候选中接受 CXT v1、Clip v1、Behavior Event v1/Keyframe 与旧 opacity 作者写法的条款属于设计错误；旧 v4 及更早版本仍走各自兼容路径。当前 CXT v2 Core 展开器和 schema 尚未提供完整 Animation 导入与绑定，不能把旧 CXT v1 helper 冒充此能力；缺失的有界源适配、模块/导出绑定和拒绝向量在 T-13 设计内明确，T-11 负责基础 v5 接受集。既有内部动画求值与混合机制继续复用，不新增另一套动画运行时。

D-20 沿既定旧谱兼容原则落实最小责任，不新增支持组合或语料框架：T-01 绑定旧交付文件、规则、输入和独立预期；T-10 验旧内核结果与结算；T-13 直读旧成品，证明不依赖源工程或重编译；T-09 核对证据，不再整体延期。尚未找到的历史发布资料属于具体证据缺口；不能以当前输出代替预期，不能把候选夹具升级为正式发行承诺。发现原规则与历史预期实质冲突时，按 D-20 对该具名条目提交新旧行为裁定。

SDK 的接口组和替代签名已由[逐符号迁移合同](contracts/runtime-timing.md#rr-4-逐符号兼容迁移)给定。实验实现、默认 SDK 晋升、旧接口删除分别取得对应证据；具体发行版本由实际发布基线按[版本规则](../../../guides/VERSIONING.md#sdk-版本递增规范)确定，不能在本轮无发布证据时预先认定通过。仓内私有接口在迁移与验证完成后可清理，已安装公共类、符号或布局继续遵守原兼容边界。

### Stage 8 正式主线续议（2026-11-23）

owner 在后续计划调整后明确接受推荐方案：Stage 8 以 **Chart v5＋Gameplay v3** 为正式发行主线，
新创作默认输出及经过验证的播放组合围绕 v3 收敛。此决定取代先前 A 方案中“Stage 8 保留 v2 正式目标”的部分；
RR 实现与实验验证、旧内容兼容和公共 API 发布过渡条件继续有效，历史报告保留当时结论。

RR 交付实验实现和证据，Stage 7.5 以 v3 扩展高级能力，Stage 8 完成正式规范、默认 Writer、成品编码、
容量/平台/安装消费者与发行组合验收。正式编码以 RR 实际接受的后继成品为收敛输入，不再为完整 v2 新发行
重复实现另一套动画/成品链路；ANM0 等旧编码只按实际兼容范围承担保留与回归，不因此批准删除旧 Reader。
原 v5 的 CXT2、Clip2、Behavior Event2 和 alpha 作者规则继续适用，CXC 容器保持 v1。

已承诺支持的旧内容沿原版本、规则、时间映射与身份运行，并使用旧交付文件及独立预期核验；
旧候选 fixture 不因此获得新的正式支持承诺。迁移须显式执行，无法保持原判定/计分语义时不能称为无损转换，
不能通过自动重编译旧内容满足兼容。Stage 8 正式目标获接受不代表实际发行通过，也不自动公开全部实验接口、
删除旧 API 或解除延期的新实时 seek；需要的正式 SDK 入口按 D-19 取得实际消费者与成熟度证据。

### 残余问题的实际承接

本次静态复核覆盖 §22 的全部无卡 ID 及公共面残余。下表订正原问题的适用边界，不改写原始记录；“成立 / 部分成立 / 不成立”指所述问题判断，任务补齐不等于实现修复。T-09 按实际证据逐项关闭或保留，不能把所有残余统一记为延期。

| ID | 复核结论与边界 | 任务与验收落点 |
| --- | --- | --- |
| F132、F133 | 部分成立 / 成立：身份构造已有 ruleset/session 配置，算法标记归属仍需说明；旧毫秒采样入口确实缺拍位上下文 | T-03 核身份算法与配置归属，T-10 保留旧字节/回放对照；T-07 明示旧采样限制，T-12 让需要拍位的仓内调用使用已有完整 sample，并验旧 event/step。禁止凭缺省值反推拍位 |
| F143、F216、F217、F220 | 部分成立 / 成立 / 部分成立 / 不成立：Windows 路径编码问题仍需修复；配置初始化与 shader cache 测试接线需补；依赖许可超集不等于门禁失效 | T-16 验非 ASCII、空格与参数往返；T-04 验全新缓存配置和 cache 独立测试，并在相关目标变更时收窄具名许可，不启用不需要的 shader 编译器 |
| F203 | 成立：非法值可留在临时覆盖记录中，导致后续更新失败 | owner 于 2026-11-23 接受登记前按既有类型/值域整批验证；失败不发布 token、不改变既有覆盖。T-17 补 Runtime 写域与原子登记正反例；不新增值域、接口或回滚框架 |
| F415、F421、F427、F428 | 部分成立 / 部分成立 / 成立 / 成立：内部 epoch 副本、GL 缓存初始化前提、detach 线程守卫与无真实设备资源却报告恢复成功分别核对 | T-06 仅清理确认无读者的内部副本，保留底层失效机制；验 owner 初始化、错线程、失效 context/window；T-14 验失败不会发布恢复成功。完整 epoch 影响在实施中核对，不预先批准整套删除 |
| F406、F407 | 部分成立：Playback 初始 membership 不是完整宿主账本；九个回执字段有不同参与方，不能要求每一方都填满 | T-07 订正职责说明；T-14 对照 membership、账本、查询及 participant/phase，仅补适用字段的缺失，不再建状态机或用默认成功值填空 |
| F412、F209、F408、F210 | 部分成立：参考宿主死分支与未生效字段/类型不能由窄公共迁移自动关闭，也不据零消费直接删除 | T-07 明示实际生效边界；T-08 验参考宿主错误路径，并为公共项登记所属迁移组。功能扩展不在此修复中；已装枚举、字段与符号退出沿 D-19 |
| F404、F416、F205 | 不成立 / 部分成立 / 部分成立：Schema 的 Ok 与实例 diagnostics 分工已有合同；rollback 有恢复实现和测试；诊断追加的 bool 与截断标记须按协议理解 | T-07 核对接口用法、事务测试及截断说明；仅在实际需要完整 diagnostics 的调用补检查。T-09 按事实订正分类，不强行把内部事务接口接入生产 |
| F429、F434、F437、F438 | 部分成立 / 部分成立 / 部分成立 / 不成立：日志承诺须与安全路径一致；历史摘要分支不自动构成缺陷；现有线程政策不承诺所有 Release 误用都受检；无法构造 Error 的 OOM 仍按既有 Fatal 政策 | T-07 核注释、五个观察入口前提及政策引用；T-04 顺相关摘要测试验越界拒绝，保留历史算法。不把 F135 的无分配报告保证扩大到全部 Result |
| F435 | 不成立：合法 GameplayOnly 可以 Ready，表现能力另验 | T-17 复用 F501 的有效性、内容与表现拒绝验收，不新增状态或占位 World |
| F206、F218 | 不成立 / 部分成立：额外分配不等于异常安全缺陷；particles 构建文件已不存在，Studio stub 未接入不等于应启用 | T-07 核实现并订正事实；保持现有构造与未启用功能，T-09 记录保留理由。日后有实际分配瓶颈证据或 Studio 首次接入时由对应维护任务处理，不列为本轮必修功能 |
| F306、F405、F510 | 成立 / 成立 / 部分成立：兼容新增不等于旧宏成员退出、默认安装泄漏关闭或默认 SDK 晋升 | T-08 交逐组替代和消费者证据；T-09 列清实际发布、替代稳定性、独立安装消费者及退出/晋升接受条件。责任为对应 SDK 接口组维护者；新 seek 仍按运行合同的既有延期处理，无须预先虚构版本或发布证据 |
| F302、F310、F313、F316、F207 | 成立 / 部分成立 / 部分成立 / 成立 / 成立：完整旧 Chart 构建隔离、表外公共面、全应用分层和资源加载去重未被现有窄写域全部覆盖 | owner 选择方案 5 全部纳入：F302 归 T-04，F310 归 T-08，F313/F316 由 T-08 的内部入口与门禁、T-14 的完整应用分层共同承接，F207 归 T-06。完整写域与验收见各卡，已统一排期；公共接口删除仍受 D-19 实际发布窗口约束 |

owner 已于 2026-11-23 选择全部纳入本轮；该决定取代上述结构清理仅作后继维护的推荐，未批准新的延期。完整实施安排须包含结构清理与 T-10–T-16，不再沿用 §22 的无卡自动延期或候选阶段外移交。旧公共接口受实际发布窗口限制的退出动作继续单列，不以结构清理授权提前删除。

## 25. 实施接手复查（2026-11-23）

owner 要求再次检查 plan 与 task 中会阻碍 agent 执行的决策、待定项或不确定内容，确认后编写接手文档。本轮实读全部任务卡及对应合同，按输入、创建责任、依赖、正反验收、兼容与停止条件核对；报告和独立复核回执见[实施可执行性复查](../../../stage_reports/stages/stage-07-rr/handoffs/2026-11-23-execution-readiness-handoff.md)。

本次修订沿用既有裁定：清除 API 统一退役阶段、重复审批和候选另行移交的残留；补 T-01 的工作树原文交付及旧兼容资料处置；纠正任务验收与合同不一致的范围，并明确新用例的创建、构建、注册和执行。局部实现选择由 agent 决定；新事实确需改变产品规则、范围或兼容承诺时，只暂停受影响切片。

最终核验结果见报告及 [HANDOFF](../../../HANDOFF.md)。任务卡完整不代表产品运行通过，旧交付资料与实际发布门禁不能预认证。历史待审记录保留当时时点，由 §23/§24 的后续裁定及本节指向的当前任务承接，不作为重复询问的理由。

## 26. 文档治理归属与当前状态整理（2026-11-23）

owner 明确文档管理与整理应归入 RR。方案现作为[文档治理分册](documentation-governance.md)收录，由 RR 总计划和阶段目录提供入口；G-01–G-05 仅是待审分册的拟议步骤，治理规则接受、任务卡/统一调度接入与实际实施仍待后续明确安排。

本轮精简 CURRENT_STATUS 和计划总索引中的历史过程，保留仍有效的受限退出、性能、计时、内存/GPU/P8、P7 与 F3 限制，并链接原始责任及证据。状态合同此前要求 CURRENT_STATUS 保留 Stage 7A/S7A-3 的旧短句，本轮仅将该页对应检查项改为阶段/工作项/登记册元数据键和当前任务、阶段状态、遗留边界章节，避免重新硬锁具体阶段或“未启动”等临时状态；这些局部结构检查不证明值或行为正确，其余状态合同和通用检查器补强仍按待审分册处理。

本项只记录文档归属、摘要与检查配置的局部订正，不声明治理检查器、报告迁移或产品修复完成；验证状态见 [HANDOFF](../../../HANDOFF.md#验证状态)。

## 27. 文档治理实施（2026-11-23）

owner 已接受文档治理推荐方案并要求独立完成，执行归 T-18；实际范围、原始输入、检查器回执、迁移对照与限制见[交付报告](../../../stage_reports/stages/stage-07-rr/implementation/2026-11-23-documentation-governance.md)。本轮只执行文档治理，产品任务另按既有调度采用。

登记结构迁移保留原字段、旧处置说法和冻结输入。当前责任按已接受承接归口；可通过当前文档及已提交快照核实的文档订正恢复对应核验与处置，需产品运行或缺原始证据的项保持开放，不把原说法删除或补造验证。具体状态见定义行，交付报告解释本轮证明边界。

## 28. T-01 基线与派单（2026-11-23）

T-01 按[采用入口](plan.md#唯一派单授权入口)绑定实施基线 `a22e255f6716b22f96ad8a183136eae4fb04ac2f`（tree `7869115460b0c17a1f833f30700cd06f34e8e3c3`，工作树干净），交付派单清单、调度正反例、独立冷启动与 D-20 旧内容清单。证据、命令与口径见[基线与派单报告](../../../stage_reports/stages/stage-07-rr/readiness/2026-11-23-t-01-dispatch-baseline.md)。

- **派单口径成立**：规范哈希取基线提交的 blob 字节，191 个唯一输入、17 张卡、缺失 0；Windows `core.autocrlf` 造成的工作树行尾差异不影响该值，`manifest.json` 同时保留两种哈希。
- **D-20 部分成立**：仓库内具名正式发行判定组合为 0；唯一存储态旧判定成品是 S7A-7/8 证据包中的 `main.cxc`（内部交付身份），L2–L4 的输入与预期内嵌在测试源码，F1/F2 无判定内容且字节 golden 由 production Writer 生成，R1 已被现行 Reader 拒绝。历史发行资料是否存在无法核实；正式发行回归就现有证据不适用，具名缺口交 T-09，内部候选不得升级为正式支持承诺。核对见[旧谱兼容语料清单](../../../stage_reports/stages/stage-07-rr/readiness/2026-11-23-legacy-corpus-inventory.md)。
- **T-13 冷启动缺口已补注**：shared 导出白名单未进写域、独立消费者工程未标创建责任、RR-F431 用例未标新增、D-20 清单无指针四项，均按卡内规则改卡；同一冷启动代理的复验回执随修订提交登记。不改产品规则、wire、接受集或兼容窗口。T-10 的 D-20 条目同步补指针。
- **边界**：本节的“已绑定”只表示输入可定位，不表示任何产品行为已验证；构建、CTest、安装消费者与设备证据随各责任任务取得。

## 29. 命名硬规则（2026-11-23）

owner 要求生产源码、安装头与构建系统文件不使用阶段名或任务名，测试与工具可以；“全部都改掉，然后加上硬规则”。

- 规则写入 [CODE_POLICY](../../../guides/CODE_POLICY.md#文件与命名)，检查器 `tools/verification/check_source_naming.py` 随 `python -B tools/check_docs.py` 运行。扫描 `engine/`、`app/`、`cmake/` 的路径段，禁止 token 为 `rr*`、`s<数字><字母>`、`stage<N>`、`task<N>`、`t<两位数字>`、`f<2–3 位数字>`、`cfu`、`pd<N>`；`tests/`、`tools/` 不扫描。
- 两张计划中的门禁脚本改为领域名：T-04 的 `VerifyPlaybackCandidateIsolation.cmake`、T-08 的 `VerifySdkFacadeCompat.cmake`，任务卡已同步；对应测试目标名不变。
- 四个既有 `cmake/Verify{S4GPerformance,S5HPerformance,CfuF3Determinism,CfuF4Performance}.cmake` 保留：约 107 处冻结验收清单按路径与内容 SHA-256 绑定，改名或改内容会让历史记录永久失配。它们按路径登记在 [source_naming_exceptions](../../../governance_rules.json)，不构成新文件的命名依据。
- 本规则不改产品行为、wire 或兼容窗口，只约束新文件与后续重命名。

## 30. T-03 合同与诊断（2026-11-23）

T-03（scope `RR2`）完成既有合同对照与回归取证，见[交付报告](../../../stage_reports/stages/stage-07-rr/implementation/2026-11-23-t-03-contracts-and-diagnostics.md)。本轮只订正文档、追加对照测试，未改 `engine/`、`app/` 生产源码。

- 合同订正 8 篇：packed Header CRC 只覆盖前 92 字节且旧算法值必须被拒；TIME/REQ0 非 0 mode 的稳定拒绝与 GridDelta 仅测试地位；Chart Beat 上限改用 `ChartLimits::maxBeatNumeratorMagnitude`/`maxBeatDenominator` 并写明约分后判定与分子符号两侧；帧摘要拆成宿主可观察默认版本与内部定版本；candidate chart entry 的 playback encoding 接受集。
- 回归：新增 `rr2_contract_packed`、`rr2_contract_entry`、`rr2_contract_beat`、`rr2_contract_input`，在 candidate-debug 实测通过（CTest #121/#170/#326/#488，四次单测与两条 RPA B3 对照均退出 0）；既有旧 profile 断言未改。
- **RR-F131 分层纠正（保留原记录）**：普通规范化只生成有序观测；旧执行 profile 按其 §8 在路由前拒绝同原 tick 的不同 subject；RPA 按 occurrence 槽与 T2 规则接纳。原登记把两个稳定码判作互换，来自入口混用后的错误归并；三个入口各自的稳定码与首错次序不变，生产输入逻辑不改。
- 移交：F135 的生产修复与五类故障注入归 T-06；F132 的旧身份字节由 T-10 在改动前 dump 并比对；RR-4 安装/宏头 diff 与逐符号迁移归 T-08；RR-F142 归 T-15；RR-F121 的测试重复编译归 T-04。
- 未执行：F135 故障注入、安装消费者、F132 旧回放字节对照；“通过”只覆盖上述四个用例与两条 RPA 对照。

## 31. 外部旧设备验收包与 D-20 口径（2026-11-23）

owner 提供两套仓库外设备验收包，清点见[外部旧设备验收包清点](../../../stage_reports/stages/stage-07-rr/readiness/2026-11-23-external-legacy-corpus.md)。

- 包 A（2026-11-14，SDK `0.9.1`，源码 `d1951326`）与包 B（2026-10-19，SDK `0.9.0`，`d3440882` 加 81 项未提交输入）都不是正式发行：无 tag、发行说明、发布清单或对外交付记录，包内分别自记 `formal_acceptance: false` 与“不预先记为验收通过”。
- 两包齐备 Chart/Gameplay 成品、配置、tick 输入序列与聚合期望：A dense 为 778/387/0、−387/0/387、768/385/2，B 为 46/21/0、−21/0/21、36/19/2，均可由 `scoreRules` 推出；但没有逐音符期望、replay 字节与设备输入时间戳。
- D-20 的“正式发行判定语料”仍为空集且无法核实；可执行回归输入与聚合期望部分成立，作为 T-09/T-10/T-13 的外部输入，不写成发行通过。
- F132 的历史身份字节不能从两包取得（无 `judgementIdentity` 记录，只有 Player 会话配置身份 `ff578f4f…`）。本机旧副本与 11-14 存档的身份代码与当前 HEAD 逐字节相同，改用 `rr_f132_identity_baseline` 在 T-10 改动前后各取一次字节。
- 仓库记录的原 P7 DFJK `40/19/2` 在两包中不存在；包内有完整末态的 DFJK 为 34/15/0、43/20/1、46/21/0。差异登记，不据此改写历史报告。
- 口径：D-20 按“无正式发行身份、内部候选语料可回归”记录；Stage 8 正式化前的兼容承诺仍需复核。

## 32. 候选 hooks 拆分移交 T-08（2026-11-23）

T-04a 实施 R1 构建隔离时发现，`playback_session_gameplay.cpp` 与 `playback_session_prepare_gameplay.cpp` 的候选 hook 入口要独立成文件，必须让默认闭包不看见候选 hook 声明并访问私有 State，需要动内部头或公共头边界；T-04 卡面禁止本轮改公共头。

owner 2026-11-23 裁定：移交 T-08，与 RR-F122 的候选宏、安装面和逐符号迁移同批处理。T-04 保持现状并在证据登记，T-08 卡已加入对应条目：拆分后默认闭包不含候选 hook 声明，实验闭包每个 hook 仍只有一个定义，用 `cuexis_rr_playback_isolation` 与安装消费者同一套证据核验。

## 33. D-20 旧语料双侧重放（2026-11-24）

两套外部包的判定语料在旧实现（`d1951326`）与当前实现（`3943fb13`，Debug）上分别重放，结果见[当前实现重放](../../../stage_reports/stages/stage-07-rr/evidence/external-corpus/replay-current/replay-current.md)与 `out/review/stage-07-rr/T-01/legacy-corpus-external/replay-old/report.md`。

- 包 A `keyboard-dense-v5`：两侧都输出 778/387/0、−387/0/387、768/385/2，`replay=equal`；三行拼接 166 字节，SHA-256 `3c6723af…901c`，与包内 `verification.log` 逐字节相同。
- 包 B `keyboard` 与 `keyboard-release`（同一课程两个入口）：当前实现输出 46/21/0、−21/0/21、36/19/2；CRLF 归一后 157 字节，SHA-256 `a124c5fd…51ab`，与包内 `verification.json.logicalCases` 相同；用 `d1951326` 补跑同一输入，stdout 与当前逐字节相同。
- 两侧驱动源码是同一 blob `35af0a91…`；`d1951326..3943fb13` 的 65 个提交里 `engine/judgement` 与 `engine/gameplay` 无文件差异，差异集中在 `engine/playback` 准备路径与 chart/cxc 读取。
- 原样完整 R4 用例文件（160,354 字节）仍超过工具内 128 KiB 解析预算，两侧都 exit 2、同一条 `json.parse.size_limit`；单例拆分路径成立，完整文件路径记为不成立。
- 这批语料可以充当当前实现的内部回归输入，但聚合值相同只证明 score/hits/misses 与同构建 replay 自洽；逐音符判定窗口、跨实现 replay 字节与设备输入仍无法核实。

## 34. T-04 构建闭包拆分与隔离门禁（2026-11-24）

T-04 交付见[构建闭包拆分与隔离门禁](../../../stage_reports/stages/stage-07-rr/implementation/2026-11-24-t-04-build-isolation.md)。

- RR-F302 处置：**已关闭**。`cuexis_chart` 只保留基础 Reader/CXT1/v4 Resolver 源表与 include 根；codec 与 tools 拆成独立内部静态库，各自只含其消费合同内的源与 include 根；每个 Chart TU 只归属一个库，门禁在四个预设与 `rr-f302-minimal`/`rr-f302-tools` 两个独立配置按实际配置通过。
- RR-F216 处置：**在 Windows 可核实范围关闭**。启用 observer 且不显式传库类型时默认 STATIC 配置通过；显式 SHARED 在 Windows 被拒绝，首错 `PD allocation observer requires Windows STATIC`。非 Windows 分支需要非 Windows 主机，未核实。
- RR-F217 处置：**已关闭**。GL ON/shader tools OFF 下 `cuexis_shader_tests` 构建成功，6 个 cache 用例注册并全部通过；codemodel 显示该目标不含 shader compiler 源与 shaderc 链接边。本机无 tools ON + tests ON + GL ON 配置，compiler/cache 对照未做，记为遗留。
- RR-F434 处置：**已关闭**。摘要版本 0 与 4 被 `playback.frame_digest.version_unsupported` 拒绝；默认 v3 与历史 v1/v2 的摘要值未变。
- 新发现（门禁自身）：`cuexis_target_dependency_names()` 把 File API `dependencies` 元素当字符串读取，导致基础 Chart 的链接边检查恒为空，A2 阴性在候选配置假通过；修复为读取 `id` 成员（`92539ad4`），修复后 A2 按预期失败。该缺陷由阴性对照发现，说明拆分门禁需要定期用反对照自检。
- 遗留：候选 hook 文件拆分按 §32 移交 T-08；安装消费者的 `find_package` 接线由本卡脚本直接指向前缀，`VerifyGameplayInstalledConsumer` 只做前缀侧断言；全部运行是 Windows Debug，Release 与非 Windows 未覆盖；设备与实体输入不在本卡。

## 35. 波次 5：T-02、T-05、T-17（2026-11-24）

三张卡按唯一调度并行完成，交付报告分别是[T-02 状态事实对齐](../../../stage_reports/stages/stage-07-rr/implementation/2026-11-24-t-02-state-facts.md)、[T-05 像素几何与 GL 边界](../../../stage_reports/stages/stage-07-rr/implementation/2026-11-24-t-05-frame-layout.md)与[T-17 会话层正确性修复](../../../stage_reports/stages/stage-07-rr/implementation/2026-11-24-t-17-session-correctness.md)。

- RR-F001/F002/F101/F102/F103/F104/F424：**已修**。CURRENT_STATUS 版本与分支摘要、AGENTS 与 ROADMAP/V2 Spec 旧阶段叙述、SDK API `0.8.0`→`0.9.1`、MATERIAL_SHADER 当前与历史版本分离，四项文档门禁退出 0，负例注入按预期失败。
- RR-F105：**部分成立**。Packed Reader/Writer 已实现、默认 Playback/Player 入口未消费；`[packed]` 69 例全过。真实外部 Packed 消费者与 Writer 互操作仍归 T-09。
- 新发现（T-04 拆分遗漏）：`SPEC-02` 在 T-04b 后变红，原因是 guard 移入新单元 `packed_chart_tables_common.cpp`，`packed_budget_tests.cpp` 的源单元列表未同步。波次 5 集成时补入第五个单元，`[packed]` 恢复 69/69。以后移动源文件时要搜测试里的按文件读取列表。
- RR-F203/F501/F508/F509：**已修**。Prepared owner 生命周期由 lease 守卫（失效 `host.rpa.context_stale`）；unload 清零五个宿主导出与授权字段；Presentation 有效性按实际 prepared 内容判断；临时覆盖在发布 token 前整批校验（失败 `world.property.value_invalid` 等，无部分覆盖）。新增 4 个用例，候选预设下新用例与既有 override 集全过，默认闭包未回退。
- T-05 的 R5 几何与 GL：**部分成立**。几何/选择、Q/R/Pout 与完整逆算、借用 FBO 的 clear/draw/区域外 sentinel/状态归还、五类附件负例、draw failure 恢复与两 flavor 导出成立；`framebuffer_incomplete` 在本机驱动不可构造（驱动对 draw/read buffer 指向缺失附件仍返回 COMPLETE），记为无法核实，留 T-06 或设备/驱动 lane。
- 集成验证：合并树上 `candidate-headless-debug` 的 T-17 三例、override 集与 `[packed]` 69/69 通过；`debug` 的 `rr_frame_layout` 4/4、`[packed]` 69/69、隔离门禁与 T-03/T-04 七项测试通过。
- 遗留：T-04 卡预期的两个候选 hook 文件仍未拆分，维持 §32 移交 T-08；T-17 未跑 shared 与设备；T-05 的 `framebuffer_incomplete` 需要合同补可达条件或由 T-06 覆盖。

## 36. 波次 6：T-06 部分成立、F508 冲突与 GL 边界缺口（2026-11-24）

T-06 按卡实施，交付范围只到 Playback 帧对象与 GL 实验桥，宿主/Player 接线不在本轮结果内，按四档记**部分成立**。

- RR-F135 场景 1/2/3/5：**成立**。分配失败分类、实测预算容量、非致命丢失与 submit/present 真实性在 `rr_frame_observation_errors` 通过；固化 reason/stage 的故障槽读取 0 次分配。
- RR-F135 场景 4：**不成立**。持续 OOM 下失败交付仍抛 `std::bad_alloc`，根因是 owning `core::Error` 的拷贝会分配。固定 `FaultSlot` 可用，但现有 owning 错误类型不是不分配表示；需要 D-04 先裁定错误表示，再谈闭环。
- 帧对象与提帧：**成立**。`LaidOutFrame`、`preparedFramings`、`extractLaidOutFrame`、`relayoutRetainedFrame` 的同 owner 布局/快照/来源/token、Pout/fovY/viewport 与 retained 重排不重读 Runtime 均有 `rr_laid_out_frame` 覆盖；非法画幅在 validate 与物化前都拒绝。
- GL 实验桥：**部分成立**。缺能力/非法表拒绝、target/token/画幅成员校验、借用 FBO 绘制与状态归还在 `rr_frame_layout_gl_bridge` 通过；窗口路径只编译，未做真实提交对照。
- 实验安装面与共享导出：**成立**。仅实验 flavor 安装 Playback 实验头，默认不含；两个 `cuexis_shared_export_surface` 通过，dumpbin 实测 11 个新符号。
- 未实施：ExecutionHost/Player 帧链（`FramePayload` variant、worker owner、`frame_delivery`、deadline、target-changed 弃帧）、`rr_frame_layout_shared`、`rr_frame_diagnostics`、F207 资源去重与 F419/F427/F428/F421/F415。这些仍是 T-06 卡内后续步骤，不能按通过计。
- 集成更正：T-06 报告的 B4/R0“基线缺陷”不成立。实测在 T-17 提交之前两例通过，之后 7 个 worker 用例（B4、R0、B5×2、C10、C4、R1）一起变红，根因是 `PlaybackSession::unload` 清零 `hostFence`/`acceptedHostControl` 后，定向 Close 与 retire 幂等重放失去依据。回退该清零后 7 例全部恢复，其余 T-17 修复（F501/F509/F203）不受影响。
- RR-F508 因此**保持开放**。清零与幂等重放需要同一个 replay ledger：保留已应用的 retire/close 回执（含 effects 与 generation），让重复步骤在不再持有 live control 时返回原记录。没有这层设计时，清零会破坏 R1 的关闭与重放合同；这是本合同冲突，不是测试问题。
- GL 边界缺口：T-05 与 T-06 的渲染测试直接 `#include <glad/glad.h>` 并调用 `gl*`，违反 `VerifyArchitecture.cmake` 的“GL 头与调用只在 render_opengl”规则，`cuexis_architecture_tests` 变红。修复中（见 task-25）。

## 37. CI 红灯与文档基线冲突（2026-11-24）

- MinGW 与 GCC 构建：`open_gl_frame_layout_bridge.cpp` 的聚合初始化缺 `DrawSummary` 初值，`-Werror=missing-field-initializers` 直接失败。已补默认成员初值（`12d60eab`），MSVC 不报但 GCC/Clang 会报。
- 覆盖率与 Release 构建：`cuexis_rr_playback_isolation` 在 clean configure 下没有 File API codemodel，测试固定失败。门禁改为仅当构建目录已有 codemodel 回执时注册测试，并在状态行说明启用方式（`6510d7c7`）；T-04 的专用 harness 配置仍跑门禁，普通预设不再误报。CI 侧的 codemodel 车道仍缺失，属后续补强项，不能按“CI 已覆盖隔离门禁”计。
- `Documentation contracts` 的 PR 事件运行失败：PR 47 的基线是 `master` 的 `aac83e5e`，该提交还没有 `docs/MAP.md`、`docs/governance_rules.json` 与登记册；历史保护检查要求受保护文件与 legacy 豁免在可信基线中已存在，因此整个治理树在同一个 PR 中首次落地时必然失败。push 事件的运行以 `event.before` 为基线，能通过。这不是代码缺陷，不能用放宽门禁的方式绕过；需要 owner 决定治理树的合入顺序（例如先单独落入基线）或调整该 PR 的检查基线。

## 38. T-06 第二轮：宿主/Player 帧链（2026-11-24）

- `FramePayload` 变体、envelope 视图与 `payloadBytes`/`frameByteBound`：**成立**。`FramePayload` 尺寸由 328 B 增至 904 B（MSVC x64 debug），PD-5 的元素尺寸与容量基线需要按新口径重生成，本轮未做。
- worker 布局提取/重排、同 target 别名、空目标 `skippedEmptyLayout`、模式不符 `noRetainedFrame`：**部分成立**。两向 retained 模式与请求选择差异没有独立用例。
- `FrameRequest` 的 `target`/`framing`、指纹与准入忽略旧 viewport：**成立**。
- Player binding 到同一 `OpenGlBackend`、普通循环与 ExecutionHost 装配、宿主 `prepareLaidOutOwned` 与画幅表私有传递：**成立**。
- 宿主 deadline 弃帧与 target-changed 弃帧：**成立**。边界内一次 submit+present，到点零提交且保 retained；target 逐字段比较 extent/host/rotation，变化即弃帧，不挂起。
- 非满区 host/旋转的嵌入来源、`resize` 触发弃帧、非空作者画幅表端到端：**部分成立**。只测了 `setFrameTarget`；正式 source 目前只产生空表，依赖 T-11/T-13。
- `rr_frame_layout_shared`：**部分成立**。两个 shared 预设的真实 Player smoke 与仓内消费者通过（消费者 digest 一致，`Completed frames: 6`）；安装消费者 opt-in 与导出表负向变异未做。
- RR-F419/`rr_frame_diagnostics`、F207、F427/F428、F421/F415：**不成立**（未实施）。`renderMicroseconds` 赋值先于 `notePresented` 的修复单独成立。
- 验证：`cuexis_execution_host_tests` 68/68、candidate-headless `rpa` 182/182、`cuexis_playback_tests` debug 137 例、candidate 193 例、debug 布局 6/6、shared 两预设各 2/2、架构与隔离门禁 3/3。
- 全局格式门禁：本轮改动的源码里 17 个文件未按 clang-format 输出，已统一格式化（`a977e01a`），`cuexis_format_check` 恢复通过。

## 39. T-08 独立 RR-4 公共面（2026-11-24）

- 候选实验 facade（`gameplay`、`gameplay_realtime`、`gameplay_replay`）与逐符号转发、候选 `FILE_SET` 与默认安装拒绝：**成立**。默认前缀编译实验头必须失败、未 opt in 的 `find_package` 必须被拒，两条负例都实际命中。
- `detail::ExecutionHostAccess` build-only 入口、`CUEXIS_PLAYBACK_API` 逐函数导出、三处 friend 与 `host_candidate.cpp` 消费同步：**成立**。`rr_sdk_execution_host_shared` 证明 Player 与测试只 import 同一个候选 `cuexis_playback` DLL，bridge 21 条、真实 host Pause/Resume/Seek/Close 68 条断言通过。
- 候选 hooks 拆分：**成立**。默认闭包不含候选 hook 声明，实验闭包每个 hook 只有一个定义，`cuexis_rr_playback_isolation` 与安装消费者同一套证据核验。
- replay identity 与 frame layout 的窄内部入口迁移、F313/F316 具名 include 封闭：**成立**。全应用反向依赖与目录搬迁按卡面留 T-14。
- RR-F209/F412：**成立**。reference host 改为 Result 驱动，不再等待未进入的 Playback `SessionState::Failed`；公开枚举保留，删除仍走所属接口组退出条件。
- 共享导出对照：候选 219→275（新增 56，移除 0，其中 39 个实验符号、17 个内部符号，各含一个两重载）；默认 109→109（0 增 0 减）。候选门禁正例退出 0，两个负例分别以缺符号与默认拒绝候选 detail 符号退出 1。
- 四配置安装消费者、facade 兼容消费者、reference-host staging：**成立**（复用既有构建目录，未做 `--fresh` 与全量 CTest）。`--fresh` 四配置与全量 CTest 连同符号移除变异负例列为 T-09 补证项，不能按已通过计。
- 风险面：实验 shared 新增 56 个导出符号，是限定策略变更；候选 facade 尚未获准发布，保留候选面，不预填退役版本。

## 40. T-06 第三轮：诊断、资源去重与 retained 用例（2026-11-24）

- RR-F419/`rr_frame_diagnostics`：**成立**。注册在既有 `cuexis_player_control_tests`，常规路径断言 `renderMicroseconds` 有限非负，受控 2 ms renderer 首帧即报 ≥1000 µs，`notePresented` 与 `validatePresented` 看到同一值，证明赋值先于钩子。`0` 的语义按“低于微秒分辨率的一次真实测量”表述，不再承诺恰好为零。
- F207 五类资源去重：**成立**。`resource_scope.cpp` 的五段 typed load 与五段 fallback 合并为两个按 Tag 分派的模板，Entry/Lease variant、索引、回滚边界与诊断 severity/code/context/cause 排序全部保留；新增五类共用路径与 depth/type/fallback 用例，资源用例 14/14 通过。`fallback_failed` 与 scope 级 `dependency_cycle` 在当前构造下不可达，记无法核实并附原因。
- RR-F428：**成立**。假后端 close 后 `rebuild()`/`resize()` 返回 `presentation.renderer.closed` 且 generation 不前进；真实 GL 释放后返回 `render.opengl.backend_unavailable`，后续 submit/present 仍失败。原用例把“close 后 rebuild 成功”编码成预期，已按合同改正。
- RR-F421：**成立**。能力缓存改为已发布缓存任意线程直读、首次初始化只允许 owner 主线程且活 context、非 owner 未初始化查询返回未初始化集，真实 GL 前后两次跨线程取证。
- RR-F427：**部分成立**。owner 路径（析构、move-assign 入站、discard）用例成立；跨线程 detach 实现为改状态前 `std::terminate()`，进程内无断言可测，未做子进程死亡探针。
- RR-F415：**部分成立**。读者普查显示 `mutationEpoch` 生产 0 读者，唯一读者是 PD 寿命探针的等值断言；按卡面“仅清理确认无用途的副本”条件未满足，保留字段并逐条引用证据。
- retained 两向模式与请求选择差异：**成立**。legacy handle+有 target、layout handle+`nullopt` 两向都在帧 ID/attempt/reservation 前记终态 `noRetainedFrame`（新增 `FrameDelivery::refuse`），重复请求同终态，随后匹配模式复用原 handle/serial；Default handle 时 BestFit 请求被拒、同选择复用。
- CI 适配：`rr_frame_layout_shared` 在无 OpenGL 上下文的宿主上把真实 Player smoke 记为 NOT CHECKED 而不是产品失败；本地 NVIDIA 上下文仍真实执行。`cuexis_rr_sdk_facade_compat` 需要 vcvars 环境，未加载 MSVC 环境时嵌套 configure 会因缺库失败并误报，运行说明需写明这一点。

## 附录 处置记录（按日期保留，§12–§16）

本附录保留各轮审查的处置记录，按当时时点成立，不改写结论；编号沿用历史顺序，故排在 §17 之后阅读。

### 12. 提案复核记录（2026-11-21）

本节保留上一轮文档处置的原有结论；本轮收敛与验收状态见 §13。

本轮以 `ed1537fd` 为文档修订依据，核对本地 `Cuexis-Stage-7-RR-PR47-审查报告-2026-11-21.md` 与源码、台账和 owner 已接受共识。该外部报告是待验材料；P1–P6 是提案编号，与本册严重度编号无关。按 owner 本轮补设计授权，P1–P5 五项设计缺口均已有待审推荐；下表只记录核对判断，详细设计见链接。

| 项 | 结论 | 核对与文档处置 | 证据与详细设计 |
| --- | --- | --- | --- |
| P1 | 部分成立 | 作者类型、头链、target 分离与成品装载校验确有边界缺口；“必须至少四个 target”未获证明，仅链接依赖也不足以满足已接受目标。D-06/D-10 已订正为拆出的运行内核在头文件与链接上仅依赖 `core`；类型、构建与安装、装载、当次输出及旧 SDK 迁移已补待审方案。 | [当前头链](../../../../engine/judgement/include/cuexis/judgement/kernel_types.hpp)、[装载校验](../../../../engine/gameplay_packed/src/gameplay_capsule_read.cpp)；[D-21](architecture-design.md#d-21-谱面表现与判定共用拍位换算)、[P1](simplification-proposal.md#p1-数据与-sdk) |
| P2 | 部分成立 | 原 D-23 的约数示例未冻结整数取整；把约数与 P2 待审 floor 规则判为已接受合同的硬冲突不成立。具体矩形、帧布局与整数用例已补待审方案，不在台账预先选定。 | [D-23](architecture-design.md#d-23-谱面画面比例与宿主尺寸)、[P2](simplification-proposal.md#p2-渲染与兼容迁移) |
| P3 | 部分成立 | 报告混用 Chart 的定拍位停顿与 Judgement 的拍位区间冻结/终点跳变两种模型；不能直接用旧区间 fixture 推断新模型必然改变同一接受集。新 Chart 的量化退化、碰撞、负拍/零拍边界及 clamp/seek 已补待审方案；旧零拍相邻区间样例保留，单区间跨零沿旧规则。 | [Chart 表示](../../../../engine/chart/include/cuexis/chart/timing_map.hpp)、[Judgement 表示](../../../../engine/judgement/include/cuexis/judgement/timebase_profile.hpp)；[D-21](architecture-design.md#d-21-谱面表现与判定共用拍位换算)、[P3](simplification-proposal.md#p3-时间与会话时钟) |
| P4 | 部分成立 | 台账确有漂移；owner 已接受目标 `R=Retry`、Reload 另键，F5 仍为推荐。源码 `R=Reload` 是未迁移现状；“Retry 键未定”的判断已不适用。Failed 装载恢复、设备状态矩阵、旧 Rebuild 职责迁移及无音轨与启动交互已补待审方案；Empty 仍只走首次 Load。 | [当前按键接线](../../../../app/player/src/player_assembly.cpp)；[D-13](architecture-design.md)、[P4](simplification-proposal.md#p4-player-与旧控制权) |
| P5 | 部分成立 | 损失接口、首批目标版本、旧签名同批退出，以及输入到转换模型的损失记录与 Writer 损失合并，均已补待审方案；写入前报告具体已知损失、覆盖由 Studio／第三方决定的原则已获接受，不再列为未决归属。 | [当前 Writer](../../../../engine/chart/include/cuexis/chart/chart_writer.hpp)；[P5](simplification-proposal.md#p5-写回与诊断)、[D-19 保存职责](architecture-design.md#d-19-sdk-示例与宿主试用) |
| P6 | 部分成立 | plan 漏登 D-23–D-25、D-15 局间保留漏同步成立，本轮作摘要订正并标明历史替代。D-05 仅限定 RR-3；RR-4 独立前置和实施授权边界已明列。历史基线与当前文档依据须分别绑定，不能据相差提交数推断全部源码事实失效。 | [台账与实施状态](plan.md#4-已确认方向与决策台账)、[D-15](architecture-design.md)、[P6](simplification-proposal.md#p6-迁移与验收) |

历史研究继续绑定 `aac83e5e`。本轮实跑 `git diff --name-status aac83e5e..ed1537fd -- engine app tests CMakeLists.txt CMakePresets.json cmake/CuexisVersion.cmake vcpkg.json`，列出变化仅为根 CMake、Presets、版本、`engine/debug/CMakeLists.txt`、删除 `engine/particles/CMakeLists.txt`、`tests/pd/CMakeLists.txt` 与 `vcpkg.json`；该范围内 `engine/chart`、`engine/judgement`、`engine/playback` 及 `app/player` 实现源码无差异。这是有限差异核对，未重跑历史证据命令，不证明当前构建、安装面或运行验收通过。

本轮修正状态为文档订正与待审设计补齐，实现未验；详细提案由[附页](architecture-design.md)和[提案](simplification-proposal.md)维护，新增 P1–P6 技术推荐仍待审。本轮统一文档校验状态与结果以 [HANDOFF](../../../HANDOFF.md) 为准。

### 13. 可实施性审查处置（2026-11-21）

输入为本地 `pr47-stage-07-rr-plan-review-2026-11-21.md`（PR #47，`9623efa321fe488e9c6b8549f7059283020c1861`）。报告指出合同缺口及拆分未落地，**成立**；对照落点逐项登记在[可实施性索引](design-readiness.md#缺口追踪)。本轮推荐已具体化为候选合同，拆分已落盘；owner 已接受方向继续由[附页](architecture-design.md)维护。采用和实施证据分别见[计划](plan.md#唯一派单授权入口)、[验收](acceptance.md)。不改写 §12 的历史裁定。

历史研究绑定 `aac83e5e`；`ed1537fd` 是上一轮文档对照。当前设计核对使用 `9623efa3`。本轮静态比较 `git diff --name-status aac83e5e..9623efa3 -- engine app tests CMakeLists.txt CMakePresets.json cmake tools vcpkg.json`，可见根 CMake、候选 preset、版本及清单、debug CMake、`engine/particles/CMakeLists.txt` 删除、PD 测试 CMake、`tools/CMakeLists.txt` 与验证工具变化；其中工具构建登记含 `cuexis_asset_importer`。该范围内 Chart/Judgement/Playback/Player 实现源码无差异。此核对不沿用旧安装、运行或构建证据。实施核验按 [T-01](tasks/T-01.md) 记录实际 SHA、变化对象及需重跑用例。

本轮验证以 [HANDOFF](../../../HANDOFF.md#验证状态) 为准。新增技术实现、安装和运行验收未开始；延期与范围不随设计补齐而变化。

### 14. 自主技术收敛与任务对齐（2026-11-22）

本轮承接 §13 的审查基线，在 owner 已授权自主细化设计的范围内收敛合同、对齐任务并开展独立冷启动审查。已有共识由[架构附页](architecture-design.md)保留；本轮补充的技术细节由代理按该边界形成，不记为 owner 逐字接受，不改变历史审查及受限退出结论。

架构附页已接入运行、时间与判定，画幅、Player 与写回，以及后继成品和 CXC 源包的候选合同。任务与证据要求见[架构附页](architecture-design.md#6-已接受的行为边界与候选合同入口)、[可实施性索引](design-readiness.md)及[验收](acceptance.md)。

三合同与 16 卡已对齐；四组无父上下文首轮审查发现、作者补全及原审查者最终复验记录在[本轮可实施性报告](../../../stage_reports/stages/stage-07-rr/verification/2026-11-22-design-readiness-review.md)。设计判断为**成立**：Huygens、Epicurus、Bacon、Nash 全部成立，无本轮未决合同项。Epicurus 三处 fixture 与原六项规则均闭合；Bacon 定义体 typed span／stable merge 及关联 fixture 阻断关闭。报告转录四组 tool 回执作为耐久记录，并列出每卡首改、测试、正反例和停止条件的冷启动定位证据。

Einstein 独立调度阴性与集成路由核对在其快照内成立；主代理 18 项独立 Fraction/整数算例成立，位宽为 160/128 位。主代理本批链接／标题锚、尾空白与保护段检查成立，证据路径、哈希与限制见报告。主代理已生成 MAP／AGENTS 并完成最终组合文档检查，文档、正式事实检查、16 卡调度、17 项检查器测试及 diff 全部 exit 0；实际日志位于 `out/review/stage-07-rr/T-01/main-final-20261122/`。本次唯一补录已完成，设计收敛及本单元文档工作收尾并停止修改。Einstein 随后 `--capture` 封存最终文件及独立再验，固定证据目录 `out/review/stage-07-rr/T-01/final-20261122/` 的读取口径见报告，不再回写本批文件。本轮生产实现未开始，文档门禁、算术复算和设计审查不替代运行验收或阶段采用；D-05 与 NEXT 采用规则不变。验证路由见 [HANDOFF](../../../HANDOFF.md#验证状态)。

### 15. Astra 执行者复审（2026-11-22）

首次复审时，合并结论为**部分成立**。[当期报告](../../../stage_reports/stages/stage-07-rr/verification/2026-11-22-astra-executor-review.md#结果与证据)列出逐项证据、触发、最小修正和两项撤回记录。以下九项在首次登记时均待收敛。

- P1：AX-01 默认 shared 帧调用；AX-02 裸 Reader 身份参照；AX-03 RPA 作者测试依赖；AX-04 Retry 生产命令前置。
- P2：AX-05 同 tick 接纳与提交；AX-06 成功窗阶段序号；AX-07 Graph 相机键；AX-08 迁移 CLI 验收配置；AX-09 其他打包工具帮助归属。

#### Astra 九项设计修订

AX-01 至 AX-09 均已写入候选合同或任务卡；逐项落点、实施时的正反例及交叉复验回执见[修订报告](../../../stage_reports/stages/stage-07-rr/verification/2026-11-22-astra-review-resolution.md)。生产未实施。

AX-05 / RR-F131：首次登记把两个入口的稳定码判作互换；首次复审又误以为同一原 tick 的不同 subject 能在旧 submit 触发 dispatch collision。普通规范化 (`false`) 与旧执行 (`true`) 各有既定首错；后者在路由前已拒绝同原 tick 的不同 subject。用原 tick 900/901 汇到 901 才触发本例的 dispatch collision。保留旧 profile §8 与既有生产输入逻辑，RPA occurrence 分开验收。见 [T-03](tasks/T-03.md#任务级验收)和[纠正依据](../../../stage_reports/stages/stage-07-rr/verification/2026-11-22-astra-review-resolution.md#结果与证据)。

### 16. 独立 checkout 复审处置（2026-11-22）

owner 转交对 `8599c83a` 的复审，按 IR-01–IR-20 分开核对类型与构建、数据生产、渲染接线、宿主控制、任务与证据。逐项裁定、修订及实际验证见[本轮报告](../../../stage_reports/stages/stage-07-rr/verification/2026-11-22-checkout-review-resolution.md)。§14/15 保留原时点判断，旧九项通过不代表本轮新发现已经覆盖。

主代理已复现七段卡结构的漏检并补检查器、对应反例及 active/candidate 区分；T-01 改为跨平台哈希和显式采用基线。候选 schema、诊断码与目标尚待任务交付的事实，按创建责任及前置是否完整判断，不把尚未实施本身登记为设计失败。A-N 仍只移交候选设计；本次核实不扩大 D-05 或后继采用范围。
