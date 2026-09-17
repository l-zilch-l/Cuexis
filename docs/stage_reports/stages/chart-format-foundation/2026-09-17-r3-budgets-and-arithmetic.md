# Chart Format Foundation Hardening R3：预算表与 checked arithmetic

日期：2026-09-17

状态：R3 完成；Spec §3.3 冻结预算表，`maxPackedSectionBytes` 与逐字段预算在所有支持入口生效

起始 SHA：`bc3b5d4`（R2 本体提交，`codex/chart-format-foundation-hardening`，起始工作区干净）

上游依据：[加固计划](../../../stage_plans/active/chart-format-foundation-hardening/plan.md) 第 4 节 R3、
[R2 报告](2026-09-17-r2-profile-rejection.md)、[R1 报告](2026-09-17-r1-semantic-identity.md)、
[R0 报告](2026-09-16-r0-baseline-and-reproduction.md)、
[Packed Chart Spec](../../../formats/PACKED_CHART_FORMAT.md) 第 3、5、9 节、
[Foundation 原计划](../../../stage_plans/completed/chart-format-foundation/plan.md) 第 3.5/5 节。

本轮把 Foundation 声明的预算变成可执行门禁：`PackedChartLimits` 的七个字段此前只有一个
（`maxPackedFileBytes`）被间接使用，`maxPackedSectionBytes` 从未被任何入口读取（R0-H04）。
R3 不改变 wire revision、`flags=1`/`candidateRevision=1`、Header 布局、section 编码或
semanticIdentity 算法，也不放宽任何既有上限。

## 1. 先复现，再修复

新增 `tests/chart/packed_budget_tests.cpp`（`[chart][packed][budget][r3]`），其中负例都是
手工构造、CRC 自洽的 artifact，Writer 无法产出。**修复前**接入的 10 个用例（当时还没有
`packed::encode` 的 limits 参数，Writer 侧用例与「全部预算放宽」对照随后补入）结果：

```text
test cases:  10 |  0 passed | 10 failed
assertions: 136 | 87 passed | 49 failed
3 个用例以 unexpected exception 结束（bad allocation ×2、vector too long ×1）
```

| 复现项 | 修复前实际行为 |
| --- | --- |
| `maxPackedSectionBytes = 1` | `inspect`/`decode`/`PackedChartReader::read`/`writeAtomic` 全部 **接受**，声明值从未被读取 |
| CNS0 lane = `2^32 + 2` | **接受**：`static_cast<std::uint32_t>` 截断成已登记的 lane 2，typed 模型与合法 artifact 完全相同 |
| IDN0 iteration = `2^32 + 2` | **接受**：同样截断成 repeat index 2 |
| IDN0 `scopeCount = 0xFFFFFFFF` | `scopes.reserve(0xFFFFFFFF)` → **bad allocation** |
| IDN0 `pathCount = 0xFFFFFFFF` | `paths.reserve(...)` → **vector too long** |
| IDN0 `steps = 2^64-1` | `path.reserve(...)` → **bad allocation** |
| Header `directoryCount + 2^27`（u32 乘积回绕） | 通过 `count * 32U` 回绕校验，预留 1.07 GiB 后才以 `packed.directory.invalid` 失败 |
| Header `entityCount = 50000` + 放宽 limit | `inspect` **接受**（无 clamp） |
| 全部计数共用诊断 | 统一落到 `packed.budget.header`，无法区分是哪个上限被突破 |
| >16 MiB artifact + 放宽 limit | 未被文件门禁拒绝，仅被无关的聚合计数诊断（`packed.budget.header`）挡下 |

最后一行是 R3 决定把 16 MiB/40,000 做成不可放宽上限的直接原因：当时文件门禁可以被
「恰好另一个预算也超了」掩盖。

## 2. 冻结预算表（R3.1/R3.2）

权威表见 Spec §3.3，实现集中在 `engine/chart/src/packed_limits_internal.hpp` 与实际检查点：

