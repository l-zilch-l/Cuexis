# T-03 RR-2 既有合同与诊断

状态：evidence

更新日期：2026-11-23

<!-- cuexis-meta
id: EVIDENCE-stage-07-rr.t-03-contracts-and-diagnostics
status: evidence
authority: none
date: 2026-11-23
-->

## 结论

- RR-F111（Packed CRC 前 92 字节算法）**成立**：合同、Writer 与 Reader 一致；本卡补了旧算法值必须报
  `packed.header.crc` 的对照用例，已在 candidate-debug 实测通过（Test #121），未改变算法或现有产物。
- RR-F112（TIME/REQ0 只接受 mode 0）**成立**：合同只声明 mode 0，Reader 对 TIME/REQ0 的非 0 mode 分别报
  `packed.time.invalid` / `packed.requirements.codec`，GridDelta 仍只由 primitives 测试覆盖。本卡补了
  TIME/REQ0 mode 1 的 envelope 保持对照（随 #121 通过），并写明 GridDelta 不在 wire 接受集。
- RR-F113（Chart Beat 上限）**成立**：v1–v3 生产 Reader 在约分后拒绝超限值；合同已改为引用
  `ChartLimits::maxBeatNumeratorMagnitude` 与 `maxBeatDenominator`，`rr2_contract_beat` 已实测 v1–v3
  边界通过（Test #170）。
- RR-F114（CXC playback encoding 按 profile 固定）**成立**：两份合同与 `cxc_candidate.cpp` 一致；
  静态 profile 只接受 `packed-chart`、Behavior profile 只接受 `source-json`，`rr2_contract_entry` 已实测
  两 profile 与交换编码拒绝（Test #326），普通 CXC v1 默认入口不变。
- RR-F115（CXT v2 Chart invocation 未实现）**成立**：`cxtImports` / `cxtInstances` 在实现与测试中 0 命中，
  §6 与 §12 已标未实现；本卡只在 §6 补了当前唯一的显式展开入口与 `tools/chart/chart_candidate` 的
  显式参数调用，没有新增 Chart 自动 import 路径。
- RR-F121（digest 版本入口）**部分成立**：公开 API 确实不收版本入参，宿主只能观察默认版本摘要；文档已把
  “宿主可观察默认版本摘要”与“内部定版本测试”分开。测试重复编译 `frame_digest_version.cpp` 的残余仍归
  T-04/RR-F214，本卡未运行 parity。
- RR-F122（`materializePresentation` 默认安装）**成立**：默认构建的安装头集合无条件包含
  `playback_session.hpp` / `presentation_candidate.hpp`，`materializePresentation` 不在候选宏块内，实现
  `playback_session_prepared.cpp:10-66` 也无条件编入并返回 `host.rpa.*`。本卡只做基线证据，不实施修复。
- RR-F131（入口分层）**部分成立**：原记录把普通规范化的 same-tick 归属与旧执行 profile 的 duplicate
  queue 首错混在一条；实际是普通规范化、旧执行 §8、RPA occurrence 三层各自首错。合同已写明分层并保留
  旧 profile 次序与 Replay 行为，生产输入逻辑未改；`rr2_contract_input`（Test #488）与两条 B3 RPA
  对照均已实测通过。
- RR-F132（身份映射）**部分成立**：原记录“`gameplay.fold.finite.v1` 应进 ruleset/session identity”把
  engine 算法标记与 ruleset/session 实际配置混为一条。实际实现同时写两层：算法标记进 engine revision，
  配置（identity token/build/module order/loadout）进 ruleset/session 分量；grade 后缀是
  `;grade.signed-interval.v1`。文档已按实际构造订正；旧 fixture 的十六进制身份因没有运行身份字节转储
  而未取得。
- RR-F135（观测桥错误分类）**部分成立**：2026-11-23 已接受映射真实存在，但旧实现仍把预建分配失败报成
  `host.rpa.capacity_exceeded`，部分 catch 吞成成功；本卡固定旧新映射表与 T-06 用例名，不实施、不重开裁定。
- RR-F142（Player 执行错误码）**成立**：`error()` 辅助把多类失败统一写成
  `player.execution.test_only`；`:632` 在 fault 自带码时透传。原记录所称“另有 7 个
  `player.execution.*` 码”实为 logger 通道名，不是错误码。该条归 T-15，本卡只列基线。

