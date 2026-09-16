# Chart Format Foundation Hardening：交接加固实施计划

状态：active；当前实施阶段；R0-R5 尚未执行完成，Stage 6 保持 future

更新日期：2026-09-16

归档来源：[Foundation 原计划](../../completed/chart-format-foundation/plan.md)、
[Foundation 关闭与交接记录](../../../stage_reports/stages/chart-format-foundation/2026-09-16-closure-and-handoff.md)、
[交接复核记录](../../../stage_reports/stages/chart-format-foundation/2026-09-16-handoff-review.md)、
[Stage 6 计划](../../future/stage-06/plan.md)。

## 1. 阶段目标

在既有 Foundation candidate 原型之上补齐语义身份、profile 拒绝、预算、端到端验证和
可追溯证据，使 Stage 6 能够消费明确且验证充分的 subset。此阶段不是 Chart v5 正式发行，
也不重新设计 Chart v5。字段和算法分别由
[Packed Chart Spec](../../../formats/PACKED_CHART_FORMAT.md)、
[CXT v2 Spec](../../../formats/CXT_V2_FORMAT.md) 和
[CXC Spec](../../../formats/CXC_FORMAT.md) 拥有，本计划不复制完整 wire 合同。

Foundation 的原 owner 完成确认与 completed 归档保留；新发现的交接问题在本阶段修复。
归档、PR 合并、文档检查通过不等于技术退出门禁通过。当前状态以
[CURRENT_STATUS.md](../../../CURRENT_STATUS.md) 为准。

## 2. 范围、前置与执行规则

### 2.1 包含范围

- Packed candidate semantic identity 的计算、写入、重算与 CXC 比对。
- Writer/Reader/CXC 对 Foundation 已登记 profile 的一致拒绝行为。
- 已声明预算的执行、checked arithmetic 和预算文档统一。
- CXT/semantic/Packed/CXC 等价、40k 容量、失败回滚与旧格式兼容验证。
- 同一最终候选 SHA 的 hosted 证据、交接报告和 owner acceptance。

主要代码落点是 `engine/chart/`、`tools/cxc_common/` 及相应测试；CXC core 的修改必须由
真实集成缺口驱动，不为了复用把工具层依赖反向引入 engine。

### 2.2 禁止范围

- 不切换默认 Writer，不删除 v1-v4 Reader，不增加正式 v5 Playback capability。
- 不实现 Hold/Release、Slide/Flick、多指、未登记 Behavior/Animation/Effect sections。
- 不让 Packed Reader 执行 CXT，不在 `engine/animation/` 增加 source parser。
- 不冻结 source/build 或 prepared/replay identity，不修改 v4 identity/FrameDigest 算法。
- 不实施 Stage 6 的 Player、媒体、渲染重构或 candidate Playback 接线。
- 不默认提升 SDK API `0.7.0`，不增加公共 SDK 类型或第三方传递依赖。
- 运行时脚本和逐帧回调继续无限期延后，不增加字段、capability 或执行 hook。

`chart-format-update-for-v5` 仍是 active 的跨阶段总工作包，不移动、不标记完成。
Stage 6 在本阶段结束并经 owner 明确接受前保留 future；可以阅读和研究合同，但不据此
恢复 S6 实施。不得把本阶段修复测试写成 Stage 6 功能已经交付。

### 2.3 证据规则

- 每轮先检查工作区、HEAD 和未提交修改，保留用户工作，不重置历史。
- 先复现已报告问题，再修改实现；静态 finding 与可执行验证结果分别记录。
- 每个问题至少有正例、失败例、确定性或兼容证据和明确的交接结论。
- 禁止用任意 64 位十六进制占位值充当已校验 semantic identity。
- 不能由 Writer 不生成非法数据推断 Reader 会拒绝外部非法 bytes。
- 检查例程、测试命令和报告必须对应实际调用路径，而不是未使用的辅助实现。
- 未执行、环境阻塞、失败和通过必须分别记录；不以旧运行替代修复后验证。

## 3. 工作顺序与看板

```text
R0 基线、复现和合同决策
  -> R1 semantic identity 闭环
  -> R2 candidate profile 拒绝
  -> R3 预算与 checked arithmetic
  -> R4 等价、集成、容量与回滚
  -> R5 回归、hosted 和 owner 交接
  -> Stage 6 恢复 active
```

