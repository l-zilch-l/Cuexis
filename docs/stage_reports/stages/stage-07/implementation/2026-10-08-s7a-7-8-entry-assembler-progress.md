# S7A-7/8 Graph、Entry 与 assembler 集成证据

状态：dated implementation evidence；受限验收进行中，未提交/推送

证据日期：2026-10-08

## 基线与边界

实际分支 stage-7，HEAD 仍为 55fc8e6920e7ddb4b34a56bd0b9c27d07d60b833，现有 PR #32。
本报告验证的是该 HEAD 上未提交的工作区，不能把 baseline SHA 或此前 CI 记作新增行为实现 SHA。
原规划/拆分/审查文档保留；未 reset、另建 PR、owner approval、bootstrap、修改保护、合并或发行。
前一日期证据见 [10-07推进记录](2026-10-07-s7a-7-8-integration-progress.md)，
当前状态只归 [CURRENT_STATUS](../../../../CURRENT_STATUS.md)。

## 本轮合同与实现

| 首用行 | 合同版本/实现 | 本轮证据 | 未退出范围 |
| --- | --- | --- | --- |
| C78-05 / R78-08 | [Graph v1](../../../../formats/GAMEPLAY_GRAPH_V1_FORMAT.md)，Capsule 3；专用 bounded typed SAX Reader，直接 typed Writer，整数不经 f64；shared row visitor/link/validate/preimage/reconstruct | Graph/Packed 完整 Capsule 重编码字节一致；header 先拒绝；重复/count/行预算；payload 表顺序置换；Graph 4 cases/163 assertions | 最终平台、integer-extreme Graph 公共路径、生产预算不在本轮接受 |
| C78-01/02/08 | [ABI configuration 首用](../../../../api/GAMEPLAY_V2_ABI.md)，config JSON v1、显式 decode budgets；Graph public source 沿同一 actual prepare/kernel/Fold/recovery | i64/u64 extrema、2^53+1、所有必需字段、未知枚举/重复/缺键/预算；codec 输出实际 Packed prepare | 本行整体故障矩阵、最终安装树/平台仍待回填 |
| C78-05/09 | [Entry 首用](../../../../formats/CHART_ENTRY_V1_FORMAT.md)、[CXC 物理闭包](../../../../formats/CXC_FORMAT.md)、[Playback factories](../../../../api/playback-session.md)；七项完整 metadata，project 严格 entries 目录；CXC 仅核验物理 path/exact hash | typed/CXC file/CXC memory/捕获 generation 内 filesystem，各走一次实际 source→prepare→kernel/Fold→完整 Replay；七项缺失、假 closure/hash/profile/count/path/encoding/sourceOf 拒绝 | adopted 捕获属于 caller；完整 group/表现缺项、宿主消费仍待回填 |
| C78-06 / R78-09a | Gameplay author 原 compiler、固定实际 gameplay.author.t4-k4.v1 profile；生产 cuexis_gameplay_assemble CLI 同次编译输出 Graph/Packed，完整 fallback 资源；发布前两载荷、两 intent actual prepare+commit | 13 次实际进程；inline/CXT prototype、2x3、重排输入完整字节/metadata 比较；deterministic exact CXC；非法 author 不替换旧输出；CXC 原 atomic；generation 单 adopted；采纳前/后 failure、commitVisible=true、旧树保持 | 最终 static/shared/ON-OFF/平台、安装 Host/Player，容量和门禁没有整体退出 |

Graph 静态表第一次完整 CLI 比较发现原实体遍历顺序影响 Graph 字节，但 Packed 字节和 semantic SHA 相同。
最小反例为 inline_2x3 与 inline_2x3_permuted：staticChart.entities[0..5] 的生成路径顺序不同。
修正归直接 typed Writer 的 canonical 排序（feature id、Beat、resource、entity identity bytes、component tag），
不经过 Packed/JSON 重编码，不改变原 3/4/5/6 判定规则；负零有限浮点表示沿 Graph 现有合同。
普通表示决策写回 plan-a，不把推荐登记当合同退出。

code-quality 只读复审发现 configuration 和 Entry directory 两处 parser budget 映射不正确：
bounded SAX 实际返回 json.parse.*_limit，不能以是否含 budget 子串判断。
两处已按确切五个码映射 capability.budget_insufficient/budget_exceeded，保留 sourceCode/severity/faulted；
测试增加 code/category/sourceCode 断言。复审未独立执行测试，不能以审查代替运行证据。

## 命令与原始输出

所有路径相对仓库，原始中间失败保留。日期构建号/SDK 尚未更新；实现 SHA 待实际提交后补。

