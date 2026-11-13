# Cuexis Current Status

状态：current

更新日期：2026-11-14

本文只维护当前阶段、正在进行的任务和未解决的问题。实施与测量细节见带日期的阶段报告。

## 当前任务

- **R4 自动收尾进行中**：R1、R2、R3 已退出；按
  [本轮收尾裁定](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-13-closeout-decisions.md)
  继续取证、处置未定项、落档与最终验证。P5 第四次及设备后端首个三窗口组已完整执行，
  但仍有重复性 inconclusive；完整内存语义准入、GPU observer 准入、DFJK 功能失败、
  P7 normal 证据与最终候选 F1/F2 等自动事项尚未闭合。
  目标仍是完成自动工作后，仅保留 P7 听音、O2 实体操作、F3 校准与生产参数接受、
  O3／PD-5 退出接受；**目前尚未达到该目标**。详见
  [R4 门账](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-08-r4-gate-status.md)。
- **P0 已结案，Seek 保留两项限制**：legacy/RPA 的固定计时与 `read_long` 均已取得通过结论；
  重建 pre-PD A 的绑定口径已获接受，旧失败和不同组的样本保持独立。见
  [P0 计时结案](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-12-p0-trace-legacy.md)、
  [`read_long` 正式组](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-12-read-long-formal-group.md)及
  [裁定记录](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-12-read-long-ruling.md)。
  五类 Seek 的 20 ms 绝对门全部通过，`natural`／`hot`／`external` 重复性通过；
  `fallback`／`cold` 重复性仍为 inconclusive。本轮不再重跑、不放宽判据，限制接受留给 O3，见
  [五类 Seek 结果](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-13-seek-five-classes.md)。
- **CPU 探针结果保持，P5 第四次完整组仍有两项未定**：七个 CPU 探针已有三窗口结果；
  `p4-capture-1` 的分辨率受限 inconclusive 已获接受，不再重跑；探针仍只报结构计数与重复性，
  不新增绝对阈值。普通 host 的 request→ready 九进程 p99 已通过 4.167 ms 预算，
  ready→present-return 的重复性 inconclusive 保留。见
  [探针批报告](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-08-probe-matrix-admission-and-environment-stop.md)与
  [普通 host 报告](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-08-common-host-unit.md)。
  P5 attempt 4 在测量候选 `ce2a9a69` 上完成三窗口九进程，每进程 73 条 complete 样本；
  63/63 个逐进程绝对门通过，pause／resume／resize／reset_paused／reset_running 五组重复性通过，
  close／load_host 为 inconclusive，P5 整体尚未接受。累计最多五次，第四次已消耗、第五次未用；
  不替换旧失败，不为用尽额度自动启动第五次。见
  [P5 第四次完整组](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-14-p5-attempt4.md)。
- **容量与 P8 保持有限证据，正式内存门阻断**：O1 方案 B 的 64/2048/40K reference
  设计 caps、已接受的命令／执行域归属修正及 P8 `(40000, 4096)` 内容口径继续有效，原 caps 不变。
  本轮八个容量 profile 登记 48 次 invocation，得到 47 份配对 summary；七份 qualification 均 blocked，
  allocation-origin 与 semantic 总评级均 unrated。配对及 residency 合计保留两处环境停止，
  完整 ownership/lifetime 图未实现；小 profile 原点超额、prepare-only 上界、完整释放与 observer 扰动
  仍未闭合。n64 仅取得有限 sampled GPU residency，n2048 无可用 residency summary，均不构成 GPU observer 准入。
  P8 历史五份 invocation packet 保留各自候选与次数；旧 high 报告已追加字节来源勘误，额外 high 调用及追溯缺口仍待裁定，
  不据旧 high-3 文件证明修正后的归属数字。low 只使 requirement 结束 tick 去相关，不能外推通用低复用上限。
  见[容量与 Player 有限证据](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-14-capacity-and-player-bounded-evidence.md)、
  [O1 接受记录](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-05-o1-budget-acceptance.json)与
  [owner 裁定](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-11-owner-decisions.md)。
