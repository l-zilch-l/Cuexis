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
| 七个探针单元 | owner 豁免额度后全部取得三窗口结果：六个计时组完成（全九 ≤11.1%），`p4-capture-1` 全九 16.7% → inconclusive | 见[探针批报告](2026-11-08-probe-matrix-admission-and-environment-stop.md) |
| 内存逐域归因 | 精确分区下 n64 与 n2048 的 `state.current` 超出 proposed 上限（2.17×、3.11×），n2048 的 `command.session` 超出 1.24× | 见[内存归因报告](2026-11-08-memory-attribution.md) |
| P8/40K | 未取得数据：fixture 在 40K 下触发 `packed.budget.strings`（上限 100,000） | 需内容设计或 caps 裁定 |
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
| R4-5 P1–P4/P6 | 七单元三窗口完成；六个计时组通过重复性，`p4-capture-1` inconclusive | 未接受探针绝对预算；`p4-capture-1` 待裁定 |
| R4-5 P5 控制 | 未登记 | 需要 controls adapter 与固定请求数 |
| R4-5 五类 Seek | 本候选未重验 | 需要 device/seek adapter 与真实端点 |
| R4-5 memory | 逐域精确归因完成；40K 实体实测只有 `command.session` 超（7.36×），`state.world`/`state.current` 在声明内 | 上限修订待逐分配归因；GPU/驱动与 prepare 峰值未测 |
| R4-5 P8 | 已准入：40K 实体 + 4,096 内容行跑通（exit 0），唯一超出是 `command.session` | 内容口径与上限修订待签署；low 变体未做 |
| R4-6 F1 | **候选 `2c4e1274` 的 11 行全部 local_pass** | 无；报告见 2026-11-10 批 |
| R4-6 F2 | **`2c4e1274` 同 SHA hosted 三平台 success** | 无 |
| R4-7 O2/P7 | 实体清单已准备 | owner 实体参与、仪器与听音 |
| R4-7 F3/生产 | testOnly | 校准与 owner 接受 |
| R4-7 O3/PD 退出 | 未接受 | owner 接受带限制移交 |
| Version Gate | **success（`2c4e1274`）** | 无；批准记录随候选 SHA 变化 |

## 3. 需要 owner 裁定的事项与推荐方案

### 3.1 `p4-capture-1` 的重复性（全九 16.7% > 15%）

| 方向 | 利 | 弊 |
| --- | --- | --- |
| A. 接受 inconclusive（**推荐**） | 遵守冻结判据；绝对值 2.3–2.7 µs 已稳定 | P4 的单消费者单元缺一个重复性门 |
| B. 豁免后再登记一次，跑前固定“只跑一次”规则 | 可能得到干净的三窗 | 有挑选样本的嫌疑；若再失败仍要回头选 A |
| C. 把该单元改为结构计数判定 | 不受 0.1 µs 量化限制 | 协议变更，需重新接受判据 |

推荐 A：它的 16.7% 来自 2.4 µs 量级上 0.1 µs 的量化步长（0.4 µs 跨度即 16.7%），不是产品
回退；把它记成“分辨率受限的 inconclusive”比再跑一次更诚实。

### 3.2 探针指标是否补绝对预算

| 方向 | 利 | 弊 |
| --- | --- | --- |
| A. 保持无绝对预算，只报结构计数与重复性（**推荐**） | 不发明阈值 | 探针单元无法单独证明“够快” |
| B. 为 P1/P2/P4/P6 各补一个已接受上限 | 可形成完整裁定 | 需要新数值接受；当前没有独立依据 |
| C. 用相对 A/B 判定 | 有回归含义 | 探针是 B-only，A 无对应 harness |

### 3.3 内存：`state.current` 与 `command.session` 的超出

观测：n64 `state.current` 427,015 B（上限 196,608，2.17×）；n2048 13,249,941 B（4,259,840，
3.11×）；n2048 `command.session` 27,049,016 B（21,854,208，1.24×）。

