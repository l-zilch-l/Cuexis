# R4 展示矩阵探针准入、pilot 与环境阻断的固定组

状态：candidate；四单元已登记，attempt-1 在第二窗口因会话锁定停止，无正式裁定

日期：2026-11-08

返回 [New RPA 报告索引](../README.md)。当前结论只以
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md) 为准；本文保存本轮的实施、pilot、
固定组现场与阻断条件。原始字节与复核入口见
[本轮证据](2026-11-08-probe-matrix-evidence/index.md)。

## 1. 授权、候选与边界

owner 的最新有效指示仍是继续 R4 技术工作且**不推送**；本轮没有 push、merge 或 release，
也没有向 PR #32 同步任何提交。技术授权依据
[执行授权](2026-11-05-execution-authorization.json)；数值仍只消费 2026-11-05 已接受的
O1 方案 B，本轮没有新预算、新阈值或新公共语义。

| 项 | 值 |
| --- | --- |
| 工作树 | `D:/Cuexis-worktree`，分支 `codex/r4-integration` |
| 候选源码 | `00af7f90`（本地，未推送） |
| 日期版本 | `26.11.07-5` |
| SDK API | `0.9.1` preview |
| 本轮产品差异 | 无；只新增准入/准备工具，并把展示矩阵探针的采样时钟改为纳秒分辨率 |

候选源码包含工具、登记文本与 fixture 修订，不含未跟踪或 dirty 文件；`clean_source`
在构建、pilot 与每次正式进程前都会整树校验。F1/F2 平台证据仍只属于较早候选
`24f0a914`，本轮没有重跑矩阵或 hosted，因此**不得**把本候选写成已通过 F1/F2。

本报告与后续文档提交位于候选之后；`preparation`/`seal`/`build binding` 只绑定
`00af7f90`，复核这些字节必须检出该 SHA。报告提交本身不重新冻结证据，也不继承其结论。

## 2. 工作区清理（用户本轮显式要求）

用户要求开工前清理旧构建产物与不需要的临时文件。清理前后磁盘空余从 22.8 GB 升至
226.7 GB。删除对象全部是可从已记录身份重新生成的构建树或工具测试临时目录，
冻结证据、来源树、A 基线二进制与 attempt 台账一律保留：

| 删除对象 | 实测大小 | 保留的证明 |
| --- | ---: | --- |
| `out/build/r4/final-20261107` | 116.8 GB | 候选 `24f0a914` 的 11 行矩阵 configure/build/CTest 日志与清单在 `out/evidence/r4/final-20261107` 与 [本轮交接报告](2026-11-07-owning-and-cpu.md) |
| `out/build/r4/post-o1/final-matrix` | 38.9 GB | 早期矩阵日志在 `out/evidence/r4/post-o1` |
| `out/build/r4/pre-o1` | 42.2 GB | R4-1/2 预检日志与 binding 在 `out/evidence/r4/pre-o1` |
| `out/build/r4/world-pages-*` | 6.0 GB | 缩页效果证据在 `out/evidence/r4/world-pages-*` 与 [效果报告](2026-11-06-world-storage-effect.md) |
| `out/pd5-etw-*`、`out/pd5-tool-contract-tests` | <0.1 GB | 工具测试可直接重跑 |

保留：`out/evidence`（0.3 GB）、`out/sources`（0.87 GB，含 pre-PD 只读树）、
`out/build/r4/post-o1`（含 pre-PD A 的 `prepd-english-release` 与其 build binding）、
`out/build/r4/owning-20261107`、`out/build/r4/read-cost-20261107`、`out/evidence/r4/attempt-ledger`。
清理不追改任何历史判定，也没有删除 archive tag 或他人的修改。被删构建树若需复现，
按其报告中的 fresh configure 命令与同一候选 SHA 重建即可；本轮没有删除任何
`out/evidence` 下的原始字节。

## 3. 工具准入

原 PD-5 展示矩阵探针已经包含 P1/P2/P4/P6 的真实负载（14,400 帧实际时间 60 秒、
2,400 帧预热、消费者数、8:1 更新比例、200/201 实体与 256 帧重载）。本轮缺的不是
负载，而是把负载变成**可准入单元**的协议适配，因此新增：

