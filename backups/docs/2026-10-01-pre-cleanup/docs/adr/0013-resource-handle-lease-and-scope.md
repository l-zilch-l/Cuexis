# ADR 0013：资源 Handle、Lease 与 Scope 生命周期

日期：2026-07-17

状态：已接受

## 背景

Cuexis 的 Component 需要轻量引用纹理、材质、音频和行为资源，同时 RuntimeSession、Studio Preview、热重载和未来内存预算要求资源生命周期可集中控制。直接在 Component 中保存 `shared_ptr` 或第三方 Resource 类型会扩散引用计数和第三方语义，也难以识别已经复用的资源槽位。

## 决策

ResourceManager 是资源槽位、状态、依赖和生命周期的唯一所有者。

```text
AssetId             持久化稳定标识
ResourceHandle<T>   index + generation 的类型化弱句柄
ResourceLease<T>    RAII 强引用
ResourceScope       批量持有 Lease
```

Component 只保存 Handle。RuntimeSession 和 Studio Preview 使用 Scope 持有全部直接及传递依赖的 Lease。槽位真正卸载并复用时递增 generation，使旧 Handle 查询失败。

资源状态为 `Unloaded`、`Loading`、`Ready`、`Failed` 和 `Reloading`。阶段 1 只实现同步加载，不冻结 Future、协程或任务系统 API。

资源引用显式声明 `Required`、`Fallback` 或 `Optional`。主谱面、主音乐和 BehaviorClip 默认 Required；Mesh、Texture、Material 和 Shader 默认 Fallback；装饰粒子和可选音效默认 Optional。

热重载成功保持 Handle 并递增 `contentRevision`。热重载失败保留上一份有效内容。首次加载失败且无旧内容时进入 Failed，由引用策略决定上层结果。

ResourceManager 不调用 OpenGL。未来异步路径中，Worker 处理文件和 CPU 转换，资源所有者线程提交槽位，Render Thread 创建和延迟销毁 GPU 对象，Audio Thread 不参与资源加载。

可以在 `cuexis_assets` 内部评估 EnTT Resource Cache，但公共 API、Component 和 Chart 不暴露 `entt::resource`。

## 备选方案

### Component 直接保存 shared_ptr 或 entt::resource

拒绝。它会把生命周期和第三方类型扩散到 ECS，并让大量 Entity 参与引用计数。

### Handle 自身拥有强引用

拒绝。Renderable 等高频 Component 会导致分散的引用计数更新，也难以表达一次 RuntimeSession 的整体资源边界。

### 只提供 ResourceScope，不提供单资源 Lease

未选择。它适合谱面批量加载，但 Studio 预览、跨 Scope 共享和工具操作仍需要单资源强引用。Scope 应作为 Lease 的批量容器。

### 第一版直接实现异步加载框架

拒绝。任务模型、线程和取消语义尚未形成真实需求。同步加载足以完成阶段 1，并可先验证生命周期规则。

## 影响

```text
cuexis_assets 需要类型化槽位池和 generation 校验
RuntimeSession 销毁时释放 ResourceScope
Chart 和资产文件只保存 AssetId
渲染后端按 Handle + contentRevision 管理派生 GPU 对象
测试覆盖旧 Handle、Scope 释放、加载策略和热重载失败
```

## 后续风险

延后异步 API 可以避免过度设计，但 Loader 必须避免把同步假设写入资产格式。未来引入异步后，需要为取消、优先级、依赖加载和主线程提交另写 ADR。

缓存预算和自动淘汰算法尚未决定。Scope 释放只表示资源可被回收，不要求立即卸载。
