# Stage 6 复核：Standards 轴

状态：active
快照日期：2026-09-28
更新日期：2026-09-28

本文件只记录 **Standards 轴**：diff `13dab93...eaaf375` 是否违反仓库文档化的编码/文档标准，
以及 Fowler《重构》第 3 章 smell 基线。Spec 轴见 [Spec 记录](2026-09-28-spec.md)；
两轴不合并。范围、方法、局限与门禁命令结果见[汇总](2026-09-28-summary.md)。

标准来源：`AGENTS.md`、[CODE_POLICY](../../../guides/CODE_POLICY.md)、
[DEPENDENCY_POLICY](../../../guides/DEPENDENCY_POLICY.md)、[VERSIONING](../../../guides/VERSIONING.md)、
[DOCUMENTATION_POLICY](../../../DOCUMENTATION_POLICY.md)、[BUILDING](../../../guides/BUILDING.md)、
[PROJECT_GUIDE](../../../PROJECT_GUIDE.md)、[API Reference](../../../api/README.md)、
[格式契约](../../../formats/README.md)。工具已强制的部分（架构测试、target 依赖 allowlist、
`cuexis_format_check`、`tools/check_docs.py`）不重复报。

---

## 1. 文档化契约违规

### STD-01 候选预算诊断码未使用契约码

- **现象**：候选 Chart entry 的 16 MiB 上限在 `engine/cxc/src/cxc_candidate.cpp:532-534` 报出
  `cxc.budget.exceeded`；同一文件的 `:93` 定义了 `cxc.candidate.budget_exceeded`，并在 `:356`、`:360`
  的其它候选预算路径使用它。
- **证据**：[CHART_ENTRY_V1_FORMAT.md](../../../formats/CHART_ENTRY_V1_FORMAT.md) 的诊断表第 122 行规定
  `cxc.candidate.budget_exceeded` = "A file, section, decoded or count limit is exceeded"；
  第 118–126 行是候选 entry 的完整诊断集。`engine/cxc/src/cxc_candidate.cpp` 是本轮新增文件
  （+642 行）。**父代理已核实**。
- **影响**：调用方与 golden 按契约码分支时会漏判该失败；同一语义在同一文件内有两种可序列化诊断码，
  违反 [CODE_POLICY](../../../guides/CODE_POLICY.md) 的稳定诊断约定。
- **处置建议**：把 `:533` 改用候选码（复用 `:93` 的常量），并在候选负例中固定断言该码。

### STD-02 报告声称交付的文档不存在

- **现象**：
  [2026-09-27-s6-c4-reference-host-and-player-distribution.md](../../stages/stage-06/2026-09-27-s6-c4-reference-host-and-player-distribution.md)
  §2 声称交付了 `examples/reference_host/README.md`。
- **证据**：`examples/reference_host/` 实际只有 `CMakeLists.txt` 与 `src/`（7 个源文件），无任何 `.md`；
  `docs/guides/BUILDING.md:468` 已把该 README 当作既有入口引用。**父代理已核实**。
- **影响**：违反 [DOCUMENTATION_POLICY](../../../DOCUMENTATION_POLICY.md)「Report：某个时间点的实施、审查和证据」与
  「不得把未发生写成已发生」；构建指南指向不存在的文件。
- **处置建议**：补写该 README（宿主主循环、命令循环、provider、构建/运行步骤），或修正 C4 报告与
  `BUILDING.md` 的表述，二者取其一，不要留悬空引用。

### STD-03 Stage 6 状态词与已归档事实冲突

