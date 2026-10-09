# S7A-7/8 集成推进记录（2026-10-07）

状态：implementation-progress；本报告不是受限功能验收、生产预算接受或阶段关闭报告。

## 实际基线与授权边界

接手实际 branch 为 stage-7，HEAD 为 `55fc8e6920e7ddb4b34a56bd0b9c27d07d60b833`，
现有 PR #32 为 master ← stage-7。原有 16 个 tracked 规划修改及四个 untracked 规划/审查文件保留。
本轮实现仍在未提交工作区；尚无本轮 implementation SHA，不能把 HEAD 或旧 CI 当作这些代码的证据。
按 owner 本轮授权采用原 P78 推荐组合与 R78-01–08 A、09a B、09b A、10a B、10b A。
此授权不构成 owner approval、trusted bootstrap、保护配置、合并或发行。

权威计划仍为 [plan-a](../../../../stage_plans/active/stage-07/plan-a.md)，当前状态仅归
[CURRENT_STATUS](../../../../CURRENT_STATUS.md)。保留 S7A-3/4/5/6 的既有规则、生产 INCOMPLETE GATE
和 S7A-8.4 的 owner-only / hosted 门禁；I78-6 不开展 S7A-9 最终验收。

## 合同与实现的消费位置

Graph 首用物理合同新增 [Compiled Graph v1](../../../../formats/GAMEPLAY_GRAPH_V1_FORMAT.md)，
Writer 已开始消费，Reader/Entry 尚未消费。SAX 早拒绝 revision 与任意 root 顺序存在实际表示张力：
`{"GPR0":[大量记录],"capsuleRevision":99,...}` 会在获知错误 revision 前构造 payload。
合同采用四个有限 header 字段先于 payload、header 内与 payload 内分别可重排；不猜 revision。
影响仅为尚未实现的新 Graph JSON Reader/Writer；Foundation/Capsule 与判定语义不变。
Writer 浮点数明确保留 decimal/exponent 和 -0.0，防止静态浮点负零被 integer token 擦除；
Gameplay 整数字段继续禁止 f64。稳定 Graph 首用码已定义并完成中央登记（69 public codes）；Writer 定向消费已验证；Reader/Entry 尚未消费。
草稿 format 的点分隔拼写与既有 Spec §3.6 不一致，已按 Spec 改为 cuexis.gameplay-graph；
最小反例为符合 Spec 的 format 被草稿拒绝。影响仅为未实施的首用草稿，无别名或判定修订。

ABI 与 Playback 的 2026-10-07 revision 1 首用补充定义显式 Gameplay、owning handle、组合事务、
H/T/RuntimeFrame 分区、恢复、状态/操作与 Prepared 失效；Spec 与 Animation 补充 resolver 独立 Gameplay
层和 typed lifetime。ChartEntry 的新段仅定义 filesystem generation/adoption 发布，不改变 Foundation
载荷解释。合同文字不是下列整行的冻结或验收证明。

| C78 | 本次消费及证据 | 尚未满足的本行门禁 |
| --- | --- | --- |
| 01 | 显式配置、实际有限 Ruleset/module/Hook prepare，testOnly；零输入 Miss 与纯播放成对测试 | 生产预算接受独立；完整 manifest 配置链未完成 |
| 02 | Prepared 内 owning kernel/content；实际 owner/generation；组合 Runtime 克隆、预分配后发布 | 全组合分配故障扫点与真实新内容 Reload 矩阵 |
| 03 | 显式 i64 H/T/observation，原始 provenance 不影响结果，exact H→F(H)，future pending | 真实宿主输入捕获、完整 Tempo/Stop 时间桥矩阵 |
| 04 | Pause/Resume/Stop/Reset、W2 restore、archive/cut/H/checkpoint Seek；惰性分支复用原路径 | 持有 Hold/尾判、新旧表现 Reload、真实 audio discontinuity |
| 05 | owning typed revision 3 Packed source 一次 decode→实际 prepare，未 fallback JSON | compiled Graph Writer 定向实现；专用 Reader、七项 Entry metadata、filesystem/CXC 实际入口未实现 |
| 06 | filesystem publishAndAdoptGeneration 复用既有不可变 generation，单 adopted 切换点/写锁 | 生产 Gameplay assembler CLI、完整闭包、Graph/Packed deterministic equivalence |
| 07 | Fold 可发布 cursor；独立 resolver 层；future/expiry、false、Host priority 极值与重建；early/exact/late/Miss 人工 golden；dense/sparse/skipped 对照 | group aggregation/partial-group、sourceMap、多目标与宿主完整矩阵 |
| 08 | public owning Result/Replay/Snapshot/Checkpoint、W2 bytes 与 cutAt；原完整 ReplayEvaluation；static/shared 安装树消费已实际运行 | 跨工具链 codec/完整失败矩阵；shared 最新回执字段须复验 |
| 09 | 显式 GameplayOnly 与 Presentation 相同实际 kernel/Fold；无 World；必需 CPU 资源继续校验 | CXC/hash 入口完整性与 partial-group |
| 10 | actual registry 七字段与机器描述比较；公开首用 code 登记；src-only 公共投影 | 完整四态/closure 描述链与 first-error 顺序矩阵 |
| 11 | 安装树公共消费者源已增加 | Player 实际离散适配与 Gameplay Reference Host 六动词+tick 尚未接线 |
| 12 | ON 条件公共头/宏/方法，private link/static archive，未安装执行图 | 最终 ON/OFF/static/shared/MinGW/Linux consumer、API差异及owner/hosted |

