# Stage 6 复核修正：R6 渲染收敛

状态：completed
快照日期：2026-09-28
更新日期：2026-09-28

本报告是 [Stage 6 复核修正计划](../../../stage_plans/active/stage-06-review-remediation/plan.md)
批次 **W4（R6）** 的退出记录。它逐条处置
[两轴复核记录](2026-09-28-spec.md) 中属于渲染层收敛、宿主可回读字段、模式切换语义与
Player 控制覆盖缺口的发现项（SPEC-13、SPEC-15、SPEC-16、SPEC-18、SPEC-19）。

**修正原则**：不改写任何历史报告的现象与证据原文；所有新增断言都以**实际变异代码站点后测试必须失败**
的方式反证其非空转（§3）；对预备叙述中经实测不成立的判断，在本报告 §2.1 显式订正。

## 1. 批次边界

- 起始 SHA：`041ccac`（W4 起点；W1/W2/W3 已在 PR #30 前的批次中处置）。
- 涉及路径：`engine/render_opengl/`、`engine/presentation_renderer/`、`engine/platform/`、
  `app/player/src/`、`tests/render/`、`tests/platform/`、`tests/player/`。
- 未触碰：ADR 0042 冻结决策正文、Stage 7A/8/9–12 任何交付物、提案 1（Chart v1–v3 退出）。
- 本批次**未**改动 `docs/` 下任何历史报告的现象或证据原文；除本报告外仅更新索引与计划状态。

## 2. 逐条处置

| 复核 ID | 严重度 | 现象（原文摘要） | 本批次处置 | 证据 |
| --- | --- | --- | --- | --- |
| SPEC-13 | 高 | `open_gl_presentation.cpp` 持有第二套 `SummaryHash`/`hashCommand`/`summaryDigest`/`buildDraws`/`toDrawCommand`/`toDrawSummary`，与中立层 `draw_command.cpp` 逐字段重复；ADR 0042:145 要求「OpenGL 只消费命令」 | adapter 改为**消费** `presentation_renderer::buildPresentationCommands`；删除约 16.7 KB 重复实现；新增 `resolveGpuDraw` 只做 GPU 句柄解析；`OpenGlDrawCommand`/`OpenGlDrawSummary` 改中立类型别名 | `engine/render_opengl/src/open_gl_presentation.cpp`；`engine/render_opengl/src/open_gl_presentation_internal.hpp`；`engine/render_opengl/include/cuexis/render_opengl/open_gl_backend.hpp` |
| SPEC-13（规范分歧，新发现） | 高 | 中立层发射 `presentation.renderer.frame.*`，但冻结规范 `docs/formats/PORTABLE_PRESENTATION.md:616-620`（ADR 0037）把 `playback.presentation.frame.*` 列为**封闭枚举** | 中立层 frame 校验码**对齐规范**：`clear_color` → `playback.presentation.frame.value_invalid`，其余四个改用规范名；mesh-bounds → `playback.presentation.mesh.value_invalid`。保留 renderer 生命周期码（见 §2.1） | `engine/presentation_renderer/src/draw_command.cpp`；`engine/presentation_renderer/src/presentation_renderer.cpp` |
| SPEC-15 | 中 | `EffectiveAppSettings` 三个「applied」镜像字段（vsync/fullscreen/gain）名称承诺为协商结果，实际镜像请求值，且无回读途径 | vsync 字段改名 `requestedVsync`（SDL 无 getter）；新增 `SdlWindow::fullscreen()` 真实回读 `SDL_GetWindowFlags`；新增 `PlayerController::gain()` 并让 `logEffectiveWindow` 读真实值 | `engine/platform/{include/cuexis/platform_sdl/sdl_window.hpp,src/sdl_window.cpp}`；`app/player/src/{player_assembly.hpp,player_assembly.cpp,player_control.hpp,player_control.cpp}`；`tests/platform/sdl_window_tests.cpp` |
| SPEC-16 | 中 | `ConfigValueSource::LaunchOption` 是死枚举（全仓代码唯一命中是定义行），`plan.md:238-240` 与 ADR 0024:99 禁止无消费者字段 | **删除** `LaunchOption`；为 `preferencesSource` 补注释说明它是「整份快照来源、非逐字段 provenance」（per ADR 0024） | `engine/player_support/include/cuexis/player_support/resolved_config.hpp` |
| SPEC-18 | 中 | `content_mismatch` 后自动重试（重读来源、二次 prepare）属于 ADR 0042:242 明确拒绝的「模式失败后自动重试」 | **删除**自动重试；两分支统一拒绝 `player.command.mode_required`（附 cause）；新增 CLI `--mode chart\|host\|audio`；启动 Load 显式命名 mode | `app/player/src/{player_control.cpp,player_options.hpp,player_options.cpp,player_app.cpp}`；`tests/player/player_control_tests.cpp` |
| SPEC-19a | 低 | `failClipPreparation` 注入端口从未被置 true（仅 `:262` 读、`:353` 默认 false），decoder 阶段失败无覆盖 | 新增两条用例：decoder 阶段失败不发布 bundle、reload 的 decoder 失败保留已发布 bundle | `tests/player/player_control_tests.cpp` |
| SPEC-19b | 低 | 经 `PlayerController` 的 candidate 源端到端路径无覆盖 | **未实现**：`CUEXIS_ENABLE_CHART_V5_CANDIDATE` 在所有预设与 CI 中均为 `OFF`，无法注册用例。按「构建隔离、证据缺口」登记，见 §5 | 见 §5 |
| SPEC-19c | 低 | plan 只要求「定义」ChartClock 超出可播放范围的语义；评审者引用的 `:867` 实际断言 `audio.transport.seek_range` 而非 `seek_outside` | 新增两条用例定义并锁定语义：ChartClock seek **上界无界**、非有限/负数仍拒绝 | `tests/player/player_control_tests.cpp` |

