# RPA 生产架构方案

状态：design decision record；2026-10-11后续委托裁定已收口，未实施、未接受生产阈值、未获原型授权

更新日期：2026-10-11（Asia/Shanghai）

工作范围与门禁归 [RPA 分册](../../../../stage_plans/active/stage-07/plan-rpa.md)，决策方向归
[ADR 0046](../../../../adr/0046-realtime-playback-coordination.md)，字段与语义仍归各自
Spec/ABI/profile/API 合同。本文保存本日较早的推荐理由；owner随后选择K1并授权其余普通设计自行收敛，
最终选择以[ADR0046](../../../../adr/0046-realtime-playback-coordination.md)、
[RPA补充Spec](../../../../formats/rpa-execution-contract.md)和[宿主边界](../../../../api/realtime-host-boundary.md)为准。
下文保留“推荐”的标题供旧链接和比较理由追溯，不再代表等待逐项点选。

开工身份：分支 `stage-7`，HEAD=`7ca1fc6`，SDK API `0.7.5`，日期构建 `26.10.10-1`。
开工另有 7 个未提交源码/测试文件与既有文档修正，全部保留、本轮未修改。

## 1. 方案主张

一句话：**保留唯一判定内核与单 owner 公共面，把宿主侧拆成六个有明确 owner 的状态域，
用「捕获时分发的 canonical Tick」和「可复算的推进许可」两条显式协议把它们连起来，
并让热路径成本与历史长度解耦。**

```text
主线程(owner: 宿主)                    播放 worker(owner: D2+D3)       音频 worker(owner: D4)
+-----------------------------+        +--------------------------+   +--------------------+
| D1 捕获  capture-time Tick  |  已交付 | D2 执行  kernel/Fold 唯一 |   | D4 音频服务        |
| D5 设备提交  SDL/GL        | -------> | 许可谓词 / 准入 / 归档    |   | 供音/观察/设备     |
| D6 控制  命令表 / fence    | <------- | D3 表现  采样/绑定/帧     |   |                    |
+-----------------------------+  回执   +--------------------------+   +--------------------+
                     |                          |                             |
                     +------- D6 串行协调各域回执，不持有判定语义 ------------+
```

三条不可让渡的事实决定了这张图的骨架：SDL3 要求事件轮询与 GL 提交在主线程；
音频供给必须脱离 GPU 等待链；判定推进不能由绘制节奏定义。

## 2. 推荐决策一览

| ID | 决策 | 推荐 | 性质 |
| --- | --- | --- | --- |
| AD-01 | 部署拓扑 | 主线程 D1+D5+D6 / 播放 worker D2+D3 / 音频 worker D4 | 已选初始部署，生产适用性须测量验证 |
| AD-02 | 执行与表现是否同线程 | 首版同 worker，对象与接口分离，保留拆分路径 | 委托采用 |
| AD-03 | canonical 时间来源 | 固定 capture-time；H 用微秒时间基 | **已裁定 2026-10-11（C1）** |
| AD-04 | 时间映射 | 收敛为一层纯函数 + 独立有理数 oracle | 委托采用 |
| AD-05 | 同 Tick 输入 | kernel 支持「一个 Tick 一个已关闭输入批」，范围收窄 | **已裁定 2026-10-11（T2）** |
| AD-06 | 推进许可（封窗） | G1：核验完整前缀、原请求等待；Q1：单等待槽、新推进明确拒绝 | **已裁定G1/Q1/K1**；剩余合同由RPA补充Spec闭合 |
| AD-07 | Pause/Resume 计分语义 | R2 恢复状态对齐 | **已裁定 2026-10-11（R2）** |
| AD-08 | 存储 | 分块追加 + 结构共享；结果自足 | 委托采用 |
| AD-09 | 帧发布 | owning 帧 + scope/代数；只合并完整帧 | 委托采用 |
| AD-10 | 首见反馈保证 | F1 + F3-可观测：不承诺保证，但让未上屏可计数可报告 | 委托采用F1+F3可观测 |
| AD-11 | 控制协议 | 窄协议 + fence + 软件/设备回执分离 | 委托采用 |
| AD-12 | 关闭顺序 | 按域有向清理，禁止 detach 与假 Closed | 委托采用 |
| AD-13 | 音频观测字段 | 补采样时刻/segment/有效期/误差 | 已采用连贯观察，归RPA补充Spec §2 |
| AD-14 | 明确的边界外事项 | 见 §5（AD-14） | 冻结项 |
| AD-15 | offset 归属与舍入（TB-05） | 各层各用一次；presented estimate 不重复扣输出延迟 | **已裁定 2026-10-11** |