四个新增合同用例与两条 RPA 对照已由主代理在 candidate-debug 构建并实测通过（命令、注册号与退出码
见「执行记录」）；本代理未亲自运行这些命令。F135 五类故障注入、安装消费者、F132 旧身份字节等仍未
执行，分别由 T-06/T-08/T-10 承接。

## 范围与方法

实读输入：任务卡、计划与 D-25、登记册 §22/§23/§24/§15、Packed/Chart/CXC/Chart Entry/CXT v2/Gameplay
V2 Spec/执行 profile/帧摘要/宿主边界/执行类型、`schemas/cuexis.gameplay-diagnostics.v2.codes.json`、
`engine/chart`、`engine/cxc`、`engine/judgement`、`engine/playback`、`app/execution_host`、
`app/player` 的相关源码与 `tests/{chart,cxc,judgement,rpa}` 的既有用例、CMake 注册。

方法：以代码、schema 与 CMake 为权威，逐项把合同句子对到实现和测试；只订正与代码不符或缺少具名
事实的表述；测试只追加到卡面点名的既有文件，不新建测试文件。

基线漂移：派单给出的基线是 `3b8bf41e`；开读后实际 `HEAD` 为 `569253d8`（`docs(stage-7-rr): close the
T-13 cold-start recheck`），收尾时已到 `6c77a888`（并发提交 `03334ecd` 命名规则、`6c77a888` T-01
hash 订正）；`3b8bf41e` 是所有后续提交的祖先。开读期间工作树含主代理/其他代理的未提交改动，收尾时
它们已提交，工作树只剩本卡改动。本代理未提交、未切换分支，改动只落在本卡写域与
`out/review/stage-07-rr/T-03/`。该目录下既有的 `configure.*` / `build-tests.*` 日志非本代理生成；
本轮构建/CTest 结果以主代理回执为准，没有用这些日志替代回执。

未做：本代理没有运行 `cmake --preset`、构建、CTest、安装或故障注入；主代理随后在 candidate-debug
构建并运行了本卡四个用例与两条 RPA 对照，结果见下节。没有改 `engine/`、`app/` 的生产源码；没有改
`docs/CURRENT_STATUS.md`、`docs/HANDOFF.md`、`docs/MAP.md`、`AGENTS.md`、`docs/stage_plans/**`、
`docs/stage_reports/**`；没有 `git add` / `commit`。

## 执行记录

```text
python -B tools/check_docs.py
  exit 0
  Documentation checks passed: 785 Markdown files and 20 candidate JSON/CXT files validated.
  NOT CHECKED: CMake target facts are missing; set CUEXIS_TARGETS_FILE to fresh configure output
  NOTICE: Git protection checked against 6c77a888a5ac5e564393dc8c2e60b22fc1656d1f: 149 definitions
  NOTICE: source naming: 712 paths scanned, 4 frozen exceptions (cmake/Verify{S4G,S5H,CfuF3,CfuF4}*.cmake)

python -B tools/verification/check_docs_facts.py
  exit 0
  version-drift findings: 0 (current claims fail; see --list)
  documentation facts are current (SDK 0.9.1, build 26.11.22-1)

git rev-parse --abbrev-ref HEAD          -> stage-7-rr
git rev-parse HEAD                        -> 6c77a888a5ac5e564393dc8c2e60b22fc1656d1f（收尾时）
git merge-base --is-ancestor 3b8bf41e HEAD -> exit 0

只读检索（示例）：
  Select-String engine/chart/src/packed_chart_tables_writer.cpp -Pattern crc32
  Select-String engine/judgement/src/{input.cpp,execution_kernel_submit.cpp,execution_kernel_prepare.cpp}
  Select-String app/execution_host/frame_observation.{cpp,hpp}
  Select-String app/player/src/player_execution.cpp -Pattern 'error\('
```

上面是本代理执行的文档门禁与只读命令，本卡 docs 改完后两者均退出 0；`check_docs.py` 的
“CMake target facts missing”是缺少新鲜 configure 输出的既有提示，不是退出失败。

