# 阶段 1C 实施计划：时间、基础行为与 Headless Playback 闭环

状态：实现与最终验收完成；API、时间、Snapshot、采样和预算语义按 ADR 0028/0029 落地。260722 全量审查的 R01-R21 已于 2026-07-26/27 全部关闭，最终证据见审查报告与完成报告第 10 节
规划日期：2026-07-18；SDK 调整：2026-07-20；实现完成：2026-07-22
前置基线：[阶段 1B 完成报告](../stage_reports/stage_1b_completion_report.md)
后续阶段：[阶段 1D 实施计划](stage_1d_implementation_plan.md)

## 1. 阶段目标

阶段 1C 要完成以下纵向闭环：

```text
Chart Behavior typed Track
-> ChartCompiler 将 Beat 编译为 chartTimeMs
-> Runtime prepare 建立 BehaviorProgram / Entity Binding
-> BehaviorSystem 绝对时间采样
-> PropertyWriteBuffer
-> Transform/Camera PropertyResolver 从初始值重建局部 Transform 与相机 FOV
-> TransformSystem 更新世界矩阵
-> PlaybackSession 提取不可变 FrameSnapshot
-> headless consumer 或 Player Render adapter 消费
```

完成后，Entity 的位置、旋转和缩放，以及相机实体的垂直视场角 `fovY`，可以由 `chartTimeMs` 驱动；从任意历史直接跳转到同一目标时间，必须得到相同结果。外部 consumer 无需访问 RuntimeSession、World/EnTT、SDL 或 OpenGL，即可完成加载、更新、Seek、帧提取和卸载。

## 2. 推荐冻结的设计

### 2.1 Behavior 格式版本

继续完成 `cuexis.chart` v1 已预留的 `behavior.transform.keyframe` version 1，不提升 Chart 顶层版本。阶段 1A 对 `tracks` 的 opaque 接受只是分阶段加载能力，不代表任意 Track 已具有运行语义；1C 开始后，无效 version 1 Track 必须在 typed Reader/编译阶段稳定失败。

version 1 允许五个 Transform Property 和一个相机 Property。`camera.fovY` 是 ADR 0028 已接受的相机事件能力，不改变 `behavior.transform.keyframe` 的类型名或 Chart 顶层版本：

```text
transform.position.x  finite scalar
transform.position.y  finite scalar
transform.position.z  finite scalar
transform.rotation    normalized Quaternion [x, y, z, w]
transform.scale       finite Vec3
camera.fovY            finite scalar，单位为度，严格位于 (0, 179)
```

`camera.fovY` 只可绑定到具有 `cuexis.camera` Component 的对象；不支持通过 v1 Track 修改 `camera.type`、`near` 或 `far`。文档 Key 继续使用规范 RationalBeat。运行时 Key 在编译阶段一次性转换为 `chartTimeMs`，求值阶段不再执行 Beat 解析、字符串 Property 查找或 TimingMap 转换。

### 2.2 最小采样语义

version 1 只接受方案 B v1 已经声明的四种 easing，不开放通用 Curve、循环或外推配置：

```text
linear / in_cubic / out_cubic / in_out_cubic
easing          由目标 Key 声明，控制 (previous, current] 区间；缺省 linear
scalar / Vec3   easing 后的 t 执行分量线性插值；scalar 包含 camera.fovY
Quaternion      easing 后的 t 执行 shortest-path slerp，结果重新归一化
单个 Key       全时间常量
首个 Key 之前  钳制到首值
最后 Key 之后  钳制到末值
```

首个 Key 的 easing 没有入段可控制，必须省略。Track 输入顺序无语义；编译器按 Beat 排序。空 Track、重复 Beat、重复 Property、未知 easing、非有限值、非规范 Quaternion、越界 `camera.fovY` 和未知字段均为错误。同一 Behavior 的 Track 不得写入同一 Property 两次。绑定 `camera.fovY` 但对象不含 `cuexis.camera` Component 是 prepare 错误，不得静默跳过。

通用 `Curve<T>` 不作为阶段 2 的谱面层格式；阶段 2 通过新的 Behavior Event version 扩展连续属性和 Hermite 进度，并冻结 Material/Visibility 边界。ParentBinding、BehaviorClip、局部 Beat 和循环是否进入阶段 2 由现行 2E 门禁决定，不静默改变 version 1 的采样结果。具体 v3 字段见 `docs/CHART_FORMAT.md`。

