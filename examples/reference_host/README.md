# Cuexis Reference Host

一个**独立**的示例宿主工程，用来证明「安装包 + 公共边界」足以驱动 Playback：
它只通过 `find_package(Cuexis ... COMPONENTS Playback)` 消费**安装后的**公共头与
`Cuexis::Playback` 导出 target，不包含仓库内私有头，不链接 Player 的配置实现
（`cuexis_player_support`），也不读取 Cuexis 源码树。

本工程由 [ADR 0042](../../docs/adr/0042-stage-6-productization-boundaries.md) 的 `S6-D08` 冻结在
`examples/reference_host/`；它对齐的 SDK API 基线是 `0.7.1`。工程是**宿主模板**，
不是 SDK 交付面——它的内部结构不是公共承诺。

## 1. 组成

| 文件 | 职责 |
| --- | --- |
| `CMakeLists.txt` | 独立工程声明；`find_package`、SDK API minor 校验、可执行目标 |
| `src/main.cpp` | 参数解析、`--help`、报告落盘 |
| `src/host_runner.{hpp,cpp}` | 宿主编排：加载、提交、帧脚本、Seek、重载与拒绝重载 |
| `src/host_content.{hpp,cpp}` | 宿主自有 content 模型与 `IContentProvider` 实现 |
| `src/host_report.{hpp,cpp}` | 宿主自有运行记录（`host.<name> key=value` 行） |

## 2. 宿主主循环

`runHost()`（`src/host_runner.cpp`）按固定顺序执行，**任何一步失败都以非零退出码结束**，
并把失败步骤与诊断码写进运行记录：

1. **start** —— 创建全新 `PlaybackSession`，断言初始状态为 `Empty`。
2. **load** —— 由宿主构造 `PlaybackSource`（typed project source，见 §3），
   以 `PlaybackMode::ChartClock` 调用 `prepareLoad`，校验返回的 `PreparedSemanticIdentity`
   与 presentation manifest 完整后 `commit`。
3. **frames** —— 运行宿主时钟帧脚本（`scriptedSteps`：chart time `0 → 625 → 250 → 1250` ms，
   每次跳变递增 `discontinuityId`），随后进入宿主自有的 advance 循环（默认 4 帧、
   每帧 `+250 ms` 且携带真实 `simulationDeltaTimeMs`）。每一步都经 `consumeStep` 取帧、
   计算帧摘要并记录。
4. **digest 校验** —— 逐个核对 `--expect-digest <index>=<value>`；不匹配即失败。
5. **reload** —— 用同一内容 `prepareReload(..., ReloadPolicy::KeepChartTime)` 并 `commit`；
   要求活动身份不变、且重载后观测到的帧摘要序列与重载前**逐项相同**。
6. **reload-failed** —— 注入宿主 provider 故障（`buildProjectSource(..., faulty=true)`），
   要求 `prepareReload` **被拒绝**，并且活动身份、会话状态与活动帧摘要**均不被扰动**。
7. **package（可选）** —— 指定 `--package` 时额外加载一个已发布的 Cuexis 包，并走完
   上述身份/摘要/重载检查。
8. **summary** —— 输出总结行；`ok()` 汇总本次运行是否成功。

## 3. Content provider 构造

宿主**自己**解析逻辑资源并把字节交给 SDK，SDK 不直接读宿主文件系统：

- `HostContent`（`src/host_content.hpp`）持有内容根目录、chart entry 相对路径
  （默认 `charts/main.cuexis.chart.json`）与 provider root id。
- `HostFileProvider::readBlob()` 实现 `cuexis::content::IContentProvider`：把请求解析到
  内容根**之下**，**拒绝任何越界请求**，并把每次读取的序号记入运行记录
  （`provider.read index=N`），使宿主能证明 SDK 确实向宿主索要了内容。
- `hostAssetTable()` 给出**显式声明**的资产表。生产宿主从自己的创作数据派生它；
  参考宿主保持显式，以便公共契约保持可见。
