# S6-C4 参考宿主与 Player 分发

状态：进行中（实现与本地门禁证据；批次退出结论、本地 shared 分发目录与 hosted 复验尚未完成）

日期：2026-09-27

显示版本：`26.09.27-1`（可信 UTC 日期进入 `2026-09-27` 后按 Version Gate 规则滚动，仅日期构建身份，
SDK API 仍为 `0.7.0`）

范围：Stage 6 计划 §S6-C4「SDK、真实宿主与安装升级」与 [ADR 0042](../../../adr/0042-stage-6-productization-boundaries.md)
§S6-D08。本报告是**实现与验证证据**，不是批次退出、不是 Stage 6 关闭，也不构成 owner acceptance。
SDK API 仍为 `0.7.0`。

## 1. 计划项对照

| §S6-C4 项 | 交付 | 证据 |
| --- | --- | --- |
| 1 具名参考宿主 | `examples/reference_host/`：独立宿主工程，自带主循环、`IContentProvider` 实现、宿主时钟与帧消费，只使用安装后的公共 Playback 头与导出 target | §3、门禁 `cuexis_reference_host_staging` |
| 2 生产宿主验证 v4，candidate 明确隔离 | 宿主通过 `fromCxcFile` 加载发布的 v4 `.cxc` 包，身份与参考帧摘要等于 CFU-F golden；candidate 开关不进入安装树 | §3.3、§5 |
| 3 独立 SDK 安装树与可运行 Player 分发目录 | `cuexis_player_dist` 生成一个 flavor 目录；`VERSION.txt` 记录版本与 flavor | §4 |
| 4 clean staging 演示与拒绝门禁 | staging 前缀安装 + 源树之外的宿主副本 + 清理 PATH 运行；toolchain 不一致被拒 | §3.2、§3.4 |
| 5 `0.7.0` 基线升级示例 | 宿主声明 `CUEXIS_HOST_API_VERSION=0.7.0`；升级流程与配置回滚边界写入 VERSIONING | §6 |

## 2. 交付清单

新增：

- `examples/reference_host/`（`CMakeLists.txt`、`README.md`、`src/host_report.{hpp,cpp}`、
  `src/host_content.{hpp,cpp}`、`src/host_runner.{hpp,cpp}`、`src/main.cpp`）。
- `cmake/VerifyReferenceHost.cmake`、`cmake/PackagePlayer.cmake`、
  `cmake/VerifyPlayerDistribution.cmake`。
- `app/player/CMakeLists.txt` 中的 `cuexis_player_dist` 自定义 target；
  `CMakeLists.txt` 中两个门禁的注册与 `cuexis_player_dist` 的 active-target 登记。

修改：`docs/guides/BUILDING.md`、`docs/guides/VERSIONING.md`、`docs/guides/DEPENDENCY_POLICY.md`、
`THIRD_PARTY_NOTICES.md`、`docs/architecture/PLAYER_APPLICATION.md`、
`docs/architecture/STAGE6_PRODUCTIZATION_BOUNDARIES.md`、`docs/proposals/STAGE6_API_AND_INSTALL_DRAFT.md`、
`docs/CURRENT_STATUS.md`、`docs/stage_plans/completed/stage-06/plan.md`。

没有改动任何公开头、签名、枚举语义、默认入口或 SDK API 版本；Playback 的链接闭包不变；新增的
`cuexis_player_dist` 是自定义 target，无编译单元。

## 3. 参考宿主

### 3.1 公共边界

`CMakeLists.txt` 只调用一次包查找，并拒绝非 `0.7.x` 的基线：

```cmake
find_package(Cuexis ${CUEXIS_HOST_API_VERSION} CONFIG REQUIRED COMPONENTS Playback)
```

宿主源码只包含 `<cuexis/playback/*.hpp>`；门禁用正则扫描全部宿主源文件与 CMakeLists，任何非
Playback 的 Cuexis include、`Cuexis::Internal|Core|Content|Audio`、`cuexis_cxc`、
`player_support`、`CUEXIS_SOURCE_DIR` 或 `../` 相对 include 都会失败。因此宿主不链接 Player 配置
实现，也不依赖源码树。

宿主自行提供：

- **主循环与时间**：`host_runner.cpp` 拥有时钟脚本（`ClockStep`），自己累积或跳转
  `chartTimeMs` 并维护 `timeDiscontinuityId`，只在跳变时递增；SDK 只接收
  `RuntimeFrame`。
