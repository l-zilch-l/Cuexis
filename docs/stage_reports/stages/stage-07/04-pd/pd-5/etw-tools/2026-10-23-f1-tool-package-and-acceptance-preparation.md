# PD-5 F1 工具包合并封包与三用例验收准备

日期：2026-10-23。分支：`codex/pd5-gameplay-40k-admission`（HEAD `3e5837f`）。主工作树
`D:/Cuexis-worktree`（`stage-7`，HEAD `eed3664`）保持干净，未参与本轮改动。

本记录只覆盖正式测量前的工具修复与封包准备。管理员已在 r2、r3 各真实运行一次：r2 在 collector
解析合同上失败，r3 的头两项用例通过、第三项在死亡证明上崩溃；两次失败都已按工具缺陷修复并新建
封包，未重跑。当前在用封包是 **r4**，管理员已对其运行一次并三项全过（启动器退出 0，见
`## r4 管理员运行结果`）；r4 因此也进入"已运行、不得重跑"状态。F1 三项用例全绿只证明工具合同成立，
任何性能判定和 PD-5 测量开始都不在本记录的证据范围内。

## 结论

1. F1 collector 已接入修复后的控制器，合并为一个自校验工具包：一个 `binding.json`、
   一个 `bootstrap.py`、两个固定角色（`canary`、`f1`）、两个 PowerShell 启动器。
2. 旧的 F1 包装器合同要求 collector 收据里的 `wrapper_script_sha256`，而只有旧 PowerShell
   包装器能诚实写入该字段，因此修复后的控制器无法签字。合并封包用
   `controller_source_sha256` + `controller_claim_sha256` 取代它，溯源链不再依赖包装器。
3. `bootstrap.py` 现在在 import 控制器**之前**复核每个交付文件（含 `plan.json`）。此前被篡改的
   计划只会在 WPR 启动之后由 collector 拒绝；该窗口已关闭。
4. 新增 `tests/pd/pd5_etw_f1_scale_tests.py` 等 5 个测试文件，`pd5_etw*_tests.py` 发现集共
   **121 项全绿**（本地约 2.1 s）。计划与用例表的一致性、claim 合同、收据签发、三条工作负载路径、
   profile 选择、preflight、判定，以及后来补上的死亡证明、身份回收与输入 pin 都有断言。
5. 真实封包已生成并完成只读核验：`out/pd5-etw-f1-tools-20261023-3e5837f-r2`，
   `binding.json` `e2b2711d…`；未启动 WPR、未执行任何角色。第一版只因启动器横幅把
   harness 退出码与控制器退出码混写而作废（未运行，目录保留为 `…-superseded-banner-only`）。
   管理员随后真实运行了 r2 与 r3，二者都暴露工具合同缺陷（见下文两节），因此当前在用封包是
   **r4**（`binding.json` `5cce9b67…`）；管理员已对 r4 运行一次，三项全过、启动器退出 0。
   当前不存在"待运行"的封包，也不存在需要重跑的封包。

## 为什么合并而不是续用旧包

| 旧包 | 合同 | 结果 |
| --- | --- | --- |
| `out/pd5-etw-lifecycle-minimal-20261023` | binding v2，仅 canary | 历史有效，保留原样 |
| `out/pd5-f1-noise-control-v3-20261023-e6959a9` | `pd5.f1-noise-control-binding.v1`，包装器签发 | 不可与修复后控制器拼接 |

封包是 create-new 且不可变。新包不与旧包共享文件，旧包的 binding 与运行记录仍按原时点解释。

## 交付内容

封包内 12 个模块、`README.md`、`lifecycle.wprp`、`f1_scale.wprp`、`plan.json`、
`binding.json` 和两个启动器：

```text
bootstrap.py  cases.py  collect_f1_scale.py  common.py  controller.py  etw.py
f1_scale.py  flow.py  job.py  policy.py  profile_config.py  worker_gate.py
README.md  lifecycle.wprp  f1_scale.wprp  plan.json
binding.json  run_tool_canary.ps1  run_f1_scale.ps1
```

`binding.json` 使用 `pd5.etw-tool-canary.binding.v3`，显式记录
`formal: false`、`formal_acceptance: false`、`performance_acceptance: false`、
`preparation_revision: 4`、`native_launches: 0`，以及：

- `sources`：逐模块 SHA256，集合必须与封包内模块完全一致；
- `controller_source_sha256`：控制器哈希，收据与 claim 都以它为准；
- `artifact_sha256`：每个交付文件（含 `plan.json`、README、两个 `.wprp`）的 SHA256；
- `input_sha256`：frozen 输入、支持文件、运行时资产、解析证据共 15 项；
- `profiles`：`canary` 与 `f1-scale` 各自的文件名、profile 名与 SHA256；
- `cases` / `case_envelopes`：五项固定用例的封包时快照；
- `wpr`（路径、哈希、观察到的版本）与 `python`（路径、哈希）。

