# S6-F1 最终验证退出记录

- 日期：2026-09-27
- 范围：Stage 6 `S6-F1`（最终验证）
- 候选 SHA：`c80a5b5f43a6aa7021037aaf2e308e2b9ec3d764`
- 代码末端 SHA：`d7980bd9b5cf7730889157ea8ec093799d53d7fc`
- 结论：`S6-G14` 与 `S6-G15` 中属于 F1 的验收项已取得逐项证据；**本记录是 F1 批次退出，不是 Stage 6 关闭，也不构成 owner acceptance**。

## 1. 候选、版本与基线

| 项 | 值 |
|---|---|
| 基线 master | `46b65d1f2345f543b98e7e87fe5ec9ed735f10bf` |
| 代码末端 SHA | `d7980bd9b5cf7730889157ea8ec093799d53d7fc`（`fix(stage6): attach the instrumentation arguments to the reference host test`） |
| 最终候选 SHA | `c80a5b5f43a6aa7021037aaf2e308e2b9ec3d764`（`docs(stage6): exit S6-E1/E2 with mapped acceptance evidence`） |
| 候选相对父提交 | 4 个文档文件，`+250 / -8`；无 C++、构建输入、preset、CI 或检查器改动 |
| 版本 | `26.09.27-1`（build `1`，preset `debug` 下显示 `26.09.27-1-dev`） |
| SDK API | `0.7.0`（未变更） |
| 版本门禁 | `version.gate.pass`，`base=46b65d1f…`、`candidate=c80a5b5f…`、`trusted_utc_date=2026-09-27` |

候选 SHA 只在其代码末端之上追加文档提交，**代码内容等同 `d7980bd`**：§4 的本地矩阵、§6 的设备证据与 §7 的三平台 hosted 证据因此落在同一份代码上。§10 单独说明报告提交与实现 SHA 的差异。

## 2. `S6-G14`：兼容、架构、许可证与候选隔离没有回归

| 验收项（plan.md:457） | 证据 | 结果 |
|---|---|---|
| v1-v4 / FrameDigest golden | 默认、shared、MinGW 与 media-tools 配置的完整 CTest 全绿（734 / 737 / 776，§4），其中 Chart v1-v4 路由、typed 记录与 FrameDigest 家族均实际运行；E1/E2 的 canonical golden 在 media-tools 配置（776）与 hosted `Clang ASan + UBSan media-tools`（680）中运行 | 通过 |
| 头泄漏 / ASCII | 每个本地 preset 均运行 `cuexis_architecture_tests`（安装头不得暴露 EnTT/SDL/OpenGL/JSON DOM 等）与 7 条外部消费者门禁；hosted Linux 与 Windows 各自运行同一批目标（§7） | 通过 |
| developer-tools OFF | `tests/external/add_subdirectory*` 三处嵌套门禁强制 `CUEXIS_BUILD_DEVELOPER_TOOLS=OFF`，并在 §4 的**每一个** preset 中配置、构建、运行成功；本地 `headless-debug`（`CUEXIS_BUILD_DEVELOPER_TOOLS: OFF`，`CMakePresets.json:102`）另以 640/640 全绿通过，其配置输出明确打印 `cuexis_cxc_tests: developer tool layer absent; candidate CXC cases excluded`——即该配置确实会少注册用例，因此它**不作为**这些用例的覆盖来源，覆盖由全量配置（734/737/776）与 hosted 插桩/覆盖作业承担（§5、§7） | 通过 |
| shader-tools OFF | 默认与媒体配置（`CUEXIS_BUILD_SHADER_TOOLS` 未开启）：734 / 737 / 776 全绿；hosted `Clang ASan + UBSan`（638，着色工具关闭）全绿 | 通过 |
| shader-tools ON | 本地 `debug-shader-tools` 766/766；hosted `Clang ASan + UBSan shader-tools` 669、`GCC Shader Tools Coverage` 662 | 通过 |
| 候选隔离 | 默认配置不定义 `CUEXIS_ENABLE_CHART_V5_CANDIDATE`；开启后单独构建并运行 737/737（§5） | 通过 |
| 许可证 | `cuexis_player_distribution` 门禁通过；分发包含 10 份许可证文本、`THIRD_PARTY_NOTICES.md`、`NOTICE`、`LICENSE`（§8） | 通过 |

