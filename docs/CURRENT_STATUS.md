# Cuexis Current Status

状态：current

更新日期：2026-11-03

本文只维护当前阶段、正在进行的任务和未解决的问题。实施与测量细节见带日期的阶段报告。

## 当前任务

- **R3 已按 owner 本次要求恢复实施，尚未退出**：继续六项重构优化；
  P1 v2 最终固定组未达收益/保护门，已不选用且保持产品源码不变；随后验证预算候选。
  新证据见[恢复实施与预算验证](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-30-r3-resume-and-budget-verification.md)。
  已冻结 prepare 输入的两次整份复制已消除，生命周期、相关回归和身份/Packed 字节对照通过。
  新 0.9.1 普通固定对照的 setup 中位数在 1K/10K/40K 分别下降约 7.6%/6.8%/6.2%；1K 基线重复性超 15%，
  其历史高点尚不能唯一归因，见[本轮准备复制优化](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-28-r3-resolved-prepare-copy.md)。
  [轻量探针](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-28-r3-seek-noise-attribution.md)已确认 40K 主成本在恢复后的 Fact 投影：
  零绑定仍读取/构造 40K Fact/key。同映像切换未稳定复现原约 11% 差距，A/A 也有慢点；诊断出现 20.591 ms，
  原因机制及正式/Seek 接受仍未关闭，不能改判原窗口或门限。
  后续[零绑定投影修复与通用方案撤回](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-29-r3-projection-empty-and-general-attempt.md)
  保留完整 Fact 校验，只跳过空索引下的 owning key 构造；固定普通 CPU Host 对照的 40K 首次中位数
  从 16.531 ms 降至 2.562 ms（下降 84.5%）。相对重复性仍超 15%，不代表正式接受。
  通用借用 key / 有序合并虽改善微基准，但 10K Host 首次三对均上升，已撤回并留档。
  [40K 可见实体真实 GPU 测试](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-29-r3-40k-physical-gpu-throughput.md)
  已完成：RTX 4060 Laptop、1280×720、VSync OFF，12 个有效进程 / 1440 帧均超 4.167 ms。
  无 timestamp 对照的复用快照、静态提帧、全量 Chart v1 行为分别约 18.25 / 13.39 / 4.47 FPS；
  当前不能达到 240 FPS，正常帧的 update / extract / submit 成本尚待优化。初版相机夹具作废并保留，
  有效结果来自补充可见性与材质像素验证后的新登记；不作为 R3/R4 或生产接受。
  后续 [1080P 实体容量](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-29-r3-1080p-entity-capacity.md)
  已完成 10–40K 粗扫、边界重复和持续验证：按完整帧周期 p99≤4.167 ms、平均及每秒窗口≥240 FPS，
  已确认仅渲染 1800 实体、静态 update / extract / render 1300 实体，各通过三次独立 20 秒。
  1900 render-only、1400 / 1500 static 有持续超预算，全部保留；此为本夹具的保守测试点，不是绝对容量或正式退出。
  已有分页共享状态、不可变准备复用、自然恢复锚点，以及 SHA 加速和有界编码器恢复点 `198ab96`。
  P3 后续[Runtime 外观重复查询优化](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-29-r3-runtime-appearance-lookup.md)
  已将同实体四次查询合为一次，固定 18 进程对照的 40K 静态 update 中位数由 11.414 降至 5.555 ms，
  完整 cycle 约下降 7.8%；1K 候选 update 重复性仍超 15%，不作为正式接受或容量重测。
  继续[消除本帧触及集合平方扫描](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-29-r3-runtime-touched-membership.md)，
  新固定对照的 40K 全量动画 update 由 147.933 降至 47.343 ms，完整 cycle 由 209.573 降至 109.652 ms；
  静态路径未见对应退化。只加标记的初版因晚插入基线的语义回归停止并撤回；最终版同步索引并通过反例及相关回归。
  当前渲染仍超预算，P3 和其余待优化项继续开放。
  [MSVC Ninja 头依赖漏记](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-30-r3-dependency-tracking.md)已修复，独立/嵌入夹具的真实重编通过。
  owner 要求恢复后连续完成六项剩余工作，候选与完整验证进度统一见
  [六项实施与验证账本](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-29-r3-six-item-completion-work.md)。
  Pattern 编译复用、外观索引提示和新编码缓存因固定对照未达预设收益/保护条件已撤回；
  名称索引、Packed 字典查找及内部 identity 投影复用分别通过独立诊断并提交；
  OpenGL 普通提交的同步诊断读回已分离，烟雾测试/像素探针显式开启，同源矩阵见下条。
  Host Seek 的 CPU 功能矩阵扩为 60 组合，新增首条长记录内部 H=5/902，实际 GPU 仍为 96 组合。
  这两处目标的 timer/ledger 无用身份复制已消除；后续标量 phase 写入再减少整行复制，独立对照中
  40K H=5 为 22.873 → 16.678 ms，H=902 为 265.502 → 232.641 ms。后者仍超 20 ms，
  原性能门未关闭；不同登记组不拼接为同一连续基线。
  2026-10-30 MinGW metrics ON 的四个相关目标共 111 cases 通过，实际链接未再出现 TLS 重复定义；
  有限通道 canary 的 runtime / snapshot 等阶段各 1,088 样本且无遗漏。两处移植性修复和失败记录
  见六项账本；这不替代其余配置、安装消费者或正式性能接受。
  随后 fresh RPA observation ON 构建的 Host / Judgement / RPA 回归及显式 60 组合 Seek 路径通过；
  P1 v2 两版 Packed 回归已通过，最终固定性能组未达预设收益/保护条件，已不选用。
  R3c 无预算隔离原型通过有限恢复、真实分配失败、分支 Replay 字节和堆观察；其 40K H902
  CPU Host 诊断为 237.105 → 2.7385 ms，不能当作 GPU 或后续预算版的成绩。
  最新预算 v2 的 fresh Release STATIC 四目标回归共 426 cases 通过；恢复后独立故障/边界等
  11 cases / 935 assertions 通过，真实堆确认最终 requested/storage 差值归零，但 40K 在旧额度下
  不收任何点。128 MiB 当时缺乏产品预算推导（owner 已于 2026-11-02 接受预算超过该值，见本节首条）；
  当前先建立实际保留成本依据，普通 Seek/writer 仍待测。
  后续未选用 v4 修正共享 Fact 页重复计费和归档赋值 source-floor 生命周期，四套件共428 cases通过。
  普通40K H902为250.021→2.655 ms，writer保护通过；Seek重复性仍超15%，不作正式接受。
  固定40K六代成本曲线已记录：允许全部六点时实际额外requested约1.18MB；128MiB当时仍无产品预算依据（该上限已由 2026-11-02 裁定取消）。
  [新发布归因](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-30-r3-publication-attribution.md)确认H902主要在Fact投影，H5主要在首次activation；批量Fact候选虽改善H902约33%，但保护项及重复性未过，未选用。
  [activation与活跃索引候选](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-30-r3-activation-and-active-sequence.md)将40K H5/H902进一步降至0.355/1.517ms，数值保护通过但完整重复性未过，均未选用；独立产品分页索引固定组无明显收益且Seek重复性失败，已不选用。
  [窗口启动响应与新归因](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-31-r3-startup-and-render-attribution.md)：
  owner反馈的40K长时间假死定位为探针建窗后同步CPU准备且无消息泵，测试探针已把准备移到建窗前并支持关闭取消，未声称产品加载已解决。
  JSON回调DOM解析存在宽数组平方扫描；SAX/legacy parse-once组合的40K prepare约17.555→2.028秒，身份与帧摘要一致，但完整重复性未过，未选用且不算首画面≤2秒通过。
  实例化原型仍未达预设收益门；诊断显示剩余约30ms主要在命令构造/排序/复制/摘要，下一步继续消除CPU侧重复物化。
  早期GL组缺精确Release DLL绑定的限制已留档，后续组显式绑定同工具链Release运行库；原组不回填接受。
  随后[中立命令转移及顺序检测](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-31-r3-neutral-command-transfer.md)已选用：
  保留原比较器/digest，避免重复排序与临时命令字符串复制；16进程普通GPU组40K render-only/static cycle改善10.33%/12.05%，收益/保护/重复性通过。
  逆序保护组与最终格式化CPP相关回归通过；未达到40K 240FPS，完整矩阵和其余接受仍开放。
  同一函数的工作树细化（就地构造、删除临时命令队列与move转移）经24排列摘要等价、399/140回归
  及独立16进程非劣性组（40K −12.63%/−6.65%，四点均在+5%带内、相对范围≤6.82%）后提交为`97b2570`；
  该组基线版本与日期不同，不与上一组数值拼接，仍不构成容量或正式接受。
  这些候选均未进入产品实现或完成接受；恢复前暂停现场、统计口径更正及版本化实验材料见
  [2026-10-30 暂停与候选交接](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-30-r3-pause-and-candidate-handoff.md)。
  历史[来源绑定实验](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-27-r3-source-binding-experiment.md)用同一 Release 二进制比较 canonical SHA 与进程内 owning source binding。
  2048 requirements、33 条历史的 CPU 宿主首次请求到目标帧由 91.809 ms 降至 1.016 ms；129 条历史由 333.624 ms 降至 1.057 ms。
  这是固定本地诊断的点值，wire/导入验证保持原合同，candidate C++ 消费者须重建；
  R3 未退出，R3c 已有隔离原型但未纳入产品或接受，R4、设备与生产接受仍独立。
  原始归因及历史失败保留，后续门禁见[实施登记](stage_plans/active/stage-07/05-rpa-new/r3/plan.md)。
  本轮保留 GPU 测试配额修复和 content-only closure 无复制优化（产品恢复点 `5fd84fd`）；身份 writer 范围插入试验
  未取得稳定收益，已撤销。该轮恢复后的四套相关回归、架构门与 96 份身份字节对照通过；新复制候选另行登记。
  架构 / 设计变更、对照结果与停止边界见[本轮登记 §5](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-28-r3-redesign-verification-and-optimization.md#5-后续取证和修复顺序)。
  现行内部设计集中在[执行状态、恢复与准备路径](architecture/execution-state-and-recovery.md)，
  架构/API 入口已同步；后续继续跟踪首次 Seek 与 Packed/prepare 剩余热点。
- **R3 门账已于 2026-11-02 收口，R3 仍未退出**：十五项门为 **14 项通过 + 1 项部分通过（适用 Seek 覆盖 2/5 类）+ 0 项未判定**。
  owner 同日作出四项裁定：接受“可开关阶段计时 + 归因 + 开销披露”作为 D2/D3/Snapshot/排队 的登记方式
  （四项按该方法通过：judgement/projection/runtime/snapshot/queue 每进程 p99 为 45.8–122.2 / 17.9–19.2 /
  6.0–6.4 / 11.0–18.4 / 26.2–88.4 µs，余量 8–300×，插桩开销约 +12% 已披露，判据未改）；
  接受 40K @ 240 FPS 判定为结构性不可能（40K 不再作为该能力的适用规模）；接受内存预算超过 128 MiB
  （128 MiB 不再作为 R3 上限判据）；本轮不运行 hosted CI。其余通过项读数：初次 load 580.0–603.2 ms、
  重复性 3.95%、request→ready p99 0.439–0.474 ms、GPU render 3.1 µs、Pause 16.754 ms、Resume 4.844 ms、
  resize 3.978 ms、Reset 17.062/16.270 ms、Close 5.059 ms、相对读取门 1.0405 [1.0191, 1.0624]、
  适用 Seek 两类最差 941 / 1012 µs。仍未完成：适用 Seek 余下 3 类、最终源码上的同源矩阵重跑、
  hosted 同 SHA、R3b 预算候选与 R3c 原型的采用决定、owner 对 R3 退出的裁定。
  见[四项门裁定](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-02-r3-owner-gate-rulings.md)。
- **2026-11-01 同源本地矩阵已执行（test-only）**：`tools/verification/run_rpa_acceptance_matrix.py`
  在 `466ff76`、worktree 干净的同一份源码上按固定行序跑完全部 11 行，逐行 configure / build /
  CTest / `[measurement]` 重复与未执行项见
  [同源验收矩阵](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-01-r3-same-source-acceptance-matrix.md)。
  本轮 11 行里 7 行 `local_pass`、1 行有用例失败（`msvc-on-release` 的
  `#904 Audio publication preserves device observation time across service polls`，同一二进制按其
  sha256 隔离重跑 108 次未复现）、3 行 MinGW 在构建阶段被 GCC 报出的测试源码可移植性缺陷挡住而未进入
  CTest；清单级 `source_unchanged` 为真，全部阶段日志按磁盘字节复核哈希。因此 P6「同源完整矩阵」本轮
  **未达成**（第三轮在同一批配置上 11/11 达成，见下一条），这次运行不产生 240 FPS、容量或 R4 结论，也不因任何单行在隔离重跑中通过而改判；
  [矩阵证据附件](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-01-rpa-acceptance-matrix-evidence/index.md)
  登记每份日志的来源、字节与哈希。
- **2026-11-01 第二次运行止于仪器与磁盘，MinGW 崩溃归因于探针而不是产品**：在 `3fbc31f` 的干净工作树上，
  10 行里 8 行 `local_pass`；`mingw-on-debug` 在 CTest 里 51/1264 失败、其中 102 次 `0xc0000374`；
  `mingw-on-shared-debug` 撞上磁盘写满而没有留下判定；第 11 行未启动。把探针的 replacement
  `operator new/delete` 从构建里去掉，同一套用例全部通过；gdb 把破坏点钉在
  `Catch::Detail::unique_ptr<T>::~unique_ptr()` 经虚表派发进 `libstdc++-6.dll` 的 deleting
  destructor，载荷位于 16 字节偏移处；隔离矩阵显示同样的偏移载荷在两套运行时的所有匹配释放路径上都存活，
  只在 `new` 配 `std::free` 的错配路径上致命。结论是**仪器缺陷**，不改产品；当 replacement 被编译掉时
  也不得声称任何分配普查（`scope=explicit-pmr-upstream`）。见
  [第二次运行与 MinGW 分配探针极限](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-01-r3-run2-and-mingw-allocation-probe-limit.md)、
  [证据附件](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-01-mingw-allocation-probe-evidence/index.md)。
- **2026-11-01 同源完整矩阵第三次运行为 11/11（test-only，本地）**：修好探针与磁盘下限后，同一条固定行序在
  一份源码上跑完 11 行，全部 `local_pass`，共 13,613 个用例、0 失败，6 行记录 120 次测量事件，清单级
  `source_unchanged` 为真；启动时磁盘下限 12.0 GiB、当时空闲 19.9 GiB。六处修复（三份测试
  `CMakeLists.txt` 的探针可用性条件、探针头文件的 magic/foreign-free 加固与 LF 固定、`tests/pd/README.md`、
  运行器的磁盘下限与 `skipped_disk_space`）已提交为 `8963889`；内容层闭合由
  `verify_identity_closure.py` 证明——该 commit 的检出与运行记录在 3388 个文件上逐字节相等、两个聚合哈希相同，
  换一处全新检出时只有换行形式不同（2829 原样 + 488 纯 LF + 4 纯 CRLF + 67 混合，0 处漂移），可见
  `worktree_sha256` 记录的是工作树历史而不是内容。这**只**说明每个配置在同一份工作树上可构建、可跑完全部
  CTest，`skipped_disk_space` 之外没有静默跳过的用例；它不证明性能门、设备路径、听音或 hosted 同 SHA，
  也不改变 40K 结论与 R3 退出状态。见
  [同源验收矩阵第三次运行](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-01-r3-acceptance-matrix-third-run.md)、
  [证据附件](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-01-rpa-acceptance-matrix-run3-evidence/index.md)。
- **2026-11-01 1080p 容量前缘是实测区间，不是容量值**：预注册阶梯跑完 8 个实体数 × 两种模式 × 3 个新进程
  × 20 s，`仅渲染`在 1800–2400 通过、2600 与 2800 未通过，区间 [2400, 2600)；`静态提帧`在 1800–2200 通过、
  2300 起未通过，区间 [2200, 2300)。冻结的 1800 / 1300 仍是唯一登记过的容量值，本阶梯不升级任何通过点。
  1800 锚点的 `仅渲染` 中位数比 2026-10-31 刷新低 14.41%（超出 ±10% 漂移带），所以两次不是同一测量条件下的
  同一份数据；因此 1300→1800 段拟合出的 2544 / 2137 这类外推被实测证伪。越界形状是台阶而非斜率：
  额外时间是 `backend.submit` 调用内部一段约 7.15 ms 的 CPU 停顿，间隔中位数 4 帧，与实体数几乎无关，
  根因尚未定位。见
  [1080p 容量前缘区间](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-01-r3-capacity-frontier-bracket.md)、
  [证据附件](stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-01-capacity-frontier-evidence/index.md)。
- **40K 启动响应已有可观测证据，仍未达“首帧 ≤2 秒”或首次加载**：同一 40K v3 夹具的八轮窗口观察中，
  进程内首次可响应从设计改动前的 35,141 ms 降到 3,029 / 2,821 ms（v7 / v8），首个窗口帧 2,858 / 2,821 ms；
  v8 首次把观察绑定到具体二进制哈希，v1–v7 无 exe 哈希。235 个采样里 3 次未应答，单次等待 61–63 ms、最长 66 ms。
  窗口出现时间跨 `6ae7bd7` 设计改动不可比，可比指标是首次可响应；2.82 s 不是“进程到首帧 ≤2 秒”通过，
  也不是冷缓存首次加载，`render_us 0` 表示未采到渲染计时。prepare 侧同源两轮探针显示 `prepareLoad`
  从 2,876 ms 降到 2,172–2,202 ms、产品日志 `content_prepared` 从 3,191 ms 降到 2,286–2,323 ms；
  两轮同源探针相差约 24%，故只作组合项方向性陈述，逐阶段差值不归因到单个提交。
  见[启动可观测性与准备复用](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-31-r3-startup-observability-and-prepare-reuse.md)、
  [证据附件](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-31-startup-observability-evidence/index.md)。
- **40K @ 240 FPS 判定为不能，缺口是结构性的**：1080p、`466ff76`、真实 OpenGL 可见窗口下，40K 入镜实体的
  完整周期 p99 为 50.282 ms（仅渲染）/ 61.325 ms（静态提帧），分别是 4.166667 ms 预算的 12.07× / 14.72×，
  需要再快 10.71× / 13.71×；冻结点为 16.39× / 20.18×。降低分辨率无效（720P 同二进制对照：仅渲染 FPS 低 1.53%、
  p99 高 7.00%，静态 FPS 高 0.09%、p99 高 1.39%）；实例化这一唯一被测量的架构杠杆只值 1.32×，差一个数量级。
  容量前缘仍按冻结值 1800 仅渲染 / 1300 静态（HEAD 上两者仍有更多余量），不发布新值。
  **owner 于 2026-11-02 接受该判定为结构性结论**：40K 不再作为 240 FPS 的适用规模，判定文本与读数不改写，
  240 FPS 的边界仍以冻结容量值与前缘括区表述，不发布新容量值。
  128 MiB 的双层事实保留（作为进程上限装不下 40K 准备：峰值私有最小 254.7 MiB；作为续点计费参数
  挡住的不是内存：计费 244,028,048 B 对实际多留 323,303 B，755× 高估），但 owner 同日接受内存预算
  超过 128 MiB，128 MiB 不再作为 R3 的适用上限判据；不设新数字上限，正式 caps/profile 注册仍归 R4/PD 线。
  见[40K 240 FPS 与预算判定](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-31-r3-40k-240fps-and-budget-verdict.md)、
  [证据附件](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-31-40k-verdict-evidence/index.md)。
- **PD-5 定案与 Stage 7-PD 退出裁定**：判定报告已提出“未通过但定案”及退出清单，尚待 owner 接受；读取与 Seek 整改由 RPA 线承接，
  分域内存及正式/设备/生产验收缺口仍按下文跟踪。见[判定报告](stage_reports/stages/stage-07/04-pd/pd-5/exit/2026-10-24-pd5-verdict-and-stage-7-pd-exit.md)。
- **超限源文件拆分完成（2026-11-03）**：`engine/`、`app/`、`tests/`、`tools/` 下已无超过 800 行的 `.cpp`
  与超过 500 行的 `.hpp`（终检 0 个）。拆分覆盖源文件、测试文件与内部/公开头文件，每个文件一次独立提交，
  提交说明记录拆分前后行数、增量构建、相关测试二进制、`cuexis_format_check` 与逐行等价校验的结果；
  逐文件进度表见工作区 `out/_split-plan/PLAN.md`（`out/` 不入库）。拆分同步修正了三处门与链接面：
  诊断形式化门改扫 `execution_kernel*.cpp` 与 `execution_kernel_internal.hpp`；架构门对
  `input_boundary` / `gameplay_assembler` / `gameplay_graph` 三个分区按“伞头 + 分区头”拼接后比对
  （文件存在性与声明清单不变）；`tests/external` 的两个整包消费者补齐拆分后的 Validation Sink 源文件。
  末次拆分后完整 Debug CTest 全绿（仅保留既有 symlink 跳过项）。该批只改组织方式，不改行为契约。
  收口复核同样由实施侧完成，四项独立校验通过：架构门变异自测 4/4（改掉被检查类型必失败、改未检查类型
  必通过）、解码器 12 个阶段按原文重建后逐 token 一致 12/12、9 组拆分头拆前 514 个命名空间级声明缺失
  0 个、`GraphSink` 21 个类外定义逐字一致 21/21、27 个拆分后头文件（含伞头与分区头）各自单独编译
  通过。复核另发现并修复两处真缺陷：
  架构门的类型条目此前用子串匹配，`class CompiledPatternStorage;` 这类前向声明即可满足
  `class CompiledPattern`（拆前即如此），已改为必须匹配定义；`timebase_rational.hpp` 缺
  `<cuexis/core/result.hpp>`，此前靠伞头先包含才通过。终树 Release 全量构建通过，
  `ctest --preset release` 1211/1211 通过；格式门、文档门与诊断形式化门（含 `--self-test`）通过。
  无上下文子代理复核（同日）另发现两处门强度问题并已修：架构门的声明检查只做文本匹配，被注释掉的、
  包在 `#if 0` 里的、以及只是前缀相同的更长名字都能通过，函数条目只要是子串即可；门也没有可达性检查，
  伞头漏掉一个分区头时仍全绿。现改为先剥离注释与 `#if 0` 段、类型条目要求标识符边界与真实定义、函数条目
  要求参数表，并要求每个分区头被伞头或同分区的其他文件包含（`95de4a00`），变异自测 8/8。诊断形式化门在
  输入缺失时静默缩窄（代码表 schema 被删、`execution_kernel*.cpp` 全部消失、扫描目录不存在都仍通过），
  现固定七个输入文件、六个扫描目录、至少两个内核编译单元与每目录扫描文件下限，`--self-test` 增加三种
  输入缺失用例，均以 exit 8 失败（`64cf90a4`）。

本重构分支的唯一实施入口为 `D:/Cuexis-redesign`，分支 `Cuexis-redesign`。
接手资料中的其他 worktree 和分支均为历史来源，本批不在其中执行操作。
已完成工作、候选处置和逐轮测试记录见[Stage 7 报告索引](stage_reports/stages/stage-07/README.md)。

## 阶段状态

| 阶段 | 当前状态 | 入口 |
| --- | --- | --- |
| Stage 7A | S7A-3/4、5/6、7/8 的受限功能验收完成；整体关闭仍待 RPA 收尾 | [Stage 7A 计划](stage_plans/active/stage-07/01-a/plan.md) |
| Stage 7-RPA | R1、R2 已退出；owner 于 2026-10-26 接受 R2 按原计划退出，受控局部结果及历史限制保留；R3 未退出，实施已按 owner 本次要求恢复，2026-11-02 门账收口为 14 项通过 + 1 项部分（适用 Seek 2/5 类）、0 项未判定，四项阶段门按 owner 接受的插桩登记方式通过，退出裁定仍待 owner；R4 正式、设备与生产接受未登记 | [整改计划](stage_plans/active/stage-07/05-rpa-new/plan.md)、[退出复核](stage_reports/stages/stage-07/05-rpa-new/r2/2026-10-26-r2-technical-closure-review.md) |
| Stage 7-PD | PD-0–4 内部实施及适用门禁完成；PD-5 判定为「未通过但定案」，定案与 Stage 7-PD 退出清单尚待 owner 批准。原 40k 功能与小规模最终释放已核验，40k 分域内存仍未测量、正常分配器正式性能未通过。B0 已接受、B1 静态工具完成且未采数，B2/B3 未授权；ETW r4 三用例通过只证明工具合同。读取优化归 RPA R2，Seek 优化归其后续整改，旧窗口 3 停止，子阶段未退出 | [PD 计划](stage_plans/active/stage-07/04-pd/plan.md)、[PD-5 判定报告](stage_reports/stages/stage-07/04-pd/pd-5/exit/2026-10-24-pd5-verdict-and-stage-7-pd-exit.md) |
| Stage 7B+ | 未开始，等待 RPA 完整退出 | [Stage 7B+ 计划](stage_plans/active/stage-07/06-b/plan.md) |

Stage 0–6 已完成；Stage 8–14 尚未开始。整体 Stage 7 未关闭。
阶段依赖与后续安排见 [Stage 7 总计划](stage_plans/active/stage-07/plan.md)和[路线图](ROADMAP.md)。

## 当前问题与关闭前置

- **完整 Packed / prepare 已继续优化**：[前轮索引优化](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-27-r3-packed-index-optimization.md)后，进一步[细分 prepare](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-27-r3-prepare-declaration-optimization.md)确认 assembler 声明查找及回填仍有平方扫描，现已改为有序表二分。本轮独立普通对照中，10K 完整准备从 3.05–3.16 s 降至 1.74–1.75 s，40K 从 27.6–27.8 s 降至 7.44–7.63 s，首次请求到 CPU 测试呈现约 15.5–16.4 ms。四配置相关回归及三规模字节/96 场景兼容对照通过；timer、identity/closure 及编解码仍有成本。点值不替代正式、设备、内存或生产接受，R3 未退出；一次旧诊断 exe 误覆盖及证据保全见报告 §6。
  2026-10-28 全量核验已登记：完整桌面候选 1393 项中 1388 通过、4 个 external consumer 失败、1 个跳过；许可证清单修复后 verification-full 的四个 Release consumers、redesign-shared 的四个 SHARED consumers 和两个 Shared import checks 均通过。设备探针的过时平铺字段断言已改为 owning `snapshotView()`，精确实体设备用例 1 case / 37 assertions 通过。GPU capture 的 renderer 测试配额已修正，capture-1/2/4 各 8 frames 的独立功能复核通过，生产额度校验未放宽；不替代设备性能接受。40K decode/re-prepare 和 updated 提帧仍是下一轮热点。详见[2026-10-28 全量核验与优化登记](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-28-r3-redesign-verification-and-optimization.md)。


- **历史慢态及尾延迟未归因**：PD-0 最新登记组重复性通过，但不解释历史近两倍慢态；该组 Debug 宿主仍保留 15.93 ms 的单帧请求到回执尖峰。PD-5 本机有限 Release 门槛已接受，生产尾延迟预算未接受，不能据均值重复性通过推断生产延迟合格。
- **控制附近帧率波动待量化**：PD-4 实体 DFJK 完整课程与控制观察已完成，旧版容量错误本轮未复现；Reset/暂停恢复时约 240→130→240，持续时间与模块成本未测量。设备失效/恢复仍未验收。
- **局部偶发故障追踪**：B5 Pause 的 `capture_gap` 仍未定位。CXC unpack 目录提交已用不共享删除的读句柄复现阻塞机制，并加入有限原子重试与确定性测试；原始现场的具体持有者仍未知，详见[整合记录 §3.2](stage_reports/stages/stage-07/90-shared/worktree-consolidation/2026-10-26-worktree-consolidation.md#32-shared-probe-and-cxc-output-commit-regression-repairs)。
- **40k 功能已核验，内存仍未测量、正式性能未通过**：原 low/high 不重跑，原超包络记录保留；后续归因将其改记为观察器量程产物，不能据此判产品超界或内存通过。小规模最终释放不替代 40k 分域预算和最终释放。B0 静态账本已接受，B1 通过静态工具门但未运行；B2/B3 的 caps 规模口径、覆盖/阶梯缺口及采数仍待授权和裁定。F1/C1/C2 原回归与噪声结论保留，读取整改由 RPA R2 承接；ETW r4 三用例已过，工具结果不替代性能接受。见[40k 包络归因](stage_reports/stages/stage-07/04-pd/pd-5/memory-attribution/2026-10-23-40k-memory-envelope-attribution.md)、[B0 账本](stage_reports/stages/stage-07/04-pd/pd-5/memory-attribution/2026-10-24-b0-memory-cap-ledger.md)、[B1 工具](stage_reports/stages/stage-07/04-pd/pd-5/memory-attribution/2026-10-24-b1-domain-attribution-tooling.md)与[判定/退出清单](stage_reports/stages/stage-07/04-pd/pd-5/exit/2026-10-24-pd5-verdict-and-stage-7-pd-exit.md)。
- **RPA 验收未完成**：已取得 PD-4 受限实体键盘与听音观察；精确音画同步、设备失效与压力验证、生产参数及预算接受仍待完成。R0 的 Seek/ON-OFF、Close 软件退休观测、旧 fence 与导出容量 partial 均按[原报告](stage_reports/stages/stage-07/05-rpa-new/r0/2026-10-24-r0-observation-and-anchor.md)保留历史结论。Q4 仍为 `inconclusive`（历史 B 探针缺失）。R1 已新增真实 owner 许可反例、独立 Close step 观测及四类续行证据，未回填 R0；Debug 和首错缺口已解决并经当前五构建复验；当前修复身份有限 pilot 通过，R1 已按本次授权完成退出核验，RPA 最终接受仍未完成；历史全仓格式限定及 noIndependentContract 导出事实见[新记录](stage_reports/stages/stage-07/05-rpa-new/r1/2026-10-25-r1-continuation.md)。
- **预算证据缺项**：端到端 Seek 延迟、prepare-only 峰值堆和 decoded semantic heap 尚未完成有效测量与接受。

问题证据与剩余门禁见 [PD-0 复测归档](stage_reports/stages/stage-07/04-pd/pd-0/2026-10-17-pd0-supplemental-retests-and-archive.md)、
[RPA 矩阵](stage_reports/stages/stage-07/03-rpa-legacy/f1-f2/2026-10-15-f1-f2-final-matrix.md)及
[预算基线](stage_reports/stages/stage-07/03-rpa-legacy/f3/2026-10-15-f3-final-baseline.md)。

## 产品边界

Cuexis 由可嵌入的 Playback SDK、独立 Cuexis Player 和 Cuexis Studio 构成。
外部宿主使用公共 Playback API；RuntimeSession、World、EnTT、SDL 和 OpenGL 属于内部或可选实现。

产品决策见 [ADR 0027](adr/0027-playback-sdk-product-boundary.md)，公共接口见 [API reference](api/README.md)。

## 当前格式与 SDK 合同

- 当前工作树 SDK API 为 **`0.9.1` preview**：按 2026-10-28 owner 指定，登记 R3 来源绑定新增 candidate 类型和 `StepTarget` 字段；
  消费者须整体重建，不能混用旧二进制。版本决定见[版本政策](guides/VERSIONING.md#091-r3-candidate-来源绑定)。
  PD-4 历史基线仍为 0.9.0，日期构建仍为 **`26.10.19-1`**；这不代表已经发行。
  新版本 STATIC/SHARED 干净构建、安装消费者、版本接受/拒绝和架构/导出定向门已通过，详见
  [0.9.1 与架构同步记录](stage_reports/stages/stage-07/05-rpa-new/r3/2026-10-28-sdk-091-architecture-sync.md)；未重跑完整矩阵或性能接受。
- Gameplay v2 是当前实施基线，顶层合同仍为 candidate；局部实现不等于整批验收。语义与运行边界见
  [Gameplay V2 Spec](formats/GAMEPLAY_V2_SPEC.md)、[ABI](api/GAMEPLAY_V2_ABI.md)及[RPA 补充合同](formats/rpa-execution-contract.md)。
- Chart v1–v4 的既有兼容路径保留；Chart v5、CXT v2、Packed Chart 与 Canonical Gameplay Graph 仍为显式候选路径，正式发行收敛归 Stage 8。
- 稳定 C ABI 归 [Stage 14](stage_plans/future/stage-14/plan.md)，当前只提供 C++ typed preview。

各格式的生产/候选状态与权威文档见[格式索引](formats/README.md)。

## 延期和禁止边界

运行时脚本和逐帧 script callback 无限期延后，不预留字段、capability、bytecode、ABI 或 Playback hook。
离线 authoring generator 不会被 pack、prepare 或 Playback 隐式执行。

## 更新规则

只更新当前结论，完成或被替代的状态应就地更新或删除。
测试轮次、命令、提交经过、授权沟通和历史问题处置保存在阶段报告中，不向本文追加过程日志。
