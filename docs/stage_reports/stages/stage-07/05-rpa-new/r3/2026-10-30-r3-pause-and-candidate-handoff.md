# R3 暂停现场、待确认候选与实验留档

状态：historical；2026-10-30 文档维护快照，两项候选未正式选用

更新日期：2026-10-30

返回 [RPA 报告索引](../README.md)。阶段当前状态只见
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md)，各轮实施证据见
[六项账本](2026-10-29-r3-six-item-completion-work.md)。本报告记录 owner 暂停后的文档补充，
不恢复实现、构建或性能测试，不改变原计划、SDK 0.9.1、日期版本 26.10.19-1 或 recovery wire。

## 现场和本次范围

实施工作区为 `D:/Cuexis-redesign`，分支 `Cuexis-redesign`。本次文档提交前 HEAD 为
`b1534a1f7482c59f6771685437576107ad7b0b9f`，engine/app/tests 与产品恢复点
`ff934843663d2e59e80983da1e5fc2fddf77ee17` 相同。新提交仅更新说明、导航及不可执行的实验附件，
不把任何候选补丁应用到产品树；提交后的准确 HEAD 由 Git 和更新后的桌面现场 JSON 记录。

owner 原要求恢复后连续完成 P1–P6，当前暂停请求优先。此批只核对已有文件/日志、补文档、
执行文档检查和差异检查、提交一次，再更新桌面 Markdown/JSON。没有新编译、产品测试、
性能采样、设备验证、caps 接受、PR、合并或发行。

“待确认候选 v2”表示当前待验证的迭代版本，不表示已决定正式选用。两个版本的选择状态如下：

| 对象 | 已有证据 | 仍缺 | 产品状态 |
| --- | --- | --- | --- |
| P1 claim/definitionSource v2 | 编译/链接；新旧 Packed 各 51 cases / 5,641 assertions | v2 固定性能/字节对照、最终选择及组合回归 | 未正式选用、未提升产品 |
| R3c payload budget v2 | 同版本 fresh 普通 Release STATIC 四目标功能回归共 426 cases | 真实分配失败、普通 Seek/writer、实际内存、独立闭包/版本/profile 决定 | 隔离原型、未正式选用 |

## P1：前一版有收益，v2 尚缺最终对照

统一证据根为 `out/verification-20261029/r3-completion/`，下文路径相对此根。

`packed-remaining-v1` 把 Writer claim 的资源线性查找改为 canonical ResourceRef lower_bound；
Reader reconstruct 只对会重复查询的 typed kind 建本次调用期借用指针索引。单行表继续扫描，
索引 bad_alloc 回原路径，保持原 typed 遍历、require 路径、首匹配和 sourceFor 校验。
一个只读子代理分两次审阅设计/具体 diff，主代理亲自检查差异与日志。

`packed-remaining-measure-v1` 固定 10 个普通进程：64 split canary 一对，1K shared、10K split
各两对 AB/BA。Packed 与 Chart/Content/Prepared raw 字节一致。10K 编解码中位数
2.6491365 → 2.1324665 s（-19.5033%）；1K codec -1.66%，source prepare guard 分别
-1.94% / +0.81%，未触发 5% 保护条件，过程相对范围低于 7%。这是 v1 的局部保留门，非正式接受。

审阅要求借用索引在关系复制/AssemblyRequest/prepare 前销毁，并显式 include `<new>`。
`packed-remaining-v2` 已完成这些调整及两版 Packed 回归，原目录的 binding/commands/日志保留。
本次核对 `packed-remaining-measure-v2` 仍不存在，没有把 v1 成绩移给 v2；也没有启动该组。

用户在新会话明确恢复后，先只读核对 HEAD、P1 原稿 SHA、冻结候选二进制及无并行构建，再运行
现有 `run_packed_remaining_v2.py prepare`；只有固定组全部有效、字节比较完成才运行
`analyze_packed_remaining_v2.py`。原门是 10K codec 至少改善 10%，1K shared codec 与两个规模
source prepare 各不得退化超过 5%。无效或失败保留，不替样或追加通过。通过也须先做明确选择、
仅提升已核对的单 CPP，另行更新文档并验证最终组合。

## R3c：严格区分无预算与预算 v2

