# 四项 Stage 6 交接收口台账

状态：candidate；S7A-0 准入产出，待 owner acceptance

更新日期：2026-10-02

上级文档：[Gameplay V2 acceptance package](README.md) ·
[Stage 7A 实施计划](../../stage_plans/active/stage-07/plan.md) 的 §1.2

文档角色：交接台账。它记录四项 Stage 6 遗留内容的**当前**实现状态、缺口、阻塞与最小关闭证据。
它不重开 Stage 6，也不把历史报告当作当前证据。

## 0. 基线事实

| 事实 | 值 | 证据 |
| --- | --- | --- |
| 分支 / HEAD | `stage-7` / `449e864` | `git rev-parse` |
| `master` | `5472c46`（Merge PR #31） | `git rev-parse master` |
| 关系 | `master` 是 HEAD 的祖先（HEAD 已包含 PR #31） | `git merge-base --is-ancestor master HEAD` = 真 |
| 显示版本 | `26.09.29-1` | `python -B tools/update_version.py --check` |
| SDK API | `0.7.0` | `cmake/CuexisVersion.cmake` |
| 四项归属 | Stage 7A 承接，作为其关闭前置条件 | `docs/CURRENT_STATUS.md` §"Stage 6 未完成项的归属变更" |
| 四项在计划中的位置 | **§1.2（第 95-107 行）**，不在 §2 或 §6 | 实测 |

> **文档缺陷 D-1**：`docs/CURRENT_STATUS.md` 记载四项"登记在 Stage 7 计划的 §2 与 §6"。
> 实测 §2 是"总体依赖和发布路线"、§6 是"Stage 7B+ 分批规划"，两处均无四项表；实际位置是 §1.2。
> 该缺陷不改变归属，但会让读者按错误的小节去核对，应在接受本 package 时一并订正。

## 1. 显式 candidate 与实验隔离

### 1.1 已实现

| 内容 | 证据 |
| --- | --- |
| 构建开关 `CUEXIS_ENABLE_CHART_V5_CANDIDATE`，默认 OFF | 根 `CMakeLists.txt:42-43` |
| 开关只给 `cuexis_playback` 加 PRIVATE 编译定义（不改公共头与布局） | `engine/playback/CMakeLists.txt:28-30` |
| 测试侧同名定义 | `tests/playback/CMakeLists.txt:44-47` |
| 三个显式命名公开工厂，声明在安装 `FILE_SET HEADERS` 内 | `engine/playback/include/cuexis/playback/playback_source.hpp:83-91`；`engine/playback/CMakeLists.txt:35-47` |
| OFF 时工厂稳定拒绝 | `engine/playback/src/playback_source.cpp` 的三处 `#if !defined(...)` 分支 |
| entry 表 Schema | `schemas/cuexis.chart-entry.v1.schema.json` |
| 安装树 candidate 隔离扫描（含 `Cuexis_ALLOW_EXPERIMENTAL` token） | `cmake/VerifyReferenceHost.cmake:394-399` |
| 默认 OFF 下的工厂禁用断言（常驻用例） | `tests/playback/playback_candidate_tests.cpp:147-167` |

### 1.2 未实现 / 未接线（实测）

| 缺口 | 实测证据 |
| --- | --- |
| `Cuexis_ALLOW_EXPERIMENTAL` 无实现 | 非文档命中只有两处**禁止 token 表**：`cmake/VerifyReferenceHost.cmake:396`、`tools/check_stage6_a2.py:259` |
| candidate flavor 库名 / 安装元数据不存在 | 导出名仍是单一 `Playback`（`engine/playback/CMakeLists.txt:32`） |
| Player `--candidate-entry` 不存在 | `git grep candidate-entry -- app` = 0 命中；Player 只有 `--chart`、`--project` |
| **没有任何 preset 或 CI 开启 candidate** | `CMakePresets.json` 与 `.github/workflows/*.yml` 对 `CUEXIS_ENABLE_CHART_V5_CANDIDATE` 零命中（`version-gate.yml` 中的 "candidate" 是版本门禁的候选 SHA，与构建开关无关） |

