# Cuexis Stage 7-RPA R4 实施与验证方案

状态：active；O1 方案 B 已接受；正式取证与最终接受未登记

更新日期：2026-11-05

返回 [New RPA 计划索引](../README.md)。当前进度只见
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md)。本方案消费
[整改计划 §9](../plan.md#9-r4集成后取证区分时间内存与设备)的范围、判据和统计协议；
[选项比较](decision-proposal.md)保留取舍依据，不再作为待选路线清单。

## 1. 目标与选定路线

owner 于 2026-11-05 要求“按照你的推荐方案编写 R4 实施方案”。据此采用
**D1-B、D2-A、D3-B、D4-B、D5-B、D6-A、D7-A** 作为本计划的编制依据。
最初编制只交付计划，不把路线选定写成预算数字已接受、PD-5 已退出或任何实验已运行。
实际开工登记授权、起点和运行身份；不重复要求 owner 重选这七项路线。

2026-11-05 owner 随后明确要求“你先独立完成到 O1”。R4-0～R4-2、本地两平台正确性预检、
R4-3 完整建议表和 R4-4 不依赖新增数值的准备已完成；[O1 数值提案](caps-proposal.md)、
[预注册草案](formal-registration.md)及审计证据包已建立。
owner 于 2026-11-05 裁定采用方案 B；O1 接受的精确范围和输入哈希见
[O1 接受记录](../../../../../stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-05-o1-budget-acceptance.json)。
R4-4 工具/observer 准入、共同 pre-PD 基线重建绑定和最终候选冻结可独立推进；正式采样
须在这些准入完成后登记，且生产目标仍待测量和独立接受。

| 决定 | 实施安排 | 保留的接受边界 |
| --- | --- | --- |
| D1-B | 独立完成全部可自动执行的 R4 集成、工具、reference 取证与接受包 | 生产与实体设备缺口分别列出，技术交付不保证所有门通过 |
| D2-A | 活跃任务期间使用本机连续专用窗口，构建与正式采样分时执行 | 供电、前台、GPU/audio 端点须稳定；实体操作集中安排 |
| D3-B | 先静态推导完整 caps 和生产参数表，再集中提交具体数值 | 未接受的新 profile 不进入相应正式采样 |
| D4-B | 合同内修复、每问题至多两轮有限诊断、每正式单元至多一次全新登记 | P8 不适用此重开额度；原失败和慢值保留，详见 §4 |
| D5-B | 执行时使用 codex/r4-integration，正常提交、推送新分支、跟踪 CI | 不包含 force-push、合并、发行或替 owner 作出批准 |
| D6-A | 先平台预检，再冻结共同候选 C；最终本地和 hosted 验证同一 SHA | 后继报告提交 E 不能自动继承 C 的最终候选资格 |
| D7-A | 准备 PD-5 带已知限制退出与 RPA 承接清单 | 仍需具体移交接受记录，历史“未通过但定案”保持 |

本批以现有合同内必要集成和缺陷修复为范围，不预设新增性能机制。新增 owner/线程、
公共 API/恢复语义、性能架构、R3c 重新采用或产品能力范围改变须单独形成决定。
运行时脚本及逐帧回调继续无限期延后。

## 2. 基线、依赖与推进顺序

计划编制起点为 D:/Cuexis-worktree、cuexis_worktree、
8b499a86fae9616609c3ed76fd6bfc431ab6147b；已有状态清理和方案文档差异须保全。
当前版本为 26.11.04-1 / SDK 0.9.1，尚无构建目录；这些是起点，不是 R4 验证身份。

R1/R2/R3 已退出。R3 本地 d70df15a 与 hosted de73aa2c 的限定 delta 只适用 R3。
R3c 未采用；128 MiB 不再是 R3 绝对上限，但没有新的总 caps。40K 不属于 240 FPS
适用规模，已登记容量仍为仅渲染 1800 / 静态 1300，不通过本计划扩大能力宣称。

tests/rpa/CMakeLists.txt 已编入 B3 及 candidate 下的 B4/B5 测试。先审计已有实现和
证据缺口，再决定必要补丁；不得因批次待接受而整批重写。B/F 名称消费
[历史分批定义](../../../../../archive/stage-07-planning/03-rpa-legacy/2026-10-11-rpa-plan.md)，
语义仍以当前 ADR/Spec 为准。

| 批次 | 前置 | 主要交付 | 推进门 |
| --- | --- | --- | --- |
| R4-0 起点与缺口审计 | 既有退出记录、当前工作树 | 保全清单、合同/实现/测试/证据映射、构建环境记录 | 起点可复原，历史证据不受写入 |
| R4-1 B3 协议集成 | R4-0 | 时间、执行、恢复及版本闭包的最小修复与反例 | 相关确定性正确性、兼容门通过 |
| R4-2 B4/B5 宿主部署 | R4-1 | owner/fence/Close 与实际线程、设备部署核验 | 机制门通过；未接受 profile 的部署取证继续受限 |
| R4-3 caps 与 F3 参数 | R4-0 后可并行推导，最终消费 R4-1/2 | 完整分域预算、生产参数建议、超限行为表 | owner 集中接受具体值及适用范围 |
| R4-4 工具、注册与冻结 | R4-1/2、适用 R4-3 接受 | 工具拒绝门、固定运行表、平台预检、候选 C | 身份、工具、内容、环境及预算均可核验 |
| R4-5 正式与设备取证 | R4-4 对应单元准入 | P0–P7、适用 Seek、分域内存、新 P8、自动设备证据 | 固定组结束并裁定；失败按 §4 处理 |
| R4-6 F1/F2 最终矩阵 | R4-4 的 C；可与非本机工作协调 | 11 行本地矩阵、同 SHA 三平台 hosted | 无省略地登记通过、失败、跳过与缺证据 |
| R4-7 F4 接受与移交 | 前述结果均已明确 | 全门账、人工操作清单、PD 移交、最终接受包 | owner 决定退出范围及剩余责任 |

“可并行”指工作依赖允许交错推进，不在正式采样期间运行本机编译、CTest 或压力工具。
某个新增预算、人工项目或条件子项待定时，其他独立工作继续；不能借独立推进越过该项准入。

## 3. 分批实施与验收

### 3.1 R4-0：建立可追溯起点

1. 核对工作树 HEAD/status/diff、未跟踪文件与远端祖先关系；保存文件级 hash、patch 和新增
   文件清单。执行分支从实际起点建立；若同名分支已存在先核对，不重置。既有 dirty 修改不覆盖。
2. 从证据清单解析冻结构建、旧 invocation、F1 collector r4、B0 工具/数据及原始日志路径，
   加入写入拒绝清单；已清理的历史构建不原地重建。所有新构建与证据使用新的唯一目录。
3. 建立门账：每条合同列权威来源、现有符号/测试、历史证据、新候选待证项、交付批次和判据。
   缺口区分代码缺陷、覆盖缺口、工具缺口、环境条件和 owner 决定，不把历史绿色当新验证。
4. 检查 cl/Ninja/CMake、D:/vcpkg、磁盘与设备库存。通过继承 VS 环境的构建进程或现有
   runner 的 vcvars 包装配置 MSVC，并显式保留 VCPKG_ROOT；不假定 PowerShell 调用 bat
   能修改父 shell 环境。首次构建 fresh，按实际变更运行 Debug/格式/版本/文档门。

退出条件：起点与受保护文件可核验，B3/B4/B5 和 F1–F4 缺口均有归属，构建/证据目录不冲突。
本批只核实基线和环境，不以设备枚举结果证明播放、听音或设备接受。

### 3.2 R4-1：B3 时间、执行与恢复闭包

核对 engine/judgement/src/execution_kernel*、恢复实现、
engine/playback/src/playback_session_gameplay* 与 tests/rpa/ 的现有实现。
逐项消费 typed/version/wire、reducer/advance、control cut/recovery、clock bridge 和 profile 集成门。

- 保留独立 oracle，覆盖同 Tick 排序、连续前缀、重复/冲突输入、暂停切段、future pending、
  早期 cut→Snapshot→recover→继续执行、非法来源与错误投影；比较完整字段、wire 和后续行为。
- 核对 44.1/48 kHz、负 Tick/极值、checked 舍入与 offset 只应用一次、旧时钟观察、欠载/换段、
  Tail/EOF 与暂停重基准；testOnly 调度值不冒作生产 Scheduling/TailProfile。
- 修复必须对应可复现反例；无缺陷处保留已有实现。公开头 ASCII、依赖 allowlist、
  exception/Result、candidate ON/OFF 与 STATIC/SHARED consumer 边界一起核对。

退出条件：适用确定性测试和兼容门通过，真实时钟 H、F(H)、processedFrontier 的观测边界明确；
剩余生产 drift/预算进入 R4-3/5，不能由 headless 测试代验。

### 3.3 R4-2：B4/B5 owner、资源与实际部署

从 app/execution_host/、app/player/src/execution_host* 及既有 host protocol/worker 测试入手。
先完成可控 B4，再验证 audio owner、D2+D3 同 worker、主线程协议及设备连接的 B5 部署。
已接受 reference 参数可用于相应功能核验；依赖新增参数的正式部署取证等待 R4-3。

- 核对 Apply/Cancel 终态、candidate/active 资源事务、prepare/commit fence、generation、
  device handshake、receipt/ACK/present 的真实边界，避免复制协议权威或把不同端点混成同一指标。
- 回归 R1 的清理许可与阶段续行：原 job/operation/typed reply、首次超时/失败、旧 fence、
  accepted binding、flush 失败、generation、pump 内 query、最终退出与 join。
- 对 Pause capture_gap、重复 Pause 与 Reset/Resume 周边波动保留故障记录，按 §4 定量诊断；
  不在没有复现和反例时宣称根因已解决。CXC 历史句柄持有者未知不改写为已定位。
- CPU owning handle 可跨 Close；设备/worker 在途退休和 join 仍计入 Close。先软件故障注入，
  再按设备矩阵核对实际结果；软件模拟不替代合同要求的物理故障或人工观察。

退出条件：机制反例与实际线程功能回归通过，剩余性能/设备门有明确运行单元。超时进程先保存
日志、退出原因和身份，再终止本批已记录的进程树并确认退出；不能遗留 GPU/audio 占用。

### 3.4 R4-3：完整 caps、生产参数与集中数值裁定

以声明容量、实际 sizeof/alignof、容器增长上限、同时存活关系和明确余量推导预算。
复用冻结 B0 常量时只读导入，新 profile/caps 单独版本化；B0 的 1/200/201 注册不能外推为
64/2048/40K 正式注册。选项比较中的部分域算术示例不作为总预算或接受请求。

交付两张完整表和推导/拒绝校验：

| 表 | 必填内容 |
| --- | --- |
| 分域 caps | owner/domain、count、bytes、age、retention/generation、共享/别名去重、并存峰值、生命周期、排除项、超限行为、计算依据与候选值 |
| F3 参数 | 内容/规模/设备适用性、Scheduling/TailProfile、时钟误差、steady/恢复/Replay/Packed 容量、预算、错误/降级行为及来源；reference 与 production 分列 |

覆盖 World/current/candidate/scratch/index、payload/attempt、journal/receipt/command/delivery/history、
prepare-only、decoded semantic heap、CPU owning 结果、GPU/audio。分别声明 active、Close 后
保留合法 handle、最终 release/fence/owner 回收完成三个阶段；requested/storage、进程
commit/working set 和 GPU 实占独立列口径。未知项记缺证据，不能填 0 或无限。

有限诊断只核对覆盖、计数与观察开销，不从测得高水位反填 caps。先具名登记诊断范围和工具
量程；未具备有效上限的 profile 不启动正式采样。通过下文 O1 一次接受具体表与适用范围后，
冻结机器可读版本/hash；仍缺生产目标的条目保留未接受，继续完成其他 reference 工作。

owner 于 2026-11-05 另批准保持预算的语义整改 A 批：最多新增两轮 64/high、各一进程、
120 秒，不运行 40K/P8。该批已执行完，第二轮证明 active World 唯一下界超出原 cap，
按停止条件暂停 memory/P8；原 O1 与所有正式额度保持。证据与待裁定的后续范围见
[语义整改报告](../../../../../stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-05-semantic-memory-remediation.md)。
owner 随后选择保持 O1、先完成存储整改设计；[World page storage 设计](../../../../../stage_reports/stages/stage-07/05-rpa-new/r4/2026-11-05-world-storage-remediation-design.md)
建议评估组件级 128 槽页容量，但具体实现和任何新 workload/诊断均须另行裁定。

### 3.5 R4-4：工具准入、预注册、平台预检与候选冻结

优先复用 tools/verification/run_rpa_acceptance_matrix.py 和 playback_performance/ 下的
measurement.py、memory_domains.py、prepare_profiles.py、device_inventory.py；
memory_domains.py 是静态解析器，device_inventory.py 的枚举不是设备运行证据。
新增 R4 绑定、注册和裁定能力单列，不重写冻结统计来源或 observer.py::memory()。

1. 先核对 matrix runner 的目录隔离：当前 --output 只改变证据目录，构建仍使用硬编码
   out/build 路径。正式执行前补齐并验证独立 build root 或等价隔离方案，未实现的参数不能
   写成已可用命令。对路径冲突、历史目录、磁盘不足和中断保留有拒绝门；跳过不算通过。
2. 注册器展开 P0–P8、Seek 类别和设备单元：ID、适用性、共同 A/候选 B、profile/caps、fixture、
   完整轨迹、进程数、请求数、顺序、超时、invalid 条件、失败处置、输出 schema 和预计时间。
   检查 A 是原 pre-PD 共同基线及匹配 harness；无法合法重建时阻断相对门，不以 R3/Q4 代 A。
3. 为缺字段/身份错、A/B 不匹配、端点 schema 不同、非法统计值、分位数样本不足、失败替样、
   窗间隔不足、重复身份与未接受 caps 建拒绝测试。复用既有 log-pair Student-t 计算并校验结果；
   matrix runner 的重复 measurement 测试不当作 R4 三窗口九对。
4. 工具/profile 准入后才安排具名、有限的 pilot；登记其上限和终止条件，pilot 不进入正式组。
   尽早做 MSVC/MinGW 与 hosted 平台预检，先解决可发现的平台差异再冻结最终 C。
5. C 包含稳定产品、工具、版本、配置、fixture、profile 与预算引用。记录 SHA/tree、N3 差异、
   原 pre-PD A 身份、工具链、构建参数、二进制/hash、设备/环境和注册表；产品及工具输入须
   已进入 C，不带 dirty/未跟踪源码。确切 C SHA 和构建后 hash 写入外部 binding/seal，
   不为填自引用而修改 C。普通计时、归因计时、分域内存分别构建/启动，绑定同一产品行为。

退出条件：注册表无缺失必填项、工具拒绝门通过、适用数值已接受、平台预检完成，C 可重建。
采样开始前再核验设备、供电、前台、磁盘和进程残留；环境不符则不启动该单元。

### 3.6 R4-5：固定正式运行、内存与设备取证

正式 reference 使用 RTX 4060 Laptop、1280×720 窗口、VSync ON、240 Hz、固定 Senary Audio
端点、gain 0.35；保持并登记实际电源方案，确认交流电供电，亲和性消费既有拓扑策略。
桌面无需改为 720p。1080p/VSync OFF 容量诊断另属 profile，不混入本组或自动重开。

运行表严格消费总计划 §9 与原 PD-5 测量方案，以下是执行检查点，不替代其完整判据：

| 类别 | 执行检查点 |
| --- | --- |
| 普通计时 P0–P7 | Release STATIC、candidate ON、普通 allocator、归因/trace OFF；保持原内容/规模/负载，不用新小夹具替代 |
| 可比较单元 | 每单元 3 窗×3 对独立 A/B；顺序 AB/BA/AB、BA/AB/BA、AB/BA/AB；上窗全部结束后间隔至少 600 s |
| 热身与稳态 | 每进程机器热身 30 s、fresh-session 64 steps；P0 8 块且 read_long 每块至少 100 ms；稳态另满足 10 s、采样至少 60 s/10000 updates/10 倍 retention 或容量要求 |
| 控制与 Seek | P5 每进程 20 Pause/20 Resume/10 paused Reset/10 running Reset/10 resize/1 Close；五类 Seek 分开注册，逐类冻结每进程请求数、次序与冷热身份 |
| B-only | 每窗 3 个 B；没有共同指标时 A 为 N/A，禁止填 0 或伪造比值 |
| 新 P8 | 新 source/tool/fixture/binding 下 40K low/high 各一次完整规模单侧取证；不续跑旧 invocation，不补成九对，不检验 40K @240 FPS |
| 阶段计时 | 仅 D2/D3/Snapshot/排队沿已裁定的可开关例外；逐进程 p99、同负载开销对照，不混入无插桩门 |
| 分域与设备 | 与普通时间分开进程；active、Close 后持有 handle、最终释放均取证；自动设备结果与人工/仪器需求分列 |

Seek 的首次自然 checkpoint、同档热使用、外部归档首验、冷进程、无合法 checkpoint 回退
分别保留必要成本。可重复构造同类语义的单元采用每进程 16 次请求；首用类每次建立独立
会话/来源以保持首用身份。冷进程类每个新进程只计一次冷请求，后续热请求不混入；各类
均保持原固定进程组，不为凑请求数追加进程，并披露样本量及统计适用性。
load/Seek/Close 保留原端到端边界，含必需 sample/present、退休及 join；不把相位 p99 相加。

统计工具输出每进程 median/p95/p99/max/n、每对 log-ratio、逐窗/全组 CI 与正式重复性。
沿原 1.10、15% 和各绝对预算裁定；分位数不足不跨进程拼样本，慢值不删除。P8 low/high
不计算正式重复性。物理/hash 门按原判据 ID 和适用性逐项登记，不将 P8 一项外推为全门重验。

内存观察器的锁、块头、快照和自身存活量单列，不用历史开销比例扣除新读数。最终相关
payload/attempt/GPU live 归零，有界 World/cache/journal 回到声明稳态；不要求进程 RSS 为零。
prepare-only 峰值堆、decoded semantic heap 缺有效测量时必须单列缺口。

排程按完整单元估算：独立 A/B 单元仅 18×30 s 热身及两次窗间隔已需 29 min，另加实际采样、
准备和回收。启动前确认能完成整组，不为跨日/离线缩短窗口；任务不活跃时不承诺定时启动。
退出条件是全部获准单元有完整裁定或明确停止/缺证据原因，不是所有结果必为通过。

### 3.7 R4-6：最终 F1 本地矩阵与 F2 同 SHA hosted

沿用并审计现有 11 行矩阵：MSVC ON 的 Debug/Release/SHARED Debug，MSVC OFF 的
Debug/headless Debug/Release/headless Release/SHARED Debug，以及 MinGW ON 的
Debug/SHARED Debug/Release。各行 fresh configure、clean-first build、未过滤 CTest，
保存 architecture、STATIC/SHARED clean consumers、版本/格式/文档门及全部失败/跳过原因。
磁盘低于 runner 门限时停止/显式跳过，不以删冻结证据腾空间或将跳过写成通过。

Linux sanitizer/coverage/tidy 使用 Linux Quality，不在 MSVC 上强行套 Linux 专用 presets。
将 C 正常推送新分支，确认 Linux Quality、Windows MSVC、Windows MinGW 的实际 run SHA、
结论和日志；已有 workflow 支持分支 push，不依赖先更新旧 PR。推送前核实远端与 PR 实况，
区分 R3 基线和 R4 差异；不复用旧 CI、旧批准或默认认为 SDK 0.9.1 不触发 Version Gate。

需要版本批准时生成精确 SHA/tree/base/日期的 owner 评论材料；代理不替 owner 作批准。
任一代码、工具、版本或 profile 修复产生新候选并重新核定受影响证据；最终 F1/F2 仍须同 SHA。
报告/原始证据可入后继 E，但只声称验证 C；若 E 被选为最终接受/合并候选，按现行规则
重新冻结和验证，不借用 R3 delta 或自动设文档豁免。

### 3.8 R4-7：F4 接受包与 PD-5 带限制移交

以 gate ID 汇总实现、正确性、性能、内存、兼容、平台、设备与生产接受；每项给对应 C、
registration/binding、原始证据/hash、判据、结果、未覆盖范围、后续责任和恢复条件。
区分“实施完成”“证据通过”“owner 接受”，不靠报告数量或固定组运行完毕宣称整阶段完成。

自动证据整理完后提交一份实体 DFJK/Hold、听音、物理拔插、精确 AV 同步操作清单，
每项列场景、观测方式、人工/仪器、成功条件和记录位置；按实际合同和已有有效证据确定
仍需参与的项目。无法执行或没有测量手段的项保持缺证据。

PD 移交表逐项承接内存、正式性能、设备和生产预算责任到 R4/F1–F4；保留历史失败/慢值
和“未通过但定案”。owner 接受具体移交记录后才更新 PD 退出状态；R4 通过也不自动
关闭 RPA、Stage 7A 或 Stage 7。最后由 owner 选择已满足范围退出、带限制移交或继续限定整改。

## 4. 独立修复、重开与停止规则

- 身份/执行/语义错误立即停止受影响正式单元并保存现场；性能超界保留慢值、完成已冻结组
  后裁定；环境噪声为 inconclusive。invalid 必须事前具名，缺字段/身份错/超时慢值不事后改类。
- 合同内缺陷可自主修复并运行必要正确性回归；每问题最多两轮具名有限诊断，先写问题、
  组数、时限、停止条件，保留失败。不用诊断样本补正式组。
- 每 A/B 或 B-only 正式单元最多一次完整新登记，前提是存在可验证的代码/工具/环境处置。
  新 binding、新冻结组，旧结果独立保留且不择优合并；额度跨版本累计，不靠改名重置。
- **P8 不享有上述重开额度**：low/high 各一次；额外 P8 必须独立裁定。Close pilot 的既有
  限次规则也不自动变成正式失败进程替补权。没有明确处置的超界/inconclusive 不自动重开。
- 第三次正式尝试、阈值/产品范围变更、新性能机制/公共语义、新设备/负载类别或 ETW，
  提交原始证据和有限选项；其他无依赖批次继续。修复触及 C 时先重新冻结，禁止混代拼成绩。
- 每批保留准确差异与撤回清单；撤回只操作本批已核验文件，不用 git reset --hard/git clean
  清空工作树，不删历史失败、冻结证据、既有 archive tag 或他人修改。

停止裁定包沿总计划 §12.2：明确“当前候选达不到”与已证明的下界之别，提供保持判据继续
限定修复、改变适用范围/预算、暂停该能力三种路径及对 F1–F4 的影响，不默认放宽门槛。

## 5. 集中提交 owner 的决定

| 节点 | 代理先完成的材料 | owner 决定 | 等待期间继续的工作 |
| --- | --- | --- | --- |
| O1 数值接受 | 全域 caps、F3 参数、推导/余量、超限行为、profile 适用性和待证项 | 接受具体值/范围或给出调整；既有已接受值不重复申请 | 不依赖新值的集成、正确性、工具和平台预检 |
| O2 设备证据 | 一次完整操作清单与自动结果、所需仪器和准确记录方法 | 完成必要实体操作/观察，或明确保留限制 | 自动证据整理、其他设备/平台门 |
| O3 退出与移交 | 全门账、失败与缺口、PD 移交及 F4 接受包 | 接受退出范围、剩余责任和恢复条件；PD 历史结论保持 | 已获准且不依赖退出决定的收尾 |
| 条件节点 | 超出 §4 额度/合同的处置包，或最终 SHA 版本批准材料 | 仅决定这项新增边界或精确 SHA 批准 | 无依赖任务继续，不要求重选整体路线 |

常规代码修复、测试、运行登记、报告和既定 CI 工作不逐步回问；关键失败、完成或需要
决定时报告。O1/O3 可合并相关决定；人工/仪器事实和精确 SHA 批准不能由代理代签。

## 6. 交付位置、记录与完成标准

以下是计划中的产物名称，创建时填写真实日期/身份并接入索引；本次不预建空报告或伪证据：

| 位置 | 计划产物 |
| --- | --- |
| 本计划目录 r4/ | `caps-proposal.md`、`formal-registration.md`；分别负责待接受数值和冻结运行安排 |
| `tools/verification/execution_acceptance/` | R4 专用注册/绑定/裁定扩展、版本化 profile/caps 及其测试；复用现有底层实现 |
| `out/build/r4/<candidate-id>/<configuration>/` | 经目录隔离验证后的全新构建树；A/B、普通/归因/内存分别命名 |
| `out/evidence/r4/<registration-id>/<invocation-id>/` | 原始输出、binding、环境、hash 清单、开始/结束与退出状态；失败同样保留 |
| `docs/stage_reports/stages/stage-07/05-rpa-new/r4/` | 带日期的起点审计、集成核验、数值接受、运行裁定、F1/F2、PD 移交与 F4 接受包；必要附件按字节归档 |

计划只维护范围/依赖；CURRENT_STATUS 只更新当前结论；报告保存带日期证据。新报告及附件
从现有报告索引可达，原始字节不为通过 diff 检查而裁剪；代码和文档差异另行检查。

本轮**计划交付**以索引可达、链接有效、单一 H1、无过时待选状态以及
python -B tools/check_docs.py、git diff --check 通过为完成条件；不要求为纯计划修改构建 C++。
后续**技术交付**要求所有获准项有结果及证据/缺口；**R4 接受**还要求适用门通过或被 owner
明确处置、具体预算与设备责任已接受、同 SHA 证据齐备和 O3 记录存在。

## 7. 权威来源

- [总体 R4 协议](../plan.md#9-r4集成后取证区分时间内存与设备)及[停止条件](../plan.md#122-何时停止继续堆优化)。
- [RPA ADR 0046](../../../../../adr/0046-realtime-playback-coordination.md)、[补充 Spec](../../../../../formats/rpa-execution-contract.md)及[实时宿主边界](../../../../../api/realtime-host-boundary.md)。
- [原 PD-5 测量方案](../../04-pd/pd-5/pd5-measurement-proposal.md)及[B0 上限账本](../../../../../stage_reports/stages/stage-07/04-pd/pd-5/memory-attribution/2026-10-24-b0-memory-cap-ledger.md)。
- [R3 退出与 delta 边界](../../../../../stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-04-r3-hosted-delta-and-exit-ruling.md)、[四项门裁定](../../../../../stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-02-r3-owner-gate-rulings.md)、[重复性裁定](../../../../../stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-03-r3-repeatability-scope-ruling.md)。
- [PD-5 退出清单](../../../../../stage_reports/stages/stage-07/04-pd/pd-5/exit/2026-10-24-pd5-verdict-and-stage-7-pd-exit.md)、[证据清单](../../../../../stage_reports/stages/stage-07/05-rpa-new/evidence-catalog.md)及[版本政策](../../../../../guides/VERSIONING.md)。
