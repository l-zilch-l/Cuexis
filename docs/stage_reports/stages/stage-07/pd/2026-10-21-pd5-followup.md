# PD-5 evidence correction and finite follow-up

日期：2026-10-21，Asia/Shanghai。角色：分批实施证据与交回核验材料。
当前状态以 [CURRENT_STATUS](../../../../CURRENT_STATUS.md) 为准；
[已接受协议](../../../../stage_plans/active/stage-07/pd5-measurement-proposal.md)不变。

## 起点与保护

基础 HEAD 为 2ab7b44，工作区干净；先做空提交留档 62ca993。SDK 0.9.0，构建 26.10.19-1。
证据根为 `out/pd5-followup-20261021-62ca993/`。旧窗口 3 不续跑，所有历史证据只读。
本轮只授权到第 5 批交回核验，不启动正式注册/窗口，不 push/PR/merge/release。

## 第 1 批：问题表与纠正

下表历史路径均相对于 `out/pd5-rootcause-20261020/`，正式两窗路径另注明。

| 问题 / 已核事实 | 原始证据 | 当前解释与尚缺实验 | 门禁影响 |
| --- | --- | --- | --- |
| 修复前 RPA read_long 两窗约 1.5362/1.5754 | performance-formal-20261020 注册窗口日志 | 真实局部成本增加；完整第三窗缺失 | 两窗超界，非全组接受 |
| F1 213.3→276.5，旧 B 338.0 ns | ladder-fix-F1.log | 同轮差额回收约 49%；缺 F1 后独立配对区间 | 诊断效果，不替代正式 |
| 无 1.10 不可达证明 | patch-Q*、prototype-V*、源码 | 变体有限，保语义空间未穷尽 | 保留原阈值 |
| IPO 全样本高波动 | ladder-fix-ipo*.log | 前段约 7 ns 是选段结果；缺受控对照 | 不作优化极限证据 |
| Q3 1.0988 点估计且丢保护 | local_measure.py、patch-Q3-direct-read.diff | 历史 PASS 非配对区间；不可作为候选 | 未正式通过 |
| 规模为修复前 A/B | scale-analysis.json、scale-probe-block.cpp.txt | 2 进程/侧、3 块/配置/进程，2048 读/块；缺 F1 长块 | 不接受 F1/尾延迟 |
| 差额拟合 71.6765+27.0590N ns | scale-analysis.json | N=Snapshot 对象；固定 owner 探针为 1 对象，camera 另计 | 不推导 2 对象下限 |
| total 终点不同 | analyze_host_ready.py、host-ready-phase-analysis.json | ACK 与 present-return 各自报告；缺共同完成端点 | 禁止 total 配对 |
| 46.55/57.30 µs | 同上 window-1 | 差 10.75 µs，帧预算 0.258%，数值增幅 23.1%；因端点不同不能作整帧收益 | 原 total 标签无效 |
| 公共 ready 仍可比较 | host_delivery_probe.cpp、两窗日志 | 同逻辑工作/请求→ready；额外往返是原因假设 | 不抹掉 ready 增幅 |
| 小中位数与 p99 不同 | measurement_method_tests.cpp | 块平均成本不能当操作尾部；需每进程足量样本 | 未判 p99 |
| 10% 与 15% 是不同统计量 | 已接受协议 §3/4 | 配对进程 CI、重复性分别判定 | 无放宽、无自动豁免 |

历史完整性新入口 `tools/verification/playback_performance/historical_integrity.py` 不要求旧 HEAD，
也不声称当前源码可登记。`history-before.json`：341/442 文件哈希和 301 个历史绑定构建文件通过。
原清理脚本保持原样，其 HEAD 断言不适用当前候选。

验证命令与日志：`python -B tools/check_docs.py` → `batch1-docs.log`；
`git diff --check` → `batch1-diff-check.log`。本批不要求 C++ 构建。


## 第 2 批：工具契约

新增有限诊断分析入口，复用 measurement.py 的进程配对 log-ratio CI 和原阈值；
不把诊断写入 formal manifest。身份、计数、工作量、端点、输出和进程完整性先校验，
不同完成终点拒绝。旧 require_comparable 补上双方都缺身份字段时的拒绝。
构建入口增加显式 --build-dir，默认行为兼容，并为新路径拒绝误覆盖。

