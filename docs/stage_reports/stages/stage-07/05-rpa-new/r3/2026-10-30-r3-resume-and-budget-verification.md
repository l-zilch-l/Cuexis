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

## 固定成本 census：先量成本，再决定预算

[登记](2026-10-30-resume-evidence/retention-census-registration.json)固定 1K dense、40K dense、
40K 六波/六代 owning archive；每场景比较 quota=0、历史 128 MiB 和 SIZE_MAX。
SIZE_MAX 只用于关闭字节准入、保留四槽上限以观察成本，不是候选生产上限或为了通过原门。
1 case / 141 assertions 通过；所有 requested/CRT storage 在最后 owner 释放后差值归零。

| 场景 | 收点版较 quota=0 多保留 requested | 多保留 CRT usable | 全额计费 |
| --- | ---: | ---: | ---: |
| 1K dense 单点 | 11,303 B | 11,559 B | 6,115,528 B |
| 40K dense 单点 | 323,303 B | 323,559 B | 244,028,048 B |
| 40K 六波、六代归档 | 1,176,174 B | 1,178,062 B | 966,657,872 B |

上表在 writer 销毁、所有原始 owning generations 仍保留时配对相减。六代中只留下最早一代后，
requested 额外保留回到 56,639 B；最后一代释放后 optional charge=0。40K dense 峰值 requested
两版同为 463,568,821 B；六波峰值从 369,323,129 增至 370,166,905 B（+843,776 B）。
这是单次固定夹具实际成本，不是一般内容的上界、统计性能接受或完整进程峰值。
CRT usable 包含观察器 header/alignment；private/working set 为含夹具及分配器滞留的绝对采样。
完整[原始结果](2026-10-30-resume-evidence/retention-census-results.json)与
[摘要](2026-10-30-resume-evidence/retention-census-summary.json)已留档。

当前证据支持避免重复计算 mandatory owner 共享页，但不直接推出某个合理产品预算。
独立只读审阅确认普通 ledger append-only 路径，同时发现 ReplayArchive 默认赋值会先替换 data_
再销毁 nativeRecovery_；若使用完整页豁免，必须先关闭这个生命周期窗口。计划用 index 自有的
exact source floor 将源保活到 ticks/points 析构之后，保持 index 不可独立导出；并以真实 deleter
事件验证 copy/move assignment，而不是只查看最终 live==0。
另需让 reference 独立 decodeReplay，避免同一个 anchor 的外部参考意外保活待验证的旧源。

## R3c v4：共享页计费与 exact source floor

v3 候选只对 append-only Fact 完整页免重复计费，仍计目录、partial tail 容量和嵌套字段；
其他所有状态仍全额保守计费。成功原记录先发布，再核对每个完整 Fact 页的 owner 身份，
不同 owner 即 miss，值相等不能代替 provenance。暂存阶段仍依赖 ledger 不改写完整页的证明。

独立审阅发现归档默认赋值顺序问题后，v4 在私有 NativeRecoveryIndex 的首成员持有
exactSourceFloor，源先绑定、ticks/points 后复制，析构时先释放点与 lease、最后释放精确源。
不修改 ReplayArchive 的安装载体或默认赋值，也不允许独立导出 index。Seek 分歧先释放索引，
再释放原 source。新的分支反例采用独立 decodeReplay，并检查原 ReplayData weak owner 确实过期，
不再用共享同一 anchor 的 reference 掩盖生命周期缺口。

v3 四目标做完整 fresh Release STATIC，随后 v4 只变 kernel 私有定义及 B3 测试。
逐 tracked 源码比较确认所有 header/template/其他 cpp 与 fresh v3 完全一致，因此显式重编两个 TU，
四个套件重新链接并运行，不借助不完整的增量依赖推断。

| v4 套件 | cases / assertions |
| --- | ---: |
| RPA | 79 / 27,258 |
| Judgement | 236 / 56,139 |
| Host | 62 / 54,548 |
| Packed | 51 / 5,641 |

总计 428 cases。原型 writer 性能用例已隐藏，普通 CTest 不会默认进入 stdin barrier。
相关[构建身份](2026-10-30-resume-evidence/r3c-v4-build-complete.json)和日志已归档。
Judgement 头仍不安装；当前差异不增加安装版 Playback carrier/wire 字段，API/date 版本未改。
这不是 Debug/SHARED/MinGW/安装消费者或完整 CTest 的最终矩阵。

独立 observer 故障/边界等 **14 cases / 1006 assertions** 通过，包括真实 lease 后 allocation 失败，
故障不提升、退款、原点回退，以及 copy/move assignment 时源 deleter 直接观察 point 已过期、live=0。
故意删除 source-floor 绑定的 mutant 为 exit 42、32 assertions 中 4 项失败。
实际 promotion guard 用例对最终首个/最后一个完整 Fact 页执行 no-op COW，状态值与独立参考及
Snapshot wire 相同，实际 point 拒绝提升并退款，**58 assertions** 通过；绕过 guard 的 mutant
为 exit 42、58 assertions 中 4 项失败。完整页谓词测试不再是唯一 guard 接线证明。

v4 实际堆组 **1 case / 103 assertions** 通过。40K dense 收到一个点，payload charge 为
45,263,048 B，较无捕获 writer 的实际 requested 多保留 323,303 B；峰值 requested
两版均为 463,568,877 B。最终 requested/CRT storage 差值均 0。
观察器对 source owner 使用自定义 deleter 记录顺序，其 header/control-block 与进程样本包含扰动，
不能把该绝对值直接当普通进程生产峰值。

