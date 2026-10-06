# S7A-3 设计收口与实施交接报告

状态：dated design evidence；S7A-3 文档与决策已收口，实施验收未完成

日期：2026-10-04

## 1. 授权、基线与结论

owner 要求按选定方案完成 S7A-3，并授权自主裁决以完成优秀的设计；
本轮沿用已明确选择的 **Documentation only for now**，只完成候选合同文档。
这不是执行附加文档中的操作指令，也不是后续实现、暂存、提交、合并或发行授权。

核对基线为 `stage-7`、HEAD `449e86497c7ccccbb4c996d7f571976ac7eee70c`。
工作树已有 runtime/CMake/测试/文档的未提交和未跟踪改动，均保留；
本轮不把它们计作新的实现成果，不编辑 owner 维护的 `AGENTS.md`。
编辑前文档副本保留在 `.dsh/scratch/2026-10-04-s7a3-doc-contract-before`，不是合同权威。

最终采用 **G2-S2 + G2-T4 + R2-K4 + P1-W1 + E1**。S2/T4/K4 和首次 Packed
candidate 物理合同不再留待实现时任选；未选方案只作历史比较。
**设计完成不等于 S7A-3 整批完成**：来源投影修正、Capsule 实现和 E1 实测仍必须关闭。
当前阶段状态只由 [CURRENT_STATUS](../../../../CURRENT_STATUS.md) 拥有。

## 2. 单一权威与裁决

| 内容 | 唯一正文 / 边界 |
| --- | --- |
| 来源解析与身份分区 | [Gameplay V2 Spec](../../../../formats/GAMEPLAY_V2_SPEC.md) §3.8.10；来源 / resolver 与有效执行 policy 分离 |
| 覆盖、窗口与终止 | 同 Spec §3.8.11；tail 的 checked D=end+grace，无 tail 的 D=end；timer 在输入前 |
| 竞争键、展开与 proof | 同 Spec §3.8.12；i64 priority / u64 tieRank 升序，同资源唯一，局部 first-eligible 唯一性证明 |
| Packed 物理字段与结构 hash | [Gameplay Capsule v2 format](../../../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md)；candidateRevision=2 |
| C++ typed、所有权与异常 | [Gameplay V2 ABI](../../../../api/GAMEPLAY_V2_ABI.md)；不把 wire opcode 当成公共 enum |
| 范围、依赖与门禁 | [Stage 7 计划](../../../../stage_plans/active/stage-07/plan.md) 与 [BATCH_GATES](../../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md) |
| 方案讨论与选择历史 | [决策登记](../decisions/2026-10-03-s7a-3-decision-register.md) 与 [W1 选择历史](../../../../proposals/gameplay-v2-acceptance/S7A-3_PACKED_W1_CANDIDATE.md)；不再复制物理表 |

针对旧草案与跨文档矛盾，已作如下明确处置：

1. 四个 Gameplay sections 使用统一 16-byte framing；**REF0 不加表头**，保持 Foundation raw rows。
2. `REQ0` 保留 `00 00` 且 count=0；`CNS0` 零 bytes/count=0；Header requirementCount
   与 ENT bit 2 改由 GPR0 交叉核对。revision 1 不被偷偷重新解释。
3. observe 没有 namespace/claimKey/pair/slot；consume 与 claim 不折叠。
   capacity=1 的全部占用候选共享 slot 0，claim 行号不分配新 slot。
4. K4 只按业务 pair 竞争，不用 hash、identity、源数组顺序或容器位置补平局；
   显式全实例表和有限 affine mixed-radix 两种来源在 prepare 后统一为 pair。
5. T4 的早释放 / 错过最后窗口可以立即失败；成功也立即终止资源，不被 grace 强制拖到 D。
   Requirement preparedGrace 与禁止的资源 declaredGapGrace 明确区分，不引入 recovery/handoff。
6. REF0 8–13 保留已选 kind 分配；kind 13 用固定 typed 子域补齐判定投影。
   **仅 13/0 是 capability**，timebase/solver 等投影不冒充能力，不滥用 Foundation kind 7。
7. 完整 typed 定义、来源、时基、窗口、closure 与 solver 有序列表均有物理字段；
   匿名 Measure 不要求新增现有公共类型的命名 ID 成员。
8. 结构 hash 保留 graphRevision、最终 grace 与有效 policy；排除来源 / resolver revision；
   解引用字典/table indices，Requirement↔Claim 通过 E/resourceId key 断环。
   content-artifact exact SHA-256 仍能区分不同来源及不同物理产物。
9. 旧“任何 wire 都未冻结”禁令明确限定为未授权编码；本次 Capsule 静态输入是限定例外，
   不延伸到 runtime state、Snapshot/Replay、FactId/CommitId 或公共 proof codec。

