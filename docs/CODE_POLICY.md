# Cuexis 编码、错误与线程政策

状态：已接受

更新日期：2026-07-27

## 文件与命名

```text
类型                  PascalCase
函数、变量            camelCase
命名空间              cuexis::module
宏                    CUEXIS_UPPER_CASE
C++ 文件              snake_case.hpp / snake_case.cpp
测试文件              <subject>_tests.cpp
```

公共头位于 `include/cuexis/<module>/`，实现位于模块 `src/`。不再允许 PascalCase 文件名与 snake_case 混用。

## Result 与异常

C++20 使用 `tl::expected` 作为成熟基础实现，通过 `cuexis::core::Result<T, E>` 和项目 Error 类型暴露。除该明确基础依赖外，业务接口不暴露第三方错误类型。

```text
可预期运行错误        Result 返回
批量校验问题          Diagnostics 集合
程序员不变量破坏      Debug assert + Fatal Error
第三方异常            在模块边界捕获并转换为 Error
```

若进程内存已经耗尽到无法构造 `Error` 或其 context，则视为进程级 Fatal，不承诺通过 `Result` 恢复。

异常不得跨 Cuexis 模块公共边界。析构函数、音频线程和渲染资源释放路径不得抛异常。

Error 至少包含稳定 code、可读 message 和可选 context/cause。可序列化诊断使用稳定 code，不依赖本地化 message 做程序判断。

禁止忽略 Result。确实无需处理时必须显式调用命名清晰的 discard/log helper。

## 所有权

```text
unique_ptr     独占堆对象
shared_ptr     仅在确实共享所有权且无法由 Scope/Lease 表达时使用
raw pointer    非拥有、可空观察者
reference      非拥有、不可空且调用期有效
ResourceHandle 资源弱引用
ResourceLease 资源强引用
```

公共 API 必须能从类型或文档判断所有权。不得使用裸 `new/delete` 穿越模块边界。

## Playback SDK 公共边界

`cuexis_playback` 是宿主集成门面；`RuntimeSession`、World、Registry、ResourceManager 槽位和后端对象属于内部实现。安装后的公共 SDK 头不得包含 EnTT、SDL、OpenGL/GLAD、JSON DOM、spdlog/fmt 或其他实现依赖类型。

第一版 C++ 门面可以在内部适配 `core::Result`，但稳定 C ABI 不得暴露 `tl::expected`、标准库容器所有权或跨模块异常。跨 shared library 的数据必须有明确创建/释放方、有效期、版本和线程规则。

宿主回调默认不得重入同一 PlaybackSession。ContentProvider、日志 Sink、帧 Sink 和输入提交接口必须记录调用线程、所有权和可阻塞性；Audio Thread 不调用宿主文件、日志、渲染回调或触发判定结果查询。阶段 1D 的 Host 主音乐回调只能在 owner thread 同步消费调用期有效的 `MainMusicSourceView`，返回后不得保留 view。

`World::withRegistry()` 与 `RuntimeSession::withWorld()` 只在 owner thread 同步执行 callback，
不保存 callback。调用方不得从 callback 重入同一 World/RuntimeSession，不得在返回后保留
Registry/World 引用或指针；阻塞 callback 会直接阻塞 owner thread。callback 返回的
`core::Result<T>` 保持单层 Result，非 OOM 异常由公共边界转换为稳定 Error；内存耗尽继续按
进程级 Fatal 处理。所有 callback Result 都必须显式检查。

## 线程所有权

```text
Owner Thread      PlaybackSession、RuntimeSession、World、ChartDocument commit、帧编排和判定计算
App Main Thread   Player/Studio 的窗口、平台事件和适配器组合
Render Thread     RenderBackend 与 GPU 对象提交/销毁
Audio Thread      SDL postmix 实时计数；不组装大结构快照
Worker            后续异步文件 I/O、解码、导入和 Shader 编译
```

阶段 1D 的 Prepared load/reload、同步 ContentProvider、WAV 解码和 AudioClipStore 注册在 owner
thread 完成；Prepared 对象不可跨线程提交。后续 Worker 不访问 EnTT Registry、OpenGL/图形
Context、SDL Window 或实时 AudioStream。Render/Audio Thread 不读取可变 ChartDocument。

跨线程数据使用拥有关系清晰的消息、不可变快照或预分配队列。互斥锁、动态分配、格式化、
宿主回调和资源查询不得进入音频实时路径。postmix callback 只更新预分配的 lock-free 整数
原子；owner thread 使用 sequence counter 发布完整 AudioClockSnapshot。所有要求特定线程的
公共函数在文档或命名类型中标注，Debug 构建检查线程断言。

不同 PlaybackSession 可以由不同 owner thread 拥有；同一 Session 不因嵌入宿主而变成隐式线程安全。跨 Session 共享的不可变数据和缓存必须通过明确服务对象或 Lease 表达，禁止全局可变 Registry、当前 Session 或宿主 Context。

## 日志

日志使用结构化 category、severity 和稳定 error code。实时线程只更新预分配计数或投递轻量事件，由非实时线程格式化日志。

## 格式与检查

正式实现建立统一 `.clang-format` 和编译警告基线。CI 至少执行 configure、build、CTest 和格式检查；第三方头文件通过 SYSTEM include 隔离项目警告。