### 1.3 已知阻塞

- **SPEC-19b 登记 BLOCKED**：开关在所有 preset 与 CI 中为 OFF，因此对应门禁**无法注册**，
  属构建隔离证据缺口；恢复条件是"在至少一个预设中开启该选项并注册门禁"。
- 证据强度限制：真实安装树中是否存在 candidate flavor 与 experimental 元数据，
  历史报告自述为**由 CMake 源码推断、非实测**。

### 1.4 现有门禁入口

- CTest `cuexis_reference_host_staging`（根 `CMakeLists.txt:412-439`）。
- CTest `cuexis_contract_s6_a2`（`tests/contracts/CMakeLists.txt:30-39`）。
- 用例 `Candidate factories stay disabled and do not read their inputs`
  （`tests/playback/playback_candidate_tests.cpp:147`）。
- candidate-only 用例位于 `#ifdef CUEXIS_ENABLE_CHART_V5_CANDIDATE` 块内
  （同文件第 169 行起）。

### 1.5 最小关闭证据（引用计划与 ADR）

- 计划 §1.2 第 1 行：默认 OFF 与显式 ON 的**构建/安装/运行矩阵**；production consumer 证明不会
  误启用 candidate；错误入口稳定拒绝。
- 计划 §S7A-8 工作内容 1 与 S7A-8.1：实现 candidate flavor、显式 entry、Player `--candidate-entry`、
  项目/CXC metadata 选择与默认 OFF 保护；**至少一个 preset 或 CI 真正开启并消费**。
- ADR 0042 S6-D01 冻结项：ON 构建与安装元数据须标记 experimental；生产与实验前缀禁混用；
  `CuexisConfig.cmake` 要求 consumer 显式 `Cuexis_ALLOW_EXPERIMENTAL=ON`；binary/import library
  名称区分 flavor；包元数据与 Player 启动诊断显示 flavor；clean staging 不得交叉误加载。

## 2. 离线 typed assembler 与 feature 派生

### 2.1 已实现

| 内容 | 证据 |
| --- | --- |
| 身份装配（**仅身份**）：`assembleCandidatePreparedSemanticIdentity` | `engine/chart/include/cuexis/chart/candidate_lowering.hpp:72`、实现 `src/candidate_lowering.cpp:330` |
| `assemblePreparedSemanticIdentity` | `engine/chart/include/cuexis/chart/prepared_semantic_identity.hpp:21` |
| resource closure **在 decode 期派生** | `engine/chart/src/packed_chart_tables.cpp:2012-2030` |
| 声明 closure 必须等于派生 closure | `engine/chart/src/packed_semantic_identity.cpp:449-476`（`packed.identity.closure`） |
| feature 注册表与 profile 校验 | `engine/chart/src/packed_profile_internal.hpp:20-21`、`packed_profile.cpp:31-51` |
| feature 参与 semantic identity（排序 + 重复拒绝） | `engine/chart/src/packed_semantic_identity.cpp:372-388` |

### 2.2 未实现 / 未接线（实测）

| 缺口 | 实测证据 |
| --- | --- |
| **不存在派生 feature 的 typed assembler** | `git grep assembl -- tools` = 0；`engine/` 内只有身份装配函数与注释 |
| **features 由调用方写入**，测试直接 `chart.features.push_back(...)` | `git grep -l features.push_back -- tests` 命中 **15 个文件** |
| `tools/` 无 chart-candidate 工具入口、无 closure report 产物 | 同第一行 |

### 2.3 已知阻塞

- ADR 0042 明文禁止当前做法："assembler 的正例必须使用实际离线工具入口验证，**不能仅在测试中
  注入 feature**"。
- 计划侧同禁令：§1.2 第 2 行"禁止测试注入 feature 作为生产后门"；§S7A-8 工作内容 2 要求
  "移除测试注入 feature 的生产后门"。
- 该缺口**未登记为 BLOCKED**（不同于第 1 项的 SPEC-19b），只以"部分实现 / 关键缺口"记录。

