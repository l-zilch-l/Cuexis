# Stage 6 关闭报告

状态：completed（2026-09-27 关闭并归档）

更新日期：2026-09-28

本报告是 [Stage 6 计划](../../../stage_plans/completed/stage-06/plan.md) `S6-F2` 要求的阶段关闭记录：
按计划 §3 的验收 ID 链接实现、测试、SHA 与证据，汇总三个核验问题的处置，并给出 Stage 7A /
Stage 8 交接清单与未执行/残余项。项目所有者于 2026-09-27 明确接受该交接清单与残余清单，Stage 6
随之关闭，计划移至 `docs/stage_plans/completed/stage-06/`。PR、合并与发布仍属另行授权，本报告不
主张任何合并或发布动作。

## 0. 关闭与接受记录

- 2026-09-27：项目所有者接受 §6 Stage 7A 交接清单、§7 Stage 8 交接清单与 §8 未执行/残余清单，
  Stage 6 关闭并归档。
- 同时确认：三个核验问题（版本强制更新、后端中立 renderer、常用媒体）按 §5 处置为 `closed`；
  Chart v1–v3 退出提案保持 `candidate`；Stage 7A 与 Stage 8 仍按各自计划另行启动。

## 1. 关闭结论与当前状态

- 子批次 A1、A2、B1、C1、C2、C3、D1、D2、E1/E2、E3、C4 与 F1 均已退出，退出记录集中在
  [本目录](README.md)；F1 的最终验证见 [S6-F1 退出记录](2026-09-27-s6-f1-final-validation.md)。
- `S6-G01` 至 `S6-G15` 全部取得实现、注册与执行证据（§3），`S6-G15` 的 owner 交接与接受记录
  由 §0 完成。
- 三个开放工程问题（版本强制更新、后端中立 renderer、常用媒体支持）均已取得处置证据，核验记录
  中对应项已改为 `closed`（§5）；历史现象与影响未改写。
- 本阶段**不**关闭 Stage 7A/Stage 8，也**不**把 F1 的全绿测试解释为发布授权（plan §2 F2 第 4 条）。

## 2. 交付范围与批次

| 批次 | 内容 | 退出证据 | 状态 |
|---|---|---|---|
| A1 | 固定基线、代码入口盘点、测试与支持矩阵 | [基线报告](2026-09-20-s6-a1-baseline.md) | completed |
| A2 | Spec/Schema、API 草案、依赖图与接口表征 | [合同与表征](2026-09-21-s6-a2-contracts-and-characterization.md) | completed |
| B1 | 版本比较器、受保护门禁、发行 checklist | [版本门禁报告](2026-09-20-s6-b1-version-gate.md)、`tools/check_version_gate.py`、`.github/workflows/version-gate.yml` | completed |
| C1 | candidate 消费链、身份与要求数据保留 | [C1 退出](2026-09-22-s6-c1-exit.md) | completed |
| C2 | 配置解析、快照、设备 profile 与受控应用 | [C2 退出](2026-09-23-s6-c2-exit.md) | completed |
| C3 | Player 用户入口、状态机、跨子系统事务 | [C3 退出](2026-09-24-s6-c3-exit.md) | completed |
| D1 | 无环渲染合同、事务 token、测试 renderer | [D1 退出](2026-09-22-s6-d1-exit.md) | completed |
| D2 | OpenGL adapter 迁移、统一帧与诊断路径 | [D2 退出](2026-09-23-s6-d2-exit.md)、[最小化/恢复补充](2026-09-23-s6-d2-minimize-restore.md) | completed |
| E1 / E2 | 图片 / 音频离线导入及各自 golden | [E1/E2 退出](2026-09-27-s6-e1-e2-exit.md)、[实现报告](2026-09-25-s6-e1-e2-media-importer.md) | completed |
| E3 | 资源身份、缓存、索引与 CXC 原子发布 | [E3 退出](2026-09-26-s6-e3-exit.md)、[实现报告](2026-09-25-s6-e3-publication-transaction.md) | completed |
| C4 | 具名真实宿主、安装包、升级与部署证明 | [C4 退出](2026-09-27-s6-c4-exit.md)、[实现报告](2026-09-27-s6-c4-reference-host-and-player-distribution.md) | completed |
| F1 | 最终 SHA 全量、hosted、GPU 与许可证证据 | [F1 退出](2026-09-27-s6-f1-final-validation.md) | completed |
| F2 | 关闭报告、问题处置、Stage 7A/8 交接与 owner 接受 | 本报告 | completed（owner 接受已记录，见 §0/:15/:129） |

