# R2 HighQoS 局部收益与 N1 同进程消融

状态：local benefit confirmed for N8/N64/N256 under the registered HighQoS profile；
N1 公开读取不退化区间仍 inconclusive，R2/R4 未退出。

日期：2026-10-26（Asia/Shanghai）。当前状态见 [CURRENT_STATUS](../../../../../CURRENT_STATUS.md)，
设计/门禁见[整改计划](../../../../../stage_plans/active/stage-07/05-rpa-new/plan.md)。

## 1. 用户背景、确认结果与实验顺序

用户说明上一次 B 基线测量期间正在使用 IDEA 跑服务，认为可能存在调度干扰；随后要求
“再跑一遍确认一下，然后继续推进R2”。这是用户补充的环境背景，旧窗口没有逐服务负载记录，
不能据此追认 IDEA 为唯一历史根因。

本次按同一 B-only 参数新建独立确认：N1 前三块约 261 ns/次，之后约 301–328 ns/次，
目标核系统报告频率再次从 3722 降至 3040 MHz。N1 中位数 310.69 ns/次，块内范围 21.49%；
N64 中位数 4892.80 ns/次，块内范围 1.21%。原始材料在
`D:/Cuexis-worktree/out/r2-noise-confirm-20261026T065945Z/`，此前各次诊断均未替换。

