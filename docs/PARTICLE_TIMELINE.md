# Cuexis Particle Timeline 规范

状态：阶段 8 设计已接受

更新日期：2026-07-20

## 时间域

Emitter 显式选择：

```text
ChartTime  随 chartTimeMs，支持暂停和任意 Seek
LocalTime  由所属运行逻辑推进，但仍响应 Session discontinuity
```

谱面表现粒子默认 ChartTime。暂停时 simulationDeltaTimeMs 为 0。

## 确定性

CPU 粒子使用固定步长，默认 120 Hz。步号从目标时间直接计算，不累加近似的 8.333ms 浮点值。

随机序列由以下内容确定：

```text
chartId
ChartObjectId
EmitterAsset seed
particleSimulationVersion
```

随机算法和 simulationVersion 必须固定。修改随机算法、发射顺序或积分规则时递增版本。可以采用成熟 PCG 实现，但必须固定版本并封装在 Particle 模块内。

## Seek 与倒放

粒子不执行负 delta 反向积分：

```text
小幅向前：按固定步继续模拟
向后或大幅跳转：恢复目标之前最近 Checkpoint，再正向重放
无 Checkpoint：从 Emitter 激活时间重放
目标早于激活时间：空状态
```

Checkpoint 是 Runtime 缓存，不序列化到 Chart。默认每 1000ms 建立候选检查点，并受 LRU 内存预算限制。是否存在 Checkpoint 不得改变最终粒子结果。

重放超过单帧预算时进入 Rebuilding。Player、Studio 或嵌入宿主可以根据 FrameSnapshot 中的明确状态暂时不显示该 Emitter，分帧完成重建后一次发布目标状态；不得跳过模拟步骤并显示近似但不可复现的结果。

## Checkpoint 内容

```text
simulation step index
RNG state
Emitter 累积发射状态
所有存活粒子的 CPU 状态
simulationVersion
```

GPU Buffer 和后端对象不进入 Checkpoint。粒子表现数据在恢复完成后重新提取，并作为
`FrameSnapshot` 的版本化粒子扩展字段交给宿主/内建渲染 adapter；Stage 8 不建立第二套公共帧。

## Reload

资源 contentRevision 改变、Emitter 参数改变或 simulationVersion 不匹配时，相关 Checkpoint 全部失效并从激活时间重建。其他 Emitter 不受影响。

## 后续 GPU 粒子

GPU 粒子必须明确是否提供与 CPU 相同的确定性与 Seek 能力。不能用 GPU 实现替换 CPU 路径后静默破坏 Studio 任意时间预览。

粒子是 Cuexis 表现扩展，不是通用游戏粒子引擎。宿主能力不足时必须依据 portable/built-in/host-specific profile 稳定失败或使用显式受控降级；不得要求 Playback 核心依赖特定图形 API。
