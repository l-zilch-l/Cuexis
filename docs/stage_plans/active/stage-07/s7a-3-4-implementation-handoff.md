# S7A-3 remaining work and S7A-4 implementation handoff

状态：active；选定方案的实施计划，不是完成报告

更新日期：2026-10-05

上级：[Stage 7 计划](plan.md)；当前状态唯一入口：[CURRENT_STATUS](../../../CURRENT_STATUS.md)。
设计依据：[ADR 0045](../../../adr/0045-gameplay-v2-execution-profile.md)、
[execution Spec](../../../formats/gameplay-v2-execution-profile.md)、
[author profile](../../../formats/gameplay-v2-author-profile.md)、
[typed supplement](../../../api/gameplay-v2-execution-types.md)、
[Capsule §12](../../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md)。

同日后续[歧义复核](../../../stage_reports/stages/stage-07/2026-10-05-s7a-3-4-ambiguity-audit.md)
已修订本计划引用的合同与 §4/§5 验收；旧推荐与早期报告不覆盖现行补充。

## 1. 给实施者的范围与基线

基线 branch=`stage-7`，已核对 HEAD=`6b11d102f6dcac4d9782cd03d0bae15add54af28`。
该 SHA 的四条 hosted workflow 证据及实际余项见
[余项核对报告](../../../stage_reports/stages/stage-07/2026-10-05-s7a-3-remaining-evidence.md)。
新对话先执行 git status/HEAD/diff；工作区已有本轮未提交设计文档，必须保留。
不要重做已存在 Reader/Writer/file-memory/S2 修复，也不要把旧 CI 成绩移给新行为 SHA。
不要恢复本轮开始前已回退的 CI 优化；本计划没有 CI 省略/裁剪授权。

先完成 S7A-3 余项与 revision3 静态合同，再完成 S7A-4 最小 kernel。
实施范围不包括 S7A-5 Score/Ruleset fold、S7A-6 wire/seek、S7A-7 发布入口、S7A-8
owner-only SDK gate、S7A-9 新业务限额、Stage 8 正式发行。
内部独立 evaluator 与 proof/checking fixtures 属本范围，不能伪装成 snapshot/replay 产品实现。
不创建提交、推送或 PR，除非新对话的用户明确要求；本计划本身不是发布许可。

## 2. 决策闭合清单

| 前一轮空白 | 本轮选择 / 唯一落点 | 实施者不用再选的内容 |
| --- | --- | --- |
| late-policy | execution Spec §3 | O/C/F/H 的单位、关系、相等边界、一次转发、finalization、checked overflow |
| 无 observation 的 error / 缺 chartTick | execution Spec §§2,6 + Capsule §12 | 显式 phase targets，timer/依赖 error absent |
| atom 与实际输入如何匹配 | execution Spec §§2,5 | 完整 binding、多 atom 集合、progress、ε、tailOnly |
| Candidate/contact/lease | execution Spec §4 + typed supplement | 结构字段、generation、press/release/update reducer、不可抢占 |
| Fact ordinal / rank / origin | execution Spec §7 | logical tuple、定表、timer batch、commit grouping、ID overflow |
| considered/consumed/stray | execution Spec §5 | considered 过滤层、赢家 seal 才消费、observe-only consumeEmpty |
| Hold 失败传播 | execution Spec §6 | head/body/tail 完整表、early release、deadline 去重、原时刻覆盖 |
| source 双路 / affine | author profile | 显式 extension、typed lowering、mixed-radix、全部源字段、等价范围 |
| Packed 缺字段 | Capsule §12 | candidateRevision3 与新 hash，旧 golden 保留、无静默迁移 |
| 全 Tick rollback / Fold 后失败 | execution Spec §7 | 未 seal 草案回滚；S7A-5 已 seal Ledger 不回滚 |
| S7A-3 是否必须等 runtime/全部容量 | 本计划 §5 | 分消费阶段对账，缺测独立，不虚报整阶段通过 |