因此继续进行一个预先登记的单变量环境诊断：只为新启动的诊断 native 进程显式请求
HighQoS，ProcessPowerThrottling 的 execution-speed ControlMask=1 / StateMask=0。
该设置影响当前新进程的服务质量请求；不是固定硬件频率，不修改整机电源方案、优先级或 SDK 行为。
接口语义见[微软 QoS 说明](https://learn.microsoft.com/en-us/windows/win32/procthread/quality-of-service)和
[设置进程信息](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessinformation)。

这一个 HighQoS B-only 诊断得到 N1/N64 块内范围 0.54%/0.50%，中位数为 262.35/3898.63 ns/次，
读取区间报告频率保持 3722 MHz。参数在所有计时工作之前设置并读回成功。
原始材料在 `D:/Cuexis-worktree/out/r2-qos-20261026/`；这仅支持受控环境因素值得继续验证，
不能把一个进程的稳定性直接等同于跨进程接受。

## 2. 同一第三候选的新独立 B/E 窗口

随后在 `D:/Cuexis-worktree/out/r2-qos-window-20261026/` 预登记唯一的新对照窗口。
登记 SHA-256：`7325178efa1ab09d4397f7109b758fb329951a4f510c9b70dabcdff6a95dc97e`，formal_measurement=false。

源码/二进制仍是上一轮绑定的 B/E：B 为冻结 R1 读取加两侧共同整合修复，E 为 direct borrowed Result
第三候选，产品差异仍只有 Runtime 的 presentation_read.hpp/.cpp 和 Playback 的
presentation_materializer.hpp/.cpp。没有第四产品机制，没有修改产品、测试或构建输入。
主树 HEAD 保持 `cccbd47eda8ecd65e018ec31063ec027c39bd04e`；九个主树 R2 输入、双方全部源、
编译器、生成头/传递依赖、库、DLL 和二进制哈希均在窗口前后及各进程边界重新验证。

本轮复用已绑定的五构建正确性、包边界、回滚和对象码证据，并核对源/产物身份；
没有宣称又完整编译五配置或重跑消费者。历史详细范围见
[第三候选续接报告](2026-10-26-r2-direct-result-continuation.md)。

协议保持 B-E / E-B / B-E 三对六进程，每个读取进程前独立 30 秒机器热身；
每规模 fresh-session 64-step 预热，8 blocks，每块至少 100 ms，mask=4、viewport=1280×720。
N1/N8/N64/N256 每块分别读取 2097152/1048576/262144/65536 次，普通 allocator，走公开 reusable
extractFrame/read_long，产品插桩关闭。两侧每个新热身/读取 native 进程都执行同样的 HighQoS 请求，
并验证设置时间早于全部计时样本；process/thread priority 仍为 normal/0。

本次成对窗口不运行连续外部 PDH 观察器，以保持普通读取计时。其记录证明的是显式进程策略和
实际噪声，不证明计时区间硬件频率恒定。全机电源方案、最大/最小处理器状态的前后只读快照一致。

## 3. 独立复算与有效结果

六个读取及六个热身进程均首次 exit=0，所有身份、fixture/digest、QoS、输出和块时长门通过，
未替换或剔除样本。主代理从原始 raw 独立复算 192 个读取块、各进程八块中位数、三对 log 比值、
固定 t(2)=4.302653 的 95% 区间及两类噪声，与控制器逐项相符；最短读取块为
534.380 ms。没有把块当作独立进程，没有跨旧窗口拼乘。

| E/B | 几何比 | 固定 95% 区间 | 最大块内范围/中位数 | 最大进程间范围/中位数 | 结论 |
| --- | --- | --- | --- | --- | --- |
| N1 | 0.9754 | [0.7851, 1.2120] | 1.64% | 10.62% | 不退化区间 inconclusive |
| N8 | 0.8588 | [0.8054, 0.9157] | 2.84% | 14.77% | 局部收益确认 |
| N64 | 0.7981 | [0.7814, 0.8151] | 1.00% | 3.73% | 局部收益确认 |
| N256 | 0.8029 | [0.7921, 0.8139] | 2.04% | 1.40% | 局部收益确认 |

四规模两类噪声均 ≤15%。N8/N64/N256 的局部点估计分别改善 14.12%/20.19%/19.71%，
区间上界均低于 1，确认此 HighQoS 配置下的局部收益；主点 N64 的改善区间约为 18.49%–21.86%。
N1 点估计改善约 2.46%，但其区间上界 1.2120 超过 1.10，不能声明不退化通过；
它也不是已证明超界，因为区间下界未超过 1.10。

registration/claim、六份 processes、全部 raw 和 results.json 保留本轮身份。
independent-results-audit.json 与 final-evidence-verification.json 保存独立复算、全部十二个
native 进程策略、主树源保全和电源快照复核。旧六进程窗口仍保留其 noisy/inconclusive 结论。

## 4. N1 的固定同进程机制消融

为缩小 N1 疑点，另在 `D:/Cuexis-worktree/out/r2-n1-ablation-20261026/` 预登记且仅运行一个
N1 物化器消融进程。使用既有正确性测试中的真实 Fixture、同一 Runtime/布局和两个独立 reusable
owning DTO，冻结旧物化器与当前物化器分别编译为不同翻译单元，以匹配外部调用边界，未启用 LTO。
compiler/库仍来自当前 E 的同一工具链与绑定库，普通 allocator，native mask=4、显式 HighQoS。

进程内先 30 秒热身，两路径各 64 次预热；每路径每块 2097152 次，八对 blocks 按旧/新、新/旧交替。
完整字段比较在计时外进行。一个进程的十六块全部有效，286 项断言通过；主代理独立复算与
工具结果一致。八个块比值都小于 1，范围为 0.9467–0.9582，中位比值为
0.9516，对应约 4.84% 的内部物化成本降低。

这只是固定的合成正确性 fixture 和 Runtime 同步读取窗口/物化器，不包含公开 Gameplay
publication/extractFrame 的外层工作，不能把它当作真实公开 N1 的接受。八对块仍只是一进程，
不计算独立进程的 95% 接受区间。结果不支持据此再改产品机制，也不能断定全部公开差异来自外层。

该临时探针的两次早期链接失败及一次不支持的列举参数失败保留在日志中；这些均发生于
登记/计时前，原因是缺少 manual-link Catch2Main、console subsystem 和 CLI 参数适配。
修正后一次固定测量完成，没有重跑或替换计时样本。

## 5. 当前结论与剩余边界

本轮将 R2 从“四规模收益未确认”推进到 N8/N64/N256 在明确的本机 HighQoS 测量配置下收益确认，
主点 N64 已有有效局部证据。N1 公开提帧的不退化区间仍未收敛；同进程消融在其固定 fixture 中
未发现当前物化器的成本退化，但不能填补公开 N1 的区间缺口。因此 R2 整体/R4 仍不记为退出。

下一步聚焦公开 N1 的进程间成本差异及其外层读取工作，在已有证据上形成可证伪的限定诊断，
不以追加同一窗口样本或缩小适用规模处理缺口。当前第三候选继续保留，产品机制未扩张。
原 15%、1.10/95% 规则、R0/Q4 历史限定及失败证据保持，不进行频率归一化修正。

新窗口只回答当前 B/E 和显式 HighQoS 条件，不能给默认系统 QoS、冻结 A 的正式对照、R4、
40k、设备/生产或 Stage 7 关闭作接受。未运行 ETW/WPR、GPU/设备、正式 R4、R3 或 hosted CI。
