# Chart Format Foundation Hardening R0：基线与复现

日期：2026-09-16

状态：R0 基线与复现完成；决策清单待 owner 确认；R1 未开始

起始 SHA：`508584064379961bb2bedc95802f2a8e9ada0dc5`（`codex/chart-format-foundation-hardening`，
起始工作区干净）

上游依据：[加固计划](../../../stage_plans/active/chart-format-foundation-hardening/plan.md)、
[交接复核记录](2026-09-16-handoff-review.md)、
[Packed Chart Spec](../../../formats/PACKED_CHART_FORMAT.md)、
[CXC Spec](../../../formats/CXC_FORMAT.md)。

本记录只保存 R0 的基线、实际调用路径复核和缺陷复现。它不宣称 H01-H04 已修复，不宣称取得新的
hosted PASS，也不把本轮的本地构建说成最终 SHA 证据。

## 1. 基线记录

| 项 | 值 |
| --- | --- |
| 分支 | `codex/chart-format-foundation-hardening` |
| 起始 SHA | `508584064379961bb2bedc95802f2a8e9ada0dc5`（2026-09-16T17:49:23+08:00，计划提交） |
| 起始工作区 | 干净，无未提交修改 |
| 构建身份 | `26.08.01-1-dev`（`generated/cuexis/version.hpp`，UTC 构建） |
| SDK API | `0.7.0` |
| 平台 | Windows，x64 |
| 工具链 | MSVC 14.51.36231（VS 18 Community），CMake 3.3x，Ninja |
| preset | `debug`（`cmake --preset debug --fresh`） |
| 依赖 | `VCPKG_ROOT=D:\vcpkg` |
| Foundation 归档 | PR #24，合并提交 `13dab93e11ddb77f0f39807bf6b82f1da9eb4fd1`（2026-09-08） |
| 加固分支 hosted | 无运行记录（尚未推送；符合计划第 4 节授权边界） |

已有 CI 证据（查询，非本次运行）：`13dab93` 上三条 hosted 运行全部 success，均为 `master` 的
push 事件，2026-09-08：

- Linux Quality `34194672241`
- Windows MSVC `34194672234`
- Windows MinGW `34194672248`

合并 SHA 与测试 SHA 分离记录：以上是**合并提交**的运行，不是本轮加固验证。

### 1.1 本地 CTest 基线

```powershell
cmake --preset debug --fresh
cmake --build --preset debug
ctest --preset debug --no-tests=error
```

在 VS Developer shell 内：**621 个测试，100% 通过**，1 项按环境跳过
（`Secure file rejects physical containment escapes through symlinks`），总耗时 362.75 s。

同一命令在**非 Developer shell**下得到 7 个失败（`cuexis_external_consumer_*`），失败原因为
`LINK : fatal error LNK1104: 无法打开文件 kernel32.lib`：外部 consumer 门禁在测试内部再嵌套
一次 CMake configure/build，而该 shell 没有 `INCLUDE`/`LIB` 环境。这是环境记录，不是代码
finding；后续所有 CTest 证据均在 Developer shell 内取得。

加入本轮 11 个用例后，同一 preset 的**全量** CTest 为 **632 个测试全部通过**
（621 + 11，323.47 s），确认新增测试以及 `cuexis_cxc_tests` 的依赖 allowlist 变更没有破坏
架构门禁、consumer 门禁或既有用例。

## 2. 复核的实际调用路径

按照计划 R0.2，本轮阅读了真正被使用的路径，而不是未使用的辅助实现：

| 层 | 入口 | 位置 |
| --- | --- | --- |
| typed 编码 | `chart::packed::encode` | [packed_chart_tables.cpp](../../../../engine/chart/src/packed_chart_tables.cpp) |
| 结构检查 | `chart::packed::inspect` | 同上 |
| 语义解码 | `chart::packed::decode` | 同上 |
| 文件/原子输出与读取桥 | `PackedChartWriter::size/writeAtomic`、`PackedChartReader::read/decode` | [packed_chart_io.cpp](../../../../engine/chart/src/packed_chart_io.cpp) |
| 预算声明 | `ChartLimits`、`PackedChartLimits` | [limits.hpp](../../../../engine/chart/include/cuexis/chart/limits.hpp) |
| CXC candidate 校验 | `tools::validateCandidateChartExtension` | [cxc_candidate.cpp](../../../../tools/cxc_common/src/cxc_candidate.cpp) |
| CXC 包闭包 | `CxcPackageLoader` | [cxc_package.cpp](../../../../engine/cxc/src/cxc_package.cpp) |
| 容量测试 | low-reuse 40k | [chart_foundation_capacity_tests.cpp](../../../../tests/chart/chart_foundation_capacity_tests.cpp) |

