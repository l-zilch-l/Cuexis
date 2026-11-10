# R4 展示矩阵探针准入、pilot 与环境阻断的固定组

状态：candidate；owner 豁免额度后七个探针单元全部取得三窗口结果，六个计时组完成、p4-capture-1 为 inconclusive

日期：2026-11-08

返回 [New RPA 报告索引](../README.md)。当前结论只以
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md) 为准；本文保存本轮的实施、pilot、
两次固定组现场、阻断条件与待 owner 裁定的处置方向。原始字节与复核入口见
[本轮证据](2026-11-08-probe-matrix-evidence/index.md)。

## 1. 授权、候选与边界

owner 的最新有效指示仍是继续 R4 技术工作且**不推送**；本轮没有 push、merge 或 release，
也没有向 PR #32 同步任何提交。技术授权依据
[执行授权](2026-11-05-execution-authorization.json)；数值仍只消费 2026-11-05 已接受的
O1 方案 B，本轮没有新预算、新阈值或新公共语义。owner 在 2026-11-08 明确选择
“现在解锁并继续”，该指示只覆盖继续执行，不构成第三次登记或策略变更的授权。

| 项 | 值 |
| --- | --- |
| 工作树 | `D:/Cuexis-worktree`，分支 `codex/r4-integration` |
| attempt-1 候选 | `00af7f90`（本地，未推送） |
| attempt-2 候选 | `b7edc959`（本地，未推送；含补救工具与文档） |
| 日期版本 | `26.11.07-5`（两次登记相同） |
| SDK API | `0.9.1` preview |
| 本轮产品差异 | 无；只新增准入/准备/补救工具，并把展示矩阵探针的采样时钟改为纳秒分辨率 |

候选源码包含工具、登记文本与 fixture 修订，不含未跟踪或 dirty 文件；`clean_source`
在构建、pilot 与每次正式进程前都会整树校验。F1/F2 平台证据仍只属于较早候选
`24f0a914`，本轮没有重跑矩阵或 hosted，因此**不得**把任一候选写成已通过 F1/F2。

本报告与后续文档提交位于两个候选之后；两次 `preparation`/`seal`/`build binding` 分别绑定
`00af7f90` 与 `b7edc959`，复核这些字节必须检出对应 SHA。报告提交本身不重新冻结证据，
也不继承其结论。

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

两次登记各用一次全新 Release STATIC 构建，规则相同：`--fresh` 配置、`--clean-first` 构建、
`CUEXIS_LIBRARY_TYPE=STATIC`、`CUEXIS_ENABLE_CHART_V5_CANDIDATE=ON`、全部插桩选项 OFF、
普通 allocator、英文编译器输出。

| 项 | attempt-1（`00af7f90`） | attempt-2（`b7edc959`） |
| --- | --- | --- |
| 构建根 | `out/build/r4/probe-20261107/release-b1` | `out/build/r4/probe-20261108/release-b2` |
| 绑定证据 | `out/evidence/r4/probe-20261107/build-release-b1` | `out/evidence/r4/probe-20261108/build-release-b2` |
| 目标 | `cuexis_pd_host_probe`、`cuexis_pd_baseline_probe` | 同左 |
| 工具链 | MSVC 19.51.36256.0 / x64，与 pre-PD A 同签名 | 同左 |
| 探针二进制 | `cuexis_pd_host_probe.exe` 5,718,016 B；`cuexis_pd_baseline_probe.exe` 4,864,000 B | 与左列逐字节相同（无产品源码差异） |

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
绝对预算通过，也不证明端点 p99 或设备条件。attempt-2 换用新候选后重跑同一四单元 pilot，
证据在 `out/evidence/r4/probe-20261108/probe-pilot-v2`；两次 pilot 的中位一致到
0.1 µs 量级，说明构建与计时口径没有漂移，但 pilot 本身仍不是正式样本。

## 6. 固定组：七单元登记与裁定

### 6.1 attempt-1

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

### 6.2 attempt-2：补救验证后重登记

owner 于 2026-11-08 明确选择“现在解锁并继续”。会话解锁后按 §9 的顺序执行：

1. **补救证据**：`environment_stability.py` 在 900 秒窗口内保持
   `SetThreadExecutionState(ES_CONTINUOUS|ES_SYSTEM_REQUIRED|ES_DISPLAY_REQUIRED)`，并按
   240 秒发送一次 1 px 往返相对鼠标事件以重置输入空闲计时。893 个采样中前台恒为 24,212、
   交流供电恒为 1、电源方案与进程集不变，`passed=true`（`remedy-verify-v2.json`）。
   第一次尝试用 2 秒采样间隔时因间隔本身超过冻结策略的 2 秒上限而被判不完整，该 v1 记录保留，
   v2 改用 1 秒间隔。两次尝试之间还有一次失败记录：前台在验证窗口内切到 `explorer` 与
   `ShellHost`，说明该补救只对抗系统空闲，不对抗人工切换窗口。