| 方向 | 利 | 弊 |
| --- | --- | --- |
| A. 修正提案系数（`state.current` 约 6.5–8 KiB/实体；`command.session` 增加随 N 的项）后重新接受（**推荐**） | 与实测形状一致；不阻塞其他门 | 需要新的数值接受，且会放宽 M1 语义 |
| B. 先做存储整改再按原提案判定 | 不放松预算 | 需要产品工作；本候选无此预算 |
| C. 保持 unrated，把超出写进 O3 缺口 | 不动数值 | 内存门长期无结论 |

推荐 A，并以 n2048 观测作为系数依据、n64 作为小尺度固定项依据。

### 3.4 P8/40K

观测：40K 进程在内容构造阶段以 `packed.budget.strings` 失败（`maxPackedStrings=100000`）。

| 方向 | 利 | 弊 |
| --- | --- | --- |
| A. 让 40K fixture 复用字符串池（**推荐**） | 不改产品上限；仍覆盖 40K 实体 | 改变了 40K 内容设计，需要登记为新 fixture |
| B. 经 caps 接受提高 `maxPackedStrings` | 不改 fixture | 改动的是产品上限，影响所有内容 |
| C. 不做 P8，接受“40K 不可认证” | 省事 | 新 P8 门空缺 |

### 3.5 已按 owner 指示记录的处置

owner 于 2026-11-08 明确“允许推送，可以按照推荐方案继续进行剩余工作”。据此记录两条处置，
其余仍按 §3.1–3.5 的选择项等待签署：

| 事项 | 处置 | 依据 |
| --- | --- | --- |
| `p4-capture-1` 重复性 | **接受 inconclusive**：三窗口完成、绝对值 2.3–2.7 µs 稳定，16.7% 来自 0.1 µs 量化步长，记为“分辨率受限”，不再重跑、不改判据 | 推荐方案 3.1 A |
| 探针指标绝对预算 | **维持无绝对预算**：七个探针单元只报结构计数与重复性，不为探针发明阈值 | 推荐方案 3.2 A |
| 内存上限修订 | 转为[修订提案 v2](2026-11-08-memory-budget-revision.md)：给出两条解析路径（128 槽页结构 / 固定+线性）与待签署项，**不把观测反填为上限**，接受仍待签署 | 推荐方案 3.3 A 的准备步骤 |
| P8/40K | 维持“内容设计优先”方向；本批未实现 fixture 变体，原因是 `maxPackedStrings` 属冻结 Foundation 门，内容改造需先确认 40K 实体与需求计数的 1:1 语义 | 推荐方案 3.4 A 的前提 |

推送：`codex/r4-integration` 已推到 origin（`2ec3480e..7e1199e1`），此后新增提交继续推送；
仍未合并、未发行。

### 3.6 其余

最终候选 C 的选定（决定 F1/F2 绑哪个 SHA）、F2 推送授权、O2/P7 实体参与、F3 校准、
O3 移交接受、Version Gate 的 SDK owner 批准——这些都需要 owner 参与，代理不自行推进。


### 3.7 2026-11-10 收口记录

- 候选冻结点是 `2c4e1274`（`26.11.09-1`）。F1、F2 与 Version Gate 都在这个 SHA 上通过，
  证据见 [F1/F2 报告](2026-11-10-f1-f2-final-candidate.md)与
  [P8 40K 报告](2026-11-10-p8-40k-entity-memory.md)。
- P8 的 40K 实体跑通后，唯一的超出是 `command.session`（160.8 MB vs 21.85 MB）。结构归因指向
  `frameByteBound` 的 4,096 B/成员交付缓冲，但缺逐分配证据，因此没有写新的数值上限。
- 本页 §2 的其余行（P5 控制、五类 Seek、device/P3/P7、`read_long`）不变，仍待后续工作。

## 4. 边界

- 本文的“通过”只指该单元、该指标、该协议内的裁定，不外推到 RPA、Stage 7A、Stage 7、
  设备或生产接受。
- 两次停止的环境原因与现场字节保持不改；没有样本替换、没有事后改类。
- 本轮所有提交都在本地，未推送、未合并、未发行。
