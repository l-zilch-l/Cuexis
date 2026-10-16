# PD-0 固定轨迹与块计时方法

日期：2026-10-16（Asia/Shanghai）

状态：新方法已通过本机受控 testOnly 验证；原组仍为 inconclusive。不是 PD-0 退出。

## 保留的历史和本次范围

[重复性诊断](2026-10-16-pd0-noise-investigation.md)中的原始慢运行与后续诊断全部保留，
原组仍为 inconclusive，历史触发原因未确定。本方法不删除、替换或重新解释原组。
只修改 PD 测量设施与 testOnly 观测窗口，不改变 Gameplay、Runtime 或交付行为。
测试归 `tests/pd/`，不计入 RPA 验收；公共头、SDK 和接受阈值不变。

## 工作量与计时边界

`tools/measure_pd0_method.py` 在每阶段首次运行前写入 manifest；源码身份包括 HEAD、
工作区 implementation hash 和逐文件 hash；二进制、CMake cache、编译器记录、runner 和
资源 hash 单列。内容 Packed/configuration、完整 Replay/Snapshot bytes 的 SHA-256 和
FrameDigest 在 probe 中生成，逐轨迹、逐模式复核精确相等。

legacy 保留原 1088 步轨迹：64 步之后测量固定 1024 步，不延长同一 session 的历史。
另立 RPA workload，通过真实 begin/capture-prefix/coherent advance/retire/presentation
入口执行相同数量的固定步骤；RPA 的 H 来自 synthetic clock，同 owner、不创建宿主线程。
两者分开统计，不把新 RPA workload 称为已经证明稳态，也不把 legacy 的复制增长归入 RPA。
两种 profile 的 RuntimeFrame 时间单位分别明确；不跨 profile 比较 digest 是否相同。

每进程先用一个独立 session 完整执行固定轨迹以暖代码/缓存；然后依次创建八个新的等价
session。每个 session 仍从原始开始，64 步后进行一次块外计时的 1024 步轨迹。
prepare、完整结果/Replay/Snapshot 校验和销毁不在该块时间内，prepare 另列。
测量包含更新、提帧、逐步可见性 oracle 与 FrameDigest，不能将其标为纯 Judgement CPU。
legacy 的历史增长是该固定轨迹的一部分；单次阶段样本只用于分段诊断，不再承担 15% 门禁。

短 Snapshot 单独采用 128 块、每块 4096 次读取同一已提交状态的块外计时；复用输出容量，
每块后核对 FrameDigest，全部读取后核对 Replay/Snapshot bytes 没有变化。
同结构空循环只用于测量下限与观察开销适用性，不从业务样本中机械相减。
不重复提交同一次有状态转移，不将 Snapshot 读块统计量冒作旧的单次中位耗时。

## 校准、环境与预登记判据

仅使用 Windows/MSVC、candidate ON、STATIC、testOnly instrumentation 编译开启的探针。
第一阶段 `calibrate` 对 Debug/Release 各做三次时钟/空嵌套 Timer 校准，再对两个 workload
各做一对运行时记录 dormant/active 试运行。校准样本不进入正式验收。
运行时 dormant 仍含已编译的计时分支，不宣称等同编译关闭版本。

采用继承自父进程许可集合的最低逻辑 CPU bit，保持进程优先级和系统电源方案不变。
登记 OS、CPU 标识、核心 mask、供电、power scheme、原始日志与时间；不声称已识别核心等级。
所有构建结束后串行测量，进程之间间隔一秒；默认调度的诊断证据仍独立保留。

预登记适用性：每个业务块至少为实测最小正时钟步长的 100 倍；空读块中位时间不超过
业务读块中位时间的 5%；active 的整轨迹记录开销上限为 dormant 的 15%。这些只用于
本 testOnly 测量方法，不接受产品预算。超过限值不减去校准值挽救结果。

校准适用后，`accept --calibration <manifest>` 先核对 implementation/binary/受控环境
未变，另建 manifest，固定三对顺序：dormant/active、active/dormant、dormant/active。
两个 workload、Debug/Release 分组；每个模式以进程内八条固定轨迹或 128 个读块的中位数
作为一次运行结果，再以 `(max-min)/median` 检查三次运行的 **15%** 噪声界限。
不跨 profile、配置或 instrumentation 模式合并均值使结果通过，不剔除异常或自动重跑。

每次及总体报告样本数、mean/median/max；p95 至少 200 样本、p99 至少 1000 样本，
不足用 null 标明。分段单次记录保留 8192 个精确窗口样本/阶段并检查 omitted=0；
分辨率与嵌套开销限制继续随诊断数据声明，不加总父子嵌套时间。

失败保留日志并停止；方法仍不满足噪声界时记 inconclusive，再向 owner 求助，
不持续重跑直到选到一组较快结果。

