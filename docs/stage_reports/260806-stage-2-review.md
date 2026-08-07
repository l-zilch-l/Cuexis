# 260806 阶段 2 验收与实现审查

状态：本地验收通过；hosted Linux CI 未验证

审查日期：2026-08-06

审查提交：`bc4bcb33043be971bfe00214205958e8f8ab2781`

对比基线：`d1e03f8`

## 1. 范围与结论

本次审查覆盖阶段 2 的 Chart v3、TimingMap、Behavior Event、Visibility/Material、迁移工具、
Playback/Player、SDK package consumer、格式与架构门禁，并检查 Windows/MSVC、Windows/MinGW
和 hosted CI 证据。审查以 ADR 0034/0036、`CHART_FORMAT.md` 和阶段 2 实施计划中的兼容、无损
迁移、零分配、Player parity 和跨平台门禁为验收合同。

阶段 2 的 9 项问题已经逐项修复，并由对应回归测试、安装包 consumer、零分配、CLI、GPU smoke
和本地构建矩阵复核。Windows/MSVC static/shared Debug/Release、headless、格式、架构、迁移
golden、external consumer 和全不可见帧均已通过。由于当前工作树未提交且 `gh run list --commit`
为空，hosted Linux GCC/Clang、sanitizer 和 hosted package jobs 尚无证据；这不阻断本地修复关闭，
但仍是跨平台发布前置条件。

## 2. 审查发现

| ID | 级别 | 问题 | 当前状态 |
|---|---|---|---|
| R01 | P1 | GCC/MinGW Release 因新增 aggregate 成员缺失初始化而被 `-Werror` 阻断 | 已关闭 |
| R02 | P1 | 迁移器成功返回但删除未绑定单 Key Behavior | 已关闭 |
| R03 | P1 | Chart v1/v2 的历史合法 BPM 被新 v3 范围拒绝 | 已关闭 |
| R04 | P1 | Material Step Event 在 Runtime update 中复制 AssetId 并分配 | 已关闭 |
| R05 | P1 | Player 将合法的全不可见帧视为 `player.render_scene.empty` 错误 | 已关闭 |
| R06 | P1 | 三个独立 `find_package` consumer 错误继承 manifest mode | 已关闭 |
| R07 | P2 | Windows checkout 的 CRLF/LF 差异击穿 byte-stable golden | 已关闭 |
| R08 | P2 | Runtime debug `finalValue` 记录 Behavior 输出而非提交后的值 | 已关闭 |
| R09 | P2 | 阶段计划、完成报告和 ADR 0030 与实际版本/门禁结果漂移 | 已关闭 |

### R01：跨编译器 Release 构建失败

`RuntimeBehavior`、`ChartMigrationReport`、`ChartBehavior` 和 `ChartTiming` 新增成员后，旧 aggregate
初始化没有显式补齐字段。本地使用 `headless-release`、GCC 16 和 `x64-mingw-dynamic` 依赖树时，
`chart_runtime.cpp:627`、`chart_migrator.cpp:481`、`canonical_chart_loader.cpp:1236/1626` 均触发
`-Werror=missing-field-initializers`。这会阻断 Windows MinGW Release，并同样影响启用 `-Werror`
的 GCC Release 门禁。

关闭标准：不禁用警告，显式初始化所有新增成员；MinGW headless Release 和 hosted GCC Release
均完成构建。

### R02：未绑定单 Key Behavior 数据丢失

迁移器把首 Key 写入绑定对象基准，剩余相邻 Key 转为 Event。未绑定单 Key Behavior 既没有可改写
对象，也不会生成 Event，随后在 `chart_migrator.cpp:548-562` 被删除。复现时 CLI 退出 `0`，报告
包含 `unbound.motion`，但 v3 输出已不存在该 Behavior。这违反 `CHART_FORMAT.md` 对未绑定数据
不得丢弃、无法证明等价时必须失败的要求。

关闭标准：可等价迁移的未绑定 Behavior 保留；不可表示的未绑定单 Key/空 Track 以稳定诊断失败，
且失败不修改目标文件。

### R03：旧 Chart BPM 兼容回归

