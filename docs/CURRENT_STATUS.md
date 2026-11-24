# Cuexis Current Status

<!-- cuexis-meta
id: STATUS
status: current
authority: owns
current_stage: Stage 7-RR
current_item: stage_plans/active/stage-07-rr/plan.md
current_register: stage_plans/active/stage-07-rr/findings.md
-->

状态：current

更新日期：2026-11-24

当前任务、下一步与阻塞见 [交接页](HANDOFF.md)。

本文只维护当前阶段、正在进行的任务和未解决的问题。实施与测量细节见带日期的阶段报告。

## 当前任务

当前为 [Stage 7-RR](stage_plans/active/stage-07-rr/plan.md)，包含合同与正确性修正、架构减重、完整结构清理、Gameplay v3 实验实现，以及文档治理。发现与处置以[登记册](stage_plans/active/stage-07-rr/findings.md)为准，任务与依赖见[阶段目录](stage_plans/active/stage-07-rr/README.md)和[唯一调度](stage_plans/active/stage-07-rr/schedule.md)。

RR 的 [T-18 文档治理](stage_plans/active/stage-07-rr/tasks/T-18.md)已完成：规则、登记册保护、校验器、报告分类及证据追踪已落地，转入日常维护。实际通过范围与缺输入项见[交付报告](stage_reports/stages/stage-07-rr/implementation/2026-11-23-documentation-governance.md)。

RR 产品任务已完成本轮设计裁定与实施准备复查。T-01 已完成：实施基线绑定提交 `a22e255f`，交付派单清单、调度正反例、独立冷启动与 D-20 旧内容清单，见[基线与派单报告](stage_reports/stages/stage-07-rr/readiness/2026-11-23-t-01-dispatch-baseline.md)。T-03 已完成 8 篇合同订正与四个回归用例，并在四个预设实测通过，见[交付报告](stage_reports/stages/stage-07-rr/implementation/2026-11-23-t-03-contracts-and-diagnostics.md)。T-04 已完成构建闭包拆分与隔离门禁：四个预设与两个独立配置构建通过，安装消费者五档与阴性对照通过，见[交付报告](stage_reports/stages/stage-07-rr/implementation/2026-11-24-t-04-build-isolation.md)。波次 5 已完成：T-02 对齐四篇状态事实，T-05 交付像素几何与 GL 边界（`framebuffer_incomplete` 记无法核实），T-17 修复会话层 F501/F509/F203；F508 在第二轮按“内容卸载/控制生命周期终结”拆分落地，`rr_session_unload_lifecycle` 六个 SECTION 通过，登记册 §43。T-06 部分完成：帧对象、GL 实验桥、安装面、F135 场景 1/2/3/5、宿主/Player 帧链与 shared Player smoke 成立；场景 4、诊断用例、F207 与去重未闭环，见[部分交付](stage_reports/stages/stage-07-rr/implementation/2026-11-24-t-06-frame-bridge-partial.md)。T-08 已完成独立 RR-4 公共面主体（候选 facade、逐符号导出、hooks 拆分、安装与共享导出对照），见[交付报告](stage_reports/stages/stage-07-rr/implementation/2026-11-24-t-08-rr4-surface.md)；`--fresh` 四配置与全量 CTest 留 T-09 补证。T-06 第三轮补齐诊断、资源去重与 retained 用例。T-10 已完成九轮：值层、计分、作者 CLI、`runtime` records 与 Prepared 存储、`cuexis_judgement_compile` 骨架、`runtime::AdvanceKernel`（编号/Stop、GP-02 四向量、单元耗尽 Fold、GP-04 多单元、最小 replay）落地；GP-03、Playback owning DTO 与 legacy 输入切换未完成。T-12 部分完成：timing 库、编译闭包、chart 时间适配、beat-us 消费者与 RR-F133 成立，canonical 行与安装隔离未做。F508 与 F135 的裁定见登记册 §42，F508 落地与 PR 47 编译修复见 §43；PR 47 的 GCC/MSVC/Clang 编译红灯与隔离门禁 Chart 源表漂移已修复，T-10 第九轮、T-12、F508 与修复提交均在本地，未推送。F135 采用形状 2（`Error` 112 B），task-34 实施中；治理树引导 PR 48 已开，合并后 PR 47 的 PR 事件才有含治理树的可信历史基线。状态见 [HANDOFF](HANDOFF.md)。

## 阶段状态

