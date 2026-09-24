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
- gate 的内存上限用例原来用 2x2 的 `rgb8.png` 加 1 MiB 上限。这个用例在 MSVC 与 Linux 上失败
  关闭，但在 MinGW 上**通过**了：MinGW 运行时基线提交低于 1 MiB，2x2 导入的分配量也低于 1 MiB，
  于是 job object 上限根本没被触发，用例什么都证明不了。现在改为解码新增的
  `image/budget_1024.png`（1024x1024，解出 4 MiB RGBA8），1 MiB 上限在四个平台都必须失败关闭。
- POSIX 的 `RLIMIT_AS` 上限与 AddressSanitizer 不兼容：ASan 需要预留 TB 级 shadow 地址空间，
  设上限后 sanitizer runtime 在 `main` 之前就 abort（hosted 日志为
  `AddressSanitizer failed to allocate 0x10000000 ... (errno: 12)`），于是 Clang ASan media-tools
  作业里**每一次** worker 调用都失败。现在 worker 在检测到 sanitizer 时跳过地址空间上限并在
  stderr 说明 `--memory-limit` 被忽略，gate 在 sanitized 构建里跳过该用例；上限本身仍由 MSVC、
  MinGW 与 GCC media-tools 三个作业覆盖。

## 6. Hosted 四平台结果与阻塞项

本节的 hosted 证据对应提交 `ec9c6f8`（push run `35977679483`、`35977679432`、`35977679343`；
PR run `35977684169`）。

### 6.1 通过的 hosted 检查

| 作业 | 结果 |
| --- | --- |
| Windows MSVC `debug` / `release` | 通过 |
| Windows MinGW `debug` / `release`（`test optional media tools`） | 构建、格式检查、`-Werror` 通过；CTest 只有 6.2 的一项失败 |
| Linux Quality `GCC media-tools` | 构建与 656 项中的 655 项通过；只有 6.2 的一项失败 |
| Linux Quality `Clang ASan + UBSan media-tools` | 同上；只有 6.2 的一项失败 |
| Linux Quality 其余 10 个作业（`GCC Coverage`、`GCC Release`、`GCC Shared Release`、`GCC Adapter Coverage`、`GCC Shader Tools Coverage`、`Clang Shared Debug`、`Clang ASan + UBSan`、`Clang ASan + UBSan shader-tools`、`clang-tidy`、`Documentation contracts`） | 通过 |

也就是说：所有图像 golden（libpng/libjpeg-turbo）、MP3 golden（minimp3）与 FLAC golden
（libFLAC）在 Windows MSVC、Windows MinGW GCC、Linux GCC 与 Linux Clang 上逐字节一致；FLAC
还额外与 ffmpeg 写出的 PCM 源逐字节相同。`audio_mono_ogg` 也一致。CLI gate 在 sanitized 构建
之外的三个作业上完整执行（含内存上限失败关闭用例）。

### 6.2 阻塞项：Ogg Vorbis 立体声的 canonical identity 不满足四平台一致

| 平台 | `audio_stereo_ogg` canonical WAV SHA-256 |
| --- | --- |
| Windows MSVC | `6d961c9ee42002962c4448109c5e20c192b44b331c46428037530e272b70db59`（已提交的 golden） |
| Linux GCC | `df73cb81daa93c9e53370a3887083c98543fb2bd68615239a2680c976a5d10e8` |
| Linux Clang（ASan+UBSan） | 同上 `df73cb81...` |
| Windows MinGW GCC | 同上 `df73cb81...` |

失败断言只有一条（该 TEST_CASE 内 47 条断言中 46 条通过）：采样率、声道数、帧数与**字节数**
全部一致，只有样本字节不同。`audio_mono_ogg` 在四个平台一致。

#### 6.2.1 差异形态的实测（`a4430c1`，分块 4096 样本）

测试在该用例里输出分块摘要与精确整数统计，四平台的结果如下（MSVC 为本机，其余为 hosted）：