`canonical_chart_loader.cpp:1608` 和 `timing_map.cpp:93` 将 `[1,65536]` 同时应用到旧路径，而 v1/v2
Schema 仍只要求 BPM 为有限正数。实测 v1 `defaultBpm=0.5` 与 `70000` 均被 validator 拒绝。

关闭标准：v1/v2 恢复有限正数合同；v3 继续严格执行 `[1,65536]`；三种格式均有边界测试。

### R04：Material Step Event 运行时分配

`behavior_system.cpp:197-205` 按值返回含 `std::string` 的 `PropertyValue`。使用长 Material AssetId
的探针在预热后连续 64 次 update 中观察到 384 次分配。现有 allocation fixture 的
`stepEvents` 为空，因此原门禁没有覆盖离散 Material 路径。

关闭标准：Material 选择在 prepare 后以稳定 view/index 贯穿 evaluate 和 resolver；包含长 AssetId
并跨 Step 边界的 update/extract allocation 测试为零。

### R05：全不可见帧无法由 Player 播放

`player_app.cpp:657` 将空 `RenderScene` 当作失败。包含三个 renderable 且 Beat 0 全部
`render.visible=false` 的合法 v3 Chart 可通过 validator，但 Player GPU smoke 退出 `1` 并报告
`player.render_scene.empty`。

关闭标准：空 Scene 正常 clear/present 并计入帧数；全不可见、可见性切换和 Seek fixture 均通过。

### R06：独立 consumer 配置失败

`VerifyExternalConsumer.cmake:112-117` 把顶层 `VCPKG_MANIFEST_MODE=ON` 传给没有 `vcpkg.json` 的
独立 package consumer。Playback、Core 和 AudioSDL 三个 `find_package` 测试均由 vcpkg 报告
找不到 manifest；shared import 检查因 consumer 可执行文件未生成而派生失败。

关闭标准：consumer 使用现有安装依赖树的 classic/offline mode；static/shared Debug/Release 的
三个 `find_package` 门禁均通过。

### R07：golden 换行不稳定

迁移器固定输出 LF；当前 Windows checkout 使用 `core.autocrlf=true`，两个 committed golden 被
检出为 CRLF。`cmake -E compare_files` 和 Catch2 字节比较同时失败。

关闭标准：`.gitattributes` 固定 golden 为 LF，并从启用 `core.autocrlf=true` 的全新 checkout
验证 CLI/Catch2 byte-stable 门禁。

### R08：调试最终值不准确

`runtime_session.cpp:901` 直接令 `finalValue = behaviorValue`。Transform scalar 随后会从 double
量化为 float，因此调试记录不一定等于最终 World 值，无法完整解释 Initial -> Behavior -> Final。

关闭标准：从各 resolver 的已验证候选值生成 `finalValue`；增加不可精确表示为 float 的标量回归。

### R09：文档与证据漂移

阶段 2 完成报告声明 Windows external consumer、golden 和零分配门禁通过，实际均有反例；计划仍
将 2G 标记完成；ADR 0030 仍称当前 consumer 使用 `0.3`，而 Stage 2 SDK API 已为 `0.4.0`。

关闭标准：整改期间明确标记验收不通过；全部技术问题关闭后，以同一提交的本地矩阵和 hosted run
URL 更新计划、完成报告和 ADR。

## 3. 修复前验收矩阵

| 配置 | 构建 | CTest/结果 |
|---|---|---|
| MSVC static Debug | 通过 | `240/245`，5 项失败 |
| MSVC static Release `/WX` | 通过 | `240/245`，5 项失败 |
| MSVC headless Debug | 通过 | `209/213`，4 项失败 |
| MSVC shared Debug | 通过 | `241/247`，6 项失败 |
| MSVC shared Release `/WX` | 通过 | `241/247`，6 项失败 |
| MinGW headless Release `/Werror` | 失败 | aggregate 初始化警告升级为错误 |
| Format / Architecture | 通过 | 无失败 |
| Debug / Release GPU smoke | 通过 | NVIDIA RTX 4060、OpenGL 3.3、3 帧 |
| Hosted CI | 无证据 | HEAD 无远端分支，commit run 列表为空 |

