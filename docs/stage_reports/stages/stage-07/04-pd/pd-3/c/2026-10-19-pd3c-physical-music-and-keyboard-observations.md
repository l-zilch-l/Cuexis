# PD-3-c 后独立音乐与实体键盘观察

日期：2026-10-19（Asia/Shanghai）。状态：受限实体观察；完整播放、长期容量及设备验收未通过或未完成。未修改生产源码，PD-4/5 未开始。

## 输入与环境

本轮按 owner 要求准备真实 GPU/audio 与实体操作，未启动正式性能验收。
当前状态见 [CURRENT_STATUS](../../../../../../CURRENT_STATUS.md)，验收归属见
[RPA 计划](../../../../../../stage_plans/active/stage-07/03-rpa-legacy/plan.md)。

工作区 HEAD 为 `d344088236ae589c0b49a4994ef0fbddd6567ce2`；PD-3-c 的未提交实施/测试/构建输入指纹为
`abdf37111c38826b803d3e6d4ec36c561861d2f94d53d8247ea6844b1e980ed8`，58 个输入逐项核验未变。
本轮仅增加测试素材、启动与诊断工具、独立 Release 构建及设备记录；不以 HEAD 单独认证这些未提交输入。

用户确认环境稳定，系统默认输出为耳机（Senary Audio）。实际日志记录 Windows SDL、RTX 4060 Laptop / NVIDIA 616.64 OpenGL。
显示环境先前盘点为 2560×1600 / 240 Hz；测试窗口为 1280×720、VSync 开、gain 0.35，未为规避容量改变刷新率。

素材和启动目录为 `C:/Users/Zilch/Desktop/Cuexis-PD3c-DeviceAcceptance-2026-10-19/`。
键盘 CXC/Packed/configuration 与用户原 `Cuexis-B5-DevicePractice-2026-10-13` 保持相同；原目录和历史 runs 不修改。
音乐单独制作 CXC，使用用户提供的 `D:/CloudMusic/Junk - Life is PIANO.flac`，通过指定 ffmpeg 转换为完整
129.03195 秒、44.1 kHz 双声道 PCM16，未归一化、混入提示音或附加静音，原 FLAC 不修改。
两套 CXC 均经当前工具校验。首轮音乐素材误用 v5 alpha 字段导致 v4 校验拒绝，修正后通过；失败包与日志保留。

独立音乐走普通 Playback AudioClock + SDL/OpenGL；DFJK 走显式 testOnly RPA ExecutionHost，两者不互相代验。
Release Player 位于 `out/build/device-release-20261019/`，由 release preset、candidate ON、WX ON、BUILD_TESTING OFF
fresh configure 与 clean-first 构建；配置、编译与 CPU 诊断均退出 0。
实际 Release Player SHA-256 为 `05f12917d347b8e3e76a63f4730a3e142f0b9bff757e9c8b9bf49a3d608b99cc`。

## 音乐与重载响应

| 运行 | 人工观察 | 实际日志与处置 |
| --- | --- | --- |
| `110841-131-music` / Debug | 其他操作正常；暂停时 R 约 5–6 秒无响应后恢复 | 重载返回到首帧约 7 ms；此前同步准备未被首帧日志覆盖；最终 underruns=7 |
| `111312-473-music` / Debug | 载入约 3 秒无响应 | RTX 4060 与音频实际打开；重载返回到首帧约 5 ms；最终 underruns=1 |
| `112219-849-music-release` | 卡顿明显减少，本轮未出现无响应；reset 约 0.5 秒 | 退出 0、stderr 空；最后一次暂停后 R 仍为 paused；最终 underruns=1 |

上表运行名均以 `20261019-` 开头。旧 Debug 启动器保存的退出码为 null，未改写成 0；后续 Release 启动器通过原生进程 handle 取退出码，独立 exit 7 检查通过。
用户的 0.5 秒为人工估计，不能当作校准后的端到端耗时。underrun 计数与主观听感分别保留，产生时刻尚未定位。