关键结论：`PackedChartReader::decode` 不是第二份解码器，它先做自己的 DBG0 允许性检查，然后委托
`packed::decode`（`packed_chart_io.cpp`）；因此语义校验、预算校验只有一份实现，R1 的重算与
比对点唯一。

## 3. 复现（先复现，后修改）

本轮新增两个测试文件，全部为**表征测试**：断言当前（修复前）行为，供修复批次翻转为合同要求
的行为。基线因此保持全绿，同时每个 finding 都有可执行证据。

```powershell
cmake --build --preset debug --target cuexis_chart_tests cuexis_cxc_tests
ctest --preset debug -R "R0" --output-on-failure
```

结果：**11/11 通过**（0.70 s）；加入这些用例后的全量 CTest 为 **632/632 通过**。证据文件：

- [chart_foundation_hardening_characterization_tests.cpp](../../../../tests/chart/chart_foundation_hardening_characterization_tests.cpp)
- [cxc_candidate_extension_tests.cpp](../../../../tests/cxc/cxc_candidate_extension_tests.cpp)

### 3.1 已确认的原复核发现

| ID | 复现用例 | 当前被断言的行为 | 静态位置 |
| --- | --- | --- | --- |
| H01 写入 | `R0-H01 Packed writer emits an all-zero semanticIdentity header field` | Header 偏移 32..63 全为 0，而 Spec 10.1/10.2 已冻结预映像与 golden | `packed_chart_tables.cpp` encode：写 32 个零字节 |
| H01 读取 | `R0-H01 Packed reader accepts a tampered semanticIdentity once the header CRC is fixed` | 篡改语义 32 bytes 并重算 Header CRC 后，`inspect`/`packed::decode`/`PackedChartReader::decode` 仍接受 | decode 读取 `semanticHash` 后从未比对 |
| H01 CXC | `R0-H01 CXC validation accepts a compiledSemanticIdentity that does not match` | `artifactIdentity` 正确、`compiledSemanticIdentity` 为格式合法但无关的 64 位十六进制时，诊断为空 | `cxc_candidate.cpp` 只做 `isSha256` 形状检查 |
| H02 range | `R0-H02 Range requirements outside the registered profile are encoded and decoded` | interval `HalfOpenRange` 可编码并可解码回模型 | decode 只拒绝 `interval > 1`；encode 无 profile 校验 |
| H02 feature | `R0-H02 Requirements are accepted without the registered feature declaration` | feature 缺失、version 不符、ID 不符三种输入均被接受 | META feature 解码无登记校验 |
| H04 | `R0-H04 A custom maxPackedSectionBytes does not constrain any entry point` | 把 `maxPackedSectionBytes` 设为 1 后 `inspect`/`decode`/`size` 全部仍成功 | `limits.hpp` 声明该字段，但读取方从未使用 |

H03（40k 证据深度）确认：现有容量用例只调用 `PackedChartWriter::size` 并核对计数与字节数，
没有执行 encode→decode→完整语义比较→re-encode；原始测量数据与同 SHA hosted 证据仍缺，属 R4/R5。

### 3.2 R0 新发现

