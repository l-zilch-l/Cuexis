# 2026-10-26 Worktree consolidation and verification

状态：整合和本地验证完成。本报告记录用户授权的工作树整合、工具合同修正和本地构建验证，
不构成 R2 收益、PD-5、RPA、设备、生产或 Stage 7 接受。

## 1. Authorization and scope

2026-10-26 用户明确授权：先备份重要文件，再将
D:/Cuexis-pd5-40k-admission 整合到 D:/Cuexis-worktree，更新指定的两份桌面接手文件，
并构建、测试。后续实施入口只有 D:/Cuexis-worktree。
旧准入树保留为只读历史来源，不继续在那里修改或构建；没有删除其 out，
没有改写旧登记、claim/seal、raw、ETL、失败窗口或既有封包。
本次未授权新的性能窗口、40k、ETW、设备运行、候选机制或推送/发行。

## 2. Verified backup and source integration

完整备份根：D:/Cuexis-worktree-evidence/integration/20261026-consolidate-9c2e7b。
verified.json、两份 source ZIP、逐文件 manifests、Git bundle、管理目录/index、
二进制 diff 与两份原始桌面文件均已保存并校验。该备份包含全部受版本控制和未跟踪的源码、
主树本地环境脚本及关键登记；被忽略的原始 out 仍在原处保留。

| Identity | Before consolidation |
| --- | --- |
| Destination | stage-7; 0157486800f8f40a4121ef0d5ab26bd041a08304 |
| Historical implementation | codex/pd5-gameplay-40k-admission; 3e5837f63f76397429c9f7603fd7450d3ce9855d |
| Common ancestor | eed36648a692333e47f798aa45be36ca850d08de |
| Unique commits | Destination 2; implementation 48 |
| Existing working changes | Destination 21 document paths; implementation 77 paths |
| Destination document checkpoint | 59b743383eebedc23044ce327ae12799079815ea |
| History merge | 1048e4ea3f17e32d20e438f359eab03627ab465a |
| Imported implementation checkpoint | 372ec94045d4d39586f24cbdd3d4ae762cd2fca2 |

历史合并保留两边提交及撤回历史。CURRENT_STATUS.md 的同路径冲突按主题整合；
其余 76 个既有未提交路径先按备份原字节导入并单独 checkpoint，再作限定的整合修正。
out/consolidation-20261026-9c2e7b/import-manifest.json 登记原始导入内容。

## 3. Documentation and tool contracts

保留主树 RPA 旧计划的归档、compatibility 和 legacy 导航、六份 RPA 报告及准入树二十九份 PD 报告，
整合索引与当前状态；不把历史通过、失败或未测量改成新结论。
补齐第三候选停止状态，清理 R2 未启动、B0 尚未完成和 ETW HOLD 等旧现行摘要。

工具修正限于：

- 旧独立 canary 准备入口明确退役，在创建目录前拒绝并指向统一封包入口。
- 三项 deadline 必须全部相等，补三种单项漂移反例。
- 尺寸审计支持明确的 --generated，多候选时不再依据修改时间猜选。
- 真实失败 F1 日志作为固定、可溯源且保留原字节的测试 fixture，避免迁移后静默 skip。
- ETW、内存解析与准入工具的离线合同测试接入 CTest。

旧 B0 尺寸 JSON/caps 保留原审计身份，不因本次脚本修正而改写历史哈希或预算。
新工具实现不认证旧审计为新源码的预算测量；后续审计需显式输入和新的身份记录。
Windows Job 单测可启动有界 benign Python 子进程；本次不运行 WPR/ETW 角色。

### 3.1 Repairs found by the first complete regression

第一次完整 Debug CTest 保留了 14 项失败（日志 debug-tests.log，exit=8）：
11 项 Hash 计量/正控、2 项上游 OOM 注入及 1 项资产原子替换。
两个 OOM 用例进入 MSVC CRT abort 对话；先保存 PID、命令行和对话正文，
再只停止这两个已确认的测试进程，使 CTest 如实登记失败，没有删除初次日志。

- Hash count/stream 路径的私有 ByteWriter/Preimage 改为惰性构造，去除未使用的 Debug STL proxy 分配。
- MSVC PMR vector 的 allocator 构造 noexcept 会把预算/注入异常变成终止；
  仅将有界 Hash 的 POD 临时存储换成内部 PmrBuffer，构造不分配，增长可抛且保持旧块，
  迁移期间两块同时计费。公共 API、profile、HashScratch 和四项预算保持不变。
- STATIC 仍保留全局观察器与 meter 的严格等式、全部 allocation 点 OOM、释放归零与 SHA 断言。
  新增固定 workspace floor、强异常保证、移动所有权及 exact/少一容量边界回归。
