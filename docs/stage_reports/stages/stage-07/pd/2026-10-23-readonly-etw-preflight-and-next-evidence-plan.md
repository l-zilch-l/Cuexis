# PD-5 只读管理员预检与下一证据方案

状态：本批工具单元验证与方案复核完成；实际 ETW capture、性能候选登记和 PD-5 acceptance 未完成。

更新日期：2026-10-23，Asia/Shanghai。前序见
[ETW 失败与手动清理](2026-10-23-etw-wrapper-failure-and-repair.md)及
[小规模寿命与读取噪声诊断](2026-10-23-memory-lifetime-and-read-noise-diagnostics.md)。
本报告记录本批证据，不修改已接受预算或三窗协议。

## 范围与实际改动

本批从隔离树 `e6959a9de51bada2b819c9cd8f235f6291955725` 继续，主树 F1 仍为
`eed36648a692333e47f798aa45be36ca850d08de`。没有改产品源码、旧准备、旧采样、原 40k
invocation 或旧 ZIP；新工具/测试仅写新 out 目录，仓库提交只含报告、索引与当前状态。
三个无上下文 gpt-6-luna 子代理分别分析读取候选、内存方案和独立复审 ETW 工具；没有
执行 Cuexis native、构建、性能样本、正式注册或管理员 ETW 操作。

## 可运行交付只含只读预检

新准备位于 `out/pd5-f1-noise-control-v3-20261023-e6959a9`。PowerShell 默认入口只接受
只读预检；`-Capture` 直接拒绝，Python 交付 CLI 不提供 capture 参数，独立控制器草案的
命令行入口也无条件 HOLD。plan 中 capture_enabled=false。

预检固定 Python/WPR 绝对路径和 hash，校验 9 个 artifact 与 15 个输入；先检查 64-bit
Python 和管理员 token，再检查 profile privilege、直接 ETW inventory、默认 WPR status
及 Power profile。它不执行 WPR start/stop/cancel、不启动 F1、不改变电源策略，不自动提权。
新 JSON receipt 采用完整写入后原子发布，避免把半写入状态误读为完整声明。

普通权限下只执行一次真实预检，exit 2、NOT RUN；WPR start/native launch 均为 false。
管理员 QueryAllTracesW 的实际可用性尚未执行验证，须由 owner 运行一次只读入口后独立核验。
QueryAllTraces 只返回调用方可查询的会话，并不枚举 private logging sessions，因此
不能把任意非管理员的空列表写成全局 idle；见
[QueryAllTracesW](https://learn.microsoft.com/en-us/windows/win32/api/evntrace/nf-evntrace-queryalltracesw)。

## 已验证的工具组件与尚未关闭的清理问题

新增的工具 Job 包装器先把等待握手的子进程加入 kill-on-close Job，再允许它启动命令。
正常结束、超时或取消时同步回收，查询 Job active-process count 为 0 后才返回。
这修复了前版 collector 仅 Stop-Process 后立即进入清理的缺口。

14 个工具单元测试通过：x64 ETW 结构 ABI、精确名称/路径归属、错误 nonce/路径拒绝、
查询失败不作 idle、stop code=0 但会话仍在时不作清理完成、原子 JSON 不覆盖、非管理员
拦截、capture 入口拒绝、只读命令白名单、普通 exit 0/23、超时、取消、控制器被杀后普通
Python 子孙退出，以及模拟看护进程对父进程死亡的处理。所有子进程均为普通 Python，
ETW 用假 backend；未执行真实 ETW stop 或 Cuexis 性能例。

首轮测试发生过 Windows 临时日志删除失败；改为保留后续工具测试目录，不以删除日志作为
回收判据。回收判据仍为实际 Job active count 和控制器死亡后的子孙 process handle 状态。
另一个测试夹具缺少假 plan/binding，已在隔离单元中显式注入假 hash，未绕过实际入口校验。

控制器/看护草案仍有独立复审未决项：紧急停止的重试/截止与异常路径；WPR 正常合并与
直接 ETW stop 的唯一清理权；Job 回收失败时的后续动作；完整 420 秒内的启动、回收、停止
与证明预算；WPR 自身 affinity 与测量 affinity 的区别；空 logfile 时的归属证明。
这些没有由 mock 测试消除。**只读预检可交付不等于实际采集可以恢复。**

## 单一读取候选的复核

Luna 提出一个有界 ordered visitor：在既有 owner/window 内按布局索引顺序读取，每项
立即交给唯一物化器处理，首错即停；最多 64 项栈索引，不保存完整中性对象数组或持久缓存。
主代理核对实际 Runtime/Playback 均使用默认 core::Error 的 core::Result，因此 callback
不需要引入 Runtime 对 Playback 的依赖或新错误转换；栈 context/trampoline 可沿用既有
PresentationAccess::read 形态。较早资源错误仍必须先于后续索引错误，不能预验证整批。

这仍是低把握设计：每对象的直接 Runtime 调用被间接 Playback visitor 调用替代，并未消除
每对象跨模块调用；Result 传播和组件读取仍在。历史 caller-storage 批处理更慢，不能因
接口名为 batch 就声称有收益。本批没有实施、登记或计时该候选，也没有追加第二候选。
后续先做隔离语义/代码生成验证，只有值得保留时才按原自检协议登记一个性能候选。

设计材料位于 `out/pd5-next-read-design-20261023-e6959a9`。主代理核验其 14 个 source
binding，发现其中一项候选文档 SHA 被手抄成 63 字符，原 binding 不修改；实际完整 hash
和源文件字节另存在本批 source-check/snapshot。该项不能报告为原绑定通过。

## 下一内存证据方案

Luna 的 `out/pd5-memory-next-plan-20261023-e6959a9` 方案以 17 项源文件和既有 raw logs
为依据，主代理复算 SHA 全部一致。连同读取设计，本批复算 31 项来源：30 项原 hash 一致，
一项上述手抄错误；全部实际 source bytes 单独快照，不更改历史审计文件。

保留七项 Packed 物理限制、四项 hash ceilings 和 reply deadline30秒。现有 40k global
observer 的超包络不能直接分配为某个 owner 超预算，也不能由小规模最终 aggregate
相等接受 40k。下一方案区分 allocation 原始归属、外部 owning 值、shared backing 的唯一
计费、phase-local peak、累计 peak、retired live、设备 fence 和最终释放点。

主代理复核的实施前约束：主线程 phase 标签不能代替 worker 内真实存储归属；新增 observer
header/tag/id 自身开销必须显式记录；未归因桶必须保留并与 aggregate 对账；共享引用不能
重复计费；缺失的 reference-workload 每域有限额度必须先列全，不能填 0 或无限制。
该方案尚未插桩、构建或采样；没有自动登记或运行新的 40k。

## 验证、保护与下一步

证据根为 `out/pd5-continuation-20261023-e6959a9`，保存工具测试日志、来源逐项核验与快照、
独立工具复审、只读入口绑定、保护核验及封包回执。旧 7,027,556,352-byte 失败 trace 保留在
原目录，只引用既有清理 receipt，不复制或删除它；它没有 F1 measured PID，不作性能证据。

文档执行 `python -B tools/check_docs.py` 与 `git diff --check`。原主树 F1、C2、old golden
及 Stage C frozen inputs 继续保护。没有新性能样本、三窗注册、旧窗口3续跑、merge/push，
没有修改 1.10/15% 或公开停止接口。下一步先消费管理员只读预检结果并完成清理设计复审，
再决定新采集准备；读取候选与内存插桩另按上述窄范围推进。Stage 7-PD 尚未退出。
