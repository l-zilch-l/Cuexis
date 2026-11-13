# R4 门账更新（2026-11-13 收尾中）

状态：status ledger；不是退出接受记录，也不替代 CURRENT_STATUS

初版日期：2026-11-08；更新日期：2026-11-13

返回 [New RPA 报告索引](../README.md)。本文保留原门账路径，按最新裁定更新当前门状态；
历史原始测量、失败和运行身份保持不改。判据与协议以
[实施方案](../../../../../stage_plans/active/stage-07/05-rpa-new/r4/plan.md)和
[固定运行登记](../../../../../stage_plans/active/stage-07/05-rpa-new/r4/formal-registration.md)为准。
本轮执行边界见[自动收尾裁定](2026-11-13-closeout-decisions.md)，上一版移交准备见
[F4/PD 移交门账](2026-11-07-f4-gate-and-handoff.md)。

## 1. 当前候选与目标

当前工作树为 D:/Cuexis-worktree，分支 codex/r4-closeout，以 origin/master 的
85ebf64a 为本轮起点；收尾候选日期构建为 26.11.13-1，SDK API 为 0.9.1。
当前工具与探针已完成 scratch 编译和有限诊断，尚未冻结新候选或完成正式组。
DFJK 功能诊断通过；memory/P8 因完整语义图缺失保持拒绝正式准入。见
[实施复核](2026-11-13-closeout-implementation.md)。

目标是在完成自动实施、取证、落档与最终验证后，只保留 P7 听音、O2 实体操作、
F3 校准与生产参数接受、O3／PD-5 退出接受。**目前尚未达到该目标**；下表中的自动门
不能因目标已获授权而删除、改判通过或转写为仅需人工。

## 2. 全门状态

