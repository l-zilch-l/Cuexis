# ADR 0029：Behavior Keyframe v1、绝对时间采样与 PlaybackSession 帧契约

日期：2026-07-22

状态：已接受

## 背景

ADR 0028 冻结了相机组件归属、FOV 语义和 FrameSnapshot 相机字段，但没有冻结 Behavior Track 从 Chart 文档到 Runtime 的最小求值闭环。阶段 1C 需要让 Player、headless consumer 和未来 Studio 对同一目标 `chartTimeMs` 得到相同的 Transform 与相机结果，同时保持宿主对时钟、窗口和后端的所有权。

## 决策

### 1. Chart Behavior v1

继续使用 `behavior.transform.keyframe` version 1，不提升 Chart 顶层版本。固定支持六个 Property：

```text
transform.position.x/y/z  finite scalar
transform.rotation        normalized Quaternion [x, y, z, w]
transform.scale            finite Vec3
camera.fovY                finite scalar，单位为度，范围 (0, 179)
```

每个 Track 至少一个 Key；Beat 使用规范 RationalBeat，Compiler 按 Beat 排序并预计算 `chartTimeMs`。同一 Behavior 不得重复 Property 或 Beat。Key 的 easing 属于目标 Key 控制的区间，v1 只允许 `linear`、`in_cubic`、`out_cubic`、`in_out_cubic`。标量/Vec3 使用分量线性插值，Quaternion 使用 shortest-path slerp，首尾时间钳制，单 Key 为常量。

Reader 对 canonical Behavior、Track、Key 的未知字段严格报错；Simple v1 对相同子树严格报错，其他 Simple 未知字段保持 warning/保留。任何非法 Track 必须在资源请求和 World 发布前失败。

### 2. RuntimeFrame 和更新事务

宿主通过以下值驱动 Session，Session 不拥有隐式时钟或 `seek()` 状态：

```cpp
struct RuntimeFrame final {
    double chartTimeMs;
    double simulationDeltaTimeMs;
    std::uint64_t timeDiscontinuityId;
};
```

`chartTimeMs` 必须有限，允许为负；delta 必须有限且非负。Discontinuity ID 只比较是否变化；ID 变化后的首帧 delta 必须为零。相同 ID 下向后移动时间是未声明 Seek，必须拒绝且不修改 World。

固定更新顺序为：

```text
Behavior absolute evaluate
-> PropertyWriteBuffer
-> Transform/FOV candidate resolver
-> atomic local-component commit
-> World transform update
```

Resolver 从 prepare 时捕获的初始值重建每帧候选值，不把上一帧最终值作为基线。Transform 和 FOV 必须整体校验通过后才提交，失败不得发布半帧结果。`camera.fovY` 绑定缺少 `cuexis.camera` 的对象在 prepare 阶段失败。

### 3. PlaybackSession 公共边界

`PlaybackSession` 是宿主唯一的播放门面；公共头不暴露 RuntimeSession、World、EnTT、SDL、OpenGL 或 JSON DOM。阶段 1C 冻结以下 C++20 操作：

```text
loadChart(string_view)
update(RuntimeFrame)
extractFrame(FrameViewport) -> owning FrameSnapshot
reload(replacement, targetFrame, ReloadPolicy)
unload()
```

`FrameSnapshot` 持有稳定 Object ID、World Matrix、Camera View/Projection、当前 FOV 和显式 viewport。Reload/Unload/后续更新不得使已返回 Snapshot 悬空。视口 aspect 只在提取时计算，不写入 Chart。

### 4. Player 组合

Player 在创建 SDL/OpenGL 前完成 Project、Asset Index、Chart 和 Behavior preflight。每帧只能执行：

```text
Player Clock -> PlaybackSession::update
-> PlaybackSession::extractFrame
-> RenderScene adapter
-> optional OpenGL backend
```

1C 默认 `stage1c_project` 不含 Renderable，使用三对象/三行为 fixture 展示 position、rotation、scale 和 camera.fovY；阶段 1B 的资源 fixture 继续作为回归数据，不建立第二条 Runtime 路径。

## 备选方案

### 运行时解析 Beat 和 Property 字符串

拒绝。每帧解析增加不可控分配和诊断差异，且无法保证 Player、headless 与不同帧率路径一致。

### 把上一帧 Component 作为下一帧基线

拒绝。结果会依赖帧率和更新历史，Seek 无法得到与直接目标采样相同的状态。

### Player 继续直接使用 RuntimeSession

拒绝。会重新建立 SDK 与 Player 两条行为路径，违反 ADR 0027 的公共边界。

## 影响与后续

阶段 2 在新 Behavior version 上采用 Behavior Event，事件使用 Chart Beat、端点值和端点斜率表达属性变化；运行时可以编译为内部 Segment。循环、离散属性和局部 BehaviorClip 组合仍由阶段 2 专项计划冻结。不得静默改变 v1 的采样结果。阶段 1D 只替换 RuntimeFrame 的时间来源；阶段 1E 再完成 ContentProvider 注入、安装导出和仓库外 consumer 门禁。稳定 C ABI、Judgement/Replay 和资源 Renderable 扩展不由本 ADR 提前冻结。
