# R4 O1 前集成核验与预算审计

状态：candidate；O1 前工作完成，新增数值等待 owner 接受

快照日期：2026-11-05

返回 [New RPA 报告索引](../README.md)。实施范围见
[R4 计划](../../../../../stage_plans/active/stage-07/05-rpa-new/r4/plan.md)，
待裁定内容见 [O1 预算与参数提案](../../../../../stage_plans/active/stage-07/05-rpa-new/r4/caps-proposal.md)。

## 1. 授权、起点与实际改动

owner 于 2026-11-05 要求“你先独立完成到 O1”。从
D:/Cuexis-worktree / cuexis_worktree / 8b499a86fae9616609c3ed76fd6bfc431ab6147b
保存原有 tracked patch、两份 untracked 文档及 5074 个文件的 SHA-256 后，
建立 codex/r4-integration。完整原始现场记录在 out/evidence/r4/pre-o1/；
本轮不是冻结的最终候选 C，不声称具有 R4 同 SHA F1/F2 接受。
关键原始记录归档到[本轮证据包](2026-11-05-pre-o1-evidence/index.md)，原始失败同样保留。

本轮先添加内部 constexpr 尺寸查询：OwnerChannel、OwnerStepLedger、
FrameDelivery 与 FrameObservationLedger 的真实私有槽位；随后依据 MinGW 预检失败，
修复音频启动失败时 Close 与 owner 退出发布的竞态，补充确定性反例，见 §3。
内部 AudioWorkerOptions 新增一次性退出测试门；公开 SDK、wire、版本及预算判据保持。
此处内部头属于 app/execution_host，不是 installed public headers。

工具侧补齐：

- run_rpa_acceptance_matrix.py 的 --build-root：每 invocation/row 新建目录；拒绝现有
  invocation、嵌套已有 CMakeCache 的构建树、跨工作树路径和解析到树外的构建根。
- R4 Release sizeof-only 探针及其当前布局记录，包含私有真实类型与 276 个项目输入 hash。
  不再使用私有类型的手写结构镜像作“标准保证布局相同”的证明。
- 覆盖 24 个域、3 个规模的可重算预算建议表和草案校验；保留 unaccepted、
  formal_registration=false 和 analytical_upper_bound_proven=false。
- 独立目录、缺域/零值/自接受、错误统计值、cold 请求污染、阈值/窗协议修改等拒绝测试。
  当前草案工具没有正式执行器或 claim/seal，不把它写成正式工具门已全部关闭。

工具按 CODE_POLICY 的功能命名规则置于 tools/verification/execution_acceptance/；
历史日志中的初始目录名保留。草案校验明确 workload 与五类 Seek，四类可重复请求固定
每进程 16 次，cold 固定 1 次；拒绝用 3 请求复活历史重复性筛查，非 Seek 的原计数保持。

## 2. B3/B4/B5 合同—实现—测试核对

现有实现已经覆盖这些分批；本轮保留 B3/B4/B5 实现，只对新复现的音频启动失败 Close
竞态作局部修复。原 tests/rpa 中 B3 有 6 份测试源/56 个 TEST_CASE，B4 有 4 份/36 个，
B5 有 9 份/62 个，共 154 个入口；本轮新增 1 个 B5 确定性入口。运行程序还包含 B1/B2、
PD 和执行边界等测试。

| 审计项 | 当前实现/测试落点 | 本轮结果与范围 |
| --- | --- | --- |
| B3 typed/version/wire | execution_kernel、recovery codec；rpa_b3_control/recovery_tests | rev3 拒绝旧版本、完整字段/错误、golden wire 及逐字节截断用例通过 |
| B3 T2/推进与前缀 | execution scheduler；rpa_b3_control/schedule_tests | 独立扫描 oracle、同 Tick、乱序/缺片、Q1/K1 和 resumed H 边界通过 |
| B3 R2/recovery | execution_kernel_archive/restore；rpa_b3_recovery/checkpoint/native_tests | 早期 cut→恢复→后续行为、future semantic allocation failure、原生来源与 partial cut 通过 |
| B3 时间桥/尾段 | realtime_clock；rpa_b3_clock/schedule_tests | 44.1/48 kHz、独立 offset oracle、负 ties/溢出、旧缓存、EOF/Tail、暂停重基准通过 |
| B3 profile 集成 | RealtimeClock/Scheduler；现有 consumer 门 | 缺参拒绝与 reference 路径通过；生产 drift/参数仍不能由本机确定性测试接受 |
| B4 Apply/Cancel/owner | host_protocol、owner ledger；rpa_b4_owner/close/capacity_tests | 取消/启动互斥、原终态、跨线程拒绝、预留槽和耗尽路径通过 |
| B4 资源/fence/generation | host_candidate、audio_owner、renderer_owner；B4 resource/renderer 用例 | 旧 active、候选拥有量、丢失发布、Prepare 失败与 stale generation 反例通过 |
| B4/B5 Close | execution_host_close、session_worker；R1 C2/C4/C7/C9/C10 等 | 清理许可、accepted binding、原操作续行、flush/导出失败、pump、退出/join 通过 |
| B5 实际线程/音频 owner | SessionWorker/AudioWorker；rpa_b5_session_worker/audio/tail_tests | 真实 worker 生命周期、reply 预留、coherent owner clock、EOF/Tail 通过；测试设备不等于实体听音/拔插 |
| B5 主线程与表现 | execution_host、capture_route、retained 控制用例 | Pause/Resume/Seek/Close、capture reserve、保留画面和设备未知终态路径通过 |

