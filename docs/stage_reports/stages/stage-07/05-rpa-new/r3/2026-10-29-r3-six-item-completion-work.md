# R3 六项优化实施与验证账本

状态：active；owner 已要求恢复并完成剩余任务，候选尚未全部验证，不是 R3/R4 退出

日期：2026-10-29

更新日期：2026-11-01

返回 [RPA 报告索引](../README.md)。当前阶段状态只见
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md)。owner 要求连续完成桌面接手文档 P1–P6，
按六项剩余工作持续推进；原预算、错误语义、身份字节和历史证据保持原要求。
最新 P1 否决、R3c v4 及预算/真实成本证据见[恢复实施记录](2026-10-30-r3-resume-and-budget-verification.md)。
新[Seek 发布归因与批量候选处置](2026-10-30-r3-publication-attribution.md)区分 H902 发布与 H5 activation 成本；批量候选未过保护门，不选用。
后续[activation与活跃索引共享](2026-10-30-r3-activation-and-active-sequence.md)候选数值门改善，完整重复性仍未过；已新增真实多批事务反例及实际内存/分配失败证据。
新[窗口启动响应与渲染归因](2026-10-31-r3-startup-and-render-attribution.md)修复测试探针建窗后同步准备导致的长时间无消息泵，支持关闭取消并记录启动阶段。
独立产品分页无明显收益且Seek重复性失败，未选用。JSON回调宽数组平方扫描已定位，SAX/legacy parse-once组合的40K prepare约17.555→2.028秒，身份一致但整组重复性失败，未纳入产品。
实例化未达预设收益门，带探针归因显示剩余主要在命令/排序/复制/digest。早期GL组的DLL身份缺口与后续精确Release运行库绑定均留档，不回填旧组接受。
随后[中立命令转移及已有序检测](2026-10-31-r3-neutral-command-transfer.md)通过16进程普通GPU局部组与逆序保护，40K render-only/static cycle改善10.33%/12.05%，已选用。
最终格式化CPP显式重编的中立3/399与OpenGL22/140回归通过，digest/order/拥有语义保持；完整矩阵及240FPS仍开放。
新[中立路径 CPU 杠杆](2026-11-01-r3-neutral-path-cpu-lever.md)在 1800/2600 render-only 上把提交路径中位数
压低 33.98%/36.16%、完整 cycle 压低 29.07%/34.63%，七个预注册门全过、重复性在 15% 内；40K 诊断同向，
候选臂通过 2600 不构成新前沿点。
恢复前 P1/R3c v2 现场及未选用源码归档见
[2026-10-30 暂停与候选交接](2026-10-30-r3-pause-and-candidate-handoff.md)。下文各批记录保留当时范围，
不将某个旧版本的“尚未实施”或通过结果误作最新预算副本的结论。

## 当前工作与完成条件

