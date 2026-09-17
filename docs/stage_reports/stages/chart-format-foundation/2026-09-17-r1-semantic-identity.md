# Chart Format Foundation Hardening R1：Semantic Identity 闭环

日期：2026-09-17

状态：R1 完成；R2-R5 未开始

起始 SHA：`1215567`（R0 决策记录提交，`codex/chart-format-foundation-hardening`，起始工作区干净）

上游依据：[加固计划](../../../stage_plans/active/chart-format-foundation-hardening/plan.md)、
[R0 基线与复现](2026-09-16-r0-baseline-and-reproduction.md)、
[Packed Chart Spec](../../../formats/PACKED_CHART_FORMAT.md)、
[CXC Spec](../../../formats/CXC_FORMAT.md)。

本轮只关闭 R1（语义身份闭环）与随其落地的决策 D1/D3。它不宣称 H02、H04、A01 已修复，
不宣称取得 hosted PASS，也不把 Debug 全量结果当作 Release/MinGW/Linux 证据。

## 1. 范围与合同决策

按计划第 4 节 R1 的六条要求实施，对应决策：

| 决策 | 内容 | 落地方式 |
| --- | --- | --- |
| R1.1 | 单一 typed 语义预映像 + SHA-256，复用 core hash 工具 | 新增 `semanticPreimage`/`semanticIdentity`/`semanticIdentityHex`，使用 `core::detail::Sha256`，不读字典索引、内存布局或文件 bytes |
| R1.2 | hash 前置合法性与规范化 | 预映像前置校验（唯一性、父图、引用、资源闭包、Beat、浮点有限值、负零、排序），重复项拒绝而非排序后静默去重 |
| R1.3 | Writer 发布前校验并写 Header | `encode` 在任何 section/字节构建之前调用 `semanticIdentity`，并把摘要写入 Header 偏移 32 |
| R1.4 | Reader 重算比对，不发布部分结果 | `packed::decode` 在结构/预算/语义校验完成后重算比对，不一致返回 `packed.identity.mismatch`，不构造返回 chart |
| R1.5 | CXC 用实际解码结果比对 compiledSemanticIdentity | `validateCandidateChartExtension` 在 `decode` 成功后重算语义身份并比对；`artifactIdentity` 仍独立比对 exact entry bytes |
| R1.6 | 零 hash 产物处置 | 按 D4：不递增 revision，零摘要与任何不一致摘要一律拒绝；Spec 10.1 补澄清句（须重新生成） |
| D1 | IDN0 按 Spec 6.5 | scope/path 表 + 索引式 tag 1；实体序改为 canonical identity bytes 序 |
| D3 | CXC 闭包容纳候选 playback entry | `cxc_package.cpp` 的 `reachable` 纳入登记扩展 `cuexis.chart-entry.v1` 中 `playback=true` 声明的 path；CXC Spec 5 补澄清句 |

明确非目标（保持 R2-R5）：不收紧 profile 拒绝（range/feature/domain/lane 的登记校验属 R2）、
不动预算与 checked arithmetic（R3）、不做 CXT 展开等价/40k 往返/回滚（R4）、不做 Release 与
hosted 验证（R5）、不新增公共 SDK 类型或 capability、不改变 v4 identity 与 FrameDigest 算法。

## 2. 实现

