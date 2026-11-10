# PD-5 Stage C 独立功能验收、内存责任与退出缺口

状态：completed（本批审计与交付）；Stage C overall acceptance / PD-5 acceptance：not passed。

快照日期及更新日期：2026-10-23，Asia/Shanghai。当前摘要唯一见
[CURRENT_STATUS](../../../../../../../CURRENT_STATUS.md)。本报告记录既有运行的独立核验和缺口，
不新增预算、采样或 owner 接受，不覆盖历史失败报告。

## 1. 结论、范围与身份

固定原样 40,000 entities / 40,000 requirements 的 revision 4 low/high 各一次已完成。
本轮独立核验完整原始日志、衍生记录、参考 wire、源码与构建绑定：功能部分通过；
全进程内存观察包络超限，完整分域内存与最终释放证明不足，不能接受 Stage C 整体或 PD-5。
本轮没有启动 native、构建、saved-wire decode、pilot、性能候选或正式 preparation/registration。

| 身份 | 本轮核实值 |
| --- | --- |
| 主树 / F1 | `D:/Cuexis-worktree`，`stage-7`，`eed36648a692333e47f798aa45be36ca850d08de`，干净 |
| 隔离树审计起点 | `D:/Cuexis-pd5-40k-admission`，`codex/pd5-gameplay-40k-admission`，`08e145cad6eeff1f3c171b3c346fc1f1f608f946`，干净 |
| native source | `54e8c2a1d770a90e1d7a2728f00510b1b19aabaf`；本批只有报告层差异 |
| 冻结构建 | `out/build/pd5-stage-c-snapshot-release-20261023-54e8c2a`；Release STATIC、candidate ON、lifetime observer ON，metrics/trace/execution diagnostics OFF |
| host probe SHA-256 | `6722c6e86a0908d6375ad0022b0a503d7b9a99b534b07e60e84f4cfe87b1cbfd` |
| 原始 invocation | `out/pd5-stage-c-cli-20261023-08e145c`；原目录不增加文件、不改写 |
| 本批新增审计/封包目录 | `out/pd5-stage-c-audit-20261023-08e145c` |

最终文档提交身份由新审计目录的 `report-binding.json` 记录，不把报告提交当 native source。
主树尚未集成隔离树 R4/生命周期改动；隔离树的功能结果不能冒充主树已集成或正式测量已就绪。

## 2. 原始功能结果的独立验收

两侧均恰有 24 个有序阶段、48 个 begin/succeeded 事件，51 assertions / 1 test case，
native exit 0、stderr 为空，无外部终止。low/high 为不同 PID，high 在 low 完成及验证后启动。
精确 discovery 使用已绑定二进制的 `--list-tests --verbosity quiet`，返回唯一真实测试名并 exit 0。
没有再次执行 discovery 或 native。完整 stdout、raw-records.jsonl、stage-log、progress、
launch-intent 与 acceptance-state 一致；检查声明在 decode begin 与 succeeded 之间。

实际链路为 encode → GameplayContent::fromPacked → Host load/advance/query → Snapshot
编码/解码 → archive/evaluateReplay 的 sameResult → pause/replayCut/seek → sample/present →
Seek 后 sameResult → frame 释放 → 显式 close/host.reset → after-close checkpoint → 进程正常退出。
Snapshot 解码断言是 `decodedSnapshot.valid()`；本例没有额外断言解码后重新编码逐字节相等，
也没有以该 decodedSnapshot 执行独立 restore。Replay 与 Seek 的 sameResult 不应改名成这些额外证明。

| 项目 | low | high |
| --- | ---: | ---: |
| Packed file bytes | 6,818,424 | 6,746,664 |
| decoded physical bytes | 6,817,880 | 6,746,120 |
| 最大 section bytes | 3,725,942 | 3,654,182 |
| strings / references | 40,048 / 13 | 40,048 / 13 |
| Snapshot bytes | 90,823,328 | 90,823,328 |
| semantic preimage bytes / hash scratch peak | 99,588,531 / 7,145,909 | 99,588,531 / 7,145,909 |
| SHA 求值次数 | 1 | 1 |
| 原进程运行秒数（仅观察器功能运行） | 1780.4 | 1680.4 |
| 原 Seek→sample/present 观察值，微秒 | 6,372,182 | 6,089,257 |