2. **处置绑定**：`prepare_probe --retry-remedy` 为四个单元各写一份
   `rpa.execution.retry-disposition.v1`，绑定 attempt-1 的 claim sha 与上述补救证据。
3. **新候选与构建**：报告与工具提交后的新候选 `b7edc959`，全新构建
   `out/build/r4/probe-20261108/release-b2`（同一 Release STATIC/普通 allocator/插桩全关配置）。
4. **重登记**：四单元 pilot 重跑（`probe-pilot-v2`）→ `formalize` 一次性 claim attempt-2 →
   `runner` 固定三窗口；同时后台运行同一补救工具（`remedy-run-hold-v1.json`）持有显示并
   逐秒记录环境。

attempt-2 完成了**前两个窗口的全部 24 个测量进程**，第三窗口在 `p1-no-consumer` 第 1 对完成、
第 2 对的采样阶段停止（09:45:33 UTC）。停止原因仍是环境条件：该进程的
`sample-runtime-environment.json` 记录前台在 24,212 与 9,456（`explorer`）之间变化。
同一时刻 owner 报告网络中断并回到本窗口；也就是说这一次是**人工切换窗口**，不是锁屏。

运行期补救记录（`remedy-run-hold-v1.json`，5,556 个采样、23 次 nudge）显示：前 3,641 秒
前台恒为 24,212，说明“不锁屏 + 输入计时重置”确实延长了可用窗口（attempt-1 的锁屏出现在
约 56 分钟空闲处，本次在 60 分钟内没有锁定）；随后在 3,641–3,669 秒出现一串窗口切换，
与 owner 的网络中断处理吻合，之后又在 4,159 秒与 5,510 秒各切换一次。该记录另在
第 165 秒的一个采样点看到 `wpr.exe`（1 秒内消失）：登记用的 `RuntimeMonitor` 未记录该事件，
按冻结规则以登记监控为判据，但这一偶发事件保留披露，不据此改判。

### 6.3 两次尝试的合并披露

两次尝试分别完成 13 与 25 个测量进程：attempt-1 为窗口 1 的 12 个加窗口 2 的 1 个；
attempt-2 为窗口 1 的 12 个、窗口 2 的 12 个加窗口 3 的 1 个。按冻结规则，重复性函数要求
恰好三窗口×三进程，因此**两次尝试都没有正式裁定**；下表只披露 attempt-2 的两个完整窗口，
不与新登记合并，也不构成通过：

| 单元/指标 | attempt-2 窗口 1 | attempt-2 窗口 2 | 窗内极差/中位 |
| --- | --- | --- | --- |
| P1-no-consumer / sdk_update | 1.7, 1.6, 1.6 | 1.6, 1.7, 1.6 | ≤6.3% |
| P2-low-delivery / sdk_update | 0.8, 0.8, 0.8 | 0.8, 0.8, 0.9 | ≤12.5% |
| P2-low-delivery / capture_delivery | 2.2, 2.2, 2.1 | 2.3, 2.3, 2.3 | ≤4.5% |
| P4-capture-1 / capture_delivery | 2.5, 2.5, 2.5 | 2.5, 2.5, 2.5 | 0.0% |
| P6-sparse / capture_delivery | 16.6, 17.9, 16.6 | 17.4, 16.7, 16.7 | ≤7.8% |

两次尝试的窗内重复性都低于 15%，但这只是**两个窗口的披露**：第三窗口缺失，冻结的
“逐窗 + 前六 + 全九”三门没有被评估，也没有任何单元获得正式结论。每单元的两个
attempt 额度至此已用尽（`admission.claim` 对非 P8 单元上限为 2），第三次登记必须由
owner 裁定，不能由代理自行重开。

### 6.4 attempt-3：owner 豁免额度后的完整组

owner 于 2026-11-08 20:05 明确指示：四个额度用尽的探针单元与内存归因/P8 可以继续、不再考虑
额度限制，裁定仍由 owner 作出。该指示记录为
[owner 豁免](2026-11-08-owner-attempt-waiver.json)；`admission.claim` 只在具名 waiver 覆盖该
单元时放宽上限（默认仍是 P8 一次、其他两次），并把 waiver 的 artifact 写入每条 claim，
因此第三次尝试不是静默放行。

候选 `f29f0013`（`engine/`、`app/` 相对 `24f0a914` 零 diff，只新增工具与文档）全新构建
`out/build/r4/probe-20261108/release-b3`；四单元重跑 pilot（`probe-pilot-v3`）、绑定已验证
补救 `remedy-verify-v2.json`、claim attempt-3、执行固定组 `formal-run3`。2026-11-08
20:19–21:52（93 分钟）36 个测量进程全部完成，每个进程的原生环境采样均为 `passed`。