| 计划单元 | 当前结果或实施状态 | 剩余门与证据边界 |
| --- | --- | --- |
| R4-0 起点 | 历史起点审计保留；本轮起点为 85ebf64a | 本轮最终候选、源码／工具／fixture／profile／二进制身份仍须冻结 |
| R4-1/2 B3/B4/B5 | 既有机制与可控反例保留 | 新候选完整矩阵、实际线程／设备适用项仍须核验，旧局部结果不外推 |
| R4-3 O1 | 方案 B 的 64/2048/40K reference 设计 caps 已接受，原数字保持 | 设计 caps 不是完整堆上界或生产接受；修订数值与生产 profile 尚未接受 |
| R4-4 工具 | CPU、host、controls、Seek、device capture、P3、容量和 Player 工具已集成；scratch 编译、DFJK 与设备 n64 诊断完成 | 尚未取得干净候选正式组；memory/P8 完整语义准入缺失时阻断，不因原点精确或报告合入而视为通过 |
| R4-5 P0 legacy/RPA | 固定计时与 read_long 均已结案；legacy 后续 validated_trace／playback_only 也通过 | 只覆盖对应固定 CPU 工作的相对成本／重复性；首组失败、不同来源的 A 与后续组不合并 |
| R4-5 P0 host | 三窗口九进程 request→ready p99 为 87.9–99.1 µs，9/9 通过 4.167 ms 预算；两条外层腿重复性通过 | ready→present-return（约 4 µs）重复性 inconclusive 保留；无 GPU/audio，不代替 P3/P7；该单元次数已用尽 |
| R4-5 P1/P2/P4-CPU/P6 | 七个 CPU 探针单元都有三窗口结果；六个计时组重复性通过；p4-capture-1 全九 16.7% 的分辨率受限 inconclusive 已获接受 | 不再重跑 p4-capture-1；探针只报结构计数与重复性，维持无新增绝对预算 |
| R4-5 P3 renderer | 独立真实 renderer 自动证据仍缺 | 单 renderer、U:F=1:1 的原内容／时间／viewport，实际 request→ready、CPU submit 与 present-return；不能由 P4 的 renderer 加 capture 负载代验 |
| R4-5 P4-device | device adapter 已准备；真实设备身份包含一个 renderer 与额外计数，backend 独立键已在工作树实现 | capture-1/2/4 的本轮编译、pilot、注册与三窗口正式组尚未完成；设备默认各两次，历史 CPU 键与次数不变 |
| R4-5 P5 controls | 仍用独立脚本化探针；累计最多五次，前三次已消耗，尚无完整三窗口正式结论 | 第四／五次须有效补救、新 preparation 与 claim/seal；核对 73 条实际样本及控制绝对门，有有效结论后停止，不要求耗尽次数 |
| R4-5 五类 Seek | 五类 20 ms 绝对门全部通过，最差 1,014 µs；natural／hot／external 重复性通过；fallback 为 20.27%、cold 为 39.92%／35.92%，重复性 inconclusive | 按本轮裁定不再重跑、不放宽判据，两项限制接受留给 O3；只覆盖报告登记的设备与端点范围 |
| R4-5 阶段／GPU 时间 | D2/D3/Snapshot/queue 的可开关计时例外保持，真实 GPU timer 门仍待本轮证据 | 独立进程的逐进程 p99、同负载开销对照、GPU query 完整性与实际边界仍须核验；CPU submit／VSync／ACK 不冒作 GPU 时间 |
| R4-5 memory | 已有链接期 C++ 分配精确分区；40K 命令／执行域归属修正已获接受；小 profile 的 state.current 历史超出保留 | n64/n2048 low/high、prepare 峰值、完整生命周期／最终释放、storage／进程口径、GPU backend／driver、observer 扰动仍未闭合；40K 在声明内不能消除小 profile 超出 |
| R4-5 P8 | 正式内容口径为 (40000, 4096)；已有 high 完整规模结果与 low candidate 单进程报告，归属修正未改总字节或 caps | low 只改变 requirement 结束 tick，不是通用低复用／冷缓存上限；现存运行身份、次数、绑定与适用边界仍须整理，不据此宣告最终接受 |
| R4-5 P7 自动轨迹 | 真实音乐与独立 DFJK 的本轮自动证据未完成 | 分别绑定实际 renderer／音频端点、脚本、内容、时长、计数与环境；设备枚举或合成无音频探针不能代验 |
| R4-6 F1 | 历史候选 2c4e1274 的十一行全部 local_pass | 仅认证原 SHA；本轮新工具／探针不继承，最终候选须完成新的十一行矩阵 |
| R4-6 F2 | 2c4e1274 同 SHA 三平台 hosted success 的历史结果保留 | 本轮最终候选须取得与 F1 同 SHA 的 Linux Quality、Windows MSVC、Windows MinGW 证据 |
| Version Gate | 2c4e1274 的 success 保持原身份 | 新候选按实际 PR/base/candidate/tree/UTC 日期重新核验；评论授权不等于 gate 已通过 |
| R4-7 O2／P7 听音 | 实体清单已准备；本轮实体事实尚未接受 | owner 完成听音、实体操作及所需仪器观察，或明确接受限制 |
| R4-7 F3／生产 | 当前仍为 testOnly；生产参数只获准作为校准目标 | 校准与具体生产 profile／参数接受，不能由 reference 通过代签 |
| R4-7 F4／O3／PD 退出 | 移交准备存在，最终退出未接受 | 完成自动门账、失败／缺证据和责任清单后，owner 接受最终退出范围及 PD-5 移交 |

## 3. 已接受的处置

| 事项 | 现行处置 | 记录 |
| --- | --- | --- |
| CPU p4-capture-1 | 接受分辨率受限的 inconclusive，不重跑、不改判据 | [探针批报告](2026-11-08-probe-matrix-admission-and-environment-stop.md)、[11/08 waiver](2026-11-08-owner-attempt-waiver.json) |
| CPU 探针绝对预算 | 维持无新增阈值；B-only 不伪造共同 A | [普通 host 报告](2026-11-08-common-host-unit.md)、本表的对应适用范围 |
| P5 形态与次数 | 保留探针形态；前三次现场不替换；最多五次，剩余第四／第五次按原规则准备 | [控制探针](2026-11-11-p5-controls-unit.md)、[工具停止记录](2026-11-13-p5-tooling-stops.md)、[本轮收尾裁定](2026-11-13-closeout-decisions.md) |
| 内存归属与 P8 内容 | 接受命令／执行域归属修正及 (40000, 4096) 内容口径；未修改上限数字 | [owner 裁定](2026-11-11-owner-decisions.md)、[P8 high 报告](2026-11-10-p8-40k-entity-memory.md) |
| P0 read_long 与共同 A | 具名重开与后续组结案；接受 d3440882 重建 A 的绑定口径，不合并不同组 | [相机逆矩阵整改](2026-11-11-read-long-camera-inverse.md)、[正式组](2026-11-12-read-long-formal-group.md)、[裁定记录](2026-11-12-read-long-ruling.md)、[P0 计时结案](2026-11-12-p0-trace-legacy.md) |
| Seek fallback/cold、device backend 与容量工具 | 两类 inconclusive 留给 O3；device 独立计次，最小扩展真实 GPU 容量工具 | [五类 Seek 结果](2026-11-13-seek-five-classes.md)、[本轮收尾裁定](2026-11-13-closeout-decisions.md) |
| PR 与评论 | 尽量集中，仅在推进必需或避免漫长失效重验时新开 PR；评论绑定精确身份 | [本轮收尾裁定](2026-11-13-closeout-decisions.md)；不推导合并、发行或退出接受 |

