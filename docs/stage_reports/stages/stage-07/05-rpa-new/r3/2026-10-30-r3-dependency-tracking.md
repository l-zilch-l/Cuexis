# R3 MSVC Ninja 依赖修复与真实重编验证

状态：本地修复与定向验证完成；P6 最终完整矩阵仍开放

日期：2026-10-30

返回 [RPA 报告索引](../README.md)。当前状态只见
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md)。

## 问题与原因

此前 fresh 构建发现 Ninja deps 没有头文件。最小真实反例将宏 41 改为 42，Ninja 不重编，
程序仍返回 41。设置 `VSLANG=1033` 也未解决：本机只有 2052 编译器语言包，原反例保留。

本机 CMake 4.3.3 的 `CMakeDetermineCompilerId.cmake` 用 `ENCODING AUTO` 解码
`/showIncludes`。已查阅同版上游 `cmProcessOutput.cxx` 与 `cm_codecvt.cxx`：前者用
`GetConsoleCP()`，生成 Ninja 前缀时后者用 `GetConsoleOutputCP()`。实测输入代码页 936、
输出代码页 65001，UTF-8 编译器输出被按 936 解码，生成规则中的前缀与实际输出不同。
这能解释本次漏记；不推断为所有历史构建问题的唯一原因。

## 修复与验证

- 新模块探测原始前缀，只对合法 UTF-8 前缀校正；非 UTF-8 保留 CMake 原处理。
- Ninja 使用最外层目录的一个全局前缀，模块以延迟函数更新最外层变量，同时覆盖嵌入宿主。
- 仅对 MSVC compiler ID + Ninja 生效；原生构建注册 CTest，转发 compiler 及 ARG1。
- 回归夹具同时覆盖独立与嵌入布局：先实际编译运行，再确认 `.ninja_deps` 包含头文件，
  修改头文件后再编译运行，要求程序由 17 变为 18。每次新建目录并保留日志。

`dependency-root-v3` 为根项目 fresh headless Release 配置；注册的
`cuexis_msvc_dependency_tracking` 通过，13.75 秒，两个布局均由 17 变为 18。
首版 v2 的顶层夹具也通过，但审查发现嵌入作用域、旧编码兼容和 ARG1 缺口，均在 v3 修正。
只读复审未发现当前范围内新的阻塞项。非 UTF-8 分支及其他输入/输出代码页组合尚未实测。

原始目录为 `out/verification-20261030/r3-next-v1/`；有限附件和源码哈希见
[manifest](2026-10-30-dependency-evidence/manifest.json)。根 v1 因 PowerShell 将未引用版本后缀
分割而在 project 前失败，未编译且保留，不作为通过结果。根 v2/v3 复用已安装依赖，未构建产品。

## 后续边界

已有非冻结构建必须重新 configure 并完整重建，不能仅修前缀后信任旧对象。冻结现场不修改。
最终源码收敛后的 Debug/Release、STATIC/SHARED、MinGW、安装消费者与全量 CTest 仍须执行。
SDK API 和日期版本未变；本次不是 R3/R4 退出、设备或生产接受。
