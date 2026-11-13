# R4 收尾验证与待裁定移交（2026-11-14）

状态：verification preparation / unresolved gates；不是退出接受记录。

返回 [New RPA 报告索引](../README.md)。本轮授权见[收尾裁定](2026-11-13-closeout-decisions.md)，
当前状态以 [CURRENT_STATUS](../../../../../CURRENT_STATUS.md)和[R4 门账](2026-11-08-r4-gate-status.md)为准。

## 1. 测量候选与最终验证候选

本轮测量候选为 `ce2a9a6952fd4a9a4d82949544f7ef3482e204e6`，tree 为
`f2ec992cd94aa32c838d5ee863bdbca0b37033d2`，构建为 `26.11.13-1`，SDK API 为 `0.9.1`。
P5、设备组、观察器、容量与 Player 的结论各自绑定该身份；历史 P8 保持原身份。
小记录的原字节及来源哈希见[归档](2026-11-14-closeout-evidence/index.md)。

后继最终验证候选还包含本报告、状态／索引／勘误，以及环境补救采集器修复和九项回归测试。
该工具修复不修改 C++ 产品代码、fixture、profile、cap 或阈值，但仍是明确的工具 delta；
不以“只有文档变化”继承正式采样身份，也不套用 R3 的限定 delta 裁定。
最终候选必须实际执行十一行 F1 与同 SHA 的三平台 F2；当前报告不预签结果。

按 [R4 计划 §3.1／§3.7](../../../../../stage_plans/active/stage-07/05-rpa-new/r4/plan.md)，
最终候选的 SHA、tree、base、UTC 日期、命令结果、原始日志哈希与 hosted run ID 将写入
`D:/Cuexis-worktree/out/evidence/r4/closeout-20261113/final-verification/` 的外部绑定清单，
并在必要的单个集成 PR 中发布、回读已授权评论；不为回填候选自身 SHA 和结果再次改变冻结提交。
十一行矩阵使用实际短工作树 `D:/w1` 与全新隔离构建目录，不过滤失败用例或继承旧二进制。
实际 PR/base/candidate/tree/UTC 日期仍须通过适用 Version Gate；SDK API 与实际 base 相同才按 checker
判定批准不适用，不制造无须的 SDK 批准。不开单独报告 PR；唯一集成 PR 的必要性是取得实际 pre-merge 门。
本报告不授权 merge、release 或阶段退出。

## 2. 环境补救采集器修复

原 `environment_stability.verify()` 在 `finally` 中只检查已有样本，采集不足要求时长而被中断后，
仍可能写出 passed；首份 snapshot 失败又可能被 `summary([])` 遮盖。修复后，只有正常完成循环、
达到要求时长、首尾及相邻样本满足既有两秒覆盖规则、hold 和 nudge 成功时才记录通过；
异常写失败记录并在清理后继续传播。时长截点早于 hold 释放，清理开销不能补足采集时长。

九项新用例使用虚拟时钟，覆盖 60／900 秒正常完成、725 秒中断、首个及后续 snapshot 异常、
首尾缺口、hold 和 nudge 失败。原实现为六项 failure、三项 error；修复后本文件 17 项与完整工具包
130 项均通过。日志为 `out/evidence/r4/closeout-20261113/stability-red.log`、`stability-green.log`、
`execution-acceptance-final-green-package.log`。首个包发现命令漏了 `-t .`，其七个相对导入错误保留在
`execution-acceptance-final-green.log`；正确包发现重新运行成功，不把调用错误写成产品失败。

P5 第四次 claim 前另行核对过实际 900.884 秒、895 个样本、899.878 秒跨度、三次成功 nudge、
完整预检文件哈希与唯一前台 PID；本次修复不替换该记录，也不追改旧失败。采样期间没有执行构建、
CTest 或源码修改；全部序列结束并释放 hold 后才应用修复。

## 3. 当前具体结果与裁定建议

