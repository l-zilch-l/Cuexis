# 轻量回收进度、幂等退役与回复等待边界实施

状态：implementation evidence / affected regressions verified；PD-0 最终源码计时组超噪声界，已停止追加测量。
日期：2026-10-17（Asia/Shanghai）。PD-0 尚未退出。

最终接口边界复核发现，首轮候选把回收读取方法放入了 `PlaybackSession` 的 candidate public 段。
现已移至真正 private 段，仅授予非安装 `HostCoordinationAccess` 桥友元访问；
新增编译期检查禁止外部作用域调用。最终实现作用域为
`42ac289c3ed56ad4bc792499e6cf7829b7abce995cb957188e066cbc3af2d9f9`。
下文注明 `ccf03f...` 的数据保留为私有化前候选证据，不能直接改绑到最终源码。

本轮按用户“先修改相关文档，然后开始按照新方案继续实施”的授权，消费
[方案复审](2026-10-17-wait-and-retirement-design-review.md)。先更新
[RPA 合同](../../../../formats/rpa-execution-contract.md)、
[宿主边界](../../../../api/realtime-host-boundary.md)与
[SDK 首用说明](../../../../api/execution-coordination.md)，文档检查通过后实施。
当前任务由 [CURRENT_STATUS](../../../../CURRENT_STATUS.md)维护。

## 实现与职责边界

### 轻量回收进度

[ExecutionScheduler](../../../../../engine/judgement/src/execution_scheduler.cpp)继续唯一裁定接纳高水位与回收前缀。
`retentionProgress()` 返回构建执行实例时的 context、lastAcceptedId、reclaimedThrough，
只复制小型值，不读取或复制 KernelProjection。Resume/时钟段转换不改写该实例身份。
已确认连续前缀释放 map 节点；乱序退役空洞后的身份继续保留，Waiting 不可退役。

[内部宿主桥](../../../../../app/execution_host/host_candidate.hpp)经 Playback 私有读取方法取得该进度。
读取成功路径不构造完整 GameplayResult。最终桥与元数据结构不安装；
最终读取方法为 private，Playback 原有公共方法签名和对象布局没有改变。
内部 Judgement 头没有新增 Playback/宿主依赖。

[SessionWorker](../../../../../app/execution_host/session_worker.cpp)只依进度清理 coherent 原始载荷：
同实例释放前缀内记录，实例更换则清理旧实例缓存。它不独立计算回收前缀，
也不再遍历并 query 完整结果判断寿命。普通成功推进不额外查询完整结果来确认是否已接纳；
拒绝/Busy 路径使用 lastAcceptedId 判断是否保留原始请求。
原结果查询仍用于显式 query 和未退役请求的精确重传。

已回收请求在映射时钟之前拒绝；未退役同 ID 异 coherent 载荷仍撤销执行许可。
两类原始数据没有强行合并：宿主保存 ClockObservation，调度器保存映射后的 AdvanceRequest。

### 重复退役与控制切换

前缀内 query/推进重传返回 `host.rpa.request_retired`，不恢复结果或重新执行。
前缀内重复 retire 幂等成功；这只确认无需保留，不证明原请求执行成功。
尚未确认或非终态请求仍按原规则保留/拒绝退役，容量没有扩大。

[Playback](../../../../../engine/playback/src/playback_session.cpp)在成功 Seek、restore 或 Reset/Stop
替换执行状态时，将旧调度器已接纳高水位暂存为新实例的 retired floor。
该转移只发生在可失败准备/发布已成功后的替换点；失败不提前删除请求身份。
新调度器创建成功后清除临时转移值，重新由活动调度器拥有边界。
新内容提交和 Close 清除该转移状态，不能把旧实例临时数据带入独立加载。

这不序列化请求缓存到 Replay/Snapshot，不改变 canonical 时间、Fact/Fold 结果或 B4 设备证明。
Seek 的旧 context/请求结果历史在显式状态替换后失效；旧 ID 不能通过新 context 再执行。

### 回复等待

