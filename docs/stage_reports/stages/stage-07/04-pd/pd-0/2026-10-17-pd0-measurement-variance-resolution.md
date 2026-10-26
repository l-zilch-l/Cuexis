# PD-0 测量波动方案实施

状态：两窗口正式测量已完成；结果为 `inconclusive`，PD-0 尚未退出

本报告记录当前测量方案的实施边界。历史正式组和补充组的 `inconclusive` 结论保持不变；本方案只定义后续可复核的无插桩基线。

## 测量边界

正式 owner 基线使用 Release/Debug 与 legacy/RPA 的固定轨迹，每个配置在两个时段各运行三个独立进程，时段间至少十分钟。运行条件为正常分配器、编译插桩关闭、无 ETW、交流电和冻结的电源策略。15% 门禁仍使用每个时段三进程和全部六进程的 `(max-min)/median`，不删除慢样本，不替换失败组。

owner 结果分为三项：

- `validated_trace`：固定轨迹推进、FrameSnapshot 提取、正确性检查和 FrameDigest 的总耗时；
- `playback_only`：只包含固定轨迹推进，用于观察纯 Playback 成本；独立 fresh session 的完整轨迹仍用于正确性验证；
- `read_long`：独立长只读 Snapshot 块，作为长阶段计时和计时分辨率检查。

`validated_trace` 是完整行为指标，`playback_only` 是归因指标，二者不合并为一个结论。宿主 request 到 receipt 的均值、尾延迟和超预算次数单独报告，不进入 owner 重复性门禁。

## 调度与环境

pilot 只验证候选物理核心和读取批长。正式核心在 pilot 前按拓扑顺序登记为第一个候选核心；pilot 噪声分数只作为诊断信息，不再用于择优，避免根据观测结果选择有利样本。进程亲和性、优先级、电源节流状态和电源方案必须保持不变；当前处理器编号和处理器组只作为动态观测，不作为跨进程相等条件。

每个进程记录 QPC 起止边界、UTC 对齐、亲和性和处理器位置。短阶段的亚微秒结果只用于诊断，不单独触发性能结论；长块必须达到登记的最小时长。

## 失败处理

任一门禁超出 15% 即保持 `inconclusive` 并停止该注册组。只有在明确需要归因时才使用带插桩或 ETW/WPA 的独立诊断运行；诊断数据不混入正式基线，也不改写历史结果。PD-0 在新一轮完整正式基线和既有实现门禁均完成前不退出。

实现入口：`tests/pd/formal_baseline.py`、`tests/pd/measurement_method_tests.cpp`、`tests/pd/measurement_environment.hpp`。

## 2026-10-18 正式测量结果

注册文件：`out/pd0-formal/registration-20261017T150122Z.json`。第一窗口和第二窗口各包含每个配置三个独立进程；第二窗口在恢复注册时的电源设置指纹后完成，未重跑第一窗口，也未替换样本。

证据目录：

- `out/pd0-formal/window-1-20261017T150141Z-617fe2/`
- `out/pd0-formal/window-2-20261017T161346Z-205bfe/`

两窗口共 36 个正式进程，全部退出码为 `0`，长读块时长门禁通过。合并六进程的最大波动如下：

| 类别 | 最大波动 | 配置与指标 |
| --- | ---: | --- |
| owner | 41.82% | Debug / RPA / `playback_only` |
| owner | 25.92% | Debug / RPA / `read_long` |
| owner | 24.24% | Debug / RPA / `validated_trace` |
| host | 23.99% | Release / `request_to_receipt_ack_us` 均值 |

其余 owner 指标中，Release/RPA `validated_trace` 为 17.07%，Release/legacy `validated_trace` 为 15.32%，其余均低于 15%。由于至少一项 owner 和宿主门禁超出 15%，注册状态保持 `inconclusive`；历史失败结果不被本轮替代，PD-0 不退出。
