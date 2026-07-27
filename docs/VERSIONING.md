# Cuexis Versioning

状态：已接受

更新日期：2026-07-27

## 格式

规范版本：

```text
yy.mm.dd.hh-v
```

时间使用 UTC。`v` 是同一 UTC 小时内从 1 开始人工递增的构建号。

构建后缀：

```text
正式      无后缀
开发      -dev
测试      -test
内部      -internal
实验      -exp.<name>
```

## 单一来源

正式骨架建立 `cmake/CuexisVersion.cmake` 或等价单一配置，保存年、月、日、小时和 build。其他 CMakeLists 不重复手写版本。

CMake `project(VERSION)` 使用无前导零的四段数字，例如 `26.7.18.18`。完整显示版本单独生成，例如 `26.07.18.18-1-dev`。

`vcpkg.json` 的 `version-string` 保存不带构建类型后缀的规范版本，例如 `26.07.18.18-1`，因为同一源码可以同时构建 Debug 和 Release。

## 更新流程

```text
取得当前 UTC yy/mm/dd/hh
-> 若小时变化，将 build 设为 1
-> 若同一小时再次构建，人工递增 build
-> 更新单一版本配置
-> 同步 vcpkg version-string
-> configure 生成 version.hpp
-> 运行版本一致性测试
```

## 生成头

`${binaryDir}/generated/cuexis/version.hpp` 至少提供完整字符串、四段数字、build 和 suffix。生成头不提交源码控制。

## 验证

测试检查：

```text
CMake 四段版本与版本配置一致
月份、日期、小时和 build 合法
vcpkg version-string 与规范版本一致
suffix 只来自允许集合或 exp.<name>
窗口标题和启动日志使用完整显示版本
```

版本错误必须使 configure 或测试失败，不能只产生警告。

## SDK、ABI 与内容版本

项目显示版本不替代独立的兼容性版本：

```text
Cuexis display version     仓库/发行构建身份
C++ SDK API version        CMake package 与源码兼容版本；1D preview 当前为 0.2.0
C ABI version              shared library 二进制契约，阶段 12 首次冻结
Chart/Project/Asset format 各自持久化 format + version
ReplayData format          阶段 11 冻结的独立持久化版本
simulationVersion          粒子等确定性算法版本
```

当前 `CUEXIS_SDK_API_VERSION` 是 CMake package version 的唯一来源，生成头通过
`cuexis::version::sdkApi` 暴露，package config 同时设置 `Cuexis_VERSION` 和
`Cuexis_API_VERSION`。`Cuexis_VERSION_DISPLAY` 继续保存完整构建身份。preview 使用
`SameMinorVersion`：`0.2.x` 内允许满足不高于已安装版本的请求，minor 或 major 变化必须由
consumer 显式接受。日期构建号变化不改变 `find_package` 兼容性。

阶段 1D 的 SourceClockSample、RuntimeTimeline、Prepared Playback 和 Audio package components
首次交付构成 preview minor 版本变化。`CUEXIS_SDK_API_VERSION`、package version、生成头和
external consumer 已同步提升到 `0.2.0`；后续不兼容修改必须再次提升 minor，禁止只修改其中
一个版本来源。

升级项目显示版本不得隐式升级 SDK API、内容格式或 ABI。安装包必须提供可查询的显示版本、
SDK API 版本和已启用组件；稳定 C ABI 在阶段 12 建立后再提供独立可查询版本。不兼容 API/ABI、
未来内容版本和不支持的 ReplayData 版本必须稳定失败。