## 3. 验收 ID 映射

每行只列主要证据来源；完整命令、负例与测试注册数量在对应批次报告中。

说明：`S6-G10` 至 `S6-G15` 在批次报告中已按 ID 逐项映射；`S6-G01` 至 `S6-G09` 的批次报告写在
计划 §3.1 的 ID 表之前，正文未按 ID 引用，因此本表按计划 §3.1 的"主要批次"列建立 ID 级索引，
判定以批次报告中的实际命令与用例为准。A1/A2 属合同与基线批次，其证据是基线报告、ADR 0042、
Schema/API 草案、独立 golden 与无环依赖/安装图，按计划不要求 hosted。

| ID | 必须证明的结果 | 批次 | 证据 | 结果 |
|---|---|---|---|---|
| S6-G01 | 基线与八项冻结决策完成合同落盘/表征 | A1/A2 | [A1](2026-09-20-s6-a1-baseline.md)（记录固定基线 `e7d6c1ea…`、入口盘点与支持矩阵）、[A2](2026-09-21-s6-a2-contracts-and-characterization.md)、ADR 0042、Schema/API 草案与独立 golden | 通过 |
| S6-G02 | 显示版本相对受信任合并基线正确前进 | B1 | [B1 报告](2026-09-20-s6-b1-version-gate.md)；bootstrap 提交 `4545742ed63ae2d8f11ad07e80930ce5b88fa0ce` 通过受保护 Version Gate run `35586930775`，另有 run `35590201888`；正负例与 `version.bootstrap.required` 路径 | 通过 |
| S6-G03 | ProjectConfig 与 CXC file/memory 显式 candidate 消费，v4 仍可显式选择 | C1 | [C1 退出](2026-09-22-s6-c1-exit.md)（候选 SHA `3e11b16…`，hosted runs 35689650429/449/455/501）；默认 production 不隐式启用 candidate | 通过 |
| S6-G04 | 语义入口、profile/revision/预算和身份不可绕过 | C1 | 同 C1：非法 bytes、CRC/hash 篡改、不支持要求/section、失败无部分发布 | 通过 |
| S6-G05 | generated identity、父图、typed requirements 与 prepared identity 保真 | C1 | 同 C1：重排/碰撞/资源变化用例与完整字段比较；A16 处置见 §7 | 通过 |
| S6-G06 | 配置有唯一来源，不污染领域数据和活动 Session | C2 | [C2 退出](2026-09-23-s6-c2-exit.md)（最终 SHA `e01a4b1e74b1e7703b7df75710cacfeea6f8f381`，hosted runs 35773232548/573/675、35773238043/062/118/183） | 通过 |
| S6-G07 | 显式音频设备与输出校准按合同执行 | C2 | 同 C2：fake device 与真实设备 smoke、缺失/歧义/热拔插、无静默换设备 | 通过 |
| S6-G08 | v4/v5 共用加载/播放/暂停/停止/Seek/Reload 状态机 | C3 | [C3 退出](2026-09-24-s6-c3-exit.md)（候选 SHA `e5eb169d…`，hosted runs 35912538411/491/531、35912542466） | 通过 |
| S6-G09 | 中立 renderer 覆盖候选事务和统一帧，Player 正式循环不依赖具体 adapter | D1/D2 | [D1 退出](2026-09-22-s6-d1-exit.md)（`c2861c3d…`，runs 35701913616/626/630/660）；[D2 退出](2026-09-23-s6-d2-exit.md)（实现 `fdfaa46d…`；上一 hosted SHA `318b7d47…` 的 MinGW 因 `-Werror=reorder` 失败，`fdfaa46` 当时无同 SHA 三平台证据；计划行记录成员顺序修正后 `9bb94b4` 的 MinGW 通过，但报告目录内无其 run id；该报告同时记录 OpenGL 绘制未改为直接消费 `buildPresentationCommands`）；F1 在最终 SHA 上取得 MinGW push/PR 均 success，并另有 `headless-debug` 640/640、测试 renderer 与 OpenGL summary/pixel GPU smoke | 通过（历史缺口与残余见 §8） |
| S6-G10 | JPEG/PNG 输出可复现且有界 | E1 | [E1/E2 退出](2026-09-27-s6-e1-e2-exit.md)：18 个 TEST_CASE、CLI 门禁、18 个 golden；`730d0d6` 四平台 hosted 全绿（Linux 36007593499/36007604661 等） | 通过 |
| S6-G11 | MP3/Ogg Vorbis/FLAC 输出可复现且有界 | E2 | 同 E1/E2 退出记录：三格式 golden、长度/起止样本、解压预算与失败负例；golden `df73cb81…` 重新冻结 | 通过 |
| S6-G12 | 媒体 provenance、缓存、资源 closure 和发布事务完整 | E3 | [E3 退出](2026-09-26-s6-e3-exit.md)（`47d90c3`/`5a5e21e1…`/`0753e6a0…`/`730d0d6`，runs 36261277766/831/846、36261281151、36264473853/856/857、36264475784） | 通过 |
| S6-G13 | SDK 与 Player 可独立安装部署，真实宿主使用公开边界 | C4 | [C4 退出](2026-09-27-s6-c4-exit.md)：实现与门禁修复 SHA `d7980bd`（runs 36298996619/623/643、36298998943/948/952/954、36298998954）；docs 证据 SHA `2c8211e`（runs 36301619334、36301616745、36301619354、36301616792、36301619357、36301616748、36301619361）；承载退出报告的 docs-only SHA `cb56e62`（runs 36304734922、36304732705、36304734892、36304732708、36304734884、36304732697、36304734904）。`716b697` 在该报告中无同 SHA run 记录。安装后宿主、staging、static/shared、升级/拒绝门禁 | 通过（归因订正见下条） |
| S6-G14 | 兼容、架构、许可证与候选隔离没有回归 | F1 | [F1 退出](2026-09-27-s6-f1-final-validation.md)：8 个 preset 的 fresh/clean-first 全量矩阵、developer-tools OFF 与 shader-tools OFF/ON 两向、候选开关隔离、分发许可证集合 | 通过 |
| S6-G15 | 最终代码拥有完整同 SHA 验证与 owner 交接 | F1/F2 | F1 部分：`c80a5b5` 的 7 个 hosted run 全绿（36323246316/366/298/392/305/349/354）、`95bfa70` 的 7 个 run 全绿（36328047694/358/772/360/781/305/306）、GPU/设备单列证据；owner 交接部分：项目所有者于 2026-09-27 接受 §6/§7 交接清单与 §8 残余清单（§0） | 通过 |