[OwnerChannel](../../../../../app/execution_host/owner_channel.hpp)保留目标回复持久终态、同 mutex
及独立 reply CV；poll/ACK 生命周期不变。close admission 不伪装 pending 回复已完成。

[ExecutionHost](../../../../../app/player/src/execution_host.cpp)在等待 session/audio 回复时，
主线程泵之后重新计算剩余 deadline，等待不超过剩余时间与原泵切片的较小值。
内部等待使用 steady_clock duration，避免把不足 1 ms 的剩余预算扩成完整一片。
总体超时仍由 host 裁决；这不承诺操作系统在 deadline 时刻立即调度线程，也不移除输入/窗口泵。

通道现编译期要求 Reply 与 core::Error 可无异常移动。实际 SessionReply/AudioWorkerReply 构建通过此约束。
不新增 owner、future、无锁容器、平台句柄、sleep、优先级或绑核要求。
PD-4 的观察性 receipt 异步化仍未实施；可靠 Gameplay/control 不因该计划被改成可丢消息。

## 首轮候选普通回归

实现作用域 SHA-256：`ccf03f8657e9854de55948e88d707737c6409f5791fb0c898871a40894b2d43c`。
HEAD 为 `ef7fa30d5b578896ccc165622fe256c553a5641c` 加工作区修改。

Debug/Release 均构建后，不传测试名/tag 过滤，使用固定 rng seed 1，顺序运行八个程序：

| 程序 | 每配置普通 case 数 | Debug / Release |
| --- | --- | --- |
| cuexis_pd_baseline_probe | 6 | 通过 / 通过 |
| cuexis_pd_host_probe | 6 | 通过 / 通过 |
| cuexis_host_protocol_tests | 35 | 通过 / 通过 |
| cuexis_execution_host_tests | 26 | 通过 / 通过 |
| cuexis_rpa_tests | 52 | 通过 / 通过 |
| cuexis_judgement_tests | 207 | 通过 / 通过 |
| cuexis_playback_tests | 154 | 通过 / 通过 |
| cuexis_runtime_tests | 39 | 通过 / 通过 |

共 16 次程序运行、1050 次普通 case 执行通过，manifest 核验实现作用域未变化。
隐藏测量 case 不在上述默认运行内；这不是全仓库 CTest 或设备验收声明。
旧 EOF tail 身份容量失败已在两配置完整宿主程序中通过。

新增 PD 回归包括：容量 2 下连续 64 次 coherent 推进/退役并重复确认，
未退役重传/冲突、乱序空洞和满额度拒绝、失败 retire 不清缓存、过期时钟下已回收 ID 拒绝。
SDK Seek 用例保留一个未确认终端结果，检查失败 Seek 保留结果、成功 Seek 继承 ID floor、
旧 ID 不重新采样时钟以及随后超过原容量的新请求继续运行。
原回复等待四个普通 case继续覆盖 completion-before-wait、排队/无关结果、worker 等待及关闭错误 ACK。

旧 Playback 退役后冲突断言也已同步：用新的未确认请求验证 payload conflict 仍进入 Suspended，
不再要求已释放的完整载荷参与比较。对应旧失败日志保留，没有降低噪声/行为门禁。

## 故障注入用例修正与证据限制

第一次新增的“回复移动抛异常”注入不适用于当前 Result 依赖：
tl::expected 的 `construct_with()` 是 noexcept，内部移动注入类型可直接终止，
通道外围 catch 不能把这种用法变成错误回复。
完整程序曾退出 `3221226505 / 0xC0000409`；未取得独立 crash dump，
故源码机制与该退出一致，但不冒充已取得调用栈证明。
该人为类型已移除，改以编译期约束阻止类似回复类型进入通道；没有修改依赖或全局分配行为。
生产终端 Error 回复、持久终态和 ACK 测试保留。

原始记录：

- `out/pd-20261017/retirement-regression-20261017T035703Z/`：注入类型存在时的退出。
- `out/pd-20261017/unsupported-reply-move-injection.txt`：注入形状、依赖源码机制和处理边界。
- `out/pd-20261017/retirement-regression-20261017T035902Z/`：旧 Playback 冲突断言失败。
- `out/pd-20261017/retirement-regression-20261017T040001Z/`：最终 16 次普通回归、源身份与日志 hash。
- `out/pd-20261017/retirement-build-{release,debug}.log`：最终构建。

