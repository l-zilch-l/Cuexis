# PD-5 40k Gameplay-host 准入：阶段 A 实施登记

状态：historical implementation registration；阶段 A/B/revision 4 与原 40k 功能证据已登记；不构成 PD-5 完整内存或正式性能接受。

更新日期：2026-10-26

当前实施只在 `D:/Cuexis-worktree`，`D:/Cuexis-pd5-40k-admission` 为只读历史证据树。本次整合/构建测试已获授权，不包含新 ETW、40k、性能诊断、pilot、正式窗口或新候选。下文保留阶段 A/B 当时的登记与授权范围；后续功能、内存和性能结论以 [CURRENT_STATUS](../../../CURRENT_STATUS.md) 及对应 dated 报告为准。

阶段 A 定量交回完成；2026-10-22 owner 已接受
[B0](../../../stage_reports/stages/stage-07/pd/2026-10-22-gameplay-admission-b0-budget-ruling.md)
的四维 envelope、Ix/拒绝顺序、内部 profile、revision 4 差异和兼容矩阵，并授权阶段 B。
阶段 B 限合同、Reader/Writer、有界 counting/hash、metadata 和相关测试。
scratch 高水位、身份、旧版兼容或完整物理预算验证失败立即停止交回。
当时阶段 C、正式测量、主树合并及 SDK/公共 API 升级尚未授权；这是 2026-10-22 的历史授权边界，不覆盖后续独立授权。下文阶段 A 登记保留原观察范围。
当前状态由 [CURRENT_STATUS](../../../CURRENT_STATUS.md) 汇总。桌面方案
`Cuexis-Stage7-PD5-40k-Gameplay-host-准入修复实施方案-2026-10-21.md` 是本轮实施输入；
既有[字典调查](../../../stage_reports/stages/stage-07/pd/2026-10-21-gameplay-host-string-budget.md)
拥有历史测量事实，[Capsule 格式](../../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md) 拥有现行字段。

## 基线与范围

从干净 F1 `eed36648a692333e47f798aa45be36ca850d08de` 新建
`codex/pd5-gameplay-40k-admission` / `D:/Cuexis-pd5-40k-admission`，
`1c7a145` 开工留档。主树、C2、历史正式窗口、旧 golden/失败输入/调查均不修改。
原七项预算、1.10/15%、生命周期/原子发布与全部 Gameplay 身份和行为语义保持原义。

阶段 A 只增加隔离诊断工具、工具测试和有索引的计划/草案/报告。通过归档构造带插桩的独立源码，
不修改产品路径，不新增公共 API，不使正式 Reader/Writer 接受 revision 4，不输出新格式成功包。
构建新正常 Release STATIC 诊断树，candidate ON，metrics/trace/lifetime observer OFF，
最多 4 并行、BelowNormal，不复用任何旧冻结构建。

## 固定观察清单

按 N=1 low、1 high、64 low、64 high、40000 low、40000 high 顺序，各一个原生进程。
使用原 `pd5_capacity_probe.hpp` 的原样生成循环，只在 encode 前接入诊断分支。
low/high 保持其 timing.end 数值差异，不能改称字符串低/高复用。
本清单不是性能 pilot、preparation 或 registration；不启动 Host/Snapshot/Seek/Replay。
源码、fixture、工具、插桩 diff、exe/library/cache/compile_commands/ninja 绑定后才执行；
执行、schema、输入、身份或小规模交叉验证错误立即停止，不替换原生样本。

## 计数与交叉验证

1. 原始 visitor 收集旧三类投影，联合实际 static 字典，复核前轮 STR0/REF0 增长模型。
2. 提议物理 visitor 仅省略 GPR0 两个投影引用与 GRC0 整个 optional claimKey 字段；所有真实字段不变。
   原有 bounded encodeStatic 输出实际静态 sections；共享 atom 输出器逐字段精确 sizing 四个 Gameplay
   sections，包含 16-byte framing、96-byte Header、32-byte directory 项和完整 decoded/file 合计。
3. 对 N≤64 的提议 payload 用共享 ByteWriter 实际序列化并核对 sizing；旧完整 Writer 成功输出仍保留，
   其 semanticPreimage 长度/hash/Header digest 与无完整输出缓冲的 counter/hash sink 交叉核对。
   40k 的提议 Gameplay payload 只给精确 visitor 计数，不声称存在已通过的新 wire/Reader。
4. 旧 revision 3 semantic visitor 的字段/域/展开次数不改。诊断累加完整 preimage 字节和摘要，按
   prefix/global/requirements/definitions/coordination 分拆，并记录索引展开次数/字节与最大单项。
   共享定义重复展开必须明列；不同层次的嵌套贡献不可无条件相加。
5. 派生引用计数 checked 计算 `2*requirements + occupyingClaims`，现有 validate 复核唯一 owner/
   backlink 和最多一条 claim。最大 projection 字节、prefix size/capacity、完整 hash 输出缓冲为零
   单列。scratch 按已观测 buffer 与可推导模型披露，不能冒充整个进程堆峰值。

所有需求计数可超过旧预算，但正式 Writer 仍按旧预算检查；40k 仍预期 packed.budget.strings。
`measured count`、`proposed wire size`、`wire accepted`、Host reached、性能接受各自标记。
新完整提议文件仍超原物理预算则交回设计，不削减 profile，也不自动扩大格式变更。

## 验证、退出与 B/C 检查点

运行计数数学/溢出与工具拒绝测试、小规模 sizing/hash 交叉验证、适用旧 codec/预算/golden 用例、
架构/格式/文档检查；按命令与实际执行项记录。本阶段产品源码不变，完整产品 B/C 回归尚未发生。
结果报告给出六配置原始日志、逐 section/完整文件、hash/派生/scratch 数据、攻击放大模型与预算草案。

阶段 A 完成后交回核验。必须先接受新会计和明确上限，再更新权威格式并实施 B，之后才做 C 的
完整原样 40k 功能链路；本计划不接受任意新数值，不默认切换 Writer，不改变 SDK/发布版本。
旧窗口 3 继续停止，C2 不合入，read64 不在本轮范围。
