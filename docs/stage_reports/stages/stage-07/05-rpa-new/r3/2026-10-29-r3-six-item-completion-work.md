# R3 六项优化实施与验证账本

状态：进行中；候选尚未全部验证，不是 R3/R4 退出

日期：2026-10-29

返回 [RPA 报告索引](../README.md)。当前阶段状态只见
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md)。owner 要求连续完成桌面接手文档 P1–P6，
不再按单个局部候选结束工作；原预算、错误语义、身份字节和历史证据保持原要求。

## 当前工作与完成条件

| 项目 | 本批实施/调查 | 尚需完成的核验 |
| --- | --- | --- |
| P1 Packed / re-prepare | 名称索引与随后独立 Packed 查找候选分别通过固定诊断保护条件；后者 40K unique 总耗时 23.58 → 7.09 s；身份投影复用另见 P2 | 多资源 Packed 剩余扫描、最终 fresh 回归与原端到端 load 门未完成 |
| P2 准备热点 | 保留名称索引、identity 投影复用及多资源查找/claim 分组；timer/ledger 及 scalar phase 写入均通过固定对照 | 最终完整矩阵及原 load 门未完成 |
| P3 update / Snapshot | 保留四属性合并查找、touched 和 OpenGL 诊断读回分离；resolver hint 及组件池借用候选均因对照不达预期撤回 | 最终源码完整矩阵；extract 仍有成本，不重做 R2 表示 |
| P4 编码与真实内存 | 第二次使用 admission 和测量预遍历计划均无实际峰值收益，已放弃，恢复旧独立字节缓存；已取得至 40K 的实际峰值及最终释放 | 本地轨迹不替代原 caps 或历史 R4 身份参考；编码输出本身占用及同时持有范围必须明示 |
| P5 端到端 Seek | 内置 Host 可转发既有 checkpoint span；CPU 60 组合与 1080p GPU 96 组合核对独立 origin、目标 tick/context/receipt/像素；已发现并减轻首条记录内部成本 | 40K partial Seek 仍超 20 ms；尾延迟及重复性、观察路径与故障覆盖未完；worker 替换并非当前 Host 能力 |
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

`identity-candidate-v1` 独立 fresh MSVC Release STATIC 编译 165 个命令并通过 Judgement
234 cases / 56,103 assertions、Packed 51 / 5,641、Host 62 / 49,645。新增实际 ruleset 与
grade table 的 runtime 身份对照及 session 销毁后 owning 字节检查；独立审阅未发现字段顺序、
借用生命周期、溢出预留或实际声明使用的缺陷。该候选没有额外安装 API 或新持久缓存。
相关构建不是所有工具链/共享包完整矩阵。测量 runner 首次生成有 Python 语法错误，尚未启动
任何测量进程；失败脚本保留，修正后才创建有效登记。

`identity-measure-v1` 预登记每规模/输入两对 AB/BA，共 24 个普通 allocator 新进程；完整
Packed / Chart / Content / Prepared raw 字节一致。uniform 1K/10K/40K 总和中位数下降
3.79%/8.50%/15.79%，unique 下降 7.04%/5.34%/5.93%，符合预设大规模改善与小规模 5% guard。
但 40K uniform 原 baseline relative range 为 21.43%，该组 15.79% 受慢点影响，不能用作稳定
收益或原 15% 门通过。其余组 relative range 均小于 10%。

针对这一初测异常另登记 `identity-uniform-anomaly-v1`，只跑 40K uniform 两对，共 4 个新进程；
未改原组或二进制，raw 字节仍一致。baseline 总和 6.185123 / 5.738604 s，candidate
5.379228 / 5.360227 s；中位数 5.961864 → 5.369728 s，下降约 9.93%，两边 relative range
为 7.49% / 0.35%。原更高慢点未稳定复现，按偶发/未复现保留，不删除，也不并入原组改判。
两项固定组支持保留该内部复用，正式 CI、端到端 load 和 Seek 原门仍独立开放。

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

## 普通 GPU 提交与诊断读回分离

实际源码核实 `OpenGlBackend::submit()` 原来每帧都传入 pixelProbe，触发同步 `glReadPixels`。
当前以现有私有 pimpl 的默认关闭开关控制该读回，owner-only 私有桥供诊断开启，没有新增 SDK
配置字段、公开方法、全局开关或 backend 对象布局。`renderPresentation()` 本身继续更新 draw 标志，
移除 submit 的冗余覆盖，避免关闭读回时把有效 `presentationDrawn` 清零。