**订正说明（2026-09-28，Stage 6 复核 SPEC-26）**：`S6-G13` 行原先把
runs `36298996619/623/643`、`36298998943/948/952/954`、`36301616745` 记在 SHA
`716b697`、`2c8211e`、`cb56e62` 名下，与 [C4 退出报告](2026-09-27-s6-c4-exit.md) §8 不符。
该报告 §8 明确：这批 `3629…` run 属于**实现与门禁修复 SHA `d7980bd`**；
`36301616745` 与其它 `3630161…` run 属于 docs 证据 SHA `2c8211e`；
`cb56e62`（承载该退出报告的 docs-only 提交）有自己的一组 run（`36304732…`/`36304734…`）；
`716b697` 在该报告中没有任何同 SHA run 记录（它只作为实现过程的中间提交出现）。
上表已按 §8 的 run 表重写归属（把 `d7980bd` 放回、补 `cb56e62` 的 run），
并按 `S6-F2` 第 1 条的要求保留原现象不回改。`S6-G13` 的「通过」结论本身不变——
它依据的是那些 run 实际 success，订正的只是 SHA↔run 的追溯指针。

## 4. 最终 SHA 与验证矩阵

| 项 | 值 |
|---|---|
| 代码末端 SHA | `d7980bd9b5cf7730889157ea8ec093799d53d7fc` |
| 最终候选 SHA | `c80a5b5f43a6aa7021037aaf2e308e2b9ec3d764`（仅在其上追加文档） |
| 报告承载 SHA | `95bfa70343e1628e49b292ba994e7ba7e2c55a32`（F1 退出记录；7 个 hosted run 全绿） |
| 受信任基线 | `46b65d1f2345f543b98e7e87fe5ec9ed735f10bf` |
| 版本 / SDK API | `26.09.27-1` / `0.7.0`（本阶段没有新增公共 SDK 契约） |

