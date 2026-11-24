# D-20 旧谱兼容语料清单

状态：evidence

更新日期：2026-11-23

<!-- cuexis-meta
id: EVIDENCE-stage-07-rr.legacy-corpus-inventory
status: evidence
authority: none
date: 2026-11-23
-->

状态：evidence（T-01 步骤 3 交付物）

更新日期：2026-11-23

基线：`stage-7-rr`，HEAD `a22e255f6716b22f96ad8a183136eae4fb04ac2f`，tree `7869115460b0c17a1f833f30700cd06f34e8e3c3`，工作树干净（`git status --short` 无输出）。本清单未修改任何已跟踪文件，未运行构建、设备或 hosted 任务。

## 结论

结论：**部分成立**。当前仓库能绑定 7 行旧内容对象的原始输入、状态和设置，其中 4 个判定组合带有可核对的预期来源；但经查仓库内**未找到任何具名正式发行的判定组合**；`D-17` 明确格式合同不自动构成发行支持，`ADR 0046` 标注当前分支“已有 candidate 实施，未发行”，本阶段计划 §13 与验收“发布与迁移门禁”也明文不发布。历史发行资料是否存在无法核实。因此：

- D-20 的「仍正式支持的判定组合」在仓库内为空集；就仓库内可得证据而言，正式发行回归**不适用**，不是本轮可以按通过退出的对象。D-20 正式条款的满足状态：**无法核实**（没有可执行的正式发行组合，只能核实内部候选回归）。
- 内部候选判定回归可以执行，但只有 1 个组合（L1）同时具备存储态成品、设置、输入和交付时人工 golden；其余 3 个组合的输入/预期内嵌在测试源码中，没有独立成品文件。
- 格式兼容组合（F1/F2）有合同级读取承诺，但没有游戏内容，不能作为 D-20 判定语料；其字节 golden 由实现生成，不能当作独立预期。
- 恢复 wire revision 1（R1）已经退出，现行 Reader 明确拒绝；它是稳定拒绝的负例，不是仍支持的组合。
- T-09 必须把「仓库内未找到具名正式发行判定组合、历史发行资料无法核实、D-20 正式条款无法核实」作为具名残余交接，不能用本清单的内部夹具代替正式发行语料；按 findings §24，尚未找到的历史发布资料属于具体证据缺口，不能预认证。

## 1. 判定口径