## 3. `S6-G15`：最终代码拥有完整同 SHA 验证（F1 承担部分）

| 验收项（plan.md:458） | 证据 | 结果 |
|---|---|---|
| 三平台 hosted 同 SHA | Linux Quality、Windows MSVC、Windows MinGW 的 push 与 pull_request 共 6 个 run，加上 Version Gate 共 7 个 run 全部 success，SHA 均为 `c80a5b5`（§7） | 通过 |
| GPU / 设备补充 | GUI/GPU 与真实音频设备 smoke 单列环境、命令与结果（§6） | 通过（含一条间歇 underrun 观察，见 §6） |
| 问题 1/2/3 处置与 owner 接受 | 属 `S6-F2`，**未执行**；本记录不主张 | 未执行（F2） |

## 4. 本地全量矩阵（fresh configure + `--clean-first`）

驱动：`out/f1-local-matrix.ps1`，每个 preset 依次执行 `cmake --preset <p> --fresh` → `cmake --build --preset <p> --clean-first` → `ctest --preset <p> --no-tests=error`，日志在 `out/f1/<preset>.{configure,build,test}.log`。

| preset | 配置 | 构建 | 测试 | 用例 | 耗时 | 跳过 |
|---|---|---|---|---|---|---|
| `debug` | 通过 | 通过 | 通过 | 734/734 | 9.4 min | 1 |
| `release` | 通过 | 通过 | 通过 | 734/734 | 11.0 min | 1 |
| `shared-debug` | 通过 | 通过 | 通过 | 737/737 | 9.8 min | 3 |
| `shared-release` | 通过 | 通过 | 通过 | 737/737 | 10.4 min | 3 |
| `debug-media-tools` | 通过 | 通过 | 通过 | 776/776 | 9.1 min | 1 |
| `debug-shader-tools` | 通过 | 通过 | 通过 | 766/766 | 17.8 min | 1 |
| `headless-debug`（adapter 关闭） | 通过 | 通过 | 通过 | 640/640 | 7.2 min | 1 |
| `mingw-debug` | 通过 | 通过 | 通过 | 734/734 | 16.4 min | 1 |

- 每个 preset 均以 `--fresh` 重新配置、以 `--clean-first` 重新构建，未使用任何未重建的旧生成头。
- 每个 preset 实际运行的门禁：`cuexis_reference_host_staging`、`cuexis_player_distribution` 各 1 项，外部消费者门禁 7 项；媒体 CLI 门禁 `cuexis_media_importer_tool_tests` 只在媒体配置中注册并运行（`debug-media-tools` 1 项），未注册的配置不计入该项覆盖。
- `mingw-debug` 首次运行失败的原因是驱动脚本在 MSVC 环境中调用该 preset，CMake 因而选择 `cl.exe` 与 `x64-mingw-static` 三元组组合，被 `CMakeLists.txt:17` 的正确守卫拒绝；改为干净环境（`C:\msys64\ucrt64\bin` 的 `g++`）后配置、构建、测试全部通过。这是驱动脚本的调用问题，不是仓库缺陷。
- headless/adapter 两个方向都有本地证据：适配器可用配置为 `debug`、`release`、`shared-debug`、`shared-release`、`debug-media-tools`、`debug-shader-tools`（734–776 全绿），适配器关闭配置为 `headless-debug`（640/640 全绿）。

## 5. candidate 开关与工具隔离

