# PD-5 window protocol and matrix preparation

日期：2026-10-19。状态：**本轮工具扩展与本地验证完成；完整测量准备未完成，pilot/正式采样未开始**。
当前状态唯一见 [CURRENT_STATUS](../../../../../../CURRENT_STATUS.md)。本报告接续
[首批工具准备](2026-10-19-pd5-tools-and-profile-preparation.md)，消费
[已接受的范围、阈值与协议](../../../../../../stage_plans/active/stage-07/04-pd/pd-5/pd5-measurement-proposal.md)。
没有改变阈值、公共 API、SDK 版本、产品 owner 或线程划分。

后续目录整理已将 Python 工具和 profile 迁到
[tools/verification/playback_performance](../../../../../../../tools/verification/playback_performance/README.md)。本页文件链接指向当前实现，
下述归档仍保留验证时点的旧路径与原始内容；迁移后需重新 prepare。

## 1. 本轮交付

[owner_runner.py](../../../../../../../tools/verification/playback_performance/owner_runner.py) 为四个 Debug/Release × legacy/RPA 单元及两个共同短 host 单元提供
prepare、一次有限 pilot、register 和 window 命令。A 只读绑定冻结 PD-0 二进制、实现 ZIP 和运行资源；
B 绑定独立 Debug/Release 构建及传递 object/header/library/config 清单。
共同 host 指标仅为 request→ready；ACK 与 present-return 不混用。

[window_protocol.py](../../../../../../../tools/verification/playback_performance/window_protocol.py) 固定三窗口、每窗三对进程、AB/BA/AB 交替、至少 600 秒间隔；UTC 和 monotonic 均需满足。
每个正式进程前有独立 30 秒机器热身。每 preparation 只有一个 pilot claim 和 registration claim，
窗口 claim 排他创建；失败停止，不替换进程。推进窗口和最终统计前重算此前 claim/result/record/raw log
hash，并重新解析原始统计值和工作负载身份。15% 重复性、前六/全部九进程、宿主均值门禁及配对区间保持原方案。
同一指标三个有效窗口超界才输出拆分调查触发，不自动宣布完整 PD-5 或生产接受。

[process_control.py](../../../../../../../tools/verification/playback_performance/process_control.py) 用 Windows Job 管理包装器和原生 probe：包装器先等待握手，Assign 成功后才启动负载，
所有退出路径关闭 kill-on-close Job。正常非零、启动异常和超时均留存命令、原始日志及 hash；
正常返回另存退出码、CPU mask 和启动策略，避免失败后遗留进程继续占核。

`pd5_matrix_probe.cpp` 是独立 SDK + FrameDelivery CPU 工具，没有 Gameplay、Replay、SessionWorker、
renderer、GPU 或 audio；Replay 明确 N/A。P4 的 renderer/capture 组合仍需另一条路径。
同修订各 capture 的 FrameDigest 必须相同，物化和交付分别计时，校验和日志在局部区间外。

| 工具轨迹 | 短测 measured U / F | SDK 内部 Snapshot 调用 | payload 同时持有峰值 |
| --- | ---: | ---: | ---: |
| 无消费者 | 8 / 0 | 0 | 0 |
| 高频更新/低频交付 | 64 / 8 | 8 | 1 |
| 1 capture | 8 / 8 | 8 | 1 |
| 2 captures | 8 / 16 | 16 | 2 |
| 4 captures | 8 / 32 | 32 | 4 |
| 200 实体、一个动画 transform | 8 / 8 | 8 | 1 |
| 200/201 实体交替重载 | 8 / 8 | 8 | 1 |

短测另有 4 个预热外层周期，拓扑 measured 段重载 2 次。重载时撤销旧消费者、关闭旧 delivery、
递增资源/控制代并注册新消费者。结束后 payload/attempt live 均为 0。
内部 Snapshot 计数来自单独 metrics ON 构建，其时间不用于无插桩接受。
每 payload 的 1 MiB 是保守 storage charge，8 payloads / 8 MiB 为 delivery 上限，
不是实际 C++ allocation、World heap 或进程内存。

[profiles.json](../../../../../../../tools/verification/playback_performance/profiles.json) 用 matrix_defaults 描述实际范围。正式候选声明 10 秒预热、60 秒/14,400 个
外层周期、240 Hz；本轮仅运行无 pacing 的短工具验证，不算 pilot 或正式窗口。

