# ADR 0027：Cuexis Playback SDK 产品边界与宿主职责

日期：2026-07-20

状态：已接受

## 背景

现有阶段 0 至 1B 已建立 Chart、Runtime、World、Assets、Render 前端和独立 Player 的基础闭环，但项目长期描述仍以构建完整音乐游戏引擎为方向。该方向会要求 Cuexis 重复建设通用场景、UI、平台外壳和游戏流程，也会使 Player 的 SDL、OpenGL、文件系统与主循环假设固化到公共使用方式中。

Cuexis 的核心价值是处理和播放 Cuexis 谱面及资源，并提供确定性的时间、表现、判定、计分和回放能力。其他游戏引擎、应用与工具应能把这些能力作为第三方依赖使用。

## 决策

Cuexis 的正式产品结构为：

```text
Cuexis Playback SDK  可嵌入的谱面处理与播放核心
Cuexis Player        使用 SDK 和可选 SDL/OpenGL 后端的独立参考播放器
Cuexis Studio        使用同一 SDK 预览的独立编辑程序
```

Playback SDK 通过高层播放会话门面编排 Chart、Assets、Runtime、Behavior、Render 前端以及后续 Judgement。现有 `RuntimeSession` 和 EnTT World 是 SDK 内部实现，不是宿主集成接口。公共 SDK 不暴露 EnTT、SDL、OpenGL、JSON DOM 或其他第三方实现类型。

宿主负责主循环、平台生命周期、原始输入采集、游戏状态和 UI。宿主可以提供内容 Provider、绝对时间和渲染适配器，也可以选择 Cuexis 的 Filesystem、SDL Audio 和内建渲染适配器。SDK 必须支持无窗口、无 SDL、无 OpenGL 和无物理音频设备的 headless 使用方式。

`cuexis_judgement` 是 Playback SDK 的必选交付模块：宿主提交标准化 InputEvent，SDK 计算并返回判定、分数、连击和统计，同时支持输入记录与确定性回放。纯播放或 Studio 预览不提交 InputEvent 时，该模块休眠且不影响表现求值。

ProjectConfig 与固定文件布局继续作为 Player/Studio 的标准项目入口。嵌入 SDK 同时允许 typed/memory project source 和宿主 ContentProvider；非文件系统 Provider 不受物理路径 containment 规则约束，但其字节仍按不可信输入执行格式和预算校验。

第一版第三方消费以 C++20 门面、组件化 CMake package、FrameSnapshot/RenderPacket 和通用 external consumer 为主。稳定 C ABI、语言绑定与特定游戏引擎插件在生命周期通过真实消费验证后冻结。新增阶段 1E 作为安装、导出和仓库外消费门禁。

## 依赖方向

```text
host / player / studio -> cuexis_playback
cuexis_playback -> project + chart + assets + runtime + judgement + 前端契约
filesystem / SDL audio / OpenGL / host adapters -> 对应前端模块
runtime / chart / judgement -X-> SDL / OpenGL / host engine SDK
```

Player 和 Studio 不得各自建立私有的 Chart -> Runtime 路径。可选适配器失败不能改变核心 SDK 的平台无关语义。

## 备选方案

### 继续建设完整游戏引擎

拒绝。它会扩大到 UI、场景、平台发布和通用游戏流程，偏离 Cuexis 格式与播放核心，并重复成熟宿主引擎已有能力。

### 直接把现有 Player 源码嵌入宿主

拒绝作为正式方案。它会把命令行、SDL 生命周期、窗口、OpenGL Context、物理路径和宿主主循环耦合在一起，只适合短期原型。

### 立即冻结跨语言稳定 ABI

拒绝。当前 PlaybackSession、帧输出、内容 Provider 和回放格式尚无外部消费证据；先冻结 C++ 所有权与线程语义，再在阶段 6 冻结 opaque handle C ABI。

## 影响

阶段 0 至 1B 的内部实现与历史报告继续有效，但需要通过阶段 1C 至 1E 增加 PlaybackSession、headless 帧输出、ContentProvider、可选音频模式和正式安装包。后续动画、粒子、材质和 Shader 只服务 Cuexis 表现，并通过 portable/built-in/host-specific capability 分层。

现有 ADR 中关于 RuntimeSession、SDL Audio、OpenGL、Android 和配置的内部决策继续有效；凡是把 Player/Studio 视为唯一组合者、把物理文件系统视为唯一内容来源或把后端视为 SDK 必选项的表述，由本 ADR 补充和限定。

## 后续风险

宿主渲染互操作、二进制 ABI 和 ReplayData 格式是主要风险。必须先通过 headless consumer、Player 对照测试和至少一个真实宿主适配证明，再扩大公共契约；不得为了支持所有宿主提前建立无边界插件框架。