| 分块 | MSVC | Linux GCC / Linux Clang / MinGW GCC |
| --- | --- | --- |
| block0（样本 0-4095） | `sha=4426ec1f…ce7a` `absSum=20451081` `sqSum=134722838039` | **完全相同** |
| block1（样本 4096-8191） | `sha=9e918ce5…1bae` `absSum=20392739` `sqSum=135221063701` | `sha=cff293f7…581f` `absSum=20392741` `sqSum=135221084785` |
| block2（样本 8192-8819） | `sha=bb9ce8f8…dcc8` `absSum=3252763` `sqSum=21916244563` | **完全相同** |
| head / tail 各 16 个样本 | 见下 | **完全相同** |

`head=-9978,-9668,-9986,-9700,-9944,-9572,-9916,-9479,-9843,-9317,-9798,-9157,-9763,-9046,-9711,-8895`
`tail=-8047,-4795,-8171,-4869,-7946,-4915,-8246,-5626,-7971,-5876,-7542,-5743,-7627,-5709,-7236,-4955`

结论：差异**不是结构性的**，而是舍入级的局部事件。

- 8820 个样本里首块（4096 个）与末块（628 个）在四个平台上逐字节相同，头尾样本也相同；差异只落在
  样本 4096-8191 这一段内。
- `absSum` 之差恰好为 **2**、`sqSum` 之差为 **21084**。按 `sqSum` 的增量反推，等价于「1 个样本差 2
  （量级约 5271）」或「2 个样本各差 1（量级之和约 10542）」——也就是**至多一两个样本、1-2 个 LSB**
  的差，而不是某一段曲线整体偏移。
- 这与「某个浮点值恰好落在整数 0.5 边界附近，两侧的取整规则/最后一位不同」一致：importer 自己的
  `quantizeToS16` 把 libvorbis 的 `float` 样本按 `floor(sample*32768+0.5)` 量化，边界上的 1 ULP
  浮点差会被放大成 1-2 个整数 LSB。

已排除的次要嫌疑：

- 依赖版本在四个平台完全相同（libvorbis 1.3.7、libogg 1.3.6）；MSVC 侧 vcpkg 构建日志为
  `/O2 /Oi /Gy /MD`，**没有** `/fp:fast`，也没有 FMA 级别的 `/arch`；`tools/media_import` 与
  `tools/media_importer` 自身在两侧都强制 `/fp:precise` 或 `-fno-fast-math -ffp-contract=off`。
- libvorbis 在 `_WIN32` 下把 `rint()` 重定义为 `floor((x)+0.5f)`，但 MinGW 同样是 `_WIN32` 却与
  Linux 一致，所以它不是分组差异的原因；它保留为“上游解码器不保证跨平台逐位一致”的旁证，而不是
  本次根因。

关于 6.2.1 的统计量本身有一个必须写明的限度：`sum(abs(x)) - sum(abs(y))` 不等于
`sum(abs(x-y))`，样本置换甚至能保持全部聚合量不变。因此那些数字只用于**刻画**差异，不构成对逐样本
差值的上界；验收判据始终是逐字节相等，诊断输出不替代它。

#### 6.2.2 根因（已复现，单变量）

`libvorbis 1.3.7` 的 `lib/os.h` 在 `<math.h>` 没有提供 `M_PI` 时回落到只有十位有效数字的 float
字面量：

```c
#ifndef M_PI
#  define M_PI (3.1415926536f)
#endif
```

MSVC 未定义 `_USE_MATH_DEFINES` 时 `<math.h>` 不暴露该常量，于是走这条回落；GCC、Clang 与 MinGW
的 `<math.h>` 提供全精度 double。本机用同一份源码预处理 `lib/os.h`（只读）得到：

| 编译器 | `M_PI` 实际展开 |
| --- | --- |
| MSVC | `3.1415926536f` |
| Windows Clang | `3.1415926536f` |
| MinGW GCC | `3.14159265358979323846` |