| 预算 | 冻结默认 | 检查点 | 诊断 |
| --- | --- | --- | --- |
| `maxPackedFileBytes` | 16,777,216 | Writer envelope（组装前）、`inspect` size、file bridge | `packed.budget.file_bytes` |
| `maxPackedDecodedBytes` | 16,777,216 | Header 预检、Writer section 总和 | `packed.budget.decoded_bytes` |
| `maxPackedSectionBytes` | 16,777,216 | directory 逐项（所有 role，R3.3 新增） | `packed.budget.section_bytes` |
| `maxPackedEntities` | 40,000 | Header 预检、Writer 实体计数 | `packed.budget.entities` |
| `maxPackedRequirements` | 40,000 | Header 预检、Writer 要求累计 | `packed.budget.requirements` |
| `maxPackedStrings` | 100,000 | Header 预检、Writer 字典、STR0 行数 vs payload | `packed.budget.strings` |
| `maxPackedReferences` | 100,000 | Header 预检、Writer 引用、REF0 行数 vs payload | `packed.budget.references` |

保留不变：40,000 实体、16 MiB Packed 文件、以及 CXT v2 展开侧既有
`maxCxtV2ExpansionEntities/Requirements`（40000，`engine/chart/src/cxt_v2_loader.cpp`
既有预估检查）。本轮未放宽任何 fixture 以迁就测试；40,000 实体容量 profile 仍在同一套
默认 limits 下通过（packedBytes 1,232,408 / decodedBytes 1,232,024）。

## 3. 诊断按字段拆分（R3.1）

| 旧诊断 | 现诊断 | 说明 |
| --- | --- | --- |
| `packed.budget.header`（五个计数聚合） | `packed.budget.entities` / `.requirements` / `.strings` / `.references` / `.decoded_bytes` | 逐字段预检，拒绝原因即被突破的字段 |
| `packed.header.invalid_size`（过大/过小合并） | 过小仍为 `packed.header.invalid_size`；过大改为 `packed.budget.file_bytes` | 文件门禁在所有入口同名 |
| `packed.io.file_limit` | `packed.budget.file_bytes` | reader 侧预检、writer envelope、byte 级校验共用一个码 |
| 新增 | `packed.budget.section_bytes` | 单 section 超预算，或 section 内部 u32 字段越界 |

R3 保留 `packed.constraints.invalid`（CNS0 行级）与 `packed.strings.count` /
`packed.references.invalid`（行数超出 payload）作为各自结构检查的诊断。

## 4. 入口一致性（R3.3）

`packed::encode` 增加第三参数 `PackedChartLimits limits = {}`，`PackedChartWriter::size`/
`writeAtomic` 把 limits 透传给它，因此 Writer 侧与 Reader 侧共用同一预算判定。每个新增用例
同时断言全部适用入口：

| 入口 | 预算检查来源 |
| --- | --- |
| `packed::encode` | profile 门禁 -> 计数预算 -> 身份 -> 字典预算 -> section/文件 envelope |
| `packed::inspect` | `inspect` 自身的 size/计数/逐 section 检查 |
| `packed::decode` | `inspect` + profile + identity |
| `PackedChartWriter::size` | `encode` + `inspect` |
| `PackedChartWriter::writeAtomic` | 同上，且拒绝发生在任何文件 I/O 之前 |
| `PackedChartReader::decode` | clamp 后的文件预检 + `packed::decode` |
| `PackedChartReader::read` | clamp 后的 `file_size` 预检 + `decode` |

自定义值验证使用「明显低于文件上限」的 section 值（对 `tapChart()` 取
`max(encodedBytes)`：等值成功、减 1 拒绝），以及每个字段的等值成功/减 1 拒绝对照。

## 5. limits 只收紧不放宽（R3.4 决策 D6）与零值策略（D7）

- **D6**：`PackedChartLimits` 的每个字段是上限而不是目标。入口按
  `min(请求值, 冻结默认值)` 求有效值（`limits_detail::effectiveLimits`），调用方可以收紧，
  但不能通过传入更大的值放宽 16 MiB 文件门禁与 40,000 实体门禁。值被 clamp 而不是报错，
  拒绝时的诊断说明超出冻结预算。
