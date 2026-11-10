# Stage 6 复核：Standards 轴

状态：historical review snapshot
快照日期：2026-09-28
更新日期：2026-09-28

本文件只记录 **Standards 轴**：diff `13dab93...eaaf375` 是否违反仓库文档化的编码/文档标准，
以及 Fowler《重构》第 3 章 smell 基线。Spec 轴见 [Spec 记录](2026-09-28-spec.md)；
两轴不合并。范围、方法、门禁命令结果见[汇总](2026-09-28-summary.md)；
二轮范围判定与命令记录见[细化记录](2026-09-28-refinement.md)。

标准来源：`AGENTS.md`、[CODE_POLICY](../../../guides/CODE_POLICY.md)、
[DEPENDENCY_POLICY](../../../guides/DEPENDENCY_POLICY.md)、[VERSIONING](../../../guides/VERSIONING.md)、
[DOCUMENTATION_POLICY](../../../DOCUMENTATION_POLICY.md)、[BUILDING](../../../guides/BUILDING.md)、
[PROJECT_GUIDE](../../../PROJECT_GUIDE.md)、[API Reference](../../../api/README.md)、
[格式契约](../../../formats/README.md)。工具已强制的部分（架构测试、target 依赖 allowlist、
`cuexis_format_check`、`tools/check_docs.py`）不重复报。

**范围标注**（每条给出）：`本轮新增` = 该行由本区间的提交引入；`本轮修改文件中的既有行` =
文件在 diff 内被改、但该行更早；`本轮范围外` = 既非新增也非本轮文件。

---

## 1. 文档化契约违规（3 项，均为本轮新增）

### STD-01 候选预算诊断码未使用契约码

- **范围**：本轮新增。`engine/cxc/src/cxc_candidate.cpp:533` 由 `2eb90bd`（S6-C1）引入。
- **现象**：候选 Chart entry 的 16 MiB 上限在 `engine/cxc/src/cxc_candidate.cpp:532-534` 报出
  `cxc.budget.exceeded`；同一文件的 `:93` 定义了 `cxc.candidate.budget_exceeded`，并在 `:356`、`:360`
  的其它候选预算路径使用它。
- **证据**：[CHART_ENTRY_V1_FORMAT.md](../../../formats/CHART_ENTRY_V1_FORMAT.md) 的诊断表第 122 行规定
  `cxc.candidate.budget_exceeded` = "A file, section, decoded or count limit is exceeded"，
  第 118–126 行是候选 entry 的完整诊断集。
  **旁证**：[C1 退出报告](../../stages/stage-06/2026-09-22-s6-c1-exit.md) 第 58 行把
  「超过 16 MiB 的 candidate entry 失败」的验收证据直接写成「既有 R4 `cxc.budget.exceeded` 用例」——
  即报告与实现都与格式契约不一致，不是代码笔误。父代理已核实两者。
- **影响**：调用方与 golden 按契约码分支时会漏判该失败；同一语义在同一文件内有两种可序列化诊断码，
  违反 [CODE_POLICY](../../../guides/CODE_POLICY.md) 的稳定诊断约定。
- **处置建议**：把 `:533` 改用候选码（复用 `:93` 的常量），同步订正 C1 退出报告第 58 行的措辞，
  并在候选负例中固定断言该码。

### STD-02 报告声称交付的文档不存在

- **范围**：本轮新增（报告与所声称的交付物都属本轮）。
- **现象**：
  [2026-09-27-s6-c4-reference-host-and-player-distribution.md](../../stages/stage-06/2026-09-27-s6-c4-reference-host-and-player-distribution.md)
  §2 声称交付了 `examples/reference_host/README.md`。
- **证据**：`examples/reference_host/` 实际只有 `CMakeLists.txt` 与 `src/`（7 个源文件），无任何 `.md`；
  `docs/guides/BUILDING.md:468` 已把该 README 当作既有入口引用。父代理已核实。
- **影响**：违反 [DOCUMENTATION_POLICY](../../../DOCUMENTATION_POLICY.md)「Report：某个时间点的实施、审查和证据」与
  「不得把未发生写成已发生」；构建指南指向不存在的文件。
- **处置建议**：补写该 README（宿主主循环、命令循环、provider、构建/运行步骤），或修正 C4 报告与
  `BUILDING.md` 的表述，二者取其一，不要留悬空引用。

