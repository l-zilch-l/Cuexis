# Stage 6 Implementation Plan: Playback C++ API and Player Productization

状态：active；当前实施阶段；Foundation 交接加固（R0-R5）已于 2026-09-17 关闭并经 owner
接受，本阶段自此恢复实施

更新日期：2026-09-22

归档来源：[旧版 PROJECT_GUIDE](../../../archive/PROJECT_GUIDE_LEGACY_2026-08-10.md) 与
[SDK transition plan 快照](../../../archive/CUEXIS_SDK_TRANSITION_PLAN_2026-08-10.md)。2026-09-01
[阶段核验记录](../../../stage_reports/reviews/stage-verification-2026-09/2026-09-01-findings.md)
中的三个 open 问题已纳入本计划；在本阶段启动、实现、验证和处置证据完成前，它们保持 open。

Chart Format Foundation 的原 owner 完成确认保留，但后续技术复核发现身份、profile、
预算与验证证据缺口，见
[复核记录](../../../stage_reports/stages/chart-format-foundation/2026-09-16-handoff-review.md)。
本阶段于 2026-09-16 移回 future；恢复前必须完成
[Foundation 交接加固](../../completed/chart-format-foundation-hardening/plan.md) 的 R0-R5，
取得最终候选 SHA 的要求验证及 owner acceptance。这三项已于 2026-09-17 完成：本地六个配置
全量回归、最终 SHA `0e501a5` 容量复跑与同 SHA hosted 三平台验证（报告 SHA `e0ca9ff`，
docs-only 复验 `c24f34e`）全部通过，owner 接受其
[R5 交接清单](../../../stage_reports/stages/chart-format-foundation/2026-09-17-r5-regression-and-handoff.md)
（允许/禁止消费、identity/revision 政策、预算与残余见该报告 §10）。S6-A 至 S6-F 不因此
标记完成，三个工程问题仍保持 open。

**本阶段于 2026-09-17 启动**，从 §2 的第一个批次 S6-A（合同、基线和依赖决策）开始；S6-A 至
S6-F 的前置、范围和门禁以本计划各批次为准，当前实现对 v5 candidate 的消费不得超出
[R5 报告](../../../stage_reports/stages/chart-format-foundation/2026-09-17-r5-regression-and-handoff.md)
§10.1 的允许清单。

本阶段位于 Chart Format Foundation 之后，采用 Chart v5 Core/Packed candidate 作为主要开发
和验证基线，同时保留 Chart v4、CXT v1、CXC v1 和 SDK `0.7.0` 作为兼容与回退基线。Stage 6
负责证明 v5 candidate path 可以被 Player/Playback 消费，但不要求一次实现完整 Slide、多指、
Flick 或全部未来 Judgement 能力。Chart v5 的正式默认 Writer、正式发行入口和完整语义收敛
留在 Stage 8；Chart v4 在本阶段继续作为可靠回退输入。

## 1. 阶段目标

在 Stage 1E 的外部消费和 Stage 3 的 portable presentation 基础上，稳定 Playback C++ 使用、
弃用和升级政策，并把独立 Cuexis Player 产品化。本阶段继续是 matching-toolchain C++ 边界，
不在 Judgement/Replay 完成前冻结稳定 C ABI。

Stage 6 同时关闭阶段核验发现的三个工程缺口：恢复可追溯的仓库版本门禁；建立 Player 与具体
OpenGL adapter 之间的后端中立表现渲染边界；为常见图片和音频建立可验证的导入或解码路径。
Chart v1-v3 退出仍是待项目所有者决策的 candidate 提案，不因本阶段启动而进入实施范围。

2026-09-20 的可实施性评审以本地 `master` 的 `e3c4589` 为阅读基线。owner 已接受将评审
建议落实为详细计划，并授权冻结关键实施决策；其权威结论见
[ADR 0042](../../../adr/0042-stage-6-productization-boundaries.md)。设计已冻结不表示字段级
Schema、表征或实施批次已经通过。A1、A2、B1、C1、C2、D1 和 D2 已有各自退出证据；其余批次尚未完成。
本计划不替代
[ADR 0024](../../../adr/0024-configuration-ownership-and-staged-formats.md)、
[ADR 0033](../../../adr/0033-cpp-shared-library-preview-boundary.md)、
[Packed Spec](../../../formats/PACKED_CHART_FORMAT.md) 或 R5 交接合同。

## 2. 工作流和批次

### 2.1 实施顺序与门禁

批次编号表示职责，不表示必须按字母串行。实际顺序如下：

```text
S6-A1 基线、入口和证据矩阵
  -> S6-A2 按 ADR 0042 落盘合同并完成表征
       -> S6-B1 版本门禁
       -> S6-C1 candidate source / CXC / typed lowering
       -> S6-D1 渲染接口与测试 renderer -> S6-D2 OpenGL 迁移
       -> S6-C2 配置与音频设备
       -> S6-E1 图片导入 / S6-E2 音频导入
  E1 + E2 + C1 -> S6-E3 资源与打包事务
  C1 + C2 + D2 -> S6-C3 Player 命令、状态机与事务集成
  C3 + E3     -> S6-C4 真实宿主、安装和升级验证
  B1 + C4 + 各批次证据 -> S6-F1 最终矩阵 -> S6-F2 关闭与交接
```

- A1 必须先完成；A2 必须将已冻结决策落实为 Spec/API 草案与表征证据，才能释放后续实现。
  研究性原型不能作为已发布 API 或完成证据。新证据要求改变关键决策时，先提交复现和替代
  方案，经 owner 重新裁定 ADR 0042 后回到 A2，并重验所有受影响批次。
