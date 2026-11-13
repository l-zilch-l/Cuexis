# Cuexis Current Status

状态：current

更新日期：2026-11-07

本文只维护当前阶段、正在进行的任务和未解决的问题。实施与测量细节见带日期的阶段报告。

## 当前任务

- **RPA 当前承接为 R4 集成取证**：R1、R2、R3 已退出，O1 方案 B 的 reference 设计预算保持。
  取证候选为 `2c4e1274` / `26.11.09-1`：F1 本地 11 行矩阵全部 `local_pass`，
  同 SHA 的 Linux Quality、Windows MSVC、Windows MinGW hosted 全部通过，
  Version Gate 也已 success（当天日期 + build 1 + owner 批准记录）。
  早前 `24f0a914` / `26.11.07-1` 的矩阵与 `2ec3480e` / `26.11.07-3` 的 hosted 作为历史证据保留。
  日期版本随后推进到 `26.11.10-1` 以满足 Version Gate 的当天日期要求；该提交只改版本标识，
  产品源（`engine/`、`app/`、`tests/`）与 `2c4e1274` 逐字节相同。
  见[最终候选验证](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-10-f1-f2-final-candidate.md)。
  原 128-slot 优化的同子集 71.04% 降幅保留；扩大拥有覆盖后，64/high 与 low 的 active World
  唯一下界均为 119,534 B，完整图仍为 unrated、正式 memory/P8 未准入，未改预算或运行40K。
  首个正式 CPU 三窗九对组已完整结束：RPA validated_trace/playback_only 通过；legacy 对应两项
  因重复性为 inconclusive；两种 read_long 超出原 1.10 相对门，失败记录保留。
  2026-11-12 `read_long` 已按 owner 批准的重开额度完成第二次正式组并结案：`legacy` 通过
  （0.91913，区间 [0.90916, 0.93081]，A/B 波动 11.62%/9.44%）；`RPA` 首轮点估计 0.92582、
  区间 [0.90748, 0.94914]，因 B 侧一个受环境扰动的进程重复性 19.25% 超界判 inconclusive，
  owner 批准后只跑 RPA 复跑：几何平均 0.90990、区间 [0.87034, 0.95326]、A/B 波动 10.80%/13.75%，
  **判通过**。两组的 A 侧均为按 `d3440882` 重建并重新绑定（原冻结目录已不在本机），owner 已接受该口径。
  见[正式组结果](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-12-read-long-formal-group.md)与
  [`read_long` 裁定记录](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-12-read-long-ruling.md)。
  同日复跑 `legacy` 的另两项计时：`validated_trace` 0.999764（区间 [0.978540, 1.018122]，
  A/B 波动 11.79%/5.10%）、`playback_only` 0.991082（区间 [0.973358, 1.012269]，11.28%/7.66%），
  两项**均通过**，P0 四个计时单元至此全部结案；同组日志里的 `read_long` 为 0.923907，与专项组一致。见
  [P0 legacy 计时复跑](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-12-p0-trace-legacy.md)。
  五类 Seek 于 2026-11-13 在 RTX 4060 上重验：`natural`/`hot`/`external` 三类重复性与 20 ms 绝对门
  全部通过（单进程最大 1,014 µs），`fallback`（20.27%）与 `cold`（39.92%/35.92%）绝对门通过但重复性
  inconclusive。见[五类 Seek 结果](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-13-seek-five-classes.md)。
  隔离读取原型首轮诊断反而变慢（legacy +1.7%、RPA +5.6%），依预注册停止、不采用。
  11行本地矩阵全部通过（13,652项测试注册、0失败、26项显式skip）；同SHA的六个push和三个PR检查通过。
  2026-11-08 新增展示矩阵探针适配：P1-no-consumer、P2-low-delivery、P4-capture-1、P6-sparse
  已两次登记并各完成 pilot；attempt-1 因会话锁定、attempt-2 因人工切换前台窗口停止，
  两次都没有形成三窗口，因此**没有任何正式裁定**，每单元两次额度已用尽。
  P4-capture-2/capture-4/P6-topology 已注册并通过全规模身份预检但未运行。
  同批另登记 `p0-host-common`（B-only、1024 帧）：attempt-1 因前台持续变化停止；
  attempt-2 在绑定已验证补救后完成三窗口九进程，request→ready p99 为 87.9–99.1 µs，
  **九个进程全部通过已接受的 4.167 ms 预算**，两条披露腿重复性通过，
  ready→present-return（约 4 µs）按 15% 门为 inconclusive；该单元额度已用尽。
  owner 随后豁免额度（`2026-11-08-owner-attempt-waiver.json`）：四个已耗尽单元与三个
  未运行单元在同一候选上全部取得三窗口结果——六个计时组重复性通过（全九 ≤11.1%），
  `p4-capture-1` 全九 16.7% 为 inconclusive。内存逐域归因亦完成：精确分区下
  `state.current` 在 n64/n2048 超出 proposed 上限 2.17×/3.11×，n2048 的
  `command.session` 超出 1.24×。
  P8 已在 `2c4e1274` 上以 40,000 实体 + 4,096 内容行跑通（exit 0）：总量 365 MB，
  逐分配追踪（2026-11-10）证明该域只有 118 个分配：固定 19,766,750 B（128 个命令槽 +
  65,536 条保留前进记录）+ 每实体 3,524 B 的执行内核数组；早先“交付缓冲按 `frameByteBound` 分配”
  的解释**已撤回**（`FrameStorage` 只记账不分配）。约 141 MB 之所以记在 `command.session`，是
  `dispatch()` 的单个命令级标签所致；把 `SessionUpdatePresentation` 与 `SessionSeek` 改记到执行域后，
  同一运行（总字节不变，365,084,102 B）为 `command.session` 19.76/21.85 MB（0.90）、
  `execution.retained` 141.08/1,327.50 MB（0.11），**21 个域全部在声明内**。
  该归属修正已由 owner 于 2026-11-11 接受，P8 内容口径同时定为正式的 `(40000, 4096)`
  （扩充规则按二维系数推算）。见
  [P8 报告](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-10-p8-40k-entity-memory.md)与
  [owner 裁定记录](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-11-owner-decisions.md)。
  继续该批不再由代理决定：需 owner 在“带限制退出 / 豁免第三次登记 / 只改 CPU-only 环境判据 /
  缩短稳态窗口 / 换隔离环境”中选择；未验证的处置不会被写成补救。见
  [探针批报告](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-08-probe-matrix-admission-and-environment-stop.md)
  与[普通 host 报告](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-08-common-host-unit.md)。
  其他 P0–P7、Seek、完整内存、设备和生产接受未完成；O2 实体清单及 F4/PD 移交门账已准备。
  本轮按 owner 指示清理了可再生成的旧构建树（磁盘 22.8 GB → 226.7 GB），
  `out/evidence`、来源树、A 基线二进制与 attempt 台账保持；清理不追改历史判定。
  本地最终矩阵和同 SHA hosted 状态见[本轮报告](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-07-owning-and-cpu.md)。
  Version Gate 在 `2c4e1274` 上已 success；批准记录是 PR #32 上一条未编辑的 owner 评论，候选 SHA 变化时需要重新记录。
  2026-11-11：归属修正作为 PR #34 合并（`0d9a329e`），F1 11/11、同 SHA 三平台 hosted 与 Version Gate 通过。
  P5 脚本化控制单元已登记为独立适配器 `controls`（计数由周期数推导，72 条样本/进程），
  pilot 在安静前台窗口下通过：`PD5_ID` 与声明逐字段一致，pause/resume 中位 194.5/323.5 µs、
  reset 3.16/2.94 ms、resize 152 µs；同一单元前两次分别因机器预热与 sample 阶段的前台变化停止，
  记录保留。P5 保留探针形态（探针在该构建下不创建窗口），不做夹具改造。见
  [控制探针单元](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-11-p5-controls-unit.md)。
  [实施方案](stage_plans/active/stage-07/05-rpa-new/r4/plan.md)及
  [固定运行登记](stage_plans/active/stage-07/05-rpa-new/r4/formal-registration.md)继续持有剩余门。
  实体操作及移交准备见[O2清单](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-07-o2-physical-checklist.md)
  和[F4门账](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-07-f4-gate-and-handoff.md)。
  跨对话承接、最新 hosted 身份及未裁定方向见
  [交接留档](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-07-session-handoff.md)。
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
  128 MiB 不再作为 R3 上限判据，也未设新数字上限；R4/PD 的正式 profile/caps 仍须注册并获接受。
  D2/D3/Snapshot/排队四项沿用可开关计时、归因与开销披露的限定例外，其余预算不外推。见
  [owner 四项门裁定](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-02-r3-owner-gate-rulings.md)、
  [最新容量前缘诊断](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-01-r3-capacity-frontier-rederive.md)。
