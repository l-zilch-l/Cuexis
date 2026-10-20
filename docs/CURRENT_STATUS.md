# Cuexis Current Status

状态：current

更新日期：2026-10-20

本文只维护当前阶段、正在进行的任务和未解决的问题。实施与测量细节见带日期的阶段报告。

## 当前任务

**Stage 7-PD / PD-0 testOnly 基线与行为表征、PD-1 发布视图、PD-2 绑定索引/Projection Adapter 的内部实施与本地适用门禁已完成；PD-3-a 已补正 reload 资源 OOM 门禁并经 GPT-6-Luna 复审通过；PD-3-b 状态修订、只读桥与唯一物化器的内部实施、本地适用门禁及独立复审通过；PD-3-c 内部实施与适用本地门禁完成（SDK 0.8.1 已获接受，新建无上下文 GPT-6-Luna 独立审核通过）；PD-4 三项合同/SDK 0.9.0 已获接受，内部实施与适用本地门禁完成，新的无上下文独立复审通过；PD-5 P0 已完成两窗各 36 个正式进程，发现局部性能回归；owner 要求先核查与修复后重新测量；根因已定位到 PD-3-b 中性读取边界并完成成本分解，修复未实施，完整正式接受未完成。**

以下性能结论属于冻结的 PD-0 实施输入，不代表 PD-4 当前源码的正式性能验收。该固定环境与干净重建后的完整注册组满足原定 15% 门槛，owner 最大进程级波动 3.89%，宿主均值最大波动 11.11%。测量前修复并验证了 MSVC/Ninja 编码不一致造成的头文件依赖丢失；历史慢态根因仍未确定，旧失败组和源码绑定失败组均保留。该结论限于已登记的有限 testOnly profile，不接受生产预算或关闭 Stage 7。

方案记录：[PD-0 测量波动方案实施](stage_reports/stages/stage-07/pd/2026-10-17-pd0-measurement-variance-resolution.md)。
最新证据：[受控环境、构建绑定与 PD-0 退出](stage_reports/stages/stage-07/pd/2026-10-18-pd0-controlled-environment-and-build-binding.md)。
PD-1 证据：[Judgement 发布桥、读取失效与包边界](stage_reports/stages/stage-07/pd/2026-10-18-pd1-publication-bridge.md)。
PD-2 证据：[prepare 绑定索引、候选投影与等价回归](stage_reports/stages/stage-07/pd/2026-10-18-pd2-projection-adapter.md)。
PD-3-a 证据：[Runtime/World 事务、派生状态与安装后失败](stage_reports/stages/stage-07/pd/2026-10-18-pd3a-software-transaction.md)。
PD-3-b 证据：[提交修订、中性读取与唯一 Snapshot 物化](stage_reports/stages/stage-07/pd/2026-10-19-pd3b-state-read-and-materialization.md)。
PD-3-c 实施与版本决定：[宿主细化与公共来源查询/版本决策](stage_reports/stages/stage-07/pd/2026-10-19-pd3c-contract-and-version-decision.md)。
PD-4 的 SDK 0.9.0、消费注册/交付、真实 CPU/attempt 回收、有界观察与 NoRetainedFrame 内部实施及适用本地门禁完成；见[实施与验证](stage_reports/stages/stage-07/pd/2026-10-19-pd4-delivery-retirement-and-observation.md)。[三项 owner 决策](stage_reports/stages/stage-07/pd/2026-10-19-pd4-readiness-and-owner-decisions.md)已接受；不代表物理设备、正式性能或发行接受。
设备观察：PD-4 Release 独立音乐 1 轮、DFJK 2 轮均退出 0；DFJK 首轮实际执行完整 terminal horizon，第二轮 Pause/Reset/Resume 正常。owner 报告控制附近 FPS 约 240→130 后恢复 240，待量化；音乐听感正常，underruns=2 经 owner 说明发生于切换 Codex 打字，不据此判定播放故障。见[PD-4 实体观察](stage_reports/stages/stage-07/rpa/device/2026-10-19-pd4-physical-music-and-keyboard-observations.md)。[旧版 DFJK 约 17.6 秒 capacity_exceeded/退出 1](stage_reports/stages/stage-07/rpa/device/2026-10-19-pd3c-physical-music-and-keyboard-observations.md)保持失败历史；设备失效/恢复与正式预算门禁仍未完成。
PD-5 准备：owner 已接受[本机范围、全部阈值和三窗口协议](stage_plans/active/stage-07/pd5-measurement-proposal.md)；[首批工具/profile](stage_reports/stages/stage-07/pd/2026-10-19-pd5-tools-and-profile-preparation.md)具备显式端点 collector、传递构建绑定、CPU/GPU 短流式与完整控制脚本验证。[窗口协议与矩阵扩展](stage_reports/stages/stage-07/pd/2026-10-19-pd5-window-and-matrix-preparation.md)已验证 P0 不可变 runner、七种独立 SDK/delivery 短轨迹与内部物化计数，完成稳定音频端点盘点和 Packed 规模工具回绑；[初始就绪检查](stage_reports/stages/stage-07/pd/2026-10-19-pd5-measurement-readiness.md)保持原时点证据。P0 已完成一次 12 进程 pilot 和两个各 36 进程的正式窗口；第三窗按 owner 要求暂停，后续转入核查与修复后重新测量。组合 renderer/captures、observer、完整内存、真实音频/输入及规模 Seek/Replay 仍待补齐，无完整正式接受或生产性能通过结论。

