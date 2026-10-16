# PD 消费注册生命周期与计划自查

状态：dated design review；文档设计闭合与自查记录，不是实施、模拟、测试或阶段接受

日期：2026-10-16

核对基线：`stage-7`，HEAD `d076240a3f66fae9193195c2343afb1aedf94926`，工作区含未提交的 PD 计划与合同候选修订。
本记录只解释此次补齐，不改变 [CURRENT_STATUS](../../../../CURRENT_STATUS.md) 的实现结论。

## 1. 要补的问题

独立设计评审发现，PD 细则允许呈现消费者注销后以新 ConsumerId 重新注册，但普通帧的执行入口
没有完整说明注册实例身份如何到达 Renderer、注销何时实际生效、迟到帧/激活回执如何处理。
原有 last-frame 执行前沿若按注册清零，可能允许旧普通 envelope 在同一设备/context 再执行。
此前 §12.4 只明确保留重绘的注册代，不足以证明普通新帧已覆盖该边界。

本人复核时另发现相邻的内存寿命与时效表述缺口：若 request 去重历史一直持有已领取 envelope，完整帧即使离开
FrameObservationLedger 仍会被间接保活，违背帧账本回收目标。这些均为设计缺口，不是已观测
到的当前产品实现缺陷。request deadline 也需明确限制移交，避免与实际 present 或观察期限混用。

## 2. 当前裁定

消费注册键为 `(sessionScope, ConsumerId)`。ConsumerId 直接充当注册 incarnation，不再新增并行
注册代数；同一 SessionWorker 在会话内单调 checked 签发。普通帧与 retained 重绘的
FrameEnvelope、Response 和 attempt 记录均关联其 ConsumerId。AttemptKey 仍为完整
HostContext + projectionScope + frameId；ConsumerId/RequestId 是记录的不可变关联，
不增加第二套查找身份。公共 HostContext、Gameplay identity 和 Replay 均不增加字段。

同一个 Renderer instance/generation 同时至多有一个呈现注册处于申请、活动或撤销状态。
Worker 先停止 admission，再由 Renderer owner 在不与 present 调用重入的串行边界确认注销；
确认前不开放同端新 ConsumerId。注销只表示旧注册不能再开始呈现，不等待旧 CPU owning 值或
GPU 工作销毁；实际存储仍计费，设备责任仍由原 owner/fence 管理。

Renderer 保留当前注册高水位及其 Active/Inactive 身份，确认撤销后才允许较大新身份激活。
迟到旧帧/激活不改变新身份；同身份的激活/撤销按消息身份幂等，激活前撤销也留下 Inactive
拒绝标记。前沿不是永久历史表；session/route 终结且旧路由失效后才随新确认边界重置。
注册确认不解除控制 fence；控制切换本身不复活已撤销注册。
同端普通控制或仅资源代变化可以保留注册，仍须取得新绑定；Renderer 实例或代数改变必须
先证明旧注册不能再开始调用，再注册新身份。当前注册描述保留用于同键异载荷核对，旧身份
精确历史淘汰后只报告无资格/历史不可得，不编造原载荷与结局。

FrameSnapshot Ready 槽是可拥有帧的唯一尚未移交响应。take 和取消在交付槽同步边界形成唯一
先后，take 后请求历史只保留有界元数据，不持有 envelope/帧句柄；重复 take 不重新移交。
request deadline 在准入、Ready 发布及 take 时检查；过期同取消一样与领取裁决唯一先后，
不撤回已完成的软件更新或已移交 owning 值，也不承诺实际 present 在此时限前完成。
Renderer 调用前依序检查 session/Active ConsumerId、完整既有绑定、retained 路径身份（若适用）
和本注册 frameId 执行前沿，检查全部成功后先登记执行标记再调用。注册不匹配的副本不推进
新注册前沿，也不替换新 last-success。若旧 attempt 历史已淘汰，拒绝副本不证明其原来未调用，
因此只能给出历史不可得，不能伪造 NotSubmitted。

## 3. 为什么采用这个最小扩展