本表是导航，不复制字段合同。完整方案没有“任选 H1/H2”或“遇到不清楚自行决定”。
若发现真实不一致，用最小反例提合同缺陷；先修合同再修实现，不把报错变成隐式默认值。

## 3. 顺序实施卡

### A：静态 schema、typed fields 与诊断

新增 PhaseTarget/AtomBinding/独立 pair/executionProfile；离线 candidate source schema，
Capsule revision3 Reader/Writer/hash 与 dispatcher。保留 revision1/2 合同、测试、默认模式。
先同步集中详细码表/校验，补 kernel.transaction_failed 的 faulted scope，不改九类或 R19。
检查 new fields 经 owning buffers 长期存活；非法输入不可覆盖旧 active/旧 artifact。
退出：revision3 full round-trip + header mutual rejection + mutation/hash + legacy tests 通过。

### B：author 双路与 affine lowering

tools 私有 author target 按 typed supplement 的唯一方案登记；Foundation 基础 Reader 与
Gameplay extension adapter 分工明确，JSON types 不逃 json_support。
完整 Chart inline fixture 和 CXT prototype/pattern+Binding fixture 产生同 E/stableId/sourceDocumentId；
explicit table 对照 affine tree；输出 owning GameplaySource 后只走同一个 assembler。
退出：canonical graph/derived closure diff=[]，Writer bytes 相同，file/memory reprepare 相同；
全部 source/rank/path/overflow 负例原子拒绝。不是把同一硬编码 typed graph 包两层函数。

### C：完整 S7A-3 E1 对账

审计 Capsule §11 八组 + §12 增量，逐条记录对应源码符号、fixture、测试名和断言。
记录四态：implemented+verified、implemented+unverified、missing、later-batch。
不以关键词没命中宣判无实现，不以总测试数替代某个要求断言。
T4/K4 runtime 行明确 later-batch=S7A-4，不删除；包含性 gate 必须当场失败。
退出：所有 S7A-3 首次消费行 verified；后续 runtime 行有 S7A-4 卡关联；
状态数缺测/证得下界/未接受阈值分维度存档，受限功能验收成立。

### D：L1 late gate + L2 独立模型

给现有 validatePrepare 补执行消费 gate，不能暗改 S7A-2 helper 的历史职责。
显式 O/C/F/H、A/P、pending/admission journal、closed/finalized/一次转发；
L2 从表计算结果，不调用 L1 routing/canonical comparator helper。
退出：fixture 边界、负 Tick、i64 两端、batch 原子性、重复与 dispatch collision 对照一致。

### E：H2/R1 kernel 与 phase reducer

先 self-owned config/prepared/query 接口，再 contact、Pattern progress、Candidate、K4 winner、
资源 variant、T4 reducer、TickDraft seal。保留未实现的无参 skeleton 和 snapshot/seek 拒绝。
不调用 World/SDL/audio/GL/host SDK，例外边界都返回 Result。
退出：完整 phase 事件表、owner-first control、fanout1、无 tail、observe-only、终态去重、
资源即时 free/terminal、query 生命周期和 failure injection 验证。

### F：S2 timer 与 F1/F3 Ledger

准备 immutable timer table、active index；另建不调用 S2 的 S1 扫描 reference evaluator。
Fact registry、tagged origin、逻辑 tuple、全 Tick sorting、monotonic IDs、signal 下一 Tick。
S1 可使用现有纯 compiled Pattern 的语言判定作为已验子模块；timer/contact/resource/phase
和 winner 算法自行从 Spec 计算，不共享生产 reducer/helper。
退出：完整 normalized/admission trace 的 phase/resource/receipt/Fact prefix 对照一致；
每个 Tick 的 mismatch 都能报告最小输入与结构 diff，不只比较最终 score/hash。

### G：最终证据与状态维护

按 §5 的命令和矩阵验证最后行为 SHA；新增 dated report 更新 CURRENT_STATUS。
代码实际未实现的项保持未实现，不因为“设计已闭合”填完成。
允许关闭 S7A-3/4 的受限功能验收；Stage 7A 整体、S7A-9 容量和 Stage 6 handover 门禁继续保留。

