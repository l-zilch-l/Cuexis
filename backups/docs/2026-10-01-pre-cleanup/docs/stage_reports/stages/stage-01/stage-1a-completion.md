# Cuexis 阶段 1A 完成报告

状态：已完成  
报告日期：2026-07-18  
完成版本：`26.07.18.14-1`  
阶段目标：完成规范谱面加载、确定性 Runtime 编译、World 实例化与 OpenGL DebugDraw 的最小闭环。

后续取代说明：本报告中的 Simple Chart/方案 B 是阶段 1A 完成时的历史事实；ADR 0035 已在阶段 2A.1 删除该格式、Schema、Importer、测试和 fixture。

## 1. 完成结论

阶段 1A 的任务和验收标准已经完成。当前仓库可以加载方案 A canonical chart 和方案 B simple chart，将两者统一为 typed `ChartDocument`，确定性编译为 `ChartRuntime`，事务式发布到新的 `World`，计算父子世界矩阵，并通过后端无关的 `RenderScene` / `RenderCommand` 生成 DebugDraw 坐标轴，最终由 OpenGL 3.3 Core 管线显示。

配置设计门禁已经由 ADR 0024 完成：阶段 1B ProjectConfig 的固定位置为 `<project-root>/cuexis.project.json`，格式身份为 `format: "cuexis.project"`；v1 字段、Schema 和迁移仍按约定留到阶段 1B 随真实消费者冻结。阶段 1A 没有建立全局可变 `EngineConfig`，也没有提前实现 UserPreferences、DeviceProfile 或设备校准格式。

最终 Debug 和 Release 均完成 clean build、`113/113` 项 CTest、全仓 clang-format 检查，以及 canonical/simple 两条真实 GPU 三帧冒烟。四次 GPU smoke 均提交 3 个对象、生成 9 条 Debug Line 命令并恰好完成 3 帧。

## 2. 实际交付范围

| 领域 | 完成内容 |
| --- | --- |
| 配置边界 | ADR 0024、跨阶段配置所有权、覆盖与约束、失败和诊断规则、ProjectConfig 近期格式身份 |
| Core | 有上限的 `Diagnostic/Diagnostics`、Cuexis-owned `Vec3/Quat/Mat4`、GLM 私有实现 |
| JSON Support | Cuexis-owned JSON Value、大小/深度/字符串限制、重复键拒绝、typed Reader、稳定字段路径、确定性序列化、Schema adapter |
| Chart A | canonical v1 loader、typed ChartDocument、模板单继承、固定 Schema Patch、引用和扩展校验 |
| Chart B | simple v1 importer、确定性 UUIDv5、Beat/欧拉角转换、模板合并、未知字段保留 |
| Timing | 有理数 Beat、固定 BPM 与 offset 的 `TimingMap`，拒绝非空 BPM Changes/Stops |
| Compiler | 与输入对象顺序无关的 `ChartRuntime`、稳定对象/Behavior 排序和 `parentIndex` |
| World/Runtime | Transform/Hierarchy/WorldTransform、原子矩阵发布、ChartWorldInstantiator、prepare/commit/reload/unload |
| Assets/Behavior/Gameplay | typed AssetId/Handle 基础、Behavior 引用组件、NoteTag/ElementTag；不提前实现资源管理或 Behavior 求值 |
| Render/Debug | `RenderScene`、Debug Line Command、typed RenderableComponent、稳定 XYZ 轴线提取 |
| OpenGL | OpenGL 3.3 Debug Line Shader/VAO/VBO 管线，业务和 Chart 代码不直接调用 OpenGL |
| Player | `--chart <path>`、默认 canonical 示例、A/B 加载到 GPU 的三帧闭环、稳定失败路径 |
| 测试与守卫 | 12 个 Catch2 测试程序、CTest 发现、架构扫描、Player 失败路径、格式门禁、深链压力测试 |

正式激活的 13 个非测试 target：

```text
cuexis_core
cuexis_json_support
cuexis_platform_sdl
cuexis_world
cuexis_assets
cuexis_chart
cuexis_behavior
cuexis_gameplay
cuexis_render
cuexis_debug
cuexis_runtime
cuexis_render_opengl
cuexis_player
```

## 3. 关键实现结果

### 3.1 配置 ADR 与所有权