| 文件 | 内容 |
| --- | --- |
| `engine/chart/src/packed_semantic_identity.cpp`（新增） | Spec 10.1 预映像写入器、前置校验、SHA-256；6.5 canonical identity bytes 与 UUID 解析 |
| `engine/chart/src/packed_identity_internal.hpp`（新增） | 表编解码与预映像共用的内部 helper 声明 |
| `engine/chart/include/cuexis/chart/packed_chart_tables.hpp` | 新增 `PackedSemanticIdentity`、`semanticPreimage`、`semanticIdentity`、`semanticIdentityHex`；写明 `decode` 会校验身份、`inspect` 仅结构检查（D5） |
| `engine/chart/src/packed_chart_tables.cpp` | encode 先校验并写 Header 摘要；decode 末尾重算比对；IDN0 改 6.5 表；实体序与 parent 解析改用 canonical identity bytes（消除原字符串 key 的碰撞类） |
| `engine/chart/src/packed_chart_io.cpp` | 删除 344 行未被调用、仍按旧 IDN0 布局的重复解码器（A05） |
| `tools/cxc_common/src/cxc_candidate.cpp` | R1.5：用解码结果重算的语义身份比对 `compiledSemanticIdentity` |
| `engine/cxc/src/cxc_package.cpp` | D3：闭包纳入登记扩展声明的 playback entry |
| `docs/formats/PACKED_CHART_FORMAT.md` | 10.1 补零摘要/不一致摘要必须拒绝且旧产物须重新生成的澄清 |
| `docs/formats/CXC_FORMAT.md` | 5 补闭包关系：仅登记扩展的 `playback=true` 条目路径进入闭包 |

预映像顺序完全按 Spec 10.1：域串 + NUL + u16(5)、chartId UUID16、mainMusic、默认相机、
features、TIME 头、tempo、stop、资源闭包、按 canonical identity bytes 排序的实体、三个 u32(0)。
`B(beat)` 写 i64 补码 numerator + u64 正 denominator；`I(identity)` 写 u32 长度 + 6.5 canonical
identity bytes；浮点写原始 bits 并把 `-0` 规范为 `+0`；不使用 varint。

## 3. 验证矩阵

执行 SHA 为 R1 改动后的本地提交；平台 Windows x64 / MSVC 14.51 / preset `debug`。

| 计划要求的验证 | 用例 | 结果 |
| --- | --- | --- |
| Spec 独立 golden 与实际预映像匹配（不是同一实现算出的两个值） | `R1 semantic preimage matches the frozen empty golden`（165 B / `9372e8f7…`）、`R1 semantic preimage matches the frozen one-tap-lane2 golden`（312 B / `9b1714dc…`），两者直接取自 Spec 10.2 | 通过 |
| 允许重排的输入次序不改变语义身份 | `R1 semantic identity ignores input ordering that the format does not order`：实体顺序反转后 digest 与原 canonical bytes 都不变 | 通过 |
| 实体值/Beat/引用等语义变化会改变身份 | `R1 semantic identity changes when semantic content changes`：lane、requirement Beat、feature version、entity identity、默认相机位置、mainMusic、action 七个维度 | 通过 |
| 修改语义 bytes 并重算 section/Header CRC 后旧 hash 仍被拒绝 | `R1 reader rejects semantic bytes that no longer match the header digest`：改 META chartId 一个 bit → 重算 META 段 CRC 与 Header CRC → `inspect` 通过、`decode` 返回 `packed.identity.mismatch` | 通过 |
| 修改 Header hash 并重算 Header CRC 被拒绝 | `R1 reader rejects a rewritten header digest even after a header CRC refresh`（含把摘要改回全零） | 通过 |
| manifest hash 格式正确但内容不符被拒绝 | `R1-H01 CXC validation rejects a compiledSemanticIdentity that does not match`（`cxc.candidate.compiled_identity_mismatch`）；`R1 CXC validation rejects an artifactIdentity that does not match the entry bytes`（`cxc.candidate.artifact_identity_mismatch`） | 通过 |
| artifact 与 semantic identity 职责分离 | 同一候选包分别校验 entry SHA-256 与重算语义身份，两类负例互不替代 | 通过 |
| v4 identity 与 FrameDigest 回归不变 | 全量 Debug CTest 642/642，含 `prepared_semantic_identity_tests`、playback identity/digest 与 FrameDigest 用例 | 通过 |
| 前置合法性与规范化（唯一性/父图/引用/闭包/Beat/有限值/负零/排序） | `R1 hash preconditions fail with stable errors instead of throwing` 的 13 个 SECTION，全部返回稳定错误码且不抛异常 | 通过 |
| D1：IDN0 按 6.5 表 | `R1-A02 IDN0 stores the Spec 6.5 scope and path tables`（scopeCount/pathCount/identityCount）、`R1 generated identities round trip through the Spec 6.5 scope and path tables`（共享 scope/path、按 iteration 区分身份、parent 经新布局仍可解析、重新编码字节相同） | 通过 |
| D3：Spec 形状候选包可打包与校验 | `R1-A03 the Spec-shaped compiled playback entry is admitted by the CXC closure`；`R1-A03 only extension-declared playback entries join the CXC closure`（非 playback 条目与未声明条目仍 `cxc.entry.unlisted`） | 通过 |
| Writer 不发布非法产物 | `R1-H01 Packed writer publishes the recomputed semantic identity in the header`（Header 摘要等于重算值且不含零摘要） | 通过 |

