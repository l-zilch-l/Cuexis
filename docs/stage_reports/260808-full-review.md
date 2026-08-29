# 260808 全项目代码审查（Full Review）

状态：historical review snapshot；findings open，未整改；2026-08-29 完成全量事实核验（144/144 实质成立，勘误已并入本文，见第 9 节核验记录）

审查日期：2026-08-28 至 2026-08-29

审查 HEAD：`d380fc91e59ed893a3b17128d5d6481d924711e7`（工作区干净，与 `master` 同 SHA）

阅读说明：本文冻结 2026-08-28/29 的只读全仓库代码审查现场。它不改写
[CURRENT_STATUS.md](../CURRENT_STATUS.md) 或任何完成报告中的阶段关闭结论；当前产品状态仍以
CURRENT_STATUS 为准。所有 finding 均带唯一编号（引索号）与 `file:line` 定位，行号以审查 HEAD
为快照基准。代码路径均相对仓库根目录。2026-08-29 已对全部 144 项 finding 逐条只读复核（对照同一
HEAD，6 个核验子代理分 3 批、每批不超过 2 个）：无一项不成立；复核发现的行号、文件归属、数量级
与定性偏差已直接修正入文，明细见第 9 节核验记录。

## 1. 范围与方法

审查覆盖 `engine/` 全部 23 个模块（约 46,000 行）、`app/player`、`tools/`、构建系统
（根 `CMakeLists.txt`、`cmake/`、`CMakePresets.json`）、`tests/` 体系结构，以及实现与
文档/ADR/spec 的对齐。`app/studio` 与 `engine/particles` 按磁盘现状核查。

方法：3 批共 6 个并行受限的审查子代理（每批不超过 2 个），每个子代理逐文件阅读其分配
模块的 `include/` 与 `src/`，对照本仓库成文标准（`AGENTS.md`、
[CODE_POLICY.md](../guides/CODE_POLICY.md)）与对应 spec/ADR，产出带证据的 finding；主审查
随后对最高严重级发现做独立抽查复核。

已抽查复核并证实的关键行：

| Finding | 抽查内容 | 结果 |
| --- | --- | --- |
| CH-01 | `chart_v4_reader_internal.cpp` properties 循环无诊断分支 | 证实 |
| CM-01 | `cuexis_animation` 为 6 源文件 STATIC 库且在根 CMakeLists 接线 | 证实 |
| AP-01 | `BUILDING.md` 的 `find_package(Cuexis 0.6 ...)` 示例 | 证实 |
| CM-03 | `audio_transport.cpp` 的 `HostClock::submit/snapshot` 无同步原语 | 证实 |
| CX-02 | `asset_importer/main.cpp:325` standalone identity 硬编码 `"main"` | 证实 |

## 2. 严重级定义

| 级别 | 定义 |
| --- | --- |
| P0 | 正确性 bug 或契约违约，会实际产生错误行为 |
| P1 | 明确的对齐/正确性风险，或用户可见的功能性断层 |
| P2 | 效率或维护性明显问题 |
| P3 | 小瑕疵、一致性或文档细节问题 |

CTest 已强制的架构 allowlist、JSON/GL/GLM 隔离等不重复列为 finding；第 5 节记录其核查
通过的证据。

## 3. 摘要统计

共 **144 项 finding**：1 项 P0、15 项 P1、56 项 P2、72 项 P3。整体结论：架构硬约束全绿，
spec 驱动实现质量高；问题集中在正确性边角、热路径效率债、文档漂移与重复代码。

| 模块 | P0 | P1 | P2 | P3 | 小计 |
| --- | --- | --- | --- | --- | --- |
| engine/chart（CH） | 1 | 2 | 5 | 8 | 16 |
| engine/playback（PB） | 0 | 2 | 11 | 3 | 16 |
| engine/cxc · project · assets · shader（CX） | 0 | 2 | 10 | 15 | 27 |
| engine/runtime · world · render · render_opengl（RT） | 0 | 4 | 13 | 17 | 34 |
| engine/core 与媒体模块（CM） | 0 | 3 | 7 | 17 | 27 |
| app · tools · 构建 · 测试体系（AP） | 0 | 2 | 10 | 12 | 24 |
| 合计 | 1 | 15 | 56 | 72 | 144 |

## 4. 问题清单与索引

编号规则：`<模块前缀>-<序号>`。CH=chart，PB=playback，CX=cxc/project/assets/shader，
RT=runtime/world/render/render_opengl，CM=core 及媒体模块，AP=app/tools/构建/测试体系。

### 4.1 engine/chart（CH-01 至 CH-16）

#### CH-01 [P0] mask `properties` 非法元素静默丢弃整个 Layer/Instance

- 位置：`engine/chart/src/chart_v4_reader_internal.cpp:325-328`（消费点 `:983-985` Layer、
  `:1105-1108` Instance；capability 推导 `engine/chart/src/chart_v4_resolver.cpp:1312-1325`）
- 问题：property 循环对空串/超长串只 `valid = false; continue;`，不调用 `addV4Error`；
  mask 返回 nullopt 后 Layer/Instance 消费点静默 `continue`，整段动画无声消失。若这是唯一
  Layer，连 `cuexis.animation.*` capability 都不要求，Animator 直接 inert。违反
  CHART_V4 §10 稳定诊断契约与 §6.3 错误报告语义。
- 证据：同函数 prefix 循环（`:349-356`）同条件每个分支都报错；含
  `{"propertyMask":{"properties":[""]}}` 的 Chart 可通过 load/resolve/prepare 全流程。
- 建议：`:325-328` 补 `addV4Error("chart.animation.mask_conflict", ...)`；审计
  `readPropertyMask`/`readAnimatorComponent` 全部静默 `continue` 分支；补空串/超长 mask
  回归测试断言 diagnostics 非空（约 10 行改动 + 2 个测试）。

#### CH-02 [P1] v1/v2/v3 的 PreparedSemanticIdentity 不覆盖 timing 事件与模板定义

- 位置：`engine/chart/src/chart_writer.cpp:666-675`（`chartDocumentValue` 写出
  `timing.stops`/`tempoEvents` 恒为空数组 `:671-672`、`templates` 恒为空数组 `:666`；
  v1/v2 Behavior keyframe `tracks` 丢弃 `:600-617`）+ `engine/playback/src/playback_session.cpp:1416-1420`
- 问题：Playback 对 v1/v2/v3 用 `ChartWriter::write(*document)` 的 SHA-256 作 chartIdentity；
  两个仅 tempoEvents 或 stops 不同的 v3 Chart 得到相同 `PreparedSemanticIdentity`，而 tempo
  事件直接改变 `RuntimeKey.chartTimeMs`（`engine/chart/src/chart_runtime.cpp:325`）即运行
  表现。CHART_V4 §3 未指明用投影字节还是 canonical 全保真字节。
- 证据：migrator 对 source identity 用 `writeCanonicalJson`
  （`engine/chart/src/chart_migrator.cpp:179`），同一概念两种定义。
- 建议：v1/v2/v3 identity 改用 `ChartWriter::writeCanonicalJson`，或在 CHART_V4 §3 明文
  声明投影语义并说明 timing 不参与 identity 的理由；若改源，需一次性切换并重生成 identity
  golden，不能双轨。

#### CH-03 [P2] 两参/四参 TimingMap::create 的 BPM 域规则不一致

- 位置：`engine/chart/src/timing_map.cpp:84-97`（两参版 `:88-91` 只要求 finite 且 >0；
  四参版 `:104-108` 强制 [1.0, 65536.0]）；公共头
  `engine/chart/include/cuexis/chart/timing_map.hpp:40` 未注明差异
- 问题：TIMING_MODEL 的 BPM 节写 `[1.0, 65536.0]` 无版本区分；公共 API 直连可构造
  defaultBpm=0.5 的 TimingMap。Reader 路径自洽（CHART_FORMAT.md:175 允许任意正 BPM）。
- 建议：两参版执行同一域，或在头文件注明版本差异规则。

#### CH-04 [P1] Segment 端点约束"代码有、文档无"

- 位置：`engine/chart/src/chart_v4_reader_internal.cpp:744-749`
- 问题：`segment.endBeat > clip.durationBeats` 报 `chart.animation.clip_invalid`；
  CHART_V4 §5.2 与 §7 边界表均未声明该约束，合法输入被拒却无 spec 依据。
- 建议：CHART_V4 §5.2 补 "segment.endBeat ≤ clip.durationBeats"。

#### CH-05 [P2] v4 prepare 链路对同一 Chart 内容最多 6 次全量 JSON 解析

- 位置：`engine/chart/src/chart_v4_loader.cpp:682-700`（isV4 #1）、`:708-710`（load #2）、
  `makeLegacyProjection` 后 CanonicalChartLoader #3（`chart_v4_loader.cpp:494` 调用，实际
  parse 在 `canonical_chart_loader.cpp:1863-1865`）、
  `engine/chart/src/chart_v4_resolver.cpp:1347-1349`（#4）、`makeConcreteChart` `:257-262`
  （#5）、`engine/chart/src/chart_writer.cpp:702-704`（#6）；每个 import 另有一次模板 parse
- 问题：#1/#2 解析原始输入文本，#3-#6 解析的是重序列化的投影/规范文本（同内容再解析）。
  16 MiB 输入下 prepare 的解析成本放大约 6 倍；F4 热帧零分配不受影响，但 prepare
  延迟与峰值内存明显浪费。
- 建议：parse-once（见第 8 节方向 2）：`ChartV4Loader::load` 返回值携带已解析
  `json::Value`，resolver/Writer 接受预解析文档；需复跑 F1-F4 确定性指纹确认逐字节一致。

#### CH-06 [P2] ChartLoader::load 双重解析

- 位置：`engine/chart/src/chart_loader.cpp:38-55`
- 问题：先 parse 嗅探 `format` 字段，再把原始文本整体交给 `CanonicalChartLoader::load`
  再 parse 一次；v1/v2/v3 prepare 至少 2 次解析。
- 建议：嗅探与加载共享一次解析，或提供 `peekChartVersion(text, limits)`。

#### CH-07 [P2] validateProgram 在 O(L^2) 循环内重复构建 mask 集合

- 位置：`engine/chart/src/chart_v4_resolver.cpp:1130-1145`
- 问题：右层 `expandMask`（每次新建 set、9 次 string 插入）在每对 (left,right) 比较中
  重复执行；L 最大 64 显式 + 256 generated/对象，最坏约 10 万次 set 构建/对象。
- 建议：prepare 期每层展开一次缓存复用。

#### CH-08 [P2] Animator 组件被完整解析校验两遍且诊断重复

- 位置：`engine/chart/src/chart_v4_loader.cpp:565-621`（loader readAnimators）与
  `engine/chart/src/chart_v4_resolver.cpp:294-300`、`:1378`（resolver concreteAnimators），
  各调一次约 290 行的 `readAnimatorComponent`（`chart_v4_reader_internal.cpp:860-1149`）
- 问题：Animator 组件在 loader 与 resolver 两阶段各被完整解析校验一遍，prepare 期做双倍
  重量级工作；loader 报错时 prepare 即早退（`playback_session.cpp:1361-1367`），两套诊断
  通常不并存，双倍成本主要体现在 loader 通过后的正常路径。
- 建议：loader 校验结果传递给 resolver 复用，或 resolver 信任 loader 前置校验。

#### CH-16 之前的 P3 项（CH-09 至 CH-16）

| ID | 级别 | 位置 | 问题 | 建议 |
| --- | --- | --- | --- | --- |
| CH-09 | P3 | `engine/chart/src/chart_loader.cpp:57`、`chart_v4_loader.cpp:759` vs `canonical_chart_loader.cpp:1918` | 同一"format 不是 cuexis.chart"条件使用 `chart.format.unsupported` 与 `chart.format.invalid` 两种诊断码（版本条件两处统一用 `chart.version.unsupported`，无分裂） | 统一 format 类诊断码 |
| CH-10 | P3 | `engine/chart/src/chart_v4_resolver.cpp:636-640`、`:655-660` vs `chart_parameter_resolver.cpp:67-70` | resolver 用 `chart.parameter.type_mismatch` 报范围错（weight 解析出 [0,1]、durationScale 非正），与 `out_of_range` 惯例不一致（实际难触发，normalizeValue 先挡） | 统一为 out_of_range 类码 |
| CH-11 | P3 | `engine/chart/src/chart_v4_reader_internal.cpp:307-311` | `chart.animation.mask_conflict` 被挪用作 mask 条目数预算错，与 §10 预算类用 `*_limit` 的惯例不符 | 拆分专用 `*_limit` 码 |
| CH-12 | P3 | `engine/chart/src/chart_v4_resolver.cpp:1345`、`:1377` vs CHART_V4 §1.1 | 文档写 "template expansion → ParameterSet 冻结"，实现相反（参数先解、图谱后成）；§3 又支持实现，文档内部两说 | 修订 §1.1 顺序描述 |
| CH-13 | P3 | `engine/chart/src/chart_runtime.cpp:308-311` | `compileBehaviorTracks` 诊断 `keyPath` 用排序后下标，宿主按路径定位会命中错误源元素 | 保留输入顺序的路径或额外携带源索引 |
| CH-14 | P3 | `addError`/`addDiagnostic` 6 份（canonical `:38-63`、runtime `:26-36`、v4_loader `:29-36`、resolver `:40-53`、migrator `:30-34`、param_resolver `:21-25`）；`readVec3/readQuat/readFiniteNumber` 两套（canonical `:136-220` vs reader_internal `:42-124`；漂移实例：分量越 float 域时 canonical `readFloat` `:136-149` 报 `chart.number.out_of_range`，v4 侧 `readVec3/readQuat` `:69-109` 仅置 `valid=false` 无诊断）；相机校验三处（canonical 组件级 `:618-700`、canonical readCamera `:1748-1856`、chart_runtime `:144-213`）；`parameterTypeName` 两份（`chart_v4_loader.cpp:38`、`chart_parameter_resolver.cpp:27`） | chart 内部读取/校验工具重复约 400 行且已漂移 | 收敛到单一 internal 模块（见第 8 节方向 8） |
| CH-15 | P3 | `engine/chart/src/chart_runtime.cpp:262`、`engine/chart/src/chart_migrator.cpp:521-527` | 以 `"{}"` 字面量判定 opaque tracks（与默认 `"[]"` 隐式耦合）；`binary_search(emptyBehaviorIds)` 依赖其恰好按序构造（`:452-454` 先按 id 排序后按序遍历 push，从未显式排序） | 具名常量/显式排序断言 |
| CH-16 | P3 | `engine/chart/src/canonical_chart_loader.cpp:1459-1463`、`chart_runtime.cpp:546-553` | canonical 侧超限即早退（`addError(...); return result;`；templates/behaviors 顶层计数检查 `:961-966`、`:1087-1092`、`:1254-1259` 同样早退，仅元素级检查 `:1131-1136`、`:1176-1183` 用 continue）；`chart_runtime.cpp:546-553` object/behavior 超限后只加一条错误、循环继续处理全部元素（仅受诊断上限间接截断），直连 API 传超大文档做无用功 | chart_runtime 侧超限即中止该类循环 |

