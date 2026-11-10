# 40K 实体真实 GPU 吞吐核验

状态：有限本地设备诊断；不作为 R3/R4 或生产接受

日期：2026-10-29

返回 [RPA 报告索引](../README.md)。当前接受状态只见
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md)。

## 场景与计时边界

owner 要求实测 40,000 个实体能否达到 240 FPS。前轮 Seek 对照的 40K 是 gameplay requirements，
presentation 只有一个对象；其 2.562 ms 请求到 CPU 测试呈现不能用来推断 40K 渲染吞吐。
本轮新增显式设备探针 `tests/player/gpu_throughput_probe.cpp`，挂入既有
`cuexis_execution_device_probe`，没有修改产品运行算法或现有 PD5 profile。

| 项目 | 本轮配置 |
| --- | --- |
| 设备 | AMD Ryzen 7 8845H；NVIDIA GeForce RTX 4060 Laptop GPU；OpenGL 3.3，驱动 616.64 |
| 显示 | 设备报告 2560×1600 / 240 Hz；本轮使用可见窗口、实际 framebuffer 1280×720 |
| 构建 | MSVC Release STATIC，SDK 0.9.1，普通分配器；PD metrics / RPA observation / lifetime observer 全部 OFF |
| 调度 | 构建串行、BelowNormal、affinity 3；实测进程使用默认 16 逻辑 CPU 集合（mask 65535）、BelowNormal、执行节流关闭；电源 AC |
| 图形 | VSync 实际读回 0，不限帧，不 sleep；无音频、无 Gameplay、无 CPU capture 消费者 |
| 实体 | 40,000 个独立 transform + renderable，200×200 网格，共享一个三角形 mesh 和一个不透明无光照材质；缩放 0.06 |
| 相机 | 位置 z=+10，默认朝 -Z；各实体中心均在裁剪空间内；最终材质像素 `[255,204,51,255]` |
| 验证 | 每帧确认 40,000 条 opaque 命令、没有 transparent 命令；动画场景结束确认全部 40,000 个对象 z 已变化 |

三种路径分别为：复用已有 FrameSnapshot，只执行 backend submit / present；每帧
PlaybackSession update + 静态 extractFrame + submit / present；每帧更新所有实体的
Chart v1 `behavior.transform.keyframe` z 轨道，再提取及渲染。最后一种是明确的 legacy 行为夹具，
不能外推为所有 Chart v4 动画或完整 Gameplay 负载的表现。

完整帧时间包含相应 update、extract、submit 和 present。wall FPS 还计入循环中的 SDL pump、
query poll 等开销；不计加载、warmup、最后 query drain 或输出日志。FrameSnapshot 目标缓冲复用。
GPU timestamp 包围实际 backend submit，不含 present / VSync；这个区间可能包含 GPU 等待 CPU
提交的空隙，且产品 submit 自带同步像素回读，不能称为纯 shader busy time，也不能与 CPU submit 相加。

## 登记与有效性

证据根为 `out/verification-20261029/gpu-40k-v1/`。先以 `pilot-v2/` 为三种路径各做
8 measured / 2 warmup 的功能小样，再独立登记 `measure-v2/`：每路径三次 timer-ON 新进程，
顺序 render-only / static / animated、animated / static / render-only、render-only / static / animated；
最后各一次 timer-OFF 新进程。每进程 120 measured / 30 warmup，总计 12 个进程、1440 个测量帧。
这是依据小样运行成本在主组登记前固定的有限规模，没有补样或延长直到通过。

主组 12 个进程全部有效，1440 个测量帧均超过 4.167 ms。当前实现不能在此场景跑满 240 FPS。
无时间戳对照的结果如下，每格来自对应路径的独立 120 帧进程：

| 路径 | wall FPS | 完整帧均值 | 完整帧 p99 |
| --- | --- | --- | --- |
| 复用快照，仅 submit / present | 18.25 | 54.776 ms | 65.020 ms |
| 每帧静态 update / extract / render | 13.39 | 74.698 ms | 89.889 ms |
| 全部实体 Chart v1 行为更新 / extract / render | 4.47 | 223.868 ms | 239.218 ms |

