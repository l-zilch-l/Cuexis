# R9：Reference Host 命令循环与 play/pause（实施文档）

状态：active；设计已获 owner 接受，**已实现并经本地与最终 SHA 验证**
（最终 SHA `71de8b1` 的 hosted 四工作流全绿，证据见
[R9 报告](../../../stage_reports/stages/stage-06/2026-09-28-r9-reference-host-command-loop.md)）；
**尚未获 owner 接受退出**，`SPEC-27` 仍为 open。本文是 R9 的唯一规范来源

更新日期：2026-09-29（订正：本行原写「**未实现**」与更新日期 `2026-09-28`，二者均为实现落地前的
状态。§9 要求在退出时按政策更新状态，且**不得把 owner 接受设计写成已接受最终退出**——本行据此更新，
而退出状态**仍未达成**，故状态词保持 `active`）

基线：`670cca8`（PR #30 合并后的 `master`；分支 `stage-06-r9-reference-host-command-loop`）

替代：[stage-06-review-remediation/plan.md](plan.md) 只引用本文，不复制本文常量

---

## 0. 本文的地位与阅读顺序

本文是 R9 批次的**命令语法、状态机、限额、计数、错误语义、验收判据与门禁接线的单一权威来源**。
实现常量只在宿主私有头定义一次；主 remediation 计划只引用本文；README 是使用说明、报告是证据，
两者均不得另定合同。

**给未来的自己（上下文可能已被压缩）**：先读 §1（边界）与 §2（基线代码地图）——§2 记录了
`670cca8` 上实测的文件、行锚点与**逐条硬约束**，是本批次最容易丢失的部分。然后读 §3（合同）、
§6（摘要裁判）、§7（用例）、§9（门禁接线）。§11 是执行顺序。

### 0.1 R9 要解决的唯一问题

Stage 6 复核发现项 **SPEC-27**：ADR 0042 `:350-351` 冻结的命名宿主契约要求宿主
「自己拥有命令循环、内存 ContentProvider 和可控 HostClock；支持 open/play/pause/seek/reload/quit」，
而实现 `examples/reference_host/` 是**脚本式宿主**——`main.cpp` 只解析 argv 后跑一段固定序列，
六动词中 **play / pause / quit 无法被外部下达**。

R9 交付：一个**有界命令文件驱动、有状态分发、无 stdin** 的命令循环，使六动词全部可下达，
且 `pause` 可被反证。**ADR 冻结正文不修改。**

### 0.2 SPEC-27 的关闭标准（owner 已接受）

「确定性 headless play 与本方案 H1–H7 足以作为 **SPEC-27 本批次功能验收口径**；不等于问题已关闭。」
关闭还要求：owner 批准语义 + H1–H7 实测/变异 + 旧契约未退化 + 最终 SHA 门禁 + 新报告/追加订正 + owner 接受退出。

---

## 1. 决策记录与边界

owner 于本轮接受 R9 设计，并**已手动合并 PR #30**（合并提交 `670cca8`）。

| 决策 | 已接受口径 |
| --- | --- |
| 载体 | R9 从合并后 `master` **独立开分支、独立 PR**，不扩展 PR #30 |
| 命令来源 | 有界命令文件驱动、有状态分发、**无 stdin**、无实时墙钟 |
| play/pause | 宿主 transport 控制普通 tick；`pause` 冻结 `chartTimeMs`；恢复不补偿、不增加 `discontinuityId` |
| tick | 允许作为**第七个宿主测试调度输入**；不修改公共 SDK 或内容脚本能力 |
| 首帧 | 用户模式只由 Playing 下 `tick N>0` 或 `seek` 产生；`open`/`play` 均不产生首帧 |
| open | 仅首次加载；重复 `open` 拒绝，不以 `open` 操作替换或卸载活动内容 |
| reload | 已 open **且已有成功 update 帧**；原地 `KeepChartTime`；暂停中显式 `reload` 允许内部采样 |
| pause 例外 | 暂停中显式 `seek` 合法；满足前置条件的 `reload` 合法；**普通 tick 全部抑制** |
| 错误 | 语法执行前拒绝；运行错误 fail-fast、统一清理、非零退出；不恢复、不自动重试 |
| 主门禁 | 既有根 CTest staging 门禁内 `execute_process` 直接运行宿主及报告断言；**不新增嵌套 CTest 调用** |
| 摘要裁判 | 复用既有绝对 golden 锚点 + 精确帧轨迹/关系断言；**不录制新 golden** |
| 旧 flag | 命令模式拒绝 `--expect-digest`；legacy 语义保持 |

### 硬边界

- R0–R8 `completed` 不重开；Stage 6 归档状态不变；R9 只处理 SPEC-27。
- **不改 ADR 0042 冻结正文**、SDK 安装公共头/API/枚举、SDK API `0.7.0`。
- 不触碰 Stage 7A/8/9–12 交付物；不实现音频 pause、真实时间播放、输入/判定/回放、运行时内容脚本。
- **不修改 golden**、不删旧门禁、不改变失败回滚、不扩大公共头 allowlist。
- 历史报告**只追加**订正与后续证据链接；不覆盖旧现象与验证结果。

---

## 2. 基线代码地图（在 `670cca8` 上实测）

> 本节是防上下文压缩的核心。**每个锚点都在 `670cca8` 上实查过**；实现时若行号漂移，按名字定位。

### 2.1 文件清单与规模

| 文件 | 行数/大小 | R9 是否改动 |
| --- | --- | --- |
| `examples/reference_host/CMakeLists.txt` | 51 行 | **改**（源文件清单） |
| `examples/reference_host/README.md` | 6788 B | **改**（命令模式文档） |
| `examples/reference_host/src/main.cpp` | 5892 B | **改**（`--command-file` 解析） |
| `examples/reference_host/src/host_runner.cpp` | 17121 B / 392 行 | **改**（dispatcher + 共享动作） |
| `examples/reference_host/src/host_runner.hpp` | 987 B | **改**（`HostOptions` + 命令模式选项） |
| `examples/reference_host/src/host_report.hpp` | 1184 B | **改**（新事件字段） |
| `examples/reference_host/src/host_report.cpp` | 1555 B | 可能改 |
| `examples/reference_host/src/host_content.hpp` | 2220 B | 只读（固定资产表） |
| `examples/reference_host/src/host_content.cpp` | 7481 B | 只读 |
| `examples/reference_host/src/host_commands.{hpp,cpp}` | 新增 | **新建**（解析/类型化命令） |
| `examples/reference_host/src/host_clock.{hpp,cpp}` | 新增 | **新建**（transport + 有界时间） |
| `examples/reference_host/tests/commands/*.txt` | 新增 | **新建**（命令 fixture） |
| `cmake/VerifyReferenceHostCommands.cmake` | 新增 | **新建**（case runner） |
| `cmake/VerifyReferenceHost.cmake` | 591 行 | **改**（include 新 runner） |
| `tools/check_stage6_a2.py` | 402 行 | **改**（增量检查 + 注释订正） |
| `docs/stage_plans/README.md` | — | **改**（索引链接） |
| `docs/stage_plans/reviews/stage-06-review-remediation/plan.md` | 468 行 | **改**（追加 R9 行与链接） |

### 2.2 现存控制流（`host_runner.cpp`，392 行）

```
:15  namespace cuexis_reference_host {
:16  namespace {
:30    struct ClockStep final { double chartTimeMs; double simulationDeltaTimeMs; uint64_t discontinuityId; };
:36    constexpr std::array<ClockStep, 4> scriptedSteps{          // 这就是既有的 "seek"
:37        {0.0,    0.0, 0},   // 首帧，dt=0，id=0
:38        {625.0,  0.0, 1},   // id 跳变 → seek
:39        {250.0,  0.0, 2},   // 时间回退 → seek
:40        {1250.0, 0.0, 3}};  // 时间前进 → seek
:43    stateName(SessionState) -> string_view                  // Empty/Ready/Running/Failed
:61    struct FrameObservation final { RuntimeFrame frame; uint64_t digest; size_t objects; bool seek; };
:70    consumeStep(PlaybackSession&, const ClockStep&, bool seek, HostReport&, size_t index)
         -> optional<FrameObservation>                            // ★ 唯一的 SDK 推进路径
:124   runFrameScript(PlaybackSession&, const HostOptions&, HostReport&, string_view stage)
         -> optional<vector<FrameObservation>>
:161   sameDigests(const vector<FrameObservation>&, const vector<FrameObservation>&) -> bool
:174  } // namespace
:176  auto runHost(const HostOptions& options, HostReport& report) -> int {
         // 线性 6 阶段，任一步失败 early return 1：
         //   start → load/commit → frames/digest → reload（重跑脚本，digest 必须逐项相同）
         //   → reload-failed（故障 provider 必须被拒） → package（unload + fromCxcFile）
         //   → destroy（回到 Empty） → verify
:390  }
:392  } // namespace cuexis_reference_host
```

**`consumeStep`（`:70-101`）是唯一 SDK 推进路径**，逐字如下（R9 必须复用它，不得复制一套）：

```cpp
const RuntimeFrame frame{.chartTimeMs = step.chartTimeMs,
                         .simulationDeltaTimeMs = step.simulationDeltaTimeMs,
                         .timeDiscontinuityId = step.discontinuityId};
const auto stepName = seek ? "seek" : "advance";
auto updated = session.update(frame);                      // ← 唯一 update
if (!updated) { report.failure(stepName, updated.error().code()); return std::nullopt; }
auto snapshot = session.extractFrame(FrameViewport{.width = 1280, .height = 720});
if (!snapshot) { report.failure(stepName, snapshot.error().code()); return std::nullopt; }
auto digest = cuexis::playback::computeFrameDigest(frame, *snapshot);
if (!digest || digest->algorithmVersion != 3U) { report.failure(stepName, "frame digest unavailable"); return std::nullopt; }
report.event("frame",
             "index=" + ... + " mode=" + stepName +
             " chartTimeMs=" + ... + " discontinuityId=" + ... +
             " objects=" + ... + " digest=" + ... + " algorithm=" + ...);   // ★ 字段顺序是冻结的
```