- ADR 0024 已接受，定义 ProjectConfig、UserPreferences、DeviceProfile、PreflightCapabilities、设备/校准 Profile、LaunchOptions、ResolvedAppConfig、ResolvedSessionConfig 和 EffectiveSettings 的唯一职责。
- 引擎模块只接收已经解析和校验的 typed config 或不可变快照，不自行读取配置文件。
- ProjectConfig 的确定性内容不能被 UserPreferences 隐式覆盖；DeviceProfile 是能力约束，不是普通的最后写入覆盖层。
- 每种持久化格式只在首次出现真实消费者时冻结字段、Schema、迁移和未知字段策略。
- 阶段 1A 只冻结阶段 1B ProjectConfig 的文件定位和 format 身份，不提前冻结 v1 Schema。

### 3.2 Core 与 JSON Support

- `Diagnostics` 可配置总条目硬上限；首次超限时以唯一 sentinel 占据最后一个位置，总数始终不超过上限。
- Cuexis 公共数学类型为 `Vec3`、`Quat`、`Mat4`，采用 column-major 矩阵语义；GLM 只存在于 Core 私有实现。
- JSON parser 拒绝超出输入字节、嵌套深度和单个解码后 UTF-8 object key/string value 限制的输入。
- JSON parser 拒绝重复 object key，并保持 signed/unsigned/float/string/array/object 的值类别。
- typed `Reader` 对缺失字段、类型错误、安全整数转换和未知字段输出稳定字段路径诊断。
- `cuexis_json_support` 提供 JSON Schema adapter 和独立测试；阶段 1A Chart loader 当前使用 typed Reader 加 Chart 语义校验，尚未在加载链调用该 adapter。
- GLM、nlohmann-json 和 json-schema-validator 类型均未泄漏到 Cuexis 公共 API。

阶段 1A 默认 Chart 预算：

```text
输入：16 MiB
JSON 嵌套深度：64
单个 JSON key/string value：1 MiB
Metadata 成员：1024
Template：10000
Behavior：10000
Object：100000
每 Template Patch：256
每 Behavior Track：65536
每 Track Key：65536
扩展：256
诊断总数：1024
Identifier：256 bytes
Simple Beat 文本：128 bytes
Beat numerator magnitude：1,000,000,000,000
Beat denominator：1,000,000,000
```

### 3.3 方案 A canonical chart

- 格式身份为 `format: "cuexis.chart"`、`version: 1`，正式 Schema 为 `schemas/cuexis.chart.v1.schema.json`。
- `chartId` 始终要求规范 UUIDv7。
- 原生方案 A Object/Template ID 使用 UUIDv7；方案 B 导入后保存的确定性 Object/Template ID 允许 UUIDv5。
- 引用使用显式 domain/id 对象，并校验 object、template、behavior 和 asset 域。
- Template v1 支持单继承；根模板提供 prototype，派生模板提供固定 component 路径 Patch。
- Template 解析和 Object 层级校验均使用显式链栈，不依赖进程递归栈。
- `requiredExtensions` 先逐项校验 object、`id`、正 `version` 和未知字段，再对合法但没有处理器的条目返回稳定 unsupported 诊断。
- 未知 optional extension 被原样保留并产生 warning，不影响 v1 Runtime。

### 3.4 方案 B simple chart

- 格式身份为 `format: "cuexis.chart.simple"`、`version: 1`，正式 Schema 为 `schemas/cuexis.chart.simple.v1.schema.json`。
- Simple ID 使用稳定的小写 ASCII 规则；Object/Template ID 通过 `UUIDv5(chartId, domain + simpleId)` 确定性生成。
- UUIDv5 实现通过 RFC 4122 DNS namespace 黄金向量；输入键顺序不会改变转换后的 ID 或 Runtime 排序。
- Beat 字符串支持整数、分数和十进制精确转换，不经浮点累计。
- Euler degree 按固定 `Rz * Ry * Rx` 规则转换为规范 Quaternion。
- Template 只支持一个模板引用和固定字段递归覆盖；数组整体替换，null 删除可选字段。
- 未知 root、timing、template、object、transform、render、behavior、track 和 key 字段均 warning，并以原始稳定字段路径保存到 `extensions["cuexis.simple.unknown"]`。
- Object/Template/Behavior 范围的未知字段诊断携带 canonical ID，上述未知数据不进入 Runtime 行为。

### 3.5 ChartRuntime、World 和事务生命周期

- `ChartCompiler` 输出按稳定 ID 排序的 Object 与 Behavior，结果不依赖 canonical objects 数组顺序或 simple object key 顺序。
- 每个 Runtime Object 使用稳定 `parentIndex`；缺失父对象时确定性跳过整个子树并产生 warning，层级环使编译失败且不发布部分结果。
- World 保存 Local Transform、Hierarchy 和完整 WorldTransform；验证失败保留上一份完整矩阵结果。
- `RuntimeSession::prepare()` 在临时 World 中完成实例化和不变量检查，`commit()` 只发布完整结果。
- reload 失败保留旧 World 和 Object mapping；成功 reload 发布完整 replacement；unload 的销毁顺序经过测试。
- RuntimeSession 使用 `std::unique_ptr<World>`，并在 owner thread 约束下拒绝 worker-thread prepare。
- 阶段 1A 不支持加载外部 Renderable 资源；存在 mesh/material 引用时返回 `runtime.chart.renderable_resources_unsupported`，且不发布 World。

