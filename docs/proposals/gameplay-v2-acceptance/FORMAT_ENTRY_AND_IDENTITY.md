# 格式、入口与 identity 矩阵

状态：candidate；S7A-0 准入产出，待 owner acceptance

更新日期：2026-10-02

上级文档：[Gameplay V2 acceptance package](README.md) ·
[Stage 7A 实施计划](../../stage_plans/active/stage-07/plan.md)

文档角色：格式与入口表征。它记录版本层级、Playback entry、四类身份与迁移规则，并标出未决点。
它不是格式 Spec 正文，也不产生格式变更授权。

## 1. 版本层级与接受/拒绝矩阵

| 版本字段 | 归属 | 候选取值 | 7A 处置 | 未决 |
| --- | --- | --- | --- | --- |
| Chart 外层 `version` | `cuexis.chart` | `5` | 接受 `5`；其它值稳定拒绝 | 无 |
| Gameplay 语义版本 | `gameplay.version` | `2` | 只接受 `2` | 无 |
| 旧 Gameplay 语义版本 | `gameplay.version` | `1` | **不在最早入口接受**；必须显式离线迁移或稳定拒绝 | 迁移器属 S7A-8.2 |
| 同义替代字段 | `semanticRevision` | 禁止 | 不得引入 | 无 |
| Packed 载体版本 | `packedVersion` | `1`（candidate） | 保留 | 无 |
| Packed 候选期修订 | `candidateRevision` | `1` | 保留；**是否改变 `compiledSemanticIdentity` 未定** | CM-V07 |
| capability 修订 | `revision` | 单调递增 | 语义变化必须新增 ID 或 revision | 无 |
| 判定语义版本 | engine 分量的一部分 | 未冻结 | 与 SDK API 版本独立 | CM-I06 |
| Replay 格式版本 | Replay header | 未冻结 | 与判定语义版本独立 | CM-K01 |
| Snapshot 状态 schema 修订 | Snapshot header | 未冻结 | 必须写入 header | CM-K07 |
| SDK API 版本 | 安装包 | `0.7.0` | 本 package 不改变它 | S7A-8.4 |

规则：Reader 不得通过"某字段是否存在"猜测版本；未知必需字段/未知能力/新 requirement kind
一律稳定拒绝，不得静默解释成 tap（AR P0-07）。

## 2. 载体与 Playback entry 矩阵

| 载体 / entry | format | 语义角色 | 可否 Playback | 携带 identity | 7A 状态 |
| --- | --- | --- | --- | --- | --- |
| Chart v5 source | `cuexis.chart` v5 | 作者聚合清单：时间轴、`cxtImports`/`cxtInstances`、`gameplay` 区段、presentation 关联 | 可（经 typed prepare） | source identity；编译后产生 semantic identity | 接受（candidate path） |
| CXT v1 source | `cuexis.animation-template` v1 | 旧作者层模板 | 仅经 v4 回退路径 | source identity | 保留回退 |
| CXT v2 source | `cuexis.animation-template` v2 | 作者层模板与有限生成输入（Prototype/Pattern/Animation） | 不是 Playback AST | source identity | 接受为 source input |
| Packed Chart entry | `cuexis.packed-chart` | Canonical Graph 的物理编码 | 是（`playback=true`） | compiled semantic identity + artifact identity | 接受 |
| Canonical Gameplay Graph entry | `cuexis.gameplay-graph`（候选） | 已验证的 Canonical Graph | 是（显式 `playback=true` 时） | compiled semantic identity | 接受路线；**manifest closure / 预算 / 拒绝编码未定** |
| `author-source` entry | `cuexis.chart` | 审查、迁移、复现 | **否** | source identity | 必须显式非 Playback |
| Ruleset package | `cuexis.ruleset` | prepare 输入：Interface、模块、常量参数 | **否**（不是第三种 Playback entry） | ruleset identity（Interface 投影、模块顺序、Build hash） | 7A 是否包含待决策（CM-S08） |
| Presentation pack（候选） | 未定义 | 可选 L4 资源/表现能力集合 | 否 | presentation identity | 后续 |