**`runFrameScript`（`:124-159`）**：先按 `scriptedSteps` 跑 4 步（`seek = index > 0 && id 与前一步不同`），
再用宿主自持循环 `chartTimeMs += 250.0`（`discontinuityId` 不递增）跑 `options.advanceFrames` 步（默认 4）。
**这个 `+= 250.0` 循环就是 R9 的 `play`+`tick` 雏形**，`scriptedSteps` 的 id 跳变就是 R9 的 `seek`。

**`runHost`（`:176-390`）** 是线性 6 阶段函数，无命令分发能力——**R9 必须把它重构成
`HostContext` + 顺序 dispatch**，不能只在外面套一层名义 dispatcher（否则无法在两次推进之间处理 `pause`）。

### 2.3 现有 CLI（`main.cpp`）

usage 在 `:21-29`，解析循环在 `:85-158`，用 `argument == "--xxx"` 字面比较；
未知参数 → 打 usage + `return 1`。七个 flag：`--content` `--package` `--advance`
`--expect-identity` `--expect-digest` `--report` `--help`（`--help`/`-h` 在 `:94`）。

### 2.4 ★ 门禁硬约束（逐条实测，违反即红）

#### (A) `tools/check_stage6_a2.py:294-377` — `check_reference_host_contract()`

**全部是正向断言，所以纯追加式改动不会打红它；但下列条款限定了重构的自由度：**

| 行 | 断言 | 对 R9 的含义 |
| --- | --- | --- |
| `:304-310` | 9 个文件必须存在 | 追加新文件安全 |
| `:316-322` | 存在**行首**且未注释的 `find_package(Cuexis ` | 不得改动 |
| `:323-330` | 存在 `target_link_libraries(...Cuexis::Playback)` 整行 | 不得改动 |
| `:331-332` | `Cuexis::Playback` 出现 | 不得改动 |
| `:334-336` | **除 `target_include_directories(cuexis_reference_host PRIVATE src)` 外不得再有 `target_include_directories`** | ★ 不能为新文件加第二个 include 目录 |
| `:337-338` | 无 `engine/`；`find_package` 之前无 `EXPORT` | 不得改动 |
| `:345-348` | **7 个 flag 必须以 `argument == "<flag>"` 出现在 `main.cpp`** | ★ 7 个旧 flag 的比较语句必须留在 `main.cpp`，不能搬到新文件 |
| `:354-364` | 9 个 stage 名（`start/load/commit/frames/digest/reload/reload-failed/active/package`）必须能经 `report.event\|failure\|rejection("<name>"` 从 **`host_runner.cpp`** 到达；`load`/`reload`/`package` 还需**同时**有 `report.failure("X"` 与 `report.event("X"` | ★ **文件名被硬编码 4 次**——把 dispatcher 拆到新文件会打红；这 9 个 stage 与 `load`/`reload`/`package` 的双路径必须留在 `host_runner.cpp` |
| `:365-368` | `buildProjectSource`、`prepareReload`、`commit(` 必须在 **`host_runner.cpp`** | 同上 |
| `:374-377` | ADR 0042 正文仍含 `` Stage 6 SDK 目标冻结为 `0.7.1` `` 与 `examples/reference_host/` | 不得改 ADR |

**R9 的增量（实施时加）**：新入口、私有源文件接线、case runner 调用、必跑 case 声明。

**★ 一个已发现的缺口（R9 应修）**：`check_stage6_a2.py:299` 的 docstring 自称本检查断言宿主
「**owns its command loop**」，但下面的检查项**没有一条验证命令循环**——只验证了 argv flag 面。
这正是 SPEC-27 能在门禁全绿下存活的机械原因。R9 的 A2 增量应补上真实表征，
并订正 `:340-344` 的注释（它把脚本式宿主写成了冻结事实）。

#### (B) `cmake/VerifyReferenceHost.cmake:70-103` — 源码卫生（**新文件自动落入检查**）

```cmake
:74  file(GLOB host_sources "${host_source_dir}/src/*.cpp" "${host_source_dir}/src/*.hpp")
:78  foreach(source IN LISTS host_sources)
:80      if(contents MATCHES "#[ \t]*include[ \t]*\"\\.\\.")         → FATAL  禁止 ../ 相对包含
:83      string(REGEX MATCHALL "#[ \t]*include[ \t]*[<\"]cuexis/[^>\"]+[>\"]" includes ...)
:85      if(NOT include_line MATCHES "cuexis/playback/")             → FATAL  只允许 cuexis/playback/ 头
:92  if(host_cmake MATCHES "Cuexis::(Internal|Core|Content|Audio)" OR "cuexis_cxc" OR "player_support") → FATAL
:97  if(host_cmake MATCHES "add_subdirectory(...CUEXIS_SOURCE_DIR" OR "CUEXIS_SOURCE_DIR") → FATAL
:101 if(NOT host_cmake MATCHES "find_package\\(Cuexis")               → FATAL
```

★ **`host_commands.{hpp,cpp}` 与 `host_clock.{hpp,cpp}` 放在 `src/` 下会被 glob 到并逐个检查**：
不得使用 `#include "../..."`；若包含任何 `cuexis/...` 头，必须位于 `cuexis/playback/` 下。
（它们本应不依赖 SDK，只做解析与有界算术，所以天然满足。）

#### (C) 门禁调用链与根注册

`CMakeLists.txt:412-439` 注册 `cuexis_reference_host_staging`：
`COMMAND ${CMAKE_COMMAND} -DCUEXIS_... -P cmake/VerifyReferenceHost.cmake`，
`LABELS "external;package;s6-c4;reference-host"`，`RESOURCE_LOCK cuexis_external_consumer`，`TIMEOUT 900`。
`:431-432` 额外传 `CUEXIS_IMPORT_TOOL` 与 `CUEXIS_IMPORT_TOOL_KIND`。

`VerifyReferenceHost.cmake` 内部关键锚点：

| 行 | 内容 | R9 用途 |
| --- | --- | --- |
| `:37-39` | `work_dir = ${CUEXIS_BINARY_DIR}/ec/host`，先 `REMOVE_RECURSE` 再建 | case 输出目录的父目录 |
| `:108` | `prefix = ${work_dir}/prefix`（clean install 前缀） | — |
| `:137-138` | `host_project = ${work_dir}/host`；`file(COPY "${host_source_dir}/" DESTINATION ...)` | ★ **整目录复制**，所以 `tests/commands/` 与 fixture 会一起被复制到源树外 |
| `:139-143` | 复制 `cfu_f_reference_project` 与 `cfu_f_v4_reference.cxc` 到 `content/` | case 的内容根 |
| `:145` | `host_build = ${work_dir}/host-build` | — |
| `:170-` | `configure_arguments` | — |
| `:210` | `--build "${host_build}" --target cuexis_reference_host`（**只构建宿主，不跑测试**） | — |
| `:251` | `set(golden_identity "6d01494c126f3ae8fc9420259dc92873233022dec9dd6bf9caf04b217f100cc5")` | ★ 绝对锚点来源 |
| `:256` | 原 legacy 调用传 `--expect-identity "${golden_identity}"` | — |
| `:282` | `execute_process(...)` 运行宿主，产出报告 | ★ **R9 新 case 照此模式** |
| `:305-334` | 原报告 `MATCHES` 断言（含 `:326` golden frame pattern） | ★ 必须继续通过 |
| `:351-591` | minor / toolchain / import / 安装边界等后续检查 | 不动 |

★ **`VerifyReferenceHost.cmake` 全 591 行内没有任何 `ctest` 调用**；全仓库唯一调用 `ctest` 的是
`cmake/VerifyExternalConsumer.cmake:580,586`（另一套门禁）。`examples/reference_host/CMakeLists.txt:49`
定义的 `add_test(NAME cuexis_reference_host_run ...)` **除定义处外零引用**，根本门禁不执行它。

★ **`:326` 的 golden frame pattern 要求字段连续**：

```cmake
"host\\.frame index=1 mode=seek chartTimeMs=625 discontinuityId=1 objects=2 digest=11596562486377158370 algorithm=3"
```

`host_report MATCHES` 是**子串/正则**匹配，所以新字段**追加在 `algorithm=3` 之后**是安全的；
**`algorithm=3` 是"原有连续字段段"的末项，不是整行最后字段**。绝不插入中间。

#### (D) 其他不得触碰的锚点

- `engine/playback/src/playback_session.cpp:1153-1171` `commitFrameStage`：`:1159` 把
  `simulationDeltaTimeMs` 归零；`:1160-1162` 在 `RestartAtZero` 下另把 `chartTimeMs` 归零；
  **`:1163` 会真的调用一次 `runtimeSession->update(...)`**——所以 reload **必然**产生一次 SDK 采样。
  另注 `:1155` `if (!context.replacement || context.targetFrame == nullptr) return {};`
  → **初次加载不产生 update**（这是用例 C06 断言 `publicUpdateSuccesses=0` 成立的原因）。
- `engine/playback/include/cuexis/playback/playback_session.hpp:294-296`：
  `prepareReload(PlaybackSource&&, const RuntimeFrame& targetFrame, ...)`。
- `SessionState` 只有 `{Empty, Ready, Running, Failed}`——**SDK 没有 `Paused`**。
  `PlaybackState`（含 `Paused`）在 `engine/audio/include/cuexis/audio/audio_transport.hpp:14`，
  属音频传输层，与本批次无关。

