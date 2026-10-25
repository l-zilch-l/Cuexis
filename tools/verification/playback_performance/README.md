# Playback performance measurement tools

本目录按功能组织 Playback 性能测量驱动和支持实现，目前服务于 Stage 7-PD。
源文件和目录使用功能名，阶段名仅用于测试、测量协议身份和验收记录。测试、C++ 探针和 fixture 位于
[tests/pd](../../../tests/pd/README.md)，日志和证据写入命令指定的 `out/` 目录。
目录迁移不表示正式采样或性能接受，进度见 [CURRENT_STATUS](../../../docs/CURRENT_STATUS.md)。

## 入口与实现

| 文件 | 职责 |
| --- | --- |
| [prepare_profiles.py](prepare_profiles.py) | prepare/verify/probe/collect：绑定源码、构建和 profile，短测只验证工具 |
| [owner_runner.py](owner_runner.py) | P0 owner/common-host 的 prepare、一次 pilot、注册及固定三窗口执行 |
| [profile_runner.py](profile_runner.py) | B-only profile 的一次 pilot、固定三窗口执行和原始记录校验 |
| [baseline_method.py](baseline_method.py) | 既有插桩基线校准及 child/身份支持，历史复现范围不变 |
| [observer.py](observer.py) | 有限短轨迹的阶段与分配观测器交叉检查，不能代替正式预算证据 |
| [gpu_residency.py](gpu_residency.py) | Windows GPU 进程内存计数器支持，缺失计数明确不可用 |
| [build_profiles.py](build_profiles.py) | 独立正常计时、阶段观测及分配观测构建 |
| [device_inventory.py](device_inventory.py) | 只读盘点默认音频端点、显示、驱动和音乐/DFJK 内容，不启动播放 |
| [measurement.py](measurement.py) | 日志解析、独立进程统计和预算判定支持库 |
| [string_budget_audit.py](string_budget_audit.py) | 在独立源码副本观察 Gameplay-host 实际 STR0/REF0 与字节增长；保持预算，不能用作性能/正式注册 |
| [window_protocol.py](window_protocol.py) | 排他领取、窗口间隔和原始证据链支持库 |
| [process_control.py](process_control.py) | Windows Job 进程树及内部 child 支持，不是面向用户的独立入口 |
| [prepare_etw_canary.py](prepare_etw_canary.py) / [etw_safety](etw_safety/README.md) | 只封包两项空载 ETW 生命周期 canary；不执行 Cuexis/F1，不注册正式测量 |
| [prepare_etw_tools.py](prepare_etw_tools.py) | 把 canary 与三项 F1 用例合并封包为一个自校验工具包并生成两个启动器；不启动采集 |
| [profiles.json](profiles.json) | 有限工具 profile、容量声明、逐 profile `content_entities` 和显式就绪阻断项 |
| [memory_caps.py](memory_caps.py) | PD-5 B0：由已声明容量静态推导 `pd5.memory.caps.v1` 逐单元上限；缺项记 `NA:<原因>`，不运行工作负载 |
| [memory_caps.v1.json](memory_caps.v1.json) | 上者的生成物；`--check` 逐字节比对 |
| [audit_element_sizes.py](audit_element_sizes.py) | PD-5 B0：编译期 `sizeof` 审计账本元素尺寸，供 `memory_caps.py` 折算字节上限 |
| [element_sizes.v1.json](element_sizes.v1.json) | 上者的生成物；`--check` 重新编译比对 |
| [memory_domains.py](memory_domains.py) | PD-5 B1：把 `PD5_MEMORY_DOMAIN`/`PD5_MEMORY_PARTITION` 归到 owner×domain 并对照 B0 上限；只解析已采集日志，不运行工作负载 |

## 调用

从仓库根目录执行：

```powershell
python -B tools/verification/playback_performance/prepare_profiles.py --help
python -B tools/verification/playback_performance/prepare_profiles.py prepare --profile cpu-capture-4-v1 --build out/build/pd5-preparation-release
python -B tools/verification/playback_performance/prepare_profiles.py verify --manifest <manifest.json>
python -B tools/verification/playback_performance/owner_runner.py prepare --output out/pd5-owner
python -B tools/verification/playback_performance/device_inventory.py --output <new-inventory.json>
python -B tools/verification/playback_performance/memory_caps.py --check
python -B tools/verification/playback_performance/audit_element_sizes.py --check
python -B tools/verification/playback_performance/memory_domains.py collect-domains <observer.log> --json <domains.json>
python -B tools/verification/playback_performance/memory_domains.py check-domains <observer.log> --caps tools/verification/playback_performance/memory_caps.v1.json --profile cpu-stream-v1 --entities 1 --json <verdict.json>
python -B -m unittest discover -s tests/pd -p 'pd5*_tests.py' -v
```

