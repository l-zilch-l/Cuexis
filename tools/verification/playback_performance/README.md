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
| [profiles.json](profiles.json) | 有限工具 profile、容量声明和显式就绪阻断项 |

## 调用

从仓库根目录执行：

```powershell
python -B tools/verification/playback_performance/prepare_profiles.py --help
python -B tools/verification/playback_performance/prepare_profiles.py prepare --profile cpu-capture-4-v1 --build out/build/pd5-preparation-release
python -B tools/verification/playback_performance/prepare_profiles.py verify --manifest <manifest.json>
python -B tools/verification/playback_performance/owner_runner.py prepare --output out/pd5-owner
python -B tools/verification/playback_performance/device_inventory.py --output <new-inventory.json>
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