旧行为例：local_measure.py 的 1.0988 打印 PASS；新行为仅返回 diagnostic_*，acceptance=false，
3 对不足/重复进程、短块、不同 ready/completion 端点会拒绝。相同 ready 则允许比较。
块统计不会升级为单操作 p99；样本不足仍保留 None。原阈值 1.10/15% 及绝对预算未改。

本次 `python -B -m unittest discover -s tests/pd -p 'pd5*_tests.py' -v` 47 项通过，
日志 batch2-tests.log；文档与 diff 检查分别记 batch2-docs.log、batch2-diff-check.log。


## 第 3 批：有限诊断预声明

假设 H1：F1 仍有每对象中性读取/复制成本；H2：真实更新→提取与未更新热读的相对表现不同；
H3：公共 request→ready 的延迟仍须独立于 ACK/present-return 解释。主指标为各规模 read 的
每进程块中位；updated 为 update-start→Snapshot-ready 的组合成本，不能相减得到子项归因。
不采纳任何子项 p99 推断，不增加插桩或 IPO。

新正常计时构建为 out/build/pd5-followup-62ca993-f1；A 从冻结 implementation-inputs.zip
在证据根 baseline-source 解包，另建 out/build/pd5-followup-62ca993-a，仅增加同一个
read_scaling_probe.hpp include，host 两侧固定 32 预热+1024 测量、capacity=2048。
原冻结 A 不改动；适配 A 不等于原二进制，其归档/补丁/库与工具链另登记。
实际 build/cache/source/exe SHA256 在采样前冻结于 plan.json 并提交其摘要；运行时重新复核。

每侧 3 独立进程，顺序 AB/BA/AB；规模每进程固定 1/8/64/256，各自 read 与 updated：
3 块，read 次数分别 2097152/1048576/262144/65536，updated 次数分别
262144/131072/32768/8192。每配置先 64 次 update+extract；仅首对象有真实移动 Behavior。
每进程前 30 秒机器预热；本次不作校准。短块小于 100 ms 停单元并保留失败，不补样。
scale 仍绑定 mask=4（沿既定核心策略），host/生产使用冻结默认允许集合；不按结果挑核心。
normal allocator、Release STATIC、candidate ON、metrics OFF，无 ETW/GPU/audio。
环境快照随日志记录；OEM/后台安静程度不能独立保证时明确列为限制，不借诊断接受正式环境。

内容 chart SHA256、资源字节、序列、viewport、U/F/R、Snapshot 对象与相机数均绑定。
规模 registry 为 N 对象+1 camera，无 Gameplay requirement；更新轨迹不 Reset，不追加历史；
每块检查输出 digest，updated 必须变化，A/F1 必须精确相等。宿主在固定合法容量内完整走
1056 帧并逐帧对 reference oracle；任何容量/EOF/身份/输出错误停止该单元，无事后截短。
ACK 与 present-return 分项，enqueue/drain/GPU 缺无侵入边界，保持 unavailable。
宿主每进程 1024 操作可报告 p99，scale 仅块均值，不能报告操作 p99。

生产单元 B-only 每种 low/high reuse 固定 3 进程，40k entities/requirements，使用已有
PD5 full content Snapshot Seek Replay boundary；它与原 RPA-F3 Packed probe 内容分别命名，
检查完整 Snapshot/Replay/Seek/Close。次数少只报成本/max 与正确性，不作生产或尾部接受。

运行入口 finite_followup.py freeze/run，先冻结再提交登记摘要再执行；原始日志、进程退出码、
哈希及全部样本输出到独立证据根。错误停该单元；噪声与超界保留固定全部进程，不追加替代。
工具错误只可新身份再登记，不能覆盖旧 log。有限诊断始终 acceptance=false。

预检全量 Release 构建发现原有 gameplay_author 同时编译 gameplay_entry.cpp 却缺少
Judgement 私有 include 路径（build-f1.log C1083）。补齐 PRIVATE 路径，不导出 internal 头；
重建日志 build-f1-r2.log，受影响完整构建/架构验证随本轮留下。


构建准备记录：F1 全量 Release 第一次因上述 PRIVATE include 失败；第二次暴露本轮探针误拷
move-only PlaybackSource，已在每模式独立建立并 move，第三次完整构建退出 0，失败日志不覆盖。
A 冻结 implementation archive 未收录完整 tools/，首次配置因开发工具 allowlist 缺 target 拒绝；
适配 A 配置仅关闭不参与计时的 CUEXIS_BUILD_DEVELOPER_TOOLS，正常计时库仍为同工具链
Release /O2 /Ob2 STATIC、candidate ON、metrics OFF；不补入 B 库或产品源码。
此构建差异记入 plan 与比较限制，原 A 不重建。本轮 host 扩展通过显式
CUEXIS_PD_HOST_DIAGNOSTIC_1024=1 启用，原 formal 默认仍为 512/1024。