---

## 3. Normative Contract

### 3.1 模式与 CLI 兼容

新增**唯一**入口：`--command-file <path>`。

| CLI 项 | legacy（不传新入口） | 命令模式 |
| --- | --- | --- |
| `--content` | 维持现有必需内容根 | 可选默认根，仅无参数 `open` 依赖它 |
| `--package` | 原有包等价自验 | 显式传入即 `flag_conflict` |
| `--advance` | 原有默认 4 及显式 N | 显式传入即 `flag_conflict`；**默认值不能触发冲突**（须记录 flag 是否**实际出现**） |
| `--expect-identity` | 原有语义 | 检查首次成功 `open` 的 identity；无 `open` 则验收失败 |
| `--expect-digest` | 原有索引及摘要期望 | 显式传入即 `flag_conflict`，**不重载索引语义** |
| `--report` / `--help` | 原有行为 | 支持；报告仍落盘并回显 |

旧七个 flag 在 `main.cpp` 中的完整 `argument` 比较**保留**。重复 `--command-file` 拒绝；
不顺带改旧模式的重复 flag 策略。命令模式**不自动 open、不追加 legacy 自验、不运行 packageProof**。
`--help` 不执行命令。

### 3.2 文件语法与数值限额

每行一条命令；允许空行、前导/尾随 ASCII 空白、首个非空白字符为 `#` 的整行注释；
**不支持**行内注释、分号串联、嵌套、include、变量、shell 或网络命令。

支持 LF/CRLF，首字节处可有 UTF-8 BOM；**拒绝 NUL**。语法标记是 ASCII；路径保持现有宿主
filesystem 能力，不作额外 Unicode 支持承诺。

`open` 路径**必须是双引号参数**；用 `std::quoted` 解析前**先检查起始引号**，
不能因 `std::quoted` 的无引号回退放宽语法。反斜杠作为转义符；示例优先用正斜杠；
双引号和反斜杠需转义。拒绝空路径、未闭合引号和尾随 token。

命令文件路径及 `--content` 路径**相对进程 cwd**；**文件内 `open` 路径相对命令文件所在目录**。
路径来自文件系统参数，不交给 shell 拼接。

`tick`/`seek` 只接受十进制非负整数；不接受正负号、小数、指数、NaN/Inf 或额外字符。
使用 `std::from_chars` 并检查**完整消费**与 `result_out_of_range`。

| 常量/限制 | 本批次值 | 执行位置 |
| --- | --- | --- |
| `tickStepMs` | 250 ms | 每个实际 tick 帧 |
| `maxCommandFileBytes` | 1,048,576 字节，含 BOM/注释/换行 | 读最多上限+1 字节即拒绝，不先无界读入 |
| `maxCommands` | 10,000 条有效指令 | 完整预解析；空行/注释不计 |
| `maxTickAttempts` | 单条和全文件累计均 100,000 | 完整预解析；**Paused 中的请求也计入预算** |
| `maxChartTimeMs` | 9,007,199,254,740,991（2^53−1） | `seek` 参数预检；`tick` 相加在 `update` 前检查 |
| `discontinuityId` | `uint64_t`，不允许回绕 | `seek` 增加前检查 |

端点**包含**在限额内。clock 建议用受限**整数毫秒**存储，在 SDK 边界转换 double；该范围内整数精确可表示。
Paused tick 不作 clock 相加，**不能因一项不会发生的推进报时间溢出**。

实现常量只在宿主私有头定义一次，解析器和执行器共用。**独立验证器保留合同端点的独立断言，
不能导入被测实现的常量后自行适应错误值。**「单源」指合同权威与实现各自避免重复，
**并非用被测实现生成测试期望**。

完整文件**必须以 `quit` 为最后有效指令**；缺 `quit`、`quit` 后命令、语法/预算错误**均在任何 SDK 加载前拒绝**。
运行前置条件**不伪装成语法**：例如「第二次 `open`」在执行到该命令时拒绝，以保留正确的生命周期证据。

### 3.3 动词与效果

| 动词 | 前置条件 | 成功效果 |
| --- | --- | --- |
| `open [双引号路径]` | 宿主 Empty 且 SDK Empty；无参数时已提供 `--content` | `buildProjectSource`/`prepareLoad`/`commit`；transport=Paused，SDK Ready，t=0、id=0，**无成功 update 帧** |
| `play` | 已 open，SDK Ready 或 Running | transport=Playing；**不采样**，不改 clock/id；重复 `play` 幂等 |
| `pause` | 同 `play` | transport=Paused；**不采样**；重复 `pause` 幂等 |
| `tick N` | 已 open，SDK Ready 或 Running | Playing 时每次 `t+=250`、`dt=250`、id 不变，`update`/`extract`/`digest`；Paused 时只统计 suppressed |
| `seek T` | 已 open，SDK Ready 或 Running | 显式定位，`id+=1`、`t=T`、`dt=0`，`update`/`extract`/`digest`；**transport 不变** |
| `reload` | 已 open，SDK Running，**且有成功 update 帧** | 同一内容根 `prepareReload(KeepChartTime)`/`commit`；transport/t/id 不变，保存 normalized `dt=0` 采样帧 |
| `quit` | 宿主未终止 | 卸载活动会话、验证 Empty、终止循环；**无会话也可正常结束** |

`seek` 即使目标等于当前时间**仍增加 id**；`play`/`pause`/`reload` 都**不**增加 id。
**用户模式不提供 `sample`**：无法得到 legacy 初始 `(0,0,0)` 这一点**有意保留**。
复用 SDK 动作**不要求**两种编排具有相同的首帧隐式动作。

`open;play;quit` 合法且**零帧**；`open;reload` 拒绝（`no_sample`）；`open;seek 0;reload` 合法。
`reload` 的「已有帧」来自**成功 update**，不来自只读 `extractFrame`。

`open` 仅支持参考宿主**既有显式资产表/内容布局**；不承诺任意项目资产发现或运行中内容切换。
R9 验收使用**内容不变**的参考 fixture，identity 保持断言适用于这种前提；**测试不得在两次采样间偷偷改 fixture**。
同根内容编辑后的热重载语义**不作为 R9 新增承诺**，沿用已有 `prepareReload`/`commit` 合同。

### 3.4 完整状态矩阵

`P=Paused`，`L=Playing`，`E=Empty`，`T=Terminated`。任何表中「拒绝」都进入**统一失败退出**，
**不继续后续用户命令**。

| transport × SDK 状态 | open | play | pause | tick N | seek | reload | quit |
| --- | --- | --- | --- | --- | --- | --- | --- |
| E × Empty | →P×Ready | 拒绝 | 拒绝 | 拒绝 | 拒绝 | 拒绝 | →T×Empty |
| P × Ready，无成功帧 | 拒绝 | →L×Ready | 幂等 | 全部 suppressed | →P×Running | `no_sample` 拒绝 | unload→T×Empty |
| L × Ready，无成功帧 | 拒绝 | 幂等 | →P×Ready | N>0 首次成功后→L×Running；N=0 不变 | →L×Running | `no_sample` 拒绝 | unload→T×Empty |
| P × Running，有成功帧 | 拒绝 | →L×Running | 幂等 | 全部 suppressed | 保持 P×Running | 保持 P×Running | unload→T×Empty |
| L × Running，有成功帧 | 拒绝 | 幂等 | →P×Running | 正常推进 | 保持 L×Running | 保持 L×Running | unload→T×Empty |
| 任意 × Failed，或运行操作失败 | 不再分发 | 不再分发 | 不再分发 | 不再分发 | 不再分发 | 不再分发 | 自动进入清理，不等待 quit |
| T × Empty | 不再分发 | 不再分发 | 不再分发 | 不再分发 | 不再分发 | 不再分发 | 不再分发 |

其他组合（如 `E×Running`、`P×Empty`、`Ready` 却标记有成功 update 帧）为**内部状态不一致，失败退出**。
SDK 操作返回 error **不代表** state 一定变成 `Failed`；即使仍 Running 也按运行错误结束。

### 3.5 错误、清理与诊断码

稳定宿主诊断码：

- `host.command.file_read` / `syntax` / `unknown_verb` / `invalid_number` / `limit_exceeded`
- `host.command.missing_quit` / `after_quit` / `flag_conflict`
- `host.command.not_open` / `already_open` / `no_sample` / `clock_overflow`
- `host.command.state_mismatch` / `expectation_failed`

解析错误带**源行号**；文件/CLI 错误用明确 source；**SDK 错误保留 SDK 原 code 和所属命令**，
不用泛化宿主码掩盖根因。

二次 `open` 处理函数**先拒绝**，拒绝时旧 identity/state/采样不被改变；之后统一 fail-fast 退出**可以** unload。
失败 `reload` 同理：候选失败不改变 active，随后退出清理。
**不得把「拒绝动作保留 active」误写成「退出永远不卸载」。**

清理**恰好**走统一出口，避免手工 `quit` 与 scope 清理重复卸载。清理失败**附加**错误，
不覆盖最初原因，不虚报 Empty；退出码仍非零。`HostReport::rejection` 仅供**既有预期失败自验**，
**不能把用户运行错误记成成功**。

无 `open` 但指定 `--expect-identity`，结束时 `expectation_failed`。
`--expect-identity` 在首次 `open` 后核验，失败走同一清理；**不以没有 identity 跳过检查**。

### 3.6 帧、计数与报告

计数定义必须区分：

- `tickAttempts`：**已经开始处理**的 tick 调度次数；正常处理完成的 `tick N` 累计加 N。
- `suppressedTicks`：Paused 时抑制的次数；**没有对应 update**。
- `emittedTickFrames`：Playing tick 成功 update/extract/digest 并发出 frame 的次数。
- `publicUpdateAttempts` / `publicUpdateSuccesses`：宿主对 `PlaybackSession::update` 的**实际调用/成功**次数；
  含 tick、seek、legacy sample，**不含 reload 内部 runtime update**。