`M_PI` 参与解码路径：`mdct.c:65-72`（MDCT 系数表）、`lsp.c:68/251`（LSP 曲线步长）与
`lsp.c:150`。这正好解释了观测到的分组（MSVC 一侧，其余三个平台一侧）。

单变量验证：只给 libvorbis 打一个把 `M_PI` 固定为全精度 double 的补丁（`rint()`、量化函数、输入
与编译优化都不动），完整重建依赖后，同一台 MSVC 上 `audio_stereo_ogg` 的 canonical WAV 变为
`df73cb81daa93c9e53370a3887083c98543fb2bd68615239a2680c976a5d10e8`，与 Linux GCC、Linux Clang
和 MinGW GCC 完全一致；分块摘要也随之一致（block1 由 `9e918ce5…` 变为 `cff293f7…`）。修复前后对照
见 6.2.1 与本节。

#### 6.2.3 修复与 golden 重新冻结

- 新增仓库内 overlay port `vcpkg-overlays/libvorbis`，在注册表 port 之上叠加
  `0005-unify-m-pi-precision.patch`（只改回落常量），port 版本仍为 `1.3.7#4`；overlay 内容参与
  vcpkg ABI 哈希，旧缓存不会被复用。三个 media-tools preset 通过 `VCPKG_OVERLAY_PORTS` 使用它，
  因此四个平台构建同一份依赖源码。
- 解码器身份字符串与 profile identity 更新为
  `libvorbis-1.3.7-pinned-mpi-libogg-1.3.6`，profile 由
  `46a74958149d185b6963f3ed28cffb2665b3118fea1d83ffa8b742e3c65beee8` 变为
  `928c22b9761bca9829aca174a826334d2b8ce59069fe67050ab6323eafdc4610`，使旧 profile/缓存不能被
  沿用。
- 重新冻结 golden 的**范围**：18 个 golden 中 17 个只改了 `profile` 字段（内容摘要与字节数完全
  不变），只有 `audio_stereo_ogg` 的内容摘要从 `6d961c9e…` 变为 `df73cb81…`。没有批量改内容摘要，
  也没有给任何产物加 epsilon、丢低位或平台分支。
- 本机复核：`debug-media-tools` 下 `cuexis_media_import_tests` 18 个 TEST_CASE、534 条断言全通过；
  `ctest -R media` 3/3 通过（含 CLI gate）；改动过的媒体源文件在 MinGW g++ 20
  `-Wall -Wextra -Wpedantic -Werror -fsyntax-only` 下通过；`clang-format --dry-run --Werror` 通过。

四平台字节相等的最终确认由本次提交的 hosted 矩阵给出（MSVC、MinGW、Linux GCC、Linux Clang
media-tools 四个作业）。

### 6.3 Version Gate

PR 触发的 `Version Gate` 失败与 E1/E2 无关，是日期滚动：`version.release_date.stale: candidate
date 26.09.23-1 is before trusted UTC date 2026-09-24`。该失败在 C3 之前的 tip（`5c638ea`）上
同样复现，而两个 SHA 在 `--trusted-utc-date 2026-09-23` 下都通过。修它需要版本号推进，超出本批次
范围，因此没有改动。

## 7. 未完成项

- 四平台字节一致的 hosted 确认（本次提交的媒体作业）；本地与单变量实验已给出修复前后对照。
- 安装许可证文件（`THIRD_PARTY_NOTICES.md`、`DEPENDENCY_POLICY.md` 已更新；安装闭包不含媒体
  工具）属于 hosted 证据。
- 本批次不包含 E3（媒体工具与项目/缓存集成）与 C4。
- 次要清理项：`tools/media_import/CMakeLists.txt` 里 `target_include_directories` 引用了未定义
  的 `${MINIMP3_INCLUDE_DIRS}`（无副作用的空展开，minimp3 头文件经由 vcpkg include 根解析）。

## 8. 边界

本页是 S6-E1/E2 的实现证据，不是批次退出、不是 Stage 6 关闭、不是 PR 合并，也不构成 owner
acceptance。媒体工具保持默认 `OFF`，不进入 SDK 安装闭包。