| 方向 | 代价与风险 | 结论 |
| --- | --- | --- |
| 仅给每个新注册重置 frame high-water | 字段最少，但同设备重注册后迟到普通帧可重放；last-success 归属不清 | 拒绝 |
| 只保留跨注销的 session frame high-water | 阻止低 frame 重放，但无法单独证明旧注册已撤销；旧队列和新注册并存时仍可能错误归属 last-success | 不单独采用 |
| ConsumerId 绑定每帧，加 Renderer 串行激活/撤销确认 | 多一个内部身份检查和有界生命周期转换；可以区分注册许可、设备控制许可与设备退休 | 采用；frameId 签发器保持会话内单调，注册内执行前沿仅在旧注册撤销确认、新注册激活后初始化 |
| 永久保存全部 ConsumerId/消息历史 | 可提供更精细追溯，但历史存储无限增长并与回收目标冲突 | 拒绝；使用高水位、当前身份及有界终态/覆盖记录 |

该选择比“只加 nonce”更明确，却将协议增量限制在宿主 ConsumerId 与已有可靠生命周期传输上；
不扩展到多个 Renderer、通用端点注册器或第二个 Replay/控制状态源。注册 transitions 有自己
预留的有界记录；不复用 PresentReceipt 观察通道，不把注册生命周期加入 Gameplay/Replay。

## 4. 本人对计划整体的针对性复核

复核了 PD 主计划 §0–12、设计细则 §1–12、实时宿主边界 §1–8、相关 RPA Spec/ABI 与阶段计划，
重点逐项对照以下序列：

- Registering 期间控制替换或 Close、激活拒绝/迟到、同端注册竞争、撤销超时与新身份到达。
- 同端控制/资源换代与 Renderer 实例/代数替换、当前注册描述退休和旧身份精确历史缺失。
- 旧普通帧、保留重绘、receipt、撤销确认跨过注册边界，设备仍有 GPU 工作或调用者仍持有 CPU 值。
- Ready 帧等待领取、take 与取消/过期竞争、重复 request/poll/take、响应丢弃和 request 去重窗口淘汰。
- 成功 Runtime 安装后提帧失败；旧 World/候选/缓存和 Runtime finalize 边界。
- Resume/Pause/EOF 的绑定切换、Seek/Reset 的 scope 变化，以及 Stage 8–14 的能力/域/预览/恢复/重建/ABI 责任。

对应规则现在集中在[设计细则 §6.1、§6.3.1、§6.4、§7.2–7.3、§8.1–8.2](../../../../stage_plans/active/stage-07/presentation-decoupling-design.md)，
计划批次、验收和公共差异门禁同步在 [PD 主计划 §5–8、§11](../../../../stage_plans/active/stage-07/plan-presentation-decoupling.md)
及[宿主候选 §8.3](../../../../api/realtime-host-boundary.md)。Stage 7-RPA 的 control、resource fence、
设备 retire 与 Close 仍是权威合同，PD 的注册确认不替代这些责任。

本次识别的缺口已补入设计与验收，针对上述范围未发现仍待裁定的设计阻塞；这不证明实现可行性。
以下仍是明确的实施/阶段门禁，而非已验证结论：
EnTT storage 无失败安装能力、可靠生命周期设施的真实容量与响应延迟、Ready/take 实现的线程安全、
旧 owning 值的长时存活高水位、跨设备/低内存行为、正式公共合同与 SDK minor 接受，以及性能目标。
此文档复核没有原型、模拟实施、构建、CTest、故障注入、设备或性能运行证据。

## 5. 文档验证

2026-10-16 在上述工作区完成：

- `python -B tools/check_docs.py`：通过，验证 451 份 Markdown 和 20 份候选 JSON/CXT 文件。
- `git diff --check`：通过；仅有仓库换行策略的 LF → CRLF 提示，没有空白错误。
- 对全部 23 份已修改/未跟踪 Markdown 检查 UTF-8、单一 H1、代码围栏闭合与行尾空白：通过。
  该数量包含前轮已存在的未提交文档，不表示它们都在本轮修改。

本轮只修改设计、计划、术语和导航/记录文档；CURRENT_STATUS、产品源码、安装头与版本未改。
这些结果仅证明文档检查通过，不是实现、模拟实施、构建、CTest、设备或性能验证。
