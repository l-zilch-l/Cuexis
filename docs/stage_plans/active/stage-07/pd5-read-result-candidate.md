# PD-5 C2: caller-owned neutral read result candidate

状态：单候选预登记；实施/采样状态见 CURRENT_STATUS 与独立结果报告。登记日期：2026-10-21。
本计划只登记 C2，不重复已放弃的 C1。当前状态由 [CURRENT_STATUS](../../../CURRENT_STATUS.md) 汇总。
统计与正式接受仍受[已接受 PD-5 协议](pd5-measurement-proposal.md)约束。

## 目标与可证伪假设

F1 的 readObject 返回 Result<ObjectPresentation>，正常路径在中性值与 owning Snapshot 之间
还有大返回值构造。已封存 MSVC /O2 /Ob2 反汇编在返回点 0x5A0–0x5D4 将 128 bytes 中性值
写入 expected 成功 payload，另写 discriminator；这支持调查返回值数据路径，不证明其独立成本。

唯一 C2 假设：让唯一物化器提供单个 ObjectPresentation 输出槽，使用内部
readObject(index, ObjectPresentation&) -> Result<void>，可减少成功路径的大返回 payload
搬运，并在不改变读取工作及任何保护时改善 read64 至少 5%。如果生成代码未减少该搬运，
或固定比较不满足下述数值与语义条件，该假设被否定，C2 不具备保留资格，不登记 C3 或追加替代样本。

## F1 基线与前置

比较基线固定为 F1，不将被放弃的 C1、当前时间较新的重编译 library 或旧修复前 B 当作 F1。
基线源码清单 SHA256 为 `1bfebe4b6e49dcc291136d3d2dec13e64e296341493e3e3d28023afcdecd6da3`；
既有正常 baseline probe SHA256 为
`ef1efdb280135cca40e573dd945706787cc8dc8191b0afb277b7e23c0b84ff6b`。
锚定证据为新调查根 f1-comparison-anchor.json 及 f1-read-object-codegen.txt。

旧 F1 构建的 runtime.lib 原路径已经被 CTest 分发 gate 同源重编译，不能复用它做当前身份验证；
原字节及失配记录均保留。有限对照的 F1 与 C2 应各建新的匹配工具链正常 Release STATIC 树，
使用同一 harness；本方案冻结后、采样前另行绑定实际源码、二进制、工具与全部构建身份。
任何身份绑定都不得修改本方案的阈值、内容、顺序或停止规则。

独立的 [Gameplay-host 字典调查](../../../stage_reports/stages/stage-07/pd/2026-10-21-gameplay-host-string-budget.md)
拥有容量事实；读取候选不修改或豁免任何 Packed ceiling，不将 40k 准入失败算作读取成本。

## 唯一变更范围与维护成本

仅改内部 PresentationRead 头/实现和 presentation_materializer 调用点；保留原值返回接口作为
薄封装，所有读取共用一套 checked 实现。唯一物化器仍在 Playback；不引入设备/EnTT 到公共 SDK。
逐对象填 caller 的中性值，只使用一个有界栈槽，不增加完整中间数组、缓存、owner、线程或回收协议。
不采用 storage-pointer C1、不调 IPO/LTCG、不改优化级别或读取工作量。

checked 实现仍依次检查 owner/thread、active/serial、对象索引/映射及 entity 有效性；
在前置错误时不写输出槽。有效 entity 缺组件时按 F1 重置默认值和 presence，防止上一对象残留。
旧接口仍传播相同 Result/Error 与异常边界。所有 string_view 仍只在原窗口/回调内使用，
旧状态/布局、过期、跨线程、Close 保活、逃逸、重入、暖态零分配、资源匹配与唯一物化规则不变。

维护成本是一项内部输出槽重载和一个共用实现。新增测试只覆盖错误时输出保持、连续读取缺组件的
默认重置和新/旧接口中性字段一致性；其余生命周期/资源错误继续运行既有测试。

## 冻结工作量与有限观察

正常对照：Release /O2 /Ob2、STATIC、candidate ON、metrics/trace/lifetime observer OFF，
同工具链/依赖和资源。代码生成只读审计与正常计时分开；完整 CTest、分发/安装 gate 使用独立
验证树，不再触及冻结测量树。构建最多 4 并行 BelowNormal；一次仅一个计时进程。

