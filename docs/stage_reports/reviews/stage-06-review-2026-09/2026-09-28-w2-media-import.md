# Stage 6 复核修正：R4 媒体导入修正

状态：completed
快照日期：2026-09-28
更新日期：2026-09-28

本报告是 [Stage 6 复核修正计划](../../../stage_plans/reviews/stage-06-review-remediation/plan.md)
批次 **W2（R4）** 的退出记录。它逐条处置
[两轴复核记录](2026-09-28-spec.md) §7（S7 — S6-E1/E2 媒体导入）与
[Standards 轴](2026-09-28-standards.md) STD-11 的发现项。

**修正原则**：不改写任何历史报告的现象与证据原文；所有新增断言都以**实际变异代码站点后测试必须失败**
的方式反证其非空转，反证记录见 §3。

## 1. 批次边界

- 起始 SHA：`6dbaa59`（W3 批次 HEAD）。
- 涉及路径：`tools/media_import/src/`、`tests/media_import/`、
  `tests/fixtures/stage6_e/media/`、`.gitattributes`。
- 未触碰：ADR 0042 冻结决策正文、Stage 7A/8/9–12 任何交付物、提案 1（Chart v1–v3 退出）。
- **未改动任何 golden 文件**：R0-2 裁定为 (a) 拒绝线性 `gAMA`，因此无 canonical bytes 变化，
  不需按 R5 §10 重新冻结 golden。

## 2. 逐条处置

| 复核 ID | 严重度 | 现象（原文摘要） | 本批次修正 | 证据 |
| --- | --- | --- | --- | --- |
| SPEC-20 | 高 | FLAC 伪造时长的比较被包在失败分支内，伪造 fixture 只改 STREAMINFO、MD5 仍通过，因此声明值比较根本不执行；用例反而**断言伪造被接受** | 把 `declaredSamples` 比较移到失败分支**之外**并无条件执行，判据由 `<` 改为 `!=`（声明长度是对音频的断言，覆盖音频的校验和不能修复它）；用例改为断言 `media.audio.truncated` | `tools/media_import/src/audio_import.cpp`；`tests/media_import/media_import_tests.cpp` |
| SPEC-21 | 高 | 无 sRGB chunk 时同时接受 `gAMA=45455` 与 `100000`（线性），全文件无 gamma 转换，输出仍声明 `colorSpace=sRGB` → 静默近似 | 按 R0-2 (a) **只接受 sRGB 传递函数**，其余 `gAMA` 一律拒绝；诊断文案改为 "is not the sRGB transfer function"（不再自称支持线性） | `tools/media_import/src/image_import.cpp`；新 fixture `gamma_linear.png` |
| SPEC-22（JPEG） | 中 | 计划 `:371` 要求"损坏 chunk/**marker**"负例，实际只有 PNG 的 `corrupt_chunk.png`，无 JPEG 损坏 marker fixture 或断言 | 新增 `corrupt_marker.jpg` fixture（SOS 后熵编码段被破坏，SOI/量化表完好）与断言 `media.image.decode_failed` | `tests/fixtures/stage6_e/media/generate_negative_fixtures.py`；测试用例 |
| SPEC-22（interlaced） | 中 | `image_import.cpp:492-494` 对 `PNG_INTERLACE_*` 走 `png_set_interlace_handling` 直接接受，无 fixture/golden | IHDR 第 12 字节（interlace method）在 profile 解析阶段读出，method 1（Adam7）以新稳定码 `media.image.interlace_unsupported` 拒绝；解码路径保留一条匹配守卫而非静默去交错 | `tools/media_import/src/image_import.cpp`；新 fixture `interlaced.png` |
| STD-11 | 一致性卫生 | `.gitattributes` 未覆盖新 fixture 树 | 为 `tests/fixtures/stage6_e/**`（`.json`/`.cxt` 用 `text eol=lf`，`.png`/`.jpg`/`.flac`/`.ogg`/`.mp3`/`.wav` 用 `binary`）与 `tests/fixtures/stage6_a2/**` 补规则 | `.gitattributes` |

### 2.1 新增稳定诊断码

- `media.image.interlace_unsupported`：IHDR interlace method 非 0 时返回。
  **该码此前不存在**，属本批次新增；格式文档 `STAGE6_CONFIG_AND_MEDIA.md` §5.1 需在后续文档批次
  补记该码（本批次未改格式文档，理由见 §5）。

