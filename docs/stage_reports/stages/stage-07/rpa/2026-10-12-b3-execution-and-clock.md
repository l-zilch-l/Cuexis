# Stage 7 B3 执行与时钟实施记录

日期：2026-10-12；状态：B3 本地实施与适用检查完成（testOnly）；Stage 7 仍 active。

源码起点：stage-7 / `8f18394`；工作树有未提交实施变更。
SDK API `0.8.0`，日期构建 `26.10.12-2`。源码清单 `out/b3-source-manifest.json` / tracked patch `out/b3-source.diff` 固定本轮未提交源码。
modified-source SHA-256：`9a31655c0fb206b66d04a5ed1df5d65b38e1556249c8a1d8c1a4a6112a33f193`。
digest 为按路径排序的 `path SPACE file-SHA256 LF` UTF-8 字节的 SHA-256。
清单同时登记新增未跟踪源文件的路径与 SHA-256；该 digest 排除报告自身与其他 Markdown。
GNU 16.1.0 / x64-mingw-static，Debug、Release warnings-as-errors、shared Debug 和 candidate OFF。
命令日志位于 out/，不是已提交的同 SHA hosted 验收产物。
没有 commit、push、PR 或发行行为。

## 实施与批次

| 批次 | 实施 |
| --- | --- |
| B3.1 | occurrence/control typed carriers、新 capability/profile registry、rev3/schema2、字段顺序 wire 表与 occurrence golden；旧 profile 保持 rev2；新增原因有限公共投影 |
| B3.2 | 单内核 T2/O1/I1/C1/L1、一次完整批次准入、G1/Q1/K1、缺片与连续 ack、request terminal/retirement；完整结果与 source-scan 独立 oracle |
| B3.3 | q inclusive termination、独立 ControlDrain 标记、R2 immediate/deferred/quarantine、same-q Fold、重复恢复、完整 Replay/Snapshot/Seek、A1 与实际 Fold fault 分界 |
| B3.4 | checked rational coherent observation、H=P-A/obs=P-A-I/T=P-A+V、一次舍入、失效段锁定、显式参数化 EOF tail/pause rebase；新路径没有 hStep |
| B3.5 | PlaybackSession 显式首用、安装树独立消费者、owning 值、真实 steady_clock H/frontier 分项测量 |

生产源文件按职责命名 execution_scheduler、execution_log、realtime_clock；新增 rpa_ 文件仅在 tests/。
旧文档 rpa-execution-contract.md 是已有权威路径。Chart/CXC/CXT 字段保持原合同。
分块日志普通追加最多复制 active tail；显式 archive/snapshot 展平历史。

## 本地验证

子代理复核后的最终 B3 标签 33/33 通过；另有公共首用 case 在完整 Playback executable 中通过。

| 最终检查 | 结果 / 日志 |
| --- | --- |
| Debug 完整 Judgement executable | 207 cases / 35292 assertions，通过；out/b3-review-debug-judgement.log |
| Debug 完整 Playback executable | 152 cases / 26743 assertions，通过；out/b3-review-debug-playback.log |
| Debug 完整 RPA executable | B1/B2/B3 共50 cases / 20658 assertions，通过；out/b3-review-debug-rpa.log |
| Release 同三个完整 executable | 同409 cases 全部通过；out/b3-review-release-*.log；warnings-as-errors 构建通过 |
| 静态安装树首用 / owning consumer | 通过；out/b3-review-debug-installed.log，包含旧 profile 和显式新 profile |
| shared 基础包、架构、诊断、安装消费者 | 六项通过；out/b3-shared-ctest.log；最终更新后的首用再跑通过 out/b3-final-shared-installed.log |
| candidate OFF | fresh/clean-first build、纯 ASCII 基础安装门禁通过；Reference Host 与诊断/架构复跑5/5；最终代码重建与诊断/架构4/4通过，out/b3-final-off-gates.log |
| 文档 / 版本 / 空白 / 格式 | check_docs.py、check_gameplay_diagnostic_formalization.py --self-test、update_version.py --check、git diff --check 与修改 C++ clang-format 检查通过 |

