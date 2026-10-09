# Player discrete device input repair

状态：实际设备失败复核与实施验证；修复后真实设备结果待回填

证据日期：2026-10-09

## 实际用户运行与反例

接手 stage-7 / PR32 HEAD `1afe382a91198a22090619529c958d499f759103`，工作区初始干净。
用户执行桌面 keyboard/audio 两个 cmd，并发表新的 SDK 审批评论 `6078336026`。
该 API-observed 评论 owner、未编辑、日期与精确 tuple 均有效，本地单独审批验证通过。
Version Gate run `37911558574` attempt 2 的真实拒绝为
`version.build.skipped: same-day candidate build must be 2, found 3`。
代理上轮错误按上一候选递增 build；合同要求按 trusted master 26.10.09-1 精确递增。
本轮 updater 恢复候选 26.10.09-2，SDK 0.7.1 不变，不改 gate 规则或 master，不 reset 历史。

实际设备文件：桌面 `Cuexis-S7A78-DeviceAcceptance-2026-10-09/runs/20261009-174834-212`
为 keyboard，17:48:39 开始，最后成功 H=86；`20261009-174846-006` 为 audio，17:48:47
开始，最后成功 H=135。两个 stderr 为空，但 stdout 明确记录 `player.failure`：
`input.continuous_unsupported` / `field.path=discontinuity`，所以不能用 stderr 空证明成功。
两个旧 run.json 的 exitCode 为 null，无法补造退出码。实际日志显示 OpenGL3.3 NVIDIA616.64，
音频 source 48000Hz/1ch、device48000Hz/2ch、480 frames；这些配置日志不证明实测延迟为10ms。
用户明确反馈音频版“听到声音，也看到六个方块”，记录为听音/画面启动的人工观察；
“按任何键会关闭窗口”对应失败，未验收 Tap/Hold、正反馈像素或完整基础 golden。

最小反例：实际 PlayerController Load→Play→首次新 SDL 离散 press。
commitBundle 调用 adapter.reset，使 gap_=true；adapter.step 将其写入 crossedSamplingGap；
既有 normalization 正确拒绝任一 discontinuity 声明，Player 返回失败并关闭窗口。
原 direct adapter 构造测试没有经过 Controller.reset，故漏检实际首次消费路径。

## 合同与实施

先订正 Playback Player 首用合同：传输/内容边界与输入轨迹 discontinuity 分开。
丢弃旧 poll 批次、Pause/focus isolation、真实新 press/release 沿原规则；Load/Stop/reload/Seek
或 Pause/Resume 不能自动把新的离散转换变成 trajectory gap。删除 adapter 伪造的 gap 状态。
显式 public GameplayInput 的跨 gap/重连/丢样仍返回既有 `input.continuous_unsupported`，
不改 S7A-2/3/4/5/6 的规范化、kernel、Fold、Recovery 或 capability 规则；无第二判定路径。
所属合同：[Playback session](../../../../api/playback-session.md) Player 首用补充；
ABI/Spec v2 的拒绝规则保持不变。公开生产 API/layout 不变，SDK 保持 preview 0.7.1。

新增真实 Controller→frame loop 回归，修复前 1 case/8 assertions 失败；修复后覆盖 fresh Load、
Pause/Resume、Stop/Play、exact Seek 后 fresh key，并比较 query 与完整 ReplayEvaluation。
原 focus-lost/control batch suppression 和 audio-control rejection 测试继续保留。
`out/device-input-red.log`、`out/device-input-green.log` 保存原失败与通过输出。
启动脚本在进程退出前缓存 handle，WaitForExit 后读取 ExitCode；本机独立退出7进程实测为7，
证据 `out/device-input-exit-code-probe.log`。不会改写用户旧 run.json 的 null。

## 本轮受影响验证

候选 build 26.10.09-2 / SDK 0.7.1；合同为 Playback Player 首用补充及既有 Gameplay ABI/Spec v2。
测试对象为本报告实现提交对应的工作区改动；提交后在下方绑定 SHA。fixture 为既有
`s7a78-public-fixture` 与桌面 DFJK keyboard/audio CXC；包哈希及原始用户日志归
[原始证据 ZIP](2026-10-09-player-discrete-device-repair-evidence.zip)。ZIP 保存红/绿输出、命令与 exit、
fresh 构建、审批验证、版本算术、质量检查和测试时工作区 diff；用户旧 run.json 原样保留。

| 验证 | 结果 |
| --- | --- |
| MSVC candidate-debug fresh configure + clean-first build，warnings-as-errors | 通过；Player、assembler、Playback/PlayerControl tests |
| MSVC s7a78-candidate-release fresh configure + clean-first build，warnings-as-errors | 通过；同上 |
| 两轴 Playback `[candidate][gameplay]` | 各 25 cases / 1467 assertions 通过 |
| 两轴完整 PlayerControl | 各 31 cases / 634 assertions 通过；包含四个新控制边界 section |
| 两轴架构 + installed Playback consumer + Gameplay installed gate | 各 3/3 通过，含安装树 headless consumer |
| updater/check-current 与 trusted base1→candidate2 日期构建号比较 | 通过；算术比较不代替新 SHA 的 owner 审批 |
| docs / status / section / Version Gate Python tests / format / diff | 391 Markdown/20 JSON；4/4、5/5；26 tests（Windows 两项 POSIX skip）；通过 |

本次未重跑 candidate OFF、独立 shared、MinGW、Linux C++/sanitizer/coverage、完整设备矩阵；
此前同项成功仍只绑定其旧实现，不能证明本次 Player 修复。既有人工 fixture golden 和独立
ZIP/PCM oracle 未改动，本轮没有把新 Controller 回归称为独立 oracle，也没有声称新增设备结果。

## 保留门禁

修复后键盘/音频真实操作、命中后方块变化、暂停/Seek/reload/Stop、设备丢失及校准仍待回填。
构建、CPU/独立 golden 与用户听音启动观察不能代替这些门禁。
现有审批只绑定 1afe382；修复后新 HEAD 仍需 owner 新记录与同 SHA hosted，不能代发。
不新建 PR、不写 master、不改保护、不合并或发行；S7A-8.4 和 Stage7A 保持未退出。
