# Gameplay-host Packed dictionary and budget audit

日期：2026-10-21。角色：独立正确性/准入调查，非性能接受或正式测量。
基础 HEAD 6014611，开工留档 a87e80d；最终状态仍由 [CURRENT_STATUS](../../../../CURRENT_STATUS.md) 汇总。

## 合同与待证问题

[Packed §3.3](../../../../formats/PACKED_CHART_FORMAT.md) 冻结 STR0 100,000 行、REF0 100,000 行、
单 section 16,777,216 bytes，并由全部入口按 min(requested,frozen) 求有效上限；0 是字面上限。
[Gameplay v2 §8.1](../../../../formats/GAMEPLAY_V2_SPEC.md) 明确复用这些预算，不随 Gameplay 扩展放宽。
40,000 entities 与 40,000 requirements 是独立 ceiling，不保证任意复杂度的 40k 内容都能编码。
STR0 行数、UTF-8 bytes、REF0 行数、section bytes 与解码/prepare 堆预算分别记账。

[前轮交回](2026-10-21-pd5-followup.md) 的两种 40k Gameplay-host 都在编码准入阶段返回
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
