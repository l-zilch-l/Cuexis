# S6-A2 冻结合同落盘与表征

状态：completed（S6-A2）；下游批次未完成

日期：2026-09-21

本报告只关闭 Stage 6 计划中的 S6-A2。A2 将 ADR 0042 已接受的决策落为字段级 Spec、
Schema、API/安装草案、依赖边界和可复算表征；它不实现 candidate Playback、Player support、
renderer、media importer、版本门禁或 Reference Host，也不产生新的 owner acceptance。

## 1. 执行基线

| 项 | 本次事实 |
| --- | --- |
| 工作区 | `C:/Users/Zilch/.codex/worktrees/7596/Cuexis`；未操作 `D:/Cuexis` 或其它 worktree |
| HEAD | `e7d6c1ea3ff950226b7a3582eb128cb2d01800fb`；本报告对应执行 HEAD 未变 |
| 分支 | detached HEAD；未创建分支、提交、PR、推送、合并或发布 |
| 日期版本 | `26.08.01-1`；仍为当前实现版本 |
| SDK API | `0.7.0`；没有提前升级到目标 `0.7.1` |
| 前置 | [S6-A1 报告](2026-09-20-s6-a1-baseline.md) 已完成；ADR 0042 已接受且仍标注尚未实现 |

A2 只修改合同、Schema、fixture、表征 checker、文档索引和状态证据。没有修改现有公共头、
CMake target、版本源、`vcpkg.json`、既有 v4 identity 或 FrameDigest v1-v3 golden。

## 2. A2 产物

| 产物 | 位置 | 状态与边界 |
| --- | --- | --- |
| Chart entry extension v1 Spec | [CHART_ENTRY_V1_FORMAT.md](../../../formats/CHART_ENTRY_V1_FORMAT.md) | 已落盘；ProjectConfig/CXC 共用扩展、显式 path、R5 profile 和拒绝顺序 |
| Chart entry Schema | [cuexis.chart-entry.v1.schema.json](../../../../schemas/cuexis.chart-entry.v1.schema.json) | 已落盘；未知字段拒绝，candidate playback 字段和范围有约束 |
| Player preferences Schema | [cuexis.player-preferences.v1.schema.json](../../../../schemas/cuexis.player-preferences.v1.schema.json) | 已落盘；只包含 ADR 0042 首批实际消费字段 |
| Audio device profile Schema | [cuexis.audio-device-profile.v1.schema.json](../../../../schemas/cuexis.audio-device-profile.v1.schema.json) | 已落盘；system-default/exact selector、校准范围和默认路由规则有约束 |
| 配置、身份、媒体和事务 Spec | [STAGE6_CONFIG_AND_MEDIA.md](../../../formats/STAGE6_CONFIG_AND_MEDIA.md) | 已落盘；字段 owner、身份预映像、设备公式、预算和原子发布边界 |
| API/安装声明草案 | [STAGE6_API_AND_INSTALL_DRAFT.md](../../../proposals/STAGE6_API_AND_INSTALL_DRAFT.md) | 已落盘；三个新工厂名称、OFF/ON parity、实验安装和 Reference Host 边界 |
| 模块/安装依赖图 | [STAGE6_PRODUCTIZATION_BOUNDARIES.md](../../../architecture/STAGE6_PRODUCTIZATION_BOUNDARIES.md) | 已落盘；renderer、Player support、media tools 的 planned one-way graph |
| 独立表征 | [stage6_a2 fixtures](../../../../tests/fixtures/stage6_a2/) 与 [checker](../../../../tools/check_stage6_a2.py) | 已落盘；entry 正反例、identity/session、媒体 bytes 和音频校准算术 |

## 3. 冻结合同的落盘结论

### 3.1 Candidate entry 与兼容边界

- candidate 只由默认 OFF 的 `CUEXIS_ENABLE_CHART_V5_CANDIDATE` 和显式 entry factory 进入。
- 新名字固定为 `fromFilesystemProjectEntry`、`fromCxcFileEntry`、`fromCxcMemoryEntry`；
  旧 `fromFilesystemProject`、`fromCxcFile`、`fromCxcMemory` 和 `fromChartText` 的 v1-v4
  默认语义不变，不自动探测、重试或升级。