### 4.2 engine/playback（PB-01 至 PB-16）

#### PB-01 [P1] Chart v4 + 非 portable renderable 组合硬失败且错误码误导，限制未文档化

- 位置：`engine/playback/src/playback_session.cpp:1388`（v4 无条件采用 resolver 的
  resourceRequirements）、`:1500` 与 `engine/playback/src/presentation.cpp:1946-1948`
  （非 CXPRES01 payload 时 preparePresentation 返回 nullopt）、`:1587` 与 `:513-518`
  （identity 组装对 wantsPresentation requirement 报 `playback.identity.resource_missing`
  "missing a fetched presentation resource"）；需求收集
  `engine/chart/src/chart_v4_resolver.cpp:1246-1247`（对所有 renderable 无条件收集
  RenderableMesh/RenderableMaterial uses）；v3 对照 `:1569-1585`（仅 presentation 存在时
  才追加，legacy 成功）
- 问题：同一 legacy Stage 1B chart 用 v3 包装可 prepare 成功（
  `tests/playback/playback_allocation_tests.cpp:269` 为 v3+legacy renderable 用例；
  `tests/playback/presentation_tests.cpp:656` 的对照 fixture 为 version 1），用 v4 则整次
  prepare 失败且错误码指向不存在的"fetched presentation"；CHART_V4 无任何 "v4 renderable
  必须 CXPRES01" 条款；全部测试无 v4+legacy 组合覆盖。
- 建议：在 CHART_V4 明文冻结该限制并把错误改为专用 code（如
  `playback.chart.v4.requires_portable_presentation`），或对 nullopt presentation 的 v4
  情形给出与 v3 一致语义；补组合测试。

#### PB-02 [P2] prepareReload 两个重载对 AssetDatabase/ContentProvider 继承不对称

- 位置：`engine/playback/src/playback_session.cpp:1262-1267`（文本重载显式注入
  contentProvider 与深拷贝 database）vs `:1284-1298`（source 重载不注入）
- 问题：宿主用 `fromChartText` 造 source 再走 source 重载 reload 带 main music 的 chart
  会得到 `playback.content.asset_database_missing`（`:1471-1474`）；ADR 0038/CFU-E1 只
  描述了 source 统一，未描述该差异。
- 建议：统一两重载继承语义并在头文件文档化。

#### PB-03 [P2] lastOperationDiagnostics 仅在约半数失败路径写入且规则无文档

- 位置：`engine/playback/src/playback_session.cpp`；写入路径 `:1352`、`:1361`、`:1373`、
  `:1395`、`:1409`、`:1426`、`:1435`、`:1446`、`:1481`、`:1495`、`:1504`、`:1529`、
  `:1551`、`:1590`；未写路径：ChartWriter 失败 `:1417-1419`、mode/content mismatch
  `:1454-1460`、commit 失败 `:1535-1537`、buildSnapshotLayout 失败 `:1558-1561`、source
  invalid `:1322-1332`、全部 catch `:1628-1636`（异常路径留下上一操作旧 diagnostics）；
  公共头 `engine/playback/include/cuexis/playback/playback_session.hpp:319` 零注释
- 证据：`tests/playback/playback_session_tests.cpp:181/205/433` 只覆盖"会写入"的一半。
- 建议：RAII DiagnosticsRecorder 析构时无条件写入（见第 8 节方向 4）。

#### PB-04 [P2] 规范错误码 playback.presentation.frame.value_invalid 在 engine/playback 生产代码零发射

- 位置：`docs/formats/PORTABLE_PRESENTATION.md:611`（规范定义）vs
  `engine/playback/src/presentation_extraction.cpp:306-330`（normalizePresentationFrame
  只查 finite）
- 问题：[0,1] 冻结实际在输入边界执行（`engine/world/src/property.cpp:214`、`:224-231` 与
  `engine/chart/src/canonical_chart_loader.cpp:448-449`），extract 层只报 non_finite、
  永不发射该码。注：Stage 3 Validation Sink（`tests/presentation/validation_sink.cpp:110-117`，
  发射点 `:1411`/`:1430`，断言 `validation_sink_tests.cpp:596/605`）实现了该码，故并非
  全仓零实现，但生产路径仍无发射点。
- 建议：规范删除该码，或注明"frozen range 在上游边界拒绝、extract 层只报 non_finite"。

#### PB-05 [P2] update() 使 PreparedPlayback 过期的语义未文档化

- 位置：`engine/playback/src/playback_session.cpp:1743-1746`（每次 update
  `++generation`）、`:1657-1659`（commit 时 stale 检查）；测试锚定
  `tests/playback/playback_session_tests.cpp:493`
- 问题：任何 in-flight `PreparedPlayback` 在下一次 update() 后 commit 即
  `playback.prepared.stale`；PORTABLE_PRESENTATION §9（`:519`）只写 commit 检查，未写
  "update 也会使 candidate 过期"，而这对宿主"prepare → 下一帧 → commit"编排有直接影响。
- 建议：PORTABLE_PRESENTATION §9 补该语义。

#### PB-06 [P3] ADR 0030 版本示例过期

- 位置：`docs/adr/0030-playback-preview-api-version-and-result.md:51`（"当前 external
  consumer 必须带最低 SDK API 版本调用 `find_package(Cuexis 0.4 ...)`"）vs
  `cmake/CuexisVersion.cmake:8`（0.7.0）与 `docs/guides/VERSIONING.md:79`
- 建议：按项目惯例在文首加快照说明。

#### PB-07 [P3] fromFilesystemProject 对 v4 chart 的 CXT import 读取失败静默吞掉

- 位置：`engine/playback/src/playback_source.cpp:690-698`
- 问题：`if (cxtText) { push_back }`，失败推迟到 prepare 期以 entry/import missing 类错误
  出现，丢失 `playback.cxt.*` 原始错误上下文。
- 建议：读取失败即返回带 cause 的 source 错误。

#### PB-08 [P1] 五个公共方法可让 bad_alloc 穿越公共边界

- 位置：`engine/playback/src/playback_session.cpp:1187`（capabilities）、`:2068`
  （contentInfo）、`:2094`（diagnostics）、`:2105`（lastOperationDiagnostics）、
  `:2135-2146`（acquireHostOverride 的 reserve/push_back/string 拷贝）
- 问题：按值返回含 `vector<std::string>` 的集合，bad_alloc/length_error 直接逃逸；违反
  "Exceptions must not cross module public boundaries" 硬约束与 PORTABLE_PRESENTATION §2
  "公共方法捕获分配异常"；同文件已有正确范式（`presentationManifest` `:2018-2027`、
  `validatePresentation` `:1108-1130`、`prepare` `:1628-1636`）。`PreparedPlayback` 一侧
  干净。
- 建议：按同文件既有范式补 try/catch 转 Result 错误。

#### PB-09 [P2] noexcept 构造函数经 allCapabilities() 做堆分配，OOM 即 terminate

- 位置：`engine/playback/src/playback_session.cpp:1156-1162`；公共头
  `engine/playback/include/cuexis/playback/playback_session.hpp:252-253` 无提示
- 建议：构造期惰性初始化或头文件注明 noexcept-分配组合。

#### PB-10 [P2] 跨线程析构/移动 = std::terminate 的契约仅在错误码可推测，头文件零文档

- 位置：`engine/playback/src/playback_session.cpp:774-796`、`:1164-1168`；ThreadChecker
  `engine/core/include/cuexis/core/thread_checker.hpp:17`（playback 以 `core::ThreadChecker
  ownerThread` 成员使用，`playback_session.cpp:709`）
- 建议：`playback_session.hpp`/`playback_source.hpp` 顶部加 owner-thread/terminate 的
  英文 ASCII 注释块。

#### PB-11 [P2] 四处只写不读的死状态

- 位置：`engine/playback/src/playback_session.cpp:713`（`State::activeChartJson`，写
  `:1670`、清 `:1968`，每 session 常驻持有整份 chart 文本，上限 16 MiB/文档
  `playback_source.cpp:32`）、`:723`（`State::parameters`，写 `:1677`、清 `:1977`）、
  `:746`（`PreparedPlayback::State::parameters`，写 `:1615`）、
  `engine/playback/src/playback_source_state.hpp:20`（`PlaybackSource::State::sourceId`，
  写 `playback_source.cpp:498/606/711/740/770`）
- 建议：直接删除（已 grep 确认无读者）。

#### PB-12 [P2] prepare 期 O(n^2) 资源查找

- 位置：`engine/playback/src/presentation.cpp:2325-2336`（`findPresentationResource` 对
  map 线性 find_if）；根因 `engine/playback/src/presentation_internal.hpp:27`
  （`std::less<PresentationResourceKey>` 不可异构查找）；调用点
  `engine/playback/src/playback_session.cpp:600-666`（每 object×material×step-event）与
  `:520-523`（assembleResourceIdentities 线性扫 manifest）；上限 65,536 entries
  （`presentation.cpp:36`）
- 建议：`std::less<>` 透明比较器 + assetId 唯一性前移（见第 8 节）。

#### PB-13 [P2] prepareReload(文本) 每次深拷贝整个 AssetDatabase

- 位置：`engine/playback/src/playback_session.cpp:1265`
- 问题：CXC 包 index 可达数万条记录，reload 是可频繁操作。
- 建议：reload 复用或共享 database（shared owning）。

#### PB-14 [P2] 三个超长函数构成最大可维护性税

- 位置：`engine/playback/src/playback_session.cpp:1301-1638`（prepare 恰 338 行、24 处
  return 退出点、五层嵌套）；`validatePresentation` 约 286 行（8 组 capability 检查——
  7 组在 `:892-919`、1 组在 `:993-997`——与 5 组 limit 检查 `:944-969` 为纯重复模式）；
  `engine/playback/src/presentation.cpp:974-1350`
  （parseShader 约 375 行，"read counted name → identifier 校验 → set 去重"重复 4 次，
  keywords/parameters/bindings/extensions `:1099-1264`）
- 建议：见第 8 节方向 4 与表驱动化。

#### PB-15 [P3] 一致性小瑕疵组

| 位置 | 问题 |
| --- | --- |
| `playback_session.cpp:189`、`presentation.cpp:55`、`presentation_extraction.cpp:27` | `resourceTypeName` 三处重复实现 |
| `playback_session.cpp:610-628` vs `:651-666` | buildSnapshotLayout 内 material 解析逻辑双份 |
| `playback_session.cpp:620-623` | 死防御检查 `(*material)->reference.type == Shader`——`findPresentationResource` 按 (assetId, type) 匹配，Unlit/Parameterized 查找不可能返回 Shader，条件恒假、`:621-623` 不可达 |
| `presentation.cpp:665-672` | parseTexture 的 `expectedBytes < bytes.size()` 特判在 texture 路径遮蔽 `validateExactSize` 的 `expected < actual` 分支（`presentation.cpp:254-258`），但改报 `playback.presentation.texture.pixel_size_invalid` 而非 size_mismatch |
| `playback_session.cpp:300-310` | allCapabilities() 初始化序未排序（构造函数会 normalize，无实际影响） |
| `playback_session.cpp:1750-1773` | extractFrame 值重载与目的重载双重校验 owner/reentry |
| `engine/playback/src/runtime_timeline.cpp:37-41` | advance 回归检查对所有 state 生效但错误文案只提 "Playing source position" |

#### PB-16 [P2] presentationManifest() 每调用深拷贝整个 manifest

- 位置：`engine/playback/src/playback_session.cpp:2018-2019`
- 问题：每次调用按值拷贝全部 entry strings/dependencies，而规范只要求 "owning copy"
  语义；宿主轮询该接口时成本随 manifest 规模线性放大。
- 建议：Stage 6 SDK 0.8.0 增加 `shared_ptr<const PresentationResourceManifest>` 新重载
  （旧 API 保留 deprecated），同步修订 PORTABLE_PRESENTATION §2:240；属 minor 版本变
  更，需按 VERSIONING 流程走。

### 4.3 资源与包管线：cxc · project · assets · shader（CX-01 至 CX-27）

#### CX-01 [P1] Asset Index record 级 `extensions` 字段是实现私自扩展

- 位置：`engine/project/src/asset_index_reader.cpp:243-251`、`:342-347`；字段保留于
  `engine/project/include/cuexis/project/asset_index_reader.hpp:52`
- 问题：record schema 按 ADR 0026 只有 `id/type/source/dependencies` 并明确"核心未知字段
  是错误"（0026 同段允许 opaque extensions 在 document 级保留并告警，但 record 级示例无
  该字段）；ADR 0031 v2 与 MATERIAL_SHADER § 3 的 v3 示例同样没有该字段；
  `tests/project/asset_index_reader_tests.cpp` 未覆盖。CXC pack 会原样打包并发布一个
  三份 ADR/spec 都未定义的字段，与严格性矛盾。
- 建议：要么在 ADR 0026/MATERIAL_SHADER 补记该字段（含预算），要么 Reader 拒绝之。

#### CX-02 [P1] importer 默认缓存键与 Player 查询键域分叉，缓存永远 miss

- 位置：`tools/asset_importer/main.cpp:325`（无 `--identity` 时用
  `hashStandaloneSourceIdentity(*vertex, *fragment, "main", "main", selected)`，entry
  硬编码 "main"；`engine/shader/src/shader_cache.cpp:266-284` 的 standalone 回退 identity
  只哈希源码+entry+keyword，而缓存键 `encodeCacheKey` `:230-262` 本身含
  sourceIdentity/importer/targets/tools 字段——分叉在 identity 取值域：standalone 哈希
  ≠ CXPRES identity）
  vs Player/OpenGL 查询 `engine/render_opengl/src/open_gl_presentation.cpp:546-552`
  （`shaderResource.reference.identity.sha256`，按 MATERIAL_SHADER § 7 含 schema/binding/
  profile/default render state）
- 问题：MATERIAL_SHADER.md § 10 规定缓存键含 "source shader semantic identity"，是单一
  契约；`engine/shader/include/cuexis/shader/shader_cache.hpp:57-59` 注释自认分叉。默认
  importer 工作流产出的缓存对 Player 不可见（`shader.cache.missing` → 重编译或失败），
  且与 payload 实际 entry 无关的 "main" 哈希造成不同 shader 的 identity 碰撞面。
