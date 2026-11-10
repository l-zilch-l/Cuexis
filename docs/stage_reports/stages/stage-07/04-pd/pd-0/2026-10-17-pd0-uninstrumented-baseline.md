# PD-0 无编译插桩正式基线协议与实施

状态：原正式组通过；后续补充复测未通过，当前 PD-0 退出待重新核验

后续证据见[三轮补充复测与归档](2026-10-17-pd0-supplemental-retests-and-archive.md)。
以下结果和退出核验保留为原正式组时点证据，不覆盖补充组失败或当前状态。

日期：2026-10-17

## 范围

采用[设计细则 §9.2](../../../../../stage_plans/active/stage-07/04-pd/design.md#92-基线与对照方法)
更新后的三类证据边界。历史失败及[块级诊断](2026-10-17-measurement-method-history.md)保留，
本轮不解释为测量算法缺陷已消失或历史根因已解决，不引入新 owner 或修改账本容量。

新增独立正式入口及 Debug/Release STATIC candidate ON、metrics OFF 构建。
必须核对 cache、实际编译命令、二进制身份和探针输出中的编译条件；正常分配器，
不使用 recording ON/OFF 作为正式实现对照。旧工具仍保留旧证据用途。

## pilot 预声明

在新源码和构建冻结后登记一次有限 pilot：CPU 0 与由系统拓扑确定的另一物理核首个可用逻辑 CPU，
每个核心、每配置/profile 两个独立进程，共 16 个；不因噪声追加 pilot。
每核心先进行 30 秒 CPU 预热，不在 legacy session 内延长历史；每轨迹仍 fresh session、
64 步 session warm-up、1024 步含逐步验证与 FrameDigest 的测量。
pilot 每进程 3 条轨迹与 3 个 4096 次读取块；读取块只用于校准，不形成正式噪声判断。
按两进程重复性在四个配置/profile 的最坏值选择较小者，平局选非 CPU 0；保留两候选全部结果。
批长按所选核心两个 pilot 的最快单次成本估计，以 next power-of-two 达到 312.5 ms、
至少 4096 次、至多 4194304 次；各配置/profile 独立冻结，不在正式组内自适应。

## 正式组预声明

Release/Debug × legacy/RPA，每单元六个独立进程，总计 24 个，两个时段各三次。
每进程 8 条 fresh 固定轨迹及 8 个固定长读取块，均不启用阶段插桩、分配探针或 ETW。
每时段先在所选核心执行同一 30 秒机器 CPU 预热；session 预热不变。
顺序轮换配置/profile；计时块中保留验证工作，prepare 和结果序列化在边界外。
QPC 记录块起止；CPU time/cycles 仅作外层诊断，不能转换为 GHz。

每配置/profile 两项主指标分别计算各进程块中位数，两个时段及全六进程原公式均须 ≤15%。
慢样本全部保留；块级 min/median/max、可支持分位数另报，不与进程重复性混为一项。
每个正式长读块至少 100 ms，否则该组为 inconclusive，不重试；正式组批长不变。
已接受的两时段最小间隔为 **10 分钟**，从第一时段完成开始计算。

交流电、可查询完整电源参数、拓扑、亲和性、进程/线程优先级、可查询显式节流状态及前后台信息
均登记并核对。OEM 模式、散热摆放依赖宿主声明，未确认项显式 unknown，不冒充设备生产校准。
不更改电源方案、QoS、优先级、timer resolution 或后台应用。

## 宿主与归因

宿主使用独立 metrics OFF 二进制及默认允许 CPU 集合，每配置六个独立短轨迹，
也分两时段。每次 32 预热 + 512 采样，544 小于账本容量 1024。
原预登记进程均值保留为重复性指标，四类延迟分别报告均值、可支持的 p95/p99、最大值；
尾部预算未接受时超预算次数记 not applicable，可另列明确标注的诊断参考阈值计数。
不将该短轨迹称为跨容量稳态或实体 GPU/audio 验收。

本轮先完成无追踪基线。逐调用有界 QPC 缓冲、嵌套关系和 ETW 精确归因仅在新组出现明确问题后登记，
不先扩展诊断设施。源码/身份/环境或执行错误立即停止；固定组性能结果全部采齐后统一裁定，
不追加成功样本覆盖失败。仍不满足 15% 时保留 inconclusive，向宿主求助并考虑专用性能机。

方法参考：[Google Benchmark user guide](https://google.github.io/benchmark/user_guide.html)、
[reducing variance](https://google.github.io/benchmark/reducing_variance.html)、
[Microsoft QPC](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps)、
[Windows QoS](https://learn.microsoft.com/en-us/windows/win32/procthread/quality-of-service)。
协议与执行结果分列如下；历史失败结论保留。

## 实施与准备验证

新增 `tests/pd/formal_baseline.py`、`formal_baseline_tests.py` 与 `measurement_environment.hpp`，
固定轨迹及宿主探针扩充隐藏的无插桩测量入口与环境记录。
旧 `tools/verification/measure_pd0_method.py` 只复用身份、统计和 child 亲和性入口，
新入口独立检查 metrics OFF；没有移动工具或增加第三方依赖。

实现 SHA-256：`e7e7c1223c63be37fb6f7d00e728983f8739b0269a8c05f0b9a795e6a318ea6e`。
独立构建目录为 `out/build/pd0-formal-release/` 与 `out/build/pd0-formal-debug/`。
两者 STATIC、candidate ON、metrics OFF，cache 与实际编译命令均检查通过；
无分配探针宏，原生输出也确认 metrics 编译关闭和 normal allocator。

MSVC 两配置构建及四个完整普通探针程序回归通过。
新增三个六进程统计测试覆盖跨时段漂移、15% 边界、慢样本保留与缺失组拒绝；
与已有三个 QPC/电源校验、五个统计测试共十一项 Python 测试通过。
修改 C++ 的格式检查、文档检查通过。未重跑不受影响的整个 SDK 平台矩阵。

## pilot 结果与正式注册

pilot：`out/pd0-uninstrumented/pilot-20261017T114551Z-544965/`。
两个核心各 30 秒 CPU 预热、各八个测量进程，共十六个进程全部成功；无额外 pilot。
由系统拓扑取得物理核 masks `3,12,48,192,768,3072,12288,49152`，
两个候选逻辑 CPU masks 为 `1`（CPU 0）和 `4`（CPU 2），不互为 SMT 同胞。

预登记选择分数为四种配置/profile 两项指标中最大的两进程相对跨度：
CPU 0 **20.7646%**、CPU 2 **4.4265%**，因此选定 CPU 2。
这是 pilot 核心选择结果，不作为三个重复的正式通过证据，也不能单独确定 CPU 0 慢的原因。
正式长读批长：Release legacy/RPA `2097152` 次，Debug legacy/RPA `32768` 次。

正式注册：`out/pd0-uninstrumented/registration-20261017T114840Z.json`。
总计二十四个单 owner 进程、十二个宿主进程，分两个时段。
每时段十八个进程，不混用插桩二进制或 ETW；宿主继承允许 CPU 集合，未挤到 CPU 2。
单 owner 的 correctness/Replay/Snapshot/FrameDigest/Packed 身份跨进程精确核对，
宿主按原内容/帧身份验证；没有把内部调用数当作独立重复。

显式节流的进程/主线程查询均成功，ControlMask/StateMask 均为 0，
进程 priority class 为 NORMAL（32），主线程 base priority 为 0。
这只表示未显式控制节流，不代表 OS 自动 QoS、有效频率或热状态恒定。
前台 PID 在各进程前后环境检查点记录，不声称连续监测前台活动。
宿主 worker 的逐线程有效 QoS/动态优先级未独立采集，标记未测，不由主线程值推断。
OEM 模式未收到确认，登记为 unknown；未更改系统模式、散热设置或后台任务。

## 第一时段

证据：`out/pd0-uninstrumented/window-1-20261017T114911Z-b9e2a0/`。
十八个进程全部 exit 0，源码、二进制和各检查点电源身份通过。
单 owner 八项主指标的三进程中位数跨度为 **0.6970%–3.3988%**；
Release/Debug 宿主进程均值跨度分别为 **5.5286% / 4.1020%**。

第一时段完成于约 UTC `11:52:31.531498`；第二时段不得早于 UTC `12:02:31.531498`。
runner 以单调时钟 enforce 600 秒，UTC 只作展示；没有缩短间隔或追加替换进程。
等待期间仅整理证据、文档与轻量验证，不运行构建或其他性能采样。
第二时段按登记间隔执行，结果如下。

## 正式组最终结果

第二时段：`out/pd0-uninstrumented/window-2-20261017T120240Z-4b9682/`。
实际间隔 **609.348 秒**，超过登记的 600 秒。两个时段共 24 个单 owner 与 12 个宿主进程，
全部成功，没有追加、替换或丢弃慢样本。源码、冻结二进制、检查点电源指纹和语义身份核验通过。
单 owner 共 384 个计时块；每配置/profile 的每项指标各有 48 个块，独立实验单位仍为六个进程。
全部正式长读块超过登记的 100 ms 下限。

下表百分比均采用原相对跨度公式。前三列用于登记的进程级门禁，最后一列保留原始块的跨度，
不把两种统计量混用。`validated_trace` 包含验证和 FrameDigest；`read_long` 是固定长读取块的摊销成本。

| 配置/profile | 指标 | 时段一 | 时段二 | 六进程 | 原始块跨度 |
| --- | --- | ---: | ---: | ---: | ---: |
| Release legacy | validated_trace | 3.40% | 2.63% | 3.68% | 12.53% |
| Release legacy | read_long | 1.87% | 13.61% | **14.82%** | **23.62%** |
| Release RPA | validated_trace | 2.59% | 3.30% | 3.43% | 17.74% |
| Release RPA | read_long | 1.29% | 4.37% | 4.36% | 7.23% |
| Debug legacy | validated_trace | 2.93% | 0.39% | 2.98% | 9.02% |
| Debug legacy | read_long | 0.70% | 1.17% | 1.84% | 14.19% |
| Debug RPA | validated_trace | 3.02% | 2.35% | 3.00% | 23.36% |
| Debug RPA | read_long | 1.50% | 0.25% | 1.71% | 2.90% |

八项主指标在两个时段和全六进程均 ≤15%，正式基线通过预登记重复性门禁。
最高 14.8239% 接近门限，不能推断有充足余量，也不能据此声称单调用尖峰或系统波动消失。
原始块跨度最大 23.62%，保留在汇总及原始数据中；没有用进程中位数删除这些块。

## 宿主交付与尾部

宿主采用默认允许 CPU 集合、独立无插桩二进制，共 12 个进程、6144 个测量帧。
request 到 receipt ACK 的进程均值重复性：Release 两时段为 5.53% / 7.96%、全六进程 **8.99%**；
Debug 为 4.10% / 0.29%、全六进程 **4.08%**。两配置均通过独立的 15% 重复性门禁。

| 配置 | 帧数 | 均值 μs | p95 μs | p99 μs | 最大 μs |
| --- | ---: | ---: | ---: | ---: | ---: |
| Release | 3072 | 40.63 | 65.7 | 88.9 | 228.9 |
| Debug | 3072 | 356.21 | 444.1 | 493.6 | 792.2 |

表中分位数是六进程合并帧的描述统计，不作为 3072 个独立实验或收益置信区间。
每进程仅 512 帧，单进程 p99 按统计规则记未支持。
request→Ready、Ready→ACK、present call→ACK 的独立统计同存于注册 state 和退出 manifest。
1000 / 5000 / 16667 μs 诊断参考线的 request→ACK 超线次数均为零；
这些参考线不是已接受尾延迟预算，正式预算和超预算次数均为 not applicable。
短轨迹未跨账本容量，不能证明长时稳态或实际设备性能。

## 同源码存储与计数重绑定

存储证据：`out/pd0-storage-v1/20261017T120806Z-c45479/`，24 个进程全部通过。
正常分配器交付观测：`out/pd-20261017/normal-delivery-20261017T121108Z/`，12 个进程全部通过。
两组都绑定同一实现 SHA，并核验运行前后源码和二进制未变；插桩观测与正式性能结果分开。
存储组的 Replay、Snapshot、FrameDigest、Packed 身份与无插桩正式组按 profile 精确一致。

每配置/profile 的 24 个 recording ON 轨迹窗口，以下分配计数全部一致。

| 配置/profile | 分配次数 | 请求分配字节 | 存活增量字节 | 峰值增量字节 |
| --- | ---: | ---: | ---: | ---: |
| Release legacy | 102756 | 437546184 | 8538632 | 9273550 |
| Release RPA | 185668 | 74544840 | 8716776 | 8749033 |
| Debug legacy | 2692917 | 553965664 | 10438936 | 11372438 |
| Debug RPA | 700181 | 91890688 | 10781944 | 10821705 |

这是 operator new 请求计数，排除 malloc/CRT、栈/TLS、驱动及 GPU；逻辑复制字节不是硬件内存流量。
阶段计数、冷 projection 写入、Runtime 状态逻辑复制、Snapshot 数量及 CPU renderer/账本计数
保留在原始输出和退出 manifest，不将分配探针耗时作为性能基线。

## 退出核验与限制

独立退出核验：`out/pd-20261017/pd0-exit-20261017-e7e7c122/closure-manifest.json`，
配套 `evidence-hashes.json` 固定本轮证据文件身份；核验脚本为
`out/pd-20261017/audit_pd0_exit.py`。

| PD-0 核验项 | 结果 |
| --- | --- |
| 内容、输入、时钟、固定参数、源码及构建 manifest | 完整登记并核对；64 步预热、1024 步轨迹、1280×720、单对象有限 fixture |
| Debug/Release × legacy/RPA 的可重复性能基线 | 每单元六进程，两时段与合并门禁均通过 |
| 阶段耗时、扫描/写入、Runtime 复制、Snapshot 与分配观测 | 同源码存储/阶段观测及正常分配器交付观测通过；亚微秒阶段只作诊断 |
| 交付、跳过、receipt 与窗口/内容身份 | 原有路径的宿主 oracle 和计数核验通过；新结果版本、年龄/滞后等不存在项记 not applicable |
| 正确性、Replay、FrameDigest、Snapshot、Packed 身份 | 正式重复及插桩组间一致 |
| 受影响验证 | 两配置构建、四个完整普通探针、十一项 Python 测试、C++ 格式和 architecture 检查通过 |

据此完成 **PD-0 的固定 testOnly 基线与行为表征**。本轮没有旧/新实现的收益比较，
因此没有改善效果量或 95% 置信区间；后续实现对照仍须按可执行文件平衡配对，不能沿用 recording 开关对照。

历史无追踪慢运行与毫秒级单调用尖峰根因仍未确定，旧 inconclusive 结果不变。
帧账本容量缺陷继续由 PD-4 处理：容量 4 在第 5 帧、容量 4096 在第 4097 帧耗尽，冻结 tick 亦可复现；
没有扩大容量或绕过回执。OEM/热状态与 worker 有效 QoS 未测，不宣称这些系统因素受控。
runner 强制检查无 WPR 录制，但没有逐次保存 WPR 状态响应，这是证据留存限制。
PD-1 至 PD-5 未开始；RPA 实体设备、生产预算、最终接受、整个 PD 阶段退出均未完成。