- `frameCount`：成功 `host.frame` 事件数；用户模式 = `emittedTickFrames` + 成功 seek 数；**reload 不占 frame 索引**。

在无运行失败的用户序列中，`tickAttempts == emittedTickFrames + suppressedTicks`。
失败中途**不得**把未执行剩余 tick 计为成功；**不强求**失败轨迹满足成功恒等式。

计数在**真实调用处**记录；SDK `update` 成功但 `extract`/`digest` 失败时**也应记录 update 成功**，
随后终止，**不能假装 SDK 未推进**。clock 在成功 update 后反映真实输入，不因后续报告失败继续播放。

**保留旧 `host.frame` 连续前缀**：

```text
host.frame index=... mode=... chartTimeMs=... discontinuityId=... objects=... digest=... algorithm=3
```

新字段**只能追加在 `algorithm=3` 之后**。命令模式 frame 索引**全程从 0 递增**；legacy 原索引规则不变。
追加 `cmdIndex`、`simulationDeltaTimeMs` 及必要计数。

`host.command` 带源行/动词/outcome/transport 前后；`host.play`/`host.pause` 带实际 SDK state 及 clock/id；
`host.clock` 带统计。**终止后无命令分发/frame**。

暂停核查**可** state/extract/digest，但发 `host.observation` **而非** `host.frame`；
使用 `lastSampleFrame` 进行 digest，**不人为 update 一次「验证暂停」**。
尚无成功帧时**不计算**依赖 `lastSampleFrame` 的摘要。

`reload` 记录**实际 targetFrame 参数**及 commit 后的 normalized 采样信息，用 `host.reload-sample`
或等价非 frame 事件。**事件数据来自传入 SDK 的同一个局部变量**，不能重新拼装「期望 target」打印。

### 3.6.1 命令模式报告 schema（**冻结，R9-1**）

本节是冻结点 **F4**：case fixture 与 case runner 都按此写，**实现不得单方面改字段名或顺序**。
旧事件与旧字段一律不变；新字段**只能追加**。

**旧 `host.frame` 的连续前缀必须逐字保留**，新字段追加在其后：

```text
host.frame index=<n> mode=<advance|seek> chartTimeMs=<t> discontinuityId=<id> objects=<n> digest=<d> algorithm=3 cmdIndex=<n> simulationDeltaTimeMs=<dt>
```

`simulationDeltaTimeMs`：tick 产生的帧为 `250`，seek 产生的帧为 `0`。命令模式 `index` **全程从 0 递增**。

**新增事件**（字段顺序固定，`<...>` 为占位）：

```text
host.command index=<n> line=<源行号> verb=<open|play|pause|tick|seek|reload|quit> outcome=<ok|rejected> transport=<前>-><后>
host.play state=<SessionState> chartTimeMs=<t> discontinuityId=<id>
host.pause state=<SessionState> chartTimeMs=<t> discontinuityId=<id>
host.observation reason=pause-check state=<SessionState> chartTimeMs=<t> discontinuityId=<id> sampled=<yes|no> objects=<n> digest=<d|none>
host.reload-sample cmdIndex=<n> targetChartTimeMs=<t> targetSimulationDeltaTimeMs=<dt> targetDiscontinuityId=<id> normalizedChartTimeMs=<t> normalizedSimulationDeltaTimeMs=0 normalizedDiscontinuityId=<id>
host.clock tickAttempts=<n> suppressedTicks=<n> emittedTickFrames=<n> publicUpdateAttempts=<n> publicUpdateSuccesses=<n> frameCount=<n>
host.diagnostic step=<open|play|pause|tick|seek|reload|quit|parse|flags|file> code=<host.command.*> detail=<文本>
```

`transport` 取值域 `Empty|Paused|Playing|Terminated`。被拒绝的命令**也发** `host.command`（`outcome=rejected`，transport 前后相同），
随后紧跟 `host.diagnostic`。

`line=` 是 **1 基物理源行号**：注释行与空行**照常计数**，BOM 不占行。
因此 fixture 顶部加一行 `#` 注释会使其后所有行号后移一位——C01 的 `seek` 在第 **3** 行、
C05 的 `reload` 在第 **5** 行（两者都有 case-id 头注释）。用户诊断指向真实文件行，故取物理行号而非「有效命令序号」。

**出现规则**（runner 依赖这些规则，不能靠猜）：

| 事件 | 何时出现 |
| --- | --- |
| `host.command` | 每条**被分发**的命令一次。预解析拒绝时**没有**（没有任何命令被分发） |
| `host.play` / `host.pause` | 每次**被接受**的 `play` / `pause` |
| `host.observation` | 每次**被接受**的 `pause`；`sampled=yes` 当且仅当已存在成功 update 帧，此时 `objects`/`digest` 取自 `lastSampleFrame`，且**不调用 `update`** |
| `host.reload-sample` | 每次**成功**的 `reload` |
| `host.clock` | **恰好一次**，在命令循环结束、统一清理与 unload 之后、`host.summary` 之前；**仅当程序被接受并开始执行** |
| `host.diagnostic` | 每次用户错误一次，且**置 run 为 failed**（`rejection` 只用于既有预期失败自验，不得用于用户运行错误） |
| `host.destroy` | `quit` 成功卸载后，沿用旧形状 `state=Empty` |

`host.diagnostic` 置 run 失败意味着负例的 `host.summary` 为 `outcome=failed`，退出码非零——**这正是负例的判据**。

**报告文件在 CLI/flag/文件/解析的任何验证之前打开。** 因此 `flags`/`parse`/`file` 级拒绝
**也会留下记录**，负例的「准确原因」才有处可查。若在验证之后才开报告，
`file_read`/`flag_conflict`/`syntax` 这些负例就只剩一个退出码而**没有原因**，
不满足 §8.5「不接受任意崩溃」。

**不新增** `host.tick`/`host.seek`：§7 的断言全部可由 `host.frame` 与 `host.clock` 表达，
少一个事件就少一处需要独立验证的表面。

### 3.6.2 case fixture 与 `.expect` 语法（**冻结，R9-1**）

case 数据放在 `examples/reference_host/tests/commands/`（随既有整目录复制到源树外）：

- `<case-id>.cmd`：命令文件，语法见 §3.2。
- `<case-id>.expect`：期望，**面向行的固定语法**，由 case runner 逐行解析。

`.expect` 行（`#` 起首为注释，空行忽略）：

```text
exit ok                       # 或 exit fail
code host.command.not_open    # 负例必需：host.diagnostic 必须含此后缀
diagnostic-step parse         # 负例可选：限定 step=
frame <t>/<dt>/<id>           # 按序匹配 host.frame 的 chartTimeMs/simulationDeltaTimeMs/discontinuityId
digest-anchor                 # 该帧 digest 必须等于 §6.1 的冻结 golden（不写字面值）
count tickAttempts 6          # host.clock 的字段必须等于该值
require <子串>                # 报告必须含该子串
require-count <子串> <n>      # 该子串必须**恰好**出现 n 次（用于「两次 reload」这类次数断言）
absent <子串>                 # 报告必须不含该子串
```

`code` 的检查对象是 `host.diagnostic` 行的 `code=` 字段**后缀**（稳定诊断码见 §3.5）。
**`.expect` 里不允许出现任何 64 位摘要字面值**：锚点只写 `digest-anchor`，
其余一律用 §6.2 的关系判据（逐帧相等、帧数、计数、state），从而**不新增 golden**。

**case 种类**（决定需要哪些文件）：`cmd`（有 `.cmd` fixture）、`invocation`（无 fixture，
由 runner 直接构造调用，如不存在的命令文件路径）、`generated`（runner 在测试时生成，
如超限文件）、`legacy`（C12，证据是既有 `VerifyReferenceHost.cmake:282-334` 那段，不新增 fixture）。

`.expect` 对 **`cmd`/`invocation`/`generated` 三种都必需**——否则 `generated`/`invocation`
用例没有任何断言、会**空过**。`.cmd` 只对 `cmd` 种类必需。
因此集合相等校验分两条：`*.cmd` 集合 == `cmd` 种类 id 集合；`*.expect` 集合 == 其余三种 id 集合。
三者（声明、磁盘上的 fixture、磁盘上的期望）任一不等即失败。

**必跑清单与集合相等**（承接 §14 未决项）：case runner 在 CMake 里**声明** case 清单，
并用 glob **仅作校验**断言「声明的 id 集合 == `*.cmd` 文件集合 == `*.expect` 文件集合」，
三者任一不等即失败。**glob 永不作为发现手段**，因此「fixture 存在但从未被声明」会硬失败而不是被静默跳过。

---

## 4. 实现结构

新增宿主私有 `host_commands.{hpp,cpp}`（有限解析/类型化命令/位置）与
`host_clock.{hpp,cpp}`（transport 与有界时间计算）。文件可合并以保持规模，不改变职责，
**也不进入 SDK 安装**。

`host_runner.cpp` **保留** SDK 动作及旧阶段报告，抽取 `HostContext` 与动作：

```text
main -> parse argv -> parse complete program or build legacy program
runHost -> HostContext -> sequential dispatch -> uniform cleanup/verification
actions: openContent / consumeStep / reloadContent / quitHost
legacy-only actions: SampleAtCurrent / ReloadReplayProof /
                     RejectedReloadProof / PackageProof
```

用户程序中的 `pause` **必须在下一条 tick 之前真正改变上下文**；
**不能对每条命令重新调用整个旧 `runHost`**。两模式复用实际 `load`/`reload`/`unload`/`consumeStep`，
不复制一套 SDK 调用。