| 批次 | 当前状态 | 必要输出 | 退出条件 |
| --- | --- | --- | --- |
| R0 | pending | 基线报告、问题复现、验收矩阵、决策清单 | 实际缺口与证据来源明确 |
| R1 | pending | 语义 hash、Header/CXC 比对、golden | 身份伪造和不匹配均被拒绝 |
| R2 | pending | profile 验证与独立负例 | 所有入口仅接受登记 subset |
| R3 | pending | 预算表、检查实现、边界测试 | 硬限制生效，观测项不冒充硬门禁 |
| R4 | pending | 端到端测试、容量原始数据、回滚证据 | 完整语义、身份与失败事务一致 |
| R5 | pending | 回归矩阵、同 SHA hosted 记录、关闭报告 | owner 接受后才授权 Stage 6 |

批次状态在取得证据时逐项更新，不在阶段末一次性补勾。后续对话默认一次执行一个批次；
若发现前置批次未关闭，先补齐前置，不跳到 Stage 6。

## 4. 详细实施步骤

### R0：基线、复现与验收合同

1. 记录起始 SHA、分支、工作区差异、UTC 构建身份、SDK API、工具链、平台和 preset。
   记录现有 Foundation 报告、PR #24 与可查询的 CI 运行 SHA；合并 SHA 与测试 SHA 分开。
2. 阅读实际 encode/decode/inspect、CXC validator、容量测试和调用入口；定位当前真正使用的
   路径。已有静态问题仅是复核输入，不自动假设已取得运行时失败。
3. 为零 semantic hash、manifest identity 不匹配、range 被接受、section limit 未生效
   增加最小复现。若复核推翻某 finding，记录理由和测试，不为保留原判断而改实现。
4. 建立矩阵：合同条款、支持入口、实现位置、正例、失败例、诊断、兼容要求、执行 SHA、
   结果及证据路径。测试未接入构建或仅有 JSON 示例时不能填为已验证。
5. 对照 Spec 核对完整 section 清单，包含 META/TIME，以及适用时的 CAM0；澄清计划摘要
   与 Spec 的遗漏，不通过删去实际支持内容来让文档表面一致。
6. 冻结决策：revision 1 旧零 hash artifact 的处置、inspect/decode 职责、硬预算与观测项。
   改变已接受格式语义或公共 API 的决策先形成 ADR/Spec 更新并取得 owner 接受。

输出：带日期 R0 报告和可执行回归测试；在其中记录尚未通过的用例及原因。
默认不移动阶段目录、不调整默认格式、不把本轮报告标记为最终关闭。

### R1：Semantic Identity 闭环

1. 按 Packed Spec §10.1 实现单一 typed semantic 预映像与 SHA-256 入口，复用现有 core hash
   工具；不基于 Packed 字典索引、内存布局或整个文件 bytes 计算语义 hash。
2. 核对 hash 前置合法性与规范化：实体/局部 requirement identity 唯一性、父图、引用、
   资源闭包、Beat、浮点有限值、负零与各排序规则；重复项不得排序后静默去重。
3. Writer 在任何有效产物发布前验证语义、计算 hash，并写入 Header。
4. Reader 完成结构、预算及语义校验后重算 hash，与 Header 比对；不匹配时返回稳定错误，
   不发布部分 CanonicalSemanticChart。hash 计算不能绕过或推迟必要的输入预算检查。
5. CXC validator 使用实际解码验证结果比对 compiledSemanticIdentity；artifactIdentity
   独立比对 exact entry bytes。遵守已冻结的 compiled/playback entry 验证范围，不依赖
   source JSON/CXT 回退或展开。
6. 明确旧零 hash 产物需显式再生成，或按接受的 revision 决策处理；不得增加“零值即跳过”
   后门。若修订 revision，同步 profile、fixtures、Spec 和拒绝测试。

必须验证：

- Spec 的独立 golden 与实际预映像匹配，不能仅比较同一实现算出的两个值。
- 允许重排的输入次序不改变 semantic identity；实体值/Beat/引用等语义变化会改变它。
- 修改语义 bytes 并重算 section/header CRC 后，旧 semantic hash 仍被拒绝。
- 修改 Header hash 并重算 Header CRC 被拒绝；manifest hash 格式正确但内容不符被拒绝。
- artifact 与 semantic identity 的职责分离；v4 identity 与 FrameDigest 回归不变。

### R2：Foundation Profile 严格拒绝

1. 先增加 typed Writer 与外部 Packed bytes 两组独立负例。
2. 统一检查登记的 Tap/point、feature ID/version、domain/action、lane、constraint 数量和
   空 effects；拒绝 range、未知种类、缺失/不匹配 feature 和未启用语义。