入口也支持 `python -B -m tools.verification.playback_performance.prepare_profiles ...` 的模块调用。
仓库根目录和默认 profile 从工具文件自身定位；显式相对路径参数仍相对于调用者工作目录。
owner runner 按需从 `tests/pd/` 加载既有 PD-0 support，没有迁移冻结的 PD-0 实现。

## 迁移与兼容

本轮从 `tests/pd/` 迁移六个 Python 实现及一份 JSON；五个 Python 测试和 C++ 探针留在原处。
旧 Python 路径保留薄兼容入口，转发 CLI 和 import；新实现仅保存在本目录，测试直接导入新模块。
兼容入口至少保留一个整理周期，新命令使用上述路径。
旧 `tests/pd/pd5_profiles.json` 已迁到本目录，不保留双份配置，调用方须使用新路径。
按功能命名整理后，工具统一放在 `tools/verification/playback_performance/`。
`app/`、`engine/` 中的 Player 轨迹、分配观测、GPU 计时与存储观测同样使用功能文件名。

历史归档的旧路径和命令保留原含义，不重写 ZIP、原始日志或 frozen manifest。
迁移会使旧 preparation 的完整输入身份失效，后续运行须重新 prepare，不能绕过身份校验。
新注册还须生成 preparation 外层的 `registration.seal.json`；窗口开始及历史窗口加载会复核它，
旧的未封存注册不能继续。正式窗口与 pilot 的 process record/原始日志必须独立，硬链接也不能重复使用。
本次不运行 pilot 或正式窗口，也不改变已接受阈值。范围及剩余项见
[窗口协议与矩阵报告](../../../docs/stage_reports/stages/stage-07/pd/2026-10-19-pd5-window-and-matrix-preparation.md)。

功能命名同时覆盖既有基线方法：旧 `tools/measure_pd0_method.py` 与
`tools/verification/measure_pd0_method.py` 改为 [根级功能入口](../../measure_playback_method.py) 和
[baseline_method.py](baseline_method.py)。当前调用方已更新；冻结归档中的原命令按原时点解释。

## 有限诊断与历史完整性

`diagnostic.py <input.json>` 当前使用 pd5.followup.v2 / local-reference-v1-diagnostic，
显式校验 source/binary SHA256、内容/操作序列摘要、Snapshot/registry/camera 计数、U/F/R、
viewport、输出摘要、单位及起止点。恰好三对独立进程，每进程先取中位；块不增加自由度，
block_average 不产生操作 p95/p99。区间及 15% 进程重复性分开披露，结果始终 acceptance=false。
共同 request→ready 可比较；ACK、present-return、enqueue、drain、GPU 完成各自命名，
不能用 total 别名合并。诊断标签 diagnostic_pass 不构成正式接受；正式 runner 仍用原三窗协议。
旧 local_measure.py 的历史 PASS 只表示汇总点估计，不修改其日志。

`historical_integrity.py` 只核对旧暂停快照及绑定文件，不依赖当前 HEAD，也不评估当前可登记性。
`build_profiles.py --build-dir <fresh-directory> --output <log-directory>` 可显式选新构建目录；
仅改变 --output 不改变默认 pd5-ready-* 构建目录。显式目录已存在时非 incremental 拒绝，
构建最多 4 并行、BelowNormal 优先级。历史绑定构建不用于本轮重建。

2026-10-21 v2 进一步以 native PID/QPC 起点和原始日志 SHA256 拒绝改名进程/日志复用；
v1 输入与结果保持原含义，不能直接当 v2 输入。保留的 v1 分析文件可用于历史只读复算。
`host_diagnostics.py <frozen-folder> --output <new-json>` 补全 host 的内容/序列、相机、U/F/R、
公共 ready 端点和输出合同；相机/registry 数与 U/F/R 明确来自冻结内容/循环，非独立 Runtime 计数器。
ACK/present-return 原字段继续分别报告；enqueue/drain/GPU 无边界时仍 unavailable。
`finite_followup.py` 的 host 汇总复用同一完整契约，拒绝将改名后的两个 native 日志计作三对进程；
它的外层正常结束仍可能包含容量单元失败，须读取 run-result.json 的 failures，不能仅看控制器退出码。
`compare_read_candidates.py` 是一候选、三侧固定进程的规模对照；先审计归档方法，再绑定新方法，
不恢复旧正式窗口或修改其日志。全部结果仍是有限诊断。

`read_result_candidate.py bind` 只绑定已预登记的 F1/C2 候选与两套新正常 Release 树，要求独立
语义/代码生成验证已经完成。它校验历史 F1 库存及原始归档，再比较编译输入并核对 CMake 源目录；
历史 `out/pd5-followup-20261021-62ca993` 是必需证据，缺失时拒绝绑定，不能把任意输入命名为 F1。
`run --output <已绑定候选根>` 仅运行固定 12 个独立进程，无补样；结果保留 pending_review=true、
acceptance=false。数值自检通过也不授权保留候选或建立正式 preparation/registration。