## 3. 只读核对的现行事实

设计的硬约束来源。行号仅作本次快照定位，活引用写符号名。

| 边界 | 入口 | 核对事实 | 约束 |
| --- | --- | --- | --- |
| SDL 事件轮询 | `app/player/src/player_control.cpp` `runPlayerFrameLoop` | 每轮首步 `surface.pollInput()` | 捕获留主线程 |
| SDL3 契约 | vcpkg `SDL3/SDL_events.h`、`SDL_video.h` | `SDL_PollEvent`、`GL_MakeCurrent`、`GL_SwapWindow` 标注 only on the main thread | 同上 |
| GL 提交 | `engine/render_opengl/src/open_gl_backend.cpp` | 析构/移动/`close`/`renderFrame` 要求 `SDL_IsMainThread()` 且 owner thread，违反即 `terminate` 或稳定拒绝 | 设备提交留主线程 |
| 音频回调 | `engine/audio_sdl/src/sdl_audio.cpp` `Impl::postmix` | 只写 atomics；SDL3 标注其从 SDL 拥有的后台线程运行 | 回调不承担业务 |
| 音频 owner | 同文件 `requireOwner`、`publish` | 公开方法校验创建线程；`publish()` 为 seqlock | D4 已有独立线程基础 |
| 音频观测 | `engine/audio/include/cuexis/audio/audio_transport.hpp` | `AudioClockSnapshot` **无单调采样时刻**；`presentedFrame` 由设备 mixed 帧数减 buffer 后换算并 clamp | 是估计位置而非硬件游标 |
| 时间基 | `engine/judgement/include/cuexis/judgement/timebase.hpp` | Stage 7A 推荐 `engine.tick.us.v1`，`tickScale=1/1`，**一 Tick 恰为一微秒** | 同 Tick 冲突是批次问题，不是分辨率问题 |
| 判定推进 | `engine/playback/src/playback_session.cpp` `advanceGameplay` | 成功推进后同步 `query()` 构造整份 owning 结果，再同步投影与 Runtime 更新 | 执行与表现同调用串联 |
| 内核存储 | `engine/judgement/src/execution_kernel.cpp` `submit`、`advance` | 每次 `make_shared<ReplayData>(*impl_->recording)` 全量复制历史 | 推进成本随历史线性增长 |
| 投影发布 | `playback_session.cpp` `publishGameplayProjection` | 本地副本推进 `cursor`/`scheduled`/`completedBindings`/`rejectedGroups`，仅成功路径写回；`publishable` 取 Fold 事实游标 | 已是追加式，但仍与推进同调用 |
| 公开结果 | `engine/playback/src/gameplay_internal.hpp` `GameplayAccess::result` | 每次复制完整 `KernelProjection` | 查询非共享不可变句柄 |
| 表现状态 | `engine/runtime/src/runtime_session.cpp` `updateGameplay` | 复制整份 `RuntimeEvaluationState`，跑 Behavior/Animation/resolver，成功才 swap | 求值可复用，成本随状态增长 |
| 表现寿命 | 同文件 `overrideIsActive`、`tickOverrideLifetimes` | `RemainingFrames` 按 Runtime 更新消耗 | 既有含义须保持 |
| 采样桥 | `app/player/src/player_gameplay.cpp` `PlayerGameplay::step` | 每转换 `h += hStep`，无输入帧再 `+=`；T 每帧一次；随后 submit/advance/query/archive/Replay 比较 | H 由帧数与按键数决定，非音乐；每步一次完整 Replay |
| 准备边界 | `engine/presentation_renderer/.../presentation_renderer.hpp` `prepare(PreparedPlayback&)` | 同步借用 owner-bound 对象 | 跨域前须先拆 owning 资源包 |
| 时间线 | `engine/playback/src/runtime_timeline.cpp` | `RuntimeTimeline::advance` 由 `SourceClockSample` 产出 `RuntimeFrame`；同段位置回退拒绝 | 时钟到表现已有一层，可继续分离 |
| 安装面 | 根 `CMakeLists.txt` | 公开导出 `cuexis_core`/`cuexis_audio`/`cuexis_content`/`cuexis_playback`；SDL/GL/AudioSDL 为可选叶子适配器 | 新增域不得引入平台头 |