- `CUEXIS_ENABLE_CHART_V5_CANDIDATE=ON`（`out/f1-candidate.ps1`，`out/build/f1-candidate`）：配置、构建、测试全部通过，**737/737**，`Total Test time 708.54 s`，耗时 16.5 min。
- 与默认 `debug` 的用例差异：新增 8 项候选路径用例（含 `Candidate lowering remaps parents into execution-id order`、`Candidate prepared identity matches the Stage 6 golden and rejects duplicates`、`Explicit project candidate entry prepares the packed chart rather than v4`、`Candidate prepare failure leaves the active session unchanged` 等），替换 5 项 chart v4 时代的 Reader 用例，净 +3。
- hosted preset 不设置该开关，因此候选路径与 lowering 的**开关开启**配置由本地 §5 与 §4 的默认配置共同覆盖；hosted 侧负责的是同一批 candidate/lowering/config/media 目标在 sanitizer 与 coverage 配置下的注册与运行（§7）。
- 工具隔离的另一半证据是开发者工具关闭：见 §2 的嵌套外部消费者门禁。
- 工具关闭会**实际减少注册用例**（`headless-debug` 640 对全量 734，配置输出给出 `candidate CXC cases excluded`），本记录不把这种"总体绿色"当作被排除用例的覆盖；被排除用例的覆盖来源是全量配置（734/737/776）、candidate 开关配置（737）与 hosted 插桩/覆盖作业（§7）。

## 6. GPU / 真实音频设备 smoke（单列证据）

环境（记录于 `out/f1/smoke-environment.txt`）：

| 项 | 值 |
|---|---|
| SHA | `c80a5b5f43a6aa7021037aaf2e308e2b9ec3d764` |
| 系统 | Windows 11 家庭版 build 26200（64 位） |
| CPU / 内存 | AMD Ryzen 7 8845H（8 核 / 16 线程）、31.3 GB |
| GPU | AMD Radeon 780M（驱动 31.0.22032.1）、NVIDIA GeForce RTX 4060 Laptop（驱动 32.0.16.1664，2560×1600 @ 240 Hz） |
| 音频端点 | NVIDIA High Definition Audio、AMD High Definition Audio Device、Senary Audio、Nahimic mirroring device、Steam Streaming Speakers |
| 二进制 | `out/build/debug/bin/cuexis_player.exe`（11 948 032 字节，由 §4 的 fresh + clean-first 构建产生） |

命令与结果：

| 证据 | 命令 | 退出码 | 结果 |
|---|---|---|---|
| GPU 窗口/渲染 | `cuexis_player.exe --smoke-test` | 0 | 完成 6 帧；第 5 帧 `pixel=51,32,71,192`、`summary=18316288860163381829`、`render_us=591.4`，与 E1 冻结值一致；重载后 `Successful reload activated a complete OpenGL presentation cache` |
| 真实音频设备 | `cuexis_player.exe --audio-smoke-test` | 0 | 完成 90 帧；`Two-second pause preserved the audio clock`、`Failed reload preserved active playback and audio state`、`Reload transaction completed` |

- 音频终态为 `playing, queue: 4800 frames, discontinuity: 4, underruns: 0–1`：空载复跑 3 次的 underruns 依次为 1、1、0，即**间歇**现象；三次运行均退出码 0、90 帧、暂停/重载断言全部通过。本记录如实保留该观察，不将 headless 或单次 success 等同于音频通过，也不把它记为确定性失败。
- 两次 smoke 结束后均确认无遗留 `cuexis_player` 进程。

## 7. hosted 同 SHA 证据

SHA `c80a5b5`，7 个 run 全部 success（时间为 UTC）：

| workflow | 事件 | run id | 区间 | 耗时 | 结论 |
|---|---|---|---|---|---|
| Linux Quality | push | 36323246316 | 13:41:04 → 14:11:13 | 30.2 min | success |
| Linux Quality | pull_request | 36323249366 | 13:41:07 → 14:16:57 | 35.8 min | success |
| Windows MSVC | push | 36323246298 | 13:41:04 → 14:29:00 | 47.9 min | success |
| Windows MSVC | pull_request | 36323249392 | 13:41:07 → 14:32:45 | 51.6 min | success |
| Windows MinGW | push | 36323246305 | 13:41:04 → 14:28:07 | 47.0 min | success |
| Windows MinGW | pull_request | 36323249349 | 13:41:07 → 14:38:22 | 57.2 min | success |
| Version Gate | pull_request | 36323249354 | 13:41:07 → 13:41:16 | 0.2 min | success |