- **设备首个正式组完成，外层与 capture-4 重复性未定**：测量候选 `ce2a9a69` 上的独立 P3 与
  capture-1/2/4 完成三窗口、36 个 B 进程。P3 的 request_to_ready／close／load 绝对门各 9/9 通过；
  request_to_ready／close／load_host 重复性通过，cpu_submit／present_call／request_to_present_return
  重复性 inconclusive；capture-1/2 的 capture_delivery 重复性通过，capture-4 为 inconclusive，
  capture 无新增绝对门。CPU 历史次数不变，四个设备单元分别消耗首次 device claim。
  CPU submit 与 present-return 不作 GPU 执行时间；独立 observer 12 进程完成，四阶段在既有例外内数值门通过；GPU 仍未准入。
  见[设备首个正式组](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-14-device-first-formal-group.md)。
- **Player 功能轨迹有明确缺口，最终候选仍待验证**：原音乐约 129 秒活动轨迹功能 valid，
  但不是 normal 性能、听音或实体输入接受；原 DFJK 在本测量候选上为 40 分、19 命中、2 misses，
  未达到 46 分、21 命中、0 misses 的 oracle。旧 scratch 成功、Replay 一致和退出码不替代该失败；
  两条 trace 的正式计时次数均为 0，DFJK 适用的长活动证据仍缺，音乐 retention 适用性独立。
  见[容量与 Player 有限证据](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-14-capacity-and-player-bounded-evidence.md)。
  历史 `2c4e1274` 的十一行 F1、同 SHA 三平台 F2 与 Version Gate 不认证本轮候选；
  最终候选须在实施及文档稳定后冻结，再执行新的同 SHA F1/F2 和实际适用的 Version Gate。
- **R3 已关闭，证据范围保持**：门账为 15 项通过、0 项部分通过、0 项未判定；最终本地同源矩阵
  在 `d70df15a` 上为 11/11 行 `local_pass`、13,635 个用例、0 失败。
  owner 接受矩阵点到 hosted 点 `de73aa2c` 的限定 delta，后者 Linux Quality、Windows MSVC、
  Windows MinGW 验证通过。该裁定仅关闭 R3，不覆盖 R4、设备、生产接受或 Stage 7。
  R4 及以后仍要求同 SHA hosted 验证，除非另有 owner 裁定。见
  [R3 退出裁定](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-04-r3-hosted-delta-and-exit-ruling.md)、
  [最终矩阵证据](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-04-rpa-acceptance-matrix-final-evidence/index.md)。
- **R3c 候选不采用**：正式采用单元因执行失败未形成有效样本，结论为 `inconclusive`；
  R3c 补丁与 128 MiB quota 均未进入产品。R3b 可信恢复边界不重开；五类适用 Seek 的本地门已通过，
  不替代 R4 集成后的正式取证。见
  [最终验证与候选处置](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-04-r3-final-validation-and-r3c-decision.md)。
- **适用范围与预算裁定继续有效**：40K 不再作为 240 FPS 的适用规模；1080p 唯一登记容量仍为
  仅渲染 1800、静态提帧 1300，后续前缘阶梯只报告实测括区，不发布新容量值。
  128 MiB 不再作为 R3 上限判据；R4 的 reference 设计 caps 已获 O1 接受，
  正式 profile／工具注册、修订数值与生产接受继续独立核验。
  D2/D3/Snapshot/排队四项沿用可开关计时、归因与开销披露的限定例外，其余预算不外推。见
  [owner 四项门裁定](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-02-r3-owner-gate-rulings.md)、
  [最新容量前缘诊断](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-01-r3-capacity-frontier-rederive.md)。
- **PD-5 与 Stage 7-PD 退出仍待裁定**：历史结论为“未通过但定案”，退出清单尚待 owner 接受；
  读取、Seek 与关闭整改由 RPA 承接，旧 PD-5 窗口及其失败不追改。见
  [PD-5 判定与退出清单](stage_reports/stages/stage-07/04-pd/pd-5/exit/2026-10-24-pd5-verdict-and-stage-7-pd-exit.md)。

