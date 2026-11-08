# R4 门账更新（2026-11-08 批次后）

状态：status ledger；不是退出接受记录，也不替代 CURRENT_STATUS

日期：2026-11-08

返回 [New RPA 报告索引](../README.md)。本文只汇总 2026-11-08 批次后的 R4 门状态与
待裁事项，判据与协议仍以 [实施方案](../../../../../stage_plans/active/stage-07/05-rpa-new/r4/plan.md)
和 [固定运行登记](../../../../../stage_plans/active/stage-07/05-rpa-new/r4/formal-registration.md)
为准；上一版门账见 [F4/PD 移交门账](2026-11-07-f4-gate-and-handoff.md)。

## 1. 本轮新增的裁定与缺口

| 单元 | 本轮结果 | 证据与边界 |
| --- | --- | --- |
| `p0-host-common` | **已接受绝对预算通过**：三窗口九进程 request→ready p99 = 87.9–99.1 µs（预算 4,166.7 µs），9/9 pass；request→ready 与 request→present-return 重复性通过（全九 7.0%/9.4%）；ready→present-return（约 4 µs）inconclusive | 只覆盖无 GPU/audio 的普通 host 路径；不代替 P1–P7、Seek、设备、内存、F1/F2。见[普通 host 报告](2026-11-08-common-host-unit.md) |
| `p1-no-consumer` | 两次登记均因环境停止（锁屏、人工切窗），无裁定；额度用尽 | 已声明计数 sdk_update 14,400；见[探针批报告](2026-11-08-probe-matrix-admission-and-environment-stop.md) |
| `p2-low-delivery` | 同上（attempt-1/2 各完成部分窗口） | 8:1 更新与每帧交付计数已声明 |
| `p4-capture-1` | 同上 | 单消费者材质化计数已声明 |
| `p6-sparse` | 同上 | 200 实体稀疏内容已声明 |
| `p4-capture-2`、`p4-capture-4`、`p6-topology` | 未 claim、未运行；全规模身份预检逐字段一致 | 各 2 次额度仍可用 |
| 工具 | 新增展示矩阵探针适配、普通 host 准备、补救验证与 disposition 工具；59 项工具测试通过；停止记录不再丢弃逐进程绝对门 | 仍缺 renderer/capture(device)/controls/Seek/device adapter |

## 2. 全门状态

| 计划单元 | 状态 | 缺少什么 / 恢复条件 |
| --- | --- | --- |
| R4-0 起点 | 完成 | 最终候选仍待选定 |
| R4-1/2 B3/B4/B5 | 既有机制与反例保留；本轮未改产品代码 | 最终候选完整矩阵另列 |
| R4-3 O1 | 已接受数值未改 | 无 |
| R4-4 工具 | 探针与 host 适配完成；mem/device/seek adapter 缺失 | 内存全图、observer 扰动、设备 adapter |
| R4-5 P0 legacy/RPA | 两 read_long 超界、legacy 两项 inconclusive（2026-11-07） | 读取整改与重开额度保留 |
| R4-5 P0 host | **绝对预算通过（本轮）** | 无；该单元额度用尽 |
| R4-5 P1–P4/P6 | 四单元额度用尽但无裁定；三单元未运行 | owner 决定方向（见 §3） |
| R4-5 P5 控制 | 未登记 | 需要 controls adapter 与固定请求数 |
| R4-5 五类 Seek | 本候选未重验 | 需要 device/seek adapter 与真实端点 |
| R4-5 memory | 119,534 B 下界、unrated | 完整拥有图、prepare/decoded 峰值、native/GPU 归因；两诊断额度用尽 |
| R4-5 P8 | 未准入 | 完整内存与 40K 工具准入；low/high 各一次额度保留 |
| R4-6 F1 | 仅候选 `24f0a914` 有 11 行矩阵 | 最终候选需同 SHA 重跑 |
| R4-6 F2 | 仅 `2ec3480e` 有同 SHA hosted | 需恢复推送授权后处理 |
| R4-7 O2/P7 | 实体清单已准备 | owner 实体参与、仪器与听音 |
| R4-7 F3/生产 | testOnly | 校准与 owner 接受 |
| R4-7 O3/PD 退出 | 未接受 | owner 接受带限制移交 |
| Version Gate | 仍失败（SDK owner 批准无效） | owner 提供针对所选候选的精确批准 |

## 3. 需要 owner 决定的两件事

1. **探针单元如何继续**：四个单元额度用尽、三个未运行。
   [探针批报告 §9](2026-11-08-probe-matrix-admission-and-environment-stop.md#9-当前阻断与待-owner-裁定的方向)
   给出六个方向；代理建议 B+C（豁免一次 + 只改 CPU-only 单元的前台判据）。
   本轮 host attempt-2 证明“桌面安静 30 分钟”时该策略可以完整跑完，但需要 owner
   真正空出时间。
2. **内存与 P8**：两轮诊断额度用尽、完整图未闭合；需要 owner 决定是否批准
   一个覆盖目标、进程数、时限与停止条件明确的有限批次，以及是否调整参考预算。

## 4. 边界

- 本文的“通过”只指该单元、该指标、该协议内的裁定，不外推到 RPA、Stage 7A、Stage 7、
  设备或生产接受。
- 两次停止的环境原因与现场字节保持不改；没有样本替换、没有事后改类。
- 本轮所有提交都在本地，未推送、未合并、未发行。