D-20 要求对仍正式支持的判定组合，使用发行时交付的同一份谱面或成品，从相同初始状态，按相同判定/计分设置和同一组带时间与顺序的输入，复现已发布规则约定的逐音符结果与结算（[architecture-design.md §D-20](../../../../stage_plans/active/stage-07-rr/architecture-design.md#d-20-旧谱判定兼容)）。

本清单把证据分成四类，缺口按类登记：

1. **发布身份**：文档/ADR/发行记录承诺该组合被支持，并给出可追溯的发行或交付身份。
2. **原始输入**：源谱、编译成品、Replay/Snapshot 或等价的可执行输入文件，附仓库相对路径和 SHA-256。
3. **设置记录**：初始状态、判定/计分设置、规则集、时间基准、输入映射等。
4. **独立预期**：逐音符结果与结算的预期不是当前实现输出回写；人工 golden、独立参考实现或明确规则推导均可，但必须说明来源。

「正式发行回归不适用」就仓库内可得证据而言，表示该组合没有具名正式发布承诺，或内容本身不含判定语义；「承诺内缺证据」表示已有具名正式承诺但材料缺失，后者不能按通过退出。`CURRENT_STATUS`、`D-17` 和验收 A-07 的旧内容兼容是一般性条款，按 D-17 只对实际具名且验证过的发行组合承诺，不构成具名组合承诺；按 findings §24，尚未找到的历史发布资料属于具体证据缺口。本轮未找到的是具名组合与资料，不是已确认承诺缺资料。

## 2. 组合总表

共 7 行：4 个判定组合（L1–L4）、2 个格式兼容组合（F1–F2）、1 个已退出 wire（R1）。正式发行身份（仓库内）：0/7；内部证据 6/7（判定候选 4、格式夹具 2）；已退出：1/7。

| ID | 内容组合（Chart × Gameplay/Ruleset × 形态） | 支持状态与身份 | 原始交付物 | 初始状态 / 设置 | 输入 | 独立预期 | 缺口分类 | 结论 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| L1 | Chart v4 表现谱 + CXC v1 包 + gameplay capsule.3 + `gameplay.execution.t4-k4.v1` + `cuexis.finite-fold.1`；Reference Host 成品 | 内部候选交付：S7A-7/8 证据 manifest（build `26.10.08-1`，SDK `0.7.1`，implementationSha `83935f…`）；无正式发行身份 | `…/s7a78-host-fixture/main.cxc`（zip 内），SHA-256 `a8cf5275…`；zip `012efd46…` | 初始 score/combo = 0/0，Ready；设置 = 包内 `configuration.json`（hit +2 / miss −1，finite-fold，testOnly） | `input.txt`：tick 900 单次 press；`commands.txt` 10 条命令时序 | 交付报告人工 golden：hit `score2/combo1/hits1/misses0`；零输入 miss `score−1/combo0/hits0/misses1` | 发布身份缺；逐音符预期仅聚合；现行 Reader 兼容未在本卡运行 | 部分成立 |
| L2 | Chart v5 候选语义谱 + gameplay v2 + `gameplay.execution.t4-k4.v1` + `cuexis.finite-fold.1` + capsule.3；测试内生成 | 内部候选基线；无正式发行身份，无成品文件 | 无独立文件；由 `playback_gameplay_fixture.hpp` 生成，hash 见 hashes.txt | 初始 0/0，无 Fact；规则命中 +2 / Miss −1，mapping `[−100,100]` | tick 5 单次 press，seq 1，`lane.one/domain.binding.one/keyboard`；horizon 20 | 硬编码 `score2/combo1/hits1/misses0`（hit）与 `−1/0/0/1`（miss）；另有独立 `scanReference` 对照 | 发布身份缺；无成品文件；历史 oracle 运行曾 SIGSEGV，本卡未复跑 | 部分成立 |
| L3 | Chart v5 候选语义谱 + `gameplay.execution.t4-k4.v1`（无 Ruleset，Fold absent）；长按尾判边界 [990,1000) | 内部测试；无正式发行身份 | 无独立文件；输入与窗口内嵌于 `execution_kernel_phase_tests.cpp` / fixture 头 | 新会话，无 Fact；grace 0；body [900,990)，tail [990,1000)；late policy 见 fixture | press 900，release 依次取 989/990/999/1000；horizon 1103 | 手工边界公式：release≥990 为 body Hit，990≤release<1000 为 tail Hit，否则 Miss | 发布身份缺；无结算预期（无 Ruleset，无结算） | 部分成立 |
| L4 | Chart v5 候选语义谱 + `gameplay.execution.t4-k4.v1` + `cuexis.finite-fold.1`；hold 三阶段 | 内部测试；无正式发行身份 | 无独立文件；输入、规则集声明内嵌于 `fold_recovery_tests.cpp` / `fold_recovery_fixture.hpp` | 新会话，0/0；head/body/tail 窗口 [850,901)/[900,1000)/[1000,1020)，grace 40 | press 900 seq1，release 1000 seq2；horizon 1103 | 人工 golden：score 6、maxCombo 3；独立 Fold oracle 逐 Fact 对账 | 发布身份缺；无成品文件；oracle 消费生产 Fact 流 | 部分成立 |
| F1 | Chart v1 → v3/v4 与 v3 → v4 迁移；源谱 + 规范化 JSON/报告 golden | 合同级正式读取承诺（ADR 0041；格式矩阵 Chart v1/v2/v3 implemented，v4 默认基线）；语料本身仅内部，无发行身份 | `stage2_migration_v1.cuexis.chart.json`、`chart_v3_static_migration.json` 及对应 golden；hash 见 §4.1 | 无判定输入/设置（Chart 内容不含 gameplay） | 无判定标准输入 | 字节 golden 由实现 Writer/迁移器生成（README 明说 production Writer），不是独立规则推导；结构断言为手写 | 发布身份缺；独立字节预期缺；无判定内容 | 不成立（作为 D-20 判定语料）；格式读取承诺成立 |
| F2 | CXC v1 包：`cxc_v1_v4_static.cxc`、`cxc_v1_v4_cxt.cxc`、`cfu_f_v4_reference.cxc` | 合同级格式承诺（CXC v1 accepted and implemented internally，无公共 package API）；仅内部 golden，无发行身份 | 三个 `.cxc` 文件，hash 见 §4.2 | 无判定输入/设置（CXC 内容不含 gameplay） | 无判定标准输入 | golden 由 production Writer 生成（README）；`cfu_f` 经公共 Playback 路径作为参考包核对 | 发布身份缺；独立字节预期缺；无判定内容 | 不成立（作为 D-20 判定语料）；CXC v1 格式承诺成立 |
| R1 | Execution Recovery revision 1（healthy/kernel/fold replay+snapshot） | 已退出；现行 revision 2/3 Reader 明确拒绝 revision 1；内部 fixture，无发行身份 | `tests/fixtures/diagnostic_revision1/*.replay|.snapshot`，6 个文件，hash 见 hashes.txt | 无现行判定设置；期望为 `codec.version_unsupported` | 六个原始字节文件 | 拒绝预期由合同（diagnostics 兼容页）和兼容测试给出 | 发布身份缺；已退出，不属仍支持组合 | 不成立（作为仍支持组合）；拒绝契约成立 |

## 3. 组合证据与缺口

### 3.1 L1：S7A-7/8 Reference Host 成品（唯一存储态旧成品）

**组合。** 包内表现谱为 Chart v4（`assets/charts/main.cuexis.chart.json`，chartId `019f0000-…-0401`，与 CFU-D 的 Static v4 Migration 示例同 chartId/对象来源，但字节不同）；CXC v1 包内含 `compiled/gameplay.packed`（capsule.3，`compilerProfile = gameplay.author.t4-k4.v1`）和 `compiled/gameplay.graph.json`（graphFormatRevision 1，capsuleRevision 3）。执行 profile 为 `gameplay.execution.t4-k4.v1`，规则集为 `cuexis.finite-fold.1`。

**交付身份证据。** 该成品在 S7A-7/8 证据清单中有内部交付身份：`baselineHead = 55fc8e6920e7ddb4b34a56bd0b9c27d07d60b833`、`implementationSha = 83935f1375be61ba0f3ea16205fccc6a70094c8e`、`buildIdentity = 26.10.08-1`、`sdkApi = 0.7.1`（`2026-10-08-s7a-7-8-evidence-manifest.json`）。报告明确写“实现仍为未提交工作树”“不关闭 S7A-9/7A、不进入 7B+/7C 或 Stage 8 发行”（`2026-10-08-s7a-7-8-entry-assembler-progress.md` §命令与原始输出、§Reference Host）。这是**内部交付身份，不是正式发行身份**。

**原始交付物。** 仓库跟踪的证据压缩包：

- 压缩包：`docs/stage_reports/stages/stage-07/01-a/s7a-7-8/2026-10-08-s7a-7-8-raw-evidence.zip`，SHA-256 `012efd46392cb54b44f79b468bad3e861b7017235711b58171097ee070f25861`（与 manifest 的 `archiveSha256` 一致）。
- 包内条目（字节哈希已在交付 manifest 中登记，并已复核）：
  - `out/s7a78-host-fixture/main.cxc`，22501 bytes，`a8cf5275f5140c808bc521696e6592e09b4f26982f655325c7bb3f0cb2c86178`
  - `out/s7a78-host-fixture/configuration.json`，1647 bytes，`3d428edec69a70827e10efbe9220459104027726f7c40d120da8f525f4be6817`
  - `out/s7a78-host-fixture/input.txt`，72 bytes，`c848c884e2fe30c59e8389044d46303efad69caa4e8137c7a733eaed43567394`
  - `out/s7a78-host-fixture/commands.txt`，74 bytes，`5462070e7bf427cf4b94a5376e721ebccf14216e78d78834bbf6bf7f2030a523`
  - `out/s7a78-host-fixture/hit.log`，4573 bytes，`b6289f1ebf4a1d1aadd039b845f7df0dd59af1d0568ef81222e9415d44dbe6ec`
  - `out/s7a78-host-fixture/miss.log`，4490 bytes，`e294a65fec805f3f78f1461bb5f8c9d8fdd1a9d51cb3c41fecc3bfab9429bf27`
- `main.cxc` 内部 manifest 及条目：`assets/charts/main.cuexis.chart.json` `aec61d9ec074b424e5801020bb83905d6a1dcc1e9f4ada8bfed2a5e9531066a1`；`compiled/gameplay.packed` `7522d44ae6674c3db02d08e81fdb76604c0e9b15808e3f6b6ae6f67f6932547b`（2515 bytes，与 manifest 的 `artifactIdentity` 一致）；`compiled/gameplay.graph.json` `cb4cd5cd57343b8ba3a97ad9b17d23f70da8e756d0d04d8807273e9fb0009843`；`cuexis.project.json` `88d140210a7b63d62ab5199bcddf02e5b1b1cbfe65affd1036248cb25b77206d`。
- 为便于 T-13 直接消费，已按字节抽取到 `out/review/stage-07-rr/T-01/legacy-corpus/fixtures/s7a78-host-fixture/`（`main.cxc`、`configuration.json`、`input.txt`、`commands.txt`，以及明确标注为历史观测而非预期的 `historical-output/hit.log`、`historical-output/miss.log`）。抽取命令见 `commands.log`；抽取后逐字节和 SHA-256 均与压缩包内条目一致。

**初始状态。** Host 以空会话启动；`open` 后进入 Paused 并 commit 为 Ready；chart time 0；score 0、combo 0（`configuration.json` 的 `initialScore/initialCombo` 均为 0）。`commands.txt` 的完整时序为 `open / play / tick 4 / pause / tick 2 / seek 1000 / reload / play / tick 4 / quit`。

**判定/计分设置。** `configuration.json` 给出 finite 规则集：`actualRulesetBuild = cuexis.finite-fold.1`、`actualRulesetInterface = cuexis.ruleset.finite`、revision 1；`scoreRules` 为 tap hit `+2`（`incrementsCombo=true`）和 tap miss `−1`；domain `domain.binding.one` 范围 `[−100, 2000]`；`initialScore/initialCombo = 0/0`；`life=false`；`testOnly=true`；engine 六元组包含 `judgement.t4-k4.v1`、`gameplay.execution.t4-k4.v1`、`late.window.logical.v1`。graph 内的单 tap requirement 窗口为 `[850, 920)`，目标 tick 900，与输入 tick 900 匹配。

**标准输入（带时间和顺序）。** `input.txt` 只有一行：`900 1 press lane.one domain.binding.one keyboard 100 200 300 400 0 0 0`。事件时刻为 tick 900，`IngressSequence = 1`，动作 press，通道 `lane.one`，domain `domain.binding.one`，设备时间戳 100/200/300/400 仅作 provenance。主机每步 `H=250`、`T=7`；`--gameplay-observations` 指向该文件。`miss.log` 的对照组使用同一包和同一命令序列，但省略 `--gameplay-observations`（零输入）。

**独立预期与判断依据。** 交付报告在 `2026-10-08-s7a-7-8-entry-assembler-progress.md` §Reference Host 明确写：“`out/s7a78-host-fixture/hit.log` 人工 golden score2/combo1/hits1/misses0；`miss.log` 零输入 score−1/combo0/hits0/misses1”。报告标注为人工 golden；未提供独立预期文件，故只能作为交付时人工核对值，不能当作独立 oracle；它不是当前实现输出回写。同一节的 `host.log`（H20、score0）被明确标注“不能当 Hit golden”，说明报告区分了预期与错误运行。局限是预期只有聚合值（单音符，因此等价于逐音符 Hit/Miss 一次），没有逐事件清单；`hit.log`/`miss.log` 本身是历史运行输出，T-13 只能把它们当“历史观测”而不是 oracle。

**缺口。** 发布身份缺（内部候选交付）；逐音符预期只到聚合/单音符；现行 Reader 能否无源直读该包未在本卡运行验证（T-13 负责）；包内表现谱 chartId `019f…0401` 与 graph 的 staticChart chartId `019a…0001` 不同，判定身份来自 graph/capsule，T-13 需要分别核对表现谱身份与判定身份；`main.cxc` 是 S7A 时代的 candidate entry，当前 CXC 可能需要候选条目注册/显式候选入口才能装载（历史报告记录过 `cxc.entry.unlisted` 初次拒绝）。

**分类与结论。** 就仓库内可得证据而言，正式发行回归不适用（无具名正式发行判定组合）；内部候选回归可执行且证据较完整。结论：部分成立。

### 3.2 L2：当前候选 facade 的单 tap hit/miss 组合

**组合。** Chart v5 候选语义谱（`gameplay_test_fixture.hpp` 的 `chartVersion = 5`、`gameplayVersion = 2`）经 `playback_gameplay_fixture.hpp` 组装为 capsule revision 3；execution profile `gameplay.execution.t4-k4.v1`；规则集 `cuexis.finite-fold.1`；表现源是内嵌的 Chart v1 JSON（仅呈现用途）。

**交付身份证据。** 无正式发行身份。该组合属于 `CURRENT_STATUS` 所称“Gameplay v2 是当前实施基线，顶层合同仍为 candidate”的候选路径；S7A-3/4 与 S7A-5/6 报告只记录“受限功能验收”，不是发行。

**原始交付物。** 没有独立成品文件。谱面、graph、capsule 由测试代码现场生成；字节级 capsule golden 在 `tests/gameplay_packed/gameplay_capsule_revision_tests.cpp` 中固定为 revision 2 `471e5cef…`（3264 bytes）和 revision 3 `ff1ff218…`（3370 bytes），source/preimage hash 见 S7A-3/4 报告 §3.1。测试源码哈希见 `hashes.txt`。

**初始状态。** 新建 `PlaybackSession`，commit 后 score 0、combo 0、无 Fact。输入为一次 press；先调用 `advanceGameplay({8}, {8}, {8,0,0})`，再调用 `advanceGameplay({20}, {20}, {20,12,0})` 推进到 20。

**判定/计分设置。** `TestContent` 的 `config`：`actualRulesetBuild = cuexis.finite-fold.1`、`actualRulesetInterface = cuexis.ruleset.finite`、revision 1；score rules tap hit `+2`、tap miss `−1`；`initialScore = 0`、`initialCombo = 0`、`life = false`；domain `[−100, 100]`；`testOnly = true`；engine 六元组与 L1 同族。requirement 的 tap 窗口为 `[0, 8)`、目标 tick 5、grace 0。

**标准输入。** `playback_gameplay_fixture.hpp::input()`：`{5}` tick、sequence 1、`Press`、`lane.one`、`domain.binding.one`、`keyboard`。hit 分支先 `submitGameplay` 再推进；miss 分支不提交任何输入。

**独立预期与判断依据。** `tests/playback/playback_gameplay_contract_tests.cpp` 中 `[candidate][gameplay][oracle]` 用例先用独立参考实现 `scanReference`（其注释明确不调用生产 advance、timers、Fold、replay 或 recovery）算出 Facts/phases/resources/contacts/ownership/receipts/observers/processedFrontier，再与生产投影逐项比较；同时硬编码 Fold 期望：hit 时 `score=2、combo=1、maxCombo=1、hits=1、misses=0、factCursor=1`，miss 时 `score=−1、combo=0、hits=0、misses=1`。这些数值不是从当前输出生成。局限是参考实现与硬编码预期和被测实现同仓同批演进，不能证明它们来自正式发行规则；证据 zip 内的 `out/s7a78-oracle.log`（当前工作树不可得）还记录过同名 oracle 用例的 SIGSEGV（当时的 `playback_gameplay_tests.cpp`），本卡未复跑现行目标，不能把历史通过或历史失败外推为当前结果。

**缺口。** 发布身份缺；无独立成品文件；独立 oracle 的历史运行曾失败且本卡未复核；没有“发行时交付的同一份谱面”对象。分类：就仓库内可得证据而言，正式发行回归不适用；内部候选回归可执行，预期来源部分成立。

**结论。** 部分成立。

### 3.3 L3：D-20 点名的长按尾判 [990,1000) 边界组合

**组合。** Chart v5 候选语义谱 + `gameplay.execution.t4-k4.v1`（无 Ruleset，Fold absent）；T4 hold 的 head/body/tail 三阶段。D-20 原文即引用该测试：“仓库的长按内部测试采用尾判窗口 `[990, 1000)`。第 999 个 tick 松手应判 Hit，第 1000 个 tick 松手应判 Miss。”

**交付身份证据。** 无正式发行身份；测试名即“Manual T4 release goldens…”，属于内部手工 golden。

**原始交付物。** 无独立文件；输入、窗口和预期内嵌在 `tests/judgement/execution_kernel_phase_tests.cpp`（L418–L458）与 `tests/judgement/execution_test_fixture.hpp`。

**初始状态。** 每个 `SECTION` 新建 `KernelFixture` 会话，无 Fact，`advance` 前无投影。`executionFields` 给出 head/body/tail 窗口 `[850,901)/[900,1000)/[1000,1020)`、目标 1000、grace 40；grace-zero 分支把 body 结束改为 990、body 窗口结束改为 990、tail 窗口改为 `[990,1000)`，grace 改为 0。

**判定/计分设置。** `KernelFixture::configuration()` 使用 `gameplay.execution.t4-k4.v1`、`late.window.logical.v1`、`fact.semantic.phase-local.v1`、keyboard mapping、domain 量程 `[−100,100]`；late policy 为 `finalizationWatermark=8`、`maxQueueHop=4`（queueNextTick 时）或 0、`windowCloseThreshold=3`、`windowOpenThreshold=2`。没有 Ruleset，因此没有 score/combo 结算。

**标准输入。** 同一批提交 `{press at 900 seq1, release at X seq2}`，其中 grace-zero 分支 `X ∈ {989, 990, 999, 1000}`，非 zero 分支 `X ∈ {999, 1000, 1019, 1020, 1040}`；随后一次 `advance(1103)`。

**独立预期与判断依据。** 预期由测试内手写公式给出：body 状态 = `release < bodyEnd ? Miss : Hit`，tail 状态 = `bodyEnd <= release < tailEnd ? Hit : Miss`，并额外检查 head/body/tail 各恰好一个 Fact、ownership 为空。该预期是手工边界推导，不是当前输出回写；同一文件其他用例有 `scanReference` 对照，但这个具体用例没有调用独立 oracle。D-20 的 999/1000 结论对应 grace-zero 分支：999 为 body Hit + tail Hit，1000 为 body Hit + tail Miss。

**缺口。** 发布身份缺；无成品文件；没有计分/结算（Fold absent），因此 D-20 的“结算”部分不覆盖；逐音符预期的独立性只到手工公式。分类：就仓库内可得证据而言，正式发行回归不适用；内部候选回归可执行，但只覆盖逐音符，不覆盖结算。

**结论。** 部分成立。

### 3.4 L4：finite Fold hold 结算手工 golden 组合

**组合。** Chart v5 候选语义谱 + `gameplay.execution.t4-k4.v1` + `cuexis.finite-fold.1`；T4 hold 三阶段，有 Fold 结算。

**交付身份证据。** 无正式发行身份；S7A-5/6 报告把它列为“受限功能验收”的 5.3/5.5 行，属于内部证据。

**原始交付物。** 无独立文件；规则集声明、输入和预期都在 `tests/judgement/fold_recovery_fixture.hpp`、`tests/judgement/fold_recovery_tests.cpp`。

**初始状态。** 新 `FoldFixture` 会话；score 0、combo 0，没有 Fact。

**判定/计分设置。** `fold_fixture::declaration()`：`cuexis.ruleset.finite`、build `cuexis.finite-fold.1`、modules score/combo/statistics、`ProgramPolicy::locked`、`OutcomeScope::declared`、checked arithmetic；每个 phase 的 hit 加 2 并增加 combo，miss 减 1 并清零 combo；`initialScore/maxCombo = 0`。hold 窗口由 `executionFields` 给出：head `[850,901)`、body `[900,1000)`、tail `[1000,1020)`、grace 40。

**标准输入。** 一次 submit：`press 900 seq1` 和 `release 1000 seq2`；随后 `advance(1103)`。

**独立预期与判断依据。** 人工 golden 写死 `score == 6`、`maxCombo == 3`（head/body/tail 三个 Hit）；另有 `fold_fixture::oracle` 独立 Fold reducer，逐 Fact 计算 score/combo/maxCombo/hits/misses/counts/stray/consumeEmpty 并与生产 Fold 比较。oracle 读取的是生产 Fact 流，因此它独立于 Fold 实现，但不独立于逐音符 Fact 的产生。

**缺口。** 发布身份缺；无成品文件；oracle 与输入同仓；没有发行规则文本。分类：就仓库内可得证据而言，正式发行回归不适用；内部候选回归可执行，逐音符与结算都有手工/独立对照。

**结论。** 部分成立。

### 3.5 F1：Chart v1/v2/v3 → v3/v4 迁移兼容组合

**组合。** Chart v1 源谱 `stage2_migration_v1.cuexis.chart.json` → v3 golden `stage2_migration_v3.golden.cuexis.chart.json` 与报告 golden；Chart v3 源谱 `chart_v3_static_migration.json` → v4 规范化 golden `chart_v4_static_migration.canonical.json`；v1 → v4 golden `chart_v1_to_v4.cuexis.chart.json`。

**交付身份证据。** 合同级承诺：`docs/formats/README.md` 把 Chart v1/v2/v3 列为 implemented，并保留生产读取路径；`ADR 0041` 明确“Chart v1/v2/v3 Reader、显式 v1/v2/v3 到 v4 迁移……仍按各自 ADR 保留”。交付记录：CFU-D1/D2 报告（2026-08-13）与 Stage 2 migration golden 测试。没有正式发行身份，也没有“发行时交付的同一份谱面”。

**原始交付物。** 源谱和 golden 都是仓库跟踪文件，hash 见 `hashes.txt` 与 §4.1；源谱不含 gameplay、requirement 或判定设置（`grep gameplay|requirement|judgement` 在 `tests/fixtures` 的 chart JSON 中无命中）。

**初始状态 / 设置。** 不适用（无判定输入、无计分设置）。

**标准输入。** 无判定标准输入；迁移测试的输入是源谱 JSON 字节。

**独立预期与判断依据。** `tests/fixtures/chart_format_update/golden/README.md` 的“compared byte-for-byte”只对 CXC goldens 明说；Chart 的逐字节比较在 `tests/chart/chart_writer_tests.cpp:95`（v4 canonical）与 `tests/chart/chart_migrator_tests.cpp:206,247`（v3→v4、v1→v4），Stage 2 golden 的比较在 `tests/chart/chart_migrator_tests.cpp:96-104`。`chart_v4_static_migration.canonical.json` 和 `chart_v1_to_v4.cuexis.chart.json` 是 Writer/迁移器输出；Stage 2 golden 也是 `migrateToV3` 输出。因此字节 golden 属于**输出回写基线**，不是独立规则推导。测试中的版本号、计数、空数组、identity 长度为手写结构断言，但不足以构成独立判定预期。

**缺口。** 发布身份缺；独立字节预期缺；无判定内容。分类：D-20 判定回归不适用；格式读取/迁移的合同承诺成立，候选兼容测试可用。

**结论。** 不成立（作为 D-20 判定语料）；格式读取承诺本身成立。

### 3.6 F2：CXC v1 成品包兼容组合

**组合。** `cxc_v1_v4_static.cxc`、`cxc_v1_v4_cxt.cxc`、`cfu_f_v4_reference.cxc` 及其 manifest。均为 CXC v1 包；`cxc_v1_v4_static.cxc` 是 CFU-E4 的 static v4 包，`cxc_v1_v4_cxt.cxc` 是 CFU-C3 的 canonical 包，`cfu_f_v4_reference.cxc` 是 CFU-F1 reference package。

**交付身份证据。** `docs/formats/README.md`：CXC v1 “accepted and implemented internally; archive/tools and Playback source/prepare/identity gates closed; no public CXC package API”；`ADR 0041` 保留既有兼容读取。CFU-F/G 已封存格式阶段，但仍不是完整 CXC 产品或公共 package API。无正式发行身份。

**原始交付物。** 三个 `.cxc` 文件与两个 manifest，hash 见 §4.2。包内条目已核对：均无 gameplay entry（只有 chart、asset index、project、CXT/材质/网格等）。

**初始状态 / 设置。** 不适用（无判定内容）。

**标准输入。** 无判定输入；若做装载回归，输入是包字节。

**独立预期与判断依据。** golden README 明说“generated by the production Writer and compared byte-for-byte”；因此字节预期是生产 Writer 输出回写。`cfu_f_v4_reference.cxc` 另有“checked against its filesystem project through the public Playback path”的行为对照，这可作为装载行为证据，但不产生逐音符/结算预期。

**缺口。** 发布身份缺；独立字节预期缺；无判定内容。分类：D-20 判定回归不适用；CXC v1 格式承诺成立，候选/内部装载回归可用。

**结论。** 不成立（作为 D-20 判定语料）；CXC v1 格式承诺本身成立。

### 3.7 R1：Execution Recovery revision 1（已退出）

**组合。** `tests/fixtures/diagnostic_revision1/` 下 6 个旧 wire 文件（healthy/kernel/fold 的 replay 和 snapshot），manifest 记录 `base_head = 2ddbcafe22bd3facc63b4917cf99eeccc3d81cd1`、generator 为 “baseline MSVC fresh candidate-headless-debug; fold_recovery_tests baseline exporter”。

**交付身份证据。** 内部 fixture，无正式发行身份；`docs/api/diagnostics-identity-and-compatibility.md` §兼容合同写明“candidate Recovery revision2 明确拒绝 revision1”。

**代码核实。** `engine/judgement/src/recovery_codec.cpp` 的 `unwrap` 只接受 `initial.rpa ? 3 : 2`，其余版本返回 `codec.version_unsupported`；`tests/judgement/fold_recovery_diagnostic_tests.cpp` 的 `[diagnostic][recovery][compatibility]` 用例对 6 个原文件断言 `codec.version_unsupported`，并核对错误码、类别和 `faulted=false` 上下文。

**独立预期与判断依据。** 预期是稳定拒绝，不是逐音符复现；来源是合同和兼容测试，不是当前输出。R1 已退出，不能算“仍被承诺支持”的组合，也没有 D-20 意义上的输入/结算需要复现。

**缺口。** 发布身份缺（内部 fixture）；不属于仍支持集合。分类：就仓库内可得证据而言，正式发行回归不适用；拒绝契约成立。

**结论。** 不成立（作为仍支持组合）；拒绝契约成立。

## 4. 关键对象哈希与核验

完整清单见 `hashes.txt`（路径 + 两个空格 + SHA-256，一行一个；路径均为工作区相对路径，正斜杠）。此处只列判定语料关键对象。

### 4.1 格式兼容对象

| 对象 | 仓库路径 | SHA-256 |
| --- | --- | --- |
| Chart v1 源谱 | `tests/fixtures/stage2_migration_v1.cuexis.chart.json` | `407667026475d92f6e578767b635878dd915d38dea5b15b805fbf5f752e9f894` |
| Chart v3 golden | `tests/fixtures/stage2_migration_v3.golden.cuexis.chart.json` | `b3b9582d95389de99f6d26da8e9a884365ef048ba1b28a527bf23160dfbfe950` |
| Stage 2 report golden | `tests/fixtures/stage2_migration_report.golden.json` | `49490f044816003d0eb42f7abc3cad24786a08e7233195f49521d16cc9e3546c` |
| Chart v3 源谱 | `tests/fixtures/chart_format_update/valid/chart_v3_static_migration.json` | `9cfbe70c9b23b6fc46fcc84ab5e739f449632e386a843723bd16485d787c690f` |
| Chart v4 canonical golden | `tests/fixtures/chart_format_update/golden/chart_v4_static_migration.canonical.json` | `1ca9f60feee215fdc4eca1f7cafbbea8704976eca18ed40e51333b6b2e7a5385` |
| Chart v1→v4 golden | `tests/fixtures/chart_format_update/golden/chart_v1_to_v4.cuexis.chart.json` | `912b25083338b612ff830cec76a7bad04c98c158485a0d9c11c31fe9beccaa4e` |

### 4.2 CXC v1 包

| 对象 | 仓库路径 | SHA-256 |
| --- | --- | --- |
| 静态 v4 包 | `tests/fixtures/chart_format_update/golden/cxc_v1_v4_static.cxc` | `af41afef1ea76ca152425426147546806aed21634e25b253fe73b0065f5a80a5` |
| 静态包 manifest | `tests/fixtures/chart_format_update/golden/cxc_v1_v4_static.manifest.json` | `581d4225f11c3257daa418da3038bf8cab7939b4f13a1bf6b09db8a6b60956d8` |
| CXT 包 | `tests/fixtures/chart_format_update/golden/cxc_v1_v4_cxt.cxc` | `95736452da4de84da2cbbb7ea77ff72450ebd34a3ab8069b411e720f1f4bab54` |
| CXT 包 manifest | `tests/fixtures/chart_format_update/golden/cxc_v1_v4_cxt.manifest.json` | `7b870364a16dee98021e1ceb1fd01bd118ff0f943ef65ceaea6a9a50f9a23b76` |
| CFU-F1 reference 包 | `tests/fixtures/chart_format_update/golden/cfu_f_v4_reference.cxc` | `1cb2fcbf7a852a4db2ed9119359c68ff1cc4a06d1d41d18018fff89cf737d723` |

### 4.3 L1 成品与记录

| 对象 | 路径 | SHA-256 |
| --- | --- | --- |
| 证据 zip | `docs/stage_reports/stages/stage-07/01-a/s7a-7-8/2026-10-08-s7a-7-8-raw-evidence.zip` | `012efd46392cb54b44f79b468bad3e861b7017235711b58171097ee070f25861` |
| 证据 manifest | `docs/stage_reports/stages/stage-07/01-a/s7a-7-8/2026-10-08-s7a-7-8-evidence-manifest.json` | `19986fe2f45f1a84af453677bd7ea7c2767d3937da0141055296a2e99767b40e` |
| 交付报告 | `docs/stage_reports/stages/stage-07/01-a/s7a-7-8/2026-10-08-s7a-7-8-entry-assembler-progress.md` | `c8bb9d2da39f3e867b5b16162aec7da66930021bc21e1dbb9a879524619fbc31` |
| 成品 | zip 内 `out/s7a78-host-fixture/main.cxc`；抽取副本 `out/review/stage-07-rr/T-01/legacy-corpus/fixtures/s7a78-host-fixture/main.cxc` | `a8cf5275f5140c808bc521696e6592e09b4f26982f655325c7bb3f0cb2c86178` |
| 配置 | zip 内同目录 `configuration.json`；抽取副本同名 | `3d428edec69a70827e10efbe9220459104027726f7c40d120da8f525f4be6817` |
| 输入 | zip 内同目录 `input.txt`；抽取副本同名 | `c848c884e2fe30c59e8389044d46303efad69caa4e8137c7a733eaed43567394` |
| 命令序列 | zip 内同目录 `commands.txt`；抽取副本同名 | `5462070e7bf427cf4b94a5376e721ebccf14216e78d78834bbf6bf7f2030a523` |
| 历史观测 hit | zip 内同目录 `hit.log`；抽取副本 `historical-output/hit.log` | `b6289f1ebf4a1d1aadd039b845f7df0dd59af1d0568ef81222e9415d44dbe6ec` |
| 历史观测 miss | zip 内同目录 `miss.log`；抽取副本 `historical-output/miss.log` | `e294a65fec805f3f78f1461bb5f8c9d8fdd1a9d51cb3c41fecc3bfab9429bf27` |

核验命令和退出码见 `commands.log` §3；6 个条目的字节数和 SHA-256 全部与 `2026-10-08-s7a-7-8-evidence-manifest.json` 的 `rawOutputs` 及 `archiveSha256` 一致。`main.cxc` 内部 `compiled/gameplay.packed` 的 SHA-256 为 `7522d44ae6674c3db02d08e81fdb76604c0e9b15808e3f6b6ae6f67f6932547b`，与包内 manifest 的 `artifactIdentity` 一致。

### 4.4 R1 文件

`tests/fixtures/diagnostic_revision1/manifest.json`（`f06e94039b49808fb6b7fdbc1cbed2c3eed0ab6f5015c0b608718f0af7bd7db2`）登记 6 个文件；逐文件 SHA-256 已复核并与 manifest 一致（见 `hashes.txt` 第 39–44 行）。

## 5. 正式发行身份核查记录

按任务要求，不能只凭“找不到文件”判不适用；以下记录实际查过的发布/合同依据。判断优先采用代码/schema/CMake 与合同文档，阶段报告只作证据。

| 依据 | 查到的内容 | 对本清单的含义 |
| --- | --- | --- |
| `docs/formats/README.md` 格式矩阵 | Chart v1/v2/v3 implemented；Chart v4 accepted/implemented 且为默认发行基线；CXC v1 accepted/implemented internally；Gameplay Judgement/Gameplay V2/Packed Chart/Capsule 为 candidate | 旧 Chart 读取有合同承诺；判定与成品格式仍是候选，没有正式判定组合 |
| `docs/adr/0041-legacy-format-exit-policy.md` | “兼容读取按既有合同保留”，不删除 Chart v1/v2/v3 Reader 与显式迁移；旧 API 退出需单独版本决策 | 支持的是读取/迁移，不等于判定发行；没有给出任何交付谱面或成品身份 |
| `docs/adr/0043-gameplay-judgement-ruleset-convergence.md`、`docs/formats/GAMEPLAY_JUDGEMENT_SPEC.md` | 状态 superseded / historical，“从未被接受为实施基线”，字段语义由 Gameplay V2 Spec 接替 | 不存在旧的正式判定规则版本可供复现 |
| `docs/CURRENT_STATUS.md` | Gameplay v2 是当前实施基线，顶层合同仍为 candidate；局部实现不等于整批验收；“旧内容按已承诺规则、时间及身份兼容” | 当前判定实现是候选基线，不是发行 |
| `docs/stage_plans/active/stage-07-rr/architecture-design.md` §D-17 | “D-17 … 不是已发行组合声明”；“D-17 明确格式合同不自动构成发行支持”（原文：“格式合同……不自动构成 Player 的发行支持”）；“Player 每次发行只对实际具名且验证过的组合……承诺”；Stage 8 负责正式化与发行组合 | 判定组合的正式发行支持在仓库内未找到；就仓库内可得证据而言，正式发行回归不适用 |
| `docs/adr/0046-realtime-playback-coordination.md` | 状态行：“accepted design；…当前分支已有 candidate 实施，未发行” | 当前分支未发行，不能绑定“发行时交付”的成品 |
| `docs/stage_plans/active/stage-07-rr/plan.md` §13 | “**不发布、不合并、不提升 SDK minor**” | 本阶段明文不发布 |
| `docs/stage_plans/active/stage-07-rr/acceptance.md` §发布与迁移门禁 | “本阶段不发布（计划 §13）”；旧 API 退役须先有实际发布的过渡版本和消费者证据 | 阶段验收同样不发布；没有实际发布基线时不预填退役版本 |
| `docs/stage_reports/stages/stage-07/01-a/s7a-8-4/2026-10-10-s7a-8-4-closure.md` | SDK 0.7.1、日期构建 26.10.09-2；owner 审批绑定 version gate/API patch；同文“不合并、发行或关闭 Stage7A” | S7A-8.4 的 owner 审批是 SDK version gate 审批，不是产品/内容发行审批 |
| `docs/guides/VERSIONING.md`、`cmake/CuexisVersion.cmake` | SDK `0.9.1` 是“未发行 candidate”的显式例外；日期构建 `26.11.22-1`；SDK 仍是 preview | 没有可以绑定“发行时交付”的正式版本 |
| `docs/stage_reports/stages/stage-07-rr/readiness/2026-11-22-pre-implementation-plan-check.md` | D-17/D-20 行判“部分成立”；“正式发行语料不能由当前有限用例替代，T-09 单列残余交接” | 与本清单结论一致：内部用例不能替代正式发行语料 |
| `docs/stage_reports/stages/stage-07-rr/decisions/2026-11-23-design-decisions-and-schedule.md` | “旧谱兼容绑定独立预期，成品无需源工程重放”，候选夹具与正式交付分开；限制写明“旧发行语料的存在性与独立预期须在 T-01 绑定，缺失时按具体材料登记，不能预认证” | 本清单已把候选夹具和正式交付分开，并把正式语料缺口交 T-09 |
| `docs/stage_plans/active/stage-07-rr/findings.md` §24 | D-20 由 T-01 绑定旧交付文件、规则、输入和独立预期；T-09 核对，不能整体延期，不能把候选夹具升级为正式发行承诺。§24 原话：“尚未找到的历史发布资料属于具体证据缺口；不能以当前输出代替预期，不能把候选夹具升级为正式发行承诺。” | 责任链与本清单一致；仓库内未找到不等于历史上从未存在，缺失资料按具体证据缺口登记 |
| `git tag --list` | 仅有 `archive/pd5-c2-read-result`、`archive/version-gate-bootstrap`、`r3-gate-baseline-2026-11-01`、`readlong-rollback-20261111` 等内部门禁 tag，没有发行 tag | 没有可绑定的发行记录 |

**核查结论。** 仓库内判定组合的正式发行身份为 0；历史发行资料是否存在无法核实。旧 Chart/CXC 只有合同级读取/格式承诺，没有发行时交付物。任何把内部 golden、示例或测试源码当成正式发行语料的说法都不成立。D-20 正式条款是否被满足：无法核实（没有正式发行判定组合可执行）；内部候选回归（L1–L4）可执行但不构成正式发行通过。findings §24 的“尚未找到的历史发布资料属于具体证据缺口”继续适用，不能用仓库内清点结果预认证。

## 6. T-10 / T-13 交接

- **T-10（旧内核逐音符与结算）**：L2 在 `cuexis_playback_tests` 的 `tests/playback/playback_gameplay_contract_tests.cpp`（`[candidate][gameplay][oracle]`，含 `scanReference` 独立对照）；L3 在 `cuexis_judgement_tests` 的 `tests/judgement/execution_kernel_phase_tests.cpp`（`[execution][kernel][golden][t4]`）；L4 在 `cuexis_judgement_tests` 的 `tests/judgement/fold_recovery_tests.cpp`（`[fold][golden][oracle]`）。不得用本次运行输出生成自身 oracle；L3 没有结算，T-10 不得在 L3 上宣称结算已验证；L1 的历史 hit/miss 日志只能作为历史观测，不得当作独立预期。
- **T-13（旧成品无源直读）**：唯一可用的存储态旧判定成品是 L1 的 `main.cxc`。建议先从证据 zip 抽取或使用本目录 `fixtures/s7a78-host-fixture/` 的字节副本，核对 SHA-256 后，按 S7A-7/8 的 `commands.txt`/`input.txt`/`configuration.json` 运行，目标为人工 golden `score2/combo1/hits1/misses0` 与零输入 `score−1/combo0/hits0/misses1`。不要重新取得源工程或重新编译；不要用历史 `hit.log` 替代预期。包内表现谱和 graph staticChart 的 identity 不同，须分别核对；当前候选入口的注册/CLI 可能与 2026-10-08 不同，按 T-13 的当前构建实际入口执行。
- **T-09**：本清单的 7 个组合都不能作为正式发行回归的通过证据；T-09 需要把“仓库内未找到具名正式发行判定组合、历史发行资料无法核实、D-20 正式条款无对象”列为具名残余，并说明内部候选回归（L1–L4）已绑定；不能用 L1/L2 的局部用例替代 Stage 8 的正式发行语料。

## 7. 检查后未纳入的对象

| 对象 | 为什么未纳入 | 处理 |
| --- | --- | --- |
| `tests/fixtures/gameplay_author/*`（JSON 与 CXT，如 `inline_2x3.json`） | Chart v5 + gameplay v2 的候选作者源，用于 assembler 双路/展开测试；没有任何逐音符或结算预期，也不是旧发行内容 | 只作候选 author 证据，不列入 D-20 语料；T-13 若要验证“源→成品”可用它，但不能当作旧成品 |
| `assets/projects/demo20s.cxc` | 仓库示例包（2026-09-02 提交，Chart v3），有 `docs/examples/demo20s.md`，但不是兼容 golden，也没有判定内容或发行身份 | 记录哈希 `dbf8fccafe73cd8258e350f63157f6f6422ed366f3e0d1f2c6258d1b08319c56`；不列入 D-20 组合 |
| 证据 zip 内的 `out/s7a78-oracle.log`、`out/s7a78-hold-oracle*.log`、`out/s7a78-manual-timing-golden.log` 等历史运行日志（当前工作树不可得） | 是历史证据，不是当前通过证据；其中 `oracle.log` 记录同名 oracle 用例 SIGSEGV，不能据此写通过 | 作为 L2 的历史风险记录，不当作预期或当前结果 |
| zip 内 `out/s7a78-host-fixture/host.log`（H20） | 交付报告明确写“尚未进入窗口，score0 不能当 Hit golden” | 明确排除，防止误用 |
| 报告引用的 `out/s7a-3-4-behavior-inputs-final.json`、`out/s7a-prepush-inputs-final.json` 等指纹 | `out/` 是忽略目录，不在跟踪文件内，独立 checkout 不依赖它 | 只记录为历史证据，不作为语料身份 |
| Stage 7A 报告中的 Capsule revision 2/3 golden hash | 固定 bytes/hash 在 Catch2 测试中，无独立 fixture 文件 | 归入 L2 的设置/成品字节证据，不另立语料 |

## 8. 复现与边界

- 哈希复现：`hashes.txt` 每行 `<工作区相对路径><两个空格><sha256>`；命令模板即 T-01 步骤 3 给出的 `python -c "import hashlib,pathlib,sys; print(hashlib.sha256(pathlib.Path(sys.argv[1]).read_bytes()).hexdigest())" "<路径>"`。`commands.log` 逐文件记录了命令和退出码；全部 exit 0。
- L1 抽取：`fixtures/s7a78-host-fixture/` 的 6 个文件由 `2026-10-08-s7a-7-8-raw-evidence.zip` 的对应条目逐字节写出；抽取后字节数和 SHA-256 已与 evidence manifest 交叉核对。`historical-output/` 两个日志只作历史观测，不是预期文件。
- 未执行：未运行任何构建、CTest、Reference Host、设备、hosted 或长时任务；未修改已跟踪文件；未 `git add` / `commit`。本子代理的写入只涉及 `out/review/stage-07-rr/T-01/legacy-corpus/`；收尾 `git status --short` 显示的已跟踪修改来自 lead 的 T-01 并行落盘，不属于本子代理写入（详见 §9）。
- 结论四档只用于本清单的 D-20 证据充分性；不表示 Stage 7-RR 的任务完成或 D-20 已按通过退出。

## 9. 交叉核验处置（2026-11-23）

核验报告：`out/review/stage-07-rr/T-01/legacy-corpus-crosscheck/report.md`。核验总体结论为部分成立，确认 7 个组合可绑定、仓库内无具名正式发行判定组合、L1 是唯一具备存储态成品/设置/输入/人工 golden 的组合、F1/F2 字节 golden 由生产端输出、R1 已退出。以下按核验报告「需改写之处」逐条处置。

1. **D-17 引文。** 已删除“D-17 明文说明 Player 尚未发行”的归属写法，改为“`D-17` 明确格式合同不自动构成发行支持”，并补 `ADR 0046`（当前分支“已有 candidate 实施，未发行”）、`plan.md` §13“不发布、不合并、不提升 SDK minor”、`acceptance.md` §发布与迁移门禁“本阶段不发布”作为当前未发行依据。依据 A.3。
2. **“不存在”与“为 0”。** 结论第 1 条改为“D-20 的『仍正式支持的判定组合』在仓库内为空集；历史发行资料是否存在无法核实”；§5 核查结论改为“仓库内判定组合的正式发行身份为 0；历史发行资料是否存在无法核实”。补入 findings §24 原话“尚未找到的历史发布资料属于具体证据缺口；不能以当前输出代替预期，不能把候选夹具升级为正式发行承诺”。依据 C.2。
3. **“本轮没有发现后者成立”的展开。** 已补分析：`CURRENT_STATUS`、D-17 和验收 A-07 的旧内容兼容是一般性条款，按 D-17 只对实际具名且验证过的发行组合承诺，不构成具名组合承诺；未找到的是具名组合与资料，不是已确认承诺缺资料。依据 C.2。
4. **§5 依据表。** 已补 `ADR 0046`、`plan.md` §13、`acceptance.md` §发布与迁移门禁、`s7a-8-4-closure.md`；并注明 S7A-8.4 的 owner 审批是 SDK version gate/API patch 审批，不是产品/内容发行审批。依据 A.1/A.3。
5. **L1 “人工写定”。** 已改为“报告标注为人工 golden；未提供独立预期文件，故只能作为交付时人工核对值，不能当作独立 oracle”。依据 B.3。
6. **`advanceGameplay` 描述。** 已补第二次调用：先 `advanceGameplay({8}, {8}, {8,0,0})`，再 `advanceGameplay({20}, {20}, {20,12,0})` 到 20。lead 的转述把位置写成 L1；核验报告 B.3/D.3 指出的实际对象是 L2 的 `playback_gameplay_contract_tests.cpp`，L1 是 Reference Host 的 `commands.txt` 命令序列、没有 `advanceGameplay`，因此只改 L2。依据 B.3。
7. **README 归属。** 已改为：README 的“compared byte-for-byte”只对 CXC goldens 成立；Chart 逐字节比较出处为 `tests/chart/chart_writer_tests.cpp:95`、`tests/chart/chart_migrator_tests.cpp:206,247`，Stage 2 golden 比较为 `chart_migrator_tests.cpp:96-104`。依据 B.6。
8. **`out/s7a78-oracle.log` 可取得性。** §3.2 与 §7 均已标为证据 zip 内的条目，并注明当前工作树不可得。依据 B.12。
9. **“内部候选 6/7”。** 已改为“内部证据 6/7（判定候选 4、格式夹具 2）”，正式发行身份限定为“仓库内 0/7”。依据 D.3。

**结论段第一句**已明确为“结论：部分成立”；所有“正式发行回归不适用”均加“就仓库内可得证据而言”限定（§1 定义并逐组合分类）。

**哈希同步。** 因新增引用 `docs/adr/0046-realtime-playback-coordination.md`、`docs/stage_reports/stages/stage-07/01-a/s7a-8-4/2026-10-10-s7a-8-4-closure.md`、`tests/chart/chart_writer_tests.cpp` 与 `docs/stage_plans/active/stage-07-rr/plan.md`，`hashes.txt` 由 70 行增至 74 行；新增哈希已写入，`commands.log` 追加对应命令与退出码。交回前发现 `docs/CURRENT_STATUS.md`、`docs/stage_plans/active/stage-07-rr/findings.md`、`docs/stage_plans/active/stage-07-rr/tasks/T-10.md`、`docs/stage_plans/active/stage-07-rr/tasks/T-13.md` 被 lead 的 T-01 并行落盘修改（不在本子代理写域），其哈希已按当前工作树重取；其余文件哈希未变。产品/测试/fixture 的哈希未受这些文档改动影响。

**边界。** 本轮仍只写入 `out/review/stage-07-rr/T-01/legacy-corpus/`；`git status --short` 显示 lead 对 `CURRENT_STATUS.md`、`findings.md`、`plan.md`、`T-10.md`、`T-13.md` 的修改和未跟踪的 `2026-11-23-t-01-dispatch-baseline.md`，均不在本子代理写域，本子代理没有修改这些已跟踪文件；本目录内只有 `inventory.md`、`hashes.txt`、`commands.log`、`inspected.md` 与抽取的 fixture 变化。除第 6 条的位置标注外，核验意见均可执行且未发现判断不当；第 6 条按实际代码归属改在 L2。