### 2.4 现有门禁入口

- `cuexis_chart_tests`（含 `candidate_lowering_tests.cpp`）、`cuexis_playback_tests`。
- 既有 candidate lowering 用例：显式 identity/requirements/opacity、Stage 6 golden 执行 ID、
  重复 identity/缺父节点/环拒绝、父节点重映射、prepared identity golden。
- **没有**独立的离线 assembler / chart-candidate 门禁入口。

### 2.5 最小关闭证据

- 计划 §1.2 第 2 行：正例、缺字段、非法引用、预算、确定性 identity、原子失败用例；
  Playback 不展开 CXT。
- 计划 S7A-8.2：chart-candidate tool 与 closure report；feature/resource closure 由 typed assembler
  派生；测试注入后门被检测；输入到 prepared identity 有完整链路。
- ADR 0042 S6-D03：`cuexis_chart` 增加 candidate lowering；typed assembler 接收显式 chart metadata、
  静态实体与展开结果，验证身份/父图并派生 feature 与 resource closure；lanes4 由被验证要求确定、
  不由 Player 补写；离线工具显式调用 assembler → encode → package。

## 3. 具名宿主命令循环

### 3.1 已实现（当前工作树与 `master` 均已有 R9 实现）

| 内容 | 证据 |
| --- | --- |
| Reference Host 位置 | `examples/reference_host/`（`src/` 六个 `.cpp/.hpp` + `tests/commands/` 命令夹具） |
| 命令文件入口 `--command-file` | `examples/reference_host/src/main.cpp:181-190`，冲突规则 `:198-212`，执行分支 `:277-292` |
| 动词枚举 | `examples/reference_host/src/host_commands.hpp:49`：`Open, Play, Pause, Tick, Seek, Reload, Quit` |
| 命令夹具规模 | 实测 37 个 `.cmd`、40 个 `.expect` |
| 稳定诊断码与上限 | `host_commands.hpp:22-47`（1 MiB 命令文件 / 10,000 命令 / 100,000 tick） |
| 合并事实 | `master` = `5472c46`（Merge PR #31）；HEAD 已包含它 |
| owner 处置 | `docs/CURRENT_STATUS.md`：R9 最终 tip 七个 hosted run 通过；owner 于 2026-09-29 接受 R9 退出 |

### 3.2 命名分歧（必须先消除，否则无法判定"六动词"是否已交接）

| 出处 | 所列动词 |
| --- | --- |
| ADR 0042（S6-D08 具名宿主段） | `open` / `play` / `pause` / `seek` / `reload` / `quit` |
| Stage 7A 计划 §1.2、§S7A-8、§S7A-8.3 | `load` / `play` / `pause` / `stop` / `seek` / `reload` |
| 实测实现（R9） | `Open` / `Play` / `Pause` / `Tick` / `Seek` / `Reload` / `Quit` |

- 实现中**不存在 `load` 与 `stop` 动词**；`load` 的语义由 `open` 承担，暂停语义由 `pause` 承担，
  退出由 `quit` 承担，另有计划未提及的 `tick`。
- 因此"六动词命令循环"这一措辞在计划里指的是一个**与实现不同名的集合**。

> **文档缺陷 D-2**：计划的三处"六动词"命名（`load/.../stop/...`）与 ADR 0042、与实现三方不一致。
> 要么承认 `open/tick/quit` 是 `load/stop/quit` 的等价实现并在计划中改词，要么登记一个改名任务。
> 这是 §6 的待决策项 3。

### 3.3 与旧报告的边界

- 交付报告在 Stage 6 关闭基线（`670cca8`）上的结论是"六动词命令循环不存在"；
  该结论已被 PR #31 之后的仓库状态取代。S7A-8.3 明确要求**不复制历史报告代替当前运行**。

### 3.4 现有门禁入口

- CTest `cuexis_reference_host_staging`（根 `CMakeLists.txt:412-439`）。
- CTest `cuexis_reference_host_command_parser`（根 `CMakeLists.txt:447-455`；可独立执行为
  `cmake -P cmake/VerifyReferenceHostCommandParser.cmake`）。