启动器不入 `binding.json`：它们内嵌 binding 哈希，若被哈希会形成循环。启动器身份记录在
封包摘要与新报告中。

## 溯源链

```text
launcher  → Get-FileHash 核对 binding.json 与 bootstrap.py
          → python -B -I -S -u bootstrap.py <role> <新任务目录>
bootstrap → schema / 源集合 / profiles / controller 哈希
          → artifact_sha256 全量复核（含 plan.json）
          → 用例表 / 解释器身份 / wpr.exe 哈希
          → import controller; controller.main(here, binding)
controller→ f1-owned-capture.json：controller_source_sha256 + controller_claim_sha256
collector → 用 binding 与命令行 claim 双向核对，二者不符即拒绝
```

任一环节不符即 `FAILED BEFORE ROLE` 或直接拒绝，退出码 2，不产生 WPR 动作。

## 三用例与其证据要求

| 用例 | 控制器退出码（harness 的判定条件） | 必须证明 |
| --- | --- | --- |
| `f1-scale` | 0 | 6 次 native 启动全部完成、3 个进程完成、Job 排空为 0、正常命名停止、两个精确名查询与提权 `QueryAllTraces` 缺席 |
| `f1-scale-timeout` | 74 | 预热 20 s 处杀死 collector、Job 排空、每个观测到的 native PID 由进程身份证明已死、有序停止与缺席证明 |
| `f1-scale-death` | 73 | 死亡瞬间存在存活 native 身份、守护者接管共享 Job、控制器与守护者都不存在后该 PID 被证明已死 |

启动器返回的是 harness 退出码：三项全部通过为 `0`，任一项不成立为 `1`（`FAILED/UNRESOLVED`）。
74 与 73 只是对应用例内部被检查的控制器退出码，写在该用例的 `result.json`；启动器退出 74 或
73 不表示通过。

关键机制：collector 在自己的内层 Job（`process_control.py`，`KILL_ON_JOB_CLOSE`）里启动
native F1 可执行文件，因此该子进程不属于外层采集 Job。仅看外层 `job.active() == 0` 不足以
证明 native 子进程已回收，必须同时用 collector 自身 native 日志里 `PD5_PROCESS_START` 锚点
逐 PID 证明。死亡用例用 `os._exit(73)` 跳过 case 的 `finally`，守护者自持 Job 句柄，
`KILL_ON_JOB_CLOSE` 只在最后一个句柄关闭时生效，因此仍能排空。

## 数值真源

`plan.json` 是 F1 数值的唯一真源，封包时与 `etw_safety/cases.py` 交叉核对，测试再复核一次：

| 项 | F1 | canary（不变） |
| --- | --- | --- |
| 整次采集硬上限 | 420 s | 120 s |
| 守护者接管 | 380 s | 95 s |
| 磁盘阈值 | 2 GiB | 128 MiB |
| collector 预算 | 250 s | — |
| 监督者工作负载预算 | 300 s（250 + 50 宽限） | — |
| WPR 启动 / 停止 | 20 s / 110 s | 15 s / 60 s |
| harness 监督 | 480 s | 135 s |
| native 启动 / 进程 / mask / 预热 | 6 / 3 / 4 / 30 s | — |

健康工作负载由 collector 预算约束，只有挂死的 collector 才会触及监督者预算。这些全部是工具
上限，不是产品预算，不能复用为产品预算。

## profile 验证

`PD5F1Scale` 由 `f1_scale.wprp` 定义：一个 128 KiB/64 缓冲的系统收集器（CSwitch、DPC、
IdleStates、Interrupt、Power、ReadyThread），一个 128 KiB/32 缓冲的事件收集器，
`Microsoft-Windows-Kernel-Processor-Power` 关键字掩码 `0x2ff`。

初次生成时掩码写作 `0xffffffffffffffff`，`wpr -profiledetails` 把它报告为 `0xff`；改成显式
`0x2ff` 后重新验证，退出码 0，解析出 `PD5F1Scale.Verbose.File`。封包绑定该文件哈希，
preflight 与启动使用同一 XML 与 profile 名；若 `f1-scale` 解析到生命周期 profile 名，
bootstrap 直接拒绝。

## 测试

```powershell
& C:\msys64\ucrt64\bin\python.exe -B -m unittest discover -s tests/pd -p 'pd5_etw*_tests.py' -v
```