## 4. 整改结果与最终验收矩阵

| ID | 修复方案与证据 | 状态 |
|---|---|---|
| R01 | 显式初始化 Chart/Behavior aggregate 新成员；修复 MinGW 对齐分配测试；Windows MinGW headless Release `211/211` 通过，MSVC Release `/WX` 通过 | 已关闭 |
| R02 | 未绑定单 Key 不再静默删除；返回 `chart.migration.unbound_single_key_unrepresentable`，不写 artifact；新增单 Key 与多 Key 回归 | 已关闭 |
| R03 | v1/v2 保持有限正 BPM 合同，v3 保持 `[1,65536]`；增加 `0.5`、`70000` legacy 接受与 v3 拒绝测试 | 已关闭 |
| R04 | Material Step 使用稳定 `string_view`/候选引用；prepare 阶段预留 candidate、rollback、World 与 Snapshot 容量；真实长 AssetId 64 次 warmup 后更新/extract 为零分配 | 已关闭 |
| R05 | 新增 Player Snapshot-to-Scene 适配层，合法空 Scene 继续 clear/present；单测覆盖不可见/可见/Seek；Debug/Release 全不可见 GPU smoke 均为 `Objects: 4, debug commands: 0`、3 帧完成 | 已关闭 |
| R06 | package 构建复用显式 `VCPKG_INSTALLED_DIR`，独立 consumer 强制 manifest off 并传递 triplet prefix；static/shared Debug/Release 三种 find_package consumer 全部通过 | 已关闭 |
| R07 | `.gitattributes` 固定两个 golden 为 LF；当前字节扫描为 report `11 LF/0 CRLF`、chart `270 LF/0 CRLF`，CLI/Catch2 golden 通过 | 已关闭 |
| R08 | Transform/Camera/Appearance 从已验证 candidate 生成 debug `finalValue`；`0.1` float 量化回归证明 Behavior 与 Final 可区分 | 已关闭 |
| R09 | ADR 0030、阶段计划、完成报告和本 review 同步到 SDK API `0.4.0` 与真实门禁结果；hosted CI 明确标记未验证 | 已关闭 |

| 配置 | 构建 | CTest/结果 |
|---|---|---|
| MSVC static Debug | fresh/build | `249/249` 通过，1 项 symlink 条件跳过 |
| MSVC static Release `/WX` | fresh/clean-first | `249/249` 通过，1 项 symlink 条件跳过 |
| MSVC headless Debug | fresh/build | `216/216` 通过，1 项 symlink 条件跳过 |
| MSVC headless Release `/WX` | fresh/clean-first | `216/216` 通过，1 项 symlink 条件跳过 |
| MSVC shared Debug | fresh/build | `251/251` 通过，1 项 symlink 条件跳过 |
| MSVC shared Release `/WX` | fresh/clean-first | `251/251` 通过，1 项 symlink 条件跳过 |
| Windows MinGW headless Release | `-Werror` | `211/211` 通过，1 项 symlink 条件跳过；external package gate 未纳入该 headless 配置 |
| Format / Architecture / CLI / golden | 定向门禁 | 全部通过 |
| Debug/Release GPU smoke | NVIDIA RTX 4060, OpenGL 3.3 | 普通场景与全不可见场景均完成 3 帧 |
| Hosted CI | 无证据 | HEAD 无远端分支，`gh run list --commit` 返回空数组 |

## 5. 整改顺序与最终门禁

整改顺序为 R01/R03 -> R02 -> R04 -> R05/R08 -> R06/R07 -> R09。每项修复必须先增加或强化
对应回归，再运行所属模块测试。最终关闭需要同时满足：

- MSVC static/shared Debug/Release 与 headless CTest 全部通过：已满足。
- Windows MinGW Release 已通过；Linux GCC/Clang、sanitizer 和 package consumer hosted jobs 尚待 hosted run。
- format、architecture、external consumer、migration golden 和零分配门禁全部通过。
- Debug/Release GPU smoke 可播放普通帧与全不可见帧。
- 完成报告记录本地结果；hosted run URL 当前不存在，剩余风险为跨平台 CI 未验证。