- B1、C1、D1、C2、E1/E2 可在合同冻结后并行，但修改相同公共头、根 CMake allowlist、
  package export 或 manifest 时必须串行集成。所有新 `cuexis_*` target 同步登记。
- D1/D2 首先用 v4 和既有 portable fixture 验证，不等待 v5 接线；E1/E2 先输出既有
  portable texture / WAV，不等待 Player 新控制层。E3 再接入 candidate 与 v4 包。
- C3 可以先用测试 renderer/device 开发，但不得在 C1、C2、D2 未退出时标记完成。
- B1 生效后的每次合并都执行版本门禁，不等到 F 才更新版本；B1 按本节第 6 项使用 trusted UTC
  日期修正当前显示版本，并在版本源变化后执行 fresh configure 和 clean-first build。
- 后续执行默认按一个子批次推进；允许依据依赖图并行，不允许因下游测试通过而跳过上游
  合同、失败路径或外部消费验收。

| 子批次 | 前置 | 必要输出 | 实施状态 |
| --- | --- | --- | --- |
| A1 | R5 交接已接受 | 固定基线、代码入口盘点、测试与支持矩阵 | completed ([报告](../../../stage_reports/stages/stage-06/2026-09-20-s6-a1-baseline.md)) |
| A2 | A1、ADR 0042 已冻结 | Spec/Schema、API 草案、依赖图与接口表征 | completed ([报告](../../../stage_reports/stages/stage-06/2026-09-21-s6-a2-contracts-and-characterization.md)) |
| B1 | A2 | 版本比较器、受保护门禁、发行 checklist | completed（trusted baseline、required Version Gate、同 SHA hosted 验证与合并后审计均已记录，见报告） |
| C1 | A2 | candidate 消费链、身份和要求数据保留 | completed（本地 MSVC 快照与同 SHA hosted 默认 OFF 矩阵见 [退出报告](../../../stage_reports/stages/stage-06/2026-09-22-s6-c1-exit.md)；不是 Stage 6 关闭或 owner acceptance） |
| D1 | A2 | 无环渲染合同、事务 token、测试 renderer | completed（本地 MSVC 快照与同 SHA hosted 默认矩阵见 [退出报告](../../../stage_reports/stages/stage-06/2026-09-22-s6-d1-exit.md)；OpenGL 迁移仍属 D2） |
| D2 | D1 | OpenGL adapter 迁移、统一帧与诊断路径 | completed（退出当时的记录见 [退出报告](../../../stage_reports/stages/stage-06/2026-09-23-s6-d2-exit.md)；成员顺序修正后的 MinGW 在 `9bb94b4` 已通过。本机最小化/恢复见 [补充记录](../../../stage_reports/stages/stage-06/2026-09-23-s6-d2-minimize-restore.md)，最小化时 drawable 仍为 1280x720） |
| C2 | A2 | 配置解析、快照、设备 profile 与受控应用 | completed（本地验收与 `e01a4b1` hosted 见 [退出报告](../../../stage_reports/stages/stage-06/2026-09-23-s6-c2-exit.md)；新增测试的 hosted 复验尚未发生） |
| C3 | C1、C2、D2 | Player 用户入口、状态机、跨子系统事务 | completed（本地验收见 [退出报告](../../../stage_reports/stages/stage-06/2026-09-24-s6-c3-exit.md)；该退出不是 Stage 6 关闭或 owner acceptance） |
| E1 / E2 | A2 | 图片 / 音频离线导入及各自 golden | planned |
| E3 | E1、E2、C1 | 资源身份、缓存、索引与 CXC 原子发布 | planned |
| C4 | C3、E3 | 具名真实宿主、安装包、升级与部署证明 | planned |
| F1 | B1、C4 及其前置全部退出 | 最终 SHA 全量、hosted、GPU 与许可证证据 | planned |
| F2 | F1 | 关闭报告、问题处置、Stage 7A/8 交接与 owner 接受 | planned |

`planned` 表示尚未取得退出证据；Stage 6 的 active 状态不等于尚未列出证据的批次已完成。实际工作开始后
逐项更新为 in_progress、blocked 或 completed，并链接证据。阻塞项必须记录原因和恢复条件，
不能以缩小测试矩阵或静默放宽合同解除。

### S6-A：合同、基线和依赖决策

#### S6-A1：建立可执行基线

1. 开工时重新记录 HEAD、分支、工作区、UTC 日期、显示版本、SDK API、工具链与 preset。
   `e3c4589` 只是本次阅读基线，不替代执行时的 SHA，也不替代 R5 已记录的测试 SHA。
2. 对照 R5 §10 建立支持表：仅 `flags=1` / `candidateRevision=1` 与已接受的静态组件、
   Tap point / lanes4 profile；正式格式仍为既有 v4/CXT v1/CXC v1，SDK 基线仍为 `0.7.0`。
   Hold/Release、range 判定、Slide/Flick、多指及未登记 Behavior/Animation/Effect 不进入范围。
3. 盘点 source、CXC entry 校验、Packed decode、Runtime lowering、prepared identity、
   presentation、音频、配置、安装与 consumer 的实际调用链。分别标明现有能力、新增桥接
   和工具层独有能力，不将测试辅助函数视为生产入口。
4. 建立验收矩阵，逐项记录合同、入口、目标模块、正例、负例、诊断、兼容要求、执行命令、
   SHA 与证据路径。区分 production package、candidate opt-in 和开发工具配置。