`pixel-policy-v1` 重编两个 OpenGL TU、设备测试及 Player smoke TU，公共实现依赖优先使用
已验证的 fresh identity 构建，其他冻结适配器只读复用。四个固定进程的默认关闭/显式开启及
前后橙色像素核验通过；Player 实体烟雾测试完成 6 帧，resize/故障/重载相关颜色 golden 通过。
`pixel-seek-regression-v1` 的 96 组合、22,018 assertions 通过。该 overlay 不是最终 fresh 全矩阵。

`pixel-policy-v2` 的吞吐探针显式区分计时帧是否读回，两份实际像素核验放在稳态计时外。
复用对象的 source/object 哈希和父 binding 另有 `reuse-receipt.json`，不改原绑定。
`readback-throughput-v1` 预登记 1K/40K render-only，各两对 AB/BA，共 8 个新进程、512 个
计时帧；1080p、真实入镜与绘制数量、普通 allocator、GPU 时间戳、VSync OFF 均核验。
关闭/开启读回的完整 CPU cycle 中位数在 1K 为 1.631925 / 1.855300 ms，下降 12.04%；
40K 为 54.781325 / 55.558800 ms，下降 1.40%，通过预设 5% guard。两份额外 proof 帧不计入
512 个稳态样本；没有沿用旧读回策略的容量结论或声称 40K 达到 240 FPS。

1K GPU timestamp 区间关闭/开启约 0.023552 / 0.179456 ms，40K 约 15.421696 / 15.633152 ms；
开启读回会影响 end query 发出时机，不能把该差值全部解释为 GPU 着色耗时下降。
四个小探针的第二次 proof 提交仍有 183.602–195.782 ms 冷等待，保留为待归因图形启动问题，
不声称 CPU 不再等待就证明 GPU 提前完成。基于可验证收益与更清晰的诊断职责，保留读回分离。

