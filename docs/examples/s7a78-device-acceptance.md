# S7A-7/8 离线验收夹具

状态：历史 test-only candidate 的离线生成与核对材料；不再提供 Player 专用四轨引导。

更新日期：2026-10-14。

## 内容与工具

`tools/make_device_acceptance_chart.py` 使用真实 candidate assembler 生成
keyboard/audio 两种 Graph 与 Capsule entry、作者源、配置与独立核对输入。
五条 Requirement、七个 phase 覆盖四个 Tap 和一个 Hold；素材由程序生成。
生成器不再输出私有 guide 文件。`tools/check_device_acceptance_chart.py` 直接核对
作者 Requirement、FactBinding、包 manifest/身份与 Reference Host 的人工 golden，
不依赖 Player 的绘制或按键推测结果。

```powershell
.\out\build\candidate-debug\bin\cuexis_playback_tests.exe "[.candidate-gameplay-fixture]"
python -B tools/make_device_acceptance_chart.py --tool out/build/candidate-debug/bin/cuexis_chart_candidate.exe --fixture out/build/candidate-debug/s7a78-public-fixture
python -B tools/check_device_acceptance_chart.py --directory out/device-acceptance --host out/build/candidate-debug/ec/host/host-build/cuexis_reference_host.exe
```

Reference Host 的二进制由安装 consumer gate 生成。检查器的固定 ChartClock 与音频内容
拒绝不能当作设备播放通过；离线 golden 也不证明 GPU、键盘或听音通过。
该历史夹具的 H/T 以逻辑步计数，不是 B5 的真实音频/单调时钟。

## 已撤销的窗口路径

旧 Player 引导曾由应用绘制轨道、文字和音符。按 owner 要求，2026-10-14 已移除
该绘制/解析/参数路径及 `tools/run_device_acceptance.ps1` 专用启动器。
旧目录与历史报告是当时证据，不要求当前 Player 继续支持私有 guide。
这份离线夹具本身不含完整的动态课程实体，不能直接作为当前窗口的可玩验收谱面。

现行实体谱面应依照 [Chart v5 作者指南](../guides/CHART_V5_AUTHORING.md)，将几何、
动画和反馈定义在内容中，显式选择候选 entry；输入和判定能力单独准备与验证。
B5 当前进展见 [CURRENT_STATUS](../CURRENT_STATUS.md) 与
[B5 工作记录](../stage_reports/stages/stage-07/rpa/2026-10-13-worker-deployment.md)。
