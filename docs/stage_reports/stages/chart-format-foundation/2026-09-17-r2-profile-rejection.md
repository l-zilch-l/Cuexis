# Chart Format Foundation Hardening R2：profile、section 注册表与内部次序严格拒绝

日期：2026-09-17

状态：R2 完成；profile、section 注册表/flag 策略与内部次序在所有支持入口一致拒绝

起始 SHA：`06ca6a9`（R2 前置提交，`codex/chart-format-foundation-hardening`，起始工作区干净）

上游依据：[加固计划](../../../stage_plans/active/chart-format-foundation-hardening/plan.md) 第 4 节 R2、
[R2 前置报告](2026-09-17-r2-prerequisites.md)、[R1 报告](2026-09-17-r1-semantic-identity.md)、
[R0 报告](2026-09-16-r0-baseline-and-reproduction.md)、
[Packed Chart Spec](../../../formats/PACKED_CHART_FORMAT.md) 第 5、6、7、9 节。

本轮兑现 R2 前置清单第 7 节的 7 项，并把该清单导出的三处缺口（Writer/Reader 判定分裂、
入口不一致、A09 次序未校验）闭合。wire revision、`flags=1`/`candidateRevision=1`、
Header 布局与 semanticIdentity 算法均未改变。

## 1. 统一 profile 判定（R2.1/R2.2）

新增单一实现 `engine/chart/src/packed_profile.cpp`（内部头 `packed_profile_internal.hpp`），
`packed::encode` 与 `packed::decode` 调用同一个 `validateFoundationProfile()`：

- Writer 在任何 bytes 或身份产生之前判定，out-of-profile 模型不再可能被发布；
- Reader 在 typed 模型重建之后、`semanticIdentity` 比对之前判定；
- `PackedChartWriter::size/writeAtomic`、`PackedChartReader::decode`、
  `validateCandidateChartExtension` 全部经由这两个入口继承，不存在第二套判定。

| Spec 7.6 规则 | typed Writer | 外部 bytes | 统一诊断 |
| --- | --- | --- | --- |
| 声明未登记的 feature ID | 拒绝 | 拒绝（META/STR0） | `packed.profile.feature` |
| 已登记 feature 的 version ≠ 1 | 拒绝 | 拒绝 | `packed.profile.feature` |
| 存在 REQ0 但未声明该 feature | 拒绝 | 拒绝（featureCount=0） | `packed.profile.feature` |
| requirement kind ≠ `tap`（wire 1） | 拒绝 | 拒绝（wire 2） | `packed.profile.requirement` |
| interval ≠ `point`（wire 0） | 拒绝（range 模型） | 拒绝（wire 1/2） | `packed.profile.interval` |
| domain ≠ `candidate.lanes4` | 拒绝 | 拒绝（STR0 文本） | `packed.profile.domain` |
| action ≠ `press` | 拒绝 | 拒绝（STR0 文本） | `packed.profile.action` |
| constraint 数量 ≠ 1 / 非 lane | 拒绝 | 拒绝（index 0、kind ≠ 1） | `packed.profile.constraints` |
| lane ∉ [0,3] | 拒绝 | 拒绝（CNS0 lane 7） | `packed.profile.lane` |
| 非空 effect set | 拒绝 | 拒绝（index ≠ 0） | `packed.profile.effects` |

没有 REQ0 的 chart 可以省略 feature（正例在 R2 用例中）；未登记的 feature 即使没有 REQ0
也被拒绝，因为 Foundation 注册表只登记一个 capability。

## 2. 诊断统一（R2 前置清单第 1 项）

`packed.profile.*` 成为 profile 规则的唯一诊断族。R1 预映像中同一条规则的三个分支改为
同一族代码：条件与拒绝位置不变，只有名称统一（R1 报告已加前向说明）。

| 旧名 | 现名 | 说明 |
| --- | --- | --- |
| `packed.requirements.profile` | `packed.profile.requirement` / `.interval` / `.domain` / `.action` / `.constraints` / `.effects` | 一条 REQ0 行内混在一起的四种原因拆开 |
| `packed.constraints.invalid`（lane > 3） | `packed.profile.lane` | 改由 typed 模型判定，Writer/Reader 完全一致 |
| `packed.constraints.invalid`（kind ≠ 1） | `packed.profile.constraints` | 注册表规则 |
| `packed.identity.constraints` | `packed.profile.constraints` | 同一规则的预映像分支 |
| `packed.identity.effects` | `packed.profile.effects` | 同上 |
| `packed.identity.interval` | `packed.profile.interval` | 同上（range 缺 end、point 带 end 均归此码） |
| `packed.requirement.unsupported`（Writer CNS0 前置） | `packed.profile.constraints` | 同一规则 |
| `packed.directory.unsupported` | `packed.directory.invalid` / `.flags` / `.refused` | 按原因拆分（见第 3 节） |
| `packed.io.unknown_section` | 删除 | IO 桥不再维护注册表 |
| `packed.io.directory_invalid` / `truncated_header` / `trailing_bytes` 等 | 删除 | IO 桥重复结构校验连同 `sections()` 一起移除 |

