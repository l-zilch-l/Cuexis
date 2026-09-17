# Chart Format Foundation Hardening R2 前置：负例基础设施与拒绝行为盘点

日期：2026-09-17

状态：R2 前置完成；R2 本体未实施（本轮不改变任何拒绝行为）

起始 SHA：`2603865`（R1 提交，`codex/chart-format-foundation-hardening`，起始工作区干净）

上游依据：[加固计划](../../../stage_plans/active/chart-format-foundation-hardening/plan.md) 第 4 节 R2、
[R1 报告](2026-09-17-r1-semantic-identity.md)、[R0 报告](2026-09-16-r0-baseline-and-reproduction.md)、
[Packed Chart Spec](../../../formats/PACKED_CHART_FORMAT.md)。

本记录只做准备：建立 R2 需要的两组独立负例、固定失败顺序、确认调用关系、盘点未知值与排序规则的
现状。它不宣称 H02/A01/A09 已修复，也不改变任何入口的接受/拒绝行为（新增用例全部断言当前行为，
基线保持全绿）。

## 1. 本轮交付

| 交付 | 位置 |
| --- | --- |
| 字节级 fixture 基础设施（定点读写、段定位、REQ0/CNS0/REF0 行定位、CRC 刷新、追加段、段改名） | `tests/chart/packed_fixture_support.hpp` / `.cpp`（新增，编入 `cuexis_chart_tests`） |
| A 组：typed Writer 负例（out-of-profile 模型 → encode → decode） | `tests/chart/packed_profile_rejection_preparation_tests.cpp`，`R2P-A` |
| B 组：外部 Packed bytes 负例（Writer 无法产生的 wire 值 → 定点补丁 + 重算段/头 CRC） | 同文件，`R2P-B` |
| 失败顺序策略与调用关系确认 | 本文件第 3、4 节 |

结果：R2 前置用例 **8/8 通过**，全量 Debug CTest 保持全绿（见第 8 节）。

## 2. 两组独立负例（对应 R2.1）

### 2.1 A 组：typed Writer 产出的非法产物

R1 的 hash 闭环让这类产物**无法被身份校验发现**：`encode` 会为 out-of-profile 模型计算并写入
一致的语义身份，因此只有 profile 校验能拦下它。这正是 R2 必须存在的理由。

| 模型变异 | 当前 encode | 当前 decode | R2 目标 |
| --- | --- | --- | --- |
| lane = 7（不在 [0,3]） | 成功（CNS0 写 lane 7） | `packed.constraints.invalid` | Writer 与 Reader 双侧拒绝 |
| action = "release" | 成功 | `packed.requirements.profile` | Writer 与 Reader 双侧拒绝 |
| domain = "candidate.lanes5" | 成功 | `packed.requirements.profile` | Writer 与 Reader 双侧拒绝 |
| interval = HalfOpenRange（range） | 成功 | **接受** | Writer 与 Reader 双侧拒绝（Spec 7.6 仅登记 point） |
| feature 缺失 / version 不同 / id 不同 | 成功 | **接受**（R0-H02 已记录） | 双侧拒绝 |
| constraint 数量 ≠ 1 | **拒绝** `packed.identity.constraints`（R1 前置校验） | 不适用（wire 只能表达一个） | 诊断命名统一 |
| 非空 effects | **拒绝** `packed.identity.effects`（R1 前置校验） | `packed.requirements.profile` | 诊断命名统一 |
| feature ID 重复 | **拒绝** `packed.identity.feature_duplicate`（R1） | 不适用 | 诊断命名统一 |

结论：A 组暴露的核心问题是 **Writer/Reader 判定不一致**（前四行：writer 接受、reader 拒绝），
以及 R1 引入的 hash 前置拒绝码与 profile 拒绝码的**命名分裂**。R2 需要统一成"登记 profile
拒绝"的一组稳定诊断，同时保持 R1 的 hash 前置校验不被绕过。

### 2.2 B 组：Writer 无法产生的外部 bytes

