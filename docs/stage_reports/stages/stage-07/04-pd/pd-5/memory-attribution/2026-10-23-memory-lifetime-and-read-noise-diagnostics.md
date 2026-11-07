# PD-5 小规模寿命诊断与读取噪声独立复算

状态：completed（本批有界诊断与工具准备）；PD-5 acceptance：not passed。

快照日期及更新日期：2026-10-23，Asia/Shanghai。当前摘要见
[CURRENT_STATUS](../../../../../../CURRENT_STATUS.md)，完整退出缺口继续见
[Stage C 独立验收报告](../admission/stage-c/2026-10-23-stage-c-independent-acceptance-and-exit-gaps.md)。
本报告不修改已接受的预算、统计协议或历史运行结果。

## 1. 授权、分工与证据身份

owner 要求继续处理剩余阻塞，并结合无上下文 Luna 子代理。两名新建 gpt-6-luna 子代理分别
只读复算已有 F1/C1/C2 数据、追踪内存所有权；产物随后由主代理从原始数据与源码独立核对。
主代理设计、编译和执行一次预声明的小规模寿命诊断；另一子代理只读复审该方法及产物。
子代理没有启动 native、构建、性能候选、正式窗口或修改产品源码。

| 身份 | 本轮值 |
| --- | --- |
| 主树 F1 | `D:/Cuexis-worktree`，`stage-7`，`eed36648a692333e47f798aa45be36ca850d08de`，保持干净 |
| 隔离树起点 | `733ec8e685a75d527eac1ef9986278a9482f2098`，`codex/pd5-gameplay-40k-admission` |
| 既有产品编译输入 | native source `54e8c2a1d770a90e1d7a2728f00510b1b19aabaf` 的冻结 STATIC 对象/库；只读复用，无旧构建重编译 |
| 新探针目录 | `out/pd5-memory-lifetime-20261023-733ec8e` |
| 新探针身份 | 原 native source 归档 + 唯一测试 header diff；不是一个新的干净产品源码提交 |
| harness.diff SHA-256 | `d85390233176b7c7a41a56167ab98b45cdc7ef7a7e562966e5f48012ec869c13` |
| 新 EXE SHA-256 | `6ae2ae07204458ed4fda89a305b7961e7da06cb39b2acb7a307a1dd763ec3470` |

新二进制只重新编译 derived `pd5_matrix_probe.cpp` 翻译单元，单独链接新路径。
复用对象/库的实际 SHA 与原 frozen inventory 对照；compile command、link command、
sourceDependencies 内全部实际头文件、EXE/object、plan 和脚本摘要均在新目录登记。
主代理逐文件核验派生 source 仅 `pd5_capacity_probe.hpp` 不同；未扩大公开 API、改 Runtime/
Judgement/Gameplay 产品实现或合入 C2。编译/链接均 exit 0，没有 CMake post-build 写旧目录。

独立复审核对 32 项复用对象/库和 373 项 compile dependencies，其中 41 个 live checkout
头文件与原 archive 对应字节一致。另发现两项 provenance 缺口：运行时 mesh/material assets
及 Python validator 未写入逐 case binding；本轮事后核对均与原 archive 一致，但没有逐 case
的独立快照。linker/SDK 默认系统库也没有单独版本 hash。补充核验单独记入 post-run addendum，
不修改已经执行的 plan/runner 或追溯称其采样前绑定齐全；后续新采集工具须补齐这些输入。

## 2. 修正静态所有权清单

Luna 审计发现前轮持有表遗漏了 `GameplayAdvanceDecision::result`。主代理核对
`execution_coordination.hpp`、`publicDecision` 和 `GameplayAccess::result` 后确认：
advance 返回的 `decision`、query 返回的 `expected`、Replay evaluation 的 `evaluated.result`
都持有 owning GameplayResult；结果 storage 拷贝 KernelProjection 的值向量，不能把三个
wrapper 误当同一份只读 backing。投影中的 runScope 等子对象仍可能共享，不应简单三倍求和。

其他已确认的结构性峰值来源包括：fixture、wire 与 decoded PreparedCapsule 同时存活；
ExecutionKernel 为故障保留复制的 RuntimeState；Snapshot capture 的 DTO/历史复制；
独立编码 vector 与 decode DTO；decode 内部恢复校验的临时 kernel；Seek 内部 Replay 编码。
这些是源码证明的复制/存活路径，尚不是各域精确字节数或已定位的生产缺陷。

decode 的内部 restore 校验也不等于将 decodedSnapshot 安装到活动 session；本批没有增加
该证明。既有 40k 全局峰值不能据这些静态路径直接归为某一个 owner 超预算。

观察器记录位于当前存活 new 块的 intrusive Header。delete 会移除节点并 free 对应 base；
只保留标量累计峰值、sticky exceeded 和固定 128 项结构高水位，不保存所有已释放块的历史节点。
因此不能用“观察器保留了整个分配历史”解释 after-close 的 live 数字。