5. 跑 Debug/Release 基线以及已有 architecture/package/consumer 门禁；设备与 GPU 测试单列。
   检查 Foundation candidate CXC 用例在各 CI 配置是否实际注册，不照抄历史报告的覆盖推断。
   遇到基线失败先定责，不能用旧报告将本次结果写为通过。

代码盘点起点包括
[PlaybackSource](../../../../engine/playback/include/cuexis/playback/playback_source.hpp)、
[Playback prepare](../../../../engine/playback/src/playback_session.cpp)、
[CXC candidate validator](../../../../tools/cxc_common/src/cxc_candidate.cpp)、
[Player](../../../../app/player/src/player_app.cpp) 和
[OpenGL adapter](../../../../engine/render_opengl/include/cuexis/render_opengl/open_gl_backend.hpp)。

输出与退出：带日期基线报告、实际注册测试清单及缺口台账；每个新增验收项有负责子批次，
不存在“由后续自行处理”的无归属项。A1 不宣称实现了 v5 Playback。

#### S6-A2：冻结决策的合同落盘与表征

以下八项已由 ADR 0042 固定。实现者不得以实现成本低为由改用其它架构；A2 的任务是明确
字段/声明、形成独立 golden 和最小表征，证明可以按冻结方案实施。若表征失败，报告阻塞，
不能通过自选替代方案或放宽验收宣布退出。下面只列摘要，精确边界与拒绝方案以 ADR 为准。

| ID | 已冻结主题 | 固定选择 | 主要消费批次 |
| --- | --- | --- | --- |
| S6-D01 | candidate source 与安装边界 | 默认 OFF 的 experimental 构建开关 + 通用显式 entry 工厂；旧工厂不变，项目/CXC metadata 选择，裸 Packed 不作用户入口 | C1、C4 |
| S6-D02 | lowering、要求保留与身份 | typed lowering；完整 tuple/requirements 保留，域分离 SHA-256 execution ID 并检测冲突；内容身份与执行配置身份分开 | C1、C3 |
| S6-D03 | A16 feature / resource closure | chart 内部离线 typed assembler 派生并校验；Writer 不替调用方修复，Playback 不展开 CXT v2 | C1、E3 |
| S6-D04 | renderer 分层 | 新内部 cuexis_presentation_renderer 位于 Playback/render 之上；不安装、不反向依赖；统一 command builder 和事务 token | D1、D2、C3 |
| S6-D05 | 配置、设备与控制 | 内部 player_support，单 owner-thread 命令层；设备精确 selector，不冒充硬件认证；音频激活在无失败内容/GPU 交换前 | C2、C3 |
| S6-D06 | 媒体与发布 | 固定五种格式解码栈；离线 RGBA8 sRGB / S16LE WAV；独立 media-tools、严格预算、跨平台 bytes golden、不可变 generation/原子包发布 | E1、E2、E3 |
| S6-D07 | 版本门禁 | 受保护 master 最新 SHA、分支保持最新、串行合并、受信任 UTC；docs-only 不豁免，历史复验不产生发行 | B1、F1 |
| S6-D08 | SDK 与真实宿主 | 条件性源兼容增量目标 0.7.1；Stage 8 不预留版本号；独立 Cuexis Reference Host 使用安装公共 API | C4、F2 |

特别要求：

- SDK `0.7.1` 只允许 ADR 0042 的 source-compatible additive 变更；不改既有布局、签名、
  虚表和默认行为。需要不兼容变更或集成基线变化时先重开版本决策，按
  [SDK 版本规范](../../../guides/VERSIONING.md) 裁定并同步后续计划；不为 Stage 8 预留 minor，
  不以控制版本号增长为由维持不兼容的 patch。
- A2 必须为 execution ID、prepared 内容身份、执行配置身份提供独立预映像/golden；
  旧 v4 identity 与 FrameDigest v1-v3 算法保持不变。禁止为了容纳 generated ID 改旧 golden。
- A16 按离线 assembler 方案实施，不保留“由测试或调用方自行补 feature”的后门。
- Cuexis Reference Host 必须有自己的交互命令循环、时钟、provider 和帧/资源消费；
  无需第三方引擎 SDK，不得将纯编译 consumer 改名充当适配证明。
- 媒体库已按固定 baseline 选择。A2 核对对应版本许可证、构建选项、预算能力和确定性，
  不静默替换 decoder。无法满足时阻塞该决策并请求重新裁定。

输出与退出：按 ADR 0042 形成 API 声明草案、无环依赖/安装图、配置字段所有权表、
媒体预算/profile Spec、entry 扩展 Schema、独立 golden、接口表征和支持矩阵。
所有新增合同必须可测试，不能只复制 ADR 正文。新 Spec 承担字段合同，本计划仅引用。

### S6-B：仓库版本和发行门禁

#### S6-B1：可追溯的合并与发行门禁

前置：A2 / S6-D07。主要落点为 `tools/update_version.py`、版本测试、CI workflow、
合并保护配置和 [VERSIONING.md](../../../guides/VERSIONING.md)。

1. 分离当前版本一致性、相对基线前进和发布日有效性三类检查。比较规范化
   `(year, month, day, build)`，不能比较显示字符串或带 Debug suffix 的版本。
2. PR 合并门禁针对候选最终树与最新目标分支 SHA 比较；基线不可由候选代码任意指定。
   明确 checkout/fetch 的来源、缺少历史时的失败行为与检查自身的受信任执行边界。
3. 同一 UTC 发布日必须为基线 build + 1；日期前进时 build = 1；日期倒退、完整规范版本
   不变、非法日期、manifest 不一致必须失败。发布日检查使用受信任 UTC 输入；历史 SHA 的复验使用其记录的
   发布上下文，不因复验发生在另一日而要求重写历史。
