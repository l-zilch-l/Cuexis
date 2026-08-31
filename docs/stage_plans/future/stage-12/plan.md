# Stage 12 Implementation Plan: Stable ABI and Playback SDK v1

状态：future；未开始

更新日期：2026-08-10

归档来源：[旧版 PROJECT_GUIDE](../../../archive/PROJECT_GUIDE_LEGACY_2026-08-10.md) 与
[SDK transition plan 快照](../../../archive/CUEXIS_SDK_TRANSITION_PLAN_2026-08-10.md)。

## 1. 阶段目标

在 C++ preview 和 Judgement/Replay 公共生命周期获得真实 consumer 证据后，冻结稳定 C ABI，
提供薄 C++ RAII wrapper，并发布正式 Cuexis Playback SDK v1。

## 2. 前置条件

- Stage 11 的 Input/Judgement/Replay 合同、实现、external consumer 和确定性回放门禁全部完成。
- Stage 1E 与 Stage 6 已积累 C++ 所有权、线程、错误、部署、升级和真实宿主证据。
- 公共 FrameSnapshot、JudgementResult、ReplayData 和 ContentProvider 生命周期无开放语义问题。

## 3. ABI 工作范围

- 冻结 opaque handle、allocator、字符串、数组、回调和快照有效期。
- 定义独立 C ABI version、符号可见性、capability 查询、兼容和弃用政策。
- 规定 Result/error、诊断、线程 owner、重入、取消和资源释放合同。
- 验证 Windows CRT、Debug/Release、static/shared 和支持平台矩阵。
- 提供不增加第二套语义的薄 C++ RAII wrapper。
- 提供至少一个正式宿主 adapter 和纯 C external consumer。
- 发布完整集成、升级、部署、许可证和符号文档。

## 4. 版本边界

以下版本独立演进，任何一个变化都不得隐式升级其他版本：

- 项目显示版本。
- C++ SDK API version。
- C ABI version。
- Chart、Project、Asset Index、CXC/CXT 和 ReplayData 内容格式版本。

## 5. 验收标准

- 纯 C consumer 不包含 C++ 标准库、异常、RTTI 或第三方实现类型。
- ABI 对象创建、销毁、错误、回调和快照生命周期具有正反例测试。
- 不同支持编译器/运行库组合按兼容矩阵接受或稳定拒绝。
- 旧 minor consumer 与兼容的新 SDK 通过二进制兼容测试；不兼容版本在加载前失败。
- static/shared、Debug/Release、Windows CRT 和支持平台矩阵全部通过。
- Playback、Judgement 和 Replay 的 external consumer 只使用安装产物。
- package、符号、license/NOTICE、部署和升级文档完整。
- C++ RAII wrapper 与 C ABI consumer 对相同输入产生相同 FrameSnapshot/Judgement/Replay 结果。
- SDK v1 发布不改变既有内容格式 identity 或迁移语义。

## 6. 明确不包含

- 在证据不足时冻结宿主插件 ABI、编辑器 ABI 或渲染后端 ABI。
- 通过 C ABI 暴露 RuntimeSession、World、EnTT、SDL、OpenGL、JSON DOM 或宿主引擎类型。
