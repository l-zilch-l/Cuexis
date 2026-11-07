# Gameplay-host Packed dictionary and budget audit

日期：2026-10-21。角色：独立正确性/准入调查，非性能接受或正式测量。
基础 HEAD 6014611，开工留档 a87e80d；最终状态仍由 [CURRENT_STATUS](../../../../../../../CURRENT_STATUS.md) 汇总。

## 合同与待证问题

[Packed §3.3](../../../../../../../formats/PACKED_CHART_FORMAT.md) 冻结 STR0 100,000 行、REF0 100,000 行、
单 section 16,777,216 bytes，并由全部入口按 min(requested,frozen) 求有效上限；0 是字面上限。
[Gameplay v2 §8.1](../../../../../../../formats/GAMEPLAY_V2_SPEC.md) 明确复用这些预算，不随 Gameplay 扩展放宽。
40,000 entities 与 40,000 requirements 是独立 ceiling，不保证任意复杂度的 40k 内容都能编码。
STR0 行数、UTF-8 bytes、REF0 行数、section bytes 与解码/prepare 堆预算分别记账。

[前轮交回](../../measurement/2026-10-21-pd5-followup.md) 的两种 40k Gameplay-host 都在编码准入阶段返回
packed.budget.strings；尚未测得实际字典条数。原 RPA-F3 codec 的 40k profile 不含同等完整
Gameplay 结构引用，不能替代此问题。此次不改限额、身份、Gameplay/Replay 编码或 fixture 含义。

源码证据：gameplay_capsule.cpp 的 requirementRow 对每个身份写 kind 12/subdomain 0 与
kind 13/subdomain 1 投影，claimRow 再写 kind 11 结构 claim key；Output::ref 同时收集 STR0
文本与 REF0 的 (kind,text)。capacity fixture 使用唯一 capacity.i；low 只改变数值 timing.end，
high 保留时间值。假设为每 Requirement 的唯一文本与三类结构引用造成线性增长，且 low/high 的
字符串集合不会因数值 timing 的差异而复用更多。最终条数、字节与各预算影响待实际计数确认。

## 有限观察预声明