| 项目 | 本批实施/调查 | 尚需完成的核验 |
| --- | --- | --- |
| P1 Packed / re-prepare | 名称索引、Packed 查找及 typed-existence 二分已保留；claim/definitionSource v2 最终固定组收益/保护门未过，已否决并保留证据 | 剩余热点按实测价值取舍；最终组合回归与原端到端 load 门 |
| P2 准备热点 | 保留名称索引、identity 投影复用及多资源查找/claim 分组；timer/ledger 及 scalar phase 写入均通过固定对照 | 原 load 门仍未作为门记录（40K 压力读数 2 148 938 µs，超门 148.9 ms；owner 已裁定 40K 属极端情形）；最终源码完整矩阵已在 `6256af8` 上达成（[第四次运行](2026-11-01-r3-acceptance-matrix-fourth-run.md)） |
| P3 update / Snapshot | 保留四属性合并查找、touched 和 OpenGL 诊断读回分离；resolver hint 及组件池借用候选均因对照不达预期撤回 | 最终源码完整矩阵已在 `6256af8` 上达成（[第四次运行](2026-11-01-r3-acceptance-matrix-fourth-run.md) 的候选行即该矩阵）；extract 仍有成本，不重做 R2 表示 |
| P4 编码与真实内存 | 无峰值收益的编码 admission/预遍历已撤回；已有本地 40K 峰值/释放；v4 与六代归档实际 requested/storage 已测，故障退款和最终释放通过 | 通用上界、合理预算与保留策略、完整播放链 headroom；原 caps/profile/历史 R4 参考仍缺 |
| P5 端到端 Seek | 产品 CPU 60/GPU 96 组合通过有限功能核验，产品 H902 仍慢；R3c v4 相关428 cases、真实故障/字节/释放通过，普通H902明显改善但未选用 | 归因重复性超15%；合理预算/策略及全部适用场景；决定产品取舍，原CI门仍开放 |
| P6 维护/工具/矩阵 | metrics TLS 改为函数内每线程对象，消费者同步；全仓格式债处理；旧 b2 Release 全套通过；新增 Ninja 依赖漏记已由真实头文件修改反例定位并修复验证；[2026-11-01 同源矩阵](2026-11-01-r3-same-source-acceptance-matrix.md) 已在 `466ff76` 的同一份源码上跑完全部 11 行：7 行 local_pass、1 行 CTest 失败（`#904`，同一二进制隔离重跑 108 次未复现）、3 行 MinGW 在构建阶段被 GCC 报出的测试源码可移植性缺陷挡住（五处测试源码修复已提交为 `3fbc31f`）；第二次运行在 `3fbc31f` 上 10 行里 8 行通过，`mingw-on-debug` 的 51 个失败由探针的 replacement 分配器造成（归因为仪器、不改产品），`mingw-on-shared-debug` 撞上磁盘写满，第 11 行未启动；补上探针加固与运行器磁盘下限（`8963889`）后，[第三次运行](2026-11-01-r3-acceptance-matrix-third-run.md) 在同一份源码上 11/11 行 `local_pass`、13,613 用例 0 失败 | 构建修复已完成审阅及独立/嵌入定向门；同源完整矩阵已在本地达成（11/11），仍缺 hosted 同 SHA 验证与 owner 接受；[第四次运行](2026-11-01-r3-acceptance-matrix-fourth-run.md) 在 `6256af8`（中立路径杠杆之后）上再次 11/11 `local_pass`、13,635 用例 0 失败、120 次测量事件，10 棵构建树从空目录重建；因此 P6 只记本地完成，不升级 R3 退出或任何阶段结论 |

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

## 隔离 sealed Tick 原型与故障反例

R3c 仍未提升为产品实现。`tick-census-measure-v1` 仅观察 1K/40K 各一进程、每进程三请求，
确认 Tick 8 产生 1K/40K 个事实，pending 为空、idleFoldReady 为 false，候选位置实际可达；
其观察时间不作为性能成绩。`tick-prototype-v1` 在 tracked 源码独立副本中实现 writer-only 收集，
每次 advance 本地暂存，成功 finalization 且原记录实际发布后才提升。Replay、Seek 和导入验证
显式禁用收集；私有状态不进入 SDK 安装头或 recovery wire。

点位拥有已经完成 kernel、Fold 和 fault reserve 的不可变 RuntimeState，绑定该记录开始前的
prefix、recording 和 ordinal；不伪造 advance，不把中间状态当最终 Snapshot。单个索引暂限四点，
超过后保留最早点和最近三点。这是实验结构上限，尚非已接受的保留字节或长期 retired-owner 预算。

`build-v3` 的有限正向测试为 2 cases / 525 assertions；普通 RPA 回归为 72 / 27,029。
包含前缀 ordinal 0/1/2/3、重复 advance、Pause/Resume、输入批次、source writer 销毁后恢复，
以及超过四点的轨迹；当前 Snapshot 全成员及编码字节、后续推进与独立外部原点一致。
独立审阅指出还应比较后续分支完整 Replay 输出，正在补充，不把现有检查写成全部闭包完成。
随后 `writer-build-v2` 的同两用例扩为 625 assertions，增加后续分支完整 Replay history 与
encodeReplay 字节比较，全部通过；普通 writer 开销另以 stdin barrier 后的独立进程观察。
该探针首次编译因直接 getenv 触发 MSVC Werror 而停止，失败目录保留；新副本使用仓库现有
environmentVariable，不放宽警告，也不把测试方法错误解释为产品故障。
初版 test include 跨物理目录导致重复定义的构建失败保留；修复仅统一 Core 内部头路径。
`build-v3/reuse-receipt.json` 明确四个生产对象实际复用 build-v1；原 binding 的“全部 fresh”措辞
不准确，补充回执记录真实对象和格式化后的源码 hash，不追改原回执。

`fault-build-v1` 是另一个观察副本：仅 Kernel CPP 插入不分配的阶段事件，测试 TU 使用静态
global allocation replacement，普通计时二进制不含这些探针。3 cases / 196 assertions 通过：
在实际暂存一个点后，分别让 finalization、下一 Tick Fold copy 和 kernel seal 前 reserve 分配失败，
确认故障记录仍发布、原错误状态与独立 controls oracle 完整一致，但暂存点均不提升。
可选点分配及恢复 state/reserve 分配失败均走原回退，完整 Snapshot 字节一致；实际命中事件
还证明四槽淘汰后选择 frontier 901、1501、1701、1901，而不只是检查“某点命中”。