本轮还运行了**未过滤** Debug/Release CTest：初始各963项，Release报告0失败；
两者均有一项 Windows symlink 权限条件 Skip。Debug 初跑只有安装消费者请求旧 SDK minor
失败；升级该默认值后，包含对应消费者、实际新 profile 安装消费者和诊断/架构的复跑6/6通过
（out/b3-final-debug-gates.log）。新增边界 case 后注册总数为969；最终版本使用上述完整
受影响 executable / B3 标签 / 安装门禁复验，不宣称最后969项已再次全量执行。
子代理复核新增3项 case 后注册总数为972；最终复验为409项完整相关 executable、
33项 B3 和适用安装/架构/诊断门禁，不宣称972项再次全量通过。
旧 CFU performance wrapper 的 Passed 不作为新的生产性能预算证据。

发现并修复了 SDK minor 升级后的 Reference Host 拒绝期望、Playback-only consumer 默认版本，
以及 Windows shared gate 的 MSVC/MinGW runtime-import 配对；没有调大门禁 timeout。
B2 lifetime counter 改由独立 allocation 拥有，保留统计语义并避开 GCC16 嵌套 make_shared
销毁分析的 array-bounds 误诊；Release 保持 -Werror，没有屏蔽该诊断。

新增尾段终点 case 验证 fixedRate=2 也能到达奇数终止 H；轮询越过终点冻结最终位置。
EOF 后先推进未到 activation 的真实时间，随后经 prefix 到最终 H，执行全部剩余 timer，
完整 Replay 相等。EOF publication identity 保留；不存在 EOF 一次性提前判 Miss。

故障注入分别覆盖 beforeSeal 与 beforeFold：前者保留实际 paused kernel 并终止宿主直接续跑，
后者保留 3 个 sealed facts 与 1 个 folded cursor。注入 fault 的完整 Replay
使用明确故障注入参数验证；它不冒充可从普通 archive 自动再现的规则配置故障。
故障公共 Snapshot 继续拒绝；内部 capture DTO 用于证明 fault prefix 的恢复能力。

## 子代理复核与修正

三名子代理分别做规范、合同和证据审查。规范审查发现公共 Suspended 状态同步遗漏，
合同审查发现 Resume 先改时钟后验 context、恢复后低于 ControlDrain 的 H 进入内核、
实际 A/I 未绑定既有校准身份。以上均修复，并经两个审查代理只读二次复核。
调度器初始化同时改为 Result 工厂，不以构造异常跨模块报告无效配置。

新增反例证明：错误 Resume scope/epoch 不占新 segment；成功 Resume 的冲突重传
显示 Suspended；后续时钟失败保持 ResumeAborted；H 未追赶时不接纳/消费 ID 或 ordinal；
实际 A/I/rate 与 mapping revision 通过约分身份绑定，V 不影响身份；不同校准拒绝
Snapshot restore 与 Replay Seek 导入。Replay 自包含 evaluation 保持其既有语义。

Debug、Release、shared Debug、candidate OFF 均重新构建。
out/b3-review-targeted.log 记录33/33；静态/shared新 profile安装消费者分别通过，
见 out/b3-review-debug-installed.log / out/b3-review-shared-installed.log；
candidate OFF 诊断/架构4/4通过，见 out/b3-review-off-gates.log。

## 时间测量与限制

`[b3][measurement]` 从 std::chrono::steady_clock 的真实 elapsed 得到 H，32 次普通推进后
分别记录 H-lastAdvanceHorizon 与 F(H)-processedFrontier 的 p50/p95/p99/max。
`out/b3-clock-measurements.json` 保存首次验收的独立 probe；该次 Release probe 的 H lag / frontier lag 分别为
`[63, 109, 172, 172]` / `[63, 109, 172, 172]` us
（p50/p95/p99/max，32 samples）。两者本次相等是普通成功推进的实测结果，仍分别计算。
子代理修正后 Release 复验为 `[79,133,155,155]` / `[79,133,155,155]` us，
记录在 out/b3-review-release-rpa.log；真实测量随运行变化，未覆盖此前 JSON。
probe 的 sample time 和 presented position 每次来自同一真实 steady_clock，未重贴旧观察时间戳。
这是微型 testOnly 内容与 profile，不接受 production latency/容量/误差预算。

B4/B5 owner/资源/设备/Player 线程部署和 F1-F4 同 SHA hosted、设备、预算、owner 接受仍待执行。
EOF tail 时间桥已验证；Player/audio 的 tail 输入截止与实际音频切换属于 B5。
本地 B3 不关闭 Stage 7，也不证明实体听音/真实按键或发行。

API 首用见 [execution coordination](../../../../api/execution-coordination.md)，
wire 见 [revision3](../../../../formats/execution-recovery-wire-v3.md)，
实施范围见 [B3 计划](../../../../stage_plans/active/stage-07/plan-rpa.md)。