新根：`D:\Cuexis-worktree\out\pd5-string-budget-20261021-6014611\`。
工具为 string_budget_audit.py；从已提交 HEAD 导出独立源码 archive，仅在副本插入观察日志，
以及让 capacity case 在 encode 后返回，不进入 host/Seek/Replay。实际 fixture 构造沿用原 case，
不另写缩减生成器；字典收集仍调用原四阶段 visitor，最终合并点位于原预算拒绝之前。
副本、原输入、观察补丁、工具/header 哈希及源清单另存 audit/plan.json；与正式 runner 无关。

固定 6 个原生进程，配置依次为 1-low、1-high、64-low、64-high、40000-low、40000-high，
每配置仅一次，无机器热身、无性能推断。Release STATIC、candidate ON、metrics/trace OFF，
最多 4 并行 BelowNormal 构建；新建 `out/build/pd5-string-budget-6014611-audit`。
采样阶段不构建、不下载；只采集字典事实，运行时保持默认允许 CPU 集合。
执行/身份/日志/fixture 不符合预声明时停止余项，不补样。小规模应编码成功；40k 预期仍按原预算
拒绝，该预期拒绝被观察到时工具自身可以退出 0，但 wire_accepted=false，不能称内容通过。

记录全局、Requirement、definitions、coordination 的累计去重字符串/引用数；记录最终
STR0/REF0、UTF-8 bytes、STR0 物理 bytes、reference kind 分布、capacity.i 与 cxgp2 投影条数。
字典 SHA 是排序去重文本各加 u64 LE 长度框架后的观察摘要，不冒充 wire/semantic identity。
预算保持原值；REF0/字节即使未被实际走到，也按真实收集数据分别披露，明确区分首个拒绝与后续
必然超界的计数事实。没有生成的 Packed 文件不给完整文件大小或 Reader 成功结论。

最终交付包括实际增长、预算归属、尚需单独决定的内容/profile 问题、原始日志和验证结果。
读取优化候选与本调查分开；本轮按用户指令只登记一个候选，不以当前有限计划作正式 registration。

## 实测结果与归因

登记提交 bdaa347；隔离配置/138 项构建退出 0。固定六个原生进程全部完成，无替换；
1/64 编码成功，40k 两种 reuse 均按原上限返回 packed.budget.strings，未进入 host。
audit/analysis.json 与各 *.log / *.process.json 是本轮原始记录。

| 每种 reuse 的 N | 实际 STR0 行 | 实际 REF0 行 | UTF-8 bytes | STR0 物理 bytes | 实际编码 |
| --- | --- | --- | --- | --- | --- |
| 1 | 52 | 16 | 1,735 | 1,955 | 成功 |
| 64 | 304 | 205 | 48,733 | 49,961 | 成功 |
| 40,000 | 160,048 | 120,013 | 30,643,225 | 31,283,429 | 字符串行数拒绝 |

每个 N 的 low/high 字典内容 SHA、行数、UTF-8 bytes 和 kind 分布完全相同。
这两种 fixture 只改变 timing.end 的数值复用程度，没有改变结构身份，不能称为 STR0 字符串
低/高复用对照。普通实体 UUID 在 IDN0 中以二进制表示，新增实体本身不是此处新增 UUID 文本的来源。

对于这一固定 fixture（N≥1），实际集合增长为 STR0=4N+48、REF0=3N+13：

| 增长来源 | 每个 Requirement 的新增 STR0 / REF0 | 40k 实际数量 |
| --- | --- | --- |
| 唯一 capacity.i 文本（Requirement 与 merged declaration 共享，已去重） | 1 / 0 | 40,000 / 0 |
| kind 12/subdomain 0 的完整 E 投影 | 1 / 1 | 40,000 / 40,000 |
| kind 13/subdomain 1 的完整 E 投影 | 1 / 1 | 40,000 / 40,000 |
| kind 11/subdomain 0 的 namespace+E claim key | 1 / 1 | 40,000 / 40,000 |
| 固定其他文本/引用 | 常量 | 48 / 13 |

40k 累计 visitor 结果：global 20/9、requirements 120030/80010、definitions 120036/80011、
coordination 160047/120013（字符串/引用）；静态 Chart 合并另加 1 个文本，得 160048/120013。
REF0 kind 为 4:1、5:1、7:2、10:1、11:40000、12:40000、13:40008。
底层已按字符串内容排序去重；这是合法唯一身份和既有投影表示引起的增长，不是去重失效。
三类完整结构 key 的十六进制展开还会重复承载 E 文本，造成字节增长。

## 各预算的独立处置

40k 的 STR0 行数超出 60,048，REF0 超出 20,013，STR0 单节超出 14,506,213 bytes。
Writer 先检查字符串条数，所以实际首个错误是 packed.budget.strings；后两项来自真实收集表
的独立计数，并未通过调大第一个门槛去执行后续编码。仅调大字符串数也不会让该 fixture 合法。
由于 Packed 生成已拒绝，没有 40k 完整文件大小、Reader、Snapshot、Seek 或 Replay 成功结论。

derive-growth.py / growth-analysis.json 保留可复算模型和边界。仅针对本 fixture 的必要条件：
字符串数允许到 N=24988、引用数到 N=33329、STR0 节 bytes 到 N=21497；这些边界值没有采样，
也不是完整文件/decoded envelope 或宿主可用容量保证。文件还有 Header、directory 及其他 section，
不能把 21497 宣称为已验证的完整容量；不截短既有 40k 失败输入去制造共同成功轨迹。

结论为内容/profile 与联合冻结预算不相容，预算检查按合同拒绝；本轮不调整上限、删除身份、
共享不同 Requirement 的 E/claim key、减少 requirements、修改 Gameplay/Replay 或另写缩减 profile。
若产品要求这一完整 40k claim-heavy profile 可交付，必须单独决定合法内容/profile 或新的物理表示/
预算合同，并补对应验收；原 Foundation 的两项独立 40k ceiling 本就不保证任意联合复杂度。
这项生产准入问题继续保持未通过，读取优化不能覆盖它。

## 验证与后续候选边界

本次工具 Python 53 项通过（method-tests.log）；新增拒绝测试覆盖缺阶段/计数、预算被放宽、
REF0 histogram 不一致、STR0 字节核算错误和将 40k 拒绝伪称通过。工具初稿的括号语法错误在
登记/原生运行之前修正，没有修改已冻结运行或替换样本。
另直接执行既有 F1 静态 cuexis_chart_tests 的 [chart][packed][budget][r3]，12 用例/179 断言通过，
日志 frozen-budget-contract-tests.log；没有调用 build 或改变其构建输入。
原源码与 limit resolver 未修改，观察副本及 instrumentation.diff 单独保留，工具不是公共 SDK API。

前轮证据包 3167 项与旧暂停/构建绑定开工核对通过，日志 previous-packet-before.log、history-before.log；
交回时再次核对。字典调查不冒充性能测量，控制器 0 只表示所有预声明观察符合预期，40k wire 仍拒绝。
唯一读取候选的未来机制与冻结条件由 [C2 计划](../../../../../../../stage_plans/active/stage-07/04-pd/pd-5/pd5-read-result-candidate.md)
拥有；实施/诊断结果另列报告。按用户补充，本轮完成候选验证后先交回核验，不自动保留或启动正式组。