- **D7**：`0` 是字面上限，拒绝任何非零值，永远不表示「不限制」；不存在「零值即跳过」的
  后门（与 D4 的零 hash 处置一致）。七个字段的 0 值各有断言。
- 两者写入 Spec §3.3 与公共头 `limits.hpp` 注释；未新增 ADR，因为不改变 wire 语义、
  公共函数签名兼容（新增默认参数）。

## 6. checked arithmetic 审计（R3.4）

| 位置 | 处理 |
| --- | --- |
| directory `count * 32` | 改为 u64 乘积，并要求 `≤ UINT32_MAX`、等于 `directoryBytes`、且 `96 + directoryBytes ≤ file size`，**在 `reserve` 之前** |
| 实体/要求计数 | 要求总数用 u64 累加后一次性比较，避免逐个 `40000 - count` 的隐式前提 |
| Writer 输出 envelope | section 总和与 `96 + 32*count + decoded` 在 u64 中先算，先过 section/文件预算，再把 count/total/decoded/offset 经 `narrowU32()` 写入 |
| STR0 `dataBytes`/offsets/行数 | u64 累加后经 `narrowU32()`；`writeStringSection`/`writeReferenceSection`/`writeTimeSection` 改为返回 `core::Result<Section>`，不再丢弃 `stringRef` 失败 |
| STR0/REF0 行数 | 按剩余 payload 定界（每行最少 4 bytes offset / 2 bytes 行），再 `reserve` |
| IDN0 scope/path/step 计数 | 按剩余 payload 定界（scope ≥ 19 bytes、step ≥ 2 bytes、path ≥ 1 byte）后才 `reserve`；预留量另设上限，增长跟随实际解析行数 |
| IDN0 iteration、CNS0 lane | u32 wire 字段，超宽值拒绝，不再 `static_cast` 截断 |
| REQ0 ordinal delta | 既有 `*delta > *entityCount || ordinal > *entityCount - *delta` 溢出保护保留 |
| Beat 运算 | `packed_chart_primitives.cpp` 已有 checked add/subtract/multiply 与 LCM 溢出拒绝；`RationalBeat::create` 全程 checked，本轮复核无缺口 |
| ENT0 parent 上限 | `*entityCount + 1U` 改为 u64 加法 |

不可达但在报告中登记的窄化：`collectStrings()` 内部的 `static_cast<std::uint32_t>(i)` 与
META feature 行数（已改为 `narrowU32`）。字典规模先被实体/要求预算约束（≤40000 实体、
≤40000 要求），要到 `2^32` 条才可能回绕，需要 >100 GB 的 typed model，任何进程都无法构造；
其余 `static_cast<std::uint32_t>` 的位置都可证明 ≤ 40,000 或 ≤ 4。

## 7. 失败优先级（R3 退出条件）

Reader 逐项次序（写入 Spec §9）：

```text
size -> magic/version/header -> header CRC -> revision -> eventCount
  -> 逐字段计数预算
  -> directory 乘积与边界 -> 重复项 -> 目录项结构 -> 注册表决策
  -> section 预算 -> section CRC -> 布局 -> decoded 总和
  -> payload 结构（STR0/REF0/IDN0/META/TIME/ARCH/ENT0/流/CNS0/REQ0）
  -> profile 门禁 -> semanticIdentity 比对
```

Writer 次序：

```text
revision/flags -> profile 门禁 -> 实体/要求计数预算 -> semanticIdentity
  -> 字典字符串/引用预算 -> section 构建 -> 逐 section 预算 -> decoded/文件 envelope
  -> 组装
```

由此得到三条可断言的性质，均有用例：section 预算先于 section CRC（不触碰超预算 payload）；
注册表决策先于 section 预算（`BEH0` 仍报 `packed.directory.refused`）；任何预算/结构/profile
拒绝先于 identity 比对，绝不报 `packed.identity.mismatch`。

## 8. A13 裁定（决策 D8）

Header offset 84 的文字原为「REF0 中 asset 类引用数」，而 Writer 写的是 REF0 全部行数，
Reader 也按该值逐行读取并 `maxPackedReferences` 预检。R3 裁定：该字段是
**REF0 行总数**，Spec §5.1 改名为 `referenceCount` 并加注说明；asset 类引用改由 typed
reference kind（1=asset，见 §6.2）与资源闭包在 typed 模型层派生。此裁定只改命名与文字，
不改 offset、宽度、写出的值或 revision，也不新增计数。