- **ContentProvider**：`HostFileProvider` 直接实现 `cuexis::content::IContentProvider`，在一个宿主
  内容根下解析逻辑资源路径，拒绝非便携相对路径与越出根目录的请求，统计读取次数，并支持宿主侧
  故障注入（设备不可用时全部读取失败）。
- **帧消费**：每步 `update` + `extractFrame` + `computeFrameDigest`，记录帧摘要。
- **销毁**：`unload()` 后要求状态回到 `Empty`。

### 3.2 运行记录与判据

门禁把当前构建安装到 staging 前缀（`cmake --install <binary> --prefix <work>/prefix`），把示例
工程复制到 `<work>/host`，在副本上配置与构建，然后在 `PATH` 只含宿主运行时目录与系统目录、
`LD_LIBRARY_PATH` 只含宿主构建目录的环境下运行，工作目录在源树之外。运行记录（同时写入
`host-report.txt`）必须包含完整生命周期：

```text
host.start sdk_api_baseline=0.7.0 content=cfu_f_reference_project package=yes
host.load source=host-project identity=6d01494c... presentation_entries=4
host.commit state=Ready identity=6d01494c...
host.frame index=1 mode=seek chartTimeMs=625 discontinuityId=1 objects=2 digest=11596562486377158370 algorithm=3
host.digest index=1 expected=11596562486377158370 matched=yes
host.reload outcome=ok identity=6d01494c... state=Running frames_identical=yes
host.rejection step=reload-failed code=playback.session.prepare_failed
host.active identity=6d01494c... state=Running digest=2541756539773537692 preserved=yes
host.package outcome=ok identity=6d01494c... identity_matches_host_content=yes frames_identical=yes
host.destroy state=Empty resources_released=yes
host.summary outcome=ok steps=33
```

参考身份 `6d01494c126f3ae8fc9420259dc92873233022dec9dd6bf9caf04b217f100cc5` 与帧摘要
`11596562486377158370` 是 `tests/external/playback_consumer.cpp` 已在四平台断言的 CFU-F golden
（`expectedSemanticIdentity`、`expectedStopDigest`）。门禁独立复核这两个值，因此判据不依赖宿主
自身的实现。

### 3.3 生产 v4 与 candidate 隔离

宿主先加载宿主提供的 typed project 内容，再显式 `unload` 并加载发布的
`cfu_f_v4_reference.cxc` 包；两次的身份与全部帧摘要必须相等，记录为
`identity_matches_host_content=yes frames_identical=yes`。这把 E3 的发布产物与 C4 的宿主消费连成
一条链：同一个包在参考宿主中产生与作者侧内容相同的语义观测。

candidate 隔离的当前事实：`CUEXIS_ENABLE_CHART_V5_CANDIDATE` 只是 `cuexis_playback` 与其测试的
PRIVATE 编译定义，不进入安装树、不改变导出 target 或包元数据，也没有 consumer 可见开关；草案中的
`Cuexis_ALLOW_EXPERIMENTAL` opt-in 仍未实现，因此当前没有可被生产 consumer 误用的实验安装 flavor。
门禁扫描安装树的公共头，确认其中不含开发树路径；生产宿主的构建与运行本身也证明生产路径不需要
candidate 能力。

### 3.4 拒绝门禁

- **Toolchain**：shared 安装包记录 `Cuexis_COMPILER_ID`。门禁复制 prefix、把该值改写为
  `AnotherCompiler`（并断言改写确实发生），再要求宿主配置失败且输出包含
  `requires compiler AnotherCompiler`。static 安装的配置本身不带 toolchain 门禁，门禁会明确报告
  该用例只在 shared flavor 下运行，而不是假装通过。
- **API minor**：宿主在 configure 阶段拒绝 `0.7.x` 之外的接口版本。本地手动核对：以
  `-DCUEXIS_HOST_API_VERSION=0.8.0` 对 staging 安装前缀配置宿主时，`find_package(Cuexis 0.8.0 ...)`
  立即失败并停止 configure；`0.7.0` 是门禁中的正例（`CUEXIS_HOST_API_VERSION=0.7.0`）。该用例目前
  是手动核对，尚未注册为门禁用例。SDK 侧的 `0.6`/`0.8` 版本拒绝已由既有 find_package 门禁覆盖。
- **内容与参数**：`--expect-identity`、`--expect-digest` 不匹配即失败；被拒绝的宿主提供者故障
  必须保持活动身份、状态与帧摘要不变。
