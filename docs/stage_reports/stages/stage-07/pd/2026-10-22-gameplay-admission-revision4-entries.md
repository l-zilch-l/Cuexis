# Capsule revision 4 入口接入与阶段 B 交回

日期：2026-10-22。状态：本轮实现及适用本地核验完成，停止交回 owner 核验；未接受阶段 C。

## 范围与起点

本批从干净 243021e1adf37e3ae3aee48e876ea0f7f7cbff21 开始，2870201 开工留档。
Owner 本轮授权 revision 4 Reader/Writer、Header/metadata 及相关入口、兼容/拒绝/生命周期/
完整物理预算验证；完成后停止交回，不进入阶段 C。四维 accepted ceiling、七物理预算、
F1、1.10/15%、旧 golden/证据及全部生命周期规则保持。合同归
[Capsule §13](../../../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md)，
[Chart Entry](../../../../formats/CHART_ENTRY_V1_FORMAT.md) 与
[CXC](../../../../formats/CXC_FORMAT.md)；前批核心证据见
[阶段 B](2026-10-22-gameplay-admission-stage-b.md)。

## 物理与语义路径

- Writer 必须显式选择 4；原默认 2 与 Graph v1 的 revision 3 不变。GPR0 删除两个物理投影，
  GRC0 删除整个 optional Claim key（含 presence），其余字段和次序原样保留。
- collect/wire 与 count/hash 仍使用同一套字段 visitor，通过 physical/semantic 模式决定上述省略。
  REF0/STR0 仅收真实 wire 用途；11/12/13:1 即使 unused 也在 Reader REF0 解码时拒绝，
  13 只允许 0/2–8。真实 referenceCount 与 Header/GPH0/实际 REF0 对账。
- 同一物理操作模型负责 Writer 规范化/字典/序列化或 Reader owning decode/link/校验。
  hashModel 只借用该模型的 chart/graph/ENT/owners/Ix/definitions/claims/profiles/timebase；
  不重新 makeModel、不复制模型、不重解析 wire、不建三类隐藏长 key 字典。
- revision 4 显式映射到 semantic selector 3，复用完整 Gameplay.2 prefix、E/Claim framing、
  hex、execution fields、嵌套 Ix 和重复 bytes。logicalDerivedProjectionCount 在既有一条 claim/
  唯一 owner/backlink 校验后以已受 u32 物理预算约束的 u64 计算，不占用物理 REF0 额度。
- Writer 经完整 section/file/count 物理 preflight 后才调用核心；Reader 经 Header/directory/CRC/
  全物理预算、typed closure/owner/形状和引用闭包校验后调用核心。核心强制 count→budget→SHA，
  Header 使用这一次 digest；Reader 比对 Header 后才 shared prepare，返回已验证 digest。
- PreparedCapsule 的新增固定 32-byte optional digest 属于未安装的离线 adapter 类型；
  GameplayContent 将其连同 prepared result 封装为不可变 owning 内容。metadata 复用该 digest，
  不重建旧 preimage/字典、不重算语义 SHA；仍单独对 exact artifact bytes 求 artifact SHA。
  packed metadata 生成同时核对 artifact Header revision/digest 与内容，capsule.3/4 严格对应 Header。
- Playback raw packed dispatcher 仅对显式 Header 4 选择新 Reader；其余仍由旧上下文拒绝/处理。
  metadata strict parser、CXC optional extension registry 与显式 Memory/File/filesystem factories
  接受 capsule.4。Foundation factory 不接受新 Gameplay，Graph.1 原义保持。

## 哈希适配空间与完整分配边界

| 新入口涉及的空间 | 归属与计费 |
| --- | --- |
| SemanticInput 视图、固定 digest、adapter 参数 | 无动态分配；既有 65536-byte 核心控制额度保守覆盖操作/hex/SHA/调用栈；结果 digest 固定值，无文本缓存 |
| relation resource 索引 | 已有 validated 索引可借用；否则 HashCoreWork 内 PMR u32 vector，经同一 HashScratch reserve，按 capacity 计费；查找使用已验证资源表的 lower_bound |
| 其余 cost cache/prefix sort/identity arena/Pattern pages/export | 沿用前批完整账本，扩容新旧并存、分配前检查，所有失败 release；本批未引入其他 hash workspace |
| Reader/Writer owning 物理模型、字典和 wire sections | 原物理 codec 操作必需的输入/输出，不是为 hash 创建的第二份适配模型；同一份被借用。七预算计 physical decoded/file bytes，不冒充全部 C++ heap 或整个过程 <=8MiB |
| metadata JSON/config/assets 排序/hex digest 文本 | metadata/资源闭包输出工作，不参与语义 hash；没有摘要重算或语义适配/子摘要缓存；不把其 heap 宣称为已计 hash scratch |

