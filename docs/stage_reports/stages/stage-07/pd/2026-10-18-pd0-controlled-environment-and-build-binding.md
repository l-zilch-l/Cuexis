# PD-0 受控环境、构建绑定与退出证据

状态：PD-0 testOnly 基线与行为表征完成；PD-1 至 PD-5 未开始

日期：2026-10-18（Asia/Shanghai）

本轮在 owner 提供的安静环境中，先解决测量程序的源码绑定缺陷，再使用干净重建的程序完成一次预登记的两窗口正式组。全部 owner 与宿主均值重复性满足原定 15% 门槛，长读取块、精确语义身份、同源码存储/阶段/交付观测及受影响检查通过，因此 PD-0 退出。

该结论限于登记的有限 testOnly profile。历史失败、未测项和帧账本回收缺陷继续保留；本轮没有实现 PD-1 至 PD-5，没有接受生产预算、尾延迟或实体设备门禁。当前状态唯一见 [CURRENT_STATUS](../../../../CURRENT_STATUS.md)，方法和批次范围见[设计 §9.2](../../../../stage_plans/active/stage-07/presentation-decoupling-design.md)与 [PD 计划](../../../../stage_plans/active/stage-07/plan-presentation-decoupling.md)。

## 环境与不可替换的历史

owner 明确当前电脑已就绪，已退出其他高占用程序，使用均衡模式、主动散热，可保持空闲至本地 12:00。现场确认接入交流电，Windows 电源方案为 Balanced；正式窗口前后活动方案及全部隐藏电源设置的结构化指纹相同。OEM 模式、实际散热状态和温度没有独立遥测，不据此宣称频率恒定或独占核心。

机器为 Windows 11 `10.0.26300`、AMD64 Family 25 Model 117、16 个逻辑处理器；工具链为 MSVC `19.51.36256.0`、CMake `4.3.3`。四个构建均为 headless、STATIC、candidate ON、warnings-as-errors ON。GPU、音频、VSync 不参与本 profile。

此前[固定核心组](2026-10-18-pd0-core-policy-and-contention.md)仍为 inconclusive，Debug/RPA 长读取合并波动 72.09%，宿主 Release 均值波动 17.69%；后续 SMT 诊断没有重现严重慢态。本轮改变的是已声明的环境与构建绑定，另建注册，不替换旧组任何进程，不放宽公式或阈值，不将较好时段拼入旧组。

## 构建绑定缺陷与联网核对

第一份受控环境尝试保留在 `out/pd0-controlled-environment-20261018/`。其注册为 `formal/registration-20261018T002704Z.json`；已完成 13 个进程时，发现冻结宿主程序早于当前 `measurement_environment.hpp`，原生输出缺少源码中的 affinity 字段。按身份错误立即停止，controller/state 标记 `stopped_source_binding`；已完成样本、被中断日志及预热日志全部保留。该组不能证明当前源码的性能，也不被改写成性能通过或失败。

进一步检查发现 Ninja 对测量对象的 `-t deps` 输出为 `#deps 0`。现场控制台输入代码页为 936、输出为 65001；编译器实际输出的中文 `/showIncludes` 前缀与 CMake 缓存的乱码前缀不匹配，导致依赖解析丢失。仅设置 `VSLANG=1033` 的尝试无效，因为本机只安装了中文编译器语言资源。