当前工作树为 `D:/Cuexis-worktree`，执行分支为 `codex/r4-closeout`，以 `origin/master` 的
`85ebf64a` 为本轮起点；日期构建为 `26.11.13-1`，SDK API 保持 `0.9.1`。
本轮已完成的 P5／设备测量绑定 `ce2a9a6952fd4a9a4d82949544f7ef3482e204e6`
（tree `f2ec992cd94aa32c838d5ee863bdbca0b37033d2`）；它不是已完成 F1/F2 的最终验证候选。
最终候选仍须在本轮证据落档和必要处置后冻结。PR 与评论授权按本轮收尾裁定执行，
不从该授权推导合并、发行或阶段关闭。
逐项推荐与最终验证结果的绑定入口见[收尾验证与待裁定移交](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-14-closeout-verification-and-handoff.md)。
迁移后的原始运行字节位置见[证据清单](stage_reports/stages/stage-07/05-rpa-new/evidence-catalog.md)。
源码拆分已完成，审计与复核材料见
[源码拆分安全网独立审计](stage_reports/stages/stage-07/90-shared/source-split-audit/index.md)；
逐轮实施、候选处置和测试记录从[Stage 7 报告索引](stage_reports/stages/stage-07/README.md)导航。

## 阶段状态

| 阶段 | 当前状态 | 入口 |
| --- | --- | --- |
| Stage 7A | S7A-3/4、5/6、7/8 的受限功能验收完成；整体关闭仍待 RPA 收尾 | [Stage 7A 计划](stage_plans/active/stage-07/01-a/plan.md) |
| Stage 7-RPA | R1、R2、R3 已退出；R4 自动收尾中。P0 已结案；P5／设备首组已执行但保留 inconclusive，完整内存与 observer 准入阻断、原 DFJK 失败及 P7 normal 缺口仍待处置；最终候选 F1/F2 未完成，尚未达到只剩人工项的移交条件 | [整改计划](stage_plans/active/stage-07/05-rpa-new/plan.md)、[R4 门账](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-08-r4-gate-status.md)、[收尾裁定](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-13-closeout-decisions.md) |
| Stage 7-PD | PD-0–4 内部实施及适用门禁完成；PD-5“未通过但定案”，退出清单待 owner 接受；分域内存及正式性能缺口由 RPA/PD 后续取证承接 | [PD 计划](stage_plans/active/stage-07/04-pd/plan.md)、[PD-5 判定报告](stage_reports/stages/stage-07/04-pd/pd-5/exit/2026-10-24-pd5-verdict-and-stage-7-pd-exit.md) |
| Stage 7B+ | 未开始，等待 RPA 完整退出 | [Stage 7B+ 计划](stage_plans/active/stage-07/06-b/plan.md) |

Stage 0–6 已完成；Stage 8–14 尚未开始。整体 Stage 7 未关闭。
阶段依赖与后续安排见[Stage 7 总计划](stage_plans/active/stage-07/plan.md)和[路线图](ROADMAP.md)。

## 当前问题与关闭前置

