# Cuexis Current Status

状态：current

更新日期：2026-11-07

本文只维护当前阶段、正在进行的任务和未解决的问题。实施与测量细节见带日期的阶段报告。

## 当前任务

- **RPA 当前承接为 R4 集成取证**：R1、R2、R3 已退出，O1 方案 B 的 reference 设计预算保持。
  本轮候选 `24f0a914` 已同步至 PR #32，日期版本为 `26.11.07-1`。
  原 128-slot 优化的同子集 71.04% 降幅保留；扩大拥有覆盖后，64/high 与 low 的 active World
  唯一下界均为 119,534 B，完整图仍为 unrated、正式 memory/P8 未准入，未改预算或运行40K。
  首个正式 CPU 三窗九对组已完整结束：RPA validated_trace/playback_only 通过；legacy 对应两项
  因重复性为 inconclusive；两种 read_long 均超出原 1.10 相对门，失败及重开额度分别保留。
  隔离读取原型首轮诊断反而变慢（legacy +1.7%、RPA +5.6%），依预注册停止、不采用。
  11行本地矩阵全部通过（13,652项测试注册、0失败、26项显式skip）；同SHA的六个push和三个PR检查通过。
  其他 P0–P7、Seek、完整内存、设备和生产接受未完成；O2 实体清单及 F4/PD 移交门账已准备。
  本地最终矩阵和同 SHA hosted 状态见[本轮报告](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-07-owning-and-cpu.md)。
  Version Gate 仍缺 owner 针对所选候选的有效 SDK 批准，按 owner 要求暂不修、不代签或豁免。
  [实施方案](stage_plans/active/stage-07/05-rpa-new/r4/plan.md)及
  [固定运行登记](stage_plans/active/stage-07/05-rpa-new/r4/formal-registration.md)继续持有剩余门。
  实体操作及移交准备见[O2清单](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-07-o2-physical-checklist.md)
  和[F4门账](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-07-f4-gate-and-handoff.md)。
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
原 `D:/Cuexis-redesign` 工作树与同名分支已删除；R4 构建分别使用独立的 `out/build/r4/pre-o1/` 与 `out/build/r4/post-o1/`。
迁移后的原始运行字节位置见[证据清单](stage_reports/stages/stage-07/05-rpa-new/evidence-catalog.md)。
源码拆分已完成，审计与复核材料见
[源码拆分安全网独立审计](stage_reports/stages/stage-07/90-shared/source-split-audit/index.md)；
逐轮实施、候选处置和测试记录从[Stage 7 报告索引](stage_reports/stages/stage-07/README.md)导航。

## 阶段状态

| 阶段 | 当前状态 | 入口 |
| --- | --- | --- |
| Stage 7A | S7A-3/4、5/6、7/8 的受限功能验收完成；整体关闭仍待 RPA 收尾 | [Stage 7A 计划](stage_plans/active/stage-07/01-a/plan.md) |
| Stage 7-RPA | R1、R2、R3 已退出；R4 O1 有效；扩大 World 下界为119,534 B且仍unrated；首CPU组读取超界，其他正式/设备/生产门及最终接受未完成 | [整改计划](stage_plans/active/stage-07/05-rpa-new/plan.md)、[缩页效果](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-06-world-storage-effect.md)、[R3 退出裁定](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-04-r3-hosted-delta-and-exit-ruling.md) |
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

- 当前工作树 SDK API 为 **`0.9.1` preview**，日期构建已推进至 **`26.11.07-2`**；这不代表已经发行。
  新日期版本随报告准备提交E，尚未作为本地11行矩阵候选；矩阵与同SHA hosted evidence绑定C=`24f0a914`
  / `26.11.07-1`，不能把E写成同SHA已验证。SDK批准仍由Version Gate独立处理。
  R3 来源绑定新增 candidate 类型和 `StepTarget` 字段，消费者须整体重建，不能混用旧二进制。
  PD-4 历史基线仍为 0.9.0。版本决定见[版本政策](guides/VERSIONING.md#091-r3-candidate-来源绑定)，
  STATIC/SHARED、安装消费者及兼容门的最新本地证据见上方 R3 最终矩阵。
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