旧默认自验迁入同一执行框架但**保留完整原行为**：

1. Empty 检查、host source prepare/commit 与 manifest/identity；
2. 私有初始 sample `(0,0,0)`，seek 625/250/1250，对应 id 1/2/3；
3. `--advance N` 连续 250 ms 推进，默认 N=4；**保留原 digest 期望索引**；
4. 成功 reload 后原轨迹逐项重放等价、provider 故障 reload 拒绝及 active 不扰动；
5. 可选 package 路径、身份/帧等价、unload/Empty。

**私有 sample 不向用户开放**；初始 sample 不可替换为 `seek 0`。用户 `reload` 只**原地保时**，
不能偷偷执行 `ReloadReplayProof` 或重置 clock/id。自验重放重置上下文**仅属于 legacy 等价试验**。

`host_runner.cpp` 中**真实保留** `buildProjectSource`、`prepareReload`、`commit` 及
九个受 A2 检查阶段；`load`/`reload`/`package` 都有成功与失败路径。
纯解析器拆出不影响这些约束；**禁止为过字符串检查留下死代码**。

**不新建根 CMake 构建 target**，不把示例 `add_subdirectory` 进主工程；
仅为既有示例可执行目标加入私有源文件——即改 `examples/reference_host/CMakeLists.txt:33-37` 的源文件清单。

---

## 5. Reload 帧模型与 H4

`HostContext` 区分 `clock`、`hasSuccessfulUpdate`、`lastSampleFrame` 和 frame 记录。
**首次 open 后 `lastSampleFrame` 为空。**

成功 `tick`/`seek` 把**实际提交的** `RuntimeFrame` 保存为 `lastSampleFrame`；
`reload` 取得该帧作为 `targetFrame`；prepare/commit 成功后保存 `(t 相同, dt=0, id 相同)` 作为
`lastSampleFrame`。**没有新的用户 frame 事件。**

下一次连续 tick 独立计算 `(t+250, 250, id)`，**不能沿用 reload 的 dt=0**，
**也不能把暂停时间计入 dt**。

本批次 H4 至少包含：

1. Paused 下**连续两次** `reload`，第二次传给 `prepareReload` 的 `targetFrame.dt` **确为 0**；
2. Playing 下 `reload` 保持 Playing，下一 tick 继续；Paused 下 `reload` 后普通 tick 仍抑制；
3. 相同 fixture 在 reload 前后用**同一 normalized `RuntimeFrame`** 对 snapshot 计算 digest 相等；
4. 同样的 tick/seek 轨迹，**插入 pause 抑制段及 reload 前后**，成功 frame 轨迹和 digest **逐项等价**；
5. 原 legacy 重放等价和 faulty-provider active 不扰动仍通过。

**第 1 项是操作输入合同，不能仅靠「第二次 reload 成功」证明**：SDK 自己会清 dt，
可能掩盖传入陈旧非零 dt 的宿主 bug。**验证实际 SDK 调用参数事件**；变异 `lastSampleFrame` 归零赋值必须被该断言抓住。

任一失败 `reload` **不提交** normalized 新帧，保留错误发生前的 active 和 `lastSampleFrame` 用于诊断，
然后统一退出。**正常用户模式不注入故障开关**；既有 legacy 预期失败自验复用相同 SDK 动作并验证错误结果。

---

## 6. 摘要裁判（混合）

### 6.1 绝对锚点 — 复用 `cmake/VerifyReferenceHost.cmake:251` 已冻结值

```text
identity  = 6d01494c126f3ae8fc9420259dc92873233022dec9dd6bf9caf04b217f100cc5
frame     = (chartTimeMs=625, simulationDeltaTimeMs=0, discontinuityId=1)
objects   = 2
algorithm = 3
digest    = 11596562486377158370
viewport  = 1280 x 720
```

**锚点命令**：`open; seek 625; quit`。其 frame 索引为 **0**，**不要求等于 legacy 的 1**。
内容使用**同一 staged reference project**。

执行前置「省略 legacy 初始 sample 仍满足同一参考采样」**由此用例验证**，
**不靠推断写成已经通过**。若失败，**调查初始化依赖，不重新录 golden、不加用户 sample 或偷偷改 open 语义放行**。

验证器从**原 golden 变量/既有来源**取得期望，可做无数值变更的变量抽取，**不新增一套硬编码数字**。
**禁止首次运行录值，禁止从实际输出回填 expected。**

### 6.2 关系判据

同时核对**实际 `RuntimeFrame` 三元组、帧数、计数、state、identity、digest 相等**，
并对选定不同参考帧核对 **digest 不全相同**，杀死常量 digest 变异。关系判据**不是**所有时间点的完整绝对 golden；
**不得宣传它能排除每一种系统性回归**，保留既有 SDK/fixture 回归承担更广覆盖。

### 6.3 ★ 数值比较陷阱

`digest` 是 `uint64` 十进制文本。CMake 断言**按完整字符串比较**，
**禁止用 `math(EXPR)` 转换 `11596562486377158370` 等超出有符号 64 位的数**。
数字轨迹的小范围算术与摘要比较**分开**。

### 6.4 C01 前置的探索性测量（**设计探索，不是证据**）

§6.1 把「省略 legacy 初始 sample 仍满足同一参考采样」列为 C01 的执行前置，并要求**不靠推断写成已通过**。
该前置在动手实现前做过一次**探索性**测量，结论如下，供 R9-1/R9-3 定形使用。

**问题**：legacy 路径的 `(625,0,1)` 是**第二次** update（前面有 `(0,0,0)`）；C01 的锚点命令
`open; seek 625; quit` 使它成为**第一次**。两次 digest 是否相同？

**方法**：临时把 `examples/reference_host/src/host_runner.cpp:36-41` 的 `scriptedSteps`
由 4 项改为 3 项、去掉 `(0,0,0)` 那一项，跑
`ctest --preset debug -j1 -R cuexis_reference_host_staging`，读
`out/build/debug/ec/host/host-report.txt`，随后**逐字节恢复**。
因为 `runFrameScript` 用 `scriptedSteps.back()` 初始化 advance 循环，`back()` 前后都是 `(1250,id=3)`，
所以该扰动**只改变了「首帧之前是否有一次 update」这一个变量**。

**观察结果**（逐字摘自记录）：

```text
host.frame index=0 mode=advance chartTimeMs=625 discontinuityId=1 objects=2 digest=11596562486377158370 algorithm=3
```

**与冻结 golden 逐位相同。** 结论：宿主 digest **不依赖**先采样 `t=0`；C01 的锚点语义可达。
这与源码一致：`FrameSnapshot`（`playback_session.hpp:129-162`）不含任何历史相关字段，
`computeFrameDigestVersion`（`frame_digest_version.cpp:123-174`）只哈希
(frame 三元组, viewport, camera, clear color, 每个 object 的 id/变换/可见性/材质/引用)。

**边界（必须遵守）**：

1. 这是**探索**，**不是退出证据**。C01 仍须在 R9-5 真实执行；本节的观察值**不得**复制进任何 `expected`。
2. 该次运行的 advance 循环摘要（1500/1750/2000/2250 ms）是**探索副产物**，
   按 §5.2 **不得记录**；C02–C08 继续只用 §6.2 的**关系判据**。
3. 恢复后已复跑该门禁确认通过（基线 9.62 s / 恢复后 9.01 s），且
   `git diff -- examples/reference_host/` 为空，即扰动未残留。
4. §10 的停止条件「绝对锚点无法解释地不匹配」**未被本次测量触发**。若 R9 实现后 C01 仍失败，
   按 §6.1 调查初始化依赖，**不重录 golden**。

---

## 7. 验收用例

以下 C/N 是 **staging 内部 case ID**，不是新增根 CTest 名称。分号在此表仅表示多行文件内容的简写。

| Case | 程序/触发 | 必须断言 | H 映射 |
| --- | --- | --- | --- |
| C01 absolute-anchor | `open;seek 625;quit` | frame0=(625,0,1)，绝对 identity/digest/objects/algorithm 匹配 | H1/H3/H7 |
| C02 pause-resume | `open;play;tick 2;pause;tick 3;play;tick 1;quit` | 仅 250/500/750，dt250、id0；attempts6、suppressed3、emitted3；暂停观测不变 | H2 |
| C03 continuous-control | `open;play;tick 3;quit` | 3 个 frame 与 C02 逐项相等；attempts3、suppressed0 | H1/H2 |
| C04 paused-reload-twice | C02 中 pause 后插入 `reload;reload` | frame 序列仍等于 C02；两次 target/normalized 正确，暂停 3 tick 仍抑制 | H4 |
| C05 playing-reload | `open;play;tick 2;reload;tick 1;quit` | Playing 保留；第三帧 750/dt250/id0，与 C03 相等 | H4 |
| C06 ready-suppression | `open;pause;tick 2;quit` | Ready 保持，attempts2、suppressed2、`publicUpdateSuccesses=0`、frameCount=0 | H3 |
| C07 paused-seek | `open;seek 1250;tick 2;play;tick 1;quit` | 首帧 (1250,0,1)，P×Running；抑制 2 次；次帧 (1500,250,1) | H3 |
| C08 idempotence | `open;pause;pause;tick 0;play;play;tick 1;pause;pause;quit` | 只有 (250,250,0)，幂等不采样、不增 id | H3 |
| C09 zero-frame-play | `open;play;quit` | Ready，不产生 frame，正常 Empty 退出 | H3/H5 |
| C10 empty-quit | `quit` | 无加载/更新，正常终止；**另测携带 identity 期望应失败** | H5 |
| C11 input-portability | 合法程序使用空格路径、LF/CRLF/BOM/注释；默认根与显式相对根 | 路径基准正确，轨迹一致，无编码扩承诺 | H6 |
| C12 legacy-regression | 原 staging 完整调用不变 | 原 golden/重放/faulty reload/package/minor/符号等全部保留 | H7 |
| N01 before-open | `play`/`pause`/`tick`/`seek`/`reload` 逐一置于 `open` 前，末尾有 `quit` | `not_open`，零 frame，不执行后续指令 | H5 |
| N02 reload-no-sample | `open;reload;quit` 及 `open;play;reload;quit` | `no_sample`，非零；**没有 reload 内部采样成功事件** | H4/H5 |
| N03 second-open | `open;play;tick 1;open;tick 1;quit` | `already_open`，拒绝瞬间 active 保留；后续 tick 不执行；清理独立记录 | H5 |
| N04 malformed-program | 未知动词、尾参、缺参、坏数、无 `quit`、`quit` 后指令 | 稳定原因/行号，**任何 open 前预解析拒绝** | H6 |
| N05 budgets | 每一限额上限/上限+1、整数溢出、seek 后 tick 相加越界 | 参数范围/总预算拒绝；**允许边界不靠巨量实际帧验证** | H6 |
| N06 mode-flags | 新入口与显式 `advance`/`package`/`expect-digest` 组合、重复 `command-file` | `flag_conflict`，**未执行 SDK 动作** | H6/H7 |
| N07 input-io | 不存在/不可读文件、合法语法但错误内容根 | 文件错误在执行前；内容错误带 SDK/host 原原因并失败退出 | H5/H6 |

