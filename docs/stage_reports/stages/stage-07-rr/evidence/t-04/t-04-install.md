# T-04 安装消费者三档验收（含 minimal 等价与 tools=ON 附加档）

状态：evidence

<!-- cuexis-meta
id: EVIDENCE-stage-07-rr.t-04-install
status: evidence
authority: none
date: 2026-11-24
-->

更新日期：2026-11-24
仓库：`E:\Cuexis-worktree`，提交 `3943fb1365853b6f5af5c8a41ef322d147863ee6`（`git status` 干净）
SDK API：`0.9.1`；构建类型 `Debug`；生成器 `Ninja`；工具链 MSVC 19.51.36256（vcvars64）
写域：本目录及其下 5 个安装前缀；未改生产源码，未 git add/commit，安装只读既有构建目录。

## 判定

**成立。** 四档前缀的安装清单、导出 target、泄漏检查和独立消费者配置-构建-运行全部通过，另补验默认 `tools=ON/测试=ON` 的安装前缀。

| 档位 | 构建目录（配置） | 安装前缀 | 安装 | 清单/导出 | 消费者（两种接线） | 判定 |
|---|---|---|---|---|---|---|
| 默认 static | `out/build/headless-debug`（candidate OFF，tools OFF，tests ON） | `prefix-static` | 0 | 通过 | 配置/构建/运行均 0 | 成立 |
| 实验 static | `out/build/candidate-headless-debug`（candidate ON，tools ON） | `prefix-candidate-static` | 0 | 通过 | 配置/构建/运行均 0 | 成立 |
| 实验 shared | `out/build/candidate-shared-debug`（candidate ON，SHARED，tools ON） | `prefix-candidate-shared` | 0 | 通过 | 配置/构建/运行均 0 | 成立 |
| minimal 等价 | `out/build/rr-f302-minimal`（candidate OFF，tools OFF，tests OFF） | `prefix-minimal` | 0 | 与默认 static 文件清单逐项一致 | 配置/构建/运行均 0 | 成立 |
| 附加：默认 tools=ON | `out/build/debug`（candidate OFF，tools ON，tests ON） | `prefix-static-tools` | 0 | 通过 | 配置/构建/运行均 0 | 成立 |

失败项：无。

## 判据与接线

判据来自任务卡 `docs/stage_plans/active/stage-07-rr/tasks/T-04.md` 第 48、61 行：默认 static 保留 `Cuexis::InternalChart`/`Cuexis::InternalJudgement` 且不导出/安装 Chart codec/tools、`cxc_candidate`；实验 static 必须含实际闭包的 `Cuexis::InternalChartCodec`/`Cuexis::InternalChartTools`；shared 只导出公共库；安装头与 `.cmake` 不得出现构建树绝对路径；默认 tools=ON/测试=ON 仍不得泄漏作者归档。

仓库消费者入口的实际参数如下。

- `cmake/VerifyExternalConsumer.cmake`：必需 `CUEXIS_CONSUMER_MODE`、`CUEXIS_SOURCE_DIR`、`CUEXIS_BINARY_DIR`、`CUEXIS_GENERATOR`、`CUEXIS_BUILD_TYPE`、`CUEXIS_LIBRARY_TYPE`、`CUEXIS_SDK_API_VERSION`，支持 `add_subdirectory[_playback|_audio_sdl]` 与 `find_package[_playback|_core|_audio_sdl]`。它没有“已安装前缀”参数：`find_package` 分支会用 `CUEXIS_SOURCE_DIR` 重新配置、构建并安装一份自己的包到 `${CUEXIS_BINARY_DIR}/ec/<id>/package`。因此本目录按该脚本在 `find_package` 分支生成的同一套消费者工程与参数，仅把 `Cuexis_DIR`/`CMAKE_PREFIX_PATH` 换成待验前缀，分别运行 `find_package_playback`（Playback-only）与 `find_package`（Playback+Content+Audio）两种接线，fixture 目录也按脚本 `file(COPY)` 的同一套内容预生成。
- `cmake/VerifyGameplayInstalledConsumer.cmake`：必需 `CUEXIS_SOURCE_DIR`、`CUEXIS_BINARY_DIR`、`CUEXIS_PLAYBACK_TESTS`、`CUEXIS_GENERATOR`、`CUEXIS_BUILD_TYPE`、`CUEXIS_CXX_COMPILER`。它用 `cmake --install "${CUEXIS_BINARY_DIR}"` 把当前构建目录安装到随机临时前缀，再运行 `tests/external/find_package_gameplay`，还要求 `cuexis_playback_tests` 可执行文件与 `<binary>/s7a78-public-fixture/main.packed`。它同样不能指向预建前缀；两个 candidate 构建目录都没有该测试可执行文件与 fixture（已核对），且本任务只允许写入本目录。因此对它只执行前缀侧断言：`include/cuexis/playback/gameplay_candidate.hpp` 存在，`judgement`/`gameplay_packed`/`runtime`/`world` 头目录不存在（两个 candidate 前缀均满足）。
- `cmake/VerifyChartInstalledConsumer.cmake`：不存在。

