# PD-5 现有生命周期最小修复与有界关闭验证

状态：local regression passed / original 40k shutdown unresolved

日期：2026-10-23。起点为隔离分支 `codex/pd5-gameplay-40k-admission` 的干净
`7965fdfce254c4446a29242562fd6b9bdf8e72c5`。本次不重进阶段 C，不运行 40k 或正式测量。

## 已证实的缺陷与最小修复

`SessionWorker::create` 在分配 worker 后调用 `FrameDelivery::create`；delivery 配置被拒绝时，
局部 worker 析构会进入 `requestShutdown`，原实现无条件解引用尚未赋值的 `delivery_`。
仅为该访问增加空检查，保留队列关闭、shutdown 标记、唤醒和 owner join 的原顺序。
回归使用合法顶层 worker capacities，仅把 `delivery.consumers` 设为零。
修复前独立子进程输出 Catch SIGSEGV，原生退出码 `3221225477`；修复后正常返回既有
`host.rpa.context_stale` 错误。该工厂缺陷不是旧 40k 阻塞的根因结论。

`runExecutionPlayer` 中 owning host 早于 `captureClock`、输入队列、错误状态和回调依赖声明。
异常展开原先先析构这些局部，再由 Host 析构调用 Close/pump，存在悬空引用。
在 ports 之后、首次创建之前增加仅本函数使用的 `HostLifetime` 守卫，析构只执行
`host.reset()`，使 Host 在其回调依赖仍存活时先销毁。正常显式 Close 语义保持。
此项完成 Release Player 编译及静态析构顺序核查；没有直接注入载入后 Player 异常，
不将普通 CLI 失败测试当作该路径的动态证明，也不声称修复了长作业 join。

## 原 job 与关闭验证

`ExecutionHost::cleanup` 已按原 `sessionJob_` 查询并 acknowledge，等待期间继续调用
`pumpMain`。本次不改写该机制。新增测试沿现有 `monotonicNow` callback 阻塞 worker，
以显式测试 profile 的 100 ms reply deadline 取得真实 `control_pending`；首次 Close
仍返回 pending 且继续 pump，不报告 Closed。第二次 Close 的 pump 释放同一 barrier，
cleanup 收取唯一原 Tail job 的迟到结果，然后完成有向 Close。没有再次调用 Tail；最终
clockSegment 恰好增加一次，重复 Close 成功。测试局部析构守卫在任何断言失败时先释放
barrier，再允许 Host 析构，避免测试失败自身挂住 join。

这是有限 Tail dispatch/原 job 清理证明，不覆盖任意阻塞 provider、长 advance、所有
Closing 中间阶段或旧 40k 的 worker 栈。旧失败没有原 worker 栈，本轮没有重现它。

## 实际验证

新建 `out/build/pd5-minimal-lifecycle-release-20261023/` 与
`out/build/pd5-minimal-lifecycle-shared-20261023/`；没有覆盖历史绑定构建。
两者使用 MSVC 18.9.2 / Ninja / Release、warnings-as-errors 和 Chart-v5 candidate，
沿已核验的 showIncludes override；分别设置 STATIC/SHARED，不开启停止、metrics 或 trace。

- factory 独立 red：SIGSEGV / `3221225477`；green：1 case / 2 assertions 通过。
- deadline 独立 green：1 case / 28 assertions 通过。
- STATIC ExecutionHost 默认全套：35 cases / 34984 assertions 通过。
- Windows SHARED ExecutionHost 默认全套：35 cases / 36097 assertions 通过；断言数含有界循环。
- Release Player 编译、全仓 `cuexis_format_check`、架构 CTest 1/1、现有 Player CLI/SDL
  失败路径 CTest 7/7 通过。未宣称全仓 CTest、GPU smoke 或 Linux 矩阵。

原始命令、configure/build、factory red/green、deadline、完整回归、架构及 Player 失败日志
保存于 `out/pd5-minimal-lifecycle-20261023/`；文档与 diff 检查另存同目录。
公开操作级停止方案仅校准为暂停、未接受候选，移出本次最小修复的必需前置；原报告原文保留。
没有新增公开 API、TLS/global 状态、DLL 私有导出或 SDK 版本，没有放宽原 deadline、预算、
1.10/15% 门槛，没有修改 F1/C2、主树、旧窗口 3 或冻结证据。
旧 40k low/high 功能链路与关闭风险仍未被本次证据覆盖，不构成 PD-5、RPA 或 Stage 7 接受。