预算成功边界可用 **Paused 下 `tick 100000`** 避免 10 万 SDK 帧；文件长度用注释填充，
命令数用幂等操作验证。**不能把 SDK 对极端 chartTime 的接受能力等同于解析器范围**：
数值端点解析和 SDK 采样测试**分层**；若纯解析测试需要单独可执行辅助器，
**应先按项目 target 登记规则接入**，而不是为测试给产品 CLI 增加旁路。

### H 判据

- **H1**：外部命令真正改变顺序和持久状态，六动词全部可下达；不能固定脚本伪装。
- **H2**：暂停无普通 update，clock 冻结，恢复无追赶；**两种 pause 错误都可反证**。
- **H3**：完整状态矩阵、Ready/Running 区别、seek 与幂等按合同执行。
- **H4**：原地 reload、normalized target、重复 reload、续播和故障 active 保留。
- **H5**：首次 open、二次拒绝、quit、运行错误清理有真实 SDK 证据。
- **H6**：解析、预算、路径、flag 负例在正确层以正确原因失败。
- **H7**：旧 CLI、九阶段、golden、安装边界和原分发/兼容验证不退化。

### 必做变异

**每条新增用例都要指定并运行至少一个能使目标断言失败的变异。**
参数化负例**逐项对应其守卫**，**不能以一个「删除所有验证」的变异声称覆盖所有错误码**。

必做变异包括：`pause` no-op；暂停只停消费却推进 clock；resume 补偿等待；
`seek` 不增 id / 自动 play；`reload` 清 transport / 重置 clock / 保留陈旧非零 dt；
常量 digest；忽略命令文件；二次 `open` 先卸载；绕过预算/尾参/终止检查；**删除门禁调用**。

变异应在**隔离副本**中：原版绿 → 变异以预期断言红 → 恢复绿，记录补丁/命令/退出码/诊断。
**编译失败、崩溃、超时不是语义变异通过证据。**
用户 `quit` 后代码执行路径**可用内部 dispatcher 试验验证**；
**仅用预解析拒绝尾随命令不能证明 `quit` 处理器自身终止**。

报告是**可观测接口，不是独立 SDK 真值**：state/snapshot/digest 必须来自实际调用，
update 计数贴近真实边界；通过相关变异证明打印固定 `preserved=yes` 或假计数不能替代行为验证。
必要的内部 focused 试验复用宿主私有代码，**不能假设只读外部日志可证明不可观测的内部参数**。

---

## 8. 门禁接线：明确采用 `execute_process`

```text
根 CTest cuexis_reference_host_staging
  -> cmake/VerifyReferenceHost.cmake
  -> clean install、示例整目录源外复制、find_package、build 现有 host、运行库准备
  -> 原 legacy executable run + 原报告/golden 断言          (VerifyReferenceHost.cmake:282 / :305-334)
  -> 新 R9 case runner：execute_process 逐例调用同一 host + 报告断言   ← R9 新增
  -> 原 minor/toolchain/import 等后续检查
```

**R9 不新增示例 `add_test` 作为主证据、不新增嵌套 `ctest` 调用。**
既有 `cuexis_reference_host_run` 保留独立工程使用，**不将它描述为当前根门禁已执行的测试**
（实测：全仓库除定义处零引用）。

推荐新增 `cmake/VerifyReferenceHostCommands.cmake`，由 `VerifyReferenceHost.cmake` 显式
`include`/调用；它接收 `host_executable`、复制后的 fixture 根、case 输出目录、golden 值和 clean 环境参数。
命令 fixture 放 `examples/reference_host/tests/commands/`，**随 `:138` 的整目录复制**到源树外。
验证器是根门禁工具，可以在仓库 `cmake/` 目录；被测示例仍仅消费安装 SDK 及复制内容。

### Case runner 要求

1. 使用**明确必跑 case 清单**，不靠 glob 偶然发现；**文件缺失、清单为空、case 未执行都失败**。
2. 每 case **独立报告路径**；执行前**去掉该 case 自己的旧报告**；避免上次成功残留冒充本次证据。
3. `execute_process` 直接传**参数数组**、设工作目录与 `TIMEOUT`；**不得通过 shell 拼接路径**。
4. 净化 `PATH`/`LD_LIBRARY_PATH`，取得 `result` 后**立即恢复**，再做任何 `FATAL_ERROR` 断言；**子运行失败也必须恢复**。
5. 正例要求整数退出码 0、报告存在、summary 成功及精确轨迹；
   负例要求**正常可识别非零退出和准确原因**，**不接受任意崩溃/动态库缺失/超时**。
6. 按**事件类型和字段逐行解析**，校验数量/顺序/唯一索引/期望值，**不能只 `MATCHES` 动词名**。摘要以字符串比较。
7. 输出每 case 证据和执行总数；**硬断言 `expected`/`completed`/`passed` 集合一致**。
   删除调用由 A2 接线表征和其变异捕获，**不把「0 cases」当通过**。

**不改变根 staging 的 `RESOURCE_LOCK` 和既有 `TIMEOUT` 预算来换绿色。**
若新增用例确实使预算不足，**先提供测量并按变更控制处理**。

A2 保留旧七 flag、九阶段、真实 `find_package`/链接及冻结 ADR 检查；
**增量**检查新入口、私有源文件接线、case runner 调用和必跑 case 声明。
**更新旧注释而非宣称旧 argv 已等价实现命令循环。**静态检查不能证明 `pause` 行为，由动态 case 承担。

**新增 `add_test` 不等于新增构建 target**；本设计的主路径两者均不必新增。
若实现中确有额外 `cuexis_*` 辅助可执行目标，再按 `BUILD_TESTING`/`CUEXIS_ACTIVE_TARGETS`/依赖 allowlist 登记，
**不能为省登记使用私有 engine 包含路径**。

---

## 9. 文档落点与索引

本文的**唯一维护副本**位于：

```text
docs/stage_plans/reviews/stage-06-review-remediation/R9-reference-host-command-loop.md
```

接线：

1. 同目录 [plan.md](plan.md) 追加 R9 行、依赖、状态和本文相对链接；**R7 原退出描述保留**。
2. [docs/stage_plans/README.md](../../README.md) 的「修正工作包」条目追加指向本文的链接及新增批次说明。
3. **不新建叶目录 README**，也不新增 `reviews/README`；当前 `STAGE_NAVIGATION_INDEXES` 只允许已登记导航 README，**不需要改检查器**。
4. `CURRENT_STATUS` 只同步修正包状态，**Stage 6 仍已关闭**；报告目录按现有 `stage_reports/README` 导航接入新的带日期 R9 报告。

规划文档状态 `active`；正文明确「已批准设计 / 未实现或正在实现」的事实。
**退出时按政策更新状态；不能把 owner 接受设计写成已接受最终退出。**

历史 delivery-report §5 第 1 项与 R7 报告**只追加订正**：零 stdin **不是**缺陷；
缺少可下达命令及 play/pause 语义**才是**缺口；原计划**允许本批次补实现**，不只限 Stage 8。
追加后续 R9 链接，**不改历史证据**。

---

## 10. 实施子步骤

| 子步骤 | 工作与文件范围 | 退出条件 |
| --- | --- | --- |
| **R9-0** 基线/合同 | 确认 #30 已合并（✅ `670cca8`）；建新分支（✅ `stage-06-r9-reference-host-command-loop`）；落盘本文、计划与导航链接 | 记录基线 SHA；设计接受引用；**`check_docs.py` 通过** |
| **R9-1** 先红用例 | case 文件、`VerifyReferenceHostCommands.cmake`、staging 调用 | **旧 host 因缺入口/合同而红**；安装/环境原门禁正常 |
| **R9-2** parser/clock | `main.cpp`、`host_commands`、`host_clock` 私有文件；CMake 源清单 | 严格语法/预算/transport 实现；focused 边界验证 |
| **R9-3** 编排 | `HostContext`、共享 SDK 动作、真实 dispatcher、legacy 迁移 | C01–C03/C06–C12 及 H7，**旧 golden 不变** |
| **R9-4** lifecycle/reload | `no_sample`、`lastSampleFrame`、统一清理、重复 reload | C04/C05、N01–N03/N07；相关变异被杀死 |
| **R9-5** 门禁闭环 | A2 增量/自测、全部 N 例、case 计数、报告数据字段 | H1–H7 覆盖；每个新例变异证据；**零 case 硬拒绝** |
| **R9-6** 回归/交付 | README、追加订正、显示版本、全量矩阵、报告 | 最终 SHA 的 Version Gate + 三平台、owner 退出接受 |

