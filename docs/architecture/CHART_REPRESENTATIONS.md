# Chart 表示与转换点

状态：active

更新日期：2026-11-18

<!-- cuexis-meta
id: ARCH-chart-representations
status: active
authority: none
-->

本文是 Chart 数据的**表示地图**，不定义 wire 字段；字段合同以
[CHART_FORMAT](../formats/CHART_FORMAT.md)、[CHART_V4_FORMAT](../formats/CHART_V4_FORMAT.md)、
[PACKED_CHART_FORMAT](../formats/PACKED_CHART_FORMAT.md) 与
[CXT_FORMAT](../formats/CXT_FORMAT.md) 为准，本页只回答“同一份谱面在代码里有几种表示、
在哪里转换、各自参与哪套 identity”。

## 1. 三条输入族，不是一条流水线

```text
族 A（v1–v3 JSON）:  JSON -> ChartDocument -> ChartRuntime
族 B（v4 JSON）:      JSON -> ChartV4SourceDocument -> ChartV4ResolvedArtifact(+ChartDocument)
                           -> ChartRuntime
族 C（Packed/CXT/candidate/gameplay）:
                      bytes/CXT -> CanonicalSemanticChart -> CandidateRuntimeArtifact
                           / PreparedCapsule
```

三条族共用 `ChartDocument` 与 `ChartRuntime` 两个节点，但入口不同；把五者画成一条链会掩盖
“v4 与 Packed 各自有独立中间层”这一事实。分支选择见
`engine/playback/src/playback_session_prepare.cpp:222`（candidate）与 `:243`（document）。

## 2. 内容型表示（7 个）

| 类型 | 层次/输入族 | 定义 | 生产者 → 消费者 | 转换入口 | identity | 已知有损点 |
| --- | --- | --- | --- | --- | --- | --- |
| `ChartDocument` | 作者文档；族 A 主表示，族 B 复用为投影 | `chart_document.hpp:211` | v1–v3 Loader / v4 投影 → Compiler、CXC、迁移 | `canonical_chart_loader.cpp:225`、`chart_v4_loader_projection.cpp:189`、`chart_v4_resolver_patching.cpp:247` | v3 canonical JSON | 被 v4 流程复用 3 次；`ChartWriter::write` 有损（见 §7） |
| `ChartV4SourceDocument` | v4 未解析源 | `chart_v4_document.hpp:194` | v4 Loader → Resolver | `chart_v4_loader.cpp:197` | v4 canonical JSON | 类型内部已是 typed + `canonicalSource` 原文双表示 |
| `ResolvedChartDocument` | v4 解析结果的具体 chart + 参数 | `chart_v4_resolver.hpp:42` | Resolver → 编译/投影 | `chart_v4_resolver.cpp:405` | 随所属 artifact | 同层冗余包装，只含 `ChartDocument` + 参数数组 |
| `ChartV4ResolvedArtifact` | v4 解析产物 | `chart_v4_resolver.hpp:65` | Resolver → Prepare/Playback | `chart_v4_resolver.cpp:342` | v4 canonical JSON | 捆绑图 + 动画程序 + 4 类 identity + 资源/能力闭包 |
| `ChartRuntime` | 编译结果 | `chart_runtime.hpp:76` | Compiler / candidate lowerer → Runtime、Playback | `chart_runtime.cpp:540` | 由所属来源决定 | 不含 JSON/未解析参数；`renderableOpacity` 在此与 candidate metadata 都有 |
| `CandidateRuntimeArtifact` | candidate 运行时 | `candidate_lowering.hpp:49` | candidate lowerer → Prepare | `candidate_lowering.cpp:326` | candidate preimage | `ChartRuntime` + `CandidateRuntimeMetadata` 的包装 |
| `CanonicalSemanticChart` | 族 C 语义图 | `canonical_semantic_chart.hpp:180` | Packed 解码 / CXT v2 展开 / candidate / gameplay → lowerer | `cxt_v2_loader.cpp:28`、`packed_chart_tables_decoder.cpp:76` | Packed typed preimage、prepared | 与 v4 族不相邻；字段见 §3 修正后的合同 |

## 3. 载体（6 个，不新增字段模型）

| 载体 | 定义 | 持有内容 |
| --- | --- | --- |
| `ChartDispatchResult` | `detail/chart_dispatch_internal.hpp:17` | 入口分派结果 |
| `ChartMigrationArtifact` | `chart_migrator.hpp:52` | 迁移中间结果 |
| `PrepareArtifact` | `playback_session_prepare_internal.hpp:50` | 同时持有 `ChartDocument`、v4 源、v4 解析产物、`AnimationProgramInput` 与 candidate metadata |
| `PreparedRuntimeSession` | `runtime_session.hpp:106`（`chartRuntime_` 在 `:129`） | 按值再存一份 `ChartRuntime` |
| `PreparedCapsule` | `gameplay_capsule.hpp:54` | gameplay capsule 准备结果 |
| `CandidateChart` | `cxc_candidate.hpp:45` | `CanonicalSemanticChart` + Packed bytes + 可选 `CandidateRuntimeArtifact` |

