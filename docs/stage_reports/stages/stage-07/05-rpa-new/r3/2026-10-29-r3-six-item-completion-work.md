# R3 六项优化实施与验证账本

状态：进行中；候选尚未全部验证，不是 R3/R4 退出

日期：2026-10-29

返回 [RPA 报告索引](../README.md)。当前阶段状态只见
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md)。owner 要求连续完成桌面接手文档 P1–P6，
不再按单个局部候选结束工作；原预算、错误语义、身份字节和历史证据保持原要求。

## 当前工作与完成条件

| 项目 | 本批实施/调查 | 尚需完成的核验 |
| --- | --- | --- |
| P1 Packed / re-prepare | 名称索引与随后独立 Packed 查找候选分别通过固定诊断保护条件；后者 40K unique 总耗时 23.58 → 7.09 s | identity 投影仍重复生成；最终 fresh 回归与原端到端 load 门未完成 |
| P2 准备热点 | 保留名称索引，消除逐 requirement 的 namespace 全表查找；40K unique source prepare 6.404 → 1.533 s；identity 复用候选正在 fresh 编译与字节回归 | 候选尚无测量结论；timer/resource 不因这次局部改善而宣布完成 |
| P3 update / Snapshot | 保留先前四属性合并查找和 touched 优化；新增 resolver 索引提示因真实 GPU 保护条件失败而撤回 | 最终源码完整矩阵；extract 仍有可调查成本，不重做 R2 表示 |
| P4 编码与真实内存 | 第二次使用 admission 和测量预遍历计划均无实际峰值收益，已放弃，恢复旧独立字节缓存；已取得至 40K 的实际峰值及最终释放 | 本地轨迹不替代原 caps 或历史 R4 身份参考；编码输出本身占用及同时持有范围必须明示 |
| P5 端到端 Seek | 内置 Host 可转发既有 checkpoint span；CPU 36 组合与 1080p GPU 96 组合核对独立 origin 全字节、目标 tick/context/receipt/像素 | 原尾延迟及重复性、观察路径与故障覆盖；worker 替换并非当前 Host 能力 |
| P6 维护/工具/矩阵 | metrics TLS 改为函数内每线程对象，消费者同步；全仓格式债处理；候选版 Release 全套通过、Debug 编译通过 | 撤回/新候选后的最终 fresh STATIC/SHARED、Debug/Release/MinGW、安装消费者与完整 CTest |

这些是候选及核验账目，不将“已写代码”记成完成，也不以 R3 技术工作代替设备/生产或 owner 接受。
40K 历史 Packed reference 缺失、B0 caps 的规模适用性与原 R4 固定窗口保持原证据边界。

## 已取得的独立证据

统一根为 `out/verification-20261029/r3-completion/`，起点 `4e9cea6`。

- `cpu-reset-measure/`：固定 36 新进程 / 3600 帧，normal allocator、metrics OFF、mask 4、BelowNormal，
  实际 update / reusable extract，无 GPU。少做重复 reset 的候选改善动画约 18%，但 10K static
  中位数上升 11.26%，不满足预设 guard，已撤回；原值不替换、不追加。
- `cpu-hint-measure/`：新的独立 36 新进程 / 3600 帧，同一明确 CPU 边界；40K static update
  中位数 5.401459 → 2.851241 ms，animated 50.117857 → 44.217372 ms。
  前一组真实 GPU 使用全部允许 CPU，本组是 mask 4 的 CPU 诊断，不能拼接成同一连续基线。
- `metrics-tls/`：真实 MinGW 两个 translation units，旧 inline 非平凡 TLS link 退出 1，原 multiple
  definition 日志保留；新实现 link / run 均退出 0，验证同线程跨 TU 同址、不同线程独立存储。
- `format/`：按 CMake 同一范围扫描 643 个 cpp/hpp/hpp.in，7 文件需要整理，前后源码/哈希和完整
  dry-run 日志保留。后续新文件与新改动仍要在最终门重查，不以此前通过替代最终源码检查。

偏移缓存的静态审阅发现既有 `rpa_b3_tests.cpp` 明确要求 Writer 输出可 clear / edit 后继续编码，
因此仅靠注释约束 append-only 不够；该版本已撤回并保存源码，不能削弱原测试来保留候选。

## 固定组结果与放弃决定

`isolated-probes-v1` 命令抽取错误和 `isolated-probes-v2` MSVC getenv 警告失败均保留。
有效独立探针为 `isolated-probes-v3`；相同 fresh Release 库，只重编并显式覆盖对应旧 TU，
分别隔离 prepare/assembler、codec/wire。canary 6 进程通过，raw 文件逐字节对照通过；
普通 allocator 性能与 lifetime observer 内存结果分开。

