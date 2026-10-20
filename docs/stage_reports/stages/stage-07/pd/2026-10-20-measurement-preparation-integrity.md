# 测量准备的注册完整性与容量短测修正

日期：2026-10-20。状态：两项局部工具修正已验证；完整测量准备未完成。
本报告接续[窗口与矩阵准备](2026-10-19-pd5-window-and-matrix-preparation.md)和
[功能命名修正](2026-10-20-performance-tool-functional-naming.md)。当前状态唯一见
[CURRENT_STATUS](../../../../CURRENT_STATUS.md)，阈值仍使用
[已接受的本机方案](../../../../stage_plans/active/stage-07/pd5-measurement-proposal.md)。

## 注册与原始证据绑定

[window_protocol.py](../../../../../tools/verification/playback_performance/window_protocol.py)
在 preparation 的排他 registration claim 外新增只写一次的 seal：绑定完整 `registration.json`
及原 claim 的 SHA-256。窗口开始和历史窗口加载均复核 seal、preparation 和 pilot；
没有 seal 的旧注册不能继续。首窗前修改指标集合不再能获得有效窗口 claim。

[owner_runner.py](../../../../../tools/verification/playback_performance/owner_runner.py) 重建固定单元，
[profile_runner.py](../../../../../tools/verification/playback_performance/profile_runner.py)
从冻结 preparation 重建 profile/指标单元，拒绝注册内容与来源不一致。
正式 profile 的控制次数从 profile 推导，少采 Pause、Resume、Reset 或 resize 会在 pilot 身份验证时被拒绝。

原始 record、warmup 和 measure 日志须在所有正式窗口间独立，也不能复用 pilot 的文件。
汇总函数直接调用时仍复核跨窗唯一性，不能绕过保存链检查。
检查同时使用规范路径和本机文件 ID，独立文件名的硬链接也不能冒充另一个进程样本。
注册及窗口失败的 claim 不删除、不重试，慢样本与历史失败记录不替换。

## 容量探针的失败原因与短测结果

[容量探针](../../../../../tests/pd/pd5_capacity_probe.hpp) 的 64 实体工具检查原本仍声明
40,000 个帧反馈成员。保守单帧预留包含这些成员，两份 renderer 帧的预留超过固定 64 MiB，
注册时返回 `Delivery has no storage credit`。现在按实际 `scale` 声明对象和成员容量，
双帧总存储上限从既有 `frameByteBound(scale, 2, scale) * 2` 推导，并将声明写入结果。
它是 workload 的有限配置，不是实际分配字节，也不是放宽已接受性能阈值。

修正存储配置后暴露出局部 `ClockObservation` 未初始化标量字段的问题。探针现在值初始化，
并使用明确的 `maximumPresented` 防止固定源观察被外推到其他逻辑时点。
两个错误均只修测试探针，没有修改生产时钟或存储准入规则。

最终 64 实体/64 requirements 的 low/high reuse 各 20 assertions 通过，包含实际 host 加载、
执行、Snapshot 字节往返、Replay 等价、Pause、Seek 确认到首帧 present 和 Close。
对应单次 Seek 观察分别约 17.088 ms、7.284 ms；这是短工具验证，不能作为正式 20 ms 预算结论。
没有运行本轮 40k 正式规模测量。

## 验证、保留项与下一步

全部 Python 测试 65 项通过；Release 增量构建通过，容量 low/high 短测各 1 case 通过。
新增拒绝检查覆盖首窗前篡改、缺失 seal、跨窗口日志复用、pilot 文件复用、硬链接复用及少采控制。
新的无上下文审查指出独立汇总调用仍可能复用 pilot 文件；现已将 pilot 原始文件集合同时用于
保存链加载和独立汇总，并增加对应拒绝回归检查。
审查代理再次独立运行 17 项窗口协议测试全部通过，最终未报告未修复的实质问题；
该复审没有重新构建或采样。
命名阶段的验证归档保持原字节，新增验证保存到
`out/pd5-complete-preparation-20261019/`；其中 `capacity-high64-2.log` 保存时钟失败，
`capacity-high64-4.log` 与 `capacity-low64.log` 保存最终短测。所有更早失败日志继续保留。

完整测量准备还包括真实 Player/音乐/DFJK 驱动、设备观察器与开销证据、内存完整清单、
40k 端到端路径及最后的 profile/readiness 冻结；当前没有据此关闭这些缺项。
源码已变化，后续运行必须重新 prepare；尚未开始 pilot、正式注册、正式窗口或性能接受。