3. 修改恶意 bytes 后同步修复相关 CRC；针对 semantic identity 检查，明确预期失败顺序，
   必要时用专门 fixture helper 生成结构及 hash 一致的非法语义，证明命中 profile 拒绝。
4. 核对 inspect/decode/文件 Reader/CXC 的调用关系。inspect 若仅承诺结构检查，在 API 注释
   与调用点明确其不足，任何语义消费方必须完成 decode/validation。
5. 校验未知 revision、flags、section、RequirementKind 不会被静默忽略或转成已知类型。
   不增加 opaque JSON、CXT AST 或“失败后尝试 v4”的回退。

退出：每个支持入口均有登记 subset 正例与至少一种独立非法输入；diagnostic 稳定；
合法 candidate 不退化，非法输入不返回部分结果。修复不引入新的 gameplay 能力。

### R3：预算和算术安全

1. 建立预算表：字段、单位、默认值、硬限制/观测、检查入口、分配前检查位置、测试和诊断。
   不把 section decoded bytes 解释为对象堆内存或进程峰值。
2. 保持 40,000 实体与 16 MiB Packed 文件限制；逐项复核 requirement、字符串、引用、
   section、decoded、CXT expansion 的现有限制，不因某 fixture 失败而放宽。
3. 实现 maxPackedSectionBytes，使用明显低于文件上限的自定义值验证其独立生效。
   对所有公开候选入口测试自定义限制，不能只覆盖默认 limits。
4. 核对 count 乘法、offset/byte length 累加、整数窄化和 Beat 运算的 checked arithmetic；
   可提前预测的超限必须在大 reserve/resize 或输出构建前拒绝。
5. 处理原计划“暂不冻结”与“均有独立预算”的冲突：在本批次决策报告和权威 Spec 中
   指明当前硬门禁；峰值/耗时等未接受阈值的项目只作观测。原 completed 计划保留历史
   合同，添加指向新决策的澄清，不伪造原阶段已实现的预算。

退出：每个硬限制均有边界成功、越界拒绝、自定义值测试；记录零值策略与错误优先级。
对未冻结的 runtime/prepare 内存不作保证，不借本阶段实现尚不存在的 Playback prepare。

### R4：端到端、容量和回滚

1. 保留现有 low-reuse profile 名称与 40k 独立 identity 定义。测试 encode -> decode ->
   完整语义比较 -> re-encode/identity；只核对计数或仅调用 size() 不满足退出要求。
2. 对 CXT 阶梯和参数冻结输入执行 expand -> semantic -> Packed -> semantic，验证实体、
   父关系、Component、requirement、Beat、typed reference、资源闭包和 identity。
3. 以小规模可解释 fixture 覆盖 Transform/Renderable/Camera、资源引用和重复排序边界；
   它们补充语义覆盖，不偷偷替换容量 profile 或宣称任意谱面满足 16 MiB。
4. 构造真实 CXC candidate package，覆盖成功验证、缺 entry、profile/count/hash 不匹配、
   Packed 损坏、预算失败和 source 可选但不能充当 playback entry。测试必须调用真实工具
   或相同实现入口，不仅解析示例 JSON。
5. 原子写入先创建旧有效文件，再制造编码失败、预算失败和可控替换失败，验证旧 bytes
   不变、无有效部分产物、临时文件清理且不删除非本次拥有的文件。
6. 输出机器可读容量数据及人类摘要：fixture/revision/SHA/toolchain、输入类别与大小、
   展开计数、Packed/各 section/IDN0 bytes、字符串/引用数量、闭包信息、耗时和峰值口径。
   直接构造 typed model 时 source bytes 写不适用，不能虚构 JSON 大小。
7. 测量峰值时注明工具、基线进程占用与操作区间；记录不可测项。高复用/混合仍为可选，
   不把未运行观测计为通过，也不将其重新升级为本阶段硬门禁。

退出：等价、容量、CXC 和回滚矩阵通过；测试隔离并行临时文件，避免固定文件名互相覆盖。
本阶段回滚指工具/解码/文件事务，不宣称已验证 Stage 6 尚未实现的 v5 Session reload。

### R5：回归、Hosted 和交接

1. 完成 focused 后运行 Debug/Release、headless、架构、static/shared package 与安装头
   external consumer；确认私有候选实现没有泄露到公共 SDK 或增加 adapter 传递依赖。
2. 运行旧 Chart/CXT/CXC、默认路由、合法 v4 identity/FrameDigest 回归。新增 hash 不应改变
   既有格式的合法 canonical bytes；任何差异必须明确解释并解决。
