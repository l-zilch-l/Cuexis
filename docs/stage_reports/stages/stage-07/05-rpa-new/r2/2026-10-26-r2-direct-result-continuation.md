# R2 direct borrowed Result 续接与固定诊断

状态：local verification complete；候选未接受，R2/R4 未退出。

日期：2026-10-26（Asia/Shanghai）。

用户本次请求“阅读接手文档，继续实施RPA-R2”，续接的是已实现并整合的第三候选。
本轮完成剩余核验和唯一固定局部诊断；没有增加第四机制。当前状态以
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md) 为准，设计和停止门由
[整改计划](../../../../../stage_plans/active/stage-07/05-rpa-new/plan.md) 持有。

## 1. 源码与对照身份

唯一实施根为 `D:/Cuexis-worktree`，分支 `stage-7`，本轮 HEAD 保持
`cccbd47eda8ecd65e018ec31063ec027c39bd04e`。接手文档哈希、HEAD 和九个 R2 源码哈希逐项相符。
文档目录整理及 checker 适配的既有未提交改动保留；本轮没有改动产品源码、测试、构建输入或新增产品机制。
`D:/Cuexis-pd5-40k-admission` 仅作只读历史来源，未在那里构建、执行旧脚本或重跑旧窗口。

全部新源码副本、构建、登记和日志在 `D:/Cuexis-worktree/out/r2e-20261026/`。
`start/state.json` 保存开始时完整现存受控/未跟踪源码哈希与 Git 状态，`start/` 保留桌面两文件原字节。
本轮主代理亲自完成设计边界核对、构建、测试、对象码检查及诊断，未使用子代理。

B 的四个读取产品文件来自已核验归档 `Cuexis-r2-direct-baseline-20261025.zip` 的冻结 R1 真实字节；
其余文件应用两侧相同的当前整合修复与格式整理，另用归档的共同诊断夹具。
`preparation.json` 逐项登记冻结字节到共同修复的哈希增量、归档哈希和 B/E 全部源文件哈希。
因此本次 B 是重新绑定的 R1 读取对照，不能沿用旧 B 二进制或旧窗口身份。
E 四个产品文件与主树第三候选完全相同；B/E 的产品差异严格限于 Runtime 的
`presentation_read.hpp/.cpp` 与 Playback 的 `presentation_materializer.hpp/.cpp`。
共同夹具字节一致，四规模 Packed、Replay、Snapshot identity 和 FrameDigest 在两个新构建中全部一致。

第三候选保留 owning `readObject` 和 Gameplay 发布，Snapshot 使用 `Result<ObjectVisualRead>`
返回借用行，在现有外层同步读取 callback 内由原序循环立即物化；逐对象 typed Result 仍然存在。
没有逐对象消费函数指针/可变上下文、跨帧缓存、池、新线程或公开 Registry 通路。

## 2. 当前核验及历史证据复用边界

上一轮完整五构建/回归及消费者证据经日志与库哈希重新验证；实现输入与
`96e48e4ba644ce091d6ce1d4c7c1e1aa7b859db7` 的受控产品/测试字节一致。
沿用消费者仍绑定原 `4afe577`，没有把它写成本轮重新运行。详细原范围见
[整合报告](../../90-shared/worktree-consolidation/2026-10-26-worktree-consolidation.md)。

本轮额外亲跑下列适用 R2 和架构检查，首次测试执行均 exit=0：

| 当前配置 | 本轮执行 | 失败 |
| --- | --- | --- |
| Debug STATIC observation OFF | 9 个 R2 cases + 架构，10 项 | 0 |
| Release STATIC observation OFF | 9 个 R2 cases + 架构，10 项 | 0 |
| Debug STATIC observation ON | 9 个 R2 cases + 架构，10 项 | 0 |
| Release STATIC observation ON | 9 个 R2 cases + 架构，10 项 | 0 |
| Release SHARED observation OFF | 5 个适用 R2 cases + 架构，6 项 | 0 |

合计 46 项。SHARED 的内部私有物化器对照不在原注册范围内，不能称为本轮执行了 STATIC 的全部九项。
`regression.json` 保存历史核验和本轮逐命令日志。
验证脚本起初误将适用项数写为所有配置 11 项，随后改为 STATIC 10 / SHARED 6；
两次计数断言错误均发生在 CTest exit=0 之后。修正记录校验后沿用同一次成功日志，没有重跑通过项。

Runtime 核验包括 wrong owner、逃逸/旧 serial、重入/mutation、first/last/first-outside/SIZE_MAX、
真实矩阵地址、缺组件、callback error/throw/bad_alloc 的 RAII 和预热零 C++ 分配。
Playback 以冻结旧物化器比较全字段/default/resource identity、DTO 污染恢复、跨会话/重载/卸载、
首错/上下文/部分写入、0/1/31/32/33/65 对象、非连续索引和相机；digest 仅作补充。
SHARED 不据此声称跨 DLL 全局零分配。

另在新隔离 B/E 下分别完整构建 Release 普通 allocator 的 `cuexis_pd_baseline_probe`，
两侧 warnings-as-errors 保持 ON，observation/metrics/execution diagnostics/allocation observer OFF。
登记时再次实际执行无编译工作的构建检查，二进制哈希不变；编译器、全部 Ninja 传递头依赖、
库、DLL、CMakeCache 和 compile commands 均绑定到登记。

现场实际类型探针使用主树材料化目标的真实编译选项，Debug/Release 均编译并执行：