4. 每次合并均适用，包括 docs-only；并发 PR 在 `master` 前进后必须更新基线并重验。
   选择要求分支保持最新或等价串行合并保护，不能只靠提交一次绿色 CI。
5. 明确 PR、合并队列、合并后 push 和手动复验的事件规则。合并后审计只作防漏证据，
   不冒充合并前保护；直接 push 默认禁止，例外必须有同等检查与审计。
6. 使用现有更新工具纠正 `26.08.01-1`；具体版本取执行门禁时的 UTC 日期，不在计划中预填。
   版本改动后执行 fresh configure、clean-first build，核对生成头、标题、日志和安装元数据。

验收：同日/跨日正例；未递增、跳号、回退、未来日期、缺失基线、两份版本不一致、两个 PR
竞争、跨日重新发布与历史复验负例/边界。记录保护配置实际生效证据；无仓库权限时将门禁
启用列为阻塞，不能用本地脚本通过代替。显示版本变化不隐式更改 SDK API 或内容版本。

### S6-C：Playback SDK 和 Player 产品化

#### S6-C1：Candidate source、CXC 与 typed lowering

前置：A2 / S6-D01 至 D03。落点为 `engine/chart/`、`engine/cxc/`、
`engine/playback/` 及对应 tests；工具层只复用生产校验，不反向成为 Playback 依赖。

1. 实现拥有 bytes 生命周期的显式 candidate source，记录 encoding、profile/revision、
   entry 和 provider。项目 locator、CXC file/memory 的支持范围按 ADR 0042 固定；
   必须覆盖 ProjectConfig 关联 candidate entry 与 CXC file/memory，不靠扩展名或失败重试猜格式。
2. 将工具独有的 candidate entry 解析/身份校验迁移或提取到允许的内部层，供工具与 Playback
   共同调用；保留既有诊断。CXC 默认 v4 路由不变，多 entry 的选择与歧义拒绝由显式 metadata
   控制；candidate 失败不能自动读取同包 v4，回退必须是新的显式用户选择。
3. 只经 `packed::decode` 取得有效 typed 语义；`inspect` 不能代替 profile/identity 校验。
   复用已验证解码结果，避免“校验一次、prepare 再解码一次”。比较 artifact、compiled semantic
   和 package identity，并验证 provider 资源闭包。
4. 新增 typed lowering 到现有唯一 Runtime 路径；定义 generated identity 到执行 ID 的稳定
   映射及反向关联、父图、相机、TimingMap 和 alpha 转换，不经 v4 JSON 中转。
   requirements 保留完整内部 typed 数据和身份供 Stage 7A 接手，不新增公共 Judgement API，
   不伪装为已判定 Tap，也不静默删除。
5. 将 candidate semantic 与实际资源内容组合为 prepared content identity；Session 配置
   单独规范化，并在应用层形成 execution identity。不把 Packed header hash 直接当作完整
   执行身份，不将本机设备/窗口混入内容身份，不修改既有 v4 identity 算法。
6. 落实 A16 的 feature/闭包处置，增加非测试专用的正例路径。参数变化只在离线显式重编译
   新 Packed；candidate Playback 不执行 CXT v2 展开，v4/CXT v1 与参数 prepare 保持原行为。
7. 将 decode、lowering、资源准备和 snapshot 分配纳入整体预算测量；16 MiB wire 限制不等于
   16 MiB 进程内存。40k 代表指定 profile 的回归规模，不承诺任意内容都能装入该预算。

验收：file/memory 与项目/CXC 相同内容的语义及帧结果一致；generated ID 稳定且不碰撞；
改资源 bytes 会改变相应 prepared 身份；非法 kind/revision/section、篡改 hash/CRC、重复身份、
父环、缺资源、预算边界、零限制和 stale candidate 稳定失败。失败不发布部分 typed chart，
不改变 active Session。v4 与旧 FrameDigest golden 全部保持。

#### S6-C2：配置快照、持久化与音频设备

前置：A2 / S6-D05。配置与命令层放在内部 `cuexis_player_support`，不进入 Playback 全局配置。
结构化解析复用 `json_support`，不向其它模块暴露 JSON DOM。

1. 按 ADR 0024 列出最小实际消费字段：窗口请求、音量、已实现 profile ID、诊断与 launch
   选项。每项指定唯一默认值、来源优先/约束关系、范围、静态或动态分类及失败策略。
   不新增无消费者的 Device/Input/Calibration profile 字段。
2. 按平台规范定位用户目录，实现版本化 UserPreferences 与最小 AudioDeviceProfile，
   覆盖未知字段、未来版本、迁移、同目录临时文件与原子替换。不得覆盖无法理解的未来版本
   原件；损坏偏好只在诊断后使用单一代码默认值。
3. 先读取和校验项目/偏好，收集创建前事实，发布不可变 ResolvedAppConfig；子系统创建后
   收集 EffectiveSettings 并复核。再创建/prepare 活动 Session，派生 ResolvedSessionConfig。
   必要的 SDL 枚举初始化与打开输出设备分开；不能把创建后才知道的能力伪装成 preflight。
4. ResolvedSessionConfig 只包含实际影响本次执行的字段，使用有版本的规范编码和 identity；
   本机路径、日志位置、窗口位置等无关偏好不污染执行身份。配置来源日志不得泄漏多余本机数据。
5. AudioDeviceProfile 唯一拥有设备匹配与输出校准，UserPreferences 只保存 profile ID。
   按 ADR 0042 实施 driver/name 精确 selector 与命名默认路由，不宣称硬件序列号认证；
   不得持久化进程设备句柄或列表序号。匹配不到或存在歧义必须失败，不静默选第一台设备。
