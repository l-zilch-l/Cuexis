# ADR 0023：编码、错误与线程政策

日期：2026-07-17

状态：已接受

## 背景

C++20 缺少标准 expected，且 Runtime、Render、Audio 和 Worker 有不同线程限制。没有统一错误和线程规则会导致异常泄漏、忽略错误和跨线程访问 ECS/后端。

## 决策

使用 tl::expected，通过 `cuexis::core::Result` 暴露；可预期错误用 Result，批量问题用 Diagnostics，异常在模块边界转换。文件统一 snake_case。

Main Thread 拥有 RuntimeSession/World，Render Thread 拥有后端提交，Audio Thread 仅做实时供数，Worker 只生成 CPU/Prepared 数据。跨线程使用消息和不可变快照。

## 备选方案

全面异常会使实时与资源清理路径难以控制；自研 expected 重复成熟库；混合文件命名降低一致性，均不采用。

## 影响

vcpkg 增加 tl-expected，Core 提供 Result/Error，Debug 构建检查线程断言，CI 增加格式检查。

## 后续风险

Result 链式代码可能变得冗长，应提供小型组合 helper，但不得隐藏错误或重新引入异常控制流。

## SDK 转型补充（2026-07-20）

Main Thread 规则泛化为每个 PlaybackSession 的 owner thread；独立 Player/Studio 通常使用应用主线程，外部宿主可以为不同 Session 选择不同 owner thread。同一 Session 不提供隐式线程安全。

内部模块继续使用 `core::Result`/tl::expected；安装后的稳定 C ABI 不暴露 tl::expected、C++ 异常或标准库所有权。宿主回调必须定义线程、重入、阻塞和生命周期规则。