- **R4 集成与正式证据未完成**：B3/B4 可控机制、B5 实际部署和 F1–F4 对应门须按依赖核实与取证。
  正式矩阵沿用共同 A 和三窗口九对协议，重复性口径及每进程请求数按
  [整改计划 §9](stage_plans/active/stage-07/05-rpa-new/plan.md#9-r4集成后取证区分时间内存与设备)
  登记；R1/R2 局部结果与 R3 本地门不能拼接成 R4 接受。
- **分域内存仍缺完整闭合证据**：n64/n2048 的 `state.current` 历史观测分别为 427,015 B／
  13,249,941 B，对应原设计 cap 为 196,608 B／4,259,840 B；原失败保持，未提高数值。
  40K 归属修正只接受其命令／执行域的记账修正，不消除这些小 profile 的超出。
  本轮容量配对与 residency 有两处环境停止；完整语义图缺失，正式 memory/P8 仍 blocked。
  n64 的有限 sampled GPU residency 不覆盖 n2048，也不证明连续峰值或 observer 扰动接受；
  prepare-only 上界、完整生命周期与最终释放，以及 requested/storage/进程内存的独立口径仍未闭合。
  128-slot 的历史效果及扩大拥有覆盖的下界
  保留在[World 效果报告](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-06-world-storage-effect.md)与
  [拥有覆盖报告](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-07-owning-and-cpu.md)，不作为完整预算通过。
- **真实设备与生产接受未完成**：已有 PD-4 受限实体 DFJK 键盘及听音观察，精确音画同步、设备失效/恢复、
  压力验证、生产参数与预算仍待验收；Reset/暂停恢复附近的帧率波动持续时间及模块成本尚未量化。
  P3 与设备 P4 已有本轮完整组，但 P3 三项外层计时及 capture-4 重复性仍 inconclusive；
  四阶段例外下的数值门通过，GPU observer 准入尚未闭合。原音乐只具功能证据，原 DFJK 失败、P7 normal
  及适用长活动证据仍缺；这些自动事项不能移写成仅需听音或实体操作。
  实体参与及移交准备见[O2 清单](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-07-o2-physical-checklist.md)与
  [F4 门账](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-07-f4-gate-and-handoff.md)。
  R3 初次 load 的已通过读数不代表极端 40K 启动达标；40K 启动与 prepare 的有限证据见
  [启动可观测性与成本](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-01-r3-40k-startup-observability-and-cost.md)。
- **正式采样继续遵守冻结环境与次数**：交互会话、电源、前台和设备端点须满足对应单元登记；
  旧锁屏／前台变化及工具停止记录保留。新尝试须先绑定可核验补救与新 claim/seal，
  不用换候选、backend 误标或重命名重置历史次数，不替换失败进程。
- **历史限制与偶发故障保留**：PD-0 历史慢态及尾延迟未归因；R0 的 partial 和旧 Q4 `inconclusive`
  不因 R1/R2/R3 退出而回填。B5 Pause 的 `capture_gap` 仍未定位；R3c 正式采用尝试另保留重复 Pause
  失败记录。CXC unpack 已加入有限原子重试和确定性测试，原现场具体句柄持有者仍未知。见
  [PD-0 归档](stage_reports/stages/stage-07/04-pd/pd-0/2026-10-17-pd0-supplemental-retests-and-archive.md)、
  [R0 记录](stage_reports/stages/stage-07/05-rpa-new/r0/2026-10-24-r0-observation-and-anchor.md)、
  [整合记录 §3.2](stage_reports/stages/stage-07/90-shared/worktree-consolidation/2026-10-26-worktree-consolidation.md#32-shared-probe-and-cxc-output-commit-regression-repairs)。

## 产品边界

Cuexis 由可嵌入的 Playback SDK、独立 Cuexis Player 和 Cuexis Studio 构成。
外部宿主使用公共 Playback API；RuntimeSession、World、EnTT、SDL 和 OpenGL 属于内部或可选实现。

产品决策见[ADR 0027](adr/0027-playback-sdk-product-boundary.md)，公共接口见[API reference](api/README.md)。

## 当前格式与 SDK 合同

- 当前工作树 SDK API 为 **`0.9.1` preview**，日期构建为 **`26.11.13-1`**；本轮测量候选
  `ce2a9a69` 尚无最终 F1/F2 或发行结论。历史 `2c4e1274` / `26.11.09-1` 有同 SHA F1/F2 与
  Version Gate 证据，当前源码、工具、探针和版本变化不能自动继承它。Version Gate 仍按实际
  PR/base/candidate/tree/UTC 日期核验，SDK 批准是否适用以该检查的实际输入与结果为准。
  R3 来源绑定新增 candidate 类型和 `StepTarget` 字段，消费者须整体重建，不能混用旧二进制。
  PD-4 历史基线仍为 0.9.0。版本决定见[版本政策](guides/VERSIONING.md#091-r3-candidate-来源绑定)，
  STATIC/SHARED、安装消费者及兼容门的历史完整本地证据见上方 `2c4e1274` 的矩阵。
- Gameplay v2 是当前实施基线，顶层合同仍为 candidate；局部实现不等于整批验收。语义与运行边界见
  [Gameplay V2 Spec](formats/GAMEPLAY_V2_SPEC.md)、[ABI](api/GAMEPLAY_V2_ABI.md)及[RPA 补充合同](formats/rpa-execution-contract.md)。
- Chart v1–v4 的既有兼容路径保留；Chart v5、CXT v2、Packed Chart 与 Canonical Gameplay Graph 仍为显式候选路径，正式发行收敛归 Stage 8。
- 稳定 C ABI 归[Stage 14](stage_plans/future/stage-14/plan.md)，当前只提供 C++ typed preview。

各格式的生产/候选状态与权威文档见[格式索引](formats/README.md)。

## 延期和禁止边界

运行时脚本和逐帧 script callback 无限期延后，不预留字段、capability、bytecode、ABI 或 Playback hook。
离线 authoring generator 不会被 pack、prepare 或 Playback 隐式执行。

## 更新规则

只更新当前结论，完成或被替代的状态应就地更新或删除。
测试轮次、命令、提交经过、授权沟通和历史问题处置保存在阶段报告中，不向本文追加过程日志。