PD-5 Python 测量工具及 profile 已整理到 [tools/verification/playback_performance](../tools/verification/playback_performance/README.md)，测试和 C++ 探针留在 `tests/pd/`；非测试源码、工具与配置已按功能命名并更新引用，见[命名修正与验证](stage_reports/stages/stage-07/pd/2026-10-20-performance-tool-functional-naming.md)。测试目录的旧 Python 入口保留兼容；历史证据不改写，后续运行需重新 prepare。

[注册完整性与容量短测修正](stage_reports/stages/stage-07/pd/2026-10-20-measurement-preparation-integrity.md)补齐注册外层哈希、跨窗口/pilot 原始文件独立性及完整控制次数拒绝检查，64 实体 low/high 的 Snapshot/Seek/Replay 端到端工具短测通过；40k、真实 Player/设备观察和完整测量就绪仍待验证。

PD-5 最新证据：[P0 两窗结果、初步归因与诊断接手](stage_reports/stages/stage-07/pd/2026-10-20-pd5-two-window-findings-and-diagnostic-handoff.md)。Release RPA 只读 Snapshot 提取的两窗 B/A 约 1.54/1.58，重复性合格。受控隔离实验把同梯次 B−A 的 111.9 ns 分解为跨 TU 中性值路径约 65 ns（58%）、weak_ptr 提升约 25 ns（22%）、重复窗口校验约 17 ns（15%）、窗口注册/门禁约 9 ns（8%）；另有 9–21 ns 在读取窗口之外，保留语义的最优单变量仍为 1.34。该差额占 240 Hz 帧周期约 0.0027%，按成本偏离记录并待 owner 决定，不据此判定用户可见变慢。补充两项只读核查：宿主 `request→ready` 的 2.16–2.20× 来自等待区间由一次通道往返变成两次（回执端点改为 present-return），同口径整帧只多 0.26%，不是整帧翻倍；1/8/64/256 对象 A/B 提取显示退化为每次调用约 56 ns 固定项加每对象约 27 ns，2 对象即约 110 ns，渐近比值 1.65×，256 对象多 7.0 µs/帧（预算 0.17%）。宿主 ready 延迟还受同步等待边界改变影响，不能据此推断整帧成本翻倍。无上下文 GPT-6-Luna 已独立核查；未实施性能修复，未重跑已登记窗口。

## 阶段状态

| 阶段 | 当前状态 | 入口 |
| --- | --- | --- |
| Stage 7A | S7A-3/4、5/6、7/8 的受限功能验收完成；整体关闭仍待 RPA 收尾 | [Stage 7A 计划](stage_plans/active/stage-07/plan-a.md) |
| Stage 7-RPA | testOnly 实施与自动化证据已具备；实体设备、生产预算和最终接受未完成 | [RPA 计划](stage_plans/active/stage-07/plan-rpa.md)、[交接状态](stage_reports/stages/stage-07/rpa/2026-10-15-rpa-f4-handoff.md) |
| Stage 7-PD | PD-0 基线与 PD-1/PD-2 完成；PD-3-a 补正、PD-3-b 内部实施及本地适用门禁与独立复审通过；PD-3-c 内部实施与本地门禁完成，公共查询/0.8.1 已获接受，新建无上下文 GPT-6-Luna 独立审核通过；PD-4 内部实施、本地适用门禁及新的无上下文独立复审完成（已接受 SDK 0.9.0）；PD-5 P0 两窗完成，局部回归已定位到中性读取边界并完成成本分解，修复与重新测量未开始，完整正式接受未完成，子阶段未退出 | [PD 计划](stage_plans/active/stage-07/plan-presentation-decoupling.md) |
| Stage 7B+ | 未开始，等待 RPA 完整退出 | [Stage 7B+ 计划](stage_plans/active/stage-07/plan-b.md) |