6. 输出校准的单位、符号、范围、应用位置和反向 Seek 换算按 ADR 0042，不与 Chart offset
   或估算输出延迟重复叠加。设备热拔插、默认路由变化和格式变化按合同进入明确状态。
7. 动态修改通过显式 apply；静态修改通过受控 Session/backend 重建。文件保存与运行时生效
   是不同事务，分别记录结果；失败不得把 requested 值误记为 effective。

验收：缺失/损坏/未来版本、迁移失败、只读目录、写入/替换失败和多进程冲突策略；
相同配置规范身份一致，活动 Session 不回读后续文件修改；显式 profile 缺失/设备不匹配、
匹配歧义与热拔插失败。使用 fake device 覆盖自动测试，真实设备提供单列 smoke 证据。

#### S6-C3：Player 控制与跨子系统事务

前置：C1、C2、D2。将当前集中式 `run` 按实际职责拆为配置组合、命令/状态控制、后端装配
和诊断；不借机进行无关重构。

1. 建立统一命令入口和状态转换表，覆盖 Empty/Loaded/Playing/Paused/Stopped/Failed、
   loading/reloading 期间的允许操作及重复命令策略；应用状态不强迫公共 Session 枚举同步扩张。
2. 提供正式可操作的加载、播放、暂停、停止、Seek 和 Reload 用户入口；通过窗口输入或其它
   A2 接受的应用控制面进入同一命令层，不再以 smoke-test 分支作为唯一实现。
3. 定义负时间、超出可播放范围、暂停 Seek、停止后播放、EOF、无音轨、加载空内容以及切换
   有/无音轨的行为。Reload 不换 mode，切换通过显式 Open/Rebuild。
   ChartClock、HostClock、CuexisAudio 共用 timeline/discontinuity 规则。
4. 为 reload/重建建立事务协调：内容 prepare、渲染资源 candidate、音频 replacement 完成后
   才能进入 commit；先预检 token，再激活音频，最后执行无失败 Playback/GPU 状态交换。
   软件准备失败丢弃候选，旧内容/资源不变，旧播放允许自然前进；物理设备激活或提交后
   present 错误按 ADR 0042 进入明确故障状态，不能虚称所有硬件操作都可回滚。
5. 把诊断内容和正式内容组合到同一帧提交；Player 通过 PlaybackSession 获取内容与
   portable snapshot，应用层持有 Timeline、renderer 与设备生命周期。不要把渲染器所有权
   倒灌到 PlaybackSession，也不要创建 Player 私有 Runtime。

验收：v4 与 candidate 共用命令表；load/reload/Seek 各允许状态均有成功与拒绝测试；
按阶段注入 decoder/resource/shader/audio/renderer/commit 失败并验证旧 active 状态、
identity、时间、GPU cache 与音频句柄。覆盖连续 reload、stale token、暂停后恢复、
discontinuity、窗口缩放/最小化和关闭顺序；一般状态机测试不依赖 GPU。

#### S6-C4：SDK、真实宿主与安装升级

前置：C3、E3。保持 PlaybackSession 的 owner-thread、Result、对象寿命与 matching-toolchain
约束，变更说明与弃用窗口按 accepted ADR 执行，不承诺稳定 C ABI。

1. 完成 S6-D08 指定的具名宿主适配：使用安装公共头和 package target，宿主自行提供主循环、
   ContentProvider、时间与帧消费，不使用源码树私有 include，也不链接 Player 配置实现。
   不把宿主 SDK 引入 Playback 核心。
2. production 宿主验证 v4；candidate 路径使用明确隔离的实验构建/产物验证。
   默认发行 consumer 必须证明 candidate 未被意外启用；需要实验公共入口时按已接受的
   独立边界和版本规则执行，不将内部 CanonicalSemanticChart 直接导出。
3. 分别交付 SDK 安装树与可运行 Player 分发目录：清晰组件、版本元数据、许可证、
   必需运行库、默认资源定位和可复现打包命令。明确 Debug/Release 与 static/shared
   安装前缀不可混装，不依赖开发机 PATH 或源码 assets 目录。
4. 在 clean staging 中演示构建、启动、加载、Seek、成功/失败 reload 和销毁；验证符号、
   linkage、运行库部署、缺组件、错误 SDK minor/工具链/配置拒绝。
5. 增加从 `0.7.0` 基线重新构建宿主的升级示例与破坏性变更说明；验证配置迁移不损坏旧文件，
   明确回退边界。升级不等于无需重编译地替换 shared binary。

验收：具名宿主运行记录、安装树外编译和部署日志；static/shared、Debug/Release 与承诺平台
矩阵；公共头 ASCII/泄漏检查；production 与 candidate 产物区分清楚。编译一个测试 consumer
不能单独替代真实宿主和完整 Player 分发验收。

### S6-D：后端中立表现渲染边界

#### S6-D1：无环接口与可测试事务

前置：A2 / S6-D04。在新内部 `cuexis_presentation_renderer` 建立 `IPresentationRenderer`，
位于 Playback 与底层 render 之上；类型/target 方向按 ADR 0042，不增加安装公共 component。

1. 解决当前 Playback/Runtime 已依赖 render、而新接口消费 Playback 值的问题。
   分别检查 include 图、链接图、安装图与 shared 符号；前置声明只能解决真实的不完整类型
   需求，不能掩盖实现层循环或绕过 allowlist。共用 draw builder 归新层，底层 render 不搬
   Playback 公共类型，也不反向依赖新层。