载体不是“第 8 种表示”；它们的风险是同一内容被复制多份（`PrepareArtifact` 最明显）。

## 4. 物理/文本编码（3 类）

| 编码 | 写入 | 读取 | 说明 |
| --- | --- | --- | --- |
| v3 canonical JSON | `writeCanonicalJson` | v3 Reader | 族 A 的 wire |
| v4 canonical JSON | `writeV4` | v4 Reader | 族 B 的 wire，v4 identity 的 preimage |
| Packed 二进制 | `packed::encode` / `writeAtomic` | `packed::decode` / `PackedChartReader::read` | 族 C 的 wire，CXPk5 rev1 |

## 5. 主要转换点

| 转换 | 位置 |
| --- | --- |
| v1–v3 JSON → `ChartDocument` | `chart_loader.cpp:33` → `canonical_chart_loader.cpp:225` / `:206` |
| v4 JSON → `ChartV4SourceDocument` | `chart_v4_loader.cpp:197` |
| v4 JSON → `ChartDocument`（投影 A，供 CXC/迁移） | `chart_v4_loader_projection.cpp:189` |
| `ChartV4SourceDocument` → `ChartV4ResolvedArtifact` | `chart_v4_resolver.cpp:405` / `:342` |
| v4 JSON → `ChartDocument`（投影 B，供编译） | `chart_v4_resolver_patching.cpp:247` |
| `ChartDocument` → `ChartRuntime` | `chart_runtime.cpp:540` / `:711` |
| `ChartDocument` → JSON（有损） | `chart_writer.cpp:703` / `:602` |
| v1/v2 → v3 → v4 | `chart_migrator.cpp:415` / `:556` |
| `CanonicalSemanticChart` → `ChartRuntime`（candidate） | `candidate_lowering.cpp:326` / `:107` |
| `CanonicalSemanticChart` ⇄ Packed bytes | `packed_chart_tables_writer.cpp:395`、`packed_chart_tables_decoder.cpp:76`、`packed_chart_io.cpp:318` |
| CXT v2 JSON → `CanonicalSemanticChart` | `cxt_v2_loader.cpp:28` |
| 各表示 → identity | 见 §6 |

## 6. identity 来源（五类，candidate 内部有分支）

| identity | preimage | 位置 |
| --- | --- | --- |
| v4 | canonical v4 JSON 的 sha256 | `chart_v4_resolver.cpp:348` / `:397` |
| legacy v1–v3 | canonical v3 JSON | `playback_session_prepare_stages.cpp:104` |
| Packed | typed preimage（`packed::semanticIdentity`） | `packed_semantic_identity.cpp:594` / `:605` |
| candidate（static） | **复用 Packed 的 typed preimage** | `candidate_lowering.cpp:140` |
| candidate（behavior-source） | canonical bytes 的 sha256 | `behavior_candidate.cpp:127` |
| prepared | 在 candidate preimage 外再封装的 prepared 域 | `candidate_lowering.cpp:343` / `:371`（`cuexis.prepared-semantic.v5.candidate.1`） |

注意：candidate 不是永远独立的一套；static candidate 与 Packed 共用 preimage，behavior-source
candidate 才有独立 preimage，prepared 再包一层。因此“同一内容多种 identity”不能按表示数量直接推断。

## 7. 已知漏转与重复（登记，不在本页修复）

- **v4→v3 投影逻辑写了两份**，必须同步改：`chart_v4_loader_projection.cpp:197-201` 与
  `chart_v4_resolver_patching.cpp:256-260`（erase 列表）；animator 过滤同样有两份。
- **`ChartWriter::write` 有损**（RR-F401）：`templates`/`requiredExtensions` 恒空、`stops`/
  `tempoEvents` 恒空、`version` 恒 3、behavior 不写 tracks（`chart_writer.cpp:676-685`、`:611-628`）。
- **`ChartDocument` 在 v4 流程出现 3 次**（源内 `legacyProjection`、解析产物、编译入参）；
  `PrepareArtifact` 在 `playback_session_prepare_stages.cpp:149` 深拷贝后又保留整个 artifact。
- candidate 路径没有 `ChartDocument`，能力预检使用默认构造的空文档
  （`playback_session_prepare.cpp:235`、`playback_session_prepare_stages.cpp:461`）。

## 8. 新增字段/表示时的规则

1. 新字段必须在本表声明：进入哪几种表示、在哪一步转换、是否进入 identity；
2. 新表示必须声明是**替代**还是**并存**，并说明替代谁；
3. 不得把“用某种表示算出的 hash”直接当作另一种表示的身份；identity 必须与其 preimage 对齐。