无预算 `tick-host-v1` / `tick-screen-40k-v1` 的 CPU Host、40K、第一条记录内 H902，
请求到目标帧中位数 237.105 → 2.7385 ms；首次候选为 2.899/2.952 ms，完整目标 Snapshot
92,623,328 B，raw 一致。这不是 GPU 渲染时间，不是只计 fork，也不属于后续预算 v2。
setup 仍包含夹具、Packed 和 Host 准备，不能当作进程启动至首画面的 SDK load。

两候选进程中位数为 2525/2952 us，按原 `(max-min)/median` 为 **15.5924776%**。
此前账本 16.91% 使用 `(max/min-1)`；本次只更正表述，不重写登记/原样本或补采。
两者都不足以宣布原 15%/正式窗口通过；该 screen 只预设至少 50% 改善以继续探索。

无预算版的真实 fail-next、fault DTO/wire、失败不提升 mutant、单 record heap、完整分支 Replay
和 writer 保护组已记录在六项账本。它们不覆盖新增预算收费/退款路径。

最新 `tick-prototype-v1/budget-source-v2` 与 `budget-build-v2` 是隔离副本。因为 SharedSequence
定义改变，所有相关 common libraries 已用同一源码 fresh 构建，未混链接旧类定义。
`complete.json` 的源码、结果和四个二进制 hash 已只读复核：

| 套件 | cases / assertions |
| --- | ---: |
| RPA | 77 / 27,219 |
| Judgement | 236 / 56,139 |
| Host | 62 / 54,282 |
| Packed | 51 / 5,641 |

合计 426 cases，覆盖零/非零剩余额度 miss、旧 archive 最后引用退款、Seek fork 共享账本和
capacity callback 重入。现有回归显式筛选 `~[r3c-writer]`：原型 B3 还有等待 stdin 的非隐藏
writer 探针，不能不加处理地放入产品完整 CTest。assertions 随分配枚举和循环进度变化，
不把计数差异推断为语义改变。这不是完整 CTest 或 Debug/SHARED/MinGW/消费者全矩阵。

预算是 preparation lineage 的 atomic live payload + 每点 move-only lease；暂存收费、提升不重复，
最后 point 引用销毁才退款。state 页/嵌套容量按每点全额保守收费，prefix 依赖 exact mandatory
journal/archive 保活；未知 payload 拒绝准入。capacity visitor 保活入口 Directory，避免回调
clear/write 使遍历失效；额度估算按剩余额度早停。内部 probe setter 要求整个 lineage quiescent，
live==0 不是并发锁。

128 MiB 是 payload charge 实验参数，未包含 allocator/shared control block、CRT rounding、
RSS/commit、GPU 和 mandatory source/preparation，不是生产 cap 或整个进程上限。
其数值/headroom 未登记或接受。最新预算 v2 的真实故障、普通性能和实际内存都尚未测。
若增加 RuntimeState、wire traversal 或 SharedSequence owning 字段，必须重新核对闭包及容量覆盖。

下一轮须在 v2 独立观察副本验证 lease acquire 后 make_shared 失败退款、staged 后 fault 无提升、
restore 失败回退、overflow 与六个 stage1 frontier/facts/ordinal，再测多代 owner 内存和普通
first/repeat Seek/writer。普通探针必须重编到 v2 自洽头/库，旧 tick-host/scalar 对象不能直接复用。
阶段计划 §8.5 的独立闭包、语义和版本决定仍适用，不以已写代码或本归档代替接受。

## 版本化归档与原始证据

[实验材料索引](2026-10-30-candidate-archive/index.md) 保存两份未选用源码补丁、设计/脚本文本、
原始 binding/results/有限日志及逐文件清单。补丁相对 b1534a1 的产品源码生成，仅执行
`git apply --check` 只读适用性核对，没有实际应用；完整 runtime 源码路径没有变化。

归档的脚本以 `.txt` 保留原字节，其路径与目录名有历史语义，不能直接当成当前可运行控制器。
源码差异与重建线索可从 Git 找回，不等于二进制、完整失败组或历史成绩可由此重建。
完整原始资料仍在 `out/verification-*`，本次未清理、替换或重新绑定其历史文件。

本次文档提交与候选正式选择是两个独立动作；两项 selection 均为 false。架构参考继续只描述
当前产品实现，候选设计只在归档/报告中定位。提交后更新桌面 Markdown/现场 JSON 的新 HEAD、
tracked 文件与附件哈希，使其与仓库暂停状态一致。原 R3/R4、load/Seek/重复性/CI、40K reference、
内存/profile 和设备/生产/owner 缺口均保留，文档交接完成后继续停止实施。
