# PD-5 tool and profile preparation

日期：2026-10-19。状态：本机阈值/协议已接受；首批测量工具与有限 CPU/GPU profile 已验证，
**完整测量准备仍在进行，未注册或运行正式窗口**。本报告不宣布 PD-5 完成。
当前状态唯一见 [CURRENT_STATUS](../../../../../../CURRENT_STATUS.md)，已接受门槛见
后续工具扩展见[窗口协议与矩阵准备](2026-10-19-pd5-window-and-matrix-preparation.md)；下文保留首批验证时点。
[测量方案](../../../../../../stage_plans/active/stage-07/04-pd/pd-5/pd5-measurement-proposal.md)。

## 1. 接受与实施范围

owner 明确接受“范围、全部候选阈值和协议”，授权继续工具/profile 准备。
这关闭了[初始就绪检查](2026-10-19-pd5-measurement-readiness.md)中的阈值接受项；
其他观测、覆盖、源码绑定和设备/环境门禁仍逐项落实，不以接受阈值替代运行证据。

实施输入：分支 `stage-7`，HEAD `d344088236ae589c0b49a4994ef0fbddd6567ce2`，已有 PD-2/3/4 未提交修改。
入口 116 项修改/未跟踪文件的 ZIP 与 hash 保存在 `out/pd5-tools-20261019/entry-workspace.zip`、
`entry-state.json`。本批修改测试工具、测试目标配置和文档；新增 OpenGL 查询实现只编入显式 device probe，
正常 Player/安装 SDK 不带该实现，没有新增公共 API、SDK 版本或 owner/线程。

## 2. 首批工具

- [measurement.py](../../../../../../../tools/verification/playback_performance/measurement.py) 区分 owner 固定轨迹、旧 worker ACK、新异步 present-return 和 PD5 事件 schema。
  只有显式共同端点和精确工作负载身份可比；缺样、重复、非有限值、失败/未知退出、插桩与样本不足不会通过。
  15% 每窗/前六/九进程波动与 10% 配对区间分别报告，内部帧数不作为独立实验数。
- [prepare_profiles.py](../../../../../../../tools/verification/playback_performance/prepare_profiles.py) 建立唯一目录/不可覆盖 manifest，检查实际编译宏、完整仓库库存及传递构建闭包。
  不只检查 probe 对象，还检查链接目标内的实现对象/头文件时序、内部库、CMake/编译器元数据与命令身份。
  源码、配置或产物变化均拒绝沿用；旧 pd0/pd4 构建不能作为当前 B 写入准备记录。
- `tests/pd/pd5_host_probe.cpp` 用固定 7,000 tick/1 MHz 源步长，要求实际 Executed 与正确 horizon，
  Runtime delta 从同段前后 tick 推导并在暂停/换段时归零。流式所有帧参与摘要，校验在计时区外；
  另核验 Replay 和 Close 后 payload live=0。
- `tests/pd/pd5_device_probe.cpp` 复用同一宿主脚本。非安装 `GpuTimer` 在 `engine/render_opengl/` 内实现；
  GPU 时间戳包住 submit 的命令范围，present/VSync 分开，查询异步读取，未完成/容量不足拒绝丢样。
  查询必须先释放，随后才允许 backend 释放 GL context。
- [profiles.json](../../../../../../../tools/verification/playback_performance/profiles.json) 展开四个有限工具 profile：CPU/GPU × stream/controls。
  全流式配置为 240 Hz、10 秒预热、60 秒/14,400 次测量更新、1 秒 journal retention；
  当前 `probe` 仅做隐藏窗口短程工具验证，显式标记 smoke、acceptance=false。

控制端点记录 command start、state confirmed、first valid present-return；Reset 重建宿主和 renderer，
暂停/运行两类分开。测试入口直接调用宿主，不包含实际 SDL 键盘事件排队延迟。
`load_host` 从宿主创建开始，故不能冒充进程启动到首帧；另保留原生 QPC 首帧 marker 供后续外层 runner 使用。
GPU profile 不含音频/真实 DFJK，不将原声音乐或人工听感替换为这个静态 fixture。

