# Stage 13 Implementation Plan: Advanced Presentation and Chart v8

状态：future；未开始

更新日期：2026-10-01

前置：

```text
Stage 8 Chart v5 production baseline
Stage 9 Chart v6 / Model v1 baseline
Stage 11 Chart v7 single-axis presentation baseline
Stage 12 scale and target-platform evidence for the selected presentation profile
```

本阶段吸收原 `chart-format-update-for-v8` 延期计划，并接收确定性粒子提案。它只扩展
Presentation 和表现格式，不改变 Input、Judgement、Score、Replay 或既有 Chart v5/v6/v7
语义。Stage 13 是可选的高级表现线；Stage 14 不必等待全部未来表现能力完成。

## 1. 阶段目标

交付受控的高级表现能力：

```text
Chart v8
  -> 双轴贝塞尔形变
  -> 模型基本动画预设
  -> 可执行的 SDK 内置后处理
确定性粒子
  -> ChartTime/LocalTime
  -> 固定步长和版本化随机
  -> 有界 Checkpoint 与 Seek 重建
```

## 2. 工作批次

### S13-A：双轴贝塞尔

- `mappingAxes` 恰好为两条互不相同的 X/Y/Z 轴；第三轴保持欧氏偏移。
- 继续使用分段三次 Bézier；拒绝 `formula`、自由表达式和三轴映射。
- 采样只依赖绝对时间和当前局部 TRS；Seek、Reload、不同帧率必须一致。
- 沿用 Stage 11 的单轴 identity 和标架规则；合成顺序由独立 ADR/Spec 冻结。

### S13-B：模型基本动画

- 复用既有 Behavior/AnimationClip 白名单：position、rotation、scale、visible、alpha、材质槽。
- 提供少量冻结的 SDK 预设，不建立骨骼、morph 或运行时控制点生成。
- 预设必须拥有 capability、资源 identity、预算和 headless golden。

### S13-C：内置后处理

- 执行 Stage 11 冻结的包根 `shader.json` 接口。
- 只允许 SDK 内置效果与有类型参数；首批至少包括 identity、fade、vignette、color-multiply。
- Player 与 in-tree OpenGL adapter 执行；未知效果、参数越界或能力缺失稳定拒绝。
- 后处理不得改变 Judgement/Replay；是否进入 FrameDigest 由 ADR 明确分离。

### S13-D：确定性粒子

- 默认使用 ChartTime，允许显式 LocalTime；暂停、Seek、discontinuity 语义固定。
- 固定步长、版本化 RNG、绝对时间重建；Checkpoint 只作运行时缓存，不写入 Chart。
- 重建超出预算时进入可诊断的 rebuilding 状态，不显示不可复现近似结果。
- GPU 路径不得静默替代具备确定性要求的 CPU 语义。

## 3. 关闭标准

- v8 ADR、Spec、Schema、Reader/Writer 和 capability 矩阵先于生产实现。
- v7 单轴 golden、空 `shader.json` 和无粒子输入回归保持不变。
- 双轴、后处理和粒子均有 headless/adapter golden、Seek/Reload、预算和拒绝测试。
- 不支持能力稳定拒绝，不降级为 v7 或普通 Transform。
- 合法 v4-v7 输入的既有 digest、Judgement、Score 和 Replay 结果不变。
- CXC closure、FrameSnapshot 扩展和跨平台 identity 有完整 owner acceptance。

## 4. 明确不包含

- 任意运行时脚本、用户 shader 源、Shader Graph、三轴形变、骨骼、morph。
- 修改 Chart v5 核心语义或把表现轨迹当成 Judgement 轨迹。
- 稳定 C ABI；该合同属于 Stage 14。

## 5. 交接

交付 Chart v8 candidate/formal capability、双轴 identity、内置后处理目录、粒子时间域与
重建合同。未纳入发行矩阵的高级效果继续稳定拒绝。