timer-ON 的三次独立重复保留如下；表中 p99 是各进程的 p99 范围，没有将同进程帧当作独立进程：

| 路径 | 三次 wall FPS 范围 | 完整帧 p99 范围 | GPU submit 时间戳区间 p99 范围 |
| --- | --- | --- | --- |
| 复用快照 | 17.49–17.75 | 64.716–67.853 ms | 22.405–25.420 ms |
| 静态提帧 | 12.94–13.13 | 85.344–90.614 ms | 26.136–29.638 ms |
| 全量 Chart v1 行为 | 4.36–4.38 | 240.045–243.464 ms | 21.960–25.876 ms |

关闭 timestamp 后 wall FPS 比 timer-ON 三次中位数分别高 3.0% / 2.0% / 2.4%。
这只是各一次有限对照，不能作为正式 observer 校准，但两种配置都明确未达到目标。
三次 timer-ON 完整帧均值的极差 / 中位数约为 1.41% / 1.29% / 0.41%；
这是局部描述统计，不替代原正式三窗口、15% 及其他接受门。

## 分段结果与可支持的判断

无 timestamp 对照的每帧平均耗时分段如下，分段合计等于上表完整帧均值：

| 路径 | update | extractFrame | submit（含驱动、GPU 等待及像素回读） | present |
| --- | --- | --- | --- | --- |
| 复用快照 | 不执行 | 不执行 | 54.700 ms | 0.076 ms |
| 静态提帧 | 11.604 ms | 7.271 ms | 55.747 ms | 0.077 ms |
| 全量 Chart v1 行为 | 162.012 ms | 7.184 ms | 54.592 ms | 0.080 ms |

因此只解决 Seek 不会解决正常帧吞吐：即使跳过 update / extract，当前 submit 路径仍远超预算；
每帧静态提帧的 update 和 extract 也分别超出完整 4.167 ms 预算。全量旧格式行为的额外成本
主要出现在 update。present 返回本身很短，不支持将主要差距归为 VSync 等待。

源码显示，当前后端在 `drawUnlitCommands()` 中逐实体调用 `glDrawElements`；
每帧还构造与解析完整 DrawSummary，并在 submit 内同步读取像素。
这些是下一步可细分的路径，不能仅由现有区间就唯一判定 CPU 构造、驱动调用和 GPU busy 各占多少。
本轮没有移除回读、减少实体、启用实例化或改变产品算法来生成更好的结果。
硬件原始容量、其他几何/材质、Chart v4 动画、完整 Player 协调和正式接受仍须分别验证。

## 验证与留档

最终 fixture 的三种 pilot 均为 1 case / 16 assertions 通过；主组 12 个进程同样全部通过，
每个进程验证实际 GPU、viewport、VSync、绘制数、可见性、材质像素及运动轨迹。
对全部原始帧独立核验 ordinal、样本数、分段合计、日志 SHA-256 与源 / 二进制绑定，
见 `measure-v2/independent-summary.json`。本轮架构门、文档、clang-format 及差异检查通过，
没有重跑全量矩阵或将有限 GPU 诊断升级为 R3/R4 设备接受。

原始登记、runner 与日志原位保留；`final-manifest.json` 封存最终 probe、CMake、资产、库、
二进制、构建配置和各组证据哈希。新增探针仅供显式调用，不进入默认 CTest 设备接受。

## 初版夹具作废

初版复用了旧示例的相机 z=-10、identity rotation，物体在右手相机背面。
旧检查只确认命令数和 `presentationDrawn`，后者按命令非空置位，不证明物体入镜。
核对像素发现 `[14,16,18]` 恰为背景色后立即停止主组，撤回小样 FPS，原日志和登记不覆盖。
`pilot-v1/invalidated.json`、`measure-v1/invalidated.json` 记录原因；
`fixture-v1.cpp` / `.exe` 保存旧源和二进制。旧控制器中 valid 字段仅表示旧检查通过，
整组以 invalidated 处置为准，不进入本报告统计。

第二版修正相机、调整小三角形像素覆盖，并加入初末全部实体的 clip-space 检查和材质像素断言。
新组使用独立登记、全新进程和最终二进制，只有 v2 通过上述验证的样本可用于结论。