2. 接口覆盖 capability、候选 prepare、activate/discard、统一帧提交、present、重建和关闭。
   定义线程、borrow/owning 数据寿命、唯一 outstanding candidate、generation 和 stale token
   行为；合法候选激活的无失败保证不得在迁移中无意丢失。
3. 分离窗口表面和 renderer 的所有权/销毁顺序。后端工厂可在应用装配层选择 adapter，
   正式帧循环只看中立接口；SDL/GL 类型不进入该接口或 Playback API。
4. 明确设备/表面丢失、零尺寸、resize、present 失败的可恢复与不可恢复分类；
   后端重建是否保留 CPU 资源、如何重新上传和失效旧 token 均须可测试。
5. 将确属可移植的 draw command/summary 放到批准的中立层，保留排序、深度、混合、
   Debug pass 与摘要规则；像素探针和 API handle 保留 adapter 专用。

验收：无 GPU 的测试 renderer 实现同一接口，覆盖成功帧、prepare 失败、discard、
旧 token、重建和 present 失败；headless/validation consumer 通过且不引入 SDL/GL。
本阶段不安装该内部接口；真实安装宿主直接消费已有公开 snapshot/resource 合同。

#### S6-D2：OpenGL 迁移与 Player 接线

前置：D1。将 OpenGL adapter 接到同一候选事务与帧合同，Player 正式路径不再直接调用
`OpenGlBackend::renderPresentationFrame`。后端特定创建仅留装配层，像素探针留 smoke/测试层。

收敛 legacy RenderScene/DebugLine 为可选诊断输入，避免第三条内容渲染路径；保留
SDK `0.7.0` 所需兼容入口，不未经弃用决策删除。准备失败保留 active cache，重建成功
后 token/资源代际正确更新，关闭顺序不得使 context 早于 GPU 资源释放。

验收：v4 portable fixture 在迁移前后 draw summary/帧结果保持约定一致；debug omitted、
empty、populated 三态、透明排序、Shader cache 失败与 stale candidate 回归；真实 GPU
验证像素、resize、最小化/恢复、reload、关闭。设备丢失无法物理复现的路径使用故障注入，
报告清楚区分注入证据与真实设备证据。Vulkan 仍 deferred。

### S6-E：常用图片和音频支持

前置：A2 / S6-D06。本阶段实施离线导入，Playback 继续消费 canonical portable texture 与
既有 WAV/PCM。运行时直解码不是默认扩展；若要新增，必须重新评审依赖、预算、安全与
发行边界，不得以方便 Player 打开文件为由临时接入。

共通要求：媒体 importer 可复用工具骨架，但不能因现有 asset_importer 受 Shader target
控制而要求图片/音频导入强制启用 shader-tools。新增独立 media-tools feature、内部
`cuexis_media_import` 与 `cuexis_media_importer`；依赖选型及预算按 ADR 0042。
实际增加依赖时同步修改 `vcpkg.json`、
必要的 baseline、[依赖记录](../../../guides/DEPENDENCY_POLICY.md) 和
[许可证清单](../../../../THIRD_PARTY_NOTICES.md)；第三方类型不进入公共头，decoder 不成为
Playback 的意外传递依赖。

#### S6-E1：JPEG / PNG 到 portable texture

1. 实现有界 source 读取、明确格式识别与 profile 校验，输出 `CXPRES01` RGBA8。
   冻结 sRGB/linear、ICC/gamma、EXIF 方向、alpha/预乘、行序、通道和元数据处理；
   不支持的变体必须明确拒绝，不静默近似转换。
2. 在分配前检查尺寸乘积，在解码期间执行输出、scratch/峰值和累计资源预算。
   profile 必须给出 encoded bytes、宽高、像素数、decoded bytes 与峰值限制。
3. 固定转换算法和 decoder 配置；同一输入与 profile 产生相同 canonical bytes。
   如某平台实现无法满足逐字节一致，先阻塞并复核算法/构建选项；不得私自按平台拆分
   profile，也不能将容差图像比较当作 canonical identity 一致。

验收：JPEG/PNG 各有真实 fixture；方向、透明、颜色元数据、极限尺寸、截断、伪造尺寸、
损坏 chunk/marker 与超预算负例；重复导入和跨承诺平台 canonical golden；Player 实际显示。

#### S6-E2：MP3 / Ogg Vorbis / FLAC 到音频产物

1. 按 ADR 0042 输出 S16LE WAV，保留采样率与 mono/stereo，不重采样、混音或加 dither；
   delay/padding、granule 截尾、链式 Ogg 拒绝与异常浮点处置在 A2 Spec/golden 中落盘。
2. 给出 encoded bytes、采样率/声道范围、帧数/时长、decoded bytes 和工作内存硬限制，
   使用 checked arithmetic 并在解码循环提前停止，不能完全解压后才检查预算。
3. 不因 codec 浮点实现差异放宽资源身份；canonical 输出的确定性范围覆盖承诺平台，
   decoder build/options 与转换算法版本进入 profile/cache 身份，golden 更新需说明原因。

验收：三种格式各有完整导入/播放正例；截断、损坏头、伪造时长、越界帧数、超预算、
不支持声道/采样率等负例；音频长度和起止样本符合已冻结规则；同输入/profile 输出一致。

#### S6-E3：资源身份、缓存与包原子发布

前置：E1、E2、C1。

1. 分离原始输入身份、importer/profile 身份、canonical 产物身份与实际资源 AssetId；
   明确原始资源保留位置和 provenance，不把调试来源记录强行加入运行时资源闭包。
2. 缓存键包含影响结果的全部输入。命中时校验记录与输出身份，损坏缓存稳定拒绝或按
   显式离线重建策略处理，不能由 Playback 静默再导入。
