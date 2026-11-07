# 任务 3 分支 backlog

状态：completed

更新日期：2026-08-31

所属计划：[任务 3 关键模块分支覆盖率](task-3-critical-branch-coverage.md)

本清单记录任务 2 当前实现上的函数级测试 backlog 和最终 hosted 覆盖率。`current hit` 使用各 lane
的 gcovr 分支 covered/total 汇总；具体条件以同目录的 coverage artifact 和下面的行为条目为准。
测试关闭条目时必须补充可观察断言，不以源码命中作为完成依据。

## 最终 Hosted 覆盖率

| lane | preset | covered / total branches | coverage | evidence |
| --- | --- | ---: | ---: | --- |
| Chart v4 | `headless-coverage` | 2392 / 5174 | 46.2% | `cuexis-task-3-core-coverage-299596c533a8c66a78b5c4ada341b1163528fb25/chart-v4-branches.txt` |
| CXC | `headless-coverage` | 1410 / 3400 | 41.5% | `cuexis-task-3-core-coverage-299596c533a8c66a78b5c4ada341b1163528fb25/cxc-branches.txt` |
| Playback | `headless-coverage` | 2728 / 7460 | 36.6% | `cuexis-task-3-core-coverage-299596c533a8c66a78b5c4ada341b1163528fb25/playback-branches.txt` |
| Prepared identity | `headless-coverage` | 10 / 14 | 71.4% | `cuexis-task-3-core-coverage-299596c533a8c66a78b5c4ada341b1163528fb25/prepared-identity-branches.txt` |
| HostClock | `headless-coverage` | 49 / 72 | 68.1% | `cuexis-task-3-core-coverage-299596c533a8c66a78b5c4ada341b1163528fb25/audio-host-clock-branches.txt` |
| Platform SDL | `adapter-coverage` | 93 / 250 | 37.2% | `cuexis-task-3-adapter-coverage-299596c533a8c66a78b5c4ada341b1163528fb25/platform-sdl-branches.txt` |
| AudioSDL | `adapter-coverage` | 337 / 930 | 36.2% | `cuexis-task-3-adapter-coverage-299596c533a8c66a78b5c4ada341b1163528fb25/audio-sdl-branches.txt` |
| OpenGL | `adapter-coverage` | 216 / 2708 | 8.0% | `cuexis-task-3-adapter-coverage-299596c533a8c66a78b5c4ada341b1163528fb25/render-opengl-branches.txt` |
| Shader cache | `shader-tools-coverage` | 386 / 1086 | 35.5% | `cuexis-task-3-shader-tools-coverage-299596c533a8c66a78b5c4ada341b1163528fb25/shader-cache-branches.txt` |
| Shader pipeline cache | `shader-tools-coverage` | 93 / 238 | 39.1% | `cuexis-task-3-shader-tools-coverage-299596c533a8c66a78b5c4ada341b1163528fb25/shader-pipeline-cache-branches.txt` |