本地全量矩阵（fresh configure + `--clean-first`，8 个 preset）：`debug` 734/734、`release` 734/734、
`shared-debug` 737/737、`shared-release` 737/737、`debug-media-tools` 776/776、
`debug-shader-tools` 766/766、`headless-debug` 640/640、`mingw-debug` 734/734；候选开关配置
737/737。宿主机门禁（staging、分发、7 条外部消费者）在每个 preset 中实际运行。

同 SHA hosted：Linux Quality 12 个作业、Windows MSVC 与 Windows MinGW 各自 Debug/Release 加
`debug-media-tools` 整包（734 与 776），Linux 侧 631–680 不等；push 与 pull_request 两类事件
全部 success。插桩与覆盖作业中 candidate、lowering、配置与媒体用例均有实际注册与运行记录。

## 5. 三个核验问题的处置

核验记录：[Stage Verification 2026-09 findings](../../reviews/stage-verification-2026-09/2026-09-01-findings.md)，
对应项状态已改为 `closed`，历史现象/影响保持原样。

| 问题 | 处置证据 | 状态 |
|---|---|---|
| 问题 1：仓库版本号更新落后，需增加强制更新约束 | B1 落地 `tools/check_version_gate.py`、focused tests 与 `.github/workflows/version-gate.yml`：受保护 `master` 基线、trusted UTC 日期、四元组比较、祖先关系与 bootstrap 例外；`4545742…` 通过受保护 run `35586930775`；[VERSIONING.md](../../../guides/VERSIONING.md) 记录约束与命令。本阶段实际执行了多次日期滚动（`26.09.24-1`→`26.09.26-1`→`26.09.27-1`），期间 `26.09.23-1` 被 `version.release_date.future` 正确拒绝、`66a15d0` 的 Version Gate 因 `version.release_date.stale` 失败并在滚动日期后恢复，均由门禁而非人工判断捕获 | closed |
| 问题 2：渲染后端抽象过薄，阻碍 Vulkan 接入 | D1 建立无环渲染合同、事务 token 与测试 renderer，D2 完成 OpenGL adapter 迁移到统一帧与诊断路径；边界写入 [ADR 0042](../../../adr/0042-stage-6-productization-boundaries.md)。两向证据：适配器关闭的 `headless-debug` 640/640 全绿，适配器可用的 Debug/Release 与 shared 组合 734–737 全绿，另有 OpenGL summary/pixel 的 GPU smoke（退出码 0、6 帧，像素与冻结值一致）。D2 报告同时记录"OpenGL 绘制未改为直接消费 `buildPresentationCommands`"（§8 保留）。Vulkan 本体仍属 Stage 10，本阶段不建立未验证的公共占位接口 | closed |
| 问题 3：当前无法直接使用常见图片/音频 | E1/E2 以可选 `media-tools` feature 接入 libpng、libjpeg-turbo、minimp3、libvorbis、libogg、libFLAC，交付内部 `cuexis_media_import` 与 developer-only CLI，18 个用例与 18 个 golden；E3 补齐 provenance、缓存、资源 closure 与 CXC 原子发布；[DEPENDENCY_POLICY.md](../../../guides/DEPENDENCY_POLICY.md)、[THIRD_PARTY_NOTICES.md](../../../../THIRD_PARTY_NOTICES.md) 与 [BUILDING.md](../../../guides/BUILDING.md) 已同步，工具默认关闭且不进入 Playback/Player 链接闭包 | closed |

