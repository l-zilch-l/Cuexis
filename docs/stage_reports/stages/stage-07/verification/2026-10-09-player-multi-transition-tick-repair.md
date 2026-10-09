# Player multi-transition Tick bridge repair

状态：受限 CPU 验证通过；修复后真实设备与新 SHA hosted 待回填

证据日期：2026-10-09（Asia/Shanghai）

## 故障与最小反例

父 HEAD `1458ec55575a4b893d4d7be480a978dfa795cab7`，stage-7 / existing PR32。
保留接手时未提交的[设备复测报告](2026-10-09-device-retest-same-tick-collision.md)及原始 ZIP；
keyboard/audio 已各命中一次，但分别 H185/H277 后 `same_tick_collision`、exit1，整体失败。
旧日志没有逐条转换，不能声称重建了用户当时的全部按键；本轮验证能触发相同实际失败的最小反例。

Controller Load→Play→一个 PlayerSurface poll 含同键 press/release：旧桥将二者同赋 H5。
不同映射键同 poll 同样失败；连续 press/release/press/release 则还能触发重复排队拒绝。
实际公共 submit→ExecutionKernel::submitBatch→prepareIngressBatch(resolveDomains=true)
进入已冻结的 execution profile §3：**一个 Tick 至多一个 admitted input**，不同 subject 的
同 Tick 碰撞也是原子拒绝。该规则及诊断码不改；诊断 summary 的 identical 字样不能用于
证明用户两次事件真的具有相同 canonical 身份。修复前回归 1 case / 101 assertions，3 section 失败。

## 合同修订与选择

所属合同：[Playback Player 首用桥](../../../../api/playback-session.md)（2026-10-09 多转换修订）；
实施决策：[plan-a §3.4.16](../../../../stage_plans/active/stage-07/plan-a.md)。
Gameplay Spec/ABI v2 与 execution profile 既有规则不重开。

原“每帧一个 H、保留全部转换”与“一个 Tick 至多一个 input”不能同时满足多转换 poll。
最小影响面限于 candidate Player 的显式 testOnly 离散采样桥；生产 API、配置、Graph/Packed、
kernel/Fold/Replay/Recovery、预算、物理限额及诊断码表无新字段或语义。

采用显式逐转换工作 Tick：保留 SDL poll 的转换顺序，第 i 个映射转换捕获帧前 H+i×hStep；
未映射键不占 Tick，无映射转换仍推进一个 H step；最终 H 增 max(1,N)×hStep。
T 仍每 admitted frame 只加一次 tStep。先完成所有 checked 加法和 provenance/sequence 检查，
再一次原子 submit，沿同一实际 kernel/Fold 按 F(H) advance 到最终 H，保留完整 ReplayEvaluation。
没有丢弃转换或跨帧暂存队列；控制/focus isolation、Seek/内容替换的既有边界继续适用。
它不宣称同帧 chord 同时性，也不把 SDL 时间戳或 Runtime f64 用作 canonical Tick；
生产校准仍待其既有门禁。DFJK 包与窗口不变；多转换使 H 多步增加，操作说明/脚本已同步。

## 实施与验证

实现 SHA：`90b8115b547b93fc80d66afb688145859ab54e40`；测试时父 SHA 为1458ec5，原始证据含
工作区 diff 与 fixture 哈希，桌面 implementation.json 绑定本机 Player 二进制哈希。
后续证据绑定提交只改文档，不改变本次测试的实现或构建输入。
SDK preview 0.7.1；无 public API 差异，日期 build 按 trusted master build1 保持26.10.09-2。
推送前 updater 运行26.10.09-2，未按上一 PR 候选自行加号；本次没有版本输入变化。
Debug/Release 为上轮 fresh/clean-first 后的相同配置，本轮增量编译受影响目标，不伪称再次 clean-first。

| 验证 | 结果 |
| --- | --- |
| Controller/frame-loop 同 poll press/release、多键、连续点按；Load/Pause/Stop/Seek | 通过；人工 golden score2/hits1/misses0 与最终 H=5×(max(1,N)+3) |
| 完整 GameplayResult 与 ReplayEvaluation/evidenceValid | 全路径比较通过，不只比较分数摘要 |
| 溢出故障注入：hStep=2^62，一个 poll 两转换 | 第二次加法拒绝；H保持0、完整 Replay archive bytes 和 query result 均保持原样 |
| MSVC Debug/Release candidate ON、STATIC、warnings-as-errors | PlayerControl各32 cases/713 assertions；Gameplay各25 cases/1467 assertions，通过 |
| 两轴 architecture / installed Playback / Gameplay installed consumers | 各3/3通过，包含 headless 安装消费者 |
| docs / status / section / target contracts / format / diff / version consistency | 393 Markdown/20 JSON；4/4、5/5、2/2；通过 |
| 诊断码表与 Version Gate contract CTest | 2/2通过，码名与原规则保持 |

按 code-quality workflow 进行了独立只读复核，未发现阻断缺陷；其边界复核结论归原始 ZIP。
新增多转换人工 golden 使用 Tap，没有直接验收同批 Hold head/body/tail，仍列为验证缺项。

红/绿、构建/运行命令及原始输出归[本轮原始证据 ZIP](2026-10-09-player-multi-transition-tick-repair-evidence.zip)。
这不是新增独立全 judgement oracle：人工 expected 常量不调用生产推进，完整 Replay 比较用于
同一路径一致性。已有独立 fixture oracle 没有重写或借生产函数生成新期望。

## 未退出项

本轮未重新跑 MinGW/Linux C++、candidate OFF、独立 shared、sanitizer/coverage 全矩阵；
未执行修复后的人工 GPU/window/audio/按键与控制、完整DFJK golden、设备校准/丢失矩阵。
旧SHA CI 不证明新桥；新HEAD仍需其同SHA hosted与真实owner approval，不能代发。
不新建 PR、不写 master、不改保护、不合并或发行；S7A-8.4、S7A-9最终验收和Stage7A关闭均未退出。