| 单元/指标 | 逐窗极差/中位 | 前六 | 全九 | 状态 | 九进程中位（µs） |
| --- | --- | --- | --- | --- | --- |
| P1-no-consumer / sdk_update | 5.9% / 6.3% / 0.0% | 11.8% | 12.5% | 计时组完成 | 1.6–1.8 |
| P2-low-delivery / capture_delivery | 4.5% / 4.5% / 0.0% | 4.5% | 4.5% | 计时组完成 | 2.2–2.3 |
| P2-low-delivery / sdk_update | 0.0% / 0.0% / 0.0% | 0.0% | 0.0% | 计时组完成 | 0.8 |
| P4-capture-1 / capture_delivery | 12.0% / 4.2% / 4.2% | 12.2% | **16.7%** | **inconclusive** | 2.3–2.7 |
| P6-sparse / capture_delivery | 3.0% / 8.7% / 4.6% | 11.1% | 11.1% | 计时组完成 | 16.6–18.5 |

四个单元的结构计数（14,400 样本、2,400 预热、8:1 更新、256 帧重载、200/201 实体）在解析器中
逐进程通过。三个单元的计时组在三窗、前六和全九三个重复性门全部低于 15%；`p4-capture-1`
的全九为 16.7%，超过 15%，按冻结规则为 inconclusive，不因“只差一点”改判。它的每进程中位只有
2.3–2.7 µs，0.1 µs 的计时分辨率本身带来约 4.2% 的量化步长，九个进程 0.4 µs 的跨度即 16.7%：
这是分辨率限制、不是回退，但同样不能写成通过。

运行期补救记录 `remedy-run-hold-v3.json` 在 80 分钟内只记录一次 4 秒的 `explorer` 前台切换
（21:05，落在两个测量进程之间），没有锁屏；另记录 12 次 `wpr.exe` 冲突，经查是补救工具
自身调用 `wpr -status` 时的自观测误报（见 §6.6）。窗口内进程的 `RuntimeMonitor` 全部通过。

### 6.5 三个未运行单元：attempt-1 全部完成

`p4-capture-2`、`p4-capture-4`、`p6-topology` 各自的 attempt-1 在同一候选 `f29f0013` 上登记
（`probe-pilot-v4`、claim attempt-1），固定组 `formal-run4` 于 2026-11-08 21:56–23:11 完成
27 个测量进程：

| 单元/指标 | 逐窗极差/中位 | 前六 | 全九 | 状态 | 九进程中位（µs） |
| --- | --- | --- | --- | --- | --- |
| P4-capture-2 / capture_delivery | 5.0% / 5.0% / 0.0% | 5.0% | 5.3% | 计时组完成 | 1.9–2.0 |
| P4-capture-4 / capture_delivery | 0.0% / 5.3% / 0.0% | 5.3% | 5.3% | 计时组完成 | 1.9–2.0 |
| P6-topology / capture_delivery | 1.2% / 2.9% / 1.7% | 4.1% | 4.1% | 计时组完成 | 17.1–17.8 |
| P6-topology / topology_reload | 0.9% / 0.6% / 1.5% | 2.2% | 2.2% | 计时组完成 | 4,815–4,921 |

拓扑计数（`reloads`=65、`delivery_generations`=66、`topology_reload`=56）与预检一致。
`remedy-run-hold-v4.json` 的 90 分钟记录里前台全程为 24,212、没有锁屏；它被判 failed 的原因是
本批结束后立即启动的内存构建（cl/ninja/cmake，23:11:16–23:17:38）与同一个 `wpr` 自观测，
两者都发生在 27 个测量进程结束（23:10:35）之后，不污染任何样本。
三个单元在三个重复性门上全部低于 15%，`topology_reload` 的九进程中位约 4.9 ms 也保持 2.2%
的重复性。至此七个已注册探针单元全部有三窗口正式结果：六个计时组完成、`p4-capture-1`
仍为 inconclusive。

### 6.6 两个工具缺口

1. 补救工具的冲突扫描在 `no_trace()` 之后运行，`wpr -status` 的子进程尚未退出时会被记成
   `wpr.exe` 冲突（本轮两份记录共 12 次）。修复方式是把冲突扫描移到 `no_trace()` 之前；
   判据、阈值与环境策略不变，修复前的记录保留原样并标注为自观测误报。
2. 整组停止时原 `runner.py` 丢弃已算出的逐进程绝对门，本轮改为写入 `partial_absolute_gates`。

## 7. 身份预检