这些是**依赖顺序**，不要求每步独立可合并 PR；先红提交属于 R9 工作分支证据，
**最终 PR 不得保留红门禁**。**不要在 R9-2 声称全部集成用例通过。**

**停止并重新评估的情况**：需要改 SDK 公共合同/ADR/golden；绝对锚点无法解释地不匹配；
只能删除原自验才可迁移；需要修改本方案已接受语义或平台跳过才能过门禁。
先记录复现/影响/替代方案/兼容迁移/测试，**再由 owner 裁定，不自行缩小范围**。

---

## 11. 版本规则（已对 `tools/check_version_gate.py:136-213` 逐分支核实）

R9 建分支时**继承合并后 master**，不因建分支或每个提交机械 +1。
准备合并候选时，以受保护目标基线 **B** 和 trusted UTC 日期 **D** 计算：

- **live 候选日期必须等于 D**；不是仅满足 `candidate_date >= base_date`。
  （实现：`context == "live"` 时 `< D` → `version.release_date.stale`；`> D` → `version.release_date.future`）
- 若 B 日期**等于** D，候选 `build = B.build + 1`。
  （实现：`expected_build = base.build + 1`；少 → `version.build.backward`；多 → `version.build.skipped`）
- 若 B 日期**早于** D，候选 `build = 1`。（实现：`version.cross_day_build.invalid`）
- 若 B 日期**晚于** D，**报错调查**，不通过修改 trusted 日期绕过。（实现：`version.baseline.future`）

通过 `tools/update_version.py` 同步日期显示版本与 `vcpkg.json`；**SDK API 不变**。
跨 UTC 日或 master 前进**重新计算/重跑**。**不得预填 `26.09.28-2`，也不得沿用历史绿色**；
**历史复验不授予 live 合并资格**。

当前状态：本分支继承 `26.09.28-1`。**不要预设下一个版本号**——按上式计算。

**2026-09-29 复算记录**：UTC 进入 `2026-09-29` 后，分支当时携带的 `26.09.28-2` 被 Version Gate
以 `version.release_date.stale` 登记——该值在 UTC `2026-09-28` 当天是正确的，跨日后按上表即过期。
复算：基线 `670cca8` 日期 `2026-09-28` **早于** D `2026-09-29`，故 `build = 1`，唯一正确取值为
**`26.09.29-1`**，已用 `tools/update_version.py` 落地（SDK API 仍 `0.7.0`）。这就是上面"跨 UTC 日
重新计算"所要求的情形，属门禁按设计工作，不是门禁缺陷；日期显示版本的当前值以
`cmake/CuexisVersion.cmake` 与 `vcpkg.json` 为准。

---

## 12. 实施后的验证命令

```powershell
. .buildenv.ps1
python -B tools/check_docs.py
python -B tools/check_stage6_a2.py
python -B tools/update_version.py --check
git diff --check
cmake --preset debug --fresh
cmake --build --preset debug --clean-first
ctest --preset debug -j1 --no-tests=error -R 'cuexis_contract_s6_a2|cuexis_reference_host_staging' --output-on-failure
ctest --preset debug -j1 --no-tests=error --output-on-failure
cmake --preset release --fresh
cmake --build --preset release --clean-first
ctest --preset release -j1 --no-tests=error --output-on-failure
```

- `.buildenv.ps1` 是本地既有 shim，**不是发行环境证明**。
- **显式对 `examples/` 新增 C++ 做 format 检查**，不能假设根 format glob 覆盖它。
- 保留项目现有适用 static/shared 配置和 SDK minor/toolchain/import 检查；
  **不为 R9 开启未来 candidate 交付或假称未跑配置通过**。
- root staging 执行安装与源外消费；**仅 build 仓库 host 或本地跑脚本不足以验收**。
- Linux Quality、Windows MSVC、Windows MinGW 及 Version Gate 按**实际配置/保护规则**重跑，
  **最终证据绑定同一候选 SHA**；不机械套用历史「7 个 job」数量。
- **本地已知环境偏差（非回归，勿改门禁换绿）**：`cuexis_contract_version_gate` 在本机
  `C:\Windows\system32\bash.exe`（WSL 启动器）下失败，前置原生 bash 后 19/19；
  `cuexis_player_distribution` 因本机 vcpkg DLL 在 `PATH` 而非 `bin/` 失败。

---

## 13. 退出清单与证据格式

每条 case 证据至少包含：Case ID / H 映射、命令文件 hash、源码 SHA、构建配置 / SDK 安装来源、
执行命令、退出码、报告路径、期望来源、实际轨迹、**变异补丁及预期失败断言**。

R9 报告至少**分开**记录：已批准设计、已实现能力、机器验收、变异证据、本地环境偏差、
hosted run 引用、未覆盖范围、历史追加订正、owner 最终接受。**不能把 case 成功等同于整体退出。**

退出条件全部满足：

- H1–H7 **实际通过**，C/N 展开后的必跑清单**没有未执行项**。
- 暂停两种关键变异、重复 reload target 变异、恒定 digest 变异、**门禁调用删除变异**均有目标失败证据。
- 旧 golden、原完整 staging 自验、安装/符号/minor 边界**未弱化**。
- 显示版本 live 检查、最终 SHA 三平台适用门禁通过；**最后文档提交改变 SHA 后按政策重新验证**。
- 历史报告订正为**追加**；SPEC-27 有完整关闭依据；**owner 接受 R9 退出**。

合并 R9、发布和其他残余处置**依项目授权进行，不由本文自动触发**。
**确定性 headless transport 只冻结普通宿主调度**，不承诺音频/输入/所有内容活动同时冻结，
也不假装已交付完整媒体播放器。

---

## 14. 与 v2 方案的差异记录

本文是 v2 实施方案的落盘副本，并做了以下**增量**（均为在 `670cca8` 上实测所得，不是设计变更）：

1. **新增 §2 基线代码地图**：把 v2 中分散提到的代码事实集中为可执行的锚点表，
   含 `VerifyReferenceHost.cmake:74-90` 的**新文件自动落入源码卫生检查**这一硬约束
   （v2 未记录），以及 `:251` 的 golden identity 实际值。
2. **新增发现**：`tools/check_stage6_a2.py:299` 的 docstring 自称检查宿主
   「owns its command loop」，但**没有任何检查项验证它**——这是 SPEC-27 能在门禁全绿下存活的机械原因。
   R9 的 A2 增量应补真实表征，并订正 `:340-344` 的注释。
3. **§11 版本规则逐分支对照了实现**：补齐了门禁的实际 error code
   （`release_date.stale` / `future`、`build.backward` / `skipped` / `exhausted`、
   `cross_day_build.invalid`、`baseline.future`、`date.backward`、`unchanged`）。
4. **§6.3 / §12 补入本地已知环境偏差**，避免把本机失败误判为 R9 回归。
5. **§2.1 补齐文件规模表**与 §10 的 R9-0/R9-1 完成状态。

**v2 中已核实为正确的两处，本文原样沿用**：
`VerifyReferenceHost.cmake:305-334` 用 `MATCHES`（子串）而非整行相等，故新字段追加安全；
`playback_session.cpp:1158-1163` 确实归零 dt **并调用一次 `update`**，故 pause 必须定义为
「不产出新 `ClockStep`」而非「不产生任何 SDK 采样」。

**保留的未决项**：`examples/reference_host/tests/commands/` 的 fixture 文件若与 §8 的必跑清单漂移，
清单硬编码本身抓不到。**建议 case runner 用「声明清单」与「fixture 文件枚举」做 set-equality 交叉校验**
（glob 仅作校验，不作发现）——此项**尚未获 owner 裁定**，实施时若采纳需记录。

---

## 15. 子代理并行执行流程表

> **本节是执行编排序，不是合同。** 合同仍是 §3–§9；本节只回答「谁在什么时候动哪个文件」。
> 行号、限额、判据一律以 §3–§9 为准，本节不复述数值。

### 15.1 编排的三条硬约束

1. **文件独占**：任一文件在同一时刻**只能有一个写者**。并行只发生在**不相交文件集**之间。
2. **构建串行**：本仓库**不并发构建同一 build 目录**——`ctest` 并行在本机会挂（必须 `-j1`），
   两个 agent 同时 `cmake --build --preset debug` 会互相破坏产物。**并行的是作者，构建是串行的。**
3. **唯一集成写者**：`src/host_runner.cpp` 与 `src/host_report.hpp` 是集成核心，
   **永远只有一个写者**（见 §2.4(A)：A2 把该文件名硬编码 4 次，且 §4 要求九阶段与
   `buildProjectSource`/`prepareReload`/`commit` 留在该文件内）。

**例外**：`host_commands` 与 `host_clock` 按 §4 是**不依赖 SDK 的私有单元**，
所以它们可以由各自作者用**直接编译器调用**（不经 CMake build 目录）做 focused 自测，不违反约束 2。

### 15.2 阶段与泳道

