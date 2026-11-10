# R4 固定取证登记与剩余预注册

状态：active；O1 已接受；P0 legacy/RPA 首固定 CPU 组已结束；七个探针单元三窗口完成，内存归因完成，接受未完成

更新日期：2026-11-07

返回 [R4 实施方案](plan.md)；新增数值见 [O1 裁定包](caps-proposal.md)。
本文件展开工作单元和待填身份，不代替总计划 §9 或既有测量合同。

## 1. 单元展开

| 单元 | 工作内容 | 对照与固定执行 |
| --- | --- | --- |
| P0-legacy / P0-rpa / P0-read | 原固定 trace、Playback-only、read-long；内容/Replay 精确绑定 | 合法 pre-PD A/B；每进程 8 块，fresh 64 steps，read-long 每块≥100 ms |
| P1-no-consumer | 原同 U、F=R=0 | 无共同指标为 B-only，记录结构计数 |
| P2-low-delivery | 原 U:F=8:1 | 原 profile，A 不支持则 B-only |
| P3-renderer | 原 U:F=1:1、单 renderer、相同内容/viewport/time | request→ready 可比较才配对；ACK 不冒充 present-return |
| P4-capture-1/2/4 | 三个子单元，同修订 CPU capture；GPU renderer 仍只有一个 | B-only；分别保留原 payload/attempt/consumer 数 |
| P5-controls | 20 Pause/20 Resume/10 paused Reset/10 running Reset/10 resize/1 Close | 同前置状态；稳态与控制间帧空窗分别报告 |
| P6-sparse / P6-topology | 原稀疏轨迹；拓扑/资源代更换独立 | 200/201 原规模及 profile，不能用新 64/2048 小夹具替换 |
| P7-music / P7-dfjk | 真实音乐与独立 DFJK 谱面，脚本分别固定 | B-only 设备；音乐完整 129 秒与控制压力分开，实体参与另列 |
| Seek-natural / hot / external / fallback | 首次自然锚点、同档热用、外部首验、无合法锚点 | 各自登记每进程 16 次的同类语义构造；不得预造测量 H 的 checkpoint |
| Seek-cold | 新进程的首次 Seek | 每进程仅一次 cold 请求；后续请求不混入，文件缓存未控制不称 disk-cold |
| memory-64-low/high / 2048-low/high | 新分域、同内容两种 reuse | 预算接受、标签闭合、观察器准入后才启动；不作为 P8 替样 |
| P8-low / P8-high | 新身份 40000 entities/requirements 的原完整规模流程 | 各一次完整单侧；新 invocation，不续接旧失败、不追加补样，不验 40K @240 FPS |

P0–P7 的 fixture/脚本从既有权威轨迹展开，以上不是新发明的合并负载；
仅有单元名称不能证明实际参数、A、构建或工具已就绪。

## 2. 注册时必须填入的内容

- 源码 C 的 SHA/tree、产品/tool/profile 差异、pre-PD A 完整来源及匹配 harness；
  分别记录工具链、选项、依赖基线、二进制/hash；禁止混用 ABI 或以 R3/Q4 代 A。
- 每个 content/Replay/输入、逻辑时间、对象/绑定/消费者数、U/F/R、规模与 low/high、原预算
  和新增预算 hash、O1 接受记录、工作端点及必需主指标。正式与诊断适用性逐项写清。
- 每进程请求/样本数、热身、固定轨迹、组内次序、窗开始与完成时间、deadline、
  事前具名 invalid 及 §4 有限新登记额度；旧失败仍有独立 ID 和证据引用。
- 三窗×每窗三对；AB/BA/AB、BA/AB/BA、AB/BA/AB；窗间≥600 s；
  B-only 每窗三进程。P8 不借该协议扩大次数。
- 每进程机器热身 30 s；稳态原 10 s 预热及≥60 s/10000 updates/10 倍 retention/容量；
  Player 60 s retention 的单元必须实际采样≥600 s，不能套用原 1 s retention 单元的 60 s。
- 真实 GPU、driver、240 Hz、1280×720 window/VSync ON、固定 Senary endpoint/gain0.35、
  交流电、实际电源方案、CPU 亲和、前台状态、磁盘和无采样干扰；
  CPU/headless 明示无 GPU/audio，设备枚举不当作运行证明。
- 有限 pilot、语义和 observer 开销证据；pilot 与正式原始文件隔离；
  binding/claim/seal 在候选之外写入，避免为了填自身 SHA 改动 C。

P0 legacy/RPA 首组已绑定候选24f0a914、原pre-PD d344088及匹配普通Release STATIC二进制，
正式claim/seal和三窗36个测量进程的字节归档见
[2026-11-07报告](../../../../../stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-07-owning-and-cpu.md)。该登记仅覆盖固定CPU trace相对成本与重复性，
不认证端点p99；两read_long超界，legacy另两指标因重复性inconclusive，RPA另两项通过。
其他单元字段保持待生成，不使用这一组身份或小夹具代替各自准入。

## 3. 裁定与阻断

复用 measurement.py 的 t 区间、重复性、分位数规则；草案校验要求明确 workload/Seek class，
Seek-cold 每进程 1 次，其他可比 Seek 每进程固定 16 次；非 Seek 工作单元使用各自协议计数。
工具测试拒绝缺字段、旧 A、非法数值、替样、窗协议变更和将 cold 请求混成 hot。正式前必须补齐端到端工具及 seal
拒绝门；registration.py 只校验草案，新增 admission/binding/runner 则分别承担准入、
外部身份与不可变 claim/seal、固定组执行。它们当前只实现 owner/common-host 普通计时适配；
工具测试通过不等于真实工作单元已经准入。内存语义拥有关系与设备适配仍未闭合。