共 **121 项**：`pd5_etw_controller_tests.py`（27：控制器、ETW 身份、生命周期 profile、输入 pin
前置校验）、`pd5_etw_flow_tests.py`（12：`take_over` / `released_clean` 纯顺序）、
`pd5_etw_job_tests.py`（13：命名 Job、gate、嵌套子进程回收、`(pid, creation_time)` 身份获取）、
`pd5_etw_f1_scale_tests.py`（53：计划↔用例表、claim 合同、收据、工作负载/超时/死亡路径、
启动记录解析、身份捕获与回收、profile 选择、preflight、判定与派发）、
`pd5_etw_collect_f1_parse_tests.py`（16：collector 的行/内容/锚点解析合同与工作负载形状，见下文
r2 一节）。测试不启动 WPR、不需要管理员、不执行 Cuexis 或 F1。

编写测试时修正了两处实现：控制器把 canary/F1 的横幅与通过文本提为模块常量以便断言；
`f1_scale.live_reference` 把超范围的 PID 视为"不存在"而不是抛异常，避免一条损坏的启动记录
中断回收证明。

## 机制演练

在 `%TEMP%\pd5-etw-tools-rehearsal-2` 演练了一次完整封包：15 项 pinned 输入全部存在且未变、
计划↔用例表交叉核对通过、生成两个启动器；bootstrap 接受门返回 0（12 个模块、16 个交付文件），
三个篡改门分别以 `prepared artifact changed: plan.json`、`prepared artifact changed: f1_scale.py`、
`unexpected sealed profile binding: f1-scale` 拒绝。演练目录在仓库外且随后源码又有改动，
因此该 binding 已失效，真实封包必须重新生成；演练不作为任何验收证据。

## 正式封包与只读核验

```powershell
python -B tools/verification/playback_performance/prepare_etw_tools.py `
    --output out/pd5-etw-f1-tools-20261023-3e5837f-r2 `
    --wpr "D:\Windows Kits\10\Windows Performance Toolkit\wpr.exe"
```

| 身份 | 值 |
| --- | --- |
| 封包（r2：已运行，因下文解析缺陷被 r3 取代） | `out/pd5-etw-f1-tools-20261023-3e5837f-r2` |
| `binding.json` | `e2b2711dc410a44be89614e89e81700f7dbc5f96766d8f550737db298abf03f5` |
| `run_tool_canary.ps1` | `cc03cdf61966e12c884322610a81ad4fc9afc7794fddffe219e85ab7e2d8530c` |
| `run_f1_scale.ps1` | `ea6c753888f1e0cee983bc5fa7dd12734556f37c64431eb1a669eb29da27fa9d` |
| `controller.py` | `350550ef68eef4a4c1dfae33415f1185919d93a9f1c38f4fa9260389ffbc96dc` |
| `f1_scale.wprp` | `3dfa6d65da03d6c98114f2c0da3204ac82a3063e372bc45f152315c527d7ea47` |
| `wpr.exe` | 观测版本 `10.0.26100 (CoreSystem)`，哈希与前批一致 |

第一版封包 `out/pd5-etw-f1-tools-20261023-3e5837f`（`binding.json` `32248b57…`）在只读核验后
发现一处文案缺陷：`run_f1_scale.ps1` 的横幅写成「exit 0/74/73」，会被读成启动器退出码，
而启动器实际返回 harness 的 `0`（三项全过）或 `1`（`FAILED/UNRESOLVED`）；0/74/73 是对应
用例内部被检查的控制器退出码。它**未运行**即被 r2 取代，目录改名为
`out/pd5-etw-f1-tools-20261023-3e5837f-superseded-banner-only` 保留，不得运行。两版的
`controller.py` 与 `f1_scale.wprp` 哈希相同，说明差异只在启动器横幅与 README，源码和计划未变。

只读核验（`executed: false`，未启动 WPR、未执行任何角色）：16 个交付文件与 12 个源模块的
SHA256 全部匹配 `binding.json`；15 项 pinned 输入存在且未变；解释器与 `wpr.exe` 哈希匹配；
canary 与 `f1-scale` 两个 profile 的文件名、profile 名与哈希匹配；封包内 `bootstrap.py`
的闸门在注入替身控制器的条件下返回 0，且把封包根目录交给控制器；用封包内 `plan.json`、
`cases.py`、`profile_config.py` 重新执行 `cross_check` 通过；封包内的用例包络为
420/380/2 GiB/20/110/480，工作负载预算 300 s（超时用例 20 s），控制器退出码 0/74/73，
`formal_acceptance` 与 `performance_acceptance` 均为 false，canary 保持 120/95/128 MiB；
`run_f1_scale.ps1` 的横幅明确区分启动器退出码与控制器退出码。

F1 用例的包络在 r4 起把 `native_launches` 固定为 **6**（`cases.F1_SCALE` 与 `binding.json` 的
`{canary: 0, f1-scale: 6}` 一致），该数字来自冻结 exe 对 3 个进程各写 2 条
`PD5_PROCESS_START` 的实测形状；控制器仍然拒绝任何预先声明启动数的 F1 claim，所以数字只能由
封包内的计划、collector 运行记录与控制器判定共同证明，claim 既不能放宽也不能收紧它。该字段是
**计划包络**而不是观测值：超时用例只允许启一个 native，其观测数记在 `workload.native_launches`
与 `f1-workload-timeout.json` 里，`case_passed` 也不读顶层字段（r4 运行时已按此核对）。