| 文件 | 职责 |
| --- | --- |
| `probe_units.py` | 采样前声明单元、计数、身份与环境变量；计数由规则推导，不从读数回填 |
| `prepare_probe.py` | 每单元一个受限 pilot 进程、三份 proof、准备文档与拒绝门 |
| `formalize.py` | 只做一次性 claim/seal，不启动负载 |
| `environment_stability.py` | 有界持有 `SetThreadExecutionState` 并逐秒采样前台/供电/方案/冲突，产出补救证据；不模拟输入、不启动负载 |
| `runner.py` | 新增 `probe` 适配器：B-only、按帧校验计数、只对已有接受预算的指标登记绝对门 |
| `admission.py` | 探针单元必须 steady、B-only、帧数与登记一致、声明身份字段齐全 |
| `measurement.py` | `pd5.probe.v1` 的可比性身份（新增分支，不改 owner/host 既有语义） |

探针单元全部为 **B-only**：pre-PD tree 没有对应 harness，A 记 N/A，不填 0、不造比值。
P3、P7 与设备 capture 变体需要真实 renderer/音频端点，本轮不注册。

同时修正一个真实的可测性缺陷：原探针用整数微秒计时，而 no-consumer 的单次
`session->update` 低于 1 µs，会被记成 0，进而无法形成正的中位成本。现将采样改为纳秒
计数并输出小数微秒（`tests/pd/pd5_matrix_probe.cpp`），帧数、消费者数、比例与重载间隔
均未改变，因此工作负载身份不变；但该修订前的整数微秒读数不得与本轮小数读数混用。

工具拒绝门：`python -B -m unittest discover -s tools/verification/execution_acceptance -t . -p 'test_*.py'`
共 47 项通过（原 40 项 + 6 项新增探针单元用例 + 1 项登记形状用例）。
`clang-format --dry-run -Werror`、`python -B tools/check_docs.py`（682 Markdown、
20 candidate JSON/CXT）与 `git diff --check` 均通过。

## 4. 构建与绑定

候选 `00af7f90` 的全新 Release STATIC 构建按既有规则完成：`--fresh` 配置、
`--clean-first` 构建、`CUEXIS_LIBRARY_TYPE=STATIC`、`CUEXIS_ENABLE_CHART_V5_CANDIDATE=ON`、
全部插桩选项 OFF、普通 allocator、英文编译器输出。

| 项 | 值 |
| --- | --- |
| 构建根 | `out/build/r4/probe-20261107/release-b1` |
| 绑定证据 | `out/evidence/r4/probe-20261107/build-release-b1` |
| 目标 | `cuexis_pd_host_probe`、`cuexis_pd_baseline_probe` |
| 工具链 | MSVC 19.51.36256.0 / x64，与 pre-PD A 同签名 |
| 探针二进制 | `cuexis_pd_host_probe.exe` 5,718,016 B；基线/热身 `cuexis_pd_baseline_probe.exe` 4,864,000 B |

build binding 覆盖目标级 Ninja 对象/头文件依赖清单、CMakeCache、compile_commands、
compiler、vcpkg manifest 与 configuration、命令文本与全部输入字节，并拒绝任何
插桩定义或 sanitizer/coverage 标志。

## 5. pilot

四个单元各一个受限 pilot 进程（30 秒机器热身 + 已登记的 14,400 帧），环境、原生调度
状态与身份逐项校验；pilot 不进入正式样本，也不消费正式额度。证据在
`out/evidence/r4/probe-20261107/probe-pilot-v1`。

| 单元 | 观测中位（µs） | p99（µs） | 说明 |
| --- | ---: | ---: | --- |
| P1-no-consumer | sdk_update 1.7 | 42.2 | 单次 update 成本约 1.7 µs |
| P2-low-delivery | sdk_update 0.8；capture_delivery 2.2；materialize 1.0 | 25.6 / 21.5 / 12.4 | 8:1 更新下每帧交付 2.2 µs |
| P4-capture-1 | capture_delivery 2.5；materialize 1.1 | 22.0 / 11.9 | 单消费者 |
| P6-sparse | capture_delivery 16.5；materialize 14.7 | 80.7 / 69.3 | 200 实体 |

pilot 只证明**已声明的身份、计数、环境与固定轨迹可执行**；它不是性能裁定，不构成
绝对预算通过，也不证明端点 p99 或设备条件。

## 6. 固定组 attempt-1

登记与封存按既定顺序完成：`prepare_probe` → `admission` 校验 → `formalize` 一次性
claim/seal → `runner` 固定三窗口，每窗三进程，窗间 600 秒。四个稳定单元键
（`p1-no-consumer-release`、`p2-low-delivery-release`、`p4-capture-1-release`、
`p6-sparse-release`）各消耗 attempt-1，台账在 `out/evidence/r4/attempt-ledger`。