## 4. 必须固定的 golden 与负例

fixture 的数字是测试数据，不是 registry default 或 accepted production limit。

| 组 | 输入边界 | 必需断言 |
| --- | --- | --- |
| L | O2/C3/F8/H4，t100，A102/103/106/107/108 | native100/forward101/forward104/reject-hop/reject-final；原 observation 不改 |
| L-negative | O<0、C>=F、H0 queue、H>F-C、H>0 reject、missing/pending、overflow | 指定 category、旧 ingress/cursor/queue 完全相同 |
| D-time | 同一 subject 重复、distinct 同原 Tick、distinct 同 dispatch Tick、回退 clock/horizon | 无 arrival-order tie-break，整批无半个 admission |
| H | 同 channel press/press/update/release/release；重新 press；两个 sourceClass | repeat/orphan 明确，重新 press 的 handle 不同，旧 lease 不能复用 |
| P | atom 集合、sequence、choice、repeat0、skip/instant、prefix complement、ε Tap/head | prefix progress winner 才更新；ordered choice 不被排序；Hold ε prepare reject |
| T | end1000/grace40/body[900,1000)/tail[1000,1020)，release999/1000/1019/1020/1040 | 999 body+tail miss；1000/1019 body+tail hit；1020/1040 只一个 tail miss，所有 phase 单发 |
| T0 | end1000/grace0/body[900,990)/tail[990,1000)，release989/990/999/1000 | early miss / 合法 tail / 合法 tail / deadline 已 miss；no clipping |
| T-head | 无 head、head 晚于 b0、竞争 head 落败、最后 window close | 下游依赖全部 miss 一次；没有未获 lease 的 release patch |
| T-notail | nonzero grace，无 tail，release 在 b1 前/后 | 不产生 tail；b1 立即终止 lease；物理 contact 独立 |
| T-late | 原 release<b1 转发到 bodyEnd 之后；body 已 seal | 不能撤回 body Hit 或生成第二 outcome；已 seal 冲突拒绝 |
| K | pair -1/0 priorities、same priority different tieRank、collision、held/terminal、资源顺序 | 精确 K4、held 不抢占、所有占用组仅一 winner、owner release 优先 |
| C12 | 无 considered、只有 observe、held loser、无 transition、owner 早 release | stray/consumeEmpty 互斥；observe 不占资源；释放 miss 仍 consumed |
| F | 同 Tick observation/timer/receipt、多 timer 合并、phase propagation | 执行 timer-before-input；Ledger origin rank；commit Fact 连续与 IDs 单调；absent error |
| X | allocation fail、error subtraction overflow、duplicate phase draft、ID/signal overflow | 全 Tick 未 seal state/ledger/cursor 不变，faulted 可 query/reset |
| R | 相同 submit/admission 轨迹，1次/分段 advance，World/后端/源集合重排 | phase/resource/receipt/Fact 逐字段同；ordered 列表变化正确改变结果/identity |
| S | signal visible=t+1、sameTick 不可见、无真实 Fold fixture | 不触发同 Tick 二次判定，不假装 Ruleset 已完成 |
| E1 | 两路源码、affine golden6实例、rev1/2/3、file/memory、字典/sections重排 | semantic diff=[]；revision3 新 bytes/preimage固定；旧 golden 不变 |
| X-prefix | advance 在成功 Tick 后失败、空区间、最小 Tick，ID 到 UINT64_MAX | 保留已 seal prefix，失败 Tick 完全不变；A/P/fault 准确；最后合法 ID 不提前拒绝 |
| P-state | window gap、死分支、自环、多 atom、head prefix/repeat press、observer ε/timeout | 不重置/不重试，单步多分支；owner update 不假消费；observe 只观察记录一次/无 Miss |
| A-source | 原始 required extension、root version、Repeat marker、一个 entity 多/零 requirement、unused definitions | 未知不删，CXT 基础 reader 不原样吞扩展；emit0/repeat index+1，rank 减一；全实例 owning 输出 |
| I | 配置/图字段或 engine token不符、mapping/校准变化、reset/cross-run/query 生命周期 | prepare 原子拒绝或运行身份改变；保持已有 query 签名；旧 projection 保活；不同 run 不混用 |