3. 先在 staging 生成并验证全部资源、索引与包，再以一个明确发布点切换可见产物。
   多个文件各自 rename 不等于项目级原子事务；按 ADR 0042 使用不可变 generation 目录
   或已验证的单包原子替换，进程间发布锁拒绝并发 writer，明确失败清理与重启恢复。
4. 使用同一批导入产物建立 v4 与 candidate CXC 闭包，验证包内所有必需资源和 identity；
   失败不覆盖上一有效包，也不改变活动 Playback。

验收：缺失原始资源、旧 profile、坏缓存、磁盘满/权限/替换失败和并发发布；
完整产物可由 clean staging Player 与宿主消费。shader-tools OFF 下媒体导入可构建，
developer-tools OFF 下 Playback 安装消费者不依赖媒体解码库。

### S6-F：关闭、hosted 验证和交接

#### S6-F1：最终验证

前置：B1、C4 和所有上游批次已退出。固定最终实现 SHA，运行 §3 的完整矩阵：
Debug/Release、static/shared、headless/adapter、architecture、installed consumer、
失败回滚、媒体 golden、文档与许可证。最终构建使用 fresh configure 与 clean-first，
不能用未重建的旧生成头验证新版本。

同一候选 SHA 必须取得 Linux Quality、Windows MSVC、Windows MinGW hosted 证据。
Linux sanitizer/coverage 必须实际编译和运行新增的 candidate 校验、lowering、配置及媒体
目标；不能以 developer-tools OFF 导致测试未注册却总体绿色作为覆盖。GUI/GPU/真实音频
设备 smoke 单列执行环境和证据，不把 headless success 等同图形/音频通过。

实现、构建输入、依赖或测试发生变化后重跑受影响门禁，并在新的最终 SHA 完成要求矩阵。
报告提交与实现 SHA 不同，必须列出差异并完成对应复验；影响检查器/CI 的改动不能简单归为
纯叙述文档。容量记录测量整个 prepare 峰值和指定数据集，不只记录 Packed 文件大小。

#### S6-F2：关闭与 Stage 7A / Stage 8 交接

1. 形成阶段报告，按 §3 的验收 ID 链接实现、测试、SHA 与证据，列出未执行/阻塞/残余。
   版本、renderer、媒体三个问题各自有处置证据后，才更新
   [阶段核验记录](../../../stage_reports/reviews/stage-verification-2026-09/2026-09-01-findings.md)
   为 closed；历史现象不改写，Chart v1-v3 退出提案保持独立。
2. Stage 7A 接收 typed requirements、实体身份、时间/discontinuity、执行配置身份及拒绝边界；
   Stage 8 接收 candidate 入口/隔离策略、source/compiled/prepared 身份、A16 处置、
   profile/预算/媒体 provenance 和发行剩余门禁，不把实验入口视为正式 v5 发行。
3. 更新 CURRENT_STATUS、ROADMAP、API/构建/版本/依赖与安装升级文档。未来 Studio 只继承
   单一 PlaybackSession 路径约束，本阶段不要求实现 Studio Preview。
4. 记录 owner 对交接清单和残余的明确接受后才关闭/归档 Stage 6。PR、合并或发布属于另行
   授权动作；测试全绿不自动授权发布，也不自动关闭 Stage 7A/8。

## 3. 验收标准

### 3.1 阶段退出矩阵

以下为最低门禁，不能替代各子批次的细化负例。A1 将每项映射到真实测试与命令，后续报告
记录同一 ID；没有实现、没有注册或没有执行都不能写为通过。

| ID | 必须证明的结果 | 主要批次 | 必需证据 |
| --- | --- | --- | --- |
| S6-G01 | 基线与八项冻结决策完成合同落盘/表征 | A1/A2 | 基线报告、ADR 0042、Schema/API 草案、独立 golden、无环依赖/安装图 |
| S6-G02 | 显示版本相对受信任合并基线正确前进 | B1 | 正负例、竞争/跨日用例、保护检查与安装版本一致 |
| S6-G03 | ProjectConfig 与 CXC file/memory 显式 candidate 消费，v4 仍可显式选择 | C1 | 同内容跨入口帧/身份一致，默认 production 不隐式启用 candidate |
| S6-G04 | 语义入口、profile/revision/预算和身份不可绕过 | C1 | 独立非法 bytes、CRC/hash 篡改、不支持要求/section、失败无部分发布 |
| S6-G05 | generated identity、父图、typed requirements 与 prepared identity 保真 | C1 | 重排/碰撞/资源变化用例、完整字段比较、A16 处置 |
| S6-G06 | 配置有唯一来源，不污染领域数据和活动 Session | C2 | 规范身份、坏文件/未来版本/迁移/写入失败、文件变化不影响活动会话 |
| S6-G07 | 显式音频设备与输出校准按合同执行 | C2 | fake device、真实设备 smoke、缺失/歧义/热拔插、无静默换设备 |
| S6-G08 | v4/v5 共用加载/播放/暂停/停止/Seek/Reload 状态机 | C3 | 命令转换表、时钟/discontinuity、用户入口与全阶段故障注入 |
| S6-G09 | 中立 renderer 覆盖候选事务和统一帧，Player 正式循环不依赖具体 adapter | D1/D2 | 测试 renderer、headless 验证、OpenGL summary/pixel/GPU smoke |
| S6-G10 | JPEG/PNG 输出可复现且有界 | E1 | 真实 fixture、跨平台 canonical golden、损坏/方向/颜色/预算负例 |
| S6-G11 | MP3/Ogg Vorbis/FLAC 输出可复现且有界 | E2 | 三格式 golden、长度/起止样本、解压过程预算及失败负例 |
| S6-G12 | 媒体 provenance、缓存、资源 closure 和发布事务完整 | E3 | 缓存损坏/失效、磁盘/替换/并发失败、旧包和活动会话保留 |
| S6-G13 | SDK 与 Player 可独立安装部署，真实宿主使用公开边界 | C4 | 具名宿主、clean staging、static/shared、Debug/Release、升级/拒绝门禁 |
| S6-G14 | 兼容、架构、许可证与候选隔离没有回归 | F1 | v1-v4/FrameDigest golden、头泄漏/ASCII、tools OFF 和 shader-tools OFF 验证 |
| S6-G15 | 最终代码拥有完整同 SHA 验证与 owner 交接 | F1/F2 | 三平台 hosted、GPU/设备补充、问题 1/2/3 处置与接受记录 |