原始工作目录为 `out/verification-20261030/r3-resume-v1/`，v4 源码为 `budget-source-v4`，
完整 fresh 公共库为 `budget-build-v3`，最终四套件为 `budget-build-v4`。
[未选用源码补丁](2026-10-30-resume-evidence/r3c-v4-unselected.patch)相对当前产品树生成，
仅做 apply --check，未应用产品树；[selection](2026-10-30-resume-evidence/r3c-v4-selection.json) 为 false。
[独立审阅记录](2026-10-30-resume-evidence/r3c-v4-independent-review.txt)、
[最终 observer 身份](2026-10-30-resume-evidence/v4-observer-identity-supplement.json)
明确区分 preparatory draft binding 与最后实际源码，原绑定的旧标签未追改。

## 最新普通 writer / Seek：收益与重复性分开

全部普通子进程 mask=4、BelowNormal、execution throttling OFF，controller mask=3；
没有与构建并行。观察器与故障 hook 不参与普通性能。每进程三个全新 writer 或三个 Seek 请求；
固定 AB/BA，不补样、不丢慢值。

[Writer 固定 12 进程](2026-10-30-resume-evidence/v4-writer-registration.json)：

| 场景 | baseline advance 中位数 | candidate | 变化 |
| --- | ---: | ---: | ---: |
| 1K dense | 11.525 ms | 11.76615 ms | +2.0924% |
| 40K dense | 654.013 ms | 628.591 ms | -3.8871% |
| 1536 六波 | 14.2997 ms | 15.1941 ms | +6.2547% |

40K <=5%、两保护项 <=10% 及原相对范围 <=15% 的本地 guard 通过，Snapshot wire hash/长度相同。
prepare/encode 在这项 advance 计时外，不能当 SDK load 或整体生成成本接受。

[Seek 固定 14 进程](2026-10-30-resume-evidence/v4-seek-registration.json)：

| 场景 | baseline 请求到 CPU target present 中位数 | candidate | 变化 |
| --- | ---: | ---: | ---: |
| 1K H902 | 4.171 ms | 0.3165 ms | -92.4119% |
| 40K H5 | 16.439 ms | 16.549 ms | +0.6691% |
| 40K H902 | 250.021 ms | 2.655 ms | -98.9381% |
| 1K final guard | 0.326 ms | 0.337 ms | +3.3742% |

所有候选请求 <=20 ms，保护项未超 5%；40K H902 candidate 首次 2.419/2.842 ms，
重复 2.338–3.047 ms。完整 Snapshot hash/长度及四组 raw 文件逐字节相同。
但是候选 process medians 的原 `(max-min)/median`：1K H902 为 **22.4329%**，
40K H902 为 **17.7778%**；40K baseline 为 17.0458%。**本地数值收益门通过、重复性 screen 未过**。
保留失败，不重复到通过，不把旧无预算版成绩换入新组；不是正式/CI、GPU、设备或生产接受。
原始[结果](2026-10-30-resume-evidence/v4-seek-results.json)及
[判定](2026-10-30-resume-evidence/v4-seek-decision.json)已版本化。

## 固定 quota / 成本曲线：没有选定产品额度

[固定登记](2026-10-30-resume-evidence/v4-quota-frontier-registration.json)为 40K 六波、六代归档，
额度事先固定为 0/32/64/128/256/512 MiB；100 assertions 通过，各自最终 requested/storage 差值为 0。
Snapshot 对照使用相同六次 advance 的无点参考，数据无差异。

| 额度 | H904→1904 的当前索引点数 | 六代保活收费 | writer 销毁后实际额外 requested |
| --- | --- | ---: | ---: |
| 0 / 32 MiB | 0,0,0,0,0,0 | 0 | 0 |
| 64 MiB | 1,1,1,1,1,1 | 45,053,456 B | 56,639 B |
| 128 MiB | 1,2,2,2,2,2 | 90,159,856 B | 166,550 B |
| 256 MiB | 1,2,3,4,4,4 | 225,868,160 B | 843,776 B |
| 512 MiB | 1,2,3,4,4,4 | 271,139,384 B | 1,176,174 B |

256 MiB 下第六点被拒，旧归档仍保活被当前索引淘汰的点；512 MiB 收了全部六点但当前索引仍最多4。
因此“当前四槽”与“跨代全部存活点”必须分别计账。曲线说明保守收费仍远大于实际新增内存，
以及 128 MiB 会影响多代覆盖；**它不推荐把额度调到512 MiB，也不证明128 MiB合理**。
目前缺少设备/整条播放链 headroom 和一般内容上界，因此产品额度仍未选定。
完整[成本曲线](2026-10-30-resume-evidence/v4-quota-frontier-summary.json)留档。

## 当前处置与未完成项

P1 v2 不选用；R3c v4 的可恢复补丁与证据已留档，但仍未提升产品树。产品源码继续保持 ff93484。
后续先处理普通 Seek 重复性来源、合适的缓存预算/保留策略及全部适用目标覆盖，再决定产品选择。
不能因 H902 数值很好而跳过外部/首次/cold/fallback、load 原门或最终 Debug/Release/STATIC/SHARED
消费者与全 CTest。40K 实际整进程/headroom、原历史 reference、R4/PD-5/设备/生产/owner 仍独立开放。
本轮还观察到 fresh v3 的 Ninja deps 记录为空（中文 showIncludes 前缀未被消费）；因此未用增量
依赖图推断可复用范围，而逐文件核对头与源码并显式编译/链接。后续完整配置须单独修正本地依赖跟踪，
不修改冻结成功构建以补写历史证据。