消费者通用配置参数（每档替换前缀、vcpkg 根与库类型）：

```text
cmake -S E:/Cuexis-worktree/tests/external/<find_package_playback|find_package> -B <work> -G Ninja
  -DCMAKE_BUILD_TYPE=Debug
  -DCMAKE_CXX_COMPILER="D:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/cl.exe"
  -DCMAKE_MAKE_PROGRAM=<headless-debug 缓存中的 ninja>
  -DCMAKE_RC_COMPILER=<rc.exe> -DCMAKE_MT=<mt.exe>
  -DCMAKE_TOOLCHAIN_FILE="D:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake"
  -DVCPKG_TARGET_TRIPLET=x64-windows -DVCPKG_MANIFEST_MODE=OFF
  -DVCPKG_INSTALLED_DIR=<该档构建目录>/vcpkg_installed
  -DCUEXIS_SOURCE_DIR=E:/Cuexis-worktree -DCUEXIS_FIXTURE_DIR=<install>/work/fixture
  -DCUEXIS_EXPECTED_SDK_API_VERSION=0.9.1 -DCUEXIS_EXPECTED_LIBRARY_TYPE=<STATIC|SHARED>
  [-DCuexis_ALLOW_EXPERIMENTAL=ON -DCUEXIS_ENABLE_CHART_V5_CANDIDATE=ON -DCUEXIS_VERSION_SUFFIX=exp.candidate]
  -DCuexis_DIR=<install>/<prefix>/lib/cmake/Cuexis
  -DCMAKE_PREFIX_PATH=<该档构建目录>/vcpkg_installed/x64-windows
```

配置后的构建与运行：

```text
cmake --build <work> --target cuexis_external_consumer
ctest --test-dir <work> -V
```

运行前把 `<prefix>/bin`、`<vcpkg>/x64-windows/debug/bin`、`<vcpkg>/x64-windows/bin` 加到 `PATH`；shared 的消费者 DLL 依赖由 `<prefix>/bin` 提供。全部命令通过 `cmd /d /c "call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && <命令>"` 执行；每个日志首行保存完整命令。

fixture 使用脚本同款集合：`assets/projects/stage3_project`、`tests/fixtures/chart_format_update/{cfu_f_reference_project,parameterized_project,source_project}`、`tests/fixtures/chart_format_update/golden/{cfu_f_v4_reference.cxc,cxc_v1_v4_cxt.cxc}`、`tests/fixtures/chart_format_update/valid/chart_v4_parameterized_rational.json`，复制到 `work/fixture`。未额外生成 typed fixture。

## 档 1：默认 static（headless-debug → prefix-static）

先按安装清单构建归档（`CUEXIS_STATIC_IMPLEMENTATION_TARGETS` 加公共目标）：

```text
cmake --build out/build/headless-debug --target cuexis_core cuexis_audio cuexis_content cuexis_playback
  cuexis_filesystem cuexis_json_support cuexis_project cuexis_world cuexis_assets cuexis_chart
  cuexis_animation cuexis_cxc cuexis_behavior cuexis_judgement cuexis_render cuexis_runtime
```

退出码 0（`build-headless-debug-install-targets.log`；本次补齐了此前缺失的 `cuexis_judgement.lib`）。

```text
cmake --install out/build/headless-debug --prefix out/review/stage-07-rr/T-04/install/prefix-static
```

退出码 0，58 个文件：16 个静态归档、26 个公共头、4 个 CMake 文件、8 个许可证与 LICENSE/NOTICE/THIRD_PARTY_NOTICES/build-flavor。清单 `manifest-prefix-static.txt`，安装日志 `install-prefix-static.log`。

导出 target（`lib/cmake/Cuexis/CuexisTargets.cmake`）：`Cuexis::Core`、`Cuexis::Audio`、`Cuexis::Content`、`Cuexis::Playback`，以及 `InternalAnimation`、`InternalAssets`、`InternalBehavior`、`InternalChart`、`InternalCxc`、`InternalFilesystem`、`InternalGameplay`、`InternalJsonSupport`、`InternalJudgement`、`InternalProject`、`InternalRender`、`InternalRuntime`、`InternalWorld`。`Cuexis::InternalChart` 与 `Cuexis::InternalJudgement` 存在；`InternalChartCodec`、`InternalChartTools`、`InternalCxcCandidate`、`InternalGameplayPacked` 均不存在，`CuexisTargets`/头文件中对这些名字的引用为 0 处。`Cuexis::Playback` 的 `INTERFACE_LINK_LIBRARIES` 含 `$<LINK_ONLY:Cuexis::InternalCxc>`。