### 3.2 测试层级与执行要求

- 单元层：版本比较、配置解析/组合、设备匹配、typed lowering、身份、媒体转换和命令状态机。
  测试不要求窗口/GPU/真实音频设备，错误码与字段路径稳定。
- 集成层：真实 CXC、ContentProvider、资源准备、渲染/音频候选事务、包发布及 reload；
  失败前后比较 active generation、identity、时间、资源句柄与输出文件，而不只检查返回错误。
- 包层：安装公共头、头文件 ASCII、符号、依赖 allowlist、错误 minor/工具链/配置拒绝，
  production/candidate 隔离，以及无工具、无 SDL/OpenGL 的 Playback 构建。
- 图形/设备层：OpenGL 像素与 summary、窗口变化、音轨与输出设备、真实宿主主循环；
  运行时记录后端/驱动/设备、命令和 SHA，结束后关闭进程，不留下后台 Player。
- 确定性/容量层：沿用 Foundation 指定数据集并增加端到端 prepare 测量，记录 wire、
  decoded、CPU/GPU 准备阶段、峰值口径和预算拒绝。父图 O(n²) 观测项保留；只有测量表明
  本阶段门禁受阻时才进行有界优化，不借此启动无关 World/Animation 重构。
- 最终回归层：本地 Debug/Release 与可用 static/shared/headless 组合，加同 SHA Linux
  Quality、Windows MSVC、Windows MinGW；sanitizer/coverage/clang-tidy 在支持的 hosted
  工具链运行，不以 Windows/MSVC 的不支持配置伪失败替代结果。

标准本地门禁：

```powershell
cmake --preset debug --fresh
cmake --build --preset debug
ctest --preset debug --no-tests=error
cmake --preset release --fresh
cmake --build --preset release --clean-first
ctest --preset release --no-tests=error
cmake --build --preset debug --target cuexis_format_check
python -B tools/update_version.py --check
python -B tools/check_docs.py
git diff --check
```

版本变化后的 Debug 也需 `--clean-first`。版本前进门禁、子批次 focused 测试、宿主与
GPU/设备命令在实现后写入构建指南和报告；本计划不虚构尚不存在的命令或 target。
文档-only 修改按仓库规则只运行文档及差异检查，不因此宣称实现门禁通过。

## 4. 明确不包含

- Studio 编辑器或 Studio Preview 实现。未来 Studio 必须使用 PlaybackSession 是架构约束，
  不是本阶段需要交付的应用。
- InputProfile、CalibrationProfile、Judgement/Replay 或稳定 C ABI。
- Player 私有 Runtime 路径或宿主专用依赖进入 Playback 核心。
- Vulkan adapter 实现、Vulkan API 类型或为未来 Vulkan 建立未验证的公共占位接口。
- 未经另行决策的运行时媒体直解码、任意格式猜测、无限制解压或第三方类型进入公共 API。
- Chart v1-v3 Reader/Writer/迁移的退出；该项仍按 ADR 0041 和独立 owner 决策处理。
- 完整 Chart v5/CXT v2 正式发行、默认 Writer 切换、v4 -> v5 迁移和 CXC v1 Packed playback
  entry 的最终发布门禁；这些属于 Stage 8。
- Hold/Release、Slide/Flick、多指和未登记 Behavior/Animation/Effect，或为 generated entity
  新增动画绑定。扩大 subset 必须单独取得 owner 范围授权、candidate revision、typed
  字段、capability 与 golden，不能借 S6-A 的设计工作绕过 R5 禁止清单。
- 运行时脚本和逐帧 script callback 无限期延后；不预留字段、extension、capability、
  bytecode、ABI 或 Playback hook。离线 authoring generator 仍是单独的未来工具议题。

## 5. 证据维护与实施交接

- 每个子批次报告记录起始/最终 SHA、实际命令、测试注册数量、结果、环境阻塞、公开边界
  变化和下一批次允许/禁止消费清单。报告存于 `docs/stage_reports/stages/stage-06/`，
  首次创建时补该集合 README 及上级索引，不预先创建空完成报告。
- 子批次退出需实现、正负例、兼容证据、文档和依赖记录齐备；缺硬件或仓库保护权限时
  明确保留未完成项，不能用“计划已写明”代替交付。
- 新功能文档只在实现及验证后更新 API reference 的“已支持”描述；设计期内容留在本计划、
  candidate Spec 或 ADR 0042 等标明尚未实现的决策中。历史报告不重写为新的测试结果。
- 阶段状态由 CURRENT_STATUS 单独汇总；本看板只记录批次。阶段关闭须同步状态、路线图、
  索引和接手计划，但不能因 Stage 6 文档细化将三个工程问题提前改为 closed。