差分测试须固定 seed 并记录失败最小 trace；timer/P1/T4/K4 边界不只靠随机覆盖。
独立 oracle 必须有人工推导 golden 锚点，两个实现一致不自动等于都正确。

## 5. 验证与关闭条件

每张卡先跑受影响 Catch2/脚本检查；最后按 AGENTS 的 Debug/Release 构建流程执行完整必要检查，
不为 docs-only 阶段伪造 C++ 证据。Windows sanitize/coverage 走现有 Linux hosted，不用 MSVC 假失败。

```powershell
python -B tools/check_docs.py
python -B tools/check_docs_status_contract_tests.py
python -B tools/check_docs_target_contract_tests.py
git diff --check
cmake --build --preset debug --target cuexis_format_check
ctest --preset debug --no-tests=error -L '^(judgement|gameplay_packed|gameplay_author)$'
ctest --preset debug --no-tests=error -R '^(cuexis_architecture_tests|cuexis_gameplay_diagnostics_codes)$'
```

以上 labels 必须先由卡 B/E 登记到 catch_discover_tests 的 PROPERTIES；当前 HEAD 尚未登记。
逐一用 `ctest --preset debug -N -L '^judgement$'`（另外两 label 同理）核对各自非零的
实际 Catch2 case 清单；三 label 合计非零不能证明每一组都有测试。现有 executable 名不等于
Catch2 case 名，旧按 target 名的 -R 不能证明跑到了 unit cases。诊断脚本实际名为
cuexis_gameplay_diagnostics_codes。最终完整 Debug/Release CTest 仍必须运行。

Debug/Release 完整 builds/tests、static/shared clean external consumers、Linux GCC/Clang、
Windows MSVC/MinGW 与 revision3 相同输入 byte/preimage golden：按当前既有矩阵保留。
不能把本地未运行的 hosted、真实设备/GPU、coverage 说成通过；本批 kernel tests 应 headless。
生产性能阈值未冻结时记录各维实际 count、峰值、耗时分布与 fixture，不引用 4096/1024/INT64_MAX
作为新业务 cap；只执行已有 accepted physical bound 与显式 measured containment。

关闭 S7A-3：A/B/C 的全部首次消费项 verified，reader 原子性与字节跨工具链证据归档；
T4/K4 runtime 验收在 E/F 关闭前标 later-batch，随后回填最终 kernel evidence 链。
关闭 S7A-4：D/E/F 的全部要求 verified，S1/S2 差分、独立 golden、失败注入、矩阵齐全。
未 measured state-budget/未接受 steady thresholds 属 S7A-9 **INCOMPLETE GATE**，
两批可标“受限功能验收完成，容量整体证明未完成”；不能标“所有预算通过”或关闭 Stage 7A。
原 E1 要求没有删除，只按实际首次消费阶段提供证据。

## 6. 新对话可直接使用的指令

> 在 D:\Cuexis-worktree 的 stage-7 分支实施 S7A-3 剩余工作和 S7A-4。
> 先阅读 docs/CURRENT_STATUS.md、AGENTS.md、本交接计划及它链接的 ADR/Spec/ABI/Capsule。
> 采用已选定 execution profile，不再自行选择语义；先 A/B/C，再 D/E/F，最后 G。
> 保留当前未提交设计文档和其他用户改动，不改 CI、不升 SDK、不做提交/推送。
> 不开展 S7A-5/6/7/9 产品实施。逐条给出测试/golden/最终行为 SHA 证据，
> 状态预算缺测保持独立，不能用默认值或成功占位绕过门禁。
> 真正合同矛盾须提供最小反例并修订合同；普通实现错误自行修复，持续工作到本范围验收完成。