篡改演练在仓库外的临时副本进行：把副本 `plan.json` 的整次采集上限改成 900 s 后，
封包内 `bootstrap.py` 在任何角色动作之前以
`RuntimeError('prepared artifact changed: plan.json')` 和退出码 2 拒绝，副本随后删除。
真实封包未被修改。

## r2 管理员运行失败：内容记录合同与 r3 修复

管理员在 r2 内真实执行了一次 `run_f1_scale.ps1`，启动器退出 **1**，证据根为
`out/pd5-etw-f1-tools-20261023-3e5837f-r2/f1-20261023T122949Z-8ca88299bbd24d30a8c40ba75ec191b5/`。
该运行**未重跑**，全部回执与 143 MB 原始 ETL 保留原处。

第一项 `f1-scale` 的结果是 `FAILED`：`RuntimeError('f1-scale failed: controller=1, guardian=0,
reason=controller exited; no retry and no next case')`，根因是封包内 collector 的
`ValueError: expected 24 rows and 4 contents, got 24 and 8`。

**生命周期链路本身工作正常**（这是失败运行仍然给出的有效证据）：

- WPR 启动成功，命名状态可读（`10.0.26100`，dropped 事件 0），guardian 就绪，harness 附着，
  提权授权回执 `administrator_token: true`；
- collector 退出 1 后控制器判定失败、guardian 接管（`takeover: true`，`reason: controller exited`），
  有序 STOP：两个精确 logger 名
  `WPR_initiated_PD5F1Noise_6a0feaf0…_WPR Event Collector` / `…_WPR System Collector`
  各以 code 0 停止，一次性 STOP helper `helper_exit_proved: true`（pid 19148，exit 0）；
- 停止后提权 `QueryAllTraces` 中 PD5 会话数为 0、`sessions_absent: true`、`active_descendants: 0`，
  `released_clean: false` 只是因为控制器自己先退出、由 guardian 接管，与设计一致；
- `result.json` 写入 `formal: false`、`instruction: DO NOT RERUN`，没有重试也没有进入下一用例。

**缺陷在 collector 的解析合同，不在采集机制**：冻结的 F1 可执行文件（`c17f8ddb…`）对每个 objects
档输出**两条** `PD_FOLLOW_CONTENT`（读块前后各一条，二者 `sha256` 相同，证明读不改内容），因此真实
形状是 24 行 + **8** 条；而 r2 的 `validate_output` 按"每档一条"写成 4 条，并按 `len({objects}) != 4`
把成对的第二条判成"重复内容记录"。同一份日志里 24 行、2 条 `PD5_PROCESS_START`、掩码 4 的
`PD_ENV` begin/end、`All tests passed (109 assertions in 1 test case)` 全部符合预期。

审计结论：88 项测试里**没有任何一项**覆盖 collector 的解析合同（`validate_output`、
`validate_anchors`、`marker_records` 均无测试），解析器只按阅读印象写成，从未与真实 exe 的输出对齐。
这是封包流程的漏洞，不是测量结果、也不是产品失败。

r3 的修复（`collect_f1_scale.py` 单文件）：

- `validate_content_identity`：要求 8 条、顺序为每个 objects 档一对、每对 `sha256` 相等且为 64 位摘要，
  失败信息直接给出观测到的条数；
- `validate_workload_shape`：在 `verify_bindings` 内校验 `plan.json` 的 objects/modes/blocks/counts
  与本解析器的固定序列一致，形状不符时在解析前就停止，而不是产生误导性失败；
- 失败分支也把已写出的每进程记录计入 `native_start_records`，失败回执不再丢失启动证据
  （r2 的失败回执里该字段是 0，尽管日志有 2 条启动记录）。

新增 `tests/pd/pd5_etw_collect_f1_parse_tests.py`（16 项）：用 `plan.json` 生成真实形状的合成日志并
断言通过；拒绝"每档一条"、顺序错、成对摘要不等、缺行、操作数漂移、读块过短、块重复、锚点数量错、
掩码非 4、坏 JSON 等形状；并**直接读取上面这份被保留的真实失败日志**，断言它现在通过新合同。

只读核验通过 r3（`executed: false`，未启动 WPR）：