- `buildProjectSource()` 把 `HostContent` 组装成 typed `PlaybackSource`；
  `faulty=true` 时 provider 会拒绝后续读取，用于第 6 步的拒绝重载验证。

## 4. 命令循环与命令行

参考宿主是**脚本式宿主**：`main.cpp` 解析 argv 后运行 §2 的固定命令序列，
不接受交互式 stdin 命令。这是本阶段记录的口径（见
[Stage 6 复核修正计划](../../docs/stage_plans/reviews/stage-06-review-remediation/plan.md) 的 R7）。

```text
usage: cuexis_reference_host --content <project-directory> [options]
  --content <dir>         host content root (a project directory)
  --package <file.cxc>    also load a published Cuexis package
  --advance <n>           extra host-clock advance frames (default 4)
  --expect-identity <hex> require the reference content identity
  --expect-digest <i>=<v> require a frame digest (repeatable)
  --report <file>         write the run record to a file as well
  --help                  print this usage text
```

判据（CFU-F golden，供门禁核对）：参考内容身份 `6d01494c...`，参考帧摘要
`11596562486377158370`。

`--report <file>` 会把运行记录同时写入文件，随后仍回显到 stdout，便于门禁在
无 GPU、无窗口、无调试器的条件下检查一次完整运行。

## 5. SDK API 基线校验

`CMakeLists.txt` 声明 `CUEXIS_HOST_API_VERSION`（默认 `0.7.1`）作为宿主编写时对齐的基线，
并在 configure 阶段拒绝**不兼容的 SDK minor**：`find_package` 的
`COMPATIBILITY SameMinorVersion` 与显式的

```cmake
if(Cuexis_API_VERSION VERSION_LESS "0.7.1" OR NOT Cuexis_API_VERSION VERSION_LESS "0.8.0")
    message(FATAL_ERROR "The reference host supports SDK API 0.7.x; found ${Cuexis_API_VERSION}")
endif()
```

共同保证越界版本在 configure 阶段失败而不是运行期才崩。

## 6. 构建与运行

需要一个已安装的 Cuexis SDK（见
[BUILDING.md](../../docs/guides/BUILDING.md) 的「具名参考宿主」一节）。

```powershell
# 1. 用安装后的 SDK 配置并构建宿主（源树之外）
cmake -S examples/reference_host -B out/build/reference-host `
  -DCuexis_DIR=out/install/headless-release/lib/cmake/Cuexis
cmake --build out/build/reference-host

# 2. 直接运行，给定一个宿主内容目录
out/build/reference-host/cuexis_reference_host.exe --content <project-directory>
```

也可以直接跑仓库已注册的门禁（会执行 staging 安装、源树外复制、清理 PATH 后运行）：

```powershell
ctest --preset debug -R cuexis_reference_host_staging --output-on-failure
```

门禁会额外校验：示例源码只包含 `cuexis/playback/` 公共头、不引用仓库内 target、
包身份与参考帧摘要匹配 CFU-F golden，以及（shared 包）记录的 toolchain 与 consumer
不一致时被拒绝。

## 7. 不作为承诺

- 本工程是宿主**模板**，内部结构、文件划分与记录格式都不是 SDK 公共 API。
- 它不构成 Cuexis 的发布产物，也不进入 SDK 安装树。
- `CUEXIS_HOST_API_VERSION` 是**宿主侧**的编写基线，不是 SDK 版本来源。

## 8. Experimental candidate entry

The updated host requests SDK API 0.7.1 for the additive Entry factories. To consume an
experimental install, explicitly pass `-DCuexis_ALLOW_EXPERIMENTAL=ON` when configuring.
At runtime select `--candidate-entry compiled/chart.packed` together with an explicit
`--content` project root; the legacy source path does not select a candidate. The host logs
`host.build flavor=experimental` or `production`. It still uses public Playback APIs for all
open/play/pause/tick/seek/reload/quit commands. OFF libraries reject Entry factories even when
the consumer permitted experimental packages at configure time.
