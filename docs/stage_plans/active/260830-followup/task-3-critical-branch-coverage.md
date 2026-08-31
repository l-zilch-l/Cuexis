# 任务 3：关键模块分支覆盖率实施计划

状态：active

更新日期：2026-08-31

所属计划：[260830-followup 维护计划](plan.md)

本计划承接接手文档中的第三项任务，并以
[Full Review 原始审查](../../../stage_reports/reviews/full-review-2026-08/2026-08-29-review.md)
和 [Full Review 最终关闭报告](../../../stage_reports/reviews/full-review-2026-08/2026-08-30-final.md)
保存的风险、characterization 与 coverage 证据为起点。任务目标不是追求孤立覆盖率数字，而是用
可观察行为测试固定关键失败分支、事务回滚、异常转换、并发发布和稳定 diagnostics。

## 1. 当前基线与问题边界

### 1.1 历史 coverage 基线

`260829-full-review` 最终 Linux Quality artifact 记录：

| 范围 | 行覆盖率 | 分支覆盖率 | 函数覆盖率 |
| --- | ---: | ---: | ---: |
| 全部 `engine/` | 78.1%（14,593/18,688） | 41.1%（14,778/35,994） | 未单列 |
| CXC 重点路径 | 78.9% | 41.5% | 93.3% |
| Chart v4 重点路径 | 84.6% | 47.6% | 97.6% |

这些数字是任务 2 实施前的历史锚点，不作为任务 3 的直接完成基线。B0 必须在任务 2 当前实现上重新
采集同口径结果，记录 covered/total 和百分比，避免因 parse-once 改变分支分母而产生错误比较。

### 1.2 现有 coverage 缺口

当前 `.github/workflows/linux-quality.yml` 的 GCC Coverage 使用 `headless-coverage`：SDL、AudioSDL、
OpenGL adapter 和 shader tools 均未构建。现有 artifact 只单列 CXC 与部分 Chart v4 文件，不能证明
以下任务范围的分支变化：

- Playback prepare/reload 与 presentation；
- Prepared identity 和 shader cache-key；
- HostClock 与 SDL transport；
- OpenGL configuration、presentation prepare 和 frame consumption。

任务 3 因此包含 coverage 证据完善，但不直接提高全局 `engine/` 40% 行覆盖硬门槛。

### 1.3 测试价值边界

本任务只把以下测试计入完成证据：

- 输入或依赖改变后，公共或模块边界结果发生可解释变化；
- 失败后 active state、identity、frame、manifest、cache 或 published snapshot 保持合同；
- diagnostic code、fieldPath、context、cause 和顺序有稳定断言；
- exact-limit、limit+1、重复项、溢出、异常或线程边界有明确结果；
- 并发测试使用 barrier/latch 或有界循环，不依赖随机 sleep 获得命中。

只检查源码字符串、只执行成功路径、没有行为断言、仅为命中一行而构造的测试不计入任务 3 的分支关闭。

## 2. 总体原则

1. **先报告未覆盖分支，再写测试。** B0 输出按文件和函数定位的 branch backlog；不得凭感觉批量补用例。
2. **优先测试现有行为。** 只有测试暴露真实错误时才修改生产代码，修复必须与对应回归测试同批提交。
3. **不为覆盖率扩展公共 API。** fault injection、probe 或 helper 必须保持 internal/test-only，不能进入安装头。
4. **复用现有测试入口。** 优先使用 `HostContentProvider`、FakeAudioTransport、dummy SDL driver、
   Playback allocation executable 和 OpenGL internal probe，不新建平行实现。
5. **异常注入必须可控。** 不通过巨大分配、系统内存耗尽或未定义行为制造 OOM；使用现有 allocator/provider
   注入，并证明异常被转换或按合同保留。
6. **不把不可控平台失败伪装成覆盖。** 无法在 dummy driver 或内部纯逻辑 probe 中稳定触发的 GPU/SDL
   驱动失败，记录为环境分支，交由 smoke/hosted 验证，不添加脆弱的 sleep 或机器相关断言。
