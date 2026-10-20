# PD-5 measurement and threshold proposal

日期：2026-10-19。状态：**本机 reference 范围、全部阈值和协议已获 owner 接受；工具/profile 准备中**。
owner 明确回复“接受范围、全部候选阈值和协议，继续工具/profile 准备（推荐）”。
正式采样未注册、未开始，没有接受完整生产 profile 或 owner 拆分。
当前阶段状态唯一见 [CURRENT_STATUS](../../../CURRENT_STATUS.md)。

本方案消费 [PD 计划](plan-presentation-decoupling.md)及[设计 §9](presentation-decoupling-design.md#9-容量-profile测量与退出pd-0--pd-5)，
现场盘点见[就绪检查](../../../stage_reports/stages/stage-07/pd/2026-10-19-pd5-measurement-readiness.md)。
以下新增数值是本次接受的本机有限 testOnly 工程目标，不是已有实测、行业标准或硬件保证。

## 1. 已接受的范围

本次接受 **local-reference-v1 的有限 testOnly 验收**：Windows/MSVC、STATIC、candidate ON、
Release 正常分配器，PD metrics 编译 OFF、无 ETW。Debug 同方法独立报告，不套用 Release 延迟预算。
主对照为冻结 pre-PD 实现 A 与当前 PD-4 实现 B；两者使用独立程序、相同工具链及对应源码清单。
旧 SDK 不兼容时分别编译各自 harness，不把旧头文件或库混入 B。新增测量边界若需要改编 harness，
另存新程序并保持原始程序/日志不变；语义差异须先通过精确等价检查。

设备参考设置：本机 RTX 4060 Laptop、1280×720、窗口模式、VSync ON、当前系统默认 Senary Audio 耳机、
gain 0.35。240 Hz 是建议的测试目标：登记时核验实际显示刷新率、设备/驱动和默认音频端点；不从 FPS=240
推定显示器配置。若不是 240 Hz，停止登记并修订方案，不能测完后改目标。音频设备使用实际稳定端点身份冻结，
不是仅记录易变化的 system-default 别名。CPU headless 不启用 GPU/audio/VSync。

生产 40,000 实体、40,000 requirements、Packed ≤16 MiB 与适用 Seek ≤20 ms 保持原
[RPA-F3](../../../stage_reports/stages/stage-07/rpa/2026-10-15-f3-final-baseline.md)目标及 PD 后重测要求；
小场景通过不能覆盖这些规模。prepare-only heap、decoded heap、设备恢复、精确 AV 同步和完整生产预算
继续单独接受，不以本方案补默认值。

## 2. 工作负载与不可混用项

每个启用 profile 必须在采样前给出 content/Replay/输入、逻辑时间序列、viewport、U/F/R、
对象数、绑定数、消费者/请求策略和各容量的精确清单/hash；下表是待实现的有限矩阵，不是可直接执行的注册。
已有 DFJK 和真实音乐继续使用各自独立谱面；不得将二者合并为一个性能样本。

| 单元 | 固定工作与对照要求 | 判定范围 |
| --- | --- | --- |
| P0 owner baseline | 保留固定 legacy/RPA trace、Playback-only、read-long；同输入、同逻辑采样点 | A/B 三项端到端边界；read-long 每块至少 100 ms |
| P1 无消费者 | 同 U，F=R=0；两实现均无完整帧消费者 | B 无完整物化的结构计数；旧路径无共同指标则 N/A |
| P2 高频更新/低频画面 | U:F=8:1，固定同一逻辑轨迹 | 不用少画帧伪装相同 F 下的收益；A 无该模式则 B-only |
| P3 每次更新交付 | U:F=1:1，单 renderer，同一 content/time/viewport | 公共 request→ready、CPU submit、present-return；D2/D3 另行归因 |
| P4 同修订截图 | 固定 1/2/4 个 CPU capture 消费者，renderer 仍只有一个 | B-only 容量/重复物化成本；A 不支持时不能补 0 |
| P5 控制与暂停画面 | 固定 Pause/Resume/Reset/resize 脚本；运行中和暂停中 Reset 分层 | 控制排队、重载、首帧恢复、resize、Close；不混入稳态分位数 |
| P6 稀疏/拓扑 | 同规模的固定稀疏变更轨迹，拓扑/资源代更换独立子单元 | 触及对象、候选/scratch、复制 bytes、析构/回收 |
| P7 GPU/audio | 独立音乐 + DFJK 的脚本化输入轨迹，真实 renderer/音频 | B-only 设备测量；旧版长 DFJK 已知失败，不用 Reset 隐藏容量缺陷 |
| P8 RPA-F3 回绑 | 40,000 两种 reuse 内容、Packed、Snapshot/Seek、Replay | 另行完整规模证据；不以 P0–P7 代替 |

旧 host 的 request→receipt ACK 与 B 的 request→present-return 不同，禁止直接计算收益。
request→ready 只有重建双方相同边界、核对固定逻辑工作后才可配对。submit、present 返回、receipt 入队、
worker 处理 receipt、GPU 完成分别命名；异步成功数不是 ACK 完成数。
旧 ledger 超界后的长稳态不构成合法 A/B 单元：A 保留失败，B 长稳态独立判定；
双方共同短轨迹的对照必须预先命名为短轨迹，不能事后截取到 A 失败之前。

## 3. 统计与运行协议

- 延续原重复性公式 `(max-min)/median ≤15%`；不是允许性能退化 15%。保留慢样本、失败组，不追加替代进程。
- 建议每个可比较单元运行 **3 个预声明窗口，每窗 3 对独立 A/B 进程，共 9 对**。
  窗内顺序固定 AB/BA/AB，下一窗 BA/AB/BA，第三窗 AB/BA/AB；不同单元的顺序也在登记时冻结。
  窗间至少从上一窗口全部完成后等待 600 秒。每个窗口同一实现的三进程及全部九进程均检查 15%；
  前两窗合并六进程也单独报告并检查，保留原两窗口门禁。第 3 窗是方案新增，不允许按前两窗结果取消或替换。
- 无共同 A 指标的 B-only 单元每窗运行 3 个独立 B 进程。额外的 GPU/内存探针单独进程，不能把其时间混入无插桩正式时间。
- 每进程先完成 30 秒机器热身，再完成独立 fresh-session 预热：固定 trace 沿用 64 steps；
  稳态流式负载预热 10 秒。pilot 只用于一次性确定批长、固定轨迹、容量可执行性，不进入正式结果。
  正式 CPU owner 使用原物理核心拓扑策略；宿主/GPU 默认允许 CPU 集合，禁止根据测得快慢挑核。
- P0 沿用每进程 8 个固定 trace/read 块；P1–P4/P6 的稳态段每进程至少 60 秒、至少 10,000 次更新，
  且至少覆盖该 profile 的 10 倍 observation retention 和 10 倍最大交付容量发布数，取较长者。
  必须预先冻结实际运行次数/时长及 EOF 处理；不能循环自然结束的谱面而不登记 Reset 成本。
- P5 每进程固定 20 次 Pause、20 次 Resume、10 次暂停 Reset、10 次运行 Reset、10 次 resize 和 1 次 Close；
  每个 Reset 后再次进入同一固定前置状态。冷启动每进程仅 1 次，文件缓存状态记录为未控制，
  不称 disk-cold；音乐完整 129 秒源播放与控制压力分开登记。
- 每进程报告 median、p95（n≥200）、p99（n≥1000）、max、n；不足记不可判定，不能靠跨进程合并制造独立样本。
  P5/启动主要使用 median/max，少量控制样本不宣称 p95/p99。正式重复性使用 P0 块中位数、
  流式每进程中位数，宿主端到端均值另受 15% 门禁；尾部按独立预算判定。
- A/B 收益以进程对为单位：`r_i=median(B_i)/median(A_i)`，用 log(r_i) 的均值和 Student-t
  双侧 95% 区间报告几何平均比。全组 n=9、df=8，t=2.306004；逐窗 n=3、df=2，t=4.302653。
  不能把内部帧数作为自由度。原始每对比值与非正值拒绝均保留；预登记主指标清单后逐项判定，
  所有必需项通过才接受，不能事后挑某个显著收益作为整组结论。
- 先验证身份、环境和精确语义等价。执行/身份错误立即停止；性能超界在固定组完成后统一判定。
  噪声失控时记 inconclusive，不继续收集到通过；缺失字段、分位数或预算不能算通过。

## 4. 已接受的本机阈值

下面全部为 **local-reference-v1 / Release 已接受值**。选择 240 Hz 的参考周期为 1000/240≈4.167 ms；
D2/D3/排队的分配是工程预算建议，不从现有 FPS 或旧 CPU 数据反推。各分位数不能相加替代端到端实测。
限于已登记 workload，不能推广为整个 SDK 的实时保证。

| 指标 | 建议门槛 | 计时边界与依据 |
| --- | --- | --- |
| 共同 CPU 工作的回归 | B/A 中位成本允许上限 1.10 | 全组比值区间上界 ≤1.10 为通过，下界 >1.10 为超界，跨线为 inconclusive；10% 是本机已接受容忍线 |
| D2 Judgement/Fold | 每进程 p99 ≤1 ms | worker 开始逻辑执行→判定/发布可消费结果；嵌套细项不重复求和 |
| D3 Projection/Runtime | 每进程 p99 ≤2 ms | 消费逻辑结果→提交 Runtime 当前状态，不含独立 Snapshot 物化 |
| 单次 Snapshot 物化 | 每进程 p99 ≤2 ms | 从已提交修订开始读取→ owning Snapshot ready，F 次单独报告 |
| 消费者/worker 排队 | 每进程 p99 ≤1 ms | 请求入队→worker 开始对应工作；与执行时间分开 |
| request→ready | 每进程 p99 ≤4.167 ms | 实际公共端到端测量，包含本轮必要更新/物化/等待；不靠阶段预算相加认定 |
| 真实 GPU render | 每进程 p99 ≤4.167 ms | 同 GPU 命令范围 timer query，异步读取；CPU submit、VSync 等待另报 |
| Pause/Resume | 每次完成状态确认与可用画面 ≤50 ms | UI 命令入队→该控制完成且所需保留/新帧实际 present-return；两种控制分别统计 |
| Reset | 每次 ≤1000 ms | 命令入队→新 source/绑定完成且首张有效画面 present-return；暂停/运行两类分开 |
| resize | 每次 ≤50 ms | 尺寸变更请求→新 viewport 有效画面 present-return |
| 初次加载 | 每次 ≤2000 ms | 进程启动→首张有效画面 present-return；包含 decode/prepare/renderer 首用，不是磁盘冷启动保证 |
| Close | 每次 ≤1000 ms | 请求关闭→worker join、设备 owner 完成其可完成的退出；外部合法 lease 仍占用须另报 |
| 适用 Seek | 沿用 ≤20 ms 目标 | 请求 Seek→实际完成所需重建/时钟绑定且目标画面可用，严禁以序列化或局部 Seek 替代 |

阶段边界缺少无侵入计时能力时，D2/D3/物化数据只能先作独立归因探针结果；必须记录探针开销、
覆盖与边界，**不能据插桩时间直接接受无插桩预算**。这些子项维持未判定，直到独立可比较边界/开销证据齐备。
首轮如果不具备正式阶段计时，只能接受已实现的外层指标并明确 PD-5 判定证据仍不完整。

控制预算依据是交互容忍目标：Pause/Resume/resize 50 ms，Reset/Close 1 秒，加载 2 秒；
不是把用户此前约 0.5 秒 Reset 观察当正式基线。FPS 240→130→240 没有持续时间或事件对应关系，
不能证明满足或违反上表。保留操作间隔的真实 frame-time，包括控制期间的空窗，稳态段单独命名。
GPU query 也有观测开销，单独做 observer 校验；VSync present 阻塞不能被记为 GPU render 时间。

## 5. 内存、积压与 owner 拆分判据

不建议本轮凭空接受一个全 SDK MiB 限额。将两种 testOnly 边界分开登记：

- tight-retirement 复用已有 PD-4 小容量测试：journal attempts/feedback=4/4、bytes=16384、
  tombstones=2、coverageIntervals=2，单 renderer、CPU payload peak≤2；其余容量必须从实际配置展开完整写入 manifest。
  这是 B-only 生命周期/压力单元，预期 journal 淘汰可降低 coverage，不能误报为 Gameplay 失败。
- reference-workload 为每个 content 显式给出 CPU payload count/bytes、attempt count/bytes、GPU in-flight count/bytes、
  World/current/candidate/scratch、索引、Ready/history/receipt/journal、retention/deadline、版本 age 上限。
  这些量尚未齐备；填齐、校验可运行并冻结前，该 profile 不能采样。配置总额不得用 0/无限制占位。

无泄漏的判据为：始终不突破已声明的各域上限，撤销/Close 后仍在外部持有的 lease 继续计入 retired live，
释放全部 lease、完成设备 fence 和 owner 回收后，相应 payload/attempt/GPU live 回到 0；
允许保留的 World/cache/journal 则回到独立声明的有界稳态，不要求进程 RSS 归零。
逐项报告 requested C++ bytes、实际 storage charge、进程 working set/commit、GPU 实占，不相互冒充。

拆分触发沿用设计 §9.3：同一 accepted profile 下、**同一个预算项在 3 个连续预声明有效窗口超界**。
绝对延迟窗口统计固定为该窗口三个进程 p99 的中位数；控制类为该窗口全部预声明操作的 max。
相对回归超界要求逐窗配对比 95% 区间下界 >1.10。每个进程仍分别披露预算合格性，
不能因窗口中位数合格隐藏单进程失败。任一强制预算未通过，则该项验收未通过；只有持续窗口条件才触发拆分设计。
噪声未通过、阈值未接受、探针影响未验证均为证据不足。积压还须同时超过已登记 age/count/bytes 上限；
增长趋势仅作调查信号。触发后先归因、优先评估现有 owner 内存储/复用/扫描和等待问题，
形成独立设计报告；owner 接受且另有实施授权才新增 owner/线程。

## 6. 接受记录与执行顺序

2026-10-19 owner 已一次接受以下三项，并授权继续工具/profile 准备：

1. 范围：先做本机 240 Hz reference/testOnly 和 PD 后 RPA-F3 回绑，不宣称完整生产 profile 或整个 Stage 7 关闭。
2. 判据：保留 15% 重复性；共同 CPU 成本允许回归 10%，采用上述配对区间；接受第 4 节的 Release 延迟候选值。
3. 协议：3 窗口×3 对进程、窗间至少 10 分钟；先补 collector、控制事件/GPU 边界、完整 profile 和校验，
   然后生成独立不可变注册；失败保留，不靠重复采样过关，不自动增加线程。

接受方案不等于登记已完成。执行顺序：补齐工具/有限 profile → 语义及统计拒绝门禁 → 一次有限 pilot →
冻结源码与旧/新构建、设备/电源/内容/序列/运行数 → 正式固定窗口 → 报告通过/超界/inconclusive 和拆分判定。
运行前由 owner 确认安静环境、接交流电和稳定音频端点；正式 GPU/audio 采样期间保持应用前台、避免切换打字。
音频设备的人工听感和故障恢复接受继续独立；已有 Codex 切换时 underrun 的用户归因及原始计数均保留。