### 2.3 RuntimeFrame

1C 冻结供 1D 复用的帧输入：

```cpp
struct RuntimeFrame final {
    double chartTimeMs;
    double simulationDeltaTimeMs;
    std::uint64_t timeDiscontinuityId;
};

struct FrameViewport final {
    std::uint32_t width;
    std::uint32_t height;
};
```

规则：

```text
chartTimeMs 必须有限，可以为负
simulationDeltaTimeMs 必须有限且 >= 0
Pause 和 discontinuity 后的首帧传入 0 delta
discontinuity ID 只比较“相同/变化”，0 合法，不要求连续递增
同一 ID 下 chartTimeMs 向后移动是未声明 Seek，update 失败且不修改 World
ID 变化后所有系统从目标绝对时间重采样，不消费跳转前 delta
Seek 属于 Clock/Timeline；PlaybackSession 和 RuntimeSession 都不生成时间或 discontinuity ID，1C 不新增隐式 `seek()` 状态接口
```

Behavior 本身每帧都执行绝对采样；discontinuity 还为未来粒子、状态机等状态型 System 提供统一通知边界。宿主通过新的 `RuntimeFrame` 提交 Seek，不通过 Session 内部时钟完成跳转。

## 3. 模块与所有权

| 数据或服务 | 定义模块 | 所有者与规则 |
| --- | --- | --- |
| typed Beat Key、Property enum、文档 Track | `cuexis_chart` | `ChartDocument`，不暴露 JSON DOM |
| 已排序的 ms Runtime Track IR | `cuexis_chart` | 不可变 `ChartRuntime` |
| `PropertyId`、`PropertyValue`、`PropertyWriteBuffer` | `cuexis_world` | 会话期数据，不序列化；PropertyId 可表达 camera.fovY，但不包含 Render 类型 |
| Transform 初始值与 Transform `PropertyResolver` | `cuexis_world` | Resolver 实例由 RuntimeSession 持有 |
| CameraComponent、FOV 初始值与 Camera FOV Resolver | `cuexis_render` / `cuexis_runtime` | Component 留在 Render；RuntimeSession 持有 FOV baseline 和 Resolver，消费 World 的通用 PropertyWriteBuffer，避免 `world -> render` 反向依赖 |
| specialized Track/Sampler、BehaviorProgram、BehaviorSystem | `cuexis_behavior` | active/prepared Session 持有 |
| Chart Track 到 BehaviorProgram 的有界转换 | `cuexis_runtime` | prepare 阶段完成 |
| `RuntimeFrame`、更新顺序和 discontinuity | `cuexis_runtime` | RuntimeSession owner thread；由 Playback 门面转交 |
| 第一版 PlaybackSession、FrameViewport 与 FrameSnapshot | `cuexis_playback` | 宿主公共门面；Snapshot 是拥有数据的不可变值，不暴露 Runtime/World/EnTT |
| ChartClock、Seek 操作和帧输入生成 | 宿主/Player 组合层 | 不由 PlaybackSession 或 RuntimeSession 隐式拥有 |

依赖方向保持：

```text
chart    -> core + json_support
world    -> core + EnTT
behavior -> core + world
runtime  -> chart + behavior + world + 既有模块
playback -> project + chart + assets + runtime + render 前端（cuexis_judgement 在阶段 11 成为必选依赖）
player   -> playback + clocks + optional backend
```

禁止新增 `chart -> behavior/world`、`behavior -> chart` 或 `playback -> SDL/OpenGL adapter`。Chart 与 Behavior 之间允许在 prepare 时做一次受预算约束的 typed IR 转换，以保持模块边界。安装后的 Playback 公共头不得包含 EnTT、SDL、OpenGL 或 JSON DOM。

## 4. Property 求值与更新事务

Transform PropertyResolver 与 Camera FOV Resolver 每帧从实例化时捕获的初始值开始：

```text
Initial Transform + initial Camera FOV
-> 应用 Behavior 绝对写入
-> 校验完整候选 Transform 与 FOV 范围
-> 一次提交局部 Transform 与 CameraComponent.fovY
-> 更新父子世界矩阵
```