| 命令 | 原始输出 | 结果/限制 |
| --- | --- | --- |
| vcvars64 → cmake --preset candidate-debug → build --clean-first；playback tests [gameplay]，packed tests [graph] | out/s7a78-graph-public-clean.log | 早期 Graph 4/163、Gameplay 18/981；不能证明后续 Entry/CLI |
| build；playback tests [gameplay] | out/s7a78-configuration-tests.log | config 初版 19/1039；后续 budget mapping 复验包含在下一行 |
| build；playback tests [gameplay] | out/s7a78-entry-tests-rerun.log | metadata 20/1119 |
| build；playback tests [gameplay] | out/s7a78-cxc-entry-closure-tests.log | CXC/fs 21/1246；随后 entries directory/预算回归增加 |
| build；playback tests [gameplay] 和 [.candidate-gameplay-fixture] | out/s7a78-assembler-fixture-tests-rerun.log | 21/1254；hidden fixture 1/17；配置 1528 bytes/Foundation 561 bytes/Packed 3082 bytes，仅 fixture 测量 |
| build --clean-first；packed tests [graph]；playback tests [gameplay]；CLI Python matrix | out/s7a78-assembler-cli-clean-matrix.log | 全构建、4/163 与21/1254通过；CLI前11项通过，第12项用了错误的失败hook，不能称13次通过 |
| build；python -B tools/check_gameplay_assembler_tests.py --tool out/build/candidate-debug/bin/cuexis_gameplay_assemble.exe --fixture out/build/candidate-debug/s7a78-public-fixture --log-dir out/s7a78-assembler-cli-final | out/s7a78-assembler-cli-final-matrix.log，子目录01–13原始命令/stdout/stderr/exit | 13 次全部满足正负预期；完整 carrier artifacts 独立 zipfile/JSON/hash 比较；无调用生产推进函数伪装 oracle |

新增 source/config/Entry 测试不代替已有手写 timing/score golden、scanReference 独立 kernel oracle 和完整
sameKernelResult/ReplayEvaluation 测试；这些仍保留。当前 CLI 测试证明生产装配与发布，不声称设备输入 oracle。
MSVC 中文 showIncludes 依赖跟踪既有问题使内部 header 修改未触发增量编译；最终 Writer 证据采用 clean-first。
早期增量测试因此不证明排序修复，原输出 out/s7a78-graph-order-tests.log 保留；未修改 CI/工具全局策略。
CXC 初次被 cxc.entry.unlisted 拒绝，详细诊断在 out/s7a78-cxc-entry-diagnostics.log；按所属合同
注册 Gameplay 物理闭包后通过，未放宽一般额外文件检查。

## 未执行和门禁

本轮新增 Graph/Entry/生产 CLI 的 Release、ON/OFF headless、static/shared 安装、MinGW、Linux
与架构/package/ASCII/诊断/docs/version 最终验证尚未完成；旧矩阵不能证明这些新行为。
Player 真实离散输入、Reference Host Gameplay 六动词+tick、完整 registry 四态、group/partial-group
及剩余恢复/故障矩阵仍需实施与验收。GPU/window/audio/真实键盘/其他设备证据单独保留未执行。
SDK API patch/minor 差异证明、日期构建号更新后的 fresh/clean consumer 与新 SHA hosted 待回填。
Version advancement trigger 修复仍在原工作区，新 hosted 启动证据只能在推送后回填。
S7A-8.4 owner-only approval/可信基线 bootstrap/保护/hosted 未退出，不妨碍其余授权工作。
I78-6 只累计上述计数与输入；不接受生产阈值、不关闭 S7A-9/7A、不进入 7B+/7C 或 Stage 8 发行。


## Reference Host 与 Player 实際适配增量（2026-10-08）

本节实现仍为 `55fc8e6920e7ddb4b34a56bd0b9c27d07d60b833` 上的未提交工作树，不能用旧 SHA CI
证明行为。合同版本为本文当日 Playback API 宿主/Player 首用补充；未登记 owner approval。

- P78-05 A 已回到既有 `cuexis_chart_candidate --gameplay` 入口；独立工具名只是同一函数别名。
  实际入口 13 次测试通过：`out/s7a78-assembler-existing-cli-matrix.log` 和逐次原始输出目录。
  Producer 按 typed mainMusic 选择既有 HostClock/ChartClock 内容模式；有音乐正例仍待补。
- Reference Host 使用干净安装树 `out/s7a78-host-prefix`，configure/build 原始输出为
  `out/s7a78-host-build.log`（首编译 receipt 类型错误保留）和修复后的
  `out/s7a78-host-rebuild.log`。没有内部 execution include。
- 实际 six verbs + tick 序列 `open/play/tick4/pause/tick2/seek1000/reload/play/tick4/quit`：
  H 每步 250、T 每步 7，观察 Tick900，原始设备时间戳 100/200/300/400 仅 provenance。
  `out/s7a78-host-fixture/hit.log` 人工 golden score2/combo1/hits1/misses0；
  `miss.log` 零输入 score-1/combo0/hits0/misses1；`graph.log` 与 Packed Hit 相同。
  每步记录完整 kernel/Fold/diagnostic Replay 比较、暂停抑制两 Tick、Seek cut 和重载后相同 golden。
  JSON 配置 domains.maximum=2000 是显式 testOnly fixture 范围，不是生产业务预算。
  较早 `host.log` H20 尚未进入 author 850–920 窗口，score0 不能当 Hit golden。
- Player 公共 Gameplay 配置/typed 采样桥、真实 SDL scancode press/release、repeat 过滤、映射键
  隔离控制键、focus loss 保守批次隔离、公共 control/Replay/cut typed Seek、score 输出已接线。
  `out/s7a78-player-focus-tests.log` 2 cases/49 assertions 通过，包含真实应用 frame loop 的
  synthetic surface 最小反例：Playing focusLost+旧Space 不恢复，Paused press+Resume 不补交。
  独立人工 Hit/Stop 后零输入 Miss 也通过。该 testOnly 每帧 H/T 桥不是设备校准证明。
- code-quality 子代理复审发现两项实际 Player 缺陷并修正：上述聚合队列排序缺口，以及 audio
  control 失败前已变更 Gameplay。现在 audio 拒绝不变更 Gameplay；其已成功而 Gameplay 失败时
  明确标记应用 Failed。后者 fake audio 故障 fixture、实际设备验证尚未执行，不能只靠源码关闭。