### 3.1 前置校验与稳定错误码

| 错误码 | 触发条件 |
| --- | --- |
| `packed.identity.invalid_uuid` | chartId 或实体身份不是规范 UUID 文本 |
| `packed.identity.duplicate_identity` | 两张实体身份 bytes 相同 |
| `packed.identity.parent_missing` | parent 不在本图实体集合中 |
| `packed.identity.parent_cycle` | parent 指向自身或形成环 |
| `packed.identity.feature_duplicate` | feature ID 重复 |
| `packed.identity.requirement_duplicate` | 同一实体 localId 重复 |
| `packed.identity.constraints` | constraint 数量不是恰好 1 |
| `packed.identity.component_duplicate` | 同一实体出现两个同种类 Component |
| `packed.identity.effects` | 非空 effect set |
| `packed.identity.interval` | range 缺 end beat，或 point 携带 end beat（会被 wire 静默丢弃） |
| `packed.identity.non_finite` | f32/f64 非有限值 |
| `packed.identity.beat` | Beat denominator 非正 |
| `packed.identity.timing_order` | tempo 或 stop 共享同一 Beat |
| `packed.identity.camera_type` | 默认/组件相机类型不是 perspective |
| `packed.identity.scope_chart` | generated identity 的 scope chartId 与 META 不一致（编码与解码两侧） |
| `packed.identity.generated_path` | generated path 为空或最后一步是 indexed |
| `packed.identity.mismatch` | 解码后重算摘要与 Header 不一致 |

`packed.identity.invalid_uuid` 在编码与解码路径都存在；其余为编码（Writer 前置）或解码
（Reader 结构）侧，两侧对同一规则的命名保持一致。

后续更新（R2，2026-09-17）：上表中属于 Spec 7.6 profile 规则的三项已统一到
`packed.profile.*` 诊断族，现名分别为 `packed.profile.constraints`、`packed.profile.effects`、
`packed.profile.interval`；判定条件与拒绝位置未变。当前代码名以
[R2 报告](2026-09-17-r2-profile-rejection.md) 为准。

## 4. 本轮新发现

| ID | 内容 | 处置 |
| --- | --- | --- |
| A05 | `engine/chart/src/packed_chart_io.cpp` 携带 344 行 `[[maybe_unused]]` 的重复 Packed 解码器 `decodeCandidate`，无人调用，且仍按 R1 之前的内联 IDN0 布局读取。它会让任何误用者（含未来工具）读到错误身份 | 已删除；该文件只保留 header/目录 sizing 与 reader 桥。全仓检索确认无调用点 |
| A06 | Writer 可能把异常抛过模块公共边界：`writeEntitySection` 的 `order.ordinals.at()`（悬挂 parent）与 REQ0 写入的 `std::get<LaneConstraint>(requirement.constraints[0])`（空约束） | 已由预映像前置校验拒绝（`packed.identity.parent_missing`/`packed.identity.constraints`），`writeEntitySection` 亦改为查找并返回错误，双层防护 |
| A07 | `CanonicalResourceClosure` 在引擎内没有任何生产者，只有类型声明 | 本轮不接线：预映像按 Spec 10.1 第 9 步从 chart 的资源引用（mainMusic + Renderable mesh/material）派生规范资源集合，并将其记为 R4/R5 待决（接线或移除该字段） |
| A08 | META 只保存默认相机位置；`defaultTransform` 的 rotation/scale 既不在 wire 也不在 10.1 预映像中，因此仅默认相机旋转不同、语义不同的两张图会有相同语义身份 | 记为本轮观察，建议 R3/R5 在 Spec 10.1/7.3 明确该覆盖范围（本轮不改变已接受合同） |
| A09 | Reader 不校验 STR0/REF0/IDN0 的内部升序与去重，只保证语义身份与次序无关；一个非规范次序的文件仍可解码 | 记为 R2 的排序规则复核输入（计划 R0.5/R2.5 范围） |

