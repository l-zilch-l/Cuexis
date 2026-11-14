# R4／RPA 带限制退出与缺陷交接（2026-11-14）

状态：completed；owner 已接受受限退出与交接，生产接受延期。

更新日期：2026-11-14；快照日期：2026-11-14（Asia/Shanghai）。

返回 [New RPA 报告索引](../README.md)。当前摘要归
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md)，后续能力范围归
[Stage 7B+ 计划](../../../../../stage_plans/active/stage-07/06-b/plan.md)。
本记录集中保存关闭裁定及遗留责任，不另建本轮整改计划。

## 1. owner 裁定与关闭范围

owner 先要求“先审查当前R4是否具备关闭状态，我先对未作出的决策进行裁决”。
代理完成审查，提交九项推荐：退出范围、新增性能缺陷、计时未定、内存/GPU、P8 历史、
P7 自动缺口、听音/实体操作、F3 生产、关闭文档及证据延续。
owner 随后原话确认：

> P7 听音与 O2 实体操作  没有出现异常。我可以接受当前推荐裁决方向

据此登记以下接受范围，而不再只登记为“准备移交”：

- R4 与 Stage 7-RPA 带已知缺陷及未认证范围退出，O3 本轮退出裁定完成。
- 接受 PD-5／Stage 7-PD 的具体受限退出及遗留责任交接；PD-5 历史“未通过但定案”保持。
- 原 Stage 7A 收尾包随 RPA 作受限交接并获接受，不另回挂 7A 补开收尾批次。
- Stage 7B+ 消费此受限基线，其原“RPA 完整退出”入场前置按本次明确裁定满足；
  独立能力的合同、正确性、预算、Replay、平台与退出门继续适用。7B+ 尚未开始产品实施。
- 整体 Stage 7 未关闭；Stage 8 发行、F3 生产参数、merge／release 均未获本次接受。

owner 对本次听音和实体操作报告无异常，主观听感／操作接受据此登记。
未提供逐项操作时间、设备拔插细目及仪器原始数据，不推定所有清单项逐项执行，
不补造精确 AV offset、误差、延迟或 GPU 计时通过。此前报告的卡顿及持续降帧仍保留。

## 2. 已验证实现点与文档差异

| 项目 | 绑定或实际结果 |
| --- | --- |
| 已验证代码候选 | `d1951326b0f2683801fa5fe168f09c8dac43f957` |
| Tree／base | `55ff54c9160c52de3cdb318687d5fd6bb8ba9623`／`85ebf64a3a81d0ecfa7cfba3ebff1c56b758b7fc` |
| Build／SDK | `26.11.13-1`／`0.9.1` |
| P5／设备测量候选 | `ce2a9a6952fd4a9a4d82949544f7ef3482e204e6`，不改标为 d195 |
| F1 | 11/11 行 local_pass；13,658 项登记，13,632 项执行，26 项 skip，0 失败；120 条预算事件 |
| F2 | 同 d195：Linux `43495086434`、MSVC `43495086461`、MinGW `43495086372` 均 success |
| 适用版本门 | `43495100635`／`43495095402` pre-merge success；真实运行 UTC 日期为 2026-11-13 |
| 封存清单 | 226 文件／47,398,848 B；本次退出审查逐文件复验无差异 |
| 清单 SHA-256 | `9be14b7d8401e2cf1be11b837b2bff7a81ac91e061f5761ec7acd58379b649fe` |

封存目录是 `D:/Cuexis-worktree/out/evidence/r4/closeout-20261113/final-verification/`，
入口为 `FINAL_HANDOFF.md`、`final-manifest.json`、`final-seal.json` 与
`post-publication-completion-audit.json`。其 `stage_exit=false` 是本次裁定前的真实快照，
由本记录补充后续决定，不覆盖原包。此前发布的 PR 结果评论为
`l-zilch-l/Cuexis#44` 的 `issuecomment-6869272832`。

owner 接受关闭记录及必要状态／索引／计划前置说明的限定文档差异；本轮不重跑完整矩阵。
本记录不把后继文档 SHA 声称为已执行 F1/F2 的 SHA。产品、工具、fixture、profile、caps、
版本与 SDK 均不因关闭落档修改。后续正式提交作为新候选推送、合并或发行时，仍须履行
[版本政策](../../../../../guides/VERSIONING.md)的实际日期／版本门；本次文档差异接受不豁免该门。

## 3. 遗留项处置与恢复条件