以上没有退出 C78-07/10 group/registry 闭包，没有替代最终 Debug/Release、ON/OFF、static/shared、
MinGW/Linux、架构/安装/ASCII/诊断/版本矩阵。GPU/window/audio/真实设备、fresh version consumer、
本轮最终提交 SHA、hosted workflow 触发与 owner/可信 bootstrap 仍需逐项回填；S7A-8.4 未退出。

## 聚合、查询与完整 Hold 增量

合同版本：Spec §3.23 2026-10-08 聚合/alias 补充、ABI 2026-10-08 capability 四态与 presentation map、
Playback API 2026-10-08 宿主/Player 首用。代码仍为基线上的未提交工作树，最终 SHA 随提交记录。

- Any/All/GroupCommit 与 canonical group members、alias identity 校验已消费；独立 mutation generation
  与 owning projectionScope/sourceMap。group golden 含缺目标 partial-group、当前资源恢复、到期清除、
  all 未满足、member 缺失、missing-only 到 later-valid 反例和正逆 alias 输入拒绝。
- 复审的两个 P2 最小反例：仅缺目标早期区间 [0,10) 不应拒绝后来 [20,30) 的有效目标；相同 ID
  不同聚合或 group 身份不能由 find-first 遍历选语义。修复分别限制拒绝只在实际 partial commit，
  校验所有 aliases 并加正逆序负例。原输出 out/s7a78-group-missing-only-rerun.log 为23/1373；
  最新 alias 由最终矩阵复验，不能以这份旧输出证明。
- capability query 在实际 commit 前 insufficient，已提交完整 canonical 配置匹配 available；
  unknown/revision/disabled、changed pending config、paused available、query 不使 prepared stale 均测试。
- 新 complete Hold 测试使用原 Head/Body/Tail/release/preparedGrace 规则，不重开 S7A-3/4；
  owning Graph/Packed →实际prepare/commit/submit/advance；独立 scanReference 不调用生产推进，
  比较 facts/phases/resources/contacts/ownership/observers/receipts。人工 Fold golden score10/combo3/hits3，
  完整 ReplayEvaluation、pending snapshot 恢复、exact checkpoint/cut Seek 逐项完整结果相同。
  out/s7a78-hold-oracle-final.log：1case/73assertions；两次测试 API 拼写错误原输出保留。
- fake audio 控制拒绝实际 PlayerController：failed Play 不改变 Paused、owning完整结果与H0，Replay完整；
  和 focusLost/resume、人工 Hit/Stop→Miss 共3case/75assertions，out/s7a78-audio-failure-rerun.log。
  初次 music fixture resourceClosure 缺 mainMusic 被正确拒绝，out/s7a78-audio-failure.log 保留；
  修复 typed fixture 自身完整 closure，没有放宽 Reader。
- tools/check_gameplay_assembler_tests.py 14次，包括独立 zip/hash 完整音乐闭包与两载荷/两intent实际prepare；
  out/s7a78-cli-music.log 和 out/s7a78-cli-music/01–14.log。无新音频设备接受。
- tools/check_gameplay_host_tests.py 14实际进程，四载体/编码×Hit/Miss、六动词+tick、Paused两Tick抑制、
  exact Seek/reload、完整Replay、非整数/重复sequence负例；out/s7a78-host-matrix-rerun.log 与逐次log。
  首次脚本使用了不存在的 flag，out/s7a78-host-matrix.log 保留；修复脚本及 help 同步真实参数。
  新 CTest 从 fresh安装树编译运行 Host，不使用 private 执行图；最终 static/shared/平台结果独立记录。
- 最终只读 code-quality 复审发现 tools OFF 时无条件 target file 引用使 candidate SDK configure 失败；
  已改为工具存在才追加 Host producer 参数，安装 lifecycle 常驻。tools OFF configure 正例仍须执行。
- SDK实际差异：out/s7a78-sdk-api-diff-proof.log；按实际 production headers/原consumer差异选择保留0.7.1
  preview patch，不预先锁定minor。date identity 通过 python -B tools/update_version.py 26.10.08-1。
  新 fresh/clean-first/consumer 验证正在执行，不能用更新前旧输出或旧SHA CI替代。

I78-6 测量输入另增 group completed/rejected集合、future scheduled容量、sourceMap owning复制、Graph
bounded SAX字符/row与Host进程计数；O(N)成本尚需真实内容测量，未接受任何生产数值。

## I78-5 逐行首用实现与受限验收映射

以下每行合同版本为本轮 2026-10-07/08 所属合同补充，实施基线 55fc8e；实现 SHA 在提交后
单独登记。命令与原始输出使用下面最终矩阵；此表的代码/fixture映射不把未完成平台门禁标记通过。