收尾重跑 `check_docs.py` 时退出 1，报错只有四条相对链接指向尚未创建的
`docs/stage_reports/stages/stage-07-rr/implementation/2026-11-23-t-03-contracts-and-diagnostics.md`
（`docs/CURRENT_STATUS.md`、`docs/HANDOFF.md`、`docs/stage_plans/active/stage-07-rr/findings.md`、
`docs/stage_plans/active/stage-07-rr/plan.md`）。这四个文件不在本卡写域，改动来自并发集成，本代理未
触碰；同一时点 `check_docs_facts.py` 仍退出 0。该失败不是本卡文档改动造成的。

### 构建与测试记录（主代理执行）

执行者：主代理；环境：MSVC 19.51 + `candidate-debug`；本代理未运行下列命令。结果由主代理回执，
本代理按回执记录。

```text
cmake --build --preset candidate-debug --target cuexis_chart_tests cuexis_cxc_tests cuexis_judgement_tests
  exit 0；三个测试可执行均链接成功

ctest --test-dir out/build/candidate-debug -N -R '^rr2_contract'
  Test #121  rr2_contract_packed
  Test #170  rr2_contract_beat
  Test #326  rr2_contract_entry
  Test #488  rr2_contract_input

ctest --test-dir out/build/candidate-debug --no-tests=error -R '^rr2_contract_packed$'   exit 0（1/1 passed）
ctest --test-dir out/build/candidate-debug --no-tests=error -R '^rr2_contract_beat$'     exit 0（1/1 passed）
ctest --test-dir out/build/candidate-debug --no-tests=error -R '^rr2_contract_entry$'    exit 0（1/1 passed）
ctest --test-dir out/build/candidate-debug --no-tests=error -R '^rr2_contract_input$'    exit 0（1/1 passed）

cmake --build --preset candidate-debug --target cuexis_rpa_tests
  exit 0
ctest --test-dir out/build/candidate-debug --no-tests=error -R '^B3 different original Ticks forwarded to one dispatch retain canonical occurrence order$'
  exit 0（1/1 passed）
ctest --test-dir out/build/candidate-debug --no-tests=error -R '^B3 occurrence duplicate and slot conflict are atomic and distinguish content$'
  exit 0（1/1 passed）
```

四个用例的断言在实测中全部通过，未为通过而放宽检查。F135 五类故障注入、安装消费者、ABI/安装面、
F132 旧身份字节与 digest parity 不在这次构建/CTest 范围内。

## 结果与证据

### 合同差异与订正（文档原文 → 代码事实 → 改法）

