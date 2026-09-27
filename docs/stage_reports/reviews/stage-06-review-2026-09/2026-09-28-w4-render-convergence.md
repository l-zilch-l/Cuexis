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
| `python -B tools/update_version.py --check` | `Cuexis version is consistent: 26.09.27-2` |
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