只登记以下两单元；它们是有限候选证据，不是正式 preparation 或 registration：

| 单元 | 固定配置 | 进程与样本 |
| --- | --- | --- |
| read / updated | 1/8/64/256 Snapshot 对象，各加 1 camera；只有首对象移动，内容/序列/viewport 与 F1 共用 harness | 3 对独立进程，AB/BA/AB；每配置先 64 次 update+extract；每进程每配置 3 块 |
| 公共 request→ready | 相同 2 对象/1 camera、短闭环内容、32 预热、1024 被测操作、capacity=2048 | 3 对独立进程，AB/BA/AB；每进程操作样本分别计算分布 |

A 始终 F1，B 始终 C2。每个被测进程前独立 30 秒 CPU 热身；read 块至少 100 ms。
read 次数依次为 2097152/1048576/262144/65536，updated 为 262144/131072/32768/8192。
scale 固定 mask=4，host 固定为采样前允许集合，不按速度挑核。采样前记录电源方案、CPU/OS、
后台进程快照、工具链与配置；原生 begin/end PD_ENV、PID/QPC 与 log SHA 也须齐全。
缺项或执行/身份/输出错误停止该单元并判候选无资格，不补样。噪声大或区间跨线保留所有固定样本，
判不可接受，不追加进程直到通过。每进程中位是观察值，内部块/操作不增加自由度；
3 对 log-ratio 双侧 95% CI 使用 t=4.302653。不同完成端点不得合并。

## 一次性保留条件

以下条件全部满足才称 C2 自检通过，仍须交回核验后决定保留；任一失败均保留全部记录：

1. 主指标 read64 的 C2/F1 配对 CI 上界 ≤0.95；点估计变快不足以接受。
2. 所有 read、updated、公共 ready 的 C2/F1 配对 CI 上界 ≤1.10；每项每侧进程重复性 ≤15%。
3. 相同内容、U/F/R、对象/相机数、请求节奏、viewport 与输出完全相等；独立 native/log 不复用。
4. 生成代码审计支持减少目标成功 payload 搬运；不能靠关闭保护、少做工作或编译配置差异取得收益。
5. 受影响 runtime/playback/allocation/world、架构、格式及完整 Release CTest 在独立验证树通过；
   按实际执行/跳过项报告。新风险测试和既有生命周期、资源匹配、错误次序与暖态零分配全部满足。

上述 5% 是唯一候选的收益条件，不替换原 1.10/15% 或任何绝对预算。
read 的块平均不是单操作 p99；host 的 1024 操作只覆盖该短轨迹，不接受整个 SDK 或生产尾部。
F1 对照的改进也不自动消除原协议对修复前基线的累计回归。

## 正式组进入条件与排除项

本轮按“一个候选→构建/语义/有限诊断→交回核验”停止。C2 产品 diff 放独立工作树/分支，主
工作树维持 F1；无论自检数值如何，先不宣布保留、不新建正式测量登记。身份、结果和门禁经
核验通过并决定保留后，才进入全新正常 Release 构建、preparation、有限 pilot、完整身份冻结及
registration；验证树与测量树独立。正式比较定义和 1.10/15% 保持已接受协议含义，不因本计划
将正式历史 A 偷换成 F1；F1 是本候选的固定主对照。正式组仍可能如实报告超界或不可判定。
当前有限诊断 plan、此候选预登记或旧 F1 构建都不能直接用于正式 register。

旧窗口 3 与更早中断组继续停止；正式组须使用新 schema/manifest 身份与全新输出根，
不接续旧 claim、seal 或两窗。P0 不覆盖生产 40k、P1–P8、完整内存、设备或 Stage 7 关闭。
任何偏离、预算/协议变更、Gameplay/Replay 修改、owner/线程或物化归属调整均不在本候选范围。

## 依据与恢复点

[PD 设计 §9.4](presentation-decoupling-design.md)、[PD 计划](plan-presentation-decoupling.md)、
[前轮证据与 C1 放弃](../../../stage_reports/stages/stage-07/pd/2026-10-21-pd5-followup.md)
分别拥有既有成本边界、阶段目标与历史事实。恢复点为 F1 热路径字节；C2 失败只撤回自身产品 diff，
不覆盖调查结果、原始日志、工具快照或历史证据。