## 4. 不变量

违反任一条即淘汰该候选，不由实现放宽。

| ID | 不变量 |
| --- | --- |
| IN-1 | 唯一 kernel/Fold；无第二判定路径、第二动画求值器或第二份时间规则 |
| IN-2 | H 只由显式推进请求改变；帧数、按键数、动画求值次数都不能推进或回退 H |
| IN-3 | 捕获时点是 canonical 来源；交付不重采样；设备时间戳只作 provenance |
| IN-4 | 已封存事实不重开；迟到输入按既有 late-policy 拒绝或转发 |
| IN-5 | 每个可变对象恰有一个 owner；锁不授予跨线程许可；`PreparedPlayback` 不跨线程移动或析构 |
| IN-6 | 不可变共享节点不反向引用活动 session、World、kernel 可变状态或设备对象 |
| IN-7 | 输入/控制/Fault/Fact/Replay 为可靠有序通道，不丢旧取新；只有完整表现帧可合并 |
| IN-8 | 完整 ReplayEvaluation 保留且可独立取得，但不进入实时热路径 |
| IN-9 | 任何 worker 都不阻塞等待主线程完成任务 |
| IN-10 | 控制操作有接收与终态回执；排队不等于生效，超时不等于取消，不可逆设备动作不假称可回滚 |
| IN-11 | 资源与帧携带 scope/代数；跨代旧帧只被拒绝或回收 |
| IN-12 | 设备失效时撤销推进许可并停止表现推进，不清空已有结果，不用失效时钟外推 |

## 5. 决策详情

### AD-01 部署拓扑 —— 推荐：主线程 D1+D5+D6 / 播放 worker D2+D3 / 音频 worker D4

主线程成员不是选择而是事实（§3 的 SDL3 与 `OpenGlBackend` 约束）。因此可选空间只剩
「D2+D3 放哪、D4 放哪」：

| 部署 | 线程分配 | 淘汰/保留条件 |
| --- | --- | --- |
| P-A 基线 | 全部主线程 | 音频 refill 受 present 阻塞、捕获受前一轮 CPU 工作阻塞；仅作语义下限 |
| **P-B 推荐** | 主线程 D1+D5+D6；播放 worker D2+D3；音频 worker D4 | 音频脱离 GPU 等待链，判定脱离渲染节奏 |
| P-C 全流水线 | D2/D3/D4 各一线程 | 除非 P-B 实测不成立，协调与关闭成本不值得 |
| P-D renderer 入 worker | — | 本地 SDL3 契约与 Cuexis owner-thread 检查不允许 |

代价必须写明：P-B **不解决主线程捕获延迟**。present 阻塞期间 `pollInput` 也不执行；
恢复后捕获到的按键其 canonical Tick 是恢复后的当前 Tick，不是物理时刻。本方案不承诺回溯。

### AD-02 执行与表现 —— 推荐：首版同 worker，对象与接口分离

两域之间传的是**可靠有序的已提交前缀**；同线程交接让顺序性不需要额外协议。
但对象、推进接口与成本边界分开，所以日后拆 D3 是部署变更而非语义重写。

拆分触发条件（写成可执行的判据，不是感觉）：实测表现最坏耗时（Behavior+Animation+resolver）
使 H 与音频位置漂移超出 profile 声明的界限时，必须拆 D3。这条判据进 A2 测量表。

### AD-03 canonical 时间来源 —— 已裁定：固定 capture-time，H 用微秒时间基（C1）

owner 于 2026-10-11 裁定维持 capture-time（方向 C1）。

理由：

- 设备时间戳的可信度、映射段与有效区间都无法在本阶段证明，接受它就要重开 V2 Spec §3.7.3 的
  唯一来源规则与 identity，属于远超本阶段收益的范围。
- 微秒 Tick 使「捕获时取整」的误差远小于任何判定窗口，语义上够用。

