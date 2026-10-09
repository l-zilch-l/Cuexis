# 实时宿主边界设计草案

状态：candidate design；未冻结字段、未实施，不是现有公共 API

更新日期：2026-10-09

## 权威和范围

决策方向归 [ADR 0046](../adr/0046-realtime-playback-coordination.md)，工作与门禁归
[Stage 7-RPA子阶段计划](../stage_plans/active/stage-07/plan-rpa.md)。
本文记录需要冻结的宿主协议，不改变现有头文件、序列化或预算。旧 V2 Spec/ABI、execution profile、
Playback lifecycle、Portable Presentation 与 ADR 0032 的已定规则继续有效。

## 1. 时间职责和 first-use 字段清单

下表是字段设计要求，不是可直接编译的 struct。类型的最终拼写、编码、舍入、越界码及新增 identity
投影须通过本表后的问题表后，落回对应权威合同。没有默认生产数值。

| 载荷/责任 | 必需信息 | owner、寿命与身份 | 失败/替换边界 |
| --- | --- | --- | --- |
| Source clock observation | 时钟域 ID、单调采样时刻、source frame 与 rate、有无音轨、播放状态、segment、位置估计的有效性/误差信息 | audio/host adapter 发布一致快照；读取者不得拼接不同更新；硬件 ID 不直接进判定四分量 | 欠载、rate/设备变化与过期快照必须可区分；缺相关信息不得伪称精确硬件游标 |
| Calibrated session clock | 源位置与会话整数 Tick 的锚点、单位、offset、有效区间、profile/revision、运行 segment | 显式 prepare/session 配置；影响 canonical Tick 的有效规则归既有 timebase/calibration identity；运行 generation 本身不入 identity | checked 有理数转换；只在规定点舍入；禁止 RuntimeFrame f64 反推；新 segment 不能伪装同段回退 |
| Captured input | 入管线时捕获的 canonical observationTick、sequence、映射声明、归属 segment、原始 provenance；captured/delivered/admitted 分开记录 | 捕获处确定 Tick，消费者不能取队列时重采样；canonical Reader 使用同实际校验路径 | 旧段、重复、倒序、同 Tick 碰撞按既有规则；不偷偷加 Tick、不静默丢 release |
| Input delivery progress | 哪些捕获记录已经交付/准入、哪些尚未交付、progress 声明的时钟和证据 | 宿主调度责任；不是 kernel finalizationWatermark，也不是 lastObservedTick | 不知道前缀完整性时不能声明所有旧输入已到；超出合同窗口走既有 late-policy；不改封存 Fact |
| Advance request | typed H、明确的源 observation、与本次输入/控制顺序的关系 | session owner 串行准入；H 不从绘制数或事件数计算 | exact H 仍按既有 F(H) 执行；不能把 F(H) 混作音频帧/Beat 换算；future pending 保留 |
| Presentation sample request | typed T、RuntimeFrame、viewport、消费的已提交结果位置、projection scope | 表现 owner；T 的映射显式，不要求与 H 同值；RuntimeFrame 继续表现浮点合同 | 不使用 watermark 当表现位置；faulted 只读查询不触发表现采样；失败 Tick 无 token |
| Published frame envelope | owning FrameSnapshot、sample time、内容/资源代数、projection scope、已应用控制序号与结果前缀 | 生产者构造后只读发布；消费者不能让生产者复用仍被读的缓冲区 | 只接受已激活资源代数；Seek/reload 后拒绝旧帧；旧帧归还后才能复用/释放 |
| Control request/receipt | 操作 ID、期望会话/资源/时钟段、显式目标与成功/拒绝/不可恢复阶段 | coordinator 串行排序，各 owner 确认自身部分；重试依据回执 | 排队不等于生效；无回执超时不等于取消；Pause/Seek 不可被旧 frame 覆盖 |

source observation 用于建立明确的校准会话时钟，不意味着 raw 音频时间可以直接作为 observationTick。
“在 ingress 捕获当前校准时钟”和“回溯设备事件时间再生成 Tick”是不同语义，不能混用。
输入到達比 event timestamp 晚不证明该 timestamp 真实、精确或与音频同源。