## 交付边界

本次仅解决测量方法；分配/深复制字节、设备/GPU、生产 profile、完整工具链/安装消费者矩阵
及 PD-0 其余指标仍需各自证据。未使用 `pd_allocation_probe.hpp`，不宣称已有分配观测。
即使本方法验证通过，也不能独自关闭 [PD-0](../../../../stage_plans/active/stage-07/plan-presentation-decoupling.md)。

新 fixture 的首次功能 smoke 曾因请求 H 与 coherent clock 不一致而被现有合同拒绝；
日志保留于 `out/pd-20261016/method-rpa-smoke.log`，该次不是校准或正式样本。
修正只作用于 synthetic clock/closure 夹具，不放宽产品校验。

## 本轮校准与正式验证结果

先完成所有构建，再串行执行校准和正式组；没有根据正式结果修改本轮参数或追加重跑。
校准组为 6 次时钟校准、8 次 workload 试运行，共 14 次；正式组为四个配置/profile
组合、每组两种记录模式各三次，共 24 次。全部进程 exit=0，正式组每条轨迹及模式间
FrameDigest、完整 Replay/Snapshot bytes hash 精确相等。

| 配置 | workload | 轨迹块最大跨运行波动（两种模式取最大） | 读块最大跨运行波动 | 15% 界限 |
| --- | --- | ---: | ---: | --- |
| Release | legacy | 12.4843% | 6.7848% | 通过 |
| Release | RPA | 12.7603% | 3.3175% | 通过 |
| Debug | legacy | 3.0573% | 3.2124% | 通过 |
| Debug | RPA | 4.1508% | 1.8075% | 通过 |

表中是每进程内部块中位数之间的波动，不是每次调用耗时的波动；四组没有混报。
正式组最短业务块为 825.1 μs，实测时钟台阶为 0.1 μs，满足至少 100 台阶的适用性要求。
最大空读块/业务读块中位数比为 0.1353%，低于预登记的 5%。
分段记录开启/关闭的整轨迹中位数观测差值为 −3.3478% 至 +0.6459%，满足本轮上限；
负差值仍包含运行噪声，不能解释为负开销、加速收益或精确的 Timer 成本。
所有 active workload 进程的四个阶段各取得 8192 个窗口样本，omitted=0。
Release RPA 的 Projection 单次中位数仍约 0.4 μs、Snapshot 约 0.3 μs，
这些值仍受分辨率/观察开销限制，只用于诊断；没有因块级方法通过就提升它们的证据等级。

新测量源码基线：HEAD `276358d99baf0e280a7d6e0429f9990ffcc9f6f1` 加工作区差异；
implementation SHA-256 为 `9b6e7727ede81db754e16cd42235c3e9b4945034c175e6157468e83efdeef0c8`。
校准及正式组都核对 implementation 与 executable 在运行前后未变化；
校准与正式组的 binary/cache/compiler/affinity/供电/电源记录匹配。
probe 为静态链接、同 owner、单 CPU、无 GPU/audio/窗口；不外推到默认调度或真实设备。

机器可读证据保留在以下目录，每组有预登记 manifest、原始 log/JSONL、逐进程统计、
源码/二进制身份和 `evidence-hashes.json`；正式组另有 `descriptive-summary.json`，
给出各组总体描述统计与单次分辨率限制标记，不参与替换原门禁统计量：

- `out/pd0-method-v1/calibrate-20261016T124628Z-6e239b/`
- `out/pd0-method-v1/accept-20261016T124904Z-866417/`

正式 manifest 的 `status=method_validated` 仅表示上述测量方法通过，`pd0_exit=false`。
原 legacy digest 保持 `17504891284788938091`；新 RPA workload 为
`3815754596451481321`。两种 workload 各自比较，不能把不同 digest 当作语义差异。

## 相关回归与仍未覆盖项

Debug/Release 的独立 PD probe 构建通过；迁移保留的原 legacy 用例再次直接运行，
各 5,464 条断言通过，digest 保持原值。两种配置的 architecture CTest 均 1/1 通过，
本次 PD C++ 文件及计时头的 clang-format dry-run 通过。

曾尝试用 CTest 的 `pd` label 选择探针，但隐藏的测量用例未被 Catch 自动注册，输出
No tests were found；该调用没有被计为测试通过。测量用例由 runner 按完整名称直接执行，
上述 14/24 次证据来自真实 executable 调用。未运行全量 CTest、安装消费者、MinGW/Linux、
GPU 或实体设备验收，未接受生产参数。历史慢运行的具体触发原因仍未知。

文档检查通过：454 份 Markdown、20 份 candidate JSON/CXT；差异空白检查通过。
校准组 39 个、正式组 62 个产物的 SHA-256 已逐一重新核对。
