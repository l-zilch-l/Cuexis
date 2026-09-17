# Chart Format Foundation Hardening R4：端到端、容量与回滚

日期：2026-09-17

状态：R4 完成；`encode -> decode -> 完整语义比较 -> re-encode` 在 40,000 实体 profile、CXT 阶梯和真实 CXC candidate 包上闭环

起始 SHA：`4ebf244`（R3 提交，`codex/chart-format-foundation-hardening`，起始工作区干净）

上游依据：[加固计划](../../../stage_plans/active/chart-format-foundation-hardening/plan.md) 第 4 节 R4、
[R3 报告](2026-09-17-r3-budgets-and-arithmetic.md)、[R2 报告](2026-09-17-r2-profile-rejection.md)、
[R1 报告](2026-09-17-r1-semantic-identity.md)、[R0 报告](2026-09-16-r0-baseline-and-reproduction.md)、
[F8 容量 harness](2026-09-07-f8-capacity-harness.md)、
[Packed Chart Spec](../../../formats/PACKED_CHART_FORMAT.md) 第 3、5、6、7、9、10、11 节。

R0-H03 记录的缺口（容量用例只调用 `size()`、没有 decode 与语义比较）在本轮关闭：所有端到端
用例都做完整 typed 模型比较，并再用 re-encode bytes 与 semanticIdentity 双重确认规范性。
本轮不改变 wire revision、Header 布局或 semanticIdentity 算法。

## 1. 修复与新发现

先复现后修复。R4 的完整语义比较直接暴露了四个静默数据丢失/次序缺陷，其中两个是 R4 新发现的：

| ID | 缺陷 | 复现方式 | 处理 |
| --- | --- | --- | --- |
| A10 | `writeComponentStream` 的 CAM0 差异只比较 `type`/`fovY`，`near`/`far` 被静默退回 Archetype 默认值 | 两个共享 camera mask 的实体只差 `nearPlane`，旧 Writer 写出的差异 mask 为 0 | 按 Spec 7.3 的 field index 逐字段比较与写出（bits 0..3），Reader 允许 mask `0x0f` 并逐字段应用 |
| A11 | Archetype 默认值取"模型中最后一个完整值"，与 Spec 7.1 的"出现最多、同频按 bytes 决胜"不符，且使规范 bytes 依赖模型次序 | 3 个同 mask 实体：`x=5,5,7` 与反转次序的同一 chart 产生不同 bytes | `DefaultPicker`：按完整 Component 值计数取众数，同频按规范字段 bytes 升序；模型次序不再影响产物 |
| A12 | TIME 的 tempo/stop 按模型次序写出，Reader 不校验 Beat 升序 | 逆序 tempo/stop 模型产生非规范 bytes；手工构造的降序 TIME 被接受 | Writer 按 `startBeat`/`beat` 升序写出；Reader 校验严格升序（`packed.time.order`） |
| A17（新） | `decode` 用 `{"candidate.lanes4", id}` 重建 `judgementDomain`，把 id 写进 `domain` 槽；wire 不保存类别文本，于是往返后 `domain` 被改写 | 完整语义比较在 40k 与组合 fixture 上失败 | Reader 还原类别文本 `{"judgement-domain", id}`；预映像拒绝其他类别文本（`packed.identity.reference_domain`） |
| A18（新） | `writeComponentStream` 的组件查找对每个不匹配组件写回 `nullptr`，因此只有"实体 components 数组里最后一个该类型组件"才会被比较：多组件实体的 transform 差异行被静默丢弃 | 同时含 Transform+Renderable 的实体：TRN0 行的 mask 为 0，decode 后位置退回默认值 → `packed.identity.mismatch` | 三个指针各自只在类型匹配时赋值；差异比较与组件数组位置无关 |

另有两条 canonical 口径在本轮冻结（决策 D9/D10）：