| 身份 | 值 |
| --- | --- |
| 封包（r3：已运行，因死亡证明缺陷被 r4 取代） | `out/pd5-etw-f1-tools-20261023-3e5837f-r3` |
| `binding.json` | `82f3c30b6f90c07d7dec21ac719ae22a99ff2b83fa2ec9796f5b72e27d65b431` |
| `run_tool_canary.ps1` | `0689bf352b5f2ec3e379d492f23f7f137af1986fec999229479b944eb2bafe58` |
| `run_f1_scale.ps1` | `e52584b9c7d8c5de500f40518a055560834b3e2a1cf590b410b9398d913ea8b3` |
| `collect_f1_scale.py` | `d9aa30b8d9510f3be26929677cdc50f5c0da83313bf5d3828eb37f62ca6100cc` |
| `controller.py` | `350550ef68eef4a4c1dfae33415f1185919d93a9f1c38f4fa9260389ffbc96dc`（与 r2 相同） |
| `f1_scale.wprp` | `3dfa6d65da03d6c98114f2c0da3204ac82a3063e372bc45f152315c527d7ea47`（与 r2 相同） |

r2 与 r3 的 `artifact_sha256` 逐项比较后**只有 `collect_f1_scale.py` 一个交付文件不同**，15 项 pinned
输入与三个用例包络完全一致；`preparation_revision` 仍是 4，计划里的固定数字没有变动。

r2 目录**不改名、不编辑**：它现在保存着那次真实运行的证据，改目录名会让回执里的绝对路径失效；它只是
不再作为运行入口，不得重跑。r3 是新的 create-new 目录，`binding.json` 不同，等价于新的封包。

## r3 管理员运行结果：两例通过，死亡用例崩溃

管理员在 r3 内真实执行了一次 `run_f1_scale.ps1`，启动器退出 **1**，证据根为
`out/pd5-etw-f1-tools-20261023-3e5837f-r3/f1-20261023T124907Z-ab3affecfe0342eb973aca0f1e68a780/`
（本地 20:49:08–20:52:09）。根 `result.json` 为 `status: FAILED`，`cases[]` 只有前两项，
`instruction: DO NOT RERUN`。该运行**未重跑**，全部回执与原始 ETL 保留原处。

**用例 1 `f1-scale` 通过（控制器退出 0）**：`COMPLETE`、`released_clean: true`，3 个测量进程全部
完成、6 条启动记录，ETL `401473536` 字节（`bae48072…`）；每份日志 24 行 / 8 条
`PD_FOLLOW_CONTENT` / 2 条 `PD_ENV` / 2 条 `PD5_PROCESS_START`，以
`All tests passed (109 assertions in 1 test case)` 结束。这说明 r2 的解析合同修复在真实输出上成立。

**用例 2 `f1-scale-timeout` 通过（控制器退出 74）**：`F1 WORKLOAD TIMEOUT; RECLAIMED;
ORDERLY STOP COMPLETED`，native pid 28288 在终止时被判为存活、终止后 `proved_dead: true`，
ETL `68026368` 字节（`cdba8b63…`）；20 s 工作负载预算与 30 s 预热之间的残余时序风险没有发生。

**用例 3 `f1-scale-death` 失败：这是工具缺陷，不是测量结果，也不是产品失败。**
控制器以 `OSError(22, 'OpenProcess', None, 87)` 崩溃，根因两处：

1. 死亡证明对一个**已经退出**的 native PID 重新 `OpenProcess`。该 PID 已不存在时系统返回
   87，而旧实现只把"打不开"当作异常；在控制器自己还持有句柄的场景里这一点被掩盖过。
2. `expected-controller-death.json` 只由 canary 分支写出，F1 死亡用例从不写它，因此即使死亡证明
   不崩溃，`case_passed('f1-scale-death')` 也永远无法为真。

guardian 接管本身按设计工作：`takeover: true`、`released_clean: false`（控制器先死，由 guardian
接管）、`sessions_absent: true`、`active_descendants: 0`，两个精确 logger 名的 STOP 退出码均为 0；
但该用例目录没有 `f1-native-reclaimed.json`、也没有本用例的 `result.json`。

独立只读核验（不运行工具）对 r3 的结论是 **NO SEAL**，并额外指出两处诚实性缺口：

- 正常用例（用例 1）的"回收"只按外层 Job 归零判断，而 native 运行在 collector 自己的内层 Job 里，
  外层 Job 归零不能证明这些 native 已退出——那只是推断，不是证明；
- 15 项 `input_sha256` 只在 WPR 已经启动之后由 collector 复核，采集已经发生，前置信任不成立。

## r4 修复与封包

r4 只改工具，不改计划里的任何预算、阈值、采样或统计数字：

1. `job.py` 新增 `ProcessRef.acquire(pid, creation_time)` 与公开的 `GONE_WINERRORS = {2, 87, 1168}`：
   `OpenProcess` 返回这三个错误或测得创建时间不匹配，即判定**该进程实例已不存在**（这是可靠的死亡
   证明）；访问被拒仍然抛出，不伪装成死亡。`(pid, creation_time)` 唯一命名一个进程实例，所以不需要
   保留句柄——恰恰是保留句柄才会让一个已死的 PID 仍可被打开。
