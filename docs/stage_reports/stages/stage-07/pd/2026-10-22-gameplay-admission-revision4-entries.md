# Capsule revision 4 入口接入与阶段 B 交回

日期：2026-10-22。状态：实施候选，待固定提交验证与 owner 核验。

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

待最终源码固定后填写。证据仅新增于 out/pd5-r4-entry-20261022-243021e；start/frozen-before
登记起点与 6386 个冻结文件，失败开发日志保留。最终 scratch/身份/兼容/物理预算若失败立即
交回，不上调预算、不修改旧 golden、不换轻输入。