## 9. 「暂不冻结」与「均有独立预算」的冲突

Foundation 原计划第 3.5 节写「decoded bytes、section bytes、prepare peak 和编译耗时暂不
冻结」，第 5 节验收标准又写「decoded bytes、展开数量、峰值内存和编译时间均有独立预算」。
R3 的处理：

- 在当前硬门禁中冻结并在实现中执行：decoded bytes、section bytes、实体/要求/字符串/引用
  计数、CXT v2 展开实体与要求（Spec §3.3）。
- 只作观测、不承诺阈值：prepare 峰值（`preparePeakBytes`，本阶段没有 prepare 实现）、
  展开与编译耗时、把 decoded section bytes 当作堆占用、IDN0 scope/path 解码后的对象内存。
  这些不构成关闭门禁，也不得用来放宽硬预算。
- 原 completed 计划保留历史文字，仅加两处澄清（第 3.5 节引用说明与第 5 节验收标准脚注），
  明确 Foundation 交付过的硬门禁仍只有 16 MiB 与 40,000，不伪造原阶段已实现这些预算。

## 10. 用例与结果（R3.5）

| 用例 | 覆盖 |
| --- | --- |
| `R3 a custom maxPackedSectionBytes is enforced by every entry point` | 七个入口的边界成功/减 1 拒绝，`read`/`writeAtomic` 保留旧文件 |
| `R3 every declared budget is reported by its own diagnostic` | 七个字段各自的等值/减 1 与三入口一致性 |
| `R3 zero budget values are literal ceilings, never unlimited` | 七字段 0 值 |
| `R3 a caller cannot relax the frozen Foundation budgets` | 声明 50000 实体 + 放宽 limit；>16 MiB + 全部其余预算放宽 |
| `R3 the directory count product is checked before any reservation` | u32 回绕的 count/dirBytes |
| `R3 CNS0 lane values outside the uint32 wire range are refused` | lane `2^32+2` |
| `R3 IDN0 declared counts are bounded by the payload before reservation` | scope/path/step 三个计数 |
| `R3 IDN0 iteration indices outside the uint32 wire range are refused` | iteration `2^32+2` |
| `R3 budget diagnostics stay ordered against structural and registry checks` | 预算 vs CRC、预算 vs 注册表 |
| `R3 budget rejection precedes every semantic and profile diagnostic` | 预算先于 profile/identity |
| `R3 the Writer budget gates run before any artifact or file output` | `encode`/`size`/`writeAtomic` 四组预算、文件 envelope 精确性、字节不变性、无落盘 |
| `R3-H04`（原 `R0-H04` 翻转） | `maxPackedSectionBytes=1` 在四入口一致拒绝 |

命令与结果：

```powershell
cmake --build --preset debug --target cuexis_chart_tests
.\out\build\debug\bin\cuexis_chart_tests.exe "[r3]"
.\out\build\debug\bin\cuexis_chart_tests.exe
.\out\build\debug\bin\cuexis_cxc_tests.exe
ctest --preset debug --no-tests=error
cmake --build --preset debug --target cuexis_format_check
python -B tools/check_docs.py
python -B -m unittest discover -s tools -p "check_docs*tests.py"
git diff --check
```

| 项 | 结果 |
| --- | --- |
| R3 用例 | **12/12 通过**（179 assertions；修复前 0/10、49 assertions 失败） |
| `cuexis_chart_tests` | **198/198 通过**（1648 assertions；含 40000 实体容量 profile） |
| `cuexis_cxc_tests` | **52/52 通过**（706 assertions） |
| 全量 Debug CTest | **663/663 通过**（338.78 s；1 个预先存在的 symlink 用例按平台跳过） |
| `cuexis_format_check` | 通过（本轮新文件与改动文件已 clang-format） |
| `check_docs.py` / 检查器单测 | 通过（230 个 Markdown、20 个 candidate JSON/CXT；6 个 unittest） |
| `git diff --check` | 干净 |
| 平台 | Windows x64 / MSVC 14.51 / preset `debug` |
| 未执行 | Release、MinGW、headless sanitize、hosted（R5）；A10-A12 探针（R4）；GPU smoke（本批次不涉及） |