### STD-03 Stage 6 状态词与已归档事实冲突

- **范围**：本轮新增（关闭与归档动作在本轮内完成，`f145af9`、`5668b27`）。
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
- **证据**：`tools/check_docs.py` 通过（该类一致性不在其校验范围）；plan.md 的三处由父代理逐行核实
  （`git grep` 命中 `:51`、`:103`、`:106`）。
- **影响**：[DOCUMENTATION_POLICY](../../../DOCUMENTATION_POLICY.md) 的权威顺序与状态字段规则被破坏；
  同一文档内互斥的状态会让下游读者无法判断 Stage 6 是否已关闭，并会污染后续交接。
- **处置建议**：以 `CURRENT_STATUS.md` 为唯一当前摘要，把 plan/completion 的历史行改为指向关闭报告的
  一次性修正（不改写证据，只改状态词），并同步 `ROADMAP.md`、`VERSIONING.md`。

---

## 2. smell 基线（8 项，均为判断项）

### STD-05 possible Duplicated Code

同一种逻辑形状在本次改动的一个以上 hunk 或文件里出现，共 8 组。

**范围**：以下各簇的载体文件经 `git diff --name-status` 判定，除注明者外均为本轮新增（`A`）。
唯一混入既有代码的是第 1、2 组：adapter 侧的 `SummaryHash`/`summaryDigest` 早于 fixed point
（`b71ef23b`, 2026-08-09），本轮新增的是与之并行的中立层；重复关系由本次新增产生。

1. **中立层与 adapter 各一份 digest**：`engine/presentation_renderer/src/draw_command.cpp:58-242`
   （`SummaryHash`/`hashCommand`/`summaryDigest`/`Point3`/`transformPoint`/`finitePoint`/`finiteMatrix`）
   与 `engine/render_opengl/src/open_gl_presentation.cpp:810-921` 逐字重复，域串同为
   `cuexis.validation.summary.v1`。
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
   `tools/media_importer/src/publish.cpp:75`（`publishImmutable`）与 `:164`（`promoteTemporary`）
   也重复"存在→读/哈希→冲突"形状。
6. **配置层**：`engine/player_support/src/user_preferences.cpp:111-126` 的 `unsupportedVersion` 与
   `audio_device_profile.cpp:10-25` 的 `unsupportedProfileVersion` 形状相同（仅 format 常量不同，
   两处都由 `e01a4b1` 引入）；`user_preferences.cpp:320,331,341,349,355` 的临时文件清理块重复 5 次；
   `config_location.cpp:29-45` 复刻 `schemas/cuexis.player-preferences.v1.schema.json:27` 的 profile ID pattern；
   `engine/audio_sdl/src/sdl_audio.cpp:69-88` 的 `findUniqueDevice` 重写了
   `audio_device_profile.cpp:132-161` 的唯一匹配+歧义逻辑，并把 `player.audio_profile.*` 码硬编码进后端
   （两处均由 `e01a4b1` 引入）。
7. **cxc 候选字段表**：`engine/cxc/src/cxc_candidate.cpp:182-194` 与 `:485-497` 是逐字相同的
   11 元素 `constexpr std::array fields`；`engine/playback/src/playback_source.cpp:852-893` 与
   `:895-936`（`fromCxcFileEntry`/`fromCxcMemoryEntry`）约 30 行同构，仅 loader 与 tag 不同；
   `candidate_lowering.hpp:42` 的内联字面量 `"candidate.static-tap-lanes4-v1"` 重复了
   `cxc_candidate.cpp:27` 的命名常量。
8. **CMake 门禁脚本**：`cmake/VerifyReferenceHost.cmake:41-51` 的 `cuexis_host_run_checked` 与
   `cmake/VerifyPlayerDistribution.cmake:35-45` 的 `cuexis_dist_run_checked` 同形；
   `examples/reference_host/src/main.cpp:32-45` 与 `:60-68` 的数字累加循环重复；
   门禁正/负例 configure 参数在 `VerifyReferenceHost.cmake:170-205` 与 `:361-376` 重复。

**修法**：每组抽出一个共享实现，其余处调用它。

### STD-06 possible Repeated Switches

- `engine/player_support/src/player_command.cpp:75-128`：对 Play/Pause/Stop/Seek/Reload 各重复一次
  相同的 `Empty→not_loaded`、`Failed→failed` 前置级联（文件本轮新增）。→ 提到 switch 前做一次前置检查。
