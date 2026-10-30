# R3 恢复实施与预算候选验证

状态：实施中；R3/R3c 未退出，候选选择须逐项记录。

更新日期：2026-10-30（Asia/Shanghai）。

## 授权与现场

用户在本次对话明确要求阅读接手文档并继续推进重构优化，取代此前普通实施暂停。
正式 R4、PD B2/B3、profile/caps、设备/生产和发布接受仍分别保留原边界。
入口 HEAD 为 `b844da0c4452ea37af95118db2d2af8831856df5`，工作树干净；
桌面核验的 2267 个 tracked 文件及 340 项 evidence 全部哈希相符。
见[入口回执](2026-10-30-resume-evidence/entry-verification.json)。

## P1 v2 固定对照：不选用

沿用已冻结二进制，没有重新构建。首次工具返回被对话中断，但后台控制器自行完成，
10 个普通进程、10 份结果及 comparison 全部存在，exit 均为 0，未补样或重跑。
Packed、Chart/Content/Prepared 的哈希/长度相同，12 对 raw 文件逐字节一致。
原始日志/raw 位于 `out/verification-20261029/r3-completion/packed-remaining-measure-v2/`。

| 场景 | baseline codec 中位数 | candidate codec 中位数 | 变化 | source prepare 变化 |
| --- | ---: | ---: | ---: | ---: |
| 1K shared | 79.567 ms | 89.9975 ms | +13.1091% | -1.3043% |
| 10K split | 2526.2155 ms | 2283.362 ms | -9.6133% | +0.7925% |

10K 改善未达预设 10%，1K codec 超过 5% 保护线；判定为 **不选用**。
1K candidate source/codec 的相对范围分别 23.2571%/45.4802%，未唯一归因。
该波动不构成删样或追加到通过的理由；v1 的通过也不覆盖本次 v2 失败。
产品 `engine/gameplay_packed/src/gameplay_capsule.cpp` 保持原实现。

可复核附件：[登记](2026-10-30-resume-evidence/p1-v2-registration.json)、
[结果](2026-10-30-resume-evidence/p1-v2-results.json)、
[字节对照](2026-10-30-resume-evidence/p1-v2-comparison.json)、
[判定](2026-10-30-resume-evidence/p1-v2-decision.json)。
控制器和分析器以 `.txt` 原字节归档；[manifest](2026-10-30-resume-evidence/manifest.json) 绑定附件。

## 预算候选验证工作

继续核查隔离的 `tick-prototype-v1/budget-source-v2`，成功冻结的源码/构建保持只读。
从独立观察副本验证 lease acquire 后真实分配失败退款、staged 故障不提升、恢复失败回退、
Counter overflow/remaining 边界、六个暂存 frontier/facts/ordinal 和多代 owner 释放。
随后使用相同 SharedSequence 定义的普通探针测 first/repeat Seek 和 writer 开销。
128 MiB 仍只是未接受的 payload charge 参数，不是进程内存 cap。
本报告后续仅记录本次新增证据，不沿用旧无预算版成绩作新版本接受。