7. **百分比是结果，不是唯一目标。** 完成判定以命名的高风险分支关闭为主，同时要求各 lane 的 covered
   branch 绝对数量有实质增加或因生产简化而有可审计的分母下降。

## 3. Coverage 证据设计

### 3.1 Headless 核心报告

保留现有 `headless-coverage` 与全局 `--fail-under-line 40`，扩展 artifact，至少输出：

- `engine-summary.txt` 和 `coverage.xml`；
- `chart-v4-branches.txt`；
- `cxc-branches.txt`；
- `playback-branches.txt`；
- `prepared-identity-branches.txt`；
- `audio-host-clock-branches.txt`。

Chart v4 filter 应覆盖当前实际文件名，不再引用已经拆分或不存在的旧 reader 文件：

- `chart_v4_loader.cpp`；
- `chart_v4_resolver.cpp`；
- `chart_v4_common_reader_internal.cpp`；
- `chart_v4_animation_reader_internal.cpp`；
- `chart_parameter_resolver.cpp`；
- `prepared_semantic_identity.cpp`。

### 3.2 Adapter 报告

新增独立 adapter coverage 配置，构建 SDL、AudioSDL 和 OpenGL adapter，但不要求启动 Player 主循环。
测试使用现有 `SDL_AUDIODRIVER=dummy`、`SDL_VIDEODRIVER=dummy` 和无 GPU internal probe，输出：

- `platform-sdl-branches.txt`；
- `audio-sdl-branches.txt`；
- `render-opengl-branches.txt`。

若某个 OpenGL 分支必须创建真实 context 才可到达，coverage 报告将其标记为环境分支；行为验证放入
现有 smoke/hosted 矩阵，而不是让普通单元测试依赖 GPU。

### 3.3 Shader cache 报告

新增继承 coverage 的 shader-tools 配置，启用 `CUEXIS_BUILD_SHADER_TOOLS=ON` 和
`tests;shader-tools` feature，单列：

- `shader-cache-branches.txt`；
- `shader-pipeline-cache-branches.txt`。

该配置只承担 cache-key、cache envelope、compile/load/swap 失败路径的证据，不改变 shader tools
作为可选模块的产品边界。

### 3.4 Artifact 元数据

每组 artifact 必须记录：

- implementation SHA；
- compiler 与版本；
- gcovr 版本；
- preset、filter 和排除参数；
- 实际执行的测试集合；
- lines/functions/branches 的 covered、total 和 percent；
- skipped 环境分支及理由。

B0 与最终报告必须使用相同 filter。若生产文件移动或拆分，先给出 old/new filter 映射，再比较聚合结果。

## 4. B0：重建基线与 branch backlog

B0 不修改生产行为，只完善报告和建立分支清单。

### 实施

1. 在任务 2 当前实现上运行 headless、adapter 和 shader-tools 三类 coverage。
2. 从 gcovr 文本/XML 中提取未覆盖或部分覆盖的条件，按函数而不是只按文件记录。
3. 建立 [branch backlog](task-3-branch-backlog.md)，字段至少包含：lane、文件、函数/条件、当前命中、风险、拟用测试、
   可观察断言、环境要求和状态。
4. 将分支分为：
   - P0：可能破坏 active state、越过公共异常边界、产生错误 identity/cache 或数据竞争；
   - P1：非法输入、预算、重复项、diagnostic order 和后端失败；
   - P2：低风险 defensive/default 分支或只能由平台环境触发的分支。
5. P0/P1 全部进入本任务；P2 只在有稳定行为价值时实施，其他写入完成报告的残余列表。

### 验收

- 三类 coverage 能在独立构建目录重复运行。
- 所有任务 lane 都有 covered/total 基线，不再用全局百分比推断局部结果。
- 每个拟新增测试先对应 branch backlog 条目。

## 5. B1：Chart v4 与 CXC 非法输入分支

### 5.1 Chart v4

主要生产文件：

