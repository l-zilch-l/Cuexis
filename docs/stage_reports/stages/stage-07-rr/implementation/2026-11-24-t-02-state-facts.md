# T-02 RR-1 状态事实对齐

状态：evidence

<!-- cuexis-meta
id: REPORT-stage-07-rr.t-02-state-facts
status: evidence
authority: none
date: 2026-11-24
-->


- 任务：`docs/stage_plans/active/stage-07-rr/tasks/T-02.md`（看板 task-17）
- 日期：2026-11-24
- 结论口径：成立 / 部分成立 / 不成立 / 无法核实；未验证项不写成已修复。

## 结论

| 检查项 | 判定 | 依据 |
| --- | --- | --- |
| RR-F001、RR-F002：`CURRENT_STATUS` 的版本与分支/PR 摘要 | 成立 | 现行正文无过期构建号与旧分支/PR 断言；版本只以链接指向 `cmake/CuexisVersion.cmake` |
| RR-F101、RR-F102：AGENTS 旧阶段叙述与 ROADMAP/V2 Spec 的 Stage 12 说法 | 成立 | `AGENTS.md` 0 命中；两处正文保留“2026-11-17 已删除”的带日期历史句 |
| RR-F103、RR-F424：诊断页与 V2 ABI 的 SDK API 字面量 | 成立 | 两页现行正文均为 `0.9.1`；`check_docs_facts.py --list` 0 条漂移 |
| RR-F104：`MATERIAL_SHADER` 的当前 SDK 与历史基线 | 成立 | 现行句写 `0.9.1` 并链 `CURRENT_STATUS`；`0.7.0` 只留在“历史基线”代码块 |
| RR-F105：Packed Chart v1 权威矩阵状态与可达测试 | 部分成立 | 矩阵已写 candidate、Reader/Writer 已实现；69 个 `[packed]` 用例中 68 通过、1 例因 T-04 源拆分后测试的源单元列表过期而红（见 §4） |
| 卡面任务级验收：`gen_docs_map.py`、`check_docs.py`、`gen_docs_map.py --check`、`check_docs_facts.py --list` | 成立 | 四项退出 0；隔离副本注入错误 SDK 字面量后 `--list` 精确报文件:行:值，退出 1 |
| 卡面“版本漂移在现有检查器中是警告”描述 | 部分成立 | 定位与反例输出成立；现行检查器把漂移计入 failures 并以退出 1 失败（`DOCUMENTATION_POLICY.md` §自动检查同）；“警告”一说不符合现行实现 |
| 看板 task-17 描述“烧录契约、clip 播放参数、未知参数拒绝、加载器阶段映射、规则发行说明与预算常量” | 不成立 | 与 `T-02.md` 卡面不符；卡面是唯一派单依据，本报告按卡面执行（见 §5.3） |

## 1. 基线与环境

开工时绑定：

```text
branch = stage-7-rr
HEAD   = 8cfb302dbcd88d7b9753d1962991ed8755449666
status = clean
```

执行期间主线在同一工作树提交了 `8b987c25`（`docs(stage-7-rr): correct the T-05 title in the handoff`，只改 `docs/HANDOFF.md`），并出现并发 engine/tests 修改（playback、runtime、world、render_opengl、`tests/playback`，含新增未跟踪文件）。这些修改不属于 T-02，本代理没有触碰。事实快照见 `logs/facts-commands.log` 与收尾的 `logs/final-gates.log`：

```text
branch: stage-7-rr
HEAD: 8b987c25f84c763d6ab568515b8ede709648276e
status: M docs/formats/README.md
        M engine/playback/src/... (并发任务)
        ...
```

本任务只改 `docs/formats/README.md`；其余改动来自主线/并发任务，不在 T-02 写域内。文档门禁在 `8b987c25` 的文档树上运行，结论不受并发 engine 修改影响。

预设选择：使用 `candidate-headless-debug`。理由：T-02 的 RR-F105 判定对象是 candidate Packed 实现与 `[packed]` 测试，该预设打开 `CUEXIS_ENABLE_CHART_V5_CANDIDATE` 且无窗口/设备依赖；本任务未改代码，`cmake --build --preset candidate-headless-debug --target cuexis_chart_tests` 返回 `ninja: no work to do`，测试二进制与测试源一致。

版本权威对照：

```text
cmake/CuexisVersion.cmake:1-4  26 / 11 / 22 / 1  -> 26.11.22-1
cmake/CuexisVersion.cmake:8    CUEXIS_SDK_API_VERSION = 0.9.1
vcpkg.json:3                   "version-string": "26.11.22-1"
```