- `engine/cxc/src/cxc_candidate.cpp:182-194` / `:485-497`：同一字段数组两处（均本轮新增）。
- **范围（混合）**：`engine/chart/src/packed_chart_tables.cpp:46-49`（section 注册表）由 `bc3b5d48`（R2）
  本轮新增，`:1262-1263`（required section 清单）由 `9e4ce4da`（2026-09-07）早于 fixed point——
  重复关系是本轮新增注册表与既有清单并存造成的。格式文档
  [PACKED_CHART_FORMAT.md](../../../formats/PACKED_CHART_FORMAT.md) §5.3 要求"同一注册表判定，
  不得各自维护清单"；两者语义（registered vs required）确实不同，故仍列为判断项。

### STD-07 possible Speculative Generality / 死代码

**范围**：以下均为本轮新增。

- `app/player/src/player_control.cpp:64-66` `NullJudgeSystem` 空实现，仅 `:786`/`:926` 调用，
  对应尚未存在的 Stage 7A Judgement。
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

- **范围（混合）**：`tools/cxc_common/src/cxc_candidate.cpp:7-9` 纯转发到
  `cxc::validateCandidateChartExtension`——该文件在本轮被从 +240 行削减为 +2 行，转发形态是本轮产物；
  这是 spec 要求的"生产层共享校验"入口，属可接受的薄适配。
- **范围（既有行）**：`engine/chart/src/packed_chart_io.cpp:220-234` / `:285-297` 基本只转发
  `packed::encode/inspect/decode`（该两行由 `9e4ce4da`, 2026-09-07 引入；文件本轮被削 −482 行）；
  文件注释声明为有意的 file bridge。
- **范围（本轮新增）**：`engine/render_opengl/src/open_gl_presentation.cpp:1959-2001` 的
  `toDrawCommand`/`toDrawSummary` 纯字段转发，只为两套并行类型存在。

### STD-09 possible Primitive Obsession / Data Clumps

**范围**：以下均为本轮新增。

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
  字段完全相同（`e01a4b1` 引入）。
- `tools/check_version_gate.py:97-104,232-240` 的 `(trusted_utc_date, context, allow_sdk_api_change)`
  三参在 `compare_refs`→`compare_snapshots` 结伴传递，`context` 是裸字符串 + 内联集合校验。

### STD-10 可移植性：`_wfopen_s` 仍按 `_WIN32` 选择

- **范围**：本轮新增（`tools/asset_publish/src/publish_fs_internal.cpp` 为本轮新增文件；
  修复提交 `0753e6a` 也在本轮内）。
- **现象**：`:198-199` 按 `_WIN32` 选 `_wfopen_s`，而同一文件 `:38-55` 的 `readEnvValue`
  已在 `0753e6a` 改为按 `_MSC_VER` 选 `_dupenv_s`。
- **影响**：`_wfopen_s` 是 MSVC CRT 扩展；非 UCRT 的 MinGW 会复现 `0753e6a` 记录的失败模式——
  该类缺陷此前只修了一半。
- **处置建议**：两处统一按 `_MSC_VER` 分支（或用不依赖 CRT 扩展的读取路径）。

### STD-11 `.gitattributes` 未覆盖新 fixture 树

- **范围**：本轮新增（`.gitattributes` 第 7 行 `vcpkg-overlays/** text eol=lf` 是本轮唯一新增）。
- **现象**：`tests/fixtures/stage6_e/**`（18 个 golden `.json` + 40 余个
  `*.png/*.jpg/*.mp3/*.ogg/*.flac/*.wav` 与 `*.py/*.cpp/*.md`）与 `tests/fixtures/stage6_a2/**`
  没有任何规则；同仓库对 `tests/fixtures/chart_format_update/**` 既有 `*.json text eol=lf`
  与 `*.bin binary` 规则。
- **影响（已量化，故降为一致性项）**：二进制 fixture 含 NUL，git 会自动按二进制处理，不受
  `core.autocrlf` 影响；golden 的消费方式是**子串查找**而非整文件字节比较
  （`tests/media_import/media_import_tests.cpp:67-89` 的 `golden`/`jsonString`、
  `:192-200` 的 `requireCanonical` 只比对 `sha256`/`byteCount`/`bytesHex` 的值），
  行尾被改写不会使检查失败。因此这是与仓库既有做法不一致的**卫生缺口**，不是复现性风险。
  **父代理已核实这两点。**