### 3.6 Render、DebugDraw 和 Player

- Render 前端只接收后端无关的 `RenderFrame`、`RenderScene` 和 Debug Line `RenderCommand`。
- DebugDraw 按稳定实体顺序从 WorldTransform 产生每个对象的 X/Y/Z 三条世界空间轴线。
- OpenGL 调用只存在于 `cuexis_render_opengl`，阶段 1A 增加 Debug Line Shader、VAO/VBO 上传和绘制。
- canonical 和 simple 示例均包含 `root -> child -> grandchild` 三层 Transform，刻意不引用阶段 1B 资源。
- Player 默认加载构建后复制到可执行文件旁的 canonical 示例；`--chart` 可以显式加载 simple 或 canonical 文件。
- `--smoke-test` 严格完成 3 帧；未知参数、缺失 `--chart` 值、缺失谱面文件和 SDL 初始化失败均通过 CTest 验证。
- Player 组合层保留最小 NullClock、NullInput 和 NullJudge；正式时间驱动与判定不属于阶段 1A。

## 4. 直接依赖与许可证

固定 vcpkg baseline：`8e8dfb4ba483886936ded5ca201b500b8d8b0096`

| 依赖 | 解析版本 | 阶段 1A 用途 | 许可证 |
| --- | --- | --- | --- |
| SDL3 | 3.4.12 | Window、事件与 OpenGL 平台集成 | zlib，部分配置包含 MIT/Apache-2.0 |
| EnTT | 3.16.0 | World ECS Registry | MIT |
| GLM | 1.0.3 | Cuexis-owned 数学类型的私有实现 | MIT |
| nlohmann-json | 3.12.0#2 | JSON DOM、解析与内部转换 | MIT |
| JSON Schema Validator | 2.4.0 | JSON Schema adapter | MIT |
| spdlog | 1.17.0#1 | Core 日志实现 | MIT |
| fmt | 12.2.0 | Core/spdlog 格式化 | MIT |
| glad | 0.1.36 | OpenGL 函数加载 | MIT，生成输入含 Khronos 相关许可证 |
| Catch2 | 3.15.2 | 测试 target | BSL-1.0 |
| tl-expected | 1.3.1 | C++20 Result 基础 | CC0-1.0 |

`stduuid` 曾作为 UUID 候选依赖评估，但当前网络无法访问其 GitHub 源且本机没有可用缓存，因此没有进入 manifest 或第三方 notice。阶段 1A 在 Chart 私有实现中提供严格 UUID 文本校验与 RFC 4122 UUIDv5，并以黄金向量和非法 namespace 测试锁定行为；公共 API 不暴露 UUID 库类型。

完整上游地址、分发方式和退出路径见根目录 `THIRD_PARTY_NOTICES.md`。正式发布前仍须根据实际打包产物重新收集传递依赖 notice。

## 5. 最终验证环境

```text
操作系统：Windows x64
Developer Shell：Visual Studio 2026 Developer PowerShell 18.7.3
MSVC：19.51.36248，工具目录 14.51.36231
Windows SDK：10.0.26100.0
CMake：4.3.3
Ninja：1.13.2
clang-format：22.1.3
vcpkg triplet：x64-windows
GPU：NVIDIA GeForce RTX 4060 Laptop GPU
Driver/OpenGL：NVIDIA 596.36 / OpenGL 3.3.0
SDL video driver：windows
```

## 6. 实际验证结果

| 验证项 | 结果 |
| --- | --- |
| Debug fresh configure | 通过，全部依赖命中本机缓存 |
| Debug clean build | 通过，78 个 Ninja 编译/链接步骤，最终无编译警告 |
| Debug CTest | `113/113` 通过，0 失败，22.28 秒 |
| Debug architecture scan | 通过，包含在 CTest |
| Debug clang-format dry-run | 通过 |
| Debug canonical GPU smoke | 通过，3 objects / 9 commands / 3 frames |
| Debug simple GPU smoke | 通过，3 objects / 9 commands / 3 frames |
| Release fresh configure | 通过，新增依赖从本机 vcpkg archive 恢复 |
| Release clean build | 通过，78 个 Ninja 编译/链接步骤，无编译警告 |
| Release CTest | `113/113` 通过，0 失败，3.09 秒 |
| Release architecture scan | 通过，包含在 CTest |
| Release clang-format dry-run | 通过 |
| Release canonical GPU smoke | 通过，3 objects / 9 commands / 3 frames |
| Release simple GPU smoke | 通过，3 objects / 9 commands / 3 frames |