## 首轮候选兼容检查与恢复测量

同实现作用域补完 shared candidate ON / metrics OFF 的 Playback、Runtime、execution host，
以及 static candidate OFF / metrics OFF 的 Playback、Runtime，五次普通程序均通过。
证据：`out/pd-20261017/retention-guards-20261017T040225Z/`。
Release 架构检查、diagnostic formalization 检查和安装头 ASCII 核对通过。

安装 SDK 消费者检查在 VS Developer 环境下通过，见
`out/pd-20261017/retention-installed-consumer-devshell.log`。
首次从普通 shell 调用因 linker 找不到 kernel32.lib，未到达有效的消费者兼容验证；
原环境失败保存在 `retention-installed-consumer.log`，没有改动源代码或放宽门禁后重试。

`out/pd0-owner-wait/20261017T040334Z-9984b4/` 的预登记 12 个 barrier 进程全部通过，
覆盖 Debug/Release 与 default/single affinity 三轮；状态 reply_wait_verified。
该矩阵只验证受控回复时序，不证明所有历史慢运行的成因。

默认亲和性的宿主 CPU profile 完成四进程校准及三组配对、12 进程正式测量：

- `out/pd0-host-delivery/calibrate-20261017T040343Z-1f4814/`：calibration_applicable。
- `out/pd0-host-delivery/accept-20261017T040350Z-e497fa/`：baseline_validated。

| 配置/观测 | 总请求至 receipt ACK median / p95 / p99 / max（μs，n=1536） | 三进程均值噪声 |
| --- | --- | --- |
| Release dormant | 38.4 / 67.8 / 91.9 / 157.1 | 11.99% |
| Release active | 40.2 / 67.2 / 93.8 / 167.1 | 7.59% |
| Debug dormant | 344.3 / 442.9 / 493.6 / 684.1 | 1.84% |
| Debug active | 348.0 / 446.6 / 495.2 / 786.1 | 4.66% |

预登记主噪声界为 15%；配对观测相对效应 Release 3.29%、Debug 1.26%，均在 15% 内。
全部样本和尾部保留，源码及 binary 核验未变化，既有 inconclusive 组保持原结论。
该 profile 仍是两对象、无 Gameplay/audio/GPU 的现有 CPU TestPresentationRenderer 路径，
不能代表 40,000 实体、GPU 完成、物理设备或生产预算。没有新的受控旧/新 A/B 加速倍数结论。

单 owner 方法及存储基线仍需在该源码下完成重绑定。本报告不宣称 PD-0 退出。

## 最终私有接口源码的验证

最终源码 `42ac289c...` 已完成下列不带用例名/tag 过滤的普通程序运行，全部通过：

| 配置 | 程序运行数 | 证据目录（均在 out/pd-20261017/） |
| --- | --- | --- |
| MSVC static candidate ON，Debug/Release，metrics ON | 16 | retirement-regression-20261017T043051Z |
| MSVC shared candidate ON / static candidate OFF，metrics OFF | 5 | retention-guards-20261017T043412Z |
| MinGW Release candidate ON | 4 | retention-mingw-20261017T043428Z |
| Ubuntu/WSL Clang Debug，headless candidate ON，ASan/UBSan、warnings-as-errors | 8 | retention-linux-20261017T043300Z |

共 33 次程序运行。各 manifest 记录同一最终实现作用域，包含逐程序 binary/log hash；
Linux 启用 leak detection 和 sanitizer halt-on-error。
这是受影响组件完整普通用例验证，不是全仓库 CTest、全部 RPA 矩阵或实体设备验证声明。
最终安装消费者、架构、诊断码检查及私有化两文件 clang-format 检查也已通过；
安装消费者记录为 `out/pd-20261017/private-installed-consumer.log`。

