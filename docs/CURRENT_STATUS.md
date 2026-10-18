# Cuexis Current Status

状态：current

更新日期：2026-10-18

本文只维护当前阶段、正在进行的任务和未解决的问题。实施与测量细节见带日期的阶段报告。

## 当前任务

**Stage 7-PD / PD-0 已完成 testOnly 基线与行为表征：无插桩两窗口及合并重复性通过，同源码存储、阶段与交付观测已核验。PD-1 至 PD-5 尚未开始。**

固定环境与干净重建后的完整注册组满足原定 15% 门槛，owner 最大进程级波动 3.89%，宿主均值最大波动 11.11%。测量前修复并验证了 MSVC/Ninja 编码不一致造成的头文件依赖丢失；历史慢态根因仍未确定，旧失败组和源码绑定失败组均保留。该结论限于已登记的有限 testOnly profile，不接受生产预算或关闭 Stage 7。

方案记录：[PD-0 测量波动方案实施](stage_reports/stages/stage-07/pd/2026-10-17-pd0-measurement-variance-resolution.md)。
最新证据：[受控环境、构建绑定与 PD-0 退出](stage_reports/stages/stage-07/pd/2026-10-18-pd0-controlled-environment-and-build-binding.md)。

## 阶段状态

| 阶段 | 当前状态 | 入口 |
| --- | --- | --- |
| Stage 7A | S7A-3/4、5/6、7/8 的受限功能验收完成；整体关闭仍待 RPA 收尾 | [Stage 7A 计划](stage_plans/active/stage-07/plan-a.md) |
| Stage 7-RPA | testOnly 实施与自动化证据已具备；实体设备、生产预算和最终接受未完成 | [RPA 计划](stage_plans/active/stage-07/plan-rpa.md)、[交接状态](stage_reports/stages/stage-07/rpa/2026-10-15-rpa-f4-handoff.md) |
| Stage 7-PD | PD-0 testOnly 基线与行为表征完成；PD-1 至 PD-5 未开始，子阶段未退出 | [PD 计划](stage_plans/active/stage-07/plan-presentation-decoupling.md) |
| Stage 7B+ | 未开始，等待 RPA 完整退出 | [Stage 7B+ 计划](stage_plans/active/stage-07/plan-b.md) |

Stage 0–6 已完成；Stage 8–14 尚未开始。整体 Stage 7 未关闭。
阶段依赖与后续安排见 [Stage 7 总计划](stage_plans/active/stage-07/plan.md)和[路线图](ROADMAP.md)。

## 当前问题与关闭前置

- **历史慢态及尾延迟未归因**：最新登记组重复性通过，但不解释历史近两倍慢态；本次 Debug 宿主仍保留 15.93 ms 的单帧请求到回执尖峰。尾延迟预算未接受，不能据均值重复性通过推断生产延迟合格。
- **帧账本不回收**：`FrameObservationLedger` 在成功回执和过期处理后仍保留槽位，满容量后拒绝发布；由 PD-4 处理寿命与回收。
- **局部偶发故障未定位**：B5 Pause 的 `capture_gap` 与 CXC unpack 的 `Access Denied` 尚无根因结论。
- **RPA 验收未完成**：实体键盘、听音/同步、设备失效与压力验证、生产参数及预算接受仍待完成。
- **预算证据缺项**：端到端 Seek 延迟、prepare-only 峰值堆和 decoded semantic heap 尚未完成有效测量与接受。

问题证据与剩余门禁见 [PD-0 复测归档](stage_reports/stages/stage-07/pd/2026-10-17-pd0-supplemental-retests-and-archive.md)、
[RPA 矩阵](stage_reports/stages/stage-07/rpa/2026-10-15-f1-f2-final-matrix.md)及
[预算基线](stage_reports/stages/stage-07/rpa/2026-10-15-f3-final-baseline.md)。

## 产品边界

Cuexis 由可嵌入的 Playback SDK、独立 Cuexis Player 和 Cuexis Studio 构成。
外部宿主使用公共 Playback API；RuntimeSession、World、EnTT、SDL 和 OpenGL 属于内部或可选实现。

产品决策见 [ADR 0027](adr/0027-playback-sdk-product-boundary.md)，公共接口见 [API reference](api/README.md)。

## 当前格式与 SDK 合同

- 当前工作树 SDK API 为 **`0.8.0` preview**，日期构建为 **`26.10.15-1`**；这不代表已经发行。
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
