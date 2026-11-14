# R4 容量与 Player 有限证据（2026-11-14）

状态：candidate bounded evidence；不是正式计时、完整内存预算或退出接受。

返回 [New RPA 报告索引](../README.md)。依据 [11/13 收尾裁定](2026-11-13-closeout-decisions.md)、[工具实施报告](2026-11-13-closeout-implementation.md)、[P8 low 报告](2026-11-13-p8-low.md)及 [11/11 owner 裁定](2026-11-11-owner-decisions.md)。既有归属修正与 P8 `(40000,4096)` 内容口径的接受继续有效；下述缺口尚未成为已接受限制。

本轮容量与 Player 取证绑定候选 `ce2a9a6952fd4a9a4d82949544f7ef3482e204e6`。历史 P8 packet 保留各自 source/build 身份，不绑定本候选。原始证据与结构化摘要：

- 容量：`D:/Cuexis-worktree/out/evidence/r4/closeout-20261113/capacity-ce2a9a69/`；汇总为同级 `capacity-audit-ce2a9a69.md`、`capacity-audit-ce2a9a69.json`。
- P8：`D:/Cuexis-worktree/out/evidence/r4/closeout-20261113/p8-identity-audit.md`、`D:/Cuexis-worktree/out/evidence/r4/closeout-20261113/p8-identity-audit.json`。
- 音乐：`D:/Cuexis-worktree/out/evidence/r4/closeout-20261113/functional-ce2a9a69/music/parsed.json`。
- DFJK：`D:/Cuexis-worktree/out/evidence/r4/closeout-20261113/dfjk-stall-offline-audit.json`；失败原件为 `D:/Cuexis-worktree/out/evidence/r4/closeout-20261113/functional-ce2a9a69/dfjk/stopped.json`。

八个容量 profile 登记 24 对 normal/memory、48 次 invocation，得到 47 份配对 summary；七份 qualification 均有对应 blocked.json，正式语义准入全部 blocked。四个计划 residency 中，两份完整、一份环境停止、一份未启动；配对与 residency 合计保留两处停止。driver exit 0 不汇总为全组通过。

| Profile | 配对 summary | Qualification / 正式准入 | Residency |
| --- | ---: | --- | --- |
| cpu-64-low / high | 各 6/6 | 各存在 / blocked | 不适用 |
| cpu-2048-low / high | 各 6/6 | 各存在 / blocked | 不适用 |
| device-64-low / high | 各 6/6 | 各存在 / blocked | 各有完整非正式 summary |
| device-2048-low | 5/6 | 未生成 | 前序失败后未启动 |
| device-2048-high | 6/6 | 存在 / blocked | 环境停止，缺 summary |

两处停止分别为 `device-2048-low/pair-3-memory` 的电源策略或 AC 状态变化（缺 process.json、summary.json），以及 `device-2048-high/residency` 的环境变化（process exit 0，但缺 summary）。两处 runtime-environment 均失败；原始现场保持，不补填退出码、summary 或通过结论。

所有可读 memory summary 的 allocation-origin 总评级和 semantic 评级均为 `unrated`，完整语义拥有图为 false。相同 packed/Snapshot 输出、精确原点分区、已跟踪 World 子集释放与 product C++ new-book 最终归零，不证明完整 owning/lifetime 图或 native/SDL/OS 全堆释放。

| 规模 | prepare 后 state.candidate 请求 / 原 cap（B） | active、after-close 的 state.current 请求 / 原 cap（B） | 截至 host load 的产品累计并发请求峰值（B） |
| --- | ---: | ---: | ---: |
| n64 | 330,093 / 327,680 | 427,015 / 196,608 | 25,513,898 |
| n2048 | 8,598,281 / 8,454,144 | 13,249,941 / 4,259,840 | 57,834,520 |

candidate/current 的累计同时原点峰值分别为 n64 489,134 / 606,827 B、n2048 12,441,301 / 17,420,530 B，亦超过各自原 cap。设备 n2048 的 active `delivery.attempt` 原点请求为 426,102 B（CRT usable 426,318 B），超过原 cap 128,576 B；low 两份可读 memory summary、high 三份均出现，不为 low 缺失的第三份补值。这些是原点归域超额，不能直接改称完整语义 owning 超额。

prepare 数字是新进程截至 host load 的累计原点峰值，不是完整 prepare-only 或 decoded semantic heap 上界；各域峰值不能跨域或跨检查点相加。原 caps 不变，完整 semantic ownership/lifetime graph validator 尚未实现，qualification 也不覆盖 phase-hold/residency 的正式准入。

设备可读 memory summary 的逻辑 GL 存储峰值均为 72 B，最终 live_count 与逻辑存储为 0；只覆盖 GL buffer 与 RGBA8 texture，不覆盖 shader、framebuffer 或 driver。逻辑 GL 与 Dedicated+Shared 进程 GPU residency 为独立口径，不相加。

| n64 residency | Sampled active / peak delta（B） | Load-through-close 样本 | 最大采样间隙（秒） |
| --- | ---: | ---: | ---: |
| low | 1,122,304 / 1,122,304 | 6 | 0.2634816 |
| high | 1,122,304 / 1,122,304 | 6 | 0.2605631 |

