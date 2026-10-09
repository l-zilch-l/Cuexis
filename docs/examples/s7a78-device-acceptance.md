# S7A-7/8 真实设备验收谱面

状态：test-only candidate 验收材料；不是生产谱面或设备验收通过声明

更新日期：2026-10-09

## 内容与素材

`tools/make_device_acceptance_chart.py` 通过生产 `cuexis_chart_candidate --gameplay`
生成 `keyboard.cxc`、`audio.cxc`，均包含 Graph 与 Capsule revision 3 两种 entry。
原始 author/configuration、装配命令/输出、SHA 清单和 Host observations 同时输出。
五条 requirement、七个 phase：D/F/J/K 四个 Tap，最后一个 K Hold。
`--gameplay-guide` 显示四轨下落音符、判定线、长条与 Hit/Miss/分数；不再要求故意漏 J。
guide 与包一起生成，反馈读取实际 FactBinding 的 outcome 标记，不按画面或按键伪造判定。

音频由 Python 标准库自行合成：60 秒、120 BPM、单声道 PCM16/48 kHz，每四拍高音重拍。
白色纹理由程序生成，quad/material 复用仓库 demo20s 的 portable resource；不下载外部素材。
所有预算与 `testOnly=true` 只用于此 fixture，不接受生产预算或阈值。

## 生成与离线检查

在仓库根目录执行（需已完成 candidate-debug 构建）：

```powershell
.\out\build\candidate-debug\bin\cuexis_playback_tests.exe "[.candidate-gameplay-fixture]"
python -B tools/make_device_acceptance_chart.py --tool out/build/candidate-debug/bin/cuexis_chart_candidate.exe --fixture out/build/candidate-debug/s7a78-public-fixture
python -B tools/check_device_acceptance_chart.py --directory out/device-acceptance --host out/build/candidate-debug/ec/host/host-build/cuexis_reference_host.exe
```

Reference Host 路径来自实际安装 consumer；没有该文件时先运行对应 external consumer gate。
检查器以标准库独立核对完整 ZIP manifest 的 bytes/SHA、两 entry 的 artifact/semantic identity
和音频 PCM 参数。四个键盘 Host 进程覆盖 Graph/Packed、输入/零输入，核对人工 golden 及
生产公共路径的完整 Replay 比较。Host 固定 ChartClock，两次音乐拒绝是预期负例，
不能把它们说成音频播放验证。装配器本身已经做两载荷、两 intent 的实际 prepare/commit。

## 真实键盘操作

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_device_acceptance.ps1 -Mode keyboard
```

按脚本提示按 Enter 打开窗口，再按 **Space 开始**。
四条轨道下方分别标 **D / F / J / K**：

1. 短音符落到横向判定线时，按对应键并松开；四个键都会用到。
2. K 长条的头到线时按住 K；长条尾端到线时松开。
3. 窗口上方显示实际分数、Hit 和 Miss；无需盯终端或记 H 数字。
4. Space 暂停/恢复，R 重来，Esc 退出。先完整打一遍，再另测其他控制动作。

以下 Tick 表只用于验收证据核对，普通操作看窗口里的音符和判定线：

| 阶段 | 实际按键 | 判定 H 窗口（右端不含） | 操作/预期 |
| --- | --- | --- | --- |
| Tap D | D / SDL scancode 7 | [40,80) | 目标60；+2 |
| Tap F | F / scancode 9 | [100,140) | 目标120；+2 |
| Tap J | J / scancode 13 | [160,200) | 目标180；+2 |
| Tap K | K / scancode 14 | [220,260) | 目标240；+2 |
| Hold K head | K / scancode 14 | [280,320) | 目标300按住；+2 |
| Hold K body | 持续按住 K | body [320,420) | 覆盖完整区间；+3 |
| Hold K tail | 松开 K | [420,460) | 目标440松开；+5 |

H>=480 时人工结果：`score=18 combo=7 hits=7 misses=0`；分数组合
为 `2+2+2+2+2+3+5`。零输入重跑结果为 `score=-7 combo=0 hits=0 misses=7`。
各结果必须伴随 `completeReplay=same`；不以最终总分替代完整 Replay 比较。
晚策略沿既有参数（finalizationWatermark=8），截止到期与已封存之间可有延迟。

## 控制、窗口与音频验收

基础 golden 之外另开进程记录，不混用其最终分数：

1. Space 暂停：H 停止推进；暂停期间按键不补交，恢复不能伪造输入。
2. 按住 K 时切换到其他窗口：focus loss 暂停；返回/恢复后的边界结果须单独保存。
3. 方向键：Gameplay 当前只 Seek 一个 H Tick，不是音频的固定毫秒跳转；核对恢复
   后 Replay 一致、反馈无重复 token。不要用它证明多秒音画 Seek。
4. R 重载：恢复按当前 binding/resource 求投影；S Stop 后重播须从重置状态开始。
5. `-Mode audio` 启动音乐版：核对真实扬声器/耳机的 120 BPM 重拍、暂停静音、恢复、
   Stop/reload 的设备行为。测试拔设备/切换输出时记录设备型号、操作时间与实际错误，
   不预先假定设备一定会报告丢失。

当前显式 test-only 桥：每个映射 press/release 按 poll 保留顺序占一个 H Tick，无映射转换帧
也占一个 H Tick；T 每个 admitted 渲染帧 +1。同一帧多转换会让 H 多步推进，不声明 chord 同时性。
**H/T 不等于毫秒、音频 sample 或设备时间。** 不能按固定秒数操作；帧率不同耗时不同。
音频重拍用于输出/控制验收，不能作为这些 H 窗口的节拍定位依据。
该谱面不能关闭 wall-clock/audio 时钟校准、端到端延迟与同步门禁。
音符位置、判定线和反馈图形是窗口观察项，离线 golden 不证明 GPU 像素正确。

## 保存证据

每次启动在 `out/device-acceptance/runs/<日期时间>/` 保存 command arguments、电脑名、
Player/package SHA256、开始/结束时间、exit code、stdout/stderr。脚本退出会停止该次启动的
Player，不留下孤儿进程。运行结果仍标 `pending owner observation`，exit=0 不自动代表设备通过。

另写观察记录：键盘/音频设备型号、显示器刷新率、图形驱动、仓库实现 SHA、操作经过、
实际 score/完整 Replay 状态、轨道/音符/Hit/Miss观察与截图、音频听感和故障结果、未执行项。
屏幕录像可作为补充，不替代原始日志。校准、GPU/window/audio 与设备项逐项验收；
不接受生产阈值，不关闭 S7A-9 或 Stage 7A，不进入 Stage 8 发行。
