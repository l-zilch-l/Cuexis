# R4 O1 后工具准入与覆盖核验

状态：candidate；自动实施进行中，内存与 P8 尚未准入，非 R4 接受报告

快照日期：2026-11-05

返回 [New RPA 报告索引](../README.md)。范围与限次规则见
[R4 实施方案](../../../../../stage_plans/active/stage-07/05-rpa-new/r4/plan.md)。
[O1 接受记录](2026-11-05-o1-budget-acceptance.json)接受方案 B 的 reference 设计预算；
owner 后续“那么你继续完成”的授权见[执行授权记录](2026-11-05-execution-authorization.json)。
历史 O1 前结论保留在[前次审计](2026-11-05-pre-o1-integration-and-budget-audit.md)。

## 1. 已实施与当前范围

新增 admission/binding/build_binding/runner/results 工具，绑定 clean committed 源码、原 pre-PD A、
二进制、工具链、依赖、精确 fixture、预算与外部授权。claim/seal 使用授权中唯一 ledger；
正式单元采用规范化协议键跨候选累计，普通单元最多原始一次加具备可验证处置的一次新登记，
P8 low/high 各一次。seal 与运行标记不可覆盖；失败、崩溃和部分 claim 不恢复额度。

普通组执行器目前仅支持 owner/common-host，实际执行前还须完成每单元的语义、有限 pilot、
环境证明及冻结。三窗九对、顺序、600 秒间隔、1.10/15% 判据保持原合同；不补样、
不删除慢值、不将相位 p99 相加。保留每进程 n/median/p95/p99/max 与绝对门；
相对结果不冒充其他门通过。其他适配器尚未实现，不得据此启动对应正式单元。

普通构建检查 metrics、diagnostics、observation、trace、allocation observer、sanitizer、
coverage 等选项及目标编译命令，重新解析绑定的 CMake cache；检查整个目标的 Ninja
对象/头文件依赖、文件字节及时间关系。新增 fresh build 工具使用 VSLANG=1033、console
CP65001，保留 configure/build 原始输出，不改原 pre-PD 源码。

CPU/headless 环境沿原拓扑选核，显式固定前台 PID，机器热身每进程 30 秒。新增每秒轮询
进程、电源方案、交流电与前台状态，保留实测间隔和父进程 CPU 时间；间隔超过两秒拒绝。
这是一项有间隔的观测，可能遗漏间隔内活动，不能证明无所有外部干扰，也不证明 GPU/audio
环境或精确设备同步。原生 begin/end 的亲和性、优先级、throttling 仍独立核验。

## 2. 原 pre-PD A 与语义证据

原 pre-PD A 精确源码为 d344088236ae589c0b49a4994ef0fbddd6567ce2，detached clean worktree
位于 out/sources/r4/prepd-original。第一次 fresh Release STATIC 重建成功，但新增依赖门发现
Ninja 对象 #deps=0：中文 showIncludes 前缀经过原 CMake 的编码转换后不匹配。该构建拒绝
用于正式比较；保留原构建，不修改 A，也不以 R3 代 A。

在新的 prepd-english-release 目录，以同一源码、MSVC 19.51.36256.0、原依赖基线和英文
输出 fresh/clean-first 重建成功；owner/host 分别有 108/110 个对象的有效依赖清单，
构建身份复核通过。此处不表示新候选已完成所有构建/准入。

早期准备 B 与 A 的 deterministic presentation baseline 均通过 5464 assertions，
digest=17504891284788938091。另一次预声明的语义检查为 A/B × legacy/rpa 四进程，
digest、Replay/Snapshot/Packed SHA 相同；没有机器热身，不作 timing pilot 或正式证据。
当时 reads=4096，read-long 仅约 1–2 ms，不能通过正式每块至少 100 ms 的门。
新的普通候选必须重新绑定，并通过独立有限 pilot 冻结 reads 后才可登记。

## 3. Extended memory observer 的证据与缺口

新增 opt-in CUEXIS_EXECUTION_ALLOCATION_OBSERVER，要求原 lifetime observer 开启；
独立 collector、RPA_MEMORY 前缀和 schema，不改变历史 pd5_memory_observer.cpp。
普通和历史模式新增 scope 编译为空操作；没有新 installed public header 或生产启用。
30 个 allocation-origin 域及预算聚合映射由同一内部 .def 提供给 C++/Python。
同一锁内更新全局、cell、唯一产品和聚合 live/同时峰值，单列 fixture、observer 与产品。

两轮预声明的 64/high 工具覆盖诊断均为单进程、120 秒上限、非正式、非 P8。
完整内容 Snapshot/Seek/Replay 边界各通过 21 assertions，19 个 checkpoint 分账一致，
无 unknown live allocation。最终释放全部合法内容、结果和 fixture 后产品请求字节为 0，
仅 framework/bootstrap fixture 约 16,632 B 存活。该小规模结果不代验其他规模与设备释放。

第二轮仍有 prepare-origin 存活 28,915 B；current/candidate/world 的 origin 峰值分别
606,827/489,134/460,255 B，高于对应 reference 域数值 196,608/327,680/327,680 B。
现有 origin 标签尚不能区分共享当前、候选、历史结果和旧代的语义拥有关系，不能据此宣告
语义预算失败或通过。原 rating.json 的 54 条重复 checkpoint 告警内部仍写 over_budget；
原件保留，本报告纠正其适用解释。当前 parser 将其标为 allocation_origin_ceiling_exceeded，
整体为 partial/unrated、formal_acceptance=false，不静默覆写历史派生文件。

正式内存/P8 仍缺：语义拥有关系与去重闭合、count/age/lifetime、prepare-only 归零解释、
观察器量程/开销 reference 准入、native heaps、GPU backend/driver 及最终退休门。
不按已观察高水位上调 O1 预算，不启动第三轮等价诊断，也不消耗 P8 次数来探索工具。
其他无依赖单元继续；新增诊断额度或预算/适用范围变更须另附具体方案交 owner 裁定。

## 4. 验证与接受边界

当前新 Python 工具测试 29 项通过，覆盖不可变绑定、阈值/次数篡改、P8 伪装、观察器缺失、
编译插桩、绝对预算/样本不足，以及环境中途变化。extended observer 的独立合同测试为
2 cases/7 assertions，通过嵌套标签恢复、对齐分配/跨线程释放与并发一致分账。
新增 C++ source 已完成目标 Release/probe 构建；全量本地 11 行及 hosted 同 SHA 尚待执行。
O1 前 1419/1267 用例不能算本轮新源码的全量验证。

当前没有启动正式性能、memory 或 P8；正式额度未使用。证据原始位置为
out/evidence/r4/post-o1/，包括失败构建、两轮诊断、目标构建与语义检查，后续最终交付时
按字节归档并给出 manifest。最终候选、正式组和 F1/F2 将分别记录实际 SHA，不继承 R3 的
限定 doc-delta 裁定。O2 实体事实、生产接受、O3 退出与 PD 移交继续保持独立。