low wire SHA-256 为 `f03936693c13d0517cb8dd7c91e13977b996f6f37457da2cadb1eed6a627a301`，
high 为 `f256fe8fdc1324b34510fb2a1cd409c0fb67a48b662292bcaf0154f2af86bbb7`；
本轮重新逐字节比较原 reference 且复算 SHA。原 stdout 摘要分别为
`5183c7faff9e23f773859f2f598d5fc0a5223aeae48885831ef2906fdaccd8a5`、
`c24431b0e76eca00a5d62e3ef5a63a6b33ef0118e4f0c4e62a237be4d296d52c`。

R4 TEST ONLY Snapshot maxElements 已由 20,000,000 提高到 67,108,864，本次真实 decode
使用该声明；maxBytes=268,435,456、maxRecords=100,000、testOnly=true，revision 3 不变。
这项已披露的测试声明修复不是生产预算接受，详见[原修复报告](2026-10-23-snapshot-probe-budget-repair.md)。
七项 Packed 物理预算与四项 hash ceilings 均保持原合同；本例不独立重验所有攻击边界。

插桩时间不用于 20 ms Seek、1 秒 Close 或任何正常分配器预算结论。
本次显式关闭成功只证明这个固定 profile；旧约 25 分钟关闭故障仍无根因证据。

## 3. observer exceeded 的合同归属与实测