## 3. 已验证与历史问题

18 项 Python 测量/准备校验通过；包含慢样本、跨窗漂移、CI 不可判定、分位数最低样本、
ACK/异步拒绝混用、不可覆盖、传递实现/链接库/编译配置变更拒绝。
新建 `out/build/pd5-tools-release`，MSVC Release STATIC/candidate ON/metrics OFF/WX ON；
构建 baseline、host 和 device 三个工具目标通过。架构和诊断码两个适用 CTest 通过。

首批四条工具路径均退出 0：CPU/GPU 各自的 controls 完成 20 Pause、20 Resume、
10 paused Reset、10 running Reset、10 resize 和 Close；各自的短 stream 采集 8 个 request→ready、
8 个 request→present-return，GPU 另有 8 个独立 timestamp 区间。
CPU/GPU 相同固定逻辑轨迹的摘要一致。8 个样本不能报告正式 p95/p99 或判定 240 Hz 预算通过。
最终源码与构建绑定、检查和运行清单见本批 evidence 的 `validation-summary.json`。

保留首次失败及修复：CMake imported glad target 作用域、测试环境头与 GLAD include；
控制探针 Resume 首帧原先错误使用非零 discontinuity delta。独立无上下文静态审查随后指出
GPU 查询晚于上下文释放、delta 单位、非 Executed 路径漏拒绝、stream 摘要未覆盖测量帧和
传递源码/编译配置绑定缺口；均已修正，复审未发现剩余可执行问题。
审查不等于运行证据，原始构建/失败/短测日志单独保存，没有把它们计为正式窗口或删除慢样本。

## 4. 未关闭的准备项

| 项目 | 当前界限 / 下一步 |
| --- | --- |
| P0 A/B 与窗口编排 | 旧 owner/host 日志解析已用原始证据验证；还需新 B/Debug 的配对准备、有限 pilot 和不可变三窗口 runner |
| P1/P2/P4/P6 | 无消费者、8:1、独立 capture 注册及稀疏/拓扑/资源代变更轨迹尚未加入本批宿主工具 |
| D2/D3/物化/队列 | 没有可用于接受的独立阶段计时/observer 校验；不能用 request→ready 冒充各阶段 |
| GPU observer | 时间戳路径已通；开销与覆盖校验未完成，不接受无插桩 GPU 预算 |
| 内存 / 积压 | CPU payload 和 journal 有验证；World/current/candidate/scratch/index、attempt/GPU 实占与 age 上限完整清单待补 |
| P7 | 真实音乐和 DFJK 的脚本化输入、音频端点身份、实际 Player UI 排队及精确同步仍需独立路径 |
| P8 | 40,000 实体/requirements、Packed、Snapshot/Seek/Replay 仍待 PD 后新绑定和端到端测量 |
| 环境 | 正式采样前重新确认交流电、安静环境、240 Hz、稳定驱动/音频端点与前台窗口 |

这些是工具/覆盖缺口，不是预算接受被撤销。代理可以继续独立补齐自动化、profile、统计/拒绝和归档；
需要 owner 的是正式运行时保持环境、真实听感/输入手感和按明确步骤制造的设备事件。
当前不需要 owner 继续重复手动测试；正式窗口前再通知所需操作。

## 5. 留档与退出界限

本批证据根 `out/pd5-tools-20261019/`：入口 ZIP、失败日志、独立构建、每个准备 manifest/短测原始输出、
解析数据和 review 摘要分开保存。`tools-and-profile-evidence.zip` 和 `evidence-manifest.json` 记录最终留档。
原 PD-0、PD-4 源码归档/二进制/日志和三轮实体记录保留；本批新构建不覆盖旧 Player 启动入口。

本报告只证明已列出的工具准备。正式性能接受、完整 PD-5、生产 profile、设备恢复、RPA、
Stage 7 关闭及提交/推送/PR/合并/发行均未由本批声明完成。
