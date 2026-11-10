# S6-D2 最小化与恢复

状态：本地补充证据（不是新的 D2 退出，也不是 Stage 6 关闭）

日期：2026-09-23

[D2 退出报告](2026-09-23-s6-d2-exit.md) 写退出时，最小化/恢复还没有执行。本页只记录之后的本机补充，不改写那份退出记录。

## 做法

`--smoke-test` 在第 0 帧通过后调用 `SDL_MinimizeWindow`，确认窗口进入最小化，再 `SDL_RestoreWindow`。恢复后用同一画面再提交、呈现一次，比较 digest 和中心像素。活动 presentation 在最小化和恢复之后都还在。

这次没有单独推送。改动留在本地，等下一次推送一起送出。

## 本机结果

`cuexis_player.exe --smoke-test` 退出码 0。驱动 `windows`，GPU 为 NVIDIA GeForce RTX 4060 Laptop GPU，OpenGL 3.3.0。

| 步骤 | 结果 |
| --- | --- |
| 最小化前第 0 帧 | digest `4536622714229612252`，像素 `255,204,51,255` |
| 最小化后的 drawable | 仍是 `1280x720`。Windows 这次没有把表面变成 0，因此没有走到 `presentation.renderer.surface.zero_size` |
| 恢复后 | 同一 digest，中心像素与最小化前一致，活动 presentation 仍在，drawable `1280x720` |
| 其余 smoke | 第 1 到第 5 帧、失败 reload、debug parity 仍通过，共 6 帧 |

零尺寸表面暂停提交仍由无 GPU 的 presentation renderer 测试覆盖。这次最小化没有产生零尺寸，不能把那个单测写成真实窗口证据。