2. `controller.death_proof` 改用 `acquire`，在回执里写入 `identity_now`、`proved_dead`、`basis`
   与 `checked_after: 'controller exit and guardian drain'`；guardian 附着也改用 `acquire`，
   返回 `None` 时拒绝做成功推断。
3. `f1_scale.write_death_intent` 在 `os._exit(73)` 之前写出 `expected-controller-death.json`，
   F1 死亡用例因此可以真正通过。
4. 正常用例改为**真证明**：`observe_identities` 在 native 存活期间按启动记录捕获每个
   `(pid, creation_time)`（不保留句柄），collector 退出后逐个 `acquire` 复核，写
   `f1-native-identities.json` 与 `f1-native-reclaimed.json`；捕获不足 6 个身份或有身份仍存活时
   fail closed（`native identity captured for only N of 6 launches` / `native F1 processes
   survived the sealed collector`），不再用 Job 归零冒充回收。
5. 输入信任前置：`controller.verify_inputs` 在 `case()` 的最前面重算全部 15 项 `input_sha256`，
   在 `Job.create` 之前、任何 WPR 启动之前完成，并写 `input-pins.json`；collector 事后复核保留为
   纵深防御。
6. 元数据与真实输出对齐：`cases.F1_SCALE["native_launches"]` 由 `null` 改为 `6`、
   `controller-death` 包络退出码为 `[73]`，`binding.json` 记录 `{canary: 0, f1-scale: 6}`，
   `plan.json` 的 stop 形状改回真实文件名 `f1-scale.etl`，README 改为"工具不改变电源计划、系统
   亲和性或 DiagTrack，只报告封包内工作负载对自身进程施加的固定掩码 4"，
   `prepare_etw_tools.py` 的启动数交叉核对不再是一条恒真的链式比较。

只读核验通过 r4（`executed: false`，未启动 WPR、未执行任何角色）：

| 身份 | 值 |
| --- | --- |
| 封包（已运行一次） | `out/pd5-etw-f1-tools-20261023-3e5837f-r4` |
| `binding.json` | `5cce9b679940ea922c526d962fe388db37d372e3905becf1df9196142bbb37d0` |
| `run_tool_canary.ps1` | `1327a6b8e822c57393d799dfa841df826e759a9678ba416af49a91e86636391a` |
| `run_f1_scale.ps1` | `05285f443fa95e1973f84dc047d5601f4ec37ac92e5dbb1105138f242b749847` |
| `controller.py` | `6d88a7a5c798acae47f5aa28d4bbc1475ef698464ee9ad0fb270c4b3dcdf4d5f` |
| `f1_scale.py` | `58ba4eee74c1e471a83b3bbe32254de41bfcbfc0f91cf5b033a789956b44e2ba` |
| `job.py` | `a3de0bd5a120fa5bb9f6c26d39dee6e734a24029ba5a16de1f8ae2148dfb0558` |
| `cases.py` | `0b7273ee343b27779bf1d25b3d472c44b5367ddd4f59791c6516bd2b62429a83` |
| `plan.json` | `b2ae51f48590542d1d40b200d49fa1bef61ba172c238d19908ede148e077c467` |
| `collect_f1_scale.py` | `d9aa30b8d9510f3be26929677cdc50f5c0da83313bf5d3828eb37f62ca6100cc`（与 r3 相同） |
| `f1_scale.wprp` | `3dfa6d65da03d6c98114f2c0da3204ac82a3063e372bc45f152315c527d7ea47`（不变） |

r4 的 16 个交付文件、12 个源模块、15 项 pinned 输入与 2 个 profile 全部哈希匹配，且封包内文件与
工作树源码逐字节一致；封包内没有证据目录。r2、r3 两个目录**不改名、不编辑**（回执里嵌着绝对路径），
它们已被真实运行过，只是不再作为运行入口，不得重跑；"不得运行"这一约束由外部操作纪律保证，
封包内部没有也不需要运行拦截器。

另一次独立只读核验（同样不运行工具、不启动 WPR、不需要管理员）对 r4 的结论是 **SEAL**：16/16 交付
文件、12/12 源模块（与工作树逐字节一致）、15/15 pinned 输入、2/2 profile 全部重算匹配，两个启动器
各自钉住同一 binding、`bootstrap.py` 与解释器哈希，封包内无任何证据目录；并逐条确认 r3 的四处缺陷
都在身份层修好而不是挪位，`verify_inputs` 确实在唯一的 WPR `-start` 之前，死亡证明没有"报死实活"
或"报活实死"的窗口。剩余风险全部是**可用性**而非假通过，按等级记录为 P2：20 s 超时预算对
collector 启动成本敏感（实测余量约 17 s）、用例 1 若任一测量进程打满 90 s 上限会使
3×(45+90) s 超出 250 s、`verify_inputs` 与 WPR 启动之间的 TOCTOU 只会"先采集后失败"、同一工作负载
内 PID 复用会让身份计数不足而 fail closed；P3 为本地化前缀门禁与非英语宿主相关的文案假设。这些都
只能让这一次运行失败，不会伪造绿灯。