消费者结果（`consumer-static-pp-*`、`consumer-static-fp-*`）：

| 接线 | configure | build | run | 可执行文件实际输出 |
|---|---|---|---|---|
| find_package_playback | 0 | 0 | 0 | `Cuexis Playback-only CFU-F2 consumer passed identity=6d01494c...100cc5 stop_digest=11596562486377158370`；`S4-F animation_identity=fb662e...e9b4 animation_digests=105060921077611920,...` |
| find_package | 0 | 0 | 0 | `Cuexis external consumer passed digest=6442711505793857933 validation=18316288860163381829` |

构建日志显示消费者编译包含前缀内 `include/cuexis/...` 头，未使用源码树头。

## 档 2：实验 static（candidate-headless-debug → prefix-candidate-static）

构建目标在档 1 基础上加 `cuexis_gameplay_packed cuexis_cxc_candidate cuexis_chart_codec cuexis_chart_tools`，退出码 0（`build-candidate-headless-debug-install-targets.log`）。

```text
cmake --install out/build/candidate-headless-debug --prefix out/review/stage-07-rr/T-04/install/prefix-candidate-static
```

退出码 0，65 个文件：20 个静态归档（含 `cuexis_chart_codec.lib`、`cuexis_chart_tools.lib`、`cuexis_cxc_candidate.lib`、`cuexis_gameplay_packed.lib`）、29 个公共头、4 个 CMake 文件、8 个许可证与文档。清单 `manifest-prefix-candidate-static.txt`。

导出 target 含 `Cuexis::InternalChart`、`Cuexis::InternalJudgement`、`Cuexis::InternalChartCodec`、`Cuexis::InternalChartTools`、`Cuexis::InternalCxcCandidate`、`Cuexis::InternalGameplayPacked`。`Cuexis::Playback` 的 `INTERFACE_LINK_LIBRARIES` 以 `$<LINK_ONLY:...>` 带上 `InternalJudgement`、`InternalGameplayPacked`、`InternalCxcCandidate`、`InternalChartCodec`、`InternalChartTools`、`InternalChart`、`InternalCxc`，说明 codec/tools 属于实际链接闭包。codec/tools 头是构建侧头，未进入安装包：安装头只有公共 `cuexis/{core,audio,content,playback}/*.hpp`（29 个，含 `gameplay_candidate.hpp`），没有 `cuexis/chart`、`codec_include`、`tools_include`。

消费者结果（`consumer-candidate-static-*`）全部 configure/build/run = 0，实际输出与档 1 同值（Playback-only identity `6d01494c...`、stop_digest `11596562486377158370`；完整消费者 digest `6442711505793857933`/`validation=18316288860163381829`）。

## 档 3：实验 shared（candidate-shared-debug → prefix-candidate-shared）

先构建公共动态库与（该 preset 开启的）`cuexis_audio_sdl`，退出码 0（`build-candidate-shared-debug-install-targets.log`）。

```text
cmake --install out/build/candidate-shared-debug --prefix out/review/stage-07-rr/T-04/install/prefix-candidate-shared
```

退出码 0，61 个文件：6 个 DLL（`cuexis_{core,audio,content,playback,audio_sdl}-candidate-0.9d.dll` 与 `SDL3.dll`）、5 个导入库、32 个公共头、6 个 CMake 文件、8 个许可证与文档。清单 `manifest-prefix-candidate-shared.txt`。

`CuexisTargets.cmake` 只有 `Cuexis::Core`、`Cuexis::Audio`、`Cuexis::Content`、`Cuexis::Playback`，不含任何 `Cuexis::Internal*`（`Cuexis::Internal` 匹配为 false）。与 `prefix-candidate-static` 的清单对比（`manifest-diff-candidate-static-vs-shared.txt`）：shared 去掉 16 个内部归档、4 个静态公共归档与 minizip 许可证，换成 5 组 DLL/导入库，并新增公共 `audio_sdl` 适配器与其头；安装包内 `cuexis_chart*`、`cuexis_judgement*`、`cuexis_cxc_candidate*`、`cuexis_gameplay_packed*` 等内部名字均为 0 处。`CuexisTargets` 中 `EnTT::`、`glm::`、`nlohmann_json::`、`MINIZIP::`、`minizip-ng`、`spdlog::`、`glad::` 均不出现；`CuexisConfig` 不含 `MINIZIP::`/`minizip-ng` 或 OpenGL 依赖；没有 `cuexis_{assets,runtime,world}*.dll` 等内部运行时。

