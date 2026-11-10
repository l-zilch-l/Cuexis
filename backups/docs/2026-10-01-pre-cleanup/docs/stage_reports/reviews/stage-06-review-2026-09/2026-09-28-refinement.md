# Stage 6 复核：二轮细化（范围判定与实测）

状态：active
快照日期：2026-09-28
更新日期：2026-09-28

本文件是[汇总](2026-09-28-summary.md)之后的第二轮工作记录。一轮由 20 个只读子代理给出，
其中 10 个子代理（S2-Spec、S3 两轴、S6 两轴、S8 两轴、S9 两轴、S10-Spec）所在环境**没有 shell**，
只能直读工作树，因此**无法区分某行是本次新增还是早已存在**，也无法执行只读自测。
本文件补齐这一层：用 `git diff --name-status` + `git blame` 逐条判定范围，
实际运行两个只读检查脚本，并据此修正 11 条判断（含撤回 1 条）。

两轴的结论已按本文件更新：[Standards 记录](2026-09-28-standards.md)、[Spec 记录](2026-09-28-spec.md)。

## 1. 改动分类

`git diff --name-status 13dab93...eaaf375` 的结果：

| 分类 | 数量 |
| --- | ---: |
| A（新增） | 230 |
| M（修改） | 80 |
| D（删除） | 1 |
| R093（重命名） | 1 |

删除与重命名都属阶段生命周期动作，无孤儿：

- `D docs/stage_plans/future/stage-06/plan.md`（配 `A docs/stage_plans/completed/stage-06/plan.md`）
- `R093 docs/stage_plans/active/chart-format-foundation/plan.md → docs/stage_plans/completed/chart-format-foundation/plan.md`

**父代理已核实**：`docs/stage_plans/legacy-paths.md` 在本轮被同步修改，
第 29-33 行把上述四个旧逻辑路径都映射到 canonical 文档，
符合 [DOCUMENTATION_POLICY](../../../DOCUMENTATION_POLICY.md) 对批量重组的要求——这是一条正面结论。

与本轮发现相关的被**修改（M）**文件（其余为此前未见问题的列表，略）：

```
app/player/src/player_app.cpp, player_app.hpp
engine/chart/src/packed_chart_tables.cpp, packed_chart_io.cpp
engine/chart/include/cuexis/chart/packed_chart_tables.hpp
engine/playback/src/playback_session.cpp, playback_source.cpp
engine/render_opengl/src/open_gl_presentation.cpp, open_gl_backend.cpp
engine/audio_sdl/src/sdl_audio.cpp, include/.../sdl_audio.hpp
tools/cxc_common/src/cxc_candidate.cpp
```

其余与发现相关的载体文件（`engine/cxc/src/cxc_candidate.cpp`、`engine/chart/src/packed_profile.cpp`、
`packed_semantic_identity.cpp`、`candidate_lowering.cpp(+hpp)`、`engine/player_support/**`、
`engine/presentation_renderer/**`、`tools/check_version_gate*.py`、`tools/check_stage6_a2.py`、
`tools/asset_publish/**`、`tools/media_import/**`、`tools/media_importer/**`、
`cmake/PackagePlayer.cmake`、`cmake/VerifyReferenceHost.cmake`、`cmake/VerifyPlayerDistribution.cmake`、
`examples/reference_host/src/**`、`app/player/src/player_control.cpp` 等 11 个、
`.github/workflows/version-gate.yml`）**全部为 A**，即由本轮引入。

## 2. 逐行范围判定（`git blame -L <line>,<line> -- <file>`）