- ProjectConfig 与 CXC 使用同一个 `cuexis.chart-entry.v1` 扩展；裸 Packed 不是用户入口，
  `entry.chart` 继续是 v4 回退入口。entry path 必须显式提供，不能按扩展名、magic、排序
  或失败后的其它格式猜测。
- candidate 消费范围保持 R5 §10：`flags=1`、`candidateRevision=1`、
  `cuexis.gameplay.candidate.lanes4`、`press`、单 lane `[0,3]`、tap/point、空 effects。
  Hold/Release、range、Slide/Flick、多指和未登记 sections 仍拒绝。

### 3.2 Typed lowering 与 identity

- source state 使用显式 tagged payload；已校验 candidate 必须拥有 entry bytes、provider、
  decoded typed chart、identity map 和 resource closure。A2 草案禁止执行层重新解析 JSON、
  CXT v2 或二次 Packed decode。
- generated identity、parent relation、完整 Requirement tuple、constraints/effects、
  feature derivation 和 transitive resource closure 均必须保留；A16 由离线 typed assembler
  派生，Playback 不补 feature。
- execution identity、prepared semantic identity、resolved execution config identity 和
  composite session identity 使用分离域和固定 little-endian 预映像；设备、窗口、路径和
  gain 不污染 prepared content identity。旧 v4 identity 和 FrameDigest v1-v3 不改。

### 3.3 Renderer、配置、媒体和安装边界

- `cuexis_presentation_renderer` 位于 Playback/render 之上，底层 render/runtime 不反向依赖；
  `cuexis_player_support`、`cuexis_media_import` 和 Reference Host 均是未实现的内部或示例
  边界，不是已安装 SDK component。
- 配置由 application/support layer 拥有；UserPreferences 只保存实际首批字段，
  AudioDeviceProfile 拥有 selector 和 output correction。应用提交顺序、token/generation、
  音频激活失败和 post-commit renderer failure 的处理已写入 Spec。
- 媒体 profile 固定为 PNG/JPEG 到 RGBA8 sRGB、MP3/Ogg Vorbis/FLAC 到 canonical S16LE WAV，
  并记录 source/output/worker memory 的 hard budget、逐字节跨平台目标和 generation/atomic
  publication 要求；不把未执行的 decoder 或 license 检查写成通过。
- `0.7.1` 仍是满足兼容条件后的条件性目标；A2 没有切换正式 Writer、没有建立稳定 C ABI、
  没有扩大 R5 允许消费范围，也没有为 Stage 8 预留 `0.8.0`。

## 4. 独立 golden 与负例

checker 重建并比较以下稳定数据，而不是只检查文件存在：

| 表征 | 结果 |
| --- | --- |
| generated execution identity | `v5g1:75dafa6d8fa0e5f47590d81ce9809c5ea0c183009cc3835defcb22fe4b501e57` |
| prepared semantic identity | `63451a64d86865dca648c88c12addf1762160e23e8e678ce65447d039f556999`；资源按 ASCII AssetId 排序 |
| execution config identity | `4983d9c831c892c210853c9889b780b2ffc1b36d34c71b0c388995187dcee2e3`；CuexisAudio、`-12500 us` |
| composite session identity | `8a3060c5554d2de4d614e5346ca6697b9a1c2bc09eaad30d4e08b1e7ae43a833` |
| Texture2D canonical bytes | 48 bytes；SHA-256 `bd79151023e167aa03f9bed988f34041ada556fee6f0cffae3567d46284634f6` |
| WAV canonical bytes | mono 8000 Hz、samples `-32768, 0, 32767`；SHA-256 `cad856a7a36258893d7a7f4c9bf7e55dbd6d58e0ffb8f247157c7cfe393113fb` |
| audio correction arithmetic | zero saturation、negative correction、reverse seek 三个微秒整数 case 均重建通过 |