**不采纳**新增历史设备时间 profile（C2 不采纳）。若日后需要，须先定义可信度、映射段、
有效区间、canonical 生成时点与迟到处理，并明确 supersede 哪一句旧合同。
C3（capture-time + 未来高分辨率校准）不是独立选项：现行合同已把这条路留给 S7C-1，
且限制 S7C-1 **只能扩展 `CalibrationProfile` 参数、不得替换 canonical source**。

**必须写进合同的一句话（本裁定的直接后果）：** 主线程被 present 阻塞期间发生的按键，
其判定时间记为**恢复后的当前 Tick**，不记为物理按下时刻。该限制必须显式声明，
使宿主可以依赖它，而不是把它当成未声明的行为。

### AD-04 时间映射 —— 推荐：收敛为一层纯函数 + 独立有理数 oracle

映射链固定为 `源位置 → chart 时间 → Beat（既有 TimingMap）→ Tick（既有 TimebaseProfile）`。
不新增数学，只要求把这条链实现为一个**检查过溢出、只在一处舍入**的显式函数，
并让 segment、采样时刻、有效期与误差随结果一并携带。TB-04/TB-05 因此可用独立有理数预期复算，
不复用生产换算函数充当 oracle。

### AD-15 offset 归属与舍入（TB-05）—— 已裁定：各层各用一次，不重复扣减

owner 于 2026-10-11 裁定。

**现行事实：** 源 441 帧对应 10ms，若误按 48kHz 换算约 9.1875ms；`presentedFrame` 已由设备
mixed 帧数减 buffer 后按采样率比换算并 clamp（见 §3 音频观测行）。因此若 presented estimate
已含输出延迟，校准再扣一次会移动窗口。

**裁定内容：**

1. **源帧与设备帧严格分开。** 换算用明确的有理数关系，只在一处舍入（沿用现行「只舍入一次」口径）。
2. **输出延迟只在 presented estimate 与校准之间扣减一次。** 不得两处各扣一次。
3. **音频 offset、输入 offset、显示 offset 分属各层，各用一次。**
4. **负值与溢出按现行 checked 规则处理**，不新增令牌、不饱和。

**逐字段须标注**（A1 产物）：单位、所属时间域、应用次数、负值处理与溢出边界。
备查：现行整数校准与 checked 运算保持，不得改写其数学。

### AD-05 同 Tick 输入 —— 已裁定：kernel 支持「一个 Tick 一个已关闭输入批」（T2）

owner 于 2026-10-11 裁定：首版接受「一个 Tick 一个已关闭输入批」，
范围收窄为快速点按与和弦，**不做同 Tick 的多 timer 与输入交错**。

> 更正记录：本方案 2026-10-11 早前的版本在此处推荐「捕获侧独占分配递增 Tick」。
> 该推荐在核查 `GAMEPLAY_V2_SPEC.md` 的 S7A-2 不得消费清单后被**撤回**——
> 清单明文禁止「按 ingress 顺序解决同 Tick 冲突」。撤回理由与四个方向的完整比较见
> [A1–A3设计与准入审查](2026-10-11-design-and-entry-review.md)的后续补充；
> 本章按 owner 裁定重写。早前版本的该推荐不构成设计依据。

**问题的真实形态。** 一 Tick = 一微秒，所以同 Tick 冲突不是「判定分辨率太粗」：
`observationTick` 的唯一 canonical 来源是「输入进入判定管线时捕获的校准会话时钟」，
因此**一次 poll 批次内捕获到的多个转换共享同一个捕获 Tick**。而 canonical key 为
`(observationTick, domain, sourceClass, channelToken, actionRank, amountPresence, amountInteger)`，
现行单输入实现及修订前的execution profile §3.3把**第二个不同 subject**也归入
`same_tick_collision`。该设计条款现已由下述T2合同替代，产品实现尚未修改。
旧基线会拒绝以下两类同Tick输入：

| 类 | 例子 | 为什么必发生 |
| --- | --- | --- |
| (i) 同一 channel 的 press+release | 快速点按，同帧内按下并松开 | actionRank 不同 → 不同 canonical key → 被拒 |
| (ii) 不同 channel 同时 press | D 与 F 同帧按下 | channelToken 不同 → 被拒 |

两者都不是非法操作。CURRENT_STATUS 记录的实机 `same_tick_collision` 退出正属此类。

**四个方向与裁定结果：**