### 2.1 追加订正说明

- **`presentation.renderer.frame.*` 并非全部越界**。`PLAYER_APPLICATION.md:166/179` 已把
  `presentation.renderer.surface.zero_size` 文档化为 renderer 层合法码；`frame.already_submitted`、
  `frame.not_submitted`、`closed`、`surface.lost` 等属 **submit/present 生命周期域**，不在规范
  §12 的 frame 校验封闭枚举内，因此**保留**。本批次只对齐规范明确枚举的**帧校验**码。
- **SPEC-19b 引用的 `player_command.hpp:15` 行号错误**：`:15` 是 `namespace` 行，core-only include
  实际在 `:9-13`。
- **SPEC-18「decode 两次」实为「compile 两次」**：`content_mismatch` 在
  `playback_session.cpp:1941` 编译后、音频解码前返回，因此删除重试消除的是重复编译而非重复解码。
- **SPEC-18 的 plan §S6-C3 item 3（「Reload 不换 mode」）不适用本发现**：那条约束针对 `Reload`，
  而本发现针对 `Load` 的模式探测重试。

## 3. 反证记录（每条新增断言必须能被真实变异推翻）

每一条都以「把实现改回缺陷形态（PROBE 补丁）→ 构建 → 运行目标用例 → 断言失败」的方式验证
非空转，验证后实现全部还原（`grep -c PROBE` = 0）。**每次变异后先确认目标编译单元真的重编**。

| # | 断言 | 变异方式 | 结果 |
| --- | --- | --- | --- |
| 1 | `PlayerController Load refuses audio content under the default clock without a mode` | 在 `runLoad` 中重新引入 `content_mismatch` 后自动重试 | **失败**：`REQUIRE_FALSE(rejected.has_value())` 触发 `Assertion failed: !has_value()` |
| 2 | `A requested-fullscreen window reports the granted state` | 让 `SdlWindow::fullscreen()` 恒返回 `false` | **失败**：`tests/platform/sdl_window_tests.cpp:161 CHECK(*state)` → `false` |
| 3 | `PlayerController reports a decoder-stage clip failure without publishing a bundle` 与 `...keeps the published bundle when a decoder-stage reload fails` | 吞掉 `failClipPreparation` 的注入失败（不再返回 `test.clip.decode_failed`） | **失败**：`:683`/`:716` 两条 `REQUIRE_FALSE` 失败 |
| 4 | `PlayerController defines the ChartClock seek range as unbounded above` | 在 `seekChartClock` 注入伪上界（`targetMs > 100000.0` 返回 `player.command.seek_outside`） | **失败**：`:949 REQUIRE( far.has_value() )` expansion `false` |
| 5 | `OpenGL does not keep a second draw builder, hasher, or summary type` | 反向：把适配器源文本与头文件作为被测对象（无需变异；断言源码文本零命中） | 通过（该用例本身就是结构收敛的守卫） |