## 2. 必须在实施前裁定的时间问题

| ID | 最小反例 | 推荐设计方向/需要补齐的合同 |
| --- | --- | --- |
| TB-01 捕获时点 | t=100ms 的键在500ms才被应用获取；当前时间捕获与历史重建给出不同 Hit/Miss | 优先维持唯一校准会话时钟；定义最早可靠 ingress。若需历史重建，显式修订 V2 Spec §3.7.3/ABI，不以普通适配名义采用 |
| TB-02 封窗顺序 | worker 已 advance 至越过输入 t 的封窗边界，主线程才交付它 | 定义输入交付进度与推进许可；既有 window/late-forward 规则保持。不得无条件等所有未来输入，也不能追溯修改 sealed Miss |
| TB-03 同 Tick | 两个真实 press 量化到同一微秒；按 poll 顺序加1改变窗口/接触时间 | 保留当前执行 profile 拒绝；若产品必须支持这种输入，登记能力/规则修订及全量影响，不由 adapter 改写 |
| TB-04 估计和欠载 | postmix 在输出静音时继续计数；旧 published snapshot 看起来仍为 Playing | 区分 device mixed/source consumed/presented estimate；同步 capture time、有效区间和质量；欠载段与恢复策略先有证据 |
| TB-05 offset 与舍入 | source 44.1kHz、device48kHz、非零chart/input/output offset | 分清源帧/设备帧率，有理数换算；输出延迟不能在 presented estimate 和校准中重复扣；负Tick/溢出规则保留 |
| TB-06 非音频和结束 | ChartClock pause期间宿主继续计时；音轨结束但尾判timer尚未到finalization | 无音频显式 monotonic host clock；暂停不累加；不得静默从失效音频fallback；音轨结束与Gameplay complete分别定义 |

| TB-07 暂停期间释放 | 按住K→Pause→暂停时松K→Resume；held被保留但release未进入执行，会与真实设备状态分离 | 保留现有暂停保持held规则；候选A由宿主保留真实转换及provenance、在显式恢复边界按待裁定时间/准入规则交付；候选B检测不一致后拒绝直接恢复并要求显式重建。A需补Spec/ABI的捕获与准入时点、重复/同Tick/迟到规则，B需补Playback控制回执与恢复策略；均需明确焦点变化、积压与诊断，不伪造release、不默认丢弃。两候选尚未选择 |

TB-01/02/03 未闭合前，不承诺任意卡顿中真实键盘结果与无卡顿完全相同。本轮不替换旧 canonical source、
同 Tick 规则或接受新的 late window 数值。100ms/500ms 是未来故障注入场景，不是被接受的延迟容忍阈值。

## 3. 所有权与资源发布

Session、非空 PreparedPlayback、Runtime/World 继续唯一 owner。锁住对象不产生跨线程调用许可。
现有 IPresentationRenderer::prepare 接收 PreparedPlayback 引用，不能直接跨域调用。

若选择 Playback worker，须先设计 owner 内资源 acquisition 与 renderer 内 GPU preparation 的分离：

1. 在 Session owner 完成 prepare/validate/acquire，捕获 owning portable resources、manifest 与候选凭据。
   不能把 borrowed span、MainMusicSourceView、ContentProvider 回调或 PreparedPlayback 引用塞入队列。
2. renderer owner 对 owning resource bundle 准备 GPU candidate，回执绑定内容、资源代数、renderer
   instance/generation 和候选凭据。资源未就绪时不得采纳新 frame。
3. coordinator 在承诺提交前验证所有回执并建立不再被普通 advance 使其失效的提交边界；不能假定
   后台持续推进时 Prepared generation 仍有效。失败取消候选，保留旧组合；物理设备不可逆错误单列。
4. 提交与取消只在对应 owner 执行；stale/out-of-order receipt 不激活资源。旧资源须活到最后一位读者
   与设备使用结束；GPU 资源仍在 renderer owner 回收。