`fault-negative-v1` 故意改成失败 advance 也提升暂存点，同一故障测试以 exit 42 拒绝，
99 assertions 中 6 个失败，实际发现提升计数及 tickPointCount 为 1；该反例源码与日志保留，
永不进入产品。四槽提升不分配内存，因此没有虚构“提升分配失败”的测试位置。

同一观察构建的 1 case / 30 assertions 取得 requested heap：1K/40K 无缓存峰值分别为
11,741,098 / 463,568,453 B，候选峰值相同；writer 销毁但 archive 仍持有时，候选比基线多保留
11,279 / 323,279 B。两版最后 owner 释放后的差值均为 0。此为 typed fixture 单条长记录，
不含 CRT usable、RSS、GPU 或通用 caps；旧 owning archive 可延长被淘汰点存活，四槽并不证明
整个进程总保留量有界，多点、较长输入/控制历史及 retired owner 成本继续开放。

`tick-screen-v1` 先做 1K H=902、两对 AB/BA、三请求的普通 allocator 探索筛选，
进程内三请求中位数再汇总为 3.5505 → 0.2520 ms；首次为基线 3.158/3.133 ms、
候选 0.481/0.438 ms，完整目标 Snapshot 字节一致。预设仅以至少 30% 改善判断是否值得继续
故障/内存验证，不是保留决定、40K 结论或 20 ms 正式接受。生成开销、保留预算和其余原门仍开放。

随后 `tick-writer-measure-v1` 固定 12 个普通 allocator 进程，每进程三个新 writer，分别测
1K dense、40K dense、1536 分六波。只计 advance，prepare、archive、Snapshot 编码在计时外。
baseline 使用同一私有实现的 advanceForReplay 禁用收集，candidate 使用 writer advance；
mask 4、BelowNormal、QoS OFF 逐进程核对。40K dense 中位数 623.478 → 597.606 ms，
1K 10.835 → 10.736 ms，六波 14.656 → 14.865 ms；最大进程中位数相对波动 10.27%，
未触发预设 40K +5%、其余 +10% 保护条件。完整最终 Snapshot 大小和 SHA 一致。
此比较用于检查是否转移生成成本，不将小幅点值下降归因成 cache 的直接性能收益。

`tick-screen-40k-v1` 另取固定四个普通 Host 进程、两对 AB/BA、每进程三请求，只探索 40K
第一记录内 H=902。进程中位数再汇总为 237.105 → 2.7385 ms（-98.845%），
首次为 baseline 227.763/209.507 ms、candidate 2.899/2.952 ms；候选重复 2.401–3.056 ms。
完整目标 Snapshot 为 92,623,328 B，SHA 及 raw 全字节一致。两候选进程中位数为 2525/2952 us，
按原 `(max-min)/median` 计算相对范围为 **15.5924776%**，超过 15%。此前版本写的 16.91%
采用 `(max/min-1)` 的另一分母，本次更正统计表述，保留全部原样本和登记；这也不是正式注册窗的接受结果。
本组预设仅要求至少 50% 改善以决定是否值得继续字节预算设计，
不能将 screen_pass 写成原重复性门通过。setup 包含夹具、Packed 和 Host 准备，不是 SDK load。
仍缺跨旧 archive 生命周期的字节准入；原型不进入产品，原性能接受保持开放。

上述为无预算原型当批证据。之后共享 lineage payload 预算 v2 已在隔离副本完成 fresh 相关功能验证，
但其性能/实际故障/内存未测；最新版本差异、426 cases 和 P1 待确认 v2 详见
[暂停现场报告](2026-10-30-r3-pause-and-candidate-handoff.md)。两项实验材料已版本化留档，不表示正式选用或合入。

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

`release-rpa-v1` 随后按 ff93484 的产品源码独立 fresh 构建，RPA observation ON、PD metrics OFF。
Host 69 cases / 55,371 assertions、Judgement 236 / 56,139、RPA 71 / 26,545 全通过；
显式 Seek matrix 1 case / 12,430 assertions 产生恰好 60 条矩阵记录，包含未来 checkpoint
及第一条记录内 partial 的 actualRecords 断言。该通道核验不使用其观测时间作性能接受。
该 RPA 观察构建使用产品 ff93484，不含 R3c 原型；同一时期的早期无预算原型另在
`tick-prototype-v1/source`，后续隔离版本另见暂停现场报告。
无预算版或预算版的有限功能通过均不表示 R3c 已进入产品或完成接受。

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

