# Stage 7-RR 架构设计附页：Player 实时执行与普通播放收敛

状态：active
更新日期：2026-11-19

<!-- cuexis-meta
id: PLAN-stage-07-rr.architecture-design
status: active
authority: none
-->

本文是现有 active plan 的讨论附页，不是新 plan、报告或 ADR，也不是现行 API 合同。
它整理已确认方向、代码现状、目标约束和仍待决定的问题；格式与公共接口语义仍以现行合同为准。

## 1. 范围与读法

本文聚焦 Player 普通播放与实时游玩判定如何收敛到长期执行线。代码现状、目标方向与候选事项分开标记；
候选入口或格式不因被提及而视为已进入默认产品路径。

Stage 7-RR 的任务边界仍见[计划 §4](plan.md#4-已确认方向与决策台账)，已登记发现见[findings](findings.md)。

## 2. Owner 已确认方向

- 长期由一条 Player 执行线承接实时游玩判定；普通播放不开启判定，实时游玩保留判定。
- 合并后的 Player 仅在启动时显式传入请求判定的参数时才可启用实时判定；未带判定请求参数时只按普通播放处理，即便内容可玩也不自动开启。判定请求仍须经过校验。此项不指定参数名或值、不要求沿用现有 test-only 参数组，也不裁定交互或错误策略；不扩大 D-05，不更改公共 CLI 合同或生产代码。
- D-08 已确认采用统一会话控制权方向；与 D-06/D-07 兼容，不调整 D-05，也不批准实施。
- D-09（2026-11-19，owner）：承接 D-08，确认每会话一个执行线程串行推进播放与可选实时判定，并确认 §6 所列 Pause/Resume 控制目标与验收场景。UI、窗口、渲染、I/O 与音频设备不要求共用该线程，仍遵守既有 owner；未来物理部署与迁移顺序未裁定。`judgement` 不反向控制 Player。以上是目标与验收约束，不表示已实现或授权实施。
- D-10（2026-11-19，owner）：将可复用会话控制与 Player 外壳分开，由 Player 与测试共用同一实现；未来合并须通过构建约束和行为测试。详细验收目标及明确不涉及的边界见 §6；未表示已实现或已验证，不改变 D-05 或授权实施。
- 2026-11-19 owner 确认 D-11（`ExecutionHost` 控制权迁移与退役顺序）、D-12（设备中断后的安全续局边界）、D-13（Retry/Reload 与失败边界）、D-14（纯渲染资源重建及画面恢复后的播放规则）和 D-15（主动 Stop 与自然结束的统一完成处理）；具体目标见 §6。五项均不改 D-05，也不表示已实现或运行验证。
- `judgement` 只依赖 `core`，保持判定语义与应用、音频、渲染适配器解耦。
- 接受共享播放行为的改动会影响两种用途；变更后必须分别核实普通播放与实时游玩。
- 保留三种时间含义：音源位置、谱面时间、判定 tick。Playback 是会话内时间换算的唯一责任方；
  音频报告音源位置，Chart 定义谱面零点与节拍，Judgement 消费 tick。
- 设备校正和谱面 offset 纳入 Playback 的换算链；不以这次收敛为由重写全部时钟。
- 现有普通播放宿主与实验执行宿主的双宿主隔离不是长期目标。
- 本方向不改 D-05：RR-3 的 R1/R5/R6 路线和 R2/R3/R4 deferral 继续有效；本附页不批准实现或排期。

## 3. 路线选择与未选替代

选择一条 Player 执行线，是因为既有产品边界已把 Player 与 Studio 定位为 Playback 的宿主，
且禁止两者各自建立私有 Chart → Runtime 路径（ADR 0027）。代码已有 `PlaybackSession::commit`，
候选执行提交也能在校验宿主证明后复用该入口；正式帧交付已有 `submit` / `present`。
这些接缝可承载收敛方向，同时保留普通播放与实时游玩的模式差异。

以下替代没有选作长期路线，记录其取舍供继续讨论：

- **长期保留独立实验宿主**：短期可保留执行试验的边界；代价是继续维护两套会话控制、时钟和交付协调，
  与 Player 承接实时判定的方向不一致。现有分离可作为过渡事实，不能据此推定最终边界。
- **由实验宿主取代 Player**：可直接沿用实验执行协议；代价是把候选执行入口变成普通产品入口，
  混合普通播放与 test-only 玩法适配的生命周期。长期产品宿主仍是 Player。

以上是路线讨论依据，不是具体 API 决定或实施批准。

## 4. 源码现状、目标与候选

| 标记 | 部分 | 说明 |
| --- | --- | --- |
| 现有源码 | 提交入口 | `commitExecutionCandidate` 校验 fence、Prepared 身份与 ready receipt 后，复用 `session.commit`；普通 `commit` 仍执行自身会话状态校验。 |
| 现有源码 | 渲染交付 | Player 帧循环和 `ExecutionHost` 的正式交付都经过 renderer `submit` / `present`。 |
| 现有源码 | `ExecutionHost` | 承担实时控制、输入与时钟校验、时钟失效/过期处理、帧交付期限与回执等协调；合并时须保住这些语义。 |
| 现有源码 | `judgement` | 静态库仅链接 `core`；实时会话按游玩路径接入判定。 |
| 目标 | Player 执行线 | 普通播放与实时游玩共用会话和执行骨架；只有游玩路径启动 Judgement。 |
| 候选玩法适配 | `PlayerGameplay::step` | 固定 test-only 步长用于候选玩法适配；该路径提交输入并推进 Judgement，不代表普通播放适配或生产时钟方案。 |
| 现有格式边界 | CXC 与旧 Reader | CXC v1 已接入；Chart v1/v2/v3 Reader 与显式迁移按 ADR 0041 保留。 |
| 候选/开放 | 格式入口 | Packed、CXT v2、Chart v5 的候选条目选择及其隔离边界仍待讨论。 |

```text
现状：普通播放 ───────────────┐
      实时执行 / ExecutionHost ─┴─> PlaybackSession ─> renderer submit / present
                                      └─ 实时游玩路径 -> Judgement

候选玩法测试：PlayerGameplay::step（固定 test-only 步长 + 输入）
                                      └─> PlaybackSession -> Judgement

目标：普通播放适配（不启动判定） ─────┐
      实时音频与输入适配 ────────────┴─> Player 执行线 -> PlaybackSession
                                                       ├─> renderer submit / present
                                                       └─> 仅游玩时调用 Judgement
```

图示表达职责方向，不承诺具体类、API、模块边界或宿主退役方案。`ExecutionHost` 的当前边界是实现事实，
不是长期双宿主隔离的设计目标。

## 5. 一次性结构代价与验收方向

一次性代价需要在后续设计中逐项盘清，不在此给出工期或实施批次：

- 归并 Player 的会话生命周期与播放控制接线，同时保留 `PlaybackSession` 的 prepare/commit 校验和失败原子性。
- 将音源位置、设备校正、Chart offset 与 chart time/tick 的映射收口到 Playback，避免各宿主重复换算。
- 将实时输入、时钟过期、交付回执、seek/replay 状态接到共同执行骨架，并保留失败恢复边界。
- 处理旧渲染兼容入口与 `submit` / `present` 正式交付路径的关系。
- 共享播放行为每次改变时，分别验证普通播放和实时游玩。

验收方向至少覆盖：

- 非零 offset 与设备校正同时存在时，source position、chart time 和 judgement tick 的映射一致，校正不重复应用。
- 暂停/恢复不推进暂停期间的判定时间，也不重放已消费输入；seek 不伪造目标之前或未来的输入历史。
- 帧丢失、未交付或交付过期时，不因重试/重放帧重复判定；交付回执与实际 `submit` / `present` 结果一致。
- reload 失败按准备、提交、设备激活及提交后故障分别记录实际状态；不得一概承诺旧内容保活或回滚，
  尤其设备激活失败或提交后的故障不能描述成已回滚。
- 纯播放可正常准备、更新、渲染和 seek，且不创建或启动 Judgement。
- 实时游玩保留输入采集、时钟有效性校验、判定与恢复行为；两种用途都覆盖共享改动。

## 6. 开放问题

- **候选条目与隔离边界**：Packed、CXT v2、Chart v5 各自继续 candidate、提升或归档的选择是什么？
  它们进入 Player 的条件构建、安装面和默认执行路径如何隔离？CXC v1 已接入，不是待决项；
  Chart v1/v2/v3 Reader 与显式 v1/v2/v3→v4 迁移受 ADR 0041 保护，不作为退役候选。
- **渲染兼容入口**：`renderFrame`、`renderPresentationFrame` 等旧入口如何迁移或退役，怎样统一到 `submit` / `present`？
- **宏与公开面**：`CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE` 当前造成的声明面差异如何收敛，并如何处理 ADR 0042 的约束？
- **tickScale**：时间单位、scale 来源及由 Playback 映射到 Judgement tick 的规则尚未决定。
- **ExecutionHost 控制权**：迁移或复用及旧控制权的退役顺序按下文 D-11；不再作为笼统未决项。
- **Studio 预览与实机游玩（未来边界共识）**：Studio 预览是纯播放，不创建实时 `Judgement`，不记录成绩或连击。首版实机从头判定；新谱须重新载入并从头开局。未来可在同一实机路径从指定点开始或跳过中段：被跳过的未决音符按普通 `Miss` 结算并正常计分、影响连击；音符起点早于跳转目标，且音符跨越该目标的未决长按，也直接判 `Miss`；已完成结果不改写。防止谱面失败只作为未来选项，不改变上述 `Miss` 语义。以上是未来边界共识，不是实施批准；Retry/Reload 语义见 D-13，其余 Player 会话生命周期仍有待决项。

### 判定请求的未决细节

启动时的判定请求规则见 §2：未显式请求时按普通播放处理，内容可玩也不自动启用判定；显式请求仍须经过校验。实际参数名和值、是否沿用现有 test-only 参数组、交互方式、校验失败后的回退或错误处理，以及启动后的运行时切换行为仍未裁定。

### 会话控制串行化（D-09：已确认目标与验收场景，非现状或实施授权）

D-09 确认每个会话由一个执行线程串行推进播放与可选实时判定；普通播放不创建或推进 `Judgement`，实时游玩由同一执行线程推进 Playback 与 `Judgement`。`judgement` 不向 Player 反向发起控制。UI、窗口、渲染、I/O 与音频设备不强制共用该线程，相关操作仍遵守各自既有 owner；未来物理线程部署与迁移顺序仍待决定。

以下是目标验收场景，不是已实现行为：

- **用途与串行推进**：普通播放即使内容可玩也不启动判定；显式请求且校验通过的实时游玩才启动判定，Playback 与判定在该会话执行线程上串行推进。共享改动分别检查两种用途。
- **并发 Pause**：运行中的会话同时收到两个 Pause 请求时，共享一次冻结、一个判定边界和同一结果；第二个请求若在操作完成前到达，则等待并共享该结果。各自来源仍单独登记。所有必要参与方确认成功后才可报告成功；任一参与方失败时不得报告成功。
- **重复与独立暂停原因**：Pause、Resume 是明确命令，不是 Toggle。已暂停时再收 Pause，不改变时间、输入或挂起判定；同一来源重复 Pause 不叠加。玩家手动暂停、设备中断等独立持久原因分别解除；仍有另一原因时，解除其中一项不得恢复播放。
- **恢复后的迟到操作**：Resume 后，旧 Pause 操作及旧会话回执均不能重新暂停或改变结果；Resume 后真正发起的新 Pause 请求仍可暂停会话。回执只确认既有操作，不构成新的暂停意图。

### ADR 0046 的候选部署（与 D-09 目标分开）

[ADR 0046「决定」第 1、3 条](../../../adr/0046-realtime-playback-coordination.md)记录了 B 职责边界及初始 P-B 部署：主线程承载 D1 捕获、D5 GL 与 D6 协调，Playback worker 共置 D2 执行和 D3 表现，audio worker 独立。该 ADR 标注当前分支为 candidate 实施、尚未发行；这项候选现状不证明 D-09 已实现，也不裁定 D-09 所留的未来部署与迁移顺序。

现有 [`ExecutionHost::pause`](../../../../app/player/src/execution_host_frame.cpp) 在非 running 或操作冲突时会拒绝请求；这是代码现状，不能据此把上面的幂等、合并与共享结果目标描述为已实现。

### 可复用会话控制与 Player 外壳（D-10：未来验收目标，非现状或实施授权）

D-10 要求将可复用的会话控制实现与 Player 外壳分开；Player 和测试必须使用同一份实现，不设测试专用控制实现。该实现须能脱离窗口、CLI 解析与 SDL 主循环独立编译，`judgement` 仅依赖 `core`。未来合并时，构建约束与以下行为测试都是必跑门槛。

- 测试直接提供内存可玩谱面，驱动 Player 实际使用的同一份会话实现；不创建窗口、不解析 CLI、不模拟按键，并可替代外部设备。
- 普通播放完整经过播放、Pause、Resume，不创建也不推进判定；显式请求且校验通过的游玩使用相同 Pause/Resume 规则，并运行判定。

以上是 D-10 的未来验收目标，尚未实现或运行验证。不扩大 D-05、不新建共享库、不冻结 Studio 公共 API。D-10 当时未裁定 `ExecutionHost` 是否退役或故障恢复；后续共识分别见 D-11、D-12、D-13，不回写扩大 D-10。

以下 D-11 至 D-15 均为未来目标，不是已运行验证，也不改变 D-05。

### `ExecutionHost` 控制权迁移与退役（D-11：2026-11-19 owner 共识）

`ExecutionHost` 可按行为迁移或直接复用；须保留时钟失效/过期、帧交付期限与回执等故障路径的行为验证。新路径验证通过后，才撤销旧控制权；同一局的同一操作始终只有一处决策。旧测试参数与旧控制权分别退役。仅当具名消费者无法同步迁移时，才临时转发到同一实现，迁移完成后删除；没有这类消费者就不建兼容层。现有工具或故障测试可作行为用例，例如 [`player_evidence.py`](../../../../tools/verification/execution_acceptance/player_evidence.py) 与 [`rpa_b5_close_tests.cpp`](../../../../tests/rpa/rpa_b5_close_tests.cpp)。历史证据留在原基线，不改其结论。

### 设备中断后的安全续局（D-12：2026-11-19 owner 共识）

设备只报告事实，由会话控制者裁决；`judgement` 不新增依赖。中断或暂缺确认本身不等于不可恢复；位置或判定边界不明时停止推进，不凭猜测计分。已确认暂停且仍为同一局时，只有音频能对齐冻结位置、已结算输入边界可核实，才允许玩家主动 Resume；独立暂停原因按 D-09 分别解除，设备恢复本身不代替手动 Resume。首版保持停止推进，只有最终仍无法证明安全续局时才提示重开或退出；不自动清局、不补 Miss，也不改变 Studio 主动跳段的意图。长期要求不得漏算、重复计算或在不确定时计分；强制重开不作为永久规则，未来可增强，当前不要求快照或新协议。没有发生率数据，不把“罕见”写作事实；除 D-13 规定的 Retry/Reload 准备失败与提交边界外，装载、提交等其他故障恢复仍开放。

### Player 其他会话生命周期（部分仍待决定）

除 D-09 的 Pause/Resume 控制约束、D-12 的设备中断续局目标、D-13 的 Retry/Reload 目标及 D-15 的结束处理外，Player 结束后的重启、运行时用途切换，以及其他装载/提交故障恢复生命周期仍未决定。以下尚未另行确认的内容仅为设计草案，不是已实现行为；其承接 D-07 的启动请求边界：

- 启动时的显式请求声明当前进程/会话允许的能力上限；未请求时保持普通播放。请求只表示允许按游玩模式装载，不代表所有内容都能进入该模式。
- 一次 `PlaybackSession` 装载并确定用途后，整个会话保持该用途：普通播放不创建 `Judgement`；实时游玩在会话装载时创建并接入 `Judgement`，沿该会话持续运行。内容或配置不支持显式请求的游玩模式时，建议明确失败，不静默降级为普通播放；失败时机、诊断码和交互策略仍待决定。
- 主动 Stop 与自然结束均按 D-15 结束本局并卸载谱面；卸载后要再玩，须先加载谱面。下一局是否复用底层会话对象未定。
- 运行中的会话能否从普通播放切为游玩或反向切换仍未决定；创建新会话或重启后重新装载并校验请求与内容只是待比较的方向。

替代方案及代价：

- **运行时切换用途**：无需重启即可响应用户操作，但必须定义 Judgement 的创建/销毁时点，以及输入历史、判定 tick、暂停状态、播放时钟和切换失败时的原子性；还会与当前游玩模式不接受普通 chart seek 的限制相互影响。
- **按内容自动选择用途**：可减少显式配置，但“内容可玩”不等于用户希望游玩，会改变 §2 已确认的未请求即普通播放规则；还需定义稳定的能力判定条件，避免内容、配置和运行时能力不一致时产生隐式模式变化。

现有接线仅作为讨论依据，不表示上述目标已实现：[`runPause` / `runStop` / `runSeek`](../../../../app/player/src/player_control_transaction.cpp) 在 `gameplay_` 存在时分别转发暂停、停止后 `reset(true)`，并拒绝普通毫秒 chart seek、要求显式 typed Tick；[`PlayerController` 字段](../../../../app/player/src/player_control.hpp)以可选 `gameplay_` 和 `PlayerControlPorts::gameplay` 承载候选玩法配置。[`player_app`](../../../../app/player/src/player_app.cpp) 在配置存在时读取 profile 并接入 ports，而 `executionTestOnly` 仍分流至 [`player_execution`](../../../../app/player/src/player_execution.cpp) 的独立候选执行路径。尤其 `commitBundle` 可在替换 bundle 时重置已有 gameplay 状态；这些候选接线不构成未来会话语义保证，Retry/Reload 目标见 D-13。

### Retry、Reload 与失败（D-13：2026-11-19 owner 已确认的未来设计）

D-13 不改变 D-05，也不是实现或验证声明。当前普通 Player 的窗口 `R` 映射为 `Reload`：重新读取启动配置所确定的当前来源并执行 `RestartAtZero`；没有独立的 Retry 按键。这里仅记录现有按键映射，不声称无谱空会话已有 Reload 命令。启动来源由 `--chart`、`--project`、`--cxc` 之一确定，均未指定时使用默认工程（[选项解析](../../../../app/player/src/player_options.cpp)、[启动默认值与来源端口](../../../../app/player/src/player_app.cpp)）；当前 `R` 接线见[按键绑定](../../../../app/player/src/player_assembly.cpp)、[命令映射](../../../../app/player/src/player_control.cpp)及[来源解析与重载](../../../../app/player/src/player_control_transaction.cpp)。这些是现状，不代表未来 Retry/Reload 目标已实现。内部 `PlayerCommand` 可携带显式 `PlaybackSource`，但不表示窗口提供选谱功能（[PlayerCommand](../../../../app/player/src/player_control.hpp)）。

未来设计区分两个操作：**Retry** 使用当前已加载的谱面版本，不重读当前来源；**Reload** 每次都重新读取同一启动来源，即使来源内容未变化也不跳过读取。Reload 解析、校验来源，仅在需要时准备和解码音频，并检查各步骤的 `Result`；成功后不再分支判断是否自动播放，因为 Reload 成功替换后一律以新谱从零开始、处于暂停且无声，不推进判定，等待显式 Play。Retry 与 Reload 共用新局初始化逻辑；新局不继承旧局的输入历史、成绩、连击或判定进度。Retry 成功时，若设备及其他必要条件已就绪，无论此前是 Playing 还是 Paused，都直接以已加载版本从零开始播放，不需再按 Play；必要条件未就绪时不得启动播放或先行推进判定。普通播放始终不创建或推进 `Judgement`。

- Reload 的任何失败都必须报告实际原因与阶段，并使播放和判定停止：不得续播 A，也不得把无效或失败的 B 呈现为已在零点就绪的新局。停播不等于卸载；若旧谱 A 仍完整可用，保留其已加载版本供显式 Retry，或在修好来源后再次 Reload。若没有可用版本，则保持无谱、停播；这不表示空会话已有 Reload 命令。
- 若故障已影响设备或发生在提交之后，不承诺原子回滚，也不保证旧谱仍可用；仍须报告实际原因与阶段、停止播放和判定，且不得报告成功或把失败的 B 呈现为就绪新局。上述目标不规定收到 Reload 后的具体切换顺序。
- D-13 当时未决定纯渲染资源 rebuild 是否开新局；D-14 首次确认恢复当前已加载版本和本局状态的边界，2026-11-19 续议补充画面不可用原因的解除及普通播放/实时游玩的恢复规则。D-14 仍不覆盖所有 Rebuild/Failed 恢复。D-13 仍不决定运行中用途切换、快捷键或 CLI 入口、设备影响或提交后的具体恢复方式，以及线程排期。

以下场景用于未来验收，不表示已实现：

- **Retry 已加载版本**：分别从 Playing 和 Paused 的 A 发起 Retry，同时把来源改为 B；Retry 不读取来源，必要条件就绪时直接以 A 从零开始播放，不需再按 Play，且不带入旧局进度。必要条件未就绪时不启动播放、不先行推进判定。
- **改谱重载**：正在播放或暂停 A 时，同一来源改为有效的 B；Reload 确实重读、解析并校验来源，且遵守对应 `Result`。成功替换后以 B 从零开始、暂停、无声，不推进判定，直到显式 Play；不继承输入、成绩、连击或判定进度，也不在加载后另行判断是否自动播放。
- **来源未变重载**：正在播放或暂停 A 且来源仍为 A；Reload 仍重读、解析并校验来源，成功后同样以该版本开从零、暂停、无声的新局，不推进判定，直到显式 Play。
- **Reload 失败**：正在播放或暂停 A 时，来源变为无效 B，解析、校验或解码失败；无论失败发生在哪个阶段，均报告实际原因与阶段并停止播放和判定，不续播 A，也不把 B 显示为零点就绪的新局。A 仍完整可用时保留其已加载版本，供显式 Retry 或修好来源后再次 Reload；没有可用版本时维持无谱、停播，不假定空会话已有 Reload 命令。另注入设备已受影响或提交后的故障，验证不承诺原子回滚或 A 仍可用，且不报告成功。

现有 Player 对照证据显示，解码准备失败会保留旧 bundle，且当前实际状态仍为 `Playing`（L299）；设备激活失败会保留旧 bundle 但进入 `Failed`（L319）。L299 的继续播放是当前现状证据，与 D-13 未来“Reload 失败即停播”的设计不同；D-13 尚未实现，不能把该测试描述为已满足未来目标。这些测试只说明资源事务现状，不证明 gameplay 状态可整局回滚：[`player_control_load_tests.cpp`（准备失败）](../../../../tests/player/player_control_load_tests.cpp#L299)、[（激活失败）](../../../../tests/player/player_control_load_tests.cpp#L319)。跨来源读取的具体 UI/CLI 入口仍未决定；D-13 也不改变未来在同一实机路径内从指定点开始或跳段时的 `Miss` 语义。

### 纯渲染资源重建（D-14：2026-11-19 owner 共识，边界确认，非实现或授权）

D-14 只覆盖纯渲染资源重建：重建后的画面应来自当前已加载的谱面版本，并保留本局位置、判定、输入/连击及其他进度；不重读谱面来源，不换谱、清分或重开，也不要求在任何时刻一律冻结或倒带。例：正在游玩已加载版本 A 时，磁盘上的同一来源已改为 B，纯画面重建仍呈现 A 和当前局面；只有显式 Reload 才读取并采纳该来源的新版本。

**2026-11-19 续议：**渲染器只报告带当前会话身份的画面可显示事实，不直接发 Pause/Resume；唯一会话控制者只解除当前会话的“画面不可用”暂停原因，不触碰其他原因。纯画面重建若没有因画面不可用而实际暂停播放，不额外发 Pause/Resume，也不倒带。

普通播放确实因画面中断暂停时，控制者仅在中断前正在播放、恢复时仍为同一会话和同一局，且解除画面不可用原因后没有其他暂停原因，才自动续播。玩家手动暂停（包括中断期间新到的）保持为独立原因，不能被画面恢复回执解除；Reload 成功建立的零点暂停新局继续等待显式 Play。旧会话的迟到画面回执不得改变当前会话状态或带起播放。

实时游玩若因画面中断暂停，须先显示冻结画面，再等待玩家显式 Resume；用于继续的那次按键不得同时进入游玩判定，恢复前播放位置和判定均不推进。若音频也中断，恢复还须满足 D-12 的安全续局条件。首版不设时长阈值、倒计时或配置项。以上续议不扩大 D-05，不改变 API 或线程，也不构成实现授权。

现状对照：[`player_control_transaction.cpp` 的 `runRebuild`](../../../../app/player/src/player_control_transaction.cpp#L156) 比纯画面恢复更广，会重建 renderer、处理 audio seat、读取来源，并调用 `prepareReload(..., KeepChartTime)`。当尚无 bundle 且处于 `Failed` 时，该路径还会转入 `runLoad`。因此 D-14 不适用于所有 `Rebuild` 或 `Failed` 恢复；现有接线也不表示 D-14 已实现。

以下为未来验收方向，不表示已实现或验证：

- **纯画面重建未造成暂停**：A 正在播放时执行纯渲染资源重建，播放没有因画面不可用而暂停；重建不额外发 Pause/Resume、不倒带，位置连续向前，且不重读来源、不换谱、不清分或重开。若来源已改为 B，画面和状态仍对应已加载版本 A；只有显式 Reload 才采纳 B。
- **普通播放自动续播**：A 因画面不可显示而实际暂停；此前正在播放，恢复回执属于当前会话和同一局，且没有其他暂停原因。控制者只解除画面不可用原因并从冻结位置自动续播，不倒带。
- **手动暂停阻止自动续播**：分别覆盖画面中断前已有手动暂停，以及暂停期间新到手动 Pause；恢复时只解除画面不可用原因，手动原因仍在，播放保持暂停，须由玩家显式 Resume。
- **Reload 与旧局迟到回执**：画面中断期间 Reload 成功建立新局，新局从零暂停并等待显式 Play；新局的画面可显示回执不启动播放。随后到达的旧会话回执也不得改变新局或启动播放。
- **实时游玩显式恢复**：实时游玩因画面中断暂停后，先确认冻结画面已显示，再接受玩家显式 Resume；用于 Resume 的按键不命中音符，恢复前播放位置与判定 tick 均不推进。若同时注入音频中断，另验证 D-12 的同局、音频位置及已结算输入边界安全条件。
- **无时间策略配置**：首版验收不设置中断时长阈值、恢复倒计时或相关配置项；纯渲染重建不新增 API 或线程要求，也不改变 D-05 范围。

### 主动 Stop 与自然结束的统一完成处理（D-15：2026-11-19 owner 已确认的未来设计）

以下是 owner 2026-11-19 对 D-15 的决定：属于未来设计，不是当前生产合同、实现或验证声明，不改变 D-05，也不授权实施。

**自然结束。** 普通有音轨播放由播放路径报告本局 EOF，不创建或推进 `judgement`。有音轨实时游玩须分别取得带本局身份的两项事实：播放路径确认音频自然播完；判定执行路径确认依原规则应处理的输入与判定尾段已实际执行完成。唯一会话控制者仅在两项事实都成立后结束本局。EOF 不代替输入/尾段完成：合法输入窗口仍有效，未处理到期判定须实际处理，不能因 EOF 批量判 Miss；队列为空或时刻已到也不证明尾段已执行。尾段先完成则继续播放至 EOF；EOF 先到则保持本局并继续所需输入/尾段处理。候选 RPA 或 test-only 尾段参数不能作为生产默认来制造完成事实。无音轨自然终点仍未裁定，继续由既有唯一会话控制者协调，不在此新增触发条件。

**Stop 截止。** 唯一会话控制者按本局输入顺序确定 Stop 的截止边界。只结算截止前已接收、且按既有规则应在截止前生效或到期的输入/判定，包括已接收但调度较晚、规则生效时间仍在截止前的命中；截止后才接收的输入不计。不得用 UI 时间、末帧分数或空队列代替该顺序与结算结果。Stop 不等曲终；截止时尚未到达的音符不补 Miss，尚未到期的长按尾不预判。结算遇到故障时保留并报告故障证据，不把未证实结果伪装为完成。

**成绩与卸载。** 只装载了谱面，或成功 Reload 后还没开始游玩，Stop 只卸载，不交付成绩。已经开始的游玩局即使中途暂停、分数为零，也按实际成绩结算。普通播放始终没有判定成绩。已有游玩局的 Stop 和自然结束共用同一流程：先确认最终成绩，再将卸载后仍可使用的结果交给宿主，最后卸载谱面。交付不要求写文件，也不规定展示或持久化方式。

每局只执行一次结束处理。Stop 和曲终同时到达不重复结算或交付；旧局的迟到通知不能结束新局。如果成绩已经交付而卸载失败，只处理余下的清理工作，不重算也不再次交付。

故障暂缓与 Reload 失败不进入此完成卸载流程，分别遵循 D-12 与 D-13。卸载后已无本局已加载谱面可供 D-13 Retry 使用，也不隐式重读来源。`judgement` 不感知音频、Player 或卸载；Player 不重算判定窗口。D-15 不新增结束管理器、API、固定等待或谱面字段。

候选规则参考：[RPA 执行合同 §7](../../../formats/rpa-execution-contract.md#7-需要测量的后续门禁)描述候选 profile 的 EOF 尾段路径及生产测量门槛；该合同仍为 candidate，不代表通用生产语义或已实现，也不裁定无音轨自然终点。

现状对照：普通音频 `Ended` 只使应用进入 `Stopped`（[会话测试](../../../../tests/player/player_control_session_tests.cpp#L141)）。候选玩法的 [`runStop`](../../../../app/player/src/player_control_transaction.cpp#L262) 调用 `GameplayControl::Stop` 后会重置适配器；该控制本身还会[替换判定会话](../../../../engine/playback/src/playback_session_realtime.cpp#L481)。因此不能先走旧 Stop 再查询本局成绩。[候选结果对象](../../../../engine/playback/include/cuexis/playback/gameplay_candidate.hpp#L230)可以独立持有取出的结果，但目前尚无 D-15 的统一结算、交付与卸载流程。

未来验收方向（不表示已实现或验证）：

- **自然结束顺序**：用可分别控制 EOF 与判定完成时点的有音轨实时局，分别先发 EOF、先完成尾段；前者在输入窗口及应执行尾段完成前不得结束或批量 Miss，后者须播放至 EOF。额外让队列为空、时钟到达尾段时刻但尾段尚未实际执行，确认仍不结束；确认生产路径不依赖 test-only 尾段默认值。普通有音轨播放自然结束后卸载，且始终无 `judgement`。
- **Stop 截止与成绩**：安排一项截止前已接收、有效时间在截止前但调度较晚的命中，一项截止后输入、一个未到达音符和一个未到期长按尾；让 UI 时间与会话顺序冲突、末帧分数滞后且队列暂空后发 Stop。核对只按会话顺序结算应在界内生效/到期的项目，后到输入被排除，未到达音符和未到期长按尾不补 Miss，Stop 不等 EOF；注入结算故障时仍能观察到原故障证据。
- **开局与交付**：成功 Reload 后未 Play 即 Stop，断言只卸载且无成绩交付；实际开始游玩后暂停并以零分 Stop，断言交付真实零分，且卸载后调用方仍可读取结果。普通播放全过程不创建/推进 `Judgement`。
- **竞合与清理**：让 Stop 与自然完成及重复通知竞合，断言只收束、交付一次；新局开始后再送旧局 EOF/Stop 通知，断言新局不结束。结果交付后注入卸载失败并重试清理，断言不重算、不重复交付。边界审查确认无音轨终点仍未被预设，且没有新增结束管理器/API 或 `judgement` 对音频、Player 的依赖。

### 其他音游的对照（调研，不是决定）

- **Friday Night Funkin'（FNF）**：固定提交 [`94ee0f2a9386d80e445be39ee2ed4b4a630cfbac` 的 Chart Editor](https://github.com/FunkinCrew/Funkin/blob/94ee0f2a9386d80e445be39ee2ed4b4a630cfbac/source/funkin/ui/debug/charting/ChartEditorState.hx#L6547-L6611) 以当前编辑数据构造试玩曲；默认从零开始，也可用编辑器滚动位置与 playhead 计算起点，并可传入 Bot Play。该试玩曲的 [`Song.buildRaw` 默认 `validScore = false`](https://github.com/FunkinCrew/Funkin/blob/94ee0f2a9386d80e445be39ee2ed4b4a630cfbac/source/funkin/play/song/Song.hx#L196-L200)；[结算仅在 `currentSong.validScore` 为真时写入最高分](https://github.com/FunkinCrew/Funkin/blob/94ee0f2a9386d80e445be39ee2ed4b4a630cfbac/source/funkin/play/PlayState.hx#L3472-L3508)。同提交的 [F5 `reloadAssets`](https://github.com/FunkinCrew/Funkin/blob/94ee0f2a9386d80e445be39ee2ed4b4a630cfbac/source/funkin/play/PlayState.hx#L1816-L1838) 标明仅 debug 构建触发：清理状态、强制重载资源、按当前歌曲 ID 与 variation 重新取谱，再用 `lastParams` 加载 `PlayState`。
- **osu!lazer**：同一 `Player` 的 [Pause/Resume](https://github.com/ppy/osu/blob/fe5257f670cd46b3ff71ad836ce4f00f5543826a/osu.Game/Screens/Play/Player.cs#L1072-L1108) 停止并恢复该局的 gameplay clock；[Quick Restart](https://github.com/ppy/osu/blob/fe5257f670cd46b3ff71ad836ce4f00f5543826a/osu.Game/Screens/Play/PlayerLoader.cs#L550-L558) 会创建并设为 `CurrentPlayer` 的新 `Player`，重开路径也调用 `prepareNewPlayer()`（[L584-L613](https://github.com/ppy/osu/blob/fe5257f670cd46b3ff71ad836ce4f00f5543826a/osu.Game/Screens/Play/PlayerLoader.cs#L584-L613)）；新 `Player` 初始化自己的 [`ScoreProcessor` 与 `Score`](https://github.com/ppy/osu/blob/fe5257f670cd46b3ff71ad836ce4f00f5543826a/osu.Game/Screens/Play/Player.cs#L262-L281)。这些代码不能证明 Retry 会重新读盘。
- **Clone Hero**：[官方手册](https://wiki.clonehero.net/books/clone-hero-manual/page/how-to-play)说明 Quickplay 完曲保存分数和星数，Practice 可选一个或多个段落并调速；[命令行预览说明](https://wiki.clonehero.net/books/clone-hero-manual/page/chart-previews-via-the-command-line)支持显式指定 bot 预览谱面。
- **StepMania v5.1.0-b2**：[编辑器菜单](https://github.com/stepmania/stepmania/blob/v5.1.0-b2/src/ScreenEdit.cpp#L793-L807)提供 Play selection 与 Revert from disk；[Play selection](https://github.com/stepmania/stepmania/blob/v5.1.0-b2/src/ScreenEdit.cpp#L4797-L4824)按选段设置试玩范围，[RevertFromDisk](https://github.com/stepmania/stepmania/blob/v5.1.0-b2/src/ScreenEdit.cpp#L6170-L6200)从歌曲目录重新加载并恢复谱面。这些代码说明编辑器选段试玩与显式磁盘回退，不构成正式成绩迁移的证据。

这些案例仅供设计借鉴，不构成 Cuexis 合同：保留编辑器光标只说明试玩起点，不代表继承上一局成绩；FNF 与 osu!lazer 的引用没有证明 Retry 会重新读盘。F5 debug reload 不能等同普通玩家入口，也不保证始终从零或失败回滚；Clone Hero 的 bot 预览不表示普通游玩支持改谱续分；StepMania 的磁盘回退也不证明正式成绩迁移。Studio 的未来边界共识见本节前文；案例不替代其余 Player 生命周期决定或实施设计。

## 7. 相关依据

- 决策与发现：[Stage 7-RR plan §4](plan.md#4-已确认方向与决策台账)、[findings](findings.md)。
- 源码：[PlaybackSession commit](../../../../engine/playback/src/playback_session_prepare.cpp)、
  [candidate commit validation](../../../../engine/playback/src/playback_session_execution.cpp)、
  [PlaybackSession realtime](../../../../engine/playback/src/playback_session_realtime.cpp)、
  [PlayerGameplay step](../../../../app/player/src/player_gameplay.cpp)、
  [Player frame loop](../../../../app/player/src/player_control_frame_loop.cpp)、
  [ExecutionHost frame path](../../../../app/player/src/execution_host_frame.cpp)、
  [frame delivery](../../../../app/execution_host/frame_delivery.cpp)、
  [Judgement dependencies](../../../../engine/judgement/CMakeLists.txt)。
- 合同与地图：[ADR 0027](../../../adr/0027-playback-sdk-product-boundary.md)、
  [ADR 0032](../../../adr/0032-playback-clock-and-prepared-audio-transaction.md)、
  [ADR 0041](../../../adr/0041-legacy-format-exit-policy.md)、
  [ADR 0042](../../../adr/0042-stage-6-productization-boundaries.md)、
  [ADR 0046](../../../adr/0046-realtime-playback-coordination.md)、
  [ADR 0047](../../../adr/0047-pd-feedback-lifetime-and-delivery-retirement.md)。
- 格式参考：[Chart representations](../../../architecture/CHART_REPRESENTATIONS.md)、
  [format index](../../../formats/README.md)、[CXC v1](../../../formats/CXC_FORMAT.md)、
  [timing model](../../../formats/TIMING_MODEL.md)。