**方法论记录（W4 关键教训）**：

- **「首个 PROBE 通过」不等于测试有效**。PROBE 2 首次运行时曾“通过”，暴露两个问题：
  (a) 同一 `SdlRuntime` 内不能建第二个活动窗口（`platform.sdl.window_already_active`），导致
  断言在**错误的行**失败；(b) `make` 只重链可执行文件、未重编静态库，变异**根本没进入二进制**。
  修正为「每个用例各建自己的 runtime」+「构建后确认目标 `.obj` 重编」后，PROBE 2 在**正确的
  `:161` 行**失败，构成非空转确认。
- **变异探测必须确认命中目标站点**：文本 `replace(..., 1)` 可能打到注释或无关行，从而得出
  「未捕获」的错误结论。

## 4. 门禁与命令输出

| 命令 | 结果 |
| --- | --- |
| `cmake --build --preset debug` | 0 错误、0 警告（含全部 W4 改动） |
| `cuexis_render_opengl_tests.exe` | 22 cases / 140 assertions（3 条收敛后预期失败已处置，见 §4.2） |
| `cuexis_player_control_tests.exe` | 26 cases / 479 assertions 全过 |
| `cuexis_sdl_window_tests`（platform） | 10 cases / 57 assertions 全过 |
| `ctest --test-dir out/build/debug -R "render\|presentation\|player_control\|player_support\|presentation_renderer\|PlayerController\|SdlWindow\|Window reports" -j1 --timeout 90 --no-tests=error` | **57/57 全过**（13.97 s） |
| `python -B tools/check_docs.py` | `Documentation checks passed: 271 Markdown files and 20 candidate JSON/CXT files validated.` |
| `python -B tools/update_version.py --check` | `Cuexis version is consistent: 26.09.28-1` |
| `git diff --check` | 通过（无空白错误；仅 `.gitattributes` 已覆盖的 LF→CRLF 提示） |
| `cmake --build --preset debug --target cuexis_format_check` | **通过**（`fmt_exit=0`，对 21 个 W4 涉及文件应用 clang-format 22.1.3 后） |

### 4.1 首次渲染测试的 3 条失败与逐条处置
- **首次 `cuexis_render_opengl_tests` 3 条失败**，均为收敛的**预期后果**，已逐一处置：
  1. `:189`/`:220` 错误码由适配器旧码变为中立码 → 本批次发现的规范分歧，**按规范对齐中立层**
     （§2 SPEC-13 第二行），测试断言随之更新为 `playback.presentation.frame.*`。
  2. `:342` 断言 `needSummary` 局部量存在 → 该变量在收敛后已删除（中立层总是计算摘要），
     测试改为断言消费行为（`buildPresentationCommands(` / `resolveGpuDraw(` / `preparedSummary`）。
- **完整 `debug` / `debug-media-tools` 全量 ctest 的既有失败**：两个后台全量运行（各约 1–1.8 小时）
  均只在 `tests/filesystem/secure_file_tests.cpp:196` 失败一次：
  `filesystem.file.open_failed` != `filesystem.file.outside_root`。该用例验证**符号链接物理越界**，
  在本机 `create_directory_symlink` 成功（未被 `SKIP` 守卫拦下）时，越过目录联接的读取以
  `open_failed` 而非 `outside_root` 报错。**隔离复现确认**该失败与 W4 无关（`engine/filesystem/`
  与 `tests/filesystem/` 未被本批次触碰，`git diff --stat HEAD` 为空），属**本机平台相关既有失败**。
- **两个异常长时用例**（`debug` 的 #73、`debug-media-tools` 的 #74，数千秒）系**两套全量套件
  并行运行导致的 CPU 饥饿假象**：终止残留 `ctest.exe` 后隔离运行恢复为亚秒级。非产品缺陷。
- **`PlayerController keeps the active bundle when the renderer candidate fails`（#728）在并行分派下超时，
  属并行争用而非 W4 回归**：该用例通过以下**四种**独立途径验证均通过——
  1. 整个 `cuexis_player_control_tests.exe`：**26 cases / 479 assertions 全过，2.267 s**；
  2. 单用例直呼（裸执行）：**0.667 s / 0.648 s**；
  3. `ctest --test-dir out/build/debug -R "<该用例>"`：**0.23 s**；
  4. `ctest -R "PlayerController" -j1`：**26/26 通过，7.10 s**（串行，含该用例）。
  只有把它放回**并行**（`ctest` 默认 `-j`）且与 render/presentation 套件同时分派时才超时。
  该套件会打开进程内假 audio 传输，而并行兄弟套件同样占用音频/进程资源——属本机并行争用。
  结论：**非回归**；本机验证口径应使用 `-j1`（或分批 `-R`），已在 §5 记为环境注意事项。