## 6. Stage 7A 交接清单

| 交接项（plan §2 F2 第 2 条） | 交接内容与证据 |
|---|---|
| typed requirements | C1 交付 typed 记录与完整字段比较（`S6-G05`，[C1 退出](2026-09-22-s6-c1-exit.md)）；Playback 侧不解析 JSON/CXC/CXT，Stage 7A 直接消费 typed 结构 |
| 实体身份 | generated identity、父图与 prepared identity 的保真与拒绝边界（`S6-G05`）；E3 的资源身份与 provenance（`S6-G12`） |
| 时间与 discontinuity | C3 的时钟、discontinuity、Seek/Reload 状态机与命令转换表（`S6-G08`，[C3 退出](2026-09-24-s6-c3-exit.md)）；`headless-debug` 与 C3 故障注入 |
| 执行配置身份 | C2 的配置唯一来源、规范身份、快照与设备 profile（`S6-G06`/`S6-G07`，[C2 退出](2026-09-23-s6-c2-exit.md)） |
| 拒绝边界 | 语义入口、profile/revision/预算、不支持 section、坏文件/未来版本/工具链 minor 的稳定拒绝（`S6-G04`、`S6-G06`、`S6-G13`） |

## 7. Stage 8 交接清单

| 交接项（plan §2 F2 第 2 条） | 交接内容与证据 |
|---|---|
| candidate 入口与隔离策略 | 显式 candidate 消费（ProjectConfig、CXC file/memory）与默认 production 不隐式启用；构建开关 `CUEXIS_ENABLE_CHART_V5_CANDIDATE` 默认关闭，F1 记录开关开启配置 737/737 |
| source / compiled / prepared 身份 | C1 的身份保真、重排/碰撞用例与 prepared identity（`S6-G05`）；E3 的包身份与 generation 目录（`S6-G12`） |
| A16 处置 | 按 R5 交接清单与 ADR 0042 记录于 C1 退出报告；本阶段不扩张 [R5 消费边界 §10](../../stages/chart-format-foundation/2026-09-17-r5-regression-and-handoff.md) |
| profile / 预算 / 媒体 provenance | C2 的 profile 与预算拒绝；E3 的 provenance 四类身份、缓存与 closure 预算（`S6-G12`）；E1/E2 的字节预算与拒绝负例 |
| 发行剩余门禁 | 完整 Chart v5/CXT v2 正式发行、默认 Writer 切换、v4→v5 迁移与 CXC v1 Packed playback entry 的最终发布门禁仍属 Stage 8；**实验入口不视为正式 v5 发行** |

## 8. 未执行、阻塞与残余

