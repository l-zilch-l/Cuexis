# R3b certified prefix implementation and bounded cost verification

状态：in progress，2026-10-27；本候选功能/兼容核验通过，当前成本不满足 Seek 预算，R3 未退出，进入总计划 §12.2 裁定门。
返回 [New RPA 报告索引](../README.md)。范围来自[总计划 §8](../../../../../stage_plans/active/stage-07/05-rpa-new/plan.md)，
实现选择见[受控候选](../../../../../stage_plans/active/stage-07/05-rpa-new/r3/trusted-restore-candidate.md)。
[R3a 报告](2026-10-27-r3a-recovery-and-prepare.md)的冻结构建与成本不换标为本候选。

## 1. 实际实施范围

首版采用总计划 §8.2 已允许的最终 archive 工厂入口，不新增公开 API、自动缓存、周期或线程。
此前误判为必须另行确认的请求已撤回，不存在待 owner 选择该入口的前置条件。
只优化单个已认证、完整 horizon record 边界 hint；多 hint、partial checkpoint、外部来源和替换字段
继续合法原点路径。完整 checkpoint 可恢复到后续 partial 目标。未交付两级自然绑定、生产间隔或 caps。

私有证书绑定完整 source owner、canonical SHA 生成证据、准确 cut/H、不可变完整 DTO 与状态版本。
工厂先通过原完整结构检查和前缀执行，再编码原 canonical bytes/hash；不把 Reader 或结构合法当执行认证。
RPA archive 用局部 builder 移入 `make_shared<const ReplayData>` 后封存，普通外部可变 alias 不得授信。
Seek 核对不可变 owner 和私有绑定，不重新 hash；仍扫描全 source 的 shape/count/dialect，未删除该 O(history) 成本。

恢复继续正常 prepare 和当前配置 identity 门，重建完整 KernelProjection/Fold/RPA、matcher、coverage、
active requirements、timer cursor、commit ID/exhaustion、pending、新 runScope 和 fault reserve。
所有可失败分配在提交前完成；失败使用原点候选，后缀错误不修改 live session。
正常 RPA ingress 存在 projection 的 accepted 中，独立 legacy ingress 保持 prepare 默认值；
测试 signalVisibility 不闭合，含该状态或 test controls 的候选不授予认证。

现有分块日志增加只读 applied-prefix base，引用 checkpoint 的 DTO/history；后续仍追加原 32-record 尾块。
base 不直接持有 source suffix，成功新操作解除 seekSource。工厂 alias 原 SnapshotStorage::dto，省去再次复制；
这同时保留 SnapshotStorage 的 RecoveryInputs/config/prepared 和旧 history result owners，不能宣称驻留免费。
captureState/archive 的全量公开值物化边界不变。

## 2. 正确性、安全与回退

主代理检查差异并亲自执行；无上下文 max gpt-6-luna 只独立审查，没有以其结论代替测试。
审查发现的非 const 实体封存、额外 Snapshot owner 保留和两项验证缺口均落实处理。

- R3 定向 8 cases / 2279 assertions 通过：同 H 的不同 cut、partial 目标、同 Tick 控制、未来输入、
  Fold/故障/分配失败继续执行、字段替换/外部 alias/多 hint、512 输入及确定性拒绝、0/31/32/33 尾块、ID 耗尽和旧 owning 释放。
- Playback 的 RPA wrapper 新增 1 case / 35 assertions 通过：同一封存源经公开 checkpoint/Seek、事务替换、
  完整 Snapshot 对比及 unload 后旧结果仍有效。没有以 legacy wrapper 测试代替 RPA。
- 独立认证失败探针 1 case / 56 assertions 通过：在工厂中对 ≥2,086,041 B 分配注入一次失败，
  owning checkpoint 仍成功、恢复走 base=0 原点并全状态一致；取消注入后相同 source 的 base=17。
  该失败点落在可选 canonical 编码内；SHA 实现无独立堆分配，不声称额外注入 hash 内部故障。
- Faulted terminal 不属于合法 checkpoint：现有 snapshot 要求 Prepared，原点 Seek 遇 fault 返回错误。
  测试覆盖健康认证态恢复后的未来故障，没有扩展 Faulted checkpoint 合同。

原始证据位于 `D:/Cuexis-worktree/out/rpa-r3-20261026/`：`r3b-latest-targeted.log`、
`r3b-wrapper-debug.log`、`r3b-cert-failure-raw.log` 及对应源、编译日志。

## 3. 构建与兼容

独立新目录以 `out/build/rpa-r3b-final-` 开头；R3a 已冻结目录不重建。
中文 MSVC showIncludes 依赖发现异常仍按 R3a 记录处理，涉及 private header 的 Debug 核验使用 clean rebuild。
后补 wrapper 只有测试 cpp 变化，已完成配置另行增量编译并核验该 suite。
| 配置 | Judgement | RPA | Playback | ExecutionHost | 架构 |
| --- | --- | --- | --- | --- | --- |
| STATIC Debug candidate ON | 224 | 60 | 180 | 61 | 通过 |
| STATIC Release candidate ON | 224 | 60 | 180 | 61 | 通过 |
| STATIC Release observation ON | 224 | 61 | 180 | 68 | 通过 |
| SHARED Debug candidate ON | 224 | 60 | 171 pass / 1 skip | 61 | 通过 |
| SHARED Release candidate ON | 224 | 60 | 171 pass / 1 skip | 61 | 通过 |
| STATIC Debug candidate OFF | 224 | 60 | 130 | 不适用 | 通过 |