40K 仍有逐对象命令构造及提交成本。后续若探索批量绘制，应先独立量化其占比，保留 opaque /
transparent 顺序、通用 shader 和资源语义，不因简单场景收益扩张 SDK 边界。
OpenGL 3.3 的实例化调用机制见 [Khronos 原始规范 §2.8](https://registry.khronos.org/OpenGL/specs/gl/glspec33.core.pdf)，
此处仅登记探索方向，当前没有实例化实现或相应性能接受。

## 多资源准备的平方扫描

2026-10-30 后续 Packed 候选 `packed-exists-v1` 只替换 validate 中 resource / domain / solver
声明的三处 typed-existence 查询。表在同一调用中已先通过严格顺序检查，单行/空表继续原扫描，
多行二分；不增加索引或改变声明遍历、首错选择及写入顺序。resource / solver 使用与既有表排序
一致的字符串序，domain 保持 textLess。两版 Packed 各 51 cases / 5,641 assertions 通过。

`packed-exists-measure-v1` 预登记 10 个普通进程：64 split canary 一对，1K shared 与
10K split 各两对 AB/BA。完整 Packed 和 Chart / Content / Prepared 三身份 raw 字节逐块一致。
10K split 的 encode + decode 中位数 3.183242 → 2.608776 s，下降 18.05%；1K shared
下降 2.83%，source prepare 两场景均未触发 5% guard，各相对范围低于 4%。满足预设 10K
codec 至少改善 10% 的局部保留条件，已将该独立源码副本提升为产品改动。未解决 makeModel
的 claim 资源 findIndex 或 reconstruct 的 definitionSource 扫描，也不替代正式 CI / load 门。

R3c 前置普查 `tick-census-measure-v1` 仅运行 1K/40K H902 各一个观察进程、每进程三请求。
两者均在 Tick 8 分别产生 1,000 / 40,000 个本 Tick FactDescriptor，idleFoldReady=false、pending=0、
historyAvailable/nativeAnchorEligible=true；完整 Snapshot 原点对照通过。该副本尚未加入 cache，
只证明拟议事实门槛可满足，不证明完整采集资格、命中或恢复收益，观察时间不作普通时间接受。
R3c v2 草案和独立调用点审阅还确认 decodeReplay 是另一重放入口，不能遗漏；其 source writer
与验证 replay 必须显式区分，失败 record 发布不能自动晋升候选，真实故障路径不能被
testControls 的 cache 禁用条件掩盖。闭包、容量/内存和故障覆盖尚未关闭，产品续点仍未实施。

`resource-diagnostic-v1` 只在独立源码副本插入资源阶段时钟，产品实现未加探针。
`resource-phase-attribution-v1` 固定三个一次性诊断：64 个独立资源的完整 encode/decode canary
通过；10K requirements 共用一个资源时，资源阶段约 31–34 ms，分为 10K 资源后约
956–1,013 ms，source prepare 总计约 1.657 s。观测时间只用于归因，不当作普通性能接受。

资源候选用已有 canonical 顺序替代 `findResource` 全扫描，并仅在多资源输入时一次性分组
claims，沿原顺序继续校验和解算。新增三资源（含未使用资源）、competition 顺序及输入置换
回归，新旧两版各 235 cases / 56,126 assertions 通过。独立审阅确认排序前提、相同键首项、
分组遍历/错误顺序及 Result 传播；最终版在 finalization 前清空借用索引，明确生命周期。

`resource-measure-v1` 的 2 个小 canary + 16 个固定普通 allocator 进程全部有效，原始 Packed /
三种身份字节一致。10K 独立资源的 source prepare 中位数 1.703996 → 0.318486 s，下降 81.31%；
source prepare + Packed encode + decode 总和 6.740689 → 3.731611 s，下降 44.64%。
1K 独立资源总和下降 11.51%；1K/40K 共用单资源总和下降 4.37%/1.33%，通过 5% guard。
该组 relative range 均小于 10%，没有以它替换原组或正式窗口。

`resource-candidate-v2` 提前结束借用索引生命周期后，两版完整 Judgement 回归再次通过；
`resource-final-measure-v1` 只对变化相关的 10K 独立资源另登记两对 AB/BA，完整字节仍一致。
最终 source prepare 中位数 1.687775 → 0.319911 s，下降约 81.05%；总和
6.446277 → 3.697551 s，下降约 42.64%。因此保留这两处扫描消除，不扩展为持久资源缓存。

## 首条长记录内部 Seek 的遗漏与局部修复

此前 CPU 矩阵的 partial 目标在首条 H=1103 记录之后，不能覆盖该记录内部的成本。
新增 H=5（活动状态）及 H=902（已产生 miss）代表点；正常 Host 流程不增加任何 advance。
首个 `partial-seek-measure-v1` 在暂停前错误地回调到旧时刻，触发正常倒退时钟拒绝，尚未进入
Seek，保留失败组。v2 修正为按当前时刻暂停后再发起回溯，另行登记 10 个普通 allocator 进程，
每场景两个进程、每进程首次及两次重复请求；计时从 replayCut 前至 CPU test renderer present 返回。

129 条记录、owning source、Packed rev4 下，40K H=5 的 request-to-present 为 53.520–59.192 ms，
H=902 为 573.133–647.635 ms；final H=1231 为 2.113–2.972 ms。source setup、archive 与原点
oracle 在计时外，不能将 setup 约 6.28–6.50 s 混入 Seek，亦不能把 final 快点泛化为任意目标。
public constructor 重建的 unsealed archive 不带 native 索引，作为原点 oracle；每个请求都比较
完整 `sameSnapshot`。全部有效。独立只读审阅核对绑定、进程、clock、cut 和口径，未发现使该结论
失效的问题。该组仍不是 GPU 或正式性能接受。

`partial-seek-phase-measure-v1` 只取 40K H=5/902 各一个观察进程、三次请求。探针插在源码副本中，
使用独立类型避免跨 TU 混用 inline 观察器定义，不进入产品。native prepare 约 9–11 us；
H=5 的 advance 约 57–64 ms，其中 80K timer 调用约 55–62 ms。H=902 的 advance 约
593–638 ms，其中 160K timer 约 163–170 ms、ledger 约 399–441 ms，Fold 约 0.7 ms。
这些含时钟开销的分段只用于归因，不能替代普通时间。

修复不新增恢复状态：timer 按需构造 owning OriginId，保留 activation/windowOpen 后的 epsilon
逻辑；hardDeadline 无 Pending phase 时仍执行 endOwnership。有 Pending 的分支继续 failPending，
增加一次小型 phase 扫描。ledger 的排序比较改用局部 rank 与引用 tuple，避免每次比较复制
OriginId / RequirementIdentity，排序键及事实拥有型身份不变。独立审阅确认同步消费与比较等价性。

`timer-ledger-candidate-v1` 的 Judgement 235 cases / 56,126 assertions、两版 Host 各 62 cases
通过；Host 分别为 54,531 / 54,612 assertions，含完整 CPU 60 组合。不同断言数保留，
不据此声称执行轨迹完全相同；分配失败枚举等测试会随被消除的分配机会变化。
`timer-ledger-measure-v1` 固定 18 个普通进程：四个 partial 场景各两对 AB/BA，40K final 一对 guard；
每个进程三次请求。每进程先取三次请求中位数，再汇总进程中位数，40K H=5 为
57.3115 → 21.9735 ms（-61.66%），H=902 为 607.931 → 270.5975 ms（-55.49%）；
1K 两点分别 -37.40% / -55.27%，40K final 为 2.441 → 2.424 ms，通过预设 guard。

首次请求单独保留：候选 40K H=5 为 18.414 / 18.710 ms，重复为 21.935–22.833 ms；
H=902 首次 230.347 / 238.367 ms，重复 264.213–299.831 ms。不能选择首次快点宣布 20 ms 通过。
完整 snapshot wire 的大小和 SHA-256 全部一致，partial raw 完整字节逐块一致；wire 编码在三次
计时后执行。final 129 记录 wire 达 3,532,762,976 bytes，不是 40K 内存接受。
因此保留局部修复，剩余长 record 成本及原门继续开放；R3c 尚未实施。

## 标量 phase 写入

`SharedSequence::update()` 为保障任意回调可捕获旧 owner、抛异常而不泄漏修改，先复制完整行，
再分离目标页。phase 只改一个枚举，因此增加仅接受同类型行的内建标量成员指针的
`assignScalar()`，分离后直接执行内建赋值，不运行回调。原 update 合同保持；有效非空成员
指针是前提，产品唯一调用传固定 `&PhaseProjection::state`。独立审阅确认旧 owner、encoder
storageOwner 和失败分离安全；SharedSequence<int> 仍可实例化，不提供通用 callable 检测合同。
新增回归覆盖共享/独占页、encoder owner 保活、复制失败、越界与后续分支隔离。

`scalar-candidate-v1` 全相关依赖 fresh 编译 165 个命令约 716.9 s，避免混合新旧模板定义；
Judgement 236 cases / 56,139 assertions、Packed 51 / 5,641、Host 62 / 54,683 全通过。
首个探针链接遗漏 Host 实现对象而失败，尚未测量；原日志保留。`scalar-candidate-ready-v1`
仅补齐已 fresh 编译的 Host 对象重新链接，不重跑构建/单测，绑定父 manifest、对象、库与二进制。

`scalar-measure-v1` 固定 14 个普通 allocator 新进程、42 次请求；1K H=5、40K H=5/902 各
两对 AB/BA，40K final 一对 guard。mask 4、BelowNormal、QoS OFF 逐进程核对，AC line 均为 1。
每进程三次请求的中位数再汇总，40K H=5 为 22.873 → 16.678 ms（-27.08%），
H=902 为 265.5015 → 232.6405 ms（-12.38%）；1K H=5 下降 33.75%，final 未触发保护条件。
候选 H=5 首次 13.108 / 14.314 ms，重复 16.577–17.798 ms；H=902 首次
215.819 / 220.393 ms，重复 227.313–259.486 ms。当前局部 H=5 诊断样本低于 20 ms，
不能据此宣布全 Seek 通过，H=902 仍明显超界。final guard 只有一对且不执行此 phase 写入，
其点值下降不归因为该方法的直接收益。

四场景完整 snapshot wire 大小/SHA-256 一致，partial raw 全字节一致；wire 仍在全部计时后
编码。通过预设 40K H=5 至少改善 15% 及其他场景 guard，保留该内部原语。全仓 647 文件
format dry-run 通过；该 fresh 相关构建尚不是最终 SDK、RPA 观察通道或工具链全矩阵。

R3c 仅形成 sealed Tick 条件设计草案，尚未实施；设计审阅已发现 writer-only capture、记录 ordinal
绑定、nativeAnchorEligible / history / ingress 闭包，以及故障注入覆盖等必须先解决的门。
不能把普通最终 Snapshot 改名当作续点，也不能为通过本地目标而跳过这些条件。

## Snapshot 组件池借用候选撤回

`pool-candidate-v1` 首次探针因未使用的 SDL include 缺少搜索路径而编译失败，未进入测量；
v2 移除无关 include，沿相同普通 Release 依赖只覆盖 presentation read TU。
新增组件池整体移除、缺失读取及重新创建后的字段读取回归，两版 Runtime 各
54 cases / 11,614 assertions、候选 Playback 184 / 34,747 通过。

`pool-measure-v1` 预登记 10 个普通 allocator 进程、每进程 20 帧预热和 100 帧测量：
1K/40K static 各两对 AB/BA，40K animated 一对 guard。最终 FrameDigest 全部一致。
40K static extraction 中位数 7.113125 → 7.143575 ms，反而增加约 0.43%；
1K 下降约 2.30%，40K animated 增加约 0.28%。没有达到预设 40K extraction 至少改善 10%
的保留判据，因此撤回产品代码，不为该结果追加复测。保留源码、二进制、登记与全部样本；
保留独立的存储移除/重建生命周期回归，作为既有只读窗口合同检查，不引入新的组件池缓存。

## 构建、版本与原门限

2026-10-30 恢复批次：`release-mingw-metrics-v4` 完成独立 MinGW GCC 16.1.0 配置，
metrics ON、Release STATIC、warnings-as-errors。原 v3 的 owner 暂停记录保留，不恢复或覆盖旧组。
v4 在实际项目编译中发现四处 DeliveryAdmission 省略 attempt 初始化，被 missing-field-initializers
拒绝。修复仅显式补上原本即为空的第四项；`mingw-metrics-resume-v1` 又发现 playbackOnly
只在未插桩正式测量中调用，metrics ON 却编译其未使用定义。其定义现使用与调用方相同的条件，
不放宽警告、不执行正式测量。原始源码、失败日志及两次显式增量恢复绑定全部保留。

`mingw-metrics-resume-v2` 的 Core 37 cases / 2,610 assertions、PD baseline 6 / 16,844、
Host 62 / 56,642、PD host 6 / 3,543 均通过，合计 111 cases。该项目级多 TU 构建和链接
未再出现旧 TLS multiple-definition 问题。显式有限 deterministic presentation canary 通过，
runtime / snapshot / judgement / projection 等阶段各 1,088 个样本；数组长度一致、非空且
omitted 为 0。它证明 metrics 实际通道和 MinGW 相关目标，不是普通性能、完整桌面包或全部
STATIC/SHARED / Debug/Release 矩阵接受。项目编译使用 --parallel 1、BelowNormal 和 affinity 3；
vcpkg 同样设置并发数 1，但其 helper 的 Debug/Release 配置探测仍可能并行，不将其描述为全程串行。

另在当前源码直接执行 VerifyArchitecture.cmake，退出 0。Packed typed-existence 候选和
R3c 逐 Tick 事实普查均先准备独立源码副本，未混入本次 MinGW 冻结构建。

项目目标构建使用串行、BelowNormal、affinity 3，由既有 Windows Job 包装器约束后代；
第三方配置 helper 的并行例外见本节恢复批次。
旧绑定构建/二进制只读，采用新的独立构建目录。Release 首次配置因 PowerShell 对未引用 suffix
拆分失败，原目录与错误保留；有效新构建是 `release-static-v2`，参数由 Python argv 传递。
当前 SDK 仍为 0.9.1，未添加公开安装字段或改变 wire / codec 版本。

`release-static-v2` 已完成 fresh Release STATIC 全量构建及 CTest：1,408 项，0 失败，
1 项物理符号链接测试跳过。恢复构建日志为 `release-resume1-build.log`，全量测试日志为
`release-resume1-ctest.log`，退出码及命令见 `release-resume1-results.json`。
首次构建因新测试错误地默认构造必须携带 PreparedIdentity 的 KernelProjection 而失败，
原日志保留；测试改为使用真实 assembler 产生的身份后再构建、运行全套，不删除该反例。
该结果覆盖当时 b2 候选的常规功能回归，不覆盖此后保留或撤回的所有变更；
不能替代最终源码矩阵、性能测量、观察通道路径证明或其他工具链矩阵。
后续独立 Debug 构建目录为 `debug-static-v1`。

`debug-static-v1` 全量编译成功；CTest 在预先设置的性能分阶段等待处被对话中断，尚未执行，
保留 `debug-performance-stage-entered.json` 和未完成日志。最终源码改变后重新执行独立矩阵，
不能把这个候选编译结果记成最终 Debug 全套通过。

原适用 Seek ≤20 ms、request/ready/GPU p99≤4.167 ms、初次 load≤2000 ms、15% 重复性和正式
配对置信区间要求均未放宽。未完成、失败、不可比与缺外部证据的项必须分别记录，不追加到通过。
