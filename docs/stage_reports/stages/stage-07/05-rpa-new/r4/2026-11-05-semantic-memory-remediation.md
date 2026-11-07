# R4 语义内存整改：两轮诊断与 World 下界失败

状态：dated evidence；本批停止，memory/P8 未准入；未修改 O1 预算

日期：2026-11-05

返回 [New RPA 报告索引](../README.md)。当前进度以
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md) 为准；执行范围消费
[R4 计划 §4](../../../../../stage_plans/active/stage-07/05-rpa-new/r4/plan.md#4-独立修复重开与停止规则)。
原两轮 origin 工具诊断和缺口见[O1 后工具准入](2026-11-05-post-o1-tool-admission.md)，
本报告不覆盖其原始记录。

## 1. 授权与结果

owner 选择 A：保持原预算，批准一个语义归属整改批次，最多新增两轮 64/high 诊断，
每轮一个进程、120 秒；不运行 40K/P8，允许结果仍证实超界。
授权及逐轮预登记、原始 stdout/stderr、构建绑定、独立复核和最终处置均归档在
[本批证据附件](2026-11-05-semantic-memory-evidence/index.md)。

**第二轮证明本候选 64/high 工作流的 World 域失败：真实 Playback active 时，
25 个唯一分配的保守下界为 358,241 B，超过原 327,680 B 上限，至少超出 30,561 B。**
尚未遍历的对象只会使完整 World 拥有量增加；此处不声称完整图或分析上界。
两轮新增额度已用完，按预登记停止 memory/P8；正式性能、memory、P8 均未启动，额度未使用。
O1 方案 B 的数值和适用范围未更改。

| 轮次 | 精确源码 | 功能与时间 | 工具/预算结论 |
| --- | --- | --- | --- |
| 新批第 1 轮 | `3aec896211bbfe003abd3ec79f9737ec95684b3c` | 1 进程、0.0875162 秒、21 assertions 通过 | tool-invalid；精确分配起点匹配不能识别 MSVC allocator 内部对齐地址；原件保留，不裁定产品 |
| 新批第 2 轮 | `5fd1bd3c90ebba767e2b6ac54375d63f209c2504` | 1 进程、0.0940668 秒、21 assertions 通过 | 有效；19 个 coherent checkpoints；active World 下界超界；未补跑 |

两轮时长仅为有界功能/覆盖诊断的进程墙钟时间，不是经环境准入的正式性能读数。

## 2. 从 origin 到可证明的拥有子集

原 collector 的 allocation-origin 仍保留，不重命名、搬域或改写旧失败。本批新增
noninstalled semantic bridge，默认与历史观察模式编译为空；没有 installed public API、
产品 allocator、线程、恢复协议或配置预算变更。

静态 root 表使用非 owning World identity、独立 generation 和分配 ID。同一观察器锁内维护
分配、free、去重和 phase；构造失败也退入 retiring，最后真实 free 后才标 released。
root 槽终生最多 256 个，不复用；root/分配 ID 溢出、缺页、范围错误和表耗尽均 sticky invalid。

窄 scope 只捕获 registry 构造存储、实体/纯值组件存储。World 对象本体、成员初始化前的分配、
ObjectEntityMap、未访问容器与 transform scratch 不纳入下界。Appearance/Camera emplace
排除嵌套字符串，因为材质字符串可与 RuntimeEvaluationState 交换；派生 transform 页也可能
交换进事务临时存储。因此这些页不永久 adopt，而是在实际持有时做同步只读地址 census。
不增加 shared_ptr，不改变 COW 引用计数，不提前释放合法结果句柄。

Runtime commit 只标 runtime-installed，不能冒充 Playback 激活。本批在真正
PlaybackSession::commit 完成状态转移、generation/presentation 发布和 PreparedPlayback
清理后再标 active 并 census。prepared、runtime-installed、active 保留同一 root。

首轮暴露 MSVC 14.51 标准 allocator 的大块对齐：EnTT 按页请求 payload，STL 向
operator new 请求 payload 加私有对齐空间，再返回内部地址。第二轮修复使用完整 payload
span 必须位于唯一 live new 分配内的检查，以减法避免边界加法溢出；按 ID 和 offset 求
区间并集。稳定 root 已计完整块时不会再次加页，重复/重叠别名只计一次。

计费口径为：

| 项 | active 第 3 次 census | 用途 |
| --- | --- | --- |
| 稳定 registry 子集，22 个块 | 145,249 B | 已知持有者的实际 new requested bytes |
| 三个实际组件页 payload | 65,536 + 81,920 + 65,536 B | 仅 payload 区间并集，排除 STL 对齐前后缀 |
| 唯一语义下界，25 个块 | **358,241 B** | 与 state.world 原 requested ceiling 比较 |
| 对应完整 backing new 请求 | 358,358 B | 独立披露，不当作 payload 下界 |
| 对应 CRT usable storage | 360,158 B | 独立披露，不当作 requested bytes |

全局产品账和 World 子集有重叠，不能相加。scope-origin sticky peak 仍可包含构造临时量，
不能用来判 persistent World 通过或失败；本次失败只使用真实 active 瞬间的唯一拥有子集。

## 3. 验证、开销与局限

第二轮全日志 19 个 checkpoint 无 observer overflow，origin/global partition 精确。
World root 在 Close 后真实 released，子集 live/requested/storage 均为零；最后释放所有原有
合法内容、结果与 fixture 后，产品 C++ requested/storage 为零。root released 只证明跟踪
子集释放；不能代替完整拥有图、native/GPU/driver 或设备 fence/join 的独立门。

观察器 v2 header 为 64 B，较此前扩展版本增加 8 B 分配 ID；历史 collector 保持 56 B。
该额外量在观察器开销中列出，不计进 payload 下界。root/table static 为观察器独立存储，
本轮日志合计 static 51,776 B；checkpoint 新增局部表 227,328 B；census 局部表 208,896 B，
另有调用侧固定页表。没有通过 reference 扰动、量程或设备开销准入，不能进行正式内存/P8。

验证对象为源码 `5fd1bd3c`：

- 37 项 Python 工具测试通过，包括完整日志 sticky-invalid 拒绝、独立 span union 重算、
  越界/孤立/重复记录拒绝和下界只能证明失败。
- C++ 正向契约：4 cases / 26 assertions；独立 unknown-root、root-capacity、page-overrun
  负向契约各 1 case / 1 assertion。实际 MSVC vector 内部对齐地址和重叠别名均被验证。
- fresh ordinary MSVC Debug STATIC candidate ON：World 29、Runtime 54、Playback 185
  用例，合计 268 cases / 46,139 assertions，通过；全部 observation/trace/metrics 关闭。
- 架构 CTest、格式、版本一致性、文档及 diff 检查通过。两轴复核独立核对源码、hash 与原始日志，
  未启动额外 Cuexis 进程；均支持本次限定失败。

这些是本批目标回归，不是新源码的完整 11 行 F1/F2。原候选 `22a50762` 的 Linux Quality、
Windows MSVC、Windows MinGW hosted 已同 SHA 通过，网络 504 原始失败与 failed-only 重试保留。
本地仅 MSVC ON Debug、Release 两行完整；Shared Debug 的 1402 tests/0 failure 可确认，
但其预算重复检查中断且没有 source_after，不能依原 runner 提前写入的 local_pass 算完整行。
这些旧候选结果不能授予新源码资格；后继报告 SHA 也不自动继承例外。

完整 World/current/candidate/scratch、共享 retained、prepare-only、合法 owning handle 图、
跨域同时峰值、native/GPU/driver、观察器 reference 开销仍未闭合。本次不得写内存通过、
全语义准入、生产接受、P8 通过或 R4/Stage 7 退出。

## 4. 后续裁定方案

本批的授权工作已经完成；以下均是待裁定范围，不能把再次运行视为已获准。
预算失败的保留本身不要求改变预算，也不阻止无依赖工具/正确性工作。

| 方案 | 具体安排 | 优点 | 成本与限制 |
| --- | --- | --- | --- |
| A：保持 O1，单列产品存储整改（推荐） | 先以组件页固定容量与实际分配关系做设计，审查能否用组件级分页/稀疏存储策略降低小规模 World 固定成本；同时明确完整语义归属的剩余覆盖。设计冻结后另行登记有限诊断额度；仍不跑 40K/P8，直到各自准入 | 保持原判据，解决已证实的产品成本；不靠重标域消除失败 | 属新增存储/性能机制，不能由本批授权自动实施；会影响 2048/40K 分配次数和吞吐，需独立正确性、兼容和后续正式验证；省下 30,561 B 也不保证完整图达标 |
| B：保留失败，暂停 memory/P8 | 不追加工具诊断，不改产品存储/预算；将 World 失败及未覆盖域写入限制清单，继续其他准入和证据，O3 时由 owner 明确决定退出范围与责任 | 最少新增实验，失败和边界清楚 | 内存/P8 不能通过，完整 R4 接受仍有明确缺口；不能由代理自行签作退出 |
| C：另开适用规模/预算评审 | 若产品目标确需变化，先给出需求、容量和可接受退化，再独立修订预算/能力范围；旧 O1 和本次失败完整保留 | 让目标与实际能力重新对齐 | 工作范围最大；358,241 B 只是下界，不能据它选新 cap，也不能把更高数值作为本轮通过；新方案仍需技术准入 |

推荐 A，因为当前已有真实产品成本证据；具体分页参数须从声明容量、拥有关系及同时存活规则
推导，不能从本轮高水位反填。若优先尽快形成带限制的交付，选择 B 更合适。
在 owner 裁定前，memory/P8 保持停止，原 O1 不动；不创建 PR、合并或发行。