- **Player 分发**：缺失/多余的许可证文件、构建产物、SDK 安装树内容与 flavor 记录不一致都会失败。

## 4. Player 分发目录

`cmake/PackagePlayer.cmake` 由 `cuexis_player_dist` 驱动，输出
`out/build/<preset>/dist/cuexis-player-<display-version>-<system>-<linkage>-<build-type>/`：

| 内容 | 来源与判据 |
| --- | --- |
| `cuexis_player.exe` | `$<TARGET_FILE:cuexis_player>` |
| 运行时库 | `file(GET_RUNTIME_DEPENDENCIES)` 解析结果 + 可执行文件同目录 DLL；排除系统目录；shared 构建包含 Cuexis 运行时库 |
| `assets/` | 构建树的 Player 资源目录（charts、projects、schemas），默认资源位置 |
| `VERSION.txt` | `format`、`display_version`、`sdk_api_version`、`library_type`、`build_type`、`system_name`、`system_processor`、`compiler`、`executable`、`resources`、`runtime_libraries` |
| `README.txt` | 自包含说明与 flavor 混装禁令 |
| `LICENSE`、`NOTICE`、`THIRD_PARTY_NOTICES.md` | 仓库 notices |
| `licenses/` | entt、fmt、glad、glm、json-schema-validator、minizip-ng、nlohmann-json、sdl3、spdlog、tl-expected 的 vcpkg 版权文本 |

`cuexis_player_distribution` 门禁：打包 → 校验内容与元数据 → 拒绝 `.lib`/`.pdb`/`.ilk`/`CMakeCache.txt`
等构建产物与 SDK 安装树 → 精确校验许可证集合 → 复制目录到别处 → 在清理 PATH 下启动，要求
`--bogus`、缺失 chart 参数与不可读 chart 分别给出 `player.arguments.unknown`、
`player.arguments.chart_path_missing`、`player.chart.open_failed`，并拒绝任何加载器失败迹象。

`--smoke-test` 仍需要窗口与 GPU，因此不属于无头门禁；GPU 分发验证属于 F1。

## 5. 本地证据

### 5.1 完整矩阵

| 检查 | 结果 |
| --- | --- |
| `cmake --preset debug` + `ctest --preset debug` | `100% tests passed, 0 tests failed out of 734`；1 个既有 symlink 用例 skip；`s6-c4` 标签 2 个用例通过（`cuexis_reference_host_staging` 23.31 s、`cuexis_player_distribution` 19.24 s） |
| `cmake --preset debug-media-tools` + `ctest` | `100% tests passed, 0 tests failed out of 776`；同一 skip；两个新门禁同样注册并通过 |
| `ctest --preset mingw-debug -R "cuexis_reference_host_staging\|cuexis_player_distribution"` | 两个门禁通过（21.87 s / 55.51 s）；证明宿主在 MinGW 工具链下也只需要安装后的公共包 |
| `ctest --preset shared-debug -R "cuexis_reference_host_staging\|cuexis_player_distribution"` | 两个门禁通过（31.54 s / 3.03 s）；shared flavor 会执行 toolchain 拒绝用例 |
| `ctest --preset release -R ...` | 两个门禁通过 |
| `ctest --preset shared-release -R ...` | 两个门禁通过（30.72 s / 3.72 s） |
| MSVC `/W4` 编译宿主源码（对着 staging 安装头） | 四个源文件无警告 |
| `clang-format --dry-run --Werror` | 宿主全部源文件通过 |
| `python -B tools/check_docs.py` | 通过（256 个 Markdown 文件，含新 target 的 BUILDING 目标清单一致性） |

### 5.2 分发目录与随包 smoke

四个 flavor 各生成一个独立目录，并从复制到别处、`PATH` 只含分发目录与系统目录的环境下运行
`--smoke-test`：

| flavor | 目录 | 运行时库 | `--smoke-test` |
| --- | --- | --- | --- |
| static Debug (MSVC) | `cuexis-player-26.09.27-1-dev-windows-static-debug` | 4（`fmtd`、`nlohmann_json_schema_validator`、`SDL3`、`spdlogd`） | exit 0，6 帧 |
| static Release (MSVC) | `cuexis-player-26.09.27-1-windows-static-release` | 4（release 变体） | exit 0，6 帧 |
| shared Debug (MSVC) | `cuexis-player-26.09.27-1-dev-windows-shared-debug` | 9（`cuexis_core/playback/content/audio/audio_sdl-0.7d.dll` + 4 个第三方） | exit 0，6 帧 |
| shared Release (MSVC) | `cuexis-player-26.09.27-1-windows-shared-release` | 9（`-0.7.dll` 变体） | exit 0，6 帧 |
| static Debug (MinGW) | `cuexis-player-26.09.27-1-dev-windows-static-debug`（mingw 构建树） | 3（`libgcc_s_seh-1`、`libstdc++-6`、`libwinpthread-1`） | exit 0，6 帧 |

