# R4 收尾工具实施与准入复核（2026-11-13）

状态：implementation and diagnostic evidence；不是正式计时、内存预算或退出接受

返回 [New RPA 报告索引](../README.md)。本轮授权见
[收尾裁定](2026-11-13-closeout-decisions.md)。

## 1. 已实施范围

当前收尾分支集成 CPU/device 独立协议身份、设备 capture 的有限准备与正式 runner、独立
P3 stream 准备、控制绝对门、Seek 实现及其环境锚点。历史 CPU 台账键不变；P5 仍只用了
前三次，新豁免未消费。P3 的原始 `gpu-stream-v1` 不与 capture matrix 互相代验。

设备容量用例使用真实 SDL/OpenGL renderer；内存构建新增 prepare 检查点、逻辑 GL 存储与
独立进程 GPU residency 的 QPC 阶段。normal、memory、stage-timing 与 player-trace 构建身份
严格分离。stage/GPU observer 工具固定三轮独立进程，披露开销且不从 normal 时间扣除。

Player trace 连接实际动作、确认、音频、计分和 Replay；终态/EOF 后的冻结帧不增加正式帧
样本，并单列 active duration 与 terminal wait。单进程采集器始终标明功能证据，不冒充
三窗正式性能、600 秒 retention、听音或实体输入。

## 2. 本机诊断与验证

原始现场保留在 `out/evidence/r4/closeout-20261113/`；这些 dirty scratch 构建没有正式绑定，
不得作为后续候选的正式测量结果。

- memory-only scratch configure/build 均成功，设备与 host 两个目标已编译。
- trace scratch 的首个构建发现 `/WX` 下未使用参数，修复后增量编译成功。
- 设备 n64/high 诊断一进程完整结束，prepare/active/close/release 检查点、GL 与进程 GPU
  采样可解析。其小规模 GPU 读数在 reference ceiling 内，不外推到 n2048 或 P8。
- DFJK 原始诊断暴露端点查询 COM apartment 与 SDL 主线程冲突。修复借用既有 apartment、
  只平衡本函数成功初始化的引用，并保留具体 trace 错误后，完整轨迹验证成功：38 次捕获、
  46 分、21 命中、0 misses，Replay 一致并完成终态。
- 新设备盘点确认三种默认音频角色均为活动 Senary Audio 端点；这不能代替实际听音。

诊断目录分别为 `memory-path-diagnostic/`、`player-path-diagnostic/`、
`player-path-diagnostic-v2/` 和 `player-com-fix-check/`；所有原失败日志保留。

## 3. 内存准入仍阻断

n64/high 诊断的 allocation-origin 比较保留以下值，原 caps 不改：

| 检查点/域 | 原点请求字节 | reference ceiling |
| --- | ---: | ---: |
| after-host-load-prepare / state.candidate | 330,093 B | 327,680 B |
| active / state.current | 427,015 B | 196,608 B |
| after-close / state.current | 427,015 B | 196,608 B |

累计 simultaneous origin peaks 为 candidate 489,134 B、current 606,827 B；不能跨域或跨检查点
相加来制造新的产品峰值。最终 product C++ 请求归零及已跟踪 World 子集释放有证据，但
完整 ownership/lifetime 图尚未闭合；allocation-origin 超额不直接等于持久 World 的语义超额。

复核发现，最初容量 preparation 会把同输出、精确原点分区及检查点存在直接写为
semantic/pilot/environment/observer 四份 `passed=True`。这不满足语义准入，而且 n64/high
qualification 不能自动认证 n2048/low、P8 或新增 residency hold 模式。该路径在本轮正式
claim 前被发现并改为 fail-closed：当前 memory/P8 不能正式登记；qualification 仅作补证，
不得伪造完整图或把下界在声明内写成完整预算通过。

下一步继续原数字下的有限归属/释放补证，并将仍不能闭合的具体限制带入最终门账。
若必须改变预算、适用范围或取得额外额度，应提交具体材料供 owner 决定；现有自动授权
不构成这类放宽。其他无依赖自动单元继续执行。

## 4. 后续验证边界

首次干净实施提交为 `fb760fd6`，普通 Release 绑定已完成，三平台 hosted 已触发。
其后规格复核发现 stream observer 工具误把 15% 重复性规则用作开销准入阈值，因此
中止其余绑定构建，保留全部现场并先修正工具。四阶段沿 2026-11-02 已接受的可开关归因、
同负载与开销披露例外登记；GPU 时间保留 observer_unvalidated，不自行增加数值门槛。
该中止没有消费任何正式额度，后继候选使用新构建目录。

后继 `cf962765` 完成 normal 与 memory 绑定，但 stage-timing 构建出现 MSVC C1060。
现场物理可用内存仅约 8 MiB，停止后恢复约 25 GiB；失败指向 stage Samples 的大型数组
初始化。原观察器在每个编译单元展开大量无用零初始化，而实际输出仅读取 `[0,count)`，
且 `observe()` 先写完三个字段再递增 count。本轮移除样本数组及其外层的显式零初始化，
保留全部计数器初始化、量程和读取拒绝条件；normal 编译路径不启用该代码。
独立 MSVC `/O2 /W4 /WX` 编译与运行已核验所有初始计数为零、单次写入/计数/关联 ID
及 JSON 输出一致，原始记录在 `stage-init-check-v2/`。先前缺少 cl.exe 搜索路径的检查
现场也保留在 `stage-init-check/`，不冒充编译失败或性能结果。后继仍须完成完整绑定构建。

本报告不认证尚未执行的 P3、device capture、P5 三窗、observer 资格或最终 F1/F2。
干净候选冻结后使用全新构建与证据目录；正式测量不与本机构建/CTest 并行，旧结果和失败
不替换。历史 F1/F2 不能认证本轮实现。合并、发行及阶段关闭不在本轮代理授权内。