### 采样前身份冻结

登记 HEAD：4060c65。diagnostic/plan.json SHA256：
`8aaf06be862d2d838a72b957f3d3df7fe5876c5247ff5e7710dbb07c4b6b315e`。

| 侧 | 源码清单 SHA256 | baseline probe SHA256 | host probe SHA256 |
| --- | --- | --- | --- |
| A | `e3a79bec89fac999a0e0ff503851e68fd265aab9fe7476d31139b7aba66dd109` | `cb3b133a649ec48c85b1557cf41a832babfce611661cd334eb96fa2bbe02665f` | `2eee6441caffd56c0e0512fc3937d2f6da7ff40a20f3e7c8360160a48f363be7` |
| B | `1bfebe4b6e49dcc291136d3d2dec13e64e296341493e3e3d28023afcdecd6da3` | `ef1efdb280135cca40e573dd945706787cc8dc8191b0afb277b7e23c0b84ff6b` | `09408e92d05581748161cff660f0eecbb0381216987ca4e9671ef4777e2ff1ca` |

完整文件/库/cache/compile_commands、工具与配置身份见不可覆盖 plan.json；本段先提交再运行。


### F1 有限诊断结果

12 个 A/F1 进程（规模 6、公共宿主 6）完成，无身份/输出错误。规模每配置 3 对进程，
每进程 3 块，最短块仍满足 100 ms。以下仅为诊断，acceptance=false：

| Snapshot 对象 | F1/A read 95% CI | F1/A updated 95% CI | read 判定 |
| --- | --- | --- | --- |
| 1 | 1.2368 [1.2244, 1.2492] | 1.1046 [1.0663, 1.1444] | diagnostic_over_budget |
| 8 | 1.3441 [1.2979, 1.3919] | 1.0748 [1.0301, 1.1214] | diagnostic_over_budget |
| 64 | 1.3742 [1.3517, 1.3970] | 1.0417 [1.0278, 1.0558] | diagnostic_over_budget |
| 256 | 1.3967 [1.3523, 1.4425] | 1.0645 [1.0384, 1.0912] | diagnostic_over_budget |

read 各侧进程重复性均 <2.4%。updated 的 1/8 对象区间跨 1.10，64/256 的区间上界在
1.10 内；组合成本包含真实更新，不能当孤立 extract 成本。输出摘要在每个配置/块精确一致。
四点拟合 A 固定 181.4895 ns、斜率 41.0936 ns/对象；
F1 固定 205.6491 ns、斜率 57.5190 ns/对象；
只描述本轮 1–256 范围。拟合数据与全部每进程统计见 diagnostic/scale-fit.json、analysis.json
及各配置 input.json，可用 diagnostic.py 独立复算。

共同 request→ready 的进程中位（µs）：A [22.0,30.1,19.2]，F1 [38.5,40.4,37.6]。
配对几何比 1.6631，95% CI [1.0272,2.6927]，A/F1 波动 49.55%/7.27%，故不可判定。
A 每进程 ready p99 为 51.8/56.7/49.5 µs，F1 为 84.1/86.9/77.6 µs；样本均为 1024，
只覆盖这条短闭环轨迹，不接受整个 host/SDK。ACK/present-return 原样分项，禁止 total 比值。
enqueue/drain/GPU 仍不可用，未据内部往返“解释掉”公共 ready。

40k Gameplay-host 的 low/high 首进程均退出 42，Packed 编码拒绝 Packed string budget exceeded，
未进入宿主加载/提取/Snapshot/Seek/Replay。原默认 maxPackedStrings=100000，实际字典条数未报告；
不修改预算或 Gameplay/Replay。每种余下 2 进程按停止规则未运行，不补样。原始日志/退出码见
capacity-low-0-B.*、capacity-high-0-B.*、run-result.json。这是未通过的生产规模准入项，
需要独立归因 fixture、完整 Gameplay 字典增长与既有容量合同，不用读取优化豁免。

额外预声明一个独立、一次性原 RPA-F3 Packed codec 核查：packed-capacity-plan.json，
SHA256 `9aeb17c3a666a23364720b4e90d089bf772519ba433b24bf47af103714852886`。它运行原 cuexis_chart_capacity_probe
的 low/high 40k 与 ladder profile，一次进程，无替换；与失败的 Gameplay-host 内容明确分开，
不能用它的通过取代 Snapshot/Seek/Replay。先提交本段，再执行此单元。