## 中立路径 CPU 杠杆（2026-11-01）

基于[提交路径停顿归因](2026-11-01-r3-submit-stall-wait-attribution.md)的中立路径分解（1800 上
digest 513–588 us、resolve 155–165 us 属于中性开销），本批只做两处最便宜的结构改动：不重写 Reader、
不加跨帧缓存、不改预算、不动任何原始门。

- `DrawSummary::digest` 改为纯派生值：摘要不再携带自己算出的摘要，构造时不再写字段，改由
  `presentation_renderer::summaryDigest(summary)` 派生；空清单早返回与正常返回共用同一派生函数，
  域仍是 `cuexis.validation.summary.v1`。渲染适配器只持 `const DrawCommand*`，不再拷贝整帧命令。
- 适配器每帧结束清空 `opaqueScratch` / `transparentScratch`，让 `PreparedDraw` 的非拥有指针在早返回
  和探针路径上也有明确的生命周期边界。

固定组 `neutral-lever-measure-v1` 为 1800 / 2600 两个 render-only 格、每臂每格 3 个新进程、ABBA 交错，
两臂同时开逐帧像素回读、关 GPU timer。提交路径中位数 2 239.4 → 1 478.5 us（-33.98%）与
3 213.2 → 2 051.2 us（-36.16%），完整 cycle 2 721.9 → 1 930.7 us（-29.07%）与
3 864.9 → 2 526.6 us（-34.63%），cycle p99 同向下降（-24.93% / -29.95%）。重复性
(max-min)/median 最大 8.29%（2600 基线提交路径），其余更小，全部在 15% 屏内；七个预注册门全过，
`formal_acceptance` 保持 false。40000 诊断同向（-37.68% / -37.57% / -39.06%），只说明杠杆在压力规模
仍然生效，不进门、不进阶梯。

候选二进制在同一棵 Release 树上重建，A/B 二进制与库哈希、仪器补丁、逐进程日志、被撤回的首次启动与
全部读数归档在[证据附件](2026-11-01-neutral-path-lever-evidence/index.md)。同树全量 CTest
1418/1418 通过、0 失败、1 项按仓库设置跳过、退出 0、783.54 s。

边界：本组只压中立路径成本，不改任何容量结论。候选臂在 2600 上通过不是新前沿点，括区必须用同一份源码
在完整矩阵那一批里重新推导。正式读取相对门、正式重复性、D2/D3/Snapshot/排队 p99、request→ready 与
GPU render p99、Pause/Resume/resize、Reset/Close、适用 Seek 仍未在本轮源码上测过。

## 第四次同源矩阵与 240 FPS 前缘重推（2026-11-01）

以 `6256af8`（中立路径杠杆）为源码，两件事各跑一批，互不混用。

**第四次同源矩阵**：11 行全部 `local_pass`、13,635 个用例 0 失败、120 次测量事件，墙钟 2.62 h；
11 棵构建树里有 10 棵在 2026-11-01 的清理中被删过，因此每行都从空目录 `--clean-first` 重建。用例数
比第三次运行（13,613）多 22 个，即每行多 2 个：杠杆提交在 `tests/json_support/serialize_tests.cpp`
新增三个 `TEST_CASE`，其中 `[.serialize-capture]` 是隐藏标签、不进入默认注册。`msvc-on-release` 的
构建目录里残留着上一次运行的 `LastTestsFailed.log`（一条 `cuexis_architecture_tests`），运行器在本行
CTest 之前捕获为 `-previous-` 再删除，`summary.json` 把它记入 `stale_failure_lists`，归档按排除项处理。
身份闭合的两份记录都归零：工作树 4,259 个文件逐字节相等；全新检出 3,702 字节相等 + 486 仅换行形式 +
4 原生 CRLF + 67 混合换行、0 漂移、0 文档差异。见
[第四次运行报告](2026-11-01-r3-acceptance-matrix-fourth-run.md)与
[附件索引](2026-11-01-rpa-acceptance-matrix-run4-evidence/index.md)。

**240 FPS 前缘重推**：预注册在任何测量之前冻结（十个实体数、交错顺序、判据、两个预测模型与锚点规则），
十个点各跑一次、全部有效、探针哈希全程不变、没有作废尝试。判定：仅渲染 bracketed `[3600, 3800)`，
静态 bracketed `[2600, 2800)`。仅渲染的预注册预测与实测一致；静态预测 `[3000, 3400)` 被否证（实测低
两档），这个否证本身作为结果保留。锚点规则成立（1800 仅渲染 p99 中位数 2.3452 ms 低于冻结阶梯的
2.7479 ms），但跨仪器的差值不构成效应量。见
[前缘重推报告](2026-11-01-r3-capacity-frontier-rederive.md)与
[附件索引](2026-11-01-capacity-frontier-rederive-evidence/index.md)。

