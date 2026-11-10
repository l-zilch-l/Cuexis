# PD-5 正式测量前工具修复与方案准备

日期：2026-10-23。范围：测量前工具、故障验证与方案；没有启动 ETW、Cuexis、F1、40k、pilot 或正式窗口，没有正式 preparation/registration。主树 F1 不变；这不是 PD-5 或 Stage 7-PD 退出报告。

后续声明：本记录描述的是**该批**只含两项空载 canary 的状态（36 项测试）。同一日的后续工作已把 F1 collector 并入同一封包，并由管理员在 r2、r3 各真实运行一次、以 r4 修复缺陷重新封包（121 项测试）；那部分状态与证据以[F1 工具包合并封包与三用例验收准备](../etw-tools/2026-10-23-f1-tool-package-and-acceptance-preparation.md)与 `docs/CURRENT_STATUS.md` 为准，本记录的数字不回填。

## 本批交付与状态

在隔离树 `D:/Cuexis-pd5-40k-admission` 的 `3e5837f63f76397429c9f7603fd7450d3ce9855d` 上新增独立[工具生命周期实现](../../../../../../../tools/verification/playback_performance/etw_safety/README.md)与[封包入口](../../../../../../../tools/verification/playback_performance/prepare_etw_canary.py)。历史 v1/v2/v3、失败 ETL、清理记录与后台 inventory 全部保留。当前实现只交付两项空载 ETW 工具 canary，没有接入 F1 collector；旧 v3 仍 HOLD。

| 交付 | 状态与证据边界 |
| --- | --- |
| 正常停止与异常回收的单一控制权 | 已实现、故障注入及独立源码复审；真实管理员 canary 待执行 |
| Windows Job、PID+creation identity、预执行 gate | 8 项实际 Windows 良性 Python 进程验证通过；没有 ETW 或 Cuexis 调用 |
| take-over/release 顺序 | 12 项注入验证通过；控制进程死亡、Job zero、精确 absence 均不可跳过 |
| helper 超时、一次性 STOP、ETW identity/Access Denied、失败回执 | 16 项注入验证通过；失败不会变成 idle 或重复提交 STOP |
| 读取候选 | 只有 ordered immediate visitor 一项方案，未实施、未计时、未登记 |
| 内存与 P0–P8 清单 | 已明确归因方法、已知配置、缺项和后续准备顺序；未填造预算或结果 |

验证命令：`python -B -m unittest discover -s tests/pd -p 'pd5_etw*_tests.py' -v`，36 项通过。三个无上下文 Luna 分别承担 Windows 原语与故障测试、独立工具复审、方案与来源绑定；主代理复核源码、运行验证并负责最终裁决。没有 C++ 产品改动，因此本批没有构建或 CTest；这些 Python 测试不证明 Windows ETW 服务实际行为。

最终日志捕获曾发现一个良性子进程测试夹具竞态：PID 文件创建后尚未写完就被读取。该次 35 pass / 1 error 日志保存在证据根的 `tests-final.stderr.log`，没有替换。夹具已改为 pending 文件写入并关闭后原子 rename，独立窄范围复审通过；随后最终 36 项全部通过。这不是性能样本，也没有通过重跑接受产品性能。

本批当前令牌仍非管理员，不能在此完成真实 ETW canary。此前用户提供的 39-session 管理员 inventory **已经验证当时 elevated QueryAllTracesW 可用**；未验证的是新修复流程，不能再把它写成“管理员查询能力从未验证”。

## 修复如何约束停止与失败

控制进程只发正常 WPR start/stop，两者都经过同一个具备 KILL_ON_CLOSE 的新命名 Job。看护进程及外层 harness 保留 Job 和精确进程对象的句柄。WPR start 前，控制进程必须收到 harness 对 controller/guardian PID、creation time、Job 与 claim hash 的完整确认。

看护接管先终止并等待精确 controller，再终止 Job 并确认 active=0，随后才允许受限 API helper 对本次新 nonce 的两个精确 logger 名称停止。helper 自身重查 controller 已死、Job 已空。看护故障时，harness 也须先确认 controller 和 guardian 都已退出，再做失败回收。恢复成功不把失败用例改成通过。

每个 case 最多创建一个 raw STOP helper。提交前写 create-new authority；只有调用方等待 helper 确实退出后才写 exit receipt。helper 未回收或调用方崩溃留下未完成 receipt 时，后继只能查询，不能再提交 STOP，也不能仅凭 session 暂时 absent 声称完整清理。查询失败、Access Denied、不完整清单、名称大小写异常、未知 suffix 或非本次目录的非空 log path 都不能当成 absence。

实际停止归属来自新 nonce、封存 claim 与精确名称。WPR 临时 basename 未知，非空路径只作 trace-temp **目录**佐证；空路径不证明所有权。四项已核验的 DiagTrack name/empty-path tuples 继续保留为已记录后台，不代表安静环境或停止权限。没有全局 cancel、未知 logger stop、电源策略或 affinity 修改。