本轮同 SHA hosted 门禁为：Linux Quality [33387378354](https://github.com/l-zilch-l/Cuexis/actions/runs/33387378354)、
Windows MSVC [33387378378](https://github.com/l-zilch-l/Cuexis/actions/runs/33387378378) 和 Windows MinGW
[33387378418](https://github.com/l-zilch-l/Cuexis/actions/runs/33387378418)，均为 `success`。当前实现 SHA 为
`299596c533a8c66a78b5c4ada341b1163528fb25`；coverage 构建使用 pinned vcpkg
`40f3c709db80acf154ac4b17a1f83c564ebd022e`、GCC 13.3 和 gcovr 7.0。每个 artifact 的 metadata
记录实现 SHA、编译器、filter、测试命令和环境残余；不在发布树中保存 artifact 本体。

## 初始 backlog

| ID | lane | 文件 | 函数/条件 | current hit | 风险 | 拟用测试与可观察断言 | 环境要求 | 状态 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| B1-C01 | Chart v4 | `engine/chart/src/chart_v4_loader.cpp` | 根节点、必填字段、类型、未知字段和版本路由失败 | 部分 | P1 | 构造最小非法文档；断言 `Result` 失败、diagnostic code、`fieldPath` 和顺序 | headless | planned |
| B1-C02 | Chart v4 | `engine/chart/src/chart_v4_loader.cpp` | 参数/import/clip/layer/group/instance 重复与 exact/max+1 限制 | 部分 | P1 | exact limit 成功、max+1 失败；断言预算 code、路径和无半成品 document | headless | planned |
| B1-C03 | Chart v4 | `engine/chart/src/chart_v4_resolver.cpp` | 参数缺失、未知、类型/range/use-path 和 import identity 失败 | 部分 | P1 | Resolver 失败；断言诊断稳定且不返回部分 resolved artifact | headless | planned |
| B1-C04 | Chart v4 | `engine/chart/src/chart_writer.cpp` | concrete projection/animation program 构建失败不发布半成品 | 部分 | P0 | 失败后检查 `Result`、canonical bytes 未产生、原输入仍可复用 | headless | planned |
| B1-X01 | CXC | `engine/cxc/src/cxc_package.cpp` | entry Chart、manifest、ProjectConfig、Asset Index 失败与 file/memory parity | 部分 | P0 | 两种入口比较失败诊断、package state 和 identity；失败不暴露部分 package | headless | planned |
| B1-X02 | CXC | `engine/cxc/src/zip32_envelope_internal.cpp` | header 不一致、unsupported flag/method、offset/range 溢出、overlap/trailing bytes | 部分 | P1 | 变异 binary fixture；断言稳定 archive diagnostic 和无可读 package | headless | planned |
| B1-X03 | CXC | `engine/cxc/src/cxc_manifest_loader.cpp` | duplicate/case conflict/prefix conflict、entry/bytes/closure exact/max+1 | 部分 | P1 | 预算与路径矩阵；断言诊断顺序、limit/actual context | headless | planned |
| B2-P01 | Playback | `engine/playback/src/playback_session.cpp` | prepare/reload source、capability、resource、presentation 失败回滚 | 部分 | P0 | 保存 state/content/identity/frame/manifest，失败后逐项相等；成功 replacement 清除本次失败 diagnostics | headless | planned |
| B2-P02 | Playback | `engine/playback/src/playback_session.cpp` | stale、wrong-session、mode mismatch、moved-from token commit | 部分 | P0 | 断言错误 code、active candidate 原子保留、operation diagnostics 只描述本次操作 | headless | planned |
| B2-P03 | Playback | `engine/playback/src/playback_source.cpp` | missing document、invalid UTF-8、provider error/throw、ownership boundary | 部分 | P0 | Host provider 注入 `std::exception`/unknown；断言稳定 Result、旧 active 不变、无 lease 泄漏 | headless | headless only |
| B3-I01 | Identity | `engine/chart/src/prepared_semantic_identity.cpp` | chart/CXT/resource/parameter/presentation identity 变化与列表重排规则 | 10 / 14 | P0 | 单变量变异、order-invariant/order-sensitive 对照；断言 identity 差异或相等 | headless | planned |
| B3-S01 | Shader cache | `engine/shader/src/shader_cache.cpp` | key normalization、envelope 截断/版本/长度/key mismatch、load/store/activate 失败 | 383 / 1088 | P0 | cache miss/corrupt/replacement；断言旧 active pipeline 保留、key 差异可解释 | shader tools | planned |
| B3-S02 | Pipeline cache | `engine/shader/src/shader_pipeline_cache.cpp` | compile/load/swap failure 与旧 candidate 生命周期 | 93 / 238 | P0 | 成功替换和失败回滚对照；断言 active pipeline、identity 和 diagnostics | shader tools | planned |
| B4-A01 | HostClock | `engine/audio/src/audio_transport.cpp` | 首次 sample、segment 切换、regression、invalid/non-finite 状态 | 41 / 68 | P1 | 固定输入序列；断言 snapshot tuple、冻结规则和错误 submit 不改旧值 | headless | planned |
| B4-A02 | HostClock | `engine/audio/src/audio_transport.cpp` | owner submit 与 reader snapshot publication/sequence 边界 | 41 / 68 | P0 | barrier/latch + 有界迭代；断言不出现 torn tuple，不用 sleep 命中 | headless | planned |
| B4-S01 | AudioSDL | `engine/audio_sdl/src/sdl_audio.cpp` | Empty/Prepared/Playing/Paused/Stopped/Error 转换及 replacement rollback | 331 / 912 | P0 | dummy driver；断言 active clip、clock、effective settings 和诊断保持完整 | SDL dummy | planned |
| B4-S02 | AudioSDL | `engine/audio_sdl/src/sdl_audio.cpp` | pause/clear/resume、queue/feed/device error 稳定路径 | 331 / 912 | P1 | 可重复 dummy 输入；驱动不可控失败列 residual | SDL dummy | planned |
| B5-R01 | OpenGL | `engine/render_opengl/src/open_gl_backend.cpp` | invalid config、worker thread、failed reconfiguration、stale token | 73 / 832 | P0 | 纯 configuration/probe；断言 active backend 和 diagnostics 不被候选污染 | 无 GPU | planned |
| B5-R02 | OpenGL | `engine/render_opengl/src/open_gl_presentation.cpp` | resource/type/identity/dependency/budget/frame build 失败 | 135 / 1880 | P0 | internal preparation/probe；断言不发布半完成 summary/active presentation | 无 GPU | planned |
| B5-R03 | OpenGL | `engine/render_opengl/src/open_gl_presentation.cpp` | draw ordering、optional uniform、summary null/非 null digest | 135 / 1880 | P1 | 固定 scene；断言 command order、digest 和 summary 行为 | 无 GPU | planned |
| B6-X01 | 横向 | Chart/CXC/Playback/SDL/render public Result 边界 | `bad_alloc`、`std::exception`、unknown exception 转换 | 未单列 | P0 | 复用 provider/allocation executable；断言无未声明异常越界且 active state 保留 | headless/SDL | planned |
| B6-X02 | 横向 | owning source/candidate/token/cache/audio replacement | moved-from、lease、旧 active 生命周期 | 未单列 | P0 | move/failure/replacement 后查询稳定；断言无失效 view | headless | planned |

## 本轮关闭状态

以下条目已由可观察行为测试关闭；`covered` 不表示所有防御性分支均已命中，而是表示该条目的
P0/P1 合同已有稳定测试证据。真实驱动失败仍按本页的 P2 残余处理。

| ID | 状态 | 测试证据 |
| --- | --- | --- |
| B1-C01, B1-C02 | covered | `tests/chart/chart_v4_loader_tests.cpp`：根类型/版本、参数 exact/max+1、重复项、CXT import 路径与重复记录。 |
| B1-C03, B1-C04 | covered | `tests/chart/chart_v4_resolver_tests.cpp`、`tests/chart/chart_v4_c2_contract_tests.cpp`、`tests/chart/chart_writer_tests.cpp`：参数解析、投影、canonical writer 与无 artifact 失败边界。 |
| B1-X01, B1-X02, B1-X03 | covered | `tests/cxc/cxc_package_tests.cpp`、`tests/cxc/zip32_envelope_tests.cpp`、`tests/cxc/cxc_manifest_loader_tests.cpp`、`tests/cxc/cfu_f4_safety_tests.cpp`：file/memory parity、archive/manifest 失败和 partial package 禁止发布。 |
| B2-P01, B2-P02, B2-P03 | covered | `tests/playback/preparation_characterization_tests.cpp`、`tests/playback/playback_session_tests.cpp`、`tests/content/content_provider_tests.cpp`：prepare/reload rollback、token 状态和 `std::exception`/unknown exception 转换。 |
| B3-I01 | covered | `tests/chart/prepared_semantic_identity_tests.cpp`：Chart、CXT、resource 单组件变异的 identity 差异。 |
| B3-S01, B3-S02 | covered | `tests/shader/shader_cache_tests.cpp`：截断 envelope、cache miss/replacement 和旧 active pipeline 保留。 |
| B4-A01, B4-A02 | covered | `tests/audio/audio_tests.cpp`：非法/回归 submit 保留最后发布 sample，以及有界并发 snapshot 一致性。 |
| B4-S01, B4-S02 | covered | `tests/audio_sdl/sdl_audio_tests.cpp`：dummy SDL 状态机、replacement 成功/失败、published settings 和并发读取。 |
| B5-R01 | covered | `tests/render/open_gl_config_tests.cpp`：配置边界、worker-thread、stale/moved token 和失败重配置。 |
| B5-R02, B5-R03 | covered | `tests/render/open_gl_presentation_tests.cpp`：资源/非有限值拒绝、late frame failure 不发布 summary、排序与 digest。 |
| B6-X01, B6-X02 | covered | `tests/content/content_provider_tests.cpp`、`tests/playback/preparation_characterization_tests.cpp`、`tests/shader/shader_cache_tests.cpp`：异常 Result 边界、moved-from token 与 replacement 生命周期。 |

## P2 与环境残余

| 条目 | 原因 | 处置 |
| --- | --- | --- |
| 真实 OpenGL driver compile/link/swap failure | 需要可用 GPU context，dummy/no-GPU CTest 无法稳定复现 | 保留到 smoke/hosted 矩阵，不伪造普通单测命中 |
| SDL 设备拔出、真实格式切换和驱动级 queue failure | dummy driver 不提供稳定故障控制 | 记录为环境分支，保留现有 smoke/hosted 证据入口 |
| 低风险 defensive/default 分支 | 不改变公共结果、identity、cache 或 active state | 只有能用现有 fixture 稳定表达行为时才补测 |

## 关闭规则

- 每个 B1-B6 测试提交前必须把对应 ID 更新为 `covered` 或 `environment-excluded`，并补充测试文件。
- `covered` 必须有行为断言；只提高行/分支数字的测试不算关闭。
- 生产代码只有在回归测试证明现有行为错误时才修改，并在完成报告记录前后结果。
- 最终报告使用与本表相同 preset、filter 和 artifact 口径。