| 项 | 文档原文/原口径 | 代码事实（file:line） | 改法/状态 |
| --- | --- | --- | --- |
| RR-F111 | 旧文字曾被理解为“完整 96 字节 Header 置零后算 CRC” | Writer 对 `[0,92)` 计算并写 `[92,96)`（`engine/chart/src/packed_chart_tables_writer.cpp:557-561`）；Reader 以 `crc32(bytes.first(92))` 比较（`packed_chart_tables_inspect.cpp:73`，`decodeImpl` 先调 `inspectImpl`，`packed_chart_tables_decoder.cpp`） | 合同已正确，本卡不改；补 `rr2_contract_packed` 使旧算法值必须报 `packed.header.crc` |
| RR-F112 | §4.2 只列 mode 0 与两个拒绝码，未说 GridDelta codec 的测试-only 地位 | 非 0 mode 分别报 `packed.time.invalid`（`decoder_tables.cpp:192-197`）与 `packed.requirements.codec`（`decoder_model.cpp:340-344`）；`writeBeatDescriptor/readBeatDescriptor` 0 调用者（全仓检索） | 追加 GridDelta 仅测试覆盖的说明；补 TIME/REQ0 mode 1 对照 |
| RR-F113 | “实现额外拒绝 `|numerator| > 1e12` 与 `denominator > 1e9`”，未引用具名常量 | 上限来自 `limits.hpp:58-59`；判定在约分后，分子符号两侧同时受限（`canonical_chart_loader_primitives.cpp:317-331`） | 改为引用 `ChartLimits::maxBeatNumeratorMagnitude` / `maxBeatDenominator` 并说明约分后判定 |
| RR-F114 | 合同已按 profile 固定 encoding | `validateCandidateChartBytes` 先判 encoding：静态须 `packed-chart`，Behavior 须 `source-json`，交换即 `cxc.chart_entry.entry_invalid`（`engine/cxc/src/cxc_candidate.cpp:430-444`）；普通 CXC 默认入口不探测 candidate（同文件 `498-519`） | 合同无需改；补 `rr2_contract_entry` 与默认 CXC 可读断言 |
| RR-F115 | §6 已标“未实现”，§12 已收窄 | `cxtImports/cxtInstances` 全仓实现/测试 0 命中；显式展开入口为 `chart::CxtV2Loader::expand`，由 `tools/chart/chart_candidate` 传参调用 | 在 §6 补显式工具调用句；未新增自动 import 或展开入口 |
| RR-F121 | `:67` 把 parity 写成“需要时指定 digest version”，安装 API 不可达 | `computeFrameDigest(frame,snapshot)` 无版本参数，返回 `algorithmVersion`（`frame_digest.hpp:13-20`）；定版本实现只在 `src/frame_digest_internal.hpp` + `frame_digest_version.cpp`（不安装） | 文档拆成“宿主可观察默认版本摘要”与“内部定版本测试”两段 |
| RR-F131 | Spec §3.7.8 把 same-tick 归 `invalid_relation`；register 原记录称实现互换 | 普通规范化 `resolveDomains=false`：sequence dup → same subject（`same_tick_collision`）；旧执行 `resolveDomains=true`：sequence dup → duplicate queue → same-tick distinct → dispatch（`input.cpp:118-205`、`execution_kernel_submit.cpp:91-141`） | 合同写明三层入口与首错；旧 profile §8 与 Replay 行为保留；`execution_profile_tests.cpp` 既有断言未改 |
| RR-F132 | Spec 写 `gameplay.fold.finite.v1` 的实际配置进 ruleset/session；grade 算法写 `signed-interval/1` | 运行身份同时追加 engine 后缀 `;gameplay.fold.finite.v1` 与 `;grade.signed-interval.v1`（`execution_kernel_prepare.cpp:58-77`）；Ruleset 配置写 ruleset/session（同文件 `63-69`） | 三份文档补逐字段映射与后缀拼写；字段位置与后缀未改 |
| RR-F135 | 已接受 `host.rpa.allocation_failed` 与 capacity 的分工 | `frame_observation.hpp:230-231` 的预建 `allocationFailure_` 仍用 `host.rpa.capacity_exceeded`；`fault_` 无写入点；多个 catch 返回成功 | 在宿主边界与报告写同一张旧/新/未执行表；实现归 T-06 |
| RR-F142 | 原记录称 9 类失败统一 test_only、另有 7 个码 | `error()` 返回 `player.execution.test_only`（`player_execution.cpp:36-38`）；7 个 `player.execution.*` 字符串是 `logger.info` 通道（`:263,338,407,493,543,580,599`），不是错误码；`:632` 在 fault 有码时透传 | 只列基线并纠正“码”的措辞；归 T-15 |

### RR-F135 基线映射（与 `docs/api/realtime-host-boundary.md` 的 F135 小节同表）