- 建议：importer 默认解析 payload 的 CXPRES identity（或强制 `--identity` 必填）；删除
  standalone 分叉或在文档显式标注两个键域。

#### CX-03 [P2] MATERIAL_SHADER § 11 与实现的模块拓扑不一致

- 位置：`engine/shader/CMakeLists.txt`（`cuexis_shader_cache` 无条件 add_library，依赖仅
  core）；根 `CMakeLists.txt:129-130`、`:632-637`（注册）、`:638-650`（shader allowlist
  实际仅 core+shader_cache+三方工具）
- 问题：文档 § 11 只列 `cuexis_shader / cuexis_playback / cuexis_render_opengl /
  cuexis_asset_importer`，未提非可选的 `cuexis_shader_cache`；§ 11 写的
  "cuexis_shader optional -> cuexis_assets / cuexis_project as needed for paths" 从未实现。
- 建议：修订 § 11 拓扑。

#### CX-04 [P2] CXC_FORMAT § 9 "不作为安装 component 暴露" 与 STATIC 构建安装现实存在措辞张力

- 位置：根 `CMakeLists.txt:446-460`（cuexis_cxc 列入 CUEXIS_STATIC_IMPLEMENTATION_TARGETS）、
  `:479-483`、`:498-505`（静态块 find_dependency minizip-ng，经 `cmake/CuexisConfig.cmake.in:6`
  注入）；EXPORT_NAME `InternalCxc` 在 `engine/cxc/CMakeLists.txt:6`，文档措辞在
  `CXC_FORMAT.md:275-276`
- 问题：头文件确实未安装，但 static-link 必须随包安装内部 archive 并 find_dependency
  minizip-ng，"不作为安装 component 暴露"未描述该例外。
- 建议：CXC_FORMAT § 9 补 ADR 0033 static 拓扑说明。

#### CX-05 [P2] 热重载合同（ShaderPipelineCache）无生产消费者

- 位置：`engine/shader/src/shader_pipeline_cache.cpp:44-160`（candidate/active noexcept
  swap、失败保留上一 Pipeline）仅被 `tests/shader/shader_cache_tests.cpp:226-285` 使用；
  Player 实际直接用 `ShaderCacheStore` + 本地编译（
  `engine/render_opengl/src/open_gl_presentation.cpp:555-576`），失败路径无 "keep previous
  pipeline" 消费点
- 问题：MATERIAL_SHADER § 10 的热重载合同只有单测证明；§ 14 已声明 S5-H hosted/owner
  acceptance 完成前不得标 completed，此项属未兑现的 Player 证明。
- 建议：Player OpenGL 路径改用 `ShaderPipelineCache`，或将其移至 cuexis_shader 待 Stage 6
  接入。

#### CX-06 [P3] Reader 接受 versionNeeded ∈ [0,20] 为未文档化宽容

- 位置：`engine/cxc/src/zip32_envelope_internal.cpp:150-155`（CXC_FORMAT § 3.2 只规定
  writer 写 10）
- 建议：把 reader 宽容上限写进文档。

#### CX-07 [P2] CxcContentProvider::readBlob 重抛 std::bad_alloc 跨接口边界

- 位置：`engine/cxc/src/cxc_package.cpp:756-757`；ContentProvider 接口注释明确
  "Implementations must not allow exceptions to cross this boundary"
- 问题：`loadMemory/loadFile/CxcWriter::write/CxcManifestLoader::load` 顶层重抛 bad_alloc
  （`:827`、`:840`、`:878`；`cxc_writer.cpp:220`；`cxc_manifest_loader.cpp:335`）作为工厂
  尚可辩护，但 readBlob 是纯虚接口实现，违约文本明确。
- 建议：readBlob 捕获 bad_alloc 转 Result 错误（`content.cxc.read_failed`）。

#### CX-08 [P3] 裸调 diagnostics.add 与 static_cast<void> 风格不一致

- 位置：`engine/cxc/src/cxc_manifest_loader.cpp:28-29` vs `engine/cxc/src/cxc_package.cpp:74`

#### CX-09 [P2] buildPackage 单函数约 325 行、13 处 return（12 处早退）

- 位置：`engine/cxc/src/cxc_package.cpp:351-674`（按 CXC_FORMAT § 6 串 ZIP→manifest→
  project→root 折叠→asset index→依赖图→chart→CXT→closure 九阶段）
- 建议：以 spec § 6 验证顺序拆 stage 函数，共享 BuildContext，保持诊断顺序确定性。

#### CX-10 [P2] isV4Chart 触发整份 Chart JSON 双重解析

- 位置：`engine/cxc/src/cxc_package.cpp:113-130`、`:593`、`:651`（Chart 解析实际上限
  16 MiB = min(maxInputBytes 16 MiB, maxEntryBytes 64 MiB)，64 MiB 常量在 `cxc_package.hpp:27`）
- 建议：chart 模块提供 `peekChartVersion` 或 loader 接受预解析 DOM（并入 CH-05 的
  parse-once）。

#### CX-11 [P2] Chart/CXT 源文本约 3 份驻留

- 位置：`engine/cxc/src/cxc_package.cpp:587-588`、`:611`、`:644`（chart bytes 同时存在于
  archiveBytes、chartText 栈副本、projectDocuments 条目；CXT 分支 `*cxtText` 未 move）
- 建议：move 语义与单一驻留。

#### CX-12 [P2] 加载每字节约 4 遍、写入每字节约 6 遍

- 位置：加载：envelope 逐 entry CRC32（`engine/cxc/src/zip32_envelope_internal.cpp:504`）
  + SHA-256（`:510`），随后 `verifyWithMinizip` 全量再提取 + memcmp（`:702-719`，每 entry
  临时 vector 分配，512 MiB 包 = 约 4x I/O + 每 entry 64 MiB 峰值临时分配）；写入：每
  entry SHA（`cxc_writer.cpp:76`）→ writer CRC（`zip32:606`）→ round-trip loadMemory 再
  CRC+SHA+minizip（`cxc_writer.cpp:204-207`）
- 问题：CFU-F 双保险安全门正确但代价大，作为默认库 API 偏重。
- 建议：见第 8 节方向 6（校验分层化/流式比对）。

#### CX-13 [P3] 索引与文本工具的小额低效

| 位置 | 问题 |
| --- | --- |
| `engine/cxc/src/cxc_package.cpp:45-46`、`:370-374` | entryIndexes/contentPaths 用 `std::map<string>` O(log n) + 每 entry metadata 在 zipEntries 与 entries 双份 |
| `cxc_package.cpp:90-97`、`zip32_envelope_internal.cpp:105-113`、`:177-181` | textFromBytes/textAt/appendText 逐字节循环，可用 memcpy 等价替换 |

#### CX-14 [P3] portable path 校验三处重复且宽严不一

- 位置：`engine/project/src/project_loader.cpp:114-186` 与
  `engine/cxc/src/cxc_path_internal.cpp:48-77` 严格（`[A-Za-z0-9._-]`）；
  `engine/assets/src/asset_database.cpp:102-128` 接受任意可打印 ASCII（0x21-0x7E 除
  `\`/`:`，如 `*`、`"`、`|`）
- 问题：直接构造 `AssetDatabaseInput` 的调用方绕过 ADR 0025 规则。
- 建议：三处收敛到 `cuexis_internal/portable_path` 单实现，诊断由调用侧注入。

#### CX-15 [P3] prepareProject root overlap O(n^2)

- 位置：`engine/project/src/project_loader.cpp:307-323`（maxAssetRoots=16 下无实际影响）

#### CX-16 [P3] saveAtomic 双 serialize+双 parse 建议注释标明为设计

- 位置：`engine/project/src/project_loader.cpp:837-890`（写前读回验证：serialize→parse
  比对 `:840-847`、写临时文件后读回再 parse 并比对 `:881-890`，是正确性设计，易被误读
  为冗余）

#### CX-17 [P2] ResourceScope::request 五路 switch 双重复制约 350 行

- 位置：`engine/assets/src/resource_manager.cpp:954-1045` 与 Fallback 分支 `:1068-1204`；
  五个 case 仅 Tag 不同，同文件 `requestDirect<Tag>`（`:511-548`）已是模板化正确样板；
  `request()` 因此膨胀到约 390 行
- 建议：模板化 `acquireTyped<Tag>`，可删约 70% 且消除复制粘贴漂移风险。

#### CX-18 [P3] contains(handle) 线性扫描

- 位置：`engine/assets/src/resource_manager.cpp:1287-1320`（五个重载均 `std::any_of` 对
  entries_ 线性扫描；`entryIndex_` 键为 (AssetType, AssetId)，handle 不含 AssetId 无法
  反查，需另建 handle 键索引）

#### CX-19 [P3] AssetDatabase::build 约 330 行

- 位置：`engine/assets/src/asset_database.cpp:214-544`；依赖环 DFS 有 colors+limitReached
  防护（`:470-510`），环诊断带完整路径文本（`:164-178`），质量好，仅长度问题。

#### CX-20 [P3] readBlob 靠比较 provider 错误码字符串重映射

- 位置：`engine/assets/src/asset_database.cpp:659-667`；provider 码改名即静默失配。
- 建议：结构化错误分类替代字符串匹配。

#### CX-21 [P3] 双 AssetType 枚举并存 + 两份 assetTypeName

- 位置：`engine/project/include/cuexis/project/asset_index_reader.hpp:25-31` vs
  `engine/assets/include/cuexis/assets/asset_database.hpp:30-36`；
  `asset_index_reader.cpp:157-171` / `asset_database.cpp:196-210`
- 问题：人工同步，新增类型易漏一半。

#### CX-22 [P2] prepareCandidate 双重 encode/decode

- 位置：`engine/shader/src/shader_pipeline_cache.cpp:129-137`（encodeCache→decodeCache
  一次只为取得 normalized key 的 record）vs `engine/shader/src/shader_cache.cpp:625-632`
  （store 内部已 encode→decode 自检再写盘）
- 建议：让 `store` 返回 decoded record 即可消除。

#### CX-23 [P3] key/request 回退组合可造成缓存键不一致

- 位置：`engine/shader/src/shader_pipeline_cache.cpp:99-111`（`key.selectedKeywords` 空而
  `request.selectedKeywords` 非空时，lookup key ≠ store key → 每次 miss+重编译；当前唯一
  调用方是测试且 `makeKey()` 不填 `key.selectedKeywords`，唯一传非空 request keywords 的
  用例编译期即失败未走到 store，组合当前未实际触发，属脆弱契约）
- 建议：`ShaderCacheKeyInput` 构造即从 request 派生，或 `encodeCacheKey` 拒绝"半空"输入。

#### CX-24 [P3] compileOnWorker 每次编译新建线程并 join

- 位置：`engine/shader/src/shader_pipeline_cache.cpp:28-40`（同步 facade 而非持久 worker；
  单次开销可忽略，但 § 10 "Worker compiles a candidate" 的语义实为调用方阻塞）

#### CX-25 [P3] validateSource 不做完整 UTF-8 校验

- 位置：`engine/shader/src/shader_compiler.cpp:152-177`（仅查 BOM 与 CR）；spec § 5
  "源码为 UTF-8"、§ 12 code 表含 "non-UTF-8 shader source"（该码在 playback 侧
  `presentation.cpp:820-830` 经 `isWellFormedUtf8` 完整校验并发射，缺完整校验的只是
  shader-tools 编译器路径）；注释混入非 UTF-8 字节可穿透到 shaderc。
- 建议：一次 ValidateUtf8 扫描补齐。

#### CX-26 [P3] ShaderCacheStore::store 固定 .tmp 名 + remove/rename 非原子

- 位置：`engine/shader/src/shader_cache.cpp:634-652`（同 key 并发写竞争；崩溃留 .tmp 残
  片。缓存可容忍，值得注释边界）

#### CX-27 [P3] ShaderCompiler::compile 未校验 entry 名与 parameters≤32/bindings≤16

- 位置：`engine/shader/src/shader_compiler.cpp:236-324`（validateDeclaredSchema 无 entry
  检查）；依赖上游 CXPRES parse 兜底，绕过 Playback 直接调 ShaderCompiler 的工具路径
  （如 importer）无防护。

### 4.4 运行时与渲染：runtime · world · render · render_opengl（RT-01 至 RT-34）

#### RT-01 [P1] reload 在 debug 采样失败时"已替换却报告失败"，违反替换事务语义

- 位置：`engine/runtime/src/runtime_session.cpp:1546-1555`（`replaceWith` `:1546` 已发布
  新 World/Scope/Diagnostics、旧状态已销毁，随后 `:1549-1553` debug 路径
  `sampleChartTimeMs` 失败导致 `reloaded=false` + 错误诊断）
- 问题：`docs/architecture/RUNTIME_SESSION.md:178` 承诺"准备失败保留旧 World、Scope 和
  活动 Diagnostics"，错误等级表（`:216-221`）定义 Recoverable="Replacement 失败；保留上
  一有效状态"。重试 reload 会再走一次全量替换（可恢复），但契约错位。
- 建议：debug 采样挪到 `replaceWith` 之前（`:1536-1539` 已有 swap 手法），或失败降级为
  Warning 并返回 `reloaded=true`。

#### RT-02 [P1] "预热后 update() 不分配"被 Stage 4 动画路径打破（rebuildAnimationBaselines 每帧重建）

- 位置：`engine/runtime/src/runtime_session.cpp:1021-1024`（每帧调用）→
  `:250-266`（`state.animationBaselines.clear()` 后逐 binding 重新 push_back
  AnimationObjectBaseline，内层 `properties` vector 首次 push_back 必然分配；
  `resolver.resolvedValue()` 返回 optional 值拷贝，RenderMaterial 字符串属性每帧每对象
  堆分配）
- 问题：`docs/architecture/RUNTIME_SESSION.md:154`（更新日期 2026-08-07，早于 Stage 4
  动画路径合入）承诺不分配。
- 建议：animationBaselines 持久持有、逐字段 in-place 更新（见第 8 节方向 5），或在文档
  把"不分配"限定为无 Animation 会话并注明 Stage 4 例外。

#### RT-03 [P2] debug 启用时每帧执行两次 sampleChartTimeMs

- 位置：`engine/runtime/src/runtime_session.cpp:1000`（updatePrepared 内）与 `:1292`
  （update() debug 分支）；`applyBaseProperty` 重放路径亦然（`:1476`）
- 问题：违反 RUNTIME_SESSION.md:148 "每帧只执行一次 chartTimeMs -> BeatSample"；
  `TimingMap::sampleChartTimeMs` 是双二分（`engine/chart/src/timing_map.cpp:388-409`）。
- 建议：beatSample 提升为 updatePrepared 出参复用。