消费者结果（`consumer-candidate-shared-*`）configure/build/run = 0；运行时从 `prefix-candidate-shared/bin` 加载 DLL，实际输出与档 1 同值。

## 档 4：minimal 等价（rr-f302-minimal → prefix-minimal）

该配置 `BUILD_TESTING=OFF`、`CUEXIS_BUILD_TESTS=OFF`、`CUEXIS_BUILD_DEVELOPER_TOOLS=OFF`、candidate OFF，没有 CTest 注册，仍按同一流程执行。构建目标与档 1 相同，退出码 0（`build-rr-f302-minimal-install-targets.log`）。

```text
cmake --install out/build/rr-f302-minimal --prefix out/review/stage-07-rr/T-04/install/prefix-minimal
```

退出码 0，58 个文件，与 `prefix-static` 的 58 个文件路径逐项一致（`manifest-diff-static-vs-minimal.txt` 为空）；导出 target 集合、公共头集合、许可证集合也一致。归档文件哈希不同，原因是两次独立构建的目标文件内嵌路径/时间戳，不影响清单与接口等价。消费者结果（`consumer-minimal-*`）configure/build/run = 0，实际输出与档 1 同值。

## 附加档：默认 tools=ON/测试=ON（debug → prefix-static-tools）

用于覆盖任务卡第 61 行“默认 tools=ON/测试=ON 仍不得把作者归档泄入默认 SDK”。安装退出码 0，65 个文件（`install-prefix-static-tools.log`、`manifest-prefix-static-tools.txt`）。导出 target 与档 1 相同，无 `InternalChartCodec`/`InternalChartTools`/`InternalCxcCandidate`/`InternalGameplayPacked`；安装包内也没有 `cuexis_gameplay_author`、`cuexis_asset_publish`、`cuexis_cxc_tool_common`、`cuexis_execution_host_support` 等作者/工具归档。该 preset 开启公共 `audio_sdl` 适配器，因此多了 3 个 `cuexis/audio_sdl` 头、`CuexisAudioSDLTargets*.cmake`、`cuexis_audio_sdl.lib` 与 `sdl3-copyright.txt`，属于公共库而非作者归档。两种消费者均 configure/build/run = 0（`consumer-static-tools-*`）。

## 泄漏检查汇总

对每个前缀的 `.cmake`、`.hpp`、`.txt`、`.md` 全量扫描构建树/源码树绝对路径，模式 `E:/Cuexis-worktree/out/build`、`E:\Cuexis-worktree\out\build`、`E:/Cuexis-worktree`、`E:\Cuexis-worktree`：

| 前缀 | 扫描文本文件 | 构建树/源码树绝对路径命中 | 内部头目录 | 内部/禁止归档 |
|---|---|---|---|---|
| `prefix-static` | 38 | 0 | 无 | 无 codec/tools/cxc_candidate/gameplay_packed |
| `prefix-candidate-static` | 41 | 0 | 无 | 仅含应导出的 codec/tools/cxc_candidate/gameplay_packed |
| `prefix-candidate-shared` | 46 | 0 | 无 | 无任何内部归档或内部运行时 |
| `prefix-minimal` | 38 | 0 | 无 | 无 codec/tools/cxc_candidate/gameplay_packed |
| `prefix-static-tools` | 44 | 0 | 无 | 无 codec/tools/cxc_candidate/gameplay_packed 与作者/工具归档 |

默认 static、minimal、tools=ON 三个前缀的 `.cmake` 与头文件中 `InternalChartCodec|InternalChartTools|InternalCxcCandidate|InternalGameplayPacked|cuexis_chart_codec|cuexis_chart_tools|cuexis_cxc_candidate|cuexis_gameplay_packed` 名字引用均为 0。安装 flavor 戳分别为 production、experimental、production、production、production，符合安装脚本的前缀混用保护。

## 证据索引

| 文件 | 内容 |
|---|---|
| `manifest-prefix-*.txt` | 5 个前缀的完整文件清单 |
| `manifest-check.log` | 导出 target、孤立/禁止归档、内部头、构建树路径、许可证、shared 私有依赖、flavor、消费者输出等检查记录 |
| `manifest-diff-static-vs-minimal.txt` / `manifest-diff-candidate-static-vs-shared.txt` | minimal 等价对照 / shared 与 static 清单差异 |
| `install-prefix-*.log` | 5 次安装的完整输出与退出码 |
| `build-*-install-targets.log` | 各配置按安装清单构建的日志与退出码 |
| `consumer-<档>-<pp|fp>-{configure,build,run}.log` | 每档两种接线的完整命令、退出码与 `ctest -V` 实际运行输出 |
| `work/consumers/**` | 消费者构建目录与可执行文件；`work/fixture` 为脚本同款 fixture |

5 档 × 2 种接线共 10 组 configure/build/run，退出码全部为 0。