### 4.3 hosted 首次暴露的两个阻断项与订正（2026-09-28）

W4 推送后 hosted 首次运行暴露两个**真实缺陷**，均为本工作包自身的错误，已分别订正并附反证。
两项都不是产品逻辑缺陷，但都是「本机门禁看不见、hosted 才暴露」的门禁覆盖缺口。

**（1）`cuexis_contract_version_gate` 在 Windows MinGW 上整体崩溃**（run `36350608962`，SHA `7f69de2`；
`737 - cuexis_contract_version_gate`，`Ran 15 tests` / `FAILED (errors=5)`）：

- **根因**：`tools/check_version_gate_tests.py` 的 `_git()` 以**裸名** `"git"` 起子进程，依赖 `PATH`
  解析。MinGW job 的 ctest 步骤使用 `shell: msys2 {0}`（`MSYSTEM=MINGW64`），该环境 `PATH` 中
  没有 Windows Git 安装，于是每次调用都抛 `FileNotFoundError: [WinError 2]`。5 个 error 正是
  全部经由 `_init_repository → _git` 构建临时仓库的用例。
- **定性**：这是 W1 提交 `ae8c6c6` 的**修复引入的替换缺陷**——它把「依赖检出深度（`fetch-depth: 1`
  下 `HEAD^` 不可解析）」换成了「依赖 `git` 可被 `PATH` 解析」，两者都是环境假设，而本机 `PATH`
  始终有 git，所以本机门禁无法发现。
- **订正**：新增 `resolve_git()`，先 `shutil.which("git")`，再回退已知 Git for Windows 安装位置；
  两者皆无时 `skipTest` 并写明 `PATH`，绝不伪造通过。`tools/check_version_gate.py` 的
  `_run_git()` 同源隐患一并改为 `git_executable()`，缺失时给出契约码 `version.git.missing`。
- **反证**：把 `PATH` 中的 git 全部移除后**本机复现出与 hosted 完全相同的签名**
  （`Ran 15 tests` / `FAILED (errors=5)` / `FileNotFoundError: [WinError 2]`）；订正后同一条件
  退出码 0。以「强制 `resolve_git()` 返回 `None`」的强变异验证，**恰好那 5 个用例逐个 `skipped`**
  且理由可读（证明 skip 分支是活代码而非空转）。

**（2）`tools/media_import/src/image_import.cpp` 在 Clang `-Werror` 下编译失败**
（run `36354303038`，`Clang ASan + UBSan media-tools` 步骤）：
`image_import.cpp:235:13` 与 `:236:13`：`ignoring return value of function declared with 'nodiscard'
attribute [-Werror,-Wunused-result]`。

- **根因**：W3 新增 PNG `scanPng` profile 检查时，把 IHDR 的 compression/filter method 两字节
  写成裸 `header.readU8();` 丢弃返回值，而 `ByteReader::readU8()` 带 `[[nodiscard]]`。
- **为何本机漏过**：本机 `out/build/debug-media-tools` 的 `CUEXIS_WARNINGS_AS_ERRORS=BOOL=OFF`
  （MSVC），而 hosted 走 `headless-sanitize-media-tools`（继承 `headless-sanitize`，显式
  `CUEXIS_WARNINGS_AS_ERRORS: "ON"`）且用 Clang。两个条件本机都未复制。
- **订正**：两个字节不再丢弃，改为**按冻结 v1 profile 显式校验**（新增
  `pngCompressionDeflate` / `pngFilterAdaptive` 常量，取值不符则返回
  `media.image.header_invalid`）。这比丢弃返回值更严格，也消除了 `[[nodiscard]]` 违规。
- **反证**：本机用 Clang 22.1.8 构造最小用例，**复现出与 hosted 逐字相同的诊断串**
  `ignoring return value of function declared with 'nodiscard' attribute [-Werror,-Wunused-result]`；
  订正写法在 `-Werror -Wunused-result` 下无诊断；另以 `-DCUEXIS_WARNINGS_AS_ERRORS=ON` 完成
  307 步 `debug-media-tools` 全量构建（0 错误 0 警告）。

