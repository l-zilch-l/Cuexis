# S6-C2 Player support 配置

状态：in_progress（本地无 SDL 测试已通过；本报告不是 C2 退出证据）

日期：2026-09-23

## 1. 已实现

内部静态库 `cuexis_player_support` 依赖 `cuexis_core` 和 `cuexis_json_support`。它不进入安装列表，源文件不包含 SDL、OpenGL、Playback 或 nlohmann JSON DOM。

- UserPreferences v1 只有一处代码默认值。缺失或损坏文件使用默认值并留下诊断，不回写。
  未来版本保留原文件，`saveUserPreferences` 拒绝覆盖它。
- 保存使用同目录临时文件、写回前再次校验，以及排他锁文件。锁被占用时返回
  `player.preferences.busy`，不替换上一份有效文件。父路径不是目录时保存失败，原文件保持不变。
- 用户目录在 Windows 上是 `%APPDATA%/Cuexis`，在其他平台上是 `$XDG_CONFIG_HOME/Cuexis` 或
  `$HOME/.config/Cuexis`。偏好文件是 `preferences.json`。profile 文件是
  `audio-profiles/<id>.json`，id 必须是可移植文件名。
- `loadAppConfig` 先校验偏好，再读取被点名的 profile。缺失的 `system-default` 使用代码
  profile。其他缺失、损坏、未来版本或 id 不一致的 profile 直接失败，不改走另一份 profile。
- AudioDeviceProfile v1 按 Schema 读取。`system-default` 不挑选具体设备。`exact`
  按 driver 与 device name 的字节比较，零个或多个匹配都失败。
- 输出校准使用检查过的整数微秒运算，覆盖 A2 golden 的零饱和、负校准和反向 seek。
  `correctConsumedAudioPositionMs` 把同一公式用到毫秒位置的副本上，不改 transport snapshot。
- Resolved session identity 只编码 clock mode 与实际消费的 correction。ChartClock 不消费
  非零校准。窗口和 gain 不进入该身份。
- `AudioSDL` 增加 `enumeratePlaybackDevices`、`createForDevice`、`applyGain` 和
  `recheckBoundDevice`。旧 `create`、`AudioConfig` 和 `IAudioTransport` 虚表没有改。
  `createForDevice` 在打开前复核 driver、name 和本次 instance id。默认路由的热拔插不换设备；
  显式设备在名称不再唯一时进入错误，不打开另一台设备。
- Player 在 Session prepare 前冻结 `ResolvedAppConfig` 和 `ResolvedSessionConfig`。
  窗口尺寸、全屏和 vsync 来自请求值。子系统创建后记录 `EffectiveSettings`。
  gain 通过 `applyGain` 生效。smoke 使用空的临时配置目录，避免读到本机偏好。

## 2. 本地证据

`cuexis_player_support_tests`：5 cases，64 assertions，通过。
`cuexis_audio_sdl_tests`：17 cases，528 assertions，通过。其中显式设备用例确认未知名称被拒绝，
默认路由的 `recheckBoundDevice` 成功，越界 gain 被拒绝。

`cuexis_player.exe --smoke-test` 退出码 0。配置为 `system-default`，gain 1，音频未打开，
有效窗口 1280x720。6 帧的 digest、像素、失败 reload 和 debug parity 通过。
`cuexis_player.exe --audio-smoke-test` 退出码 0。默认路由打开了 48000 Hz / 2 ch 的输出，
暂停、失败 reload 和成功 reload 通过，90 帧完成，underrun 为 0。这是本机真实默认设备，
不是命名设备的 `createForDevice` 证据。

## 3. 尚未完成

- 没有第二个进程同时抢锁的测试。当前锁测试是同一进程内的排他创建，以及父路径不可写时的失败。
- 没有把一块真实命名设备单独作为 `createForDevice` smoke。默认路由 smoke 不能代替它。
- C2 退出和 Stage 6 关闭都还没有完成。本页没有 hosted 矩阵。D2 的本地退出见
  [D2 退出报告](2026-09-23-s6-d2-exit.md)。