每条都在保持结构与 CRC 一致的前提下补丁，并断言拒绝**不是** `packed.identity.mismatch`。

| 补丁位置 | 当前诊断 | R2 目标 |
| --- | --- | --- |
| REQ0 kind = 2 | `packed.requirements.profile` | 保持拒绝，诊断统一 |
| REQ0 interval = 2 | `packed.requirements.profile` | 保持拒绝 |
| REQ0 effect 字节 = 1 | `packed.requirements.profile` | 保持拒绝 |
| REQ0 constraint index = 0 | `packed.requirements.profile` | 保持拒绝 |
| REQ0 domain 指向 action 类引用 | `packed.requirements.profile` | 保持拒绝 |
| CNS0 kind = 2 | `packed.constraints.invalid` | 保持拒绝 |
| CNS0 lane = 4 | `packed.constraints.invalid` | 保持拒绝 |
| Header packedVersion = 2 | `packed.header.invalid` | 保持拒绝 |
| Header flags = 3 | `packed.header.invalid` | 保持拒绝 |
| Header candidateRevision = 2 | `packed.header.invalid` | 保持拒绝 |
| 段码改成未登记值（TRN0 → ZZZZ） | `packed.directory.invalid` | 保持拒绝，诊断统一 |
| 语义段带 inspection flag（META flags=1） | `packed::inspect`/`packed::decode` **接受**；`PackedChartReader::decode` 拒绝 `packed.io.unknown_section` | 三个入口一致拒绝，且诊断不冒充"未知段" |
| 追加登记 DBG0（flags=1） | `packed.directory.invalid` | 按 D2 **接受并忽略** |
| 追加未登记 inspection 段（ZZZZ flags=1） | `packed.directory.invalid` | 需决策：Spec 5.2 允许"校验长度与 CRC 后忽略" |

## 3. 失败顺序策略（对应 R2.3）

现状顺序（`packed::decode`）：`inspect`（header/目录/段 CRC/声明计数）→ 段内容与语义解析
（profile 校验在此）→ 末尾重算语义身份并比对。

因此：

1. B 组全部用例都同时断言"命中预期诊断"且"不是 `packed.identity.mismatch`"，把这个顺序固定成
   合同：**profile/结构拒绝必须早于身份比对**，否则一个 hash 一致的非法产物会先报身份不符，
   掩盖真实原因。
2. **不需要**"结构及 hash 一致的非法语义"生成器：非法语义要么模型可表达（A 组，`encode` 自然
   写出正确 hash），要么无法表达（B 组，必须由 profile/结构校验先拦下）。R2 新增的任何校验都
   必须放在 decode 的段解析/语义验证阶段，不能放在身份比对之后。
3. 若 R2 决定把 profile 校验实现为"解码后的独立验证 pass"，则必须插在身份比对之前；本节的
   用例会在实现顺序错误时立刻失败。

## 4. 调用关系确认（对应 R2.4）

| 入口 | 调用方 | 性质 | 语义消费 |
| --- | --- | --- | --- |
| `packed::inspect` | `PackedChartWriter::size/writeAtomic`（对刚 encode 的 bytes 做 sizing）、`validateCandidateChartExtension`（结构预检 + 计数比对）、测试 | 结构/预算 | 不承诺语义 |
| `packed::decode` | `PackedChartReader::decode/read`、`validateCandidateChartExtension`（`playback=true` 条目）、测试 | 完整语义 + 身份校验 | 唯一语义入口 |
| `packed::semanticIdentity` | `encode`、`validateCandidateChartExtension`（R1.5 比对）、测试 | 身份 | 只读 typed 模型 |
| `PackedChartReader::read/decode` | 测试（当前无生产调用方） | 文件/内存桥 + 段注册表检查 | 委托 `packed::decode` |
| `PackedChartWriter::size/writeAtomic` | 测试（当前无生产调用方） | 编码 + sizing + 原子写 | 委托 `packed::encode` |

确认结论：