静态表、源文件 SHA 与依据保存在
`out/pd5-memory-ownership-audit-20261023-733ec8e/{findings.md,source-binding.json}`。

## 3. 固定小规模寿命诊断

在任何 native 前写入 plan、源码差异、命令、对象/库身份和 EXE 绑定。固定顺序为
64-low、64-high、2048-low、2048-high，各一个新进程；首个执行、身份、功能或释放残留错误
即停止，不替换、不重试。外部 ceiling 为每侧 900 秒，原 reply deadline=30 秒不变。
它是寿命诊断，不是性能窗口、正式 preparation 或 registration。

新 selector 为 `PD5 scoped memory lifetime audit`；执行前必须显式 audit=1、revision=4，
规模恰为 64 或 2048，拒绝原默认 40000 和 advance-only 分支。每侧使用独立新 evidence 目录，
R4 编码只向该新目录写 input.r4.packed。原 fixture 构造文本完全一致。

原例全部操作和结果断言保持，原 24 阶段/48 事件完整；先输出原 summary，再依次释放：
decision；expected/evaluated；两个 Snapshot 与编码 buffer；Replay；content/packed/fixture。
vector 使用真正销毁 buffer 的 swap，非 clear。最后退出内层作用域，令两份 prototype、
reuse 字符串和其他局部也销毁，再与预热输出后的入口基线比较。
新增 checkpoint 在现有阶段之间读取全局 observer，不产生公共停止接口或新 owner。

| 配置 | native / stderr / 功能断言 | 进程累计 peak requested bytes | 销毁全部局部后的 live 基线 |
| --- | --- | ---: | --- |
| 64 low | exit 0 / 空 / 45 assertions、1 case | 33,823,103 | 211 allocations / 14,904 requested / 28,408 CRT storage bytes，精确回到入口 |
| 64 high | exit 0 / 空 / 45 assertions、1 case | 33,823,085 | 同上 |
| 2048 low | exit 0 / 空 / 45 assertions、1 case | 180,457,000 | 同上 |
| 2048 high | exit 0 / 空 / 45 assertions、1 case | 180,056,342 | 同上 |

四侧原顺序一次完成，三个内存维度均精确相等，并未用容差或抬上限接受。
本次小规模所有 checkpoint 的 exceeded=false；不能反向改写原 40k 的 exceeded=true。
两侧同规模的释放 delta 很接近，但不把差异或固定基数外推为任意输入的通式。

下表是新诊断 release checkpoint 间 requested bytes 的净下降，均发生在 Host 已关闭之后；
它测量释放这组外部引用后的可见净变化，不证明该对象独占所有底层 allocation。

| 释放组 | 64 low | 2048 low |
| --- | ---: | ---: |
| decision.result 所在 decision | 169,266 | 5,273,126 |
| expected / evaluated | 338,556 | 10,546,276 |
| Snapshot / decodedSnapshot / bytes buffer | 664,825 | 20,662,853 |
| Replay | 236,694 | 7,301,836 |
| content / packed / fixture | 896,725 | 27,019,769 |

最后一组之后还有 23 allocations / 1,416 requested bytes，随 prototype/reuse 等剩余局部
出作用域而回到入口。该现象与静态持有边界一致；equal aggregate totals 仍可掩盖块替换，
不具 per-owner block 身份证明，也不覆盖 direct malloc、SDL、GPU、driver/OS heaps。
各阶段 peak 字段是进程累计高水位，不能把它相减当作该域的独立峰值。
本例未在 Seek 后立即单列 checkpoint，因而不能从后续 active 内存独立拆出 Seek 的净占用。

**本批关闭的是“小规模最终作用域检查方法是否有效”这一缺口。40k 最终释放、产品各域额度、
共享计费及峰值仍未验收；没有重跑原 40k low/high。** 后续若需要同规模定向采集，必须明确
新目标、固定次数、真实产品峰值和外部持有策略，不能重用旧 invocation 或将本小例代替 40k。

## 4. F1/C1/C2 原始噪声复算

第二名 Luna 从 21 个原 scale 日志及 process records 复算 504 个 block rows，验证每个
log SHA 和 measured PID/PD_ENV；全部一致，无缺项或身份错误。结果保存在
`out/pd5-read-noise-audit-20261023-733ec8e/{analyze_read_noise.py,analysis.json,findings.md}`。
主代理另写独立算式从 C2 六份 raw logs 复算 read64，得到相同数值，并核对 55 项 source hash。

统计单位仍为进程中位数；3 对 log-ratio 区间用原 t=4.302653，重复性仍为
`(max-min)/median`，没有选取较快块、合并帧数作自由度或填补尾部 quantile。