原始权威为[已接受 PD-5 协议 §5](../../../../../../../stage_plans/active/stage-07/04-pd/pd-5/pd5-measurement-proposal.md#5-内存积压与-owner-拆分判据)
及[设计 §9](../../../../../../../stage_plans/active/stage-07/04-pd/design.md#9-容量-profile测量与退出pd-0--pd-5)：
reference workload 需逐域完整有限额度、峰值及回收证明；没有已接受的全 SDK 统一 MiB 生产额度。

本次 `global_cpp_new_envelope` 数值来自 `tests/pd/pd5_memory_observer.cpp` 的固定 testOnly
工具常量：count=2,097,152、requested=1,073,741,824、CRT storage=1,342,177,280。
`exceeded` 在任意瞬时超限后保持 true，并非只检查当前 checkpoint；它也可因结构 registry
耗尽置位，但本次 count/bytes 本身已有明确超限，不依赖该其他原因。
allocation 发生时仍返回实际存储，不因标志而主动拒绝分配。

`tools/verification/playback_performance/observer.py::memory` 把 exceeded=false 和各自
peak≤limit 作为 observer 记录可用的检查条件，另有默认 oldest-age≤900,000,000 µs。
本轮只执行该纯解析函数，low/high 均在原内存包络检查处拒绝，原文为
`Allocation envelope incomplete or exceeded`。两侧 active/after-close 的 oldest age 也超过其
默认 900 秒；没有修改参数或跳过检查。该工具本身即使通过也只标记 observer plumbing，
acceptance=false，不能代替生产或完整 PD 验收。

Stage C 的 `gameplay_admission_evidence.py::validate` 只验证三个真实 memory checkpoint 的
标签和 allocator，不要求 exceeded=false。这是两个校验器的职责差异：本例功能 PASS
并非该内存 collector PASS。未发现把该固定工具包络另行接受为 40k 全 SDK 生产总预算的登记；
本报告既不豁免超限，也不由全进程数字推断某个生产子预算必然失败。

| C++ new 观测量 | low | high |
| --- | ---: | ---: |
| peak live allocations | 8,521,200 | 8,441,200 |
| peak requested bytes | 2,960,025,594 | 2,941,182,894 |
| peak CRT usable storage bytes | 3,505,382,394 | 3,481,419,694 |
| active requested / after-close requested | 1,864,035,498 / 1,360,399,033 | 1,863,874,038 / 1,360,237,573 |
| after-close live allocations | 5,120,531 | 5,120,531 |
| after-close CRT storage bytes | 1,688,113,017 | 1,687,951,557 |
| before-load / active / after-close exceeded | false / true / true | false / true / true |

active→after-close 两侧均减少 920,477 个 live allocations、503,636,465 requested bytes。
这只是两个观察点的净差，不能归因为 Host 独占全部释放量，也不能据此分摊重叠域。
observer 覆盖 linked C++ new/delete（含 aligned/array/nothrow），不覆盖 direct malloc、SDL、
driver 或 OS heaps。CRT charge 包含观察器 lifetime Header、对齐和底层分配器可用容量；
spinlock/逐分配记录及 checkpoint 全链遍历也扰动运行，不能用其 storage/time 冒充正常分配器。

## 4. 内存持有责任与证据缺口

依据冻结 `pd5_capacity_probe.hpp` 的局部作用域、公共 owning 类型及
`gameplay_internal.hpp` / `gameplay_candidate.cpp`。下表只确认所有权与存活边界，
不从声明大小估算真实 heap bytes，也不把同一 shared backing 重复求和。

| 对象/域 | after-close 时的实际责任 | 已知事实与尚缺证明 |
| --- | --- | --- |
| fixture、document、prototype、owners、chart | 测试用例局部对象，仍存活 | 构造含 40k 内容；不是关闭 Host 应释放的私有状态，具体 heap 占用未知 |
| packed 与内容 storage | packed vector 与 owning GameplayContent 仍存活 | wire bytes 已知；PreparedCapsule/config/bindings 等不是 wire bytes 的同义词，decoded heap 未单独测量 |
| expected 与 evaluated.result | 两份 owning GameplayResult 仍存活 | GameplayResultStorage 含 KernelProjection；共享与独占占用未拆分，不能记为泄漏或全部独立副本 |
| snapshot、decodedSnapshot、bytes | 两份 owning Snapshot 和编码 vector 仍存活 | 编码逻辑长度 90,823,328 已知，vector capacity/两份 payload heap 未测；不将 wire 长度当 requested 总额 |
| replay 与 cut | owning ReplayArchive 仍存活；cut 是值 | 可合法跨 Close 保留；没有在释放这些值之后记录 checkpoint |
| frame、Host/worker | frame 在 Close 前清空；Host close 后 reset | 真实执行及进程 exit 0；本例没有输出 payload/attempt/GPU 全部 live=0 的独立分域账本 |
| World/current/candidate/scratch/index | 曾被 observer registry 记录 | after-close 的 35 个结构项是跨生命周期 per-instance high-water；不是当前 live count，不是对应 bytes，也不证明 World 尚存 |
| journal/receipt/retired leases 与 GPU/audio | 须按各 owner 独立计费 | 本例 testOnly CPU renderer/audio，不具真实 GPU/audio allocation/fence 证据；不能补 0 代表生产域通过 |

结论为“对象持有范围已解释，分域预算及释放完成仍未验收”，不是“全部 1.36 GB 已证明合理”。
测试返回后有正常进程退出证据，但无全部测试 owning 值销毁后的 observer checkpoint；
进程退出由 OS 回收也不等于逐域零泄漏证明。

后续最小证据方案需先登记：沿同一 ownership 的阶段记录各域 requested/storage、合法外部
持有与 retired live，区分 test fixture 和产品存储，并在全部 owning lease、设备 fence、
owner 回收完成后验证相应 live=0 或已声明有界稳态。本批仅列缺口，没有增加计数器、改 API、
引入 owner/线程或执行新样本。先完成静态账本，再另行固定最小采集范围，避免自动重跑整组 40k。

## 5. PD-5 退出缺口矩阵

门禁仍由[原测量协议](../../../../../../../stage_plans/active/stage-07/04-pd/pd-5/pd5-measurement-proposal.md)、
[PD 计划 §7/11](../../../../../../../stage_plans/active/stage-07/04-pd/plan.md)
及设计拥有。下表是本时点证据审计，不新增或豁免门禁；既有工具 JSON 的 pending 文案也不是
当前运行事实。全部性能单元进入前须完成正常 allocator/collector/profile、等价与新身份冻结。

| 单元/责任 | 现有证据 | 退出前缺口及判定 |
| --- | --- | --- |
| P0 owner/旧新共同宿主；Debug/Release | 修复前两窗及 F1 有限诊断；C1 撤回、C2 未达保留自检，主树保持 F1 | 正常构建上的新准备/有限 pilot/注册及固定三窗未完成；旧窗不续接，F1 read 回归仍待处理 |
| P1 无消费者 | SDK/delivery 短轨迹、结构计数 | 缺正式长稳态、完整 profile/回收/age 证据；F=R=0，缺 A 共同指标明确 N/A |
| P2 U:F=8:1 | 短轨迹与独立物化计数 | 缺固定时长/次数三窗与分域容量；不能用少出帧冒充同 F 收益 |
| P3 U:F=1:1 单 renderer | 公共 request→ready 短轨迹、CPU/GPU 工具路径 | 缺正式正常计时；旧 ACK 与新 present-return 分开；阶段 D2/D3/queue/materialize 需 observer 覆盖及开销证明 |
| P4 1/2/4 captures | 独立 CPU capture 短轨迹；组合 renderer/captures 探针存在 | 组合路径验证、完整额度与正式单元待完成；不能用独立 captures 替代真实组合 |
| P5 控制与冻结画面 | 脚本工具和最小 Tail/Close 回归；本次 40k close 成功 | 缺原声明控制次数、真实画面完成端点及正式每次 max；host-local load 不等于进程启动；旧关闭风险未归因 |
| P6 稀疏/拓扑/资源代 | 200/201 实体 SDK 短轨迹、代数与消费者重建 | 缺适用规模、触及对象/copy/scratch/析构分域、长时和正式窗口 |
| P7 GPU/audio | 设备盘点、受限音乐/DFJK 观察和工具边界 | 原协议已纳入本机 reference 真实 renderer/audio 单元，不能整体移交 RPA 后豁免；需实际端点绑定、脚本输入、GPU 时间/回收与新身份测量；人工听感仍由 owner |
| P8 40k low/high | 本次 R4 全链路功能、物理 bytes/hash 及参考 wire 身份通过 | 内存/释放、正常分配器适用 Seek≤20 ms 及完整规模 profile 未完成；本例不额外证明 decoded Snapshot restore/re-encode |
| 分域内存与长时寿命 | PD-4 小容量退休测试；本次全进程包络和结构 high-water | 两者不能拼成 40k 完整内存通过；World/candidate/scratch/index/payload/attempt/journal/receipt/GPU 的峰值、age、retired 与最终释放未齐 |
| Debug/Release/平台/安装消费者 | PD-0–4 历史矩阵；C2 独立 Release 1272 pass / 1 skip；R4 局部回归见原报告 | 不能借 C2 或旧 SHA 接受本次集成；最终采用 native/product SHA 后，按受影响路径重做适用 Debug/Release、static/shared、candidate ON/OFF、headless/installed/reference-host；hosted/MinGW/Linux 缺项明确交回 |
| 统计/环境/正式注册 | 已接受三窗协议及旧组完整性 | 缺当前准备与完整单位清单；保持 3窗×3对、600秒间隔、窗/前六/全部九进程 15% 和配对 1.10；设备/电源/安静环境在采样前确认，不倒填 |
| owner 拆分触发 | 旧两窗超界与候选噪声不足以触发 | 同一 accepted budget 三个连续预声明有效窗口才判；缺有效证据保持未触发/证据不足，不自动新增 owner |
| 报告、封包与主树集成 | 本批补功能审计、缺口矩阵与独立证据包 | 只结束本批证据整理；主树集成、最终性能/寿命接受及整个 PD 退出仍未完成；不默认 merge/push |

R1–R8、S1–S6、X1–X6 的交接追溯按设计 §10–12 分类：

| 设计责任 | 现有证据入口 | 当前缺口/边界 |
| --- | --- | --- |
| R1 | [PD-1](../../../pd-1/2026-10-18-pd1-publication-bridge.md) | 当前集成基线须保留发布前缀/失效/包边界回归 |
| R2 | [PD-2](../../../pd-2/2026-10-18-pd2-projection-adapter.md) | golden 不改；未来新 phase 不默认为已支持 |
| R3 / S2 / X2 | [PD-3-b](../../../pd-3/b/2026-10-19-pd3b-state-read-and-materialization.md)、[PD-3-c](../../../pd-3/c/2026-10-19-pd3c-contract-and-version-decision.md) | 既有域/修订与保留画面义务保留；后续新增表现域归所属阶段 |
| R4 / S1 | [PD-3-a](../../../pd-3/a/2026-10-18-pd3a-software-transaction.md) | 最终集成需受影响事务/恢复回归，不据功能退出认定失败路径全验 |
| R5 / R6 / S3 / S4 / S5 / S6 / X4 | [PD-4](../../../pd-4/2026-10-19-pd4-delivery-retirement-and-observation.md) | 长时分域容量与真实资源回收仍欠本次 profile 证据；平台内存压力/后台和 Context 失效真实证据归 Stage 12 |
| R7 | PD-4 及[最小生命周期修复](../../lifecycle/2026-10-23-minimal-lifecycle-repair.md) | 有界 Tail/工厂和本次关闭结果不消除旧阻塞的未归因风险 |
| R8 | [PD-0](../../../pd-0/2026-10-18-pd0-controlled-environment-and-build-binding.md)、本报告 | PD-5 正式判定和拆分审查未完成 |
| X1 / 正式合同差异 | PD-3-c、PD-4 的 SDK 0.8.1/0.9.0 接受与消费者证据 | 冻结实际采用 revision、SDK/迁移差异；不接受暂停的公开停止候选 |
| X3 / X5 / X6 | [设计 §12](../../../../../../../stage_plans/active/stage-07/04-pd/design.md#12-与后续阶段的兼容与设计约束) | Studio、未来有界重建与稳定 ABI 是后续接入条件，不要求 PD 预先实施；不冒充已交付 |

完整生产 profile、精确 AV 同步、实体设备失效/恢复、RPA-F4/owner 最终生产接受及整个
Stage 7 关闭仍属 RPA/生产交接；它们不因本批通过关闭。但已接受本机 reference 协议中的
P7 和适用 P8 测量不能从 PD-5 矩阵删除。大型公开停止接口继续暂停，不作为前置。

## 6. 历史完整性、封包与实际检查

编辑报告前，原 `bind.py check` 和 `bind.py frozen` 新运行均 exit 0：源码/报告/build/archive/
runner 精确绑定，47 roots / 42,745 files，主树 F1 干净及环境脚本未变。主树原 F1 编译输入、
C2 `747ef18dc8ba3933f0e9b68cee7cbf8a2ee6e2c6` 源码/构建/工具也逐项复核通过；
4 个历史根 1,184 文件、前轮包 3,167 文件及正式历史快照 341/442 项、301 构建项通过。

原 41,093 文件清单仍 FAILED：41,092 项未变，一项退休 scheduler 的 agent-events.jsonl
后续追加漂移，两项精确登记退出产物新增。此前 46 roots / 42,689 files 与本次 47 roots /
42,745 files 是不同时点的独立清单；不得追溯把旧失败改成通过。

新目录保存可复算的 `audit_existing.py`、`audit_protected_main.py`、六项只读审计日志与
命令/exit/hash 回执、原目录前后完整清单、文档前后差异及检查日志。没有运行原
`run_acceptance.py`、任何 prepare 写入模式或会向旧目录写日志的历史交接脚本。

交付包为新目录的 `stage-c-evidence.zip`，外部 `package-receipt.json` 记录 ZIP SHA、大小、
index SHA 和报告提交。包内含原 invocation 全部文件（原 source/build ZIP 原样保留）、
两份 reference wire、相关方法/合同/源码快照、审计/检查日志和本批报告精确字节。
`verify_packet.py <stage-c-evidence.zip>` 无解压、无写入地检查所有包内 hash/文件集合、
嵌套 source/build 清单和 low/high 功能日志。包外 47 个历史根及主树/C2 的完整大目录不全量
复制；它们以原清单、摘要和现场核验记录交接，离线验包不能冒充重新核验这些外部路径。
这也是本批证据封包，不是 PD-5/Stage 7-PD 最终验收包。

本批适用检查为 `python -B tools/check_docs.py`、`git diff --check` 和独立 ZIP 内容校验。
只编辑本报告、索引和 CURRENT_STATUS，不重建冻结构建，不重复 C++/设备测试。
最终提交/封包回执记录实际命令及退出码；主树和旧 invocation 前后清单须保持。

## 7. 下一步及停止条件

本批到此交回。下一步先沿已有 F1/C1/C2 噪声证据做有界成本归因，最多登记一个读取性能
候选；内存先补静态分域账本和最小采集设计，再明确新运行范围。二者不得暗中合并为多候选搜索。
本报告未启动任何新候选或采样，未为缺失分域额度接受默认值。

进入正式测量仍需选定实际采用实现与回归、完整 profile/collector、独立新准备/有限 pilot/
注册和 source/build/environment/input/run-count 冻结，确认既有授权范围及现场环境。
旧窗口 3 继续停止；不复用旧 claim/seal，不替换慢样本，不自动重试直到通过。
七项物理预算、四项 hash ceilings 134217728/1000000/4096/8388608、1.10/15%、内部
reply deadline 30 秒均不变。新身份漂移、等价错误、预算触发或范围不清时保存证据并停止交回。
本批不授权 merge/push/发布、新 owner、SDK/公共 API 变化，也不接受 PD-5、RPA、生产或 Stage 7 关闭。
