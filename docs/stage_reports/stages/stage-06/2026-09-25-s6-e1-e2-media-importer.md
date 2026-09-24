# S6-E1/E2 媒体导入实现

状态：implemented（本地实现与本地 MSVC 证据；不是批次退出、Stage 6 关闭或 owner acceptance）

日期：2026-09-25

本报告记录 S6-E1（JPEG/PNG → portable texture）与 S6-E2（MP3/Ogg Vorbis/FLAC → audio artifact）
的实现和本地验证证据。冻结决定来自 ADR 0042 §S6-D06 与
[STAGE6_CONFIG_AND_MEDIA.md](../../../formats/STAGE6_CONFIG_AND_MEDIA.md) §5；本页不重新裁定
profile，也不把本地结果当作 hosted 或 release 证据。

## 1. 批次基线

| 项 | 事实 |
| --- | --- |
| 工作区 | `C:/Users/Zilch/.codex/worktrees/7596/Cuexis` |
| 分支 | `stage-06-workspace` |
| PR | #29，`OPEN`，未合并 |
| 起始顶端 SHA | `5c638ea2e558705f0827d9aa25b1aed2b1cb0e48` |
| 目标基线 | `master` `46b65d1f2345f543b98e7e87fe5ec9ed735f10bf` |
| 显示版本 | `26.09.23-1`（本批次没有改版本） |
| SDK API | `0.7.0`（本批次没有改 SDK minor） |

本批次没有扩大 R5 §10.1 的允许消费清单，没有引入运行时脚本入口，没有让 `engine/animation/`
解析 JSON/CXC/CXT，没有改写 A2 golden，也没有把媒体工具链入 Playback 或 Player。

## 2. 计划项对照

计划 S6-E 公共要求：

- 新增独立开关 `CUEXIS_BUILD_MEDIA_TOOLS`（默认 `OFF`）与 vcpkg feature `media-tools`。
  它与 `shader-tools` 相互独立，打开一个不引入另一个。
- 内部静态库 `cuexis_media_import` 与 developer-only CLI `cuexis_media_importer`。
  Playback 与 Player 既不链接它们，也不在运行时启动该 CLI；运行时直接解码不是回退路径。
- 只使用固定的库适配器：没有 ffmpeg，没有系统 codec 探测，没有自研编解码器，没有
  failed-decoder fallback。
- 输出写入既有格式：图片是 CXPRES01 Texture2D v1，音频是 canonical RIFF/WAVE PCM S16LE。

计划 S6-E1：

- PNG 走 libpng 1.6.58，JPEG 走 libjpeg-turbo 3.2.0（integer IDCT、fancy upsampling）。
- 输出 24 字节 envelope + 紧排 top-left RGBA8；直通 alpha、无行填充、仅 mip 0。
- 缺省颜色元数据按 sRGB；palette、gray、tRNS 展开为 RGBA；16-bit PNG、APNG、
  CMYK/YCCK JPEG、不支持的 ICC/gamma、畸形或矛盾的 EXIF orientation 全部拒绝。
- EXIF orientation 1-8 只在导入时归一化一次；orientation 1/3/6/8 与 baseline 的 canonical
  字节已在测试中交叉校验。

计划 S6-E2：

- MP3 走 minimp3 2021-11-30（`MINIMP3_NO_SIMD`），Ogg Vorbis 走 libvorbis 1.3.7#4 +
  libogg 1.3.6#1，FLAC 走 libFLAC 1.5.0 原生整数路径。
- 采样率与单/双声道原样保留；不重采样、不混音、不做响度归一化、不加抖动、不写时间戳或
  源路径。MP3 只按库给出的有效 delay/padding 裁剪一次；Vorbis 用 granule end 截断并拒绝
  chained/multiplexed 流；FLAC 保留样本数并在 `FLAC__stream_decoder_finish()` 复验 CRC/MD5。
- 转换代码固定整数舍入/饱和规则：MSVC 用 `/fp:precise`，GCC/Clang 用
  `-fno-fast-math -ffp-contract=off`。

