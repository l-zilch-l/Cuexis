# ADR 0002：使用 vcpkg manifest mode

日期：2026-07-17

状态：已接受

## 背景

项目依赖 SDL3、EnTT、glm、JSON、日志、测试及后续资产工具，需要可重复的 Windows/MSVC 构建。

## 决策

第三方 C/C++ 依赖统一通过 vcpkg manifest mode 管理，根目录提供 `vcpkg.json` 和固定 baseline 的 `vcpkg-configuration.json`。工具链通过 `VCPKG_ROOT` 定位，不写死开发机路径。

## 备选方案

Git submodule 和手工复制库会分散版本、补丁和许可证记录，因此不采用。Conan 暂不引入，避免同时维护两套包管理。

## 影响

新增依赖需要更新 manifest、依赖政策记录和必要 ADR。

## 后续风险

个别库可能缺少合适 port；应优先贡献或维护 overlay port，而不是绕过 manifest。