- **处置建议**：按既有模式为两棵新 fixture 树补 `binary`/`eol=lf` 规则。

### STD-12 门禁脚本健壮性

**范围**：本轮新增（`cmake/VerifyReferenceHost.cmake`、`cmake/PackagePlayer.cmake`、
`.github/workflows/version-gate.yml` 均为本轮新增）。

- `cmake/VerifyReferenceHost.cmake:268-270` 保存并置空 `PATH`/`LD_LIBRARY_PATH`，
  `:326-329` 才恢复；两者之间的 `FATAL_ERROR`（`:283`、`:289`、`:308`、`:313`、`:317`、`:322`）
  会跳过恢复。因为每个门禁以独立 CMake 进程运行，影响限于该进程，故只作提示。
- `cmake/PackagePlayer.cmake:86-98` 在解析依赖之外 GLOB 复制目录内**全部** `*.dll`
  （含 `cuexis_architecture_tests.dll` 的显式排除与去重，说明作者对该宽度是有意识的）。
- `.github/workflows/version-gate.yml` 的 `run: |` 块内缩进不齐（54/63/66 行各多一空格）。

---

## 3. 本轮范围外的既有项（2 项，不计入本轴发现数）

### STD-04 已记录的既有整改未落实（AP-09）

- **范围**：**本轮范围外**。`app/player/src/player_app.cpp:27` 与
  `app/player/src/frame_diagnostics.cpp:19` 都由 `907dcbeb`（2026-07-27）引入，早于 fixed point；
  `app/player/src/frame_diagnostics.cpp` **完全不在本轮 diff**（`git diff --name-status -- app/player`
  只有 `player_app.cpp`/`player_app.hpp` 为 M，其余 11 个文件为 A）。**父代理已核实这两点。**
- **现象**：`player_app.cpp:27-43` 的 `audioStateName` 与 `frame_diagnostics.cpp:19-35` 的 `stateName`
  是逐字相同的 `audio::PlaybackState` 级联。
- **证据**：[full-review-2026-08 复核记录](../full-review-2026-08/2026-08-29-review.md) 的 AP-09 与
  [整改计划](../../../stage_plans/reviews/full-review-2026-08/remediation-plan.md) 第 1240 行要求
  「AP-09 合并 `stateName`/`audioStateName`」。
- **影响**：本轮把 `player_app.cpp` 削去 928 行（+126/−928）时没有顺带落实该已排期整改，
  技术债被保留。因为重复不由本轮引入，本轴不计为范围内发现。
- **处置建议**：并入后续整改清单，与 STD-13 一起处理。

### STD-13 构建产物被跟踪

- **范围**：**本轮范围外**。引入提交 `f583429`（2026-09-02）早于 fixed point `13dab93`；
  `git log 13dab93..eaaf375 -- hello.obj dump_chart_writer.obj` 无输出。
- **现象**：`hello.obj`（46,882 B）与 `dump_chart_writer.obj`（589,044 B）以 `100644` 被 git 跟踪；
  `.gitignore` 只有 `/.idea/`、`/.vs/`、`/.codex/`、`/cmake-build-*/`、`/out/`、`/vcpkg_installed/`
  等条目，没有 `*.obj`。**父代理已核实**。
- **影响**：仓库卫生问题与可重复构建的噪音。
- **处置建议**：独立小整改——`git rm --cached` 两个文件并补 `.gitignore` 规则。

---

## 4. 已确认未违反（避免误报）

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

## 5. 二轮细化改变的三处判断

| 项 | 一轮 | 二轮 | 依据 |
| --- | --- | --- | --- |
| STD-04 | 范围内"已记录整改未落实"，中 | 移入 §3 **本轮范围外**，低 | `git blame`：两行均由 `907dcbeb`(2026-07-27) 引入；`frame_diagnostics.cpp` 不在 diff |
| STD-11 | 可能影响 golden 复现性 | 降为**一致性卫生项** | golden 以子串查找消费（`media_import_tests.cpp:67-89`、`:192-200`）；二进制 fixture 由 git 自动判定 |
| STD-12 | 门禁脚本缺陷 | 保留但注明影响限于单个 CMake 进程，GLOB 有意识 | `VerifyReferenceHost.cmake:268-329`；`PackagePlayer.cmake:86-98` |