Linux Quality（push run）各作业整包用例数，均为 `100% tests passed, 0 tests failed`：

| 作业 | 用例 | 作业 | 用例 |
|---|---|---|---|
| Clang ASan + UBSan | 638 | GCC Coverage | 631 |
| Clang ASan + UBSan media-tools | 680 | GCC Adapter Coverage | 677 |
| Clang ASan + UBSan shader-tools | 669 | GCC media-tools | 680 |
| Clang Shared Debug | 655 | GCC Release | 654 |
| GCC Shader Tools Coverage | 662 | GCC Shared Release | 639 |

`clang-tidy` 与 `Documentation contracts` 不含 CTest 套件（静态分析与文档契约作业）。

Windows（MSVC 与 MinGW，push 与 pull_request 四个作业）都按预设跑整包加聚焦目标：整包计数为 `debug` 734、`release` 734、`debug-media-tools` 776，另有 `cuexis_architecture_tests` 与 `cuexis_cfu_f3_determinism` 的聚焦运行；日志中的 `Performing Test … - Failed`、`Looking for pthread_create … not found` 是 CMake 探测输出，含 "failed" 字样的行多为用例名（如 `R4 a failed replacement removes its temporary file`，结论 Passed），不是失败。

插桩与覆盖作业确实编译并运行了本阶段新增的 target 家族（在同一候选 SHA 上核对；用例名计数来自日志行，是下界，整包计数为上表权威值）：

| 作业 | 整包 | candidate/PB | lowering/typed | 配置/设备 | 媒体 |
|---|---|---|---|---|---|
| Clang ASan + UBSan | 638 | 26 | 24 | 15 | 1 |
| GCC Coverage | 631 | 26 | 23 | 14 | 0 |
| GCC Adapter Coverage | 677 | 27 | 26 | 23 | 1 |
| Clang ASan + UBSan media-tools | 680 | 30 | 23 | 14 | 14 |
| GCC media-tools | 680 | 30 | 25 | 14 | 14 |

因此不存在"developer-tools OFF 导致测试未注册、总体仍绿"的覆盖空洞：候选校验、lowering、配置与媒体用例在 sanitizer/coverage 作业中均有实际注册与运行记录，媒体目标在媒体作业中运行。

## 8. 文档、版本与许可证证据

| 项 | 证据 | 结果 |
|---|---|---|
| 文档契约 | `python -B tools/check_docs.py`：258 个 Markdown 文件与 20 个 candidate JSON/CXT 通过 | 通过 |
| 版本一致性 | `python -B tools/update_version.py --check`：`26.09.27-1` | 通过 |
| 空白/换行 | `git diff --check` 无输出 | 通过 |
| 许可证 | `cuexis_player_distribution` 门禁 Passed（4.01 s），分发目录 `cuexis-player-26.09.27-1-dev-windows-static-debug` | 通过 |
| 分发包内容 | `licenses/` 10 份（entt、fmt、glad、glm、json-schema-validator、minizip-ng、nlohmann-json、sdl3、spdlog、tl-expected）、`THIRD_PARTY_NOTICES.md`、`NOTICE`、`LICENSE`、`README.txt`、`VERSION.txt`、`cuexis_player.exe` 与 4 个运行库 | 通过 |
| 包身份 | `VERSION.txt`：`display_version=26.09.27-1-dev`、`sdk_api_version=0.7.0`、`executable=cuexis_player.exe`、`resources=assets`、`runtime_libraries=4` | 通过 |

`out/build/debug/dist` 下曾同时存在上一版本（`26.09.26-1`）的旧分发目录，已删除；保留的目录与 `VERSION.txt` 均对应当前 `26.09.27-1`，避免用旧产物充当新版本证据。