Linux 使用本地源码副本，HEAD 与原工作区一致，先校验 1925 份快照文件，
再同步私有接口两文件并独立重算最终实现作用域；细节见 `linux-native-stage.json`、
`linux-source-snapshot-files.json`、`linux-native-private-source.json`。
挂载目录和混合 Windows PATH/cwd 的配置尝试仅因 I/O 慢被停止；
在纯 Linux cwd/PATH 下完成配置。被停止的配置不算测试失败或通过。

最终测量按 `out/pd-20261017/final-measurement-20261017T043805Z/manifest.json` 中的固定顺序执行，
一项失败立即停下，不重试；首轮候选数据保留原源码绑定。

## 最终源码测量停点

最终源码 `42ac289c...` 的执行结果：

| 测量组 | 结果 | 证据目录 |
| --- | --- | --- |
| Debug/Release、default/single affinity，三轮 barrier，共 12 进程 | reply_wait_verified | out/pd0-owner-wait/20261017T043805Z-e9fe1c |
| 默认亲和性宿主校准，4 进程 | calibration_applicable | out/pd0-host-delivery/calibrate-20261017T043810Z-676179 |
| 默认亲和性宿主三组配对，12 进程 | baseline_validated | out/pd0-host-delivery/accept-20261017T043813Z-0d1b01 |
| 单 owner 方法校准，4 进程 | calibration_applicable | out/pd0-scheduling-v2/calibrate-20261017T043818Z-453e2a |
| 单 owner 方法三组正式重复，12 进程 | inconclusive | out/pd0-scheduling-v2/validate-20261017T043856Z-a8d517 |
| 最终源码存储与普通分配器交付重绑定 | 未开始 | 驱动已按首个失败停止 |

宿主组四格的噪声分别为 Release dormant 1.95%、active 7.11%，
Debug dormant 1.14%、active 2.78%，均在预登记 15% 内。
配对观测相对效应 Release -1.20%、Debug 0.98%，也通过门禁。
这仍只代表声明的两对象 CPU profile，不是设备/GPU 或生产预算。

单 owner 正式组所有进程自身的行为检查均通过，legacy/RPA 各自的 Debug/Release
Replay、Snapshot、FrameDigest 与 Packed identity 精确相同；源码、binary 没有变化，
长块时长检查通过。但 12 个主指标中的以下 4 个超过预登记的 15% 噪声界：

| 配置/profile/指标 | 三次进程中位数（μs/operation） | 噪声 |
| --- | --- | --- |
| Release legacy / long read | 0.240653、0.313984、0.234437 | 33.05% |
| Debug legacy / long read | 14.308813、11.579913、11.437477 | 24.80% |
| Debug RPA / dormant trace | 437.719922、440.069775、531.184863 | 21.24% |
| Debug RPA / long read | 11.284373、15.507069、11.458211 | 36.85% |

这里是块或完整轨迹总耗时除以 operation 数后的进程中位数，不能解释成单帧延迟分位数。
四组配对观测开销门禁均通过；整体仍必须保持 inconclusive，不能用其他通过项替代这些主指标。

现有诊断通道也有相应变化。例如 Debug RPA long read 的线程 CPU 计时为
10.967255、15.735626、10.967255 μs/operation，raw cycles 为
42524.58、58326.29、42910.31 cycles/operation。
线程 CPU 计时较粗，cycles 不作 GHz 换算；这些结果不足以把波动仅归结为脱调度等待，
也不能证明频率、温度、SMT、后台竞争或基准缓存状态中的某一项就是原因。

校准与验证原始 hash 索引分别核验 13 / 37 份文件，派生分析位于该验证目录的
`analysis.json`、`analysis-hashes.json`；分析脚本是
`out/pd-20261017/analyze_final_inconclusive.py`。
本轮没有删异常窗口、降低阈值、改用 CPU/cycles 替代失败 wall 指标或追加重测。
较早 `ccf03f...` 的成功组仍属于其旧候选，不能替代最终源码这一失败结果。

按用户要求已停下，下一步需选择系统运行状态联合诊断或先审计基准工作量/缓存。
生产修复与受影响回归已具备，PD-0 尚未完成。