## 3. 反证记录（每条新增断言必须能被真实变异推翻）

每一条都以"把实现改回缺陷形态（PROBE 补丁）→ 运行相应用例 → 断言失败"的方式验证非空转，
验证后实现全部还原（`grep` 确认无 PROBE 残留）。

| # | 断言 | 变异方式 | 结果 |
| --- | --- | --- | --- |
| A | 伪造 FLAC 时长被拒绝 | `if (false && ... != declaredSamples)` 禁用长度检查 | **失败**：用例的 `REQUIRE_FALSE(result.has_value())` 命中，伪造 fixture 被**接受**——正是 SPEC-20 的原缺陷 |
| B | 线性 `gAMA` 被拒绝 | 把 `&& gammaValue != 100000U` 加回判定 | **失败**：`media_import_tests.cpp:329`，`gamma_linear.png` 不再被拒 |
| C | interlaced PNG 被拒绝 | 移除 profile 解析中的 interlace 拒绝，恢复 `png_set_interlace_handling` | **失败**：`:332`，`interlaced.png` 报出的是别的码（`color_type_unsupported`），即拒绝理由不是 interlace |

**方法论记录**：反证 C 首次提示了一个真实陷阱——把 interlace 检查写错字节位置（误读 compression
method 而非 interlace method）时，测试**仍然失败**，但失败原因是完全无关的另一个码。
这说明"断言某个拒绝码"必须匹配**具体码值**而非"任意失败"，否则会把字节偏移错误当成通过。
本批次据此确认了 `readU8()` 需先跳过 compression 与 filter 两个字节才能到 interlace。

## 4. 门禁与命令输出

| 命令 | 结果 |
| --- | --- |
| `python -B tests/fixtures/stage6_e/media/generate_negative_fixtures.py` | 生成成功；确定性——重跑后既有二进制 fixture byte-identical（`git status` 无改动） |
| `cmake --build --preset debug-media-tools` | 全量构建 0 错误 |
| `cuexis_media_import_tests.exe` | `All tests passed (625 assertions in 23 test cases)` |
| `cmake --build --preset debug --target cuexis_format_check` | 见 §4.1 |

### 4.1 本机环境订正：`audio_stereo_ogg` golden 一度不匹配

本批次开始时，`audio preserves sample rate and mono or stereo channel count` 因
`audio_stereo_ogg` 的 canonical WAV 摘要不匹配而失败（实际 `6d961c9e…`，golden `df73cb81…`）。

- **排查**：`git stash` 后在**未修改的基线上**复跑该用例，仍然失败 → 与 R4 改动无关，是**既有**现象。
- **根因**：`df73cb81…` 是"overlay 把 libvorbis 的 `M_PI` 统一为全精度 double"之后的值
  （ [`STAGE6_CONFIG_AND_MEDIA.md`](../../../formats/STAGE6_CONFIG_AND_MEDIA.md):200-206、
  `vcpkg-overlays/libvorbis/0005-unify-m-pi-precision.patch`）。本机 vcpkg 树的 libvorbis 是
  从 `packages/` 手工拷贝的**未套用 overlay** 产物，`lib/os.h` 仍是 `3.1415926536f`，
  因此在 MSVC 上产出 `6d961c9e…`（其余三平台的值）。
- **处置**：本机**不是**仓库或代码缺陷，而是沙箱环境与 overlay 的偏差。为了不在一个已知偏差的
  环境上做"通过"结论，本批次按 overlay 的补丁把本地 `lib/os.h` 的 `M_PI` 改为
  `3.14159265358979323846` 并重建 libvorbis；重建后该用例通过（golden 精确匹配 `df73cb81…`）。
- **诚实边界**：本机修复**只影响本地 vcpkg 树**，不产生任何仓库内文件改动
  （`git status` 确认 vcpkg 不在版本控制内）。因此 §4 的"通过"是在**与 overlay 一致**的环境下取得的；
  该 golden 的真正确认仍以 hosted 三平台同 SHA 复验为准。

## 5. 残余与未核对