- `isolated-prepare-v1`：36 新进程，1K/10K/40K，uniform 与 unique named Pattern。
  uniform 的 source prepare + Packed encode + decode 总和中位数分别变化 +1.25%、-2.33%、
  +0.18%，不满足预先登记的各规模改善；40K baseline relative range 为 15.49%，原值保留。
  unique 40K 总和约 34.64/34.69 s，source 与 decoded prepare 中的逐 requirement
  `findDeclarationByName` 全表扫描已经源码核实为平方项；尚不能把全部时间归于单一函数。
- `isolated-recovery-v1`：18 普通进程 + 6 observer 进程。第二次使用 admission 的编码域峰值
  各规模均未降低；1K Snapshot/Replay 中位时间增加 10.41%/12.47%，40K Replay 增加 5.40%，撤回。
- `wire-census-v1` 与 `isolated-census-v1`：另一独立候选利用 measure pass 的重复 owner 计划，
  保持默认 Writer 可编辑输出合同。1K/40K 各 A/B observer 的编码域及全局 requested 峰值完全相同，
  在内存准入阶段放弃，不继续投入普通时间对照，不把 observer 时间当性能收益。
- 40K 实际 Snapshot/Replay 输出为 914,298,240 / 887,542,111 bytes；这条轨迹同时保留两份输出，
  全局 C++ requested 峰值 2,421,244,925 bytes。最终 ownerProbe + ownerMain 及 Catch Session
  销毁后的全局 linked C++ live count/bytes 归零，分区 exact。冻结观察器 exceeded=true，不能报
  原 caps 通过；CRT usable 包含 observer header/alignment，进程 commit 还包括其他堆，二者不混称。
- `final-gpu-v1`：36 物理 GPU 新进程 / 2,160 帧，1080p、RTX 4060、metrics/timer OFF、normal allocator，
  逐进程验证入镜数量、实际 draw、橙色像素、动画移动和目标分辨率。resolver hint 的静态 update
  明显下降，但 10K animated update +5.99%、整帧 +6.42%，未满足预设保护条件；1K animated
  update 亦未改善，故撤回。CPU 单核诊断的改善不能替代实际 GPU 组的放弃条件。

放弃源码、精确恢复文件哈希及决定见 `rejected-candidates-v1/decision.json`。
回退只撤回本批无收益候选，保留 `4e9cea6` 的已验证 Runtime/world 优化，以及本批 Seek 覆盖、TLS
修复、格式债清理和恢复输出可编辑/版本测试。新方向必须有新源码、登记和固定对照，不追加旧组。

`name-index-v1` 是随后独立构建的 namespace 名称索引 + 内部 namespace move 组合，定向 Judgement
233 cases / 56,071 assertions 通过。`name-index-measure-v1` 的 36 个新进程完成，所有身份和
Packed 原始字节对照通过；unique Pattern 的 1K/10K/40K 总和中位数下降 2.19%/28.44%/29.31%，
40K 为 33.319309 → 23.554490 s。但 uniform 40K 增加 6.74%，超过预设 5% guard，当前组合不保留。
下一候选拆除附带的 namespace move，单独检验复用既有排序索引，另建源码/二进制/固定组；
不能把这次大规模 named 改善描述为整个组合已经通过。

`name-index-v2` 恢复原 namespace copy，仅保留 merge 已生成的名称索引。索引是调用期的声明位置，
copy 和随后 stabilization 保持 stable-ID 顺序；重复检查和首错顺序不变，无跨调用缓存或借用逃逸。
独立 overlay 的 Judgement 233 cases / 56,071 assertions 再次通过；这不是 fresh SDK 全套结果。
`name-index-measure-v2` 新登记 36 个普通 allocator 进程，完整 Packed / 三种身份 raw 字节一致。
unique 1K/10K/40K 总和中位数下降 2.65%/20.23%/32.52%，40K 为 33.709632 → 22.745950 s；
其中 source prepare 6.403610 → 1.532572 s，Packed encode 11.960856 → 11.924411 s，
decode 14.852734 → 9.286722 s。uniform 总和分别 -0.98%/-0.06%/+4.85%，均在预设 5% guard 内，
各组 relative range 均小于 10%。因此保留单独名称索引，不保留 move。4.85% 上升必须保留，
不描述为普通路径无变化；该准入仍不是原 15%/置信区间正式性能接受。