## 已执行的中间验证

以下是不同中间输入的实际运行结果，后续源码变动须复验；不是最终 SHA 矩阵。
原始输出在本地 out/s7a78-* 留存，尚未作为最终受限验收归档。

| 命令 / 输出 | 实际结果 | 限定 |
| --- | --- | --- |
| vcvars64 → cmake --preset candidate-debug → clean-first build；out/s7a78-candidate-debug-build.log | 全目标构建通过，无 error/warning C | MSVC 14.51；尚未本轮日期/version fresh |
| cuexis_playback_tests.exe [gameplay]；out/s7a78-focused.log | 13 case / 512 assertions | registry/发布追加前输入 |
| cuexis_playback_tests.exe；out/s7a78-playback-all.log | 137 case / 11,203 assertions | 中间完整 Playback 组 |
| 全仓 CTest -j4；out/s7a78-candidate-debug-ctest.log | 1022 项，9 fail | CXC unpack output_commit_failed；另外8项 consumer/Host 缺 vcvars 环境；不能记为全通过 |
| vcvars64 → 再构建/定向/asset-publish/staging；out/s7a78-staged-second.log | Gameplay 15 case / 585 assertions；asset publish 24 case / 361 assertions；staging 9/10 | 新 consumer 编译/链接通过，HostClock 缺音乐导致运行拒绝；其余 CXC/consumer/Reference Host 通过 |
| vcvars64 → static consumer 复跑；out/s7a78-static-consumer-rerun.log | 1/1 通过，实际 GameplayOnly 公共生命周期 | fixture 改为 ChartClock；非生产 assembler |
| candidate-shared-debug fresh/clean-first；out/s7a78-shared-clean-build.log、out/s7a78-shared-gates.log | 全构建；架构/安装 Gameplay consumer/既有 Reference Host staging 3/3；Gameplay 15/585 | 后续发布回执字段变动须复验；Reference Host 尚未 Gameplay 接线 |
| 回执阶段与帧率 fixture 修正；out/s7a78-cadence-fixed.log | Gameplay 16 case / 675 assertions；架构/安装 consumer 2/2 | dense/sparse/skipped 同 H 的完整 kernel/Fold 与 typed lifetime；非最终 SHA |
| typed SAX 事件；out/s7a78-sax-events.log | JSON 6 case /64 assertions 通过 | 直接保留整数类型，未知字段先于 payload 拒绝；不是 compiled Graph Reader |
| Release 当前事件/golden；out/s7a78-release-events-current.log | 全目标增量构建，JSON 6/64、Gameplay 17/895、架构/安装 consumer 2/2 | 最低 0.7.1 请求已复验；未日期/version 更新，未代表最终 SHA |
| Graph Writer；out/s7a78-graph-writer-current.log | 2 case /51 assertions；完整 Capsule 18/1762 | 共享原 typed visitors/preimage；testOnly 三项显式界；重复输出、已有人工作品 identity golden、字节端点、无默认值、负零和声明的 escaping 验证；没有 Reader/双编码 prepare 完整比较 |
| Linux 当前事件/golden；out/s7a78-linux-events-current.log | 全目标增量构建，JSON 6/64、Gameplay 17/895、架构/安装 consumer 2/2 | 新 Writer 加入前输入；不能证明新 Writer |
| Graph Schema；out/s7a78-graph-schema-linux-self-check.log、out/s7a78-graph-schema-shape.log | Ubuntu Draft 2020-12 自校验与23项 shape 检查通过 | Windows Python 未装 jsonschema；shape-only fixture 不是 canonical Graph、不是 Playback 正例，不能替代 Reader/identity/closure |
| Graph 首用码登记；out/s7a78-graph-code-registration.log | 69 public code、9类别、19 R、20 pending 通过 | 登记先于消费者，不代表 Graph Reader 已实施 |
| bounded SAX；out/s7a78-bounded-rerun.log | JSON 3 case /40 assertions 通过 | 同日志 Gameplay 使用修正前 fixture 失败；JSON 通用边界不能代替 compiled Graph |
| headless ON 修正后构建；out/s7a78-headless-fixed.log | Gameplay 16/675，bounded SAX 3/40，架构/安装 consumer 2/2 | 无 SDL/OpenGL adapter、Player；未执行真实设备/GPU/audio |
| Release ON fresh/clean-first；out/s7a78-release-clean.log | 全目标构建；Gameplay 16/675，bounded SAX 3/40，架构/安装 consumer 2/2 | MSVC Release；旧日期构建号；consumer 最低 package 请求随后由 0.7 改为 0.7.1，须复验 |
| shared 当前源码 clean-first；out/s7a78-shared-current.log | Gameplay 16/675，bounded SAX 3/40，架构/安装 consumer 2/2 | 当前回执字段与最低 0.7.1 请求已消费；后续源码修复须复验 |
| OFF Debug fresh/clean-first；out/s7a78-off-debug-clean.log | Playback 121/10615，JSON 22/251，架构/旧安装 consumer 3/3 | 不编译/安装新 Gameplay header，不证明 ON 的新增行为 |
| MinGW headless fresh/clean-first；out/s7a78-mingw-rerun.log | 全构建；Gameplay 16/675，bounded SAX 3/40，架构/安装 consumer 2/2 | ON 的 candidateDisabledError unused 警告随后加 OFF guard，须增量复验 |
| Debug 不筛选全仓；out/s7a78-debug-all-unfiltered.log | 1030 项，2 fail，1 symlink skip | CXC unpack commit 与 ResourceScope fixture；不能记为全通过 |
| 两失败串行复跑；out/s7a78-two-failures-rerun.log | 2/2 通过 | CXC 间歇失败未定位；Resource fixture 根因另修，复跑不是删除初次失败 |
| Resource fixture 独占目录修复；out/s7a78-resource-fixtures-isolated.log | 11 项，各重复3次，-j8 全通过 | 原 per-process counter 都从1开始导致并发共享目录；不改资源生产语义 |
| Debug 当前源码第二轮；out/s7a78-debug-current-second.log | 1030 项，0 fail，1 symlink skip | 包含 Version Gate 与全部安装 consumer；随后增加人工 timing golden，不称最终输入 |
| Linux GCC 15.2 headless fresh/clean-first；out/s7a78-linux-clean.log | 全构建；Gameplay 16/675，bounded SAX 3/40，架构/安装 consumer 2/2 | 既有 minizip mktemp 链接 warning；OFF-only helper 警告随后已修；人工 timing golden 尚需复跑 |
| 人工 timing golden；out/s7a78-manual-timing-golden.log | Gameplay 17/895 | 手写 observation 4/5/6→TimingError -1/0/1 与零输入 Miss；正反绑定投影及完整 Replay 对照 |
| python -B tools/check_docs.py | 384 Markdown /20 candidate JSON-CXT 通过 | 最终源码/合同仍需复验 |
| python -B tools/check_docs_section_contract_tests.py | 5 tests 通过 | live anchor/归属；historical 不改写 |
| VerifyGameplayDiagnosticsCodes.cmake | 61 public code、9类别、19 R、20 pending 通过 | 新增为首次公共消费，13 src-only 不登记成 public code |