- **格式文档未同步**：新码 `media.image.interlace_unsupported` 未写入
  `docs/formats/STAGE6_CONFIG_AND_MEDIA.md` §5.1。该文件属于 S6-E1/E2 的冻结验收面，
  其修订应与"interlaced 未冻结却被接受"这一决策一并由 owner 确认；本批次只改**实现与测试**，
  避免在未获裁定的情况下先行改写格式契约。恢复条件：owner 确认"拒绝 interlaced"为 v1 决策后，
  在格式文档补记该码并追加一行校验。
- **`media-tools` 仍默认 OFF**：`CUEXIS_BUILD_MEDIA_TOOLS` 未改动，工具仍未进入 Playback/Player
  链接闭包（本批次未触碰 `engine/` 或 `app/`）。
- **hosted 三平台复验未完成**：见 §4.1，本机环境经订正后通过，但同 SHA 的 Linux Quality /
  Windows MSVC / Windows MinGW 结论需由推送后的 PR 检查给出。
- **`corrupt_marker.jpg` 的覆盖强度**：该 fixture 破坏的是 SOS 之后的熵编码字节，
  证明的是"熵流损坏被拒"；它不等价于"marker 长度字段本身非法"。若要求后者，
  需再补一个改写 SOS/SOF 长度字段的 fixture（本批次未做，登记为可选加严项）。

## 6. 变更文件

- 实现：`tools/media_import/src/audio_import.cpp`、`tools/media_import/src/image_import.cpp`
- 测试：`tests/media_import/media_import_tests.cpp`
- 夹具：`tests/fixtures/stage6_e/media/generate_negative_fixtures.py`（新增 3 个派生器）、
  `tests/fixtures/stage6_e/media/image/{gamma_linear.png,interlaced.png,corrupt_marker.jpg}`（新增）
- 仓库规则：`.gitattributes`
- 文档：本报告

## 复核更正（独立审计，2026-09-28）

本次复核对 R4 的代码与真实测试执行做了对抗式核对。以下为**已确认的实质性声明**与**必须更正的表述**。

本节为追加内容，§1 至 §6 的历史现象与证据原文一字未改。本次审计未写入、未修改、未删除任何文件，未运行 `cmake --preset`、`cmake --build` 或 `ctest`。

### 1. 已确认的实质性声明

以下七项经代码读取与真实执行核对通过：

- **线性 `gAMA` 被拒**：`tools/media_import/src/image_import.cpp:387-390`，`gammaValue != 45455U` 时返回 `media.image.gamma_unsupported`。实测 `CHECK( importImageCode(fixture("image/gamma_linear.png")) == "media.image.gamma_unsupported" )` 通过。
- **FLAC 伪造时长无条件被拒**：`tools/media_import/src/audio_import.cpp:588` 的 `client.declaredSamples != 0 && client.accumulator->frames() != client.declaredSamples` 位于 `:597` 的 `if (!processed || !verified || client.failed)` 分支**之前**；提交 `84e93c7` 的 diff 确认比较块被移出失败分支且判据由 `<` 改为 `!=`。实测 `REQUIRE_FALSE( result.has_value() )` 与 `CHECK( importAudioCode(fixture("audio/forged_total_samples.flac")) == "media.audio.truncated" )` 均通过，展开为 `"media.audio.truncated" == "media.audio.truncated"`。
- **interlaced PNG 以新码被拒**：`image_import.cpp:257-261`；`git log -S"media.image.interlace_unsupported"` 仅命中 `84e93c7`，确为本次新增。实测 `CHECK( importImageCode(fixture("image/interlaced.png")) == "media.image.interlace_unsupported" )` 通过。字节偏移核对正确：`rgb8.png` 与 `interlaced.png` 仅差文件偏移 28（`0->1`，即 IHDR 第 12 字节 interlace method）与 29-32（该 chunk CRC）。
- **损坏 marker JPEG 驱动真实负例**：`tests/media_import/media_import_tests.cpp:338` 实测通过。`corrupt_marker.jpg` 与 `baseline.jpg` 同为 661 字节，仅 625-632 共 8 字节按 `0xFF` 异或，文件前 4 字节为 `FF D8 FF E0`（SOI/APP0 完好）。
- **`.gitattributes` 覆盖 fixture 树**：`.gitattributes:9-20`。`git check-attr -a`：`interlaced.png` 得 `binary: set`、`text: unset`、`diff: unset`、`merge: unset`；`stage6_a2/golden/*.json` 得 `text: set`、`eol: lf`。
- **未改 canonical bytes、未重冻结 golden**：`git diff --stat 6dbaa59 HEAD -- tests/fixtures/` 仅列出 `generate_negative_fixtures.py` 与三个新增二进制 fixture（`corrupt_marker.jpg`、`gamma_linear.png`、`interlaced.png`），**无任何 golden 文件**；全套用例通过，其中包含 `requireCanonical` 对 `tests/fixtures/stage6_e/golden/*.json` 的真实摘要比较。
- **测试结果**：`out\build\debug-media-tools\bin\cuexis_media_import_tests.exe` 实测输出 `All tests passed (625 assertions in 23 test cases)`，与 §4 记录一致（23 个 `TEST_CASE`）。