以下是建议，尚未成为已接受限制；保持原 caps、次数、失败及判据。最终 F1/F2 仍应先完成，
其技术验证不由人工代签。需要改变受控输入或额外次数时先明确对象、理由与恢复门，不能无限重跑。

| 事项 | 实际结果／缺口 | 推荐处置与未裁定影响 |
| --- | --- | --- |
| P5 | 第四次完整执行；63/63 绝对门通过，五组重复性通过，close／load_host inconclusive；第五次未用 | 保留准确限制供 O3 接受；不为耗尽额度启动第五次。若要求完整重复性认证，须具名有界诊断及有效 remedy 后再按原规则登记 |
| P3／capture-4 | P3 三类绝对门及三项重复性通过，三项外层重复性 inconclusive；capture-1/2 通过、capture-4 inconclusive | 保留首窗慢值和全部三窗；推荐有限范围移交，不外推 GPU 时间。完整设备接受仍不能关闭 |
| 四阶段／GPU | 四阶段在既有可开关例外下的数值门通过；外层扰动按原值披露；GPU observer_unvalidated | 推荐保留 GPU 未认证范围，不新设 observer 阈值。若要求 GPU 绝对门关闭，须先接受具名观察器准入政策并取得相应证据 |
| 内存／P8 | 完整语义 owning/lifetime validator 未实现；原点超额、两处环境停止、n2048 residency 缺失；本轮无 P8 invocation | 推荐保持 testOnly 和未认证范围移交，原 caps 不变；不把原点分区或 sampled GPU delta 当完整预算。完整接受须补语义图、超额处置与对应规模证据 |
| 历史 P8 追溯 | 限定目录识别 low 1／high 4；旧 high-3 分区为修正前数字；最早 controller 路径字节被覆写 | O3 逐次处置额外 high 调用及追溯缺口；修正数字引用独立 f5f91916 low/high-control，保留既有 owner 归属裁定。勘误不恢复额度或追认调用 |
| P7 自动 | 音乐只有约 129 秒活动功能轨迹；DFJK 40 分／19 命中／2 misses，未达原 oracle；normal 三窗缺失 | 推荐先保留本轮未认证范围；原失败不能缩写成只剩听音。完整认证还需具名 DFJK 修复／定位与 normal 证据；不修改期望分数或选择旧 scratch 成功 |
| P7 retention | 原 DFJK 35 秒，缺适用 ExecutionHost 的至少 600 秒活动证据；音乐不经过 ExecutionHost，适用性独立 | O3 分别处置适用性／范围。原课程不能延长、循环或终态等待凑活动证明；没有新课程授权或明确范围裁定时，完整 retention 接受继续开放 |
| Seek | fallback／cold 绝对门通过，重复性 inconclusive；已决定本轮不重跑 | 最终 O3 明确接受这两项限制及责任／恢复条件；不追改为 pass |

以上推荐是对未证明范围的准确移交，不自动禁用功能、修改计划、宣布 N/A 或关闭 goal。
owner 若要求原完整范围继续认证，则需对无法靠现有工具或剩余额度解除的具体准入／范围问题作裁定。
未接受前，不能宣称已达到“只剩人工操作”的目标。

## 4. 集中安排的人工事项

P7 听音使用绑定的实际端点与原音乐，记录真实听感和异常时刻；自动分数、normal 与 retention
缺口继续独立保留。O2 按[实体清单](2026-11-07-o2-physical-checklist.md)完成真实键盘／Hold、
控制体验、物理拔插恢复及适用仪器观察，不以脚本注入或 Replay 一致代签。

F3 对具体生产 profile、设备／规模／retention、参数、余量与超额行为校准并接受；
O1 reference caps 的接受不等于生产接受，未接受时保持 testOnly。
O3／PD-5 等最终门账、F1/F2 和适用版本门真实结果齐备后，逐项记录接受范围、限制、责任与恢复门。
PR 评论授权不代签人工观察、预算修改或退出接受；R4、RPA、Stage 7A、PD-5、Stage 7 均尚未关闭。