- 语义消费方（CXC candidate validator 与 reader 桥）都经过 `packed::decode` ✓；没有任何生产代码
  只靠 `inspect` 消费语义。
- **Stage 6 candidate Playback 尚未接线**，因此本轮与 R2 的 reader 收紧不会影响已发布路径；
  `PackedChartReader` 是后续阶段的公共边界，安全性要求与 `packed::decode` 一致。
- `inspect` 的职责已在 R1 的公共头注释写明（仅结构），R2.4 的"调用点明确"已满足。
- 发现：语义段带 inspection flag 时三个入口结果不一致（2.2 表最后两行之一），
  `PackedChartReader::decode` 用 `packed.io.unknown_section` 报告 flag 违规，诊断误导。

## 5. 未知值盘点（对应 R2.5）

| 未知内容 | 是否被静默忽略 | 是否被转成已知类型 | 依据 |
| --- | --- | --- | --- |
| Header packedVersion / flags bit / candidateRevision | 否，拒绝 | 否 | `R2P-B unknown header values` |
| 未登记语义段码 | 否，拒绝 | 否 | `R2P-B section registry` |
| 未登记 inspection 段（含 DBG0） | 否，一律拒绝 | 否 | 同上（DBG0 需按 D2 改为接受） |
| REQ0 requirementKind（wire 2） | 否，拒绝 | 否：decode 在 `*kind != 1` 后直接赋 `CanonicalRequirementKind::Tap`，不 cast 未知值 | `R2P-B patched REQ0 fields` + 静态 |
| REQ0 intervalKind（wire 2） | 否，拒绝 | 否；wire 1（range）被接受，属 H02 | 同上 |
| CNS0 constraint kind / lane 越界 | 否，拒绝 | 否 | `R2P-B patched CNS0 fields` |
| REQ0 effect set / constraint index 0 | 否，拒绝 | 否 | `R2P-B patched REQ0 fields` |
| 引用 kind 不匹配（domain 指向 action 行） | 否，拒绝 | 否 | 同上 |
| 语义段带 inspection flag | `packed::inspect`/`packed::decode` 忽略该 flag | 否 | `R2P-B a semantic section carrying the inspection flag` |
| 字典/身份内部次序（STR0、REF0、IDN0） | 见第 6 节 | 否 | 静态 |
| 未登记的 feature ID/version、lane 越界（typed 模型） | 见 2.1 | 否 | `R2P-A` |
| 未登记扩展/回退（opaque JSON、CXT AST、"失败后尝试 v4"） | 不存在此类回退路径 | — | 全仓检索无 source/AST 回退；R2 不得新增 |

R2 的"不静默忽略、不转成已知类型"目标在除下列三项外已满足：DBG0（按 D2 改为接受并忽略）、
语义段 inspection flag（需一致拒绝）、字典/身份内部次序（见第 6 节）。

## 6. A09 排序规则复核结论（静态，探针未执行）

Spec 要求（规范性文字）：

- §4.3：Writer 按 section code、UTF-8 字符串、规范 identity bytes 升序写出；"Reader 可以接受不同
  的 section 物理次序，但**字典、实体和记录的内部排序必须符合合同**"。
- §6.1 STR0："字符串去重并按 bytes 严格升序"；§6.2 REF0："按 `(kind, string bytes)` 去重排序"；
  §6.5 IDN0：identities 按 canonical bytes "升序排列、严格去重"，并由此定义 entity ordinal 与
  ENT0 顺序。

当前实现（静态复核 `engine/chart/src/packed_chart_tables.cpp` 的 decode）：

- STR0 只校验 offsets 单调不减、区间完整；**不校验严格升序，也不校验去重**。
- REF0 只校验行数与引用目标合法；**不校验 `(kind, id)` 升序与去重**。
- IDN0 逐条读取并把文件顺序当作 entity ordinal；**不校验 identity bytes 升序与去重**（R1 的
  语义身份排序发生在预映像与 Writer 侧，Reader 端只要求 identityCount 与实体数一致）。