Microsoft 的 [/showIncludes 文档](https://learn.microsoft.com/en-us/cpp/build/reference/showincludes-list-include-files)说明其输出嵌套头文件列表；Ninja 上游的[本地化依赖跟踪问题](https://github.com/ninja-build/ninja/issues/1766)记录了前缀与编码不匹配会破坏跟踪的机制。此处的具体代码页原因由本机最小复现和修复前后实际依赖清单验证，不从上游问题推断所有 MSVC 安装都有相同故障。

最小复现产物为 `include-binding-reproducer/result.json`。在同一进程内将输入、输出代码页均置为 UTF-8，保持实际安装的 `VSLANG=2052`，然后 fresh configure + clean build，恢复了头文件依赖；脚本结束恢复原代码页，没有修改全局电源或编译器语言安装。

`fresh_build_consistent.py` 随后干净构建以下四套程序，结果保存为 `consistent-build-results.json`：

| 构建目录（均在 `out/build/`） | 阶段插桩编译 | 目标 | 实测头文件依赖数 |
| --- | --- | --- | --- |
| pd0-formal-release | OFF | baseline / host | 200 / 197 |
| pd0-formal-debug | OFF | baseline / host | 200 / 197 |
| pd0-observe-release | ON | baseline / allocation | 200 / 201 |
| pd0-observe-debug | ON | baseline / allocation | 200 / 201 |

`tests/pd/formal_baseline.py` 增加非空完整依赖清单、必须包含环境头、对象不早于源码/头文件、程序不早于测量对象的检查；三个新增测试覆盖零依赖、过期头文件及缺少环境头。`collect_storage.py` 允许显式指定独立观察构建，保留原默认路径。

旧可执行文件及 PDB 在重建前已保存为 `pre-rebuild-binaries/manifest.json` 所列的六个文件。与旧固定核心注册逐项比较的 701 个 C/C++、CMake 和依赖清单输入完全一致；本轮未修改产品 C++ 或 CMake 语义。修复构建绑定不等于解释历史时间波动，二者分别记录。

## 正式注册与原始证据

以下路径以 `out/pd0-fresh-bound-baseline-20261018/` 为根：

- 注册：`formal/registration-20261018T005117Z.json`，SHA-256 `a27bfe2062a0e1fd0a5e92f875527a65bb3a24c1cbde8611523b4c96cbaacb42`。
- HEAD：`ef7fa30d5b578896ccc165622fe256c553a5641c`；未提交工作树库存 SHA-256 `d228ed693a9e1a5c6e82882fbfd27356c1d8aa20f10eb6ea06204fc1716361a7`。
- 实现输入 SHA-256：`fd8437c1d24419bd6218fe3f32fd0d9fdc2b16152e614f016a539835eba23121`；两正式窗口、四套验证程序、存储和普通分配器交付观测均绑定该值。报告更新会改变文档库存，不改变这个实现指纹。
- 窗口一：`formal/window-1-20261018T005119Z-409801/`，本地 08:51:19 开始，08:56:42 完成。
- 窗口二：`formal/window-2-20261018T010644Z-6b2cf8/`，本地 09:06:44 开始，09:12:07 完成。
- 原始日志重算审计：`audit_formal.py` → `formal/independent-audit-20261018.json`。
- 同源码汇合审计：`audit_closure.py` → `closure-manifest.json`，SHA-256 `71a16e09a41b4151894ddbd8f381503bc30a5c2f8341c424c5c520d2215c5071`；`closure-evidence-hashes.json` 覆盖 172 个证据文件。

两窗口 claim 与完成时间差为 **603.093 秒**，启动还核对单调时钟的 600 秒下限。每窗口每配置/profile 三个 owner 进程，每配置三个宿主进程；总计 36 个进程全部成功，576 个 owner 块、6144 个宿主测量帧。192 个长读取块最短 **364.816 ms**，全部超过 100 ms。

正式程序使用正常分配器、编译插桩 OFF、没有启动 ETW；WPR 无录制响应随窗口保存。单 owner 按登记的拓扑规则固定为逻辑 CPU 2（掩码 `4`），没有按 pilot 分数选最快核心；宿主保持默认允许掩码 `65535`，原生进程亲和性查询成功且前后一致。没有改优先级或显式节流策略；默认 QoS 自动分类仍未被这些字段排除。

机器预热为 30 秒，fresh-session 预热与正式块分开。每 owner 进程每指标八块，trace 每块 1024 步；Release 长读取块为 2097152 次，Debug 为 32768 次。pilot 的 16 个进程不计入正式样本。运行顺序与批长冻结后没有追加或替换正式进程。

## 原公式重复性与描述统计

以下波动均为 `(max-min)/median`，单位为百分比；两个三进程窗口及合并六进程全部须 ≤15%。每进程先取八块的摊销成本中位数，独立实验单位仍为进程。

| 配置/profile | 指标 | 窗口一 | 窗口二 | 合并六进程 | 48 块中位数 μs/操作 | 块最大 μs/操作 |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Release legacy | validated_trace | 1.834% | 2.729% | 3.458% | 26.539 | 27.790 |
| Release legacy | playback_only | 0.920% | 2.336% | 3.804% | 24.961 | 26.919 |
| Release legacy | read_long | 2.952% | 1.408% | 3.893% | 0.2084 | 0.2328 |
| Debug RPA | validated_trace | 2.299% | 1.572% | 2.305% | 443.931 | 505.394 |
| Debug RPA | playback_only | 2.479% | 1.565% | 2.738% | 412.341 | 447.042 |
| Debug RPA | read_long | 1.851% | 1.794% | 2.132% | 11.374 | 15.326 |
| Release RPA | validated_trace | 2.015% | 0.725% | 1.990% | 15.309 | 16.862 |
| Release RPA | playback_only | 2.088% | 2.060% | 2.336% | 13.752 | 14.365 |
| Release RPA | read_long | 1.985% | 1.805% | 3.037% | 0.2072 | 0.2303 |
| Debug legacy | validated_trace | 2.000% | 1.874% | 2.590% | 1039.493 | 1271.166 |
| Debug legacy | playback_only | 1.693% | 1.507% | 2.390% | 1014.049 | 1226.172 |
| Debug legacy | read_long | 1.176% | 2.032% | 2.028% | 11.409 | 18.367 |

`validated_trace` 包含逐步正确性验证和 FrameDigest；`playback_only` 为另行登记的推进边界，`read_long` 是长块摊销读取成本。不是单次调用尾延迟。每单元 48 个块不足登记的分位数支持要求，p95/p99 明确为 null；每进程值、总体 mean/min/max 与样本数完整留在 state。legacy/RPA 是不同 workload，不据两行差异声称优化收益。当前无旧/新实现对照，配对收益与 95% 区间为 not applicable。

宿主每进程 32 帧预热、512 帧正式；使用实际 ExecutionHost → CPU TestPresentationRenderer → worker 回执确认链，每帧与直接 Playback 参考帧比较。无 Gameplay、音频或 GPU，每会话发布 544 帧，低于旧账本容量 1024；它只建立有限交付基线。

| 宿主配置 | 窗口一均值跨度 | 窗口二均值跨度 | 合并六进程均值跨度 |
| --- | ---: | ---: | ---: |
| Release | 3.851% | 3.953% | 4.759% |
| Debug | 3.192% | 10.272% | 11.114% |

宿主以下描述统计每行包含 3072 帧，单位 μs；帧不是独立进程重复。

| 配置 | 边界 | median | p95 | p99 | max |
| --- | --- | ---: | ---: | ---: | ---: |
| Release | request → ready | 18.9 | 29.8 | 44.6 | 69.6 |
| Release | ready → receipt ack | 16.9 | 25.1 | 32.5 | 74.2 |
| Release | request → receipt ack | 35.9 | 52.6 | 72.0 | 110.4 |
| Release | present call → ack | 14.0 | 21.0 | 27.0 | 71.0 |
| Debug | request → ready | 217.2 | 270.9 | 314.8 | 15734.5 |
| Debug | ready → receipt ack | 97.9 | 131.7 | 187.4 | 1299.3 |
| Debug | request → receipt ack | 326.7 | 395.7 | 460.1 | 15928.9 |
| Debug | present call → ack | 48.0 | 66.0 | 94.0 | 140.0 |

Debug 的 15.929 ms 尖峰保留在原始数据、均值和最大值中，没有删值或改用中位数通过。尾预算及 over-budget 数为 null；诊断参考计数不作为生产阈值。通过的是均值重复性，不能推断尾延迟、稳态寿命或生产环境合格。

## 同源码存储、阶段与交付表征

存储证据为 `out/pd0-storage-v1/20261018T011211Z-2665b4/manifest.json`，24 个进程全部成功：12 个 allocation workload、6 个 ledger/CPU renderer/self-check、6 个 cold-projection。每个 Debug/Release × legacy/RPA 有三个独立进程、24 个 recording ON trace 窗口，每窗口 1024 步，分配签名精确一致：

| 配置/profile | 每窗口 allocation count | requested bytes | live-after 增量 bytes | peak-live 增量 bytes |
| --- | ---: | ---: | ---: | ---: |
| Release legacy | 102756 | 437546184 | 8538632 | 9273550 |
| Release RPA | 185668 | 74544840 | 8716776 | 8749033 |
| Debug legacy | 2692917 | 553965664 | 10438936 | 11372438 |
| Debug RPA | 700181 | 91890688 | 10781944 | 10821705 |

这些是 C++ `operator new` 请求量及探针跟踪的 live/high-water，包含该固定验证 workload，排除 malloc/CRT、栈/TLS、驱动及 GPU；不是进程 RSS、prepare-only 峰值或 decoded semantic heap。分配探针的时间不进入正式性能表。

阶段 JSONL 保存 judgement、projection、runtime candidate/install、snapshot、state copy、feedback 的计数与单次诊断分布。每进程八个 recording trace 的 Runtime 状态复制为 8192 次，Release 逻辑复制 5242880 bytes，Debug 为 6619136 bytes；这是每进程聚合值，不和嵌套 Runtime 总时间相加。热轨迹绑定扫描 items 为 0，cold-projection 六进程分别验证扫描一次；不将未触发扫描误写为缺少扫描能力。Snapshot items 为每进程 8192，旧路径没有新结果版本计数。

普通分配器交付诊断另存于 `normal-delivery-20261018T011452Z/manifest.json`，12 个进程使用预定 dormant/active 次序，全部成功；带编译插桩，仍只作表征。每 active 进程核对 renderer submit/present 各 4096 次、receipt/take 各 8200 次、ledger publish 8204 次，以及 fixture 的 runtime prepare/commit/snapshot。保留旧账本在 capacity+1 拒绝发布的预期缺陷，不能将该测试通过称为账本已经回收或长期稳态通过。

正式 owner 与存储 workload 的 FrameDigest、Replay/Snapshot/Packed SHA-256 按 profile 跨配置/进程精确一致。scope、resource generation、cursor 和窗口行为沿用既有 fixture 检查；新增 resultVersion、无需求跳过构建、版本滞后等尚不存在的指标记 not applicable，不伪造 0。归因插桩不充当旧/新实现 A/B 对照。

## 验证、冻结与退出判定

构建与验证日志在 `out/pd0-controlled-environment-20261018/`；`fresh-probe-verification.json` 核验全部 15 个检查命令成功：

- 四套 fresh configure + clean-first build 成功，实际 Ninja 依赖非零且对象/程序时间关系有效。
- Debug/Release 的 formal baseline、formal host、observe baseline、observe allocation 共八次普通测试程序通过；baseline 各 6 cases / 16844 assertions，allocation 各 3 / 16420，Release host 6 / 3533、Debug host 6 / 3541。
- Python 正式 runner 9 项、电源/QPC 对齐 3 项、统计 5 项、ETW 会话所有权/清理 7 项，共 24 项通过。
- `cuexis_architecture_tests` 一项通过；两个显式隐藏 host 预检核对原生 affinity 字段。普通 Catch 测试不被误称为隐藏测量用例的覆盖。
- 两窗口的源码与程序身份、原始日志、QPC、完整样本、语义相等、未追踪状态、电源指纹、600 秒间隔和长块时长检查通过；独立审计从原始日志重算全部重复性，不只读取结论字段。

汇合审计状态为 `pd0_testonly_baseline_and_characterization_complete`。单项采样 runner 的 `pd0_exit:false` 原样保留，因为正式性能采集本身不负责关闭整批；它没有被事后改成 true。PD-0 退出依据是正式组、同源码表征、受影响验证和本文范围判定的共同证据。

`freeze_evidence.py` 重新核验 172 项证据散列与构建日志，并保存：

- `implementation-inputs.zip`：1014 个实现输入，SHA-256 `c8f3e5d6814ab918c37643d3f991a607d5c589dfecdec4867bb9339577df5b37`；逐成员读取复核，不只保存无法重建的散列。
- `frozen-binaries/`：四套程序的 EXE/PDB 与 build cache/compiler commands，逐文件核对原件，合计 645715496 bytes；无增量链接临时文件。
- `frozen-evidence-manifest.json`：原件/副本散列、大小、源输入不变核对及 closure manifest 指纹。

最后执行 `python -B tools/check_docs.py` 与 `git -c core.safecrlf=false diff --check`。不以这组定向验证宣称 MinGW/shared、Linux sanitizer、干净安装消费者或整个 SDK 新矩阵已完成；本轮没有修改产品实现，相关生产门禁仍按 RPA/后续 PD 批次执行。

PD-0 在上述有限范围内完成。历史慢态根因未定，ETW 准备工具未实际进行管理员采集；FrameObservationLedger 不回收归 PD-4，端到端 Seek、prepare-only 峰值堆、decoded semantic heap、真实设备/听音/同步、生产 profile、RPA-F1–F4 最终接受与发行均保持独立未关闭。下一批为 PD-1，本轮停止在 PD-0。