纯 CPU 诊断 main 链接实际 Player 生产库/对象，调用同一 PlaybackSource、PlaybackSession、WavDecoder 和 AudioClipStore，未创建窗口或音频设备。
每个配置为一次初始 load 和两次 reload，原始分项见 `out/device-acceptance-prep-20261019/load-probe.jsonl` 与 `release-probe/load-probe.jsonl`。
Debug 的包打开/校验/转换约 3.4–5.0 秒，prepare 约 2.5–2.7 秒，整曲解码约 1.4 秒，总 CPU 路径约 7.4–9.0 秒；
同源码 Release 总 CPU 路径约 0.465–0.476 秒。以上不是实体窗口冻结时间、正式性能对照或预算结论。

普通 Player 在主线程同步执行 sourceFor、prepareReload 和 preparePlayerAudioClip，窗口消息处理在这段工作中停止；
R 是完整 reload，重新读取校验包、准备内容并解码整曲。同 mode、同设备时复用音频 seat 的 replacement 路径。
Debug 成本显著放大等待；Release 实体观察改善，但同步准备设计未修改，响应性优化不据此宣布完成。

## DFJK 实体记录及观察差异

owner 反馈“两次测试，第一次完整播放正常退出，第二次反复 reset 及暂停均正常”。新入口的两轮原始日志显示：

| 运行 | 原始结果 | 可支持的结论 |
| --- | --- | --- |
| `112648-854-keyboard-release` | 首次判定记录后约 17.6 秒报 `host.rpa.capacity_exceeded: Frame observation protocol rejected`，退出 1；无 terminal horizon 完成记录。最后 score=34、combo=15、hits=15、misses=0、factCursor=27 | 观察到实体 Tap 与 Hold 的阶段得分；整段 35 秒课程未完成，不能记为完整播放/EOF 通过 |
| `112720-362-keyboard-release` | 两次 `RestartAtZero completed; state=paused`、两次 Resume；退出 0，stderr 空。最后一次重载后的局部结果为 score=-1、hits=0、misses=1 | 短时实体 Pause/R/Resume 与退出正常；Miss 不单独视为实现失败，不把该轮当完整课程全命中证据 |

用户观感与第一轮失败日志同时保留；窗口消失不是正常曲终或退出 0 的证据。
第一轮 34 分 / 15 Hit 对应当前课程到 Hold tail 的前缀，课程末尾仍有后续 Tap；完整全命中期望为 46 分 / 21 Hit。

当前 `FrameObservationLedger` 仍占用固定槽位且未退休回收，该实体失败与已知容量问题相符。
现场日志不含失败 frameId 或精确累计帧数，因此不把本次失败帧写为第 4097 帧。
第二轮 R 重建了宿主/账本；反复 R 后不耗尽不能证明长期回收。未提高容量、改变刷新率、重试直到通过或覆盖失败样本。

## 证据和剩余事项

本轮机器可读输入、构建命令、诊断、用户反馈和原始 runs 归档在 `out/device-acceptance-prep-20261019/`：
`preparation.json`、`debug-probe-binding.json`、`release-probe-binding.json`、`music-release-physical-observation.json`、
`keyboard-release-physical-observation.json` 和 `observed-runs/`。桌面包的 binding 文件核验二进制与素材，
`LATEST-RESULTS.md` 明确保留上述失败及局部通过范围。

继续项为 PD-4 寿命/退休/回收及适用门禁，然后在同内容、明确实现身份下重跑完整课程与长期负载。
该实施尚未开始，本轮不以调参或重载回避。当前无需反复重复同一长测。
focus、真实设备失效/恢复、真实 EOF/tail、精确音画/输入延迟、underrun 归因、正式 Release 性能窗口、
生产参数/预算与最终 owner 接受仍有未执行或未接受项；本记录不关闭 RPA、PD 或 Stage 7。