- **现象**：Stage 6 已 `completed` 并归档（PR #29 已合并、计划位于 `completed/`），但多处仍写 not-done：
  - [stage-06/plan.md](../../../stage_plans/completed/stage-06/plan.md)`:51`「E1/E2 与 F1 尚未退出」、
    `:103` F1 行末「`S6-F2` 未开始」、`:106`「Stage 6 的 active 状态不等于…」，
    与同文件 `:3`（completed）、`:104`（F2 completed）冲突；
  - [completion.md](../../stages/stage-06/completion.md)`:46` F2 行「报告已形成；owner 接受未记录」，
    与 `:15`、`:129`「明确接受…（已记录）」冲突；
  - [ROADMAP.md](../../../ROADMAP.md)`:35-36`、`:107`、`:113` 仍称 Stage 6「恢复 active」/「实施」，
    与 `:46`「已于 2026-09-27 关闭并归档」冲突；
  - [VERSIONING.md](../../../guides/VERSIONING.md)`:175` 标题「Stage 6 已冻结的版本方向（尚未实施）」
    与 `:190`「S6-B1 已落下…」及阶段关闭冲突。
- **证据**：`tools/check_docs.py` 通过（该类一致性不在其校验范围）；plan.md 的三处由**父代理逐行核实**。
- **影响**：[DOCUMENTATION_POLICY](../../../DOCUMENTATION_POLICY.md) 的权威顺序与状态字段规则被破坏；
  同一文档内互斥的状态会让下游读者无法判断 Stage 6 是否已关闭，并会污染后续交接。
- **处置建议**：以 `CURRENT_STATUS.md` 为唯一当前摘要，把 plan/completion 的历史行改为指向关闭报告的
  一次性修正（不改写证据，只改状态词），并同步 `ROADMAP.md`、`VERSIONING.md`。

### STD-04 已记录的整改项未落实（AP-09）

- **现象**：`app/player/src/player_app.cpp:27-43` 的 `audioStateName` 与
  `app/player/src/frame_diagnostics.cpp:19-35` 的 `stateName` 是逐字相同的 `audio::PlaybackState` 级联。
- **证据**：[full-review-2026-08 复核记录](../full-review-2026-08/2026-08-29-review.md) 的 AP-09 与
  [整改计划](../../../stage_plans/reviews/full-review-2026-08/remediation-plan.md) 第 1240 行明确要求
  「AP-09 合并 `stateName`/`audioStateName`」；两个文件都在本批 diff 内，重复仍在。
- **影响**：已记录、已排期的整改项在 C3 拆分 `player_app.cpp` 时被复制而不是合并，
  等于在重构中把技术债复制了一份。
- **处置建议**：在诊断模块保留一个映射函数，另一处调用它。

---

## 2. smell 基线（全部为判断项）

### STD-05 possible Duplicated Code

同一种逻辑形状在本次改动的一个以上 hunk 或文件里出现，共 8 组：

1. **中立层与 adapter 各一份 digest**：`engine/presentation_renderer/src/draw_command.cpp:58-242`
   （`SummaryHash`/`hashCommand`/`summaryDigest`/`Point3`/`transformPoint`/`finitePoint`/`finiteMatrix`）
   与 `engine/render_opengl/src/open_gl_presentation.cpp:810-921` 逐字重复，域串同为
   `cuexis.validation.summary.v1`。后者是 SPEC-13 的直接后果。
2. **两套并行类型**：`DrawCommand`/`DrawSummary`/`PresentationPass`
   （`engine/presentation_renderer/include/.../draw_command.hpp:20-53`）与 `OpenGlDrawCommand`/
   `OpenGlDrawSummary`/`OpenGlPresentationPass`（`engine/render_opengl/.../open_gl_backend.hpp:54-89`）
   字段一一对应；头注释自己声明"covers the same fields as the OpenGL summary v1 digest"。
3. **错误工厂**：`auto fail(std::string, std::string)` 在 `engine/chart/src/` 的 5 个文件各写一份
   （`packed_chart_primitives.cpp:20`、`packed_chart_io.cpp:35`、`packed_chart_tables.cpp:26`、
   `packed_profile.cpp:16`、`packed_semantic_identity.cpp:33`）；媒体工具同样有 4 份
   （`media_internal.cpp:36 mediaError`、`media_importer/src/publish.cpp:20 ioError`、
   `worker_process.cpp:16 workerError`、`media_json_internal.cpp:19 invalid`）。