3. 固定最终候选 SHA，取得 Linux Quality、Windows MSVC、Windows MinGW 必要 CI 运行。
   报告记录 workflow/run、SHA、工具链、关键命令、测试与容量产物，不只记录绿色状态。
4. 如实现、fixtures、构建或 gate 在验证后变化，在最终 SHA 重新验证；报告提交导致 SHA
   更新时，按仓库门禁要求记录 report-SHA revalidation，不将旧 SHA 称为同 SHA 证据。
5. 形成 completion report，逐项关闭 R0-R5，列出允许/禁止消费、identity/revision 政策、
   预算、已知残余与 Stage 6 接手命令。记录 owner 明确接受。
6. 只有以上完成后才将本计划归档、Stage 6 从 future 恢复 active，并同步状态、索引、
   路线图、旧路径映射、AGENTS 和文档检查器。无证据时保持 active，不抢先关闭。

推送、触发远端工作流、合并或发布须遵守当时用户授权。本计划存在不构成这些操作的授权。

## 5. 验证命令与平台边界

每轮先检查可用 preset、工具链和测试注册；以实际目标为准，不把历史报告命令当作已执行。
Windows 标准验证：

```powershell
cmake --preset debug --fresh
cmake --build --preset debug
ctest --preset debug --no-tests=error
cmake --preset release --fresh
cmake --build --preset release --clean-first
ctest --preset release --no-tests=error
cmake --build --preset debug --target cuexis_format_check
python -B tools/check_docs.py
python -B -m unittest discover -s tools -p "check_docs*tests.py"
git diff --check
```

MinGW headless 可按已存在的 `mingw-headless-debug` preset 构建；focused 可运行 chart 测试
的 `[packed]`、`[f2]` tags。新增 CXC 集成测试必须注册并明确具体执行方式，不能假设它
属于已有 `[packed]` tag。Focused 不能替代最终全量与 consumer gates。

`VCPKG_ROOT` 与 MSVC Developer shell 是对应配置前提；不要混用 MinGW 编译器和 MSVC
依赖 ABI。Linux sanitizer/coverage/tidy 走对应 Linux 配置或 hosted，不在 MSVC 上运行
不支持的 preset 后把环境错误当作代码 finding。构建或 SDK 版本变化遵守 fresh/clean-first。

本次计划建立与目录迁移只需文档检查及检查器回归测试，不要求 C++ 构建。后续实现则按
上述影响范围验证；无渲染改动时不虚构 GPU smoke 需求或结果，若影响适配器另补对应证据。

## 6. 验收标准

| 门禁 | 必须具备的证据 | 失败时 |
| --- | --- | --- |
| 身份 | 独立 golden、Header 重算、manifest 比对、伪造负例 | 不交接 candidate |
| 子集 | point-only 与 feature/profile 矩阵、未知内容拒绝 | 修复，不静默降级 |
| 预算 | 所有硬限制可执行，自定义限制有效，checked arithmetic | 不放宽门禁 |
| 等价 | CXT/semantic/Packed 与真实 CXC 集成 | 不以计数替代语义 |
| 容量 | 40k 原 profile 完整往返且 Packed <= 16 MiB，原始数据 | 不降低数量或替换样本 |
| 事务 | 解码无部分发布，写失败保留旧文件和清理证据 | 不接入消费路径 |
| 兼容 | v4/default/digest、architecture、static/shared consumer | 阻止交接 |
| 平台 | 最终 SHA 的 Linux/MSVC/MinGW 验证与产物 | 标记未验证，不关闭 |
| 管理 | 新报告、索引状态同步、owner acceptance | Stage 6 保持 future |

所有硬门禁通过才可关闭；观测项按 R3 接受的口径记录，不额外承诺尚未实现的 prepare。

## 7. 后续对话与交付格式

每轮交付至少包含：

```text
Batch / starting SHA / changed files
Contract decisions and explicit non-goals
Reproduction before fix / implementation
Positive / negative / deterministic / compatibility / rollback evidence
Commands, platform, actual results and artifact locations
Unresolved items / next batch prerequisites
```

报告放入现有 Foundation reports 目录并加入其 README，使用带日期且区分批次的文件名；
不覆盖历史 F0-F9 报告。每轮从本看板和最新报告继续，避免重新假设所有批次已完成。

建议首轮指令：

> 执行 Chart Format Foundation Hardening R0。核对当前代码、合同、工作区和已有 CI
> 证据，建立验收矩阵，以最小测试核实 identity、profile、预算及交接缺口。保留用户
> 修改和历史报告，不扩大到 Stage 6 实现，不虚构测试通过；未决合同先记录并请求决定。