| ID | 内容 | 证据 | 归属批次 |
| --- | --- | --- | --- |
| A01 | 已登记的 `DBG0` 检查段被表读取器当作未知段拒绝：Spec 5.3 把 DBG0 登记为可选 inspection 段（flags 必须为 1），IO 桥的已知段清单也包含 DBG0，但 `packed::inspect/decode` 的注册表不含它，因此一个登记段被判为 `packed.directory.unsupported` | 用例 `R0-A01`（在合法产物上追加 DBG0 目录项与 payload 并重算 Header CRC，三种入口全部拒绝） | R2（并需决策 D2） |
| A02 | `IDN0` wire 布局与 Spec 6.5 不一致：Spec 定义 `u32 scopeCount` + scopes[] + `u32 pathCount` + paths[]，再以 scopeIndex/pathIndex 引用；实现写 `0/0` 并把完整 generated tuple 内联到每条 identity | 用例 `R0-A02`（IDN0 前两个 u32 为 0，identityCount 为 2） | R1/R2（并需决策 D1） |
| A03 | CXC v1 的 project-declared closure 不为编译产物留入口：`reachable` 只含 project 文档、Asset Index、asset sources 与 entry Chart（`cxc_package.cpp`），因此 Spec 12 规定的 `compiled/chart.packed` playback entry 在 `CxcWriter::write`/`CxcPackageLoader` 即被 `cxc.entry.unlisted` 拒绝 | 用例 `R0-A03` | R1/R4（并需决策 D3） |
| A04 | Foundation 的 CXC candidate「正例」fixture 从未被执行：`cxc_candidate_extension.valid.json` 使用全占位 identity（`1111…`/`2222…`/`3333…`）并声明 40000 计数，无法通过真实校验；仓库内无任何测试或 CMake 目标引用 `tests/fixtures/chart_format_foundation`，`validateCandidateChartExtension` 在本次改动前没有测试也没有可执行调用点 | 全仓检索无引用；本轮新增的 CXC 用例是它的第一个可执行入口 | R4（证据补齐） |

A04 直接对应计划 2.3 的「检查例程必须对应实际调用路径」与 R0.4 的「仅有 JSON 示例时不能填为
已验证」：F7 报告的正例只能算映射草案，不能算已验证的候选包。

## 4. 验收矩阵

执行 SHA 为本轮基线 `5085840`；「结果」栏是**修复前**状态。

| 合同条款 | 支持入口 | 实现位置 | 正例 | 失败例 | 诊断 | 兼容要求 | 结果 | 证据 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Spec 10.1/10.2 语义 hash 写入 | `packed::encode` | tables encode | 待 R1 golden | 无 | 无 | v4 identity 不变 | 不满足 | R0-H01 写入 |
| Spec 9 解码顺序末步 identity 校验 | `inspect`/`packed::decode`/`PackedChartReader::decode` | tables decode | 无 | 篡改 32 bytes 仍接受 | 无 | 无 | 不满足 | R0-H01 读取 |
| Spec 12 compiled identity = header.semanticIdentity | `validateCandidateChartExtension` | cxc_candidate.cpp | 无 | 形状合法但不符仍通过 | 仅格式诊断 | 无 | 不满足 | R0-H01 CXC |
| Spec 12 artifactIdentity = entry SHA-256 | 同上 | cxc_candidate.cpp | 正确值通过 | 不符被拒 | `cxc.candidate.artifact_identity_mismatch` | 无 | 满足 | R0 CXC artifact |
| Spec 7.6 kind=tap（wire=1） | decode REQ0 | tables REQ0 | 通过 | `kind!=1` 拒绝 | `packed.requirements.profile` | 无 | 满足 | 静态 |
| Spec 7.6 interval=point（wire=0） | decode REQ0 | tables REQ0 | 通过 | range 被接受 | 有码但不覆盖 | 无 | 不满足 | R0-H02 range |
| Spec 7.6 domain/action | decode REQ0 | tables REQ0 | `candidate.lanes4`/`press` | 其他值拒绝 | `packed.requirements.profile` | 无 | 满足 | 静态 |
| Spec 7.6 lane [0,3] | decode CNS0 | tables CNS0 | lane 0..3 | `lane>3` 拒绝 | `packed.constraints.invalid` | 无 | 满足 | 静态 |
| Spec 7.6 恰好一个 lane constraint、空 effects | decode REQ0 | tables REQ0 | 通过 | `effect!=0` 拒绝 | `packed.requirements.profile` | 无 | 满足（隐式单约束） | 静态 |
| Spec 7.6 feature ID/version 登记 | decode META | tables META | 登记值 | 缺失/版本不符仍接受 | 无 | 无 | 不满足 | R0-H02 feature |
| Spec 5.3 需要段与 DBG0 登记 | inspect/decode 注册表 | tables 目录校验 | 需要段通过 | DBG0 拒绝 | `packed.directory.unsupported` | 无 | 不满足（过度拒绝） | R0-A01 |
| Spec 6.5 IDN0 scopes/paths | IDN0 写入/读取 | tables IDN0 | 待决策 | 无 | 无 | 无 | 偏差 | R0-A02 |
| Spec 3.1 16 MiB 文件上限 | `encode` / `writeAtomic` / `read` | tables encode 尾部、io | 通过 | 超限拒绝 | `packed.budget.file_bytes`、`packed.io.file_limit` | 无 | 满足 | 静态 + 现有用例 |
| Spec 3.1 40000 实体 | `encode` | tables encode 头 | 通过 | 40001 拒绝 | `packed.budget.entities` | 无 | 满足 | 现有容量用例 |
| Spec 3.2 分配前检查与解码后重新计数 | encode/decode | tables encode 头、decode header 计数 | header 计数与目录/IDN0 有交叉校验（`decodedSum == decoded`、`identityCount == entityCount`） | 按 section 拆分的超额报告与逐字段解码后重新计数未见实现 | `packed.budget.header` 部分 | 无 | 部分（R3 复核） | 静态 |
| 预算字段独立生效 | `PackedChartLimits` | limits.hpp 声明 | 文件/实体/需求/字符串/引用生效 | `maxPackedSectionBytes` 完全无效 | 无 | 不放宽其他门禁 | 不满足 | R0-H04 |
| Spec 12 CXC 闭包容纳 playback entry | `CxcWriter`/`CxcPackageLoader` | cxc_package.cpp | Asset Index 路径可绕过 | Spec 路径 `cxc.entry.unlisted` | `cxc.entry.unlisted` | v4/CXC v1 不变 | 不满足 | R0-A03 |
| Spec 9 原子写入与失败保留旧产物 | `writeAtomic` | packed_chart_io.cpp | 临时文件 + 原子替换 | 失败回滚未测 | `packed.io.replace_failed` 等 | 无 | 未验证（R4） | 静态 |
| Spec 10.3 40k 完整往返 | 容量用例 | capacity tests | 仅 `size()` | 无 decode/语义比较 | 无 | 无 | 不足（R4） | H03 |