## 11. 兼容性影响

- **由接受变拒绝**：超过冻结预算的文件/实体/要求/字符串/引用/decoded/section；
  `2^32` 以上 CNS0 lane 与 IDN0 iteration；行数超过 payload 的 STR0/REF0/IDN0 表；
  u32 回绕的 directory 计数。合法 candidate（含 40,000 实体容量 profile）不退化。
- **诊断改名**：见第 3 节；`packed.budget.header` 与 `packed.io.file_limit` 退役。
- **API**：`packed::encode` 新增带默认值的第三参数，源兼容；无 ABI 变化（candidate 内部入口，
  尚未进入公共 SDK 头清单）。
- **无 wire/revision 变化**：仍为 `flags=1`、`candidateRevision=1`；Writer 输出字节除
  section 内部 u32 检查外不变（用例断言预算配置不改变产物 bytes），Spec 10.2 golden 继续通过。
- **不放宽**：16 MiB、40,000 及 CXT 展开上限均未被调高。

## 12. 改动文件

- 新增：`engine/chart/src/packed_limits_internal.hpp`、`tests/chart/packed_budget_tests.cpp`
- 修改：`engine/chart/src/packed_chart_tables.cpp`、`engine/chart/src/packed_chart_io.cpp`、
  `engine/chart/include/cuexis/chart/packed_chart_tables.hpp`、
  `engine/chart/include/cuexis/chart/limits.hpp`、
  `tests/chart/chart_foundation_hardening_characterization_tests.cpp`（R0-H04 翻转）、
  `tests/chart/packed_chart_io_tests.cpp`（文件门禁码迁移）、`tests/chart/CMakeLists.txt`
- 文档：本报告、Spec §3.2/§3.3/§5.1/§9、completed Foundation 计划两处澄清、
  阶段报告 README、加固计划看板与状态行、`CURRENT_STATUS.md`

## 13. R3 退出条件对照

| 退出条件 | 证据 |
| --- | --- |
| 每个硬限制均有边界成功 | 第 4、10 节的等值用例（section/实体/要求/字符串/引用/decoded/文件） |
| 越界拒绝 | 每个字段减 1 的负例，断言具体诊断代码 |
| 自定义值测试覆盖所有公开候选入口 | 第 4 节矩阵；`encode`/`inspect`/`decode`/`size`/`writeAtomic`/`decode`/`read` |
| checked arithmetic | 第 6 节；目录乘积、envelope、窄化与 payload 定界均有负例 |
| 记录零值策略 | D7 与七字段 0 值用例，Spec §3.3 明示 |
| 记录错误优先级 | 第 7 节次序与三组顺序用例，Spec §9 同步 |
| 观测项不冒充硬门禁 | 第 9 节列表；未实现 prepare 相关预算不作承诺 |
| 不因 fixture 失败而放宽 | 40,000/16 MiB 未改；容量 profile 在同套默认 limits 下通过 |

## 14. 交接与 R4 前置

R3 关闭后，Spec §3.3 的七个预算在所有支持入口可执行，诊断逐字段稳定，零值与优先级合同
有测试。R4 接手：encode -> decode -> 完整语义比较 -> re-encode（含 A10 CAM0 delta、
A11 archetype 默认值选择、A12 TIME 次序）、40k 原始容量数据、CXT 阶梯端到端、真实 CXC
package、原子写回滚证据。R5 收口同 SHA 的 Linux/MSVC/MinGW 与 owner 交接。

R3 遗留的观测项（不阻塞，转 R4/R5 记录）：`validateFoundationProfile` 之外的父图环路检测
仍是每个实体分配一次 `visited` 的 O(n²) 遍历（40,000 实体时约 40,000 次 40 KB 分配），
耗时与峰值只作观测，未在本批次改算法。Stage 6 仍为 future，未因本批次获得任何新能力。