## 5. 命令、平台与实际结果

```powershell
# 构建与全量测试（VS Developer shell，Windows x64 / MSVC 14.51）
cmake --preset debug --fresh        # R0 已建立；本轮增量复用同一 build 目录
cmake --build --preset debug
ctest --preset debug --no-tests=error
ctest --preset debug -R "R1" --output-on-failure
cmake --build --preset debug --target cuexis_format_check
python -B tools/check_docs.py
git diff --check
```

| 项 | 结果 |
| --- | --- |
| 全量 Debug CTest | **642/642 通过**（最终一次 305.86 s，含 A05 死代码删除、A06 前置校验与 clang-format 之后的复验；R0 基线为 621，R0 报告后为 632） |
| R1 focused | **17/17 通过** |
| `cuexis_format_check` | 通过（clang-format 清理后复检） |
| `check_docs.py` | 通过（226 个 Markdown、20 个 candidate JSON/CXT） |
| 未执行 | Release、MinGW、headless、sanitizer/coverage、external consumer 加固后复验、hosted（均属 R5；external consumer 已在全量 Debug 中运行） |

## 6. 改动文件

- 新增：`engine/chart/src/packed_semantic_identity.cpp`、`engine/chart/src/packed_identity_internal.hpp`、`tests/chart/packed_semantic_identity_tests.cpp`
- 修改：`engine/chart/CMakeLists.txt`、`engine/chart/include/cuexis/chart/packed_chart_tables.hpp`、`engine/chart/src/packed_chart_tables.cpp`、`engine/chart/src/packed_chart_io.cpp`、`engine/cxc/src/cxc_package.cpp`、`tools/cxc_common/src/cxc_candidate.cpp`
- 测试翻转：`tests/chart/chart_foundation_hardening_characterization_tests.cpp`（H01 两例、A02 一例翻转为 R1 合同断言并改 `[r1]` 标签）、`tests/cxc/cxc_candidate_extension_tests.cpp`（重写为 Spec 形状路径与 identity 比对）
- 文档：`docs/formats/PACKED_CHART_FORMAT.md`、`docs/formats/CXC_FORMAT.md`
- 本报告、阶段报告 README、加固计划看板与 `CURRENT_STATUS.md`

## 7. 仍未关闭与 R2 前置

R0 表征用例中**保持未翻转**的（属 R2/R3 范围）：

- `R0-H02 Range requirements outside the registered profile are encoded and decoded`（R2）
- `R0-H02 Requirements are accepted without the registered feature declaration`（R2）
- `R0-H04 A custom maxPackedSectionBytes does not constrain any entry point`（R3）
- `R0-A01 A registered DBG0 inspection section is rejected by the table reader`（R2，按 D2 接受并忽略）

R2 的输入：D2（DBG0 接受并忽略）、A09（排序规则复核）、H02 的 profile 严格拒绝（Tap/point、
feature ID/version、domain/action、lane、constraint 数量、空 effects 的双侧独立负例），以及
inspect/decode 调用关系的最终确认。R1 已经把这些规则中的"可哈希性"部分（constraint 数量、
effects、interval/endBeat 一致性、重复项）固定在预映像前置校验里，R2 需要补齐的是**登记
profile**层面的拒绝语义与独立外部 bytes 负例，不能与 R1 的 hash 前置检查互相替代。

本地提交：见本报告提交信息；未推送、未触发远端工作流（遵守计划第 4 节授权边界）。