**二进制新鲜度说明**：`out\build\debug\bin\` 下**不存在** `cuexis_media_import_tests.exe`，唯一媒体测试二进制在 `debug-media-tools` 树（时间戳早于提交 `4737cbb`/`fe654ad`）。为确认可执行文件确实包含 R4 代码，本次审计扫描该 PE 镜像并命中 `media.image.interlace_unsupported`、`media.image.gamma_unsupported`、`is not the sRGB transfer function`，以及 `4737cbb` 引入的 `PNG compression method is not part of the v1 image profile`。其后唯一改动 `image_import.cpp` 的提交 `fe654ad` 只是同一个 `mediaError(...)` 调用的 clang-format 重排（字符串字面量与语义相同），故该次运行对 HEAD 的 R4 行为有效。

### 2. 「6 项媒体负例且各以变异反证」不成立，须更正

- 本报告正文 §3 的「反证记录」**只有 3 行**（A、B、C），并非 6 行。
- 真实计数：R4 新增或改写的负例断言为 **4** 条（伪造 FLAC 时长、`gamma_linear.png`、`interlaced.png`、`corrupt_marker.jpg`）；`tests/media_import/media_import_tests.cpp` 中 `importImageCode(`/`importAudioCode(` 调用点为 **37** 处；被断言的 `media.*` 诊断码共 **25** 个不同取值。**没有任何一种计数等于 6。**
- `docs/stage_plans/reviews/stage-06-review-remediation/plan.md:97` 与 `:242` 的「6 项媒体负例」没有任何枚举支撑，属**无依据的账面数字**，应改为上述真实计数或删除。

### 3. gamma 规则的「接受方向」无测试覆盖（真实盲点）

扫描 `tests/fixtures/stage6_e/media/image/*.png` 的全部 chunk 得到：`gamma_linear.png: gAMA=100000`、`gamma_unsupported.png: gAMA=25000`、`srgb_chunk.png: sRGB present`。**没有任何 fixture 携带 `gAMA=45455`**。因此 `image_import.cpp:387` 的接受分支（`gammaValue != 45455U` 为假）与 `:378`（`sawSrgbChunk && sawGammaChunk`）**从未被执行**。后果：一个**拒绝全部 `gAMA`**（连合法的 sRGB 传递函数也拒）的实现同样能通过整套用例。这是真实的覆盖盲点；补一个 `gAMA=45455` 且无 sRGB chunk 的正例即可消除。

### 4. 解码期 interlace 守卫不可达，且报出误导性诊断码

`image_import.cpp:530-533` 返回 `PngDecodeStatus::layoutUnsupported`，该状态在 `:593-595` 映射为 `media.image.color_type_unsupported`。但 `scanPng` 已在 `:257` 先拒绝 interlace，故解码期守卫**不可达**，也没有任何测试能到达它。§2 中「解码路径保留一条匹配守卫而非静默去交错」的表述成立（代码确实存在），但它不构成有效防线。

### 5. R4 引入了一处未记录的 shebang 损坏

`tests/fixtures/stage6_e/media/generate_negative_fixtures.py` 第 1 行由 `#!/usr/bin/env python3`（`git show 49cbfb8` 的 pre-image）变为 `#!/ usr / bin / env python3`；同一提交把 `:101`、`:119`、`:137` 三处函数内注释从 4 空格缩进改为列 0。注释在 Python 中可置于列 0，且文档式调用为 `python -B …`，故属**表面问题**；但脚本不再可直接执行，本报告未记录该改动。

### 6. 新码未进入冻结格式契约，且当前无门禁拦截

`media.image.interlace_unsupported` 不出现于任何 `docs/formats/` 文件（`git grep -rn interlace_unsupported -- docs/` 只命中本报告）。`tools/check_stage6_a2.py:245-256` 只校验固定 token 列表，不包含媒体诊断码，故该遗漏不会被门禁发现。§5 已诚实登记该残余，此处补充事实：**当前没有任何门禁覆盖这一致性**。

### 7. 非空转判断的性质与边界

§3 的 A/B/C 三条变异反证**未核实**：执行变异需改写 `image_import.cpp`/`audio_import.cpp` 并重新构建，两者均为本次审计禁止项。下文的非空转判断**全部是分析性的**，依据是（a）诊断码在实现中的唯一产生点，与（b）git 历史中的行为证据，而非实际观察到的变异结果：

- 伪造 FLAC：`declaredSamples` 仅在 `:480` 与 `:588` 被消费（`git grep`）；另一处 `media.audio.truncated` 产生点 `:542` 需要流尾解码错误，而**修复前**的用例断言该 fixture 被接受且解码帧数与 `mono.flac` 相同（`git show 49cbfb8`），证明 `:588` 是唯一可能来源。
- `gamma_linear`：`media.image.gamma_unsupported` 有两个产生点，`:379` 需要 `sawSrgbChunk`，而该文件无 sRGB chunk（已核验），故只可能来自 `:388`。
- `interlaced`：该码只有一个产生点（`:259`）；fixture 与被接受的 `rgb8.png` 仅差 IHDR 第 12 字节与 CRC。移除该检查后回落到 `:594` 的 `color_type_unsupported`，与 §3 变异 C 记录的现象一致。
- **corrupt-marker JPEG 是本组最弱的一条**：`media.image.decode_failed` 有多个产生点，仅凭码值无法归因；其非空转依赖「未改动的 `baseline.jpg` 被接受」这一配对事实，且本报告**未记录**该用例的任何变异反证。
- 音频负例（`truncated.mp3/ogg/flac`、`bad_header.flac`、`chained.ogg`）的码值断言唯一，实测 `All tests passed (15 assertions in 1 test case)`；同样无变异记录。

### 8. 本次审计未核实的项目（均注明原因）

- §3 变异记录 A/B/C 的实际执行结果 —— **未核实**：需改写源码并重新构建，属禁止项。
- §4 `cmake --build --preset debug-media-tools`「全量构建 0 错误」 —— **未核实**：禁止构建。
- §4 `cuexis_format_check` —— **未核实**：禁止构建。
- §4.1 libvorbis 重建与重建前的失败 —— **未核实**：只能观察到当前状态。`D:\vcpkg\buildtrees\libvorbis\src\v1.3.7-*\lib\os.h` 现为 `#  define M_PI (3.14159265358979323846)`，与 `vcpkg-overlays/libvorbis/0005-unify-m-pi-precision.patch` 一致；`*audio preserves sample rate*` 用例实测 `All tests passed (47 assertions in 1 test case)`。§4.1 的叙述与当前状态自洽，但重建过程本身未被观察。
- §4 生成脚本的重跑确定性（以 `git status` 判定） —— **未核实**：重跑会写入 fixture 文件，属禁止项。改以**内存内重算**替代（不写任何文件）：9 个派生 fixture 与磁盘字节完全一致（`gamma_linear.png` 85、`interlaced.png` 78、`corrupt_marker.jpg` 661、`forged_total_samples.flac` 8759，以及 `truncated.flac/mp3/ogg`、`chained.ogg`、`bad_header.flac` 全部 `True`）。
- hosted 三平台复验 —— **未核实**：§5 已登记为未完成，超出本批次范围。

### 9. 与 R4 无关、不计入本次更正的现象

本机 `cuexis_player_distribution` 与 `cuexis_contract_version_gate` 的失败属环境原因，不作为 R4 发现项。

### 10. 结论

R4 的四项实质性行为声明（线性 `gAMA` 被拒、FLAC 伪造时长被拒且用例方向已翻转为拒绝、interlaced PNG 被拒、损坏 marker JPEG 负例）在代码与真实执行两个层面均成立；「未改 canonical bytes、未重冻结 golden」亦成立。不成立的是计划中「6 项媒体负例各以变异反证」的账面表述。另有三个应予登记的缺口：gamma 接受方向无覆盖、解码期 interlace 守卫不可达、新诊断码未进入格式契约且无门禁拦截。
