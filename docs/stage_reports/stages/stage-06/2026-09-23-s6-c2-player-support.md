# S6-C2 Player support 配置

状态：in_progress（本地无 SDL 测试已通过；本报告不是 C2 退出证据）

日期：2026-09-23

## 1. 已实现

内部静态库 `cuexis_player_support` 依赖 `cuexis_core` 和 `cuexis_json_support`。它不进入安装列表，源文件不包含 SDL、OpenGL、Playback 或 nlohmann JSON DOM。

- UserPreferences v1 只有一处代码默认值。缺失或损坏文件使用默认值并留下诊断，不回写。
  未来版本保留原文件，`saveUserPreferences` 拒绝覆盖它。
- 保存使用同目录临时文件、写回前再次校验，以及排他锁文件。锁被占用时返回
  `player.preferences.busy`，不替换上一份有效文件。
- AudioDeviceProfile v1 按 Schema 读取。`system-default` 不挑选具体设备。`exact`
  按 driver 与 device name 的字节比较，零个或多个匹配都失败。
- 输出校准使用检查过的整数微秒运算，覆盖 A2 golden 的零饱和、负校准和反向 seek。
- Resolved session identity 只编码 clock mode 与实际消费的 correction。ChartClock 不消费
  非零校准。窗口和 gain 不进入该身份。

## 2. 本地证据

`cuexis_player_support_tests`：4 cases，43 assertions，通过。

## 3. 尚未完成

- AudioSDL 的独立枚举、匹配结果和 `createForDevice` 还没有加。旧 `create` 仍是默认路由。
- 没有真实音频设备 smoke，也没有第二个进程同时抢锁的测试。当前锁测试是同一进程内的排他创建。
- Player 还没有改用这个库发布 ResolvedAppConfig。那一部分仍属于 C2 的后续，不是本页退出。
- C2 退出、D2 退出和 Stage 6 关闭都还没有完成。