硬要求：

1. 同一 Chart v5 source 与它的 Packed 物理编码必须恢复**同一** Canonical Gameplay Graph、
   prepared judgement identity 与判定结果；round-trip 需要独立 semantic 比较，不能只比 hash。
2. `packed-chart` 与 `gameplay-graph` 两个 entry 必须进入同一 typed prepare、Judgement、Ruleset、
   Snapshot、Replay 与 Presentation path。
3. Packed decoder 只解码已验证表，不执行 CXT、Pattern、脚本或 solver。
4. Pack、unpack、migrate、prepare 是四个不同操作，任何一个都不得隐式执行另一个。

## 3. Chart v4 / CXT v1 回退路径

| 项 | 规则 |
| --- | --- |
| Chart v4 / CXT v1 | 继续走既有回退路径；行为不得因本阶段新增能力而改变 |
| 无输入纯播放 | Judgement 保持休眠，不得改变 FrameSnapshot / FrameDigest / Presentation candidate |
| v4/v5 → V2 | 只能由显式离线迁移器生成 V2 graph 并保存 source map；Playback 不做运行期隐式迁移 |
| 无法判断 | 输出 `ambiguous` 并停止该 Requirement 的自动迁移（不得把 render position 提升为 judgementDomain） |

## 4. 四类身份与四分量投影

### 4.1 四类身份

| 身份 | 覆盖内容 | 绑定 Replay | 备注 |
| --- | --- | --- | --- |
| source / build | 作者 bytes、参数、compiler profile、source map | 否 | 诊断用途 |
| semantic | Canonical Gameplay Graph 与声明闭包 | 作为 `chart` 分量 | V2 的核心 |
| artifact | CXC / Packed 物理 bytes | 否 | 分发与缓存 |
| judgement | engine / ruleset / chart / session 投影与 capability | 是 | 兼容判断 |

### 4.2 四分量内容（候选）

| 分量 | 内容 |
| --- | --- |
| `engine` | 判定语义版本、实际引用的定点表 `tableId`、引擎阶段顺序、TimebaseProfile、late policy、normalization profile |
| `ruleset` | Interface 投影、折叠/模块顺序、Build hash、capability、Hook 合成 |
| `chart` | 编译后的 Requirement/Pattern/Measure、锚点量化、prepared grace、CoordinationGraph 的 judgement projection、solver profile |
| `session` | Loadout、默认 grace 来源、离散输入规范化 profile、JudgementConfig |

### 4.3 逐字段归属（草案；完整表是 CM-I06 的产出）

| 字段 | 归属 | 依据 |
| --- | --- | --- |
| Requirement/Relation 语义 | judgement | AR §7 |
| 判定域与判定几何的 typed record | judgement | FB §6.2 |
| `TimebaseProfile`、offset/calibration、late policy | judgement | AR 明确 |
| Fact 排序键与 `phasePriority` 注册表 | Fact semantic revision | PD §6.2 |
| FactBinding `aggregation` | 仅当改变判定事实时进 judgement，否则 presentation | CM-P05 |
| 表现资源、材质、动画、Audio | presentation / content | FB §8 |
| source 数组顺序、注释、source map | 仅诊断，不进 judgement | FB §7 |
| Packed 压缩顺序、字典顺序 | artifact | PD §9.2 |
| 未知自定义类型 | **默认归 judgement**（保守） | FB §8 |

## 5. Canonical Requirement identity

```text
(chartEntryId, invocationId, moduleId, exportId, emissionPath, requirementLocalId)
```

性质（写入 Spec 时必须逐条验证）：