## 5. Section 清单对照（R0.5）

Spec 5.3 对 Foundation revision 1 的完整清单与实现一致，**没有发现遗漏或凭空支持**：

- 必需：META、TIME、STR0、REF0、IDN0、ARCH、ENT0、CNS0、REQ0 - 读取端要求集一致
  （`packed_chart_io.cpp` 与 `packed_chart_tables.cpp` 的必需表均含 META/TIME）。
- 条件：TRN0（bit 0）、REN0（bit 1）、CAM0（bit 4）- 实现按 component stream 写出，存在时才产出，
  读取端按 mask 应用。
- 拒绝：BEH0、BHD0、ANM0、FXS0 - 注册表不含，按未知段拒绝，符合 Spec 7.2 与第 8 节。
- 可选：DBG0（flags=1）- **未登记**，见 A01。

因此 A01 不是「Spec 摘要遗漏」，而是实现相对 Spec 注册表的偏差；修复方向必须由决策 D2 决定，
不能用「删掉 Spec 里的 DBG0」来让文档表面一致。

## 6. 决策清单（R0.6）

### 已决（在 R0 权限内，不改变已接受语义或公共 API）

- **D4 零语义 hash 产物的处置：不需要递增 candidate revision。** Spec 5.1 的
  `semanticIdentity` 字段与 10.1 的预映像合同在 revision 1 下已经冻结，写零是实现缺口而非
  wire 变更；仓库内不存在持久化的零 hash Packed fixture（`tests/fixtures/chart_format_foundation/`
  只有 CXT/JSON 文本，Packed bytes 都在测试运行时生成）。因此 R1 直接实现写入与重算比对，
  revision 1 拒绝任何与重算不一致的产物（含零值），**不引入「零值即跳过」后门**，也不递增
  revision。若 R1 实施中发现需要改变已接受 wire，则停下来先走 ADR/Spec 更新与 owner 接受。
- **D5 inspect 与 decode 职责：** `inspect` 只承诺结构与预算检查（不承诺语义校验）；
  `PackedChartReader::decode`/`packed::decode` 是唯一语义消费入口，R1 在 decode 内完成重算比对。
  该职责需要在公共头注释与调用点写明，但不需要新公共类型。

### 已决（owner 于 2026-09-16 确认，三选一均已裁定）