- [chart_v4_loader.cpp](../../../../engine/chart/src/chart_v4_loader.cpp)
- [chart_v4_resolver.cpp](../../../../engine/chart/src/chart_v4_resolver.cpp)
- [chart_v4_animation_reader_internal.cpp](../../../../engine/chart/src/chart_v4_animation_reader_internal.cpp)
- [chart_parameter_resolver.cpp](../../../../engine/chart/src/chart_parameter_resolver.cpp)

主要测试文件：

- [chart_v4_loader_tests.cpp](../../../../tests/chart/chart_v4_loader_tests.cpp)
- [chart_v4_resolver_tests.cpp](../../../../tests/chart/chart_v4_resolver_tests.cpp)
- [chart_v4_c2_contract_tests.cpp](../../../../tests/chart/chart_v4_c2_contract_tests.cpp)
- [cfu_f4_limits_tests.cpp](../../../../tests/chart/cfu_f4_limits_tests.cpp)

优先矩阵：

- root/field 类型错误、缺字段、未知字段和 unsupported format/version；
- duplicate parameter/import/clip/layer/group/instance/property/prefix；
- exact max 与 max+1：parameters、imports、clips、tracks、patches、generated records 和 diagnostics；
- parameter unknown/missing/type/range/use-path 失败；
- ProjectDocument 缺失、重复、ASCII case 冲突、错误 CXT identity 和 import 数量不一致；
- canonical writer、concrete projection 或 animation program 构建失败时，不返回半成品 artifact；
- diagnostics 截断 sentinel、fieldPath、context 和确定性排序。

### 5.2 CXC

主要生产文件：

- [cxc_package.cpp](../../../../engine/cxc/src/cxc_package.cpp)
- [cxc_manifest_loader.cpp](../../../../engine/cxc/src/cxc_manifest_loader.cpp)
- [cxc_writer.cpp](../../../../engine/cxc/src/cxc_writer.cpp)
- [zip32_envelope_internal.cpp](../../../../engine/cxc/src/zip32_envelope_internal.cpp)

主要测试文件：

- [cxc_package_tests.cpp](../../../../tests/cxc/cxc_package_tests.cpp)
- [cxc_manifest_loader_tests.cpp](../../../../tests/cxc/cxc_manifest_loader_tests.cpp)
- [zip32_envelope_tests.cpp](../../../../tests/cxc/zip32_envelope_tests.cpp)
- [cfu_f4_safety_tests.cpp](../../../../tests/cxc/cfu_f4_safety_tests.cpp)

优先矩阵：

- central/local header 字段不一致、unsupported flags/method/version/attributes；
- offset/range/size 加法溢出、trailing bytes、overlap、CRC/SHA mismatch；
- duplicate、ASCII case conflict、prefix conflict 和非 portable path；
- manifest entry count/bytes、single entry、aggregate closure 的 exact max 与 max+1；
- ProjectConfig、Asset Index、entry Chart、CXT import 和资源闭包失败的诊断顺序；
- file/memory package parity；失败不能暴露部分 package state；
- `CxcContentProvider` 的缺失资源、类型/identity 不匹配和可注入异常转换。

### B1 验收

- Chart v4 与 CXC 的 P0/P1 backlog 条目全部有行为测试或明确环境排除。
- canonical bytes、package identity、parse-once count 和现有 binary fixture 不变。
- diagnostics fingerprint 与任务 2 前后兼容。

## 6. B2：Playback prepare/reload 与 rollback 分支

主要生产文件：

- [playback_session.cpp](../../../../engine/playback/src/playback_session.cpp)
- [playback_source.cpp](../../../../engine/playback/src/playback_source.cpp)
- [presentation.cpp](../../../../engine/playback/src/presentation.cpp)

主要测试文件：

- [preparation_characterization_tests.cpp](../../../../tests/playback/preparation_characterization_tests.cpp)
- [playback_v4_prepare_tests.cpp](../../../../tests/playback/playback_v4_prepare_tests.cpp)
- [playback_session_tests.cpp](../../../../tests/playback/playback_session_tests.cpp)
- [presentation_tests.cpp](../../../../tests/playback/presentation_tests.cpp)
- [playback_allocation_tests.cpp](../../../../tests/playback/playback_allocation_tests.cpp)