`packed-lookup-v1` 隔离 Packed 字典查找及重复名称索引复用，两版共同使用已保留的 namespace
索引与原 prepare/codec，common 库冻结。独立审阅核实了模型字符串生命周期、首错顺序和
Pattern 排序忽略字段时的完整相等性要求。新增字典顺序/重复项/排序等价非法项测试；新旧两版
各 51 cases / 5,641 assertions 通过。`packed-lookup-measure-v1` 固定 36 个新进程完成，
Packed 和三种身份的完整 raw 字节一致；unique 1K/10K/40K 总和中位数分别下降
3.09%/39.15%/69.92%，40K 为 23.580411 → 7.091884 s。其中 Packed encode
12.427108 → 1.797437 s，decode 9.488453 → 3.723324 s；source prepare 未被本候选修改。
uniform 总和 +4.06%/-5.04%/-2.20%，通过预设 5% guard；各组 relative range 最大 11.27%，
原始变动完整保留。该候选保留；不同登记组不能直接相加声称连续基线收益或正式接受。

## Seek 目标图像与异常复测

`seek-gpu-v1` 命令抽取失败和 v2 缺少内部测试 include 的编译失败保留；v3 初次执行在
FactBinding 内容校验停止，原因是测试夹具使用了未声明的新 binding ID，尚未进入 Seek。
修正夹具后，v5 / `seek-gpu-measure-v2` 在 RTX 4060、1920×1080、VSync OFF 完成 96 组合，
21,154 assertions 通过。覆盖 33/129 记录、origin/early/final/partial、native/external、
无/候选/剥离证书 hint，以及 owning-source/canonical 两种身份策略；外部 hint 从未成为可信证书。
native checkpoint 生成会计算 canonical identity，因此其创建也纳入计时，并单独报告该耗时。
独立 oracle 从外部解码来源原点重放，计时外比较完整状态 wire；Host 帧、receipt、scope 和
实际橙色/背景像素一起检查。窗口隐藏，只证明实际 GPU back-buffer readback，不是可见扫描输出。

v6 补齐 eligibility/horizon/viewport/GL/swap 诊断并编译；v7 继续分解 sample、Host present、
第二次相同图像的 direct submit 与 swap。第二次 draw 是归因探针，不替换原首次 Host 区间。
`seek-display-rounds-v1` 预登记 3 轮×隐藏/显示共 6 个新进程；owner 质疑复测范围时中止，
实际完成两整轮及第三轮隐藏组，共 5×96=480 组合，最后显示组的部分日志保留，不补样或称三轮完整。
用户随后明确允许优先复测方法可能有错或初测异常的测试，既有停止组仍保持原状。

v5 的请求到读回记录为 0.711–200.402 ms。新分段组首次 Host present 中位数为
0.348–0.634 ms，未稳定复现原首次慢点；第二次 GPU submit 则在隐藏/显示两种窗口下稳定出现
约 178–189 ms 中位数，swap 本身约 0.044–0.048 ms。这将当前稳定问题定位到第二次 GPU
submit/readback 路径，尚未证明具体 GL/驱动机制；窗口是否隐藏不解释它。
原首次慢点标为偶发/未稳定复现，保留原记录，不能删除或据此宣布尾延迟通过。

后续复测优先选可能有方法错误、初测异常或相互矛盾的代表场景；无相关代码或条件变化时，
不反复运行已有稳定结论的完整矩阵。新增对照另行登记轮数、停止条件和判据，保留每次结果；
正式 15% / CI 与原预算继续适用。

## 构建、版本与原门限

所有新构建串行、BelowNormal、affinity 3，由既有 Windows Job 包装器约束后代。
旧绑定构建/二进制只读，采用新的独立构建目录。Release 首次配置因 PowerShell 对未引用 suffix
拆分失败，原目录与错误保留；有效新构建是 `release-static-v2`，参数由 Python argv 传递。
当前 SDK 仍为 0.9.1，未添加公开安装字段或改变 wire / codec 版本。

`release-static-v2` 已完成 fresh Release STATIC 全量构建及 CTest：1,408 项，0 失败，
1 项物理符号链接测试跳过。恢复构建日志为 `release-resume1-build.log`，全量测试日志为
`release-resume1-ctest.log`，退出码及命令见 `release-resume1-results.json`。
首次构建因新测试错误地默认构造必须携带 PreparedIdentity 的 KernelProjection 而失败，
原日志保留；测试改为使用真实 assembler 产生的身份后再构建、运行全套，不删除该反例。
该结果覆盖当前候选的常规功能回归，不能替代性能测量、观察通道路径证明或其他工具链矩阵。
后续独立 Debug 构建目录为 `debug-static-v1`。

`debug-static-v1` 全量编译成功；CTest 在预先设置的性能分阶段等待处被对话中断，尚未执行，
保留 `debug-performance-stage-entered.json` 和未完成日志。最终源码改变后重新执行独立矩阵，
不能把这个候选编译结果记成最终 Debug 全套通过。

原适用 Seek ≤20 ms、request/ready/GPU p99≤4.167 ms、初次 load≤2000 ms、15% 重复性和正式
配对置信区间要求均未放宽。未完成、失败、不可比与缺外部证据的项必须分别记录，不追加到通过。
