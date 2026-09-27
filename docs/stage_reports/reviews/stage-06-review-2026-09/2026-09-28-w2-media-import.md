# Stage 6 复核修正：R4 媒体导入修正

状态：completed
快照日期：2026-09-28
更新日期：2026-09-28

本报告是 [Stage 6 复核修正计划](../../../stage_plans/active/stage-06-review-remediation/plan.md)
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
