# PD-0 性能重复性诊断与基线重建交接

日期：2026-10-16（Asia/Shanghai）

状态：诊断完成；原始性能重复性仍为 inconclusive，历史慢运行的具体触发原因未确定；
不是 PD-0 退出或新基线接受。

## 范围与身份

本轮请求是协助另一对话处理 PD-0 性能重复性问题，不接续整个 PD 产品实施。
owner 确认另一对话已暂停构建和性能测试，仅由本对话诊断。
桌面接手中的后续实施指示作为历史背景，不扩大本轮任务。
现行范围归 [PD 计划](../../../../stage_plans/active/stage-07/plan-presentation-decoupling.md)，
测量规则归[设计细则 §9](../../../../stage_plans/active/stage-07/presentation-decoupling-design.md#9-容量-profile测量与退出pd-0--pd-5)，
实现状态归 [CURRENT_STATUS](../../../../CURRENT_STATUS.md)。

本轮 HEAD 为 `276358d99baf0e280a7d6e0429f9990ffcc9f6f1`。
诊断前 tracked 工作区干净，既有 `tools/__pycache__/` 保留。
未修改产品源码、原表征用例、原证据或接受阈值。
新建独立时钟校准程序和诊断脚本，产物全部保存在
`out/pd-noise-investigation-20261016/`；本报告及报告索引是本轮文档改动。

全部六次产品表征仍使用保存的 Release executable：

- 路径：`out/pd-20261016/baseline-release/cuexis_playback_tests.exe`。
- SHA-256：`c43cfb4819802058e49f064b919cb0eefb322f2135342b6dd2c187a4b7eb1c0c`；运行前后相同。
- 命令参数：`"PD deterministic presentation baseline" --rng-seed 1`。
- 64 次预热、接着 1024 次采样；固定原输入、时间序列、1280×720 viewport。
- 原 15% 定义保持为 `(最高运行中位数 - 最低运行中位数) / 运行中位数的中位数`。
- 当前源码 hash 单列于 `source-audit.json`，不据此补造保存 executable 的完整构建来源。

## 原证据复核

不止用户摘要中的五次，目录还保存了一组 Debug/Release 交替的复测。
三组共八次 Release 原始 JSONL 均纳入分析，未选择或删除样本：

| 证据组 | Judgement 中位数 μs | Projection 中位数 μs | Runtime 中位数 μs | Snapshot 中位数 μs |
| --- | --- | --- | --- | --- |
| `baseline-release` 原三次 | 19.80 / 24.15 / 19.80 | 0.4 / 0.6 / 0.4 | 2.6 / 3.4 / 2.7 | 0.4 / 0.5 / 0.4 |
| `remeasure-20261016-200956-ef62b6` | 19.50 / 19.30 / 19.15 | 0.4 / 0.5 / 0.4 | 2.6 / 2.6 / 2.5 | 0.4 / 0.4 / 0.4 |
| `release-noise-recheck-20261016-201204-334f4e` | 19.60 / 19.10 | 0.4 / 0.4 | 2.7 / 2.7 | 0.4 / 0.4 |

第一组四项超过 15%；第二组 Projection 仍为 25%；第三组仅两次，不能替换第一组的慢运行。
第二组说明，即使没有四阶段一起偏慢，短阶段的门禁仍可能失败。
复核窗口为各阶段 `exclusive_us[64:1088]`，原始阶段均有 1088 个样本、`omitted=0`。

原慢运行并非只有一个尾部尖峰：将采样窗按顺序分成八段，每段 128 个样本，
其最初三段 Runtime 中位数为 3.4 / 3.5 / 3.6 μs，正常第一运行对应为
2.4 / 2.3 / 2.4 μs。删除几个最大值不能解释或修复这种分布移动。
旧记录没有逐样本 CPU、调度、频率、抢占或页错误轨迹，无法事后确定具体触发因素。

## 已确认的测量限制

### 时钟量化与嵌套观察开销

本轮使用 MSVC 14.51.36231、`/O2 /std:c++20` 编译独立 `clock_probe.cpp`，
直接包含当前 `presentation_metrics.hpp`，三次校准得到：

- `QueryPerformanceFrequency = 10,000,000 Hz`。
- `steady_clock` 的类型 period 虽为 1 ns，实际观测最小正增量为 **100 ns，即 0.1 μs**。
- 相邻时钟读取的中位差为 0，p99 为 100 ns；0 不代表读取没有成本。
- 空 Snapshot timer 均值约 0.023 μs、中位数 0；嵌套空 Projection/Runtime 的父阶段
  exclusive 中位数为 0.1 μs、均值约 0.054 μs。

本机 MSVC `__msvc_chrono.hpp` 的 `steady_clock::now()` 使用 QPC；10 MHz 分支将计数乘 100
转为 ns。原八次 Release 样本也全部落在 0.1 μs 网格。
因此，改用 `nanoseconds` 输出或保留更多小数不会产生更高的真实分辨率。

对 0.4 μs 阶段，15% 仅为 0.06 μs；0.4→0.5 μs 的单台阶切换就被记为 25%。
样本中位数可能因分布跨过相邻台阶而跳变；不能把跳变百分比直接解释为等量业务性能回归。
这不等于证明所有波动都是量化，也不意味着应自动放宽阈值。

父 Timer 在析构时扣除子 Timer 的 `Clock::now()` 区间，但子 Timer 起始之前及结束之后的
TLS、记录样本和恢复 active 指针等工作仍可计入父区间。
校准证明观察开销相对这些短阶段不可忽略；空循环校准不等于业务路径的精确开销，
不能从每个业务样本机械减去 0.1 μs。
这解释了短阶段方法的脆弱性，**不能单独解释 Judgement 19.80→24.15 μs 的跨运行变慢**。

### 当前用例不是固定成本的稳态 RPA workload

当前表征源码的 `TestContent` 调用 `executionFields`，选择
`gameplay.execution.t4-k4.v1`，不是 `gameplay.execution.t4-k4.rpa.v1`。
见 `tests/judgement/execution_test_fixture.hpp` 和 `tests/playback/playback_gameplay_tests.cpp`。
它可以作为旧 profile 的行为/轨迹对照，但不能代表 RPA 部署路径。

`engine/judgement/src/execution_kernel.cpp` 的 `recordingCandidate()` 复制 `ReplayData`；
仅 RPA 分支将记录转入有界尾块并清空平面 records。
旧 profile 的 advance 每次追加 ReplayRecord，因此后续复制工作量随历史增加。
原正常第一运行八段 Judgement 中位数为
7.45 / 10.15 / 12.60 / 15.90 / 20.80 / 21.90 / 27.30 / 33.35 μs。
其他原运行也有同方向增长，与源码路径一致。

因此，不应把“预热 64 次后的一段确定性增长轨迹”描述为已证明稳态。
直接将同一 session 改成预热数千次或采样更长，会改变历史长度和工作量；
任何新测量都必须保留原轨迹，或另立明确不同的 workload/profile。
这一已存在的增长也不能单独解释为何原第二运行相对其他运行整体偏慢。

## 固定六次诊断

在新测量前写入 manifest，固定顺序为：默认、单核、单核、默认、默认、单核；
每次间隔 1 秒。单核条件由临时 launcher 设置亲和性并由子进程继承，
原父进程 mask 为 `65535`，单核 mask 为 `1`（逻辑 CPU 0）。
未改变系统电源方案、进程优先级或产品 executable；电源方案为平衡。
未验证核心拓扑，不将 CPU 0 声称为特定性能等级的核心。

| 条件 | Judgement 三次中位数 μs / 波动 | Projection | Runtime | Snapshot |
| --- | --- | --- | --- | --- |
| 默认调度 | 22.95 / 20.45 / 23.10；11.55% | 0.5 / 0.7 / 0.5；40% | 3.15 / 3.10 / 3.20；3.17% | 0.4 / 0.4 / 0.4；0% |
| 固定逻辑 CPU 0 | 21.60 / 21.60 / 22.00；1.85% | 0.5 / 0.5 / 0.5；0% | 3.0 / 3.0 / 2.9；3.33% | 0.4 / 0.4 / 0.4；0% |

六次各 5,464 条断言通过，digest 均为 `17504891284788938091`。
原始采样完整，无丢样。新旧 14 份 JSONL 的 SHA-256 已重新核对。

系统 CPU 计数覆盖 launcher 启动至测试进程退出的整段区间，显示整机忙碌比例约
15.6%–30.4%，不是被测核心/被测样本的负载归因。
当前诊断只支持“固定亲和性是值得保留并验证的测量控制变量”，
不能证明历史原因，也不证明 pinning 足以长期消除噪声。
单核组三次满足数值界限属于预声明的诊断条件，不能事后改名为原基线通过。

## 返回实施对话的处置

建议将原失败保留为一条不可改写的历史证据，将后续旧实现基线单独版本化。
建立旧实现基线不必先猜中不可追溯的历史扰动；需要的是证明新的测量方法适用于目标指标。
下列是重新登记方法时的具体输入，尚未实施或接受为新基线：

1. **先冻结比较对象。** 保留现有 legacy fixture 及 1088 步轨迹；另增明确的 RPA profile
   表征/性能 fixture，分别记录 profile、内容和预期行为，不把两种路径混为一组。
   重新构建并保存当前源码、差异 hash、工具链、配置、资源 hash 和 executable 身份。
2. **控制环境并记录。** 在同一固定核心上测量是本轮有证据支持的候选条件；
   登记 affinity、优先级、电源/供电状态、OS/CPU、原始时间戳和测试并发情况。
   保留默认调度数据供设备/部署解释。亲和性控制不是产品运行策略。
3. **将短阶段测量与整段吞吐分开。** 保留单次 median/p95/p99/max 诊断，
   对接近计时台阶的阶段明确标记 resolution-limited，不能填“通过”。
   若阶段本身要承担性能门禁，应另立足够长的重复工作块，块外计时，按操作数折算；
   同时校准同结构空块和 instrumentation ON/OFF 对整段耗时的影响。
   可重入的只读 Snapshot 可以重复读取同一状态；有状态的 Judgement/Projection/Runtime
   必须使用独立等价 session/事先准备的相同轨迹，不能重复提交同一次状态转移。
   块结构、session 数和缓存条件须登记，新统计量不能冒作旧的单次耗时中位数。
4. **不得直接延长原 session 来预热。** 若要更长测量，重复完整固定轨迹，
   各轨迹使用新的 session，并单列 prepare/reset 成本；每次都核对原 digest。
   对历史压力则另立 workload，显式记录历史长度，不与固定轨迹混报稳态。
5. **预登记后一次性验收。** 校准/试运行结果不得进入正式验收样本。
   明确新 profile 的采样单位、warm-up、批次大小、重复数、噪声公式、分辨率/开销适用条件，
   再固定至少三组独立运行；未来旧/新实现按原计划交替配对。
   保持 15% 原要求，或在测量前显式记录不同指标的另行接受标准；
   不用后验绝对容差、合并均值、删异常值或选最快三次挽救旧结果。
   若新方法仍不满足噪声界，保留 inconclusive，下一步采集调度/频率/抢占轨迹，
   不无限重跑。原 run2 缺少的历史轨迹不能由新轨迹补造。

本轮没有改动公共接口、阈值或 PD 批次状态；完整 manifest、其他指标、当前源码验证与
PD-0 其余覆盖仍由实施对话补齐。功能确定性通过不能替代上述性能方法验收。

## 可复查产物与验证边界

证据目录：`out/pd-noise-investigation-20261016/run-20261016T122343Z/`。

- `manifest.json`：预声明顺序、条件、二进制/计时头/脚本 hash、六次运行与环境计数。
- `historical-analysis.json`：原八次的 hash、分位数、128 样本分块中位数、量化网格。
- `clock-calibration.json`、`clock-*.log/jsonl`：三次时钟与原 Timer 校准。
- `1-default` 至 `6-pinned` 的 `.log/.jsonl`：全部新行为和原始耗时证据。
- `summary.json`：按条件的重复性统计；明确 `pd0_exit=false`、历史根因未确定。
- `source-audit.json`：本轮源码静态核对身份。
- 上级目录的 `diagnose.py`、`clock_probe.cpp`、`build-clock-probe.cmd`：可复查诊断工具。

本轮编译并运行的是独立时钟探针；未重建产品或执行全量 CTest。
Windows CIM CPU 信息读取受限，未获得其数据；CPU/亲和性和时钟记录以实际可读取项为准。
文档改动检查通过：`python -B tools/check_docs.py` 验证 453 份 Markdown 和 20 份
candidate JSON/CXT；`git diff --check` 通过。
