# ADR 0011：使用 Catch2 v3 和 CTest

日期：2026-07-16

状态：已接受

## 背景

Cuexis 需要测试 Curve 采样、谱面解析、层级变换、运行时编译和版本生成等确定性逻辑。测试方案需要适配 C++20、CMake、vcpkg 和首阶段的 Windows/MSVC 环境，同时保持测试代码简洁。

## 决策

Cuexis 使用 Catch2 v3 作为单元测试框架，使用 CTest 进行测试发现和统一执行。

测试 target 按引擎模块拆分，并使用 `Catch2::Catch2WithMain` 和 `catch_discover_tests`。Catch2 只能作为测试 target 的私有依赖，不得出现在引擎公共接口或应用 target 中。

首阶段直接通过 vcpkg manifest 提供 Catch2。若后续需要缩减非测试构建的依赖安装，可以在不改变测试框架的前提下，将 Catch2 移入专用 vcpkg manifest feature。

接口测试优先编写小型 Fake。性能测试不混入单元测试；需要时单独评估 Google Benchmark。

## 备选方案

### GoogleTest 和 GoogleMock

未选择。其生态和 Mock 能力成熟，但当前主要测试对象是数据结构和确定性算法，额外样板代码与 Mock 能力暂时没有形成实际收益。

### doctest

未选择。它接入轻量，但长期工具生态、复杂测试组织和项目采用程度弱于 Catch2。

### 同时使用 Catch2 与 GoogleTest

拒绝。两套断言、测试发现和代码风格会增加维护成本。只有出现 Catch2 与简单 Fake 无法满足的明确需求时，才允许通过新 ADR 重新评估。

## 影响

```text
vcpkg manifest 增加 catch2
CMake 在 BUILD_TESTING 下查找 Catch2 v3
测试通过 catch_discover_tests 注册到 CTest
测试 target 按 cuexis_<module>_tests 命名
阶段 0 至少建立 cuexis_core_tests
```

## 后续风险

大量 Catch2 宏和复杂 SECTION 嵌套可能降低测试可读性。测试应保持单一行为焦点，并优先使用清晰的独立 TEST_CASE。

若 Catch2 安装成本影响发布构建，应通过 vcpkg manifest feature 隔离测试依赖，而不是手动复制库或更换测试框架。