计划 S6-E 预算与发布：

- 全部计数与乘积在 reserve/resize 前做 checked arithmetic，逐行/逐帧检查；元数据长度按
  不可信处理。
- 库内部无法度量的分配交给带硬内存上限的 worker 进程：Windows 用 Job Object
  `JOB_OBJECT_LIMIT_PROCESS_MEMORY`，POSIX 用 `setrlimit(RLIMIT_AS)`。
- 输出按内容 identity 寻址且不可变：相同字节是 no-op，不同字节返回
  `media.publish.immutable_conflict`，不覆盖已发布文件。

## 3. 实现清单

新增：

- `tools/media_import/` — 公共头 `include/cuexis/media_import/media_import.hpp`，实现
  `media_identity.cpp`、`image_import.cpp`、`audio_import.cpp`、`minimp3_impl.cpp`。
- `tools/media_importer/` — CLI `main.cpp`、`publish.{hpp,cpp}`、`worker_process.{hpp,cpp}`。
- `tests/media_import/media_import_tests.cpp` — 18 个 TEST_CASE。
- `tests/fixtures/stage6_e/media/` — 生成脚本、JPEG fixture writer 与 fixture README。
- `tests/fixtures/stage6_e/golden/` — 18 个 canonical golden。
- `cmake/VerifyMediaImporter.cmake` — CLI 端到端门禁。

改动：

- 根 `CMakeLists.txt`：`CUEXIS_BUILD_MEDIA_TOOLS` 选项、`CUEXIS_ACTIVE_TARGETS` 三项、
  `cuexis_media_import` / `cuexis_media_importer` 的 allowlist。
- `vcpkg.json`：`media-tools` feature。
- `CMakePresets.json`：`debug-media-tools`、`headless-sanitize-media-tools`、
  `media-tools-coverage`。
- `tools/CMakeLists.txt`、`tests/CMakeLists.txt` 的接线。

profile identity 为
`46a74958149d185b6963f3ed28cffb2665b3118fea1d83ffa8b742e3c65beee8`，由 domain
`cuexis.media.import.profile.v1`、profile version `1` 与五个解码器标识串构成：
`libpng-1.6.58`、`libjpeg-turbo-3.2.0`、`minimp3-2021-11-30-no-simd`、
`libvorbis-1.3.7-libogg-1.3.6`、`libflac-1.5.0-native`。

## 4. 本地验证

`debug-media-tools` 配置、构建、测试全部通过，且没有编译警告：

```text
cmake --preset debug-media-tools
cmake --build --preset debug-media-tools
ctest --preset debug-media-tools --no-tests=error
```

- `cuexis_media_import_tests`：18 个 TEST_CASE、534 条断言全部通过。
- `cuexis_media_importer_tool_tests`：`--help`/用法退出码、worker 与 `--in-process` 字节一致、
  产物名等于自身 SHA-256 且等于 golden、重复发布是 no-op、被篡改的已发布产物返回
  `media.publish.immutable_conflict` 且不被覆盖、1 MiB 进程上限 fail closed 且不发布任何文件、
  CMYK JPEG 以退出码 2 和 `media.image.color_type_unsupported` 拒绝。

canonical golden 抽样（完整清单在 `tests/fixtures/stage6_e/golden/`）：

| golden | 字节数 | SHA-256 |
| --- | ---: | --- |
| `image_rgb8` | 64 | `12799c16d89a1f6cd0ca4285391ebc38607d11095bc47840c800bc7b9467caa1` |
| `image_baseline` | 72 | `548d4c3b38ad632458f395e591db4c63c8228f7f3d0e57651889f93db2f361e2` |
| `audio_mono_mp3` | 1324 | `c41966babb2bc1a40f4a746de6b68abaaa35169a80649be64553b34365933306` |
| `audio_mono_flac` | 1324 | `691a7aefdd3e2910e3567da6d02208c2e8e59ef33aa2582852de8e69b8f72ef0` |