每次 smoke 记录的 `Prepared objects: 2, behaviors: 1, resources: 4`、`Successful reload activated a
complete OpenGL presentation cache` 与 `Completed frames: 6` 均一致；Debug UCRT 与 Windows 系统 DLL
属系统库，未被打包。MinGW 分发的第三方依赖是静态链接的，因此只需要工具链运行时。

### 5.3 首轮推翻与修复

门禁在 static/MSVC 上通过后在另两个环境失败，暴露的都是**门禁自身**的缺陷（不是 SDK 缺陷）：

1. **shared flavor：`PATH` 未恢复。** 清理过的 `PATH` 在宿主运行之后没有恢复，后续 configure 找不到
   资源编译器，编译器检查（`RC Pass 1 ... failed`）在到达包之前就失败。
2. **shared flavor：负例缺少依赖解析参数。** 外来 toolchain 的负例 configure 没有带上正例使用的
   vcpkg 参数，于是先因为找不到 `tl-expected` 而失败，而不是因为工具链不一致。
3. **MinGW flavor：清理环境缺少编译器运行时。** `0xc0000135` 表示加载器找不到 DLL：宿主需要
   `libgcc_s_seh-1.dll`、`libstdc++-6.dll`、`libwinpthread-1.dll`。门禁现在与 Player 打包一样，把
   编译器目录中的这三个运行时复制到可执行文件旁；它不是 Cuexis 包文件，也不属于包许可证清单。

修复后负例复用同一套依赖解析参数，并只以 `requires compiler AnotherCompiler` 这一条已记录原因失败。
static Debug/Release、shared Debug/Release 与 MinGW static Debug 五个组合的两个门禁全部通过。

### 5.4 hosted 推翻：instrumented preset 下消费者必须同样被插桩

`a38d6cc` 的 Linux Quality run `36295147129` 给出两条结论：

- 通过：`GCC Release`、`Clang Shared Debug`、`GCC Shared Release` 全绿。其中 shared 两个 job 覆盖了
  3.4 的 toolchain 拒绝用例，说明宿主在 shared Linux 包上可用。
- 失败：`GCC Coverage`、`GCC Adapter Coverage`、`GCC Shader Tools Coverage`、
  `Clang ASan + UBSan`、`Clang ASan + UBSan shader-tools`、`Clang ASan + UBSan media-tools`
  六个 job 失败，且失败测试只有 `cuexis_reference_host_staging` 一个
  （`2 - cuexis_reference_host_staging`、`9 - cuexis_reference_host_staging`）。

原因是宿主链接**被插桩的** static SDK 时缺少同一套插桩选项：Cuexis 用目录级
`add_compile_options`/`add_link_options` 施加 `-fsanitize=address,undefined`（ASan）与
`--coverage`（coverage），这些选项不进入安装导出，因此外部工程按 `find_package` 链接时会得到
`undefined reference to __gcov_init/__gcov_exit/__gcov_merge_add`（coverage）与
`undefined reference to __asan_option_detect_stack_use_after_return/__ubsan_handle_type_mismatch_v1`
（ASan/UBSan）。`cuaxis_external_consumer_*` 在 coverage job 中被 workflow 的
`-E "^cuexis_external_consumer_"` 排除，所以此前没有暴露这个问题。

修复：门禁把父构建的插桩配置转发给宿主工程——`CUEXIS_ENABLE_SANITIZERS` 时传
`-fsanitize=address,undefined -fno-omit-frame-pointer`，`CUEXIS_ENABLE_COVERAGE` 时传
`--coverage -O0 -g`，编译与链接选项都传，正例与负例都传。该修复只能由 hosted 验证：本机是
MSVC，sanitize/coverage preset 在 MSVC 上是 configure 致命错误。

### 5.5 hosted 证据

`a38d6cc`（实现 + 三次门禁修复）hosted 结果：

