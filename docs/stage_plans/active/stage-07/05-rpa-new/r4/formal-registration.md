# R4 固定取证预注册草案

状态：candidate；O1 前准备，formal_registration=false；没有 claim/seal 或正式运行

更新日期：2026-11-05

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

所有待绑定字段现在明确为待生成；草案不填虚构 SHA 或 0。原 pre-PD 资料已归档，
重建可行性/对应 harness 仍需 R4-4 准入；无法合法重建时阻断相对门。

## 3. 裁定与阻断

复用 measurement.py 的 t 区间、重复性、分位数规则；草案校验要求明确 workload/Seek class，
Seek-cold 每进程 1 次，其他可比 Seek 每进程固定 16 次；非 Seek 工作单元使用各自协议计数。
工具测试拒绝缺字段、旧 A、非法数值、替样、窗协议变更和将 cold 请求混成 hot。正式前必须补齐端到端工具及 seal
拒绝门；当前 registration.py 只校验草案，不具备执行/批准/取证能力。

新预算未接受、observer 标签不全、比较边界不一致、语义不等价或缺少 immutable binding
时停止对应单元准入。对未知设备归属/首次认证/未控制缓存等如实记录；
未具备普通计时边界的预算保持未判定，不用归因读数跨门代验。