- **D9 resourceClosure 是派生集合**：mainMusic 一条 `MainMusic`，每个 Renderable 的
  mesh/material 各一条，相同 (assetId, use) 去重。Writer 拒绝与之不一致的声明
  （`packed.identity.closure`），Decoder 用同一集合补全模型，因此
  `decode(encode(model))` 的闭包不再被静默丢弃。
- **D10 默认相机只承载 position**：`defaultTransform` 的 rotation 必须是单位四元数、scale
  必须是 `(1,1,1)`，否则报 `packed.identity.camera_transform`（META 只写 position，静默压平
  会丢失语义）。

### 1.1 记录但未在实现层闭合的缺口（A16）

CXT v2 展开结果不声明 Packed profile 需要的 capability feature，也不派生 resource
closure（`cxt_v2_loader.cpp` 只设置 `chartId` 与 `entities`）。因此
`expand -> Packed` 必须由调用方显式补齐 `CanonicalFeature{"cuexis.gameplay.candidate.lanes4", 1}`
（本轮 E2E 用例显式执行并注释）。这不是 R4 可以单方面决定的事情：来源格式目前没有
capability/feature 字段，Stage 6 需要决定能力声明的来源（CXT 字段、项目文档或 lowering
规则）。R4 记录该交接项，不假设已解决。

## 2. 完整语义比较（R4.1/R4.3）

`tests/chart/packed_roundtrip_tests.cpp` 用逐字段比较替代计数与 `size()`：
`chartId`、`mainMusic`、`features`、`timing`（offset/bpm/tempo/stops 每个字段）、
`defaultCamera`（type/fov/near/far/pitch/yaw/roll/defaultTransform 的 position/rotation/scale）、
`resourceClosure`、以及 `entities`（identity/parent/components/requirements 已有 defaulted
比较）。CanonicalSemanticChart 本身没有 `operator==`，ChartTiming/CameraData/TransformData/
TempoEvent/TimingStop 也没有，因此比较写在测试侧而不是改动模型头。

每个正例同时断言：decode 后语义相等、re-encode bytes 完全相等、semanticIdentity hex 相等。

| 用例 | 覆盖 |
| --- | --- |
| a component chart round-trips encode, decode, semantics and re-encode | Transform+Renderable+Camera 三实体（2 个相同 + 1 个全字段不同）完整往返 |
| single-field camera differences survive the CAM0 delta | near-only / far-only / fov-only / 三字段同时，并断言差异 mask 为 4 / 0x0e |
| archetype defaults use the most frequent value and ignore the model order | 众数胜出、ties 按 bytes（`1.0F` 胜过 `-1.0F`）、模型次序无关 |
| TIME rows are canonical ascending and out-of-order rows are refused | Writer 排序 + 手工降序 TIME 报 `packed.time.order` 且不是 identity mismatch |
| the resource closure is derived from the asset references | 一致声明往返、空闭包/多余资源/错误 use kind 被拒、mainMusic 计入派生集合 |
| the registered default camera carries a position only | position-only 往返、rotation/scale 被拒 |
| the 40000 entity profile round-trips with complete semantics | 40,000 实体完整比较 + re-encode bytes + identity + 抽样字段 |
| the CXT stair ladder expands and round-trips through Packed | 展开 16 实体阶梯，比较 identity path/parent/transform/Beat/lane/closure，并与 reordered 源产生相同 bytes |

## 3. CXT 阶梯（R4.2）

输入是既有 fixture `tests/fixtures/chart_format_foundation/valid/pattern_stair.cxt`（
`pattern.stair` / export `stair` / 参数 `groups=4`），参数显式冻结为字面值
（`CxtV2Invocation::parameters`/`slotBindings`）。用例比较 16 个实体的生成 identity
（bindingId/moduleId/exportId/path 的 nodeId 与 repeat index）、parent、Transform
position、requirement 的 Beat 与 lane，并验证 `pattern_stair_reordered.cxt`（数组重排、
同一语义）产生逐字节相同的 Packed 产物。