FLAC 无损性有一个独立锚点：`audio_mono_flac` 与 `audio_stereo_flac` 的 canonical WAV 与
ffmpeg 写出的 `source_mono.wav` / `source_stereo.wav` 逐字节相同，同时校验了 canonical
WAV header 布局。

其他本地检查：

- MinGW g++ 20 `-Wall -Wextra -Wpedantic -Werror -fsyntax-only`：7 个媒体实现源文件和测试
  源文件全部通过。
- `clang-format --dry-run --Werror`：新增和改动的源文件全部通过。第一次 hosted 运行发现本地
  漏掉了 `tests/fixtures/stage6_e/media/jpeg_fixtures.cpp`（它落在 `tests/**` 的 GLOB_RECURSE
  里），已按同一 `.clang-format` 修正，并用两个 clang-format 版本复核。
- `python -B tools/check_docs.py` 与 `git diff --check` 通过。

### 4.1 Player 实际显示（E1 验收）

E1 验收要求「Player 实际显示」。本机用真实窗口与 OpenGL 路径验证，步骤与结果如下。

1. 从 `assets/projects/stage3_project/assets/textures/checker.texture.bin` 读出 2x2 RGBA8
   像素（`ffffff ff`、`2878ff c0`、`ff5050 80`、`141414 ff`），用 Python zlib 写出等价的
   `checker_source.png`。
2. 用 CLI 导入：`cuexis_media_importer --kind image --input checker_source.png
   --output-dir ... --in-process --print-info`。产物为
   `ca8bf529e969928d314f3cd838d19ee743c5cb7de47c3ab5a2a47e7b9b13e72f.texture.bin`，
   与工程里已提交的 `checker.texture.bin` **逐字节相同**。也就是说固定 profile 的 canonical
   输出就是该工程资源本身，而不是近似结果。
3. 复制工程到临时目录，把该产物放回 `assets/textures/checker.texture.bin`，运行
   `cuexis_player.exe --smoke-test --project <副本>`。退出码 0，6 帧全部完成，第 5 帧报告
   `pixel=51,32,71,192`、`summary=18316288860163381829`，与 smoke 冻结的 textured blend
   期望像素和 `presentationSmokeDigest` 完全一致。
4. 对照实验：把同一位置换成另一个导入产物（2x2 不透明绿色，
   `9b3cadda6f477e9ffc669121e4c5728ea894371b22a173a05307b4b0a009d693`），其余文件不变。
   同一个 smoke 运行在 `player.smoke_test.textured_blend_pixel_invalid` 失败，而未贴图的第 0 帧
   仍然是 `pixel=255,204,51,255`、`summary=4536622714229612252`，与基准一致。只替换导入产物
   就改变了被采样的像素，说明显示内容确实来自 importer 的 canonical 产物。

证据目录为 `out/s6e_display/`（`/out/` 已被 `.gitignore` 忽略，因此这些运行产物不进入提交）。

### 4.2 三种格式的完整导入/播放（E2 验收）

E2 验收要求「三种格式各有完整导入/播放正例」。仓库里唯一的 RIFF 解析器是
`cuexis::audio_sdl::WavDecoder`，Player 只在 `PlaybackMode::CuexisAudio` 下经
`preparePlayerAudioClip` 调用它，然后交给 `SdlAudioTransport`；`--audio-smoke-test` 是唯一
端到端覆盖这条路径的模式。本机验证如下。

1. 用 Python 写一个确定性的 3 秒 48000 Hz 双声道 S16 源 `source.wav`（576044 字节）。
2. ffmpeg 以固定选项编码出 `source.mp3`（libmp3lame 128k）、`source.ogg`（libvorbis q5）、
   `source.flac`（flac level 5）。