稀疏 Property 只替换对应字段。例如只有 `position.x` Track 时，`position.y/z` 来自初始值；无 `camera.fovY` Track 时 FOV 保留 CameraComponent 初始值。不得把上一帧最终 Component 当作下一帧基线。Transform 与 FOV 的全部候选必须先完成校验，任一候选无效时均不得提交任一 Component。

prepare 必须先完成 Behavior 结构和预算校验，再请求 ResourceScope。无效 Track 不得触发资源 I/O。update 在非法帧、线程错误、写入冲突或非有限结果时不发布半帧结果；逐帧诊断使用固定预算并替换/汇总，不形成无界历史。

1C 只建立内部 Transform 与 Camera FOV 基线；`BasePropertyCommand`、Animation Layer、HostOverride 和 Studio Preview Token 保留既有协议边界，不在本阶段发布公共修改 API。Snapshot 采用拥有数据的不可变值，Reload/Unload/下一次 update 不使已返回的 Snapshot 悬空。

## 5. 实施批次

### 1C-0：契约与 ADR

- 新增 Transform Keyframe v1 与采样语义 ADR，并与 ADR 0028 的 `camera.fovY` Track 和 FrameSnapshot 相机契约一致。
- 扩充 RuntimeSession/Timeline ADR，冻结 `RuntimeFrame`、Pause、Seek 和 discontinuity 错误契约。
- 更新 `CHART_FORMAT.md`、Chart v1 Schema、示例与预算说明。
- 明确阶段 2 从 specialized Track 泛化，不重复实现 1C。

### 1C-1：Chart typed Track 前端

- 将 `ChartBehavior::tracks` 从 opaque JSON 替换为 typed Transform Track。
- 对六种 v1 Property、value、Key、未知字段和数量进行 typed Reader 校验；`camera.fovY` 执行 (0, 179) 度范围校验。
- 保持方案 A/B 的统一加载路径；Simple v1 继续支持 scalar position Track 和既有四种 easing，不新增 Quaternion/Vec3 简写。
- Schema artifact 与 typed Reader 使用相同白名单和结构。

### 1C-2：ChartRuntime 编译

- 编译期稳定排序 Track/Key，并检测重复 Beat、重复 Property 和冲突。
- 使用 `TimingMap::beatToChartTimeMs` 预计算 Runtime Key；`offsetMs` 不重复进入 Track 时间。
- 输出与输入数组顺序无关的 Runtime Track IR。
- 在总 Key/Track 预算耗尽时停止并追加唯一的 limit diagnostic。

### 1C-3：World Property 与 BehaviorSystem

- `cuexis_behavior` 从 INTERFACE 升为 STATIC。
- 在 World 中实现 PropertyWriteBuffer、Transform baseline 与 Transform PropertyResolver；在 Runtime/Render 边界实现 Camera FOV baseline 和 Resolver，不让 World 依赖 CameraComponent。
- 实现无状态绝对 Sampler；Key 定位使用二分查找，复杂度 `O(log K)`。
- BehaviorBinding 使用稳定 Object/Behavior 索引，不依赖 EnTT view 顺序决定结果或诊断。

### 1C-4：RuntimeSession update

- Prepared Session 构建 BehaviorProgram、Binding、baseline 和预分配 scratch。
- 增加 owner-thread `update(const RuntimeFrame&)` 及 bounded result/diagnostics。
- 固定更新顺序为 Behavior evaluate -> Transform/FOV property resolve -> Transform world update；FrameSnapshot 按解析后的当前 FOV 和宿主视口计算投影矩阵。
- reload 接收显式目标 `RuntimeFrame` 与 `KeepChartTime` / `RestartAtZero` 策略；Replacement 先按目标帧执行一次 `delta=0` 绝对采样，成功后才完整替换 Behavior 状态。失败保留旧 World、Scope、baseline、最后有效帧和活动诊断。
- unload 继续遵守 World -> ResourceScope，并先停止 update/extract。

### 1C-5：PlaybackSession 与 headless 帧输出

- 建立第一版 `cuexis_playback` C++ 门面；具体类型名在 1C-0 评审冻结。
- 组合 Project/Chart prepare、内部 RuntimeSession commit/update/reload/unload 和结构化 Diagnostics。
- 宿主通过显式 `RuntimeFrame` 驱动；同一 Session 保持 owner-thread 规则，多 Session 不共享隐式当前状态。Seek 通过目标时间和变化的 discontinuity ID 表达。
- `extractFrame(const FrameViewport&)` 返回拥有数据的不可变 FrameSnapshot；CameraSnapshot 必须包含当前 FOV、世界到视图矩阵和按显式视口计算的投影矩阵。不得暴露 RenderScene 可变容器、World、EnTT Entity 或后端对象。
- 增加无 SDL/OpenGL 的 headless fixture，验证 load/update/host-driven seek/reload/extract/unload，以及旧 Snapshot 在 Reload/Unload 后仍可安全读取。

