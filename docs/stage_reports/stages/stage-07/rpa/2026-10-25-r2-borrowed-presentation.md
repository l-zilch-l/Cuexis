# R2 有界借用行实施与本地验证

日期：2026-10-25（Asia/Shanghai）。状态：首版候选已实现、正确性与包边界门通过；局部诊断完成，候选未接受，R2 收益门未完成。本报告不裁定 R4、RPA、PD、设备/生产接受或 Stage 7 关闭。

设计与门禁由[整改计划 §7](../../../../stage_plans/active/stage-07/plan-rpa-performance-remediation.md#7-r2有界借用行原序写入最终-snapshot)持有；R1 当前退出结论见[R1 本地记录 §11](2026-10-25-r1-continuation.md#11-当前修复构建有限-pilot-与-r1-退出)。

## 1. 授权、起点与范围

用户本次要求使用无上下文、Max 思考强度的 Luna 子代理理解当前进度和既定设计，并由主代理独自设计、规划和开始实施 R2。子代理只读核对；本报告中的具体设计、批次安排、代码和验证均由主代理完成。该授权不扩大到 R3/R4、正式窗口、40k、ETW、真实设备或提交/推送/合并。

主文档树为 `D:/Cuexis-worktree`，分支 `stage-7`，HEAD `0157486800f8f40a4121ef0d5ab26bd041a08304`；实施树为 `D:/Cuexis-pd5-40k-admission`，分支 `codex/pd5-gameplay-40k-admission`，HEAD `3e5837f63f76397429c9f7603fd7450d3ce9855d`。开始时分别有 19、68 条既有 dirty 状态；两树完整 status 及已登记的 94 项路径哈希与交接清单一致。桌面文档本身的登记哈希早于最后一次编辑；另存当前原始字节并绑定当前身份，没有改写旧清单。

证据根统一为实施树 `out/rpa-r2-20261025/`。`start-manifest.json` 保存预计路径的存在/缺失状态、原始字节哈希及两树身份；后加的 `tests/playback/CMakeLists.txt` 也在编辑前补了快照。它不是整树一键回滚包。具体批次在 `implementation-plan.md`。

## 2. 实现与保持的语义

生产代码只改四个内部文件：Runtime 的 `presentation_read.hpp/.cpp` 与 Playback 的 `presentation_materializer.hpp/.cpp`。既有 `readObject` 及 Gameplay 发布使用的 `hasRenderable` 保留；没有改变安装头、公开 SDK/DTO 布局、R1 Close 代码或 Judgement/Gameplay 语义。

Playback 按原布局顺序将最多 32 个 `runtimeIndex` 收集入栈上索引数组。Runtime 新增内部 `visitSnapshotObjects`，在现有同步读取窗口内向固定容量行填入 ID/material 的 `string_view`、真实 WorldTransform 矩阵地址和中性标量，然后同步调用消费者。相较计划的示意接口，增加调用方提供的固定 extent 行存储参数，使索引、行和实际消费上下文能由同一个真实 scratch aggregate 计量。Runtime 仍负责 gather；registry/entity 不进入此接口。

gather 保留只读 World，使用 const registry 的 valid/try_get，不构造错误字符串、不解析资源；先判索引边界，越界只写行状态。该段为 `noexcept`，当前依赖实现的 const 查询路径不分配；预热后的 C++ 分配计数另有测试。Playback 消费到对应行才构造原错误，不会让后行越界覆盖前行资源错误；失败后不进入下一块。矩阵直接复制到最终 owning DTO 一次。

外层 owner、完整 stamp、window/serial、mutation/reentry 排斥及 RAII 结束未改。对象顺序、相机处理顺序、错误码/消息/上下文、失败前部分写入和最终对象数恢复保持。每次成功提帧都恢复 DTO 全字段。唯一材料 key 与当前 Appearance 精确匹配时才走单材料快路径；多材料与不匹配情况仍走原 map 查找。没有引入跨帧缓存、池、持久索引副本或借用逃逸。

## 3. 现场布局与预算

当前目标为 MSVC 19.51.36256.0、MSVC STL 145 / update 202604、x64、C++20。Debug 使用 iterator debug level 2，Release 为 0。两种配置对实际当前类型的测量相同：

| 实际类型 | sizeof | alignof |
| --- | --- | --- |
| ObjectPresentation | 128 B | 8 B |
| Result&lt;ObjectPresentation&gt; | 144 B | 8 B |
| ObjectVisualRead | 72 B | 8 B |
| SnapshotReadScratch | 2648 B | 8 B |

完整 aggregate 含 32 个索引、32 个借用行、layout/snapshot 指针、offset、实际使用的单位矩阵和真实 padding。首版编译期上界为 4096 B，是内部候选预算，不是已测最优值或 ABI 承诺；已登记的 MSVC/STL x64 Debug/Release 布局另有匹配的 static_assert。该数值不包含编译器 frame、返回值、错误构造和相机工作，不能作为整个函数的栈峰值。

最早探针的 2584 B 未计入后来移入 aggregate 的单位矩阵；最终 Debug/Release 探针及代码断言采用 2648 B。历史反汇编的 136 B 口径未改写。

## 4. 验证

新增 9 个测试覆盖借用 owner/逃逸/旧 serial、重入/mutation、空集/K/尾块/超 K/SIZE_MAX、真实矩阵地址、默认字段、callback 失败/异常/bad_alloc 和预热成功路径零 C++ 分配；以及 DTO 污染、交错复用、跨 session/reload/unload、资源 identity、材料切换和 fallback、缺组件、相机错误与非连续布局索引。

STATIC 测试保留一份有原始源码身份的旧物化器作为迁移语义对照，逐字段比较结果与失败前已写字段，并比较完整错误及 cause/context。它不进入产品或安装包。跨行首错反例包括同块和 31/32 边界上的“前行材料错误、后行 SIZE_MAX”。FrameDigest 是补充，不承担全部等价性证明。

首轮测试夹具把数字序当成 ChartCompiler 的稳定 ID 顺序，造成两条实现都发生布局错误；修正为明确的稳定 ID 后通过。失败日志保留，不称为生产修复的 red/green。新增 Runtime 4 cases / 95 assertions、Playback 5 cases / 6434 assertions 通过。

五个新构建均采用独立目录，未 fresh/覆盖旧 R1 构建。选项由旧 cache 核对：STATIC 的 Debug/Release 各 observation ON/OFF，Release SHARED OFF；candidate ON，metrics、execution diagnostics 与 allocation lifetime observer OFF，warnings-as-errors 与既有 R1 检查一致为 OFF。适用 Runtime/Playback/分配、RPA host/protocol/oracle 和 STATIC PD probes 全部通过；五组架构门通过。`matrix-test.json` 登记 47 条检查命令，前后源码哈希相同。SHARED 的一项私有 parse probe 和三项可执行文件分配注入按原边界跳过，STATIC 提供对应覆盖。

STATIC staged consumers 3/3、SHARED 6/6 通过，包括普通及 Playback 专用 find_package、实际安装的 Gameplay consumer；SHARED 另含 export surface 与两项 imports 门。新 SDK 包证据在 `integration-results.json`，没有继承 R1 的“库未变”豁免。八个本批 C++ 路径格式检查通过；全仓五条既有 PD 格式失败仍为范围外限定，没有宣称全仓格式通过。安装头 ASCII 由适用架构/消费者门核对。

## 5. 独立局部诊断

状态：唯一窗口按登记完成，9 个重锚加 6 个候选测量进程全部首次 exit=0、无超时或替换；每个进程另有独立 30 s 机器热身。工具与原始输出位于证据根 `diagnostic/`；登记 `formal_measurement=false`。

新建四棵独立树与新 Release 构建：A 为 `324d874` 的 pre-PD 产品加共同诊断夹具；B 为本次 R2 开始前的当前 R1 生产源；Q4 是在 B 上迁移的五文件直读消融；C 为 R2 生产候选。Q4 不是原样 Q4，也不是产品候选。旧登记工具所选 fixed workload 的现存源码断言对象数为 1，本次不把旧窗口当 N=64 证据。

共同诊断工具沿用 `measurement_method_tests.cpp` 的公开 reusable extractFrame/read_long、原 RPA 输入和验证/计时边界，采用已有 scalePresentation 的资源/viewport/Behavior 夹具。N=64 为重锚主点；生产候选对照保留原 1/8/64/256 规模，读取数分别为 2097152/1048576/262144/65536。每规模 fresh session 预热 64 steps、每进程先做 30 s 机器热身、每规模 8 blocks 且各至少 100 ms，普通 allocator，原 mask=4。四树功能预检在四规模的 packed/Replay/Snapshot identity 和 digest 全部一致。

一次窗口先固定 A-B-Q4 / B-Q4-A / Q4-A-B，再固定 B-C / C-B / B-C。只使用三对进程中位数和有限诊断区间，不把 8 个块当成独立进程。身份、超时、非零退出、短块或输出不符即停止；噪声及区间不达线按原数据报告，不加轮或重跑。1.19 调查线只针对 Q4/A；达到时先由主代理复核构建与成本结构，再判断已登记的独立候选对照。

完整比值采用三对进程中位数的几何比与固定方法的 95% 区间，全部样本保留：

| 对照 | 几何比 | 95% 区间 | 本地解读 |
| --- | --- | --- | --- |
| N64 B/A | 1.5828 | [0.7940, 3.1549] | inconclusive；噪声未过 |
| N64 Q4/A | 1.2098 | [0.7039, 2.0793] | inconclusive；触发 1.19 调查，噪声未过 |
| N64 Q4/B | 0.7644 | [0.6171, 0.9467] | 有限样本比值下降，但噪声未过；不是 1.19 调查对照 |
| N1 C/B | 1.2532 | [1.2019, 1.3068] | 明显退化；当前候选不接受 |
| N8 C/B | 1.0611 | [0.9915, 1.1355] | inconclusive，且有噪声限定 |
| N64 C/B | 0.9700 | [0.9390, 1.0020] | 点估计改善 3.0%，区间跨 1，收益未确认 |
| N256 C/B | 0.9845 | [0.9075, 1.0681] | 区间跨 1，收益未确认 |

原始统计助手对 N64/N256 C/B 给出的 pass 只指相对当前 B 的比值区间上界低于 1.10，不表示证明有收益，更不是相对 A 的 R4 正式通过。没有跨两段窗口拼乘出 C/A 或用旧 Q4/A 代替新证据。

噪声另见 `diagnostic/analysis.json`：同时列块内范围和三个进程中位数之间的范围，限值仍为 15%。重锚 A 的进程中位数间范围为 32.35%，其首进程块内为 61.30%；B/Q4 也有超线块。候选对照的 N1 两侧、N8 B 有噪声限定；N64/N256 各自块内及三个进程中位数范围均未超过 15%。没有剔除、补轮或追测。

主代理在重锚后复核了全部登记源/构建/编译器/DLL/工具身份及 Release /O2 /Ob2 /MD、普通 allocator、插桩关闭与四规模输出闭合，未发现漂移。源码结构上，A 的布局直接持 entity，Q4 保留中性读取窗口、available 检查、runtimeIndex→ObjectEntityMap 间接访问和独立物化翻译单元；当前 Q4 并不恢复成 A 的全部成本结构。这些是源码事实，没有把其差额量化为根因。复核留在 `reanchor-investigation.json`，随后只完成已登记的独立 B/C 对照，停止沿用旧阶梯收益分解。

因此，首版虽然功能等价，仍不能作为性能整改结果接受。R2 收益门保持未完成，当前候选不进入 R4。

## 6. 回滚与未关闭项

`rollback/code/manifest.json` 登记 R2-only 九路径补丁，SHA-256 `3386f06a3cb6f155af8369178599d076702f8db0ec0940151e9b6926a0318286`。已在隔离目录验证起始字节 → 正向 check/apply → 结束字节 → 反向 check/apply → 起始字节，存在/缺失状态及 SHA-256 全部恢复；使用 `core.autocrlf=false`，未在现场反向应用旧 R1 patch。该补丁不包含整棵 dirty 树或四棵诊断树。四文档独立补丁与同样的精确恢复验证保存于 `rollback/documents/manifest.json`。

R0 的历史 partial/inconclusive、导出 noIndependentContract 和 PD-5 原裁定保留。当前诊断不能代替 R4 的 95% 区间上界 ≤1.10 或原 15% 门；没有 40k、ETW、设备或生产接受证据，也没有跨平台 hosted CI。尚未提交、推送或合并。


## 7. 本批处置与下一步

本批以“正确性门通过、候选性能未接受”交付；实施树保留未提交候选及独立补丁，旧 R1 产品和证据锚不变。R2 未退出，主文档树未合入产品实现。

已做静态编译结果核查：Release 物化对象的 `fillObjects` 在循环前无条件调用 32 行数组默认构造，数组构造体确实写入全部 32 行默认字段；随后 gather 又对实际行置值。它证明新候选含额外固定工作，不能据此声称这已解释全部 25.3% 退化。反汇编、对象身份与摘录见 `logs/materializer-disassembly.*`、`materializer-fixed-scratch-excerpt.txt`。

下一批 R2 先限于定位未用行初始化、实际行再次置值及两阶段 gather/consume 的固定成本，选择同一同步借用合同内的最小修正；K=32 仍不是扫参许可。任何后续候选须有新的字节起点、构建与诊断登记，保留本窗全部失败/不确定结果，不重跑本窗、不追加样本到通过。这里没有实施第二候选，也没有引入池、跨帧缓存、裸 registry 产品通路或改变首错/部分写入语义。