第一窗口 12 个测量进程全部完成（16:08–16:31 UTC）。12 个进程的原生环境采样均为
`passed`，已观测中位如下（**只有一窗，不足以形成任何正式裁定**）：

| 单元/指标 | 窗口 1 三进程中位（µs） | 窗内极差/中位 |
| --- | --- | ---: |
| P1-no-consumer / sdk_update | 1.7, 1.6, 1.7 | 5.9% |
| P2-low-delivery / sdk_update | 0.8, 0.8, 0.8 | 0.0% |
| P2-low-delivery / capture_delivery | 2.2, 2.2, 2.2 | 0.0% |
| P4-capture-1 / capture_delivery | 2.4, 2.5, 2.3 | 8.3% |
| P6-sparse / capture_delivery | 16.8, 16.7, 16.8 | 0.6% |

第二窗口在 `p1-no-consumer` 第 2 对的热身阶段停止：`RuntimeMonitor` 记录前台进程从
24,212 变为 0，30 个连续采样均为同一错误，随即按“环境改变或监控不完整不得替样”的
规则结束整组。现场同时刻记录：交流供电为 1、电源方案未变、无构建/测试进程冲突，
`LogonUI`（pid 19,896，会话 1）在运行，DSH 窗口仍存在但不再持有前台，输入空闲约
3.76×10⁶ ms。也就是说这是**交互会话被系统锁定**导致的环境条件失效，不是产品语义、
身份或计数错误，也不是性能超界。

处置按 R4 §4：保留慢值与失败现场，不替换样本、不补成三窗、不把窗口 1 改写成通过；
该 attempt 已消耗，撤销记录不能被“重跑一次”抹掉。`stopped.json`、每个进程的
`*-runtime-environment.json`、原始日志与已完成的 13 个测量进程全部保留。

## 7. 未运行单元的预检

为降低下一批的登记风险，对尚未启动的三个已注册单元各做一次**非正式**全规模身份预检
（不监控、不 claim、不写登记）：`p4-capture-2`、`p4-capture-4`、`p6-topology` 的
预声明身份与探针实际身份**逐字段一致**，包括 topology 的 `reloads`=65、
`delivery_generations`=66 与 `topology_reload` 计数 56。证据在
`out/evidence/r4/probe-20261107/precheck-identity-20261108`。预检只证明登记形状正确，
不构成任何计时或容量结论。

## 8. 取舍记录

本批有两个需要选择的节点，取舍依据如下（优劣按对已接受合同的影响排序）。

**8.1 用哪个批次承接剩余 R4 自动工作**

| 方向 | 利 | 弊 | 结论 |
| --- | --- | --- | --- |
| P1/P2/P4/P6 展示矩阵探针适配 | 原生负载已存在；一次适配覆盖 P1/P2 与 P4 三子单元、P6 两子单元共七个门；B-only 不需要重建 pre-PD A | 每进程 60 秒实际时间，固定组时长最大 | **采用**：覆盖门数最多，且不需要新数值或设备 |
| P0 普通 host A/B | 有直接绝对预算（request→ready p99 ≤4.167 ms） | pre-PD A 的 harness 是 receipt-ACK 边界，与 present-return 不同，需分别重编译 harness 才可配对 | 暂缓：先把口径等价证明做完 |
| 五类 Seek | 五类都是明确门 | 需要 device/seek fixture 与真实端点；adapter 与工具量最大 | 后续批次 |
| 内存归因闭合 | 是 memory/P8 准入的唯一路径 | 两轮诊断额度已用尽，需先提交具体方案 | 后续批次，且需裁定 |
| 新候选完整 F1 矩阵 | 平台门可闭合 | 11 行 × 约 119 GB；且 F2 需要推送，owner 已要求不推送 | 后续批次 |

**8.2 P1-no-consumer 的计时口径**

| 方向 | 利 | 弊 | 结论 |
| --- | --- | --- | --- |
| 把采样改为纳秒、输出小数微秒 | 保留原帧数/消费者数，恢复亚微秒分辨率；所有单元受益 | 修改 fixture 计时，需重新构建与登记 | **采用** |
| 把 P1 降级为纯结构计数 | 不改 fixture | 计划对 P1 的判据本就是结构计数，但放弃 update 成本这一唯一可测时间量 | 备选 |
| 允许中位数为 0 通过统计 | 不改代码 | 破坏 `positive()` 拒绝零值的既有守卫，产生不可判定的“成本 0” | 否决 |
| 用 200 实体构造新的 no-consumer 负载 | 可测 | 发明了原矩阵没有的负载，违反“不用新小夹具替代” | 否决 |
| 申请放宽 1 µs 以下不判定的口径 | 无需实现 | 属于阈值/范围变更，需 owner 裁定，且没有必要 | 否决 |