三个后续单元在正式登记前各做过一次非正式全规模身份预检（`precheck-identity-20261108`），
逐字段一致；它们在 2026-11-08 已按 §6.5 正式登记并完成，预检只作为登记形状的旁证。

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
| 立即用同一 preparation 重跑 | 省时间 | 额度已用尽且“不得替样”；停止原因未消除会再失败 | 否决 |
| 修改冻结的监控策略（放宽前台检查） | 可继续采样 | 属于冻结策略的合同变更，代理无权改；已按 9.C 交 owner 裁定 | 否决（代理不自行改） |
| 改登记一个前台为 0 的“环境” | 形式上可跑 | 在锁定会话中采样等于用不可核对的环境冒充正式证据 | 否决 |
| 切换到不需要前台的批次（如内存诊断） | 可继续工作 | 内存诊断额度已用尽且需先提交方案 | 否决 |

## 9. 仍待 owner 裁定的事项

探针矩阵本身已经跑完，剩下的不是执行而是裁定：

| 事项 | 现状 | 可选项 |
| --- | --- | --- |
| `p4-capture-1` 的重复性 | 三窗口完成，全九 16.7% > 15% → inconclusive；绝对值 2.3–2.7 µs 稳定 | (A) 接受 inconclusive，作为“分辨率受限”记录；(B) 以新登记再跑一次并在跑前固定停止规则；(C) 修改该单元的判据/采样长度（协议变更） |
| 六个计时组完成的单元 | 结构计数与三种重复性门全部通过；没有已接受的探针绝对预算 | (A) 接受为“结构计数 + 重复性”证据；(B) 为探针指标补一个已接受预算（需新数值接受） |
| 内存归因 | 见[内存归因报告](2026-11-08-memory-attribution.md)：逐域精确归因，`state.current` 在 n64 超提议上限 2.17× | (A) 修正提议系数/固定项后重新接受；(B) 先做存储整改；(C) 保持 unrated |
| 候选 C 与 F1/F2 | 仍是旧候选的证据 | 选最终候选 SHA，再决定是否恢复推送授权以跑同 SHA hosted |
| Version Gate | `version.sdk_api.approval_invalid` | owner 对最终候选给出精确批准 |

代理的建议在[门账更新](2026-11-08-r4-gate-status.md)与各报告末尾；所有裁定保持 owner 权限。
## 10. 不得误写的结论

- 两个窗口的披露不是正式裁定：第三窗口缺失，`repeatability()` 的“逐窗 + 前六 + 全九”三门
  一条都没有评估，没有任何单元通过或超界。
- 两次停止的原因分别是会话锁定与人工切换前台窗口，都不是产品失败、性能超界或语义不等价。
- 探针单元是 B-only，A 为 N/A；不得把 A 填 0 或据披露窗口造相对比值。
- `materialize` 的 2 ms 预算仍缺 observer 准入，本轮没有把它记成通过；
  其余探针指标本来就没有已接受的无插桩绝对预算。
- 本轮没有重跑 F1/F2，候选 `00af7f90`/`b7edc959` 都不继承 `24f0a914` 的矩阵或 hosted 结论。
- 每单元两次正式额度已用尽；第三次登记属 owner 决定，代理不自行重开。
- memory/P8、五类 Seek、P3/P7 与设备、O2 实体操作、生产参数、O3 与 PD 退出、
  Version Gate 的 SDK owner 批准均未完成。

## 11. 证据位置

| 内容 | 位置 |
| --- | --- |
| attempt-1 构建与两个目标的 build binding（`00af7f90`） | `out/evidence/r4/probe-20261107/build-release-b1` |
| attempt-1 pilot、四份 proof 与 preparation | `out/evidence/r4/probe-20261107/probe-pilot-v1` |
| attempt-1 固定组现场与停止记录 | `out/evidence/r4/probe-20261107/formal-run1` |
| 三单元身份预检 | `out/evidence/r4/probe-20261107/precheck-identity-20261108` |
| attempt-1 的证据清单（193 文件） | `out/evidence/r4/probe-20261107/evidence-manifest.json` |
| attempt-2 构建与绑定（`b7edc959`） | `out/evidence/r4/probe-20261108/build-release-b2` |
| 补救验证 v1（2 秒间隔，判不完整，保留）与 v2（1 秒间隔，通过） | `out/evidence/r4/probe-20261108/remedy-verify-v1.json`、`remedy-verify-v2.json` |
| attempt-2 pilot、四份 proof、四份 retry disposition 与 preparation | `out/evidence/r4/probe-20261108/probe-pilot-v2` |
| attempt-2 固定组现场与停止记录 | `out/evidence/r4/probe-20261108/formal-run2` |
| 运行期补救持有记录（整组期间逐秒环境） | `out/evidence/r4/probe-20261108/remedy-run-hold-v1.json` |
| 正式额度台账（四个单元各 attempt-1/2） | `out/evidence/r4/attempt-ledger` |
| 本轮附件索引与只读复核 | [本轮证据](2026-11-08-probe-matrix-evidence/index.md) |