#### RT-04 [P2] RenderBackend::renderFrame(RenderFrame) 是全仓零调用的死路径，头注释仍描述为现行帧契约

- 位置：声明 `engine/render/include/cuexis/render/render_backend.hpp:40`、override
  `engine/render_opengl/include/.../open_gl_backend.hpp:191`、实现
  `engine/render_opengl/src/open_gl_backend.cpp:524-615`；实际主路径
  `renderPresentationFrame`（`app/player/src/player_app.cpp:888/896/906`，DebugLine 经此
  消费）；`render_backend.hpp:27` 描述不存在的主路径；`maxCommandCount` 重复上限检查
  `open_gl_backend.cpp:561` 永假（`render_scene.cpp:32` 已在入队时强制）
- 建议：删除 `renderFrame`/`RenderFrame` 或头注释标注 legacy/diagnostic-only。

#### RT-05 [P3] RUNTIME_SESSION.md 调试快照字段清单落后 Stage 4

- 位置：`docs/architecture/RUNTIME_SESSION.md:158-160` vs
  `engine/runtime/include/cuexis/runtime/runtime_session.hpp:61-75`（RuntimeDebugRecord 现
  含 animationValue/hostOverrideValue/previewOverrideValue/sourceLayer/conflict/
  animationLayers）；`docs/formats/ANIMATION_MIXING.md:144-146` 已补 Animation Layer 维度
- 建议：调试小节加一行指向 ANIMATION_MIXING.md。

#### RT-06 [P3] 同一函数内异常策略不一致

- 位置：`engine/runtime/src/chart_world_instantiator.cpp:452-453`
  （`catch (const std::bad_alloc&) { throw; }` 穿过 runtime 公共静态方法，调用方
  RuntimeSession::prepare 不捕获，`runtime_session.cpp:844-959`）vs `:454-461`（其余
  std::exception 转诊断）
- 问题：符合"OOM 例外"但与文件内风格割裂；`chart_world_instantiator.hpp:83-90` 未声明
  可能抛 bad_alloc。
- 建议：头注释声明。

#### RT-07 [P3] CUEXIS_HAS_SHADER_TOOLS 以 PUBLIC compile definition 泄漏给 render_opengl 的消费者

- 位置：`engine/render_opengl/CMakeLists.txt`（末段）
- 建议：改 PRIVATE + 对外用 capability 查询（`builtInPresentationCapabilities` 已有
  `shaderGlsl450Source` 表达）。

#### RT-08 [P3] 匿名命名空间可变全局 configurationState 仅靠 requireMainThread 约束

- 位置：`engine/render_opengl/src/open_gl_backend.cpp:31-36`、`:43-51`
- 建议：注释显式声明线程契约。

#### RT-09 [P2] Behavior 引用二次二分查找缺防御检查，且同一逻辑四处重复

- 位置：`engine/runtime/src/runtime_session.cpp:476-487`（不检查直接
  `static_cast<size_t>(behavior - begin)` 并索引 `definitions[behaviorIndex]`，越界读风
  险；当前靠 prepare 先跑 `ChartWorldInstantiator::validate`（`:859`）的隐式前置）；带检
  查的样板 `:385-393`；重复处 `:898` 与 `engine/runtime/src/chart_world_instantiator.cpp:39-53`
- 建议：提取 `findBehaviorIndex()` 帮助函数统一带检查，传播错误而非信任前置。

#### RT-10 [P2] 每帧构造 std::vector<OverrideToken> activeHost/activePreview

- 位置：`engine/runtime/src/runtime_session.cpp:1043-1068`（栈上 vector + reserve 每帧堆
  分配）；`PropertyResolver::applyOverrides` 内部再分配一份 candidates
  （`engine/world/src/property.cpp:660-686`，另见 RT-20）
- 建议：复用 RuntimeEvaluationState 成员 scratch。

#### RT-11 [P2] objectIdFor 对每条 debug 记录做 O(n) 线性查找

- 位置：`engine/runtime/src/runtime_session.cpp:739-745`、`:1141`（debug 开启时
  O(records×objects)；ObjectEntityMap 按 objectId 有序但无按 entity 的反向索引）

#### RT-12 [P2] toPropertyId/toEasing 对未列枚举值静默回退默认值

- 位置：`engine/runtime/src/runtime_session.cpp:124`、`:135`、`:217`
- 问题：新增 `chart::BehaviorProperty` 枚举时错误映射成 TransformPositionX/Linear 而非报
  错，与"Result 不得静默忽略"精神相悖。
- 建议：改为 `core::unexpected` 或 terminate（与 instantiator validate 互补）。

#### RT-13 [P3] withWorld 非 const/const 两个约 30 行模板逐行重复

- 位置：`engine/runtime/include/cuexis/runtime/runtime_session.hpp:219-248` 与 `:250-277`；
  World::withRegistry 同样成对重复（`engine/world/include/cuexis/world/world.hpp:52-92`）
- 建议：`std::as_const` 委托消除。

#### RT-14 [P3] prepare 的动态 Material 预请求出错后外层循环继续请求资源

- 位置：`engine/runtime/src/runtime_session.cpp:906-927`（内层错误只记诊断、`:924` break
  退出 tracks 循环，`:928` 在对象循环外统一判定；出错后至多再请求下一个对象即被 `:878`
  的 hasErrors 检查 break。无正确性问题，仅少量无效 I/O）

#### RT-15 [P3] override lifetime 在帧提交失败回滚后仍被 tick

- 位置：`engine/runtime/src/runtime_session.cpp:1070-1071`（tickOverrideLifetimes 先于
  finalize/commit；若 `:1082` 之后失败，RemainingFrames 已扣减）；语义边界未文档化。

#### RT-16 [P3] pi 字面量 4 处（2 文件）重复；手写 ZYX 四元数构造全仓仅一处

- 位置：`engine/runtime/src/chart_world_instantiator.cpp:409-428`（`:409`/`:411`/`:413`
  含 3.14159265358979323846 字面量；`:423-428` 为全仓唯一手写 ZYX 四元数构造）、
  `engine/playback/src/playback_session.cpp:1868`（第 4 处 pi 字面量，用于
  `makePerspective` 投影计算，非四元数）
- 建议：pi 常量与 euler→quat 构造集中到 core/math 并注明 yaw/pitch/roll 复合顺序。

#### RT-17 [P2] markTouched 每次写做 touchedEntries_ 线性扫描，热路径 O(W×T)

- 位置：`engine/world/src/property.cpp:618-624`；调用链 `applyWrite`（`:609`）与
  `applyBaseProperty`（`:469`，对 committedEntries_ 另有线性查找 `:470-473`）
- 问题：大 chart（数千实体 × 每实体多属性）下每帧退化为数百万次比较；world 模块最值得
  先修的一处。
- 建议：epoch 戳 O(1) 记账（见第 8 节方向 7）。

#### RT-18 [P2] PropertyResolver::Entry 过胖（约 2.7KB/实体）

- 位置：`engine/world/include/cuexis/world/property.hpp:194-215`（10 个 PropertyState ×
  6 个 PropertyValue，variant 含 std::string 约 40B，加 3 份 TransformComponent）；
  `capture` 为每个 Transform 实体建 Entry（`property.cpp:389-422`，含默认相机）
- 问题：1 万实体约 27MB 常驻；findEntry 二分落在 2.7KB 大元素上 cache 命中率差；非动画、
  非材质属性的三份 override/animation 槽位永远空闲。

#### RT-19 [P2] ensureEntry 用 vector::insert 有序插入胖 Entry，prepare 期 O(N^2) memmove

- 位置：`engine/world/src/property.cpp:371-387`；调用点相机/外观 baseline 注册逐个调用
  （`:425-437`、`:316-353`）
- 建议：先攒后一次排序。

#### RT-20 [P2] applyOverrides 每帧每层分配 std::vector<Candidate> 并 sort

- 位置：`engine/world/src/property.cpp:660-686`；与 RT-10 叠加，override 常驻的 Studio
  预览每帧 3 次堆分配 + 排序。

#### RT-21 [P3] RenderMaterial 校验逻辑两份

- 位置：`engine/world/src/property.cpp:99-115` 与 `:199-211`（storeParsedPropertyValue 与
  parsedPropertyValue 重复"非空 string_view + present 检查"）

#### RT-22 [P3] applyLayer(duplicateIsError=false) 与 PropertyResolver::prepare 生产零调用

- 位置：`engine/world/src/property.cpp:635-643`（runtime 全部传 true；`duplicateIsError=false`
  分支无任何调用者，含 tests/）、`:731-738`（`PropertyResolver::prepare` 仅
  `tests/world/property_tests.cpp` 可达）
- 建议：标注 test-only 或删除。

#### RT-23 [P3] sameTransform 用 float 逐分量 == 做脏标记，前提未注释

- 位置：`engine/world/src/transform_system.cpp:36-39`（若未来引入重归一化将产生每帧误
  脏）；拓扑缓存 + parent-first 脏传播（`:203-269`）与先验证后提交（`:166-199`、
  `:231-269`）质量好。

#### RT-24 [P3] CameraComponent::type 每相机一个 std::string 且渲染端从不读取

- 位置：`engine/render/include/cuexis/render/camera_component.hpp:13`；
  `chart_world_instantiator.cpp:396-399` 原样转存；playback 投影只用 fovY/near/far
- 建议：改 enum 或文档标注 reserved。

#### RT-25 [P3] render 模块头注释描述的 "RenderSystem produces / renderFrame consumes" 主路径已死

- 位置：`engine/render/include/cuexis/render/render_backend.hpp:1-8`、
  `render_scene.hpp:3-6`（同 RT-04）

#### RT-26 [P1] buildDraws 每对象每帧对 resources 线性 find_if 取 mesh bounds

- 位置：`engine/render_opengl/src/open_gl_presentation.cpp:988-995`；对照
  mesh/material/texture 都有 `findGpuResource` 二分（`:677-687`，且 playback 侧 manifest
  已保证 canonical 序：`presentation_extraction.cpp:145-150`、
  `presentation_internal.hpp:17-23`）
- 问题：上限 10 万对象（`:849-855`）下是数量级风险。
- 建议：在 `GpuMesh` 里缓存 bounds（prepare 期一次）。

#### RT-27 [P1] 参数化程序数值 uniform 每帧每 draw 调 glGetUniformLocation(字符串)

- 位置：`engine/render_opengl/src/open_gl_presentation.cpp:1115-1143`、`:1167-1169`；纹
  理绑定 location 已在 prepare 期缓存（`:655-663`），数值 uniform 没有
- 建议：prepare 期为 `ShaderParameterValue::name` 缓存 location（存进 GpuMaterial）。

#### RT-28 [P2] 每帧固定堆分配与无条件 summary/digest 成本

- 位置：`engine/render_opengl/src/open_gl_presentation.cpp:1666-1701`（opaque/transparent/
  debugVertices 三个 vector 每帧新建 `:1687-1688`、`:1696`）；buildDraws 无条件把命令二
  次拷贝进 preparedSummary（`:1058-1065`，OpenGlDrawCommand::objectId 是 std::string，每
  对象每帧两次拷贝 `:962`、`:1061`）；summaryDigest（`:818-843`）对全部字符串逐字节 FNV，
  即使 `summary==nullptr` 也全做（`:1694`）；Player 常规循环永远传 `&drawSummary`
  （`app/player/src/player_app.cpp:906-908`）；Player 每帧新建 RenderScene 并 append 3x
  可见对象条 debug 轴线（`player_app.cpp:867-869`，另见 AP-08）
- 建议：scratch 复用 + 按需构建（见第 8 节方向 1）。

#### RT-29 [P2] 绘制排序只按 objectId，不按状态/程序分组

- 位置：`engine/render_opengl/src/open_gl_presentation.cpp:1046-1056`（opaque 仅按
  objectId）；`:1091-1095`、`:1151-1155`（逐 draw 翻转 GL_CULL_FACE）；unlit 与
  parameterized 同一 pass 内双遍历（`:1087-1090`、`:1148-1150`）
- 问题：状态切换次数 = 对象数级别。
- 建议：不透明 pass 排序键 `(programKey, cull, textureKey, objectId)`（透明 pass 保持
  depth key 主序）；需同步 validation_sink 第二套排序实现与 digest 域。

#### RT-30 [P2] debug pipeline 裸 GLuint + 手工 destroy，与 presentation 侧 Unique* RAII 并存三种清理路径

- 位置：`engine/render_opengl/src/open_gl_backend.cpp:120-135`、`:191-202`、`:635-643`、
  `:617-666`；对照 `engine/render_opengl/src/open_gl_presentation_internal.hpp:18-64`
  （UniqueBuffer/UniqueVertexArray/UniqueTexture/UniqueProgram）
- 建议：迁移到 Unique* 可删约 40 行。

#### RT-31 [P3] DebugVertex/shaderLog/programLog/compileShader 两份拷贝

- 位置：`engine/render_opengl/src/open_gl_backend.cpp:120-128`、`:137-189` vs
  `engine/render_opengl/src/open_gl_presentation.cpp:56-64`、`:171-219`

#### RT-32 [P3] 程序去重 key 用 '\0' 拼接 std::string

- 位置：`engine/render_opengl/src/open_gl_presentation.cpp:1522-1561`（assetId + sha256
  字节 + keywords；sha256 含 `\0` 字节靠 length 区分，正确但脆弱）
- 建议：tuple/struct key。

#### RT-33 [P3] activate/discard 合同违约一律 std::terminate() 仅头注释半句说明

- 位置：`engine/render_opengl/src/open_gl_presentation.cpp:1595`、`:1599`、`:1612`、
  `:1622`；`engine/render_opengl/src/open_gl_backend.cpp:490-492`、`:504-506`；
  `open_gl_backend.hpp:174-177`；PORTABLE_PRESENTATION 未提及 terminate 语义

#### RT-34 [P3] depthQuantization=4096.0 等常量缺来源注释

- 位置：`engine/render_opengl/src/open_gl_presentation.cpp:45`（透明排序键量化粒度选择
  依据未记录；`transformPoint`/`multiplyMatrices` 用 double 累加 `:701-730` 是正面做法）

### 4.5 core 与媒体模块（CM-01 至 CM-27）

#### CM-01 [P1] AGENTS.md "engine/animation 是未接入构建的 stub" 与现实矛盾

- 位置：`AGENTS.md`（repo layout 段）
- 证据（已抽查证实）：`engine/animation/CMakeLists.txt` 定义完整 `cuexis_animation`
  STATIC 库（6 个源文件，仅 `engine/particles` 是 INTERFACE stub）；根 `CMakeLists.txt:110`
  （CUEXIS_ACTIVE_TARGETS）、`:162`（animation_tests）、`:459`（静态包安装清单）、
  `:626-631`（依赖白名单 core/chart/world）；运行时逐帧调用
  `engine/runtime/src/runtime_session.cpp:1026`（`AnimationSystem::evaluate`）。与
  AGENTS.md 自身 "Stage 4 is complete / Stage 4 animation runtime" 声明自相矛盾。