Stage 0–6 已完成；Stage 8–14 尚未开始。整体 Stage 7 未关闭。
阶段依赖与后续安排见 [Stage 7 总计划](stage_plans/active/stage-07/plan.md)和[路线图](ROADMAP.md)。

## 当前问题与关闭前置


- **历史慢态及尾延迟未归因**：PD-0 最新登记组重复性通过，但不解释历史近两倍慢态；该组 Debug 宿主仍保留 15.93 ms 的单帧请求到回执尖峰。PD-5 本机有限 Release 门槛已接受，生产尾延迟预算未接受，不能据均值重复性通过推断生产延迟合格。
- **控制附近帧率波动待量化**：PD-4 实体 DFJK 完整课程与控制观察已完成，旧版容量错误本轮未复现；Reset/暂停恢复时约 240→130→240，持续时间与模块成本未测量。设备失效/恢复仍未验收。
- **局部偶发故障未定位**：B5 Pause 的 `capture_gap` 与 CXC unpack 的 `Access Denied` 尚无根因结论。
- **RPA 验收未完成**：已取得 PD-4 受限实体键盘与听音观察；精确音画同步、设备失效与压力验证、生产参数及预算接受仍待完成。
- **预算证据缺项**：端到端 Seek 延迟、prepare-only 峰值堆和 decoded semantic heap 尚未完成有效测量与接受。

问题证据与剩余门禁见 [PD-0 复测归档](stage_reports/stages/stage-07/pd/2026-10-17-pd0-supplemental-retests-and-archive.md)、
[RPA 矩阵](stage_reports/stages/stage-07/rpa/2026-10-15-f1-f2-final-matrix.md)及
[预算基线](stage_reports/stages/stage-07/rpa/2026-10-15-f3-final-baseline.md)。

## 产品边界

Cuexis 由可嵌入的 Playback SDK、独立 Cuexis Player 和 Cuexis Studio 构成。
外部宿主使用公共 Playback API；RuntimeSession、World、EnTT、SDL 和 OpenGL 属于内部或可选实现。

产品决策见 [ADR 0027](adr/0027-playback-sdk-product-boundary.md)，公共接口见 [API reference](api/README.md)。

## 当前格式与 SDK 合同

- 当前工作树 SDK API 为 **`0.9.0` preview**（PD-4 内部实施与适用本地门禁完成），日期构建为 **`26.10.19-1`**；这不代表已经发行。
- Gameplay v2 是当前实施基线，顶层合同仍为 candidate；局部实现不等于整批验收。语义与运行边界见
  [Gameplay V2 Spec](formats/GAMEPLAY_V2_SPEC.md)、[ABI](api/GAMEPLAY_V2_ABI.md)及[RPA 补充合同](formats/rpa-execution-contract.md)。
- Chart v1–v4 的既有兼容路径保留；Chart v5、CXT v2、Packed Chart 与 Canonical Gameplay Graph 仍为显式候选路径，正式发行收敛归 Stage 8。
- 稳定 C ABI 归 [Stage 14](stage_plans/future/stage-14/plan.md)，当前只提供 C++ typed preview。

各格式的生产/候选状态与权威文档见[格式索引](formats/README.md)。

## 延期和禁止边界

运行时脚本和逐帧 script callback 无限期延后，不预留字段、capability、bytecode、ABI 或 Playback hook。
离线 authoring generator 不会被 pack、prepare 或 Playback 隐式执行。

## 更新规则

只更新当前结论，完成或被替代的状态应就地更新或删除。
测试轮次、命令、提交经过、授权沟通和历史问题处置保存在阶段报告中，不向本文追加过程日志。