owner 追加授权的两轮 64/high 语义整改已用尽；有效第二轮证明 active World 唯一下界
358,241 B 超过原 327,680 B，历史失败保留。128-slot效果已验证，2026-11-07扩大
owning覆盖后high/low下界均119,534 B且仍unrated，memory/P8保持未准入。其余域未完成语义闭合，
不根据该下界抬高 caps 或追加负载；后续范围待裁定，见
[语义整改证据与方案](../../../../../stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-05-semantic-memory-remediation.md)。

新预算未接受、observer 标签不全、比较边界不一致、语义不等价或缺少 immutable binding
时停止对应单元准入。对未知设备归属/首次认证/未控制缓存等如实记录；
未具备普通计时边界的预算保持未判定，不用归因读数跨门代验。

## 4. 展示矩阵探针单元预注册

原 PD-5 展示矩阵探针已携带 P1/P2/P4/P6 的真实工作负载；本轮把它的工作内容固化为
可准入单元，而不是另造夹具。`tools/verification/execution_acceptance/probe_units.py`
在采样前声明全部计数：每进程 14,400 帧（240 Hz 实际时间 60 秒）、2,400 帧预热（10 秒）、
200 实体稀疏内容、200/201 拓扑代次、以及拓扑每 256 帧重载。单元计数由该表推导，
不从观察到的读数回填。

| 单元 | mode | 消费者 | 更新:画面 | 每进程样本 |
| --- | --- | ---: | ---: | --- |
| P1-no-consumer | no-consumer | 0 | 1:0 | sdk_update 14,400 |
| P2-low-delivery | low-delivery | 1 | 8:1 | sdk_update 115,200；capture_delivery/materialize 各 14,400 |
| P4-capture-1 | capture-1 | 1 | 1:1 | capture_delivery/materialize 各 14,400 |
| P4-capture-2 | capture-2 | 2 | 1:1 | capture_delivery/materialize 各 28,800 |
| P4-capture-4 | capture-4 | 4 | 1:1 | capture_delivery/materialize 各 57,600 |
| P6-sparse | sparse | 1 | 1:1 | 同 capture-1，200 实体 |
| P6-topology | topology | 1 | 1:1 | capture-1 加 topology_reload 56 |

以上全部为 **B-only**：pre-PD tree 没有对应 harness，A 记 N/A，不填 0、不伪造比值。
P3、P7 与设备 capture 变体需要真实 renderer/音频端点，本表不含它们。每个单元仍是
三窗口×每窗三进程、窗间 600 秒、每进程 30 秒机器热身；失败保留，不替样。
本轮同时把探针的采样时钟改为纳秒并输出小数微秒，使 no-consumer 的亚微秒更新不再被记成 0；
帧数、消费者数、比例和重载间隔均未改变，因此负载身份不变，但该修订之前的整数微秒读数
不能与本轮小数读数混用。

准入顺序固定为：`prepare_probe.py` 单进程 pilot 与三份 proof → `admission` 校验 →
`formalize.py` 一次性 claim/seal → `runner.py` 固定组。pilot 只证明已声明身份、环境与
计数可执行，不进入正式样本，也不消费正式额度。绝对预算只按已有接受值登记；
展示矩阵的 `materialize` 归因边界尚未通过 observer 准入，故其读数保持未判定，
不用相对门代签。

2026-11-08 实际登记与结果：P1-no-consumer、P2-low-delivery、P4-capture-1、P6-sparse
按上表登记。attempt-1（候选 `00af7f90`）完成第一窗口 12 个测量进程后，因会话锁定
（前台进程变为 0）停止；attempt-2（候选 `b7edc959`，补救验证后绑定
`rpa.execution.retry-disposition.v1`）完成两个完整窗口 24 个测量进程，第三窗口因前台切到
`explorer` 停止。两次都没有三窗口，故无正式裁定；四个稳定单元键的 attempt-1/attempt-2
均已消耗且保留，第三次登记需 owner 裁定。P4-capture-2、P4-capture-4、P6-topology 已完成
一次非正式全规模身份预检（逐字段一致），未 claim、未运行。待裁决方向见
[探针批报告](../../../../../stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-08-probe-matrix-admission-and-environment-stop.md)。

owner 于 2026-11-08 豁免额度后（`2026-11-08-owner-attempt-waiver.json`，`admission.claim`
只在具名 waiver 下放宽并把 waiver 写入 claim）：四单元 attempt-3 于 20:19–21:52 完成 36 个
进程，六个计时组重复性通过、`p4-capture-1` 全九 16.7% 为 inconclusive；
`p4-capture-2`/`p4-capture-4`/`p6-topology` 的 attempt-1 于 21:56–23:11 完成 27 个进程，
三个计时组全部通过。七个已注册探针单元至此都有三窗口正式结果。

同批另新增 `p0-host-common-release`：`prepare_host.py` 以 1,024 帧、32 帧原生预热、
无 GPU/audio 的普通 host 路径登记，B-only（pre-PD A 是 receipt-ACK 边界且 digest 不同），
唯一已接受预算为 request→ready p99 ≤4.167 ms。attempt-1（候选 `f1c4d22e`）完成两个窗口
6 个进程后因前台持续变化停止；attempt-2 绑定已验证补救后完成三窗口九进程，
**九条 p99（87.9–99.1 µs）全部通过该预算**，request→ready 与 request→present-return
重复性通过，ready→present-return（约 4 µs）为 inconclusive。两次额度已用尽。见
[普通 host 报告](../../../../../stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-08-common-host-unit.md)。
