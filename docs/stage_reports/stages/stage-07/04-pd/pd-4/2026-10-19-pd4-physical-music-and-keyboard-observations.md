# PD-4 独立音乐与实体 DFJK 观察

日期：2026-10-19（Asia/Shanghai）。状态：受限听音、DFJK 完整课程及控制观察完成；控制附近 FPS 波动待量化，正式性能与完整设备验收未完成。
当前状态归 [CURRENT_STATUS](../../../../../CURRENT_STATUS.md)，PD-4 内部实施见 [实施报告](2026-10-19-pd4-delivery-retirement-and-observation.md)。

## 输入与记录

owner 授权将启动入口切换到新 Player 并开始实体听音、DFJK 验收。使用已通过本地构建、门禁与 GPU smoke 的
`out/build/pd4-player-release/bin/cuexis_player.exe`，SDK 0.9.0、日期构建 26.10.19-1。
HEAD 为 `d344088236ae589c0b49a4994ef0fbddd6567ce2`；81 项未提交实施输入逐项核验，指纹为
`59e304b8f34afc4a124f22f5b156f5c4df20db3b90b984e2aebeaac9de787fdb`。
二进制 SHA-256 为 `72594dfebc658e6767d672aadb12aa8afc3305bbf92156633a34652946f2eca1`。

桌面 `Cuexis-PD3c-DeviceAcceptance-2026-10-19` 保留目录名，四个入口均切换到 PD-4 Release，
B5 原目录入口同步切换。脚本与哈希绑定先备份再更新；历史 runs、旧版本结果和二进制保留。
每次 `command.json` 记录相同新版 SHA、源码指纹、SDK、配置与 `--show-fps`。

音乐与 DFJK 素材保持独立；音乐仍为完整 Junk - Life is PIANO，129.03195 秒；DFJK 为原 35 秒课程。
系统默认音频输出（用户指定耳机 Senary Audio）、gain=0.35、1280×720、VSync 开。
实际日志记录 Windows SDL、NVIDIA RTX 4060 Laptop / OpenGL 3.3 NVIDIA 616.64。
音乐走普通 AudioClock/SDL/OpenGL；DFJK 走显式 testOnly ExecutionHost，两者分别提供证据。

## 三轮实体观察

下表运行名均位于桌面包的 `runs/`，前缀为 `pd4-20261019-`。三轮退出码均为 0、stderr 均为空，未发现 error 或 capacity_exceeded。

| 运行 | 日志 | 人工观察与结论 |
| --- | --- | --- |
| `170504-795-music-release` | 进程 152.65 秒；含 Pause、Seek、Reload；最终 paused，discontinuity=25、underruns=2 | 用户进行 1 次听音测试并反馈正常。随后明确说明 underrun 由切换到 Codex 打字触发，与程序故障无关；按该操作背景记录，不据此判定本轮播放故障 |
| `170750-819-keyboard-release` | 进程 37.29 秒；`Declared terminal horizon actually executed`；末次 score=43、hits=20、misses=1、factCursor=40 | 用户反馈正常；已超过旧版约 17.6 秒失败位置并完成声明的完整课程。手动 Miss 不单独判为实现失败，不记为全命中验收 |
| `170831-581-keyboard-release` | 进程 41.92 秒；Paused、Resume、两次 RestartAtZero（分别保留 paused、running）；无完整 terminal horizon | 用户反馈控制正常；支持 Pause/Reset/Resume 与正常退出，不把重载后的局部课程记为第二次完整课程 |

音乐进程持续时间包含用户控制，不能单独证明连续无控制的完整 129 秒播放或精确同步。
underrun 计数和用户对切换 Codex 的说明均保留；日志未逐次记录其触发时刻。

## 控制附近帧率观察

owner 报告 DFJK Reset 与暂停/恢复时 FPS 波动，并补充约降至 130，随后恢复 240；未提供持续时间。
本次不改生产源码、刷新率或 VSync 来消除读数，也不据此认定性能门禁通过。

`PlayerFpsMeter::notePresented` 在至少 0.5 秒的 wall-clock 窗口累计成功呈现数再更新显示，
该显示不能定位单帧延迟或直接换算控制阻塞时间。Reset 当前包含 Close、重建 backend 与 load；
本轮日志可确认对应路径发生，尚无分项时间证据支持把降帧归因到某个模块或仅归为统计效应。
性能项保留为“操作附近 FPS 下降后恢复，幅度已有人工观察，持续时间与模块成本待量化”。

## 证据与边界

原始日志在桌面包 `runs/pd4-*`。冻结副本为 `out/pd4-device-acceptance-20261019-170458/`：
`before-launchers.zip`、`setup.json`、`physical-runs.zip`、`observations.json` 与 `evidence-manifest.json`。
原始参数/日志/退出码与启动绑定均保留；桌面 `PD4-RESULTS.md` 提供入口摘要。

本轮支持受限听音、实体完整 DFJK 与控制功能观察；不替代设备失效/恢复、长时压力、精确音画同步、
正式性能 15% 门槛、生产参数接受、RPA 或 Stage 7 关闭。此前 PD-3-c capacity_exceeded/退出 1 的失败证据保持不变。
PD-5 未开始。