四次 GPU smoke 的共同关键输出：

```text
Committed objects: 3
Debug commands: 9
Video driver: windows
Version: 3.3.0 NVIDIA 596.36
Vendor: NVIDIA Corporation
Renderer: NVIDIA GeForce RTX 4060 Laptop GPU/PCIe/SSE2
Completed frames: 3
```

测试组成：

```text
12 个 Catch2 测试可执行文件
108 个 Catch2 TEST_CASE
1 个 CMake 架构扫描
4 个 CMake Player 失败路径
总计 113 项 CTest
```

重点覆盖：

```text
配置 ADR 和公共边界的架构扫描
Diagnostics 总上限与唯一 sentinel
JSON 重复键、大小、深度、UTF-8 字符串、typed Reader 和 Schema adapter
UUIDv7 校验、RFC 4122 UUIDv5 黄金向量及非法 namespace
RationalBeat 精确解析、规范化、范围和无溢出排序
canonical/simple 路由、字段诊断、未知字段保留和确定性转换
Template 单继承、Patch、缺失引用和环
10000 个 Template 的最大继承链
12000 个 Object 的深层父子链
对象输入顺序无关、缺失父子树跳过和层级环拒绝
RuntimeSession prepare/commit/reload/unload、失败回滚和线程约束
World 矩阵原子发布与跨 World 确定性
RenderScene、DebugDraw、OpenGL 配置和后端线程边界
Player 参数、谱面打开和 SDL 初始化失败路径
```

## 7. 验收标准对照

| 阶段 1A 验收标准 | 证据 | 结论 |
| --- | --- | --- |
| 配置 ADR 已接受，近期 ProjectConfig 落点明确 | ADR 0024，文件名/format 已冻结，v1 Schema 留 1B | 通过 |
| 结构化读取产生确定性字段路径诊断 | JSON Reader、Chart A/B 诊断和未知字段测试 | 通过 |
| 可以加载方案 A，编译不依赖 objects 数组顺序 | canonical 示例、排序与跨 World 测试 | 通过 |
| 方案 B 可确定性转换为方案 A | simple 示例、UUIDv5 黄金向量、键顺序测试 | 通过 |
| 显示多个父子 Transform 的 3D Entity | A/B 各 3 对象、9 轴线命令、四次 GPU smoke | 通过 |
| 业务代码不直接调用 OpenGL | 架构扫描、target 依赖白名单、源码复核 | 通过 |

## 8. 最终架构重审

- Core 和公共头未暴露 SDL、OpenGL、GLM、nlohmann-json 或 json-schema-validator 类型。
- Chart 不拥有 EnTT Registry，不直接创建 Entity，也不依赖 OpenGL。
- Runtime 负责 ChartRuntime 到 World 的实例化；World 不解析 Chart 文档。
- RenderScene 与 RenderCommand 不含 glad/OpenGL 类型；OpenGL 调用仍限制在 `engine/render_opengl`。
- `RenderableComponent` 使用 typed Mesh/Material Handle；阶段 1A 不伪造资源内容或 ResourceManager。
- Behavior 和 Gameplay target 只提供本阶段所需的 typed 数据，不包含提前实现的求值、输入或判定逻辑。
- RuntimeSession 发布、reload 回滚和 unload 顺序都由测试锁定，不暴露半构造 World。
- Chart/JSON 输入具有明确预算，诊断集合本身也有硬上限。
- 方案 A/B 示例均无外部资源依赖，可独立验证实例化和渲染闭环。

## 9. 验证期间发现并解决的问题

### 9.1 Simple 模板覆盖生成无效 remove Patch

Simple importer 最初会为“模板和合并结果中都不存在”的组件生成 `remove` Patch，Canonical loader 正确返回 `chart.patch.target_missing`。修复后仅在组件实际存在性或内容发生变化时生成 Patch，A/B 示例和模板合并测试通过。

### 9.2 深层递归与已接受预算不匹配

早期 Object 层级 DFS 和 Template 继承展开使用递归。在 100000 Object / 10000 Template 的接受预算下存在进程栈风险。最终改为显式链栈，并加入 12000 Object 与最大 10000 Template 继承链测试。

### 9.3 Simple 嵌套未知字段未完整保留