按 prepare stage 建立失败矩阵：

| 阶段 | 典型失败 | 必须断言 |
| --- | --- | --- |
| source/document | moved-from、missing entry、invalid UTF-8、Chart/CXT invalid | active state 与旧 diagnostics 不被候选污染 |
| parameters/capability | unknown、duplicate、type/range、unsupported capability | preflight 顺序和 diagnostic code/path 稳定 |
| resource/audio | provider error/throw、missing resource、revision/identity mismatch | lease 不泄漏，旧 content/identity/frame 保留 |
| runtime/presentation | compile/prepare/payload/manifest failure | 不发布 World 或 presentation candidate |
| frame/layout/identity | invalid target frame、layout failure、missing/colliding identity | 不产生半完成 PreparedPlayback |
| commit | stale、wrong-session、mode mismatch、moved-from token | active candidate 原子保留，operation diagnostics 只描述本次失败 |

每个 rollback 测试统一捕获并比较：

- `SessionState`；
- `PlaybackContentInfo`；
- `PreparedSemanticIdentity`；
- `FrameSnapshot` 和 FrameDigest；
- presentation manifest/resource references；
- active diagnostics 与 `lastOperationDiagnostics`；
- reload policy 对目标时间和 discontinuity 的影响。

异常矩阵优先复用 Host ContentProvider 和现有 allocation executable，覆盖 `std::bad_alloc`、
`std::exception` 与 unknown exception 的稳定转换。随后执行一次成功 prepare/reload，证明上一失败的
operation diagnostics 不会残留。

## 7. B3：Identity 与 cache-key 分支

主要生产文件：

- [prepared_semantic_identity.cpp](../../../../engine/chart/src/prepared_semantic_identity.cpp)
- [shader_cache.cpp](../../../../engine/shader/src/shader_cache.cpp)
- [shader_pipeline_cache.cpp](../../../../engine/shader/src/shader_pipeline_cache.cpp)

主要测试文件：

- [prepared_semantic_identity_tests.cpp](../../../../tests/chart/prepared_semantic_identity_tests.cpp)
- [playback_identity_tests.cpp](../../../../tests/playback/playback_identity_tests.cpp)
- [shader_cache_tests.cpp](../../../../tests/shader/shader_cache_tests.cpp)

优先矩阵：

- Chart、CXT、resource、parameter、presentation 任一 identity 变化时结果变化；
- 允许重排的列表保持 order-invariant，不允许重排的输入保持 order-sensitive；
- missing identity、duplicate component、resource identity collision 和输入数量边界；
- cache key 的 importer/target profile、keyword、entry、tool version 和 source identity 差异；
- profile/keyword/tool 的 canonical normalization 与大小写规则；
- metadata 非空但 semantic identity 为零时稳定拒绝；
- cache envelope 截断、版本/长度/key mismatch、损坏 record 和非法 filename；
- cache miss/compile/store/activate 任一步失败时保留旧 active pipeline；成功替换后旧 candidate 失效。

任务 3 不改变冻结的 identity 编码。若测试发现当前实现与既有 golden 冲突，先作为阻断 finding 报告，
不得直接迁移算法或 cache 格式。

## 8. B4：HostClock 与 SDL 发布分支

主要生产文件：

- [audio_transport.cpp](../../../../engine/audio/src/audio_transport.cpp)
- [sdl_audio.cpp](../../../../engine/audio_sdl/src/sdl_audio.cpp)

主要测试文件：

- [audio_tests.cpp](../../../../tests/audio/audio_tests.cpp)
- [sdl_audio_tests.cpp](../../../../tests/audio_sdl/sdl_audio_tests.cpp)

HostClock 优先矩阵：

- 首次 sample、同 segment 单调前进、segment 切换和 source regression；
- invalid state、non-finite time、stopped/playing/underrun/error 状态组合；
- owner submit 与 reader snapshot 并发时，sample 字段来自同一 publication；
- sequence counter 边界和重复 snapshot，不读到 torn tuple；
- 错误 submit 不改变最后一个有效 snapshot。

