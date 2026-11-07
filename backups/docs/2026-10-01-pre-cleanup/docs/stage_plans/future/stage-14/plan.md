# Stage 14 Implementation Plan: Stable ABI and Playback SDK v1

状态：future；未开始

更新日期：2026-10-01

这是原 Stage 12 稳定 ABI 计划的重新编号版本。旧路径保留为兼容说明。

## 1. 阶段目标

在 C++ preview、Gameplay/Replay 生命周期、发行格式和目标平台证据稳定后，冻结稳定 C ABI，
提供薄 C++ RAII wrapper，并发布正式 Cuexis Playback SDK v1。

## 2. 前置条件

- Stage 7A Input/Judgement/Replay 公共生命周期、external consumer 和确定性回放门禁完成。
- Stage 8 Chart v5 正式发行合同完成；纳入 v1 的 Stage 7B+ capability 已明确。
- Stage 9、Stage 11、Stage 12 交付的 Presentation、性能和目标平台矩阵已被接受。
- 公共 FrameSnapshot、JudgementResult、ReplayData、ContentProvider 和错误生命周期无开放语义问题。

## 3. 工作范围

- opaque handle、allocator、字符串、数组、回调、快照有效期、线程 owner、重入、取消和释放合同。
- 独立 C ABI version、符号可见性、capability 查询、兼容和弃用政策。
- Windows CRT、Debug/Release、static/shared 和支持平台矩阵。
- 至少一个正式宿主 adapter、纯 C external consumer、安装/升级/部署/许可证文档。
- 不增加第二套 Playback、Judgement、Replay 或 Presentation 语义。

## 4. 关闭标准

- C consumer 不依赖 C++ 标准库、异常、RTTI 或第三方实现类型。
- 正反例覆盖对象、错误、回调、快照、版本拒绝和二进制兼容。
- C++ wrapper 与 C consumer 对相同输入产生相同 FrameSnapshot/Judgement/Replay 结果。
- `1.x` 版本政策有接受的 ADR；实际 `1.0.0`、同 SHA 证据和 owner acceptance 完整。

## 5. 明确不包含

- 宿主插件 ABI、编辑器 ABI 或渲染后端 ABI，除非另有 ADR。
- 暴露 RuntimeSession、World、EnTT、SDL、OpenGL、JSON DOM 或宿主引擎类型。