## 4. 真实 CXC candidate 包（R4.4）

`tests/cxc/cxc_candidate_roundtrip_tests.cpp` 用产品 Writer 生成包、用产品 Loader 载入、
再用 `cuexis::tools::validateCandidateChartExtension` 验证，Packed entry 由
`packed::encode` 产生（不解析示例 JSON）：

| 场景 | 构造 | 期望 |
| --- | --- | --- |
| 成功 | extension 声明 `playback=true` + 真实 artifactIdentity/compiled identity + 与 `inspect` 一致的计数 | 无错误；包内 entry bytes 等于编码产物；Packed entry 不在 project documents 中 |
| 缺 entry | 只声明路径、不加入 archive entry | `cxc.candidate.entry_missing` |
| 计数不匹配 | `expandedEntityCount = entities + 1` | `cxc.candidate.count_mismatch`，且不是 identity mismatch |
| Packed 损坏 | 翻转 section payload 最后一个字节且不修 CRC | `cxc.candidate.packed_invalid`，且不是 artifact identity mismatch |
| 预算失败 | 16 MiB + 1 的 entry（预算门禁先于解析） | `cxc.budget.exceeded` |
| source 不能充当 playback entry | 声明非 playback、指向真实 project chart 路径的 entry，包内没有 playback=true | `cxc.candidate.playback_missing`，且不是 packed_invalid |
| 未登记 kind/encoding | `encoding=source-cxt` 或 `kind=cxt` | `cxc.candidate.entry_unsupported` |

## 5. 原子写与回滚（R4.5）

`tests/chart/packed_atomic_write_tests.cpp` 每个用例使用独立临时目录：

| 场景 | 断言 |
| --- | --- |
| 成功替换 | 目标 bytes 等于新产物、目录内无 `.tmp.` 残留 |
| profile / 预算 / canonical 前置失败 | 旧 artifact 逐字节不变、无临时文件 |
| 替换失败（目标位置是不可被文件替换的目录） | `packed.io.replace_failed`、临时文件被清理、目标目录仍在、同目录外部文件（含一个名字像临时文件的非本次文件）保持不变 |
| 输出父目录不存在 | `packed.io.parent_missing`，且不创建任何文件 |

## 6. 容量数据（R4.6/R4.7）

新增可执行 `cuexis_chart_capacity_probe` 与 `cmake/VerifyChartCapacity.cmake`：与既有
`*_performance_probe` 一样受 `CUEXIS_RUN_PERFORMANCE_PROBE` 门禁控制，校验 40 位十六进制
实现 SHA，并把 JSON 写到 `${CMAKE_BINARY_DIR}/chart-capacity/capacity.json`。机器可读数据
随本报告保存为 [2026-09-17-r4-capacity-data.json](2026-09-17-r4-capacity-data.json)。

人类摘要（Windows x64 / MSVC 19.51 / Debug / `candidateRevision=1,flags=1`）：

| Profile | 输入 | 实体/要求 | Packed bytes | decoded bytes | IDN0 bytes | 最大 section | encode/decode |
| --- | --- | --- | --- | --- | --- | --- | --- |
| low-reuse-v1（闭关闭包 profile） | 直接构造 typed model（source bytes 不适用） | 40000 / 40000 | **1,232,408** | 1,232,024 | 680,012 | REQ0 471,746 | 4.33 s / 3.38 s |
| cxt-stair-ladder | CXT v2 源 5,122 bytes | 16 / 16 | 1,290 | 874 | 115 | STR0 199 | 2.9 ms / 1.7 ms |
| high-reuse-v1（可选观测） | 同源 `groups=10000` | 40000 / 40000 | 1,247,446 | 1,247,030 | 199,543 | TRN0 503,488 | 7.80 s / 5.53 s |