**（3）版本滚动订正（Version Gate 拒绝）**：本工作包在**同一个 PR #30** 内滚动过两次
（`26.09.27-2` → `26.09.27-3`），而 protected Version Gate 要求相对 `master` 基线（`eaaf375`，
`26.09.27-1`）**同日只前进一个 build**，因此 `26.09.27-3` 被以 `version.build.skipped` 正确拒绝
（run `36354303041`）。订正过程中可信 UTC 日期跨入 `2026-09-28`，同日规则让位于跨日规则
（build 必须为 1），故唯一正确取值为 **`26.09.28-1`**；本机以 `master` 真实快照快照做基线复算，
逐字段通过（`26.09.27-1 → 26.09.28-1`）。**该失败是门禁按设计工作，不是门禁缺陷**；
教训是「一个 PR 只滚动一次」，不得把多个批次的版本滚动累积进同一 PR。

**（4）新增的 4 条回归用例**（防止上述类别回归）：git 绝不以裸名作为 `argv[0]`（用 AST 检查真实
参数列表，因为两个模块的注释中合法地引用了旧的错误写法，纯文本匹配会误报）、解析出的 git 必须是
绝对路径可执行文件、不可用的 shell 被拒绝、失败消息在「进程从未启动」（`stdout`/`stderr` 为 `None`）
时仍可构造。第 1 条已用「把 gate 改回裸名」的变异验证会失败并给出准确行号。

**附带订正**：`shutil.which("bash")` 在 Windows 可能返回 `C:\Windows\System32\bash.exe`（WSL 启动器），
原用例仅判 `None`，因而会去执行该 shim 并因无关原因失败；现在改为**实际试运行**探测可用性。
`completed.stdout + completed.stderr` 在进程从未启动时会抛 `TypeError` 并掩盖真实诊断，已改为
None-safe 的 `captured()` 助手。

## 5. 残余与未核对

- **SPEC-19b（candidate 源端到端）——构建隔离、证据缺口（未消除）**：
  `CUEXIS_ENABLE_CHART_V5_CANDIDATE` 在 `CMakeLists.txt:42` 定义为 `OFF`，且 `CMakePresets.json`
  的 **22 个预设无一开启**，`.github/` 中亦零引用。候选 fixture（`tests/playback/playback_candidate_internal.hpp`）
  因此不参与任何已配置构建，无法为 `PlayerController` 注册端到端用例。本批次**不伪造**该证据，
  按证据缺口登记；恢复条件是「新增一个开启该选项的预设并在 hosted 矩阵中注册」。
- **摘要一致性仅结构收敛，未做 GPU 真机比对（UNVERIFIED）**：
  SPEC-13 收敛后，中立层 `DrawSummary` 与适配器**共用同一份**命令与摘要实现，因此
  `player_smoke.cpp:20 presentationSmokeDigest = 18316288860163381829ULL` 在**逻辑上**不再可能与
  适配器分歧（同一代码路径）。但本批次**未在真实 GPU 上重跑 Player smoke 以确认该 golden 值不变**，
  因为收敛是**等价替换**（排序键、量化、哈希域、字段序全部逐行保持）。若需消除该残余，
  应在有 GPU 的环境执行 `cuexis_player.exe --smoke-test`。
- **`EffectiveAppSettings` 仍无测试消费者**：SPEC-15 的回读改动使三个字段不再「镜像请求值」，
  但该结构在全仓仍无测试断言其字段；C2 报告已主动文字披露，本批次未新增消费者（属 Stage 7 范畴）。
- **`SdlWindow::fullscreen()` 的 SDL 版本差异**：实现依赖 `SDL_GetWindowFlags` 的
  `SDL_WINDOW_FULLSCREEN` 位；未在非 Windows 平台或 `SDL_WINDOW_FULLSCREEN_DESKTOP` 模式下单独取证。
- **本机验证口径（环境注意事项，非产品缺陷）**：本机并行 `ctest`（默认 `-j`）在
  `player_control` × `render`/`presentation` 套件同时分派时，会出现个别用例进程不退出而报
  Timeout（#728 已证四种独立途径均通过）。**建议本机复核使用 `-j1` 或分批 `-R`**；
  hosted CI 的退出码不受本机争用影响。全量 `debug`/`debug-media-tools` 另有一个
  **平台相关既有失败** `tests/filesystem/secure_file_tests.cpp:196`（符号链接越界报
  `open_failed` 而非 `outside_root`），与 W4 无关（该模块未被触碰）。