| C78 | 所属合同与实施消费者 | 人工/独立/故障及完整对照 fixture | 仍保留 | 合同版本 | 实现 SHA |
| --- | --- | --- | --- | --- | --- |
| 01 | V2 ABI/Spec、execution profile；GameplayContent/PlaybackSession 显式配置与独立状态 | playback_gameplay_tests zero-input Miss、纯播放 digest、实际 build/无配置拒绝 | production budget/设备校准 | Gameplay v2 Spec/ABI、T4/K4 execution profile v1；10-08补充 | `83935f1375be` |
| 02 | Playback lifecycle/ABI；prepare Gameplay immutable candidate、组合 owner/generation commit | wrong owner/stale/重复commit、Runtime预检失败完整旧结果、故障 sealed/publishable prefix | hosted同SHA | Playback preview0.7.1、Gameplay v2 ABI；10-08补充 | `83935f1375be` |
| 03 | V2 time/profile、Playback typed H/T桥；actual submit/advance及host适配 | exact H/F(H)、future/lastObservedTick、重复序号/极值；Player整数raw timestamp、Host 250/7 | testOnly bridge非生产设备锚点 | T4/K4 execution profile v1、Gameplay v2；10-08补充 | `83935f1375be` |
| 04 | Playback control/恢复ABI；public control/Replay/Snapshot/Seek | actual Hold press/release oracle、pending snapshot、archive cut/checkpoint Seek、paused/reload、fakeaudio失败 | 实际设备discontinuity | Gameplay v2 ABI/W2恢复、Playback0.7.1；10-08补充 | `83935f1375be` |
| 05 | Graph1、Capsule3、ChartEntry1、CXC1；typed SAX/source/carrier factories | 64-bit整数/重复键/count/bounds/hash/closure负例、Graph/Packed完整结果、file/memory/fs | Chart v5正式发行独立 | Graph1、Capsule3、author profile v1；10-08补充 | `83935f1375be` |
| 06 | author/execution profile、ChartEntry/CXC；生产chart_candidate --gameplay | inline/CXT/2x3 permutation全artifact比较；14CLI，单CXC原子+多产物adopted故障/旧reader固定 | 内容生产阈值不接受 | ChartEntry1、CXC1、T4/K4 author profile v1；10-08补充 | `83935f1375be` |
| 07 | Spec §3.23/ABI、Animation mixing/Playback；resolver+group+scope+sourceMap | early/exact/late/Miss/false/Host/Studio、future/lifetime/cadence、partial-group/alias/恢复当前资源 | 大内容O(N)测量 | Gameplay v2 Spec/ABI、Animation mixing；10-08补充 | `83935f1375be` |
| 08 | V2 ABI/恢复、Playback；owning public Result/W2档案/全ReplayEvaluation | old owning寿命/篡改预算、actual Hold完整恢复、static/shared staged consumer codec | matching-toolchain shared须重建 | Gameplay v2 ABI/W2、Playback0.7.1；10-08补充 | `83935f1375be` |
| 09 | CXC/Playback/Spec；GameplayOnly CPU closure、optional Presentation | 两intent同actualKernel/Fold、缺target不改判定、Runtime失败旧frame、坏包拒绝 | GPU/window独立 | CXC1、ChartEntry1、Playback0.7.1；10-08补充 | `83935f1375be` |
| 10 | ABI/registry/诊断码表；static capability query/完整configuration identity | unknown/revision/disabled/pending insufficient/committed available、readonly generation与Paused查询 | 无新production预算/capability | Gameplay v2 ABI、registry1；10-08补充 | `83935f1375be` |
| 11 | Playback宿主补充；SDL scancode、PlayerController、installed ReferenceHost | Player focusLost/paused resume/audio拒绝人工golden；14进程矩阵（12 Host +2 assembler）四载体Hit/Miss/六动词+tick全Replay | 实际键盘/GPU/audio设备未执行 | Playback0.7.1宿主合同；10-08补充 | `83935f1375be` |
| 12 | Playback package/VERSIONING；ON条件头/宏、private link、SDK source compatibility | ON/OFF/ASCII/arch/allowlist/static/shared/package/0.7.0 source proof/toolsOFF配置 | owner审批/可信bootstrap/保护/新SHA hosted | SDK0.7.1 package、VERSIONING；10-08补充 | `83935f1375be` |

| R78 | 本轮选项与直接验收映射 | 实现 SHA |
| --- | --- | --- |
| 01 A | Result/Projection/Runtime publication receipt、faulted readonly旧frame；Runtime预检/失败注入 | `83935f1375be` |
| 02 A | successfulFold可发布cursor；Fold失败sealed Fact保留/failedTick无token；full Replay对照 | `83935f1375be` |
| 03 A | mutation generation owner/stale；readonly查询不使Prepared失效，空submit不伪mutation | `83935f1375be` |
| 04 A | transient schedule/dedup/group/sourceMap/token整体swap；exact Seek/Snapshot/currentbind重建 | `83935f1375be` |
| 05 A | 四分量不含纯表现细节；同identity恢复当前绑定/资源，旧owningmap独立存活 | `83935f1375be` |
| 06 A | GameplayOverride layer与layer内priority，Animation/Host/Studio完整golden | `83935f1375be` |
| 07 A | typed [start,end)/UntilReset；dense/sparse/skipped/cut/expiry比较 | `83935f1375be` |
| 08 A | json_support bounded SAX→typed Graph；整数不经f64，重复/结构/预算正确层失败 | `83935f1375be` |
| 09a B | filesystem immutable generation+singleadopted；CXC旧publishPackage；14CLI故障与capture | `83935f1375be` |
| 09b A | 实际production API diff与0.7.0 consumer保持；保留0.7.1 patch，fresh/clean消费者矩阵 | `83935f1375be` |
| 10a B | live section owner/anchor checker5正负测试、historical例外；386文档检查 | `83935f1375be` |
| 10b A | 独立 GameplayState/Kernel-Fold-Control faultStage；不改旧Playback state、不安装内部enum | `83935f1375be` |