### 1C-6：Player 与 demo

- 新建独立 `stage1c_project`，保留阶段 1B fixture 作为回归数据。
- Player 在创建 Window/OpenGL 前完成无 GPU 副作用的 Project、Asset、Chart 与 Behavior preflight。
- 无音频模式使用 Player 所有的 ChartClock；交互运行使用稳定时钟，smoke 使用确定的绝对时间序列，再统一提交 PlaybackSession。
- 每帧执行 PlaybackSession update -> extractFrame -> OpenGL adapter render，不再直接访问 RuntimeSession/World。
- demo 至少分别展示 position、rotation、scale 和 camera.fovY；Seek 后日志给出目标时间与确定采样值。

### 1C-7：门禁与交付

- 更新 target allowlist、依赖扫描、BUILDING、PROJECT_GUIDE、RUNTIME_SESSION、SDK 转型方案和 Chart 文档。
- 建立 Windows/MSVC 托管 CI：configure、build、CTest、format、architecture；GPU 不进入托管 CI。
- Debug/Release fresh configure + clean build + 完整 CTest + format + architecture。
- 执行 headless Playback、默认 1C Project 与阶段 1A canonical/simple 的 Debug/Release GPU smoke，并对比 Frame hash。
- 创建 `docs/stage_reports/stage_1c_completion_report.md`，记录实际测试数、预算、CI 状态、GPU 输出和残余风险。

## 6. 默认安全预算

推荐代码级硬上限：

```text
Behavior definitions            10000（沿用）
v1 tracks / Behavior            6（五项 Transform + camera.fovY）
Property path                   128 bytes
Keys / Track                    65536
Total Behavior keys / Chart     262144
Behavior-bound objects          100000（受 Object 上限约束）
Property writes / frame         600000
Diagnostics                     1024
FrameSnapshot commands          复用/收紧 RenderScene 既有硬上限；具体值在 1C-0 确认
```

这些是输入耗尽防护，不是 ProjectConfig、UserPreferences 或 DeviceProfile 字段。每帧禁止 JSON、Property 字符串解析、资源 I/O 和无界分配；写入/解析 scratch 在 prepare 时一次预分配。

## 7. 测试矩阵

| 范围 | 必须覆盖 |
| --- | --- |
| Reader/Schema | 六种 v1 Property、四种 easing、错误 value shape、NaN/Inf、`camera.fovY` 越界、未知字段、空 Key、超限 |
| Compiler | 乱序归一化、重复 Beat/Property、负 Beat、offset 不重复应用、输入排列不变性 |
| Sampler | 单 Key、精确 Key、首尾钳制、四种 easing、标量（含 FOV）/Vec3、Quaternion 符号等价和 shortest path |
| Resolver | Transform/FOV 稀疏写入保留基线、每帧重置、FOV 范围、冲突不依赖遍历顺序、失败无部分提交 |
| RuntimeFrame | 首帧、Pause、正常前进、声明/未声明向后 Seek、非法时间、错误线程 |
| Session | prepare 零资源副作用、reload 成功/回滚、unload、跨 Session owner 校验 |
| 确定性 | 直接采样与多帧到达同一时间相等；30/60/144 FPS 路径相等 |
| Playback | headless、公共门面生命周期、多 Session、host-driven seek、Reload 目标帧、拥有型 Snapshot 生命周期、FOV/视口投影、无后端依赖、公共头泄漏扫描 |
| Parity | Playback headless 与 Player 对相同输入的 Runtime/Frame hash 一致 |
| Player | 只经 PlaybackSession 每帧提取、1C 默认 Project、1A A/B 回归、3 objects/9 Debug commands |

## 8. 验收标准