- **PD-5 与 Stage 7-PD 退出仍待裁定**：历史结论为“未通过但定案”，退出清单尚待 owner 接受；
  读取、Seek 与关闭整改由 RPA 承接，旧 PD-5 窗口及其失败不追改。见
  [PD-5 判定与退出清单](stage_reports/stages/stage-07/04-pd/pd-5/exit/2026-10-24-pd5-verdict-and-stage-7-pd-exit.md)。

当前工作树为 `D:/Cuexis-worktree`，执行分支 `codex/r4-integration`，起点为本地 `master` 的 `8b499a86`。
owner 指定提交同步至 PR #32 的远端 `stage-7`；不得只推送独立 R4 分支而遗漏该 PR。
owner 在本次交接最后明确要求“不要推送”；本轮只本地提交，后续推送等待 owner 新指示。
原 `D:/Cuexis-redesign` 工作树与同名分支已删除；R4 构建分别使用独立的 `out/build/r4/pre-o1/` 与 `out/build/r4/post-o1/`。
迁移后的原始运行字节位置见[证据清单](stage_reports/stages/stage-07/05-rpa-new/evidence-catalog.md)。
源码拆分已完成，审计与复核材料见
[源码拆分安全网独立审计](stage_reports/stages/stage-07/90-shared/source-split-audit/index.md)；
逐轮实施、候选处置和测试记录从[Stage 7 报告索引](stage_reports/stages/stage-07/README.md)导航。

