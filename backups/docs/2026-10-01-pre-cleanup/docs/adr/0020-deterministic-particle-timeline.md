# ADR 0020：确定性粒子时间轴与 Seek

日期：2026-07-17

状态：已接受

## 背景

粒子是状态型模拟，简单 delta 积分无法支持 Studio 倒放和音频 Seek。

## 决策

CPU 粒子使用版本化随机种子和 120Hz 固定步长。向后或大幅 Seek 通过 Checkpoint 恢复并正向重放，不做负 delta 积分。Checkpoint 受 LRU 预算管理且不影响最终结果。

超出单帧预算时进入 Rebuilding，完成后发布精确状态，不跳步近似。

## 备选方案

反向积分不能可靠恢复出生/死亡和随机事件；每次从零同步重放会卡顿；跳过步骤会破坏确定性，均不采用。

## 影响

Emitter 资产增加 seed、time domain 和 simulationVersion；测试需要跨帧率和 Checkpoint 比较结果。

## 后续风险

Checkpoint 内存可能较大，需要按设备预算调整间隔和 LRU，但不能改变模拟语义。