| S7A | 本轮实现与验收映射 | 本轮状态口径 | 实现 SHA |
| --- | --- | --- | --- |
| 7.1 | C78-01–04/08；实际Lifecycle、owning查询/恢复、Tap/Hold golden+independent scan/fullReplay | 实施完成；受限本地功能验收完成，生产/设备/hosted独立 | `83935f1375be` |
| 7.2 | C78-05/06；Graph/Packed/Entry/typed/file/memory/capturedfs→actualprepare、fullartifact/oracle | 同上；正式Chart v5发行未接受 | `83935f1375be` |
| 7.3 | C78-09；GameplayOnly完整闭包与相同实际判定/故障边界 | 同上；GPU资源适配另列 | `83935f1375be` |
| 7.4 | C78-07；resolver层域、typed lifetime/group/sourceMap/current资源恢复 | 同上；没有第二判定路径 | `83935f1375be` |
| 7.5 | C78-08/12；candidate public package/opaque payload/private执行图不安装 | 同上；共享consumer按matching toolchain重建 | `83935f1375be` |
| 8.1 | explicit ON/OFF/wrong flavor/entry隔离，已有candidate preset/CI消费 | 本地矩阵已结算；新SHA hosted待回填 | `83935f1375be` |
| 8.2 | 完整生产assembler、静态registry/完整Entry/双carrier、deterministic atomic publish | 14CLI actualprepare/fullclosure通过，跨平台最终consumer已结算 | `83935f1375be` |
| 8.3 | installed ReferenceHost六动词+tick/四载体Hit-Miss/Replay，Player真实scancode接线 | CPU人工+synthetic/fakeaudio通过；真实设备独立未执行 | `83935f1375be` |
| 8.4 | actual SDK diff/日期/version/trustedchecker正负/CI触发修复 | 未退出：owner approval、trusted master bootstrap、保护、新SHA hosted；未代办 | `83935f1375be` |

I78-0–4 的对应实现已落地，I78-5 已按最终本地矩阵逐行结算；I78-6不做S7A-9最终关闭。


## 最终矩阵中的实际缺陷与修复

MSVC Debug 第一轮全仓1050/1050（1 symlink环境skip），新公共 focused 三flavor24/1452。
后续 registry unknown声明正逆序先报 byte-smallest capabilityId，新 focused25/1467，完整 Playback
149/12158；结果绑定本工作树，不用旧SHA hosted代证。

- SharedExport 表遗漏 candidate公开5 opaque类/3函数；只在明确candidate开关追加精确允许名字。
  同时保留 detail/RuntimeSession/World等禁止规则。原private metadata helper确实错误dllexport；
  已去导出，同一metadata CPP私有编译进Playback/工具/测试，原error projector移到private inline，
  不安装内部头或增加第二判定路径。新export gate已通过，原失败保留在final-shared-debug.log。
- MinGW test缺显式algorithm，std::ranges::any_of不可见；补头。public源不改语义。
- ReferenceHost contract只允许Playback includes；新host_gameplay.cpp多引core/error，已删除，
  从Playback header获得现有Result/Error。新的installed Host完整矩阵和staging复验。
- 旧external consumer CMake没有把candidate开关传到子package，因此构建production包并使shared
  imports期望candidate时失败。现在传明确candidate开关/suffix并按flavor校验实际库名，OFF原路径保留。
  不放宽任何错误flavor/混prefix/泄漏诊断。
- AssetDatabase TemporaryAssetRoot同per-process计数从1开始，并行case/多build互删同一目录，
  导致Linux读blob缺失及Releaseduplicate-source误诊断；改exclusive create_directory认领目录。
  Windows Debug/Release最新受影响负例全部通过；原full矩阵失败仍记录。
- WSL cached依赖模式不是vcpkg toolchain配置：nested consumers需要显式CMAKE_PREFIX_PATH环境，
  初次缺tl-expected是环境缺项；重跑传真实cacheprefix。Sanitizer安装consumer补同instrumentation flags，
  保留原instrumented库链接，不能用无sanitize consumer规避。扩展后的新安装Host门禁采用既有
  external gate的900s测试timeout；这是测试调度界，不是内容/运行生产预算。
- MinGW AudioSDL consumer首轮受并发vcpkg compiler-detection resource busy/locked拒绝；保留输出，
  后续串行同命令成功，不改下载策略或生产逻辑。

最终代码审查曾请求code-quality子代理；此前P2反馈均已修复，最后private符号修复补审尝试遇到
服务usage limit未执行，不声称代理独立审查已覆盖该增量。源码复核、编译/符号/完整回归分别记证。

## 2026-10-08 本轮受限验收结算

本节取代前述中间记录中的“尚未接线/未更新版本/最后增量未复审”描述，保留原失败和阶段性输出。
本轮实施基线为 `55fc8e6920e7ddb4b34a56bd0b9c27d07d60b833`，PR #32 base 为
`5472c463640cf3b66a03dd86fe87bd87239b2659`。下列本地命令运行于同一未提交实现工作树；
实际实现提交在本报告后续 SHA 记录中绑定，输入 blob 清单用于核验编译输入，不将旧 SHA hosted
成功写成新行为验证。日期 `26.10.08-1`、SDK `0.7.1`；日期变更后各受影响轴先 fresh configure
和 clean-first build，再对最终缺陷修复重编受影响输入并执行相应回归。

### 本地矩阵与命令