## r4 管理员运行结果：三项全过，启动器退出 0

管理员在提权终端内对 r4 执行了一次 `.\run_f1_scale.ps1`。证据根为
`out/pd5-etw-f1-tools-20261023-3e5837f-r4/f1-20261023T132112Z-a50598df63f74213be2c13d41a33999f`，
整轮 176.4 s（本地 21:21:12–21:24:09），根 `result.json` 记 `auto_retry: false`，未重试、未补样。

| 用例 | 控制器退出 | 判定 | 现场证据 |
| --- | --- | --- | --- |
| `f1-scale` | 0 | PASS | 3 个测量进程各 45.1 s、24 行、2 次启动、掩码 4；6 个 `(pid, creation_time)` 在存活期捕获后全部 `identity_now: absent`；合并 ETL 174,063,616 B |
| `f1-scale-timeout` | 74 | PASS | native 30004 在击杀时存活、排空后 `[]`，`job_active_after_drain: 0`；合并 ETL 29,884,416 B |
| `f1-scale-death` | 73 | PASS | 死亡意图在停表前写出；guardian `takeover: true`、`active_descendants: 0`、`sessions_absent: true`、`raw_trace_only: true`；native 10328 在 controller 退出与 guardian 排空后 absent |

逐项收据（我另外重算，不只读工具自报）：

1. **正常用例**：`f1-workload.json` 记 `status COMPLETE`、`exit_code 0`、`native_launches 6`、
   `process_count_completed 3`；`f1-native-identities.json` 的 6 条身份带 `handles_retained: false`，
   `f1-native-reclaimed.json` 逐条 `proved_dead: true`、`native_pids_alive_after_collector: []`；
   guardian `released_clean: true`、`sessions_absent: true`。三个 `f1-scale-*.log` 的 sha256 与
   `f1-workload.json` 登记值逐一相符，合并 ETL 的 sha256 `facc7b87…` 与 `result.json.trace` 相符。
2. **超时用例**：`f1-workload-timeout.json` 记 `supervisor_budget_seconds 20`、
   `active_job_processes_at_expiry 6`、`native_pids_alive_at_kill [30004]`；
   `f1-reclamation.json` 记 `native_pids_alive_after_drain []`、`proved_dead: true`；STOP 走有序路径
   （`released_clean: true`），合并 ETL sha256 `0df0960d…` 与回执相符。
3. **死亡用例**：`expected-controller-death.json` 记 `before_stop_request: true`、
   `native_pid 10328`、`active_job_processes 6`，这正好是 r3 缺失的那份意图；一次性 STOP helper
   `helper_exit_proved: true`、`exit_code 0`；`f1-native-reclaimed.json` 记
   `alive_after_controller_and_guardian: false`、`checked_after: "controller exit and guardian drain"`；
   原始轨迹按 takeover 设计保留（事件 0.12 MB + 系统 6.62 MB），不产出合并轨迹。
4. **输入信任前置**：三个用例目录都有 `input-pins.json`，均 `verified_before_start: true`、
   `pinned_inputs: 15`、`binding_sha256` 为 `5cce9b67…`；每个 STOP 回执的 `sessions`/`proof.sessions`
   列表里都没有 `PD5F1Noise_*` logger，与 `sessions_absent: true` 和提权 `QueryAllTraces` 一致。

两项只记录、不改变判定的观察：

- 超时用例根 `result.json` 的顶层 `native_launches: 6` 是**计划包络**（`cases.F1_SCALE`），观测值是
  `workload.native_launches: 1` 与 `f1-workload-timeout.json.native_pids_observed: [30004]`，
  `case_passed` 不读顶层字段。把该字段改名为 `native_launches_expected` 属于工具改动，只能进 r5，
  本次不动。
- 同一工作负载的 ETL 体积随机器级事件量变化：用例 1 这次 166.0 MB（r3 为 382.9 MB），用例 2 为
  28.5 MB（r3 为 64.9 MB），而每进程行数、内容摘要（1/8/64/256 对象分别为
  `5b62459d…`/`79336a08…`/`86efc39d…`/`9d806032…`）、掩码与启动次数与 r3 完全一致。诊断轨迹
  没有体积预算也没有阈值，这里只记录事实，不用它解释成败。
- api 角色的调用次数（69/16/5）与 r3（70/16/5）同量级：collector 每秒轮询会话状态，每次调用留下
  一个 `api-*.log` 与 `api-query-*.json`，死亡用例全程只有一次 `api-stop`。