| 文件:行 | 引入提交 | 日期 | 判定 |
| --- | --- | --- | --- |
| `engine/cxc/src/cxc_candidate.cpp:533` | `2eb90bd` | 2026-09-22 | 本轮新增（STD-01） |
| `engine/chart/src/packed_chart_tables.cpp:35` | `4ebf244` | 2026-09-17 | 本轮新增（SPEC-02） |
| `engine/chart/src/packed_chart_tables.cpp:46` | `bc3b5d4` | 2026-09-17 | 本轮新增（STD-06 注册表） |
| `engine/chart/src/packed_chart_tables.cpp:1262` | `9e4ce4d` | 2026-09-07 | **早于 fixed point**（required 清单） |
| `engine/chart/include/.../packed_chart_tables.hpp:34` | `2603865` | 2026-09-17 | 本轮新增 |
| `engine/playback/src/playback_session.cpp:681` | `2eb90bd` | 2026-09-22 | 本轮新增（SPEC-12） |
| `engine/chart/src/candidate_lowering.cpp:133` | `2eb90bd` | 2026-09-22 | 本轮新增（SPEC-01） |
| `engine/chart/src/candidate_lowering.cpp:277` | `2eb90bd` | 2026-09-22 | 本轮新增（SPEC-12） |
| `engine/render_opengl/src/open_gl_presentation.cpp:810` | `b71ef23` | 2026-08-09 | **早于 fixed point**（第二套 digest） |
| `engine/render_opengl/src/open_gl_presentation.cpp:896` | `b71ef23` | 2026-08-09 | **早于 fixed point** |
| `engine/render_opengl/src/open_gl_presentation.cpp:1854` | `01e6976` | 2026-08-30 | **早于 fixed point**（`buildDraws`） |
| `engine/audio_sdl/src/sdl_audio.cpp:69` | `e01a4b1` | 2026-09-23 | 本轮新增（STD-05/09） |
| `engine/audio_sdl/src/sdl_audio.cpp:540` | `e01a4b1` | 2026-09-23 | 本轮新增 |
| `engine/audio_sdl/include/.../sdl_audio.hpp:24` | `e01a4b1` | 2026-09-23 | 本轮新增 |
| `engine/chart/src/packed_chart_io.cpp:220` | `9e4ce4d` | 2026-09-07 | **早于 fixed point**（STD-08 转发） |
| `app/player/src/player_app.cpp:27` | `907dcbe` | 2026-07-27 | **早于 fixed point**（STD-04） |
| `app/player/src/player_app.cpp:43` | `907dcbe` | 2026-07-27 | **早于 fixed point** |
| `app/player/src/frame_diagnostics.cpp:19` | `907dcbe` | 2026-07-27 | **早于 fixed point**，且该文件不在本轮 diff |

## 3. 实际执行的命令与结果

| 命令 | 结果 |
| --- | --- |
| `python -B tools/check_version_gate_tests.py` | `Ran 11 tests in 0.562s` / `OK`（exit 0） |
| `python -B tools/check_stage6_a2.py` | `S6-A2 characterization passed: schemas, entry fixtures, identity/media goldens and boundaries`（exit 0） |
| `git diff --name-status -- app/player` | 只有 `player_app.cpp`/`player_app.hpp` 为 M，其余 11 个为 A；`frame_diagnostics.cpp` 不出现 |
| `git grep -n "buildPresentationCommands" -- "*.cpp" "*.hpp"` | `draw_command.hpp:58`、`draw_command.cpp:268`、`presentation_renderer.cpp:395`、`presentation_renderer_tests.cpp:103/105/121`——**OpenGL adapter 零命中**（SPEC-13） |
| `git grep -n "failClipPreparation" -- tests/player` | 只有 `:262`（分支）与 `:353`（定义），**没有置 true 的调用点**（SPEC-19a） |
| `git grep -n "CanonicalFeature\|resourceClosure\|requiredFeatures" -- engine` | `packed_chart_tables.cpp:1400`（feature 读入）、`:2027`（decode 侧派生闭包）、`candidate_lowering.cpp:307`（复制已派生闭包）、`packed_semantic_identity.cpp:372/471` |
| `git grep -n "appliedVsync\|appliedFullscreen\|appliedGain" -- app engine` | `player_assembly.cpp:312-314`、`resolved_config.hpp:42-44`；调用点为 `player_app.cpp:153`（传 `requested.vsync`） |
| `git grep -n "unsupported_version" -- tests` | `player_support_tests.cpp:112`、`:116`（配置版本边界有测试） |
| `git grep -n "手动" -- docs/stage_reports/stages/stage-06` | `2026-09-27-s6-c4-reference-host-and-player-distribution.md:114`、`:117` |
| `Get-Content docs/stage_reports/stages/stage-06/2026-09-22-s6-c1-exit.md -Skip 52` | `:58` 把 candidate 预算证据写成「既有 R4 `cxc.budget.exceeded` 用例」（STD-01 旁证）；`:61-63` 自认零限制/stale/`resource_conflict` 无新用例 |
| `git grep -n "dumpbin\|forbidden_symbols" -- cmake/*.cmake` | `VerifySharedExports.cmake`、`VerifySharedConsumerImports.cmake` 已有符号/导入表门禁机制（未接到宿主，SPEC-27） |
| 读 `completion.md:125-139` | §8 残余清单的实际条目（用于判定"已声明残余"） |