## 9. 跳过项与未执行项

跳过项（全部为预先记录的设计性跳过，非失败，也不是覆盖）：

| 用例 | 条件 | 说明 |
|---|---|---|
| `Secure file rejects physical containment escapes through symlinks` | Windows 平台条件 | 静态与共享配置均跳过；Foundation R5 与 g3 记录同一跳过 |
| `Playback v4 prepare reuses the initial Chart parse` | `CUEXIS_PLAYBACK_PARSE_COUNT_PROBE_UNAVAILABLE` | 仅共享库配置跳过（parse 探针在 DLL 下不可见）；静态 `debug` / `release` / `mingw-debug` 与候选开关配置中实际运行 |
| `PB-08 owning-copy queries contain allocation failures at the Playback boundary` | `CUEXIS_PLAYBACK_ALLOCATION_TEST_SHARED` | 仅共享库配置跳过（故障注入替换的是测试可执行文件的分配器）；静态配置中实际运行 |

未执行项：

- `S6-F2`（关闭报告、问题 1/2/3 处置、Stage 7A/Stage 8 交接、owner 接受）：未开始，本记录不主张。
- 本地 Linux sanitizer/coverage：Windows 主机无法运行这些预设（`CMakeLists.txt` 明确要求 Clang/GCC），其覆盖由 §7 的 hosted 作业承担；本机 WSL 复现路径此前仅作为调试工具保留，不构成本次验收证据。
- 音频设备 smoke 的间歇 underrun（0–1）未定位到确定性成因，作为观察保留（§6）。

## 10. 报告提交与实现 SHA 的差异

- 本记录描述的实现 SHA 是代码末端 `d7980bd`，最终候选为文档提交 `c80a5b5`（§1）；三平台 hosted 与本地矩阵均在 `c80a5b5` 上取得，覆盖同一份代码。
- 本记录自身由后续文档提交承载，只新增/更新 `docs/` 下文件，不修改检查器、工作流、preset、构建输入或测试，因此按 plan.md:417-419 不触发实现级重跑。承载提交 `95bfa70343e1628e49b292ba994e7ba7e2c55a32` 的 hosted 结果已取得并全部 success：Linux Quality push `36328047694`（29.8 min）与 pull_request `36328051358`（36.6）、Windows MSVC push `36328047772`（38.9）与 pull_request `36328051360`（48.0）、Windows MinGW push `36328047781`（46.4）与 pull_request `36328051305`（49.2）、Version Gate `36328051306`（0.2），共 7 个 run。
- 尚无影响检查器或 CI 的改动被归入纯叙述文档。

## 11. 证据清单

本地（随 `out/` 忽略，不入版本库）：

- `out/f1-local-matrix.ps1`、`out/f1-candidate.ps1`、`out/f1-smoke.ps1`、`out/f1-watch.ps1`
- `out/f1/<preset>.{configure,build,test}.log`（7 个 preset）
- `out/f1/candidate.{configure,build,test}.log`
- `out/f1/smoke-environment.txt`、`out/f1/smoke-gpu-window.log`、`out/f1/smoke-audio-device.log`
- `out/f1/distribution-verbose.log`、`out/f1/msvc-debug-hosted.log`

hosted：

- Linux Quality push `36323246316`、pull_request `36323249366`
- Windows MSVC push `36323246298`、pull_request `36323249392`
- Windows MinGW push `36323246305`、pull_request `36323249349`
- Version Gate pull_request `36323249354`

上游批次退出记录：E1/E2 见 [退出记录](2026-09-27-s6-e1-e2-exit.md) 与 [实现报告](2026-09-25-s6-e1-e2-media-importer.md)，E3 见 [退出记录](2026-09-26-s6-e3-exit.md)，C4 见 [退出记录](2026-09-27-s6-c4-exit.md) 与 [实现报告](2026-09-27-s6-c4-reference-host-and-player-distribution.md)。