| run | 平台 | 结果 |
| --- | --- | --- |
| `36295150629` | Version Gate | success |
| `36295150667` / `36295147115` | Windows MSVC | success；`release` job 中 `cuexis_reference_host_staging` 10.93 s、`cuexis_player_distribution` 17.16 s 通过，734/734 与 776/776 全绿 |
| `36295150623` / `36295147060` | Windows MinGW | success；`debug` job 中同一对门禁 10.85 s / 20.51 s 通过，734/734 与 776/776 全绿 |
| `36295147129` / `36295150669` | Linux Quality | failure：`GCC Release`、`Clang Shared Debug`、`GCC Shared Release` 全绿（宿主门禁 3.20 s / 4.18 s 通过，shared 两个 job 覆盖 toolchain 拒绝），6 个插桩 job 仅 `cuexis_reference_host_staging` 失败 |

因此 Windows MSVC 与 Windows MinGW 在 hosted 上同时覆盖了两个 C4 门禁，Linux 的 static/shared
非插桩 job 覆盖了宿主门禁，而插桩 job 暴露的缺陷由 5.4 的修复处理。五次门禁缺陷修复分别是：
清理 PATH 未恢复、负例缺依赖解析参数、MinGW 清理环境缺编译器运行时、插桩选项未转发，
以及第一次转发时把参数挂到了**错误的** `add_test`（Player 分发门禁，它不链接任何目标），
由 `out/build/debug/CTestTestfile.cmake` 反查发现——门禁在该 SHA 上仍然失败，直到参数挂到
staging 门禁上。

`d7980bd`（挂对参数后的修复）hosted 结果：

| run | 平台 | 结果 |
| --- | --- | --- |
| `36298998954` | Version Gate | success |
| `36298996643` | Windows MSVC | success |
| `36298996619` | Linux Quality | success：12 个 job 全部通过，包含此前失败的 `GCC Coverage`、`GCC Adapter Coverage`、`GCC Shader Tools Coverage`、`Clang ASan + UBSan`、`Clang ASan + UBSan shader-tools`、`Clang ASan + UBSan media-tools` |

Linux 每个 job 都是 `100% tests passed, 0 tests failed out of N`（631/638/639/654/655/662/669/677/680 等），
宿主门禁在所有 preset 中都注册并通过，包括 ASan+UBSan 与 coverage 插桩。至此 C4 的 hosted 四平台
闭环完成：Windows MSVC、Windows MinGW、Linux GCC、Linux Clang（含插桩 preset）与 Version Gate。

## 6. 升级示例

`docs/guides/VERSIONING.md` 新增「升级示例：从 `0.7.0` 基线重建宿主」：宿主声明基线版本、升级三步
（重新安装 SDK → 指向新前缀 → 干净重建宿主）、以及两个边界声明——`0.7.0` 与当前实现之间没有源不
兼容变更，因此不需要迁移；shared 预览不承诺 binary 可替换，必须重建。

配置存储的现状被如实记录：`cuexis.player-preferences` 与 `cuexis.audio-device-profile` 都只有 v1，
实现不发明 v0 迁移；更高或更低版本的文件被保留、加载退回默认值、保存被拒绝，因此升级与回滚都不会
静默改写用户文件。这是可验证的边界，不是迁移实现。

## 7. 残余

- hosted 同 SHA 证据已完成（§5.5），但 hosted 的 Player **分发** 门禁只在 Windows（MSVC、MinGW）注册：
  Linux preset 是 headless，不构建 Player，因此 Linux 只覆盖宿主门禁。
- 宿主二进制的符号级检查（导入表/依赖清单白名单）还不是门禁：当前证明来自“只链接
  `Cuexis::Playback`、只包含公共头、在清理 PATH 下运行成功”。把导入表纳入门禁属于 F1。
- 插桩 preset 的宿主门禁依赖“把父构建的插桩选项转发给外部工程”这一约定；它是本地与 hosted 都验证过
  的机制，但不是安装导出的一部分，因此未来新增插桩类型时需要同步维护。
- 真正的磁盘满、只读介质与配额失败仍未取证（E3 残余，属于 F1）。
- GPU、窗口与真实音频设备下的宿主/Player 行为不在本批次内；§5.2 的 `--smoke-test` 是本机一次真实
  GPU 运行，不是 CI 证据。
- 本报告不是批次退出、不是 Stage 6 关闭，也不构成 owner acceptance；SDK API 仍为 `0.7.0`。
