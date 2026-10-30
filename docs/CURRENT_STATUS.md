# Cuexis Current Status

状态：current

更新日期：2026-10-29

本文只维护当前阶段、正在进行的任务和未解决的问题。实施与测量细节见带日期的阶段报告。

## 当前任务

- **R3 继续重构优化，尚未退出**：已冻结 prepare 输入的两次整份复制已消除，生命周期、相关回归和身份/Packed 字节对照通过。
  新 0.9.1 普通固定对照的 setup 中位数在 1K/10K/40K 分别下降约 7.6%/6.8%/6.2%；1K 基线重复性超 15%，
  其历史高点尚不能唯一归因，见[本轮准备复制优化](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-28-r3-resolved-prepare-copy.md)。
  [轻量探针](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-28-r3-seek-noise-attribution.md)已确认 40K 主成本在恢复后的 Fact 投影：
  零绑定仍读取/构造 40K Fact/key。同映像切换未稳定复现原约 11% 差距，A/A 也有慢点；诊断出现 20.591 ms，
  原因机制及正式/Seek 接受仍未关闭，不能改判原窗口或门限。
  后续[零绑定投影修复与通用方案撤回](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-29-r3-projection-empty-and-general-attempt.md)
  保留完整 Fact 校验，只跳过空索引下的 owning key 构造；固定普通 CPU Host 对照的 40K 首次中位数
  从 16.531 ms 降至 2.562 ms（下降 84.5%）。相对重复性仍超 15%，不代表正式接受。
  通用借用 key / 有序合并虽改善微基准，但 10K Host 首次三对均上升，已撤回并留档。
  [40K 可见实体真实 GPU 测试](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-29-r3-40k-physical-gpu-throughput.md)
  已完成：RTX 4060 Laptop、1280×720、VSync OFF，12 个有效进程 / 1440 帧均超 4.167 ms。
  无 timestamp 对照的复用快照、静态提帧、全量 Chart v1 行为分别约 18.25 / 13.39 / 4.47 FPS；
  当前不能达到 240 FPS，正常帧的 update / extract / submit 成本尚待优化。初版相机夹具作废并保留，
  有效结果来自补充可见性与材质像素验证后的新登记；不作为 R3/R4 或生产接受。
  后续 [1080P 实体容量](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-29-r3-1080p-entity-capacity.md)
  已完成 10–40K 粗扫、边界重复和持续验证：按完整帧周期 p99≤4.167 ms、平均及每秒窗口≥240 FPS，
  已确认仅渲染 1800 实体、静态 update / extract / render 1300 实体，各通过三次独立 20 秒。
  1900 render-only、1400 / 1500 static 有持续超预算，全部保留；此为本夹具的保守测试点，不是绝对容量或正式退出。
  已有分页共享状态、不可变准备复用、自然恢复锚点，以及 SHA 加速和有界编码器恢复点 `198ab96`。
  P3 后续[Runtime 外观重复查询优化](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-29-r3-runtime-appearance-lookup.md)
  已将同实体四次查询合为一次，固定 18 进程对照的 40K 静态 update 中位数由 11.414 降至 5.555 ms，
  完整 cycle 约下降 7.8%；1K 候选 update 重复性仍超 15%，不作为正式接受或容量重测。
  继续[消除本帧触及集合平方扫描](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-29-r3-runtime-touched-membership.md)，
  新固定对照的 40K 全量动画 update 由 147.933 降至 47.343 ms，完整 cycle 由 209.573 降至 109.652 ms；
  静态路径未见对应退化。只加标记的初版因晚插入基线的语义回归停止并撤回；最终版同步索引并通过反例及相关回归。
  当前渲染仍超预算，P3 和其余待优化项继续开放。
  owner 已要求连续完成六项剩余工作，当前候选与完整验证进度统一见
  [六项实施与验证账本](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-29-r3-six-item-completion-work.md)。
  Pattern 编译复用、外观索引提示和新编码缓存因固定对照未达预设收益/保护条件已撤回；
  名称索引、Packed 字典查找及内部 identity 投影复用分别通过独立诊断并提交；
  OpenGL 普通提交的同步诊断读回已分离，烟雾测试/像素探针显式开启，最终完整矩阵仍待执行。
  Host Seek 的 CPU 功能矩阵扩为 60 组合，新增首条长记录内部 H=5/902，实际 GPU 仍为 96 组合。
  这两处目标的 timer/ledger 无用身份复制已消除；后续标量 phase 写入再减少整行复制，独立对照中
  40K H=5 为 22.873 → 16.678 ms，H=902 为 265.502 → 232.641 ms。后者仍超 20 ms，
  原性能门及最终完整矩阵未关闭；不同登记组不拼接为同一连续基线。
  最新[来源绑定实验](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-27-r3-source-binding-experiment.md)用同一 Release 二进制比较 canonical SHA 与进程内 owning source binding。
  2048 requirements、33 条历史的 CPU 宿主首次请求到目标帧由 91.809 ms 降至 1.016 ms；129 条历史由 333.624 ms 降至 1.057 ms。
  这是固定本地诊断的点值，wire/导入验证保持原合同，candidate C++ 消费者须重建；R3 未退出、R3c 尚未进入，R4、设备与生产接受仍独立。
  原始归因及历史失败保留，后续门禁见[实施登记](stage_plans/active/stage-07/05-rpa-new/r3/plan.md)。
  本轮保留 GPU 测试配额修复和 content-only closure 无复制优化（产品恢复点 `5fd84fd`）；身份 writer 范围插入试验
  未取得稳定收益，已撤销。该轮恢复后的四套相关回归、架构门与 96 份身份字节对照通过；新复制候选另行登记。
  架构 / 设计变更、对照结果与停止边界见[本轮登记 §5](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-28-r3-redesign-verification-and-optimization.md#5-后续取证和修复顺序)。
  现行内部设计集中在[执行状态、恢复与准备路径](architecture/execution-state-and-recovery.md)，
  架构/API 入口已同步；后续继续跟踪首次 Seek 与 Packed/prepare 剩余热点。
- **PD-5 定案与 Stage 7-PD 退出裁定**：判定报告已提出“未通过但定案”及退出清单，尚待 owner 接受；读取与 Seek 整改由 RPA 线承接，
  分域内存及正式/设备/生产验收缺口仍按下文跟踪。见[判定报告](stage_reports/stages/stage-07/04-pd/pd-5/exit/2026-10-24-pd5-verdict-and-stage-7-pd-exit.md)。

本重构分支的唯一实施入口为 `D:/Cuexis-redesign`，分支 `Cuexis-redesign`。
接手资料中的其他 worktree 和分支均为历史来源，本批不在其中执行操作。
已完成工作、候选处置和逐轮测试记录见[Stage 7 报告索引](stage_reports/stages/stage-07/README.md)。

## 阶段状态

| 阶段 | 当前状态 | 入口 |
| --- | --- | --- |
| Stage 7A | S7A-3/4、5/6、7/8 的受限功能验收完成；整体关闭仍待 RPA 收尾 | [Stage 7A 计划](stage_plans/active/stage-07/01-a/plan.md) |
| Stage 7-RPA | R1、R2 已退出；owner 于 2026-10-26 接受 R2 按原计划退出，受控局部结果及历史限制保留；R3 实施中，R4 正式、设备与生产接受未登记 | [整改计划](stage_plans/active/stage-07/05-rpa-new/plan.md)、[退出复核](stage_reports/stages/stage-07/05-rpa-new/r2/2026-10-26-r2-technical-closure-review.md) |
| Stage 7-PD | PD-0–4 内部实施及适用门禁完成；PD-5 判定为「未通过但定案」，定案与 Stage 7-PD 退出清单尚待 owner 批准。原 40k 功能与小规模最终释放已核验，40k 分域内存仍未测量、正常分配器正式性能未通过。B0 已接受、B1 静态工具完成且未采数，B2/B3 未授权；ETW r4 三用例通过只证明工具合同。读取优化归 RPA R2，Seek 优化归其后续整改，旧窗口 3 停止，子阶段未退出 | [PD 计划](stage_plans/active/stage-07/04-pd/plan.md)、[PD-5 判定报告](stage_reports/stages/stage-07/04-pd/pd-5/exit/2026-10-24-pd5-verdict-and-stage-7-pd-exit.md) |
| Stage 7B+ | 未开始，等待 RPA 完整退出 | [Stage 7B+ 计划](stage_plans/active/stage-07/06-b/plan.md) |

Stage 0–6 已完成；Stage 8–14 尚未开始。整体 Stage 7 未关闭。
阶段依赖与后续安排见 [Stage 7 总计划](stage_plans/active/stage-07/plan.md)和[路线图](ROADMAP.md)。

## 当前问题与关闭前置

- **完整 Packed / prepare 已继续优化**：[前轮索引优化](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-27-r3-packed-index-optimization.md)后，进一步[细分 prepare](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-27-r3-prepare-declaration-optimization.md)确认 assembler 声明查找及回填仍有平方扫描，现已改为有序表二分。本轮独立普通对照中，10K 完整准备从 3.05–3.16 s 降至 1.74–1.75 s，40K 从 27.6–27.8 s 降至 7.44–7.63 s，首次请求到 CPU 测试呈现约 15.5–16.4 ms。四配置相关回归及三规模字节/96 场景兼容对照通过；timer、identity/closure 及编解码仍有成本。点值不替代正式、设备、内存或生产接受，R3 未退出；一次旧诊断 exe 误覆盖及证据保全见报告 §6。
  2026-10-28 全量核验已登记：完整桌面候选 1393 项中 1388 通过、4 个 external consumer 失败、1 个跳过；许可证清单修复后 verification-full 的四个 Release consumers、redesign-shared 的四个 SHARED consumers 和两个 Shared import checks 均通过。设备探针的过时平铺字段断言已改为 owning `snapshotView()`，精确实体设备用例 1 case / 37 assertions 通过。GPU capture 的 renderer 测试配额已修正，capture-1/2/4 各 8 frames 的独立功能复核通过，生产额度校验未放宽；不替代设备性能接受。40K decode/re-prepare 和 updated 提帧仍是下一轮热点。详见[2026-10-28 全量核验与优化登记](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-28-r3-redesign-verification-and-optimization.md)。


- **历史慢态及尾延迟未归因**：PD-0 最新登记组重复性通过，但不解释历史近两倍慢态；该组 Debug 宿主仍保留 15.93 ms 的单帧请求到回执尖峰。PD-5 本机有限 Release 门槛已接受，生产尾延迟预算未接受，不能据均值重复性通过推断生产延迟合格。
- **控制附近帧率波动待量化**：PD-4 实体 DFJK 完整课程与控制观察已完成，旧版容量错误本轮未复现；Reset/暂停恢复时约 240→130→240，持续时间与模块成本未测量。设备失效/恢复仍未验收。
- **局部偶发故障追踪**：B5 Pause 的 `capture_gap` 仍未定位。CXC unpack 目录提交已用不共享删除的读句柄复现阻塞机制，并加入有限原子重试与确定性测试；原始现场的具体持有者仍未知，详见[整合记录 §3.2](stage_reports/stages/stage-07/90-shared/worktree-consolidation/2026-10-26-worktree-consolidation.md#32-shared-probe-and-cxc-output-commit-regression-repairs)。
- **40k 功能已核验，内存仍未测量、正式性能未通过**：原 low/high 不重跑，原超包络记录保留；后续归因将其改记为观察器量程产物，不能据此判产品超界或内存通过。小规模最终释放不替代 40k 分域预算和最终释放。B0 静态账本已接受，B1 通过静态工具门但未运行；B2/B3 的 caps 规模口径、覆盖/阶梯缺口及采数仍待授权和裁定。F1/C1/C2 原回归与噪声结论保留，读取整改由 RPA R2 承接；ETW r4 三用例已过，工具结果不替代性能接受。见[40k 包络归因](stage_reports/stages/stage-07/04-pd/pd-5/memory-attribution/2026-10-23-40k-memory-envelope-attribution.md)、[B0 账本](stage_reports/stages/stage-07/04-pd/pd-5/memory-attribution/2026-10-24-b0-memory-cap-ledger.md)、[B1 工具](stage_reports/stages/stage-07/04-pd/pd-5/memory-attribution/2026-10-24-b1-domain-attribution-tooling.md)与[判定/退出清单](stage_reports/stages/stage-07/04-pd/pd-5/exit/2026-10-24-pd5-verdict-and-stage-7-pd-exit.md)。
- **RPA 验收未完成**：已取得 PD-4 受限实体键盘与听音观察；精确音画同步、设备失效与压力验证、生产参数及预算接受仍待完成。R0 的 Seek/ON-OFF、Close 软件退休观测、旧 fence 与导出容量 partial 均按[原报告](stage_reports/stages/stage-07/05-rpa-new/r0/2026-10-24-r0-observation-and-anchor.md)保留历史结论。Q4 仍为 `inconclusive`（历史 B 探针缺失）。R1 已新增真实 owner 许可反例、独立 Close step 观测及四类续行证据，未回填 R0；Debug 和首错缺口已解决并经当前五构建复验；当前修复身份有限 pilot 通过，R1 已按本次授权完成退出核验，RPA 最终接受仍未完成；历史全仓格式限定及 noIndependentContract 导出事实见[新记录](stage_reports/stages/stage-07/05-rpa-new/r1/2026-10-25-r1-continuation.md)。
- **预算证据缺项**：端到端 Seek 延迟、prepare-only 峰值堆和 decoded semantic heap 尚未完成有效测量与接受。

问题证据与剩余门禁见 [PD-0 复测归档](stage_reports/stages/stage-07/04-pd/pd-0/2026-10-17-pd0-supplemental-retests-and-archive.md)、
[RPA 矩阵](stage_reports/stages/stage-07/03-rpa-legacy/f1-f2/2026-10-15-f1-f2-final-matrix.md)及
[预算基线](stage_reports/stages/stage-07/03-rpa-legacy/f3/2026-10-15-f3-final-baseline.md)。

## 产品边界

Cuexis 由可嵌入的 Playback SDK、独立 Cuexis Player 和 Cuexis Studio 构成。
外部宿主使用公共 Playback API；RuntimeSession、World、EnTT、SDL 和 OpenGL 属于内部或可选实现。

产品决策见 [ADR 0027](adr/0027-playback-sdk-product-boundary.md)，公共接口见 [API reference](api/README.md)。

## 当前格式与 SDK 合同

- 当前工作树 SDK API 为 **`0.9.1` preview**：按 2026-10-28 owner 指定，登记 R3 来源绑定新增 candidate 类型和 `StepTarget` 字段；
  消费者须整体重建，不能混用旧二进制。版本决定见[版本政策](guides/VERSIONING.md#091-r3-candidate-来源绑定)。
  PD-4 历史基线仍为 0.9.0，日期构建仍为 **`26.10.19-1`**；这不代表已经发行。
  新版本 STATIC/SHARED 干净构建、安装消费者、版本接受/拒绝和架构/导出定向门已通过，详见
  [0.9.1 与架构同步记录](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-28-sdk-091-architecture-sync.md)；未重跑完整矩阵或性能接受。
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
