# SDK 0.9.1 与当前架构文档同步

状态：candidate；文档与版本定向核验记录，不构成发行或 R3 退出

快照日期：2026-10-28

## 1. 范围与授权

本轮只在 `D:/Cuexis-redesign`、`Cuexis-redesign` 分支进行，入口 HEAD 为 `d2ea3de`。
owner 要求确认文档记录最新架构，随后明确指定“如果你查到有改变就更新至0.9.1”。
性能优化保持停止，不重跑 1K / 10K / 40K 测量，不改变 kernel / Fold、wire 或哈希语义。

核对 `e8fce1f` 确认安装版 candidate 头文件曾新增 `ReplaySourceIdentity` 和
`StepTarget.replaySourceIdentity`，影响嵌套 request / proof / receipt 布局。
这部分不是纯实现改写；其余分页存储、prepare、Transform、closure 和编码优化本身不要求升版。

## 2. 文档修正

此前 [当前状态](../../../../../CURRENT_STATUS.md)、R3 日期报告和来源绑定合同已有记录，
主架构入口仍遗漏内部存储/复用边界，部分页首仍标旧 SDK 或“未实施”。本轮补齐：

- 新增 [执行状态、恢复与准备路径](../../../../../architecture/execution-state-and-recovery.md)，
  集中记录共享页、不可变准备、锚点资格/回退、来源身份、Transform 信任与失效、closure、编码缓存及维护责任。
- 同步架构索引、Overview、模块边界、RuntimeSession 和 Player 路径导航；移除 RuntimeSession
  页面把早期录制/结果概念方法当作现有 API 的表达。
- 修正 ADR 0044–0046 及索引的实施状态注释；保留原决定和历史审查背景，不把设计注释当作验收。
- 同步 API 索引、执行协调、宿主边界、RPA 合同和 SDK metadata，区分当前实现、旧字段基线与历史版本。

源码核对还确认：可变 RuntimeSession `withWorld()` 负责在回调后使 Transform 缓存失效；
独立 World `withRegistry()` 本身不清除该标记，其默认 transform update 走完整验证。
文档没有把二者笼统合并为“所有 Registry 回调自动失效”。

## 3. 版本与兼容决定

SDK API 从 `0.9.0` 改为 `0.9.1`，由 `cmake/CuexisVersion.cmake` 单一来源生成头和 package version。
日期构建与 vcpkg `version-string` 仍为 `26.10.19-1`。
同步 consumer 最低版本、参考宿主、Core 版本与 Player diagnostics 断言。
Gameplay 安装消费者新增类型断言，确保新字段可从安装头消费。

版本政策明确登记 owner 指定的未发行 candidate 布局例外，不降低通常的 minor 规则；
不声称所有旧源码兼容或旧二进制可混用，consumer 须使用同一套新头/库整体重建。
`SameMinorVersion` 可接受较低的 `0.9.0` package 请求，不证明旧库提供新 API。
消费者门原先固定拒绝 `0.9.1`，现由 canonical 版本推导“下一 patch”，本次应拒绝 `0.9.2`。

## 4. 定向核验

原始命令、逐项退出码与日志保存在 `out/verification-20261028/sdk-091/`。
验证脚本是 `verify.ps1`，每次只运行一个构建，`CMAKE_BUILD_PARALLEL_LEVEL=1`，
父进程与编译/链接子进程 BelowNormal、affinity mask 3；依赖复用已有安装且禁用 vcpkg 安装。
STATIC / SHARED 使用独立 fresh build 与独立 install prefix。

首次脚本将 install 作为 build target，会带入无关测试目标；在早期发现后主动停止，
改为 clean build 必需目标，再显式 `cmake --install`，原始中断日志保留。
这不是测试失败或性能采样，也没有恢复多路编译。

用于输入数据的 owning / realtime Packed fixture 由此前验证的 0.9.0 测试程序生成，
不作为新版本可执行文件回归；新版本安装消费者使用新头、新库重新构建后运行这些固定输入。

一次普通消费者运行因 runner 将 Windows 反斜杠路径直接写入 C++ fixture 宏而失败；
改用正斜杠重新 configure、build、run 后通过。失败与后续日志都保留，未修改产品实现。
结果清单保留所有尝试，`summary.json` 按检查名称取最终结果；50 个定向步骤全部符合预期，
其中版本/candidate 拒绝步骤要求非零退出，并检查拒绝原因，不能把任意失败当通过。

| 核验 | STATIC | SHARED |
| --- | --- | --- |
| fresh configure、串行 clean build、独立安装 | 通过 | 通过 |
| 生成头、安装头及 package version 均为 0.9.1；日期构建不变 | 通过 | 通过 |
| Core 全套 | 36 cases / 2,607 assertions | 36 cases / 2,607 assertions |
| 新头的来源字段类型断言与安装版 Gameplay 生命周期 | 通过 | 通过 |
| 请求 0.9.0 与 0.9.1：普通 Playback consumer configure / build / run | 两种请求均通过 | 两种请求均通过 |
| 请求 0.9.2、0.8.1、0.10.0、1.0.0 | 均按版本原因拒绝 | 均按版本原因拒绝 |
| candidate 未许可使用 | 按 experimental 原因拒绝 | 按 experimental 原因拒绝 |
| 独立参考宿主 configure / build / 实际生命周期 | 23 steps，outcome=ok | 23 steps，outcome=ok |
| 架构门 | 1/1 | 1/1 |
| shared 导出面 | 不适用 | 1/1 |
| 安装的公共 hpp 全部 ASCII | 29/29 | 29/29 |

Player diagnostics 另外使用新 STATIC 安装库与新生成版本头构建实际四个测试，
4 cases / 78 assertions 通过，包含导出 `sdkApiVersion=0.9.1` 的断言。
版本门 Python 单测 26 项中 24 项通过、2 项 skip；跳过项不计为已验证。
文档检查 592 Markdown / 20 candidate JSON/CXT 通过，日期版本一致性检查及修改 C++ 文件的
clang-format 检查通过。既往 0.9.0 全量矩阵不计为本轮新版本测试；本轮未重跑完整 CTest、
MinGW、Debug、设备、正式性能或远程发行/批准门。

## 5. 接受边界

本轮修正文档与版本记录，不宣布正式发行、R3 / R4 退出、设备或生产性能接受。
后续实现改动须同步长期架构参考，字段合同和测试证据继续分别由 Spec 与日期报告拥有。