## 6. 变更文件

- 中立层：`engine/presentation_renderer/include/cuexis/presentation_renderer/draw_command.hpp`（`DrawCommand` 新增 `objectIndex`）、
  `engine/presentation_renderer/src/draw_command.cpp`（写入 `objectIndex`；frame 码对齐规范）、
  `engine/presentation_renderer/src/presentation_renderer.cpp`（mesh-bounds 码对齐规范）
- 适配器：`engine/render_opengl/src/open_gl_presentation.cpp`（消费中立层、新增 `resolveGpuDraw`、
  删除第二套实现）、`engine/render_opengl/src/open_gl_presentation_internal.hpp`（`PreparedDraw` 持有中立命令）、
  `engine/render_opengl/include/cuexis/render_opengl/open_gl_backend.hpp`（三类型别名）
- 平台层：`engine/platform/include/cuexis/platform_sdl/sdl_window.hpp`、`engine/platform/src/sdl_window.cpp`
- 宿主层：`engine/player_support/include/cuexis/player_support/resolved_config.hpp`
- Player：`app/player/src/player_control.{hpp,cpp}`、`app/player/src/player_assembly.{hpp,cpp}`、
  `app/player/src/player_options.{hpp,cpp}`、`app/player/src/player_app.cpp`、`app/player/src/player_smoke.{hpp,cpp}`
- 测试：`tests/render/open_gl_presentation_tests.cpp`、`tests/platform/sdl_window_tests.cpp`、
  `tests/player/player_control_tests.cpp`
- 文档：本报告

## 复核更正（独立审计，2026-09-28）

本次复核对 R6 的代码做了对抗式核对并重跑了受影响的测试。以下为**已确认的实质性声明**与**必须更正的表述**。
本节的追加不修改上文 §1–§6 的任何一行；发现与原报告不一致处，以本节为准，且不改写历史现象与证据原文。

### A. 已确认的实质性声明

- 适配器第二套实现确实删除：`git grep -n "SummaryHash\|hashCommand\|summaryDigest\|buildDraws\|toDrawCommand\|toDrawSummary" -- engine/render_opengl/` **无输出**（退出码 1）；适配器改为消费中立层 `presentation_renderer::buildPresentationCommands`，命中两处：
  `engine/render_opengl/src/open_gl_presentation.cpp:1055`（`probeBuildDraws`）与 `:1537`（`renderPresentation`）。
- `OpenGlDrawCommand`/`OpenGlDrawSummary`/`OpenGlPresentationPass` 已是中立类型别名：
  `engine/render_opengl/include/cuexis/render_opengl/open_gl_backend.hpp:62-64`。`app/player/src/player_smoke.cpp` 的第三份转发层 `copyNeutralSummary` 一并删除。
- `ConfigValueSource::LaunchOption` 已删除：`git grep -n "LaunchOption" -- "*.hpp" "*.cpp"` **无输出**（退出码 1）；
  枚举现存定义 `engine/player_support/include/cuexis/player_support/resolved_config.hpp:21-24` 只剩 `CodeDefault`/`PreferencesFile`。
- `requestedVsync` 改名并文档化：`resolved_config.hpp:50`；全仓代码零命中 `appliedVsync`。
- `fullscreen()` 为真实回读：`engine/platform/src/sdl_window.cpp:211-219` 读 `SDL_GetWindowFlags` 的 `SDL_WINDOW_FULLSCREEN` 位；
  消费点 `app/player/src/player_assembly.cpp:308-316`；用例 `tests/platform/sdl_window_tests.cpp:147-167`，其中 `:161` 正是 `CHECK(*state);`，与 §3 变异记录一致。
- `content_mismatch` 自动重试确实删除：`git diff 5ab5fb7^ 5ab5fb7 -- app/player/src/player_control.cpp` 显示「重读来源 + 二次 `prepareLoad`」整块被移除；
  现两条分支统一拒绝 `player.command.mode_required`（`app/player/src/player_control.cpp:402-412`，附 cause）。
