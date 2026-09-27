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
`docs/CURRENT_STATUS.md`、`docs/stage_plans/active/stage-06/plan.md`。

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
- **API minor**：宿主在 configure 阶段拒绝 `0.7.x` 之外的接口版本。
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

| 检查 | 结果 |
| --- | --- |
| `cmake --preset debug` + `ctest --preset debug` | `100% tests passed, 0 tests failed out of 734`；1 个既有 symlink 用例 skip；`s6-c4` 标签 2 个用例通过（`cuexis_reference_host_staging` 23.31 s、`cuexis_player_distribution` 19.24 s） |
| `cmake --preset debug-media-tools` + `ctest` | `100% tests passed, 0 tests failed out of 776`；同一 skip；两个新门禁同样注册并通过 |
| `ctest --preset mingw-debug -R cuexis_player_distribution` | 通过（55.51 s）；产物含 `libgcc_s_seh-1.dll`、`libstdc++-6.dll`、`libwinpthread-1.dll`，证明 MinGW 运行时被正确解析与部署，且该 triplet 的第三方库为静态 |
| MSVC `/W4` 编译宿主源码（对着 staging 安装头） | 四个源文件无警告 |
| `clang-format --dry-run --Werror` | 宿主全部源文件通过 |
| `python -B tools/check_docs.py` | 通过（含新 target 的 BUILDING 目标清单一致性） |

MSVC static 分发的运行时库为 `fmtd.dll`、`nlohmann_json_schema_validator.dll`、`SDL3.dll`、
`spdlogd.dll`（4 个，记录在 `VERSION.txt`）；Debug UCRT 属系统库，未被打包。

## 6. 升级示例

`docs/guides/VERSIONING.md` 新增「升级示例：从 `0.7.0` 基线重建宿主」：宿主声明基线版本、升级三步
（重新安装 SDK → 指向新前缀 → 干净重建宿主）、以及两个边界声明——`0.7.0` 与当前实现之间没有源不
兼容变更，因此不需要迁移；shared 预览不承诺 binary 可替换，必须重建。

配置存储的现状被如实记录：`cuexis.player-preferences` 与 `cuexis.audio-device-profile` 都只有 v1，
实现不发明 v0 迁移；更高或更低版本的文件被保留、加载退回默认值、保存被拒绝，因此升级与回滚都不会
静默改写用户文件。这是可验证的边界，不是迁移实现。

## 7. 残余

- 本地没有 shared Player 分发目录证据：本机现有构建树都是 static。Linux hosted 的
  `Clang Shared Debug` 不构建 Player（headless preset），因此 shared 分发只有代码路径与
  `VERSION.txt` flavor 判据，没有本地或 hosted 产物证据。
- 参考宿主的 hosted 复验尚未发生；Linux hosted 会以 shared 包运行该门禁，从而覆盖 3.4 的
  toolchain 拒绝用例。
- `cuexis_player_distribution` 只会在构建 Player 的 preset 上注册；Windows MSVC 与 MinGW 会覆盖它，
  Linux headless preset 不会。
- 真正的磁盘满、只读介质与配额失败仍未取证（E3 残余，属于 F1）。
- GPU、窗口与真实音频设备下的宿主/Player 行为不在本批次内。
- 本报告不是批次退出、不是 Stage 6 关闭，也不构成 owner acceptance；SDK API 仍为 `0.7.0`。