这是设计要求，不是新的 cross-thread Prepared API。具体 DTO、token/回执种类、CPU资源析构线程与
无分配提交证明尚需 typed review；本轮不增加公共字段。优先保持 SDK 单 owner，以应用协调完成并发。

## 4. 调度、快照和队列

- 音频 callback 不做 Gameplay、Replay、资源加载或日志IO。音频 owner 服务不等待渲染完成。
- Gameplay scheduler 只处理已准入输入/到期工作。动画求值和绘制频率不定义 H；它们也不得改变
  已封存判定。推进频率改变的等价性只在现有合同允许的相同 admission/control 轨迹下比较。
- 动画/Behavior 使用现有求值器、resolver 和绝对时间；不在 renderer 建第二条动画求值路径。
  当前组合 advance 对外仍保持既有兼容行为，未来拆分方法必须是显式接口与可审计发布阶段。
- 完整 frame 是 latest-value 消费候选；FactBinding 的cursor/去重/未来队列/聚合/token必须在表现 owner
  完整更新，不能只保留“最后一个Fact”。保持 RemainingFrames 的既有语义，不把其计数偷偷改成GPU呈现次数。
- 输入、控制、错误是可靠有序通道。队列满不能丢键或堵塞音频 callback；拒绝/暂停/终止哪种处置须
  在 typed review 确定，并先登记稳定诊断。容量、积压/延迟上限无生产默认，仅允许显式 testOnly 参数。
- 无音频、Headless、Reference Host 可由调用者显式驱动相同协议；不要求 SDK 内建线程。

## 5. 控制和失败矩阵

| 事件 | 必须保证 | 不可声称 |
| --- | --- | --- |
| Pause/Resume | 回执说明音频/判定停住的位置；保持held contact，不造release；暂停期间真实release与恢复准入待TB-07裁定；旧帧不能越过已生效控制 | 两条异步消息发出时就已同时暂停 |
| Seek/Stop/reload | 全体候选就绪再切换段/资源/作用域；游标、去重、pending表现和token整体替换 | 同H或同长度足以沿用旧投影 |
| Fold fault | 允许保留已封存Fact，失败Tick不发token，faulted只读query不改画面 | 为追音频继续动画或发布半个Tick |
| 表现发布失败 | 保留已执行Gameplay和旧画面；回执分离Result/Projection/Runtime失败；重试不重复submit | 画面旧就代表输入没有准入 |
| 欠载/设备失效 | 时钟有效性和恢复段显式可见；不能默认把静音帧当歌曲进度 | 任意设备故障仍可精确回滚且无误差 |
| 正常音轨结束 | 明确尾部timer/late window结束条件和Replay最终horizon | 音轨结束即所有判定都已finalize |
| shutdown | 停止接收、确认/取消在途命令、隔离callback、在各owner清理资源、join后释放共享数据 | 锁住队列或detach线程就完成清理 |

## 6. 存储与验证职责

执行热状态、历史日志、恢复快照、公开只读结果应分别计量所有权/分配/复制/回收成本。
分块或结构共享只是候选实现；必须保留旧owning结果寿命、故障预留不分配、完整ReplayEvaluation、
archive/cut/H checkpoint绑定、future pending/lastObservedTick和惰性分支。
不能用hash或score摘要替代完整结果，也不能为了节省日志而删除影响晚到/控制的advance记录。

完整Replay验证拟放在结束/显式命令/测试路径，必要时在拥有独立归档的评估任务运行，不能并发调用
活动Session的owner-bound方法。失败和用户提前退出也要记录实际可取得的完整结果及验证是否执行。
是否使用后台评估及峰值内存上限须测量后决定，不把工作搬线程就称作复杂度改善。

## 7. 冻结前证据

需要同输入/admission/control轨迹、多种渲染节奏、暂停/Seek/跨代旧帧、欠载/设备变化、队列满、
分配失败和线程关闭反例；逐项比完整kernel/Fold/Replay结果及表现发布边界。
对刻意改变交付/封窗的场景验证既定迟到处置，不要求本应不同的结果相同。
独立oracle不调用生产推进函数；基线探针测性能而不是oracle。真实事件时刻与听音精度另需设备证据。