两份 summary 的 sampled coverage=true，仅说明登记区间内未超过允许采样间隙，不证明连续瞬态峰值上界。before-device 各有五个样本但有效 GPU 计数为 0，保持未知。空 renderer 基线为 43,393,024 B，active 44,515,328 B，after-close/after-fixture-release 43,978,752 B；不声称进程 GPU residency 归零。n2048 没有可用 residency summary。72 B 与 1,122,304 B 分别低于原 reference ceiling 67,108,864 B 与 268,435,456 B，也不构成预算或 observer 准入接受。

P8 身份审计只覆盖指定的 `p8-low/`、`memory-40k/`、`attempt-ledger/` 三个原始根目录，不是全历史穷举。有限记录共五份 controller/native invocation packet：low 1、high 4，含一次失败与一次独立同 SHA high control。

| Packet | Source SHA 前缀 | Exit |
| --- | --- | ---: |
| memory-40k/n40000-r4096-high | 39ce3647 | 42 |
| memory-40k/n40000-r4096-high-2 | 67e905be | 0 |
| memory-40k/n40000-r4096-high-3 | 2c4e1274 | 0 |
| p8-low/n40000-r4096-low | f5f91916 | 0 |
| p8-low/n40000-r4096-high-control | f5f91916 | 0 |

五份均登记 `(40000,4096)`、capsule rev 3、maximum_processes=1、formal_sampling=false、p8=true，结果各记 p8_attempts_consumed=1；缺 PID/OS 创建身份与墙钟启动时间。限定账本未见 P8 formal claim/seal，不抹除 native 调用、不恢复额度，也不授权替换失败/control；换 backend 不恢复次数。提供的 owner 裁定未逐次处置超出 low=1/high=1 的额外 high 调用，该项仍待独立裁定。

最早 high 的 exit 42 为 `misses == scale`（4096 == 40000）断言失败，与历史报告另述的 40000 行 packed-string 失败不同；后者的原 packet 未在限定目录中识别。

P8 字节来源的两项勘误已追加到[旧 high 报告](2026-11-10-p8-40k-entity-memory.md)，其追溯缺口尚待裁定。最早 high 的 controller 原注册为 3178 B，现路径为 3280 B 且哈希不同，不能仅从该路径恢复原字节。旧 high-2/high-3 的 attribution.json 字节相同，仍记修正前 command.session=160,834,198 B、execution.retained=8,823 B、total=365,084,102 B，不能用旧 high-3 绑定证明报告中的修正数字。

修正后的 command.session=19,762,486 B、execution.retained=141,080,535 B 直接来自独立的同 SHA f5f91916 low/high-control 对：23 个声明行差值全为零，总差 6,060 B 全属 unclassified fixture。需要修正的是证据引用与字节来源，不推翻 11/11 owner 已接受的归属修正或内容口径；也不据此接受新 cap 或追认额外调用。low 仅把 requirement 结束 tick 改为 end+i，不证明通用低复用、冷缓存或 40K @240 FPS 上界。完整语义图绑定仍缺失，memory/P8 正式 claim 维持 blocked；本轮容量批 P8 invocation=0。

P7 两条路径的功能事实如下，均为单进程脚本化 opt-in trace，formal_attempts_consumed=0，不是 normal 性能或正式三窗证据。

| 路径 | 已记录事实 | 当前结论 |
| --- | --- | --- |
| 原音乐 | 30,961 帧；active 129.044984 秒，轨迹总长 130.864264 秒，其中 terminal wait 1.819280 秒；audio_ended=true、underruns=0 | 功能 valid=true；formal_registration=false、acceptance=false、normal_timing_acceptance=false；听音及实体输入均未观察 |
| 原 DFJK | 7,808 帧；active 34.980515 秒，轨迹总长 35.409508 秒；39 次 script 派发、38 次捕获；40 分、19 命中、2 misses；Replay 一致、underruns=0 | 未达到原 oracle 的 46 分、21 命中、0 misses；原失败保持，不用 Replay 一致、旧 scratch 成功或退出码替代本候选通过 |

DFJK trace 的 ordinal/interval 连续性核对有效。最大帧间隔位于 ordinal 2052：previous present 相对 8.542768 秒、current present 11.011616 秒，gap 为 2,468,848 us；本请求 request→ready 为 391 us，ready→present observation 为 2,468,168 us。间隔覆盖 10 秒附近脚本输入，与输入延后及两次 miss 相容；缺逐输入实际派发时间，不能证明具体 driver/GPU 原因或脚本时基缺陷。

至少 600 秒实际活动的要求只对应适用的 Player/ExecutionHost 60 秒 retention 门；音乐不经过 ExecutionHost，其 retention 适用性独立，不因原音乐约 129 秒就隐含延长要求。原 DFJK 是固定 35 秒课程，不能用 EOF/terminal 后等待、冻结帧补足活动样本，也不替换、延长或循环课程制造 600 秒证明。两条功能 trace 均不构成正式 normal 三窗结论；DFJK 适用的长活动证据仍缺，音乐 retention 不套用该门。

仍待处置：完整语义 graph 与原点超额；n2048 residency 缺失及 observer 准入；P8 额外调用额度和字节来源追溯缺口；DFJK 功能失败、P7 normal 与各自 retention 适用性。缺口继续保持未接受，不自动转换为 pass、N/A 或已接受限制。本报告不新增次数、预算或替代课程，不关闭 R4 或阶段退出门。

小记录的按字节副本及来源哈希见[本轮归档](2026-11-14-closeout-evidence/index.md)；原始完整进程文件与历史失败不替换。