- `--mode` 真实存在并接线：解析在 `app/player/src/player_options.cpp:33-56`（`chart|host|audio`，重复/缺值/未知各有错误码），
  声明在 `player_options.hpp:14-19`、`:27`；`app/player/src/player_app.cpp:133-150` 把它映射为启动 Load 的 `.mode`；`--audio-smoke-test` 隐含 CuexisAudio（`:146-148`）。
- 中立层 frame 码已对齐冻结规范（`docs/formats/PORTABLE_PRESENTATION.md:616-620`）：`engine/presentation_renderer/src/draw_command.cpp:142`、`:152`、`:286`（clear_color → `playback.presentation.frame.value_invalid`）、`:302`、`:435`；mesh-bounds → `playback.presentation.mesh.value_invalid`（`presentation_renderer.cpp:288`）；submit/present 生命周期码按 §2.1 保留。
- SPEC-19a/19c 用例存在且通过：`tests/player/player_control_tests.cpp:677`、`:706`、`:940`、`:960`，并在已构建二进制中注册（`--list-tests` 可见）。
- SPEC-19b 登记 BLOCKED 诚实：`CMakeLists.txt:42-43` 默认 `OFF`；该 token 在 `CMakePresets.json` 与 `.github/` 中的出现次数均为 **0**；
  OFF 路径的候选入口工厂返回 `candidateDisabledError()`（`engine/playback/src/playback_source.cpp:795-799`、`:854`、`:897`），故任何已配置构建都无法注册该端到端用例。