SDL transport 优先矩阵：

- Empty/Prepared/Playing/Paused/Stopped/Error 的合法与非法转换；
- replacement prepare、cancel、activation failure、unload 和 idempotent 操作；
- pause/clear/resume、device-format change、queue query 和 feed failure的可稳定路径；
- effective settings 与 clock snapshot 作为完整 tuple 发布；
- owner-thread 拒绝、callback/owner 并发读取和 device error 后冻结规则；
- 失败 replacement 保留旧 active clip、clock 和 effective settings。

并发测试必须使用同步原语确定开始/结束窗口并设置有界迭代；不得以运行时间或 sleep 次数作为断言。
无法由 SDL dummy driver 稳定触发的驱动错误写入 residual，不为其增加全局 SDL mock 层。

## 9. B5：Render adapter 失败与 active state 分支

主要生产文件：

- [open_gl_backend.cpp](../../../../engine/render_opengl/src/open_gl_backend.cpp)
- [open_gl_presentation.cpp](../../../../engine/render_opengl/src/open_gl_presentation.cpp)

主要测试文件：

- [open_gl_config_tests.cpp](../../../../tests/render/open_gl_config_tests.cpp)
- [open_gl_presentation_tests.cpp](../../../../tests/render/open_gl_presentation_tests.cpp)
- [render_scene_tests.cpp](../../../../tests/render/render_scene_tests.cpp)

优先矩阵：

- invalid OpenGL version/profile、worker thread、stale/moved token 和 failed reconfiguration；
- missing mesh/material/texture/shader、resource type mismatch、identity mismatch 和 dependency mismatch；
- non-finite matrix/color/bounds、empty mesh、invalid index/range 和 decoded budget；
- opaque/transparent ordering、equal-depth tie、visibility、missing transform 和 optional uniform；
- presentation prepare/upload/cache failure时保留旧 active presentation；
- frame build/render failure不发布半完成 summary，不改变上一个可用 active state；
- summary 为 null 时不执行复制/digest，summary 开启时 command order 与 digest 稳定；
- shader tools disabled/enabled 两种构型的 capability 和 fallback 分支。

无 GPU 单元测试优先调用现有 internal `probeBuildDraws` 和纯 preparation helper。真实 GL context、driver
compile/link 或 swap failure 仅在可重复的 smoke/hosted 环境中断言，不加入机器相关的普通 CTest。

## 10. B6：OOM、异常与所有权横向审计

B1-B5 完成后进行横向检查，避免每个 lane 只覆盖自己的正常错误码。

### OOM 与异常

- Static Playback allocation test 覆盖所有 owning-copy public queries 和候选准备边界。
- Provider/callback 注入覆盖 `bad_alloc`、`std::runtime_error` 和 unknown exception。
- CXC/Chart/Playback/SDL/render 的公共 Result 边界不得泄露未声明异常。
- 析构、noexcept、音频 callback 和渲染热路径不得抛异常。
- 不使用超大 fixture 逼迫真实 OOM；预算分支使用声明大小、exact max/max+1 或 allocator 注入。

### 所有权

- moved-from source/candidate/token 的结果稳定；
- failed prepare/reload 不泄漏 resource lease 或持有失效 view；
- cache/presentation/audio replacement 的旧 active 对象在 commit 前持续可用；
- concurrent snapshot 返回 owning/coherent 值，不暴露内部可变容器。

### Fault seam 审批条件

只有同时满足以下条件才新增 internal fault seam：

1. 对应 P0/P1 分支无法通过现有 provider、fake、dummy driver 或 probe 稳定触发；
2. seam 表达真实依赖失败，不是 `if (failForTest)`；
3. 不进入安装头或 SDK API；
4. 使用 RAII/thread-local 或显式依赖对象，测试结束后无全局残留；
5. 至少有一个测试断言失败转换和 active state 保留。

## 11. 批次顺序与文件所有权

推荐顺序：**B0 -> B1 -> B2 -> B3 -> B4 -> B5 -> B6**。