`admission_feasibility.py` 是 40k 准入阶段 A 的隔离计数工具：prepare 在全新输出根归档并插桩，
bind 绑定新 Release 诊断构建，run 执行 N=1/64/40000×low/high 固定六配置。原 Writer 预算不改；
提议布局 sizing、原 semantic hash 需求与 wire accepted 分别记录，不进入 Host 或正式测量。
小规模实际输出校验 sizing/hash；40k 的提议 Gameplay payload 仅计数。工作量上限与 revision 4
Reader/Writer 尚待合同接受，参见 docs/stage_plans/active/stage-07/pd5-gameplay-admission.md。

## F1 工具包（canary 与 F1 用例合并封包）

[prepare_etw_tools.py](prepare_etw_tools.py) 把 [etw_safety](etw_safety/README.md) 的全部源模块、
`README.md`、`lifecycle.wprp`、`f1_scale.wprp` 和 `plan.json` 封成一个自校验工具包，
在封包时写入 `binding.json` 并生成两个管理员启动器：

```powershell
python -B tools/verification/playback_performance/prepare_etw_tools.py `
    --output out/<全新目录> --wpr "D:\Windows Kits\10\Windows Performance Toolkit\wpr.exe"
```

`--output` 必须是尚不存在的路径（create-new，已存在的封包不可更新），`--wpr` 必须与
`plan.json` 的 `wpr.executable` 一致。封包不启动 WPR，不执行 Cuexis/F1，`executed: false`。
封包内 `run_tool_canary.ps1`（role `canary`）执行两项空载生命周期用例，
`run_f1_scale.ps1`（role `f1`）执行三项 F1 用例；两者都先核对 `binding.json` 与 `bootstrap.py`
的哈希，再以 `-B -I -S -u` 运行 `bootstrap.py`，把退出码原样返回并打印本次证据目录。
启动器返回的是 harness 的退出码：`0` 表示全部固定用例通过，`1` 表示 `FAILED/UNRESOLVED`；
个别用例自己的控制器退出码（`f1-scale` 0、`f1-scale-timeout` 74、`f1-scale-death` 73）是
判定条件之一，记录在该用例的 `result.json` 里，不是启动器退出码。两者都不是重试许可。
`f1-scale` 走“一次运行全部三项”，任一项失败即停止，不重试、不补样。

`binding.json` 使用 `pd5.etw-tool-canary.binding.v3`：`sources`（逐模块 SHA256）、
`controller_source_sha256`、`artifact_sha256`（每个交付文件的 SHA256，含 `plan.json`）、
`input_sha256`（frozen 输入/支持/运行时资产/解析证据）、`profiles`（两个 profile 的文件名、profile
名与 SHA256）、`cases`/`case_envelopes`、`wpr`、`python`，并显式写 `formal: false`、
`native_launches: 0`、`formal_acceptance: false`、`performance_acceptance: false`。
`bootstrap.py` 在 import 控制器之前复核 schema、源集合、profile 绑定、控制器哈希、
全部 artifact 哈希、用例表、解释器身份与 `wpr.exe` 哈希，任一不符即
`FAILED BEFORE ROLE` 并以 2 退出；因此被篡改的 `plan.json` 在第一个 WPR 动作之前就被拒绝。
控制器签发的 `f1-owned-capture.json` 同时记录 `controller_source_sha256` 与
`controller_claim_sha256`，collector 再用 `binding.json` 与命令行 claim 双向核对，
这取代了旧包装器才能诚实写入的 `wrapper_script_sha256` 合同；包装器哈希不入链，也不伪造。

`plan.json` 是 F1 数值的唯一真源，封包时与 `etw_safety/cases.py` 交叉核对：
整次采集硬上限 420 s、守护者接管 380 s、磁盘上限 2 GiB、collector 预算 250 s、WPR 停止 110 s、
启动 20 s、harness 480 s、监督者工作负载预算 300 s（250 + 50 宽限）、6 次 native 启动、
3 个进程、mask 4、预热 30 s，超时探针落在预热内的 20 s。canary 保持 120/95/128 MiB 的原有数值。
`f1-scale` 要求退出码 0 且 6 次启动全部完成；`f1-scale-timeout` 要求退出码 74、Job 与每个 native
PID 都被证明死亡且完成有序停止与缺席证明；`f1-scale-death` 要求退出码 73、死亡瞬间有存活的
native 身份证据，并在控制器与守护者都不存在后证明该 PID 已死。

三项 F1 用例都是工具验收，不是正式测量、不是性能接受、不是 PD-5 完成，也不扩大 ETW 故障覆盖；
每项只允许运行一次，失败即保留，不重试、不补样。相关断言在
`tests/pd/pd5_etw_f1_scale_tests.py`（`pd5_etw*_tests.py` 发现集共 88 项）：计划与用例表一致性、
claim 合同、签发收据、工作负载/超时/死亡三条路径、启动记录解析、profile 选择、preflight 与判定。