| 方向 | 内容 | 结论 |
| --- | --- | --- |
| T1 保留拒绝 + 会话可用化 | 维持现行整批拒绝，只保证稳定诊断、会话不 fault、可计数 | 作为过渡可接受，但把和弦与快速点按留在 7B+，实机失败持续存在 |
| **T2 已裁定** | 同一 Tick 允许一个显式关闭、经仲裁的输入批 | 唯一真正解决 (i)(ii) 的方向 |
| T3 捕获侧递增分配 Tick | D1 按批内观测顺序分配递增 Tick | **撤回**：踩 Spec 禁止清单；把可复算的精确映射换成受 poll 间隔约束的合成值 |
| T4 提高 tick 分辨率 | 换更细 `tickScale` | 无效：根因是批次共享采样时刻，不是刻度粗细 |

**T2 的范围边界（本裁定的具体含义，须写入合同）：**

- **支持**：同一 Tick 内多个不同 canonical subject 的已关闭批；同一来源内的合法 press/release 序列。
- **不支持**：同一 Tick 内多 timer 与输入交错的复杂次序。该形状必须**稳定拒绝**，不得静默降级。
- 输入处理采用owner于2026-10-11选定的**O1来源分组连续处理**，现行设计合同归
  [execution profile §3.4](../../../../formats/gameplay-v2-execution-profile.md#34-rpa-t2-输入批处理顺序o1已裁定未实施)。
  全局到达顺序不决定跨来源仲裁；来源内保留单调sequence，不按action rank重新排序。
- **timer 阶段序、输入仲裁序、Fact 全序三者分别定义**，不共用一个排序键。
  Fact 全序若改变即改变 `factSemanticRevision`。

**已替代的条款：** execution profile §3.3「一个 Tick 至多一个 admitted input」及dispatch碰撞拒绝
已按T2-L1的多输入执行批合同修订；
Spec「不得按 ingress 顺序解决同 Tick 冲突」的相应文字已按O1收窄为**跨来源的全局到达顺序裁决**；
来源内转换顺序与Fact总序分开。逐条被替换原文与影响测试归B0记录。

**B0 落约复核补充（2026-10-11）：** 两种顺序的组合已由owner选择O1。
后续owner选择I1来源内局部位置，发生身份合同归
[execution profile §4.1.1](../../../../formats/gameplay-v2-execution-profile.md#411-rpa-t2-发生身份i1已裁定未实施)。
捕获关批已选择T2-C1，合同归
[execution profile §3.5](../../../../formats/gameplay-v2-execution-profile.md#35-rpa-t2-捕获关批t2-c1已裁定未实施)。
跨原始Tick的dispatch批合并已选择
[execution profile §3.6的T2-L1](../../../../formats/gameplay-v2-execution-profile.md#36-rpa-t2-执行批合并t2-l1已裁定未实施)。
比较与影响见[B0合同修订记录](2026-10-11-b0-contract-revision.md)。这些裁定不直接确定Fact总序或Recovery编码。

**由此仍成立的一条**（AD-03 的后果，与 AD-05 裁定无关）：现有的 per-transition `h += hStep`
采样桥必须废除。H 必须来自校准时钟，`hStep` 只保留为显式 testOnly 夹具。


### AD-06 推进许可（TB-02）—— 推荐：可复算的许可谓词

TB-02的根因是「推进可能越过尚未交付的输入窗口」。owner于2026-10-11选择G1，
正常推进合同归[execution profile §3.7](../../../../formats/gameplay-v2-execution-profile.md#37-rpa推进许可ad06-g1已裁定未实施)。
保留本节标题供既有链接兼容；以下已选规则取代早前建议：

- D1提供捕获关闭及序列覆盖证据，D2结合实际接收/准入结局核验同时间域的完整前缀界B。
  早前将`deliveredFrontier`直接描述为序列边界并与Tick比较，缺少量纲和交付确认定义，不能实施。
- 正常运行许可为`F(H) < B`，不足时在宿主侧保留原H等待，不调用advance、不拆分或夹紧H；
  保留已接受输入与既有sealed/Fold边界。等待不阻塞输入和控制消息处理。
- 许可不成立时进入**可枚举状态**：等待输入前缀 / 时钟观察失效 / 执行积压 / 为恢复而静默。
  每个状态报告原因、关联的时间与序列界限、已等待时长、音频实际位置与所需恢复条件。

**B0关批复核订正：** D1与D5同在主线程只能说明present阻塞时捕获停顿，不能证明
所有已捕获输入已关批并交付。I1依赖完整原始Tick来源序列，最后一个Tick可能尚无完整性证明，
已捕获数据也可能仍在待交付状态。早前“不会产生悬空前缀、fence不会假性饥饿”的结论缺少
关批/交付协议前提，撤回该无条件断言。捕获关批、交付前缀与执行封存须分别证明，
T2-C1定义正常捕获关批；后续RPA补充Spec已闭合交付确认、AD-06与控制cut，不承诺固定等待时长。

四方案与已选G1见[B0记录](2026-10-11-b0-contract-revision.md#7-ad-06推进许可g1已裁定并落约)。
G2/G3/G4未采用；积压已选[Q1单等待槽](../../../../formats/gameplay-v2-execution-profile.md#371-推进请求积压ad06-q1已裁定未实施)，
新普通推进明确未接纳，输入/控制仍可处理。下一项为控制与未执行推进的顺序，见B0记录§9的K1–K4。
G1/Q1不自动冻结具体载体或控制停点，AD-06完整闭包尚未退出。

### AD-07 Pause/Resume 计分语义（TB-07）—— 已裁定：R2 恢复状态对齐

owner 于 2026-10-11 裁定 R2。

| 候选 | 内容 | 结论 |
| --- | --- | --- |
| R1 暂停点按普通 release 结算 | 映射到明确冻结的会话时点，走普通判定路径 | 未采纳 |
| **R2 恢复状态对齐（已裁定）** | 用**新的**、有 provenance 的恢复控制处理真实松键，取消被中断的持续占用；不将暂停期转换当成普通成功判定输入 | 采用：已 seal 结果保留；未完成且被中断的 body/显式 tail 结算失败；暂停期 press 不命中新 head/tap、不赚 tail Hit；之后继续本局 |
| R3 恢复前重新按住延续原 Hold | 暂停期松开不立即决定 Hold 失败 | 未采纳 |

采用 R2 的理由是维护性与明确性，不是「改动最小」：R2 是三者中**语义最容易一次说清**的——
「暂停期的转换只作恢复状态对齐，不参与判定」。R1 要靠「暂停点映射到哪个冻结时点」加规则，
R3 要靠「原 Hold 资格如何延续」加规则。

R2 不是零合同改动：它需要新增明确的恢复控制语义与 Fact 来源/错误规则，
**不得借普通 release API 实现**。失败 Fact 的 `commitTick`/`origin`、无真实 phase 观测时的
`error`/`grade`、timer 与恢复控制的先后，都须一并定义。不得生成第二次 body Miss，
也不得在没有显式 tail 的 Hold 中凭空创建 tail。

**R2 下的最小场景预期表**（B0 须逐项落入合同）：

| 场景 | R2 预期 |
| --- | --- |
| body 中途松开 | body Miss |
| body 已 Hit、tail Pending 时松开 | tail Miss；已 seal 结果保留 |
| tail 窗口边界处松开 | 同上，不产生新 Hit |
| 松开再按回相同键态 | 不证明连续性；按状态对齐处理，不产生新命中 |
| 暂停中按下新键 | 不命中新 head/tap，不赚 tail Hit |
| 无相关转换、记录连续 | 保持原状态，继续本局 |
| 无显式 tail 的 Hold | 不得凭空创建 tail |

**R2 的实现要求（不得绕过）：** 恢复准备失败保持旧暂停组合；重复 Resume 不重复结算；
暂停期真实转换既不被丢弃也不被伪造成普通输入；不调用 Paused 状态禁止的 submit/advance/update。
现行「Pause 保持 held、Resume 不补 release」被本裁定取代，须逐条列出被替换条款。


### AD-08 存储 —— 推荐：分块追加 + 结构共享；结果自足

三个成本分别优化，不用「搬到 worker」充数：

| 成本 | 现行 | 推荐方向 | 必须保持 |
| --- | --- | --- | --- |
| 推进 | 每次全量复制 `ReplayData` | 追加式分块日志 + 结构共享，成本随本次新增工作量 | 完整 ReplayEvaluation、archive/cut/H、future pending、lastObservedTick、惰性分支 |
| 发布 | 每次 query 复制完整 `KernelProjection` | 已提交事实前缀追加且不可变；发布返回 owning 句柄（前缀区间 + 会话级标量） | 完整结果语义；不安装内部 KernelProjection 类型 |
| 消费 | cursor 与派生状态可变 | 事实共享、游标归 D3，只在对应成功发布事务推进 | 去重/聚合/未来队列/token 完整更新 |

**寿命规则的关键简化**：外部 owning 结果必须自足——`shared_ptr<const Storage>` 不反向引用
session/World/设备（IN-6）。于是「会话销毁后释放结果」不构成循环依赖，
不需要回收队列，也不需要 Close 等待所有外部结果销毁（DR-05 由此消解，而非绕开）。
代价是结果在会话存在期间不能共享会话内的可变状态；这本来就是既有契约。

### AD-09 帧发布 —— 推荐：owning 帧 + scope/代数；只合并完整帧

帧封装携带 owning snapshot、采样时刻 T、执行 scope、资源代数、已应用控制序号与结果前缀。
D5 只接受与已激活资源/控制相符的帧。只有**完整帧**可合并；含增量的帧不得套用同规则。
`RemainingFrames` 的既有语义（按 Runtime 更新消耗）保持不变，不得改造成 GPU 呈现次数。

表现 T 在独立时基上推进，允许显式领先或滞后，但不得产生预测 Hit/Miss。阈值由 profile 拥有，
不藏在渲染器常数里。对外至少报告：`audioTick`、`presentationTick`、`committedFactTick`、
各自更新时间与 segment、`presentationLead`、`factLag`、最老未消费事实年龄与当前处置状态。

### AD-10 首见反馈 —— 推荐：F1 + F3-可观测（需 owner 裁定）

现状：完整帧可合并，因此「最新画面正确」不等于「反馈曾显示」。合并时被丢掉的反馈帧可能从未
上屏，这是可预测的时间语义。

推荐**不承诺**首见保证，但建立 present 回执通道并**计数**未确认上屏的反馈（F1 + F3-可观测）。
理由：承诺它就要新增一整套 typed 义务（帧 ID + 反馈 ID 的成功 present 回执、到期、取消、最大
年龄、合并保留预算），而 `IPresentationRenderer::present()` 只能证明提交给显示系统，不能证明
物理扫描输出或人眼可见——把一个无法完整证明的性质写成产品承诺，代价远高于收益。

F3 与 F2 的成本下限相同（两者都需要 present 回执通道），所以取 F3 不省成本，只是不承诺
不可证明的部分；它把「反馈可能没上屏」从不可见行为变成可报告的计数。

若产品确需首见机会，须作为**独立 typed 契约**加入，且回执只声明其最强可证明阶段，
设备丢失/过期/取消各有独立终态，不伪报为已呈现。

方案比较、边界条件与反向情形（何时应选 F2）见 [AD-10 判定说明](2026-10-11-ad10-feedback-visibility-brief.md)。

### AD-11 控制协议 —— 推荐：窄协议 + fence + 软件与设备回执分离

同一会话最多一个改变活动组合状态的控制操作进入提交阶段；无副作用的内容准备可并行，
但切换顺序由 D6 串行裁定。两条实现要求：

1. **提交 fence。** 协调域在准备阶段关闭普通推进许可，直到 commit 或取消完成。否则后台持续推进
   会使准备凭据在事务中途失效（RA-12）。fence 状态显式且可查询。
2. **回执分离软件与设备。** 软件提交成功与设备已切换分别报告；不可逆设备动作后的失败记录实际
   状态，不假称旧设备已复原。保留现行 `Result`/`Projection`/`Runtime` 的失败阶段区分。

协议必须窄、每操作单独定义语义，**不引入**通用 actor/plugin/消息总线框架。

### AD-12 关闭顺序 —— 推荐：按域有向清理

1. D6 拒绝新业务请求，只接收完成关闭所必需的回执。
2. 主线程停止捕获入队。
3. 请求音频 quiesce，等**实际停点回执**，解除设备 callback，再结束 D4。
4. 排空播放 worker：完成当前 Tick 或标记停止，发布最终状态或报告未完成，然后 join。
5. 主线程释放 GPU 资源。
6. 释放共享不可变数据。

禁止：detach 线程、超时后假称 Closed、提前销毁仍被访问的所有权、主线程阻塞等待一个需要主线程
执行任务的关闭流程（IN-9）。未完成时报告未完成并保留仍被访问的所有权。

### AD-13 音频观测字段 —— 推荐：补采样时刻/segment/有效期/误差

`AudioClockSnapshot` 现无单调采样时刻，无法判断观察新鲜度。推荐补齐：

| 载荷 | 必需信息 | 失败边界 |
| --- | --- | --- |
| 源时钟观察 | 时钟域 ID、单调采样时刻、源帧与采样率、有无音轨、播放状态、segment、位置估计的有效性与误差 | 欠载、rate/设备变化与过期快照必须可区分；缺信息时不得伪称精确硬件游标 |

`device mixed`、`source consumed`、`presented estimate` 三者继续分开，不能合并成一项。
字段落在既有 `cuexis_audio` 模块（已是安装并 PUBLIC 链接的模块），不引入平台类型。
`cuexis_audio_sdl` 已拥有全部 SDL 设备代码，可在不改模块边界的前提下独立服务。

### AD-14 边界外事项（冻结，防止范围蔓延）

- 不新增第二判定路径、不安装内部 Kernel/Recovery 执行图。
- 不引入通用任务图/actor 框架、运行时脚本或逐帧回调。
- 不建 Studio、独立 GUI、在线监控服务；观测以命令行运行/导出/比较为界。
- 不改 Chart/CXC/CXT 字段、不重写已成立的 Reader/Writer、不趁机重写格式。
- 不实施 S7B+/S7C 高级能力、Stage 8 发行或 Stage 12 规模化优化。
- 不代审批、不改保护、不新建 PR 或改变现有分支。

## 6. 实施顺序与批次

**批次表、进入/退出条件与门禁的唯一来源是 [RPA 分册](../../../../stage_plans/active/stage-07/plan-rpa.md) 的「批次表」**，
本文不复制它——早期版本此处有一份副本，已与分册产生分歧并删除。本文只保留两条设计侧的约束：

- **硬前置：** 独立诊断前置 N3 退出记录有效，是 B5 之前所有新架构原型的前置。
- **回退边界：** 每批须有定向验收与回退边界；**回退代码版本不等于回退已产生的新 archive**，
  保留原件与版本规则。语义修订与机械重构分开提交。

设计项与批次的对应关系（哪条 AD 在哪批落地）同样由分册的决策台账与批次表持有。

## 7. 后续裁定与测量边界

owner直接选择的C1/T2/O1/I1/C1/L1/G1/Q1/K1/TB05/R2保持。
其后委托裁定采用P-B、B职责边界、同worker执行/表现、P3-a、F1+F3、连贯音频观察、
分块追加与owning结果、窄控制协议和有向关闭；正式落点见本报告导语。
TB06选A1-b显式尾段时钟，AD02选同worker与测量触发规则；起点/速度/输入期限/终止H、
观察误差/有效期、容量及拆分阈值需B1/B3实测和具名profile接受，不填默认数值。
B0设计闭合与B1–B5实施依赖见RPA分册；当前不再等待普通方案点选。

## 8. 待测量（A2 口径，不给数值）

- 分域后各段耗时与缺失：捕获 → 交付 → 准入 → 封存 → Fold → 发布 → 表现消费 → 设备回执。
- 表现最坏耗时对 H 与音频漂移的影响（决定 AD-02 是否拆 D3）。
- 捕获量化跨度（AD-05）的实际分布。
- 长历史、外部旧 handle 保留与最后释放的成本曲线。
- 平台捕获能力、实际停点与所有权可行性。

所有阈值、容量与分位数保持待定；故障注入值（100ms/500ms 等）不得转为生产预算。

## 9. 授权、未执行项与门禁

本方案及后续裁定构成B0纸面设计输入；不构成原型授权、产品实施授权或运行验收。原型另需 A1 对应首用裁定、A2 测量设计、
A3 纸面准入与独立实验授权，并已满足独立诊断前置 N3 退出。存在未定语义时停止对应消费者实施，
继续不依赖该语义的工作。不新增公共符号、依赖或 target；不更改 date build/SDK API。

本轮只运行文档门禁：`python -B tools/check_docs.py`、`python -B tools/update_version.py --check`、
`git diff --check`。未运行 C++ 构建/CTest、Release 性能、原型、真实设备或 hosted 验证；
不新增这些验收结论，不改动任何既有 dated 证据。