| 项 | 状态 | 说明 |
|---|---|---|
| owner 对交接清单与残余的接受 | 已记录 | 项目所有者于 2026-09-27 明确接受 §6/§7 交接清单与 §8 残余清单（§0） |
| Stage 6 关闭与归档 | 已完成 | 计划移至 `docs/stage_plans/completed/stage-06/`，状态改为 completed；PR、合并与发布仍为另行授权动作 |
| 真实音频设备 smoke 的 underrun | 观察保留 | 空载复跑 3 次为 1、1、0；三次均退出码 0、90 帧、暂停/重载断言通过，未定位确定性成因 |
| C2 行记录的"新增测试 hosted 复验尚未发生" | 已被覆盖 | C2 退出时新增的进程锁、只读文件、热拔插观察与命名设备打开用例属默认套件；F1 在 `c80a5b5` 的同 SHA hosted 中，配置/设备用例族在 Linux 插桩与覆盖作业及 Windows 整包（734/776）中实际运行，例如 `SDL opens one enumerated playback device by its exact name` |
| D2 退出时的 MinGW hosted 缺口 | 已被覆盖 | D2 退出报告记录上一 hosted SHA 的 MinGW 因 `-Werror=reorder` 失败，实现 SHA `fdfaa46` 当时没有同 SHA 三平台证据；最终 SHA `c80a5b5` 的 MinGW push `36323246305` 与 PR `36323249349` 均 success，该历史缺口在最终 SHA 上关闭 |
| OpenGL 绘制未改为直接消费 `buildPresentationCommands` | 残余（有意保留） | D2 报告记录该项未做；本阶段以测试 renderer、headless 640/640 与 OpenGL summary/pixel GPU smoke 作为 S6-G09 证据，不把该重构项算作已完成 |
| D1/C1 报告记录的细粒度缺口 | 残余（有意保留） | C1 的"零限制与 presentation stale generation 无新增 C1 专用 Playback 用例"、`candidate.identity.resource_conflict` 无单独新用例；C3 的 `WindowKey` 上报无自动化单元测试、显式 `shutdown()` 只在正常退出路径被走到；C3 设备变化路径仅在 fake seat 验证；C4 的 hosted 分发门禁只在 Windows 注册、升级示例只验证重新构建路径 |
| A1/A2 无 hosted 证据 | 按计划不需 hosted | A1/A2 的证据要求是基线报告、ADR 0042、Schema/API 草案、独立 golden 与无环依赖/安装图（计划 §3.1 `S6-G01`） |
| 本地 Linux sanitizer/coverage | 不适用 | Windows 主机无法运行这些预设（`CMakeLists.txt` 要求 Clang/GCC）；由 hosted 作业承担，WSL 复现路径仅作调试工具 |
| 提案 1：分期退出 Chart v1–v3 | 仍为 candidate | 与本阶段独立，按 ADR 0041 与单独 owner 决策处理；本阶段未启动其第一步 |
| R5 §10.1 消费边界 | 未扩张 | 本阶段报告只引用，不新增允许/禁止消费项 |
| 批次报告自认但未进本 §8 的残余 | 追加承接 | **订正（2026-09-28，Stage 6 复核 SPEC-32）**：以下 5 条原由批次报告自认但未进本 §8，现补入本表以保可见性。它们**未随 §0 的 2026-09-27 owner 接受一并被接受**——那是对本表当时内容的接受；这 5 条属于事后复核新补的可见项，按主题移交 Stage 7A / Stage 8 处置：① C4 退出 §7：宿主二进制的符号级检查（导入表/依赖清单白名单）还不是门禁（后继见 Stage 6 复核修正计划 R7）；② C4 退出 §7：插桩 preset 的宿主门禁依赖"把父构建的插桩选项转发给外部工程"这一约定，是维护负担（已在 [BUILDING.md](../../../guides/BUILDING.md) 记录，见 R1）；③ C4 退出 §7：真正的磁盘满、只读介质与配额失败未取证（E3 残余，后继见修正计划 R5）；④ C4 退出 §7：升级示例只验证重新构建路径、未实现配置迁移（两个配置格式当前都只有 v1）；⑤ C4 实现报告 §3.4：错误 SDK minor 拒绝此前仅手动核对（后继见修正计划 R7）。|

## 9. 明确不包含（计划 §4）

Studio 编辑器或 Studio Preview 实现；InputProfile、CalibrationProfile、Judgement/Replay 与稳定
C ABI；Player 私有 Runtime 路径或宿主专用依赖进入 Playback 核心；Vulkan adapter 实现或未验证的
公共占位接口；未经另行决策的运行时媒体直解码、任意格式猜测或无限制解压；Chart v1–v3
Reader/Writer/迁移退出；完整 Chart v5/CXT v2 正式发行、默认 Writer 切换、v4→v5 迁移与 CXC v1
Packed playback entry 的最终发布门禁（属 Stage 8）；Hold/Release、Slide/Flick、多指与未登记
Behavior/Animation/Effect；运行时脚本与逐帧 script callback（无限期延后，不预留字段/extension/
capability/bytecode/ABI/hook）。

