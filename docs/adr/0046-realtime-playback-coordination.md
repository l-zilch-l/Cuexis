# ADR 0046: 实时播放的时间、执行与发布边界

状态：accepted design；2026-10-11 按 owner 授权选定实施设计，未实施、未发行

日期：2026-10-09；决定更新：2026-10-11

## 背景和授权

实际 Player 存在随运行失速与时间来源问题。owner 要求维护性优先，再考虑性能与扩展性，
本轮只做设计与文档。最初比较见 [10-09审查](../stage_reports/stages/stage-07/readiness/2026-10-09-realtime-architecture-review.md)
及 [五方向比较](../stage_reports/stages/stage-07/rpa/2026-10-11-design-and-entry-review.md)。
owner 已逐项选择 capture-time、T2/O1/I1/C1/L1、G1/Q1/K1、offset 各用一次与 R2；
随后授权剩余普通设计自行给出合理方案。以下补充决定依据该授权，不表示逐项另获 owner 点选。
同日子代理复审发现 Resume 最终切段后准备失败、NotApplied 查询后迟到 Apply 两处缺口；
owner 随后明确选择修补方案 A1+B1。该编号与实施批次 B1、尾段 A1-b 无关。

## 决定

1. 采用五方向中的 **B 职责边界**：唯一执行核心与输出域分离，保留一个 kernel/Fold。
   初始部署采用 P-B：主线程 D1 捕获/D5 GL/D6 协调；Playback worker 共置 D2 执行和 D3 表现；
   audio worker D4 独立服务。B 是状态职责选择，P-B 是线程部署，两者不能互相代称。
   D3 对象/接口独立但首版不另开线程；只有 B3 后测量证明表现导致已声明预算超界才评估拆 D3。
2. 正常输入 capture-time 不重建原始设备时间；主线程 present 阻塞期按键记为实际恢复捕获的 Tick。
   T2 支持快速点按与和弦，一个执行 Tick 一个关闭批；O1 来源分组、I1 来源内发生位置，
   C1 正常严格后续观察关批，L1 合并尚未 seal 批。控制 cut 例外须带显式 epoch 终止证明。
3. 普通推进 G1 核验连续完整前缀，保持原 H 等待；Q1 单等待槽；K1 有效控制串行接入即取消未开始请求。
   用独立 inclusive 控制 cut 解决 Pause 末批，不伪造下一个观察，不把控制 drain 冒充普通请求执行。
4. R2 以真实暂停轨迹对齐状态；保留 sealed 结果，中断未完成 body/显式 tail 结失败，
   暂停不赚成功输入。控制 origin、同 H 不同 cut、future pending 与 Recovery 版本一并修订。
5. 选择 P3-a：Prepared 留在 Session owner；内部 owning 候选把资源校验与 GPU 准备分离。
   capabilities 来自主线程，校验/acquire 在 Playback worker，GPU 创建/激活/销毁在主线程。
   不新增公共可共享 Prepared，不用锁改变 owner 约束；软件提交与设备回执分别记录。
6. 音频使用连贯、有真实采样时刻/segment/有效期/误差的观察；旧缓存不重新打戳。
   各层 offset 只应用一次，源帧/设备帧分域。失效撤销许可，不用静音进度或无声明 fallback。
   正常 EOF 选显式尾段时钟，其参数和生产阈值依 B1/B3 数据冻结。
7. 存储采用分块追加与不可变前缀共享、自足 owning 结果；只合并完整表现帧。
   选择 F1+F3 内部呈现回执观测；不保证首见，RemainingFrames 含义不变。
8. 控制、可靠输入、资源 fence 与有向关闭使用窄协议；不引入通用 actor/任务图框架。
   保留故障槽及实际阶段；不 detach，不以超时或队列为空假报成功。
9. 修补 A1：Resume 最终捕获路由已切换、R2尚未seal时，确定准备失败进入Suspended，
   保留实际输入/前缀并停音频，终止本次直接续跑资格；显式退出或新会话恢复，不原地补做软件提交。
   修补 B1：所属owner串行裁决每个步骤的Apply/Cancel；只有保证迟到Apply也不再执行的
   终态未生效确认，结合资源使用结束证据，才能回收候选。

## 取舍与替代方案

| 方案 | 本轮结论 | 原因 |
| --- | --- | --- |
| A 同步模块化 | 保留测量基线，不作为最终部署 | 音频和执行不能继续依赖 GL 等待链 |
| B 执行核心/输出域分离 | 采用职责边界，首版 D2/D3 共置 | 单 owner 与较少控制参与者，接口可分而线程不先拆 |
| C 全独立服务流水线 | 延后，仅测量触发 D3 拆分审查 | 尚无收益证据足以覆盖额外前缀/回执/关闭状态 |
| D 不可变状态驱动全部表现 | 不选整体替换；只用自足结果/前缀共享 | FactBinding 的未来队列/寿命仍有状态，不假设任意重建免费 |
| E 通用依赖任务图 | 不选 | 当前窄协议可满足需求，通用取消/依赖管理增加维护成本 |
| SDL/GL 直接迁到 worker | 不选 | 当前平台主线程契约和 renderer owner 约束不支持 |

代价明确：主线程输入捕获仍受 present 阻塞，capture-time 合同诚实保留此限制；
跨域候选与控制有握手成本。该成本须测量，但不会以要求所有方向先做原型来阻塞选定方案。

A1减少半完成Resume的状态与重试分支，代价是这类故障后不能原地续局；向前重试A2和
暂存分类后再切换A3本轮不采用。B1复用单owner和操作身份，以终态记录换取局部可判定的
取消保证；不采用等待全部先前Apply完成的屏障方案B2，也不扩大为整代操作撤销B3。
软件已seal后的Fold/设备故障仍遵循原合同，这次选择不改变正常R2的判定结果。

## 合同、实施与后果

字段/语义的唯一落点是 [RPA补充Spec](../formats/rpa-execution-contract.md) 和
[宿主边界](../api/realtime-host-boundary.md)；批次、测量与验收归 [RPA计划](../stage_plans/active/stage-07/plan-rpa.md)。
新 execution/Fact token、Recovery rev3 和下一 SDK minor 是实施设计；当前 API 仍是 0.7.5。
不修改 Chart/CXC/CXT 内容格式，不安装内部 Kernel/Recovery，不引入脚本或第二判定路径。

本决定接受架构设计，不是产品实施授权、生产参数接受、owner API/release approval 或阶段关闭。
独立诊断 N3 基线必须核对，新增错误/载体需要增量复验；B5 负责实体设备和实时同步验收。
最终 F1–F4 完整退出并获 owner 接受后转 Stage 7B+，其完成后进入 Stage 8。