```text
Entity 可以由 chartTimeMs 驱动 position、rotation 和 scale；相机实体可以由 chartTimeMs 驱动 fovY
任意目标时间的结果不依赖上一帧、帧率、输入数组或 ECS 遍历顺序
Seek/discontinuity 后一次 update 得到与直接采样相同的 Transform 与 FOV/投影
非法 Track/RuntimeFrame 不产生资源 I/O 或部分 World 更新
Chart、Behavior、World 和 Runtime 依赖方向不反转
ProjectConfig/Asset Index 不增加 Behavior 或 Timing 配置字段
宿主不访问 RuntimeSession、World 或 EnTT 即可取得帧输出
不构建 SDL/OpenGL 时 headless Playback 闭环通过
Player 只调用 PlaybackSession，不保留应用私有 Runtime 路径
Debug/Release、CTest、format、architecture、GPU 与 CI 门禁完成
```

## 9. 明确非目标

```text
AudioClock、AudioTransport、音频设备与 WAV
BPM Changes、Stops 和 TimingMap 完整逆映射
Behavior Event、Tempo Event、循环和 BehaviorClip 组合
Material、Visibility、ParentBinding 与事件 Track
Animation Layer、OverrideToken 和公开 BasePropertyCommand
正式资源格式、Mesh GPU 绘制、异步资源与 Studio
ContentProvider、install/export/find_package 和仓库外 consumer 的后续实现与验收（阶段 1E）
稳定 C ABI、语言绑定和 Unity/Unreal adapter（阶段 12；阶段 11 完成后）
正式 InputEvent、JudgementResult 和 ReplayData（阶段 11）
```

## 10. 配置整合

阶段 1C 不新增持久化配置文件。Behavior 参数、Key、Property 和 Timing 全部属于版本化 Chart/Behavior 数据；限额是代码安全上限。ChartClock 属于宿主/Player 运行时组合，不保存到 ProjectConfig 或 UserPreferences。PlaybackSession 只接收显式 typed 创建输入，不建立全局 SDK 配置。

## 11. 待确认选择

1. 激活已预留的 `behavior.transform.keyframe` version 1，不提升 Chart 顶层版本。
2. version 1 采用五个固定 Transform Property 加 `camera.fovY`、既有四种 easing、线性/slerp 和首尾钳制；FOV 遵循 ADR 0028 的 (0, 179) 度范围。
3. `RuntimeFrame` 使用绝对 `chartTimeMs`、`simulationDeltaTimeMs` 和仅比较变化的 discontinuity ID。
4. 采用总 Key `262144`、单帧 PropertyWrite `600000` 的初始硬上限。
5. 将 Windows/MSVC 非 GPU CI 纳入 1C 完成门禁。
6. 冻结第一版 PlaybackSession 的最小 C++ 所有权/线程/错误语义，但不承诺稳定 C ABI。
7. `extractFrame(FrameViewport)` 使用拥有数据的不可变 FrameSnapshot；Reload/Unload 不使已返回 Snapshot 悬空，预算与 RenderScene 转换规则在 1C-0 冻结。
8. 不新增隐式 PlaybackSession/RuntimeSession `seek()`；宿主通过 `RuntimeFrame` 和 discontinuity ID 表达 Seek。
9. Reload 使用显式目标 RuntimeFrame 和 KeepChartTime/RestartAtZero 策略，候选 Session 先按目标帧 delta=0 重采样再原子交换。
10. Behavior 绑定对象缺少 Transform，或 camera.fovY 绑定对象缺少 cuexis.camera，均在 prepare 阶段作为错误拒绝。
11. Simple 格式的 Behavior/Track/Key 子树使用严格未知字段错误；其他 Simple 未知字段继续 warning 并保留。
12. 确认 1C 暂时复用现有 Filesystem 加载，ContentProvider 完整改造留到 1E。

## 12. 向阶段 1D 的交接

1D 只能替换时间来源，不修改 Behavior/Resolver/Seek 语义：

```text
AudioClockSnapshot.positionMs
-> Timeline: audioTimeMs - offsetMs
-> RuntimeFrame
-> PlaybackSession::update
-> internal RuntimeSession::update
```

交接测试必须证明 `offsetMs = 250` 且音频位置 `250 ms` 时得到 `chartTimeMs = 0`；Seek 后只改变目标时间和 discontinuity ID，Transform 与 Camera FOV 同时和直接目标采样完全一致。1D 只能替换/提供时间来源和音频内容 adapter，不能绕过 PlaybackSession 建立 Player 私有 Runtime 路径。