MSVC 命令由 VS Developer shell 执行，vcvars 后重新设 `VCPKG_ROOT=D:\vcpkg`。
下表日志文件名统一省略实有的 s7a78- 前缀，完整路径为 out/s7a78-<表中文件名>。
各目录实际配置保存在原始包的 CMakeCache.txt；不是所有轴均全仓重跑。下面明确区分全仓、定向
与后续缺陷修复回归，不把重跑子集换算成第二次全仓通过。

| 轴 | 实际结果与证据文件（out/） | 命令口径 |
| --- | --- | --- |
| MSVC ON Debug | 初次 1050 无失败，1 Windows symlink skip；最后完整 Playback 149 cases/12158 assertions、18/18 受影响门禁、format check 通过 | `cmake --build out/build/candidate-debug`；`bin/cuexis_playback_tests.exe`；`ctest --test-dir out/build/candidate-debug --output-on-failure -R <受影响架构/诊断/consumer/CLI/Player/AssetDB>`；`cmake --build out/build/candidate-debug --target cuexis_format_check`；final-candidate-debug-rerun.log、debug-boundary-fixed.log |
| MSVC ON Release | fresh/clean 全仓初次 1050 中 AssetDB 1 fail、symlink 1 skip；修复后 Gameplay 25/1467、14/14 受影响门禁无失败 | `cmake --build out/build/s7a78-candidate-release`；`ctest --test-dir out/build/s7a78-candidate-release --output-on-failure -R <受影响门禁>`；final-candidate-release.log、release-boundary-fixed.log |
| MSVC ON shared Debug | 初次 44 门禁 5 fail 已逐项归因；最后 Gameplay 25/1467、5/5 export/installed/CLI/Host/Player fixture 门禁、4/4 candidate imports/consumer 无失败 | `cmake --build out/build/candidate-shared-debug`；对应 `ctest --test-dir ... -R ...`；final-shared-debug.log、shared-boundary-fixed.log、shared-imports-candidate-fixed.log |
| MSVC ON headless/tools OFF | fresh/clean 30/30；最终 Gameplay 25/1467、11/11 受影响门禁通过，工具不存在时安装 consumer 不引用工具 target | candidate-headless-debug build/CTest；final-headless-tools-off.log、headless-off-boundary-final.log |
| MSVC OFF Debug/Release | Debug fresh/clean 1017 中 Host header 1 fail、symlink 1 skip；修复后 10/10 受影响门禁通过。Release fresh/clean 全构建及选择的 41/41 门禁通过 | debug/release configure fresh、clean-first build、Debug 全仓/Release label matrix；final-off-debug.log、final-off-release.log、headless-off-boundary-final.log |
| MinGW ON | fresh/clean 后缺 algorithm 编译头修复；全仓 958 中 AudioSDL consumer 1 环境 fail；最终 Gameplay 25/1467、14/14 受影响 gate（含串行 AudioSDL consumer）通过 | MinGW UCRT C++16.1、显式 compiler/toolchain、vcpkg concurrency=4；final-mingw-rerun.log、final-mingw-fixed.log、mingw-boundary-final.log |
| Linux GCC ON shared Release/headless | fresh/clean 构建；GNU exports 和 installed consumer/Host 最终 2/2，Gameplay 25/1467。第一次漏 B5cxx11 ABI tag 与 staged transitive shared library 路径，修复后通过；其他首次门禁结果见 Linux 结算 | `cmake --build out/build/wsl-s7a78-shared --clean-first -j6`；`ctest --test-dir ... -R "cuexis_shared_export_surface\|cuexis_gameplay_installed_consumer" --output-on-failure -j2`；`bin/cuexis_playback_tests "[candidate][gameplay]"`；linux-shared-final.log、linux-shared-runtime-fixed.log |
| Linux Clang ASan/UBSan actual Gameplay/JSON | 最终 actual Gameplay 25/1467，完整 JSON 25/275，无 sanitizer finding；package label matrix 结果另行结算 | `out/build/wsl-s7a78-clang-sanitize/bin/cuexis_playback_tests "[candidate][gameplay]"`、同目录 `cuexis_json_support_tests`；linux-sanitize-actual-kernel.log |
| Python/文档/version | docs 386 Markdown/20 JSON-CXT、live owner checker 5、status checker 4；version gate Windows 25（2 POSIX skip）/Linux 25（无 skip）；3 workflow Bash run block syntax；version 一致 | `python -B tools/check_docs.py`、`tools/check_docs_section_contract_tests.py`、`tools/check_docs_status_contract_tests.py`、`tools/check_version_gate_tests.py`、`tools/update_version.py --check`、`git diff --check`；final-docs-current.log、final-docs-sections.log、final-status-tests.log、final-version-tests.log、final-linux-python.log、version-gate-bash-syntax-rerun.log |

CTest 具体正则、每个 fixture 名、子构建 configure/build 全命令和输出保留在原始日志及 LastTest.log，
表中 `<受影响...>` 是范围摘要，不伪装可复制的完整命令。CLI/Host Python 入口的参数原样保存在
installed gate 日志；独立标准库 zipfile/JSON/SHA oracle、人工分数和完整结果比较由 fixture 脚本记录。
14 次 CLI 与 14 进程矩阵（12 installed Host +2 assembler）（四载体 Hit/Miss、六动词+tick）均纳入实际包消费门禁。

### 人工/独立 oracle、失败注入与真实窗口

