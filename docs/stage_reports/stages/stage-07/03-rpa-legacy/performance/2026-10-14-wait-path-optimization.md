# Stage 7 RPA Wait Path Optimization Evidence

证据日期：2026-10-14

状态：implementation evidence；本报告只覆盖本轮等待路径修改和受控性能复测，不关闭 F3、B5 或 Stage 7。

## 修改范围

本轮修改了 `OwnerChannel`、`SessionWorker`、`AudioWorker` 和 Player execution host：

- `OwnerChannel::post` 和 `complete` 更新 completion generation，并通过已有 condition variable 唤醒等待者。
- `SessionWorker`、`AudioWorker` 暴露有限等待入口。
- `ExecutionHost::pump` 使用 worker completion wait 替代固定 1 ms `sleep_for`。
- RPA Player 外层帧循环移除固定 1 ms sleep；首帧等待仍使用有限等待。

控制协议、request ID、terminal reply、ack、owner 边界、Replay 和 FaultPrefix 没有改动。

基线存档为 `1174ce9`（`docs: archive stage 7 performance modification plan`）；产品修改发生在该存档之后，尚未形成新的产品提交。

## 验证环境

- Windows，RTX 4060 Laptop，1280x720，Audio clock。
- 同一谱面：`C:/Users/Zilch/Desktop/Cuexis-B5-DevicePractice-2026-10-13/practice.cxc`。
- 同一 Gameplay packed、configuration、budget 和 DFJK 映射。
- 当前候选 Debug Player，诊断副本由 `out/perf-investigation-20261014/build_deep.py` 从当前源码重新构建。
- CPU steady clock 分段计时；不是 GPU timestamp。
- 对照运行使用历史未修改诊断二进制；当前运行使用新构建的 `deep/player_profile.exe`。

## 性能结果

统计窗口为运行约 1–33.5 秒，`main.rpa.frame` 次数作为同一时长内的有效帧数近似。

| 路径 | 基线帧数 | 修改后帧数 | 变化 | 基线平均帧耗时 | 修改后平均帧耗时 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 判定、无输入、VSync on | 1,532 | 2,099 | +37.0% | 19.04 ms | 13.82 ms |
| 判定、无输入、VSync off | 1,538 | 2,804 | +82.3% | 18.97 ms | 10.41 ms |
| 判定、全命中输入、VSync on | 1,449 | 2,586 | +78.5% | 20.13 ms | 11.29 ms |

关键分段变化：

- `ExecutionHost::pump` 平均耗时从约 1.77–1.78 ms 降至约 0.55–0.59 ms。
- RPA 外层 `main.sleep` 从每帧约 1.84–1.86 ms 降为启动阶段的一次性记录，运行帧循环不再固定休眠。
- 无输入 VSync on 的 `main.advance` 从 5.97 ms 降至 2.33 ms。
- 全命中输入的 `main.advance` 从 6.88 ms 降至 2.39 ms，`main.present` 从 4.61 ms 降至 3.14 ms。

## 行为结果

- 无输入：修改后仍得到 `score=-21`、`hits=0`、`misses=21`，并完成 EOF tail。
- 全命中：修改后得到 `score=46`、`hits=21`、`misses=0`，并完成 EOF tail。
- MinGW headless RPA 标签测试：97/97 通过。
- MSVC candidate Debug 的 `cuexis_playback` 和 `cuexis_player` 目标构建通过。
- MSVC RPA 测试目标仍受仓库已有测试夹具使用 `__int128_t` 的 MSVC 兼容错误阻塞；该错误与本轮修改无关。MinGW RPA 测试提供了本轮行为验证。

## 证据边界

本轮只证明等待路径在当前受控内容和本机环境中减少了固定空等。尚未完成 Release、跨平台、长时间容量曲线、实体键盘手感、听音同步、设备故障恢复和生产 F3 阈值接受。历史诊断 CSV 和本轮 CSV 保存在 `out/perf-investigation-20261014/`，包括 `deep-current-judgement-on`、`deep-current-judgement-off` 和 `deep-current-input`。