## 2. 环境、内容与规模盘点

只读 [device_inventory.py](../../../../../../../tools/verification/playback_performance/device_inventory.py) 通过 Core Audio 查询三个默认输出角色的稳定 endpoint ID 和活跃状态，
通过 EnumDisplaySettingsW 查询主屏当前模式，记录 GPU PNP/驱动及两套既有内容 hash。
本机为 2560×1600、240 Hz；console/multimedia/communications 均指向活跃的 Senary Audio 耳机，
endpoint 为 `{0.0.0.00000000}.{0218a919-eb31-46ab-bba3-4b6beabe528c}`。
GPU 包括 RTX 4060 Laptop，驱动 `32.0.16.1664`。没有据此推定实际渲染 context 或 SDL audio backend 已回绑。
音乐和 DFJK 既有独立内容 hash 核对通过，旧 Player 启动入口保留。

盘点工具最初因设备没有可选 FriendlyName registry 属性而失败，日志保留；修正为可选
description/interface 字段，稳定身份仍取 Core Audio GetId。最终只读盘点成功，显式 `formal_ready=false`。

新 Release 构建重新绑定既有 `cuexis_chart_capacity_probe`。单次能力检查的 low/high reuse 均为
40,000 entities / 40,000 requirements，Packed 分别 1,232,408 / 1,247,446 bytes，状态 ok，
低于 16 MiB。它仅覆盖既有 Chart/Packed 路径，不覆盖 PD 后 Snapshot、Seek 或 Replay 运行时，
不把 serialization 时间当 Seek 延迟。
P0 的交流电、电源策略一致性、核心拓扑及无 WPR 门禁在本轮执行成功；正式运行仍需重验。

## 3. 验证与证据

- Python 34 项通过，覆盖统计拒绝、profile、窗口链、重复登记、慢样本保留、失败留证及真实进程树超时清理。
- 独立 Debug/Release 无插桩及 Release 插桩构建通过，均 STATIC、candidate ON、warnings as errors。
- Debug/Release 的 host、baseline 普通测试各 6 cases 通过；七个无插桩矩阵短测、七个结构计数检查通过。
- 架构和本轮 C++ 格式检查通过；文档、版本及 whitespace 最终检查见本轮证据清单。
- 新建无上下文独立代理只读审查并复查修复，最终未发现新的实质问题；它没有运行构建、测量或设备测试。

证据根 `out/pd5-preparation-continued-20261019/` 保存入口 ZIP、所有构建及失败日志、矩阵
manifest/raw/parsed、结构计数、设备盘点、Packed 原始输出/绑定、检查与审查摘要。
`continued-preparation-evidence.zip`、`evidence-manifest.json`、`validation-summary.json` 为最终归档。
历史 PD-0、PD-4、首批 PD-5 工具归档和实体记录不覆盖；没有正式注册。

## 4. 仍需准备

| 项目 | 剩余工作 / 判定限制 |
| --- | --- |
| P0 | runner 和 A/B 已绑定；有限 pilot、read 批长冻结和正式登记未进行 |
| P1/P2/P4/P6 | 短轨迹/计数已验证；B-only 三窗口、组合 renderer/captures、完整容量和 age 清单待补 |
| D2/D3/队列/物化、GPU | 结构计数已补；observer 开销/覆盖及可接受计时边界未完成 |
| 内存 | World/current/candidate/scratch/index、attempt/GPU 实占和回收、积压上限未齐备 |
| P7 | 稳定端点已盘点；实际 SDL backend 回绑、真实音乐/DFJK 脚本输入、UI/音频测量路径待补 |
| P8 | Packed 规模重新绑定；完整规模 Snapshot/Seek/Replay 端到端路径待补 |

源码/profile/文档变化会使旧 preparation 全量输入身份失效；工具稳定后重新 prepare，再做一次有限 pilot。
不得沿用失效 manifest 或把短测改称正式结果。当前无需 owner 再次手测；协助时间点为正式窗口前
确认安静/交流电/端点并保持前台，以及独立听感、按键手感或明确设备事件。
正式性能接受、PD-5 退出、生产 profile、设备恢复、RPA、Stage 7 关闭及提交/推送/PR/合并/发行均未由本报告宣布。