工具 canary 固定两项：空载 Power trace 正常停止；新空载 trace 加一个有界良性 Python sleeper，controller 在提交 stop 前直接退出，由 guardian 回收。第二项刻意保留 raw-only，不要求正常合并 ETL。各项只做一次，首败停止，不补样。工具的 20 秒 active 检查、120 秒控制/看护期限、135 秒外层监督与最多 21 秒失败回收储备、128 MiB 周期检查阈值，均不是 PD 产品预算；细节见工具 README。

边界：预 GO 的 stdlib-only Python gate 可能在 Job 外异步响应 EOF；它收不到 GO 就不能执行 WPR。Job zero 不被描述为所有惰性 bootstrap 同步消失。两个 canary 不证明真实 WPR-stop timeout 或服务侧 merge wedge，也不认证未来 420 秒 F1 诊断。若 controller、guardian、harness 同时消失，Job 关闭可以结束进程，但不能保证内核 ETW logger 停止；必须保留 UNRESOLVED，不能自动重试。

## 单一读取候选的实施前方案

在现有 PresentationRead 窗口内，以固定栈上 64 项 runtime index 分块，Runtime 按 SnapshotLayout 顺序读取一个中性值并立即调用 Playback materializer。每项 Playback 资源检查完成后才读下一项；保留 first-error 顺序，camera 仍在对象之后。零对象跳过 visitor。原 scalar readObject 保留，与 visitor 共用字段读取实现。

这只可能减少重复窗口/serial 检查和 entries 查询；每对象回调、组件读取、Playback 检查依旧存在，不能预先声称加速。先验证字段、默认值、digest/golden、0/64/跨块规模、无效 index、早期资源错误优先于后续 Runtime 错误、wrong-thread/escaped-window、重入与零分配；再检查 Release codegen 和维护成本。禁止增加公共 API、owner、线程、锁、cache 或第二条物化路径。

新候选 B 从保留的 F1 切出；正式共同成本比较的 A 仍是绑定的 pre-PD `d344088236ae589c0b49a4994ef0fbddd6567ce2`。F1 只作回滚和实现参考，不另登记第二候选。C1 已撤回、C2 不保留，不能借本方案继续轮换候选。候选失败就记录并停止本项，不搜索到通过。

用户已授权完成本批工具与方案准备；常规内部设计取舍不再额外设权限门。这里选择只完成方案，后续实施仍应清楚记录实际范围与源码身份。任何正式采样都必须先有新的准备、范围核对、一次 pilot、注册及独立 claim/seal 检查，不能把本工具封包当作正式测量准备。

## 内存采集与范围缺项

原 40k low/high 已各一次完成且独立核验，继续保留，不自动重跑。`global_cpp_new_envelope` exceeded=true 是测试诊断包络超限，既不能抹去，也不能凭重叠的全进程数字直接裁决某生产子域。小规模最终释放回到 entry baseline 仍不替代 40k 分域与最终释放证明。原合同归属见[Stage C 报告](../admission/stage-c/2026-10-23-stage-c-independent-acceptance-and-exit-gaps.md)。

| 域 | 现有可用信息 | 下次准备必须补齐 |
| --- | --- | --- |
| Content/prepare | Packed/hash 及已接受 R4 ceilings | decoded/prepared 当前量、阶段峰值、持有/释放关系 |
| Transaction/state | worker/session 与 owning GameplayResult、decision.result | worker 中的 allocation domain、current/candidate/scratch 重叠峰值 |
| Delivery | testOnly consumer/payload/attempt/history 配置 | shared backing 单次计费、lease/retired live 与年龄 |
| Registration/worker | 有界 control/step/frame/feedback 队列配置 | resident bytes、reservation、队列清空与 owner 回收 |
| Observation | testOnly ledger 与既有 tight-retirement 证据 | 本地 reference profile 的字节/记录/年龄上限与真实峰值 |
| Device/reclamation | 现有 40k 为 CPU test renderer/audio 路径 | 适用设备的在途 IDs/bytes、fence 和 owner 回收；不可填零冒充 |
| Fixture/external | 原 after-close 仍有 owning 测试值 | 全部 locals/leases 销毁后的最终 checkpoint，与产品分开归因 |
| Instrumentation/unclassified | 现 observer 覆盖/排除范围已知 | tag/header/ID 开销、未分类量与 aggregate 精确对账 |

只设计 compile-time test instrumentation：allocation ID 记原域，跨线程释放扣回原域；shared backing 只算一次，holders 单列；独立 copy 分开算；阶段 peak 从阶段入口 current 初始化，不能用累计峰值相减。最终 checkpoint 在 Host close/reset、所有合法局部值/lease 释放、worker join、owner 回收及适用 fence 完成之后。仪器开销另作 on/off 诊断，插桩时间不能验收正常分配器延迟。