- 命令夹具目录 `examples/reference_host/tests/commands/`（c01–c11b 正例、n01a–n07b 负例）。
- hosted 入口：`Linux Quality`、`Windows MSVC`、`Windows MinGW`、`Version Gate`。

### 3.5 最小关闭证据

- 计划 §1.2 第 3 行：**同一 SHA 的 hosted 回归、命令状态矩阵、错误/旧状态保持证据**；
  且交付表述为"对现有 Reference Host 的交接回归，**不重新实现 R9**"。
- 计划 S7A-8.3：`load/play/pause/stop/seek/reload` 状态矩阵、失败保持旧 active、discontinuity
  与重载证据完整。

## 4. SDK API `0.7.1`

### 4.1 已实现

| 内容 | 证据 |
| --- | --- |
| SDK API 变更须显式放行 | `tools/check_version_gate.py:213-218`（`version.sdk_api.changed`） |
| 放行参数与传递链 | 默认 `False`（`:126-133`）、`compare_refs`（`:291-321`）、CLI `--allow-sdk-api-change`（`:361`）、`main`（`:387-394`） |
| 结果字段 `sdk_api_change_explicitly_allowed` | `:71-81`、`:229` |
| 条件化放行的正反例 | `tools/check_version_gate_tests.py:222-239` |
| 安装包兼容规则 `SameMinorVersion` | 根 `CMakeLists.txt:643-647` |
| 版本规范记录 | `docs/guides/VERSIONING.md`（`0.7.1` 为 additive 目标） |

### 4.2 未实现 / 未接线（实测）

| 缺口 | 实测证据 |
| --- | --- |
| `--allow-sdk-api-change` **未被任何工作流传入** | `.github/workflows/version-gate.yml` 三处调用均不含该参数 |
| `0.7.1` 未落地 | `cmake/CuexisVersion.cmake` 仍为 `0.7.0`；`docs/api/README.md` 亦为 `0.7.0` |
| 仓库**无 CODEOWNERS** | `glob **/CODEOWNERS` 无结果 |

### 4.3 已知阻塞

- **防篡改设计导致候选分支无法自行开启放行开关**：`version-gate.yml:58-71` 把 6 个受信任文件
  （含 `tools/check_version_gate.py` 与 `version-gate.yml` 本身）**从 `CUEXIS_BASE_SHA` 取出**再运行，
  因此候选树无法修改 checker 或传入放行参数。
- ADR 0042 要求：**修改门禁本身需要代码所有者复核与独立负例**；正式生效后不允许候选自行替换
  checker 获得通过；未配置保护规则时只能报告"脚本完成、门禁未启用"。仓库无 CODEOWNERS 使该复核
  无处可做。
- **SPEC-04 登记 BLOCKED**：门禁自身修改保护需跨阶段编号与 protection 规则裁定。
  恢复条件：配置 `CODEOWNERS` + 对 checker 与 workflow 启用 PR required review，
  再追加一条"门禁被修改时若无 owner 复核即失败"的负例。
- **阻塞性质**：版本变更本身在契约上安全（`SameMinorVersion`）；阻塞全部来自**放行通路未接线**，
  不是兼容性问题。无条件传入放行开关会**永久**放开 SDK API 变更保护。
- owner 裁定：2026-09-29 决定本批次不升版本，`0.7.1` 连同实测阻塞归入 Stage 7A 关闭前置。

### 4.4 现有门禁入口

- CTest `cuexis_contract_version_gate`（`tests/contracts/CMakeLists.txt:16-26`）。
- `python -B tools/check_version_gate_tests.py`（交付报告记为 19 项通过）。
- **S7A-0 基线实测**：该 CTest 项在默认 `PATH` 下**失败 1/19**，原因是 `bash` 解析到 WSL 启动器
  （`C:\Windows\system32\bash.exe`）而 `usable_posix_shell()` 只按文件名含 `wsl` 排除 shim；
  把 Git Bash 置于 `PATH` 首位后 19/19 通过。证据与复现见
  [S7A-0 基线报告](../../stage_reports/stages/stage-07/2026-10-02-s7a-0-baseline.md) §4.2，缺陷编号 D-9。
  本项**不阻塞 S7A-0**，但 S7A-8 的同 SHA 回归必须先消除该环境噪声。