4. **版本门禁工作流**：`.github/workflows/version-gate.yml:52-65`、`:98-111`、`:141-154`
   三处近乎逐字重复 trusted-baseline materialize 循环与 `version.bootstrap.required` 失败块；
   `tools/check_version_gate.py:88-92` 又与 `tools/update_version.py:150-158` 的 `check_current` 同形同文案。
5. **发布事务**：`tools/asset_publish/src/asset_publish.cpp:826-837`（`adoptGeneration`）、
   `:917-980`（`commitPackage`）、`:992-1003`（`rollbackPackage`）各写一遍
   「写独占临时文件 → rename → 失败恢复」；父目录校验在 `:903-907`、`:1013-1017`、`:1046-1050` 重复三次。
   `tools/media_import/src/media_cache` 侧的 `publishImmutable`(`publish.cpp:75`) 与
   `promoteTemporary`(`:164`) 也重复"存在→读/哈希→冲突"形状。
6. **配置层**：`engine/player_support/src/user_preferences.cpp:111-126` 的 `unsupportedVersion` 与
   `audio_device_profile.cpp:10-25` 的 `unsupportedProfileVersion` 形状相同（仅 format 常量不同）；
   `user_preferences.cpp:320,331,341,349,355` 的临时文件清理块重复 5 次；
   `config_location.cpp:29-45` 复刻 `schemas/cuexis.player-preferences.v1.schema.json:27` 的 profile ID pattern；
   `engine/audio_sdl/src/sdl_audio.cpp:69-88` 的 `findUniqueDevice` 重写了
   `audio_device_profile.cpp:132-161` 的唯一匹配+歧义逻辑，并把 `player.audio_profile.*` 码硬编码进后端。
7. **cxc 候选字段表**：`engine/cxc/src/cxc_candidate.cpp:182-194` 与 `:485-497` 是逐字相同的
   11 元素 `constexpr std::array fields`；`engine/playback/src/playback_source.cpp:852-893` 与
   `:895-936`（`fromCxcFileEntry`/`fromCxcMemoryEntry`）约 30 行同构，仅 loader 与 tag 不同。
   `candidate_lowering.hpp:42` 的内联字面量 `"candidate.static-tap-lanes4-v1"` 重复了
   `cxc_candidate.cpp:27` 的命名常量。
8. **CMake 门禁脚本**：`cmake/VerifyReferenceHost.cmake:41-51` 的 `cuexis_host_run_checked` 与
   `cmake/VerifyPlayerDistribution.cmake:35-45` 的 `cuexis_dist_run_checked` 同形；
   宿主 `examples/reference_host/src/main.cpp:32-45` 与 `:60-68` 的数字累加循环重复；
   门禁正/负例 configure 参数在 `VerifyReferenceHost.cmake:170-205` 与 `:361-376` 重复。

**修法**：每组抽出一个共享实现，其余处调用它。

### STD-06 possible Repeated Switches

- `engine/player_support/src/player_command.cpp:75-128`：对 Play/Pause/Stop/Seek/Reload 各重复一次
  相同的 `Empty→not_loaded`、`Failed→failed` 前置级联。→ 提到 switch 前做一次前置检查。
- `engine/cxc/src/cxc_candidate.cpp:182-194` / `:485-497`：同一字段数组两处。
- `engine/chart/src/packed_chart_tables.cpp:46-49`（section 注册表）与 `:1262-1263`（required section 清单）
  各自硬编码 section code；[PACKED_CHART_FORMAT.md](../../../formats/PACKED_CHART_FORMAT.md) §5.3 要求
  "同一注册表判定，不得各自维护清单"。二者语义（registered vs required）确实不同，故列为判断项。

### STD-07 possible Speculative Generality / 死代码

- `app/player/src/player_control.cpp:64-66` `NullJudgeSystem` 空实现，仅 :786/:926 调用，对应尚未存在的
  Stage 7A Judgement。