`packed.constraints.invalid` 仍用于 CNS0 行截断；`packed.identity.mismatch` 语义不变。

## 3. section 注册表与 flags（R2 前置清单第 2、3、4 项，决策 D2）

`inspect` 与 `decode` 共用 `classifySection()`，不再各自维护清单；`PackedChartReader::decode`
删除了自己的注册表循环与 `sections()` 重复结构校验（其结果此前被丢弃），只保留文件预算
检查后委托 `packed::decode`。

| 目录项 | 判定 | 诊断 |
| --- | --- | --- |
| Foundation semantic（META/TIME/STR0/REF0/IDN0/ARCH/ENT0/TRN0/REN0/CAM0/CNS0/REQ0），flags=0 | 解析 | — |
| Foundation semantic，flags=1 | 拒绝 | `packed.directory.flags` |
| `DBG0`，flags=1 | 接受并忽略 payload（D2） | — |
| `DBG0`，flags=0 | 拒绝 | `packed.directory.flags` |
| `BEH0`/`BHD0`/`ANM0`/`FXS0`（后续合同） | 即使为空也拒绝 | `packed.directory.refused` |
| 未登记，flags=1 | 校验长度与 CRC 后忽略（Spec 5.2） | — |
| 未登记，flags=0 | 拒绝 | `packed.directory.invalid` |
| flags > 1 | 拒绝 | `packed.directory.flags` |
| 重复 code | 拒绝 | `packed.directory.duplicate` |
| section CRC 不符 | 拒绝 | `packed.section.crc` |

决定已写入 Spec 5.2/5.3：未登记 inspection 段按 Spec 5.2 的许可忽略（不是静默忽略语义数据，
因为 inspection payload 从不参与判定、身份或资源解析）；后续合同段使用独立诊断，不冒充
「未知段」。`DBG0` 的 payload 不影响解码结果（用 semanticIdentity 比对证明）。

## 4. 失败顺序合同（R2.3）

`decode` 顺序保持为：`inspect`（header/目录/CRC/计数）→ 段解析与 profile → 末尾身份比对；
新增的 profile 判定插在身份比对之前。因此每个 B 组用例同时断言命中 profile/结构诊断且
**不是** `packed.identity.mismatch`。R2 前置报告已确认不需要「hash 自洽的非法语义」生成器，
本轮所有负例都保持结构（含 CRC）自洽。

## 5. 入口一致性（R2.4）

| 入口 | 现状 |
| --- | --- |
| `packed::inspect` | 结构 + header/目录/flag/注册表判定；**不**承诺 profile 与语义 |
| `packed::decode` | 结构 + 注册表 + profile + 语义 + 身份，唯一语义入口 |
| `PackedChartReader::decode` | 文件预算检查后完全委托 `packed::decode`，两者诊断必然相同（用例逐条比对） |
| `PackedChartWriter::size/writeAtomic` | 委托 `packed::encode`，继承 profile 拒绝 |
| `validateCandidateChartExtension` | 经 `packed::decode` 继承；out-of-profile 产物报 `cxc.candidate.packed_invalid`，不报身份不符 |

## 6. 未知值拒绝（R2.5）

| 未知内容 | R2 前 | R2 后 |
| --- | --- | --- |
| Header `candidateRevision ≠ 1` | `inspect` 静默忽略（仅 decode 拒绝） | 两个入口均拒绝 `packed.header.unsupported_revision` |
| Header `eventCount ≠ 0` | 静默忽略 | 拒绝 `packed.header.events` |
| ARCH mask bit 3/6/7 与 8..63 | 静默丢弃未实现 Component 声明 | 拒绝 `packed.arch.mask` |
| TRN0/REN0/CAM0 `changed` mask 未定义 bit | 静默忽略 | 拒绝 `packed.stream.mask` |
| RequirementKind / intervalKind / CNS0 kind / effect index | 已拒绝（R1/R2 前置） | 保持拒绝，诊断统一 |
| 未登记 revision/flags/section | 部分入口不一致 | 三入口一致（第 3、5 节） |

未新增任何 opaque JSON、CXT AST 或「失败后尝试 v4」回退。