前述 C78/R78/S7A 逐行表关联的人工 Tap early/exact/late/Miss、Hold head/body/tail 权重 2/3/5、
独立 scanReference、完整 ReplayEvaluation、owning Snapshot/Seek/cut、current binding/resource 恢复，
均进入最后 25/1467 Gameplay 回归。独立 oracle 不调用生产 advance；实时/Replay 都使用实际 kernel/Fold。
Result/Projection/Runtime 注入、Fold 封存失败无 token、group partial/missing-only、scope 全量重建、
atomic CXC/immutable filesystem adopted before/after commit 注入、Graph SAX bounds/duplicate/整数极值，
保留首轮失败、人工期望和完整比较，而非仅终分相等。

实际 GPU/window 于本机 NVIDIA RTX4060 Laptop、OpenGL3.3 NVIDIA616.64 执行：
`out/build/candidate-debug/bin/cuexis_player.exe --smoke-test` 完成 6 帧，记录像素 golden、reload、
adapter 准备失败保持旧缓存；gpu-smoke.stdout.log/stderr.log。显式 Gameplay 的零输入 Player
命令原样见 gpu-gameplay-zero.command.json，H=1000 时 score=-1/Miss=1，并持续
`completeReplay=same`（stdout 至 H=141750）。原两进程均已结束，无遗留 Player；原观察运行未捕获
进程 exit code。21:13 的补充 smoke 再次完成6帧，实际捕获 exit=0，证据为
gpu-smoke-exit.stdout.log/stderr.log 与 gpu-smoke-exit.result.json。零输入 Gameplay 仍按观察证据登记，
不将补充 smoke 的 exit=0 移用于该进程。
纯播放 smoke 与显式 Gameplay 零输入 Miss 分别验证，不能互相替代。

未执行：实体键盘离散输入/设备时钟校准、真实音频输出/丢设备/端到端同步、Gameplay FactBinding
正反馈 GPU 像素专项。SDL 适配、focusLost/暂停 resume 和 FakeSeat audio 拒绝的 CPU fixture
已通过，但不能代替上述设备项。Hosted Linux sanitizer/tidy/coverage 与三平台新 SHA CI 待推送后回填。

### 本轮代码复审与门禁边界

code-quality 指定的只读子代理补审已恢复执行，private metadata helpers、精确 candidate export、
inline 原诊断映射、candidate consumer flavor、sanitizer flags 未发现新的 P1/P2；GNU ABI tag
窄修复也复审通过。子代理未执行构建，运行证据仍为实际原始输出。
Linux staged consumer 使用其新安装 prefix/lib 的 LD_LIBRARY_PATH，以解决 RUNPATH 不传递的
间接依赖加载，不借原 build/lib 混入测试。private/detail 禁止导出检查始终保留。
GCC 的 entryKind 条件括号建议与既有第三方 minizip mktemp linker warning 保留为非阻断警告；
未更改既有生产预算、第三方依赖或 CI 调度优化。

S7A-7.1–7.5、8.1–8.3 的本轮实现与受限本地功能验收按逐行表结算；设备、生产容量和新 SHA
hosted 证据独立留存。S7A-8.4 **未退出**：owner 本人 approval、可信 master bootstrap、保护/
required workflow、同 SHA hosted gate 尚未满足；本轮未代审批、bootstrap、改保护、合并或发行。
Version advancement pre-merge 恢复 pull_request 事件入口，本地 trusted-base tests 通过；trusted base
缺少 owners/checker 前置条件时仍必须 bootstrap_required，不能从 PR head 执行脚本或伪造审批。

### I78-6 测量输入和 handoff 草稿

累计输入：CLI closure-report 的 19 类结构/字节计数与 exact/semantic SHA，Graph/Packed 双载荷、
inline/CXT/2x3置换、音乐闭包；public fixture main/config/foundation/music 的 owning 字节、完整资源;
25 Gameplay cases/1467 assertions、14 CLI、12 Host +2 assembler 准备、6 smoke frames；原始运行耗时仅作为本机
调度输入。resolver O(N)、稳态/未来队列/聚合成员/归档/快照/Replay/cut 大内容成本仍待量测。
handoff：同 SHA hosted 与真实设备列表、匹配工具链 shared 重建、可复现 fixture/输入 blobs、
所有首次失败与最终重跑边界、owner-only 待办、原 immutable generation/单 CXC 回滚边界。
不接受生产阈值，不执行 S7A-9 最终关闭验收，不进入 S7B+/S7C、Stage8正式发行或Stage7A关闭。

### 可核验原始输出包

[原始输出 ZIP](2026-10-08-s7a-7-8-raw-evidence.zip) 与
[输入及输出 SHA 清单](2026-10-08-s7a-7-8-evidence-manifest.json) 保存本轮首次失败、中间修复、最终
输出、实际 CMakeCache/LastTest 命令、CLI/Host 子日志、拥有型载荷与完整闭包 fixture。
清单逐项记录 raw bytes/sha256，实施输入同时记录待提交 index blob 与 Git-normalized working blob。
两个未修改的旧合同 CHART_V4/CXT 历史 blob 保留 CRLF，因此仅 EOL 规范化 hash 不同，不覆盖原文件。
原始包保留失败日志，不意味着包内每个历史运行成功；最终结果以本报告对应行
和最后结算为准。推送后 hosted 输出独立回填，不用这里的基线 SHA 或本地输出声称 hosted 成功。

## Linux 最终结算与证据订正