数量为 case；SHARED 的既有 private parse-probe 跳过保持明示。STATIC clean Playback/Core/Gameplay
installed consumers 3/3 通过；SHARED consumers/导出边界 7/7 通过。
两套完整 Release build 和全仓格式检查通过。
文档当前检查通过 580 Markdown/20 JSON-CXT，最终报告修改后再检查。
当前三份普通 Release 库的 `execution_diagnostics::rpa::` 符号均为 0；观测版对应计数为 14/15/1549，
此精确匹配口径不同于 R3a 的较宽搜索，不能把计数差当作删除了某些观测。
两份新构建分别在独立进程仅执行 `[rpa][r0][equivalence]`，固定 seed=12345，11 行 transcript
完全一致，digest=`b4608fbfd7a89706d5c982fa8e57132e4dbb12abde7fa019cc762bbf6c1a7bf2`。
首次尝试直接比较全套测试中的 transcript，只有 load 的进程累计 sessionScope 不同（146/122）；
原行已保留为 `r3b-transcript-full-suite-off/on.txt`。该全套随机顺序不能充当相同初态的比较，
未删除该差异或归一化 scope 后宣称一致；独立进程结果见 `r3b-equivalence-*`。

## 4. 成本与剩余门

固定普通 allocator 时间和独立 requested-allocation 观察进程，N=64/256/2048、33 条 advance，
checkpoint 固定第 17 条 H=1119、目标第 33 条 H=1135；每规模 3 对 origin/hint，不追加样本。
普通进程测工厂首次成本、forkForSeek、首条/其余记录及 source unbind/reset/final release；
观察进程单独采 source/archive/reference DTO/checkpoint/active/retired/final 请求字节。
固定登记 `r3b-profile-registration.json` 绑定源码/工具/库/二进制，两个进程依次完成，
普通时间 219 assertions、内存 153 assertions 均通过，未追加样本；原始数据、结果、分析分别为
`r3b-profile-time/memory-raw.log`、`r3b-profile-result.json`、`r3b-profile-analysis.json`。
观测时间不承担性能判断，requested bytes 不冒充 RSS/生产 cap。

| requirements | 原点 forkForSeek，3 样本范围 | 可信 hint，3 样本范围 | 首次 checkpoint 工厂 |
| --- | --- | --- | --- |
| 64 | 13.448–14.333 ms | 6.021–6.801 ms | 47.866 ms |
| 256 | 53.860–61.636 ms | 24.794–25.355 ms | 195.276 ms |
| 2048 | 651.938–671.042 ms | 271.753–278.593 ms | 1769.764 ms |

这是一次进程的有限诊断，没有 CI、15% 正式噪声门或三个正式窗口；不能升级为正式改善区间。
新旧 Snapshot 全状态相符且 base=17，说明确实跳过前缀；原点与 hint 均为同一 R3b 构建，
不是将 R3a 的不同构建拼入本组。256/2048 的当前样本仅内核边界已超过 20 ms。
工厂包含原点执行、全量 Snapshot 和 canonical 编码/hash，不隐藏在 load 之前。

| requirements | 源首条 advance | 源其余 32 条合计 | 新操作解除最后 source owner | reset/释放旧 base | 最后 session handle |
| --- | --- | --- | --- | --- | --- |
| 64 | 4.774 ms | 9.709 ms | 4.854 ms | 4.648 ms | 0.0161 ms |
| 256 | 18.706 ms | 42.031 ms | 17.852 ms | 18.920 ms | 0.0012 ms |
| 2048 | 161.917 ms | 410.683 ms | 141.331 ms | 149.492 ms | 0.0015 ms |

解除 source 的数值包含该次 advance 自身工作，reset 包含既有 reset 逻辑；不冒充纯析构排他时间。
最后 session handle 很快不能代表旧历史的释放很快，其大部分释放已经发生在前两个边界。
首条 advance 为真实原始 record，没有插入假小 advance 或专门命中目标的 checkpoint。

内存下表为相对同进程已准备 fixture 基线的 requested live 增量，单位 MiB（1,048,576 B）。
峰值同样减去该 fixture 基线，为整段生命周期累计峰值，不是各阶段峰值之和。

| requirements | checkpoint 已生成 | restored + 外部 handles | 只剩 restored | 解除 source 后 | reset 后 | 本段 owning handles 释放 | 生命周期峰值 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 64 | 20.43 | 27.02 | 26.14 | 14.22 | 0.38 | 0 | 38.68 |
| 256 | 80.92 | 106.99 | 103.60 | 56.36 | 1.51 | 0 | 150.80 |
| 2048 | 645.56 | 853.64 | 826.78 | 449.77 | 12.08 | 0 | 1199.37 |