未来 Studio 只继承"单一 PlaybackSession 路径"约束，本阶段不要求实现 Studio Preview。

## 10. 文档同步

| 文档 | 动作 |
|---|---|
| [CURRENT_STATUS.md](../../../CURRENT_STATUS.md) | Stage 6 行改为 completed（关闭报告、问题处置与交接清单已接受）；追加 F2 段落 |
| [ROADMAP.md](../../../ROADMAP.md) | 阶段状态与并行工作段落更新为已关闭、计划已归档 |
| [本目录 README](README.md) | 追加本报告与 F1 退出记录入口，目录状态改为 historical |
| [stage_reports 索引](../../README.md) | Stage 6 条目追加关闭报告入口 |
| [核验记录](../../reviews/stage-verification-2026-09/2026-09-01-findings.md) | 问题 1/2/3 状态改为 `closed` 并追加处置证据；提案 1 保持 candidate |
| [Stage 6 计划](../../../stage_plans/completed/stage-06/plan.md) | 状态改为 completed、F2 行记录 owner 接受并归档；`docs/stage_plans/legacy-paths.md` 记录旧路径 `active/stage-06/plan.md` |
| `docs/api/README.md` | 无需改动：它是按任务导航的索引页，本阶段没有新增公共 SDK 契约（SDK API 保持 `0.7.0`） |
| BUILDING.md / VERSIONING.md / DEPENDENCY_POLICY.md / THIRD_PARTY_NOTICES.md | 已在 B1 与 E1/E2 批次更新，本阶段不再追加 |

## 11. 证据索引

批次报告（本目录）：A1、A2、B1、C1（含实现与退出）、C2、C3、D1、D2（含最小化/恢复）、E1/E2
（实现与退出）、E3（实现与退出）、C4（实现与退出）、F1 退出，以及本报告。

hosted 证据（按批次，SHA 见 §3）：

- B1：`35586930775`、`35590201888`
- C1：`35689650429`、`35689650449`、`35689650455`、`35689650501`
- D1：`35701913616`、`35701913626`、`35701913630`、`35701913660`
- D2：`35754056078`、`35754056117`、`35754056194`、`35754062170`、`35754062178`、`35754062179`、`35754062420`
- C2：`35773232548`、`35773232573`、`35773232675`、`35773238043`、`35773238062`、`35773238118`、`35773238183`
- C3：`35912538411`、`35912538491`、`35912538531`、`35912542466`
- E1/E2：`35977679343`、`35977679432`、`35977679483`、`35977684169`、`35997124047`、`35997124189`、`35997124364`、`35997124499`、`36007593462`、`36007593484`、`36007593499`、`36007604661`、`36007604713`、`36007604745`、`36007604800`
- E3：`36261277766`、`36261277831`、`36261277846`、`36261281151`、`36264473853`、`36264473856`、`36264473857`、`36264475784`
- C4：`36295147060`、`36295147115`、`36295147129`、`36295150623`、`36295150629`、`36295150667`、`36295150669`、`36298996619`、`36298996623`、`36298996643`、`36298998943`、`36298998948`、`36298998952`、`36298998954`、`36301616745`
- F1：`36323246298`、`36323246305`、`36323246316`、`36323249349`、`36323249354`、`36323249366`、`36323249392`、`36328047694`、`36328047772`、`36328047781`、`36328051305`、`36328051306`、`36328051358`、`36328051360`

本地证据（随 `out/` 忽略）：F1 的 `out/f1/*.log`、`out/f1-local-matrix.ps1`、`out/f1-candidate.ps1`、
`out/f1-smoke.ps1`，见 [F1 退出记录 §11](2026-09-27-s6-f1-final-validation.md)。
