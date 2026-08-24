# Cuexis Versioning

状态：已接受

更新日期：2026-08-06

## 格式

规范版本：

```text
yy.mm.dd-v
```

日期使用 UTC。`v` 是同一 UTC 日期内从 1 开始人工递增的构建号。

构建后缀：

```text
正式      无后缀
开发      -dev
测试      -test
内部      -internal
实验      -exp.<name>
```

## 单一来源

`cmake/CuexisVersion.cmake` 保存年、月、日和 build。其他 CMakeLists 不重复手写版本。
`tools/update_version.py` 是人工更新入口，负责在一次事务中同步该文件与 `vcpkg.json`。

CMake `project(VERSION)` 使用无前导零的三段数字，例如 `26.8.1`。完整显示版本单独生成，例如 `26.08.01-1-dev`。

`vcpkg.json` 的 `version-string` 保存不带构建类型后缀的规范版本，例如 `26.08.01-1`，因为同一源码可以同时构建 Debug 和 Release。

## 更新流程

```text
取得目标 UTC yy/mm/dd
-> 若日期变化，将 build 设为 1
-> 若同一日期再次发布构建，人工递增 build
-> 运行 python -B tools/update_version.py yy.mm.dd-v
-> configure 生成 version.hpp
-> 运行版本一致性测试
```

只检查当前文件而不写入：

```powershell
python -B tools/update_version.py --check
```

## 生成头

`${binaryDir}/generated/cuexis/version.hpp` 至少提供完整字符串、三段日期、build 和 suffix。
旧的公开 `cuexis::version::hour` 为保持 0.5.x 源码兼容暂时保留，并固定为 `0`；它不再属于
构建身份。生成头不提交源码控制。

## 验证

测试检查：

```text
CMake 三段版本与版本配置一致
月份、日期和 build 合法
vcpkg version-string 与规范版本一致
suffix 只来自允许集合或 exp.<name>
窗口标题和启动日志使用完整显示版本
```

版本错误必须使 configure 或测试失败，不能只产生警告。

## SDK、ABI 与内容版本

项目显示版本不替代独立的兼容性版本：

```text
Cuexis display version     仓库/发行构建身份
C++ SDK API version        CMake package 与源码兼容版本；当前 static/shared preview 为 0.6.0
C ABI version              稳定 shared-library 二进制契约，阶段 12 首次冻结
Chart/Project/Asset format 各自持久化 format + version
ReplayData format          阶段 11 冻结的独立持久化版本
simulationVersion          粒子等确定性算法版本
```

当前 `CUEXIS_SDK_API_VERSION` 是 CMake package version 的唯一来源，生成头通过
`cuexis::version::sdkApi` 暴露，package config 同时设置 `Cuexis_VERSION` 和
`Cuexis_API_VERSION`。`Cuexis_VERSION_DISPLAY` 继续保存完整构建身份。preview 使用
`SameMinorVersion`：`0.6.x` 内允许满足不高于已安装版本的请求，minor 或 major 变化必须由
consumer 显式接受。日期构建号变化不改变 `find_package` 兼容性。

阶段 1D 的 SourceClockSample、RuntimeTimeline、Prepared Playback 和 Audio package components
曾将 preview 提升到 `0.2.0`。阶段 1E 的 PlaybackSource 构造边界、FrameDigest 与 static/shared
package linkage 契约进一步把 `CUEXIS_SDK_API_VERSION`、package version、生成头和 external
consumer 同步提升到 `0.3.0`；后续不兼容修改必须再次提升 minor，禁止只修改其中一个版本来源。

ADR 0033 已将同工具链 C++ shared preview 纳入阶段 1E。`0.3.0` 已实现 Playback 的 source
构造边界与 package linkage 契约；static/shared 使用同一 API minor 和公共 target 名，但不能在
一个 install prefix 混装。`vcpkg.json` 继续只记录日期构建版本，不复制 `0.3.0`。

阶段 2 的 `FrameSnapshot` Visibility/Material 字段、FrameDigest version 2、capability 查询和
Chart v3 可观察合同是不兼容的公共结构变化，因此 ADR 0036 将 static/shared preview 提升到
`0.4.0`。`CUEXIS_SDK_API_VERSION`、生成头、package version 与 external consumer 必须保持
同步；`vcpkg.json` 仍不复制 SDK API 版本。

阶段 3B 新增 Portable Presentation Profile v1 public values、candidate manifest/token 和 owning
resource acquisition，因此 preview 已提升到 `0.5.0`。阶段 3C 已在同一 public minor 中补齐
FrameSnapshot resource refs 与 FrameDigest v3，没有重复提升版本。

Stage Chart Format Update 的 CFU-E0 接受了 owning project-document source、CXC file/memory factory、
prepare options 和 semantic identity observation 的 additive API 方向。CFU-E1 首次落下 public source
type/factory 并将 preview、生成头、package config、version rejection 与 static/shared consumer 同步
提升到 `0.6.0`；E2/E3 将在同一已批准 minor 中完成其余冻结 surface。

shared `0.x` 只支持使用匹配 SDK minor、编译器工具链、标准库、运行时、架构和 Debug/Release
配置重新构建的 consumer。CMake `SameMinorVersion` 继续表达源码/package 请求兼容，绝不表示
可以替换一个已部署 shared binary 而不重新编译 host。C ABI version 仍不存在，直至阶段 12 在
Judgement/Replay 和完整生命周期证据基础上冻结。

升级项目显示版本不得隐式升级 SDK API、内容格式或 ABI。安装包必须提供可查询的显示版本、
SDK API 版本和已启用组件；稳定 C ABI 在阶段 12 建立后再提供独立可查询版本。不兼容 API/ABI、
未来内容版本和不支持的 ReplayData 版本必须稳定失败。