- 建议：改为 "engine/particles is an INTERFACE stub; engine/animation is an active
  Stage 4 module"，并删除空目录 `tests/particles/`。

#### CM-02 [P1] docs/CURRENT_STATUS.md 对 audio 子系统零覆盖

- 位置：`docs/CURRENT_STATUS.md`（全文 206 行；grep audio/clock/transport/timeline 无任何
  命中）
- 问题：ChartClock/HostClock/CuexisAudio/RuntimeTimeline/`cuexis_audio`/`cuexis_audio_sdl`
  是 active baseline（AGENTS.md 明确声明），时钟契约只散落在
  `docs/architecture/RUNTIME_SESSION.md:143-144`；"唯一 current-status 摘要"对整块媒体
  子系统只字未提，违反其自身角色定位。实现与该声明一致（
  `engine/playback/src/runtime_timeline.cpp:78-92`；`engine/audio_sdl/src/sdl_audio.cpp:602-630`）。
- 建议：CURRENT_STATUS 补 audio/时钟小节。

#### CM-03 [P1] HostClock 公共 SDK 时钟类无任何同步原语，跨线程调用即 data race

- 位置：`engine/audio/include/cuexis/audio/audio_transport.hpp:59-69`（`CUEXIS_AUDIO_API`
  的 host 可用时钟）；`engine/audio/src/audio_transport.cpp:39-56`（`sample_` 为普通
  `SourceClockSample` 成员，`submit`/`snapshot` 无同步）
- 问题：对照同类 `SdlAudioTransport` 用 seqlock 发布
  （`engine/audio_sdl/src/sdl_audio.cpp:100-108`、`:153-170`），HostClock 却无线程契约说
  明。当前树内仅单线程使用（player 实际用 ChartClock + transport snapshot），但作为公共
  SDK 时钟类必须文档化 owner-thread-only 或改原子。
- 建议：复用 seqlock 模式，或头文件显式声明 owner-thread-only。

#### CM-04 [P2] normalize(Quat) 对大数量级输入静默返回零四元数且返回成功

- 位置：`engine/core/src/math.cpp:75-84`（|x|>1.8e19 时 `x*x` 溢出 inf，
  `lengthSquared = inf → inverseLength = 0` 分支返回 `{0,0,0,0}`）；`makeRotation`
  （`:96-104`）随后对零四元数 `glm::mat4_cast` 按公式确定性地产出单位旋转矩阵（旋转被
  静默丢弃，非未定义矩阵）；契约
  `engine/core/include/cuexis/core/math.hpp:55-57` 未声明溢出边界
- 问题：下游 behavior/animation 的 slerp 都依赖 `core::normalize`。
- 建议：补 `!isFinite(lengthSquared)` 分支返回错误 + 单测（并入第 8 节方向 5）。

#### CM-05 [P2] inverse() 绝对阈值与 nearlyEqual 纯绝对容差语义未声明

- 位置：`engine/core/src/math.cpp:133-135`（`|det| <= epsilon()` 约 1.19e-7，带 1e-4 缩放
  的矩阵 det 约 1e-12 会被误判 not invertible）、`:150-156`；`math.hpp:65` 未声明语义
- 建议：头文件写明阈值语义或改相对阈值。

#### CM-06 [P3] hexValue 仅接受小写 hex，isUuidV7 拒绝合法大写 RFC 4122 UUID

- 位置：`engine/core/src/uuid.cpp:21-29`；`engine/chart/src/chart_v4_loader.cpp:766` 用其
  严格校验 chartId；uuid.hpp 契约未写大小写说明
- 建议：头文件写明 "lowercase canonical form only" 或支持大写。

#### CM-07 [P3] animation_mixer.hpp 公共 include 暴露 entt::entity

- 位置：`engine/animation/include/cuexis/animation/animation_mixer.hpp:14`、`:25`
- 问题：不构成违约（animation 是 static implementation target，根 `CMakeLists.txt:479-481`
  只装 ARCHIVE 不装头；依赖方向在白名单内），但树内 API 与 EnTT 耦合，未来解耦需先动此头。

#### CM-08 [P3] transformPoint 直接丢弃 w 分量（无透视除法），affine-only 契约未声明

- 位置：`engine/core/src/math.cpp:145-148`

#### CM-09 [P3] sha256 无系统化 known-answer 测试

- 位置：`tests/core/` 无 sha256_tests.cpp；`tests/cxc/zip32_envelope_tests.cpp:41-48`
  经 using 别名直接以空串与 "abc" 两个标准向量测试 `cuexis::core::detail::sha256Hex`，
  但无 NIST 多向量与跨块边界用例；
  `engine/core/src/sha256.cpp:21-46` 实现经核对正确（finish 的 0x80 填充、>56 字节双块、
  大端位长、末尾 transform）
- 问题：FrameDigest/包身份全部压在其上，tests/core/uuid_tests.cpp 仅 19 行、
  result_tests.cpp 35 行，覆盖偏薄。
- 建议：补 NIST 186 向量 + 跨块边界用例（空串/"abc" 已有；见第 8 节方向 5）。

#### CM-10 [P3] Diagnostics 有界模式 append 语义怪癖

- 位置：`engine/core/src/diagnostic.cpp:56-61`（上限时 `items_.back() = limit` +
  `--acceptedCount_` 正确但绕）、`:68-78`（append 在源 Diagnostics 自身饱和时会把它尾部
  的 limit diagnostic 当普通条目搬进目标；当前调用方 schema.cpp:50 用无界源未触发）

#### CM-11 [P2] json parse 双 DOM：单次解析约 3 份瞬时内存、双倍转换开销

- 位置：`engine/json_support/src/parse.cpp:209-218`（构建完整 nlohmann DOM → 每对象
  `std::set<std::string>` 查重 `:78`、`:120` → `fromNlohmann` 深拷贝成自有 Value）；
  serialize 双重转换 `:238-247`
- 建议：SAX 直构 Value（见第 8 节方向 6）。

#### CM-12 [P3] Value::type() 直接 static_cast(storage_.index())

- 位置：`engine/json_support/src/value.cpp:52-54`（依赖枚举与 variant 顺序强耦合，无
  static_assert 防呆）

#### CM-13 [P3] sdl_runtime 文件级 sharedRuntimeState 全局可变状态

- 位置：`engine/platform/src/sdl_runtime.cpp:24`（匿名命名空间 weak_ptr，仅靠
  SDL_IsMainThread 事实串行化；建议注释说明）

#### CM-14 [P3] SDL 窗口/运行时析构跨线程 std::terminate 比头文件契约更硬（Release 也生效）

- 位置：`engine/platform/src/sdl_window.cpp:202-208`、`engine/platform/src/sdl_runtime.cpp:101-115`
- 说明：`sdl_runtime.hpp:27-30` 与 `sdl_window.hpp:41-45`/`:70-72` 已写明 owner-thread
  析构契约并称 "Debug builds assert this contract"；而 `thread_checker.cpp:12-14` 在
  Release 也真实比较并 terminate，实现比文档声明更严，文档未反映。

#### CM-15 [P3] secure_file 死分支

- 位置：`engine/filesystem/src/secure_file.cpp:197`（`size > size_t max` 在 64 位下恒假，
  无害）。模块加固质量正面：Windows reparse-point 拒绝（`:142-143`、`:181-184`）与读后
  句柄身份/时间戳复查（`:227-233`）；POSIX per-component `openat+O_NOFOLLOW`
  （`:316-336`）、`pread` EINTR 重试（`:361-377`）、读后 fstat 身份比对（`:380-384`）。

#### CM-16 [P3] Windows 路径 key 对全部字节 std::tolower

- 位置：`engine/content/src/content_provider.cpp:59-61`（对全部字节 `std::tolower`：非
  'A'-'Z' 字节在 C 标准 C locale 下返回原值、行为确定，仅当进程 setlocale 切换 locale 后
  才引入实现定义风险，可能造成 root 重叠/prefix 判定不一致）；core 已有 ASCII-only fold
  （`engine/core/src/portable_path.cpp:5-14`）可复用。Host 回调重入防护正确（`:249-260`，
  含 A→B→A 跨 provider 环）。

#### CM-17 [P2] 插值数学三份拷贝（+mixer 第四组），slerp 已现写法分叉

- 位置：`hermiteProgress` 两份（`engine/behavior/src/behavior_system.cpp:37`、
  `engine/animation/src/animation_sampler.cpp:24`）、Vec3 `lerp` 三份
  （`behavior_system.cpp:46`、`animation_sampler.cpp:33`、`animation_mixer.cpp:47`）、
  `slerp` 四处（`behavior_system.cpp:53`、`animation_sampler.cpp:40`、
  `animation_mixer.cpp:119`、核心 `animation_math.cpp:27-50`）；
  `engine/animation/src/animation_mixer.cpp:43-67` 还有第四组 quat helper
- 问题：slerp 已有 branch-on-dot（`behavior_system.cpp:56-60` 内联）vs alignHemisphere
  helper（`animation_math.cpp:29-30`）的写法分叉，是漂移温床。
- 建议：收进 `cuexis::core::math`（behavior/animation 均已依赖 core，白名单零改动），
  顺手修 CM-04（见第 8 节方向 5）。

#### CM-18 [P3] stopProgress 在 inStop==false 时也被强制校验 [0,1)

- 位置：`engine/behavior/src/behavior_system.cpp:237-240`（host 传 1.0 会被拒，契约意外）。
  sampleEvent/sampleStep 的 `upper_bound + *(next-1)` 边界已核对安全（`:190-211`、
  `:213-222`）。

#### CM-19 [P3] debug_draw 先收集再按 entity integral 排序求确定性

- 位置：`engine/debug/src/debug_draw.cpp:28-34`（调试路径可接受）

#### CM-20 [P3] "同 discontinuity 段内 position 不得回退"规则要求 Ended→Stopped 归零也先 bump discontinuityId，运行期才报错

- 位置：`engine/audio/src/audio_transport.cpp:43-48`
- 建议：写入头文件契约。

#### CM-21 [P2] activateReplacement 回滚与 Error 状态自相矛盾，回滚代码是死代码

- 位置：`engine/audio_sdl/src/sdl_audio.cpp:672-709`（openLease 失败时恢复 previous
  clip/state/frame `:691-695`，紧接着 `:696` enterError 把 state 置 Error、bump
  discontinuity、pause stream——此时 stream 已被 `:688` closeStream() 销毁）；且
  `enterError → updatePresentedFrame` 会在 `stream==nullptr` 且恢复后 state==Playing 时
  执行（`:257-286`），其中 `SDL_PauseAudioStreamDevice(nullptr)`（`:285`）靠 SDL 容错不崩
- 建议：删除回滚、固化契约"替换失败 → Error 态，需 unload 恢复"；补 replacement/
  underrun 测试（现 audio_sdl 仅 3 个 TEST_CASE，无 prepareReplacement/activateReplacement/
  cancelReplacement/underrun 覆盖）。

#### CM-22 [P2] updatePresentedFrame 的 clamp 不变式靠 5 个调用点共同维护，破坏即 UB

- 位置：`engine/audio_sdl/src/sdl_audio.cpp:279-281`
  （`std::clamp(segmentStartFrame + advanced, presentedFrame, min(submittedFrame,
  frameCount))` 要求 `presentedFrame ≤ min(submitted, frameCount)` 恒成立，由全部 4 个
  调用点（`:291` enterError、`:419` play、`:573`/`:597` service）共同维护）
- 建议：显式 min/max 防御。

#### CM-23 [P3] effectiveSettings() 跨线程读非 atomic 的 effective

- 位置：`engine/audio_sdl/src/sdl_audio.cpp:641-643`（owner 在 openLease `:239-250` 写它；
  snapshot() 有 seqlock，effective 没有；player 当前 owner 线程调用未触发）
- 建议：并入 PublishedClock seqlock 发布。

#### CM-24 [P3] stop()/seekMs 在 postmix 可能并发时 relaxed 清零计数器

- 位置：`engine/audio_sdl/src/sdl_audio.cpp:453`、`:493`（仅影响呈现估计精度，应注释）。
  回调安全正面：postmix 仅 3 个 atomic relaxed 更新（`:137-151`，static_assert
  `:22-25`）；publish/snapshot seqlock 模式正确（`:153-170`、`:602-630`）。wav_decoder
  RIFF 边界/chunk padding/blockAlign/24-bit 符号扩展/IEEE float 全部核对正确
  （`:76-146`、`:167-172`）。

#### CM-25 [P2] AnimationMixer::evaluate 单函数约 240 行，每帧重建 string-keyed map 与排序

- 位置：`engine/animation/src/animation_mixer.cpp:464-703`（内嵌约 110 行 collect lambda
  `:528-637`；每帧构建 `std::map<ChartObjectId(std::string),...>`、
  `std::map<PropertyId,vector>`、per-object 排序 layerOrder 等 `:472-534`）；ChartObjectId
  为 std::string（`engine/chart/include/cuexis/chart/chart_document.hpp:31-34`）
- 问题：runtime 每帧路径（`runtime_session.cpp:1026`）每帧字符串树查找 + 分配；预算上限
  使最坏情况有界，但收益明显的优化点。
- 建议：编译期预计算（见第 8 节方向 7 的 animation 部分）。

#### CM-26 [P3] AnimationSystem::sample 近乎死 API

- 位置：`engine/animation/src/animation_system.cpp:8-27`（仅
  `tests/animation/animation_sampler_tests.cpp:254-263` 使用，树内无生产调用）
- 建议：注明用途或移除。

#### CM-27 [P3] MixValue::resourceView 别名 chart 拥有的字符串，无注释

- 位置：`engine/animation/src/animation_mixer.cpp:25-28`（MixValue 定义，`:112` 在
  toMixValue 中赋 resourceView；复制后仍指原串；因
  program 生命周期覆盖 evaluate 而安全，但极不直观，必须有注释）。
  resolveLocalBeat 的迭代/fillMode 边界逐分支核对正确
  （`animation_sampler.cpp:176-247`，含 iterationIndex==0 边界不走 Hold）。

### 4.6 app · tools · 构建 · 测试体系（AP-01 至 AP-24）

#### AP-01 [P1] BUILDING.md 的 find_package(Cuexis 0.6 ...) 示例已失效（已抽查证实）

