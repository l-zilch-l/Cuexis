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

## 预算 v2：新增故障、退款和实际堆证据

隔离观察器与 budget-v2 使用同一 SharedSequence/类定义；仅 event kernel 与 B3 两个 TU
重新编译，其他对象/库来自同一个已冻结的 fresh 构建。全局 fail-next 在 lease acquire 后、
point make_shared 前注入。新增故障/边界与已有相关回归共 **11 cases / 935 assertions** 通过。
包括 staged 后 finalization/Fold/pre-seal 故障不提升、restore copy/reserve 失败回退、
计费加法/乘法溢出、紧额度及 retired/fork owner。
六个实际暂存 frontier 为 901/1101/1301/1501/1701/1901，每个新增 768 facts，ordinal=0；
后续选中的 earliest/latest 点与预期一致。见[故障日志](2026-10-30-resume-evidence/budget-v2-fault.log.txt)。

故意让失败 advance 也提升暂存点的 mutant，exit 42，102 assertions 中 6 项失败，
明确检出 point/charge 残留；这是测试有效性反例，见[negative 日志](2026-10-30-resume-evidence/budget-v2-negative.log.txt)。

实际 requested / CRT usable / private bytes / working set 独立观察显示：

| 场景 | 可选点 | writer 销毁后的 requested 增量（相对无点） | 计费 | 最后全部 owner 释放 |
| --- | ---: | ---: | ---: | --- |
| 1K dense | 1 | 11,303 B | 6,115,528 B | requested/storage 差值均 0 |
| 1536 六代 archive | 当前最多 4，旧代保活 | 99,103 B | 37,172,064 B | requested/storage 差值均 0 |
| 40K dense | 0 | 0 B | 准入失败，charge=0 | requested/storage 差值均 0 |

40K 完整保守估计为 244,028,048 B，其中 Facts 198,803,392 B、phases 43,275,024 B；
超过历史试验参数 134,217,728 B。**这只证明该计费/额度组合无法提供 40K 续点加速，
不证明实际新增 244 MB 或续点机制不可行。** 40K requested 峰值差值为 0，因为没有收点；
观察器绝对进程 private peak/sample 含夹具与 allocator 滞留，不能归作产品单域峰值。
见[实际堆日志](2026-10-30-resume-evidence/budget-v2-memory.log.txt)。

第一版堆检查错误地断言 40K 必须收点，失败后 Catch2 在 owner 范围内保存失败信息，
导致 requested/storage 表观残留 248/280 B；原失败日志完整保留。改为独立检查实际拒绝行为后，
所有最终差值归零。因此原残留不能判为产品泄漏，也不能删掉原失败记录。

## 预算依据与后续方向

用户要求审视 128 MiB 是否合理。本轮核对它只是原型代码中的试验参数，尚无设备 headroom、
完整播放链内存分配或 40K profile 推导；不能把它变成候选去留或生产接受的硬门。
下一步先做固定成本 census：1K/40K dense、40K 六代 owning archive，区分无缓存、旧额度拒绝、
禁用字节准入但保留四槽上限时的实际成本。此诊断不把允许收点的设置推荐为产品 cap。

另有尚未构建/选用的 append-only Fact 页候选：只对必需源日志持续持有的完整 Fact 页免重复收费，
目录与 partial tail 仍计费；promotion 检查精确共享页身份。需要证明整个暂存、fault、分支和归档
生命周期均有 mandatory owner，不能因某个引用计数或值相等就扣除。只有数据与证明支持才继续，
不为守住任意 128 MiB 引入难以维护的复杂机制。原正式/内存/profile 与最终矩阵缺口保持开放。