F1 源码字节已按原冻结清单复制到 f1-source，指纹完全相等；后续候选单独构建，F1 构建保持不动。


原 Packed 独立单元 native 退出 0，三 profile 均 status=ok 且重编码精确相等；low/high
均 40,000 entities/requirements，分别 1,232,408 / 1,247,446 bytes，字符串 5/13。
原始 packed-capacity.log 和 packed-capacity-result.json 保留。采集后的展示语句误用 profile
而非 id，导致外层 Python KeyError/退出 1；结果已在异常前落盘，后续只读解析确认 native=0，
没有重跑或替换。该工具是 codec 规模证据；完整 Gameplay 编码拥有额外字段与字典增长，
两者不混称同一内容，也不据此放宽 maxPackedStrings 或宣称 40k Gameplay 可用。

## 第 4 批：C1 有界候选预声明

只尝试一个产品候选，不进行无界参数搜索。F1 读斜率 57.52 对 A 41.09 ns/对象，
且实际 readObject 反汇编仍有逐对象的 component storage 查找。C1 在每次已验证只读窗口
建立时查询 const WorldTransform/Appearance/Renderable storage，后续 contains/get 只复用
这三个指针；不缓存对象值、不更改中性接口、唯一物化职责或资源匹配。

容量固定 3 指针（平台指针大小计费），只活在该窗口，随同 owner/serial/active 的现有窗口
建立与结束刷新/清空；没有跨更新、内容、布局或注册复用。World 只读锁和重入 guard 下，
组件存储不可被 mutation 替换。空池、缺组件仍按原 presence/default 返回，越界与所有
错误次序不变；owner/thread、active/serial、旧状态/布局、逃逸与跨 Close 保活规则保持。
维护成本是 Window 的三个字段与统一入口/结束的赋值，不增加 owner、线程、全局缓存或回收协议。
风险为误用旧池或缺组件时非法 get；新增测试覆盖窗口间组件删除/重建与新窗口读值，
既有跨线程/逃逸/重入/零分配等继续执行。回退点为 5cf4e2b（仅撤回 C1 产品 diff）。

受控运行使用原 A、保留 F1、新 C1 三侧：每侧 3 进程，顺序 ABC/CBA/ABC；
与第 3 批相同内容、序列、counts、3 块、64 预热、30 秒机器热身、mask=4。
运行前冻结所有身份与脚本；每次只一个计时进程，构建/测试与计时分开。
同时报告 F1/A、C1/F1、C1/A 的独立进程配对区间，不从不同轮比值乘出正式 CI。
本组只针对 read/updated，不把先前 host 或失败 40k 结果冒充 C1 结果。

预声明保留条件：C1/F1 的 read64 CI 上界 <1，所有 read 的进程波动 ≤15%，
read1 CI 上界 ≤1.10，updated 不出现 CI 下界 >1.10，输出精确相等且适用语义/分配/架构
验证通过。否则记录并放弃此候选；不追加进程。原 A 的 1.10 门禁仍独立报告，
候选内部改进不等于通过原门禁。最终保留代码须完整 Release 验证。


### 候选前验证与方法 v2

C1 新 Release 全量构建退出 0。直接运行 runtime/playback/allocation/world 四套分别退出 0，
用例数 50/171/9/24（断言 11298/27741/342/422）；含组件删除/重建、跨线程、过期/逃逸、
重入和暖态分配约束。架构检查通过，改动 C++ 格式通过。初次 CTest 按可执行文件名过滤仅
命中架构项，所以另用四个实际二进制验证，未把 1 项 CTest 冒充四套语义检查。
完整 Release CTest 在独立计时前运行，最终状态在第 5 批记录。

方法补强：v1 只查 run_id，仍可被同一 native 进程改名记录绕过。v2 新增 native PID/QPC 起点
及 raw log SHA256 的独立性拒绝，原 v1 的 15 个工具文件已逐一匹配旧 plan 哈希并另存 f1-tools；
historical-tool-snapshot.json 保存原路径→归档路径及 SHA。历史方法审计与当前源码/工具身份分别处理。
原 v1 日志、JSON、失败与结论不改写，v2 新输出另存，不宣称旧输入已按 v2 注册。