早期实现只保留 root 和 template/object 一级未知字段；timing、transform、render、behavior、track、key 只 warning 或直接拒绝。最终统一为 warning、保存原始字段路径和 canonical ID，且验证未知数据不进入 Runtime。

### 9.4 JSON 单字符串预算只有声明没有消费

`ChartLimits::maxStringBytes` 最初未进入 parser。最终扩展 `ParseLimits`，按解码后的 UTF-8 字节数同时限制 object key 和 string value，并覆盖边界、超限上下文和 Chart 传播测试。

### 9.5 requiredExtensions 缺少逐字段校验

早期实现对所有条目直接返回 unsupported。最终先验证条目类型、`id`、正 `version` 和未知字段；只有结构合法的 required extension 才返回阶段 1A 无处理器的稳定 unsupported 诊断。

### 9.6 JSON Schema Validator 行为差异

`json-schema-validator 2.4.0` 不完整执行元 Schema 校验，原测试中的部分非法 `type` 写法被依赖容忍。测试改为依赖明确拒绝的根标量 Schema，adapter 仍将编译/求值错误映射为 `json.schema.invalid`。Chart loader 当前不调用 adapter，因此完成报告不把 formal Schema artifact 与运行时 validator 执行混为一件事。

### 9.7 构建缓存、格式和 vcpkg 权限

阶段中曾出现公共头更新晚于旧目标文件的情况，增量构建保留了旧对象语义。最终 Debug/Release 均执行 clean build。全仓阶段 1A C++ 文件也统一经过 clang-format 22.1.3，并重新构建和运行全部测试。Release 旧构建树自动重生成时因沙箱无法写入共享 `D:/vcpkg` 而失败；使用授权的 fresh configure 后从本机 archive 恢复依赖并完成最终验证，未改变依赖版本。

## 10. 剩余非阻断风险与明确非目标

- Chart loader 当前以 typed Reader 和 Chart 语义校验为运行时权威；formal JSON Schema 与 adapter 已存在并独立测试，但尚未接入 loader。未来接入前需处理依赖主要 Draft 7 能力与文档声明 Draft 2020-12 的兼容边界。
- GPU 验证只覆盖当前 Windows/NVIDIA 环境，尚未形成 AMD、Intel、Linux 或移动设备矩阵。
- 阶段 1A 对外部 Renderable 明确失败；真实 Mesh/Material/Texture 加载、ResourceManager、Lease/Scope 和 Handle generation 失效属于阶段 1B。
- ProjectConfig v1 loader、路径规范化和 AssetDatabase 创建属于阶段 1B；本阶段只完成配置 ADR 和格式身份门禁。
- Behavior tracks 在 ChartDocument 中保持 opaque，ChartRuntime 只携带 Behavior identity/type/version；采样、Seek 重求值和 Transform Keyframe 属于阶段 1C。
- BPM Changes、Stops、音频时钟、输入、判定、Studio、粒子和正式打包均不属于阶段 1A。
- 10000 Template 深链在 Debug 下约占 18 秒测试时间；这是有意保留的最大预算回归证据，不是 Player 启动路径基准。

## 11. 阶段结论和后续边界

阶段 1A 已达到“可解析、可诊断、可确定性编译、可事务实例化、可在真实 GPU 显示”的完成状态。下一优先级进入阶段 1B 资源生命周期闭环：

```text
实现 cuexis.project.json / cuexis.project 的 ProjectConfig v1
只随真实 AssetDatabase 消费者冻结字段、Schema 和迁移
规范化项目内资产路径并拒绝越界
实现 Mesh / Material / Texture Handle 的真实资源映射
实现同步 ResourceManager、ResourceLease、ResourceScope
实现 Required / Fallback / Optional 引用策略
用资源事务替换阶段 1A 的 renderable_resources_unsupported 路径
保持 RuntimeSession reload 失败回滚和旧 Handle generation 失效
```

阶段 1B 不应顺带实现 Behavior Track 求值、AudioClock、复杂输入、正式 Judge 或 Studio；这些能力继续按 PROJECT_GUIDE 的后续子阶段推进。

## 12. 相关文档

- `../PROJECT_GUIDE.md`
- `../BUILDING.md`
- `../CHART_FORMAT.md`
- `../SIMPLE_CHART_FORMAT.md`
- `../RUNTIME_SESSION.md`
- `../TIMING_MODEL.md`
- `../adr/0024-configuration-ownership-and-staged-formats.md`
- `../../schemas/cuexis.chart.v1.schema.json`
- `../../schemas/cuexis.chart.simple.v1.schema.json`
- `../../THIRD_PARTY_NOTICES.md`