各列包含该阶段全部产品状态/共享 owner，不能把列差直接命名为 checkpoint 独占大小。
2048 的峰值原值 1,287,687,760 B，fixture 基线 30,054,051 B；本段 session/archive/checkpoint/Snapshot
handles 释放后回到基线。prepared/config fixture 仍在基线中存活，未独立计时其最后一次释放。
该零增量证明本段 requested-allocation 域的释放，不证明全进程产品 owner 清空、RSS 回落、40k、M-CAP 或 Close 接受。
实测固定对象 sizeof 为 certificate=112 B、log chunk=64 B、SnapshotStorage=1176 B；不含堆 payload/control block，
不能用这些小数代替上表的完整生命周期。

另登记一个宏 ON、普通 allocator 的结构进程 `r3b-work-registration.json`，相同三规模每种路径仅 1 次，
183 assertions 通过，未增加观测字段；原始记录 `r3b-work-raw.log`：

| 路径（每规模相同） | 执行 records | draft | fault reserve 副本 | Fold 副本 | finalization 副本 | 尾部 record 值复制 |
| --- | --- | --- | --- | --- | --- | --- |
| 原点 | 33 | 5 | 43 | 5 | 33 | 496 |
| 单可信 hint | 16 | 0 | 17 | 0 | 16 | 120 |

每次 prepare 调用均为 1；观测 prepare/index（index 嵌套其中）分别为原点/可信：
64 的 758/714 us 与 240/247 us；256 的 3068/3044 us 与 1211/1205 us；
2048 的 27344/29320 us 与 12590/12829 us。它们是插桩归因值，不接受普通时间预算。
既有 `fullTicks` 在该 ledger 实际填 advanceCalls=33/16，不能把它解读为 33/16 个 full Tick 事务；
本表直接列真实复制点。尾部均为空 batch，512 输入 payload 的边界另由功能测试覆盖。
这些计数证明剩余重复工作仍在；没有各副本/比较/析构的独立排他时间，不断言某一种复制已被证明是唯一主因。

forkForSeek 排除随后 Snapshot、Host 协议、clock、sample/present，不承担端到端 20 ms 接受。
普通时间进程冷启动一次，但各规模共享进程，不能冒充三规模各自独立冷启动；没有外部 Reader 首次认证样本。
自然首次 checkpoint、外部首次与无 checkpoint 的生产适用性仍须分别判断。
本批未进行 40k、ETW/WPR、真实设备或 R4 正式窗口；不修改阈值，也不从小规模外推通过。

## 5. 回滚与保全

`r3b-entry-files.zip` 保存 7 个入口源/测试文件，独立 `r3b-product.patch` 的隔离反向/正向字节验证通过：
6 个实际改变路径，另 2169 个入口路径未改（后补 wrapper 前的检查身份）。
验证仅证明字节恢复，没有宣称 patch variant 分别编译；主树未 reset/clean。
新增 wrapper 从独立 `r3b-wrapper-entry.cpp` 保留可撤起点；后续 `r3b-final-product.patch` 包括该用例，
8 个路径中 7 个实际改变路径的隔离反向/正向验证通过，另 2168 个入口路径相符。
继承的 R2 文档、接受记录和历史原始材料保持原字节。

## 6. §12.2 裁定包与继续工作的边界

当前候选有可验证的热路径收益，但不满足原判据，R3 不能退出。合法无 checkpoint 的 256/2048 样本仍超界，
可信路径自身也超过 20 ms，且首次生成/退休释放/生命周期峰值均不可忽略；继续堆密集 checkpoint 没有预算证明。
这触发总计划 §12.2 的当前候选停止条件，不代表所有设计都不可能，也不是 R4 的正式失败窗口。

推荐保留原判据与未通过状态，下一批先做限定表示设计：分别处理 prepare 的可封存派生索引、
canonical 首次/重复身份工作、动态 finalization/reserve 与历史 owner 退休，提供逐成员写集、回滚/first-fault、
旧 owning 与物化出口的具体映射，再按相位消融选择最小实现。现有计数不足以默认选通用持久容器或删除 reserve。
R3c 需要独立语义接受；161.917 ms 的真实首条记录是必要性线索，但 continuation 不能替代准备、首次认证、发布和退休成本，
本批没有自动进入 R3c，也没有改变公开 wire/SDK 或预算。

供 owner 裁定的具体选择仍按总计划：保持判据继续限定整改（推荐）；独立产品决定改变合同/预算；
或明确暂停相应规模/能力交付。后两项均未接受，不把当前 64 的内部样本写成部分生产交付。
F1/F4、40k/Packed/Seek 物理门、真实 audio/GPU/输入设备、生产 caps 与预算接受均保持原未完成状态。
已测/未测、失败及原始数据、各 owning 阶段、源/工具/构建绑定与可撤差异见以上各节和
`r3b-final-audit/binding.json`；该绑定是取证清单，不是退出接受书。