直接运行三个程序并保存 Catch2 XML：

| 程序 | 用例 | assertions | 失败/跳过 |
| --- | ---: | ---: | --- |
| cuexis_rpa_tests | 70 | 26504 | 0 / 0 |
| cuexis_host_protocol_tests | 62 | 121976 | 0 / 0 |
| cuexis_execution_host_tests | 62 | 50797 | 0 / 0 |

这些 XML 绑定修复音频竞态前的 Debug 二进制；修复后的最终增量重建和未过滤 CTest
另外保留。Pause capture_gap、历史重复 Pause、旧 CXC 句柄持有者等未定位问题继续保留，
通过回归不证明这些历史偶发问题已获根因解释。

## 3. 本地验证、错误处置与设备边界

| 检查 | 结果 |
| --- | --- |
| MSVC candidate Debug fresh configure | 通过，MSVC 19.51.36256 / Ninja / D:/vcpkg |
| 完整 clean-first build | 通过，571 个构建步骤 |
| 音频修复后 MSVC 增量重建 | 通过，35 个步骤 |
| 音频修复后 MSVC 未过滤 CTest | 1419 项，0 失败；1 项 symlink 物理逃逸检查因运行权限条件跳过，实际通过 1418 项 |
| architecture、版本、STATIC clean consumers、安装/分发门 | 上述 CTest 中通过 |
| cuexis_format_check | 通过 |
| R4 Python 工具拒绝测试 | 12 项通过，包括所有非 cold Seek 类的 1/3/15/17 次拒绝与 16 次接受 |
| B0 memory_caps.py --check | 原始检查 exit=1：开工即有的 CRLF 检出改变 script/profiles 两项来源哈希；仅对这两项按 LF 规范字节只读复算后，完整 B0 文档一致；冻结原件未修改 |
| update_version.py --check | 通过；版本仍为 26.11.04-1 / SDK 0.9.1 |
| 新布局/预算重算与输入 hash | 工具更名后重新审计并重算；276 个输入 hash 与交付布局一致，其中除 2 个工具路径外的输入与修复后测试源码字节相同 |
| MinGW candidate headless Debug | 原行 1266 项中 1 个 B5 失败、1 项权限跳过；修复后 37 步重建通过，完整 1267 项 0 失败、1 项同样的权限跳过，实际通过 1266 项 |
| 同 SHA hosted / 完整 11 行矩阵 | O1 前未执行最终门，保留到最终 C |

首轮 CTest 的 11 个失败是本轮执行环境错误：CTest 未继承 VS 开发环境，外部子构建缺少
INCLUDE/LIB，报 cstddef 和 kernel32.lib 缺失。保留原失败 log/exit=8，不改写为产品失败
或正式 invalid-process；通过 vcvars 包装并固定 VCPKG_ROOT 后，最终完整 CTest exit=0。
普通正确性回归的修正重验不消费 R4 正式失败组额度。

旧 B0 原始检查失败按原值保留，不写成原命令通过。两个 CRLF 来源文件均与开工清单
SHA-256 相同；LF 规范字节的完整推导一致证据见证据包。新的 R4 布局/预算工具和 JSON
显式固定 LF，避免其来源绑定再受新工具自己的换行转换影响。