- Version Gate 工作流：`pre-merge`、`post-merge-audit`、`historical-revalidation`。

### 4.5 最小关闭证据

- 计划 §1.2 第 4 行：默认拒绝、**一次显式放行**、`0.7.0` consumer 兼容、fresh/clean build、
  安装元数据与 hosted 证据；或 owner 接受例外并在 ADR/版本规范中记录。
- 计划 S7A-8.4：version-gate change、放行记录、consumer matrix；默认变更仍失败；显式一次性放行
  可审计；`0.7.0` consumer source-compatible。

## 5. 版本与门禁现状

| 项 | 值 |
| --- | --- |
| 显示版本 | `26.09.29-1`（年 26 / 月 9 / 日 29 / build 1） |
| SDK API | `0.7.0`，与日期构建身份独立 |
| required check | `Version advancement (pre-merge)` |
| 保护配置有效性 | 只有 2026-09-20 的当日快照，**未访问 GitHub API**，当前有效性未核实 |
| trusted baseline | 6 个受信任文件从 `CUEXIS_BASE_SHA` 取出；缺文件时以 `version.bootstrap.required` 失败，**从不回退到候选代码** |
| 基线祖先要求 | checker 要求 baseline 是 candidate 的祖先，且 ref 必须是完整 40 位 SHA |
| 本地三连命令 | `python -B tools/check_version_gate_tests.py`、`python -B tools/check_version_gate.py --check-current`、`python -B tools/update_version.py --check` |
| 版本变更后 | 必须 `--fresh` + `--clean-first`，避免陈旧生成头 |

## 6. 结论与待 owner 决策

| 交接项 | 实现状态 | 关闭还缺什么 | 性质 |
| --- | --- | --- | --- |
| 1 candidate 隔离 | 部分（开关 + 工厂 + 禁用用例） | `Cuexis_ALLOW_EXPERIMENTAL`、candidate flavor/安装元数据、Player `--candidate-entry`、至少一个 preset/CI 开启并消费 | 需要实现 + owner 决策（是否升 API） |
| 2 离线 assembler | 关键缺口（只有身份装配，feature 由调用方注入） | typed assembler、feature/resource closure 派生、移除测试注入后门、closure report | 需要实现 |
| 3 具名宿主命令循环 | **实现已在 `master`**（7 个动词） | 同 SHA 交接回归证据 + 动词命名口径统一 | 主要是命名与证据，非实现 |
| 4 SDK API `0.7.1` | 未实现，且门禁放行通路阻塞 | 修 version-gate 放行设计（或 owner 接受不升版本例外） | **阻塞**（SPEC-04 + 无 CODEOWNERS） |

待 owner 决策：

1. 第 1 项是否必须在本阶段实现 candidate flavor 与 `Cuexis_ALLOW_EXPERIMENTAL`，还是接受
   "default-OFF + 显式工厂 + 无 CI 开启"为当前边界并登记例外；
2. 第 2 项的 assembler 归属：`tools/` 独立离线工具，还是 `engine/chart` 内的库函数 + CLI；
3. 第 3 项的命名口径以哪一份为准（建议以实现与 ADR 0042 为准，修改计划措辞）；
4. 第 4 项：接受"不升 `0.7.1`、记录例外与后续归属"，还是先解除 version-gate 放行阻塞
   （需要 CODEOWNERS 与 required review 配置，属仓库外配置动作）。

> **文档缺陷 D-3**：计划自身引用的"W 类缺口"在仓库内没有定义出处。定义提案见
> [SUPPORT_AND_REJECTION_MATRIX.md](SUPPORT_AND_REJECTION_MATRIX.md) §6。