三个 profile 的 `status` 都是 `ok`，即 `size`/`encode`/`decode`/`re-encode` 全部成功且
re-encode 字节等于首次产物。40k profile 的 peak delta 为 50.6 MB（low-reuse）与 98.0 MB
（high-reuse）。

方法学（JSON 内也带同样字段）：

- 峰值来自进程级 working set：Windows `GetProcessMemoryInfo().PeakWorkingSetSize`，POSIX
  `getrusage().ru_maxrss`；包含 allocator、page commitment 与平台差异，只用于暴露数量级与
  回归趋势，确定性门禁仍由 §3.3 的字节/计数硬预算承担。
- 耗时用 `std::chrono::steady_clock`，每个 profile 每个操作测一次，未做预热重复。
- `notMeasured` 明确记录：`preparePeakBytes`（本阶段没有 prepare 实现）、typed-model profile
  的 source bytes（直接构造，不虚构 JSON 大小）、decoded bytes 当作堆占用、IDN0 scope/path
  表的对象内存。
- 可执行文件的 `implementationSha` 来自运行时 `git rev-parse HEAD`（本批次运行时为
  `4ebf244`，当时 R4 改动尚未提交）；最终关闭 SHA 的同 SHA 复跑由 R5 记录。

R3 遗留的观测项：`decode` 的父图环路检测仍是每实体一次 `visited` 分配的 O(n²) 遍历。
本轮测得 40,000 实体的完整 decode 为 3.38 s（Debug、本机），未改算法，继续作为观测项留给
R5/Stage 6 决定。

## 7. 新增/迁移诊断

| 诊断 | 触发 |
| --- | --- |
| `packed.time.order` | TIME tempo/stop 不是严格升序或存在重复 Beat |
| `packed.identity.closure` | 声明的 resourceClosure 不等于派生集合 |
| `packed.identity.camera_transform` | 默认相机 transform 的 rotation/scale 非单位 |
| `packed.identity.reference_domain` | TypedReference 的类别文本不是 judgement-domain / action |

## 8. 兼容性影响

- **由接受变拒绝**：非规范 TIME 次序；与派生集合不一致的 resourceClosure；默认相机带
  rotation/scale；TypedReference 类别文本不符；CAM0 mask 之前只允许 `0x01`，现在接受
  `0x0f`（由拒绝变接受，方向是补齐 Spec 7.3 的 field index 规则）。
- **Writer 字节变化**：TIME 表按 Beat 排序；Archetype 默认值改为众数；多组件实体的差异行
  从"被丢弃"变为按 Spec 写出；`judgementDomain` 类别文本一致化。语义身份只在修复了
  真实数据丢失的地方改变；合法单组件/单实体 fixture 的 Spec 10.2 golden 继续通过。
- **无 wire/revision 变化**：仍为 `flags=1`、`candidateRevision=1`；无新增字段或 section。
- **测试夹具修正**：三个 F 时代 fixture 使用 `{"candidate.lanes4", "candidate.lanes4"}`，
  与 CXT 源和 R2/R3 fixture 的 `{"judgement-domain", "candidate.lanes4"}` 不一致；已统一为
  后者（profile 规则只读 `.id`，两种写法在拒绝行为上等价，但往返语义不同）。
- **新增构建目标**：`cuexis_chart_capacity_probe` 已登记进 `CUEXIS_ACTIVE_TARGETS`、依赖
  allowlist 与 `BUILDING.md` 目标清单。

## 9. 改动文件

- 新增：`tests/chart/packed_roundtrip_tests.cpp`、`tests/chart/packed_atomic_write_tests.cpp`、
  `tests/chart/chart_capacity_probe.cpp`、`tests/cxc/cxc_candidate_roundtrip_tests.cpp`、
  `cmake/VerifyChartCapacity.cmake`、本报告与 `2026-09-17-r4-capacity-data.json`