`CuexisVersion.cmake:73-84` 在配置期读取 `vcpkg.json` 并拒绝两者不一致，因此日期构建只有这一个权威源。

## 2. 逐条事实表

原陈述取 `findings.md` 的定义行（保留其原字段）；历史依据取登记册的 `复核者`、`改动修订` 与 §22 处置，本轮不复核历史结论本身，只核对“现行正文是否仍与代码/版本权威一致”。

### 2.1 RR-F001（P0，版本摘要）

- 原陈述：`CURRENT_STATUS` 自称日期版本 `26.11.14-1`，实际版本文件已是 `26.11.16-1`。
- 当前状态：`docs/CURRENT_STATUS.md:55` 写“当前 SDK API 与日期构建身份见`版本权威`”，正文 `26.xx.xx-v` 字面量 0 条（命令见 `logs/facts-commands.log` 的 `RR-F001` 段）。
- 本次修改：无。T-18/ca5 已完成的订正保持有效；`CURRENT_STATUS` 由集成者维护。
- 历史依据：`findings.md:62`，复核者 Codex-G02（2026-11-23），`改动修订：ca5cbb91…`。
- 待后续取证：无文档侧缺口；主线若再次改 `CURRENT_STATUS`，需重跑 `check_docs_facts.py`。
- 判定：**成立**。

### 2.2 RR-F002（P0，分支与 PR）

- 原陈述：`CURRENT_STATUS` 自称执行分支 `codex/r4-closeout`、唯一集成 PR `#44` Ready for review。
- 当前状态：`docs/CURRENT_STATUS.md` 对 `codex/r4-closeout`、`Ready for review`、`PR #44`、`PR #45` 0 命中；`564e0d90` 与 `aac83e5e` 均为 `HEAD` 祖先（两 PR 已合并，与历史陈述一致）。
- 本次修改：无。
- 历史依据：`findings.md:63`，复核者 Codex-G02，`改动修订：ca5cbb91…`；`git merge-base --is-ancestor` 输出见 `logs/facts-commands.log`。
- 待后续取证：无文档侧缺口。
- 判定：**成立**。

### 2.3 RR-F101（AGENTS Stage 12 旧说法）

- 原陈述：`ROADMAP.md`、`GAMEPLAY_V2_SPEC.md` 仍称“AGENTS 写 Stage 12”。
- 当前状态：`AGENTS.md` 对 `Stage 12`、`Stage 7A 是当前`、`Stage 4-12` 0 命中；`docs/ROADMAP.md:48` 与 `docs/formats/GAMEPLAY_V2_SPEC.md:2605` 保留“2026-11-17 随 AGENTS 精简删除”的带日期说明，未再冒充现行指令。
- 本次修改：无（这两篇不在 T-02 写域；已由 T-18/ca5 订正）。
- 历史依据：`findings.md:74`；行号为旧登记值，现行位置已随文档增删漂移，报告保留旧值不作改写。
- 待后续取证：无。
- 判定：**成立**。

### 2.4 RR-F102（AGENTS 阶段状态旧说法）

- 原陈述：AGENTS 曾写“Stage 7A 是当前实施准备线”“Stage 4-12 都是 future/deferred”。
- 当前状态：`AGENTS.md` 上述短句 0 命中；现行文件只保留路由与硬约束。
- 本次修改：无。
- 历史依据：`findings.md:75`，复核者 Codex-G02。
- 待后续取证：无。
- 判定：**成立**。

### 2.5 RR-F103、RR-F424（SDK API 0.8.0 → 0.9.1）

- 原陈述：`docs/api/diagnostics-identity-and-compatibility.md`（旧行 22、64）与 `docs/api/GAMEPLAY_V2_ABI.md`（旧行 69）写 `SDK API 0.8.0`，权威为 `0.9.1`。
- 当前状态：
  - `docs/api/diagnostics-identity-and-compatibility.md:7` 适用版本 `SDK API 0.9.1`；
  - 同页 `:28` 当前 SDK `0.9.1` 兼容合同；`:70` 当前分支 SDK API `0.9.1`；
  - `docs/api/GAMEPLAY_V2_ABI.md:75` 当前分支安装包 `SDK API 0.9.1`；
  - `check_docs_facts.py --list` 对两页 0 条漂移（`logs/positive-gates.log`）。
- 本次修改：无；旧字面量已由 T-18/ca5 订正，本轮只做对照与门禁复核。
- 历史依据：`findings.md:76`（F103）、`:267`（F424）；复核者 Codex-G02，`改动修订：ca5cbb91…`。同页 `:71` 的 `0.7.5` 是兼容历史说明，不在 `SDK API` 字面量检查范围。
- 待后续取证：两页只订正“当前 SDK”文字；不表示 SDK 发行或运行验收通过（登记册理由字段已写明）。
- 判定：**成立**。