| 变化 | Requirement identity | canonical / judgement identity |
| --- | --- | --- |
| 改 Beat / lane / 参数值 | 不变 | 改变 |
| 改 Repeat count | 只增删对应 index 的 emission | 改变 |
| 重排源数组 | 不变 | 不变 |
| 改 nodeId / invocationId / moduleId / localId | 改变 | 改变 |
| 删除 Presentation | 不变 | 不变 |

`emissionPath` 是由稳定 `nodeId` 与 Repeat index 构成的有序路径；source map 记录旧字段路径、
模块与 Studio 节点，但不进入 judgement identity。

## 6. Canonical Gameplay Graph 的物理归属（未决 CM-V06）

| 候选 | 含义 | 影响 |
| --- | --- | --- |
| 选项 1：Chart v5 JSON 区段 | graph 直接是 `gameplay` 区段的语义视图 | 需要 Packed 无损承载同一结构 |
| 选项 2：独立 CXC typed entry | graph 作为 `gameplay-graph` entry 的载荷 | 需要定义 graph 自身的 wire 格式与预算 |
| 选项 3：编译中间层 | graph 只在 prepare 期存在 | 与"`gameplay-graph` 可独立 Playback"冲突 |

无论选哪一项，都必须满足：`packed-chart` 与 `gameplay-graph` 恢复同一 typed graph、同一
prepared identity、同一 Judgement kernel（PD §0）。

## 7. Packed 承载

| 项 | 规则 |
| --- | --- |
| 现行 `REQ0` / `CNS0` | 保留 Foundation 语义，**不扩大解释**；REQ0 是 tap/point/lane 演示 profile，CNS0 `eventCount = 0` |
| V2 新增语义 | 另立 candidate wire revision 或新增 section 组，至少覆盖：图元数据、resource、relation/group、solver profile、binding、capability closure |
| 旧 reader 行为 | 遇未知必需 section 必须**稳定拒绝**，不得忽略 |
| 预算 | 复用 §3.3 逐 section 预算与 `referenceCount` 口径；不新开绕过 envelope 预检的入口 |
| 禁止 | Packed 不得把 Requirement 或 Relation 变成不可恢复的表现约定 |

## 8. 迁移矩阵

| 源 | 目标 | 工具 | 失败/歧义条件 |
| --- | --- | --- | --- |
| Chart v4 | Chart v5 `gameplay.version = 2` | 显式离线迁移器 | 只有 render position 无 judgementDomain；同一视觉对象代表多个要求；Behavior/Animation 无法证明不影响判定；旧字段同时承载视觉与判定；Object parent 与 gameplay relation 不一致 → `ambiguous` |
| Chart v5 `gameplay.version = 1` | `gameplay.version = 2` | 显式离线迁移器 | 无法判断资源共享/判定几何/对象 parent 是否为 gameplay relation → `ambiguous` |
| CXT v2（纯 Gameplay 子集） | Chart v5 `gameplay.requirements` + `gameplay.coordination` | 独立转换器 | 含 Animation/Component/Object parent/`effects` 时不得整体复制 → `unsupported` |
| CXT v2（表现部分） | Presentation Graph | 同上 | 无法分离时报 `unsupported` |
| Packed ↔ source | 同语义双向 | round-trip + 独立 semantic diff | 任何字段不等即失败；只比 hash 不算证据 |

迁移工具不得把 CXC pack 称为语义迁移，也不得用 `ChartWriter` 的 v4/v5 投影伪造新的 Gameplay Graph。

## 9. 本文件的未决项

1. CM-V06 Canonical Graph 物理归属；
2. CM-V07 Packed `candidateRevision` 与 `compiledSemanticIdentity` 的关系；
3. CM-V10 `gameplay-graph` entry 的 manifest closure、预算与拒绝编码；
4. CM-K07 Snapshot header 的 state schema revision 与 byte budget；
5. CM-I06 完整 identity projection 表；
6. CM-X06 capability 派生的五来源优先级（见 [SUPPORT_AND_REJECTION_MATRIX.md](SUPPORT_AND_REJECTION_MATRIX.md) §7）。