前批全局 new/delete 与透明 PMR 上游的独立正控制、完整动态请求对账继续运行。
新增适配用例在同一全局观测窗口中要求 global count/bytes/peak 与账本相等，证明 relation
适配只增加一块已计费 u32 allocation；原输入/定义表始终在窗口前构造。入口另注入每个核心
allocation 位置 OOM，确认 workspace 清理且不伪报 budget。不是只凭 PMR high-water 声称覆盖。

旧 semanticPreimage vector API 保持 revision 2/3 原语义/字节/拒绝，不支持绕过新会计导出 4。
revision 4 的必要小规模导出经私有 hashRequest→同一完整物理 preflight→同一核心，显式 capacity
计入 scratch，返回 HashOwnedBytes，不再拷到第二个未计费 std::vector。容量不足在 SHA 前拒绝，
大输入默认不导出。该 API/控制参数未安装、不扩展 SDK/PackedChartLimits/Playback 配置。

## 验证矩阵与兼容边界

| 输入/入口 | 处理 |
| --- | --- |
| Foundation 1 | 原独立路径，拒绝 Gameplay 4 |
| Capsule 2/3 | 原 wire/preimage/预算/拒绝次序；3 的 Graph.1 不升级 |
| Capsule 4 | 新物理省略，所有成功 Reader/Writer/private bounded export 通过 count/hash 核心；file/read/decodeInto 共用这些路径 |
| 未知或上下文 revision 不匹配 | Header 早拒，不能 fallback |
| metadata capsule.3/4 与 Header 错配 | 拒绝，即使 compiled semantic hash 相同；Graph 仍仅 graph.1 |

新测试包括空图/1/64 混合 observe/consume/claim 和不同 E 长度的完整旧 preimage oracle；
逐 byte、长度、digest 与 prepared 身份对比；真实 dictionary absence、未使用非法 REF0、混写、
未知 revision、长 namespace/key、四维 0/放宽、七项物理 exact/少 1/0/放宽、OOM、原子文件/
active 不替换。原 Revision3 hostile/atomic matrix 也保留原用例并新增 revision 4 同矩阵。
扩充时一个 re-encode 断言最初仍硬编码 3，导致 4→3 wire 比较失败；只将测试使用的 revision
变量传到底，原始 dev3-capsule-tests.log 保留，没有改产品字节或旧 golden 来通过。
完整 metadata/CXC file/memory/filesystem generation 既有矩阵同时覆盖 3/4，结果与生命周期对照。

原样 capacity generator 从 tests/pd/pd5_capacity_probe.hpp 拷贝构造段至单独离线测试：
1/64/40000 × low/high；不修改旧 generator，保留唯一 E/rank/实体/声明/关系和 low 的 timing.end
差异。测试仅输出新 Writer bytes、Reader 验证及全部七项物理量，停在 owning prepared candidate，
不运行旧 capacity 的 Host/Snapshot/Seek/Replay 部分；这不是阶段 C 或正式测量。

## 最终固定提交、审阅与验证

最终实现提交：72c7716ea8e249a93cfaef214b8fc76346ccdce2。e29a741 为首个完整候选；
f169d09/72c7716 提取并接入共享 Header revision helper 后，另建最终 Release 目录，
不复用首个候选的验收绑定。最终交回提交仅更新本文与 CURRENT_STATUS，见新证据 receipt.json。

最终配置 Release/STATIC、WARNINGS_AS_ERRORS=ON、CHART_V5_CANDIDATE=ON；metrics、trace、
lifetime observer 均 OFF；IMPLEMENTATION_SHA=72c7716。fresh configure + clean-first 相关五目标
构建退出 0。沿用已验证的 UTF-8 showIncludes override，capsule 对象 #deps=75，含全部关键私有头。
2041 个 tracked 文件的实际 worktree bytes 已归档，运行前固定 30 个 exe/lib/构建与编译器配置
payload；运行后源码与全部构建文件哈希相同。没有把开发增量构建或旧候选视为最终同 SHA 结果。

| 最终核验 | 结果 |
| --- | --- |
| Capsule | 44 cases / 5399 assertions，全通过；旧 golden/拒绝与新核心/入口矩阵 |
| Playback | 172 cases / 27905 assertions，全通过；metadata/CXC/file/memory/generation/lifecycle 含 3/4 |
| Chart | 227 cases / 42425 assertions，全通过 |
| Judgement | 216 cases / 35624 assertions，全通过 |
| CXC | 64 cases / 975 assertions，全通过 |
| 原样离线 wire matrix | 另 1 case / 344 assertions，六配置全通过 |
| Architecture | 1/1 通过 |
| 全仓 cuexis_format_check | 通过 |
| tools/check_docs.py / git diff --check | 通过；477 Markdown / 20 candidate files |