- 独立正控和原 vector 迁移测试按本机实际 metadata 校准，不硬编码 MSVC proxy 大小。
- 资产诊断补全 native error/source/target，单次 adoption 未复现后没有按通过关闭问题；
  整套资产测试再次观测到 Win32 error 5。
  受控无 FILE_SHARE_DELETE 读句柄试验复现同一错误，关闭句柄后同源同目标可成功；
  无持有句柄的固定 100 次原生试验未复现。实际失败时的外部句柄持有者没有确认。
- Windows 离线发布 helper 对 access/share/lock 三类拒绝最多做 8 次原样原子 rename，
  中间最多七段 10ms 等待；不是系统调用总墙钟硬上界。持续拒绝仍返回原错误并保全源，
  不复制、删除或跳过验证；POSIX、公开 API、发布锁和 RPA 时限均不改。
  两个确定性 Windows 用例分别锁定短暂持有后的成功及持续持有后的失败与字节保全。

主代理先亲跑 Hash 完整 48 cases / 5567 assertions 与资产 26 cases / 398 assertions，
再运行下文五配置验证；这不是新的 R2 候选机制或性能接受。
上述源码修复提交为 4afe577bfffe4e83ef47071f9961d20234fdba91。
五处已迁入的 PD 格式问题和两处测试不可达 return 在本轮修正；旧日期报告保持原证据。

### 3.2 Shared probe and CXC output commit regression repairs

后续 Release 完整回归保留一项 cuexis_cxc_tools 失败；SHARED 聚焦回归保留 29 项
Hash heap corruption（0xc0000374）失败。原始失败日志均保留，没有跳过失败用例。

- SHARED EXE 曾无条件替换带 Header 的全局 new/delete，与 DLL 普通 CRT 内存跨边界释放不兼容。
  同一 floor case 原始 EXE 复现 0xc0000374，仅隔离这层替换、保持生产 DLL 不变后通过 38 断言。
  最终 SHARED 测试仅显式观察 PMR upstream，分配与释放同 EXE 配对，日志标记 scope=explicit-pmr-upstream；
  不证明 DLL 全局零分配。STATIC 保留 scope=global-cpp-allocations 和 2 次/384B 独立正控。
  两者均保留全部 Hash 语义、预算和逐点 upstream OOM 测试；未删掉 29 个失败 case。
- CXC 原始失败缺少 native 值及持有者证据；增加无主机路径的 category/value 诊断。
  100 次正常 noncanonical unpack 全成功；受控 staging root reader 不共享删除时，旧实现 10/10
  返回 system:32。读写验证完成后，目录 rename 仍会受这个外部句柄阻塞。
  仅在 unpack 备份、提交、恢复三个目录 rename 处，对 Windows 5/32/33 最多原子尝试 8 次，
  七次各请求等待 10ms；不是总墙钟硬期限。POSIX 和无关错误不重试；既有回滚/保全逻辑不变。
  修复后 30ms 释放的真实 reader 10/10 成功，持续 reader 10/10 如实失败并保全，
  另有无需定时竞态的注入和真实句柄确定性测试。原始那次 Release 失败的具体持有者仍未知。

主代理亲跑 STATIC Hash 48 cases/5567 assertions、SHARED 48/5569、CXC 66/1053 及 CXC CLI 门通过，
再按下节重建五配置和复跑适用回归。SHARED/CXC 修复提交为
07a6b5df4283ab4ab9729feb2731e3e29568f9b4。

其中一次 observation Release 回归另暴露 benign Job 单测的同步假设：Job 活动计数已零、
根 gate 已退出，并不保证所有已保留子进程句柄同时 signal。Job 实现原合同仅承诺活动计数归零。
仅修测试为同一个五秒绝对 deadline 内完成 drain、根进程 wait 和 child 只读退出确认，
不发送额外 terminate、不重置预算、不改生产 Job 实现。保留最终 child.alive==False 断言，
新增延迟 signal、永久不 signal、超界 signal 三个确定性反例；将旧测试方法装回内存后，
固定复现原失败。root-job-sync-counterexample.log 及 job-sync-counterexample.json 保留该证据。
最终测试修复和五配置重建/回归源码为 96e48e4ba644ce091d6ce1d4c7c1e1aa7b859db7。

## 4. R2 and evidence boundaries

第三候选 direct borrowed Result 源码按原停止状态迁入，不自动回退第二候选。
原停止记录在历史准入树 out/rpa-r2-direct-row-20261025/stopped-state.json：
用户于 2026-10-25 23:03:20（Asia/Shanghai）停止构建，stopped-state 于 23:03:54 生成；未登记或运行第三候选性能测量。
前两候选的旧正确性通过和 inconclusive 性能结论仍属于各自身份。
本次新构建与回归即使通过，也不能代替 R2 收益门或 R4 正式接受。