- `app/player/src/player_control.cpp:254` `timingOffsetMs()` 全仓无调用者。
- `examples/reference_host/src/host_report.cpp:45` `HostReport::steps()`、
  `host_content.cpp:90` `HostFileProvider::readCount()` 无调用者；
  `host_content.hpp:46` `HostContent::providerRootId{"main"}` 从未被读，`readBlob` 反而硬编码 `"main"`。
- `tools/check_version_gate.py:275-279` `_result_json` 把 `asdict(result)` 已含的字段重新赋同值。
- `engine/cxc/src/cxc_candidate.cpp:55` `readRequiredString` 的第三参数无名且从未使用，
  调用点传入的 `diagnostics` 被静默丢弃。
- `engine/chart/include/cuexis/chart/candidate_lowering.hpp:40-41` 的 `flags{1}`/`candidateRevision{1}`
  只写不读（全仓无消费点）。
- `tools/check_version_gate.py:292,331` 的 `--event` 只用于打印。

### STD-08 possible Middle Man

- `tools/cxc_common/src/cxc_candidate.cpp:7-9` 纯转发到 `cxc::validateCandidateChartExtension`。
  （这是 spec 要求的"生产层共享校验"入口，属可接受的薄适配；保留判断。）
- `engine/chart/src/packed_chart_io.cpp:220-234` / `:285-297` 基本只转发 `packed::encode/inspect/decode`；
  文件注释声明为有意的 file bridge。
- `engine/render_opengl/src/open_gl_presentation.cpp:1959-2001` 的 `toDrawCommand`/`toDrawSummary`
  纯字段转发，只为两套并行类型存在。

### STD-09 possible Primitive Obsession / Data Clumps

- `engine/cxc/include/cuexis/cxc/cxc_candidate.hpp:26-32` 的
  `sourceSemanticIdentity`/`compiledSemanticIdentity`/`artifactIdentity`/`compilerProfile`/`expanded*Count`
  恒定结伴出入，是一个想诞生的类型；其中 `:27-29` 用 `std::optional<std::string>` 承载 SHA-256 身份。
- `tools/media_import/include/cuexis/media_import/media_provenance.hpp:51-64` 的四类身份全用 `std::string`，
  而文档反复要求它们"must not conflate"；强类型可让混淆在编译期失败。
- `tools/asset_publish/include/.../asset_publish.hpp:84` 的 `void* handle_` 同时承载 `HANDLE` 与
  `reinterpret_cast<void*>((intptr_t)fd)`；[CODE_POLICY](../../../guides/CODE_POLICY.md) 要求公共 API
  能从类型或文档判断所有权，锁柄是独占拥有物却退化为 `void*`。
- `app/player` 控制层遍布裸 `double` 毫秒（`PlayerCommand::seekTargetMs`、`startPositionMs`、`timingOffsetMs_`）。
- `engine/audio_sdl/include/.../sdl_audio.hpp:24-34` 的 `PlaybackDeviceRecord` 与 `PlaybackDeviceTarget`
  字段完全相同。
- `tools/check_version_gate.py:97-104,232-240` 的 `(trusted_utc_date, context, allow_sdk_api_change)`
  三参在 `compare_refs`→`compare_snapshots` 结伴传递，`context` 是裸字符串 + 内联集合校验。

### STD-10 可移植性：`_wfopen_s` 仍按 `_WIN32` 选择

- **现象**：`tools/asset_publish/src/publish_fs_internal.cpp:198-199` 按 `_WIN32` 选 `_wfopen_s`，
  而同一文件 `:38-55` 的 `readEnvValue` 已在 `0753e6a` 改为按 `_MSC_VER` 选 `_dupenv_s`。
- **影响**：`_wfopen_s` 是 MSVC CRT 扩展；非 UCRT 的 MinGW 会复现 `0753e6a` 记录的失败模式——
  该类缺陷此前只修了一半。
- **处置建议**：两处统一按 `_MSC_VER` 分支（或用不依赖 CRT 扩展的读取路径）。

### STD-11 `.gitattributes` 未覆盖新 fixture 树