新增 host-contract-v2.json 是旧原始 host 日志的只读 addendum，记录 source/binary、内容与序列
SHA、2 Snapshot 对象、1 camera、3 声明 registry entities、U=F=1056/R=0、起止点及输出摘要。
相机/registry 数由冻结 Chart 与 instantiator 推导，U/F/R 由冻结循环推导，非独立运行计数器；U/F/R 仅计被测 host 会话循环，排除 reference oracle、load/close；
native 仍逐帧验证 Snapshot 与 reference，所有 1024 操作均保留。v2 比值/区间与 v1 相同，
仍因噪声不可判定；ACK/present-return 未合并。新增重复 native / raw log 与双方缺字段拒绝测试，
本次 Python 50 项通过（method-v2-tests.log）。候选采样将使用新 v2 工具身份。


完整 Release CTest 已完成（c1-release-ctest.log，退出 0）：1273 项，0 失败，1 项 Windows
symlink 权限相关测试跳过，793.73 秒。其余本机适用项包括安装/外部消费者、包边界、版本及架构。
此结果不代表 Linux/MinGW/shared/真实设备矩阵已执行。两条独立审阅因对话中断未收到最终报告，
不将其计作独立复审通过；后续只按实际收到的报告记录。


### C1 采样前冻结

候选代码提交 e89b472，方法补强提交 3bc842f。candidate/plan.json SHA256：
`9217caab2e0b0f5aeb50f3277ed4559fd93efeeaeaa0268e58f779a426ea42bc`。
A=适配 pre-PD，B=保留 F1，C=C1。v2 方法、ABC/CBA/ABC 九进程、计数和保留条件不变；
全部构建/测试已停止，随后一次只运行一个计时进程。

| 侧 | source SHA256 | baseline probe SHA256 |
| --- | --- | --- |
| A | `e3a79bec89fac999a0e0ff503851e68fd265aab9fe7476d31139b7aba66dd109` | `cb3b133a649ec48c85b1557cf41a832babfce611661cd334eb96fa2bbe02665f` |
| B | `1bfebe4b6e49dcc291136d3d2dec13e64e296341493e3e3d28023afcdecd6da3` | `ef1efdb280135cca40e573dd945706787cc8dc8191b0afb277b7e23c0b84ff6b` |
| C | `a875f9991a2febab950431a3fe61e04e35bf7123cce0db9ce9dc7863efa56f55` | `57e7b3657cf4e968876f28194b632084724de85710badda10eb8c06e615b1c05` |

### C1 结果与放弃决定

固定九个原生进程全部退出 0，24 个配置/块行每进程齐全，输出精确相等，无替换或补样。
candidate/analysis.json 保留全部三种配对；下表是 C1/F1 的进程配对比值及双侧 95% CI。

| Snapshot 对象数 | read | updated（真实 update→ready） |
| --- | --- | --- |
| 1 | 1.030349 [0.964305,1.100916] | 0.949199 [0.917343,0.982161] |
| 8 | 0.969054 [0.940496,0.998479] | 0.990467 [0.941406,1.042084] |
| 64 | 1.007907 [0.910329,1.115944] | 0.945950 [0.876336,1.021094] |
| 256 | 0.977149 [0.932869,1.023532] | 0.955447 [0.887997,1.028020] |

read64 上界没有小于 1，read1 上界 1.100916 超过 1.10，均未满足事前保留条件；
C1/F1 两侧 read 波动虽均不超过 6.58%，同组 A 的 read64 波动达到 18.22%，
故包含 A 的该项比较也不可判定。即使将“所有 read 波动”仅解释为候选对照两侧，前两项仍失败。
updated 未出现 CI 下界 >1.10，不能抵消 read 保留条件的失败。
C1/A read 的 1/8/256 对象区间分别为 [1.214684,1.378799]、[1.246387,1.630162]、
[1.279693,1.591775]；64 对象含 A 噪声超限。原门禁没有通过。

结论：放弃唯一 C1，不继续搜索，不追加进程。撤回 e89b472 中两个产品/语义测试文件到
精确 F1 字节，保留其他测量工具和 harness。本轮没有保留新的产品热路径优化。
candidate/retention-decision.json 保存逐条件决定；candidate/c1-product.patch、c1-source、
c1-tools 与 candidate-tool-snapshot.json 封存被放弃的代码、工具字节和身份。
既有完整 Release CTest 是 C1 状态的验证；最终 F1 状态另验证，不能将 C1 二进制称为最终产品。
有限失败不证明架构下限，也不构成安全检查或 1.10/15% 的偏离接受。