## 4. 内存、设备与 P8 的接受边界

O1 在 2026-11-05 接受的是方案 B 的 reference 设计 caps，不只接受延迟数值。机器表保留
accepted: null／formal_registration: false，用于区分外部数值裁定与正式注册；不能据此
反推“reference caps 未接受”。精确接受范围和 artifact hash 见
[O1 接受记录](2026-11-05-o1-budget-acceptance.json)与
[caps 裁定包](../../../../../stage_plans/active/stage-07/05-rpa-new/r4/caps-proposal.md)。
生产 profile 与[内存修订提案](2026-11-08-memory-budget-revision.md)的新增数值尚未接受。
本轮先补证据，保持既有数字，不从观测高水位反填上限。

[11/08 内存归因](2026-11-08-memory-attribution.md)记录 n64 的 state.current 为
427,015 B／196,608 B（2.17×），n2048 为 13,249,941 B／4,259,840 B（3.11×）。
这些历史超出仍需按各自 profile 处理；40K 在较宽 cap 内不构成其恢复证明。
n2048 的历史 command.session 超出、40K 的归属修正与之后的观测也须分别保留来源，
不能把后者直接套到未重验的小 profile。早先由 frameByteBound 推测实际交付缓冲分配的
解释已被逐分配证据撤回，现行解释见 P8 high 报告与 owner 裁定。

[GPU 内存单例](2026-11-13-device-adapter-and-gpu-memory.md)使用一实体 stream，不能代验
n2048 容量 profile；链接期 C++ new/delete 分区也不覆盖 GPU／驱动实占。
后续需用匹配规模、真实 renderer 与独立口径补证据，未观测域保持缺证据，不填实测 0。
prepare-only 峰值、observer 扰动、各阶段 owning handle 与最终释放亦不能由 active 单点替代。
原 World 缩页与扩大拥有覆盖的局部证据见
[缩页效果](2026-11-06-world-storage-effect.md)和[拥有覆盖／CPU 报告](2026-11-07-owning-and-cpu.md)。

[P8 low 报告](2026-11-13-p8-low.md)记录 f5f91916 上的一次 low 进程及同构建 high control；
该 low 变体只使 requirement 结束 tick 去相关。结果保持 candidate 单例范围，不称通用低复用
上限，不把它与旧 high 拼成重复性，也不据报告中的“在声明内”代替最终预算／设备接受。
本轮仍须核对原登记、运行绑定、次数与各物理／hash 门的实际适用性，不能把 P8 一项外推为全门。

## 5. 最终验证与移交

历史 2c4e1274 的 F1／F2／Version Gate 见
[最终候选报告](2026-11-10-f1-f2-final-candidate.md)；更早候选与交接见
[本轮拥有覆盖／CPU 报告](2026-11-07-owning-and-cpu.md)和
[交接留档](2026-11-07-session-handoff.md)。它们均保持原身份，不认证当前新增或恢复的工具。
本轮在实现与准入稳定后冻结新候选，完成十一行本地 F1 和同 SHA 三平台 F2，
再整理全部自动门的通过、超出、inconclusive 或未测量／缺证据。

最终包继续使用[O2 实体清单](2026-11-07-o2-physical-checklist.md)与
[F4/PD 移交准备](2026-11-07-f4-gate-and-handoff.md)，逐项列证据身份、未覆盖范围、
后续责任与恢复条件。P7 听音、O2 实体操作、F3 校准与生产参数、O3／PD-5 退出接受仍由
owner 完成；自动门尚未完成时，不先把门账压缩成只剩这四项。

本文的“通过”只适用于对应单元、指标、候选与协议；旧失败、慢值和停止现场不替换、
不事后改类。任何局部结果均不自动关闭 R4、RPA、Stage 7A、PD-5 或 Stage 7。