## 3. 字段追溯

表格只索引权威章节，不复制另一份 wire layout。

| 逻辑合同 | Capsule 字段落点 | 读入后的必要校验 |
| --- | --- | --- |
| graph/ruleset/normalization/coordinator | §5 GPH0 global record，§4 typed projections | 明确 token、实际上下文匹配、完整 prepared identity 不能只取 Header digest |
| Timebase / LatePolicy | §5 精确 rational 时基、四态测量参数与 late mode | Spec §3.7 的映射、可表示性与参数量级；不由 TIME f64 逆推 |
| Requirement identity / entity / actions | §6 GPR0，D/E/R12/R13 | E 与实体一致、phase 与引用唯一、无 missing/dangling |
| Pattern / arm-deadline 容量 | §6 capacities + §7.1 typed tree | 同一 compiler、包含性门禁；pending/absent 不变为 measured(0) |
| Measure / 判定域 / merged declarations | §7.2–§7.4 | grade ordered list、static geometry、稳定 ID 与名称冲突拒绝 |
| grace / timing / local close | §5 policy + §6 final value/windows/body/provenance | 不重跑 source rounding、不裁窗口；T4 end/body/D 一致性 |
| resource / relation / solver / claim | §8 五表 | capacity=1、slot 0、intent 保真、K4 pair 唯一、declaredGapGrace=0 |
| FactBinding 判定存在性 | §8 existence record + §4 13/7 | 只验存在性，不把 target/effect/material 拉进判定闭包 |
| feature/capability/resource closure | §4 refs、§5 global、各行 N | 从实际记录重新派生、逐项比对，保留基础表现资源 |
| semantic/artifact identity | §9 structure preimage + Foundation artifact hash | 无 row ordinal/pointer，自指 hash 禁止；原子 prepare 后才能发布 |

原始 duration 的 source min/max 已由 resolver 验证，未被 wire 携带；
Reader 只能重验最终 signed i64 非负域、T4 与执行上下文 accepted bounds，
不能声称重新验证不存在的原始字段。离线 decode 与可执行 publication 是两个结果层级。

## 4. 剩余实现与验收门禁

以下项目是实现证据门禁，不是待 owner 选择的语义方案。文档与决策已由
**G2-S2 + G2-T4 + R2-K4 + P1-W1 + E1** 闭合；若要改变已列语义、扩大 7A 范围或冻结新的公共编码，
必须另起决策记录。

以下均为 **待实现或待重新验证**，不是本轮完成的测试：

- [x] 核对 `engine/judgement/src/gameplay_assembler.cpp` 的来源身份投影：
  `writeChartProjection` 只写最终 `preparedGrace` 与有效的 `localClosePolicyToken`，
  explicit/inherited/default 及其来源引用只进入 content provenance；补充三来源同值等价测试，
  证明相同最终值与有效 policy 不改变 judgement/prepared identity。
- [x] 实现 Capsule bounded typed Reader/Writer、counts/ref/subdomain/hash 校验、owning file/memory
  桥接；旧 Reader 对 revision 2 早期拒绝，失败保持原 artifact/active publication。核心
  round-trip、negative Reader、atomic publication、file/memory 与 canonical golden 已由
  MSVC Debug 和 WSL GCC focused tests 验证；Clang 对照仍未取证。
- [x] 三类 typed entry（`gameplayGraph`、`packedChart`、`authorSource`）共用同一 prepare；
  entry 等价已增加逐字段 `semanticDiff` 断言。Chart v5 inline 与 CXT v2 emission 的完整
  逐字段 semantic diff、输入数组/参数/引用置换不变，仍待对应 authoring adapter 接线后验证。
- [x] 重验 S2 精确 single-hop/default、负值量化前拒绝、ties-even、量程和 overflow；
  重验 T4 bodyEnd/end/D 前一 Tick/相等/后一 Tick、zero grace 与提前终止。
- [~] 验证 K4 全实例/affine 展开、collision/overflow/未分配 rank、observe absence、
  consume/claim 保真、slot 0、held 无抢占，以及声明 objective 下的局部 proof。当前已补齐
  capacity、缺 policy/key、observe 伪占用、重复竞争键、legacy intent mismatch、grace
  override、置换稳定性与 observe/consume/claim 保真；运行期 held/slot 消费仍属 S7A-4。
- [~] E1 的 typed round-trip、negative Reader、atomic publication、file/memory、canonical
  bytes/hash preimage golden 已在 MSVC Debug 与 WSL GCC 通过；Clang 对照及完整三工具链
  矩阵仍未验证。
