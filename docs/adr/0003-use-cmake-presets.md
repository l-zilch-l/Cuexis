# ADR 0003：使用 CMake Presets

日期：2026-07-17

状态：已接受

## 背景

开发者、IDE 和 CI 需要共享配置、构建与测试命令。

## 决策

项目使用 `CMakePresets.json` 定义 configure、build 和 test presets。阶段 0 提供 Windows/MSVC 的 Debug 与 Release，使用 out-of-source 构建和 CTest。

## 备选方案

仅在 README 中维护命令容易漂移；IDE 私有配置不能作为项目唯一构建入口。

## 影响

阶段验收统一执行 `cmake --preset`、`cmake --build --preset` 和 `ctest --preset`。

## 后续风险

新增平台应通过继承扩展 preset，避免复制整套配置。
