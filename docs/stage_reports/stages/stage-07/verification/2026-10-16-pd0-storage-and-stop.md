# PD-0 存储观测与回归失败停点

状态：implementation evidence / stopped for owner guidance，PD-0 未退出。

日期：2026-10-16。分支 `stage-7`，起步存档 `276358d99baf0e280a7d6e0429f9990ffcc9f6f1`；
本报告对应其后的未提交工作区，不是该提交本身的验证结论。当前状态以
[CURRENT_STATUS](../../../../CURRENT_STATUS.md)为准。计划与测量合同见
[PD 计划](../../../../stage_plans/active/stage-07/plan-presentation-decoupling.md)及
[细则 §9](../../../../stage_plans/active/stage-07/presentation-decoupling-design.md#9-容量-profile测量与退出pd-0--pd-5)。

## 停止原因

owner 要求“接下来工作时遇到困难就停下来，向我求助”。本轮在受影响模块回归中遇到失败，
停止后续 PD-0 采样与实施，等待 owner 决定是否先调查这项失败。

执行命令为 Release `cuexis_execution_host_tests.exe --rng-seed 1`，未过滤普通用例；
Catch 隐藏用例不在该默认选择中。26 个用例中 25 个通过、1 个失败，1988 个断言中 1 个失败，
退出码 42。失败用例：

`B5 realtime main host cuts after inspection and drives Pause Resume Seek Close`

失败位于 [rpa_b5_host_tests.cpp](../../../../../tests/rpa/rpa_b5_host_tests.cpp) 第 163 行的
`REQUIRE(paused)`；`live->pause(source)` 返回：

```text
host.rpa.capture_gap: Capture route protocol rejected
```

尚未重跑、定位根因、与无本轮改动的版本进行对照，也未改变捕获/时钟/Pause 合同。
因此不能判定这是偶发、已有问题或本轮观测造成的回归。

原始证据：`out/pd-20261016/regression-release-cuexis_execution_host_tests.log`；
执行记录：`out/pd-20261016/regression-results.json`；日志及现有探针/宿主测试二进制的
SHA-256：`out/pd-20261016/pd0-stop-manifest.json`。
这些路径均相对仓库根目录，属于本地证据，未上传。

### owner 要求后的复测

owner 说明尚未移动 tools 文件，并要求再次测试。本次不修改代码、不重建，预先登记并执行
两次完整 Release execution host 普通用例、两次单独 Pause 用例；四次均使用 `--rng-seed 1`，
移除 `CUEXIS_PD_METRICS`，其余环境继承，未设置亲和性或优先级。

| 次序 | 范围 | 用例/断言 | 退出码 |
| --- | --- | --- | ---: |
| full-1 | 完整普通用例 | 26 / 2014，全部通过 | 0 |
| full-2 | 完整普通用例 | 26 / 2010，全部通过 | 0 |
| pause-1 | 单独 Pause 用例 | 1 / 33，全部通过 | 0 |
| pause-2 | 单独 Pause 用例 | 1 / 33，全部通过 | 0 |

完整程序含实时采样循环，两次通过的断言数不同；两次均完整执行了 26 个普通用例，
不是筛选部分用例后宣称完整通过。隐藏用例仍不包含在默认选择中。
运行前后 executable SHA-256 均为
`dd574ead26224918b58e56c763737808c705d0e04f86af432d9e9d7e2401b01b`，与原失败时一致。
原始日志和登记 manifest 位于 `out/pd-20261016/b5-retest-20261016T132615Z/`。

结论仅为四次复测未复现，表明原失败不是该二进制每次必现的失败；没有定位具体触发条件，
没有证据证明由工具移动、PD 观测改动或特定调度条件造成，也没有修复或消除偶发风险的结论。
保留原失败。本轮仅完成所要求的复测，未恢复后续 PD-0 正式采样，PD-0 仍未退出。

## 本轮新增观测

PD 文件均位于 `tests/pd/`。`cuexis_pd_baseline_probe` 保持普通分配器；独立的静态
`cuexis_pd_allocation_probe` 通过完整的 scalar/array/aligned/sized/nothrow C++ new/delete
替换统计申请次数、申请字节及存活高水位。它不用于性能判定，不统计 malloc、CRT 堆元数据、
驱动或 GPU 分配，也未验证 OOM/new-handler 行为。Release 自检的 6 个断言通过，验证了
37+73 字节计数、256 字节对齐及释放后恢复；不推广为所有分配行为已经验证。

内部观测新增 Runtime 状态复制、prepare、World commit、反馈枚举、ledger publish/take、receipt；
独立 CPU TestPresentationRenderer 探针分开记录 submit/present。
Runtime 层原有计时会扣除子计时，禁止把旧 Runtime 总时间与新增子阶段重复相加。
`runtime_state_copy.logical_copy_bytes` 仅为每次 `sizeof(RuntimeEvaluationState)` 的逻辑内联
布局量；复制期间的 `requested_allocation_bytes` 包含顶层对象与动态存储申请，
**两者都不是实际 CPU memcpy 搬运量**，也不能相加后宣称完整深复制字节。

## 已取得的存储数据

第一轮数据位于 `out/pd0-storage-v1/20261016T131529Z-48f4f6/`，包含登记 manifest、每次日志、
逐阶段 JSONL、源码/工具链/二进制身份及 `sha256.json`。12 个固定轨迹进程均通过 Replay、
Snapshot 与 FrameDigest 精确等价检查。每个配置/profile 为 3 个独立进程，每个进程 8 条
64 步预热后采集 1024 步的新 session 轨迹；预热轨迹独立。

下表为单条 1024 步轨迹，字节单位均为 byte；每个配置/profile 的 24 条采集轨迹计数相同：

| 配置/profile | C++ 申请次数 | 累计申请字节 | 存活增量 | 相对窗口起点的高水位增量 |
| --- | ---: | ---: | ---: | ---: |
| Release legacy | 102756 | 437546184 | 8538632 | 9273550 |
| Release RPA | 185668 | 74544840 | 9945576 | 9976633 |
| Debug legacy | 2692917 | 553965664 | 10438936 | 11372438 |
| Debug RPA | 700181 | 91890688 | 12199160 | 12237537 |

Debug 的标准库/迭代器调试分配包含在该配置的统计中，不与 Release 直接作性能比值。
全局 live/peak 包含测试框架及当时仍保留的上一 session，因此表中使用窗口相对增量；
prepare 窗口另见原始 `PD_MEMORY`，不能把进程总 live 当作单 session 的占用。
两条路径均有历史增长；RPA 固定轨迹也不是无限稳态。申请总量与峰值不是同一个指标。

该轮收集器的 delivery 自检与计时开关共享进程，部分 delivery JSONL 覆盖不足；
`status=observed` 不意味着 coverage 或 PD-0 退出通过。这部分数据不作完整分段基线。
现已让 delivery 用例显式启用观察，并在收集器加入各阶段精确样本数检查、固定随机种子、
三次 cold binding 扫描观察和存储重复一致性检查。还补充了独立反馈窗口与可发布 cursor oracle。
更新后的 Debug/Release 探针均已编译，但因上述 B5 失败，**尚未执行修订版收集器或新的正式计时组**。

## 已表征的旧路径限制

Release PD baseline 调用通过了 21878 个断言（含保留的 legacy、ledger、CPU renderer 三个用例）。
容量 4/4096 的账本，在逐帧 take、成功 receipt、expire 后仍分别在第 5/4097 次发布失败；
推进表现 tick 与冻结 tick 两组结果一致，Unknown 计数为 1。旧代码未修复，容量未扩大。
第 4097 次是受控探针的发布序号，不是历史设备日志中的已知帧号。

CPU TestPresentationRenderer 完成 4096 对 submit/present，并验证零表面提交拒绝；
场景为 `assets/projects/stage3_project`、1280×720、2 个对象、逻辑 Snapshot footprint 987 bytes、
renderer generation 1。逻辑 footprint 为结构布局与字符串有效长度，不是分配容量或搬运流量。
这不是实际 GPU、VSync、实体窗口、音频或生产呈现测量。

旧路径没有的新 resultVersion、消费者额度/跳过构建/交付年龄/版本滞后保持 not applicable。
ledger 使用 scope/resource generation 1；没有把新交付结构提前放入 PD-0。
超越账本容量的长期有界稳态目前不能取得，缺陷保留给 PD-4 生命周期工作。

## 已运行与未运行

本轮受影响目标在 MSVC static candidate ON、PD metrics ON 下完成 Debug/Release 编译。
Release Runtime、Playback、host protocol 三个普通测试程序已通过；随后 execution host 失败，
顺序回归脚本立即停止。其后的 RPA/Judgement 和 Debug 测试未执行，不宣称完整矩阵通过。

[先前测量方法报告](2026-10-16-pd0-measurement-method.md)中的 14 次校准/24 次正式组
属于当时的源码与二进制，不能用来认证本轮扩展计时后的代码。原最初慢组继续保持 inconclusive。
待失败调查得到处置后，才运行修订版存储采样、重新校准、登记并执行三组配对正式测量，
再评估其余平台/合同门禁与 PD-0 退出。没有 push、PR、merge 或 release。