## 4. 判断变化汇总

| 项 | 一轮（子代理） | 二轮（父代理核实后） | 依据 |
| --- | --- | --- | --- |
| SPEC-09 | 中：A16 缺生产入口 | **撤回** | `packed_chart_tables.cpp:2009-2027` 已在 decode 侧派生闭包并在 `:2031` 校验；`candidate_lowering.cpp:307` 复制的是已派生闭包 |
| SPEC-13 | 高 · 未声明 | 中 · 已声明残余 | §8:134 列为"残余（有意保留）"且 owner 2026-09-27 接受；未迁移的 adapter 行早于本区间 |
| SPEC-14 | 中 · 未声明 | 低 · 未声明 | 计划只要求"定义"线程/寿命语义（注释已定义），验收段要求的 6 类行为已测试 |
| SPEC-10 | 中：报告与代码码不符 | 低 · 报告已自述 | 报告已写"没有单独的新用例记录"，只是码名用错 |
| SPEC-15 | 中：applied* 全取自 requested | 中：7 个字段中 3 个为 requested 镜像 | `player_assembly.cpp:310-316`；调用点 `player_app.cpp:153` |
| SPEC-25 | 中 · 未声明 | 低 · 批次报告自认（未进 §8） | C4 退出 §7:259 |
| SPEC-27 | 中：三项均非门禁 | 低 · 批次报告自认（未进 §8） | 符号机制已存在未接线；minor 由 `find_package`+`SameMinorVersion` 承担一半；工具链 static 有如实说明；配置迁移有测试边界 |
| SPEC-28 | 中 · 未声明 | 低 · 报告已自述 | 实现报告 §3.2 给出候选隔离事实与产物扫描证据 |
| SPEC-31 | 中：两项不一致 | 低 · 部分已声明 | C2 hosted 项已由 §8:132 处置，只剩 preset 计数 |
| SPEC-32 | — | **新增为中** | §8:135 未承接 5 条批次自认项（C4 符号检查、插桩约定、E3 磁盘满/只读介质、配置迁移、SDK minor 手动） |
| STD-04 | 中 · 范围内"已记录整改未落实" | 低 · **本轮范围外** | `frame_diagnostics.cpp` 不在 diff；两行均由 `907dcbe`(2026-07-27) 引入 |
| STD-11 | 可能影响 golden 复现性 | 一致性卫生项 | golden 以子串查找消费（`media_import_tests.cpp:67-89`、`:192-200`）；二进制 fixture 由 git 自动判定 |
| STD-12 | 门禁脚本缺陷 | 保留，注明影响限于单进程；GLOB 有意识 | `VerifyReferenceHost.cmake:268-329`；`PackagePlayer.cmake:86-98` |

撤回与降级的共同原因：一轮把**"计划要求 X 未达成"**与**"X 已在关闭报告 §8 声明并经 owner 接受"**
混在一起统计。二轮把两者分开后，"真正未声明的缺口"减少，最严重的两项（SPEC-20、SPEC-23）
恰好都属于未声明一类。

## 5. 仍未验证的部分

- 除上表与两轴记录中标注"父代理已核实"的条目外，其余条目保留子代理给出的 `文件:行` 证据，未逐条复跑。
- **未复跑 hosted run**（未连网执行 `gh`）：`S6-G13` 之外的 run id 只做报告间文本交叉核对，
  因此 SPEC-26 的结论是"报告之间不一致"，不是"某次 run 实际失败"。
- 未运行 C++ 构建与 CTest：涉及运行期行为的判断（SPEC-18 的自动重试路径、SPEC-23 的崩溃窗口、
  SPEC-05 的并发竞争）都是基于源码路径的静态推理，未做故障注入或并发复现。
- 未核对媒体 golden 的**跨平台字节一致**（需要三平台实跑）：SPEC-22 的 interlaced 与
  SPEC-20 的伪造时长都只判定"负例缺失/被写成正例"，不判定 golden 数值正确性。