结论：Reader 对内部次序的要求**未实现**，属与"未知值不得静默忽略"同族的缺口；但注意
IDN0 次序在语义上被吸收——语义身份与 entity ordinal 无关，因此非规范次序文件仍会通过 R1 的
身份校验，只是 `write(decode(x)) != x`（Spec 10.3 只对规范输入承诺字节相同，因此这本身不违反
10.3）。

R2 建议（有证据支撑、风险低）：新增 STR0/REF0/IDN0 的严格升序与去重校验。方向安全性：仓库的
Writer 已经产出规范次序（STR0 排序去重、REF0 按 `(kind,id)` 排序去重、IDN0 按 canonical bytes
排序且 R1 拒绝重复身份），因此该校验不会拒绝任何本仓库产出的合法产物；全量用例可验证这一点。

未执行的探针（本报告如实记录，不作为已验证结论）：需要"替换既有段 payload 并重新序列化"的
fixture 能力（当前基础设施只支持定点补丁、改段码与追加段）。R2 实施该校验时必须同时补：
(a) 本仓库 Writer 产物通过新校验的正例；(b) 手工构造的非升序/重复 STR0、REF0、IDN0 负例。
建议把 (b) 与 R4 的 tamper 套件一并实现，避免为单批次重复构建重序列化器。

## 7. R2 实施清单（由本轮结论导出）

1. **统一 profile 拒绝**：Tap/point、feature ID/version、domain/action、lane `[0,3]`、constraint
   数量、空 effects 在 Writer 与 Reader 双侧拒绝，并用一组稳定诊断命名（含与 R1 的
   `packed.identity.*` 前置校验的关系说明，不互相替代）。
2. **D2**：`inspect`/`decode` 接受并忽略 DBG0（flags 必须为 1，只校验长度与 CRC），翻转 `R0-A01`
   与 `R2P-B the registered DBG0 inspection section is currently refused`。
3. **flag 一致性**：语义段带 inspection flag 必须在 `inspect`、`packed::decode`、
   `PackedChartReader::decode` 三个入口一致拒绝，且诊断不冒充未知段。
4. **未登记 inspection 段**：按 Spec 5.2 决定"忽略"还是"拒绝"，并把决定写入 Spec 与诊断。
5. **A09**：按第 6 节实施次序/去重校验并补正负例（负例可并入 R4）。
6. **不得新增回退**：不引入 opaque JSON、CXT AST 或"失败后尝试 v4"。
7. 已翻转的 R1 用例与保持未翻转的 `R0-H02`/`R0-H04`/`R0-A01` 与新的 `R2P-*` 用例必须在 R2
   完成后一并给出最终状态（翻转或保留的理由）。

## 8. 命令、平台与结果

```powershell
cmake --build --preset debug --target cuexis_chart_tests
ctest --preset debug -R "R2P" --output-on-failure
```

| 项 | 结果 |
| --- | --- |
| R2 前置用例 | **8/8 通过**（断言当前行为；R2 实施后逐条翻转） |
| 全量 Debug CTest | **650/650 通过**（301.35 s；R1 后为 642，本轮新增 8 例） |
| `cuexis_format_check` | 通过（新测试文件已 clang-format） |
| `check_docs.py` | 通过（228 个 Markdown、20 个 candidate JSON/CXT） |
| 平台 | Windows x64 / MSVC 14.51 / preset `debug` |
| 未执行 | Release、MinGW、headless、hosted（R5）；A09 负例探针（第 6 节） |

## 9. 改动文件

- 新增：`tests/chart/packed_fixture_support.hpp`、`tests/chart/packed_fixture_support.cpp`、
  `tests/chart/packed_profile_rejection_preparation_tests.cpp`
- 修改：`tests/chart/CMakeLists.txt`
- 文档：本报告、阶段报告 README、加固计划看板与 `CURRENT_STATUS.md`

无生产实现改动：本轮是准备，不是修复。