历史准入树的 raw、登记、回滚包与封包继续只读保留。
PD-5 报告提出未通过但定案，定案与 Stage 7-PD 退出均待 owner 批准。
已清理十四个临时目录的映射仍在
D:/Cuexis-worktree-evidence/cleanup/20261025-worktree-cleanup-7e49c2/README.md。
40k 内存第一遍 EXE 的登记原路径现在对应第二遍二进制，第一遍独立副本未确认；
保留两份原登记与 patch，不通过新构建或重跑补写旧身份。

## 5. Local verification

本次证据根为 out/consolidation-20261026-9c2e7b，五配置验证源码为 96e48e4ba644ce091d6ce1d4c7c1e1aa7b859db7。
五配置均已先 fresh configure、clean-first 完整构建；最后一批修复后，
全部再次 fresh configure 并增量构建成功。所有新产物和依赖复用路径都在主树。
候选功能 ON；PD metrics、execution diagnostics、allocation observer OFF；
STATIC 采用 Debug/Release observation OFF/ON，另加 Release SHARED observation OFF。
Release 告警即错误保持 ON，Debug 沿仓库预设；没有降低告警或测试门槛。

| Configuration | Final rerun total | Skipped | Earlier consumers retained | Failed |
| --- | --- | --- | --- | --- |
| debug | 1355 | 1 | 9 | 0 |
| release | 1355 | 1 | 9 | 0 |
| observation-debug | 587 | 0 | 3 | 0 |
| observation-release | 587 | 0 | 3 | 0 |
| shared-release | 562 | 4 | 3 | 0 |

验收采用明确的复合证据：4afe577 的 Debug/Release 完整 CTest 与补充配置聚焦日志，
加上最终源码的五组构建和回归；不宣称最后源码 SHA 重新执行过所有嵌套消费者构建。
Debug/Release 最终重跑除 9 项已通过消费者/staging 门外的完整适用 CTest；
补充配置重跑完整 Runtime、Playback、allocation、execution host、host protocol、RPA、
三个适用 PD probes、Hash/Packed、资产与 CXC 套件，加架构、离线工具和 SHARED export/import，
仅保留此前 3 项已通过消费者的原结果。没有用模糊 case-name 子串替代完整受影响 executable。

consumer-baseline-reuse.json 保存各旧命令/日志哈希、逐项 Passed 名称和 SDK 库原字节；
consumer-reuse-validation.json 严格核对最终变化仅限测试探针及离线 CXC 工具，
没有 SDK/消费者源输入改变，五配置共 157 个 SDK/支持库文件哈希完全相同。
所有旧失败项均包含在最终重新执行清单，失败不作沿用；每个最终选中用例逐项核对 Passed/Skipped。
STATIC 三项与 SHARED 六项消费者/导出/import 门的适用证据在 completion-summary.json 逐项列明。
跳过的实际名称与原因保留在日志及桌面 JSON，不算执行通过；没有 Not Run/失败项。
原生 Python 的九套离线合同测试最终共 178 tests 通过，适用 CTest 中也覆盖；
Windows 专属套件只在 Windows x64 注册，其他五套纯解析/合同测试跨平台注册。

标准 CTest 包含既有 Foundation 40000 实体容量等用例，不是新 PD-5/RPA 40k 采数。
本次未启动新的 PD-5/RPA 性能窗口、WPR/ETW、设备/GPU smoke，
未运行 Linux/MinGW/hosted CI 或独立尺寸审计，不扩大任何阶段/性能接受。

全仓格式、文档及 diff 检查均另有记录。
文档 checker 的 active-target 契约对应默认 candidate OFF；本轮 debug 是 candidate ON，
初次直接检查因此出现事实源配置不匹配。采用主树单独默认 configure 的真实生成事实，
通过官方 CUEXIS_TARGETS_FILE 输入核验，没有改写 BUILDING.md 的默认契约。
该默认目录只配置、未构建，不冒充第六组编译通过。

verification-results.jsonl 保留所有初次失败、诊断及最终成功命令/exit/日志哈希。
source-verification-manifest.json 绑定全部受控非 docs 文件原字节；
测试后的最终提交仅更新文档，最终文档 HEAD 与实测源码 SHA 分开记录。

## 6. Handoff and rollback

两份桌面接手材料保留原文件名作为兼容入口，内容与现场 JSON 更新到本次整合身份。
当前实施、构建与下一步命令只指向主工作树；历史准入路径仅供读证据。

原桌面文件和源码位于本报告 §2 的完整备份；本地备份分支为
codex/backup-stage7-20261026-9c2e7b 与 codex/backup-admission-20261026-9c2e7b。
回退须核对目标身份，不在合并后的现场直接反向应用旧 R0/R1/R2 局部补丁，
也不执行无范围的 reset/clean。