## 管理员验收步骤（已执行一次，留作复现说明）

以下步骤已于 2026-10-23 对 r4 执行并全部通过；**r4 不再重跑**，本节只用于复现与核对。

```powershell
# 1) 以管理员身份启动 PowerShell：WPR start/stop 与提权 QueryAllTraces 都需要
cd D:\Cuexis-pd5-40k-admission\out\pd5-etw-f1-tools-20261023-3e5837f-r4

# 2) 只读确认封包身份（可选，用于确认没有误用 r2、r3 或更早目录）
(Get-FileHash .\binding.json -Algorithm SHA256).Hash.ToLower()
# 期望 5cce9b679940ea922c526d962fe388db37d372e3905becf1df9196142bbb37d0

# 3) 运行一次：按序执行 f1-scale、f1-scale-timeout、f1-scale-death，任一项失败即停止
.\run_f1_scale.ps1
```

若执行策略阻止脚本，用 `powershell -NoProfile -ExecutionPolicy Bypass -File .\run_f1_scale.ps1`
（不改系统策略，也不改封包）。

实测：整轮 176.4 s（含三次采集与两次 STOP/接管），用例 1 的 3 个测量进程分别为 45.08/45.15/45.27 s，
远低于 collector 250 s 与工作负载 300 s 预算；本次 ETL 为用例 1 174,063,616 B（166.0 MB）、
用例 2 29,884,416 B（28.5 MB）、死亡用例原始轨迹合计约 6.7 MB，全部低于 2 GiB 上限（D: 现在剩余
137.2 GiB）。r3 的 401 MB 是同一工作负载在另一次运行里的体积，说明该轨迹大小随机器事件量浮动。

本次运行的输出就是这一形状：横幅 `F1 TOOL ACCEPTANCE ONLY …`、三次 `Starting fixed case …`、
`F1 TOOL ACCEPTANCE PASSED; DIAGNOSTIC TRACE ONLY; NO PERFORMANCE ACCEPTANCE`
和 `[PD5 tools] Exit 0; evidence: <本次证据目录>`。失败时最后一行是
`FAILED/UNRESOLVED: …`、`[PD5 tools] Exit 1`，并在该目录写入
`result.json`（含 `instruction: DO NOT RERUN`）。

判定要看证据而不是只看退出码：`result.json` 的 `cases[]` 中三项的控制器退出码依次为
0/74/73、`case_passed` 全为真，且每项都有 `job_active_after_drain == 0`、该用例的 native PID
逐个被证明已死、两个精确 logger 名与提权 `QueryAllTraces` 的缺席证明，以及有序 STOP 的
`api-stop-<id>.json`/`raw-stop-helper-exit.json` 退出 0。

运行结果已按上面的判据逐项核对：证据根路径、根 `result.json`、三份 `input-pins.json`、两份合并轨迹与
三份 native 日志的哈希都已重算相符，PowerShell 未报错误、未触发重试。失败情形的纪律不变：保留现场，
不重跑、不改封包。

## 非声明

- 工具验收不是性能接受，不是正式测量，不是 PD-5 完成，也不扩大 ETW 故障覆盖。
- r4 的三项全过只证明工具合同成立：三条路径各自可以真实通过、并在失败侧 fail closed。它不产生
  任何样本、比值或耗时比较，两份合并轨迹与死亡用例的原始轨迹都是诊断产物，不是性能证据。
- r2 的失败是**工具合同缺陷**：解析器与冻结 exe 的输出不一致；r3 的第三项是**死亡证明缺陷**：
  对一个已退出的 PID 重新打开并缺少死亡意图回执。两者都不是测量结果、也不是产品失败；这些运行不
  产生任何样本、比值或通过结论。
- 三用例每项只允许运行一次；失败即保留，不重试、不补样、不替换样本。
- 正式测量仍按已接受的无 ETW 方案执行；F1 采集只用于归因读取比超标。
- 控制器、守护者与 harness 同时失效超出内核会话的恢复保证范围。

## 待办

1. 封包 `out/pd5-etw-f1-tools-20261023-3e5837f-r4`（`binding.json` `5cce9b67…`）已通过只读核验、
   独立核验，并由管理员运行一次、三项全过；它因此也是**已运行**包，不重跑、不改名、不编辑。源码
   或计划若再改动，`binding.json` 失效，只能新建封包（下一个编号是 r5），不能修改已封包目录。
2. 目前不存在待运行的封包。r2、r3 的失败与 r4 的通过都原处保留，都不重跑；若将来确需再验证一次
   工具（例如为改名或新用例），必须重新封包并重新取得独立核验，而不能再动 r4。
3. 之后回到正式测量准备：读数候选裁决、P0–P8 profile 与消费方清单、pilot、不可变注册与
   三个正式窗口。提交与推送仍待单独授权。