独立 oracle 使用既有 scanReference：人工构造 normalized observation，不调用生产推进或路由函数；
比较 Fact、phase、resource、contact、ownership、observer、receipt、frontier，Fold 期望值人工逐字段写出。
完整 replay 比较委托原 sameKernelResult，保留全部 Fold/错误/元数据，不以最终分数替代。
故障正负例含 Fold checked overflow 的 sealed/publishable 双前缀、Runtime 极值失败后的完整旧帧 digest、
非法 Capsule、假 Ruleset build、非法 Hook route、W2 tamper/budget、错误 path、future Snapshot 与 exact Seek。

定向复审曾提出 GameplayOnly Reset 空 Runtime 反例；当前代码已有 lastFrame && runtimeSession guard，
实际 headless Reset 测试通过，不能将旧快照问题写成当前仍存在的缺陷。
MSVC showIncludes 依赖跟踪仍有既有中文前缀问题，公共布局变动采用 clean-first，未改全局工具/CI策略。
一次诊断码表写回受系统 GBK 编码影响；已从实际 HEAD 的 UTF-8 原文恢复，再显式 UTF-8 增补首用登记，
保留原分类与裁定文字，重新通过码表门禁。中间失败均留存，不删日志或当作通过。

## 待执行与提交边界

### 推送前 Version advancement 触发修复