| 项目 | 复算结论 | 对后续的含义 |
| --- | --- | --- |
| F1 原诊断 read64 | F1/A=1.3742，95% CI 约 1.3517–1.3970；A/F1 重复性约 1.64%/0.66% | 该组读取成本增加不能一概归为噪声；有限诊断不等于正式全组 |
| C1/F1 read64 | 1.0079，CI 约 0.9103–1.1159 | 未满足原保留条件；撤回决定不变 |
| C2/F1 read64 | 1.02137285，CI 0.65154234–1.60112772；C2 重复性 38.1195% | 不能保留 C2，也不能声称确定改善/退化 |
| C2 全部 8 个 read/updated 指标 | 每个 C2-side 重复性都超过 15% | 公共 ready 或 256 点估计不能代替主指标 |
| 慢态分布 | C2 B1 在 8/64/256 的 read 和 updated 多个块共同偏慢；配对 F1 A1 部分指标也慢，C2 B2 部分小规模更快 | 支持进程/时间相关变化，不识别具体原因 |

C2 B1 的原 24 行合计 wall≈19.125 秒、thread CPU≈18.953 秒。它不是由一个巨大 off-CPU
尖峰单独撑起的样本，但 GetThreadTimes 粒度、周期计数和起止环境不提供完整调度/频率证据。
原行没有逐块 QPC 锚；不能按现有 cycles 数字推导真实时间或认定某 CPU 频率/温度。
所有慢样本、三对配对和历史结论原样保留，没有新计时样本。

## 5. 读取热路径与下一步测量条件

主代理核对 F1 源码和冻结 COFF 反汇编：逐对象 readObject 仍有 owner/thread 与 serial/active
检查、out-of-line ObjectEntityMap::entries、组件读取和中性 Result payload 返回。
C2 的生成代码消除了 128-byte value return copy，但仍有 Result<void> 错误存储初始化及输出重置。
这些证明相应指令存在，不证明其独立耗时，更不证明关闭检查或暴露 World/EnTT 是合法修复。
旧 Q3 ablation 丢失保护的历史点估计继续不具保留或正式接受资格。

下一候选可考虑在现有 owner-bound 窗口内摊薄连续读取的固定调用成本，但尚未登记、实施或计时。
必须先核对布局顺序、部分读取/错误次序、窗口失效、单一物化器与有界临时存储的维护成本；
不增加第二状态源、缓存同步、owner、线程或关闭协议。一个批次最多预登记一个候选。

噪声下一步工具准备位于 `out/pd5-f1-noise-control-20261023-733ec8e`：仅冻结 F1 的三个
独立 traced control 进程，保持原 workload、mask=4 和预热；无候选、无收益/正式接受结论。
外部 ETW 的目的为核对调度/ready/中断与支持的处理器功耗事件，不把 traced 时间套入
正常 allocator 的正式阈值；旧 harness 缺逐块 QPC 的限制也必须随结果披露。

本机 WPR/xperf 已安装，准备时默认 WPR instance 报告无录制（未枚举所有 named sessions）；
当前 token 非管理员，whoami /priv 不含 SeSystemProfilePrivilege。主代理完成脚本独立复审、
全部 artifact/input hash 与语法核验后，仅执行一次普通权限预检：exit 2、NOT RUN，
receipt 的 wpr_start_attempted/native_launch_attempted 均为 false，没有创建 run 目录。
这是脚本权限拦截生效，不是操作系统拒绝一次已尝试的 capture。
准备脚本在任何 capture/native 前停止，
不自动 UAC 提权，不停止或取消他人的 WPR session。本批没有启动 ETW 或新的 F1 性能进程。
管理员环境只是后续采集条件，不是对后台安静、频率稳定或生产预算合格的保证。
可交给 owner 的入口为该新目录内 `run_f1_noise_control.ps1`，只执行一次；包含三次测量及
各自 30 秒预热，采集上限约 420 秒。实际 elevated WPR/native 路径未执行；失败保留原始
partial evidence 和 cleanup_failure，不自行重试。新的 mesh/material 运行时路径、解析它们的
源文件与编译定义已在采集前绑定，未把旧诊断的事后补证伪装成前置证明。

## 6. 验证、交付与保留边界

新目录保存 plan、编译/链接日志、derived source、唯一 diff、所有复用输入/实际依赖 hash、
四个 native raw logs、结构/内存 checkpoint 和 runner/result/summary。
`parent-independent-check.json` 保存主代理的独立数值和输入核验；独立复审结论与最终完整性
回执另行保存。子代理静态产物及其源文件 binding 保留原字节，不混入产品源码提交。

新报告/索引/状态修改执行 `python -B tools/check_docs.py` 与 `git diff --check`。
产品代码未变，只有一个派生诊断 TU 编译/链接；不宣称完整 CTest、Debug/shared、GPU/audio、
消费者或 hosted 新验证。原 47 根 42,745 文件、主树 F1/C2 和上批 evidence package 继续保护，
最终哈希核验与封包回执记录实际通过项和覆盖范围；旧清单的 events 漂移失败不追溯改写。

本批只完成静态责任、原数据复算、小规模寿命诊断和下一步采集准备；没有修复/接受读取性能，
没有接受 40k 分域内存、重新登记正式测量或结束 PD-5。七项物理预算、四项 hash ceilings、
1.10/15%、reply deadline 30 秒保持。旧窗口 3、公开停止接口、merge/push 继续停止。