### 2.6 RR-F104（MATERIAL_SHADER 0.7.5 → 0.9.1）

- 原陈述：`docs/formats/MATERIAL_SHADER.md` 旧行 632 写“当前分支为 0.7.5”，同期 `CURRENT_STATUS` 同错。
- 当前状态：`:633` 写“下表 SDK 版本记录 S5-C 公开类型引入时的历史基线；当前 SDK API 为 0.9.1，见 CURRENT_STATUS”；`:636` 的 `CUEXIS_SDK_API_VERSION: 0.7.0` 位于该历史基线代码块内。`CURRENT_STATUS` 的旧字面量已不存在（同 §2.1）。
- 本次修改：无。
- 历史依据：`findings.md:77`，复核者 Codex-G02；文件头部状态仍为 accepted，未被本轮改写。
- 待后续取证：无。
- 判定：**成立**。

### 2.7 RR-F105（Packed Chart v1 权威矩阵）

- 原陈述：`docs/formats/README.md` 旧行 40 把 Packed Chart v1 标为“未实现”；实际 Header/目录、字典/身份、分节编码、语义 hash、Reader/Writer 与 40k 测试已实现并接入 CMake。
- 当前状态：
  - `docs/formats/README.md:47` 已写 candidate、Reader/Writer 已实现、默认 Playback/Player 入口未消费、`CUEXIS_ENABLE_CHART_V5_CANDIDATE` 默认 OFF，并回指 RR-F105；
  - `engine/chart/CMakeLists.txt` 的闭包为 `cuexis_chart`（基础 Reader/v4）、`cuexis_chart_codec`（Packed Reader/原语/身份）、`cuexis_chart_tools`（CXT2/BEHAVIOR/Writer），Reader/Writer 分属 `InternalChartCodec`/`InternalChartTools`，与 T-04 报告一致；
  - `docs/formats/PACKED_CHART_FORMAT.md:3` 写“candidate；v1 wire 已实现（Reader/Writer 仅 mode 0）”，与 RR-F112 的既有接受集一致；
  - `tests/chart` 中 `[packed]` 用例 69 个，见 §4。
- 本次修改：`docs/formats/README.md:5` 更新日期改为 2026-11-24；`:47` 把“Reader/Writer 与 68 条测试已实现”改为“Reader/Writer 已实现并被 69 个 `[packed]` 测试用例覆盖”。原因：ca5 时 `[packed]` 用例恰为 68（源文件扫描口径），T-03 新增 `rr2_contract_packed` 后为 69（`tests/chart/packed_chart_io_tests.cpp`，提交 `b46ce396`），原计数已过期。
- 历史依据：`findings.md:78`，复核者 Codex-G02，`改动修订：ca5cbb91…`；登记册只关闭“文档问题”，未重跑 Reader/Writer 行为。
- 待后续取证：
  1. `[packed]` 测试当前 68/69 通过，1 例源文本断言因 T-04 源拆分过期（§4.2），需主线修复测试后重跑；
  2. Packed 真实外部消费者/互操作仍未核实（RR-F111、RR-F114 仍开放，T-09 按证据核退出）；
  3. 计数为 2026-11-24 快照，后续新增 `[packed]` 用例时需同步本行。
- 判定：**部分成立**（文档状态与实现陈述成立；可达测试未全绿，“既有验证状态”不能写成全部通过）。

## 结果与证据：文档门禁与阴性反例

全部在仓库根目录执行；日志 `logs/positive-gates.log`、`logs/negative-facts.log`。

```text
python -B tools/verification/gen_docs_map.py
  -> wrote docs/MAP.md (270 lines) and AGENTS.md state block
  -> exit 0；git status 不显示 docs/MAP.md / AGENTS.md，生成内容无变化

python -B tools/check_docs.py
  -> Documentation checks passed: 798 Markdown files and 20 candidate JSON/CXT files validated.
  -> source naming: 扫描 724 个路径段，0 个遗留例外
  -> exit 0

python -B tools/verification/gen_docs_map.py --check
  -> generated documentation is current (779 documents)
  -> exit 0

python -B tools/verification/check_docs_facts.py --list
  -> version-drift findings: 0 (current claims fail; see --list)
  -> documentation facts are current (SDK 0.9.1, build 26.11.22-1)
  -> exit 0
```

隔离副本（`out/review/stage-07-rr/T-02/negative/`）的基线先复核通过，再在 `docs/api/diagnostics-identity-and-compatibility.md` 现行正文第 8 行插入：