**8.3 环境停止后的处置**

| 方向 | 利 | 弊 | 结论 |
| --- | --- | --- | --- |
| 保留现场、记录原因、预检其余单元、准备补救设计 | 不伪造通过、不消耗更多额度；下一批风险最低 | 本轮没有正式裁定 | **采用** |
| 立即用同一 preparation 重跑 | 省时间 | 额度和“不得替样”规则不允许；且会话仍锁定必然再失败 | 否决 |
| 修改冻结的监控策略（放宽前台检查） | 可继续采样 | 属于冻结策略的合同变更，代理无权改 | 否决 |
| 改登记一个前台为 0 的“环境” | 形式上可跑 | 在锁定会话中采样等于用不可核对的环境冒充正式证据 | 否决 |
| 切换到不需要前台的批次（如内存诊断） | 可继续工作 | 内存诊断额度已用尽且需先提交方案 | 否决 |

## 9. 阻断条件与恢复前置
当前阻断是环境性的，需要 owner 的物理配合，不是新的授权问题：

1. 交互会话必须保持解锁。被锁定时 `GetForegroundWindow()` 返回 0，冻结的
   `sampled-windows-environment-v1` 策略必然判定环境改变。
2. 采样期间必须让系统不进入屏幕关闭/锁屏。可选处置（按侵入性排序）：
   - 运行前用 `SetThreadExecutionState(ES_CONTINUOUS|ES_SYSTEM_REQUIRED|ES_DISPLAY_REQUIRED)`
     保持显示与系统唤醒，并在采样期低频发送一次极小相对鼠标事件以重置输入空闲计时；
   - 或临时把当前电源方案的显示器/待机超时设为 0、停用屏保，结束后恢复原值。
   两种处置都必须登记实际取值/命令、起止时间与恢复证据，并在**新登记前**用一个等长空闲
   观察窗验证前台 pid 全程不变。
3. 每单元最多一次完整新登记，且必须有可验证处置：`admission.claim` 对 attempt-2 要求
   `rpa.execution.retry-disposition.v1` 与 `verified_remedy_binding`，其中
   `previous_claim_sha256` 必须指向 attempt-1。本轮补齐了这条工具路径
   （`environment_stability.py` 产出补救证据，`prepare_probe.py --retry-remedy` 为每个单元写
   disposition 并绑定上次 claim），但**没有**创建任何 disposition：会话仍锁定、补救无法验证，
   不能把计划写成已完成的验证。
4. 恢复顺序：解锁并保持前台 → 用 `environment_stability.py --seconds 900`（或等长空闲窗）验证
   补救 → 生成四个新 disposition 与 remedy 证据 → 新 preparation/claim/seal → 新固定组。
   旧 attempt-1 与窗口 1 原始字节独立保留，不与新结果合并。

## 10. 不得误写的结论

- 窗口 1 只是披露，不是正式裁定；没有第二、第三窗口就没有重复性门，也没有任何门通过。
- attempt-1 停止的原因是会话锁定，不是产品失败、性能超界或语义不等价。
- 探针单元是 B-only，A 为 N/A；不得把 A 填 0 或据窗口 1 造相对比值。
- `materialize` 的 2 ms 预算仍缺 observer 准入，本轮没有把它记成通过；
  其余探针指标本来就没有已接受的无插桩绝对预算。
- 本轮没有重跑 F1/F2，候选 `00af7f90` 不继承 `24f0a914` 的矩阵或 hosted 结论。
- memory/P8、五类 Seek、P3/P7 与设备、O2 实体操作、生产参数、O3 与 PD 退出、
  Version Gate 的 SDK owner 批准均未完成。

## 11. 证据位置

| 内容 | 位置 |
| --- | --- |
| 候选构建与两个目标的 build binding | `out/evidence/r4/probe-20261107/build-release-b1` |
| pilot、四份 proof 与 preparation | `out/evidence/r4/probe-20261107/probe-pilot-v1` |
| attempt-1 固定组现场与停止记录 | `out/evidence/r4/probe-20261107/formal-run1` |
| 三单元身份预检 | `out/evidence/r4/probe-20261107/precheck-identity-20261108` |
| 正式额度台账 | `out/evidence/r4/attempt-ledger` |
| 本轮附件索引与只读复核 | [本轮证据](2026-11-08-probe-matrix-evidence/index.md) |
