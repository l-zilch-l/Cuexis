# R4 门账更新（2026-11-14 收尾中）

状态：status ledger；不是退出接受记录，也不替代 CURRENT_STATUS

初版日期：2026-11-08；更新日期：2026-11-14

返回 [New RPA 报告索引](../README.md)。本文保留原门账路径，按最新裁定更新当前门状态；
历史原始测量、失败和运行身份保持不改。判据与协议以
[实施方案](../../../../../stage_plans/active/stage-07/05-rpa-new/r4/plan.md)和
[固定运行登记](../../../../../stage_plans/active/stage-07/05-rpa-new/r4/formal-registration.md)为准。
本轮执行边界见[自动收尾裁定](2026-11-13-closeout-decisions.md)，上一版移交准备见
[F4/PD 移交门账](2026-11-07-f4-gate-and-handoff.md)。

## 1. 当前候选与目标

当前工作树为 D:/Cuexis-worktree，分支 codex/r4-closeout，以 origin/master 的
85ebf64a 为本轮起点；收尾候选日期构建为 26.11.13-1，SDK API 为 0.9.1。
P5 第四次完整组、独立 P3 与设备 capture-1/2/4 首个完整组绑定测量候选
`ce2a9a6952fd4a9a4d82949544f7ef3482e204e6`，tree 为
`f2ec992cd94aa32c838d5ee863bdbca0b37033d2`；逐指标结论如下，不代表整体接受。
原音乐具有限功能结果，原 DFJK 在本候选未达到 oracle；旧 scratch 成功不认证本候选。
memory/P8 因完整语义图缺失保持拒绝正式准入，独立 GPU observer 未准入。
最终验证候选与新的十一行 F1／同 SHA 三平台 F2 尚待完成，不能继承历史候选。

目标是在完成自动实施、取证、落档与最终验证后，只保留 P7 听音、O2 实体操作、
F3 校准与生产参数接受、O3／PD-5 退出接受。**目前尚未达到该目标**；下表中的自动门
不能因目标已获授权而删除、改判通过或转写为仅需人工。

## 2. 全门状态

| 计划单元 | 当前结果或实施状态 | 剩余门与证据边界 |
| --- | --- | --- |
| R4-0 起点 | 历史起点审计保留；本轮起点为 85ebf64a；P5／设备测量绑定 ce2a9a69 / f2ec992c | 最终文档与必要处置完成后冻结最终验证候选，完整登记源码／工具／fixture／profile／二进制；本测量 SHA 尚无最终 F1/F2 |
| R4-1/2 B3/B4/B5 | 既有机制与可控反例保留 | 新候选完整矩阵、实际线程／设备适用项仍须核验，旧局部结果不外推 |
| R4-3 O1 | 方案 B 的 64/2048/40K reference 设计 caps 已接受，原数字保持 | 设计 caps 不是完整堆上界或生产接受；修订数值与生产 profile 尚未接受 |
| R4-4 工具 | CPU、host、controls、Seek、device capture、P3、容量和 Player 工具已集成；P5／设备组及容量／Player 有限取证已有结果 | 工具可执行不等于准入或接受；memory/P8 完整语义图缺失仍 blocked，DFJK 本候选失败与最终矩阵仍待处理 |
| R4-5 P0 legacy/RPA | 固定计时与 read_long 均已结案；legacy 后续 validated_trace／playback_only 也通过 | 只覆盖对应固定 CPU 工作的相对成本／重复性；首组失败、不同来源的 A 与后续组不合并 |
| R4-5 P0 host | 三窗口九进程 request→ready p99 为 87.9–99.1 µs，9/9 通过 4.167 ms 预算；两条外层腿重复性通过 | ready→present-return（约 4 µs）重复性 inconclusive 保留；无 GPU/audio，不代替 P3/P7；该单元次数已用尽 |
| R4-5 P1/P2/P4-CPU/P6 | 七个 CPU 探针单元都有三窗口结果；六个计时组重复性通过；p4-capture-1 全九 16.7% 的分辨率受限 inconclusive 已获接受 | 不再重跑 p4-capture-1；探针只报结构计数与重复性，维持无新增绝对预算 |
| R4-5 P3 renderer | ce2a9a69 首个 device claim 完成三窗口九进程；request_to_ready p99 295–323 µs、close/load max 门各 9/9 pass；request_to_ready／close／load_host 重复性 pass | cpu_submit／present_call／request_to_present_return 重复性 inconclusive，保留首窗慢进程；无 GPU timer／audio，不证明 GPU 执行时间或实际屏幕呈现；不是 P3 整体接受 |
| R4-5 P4-device | capture-1/2/4 各首个 device claim 完成三窗口九进程；capture-1/2 重复性 pass；结构与 sampled 环境核验按设备组报告保留 | capture-4 重复性 inconclusive（最大窗口及 all-nine 23.529412%）；capture 无新增绝对门；CPU 历史台账不变，不自动开启第二次 |
| R4-5 P5 controls | attempt 4 完成三窗口九进程，73 条 complete 样本／进程，共 657 条；七类绝对门 63/63 pass，五组重复性 pass | close／load_host 重复性 inconclusive，不能宣布 P5 整体接受；第四次已消耗、第五次未用，不自动重试、不替换前三次现场；load_host 没有独立绝对预算判定 |
| R4-5 五类 Seek | 五类 20 ms 绝对门全部通过，最差 1,014 µs；natural／hot／external 重复性通过；fallback 为 20.27%、cold 为 39.92%／35.92%，重复性 inconclusive | 按本轮裁定不再重跑、不放宽判据，两项限制接受留给 O3；只覆盖报告登记的设备与端点范围 |
| R4-5 阶段／GPU 时间 | 12 个 observer 进程完成，同输出；四阶段沿既有可开关例外，12 个逐进程 p99 数值门 pass；GPU 三份 query 均完整 | stage 外层请求比为 12.20–13.25，CPU submit 最大比 51.44，按原值披露；GPU 三项 observer_unvalidated，不改成 normal 性能或正式三窗接受；见[观察器报告](2026-11-14-stage-and-gpu-observer.md) |
| R4-5 memory | 八个 profile 的 48 次配对 invocation 得到 47 份 summary，七份 qualification blocked；allocation-origin／semantic 均 unrated；配对与 residency 共两处环境停止 | 完整 owning/lifetime 图未实现；n64/n2048 state 原点超额、n2048 delivery.attempt 超额保留；n64 仅有限 sampled GPU residency，n2048 无 summary；prepare-only 上界、最终释放与 observer 扰动未闭合；原 caps 不变 |
| R4-5 P8 | 接受的归属修正与 (40000,4096) 内容口径不变；有限历史审计识别 low 1／high 4 五份 invocation packet，含失败与独立同 SHA high control；本轮容量批 P8 invocation=0 | 旧 high-3 attribution 仍为修正前分区，修正后数字应引用 f5f91916 low/high-control；旧报告已追加勘误，额外 high 调用及追溯缺口仍待裁定；不替换原 packet、不恢复额度；正式语义准入仍 blocked |
| R4-5 P7 自动轨迹 | 原音乐约 129 秒活动、audio_ended=true、underruns=0，功能 valid；原 DFJK 为 40 分／19 命中／2 misses，Replay 一致但未达 oracle；两条均为 opt-in trace、正式计时次数为 0 | 原 DFJK 失败保持；两条都不构成 normal 三窗性能／听音／实体输入接受；DFJK 适用长活动证据仍缺，音乐 retention 适用性独立；不能用 terminal 等待或替换课程补时 |
| R4-6 F1 | 历史候选 2c4e1274 的十一行 local_pass 保持原身份；本轮最终候选 F1 尚未完成 | 文档及必要处置完成后冻结最终候选，实际执行新的十一行矩阵；不预写 pass |
| R4-6 F2 | 历史 2c4e1274 同 SHA 三平台 success 保持原身份；本轮最终候选 F2 尚未完成 | 取得与本轮 F1 同 SHA 的 Linux Quality、Windows MSVC、Windows MinGW 实际结果；不以旧 CI 或草稿替代 |
| Version Gate | 2c4e1274 的 success 保持原身份；本轮结果未预签 | 按实际 PR/base/candidate/tree/UTC 日期重新核验；SDK API 未从 base 0.9.1 改变时，精确批准是否适用以 checker 实际结果为准 |
| R4-7 O2／P7 听音 | 实体清单已准备；本轮实体事实尚未接受 | owner 完成听音、实体操作及所需仪器观察，或明确接受限制 |
| R4-7 F3／生产 | 当前仍为 testOnly；生产参数只获准作为校准目标 | 校准与具体生产 profile／参数接受，不能由 reference 通过代签 |
| R4-7 F4／O3／PD 退出 | 移交准备存在，最终退出未接受 | 完成自动门账、失败／缺证据和责任清单后，owner 接受最终退出范围及 PD-5 移交 |