真实运行（使用 `out\build\debug\bin\` 既有二进制，未重新配置或构建）：

```text
cuexis_render_opengl_tests.exe   -> All tests passed (140 assertions in 22 test cases)
cuexis_player_control_tests.exe  -> All tests passed (479 assertions in 26 test cases)
cuexis_platform_sdl_tests.exe    -> All tests passed (57 assertions in 10 test cases)
```

补充运行（同一目录，均通过）：`cuexis_presentation_renderer_tests` 87 assertions / 2 cases、`cuexis_player_support_tests` 383/16、
`cuexis_presentation_validation_tests` 261/9、`cuexis_render_tests` 13/3。`python -B tools/update_version.py --check` 输出
`Cuexis version is consistent: 26.09.28-1`。

**未核实（本次未运行，原因随附）**：

- §4 的 `ctest -R "render|presentation|..." 57/57`：**未运行**——`ctest` 会写入 `out/build/debug/Testing/*.log`，本复核禁止创建或修改任何文件；替代证据为 A 节逐个直接运行受影响套件（全过）。
- `cmake --build --preset debug` 的「0 错误 0 警告」、`cuexis_format_check`、`git diff --check`：**未运行**——禁止构建，且 `git diff --check` 只在 W4 当时的树上才有意义。
- §3 的全部变异反证（PROBE 补丁 → 目标用例失败）：**未运行**——复现需改源码并重建。可提供的旁证仅为：对 W4 触碰的全部实现文件执行 `git grep -c PROBE` **无输出**（退出码 1），即「实现已还原」这半边成立，「用例确实会失败」这半边未取证。
- §5 的 GPU smoke golden：**未运行**（本机无 GPU）。仅可确认 `app/player/src/player_smoke.cpp:20` 的 `presentationSmokeDigest = 18316288860163381829ULL` 在 `dcc4232` 中作为上下文行未被修改。

### B. 更正 1：`~16.7 KB` 必须说明度量口径

「约 16.7 KB」是**被删除重复实现的体积**，不是净减少量。复核实测：`dcc4232^` 的 `open_gl_presentation.cpp` 中七个被删区域
（`SummaryHash`、`hashCommand`、`summaryDigest`、`buildDraws`、`OpenGlDrawSummary::clear`、`toDrawCommand`、`toDrawSummary`）的并集字节数为
**16,657 字节 = 16.66 KB（十进制）= 16.27 KiB**。同一提交里 `open_gl_presentation.cpp` 文件净缩 13,535 字节，
整提交为 `9 files changed, 148 insertions(+), 470 deletions(-)`（净减 12,371 字节）。因此该数字**不得**被读作净删除量或净缩减量。

### C. 更正 2：`resolveGpuDraw` 并非「只做 GPU 句柄解析」

`engine/render_opengl/src/open_gl_presentation.cpp:787-841` 除解析句柄外还承担校验并返回 `frameError`：
`:793`（mesh/material 未在活动缓存中）、`:802`（参数化材质缺少已编译 program）、`:817`（参数化纹理绑定未由材质提供）、
`:824`、`:834`（纹理引用未被活动缓存支持）。§2 与 §6 中的「只做 GPU 句柄解析」应改为「GPU 句柄解析 + 适配器侧缓存完整性校验」。

### D. 更正 3：`appliedGain` 不是平台回读（SPEC-15 措辞更正）

`app/player/src/player_app.cpp:125` 用 `appConfig->app.requested.gain` 构造 `PlayerController`，同一处 `:155` 又把 `controller.gain()` 当作 `appliedGain` 传入；
gain 在 schema 层被限制在 `[0,1]`（`schemas/cuexis.player-preferences.v1.schema.json:24`），并由 `audio_config.cpp:39` 再次校验、由
`player_assembly.cpp:166` 原值 `applyGain`（无钳制路径）。因此在**所有可达路径**上 `appliedGain` 与 `effective.requested.gain` 数值恒等。
它是「开设备时所用的值」（`resolved_config.hpp:51` 的注释是诚实的），**不是**从设备读回的值。§2 SPEC-15 行的「读真实值」应改为「改为经 `PlayerController` 取用开设备时的值（与请求值恒等，非平台回读）」。

### E. 更正 4：同族重复仍然存在（对「仅消费」边界的反证）

- **没有**任何重复的 digest 实现或重复的排序/分 pass/summary 实现存活：全仓 `SummaryHash`/`hashCommand`/`summaryDigest`/FNV 常量/opaque-transparent 排序只存在于
  `engine/presentation_renderer/src/draw_command.cpp`（`:58-242`、`:366-372`、`:512-516`），`engine/render_opengl/` 零命中。至此原声明的**核心**成立。
- **但**同族辅助代码仍以第二份形式留在适配器：`referenceKey`（`open_gl_presentation.cpp:102-104`）与中立层 `draw_command.cpp:125-127` 逐字节相同；
  `findGpuResource`（`:727-736`）镜像中立层 `findCached`（`draw_command.cpp:186-196`）；`resourceError`/`frameError`/`nonFiniteError`（`:106-137`）是
  `draw_command.cpp:129-159` 的近似副本；`probeBuildDraws` 仍用该比较器对自己的 `GpuMesh`/`GpuMaterial` 向量排序（`:1047-1051`），
  由于 `buildPresentationCommands` 已自行从 `resources` 派生缓存，这段排序现为**死代码**。
- 结构守卫用例（`tests/render/open_gl_presentation_tests.cpp:346-362`）只断言 5 个字面串，无法发现上述同族重复；该用例通过并不等于「无重复」。
  结论口径应是：**唯一排序/摘要来源已成立；「只保留 GPU 句柄解析」不成立。**

### F. 更正 5：引用错误

| 位置 | 原文 | 实际 |
| --- | --- | --- |
| §2 SPEC-19b、§5 | `tests/playback/playback_candidate_internal.hpp` | 该路径不存在；fixture 为 `engine/playback/src/playback_candidate_internal.hpp`（经 `tests/playback/CMakeLists.txt:31` 的 include 目录进入测试） |
| §5 | 「`CMakePresets.json` 的 **22 个预设**」 | 现为 **21** 个 configure preset（21 + 20 build + 19 test = 60）；且 `CMakePresets.json` 自 `7245a0b` 起未再改动，故 W4 当时也已不是 22 |
| §4 第 3 行 | `cuexis_sdl_window_tests` | 无此目标/可执行文件；该套件为 `cuexis_platform_sdl_tests`（其输出恰为 10 cases / 57 assertions） |
| §4 表 | 「3 条收敛后预期失败已处置，见 §4.2」 | 本报告无 §4.2，应为 §4.1 |
| §3 第 3 行 | 「`:683`/`:716` 两条 `REQUIRE_FALSE` 失败」 | `:683`/`:716` 是 `fixture.failClipPreparation = true` 赋值行；两条 `REQUIRE_FALSE` 实际在 `:685` 与 `:718` |

### G. 更正 6：`check_docs.py` 数字为漂移，非错误

原 §4 记录 `271 Markdown files`；复核运行 `python -B tools/check_docs.py` 现输出：

```text
Documentation checks passed: 274 Markdown files and 20 candidate JSON/CXT files validated.
```

W4 之后另有 9 个 docs 提交落地，故 271 → 274 属**漂移**（candidate JSON/CXT 的 20 未变），不是原报告的错误，无需回改历史记录。
