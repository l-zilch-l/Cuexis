# ADR 0039：从构建版本移除小时字段

日期：2026-08-10

状态：已接受

## 背景

ADR 0004 使用 `yy.mm.dd.hh-v` 标识内部构建。小时字段要求人工维护，却不能在并行分支或
同一小时内自动保证唯一性，也使简单的日期发行版本包含不必要的第四段。项目仍需要让
构建身份、C++ SDK API、未来 C ABI 和内容格式版本独立演进。

## 决策

构建身份改为 UTC `yy.mm.dd-v`，当前版本为 `26.08.01-1`。`v` 是同一 UTC 日期内从 1
开始递增的正整数。构建后缀继续使用 `-dev`、`-test`、`-internal` 或 `-exp.<name>`。

`cmake/CuexisVersion.cmake` 保存 year、month、day 和 build，CMake `project(VERSION)` 使用
无前导零的 `yy.m.d` 三段版本。`vcpkg.json` 保存不带构建后缀的规范版本。

人工更新必须运行：

```powershell
python -B tools/update_version.py yy.mm.dd-v
```

脚本在写入前校验格式、真实日历日期、build 范围和目标文件结构，并在一次操作中同步
CMake 与 vcpkg manifest。`--check` 模式只验证当前状态。

已安装生成头中的 `cuexis::version::hour` 为保持 SDK 0.5.x 源码兼容而暂时保留，值固定为
`0`，不再参与 canonical、display 或 CMake project version。此次变化不移除公共符号，
因此不单独提升 `CUEXIS_SDK_API_VERSION`。

## 影响

- 新构建和发行制品使用三段日期身份；历史报告继续保留原四段版本证据。
- 日期构建号变化仍不得隐式升级 SDK API、C ABI 或内容格式版本。
- 后续如要移除兼容用 `hour` 常量，必须作为公开 C++ API 变化独立评审版本提升。
- 版本仍不能单独证明源码唯一性；正式制品和诊断证据应继续关联 Git commit SHA。

## 取代关系

本 ADR 取代 ADR 0004 的现行格式决策，但不改写使用旧格式生成的历史证据。