| 最终轴 | 结果 | 实际命令/原始输出 |
| --- | --- | --- |
| GCC15.2 ON Debug/headless 全仓 | 956/956，无失败；随后 Gameplay25/1467 | `cmake --build out/build/wsl-s7a78-gcc -j 6`；`ctest --test-dir out/build/wsl-s7a78-gcc --output-on-failure -j 6`；`out/build/wsl-s7a78-gcc/bin/cuexis_playback_tests "[candidate][gameplay]"`；out/s7a78-linux-gcc-boundary-fixed.log |
| Clang21.1 ASan/UBSan ON/headless | gameplay/architecture/package labels38/38，无失败；随后实际Gameplay25/1467、完整JSON25/275，无sanitizer finding | `cmake --build out/build/wsl-s7a78-clang-sanitize -j 4`；`ctest --test-dir out/build/wsl-s7a78-clang-sanitize --output-on-failure -j 4 -L "gameplay\|architecture\|package"`；该目录bin的Gameplay/JSON executable；out/s7a78-linux-sanitize-boundary-fixed.log |
| GCC shared Release | 首轮5项中3失败，已全部修复并逐项重跑成功：export1/1、安装consumer1/1（含12 Host+2 assembler）、existing Host staging1/1；原arch/CLI2项通过；Gameplay25/1467 | out/s7a78-linux-shared-final.log、shared-abi-tag.log、shared-runtime-fixed.log、shared-installed-isolated.log、shared-host-fixed.log（所有省略文件名均有s7a78-linux-前缀） |

上述WSL命令先 `cd /mnt/d/Cuexis-worktree`，导出
`CMAKE_PREFIX_PATH=/mnt/d/Cuexis-worktree/out/build/wsl-gcc/vcpkg_installed/x64-linux`，
完整运行shell命令另见out/s7a78-linux-final-live-commands.txt。cache依赖模式/匹配toolchain/基线注册SHA
和fresh/clean-first初次输出均保存。每次子fixture/run的真实命令和exit保存在LastTest/Host/CLI日志。
这些是本轮受限验证，不执行S7A-9最终关闭，也不代表hosted tidy/coverage已跑。

最终两处门禁修复：GNU精确公共函数允许既有B5cxx11返回类型tag；Linux新安装consumer只设
新prefix/lib、不继承开发LD_LIBRARY_PATH（复审P2假阳性反例已修复并实际隔离重跑）；
existing ReferenceHost新增candidate命令使用单次`cmake -E env LD_LIBRARY_PATH=<host_build>`，
只搜索已复制的staged runtime libraries，恢复环境后不依赖development路径。OFF/Windows路径未变。
私有detail/Kernel/Recovery图禁安装/导出保持。实际子代理仅做只读审查，运行证据来自原始日志。

计数订正：Host矩阵的14实际进程为12 installed Host（8正例+4负例）和2 assembler准备；
脚本现在分别计数，out/s7a78-host-count-corrected.log已实际验证。shared的Player项只是
gameplay_player_fixture生成输入；真实Player行为3cases/75assertions来自Debug CPU矩阵，
GPU补充smoke6帧且exit0另有原始输出。前文历史计数以本节和逐行订正为准。

S7A-7.1–7.5、8.1–8.3本轮实现/受限本地功能验收完成，C78/R78按上表与对应fixture回填；
S7A-8.4未退出，owner/可信bootstrap/保护/新SHA hosted未代办，真实键盘/音频/校准与Gameplay
反馈GPU像素待回填；生产预算不接受，Stage7A不关闭。

## 实现 SHA 绑定

本轮实现提交：`83935f1375be61ba0f3ea16205fccc6a70094c8e`；C78 十二行、R78 所选组合与 S7A逐行表的实现SHA均指向此提交。
原始输出运行于该提交之前的同一实施输入；清单全部 input index blobs 已与该提交树逐项比较一致。
后续证据提交仅更新日期报告、CURRENT_STATUS 和清单SHA，不改变运行代码/合同输入。
本地输出不声称新SHA hosted通过；推送后Hosted/owner/设备证据仍需独立回填，S7A-8.4未退出。

## 首次推送后的触发证据（不等待 hosted 完成）

首次推送HEAD `aaa9ca34311325df2086b8e02ea3b37a6c6abfbb` 的 pull_request
[Version Gate run](https://github.com/l-zilch-l/Cuexis/actions/runs/37786422185) 已实际启动
`Version advancement (pre-merge)`，随后completed/failure。只读即时查询时该job已经结束，
没有轮询等待CI完成；三平台结果继续待回填。
[原始失败输出](2026-10-08-s7a-7-8-hosted-version-trigger.log) 对应 `gh run view 37786422185 --log-failed`。
命令 `gh api repos/l-zilch-l/Cuexis/actions/runs/37786422185/jobs` 返回实际job状态。

精确诊断为 `version.bootstrap.required`：trusted base
`5472c463640cf3b66a03dd86fe87bd87239b2659` 缺 `.github/sdk-api-owners.json`，必须由owner审查
checker与保护规则后首次bootstrap。workflow拒绝回退执行candidate代码。本轮没有bootstrap、
代审批或保护修改；S7A-8.4继续未退出。触发bug已由实际job证明修复，不把该失败记成CI通过。
本证据补提交只改报告/CURRENT_STATUS和原始日志，不改873个实现输入。补提交后的最新HEAD
hosted证据仍待回填，旧run不声称验证新文档HEAD；实现输入与83935f1保持一致。