| 项目 | 本轮明确处置 | 后续责任与恢复条件 |
| --- | --- | --- |
| 新增降帧与复制／保活缺陷 | 接受已知缺陷，本轮停止扩项；整改延至 7B+ 后集中处理 | 代理承接；若期间阻断功能实施，仅做必要最小修复。恢复性能接受须在新候选重验输入增长、长活动、Replay／旧结果保持与事务失败语义 |
| 普通 host／P5／P3／capture／Seek | 普通 host ready→present-return、P5 Close/load_host、P3 三项外层、capture-4、fallback/cold 重复性 inconclusive 原样接受为限制 | 本轮不补跑。P5 已用 4/5，第五次未用；普通 host 额度用尽，device 首次 claim 已消耗。后续须具名 remedy、核实剩余额度并重新登记，不能重置或替换旧失败 |
| 四阶段与 GPU | 四阶段在既有例外内的数值门通过；外层 observer 扰动继续披露；GPU observer_unvalidated | 代理准备具名准入政策及证据；CPU submit／present-return 不代称 GPU 时间，不新增事后阈值 |
| 完整内存／residency | 完整 ownership/lifetime 图未实现、原点超额、prepare-only／完整释放边界、n2048 residency 缺失作为未认证范围接受；原 caps 保持 | 代理承接完整语义图、超额处置及对应规模／observer 证据；正式 memory/P8 qualification 继续 blocked，生产接受前不得写成预算通过 |
| P8 历史调用与来源 | 接受 low 1／high 4 及 controller 原字节覆写的追溯缺口，按已发布勘误作历史处置结案 | 不追认额外调用、不恢复额度。修正后归属数字引用独立 f5f91916 low/high-control；未来额外 P8 须重新明确授权，旧不可恢复字节保持缺口 |
| 原 P7 DFJK | 原 40 分／19 hit／2 miss 未达 46／21／0，按失败保留并延期认证 | 代理承接原脚本时基／派发／捕获定位；新 45 秒密集课程或旧 scratch 成功不替换原失败 |
| P7 normal／retention | normal 三窗及适用 ExecutionHost ≥600 秒活动证据缺失，按未认证范围延期 | 代理承接适用新工作流与证据；不得延长、循环旧固定课程或用终态等待凑活动时长；音乐不经 ExecutionHost，其 retention 适用性独立 |
| P7 听音／O2 实体 | owner 本次报告无异常，接受已报告的听感／操作范围 | owner 负责实体事实，代理整理证据。未报告细目和精确仪器项保持未证实；需相应完整保证时补实际端点、操作记录和仪器原始数据 |
| F3 校准／生产 | 校准与生产参数接受延期，继续 testOnly | 代理提出逐 profile 的设备、规模、retention、参数、余量及超额行为方案；owner 在相应校准和证据齐备后明确接受 |
| 历史偶发故障 | PD-0 慢态、旧 Q4/R0 限制、B5 capture_gap／重复 Pause、CXC 现场持有者未知等既有记录继续有效 | 代理保留原来源；实际再次阻断或拟扩大保证范围时具名重开，不因本轮退出推定已修复 |

本表不把 fail、inconclusive、blocked、unrated 或 not-observed 改成 pass／N/A；
接受的是本轮退出和责任交接。高频／连续／多接触点输入与长活动的成本风险由 7B+ 准入包明确消费，
其预算不能外推自本轮。生产接受或扩大规模、retention、设备保证前须完成对应证据及 owner 接受。

既有细目与原结果见[收尾验证与待裁定移交](2026-11-14-closeout-verification-and-handoff.md)、
[容量与 Player 有限证据](2026-11-14-capacity-and-player-bounded-evidence.md)、
[五类 Seek](2026-11-13-seek-five-classes.md)及[O2 清单](2026-11-07-o2-physical-checklist.md)。

## 4. 新增缺陷的已证事实与来源

三项机制在本轮独立诊断中已有证据：Gameplay 呈现事务复制完整 evaluation、
Advance／调度结果转换复制累积输入历史，以及 Replay 持有各个历史状态及其副本。
普通 Runtime 与 Gameplay 在原 d195 库、同内容／tick 的对照约为 0.75–0.89 ms 与 2.60–3.01 ms。
原库 770 输入的完整 Advance 约 0.514–0.534 ms，零输入约 0.012 ms。

实体 trace `20261114-114305-730835-dense-trace` 有 768 条输入，306 hit／81 miss／535 分，
Replay 一致、underrun 0；3–11 秒约 190.64 FPS，40–45 秒约 120.15 FPS。
该诊断不是 d195 普通 Release 的正式 normal 性能认证。

原库 Replay 对照在相同 770 输入、额外 255 个独立历史状态时，进程 private commit 增加
约 118.64 MiB；共同后缀 Advance p50 为 198.1／200.8 µs，差值不足证明现场剩余降速由驻留内存造成。
大组触预设内存守卫停止，未提高上限或重试。PSAPI 指标不是完整语义内存费用。
原明显卡顿、renderer submit 差额及更长时间的调度／heap 影响仍未全部解释；尚未应用产品修正。

以下路径为独立原始来源，不属于旧封存包；使用代码路径避免本机文件引用破坏跨平台文档检查：

| 来源 | 路径与 SHA-256 |
| --- | --- |
| 深入归因汇总 | `C:/Users/Zilch/Desktop/Cuexis-R4-P7-O2-2026-11-14/analysis/DEEP_SOL_REVIEW.md`；`7fd50315aa2e95ebf98fbd83a3ee64ad7ffbc9726aed1a22d983d58e2058020e` |
| 实体输入逐帧复核 | `C:/Users/Zilch/Desktop/Cuexis-R4-P7-O2-2026-11-14/analysis/PHYSICAL_INPUT_REVIEW.md`；`5ec71ecfd40491dcf4cbee1be28a776374ee8044d2c61ea224ea526b57e8cd9b` |
| 原库 Runtime 对照绑定 | `out/evidence/r4/p7-o2-20261114/deep/runtime-control/runtime-control-binding.json`；`2b0fc1d3aa1b5cbe29c95a4ff3657f58517a6d57d7961be136998665b2f151b0` |
| 原库 Replay 保活对照绑定 | `out/evidence/r4/p7-o2-20261114/deep/retention-control/retention-control-binding.json`；`42e9ec882b1693df49efe4d4880ebb5ef33767dbe6fbfc31055c2e6fade79ea8` |

## 5. 落档边界

本轮仅增加本关闭／缺陷交接记录并修正必要入口，不新增 PR、正式测量或产品整改批次。
既有 PR #44 可补充已授权裁定评论；受验 d195 及旧封存包保持不变。
关闭文档保存在工作树，本轮不推送新的候选；远端 PR 受验 head 保持 d195。
裁定评论另外绑定本记录字节，后续文档提交随实际新候选履行版本门。
后继文档与未来新候选分别绑定，不宣称现有通过结果覆盖任意后继树。
