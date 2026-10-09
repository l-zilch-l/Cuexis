# S7A-7/8进度核对与独立RPA阶段拆分

状态：dated handoff；规划修订，非实施或阶段关闭

证据日期：2026-10-09（Asia/Shanghai）

后续范围订正：owner进一步要求实体设备与实时同步直接由RPA实施和验收，并明确保留Stage7B+。
本文原“RPA→S7A-9复验”的依赖由[后续记录](../verification/2026-10-09-version-gate-and-rpa-routing.md)
取代；原读数和失败证据不改写。另据[子阶段归属修订](2026-10-09-rpa-stage7-substage.md)，
RPA现归Stage 7，本文原future独立阶段落点已被取代；以下保留当时拆分记录。
现行顺序与分支以ROADMAP和Stage 7-RPA分册为准。

## 1. 基线与调整依据

本地和PR32均为stage-7、HEAD `d33c64b15575a945e50948ad70acb88f05117dce`；PR仍OPEN。
接手保留实时架构和正式命名审计的未提交文档/ZIP，没有reset或覆盖产品改动。
用户要求评估S7A-7/8完成进度，并将架构重构与正式命名迁移独立立项。
本轮只修订规划和状态，未进行产品实施、提交、推送、代审批、保护修改、合并或发行。

## 2. 原有九个小目标的证据核对

下表回顾既有实现与受限验收，不把九项均等换算为百分比。
主实现为83935f1375be61ba0f3ea16205fccc6a70094c8e；后续Player显示/设备桥及CI修复以各报告SHA为准。

| 原目标 | 已有证据范围 | 仍需区分的门禁 |
| --- | --- | --- |
| S7A-7.1 | 显式Playback生命周期、typed submit/advance/query、完整Replay/Snapshot/exact Seek/control、组合事务已实现和受限验收 | 新实时协调设计归RPA；旧证据不覆盖新线程/时钟合同 |
| S7A-7.2 | typed Gameplay source→Graph/Packed→Entry/CXC/filesystem generation→实际prepare一致性已验证 | RPA若改变时间profile/identity需关联回归 |
| S7A-7.3 | Headless实际kernel/Fold、Replay/Seek、golden/独立oracle/故障注入已验证 | RPA的独立时间oracle和渲染节奏试验另行验收 |
| S7A-7.4 | 实际SDL输入接线、Score/Combo/Miss和FactBinding反馈；四轨guide与定向窗口golden通过 | 实体DFJK/focus/control、人工听音与实时同步仍未通过；整改归RPA，交接后复验 |
| S7A-7.5 | clean staged static/shared、公开头/ASCII/版本拒绝及candidate隔离有受限验证 | RPA新API/命名的最终消费者矩阵不能借用旧证据 |
| S7A-8.1 | 默认OFF、显式ON、wrong-entry与安装flavor隔离，hosted candidate实际运行 | 当前SHA编译CI已通过；后续行为变更新SHA需重验 |
| S7A-8.2 | 生产assembler完整闭包、确定性原子发布、actualPrepare与负例已验证 | 不扩生产预算，不把重命名兼容当作已验 |
| S7A-8.3 | Reference Host六动词加tick、installed实际命令/失败保持旧active已验证 | RPA新增实时协议需独立Host回归 |
| S7A-8.4 | 版本门禁实现、可信bootstrap、SDK0.7.1差异证明和受限consumer已具备 | 当前HEAD审批绑定失败；保护/required workflow证据本轮未刷新，门禁未退出 |

来源：[10-08联合受限验收](../implementation/2026-10-08-s7a-7-8-entry-assembler-progress.md)、
[bootstrap记录](../verification/2026-10-09-version-gate-bootstrap.md)、
[四轨练习与定向窗口验证](../verification/2026-10-09-player-readable-practice.md)。
八项达到既有受限实现/验收口径，第九项版本门禁未退出；这不等于整个S7A-7/8已获最终接受。

## 3. 当前HEAD的hosted核对

`gh pr view 32 --repo l-zilch-l/Cuexis --json number,state,headRefName,headRefOid,baseRefName,statusCheckRollup,reviewDecision`
返回与本地相同HEAD；38个success、1个failure、2个skipped。success包括push/pull_request重复事件，
不是38种不同平台配置；Linux Quality、MSVC、MinGW均成功。
Version Gate run37924467045的pre-merge失败，post-merge audit与historical revalidation跳过。
通过`gh run view 37924467045 --repo l-zilch-l/Cuexis --log-failed`确认原始原因：
`version.sdk_api.approval_mismatch: approval does not bind this change`。
该失败结束时间为2026-10-09 19:39:56（Asia/Shanghai）；本轮只读取结果，未重跑CI或代发评论。
其余保护来源/merge queue条件未刷新，不作满足声明。旧SHA审批成功不绑定当前变更。
本地未提交文档不属于当前线上SHA的检查内容。

## 4. 独立阶段与范围迁移

新建[Stage RPA：实时播放架构与接口规范化](../../../../stage_plans/active/stage-07/plan-rpa.md)，
作为独立阶段，不使用S7A-7/8子任务编号，也不重排Stage8–14。

| 原工作 | 新工作 | 范围处理 |
| --- | --- | --- |
| RT78-0–7 | RPA-A0–A7 | 审查、时间/输入协议、测量、线程选择、热路径、发布、调度及消费者验收整体迁移 |
| DN78-0–3 | RPA-N0–N3 | 67定义审计、66正式名及1占位处置候选、公共投影、持久化兼容、版本和验证整体迁移 |
| plan-a §3.4.18/19 | 兼容入口 | 保留引用锚点，详细计划由RPA唯一维护；原增补文本归本报告证据包 |
| S7A-7/8原目标 | 原任务 | 原集成/版本门禁继续；关联设备失败保持未通过，不因范围迁移删除证据 |

顺序为7/8既有集成基线及可独立门禁→RPA→S7A-9最终集成/预算/关闭决策→Stage8。
RPA不要求Stage7A先关闭即可设计；8.4 owner门禁不阻止RPA独立设计。
S7A-9最终关闭仍需后续授权，本轮只规划，不接受任何生产阈值。

## 5. 交付检查与原始证据

[证据ZIP](2026-10-09-realtime-stage-separation-evidence.zip)保存PR当前检查JSON、Version run元数据、
失败原始日志、原plan-a增补文本、命令/退出码和本轮文档检查输出及文件哈希。
文档检查不替代产品或真实设备测试。当前状态只由CURRENT_STATUS维护，桌面接手同步摘要。

本轮检查通过：docs400 Markdown/20 candidate JSON，status4/4、section5/5、target2/2，
版本只读一致26.10.09-2，git diff --check无错误。未运行产品构建/设备测试；产品和版本输入未变。