```text
- 阴性对照（T-02 隔离副本）：SDK API `0.8.0` 为注入值，仅用于验证 check_docs_facts.py 能定位文件、行和值。
```

结果（`logs/negative-facts.log`）：

```text
docs/api/diagnostics-identity-and-compatibility.md:8: found 0.8.0; SDK API is 0.9.1
version-drift findings: 1 (current claims fail; see --list)
FAIL: current documentation contains version drift; see --list (historical values need local context)
exit=1
```

文件、行、值与权威对照全部命中；退出码是 1。卡面把该漂移描述为“警告”，现行检查器按失败处理；这处差异已按事实记录，未改检查器也未改卡面。

收尾复跑（`logs/final-gates.log`，HEAD `8b987c25`）：`check_docs.py`、`gen_docs_map.py --check`、`check_docs_facts.py --list` 仍全部退出 0，漂移 0；`check_docs.py` 的 source naming 扫描为 733 个路径段、0 个遗留例外（比 02:07 的 724 多出的路径来自并发任务新增文件）。

## 4. RR-F105 的 Packed 子集、测试与回归

### 4.1 实现子集

| 目标 | 内容 | 位置 |
| --- | --- | --- |
| `cuexis_chart`（`Cuexis::InternalChart`） | 旧 Reader、CXT1、v4 loader/resolver/runtime、`chart_writer.cpp` | `engine/chart/CMakeLists.txt:1-60` |
| `cuexis_chart_codec`（`Cuexis::InternalChartCodec`） | Packed Reader、primitives、tables common/decoder/inspect、profile、semantic identity | `engine/chart/CMakeLists.txt:79-121` |
| `cuexis_chart_tools`（`Cuexis::InternalChartTools`） | CXT2、candidate lowering/assembler、Behaviour、Writer 与文件 I/O | `engine/chart/CMakeLists.txt:123-174` |

Reader/Writer 的已实现接受集按登记册与 `PACKED_CHART_FORMAT.md` 为 mode 0；默认 Playback/Player 不链接 codec/tools（T-04 闭包证据），`CUEXIS_ENABLE_CHART_V5_CANDIDATE` 默认 OFF（`CMakeLists.txt:50-51`）。

### 4.2 可达测试状态

注册确认：

```text
ctest --test-dir out/build/candidate-headless-debug -N -R "rr2_contract_packed|40000"
  -> 5 条，含 Test #164 rr2_contract_packed、Test #119 R4 the 40000 entity profile round-trips…
  -> exit 0
```

实际运行：

```text
out/build/candidate-headless-debug/bin/cuexis_chart_tests.exe "[packed]"
  -> test cases:    69 |    68 passed | 1 failed
  -> assertions: 40902 | 40901 passed | 1 failed
  -> exit 42（Catch2 测试失败码），16.0 s
```

唯一失败用例是 `SPEC-02 wire-range representability has its own code, not a budget code`，失败点是 `tests/chart/packed_budget_tests.cpp:614` 的 `REQUIRE(messageAt != std::string::npos)`。原因是该用例按硬编码的四个源单元拼接文本（`:589-591`：writer/sections/inspect/decoder）后搜索 `Packed field exceeds the uint32 wire range`；T-04b 提交 `5dc55387` 把该 guard 移入新单元 `packed_chart_tables_common.cpp:33`，四个源单元列表未同步。ca5 时该字符串位于 `packed_chart_tables_writer.cpp:39`，因此在拆分前通过。这是测试的源布局假设过期；该失败不触及 Reader/Writer 行为。其余 68 个用例（含 40k round-trip）全部通过。

建议修复（不在 T-02 写域，交主线）：把 `packed_chart_tables_common.cpp` 加入 `packed_budget_tests.cpp:589-591` 的 `units` 数组，或改为扫描 `engine/chart/src` 下全部分节单元；修复后重跑同一命令，预期 69/69、退出 0。修复前不要在报告或状态页写“Packed 测试全部通过”。

## 5. 卡面与现状冲突记录

### 5.1 版本漂移“警告”描述

卡面任务级验收说“版本漂移在现有检查器中是警告”。现行 `tools/verification/check_docs_facts.py:262-268` 把 findings 追加进 failures 并返回 1；`docs/DOCUMENTATION_POLICY.md:234` 也写“当前版本声明漂移导致失败”。反例定位输出与卡面一致，退出语义按现行检查器记为失败。未修改检查器或卡面。

### 5.2 `findings.md` §8.1 DOC-01 行