- 位置：`docs/guides/BUILDING.md:267`、`:278`（另有 `:288` 的 0.6 表述）
- 问题：当前 SDK API 为 0.7.0（`cmake/CuexisVersion.cmake:8`），package 使用
  `SameMinorVersion`（根 `CMakeLists.txt:514-518`），请求 0.6 会被兼容性拒绝；实际
  consumer fixture 均请求 0.7（`tests/external/find_package/CMakeLists.txt:9`），拒绝门
  禁是 `0.6 0.8`（`cmake/VerifyExternalConsumer.cmake:468`）。用户照抄即失败。
- 建议：示例改为 0.7。

#### AP-02 [P1] BUILDING.md "0.5/0.7 请求被拒绝" 是 SDK 0.6.0 时代的过期陈述，且与同文件自相矛盾

- 位置：`docs/guides/BUILDING.md:299` vs `:315`（"当前 SDK API 均为 0.7.0"）；门禁现拒
  0.6/0.8（`cmake/VerifyExternalConsumer.cmake:468`）
- 建议：更新为 "0.6/0.8 请求被拒绝"。

#### AP-03 [P2] BUILDING.md 开发工具清单缺 cxc 工具与 asset_importer

- 位置：`docs/guides/BUILDING.md:37-38` vs `tools/CMakeLists.txt:1-10` 与根
  `CMakeLists.txt:138-150`（实际注册 cuexis_cxc_pack/validate/unpack 与
  cuexis_asset_importer）

#### AP-04 [P2] BUILDING.md "当前正式激活的库 target" 清单缺 cuexis_animation 与 cuexis_cxc

- 位置：`docs/guides/BUILDING.md:14-35` vs 根 `CMakeLists.txt:110-111`（两者在
  CUEXIS_ACTIVE_TARGETS）与 `engine/CMakeLists.txt:16`、`:20`（真实 STATIC 库）

#### AP-05 [P3] CXC_FORMAT "unpack ... never overwrite" 与实现不完全一致

- 位置：`docs/formats/CXC_FORMAT.md:291-293` vs
  `tools/cxc_common/src/cxc_unpack.cpp:36-52`、`:126-162`（允许已存在的空目录作为输出并
  整体替换，rename + backup 回滚）
- 建议：措辞改为 "write into an empty (new or existing) directory; never overwrite
  content"。

#### AP-06 [P3] AGENTS.md animation 段过期 + tests/particles 空目录

- 位置：`AGENTS.md`（同 CM-01）；`tests/particles/` 为空目录，建议删除。

#### AP-07 [P2] player run() 单函数约 680 行，smoke/audio 剧本硬编码内嵌于帧循环

- 位置：`app/player/src/player_app.cpp:341-1020`（音频脚本按 frame 15/30/45/46/55/60
  `:559-709`；reload 脚本按 frame 3/4 `:720-846`；debug parity 按 frame 1 `:885-920`）
- 问题：新剧本只能继续加 `renderedFrames == N` 分支，难以测试与扩展。
- 建议：抽出"剧本步骤表"（frame → 可调用步骤）或独立函数（见第 8 节方向 8）。

#### AP-08 [P2] Player 每帧构造新 render::RenderScene 且 appendSnapshotAxes 无 reserve

- 位置：`app/player/src/player_app.cpp:867-870`；
  `engine/render/include/cuexis/render/render_scene.hpp:52`（commands_ vector 无 reserve）
- 问题：每帧堆分配/扩容，与 playback 层已复用 snapshot（`:547`、`:862`）的做法不一致。
- 建议：scene 提升到循环外并在首帧后复用。

#### AP-09 [P2] audioStateName / stateName 完全相同的 switch 两份拷贝

- 位置：`app/player/src/player_app.cpp:55-71` 与 `app/player/src/frame_diagnostics.cpp:19-35`
- 建议：合并为单一 helper。

#### AP-10 [P3] 显式清理链只在 happy path 执行

- 位置：`app/player/src/player_app.cpp:1003-1018`（backend.close → audioTransport.unload →
  audioStore.remove → playbackSession.unload；帧循环内错误直接 return 跳过）。RAII 兜底
  真实存在（`open_gl_backend.cpp:489-494` 析构调 release()），非 bug；建议收敛为单一
  cleanup 路径。

#### AP-11 [P3] smoke frame-4 reload 后 chartClock 仍持旧 timingOffsetMs

- 位置：`app/player/src/player_app.cpp:415` 与 `:829-841`（当前 reload 同一项目无影响，
  属潜伏不一致）

#### AP-12 [P3] 四个 option 的解析块为复制粘贴模式

- 位置：`app/player/src/player_app.cpp:121-178`（重复的 duplicate/missing 检查）；
  `--chart -x`（单破折号路径）被接受（仅拒绝 `--` 前缀 `:126-127`）
- 建议：提炼 requireValue() helper。

#### AP-13 [P3] 无 POSIX 信号处理（Ctrl+C 直接终止，不经 backend.close()/logger flush）

- 位置：`app/player/src/main.cpp`（dev CLI 可接受）

#### AP-14 [P2] 工具间重复代码，cxc_common 复用不充分

- 位置：`tools/cxc_pack/main.cpp:10-39` 与 `tools/cxc_unpack/main.cpp:10-39`
  （Arguments/parseArguments 逐行相同，仅 usage 字符串不同）；`tools/chart_validator/main.cpp:19-27`、
  `:36-58` 与 `tools/chart_migrator/main.cpp:28-36`、`:102-123`（printDiagnostics、
  readInput 近乎相同）；`tools/chart_migrator/main.cpp:78-100`、`:125-142` 重写了一整套
  更弱的 normalizedPath/samePath/临时文件/回滚（无 casefold、无原子 uniqueness、时间戳命
  名有并发窗口），而 `tools/cxc_common/src/cxc_tool_common.cpp:138-260` 已有更健壮实现
- 建议：泛化 tools_common，chart 工具接入（见第 8 节方向 8）。

#### AP-15 [P2] CLI 错误输出格式三套并存，IO 失败退出码不一致

- 位置：cxc 工具 `code fieldPath key=value: message`（`tools/cxc_common/src/cxc_tool_common.cpp:102-114`）；
  chart 工具 `code fieldPath: message`（`tools/chart_validator/main.cpp:19-27`）；
  asset_importer `error.code=/error.message=/error.context.*=`
  （`tools/asset_importer/main.cpp:240-246`）。IO 失败：cxc 工具→2
  （`tools/cxc_common/src/cxc_tool_internal.hpp:18-20`）vs asset_importer 读文件
  失败→1（`main.cpp:249-255`）。成功/内容非法/用法错误的 0/1/2 骨架一致
  （`cmake/VerifyChartTools.cmake:154-156` 亦验证）。
- 建议：统一 formatter 与退出码。

#### AP-16 [P2] check_docs.py 状态短语硬编码 100 条（82 必需片段 + 18 stale 片段）+ 过短片段误报风险 + 日期手改

- 位置：`tools/check_docs.py:167-323`（status_contracts/stale_fragments 作为 golden 文本
  锁，每次阶段推进需同步改约 10 个文件 + 本脚本）；`":289"`（`"F next"` 过短片段）；`:322`
  （`match.group(1) < "2026-08-24"` 每次 checkpoint 需手动改日期）
- 建议：状态短语 JSON 化（见第 8 节方向 9）。

#### AP-17 [P2] 文档门禁与版本只读校验不在任何 CI

- 位置：`.github/workflows/`（linux-quality/windows-msvc/windows-mingw）grep 无
  check_docs；`update_version.py --check` 同样不在 workflow。版本一致性实际由 configure
  硬门禁兜底（`cmake/CuexisVersion.cmake:73-85`），文档门禁完全没有 CI 兜底。其余覆盖良
  好：linux-quality 含 clang-tidy 与 coverage（>=40% 行覆盖硬门槛），工具退出码有 CMake
  脚本合同测试。
- 建议：加轻量 job（见第 8 节方向 9）。

#### AP-18 [P2] CMakePresets base 预设硬编码 VCPKG_TARGET_TRIPLET: x64-windows

- 位置：`CMakePresets.json:16`；Linux 全部预设需 CI 命令行覆盖
  （linux-quality.yml Configure 步骤 `-DVCPKG_TARGET_TRIPLET=x64-linux`），预设本身不跨
  平台
- 建议：拆 base（无 triplet）+ windows-base（有）。

#### AP-19 [P3] CUEXIS_PLAYBACK_EXPORT_TARGETS 是死变量

- 位置：根 `CMakeLists.txt:466-469`（全仓无消费点；真正用于 install 的是
  CUEXIS_PUBLIC_EXPORT_TARGETS/CUEXIS_STATIC_IMPLEMENTATION_TARGETS `:471-483`）
- 建议：删除。

#### AP-20 [P3] 依赖白名单约 380 行 verify 调用为重复模式；ACTIVE_TARGETS 加入顺序巧合

- 位置：根 `CMakeLists.txt:567-950`（已函数化、集中一处，可数据化）；`:407-409` 与
  `:440`（cuexis_format_check 加入晚于 warnings foreach，行为正确但顺序巧合）

#### AP-21 [P3] player_stage1c_assets 实际复制 stage1b/1c/1d/3 四个项目，名称误导

- 位置：`app/player/CMakeLists.txt:45-88`
- 建议：重命名 player_demo_assets。

#### AP-22 [P3] asset_importer main 无顶层 try/catch

- 位置：`tools/asset_importer/main.cpp:350-365`（player main 有 `main.cpp:51-66`；
  parseBinding 已局部 catch stoul `:122-134`，实际风险低，仅为一致性）

#### AP-23 [P3] tests/player 直接把 app/player 的 src 文件编进测试可执行文件

- 位置：`tests/player/CMakeLists.txt:3-6`（frame_diagnostics.cpp、snapshot_scene.cpp 非
  链接共享对象，与 cuexis_player 的编译选项/宏可能漂移，目前两文件不依赖 spdlog 才成立）；
  `CUEXIS_CXC_TOOL_TESTING` 模式（`tools/cxc_common/CMakeLists.txt` 末尾）展示了更好的
  条件编译管理方式

#### AP-24 [P3] 内联 chart JSON 字面量在多个测试文件重复

- 位置：`tests/chart/canonical_chart_loader_tests.cpp:20-43`（minimalChart）、
  `tests/player/frame_diagnostics_tests.cpp:66-107`（两份 chart）；`tests/fixtures/` 未
  覆盖这类最小 v1 图；`tests/cxc/cxc_test_support.cpp` 是良好范例

## 5. 架构约束核查结论

由 6 个子代理对全部模块逐条 grep + 读代码验证，**未发现任何真实违规**：