owner 本轮追加授权修复 pre-merge 不运行。实际 PR #32 head 是 55fc8e6，base 是
5472c463640cf3b66a03dd86fe87bd87239b2659；该 head 的 check rollup 没有 Version Gate，
workflow API 显示 active，最后记录的该 workflow 运行仍是 4d8024e 的 pull_request。
base 工作流只有 pull_request，candidate 从 fcc2af3 改为只有 pull_request_target；后者使用默认
分支工作流，形成迁移触发断层。原始 API 输出：out/s7a78-version-gate-hosted-before.json。

修复保留 target 并补回 pull_request，pre-merge 的 job/checkout/base/head/event 映射统一支持两种
PR 事件。checkout 仍是 base，head 只 fetch data；执行的 checker/tests、owner 名册与模板
仍逐个 git show trusted base，缺任一项明确 bootstrap.required，不回退 candidate。
没有 bootstrap master、发布 approval 或改保护。推送后 hosted 新事件是否启动须回填，不能把本地测试
当成 hosted 触发证明，也不能把“会运行”当成 owner 门禁已通过。
对上述实际 base 执行 git cat-file 已确认缺 `.github/sdk-api-owners.json`；因此以该 base 运行的
新版 workflow 将报 version.bootstrap.required。此 owner-only 条件准确保留，不通过候选名册绕过。

本地回归：tools/check_version_gate_tests.py 25 tests，Windows 两项 POSIX shell skip；
Ubuntu 原始输出 out/s7a78-version-gate-tests-linux.log，25 tests 全通过，无 skip，包含真正执行
workflow trusted-copy shell 的缺失基线拒绝反例。三个 workflow Bash 块另通过 bash -n，
原始输出 out/s7a78-version-gate-bash-syntax-rerun.log；首个内联命令因跨 shell 引号丢失失败，
保留 out/s7a78-version-gate-bash-syntax.log，改为逐字 UTF-8 文件后验证通过。触发回归覆盖 master/synchronize、PR 两事件的
job 准入、base checkout、实际 head SHA、merge-group 保留及禁止 head checkout。

S7A-7.1–7.5、S7A-8.1–8.3 仍在实施，不能以新增内部测试写成整行完成。
S7A-8.4 未退出：完整公共 API diff/patch-minor 证明、fresh/clean consumer、trusted master bootstrap、
保护、owner exact approval 和同新 SHA hosted 均需分别回填。现有 SDK candidate 为 0.7.1，
本轮尚未锁定最终版本或更新日期构建号；推送前按实际差异及 update_version 处理。
Graph Writer 初次测试编译因 Catch INFO ternary 未括号失败；加括号后编译通过。
escaping fixture 首用未注册 camera type，被既有 canonical gate 拒绝；随后 named Measure
未声明也被拒绝。按既有合同补足 Measure declaration 后测试通过，未扩大任何既有规则；
原始失败日志 out/s7a78-graph-writer-tests.log、out/s7a78-graph-writer-tests-rerun.log、
out/s7a78-graph-writer-final-tests.log 保留。

本轮尚未提交/推送，不另建 PR。完成要求的受限功能验收后仍按授权推送 PR #32、不等 CI。

当前中间 Debug/Release/headless/static/shared/MinGW/Linux GCC 证据已回填；新 golden 与后续代码仍须各自复验。
candidate preset 的名称筛选集不能称全仓 CTest；原始输出名含 full 也不能改变命令实际覆盖范围。
安装 consumer 当前最低请求为 0.7.1，只对应现有未发行候选，不是最终 patch/minor 裁定。
未执行：当前新增行为的 OFF Release、Linux Clang；最终 SDK 版本差异证明及日期更新后的 fresh 矩阵；
最终完整架构/package/ASCII/docs/version；GPU/window/audio/真实键盘和其他设备。
I78-6 暂仅有 fixture/计数与测量输入，不接受生产阈值、不关闭 S7A-9/7A 或进入 Stage 8 发行。

I78-6 当前输入计数：公开安装 fixture main.packed 为 3082 bytes、1 Requirement/1 Tap phase，
H=20、close=3，评分 Hit=2/Miss=-1 为人工 golden。Runtime 克隆随实际 World/实体数量增长；
上述 N=1 只证明功能路径，不提供大内容容量、长局耗时、生产 state-budget 或阈值接受。