3. 用 CLI 分别导入：`cuexis_media_importer --kind audio --input source.<fmt>
   --output-dir ... --in-process --print-info`。三者都报告
   `sampleRate=48000`、`channels=2`、`frames=144000`、`decodedBytes=576044`，decoder 分别为
   `minimp3-2021-11-30-no-simd`、`libvorbis-1.3.7-libogg-1.3.6`、`libflac-1.5.0-native`。
4. FLAC 的 canonical WAV 与 `source.wav` **逐字节相同**（lossless 路径没有引入任何改写）；
   MP3 与 Ogg 按各自的有效 delay/padding 与 granule end 规则截断，因此与源不同。
5. 复制 `stage1d_project` 到临时目录，把 `assets/audio/main.wav` 换成对应产物，运行
   `cuexis_player.exe --audio-smoke-test --project <副本>`：

   | 格式 | 退出码 | 结果 |
   | --- | --- | --- |
   | FLAC | 0 | 90 帧完成，`Source: 48000 Hz / 2 ch, device: 48000 Hz / 2 ch`，`Final state: playing, queue: 5280 frames, discontinuity: 4, underruns: 0` |
   | MP3 | 0 | 90 帧完成，reload 事务完成，`underruns: 0` |
   | Ogg Vorbis | 0 | 90 帧完成，reload 事务完成，`underruns: 0` |

   脚本覆盖 load/play/2 秒 pause/resume/seek/stop/reload 以及失败 reload 探针；三种格式全部
   走完并报告 0 次 underrun。这同时说明 importer 的 canonical WAV 布局（只有 `fmt ` 与
   `data`、`byteRate`/`blockAlign` 自洽）被既有解码器接受，不需要任何运行时直解码路径。

证据目录同样在 `out/s6e_playback/`（被 `.gitignore` 忽略）。

需要注意的边界：仓库里目前**没有**一个测试把「importer 产出的 canonical WAV 字节」直接喂给
`WavDecoder`（`tests/audio_sdl/wav_decoder_tests.cpp` 只用内存里合成的 WAV，
`cuexis_playback_tests` 不链接 `cuexis::audio_sdl`）。本节的证据是运行时的端到端路径，
不是单元测试；如果要在自动化门禁里固化这条链路，需要另开一条测试项（本批次没有把它当作已完成）。

## 5. 实现过程中发现并修复的缺陷

- minimp3 会静默接受被截断的 MP3：`mp3dec_ex_read` 在残缺帧处直接返回 0，
  `last_error` 与 `offset` 都不报告异常，因此 75% 截断的文件会被当作正常输入并只产出 47 帧。
  修复方式是拿 Xing/Info header 声明的样本数与实际解码样本数比较，不足即
  `media.audio.truncated`。该缺陷在 50%/75%/90%/95% 四个截断点复验。
- libFLAC 在非 seekable 回调下会在最后一个帧边界误报 `lost sync`，导致合法 mono.flac
  失败。修复方式是提供 seek/tell/length/eof 回调。
- FLAC 截断与损坏原来都归到 `media.audio.decode_failed`。现在按 STREAMINFO 声明的样本数区分：
  解出的样本数少于声明值（或 MD5 不匹配且元数据从未交付）分别映射到
  `media.audio.truncated` 与 `media.audio.container_invalid`。
- MSVC C4611：libpng/libjpeg 的 setjmp 桥接函数只保留 C 结构与裸指针，并在这两处调用上做
  定点抑制；此前的 C4324、C4244、C4189、C2665 也已在实现中消除。

## 6. 未完成项

- 四平台字节一致（Windows MSVC、Windows MinGW、Linux GCC、Linux Clang）与安装许可证文件
  属于 hosted 证据。本页不声明这些结果；同一 SHA 的 hosted 矩阵通过后才另行记录退出报告。
- 本批次不包含 E3（媒体工具与项目/缓存集成）与 C4。

## 7. 边界

本页是 S6-E1/E2 的实现证据，不是批次退出、不是 Stage 6 关闭、不是 PR 合并，也不构成 owner
acceptance。媒体工具保持默认 `OFF`，不进入 SDK 安装闭包。