五组完整回归合计 723 cases / 112328 assertions。没有全仓 CTest、Linux sanitizer/coverage、
跨平台新格式 byte acceptance 或公共发行接受声明。本轮 40k 核验包含同一 Reader 的 shared
prepare/proof；没有运行 40k Host/Snapshot/Seek/Replay，也没有登记性能阈值或性能通过结论。

### 原样六配置的完整物理结果

| N / 数值模式 | file bytes | decoded bytes | 最大 section | STR0 rows | REF0 rows | semantic bytes | Writer / Reader scratch peak |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 / low | 2295 | 1751 | 1213 | 49 | 13 | 4296 | 66086 / 66086 |
| 1 / high | 2295 | 1751 | 1213 | 49 | 13 | 4296 | 66086 / 66086 |
| 64 / low | 9962 | 9418 | 4092 | 112 | 13 | 158979 | 77237 / 77237 |
| 64 / high | 9954 | 9410 | 4084 | 112 | 13 | 158979 | 77237 / 77237 |
| 40000 / low | 6818424 | 6817880 | 3725942 | 40048 | 13 | 99588531 | 7145909 / 7145909 |
| 40000 / high | 6746664 | 6746120 | 3654182 | 40048 | 13 | 99588531 | 7145909 / 7145909 |

两种 40k 均为 40000 entities / 40000 requirements，与另外五项物理值联合落在原七限额内。
每个配置 Writer 与 Reader 各一次语义 SHA；两种 40k preimage=99588531 <134217728，
scratch=7145909 <8388608。未导出大 preimage vector，未改变 accepted ceiling。
完整 preimage 的字节、Ix、单 key 与 scratch 四维由同一核心在 SHA 前接受；计数/摘要分配
在同一账本，返回 workspace live=0。完整受控分配回归 24 行均满足成功 global count/bytes
与账本一致、global heap peak+65536=ledger peak；新增 relation 适配只增 4 bytes，
实测 HashCoreWork=4656 bytes，仍由固定控制额度覆盖。所有新旧 workspace 路径和失败清理
沿用前批对账；原始结果在 allocation-observations.json 与单测日志，不以本机进程 RSS 代替。

finalize_validation.py 在进程退出 0 后独立解析六个完整实际文件：每个 Header/section CRC、
decoded sum、七预算、逐 section 大小与行数、Header digest 均通过；所有六个文件的精确 sizing
及完整 Gameplay.2 SHA 与冻结 Stage A audit-r3 记录相等。Stage A 仅记录过提议，未被改写成当时
已经准入；本轮的新 wire 成功与 Reader 验证单独登记。原样 fixture 构造段除格式空白与原代码
逐字符一致，fixture-construction-proof.json 可复核；没有换较轻内容。

40k low 完整 semantic digest：
78c23507481084ba7aa900efd56f72065992d8ad2409f657fcf194784252038d。
40k high：e41eccde1593ca70b4b9c17696827f36b8884523d66e6c0ecb111a92ffc1cfcf。
artifact SHA、六个新 wire 文件、逐 section 数值和历史引用哈希见 physical-results.json。
旧日志只复制到新 packet 的 frozen-stage-a-reference，原件哈希不变。

### Standards

独立只读范围 243021e..72c7716：硬性违规 0、可行动异味 0。首轮 P3 重复 Header 小端解析
已提取为共享 packedGameplayRevision，保留两入口各自的截断、选择与错误适配，并补齐 4/
95-byte 测试。复核不替代上述同 SHA 执行证据。

### Spec

独立只读范围 243021e..72c7716：发现 0。共享字段/完整 Gameplay.2、无旁路新入口、物理省略、
unused REF0、统一 scratch、旧版/Graph.1 与预算/生命周期边界均经复核。原有公共 capability
的 requiredFormat 仍保留 capsule.3 语义描述，未改同一 capability ID/revision 的冻结记录；
这不是新 Capsule4 registry 宣告。新4支持由显式 metadata/Header/Reader 合同登记，不升级 SDK。
依据为 GAMEPLAY_V2_ABI §capability description、Capsule §13 和 Chart Entry Gameplay metadata。
两轴最终未关闭发现均为 0；完整原始 review 记录见新 packet 的 review.md。

### 冻结、绑定与停止点

6386 个既有冻结证据文件逐项核验字节不变；旧 golden、七物理预算及四维 profile 未改。
主树保持干净 F1 eed36648a692333e47f798aa45be36ca850d08de；C2、旧失败证据及窗口3均未恢复。
新证据根 out/pd5-r4-entry-20261022-243021e，最终运行在 final-72c7716；receipt/index/zip
绑定源码、diff、工具/配置、二进制、原始测试、六个 wire 与历史对照。
verify_packet.py 仅只读校验完整性，不能代替 owner 的代码与阶段接受。

本轮提交后停止交回。阶段 C 的 40k Host 功能链路、正式 preparation/registration/测量、
主树合并及 SDK/公共 API 升级均未执行；F1、1.10/15% 与全部生命周期规则保持。