| 实际对象 | Debug sizeof / alignof | Release sizeof / alignof |
| --- | --- | --- |
| ObjectPresentation | 128 / 8 B | 128 / 8 B |
| Result&lt;ObjectPresentation&gt; | 144 / 8 B | 144 / 8 B |
| ObjectVisualRead | 72 / 8 B | 72 / 8 B |
| Result&lt;ObjectVisualRead&gt; | 144 / 8 B | 120 / 8 B |
| 实际 fallback Mat4 | 64 / 4 B | 64 / 4 B |
| Runtime 行 + 调用方 Result + fallback Mat4 保守逻辑合计 | 280 B | 256 B |

工具链为 MSVC 1951、STL 145 / update 202604、x64、C++20，iterator debug level 分别 2/0。
内部 ≤512 B 断言保持；合计不含编译器 frame、其他临时对象、gather closure、相机/错误工作，
不是全调用链栈峰值或跨工具链 ABI 保证。`layout.json` 保存当前实测。

主代理亲读新 Release 对象码：E 的物化器在对象循环直接 call `readSnapshotObject`，
B 仍 call `readObject`；两者均无第二候选的 `visitSnapshotObject` 和 `SnapshotReadScratch`。
该证据只确认编译路径，不能量化各条指令的收益。原对象、symbols/disassembly 日志及哈希见
`object-code-verification.json` 与 `logs/`。

## 3. 精确回滚

`rollback/third-incremental/verification.json` 绑定第三候选 `start/` 的五路径增量；
`rollback/cumulative-r2-code-only/verification.json` 绑定首版 `start/` 的完整九路径代码增量，
包括两个新增头的原缺失状态。均在隔离目录完成起点哈希 → forward check/apply → 终点哈希 →
reverse check/apply → 原始存在状态和字节恢复，使用 `git -c core.autocrlf=false`。
主树没有反向套用补丁。早先累计全快照还含一份 `tests/pd/README.md` 导航改动，保留为辅助证据，
不把该十路径补丁冒称九路径机制补丁。

## 4. 唯一固定诊断

登记 SHA-256：`56a55792f8f4c270cd40a7476cbdd3d1ee67b48c581bca425760d53168aa9e47`。
`formal_measurement=false`。复用夹具中的 `formal` phase 字面量仅选择八个 blocks，不将本轮升级为正式窗口。

新 B/E 按 B-E / E-B / B-E 顺序执行三对六进程。每进程 30 秒机器热身，
每规模 fresh-session 64-step 预热，8 blocks，每块至少 100 ms，mask=4，viewport=1280×720。
N1/N8/N64/N256 的每块读取数分别 2097152/1048576/262144/65536，走公开 reusable
`extractFrame` / `read_long`、普通 allocator，无插桩。身份漂移、非零退出、超时、短块或输出不符立即停止；
噪声和性能结果保留全部样本，按已登记固定组完成后裁定，不换样本或加轮。

六进程均首次 exit=0，无超时、替换或身份/语义停止；192 个读取 blocks 全部满足时长门，
最短 537.365 ms。每规模先取各进程八块中位数，再对三对进程的 log 比值用
固定 t(2)=4.302653 计算 95% 区间；不把 blocks 当独立进程。

| E/B | 几何比 | 固定 95% 区间 | 最大块内范围/中位数 | 最大进程间范围/中位数 | 结论 |
| --- | --- | --- | --- | --- | --- |
| N1 | 0.9162 | [0.6249, 1.3435] | 22.67% | 23.66% | 噪声超线，收益未确认 |
| N8 | 0.7981 | [0.5256, 1.2118] | 6.86% | 30.07% | 噪声超线，收益未确认 |
| N64 | 0.7476 | [0.5437, 1.0279] | 3.38% | 25.77% | 噪声超线，收益未确认 |
| N256 | 0.7340 | [0.5350, 1.0071] | 1.88% | 25.31% | 噪声超线，收益未确认 |

主点 N64 点估计变化为改善 25.24%，区间与噪声门须共同解释，不能只择点估计。
块内、进程间两类范围均须 ≤15%。`diagnostic/results.json` 保留全部原始块和进程中位数；
主代理另从六份 raw 独立复算 192 块、三对几何比、固定区间及两类噪声，
与控制器结果逐项一致，见 `independent-results-audit.json`。

## 5. 处置与边界

第三候选固定 B/E 六进程诊断完成；N1/N8/N64/N256 超过 15% 噪声门，R2 整体收益门仍未完成，R2/R4 未退出。本轮补齐第三候选的固定局部证据，不能据此声称正式相对冻结 A 的 1.10 门通过。
即使某规模的 E/B 区间上界 ≤1.10，它也只回答新 B/E 的局部对照，不是 R4 正式身份。
旧 A/Q4/C/D 窗口未重跑，未拼乘旧比值，前两候选的失败和噪声限定保留。

本批第三候选是最后一个已登记机制，完成后停止本批实验；保留当前未接受源码和全部证据，
不自动回滚主树、不追加轮次、不产生第四机制，不进入 R3/R4。
后续需对本轮结果作明确处置后再确定工作，不能将“继续 R2”解释为无限尝试。
未运行 ETW/WPR、40k 性能测量、设备、GPU smoke、Linux/MinGW/hosted CI，未 push、PR 或发行；
PD-5、RPA/设备/生产接受和 Stage 7 关闭均不由本轮替代。
本轮未重跑完整五配置全量 CTest 或嵌套消费者；沿用与新测范围明确分列于 §2。

本轮文档变更前原字节在 `docs-before/`。文档/版本/diff 检查及最终源保全结果另见
`final-verification.json`；该文件仅在对应检查实际通过后生成，不用本报告文本代替命令证据。