| 阶段 | 当前状态 | 入口 |
| --- | --- | --- |
| Stage 7-RR | 当前阶段；T-01、T-03、T-04 与波次 5 完成；T-17 主体完成（F508 第二轮；shared 与设备未做），T-11 主体完成，T-06/T-08/T-10/T-12 部分完成，F135 实施中 | [总计划](stage_plans/active/stage-07-rr/plan.md)、[文档治理分册](stage_plans/active/stage-07-rr/documentation-governance.md) |
| Stage 7.5 | 尚未实施；以 RR 完成并获接受为入场前置 | [计划](stage_plans/active/stage-07-5/plan.md) |
| Stage 8–14 | 尚未启动；Stage 8 以 Chart v5＋Gameplay v3 为正式主线 | [路线图](ROADMAP.md)、[计划索引](stage_plans/README.md) |

Stage 7 已按原范围受限退出，旧失败、未认证范围和 testOnly 状态继续有效；原高级能力由 Stage 7.5 承接。历史阶段和批次通过[计划索引](stage_plans/README.md)与[Stage 7 报告索引](stage_reports/stages/stage-07/README.md)追溯。

## 当前遗留问题与生产关闭前置

以下限制仍有效，整理文档不能视为已修复或扩大保证范围。

| 遗留或限制 | 当前处置与边界 |
| --- | --- |
| 输入累积降帧、状态复制和 Replay 保活 | 性能影响尚未全部解释；由 Stage 7.5 的 GE-R 在能力工作线后集中整改，期间仅处理阻断功能实施的必要修复 |
| CPU/设备计时及 Seek 重复性 | 原 inconclusive 与停止补跑结论保留；既有采样额度和设备/P8 台账不因阶段退出或文档整理重置 |
| 完整内存、GPU 与 P8 认证缺口 | ownership/lifetime、释放边界、residency 和 GPU observer 等证据仍不完整；正式 qualification 保持 blocked，原 caps 与历史例外保持 |
| 原 P7 自动门失败和活动证据缺失 | 原失败及未认证范围继续延期；新课程和主观听音结果不替代原门，音乐 retention 适用性独立核实 |
| F3 校准与生产参数 | 保持 testOnly；扩大保证范围前完成相应 profile、设备、规模、余量及超额行为的校准和接受；精确 AV 等缺证项目独立补证 |
| 其他历史限制 | PD-0 慢态、R0 partial、旧 Q4 inconclusive、B5 capture_gap/重复 Pause、CXC 现场持有者未知及旧规模限制继续有效 |

逐项责任、原始指标、证据额度和恢复条件见[受限退出与缺陷交接](stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-14-limited-exit-and-defect-handoff.md)，后续承接见 [Stage 7.5 计划](stage_plans/active/stage-07-5/plan.md)。既有报告中的提交、构建身份和通过结果只适用于其原受验对象，不能外推到当前工作树。

## 当前格式与 SDK 合同

- 当前 SDK API 与日期构建身份见[版本权威](../cmake/CuexisVersion.cmake)，SDK 仍处于 preview。当前消费者的兼容和重建要求见[版本政策](guides/VERSIONING.md)，稳定 C ABI 归 [Stage 14](stage_plans/future/stage-14/plan.md)。
- Gameplay v2 是当前实施基线，顶层合同仍为 candidate；局部实现不等于整批验收。见 [V2 Spec](formats/GAMEPLAY_V2_SPEC.md)、[ABI](api/GAMEPLAY_V2_ABI.md)及 [RPA 合同](formats/rpa-execution-contract.md)。
- Chart v1–v4 的既有兼容路径保留；Chart v5、CXT v2、Packed Chart 与 Canonical Gameplay Graph 仍为显式候选路径。RR 的 Gameplay v3 实验实现与 Stage 8 的正式发行分别验收；旧内容按已承诺规则、时间及身份兼容。

格式能力状态与权威入口见[格式索引](formats/README.md)，Playback SDK、Player 与 Studio 的职责见[产品边界 ADR](adr/0027-playback-sdk-product-boundary.md)及 [API 索引](api/README.md)。

## 延期和禁止边界

运行时脚本和逐帧 script callback 无限期延后，不预留字段、capability、bytecode、ABI 或 Playback hook。离线 authoring generator 不会被 pack、prepare 或 Playback 隐式执行。

## 更新规则

本文只保留当前阶段、任务进展和仍有效的限制。完成或被替代的过程描述从摘要中移除；历史测试、提交、授权与处置经过保存在原报告和计划，不向本页追加过程日志。