- [x] 对当前 Spec 已记录的 arm/deadline 包含性状态化与装配消费点重新整批取证；缺测容量的
  `gateIncomplete` 现在由 assembly/prepare 稳定拒绝，且 `prepareInto` 失败不发布新
  identity 或改变 active publication。
- [ ] 状态预算测量仍为 **INCOMPLETE GATE**，独立记录缺测维度与已证下界；
  无测量本身不是内容超限，也不是预算通过。只有确实超过 accepted threshold 才拒绝。

**2026-10-04 owner 后续裁决：**`maxStateCount` 继续使用 `uint64_t` 计数与配置表示；
研究候选 `4096` 不作为 S7A-3 的生产阈值，也不因它阻塞本批次实施收尾。
不在 S7A-3 冻结 `UINT32_MAX`、`UINT64_MAX` 或其它数值上限；生产阈值及测量能力的
匹配留给 S7A-9 实测后裁定。上述裁决不改变状态数缺测时的 `INCOMPLETE GATE` 事实，
也不放宽本节其他实施与验收门禁。

T4/K4 在 S7A-3 关闭 prepare 合法性与 immutable plan；运行期消费测试归 S7A-4。
Playback/Player/CXC 入口、manifest 与生命周期集成归 S7A-7。
本轮复用 Foundation 七项硬预算，不新增 Gameplay 数值限额；新上限仍由 S7A-9 测量后接受。
ABI 仍为 **9 域 / 126 = 96 + 30**，九类诊断与公共码表不新增；
需要新具名公共码时先逐条登记再消费。

## 5. 本轮验证证据

以下均为本轮实际执行结果，修改验证表后再次复跑文档与 whitespace 检查：

| 检查 | 结果 |
| --- | --- |
| `python -B tools/check_docs.py` | exit 0；349 Markdown / 20 candidate JSON/CXT |
| `python -B tools/check_docs_status_contract_tests.py` | exit 0；4 tests，OK |
| `python -B tools/check_docs_target_contract_tests.py` | exit 0；2 tests，OK |
| `git diff --check` | exit 0；只有 Git LF→CRLF 配置警告，没有 whitespace error；本命令不覆盖 untracked |
| 本轮显式编辑清单的 untracked whitespace/conflict-marker 检查 | 15 份 Markdown，issues=0；5 处合法双空格 Markdown hardbreak 保留 |
| ABI 登记对编辑前副本的只读核对 | 前后均 9 域 / 126 行 / 冻结目标 96 / 待冻结 30；条目名称集合无差异 |
| `git diff --cached --stat` | exit 0；空输出，没有暂存改动 |
| MSVC Debug `cuexis_gameplay_packed_tests.exe` | exit 0；13 cases / 631 assertions |
| MSVC Debug `cuexis_judgement_tests.exe [s7a-3]` | exit 0；66 cases / 2,029 assertions |
| WSL GCC `cuexis_gameplay_packed_tests` | exit 0；13 cases / 631 assertions |
| WSL GCC `cuexis_judgement_tests [s7a-3]` | exit 0；66 cases / 2,029 assertions |
| WSL Clang `cuexis_gameplay_packed_tests` | exit 0；13 cases / 631 assertions |
| WSL Clang `cuexis_judgement_tests [s7a-3]` | exit 0；66 cases / 2,029 assertions |

本轮未运行完整 CTest、clean-first 或 runtime golden。WSL Clang 已使用当前
implementation SHA 和现有 Linux 依赖完成独立 Debug 配置、聚焦构建及测试；
上述两个聚焦目标均 exit 0。这补齐了 Clang 的聚焦证据；完整三工具链矩阵、
全量 CTest 与 hosted 验证仍未运行。
本轮补充的本地 MSVC Debug、WSL GCC 与 WSL Clang focused
`cuexis_judgement_tests [s7a-3]` 以及 `cuexis_gameplay_packed_tests` 均通过；
前者各为 66 cases / 2,029 assertions，后者各为 13 cases / 631 assertions。
该结果包含 explicit/inherited/default grace
provenance identity 等价测试、containment gateIncomplete 原子拒绝、K4 负向矩阵及
Capsule Reader/Writer、负例和原子发布覆盖。此前的
`3519 assertions / 110 test cases` 仍只属于
[2026-10-03 推进记录](2026-10-03-s7a-3-implementation-progress.md)，不移用为本轮通过证据。
其余 K4 运行期消费、Chart v5/CXT v2 authoring 双路等价与 state-budget
仍保持上方未完成或后续阶段门禁。
不主张 state-budget 完整、不主张 E1 已通过、不主张 S7A-3 / Stage 7A 实现关闭。