## 阶段状态

| 阶段 | 当前状态 | 入口 |
| --- | --- | --- |
| Stage 7A | S7A-3/4、5/6、7/8 的受限功能验收完成；整体关闭仍待 RPA 收尾 | [Stage 7A 计划](stage_plans/active/stage-07/01-a/plan.md) |
| Stage 7-RPA | R1、R2、R3 已退出；R4 O1 有效；扩大 World 下界为119,534 B且仍unrated；首CPU组读取超界；探针四单元 attempt-1 因会话锁定停止、无裁定；其他正式/设备/生产门及最终接受未完成 | [整改计划](stage_plans/active/stage-07/05-rpa-new/plan.md)、[缩页效果](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-06-world-storage-effect.md)、[R3 退出裁定](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-04-r3-hosted-delta-and-exit-ruling.md)、[探针批](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-08-probe-matrix-admission-and-environment-stop.md) |
| Stage 7-PD | PD-0–4 内部实施及适用门禁完成；PD-5“未通过但定案”，退出清单待 owner 接受；分域内存及正式性能缺口由 RPA/PD 后续取证承接 | [PD 计划](stage_plans/active/stage-07/04-pd/plan.md)、[PD-5 判定报告](stage_reports/stages/stage-07/04-pd/pd-5/exit/2026-10-24-pd5-verdict-and-stage-7-pd-exit.md) |
| Stage 7B+ | 未开始，等待 RPA 完整退出 | [Stage 7B+ 计划](stage_plans/active/stage-07/06-b/plan.md) |

Stage 0–6 已完成；Stage 8–14 尚未开始。整体 Stage 7 未关闭。
阶段依赖与后续安排见[Stage 7 总计划](stage_plans/active/stage-07/plan.md)和[路线图](ROADMAP.md)。

## 当前问题与关闭前置

- **R4 集成与正式证据未完成**：B3/B4 可控机制、B5 实际部署和 F1–F4 对应门须按依赖核实与取证。
  正式矩阵沿用共同 A 和三窗口九对协议，重复性口径及每进程请求数按
  [整改计划 §9](stage_plans/active/stage-07/05-rpa-new/plan.md#9-r4集成后取证区分时间内存与设备)
  登记；R1/R2 局部结果与 R3 本地门不能拼接成 R4 接受。
- **分域内存仍缺完整拥有证据**：O1 接受的是 64/2048/40K reference 设计预算。
  已实现 `page_size=128`，旧同子集下界从358,241降至103,738 B的效果有效；本轮扩大覆盖后
  high/low均为119,534 B，增加来自World本体/cache/order等新覆盖，不是产品回归。
  完整EnTT backing、scratch/transaction、其他语义域、shared retained/prepare峰值及设备/observer
  扰动尚未闭合，不能以下界低于327,680 B当作预算通过。两容量诊断已用尽，正式memory/P8
  未准入，预算保持；证据与覆盖边界见[本轮报告](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-07-owning-and-cpu.md)。
- **真实设备与生产接受未完成**：已有 PD-4 受限实体 DFJK 键盘及听音观察，精确音画同步、设备失效/恢复、
  压力验证、生产参数与预算仍待验收；Reset/暂停恢复附近的帧率波动持续时间及模块成本尚未量化。
  R3 初次 load 的已通过读数不代表极端 40K 启动达标；40K 启动与 prepare 的有限证据见
  [启动可观测性与成本](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-01-r3-40k-startup-observability-and-cost.md)。
- **正式采样需要解锁的交互会话**：冻结的 `sampled-windows-environment-v1` 策略要求前台进程
  全程不变；交互会话被系统锁定时 `GetForegroundWindow()` 返回 0，R4 attempt-1 因此在第二窗口停止。
  恢复需先解除锁定、保持本窗口前台，并验证不锁屏处置（`SetThreadExecutionState` 或临时关闭
  显示器/待机超时与屏保）后才能生成新的 retry disposition 与登记；本轮未把未验证的处置写成补救。
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

- 当前工作树 SDK API 为 **`0.9.1` preview**，本地日期构建为 **`26.11.09-1`**，未推送或发行。
  完整本地11行矩阵绑定 `24f0a914` / `26.11.07-1`；远端 `2ec3480e` / `26.11.07-3`
  具有独立的同SHA hosted通过证据，但未重跑完整本地矩阵。本地 `00af7f90` 与 `b7edc959`
  只带有探针准入、pilot 与两次被停止的固定组，不继承上述矩阵或 hosted 结论。
  SDK批准仍由Version Gate独立处理。
  R3 来源绑定新增 candidate 类型和 `StepTarget` 字段，消费者须整体重建，不能混用旧二进制。
  PD-4 历史基线仍为 0.9.0。版本决定见[版本政策](guides/VERSIONING.md#091-r3-candidate-来源绑定)，
  STATIC/SHARED、安装消费者及兼容门的最新完整本地证据见上方 R4 候选 `24f0a914` 的矩阵。
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