- 修改：`engine/chart/src/packed_chart_tables.cpp`、`engine/chart/src/packed_semantic_identity.cpp`、
  `tests/chart/CMakeLists.txt`、`tests/cxc/CMakeLists.txt`、`CMakeLists.txt`、
  `tests/chart/packed_semantic_identity_tests.cpp`（D9 闭包声明）、
  `tests/chart/chart_foundation_hardening_characterization_tests.cpp`、
  `tests/chart/packed_chart_io_tests.cpp`、`tests/cxc/cxc_candidate_extension_tests.cpp`
  （类别文本统一）
- 文档：Spec §6.2/§6.3/§6.4/§7.1/§7.3/§9、`docs/guides/BUILDING.md`、阶段报告 README、
  加固计划看板与状态行、`CURRENT_STATUS.md`

## 10. 命令与结果

```powershell
cmake --preset debug
cmake --build --preset debug
.\out\build\debug\bin\cuexis_chart_tests.exe "[r4]"
.\out\build\debug\bin\cuexis_cxc_tests.exe "[r4]"
$env:CUEXIS_RUN_PERFORMANCE_PROBE=1; ctest --preset debug -R cuexis_chart_capacity
ctest --preset debug --no-tests=error
cmake --build --preset debug --target cuexis_format_check
python -B tools/check_docs.py
python -B -m unittest discover -s tools -p "check_docs*tests.py"
git diff --check
```

| 项 | 结果 |
| --- | --- |
| R4 chart 标签用例（`[r4]`） | **12/12 通过**（40,200 assertions；roundtrip 8 + atomic 4） |
| R4 CXC 标签用例（`[r4]`） | **7/7 通过**（39 assertions） |
| `cuexis_chart_tests` | **210/210 通过**（41,848 assertions；含 40000 实体完整往返） |
| `cuexis_cxc_tests` | **59/59 通过**（745 assertions） |
| 容量探针 | 通过（48.2 s，三个 profile 均 `status=ok`） |
| 全量 Debug CTest | **683/683 通过**（368.01 s；1 个预先存在的 symlink 用例按平台跳过） |
| 平台 | Windows x64 / MSVC 14.51 / preset `debug` |
| 未执行 | Release、MinGW、headless sanitize、hosted（R5）；GPU smoke（本批次不涉及） |

## 11. 退出条件对照

| 退出条件 | 证据 |
| --- | --- |
| encode -> decode -> 完整语义比较 -> re-encode/identity | 第 2 节 8 个用例，含 40,000 实体 profile；没有用例只比较计数或只调用 `size()` |
| CXT 阶梯与参数冻结的 expand -> semantic -> Packed -> semantic | 第 3 节；比较实体、父关系、Component、requirement、Beat、typed reference、闭包与 identity |
| 小规模 Transform/Renderable/Camera、资源引用、重复排序边界 | 第 2 节；这些 fixture 明确补充覆盖，不替换 40k profile |
| 真实 CXC candidate 包 | 第 4 节 7 个场景，全部经产品 Writer/Loader/validator |
| 原子写回滚 | 第 5 节 4 个场景，含替换失败与"不删除非本次文件" |
| 机器可读容量数据 + 人类摘要 | 第 6 节与 `2026-09-17-r4-capacity-data.json` |
| 峰值口径与不可测项 | 第 6 节方法学与 `notMeasured`；未把未运行观测计为通过 |
| 兼容 v4/default/digest/architecture | 全量 CTest（含架构与 consumer gates）通过；未改默认路径 |

## 12. R5 交接

R5 接手：Release 构建与全量 CTest、MinGW headless、Linux/hosted 同 SHA 验证、最终 SHA 的容量
复跑与回归矩阵、owner 交接。R4 遗留：A16（CXT capability 声明来源）需要 Stage 6 决策；
父图环路检测的 O(n²) 观测项；`implementationSha` 为运行时 HEAD 而非提交后 SHA。
Stage 6 仍为 future，未因本批次获得任何新能力。