| 场景 | 旧实现（当前触发与诊断） | 已接受目标 | 未执行验收（T-06 `rr_frame_observation_errors`） |
| --- | --- | --- | --- |
| 实际分配失败导致观测桥操作失败 | `copyPresentReceipt` 的 catch 返回 `host.rpa.capacity_exceeded`（`:72-73`）；`configure` 与 `create` 的 catch 同样报 `capacity_exceeded`（`:124-125`/`:288-289`）；`observe/expire/activate/query` 的 catch 返回预建 `allocationFailure_`，其码同为 `capacity_exceeded`（`:524-527`/`:549-552`/`:597-600`/`:613-614`，定义见 `frame_observation.hpp:230-231`）。 | 精确报 `host.rpa.allocation_failed`，并保留失败阶段；不得吞掉或误报 `capacity_exceeded`。 | [实际分配失败导致观测桥操作失败](../../../../stage_plans/active/stage-07-rr/tasks/T-06.md#任务级验收) |
| 真实预算/容量耗尽 | `feedback` 超出 `limits.bytes` 时置 `coverageLost()` 并返回成功空值（`:322-327`）；`publishImpl` 超出预算同样 `coverageLost()` 后返回成功（`:396-399`）；`configure` 的预算不足报 `profile_incomplete`（`:104-126`）。 | 真实预算/容量拒绝保留 `host.rpa.capacity_exceeded`；不得借用 `allocation_failed`。 | [真实预算/容量耗尽](../../../../stage_plans/active/stage-07-rr/tasks/T-06.md#任务级验收) |
| 非致命观测记录丢失 | `dropAttempt/dropFeedback/trim` 设置 unknown/历史丢失标记后继续（`:143-231`）；`feedback`/`publishImpl`/`recordAttempt` 的 catch 只 `coverageLost()` 并返回成功（`:335-337`/`:426-428`/`:447-449`），丢失不阻塞原执行，也不区分操作失败。 | 允许原执行继续，但必须留下不完整/Unknown 标记；不得据此证明 submit/present 成功。 | [非致命观测记录丢失](../../../../stage_plans/active/stage-07-rr/tasks/T-06.md#任务级验收) |
| 持续 OOM 下报告可用 | 预留故障槽字段 `FaultSlot fault_` 已声明（`frame_observation.hpp:173`、`:229`），但本文件没有写入点；`error()` 以字符串拼接构造 `host.rpa.*`（`:8-10`），`copyPresentReceipt` 复制 cause 链时仍分配（`:58-68`）。 | 复用既有 `FaultSlot` 的 allocation/capacity Reason；故障槽与错误交付路径在持续 OOM 下仍可用，预建 owning `core::Error` 的复制/移动也须验证。 | [持续 OOM 下报告可用](../../../../stage_plans/active/stage-07-rr/tasks/T-06.md#任务级验收) |
| submit/present 真实性及异常分类 | `presentOwnedFrame` 的 catch 返回 `PresentStatus::unknown` 与 `host.rpa.device_failed`（`frame_observation.hpp:113-117`）；`observe` 在 `copyPresentReceipt` 失败时记 `receiptLost` 后返回成功（`:486-491`），自身 catch 则返回预建 `allocationFailure_`（`:524-527`）。 | 操作未完成必须报告失败；纯观测丢失标 Unknown/不完整且不阻塞 Gameplay；不得伪报完成，也不得把其他异常统一改报 allocation_failed。 | [submit/present 真实性及异常分类](../../../../stage_plans/active/stage-07-rr/tasks/T-06.md#任务级验收) |

2026-11-23 接受记录见登记册 §23（RR-F135）；该表只固定映射，不表示实现或运行已通过。
**状态：已接受、未实现。** 合同修订落在 `docs/api/realtime-host-boundary.md` 的 F135 小节；生产修复与
故障注入移交 T-06 的 `rr_frame_observation_errors` 五个场景，本卡未执行。

### RR-F142 Player 执行失败基线

`app/player/src/player_execution.cpp:36-38` 的 `error()` 固定返回
`core::Error{"player.execution.test_only", text}`。该码没有登记在
`schemas/cuexis.gameplay-diagnostics.v2.codes.json`，也不在 `docs/guides/DIAGNOSTIC_CODES.md`。

| 触发类（行） | 诊断 | 可观察失败 |
| --- | --- | --- |
| 音频设备未打开/歧义/不可用/分配失败（`:76,88,93,102`） | `player.execution.test_only`，仅 message 区分 | `makeExecutionAudioFactory` 返回失败，无稳定子码 |
| 生产预算未接受（`:110`）、显式 testOnly `rpa.v1` 配置缺失（`:115`）、calibration 不匹配（`:120`） | 同上 | 执行入口拒绝，宿主/自动化只能读 message |
| 主音乐要求显式 Audio clock（`:139`） | 同上 | 以 `Failure` 抛出并最终转成 Result |
| Audio clock 未就绪（`:197`）、时钟观测失败（`:251`）、paused 位置溢出（`:237`） | 同上 | 对应阶段失败，无独立阶段码 |
| 主事件泵失败（`:348`）、初始呈现未确认（`:388`）、初始帧未执行（`:399`）、Pause 未发布实际 cut（`:412`）、paused 物理位置溢出（`:415`）、tail deadline 非法（`:444`）、advance 失败（`:557`）、最终呈现未确认（`:608`） | 同上 | 运行期失败并停在该阶段；stage 只在 message |
| deferred event 容量溢出（`:630`）、suspend fallback（`:632`）、trace 异常（`:639`，仅 trace harness）、catch-all 分配失败（`:642`） | 同上；`:632` 在 `fault` 已带码时透传原码 | 退出码/Result 失败；分配失败与协议失败不可区分 |
| 7 个 `player.execution.*` 字符串（`:263,338,407,493,543,580,599`） | 不是错误码 | 只是 `logger.info` 通道名，供日志过滤，不进入 Result |

可观察边界：调用方能看到“失败 + 文本”，看不到“哪一类失败”的稳定 token；`player.execution.test_only`
名字也表明它是测试用途。RR-F142 归 T-15；新增稳定码须先按 D-04 取得接受并同步集中码表与诊断测试。

### RR-F122 默认安装与宏头基线

- `engine/playback/CMakeLists.txt:59-74` 把 `playback_session.hpp`、`presentation_candidate.hpp` 等放入
  无条件 `PUBLIC FILE_SET HEADERS`；只有 `gameplay_candidate.hpp` / `execution_control.hpp` /
  `execution_coordination.hpp` 在 `CUEXIS_ENABLE_CHART_V5_CANDIDATE` 下追加（`:77-82`）。
- `PreparePlayback` 的 `materializePresentation` 声明在 `playback_session.hpp:250-254`，位于
  `CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE` 块（`:279`）之前；实现
  `src/playback_session_prepared.cpp:10-66` 无条件编入，错误码为 `host.rpa.*`。
- `CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE=1` 仅候选构建以 `PUBLIC` 定义（`CMakeLists.txt:47-54`）；默认构
  建安装同一个头但宏未定义，形成“同文件不同声明面”。该差异清单见
  `candidate-install-and-macro-diff.md`，交 RR-4/T-08 限定迁移。

### RR-F131 三层取证

- 普通规范化：`input.cpp:129-140` 先报 sequence duplicate；`:174-190` 在 `resolveDomains=false` 时对
  canonical 身份重复报 `same_tick_collision`；`:191-205` 只在 `resolveDomains=true` 时做同原 Tick
  不同 subject 检查。普通入口只追加有序 journal，不产生 pending receipt。
- 旧执行 profile：`execution_kernel_submit.cpp:91` 以 `resolveDomains=true` 调 `prepareIngressBatch`，
  `:99-141` 再做 route 与 dispatch 检查；`late.duplicate_queue_entry` 与
  `late.dispatch_collision` 分别来自 `input.cpp:177` 与 `execution_kernel_submit.cpp:130/136`。
- RPA：`submitCanonical` 走 `submitRpa`（`:16-33`、`:235-262`）；既有
  `tests/rpa/rpa_b3_schedule_tests.cpp:157` 与 `rpa_b3_checkpoint_tests.cpp:405` 覆盖 occurrence 槽
  共享 dispatch 与 duplicate/conflict 两码，两条对照已由主代理实测 1/1 通过。本卡不改 RPA 规则。

### RR-F132 身份 preimage 与 fixture 绑定

`engine/judgement/src/gameplay_assembler_assemble.cpp` 的写入次序：

1. `cuexis.judgement.identity.prepared.v1`
2. `engine`：`judgementSemanticRevision`、`factSemanticRevision`、`fixedPointTableId`、
   `coordinationPhaseOrderToken`、可选 `execution.engine.v1` + profile/late token（`:10-38`）
3. `ruleset`：`interfaceProjectionToken`、module order、`buildHash`（`:30-36`）
4. `chart` projection（`:85-93`）
5. `session`：`loadoutToken`、`defaultGraceSourceToken`、`normalizationProfileToken`、
   `judgementConfigToken`（`:41-49`）
6. 运行期 execution session 追加 `cuexis.judgement.identity.session.execution.v1`、mapping、late 参数与
   calibration（`:51-83`、`:95-112`）

旧 fixture 的原始 token 值绑定在 `tests/judgement/execution_test_fixture.hpp:21-24` 与
`tests/judgement/fold_recovery_fixture.hpp:51-102`；既有期望构造在
`tests/judgement/fold_recovery_seek_tests.cpp:341-383`。该期望调用
`makeRuntimePreparedIdentity`，是镜像实现，不能独立证明身份未变；十六进制旧字节必须由 T-10 在改动前
实跑取得。本卡在 `rr-f132-identity-binding.md` 给出逐字段对照与取字节命令，不改字段、不改后缀。

### 新增测试源（candidate-debug 实测通过）

| 测试名 | 注册目标/文件 | 覆盖 |
| --- | --- | --- |
| `rr2_contract_packed` | `cuexis_chart_tests` / `tests/chart/packed_chart_io_tests.cpp` | 同一可读样本、前 92 字节 CRC、旧 96 字节算法必须报 `packed.header.crc`、TIME/REQ0 mode 1 分别在 envelope/CRC 保持时报 `packed.time.invalid` / `packed.requirements.codec` |
| `rr2_contract_beat` | `cuexis_chart_tests` / `tests/chart/chart_loader_tests.cpp` | v1–v3 最简有理数在上限（分子正负两侧、分母）通过，上限加一报 `chart.beat.limit_exceeded` |
| `rr2_contract_entry` | `cuexis_cxc_tests` / `tests/cxc/cxc_candidate_bridge_tests.cpp` | 静态 profile+`packed-chart`、Behavior profile+`source-json` 各自通过；交换 encoding 报 `cxc.chart_entry.entry_invalid`；默认 CXC v1 仍可读且无 candidate 扩展 |
| `rr2_contract_input` | `cuexis_judgement_tests` / `tests/judgement/execution_kernel_oracle_tests.cpp` | 普通规范化的 distinct/duplicate/sequence 首错；旧执行 §8 的 sequence→duplicate→same-tick→dispatch；跨批 native/forwarded receipt、拒绝不推进不消费序号、序号可再用 |

CMake 注册：四个用例追加在既有已注册源文件内，由既有 `catch_discover_tests` 自动发现，故无需改
`tests/{chart,cxc,judgement}/CMakeLists.txt`；主代理的 `ctest -N -R '^rr2_contract'` 已发现
Test #121/#170/#326/#488，四个精确名称各自 1/1 通过。RPA 沿用现有 `cuexis_rpa_tests` 与两个既有
B3 用例，也各 1/1 通过。

### 文档改动

| 文件 | 改动 |
| --- | --- |
| `docs/formats/PACKED_CHART_FORMAT.md` | §4.2 补 GridDelta 只测试覆盖、不属于 wire 接受集；更新日期 |
| `docs/formats/CHART_FORMAT.md` | §3 改为引用具名 Beat 上限，说明约分后判定与分子符号两侧；更新日期 |
| `docs/formats/GAMEPLAY_V2_SPEC.md` | §3.7.8 后补三层入口分层；J0-5 补 engine 标记与 ruleset/session 配置的分工、grade 后缀；更新日期 |
| `docs/formats/CXT_V2_FORMAT.md` | §6 补当前唯一显式展开入口与 `tools/chart/chart_candidate` 的调用方式 |
| `docs/formats/gameplay-v2-execution-profile.md` | §8 补入口分层、整批拒绝与 Replay 行为；J0 补实际身份后缀；更新日期 |
| `docs/api/gameplay-v2-execution-types.md` | §2 补 engine 算法标记与 ruleset/session 配置的两层映射；更新日期 |
| `docs/api/frames-digests-and-timelines.md` | FrameDigest 条拆成宿主默认版本摘要与内部定版本测试；更新日期 |
| `docs/api/realtime-host-boundary.md` | F135 小节追加旧/新/未执行映射表与接受记录链接；更新日期（已是 2026-11-23） |

## 限制与未做

- 本代理没有亲自执行构建或 CTest；四个新用例与两条 RPA 对照由主代理在 MSVC 19.51 +
  `candidate-debug` 实测通过，命令、注册号与退出码见「执行记录」。本文不把主代理的回执写成我自己的
  运行结果。
- 仍未执行：F135 五类故障注入与观测桥修复验证（T-06）、安装消费者与 ABI/安装面（T-08/RR-4）、
  F132 的旧 hex 身份字节（T-10）、Packed 旧文件的外部互操作、digest parity（T-04）。
- RR-F132 的旧 hex 身份字节未取得（现有四用例不覆盖身份字节）；本卡只绑定 token/preimage 结构与
  取数命令，迁移对照必须由 T-10 在改字段前完成。
- 未验证 Packed 旧算法文件的外部互操作、真实外部消费者或 artifact 迁移；只验证合同与实现一致，并以
  已通过的测试固定“旧算法值必须被拒”。
- “为错误原因通过”的风险见下节；本轮没有为消除风险而修改或放宽任何断言。
- 文档门禁：本卡 docs 改完后 `check_docs.py`/`check_docs_facts.py` 均退出 0；收尾重跑时
  `check_docs.py` 因四份并发文件链接到尚未创建的
  `docs/stage_reports/.../2026-11-23-t-03-contracts-and-diagnostics.md` 而退出 1，该文件与链接方均不在
  本卡写域，详见「执行记录」。`check_docs_facts.py` 仍退出 0。
- 基线漂移：派单基线 `3b8bf41e`，收尾 `HEAD` `6c77a888`，`3b8bf41e` 是其祖先；开读期间的其他代理
  未提交改动已由各自提交，本卡未提交。
- `out/review/stage-07-rr/T-03/` 下既有的 `configure.*` / `build-tests.*` 日志非本代理生成；构建结果
  以主代理回执为准，没有用日志替代回执。

### “为错误原因通过”的风险

逐条回查诊断产生点后，未发现四例靠“另一个原因产生同码”通过：

- `rr2_contract_entry` 的交换 encoding 断言检查 `cxc.chart_entry.entry_invalid`；在
  `validateCandidateChartBytes` 内该码只在编码门 `:435-440` 产生并立即返回，函数内没有第二个产生点，
  因此通过只能来自该门。`chart.beat.limit_exceeded` 只在 `canonical_chart_loader_primitives.cpp:328`
  产生，`packed.header.crc` / `packed.time.invalid` / `packed.requirements.codec` 也各自唯一，未发现
  “任意失败即通过”的路径。
- 仍有前置条件偏松、建议后续加强的地方：`rr2_contract_input` 的复用序号只断言
  `dispatchTick==903`，没有断言 `wasForwarded==false`；建议补
  `CHECK_FALSE(reused->front().wasForwarded)`，把“native 复用”钉住。跨批结算只断言
  `receipts.size()==1`；建议再断言该 receipt 的 dispatch/key 属于原 pending，防止“数量对但对象错”。
  这两处本轮受“只改注释”限制未改，不影响当前通过结论。
- `rr2_contract_packed` 的旧 CRC 值与存储值不等由 `REQUIRE` 先固定，TIME/REQ0 两例各自先要求 envelope
  通过、再要求精确语义码，不依赖“任意失败”。

## 对登记册与合同的影响

- 登记册（由集成者写回，本卡不改 `findings.md`）：RR-F131 需追加“三层入口”分层纠正并保留原记录；
  RR-F132 需把“`gameplay.fold.finite.v1` 应进 ruleset/session identity”改为“engine 算法标记进
  engine revision、实际配置进 ruleset/session”；RR-F142 需把“7 个码”改为 logger 通道；RR-F111/112/
  113/114/115/121/122/135 的当前处置依据应指向本报告与新增测试名。四个用例与两条 RPA 对照已在
  candidate-debug 实测通过（Test #121/#170/#326/#488），登记册可引用这些注册号；F135 仍标未实现。
- 合同：本卡只改上述八个文件；CXC、Chart Entry 与其余页面未改，因为没有发现需订正的差异。
- 移交：F135 的五个运行验收归 T-06；F132 的身份字节迁移对照归 T-10；F122 与候选安装/宏头差异归
  RR-4/T-08；F121 的测试重复编译归 T-04/RR-F214；F142 归 T-15；Packed 双入口与其它错误表若后续出现
  新语义，按 D-04 另立接受记录，不在本卡重开。

## 证据附件

- [逐项 finding 处置](../evidence/t-03/finding-disposition.md)
- [candidate 安装与宏头差异](../evidence/t-03/candidate-install-and-macro-diff.md)
- [RR-F132 身份绑定与 T-10 取样命令](../evidence/t-03/rr-f132-identity-binding.md)
- [命令与改动文件清单](../evidence/t-03/commands-and-files.md)
- [candidate-debug 构建与 ctest 原始日志](../evidence/t-03/ctest-rr2.log)