表中缺的本机有限 reference profile 上限必须显式标缺，不推导生产 MiB 值。PD-5 只验收已接受的本机范围，不把整个 RPA/生产预算关闭加作本批前置。新的 40k 内存或正常分配器 Seek 工作必须单独列身份、端点、运行数和目的，不覆盖现有两项功能样本。

## 新正式准备的清单与顺序

| 单元 | 仍需准备/验证的内容 |
| --- | --- |
| P0 | 单候选语义/codegen 后的新 A/B source/build/content 绑定、固定长块和噪声检查；旧窗口3停止 |
| P1/P2/P3 | no-consumer、8:1、1:1 的完整时长、容量与年龄；request/ready/submit/present-return/GPU 端点分别记录 |
| P4 | renderer 与 1/2/4 captures 的组合路径及完整有限 profile |
| P5 | 固定 pause/resume/reset/resize/close 的完整次数、完成端点及控制延迟 |
| P6 | sparse-change、topology/resource generation 子单元的 touched/copy/scratch/destruction 证据 |
| P7 | 已接受本机 renderer/audio/music/DFJK 范围、稳定设备端点和适用 fence/回收 |
| P8 | 复用既有功能样本；另列正常分配器 Seek、分域内存与 decoded Snapshot restore 等缺项 |

manifest 逐项写输入 hash、timeline、viewport、U/F/R、entity/requirement/binding 数、consumer request policy、所有容量/retention/deadline、时长/次数/EOF、精确端点、source/build/toolchain/options；未知项写 missing。对实际变化模块列 Debug/Release、MSVC/MinGW、static/shared、candidate ON/OFF、headless、Player/reference-host、clean installed consumer 的 required/N/A-reason/not-run 状态。最终 SHA 的平台构建证据不当作本机计时。

顺序：候选与采集方法验证 → 完整有限 profile/输入/工具清单 → 新正常分配器 build 身份 → 新 pilot（一次）→ 新注册及 claim/seal 独立核查 → 预声明三窗。保留三窗每窗三对、至少 600 秒间隔、固定 AB/BA、30 秒 warmup、逐窗/首六/全部九进程 15% 检查，以及共同成本配对 95% 区间的 1.10 判定。七项物理预算、四项 hash ceilings 和 runtime reply deadline 30 秒均不变，详见[已接受方案](../../../../../../stage_plans/active/stage-07/04-pd/pd-5/pd5-measurement-proposal.md)。

ETW canary 只约束可选 ETW 归因工具；正式正常分配器 profile 本来就无 ETW，不能让可选采集器成为所有正式准备的前置。大型公开停止接口仍暂停。主 F1、old golden、C2、历史失败/中断证据保护；没有 merge/push，Stage 7-PD、整个 RPA、生产与 Stage 7 均未在本批关闭。

## 原始产物入口

本批证据根为 `out/pd5-preformal-tooling-20261023-3e5837f`；新工具封包为 `out/pd5-etw-tool-canary-20261023-3e5837f`，两者都属于隔离树。封包前后 source/测试日志、审查 hash 与保护审计记录写入证据根。

最终 `verification-0aafc06351da4f9cb9a148a9a78b078e/verification.json`（SHA-256 `195c20a85aa37957fc54f9b35aa37633246efbd17df57af7a036918645d6e5e1`）记录 36 项测试、13 项最终审查来源匹配、封包 source/executable hashes、只读 help、篡改副本拒绝、PowerShell 语法解析，以及原 Stage C 53 项文件集合/字节数/SHA 全部相同。封包 binding SHA-256 为 `818c9400b043ff1c180b05708988c96fe98a09372c280f0d4d71fbdfdebf61cf`；launcher 为 `117152c3ba887829b7a28f93fb4f004dd7b0d7335532f7c0e110754d78059ca4`。未执行 launcher。

原冻结审计本批再次通过：47 roots / 42,745 files；主树仍为干净 `eed36648a692333e47f798aa45be36ca850d08de`。独立主树/C2 审计通过，4 个历史 roots / 1,184 文件不变。这些新审计不豁免历史 `f30b2c6` 输入 drift 失败，也没有重写任何旧组。当前变更留在隔离树，未提交、merge 或 push。

保留无上下文 agent 原文：`out/pd5-tool-repair-review-20261023-3e5837f`、`out/pd5-tool-tests-20261023-3e5837f`、`out/pd5-readiness-plan-20261023-3e5837f`。其初稿包含更强的建议门及旧 v3 命令，不作为新的权限要求或当前运行指令；后续 parent corrections 和本报告明确实际交付边界。旧 binding 的手抄 SHA 错误原文不改，新来源复算仍保存。