- B0 的 workflow、preset 和 coverage filter 由主任务串行维护。
- B1 Chart/CXC 完成后再整合 B2 Playback，避免与任务 2 刚修改的 dispatch/prepare 文件并行冲突。
- B3 shader cache 可与 B4 audio/SDL 并行，但 Chart prepared identity 测试必须与 B2 串行。
- B5 render 可与纯 Chart/CXC 测试并行；若触及 shader cache 或 Playback presentation，则串行整合。
- `CMakeLists.txt`、`CMakePresets.json`、workflow、共享 fixture 和最终报告只由主任务修改。
- 每个 lane 先提交测试；仅在测试证明 bug 后，于同一 lane 增加最小生产修复。

## 12. 分批验证矩阵

### 每个 lane

```powershell
cmake --build --preset debug --target <affected-test-targets>
ctest --preset debug -R "<focused-test-regex>" --output-on-failure
```

适用目标包括：

- `cuexis_chart_tests`
- `cuexis_cxc_tests`
- `cuexis_playback_tests`
- `cuexis_playback_allocation_tests`
- `cuexis_audio_tests`
- `cuexis_audio_sdl_tests`
- `cuexis_platform_sdl_tests`
- `cuexis_render_tests`
- `cuexis_render_opengl_tests`
- `cuexis_shader_tests`

### 本地完成门禁

```powershell
cmake --preset debug --fresh
cmake --build --preset debug --clean-first
ctest --preset debug --no-tests=error

cmake --preset release --fresh
cmake --build --preset release --clean-first
ctest --preset release --no-tests=error

cmake --preset debug-shader-tools --fresh
cmake --build --preset debug-shader-tools --clean-first
ctest --preset debug-shader-tools --no-tests=error -R "shader|cache|identity|presentation"

cmake --build --preset debug --target cuexis_format_check
python -B tools/check_docs.py
git diff --check
```

### Linux/hosted 门禁

- headless GCC coverage 与最终分模块 artifact；
- adapter coverage；
- shader-tools coverage；
- Clang ASan + UBSan；
- shader-tools sanitizer；
- Linux GCC/Clang、Windows MSVC、Windows MinGW 的受影响 Debug/Release 测试；
- static/shared external consumer 与 installed public-header boundary。

Hosted 结果必须与任务 3 最终报告记录的同一 SHA 对应；失败或 rerun 不得被写成通过证据。

## 13. 完成判定

任务 3 同时满足以下条件才可标记 completed：

1. B0 有任务 2 后的 headless、adapter、shader-tools 三类可重复 coverage 基线。
2. Chart v4、CXC、Playback、identity/cache、HostClock/SDL、render 六个 lane 都有命名的 branch backlog。
3. 所有 P0/P1 条目均由行为测试关闭，或有明确、可复核的环境排除理由。
4. 每个 lane 的 covered branch 绝对数量增加；若生产简化删除分支，报告同时给出分母变化和代码证据。
5. 全局 `engine/` 行覆盖不低于现有 40% 门槛，分支覆盖不得无解释回退。
6. 新增测试断言 diagnostics、identity/cache、rollback、published tuple 或 active state，不以源码 grep
   和无行为行覆盖充数。
7. 没有新增公共 fault API、JSON DOM 泄漏、跨模块异常、数据竞争或机器相关 flaky test。
8. Debug、Release、shader-tools、sanitizer、coverage 和跨平台门禁按影响范围通过。
9. 新增任务 3 完成报告，记录每个 backlog 条目的处置、覆盖率前后数据、测试结果、production fix 和 residual。

## 14. 明确不包含

- 仅为提高数字而重构无关生产代码或删除 defensive checks。
- 直接提高全局 engine 40% 行覆盖硬门槛。
- RT-29 render-state 大规模排序、T1 World/Animation 优化或 T2 大包解析降本。
- Stage 6、Studio、Judgement/Replay、稳定 C ABI 或新的 SDK 功能。
- runtime script、逐帧 script callback 或相关格式/extension/capability。
- 依赖真实 OOM、随机线程调度、特定 GPU/声卡或 sleep 时序的普通单元测试。