- **现象**：本 diff 只为 `vcpkg-overlays/**` 加了 `text eol=lf`；`tests/fixtures/stage6_e/**`
  （18 个 golden `.json` + 40 余个 `*.png/*.jpg/*.mp3/*.ogg/*.flac/*.wav` 与 `*.py/*.cpp`）与
  `tests/fixtures/stage6_a2/**` 没有任何规则。
- **证据**：`.gitattributes` 第 7 行是本次唯一新增；同仓库对 `tests/fixtures/chart_format_update/**`
  既有 `*.json text eol=lf` 与 `*.bin binary` 规则。
- **影响**：Windows `core.autocrlf=true` 的检出可能改写 golden 文本行尾。新 golden 以 JSON 解析方式
  被消费时风险低，但违反同仓库既有的一致做法。
- **处置建议**：按既有模式为两棵新 fixture 树补 `binary`/`eol=lf` 规则。

### STD-12 门禁脚本健壮性

- `cmake/VerifyReferenceHost.cmake:270` 设定 `ENV{PATH}`、`:328` 才恢复；中途 `FATAL_ERROR` 会跳过恢复
  （每个用例独立进程，影响有限，但模式本身脆弱）。
- `cmake/PackagePlayer.cmake:86-98` 在解析依赖之外 GLOB 复制目录内**全部** `*.dll`，范围过宽。
- `.github/workflows/version-gate.yml` 的 `run: |` 块内缩进不齐（54/63/66 行各多一空格）。

### STD-13 构建产物被跟踪（**不在本轮 diff 内**）

- **现象**：`hello.obj`（46,882 B）与 `dump_chart_writer.obj`（589,044 B）以 `100644` 被 git 跟踪；
  `.gitignore` 只有 `/.idea/`、`/.vs/`、`/.codex/`、`/cmake-build-*/`、`/out/`、`/vcpkg_installed/`
  等条目，没有 `*.obj`。
- **证据**：`git ls-files -s hello.obj dump_chart_writer.obj`；引入提交是 `f583429`（2026-09-02），
  **早于本轮 fixed point `13dab93`**，故 `git log 13dab93..eaaf375 -- hello.obj` 无输出。
  **父代理已核实**。
- **影响**：仓库卫生问题与可重复构建的噪音；二进制产物会随无意义的重新生成反复变更。
- **处置建议**：独立的小整改项——`git rm --cached` 两个文件并补 `.gitignore` 规则。
  因为不在本轮 diff 内，不作为本轮复核的发现统计。

---

## 3. 已确认未违反（避免误报）

- 安装公共头（`engine/*/include/cuexis/**` 中进入安装者）全 ASCII，0 处非 ASCII 或全角标点。
- `Result`/`core::Error` 未被静默忽略：`app/player`、`tests/player`、`engine/chart`、`engine/cxc`
  的调用点均已 `hasValue()`/`CHECK` 处理或显式转发。
- 异常在工厂边界 `catch` 并转稳定 `Error`，未跨模块公共边界；现实时路径未发现抛出。
- C3 拆分后 `app/player` 各文件职责单一（control/assembly/smoke/options/log/frame_diagnostics/snapshot_scene），
  `player_app.cpp` 已无帧循环正文，未见半迁移残留。
- 媒体依赖四处同步齐备：`vcpkg.json`、`docs/guides/DEPENDENCY_POLICY.md`、`THIRD_PARTY_NOTICES.md`、
  `vcpkg-configuration.json` 基线一致；`media-tools` 默认 OFF，`cuexis_playback`/`cuexis_player`
  未进入媒体解码链接闭包。
- 参考宿主只使用安装公共头与 `Cuexis::Playback`（`find_package(Cuexis COMPONENTS Playback)`），
  无源码树私有 include、不链接 Player 配置实现；`cmake/` 未新增安装项，未污染 consumer 导出。
- `vcpkg-overlays/libvorbis` 用 `REF v1.3.7` + `SHA512` 固定、五个 patch 齐全，未按平台拆分 profile。