负例表征包含 duplicate/file path conflict 和未注册 encoding。它们是 A2 contract characterization，
不是 C1 的真实 Packed/CXC decoder 负例；生产 Reader、typed lowering 和资源闭包负例仍由 C1
实现并验证。

## 5. 实际验证

以下命令均在本工作区执行；没有使用其它工作树的构建产物：

| 命令 | 结果 | 证据边界 |
| --- | --- | --- |
| `python -B tools/check_stage6_a2.py` | 通过：`S6-A2 characterization passed: schemas, entry fixtures, identity/media goldens and boundaries` | Schema 基本合同、fixture、预映像/bytes hash、边界 token 和依赖方向表征 |
| `python -B tools/check_docs.py` | 通过：`Documentation checks passed: 240 Markdown files and 20 candidate JSON/CXT files validated.` | Markdown links/H1/reachability、索引和 candidate consistency |
| `python -B tools/update_version.py --check` | 通过：`Cuexis version is consistent: 26.08.01-1` | 日期版本与 manifest 一致性；不执行版本升级 |
| `python -B -c "import json; ..."` | 通过：`3 Stage 6 A2 schemas parsed` | 三份新增 Schema 可解析；不是完整 JSON Schema evaluator |
| `git diff --check` | 通过，只有 Git 的 LF/CRLF 转换提示 | whitespace 检查；转换提示不是 whitespace error |

A2 不运行 Debug/Release C++ build、CTest、GPU、真实音频、hosted Linux/MSVC/MinGW、
sanitizer/coverage/clang-tidy 或 clean staged consumer：本批次没有改 C++/CMake/build input，
且这些是 C1/C2/D1/E1/E2/C4/F1 的实现或平台门禁。未运行不等于通过。

## 6. 依赖与残余证据

只读核对的 vcpkg port baseline 与 ADR 0042 一致：libpng `1.6.58`、libjpeg-turbo `3.2.0`、
minimp3 `2021-11-30`、libvorbis `1.3.7#4`、libogg `1.3.6#1`、libFLAC `1.5.0`。当前
安装树可见 libpng/libvorbis/libogg 的 share/license 材料；没有为 libjpeg-turbo、minimp3、
libFLAC 取得对应安装树 license/share 证据。因此媒体 profile 只能记录为冻结选择和待验证项；
E1/E2 必须在实际接入前复核固定 port 的构建选项、许可证闭包、错误行为和 Windows MSVC、
Windows MinGW、Linux GCC、Linux Clang 的 canonical bytes，不能以本报告关闭媒体发行门禁。

其余未完成项：

- B1 版本比较器、受保护 master/UTC 合并门禁和发行 checklist 未实现。
- C1 candidate source/CXC/typed lowering/Requirement retention/prepare identity 未实现。
- D1/D2 renderer contract 的代码、OpenGL 迁移和 GPU evidence 未实现。
- C2 配置快照、fake/real device 和音频 replacement 未实现。
- E1/E2 decoder、预算执行、cross-platform golden 和 importer target 未实现。
- C3/C4 Player 状态机、Reference Host、安装/升级和 external consumer 未实现。
- 没有 Linux hosted、真实宿主、真实设备或 owner acceptance 证据。

没有发现需要重开 ADR 0042 的表征失败；如果后续实现证明上述冻结选择不可行，必须提交复现、
影响、替代方案和迁移/测试影响，请 owner 重新裁定，不能自行修改 ADR 或 golden。

## 7. 退出结论与下一批次

S6-A2 的合同、Schema、API 草案、依赖/安装图和独立表征已落盘，且 focused checker 能重建
关键 identity、媒体 bytes、配置算术和入口边界。A2 因此标记为 `completed`，允许按依赖图
进入 B1、C1、D1、C2、E1/E2 的实现工作；各批次仍必须先满足自己的上游合同和失败路径门禁。

本报告不表示 candidate Playback、正式 Chart v5、SDK `0.7.1`、Stage 6 或 owner acceptance
已完成。下一批次建议按计划选择一个已释放的实现子批次；版本门禁 B1、candidate 消费 C1、
renderer D1 和配置/媒体实现均不能把本报告的表征结果当作实现证据。