边界：这一批仍然只回答"能不能跑完全部配置"和"本源码上的前缘括区在哪"。冻结的容量确认列表（1080p
仅渲染 1800、静态 1300）不变；前缘括区是诊断值，不做点间插值，也不允许把 3600 与冻结的 2400 相减
当成容量增幅。**正式读取相对门、正式重复性、D2/D3/Snapshot/排队 p99、request→ready 与 GPU render
p99、Pause/Resume/resize、Reset/Close、适用 Seek 在 `6256af8` 上仍未测**；R3 技术接受与退出仍是
owner 的决定。

## 第一批正式门测量（2026-11-01）

预注册在任何测量之前写定，四组各自独立，判据逐字取自 [R3 计划](../../../../../stage_plans/active/stage-07/05-rpa-new/plan.md)
§9.2–9.4。报告见[第一批正式门测量](2026-11-01-r3-first-gate-batch.md)，附件见
[门测量证据](2026-11-01-r3-gate-batch1-evidence/index.md)。

- **初次 load：通过（本场景）**。9 个新进程跑注册的 smoke 场景，进程创建→首张有效画面 present-return
  为 395.1–426.5 ms（中位 410.7 ms），门 2000 ms，余量 4.7×；进程内 94.4% 的时间在窗口创建。场景关闭
  音频，因此音频初始化不在本边界内。
- **正式重复性：7.63 %**（同一组 9 个读数），在 15 % 之内；按单窗登记，不外推。
- **适用 Seek：未判定**。唯一现成仪器是 CPU 测试渲染器上的功能矩阵（33/129 记录、`formal:false`），
  180 次 seek 的 request→present 中位 533 µs、p95 1 019 µs、最大 1 292 µs，只作代价证据；计划禁止缩小
  适用范围，所以 20 ms 门保持未判定。
- **正式读取相对门：超界**。A 臂为 pre-PD `d344088` 的独立构建，B 臂为 `6256af8` 的 Release STATIC；
  3 窗口 × 3 对、窗间 600 s、逐对 B/A 比值。全组几何平均 **1.1732，95 % 区间 [1.1574, 1.1893]**，
  三个窗口各自的下界也都大于 1.10，按 §9.2 判为超界而非 inconclusive。每对两侧的图表 sha256 与快照
  digest 全相等，说明两侧做的是同一份工作。

直接后果：**读取相对门已明确超界，R3 技术接受不能判为通过**——除非有新的实现把比值拉回 ≤1.10，或一次
新的登记窗口把它推翻。本批不修改任何判据、范围或阈值；其余门（D2/D3/Snapshot/排队、request→ready、
GPU render、Pause/Resume/resize、Reset/Close）仍未测。

### 第二批：控制与时序（同日）

仪器换成 PD5 脚本化 host 探针编译进的 `cuexis_execution_device_probe`（真实 OpenGL 后端 + GPU timer
query），报告见[第二批正式门测量](2026-11-01-r3-second-gate-batch.md)，附件见
[第二批证据](2026-11-01-r3-gate-batch2-evidence/index.md)。

- **Pause 超界**：每进程 40 次读数中恰好一条超过 50 ms，且都是该进程的第一次 pause（174.966–184.929 ms）；
  其余 39 次中位 0.79–1.86 ms。首次冻结绘制的一次性成本，不是稳态路径。
- **Resume / resize / Reset / Close 全部通过**：最大 2.248 ms / 1.171 ms / 70.666 ms（暂停态）与
  31.351 ms（运行态）/ 1.537 ms。
- **request→ready 与 GPU render 通过**：每进程 p99 0.423/0.408/0.436 ms 与 3.072 µs（n=1200）。
- **D2/D3/Snapshot/排队四项仍未判定**：本机没有可比较的无插桩子阶段边界，计划规定这种情形记未判定。
- **适用 Seek 仍未判定**：设备探针的 host 探针没有 seek 场景，功能矩阵是 CPU 渲染器、33/129 记录、
  `formal:false`，不能充作 20 ms 门。

截至本批，R3 的门状态是：初次 load 通过、重复性通过、request→ready 通过、GPU render 通过、
Resume/resize/Reset/Close 通过、**Pause 超界（首次使用）**、**读取相对门超界**、适用 Seek 与
D2/D3/Snapshot/排队未判定。