- **D1 IDN0 布局（A02）：按 Spec 6.5 修实现。** 选择方案 (a)：`writeIdentitySection`/decode 改为写读
  Spec 6.5 的 scopes/paths 表，并以 scopeIndex/pathIndex（加 indexed step 的
  iterationIndex）表达 tag 1。权威 Spec 不变、candidate revision 不变、已发布的 v4 identity 与
  FrameDigest 不受影响；canonical identity bytes（6.5 末段）与语义预映像保持同一形式，因此
  R1 的 hash golden 不受此次布局对齐影响。R1 负责实现并补 G 系列负例（非法 scopeIndex/
  pathIndex、未排序或重复 scopes/paths）。
- **D2 DBG0（A01）：让 `inspect`/`decode` 接受并忽略。** 选择方案 (a)：注册表承认 DBG0
  （flags 必须为 1），只校验长度与 CRC，不读取其中任何数据、不供 Runtime 使用；发行 Writer
  继续默认省略 inspection 段。属于使实现回到 Spec 5.3 已登记行为，不改 wire、不改 revision。
  实施批次 R2，并用 R0-A01 用例翻转验证。
- **D3 CXC 闭包（A03）：扩展闭包，容纳登记扩展声明的 playback entry。** 选择方案 (a)：在
  `cxc_package.cpp` 的 `reachable` 计算中，把已登记扩展 `cuexis.chart-entry.v1` 中
  `playback=true` 的 entry 路径纳入闭包（仅该登记扩展、仅 playback 条目，其余 entry 仍受
  原有项目闭包约束）。依据：CXC Spec 4 已要求 `playback=true` 的 entry「必须存在于同一个
  CXC」，而 Spec 5 的闭包枚举漏列该路径，属实现与 Spec 意图不一致；因此这是由真实集成缺口
  驱动的 CXC core 最小修正 + Spec 5 澄清句，不新增公共 API、不改 ZIP32/manifest 三字段合同、
  不需要 ADR。实施批次 R1（它同时解阻 R1.5 的 CXC 比对端到端证据），并在 CXC Spec 5 补一句
  闭包包含关系，R4 用真实候选包复核。

### 决策后的推进规则

- D1 已定：R1 可以同时完成预映像/hash 闭环与 IDN0 布局对齐，两者都不改变 revision。
- D2 已定：R1 不改 DBG0 行为；R2 实施并翻转 R0-A01。
- D3 已定：R1 实施闭包修正后，`compiled/chart.packed` 这一 Spec 形状路径必须能真实打包与
  校验成功；R0 的 Asset-Index 绕过路径（见 3.2/A03 与新建 CXC 用例）在 R1 落地后应改为
  使用 Spec 形状路径，把绕过降级为兼容性对照。

## 7. 本轮改动文件

- `tests/chart/chart_foundation_hardening_characterization_tests.cpp`（新增，11 个 R0 用例中的 7 个）
- `tests/cxc/cxc_candidate_extension_tests.cpp`（新增，11 个中的 4 个；`validateCandidateChartExtension` 的首个可执行入口）
- `tests/chart/CMakeLists.txt`、`tests/cxc/CMakeLists.txt`（注册新用例）
- `CMakeLists.txt`（`cuexis_cxc_tests` 依赖 allowlist 增加 `cuexis::cxc_tool_common`、`cuexis::chart`）
- 本报告与阶段报告 README、加固计划看板状态

未修改任何生产实现：本轮是复现，不是修复。

## 8. 未决项与 R1 前置

1. D1/D2/D3 已于 2026-09-16 由 owner 裁定（见第 6 节），R1 前置齐备，无未决合同阻塞。
2. R1 实现顺序建议：typed 预映像 + SHA-256 → encode 写入 Header → decode 重算比对 →
   CXC compiled identity 比对 → 独立 golden（empty 165 B / one-tap-lane2 312 B）与负例。
3. R1 必须验证的计划要求：允许重排不改变 identity、语义变化改变 identity、改 bytes 并重算段/头
   CRC 后旧 hash 仍被拒、改 Header hash 并重算 Header CRC 被拒、artifact/semantic 职责分离、
   v4 identity 与 FrameDigest 回归不变。
4. 本轮没有执行 Release、headless、MinGW、架构与 external consumer 门禁的加固后复验；这些属 R5。
5. 计划看板的 R0 行在本报告与决策确认后更新；Stage 6 保持 future。
