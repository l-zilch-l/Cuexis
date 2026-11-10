# Real device retest: same Tick collision

状态：设备复测失败；部分真实输入路径已取得证据，非完整验收通过

证据日期：2026-10-09（Asia/Shanghai）

## 实际执行与结果

两次 sourceHead 均为 `1458ec55575a4b893d4d7be480a978dfa795cab7`，对应修复实现
`afdcfcd8890cb8c5a3ae6aa8c8273d4fdc15b5ff`；Player build 26.10.09-2-exp.candidate / SDK0.7.1。
使用原桌面 DFJK Packed CXC 和显式 testOnly H/T 每 admitted frame +1 桥。
本次未重新编译、注入输入或改写用户运行文件；读取命令与独立日志扫描汇总归
[原始证据 ZIP](2026-10-09-device-retest-same-tick-collision-evidence.zip)，含原 run.json/stdout/stderr 与哈希。

| 模式 | run | 首次 Hit H | 最后成功 H | 完整 Replay=same 次数 | 退出 / 诊断 |
| --- | --- | --- | --- | --- | --- |
| keyboard | 20261009-181052-493 | 162 | 185 | 185 | 1 / same_tick_collision |
| audio | 20261009-181112-325 | 147 | 277 | 277 | 1 / same_tick_collision |

最后两者均为 score=2 / combo=1 / hits=1 / misses=0；stderr 均为空。
这证明真实新输入已到达实际 Gameplay 并产生一次命中，首次 discontinuity 问题在这两次
运行中未出现；不能证明后续 Tap/Hold、故意 Miss、完整 golden score13/hits5/misses1 已通过。
两者均以真实 exitCode=1 结束，分别于 18:11:00.924 / 18:11:32.923 记录
`judgement.s7a2.input.same_tick_collision`（invalid_relation，observationTick.sameTickCollision）。
本次仅核对日志，未新增用户对方块变化/声音/控制的人工观察。

## 碰撞调查边界

既有 Gameplay Spec §3.7.4 / ABI CM-T10 持有同 Tick canonical 身份拒绝规则。
PlayerGameplay::step 当前把一个 SDL poll 中所有映射转换赋予同一个工作 H，再批量 submit。
这提供多转换碰撞的调查入口，但本次日志没有逐条 scancode/action/timestamp/ingressSequence，
无法从现有输出还原究竟是哪组物理按键或 poll 序列触发；不能把猜测写成已复现根因。
接下来需要最小多转换反例、实际 ingress 路径复核和合同对齐，再决定适配器修订；
不通过删除碰撞检查、丢弃真实 press/release、按到达次序偷偷分配判定 Tick 或新判定路径绕过。

## 未退出项

真实设备整体验收仍失败；故障边界、完整人工 golden、Hold、控制、GPU反馈、设备校准未完成。
本次没有新实现、没有新构建/CPU测试矩阵或新 SHA hosted 证明；未代发审批、未写 master、
未新建 PR、未修改保护、未合并/发行。S7A-8.4 与 Stage7A 保持未退出。