`findings.md:307` 仍写“`PACKED_CHART_FORMAT.md:3` 仍写‘未实现’”。该文件现行 `:3` 已是“candidate；v1 wire 已实现（Reader/Writer 仅 mode 0）”（T-03/ca5 已订正）。这是登记册内一条过期旁注，按写域由集成者处理（建议见 §7.2）。

### 5.3 看板 task-17 描述与卡面不符

看板描述列为“烧录契约、clip 播放参数、未知参数拒绝、加载器阶段映射、规则发行说明与预算常量”；`T-02.md` 卡面是 RR-1 状态事实对齐，写域只有 `docs/formats/README.md`、`docs/formats/MATERIAL_SHADER.md`、`docs/api/diagnostics-identity-and-compatibility.md`、`docs/api/GAMEPLAY_V2_ABI.md` 中 RR-F101–F105 所涉状态与版本摘要。本代理按卡面（唯一派单依据）执行，未按看板描述改 engine/tests。看板描述建议由主线更正，避免下一执行者误解。

## 6. 改动清单

本代理改动：

- `docs/formats/README.md`：更新日期 2026-10-05 → 2026-11-24；Packed Chart v1 行的测试计数 68 → 69 并改为 `[packed]` 用例表述。
- `out/review/stage-07-rr/T-02/`：本报告、`logs/` 四份日志、`negative/` 四个 fixture。

未改：engine、app、tests、schemas、CMake、其它 docs（含 `CURRENT_STATUS`、`ROADMAP`、`findings`、`plan`、`schedule`、`HANDOFF`、`BUILDING`）。未 `git add`/`commit`，未跑设备。

## 7. 主线回写建议

### 7.1 定点差异

- `docs/CURRENT_STATUS.md`：无需为本卡改动；现行 `:55` 已指向版本权威，旧版本/旧分支断言已清除。若主线在 T-02 完成时写进度，建议只写“T-02 完成状态事实对齐”并链本报告，不复述版本号。
- `docs/ROADMAP.md`：无需改动；`:48` 的带日期历史句保留。
- `docs/stage_plans/active/stage-07-rr/findings.md`：三处建议（均属集成者写域）：
  1. `:307` DOC-01 旁注已过期，建议加带日期的订正：`PACKED_CHART_FORMAT.md:3` 现为 candidate + v1 wire 已实现（T-03/ca5）。
  2. `:424-425` §9.3 的“35 条版本字面量漂移”是 2026-11-17 历史记录；当前 `--list` 为 0，建议保持原记录并在其后加日期订正，不覆盖原文。
  3. RR-F105 的 `实际验证` 建议补一句：README 行已把测试计数改为 69（T-02，2026-11-24）；`[packed]` 当前 68/69，通过源文本用例 `SPEC-02` 因 T-04 源拆分过期而红，另立 finding（建议编号 RR-F548，以主线分配为准），责任归 T-04 后续修复。

### 7.2 生成物复跑清单

本卡只改索引正文，不改变文档状态/标题/权限，`docs/MAP.md` 与 `AGENTS.md` 状态块无内容变化（已实测 `gen_docs_map.py` 后 `git status` 不显示这两个文件）。汇合后仍按卡面顺序复跑：

```text
python -B tools/verification/gen_docs_map.py
python -B tools/check_docs.py
python -B tools/verification/gen_docs_map.py --check
python -B tools/verification/check_docs_facts.py --list
```

四项都应退出 0、`version-drift findings: 0`。若主线同期改动 `CURRENT_STATUS` 或 `HANDOFF`，先重新生成再跑后两项。

### 7.3 卡面状态

建议主线把 `docs/stage_plans/active/stage-07-rr/tasks/T-02.md` 标记完成并链接本报告；`schedule.md`/`plan.md` 的进度由集成者统一回写。

## 限制与未做

- 未修 `tests/chart/packed_budget_tests.cpp` 的源单元列表：不在 T-02 卡面写域；已给出一行修复建议与复跑命令。
- 未构建除 `candidate-headless-debug` 外的预设：本任务无代码改动，chart 测试二进制在增量构建中已确认无工作可做；其它预设不增加本卡结论。
- 未跑设备/hosted：按任务约束与卡面范围。
- 未改合同语义、接受集、稳定码或版本政策：无此类需求；触发时按停止条件冻结并交 finding。
- 未验证 Packed 真实外部消费者与 Writer 互操作：RR-F111/F114 仍开放，归 T-09 按证据核退出。

## 9. 停止条件核对

本卡未触发停止条件：没有改变接受集、稳定码或版本政策；文档门禁没有新增归属本卡的漂移（0 条）。发现的两处现存问题（`[packed]` 源文本用例红、findings DOC-01 旁注过期）都按写域交给主线，未擅自改合同或测试。