| 约束 | 核查结果 |
| --- | --- |
| Core 不含 SDL/glad/GL/平台头 | 通过 |
| Chart 不含 EnTT/SDL/glad/GL/World/Audio/AudioSDL 头 | 通过（`RuntimeEventTrack` 等仅是含 "entt" 子串的标识符误报） |
| Audio 不含 SDL/AudioSDL/平台 SDL 头 | 通过（audio allowlist = core，audio_sdl = audio+SDL3，与 MODULE_BOUNDARIES.md 一致） |
| Runtime 不含 SDL/glad/GL/Audio/AudioSDL/OpenGL adapter 头 | 通过（include 面为 animation/behavior/chart/core/render/world） |
| Playback 不含 SDL/AudioSDL/平台 SDL/OpenGL adapter 头 | 通过（entt 仅出现在 src/，符合 ADR 0027 内部实现定位） |
| nlohmann JSON 仅在 engine/json_support/ | 通过（chart/cxc 经 `cuexis::json` 公共包装访问，未触碰 DOM） |
| OpenGL/glad 仅在 engine/render_opengl/ | 通过 |
| GLM 不出现在公共头 | 通过（GLM 仅 engine/core/src/math.cpp 实现文件；公共头只用 core::Vec3/Quat） |
| 公共头纯 ASCII | 通过（对全部 engine/*/include 头逐字节扫描零命中；CJK 注释仅存在于 src/，符合规则） |
| 安装的 Playback 头不暴露 EnTT/SDL/GLAD/JSON DOM/日志/RuntimeSession/World | 通过 |
| Result 纪律 | 通过（`[[nodiscard]]`、显式 `static_cast<void>` discard、无裸 throw 跨边界；例外见 PB-08、CX-07 两个已列 finding） |
| 禁止跨模块裸 new/delete | 通过（全部 unique_ptr/shared_ptr；GL 句柄 presentation 侧有 Unique* RAII） |
| 依赖白名单与 CUEXIS_ACTIVE_TARGETS | 通过（project/assets/shader_cache/shader/cxc/playback 等的 allowlist 与各自 CMakeLists link 完全吻合） |
| 实时音频回调无分配/锁/throw | 通过（postmix 仅 atomic relaxed；publish/snapshot seqlock 正确） |

## 6. 文档-实现一致性正面确认

以下契约经逐字段/逐公式核对**一致**，记录为正面证据：

- CHART_V4 §10 预算表 15 项与 `limits.hpp` 一致；稳定诊断 14 个码全部存在；§3 identity
  二进制编码与 `chart_parameter_resolver.cpp:138-164`、`prepared_semantic_identity.cpp:58-86`
  逐字段一致；§6.2 lowering、§9 capability 推导、§11 迁移与实现一致。
- TIMING_MODEL 的 Hermite 公式、几何分段 `clamp(ceil(log2(maxBpm/minBpm)),1,16)`、16 点
  Gauss-Legendre、64 次二分、Stop 半开区间、4096/4096 预算全部吻合。
- CXC v1 严格 ZIP 子集（single-disk/Stored/无 flag/无 extra+comment/无 directory entry/
  local-central 逐字段一致/范围连续/trailing bytes/ZIP64 sentinel 拒绝/65534 上限）逐条
  落地（`zip32_envelope_internal.cpp:190-541`、`:543-659`）；canonical metadata 逐字节一
  致；manifest 六字段必需、未知拒绝、path 升序；闭包语义；`CxcPackageIdentity` = 精确
  bytes SHA-256；预算表与诊断码全部在冻结表内。
- PORTABLE_PRESENTATION §8 预算表与 `presentation.cpp:34-48` 常量逐项吻合
  （64 MiB/512 MiB/65536/1048576/3145728/8192/262144）；decodedByteCount 公式与实现精确
  一致；manifest canonical 排序由 `std::map` 迭代序保证并有 extraction 侧校验；
  FrameSnapshot 拥有型契约与 ADR 0030 一致（测试覆盖 reload/unload/销毁后读取）；
  FrameDigest v1-v3 保留历史定义并有 golden 测试。
- MATERIAL_SHADER：include 三重拒绝（预扫描 + shaderc include 回调）、opt 0/Vulkan 1.1/
  SPIR-V 1.3 固定、CuexisObject 反射逐成员校验、matchDeclaredBindings 位级比对；Asset
  Index v3 `shader`（先读显式 version、v1/v2 拒绝 shader、shader 叶节点、仅 Material 可
  依赖 Shader）与 § 3/ADR 0026 一致。
- ANIMATION_MIXING 求值层顺序 Initial→Behavior→Animation→HostOverride→
  StudioPreviewOverride→commit 与 `runtime_session.cpp:1014-1090` 完全一致；OverrideToken
  同 priority 冲突"写入被丢弃"与实现一致。
- Player 真实通过 PlaybackSession 驱动（prepareLoad/commit/update/extractFrame），无私有
  Runtime 路径，符合 ADR 0027；`--smoke-test` 6 帧 / `--audio-smoke-test` 90 帧 /
  `--frame-stats` 三个 artifact 与文档一致。
- 审查期间实测 `python -B tools/update_version.py --check` 与
  `python -B tools/check_docs.py` 均通过（170 文档 / 20 候选）。

## 7. 测试覆盖缺口

| 缺口 | 说明 |
| --- | --- |
| sha256 无系统化 known-answer 测试 | 仅空串/"abc" 两向量（`tests/cxc/zip32_envelope_tests.cpp:41-48`），无 NIST 多向量/跨块边界用例（CM-09） |
| audio_sdl 仅 3 个 TEST_CASE | 无 prepareReplacement/activateReplacement/cancelReplacement、无 underrun 路径测试（CM-21） |
| v4+legacy renderable 组合 | 全部测试无覆盖（PB-01） |
| lastOperationDiagnostics 异常路径 | 测试只覆盖写入的一半（PB-03） |
| prepareReload source 重载无 database 的 main-music 失败分支 | 无覆盖（PB-02） |
| 文档门禁与版本校验 | 不在任何 CI（AP-17） |
| tests/core 覆盖偏薄 | uuid_tests 19 行、result_tests 35 行 |

整体测试密度充足（约 23,500 行测试，结构镜像 engine/，catch_discover_tests 统一注册，
无 GPU/窗口依赖）；cxc 最密（zip32_envelope_tests 41 个 TEST_CASE）、animation 最充实
（mixer 817 行）、presentation 的 validation_sink 是 OpenGL buildDraws 语义的第二实现
（`tests/presentation/validation_sink.cpp:1423-1498`），两者一致性仅靠 GPU smoke 的
parity 检查（`player_app.cpp:909-918`）——每次改 buildDraws 必须同步两处 + digest 域。

## 8. 改进方向与优先级建议

### 方向 1：render_opengl 热路径优化（RT-26、RT-27、RT-28 现做；RT-29 推迟）

- 相关 finding：RT-26、RT-27、RT-28（前半，第 2 批）；RT-29（后半，等触发）
- 动机：集中在 `open_gl_presentation.cpp` 单文件的四个每帧热路径问题，是当前唯一会随内
  容规模放大而恶化的帧率风险；RT-27 属遗漏而非设计（纹理 location 早已在 prepare 期缓
  存，`:655-663`，数值 uniform 没有）。
- 做法：`GpuMesh` 缓存 bounds（prepare 期一次）；`GpuMaterial`/`GpuParameterizedProgram`
  增加 `numericParameterLocations`，prepare 期解析一次，`setNumericUniform` 改收
  location（保持 location=-1 跳过语义不变）；backend state 持久持有
  opaque/transparent/debugVertices scratch；`buildDraws` 增加 `needSummary`，summary 填
  充与 digest 移入 `if (summary)`；Player 的 debug 轴线 RenderScene 复用（AP-08）。
- 预期收益：常规播放路径零分配（与 RUNTIME_SESSION.md 对 update() 的承诺对齐）；每对象
  每帧省 2 次 std::string 构造 + 全量 FNV；消除每帧 driver 端字符串查找。
- 风险与迁移代价：location 缓存与 program 生命周期绑定；digest 语义保持"仅 summary 请求
  方可见"，validation/parity 测试不受影响；前半改动集中单文件、低风险。RT-29（不透明
  pass 按 (program, cull, texture) 状态分组排序）需同步 validation_sink 第二套排序实现
  （`tests/presentation/validation_sink.cpp:1423-1498`）与
  `cuexis.validation.summary.v1` digest 域，收益只在万级对象兑现，等触发。

### 方向 2：Chart 管线 parse-once（CH-05、CH-06、CX-10；第 3 批，随方向 4 合并）

- 相关 finding：CH-05、CH-06、CX-10
- 动机：v4 prepare 链路同一输入最多 6 次全量 JSON 解析、`ChartLoader::load` 双解析、cxc
  `isV4Chart` 再添一次；16 MiB 输入 prepare 解析成本约 6 倍。
- 做法：`ChartV4Loader::load` 返回值携带已解析 `json::Value`（或 internal 句柄）；
  `ChartV4Resolver::resolve` 增加接受已解析文档的重载；`ChartWriter::writeV4` 接受
  `json::Value` 而非重新 parse canonicalSource；`makeConcreteChart`/`makeLegacyProjection`
  消费同一 Value 的副本而非序列化-再解析；cxc 改用 `peekChartVersion(text, limits)`。
- 预期收益：大图 prepare 解析成本 6x → 约 1x，峰值内存同步下降；canonical bytes 不变则
  F3 指纹不变。
- 风险与迁移代价：动 3 个公共入口签名（加重载保兼容）；需复跑 F1-F4 确定性指纹确认逐
  字节一致；与方向 4 同管线同文件，合并实施边际成本最低，不独立排期。

### 方向 3：identity 与缓存键统一（CH-02、CX-02、CX-05、CX-23；第 1 批）★最佳方案

- 相关 finding：CH-02、CX-02、CX-05、CX-23
- 动机：唯一修复"功能静默失效"而非优化的方向。两条实证：importer 产出的缓存对 Player
  永远 miss（`asset_importer/main.cpp:325` standalone 键 + 硬编码 "main" entry vs
  `open_gl_presentation.cpp:546` semantic 键）；v1/v2/v3 identity 对 timing/模板/keyframe
  编辑无判别力，且 migrator（`chart_migrator.cpp:179`）与 playback
  （`playback_session.cpp:1416`）对同一 source identity 概念使用两种定义。identity 是本
  项目全部确定性保证的根，为 Stage 6+ Studio 铺路。
- 做法：`playback_session.cpp:1416` 改用 `ChartWriter::writeCanonicalJson(source)`；
  importer 默认解析 payload 的 CXPRES identity（或强制 `--identity` 必填），standalone
  哈希仅留显式 debug 模式；Player OpenGL 路径接入 `ShaderPipelineCache`（或将其移至
  cuexis_shader 待 Stage 6 接入）；修复 CX-23 半空 key 派生。
- 预期收益：消除"导入产物运行时不可用"与"identity 无判别力"两个用户可见断层；同一概念
  回归单一定义。
- 风险与迁移代价：identity 值变化 → 历史 golden/报告 SHA 一次性重生成并版本化说明，不
  能双轨；既有缓存目录整体失效（缓存可重建，无损失）。验证手段现成：仅 tempoEvents 不
  同的两个 v3 必须得到不同 identity；importer→Player 端到端缓存命中测试。约 3-5 天。
  不选其他方向作最佳方案的完整理由见"未入选方向的实施评估"。

### 方向 4：playback prepare 管线分层 + DiagnosticsRecorder + 错误码 taxonomy（PB-03、PB-14、PB-04、PB-15；第 3 批）

- 相关 finding：PB-03、PB-14、PB-04、PB-15
- 动机：prepare 338 行、12+ 退出路径、约半数失败路径不写 lastOperationDiagnostics 且异
  常路径留旧诊断；validatePresentation 约 285 行 14 组重复 if、parseShader 约 375 行 4
  组重复读取循环；规范码与实现码已互有漂移（PB-04）。
- 做法：prepare 拆为 loadDocument → resolveParameters → preflightCapabilities →
  compileAnimation → compileRuntime → acquireAudio → prepareRuntime →
  preparePresentation → commitFrame → buildLayout → assembleIdentity 的 stage 函数序列
  （签名 `(const PrepareContext&, PrepareArtifact&) -> core::Result<void>`）；RAII
  DiagnosticsRecorder 析构时无条件写入；错误码映射常量表（from → to + 是否保留 cause）
  + 静态测试解析 PORTABLE_PRESENTATION §12 与 MATERIAL_SHADER code 表断言每个 code 有
  emission 点（check_docs.py 已有挂载先例）；validatePresentation 表驱动
  （`CapabilityRule{flag, id, fieldPath}` + limit 规则表）；parseShader 抽
  `readCountedIdentifier` helper；`sortDeterministically` 调用点保持不变。
- 预期收益：diagnostics 全路径一致；prepare 降到 60 行内、单步可测；validatePresentation
  缩约 120 行、新增 capability 只改表；文档-实现码漂移 CI 即拦。
- 风险与迁移代价：F3 determinism 门禁对 diagnostics 顺序与 identity 指纹敏感，需逐测试
  比对；错误码是已冻结公共面，映射表只能加不能改义；diagnostics 文案/fieldPath 需与
  presentation_tests 与 external consumer 指纹逐字对齐。1-2 天 + 全量 CTest；必须排在方
  向 3 之后（identity 语义定案再重构计算它的管线，避免双重重构）。

### 方向 5：统一插值数学库进 core::math（CM-04、CM-17、RT-16；第 1 批）

- 相关 finding：CM-04、CM-17、RT-16；顺带 CM-05、CM-06、CM-09
- 动机：hermite/slerp/lerp 四份拷贝且 slerp 已现写法分叉（branch-on-dot vs
  alignHemisphere helper）——behavior 与 animation 对同一输入的 1 ULP 差异即威胁确定
  性主张（F3 门禁要么误拦、要么放行语义分叉）；`core::normalize` 溢出洞静默产出零四元
  数且返回成功。
- 做法：纯 `Quat/Vec3/double` 级插值函数收进 `cuexis::core::math`（GLM 仍锁在 math.cpp；
  behavior/animation 均已依赖 core，白名单零改动）；补 normalize 的
  `!isFinite(lengthSquared)` 分支返回错误；sha256 补 NIST 186 向量 + 跨块边界用例
  （空串与 "abc" 已有）；
  uuid 大小写、inverse 阈值、nearlyEqual 容差契约头注释顺手补齐。
- 预期收益：消漂移、修一个静默正确性缺陷、包身份链获得 known-answer 保护；手写四元数构
  造集中并注明 yaw/pitch/roll 复合顺序。
- 风险与迁移代价：低；纯函数迁移，behavior/animation 现有 3 组测试互为回归网。1-2 天。

### 方向 6：CXC/JSON 解析与校验降本（CX-12、CM-11；等触发）

- 相关 finding：CX-12、CM-11；可并入 cxc 零拷贝视图（`cxc_package.cpp:751-754` readBlob
  复制 vs `:794-801` entryBytes 已有零拷贝 span）
- 动机：加载每字节约 4 遍 + 每 entry 64 MiB 峰值临时分配；写入约 6 遍；json 双 DOM 约
  3 份瞬时内存。当前无用户痛点，且校验门是 CFU-F 定下的 owner 级决策。
- 做法（触发后按需组合）：a) `CxcPackageLimits` 增加 `independentArchiveCrossCheck`（默
  认 true 保 CFU-F 门禁语义；pack/validate/CI 保持 true，Playback 加载路径 false）；
  b) minizip 比对改流式（读一段比一段），不丢独立校验、只消大临时分配——优先变体；
  c) json parse 以 nlohmann SAX 直构 `json::Value`（duplicate-key/depth/string-limit 检
  查内联进 handler），serialize 直接从 Value 写；d) 包内资源改 span/shared-ownership
  变体（触碰 SDK 布局承诺，需 ADR）。
- 预期收益：大包加载约 2x 提速、峰值内存省约 64 MiB/entry；大 Chart 解析内存/耗时约减
  半。
- 风险与迁移代价：a) 削弱一层防 minizip-ng 自身 bug 的独立冗余；c) parse.cpp 重写约
  250 行，需保住全部 parse_tests/schema 用例与错误码逐字对齐。触发条件：真实大包
  （>100 MiB）性能投诉或 Studio 大内容场景。

### 方向 7：world/animation 每帧路径优化（RT-17/18/19/20、CM-25、RT-10、RT-11、RT-02 代码侧；等触发）

- 相关 finding：RT-17、RT-18、RT-19、RT-20、CM-25、RT-10、RT-11、RT-02（代码侧）
- 动机：markTouched O(W×T) 是大谱面下 runtime 侧第一瓶颈；Entry 约 2.7KB/实体（1 万实
  体约 27MB）且 cache 不友好；AnimationMixer 每帧重建 string-keyed map 与排序，而 layer
  优先级/mask 编译后不可变。
- 做法：Entry 增加 `lastTouchEpoch`（uint64 防回绕），Resolver 持 `currentEpoch`，
  beginFrame 递增，markTouched O(1)；ensureEntry 先攒后一次排序；applyOverrides 与
  override vector 改 scratch 复用；`AnimationCompiler::compile` 预计算每 object 有序
  layer 索引、blend-group property 分组、mask 位集，evaluate 改索引寻址 + scratch；
  animationBaselines prepare 期建骨架、帧内 in-place 更新（bindings 在 prepare 固定的
  前提当前成立）。
- 预期收益：每帧行为写从二次方降线性；动画会话每帧 N×M 次值构造降为标量赋值；兑现
  "update 不分配"承诺（代码侧）。
- 风险与迁移代价：混合语义须逐用例保真（817 行 mixer 测试回归）；建议用
  animation_performance_probe.cpp 基建做前后对比；5-8 天。触发条件：内容规模达数千实
  体以上，或 Studio 进入实施；RT-02 的文档侧修复已提前到第 0 批。

### 方向 8：重复代码大扫除（不立项，选择性搭车）

- 相关 finding：CH-14、CX-14、CX-17、CX-19、CX-21、AP-09、AP-12、AP-14、AP-15、AP-24、
  RT-13、RT-21、RT-31
- 动机：约 1500+ 行可收敛的重复；其中 CX-14（path 校验宽严不一，assets 层可绕过
  ADR 0025）与 AP-14（migrator 临时文件并发窗口）不是纯卫生而是真实弱点。
- 做法：按搭车原则逐模块做——碰到 resource_manager 顺手模板化 request（CX-17，删约
  350 行，同文件 `requestDirect<Tag>` 是现成样板）；path 校验收敛 portable_path 单实
  现（CX-14）；tools 泛化 tools_common（AP-14/15，统一 CLI formatter 与 IO 退出码）；
  player 选项解析提炼 requireValue（AP-12）、stateName 合并（AP-09）；其余（CH-14、
  CX-19、CX-21、AP-24、RT-13、RT-21、RT-31）随邻近改动顺带。
- 预期收益：净删约 1500+ 行；三层路径规则永久一致；CLI 合同对脚本调用方稳定。
- 风险与迁移代价：诊断文本/路径需与 golden 逐字对齐；migrator stderr 措辞变更需同步
  VerifyChartTools.cmake 文本断言；纯内部重构，无公共 API 承诺。

### 方向 9：文档事实数据化 + 门禁入 CI（AP-16、AP-17；第 1 批）

- 相关 finding：AP-16、AP-17；AP-01/02/03/04 修正后成为被持续校验的对象
- 动机：13 处文档漂移是"手工同步多份文档"机制的必然产物，机制仍在运转就会再产出下一
  批；check_docs.py 与 update_version.py --check 不在任何 CI，文档门禁全靠自觉。
- 做法：根 CMakeLists 以 `file(GENERATE)` 导出 `CUEXIS_ACTIVE_TARGETS` 到
  `generated/cuexis-targets.txt`；check_docs.py 新增子检查——解析 BUILDING.md 固定代码
  块内的 target 清单与 find_package 示例版本，与生成文件及 `CUEXIS_SDK_API_VERSION` 比
  对；status_contracts/stale_fragments 收敛为 `docs/status_contract.json`（schema 化、
  日期字段单点维护、stale 片段最短长度阈值防误报）；两脚本纳入 linux-quality 各一个
  step。
- 预期收益：文档与构建事实强一致；版本升级/新 target 时自动报警而非靠人眼。
- 风险与迁移代价：BUILDING.md 解析需约定固定代码块标记；约 60 行 Python + 2 行 CMake +
  1 个 CI step。

### 方向 10：音频时钟契约硬化（CM-21、CM-22、CM-23、CM-24；第 2 批）

- 相关 finding：CM-21、CM-22、CM-23、CM-24；CM-03（第 0 批先做契约声明，本方向做
  seqlock 完整修复）；第 7 节 audio_sdl 测试缺口
- 动机：HostClock 作为公共 SDK 时钟无同步原语（跨线程即 data race）；
  effectiveSettings 裸读非 atomic 成员；activateReplacement 死回滚误导维护者且
  `SDL_PauseAudioStreamDevice(nullptr)` 靠容错不崩；audio_sdl 仅 3 个 TEST_CASE，无
  replacement/underrun 覆盖。
- 做法：HostClock 复用 SdlAudioTransport 的 seqlock 模式（若 CM-03 已文档化
  owner-thread-only 可降级为备忘）；`effective` 并入 PublishedClock seqlock 发布；删除
  activateReplacement 回滚段并固化"替换失败 → Error 态，需 unload 恢复"契约；
  updatePresentedFrame 的 clamp 改显式 min/max 防御；补 prepareReplacement/
  activateReplacement/cancelReplacement/underrun 测试与跨线程 snapshot 压测。
- 预期收益：SDK 宿主可安全跨线程取时钟；消除两个潜在 data race；删除误导死代码；音频
  测试缺口闭合。
- 风险与迁移代价：低；均为局部改动，2-3 天。

### 方向 11：Chart 参数化字段 typed 化（可选；等触发）

- 相关位置：`engine/chart/src/chart_v4_loader.cpp:299-376`
  （expectedParameterType/neutralParameterValue/scanParameterReferences 字符串路径嗅探）
  与 `engine/chart/src/chart_v4_resolver.cpp:82-148`（replaceAtFieldPath 回填）
- 动机：参数化用途白名单由魔法串两端约定维护，无编译期保证；新增可参数化字段需同改两
  处；"替换失败才报错"的滞后诊断。
- 做法：transform position/scale 轴与 camera.fovY 改为与 Animator 一致的显式
  `NumberSource = variant<double, ParameterReference>`（类型已存在，
  `chart_document.hpp:28`），Reader 直接产出、Resolver 直接 byId 求值，删除三函数与
  replaceAtFieldPath。
- 预期收益：删约 200 行字符串寻址代码；参数化白名单变成类型系统事实。
- 风险与迁移代价：中；改 ChartV4SourceDocument 形状（identity 基于 source bytes，不受
  影响），loader/resolver/测试夹具需同步。触发条件：Stage 6 参数化扩展时一次完成。

### 未入选方向的实施评估

方向 3 之外的方向并非同质：两项应立即实施（与方向 3 互不阻塞、可并行）、两项作为第二波
按序实施、一项拆成两半、两项设触发条件后实施、方向 8 不立项仅选择性搭车。补充评估：方
向 10（音频时钟契约硬化）建议实施，安排在第 2 批（CM-03 快赢先行覆盖契约声明）——它修
的是潜在 data race 与误导性死代码、闭合 audio_sdl 测试缺口，成本低且不依赖任何触发条
件；方向 11（Chart 参数化 typed 化）等触发（Stage 6 参数化扩展时）。方向体系之外的独立
小项：RT-01（reload 事务语义，P1）随第 1 批，PB-12（资源索引透明比较器）随第 2 批。

**应立即实施（可与方向 3 并行）：**

- 方向 5：唯一"不做会持续积累正确性风险"的方向。两个独立理由：CM-04 是真实缺陷（大数
  量级四元数静默归零且返回成功，behavior/animation 的 slerp 全依赖它）；CM-17 的 slerp
  写法已分叉（branch-on-dot vs alignHemisphere），behavior 与 animation 若对同一输入产
  生 1 ULP 差异即威胁确定性主张——F3 指纹门禁要么误拦、要么放行语义分叉。成本 1-2 天、
  白名单零改动、817 行 mixer 测试是最强回归网。未当选仅因体量小，非不值得。
- 方向 9：13 处文档漂移是"手工同步多份文档"机制的必然产物，机制仍在运转就会再产出下一
  批；check_docs.py 不在任何 workflow（AP-17），文档门禁目前全靠自觉。约 1 天（60 行
  Python + 1 个 CI step），顺手消掉 AP-16 的日期手改。

**第二波（必须排在方向 3 之后）：**

- 方向 4：CH-02 改变 prepare 计算什么、方向 4 重构怎么计算，顺序反了 identity 调用点要
  重构两次。自身价值：PB-03 是宿主可感知的契约不一致（异常路径留旧诊断）；338 行 12 退
  出路径的 prepare 已到"每加一个 Stage 6 功能都会更糟"的临界点；F3 指纹敏感有现成门禁
  兜底。
- 方向 2：随方向 4 合并实施，不独立排期。单独立项证不足（prepare 延迟在当前内容规模下
  无感），但同管线同文件，合并后边际成本骤降，16 MiB 图解析 6x → 约 1x 属搭车收益。

**拆开实施（方向 1）：**

- 现在做：RT-27（数值 uniform location prepare 期缓存——每帧每 draw 的字符串级
  glGetUniformLocation 是 driver 层大忌，纹理 location 早已缓存，属遗漏而非设计）、
  RT-26（bounds 缓存）、RT-28（帧 scratch 复用 + summary/digest 按需构建）。三项均为
  单文件小 diff、相互独立、不动契约面，当前内容规模也有可测收益。
- 推迟：RT-29（不透明 pass 状态排序）——需同步 validation_sink 第二套排序实现与
  `cuexis.validation.summary.v1` digest 域，回归面是四项中最大，收益只在万级对象兑现。

**设触发条件，现在不动：**

- 方向 7：触发器 = 内容规模达数千实体以上，或 Studio 进入实施。例外：RT-02 的文档侧修
  复（把 RUNTIME_SESSION.md 的"不分配"承诺限定为无 Animation 会话）现在就做，一行文档
  即关闭该 P1 契约违约；baseline 持久化归入方向 7 等触发。
- 方向 6：触发器 = 出现真实大包（>100 MiB）的性能投诉。ZIP 每字节 4 遍校验是 CFU-F 定
  下的双保险安全门，削弱它需要 owner 决策而非工程决策；JSON SAX 重写约 250 行换全部错
  误码逐字对齐，当前无用户痛点。真要做时选流式比对变体（不丢独立校验、只消每 entry
  64 MiB 临时分配），不选关开关的变体。

**方向 8 不立项，三项搭车：**

- CX-17：删约 350 行复制粘贴、同文件 `requestDirect<Tag>` 是现成样板、测试兜底，任何
  时候碰到该文件都应顺手做。
- CX-14：非纯卫生——assets 层接受 `*`、`"` 等严格层拒绝的字符，是校验面不一致，直接
  构造 AssetDatabaseInput 的调用方可绕过 ADR 0025。
- AP-14/15：chart_migrator 的临时文件方案有时间戳命名并发窗口，是真实弱点而非风格问
  题；CLI 三套错误格式并存影响脚本调用方。
- AP-07 推迟到需要新增 smoke 场景时（S6）：改完正好被新场景立刻验证。

### 建议执行顺序

1. 第 0 批（约半天）：6 项快赢（CH-01 P0 补诊断、AP-01/02 版本示例 0.6→0.7、CM-01 与
   AP-06 AGENTS.md animation 段修正并删 tests/particles 空目录、PB-08 五方法 try/catch、
   AP-19 删死变量、CM-03 HostClock 线程契约）+ RT-02 文档侧修复（RUNTIME_SESSION.md 承
   诺限定范围）。紧随的第 0.5 批为纯文档/注释修正，清单见"全量 finding 处置归属"。
2. 第 1 批（三线可并行 + 一项独立小修）：方向 3（identity 与缓存键统一）‖ 方向 5（统
   一插值数学）‖ 方向 9（文档事实化 + 入 CI）；RT-01（reload 事务语义，独立 P1）。
3. 第 2 批：方向 1 前半（RT-26 bounds 缓存、RT-27 uniform location 缓存、RT-28
   scratch/按需 summary）+ 方向 10（音频时钟契约硬化）+ PB-12（资源索引透明比较器）。
4. 第 3 批：方向 4（prepare 管线分层）与方向 2（parse-once）合并执行。
5. 搭车随时：CX-17（ResourceScope 模板化）、CX-14（portable path 三合一）、AP-14/15
   （tools_common 泛化）等，完整清单见"全量 finding 处置归属"。
6. 等触发：RT-29（万级对象）、方向 7（数千实体或 Studio）、方向 6（>100 MiB 大包）、
   方向 11（Stage 6 参数化扩展）、AP-07（新增 smoke 场景需求）。

### 全量 finding 处置归属

下表把 144 项 finding 全部映射到处置路径，保证无遗漏；"接受现状"表示审查时已判定无需
行动、仅作记录。批次定义见"建议执行顺序"，方向定义见上文。

| 处置路径 | finding |
| --- | --- |
| 第 0 批（快赢，行为安全） | CH-01、AP-01、AP-02、CM-01、AP-06、PB-08、AP-19、CM-03、RT-02（文档侧） |
| 第 0.5 批（纯文档/注释/契约声明，行为安全） | CH-03（文档注明路径）、CH-04、CH-12、PB-05、PB-06、PB-10、RT-05、RT-06、RT-08、RT-25、RT-33、RT-34、CM-02、CM-05、CM-08、CM-10、CM-13、CM-14、CM-20、CM-24（注释）、CM-27、CX-03、CX-04、CX-06、CX-16、CX-26、AP-03、AP-04、AP-05 |
| 第 1 批 | 方向 3：CH-02、CX-02、CX-05、CX-23；方向 5：CM-04、CM-17、RT-16、CM-09、CM-06；方向 9：AP-16、AP-17；独立小修：RT-01 |
| 第 2 批 | 方向 1 前半：RT-26、RT-27、RT-28；方向 10：CM-21、CM-22、CM-23、CM-24（代码侧）；独立小项：PB-12 |
| 第 3 批 | 方向 4：PB-03、PB-14、PB-04、PB-15；方向 2：CH-05、CH-06、CX-10 |
| 搭车（随邻近改动，含方向 8 清单） | CH-07、CH-08、CH-09、CH-10、CH-11、CH-13、CH-14、CH-15、CH-16、PB-02、PB-07、PB-09、PB-11、PB-13、PB-16、CX-07、CX-08、CX-09、CX-11、CX-13、CX-14、CX-17、CX-18、CX-19、CX-20、CX-21、CX-22、CX-24、CX-25、CX-27、RT-03、RT-07、RT-09、RT-12、RT-13、RT-15、RT-21、RT-22、RT-23、RT-24、RT-30、RT-31、RT-32、CM-07、CM-12、CM-15、CM-16、CM-18、CM-26、AP-08、AP-09、AP-12、AP-14、AP-15、AP-18、AP-21、AP-22、AP-23、AP-24 |
| 等触发 | 方向 1 后半：RT-29（万级对象状态排序）；方向 7：RT-02（代码侧）、RT-10、RT-11、RT-17、RT-18、RT-19、RT-20、CM-25；方向 6：CX-12、CM-11；方向 11：chart 参数化 typed 化（无独立编号）；player：AP-07、AP-11 |
| 需 owner/spec 决策 | PB-01（v4+legacy 限制冻结 vs 对齐 v3 语义）、CX-01（extensions 补记 vs 拒绝）、PB-04（value_invalid 删码 vs 注明边界；随方向 4 taxonomy 落地）、CH-03（可选的统一 BPM 域路径，涉及行为变更）、RT-04（删除 renderFrame vs 标注 legacy） |
| 接受现状（记录即可） | CM-19、CX-15、RT-14、AP-10、AP-13、AP-20 |

## 9. 审查后的准确状态

- 本审查为只读快照，未修改任何产品代码；144 项 finding 均未整改（处置路径归属见第 8 节
  "全量 finding 处置归属"），除"接受现状"与"需决策"类外均待实施。
- 架构硬约束（第 5 节）在审查 HEAD `d380fc9` 全部核查通过。
- 2026-08-29 核验记录：对全部 144 项 finding 完成逐条只读复核（对照同一 HEAD `d380fc9`，
  6 个核验子代理分 3 批），**144 项实质成立、0 项不成立**。其中 18 项判为部分证实并已按
  复核结果修正细节定性（CH-08/09/14/16、PB-04/10、CX-02/04/09/10/18/23、RT-14/16、
  CM-04/09/14/16）；另 19 处为行号/路径/计数勘误，对应项结论不变（CH-02/05/15、
  PB-01/06/14/15、CX-01/16/25、RT-05/13/22、CM-17/22/27、AP-04/07/15/16）。
- 本报告不改写 [CURRENT_STATUS.md](../CURRENT_STATUS.md) 的阶段状态；Stage 5 仍为
  S5-A..G complete、S5-H local checkpoint、hosted pending。
- 修复任一 finding 后应在本目录新增带日期的整改/复审报告，并以链接方式回指本文对应
  编号。