## 7. A09 内部次序与 Writer 规范化

R2 前置第 6 节列为未执行探针的次序规则本轮全部实现，并用新 fixture 能力
（`replaceSection`、行区间解析、`reorderRows`）构造负例；所有正例由仓库 Writer 产物覆盖
（全量 CTest 通过即为正例证据）。

| 表 | 规则（Spec） | 诊断 | Writer 侧 |
| --- | --- | --- | --- |
| STR0 | 去重 + bytes 严格升序（6.1） | `packed.strings.order` | 已规范 |
| REF0 | 去重 + `(kind, text)` 严格升序（6.2） | `packed.references.order` | 已规范 |
| IDN0 scopes | 按 decoded tuple 排序去重（6.5） | `packed.identity.order` | 已规范 |
| IDN0 paths | 按逐步 `(nodeId, indexed)` 排序去重（6.5） | `packed.identity.order` | 已规范 |
| IDN0 identities | 按 canonical bytes 严格升序去重（6.5） | `packed.identity.order` | R1 已规范 |
| ARCH | 按 mask 升序（7.1） | `packed.arch.order` | 已规范（按 mask 建表） |
| REQ0 | `(entity ordinal, localId)` 严格升序（7.4） | `packed.requirements.order` | **本轮修正**：此前按模型次序写出 |
| CNS0 | 按 set payload 排序去重（7.5，即 lane 升序） | `packed.constraints.order` | **本轮修正**：此前按首次出现次序写出 |

两处 Writer 修正是「新 Reader 校验不得拒绝本仓库合法产物」这一方向安全要求的必要条件，
并由正例固定：同一语义的模型用不同 requirement/set/entity 排列编码出**逐字节相同**的产物。
`sorted`/`shuffled`、`ascending`/`reordered`、`chart`/`reversed` 三组比较都用 `==` 断言。

## 8. 本轮记录的新发现（静态，探针未执行）

| ID | 内容 | 影响 | 建议批次 |
| --- | --- | --- | --- |
| A10 | `writeComponentStream` 的 CAM0 delta 只在 `type`/`fovY` 不同时置 changed，忽略 `nearPlane`/`farPlane`；`makeArchetypes` 的默认值是「最后一个实体」。共享 camera archetype 的两个实体若只差 near/far，解码后其中一个会静默回到默认值 | Writer 数据丢失（round-trip 语义不等），不影响拒绝行为 | R4（相机/round-trip 覆盖） |
| A11 | Archetype 默认值取「最后出现的完整值」，Spec 7.1 要求出现次数最多、同频按字段 bytes 决胜 | 只影响 canonical bytes，不影响语义 | R4 |
| A12 | `writeTimeSection` 按模型次序写 tempo/stop，Reader 未校验 Beat 升序（Spec 6.4）；`writeMetaSection` 同样按模型次序，但 R2 的 feature 注册表使 META 至多一个 feature，实际不可达 | TIME 需规范化与校验 | R4 |
| A13 | Header `resourceReferenceCount` 在 Spec 5.1 中定义为「REF0 中 asset 类引用数」，Writer 写入的是 REF0 全部行数；Reader 把它当行数使用 | 计数口径与文档不符 | R3/R4（预算与计数口径） |

A10 是本轮顺带发现的最严重项，但属于 Writer 语义正确性而非 R2 的拒绝合同，且 R4 退出条件
本就要求「完整语义比较」覆盖 Camera，故不在本批次夹带修改；已在此明确交接。

## 9. R0/R1/R2 前置用例处置

| 用例 | 处置 |
| --- | --- |
| `R0-H01` ×2、`R0-A02` | R1 已翻转为 `[r1]` 合同断言，本轮未改 |
| `R0-H02`（range） | 翻转为 `R2-H02`：Writer 拒绝 `packed.profile.interval` |
| `R0-H02`（feature ×3） | 翻转为 `R2-H02`：Writer 拒绝 `packed.profile.feature` |
| `R0-H04`（`maxPackedSectionBytes` 不生效） | 保持 `[r0]`，归 R3 |
| `R0-A01`（DBG0 被拒） | 翻转为 `R2-A01`：三入口接受、payload 被忽略、语义不变 |
| `R2P-A` / `R2P-B`（8 例准备用例） | 由 `tests/chart/packed_profile_rejection_tests.cpp` 的 12 个合同用例取代，准备文件删除 |
| R1 身份用例 | 只更新 `packed.profile.constraints`/`.effects` 两个代码名，其余断言不变 |

## 10. 命令与结果