MinGW 原预检另有真实 B5 失败：音频工厂失败后，AudioWorker 先关闭接纳、排空 pending，
最后才发布 exited；Close 在两者之间把仍未 exited 的 owner 绑定为参与者，
随后 post stop 到已关闭通道，得到 host.rpa.owner_closed。原测试独立重试 10 次均通过，
说明不能依赖调度偶现来证明修复。

新增反例在排空后、exited 发布前持有音频 owner，只在 Close 达到 prepareControl 后
由主线程 pump 放行；修复前在 MSVC 以同一 owner_closed 失败，修复后 7 项断言通过。
修复通过 mutex 保护的真实 admissionClosed 查询等待实际 exited，再生成新 Close
参与者集合。等待继续使用原 closeDeadline，退出/join 和失败原因保持原账，不忽略
owner_closed、不伪造退休终态、不按错误码猜测设备已退出。测试门仅在显式 testOnly
宿主配置中使用，缺省为空，位于 worker 最终退出阶段。

两平台修复后的确定性用例均通过。复核另外确认：Load 超时仍保留原 audioJob，cleanup
必须先接收/确认该回复才进入参与者绑定，因此不添加无对应反例的启动握手。
本补丁修复已复现的启动失败窗口，不宣称覆盖运行中所有意外 worker 退出或分配失败。

真实 GPU smoke 执行六帧，验证 opaque/transparent、候选拒绝保持旧 cache、reload
激活等路径并正常退出。日志显示 OpenGL 3.3.0 NVIDIA 616.64、
NVIDIA GeForce RTX 4060 Laptop GPU。首个 Start-Process 包装未取得 ExitCode，
仅保存 completed=true/exit=null，不据此声明成功；第二次用独立进程捕获明确 exit=0。
两轮都是普通 Debug 正确性 smoke，在后台构建期间执行，不能拿启动时间或 render_us
作为 reference 预算读数。没有执行新 P8、三窗口正式组、实体 DFJK/Hold、听音、物理
拔插、精确 AV 同步或分域内存采样。

只读设备盘点确认桌面 2560×1600 / 240 Hz、活动 Senary Audio 三角色同一 endpoint、
RTX 4060 Laptop 驱动 32.0.16.1664，以及平衡电源方案；这些仍不是正式进程运行绑定。

## 4. O1 实质发现与剩余准入

1. session 完整槽位是 39664 B，而非单独的 SessionCommand；B0 旧类型尺寸不能用来
   为新版本扩展规模。audio/history/consumer/step/journal 同样按真实槽位重算。
2. 64 个 native anchors 只限制索引个数；日志结果和外部合法 owning handle 仍可延长
   状态/页面/历史存活。当前没有任意时长/持有数量下的全 SDK 固定总堆上界。
3. Player、SessionWorker、AudioWorker、ExecutionHost 仍显式要求 testOnly；
   reference 数值不能直接成为已测 production 参数或通过开关去掉生产门。
4. 旧 observer 标签与 memory_domains 分类没有覆盖新增表中所有 prepare/decoded、
   retained/owning 和设备计费关系。新 24 域数值可提交 O1，但实际 collector、标签/分区、
   开销/量程验证和 immutable 注册仍须在正式运行前完成。

因此 O1 提供“沿既有规模”“接受新 reference 设计预算”“先定较紧生产目标再设计”
三条路径，推荐第二条；不是用局部峰值或取消 128 MiB 来自行设新产品上限。

## 5. 远端与交接

本轮只读核实：远端 master=3076948ce403ea0da6fbe389835b6a4c9defbcd3，
stage-7/PR #32 head=de73aa2c474fe142ef3d85764346fcd2773afce5，PR 仍 OPEN。
本地起点比 R3 hosted 多 3 个收尾/归档提交；R3 hosted 是本地起点祖先。
三个平台 workflow 支持所有分支 push。当前未推送 R4 分支，未改 PR、合并或发行；
后续 hosted 与 Version Gate 仍按实际候选 SHA/tree/base 处理。

本轮到 O1 的技术材料不代签具体预算、PD-5 退出或生产接受。推荐接受范围与每项数字
只在 O1 提案中维护；owner 决定后登记 hash/原话/日期，再按计划继续对应准入和正式工作。

开工保护条目 3048 个中，3047 个保持原字节；唯一允许变动是本报告的父级导航索引。
原 pre-PD d344088236ae589c0b49a4994ef0fbddd6567ce2 Git 对象可读，但本轮尚未
重建/绑定共同 A；该项与完整 collector、端到端注册/封存和最终候选冻结一并保留为
正式采样前技术准入，不用 R3 或当前 Debug 结果顶替。
