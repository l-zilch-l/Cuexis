# ADR 0046: 实时播放的时间、执行与发布边界

状态：proposed；架构规划，未实施

日期：2026-10-09

## 背景和授权

owner 在实际 Player 练习中报告音符随运行变慢，并要求先重审整体架构，再设计时间、所有权、
快照发布及线程模型。随后明确本轮只做规划、不进行产品实施。本 ADR 是该规划的决策输入，
不是 owner API approval、已冻结的新时钟语义或 S7A-7/8 的退出证明。

审查基线和反例见[架构重审报告](../stage_reports/stages/stage-07/readiness/2026-10-09-realtime-architecture-review.md)。
现有 ADR 0027/0032/0042 的产品/依赖边界以及 S7A-2–6 的规则仍有效。Stage 4/5 已提供动画与表现
基础，Stage 6 完成相关产品化边界；其关闭不等于新增 Gameplay 实时协调已经验收。

## 规划决定

1. 先分离五个责任：输入捕获、音频服务、确定性执行、表现采样、设备提交。责任数不等于线程数。
   SDK 不创建隐式音频设备，不把平台线程或 SDL 时间戳送进 kernel；唯一 kernel/Fold 保留。
2. 把共同时间关系、已准入输入、控制边界和已提交结果定义为显式协议。H 不能随绘制次数或按键
   数量增长；音频估计位置、会话 Tick、表现 T、RuntimeFrame 和 finalization frontier 不能混成一项。
3. 实时路径与完整 Replay 验证分离。完整 ReplayEvaluation、fault terminal 比较、Snapshot/Seek
   保留；成本审查同时覆盖 journal、RuntimeState、KernelProjection、公开 query 与回收，不能只改 vector。
4. 音频供给必须脱离 GPU submit/present 的等待链。独立 audio owner 是优先验证的实现方向；
   初始化/析构、ClipStore、命令回执、欠载检测必须一起验证，不把现有对象直接跨线程 move。
5. 保留 SDK 单 owner 契约。判定/Playback worker 是候选，先闭合 renderer 对 PreparedPlayback 的
   同线程消费、跨域资源准备、输入交付封窗和表现采样边界，再冻结线程拓扑。
6. 只允许合并尚未消费的完整表现快照；输入、控制、Fault、Fact/Replay 记录不能采用丢旧取新的策略。
   资源代数与快照代数一致后才可显示。mutation generation、projection scope、音频 segment 不合并。
7. 不以“先跑起来”为由选择新的 canonical 时间来源、同 Tick tie rule、late-policy 数值或生产队列容量。
   真实冲突保留反例与候选修订，在所属合同冻结后才可实施消费者。

## 线程方案比较及准入条件

| 方案 | 能解决什么 | 剩余限制 | 本轮选择 |
| --- | --- | --- | --- |
| 单循环只移走每帧 Replay，并替换测试时钟 | 减少重复计算，消除计帧推进 | 音频 refill、输入 poll 仍受 GPU 阻塞；不能作为完整实时收口 | 仅基线对照，不作为最终方案 |
| 主线程保留事件/Playback/GL，独立 audio owner | 解开音频供给与渲染阻塞 | Gameplay/动画、输入捕获仍可能延迟 | 优先设计验证的第一步，不等于全部解耦 |
| SDL 主线程事件/GL，独立 Playback owner，独立 audio owner | GPU 不直接阻塞判定调度；保留单 owner | Prepared/renderer 准备接口须拆；SDL poll 仍可能受 present 阻塞；动画与判定仍需解开推进频率 | 推荐作为后续原型候选，首用门禁未退出前不冻结 |
| 判定、动画各有 worker，加 audio owner 和主线程 | 隔离动画最坏耗时 | 需要可靠增量事实、资源发布、控制事务；显著增加状态协调 | 仅在上一方案测量仍证明动画阻塞不可接受时选择 |
| 把 SDL/OpenGL renderer 直接搬到 worker | 表面上让主线程专管输入 | 当前 SDL 版本的主线程 API 契约及 Cuexis owner-thread 检查不允许直接这样做 | 当前跨平台方案不采用 |

SDL 3.4.12 的本地头明确标注 PollEvent、GL_MakeCurrent、GL_SwapWindow 为主线程调用；
跨平台输入独立捕获不能靠另一个线程调用 PollEvent。EventWatch 也不是“OS 一发生按键即必达”的保证。
原生输入后端是否必要，应先用事件采集和交付证据判断，不能把用户要求的可靠性转成未经证明的承诺。

## 替代方案和真实合同冲突

“raw timestamp 回溯映射到音频位置”与当前 V2 Spec §3.7.3 的唯一 canonical 来源存在冲突。
推荐优先研究在明确 ingress 捕获已校准会话 Tick、之后交付不重采样的方案；它不能自动解决主线程
尚未捕获事件期间的延迟。若要接受历史设备事件时刻重建，必须另行定义 timestamp 可信度、映射段、
有效区间、identity、canonical 生成时点和迟到处理，并明确 supersede 哪一句旧合同。
本 ADR 不选择隐式回溯，也不修改同 Tick 至多一个 admitted input 的执行 profile。

表现快照是数据边界，线程不是判定语义。独立 worker 不能修复 H 的错误来源，也不能逆转已封存的
Miss。只要输入 admission 轨迹改变，现有 late-policy 允许结果改变；测试应分别验证合法同轨迹等价
和跨封窗稳定拒绝/转发，不能要求任意延迟都产生相同结果。

## 落点与验收

字段职责与TB-01–07未定项（包括暂停期间真实释放与恢复准入）归[实时宿主边界草案](../api/realtime-host-boundary.md)，实施顺序归
[Stage 7-RPA子阶段](../stage_plans/active/stage-07/plan-rpa.md)。
本轮不增删公共符号、依赖或目标，不更改 date build/SDK API。
后续若改 AudioClockSnapshot、公开准备/资源接口或 Playback 方法，必须由实际 API 差异判断版本，
完成 candidate ON/OFF、static/shared、fresh/clean-first、消费者与 owner gate；不能沿用当前 patch 结论。

Stage 7-RPA子阶段负责实时架构、正式命名及实体设备/实时同步最终验收；S7A-7/8保留原集成与门禁收口，
S7A-9负责原内核/消费者矩阵和预算，Stage7B+保留高级能力线，S7C-1负责高级校准参数/设备策略扩展，
Stage 8 消费已验收合同，Stage 12 做大规模/平台优化。基本同步与播放可用性不得延期到 Stage 12。