```powershell
cmake --build --preset debug --target cuexis_chart_tests cuexis_cxc_tests
.\out\build\debug\bin\cuexis_chart_tests.exe "[r2]" --reporter compact
ctest --preset debug --no-tests=error
cmake --build --preset debug --target cuexis_format_check
python -B tools/check_docs.py
git diff --check
```

| 项 | 结果 |
| --- | --- |
| R2 用例 | **12/12 通过**（214 assertions） |
| `cuexis_chart_tests` | **187/187 通过**（1473 assertions；含 40000 实体容量 profile） |
| `cuexis_cxc_tests` | **52/52 通过**（706 assertions） |
| 全量 Debug CTest | **652/652 通过**（327.82 s；1 个预先存在的 symlink 用例按平台跳过） |
| `cuexis_format_check` | 通过（本轮新文件已 clang-format） |
| `check_docs.py` | 通过（229 个 Markdown、20 个 candidate JSON/CXT） |
| `git diff --check` | 干净 |
| 平台 | Windows x64 / MSVC 14.51 / preset `debug` |
| 未执行 | Release、MinGW、headless sanitize、hosted（R5）；A10-A13 探针；GPU smoke（本批次不涉及） |

## 11. 兼容性影响

- **由接受变拒绝**：out-of-profile 模型与 bytes（lane、action、domain、range、effects、
  constraint 数量、feature 声明）；`candidateRevision ≠ 1` 现在被 `inspect` 拒绝；
  `eventCount ≠ 0`；未登记 semantic 段；后续合同段；ARCH/stream 未定义 mask bit；
  非规范内部次序。
- **由拒绝变接受**：登记的 `DBG0`（D2）与未登记 `flags=1` inspection 段（Spec 5.2）。
- **字节变化**：多 lane 或多 requirement 的 chart 在 Writer 侧改为规范次序；语义身份不变，
  Spec 10.2 golden 继续通过。
- **无 wire/revision 变化**：仍为 `flags=1`、`candidateRevision=1`；不新增 gameplay 能力。
- **诊断改名**：见第 2 节；无公共 API/ABI 变化。

## 12. 改动文件

- 新增：`engine/chart/src/packed_profile.cpp`、`engine/chart/src/packed_profile_internal.hpp`、
  `tests/chart/packed_profile_rejection_tests.cpp`
- 删除：`tests/chart/packed_profile_rejection_preparation_tests.cpp`
- 修改：`engine/chart/src/packed_chart_tables.cpp`、`engine/chart/src/packed_chart_io.cpp`、
  `engine/chart/src/packed_semantic_identity.cpp`、
  `engine/chart/include/cuexis/chart/packed_chart_tables.hpp`、`engine/chart/CMakeLists.txt`、
  `tests/chart/packed_fixture_support.{hpp,cpp}`、`tests/chart/CMakeLists.txt`、
  `tests/chart/chart_foundation_hardening_characterization_tests.cpp`、
  `tests/chart/packed_semantic_identity_tests.cpp`、
  `tests/cxc/cxc_candidate_extension_tests.cpp`
- 文档：本报告、Spec 3.2/4.3/5.2/5.3/7.1/7.4/7.5/7.6/9 澄清、R1 报告前向说明、
  阶段报告 README、加固计划看板与状态行、`CURRENT_STATUS.md`

## 13. R2 退出条件对照

| 退出条件 | 证据 |
| --- | --- |
| 每个支持入口均有登记 subset 正例 | 全部 encode/decode/writer/reader/CXC 正例用例与 40000 实体容量用例继续通过 |
| 每个支持入口有至少一种独立非法输入 | 第 1、3、6、7、9 节用例覆盖 Writer、外部 bytes、三入口与 CXC validator |
| diagnostic 稳定 | 第 2 节统一诊断族；负例逐条断言具体代码且断言不是身份不符 |
| 合法 candidate 不退化 | 全量 651/651 通过；Writer 字节变化仅限规范次序 |
| 非法输入不返回部分结果 | 所有拒绝都发生在 `chart` 发布之前（第 4 节失败顺序） |
| 不引入新 gameplay 能力 | 仅收紧拒绝与规范次序，未新增字段、section、flag 或执行路径 |

## 14. 交接结论

R2 关闭后：所有支持入口只接受登记 subset、登记 section 与规范内部次序，且诊断按原因稳定
区分。R3 接手预算与 checked arithmetic（含 `maxPackedSectionBytes`、A13 计数口径），R4 接手
端到端/容量/回滚（含 A10 相机 delta、A11 archetype 默认值、A12 TIME 次序），R5 收口同 SHA
hosted 与 owner 交接。Stage 6 仍为 future，未因本批次获得任何新能力。