| 阶段 | 泳道 | 子代理 | 独占文件（写入） | 依赖 | 产出 |
| --- | --- | --- | --- | --- | --- |
| **P1-a 接口冻结** | 串行 | `iface` | `src/host_commands.hpp`、`src/host_clock.hpp`（**仅头文件，先落地**） | 无 | 两个头 + 冻结的类型/函数签名 |
| **P1-b 并行扇出** | 并行 ×4 | `parser` | `src/host_commands.cpp` | P1-a | 解析器/限额/类型化命令 + focused 自测 |
| | | `clock` | `src/host_clock.cpp` | P1-a | transport 状态机 + 有界整数毫秒算术 + focused 自测 |
| | | `fixtures` | `examples/reference_host/tests/commands/**` | §7 用例表 | 19 个 case 的命令文件 + 期望事件断言表 |
| | | `runner` | `cmake/VerifyReferenceHostCommands.cmake` | §7/§8 | 必跑清单、逐例 `execute_process`、报告解析、set-equality |
| **P1-c 并行扇出** | 并行 ×2 | `a2gate` | `tools/check_stage6_a2.py` | §2.4(A) | 新入口/私有源文件接线/case runner 调用/必跑清单的检查项 + 订正 `:340-344` 注释 |
| | | `readme` | `examples/reference_host/README.md` | §3 | 命令模式使用说明（**草稿，P4 定稿**） |
| **P2-a 接线** | 串行 | `wire` | `src/main.cpp`、`src/host_runner.hpp`、`CMakeLists.txt` | P1-b | `--command-file` 解析、`flag_conflict`、源文件清单加入四个新文件 |
| **P2-b 集成** | 串行（唯一写者） | `core` | `src/host_runner.cpp`、`src/host_report.hpp` | P2-a | `HostContext`、共享 SDK 动作、真实 dispatcher、legacy 迁移 |
| **P3-a 静态门禁** | 并行 ×2（只读） | `v-a2` | — | P2-b | `check_docs.py` + `check_stage6_a2.py` 通过，且**新检查项确实能红**（各自变异一次） |
| | | `v-golden` | — | P2-b | 根 staging 门禁内**旧 golden 与九阶段未退化** |
| **P3-b 动态验证** | 并行（只读） | `v-cases` | — | P3-a | C01–C12 / N01–N07 全部实跑；报告解析逐字段 |
| | | `v-contract` | — | P3-a | 对照 §3.4 状态矩阵**逐格**核对实现，输出不符格清单 |
| **P3-c 变异** | 串行（隔离副本） | `v-mutants` | 隔离副本（不碰主工作区） | P3-b | §7 必做变异逐条：原版绿 → 变异红 → 恢复绿，附补丁/命令/退出码/诊断 |
| **P4 收尾** | 串行 | `ship` | `README.md` 定稿、`plan.md`、报告、版本 | P3-c | 显示版本按 §11 计算、全量两配置矩阵、R9 报告、历史追加订正 |

### 15.3 每个子代理的任务契约（派发时逐条写明）

**公共前提（所有子代理必读，禁止转述为"已通过"）**

- 分支 `stage-06-r9-reference-host-command-loop`，基线 `670cca8`；合同见 §3–§9。
- **禁做**：改 ADR 0042、改 SDK 公共头/API、改**任何既有 golden**、删旧门禁、
   改失败回滚、扩大公共头 allowlist、触碰 Stage 7A/8/9–12。
- **禁写不属自己的文件**；需要别人的文件时**只读**，并在产出中报告发现。
- 新增 `src/` 下文件必须满足 §2.4(B)：**不得 `#include "../..."`**；
  若包含 `cuexis/...` 头，**必须位于 `cuexis/playback/` 下**（否则 `VerifyReferenceHost.cmake:74-90` 红）。
- **不得**把未执行/未通过/未注册的检查写成通过。

| 子代理 | 必须做到 | 自验方式 | 明确不做 |
| --- | --- | --- | --- |
| `iface` | 落地两个头，签名能覆盖 §3.2 的语法/限额与 §3.4 的 transport 转移；类型不含 SDK 依赖 | 头文件可独立包含（`cl.exe /c` 单 TU 语法检查） | 不写 `.cpp` |
| `parser` | §3.2 全部语法与限额；`std::from_chars` + 完整消费 + 溢出；UTF-8 BOM/CRLF；拒绝尾随 token | **直接编译器调用**跑 focused 边界自测（上限/上限+1/溢出/未闭合引号/BOM）；不占用 CMake build 目录 | 不碰 `main.cpp`、不碰 SDK |
| `clock` | §3.4 transport 转移与有界整数毫秒算术；Paused tick 不作 clock 相加 | 同上，focused 自测覆盖 §3.4 每一格的 transport 列 | 不碰 SDK、不碰报告 |
| `fixtures` | §7 的 C01–C12、N01–N07 逐条落成命令文件；每个 case 附**期望事件断言表**（含负例的期望诊断码与行号） | 逐文件人工核对与 §7 表格逐字对应 | 不写 CMake、不写 C++ |
| `runner` | §8 的七条 runner 要求；**必跑清单硬编码**；每 case 独立报告路径且执行前删除旧报告；净化 PATH 后立即恢复 | 用一个**故意的假 case**确认清单/计数/set-equality 能红 | 不改 `VerifyReferenceHost.cmake` |
| `a2gate` | §2.4(A) 的增量项 + 修 §14 记录的缺口（docstring 声称 "owns its command loop" 却无检查） | 每个新检查项**各自变异一次**确认能红 | 不放宽任何既有检查范围 |
| `readme` | §3 的用户可见语法与语义；直说确定性 headless transport，不声称完整媒体播放暂停 | 与 §3 逐条对照，无自造语义 | 不定义限额/判据 |
| `wire` | §3.1 CLI 兼容表；旧七 flag 的 `argument == "--xxx"` **留在 `main.cpp`**；`--advance` 冲突按"是否实际出现"判定 | `check_stage6_a2.py` 仍绿 | 不动 `host_runner.cpp` 的 SDK 动作 |
| `core` | §4 结构：`HostContext` + 顺序 dispatch + **共享** `consumeStep`；九阶段与三个被检查符号留在 `host_runner.cpp`；§3.6 字段**只能追加在 `algorithm=3` 之后** | 本地 focused staging 跑通 + 旧 golden 不变 | **不改 golden**、不为过字符串检查留死代码 |
| `v-a2` / `v-golden` | 见 15.2；只读 | 报告**原始命令与输出**，不得只给结论 | 不修代码，只报告 |
| `v-cases` | 逐 case 实跑并逐字段解析报告 | 附每 case 的完整命令、退出码、报告路径 | 不接受"崩溃/超时/缺 DLL"当作负例通过 |
| `v-contract` | 对照 §3.4 **逐格**核对；对无成功帧/`Failed`/`T×Empty` 等边界格单独构造 | 输出**不符格清单**，无则明说"逐格核对无不符" | 不推断，必须实测 |
| `v-mutants` | §7 必做变异逐条；隔离副本 | 记录补丁/命令/退出码/诊断；**编译失败、崩溃、超时不算变异通过** | 不在主工作区改代码 |
| `ship` | §11 版本计算、§12 命令、§13 退出清单 | 最终 SHA 上重跑；**文档提交改变 SHA 后重新验证** | 不把 case 成功等同于整体退出 |

### 15.4 接口冻结点（并行能否成立的关键）

- **F1**：`iface` 落地两个头 —— **在此之前 `parser`/`clock` 不得开工**，否则签名漂移。
- **F2**：`parser`/`clock` 的 `.cpp` 与头一致 —— **在此之前 `wire` 不得开工**。
- **F3**：`fixtures` 的 case 清单冻结 —— `runner` 的必跑清单以它为准（两者由**不同**子代理产出，
  正好构成 §14 未决项所建议的交叉校验：`fixtures` 产出文件，`runner` 声明清单，`v-cases` 核对集合相等）。
- **F4**：`core` 的**报告事件字段**冻结 —— 在此之前的 `runner` 报告解析只能按 §3.6 的既有字段写。

### 15.5 并发度与预算建议

| 项 | 建议 |
| --- | --- |
| P1-b 并发写者 | **4**（文件完全不相交，且 `parser`/`clock` 用直接编译器自测） |
| P1-c 并发写者 | **2** |
| P3-a / P3-b 并发只读者 | **2**，避免同时触发重型门禁（根 staging 有 `RESOURCE_LOCK`，互相等待而非失败） |
| 变异执行 | **串行**，且必须在隔离副本 |
| 重型构建（`--preset debug/release` 全量） | **同一时刻只有 1 个**，由 `ship` 或当前阶段唯一验证者持有 |

**不建议**同时让两个以上子代理跑根 staging 门禁：它做 clean install + 源外配置构建，
既贵又受 `RESOURCE_LOCK cuexis_external_consumer` 串行化，并发只会互相阻塞。

### 15.6 失败与回退

- 任一子代理报告**合同与实现不符**：**先记录复现/影响/替代方案/兼容迁移/测试**，
  再由 owner 裁定，**不自行缩小范围**（§10 停止条件）。
- 任一子代理发现需要改 ADR / SDK 公共合同 / golden：**立即停止并上报**，不得自行处置。
- `core` 集成失败且无法在不改断言语义的前提下修复：回退到 P1-b 产物，重开接口冻结（F1），
  **不得**通过删旧自验或平台跳过换取绿色。

### 15.7 与 §10 子步骤的对应

§15 不替代 §10；两者是同一批次的两种视图：

| §10 子步骤 | §15 阶段 |
| --- | --- |
| R9-1 先红用例 | P1-b 的 `fixtures` + `runner`，接在 P2-a 之后首次成红 |
| R9-2 parser/clock | P1-a + P1-b 的 `parser`/`clock`，加 P2-a 的 `wire` |
| R9-3 编排 | P2-b 的 `core` |
| R9-4 lifecycle/reload | P2-b 的 `core` 后半（`no_sample`/`lastSampleFrame`/统一清理） |
| R9-5 门禁闭环 | P3-a / P3-b / P3-c + `a2gate` |
| R9-6 回归/交付 | P4 的 `ship` |