## 3. 已接受的处置

| 事项 | 现行处置 | 记录 |
| --- | --- | --- |
| CPU p4-capture-1 | 接受分辨率受限的 inconclusive，不重跑、不改判据 | [探针批报告](2026-11-08-probe-matrix-admission-and-environment-stop.md)、[11/08 waiver](2026-11-08-owner-attempt-waiver.json) |
| CPU 探针绝对预算 | 维持无新增阈值；B-only 不伪造共同 A | [普通 host 报告](2026-11-08-common-host-unit.md)、本表的对应适用范围 |
| P5 形态与次数 | 保留探针形态及旧现场；最多五次，第四次完整执行但 close／load_host 重复性 inconclusive，第五次未用；不要求用尽授权 | [控制探针](2026-11-11-p5-controls-unit.md)、[工具停止记录](2026-11-13-p5-tooling-stops.md)、[P5 第四次完整组](2026-11-14-p5-attempt4.md)、[本轮收尾裁定](2026-11-13-closeout-decisions.md) |
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

2026-11-14 容量／P8 有限审计见[容量与 Player 有限证据](2026-11-14-capacity-and-player-bounded-evidence.md)。
该批保留两处环境停止，七份 qualification 均 blocked，完整语义图尚未实现。
n64/n2048 的 state.candidate、state.current 及设备 n2048 的 delivery.attempt 原点请求超过原 cap；
这是 allocation-origin 归域超额，不据此冒充完整语义 owning 评级。原 caps 及已接受的归属修正均保持。
n64 low/high 的有限 GPU residency active/peak delta 均为 1,122,304 B；只覆盖登记 sampled 区间，
before-device 的有效计数为零而保持未知，after-close residency 未归零，n2048 无可用 summary。
逻辑 GL 的 72 B 与进程 Dedicated+Shared residency 独立，不相加，也不构成 observer 或完整预算接受。

P8 历史 high-3 的 attribution.json 仍为归属修正前分区，不能证明修正后数字；修正后
command.session=19,762,486 B、execution.retained=141,080,535 B 的直接来源是独立
f5f91916 low/high-control 对。已为旧 high 报告追加勘误，保留原 packet 与既有 owner 裁定；
不追认额外 high 调用、不恢复额度、不把本轮有限审计当作全历史穷举或正式 claim。

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
